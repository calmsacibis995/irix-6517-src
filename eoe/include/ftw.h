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
#ifndef __FTW_H__
#define __FTW_H__
#ident "$Revision: 1.27 $"

#ifdef __cplusplus
extern "C" {
#endif

#include <standards.h>

/*
 *	Codes for the third argument to the user-supplied function
 *	which is passed as the second argument to ftwalk
 */

#define	FTW_F	0	/* file */
#define	FTW_D	1	/* directory */
#define	FTW_DNR	2	/* directory without read permission */
#define	FTW_NS	3	/* unknown type, stat failed */
#define FTW_SL	4	/* symbolic link */
#define	FTW_DP	6	/* directory */
#define FTW_SLN	7	/* symbolic link that points to nonexistent file */

/*
 *	Codes for the fourth argument to nftw.  You can specify the
 *	union of these flags.
 */

#define FTW_PHYS	01  /* use lstat instead of stat */
#define FTW_MOUNT	02  /* do not cross a mount point */
#define FTW_CHDIR	04  /* chdir to each directory before reading */
#define FTW_DEPTH	010 /* call descendents before calling the parent */

#if _XOPEN4UX || _LFAPI
struct FTW
{
#if _SGIAPI
	int	quit;
#else
	int	__quit;
#endif
	int	base;
	int	level;
};
#endif /* _XOPEN4UX || _LFAPI */

#if _SGIAPI || _ABIAPI
/*
 * legal values for quit
 */
#define FTW_SKD		1
#define FTW_FOLLOW	2
#define FTW_PRUNE	4
#endif	/* _SGIAPI || _ABIAPI */

#include <sys/types.h>
#include <sys/stat.h>

extern int ftw(const char *,
		int (*)(const char *, const struct stat *, int), int);

#if _XOPEN4UX
extern int nftw(const char *,
		int (*)(const char *, const struct stat *, int, struct FTW *),
		int, int);
#endif	/* _XOPEN4UX */

#if _SGIAPI
extern int _xftw(int, const char *, int (*)(const char *, const struct stat *,
		int), int);

#if !(((_MIPS_SIM == _MIPS_SIM_NABI32) || \
	(_MIPS_SIM == _MIPS_SIM_ABI64)) && defined(_FTW_INTERNAL))
extern int _xftw64(int, const char *, int (*)(const char *,
		const struct stat64 *, int), int);
#endif
#endif /* _SGIAPI */

#if _LFAPI
#if !(((_MIPS_SIM == _MIPS_SIM_NABI32) || \
	(_MIPS_SIM == _MIPS_SIM_ABI64)) && defined(_FTW_INTERNAL))
extern int ftw64(const char *,
		int (*)(const char *, const struct stat64 *,
		int), int);
extern int nftw64(const char *,
		int (*)(const char *, const struct stat64 *,
		int, struct FTW *), int, int);
#endif

#define XFTWVER		2	/* version of file tree walk */
#endif	/* _LFAPI */

#ifdef __cplusplus
}
#endif
#endif /* !__FTW_H__ */
