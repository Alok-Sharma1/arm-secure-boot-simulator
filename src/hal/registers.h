/**
 * @file registers.h
 * @brief ARM Cortex-M4 memory-mapped register definitions.
 *
 * Two classes of registers are defined here:
 *
 *  1. ARM Cortex-M4 Private Peripheral Bus (PPB) registers -- these are at
 *     fixed addresses defined by the ARM architecture spec (not the silicon
 *     vendor) and are identical across every Cortex-M4 device:
 *
 *       0xE000E000  SCS   System Control Space
 *         0xE000E008  CPUID
 *         0xE000ED00  SCB   System Control Block
 *           0xE000ED08    VTOR  Vector Table Offset Register  ← KEY FOR SECURE BOOT
 *           0xE000ED0C    AIRCR Application Interrupt and Reset Control
 *           0xE000ED14    CCR   Configuration and Control Register
 *
 *  2. Vendor / GPIO registers -- vendor-specific but needed for blinky demos.
 */

#ifndef REGISTERS_H
#define REGISTERS_H

#include <stdint.h>

/* =========================================================================
 * ARM Cortex-M4 Private Peripheral Bus (PPB) -- architecture-defined
 * =========================================================================*/

/** SCS -- System Control Space base (0xE000E000, read-only CPUID at +0x08) */
#define SCS_BASE          0xE000E000UL

/**
 * SCB -- System Control Block  (0xE000ED00)
 *
 * The most important SCB register for bare-metal startup is VTOR.
 */
#define SCB_BASE          (SCS_BASE + 0x0D00UL)   /* 0xE000ED00 */

/**
 * VTOR -- Vector Table Offset Register  (0xE000ED08)
 *
 * Bits [31:9]  TBLOFF  Base address of the vector table.
 *              Must be aligned to a power-of-2 boundary >= 4 x num_vectors.
 *              (We use 512-byte alignment -- see vector_table.c)
 *
 * On hardware reset: VTOR = 0x00000000 (most STM32s alias Flash here).
 *
 * Bootloader use:
 *   1. Bootloader sets VTOR = FLASH_BASE (0x08000000) during SystemInit().
 *   2. After verifying the application, bootloader sets VTOR = APP_BASE
 *      (0x08008000) before jumping, so the application's own exception
 *      handlers become active.
 */
#define SCB_VTOR   (*(volatile uint32_t *)(SCB_BASE + 0x08UL))  /* 0xE000ED08 */

/** CPUID -- Read-only CPU identification register  (0xE000ED00) */
#define SCB_CPUID  (*(volatile uint32_t *)(SCB_BASE + 0x00UL))  /* 0xE000ED00 */

/**
 * CCR -- Configuration and Control Register  (0xE000ED14)
 * Bit 4  DIV_0_TRP: trap on integer divide-by-zero (triggers UsageFault)
 * Bit 3  UNALIGN_TRP: trap on unaligned word access
 */
#define SCB_CCR    (*(volatile uint32_t *)(SCB_BASE + 0x14UL))  /* 0xE000ED14 */

/* =========================================================================
 * GPIO registers (vendor-specific, generic Cortex-M4 device assumed)
 * =========================================================================*/

#define PERIPHERAL_BASE  0x40000000UL

#define GPIOA_BASE       (PERIPHERAL_BASE + 0x2000UL)

#define GPIOA_MODER      (*(volatile uint32_t *)(GPIOA_BASE + 0x00UL))
#define GPIOA_OTYPER     (*(volatile uint32_t *)(GPIOA_BASE + 0x04UL))
#define GPIOA_OSPEEDR    (*(volatile uint32_t *)(GPIOA_BASE + 0x08UL))
#define GPIOA_PUPDR      (*(volatile uint32_t *)(GPIOA_BASE + 0x0CUL))
#define GPIOA_IDR        (*(volatile uint32_t *)(GPIOA_BASE + 0x10UL))
#define GPIOA_ODR        (*(volatile uint32_t *)(GPIOA_BASE + 0x14UL))

#endif /* REGISTERS_H */