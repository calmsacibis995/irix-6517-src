#ifndef __MON_H__
#define __MON_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.16 $"
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
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/



struct hdr {
	char	*lpc;
	char	*hpc;
	int	nfns;
};

struct cnt {
	char	*fnpc;
	long	mcnt;
};

typedef unsigned short WORD;
typedef unsigned short _WORD;	/* XXX */


#define _SUSPEND_PROF   0
#define _RESUME_PROF    1
#define _NEW_PROF_PHASE 2

#define MON_OUT	"mon.out"
#define MPROGS0	(150 * sizeof(WORD))	/* 300 for pdp11, 600 for 32-bits */
#define MSCALE0	4
#ifndef NULL
#define NULL	0L
#endif

extern void monitor(int (*)(), int (*)(), WORD *, int, int);
extern void     monstartup(int (*)(), int (*)());
extern void     moncontrol(int);

#ifdef __cplusplus
}
#endif
#endif /* !__MON_H__ */
