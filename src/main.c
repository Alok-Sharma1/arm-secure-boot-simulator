/**
 * @file main.c
 * @brief ARM Cortex-M4 Secure Boot Main Entry Point
 *
 * ════════════════════════════════════════════════════════════════════════════
 *  Complete Boot Sequence
 * ════════════════════════════════════════════════════════════════════════════
 *
 *  Hardware reset
 *    │
 *    ▼  (CPU auto-loads SP from vector_table[0], jumps to vector_table[1])
 *  Reset_Handler()                         [startup.c]
 *    │  ① copy .data  Flash (LMA) → SRAM (VMA)
 *    │  ② zero .bss   in SRAM
 *    │  ③ call SystemInit()
 *    │
 *    ▼
 *  main()                                  [this file]
 *    │  ① logger_init()  -- UART @ 115200, boot banner
 *    │  ② dump_vector_table()  -- log all 16 exception addresses
 *    │  ③ STAGE 1: SHA-256 hash verification
 *    │       sha256_compute(firmware) → compare with OTP trusted hash
 *    │  ④ STAGE 2: HMAC-SHA256 authentication
 *    │       hmac_sha256(device_key, firmware) → compare with stored MAC
 *    │  ⑤ TAMPER DEMO: flip one byte, re-verify → expect FAIL
 *    │
 *    ▼
 *  boot_application()   or   enter_safe_state()
 *
 * ════════════════════════════════════════════════════════════════════════════
 *  Simulation vs. Bare-Metal
 * ════════════════════════════════════════════════════════════════════════════
 *  Compile with -DSIMULATION (see Makefile 'sim' target) to run the entire
 *  boot sequence on a host Linux/macOS machine.  UART output is redirected
 *  to stdout.  No ARM toolchain or hardware needed.
 */

#include "startup.h"
#include "vector_table.h"
#include "logger.h"
#include "signature.h"
#include "sha256.h"
#include "memory_map.h"
#include "common.h"
#include "uart.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ════════════════════════════════════════════════════════════════════════════
 * Device secret key
 *
 * In a real device this would be a 256-bit value burned into OTP fuses at
 * manufacture, configured as write-once and read-protected from the debug
 * interface.  Anyone with the private key can produce a valid HMAC; without
 * it a cloned device cannot be authenticated.
 * ════════════════════════════════════════════════════════════════════════════*/
static const uint8_t DEVICE_SECRET_KEY[32] = {
    0xDE, 0xAD, 0xBE, 0xEF,  0xCA, 0xFE, 0xBA, 0xBE,
    0x01, 0x23, 0x45, 0x67,  0x89, 0xAB, 0xCD, 0xEF,
    0x11, 0x22, 0x33, 0x44,  0x55, 0x66, 0x77, 0x88,
    0x99, 0xAA, 0xBB, 0xCC,  0xDD, 0xEE, 0xFF, 0x00,
};

/* ════════════════════════════════════════════════════════════════════════════
 * Internal helpers
 * ════════════════════════════════════════════════════════════════════════════*/

static void print_separator(void)
{
    uart_send_string("----------------------------------------\r\n");
}

/* ════════════════════════════════════════════════════════════════════════════
 * Stage 1 & 2 -- secure boot verification pipeline
 * ════════════════════════════════════════════════════════════════════════════*/

/**
 * @brief Run the full two-stage secure boot verification.
 *
 * Stage 1 -- SHA-256 hash comparison (integrity):
 *   Detects any modification to the firmware binary.
 *
 * Stage 2 -- HMAC-SHA256 (authentication):
 *   Proves the firmware was built by a party holding the device secret key.
 *
 * @return BOOT_OK if both stages pass, BOOT_SIG_FAIL otherwise.
 */
static boot_status_t run_secure_boot_pipeline(void)
{
    const uint8_t  *firmware;
    size_t          fw_len;
    const uint8_t  *trusted_hash;
    uint8_t         computed_hash[SHA256_BLOCK_SIZE];
    uint8_t         hmac_sig[SHA256_BLOCK_SIZE];
    boot_status_t   status;

    /* ── Load firmware image ───────────────────────────────────────────────*/
    log_info("Loading firmware image from Flash...");
    firmware = secure_boot_get_firmware(&fw_len);
    log_info("Firmware image loaded successfully.");
    print_separator();

    /* ── Stage 1: SHA-256 integrity check ─────────────────────────────────*/
    log_info("STAGE 1: SHA-256 Integrity Verification");
    log_info("  Computing SHA-256 hash of firmware...");
    sha256_compute(firmware, fw_len, computed_hash);
    log_hex("  Computed ", computed_hash, SHA256_BLOCK_SIZE);

    log_info("  Loading trusted hash from OTP secure storage...");
    trusted_hash = secure_boot_get_trusted_hash();
    log_hex("  Trusted  ", trusted_hash, SHA256_BLOCK_SIZE);

    log_info("  Comparing digests (constant-time)...");
    status = verify_firmware_hash(firmware, fw_len, trusted_hash);

    if (status != BOOT_OK) {
        log_error("STAGE 1 FAILED -- firmware hash mismatch!");
        log_error("Possible cause: firmware tampered or flash corruption.");
        return BOOT_SIG_FAIL;
    }
    log_info("STAGE 1 PASSED -- firmware integrity confirmed.");
    print_separator();

    /* ── Stage 2: HMAC-SHA256 authentication ──────────────────────────────*/
    log_info("STAGE 2: HMAC-SHA256 Authentication");
    log_info("  Computing HMAC-SHA256 with device secret key...");
    hmac_sha256(DEVICE_SECRET_KEY, sizeof(DEVICE_SECRET_KEY),
                firmware, fw_len, hmac_sig);
    log_hex("  HMAC     ", hmac_sig, SHA256_BLOCK_SIZE);

    log_info("  Verifying HMAC signature...");
    status = verify_hmac_signature(DEVICE_SECRET_KEY, sizeof(DEVICE_SECRET_KEY),
                                   firmware, fw_len, hmac_sig);

    if (status != BOOT_OK) {
        log_error("STAGE 2 FAILED -- HMAC verification failed!");
        return BOOT_SIG_FAIL;
    }
    log_info("STAGE 2 PASSED -- firmware authenticated.");
    print_separator();

    return BOOT_OK;
}

/* ════════════════════════════════════════════════════════════════════════════
 * Tamper detection demo
 *
 * Flips one byte of the firmware image, then re-runs the hash check to
 * demonstrate that even a single-bit change is caught.
 * ════════════════════════════════════════════════════════════════════════════*/

/**
 * @brief Demonstrate tamper detection by corrupting one firmware byte.
 *
 * Copies the first few bytes of the firmware into a local mutable buffer,
 * flips one bit in byte[10], then runs verify_firmware_hash() against the
 * original trusted hash -- expecting BOOT_SIG_FAIL.
 */
static void run_tamper_demo(void)
{
    /* Local buffer holding a MODIFIED copy of the first 64 bytes */
    uint8_t       tampered[64];
    const uint8_t *firmware;
    size_t         fw_len;
    const uint8_t *trusted_hash;
    boot_status_t  status;

    log_info("TAMPER DEMO: Simulating a modified firmware image...");

    firmware     = secure_boot_get_firmware(&fw_len);
    trusted_hash = secure_boot_get_trusted_hash();

    /* Copy and corrupt byte at offset 10 (flip all bits) */
    memcpy(tampered, firmware, sizeof(tampered));
    tampered[10] ^= 0xFFu;   /* <-- attacker modified this byte */

    log_info("  Byte[10] flipped from original value.");
    log_info("  Re-running SHA-256 verification on tampered image...");

    status = verify_firmware_hash(tampered, sizeof(tampered), trusted_hash);

    if (status == BOOT_SIG_FAIL) {
        log_warn("TAMPER DEMO RESULT: Tampered firmware CORRECTLY REJECTED.");
        log_warn("  SHA-256 avalanche effect: 1-bit change -> completely");
        log_warn("  different digest.  Attack detected and blocked.");
    } else {
        /* This should never happen with a correct SHA-256 implementation */
        log_error("TAMPER DEMO: ERROR -- tampered firmware was NOT detected!");
    }
    print_separator();
}

/* ════════════════════════════════════════════════════════════════════════════
 * Boot outcomes
 * ════════════════════════════════════════════════════════════════════════════*/

/**
 * @brief Transfer control to the application firmware.
 *
 * The three mandatory steps a Cortex-M bootloader must perform before
 * jumping to the application:
 *
 *   Step 1 -- VTOR relocation
 *     Write the application's vector table address into SCB->VTOR so the
 *     CPU uses the APPLICATION's exception handlers after the jump.
 *     If omitted, NMI / HardFault inside the application would execute
 *     the BOOTLOADER's handlers -- a critical security and correctness bug.
 *
 *   Step 2 -- Stack pointer update
 *     The application image's first word (APP_BASE + 0) is its initial MSP.
 *     We must load it before jumping so the application's stack is valid.
 *
 *   Step 3 -- Jump to application Reset_Handler
 *     The application's entry point address is at APP_BASE + 4.
 *     Bit 0 must be 1 (Thumb mode) -- BX preserves this automatically.
 */
static void boot_application(void)
{
    print_separator();
    log_info("\xE2\x95\x94\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x97");
    log_info("\xE2\x95\x91    SECURE BOOT: VERIFICATION OK      \xE2\x95\x91");
    log_info("\xE2\x95\x9A\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x90\xE2\x95\x9D");
    log_info("Transferring control to application firmware...");
    log_info("  Bootloader Flash base   : 0x08000000");
    log_info("  Application Flash base  : 0x08008000");

#ifndef SIMULATION
    {
        /*
         * Step 1: Relocate VTOR to the application's vector table.
         *
         * After this write, any exception (NMI, HardFault, SysTick, ...)
         * that fires AFTER the jump will be handled by the APPLICATION's
         * handlers, not the bootloader's.  This is architecturally required.
         */
#       define SCB_VTOR_REG  (*(volatile uint32_t *)0xE000ED08U)
        SCB_VTOR_REG = APP_BASE;  /* 0x08008000 */
        log_info("  VTOR updated            : 0x08008000 (app vector table)");

        /*
         * Step 2: Load the application's initial stack pointer.
         * Step 3: Read the application's Reset_Handler address.
         * Step 4: Jump (BX preserves the Thumb bit in bit 0).
         */
        uint32_t app_sp    = *(volatile uint32_t *)(APP_BASE + 0U);
        uint32_t app_entry = *(volatile uint32_t *)(APP_BASE + 4U);

        /* Set Main Stack Pointer to application stack top */
        __asm volatile ("MSR msp, %0" : : "r" (app_sp) : );

        /* Branch to application Reset_Handler -- no return */
        ((void (*)(void))app_entry)();
    }
#else
    log_info("  [SIM] VTOR would be set : 0x08008000 (app vector table)");
    log_info("  [SIM] MSP  would be set : APP_BASE word[0]");
    log_info("  [SIM] Jump would go to  : APP_BASE word[1] (Reset_Handler)");
    log_info(">>> APPLICATION IS NOW RUNNING <<<");
#endif
    print_separator();
}

/**
 * @brief Handle a boot failure -- enter security lockout.
 *
 * In a real device this state would:
 *   - Disable all debug interfaces (JTAG / SWD).
 *   - Store an error code in RTC backup register (survives reset).
 *   - Optionally trigger a watchdog reset after a delay.
 *   - After N consecutive failures: permanently disable the device.
 */
static void enter_safe_state(void)
{
    print_separator();
    log_error("╔══════════════════════════════════════╗");
    log_error("║   SECURE BOOT FAILED -- HALTED       ║");
    log_error("╚══════════════════════════════════════╝");
    log_error("Firmware integrity / authentication check failed.");
    log_error("System entering security lockout state.");
    log_error("Manual intervention or re-flashing required.");
    print_separator();

    /* On real hardware: spin forever (watchdog will eventually reset) */
    while (1) { }
}

/* ════════════════════════════════════════════════════════════════════════════
 * main() -- called by Reset_Handler after memory initialisation
 * ════════════════════════════════════════════════════════════════════════════*/

int main(void)
{
    boot_status_t result;

    /* ── 1. Initialise logging subsystem ───────────────────────────────────*/
    logger_init();

    /* ── 2. Boot banner ────────────────────────────────────────────────────*/
    print_separator();
    log_info("ARM Cortex-M4 Secure Boot Simulator v1.0");
    log_info("Build: " __DATE__ "  " __TIME__);
    log_info("Flash : 0x08000000  (512 KB)");
    log_info("SRAM  : 0x20000000  (128 KB)");
    log_info("Stack : 0x20020000  (top of SRAM)");
    print_separator();

    /* ── 3. Dump vector table ───────────────────────────────────────────────*/
    log_info("Vector Table (base: 0x08000000):");
    dump_vector_table();

    /* ── 4. Run secure boot verification pipeline ──────────────────────────*/
    log_info("Starting Secure Boot verification pipeline...");
    print_separator();

    result = run_secure_boot_pipeline();

    /* ── 5. Tamper detection demo ───────────────────────────────────────────*/
    run_tamper_demo();

    /* ── 6. Final boot decision ────────────────────────────────────────────*/
    if (result == BOOT_OK) {
        boot_application();
    } else {
        enter_safe_state();
    }

    return 0;
}
