/* in_systm.h - internetwork header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *      @(#)in_systm.h  7.3 (Berkeley) 6/29/88
 */

/*
modification history
--------------------
02g,22sep92,rrr  added support for c++
02f,26may92,rrr  the tree shuffle
02e,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
02d,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
02c,16apr89,gae  updated to new 4.3BSD.
02b,04nov87,dnw  removed unnecessary KERNEL conditionals.
02a,03apr87,ecs  added header and copyright.
*/

#ifndef __INCin_systmh
#define __INCin_systmh

#ifdef __cplusplus
extern "C" {
#endif

/* Miscellaneous internetwork definitions for kernel. */

/*
 * Network types.
 *
 * Internally the system keeps counters in the headers with the bytes
 * swapped so that VAX instructions will work on them.  It reverses
 * the bytes before transmission at each protocol level.  The n_ types
 * represent the types with the bytes in ``high-ender'' order.
 */
typedef u_short n_short;		/* short as received from the net */
typedef u_long	n_long;			/* long as received from the net */

typedef	u_long	n_time;			/* ms since 00:00 GMT, byte rev */

n_time	iptime();

#ifdef __cplusplus
}
#endif

#endif /* __INCin_systmh */
