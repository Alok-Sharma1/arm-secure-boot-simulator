/**
 * @file startup.c
 * @brief ARM Cortex-M Startup Code
 *
 * This is the FIRST C code that runs on an ARM Cortex-M after a hardware
 * reset.  The CPU does NOT call main() directly -- it jumps to Reset_Handler
 * whose address is stored in vector_table[1].
 *
 * ════════════════════════════════════════════════════════════════════════════
 *  Cortex-M Cold-Boot Flow
 * ════════════════════════════════════════════════════════════════════════════
 *
 *  Power-on / Reset
 *       │
 *       ▼  (hardware auto-loads two words from vector table at Flash[0])
 *  CPU  ├─ word[0] ──► MSP (Main Stack Pointer)  ← SRAM top / _estack
 *       └─ word[1] ──► PC  (Program Counter)     ← Reset_Handler address
 *       │
 *       ▼
 *  Reset_Handler()                         ← THIS FILE
 *    │
 *    ├── 1. Copy .data  Flash (LMA) ──► SRAM (VMA)
 *    │         Initialized globals (e.g. `int x = 5;`) are baked into Flash
 *    │         at compile time but must live in writable SRAM at runtime.
 *    │         The linker records two addresses per .data symbol:
 *    │           LMA = where the initial value is stored in Flash (_sidata)
 *    │           VMA = where the CPU will read/write it in SRAM (_sdata.._edata)
 *    │
 *    ├── 2. Zero-fill .bss  in SRAM
 *    │         Uninitialized globals (`int y;`) occupy SRAM space but have
 *    │         no Flash storage -- their initial value is defined by the C
 *    │         standard to be zero.  No OS to do it for us, so WE must.
 *    │
 *    ├── 3. Call SystemInit()   (optional weak hook)
 *    │         Override in application to configure clocks, FPU, etc.
 *    │
 *    └── 4. Call main()
 *
 * ════════════════════════════════════════════════════════════════════════════
 *  Weak aliases
 * ════════════════════════════════════════════════════════════════════════════
 *  All exception handlers (NMI, HardFault, ...) are defined as weak aliases
 *  of Default_Handler.  Application code can override any of them by simply
 *  providing a strong definition with the same function name.
 *
 * ════════════════════════════════════════════════════════════════════════════
 *  SIMULATION mode (-DSIMULATION)
 * ════════════════════════════════════════════════════════════════════════════
 *  When compiled for a host x86/Linux machine the OS C-runtime already
 *  initialises .data and .bss, and Reset_Handler is never called.  All
 *  functions in this file become empty stubs so the translation unit
 *  compiles without requiring ARM linker-script symbols.
 */

#include "startup.h"
#include <stdint.h>

/* ══════════════════════════════════════════════════════════════════════════
 * BARE-METAL build
 * ══════════════════════════════════════════════════════════════════════════*/
#ifndef SIMULATION

/* Linker-script symbols -- NOT variables, just boundary addresses.
 * Access them with &_symbol to get the address as a uint32_t pointer.     */
extern uint32_t _estack;   /* Top of SRAM  (initial SP value)              */
extern uint32_t _sidata;   /* LMA: .data image start inside Flash           */
extern uint32_t _sdata;    /* VMA: .data runtime start in SRAM              */
extern uint32_t _edata;    /* VMA: .data runtime end   in SRAM              */
extern uint32_t _sbss;     /* .bss start in SRAM                            */
extern uint32_t _ebss;     /* .bss end   in SRAM                            */

extern int main(void);     /* Application entry point                       */

/*
 * VTOR -- Vector Table Offset Register (ARM DDI 0403E, Section B3.2.6)
 *
 * Writing this register tells the Cortex-M CPU where the vector table lives.
 * On reset it defaults to 0x00000000 (aliased to Flash on most STM32s), so
 * the device boots correctly without this write.  However:
 *
 *  1. Explicitness: makes the intent clear and survives silicon variants
 *     where the reset value differs (e.g. SRAM-boot devices).
 *
 *  2. Bootloader requirement: a bootloader MUST write VTOR = APP_BASE
 *     before jumping to the application, so the application's exception
 *     handlers replace the bootloader's.  SystemInit() demonstrates the
 *     pattern for the bootloader's own vector table.
 */
#define SCB_VTOR  (*(volatile uint32_t *)0xE000ED08U)

/* --------------------------------------------------------------------------
 * SystemInit -- weak hook called before main()
 *
 * __attribute__((weak)) means this definition is only used when no other
 * translation unit provides a strong definition.  This lets application code
 * set up clocks / FPU without touching startup.c.
 * --------------------------------------------------------------------------*/
__attribute__((weak)) void SystemInit(void)
{
    /*
     * VECTOR TABLE INITIALIZATION
     *
     * Set VTOR to the address of this bootloader's vector table.
     * The linker places vector_table[] at the start of Flash (0x08000000)
     * via the .isr_vector section; that is the value we write here.
     *
     * 'extern const uint32_t vector_table[]' is declared in vector_table.h
     * but we use the linker-symbol address directly to keep startup.c
     * self-contained and free of cross-module dependencies at init time.
     */
    SCB_VTOR = 0x08000000U;   /* = FLASH_BASE: bootloader vector table addr */
}

/* --------------------------------------------------------------------------
 * Reset_Handler -- ARM Cortex-M entry point
 *
 * The CPU jumps here from the vector table immediately after every reset.
 * The stack pointer is already valid (loaded from vector_table[0] by the
 * CPU), so we can use local variables right away.
 * --------------------------------------------------------------------------*/
void Reset_Handler(void)
{
    uint32_t *src;
    uint32_t *dst;

    /* ── Step 1: Copy initialised data from Flash → SRAM ──────────────────
     *
     * Flash layout after code (.text):   SRAM layout:
     *
     *  ┌──────────────────┐ ← _sidata    ┌──────────────────┐ ← _sdata
     *  │  .data LMA       │  (read-only  │  .data VMA       │  (r/w copy
     *  │  (initial values │   in Flash)  │   in SRAM that   │   code uses)
     *  │   baked in at    │  ──copy──►   │   code reads /   │
     *  │   compile time)  │              │   writes)         │
     *  └──────────────────┘              └──────────────────┘ ← _edata
     */
    src = &_sidata;
    dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* ── Step 2: Zero the BSS section ─────────────────────────────────────
     *
     * ISO C11 §6.7.9 ¶10: objects with static storage duration and without
     * an explicit initialiser shall be zero-initialised.
     *
     * BSS holds all such objects.  We must zero it because Flash contains
     * whatever was written last (or 0xFF after erase) -- not zeros.
     */
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0U;
    }

    /* ── Step 3: Optional pre-main HW init ───────────────────────────────*/
    SystemInit();

    /* ── Step 4: Enter application ───────────────────────────────────────
     * If main() returns (it shouldn't in bare-metal), spin forever to
     * prevent the CPU wandering into unprogrammed Flash.
     */
    main();
    while (1) { /* should never reach here */ }
}

/* --------------------------------------------------------------------------
 * Default_Handler -- catchall for every unimplemented exception / IRQ
 *
 * Infinite loop so a JTAG debugger can halt here and inspect the call stack
 * to find which exception fired.
 * --------------------------------------------------------------------------*/
void Default_Handler(void)
{
    while (1) { }
}

/* --------------------------------------------------------------------------
 * Weak aliases  -- resolve to Default_Handler unless the application
 * provides a strong override with the same function name.
 * --------------------------------------------------------------------------*/
void NMI_Handler(void)         __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void)   __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void)  __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void)         __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void)    __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void)      __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void)     __attribute__((weak, alias("Default_Handler")));

#else /* ══════════════════════════════════════════════════════════════════════
 * SIMULATION build  (-DSIMULATION)
 *
 * The OS C-runtime initialises .data and .bss before calling main().
 * Reset_Handler and friends are never invoked -- they are just stubs so
 * the linker is happy and tests can reference the symbols.
 * ══════════════════════════════════════════════════════════════════════════*/

void SystemInit(void)          { }
void Reset_Handler(void)       { }
void Default_Handler(void)     { }
void NMI_Handler(void)         { }
void HardFault_Handler(void)   { }
void MemManage_Handler(void)   { }
void BusFault_Handler(void)    { }
void UsageFault_Handler(void)  { }
void SVC_Handler(void)         { }
void DebugMon_Handler(void)    { }
void PendSV_Handler(void)      { }
void SysTick_Handler(void)     { }

#endif /* SIMULATION */
