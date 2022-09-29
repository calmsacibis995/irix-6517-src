#ident "$Revision: 1.3 $"

struct blkmap;

void
rtinit(xfs_mount_t		*mp);

int
generate_rtinfo(xfs_mount_t	*mp,
		xfs_rtword_t	*words,
		xfs_suminfo_t	*sumcompute);


#if 0
/*
 * bit map related macros
 */
#define setbit(a,i)     ((a)[(i)/XFS_NBWORD] |= 1<<((i)%XFS_NBWORD))
#define clrbit(a,i)     ((a)[(i)/XFS_NBWORD] &= ~(1<<((i)%XFS_NBWORD)))
#define isset(a,i)      ((a)[(i)/XFS_NBWORD] & (1<<((i)%XFS_NBWORD)))
#define isclr(a,i)      (((a)[(i)/XFS_NBWORD] & (1<<((i)%XFS_NBWORD))) == 0)

int
check_summary(xfs_mount_t	*mp);

void
process_rtbitmap(xfs_mount_t	*mp,
		xfs_dinode_t	*dino,
		struct blkmap	*blkmap);

void
process_rtsummary(xfs_mount_t	*mp,
		struct blkmap	*blkmap);
#endif
