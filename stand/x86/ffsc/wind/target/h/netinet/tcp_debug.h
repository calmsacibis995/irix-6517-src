/* tcp_debug.h - tcp debug header file */

/* Copyright 1984-1995 Wind River Systems, Inc. */
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
 *      @(#)tcp_debug.h 7.3 (Berkeley) 6/29/88
 */

/*
modification history
--------------------
02l,13mar95,dzb  removed prototype for tcp_report (SPR #3928).
02k,12sep94,dzb  added td_tiphdr to struct tcp_debug (SPR #1552).
02j,22sep92,rrr  added support for c++
02i,26may92,rrr  the tree shuffle
02h,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
02g,10jun91.del  added pragma for gnu960 alignment.
02f,26oct90,hjb  moved declaration of tcp_debug[], tcp_debx, tanames[]
		 to tcp_debug.c.
02e,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
02d,10aug90,dnw  changed tcpTraceRtn from FUNCPTR to VOIDFUNCPTR.
02c,26jun90,hjb  added tcpTraceRtn.
02b,16apr89,gae  updated to new 4.3BSD.
02a,03apr87,ecs  added header and copyright.
*/


#ifndef __INCtcp_debugh
#define __INCtcp_debugh

#ifdef __cplusplus
extern "C" {
#endif

/* includes  */

#include "tcp_var.h"
#include "tcpip.h"

#if CPU_FAMILY==I960
#pragma align 1			/* tell gcc960 not to optimize alignments */
#endif	/* CPU_FAMILY==I960 */

#define TCP_NDEBUG		100	/* length of debug info array */
#define TCP_DEBUG_NUM_DEFAULT	20	/* default display number */

struct	tcp_debug {
	n_time	td_time;
	short	td_act;
	short	td_ostate;
	caddr_t	td_tcb;
	caddr_t	td_tiphdr;
	struct	tcpiphdr td_ti;
	struct	tcpcb td_cb;
	short	td_req;
};

#define	TA_INPUT 	0
#define	TA_OUTPUT	1
#define	TA_USER		2
#define	TA_RESPOND	3
#define	TA_DROP		4

IMPORT VOIDFUNCPTR tcpTraceRtn;
IMPORT VOIDFUNCPTR tcpReportRtn;

#if CPU_FAMILY==I960
#pragma align 0			/* turn off alignment requirement */
#endif	/* CPU_FAMILY==I960 */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern void	tcpTraceInit (void);

#else	/* __STDC__ */

extern void	tcpTraceInit ();

#endif	/* __STDC__ */
 
#ifdef __cplusplus
}
#endif

#endif /* __INCtcp_debugh */
