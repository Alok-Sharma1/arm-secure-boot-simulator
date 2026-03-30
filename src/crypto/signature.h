/**
 * @file signature.h
 * @brief Secure-boot firmware integrity and authentication API.
 *
 * Production secure boot (e.g. STM32 SBSFU, ARM Platform Security Arch):
 *
 *  Build system                          Device at boot
 *  ──────────────────────────────        ────────────────────────────────
 *  1. Generate RSA-2048 key pair         1. Load pub-key from OTP fuses
 *  2. sig = RSA_sign(priv, SHA256(fw))   2. hash = SHA256(firmware)
 *  3. Append sig to firmware binary      3. RSA_verify(pub, hash, sig)
 *                                        4. match->run | mismatch->halt
 *
 * This SIMULATOR reproduces the CONCEPT without requiring RSA:
 *   - SHA-256 hash comparison  (simulates "known-good hash in OTP")
 *   - HMAC-SHA256              (simulates keyed firmware authentication)
 *
 * All functions use constant-time comparison (secure_memcmp) to prevent
 * timing side-channel attacks.
 */

#ifndef SIGNATURE_H
#define SIGNATURE_H

#include <stdint.h>
#include <stddef.h>
#include "sha256.h"

/* --------------------------------------------------------------------------
 * Return codes
 * --------------------------------------------------------------------------*/
typedef enum {
    BOOT_OK        = 0,  /**< Verification passed -- safe to boot           */
    BOOT_SIG_FAIL  = 1,  /**< Hash / HMAC mismatch -- firmware tampered     */
    BOOT_NULL_PTR  = 2,  /**< NULL pointer argument                         */
    BOOT_BAD_MAGIC = 3,  /**< Firmware magic header is wrong                */
} boot_status_t;

/* --------------------------------------------------------------------------
 * Simulated firmware access
 * (In production these would be memory-mapped Flash reads.)
 * --------------------------------------------------------------------------*/

/** Return pointer to the simulated 64-byte firmware image and its length. */
const uint8_t *secure_boot_get_firmware(size_t *out_len);

/**
 * Return pointer to the 32-byte trusted reference hash.
 * Simulates reading a SHA-256 digest burned into OTP fuses at manufacture.
 */
const uint8_t *secure_boot_get_trusted_hash(void);

/* --------------------------------------------------------------------------
 * Integrity verification -- SHA-256 hash comparison
 * --------------------------------------------------------------------------*/

/**
 * @brief Verify firmware integrity via SHA-256.
 *
 * Computes SHA-256(firmware) and compares it byte-for-byte against
 * expected_hash using constant-time comparison.
 *
 * @param firmware      Pointer to the firmware image.
 * @param firmware_len  Size of the firmware image in bytes.
 * @param expected_hash 32-byte trusted hash (from OTP / secure storage).
 * @return BOOT_OK if hashes match, BOOT_SIG_FAIL otherwise.
 */
boot_status_t verify_firmware_hash(const uint8_t *firmware,
                                   size_t         firmware_len,
                                   const uint8_t *expected_hash);

/* --------------------------------------------------------------------------
 * HMAC-SHA256  --  keyed message authentication
 * --------------------------------------------------------------------------*/

/**
 * @brief Compute HMAC-SHA256(key, data).
 *
 * HMAC(K, m) = H( (K' XOR opad) || H( (K' XOR ipad) || m ) )
 * where K' = key padded / hashed to 64 bytes.
 *
 * @param key       Secret key bytes.
 * @param key_len   Key length in bytes.
 * @param data      Message to authenticate.
 * @param data_len  Message length in bytes.
 * @param out_hmac  Output buffer -- must be SHA256_BLOCK_SIZE (32) bytes.
 */
void hmac_sha256(const uint8_t *key,  size_t key_len,
                 const uint8_t *data, size_t data_len,
                 uint8_t        out_hmac[SHA256_BLOCK_SIZE]);

/**
 * @brief Verify an HMAC-SHA256 signature.
 *
 * Re-computes HMAC(key, data) and performs constant-time comparison
 * against expected_hmac.
 *
 * @return BOOT_OK if valid, BOOT_SIG_FAIL if invalid or NULL args.
 */
boot_status_t verify_hmac_signature(const uint8_t *key,  size_t key_len,
                                    const uint8_t *data, size_t data_len,
                                    const uint8_t *expected_hmac);

/* --------------------------------------------------------------------------
 * Security utility
 * --------------------------------------------------------------------------*/

/**
 * @brief Constant-time memory comparison.
 *
 * Unlike memcmp() this NEVER short-circuits on the first differing byte,
 * preventing timing side-channel attacks against cryptographic secrets.
 *
 * @return 0 if a[0..len-1] == b[0..len-1], non-zero otherwise.
 */
int secure_memcmp(const uint8_t *a, const uint8_t *b, size_t len);

#endif /* SIGNATURE_H */