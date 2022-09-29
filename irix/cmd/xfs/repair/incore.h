#ident "$Revision: 1.29 $"

/*
 * contains definition information.  implementation (code)
 * is spread out in separate files.
 */

/*
 * block allocation lists
 */
typedef struct ba_rec  {
	void		*addr;
	struct ba_rec	*next;
} ba_rec_t;

void			record_allocation(ba_rec_t *addr, ba_rec_t *list);
void			free_allocations(ba_rec_t *list);

/*
 * block bit map defs -- track state of each filesystem block.
 * ba_bmap is an array of bitstrings declared in the globals.h file.
 * the bitstrings are broken up into 64-bit chunks.  one bitstring per AG.
 */
#define BA_BMAP_SIZE(x)		(howmany(x, 4))

void			set_bmap_rt(xfs_drfsbno_t numblocks);
void			set_bmap_log(xfs_mount_t *mp);
void			set_bmap_fs(xfs_mount_t *mp);
void			teardown_bmap(xfs_mount_t *mp);

void			teardown_rt_bmap(xfs_mount_t *mp);
void			teardown_ag_bmap(xfs_mount_t *mp, xfs_agnumber_t agno);
void			teardown_bmap_finish(xfs_mount_t *mp);

/* blocks are numbered from zero */

/* block records fit into __uint64_t's units */

#define XR_BB_UNIT	64			/* number of bits/unit */
#define XR_BB		4			/* bits per block record */
#define XR_BB_NUM	(XR_BB_UNIT/XR_BB)	/* number of records per unit */
#define XR_BB_MASK	0xF			/* block record mask */

/*
 * bitstring ops -- set/get block states, either in filesystem
 * bno's or in agbno's.  turns out that fsbno addressing is
 * more convenient when dealing with bmap extracted addresses
 * and agbno addressing is more convenient when dealing with
 * meta-data extracted addresses.  So the fsbno versions use
 * mtype (which can be one of the block map types above) to
 * set the correct block map while the agbno versions assume
 * you want to use the regular block map.
 */

#if defined(XR_BMAP_TRACE) || defined(XR_BMAP_DBG)
/*
 * implemented as functions for debugging purposes
 */
int get_agbno_state(xfs_mount_t *mp, xfs_agnumber_t agno,
	xfs_agblock_t ag_blockno);
void set_agbno_state(xfs_mount_t *mp, xfs_agnumber_t agno,
	xfs_agblock_t ag_blockno, int state);

int get_fsbno_state(xfs_mount_t *mp, xfs_dfsbno_t blockno);
void set_fsbno_state(xfs_mount_t *mp, xfs_dfsbno_t blockno, int state);
#else
/*
 * implemented as macros for performance purposes
 */

#define get_agbno_state(mp, agno, ag_blockno) \
			((int) (*(ba_bmap[(agno)] + (ag_blockno)/XR_BB_NUM) \
				 >> (((ag_blockno)%XR_BB_NUM)*XR_BB)) \
				& XR_BB_MASK)
#define set_agbno_state(mp, agno, ag_blockno, state) \
	*(ba_bmap[(agno)] + (ag_blockno)/XR_BB_NUM) = \
		((*(ba_bmap[(agno)] + (ag_blockno)/XR_BB_NUM) & \
	  (~((__uint64_t) XR_BB_MASK << (((ag_blockno)%XR_BB_NUM)*XR_BB)))) | \
	 (((__uint64_t) (state)) << (((ag_blockno)%XR_BB_NUM)*XR_BB)))

#define get_fsbno_state(mp, blockno) \
		get_agbno_state(mp, XFS_FSB_TO_AGNO(mp, (blockno)), \
				XFS_FSB_TO_AGBNO(mp, (blockno)))
#define set_fsbno_state(mp, blockno, state) \
		set_agbno_state(mp, XFS_FSB_TO_AGNO(mp, (blockno)), \
			XFS_FSB_TO_AGBNO(mp, (blockno)), (state))


#define get_agbno_rec(mp, agno, ag_blockno) \
			(*(ba_bmap[(agno)] + (ag_blockno)/XR_BB_NUM))
#endif /* XR_BMAP_TRACE */

/*
 * these work in real-time extents (e.g. fsbno == rt extent number)
 */
#define get_rtbno_state(mp, fsbno) \
			((*(rt_ba_bmap + (fsbno)/XR_BB_NUM) >> \
			(((fsbno)%XR_BB_NUM)*XR_BB)) & XR_BB_MASK)
#define set_rtbno_state(mp, fsbno, state) \
	*(rt_ba_bmap + (fsbno)/XR_BB_NUM) = \
	 ((*(rt_ba_bmap + (fsbno)/XR_BB_NUM) & \
	  (~((__uint64_t) XR_BB_MASK << (((fsbno)%XR_BB_NUM)*XR_BB)))) | \
	 (((__uint64_t) (state)) << (((fsbno)%XR_BB_NUM)*XR_BB)))


/*
 * extent tree definitions
 * right now, there are 3 trees per AG, a bno tree, a bcnt tree
 * and a tree for dup extents.  If the code is modified in the
 * future to use an extent tree instead of a bitmask for tracking
 * fs blocks, then we could lose the dup extent tree if we labelled
 * each extent with the inode that owned it.
 */

typedef unsigned char extent_state_t;

typedef struct extent_tree_node  {
	avlnode_t		avl_node;
	xfs_agblock_t		ex_startblock;	/* starting block (agbno) */
	xfs_extlen_t		ex_blockcount;	/* number of blocks in extent */
	extent_state_t		ex_state;	/* see state flags below */

	struct extent_tree_node		*next;	/* for bcnt extent lists */
#if 0
	xfs_ino_t		ex_inode;	/* owner, NULL if free or  */
						/*	multiply allocated */
#endif
} extent_tree_node_t;

typedef struct rt_extent_tree_node  {
	avlnode_t		avl_node;
	xfs_drtbno_t		rt_startblock;	/* starting realtime block */
	xfs_extlen_t		rt_blockcount;	/* number of blocks in extent */
	extent_state_t		rt_state;	/* see state flags below */

#if 0
	xfs_ino_t		ex_inode;	/* owner, NULL if free or  */
						/*	multiply allocated */
#endif
} rt_extent_tree_node_t;

/* extent states, prefix with XR_ to avoid conflict with buffer cache defines */

#define XR_E_UNKNOWN	0	/* unknown state */
#define XR_E_FREE1	1	/* free block (marked by one fs space tree) */
#define XR_E_FREE	2	/* free block (marked by both fs space trees) */
#define XR_E_INUSE	3	/* extent used by file/dir data or metadata */
#define XR_E_INUSE_FS	4	/* extent used by fs ag header or log */
#define XR_E_MULT	5	/* extent is multiply referenced */
#define XR_E_INO	6	/* extent used by inodes (inode blocks) */
#define XR_E_FS_MAP	7	/* extent used by fs space/inode maps */
#define XR_E_BAD_STATE	8

/* separate state bit, OR'ed into high (4th) bit of ex_state field */

#define XR_E_WRITTEN	0x8	/* extent has been written out, can't reclaim */
#define good_state(state)	(((state) & (~XR_E_WRITTEN)) >= XR_E_UNKNOWN && \
				((state) & (~XR_E_WRITTEN) < XF_E_BAD_STATE))
#define written(state)		((state) & XR_E_WRITTEN)
#define set_written(state)	(state) &= XR_E_WRITTEN

/*
 * bno extent tree functions
 */
void
add_bno_extent(xfs_agnumber_t agno, xfs_agblock_t startblock,
		xfs_extlen_t blockcount);

extent_tree_node_t *
findfirst_bno_extent(xfs_agnumber_t agno);

extent_tree_node_t *
find_bno_extent(xfs_agnumber_t agno, xfs_agblock_t agbno);

extent_tree_node_t *
findfirst_bno_extent(xfs_agnumber_t agno);

#define findnext_bno_extent(exent_ptr)	\
		((extent_tree_node_t *) ((exent_ptr)->avl_node.avl_nextino))

void
get_bno_extent(xfs_agnumber_t agno, extent_tree_node_t *ext);

/*
 * bcnt tree functions
 */
void
add_bcnt_extent(xfs_agnumber_t agno, xfs_agblock_t startblock,
		xfs_extlen_t blockcount);

extent_tree_node_t *
findfirst_bcnt_extent(xfs_agnumber_t agno);

extent_tree_node_t *
find_bcnt_extent(xfs_agnumber_t agno, xfs_agblock_t agbno);

extent_tree_node_t *
findbiggest_bcnt_extent(xfs_agnumber_t agno);

extent_tree_node_t *
findnext_bcnt_extent(xfs_agnumber_t agno, extent_tree_node_t *ext);

extent_tree_node_t *
get_bcnt_extent(xfs_agnumber_t agno, xfs_agblock_t startblock,
		xfs_extlen_t blockcount);

/*
 * duplicate extent tree functions
 */
void		add_dup_extent(xfs_agnumber_t agno,
				xfs_agblock_t startblock,
				xfs_extlen_t blockcount);

int		search_dup_extent(xfs_mount_t *mp,
				xfs_agnumber_t agno,
				xfs_agblock_t agbno);

void		add_rt_dup_extent(xfs_drtbno_t	startblock,
				xfs_extlen_t	blockcount);

int		search_rt_dup_extent(xfs_mount_t	*mp,
					xfs_drtbno_t	bno);

/*
 * extent/tree recyling and deletion routines
 */

/*
 * return an extent node to the extent node free list
 */
void		release_extent_tree_node(extent_tree_node_t *node);

/*
 * recycle all the nodes in the per-AG tree
 */
void		release_dup_extent_tree(xfs_agnumber_t agno);
void		release_agbno_extent_tree(xfs_agnumber_t agno);
void		release_agbcnt_extent_tree(xfs_agnumber_t agno);

/*
 * realtime duplicate extent tree - this one actually frees the memory
 */
void		free_rt_dup_extent_tree(xfs_mount_t *mp);

/*
 * per-AG extent trees shutdown routine -- all (bno, bcnt and dup)
 * at once.  this one actually frees the memory instead of just recyling
 * the nodes.
 */
void		incore_ext_teardown(xfs_mount_t *mp);

/*
 * inode definitions
 */

/* inode types */

#define XR_INO_UNKNOWN	0		/* unknown */
#define XR_INO_DIR	1		/* directory */
#define XR_INO_RTDATA	2		/* realtime file */
#define XR_INO_RTBITMAP	3		/* realtime bitmap inode */
#define XR_INO_RTSUM	4		/* realtime summary inode */
#define XR_INO_DATA	5		/* regular file */
#define XR_INO_SYMLINK	6		/* symlink */
#define XR_INO_CHRDEV	7		/* character device */
#define XR_INO_BLKDEV	8		/* block device */
#define XR_INO_SOCK	9		/* socket */
#define XR_INO_FIFO	10		/* fifo */
#define XR_INO_MOUNTPOINT 11		/* mountpoint */

/* inode allocation tree */

/*
 * Inodes in the inode allocation trees are allocated in chunks.
 * Those groups can be easily duplicated in our trees.
 * Disconnected inodes are harder.  We can do one of two
 * things in that case:  if we know the inode allocation btrees
 * are good, then we can disallow directory references to unknown
 * inode chunks.  If the inode allocation trees have been trashed or
 * we feel like being aggressive, then as we hit unknown inodes,
 * we can search on the disk for all contiguous inodes and see if
 * they fit into chunks.  Before putting them into the inode tree,
 * we can scan each inode starting at the earliest inode to see which
 * ones are good.  This protects us from the pathalogical case of
 * inodes appearing in user-data.  We still may have to mark the
 * inodes as "possibly fake" so that if a file claims the blocks,
 * we decide to believe the inodes, especially if they're not
 * connected.
 */

#define PLIST_CHUNK_SIZE	4

typedef xfs_ino_t parent_entry_t;

typedef struct parent_list  {
	__uint64_t		pmask;
	parent_entry_t		*pentries;
#ifdef DEBUG
	short			cnt;
#endif
} parent_list_t;

typedef struct backptrs  {
	__uint64_t		ino_reached;	/* bit == 1 if reached */
	__uint64_t		ino_processed;	/* reference checked bit mask */
	__uint32_t		nlinks[XFS_INODES_PER_CHUNK];
	parent_list_t		*parents;
} backptrs_t;

typedef struct ino_tree_node  {
	avlnode_t		avl_node;
	xfs_agino_t		ino_startnum;	/* starting inode # */
	xfs_inofree_t		ir_free;	/* inode free bit mask */
	__uint64_t		ino_confirmed;	/* confirmed bitmask */
	__uint64_t		ino_isa_dir;	/* bit == 1 if a directory */
	union  {
		backptrs_t	*backptrs;
		parent_list_t	*plist;
	} ino_un;
} ino_tree_node_t;

#define INOS_PER_IREC		(sizeof(__uint64_t) * NBBY)
void				add_ino_backptrs(xfs_mount_t *mp);

/*
 * return an inode record to the free inode record pool
 */
void		free_inode_rec(xfs_agnumber_t agno, ino_tree_node_t *ino_rec);

/*
 * get pulls the inode record from the good inode tree
 */
void		get_inode_rec(xfs_agnumber_t agno, ino_tree_node_t *ino_rec);

ino_tree_node_t *findfirst_inode_rec(xfs_agnumber_t agno);
ino_tree_node_t *find_inode_rec(xfs_agnumber_t agno, xfs_agino_t ino);
void		find_inode_rec_range(xfs_agnumber_t agno,
			xfs_agino_t start_ino, xfs_agino_t end_ino,
			ino_tree_node_t **first, ino_tree_node_t **last);

/*
 * set inode states -- setting an inode to used or free also
 * automatically marks it as "existing".  Note -- all the inode
 * add/set/get routines assume a valid inode number.
 */
ino_tree_node_t	*set_inode_used_alloc(xfs_agnumber_t agno, xfs_agino_t ino);
ino_tree_node_t	*set_inode_free_alloc(xfs_agnumber_t agno, xfs_agino_t ino);

void		print_inode_list(xfs_agnumber_t agno);
void		print_uncertain_inode_list(xfs_agnumber_t agno);

/*
 * separate trees for uncertain inodes (they may not exist).
 */
ino_tree_node_t		*findfirst_uncertain_inode_rec(xfs_agnumber_t agno);
void			add_inode_uncertain(xfs_mount_t *mp,
						xfs_ino_t ino, int free);
void			add_aginode_uncertain(xfs_agnumber_t agno,
						xfs_agino_t agino, int free);
void			get_uncertain_inode_rec(xfs_agnumber_t agno,
						ino_tree_node_t *ino_rec);
void			clear_uncertain_ino_cache(xfs_agnumber_t agno);

/*
 * return next in-order inode tree node.  takes an "ino_tree_node_t *"
 */
#define next_ino_rec(ino_node_ptr)	\
		((ino_tree_node_t *) ((ino_node_ptr)->avl_node.avl_nextino))
/*
 * return the next linked inode (forward avl tree link)-- meant to be used
 * by linked list routines (uncertain inode routines/records)
 */
#define next_link_rec(ino_node_ptr)	\
		((ino_tree_node_t *) ((ino_node_ptr)->avl_node.avl_forw))

/*
 * Bit manipulations for processed field
 */
#define	XFS_INOPROC_MASK(i)	((__uint64_t)1 << (i))
#define	XFS_INOPROC_MASKN(i,n)	((__uint64_t)((1 << (n)) - 1) << (i))

#define	XFS_INOPROC_IS_PROC(rp, i) \
	(((rp)->ino_un.backptrs->ino_processed & XFS_INOPROC_MASK((i))) == 0LL \
		? 0 : 1)
#define	XFS_INOPROC_SET_PROC(rp, i) \
	((rp)->ino_un.backptrs->ino_processed |= XFS_INOPROC_MASK((i)))
/*
#define	XFS_INOPROC_CLR_PROC(rp, i) \
	((rp)->ino_un.backptrs->ino_processed &= ~XFS_INOPROC_MASK((i)))
*/

/*
 * same for ir_confirmed.
 */
#define	XFS_INOCF_MASK(i)	((__uint64_t)1 << (i))
#define	XFS_INOCF_MASKN(i,n)	((__uint64_t)((1 << (n)) - 1) << (i))

#define	XFS_INOCF_IS_CF(rp, i) \
		(((rp)->ino_confirmed & XFS_INOCF_MASK((i))) == 0LL \
			? 0 : 1)
#define	XFS_INOCF_SET_CF(rp, i) \
			((rp)->ino_confirmed |= XFS_INOCF_MASK((i)))
#define	XFS_INOCF_CLR_CF(rp, i) \
			((rp)->ino_confirmed &= ~XFS_INOCF_MASK((i)))

/*
 * same for backptr->ino_reached
 */
#define	XFS_INO_RCHD_MASK(i)	((__uint64_t)1 << (i))

#define	XFS_INO_RCHD_IS_RCHD(rp, i) \
	(((rp)->ino_un.backptrs->ino_reached & XFS_INO_RCHD_MASK((i))) == 0LL \
		? 0 : 1)
#define	XFS_INO_RCHD_SET_RCHD(rp, i) \
		((rp)->ino_un.backptrs->ino_reached |= XFS_INO_RCHD_MASK((i)))
#define	XFS_INO_RCHD_CLR_RCHD(rp, i) \
		((rp)->ino_un.backptrs->ino_reached &= ~XFS_INO_RCHD_MASK((i)))
/*
 * set/clear/test is inode a directory inode
 */
#define	XFS_INO_ISADIR_MASK(i)	((__uint64_t)1 << (i))

#define inode_isadir(ino_rec, ino_offset) \
	(((ino_rec)->ino_isa_dir & XFS_INO_ISADIR_MASK((ino_offset))) == 0LL \
		? 0 : 1)
#define set_inode_isadir(ino_rec, ino_offset) \
		((ino_rec)->ino_isa_dir |= XFS_INO_ISADIR_MASK((ino_offset)))
#define clear_inode_isadir(ino_rec, ino_offset) \
		((ino_rec)->ino_isa_dir &= ~XFS_INO_ISADIR_MASK((ino_offset)))


/*
 * set/clear/test is inode known to be valid (although perhaps corrupt)
 */
#define clear_inode_confirmed(ino_rec, ino_offset) \
			XFS_INOCF_CLR_CF((ino_rec), (ino_offset))

#define set_inode_confirmed(ino_rec, ino_offset) \
			XFS_INOCF_SET_CF((ino_rec), (ino_offset))

#define is_inode_confirmed(ino_rec, ino_offset) \
			XFS_INOCF_IS_CF(ino_rec, ino_offset)

/*
 * set/clear/test is inode free or used
 */
#define set_inode_free(ino_rec, ino_offset) \
	XFS_INOCF_SET_CF((ino_rec), (ino_offset)), \
	XFS_INOBT_SET_FREE((ino_rec), (ino_offset))

#define set_inode_used(ino_rec, ino_offset) \
	XFS_INOCF_SET_CF((ino_rec), (ino_offset)), \
	XFS_INOBT_CLR_FREE((ino_rec), (ino_offset))

#define is_inode_used(ino_rec, ino_offset)	\
	!XFS_INOBT_IS_FREE((ino_rec), (ino_offset))

#define is_inode_free(ino_rec, ino_offset)	\
	XFS_INOBT_IS_FREE((ino_rec), (ino_offset))

/*
 * add_inode_reached() is set on inode I only if I has been reached
 * by an inode P claiming to be the parent and if I is a directory,
 * the .. link in the I says that P is I's parent.
 *
 * add_inode_ref() is called every time a link to an inode is
 * detected and drop_inode_ref() is called every time a link to
 * an inode that we've counted is removed.
 */

void		add_inode_reached(ino_tree_node_t *ino_rec, int ino_offset);
void		add_inode_ref(ino_tree_node_t *ino_rec, int ino_offset);
void		drop_inode_ref(ino_tree_node_t *ino_rec, int ino_offset);
int		is_inode_reached(ino_tree_node_t *ino_rec, int ino_offset);
int		is_inode_referenced(ino_tree_node_t *ino_rec, int ino_offset);
__uint32_t	num_inode_references(ino_tree_node_t *ino_rec, int ino_offset);

/*
 * has an inode been processed for phase 6 (reference count checking)?
 * add_inode_refchecked() is set on an inode when it gets traversed
 * during the reference count phase (6).  It's set so that if the inode
 * is a directory, it's traversed (and it's links counted) only once.
 */
#ifndef XR_INO_REF_DEBUG
#define add_inode_refchecked(ino, ino_rec, ino_offset) \
		XFS_INOPROC_SET_PROC((ino_rec), (ino_offset))
#define is_inode_refchecked(ino, ino_rec, ino_offset) \
		(XFS_INOPROC_IS_PROC(ino_rec, ino_offset) == 0LL ? 0 : 1)
#else
void add_inode_refchecked(xfs_ino_t ino,
			ino_tree_node_t *ino_rec, int ino_offset);
int is_inode_refchecked(xfs_ino_t ino,
			ino_tree_node_t *ino_rec, int ino_offset);
#endif /* XR_INO_REF_DEBUG */

/*
 * set/get inode number of parent -- works for directory inodes only
 */
void		set_inode_parent(ino_tree_node_t *irec, int ino_offset,
					xfs_ino_t ino);
#if 0
void		clear_inode_parent(ino_tree_node_t *irec, int offset);
#endif
xfs_ino_t	get_inode_parent(ino_tree_node_t *irec, int ino_offset);

/*
 * bmap cursor for tracking and fixing bmap btrees.  All xfs btrees number
 * the levels with 0 being the leaf and every level up being 1 greater.
 */

#define XR_MAX_BMLEVELS		10	/* XXX - rcc need to verify number */

typedef struct bm_level_state  {
	xfs_dfsbno_t		fsbno;
	xfs_dfsbno_t		left_fsbno;
	xfs_dfsbno_t		right_fsbno;
	__uint64_t		first_key;
	__uint64_t		last_key;
/*
	int			level;
	__uint64_t		prev_last_key;
	buf_t			*bp;
	xfs_bmbt_block_t	*block;
*/
} bm_level_state_t;

typedef struct bm_cursor  {
	int			num_levels;
	xfs_ino_t		ino;
	xfs_dinode_t		*dip;
	bm_level_state_t	level[XR_MAX_BMLEVELS];
} bmap_cursor_t;

void init_bm_cursor(bmap_cursor_t *cursor, int num_level);
