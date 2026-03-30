/**
 * @file signature.c
 * @brief Secure-boot firmware integrity and authentication.
 *
 * This module implements two independent verification mechanisms that
 * together constitute a simulated secure-boot chain-of-trust:
 *
 *  ┌──────────────────────────────────────────────────────────────────────┐
 *  │  Stage 1 -- SHA-256 hash comparison  (simulates OTP hash binding)   │
 *  │                                                                      │
 *  │  At manufacture the SHA-256 of the golden firmware is computed       │
 *  │  and burned into write-once OTP fuses.  At every boot the MCU        │
 *  │  recomputes SHA-256(firmware) and compares it against the fuse hash. │
 *  │  Any single-bit change in firmware produces a completely different   │
 *  │  hash (avalanche effect), so tampering is detected immediately.      │
 *  └──────────────────────────────────────────────────────────────────────┘
 *
 *  ┌──────────────────────────────────────────────────────────────────────┐
 *  │  Stage 2 -- HMAC-SHA256  (simulates keyed firmware authentication)  │
 *  │                                                                      │
 *  │  The device holds a secret key in OTP (write-once, read-protected). │
 *  │  HMAC-SHA256(secret_key, firmware) is computed and compared against  │
 *  │  the MAC stored alongside the firmware.  An attacker without the     │
 *  │  secret key cannot forge a valid MAC for modified firmware.          │
 *  └──────────────────────────────────────────────────────────────────────┘
 *
 * Both comparisons use secure_memcmp() which examines every byte before
 * returning, preventing timing side-channel attacks.
 */

#include "signature.h"
#include "sha256.h"
#include <string.h>
#include <stdint.h>

/* ════════════════════════════════════════════════════════════════════════════
 * Simulated firmware image
 *
 * In a real system this would be a memory-mapped region in Flash,
 * e.g. (const uint8_t *)APP_BASE.  Here we define a static byte array
 * that represents a minimal, pre-compiled ARM Cortex-M firmware image.
 *
 * Layout:
 *   [0-3]   Initial stack-pointer value  (loaded from vector_table[0])
 *   [4-7]   Reset_Handler address        (loaded from vector_table[1])
 *   [8-11]  Magic header 'ARMS'
 *   [12-15] Magic header 'CURE'
 *   [16-19] Magic header 'BOOT'
 *   [20-23] Firmware version  'V1.0'
 *   [24-31] Reserved
 *   [32-35] Canary value  0xDEADBEEF
 *   [36-63] Simulated Thumb-2 instruction bytes
 * ════════════════════════════════════════════════════════════════════════════*/
static const uint8_t SIMULATED_FIRMWARE[] = {
    /* [0-3]  Initial SP = 0x20020000 (top of 128 KB SRAM) */
    0x00u, 0x00u, 0x02u, 0x20u,
    /* [4-7]  Reset_Handler at 0x08000009 (Thumb bit set) */
    0x09u, 0x00u, 0x00u, 0x08u,
    /* [8-15]  Magic header "ARMSCURE" */
    0x41u, 0x52u, 0x4Du, 0x53u,  /* 'A','R','M','S' */
    0x43u, 0x55u, 0x52u, 0x45u,  /* 'C','U','R','E' */
    /* [16-23] "BOOTV1.0" */
    0x42u, 0x4Fu, 0x4Fu, 0x54u,  /* 'B','O','O','T' */
    0x56u, 0x31u, 0x2Eu, 0x30u,  /* 'V','1','.','0' */
    /* [24-31] Reserved / padding */
    0xFFu, 0xFFu, 0xFFu, 0xFFu,
    0xFFu, 0xFFu, 0xFFu, 0xFFu,
    /* [32-35] Canary 0xDEADBEEF */
    0xDEu, 0xADu, 0xBEu, 0xEFu,
    /* [36-63] Thumb-2 instruction bytes (simulated code) */
    0x80u, 0xB5u, 0x6Fu, 0x46u,  /* PUSH {r7,lr};  MOV r7, r13          */
    0x01u, 0x78u, 0x08u, 0x70u,  /* LDRB r1,[r0];  STRB r0,[r1]        */
    0x38u, 0xBDu, 0x00u, 0x00u,  /* POP {r3,pc};   NOP                  */
    0xC0u, 0xF8u, 0x00u, 0x00u,  /* STR r0,[r0,#0] (dummy store)        */
    0x01u, 0x23u, 0x45u, 0x67u,  /* MOV r3,#1; MOV r5,#0x67 (dummy)    */
    0x89u, 0xABu, 0xCDu, 0xEFu,  /* Data pattern                        */
    0xFEu, 0xDCu, 0xBAu, 0x98u,  /* Data pattern                        */
};

/** Cached trusted hash -- computed once from SIMULATED_FIRMWARE on first use. */
static uint8_t  s_trusted_hash[SHA256_BLOCK_SIZE];
static int      s_hash_ready = 0;

/** Compute and cache the trusted reference hash (call once). */
static void init_trusted_hash(void)
{
    if (!s_hash_ready) {
        sha256_compute(SIMULATED_FIRMWARE, sizeof(SIMULATED_FIRMWARE),
                       s_trusted_hash);
        s_hash_ready = 1;
    }
}

/* ════════════════════════════════════════════════════════════════════════════
 * Public: simulated firmware access
 * ════════════════════════════════════════════════════════════════════════════*/

/**
 * @brief Return a pointer to the simulated firmware image.
 * @param out_len  Receives the byte length of the image.
 * @return Pointer to the firmware bytes.
 */
const uint8_t *secure_boot_get_firmware(size_t *out_len)
{
    *out_len = sizeof(SIMULATED_FIRMWARE);
    return SIMULATED_FIRMWARE;
}

/**
 * @brief Return a pointer to the 32-byte trusted reference hash.
 *
 * Simulates reading a SHA-256 digest that was burned into OTP fuses
 * at manufacture time.  The value is lazily computed from SIMULATED_FIRMWARE.
 *
 * @return Pointer to 32 bytes of trusted hash data.
 */
const uint8_t *secure_boot_get_trusted_hash(void)
{
    init_trusted_hash();
    return s_trusted_hash;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Public: SHA-256 hash-based integrity verification
 * ════════════════════════════════════════════════════════════════════════════*/

/**
 * @brief Verify firmware integrity by comparing SHA-256 digests.
 *
 * Algorithm:
 *   1. Compute SHA-256(firmware[0..firmware_len-1])
 *   2. Compare result against expected_hash using constant-time comparison
 *
 * @param firmware      Firmware image bytes.
 * @param firmware_len  Number of bytes in the firmware image.
 * @param expected_hash 32-byte reference hash (from OTP / secure storage).
 * @return BOOT_OK on match, BOOT_NULL_PTR or BOOT_SIG_FAIL otherwise.
 */
boot_status_t verify_firmware_hash(const uint8_t *firmware,
                                   size_t         firmware_len,
                                   const uint8_t *expected_hash)
{
    uint8_t computed[SHA256_BLOCK_SIZE];

    if (firmware == NULL || expected_hash == NULL) {
        return BOOT_NULL_PTR;
    }
    if (firmware_len == 0u) {
        return BOOT_SIG_FAIL;
    }

    sha256_compute(firmware, firmware_len, computed);

    return (secure_memcmp(computed, expected_hash, SHA256_BLOCK_SIZE) == 0)
           ? BOOT_OK : BOOT_SIG_FAIL;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Public: HMAC-SHA256
 * ════════════════════════════════════════════════════════════════════════════*/

/**
 * @brief Compute HMAC-SHA256(key, data) per RFC 2104.
 *
 * HMAC construction:
 *   K'  = key  if len(key) <= 64,  else K' = SHA-256(key)  (then pad to 64)
 *   K'' = K' padded with zeros to exactly 64 bytes
 *
 *   HMAC = SHA-256( (K'' XOR opad) || SHA-256( (K'' XOR ipad) || data ) )
 *
 *   where  ipad = 0x36 repeated 64 times
 *          opad = 0x5C repeated 64 times
 *
 * @param key       Secret key.
 * @param key_len   Key length in bytes.
 * @param data      Message to authenticate.
 * @param data_len  Message length in bytes.
 * @param out_hmac  Output buffer (must be 32 bytes).
 */
void hmac_sha256(const uint8_t *key,  size_t key_len,
                 const uint8_t *data, size_t data_len,
                 uint8_t        out_hmac[SHA256_BLOCK_SIZE])
{
    uint8_t    k_padded[64];   /* K' (key padded / hashed to 64 bytes)  */
    uint8_t    ipad_key[64];   /* K' XOR ipad                           */
    uint8_t    opad_key[64];   /* K' XOR opad                           */
    uint8_t    inner[SHA256_BLOCK_SIZE];
    SHA256_CTX ctx;
    int        i;

    /* Step 1: derive K' ------------------------------------------------- */
    if (key_len > 64u) {
        /* Key too long: hash it down to 32 bytes, then pad to 64           */
        sha256_compute(key, key_len, k_padded);
        memset(k_padded + SHA256_BLOCK_SIZE, 0,
               64u - SHA256_BLOCK_SIZE);
    } else {
        memcpy(k_padded, key, key_len);
        memset(k_padded + key_len, 0, 64u - key_len);
    }

    /* Step 2: XOR K' with ipad (0x36) and opad (0x5C) ------------------- */
    for (i = 0; i < 64; i++) {
        ipad_key[i] = k_padded[i] ^ 0x36u;
        opad_key[i] = k_padded[i] ^ 0x5Cu;
    }

    /* Step 3: inner hash = SHA-256( ipad_key || data ) ------------------- */
    sha256_init(&ctx);
    sha256_update(&ctx, ipad_key, 64u);
    sha256_update(&ctx, data,     data_len);
    sha256_final(&ctx, inner);

    /* Step 4: outer hash = SHA-256( opad_key || inner ) ------------------ */
    sha256_init(&ctx);
    sha256_update(&ctx, opad_key, 64u);
    sha256_update(&ctx, inner,    SHA256_BLOCK_SIZE);
    sha256_final(&ctx, out_hmac);
}

/**
 * @brief Verify an HMAC-SHA256 signature using constant-time comparison.
 *
 * @param key           Secret key.
 * @param key_len       Key length in bytes.
 * @param data          Authenticated message.
 * @param data_len      Message length in bytes.
 * @param expected_hmac 32-byte expected HMAC value.
 * @return BOOT_OK if MAC matches, BOOT_NULL_PTR or BOOT_SIG_FAIL otherwise.
 */
boot_status_t verify_hmac_signature(const uint8_t *key,  size_t key_len,
                                    const uint8_t *data, size_t data_len,
                                    const uint8_t *expected_hmac)
{
    uint8_t computed[SHA256_BLOCK_SIZE];

    if (key == NULL || data == NULL || expected_hmac == NULL) {
        return BOOT_NULL_PTR;
    }

    hmac_sha256(key, key_len, data, data_len, computed);

    return (secure_memcmp(computed, expected_hmac, SHA256_BLOCK_SIZE) == 0)
           ? BOOT_OK : BOOT_SIG_FAIL;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Public: security utility
 * ════════════════════════════════════════════════════════════════════════════*/

/**
 * @brief Constant-time memory comparison.
 *
 * Standard memcmp() short-circuits as soon as it finds a difference, leaking
 * information about HOW MANY bytes matched before the mismatch.  An attacker
 * can use this timing difference to guess a secret one byte at a time.
 *
 * This function always iterates all 'len' bytes (accumulates XOR into 'diff')
 * so the execution time is independent of the data values.
 *
 * @return 0 if a[0..len-1] == b[0..len-1], non-zero otherwise.
 */
int secure_memcmp(const uint8_t *a, const uint8_t *b, size_t len)
{
    uint8_t diff = 0u;
    size_t  i;
    for (i = 0u; i < len; i++) {
        diff |= a[i] ^ b[i];
    }
    return (int)diff;
}