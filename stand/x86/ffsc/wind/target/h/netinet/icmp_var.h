/* icmp_var.h - internet control message protocol variable header file */

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
 *      @(#)icmp_var.h  7.4 (Berkeley) 6/29/88
 */

/*
modification history
--------------------
01i,22sep92,rrr  added support for c++
01h,26may92,rrr  the tree shuffle
01g,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01f,10jun91.del  added pragma for gnu960 alignment.
01e,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
02d,16apr89,gae  updated to new 4.3BSD.
02c,23jun88,rdc  made hist arrays in icmpstat be ICMP_MAXTYPE elements.
02b,04nov87,dnw  moved declaration of icmpstat structure to ip_icmp.c.
02a,03apr87,ecs  added header and copyright.
*/

#ifndef __INCicmp_varh
#define __INCicmp_varh

#ifdef __cplusplus
extern "C" {
#endif

#if CPU_FAMILY==I960
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */
/*
 * Variables related to this implementation
 * of the internet control message protocol.
 */
struct	icmpstat
    {
    /* statistics related to icmp packets generated */

    int	icps_error;		/* # of calls to icmp_error */
    int	icps_oldshort;		/* no error 'cuz old ip too short */
    int	icps_oldicmp;		/* no error 'cuz old was icmp */
    int	icps_outhist[ICMP_MAXTYPE + 1];

    /* statistics related to input messages processed */

    int	icps_badcode;		/* icmp_code out of range */
    int	icps_tooshort;		/* packet < ICMP_MINLEN */
    int	icps_checksum;		/* bad checksum */
    int	icps_badlen;		/* calculated bound mismatch */
    int	icps_reflect;		/* number of responses */
    int	icps_inhist[ICMP_MAXTYPE + 1];
    };

#if CPU_FAMILY==I960
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */
#ifdef __cplusplus
}
#endif

#endif /* __INCicmp_varh */
