#ifndef _FS_XFS_RTALLOC_H
#define	_FS_XFS_RTALLOC_H

#ident	"$Revision: 1.15 $"

struct xfs_mount;
struct xfs_trans;
struct xfs_growfs_rt;

/* Min and max rt extent sizes, specified in bytes */
#define	XFS_MAX_RTEXTSIZE	(1024 * 1024 * 1024)	/* 1GB */
#define	XFS_DFL_RTEXTSIZE	(64 * 1024)	        /* 64KB */
#define	XFS_MIN_RTEXTSIZE	(4 * 1024)		/* 4KB */

/*
 * Constants for bit manipulations.
 */
#define	XFS_NBBYLOG	3		/* log2(NBBY) */
#define	XFS_WORDLOG	2		/* log2(sizeof(xfs_rtword_t)) */
#define	XFS_NBWORDLOG	(XFS_NBBYLOG + XFS_WORDLOG)
#define	XFS_NBWORD	(1 << XFS_NBWORDLOG)
#define	XFS_WORDMASK	((1 << XFS_WORDLOG) - 1)

#define	XFS_BLOCKSIZE(mp)	((mp)->m_sb.sb_blocksize)
#define	XFS_BLOCKMASK(mp)	((mp)->m_blockmask)
#define	XFS_BLOCKWSIZE(mp)	((mp)->m_blockwsize)
#define	XFS_BLOCKWMASK(mp)	((mp)->m_blockwmask)

/*
 * Summary and bit manipulation macros.
 */
#define	XFS_SUMOFFS(mp,ls,bb)	((int)((ls) * (mp)->m_sb.sb_rbmblocks + (bb)))
#define	XFS_SUMOFFSTOBLOCK(mp,s)	\
	(((s) * (uint)sizeof(xfs_suminfo_t)) >> (mp)->m_sb.sb_blocklog)
#define	XFS_SUMPTR(mp,bp,so)	\
	((xfs_suminfo_t *)((char *)bp->b_un.b_addr + \
		(((so) * (uint)sizeof(xfs_suminfo_t)) & XFS_BLOCKMASK(mp))))

#define	XFS_BITTOBLOCK(mp,bi)	((bi) >> (mp)->m_blkbit_log)
#define	XFS_BLOCKTOBIT(mp,bb)	((bb) << (mp)->m_blkbit_log)
#define	XFS_BITTOWORD(mp,bi)	\
	((int)(((bi) >> XFS_NBWORDLOG) & XFS_BLOCKWMASK(mp)))

#define	XFS_RTMIN(a,b)	((a) < (b) ? (a) : (b))
#define	XFS_RTMAX(a,b)	((a) > (b) ? (a) : (b))

#define	XFS_RTLOBIT(w)	xfs_lowbit32(w)
#define	XFS_RTHIBIT(w)	xfs_highbit32(w)

#if XFS_BIG_FILESYSTEMS
#define	XFS_RTBLOCKLOG(b)	xfs_highbit64(b)
#else
#define	XFS_RTBLOCKLOG(b)	xfs_highbit32(b)
#endif

/*
 * Function prototypes for exported functions.
 */

#ifndef SIM
/*
 * Grow the realtime area of the filesystem.
 */
int
xfs_growfs_rt(
	struct xfs_mount	*mp,	/* mount point for filesystem */
	struct xfs_growfs_rt	*in);	/* growfs rt input struct */

/*
 * Allocate an extent in the realtime subvolume, with the usual allocation
 * parameters.  The length units are all in realtime extents, as is the
 * result block number.
 */
int					/* error */
xfs_rtallocate_extent(
	struct xfs_trans	*tp,	/* transaction pointer */
	xfs_rtblock_t		bno,	/* starting block number to allocate */
	xfs_extlen_t		minlen,	/* minimum length to allocate */
	xfs_extlen_t		maxlen,	/* maximum length to allocate */
	xfs_extlen_t		*len,	/* out: actual length allocated */
	xfs_alloctype_t		type,	/* allocation type XFS_ALLOCTYPE... */
	int			wasdel,	/* was a delayed allocation extent */
	xfs_extlen_t		prod,	/* extent product factor */
	xfs_rtblock_t		*rtblock); /* out: start block allocated */
#endif	/* !SIM */

/*
 * Free an extent in the realtime subvolume.  Length is expressed in
 * realtime extents, as is the block number.
 */
int					/* error */
xfs_rtfree_extent(
	struct xfs_trans	*tp,	/* transaction pointer */
	xfs_rtblock_t		bno,	/* starting block number to free */
	xfs_extlen_t		len);	/* length of extent freed */

/*
 * Initialize realtime fields in the mount structure.
 */
int					/* error */
xfs_rtmount_init(
	struct xfs_mount	*mp);	/* file system mount structure */

/*
 * Get the bitmap and summary inodes into the mount structure 
 * at mount time.
 */
int					/* error */
xfs_rtmount_inodes(
	struct xfs_mount	*mp);	/* file system mount structure */

#ifndef SIM
/*
 * Pick an extent for allocation at the start of a new realtime file.
 * Use the sequence number stored in the atime field of the bitmap inode.
 * Translate this to a fraction of the rtextents, and return the product
 * of rtextents and the fraction.
 * The fraction sequence is 0, 1/2, 1/4, 3/4, 1/8, ..., 7/8, 1/16, ...
 */
int					/* error */
xfs_rtpick_extent(
	struct xfs_mount	*mp,	/* file system mount point */
	struct xfs_trans	*tp,	/* transaction pointer */
	xfs_extlen_t		len,	/* allocation length (rtextents) */
	xfs_rtblock_t		*pick);	/* result rt extent */
#endif	/* !SIM */

#ifdef XFSDEBUG
/*
 * Debug code: print out the value of a range in the bitmap.
 */
void
xfs_rtprint_range(
	struct xfs_mount	*mp,	/* file system mount structure */
	struct xfs_trans	*tp,	/* transaction pointer */
	xfs_rtblock_t		start,	/* starting block to print */
	xfs_extlen_t		len);	/* length to print */

/*
 * Debug code: print the summary file.
 */
void
xfs_rtprint_summary(
	struct xfs_mount	*mp,	/* file system mount structure */
	struct xfs_trans	*tp);	/* transaction pointer */
#endif	/* XFSDEBUG */

#endif	/* !_FS_XFS_RTALLOC_H */
