#ifndef _XR_DINODE_H
#define _XR_DINODE_H

#ident "$Revision: 1.20 $"

struct blkmap;

#if 0
typedef unsigned char	bmap_bitmap_t;

#define XR_BM_SIZE_BIT	8		/* sizeof(bmap_bitmap) in bits */
#define XR_BM_SHIFT	3		/* log2 of sizeof(bmap_bitmap) in bits */
#define XR_BM_MASK	0x1

#define get_bmbmap_rec(bm, bno) \
			((bm) + ((bno) >> XR_BM_SHIFT))
#define get_bmbmap_state(bm, bno) \
			((((bm) + ((bno) >> XR_BM_SHIFT)) >> \
			 ((bno) % XR_BM_SIZE)) & XR_BM_MASK)

#define is_bmbmap_clear(bm, bno) \
	(get_bmbmap_state((bm), (bno)) == 0)
#define is_bmbmap_set(bm, bno) \
	(get_bmbmap_state((bm), (bno)) == 1)

#define set_bmbmap(bm, bno) \
		*((bm) + ((bno) >> XR_BM_SHIFT)) |= 1 << ((bno) % XR_BM_SIZE_BIT)

#define clear_bmbmap(bm, bno) \
		*((bm) + ((bno) >> XR_BM_SHIFT)) &= ~(1 << ((bno) % XR_BM_SIZE_BIT))
#endif

int
verify_agbno(xfs_mount_t	*mp,
		xfs_agnumber_t	agno,
		xfs_agblock_t	agbno);

int
verify_dfsbno(xfs_mount_t	*mp,
		xfs_dfsbno_t	fsbno);

void
convert_extent(
	xfs_bmbt_rec_32_t	*rp,
	xfs_dfiloff_t		*op,	/* starting offset (blockno in file) */
	xfs_dfsbno_t		*sp,	/* starting block (fs blockno) */
	xfs_dfilblks_t		*cp,	/* blockcount */
	int			*fp);	/* extent flag */

int	
process_bmbt_reclist(xfs_mount_t	*mp,
		xfs_bmbt_rec_32_t	*rp,
		int			numrecs,
		int			type,
		xfs_ino_t		ino,
		xfs_drfsbno_t		*tot,
		struct blkmap		**blkmapp,
		__uint64_t		*first_key,
		__uint64_t		*last_key,
		int			whichfork);

int
scan_bmbt_reclist(
	xfs_mount_t		*mp,
	xfs_bmbt_rec_32_t	*rp,
	int			numrecs,
	int			type,
	xfs_ino_t		ino,
	xfs_drfsbno_t		*tot,
	int			whichfork);

int
verify_inode_chunk(xfs_mount_t		*mp,
			xfs_ino_t	ino,
			xfs_ino_t	*start_ino);

int	verify_aginode_chunk(xfs_mount_t	*mp,
				xfs_agnumber_t	agno,
				xfs_agino_t	agino,
				xfs_agino_t	*agino_start);

int
clear_dinode(xfs_mount_t *mp, xfs_dinode_t *dino, xfs_ino_t ino_num);

void
update_rootino(xfs_mount_t *mp);

int
process_dinode(xfs_mount_t *mp,
		xfs_dinode_t *dino,
		xfs_agnumber_t agno,
		xfs_agino_t ino,
		int was_free,
		int *dirty,
		int *tossit,
		int *used,
		int check_dirs,
		int check_dups,
		int extra_attr_check,
		int *isa_dir,
		xfs_ino_t *parent);

int
verify_dinode(xfs_mount_t *mp,
		xfs_dinode_t *dino,
		xfs_agnumber_t agno,
		xfs_agino_t ino);

int
verify_uncertain_dinode(xfs_mount_t *mp,
		xfs_dinode_t *dino,
		xfs_agnumber_t agno,
		xfs_agino_t ino);

int
verify_inum(xfs_mount_t		*mp,
		xfs_ino_t	ino);

int
verify_aginum(xfs_mount_t	*mp,
		xfs_agnumber_t	agno,
		xfs_agino_t	agino);

int
process_uncertain_aginodes(xfs_mount_t		*mp,
				xfs_agnumber_t	agno);
void
process_aginodes(xfs_mount_t	*mp,
		xfs_agnumber_t	agno,
		int		check_dirs,
		int		check_dups,
		int		extra_attr_check);

void
check_uncertain_aginodes(xfs_mount_t	*mp,
			xfs_agnumber_t	agno);

buf_t *
get_agino_buf(xfs_mount_t	*mp,
		xfs_agnumber_t	agno,
		xfs_agino_t	agino,
		xfs_dinode_t	**dipp);

xfs_dfsbno_t
get_bmapi(xfs_mount_t		*mp,
		xfs_dinode_t	*dip,
		xfs_ino_t	ino_num,
		xfs_dfiloff_t	bno,
	        int             whichfork );
#if 0
int
open_bmapi(
	xfs_mount_t		*mp,
	xfs_fsblock_t		root,
	xfs_ino_t		ino,
	xfs_dfiloff_t		size,
	bmap_bitmap_t		**used,
	bmap_bitmap_t		**referenced);

void
close_bmapi(bmap_bitmap_t	**used,
	bmap_bitmap_t		**referenced);

#endif

#endif /* _XR_DINODE_H */
