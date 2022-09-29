/* uio.h - uio header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * and the VxWorks Software License Agreement specify the terms and
 * conditions for redistribution.
 *
 *	@(#)uio.h	7.1 (Berkeley) 6/4/86
 */

/*
modification history
--------------------
02h,22sep92,rrr  added support for c++
02g,26may92,rrr  the tree shuffle
02f,28feb92,wmd  changed last pragma to turn of alignment for 960.
02e,27sep91,rrr  passed through the ansification filter
		  -changed includes to have absolute path from h/
		  -fixed #else and #endif
		  -changed copyright notice
02d,10jun91.del  added pragma for gnu960 alignment.
02c,19oct90,shl  added #include "types.h".
02b,05oct90,shl  added copyright notice.
02a,03apr87,ecs  added header and copyright.
*/

#ifndef __INCuioh
#define __INCuioh

#ifdef __cplusplus
extern "C" {
#endif

#include "sys/types.h"

#if CPU_FAMILY==I960
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

struct iovec {
	caddr_t	iov_base;
	int	iov_len;
};

struct uio {
	struct	iovec *uio_iov;
	int	uio_iovcnt;
	off_t	uio_offset;
	int	uio_segflg;
	int	uio_resid;
};

enum	uio_rw { UIO_READ, UIO_WRITE };

/*
 * Segment flag values (should be enum).
 */
#define UIO_USERSPACE	0		/* from user data space */
#define UIO_SYSSPACE	1		/* from system space */
#define UIO_USERISPACE	2		/* from user I space */

#if CPU_FAMILY==I960
#pragma align 0			/* tell gcc960 to turn off alignments */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCuioh */
