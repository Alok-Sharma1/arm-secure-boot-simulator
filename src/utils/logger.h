/**
 * @file logger.h
 * @brief Boot-time serial logger.
 *
 * Wraps the UART driver with a lightweight structured logging interface.
 * Output format:
 *
 *   [INFO]  Message text\r\n
 *   [WARN]  Warning text\r\n
 *   [ERROR] Error text\r\n
 *   [DEBUG] Debug text\r\n
 *
 * Hex-dump utility for printing SHA-256 digests and HMAC values:
 *
 *   [INFO]  SHA-256: BA 78 16 BF 8F 01 CF EA ...
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdint.h>
#include <stddef.h>

/** Log severity levels. */
typedef enum {
    LOG_INFO  = 0,
    LOG_WARN  = 1,
    LOG_ERROR = 2,
    LOG_DEBUG = 3,
} log_level_t;

/**
 * @brief Initialise the logger.
 *
 * Calls uart_init(115200) and prints the boot banner.
 * Must be called once before any log_*() function.
 */
void logger_init(void);

/**
 * @brief Log a message at the specified severity level.
 * @param level  One of LOG_INFO, LOG_WARN, LOG_ERROR, LOG_DEBUG.
 * @param msg    Null-terminated message string.
 */
void logger_log(log_level_t level, const char *msg);

/* Convenience wrappers ---------------------------------------------------- */
void log_info(const char *msg);    /**< Equivalent to logger_log(LOG_INFO,  msg) */
void log_warn(const char *msg);    /**< Equivalent to logger_log(LOG_WARN,  msg) */
void log_error(const char *msg);   /**< Equivalent to logger_log(LOG_ERROR, msg) */
void log_debug(const char *msg);   /**< Equivalent to logger_log(LOG_DEBUG, msg) */

/**
 * @brief Print a labelled hex dump of a byte buffer.
 *
 * Example output (label = "SHA-256", len = 32):
 *   [INFO]  SHA-256: BA 78 16 BF 8F 01 CF EA 41 41 40 DE 5D AE 2E C7
 *                    3B 00 36 1B BE F0 46 9F A5 39 5F DC 39 41 14 68
 *
 * @param label  Descriptive prefix string (e.g. "SHA-256" or "HMAC").
 * @param data   Pointer to the byte array.
 * @param len    Number of bytes to print.
 */
void log_hex(const char *label, const uint8_t *data, size_t len);

#endif /* LOGGER_H */