#ifndef __STROPTS_H__
#define __STROPTS_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.15 $"
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


#include <standards.h>

/*
 * Streams user options definitions.
 */

#include <sys/stropts.h>

#if !_SGIAPI && _XOPEN4UX && !defined(_KERNEL)
#include <unistd.h>
#endif	/* !_SGIAPI && _XOPEN4UX && !_KERNEL */

/*
 * These three are defined in XPG4 in stropts.h but are needed
 * in unistd.h.
 */
extern int fattach(int, const char *);
extern int fdetach(const char *);
extern int ioctl(int, int, ...);

extern int isastream(int);
extern int getmsg(int, struct strbuf *, struct strbuf *, int *);
extern int putmsg(int, const struct strbuf *, const struct strbuf *, int);

extern int getpmsg(int, struct strbuf *, struct strbuf *, int *, int *);
extern int putpmsg(int, const struct strbuf *, const struct strbuf *, int, int);

#ifdef __cplusplus
}
#endif
#endif /* !__STROPTS_H__ */
