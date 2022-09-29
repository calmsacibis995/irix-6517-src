/* af.h - address family header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
 * Copyright (c) 1980, 1986 Regents of the University of California.
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
 *      @(#)af.h        7.3 (Berkeley) 6/27/88
 */


/*
modification history
--------------------
01h,22sep92,rrr  added support for c++
01g,26may92,rrr  the tree shuffle
01f,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01e,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01d,16apr89,gae  updated to new 4.3BSD.
01c,03apr87,ecs  added header and copyright.
01b,22dec86,dnw  added IMPORT to declaration of afswitch to satisfy
		   Intermetrics assembler.
*/

#ifndef __INCafh
#define __INCafh

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Address family routines,
 * used in handling generic sockaddr structures.
 *
 * Hash routine is called
 *	af_hash(addr, h);
 *	struct sockaddr *addr; struct afhash *h;
 * producing an afhash structure for addr.
 *
 * Netmatch routine is called
 *	af_netmatch(addr1, addr2);
 * where addr1 and addr2 are sockaddr *.  Returns 1 if network
 * values match, 0 otherwise.
 */
struct afswitch {
	int	(*af_hash)();
	int	(*af_netmatch)();
};

struct afhash {
	u_int	afh_hosthash;
	u_int	afh_nethash;
};

IMPORT struct afswitch afswitch[];

#ifdef __cplusplus
}
#endif

#endif /* __INCafh */
