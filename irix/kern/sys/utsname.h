/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _SYS_UTSNAME_H	/* wrapper symbol for kernel use */
#define _SYS_UTSNAME_H	/* subject to change without notice */

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 3.35 $"
#include <standards.h>
/*
 * If you are compiling the kernel, the value used in initializing
 * the utsname structure in kernel.cf must be the same as SYS_NMLN.
 */

#define _SYS_NMLN	257	/* 4.0 size of utsname elements.*/
				/* Must be at least 257 to 	*/
				/* support Internet hostnames.  */

struct utsname {
	char	sysname[_SYS_NMLN];
	char	nodename[_SYS_NMLN];
	char	release[_SYS_NMLN];
	char	version[_SYS_NMLN];
	char	machine[_SYS_NMLN];
#if defined(m_type) || _ABIAPI
	char	__m_type[_SYS_NMLN];
#else
	char	m_type[_SYS_NMLN];
#endif
#if defined(base_rel) || _ABIAPI
	char	__base_rel[_SYS_NMLN];
#else
	char	base_rel[_SYS_NMLN];
#endif
	char	__reserve5[_SYS_NMLN];		/* reserved for future use */
	char	__reserve4[_SYS_NMLN];		/* reserved for future use */
	char	__reserve3[_SYS_NMLN];		/* reserved for future use */
	char	__reserve2[_SYS_NMLN];		/* reserved for future use */
	char	__reserve1[_SYS_NMLN];		/* reserved for future use */
	char	__reserve0[_SYS_NMLN];		/* reserved for future use */
};


#if _SGIAPI && !defined(SYS_NMLN)
#define	SYS_NMLN	_SYS_NMLN
#endif /*  _SGIAPI && !defined(SYS_NMLN) */

#ifdef _KERNEL

extern struct utsname	utsname;
extern char		hostname[];
extern short		hostnamelen;
extern char		domainname[];
extern short		domainnamelen;
extern char		uname_release[];
extern char		uname_version[];

#else /* !_KERNEL */

#if _SGIAPI || _ABIAPI
int nuname(struct utsname *);
#endif	/* _SGIAPI */

#if _ABIAPI
/*
 * sigh - not POSIX/XPG compliant...
 */
/* REFERENCED */
static int
uname(struct utsname *__buf)
{
	return nuname(__buf);
}
#else
int uname(struct utsname *);
#endif /* _ABIAPI */

#endif	/* KERNEL */

#ifdef __cplusplus
}
#endif  /* __cplusplus */
#endif	/* _SYS_UTSNAME_H */
