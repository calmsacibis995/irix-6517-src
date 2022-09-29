#ident "$Revision: 1.6 $"

struct	bbmap;
struct	xfs_bmbt_rec_32;

typedef struct bmap_ext {
	xfs_dfiloff_t	startoff;
	xfs_dfsbno_t	startblock;
	xfs_dfilblks_t	blockcount;
	int		flag;
} bmap_ext_t;

#if VERS < V_62
#define	XFS_DATA_FORK	0
#endif

extern void	bmap(xfs_dfiloff_t offset, xfs_dfilblks_t len, int whichfork,
		     int *nexp, bmap_ext_t *bep);
extern void	bmap_init(void);
extern void	convert_extent(struct xfs_bmbt_rec_32 *rp, xfs_dfiloff_t *op,
			       xfs_dfsbno_t *sp, xfs_dfilblks_t *cp, int *fp);
extern void	make_bbmap(struct bbmap *bbmap, int nex, bmap_ext_t *bmp);
