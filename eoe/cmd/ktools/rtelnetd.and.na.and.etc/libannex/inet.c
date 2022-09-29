/*****************************************************************************
 *
 *        Copyright 1989, Xylogics, Inc.  ALL RIGHTS RESERVED.
 *
 * ALL RIGHTS RESERVED. Licensed Material - Property of Xylogics, Inc.
 * This software is made available solely pursuant to the terms of a
 * software license agreement which governs its use.
 * Unauthorized duplication, distribution or sale are strictly prohibited.
 *
 * Module Function:
 *
 *	BSD Internet address manipulations
 *	(correct for Xenix on the PC, others ?)
 *
 * Original Author:  Paul Mattes		Created on: 01/04/88
 *
 * Revision Control Information:
 *
 * $Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/inet.c,v 1.3 1996/10/04 12:08:39 cwilson Exp $
 *
 * This file created by RCS from:
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/inet.c,v $
 *
 * Revision History:
 *
 * $Log: inet.c,v $
 * Revision 1.3  1996/10/04 12:08:39  cwilson
 * latest rev
 *
 * Revision 1.8  1993/12/30  13:13:58  carlson
 * SPR 2084 -- made compatible with 64 bit machines.
 *
 * Revision 1.7  1991/06/21  09:49:01  barnes
 * changing _xxxx function names to xylo_xxxx
 *
 * Revision 1.6  89/04/14  13:37:39  loverso
 * useful comment about byte-orderness
 * 
 * 
 * Revision 1.5  89/04/11  01:07:25  loverso
 * comment htonl and friends, and changed htons to be in same form as htonl
 * 
 * Revision 1.4  89/04/05  12:40:17  loverso
 * Changed copyright notice
 * 
 * Revision 1.3  88/05/31  17:12:16  parker
 * fixes to get XENIX/SLIP to build again
 * 
 * Revision 1.2  88/05/24  18:35:44  parker
 * Changes for new install-annex script
 * 
 * Revision 1.1  88/04/15  11:57:35  mattes
 * Initial revision
 * 
 *
 * This file is currently under revision by:
 *
 * $Locker:  $
 *
 *****************************************************************************/

#define RCSDATE $Date: 1996/10/04 12:08:39 $
#define RCSREV	$Revision: 1.3 $
#define RCSID   "$Header: /proj/irix6.5.7m/isms/eoe/cmd/ktools/rtelnetd.and.na.and.etc/libannex/RCS/inet.c,v 1.3 1996/10/04 12:08:39 cwilson Exp $"

#ifndef lint
static char rcsid[] = RCSID;
#endif

/*****************************************************************************
 *									     *
 * Include files							     *
 *									     *
 *****************************************************************************/

#include "../inc/config.h"

#include "port/port.h"
#include <sys/types.h>
#include <ctype.h>
#include <netinet/in.h>

#include <stdio.h>

/*****************************************************************************
 *									     *
 * Local defines and macros						     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * Structure and union definitions					     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * External data							     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * Global data								     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * Static data								     *
 *									     *
 *****************************************************************************/

/*****************************************************************************
 *									     *
 * Forward definitions							     *
 *									     *
 *****************************************************************************/


/*
 * Network/external number representation conversions
 *
 * Network byte order (aka "big endian") has the four bytes in a 32-bit
 * number represented as "4321"; i.e., most-significant byte first.
 * Machines using this format are the 68K, SPARC, and Tahoe.
 *
 * Other machines might use "little endian" (aka "least-significant byte
 * first) which has its for bytes represented as "1234".  Machines using
 * this format are the VAX, 80x86, and NS32000.
 *
 * A PDP11 uses something different, represented by "3412".
 *
 * The included routines below are for "little endian" hosts
 * (and are valid for VAXes, PCs, and NS32000-based hosts, among others).
 *
 * For big-endian hosts, add a "#define big_endian".
 */
UINT32
xylo_htonl(hostlong)
UINT32 hostlong;
{
    union {
	UINT32  sp_long;
	u_char  sp_char[4];
    } SP;
    u_char ch;

#define Sp_long    SP.sp_long
#define Sp_char    SP.sp_char

    Sp_long = hostlong;
#ifndef big_endian
    ch = Sp_char[0];
    Sp_char[0] = Sp_char[3];
    Sp_char[3] = ch;
    ch = Sp_char[1];
    Sp_char[1] = Sp_char[2];
    Sp_char[2] = ch;
#endif

    return (Sp_long);
}

u_short
xylo_htons(hostshort)
u_short hostshort;
{
    union {
	u_short	sp_short;
	u_char	sp_char[2];
    } SP;
    u_char ch;

#define Sp_short   SP.sp_short
#define Sp_char    SP.sp_char

    Sp_short = hostshort;
#ifndef big_endian
    ch = Sp_char[0];
    Sp_char[0] = Sp_char[1];
    Sp_char[1] = ch; 
#endif

    return (Sp_short);
}

u_short
xylo_ntohs(s)
u_short s;
{
    return htons(s);
}

UINT32
xylo_ntohl(l)
UINT32 l;
{
    return htonl(l);
}



/*
 * Internet address interpretation routine.
 * All the network library routines call this
 * routine to interpret entries in the data bases
 * which are expected to be an address.
 * The value returned is in network order.
 */

UINT32
xylo_inet_addr(cp)
	register char *cp;
{
	register UINT32 val, base, n;
	register char c;
	UINT32 parts[4], *pp = parts;

again:
	/*
	 * Collect number up to ``.''.
	 * Values are specified as for C:
	 * 0x=hex, 0=octal, other=decimal.
	 */

	val = 0; base = 10;
	if (*cp == '0')
		base = 8, cp++;
	if (*cp == 'x' || *cp == 'X')
		base = 16, cp++;
	while (c = *cp) {
		if (isdigit(c)) {
			val = (val * base) + (c - '0');
			cp++;
			continue;
		}
		if (base == 16 && isxdigit(c)) {
			val = (val << 4) + (c + 10 - (islower(c) ? 'a' : 'A'));
			cp++;
			continue;
		}
		break;
	}
	if (*cp == '.') {

		/*
		 * Internet format:
		 *	a.b.c.d
		 *	a.b.c	(with c treated as 16-bits)
		 *	a.b	(with b treated as 24 bits)
		 */

		if (pp >= parts + 4)
			return (-1);
		*pp++ = val, cp++;
		goto again;
	}

	/*
	 * Check for trailing characters.
	 */

	if (*cp && !isspace(*cp))
		return (-1);
	*pp++ = val;
	/*
	 * Concoct the address according to
	 * the number of parts specified.
	 */

	n = pp - parts;
	switch (n) {

	case 1:				/* a -- 32 bits */
		val = parts[0];
		break;

	case 2:				/* a.b -- 8.24 bits */
		val = (parts[0] << 24) | (parts[1] & 0xffffff);
		break;

	case 3:				/* a.b.c -- 8.8.16 bits */
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16) |
			(parts[2] & 0xffff);
		break;

	case 4:				/* a.b.c.d -- 8.8.8.8 bits */
		val = (parts[0] << 24) | ((parts[1] & 0xff) << 16) |
		      ((parts[2] & 0xff) << 8) | (parts[3] & 0xff);
		break;

	default:
		return (-1);
	}
	val = htonl(val);
	return (val);
}

/*
 * Convert network-format internet address
 * to base 256 d.d.d.d representation.
 */

char *
xylo_inet_ntoa(in)
	struct in_addr in;
{
	static char b[18];
	register char *p;

	p = (char *)&in;

#define	UC(b)	(((int)b)&0xff)

	sprintf(b, "%d.%d.%d.%d", UC(p[0]), UC(p[1]), UC(p[2]), UC(p[3]));
	return (b);
}

