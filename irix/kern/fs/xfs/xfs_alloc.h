#ifndef _FS_XFS_ALLOC_H
#define	_FS_XFS_ALLOC_H

#ident	"$Revision: 1.43 $"

struct buf;
struct xfs_mount;
struct xfs_perag;
struct xfs_trans;

/*
 * Freespace allocation types.  Argument to xfs_alloc_[v]extent.
 */
typedef enum xfs_alloctype
{
	XFS_ALLOCTYPE_ANY_AG,		/* allocate anywhere, use rotor */
	XFS_ALLOCTYPE_FIRST_AG,		/* ... start at ag 0 */
	XFS_ALLOCTYPE_START_AG,		/* anywhere, start in this a.g. */
	XFS_ALLOCTYPE_THIS_AG,		/* anywhere in this a.g. */
	XFS_ALLOCTYPE_START_BNO,	/* near this block else anywhere */
	XFS_ALLOCTYPE_NEAR_BNO,		/* in this a.g. and near this block */
	XFS_ALLOCTYPE_THIS_BNO		/* at exactly this block */
} xfs_alloctype_t;

/*
 * Flags for xfs_alloc_fix_freelist.
 */
#define	XFS_ALLOC_FLAG_TRYLOCK	0x00000001  /* use trylock for buffer locking */

/*
 * Argument structure for xfs_alloc routines.
 * This is turned into a structure to avoid having 20 arguments passed
 * down several levels of the stack.
 */
typedef struct xfs_alloc_arg {
	struct xfs_trans *tp;		/* transaction pointer */
	struct xfs_mount *mp;		/* file system mount point */
	struct buf	*agbp;		/* buffer for a.g. freelist header */
	struct xfs_perag *pag;		/* per-ag struct for this agno */
	xfs_fsblock_t	fsbno;		/* file system block number */
	xfs_agnumber_t	agno;		/* allocation group number */
	xfs_agblock_t	agbno;		/* allocation group-relative block # */
	xfs_extlen_t	minlen;		/* minimum size of extent */
	xfs_extlen_t	maxlen;		/* maximum size of extent */
	xfs_extlen_t	mod;		/* mod value for extent size */
	xfs_extlen_t	prod;		/* prod value for extent size */
	xfs_extlen_t	minleft;	/* min blocks must be left after us */
	xfs_extlen_t	total;		/* total blocks needed in xaction */
	xfs_extlen_t	alignment;	/* align answer to multiple of this */
	xfs_extlen_t	minalignslop;	/* slop for minlen+alignment calcs */
	xfs_extlen_t	len;		/* output: actual size of extent */
	xfs_alloctype_t	type;		/* allocation type XFS_ALLOCTYPE_... */
	xfs_alloctype_t	otype;		/* original allocation type */
	char		wasdel;		/* set if allocation was prev delayed */
	char		wasfromfl;	/* set if allocation is from freelist */
	char		isfl;		/* set if is freelist blocks - !actg */
	char		userdata;	/* set if this is user data */
} xfs_alloc_arg_t;

/*
 * Types for alloc tracing.
 */
#define	XFS_ALLOC_KTRACE_ALLOC	1
#define	XFS_ALLOC_KTRACE_FREE	2
#define	XFS_ALLOC_KTRACE_MODAGF	3
/*
 * Allocation tracing buffer size.
 */
#define	XFS_ALLOC_TRACE_SIZE	4096

#ifdef	XFS_ALL_TRACE
#define	XFS_ALLOC_TRACE
#endif

#if !defined(DEBUG) || defined(SIM)
#undef	XFS_ALLOC_TRACE
#endif

/*
 * Prototypes for visible xfs_alloc.c routines
 */

/*
 * Compute and fill in value of m_ag_maxlevels.
 */
void
xfs_alloc_compute_maxlevels(
	struct xfs_mount	*mp);	/* file system mount structure */

/*
 * Decide whether to use this allocation group for this allocation.
 * If so, fix up the btree freelist's size.
 * This is external so mkfs can call it, too.
 */
int				/* error */
xfs_alloc_fix_freelist(
	xfs_alloc_arg_t	*args,	/* allocation argument structure */
	int		flags);	/* XFS_ALLOC_FLAG_... */

/*
 * Get a block from the freelist.
 * Returns with the buffer for the block gotten.
 */
int				/* error */
xfs_alloc_get_freelist(
	struct xfs_trans *tp,	/* transaction pointer */
	struct buf	*agbp,	/* buffer containing the agf structure */
	xfs_agblock_t	*bnop);	/* block address retrieved from freelist */

/*
 * Log the given fields from the agf structure.
 */
void
xfs_alloc_log_agf(
	struct xfs_trans *tp,	/* transaction pointer */
	struct buf	*bp,	/* buffer for a.g. freelist header */
	int		fields);/* mask of fields to be logged (XFS_AGF_...) */

/*
 * Interface for inode allocation to force the pag data to be initialized.
 */
int				/* error */
xfs_alloc_pagf_init(
	struct xfs_mount *mp,	/* file system mount structure */
	struct xfs_trans *tp,	/* transaction pointer */
	xfs_agnumber_t	agno,	/* allocation group number */
	int		flags);	/* XFS_ALLOC_FLAGS_... */

/*
 * Put the block on the freelist for the allocation group.
 */
int				/* error */
xfs_alloc_put_freelist(
	struct xfs_trans *tp,	/* transaction pointer */
	struct buf	*agbp,	/* buffer for a.g. freelist header */
	struct buf	*agflbp,/* buffer for a.g. free block array */
	xfs_agblock_t	bno);	/* block being freed */

/*
 * Read in the allocation group header (free/alloc section).
 */
int					/* error  */
xfs_alloc_read_agf(
	struct xfs_mount *mp,		/* mount point structure */
	struct xfs_trans *tp,		/* transaction pointer */
	xfs_agnumber_t	agno,		/* allocation group number */
	int		flags,		/* XFS_ALLOC_FLAG_... */
	struct buf	**bpp);		/* buffer for the ag freelist header */

/*
 * Allocate an extent (variable-size).
 */
int				/* error */
xfs_alloc_vextent(
	xfs_alloc_arg_t	*args);	/* allocation argument structure */

/*
 * Free an extent.
 */
int				/* error */
xfs_free_extent(
	struct xfs_trans *tp,	/* transaction pointer */
	xfs_fsblock_t	bno,	/* starting block number of extent */
	xfs_extlen_t	len);	/* length of extent */
#endif	/* !SIM */

