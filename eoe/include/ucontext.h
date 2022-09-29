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

#ifndef _UCONTEXT_H
#define _UCONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

#ident "$Revision: 1.4 $"

#include <sys/ucontext.h>

extern int getcontext(ucontext_t *);
extern int setcontext(const ucontext_t *);
extern int swapcontext(ucontext_t *, const ucontext_t *);
extern void makecontext(ucontext_t *, void(*)(), int, ...);

#if (defined(_COMPILER_VERSION) && (_COMPILER_VERSION >= 730))
#pragma unknown_control_flow (getcontext,swapcontext)
#endif

#ifdef __cplusplus
}
#endif

#endif 	/* _UCONTEXT_H */
