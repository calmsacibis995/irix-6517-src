#ifndef __ERRNO_H__
#define __ERRNO_H__
#ifdef __cplusplus
extern "C" {
#endif
/* Copyright (C) 1989 Silicon Graphics, Inc. All rights reserved.  */
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident "$Revision: 1.16 $" */

/* ANSI C Notes:
 *
 * - THE IDENTIFIERS APPEARING OUTSIDE OF #ifdef __EXTENSIONS__ IN THIS
 *   standard header ARE SPECIFIED BY ANSI!  CONFORMANCE WILL BE ALTERED
 *   IF ANY NEW IDENTIFIERS ARE ADDED TO THIS AREA UNLESS THEY ARE IN ANSI's
 *   RESERVED NAMESPACE. (i.e., unless they are prefixed by __[a-z] or
 *   _[A-Z].  For external objects, identifiers with the prefix _[a-z] 
 *   are also reserved.)
 *
 *  - Macros that begin with E and either a digit or an upper-case
 *    letter are reserved when including errno.h
 */

/*
 * Error codes
 */

#include <sys/errno.h>

#if (defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS))
extern int errno;

extern char *	_sys_errlist[];
extern int	_sys_nerr;
extern int	_oserror(void);
extern int	_setoserror(const int);

#if (defined(__EXTENSIONS__) && !defined(_POSIX_SOURCE))
extern char *	sys_errlist[];
extern int	sys_nerr;

extern void	perror(const char *);	/* defined in <stdio.h> */
extern char *	strerror(int);		/* defined in <string.h> */
extern int	oserror(void);
extern int	setoserror(const int);
#endif /* __EXTENSIONS__ && !_POSIX_SOURCE */

#endif /* _LANGUAGE_C || _LANGUAGE_C_PLUS_PLUS */

#ifdef __cplusplus
}
#endif
#endif /* !__ERRNO_H__ */
