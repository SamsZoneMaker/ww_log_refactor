/**
 * @file def.h
 * @brief Central simulation definitions for ww_log_v2 PC sim build.
 *
 * This file provides the base type definitions, return codes, and
 * compatibility shims that the firmware normally supplies through
 * its own def.h / project headers.
 *
 * Items marked "TODO: VERIFY" need confirmation from the fw developer
 * before porting to a real embedded target.
 */

#ifndef __SIM_DEF_H__
#define __SIM_DEF_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ======================================================================
 * Basic integer aliases
 * ====================================================================== */
typedef uint8_t   U8;
typedef uint16_t  U16;
typedef uint32_t  U32;
typedef uint64_t  U64;
typedef int8_t    S8;
typedef int16_t   S16;
typedef int32_t   S32;
typedef int64_t   S64;
typedef uintptr_t UPTR;

/* ======================================================================
 * WW_ return-code / bool type system
 * TODO: VERIFY exact values with fw team if WW_RTN is an enum, not int.
 * ====================================================================== */
typedef int WW_RTN;
typedef int WW_BOOL;

#define WW_OK         0
#define WW_ERR        1

#define WW_TRUE       1
#define WW_FALSE      0

#define WW_ENABLE     1
#define WW_DISABLE    0

/* ======================================================================
 * Null / common constants
 * ====================================================================== */
#ifndef NULL
#define NULL ((void *)0)
#endif

#endif /* __SIM_DEF_H__ */
