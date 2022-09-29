#ident "$Revision: 1.8 $"

#include "fsck.h"

void
Phase2(void)
{
	DINODE *dp;

	idprintf("** Phase 2 - Check Pathnames\n");
	inum = EFS_ROOTINO;
	thisname = pathp = pathname;
	pfunc = pass2;
	if ((dp = ginode()) == NULL)
		iderrexit("Can't read root inode. TERMINATING\n");

	switch (getstate()) {
		case USTATE:
			goto badroot;
		case FSTATE:
#ifdef AFS
		case VSTATE:
#endif
			idprintf("ROOT INODE NOT DIRECTORY");
			if (gflag)
				gerrexit();
			if (reply("FIX") == NO)
				goto badroot;
			dp->di_mode &= ~IFMT;
			dp->di_mode |= IFDIR;
			inodirty();
			setstate(DSTATE);
			/* FALLTHROUGH */
		case DSTATE:
			descend(EFS_ROOTINO);
			break;
		case CLEAR:

			/* This used to allow you to continue. But that was
			 * bogus: bad extent info in the root inode will
			 * cause the filesystem to be unusable (and will
			 * result in "directory corrupted" messages if you
			 * try!) */

			idprintf("DUPS/BAD IN ROOT INODE\n");
			if (gflag)
				gerrexit();
			goto badroot;
	}
	return;

badroot:
	idprintf("\n  Root inode is bad. The directory hierarchy cannot be saved.\n\n");
	if (!savedir)
	{
		printf("  Regular files may be saved. Rerun with -l flag to specify\n");
		printf("  a directory on another filesystem for saving files.\n");
		printf("  Make sure you have plenty of space!\n");
		errexit("\n");
	}
	else
	{
		printf("  Regular files will be saved in %s under their inode numbers\n",savedir);
		lostroot = 1;
	}
}

/*
 * chkempt() --
 * check whether a directory is "empty" .
 * i.e., has nothing but . and .. .
 * by calling extent-list checker or indirect-extent
 * checker as appropriate.
 */
int
chkempt(DINODE *dp)
{
	int ret;

	dir_size = dp->di_size;
	ret = ckinode(dp, DEMPT, 0);
	return ret == KEEPON ? YES : NO;
}
