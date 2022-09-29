#ident "$Revision: 1.5 $"

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 *  static char sccsid[] = "@(#)dumpdir.c	1.1 (SGI) 8/1/90";	  *
 **************************************************************************/

#include "dump.h"

/*
 * Convert EFS directories to Sun/BSD format.
 * Problem here is that EFS directories are smaller. The smallest direntry 
 * for EFS is 8 bytes and for BSD is 12 bytes. So the maximum expansion 
 * possible is 50%. We throw in an additional 4096 for safety sake.
 * We read the directory, one block at a time, translating
 * it into bsd format and storing it into dirbuf. Once the entire directory
 * is translated, we reflect any change in the di_size, and return
 * dirbuf to caller to be dumped.
 */
static char	*dirbuf = NULL;
static char	*cend, *cptr;	/* end marker, current block marker */
static struct direct	*cdp;	/* current entry */
static int	csize;		/* # blocks converted */

static int	convertdir(void *);

char *
bsd_dumpdir(struct efs_dinode *ip)
{
	int allocsize;

	if (ip->di_size & EFS_DIRBMASK) {
		msg("directory ino %d: size %d is not a multiple of dir block size %d\n",
			ino, ip->di_size, EFS_DIRBSIZE);
		return NULL;
	}
	allocsize = (int)BBTOB(BTOBB(ip->di_size + (ip->di_size >> 1) + 4096));
	if ((dirbuf = calloc(allocsize, 1)) == NULL) {
		perror("  DUMP: no memory to convert efs directories");
		dumpabort();
	}
	cptr = dirbuf;
	cend = dirbuf + allocsize;
	cdp = (struct direct *)cptr;
	csize = 1;
	if (walkextents(ip, walkdir, convertdir)) {
		free(dirbuf);
		return NULL;
	}

	/*
	 * Adjust the d_reclen pointer in the last directory block and in one
	 * additional block which, if used for padding, won't be looked at,
	 * but we might as well be neat. The d_reclen pointer has to account 
	 * for all the space in a directory.
	 */
	cdp->d_reclen = (int)((cptr+BBSIZE) - (char *)cdp);
	if ((cptr + BBSIZE) < cend) {
		cptr += BBSIZE;
		cdp = (struct direct *)cptr;
		cdp->d_reclen = BBSIZE;
	}
	/*
	 * We are done reading the directory blocks into memory. The blocks
	 * are all in dirbuf. Reflect that change in di_size.
	 */
	ip->di_size = BBTOB(csize);
	return dirbuf;
}

/*
 * Convert the given EFS directory entry.
 */
static int
convertdir(void *arg)
{
	struct efs_dent *dep = arg;
	struct direct cvtbuf, *dp = &cvtbuf;
	int spaceleft;

	dp->d_ino = EFS_GET_INUM(dep);	/* can't be 0 */
	dp->d_namlen = dep->d_namelen;
	bcopy(dep->d_name, dp->d_name, dp->d_namlen);
	dp->d_name[dp->d_namlen] = '\0';
	dp->d_reclen = DIRSIZ(dp);

	/* Advance to next block if there isn't room in this one. */
	spaceleft = (int)((cptr+BBSIZE) - (char *)cdp) - cdp->d_reclen;
	if (spaceleft < dp->d_reclen) {
		cdp->d_reclen += spaceleft;
		cptr += BBSIZE;
		if (cptr >= cend) {
			msg("Cannot continue because directory is too large!!");
			return -1;
		}
		cdp = (struct direct *)cptr;
		csize++;
	} else
		cdp = (struct direct *) ((char *)cdp + cdp->d_reclen);

	/*
	 * Don't do record copy since d_reclen might be smaller than
	 * sizeof(struct direct)
	 */
	cdp->d_ino = dp->d_ino;
	cdp->d_namlen = dp->d_namlen;
	bcopy(dp->d_name, cdp->d_name, dp->d_namlen);
	cdp->d_name[cdp->d_namlen] = '\0';
	cdp->d_reclen = dp->d_reclen;
	return 0;
}
