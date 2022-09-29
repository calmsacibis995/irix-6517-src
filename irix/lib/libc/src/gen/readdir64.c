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

#ifdef __STDC__
	#pragma weak readdir64 = _readdir64
	#pragma weak readdir64_r = _readdir64_r
#endif
#include	"synonyms.h"
#include	"mplib.h"
#include	<sys/types.h>
#include	<dirent.h>
#include	<string.h>
#include	<errno.h>
#include	<stdlib.h>
#include	<sys/systeminfo.h>

extern int __readdirsize;
typedef int (*gdents_t)(int, dirent64_t *, int, int *);

#ifdef _LIBC_RUN_ON_53
static gdents_t gdents = (gdents_t)NULL;

static int
fake_ngetdents64(int fd, dirent64_t *buf, int bufsize, int *eofp)
{
	int sbsize, ngd;
	dirent_t *sbuf, *sp;
	dirent64_t *bp;

	sbsize = (bufsize * DIRENTSIZE(1)) / DIRENT64SIZE(1);
	sbuf = malloc(sbsize);
	if (sbuf == NULL) {
		setoserror(ENOMEM);
		return -1;
	}
	ngd = getdents(fd, sbuf, sbsize);
	if (ngd <= 0) {
		free(sbuf);
		return ngd;
	}
	for (sp = sbuf, bp = buf;
	     (char *)sp < (char *)sbuf + ngd;
	     sp = (dirent_t *)((char *)sp + sp->d_reclen),
	     bp = (dirent64_t *)((char *)bp + bp->d_reclen)) {
		bp->d_ino = sp->d_ino;
		bp->d_off = sp->d_off;
		bp->d_reclen = DIRENT64SIZE(strlen(sp->d_name));
		strcpy(bp->d_name, sp->d_name);
	}
	free(sbuf);
	return (char *)bp - (char *)buf;
}

static void
gdents_init(void)
{
	char majvers[5], minvers[5], minvc;

	if (sysinfo(_MIPS_SI_OSREL_MAJ, majvers, sizeof(majvers)) < 1 ||
	    sysinfo(_MIPS_SI_OSREL_MIN, minvers, sizeof(minvers)) < 1) {
		gdents = (gdents_t)fake_ngetdents64;
		return;
	}
	if (minvers[0] == '.')
		minvc = minvers[1];
	else
		minvc = minvers[0];
	if ((majvers[0] == '6' && minvc >= '2') || majvers[0] > '6')
		gdents = (gdents_t)ngetdents64;
	else
		gdents = (gdents_t)fake_ngetdents64;
}
#define	CALL_GDENTS	(*gdents)
#else
#define	CALL_GDENTS	ngetdents64
#endif

int
readdir64_r(DIR *dirp, dirent64_t *entry, dirent64_t **res)
{
	register dirent64_t	*dp;	/* -> directory data */
	int saveloc = 0;
	int eof = 0;
	LOCKDECLINIT(l, LOCKDIR);

#ifdef _LIBC_RUN_ON_53
	if (gdents == NULL)
		gdents_init();
#endif
	if (dirp->__dd_flags & _DIR_FLAGS_READDIR32) {
		UNLOCKDIR(l);
		return EINVAL;
	}
	if (!(dirp->__dd_flags & _DIR_FLAGS_MODE))
		dirp->__dd_flags |= _DIR_FLAGS_READDIR64;
	if (dirp->dd_size > 0) {
		dp = (dirent64_t *)&dirp->dd_buf[dirp->dd_loc];
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
			CALL_GDENTS(dirp->dd_fd, (dirent64_t *)dirp->dd_buf,
				    __readdirsize, &eof);
		if (dirp->dd_size == 0) {	/* This means EOF */
			dirp->dd_loc = saveloc;	/* EOF so save for telldir */
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

	dp = (dirent64_t *)&dirp->dd_buf[dirp->dd_loc];
	*entry = *dp;
	strcpy(entry->d_name, dp->d_name);
	*res = entry;
	UNLOCKDIR(l);
	return(0);
}

dirent64_t *
readdir64(DIR *dirp)
{
	register dirent64_t	*dp;	/* -> directory data */
	int saveloc = 0;
	int eof = 0;
	LOCKDECLINIT(l, LOCKDIR);

#ifdef _LIBC_RUN_ON_53
	if (gdents == NULL)
		gdents_init();
#endif
	if (dirp->__dd_flags & _DIR_FLAGS_READDIR32) {
		UNLOCKDIR(l);
		return NULL;
	}
	if (!(dirp->__dd_flags & _DIR_FLAGS_MODE))
		dirp->__dd_flags |= _DIR_FLAGS_READDIR64;
	if (dirp->dd_size > 0) {
		dp = (dirent64_t *)&dirp->dd_buf[dirp->dd_loc];
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
	if (dirp->dd_size == 0) {	/* refill buffer */
		dirp->dd_size =
			CALL_GDENTS(dirp->dd_fd, (dirent64_t *)dirp->dd_buf,
				    __readdirsize, &eof);
		if (dirp->dd_size == 0)	{	/* This means EOF */
			dirp->dd_loc = saveloc;	/* EOF so save for telldir */
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

	dp = (dirent64_t *)&dirp->dd_buf[dirp->dd_loc];
	UNLOCKDIR(l);
	return(dp);
}
