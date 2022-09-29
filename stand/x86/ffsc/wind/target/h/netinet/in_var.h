/* in_var.h - internet interface header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
 * Copyright (c) 1985, 1986 Regents of the University of California.
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
 *      @(#)in_var.h    7.3 (Berkeley) 6/29/88
 */

/*
modification history
--------------------
02h,22sep92,rrr  added support for c++
02g,26may92,rrr  the tree shuffle
02f,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
02e,10jun91.del  added pragma for gnu960 alignment.
02d,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
02c,16apr89,gae  updated to new 4.3BSD.
02b,04nov87,dnw  removed KERNEL conditionals.
		 moved in_ifaddr definition to in.c.
		 moved ipintrq definition to ip_input.c.
02a,03apr87,ecs  added header and copyright.
*/

#ifndef __INCin_varh
#define __INCin_varh

#ifdef __cplusplus
extern "C" {
#endif

#if CPU_FAMILY==I960
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

/*
 * Interface address, Internet version.  One of these structures
 * is allocated for each interface with an Internet address.
 * The ifaddr structure contains the protocol-independent part
 * of the structure and is assumed to be first.
 */
struct in_ifaddr
    {
    struct	ifaddr ia_ifa;		/* protocol-independent info */
#define	ia_addr	ia_ifa.ifa_addr
#define	ia_broadaddr	ia_ifa.ifa_broadaddr
#define	ia_dstaddr	ia_ifa.ifa_dstaddr
#define	ia_ifp		ia_ifa.ifa_ifp
    u_long	ia_net;			/* network number of interface */
    u_long	ia_netmask;		/* mask of net part */
    u_long	ia_subnet;		/* subnet number, including net */
    u_long	ia_subnetmask;		/* mask of net + subnet */
    struct	in_addr ia_netbroadcast; /* broadcast addr for (logical) net */
    int		ia_flags;
    struct	in_ifaddr *ia_next;	/* next in list of internet addresses */
    };
/*
 * Given a pointer to an in_ifaddr (ifaddr),
 * return a pointer to the addr as a sockadd_in.
 */
#define	IA_SIN(ia) ((struct sockaddr_in *)(&((struct in_ifaddr *)ia)->ia_addr))

/* ia_flags */

#define	IFA_ROUTE	0x01		/* routing entry installed */

extern struct in_ifaddr *in_iaonnetof();

extern struct in_ifaddr *in_ifaddr;
extern struct ifqueue	ipintrq;		/* ip packet input queue */

#if CPU_FAMILY==I960
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCin_varh */
