/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and that due credit is given
 * to the University of California at Berkeley. The name of the University
 * may not be used to endorse or promote products derived from this
 * software without specific prior written permission. This software
 * is provided ``as is'' without express or implied warranty.
 *
 *	@(#)in_systm.h	7.2 (Berkeley) 12/7/87
 */

#ifndef __NETINET_IN_SYSM_H__
#define __NETINET_IN_SYSM_H__
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Miscellaneous internetwork
 * definitions for kernel.
 */

/*
 * Network types.
 *
 * Internally the system keeps counters in the headers with the bytes
 * swapped so that VAX instructions will work on them.  It reverses
 * the bytes before transmission at each protocol level.  The n_ types
 * represent the types with the bytes in ``high-ender'' order.
 */

typedef u_short n_short;		/* short as received from the net */

typedef __uint32_t n_long;		/* long as received from the net */
typedef	__uint32_t n_time;		/* ms since 00:00 GMT, byte rev */

#ifdef _KERNEL
extern	n_time	iptime(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* __NETINET_IN_SYSM_H__ */
