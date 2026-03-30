/**
 * @file test_boot.c
 * @brief Unit tests for the boot sequence: vector table and memory map.
 *
 * Tests run on the host (x86) using -DSIMULATION.  No ARM hardware needed.
 *
 * Build and run:
 *   make test
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "startup.h"
#include "vector_table.h"
#include "memory_map.h"

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

/* ==========================================================================
 * Vector Table Tests
 * ==========================================================================*/

/** The Cortex-M core defines exactly 16 exception vectors (indices 0..15). */
static void test_vector_table_size(void)
{
    TEST(NUM_VECTORS == 16,
         "NUM_VECTORS == 16  (ARM Cortex-M core exception count)");
}

/**
 * Entry [0] = initial SP.
 * In simulation we set it to 0x20020000 (top of a 128 KB SRAM starting at
 * 0x20000000).  This matches memory_map.h SRAM_TOP.
 */
static void test_vector_table_initial_sp(void)
{
    TEST(vector_table[0] == 0x20020000U,
         "vector_table[0] == 0x20020000 (simulated SRAM_TOP)");
}

/** Entry [1] = Reset_Handler address -- must be non-zero. */
static void test_vector_table_reset_handler_nonzero(void)
{
    TEST(vector_table[1] != 0U,
         "vector_table[1] (Reset_Handler) is non-zero");
}

/** Entries 7..10 and 13 are reserved -- must be zero per ARM spec. */
static void test_vector_table_reserved_zero(void)
{
    TEST(vector_table[7]  == 0U, "vector_table[7]  (reserved) == 0");
    TEST(vector_table[8]  == 0U, "vector_table[8]  (reserved) == 0");
    TEST(vector_table[9]  == 0U, "vector_table[9]  (reserved) == 0");
    TEST(vector_table[10] == 0U, "vector_table[10] (reserved) == 0");
    TEST(vector_table[13] == 0U, "vector_table[13] (reserved) == 0");
}

/** All non-reserved, non-SP entries must be non-zero (handlers defined). */
static void test_vector_table_handlers_nonzero(void)
{
    /* indices 2..6 = NMI, HardFault, MemManage, BusFault, UsageFault */
    int i;
    int all_nonzero = 1;
    for (i = 2; i <= 6; i++) {
        if (vector_table[i] == 0U) {
            all_nonzero = 0;
            printf("    vector_table[%d] is unexpectedly zero\n", i);
        }
    }
    /* indices 11, 12, 14, 15 = SVC, DebugMon, PendSV, SysTick */
    if (vector_table[11] == 0U) all_nonzero = 0;
    if (vector_table[12] == 0U) all_nonzero = 0;
    if (vector_table[14] == 0U) all_nonzero = 0;
    if (vector_table[15] == 0U) all_nonzero = 0;

    TEST(all_nonzero,
         "All Cortex-M exception handlers (NMI..SysTick) are non-zero");
}

/** Each handler symbol must be a valid (non-NULL) function pointer. */
static void test_handler_symbols_exist(void)
{
    TEST((void *)Reset_Handler    != NULL, "Reset_Handler symbol exists");
    TEST((void *)NMI_Handler      != NULL, "NMI_Handler symbol exists");
    TEST((void *)HardFault_Handler != NULL, "HardFault_Handler symbol exists");
    TEST((void *)MemManage_Handler != NULL, "MemManage_Handler symbol exists");
    TEST((void *)BusFault_Handler  != NULL, "BusFault_Handler symbol exists");
    TEST((void *)UsageFault_Handler != NULL, "UsageFault_Handler symbol exists");
    TEST((void *)SVC_Handler       != NULL, "SVC_Handler symbol exists");
    TEST((void *)DebugMon_Handler  != NULL, "DebugMon_Handler symbol exists");
    TEST((void *)PendSV_Handler    != NULL, "PendSV_Handler symbol exists");
    TEST((void *)SysTick_Handler   != NULL, "SysTick_Handler symbol exists");
    TEST((void *)Default_Handler   != NULL, "Default_Handler symbol exists");
}

/* ==========================================================================
 * Memory Map Tests
 * ==========================================================================*/

/** Verify the ARM Cortex-M standard base addresses. */
static void test_memory_map_base_addresses(void)
{
    TEST(FLASH_BASE      == 0x08000000UL,
         "FLASH_BASE      == 0x08000000");
    TEST(SRAM_BASE       == 0x20000000UL,
         "SRAM_BASE       == 0x20000000");
    TEST(PERIPHERAL_BASE == 0x40000000UL,
         "PERIPHERAL_BASE == 0x40000000");
}

/** Verify region sizes match the linker script. */
static void test_memory_map_sizes(void)
{
    TEST(FLASH_SIZE == 512U * 1024U,
         "FLASH_SIZE == 512 KB");
    TEST(SRAM_SIZE  == 128U * 1024U,
         "SRAM_SIZE  == 128 KB");
}

/** SRAM_TOP = SRAM_BASE + SRAM_SIZE and must match vector_table[0]. */
static void test_sram_top(void)
{
    TEST(SRAM_TOP == SRAM_BASE + SRAM_SIZE,
         "SRAM_TOP == SRAM_BASE + SRAM_SIZE");
    TEST(SRAM_TOP == 0x20020000UL,
         "SRAM_TOP == 0x20020000");
    TEST((uint32_t)SRAM_TOP == vector_table[0],
         "SRAM_TOP matches vector_table[0] (initial SP)");
}

/** Peripheral addresses must fall within the peripheral address space. */
static void test_peripheral_addresses_in_range(void)
{
    TEST(UART_BASE  >= PERIPHERAL_BASE && UART_BASE  < 0x50000000UL,
         "UART_BASE  is in peripheral region");
    TEST(GPIO_BASE  >= PERIPHERAL_BASE && GPIO_BASE  < 0x50000000UL,
         "GPIO_BASE  is in peripheral region");
    TEST(TIMER_BASE >= PERIPHERAL_BASE && TIMER_BASE < 0x50000000UL,
         "TIMER_BASE is in peripheral region");
}

/** App base must be inside Flash and above the bootloader. */
static void test_flash_secure_boot_layout(void)
{
    TEST(APP_BASE  >  FLASH_BASE,
         "APP_BASE  > FLASH_BASE  (app starts after bootloader)");
    TEST(APP_BASE  <  FLASH_BASE + FLASH_SIZE,
         "APP_BASE  is within Flash address space");
    TEST(APP_SIZE  == FLASH_SIZE - BOOTLOADER_SIZE,
         "APP_SIZE  == FLASH_SIZE - BOOTLOADER_SIZE");
    TEST(BOOTLOADER_SIZE == 32U * 1024U,
         "BOOTLOADER_SIZE == 32 KB");
}

/* ==========================================================================
 * main
 * ==========================================================================*/

int main(void)
{
    printf("\n=== Boot Sequence Unit Tests ===\n\n");

    printf("Vector Table:\n");
    test_vector_table_size();
    test_vector_table_initial_sp();
    test_vector_table_reset_handler_nonzero();
    test_vector_table_reserved_zero();
    test_vector_table_handlers_nonzero();
    test_handler_symbols_exist();

    printf("\nMemory Map:\n");
    test_memory_map_base_addresses();
    test_memory_map_sizes();
    test_sram_top();
    test_peripheral_addresses_in_range();
    test_flash_secure_boot_layout();

    printf("\n--- Results: %d/%d passed",
           g_tests_passed, g_tests_run);
    if (g_tests_failed > 0) {
        printf(", %d FAILED", g_tests_failed);
    }
    printf(" ---\n\n");

    return g_tests_failed;
}