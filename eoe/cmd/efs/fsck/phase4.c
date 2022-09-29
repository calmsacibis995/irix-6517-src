#ident "$Revision: 1.12 $"

#include "fsck.h"

void
Phase4(void)
{
	int usable_inodes;

	idprintf("** Phase 4 - Check Reference Counts\n");
	for(inum = EFS_ROOTINO; inum <= lastino; inum++) {
		switch(getstate()) {
		case FSTATE:
			phase4fstate();
			break;
		case DSTATE:
			if (!lfproblem)
				clri("UNREF",YES);
			break;
		case CLEAR:
			clri("BAD/DUP",YES);
			break;
#ifdef AFS
		case VSTATE:
			nViceFiles++;
			break;
#endif
		}
	}

	if (lostroot)
		return;

	/* lost+found may have had some entries placed in it whose inodes
	 * got zapped in later passes: rescan it. */

	if (lfdir)
	{
		bzero(pathname, MAXPATH);
		inum = lfdir;
		sprintf(pathname, "/%s", lfname);
		thisname = lfname;
		pathp = pathname + strlen(pathname);
		pfunc = pass2;
		descend(EFS_ROOTINO);
	}

	/* daveh note 8/11/89: fix of a longstanding bug. max_inodes is
	 * actually the highest numerical value of a valid inode. Inodes
	 * start at 0, and inodes 0 & 1 are never used. Thus, the actual
	 * number of usable inodes is (max_inodes - 1).
	 */

	usable_inodes = inode_blocks * EFS_INOPBB - 2;

	if(usable_inodes - n_files != superblk.fs_tinode) {
		if (!gflag)
			idprintf("FREE INODE COUNT WRONG IN SUPERBLK");
		if (gflag)
		{
			superblk.fs_tinode = usable_inodes - (__int32_t)n_files;
			sbdirty();

		}
		else if (qflag) {
			superblk.fs_tinode = usable_inodes - (__int32_t)n_files;
			sbdirty();
			newline();
			idprintf("FIXED\n");
		}
		else if(reply("FIX") == YES) {
			superblk.fs_tinode = usable_inodes - (__int32_t)n_files;
			sbdirty();
		}
	}
	flush(&fileblk);
}

void
phase4fstate(void)
{
	DINODE *dp;
	efs_ino_t *blp;
	int n;

	if(n = getlncnt())
		adjust((short)n);
	else {
		for(blp = badlncnt; blp < &badlncnt[badln]; blp++)
			if((*blp == inum) && !lfproblem){
				dp = ginode();
				if (dp && dp->di_size) {
					if((n = linkup()) == NO)
					   clri("UNREF",NO);
					if (n == REM)
					   clri("UNREF",REM);
				}
				else {
					clri("UNREF",YES);
				}
				break;
			}
	}
}
