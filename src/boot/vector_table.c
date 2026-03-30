/**
 * @file vector_table.c
 * @brief ARM Cortex-M4 Interrupt Vector Table
 *
 * The vector table is a flat array of 32-bit ADDRESSES that the Cortex-M
 * CPU reads automatically after every reset BEFORE executing any code:
 *
 *   entry[0]  → loaded into MSP (Main Stack Pointer)  ← top of SRAM
 *   entry[1]  → loaded into PC  (jumps to Reset_Handler)
 *   entry[2+] → exception / peripheral IRQ handler addresses
 *
 * WHY uint32_t and not function pointers?
 *   ARM Thumb instructions have their LSB set to 1 (Thumb-mode indicator).
 *   Storing function pointers as uint32_t gives us direct control over the
 *   exact value written to the table, and the cast `(uint32_t)&function`
 *   preserves the Thumb bit automatically.
 *
 * Linker placement:
 *   __attribute__((section(".isr_vector"))) combined with the linker rule
 *   KEEP(*(.isr_vector)) at the very start of FLASH guarantees this array
 *   appears at address 0x08000000 on an STM32-like device.
 *
 * 'aligned(512)':
 *   The VTOR (Vector Table Offset Register, 0xE000ED08) requires the table
 *   base to be aligned to a power-of-2 boundary ≥ 4 × (number of exceptions).
 *   512-byte alignment satisfies the requirement for up to 128 vectors.
 */

#include "vector_table.h"
#include "startup.h"
#include <stdint.h>

#ifdef SIMULATION
#include <stdio.h>
#endif

/* ══════════════════════════════════════════════════════════════════════════
 * BARE-METAL vector table
 * ══════════════════════════════════════════════════════════════════════════*/
#ifndef SIMULATION

extern uint32_t _estack;   /* Defined by linker script: ORIGIN(SRAM)+LENGTH(SRAM) */

__attribute__((section(".isr_vector"), used, aligned(512)))
const uint32_t vector_table[NUM_VECTORS] = {

    /* [0]  Initial Stack Pointer
     *      The CPU reads this word and loads it directly into MSP.
     *      _estack = top of SRAM = 0x20000000 + 128 KB = 0x20020000        */
    (uint32_t)&_estack,

    /* [1]  Reset Handler
     *      First instruction address executed after power-on or reset.     */
    (uint32_t)Reset_Handler,

    /* [2]  NMI -- Non-Maskable Interrupt
     *      Cannot be disabled by software.  Highest priority after Reset.  */
    (uint32_t)NMI_Handler,

    /* [3]  HardFault
     *      Catch-all fault handler.  Other fault handlers escalate here
     *      when their priority is lower than the active exception.          */
    (uint32_t)HardFault_Handler,

    /* [4]  MemManage
     *      Memory Protection Unit (MPU) access violation.                  */
    (uint32_t)MemManage_Handler,

    /* [5]  BusFault
     *      Bus error during a data access, instruction fetch, or vector
     *      table read.  Can be precise (address known) or imprecise.        */
    (uint32_t)BusFault_Handler,

    /* [6]  UsageFault
     *      Undefined instruction, invalid state, illegal unaligned access,
     *      or integer divide-by-zero (when DIV_0_TRP bit is set in CCR).   */
    (uint32_t)UsageFault_Handler,

    /* [7-10] Reserved -- must be zero per ARM architecture spec             */
    0U, 0U, 0U, 0U,

    /* [11] SVCall -- Supervisor Call
     *      Triggered by the SVC instruction.  Used by RTOS kernels as the
     *      gate into privileged mode (system-call mechanism).               */
    (uint32_t)SVC_Handler,

    /* [12] DebugMonitor
     *      Software debug monitor; fires during debug events when the CPU
     *      is not halted by a hardware debugger.                            */
    (uint32_t)DebugMon_Handler,

    /* [13] Reserved                                                         */
    0U,

    /* [14] PendSV -- Pendable Service Request
     *      Software-triggered, lowest-priority exception.  RTOS kernels use
     *      this to perform context switches without missing interrupts.     */
    (uint32_t)PendSV_Handler,

    /* [15] SysTick -- System Tick Timer
     *      Periodic interrupt driven by the 24-bit SYSTICK counter.
     *      Used by RTOS schedulers for timeslice management.               */
    (uint32_t)SysTick_Handler,
};

#else /* ══════════════════════════════════════════════════════════════════════
 * SIMULATION vector table  (-DSIMULATION)
 *
 * On x86/Linux there is no linker script, and casting function pointers to
 * uint32_t in a static initialiser is not a constant expression in C99.
 * We use representative FLASH-region constant addresses (odd = Thumb bit set)
 * so the table can be inspected and tested without real hardware.
 * The function symbols (Reset_Handler, etc.) are still linked as stubs
 * from startup.c and can be tested independently via their function pointers.
 * ══════════════════════════════════════════════════════════════════════════*/

const uint32_t vector_table[NUM_VECTORS] = {
    0x20020000U,   /* [0]  Simulated top of 128 KB SRAM (initial SP)        */
    0x08000009U,   /* [1]  Reset_Handler     (Thumb bit set: +1)            */
    0x08000011U,   /* [2]  NMI_Handler                                      */
    0x08000013U,   /* [3]  HardFault_Handler                                */
    0x08000015U,   /* [4]  MemManage_Handler                                */
    0x08000017U,   /* [5]  BusFault_Handler                                 */
    0x08000019U,   /* [6]  UsageFault_Handler                               */
    0U, 0U, 0U, 0U,/* [7-10] Reserved (ARM spec: must be zero)              */
    0x0800001BU,   /* [11] SVC_Handler                                      */
    0x0800001DU,   /* [12] DebugMon_Handler                                 */
    0U,            /* [13] Reserved                                         */
    0x0800001FU,   /* [14] PendSV_Handler                                   */
    0x08000021U,   /* [15] SysTick_Handler                                  */
};

#endif /* SIMULATION */

/**
 * @brief Print all vector table entries to stdout (SIMULATION / debug).
 *
 * On real hardware you would read from VTOR (0xE000ED08) or dump Flash
 * starting at 0x08000000.  In simulation we simply iterate the array.
 */
void dump_vector_table(void)
{
#ifdef SIMULATION
    static const char *names[NUM_VECTORS] = {
        "Initial SP      ",
        "Reset_Handler   ",
        "NMI_Handler     ",
        "HardFault       ",
        "MemManage       ",
        "BusFault        ",
        "UsageFault      ",
        "Reserved        ",
        "Reserved        ",
        "Reserved        ",
        "Reserved        ",
        "SVC_Handler     ",
        "DebugMon_Handler",
        "Reserved        ",
        "PendSV_Handler  ",
        "SysTick_Handler ",
    };
    int i;
    printf("\n  +------+------------------+------------+\n");
    printf(  "  | Idx  | Name             | Address    |\n");
    printf(  "  +------+------------------+------------+\n");
    for (i = 0; i < NUM_VECTORS; i++) {
        printf("  | [%2d] | %-16s | 0x%08X |\n",
               i, names[i], (unsigned int)vector_table[i]);
    }
    printf("  +------+------------------+------------+\n\n");
#endif
}