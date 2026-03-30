/**
 * @file uart.h
 * @brief Minimal UART driver (hardware abstraction layer).
 *
 * Provides serial TX/RX used by the boot logger.  Two compilation modes:
 *
 *   Bare-metal (default):
 *     Drives a generic PL011-compatible UART at UART_BASE via memory-mapped
 *     register writes.  The actual hardware address comes from memory_map.h.
 *
 *   Simulation (-DSIMULATION):
 *     All output is redirected to stdout (printf / putchar) so the code
 *     compiles and runs on a host Linux/macOS machine without any ARM
 *     toolchain or hardware.
 */

#ifndef UART_H
#define UART_H

#include <stdint.h>

/**
 * @brief Initialise the UART peripheral.
 * @param baud_rate  Desired baud rate (e.g. 115200).
 *
 * Must be called once before any uart_send_* or uart_receive_char().
 * In simulation mode this prints a one-line init notice and returns.
 */
void uart_init(uint32_t baud_rate);

/**
 * @brief Transmit one character (blocking).
 * @param c  Character to send.
 */
void uart_send_char(char c);

/**
 * @brief Transmit a null-terminated string.
 * @param str  Pointer to string.  If NULL, the function is a no-op.
 */
void uart_send_string(const char *str);

/**
 * @brief Receive one character (blocking).
 * @return The received character.
 */
char uart_receive_char(void);

/**
 * @brief Non-blocking RX availability check.
 * @return 1 if at least one character is waiting in the RX FIFO, else 0.
 */
int uart_data_available(void);

#endif /* UART_H */