/**
 * @file memory_map.h
 * @brief Hardware memory map for a generic ARM Cortex-M4 device.
 *
 * Modelled after the STM32F407 (Cortex-M4, 512 KB Flash, 128 KB SRAM).
 *
 * Address space overview:
 *
 *   0xFFFFFFFF  ┐
 *               │  Vendor / system space
 *   0xE0000000  ┤  Cortex-M PPB (SysTick, NVIC, SCB, DWT, ...)
 *   0x60000000  ┤  External memory
 *   0x40000000  ┤  APB / AHB peripherals   <-- PERIPHERAL_BASE
 *   0x20000000  ┤  SRAM  128 KB            <-- SRAM_BASE
 *   0x08000000  ┤  Flash 512 KB            <-- FLASH_BASE
 *   0x00000000  ┘  (alias of Flash on Cortex-M)
 */

#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

/* ---- Region base addresses ----------------------------------------------- */
#define FLASH_BASE        0x08000000UL
#define SRAM_BASE         0x20000000UL
#define PERIPHERAL_BASE   0x40000000UL

/* ---- Region sizes -------------------------------------------------------- */
#define FLASH_SIZE        (512U * 1024U)   /* 512 KB */
#define SRAM_SIZE         (128U * 1024U)   /* 128 KB */

/* ---- Derived addresses --------------------------------------------------- */
/** Top of SRAM = initial value loaded into MSP by the CPU on reset. */
#define SRAM_TOP          (SRAM_BASE + SRAM_SIZE)   /* 0x20020000 */

/* ---- Peripheral base addresses ------------------------------------------- */
#define UART_BASE         (PERIPHERAL_BASE + 0x1000UL)  /* 0x40001000 */
#define GPIO_BASE         (PERIPHERAL_BASE + 0x2000UL)  /* 0x40002000 */
#define TIMER_BASE        (PERIPHERAL_BASE + 0x3000UL)  /* 0x40003000 */

/* ---- Secure-boot Flash layout -------------------------------------------- */
/**
 * The first 32 KB of Flash is reserved for the bootloader itself.
 * The application firmware starts immediately after.
 *
 *   0x08000000  +-----------------------+
 *               |  Bootloader (32 KB)   |
 *   0x08008000  +-----------------------+
 *               |  Application (480 KB) |
 *   0x08080000  +-----------------------+
 */
#define BOOTLOADER_BASE   FLASH_BASE
#define BOOTLOADER_SIZE   (32U * 1024U)
#define APP_BASE          (FLASH_BASE + BOOTLOADER_SIZE)  /* 0x08008000 */
#define APP_SIZE          (FLASH_SIZE - BOOTLOADER_SIZE)  /* 480 KB     */

#endif /* MEMORY_MAP_H */