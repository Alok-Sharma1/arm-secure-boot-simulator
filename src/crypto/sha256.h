/**
 * @file sha256.h
 * @brief SHA-256 cryptographic hash function (FIPS 180-4).
 *
 * SHA-256 maps an arbitrary-length message to a 256-bit (32-byte) digest.
 * It is the backbone of secure boot integrity checking:
 *
 *   sha256_compute(firmware, fw_len, digest)
 *               |
 *               v
 *   compare digest against trusted_hash stored in OTP fuses
 *               |
 *    match -> proceed    mismatch -> HALT (tampered firmware)
 *
 * Implementation follows FIPS 180-4 exactly:
 *   - 8 x 32-bit initial hash values H0 (sqrt of first 8 primes)
 *   - 64 round constants K  (cbrt of first 64 primes)
 *   - Ch, Maj, Sigma0, Sigma1, sigma0, sigma1 logical functions
 *   - Big-endian message packing and digest output
 */

#ifndef SHA256_H
#define SHA256_H

#include <stdint.h>
#include <stddef.h>

/** SHA-256 output size in bytes. */
#define SHA256_BLOCK_SIZE  32U

/**
 * @brief SHA-256 context.
 *
 * Holds all intermediate state so large messages can be hashed in multiple
 * sha256_update() calls without buffering the whole message at once.
 */
typedef struct {
    uint32_t state[8];    /**< Running hash state (working variables A..H) */
    uint64_t bit_count;   /**< Total bits consumed so far                  */
    uint8_t  buffer[64];  /**< Partial 512-bit block waiting to be hashed  */
    size_t   buffer_len;  /**< Valid bytes currently held in buffer         */
} SHA256_CTX;

/* --------------------------------------------------------------------------
 * Streaming API  --  use when data arrives in chunks
 * --------------------------------------------------------------------------*/

/** Reset context to its initial state. Must be called before sha256_update. */
void sha256_init(SHA256_CTX *ctx);

/** Feed up to 'len' bytes of data into the running hash computation. */
void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len);

/**
 * Finalise the hash and write the 32-byte digest to 'hash'.
 * Applies SHA-256 padding and processes any remaining buffered bytes.
 */
void sha256_final(SHA256_CTX *ctx, uint8_t hash[SHA256_BLOCK_SIZE]);

/* --------------------------------------------------------------------------
 * One-shot API  --  convenience wrapper for contiguous buffers
 * --------------------------------------------------------------------------*/

/** Hash 'len' bytes at 'data' and write the 32-byte digest to 'hash'. */
void sha256_compute(const uint8_t *data, size_t len,
                    uint8_t hash[SHA256_BLOCK_SIZE]);

#endif /* SHA256_H */