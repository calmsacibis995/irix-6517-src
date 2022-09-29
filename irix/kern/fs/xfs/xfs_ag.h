#ifndef _FS_XFS_AG_H
#define	_FS_XFS_AG_H

#ident	"$Revision: 1.28 $"

/*
 * Allocation group header
 * This is divided into three structures, placed in sequential 512-byte 
 * buffers after a copy of the superblock (also in a 512-byte buffer).
 */

struct buf;
struct xfs_mount;

#define	XFS_AGF_MAGIC	0x58414746	/* 'XAGF' */
#define	XFS_AGI_MAGIC	0x58414749	/* 'XAGI' */
#define	XFS_AGF_VERSION	1
#define	XFS_AGI_VERSION	1
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_AGF_GOOD_VERSION)
int xfs_agf_good_version(unsigned v);
#define	XFS_AGF_GOOD_VERSION(v)	xfs_agf_good_version(v)
#else
#define XFS_AGF_GOOD_VERSION(v)		((v) == XFS_AGF_VERSION)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_AGI_GOOD_VERSION)
int xfs_agi_good_version(unsigned v);
#define	XFS_AGI_GOOD_VERSION(v)	xfs_agi_good_version(v)
#else
#define XFS_AGI_GOOD_VERSION(v)		((v) == XFS_AGI_VERSION)
#endif

/*
 * Btree number 0 is bno, 1 is cnt.  This value gives the size of the
 * arrays below.
 */
#define	XFS_BTNUM_AGF	((int)XFS_BTNUM_CNTi + 1)

/*
 * The second word of agf_levels in the first a.g. overlaps the EFS
 * superblock's magic number.  Since the magic numbers valid for EFS
 * are > 64k, our value cannot be confused for an EFS superblock's.
 */

typedef struct xfs_agf
{
	/*
	 * Common allocation group header information
	 */
	__uint32_t	agf_magicnum;	/* magic number == XFS_AGF_MAGIC */
	__uint32_t	agf_versionnum;	/* header version == XFS_AGF_VERSION */
	xfs_agnumber_t	agf_seqno;	/* sequence # starting from 0 */
	xfs_agblock_t	agf_length;	/* size in blocks of a.g. */
	/*
	 * Freespace information
	 */
	xfs_agblock_t	agf_roots[XFS_BTNUM_AGF];	/* root blocks */
	__uint32_t	agf_spare0;	/* spare field */
	__uint32_t	agf_levels[XFS_BTNUM_AGF];	/* btree levels */
	__uint32_t	agf_spare1;	/* spare field */
	__uint32_t	agf_flfirst;	/* first freelist block's index */
	__uint32_t	agf_fllast;	/* last freelist block's index */
	__uint32_t	agf_flcount;	/* count of blocks in freelist */
	xfs_extlen_t	agf_freeblks;	/* total free blocks */
	xfs_extlen_t	agf_longest;	/* longest free space */
} xfs_agf_t;

#define	XFS_AGF_MAGICNUM	0x00000001
#define	XFS_AGF_VERSIONNUM	0x00000002
#define	XFS_AGF_SEQNO		0x00000004
#define	XFS_AGF_LENGTH		0x00000008
#define	XFS_AGF_ROOTS		0x00000010
#define	XFS_AGF_LEVELS		0x00000020
#define	XFS_AGF_FLFIRST		0x00000040
#define	XFS_AGF_FLLAST		0x00000080
#define	XFS_AGF_FLCOUNT		0x00000100
#define	XFS_AGF_FREEBLKS	0x00000200
#define	XFS_AGF_LONGEST		0x00000400
#define	XFS_AGF_NUM_BITS	11
#define	XFS_AGF_ALL_BITS	((1 << XFS_AGF_NUM_BITS) - 1)

/* disk block (daddr_t) in the AG */
#define	XFS_AGF_DADDR		((daddr_t)1)
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_AGF_BLOCK)
xfs_agblock_t xfs_agf_block(struct xfs_mount *mp);
#define	XFS_AGF_BLOCK(mp)	xfs_agf_block(mp)
#else
#define	XFS_AGF_BLOCK(mp)	XFS_HDR_BLOCK(mp, XFS_AGF_DADDR)
#endif

/*
 * Size of the unlinked inode hash table in the agi.
 */
#define	XFS_AGI_UNLINKED_BUCKETS	64

typedef struct xfs_agi
{
	/*
	 * Common allocation group header information
	 */
	__uint32_t	agi_magicnum;	/* magic number == XFS_AGI_MAGIC */
	__uint32_t	agi_versionnum;	/* header version == XFS_AGI_VERSION */
	xfs_agnumber_t	agi_seqno;	/* sequence # starting from 0 */
	xfs_agblock_t	agi_length;	/* size in blocks of a.g. */
	/*
	 * Inode information
	 * Inodes are mapped by interpreting the inode number, so no
	 * mapping data is needed here.
	 */
	xfs_agino_t	agi_count;	/* count of allocated inodes */
	xfs_agblock_t	agi_root;	/* root of inode btree */
	__uint32_t	agi_level;	/* levels in inode btree */
	xfs_agino_t	agi_freecount;	/* number of free inodes */
	xfs_agino_t	agi_newino;	/* new inode just allocated */
	xfs_agino_t	agi_dirino;	/* last directory inode chunk */
	/*
	 * Hash table of inodes which have been unlinked but are
	 * still being referenced.
	 */
	xfs_agino_t	agi_unlinked[XFS_AGI_UNLINKED_BUCKETS];
} xfs_agi_t;

#define	XFS_AGI_MAGICNUM	0x00000001
#define	XFS_AGI_VERSIONNUM	0x00000002
#define	XFS_AGI_SEQNO		0x00000004
#define	XFS_AGI_LENGTH		0x00000008
#define	XFS_AGI_COUNT		0x00000010
#define	XFS_AGI_ROOT		0x00000020
#define	XFS_AGI_LEVEL		0x00000040
#define	XFS_AGI_FREECOUNT	0x00000080
#define	XFS_AGI_NEWINO		0x00000100
#define	XFS_AGI_DIRINO		0x00000200
#define	XFS_AGI_UNLINKED	0x00000400
#define	XFS_AGI_NUM_BITS	11
#define	XFS_AGI_ALL_BITS	((1 << XFS_AGI_NUM_BITS) - 1)

/* disk block (daddr_t) in the AG */
#define	XFS_AGI_DADDR		((daddr_t)2)
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_AGI_BLOCK)
xfs_agblock_t xfs_agi_block(struct xfs_mount *mp);
#define	XFS_AGI_BLOCK(mp)	xfs_agi_block(mp)
#else
#define	XFS_AGI_BLOCK(mp)	XFS_HDR_BLOCK(mp, XFS_AGI_DADDR)
#endif

/*
 * The third a.g. block contains the a.g. freelist, an array 
 * of block pointers to blocks owned by the allocation btree code.
 */
#define	XFS_AGFL_DADDR		((daddr_t)3)
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_AGFL_BLOCK)
xfs_agblock_t xfs_agfl_block(struct xfs_mount *mp);
#define	XFS_AGFL_BLOCK(mp)	xfs_agfl_block(mp)
#else
#define	XFS_AGFL_BLOCK(mp)	XFS_HDR_BLOCK(mp, XFS_AGFL_DADDR)
#endif
#define	XFS_AGFL_SIZE		(BBSIZE / sizeof(xfs_agblock_t))
typedef	struct xfs_agfl
{
	xfs_agblock_t	agfl_bno[XFS_AGFL_SIZE];
} xfs_agfl_t;

/*
 * Per-ag incore structure, copies of information in agf and agi,
 * to improve the performance of allocation group selection.
 */
typedef struct xfs_perag
{
	char		pagf_init;	/* this agf's entry is initialized */
	char		pagi_init;	/* this agi's entry is initialized */
	__uint8_t	pagf_levels[XFS_BTNUM_AGF];
					/* # of levels in bno & cnt btree */
	__uint32_t	pagf_flcount;	/* count of blocks in freelist */
	xfs_extlen_t	pagf_freeblks;	/* total free blocks */
	xfs_extlen_t	pagf_longest;	/* longest free space */
	xfs_agino_t	pagi_freecount;	/* number of free inodes */
} xfs_perag_t;

#define	XFS_AG_MIN_BYTES	(1LL << 24)	/* 16 MB */
#define	XFS_AG_BEST_BYTES	(1LL << 30)	/*  1 GB */
#define	XFS_AG_MAX_BYTES	(1LL << 32)	/*  4 GB */

#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_AG_MIN_BLOCKS)
xfs_extlen_t xfs_ag_min_blocks(int bl);
#define	XFS_AG_MIN_BLOCKS(bl)		xfs_ag_min_blocks(bl)
#else
#define	XFS_AG_MIN_BLOCKS(bl)	((xfs_extlen_t)(XFS_AG_MIN_BYTES >> bl))
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_AG_BEST_BLOCKS)
xfs_extlen_t xfs_ag_best_blocks(int bl);
#define	XFS_AG_BEST_BLOCKS(bl)		xfs_ag_best_blocks(bl)
#else
#define	XFS_AG_BEST_BLOCKS(bl)	((xfs_extlen_t)(XFS_AG_BEST_BYTES >> bl))
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_AG_MAX_BLOCKS)
xfs_extlen_t xfs_ag_max_blocks(int bl);
#define	XFS_AG_MAX_BLOCKS(bl)		xfs_ag_max_blocks(bl)
#else
#define	XFS_AG_MAX_BLOCKS(bl)	((xfs_extlen_t)(XFS_AG_MAX_BYTES >> bl))
#endif

#define	XFS_MAX_AGNUMBER	((xfs_agnumber_t)(NULLAGNUMBER - 1))

#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_AG_MAXLEVELS)
int xfs_ag_maxlevels(struct xfs_mount *mp);
#define	XFS_AG_MAXLEVELS(mp)		xfs_ag_maxlevels(mp)
#else
#define	XFS_AG_MAXLEVELS(mp)	((mp)->m_ag_maxlevels)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_MIN_FREELIST)
int xfs_min_freelist(xfs_agf_t *a, struct xfs_mount *mp);
#define	XFS_MIN_FREELIST(a,mp)		xfs_min_freelist(a,mp)
#else
#define	XFS_MIN_FREELIST(a,mp)	\
	XFS_MIN_FREELIST_RAW((a)->agf_levels[XFS_BTNUM_BNOi], \
			     (a)->agf_levels[XFS_BTNUM_CNTi], mp)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_MIN_FREELIST_PAG)
int xfs_min_freelist_pag(xfs_perag_t *pag, struct xfs_mount *mp);
#define	XFS_MIN_FREELIST_PAG(pag,mp)	xfs_min_freelist_pag(pag,mp)
#else
#define	XFS_MIN_FREELIST_PAG(pag,mp)	\
	XFS_MIN_FREELIST_RAW((pag)->pagf_levels[XFS_BTNUM_BNOi], \
			     (pag)->pagf_levels[XFS_BTNUM_CNTi], mp)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_MIN_FREELIST_RAW)
int xfs_min_freelist_raw(int bl, int cl, struct xfs_mount *mp);
#define	XFS_MIN_FREELIST_RAW(bl,cl,mp)	xfs_min_freelist_raw(bl,cl,mp)
#else
#define	XFS_MIN_FREELIST_RAW(bl,cl,mp)	\
	(MIN(bl + 1, XFS_AG_MAXLEVELS(mp)) + \
	 MIN(cl + 1, XFS_AG_MAXLEVELS(mp)))
#endif

#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_AGB_TO_FSB)
xfs_fsblock_t xfs_agb_to_fsb(struct xfs_mount *mp, xfs_agnumber_t agno,
			     xfs_agblock_t agbno);
#define XFS_AGB_TO_FSB(mp,agno,agbno)	xfs_agb_to_fsb(mp,agno,agbno)
#else
#define	XFS_AGB_TO_FSB(mp,agno,agbno) \
	(((xfs_fsblock_t)(agno) << (mp)->m_sb.sb_agblklog) | (agbno))
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_FSB_TO_AGNO)
xfs_agnumber_t xfs_fsb_to_agno(struct xfs_mount *mp, xfs_fsblock_t fsbno);
#define	XFS_FSB_TO_AGNO(mp,fsbno)	xfs_fsb_to_agno(mp,fsbno)
#else
#define	XFS_FSB_TO_AGNO(mp,fsbno) \
	((xfs_agnumber_t)((fsbno) >> (mp)->m_sb.sb_agblklog))
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_FSB_TO_AGBNO)
xfs_agblock_t xfs_fsb_to_agbno(struct xfs_mount *mp, xfs_fsblock_t fsbno);
#define	XFS_FSB_TO_AGBNO(mp,fsbno)	xfs_fsb_to_agbno(mp,fsbno)
#else
#define	XFS_FSB_TO_AGBNO(mp,fsbno) \
	((xfs_agblock_t)((fsbno) & XFS_MASK32LO((mp)->m_sb.sb_agblklog)))
#endif

#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_AGB_TO_DADDR)
daddr_t xfs_agb_to_daddr(struct xfs_mount *mp, xfs_agnumber_t agno,
			 xfs_agblock_t agbno);
#define	XFS_AGB_TO_DADDR(mp,agno,agbno)	xfs_agb_to_daddr(mp,agno,agbno)
#else
#define	XFS_AGB_TO_DADDR(mp,agno,agbno) \
	((daddr_t)(XFS_FSB_TO_BB(mp, \
		(xfs_fsblock_t)(agno) * (mp)->m_sb.sb_agblocks + (agbno))))
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DADDR_TO_AGNO)
xfs_agnumber_t xfs_daddr_to_agno(struct xfs_mount *mp, daddr_t d);
#define	XFS_DADDR_TO_AGNO(mp,d)		xfs_daddr_to_agno(mp,d)
#else
#define	XFS_DADDR_TO_AGNO(mp,d) \
	((xfs_agnumber_t)(XFS_BB_TO_FSBT(mp, d) / (mp)->m_sb.sb_agblocks))
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_DADDR_TO_AGBNO)
xfs_agblock_t xfs_daddr_to_agbno(struct xfs_mount *mp, daddr_t d);
#define	XFS_DADDR_TO_AGBNO(mp,d)	xfs_daddr_to_agbno(mp,d)
#else
#define	XFS_DADDR_TO_AGBNO(mp,d) \
	((xfs_agblock_t)(XFS_BB_TO_FSBT(mp, d) % (mp)->m_sb.sb_agblocks))
#endif

#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_AG_DADDR)
daddr_t xfs_ag_daddr(struct xfs_mount *mp, xfs_agnumber_t agno, daddr_t d);
#define	XFS_AG_DADDR(mp,agno,d)		xfs_ag_daddr(mp,agno,d)
#else
#define	XFS_AG_DADDR(mp,agno,d)	(XFS_AGB_TO_DADDR(mp, agno, 0) + (d))
#endif

#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_BUF_TO_AGF)
xfs_agf_t *xfs_buf_to_agf(struct buf *bp);
#define	XFS_BUF_TO_AGF(bp)		xfs_buf_to_agf(bp)
#else
#define	XFS_BUF_TO_AGF(bp)	((xfs_agf_t *)(bp)->b_un.b_addr)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_BUF_TO_AGI)
xfs_agi_t *xfs_buf_to_agi(struct buf *bp);
#define	XFS_BUF_TO_AGI(bp)		xfs_buf_to_agi(bp)
#else
#define	XFS_BUF_TO_AGI(bp)	((xfs_agi_t *)(bp)->b_un.b_addr)
#endif
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_BUF_TO_AGFL)
xfs_agfl_t *xfs_buf_to_agfl(struct buf *bp);
#define	XFS_BUF_TO_AGFL(bp)		xfs_buf_to_agfl(bp)
#else
#define	XFS_BUF_TO_AGFL(bp)	((xfs_agfl_t *)(bp)->b_un.b_addr)
#endif

/*
 * For checking for bad ranges of daddr_t's, covering multiple
 * allocation groups or a single daddr_t that's a superblock copy.
 */
#if XFS_WANT_FUNCS || (XFS_WANT_SPACE && XFSSO_XFS_AG_CHECK_DADDR)
void xfs_ag_check_daddr(struct xfs_mount *mp, daddr_t d, xfs_extlen_t len);
#define	XFS_AG_CHECK_DADDR(mp,d,len)	xfs_ag_check_daddr(mp,d,len)
#else
#define	XFS_AG_CHECK_DADDR(mp,d,len)	\
	((len) == 1 ? \
	    ASSERT((d) == XFS_SB_DADDR || \
		   XFS_DADDR_TO_AGBNO(mp, d) != XFS_SB_DADDR) : \
	    ASSERT(XFS_DADDR_TO_AGNO(mp, d) == \
	           XFS_DADDR_TO_AGNO(mp, (d) + (len) - 1)))
#endif

#endif	/* !_FS_XFS_AG_H */
