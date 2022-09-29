/* Copyright (C) 1989 Silicon Graphics, Inc. All rights reserved.  */
/* ------------------------------------------------------------------ */
/* | Copyright Unpublished, MIPS Computer Systems, Inc.  All Rights | */
/* | Reserved.  This software contains proprietary and confidential | */
/* | information of MIPS and its suppliers.  Use, disclosure or     | */
/* | reproduction is prohibited without the prior express written   | */
/* | consent of MIPS.                                               | */
/* ------------------------------------------------------------------ */
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/
#ifndef __PWD_H__
#define __PWD_H__

#ident "$Revision: 1.35 $"

#ifdef __cplusplus
extern "C" {
#endif
#include <standards.h>
#include <sys/types.h>

struct passwd {
	char	*pw_name;
	char	*pw_passwd;
	uid_t	pw_uid;
	gid_t	pw_gid;
	char	*pw_age;
	char	*pw_comment;
	char	*pw_gecos;
	char	*pw_dir;
	char	*pw_shell;
};

#if _SGIAPI 
struct xpasswd {
	/* The following should be identical to struct passwd */
	char	*pw_name;
	char	*pw_passwd;
	uid_t	pw_uid;
	gid_t	pw_gid;
	char	*pw_age;
	char	*pw_comment;
	char	*pw_gecos;
	char	*pw_dir;
	char	*pw_shell;
	/* sgi extension: */
	int	pw_origin;	 /* type of entry, defined below. */
	char	*pw_yp_passwd;	 /* if non-null, yp passwd value */
	char	*pw_yp_gecos;	 /* if non-null, yp gecos field */
	char	*pw_yp_dir;	 /* if non-null, yp home directory */
	char	*pw_yp_shell;	 /* if non-null, yp login shell */
	char	*pw_yp_netgroup; /* if non-null, yp netgroup name */
};

/* pw_origin values: */
#define _PW_LOCAL	0
#define _PW_YP_USER	1
#define _PW_YP_NETGROUP	2
#define _PW_YP_ALL	3
#define	_PW_YP_REMOTE	4

struct comment {
	char	*c_dept;
	char	*c_name;
	char	*c_acct;
	char	*c_bin;
};
#endif	/* _SGIAPI */

extern struct passwd *	getpwuid(uid_t);
extern struct passwd *	getpwnam(const char *);

#if _XOPEN4UX
extern void		endpwent(void);
extern struct passwd *	getpwent(void);
extern void		setpwent(void);
#endif  /* _XOPEN4UX */

#if _SGIAPI
#include <stdio.h>
extern struct passwd *	fgetpwent(FILE *);
extern int		putpwent(const struct passwd *, FILE *);
extern void		_yp_setpwent(void);
extern struct xpasswd *	_yp_getpwent(void);
extern struct xpasswd *	_pw_interpret(char *, int, FILE *);
extern int		lckpwdf(void);
extern int		ulckpwdf(void);
extern struct passwd *	fgetpwent_r(FILE *, struct passwd *, char *, size_t);
extern struct passwd *	getpwent_r(struct passwd *, char *, size_t);
#endif	/* _SGIAPI */

#if _POSIX1C
extern int getpwuid_r(uid_t, struct passwd *, char *, size_t, struct passwd **);
extern int getpwnam_r(const char *, struct passwd *, char *, size_t, struct passwd **);
#endif

#ifdef __cplusplus
}
#endif
#endif /* !__PWD_H__ */
