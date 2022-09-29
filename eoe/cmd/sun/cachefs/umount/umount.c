/* -----------------------------------------------------------------
 *
 *			umount.c
 *
 * CFS specific umount command.
 */

#pragma ident "@(#)umount.c   1.5     93/06/08 SMI"
/*
 *  (c) 1992  Sun Microsystems, Inc
 *	All rights reserved.
 */

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <mntent.h>
#include <cachefs/cachefs_fs.h>
#include "subr.h"

char *Progname;

/* forward references */
void usage(char *msgp);
struct mntent *dupmnttab(struct mntent *mntp);

/* -----------------------------------------------------------------
 *
 *			main
 *
 * Description:
 *	Main routine for the cfs umount program.
 * Arguments:
 *	argc	number of command line arguments
 *	argv	list of command line arguments
 * Returns:
 *	Returns 0 for success or 1 if an error occurs.
 * Preconditions:
 */

int
main(int argc, char **argv)
{
	char *specp = NULL;
	char *cachedir = NULL;
	char *strp;
	int xx;
	int ret;
	struct mntent mget;
	struct mntent mref;
	int mntpt = 1;
	FILE *finp;
	char *mnt_front = NULL;
	char *mnt_back = NULL;
	char *p, mnt_frontns[PATH_MAX];
	char *cacheid;
	struct stat sb;

	(void) setlocale(LC_ALL, "");

	Progname = *argv;

	/* must be root */
	if (getuid() != 0) {
		pr_err(gettext("must be run by root"));
		return (1);
	}

	/* make sure what to unmount is specified */
	switch ( argc ) {
		case 2:			/* invoked directly */
			if (strlen(argv[1]) >= (size_t)PATH_MAX) {
				pr_err(gettext("name too long"));
				return (1);
			}
			mnt_front = argv[1];

			/*
			 * An unmount from autofs may have a
			 * space appended to the path.
			 * Trim it off before attempting to
			 * match the mountpoint in mnttab
			 */
			strcpy(mnt_frontns, mnt_front);
			p = &mnt_frontns[strlen(mnt_frontns) - 1];
			if (*p == ' ')
				*p = '\0';

			/* get the mount point and the back file system mount point */
			finp = setmntent(MOUNTED, "r");
			if (finp) {

				bzero(&mref, sizeof(mref));
				mref.mnt_dir = mnt_frontns;
				mref.mnt_type = MNTTYPE_CACHEFS;
				ret = getmntany(finp, &mget, &mref);
				if (ret != -1) {
					specp = mget.mnt_fsname;
				} else {
					mref.mnt_fsname = mref.mnt_dir;
					mref.mnt_dir = NULL;
					rewind(finp);
					ret = getmntany(finp, &mget, &mref);
					if (ret != -1) {
						specp = mget.mnt_fsname;
						mnt_front = mget.mnt_dir;
					} else {
						pr_err(gettext("warning: %s not in mtab"),
						    mref.mnt_fsname);
					}
				}
				endmntent(finp);
			}

			/* try to unmount the file system */
			if (umount(mnt_front) == -1) {
				pr_err(gettext("%s %s"), mnt_front, strerror(errno));
				return (1);
			}

			/* remove the mnttab entry */
			delmntent(MOUNTED, &mget);

			/* if we do not know the name of the back file system mount point */
			if (!specp) {
				/* all done */
				return (0);
			}

			/*
			 * If the backpath mount option was used, do not unmount the
			 * back FS.
			 */
			if (hasmntopt(&mget, "backpath"))
				return (0);

			/*
			 * determine the back FS mount point
			 */
			cachedir = hasmntopt(&mget, "cachedir");
			if (!cachedir) {
				cachedir = CFS_DEF_DIR;
			} else if (!strtok(cachedir, "=") ||
				!(cachedir = strtok(NULL, ", "))) {
					pr_err(Progname, "Bad cachedir option in mtab entry");
					return(1);
			}
			cacheid = get_cacheid(specp, mget.mnt_dir);
			if (!cacheid) {
				pr_err(Progname, "Out of memory.");
				return(1);
			}
			xx = get_mount_point(cachedir, cacheid, &mnt_back, 0);
			switch (xx) {
				case 0:		/* not mounted */
					return(0);
				case -2:	/* mounted */
					break;
				default:	/* error */
					return(1);
			}
			/*
			 * If the mount poind does not exist, try using the old
			 * form of the mount point name.
			 */
			if (stat(mnt_back, &sb) && (errno == ENOENT)) {
				xx = get_mount_point(cachedir, make_filename(specp),
					&mnt_back, 0);
				switch (xx) {
					case 0:		/* not mounted */
						return(0);
					case -2:	/* mounted */
						break;
					default:	/* error */
						return(1);
				}
			}
			break;
		case 5:			/* invoked from umount */
		case 4:
			/*
			 * umount has already done the umount for the front file
			 * system.  Handle the back FS here.
			 * argv[1] is the fsname
			 * argv[2] is the mount point
			 * argv[4] is the options string
			 * Construct a reference mtab entry and use hasmntopt to find
			 * out whether or not the backpath option was used.
			 */
			mref.mnt_fsname = argv[1];
			mref.mnt_dir = argv[2];
			mref.mnt_type = NULL;
			mref.mnt_opts = argv[4];
			mref.mnt_freq = 0;
			mref.mnt_passno = 0;
			if (hasmntopt(&mref, "backpath") == NULL) {
				cachedir = hasmntopt(&mref, "cachedir");
				if (!cachedir) {
					cachedir = CFS_DEF_DIR;
				} else if (!strtok(cachedir, "=") ||
					!(cachedir = strtok(NULL, ", "))) {
						pr_err(Progname, "Bad cachedir option in mtab entry");
						return(1);
				}
				cacheid = get_cacheid(mref.mnt_fsname, mref.mnt_dir);
				if (!cacheid) {
					pr_err(Progname, "Out of memory.");
					return(1);
				}
				xx = get_mount_point(cachedir, cacheid, &mnt_back, 0);
				switch (xx) {
					case 0:		/* not mounted */
						return(0);
					case -2:	/* mounted */
						break;
					default:	/* error */
						return(1);
				}
				/*
				 * If the mount poind does not exist, try using the old
				 * form of the mount point name.
				 */
				if (stat(mnt_back, &sb) && (errno == ENOENT)) {
					xx = get_mount_point(cachedir,
						make_filename(mref.mnt_fsname), &mnt_back, 0);
					switch (xx) {
						case 0:		/* not mounted */
							return(0);
						case -2:	/* mounted */
							break;
						default:	/* error */
							return(1);
					}
				}
			} else {
				/*
				 * The backpath mount option was used, do not unmount the
				 * back FS.
				 */
				return(0);
			}
			break;
		default:
			usage("one argument must be specified");
			return(1);
	}

	/* invoke the umount command on the back file system */
	xx = execl("/sbin/umount", "/sbin/umount", mnt_back, NULL);
	pr_err(Progname,
		gettext("could not exec /sbin/umount on back file system %s"),
	    strerror(errno));
	return(1);
}

/* -----------------------------------------------------------------
 *
 *			usage
 *
 * Description:
 *	Prints a usage message.
 * Arguments:
 *	An optional additional message to be displayed.
 * Returns:
 * Preconditions:
 */

void
usage(char *msgp)
{
	if (msgp)
		pr_err(gettext("%s"), msgp);
	fprintf(stderr, gettext("Usage: umount -F cachefs dir"));
}

/*
 * this code stolen from the nfs unmount program...
 */
struct mntlist {
	struct mntent  *mntl_mnt;
	struct mntlist *mntl_next;
};

/* -----------------------------------------------------------------
 *
 *			dupmntent
 *
 * Description:
 *	Makes a copy of the specified mntent object and returns
 *	a pointer to it.
 * Arguments:
 *	mntp	mntent object to copy
 * Returns:
 *	Returns the pointer to the duplicatated object.
 * Preconditions:
 *	precond(mntp)
 */

struct mntent *
dupmntent(struct mntent *mntp)
{
	struct mntent *newp;

	newp = malloc(sizeof (*newp));
	if (newp == NULL)
		goto alloc_failed;
	(void) memset((char *)newp, 0, sizeof (*newp));
	newp->mnt_fsname = strdup(mntp->mnt_fsname);
	if (newp->mnt_fsname == NULL)
		goto alloc_failed;
	newp->mnt_dir = strdup(mntp->mnt_dir);
	if (newp->mnt_dir == NULL)
		goto alloc_failed;
	newp->mnt_type = strdup(mntp->mnt_type);
	if (newp->mnt_type == NULL)
		goto alloc_failed;
	newp->mnt_opts = strdup(mntp->mnt_opts);
	if (newp->mnt_opts == NULL)
		goto alloc_failed;
	newp->mnt_freq = mntp->mnt_freq;
	newp->mnt_passno = mntp->mnt_passno;

	return (newp);

alloc_failed:
	pr_err(gettext("dupmntent: no mem\n"));
	return (NULL);
}

