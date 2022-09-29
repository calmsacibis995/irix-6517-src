#ifndef __sys_endian__
#define __sys_endian__
/*
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution is only permitted until one year after the first shipment
 * of 4.4BSD by the Regents.  Otherwise, redistribution and use in source and
 * binary forms are permitted provided that: (1) source distributions retain
 * this entire copyright notice and comment, and (2) distributions including
 * binaries display the following acknowledgement:  This product includes
 * software developed by the University of California, Berkeley and its
 * contributors'' in the documentation or other materials provided with the
 * distribution and in all advertising materials mentioning features or use
 * of this software.  Neither the name of the University nor the names of
 * its contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)endian.h	7.5 (Berkeley) 6/28/90
 */
#ident	"$Revision: 1.7 $"

/*
 * NOTE: Use of this file is deprecated. Though, if this file should be
 *       included in a program, standards.h should be included before
 *       endian.h
 */
#if !defined(_SGIAPI)
#error "<standards.h> must be included before <sys/endian.h>."
#endif

/*
 * Definitions for byte order,
 * according to byte significance from low address to high.
 */
#if _SGIAPI
#define	LITTLE_ENDIAN	1234	/* least-significant byte first (vax) */
#define	BIG_ENDIAN	4321	/* most-significant byte first (IBM, net) */
#define	PDP_ENDIAN	3412	/* LSB first in word, MSW first in long (pdp) */
#endif   /* _SGIAPI  */

#if _SGIAPI
#define BYTE_ORDER	_BYTE_ORDER
#endif  /* _SGIAPI  */

#ifdef _MIPSEB
#define	_BYTE_ORDER	4321	/* byte order on SGI 4D */
#endif /* _MIPSEB */

#ifdef _MIPSEL
#define	_BYTE_ORDER	1234	/* byte order on tahoe */
#endif /* _MIPSEL */

/*
 * Macros for network/external number representation conversion.
 */
#if _MIPSEB
#define	ntohl(x)	(x)
#define	ntohs(x)	(x)
#define	htonl(x)	(x)
#define	htons(x)	(x)

#if _SGIAPI
#define NTOHL(d)
#define NTOHS(d)
#define HTONL(d)
#define HTONS(d)
#endif  /* _SGIAPI  */

#endif  /* _MIPSEB  */

#if _MIPSEL
static unsigned long
ntohl(unsigned long x)
{
	return ((x << 24) | ((x & 0xff00) << 8) | ((x >> 8) & 0xff00)
		| (x >> 24));
}

static unsigned short
ntohs(unsigned short x)
{
	return (x << 8) | (x >> 8);
}

#define	htonl(x)	ntohl(x)
#define	htons(x)	ntohs(x)

#if _SGIAPI
#define NTOHL(d) ((d) = ntohl((d)))
#define NTOHS(d) ((d) = ntohs((u_short)(d)))
#define HTONL(d) ((d) = htonl((d)))
#define HTONS(d) ((d) = htons((u_short)(d)))
#endif  /* _SGIAPI  */

#endif  /* _MIPSEL  */

#endif /* __sys_endian__ */
