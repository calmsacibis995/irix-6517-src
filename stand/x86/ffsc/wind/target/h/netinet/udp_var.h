/* udp_var.h - UDP kernel structure and variable header file */

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
 *      @(#)udp_var.h   7.4 (Berkeley) 6/29/88
 */

/*
modification history
--------------------
02j,14jan94,jag  Added variable udpNoPort for MIB-II support.
02i,22sep92,rrr  added support for c++
02h,26may92,rrr  the tree shuffle
02g,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
02f,14aug91,del  (intel) added fields to udpstat.
02e,10jun91,del  added pragma for gnu960 alignment.
02d,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
02c,16apr89,gae  updated to new 4.3BSD.
02b,04nov87,dnw  moved definitions of udb and udpstat to udp_usrreq.c
02a,03apr87,ecs  added header and copyright.
*/

#ifndef __INCudp_varh
#define __INCudp_varh

#ifdef __cplusplus
extern "C" {
#endif

#if CPU_FAMILY==I960
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

/* UDP kernel structures and variables. */

struct	udpiphdr
    {
    struct 	ipovly ui_i;		/* overlaid ip structure */
    struct	udphdr ui_u;		/* udp header */
    };
#define	ui_next		ui_i.ih_next
#define	ui_prev		ui_i.ih_prev
#define	ui_x1		ui_i.ih_x1
#define	ui_pr		ui_i.ih_pr
#define	ui_len		ui_i.ih_len
#define	ui_src		ui_i.ih_src
#define	ui_dst		ui_i.ih_dst
#define	ui_sport	ui_u.uh_sport
#define	ui_dport	ui_u.uh_dport
#define	ui_ulen		ui_u.uh_ulen
#define	ui_sum		ui_u.uh_sum

struct	udpstat
    {
    int	udps_hdrops;		/* bad header length */
    int	udps_badsum;		/* bad header checksum */
    int	udps_badlen;		/* bad packet length */
    int	udps_ipackets;		/* total packets received, including errors */
    int	udps_opackets;		/* total packets sent */
    int	udps_noportbcast;	/* broadcast packets, no port */
    int	udps_fullsock;		/* packets dropped socket recv side is full */
    };

extern long udpNoPorts;		/* MIB-II support. Counts # of packstes drop */

#define	UDP_TTL		30	/* default time to live for UDP packets */

#if CPU_FAMILY==I960
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCudp_varh */
