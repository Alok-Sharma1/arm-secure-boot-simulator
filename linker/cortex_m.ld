/*
 * cortex_m.ld -- Linker Script for ARM Cortex-M4
 *
 * Target device: generic STM32F407-like (512 KB Flash, 128 KB SRAM)
 *
 * ════════════════════════════════════════════════════════════════════════════
 *  Memory regions
 * ════════════════════════════════════════════════════════════════════════════
 *
 *   FLASH  0x08000000  512 KB  rx    Read-only code + rodata + .data image
 *   SRAM   0x20000000  128 KB  rwx   Stack + .data (runtime) + .bss + heap
 *
 * ════════════════════════════════════════════════════════════════════════════
 *  Key symbols exported to C startup code (startup.c)
 * ════════════════════════════════════════════════════════════════════════════
 *
 *   _estack   Top of SRAM = initial MSP value          (0x20020000)
 *   _sidata   LMA: where .data image is stored in Flash (after .text)
 *   _sdata    VMA: runtime start of .data in SRAM
 *   _edata    VMA: runtime end   of .data in SRAM
 *   _sbss     Start of .bss in SRAM
 *   _ebss     End   of .bss in SRAM
 *
 * ════════════════════════════════════════════════════════════════════════════
 *  LMA vs VMA explained
 * ════════════════════════════════════════════════════════════════════════════
 *
 *  Every variable has TWO addresses:
 *
 *    LMA (Load Memory Address)    = where the initial value is STORED at
 *                                   compile/link time.  For .data this is
 *                                   inside Flash (read-only memory).
 *
 *    VMA (Virtual Memory Address) = where the CPU EXPECTS to find the
 *                                   variable at runtime.  For .data this
 *                                   is in SRAM (read-write memory).
 *
 *  The linker packs the .data initial values into Flash right after .text
 *  (at _sidata).  startup.c copies them to SRAM (_sdata .. _edata) before
 *  main() runs.  Without this copy, all initialized globals would read as
 *  zero (or garbage), breaking the C language contract.
 */

/* Reset_Handler is the entry point (vector_table[1]) */
ENTRY(Reset_Handler)

/* ---- Memory map --------------------------------------------------------- */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

/* Stack size: 2 KB */
_stack_size = 0x800;

/* _estack = top of SRAM = initial MSP value loaded from vector_table[0] */
_estack = ORIGIN(SRAM) + LENGTH(SRAM);   /* 0x20020000 */

/* ---- Output sections ---------------------------------------------------- */
SECTIONS
{
    /*
     * .isr_vector  --  Interrupt Vector Table
     *
     * MUST be the very first thing in Flash (0x08000000).
     * 'KEEP' prevents --gc-sections from discarding it.
     * 'aligned(512)' satisfies the VTOR alignment requirement.
     */
    .isr_vector :
    {
        . = ALIGN(512);
        KEEP(*(.isr_vector))
        . = ALIGN(4);
    } > FLASH

    /*
     * .text  --  Executable code
     */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.glue_7)        /* ARM/Thumb interworking glue                  */
        *(.glue_7t)
        *(.eh_frame)
        KEEP(*(.init))
        KEEP(*(.fini))
        . = ALIGN(4);
    } > FLASH

    /*
     * .rodata  --  Read-only data (const globals, string literals)
     */
    .rodata :
    {
        . = ALIGN(4);
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH

    /* ARM exception index tables (needed for C++ stack unwinding) */
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
    .ARM       :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    /*
     * .data  --  Initialised global and static variables
     *
     * VMA (runtime address) = in SRAM
     * LMA (load address)    = in Flash  (" AT > FLASH" directive)
     *
     * LOADADDR(.data) gives the LMA, which startup.c stores in _sidata.
     */
    _sidata = LOADADDR(.data);   /* LMA: .data image start in Flash         */

    .data :
    {
        . = ALIGN(4);
        _sdata = .;              /* VMA: .data runtime start in SRAM        */
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;              /* VMA: .data runtime end in SRAM          */
    } > SRAM AT > FLASH

    /*
     * .bss  --  Uninitialised global and static variables
     *
     * Zero-initialised by startup.c.  Only a VMA (SRAM) is needed;
     * no Flash storage is required because the initial value is always 0.
     */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > SRAM

    /*
     * Stack  --  grows downward from _estack.
     * NOLOAD: not loaded from Flash (no LMA needed).
     */
    ._stack (NOLOAD) :
    {
        . = ALIGN(8);
        . = . + _stack_size;
        . = ALIGN(8);
    } > SRAM

    /* ARM metadata section (build attributes) */
    .ARM.attributes 0 : { *(.ARM.attributes) }
}
