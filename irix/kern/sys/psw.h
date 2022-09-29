#ifndef _SYS_PSW_H_
#define _SYS_PSW_H_

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*#ident	"@(#)kern-port:sys/psw.h	10.2"*/
#ident	"$Revision: 3.6 $"


/*
	PSW structure. For MIPS the PSW is just the status register.
*/

typedef int psw_t;


/*
 *	The following is the initial psw for process 0.
 *	All interrupts enabled, previously user mode.
 */

#define	IPSW	(SR_IMASK | SR_KUP | SR_IEP)

#endif
