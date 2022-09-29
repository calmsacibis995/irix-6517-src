#ifndef __DIAL_H__
#define __DIAL_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.5 $"
/*
*
* Copyright 1992, Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/
/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)head.usr:dial.h	1.2.2.1"

/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#ident	"@(#)head.usr:dial.h	1.2.4.1"


#ifndef  _SYS_TERMIO_H
#include <sys/termio.h>
#endif

/* uucico routines need these */
/*#define DIAL */

/* The following are no longer used by dial() and may be out of date.	*/
/* They are included here only to maintain source compatibility.	*/
#define STANDALONE
#define DEVDIR	"/dev/"				/* device path		*/
#define LOCK	"/var/spool/uucp/LCK.."		/* lock file semaphore	*/
#define DVC_LEN	80	/* max NO of chars in TTY-device path name	*/
/* End of unused definitions						*/

		/* error mnemonics */

#define	TRUE	1
#define FALSE	0
#define INTRPT	(-1)	/* interrupt occured */
#define D_HUNG	(-2)	/* dialer hung (no return from write) */
#define NO_ANS	(-3)	/* no answer (caller script failed) */
#define ILL_BD	(-4)	/* illegal baud-rate */
#define A_PROB	(-5)	/* acu problem (open() failure) */
#define L_PROB	(-6)	/* line problem (open() failure) */
#define NO_Ldv	(-7)	/* can't open Devices file */
#define DV_NT_A	(-8)	/* requested device not available */
#define DV_NT_K	(-9)	/* requested device not known */
#define NO_BD_A	(-10)	/* no device available at requested baud */
#define NO_BD_K	(-11)	/* no device known at requested baud */
#define DV_NT_E (-12)	/* requested speed does not match */
#define BAD_SYS (-13)	/* system not in Systems file */
#define CS_PROB (-14)	/* connection server related error */

typedef struct{
	int	version;	/* for future modifications to structure format */
	char	*service;	/* name of service to use(default = "cu") */
	char	*class;		/* "class" of device to use */
	char	*protocol;	/* returns the protocol string for the connection made */
	char	*reserved1;	/* reserved for future expansion */
} CALL_EXT;

typedef struct {
	struct termio *attr;	/* ptr to termio attribute struct */
	int	baud;		/* unused */
	int	speed;		/* 212A modem: low=300, high=1200 */
	char	*line;		/* device name for out-going line */
	char	*telno;		/* ptr to tel-no/system name string */
	int	modem;		/* unused */
	char	*device;	/* pointer to a CALL_EXT structure */
				/* this was unused previously */
	int	dev_len;	/* unused */
} CALL;

extern int dial(CALL);
extern void undial(int);

#ifdef __cplusplus
}
#endif
#endif /* !__DIAL_H__ */
