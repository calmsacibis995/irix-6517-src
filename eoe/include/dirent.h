#ifndef __DIRENT_H__
#define __DIRENT_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.35 $"
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

#include <standards.h>
#include <sys/types.h>
#include <sys/dirent.h>

/*
 * This is a POSIX/XPG header
 */
#if _SGIAPI
#define MAXNAMLEN	255		/* maximum filename length */
#define DIRBUF		4096		/* buffer size for fs-indep. dirs */
#endif /* _SGIAPI */

#if _SGIAPI && !defined(__SGI_NODIRENT_COMPAT)
#define	dd_fd		__dd_fd
#define	dd_loc		__dd_loc
#define	dd_size		__dd_size
#define	dd_buf		__dd_buf
#endif /* _SGIAPI && !__SGI_NODIRENT_COMPAT */

typedef struct {
	int		__dd_fd;	/* file descriptor */
	int		__dd_loc;	/* offset in block */
	int		__dd_size;	/* amount of valid data */
	char		*__dd_buf;	/* directory block */
	int		__dd_flags;	/* using readdir64 or not; eof */
} DIR;			/* stream data from opendir() */
/* values for __dd_flags */
#define	_DIR_FLAGS_READDIR32	1
#define	_DIR_FLAGS_READDIR64	2
#define	_DIR_FLAGS_MODE		(_DIR_FLAGS_READDIR32|_DIR_FLAGS_READDIR64)
#define	_DIR_FLAGS_SEEN_EOF	4

extern DIR		*opendir( const char * );
extern int		closedir( DIR * );
extern dirent_t		*readdir( DIR * );
extern void		rewinddir( DIR * );

#if _SGIAPI && (_MIPS_SIM == _MIPS_SIM_NABI32)
extern off_t		telldir( DIR * );
extern void		seekdir( DIR *, off_t );
#else
extern long		telldir( DIR * );
extern void		seekdir( DIR *, long );
#define rewinddir( dirp )	seekdir( dirp, 0L )
#endif

#if _SGIAPI
extern int 		scandir(const char *, dirent_t **[],
				int (*)(dirent_t *),
				int (*)(dirent_t **, dirent_t **));
extern int		alphasort(dirent_t **, dirent_t **);
extern int 		scandir64(const char *, dirent64_t **[],
				int (*)(dirent64_t *),
				int (*)(dirent64_t **, dirent64_t **));
extern int		alphasort64(dirent64_t **, dirent64_t **);
extern int		readdir64_r(DIR *, dirent64_t *, dirent64_t **);
extern off64_t		telldir64(DIR *);
extern void		seekdir64(DIR *, off64_t);
#endif /* _SGIAPI */

#if _LFAPI
extern dirent64_t *readdir64(DIR *);
#endif /* _LFAPI */

#if _POSIX1C
extern int readdir_r(DIR *, dirent_t *, dirent_t **);
#endif

#ifdef __cplusplus
}
#endif
#endif /* !__DIRENT_H__ */
