#ifndef _XR_SCAN_H
#define _XR_SCAN_H

#ident "$Revision: 1.7 $"

struct blkmap;

void scan_sbtree(
	xfs_agblock_t	root,
	int		nlevels,
	xfs_agnumber_t	agno,
	int		suspect,
	void		(*func)(xfs_btree_sblock_t	*block,
				int			level,
				xfs_agblock_t		bno,
				xfs_agnumber_t		agno,
				int			suspect,
				int			isroot),
	int		isroot);

int scan_lbtree(
	xfs_dfsbno_t	root,
	int		nlevels,
	int		(*func)(xfs_btree_lblock_t	*block,
				int			level,
				int			type,
				int			whichfork,
				xfs_dfsbno_t		bno,
				xfs_ino_t		ino,
				xfs_drfsbno_t		*tot,
				__uint64_t		*nex,
				struct blkmap		**blkmapp,
				bmap_cursor_t		*bm_cursor,
				int			isroot,
				int			check_dups,
				int			*dirty),
	int		type,
	int		whichfork,
	xfs_ino_t	ino,
	xfs_drfsbno_t	*tot,
	__uint64_t	*nex,
	struct blkmap	**blkmapp,
	bmap_cursor_t	*bm_cursor,
	int		isroot,
	int		check_dups);

int scanfunc_bmap(
	xfs_btree_lblock_t	*ablock,
	int			level,
	int			type,
	int			whichfork,
	xfs_dfsbno_t		bno,
	xfs_ino_t		ino,
	xfs_drfsbno_t		*tot,
	__uint64_t		*nex,
	struct blkmap		**blkmapp,
	bmap_cursor_t		*bm_cursor,
	int			isroot,
	int			check_dups,
	int			*dirty);

void scanfunc_bno(
	xfs_btree_sblock_t	*ablock,
	int			level,
	xfs_agblock_t		bno,
	xfs_agnumber_t		agno,
	int			suspect,
	int			isroot);

void scanfunc_cnt(
	xfs_btree_sblock_t	*ablock,
	int			level,
	xfs_agblock_t		bno,
	xfs_agnumber_t		agno,
	int			suspect,
	int			isroot);

void
scanfunc_ino(
	xfs_btree_sblock_t	*ablock,
	int			level,
	xfs_agblock_t		bno,
	xfs_agnumber_t		agno,
	int			suspect,
	int			isroot);

#endif /* _XR_SCAN_H */
