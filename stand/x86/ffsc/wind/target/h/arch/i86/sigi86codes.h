/* sigI86Codes.h - signal facility library header */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,29may94,hdn  removed I80486 conditional.
01a,08jun93,hdn  written.
*/

#ifndef __INCsigI86Codesh
#define __INCsigI86Codesh

#ifdef __cplusplus
extern "C" {
#endif

#include "iv.h"

#define     ILL_DIVIDE_ERROR			IV_DIVIDE_ERROR
#define     ILL_NON_MASKABLE			IV_NON_MASKABLE
#define     ILL_OVERFLOW			IV_OVERFLOW
#define     ILL_BOUND				IV_BOUND
#define     ILL_INVALID_OPCODE			IV_INVALID_OPCODE
#define     ILL_DOUBLE_FAULT			IV_DOUBLE_FAULT
#define     ILL_INVALID_TSS			IV_INVALID_TSS
#define     ILL_PROTECTION_FAULT		IV_PROTECTION_FAULT
#define     ILL_RESERVED			IV_RESERVED
#define     EMT_DEBUG				IV_DEBUG
#define     EMT_BREAKPOINT			IV_BREAKPOINT
#define     FPE_NO_DEVICE			IV_NO_DEVICE
#define     FPE_CP_OVERRUN			IV_CP_OVERRUN
#define     FPE_CP_ERROR			IV_CP_ERROR
#define     BUS_NO_SEGMENT			IV_NO_SEGMENT
#define     BUS_STACK_FAULT			IV_STACK_FAULT
#define     BUS_PAGE_FAULT			IV_PAGE_FAULT
#define     BUS_ALIGNMENT			IV_ALIGNMENT

#ifdef __cplusplus
}
#endif

#endif /* __INCsigI86Codesh */
