#ident "$Revision: 1.33 $"

#include "vsn.h"
#define _KERNEL 1
#include <sys/param.h>
#include <sys/buf.h>
#undef _KERNEL
#if VERS <= V_64
#include <sys/avl.h>
#else
#include "avl.h"
#endif
#include <sys/uuid.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <bstring.h>
#if VERS >= V_64
#include <ksys/behavior.h>
#endif
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_mount.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_dir2_sf.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_inode.h>

#include "sim.h"
#include "globals.h"
#include "agheader.h"
#include "incore.h"
#include "protos.h"
#include "err_protos.h"
#include "dinode.h"
#include "rt.h"
#include "versions.h"

/*
 * we maintain the current slice (path from root to leaf)
 * of the btree incore.  when we need a new block, we ask
 * the block allocator for the address of a block on that
 * level, map the block in, and set up the appropriate
 * pointers (child, silbing, etc.) and keys that should
 * point to the new block.
 */
typedef struct bt_stat_level  {
	/*
	 * set in setup_cursor routine and maintained in the tree-building
	 * routines
	 */
	buf_t			*buf_p;		/* 2 buffer pointers to ... */
	buf_t			*prev_buf_p;
	xfs_agblock_t		agbno;		/* current block being filled */
	xfs_agblock_t		prev_agbno;	/* previous block */
	/*
	 * set in calculate/init cursor routines for each btree level
	 */
	int			num_recs_tot;	/* # tree recs in level */
	int			num_blocks;	/* # tree blocks in level */
	int			num_recs_pb;	/* num_recs_tot / num_blocks */
	int			modulo;		/* num_recs_tot % num_blocks */
} bt_stat_level_t;

typedef struct bt_status  {
	int			init;		/* cursor set up once? */
	int			num_levels;	/* # of levels in btree */
	xfs_extlen_t		num_tot_blocks;	/* # blocks alloc'ed for tree */
	xfs_extlen_t		num_free_blocks;/* # blocks currently unused */

	xfs_agblock_t		root;		/* root block */
	/*
	 * list of blocks to be used to set up this tree
	 * and pointer to the first unused block on the list
	 */
	xfs_agblock_t		*btree_blocks;		/* block list */
	xfs_agblock_t		*free_btree_blocks;	/* first unused block */
	/*
	 * per-level status info
	 */
	bt_stat_level_t		level[XFS_BTREE_MAXLEVELS];
} bt_status_t;


int
mk_incore_fstree(xfs_mount_t *mp, xfs_agnumber_t agno)
{
	int			in_extent;
	int			num_extents;
	xfs_agblock_t		extent_start;
	xfs_extlen_t		extent_len;
	xfs_agblock_t		agbno;
	xfs_agblock_t		ag_end;
	uint			free_blocks;
#ifdef XR_BLD_FREE_TRACE
	int			old_state;
	int			state = XR_E_BAD_STATE;
#endif

	/*
	 * scan the bitmap for the ag looking for continuous
	 * extents of free blocks.  At this point, we know
	 * that blocks in the bitmap are either set to an
	 * "in use" state or set to unknown (0) since the
	 * bmaps were bzero'ed in phase 4 and only blocks
	 * being used by inodes, inode bmaps, ag headers,
	 * and the files themselves were put into the bitmap.
	 *
	 */
	assert(agno < mp->m_sb.sb_agcount);

	extent_start = 0;
	in_extent = 0;
	num_extents = free_blocks = 0;

	if (agno < mp->m_sb.sb_agcount - 1)
		ag_end = mp->m_sb.sb_agblocks;
	else
		ag_end = mp->m_sb.sb_dblocks -
			mp->m_sb.sb_agblocks * (mp->m_sb.sb_agcount - 1);

	/*
	 * ok, now find the number of extents, keep track of the
	 * largest extent.
	 */
	for (agbno = 0; agbno < ag_end; agbno++)  {
#if 0
		old_state = state;
		state = get_agbno_state(mp, agno, agbno);
		if (state != old_state)  {
			fprintf(stderr, "agbno %u - new state is %d\n",
					agbno, state);
		}
#endif
		if (get_agbno_state(mp, agno, agbno) < XR_E_INUSE)  {
			free_blocks++;
			if (in_extent == 0)  {
				/*
				 * found the start of a free extent
				 */
				in_extent = 1;
				num_extents++;
				extent_start = agbno;
				extent_len = 1;
			} else  {
				extent_len++;
			}
		} else   {
			if (in_extent)  {
				/*
				 * free extent ends here, add extent to the
				 * 2 incore extent (avl-to-be-B+) trees
				 */
				in_extent = 0;
#if defined(XR_BLD_FREE_TRACE) && defined(XR_BLD_ADD_EXTENT)
				fprintf(stderr, "adding extent %u [%u %u]\n",
					agno, extent_start, extent_len);
#endif
				add_bno_extent(agno, extent_start, extent_len);
				add_bcnt_extent(agno, extent_start, extent_len);
			}
		}
	}
	if (in_extent)  {
		/*
		 * free extent ends here
		 */
		in_extent = 0;
#if defined(XR_BLD_FREE_TRACE) && defined(XR_BLD_ADD_EXTENT)
		fprintf(stderr, "adding extent %u [%u %u]\n",
			agno, extent_start, extent_len);
#endif
		add_bno_extent(agno, extent_start, extent_len);
		add_bcnt_extent(agno, extent_start, extent_len);
	}

	return(num_extents);
}

/* ARGSUSED */
xfs_agblock_t
get_next_blockaddr(xfs_agnumber_t agno, int level, bt_status_t *curs)
{
	assert(curs->free_btree_blocks < curs->btree_blocks +
						curs->num_tot_blocks);
	assert(curs->num_free_blocks > 0);

	curs->num_free_blocks--;
	return(*curs->free_btree_blocks++);
}

/*
 * set up the dynamically allocated block allocation data in the btree
 * cursor that depends on the info in the static portion of the cursor.
 * allocates space from the incore bno/bcnt extent trees and sets up
 * the first path up the left side of the tree.  Also sets up the
 * cursor pointer to the btree root.   called by init_freespace_cursor()
 * and init_ino_cursor()
 */
/* ARGSUSED */
void
setup_cursor(xfs_mount_t *mp, xfs_agnumber_t agno, bt_status_t *curs)
{
	int			j;
	unsigned int		u;
	xfs_extlen_t		big_extent_len;
	xfs_agblock_t		big_extent_start;
	extent_tree_node_t	*ext_ptr;
	extent_tree_node_t	*bno_ext_ptr;
	xfs_extlen_t		blocks_allocated;
	xfs_agblock_t		*agb_ptr;

	/*
	 * get the number of blocks we need to allocate, then
	 * set up block number array, set the free block pointer
	 * to the first block in the array, and null the array
	 */
	big_extent_len = curs->num_tot_blocks;
	blocks_allocated = 0;

	assert(big_extent_len > 0);

	if ((curs->btree_blocks = malloc(sizeof(xfs_agblock_t *)
					* big_extent_len)) == NULL)  {
		do_error("could not set up btree block array\n");
		exit(1);
	}

	agb_ptr = curs->free_btree_blocks = curs->btree_blocks;

	for (j = 0; j < curs->num_free_blocks; j++, agb_ptr++)
		*agb_ptr = NULLAGBLOCK;

	/*
	 * grab the smallest extent and use it up, then get the
	 * next smallest.  This mimics the init_*_cursor code.
	 */
	if ((ext_ptr =  findfirst_bcnt_extent(agno)) == NULL)  {
		do_error("error - not enough free space in filesystem\n");
		exit(1);
	}

	agb_ptr = curs->btree_blocks;
	j = curs->level[0].num_blocks;

	/*
	 * set up the free block array
	 */
	while (blocks_allocated < big_extent_len)  {
		/*
		 * use up the extent we've got
		 */
		for (u = 0; u < ext_ptr->ex_blockcount &&
				blocks_allocated < big_extent_len; u++)  {
			assert(agb_ptr < curs->btree_blocks
					+ curs->num_tot_blocks);
			*agb_ptr++ = ext_ptr->ex_startblock + u;
			blocks_allocated++;
		}

		/*
		 * if we only used part of this last extent, then we
		 * need only to reset the extent in the extent
		 * trees and we're done
		 */
		if (u < ext_ptr->ex_blockcount)  {
			big_extent_start = ext_ptr->ex_startblock + u;
			big_extent_len = ext_ptr->ex_blockcount - u;

			assert(big_extent_len > 0);

			bno_ext_ptr = find_bno_extent(agno,
						ext_ptr->ex_startblock);
			assert(bno_ext_ptr != NULL);
			get_bno_extent(agno, bno_ext_ptr);
			release_extent_tree_node(bno_ext_ptr);

			ext_ptr = get_bcnt_extent(agno, ext_ptr->ex_startblock,
					ext_ptr->ex_blockcount);
			release_extent_tree_node(ext_ptr);
#ifdef XR_BLD_FREE_TRACE
			fprintf(stderr, "releasing extent: %u [%u %u]\n",
				agno, ext_ptr->ex_startblock,
				ext_ptr->ex_blockcount);
			fprintf(stderr, "blocks_allocated = %d\n",
				blocks_allocated);
#endif

			add_bno_extent(agno, big_extent_start, big_extent_len);
			add_bcnt_extent(agno, big_extent_start, big_extent_len);

			return;
		}
		/*
		 * delete the used-up extent from both extent trees and
		 * find next biggest extent
		 */
#ifdef XR_BLD_FREE_TRACE
		fprintf(stderr, "releasing extent: %u [%u %u]\n",
			agno, ext_ptr->ex_startblock, ext_ptr->ex_blockcount);
#endif
		bno_ext_ptr = find_bno_extent(agno, ext_ptr->ex_startblock);
		assert(bno_ext_ptr != NULL);
		get_bno_extent(agno, bno_ext_ptr);
		release_extent_tree_node(bno_ext_ptr);

		ext_ptr = get_bcnt_extent(agno, ext_ptr->ex_startblock,
				ext_ptr->ex_blockcount);
		assert(ext_ptr != NULL);
		release_extent_tree_node(ext_ptr);

		ext_ptr = findfirst_bcnt_extent(agno);
	}
#ifdef XR_BLD_FREE_TRACE
	fprintf(stderr, "blocks_allocated = %d\n",
		blocks_allocated);
#endif

	return;
}

void
write_cursor(bt_status_t *curs)
{
	int i;

	for (i = 0; i < curs->num_levels; i++)  {
#if defined(XR_BLD_FREE_TRACE) || defined(XR_BLD_INO_TRACE)
		fprintf(stderr, "writing bt block %u\n", curs->level[i].agbno);
#endif
		if (curs->level[i].prev_buf_p != NULL)  {
			assert(curs->level[i].prev_agbno != NULLAGBLOCK);
			bwrite(curs->level[i].prev_buf_p);
		}
		bwrite(curs->level[i].buf_p);
	}

	return;
}

void
finish_cursor(bt_status_t *curs)
{
	assert(curs->num_free_blocks == 0);
	free(curs->btree_blocks);

	return;
}

/*
 * no-cursor versions of the XFS equivalents.  The address calculators
 * should be used only for interior btree nodes.
 * these are adapted from xfs_alloc_btree.h and xfs_tree.h
 */
#define XR_ALLOC_KEY_ADDR(mp, bp, i) \
	(xfs_alloc_key_t *) ((char *) (bp) + sizeof(xfs_alloc_block_t) \
				+ ((i)-1) * sizeof(xfs_alloc_key_t))

#define XR_ALLOC_PTR_ADDR(mp, bp, i) \
	(xfs_alloc_ptr_t *) ((char *) (bp) + sizeof(xfs_alloc_block_t) \
			+ (mp)->m_alloc_mxr[1] * sizeof(xfs_alloc_key_t) \
			+ ((i)-1) * sizeof(xfs_alloc_ptr_t))

#define XR_ALLOC_BLOCK_MAXRECS(mp, level) \
			XFS_BTREE_BLOCK_MAXRECS((mp)->m_sb.sb_blocksize, \
						xfs_alloc, (level) == 0)

/*
 * this calculates a freespace cursor for an ag.
 * btree_curs is an in/out.  returns the number of
 * blocks that will show up in the AGFL.
 */

int
calculate_freespace_cursor(xfs_mount_t *mp, xfs_agnumber_t agno,
			xfs_agblock_t *extents, bt_status_t *btree_curs)
{
	xfs_extlen_t		blocks_needed;		/* a running count */
	xfs_extlen_t		blocks_allocated_pt;	/* per tree */
	xfs_extlen_t		blocks_allocated_total;	/* for both trees */
	xfs_agblock_t		num_extents;
	int			i;
	int			extents_used;
	int			extra_blocks;
	bt_stat_level_t		*lptr;
	bt_stat_level_t		*p_lptr;
	extent_tree_node_t	*ext_ptr;
	int			level;
#ifdef XR_BLD_FREE_TRACE
	int			old_state;
	int			state = XR_E_BAD_STATE;
#endif
#ifdef XR_BLD_FREE_TRACE
	fprintf(stderr,
		"in init_freespace_cursor, agno = %d\n", agno);
#endif

	num_extents = *extents;
	extents_used = 0;

	assert(num_extents != 0);

	lptr = &btree_curs->level[0];
	btree_curs->init = 1;

	/*
	 * figure out how much space we need for the leaf level
	 * of the tree and set up the cursor for the leaf level
	 * (note that the same code is duplicated further down)
	 */
	lptr->num_blocks = howmany(num_extents, XR_ALLOC_BLOCK_MAXRECS(mp, 0));
	lptr->num_recs_pb = num_extents / lptr->num_blocks;
	lptr->modulo = num_extents % lptr->num_blocks;
	lptr->num_recs_tot = num_extents;
	level = 1;

	/*
	 * if we need more levels, set them up.  # of records
	 * per level is the # of blocks in the level below it
	 */
	if (lptr->num_blocks > 1)  {
		for (; btree_curs->level[level - 1].num_blocks > 1
				&& level < XFS_BTREE_MAXLEVELS;
				level++)  {
			lptr = &btree_curs->level[level];
			p_lptr = &btree_curs->level[level - 1];
			lptr->num_blocks = howmany(p_lptr->num_blocks,
					XR_ALLOC_BLOCK_MAXRECS(mp, level));
			lptr->modulo = p_lptr->num_blocks
					% lptr->num_blocks;
			lptr->num_recs_pb = p_lptr->num_blocks
					/ lptr->num_blocks;
			lptr->num_recs_tot = p_lptr->num_blocks;
		}
	}

	assert(lptr->num_blocks == 1);
	btree_curs->num_levels = level;

	/*
	 * ok, now we have a hypothetical cursor that
	 * will work for both the bno and bcnt trees.
	 * now figure out if using up blocks to set up the
	 * trees will perturb the shape of the freespace tree.
	 * if so, we've over-allocated.  the freespace trees
	 * as they will be *after* accounting for the free space
	 * we've used up will need fewer blocks to to represent
	 * than we've allocated.  We can use the AGFL to hold
	 * XFS_AGFL_SIZE (128) blocks but that's it.
	 * Thus we limit things to XFS_AGFL_SIZE/2 for each of the 2 btrees.
	 * if the number of extra blocks is more than that,
	 * we'll have to be called again.
	 */
	for (blocks_needed = 0, i = 0; i < level; i++)  {
		blocks_needed += btree_curs->level[i].num_blocks;
	}

	/*
	 * record the # of blocks we've allocated
	 */
	blocks_allocated_pt = blocks_needed;
	blocks_needed *= 2;
	blocks_allocated_total = blocks_needed;

	/*
	 * figure out how many free extents will be used up by
	 * our space allocation
	 */
	if ((ext_ptr = findfirst_bcnt_extent(agno)) == NULL)  {
		do_error(
	"can't rebuild fs trees -- not enough free space on ag %u\n",
			agno);
		exit(1);
	}

	i = 0;
	while (ext_ptr != NULL && blocks_needed > 0)  {
		if (ext_ptr->ex_blockcount <= blocks_needed)  {
			blocks_needed -= ext_ptr->ex_blockcount;
			extents_used++;
		} else  {
			blocks_needed = 0;
		}

		ext_ptr = findnext_bcnt_extent(agno, ext_ptr);

#ifdef XR_BLD_FREE_TRACE
		if (ext_ptr != NULL)  {
			fprintf(stderr, "got next extent [%u %u]\n",
				ext_ptr->ex_startblock, ext_ptr->ex_blockcount);
		} else  {
			fprintf(stderr, "out of extents\n");
		}
#endif
	}
	if (blocks_needed > 0)  {
		do_error(
		    "ag %u - not enough free space to build freespace btrees\n",
		    agno);
		exit(1);
	}

	assert (num_extents >= extents_used);

	num_extents -= extents_used;

	/*
	 * see if the number of leaf blocks will change as a result
	 * of the number of extents changing
	 */
	if (howmany(num_extents, XR_ALLOC_BLOCK_MAXRECS(mp, 0))
			!= btree_curs->level[0].num_blocks)  {
		/*
		 * yes -- recalculate the cursor.  If the number of
		 * excess (overallocated) blocks is < XFS_AGFL_SIZE/2, we're ok.
		 * we can put those into the AGFL.  we don't try
		 * and get things to converge exactly (reach a
		 * state with zero excess blocks) because there
		 * exist pathological cases which will never
		 * converge.  first, check for the zero-case.
		 */
		if (num_extents == 0)  {
			/*
			 * ok, we've used up all the free blocks
			 * trying to lay out the leaf level. go
			 * to a one block (empty) btree and put the
			 * already allocated blocks into the AGFL
			 */
			if (btree_curs->level[0].num_blocks != 1)  {
				/*
				 * we really needed more blocks because
				 * the old tree had more than one level.
				 * this is bad.
				 */
				 do_warn(
	"not enough free blocks left to describe all free blocks in AG %u\n",
					agno);
			}
#ifdef XR_BLD_FREE_TRACE
			fprintf(stderr,
				"ag %u -- no free extents, alloc'ed %d\n",
				agno, blocks_allocated_pt);
#endif
			lptr->num_blocks = 1;
			lptr->modulo = 0;
			lptr->num_recs_pb = 0;
			lptr->num_recs_tot = 0;

			btree_curs->num_levels = 1;

			/*
			 * don't reset the allocation stats, assume
			 * they're all extra blocks
			 * don't forget to return the total block count
			 * not the per-tree block count.  these are the
			 * extras that will go into the AGFL.  subtract
			 * two for the root blocks.
			 */
			btree_curs->num_tot_blocks = blocks_allocated_pt;
			btree_curs->num_free_blocks = blocks_allocated_pt;

			*extents = 0;

			return(blocks_allocated_total - 2);
		}

		lptr = &btree_curs->level[0];
		lptr->num_blocks = howmany(num_extents,
					XR_ALLOC_BLOCK_MAXRECS(mp, 0));
		lptr->num_recs_pb = num_extents / lptr->num_blocks;
		lptr->modulo = num_extents % lptr->num_blocks;
		lptr->num_recs_tot = num_extents;
		level = 1;

		/*
		 * if we need more levels, set them up
		 */
		if (lptr->num_blocks > 1)  {
			for (level = 1; btree_curs->level[level-1].num_blocks
					> 1 && level < XFS_BTREE_MAXLEVELS;
					level++)  {
				lptr = &btree_curs->level[level];
				p_lptr = &btree_curs->level[level-1];
				lptr->num_blocks = howmany(p_lptr->num_blocks,
						XR_ALLOC_BLOCK_MAXRECS(mp,
								level));
				lptr->modulo = p_lptr->num_blocks
						% lptr->num_blocks;
				lptr->num_recs_pb = p_lptr->num_blocks
						/ lptr->num_blocks;
				lptr->num_recs_tot = p_lptr->num_blocks;
			}
		}
		assert(lptr->num_blocks == 1);
		btree_curs->num_levels = level;

		/*
		 * now figure out the number of excess blocks
		 */
		for (blocks_needed = 0, i = 0; i < level; i++)  {
			blocks_needed += btree_curs->level[i].num_blocks;
		}
		blocks_needed *= 2;

		assert(blocks_allocated_total >= blocks_needed);
		extra_blocks = blocks_allocated_total - blocks_needed;
	} else  {
		if (extents_used > 0) {
			/*
			 * reset the leaf level geometry to account
			 * for consumed extents.  we can leave the
			 * rest of the cursor alone since the number
			 * of leaf blocks hasn't changed.
			 */
			lptr = &btree_curs->level[0];

			lptr->num_recs_pb = num_extents / lptr->num_blocks;
			lptr->modulo = num_extents % lptr->num_blocks;
			lptr->num_recs_tot = num_extents;
		}

		extra_blocks = 0;
	}

	btree_curs->num_tot_blocks = blocks_allocated_pt;
	btree_curs->num_free_blocks = blocks_allocated_pt;

	*extents = num_extents;

	return(extra_blocks);
}

void
prop_freespace_cursor(xfs_mount_t *mp, xfs_agnumber_t agno,
		bt_status_t *btree_curs, xfs_agblock_t startblock,
		xfs_extlen_t blockcount, int level, __uint32_t magic)
{
	xfs_alloc_block_t	*bt_hdr;
	xfs_alloc_key_t		*bt_key;
	xfs_alloc_ptr_t		*bt_ptr;
	xfs_agblock_t		agbno;
	bt_stat_level_t		*lptr;

	level++;

	if (level >= btree_curs->num_levels)
		return;

	lptr = &btree_curs->level[level];
	bt_hdr = XFS_BUF_TO_ALLOC_BLOCK(lptr->buf_p);

	if (bt_hdr->bb_numrecs == 0)  {
		/*
		 * only happens once when initializing the
		 * left-hand side of the tree.
		 */
		prop_freespace_cursor(mp, agno, btree_curs, startblock,
				blockcount, level, magic);
	}

	if (bt_hdr->bb_numrecs == lptr->num_recs_pb + (lptr->modulo > 0))  {
		/*
		 * write out current prev block, grab us a new block,
		 * and set the rightsib pointer of current block
		 */
#ifdef XR_BLD_FREE_TRACE
		fprintf(stderr, " %d ", lptr->prev_agbno);
#endif
		if (lptr->prev_agbno != NULLAGBLOCK)  {
			assert(lptr->prev_buf_p != NULL);
			bwrite(lptr->prev_buf_p);
		}
		lptr->prev_agbno = lptr->agbno;;
		lptr->prev_buf_p = lptr->buf_p;
		agbno = get_next_blockaddr(agno, level, btree_curs);

		bt_hdr->bb_rightsib = agbno;

		lptr->buf_p = get_buf(mp->m_dev,
					XFS_AGB_TO_DADDR(mp, agno, agbno),
					XFS_FSB_TO_BB(mp, 1), BUF_TRYLOCK);
		lptr->agbno = agbno;

		if (lptr->modulo)
			lptr->modulo--;

		/*
		 * initialize block header
		 */
		bt_hdr = XFS_BUF_TO_ALLOC_BLOCK(lptr->buf_p);
		bzero(bt_hdr, mp->m_sb.sb_blocksize);

		bt_hdr->bb_magic = magic;
		bt_hdr->bb_level = level;
		bt_hdr->bb_leftsib = lptr->prev_agbno;
		bt_hdr->bb_rightsib = NULLAGBLOCK;
		bt_hdr->bb_numrecs = 0;

		/*
		 * propagate extent record for first extent in new block up
		 */
		prop_freespace_cursor(mp, agno, btree_curs, startblock,
				blockcount, level, magic);
	}
	/*
	 * add extent info to current block
	 */
	bt_hdr->bb_numrecs++;

	bt_key = XR_ALLOC_KEY_ADDR(mp, bt_hdr, bt_hdr->bb_numrecs);
	bt_ptr = XR_ALLOC_PTR_ADDR(mp, bt_hdr, bt_hdr->bb_numrecs);

	bt_key->ar_startblock = startblock;
	bt_key->ar_blockcount = blockcount;
	*bt_ptr = btree_curs->level[level-1].agbno;

	return;
}

/*
 * rebuilds a freespace tree given a cursor and magic number of type
 * of tree to build (bno or bcnt).  returns the number of free blocks
 * represented by the tree.
 */
xfs_extlen_t
build_freespace_tree(xfs_mount_t *mp, xfs_agnumber_t agno,
		bt_status_t *btree_curs, __uint32_t magic)
{
	xfs_agnumber_t		i;
	xfs_agblock_t		j;
	xfs_alloc_block_t	*bt_hdr;
	xfs_alloc_rec_t		*bt_rec;
	int			level;
	xfs_agblock_t		agbno;
	extent_tree_node_t	*ext_ptr;
	bt_stat_level_t		*lptr;
	xfs_extlen_t		freeblks;

#ifdef XR_BLD_FREE_TRACE
	fprintf(stderr, "in build_freespace_tree, agno = %d\n", agno);
#endif
	level = btree_curs->num_levels;
	freeblks = 0;

	assert(level > 0);

	/*
	 * initialize the first block on each btree level
	 */
	for (i = 0; i < level; i++)  {
		lptr = &btree_curs->level[i];

		agbno = get_next_blockaddr(agno, i, btree_curs);
		lptr->buf_p = get_buf(mp->m_dev, XFS_AGB_TO_DADDR(mp,
					agno, agbno), XFS_FSB_TO_BB(mp, 1), 
					BUF_TRYLOCK);

		if (i == btree_curs->num_levels - 1)
			btree_curs->root = agbno;

		lptr->agbno = agbno;
		lptr->prev_agbno = NULLAGBLOCK;
		lptr->prev_buf_p = NULL;
		/*
		 * initialize block header
		 */
		bt_hdr = XFS_BUF_TO_ALLOC_BLOCK(lptr->buf_p);
		bzero(bt_hdr, mp->m_sb.sb_blocksize);

		bt_hdr->bb_magic = magic;
		bt_hdr->bb_level = i;
		bt_hdr->bb_leftsib = bt_hdr->bb_rightsib = NULLAGBLOCK;
		bt_hdr->bb_numrecs = 0;
	}
	/*
	 * run along leaf, setting up records.  as we have to switch
	 * blocks, call the prop_freespace_cursor routine to set up the new
	 * pointers for the parent.  that can recurse up to the root
	 * if required.  set the sibling pointers for leaf level here.
	 */
	if (magic == XFS_ABTB_MAGIC)
		ext_ptr = findfirst_bno_extent(agno);
	else 
		ext_ptr = findfirst_bcnt_extent(agno);

#ifdef XR_BLD_FREE_TRACE
	fprintf(stderr, "bft, agno = %d, start = %u, count = %u\n",
		agno, ext_ptr->ex_startblock, ext_ptr->ex_blockcount);
#endif

	lptr = &btree_curs->level[0];

	for (i = 0; i < btree_curs->level[0].num_blocks; i++)  {
		/*
		 * block initialization, lay in block header
		 */
		bt_hdr = XFS_BUF_TO_ALLOC_BLOCK(lptr->buf_p);
		bzero(bt_hdr, mp->m_sb.sb_blocksize);

		bt_hdr->bb_magic = magic;
		bt_hdr->bb_level = 0;
		bt_hdr->bb_leftsib = lptr->prev_agbno;
		bt_hdr->bb_rightsib = NULLAGBLOCK;
		bt_hdr->bb_numrecs = lptr->num_recs_pb + (lptr->modulo > 0);
#ifdef XR_BLD_FREE_TRACE
		fprintf(stderr, "bft, bb_numrecs = %d\n", bt_hdr->bb_numrecs);
#endif

		if (lptr->modulo > 0)
			lptr->modulo--;

		/*
		 * initialize values in the path up to the root if
		 * this is a multi-level btree
		 */
		if (btree_curs->num_levels > 1)
			prop_freespace_cursor(mp, agno, btree_curs,
					ext_ptr->ex_startblock,
					ext_ptr->ex_blockcount,
					0, magic);

		bt_rec = (xfs_alloc_rec_t *) ((char *) bt_hdr +
						sizeof(xfs_alloc_block_t));
		for (j = 0; j < bt_hdr->bb_numrecs; j++)  {
			assert(ext_ptr != NULL);
			bt_rec[j].ar_startblock = ext_ptr->ex_startblock;
			bt_rec[j].ar_blockcount = ext_ptr->ex_blockcount;
			freeblks += ext_ptr->ex_blockcount;
			if (magic == XFS_ABTB_MAGIC)
				ext_ptr = findnext_bno_extent(ext_ptr);
			else
				ext_ptr = findnext_bcnt_extent(agno, ext_ptr);
#if 0
#ifdef XR_BLD_FREE_TRACE
			if (ext_ptr == NULL)
				fprintf(stderr, "null extent pointer, j = %d\n",
					j);
			else
				fprintf(stderr,
				"bft, agno = %d, start = %u, count = %u\n",
					agno, ext_ptr->ex_startblock,
					ext_ptr->ex_blockcount);
#endif
#endif
		}

		if (ext_ptr != NULL)  {
			/*
			 * get next leaf level block
			 */
			if (lptr->prev_buf_p != NULL)  {
#ifdef XR_BLD_FREE_TRACE
				fprintf(stderr, " writing fst agbno %u\n",
					lptr->prev_agbno);
#endif
				assert(lptr->prev_agbno != NULLAGBLOCK);
				bwrite(lptr->prev_buf_p);
			}
			lptr->prev_buf_p = lptr->buf_p;
			lptr->prev_agbno = lptr->agbno;

			bt_hdr->bb_rightsib = lptr->agbno =
				get_next_blockaddr(agno, 0, btree_curs);

			lptr->buf_p = get_buf(mp->m_dev,
					XFS_AGB_TO_DADDR(mp, agno,
						lptr->agbno),
					XFS_FSB_TO_BB(mp, 1), BUF_TRYLOCK);
		}
	}

	return(freeblks);
}

/*
 * no-cursor versions of the XFS equivalents.  The address calculators
 * should be used only for interior btree nodes.
 * these are adapted from xfs_ialloc_btree.h and xfs_tree.h
 */
#define XR_INOBT_KEY_ADDR(mp, bp, i) \
	(xfs_inobt_key_t *) ((char *) (bp) + sizeof(xfs_inobt_block_t) \
				+ ((i)-1) * sizeof(xfs_inobt_key_t))

#define XR_INOBT_PTR_ADDR(mp, bp, i) \
	(xfs_inobt_ptr_t *) ((char *) (bp) + sizeof(xfs_inobt_block_t) \
			+ (mp)->m_inobt_mxr[1] * sizeof(xfs_inobt_key_t) \
			+ ((i)-1) * sizeof(xfs_inobt_ptr_t))

#define XR_INOBT_BLOCK_MAXRECS(mp, level) \
			XFS_BTREE_BLOCK_MAXRECS((mp)->m_sb.sb_blocksize, \
						xfs_inobt, (level) == 0)

/*
 * we don't have to worry here about how chewing up free extents
 * may perturb things because inode tree building happens before
 * freespace tree building.
 */
void
init_ino_cursor(xfs_mount_t *mp, xfs_agnumber_t agno, bt_status_t *btree_curs,
		__uint64_t *num_inos, __uint64_t *num_free_inos)
{
	__uint64_t		ninos;
	__uint64_t		nfinos;
	ino_tree_node_t		*ino_rec;
	int			num_recs;
	int			level;
	bt_stat_level_t		*lptr;
	bt_stat_level_t		*p_lptr;
	xfs_extlen_t		blocks_allocated;
	int			i;

	*num_inos = *num_free_inos = 0;
	ninos = nfinos = 0;

	lptr = &btree_curs->level[0];
	btree_curs->init = 1;

	if ((ino_rec = findfirst_inode_rec(agno)) == NULL)  {
		/*
		 * easy corner-case -- no inode records
		 */
		lptr->num_blocks = 1;
		lptr->modulo = 0;
		lptr->num_recs_pb = 0;
		lptr->num_recs_tot = 0;

		btree_curs->num_levels = 1;
		btree_curs->num_tot_blocks = btree_curs->num_free_blocks = 1;

		setup_cursor(mp, agno, btree_curs);

		return;
	}

	/*
	 * build up statistics
	 */
	for (num_recs = 0; ino_rec != NULL; ino_rec = next_ino_rec(ino_rec))  {
		ninos += XFS_INODES_PER_CHUNK;
		num_recs++;
		for (i = 0; i < XFS_INODES_PER_CHUNK; i++)  {
			assert(is_inode_confirmed(ino_rec, i));
			if (is_inode_free(ino_rec, i))
				nfinos++;
		}
	}

	blocks_allocated = lptr->num_blocks = howmany(num_recs,
					XR_INOBT_BLOCK_MAXRECS(mp, 0));

	lptr->modulo = num_recs % lptr->num_blocks;
	lptr->num_recs_pb = num_recs / lptr->num_blocks;
	lptr->num_recs_tot = num_recs;
	level = 1;

	if (lptr->num_blocks > 1)  {
		for (; btree_curs->level[level-1].num_blocks > 1
				&& level < XFS_BTREE_MAXLEVELS;
				level++)  {
			lptr = &btree_curs->level[level];
			p_lptr = &btree_curs->level[level - 1];
			lptr->num_blocks = howmany(p_lptr->num_blocks,
				XR_INOBT_BLOCK_MAXRECS(mp, level));
			lptr->modulo = p_lptr->num_blocks % lptr->num_blocks;
			lptr->num_recs_pb = p_lptr->num_blocks
					/ lptr->num_blocks;
			lptr->num_recs_tot = p_lptr->num_blocks;

			blocks_allocated += lptr->num_blocks;
		}
	}
	assert(lptr->num_blocks == 1);
	btree_curs->num_levels = level;

	btree_curs->num_tot_blocks = btree_curs->num_free_blocks
			= blocks_allocated;

	setup_cursor(mp, agno, btree_curs);

	*num_inos = ninos;
	*num_free_inos = nfinos;

	return;
}

void
prop_ino_cursor(xfs_mount_t *mp, xfs_agnumber_t agno, bt_status_t *btree_curs,
	xfs_agino_t startino, int level)
{
	xfs_inobt_block_t	*bt_hdr;
	xfs_inobt_key_t		*bt_key;
	xfs_inobt_ptr_t		*bt_ptr;
	xfs_agblock_t		agbno;
	bt_stat_level_t		*lptr;

	level++;

	if (level >= btree_curs->num_levels)
		return;

	lptr = &btree_curs->level[level];
	bt_hdr = XFS_BUF_TO_INOBT_BLOCK(lptr->buf_p);

	if (bt_hdr->bb_numrecs == 0)  {
		/*
		 * this only happens once to initialize the
		 * first path up the left side of the tree
		 * where the agbno's are already set up
		 */
		prop_ino_cursor(mp, agno, btree_curs, startino, level);
	}

	if (bt_hdr->bb_numrecs == lptr->num_recs_pb + (lptr->modulo > 0))  {
		/*
		 * write out current prev block, grab us a new block,
		 * and set the rightsib pointer of current block
		 */
#ifdef XR_BLD_INO_TRACE
		fprintf(stderr, " ino prop agbno %d ", lptr->prev_agbno);
#endif
		if (lptr->prev_agbno != NULLAGBLOCK)  {
			assert(lptr->prev_buf_p != NULL);
			bwrite(lptr->prev_buf_p);
		}
		lptr->prev_agbno = lptr->agbno;;
		lptr->prev_buf_p = lptr->buf_p;
		agbno = get_next_blockaddr(agno, level, btree_curs);

		bt_hdr->bb_rightsib = agbno;

		lptr->buf_p = get_buf(mp->m_dev,
					XFS_AGB_TO_DADDR(mp, agno, agbno),
					XFS_FSB_TO_BB(mp, 1), BUF_TRYLOCK);
		lptr->agbno = agbno;

		if (lptr->modulo)
			lptr->modulo--;

		/*
		 * initialize block header
		 */
		bt_hdr = XFS_BUF_TO_INOBT_BLOCK(lptr->buf_p);
		bzero(bt_hdr, mp->m_sb.sb_blocksize);

		bt_hdr->bb_magic = XFS_IBT_MAGIC;
		bt_hdr->bb_level = level;
		bt_hdr->bb_leftsib = lptr->prev_agbno;
		bt_hdr->bb_rightsib = NULLAGBLOCK;
		bt_hdr->bb_numrecs = 0;
		/*
		 * propagate extent record for first extent in new block up
		 */
		prop_ino_cursor(mp, agno, btree_curs, startino, level);
	}
	/*
	 * add inode info to current block
	 */
	bt_hdr->bb_numrecs++;

	bt_key = XR_INOBT_KEY_ADDR(mp, bt_hdr, bt_hdr->bb_numrecs);
	bt_ptr = XR_INOBT_PTR_ADDR(mp, bt_hdr, bt_hdr->bb_numrecs);

	bt_key->ir_startino = startino;
	*bt_ptr = btree_curs->level[level-1].agbno;

	return;
}

void
build_agi(xfs_mount_t *mp, xfs_agnumber_t agno,
		bt_status_t *btree_curs, xfs_agino_t first_agino,
		xfs_agino_t count, xfs_agino_t freecount)
{
	buf_t			*agi_buf;
	xfs_agi_t		*agi;
	int			i;

	agi_buf = get_buf(mp->m_dev, XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR),
				mp->m_sb.sb_sectsize/BBSIZE, BUF_TRYLOCK);
	agi = XFS_BUF_TO_AGI(agi_buf);
	bzero(agi, mp->m_sb.sb_sectsize);

	agi->agi_magicnum = XFS_AGI_MAGIC;
	agi->agi_versionnum = XFS_AGI_VERSION;
	agi->agi_seqno = agno;
	if (agno < mp->m_sb.sb_agcount - 1)
		agi->agi_length = mp->m_sb.sb_agblocks;
	else
		agi->agi_length = mp->m_sb.sb_dblocks -
			(xfs_drfsbno_t) mp->m_sb.sb_agblocks * agno;
	agi->agi_count = count;
	agi->agi_root = btree_curs->root;
	agi->agi_level = btree_curs->num_levels;
	agi->agi_freecount = freecount;
	agi->agi_newino = first_agino;
	agi->agi_dirino = NULLAGINO;

	for (i = 0; i < XFS_AGI_UNLINKED_BUCKETS; i++)  {
		agi->agi_unlinked[i] = NULLAGINO;
	}

	bwrite(agi_buf);
}

/*
 * rebuilds an inode tree given a cursor.  We're lazy here and call
 * the routine that builds the agi
 */
void
build_ino_tree(xfs_mount_t *mp, xfs_agnumber_t agno,
		bt_status_t *btree_curs)
{
	xfs_agnumber_t		i;
	xfs_agblock_t		j;
	xfs_agblock_t		agbno;
	xfs_agino_t		first_agino;
	xfs_inobt_block_t	*bt_hdr;
	xfs_inobt_rec_t		*bt_rec;
	ino_tree_node_t		*ino_rec;
	bt_stat_level_t		*lptr;
	xfs_agino_t		count = 0;
	xfs_agino_t		freecount = 0;
	int			inocnt;
	int			k;
	int			level = btree_curs->num_levels;

	for (i = 0; i < level; i++)  {
		lptr = &btree_curs->level[i];

		agbno = get_next_blockaddr(agno, i, btree_curs);
		lptr->buf_p = get_buf(mp->m_dev, XFS_AGB_TO_DADDR(mp,
					agno, agbno), XFS_FSB_TO_BB(mp, 1), 
					BUF_TRYLOCK);

		if (i == btree_curs->num_levels - 1)
			btree_curs->root = agbno;

		lptr->agbno = agbno;
		lptr->prev_agbno = NULLAGBLOCK;
		lptr->prev_buf_p = NULL;
		/*
		 * initialize block header
		 */
		bt_hdr = XFS_BUF_TO_INOBT_BLOCK(lptr->buf_p);
		bzero(bt_hdr, mp->m_sb.sb_blocksize);

		bt_hdr->bb_magic = XFS_IBT_MAGIC;
		bt_hdr->bb_level = i;
		bt_hdr->bb_leftsib = bt_hdr->bb_rightsib = NULLAGBLOCK;
		bt_hdr->bb_numrecs = 0;
	}
	/*
	 * run along leaf, setting up records.  as we have to switch
	 * blocks, call the prop_ino_cursor routine to set up the new
	 * pointers for the parent.  that can recurse up to the root
	 * if required.  set the sibling pointers for leaf level here.
	 */
	ino_rec = findfirst_inode_rec(agno);

	if (ino_rec != NULL)
		first_agino = ino_rec->ino_startnum;
	else
		first_agino = NULLAGINO;

	lptr = &btree_curs->level[0];

	for (i = 0; i < lptr->num_blocks; i++)  {
		/*
		 * block initialization, lay in block header
		 */
		bt_hdr = XFS_BUF_TO_INOBT_BLOCK(lptr->buf_p);
		bzero(bt_hdr, mp->m_sb.sb_blocksize);

		bt_hdr->bb_magic = XFS_IBT_MAGIC;
		bt_hdr->bb_level = 0;
		bt_hdr->bb_leftsib = lptr->prev_agbno;
		bt_hdr->bb_rightsib = NULLAGBLOCK;
		bt_hdr->bb_numrecs = lptr->num_recs_pb + (lptr->modulo > 0);

		if (lptr->modulo > 0)
			lptr->modulo--;

		if (lptr->num_recs_pb > 0)
			prop_ino_cursor(mp, agno, btree_curs,
					ino_rec->ino_startnum, 0);

		bt_rec = (xfs_inobt_rec_t *) ((char *) bt_hdr +
						sizeof(xfs_inobt_block_t));
		for (j = 0; j < bt_hdr->bb_numrecs; j++)  {
			assert(ino_rec != NULL);
			bt_rec[j].ir_startino = ino_rec->ino_startnum;
			bt_rec[j].ir_free = ino_rec->ir_free;

			inocnt = 0;
			for (k = 0; k < sizeof(xfs_inofree_t)*NBBY; k++)  {
				assert(is_inode_confirmed(ino_rec, k));
				inocnt += is_inode_free(ino_rec, k);
			}

			bt_rec[j].ir_freecount = inocnt;
			freecount += inocnt;
			count += XFS_INODES_PER_CHUNK;
			ino_rec = next_ino_rec(ino_rec);
		}

		if (ino_rec != NULL)  {
			/*
			 * get next leaf level block
			 */
			if (lptr->prev_buf_p != NULL)  {
#ifdef XR_BLD_INO_TRACE
				fprintf(stderr, "writing inobt agbno %u\n",
					lptr->prev_agbno);
#endif
				assert(lptr->prev_agbno != NULLAGBLOCK);
				bwrite(lptr->prev_buf_p);
			}
			lptr->prev_buf_p = lptr->buf_p;
			lptr->prev_agbno = lptr->agbno;

			bt_hdr->bb_rightsib = lptr->agbno =
				get_next_blockaddr(agno, 0, btree_curs);

			lptr->buf_p = get_buf(mp->m_dev,
					XFS_AGB_TO_DADDR(mp, agno, lptr->agbno),
					XFS_FSB_TO_BB(mp, 1), BUF_TRYLOCK);
		}
	}

	build_agi(mp, agno, btree_curs, first_agino, count, freecount);

	return;
}

/*
 * build both the agf and the agfl for an agno given both
 * btree cursors
 */
void
build_agf_agfl(xfs_mount_t	*mp,
		xfs_agnumber_t	agno,
		bt_status_t	*bno_bt,
		bt_status_t	*bcnt_bt,
		xfs_extlen_t	freeblks,	/* # free blocks in tree */
		int		lostblocks)	/* # blocks that will be lost */
{
	extent_tree_node_t	*ext_ptr;
	buf_t			*agf_buf, *agfl_buf;
	int			i;
	int			j;
	xfs_agfl_t		*agfl;
	xfs_agf_t		*agf;
				/* REFERENCED */
	int			error;

	agf_buf = get_buf(mp->m_dev, XFS_AG_DADDR(mp, agno, XFS_AGF_DADDR),
				mp->m_sb.sb_sectsize/BBSIZE, BUF_TRYLOCK);
	agf = XFS_BUF_TO_AGF(agf_buf);
	bzero(agf, mp->m_sb.sb_sectsize);

#ifdef XR_BLD_FREE_TRACE
	fprintf(stderr, "agf = 0x%x, agf_buf->b_un.b_addr = 0x%x\n",
		(__psint_t) agf, (__psint_t) agf_buf->b_un.b_addr);
#endif

	/*
	 * set up fixed part of agf
	 */
	agf->agf_magicnum = XFS_AGF_MAGIC;
	agf->agf_versionnum = XFS_AGF_VERSION;
	agf->agf_seqno = agno;

	if (agno < mp->m_sb.sb_agcount - 1)
		agf->agf_length = mp->m_sb.sb_agblocks;
	else
		agf->agf_length = mp->m_sb.sb_dblocks -
			(xfs_drfsbno_t) mp->m_sb.sb_agblocks * agno;

	agf->agf_roots[XFS_BTNUM_BNO] = bno_bt->root;
	agf->agf_levels[XFS_BTNUM_BNO] = bno_bt->num_levels;
	agf->agf_roots[XFS_BTNUM_CNT] = bcnt_bt->root;
	agf->agf_levels[XFS_BTNUM_CNT] = bcnt_bt->num_levels;
	agf->agf_freeblks = freeblks;

#ifdef XR_BLD_FREE_TRACE
	fprintf(stderr, "bno root = %u, bcnt root = %u, indices = %u %u\n",
			agf->agf_roots[XFS_BTNUM_BNO],
			agf->agf_roots[XFS_BTNUM_CNT],
			XFS_BTNUM_BNO,
			XFS_BTNUM_CNT);
#endif

	/*
	 * do we have left-over blocks in the btree cursors that should
	 * be used to fill the AGFL?
	 */
	if (bno_bt->num_free_blocks > 0 || bcnt_bt->num_free_blocks > 0)  {
		/*
		 * yes - grab the AGFL buffer
		 */
		agfl_buf = get_buf(mp->m_dev,
				XFS_AG_DADDR(mp, agno, XFS_AGFL_DADDR),
				mp->m_sb.sb_sectsize/BBSIZE, BUF_TRYLOCK);
		agfl = XFS_BUF_TO_AGFL(agfl_buf);
		bzero(agfl, mp->m_sb.sb_sectsize);
		/*
		 * ok, now grab as many blocks as we can
		 */
		i = j = 0;
		while (bno_bt->num_free_blocks > 0 && i < XFS_AGFL_SIZE)  {
			agfl->agfl_bno[i] = get_next_blockaddr(agno, 0, bno_bt);
			i++;
		}

		while (bcnt_bt->num_free_blocks > 0 && i < XFS_AGFL_SIZE)  {
			agfl->agfl_bno[i] = get_next_blockaddr(agno, 0,
								bcnt_bt);
			i++;
		}
		/*
		 * now throw the rest of the blocks away and complain
		 */
		while (bno_bt->num_free_blocks > 0)  {
			(void) get_next_blockaddr(agno, 0, bno_bt);
			j++;
		}
		while (bcnt_bt->num_free_blocks > 0)  {
			(void) get_next_blockaddr(agno, 0, bcnt_bt);
			j++;
		}

		if (j > 0)  {
			if (j == lostblocks)
				do_warn("lost %d blocks in ag %u\n", j, agno);
			else
				do_warn(
"thought we were going to lose %d blocks in ag %u, actually lost %d\n",
					lostblocks, j, agno);
		}

		agf->agf_flfirst = 0;
		agf->agf_fllast = i - 1;
		agf->agf_flcount = i;

#ifdef XR_BLD_FREE_TRACE
		fprintf(stderr, "writing agfl for ag %u\n", agno);
#endif

		bwrite(agfl_buf);
	} else  {
		agf->agf_flfirst = 0;
		agf->agf_fllast = XFS_AGFL_SIZE - 1;
		agf->agf_flcount = 0;
	}

	ext_ptr = findbiggest_bcnt_extent(agno);
	agf->agf_longest = (ext_ptr != NULL) ? ext_ptr->ex_blockcount : 0;

	assert(agf->agf_roots[XFS_BTNUM_BNOi] !=
		agf->agf_roots[XFS_BTNUM_CNTi]);

	error = bwrite(agf_buf);

#ifdef XR_BLD_FREE_TRACE
	fprintf(stderr, "wrote agf for ag %u, error = %d\n", agno, error);
#endif

	return;
}

/*
 * update the superblock counters, sync the sb version numbers and
 * feature bits to the filesystem, and sync up the on-disk superblock
 * to match the incore superblock.
 */
void
sync_sb(xfs_mount_t *mp)
{
	xfs_sb_t	*sbp;
	buf_t		*bp;

	bp = xfs_getsb(mp, 0);

	if (bp == NULL || geterror(bp))
		do_error("couldn't getsb superblock\n");

	sbp = XFS_BUF_TO_SBP(bp);

	mp->m_sb.sb_icount = sb_icount;
	mp->m_sb.sb_ifree = sb_ifree;
	mp->m_sb.sb_fdblocks = sb_fdblocks;
	mp->m_sb.sb_frextents = sb_frextents;

	update_sb_version(mp);

	*sbp = mp->m_sb;

	bwrite(bp);
}

/*
 * make sure the root and realtime inodes show up allocated
 * even if they've been freed.  they get reinitialized in phase6.
 */
void
keep_fsinos(xfs_mount_t *mp)
{
	ino_tree_node_t		*irec;
	int			i;

	irec = find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino),
			XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rootino));

	for (i = 0; i < 3; i++)
		set_inode_used(irec, i);

	return;
}

void
phase5(xfs_mount_t *mp)
{
	__uint64_t	num_inos;
	__uint64_t	num_free_inos;
	bt_status_t	bno_btree_curs;
	bt_status_t	bcnt_btree_curs;
	bt_status_t	ino_btree_curs;
	xfs_agnumber_t	agno;
	int		extra_blocks = 0;
	uint		num_freeblocks;
	xfs_extlen_t	freeblks1;
	xfs_extlen_t	freeblks2;
	xfs_agblock_t	num_extents;
	extern int	count_bno_extents(xfs_agnumber_t);
	extern int	count_bno_extents_blocks(xfs_agnumber_t, uint *);
#ifdef XR_BLD_FREE_TRACE
	extern int	count_bcnt_extents(xfs_agnumber_t);
#endif

	do_log("Phase 5 - rebuild AG headers and trees...\n");

#ifdef XR_BLD_FREE_TRACE
	fprintf(stderr, "inobt level 1, maxrec = %d, minrec = %d\n",
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_inobt, 0),
		XFS_BTREE_BLOCK_MINRECS(mp->m_sb.sb_blocksize, xfs_inobt, 0)
		);
	fprintf(stderr, "inobt level 0 (leaf), maxrec = %d, minrec = %d\n",
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_inobt, 1),
		XFS_BTREE_BLOCK_MINRECS(mp->m_sb.sb_blocksize, xfs_inobt, 1)
		);
	fprintf(stderr, "xr inobt level 0 (leaf), maxrec = %d\n",
		XR_INOBT_BLOCK_MAXRECS(mp, 0));
	fprintf(stderr, "xr inobt level 1 (int), maxrec = %d\n",
		XR_INOBT_BLOCK_MAXRECS(mp, 1));
	fprintf(stderr, "bnobt level 1, maxrec = %d, minrec = %d\n",
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_alloc, 0),
		XFS_BTREE_BLOCK_MINRECS(mp->m_sb.sb_blocksize, xfs_alloc, 0));
	fprintf(stderr, "bnobt level 0 (leaf), maxrec = %d, minrec = %d\n",
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_alloc, 1),
		XFS_BTREE_BLOCK_MINRECS(mp->m_sb.sb_blocksize, xfs_alloc, 1));
#endif

	/*
	 * make sure the root and realtime inodes show up allocated
	 */
	keep_fsinos(mp);

	for (agno = 0; agno < mp->m_sb.sb_agcount; agno++)  {
		/*
		 * build up incore bno and bcnt extent btrees
		 */
		num_extents = mk_incore_fstree(mp, agno);

#ifdef XR_BLD_FREE_TRACE
		fprintf(stderr, "# of bno extents is %d\n",
				count_bno_extents(agno));
#endif

		if (num_extents == 0)  {
			/*
			 * XXX - what we probably should do here is pick an
			 * inode for a regular file in the allocation group
			 * that has space allocated and shoot it by traversing
			 * the bmap list and putting all its extents on the
			 * incore freespace trees, clearing the inode,
			 * and clearing the in-use bit in the incore inode
			 * tree.  Then try mk_incore_fstree() again.
			 */
			do_error(
	"unable to rebuild AG %u.  Not enough free space in on-disk AG.\n",
				agno);
		}

		/*
		 * done with the AG bitmap, toss it...
		 */
		teardown_ag_bmap(mp, agno);

		/*
		 * ok, now set up the btree cursors for the
		 * on-disk btrees (includs pre-allocating all
		 * required blocks for the trees themselves)
		 */
		init_ino_cursor(mp, agno, &ino_btree_curs,
				&num_inos, &num_free_inos);

		sb_icount += num_inos;
		sb_ifree += num_free_inos;

		num_extents = count_bno_extents_blocks(agno, &num_freeblocks);
		/*
		 * lose two blocks per AG -- the space tree roots
		 * are counted as allocated since the space trees
		 * always have roots
		 */
		sb_fdblocks += num_freeblocks - 2;

		if (num_extents == 0)  {
			/*
			 * XXX - what we probably should do here is pick an
			 * inode for a regular file in the allocation group
			 * that has space allocated and shoot it by traversing
			 * the bmap list and putting all its extents on the
			 * incore freespace trees, clearing the inode,
			 * and clearing the in-use bit in the incore inode
			 * tree.  Then try mk_incore_fstree() again.
			 */
			do_error("unable to rebuild AG %u.  No free space.\n",
				agno);
			exit(1);
		}

#ifdef XR_BLD_FREE_TRACE
		fprintf(stderr, "# of bno extents is %d\n", num_extents);
#endif

		/*
		 * track blocks that we might really lose
		 */
		extra_blocks = calculate_freespace_cursor(mp, agno,
					&num_extents, &bno_btree_curs);

		/*
		 * freespace btrees live in the "free space" but
		 * the filesystem treats AGFL blocks as allocated
		 * since they aren't described by the freespace trees
		 */

		/*
		 * see if we can fit all the extra blocks into the AGFL
		 */
		extra_blocks = (extra_blocks - XFS_AGFL_SIZE > 0)
				? extra_blocks - XFS_AGFL_SIZE
				: 0;

		if (extra_blocks > 0)  {
			do_warn("lost %d blocks in agno %d, sorry.\n",
				extra_blocks, agno);
			sb_fdblocks -= extra_blocks;
		}

		bcnt_btree_curs = bno_btree_curs;

		setup_cursor(mp, agno, &bno_btree_curs);
		setup_cursor(mp, agno, &bcnt_btree_curs);

#ifdef XR_BLD_FREE_TRACE
		fprintf(stderr, "# of bno extents is %d\n",
				count_bno_extents(agno));
		fprintf(stderr, "# of bcnt extents is %d\n",
				count_bcnt_extents(agno));
#endif
		/*
		 * now rebuild the freespace trees
		 */
		freeblks1 = build_freespace_tree(mp, agno, &bno_btree_curs,
					XFS_ABTB_MAGIC);
#ifdef XR_BLD_FREE_TRACE
		fprintf(stderr, "# of free blocks == %d\n", freeblks1);
#endif
		write_cursor(&bno_btree_curs);

		freeblks2 = build_freespace_tree(mp, agno, &bcnt_btree_curs,
					XFS_ABTC_MAGIC);
		write_cursor(&bcnt_btree_curs);

		assert(freeblks1 == freeblks2);

		/*
		 * set up agf and agfl
		 */
		build_agf_agfl(mp, agno, &bno_btree_curs,
				&bcnt_btree_curs, freeblks1, extra_blocks);
		/*
		 * build inode allocation tree.  this also build the agi
		 */
		build_ino_tree(mp, agno, &ino_btree_curs);
		write_cursor(&ino_btree_curs);
		/*
		 * tear down cursors
		 */
		finish_cursor(&bno_btree_curs);
		finish_cursor(&ino_btree_curs);
		finish_cursor(&bcnt_btree_curs);
		/*
		 * release the incore per-AG bno/bcnt trees so
		 * the extent nodes can be recycled
		 */
		release_agbno_extent_tree(agno);
		release_agbcnt_extent_tree(agno);
	}

	if (mp->m_sb.sb_rblocks)  {
		do_log(
		"        - generate realtime summary info and bitmap...\n");
		rtinit(mp);
		generate_rtinfo(mp, btmcompute, sumcompute);
		teardown_rt_bmap(mp);
	}

	do_log("        - reset superblock...\n");

	/*
	 * sync superblock counter and set version bits correctly
	 */
	sync_sb(mp);

	bad_ino_btree = 0;

	return;
}
