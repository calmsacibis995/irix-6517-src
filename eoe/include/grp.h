#ifndef __GRP_H__
#define __GRP_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.33 $"
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
#include <sys/types.h>

struct	group {	/* see getgrent(3) */
	char	*gr_name;
	char	*gr_passwd;
	gid_t	gr_gid;
	char	**gr_mem;
};

extern struct group *getgrgid(gid_t);
extern struct group *getgrnam(const char *);

#if _XOPEN4UX
extern struct group *getgrent(void);
extern void endgrent(void);
extern void setgrent(void);
#endif  /* _XOPEN4UX */

#if _SGIAPI || _ABIAPI
#ifdef _BSD_COMPAT
extern int      initgroups(char *, int);
#else
extern int      initgroups(const char *, gid_t);
#endif /* _BSD_COMPAT */
#endif

#if _SGIAPI
#include <stdio.h>
extern struct group *fgetgrent(FILE *);
extern int BSDinitgroups(char *, int);
extern int getgrmember(const char *, gid_t [], int, int);
extern struct group *	fgetgrent_r(FILE *, struct group *, char *, size_t);
extern struct group *	getgrent_r(struct group *, char *, size_t);
#endif /* _SGIAPI */

#if _POSIX1C
extern int getgrgid_r(gid_t, struct group *, char *, size_t, struct group **);
extern int getgrnam_r(const char *, struct group *, char *, size_t, struct group **);
#endif

#ifdef __cplusplus
}
#endif
#endif /* !__GRP_H__ */
