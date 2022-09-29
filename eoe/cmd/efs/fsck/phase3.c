#ident "$Revision: 1.11 $"

#include "fsck.h"

void
Phase3(void)
{
	DINODE *dp;
	efs_ino_t savino;
	efs_ino_t sloino;
	efs_ino_t tmpino;
	int go_slo;

	idprintf("** Phase 3 - Check Connectivity\n");
	for (inum = EFS_ROOTINO; inum <= lastino; inum++) {
		/*
		 * At this point, all directories which can be
		 * reached from / have had their state changed
		 * to FSTATE.
		 */
		if (getstate() == DSTATE) {
			pfunc = findino;
			srchname = "..";
			savino = inum;
			go_slo = 0;
			sloino = inum;
			do {
				/* sloino goes half as fast the the
				 * fast inode search.  If the fast
				 * path ever catches up to sloino,
				 * or if the fast path loops around
				 * to itself, we're in a dir .. loop.
				 */
				if (go_slo) {
					go_slo = 0;

					tmpino = inum;
					inum = sloino;

					if ((dp = ginode()) == NULL)
						break;
					filsize = dp->di_size;
					ckinode(dp,DATA,0);

					sloino = foundino;
					inum = tmpino;
				} else
					go_slo = 1;

				orphan = inum;
				if ((dp = ginode()) == NULL)
					break;
				filsize = dp->di_size;
				foundino = 0;
				ckinode(dp,DATA,0);

				if ((inum = foundino) == 0)
					break;

				if (inum == savino || inum == sloino)
					goto unlink;	/* dir loop */

			} while (getstate() == DSTATE);

			inum = orphan;
			if ((linkup() == YES) && !lfproblem) {
				thisname = pathp = pathname;
				*pathp++ = '?';
				pfunc = pass2;
				descend(lfdir);
			}
			inum = savino;
			continue;

	unlink:
			inum = savino;
			if (dp = ginode()) {
				printf("Unreferenced directory loop: clearing inode %d\n", inum);
				clrinode(dp);
			}
		}
	}
}
