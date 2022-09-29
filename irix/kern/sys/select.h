/*
 * Copyright 1992 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H

#ident	"$Revision: 1.17 $"

#ifdef __cplusplus
extern "C" {
#endif

#include <standards.h>

/*
 * This is an XPG4 header. It mandates that the fd_set be comprised of
 * signed quantities (it really wants longs).
 * NOTE: must NOT be included in POSIX mode since fd_set isn't a valid
 * POSIX symbol
 */

/*
 * FD_SETSIZE may be defined by the user, since select(2) only reads
 * as many bits as specified by the 'nfds' argument.
 */
#ifndef	FD_SETSIZE
#define	FD_SETSIZE	1024
#endif

#define __NBBY 8	/* number of bits per byte */

#if (_MIPS_SZLONG == 32)
typedef	long	fd_mask_t;
typedef	unsigned long	ufd_mask_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef	int	fd_mask_t;
typedef	unsigned int	ufd_mask_t;
#endif

#define	__NFDBITS	(int)(sizeof(fd_mask_t) * __NBBY) /* bits per mask */
#define	__howmany(x, y)	(((x)+((y)-1))/(y))

#if _SGIAPI
typedef struct fd_set {
#else
typedef struct {
#endif
	fd_mask_t	fds_bits[__howmany(FD_SETSIZE, __NFDBITS)];
} fd_set;

#define	FD_SET(n, p)	((p)->fds_bits[(n)/__NFDBITS] |= (fd_mask_t) (1 << ((n) % __NFDBITS)))
#define	FD_CLR(n, p)	((p)->fds_bits[(n)/__NFDBITS] &= (fd_mask_t) ~(1 << ((n) % __NFDBITS)))
#define	FD_ISSET(n, p)	((p)->fds_bits[(n)/__NFDBITS] & (fd_mask_t) (1 << ((n) % __NFDBITS)))


#if _SGIAPI
#include <string.h>
#endif

#if !defined(_SIZE_T) && !defined(_SIZE_T_)
#define _SIZE_T
#if (_MIPS_SZLONG == 32)
typedef unsigned int    size_t;
#endif
#if (_MIPS_SZLONG == 64)
typedef unsigned long   size_t;
#endif
#endif


#if _SGIAPI
#define FD_ZERO(p)      memset((void *)(p), 0, sizeof(*(p)))
#else
extern void *__memset(void *, int, size_t);
#define FD_ZERO(p)      __memset((void *)(p), 0, sizeof(*(p)))
#endif /* _SGIAPI */

#if _SGIAPI || defined(_BSD_TYPES)

/*
 * These are all backward compatibility for non XPG4 permitted symbols
 */
#ifndef NBBY		/* number of bits per byte */
#define NBBY 8
#endif

#ifndef	howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif

#define NFDBITS __NFDBITS

#if (_MIPS_SZLONG == 32)
typedef	long	fd_mask;
#endif
#if (_MIPS_SZLONG == 64)
typedef	int	fd_mask;
#endif

#endif /* _SGIAPI || _BSD_TYPES */

#ifdef __cplusplus
}
#endif
#endif /* _SYS_SELECT_H */
