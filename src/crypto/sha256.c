/**
 * @file sha256.c
 * @brief SHA-256 Cryptographic Hash Function -- FIPS 180-4 compliant.
 *
 * SHA-256 (Secure Hash Algorithm 256-bit) transforms an arbitrary-length
 * message into a fixed 256-bit (32-byte) digest.  The same input ALWAYS
 * produces the same digest; changing even one bit of input produces a
 * completely different digest (avalanche effect).
 *
 * ════════════════════════════════════════════════════════════════════════════
 *  Algorithm overview (FIPS 180-4, Section 6.2)
 * ════════════════════════════════════════════════════════════════════════════
 *
 *  1. PRE-PROCESSING
 *     a) Pad message to a multiple of 512 bits:
 *        - append bit '1' (byte 0x80)
 *        - append zeros until length ≡ 448 (mod 512)
 *        - append original message length as 64-bit big-endian integer
 *
 *  2. HASH COMPUTATION -- for each 512-bit (64-byte) block:
 *     a) Prepare 64-word message schedule W[0..63]
 *        W[i] = M[i]                                   for i = 0..15
 *        W[i] = σ1(W[i-2]) + W[i-7] + σ0(W[i-15]) + W[i-16]  for i = 16..63
 *     b) Initialise working variables a,b,c,d,e,f,g,h from current state
 *     c) Run 64 compression rounds using Ch, Maj, Σ0, Σ1 functions
 *     d) Add compressed chunk to current state
 *
 *  3. OUTPUT -- concatenate final state[0..7] as 8 big-endian 32-bit words
 *
 * ════════════════════════════════════════════════════════════════════════════
 *  Logical functions (FIPS 180-4, Section 4.1.2)
 * ════════════════════════════════════════════════════════════════════════════
 *
 *   Ch(x,y,z)  = (x AND y) XOR (NOT x AND z)       -- "choose"
 *   Maj(x,y,z) = (x AND y) XOR (x AND z) XOR (y AND z) -- "majority"
 *   Σ0(x)  = ROTR²(x)  XOR ROTR¹³(x) XOR ROTR²²(x)  -- compression Sigma0
 *   Σ1(x)  = ROTR⁶(x)  XOR ROTR¹¹(x) XOR ROTR²⁵(x)  -- compression Sigma1
 *   σ0(x)  = ROTR⁷(x)  XOR ROTR¹⁸(x) XOR SHR³(x)    -- schedule sigma0
 *   σ1(x)  = ROTR¹⁷(x) XOR ROTR¹⁹(x) XOR SHR¹⁰(x)   -- schedule sigma1
 */

#include "sha256.h"
#include <string.h>

/* ════════════════════════════════════════════════════════════════════════════
 * Constants  (FIPS 180-4, Section 4.2.2)
 * ════════════════════════════════════════════════════════════════════════════*/

/**
 * Initial hash values H(0) -- the first 32 bits of the fractional parts
 * of the square roots of the first 8 prime numbers (2, 3, 5, 7, 11, 13, 17, 19).
 */
static const uint32_t H0[8] = {
    0x6a09e667ul, 0xbb67ae85ul, 0x3c6ef372ul, 0xa54ff53aul,
    0x510e527ful, 0x9b05688cul, 0x1f83d9abul, 0x5be0cd19ul
};

/**
 * Round constants K[0..63] -- the first 32 bits of the fractional parts
 * of the cube roots of the first 64 prime numbers.
 */
static const uint32_t K[64] = {
    0x428a2f98ul, 0x71374491ul, 0xb5c0fbcful, 0xe9b5dba5ul,
    0x3956c25bul, 0x59f111f1ul, 0x923f82a4ul, 0xab1c5ed5ul,
    0xd807aa98ul, 0x12835b01ul, 0x243185beul, 0x550c7dc3ul,
    0x72be5d74ul, 0x80deb1feul, 0x9bdc06a7ul, 0xc19bf174ul,
    0xe49b69c1ul, 0xefbe4786ul, 0x0fc19dc6ul, 0x240ca1ccul,
    0x2de92c6ful, 0x4a7484aaul, 0x5cb0a9dcul, 0x76f988daul,
    0x983e5152ul, 0xa831c66dul, 0xb00327c8ul, 0xbf597fc7ul,
    0xc6e00bf3ul, 0xd5a79147ul, 0x06ca6351ul, 0x14292967ul,
    0x27b70a85ul, 0x2e1b2138ul, 0x4d2c6dfcul, 0x53380d13ul,
    0x650a7354ul, 0x766a0abbul, 0x81c2c92eul, 0x92722c85ul,
    0xa2bfe8a1ul, 0xa81a664bul, 0xc24b8b70ul, 0xc76c51a3ul,
    0xd192e819ul, 0xd6990624ul, 0xf40e3585ul, 0x106aa070ul,
    0x19a4c116ul, 0x1e376c08ul, 0x2748774cul, 0x34b0bcb5ul,
    0x391c0cb3ul, 0x4ed8aa4aul, 0x5b9cca4ful, 0x682e6ff3ul,
    0x748f82eeul, 0x78a5636ful, 0x84c87814ul, 0x8cc70208ul,
    0x90befffaul, 0xa4506cebul, 0xbef9a3f7ul, 0xc67178f2ul
};

/* ════════════════════════════════════════════════════════════════════════════
 * Logical functions (implemented as macros for performance)
 * ════════════════════════════════════════════════════════════════════════════*/

/** Circular right-rotation of a 32-bit word. */
#define ROTR(x, n)    (((x) >> (n)) | ((x) << (32u - (n))))

/* Compression functions */
#define CH(x, y, z)   (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z)  (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

/* Sigma functions (upper-case = used in compression; lower-case = message schedule) */
#define SIGMA0(x)  (ROTR((x),  2u) ^ ROTR((x), 13u) ^ ROTR((x), 22u))
#define SIGMA1(x)  (ROTR((x),  6u) ^ ROTR((x), 11u) ^ ROTR((x), 25u))
#define sigma0(x)  (ROTR((x),  7u) ^ ROTR((x), 18u) ^ ((x) >>  3u))
#define sigma1(x)  (ROTR((x), 17u) ^ ROTR((x), 19u) ^ ((x) >> 10u))

/* ════════════════════════════════════════════════════════════════════════════
 * Internal helper: process one 512-bit block
 * ════════════════════════════════════════════════════════════════════════════*/

/**
 * @brief Run the SHA-256 compression function on a single 64-byte block.
 *
 * This is the computational core of SHA-256.  It updates ctx->state[] in
 * place by running 64 rounds of the compression function.
 *
 * @param ctx   SHA-256 context (state updated in place).
 * @param block Pointer to exactly 64 bytes of input (one 512-bit block).
 */
static void sha256_process_block(SHA256_CTX *ctx, const uint8_t block[64])
{
    uint32_t W[64];   /* Message schedule array                        */
    uint32_t a, b, c, d, e, f, g, h;  /* Working variables            */
    uint32_t T1, T2;
    int i;

    /* ── Build message schedule W[0..63] ──────────────────────────────────
     *
     * First 16 words: unpack the 64 input bytes into 16 big-endian uint32_t.
     * Remaining 48 words: mix using the sigma schedule functions.
     */
    for (i = 0; i < 16; i++) {
        W[i] = ((uint32_t)block[i * 4]      << 24u)
             | ((uint32_t)block[i * 4 + 1]  << 16u)
             | ((uint32_t)block[i * 4 + 2]  <<  8u)
             | ((uint32_t)block[i * 4 + 3]);
    }
    for (i = 16; i < 64; i++) {
        W[i] = sigma1(W[i - 2]) + W[i - 7] + sigma0(W[i - 15]) + W[i - 16];
    }

    /* ── Initialise working variables from current hash state ─────────────*/
    a = ctx->state[0];  b = ctx->state[1];
    c = ctx->state[2];  d = ctx->state[3];
    e = ctx->state[4];  f = ctx->state[5];
    g = ctx->state[6];  h = ctx->state[7];

    /* ── 64 compression rounds ─────────────────────────────────────────────
     *
     * Each round:
     *   T1 = h + Σ1(e) + Ch(e,f,g) + K[i] + W[i]
     *   T2 = Σ0(a) + Maj(a,b,c)
     *   h=g, g=f, f=e, e=d+T1, d=c, c=b, b=a, a=T1+T2
     */
    for (i = 0; i < 64; i++) {
        T1 = h + SIGMA1(e) + CH(e, f, g) + K[i] + W[i];
        T2 = SIGMA0(a) + MAJ(a, b, c);
        h = g;  g = f;  f = e;  e = d + T1;
        d = c;  c = b;  b = a;  a = T1 + T2;
    }

    /* ── Add compressed chunk to current hash state ────────────────────────*/
    ctx->state[0] += a;  ctx->state[1] += b;
    ctx->state[2] += c;  ctx->state[3] += d;
    ctx->state[4] += e;  ctx->state[5] += f;
    ctx->state[6] += g;  ctx->state[7] += h;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Public API
 * ════════════════════════════════════════════════════════════════════════════*/

/**
 * @brief Initialise (or re-initialise) a SHA-256 context.
 *
 * Loads the FIPS 180-4 initial hash values H(0) into ctx->state[].
 * Must be called before sha256_update().
 */
void sha256_init(SHA256_CTX *ctx)
{
    int i;
    for (i = 0; i < 8; i++) {
        ctx->state[i] = H0[i];
    }
    ctx->bit_count  = 0u;
    ctx->buffer_len = 0u;
}

/**
 * @brief Feed data into the running hash computation.
 *
 * Can be called any number of times.  Data is accumulated in a 64-byte
 * internal buffer; whenever the buffer fills, sha256_process_block() is
 * called automatically.
 *
 * @param ctx   Initialised SHA-256 context.
 * @param data  Pointer to input bytes (may be NULL if len == 0).
 * @param len   Number of bytes to hash.
 */
void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len)
{
    size_t i;

    if (len == 0u || data == NULL) {
        return;
    }

    ctx->bit_count += (uint64_t)len * 8u;  /* Track total bits for padding */

    for (i = 0u; i < len; i++) {
        ctx->buffer[ctx->buffer_len++] = data[i];
        if (ctx->buffer_len == 64u) {
            sha256_process_block(ctx, ctx->buffer);
            ctx->buffer_len = 0u;
        }
    }
}

/**
 * @brief Finalise the hash and produce the 32-byte digest.
 *
 * SHA-256 padding scheme (FIPS 180-4, Section 5.1.1):
 *   1. Append bit '1'  (0x80 byte)
 *   2. Append zero bytes until the block length is 56 bytes (mod 64)
 *   3. Append the original message bit-length as a 64-bit big-endian integer
 *
 * The finalised 256-bit hash is written to 'hash' as 8 big-endian uint32_t words.
 *
 * @param ctx   Initialised and updated SHA-256 context.
 * @param hash  Output buffer for the 32-byte digest.
 */
void sha256_final(SHA256_CTX *ctx, uint8_t hash[SHA256_BLOCK_SIZE])
{
    uint64_t total_bits;
    int i;

    /* Append the mandatory '1' bit (as 0x80 byte) */
    ctx->buffer[ctx->buffer_len++] = 0x80u;

    /* If there is not enough room for the 8-byte length field in this block,
     * pad the rest of the block with zeros and process it, then start fresh. */
    if (ctx->buffer_len > 56u) {
        while (ctx->buffer_len < 64u) {
            ctx->buffer[ctx->buffer_len++] = 0x00u;
        }
        sha256_process_block(ctx, ctx->buffer);
        ctx->buffer_len = 0u;
    }

    /* Pad with zeros up to the 56-byte mark (leaving 8 bytes for the length) */
    while (ctx->buffer_len < 56u) {
        ctx->buffer[ctx->buffer_len++] = 0x00u;
    }

    /* Append the original message length in BITS as a 64-bit big-endian word */
    total_bits = ctx->bit_count;
    ctx->buffer[56] = (uint8_t)(total_bits >> 56u);
    ctx->buffer[57] = (uint8_t)(total_bits >> 48u);
    ctx->buffer[58] = (uint8_t)(total_bits >> 40u);
    ctx->buffer[59] = (uint8_t)(total_bits >> 32u);
    ctx->buffer[60] = (uint8_t)(total_bits >> 24u);
    ctx->buffer[61] = (uint8_t)(total_bits >> 16u);
    ctx->buffer[62] = (uint8_t)(total_bits >>  8u);
    ctx->buffer[63] = (uint8_t)(total_bits);
    ctx->buffer_len = 64u;

    sha256_process_block(ctx, ctx->buffer);

    /* Serialise final state as 8 big-endian 32-bit words = 32 bytes */
    for (i = 0; i < 8; i++) {
        hash[i * 4]     = (uint8_t)(ctx->state[i] >> 24u);
        hash[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16u);
        hash[i * 4 + 2] = (uint8_t)(ctx->state[i] >>  8u);
        hash[i * 4 + 3] = (uint8_t)(ctx->state[i]);
    }
}

/**
 * @brief One-shot convenience: hash a contiguous buffer in a single call.
 *
 * Equivalent to: sha256_init(&ctx); sha256_update(&ctx, data, len); sha256_final(&ctx, hash).
 *
 * @param data  Input bytes (may be NULL if len == 0).
 * @param len   Number of bytes to hash.
 * @param hash  Output buffer for the 32-byte digest.
 */
void sha256_compute(const uint8_t *data, size_t len,
                    uint8_t hash[SHA256_BLOCK_SIZE])
{
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, hash);
}
