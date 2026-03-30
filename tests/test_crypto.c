/**
 * @file test_crypto.c
 * @brief Unit tests for SHA-256, HMAC-SHA256, and secure_memcmp.
 *
 * SHA-256 test vectors from NIST FIPS 180-4:
 *   https://csrc.nist.gov/publications/detail/fips/180/4/final
 *
 * HMAC-SHA256 test vectors from RFC 4231:
 *   https://datatracker.ietf.org/doc/html/rfc4231
 *
 * Build and run:
 *   make test
 *
 * Expected output:
 *   All tests: PASS
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#include "sha256.h"
#include "signature.h"

/* ---- Test framework ----------------------------------------------------- */
static int g_tests_run    = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define TEST(cond, name)                                                    \
    do {                                                                    \
        g_tests_run++;                                                      \
        if (cond) {                                                         \
            printf("  [PASS] %s\n", (name));                              \
            g_tests_passed++;                                               \
        } else {                                                            \
            printf("  [FAIL] %s  (line %d)\n", (name), __LINE__);        \
            g_tests_failed++;                                               \
        }                                                                   \
    } while (0)

/* ---- Utility: print a hex buffer ---------------------------------------- */
static void print_hex(const char *label, const uint8_t *buf, size_t len)
{
    size_t i;
    printf("  %s: ", label);
    for (i = 0; i < len; i++) {
        printf("%02X", buf[i]);
    }
    printf("\n");
}

/* ==========================================================================
 * SHA-256 Tests  (NIST FIPS 180-4 test vectors)
 * ==========================================================================*/

/**
 * Test vector 1: empty string
 *   SHA-256("") = e3b0c44298fc1c149afbf4c8996fb924
 *                 27ae41e4649b934ca495991b7852b855
 */
static void test_sha256_empty_string(void)
{
    static const uint8_t expected[32] = {
        0xe3,0xb0,0xc4,0x42, 0x98,0xfc,0x1c,0x14,
        0x9a,0xfb,0xf4,0xc8, 0x99,0x6f,0xb9,0x24,
        0x27,0xae,0x41,0xe4, 0x64,0x9b,0x93,0x4c,
        0xa4,0x95,0x99,0x1b, 0x78,0x52,0xb8,0x55
    };
    uint8_t    hash[32];
    SHA256_CTX ctx;

    /* Hash an empty message using the streaming API */
    sha256_init(&ctx);
    sha256_final(&ctx, hash);

    if (memcmp(hash, expected, 32) != 0) {
        print_hex("Expected", expected, 32);
        print_hex("Got     ", hash,     32);
    }
    TEST(memcmp(hash, expected, 32) == 0,
         "SHA-256(\"\") == NIST FIPS 180-4 vector");
}

/**
 * Test vector 2: "abc"  (NIST FIPS 180-4 Appendix B.1)
 *   SHA-256("abc") = ba7816bf8f01cfea414140de5dae2223
 *                    b00361a396177a9cb410ff61f20015ad
 */
static void test_sha256_abc(void)
{
    static const uint8_t expected[32] = {
        0xba,0x78,0x16,0xbf, 0x8f,0x01,0xcf,0xea,
        0x41,0x41,0x40,0xde, 0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3, 0x96,0x17,0x7a,0x9c,
        0xb4,0x10,0xff,0x61, 0xf2,0x00,0x15,0xad
    };
    uint8_t hash[32];

    sha256_compute((const uint8_t *)"abc", 3, hash);

    if (memcmp(hash, expected, 32) != 0) {
        print_hex("Expected", expected, 32);
        print_hex("Got     ", hash,     32);
    }
    TEST(memcmp(hash, expected, 32) == 0,
         "SHA-256(\"abc\") == NIST FIPS 180-4 vector");
}

/**
 * Test vector 3: 448-bit (56-byte) message -- tests multi-block boundary.
 *   Input:  "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
 *   SHA-256 = 248d6a61d20638b8e5c026930c3e6039
 *             a33ce45964ff2167f6ecedd419db06c1
 */
static void test_sha256_56bytes(void)
{
    static const uint8_t expected[32] = {
        0x24,0x8d,0x6a,0x61, 0xd2,0x06,0x38,0xb8,
        0xe5,0xc0,0x26,0x93, 0x0c,0x3e,0x60,0x39,
        0xa3,0x3c,0xe4,0x59, 0x64,0xff,0x21,0x67,
        0xf6,0xec,0xed,0xd4, 0x19,0xdb,0x06,0xc1
    };
    static const char *msg =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    uint8_t hash[32];

    sha256_compute((const uint8_t *)msg, strlen(msg), hash);

    if (memcmp(hash, expected, 32) != 0) {
        print_hex("Expected", expected, 32);
        print_hex("Got     ", hash,     32);
    }
    TEST(memcmp(hash, expected, 32) == 0,
         "SHA-256(56-byte NIST message)");
}

/** Streaming API produces the same result as one-shot sha256_compute(). */
static void test_sha256_streaming_matches_oneshot(void)
{
    static const char *msg = "The quick brown fox jumps over the lazy dog";
    uint8_t hash_oneshot[32];
    uint8_t hash_stream[32];
    SHA256_CTX ctx;
    size_t len = strlen(msg);

    sha256_compute((const uint8_t *)msg, len, hash_oneshot);

    /* Feed same message in three chunks via streaming API */
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t *)msg,      10);
    sha256_update(&ctx, (const uint8_t *)msg + 10, 10);
    sha256_update(&ctx, (const uint8_t *)msg + 20, len - 20);
    sha256_final(&ctx, hash_stream);

    TEST(memcmp(hash_oneshot, hash_stream, 32) == 0,
         "SHA-256 streaming == one-shot for same input");
}

/** Two different inputs must produce different hashes (collision resistance). */
static void test_sha256_different_inputs_differ(void)
{
    uint8_t h1[32], h2[32];
    sha256_compute((const uint8_t *)"abc",  3, h1);
    sha256_compute((const uint8_t *)"abcd", 4, h2);
    TEST(memcmp(h1, h2, 32) != 0,
         "SHA-256: different inputs produce different digests");
}

/* ==========================================================================
 * HMAC-SHA256 Tests  (RFC 4231 Test Case 1)
 * ==========================================================================*/

/**
 * RFC 4231 Test Case 1:
 *   Key  = 0x0b * 20 bytes
 *   Data = "Hi There"
 *   HMAC = b0344c61d8db38535ca8afceaf0bf12b
 *          881dc200c9833da726e9376c2e32cff7
 */
static void test_hmac_sha256_rfc4231_case1(void)
{
    static const uint8_t key[20] = {
        0x0b,0x0b,0x0b,0x0b, 0x0b,0x0b,0x0b,0x0b,
        0x0b,0x0b,0x0b,0x0b, 0x0b,0x0b,0x0b,0x0b,
        0x0b,0x0b,0x0b,0x0b
    };
    static const uint8_t data[]    = "Hi There";
    static const uint8_t expected[32] = {
        0xb0,0x34,0x4c,0x61, 0xd8,0xdb,0x38,0x53,
        0x5c,0xa8,0xaf,0xce, 0xaf,0x0b,0xf1,0x2b,
        0x88,0x1d,0xc2,0x00, 0xc9,0x83,0x3d,0xa7,
        0x26,0xe9,0x37,0x6c, 0x2e,0x32,0xcf,0xf7
    };
    uint8_t mac[32];

    hmac_sha256(key, sizeof(key), data, 8, mac);

    if (memcmp(mac, expected, 32) != 0) {
        print_hex("Expected", expected, 32);
        print_hex("Got     ", mac,      32);
    }
    TEST(memcmp(mac, expected, 32) == 0,
         "HMAC-SHA256 RFC 4231 Test Case 1");
}

/** verify_hmac_signature returns BOOT_OK for a correct MAC. */
static void test_hmac_verify_correct(void)
{
    static const uint8_t key[]  = "secret-key-12345";
    static const uint8_t data[] = "firmware payload";
    uint8_t mac[32];

    hmac_sha256(key, sizeof(key) - 1, data, sizeof(data) - 1, mac);

    TEST(verify_hmac_signature(key, sizeof(key) - 1,
                                data, sizeof(data) - 1, mac) == BOOT_OK,
         "verify_hmac_signature: correct MAC returns BOOT_OK");
}

/** verify_hmac_signature returns BOOT_SIG_FAIL for a flipped MAC byte. */
static void test_hmac_verify_tampered(void)
{
    static const uint8_t key[]  = "secret-key-12345";
    static const uint8_t data[] = "firmware payload";
    uint8_t mac[32];

    hmac_sha256(key, sizeof(key) - 1, data, sizeof(data) - 1, mac);
    mac[0] ^= 0xFFu;   /* corrupt one byte */

    TEST(verify_hmac_signature(key, sizeof(key) - 1,
                                data, sizeof(data) - 1, mac) == BOOT_SIG_FAIL,
         "verify_hmac_signature: tampered MAC returns BOOT_SIG_FAIL");
}

/* ==========================================================================
 * secure_memcmp Tests
 * ==========================================================================*/

static void test_secure_memcmp_equal(void)
{
    static const uint8_t a[4] = {0x01, 0x02, 0x03, 0x04};
    static const uint8_t b[4] = {0x01, 0x02, 0x03, 0x04};
    TEST(secure_memcmp(a, b, 4) == 0,
         "secure_memcmp: equal arrays return 0");
}

static void test_secure_memcmp_different(void)
{
    static const uint8_t a[4] = {0x01, 0x02, 0x03, 0x04};
    static const uint8_t b[4] = {0x01, 0x02, 0x03, 0xFF};
    TEST(secure_memcmp(a, b, 4) != 0,
         "secure_memcmp: different arrays return non-zero");
}

/* ==========================================================================
 * verify_firmware_hash Tests
 * ==========================================================================*/

static void test_firmware_hash_pass(void)
{
    const uint8_t *fw;
    size_t         fw_len;
    const uint8_t *trusted;

    fw      = secure_boot_get_firmware(&fw_len);
    trusted = secure_boot_get_trusted_hash();

    TEST(verify_firmware_hash(fw, fw_len, trusted) == BOOT_OK,
         "verify_firmware_hash: authentic firmware returns BOOT_OK");
}

static void test_firmware_hash_fail_on_tamper(void)
{
    uint8_t        tampered[64];
    const uint8_t *fw;
    size_t         fw_len;
    const uint8_t *trusted;

    fw      = secure_boot_get_firmware(&fw_len);
    trusted = secure_boot_get_trusted_hash();

    /* Corrupt one byte */
    memcpy(tampered, fw, sizeof(tampered));
    tampered[5] ^= 0xFFu;

    TEST(verify_firmware_hash(tampered, sizeof(tampered), trusted) == BOOT_SIG_FAIL,
         "verify_firmware_hash: tampered firmware returns BOOT_SIG_FAIL");
}

static void test_firmware_hash_null_args(void)
{
    const uint8_t dummy[32] = {0};
    TEST(verify_firmware_hash(NULL,  8, dummy) == BOOT_NULL_PTR,
         "verify_firmware_hash: NULL firmware returns BOOT_NULL_PTR");
    TEST(verify_firmware_hash(dummy, 8, NULL)  == BOOT_NULL_PTR,
         "verify_firmware_hash: NULL expected_hash returns BOOT_NULL_PTR");
}

/* ==========================================================================
 * main
 * ==========================================================================*/

int main(void)
{
    printf("\n=== Crypto Unit Tests ===\n\n");

    printf("SHA-256 (NIST FIPS 180-4):\n");
    test_sha256_empty_string();
    test_sha256_abc();
    test_sha256_56bytes();
    test_sha256_streaming_matches_oneshot();
    test_sha256_different_inputs_differ();

    printf("\nHMAC-SHA256 (RFC 4231):\n");
    test_hmac_sha256_rfc4231_case1();
    test_hmac_verify_correct();
    test_hmac_verify_tampered();

    printf("\nsecure_memcmp:\n");
    test_secure_memcmp_equal();
    test_secure_memcmp_different();

    printf("\nFirmware hash verification:\n");
    test_firmware_hash_pass();
    test_firmware_hash_fail_on_tamper();
    test_firmware_hash_null_args();

    printf("\n--- Results: %d/%d passed",
           g_tests_passed, g_tests_run);
    if (g_tests_failed > 0) {
        printf(", %d FAILED", g_tests_failed);
    }
    printf(" ---\n\n");

    return g_tests_failed;
}