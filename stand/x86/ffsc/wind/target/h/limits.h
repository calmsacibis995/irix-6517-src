/* limits.h - limits header file */

/* Copyright 1991-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01g,01may94,kdl	 added missing mod hist for 01f.
01f,06dec93,dvs	 added POSIX AIO defines.
01e,24sep92,smb  removed POSIX ifdef.
01d,22sep92,rrr  added support for c++
01c,04jul92,jcf  cleaned up.
01b,03jul92,smb  merged ANSI limits
01a,29jul91,rrr  written.
*/

#ifndef __INClimitsh
#define __INClimitsh

#ifdef __cplusplus
extern "C" {
#endif

#include "types/vxansi.h"

#define CHAR_BIT	_ARCH_CHAR_BIT
#define CHAR_MAX	_ARCH_CHAR_MAX
#define CHAR_MIN	_ARCH_CHAR_MIN
#define INT_MAX		_ARCH_INT_MAX
#define INT_MIN		_ARCH_INT_MIN
#define LONG_MAX	_ARCH_LONG_MAX
#define LONG_MIN	_ARCH_LONG_MIN
#define MB_LEN_MAX	_ARCH_MB_LEN_MAX
#define SCHAR_MAX	_ARCH_SCHAR_MAX
#define SCHAR_MIN	_ARCH_SCHAR_MIN
#define SHRT_MAX	_ARCH_SHRT_MAX
#define SHRT_MIN	_ARCH_SHRT_MIN
#define UCHAR_MAX	_ARCH_UCHAR_MAX
#define UINT_MAX	_ARCH_UINT_MAX
#define ULONG_MAX	_ARCH_ULONG_MAX
#define USHRT_MAX	_ARCH_USHRT_MAX
#define NAME_MAX        _PARM_NAME_MAX	/* max filename length excluding EOS */
#define PATH_MAX        _PARM_PATH_MAX	/* max pathname length excluding EOS */

#define NGROUPS_MAX					/* POSIX extensions */
#define SSIZE_MAX
#define DATAKEYS_MAX		_PARM_DATAKEYS_MAX
#define AIO_LISTIO_MAX		10	/* needs _PARM in types/vxParams.h */
#define AIO_PRIO_DELTA_MAX	254	/* needs _PARM in types/vxParams.h */

#define _POSIX_ARG_MAX		4096
#define _POSIX_CHILD_MAX	6
#define _POSIX_LINK_MAX		8
#define _POSIX_MAX_CANON	255
#define _POSIX_MAX_INPUT	255
#define _POSIX_NAME_MAX		14
#define _POSIX_NGROUPS_MAX	0
#define _POSIX_OPEN_MAX		16
#define _POSIX_PATH_MAX		255
#define _POSIX_PIPE_BUF		512
#define _POSIX_SSIZE_MAX	32767
#define _POSIX_STREAM_MAX	8
#define _POSIX_TZNAME_MAX	3
#define _POSIX_DATAKEYS_MAX	16

#define _POSIX_AIO_LISTIO_MAX	2
#define _POSIX_AIO_MAX		1

#ifdef __cplusplus
}
#endif

#endif /* __INClimitsh */
