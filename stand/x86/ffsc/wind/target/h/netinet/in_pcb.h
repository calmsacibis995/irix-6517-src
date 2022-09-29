/* in_pcb.h - internet protocol control block header file */

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
 *      @(#)in_pcb.h    7.3 (Berkeley) 6/29/88
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
02b,04nov87,dnw  removed unnecessary KERNEL conditionals.
02a,03apr87,ecs  added header and copyright.
*/

#ifndef __INCin_pcbh
#define __INCin_pcbh

#ifdef __cplusplus
extern "C" {
#endif

#if CPU_FAMILY==I960
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */
/*
 * Common structure pcb for internet protocol implementation.
 * Here are stored pointers to local and foreign host table
 * entries, local and foreign socket numbers, and pointers
 * up (to a socket structure) and down (to a protocol-specific)
 * control block.
 */
struct inpcb
    {
    struct	inpcb *inp_next;	/* pointers to other pcb's */
    struct	inpcb *inp_prev;
    struct	inpcb *inp_head;	/* pointer back to chain of inpcb's
					   for this protocol */
    struct	in_addr inp_faddr;	/* foreign host table entry */
    u_short	inp_fport;		/* foreign port */
    struct	in_addr inp_laddr;	/* local host table entry */
    u_short	inp_lport;		/* local port */
    struct	socket *inp_socket;	/* back pointer to socket */
    caddr_t	inp_ppcb;		/* pointer to per-protocol pcb */
    struct	route inp_route;	/* placeholder for routing entry */
    struct	mbuf *inp_options;	/* IP options */
    };

#define	INPLOOKUP_WILDCARD	1
#define	INPLOOKUP_SETLOCAL	2

#define	sotoinpcb(so)	((struct inpcb *)(so)->so_pcb)

struct	inpcb *in_pcblookup();

#if CPU_FAMILY==I960
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCin_pcbh */
