#ident "$Revision: 1.9 $"

/*
 * read EFS inodes
 */
#include "libefs.h"

struct efs_dinode *
efs_iget(EFS_MOUNT *mp, efs_ino_t ino)
{
	static struct efs_dinode inos[BBSIZE];

	if (efs_readb(mp->m_fd, (char *)inos, EFS_ITOBB(mp->m_fs, ino), 1)!=1) {
		efs_errpr("efs_readb(inode %d) failed: %s\n",
							ino, strerror(errno));
		return 0;
	}
	return &inos[ino % EFS_INOPBB];
}

struct efs_dinode *
efs_igetcg(EFS_MOUNT *mp, int cg)
{
	struct efs_dinode *inos;

	if ((inos = (struct efs_dinode*)malloc(mp->m_fs->fs_cgisize * BBSIZE))
			== NULL) {
		efs_errpr("malloc failed reading inodes in cg %d\n", cg);
		return 0;
	}
	if (efs_readb(mp->m_fd, (char *)inos, EFS_CGIMIN(mp->m_fs, cg),
			mp->m_fs->fs_cgisize) != mp->m_fs->fs_cgisize) {
		efs_errpr("efs_readb(cg %d) failed: %s\n", cg, strerror(errno));
		return 0;
	}
	return inos;
}

/*
 * Store previously retrieved cylinder groups in a table for faster
 * retrieval time. Especially good for things which make multiple passes.
 */

struct efs_dinode *
efs_figet(EFS_MOUNT *mp, efs_ino_t inum)
{
	struct efs *fs = mp->m_fs;
	int cgno = EFS_ITOCG(fs, inum);

	if (mp->m_ilist == NULL)
		mp->m_ilist = (struct efs_dinode**)
				calloc(fs->fs_ncg, sizeof(struct efs_dinode*));
	if (mp->m_ilist == NULL) {
		efs_errpr("calloc failed in efs_figet()\n");
		return 0;
	}
	if (mp->m_ilist[cgno] == NULL)
		if ((mp->m_ilist[cgno] = efs_igetcg(mp, cgno)) == 0)
			return 0;

	return mp->m_ilist[cgno] + inum % (fs->fs_cgisize*EFS_INOPBB);
}

extent *
efs_getextents(EFS_MOUNT *mp, struct efs_dinode *di, efs_ino_t inum)
{
	struct extent *exp;
	struct extent *ex;
	int i;
	int amount;
	char *bp;
	
	if (di->di_numextents > EFS_DIRECTEXTENTS)
		amount = EFS_BBTOB(EFS_BTOBB(sizeof(extent) * di->di_numextents));
	else
		amount = sizeof(extent) * di->di_numextents;
	if ((ex = (extent *)malloc(amount)) == NULL) {
		efs_errpr("malloc failed reading in extents, inum=%d\n", inum);
		return 0;
	}
	if (di->di_numextents <= EFS_DIRECTEXTENTS) {
		memcpy(ex, di->di_u.di_extents,
			   di->di_numextents * sizeof(extent));
		return (ex);
	}
	/*
	 * Read in indirect extents
	 */
	exp = &di->di_u.di_extents[0];
	i = di->di_u.di_extents[0].ex_offset;
	for (bp = (char *)ex; i--; bp += BBTOB(exp->ex_length), exp++) {
		int newsize = bp - (char *)ex + BBTOB(exp->ex_length);

		if (newsize > amount) {
			ex = realloc(ex, newsize);
			if (!ex) {
				efs_errpr(
				 "malloc failed reading in extents, inum=%d\n",
				 inum);
				return 0;
			}
			bp = (char *)ex + newsize - BBTOB(exp->ex_length);
			amount = newsize;
		}

		if(efs_readb(mp->m_fd, bp, exp->ex_bn, exp->ex_length) !=
				exp->ex_length) {
			efs_errpr("failed to read indir extents, inum=%d\n",
				inum);
			return 0;
		}
	}
	return (ex);
}
