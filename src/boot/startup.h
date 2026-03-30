/**
 * @file startup.h
 * @brief ARM Cortex-M Startup Code Declarations
 *
 * The startup code is the bridge between hardware reset and main().
 * After every reset, the CPU reads the vector table:
 *   [0] -> loaded into MSP (Main Stack Pointer)
 *   [1] -> jumped to as Reset_Handler (this file's entry point)
 *
 * Reset_Handler then:
 *   1. Copies .data section  Flash (LMA) -> SRAM (VMA)
 *   2. Zeroes the .bss section in SRAM
 *   3. Calls SystemInit()  (optional HW pre-init, weak symbol)
 *   4. Calls main()
 *
 * All unused exception handlers are aliased to Default_Handler (infinite loop)
 * via __attribute__((weak, alias(...))).  Override any of them in your
 * application by simply defining a non-weak function with the same name.
 */

#ifndef STARTUP_H
#define STARTUP_H

#include <stdint.h>

/* -------------------------------------------------------------------------
 * Linker-script symbols (only meaningful on real ARM hardware)
 * These symbols mark the boundaries of .data and .bss in memory.
 * They are defined in linker/cortex_m.ld and NOT available when compiling
 * for x86 simulation -- hence the #ifndef SIMULATION guard.
 * -------------------------------------------------------------------------*/
#ifndef SIMULATION
extern uint32_t _estack;   /* Top of SRAM  = initial stack pointer          */
extern uint32_t _sidata;   /* LMA: start of .data image stored in Flash      */
extern uint32_t _sdata;    /* VMA: runtime start of .data in SRAM            */
extern uint32_t _edata;    /* VMA: runtime end   of .data in SRAM            */
extern uint32_t _sbss;     /* Start of .bss in SRAM                          */
extern uint32_t _ebss;     /* End   of .bss in SRAM                          */
#endif

/* -------------------------------------------------------------------------
 * Core handlers
 * -------------------------------------------------------------------------*/

/** Entry point -- CPU jumps here on every reset. */
void Reset_Handler(void);

/**
 * Catchall handler for every unimplemented exception / IRQ.
 * Spins forever (infinite loop) so a debugger can catch the fault.
 * All exception handlers below are weak aliases of this function.
 */
void Default_Handler(void);

/**
 * Optional pre-main hardware initialisation (clock, FPU, etc.).
 * Declared __attribute__((weak)) so application code can override it
 * without modifying startup.c.
 */
void SystemInit(void);

/* -------------------------------------------------------------------------
 * ARM Cortex-M Core Exception Handlers
 * (all weak-aliased to Default_Handler in startup.c)
 * -------------------------------------------------------------------------*/
void NMI_Handler(void);           /* Non-Maskable Interrupt              */
void HardFault_Handler(void);     /* All-catch fault escalation target   */
void MemManage_Handler(void);     /* MPU access violation                */
void BusFault_Handler(void);      /* Bus error on fetch/load/store       */
void UsageFault_Handler(void);    /* Undefined instruction, div-by-zero  */
void SVC_Handler(void);           /* Supervisor Call (SVC instruction)   */
void DebugMon_Handler(void);      /* Debug monitor                       */
void PendSV_Handler(void);        /* Pendable service request (RTOS)     */
void SysTick_Handler(void);       /* System-tick timer                   */

#endif /* STARTUP_H */