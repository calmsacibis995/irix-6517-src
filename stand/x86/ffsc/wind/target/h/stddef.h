/* stddef.h - stddef header file */

/* Copyright 1991-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,22sep92,rrr  added support for c++
01b,04jul92,jcf  cleaned up.
01a,03jul92,smb  written.
*/

#ifndef __INCstddefh
#define __INCstddefh

#ifdef __cplusplus
extern "C" {
#endif

#include "types/vxansi.h"

#ifndef NULL
#define NULL	((void *) 0)
#endif

#define offsetof(type, member)	((size_t) &((type *) 0)->member)

#ifdef _TYPE_ptrdiff_t
_TYPE_ptrdiff_t;
#undef _TYPE_ptrdiff_t
#endif

#ifdef _TYPE_size_t
_TYPE_size_t;
#undef _TYPE_size_t
#endif

#ifdef _TYPE_wchar_t
_TYPE_wchar_t;
#undef _TYPE_wchar_t
#endif

#ifdef __cplusplus
}
#endif

#endif /* __INCstddefh */
