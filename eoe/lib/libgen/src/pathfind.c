/*
 * libgen/pathfind.c
 *
 *
 * Copyright 1991, Silicon Graphics, Inc.
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

#ident "$Revision: 1.7 $"


#ifdef __STDC__
	#pragma weak pathfind = _pathfind
#endif
#include "synonyms.h"

/*
	Search the specified path for a file with the specified
	mode and type.  Return a pointer to the path.  If the
	file isn't found, return NULL.
*/

#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "libgen.h"
#include <unistd.h>

/*
 * Mode bit definitions -- see mknod(2)
 * Names of flags duplicate those of test(1)
 */


/* File type: 0170000 */
#define FFLAG	S_IFREG		/* normal file - also type 0 */
#define BFLAG	S_IFBLK		/* block special */
#define CFLAG	S_IFCHR		/* character special */
#define DFLAG	S_IFDIR		/* directory */
#define PFLAG	S_IFIFO		/* fifo */

#define UFLAG	S_ISUID		/* setuid */
#define GFLAG	S_ISGID		/* setgid */
#define KFLAG	S_ISVTX		/* sticky bit */

/*
 * Perms: 0700 user, 070 group, 07 other
 * Note that pathfind uses access(2), so no need to hassle
 * with shifts and such
 */
#define RFLAG	04		/* read */
#define WFLAG	02		/* write */
#define XFLAG	01		/* execute */

static int fullck(char *, mode_t, int);

char *
pathfind(register const char *path, register const char *name,
	 const char *mode)	/* mode: any combination from OPTSTRING */
{
	static char cpath[PATH_MAX];
	register char *cp;
	mode_t imode;
	int nzflag;

	/* Build imode */
	imode = 0; nzflag = 0;
	if (mode == ((char *) 0))
		mode = "";
	for (cp = (char *)mode; *cp; cp++) {
		switch (*cp) {
		case 'r':
			imode |= RFLAG;
			break;
		case 'w':
			imode |= WFLAG;
			break;
		case 'x':
			imode |= XFLAG;
			break;
		case 'b':
			imode |= BFLAG;
			break;
		case 'c':
			imode |= CFLAG;
			break;
		case 'd':
			imode |= DFLAG;
			break;
		case 'f':
			imode |= FFLAG;
			break;
		case 'p':
			imode |= PFLAG;
			break;
		case 'u':
			imode |= UFLAG;
			break;
		case 'g':
			imode |= GFLAG;
			break;
		case 'k':
			imode |= KFLAG;
			break;
		case 's':
			nzflag = 1;
			break;
		default:
			return ( (char *)0 );
		}
	}
		
	if (name[0] == '/' || path == ((char *) 0) || *path == '\0')
		path = ":";
	while (*path) {
		for (cp = cpath; (const char *)cp < &cpath[PATH_MAX] && (*cp = *path); cp++) {
			path++;
			if (*cp == ':')
				break;
		}
		if((const char *)cp + strlen(name) + 2 >= &cpath[PATH_MAX])
			continue;
		if (cp != cpath)
			*cp++ = '/';
		*cp = '\0';
		(void)strcat (cp, name);
		if (access (cpath, (int)imode&07) == 0 &&
			fullck (cpath, imode, nzflag))
			return cpath;
	}

	return ((char *) 0);
}

static int
fullck (char *name, mode_t mode, int nzflag)
{
	struct stat sbuf;
	mode_t xor;

	if ((mode & 0177000) == 0 && nzflag == 0)	/* no special info wanted */
		return (1);
	if (stat (name, &sbuf) == -1)
		return(0);
	xor = (sbuf.st_mode ^ mode) & 077000;	/* see mknod(2) */
	if ((mode & 0170000) == 0)
		xor &= ~070000;
	if ((mode & 07000) == 0)
		xor &= ~07000;
	if (xor)
		return (0);
	if (nzflag && sbuf.st_size == 0)
		return (0);
	return 1;
}

