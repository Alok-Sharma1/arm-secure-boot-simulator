/**
 * @file common.h
 * @brief Common types, return codes, and utility macros.
 *
 * Included by every module.  Keep this header lightweight -- no hardware
 * addresses (see memory_map.h) and no module-specific declarations.
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ---- Generic return codes ----------------------------------------------- */
#define SUCCESS   ( 0)
#define FAILURE   (-1)

/* ---- Utility macros ------------------------------------------------------ */

/** Number of elements in a fixed-size array. */
#define ARRAY_SIZE(arr)    (sizeof(arr) / sizeof((arr)[0]))

/** Suppress unused-variable warnings. */
#define UNUSED(x)          ((void)(x))

/** Inclusive min / max. */
#define MIN(a, b)          ((a) < (b) ? (a) : (b))
#define MAX(a, b)          ((a) > (b) ? (a) : (b))

/* ---- Bit-manipulation helpers ------------------------------------------- */
#define BIT(n)             (1UL << (n))
#define SET_BIT(reg, n)    ((reg) |=  BIT(n))
#define CLR_BIT(reg, n)    ((reg) &= ~BIT(n))
#define TST_BIT(reg, n)    (((reg) >> (n)) & 1UL)

#endif /* COMMON_H */