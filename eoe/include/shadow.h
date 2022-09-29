#ifndef __SHADOW_H__
#define __SHADOW_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.7 $"
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
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#define PASSWD 		"/etc/passwd"
#define SHADOW		"/etc/shadow"
#define OPASSWD		"/etc/opasswd"
#define OSHADOW 	"/etc/oshadow"
#define PASSTEMP	"/etc/ptmp"
#define SHADTEMP	"/etc/stmp"

#define	DAY		(24L * 60 * 60) /* 1 day in seconds */
#define DAY_NOW		(long)time((long *)0) / DAY
			/* The above timezone variable is set by a call to
			   any ctime(3c) routine.  Programs using the DAY_NOW
			   macro must call one of the ctime routines, 
			   e.g. tzset(), BEFORE referencing DAY_NOW */

/* The spwd structure is used in the retreval of information from
   /etc/shadow.  It is used by routines in the libos library */

struct spwd {
	char *sp_namp ; /* user name */
	char *sp_pwdp ; /* user password */
	long sp_lstchg ; /* password lastchanged date */
	long sp_min ; /* minimum number of days between password changes */
	long sp_max ; /* number of days password is valid */
	long sp_warn ; /* number of days to warn user to change passwd */
	long sp_inact ; /* number of days the login may be inactive */
	long sp_expire ; /* date when the login is no longer valid */
	unsigned long  sp_flag; /* currently not being used */
} ;


#ifndef _STDIO_H
#include <stdio.h>
#endif

/* Declare all shadow password functions */

extern void	setspent(void), endspent(void);
extern	struct	spwd	*getspent(void), *fgetspent(FILE *), *getspnam(const char *), *getspnam_r(const char *, struct spwd *, char *, int);
extern	int	putspent(const struct spwd *, FILE *), lckpwdf(void), ulckpwdf(void);

/* SGI specific */
extern struct spwd *fgetspent_r(FILE *, struct spwd *, char *, int);
extern struct spwd *fgetspent(FILE *);
extern struct spwd *getspent_r(struct spwd *, char *, int);
extern struct spwd *getspnam_r(const char *, struct spwd *, char *, int);

#ifdef __cplusplus
}
#endif
#endif /* !__SHADOW_H__ */
