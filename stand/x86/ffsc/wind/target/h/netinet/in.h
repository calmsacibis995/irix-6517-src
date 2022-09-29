/* in.h - internet header file */

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
 *      @(#)in.h        7.6 (Berkeley) 6/29/88
 */

/*
modification history
--------------------
01n,22sep92,rrr  added support for c++
01m,26may92,rrr  the tree shuffle
01l,07apr92,yao  changed BYTE_ORDER to _BYTE_ORDER, LITTLE/BIG_ENDIAN to
		 _LITTLE/BIG_ENDIAN.
01k,06nov91,rrr  removed `extern struct protosw inetsw[];' to shut up warnings
01j,04oct91,rrr  passed through the ansification filter
		  -changed includes to have absolute path from h/
		  -fixed #else and #endif
		  -changed copyright notice
01i,12may91,gae  restored ntoh macros removed in 01g.
01h,10jun91.del  added pragma for gnu960 alignment.
01g,18may91,gae  removed unused macros for LITTLE_ENDIAN.
01f,23mar91,del  checks BYTE_ORDER as set in vxWorks.h.
01e,15apr89,gae  updated to new 4.3BSD.
01d,28may88,dnw  added include of types.h.
01c,04nov87,dnw  removed unnecessary KERNEL conditionals.
01b,08jan87,jlf  added ifndef's to keep from being included twice.
*/

#ifndef __INCinh
#define __INCinh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "sys/types.h"

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

/*
 * Constants and structures defined by the internet system,
 * Per RFC 790, September 1981.
 */

/*
 * Protocols
 */
#define	IPPROTO_IP		0		/* dummy for IP */
#define	IPPROTO_ICMP		1		/* control message protocol */
#define	IPPROTO_GGP		3		/* gateway^2 (deprecated) */
#define	IPPROTO_TCP		6		/* tcp */
#define	IPPROTO_EGP		8		/* exterior gateway protocol */
#define	IPPROTO_PUP		12		/* pup */
#define	IPPROTO_UDP		17		/* user datagram protocol */
#define	IPPROTO_IDP		22		/* xns idp */

#define	IPPROTO_RAW		255		/* raw IP packet */
#define	IPPROTO_MAX		256


/*
 * Ports < IPPORT_RESERVED are reserved for
 * privileged processes (e.g. root).
 * Ports > IPPORT_USERRESERVED are reserved
 * for servers, not necessarily privileged.
 */
#define	IPPORT_RESERVED		1024
#define	IPPORT_USERRESERVED	5000

/*
 * Link numbers
 */
#define	IMPLINK_IP		155
#define	IMPLINK_LOWEXPER	156
#define	IMPLINK_HIGHEXPER	158

/*
 * Internet address (a structure for historical reasons)
 */
struct in_addr {
	u_long s_addr;
};

/*
 * Definitions of bits in internet address integers.
 * On subnets, the decomposition of addresses to host and net parts
 * is done according to subnet mask, not the masks here.
 */
#define	IN_CLASSA(i)		(((long)(i) & 0x80000000) == 0)
#define	IN_CLASSA_NET		0xff000000
#define	IN_CLASSA_NSHIFT	24
#define	IN_CLASSA_HOST		0x00ffffff
#define	IN_CLASSA_MAX		128

#define	IN_CLASSB(i)		(((long)(i) & 0xc0000000) == 0x80000000)
#define	IN_CLASSB_NET		0xffff0000
#define	IN_CLASSB_NSHIFT	16
#define	IN_CLASSB_HOST		0x0000ffff
#define	IN_CLASSB_MAX		65536

#define	IN_CLASSC(i)		(((long)(i) & 0xe0000000) == 0xc0000000)
#define	IN_CLASSC_NET		0xffffff00
#define	IN_CLASSC_NSHIFT	8
#define	IN_CLASSC_HOST		0x000000ff

#define IN_CLASSD(i)            (((long)(i) & 0xf0000000) == 0xe0000000)
#define IN_MULTICAST(i)         IN_CLASSD(i)

#define IN_EXPERIMENTAL(i)      (((long)(i) & 0xe0000000) == 0xe0000000)
#define IN_BADCLASS(i)          (((long)(i) & 0xf0000000) == 0xf0000000)

#define INADDR_ANY              (u_long)0x00000000
#define INADDR_BROADCAST        (u_long)0xffffffff      /* must be masked */
#define INADDR_NONE             0xffffffff              /* -1 return */

#define IN_LOOPBACKNET          127                     /* official! */


/*
 * Socket address, internet style.
 */
struct sockaddr_in {
	short	sin_family;
	u_short	sin_port;
	struct	in_addr sin_addr;
	char	sin_zero[8];
};

/*
 * Options for use with [gs]etsockopt at the IP level.
 */
#define	IP_OPTIONS	1		/* set/get IP per-packet options */


/*
 * Macros for number representation conversion.
 */
#if	_BYTE_ORDER==_BIG_ENDIAN
#define	ntohl(x)	(x)
#define	ntohs(x)	(x)
#define	htonl(x)	(x)
#define	htons(x)	(x)
#endif	/* _BYTE_ORDER==_BIG_ENDIAN */

#if	_BYTE_ORDER==_LITTLE_ENDIAN

#if	FALSE
UINT16 ntohs ();
UINT16 htons ();
UINT32 ntohl ();
UINT32 htonl ();
#endif	/* FALSE */

#define ntohl(x)	((((x) & 0x000000ff) << 24) | \
			 (((x) & 0x0000ff00) <<  8) | \
			 (((x) & 0x00ff0000) >>  8) | \
			 (((x) & 0xff000000) >> 24))

#define htonl(x)	((((x) & 0x000000ff) << 24) | \
			 (((x) & 0x0000ff00) <<  8) | \
			 (((x) & 0x00ff0000) >>  8) | \
			 (((x) & 0xff000000) >> 24))

#define ntohs(x)	((((x) & 0x00ff) << 8) | \
			 (((x) & 0xff00) >> 8))

#define htons(x)	((((x) & 0x00ff) << 8) | \
			 (((x) & 0xff00) >> 8))

#endif	/* BYTE_ORDER==LITTLE_ENDIAN */

extern	struct domain inetdomain;
struct	in_addr in_makeaddr();
u_long	in_netof(), in_lnaof();

extern void in_ifaddr_remove();

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

#ifdef __cplusplus
}
#endif

#endif /* __INCinh */
