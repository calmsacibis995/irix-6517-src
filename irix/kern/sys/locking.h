/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SVC_LOCKING_H	/* wrapper symbol for kernel use */
#define _SVC_LOCKING_H	/* subject to change without notice */

/*	Copyright (c) 1987, 1988 Microsoft Corporation	*/
/*	  All Rights Reserved	*/

/*	This Module contains Proprietary Information of Microsoft  */
/*	Corporation and should be treated as Confidential.	   */

#ident	"@(#)uts-3b2:svc/locking.h	1.2"

/*
 *   Flag values for XENIX locking() system call (os/xsys.c)
 */

#define LK_UNLCK  0	/* unlock request */
#define LK_LOCK   1	/* lock request */
#define LK_NBLCK  20	/* non-blocking lock request */
#define LK_RLCK   3	/* read permitted only lock request */
#define LK_NBRLCK 4	/* non-blocking read only lock request */

#endif /* _SVC_LOCKING_H */
