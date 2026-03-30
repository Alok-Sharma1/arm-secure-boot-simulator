/**
 * @file uart.c
 * @brief Minimal UART driver -- bare-metal and simulation builds.
 *
 * Register map (generic PL011-compatible UART):
 *
 *   UART_BASE + 0x000  DR    Data Register       (TX write / RX read)
 *   UART_BASE + 0x004  RSR   Status / Error
 *   UART_BASE + 0x018  FR    Flag Register        bit 5 = TXFF (TX FIFO Full)
 *                                                  bit 4 = RXFE (RX FIFO Empty)
 *   UART_BASE + 0x024  IBRD  Integer Baud Rate
 *   UART_BASE + 0x030  CR    Control Register     bit 0 = UARTEN
 *                                                  bit 8 = TXE
 *                                                  bit 9 = RXE
 *
 * In SIMULATION mode all hardware register accesses are replaced with
 * printf/putchar/getchar so the module can be tested on an x86 host.
 */

#include "uart.h"
#include "memory_map.h"

#ifdef SIMULATION
#  include <stdio.h>
#endif

/* ---- Register offsets --------------------------------------------------- */
#define UART_DR_OFFSET    0x000u   /* Data Register                         */
#define UART_FR_OFFSET    0x018u   /* Flag Register                         */
#define UART_IBRD_OFFSET  0x024u   /* Integer Baud Rate Divisor             */
#define UART_CR_OFFSET    0x030u   /* Control Register                      */

/* ---- Flag Register bits ------------------------------------------------- */
#define UART_FR_TXFF      (1u << 5)  /* TX FIFO Full  (wait before sending) */
#define UART_FR_RXFE      (1u << 4)  /* RX FIFO Empty (no data to read)     */

/* ---- Control Register bits ---------------------------------------------- */
#define UART_CR_UARTEN    (1u << 0)  /* UART Enable                         */
#define UART_CR_TXE       (1u << 8)  /* Transmit Enable                     */
#define UART_CR_RXE       (1u << 9)  /* Receive Enable                      */

#ifndef SIMULATION
/** Memory-mapped register access helper (bare-metal only). */
#  define UART_REG(offset) (*((volatile uint32_t *)(UART_BASE + (offset))))
#endif

/* ════════════════════════════════════════════════════════════════════════════
 * Public functions
 * ════════════════════════════════════════════════════════════════════════════*/

/**
 * @brief Initialise the UART peripheral.
 *
 * Bare-metal: disables UART, sets baud rate divisor (assumes 16 MHz SYSCLK),
 *             then re-enables with TX and RX.
 * Simulation: prints an init notice and returns immediately.
 *
 * @param baud_rate  Desired baud rate (e.g. 115200).
 */
void uart_init(uint32_t baud_rate)
{
#ifdef SIMULATION
    (void)baud_rate;   /* suppress unused-parameter warning */
    /* stdout is already open; nothing to configure */
#else
    /* Disable UART before reconfiguring (required by PL011 spec) */
    UART_REG(UART_CR_OFFSET) = 0u;

    /* Integer baud rate divisor: IBRD = SYSCLK / (16 * baud_rate)
     * Assumes SYSCLK = 16 MHz.  Fractional part (FBRD) is ignored here.  */
    if (baud_rate > 0u) {
        UART_REG(UART_IBRD_OFFSET) = 16000000u / (16u * baud_rate);
    }

    /* Enable UART, TX, and RX */
    UART_REG(UART_CR_OFFSET) = UART_CR_UARTEN | UART_CR_TXE | UART_CR_RXE;
#endif
}

/**
 * @brief Transmit a single character (blocking).
 *
 * Bare-metal: polls TX-FIFO-Full flag until space is available.
 * Simulation: writes directly to stdout.
 *
 * @param c  Character to transmit.
 */
void uart_send_char(char c)
{
#ifdef SIMULATION
    putchar((unsigned char)c);
    fflush(stdout);
#else
    /* Spin until the TX FIFO has at least one free slot */
    while (UART_REG(UART_FR_OFFSET) & UART_FR_TXFF) { }
    UART_REG(UART_DR_OFFSET) = (uint32_t)(unsigned char)c;
#endif
}

/**
 * @brief Transmit a null-terminated string.
 * @param str  String to send.  Ignored if NULL.
 */
void uart_send_string(const char *str)
{
    if (str == NULL) {
        return;
    }
    while (*str != '\0') {
        uart_send_char(*str++);
    }
}

/**
 * @brief Receive a single character (blocking).
 *
 * Bare-metal: polls RX-FIFO-Empty flag until a character arrives.
 * Simulation: delegates to getchar().
 *
 * @return The received character.
 */
char uart_receive_char(void)
{
#ifdef SIMULATION
    return (char)getchar();
#else
    while (UART_REG(UART_FR_OFFSET) & UART_FR_RXFE) { }
    return (char)(UART_REG(UART_DR_OFFSET) & 0xFFu);
#endif
}

/**
 * @brief Non-blocking RX check.
 * @return 1 if data is waiting in the RX FIFO, 0 if empty.
 */
int uart_data_available(void)
{
#ifdef SIMULATION
    return 0;   /* not applicable in simulation */
#else
    return (UART_REG(UART_FR_OFFSET) & UART_FR_RXFE) ? 0 : 1;
#endif
}