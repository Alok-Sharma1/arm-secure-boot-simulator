/**
 * @file vector_table.h
 * @brief ARM Cortex-M4 Interrupt Vector Table
 *
 * The vector table is a flat array of 32-bit ADDRESSES placed at the very
 * start of Flash (0x08000000).  On every reset the Cortex-M CPU performs
 * two automatic memory reads BEFORE executing a single instruction:
 *
 *   word[0]  -> loaded into MSP  (Main Stack Pointer)
 *   word[1]  -> loaded into PC   (jumps to Reset_Handler)
 *
 * All subsequent entries are addresses of exception / IRQ handlers.
 *
 * ARM Cortex-M4 core exception map (indices 0 - 15):
 *   [0]  Initial stack pointer value
 *   [1]  Reset
 *   [2]  NMI             Non-Maskable Interrupt
 *   [3]  HardFault       Escalation target for all precise faults
 *   [4]  MemManage       MPU access violation
 *   [5]  BusFault        Bus error (data/instruction fetch)
 *   [6]  UsageFault      Undefined instruction, divide-by-zero, ...
 *   [7-10] Reserved      Must be zero
 *   [11] SVCall          Supervisor Call (RTOS syscall gate)
 *   [12] DebugMonitor
 *   [13] Reserved
 *   [14] PendSV          Pendable service request (RTOS context switch)
 *   [15] SysTick         System tick timer
 */

#ifndef VECTOR_TABLE_H
#define VECTOR_TABLE_H

#include <stdint.h>

/** Number of Cortex-M core exception vectors (indices 0..15). */
#define NUM_VECTORS  16

/**
 * The vector table -- an array of raw 32-bit handler addresses.
 * Placed at the start of Flash by the linker script section .isr_vector.
 * In SIMULATION mode a dummy table with representative values is used.
 */
extern const uint32_t vector_table[NUM_VECTORS];

/**
 * @brief Print vector table entries to UART (simulation / debug).
 * On real hardware you would read from the VTOR register (0xE000ED08).
 */
void dump_vector_table(void);

#endif /* VECTOR_TABLE_H */