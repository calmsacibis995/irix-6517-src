/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/readdir.c	1.12"
/*
	readdir -- C library extension routine

*/

#include	<sgidefs.h>
#ifdef __STDC__
	#pragma weak readdir = _readdir
	#pragma weak readdir_r = _readdir_r
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	#pragma weak readdir64 = _readdir
	#pragma weak _readdir64 = _readdir
	#pragma weak readdir64_r = _readdir_r
	#pragma weak _readdir64_r = _readdir_r
#endif /* SIM_ABI64 || SIM_NABI32 */
#endif
#include	"synonyms.h"
#include	"mplib.h"
#include	<sys/types.h>
#include	<sys/systeminfo.h>
#include	<dirent.h>
#include	<string.h>
#include	<errno.h>

extern int __readdirsize;
typedef int (*gdents_t)(int, dirent_t *, int, int *);

#ifdef _LIBC_RUN_ON_53
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
static gdents_t gdents = (gdents_t)ngetdents;
#else
static gdents_t gdents = (gdents_t)NULL;
#endif

#if _MIPS_SIM == _MIPS_SIM_ABI32
static void
gdents_init(void)
{
	char majvers[5], minvers[5], minvc;

	if (sysinfo(_MIPS_SI_OSREL_MAJ, majvers, sizeof(majvers)) < 1 ||
	    sysinfo(_MIPS_SI_OSREL_MIN, minvers, sizeof(minvers)) < 1) {
		gdents = (gdents_t)getdents;
		return;
	}
	if (minvers[0] == '.')
		minvc = minvers[1];
	else
		minvc = minvers[0];
	if ((majvers[0] == '6' && minvc >= '2') || majvers[0] > '6')
		gdents = (gdents_t)ngetdents;
	else
		gdents = (gdents_t)getdents;
}
#endif
#define	CALL_GDENTS	(*gdents)
#else
#define	CALL_GDENTS	ngetdents
#endif

int
readdir_r(DIR *dirp, dirent_t *entry, dirent_t **res)
{
	register dirent_t	*dp;	/* -> directory data */
	int saveloc = 0;
	int eof = 0;
	LOCKDECLINIT(l, LOCKDIR);

#if defined(_LIBC_RUN_ON_53) && _MIPS_SIM == _MIPS_SIM_ABI32
	if (gdents == NULL)
		gdents_init();
#endif
#if _MIPS_SIM == _MIPS_SIM_ABI32
	if (dirp->__dd_flags & _DIR_FLAGS_READDIR64) {
		UNLOCKDIR(l);
		return EINVAL;
	}
	if (!(dirp->__dd_flags & _DIR_FLAGS_MODE))
		dirp->__dd_flags |= _DIR_FLAGS_READDIR32;
#endif
	if (dirp->dd_size > 0) {
		dp = (dirent_t *)&dirp->dd_buf[dirp->dd_loc];
		saveloc = dirp->dd_loc;   /* save for possible EOF */
		dirp->dd_loc += dp->d_reclen;
	}
	if (dirp->dd_loc >= dirp->dd_size) {
		if (dirp->__dd_flags & _DIR_FLAGS_SEEN_EOF) {
			dirp->dd_loc = saveloc;	/* EOF so save for telldir */
			*res = NULL;
			UNLOCKDIR(l);
			return 0;
		}
		dirp->dd_loc = dirp->dd_size = 0;
	}
	if (dirp->dd_size == 0) {	/* refill buffer */
		dirp->dd_size =
			CALL_GDENTS(dirp->dd_fd, (dirent_t *)dirp->dd_buf,
				    __readdirsize, &eof);
		if (dirp->dd_size == 0) {	/* This means EOF */
			dirp->dd_loc = saveloc;  /* EOF so save for telldir */
			*res = NULL;
			UNLOCKDIR(l);
			return 0;
		}
		if (dirp->dd_size < 0) {	/* error */
			UNLOCKDIR(l);
			return errno;
		}
		if (eof)
			dirp->__dd_flags |= _DIR_FLAGS_SEEN_EOF;
		else
			dirp->__dd_flags &= ~_DIR_FLAGS_SEEN_EOF;
	}

	dp = (dirent_t *)&dirp->dd_buf[dirp->dd_loc];
	*entry = *dp;
	strcpy(entry->d_name, dp->d_name);
	*res = entry;
	UNLOCKDIR(l);
	return(0);
}

dirent_t *
readdir(DIR *dirp)
{
	register dirent_t	*dp;	/* -> directory data */
	int saveloc = 0;
	int eof = 0;
	LOCKDECLINIT(l, LOCKDIR);

#if defined(_LIBC_RUN_ON_53) && _MIPS_SIM == _MIPS_SIM_ABI32
	if (gdents == NULL)
		gdents_init();
#endif
#if _MIPS_SIM == _MIPS_SIM_ABI32
	if (dirp->__dd_flags & _DIR_FLAGS_READDIR64) {
		UNLOCKDIR(l);
		return NULL;
	}
	if (!(dirp->__dd_flags & _DIR_FLAGS_MODE))
		dirp->__dd_flags |= _DIR_FLAGS_READDIR32;
#endif
	if (dirp->dd_size > 0) {
		dp = (dirent_t *)&dirp->dd_buf[dirp->dd_loc];
		saveloc = dirp->dd_loc;   /* save for possible EOF */
		dirp->dd_loc += dp->d_reclen;
	}
	if (dirp->dd_loc >= dirp->dd_size) {
		if (dirp->__dd_flags & _DIR_FLAGS_SEEN_EOF) {
			dirp->dd_loc = saveloc;	/* EOF so save for telldir */
			UNLOCKDIR(l);
			return NULL;
		}
		dirp->dd_loc = dirp->dd_size = 0;
	}
	if (dirp->dd_size == 0) { 	/* refill buffer */
		dirp->dd_size =
			CALL_GDENTS(dirp->dd_fd, (dirent_t *)dirp->dd_buf,
				    __readdirsize, &eof);
		if (dirp->dd_size == 0)	{	/* This means EOF */
			dirp->dd_loc = saveloc;  /* EOF so save for telldir */
			UNLOCKDIR(l);
			return NULL;
		}
		if (dirp->dd_size < 0) {	/* error */
			UNLOCKDIR(l);
			return NULL;
		}
		if (eof)
			dirp->__dd_flags |= _DIR_FLAGS_SEEN_EOF;
		else
			dirp->__dd_flags &= ~_DIR_FLAGS_SEEN_EOF;
	}

	dp = (dirent_t *)&dirp->dd_buf[dirp->dd_loc];
	UNLOCKDIR(l);
	return(dp);
}
