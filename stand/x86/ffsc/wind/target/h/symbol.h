/* symbol.h - symbol structure header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02c,22sep92,rrr  added support for c++
02b,14jul92,jmm  added define of SYM_MASK_EXACT for symTblRemove
02a,04jul92,jcf  cleaned up.
01f,26may92,rrr  the tree shuffle
01e,30apr92,jmm  Added support for group numbers
01d,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed TINY and UTINY to INT8 and UINT8
		  -changed copyright notice
01c,19may91,gae  changed UINT8 to unsigned char.
01b,05oct90,shl  added copyright notice.
01a,10dec89,jcf  written by pulling out of symLib.h.
*/

#ifndef __INCsymbolh
#define __INCsymbolh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "slllib.h"

/* symbol masks */

#define SYM_MASK_ALL	0xff		/* all bits of symbol type valid */
#define SYM_MASK_NONE	0x00		/* no bits of symbol type valid */
#define SYM_MASK_EXACT	0x1ff		/* match symbol pointer exactly */

/* HIDDEN */

typedef signed char SYM_TYPE;		/* SYM_TYPE */

typedef struct			/* SYMBOL - entry in symbol table */
    {
    SL_NODE	nameHNode;	/* hash node (must come first) */
    char	*name;		/* pointer to symbol name */
    char	*value;		/* symbol value */
    UINT16	group;		/* symbol group */
    SYM_TYPE	type;		/* symbol type */
    } SYMBOL;

/* END_HIDDEN */

#ifdef __cplusplus
}
#endif

#endif /* __INCsymbolh */
