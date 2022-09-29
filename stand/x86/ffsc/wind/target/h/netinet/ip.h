/* ip.h - internet protocol header file */

/* Copyright 1984-1993 Wind River Systems, Inc. */
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
 *      @(#)ip.h        7.6.1.3 (Berkeley) 2/20/89
 */

/*
modification history
--------------------
02l,19jan94,jag  Move IPFORWARD from ip_input.c to this file.
02k,03mar93,ccc  added underscore to _BYTE_ORDER, _LITTLE_ENDIAN, _BIG_ENDIAN
		 and _PDP_ENDIAN.  SPR 2355.
02j,22sep92,rrr  added support for c++
02i,26may92,rrr  the tree shuffle
02h,26feb92,rrr  cleaned up the LSB4 macro
02g,30oct91,rrr  removed bitfields
02f,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
02e,10jun91.del  added pragma for gnu960 alignment.
02d,24mar91,del  added vxWorks.h for defining BYTE_ORDER.
02c,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
02b,16apr89,gae  updated to new 4.3BSD.
02a,03apr87,ecs  added header and copyright.
*/

#ifndef __INCiph
#define __INCiph

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"

#if CPU_FAMILY==I960
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

#ifndef _BYTE_ORDER
/*
 * Definitions for byte order,
 * according to byte significance from low address to high.
 */
#define _LITTLE_ENDIAN   1234    /* least-significant byte first (vax) */
#define _BIG_ENDIAN      4321    /* most-significant byte first (IBM, net) */
#define _PDP_ENDIAN      3412    /* LSB first in word, MSW first in long (pdp) */
#ifdef vax
#define _BYTE_ORDER      _LITTLE_ENDIAN
#else
#define _BYTE_ORDER      _BIG_ENDIAN      /* mc68000, tahoe, most others */
#endif
#endif	/* _BYTE_ORDER */

/*
 * Definitions for internet protocol version 4.
 * Per RFC 791, September 1981.
 */
#define	IPVERSION	4

#ifndef LSB4
#define LSB4(x)	((x)&0xf)	/* get the 4 least significant bits */
#endif

/*
 * Structure of an internet header, naked of options.
 *
 * We declare ip_len and ip_off to be short, rather than u_short
 * pragmatically since otherwise unsigned comparisons can result
 * against negative integers quite easily, and fail in subtle ways.
 */
struct ip {
#ifdef USE_BITFIELDS
#undef USE_BITFIELDS
#endif /* USE_BITFIELDS */
#ifdef USE_BITFIELDS
#if _BYTE_ORDER == _LITTLE_ENDIAN
	u_char	ip_hl:4,		/* header length */
		ip_v:4;			/* version */
#endif
#if _BYTE_ORDER == _BIG_ENDIAN
        u_char  ip_v:4,                 /* version */
                ip_hl:4;                /* header length */
#endif
#else
	u_char	ip_v_hl;		/* version (4msb) hdr length (4lsb) */
#endif
	u_char	ip_tos;			/* type of service */
	short	ip_len;			/* total length */
	u_short	ip_id;			/* identification */
	short	ip_off;			/* fragment offset field */
#define	IP_DF 0x4000			/* dont fragment flag */
#define	IP_MF 0x2000			/* more fragments flag */
	u_char	ip_ttl;			/* time to live */
	u_char	ip_p;			/* protocol */
	u_short	ip_sum;			/* checksum */
	struct	in_addr ip_src,ip_dst;	/* source and dest address */
};

#define IP_MAXPACKET    65535           /* maximum packet size */

/*
 * Definitions for options.
 */
#define	IPOPT_COPIED(o)		((o)&0x80)
#define	IPOPT_CLASS(o)		((o)&0x60)
#define	IPOPT_NUMBER(o)		((o)&0x1f)

#define	IPOPT_CONTROL		0x00
#define	IPOPT_RESERVED1		0x20
#define	IPOPT_DEBMEAS		0x40
#define	IPOPT_RESERVED2		0x60

#define	IPOPT_EOL		0		/* end of option list */
#define	IPOPT_NOP		1		/* no operation */

#define	IPOPT_RR		7		/* record packet route */
#define	IPOPT_TS		68		/* timestamp */
#define	IPOPT_SECURITY		130		/* provide s,c,h,tcc */
#define	IPOPT_LSRR		131		/* loose source route */
#define	IPOPT_SATID		136		/* satnet id */
#define	IPOPT_SSRR		137		/* strict source route */

/*
 * Offsets to fields in options other than EOL and NOP.
 */
#define	IPOPT_OPTVAL		0		/* option ID */
#define	IPOPT_OLEN		1		/* option length */
#define IPOPT_OFFSET		2		/* offset within option */
#define	IPOPT_MINOFF		4		/* min value of above */

/*
 * Time stamp option structure.
 */
struct	ip_timestamp {
	u_char	ipt_code;		/* IPOPT_TS */
	u_char	ipt_len;		/* size of structure (variable) */
	u_char	ipt_ptr;		/* index of current entry */
#ifdef USE_BITFIELDS
#undef USE_BITFIELDS
#endif /* USE_BITFIELDS */
#ifdef USE_BITFIELDS
#if _BYTE_ORDER == _LITTLE_ENDIAN
	u_char	ipt_flg:4,		/* flags, see below */
		ipt_oflw:4;		/* overflow counter */
#endif
#if _BYTE_ORDER == _BIG_ENDIAN
	u_char	ipt_oflw:4,		/* overflow counter */
		ipt_flg:4;		/* flags, see below */
#endif
#else
	u_char	ipt_oflw_flg;		/* oflow counter (4msb) flags (4lsb) */
#endif

	union ipt_timestamp {
		n_long	ipt_time[1];
		struct	ipt_ta {
			struct in_addr ipt_addr;
			n_long ipt_time;
		} ipt_ta[1];
	} ipt_timestamp;
};

/* flag bits for ipt_flg */
#define	IPOPT_TS_TSONLY		0		/* timestamps only */
#define	IPOPT_TS_TSANDADDR	1		/* timestamps and addresses */
#define	IPOPT_TS_PRESPEC	3		/* specified modules only */

/* bits for security (not byte swapped) */
#define	IPOPT_SECUR_UNCLASS	0x0000
#define	IPOPT_SECUR_CONFID	0xf135
#define	IPOPT_SECUR_EFTO	0x789a
#define	IPOPT_SECUR_MMMM	0xbc4d
#define	IPOPT_SECUR_RESTR	0xaf13
#define	IPOPT_SECUR_SECRET	0xd788
#define	IPOPT_SECUR_TOPSECRET	0x6bc5

/*
 * Internet implementation parameters.
 */
#define	MAXTTL		255		/* maximum time to live (seconds) */
#define	IPFRAGTTL	60		/* time to live for frags, slowhz */
#define	IPTTLDEC	1		/* subtracted when forwarding */

#define	IP_MSS		576		/* default maximum segment size */


#define IPFORWARDING    1		/* Set IP to always forward */

#if CPU_FAMILY==I960
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCiph */
