#ifndef __SIGINFO_H__
#define __SIGINFO_H__
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
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include <sys/types.h>
#include <sys/siginfo.h>

struct siginfolist {
	int nsiginfo;
	char **vsiginfo;
};

extern char * _sys_illlist[];
extern char * _sys_fpelist[];
extern char * _sys_segvlist[];
extern char * _sys_buslist[];
extern char * _sys_traplist[];
extern char * _sys_cldlist[];
extern struct siginfolist _sys_siginfolist[];

extern void psiginfo(siginfo_t *, const char *);
extern void psignal(int, const char *);

#ifdef __cplusplus
}
#endif
#endif /* !__SIGINFO_H__ */
