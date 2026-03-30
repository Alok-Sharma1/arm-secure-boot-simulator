/**
 * @file logger.c
 * @brief Boot-time serial logger implementation.
 *
 * All output is routed through uart_send_string() so the same code works
 * on real ARM hardware (via UART registers) and on x86 simulation (via
 * printf inside uart.c when compiled with -DSIMULATION).
 *
 * This module deliberately avoids printf() / sprintf() to stay compatible
 * with bare-metal builds that may not link against newlib.
 */

#include "logger.h"
#include "uart.h"

/* Hex digit lookup table */
static const char HEX_CHARS[16] = {
    '0','1','2','3','4','5','6','7',
    '8','9','A','B','C','D','E','F'
};

/**
 * @brief Initialise UART and print the boot banner.
 *
 * Must be the first logging call.  Configures the UART at 115200 baud
 * and prints a decorative header so the boot log is easy to spot on a
 * serial terminal.
 */
void logger_init(void)
{
    uart_init(115200u);
    uart_send_string("\r\n");
    uart_send_string("========================================\r\n");
    uart_send_string("  ARM Cortex-M4 Secure Boot Simulator  \r\n");
    uart_send_string("========================================\r\n");
}

/**
 * @brief Log a message with a severity prefix.
 *
 * Output format:  "[LEVEL] message text\r\n"
 *
 * @param level  Severity level (LOG_INFO, LOG_WARN, LOG_ERROR, LOG_DEBUG).
 * @param msg    Null-terminated message.  Ignored if NULL.
 */
void logger_log(log_level_t level, const char *msg)
{
    if (msg == NULL) {
        return;
    }

    switch (level) {
        case LOG_INFO:  uart_send_string("[INFO]  "); break;
        case LOG_WARN:  uart_send_string("[WARN]  "); break;
        case LOG_ERROR: uart_send_string("[ERROR] "); break;
        case LOG_DEBUG: uart_send_string("[DEBUG] "); break;
        default:        uart_send_string("[????]  "); break;
    }
    uart_send_string(msg);
    uart_send_string("\r\n");
}

/* ---- Convenience wrappers ----------------------------------------------- */
void log_info(const char *msg)  { logger_log(LOG_INFO,  msg); }
void log_warn(const char *msg)  { logger_log(LOG_WARN,  msg); }
void log_error(const char *msg) { logger_log(LOG_ERROR, msg); }
void log_debug(const char *msg) { logger_log(LOG_DEBUG, msg); }

/**
 * @brief Print a labelled hex dump of a byte buffer.
 *
 * Outputs bytes as upper-case hex pairs separated by spaces, 16 per line.
 * Uses only uart_send_string() -- no printf(), no dynamic allocation.
 *
 * Example for label="SHA-256", len=32:
 *
 *   [INFO]  SHA-256: BA 78 16 BF 8F 01 CF EA 41 41 40 DE 5D AE 2E C7
 *                    3B 00 36 1B BE F0 46 9F A5 39 5F DC 39 41 14 68
 *
 * @param label  Descriptive prefix (e.g. "SHA-256" or "HMAC-SHA256").
 * @param data   Byte array to print.
 * @param len    Number of bytes.
 */
void log_hex(const char *label, const uint8_t *data, size_t len)
{
    /* Each byte needs 2 hex chars + 1 space = 3 chars; we null-terminate. */
    char    pair[4];   /* "XY " + '\0'  */
    size_t  i;

    if (data == NULL || len == 0u) {
        return;
    }

    uart_send_string("[INFO]  ");
    if (label) {
        uart_send_string(label);
        uart_send_string(": ");
    }

    for (i = 0u; i < len; i++) {
        pair[0] = HEX_CHARS[(data[i] >> 4u) & 0x0Fu];
        pair[1] = HEX_CHARS[ data[i]        & 0x0Fu];
        pair[2] = ' ';
        pair[3] = '\0';
        uart_send_string(pair);

        /* Every 16 bytes start a new indented line */
        if (((i + 1u) % 16u == 0u) && (i + 1u < len)) {
            uart_send_string("\r\n          ");
        }
    }
    uart_send_string("\r\n");
}