#ident "$Revision: 1.1 $"

/*
 * Block mapping code taken from xfs_db.
 */

/*
 * Block map entry.
 */
typedef struct blkent {
	xfs_dfiloff_t	startoff;
	xfs_dfilblks_t	nblks;
	xfs_dfsbno_t	blks[1];
} blkent_t;
#define	BLKENT_SIZE(n)	\
	(offsetof(blkent_t, blks) + (sizeof(xfs_dfsbno_t) * (n)))

/*
 * Block map.
 */
typedef	struct blkmap {
	int		naents;
	int		nents;
	blkent_t	*ents[1];
} blkmap_t;
#define	BLKMAP_SIZE(n)	\
	(offsetof(blkmap_t, ents) + (sizeof(blkent_t *) * (n)))

/*
 * Extent descriptor.
 */
typedef struct bmap_ext {
	xfs_dfiloff_t	startoff;
	xfs_dfsbno_t	startblock;
	xfs_dfilblks_t	blockcount;
	int		flag;
} bmap_ext_t;

void		blkent_append(blkent_t **entp, xfs_dfsbno_t b,
			      xfs_dfilblks_t c);
blkent_t	*blkent_new(xfs_dfiloff_t o, xfs_dfsbno_t b, xfs_dfilblks_t c);
void		blkent_prepend(blkent_t **entp, xfs_dfsbno_t b,
			       xfs_dfilblks_t c);
blkmap_t	*blkmap_alloc(xfs_extnum_t);
void		blkmap_free(blkmap_t *blkmap);
xfs_dfsbno_t	blkmap_get(blkmap_t *blkmap, xfs_dfiloff_t o);
int		blkmap_getn(blkmap_t *blkmap, xfs_dfiloff_t o,
			    xfs_dfilblks_t nb, bmap_ext_t **bmpp);
void		blkmap_grow(blkmap_t **blkmapp, blkent_t **entp,
			    blkent_t *newent);
xfs_dfiloff_t	blkmap_last_off(blkmap_t *blkmap);
xfs_dfiloff_t	blkmap_next_off(blkmap_t *blkmap, xfs_dfiloff_t o, int *t);
void		blkmap_set_blk(blkmap_t **blkmapp, xfs_dfiloff_t o,
			       xfs_dfsbno_t b);
void		blkmap_set_ext(blkmap_t **blkmapp, xfs_dfiloff_t o,
			       xfs_dfsbno_t b, xfs_dfilblks_t c);
void		blkmap_shrink(blkmap_t *blkmap, blkent_t **entp);
