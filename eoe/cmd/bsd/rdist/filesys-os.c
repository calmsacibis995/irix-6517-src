/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char RCSid[] = 
"$Id: filesys-os.c,v 1.2 1994/07/22 17:01:18 jes Exp $";

static char sccsid[] = "@(#)filesys-os.c";

static char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

/*
 * OS specific file system routines
 */

#include "defs.h"
#include "filesys.h"

#if 	FSI_TYPE == FSI_GETFSSTAT
static struct statfs   *mnt = NULL;
typedef u_long 		ulong;
#endif	/* FSI_GETFSSTAT */

#if	FSI_TYPE == FSI_MNTCTL
static struct vmount   *mnt = NULL;
#endif	/* FSI_MNTCTL */

#if	(FSI_TYPE == FSI_MNTCTL) || (FSI_TYPE == FSI_GETFSSTAT)
static char 	       *mntbuf = NULL;
static int 		entries_left;
#endif	/* FSI_MNTCTL || FSI_GETFSSTAT */

#if	FSI_TYPE == FSI_MNTCTL
/*
 * AIX version of setmountent()
 */
FILE *setmountent(file, mode)
	/*ARGSUSED*/
	char *file;
	char *mode;
{
	ulong size;

	if (mntbuf)
		(void) free(mntbuf);

	mntctl(MCTL_QUERY, sizeof(size), &size);
	mntbuf = (char *) xmalloc(size);

	entries_left = mntctl(MCTL_QUERY, size, mntbuf);
	if (!entries_left)
		return((FILE *)NULL);

	mnt = (struct vmount *)mntbuf;
	return((FILE *) 1);
}
#endif	/* FSI_MNTCTL */

#if	FSI_TYPE == FSI_GETFSSTAT
/*
 * getfsstat() version of get mount info routines.
 */
FILE *setmountent(file, mode)
	/*ARGSUSED*/
	char *file;
	char *mode;
{
	ulong size;

	if (mntbuf)
		(void) free(mntbuf);

	size = getfsstat((struct statfs *) NULL, 0, MNT_WAIT);
	size *= sizeof(struct statfs);
	mntbuf = (char *) xmalloc(size);

	entries_left = getfsstat((struct statfs *)mntbuf, size, MNT_WAIT);
	if (entries_left == -1)
		return((FILE *) NULL);

	mnt = (struct statfs *) mntbuf;

	return((FILE *) 1);
}
#endif	/* FSI_GETFSSTAT */

#if	FSI_TYPE == FSI_MNTCTL
/* 
 * AIX version of getmountent() 
 */
/*
 * Iterate over mount entries
 */
mntent_t *getmountent(fptr)
	/*ARGSUSED*/
	FILE *fptr;
{
	static mntent_t mntstruct;

	if (!entries_left)
		return((mntent_t*)0);

	bzero((char *) &mntstruct, sizeof(mntstruct));

	if (mnt->vmt_flags & MNT_READONLY)
		mntstruct.me_flags |= MEFLAG_READONLY;

	mntstruct.me_path = vmt2dataptr(mnt, VMT_STUB);
	switch ((ulong)(struct vmount*)mnt->vmt_gfstype) {
	      case MNT_NFS:
		mntstruct.me_type = METYPE_NFS;
		break;
	      default:
		mntstruct.me_type = METYPE_OTHER;
		break;
	}

	mnt = (struct vmount*)((mnt->vmt_length)+(ulong)mnt);
	entries_left--;

	return(&mntstruct);
}
#endif	/* FSI_MNTCTL */

#if	FSI_TYPE == FSI_GETFSSTAT
/*
 * getfsstat() version of getmountent()
 */
mntent_t *getmountent(fptr)
	/*ARGSUSED*/
	FILE *fptr;
{
	static mntent_t mntstruct;
	static char remote_dev[MAXHOSTNAMELEN+MAXPATHLEN+1];

	if (!entries_left)
		return((mntent_t*)0);

	bzero((char *) &mntstruct, sizeof(mntstruct));

#if	defined(MNT_RDONLY)
	if (mnt->f_flags & MNT_RDONLY)
		mntstruct.me_flags |= MEFLAG_READONLY;
#endif
#if	defined(M_RDONLY)
	if (mnt->f_flags & M_RDONLY)
		mntstruct.me_flags |= MEFLAG_READONLY;
#endif

	switch ((ulong)(struct statfs*)mnt->f_type) {
	      case MOUNT_NFS:
		(void) sprintf(remote_dev, "%s", mnt->f_mntfromname);
		mntstruct.me_path = remote_dev;
		mntstruct.me_type = METYPE_NFS;
		break;
	      default:
		mntstruct.me_path = mnt->f_mntonname;
		mntstruct.me_type = METYPE_OTHER;
		break;
	}

	mnt = (struct statfs*)(sizeof(struct statfs)+(ulong)mnt);
	entries_left--;

	return(&mntstruct);
}
#endif

#if	(FSI_TYPE == FSI_MNTCTL) || (FSI_TYPE == FSI_GETFSSTAT)
/*
 * Done with iterations
 */
void endmountent(fptr)
	/*ARGSUSED*/
	FILE *fptr;
{
	mnt = NULL;

	if (mntbuf) {
		(void) free(mntbuf);
		mntbuf = (char *) NULL;
	}
}
#endif	/* FSI_MNTCTL || FSI_GETFSSTAT */

#if	FSI_TYPE == FSI_GETMNTENT2
/*
 * Prepare to iterate over mounted filesystem list
 */
FILE *setmountent(file, mode)
	/*ARGSUSED*/
	char *file;
	char *mode;
{
	return(fopen(file, mode));
}

/*
 * Done with iteration
 */
void endmountent(fptr)
	/*ARGSUSED*/
	FILE *fptr;
{
	fclose(fptr);
}

/*
 * Iterate over mount entries
 */
mntent_t *getmountent(fptr)
	FILE *fptr;
{
	static mntent_t me;
	static struct mnttab mntent;

	bzero((char *)&me, sizeof(mntent_t));

#if     defined(UNICOS)
        if (getmntent(fptr, &mntent) != NULL) {
#else
        if (getmntent(fptr, &mntent) != -1) {
#endif
		me.me_path = mntent.mnt_mountp;
		me.me_type = mntent.mnt_fstype;
		if (mntent.mnt_mntopts && hasmntopt(&mntent, MNTOPT_RO))
			me.me_flags |= MEFLAG_READONLY;

#if	defined(MNTTYPE_IGNORE)
		if (strcmp(mntent.mnt_fstype, MNTTYPE_IGNORE) == 0)
			me.me_flags |= MEFLAG_IGNORE;
#endif	/* MNTTYPE_IGNORE */
#if	defined(MNTTYPE_SWAP)
		if (strcmp(mntent.mnt_fstype, MNTTYPE_SWAP) == 0)
			me.me_flags |= MEFLAG_IGNORE;
#endif	/* MNTTYPE_SWAP */

		return(&me);
	} else
		return((mntent_t *) NULL);
}
#endif	/* FSI_GETMNTNET2 */

#if	FSI_TYPE == FSI_GETMNTENT
/*
 * Prepare to iterate over mounted filesystem list
 */
FILE *setmountent(file, mode)
	/*ARGSUSED*/
	char *file;
	char *mode;
{
	return(setmntent(file, mode));
}

/*
 * Done with iteration
 */
void endmountent(fptr)
	/*ARGSUSED*/
	FILE *fptr;
{
	endmntent(fptr);
}

/*
 * Iterate over mount entries
 */
mntent_t *getmountent(fptr)
	FILE *fptr;
{
	static mntent_t me;
	struct mntent *mntent;

	bzero((char *)&me, sizeof(mntent_t));

	if (mntent = getmntent(fptr)) {
		me.me_path = mntent->mnt_dir;
		me.me_type = mntent->mnt_type;
		if (mntent->mnt_opts && hasmntopt(mntent, MNTOPT_RO))
			me.me_flags |= MEFLAG_READONLY;

#if	defined(MNTTYPE_IGNORE)
		if (strcmp(mntent->mnt_type, MNTTYPE_IGNORE) == 0)
			me.me_flags |= MEFLAG_IGNORE;
#endif	/* MNTTYPE_IGNORE */
#if	defined(MNTTYPE_SWAP)
		if (strcmp(mntent->mnt_type, MNTTYPE_SWAP) == 0)
			me.me_flags |= MEFLAG_IGNORE;
#endif	/* MNTTYPE_SWAP */

		return(&me);
	} else
		return((mntent_t *) NULL);
}
#endif	/* FSI_GETMNTNET */

#if	FSI_TYPE == FSI_GETMNT
/*
 * getmnt() interface (Ultrix)
 */

#include <sys/fs_types.h>

static int startmounts = 0;

FILE *setmountent(file, mode)
	/*ARGSUSED*/
	char *file;
	char *mode;
{
	startmounts = 0;
}

void endmountent(fptr)
	/*ARGSUSED*/
	FILE *fptr;
{
	/* NOOP */
}

/*
 * Iterate over mounted filesystems using getmnt()
 */
mntent_t *getmountent(fptr)
	/*ARGSUSED*/
	FILE *fptr;
{
	struct fs_data fs_data;
	static mntent_t me;

	if (getmnt(&startmounts, &fs_data, sizeof(fs_data), NOSTAT_MANY, 
		   (char *) NULL) <= 0)
		return((mntent_t *) NULL);

	bzero((char *)&me, sizeof(mntent_t));
	me.me_path = fs_data.fd_path;
	if (fs_data.fd_fstype == GT_NFS)
		me.me_type = METYPE_NFS;
	else
		me.me_type = METYPE_OTHER;

	if (fs_data.fd_flags & M_RONLY)
		me.me_flags |= MEFLAG_READONLY;

	return(&me);
}
#endif	/* FSI_GETMNT */

/*
 * Make a new (copy) of a mntent structure.
 */
mntent_t *newmountent(old)
	mntent_t *old;
{
	mntent_t *new;

	if (!old)
		return((mntent_t *) NULL);

	new = (mntent_t *) xcalloc(1, sizeof(mntent_t));
	new->me_path = strdup(old->me_path);
	new->me_type = strdup(old->me_type);
	new->me_flags = old->me_flags;

	return(new);
}
