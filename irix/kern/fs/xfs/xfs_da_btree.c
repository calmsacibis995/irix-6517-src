/*
 * xfs_da_btree.c
 */

#ifdef SIM
#define _KERNEL 1
#endif
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/debug.h>
#ifdef SIM
#undef _KERNEL
#endif
#include <sys/errno.h>
#include <sys/vnode.h>
#include <sys/kmem.h>
#include <sys/dirent.h>
#include <sys/uuid.h>
#ifdef SIM
#include <bstring.h>
#include <stdio.h>
#include <sys/attributes.h>
#else
#include <sys/systm.h>
#endif
#include "xfs_macros.h"
#include "xfs_types.h"
#include "xfs_inum.h"
#include "xfs_log.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_mount.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_alloc.h"
#include "xfs_bmap.h"
#include "xfs_btree.h"
#include "xfs_attr_sf.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode_item.h"
#include "xfs_inode.h"
#include "xfs_da_btree.h"
#ifndef SIM
#include "xfs_attr.h"
#include "xfs_attr_leaf.h"
#endif
#include "xfs_dir_leaf.h"
#include "xfs_dir2_data.h"
#include "xfs_dir2_leaf.h"
#include "xfs_dir2_block.h"
#include "xfs_dir2_node.h"
#include "xfs_error.h"
#include "xfs_bit.h"
#ifdef SIM
#include "sim.h"
#endif

/*
 * xfs_da_btree.c
 *
 * Routines to implement directories as Btrees of hashed names.
 */

/*========================================================================
 * Function prototypes for the kernel.
 *========================================================================*/

/*
 * Routines used for growing the Btree.
 */
STATIC int xfs_da_root_split(xfs_da_state_t *state,
					    xfs_da_state_blk_t *existing_root,
					    xfs_da_state_blk_t *new_child);
STATIC int xfs_da_node_split(xfs_da_state_t *state,
					    xfs_da_state_blk_t *existing_blk,
					    xfs_da_state_blk_t *split_blk,
					    xfs_da_state_blk_t *blk_to_add,
					    int treelevel,
					    int *result);
STATIC void xfs_da_node_rebalance(xfs_da_state_t *state,
					 xfs_da_state_blk_t *node_blk_1,
					 xfs_da_state_blk_t *node_blk_2);
STATIC void xfs_da_node_add(xfs_da_state_t *state,
				   xfs_da_state_blk_t *old_node_blk,
				   xfs_da_state_blk_t *new_node_blk);

/*
 * Routines used for shrinking the Btree.
 */
#if defined(XFS_REPAIR_SIM) || !defined(SIM)
STATIC int xfs_da_root_join(xfs_da_state_t *state,
					   xfs_da_state_blk_t *root_blk);
STATIC int xfs_da_node_toosmall(xfs_da_state_t *state, int *retval);
STATIC void xfs_da_node_remove(xfs_da_state_t *state,
					      xfs_da_state_blk_t *drop_blk);
STATIC void xfs_da_node_unbalance(xfs_da_state_t *state,
					 xfs_da_state_blk_t *src_node_blk,
					 xfs_da_state_blk_t *dst_node_blk);
#endif /* XFS_REPAIR_SIM || !SIM */

/*
 * Utility routines.
 */
STATIC uint	xfs_da_node_lasthash(xfs_dabuf_t *bp, int *count);
STATIC int	xfs_da_node_order(xfs_dabuf_t *node1_bp, xfs_dabuf_t *node2_bp);
STATIC xfs_dabuf_t *xfs_da_buf_make(int nbuf, buf_t **bps, inst_t *ra);


/*========================================================================
 * Routines used for growing the Btree.
 *========================================================================*/

/*
 * Create the initial contents of an intermediate node.
 */
int
xfs_da_node_create(xfs_da_args_t *args, xfs_dablk_t blkno, int level,
				 xfs_dabuf_t **bpp, int whichfork)
{
	xfs_da_intnode_t *node;
	xfs_dabuf_t *bp;
	int error;
	xfs_trans_t *tp;

	tp = args->trans;
	error = xfs_da_get_buf(tp, args->dp, blkno, -1, &bp, whichfork);
	if (error)
		return(error);
	ASSERT(bp != NULL);
	node = bp->data;
	node->hdr.info.forw = node->hdr.info.back = 0;
	node->hdr.info.magic = XFS_DA_NODE_MAGIC;
	node->hdr.info.pad = 0;
	node->hdr.count = 0;
	node->hdr.level = level;

	xfs_da_log_buf(tp, bp,
		XFS_DA_LOGRANGE(node, &node->hdr, sizeof(node->hdr)));

	*bpp = bp;
	return(0);
}

/*
 * Split a leaf node, rebalance, then possibly split
 * intermediate nodes, rebalance, etc.
 */
int							/* error */
xfs_da_split(xfs_da_state_t *state)
{
	xfs_da_state_blk_t *oldblk, *newblk, *addblk;
	xfs_da_intnode_t *node;
	xfs_dabuf_t *bp;
	int max, action, error, i;

	/*
	 * Walk back up the tree splitting/inserting/adjusting as necessary.
	 * If we need to insert and there isn't room, split the node, then
	 * decide which fragment to insert the new block from below into.
	 * Note that we may split the root this way, but we need more fixup.
	 */
	max = state->path.active - 1;
	ASSERT((max >= 0) && (max < XFS_DA_NODE_MAXDEPTH));
	ASSERT(state->path.blk[max].magic == XFS_ATTR_LEAF_MAGIC ||
	       state->path.blk[max].magic == XFS_DIRX_LEAF_MAGIC(state->mp));

	addblk = &state->path.blk[max];		/* initial dummy value */
	for (i = max; (i >= 0) && addblk; state->path.active--, i--) {
		oldblk = &state->path.blk[i];
		newblk = &state->altpath.blk[i];

		/*
		 * If a leaf node then
		 *     Allocate a new leaf node, then rebalance across them.
		 * else if an intermediate node then
		 *     We split on the last layer, must we split the node?
		 */
		switch (oldblk->magic) {
		case XFS_ATTR_LEAF_MAGIC:
#ifdef SIM
			return(ENOTTY);
#else
			error = xfs_attr_leaf_split(state, oldblk, newblk);
			if ((error != 0) && (error != ENOSPC)) {
				return(error);	/* GROT: attr is inconsistent */
			}
			if (!error) {
				addblk = newblk;
				break;
			}
			/*
			 * Entry wouldn't fit, split the leaf again.
			 */
			state->extravalid = 1;
			if (state->inleaf) {
				state->extraafter = 0;	/* before newblk */
				error = xfs_attr_leaf_split(state, oldblk,
							    &state->extrablk);
			} else {
				state->extraafter = 1;	/* after newblk */
				error = xfs_attr_leaf_split(state, newblk,
							    &state->extrablk);
			}
			if (error)
				return(error);	/* GROT: attr inconsistent */
			addblk = newblk;
			break;
#endif
		case XFS_DIR_LEAF_MAGIC:
			ASSERT(XFS_DIR_IS_V1(state->mp));
			error = xfs_dir_leaf_split(state, oldblk, newblk);
			if ((error != 0) && (error != ENOSPC)) {
				return(error);	/* GROT: dir is inconsistent */
			}
			if (!error) {
				addblk = newblk;
				break;
			}
			/*
			 * Entry wouldn't fit, split the leaf again.
			 */
			state->extravalid = 1;
			if (state->inleaf) {
				state->extraafter = 0;	/* before newblk */
				error = xfs_dir_leaf_split(state, oldblk,
							   &state->extrablk);
				if (error)
					return(error);	/* GROT: dir incon. */
				addblk = newblk;
			} else {
				state->extraafter = 1;	/* after newblk */
				error = xfs_dir_leaf_split(state, newblk,
							   &state->extrablk);
				if (error)
					return(error);	/* GROT: dir incon. */
				addblk = newblk;
			}
			break;
		case XFS_DIR2_LEAFN_MAGIC:
			ASSERT(XFS_DIR_IS_V2(state->mp));
			error = xfs_dir2_leafn_split(state, oldblk, newblk);
			if (error)
				return error;
			addblk = newblk;
			break;
		case XFS_DA_NODE_MAGIC:
			error = xfs_da_node_split(state, oldblk, newblk, addblk,
							 max - i, &action);
			xfs_da_buf_done(addblk->bp);
			addblk->bp = NULL;
			if (error)
				return(error);	/* GROT: dir is inconsistent */
			/*
			 * Record the newly split block for the next time thru?
			 */
			if (action)
				addblk = newblk;
			else
				addblk = NULL;
			break;
		}

		/*
		 * Update the btree to show the new hashval for this child.
		 */
		xfs_da_fixhashpath(state, &state->path);
		/*
		 * If we won't need this block again, it's getting dropped
		 * from the active path by the loop control, so we need
		 * to mark it done now.
		 */
		if (i > 0 || !addblk)
			xfs_da_buf_done(oldblk->bp);
	}
	if (!addblk)
		return(0);

	/*
	 * Split the root node.
	 */
	ASSERT(state->path.active == 0);
	oldblk = &state->path.blk[0];
	error = xfs_da_root_split(state, oldblk, addblk);
	if (error) {
		xfs_da_buf_done(oldblk->bp);
		xfs_da_buf_done(addblk->bp);
		addblk->bp = NULL;
		return(error);	/* GROT: dir is inconsistent */
	}

	/*
	 * Update pointers to the node which used to be block 0 and
	 * just got bumped because of the addition of a new root node.
	 * There might be three blocks involved if a double split occurred,
	 * and the original block 0 could be at any position in the list.
	 */

	node = oldblk->bp->data;
	if (node->hdr.info.forw) {
		if (node->hdr.info.forw == addblk->blkno) {
			bp = addblk->bp;
		} else {
			ASSERT(state->extravalid);
			bp = state->extrablk.bp;
		}
		node = bp->data;
		node->hdr.info.back = oldblk->blkno;
		xfs_da_log_buf(state->args->trans, bp,
		    XFS_DA_LOGRANGE(node, &node->hdr.info,
		    sizeof(node->hdr.info)));
	}
	node = oldblk->bp->data;
	if (node->hdr.info.back) {
		if (node->hdr.info.back == addblk->blkno) {
			bp = addblk->bp;
		} else {
			ASSERT(state->extravalid);
			bp = state->extrablk.bp;
		}
		node = bp->data;
		node->hdr.info.forw = oldblk->blkno;
		xfs_da_log_buf(state->args->trans, bp,
		    XFS_DA_LOGRANGE(node, &node->hdr.info,
		    sizeof(node->hdr.info)));
	}
	xfs_da_buf_done(oldblk->bp);
	xfs_da_buf_done(addblk->bp);
	addblk->bp = NULL;
	return(0);
}

/*
 * Split the root.  We have to create a new root and point to the two
 * parts (the split old root) that we just created.  Copy block zero to
 * the EOF, extending the inode in process.
 */
STATIC int						/* error */
xfs_da_root_split(xfs_da_state_t *state, xfs_da_state_blk_t *blk1,
				 xfs_da_state_blk_t *blk2)
{
	xfs_da_intnode_t *node, *oldroot;
	xfs_da_args_t *args;
	xfs_dablk_t blkno;
	xfs_dabuf_t *bp;
	int error, size;
	xfs_inode_t *dp;
	xfs_trans_t *tp;
	xfs_mount_t *mp;
	xfs_dir2_leaf_t *leaf;

	/*
	 * Copy the existing (incorrect) block from the root node position
	 * to a free space somewhere.
	 */
	args = state->args;
	ASSERT(args != NULL);
	error = xfs_da_grow_inode(args, &blkno);
	if (error)
		return(error);
	dp = args->dp;
	tp = args->trans;
	mp = state->mp;
	error = xfs_da_get_buf(tp, dp, blkno, -1, &bp, args->whichfork);
	if (error)
		return(error);
	ASSERT(bp != NULL);
	node = bp->data;
	oldroot = blk1->bp->data;
	if (oldroot->hdr.info.magic == XFS_DA_NODE_MAGIC) {
		size = (int)((char *)&oldroot->btree[oldroot->hdr.count] -
			     (char *)oldroot);
	} else {
		ASSERT(XFS_DIR_IS_V2(mp));
		ASSERT(oldroot->hdr.info.magic == XFS_DIR2_LEAFN_MAGIC);
		leaf = (xfs_dir2_leaf_t *)oldroot;
		size = (int)((char *)&leaf->ents[leaf->hdr.count] -
			     (char *)leaf);
	}
	bcopy(oldroot, node, size);
	xfs_da_log_buf(tp, bp, 0, size - 1);
	xfs_da_buf_done(blk1->bp);
	blk1->bp = bp;
	blk1->blkno = blkno;

	/*
	 * Set up the new root node.
	 */
	error = xfs_da_node_create(args,
		args->whichfork == XFS_DATA_FORK &&
		XFS_DIR_IS_V2(mp) ? mp->m_dirleafblk : 0,
		node->hdr.level + 1, &bp, args->whichfork);
	if (error)
		return(error);
	node = bp->data;
	node->btree[0].hashval = blk1->hashval;
	node->btree[0].before = blk1->blkno;
	node->btree[1].hashval = blk2->hashval;
	node->btree[1].before = blk2->blkno;
	node->hdr.count = 2;
	if (XFS_DIR_IS_V2(mp)) {
		ASSERT(blk1->blkno >= mp->m_dirleafblk &&
		       blk1->blkno < mp->m_dirfreeblk);
		ASSERT(blk2->blkno >= mp->m_dirleafblk &&
		       blk2->blkno < mp->m_dirfreeblk);
	}
	/* Header is already logged by xfs_da_node_create */
	xfs_da_log_buf(tp, bp,
		XFS_DA_LOGRANGE(node, node->btree,
			sizeof(xfs_da_node_entry_t) * 2));
	xfs_da_buf_done(bp);

	return(0);
}

/*
 * Split the node, rebalance, then add the new entry.
 */
STATIC int						/* error */
xfs_da_node_split(xfs_da_state_t *state, xfs_da_state_blk_t *oldblk,
				 xfs_da_state_blk_t *newblk,
				 xfs_da_state_blk_t *addblk,
				 int treelevel, int *result)
{
	xfs_da_intnode_t *node;
	xfs_dablk_t blkno;
	int newcount, error;
	int useextra;

	node = oldblk->bp->data;
	ASSERT(node->hdr.info.magic == XFS_DA_NODE_MAGIC);

	/*
	 * With V2 the extra block is data or freespace.
	 */
	useextra = state->extravalid && XFS_DIR_IS_V1(state->mp);
	newcount = 1 + useextra;
	/*
	 * Do we have to split the node?
	 */
	if ((node->hdr.count + newcount) > XFS_DA_NODE_ENTRIES(state->mp)) {
		/*
		 * Allocate a new node, add to the doubly linked chain of
		 * nodes, then move some of our excess entries into it.
		 */
		error = xfs_da_grow_inode(state->args, &blkno);
		if (error)
			return(error);	/* GROT: dir is inconsistent */
		
		error = xfs_da_node_create(state->args, blkno, treelevel,
					   &newblk->bp, state->args->whichfork);
		if (error)
			return(error);	/* GROT: dir is inconsistent */
		newblk->blkno = blkno;
		newblk->magic = XFS_DA_NODE_MAGIC;
		xfs_da_node_rebalance(state, oldblk, newblk);
		error = xfs_da_blk_link(state, oldblk, newblk);
		if (error)
			return(error);
		*result = 1;
	} else {
		*result = 0;
	}

	/*
	 * Insert the new entry(s) into the correct block
	 * (updating last hashval in the process).
	 *
	 * xfs_da_node_add() inserts BEFORE the given index,
	 * and as a result of using node_lookup_int() we always
	 * point to a valid entry (not after one), but a split
	 * operation always results in a new block whose hashvals
	 * FOLLOW the current block.
	 *
	 * If we had double-split op below us, then add the extra block too.
	 */
	node = oldblk->bp->data;
	if (oldblk->index <= node->hdr.count) {
		oldblk->index++;
		xfs_da_node_add(state, oldblk, addblk);
		if (useextra) {
			if (state->extraafter)
				oldblk->index++;
			xfs_da_node_add(state, oldblk, &state->extrablk);
			state->extravalid = 0;
		}
	} else {
		newblk->index++;
		xfs_da_node_add(state, newblk, addblk);
		if (useextra) {
			if (state->extraafter)
				newblk->index++;
			xfs_da_node_add(state, newblk, &state->extrablk);
			state->extravalid = 0;
		}
	}

	return(0);
}

/*
 * Balance the btree elements between two intermediate nodes,
 * usually one full and one empty.
 *
 * NOTE: if blk2 is empty, then it will get the upper half of blk1.
 */
STATIC void
xfs_da_node_rebalance(xfs_da_state_t *state, xfs_da_state_blk_t *blk1,
				     xfs_da_state_blk_t *blk2)
{
	xfs_da_intnode_t *node1, *node2, *tmpnode;
	xfs_da_node_entry_t *btree_s, *btree_d;
	int count, tmp;
	xfs_trans_t *tp;

	node1 = blk1->bp->data;
	node2 = blk2->bp->data;
	/*
	 * Figure out how many entries need to move, and in which direction.
	 * Swap the nodes around if that makes it simpler.
	 */
	if ((node1->hdr.count > 0) && (node2->hdr.count > 0) &&
	    ((node2->btree[ 0 ].hashval < node1->btree[ 0 ].hashval) ||
	     (node2->btree[ node2->hdr.count-1 ].hashval <
	      node1->btree[ node1->hdr.count-1 ].hashval))) {
		tmpnode = node1;
		node1 = node2;
		node2 = tmpnode;
	}
	ASSERT(node1->hdr.info.magic == XFS_DA_NODE_MAGIC);
	ASSERT(node2->hdr.info.magic == XFS_DA_NODE_MAGIC);
	count = (node1->hdr.count - node2->hdr.count) / 2;
	if (count == 0)
		return;
	tp = state->args->trans;
	/*
	 * Two cases: high-to-low and low-to-high.
	 */
	if (count > 0) {
		/*
		 * Move elements in node2 up to make a hole.
		 */
		if ((tmp = node2->hdr.count) > 0) {
			tmp *= (uint)sizeof(xfs_da_node_entry_t);
			btree_s = &node2->btree[0];
			btree_d = &node2->btree[count];
			ovbcopy(btree_s, btree_d, tmp);
		}

		/*
		 * Move the req'd B-tree elements from high in node1 to
		 * low in node2.
		 */
		node2->hdr.count += count;
		tmp = count * (uint)sizeof(xfs_da_node_entry_t);
		btree_s = &node1->btree[node1->hdr.count - count];
		btree_d = &node2->btree[0];
		bcopy(btree_s, btree_d, tmp);
		node1->hdr.count -= count;

	} else {
		/*
		 * Move the req'd B-tree elements from low in node2 to
		 * high in node1.
		 */
		count = -count;
		tmp = count * (uint)sizeof(xfs_da_node_entry_t);
		btree_s = &node2->btree[0];
		btree_d = &node1->btree[node1->hdr.count];
		bcopy(btree_s, btree_d, tmp);
		node1->hdr.count += count;
		xfs_da_log_buf(tp, blk1->bp,
			XFS_DA_LOGRANGE(node1, btree_d, tmp));

		/*
		 * Move elements in node2 down to fill the hole.
		 */
		tmp  = node2->hdr.count - count;
		tmp *= (uint)sizeof(xfs_da_node_entry_t);
		btree_s = &node2->btree[count];
		btree_d = &node2->btree[0];
		ovbcopy(btree_s, btree_d, tmp);
		node2->hdr.count -= count;
	}

	/*
	 * Log header of node 1 and all current bits of node 2.
	 */
	xfs_da_log_buf(tp, blk1->bp,
		XFS_DA_LOGRANGE(node1, &node1->hdr, sizeof(node1->hdr)));
	xfs_da_log_buf(tp, blk2->bp,
		XFS_DA_LOGRANGE(node2, &node2->hdr,
			sizeof(node2->hdr) +
			sizeof(node2->btree[0]) * node2->hdr.count));

	/*
	 * Record the last hashval from each block for upward propagation.
	 * (note: don't use the swapped node pointers)
	 */
	node1 = blk1->bp->data;
	node2 = blk2->bp->data;
	blk1->hashval = node1->btree[ node1->hdr.count-1 ].hashval;
	blk2->hashval = node2->btree[ node2->hdr.count-1 ].hashval;

	/*
	 * Adjust the expected index for insertion.
	 */
	if (blk1->index >= node1->hdr.count) {
		blk2->index = blk1->index - node1->hdr.count;
		blk1->index = node1->hdr.count + 1;	/* make it invalid */
	}
}

/*
 * Add a new entry to an intermediate node.
 */
STATIC void
xfs_da_node_add(xfs_da_state_t *state, xfs_da_state_blk_t *oldblk,
			       xfs_da_state_blk_t *newblk)
{
	xfs_da_intnode_t *node;
	xfs_da_node_entry_t *btree;
	int tmp;
	xfs_mount_t *mp;

	node = oldblk->bp->data;
	mp = state->mp;
	ASSERT(node->hdr.info.magic == XFS_DA_NODE_MAGIC);
	ASSERT((oldblk->index >= 0) && (oldblk->index <= node->hdr.count));
	ASSERT(newblk->blkno != 0);
	if (state->args->whichfork == XFS_DATA_FORK && XFS_DIR_IS_V2(mp))
		ASSERT(newblk->blkno >= mp->m_dirleafblk &&
		       newblk->blkno < mp->m_dirfreeblk);

	/*
	 * We may need to make some room before we insert the new node.
	 */
	tmp = 0;
	btree = &node->btree[ oldblk->index ];
	if (oldblk->index < node->hdr.count) {
		tmp = (node->hdr.count - oldblk->index) * (uint)sizeof(*btree);
		ovbcopy(btree, btree + 1, tmp);
	}
	btree->hashval = newblk->hashval;
	btree->before = newblk->blkno;
	xfs_da_log_buf(state->args->trans, oldblk->bp,
		XFS_DA_LOGRANGE(node, btree, tmp + sizeof(*btree)));
	node->hdr.count++;
	xfs_da_log_buf(state->args->trans, oldblk->bp,
		XFS_DA_LOGRANGE(node, &node->hdr, sizeof(node->hdr)));

	/*
	 * Copy the last hash value from the oldblk to propagate upwards.
	 */
	oldblk->hashval = node->btree[ node->hdr.count-1 ].hashval;
}

/*========================================================================
 * Routines used for shrinking the Btree.
 *========================================================================*/

#if defined(XFS_REPAIR_SIM) || !defined(SIM)
/*
 * Deallocate an empty leaf node, remove it from its parent,
 * possibly deallocating that block, etc...
 */
int
xfs_da_join(xfs_da_state_t *state)
{
	xfs_da_state_blk_t *drop_blk, *save_blk;
	int action, error;

	action = 0;
	drop_blk = &state->path.blk[ state->path.active-1 ];
	save_blk = &state->altpath.blk[ state->path.active-1 ];
	ASSERT(state->path.blk[0].magic == XFS_DA_NODE_MAGIC);
	ASSERT(drop_blk->magic == XFS_ATTR_LEAF_MAGIC ||
	       drop_blk->magic == XFS_DIRX_LEAF_MAGIC(state->mp));

	/*
	 * Walk back up the tree joining/deallocating as necessary.
	 * When we stop dropping blocks, break out.
	 */
	for (  ; state->path.active >= 2; drop_blk--, save_blk--,
		 state->path.active--) {
		/*
		 * See if we can combine the block with a neighbor.
		 *   (action == 0) => no options, just leave
		 *   (action == 1) => coalesce, then unlink
		 *   (action == 2) => block empty, unlink it
		 */
		switch (drop_blk->magic) {
		case XFS_ATTR_LEAF_MAGIC:
#ifdef SIM
			error = ENOTTY;
#else
			error = xfs_attr_leaf_toosmall(state, &action);
#endif
			if (error)
				return(error);
			if (action == 0)
				return(0);
#ifndef SIM
			xfs_attr_leaf_unbalance(state, drop_blk, save_blk);
#endif
			break;
		case XFS_DIR_LEAF_MAGIC:
			ASSERT(XFS_DIR_IS_V1(state->mp));
			error = xfs_dir_leaf_toosmall(state, &action);
			if (error)
				return(error);
			if (action == 0)
				return(0);
			xfs_dir_leaf_unbalance(state, drop_blk, save_blk);
			break;
		case XFS_DIR2_LEAFN_MAGIC:
			ASSERT(XFS_DIR_IS_V2(state->mp));
			error = xfs_dir2_leafn_toosmall(state, &action);
			if (error)
				return error;
			if (action == 0)
				return 0;
			xfs_dir2_leafn_unbalance(state, drop_blk, save_blk);
			break;
		case XFS_DA_NODE_MAGIC:
			/*
			 * Remove the offending node, fixup hashvals,
			 * check for a toosmall neighbor.
			 */
			xfs_da_node_remove(state, drop_blk);
			xfs_da_fixhashpath(state, &state->path);
			error = xfs_da_node_toosmall(state, &action);
			if (error)
				return(error);
			if (action == 0)
				return 0;
			xfs_da_node_unbalance(state, drop_blk, save_blk);
			break;
		}
		xfs_da_fixhashpath(state, &state->altpath);
		error = xfs_da_blk_unlink(state, drop_blk, save_blk);
		xfs_da_state_kill_altpath(state);
		if (error)
			return(error);
		error = xfs_da_shrink_inode(state->args, drop_blk->blkno,
							 drop_blk->bp);
		drop_blk->bp = NULL;
		if (error)
			return(error);
	}
	/*
	 * We joined all the way to the top.  If it turns out that
	 * we only have one entry in the root, make the child block
	 * the new root.
	 */
	xfs_da_node_remove(state, drop_blk);
	xfs_da_fixhashpath(state, &state->path);
	error = xfs_da_root_join(state, &state->path.blk[0]);
	return(error);
}

/*
 * We have only one entry in the root.  Copy the only remaining child of
 * the old root to block 0 as the new root node.
 */
STATIC int
xfs_da_root_join(xfs_da_state_t *state, xfs_da_state_blk_t *root_blk)
{
	xfs_da_intnode_t *oldroot;
	/* REFERENCED */
	xfs_da_blkinfo_t *blkinfo;
	xfs_da_args_t *args;
	xfs_dablk_t child;
	xfs_dabuf_t *bp;
	int error;

	args = state->args;
	ASSERT(args != NULL);
	ASSERT(root_blk->magic == XFS_DA_NODE_MAGIC);
	oldroot = root_blk->bp->data;
	ASSERT(oldroot->hdr.info.magic == XFS_DA_NODE_MAGIC);
	ASSERT(oldroot->hdr.info.forw == 0);
	ASSERT(oldroot->hdr.info.back == 0);

	/*
	 * If the root has more than one child, then don't do anything.
	 */
	if (oldroot->hdr.count > 1)
		return(0);

	/*
	 * Read in the (only) child block, then copy those bytes into
	 * the root block's buffer and free the original child block.
	 */
	child = oldroot->btree[ 0 ].before;
	ASSERT(child != 0);
	error = xfs_da_read_buf(args->trans, args->dp, child, -1, &bp,
					     args->whichfork);
	if (error)
		return(error);
	ASSERT(bp != NULL);
	blkinfo = bp->data;
	if (oldroot->hdr.level == 1) {
		ASSERT(blkinfo->magic == XFS_DIRX_LEAF_MAGIC(state->mp) ||
		       blkinfo->magic == XFS_ATTR_LEAF_MAGIC);
	} else {
		ASSERT(blkinfo->magic == XFS_DA_NODE_MAGIC);
	}
	ASSERT(blkinfo->forw == 0);
	ASSERT(blkinfo->back == 0);
	bcopy(bp->data, root_blk->bp->data, state->blocksize);
	xfs_da_log_buf(args->trans, root_blk->bp, 0, state->blocksize - 1);
	error = xfs_da_shrink_inode(args, child, bp);
	return(error);
}

/*
 * Check a node block and its neighbors to see if the block should be
 * collapsed into one or the other neighbor.  Always keep the block
 * with the smaller block number.
 * If the current block is over 50% full, don't try to join it, return 0.
 * If the block is empty, fill in the state structure and return 2.
 * If it can be collapsed, fill in the state structure and return 1.
 * If nothing can be done, return 0.
 */
STATIC int
xfs_da_node_toosmall(xfs_da_state_t *state, int *action)
{
	xfs_da_intnode_t *node;
	xfs_da_state_blk_t *blk;
	xfs_da_blkinfo_t *info;
	int count, forward, error, retval, i;
	xfs_dablk_t blkno;
	xfs_dabuf_t *bp;

	/*
	 * Check for the degenerate case of the block being over 50% full.
	 * If so, it's not worth even looking to see if we might be able
	 * to coalesce with a sibling.
	 */
	blk = &state->path.blk[ state->path.active-1 ];
	info = blk->bp->data;
	ASSERT(info->magic == XFS_DA_NODE_MAGIC);
	node = (xfs_da_intnode_t *)info;
	count = node->hdr.count;
	if (count > (XFS_DA_NODE_ENTRIES(state->mp) >> 1)) {
		*action = 0;	/* blk over 50%, dont try to join */
		return(0);	/* blk over 50%, dont try to join */
	}

	/*
	 * Check for the degenerate case of the block being empty.
	 * If the block is empty, we'll simply delete it, no need to
	 * coalesce it with a sibling block.  We choose (aribtrarily)
	 * to merge with the forward block unless it is NULL.
	 */
	if (count == 0) {
		/*
		 * Make altpath point to the block we want to keep and
		 * path point to the block we want to drop (this one).
		 */
		forward = (info->forw != 0);
		bcopy(&state->path, &state->altpath, sizeof(state->path));
		error = xfs_da_path_shift(state, &state->altpath, forward,
						 0, &retval);
		if (error)
			return(error);
		if (retval) {
			*action = 0;
		} else {
			*action = 2;
		}
		return(0);
	}

	/*
	 * Examine each sibling block to see if we can coalesce with
	 * at least 25% free space to spare.  We need to figure out
	 * whether to merge with the forward or the backward block.
	 * We prefer coalescing with the lower numbered sibling so as
	 * to shrink a directory over time.
	 */
	forward = (info->forw < info->back);	/* start with smaller blk num */
	for (i = 0; i < 2; forward = !forward, i++) {
		if (forward)
			blkno = info->forw;
		else
			blkno = info->back;
		if (blkno == 0)
			continue;
		error = xfs_da_read_buf(state->args->trans, state->args->dp,
					blkno, -1, &bp, state->args->whichfork);
		if (error)
			return(error);
		ASSERT(bp != NULL);

		node = (xfs_da_intnode_t *)info;
		count  = XFS_DA_NODE_ENTRIES(state->mp);
		count -= XFS_DA_NODE_ENTRIES(state->mp) >> 2;
		count -= node->hdr.count;
		node = bp->data;
		ASSERT(node->hdr.info.magic == XFS_DA_NODE_MAGIC);
		count -= node->hdr.count;
		xfs_da_brelse(state->args->trans, bp);
		if (count >= 0)
			break;	/* fits with at least 25% to spare */
	}
	if (i >= 2) {
		*action = 0;
		return(0);
	}

	/*
	 * Make altpath point to the block we want to keep (the lower
	 * numbered block) and path point to the block we want to drop.
	 */
	bcopy(&state->path, &state->altpath, sizeof(state->path));
	if (blkno < blk->blkno) {
		error = xfs_da_path_shift(state, &state->altpath, forward,
						 0, &retval);
		if (error) {
			return(error);
		}
		if (retval) {
			*action = 0;
			return(0);
		}
	} else {
		error = xfs_da_path_shift(state, &state->path, forward,
						 0, &retval);
		if (error) {
			return(error);
		}
		if (retval) {
			*action = 0;
			return(0);
		}
	}
	*action = 1;
	return(0);
}
#endif /* XFS_REPAIR_SIM || !SIM */

/*
 * Walk back up the tree adjusting hash values as necessary,
 * when we stop making changes, return.
 */
void
xfs_da_fixhashpath(xfs_da_state_t *state, xfs_da_state_path_t *path)
{
	xfs_da_state_blk_t *blk;
	xfs_da_intnode_t *node;
	xfs_da_node_entry_t *btree;
	xfs_dahash_t lasthash;
	int level, count;

	level = path->active-1;
	blk = &path->blk[ level ];
	switch (blk->magic) {
#ifndef SIM
	case XFS_ATTR_LEAF_MAGIC:
		lasthash = xfs_attr_leaf_lasthash(blk->bp, &count);
		if (count == 0)
			return;
		break;
#endif
	case XFS_DIR_LEAF_MAGIC:
		ASSERT(XFS_DIR_IS_V1(state->mp));
		lasthash = xfs_dir_leaf_lasthash(blk->bp, &count);
		if (count == 0)
			return;
		break;
	case XFS_DIR2_LEAFN_MAGIC:
		ASSERT(XFS_DIR_IS_V2(state->mp));
		lasthash = xfs_dir2_leafn_lasthash(blk->bp, &count);
		if (count == 0)
			return;
		break;
	case XFS_DA_NODE_MAGIC:
		lasthash = xfs_da_node_lasthash(blk->bp, &count);
		if (count == 0)
			return;
		break;
	}
	for (blk--, level--; level >= 0; blk--, level--) {
		node = blk->bp->data;
		ASSERT(node->hdr.info.magic == XFS_DA_NODE_MAGIC);
		btree = &node->btree[ blk->index ];
		if (btree->hashval == lasthash)
			break;
		blk->hashval = btree->hashval = lasthash;
		xfs_da_log_buf(state->args->trans, blk->bp,
				  XFS_DA_LOGRANGE(node, btree, sizeof(*btree)));

		lasthash = node->btree[ node->hdr.count-1 ].hashval;
	}
}

#if defined(XFS_REPAIR_SIM) || !defined(SIM)
/*
 * Remove an entry from an intermediate node.
 */
STATIC void
xfs_da_node_remove(xfs_da_state_t *state, xfs_da_state_blk_t *drop_blk)
{
	xfs_da_intnode_t *node;
	xfs_da_node_entry_t *btree;
	int tmp;

	node = drop_blk->bp->data;
	ASSERT(drop_blk->index < node->hdr.count);
	ASSERT(drop_blk->index >= 0);

	/*
	 * Copy over the offending entry, or just zero it out.
	 */
	btree = &node->btree[drop_blk->index];
	if (drop_blk->index < (node->hdr.count-1)) {
		tmp  = node->hdr.count - drop_blk->index - 1;
		tmp *= (uint)sizeof(xfs_da_node_entry_t);
		ovbcopy(btree + 1, btree, tmp);
		xfs_da_log_buf(state->args->trans, drop_blk->bp,
		    XFS_DA_LOGRANGE(node, btree, tmp));
		btree = &node->btree[ node->hdr.count-1 ];
	}
	bzero((char *)btree, sizeof(xfs_da_node_entry_t));
	xfs_da_log_buf(state->args->trans, drop_blk->bp,
	    XFS_DA_LOGRANGE(node, btree, sizeof(*btree)));
	node->hdr.count--;
	xfs_da_log_buf(state->args->trans, drop_blk->bp,
	    XFS_DA_LOGRANGE(node, &node->hdr, sizeof(node->hdr)));

	/*
	 * Copy the last hash value from the block to propagate upwards.
	 */
	btree--;
	drop_blk->hashval = btree->hashval;
}

/*
 * Unbalance the btree elements between two intermediate nodes,
 * move all Btree elements from one node into another.
 */
STATIC void
xfs_da_node_unbalance(xfs_da_state_t *state, xfs_da_state_blk_t *drop_blk,
				     xfs_da_state_blk_t *save_blk)
{
	xfs_da_intnode_t *drop_node, *save_node;
	xfs_da_node_entry_t *btree;
	int tmp;
	xfs_trans_t *tp;

	drop_node = drop_blk->bp->data;
	save_node = save_blk->bp->data;
	ASSERT(drop_node->hdr.info.magic == XFS_DA_NODE_MAGIC);
	ASSERT(save_node->hdr.info.magic == XFS_DA_NODE_MAGIC);
	tp = state->args->trans;

	/*
	 * If the dying block has lower hashvals, then move all the
	 * elements in the remaining block up to make a hole.
	 */
	if ((drop_node->btree[ 0 ].hashval < save_node->btree[ 0 ].hashval) ||
	    (drop_node->btree[ drop_node->hdr.count-1 ].hashval <
	     save_node->btree[ save_node->hdr.count-1 ].hashval))
	{
		btree = &save_node->btree[ drop_node->hdr.count ];
		tmp = save_node->hdr.count * (uint)sizeof(xfs_da_node_entry_t);
		ovbcopy(&save_node->btree[0], btree, tmp);
		btree = &save_node->btree[0];
		xfs_da_log_buf(tp, save_blk->bp,
			XFS_DA_LOGRANGE(save_node, btree,
				(save_node->hdr.count + drop_node->hdr.count) *
				sizeof(xfs_da_node_entry_t)));
	} else {
		btree = &save_node->btree[ save_node->hdr.count ];
		xfs_da_log_buf(tp, save_blk->bp,
			XFS_DA_LOGRANGE(save_node, btree,
				drop_node->hdr.count *
				sizeof(xfs_da_node_entry_t)));
	}

	/*
	 * Move all the B-tree elements from drop_blk to save_blk.
	 */
	tmp = drop_node->hdr.count * (uint)sizeof(xfs_da_node_entry_t);
	bcopy(&drop_node->btree[0], btree, tmp);
	save_node->hdr.count += drop_node->hdr.count;

	xfs_da_log_buf(tp, save_blk->bp,
		XFS_DA_LOGRANGE(save_node, &save_node->hdr,
			sizeof(save_node->hdr)));

	/*
	 * Save the last hashval in the remaining block for upward propagation.
	 */
	save_blk->hashval = save_node->btree[ save_node->hdr.count-1 ].hashval;
}
#endif /* XFS_REPAIR_SIM || !SIM */

/*========================================================================
 * Routines used for finding things in the Btree.
 *========================================================================*/

/*
 * Walk down the Btree looking for a particular filename, filling
 * in the state structure as we go.
 *
 * We will set the state structure to point to each of the elements
 * in each of the nodes where either the hashval is or should be.
 *
 * We support duplicate hashval's so for each entry in the current
 * node that could contain the desired hashval, descend.  This is a
 * pruned depth-first tree search.
 */
int							/* error */
xfs_da_node_lookup_int(xfs_da_state_t *state, int *result)
{
	xfs_da_state_blk_t *blk;
	xfs_da_blkinfo_t *current;
	xfs_da_intnode_t *node;
	xfs_da_node_entry_t *btree;
	xfs_dablk_t blkno;
	int probe, span, max, error, retval;
	xfs_dahash_t hashval;
	xfs_da_args_t *args;

	args = state->args;
	/*
	 * Descend thru the B-tree searching each level for the right
	 * node to use, until the right hashval is found.
	 */
	if (args->whichfork == XFS_DATA_FORK && XFS_DIR_IS_V2(state->mp))
		blkno = state->mp->m_dirleafblk;
	else
		blkno = 0;
	for (blk = &state->path.blk[0], state->path.active = 1;
			 state->path.active <= XFS_DA_NODE_MAXDEPTH;
			 blk++, state->path.active++) {
		/*
		 * Read the next node down in the tree.
		 */
		blk->blkno = blkno;
		error = xfs_da_read_buf(state->args->trans, state->args->dp,
					blkno, -1, &blk->bp,
					state->args->whichfork);
		if (error) {
			blk->blkno = 0;
			state->path.active--;
			return(error);
		}
		ASSERT(blk->bp != NULL);
		current = blk->bp->data;
		ASSERT(current->magic == XFS_DA_NODE_MAGIC ||
		       current->magic == XFS_DIRX_LEAF_MAGIC(state->mp) ||
		       current->magic == XFS_ATTR_LEAF_MAGIC);

		/*
		 * Search an intermediate node for a match.
		 */
		blk->magic = current->magic;
		if (current->magic == XFS_DA_NODE_MAGIC) {
			node = blk->bp->data;
			blk->hashval = node->btree[ node->hdr.count-1 ].hashval;

			/*
			 * Binary search.  (note: small blocks will skip loop)
			 */
			max = node->hdr.count;
			probe = span = max / 2;
			hashval = state->args->hashval;
			for (btree = &node->btree[probe]; span > 4;
				   btree = &node->btree[probe]) {
				span /= 2;
				if (btree->hashval < hashval)
					probe += span;
				else if (btree->hashval > hashval)
					probe -= span;
				else
					break;
			}
			ASSERT((probe >= 0) && (probe < max));
			ASSERT((span <= 4) || (btree->hashval == hashval));

			/*
			 * Since we may have duplicate hashval's, find the first
			 * matching hashval in the node.
			 */
			while ((probe > 0) && (btree->hashval >= hashval)) {
				btree--;
				probe--;
			}
			while ((probe < max) && (btree->hashval < hashval)) {
				btree++;
				probe++;
			}

			/*
			 * Pick the right block to descend on.
			 */
			if (probe == max) {
				blk->index = max-1;
				blkno = node->btree[ max-1 ].before;
			} else {
				blk->index = probe;
				blkno = btree->before;	
			}
		}
#ifndef SIM
		else if (current->magic == XFS_ATTR_LEAF_MAGIC) {
			blk->hashval = xfs_attr_leaf_lasthash(blk->bp, NULL);
			break;
		}
#endif
		else if (current->magic == XFS_DIR_LEAF_MAGIC) {
			blk->hashval = xfs_dir_leaf_lasthash(blk->bp, NULL);
			break;
		}
		else if (current->magic == XFS_DIR2_LEAFN_MAGIC) {
			blk->hashval = xfs_dir2_leafn_lasthash(blk->bp, NULL);
			break;
		}
	}

	/*
	 * A leaf block that ends in the hashval that we are interested in
	 * (final hashval == search hashval) means that the next block may
	 * contain more entries with the same hashval, shift upward to the
	 * next leaf and keep searching.
	 */
	for (;;) {
		if (blk->magic == XFS_DIR_LEAF_MAGIC) {
			ASSERT(XFS_DIR_IS_V1(state->mp));
			retval = xfs_dir_leaf_lookup_int(blk->bp, state->args,
								  &blk->index);
		} else if (blk->magic == XFS_DIR2_LEAFN_MAGIC) {
			ASSERT(XFS_DIR_IS_V2(state->mp));
			retval = xfs_dir2_leafn_lookup_int(blk->bp, state->args,
							&blk->index, state);
		}
#ifndef SIM
		else if (blk->magic == XFS_ATTR_LEAF_MAGIC) {
			retval = xfs_attr_leaf_lookup_int(blk->bp, state->args);
			blk->index = state->args->index;
			state->args->blkno = blk->blkno;
		}
#endif
		if (((retval == ENOENT) || (retval == ENOATTR)) &&
		    (blk->hashval == state->args->hashval)) {
			error = xfs_da_path_shift(state, &state->path, 1, 1,
							 &retval);
			if (error)
				return(error);
			if (retval == 0) {
				continue;
			}
#ifndef SIM
			else if (blk->magic == XFS_ATTR_LEAF_MAGIC) {
				/* path_shift() gives ENOENT */
				retval = XFS_ERROR(ENOATTR);
			}
#endif
		}
		break;
	}
	*result = retval;
	return(0);	
}

/*========================================================================
 * Utility routines.
 *========================================================================*/

/*
 * Link a new block into a doubly linked list of blocks (of whatever type).
 */
int							/* error */
xfs_da_blk_link(xfs_da_state_t *state, xfs_da_state_blk_t *old_blk,
			       xfs_da_state_blk_t *new_blk)
{
	xfs_da_blkinfo_t *old_info, *new_info, *tmp_info;
	xfs_da_args_t *args;
	int before, error;
	xfs_dabuf_t *bp;

	/*
	 * Set up environment.
	 */
	args = state->args;
	ASSERT(args != NULL);
	old_info = old_blk->bp->data;
	new_info = new_blk->bp->data;
	ASSERT(old_blk->magic == XFS_DA_NODE_MAGIC ||
	       old_blk->magic == XFS_DIRX_LEAF_MAGIC(state->mp) ||
	       old_blk->magic == XFS_ATTR_LEAF_MAGIC);
	ASSERT(old_blk->magic == old_info->magic);
	ASSERT(new_blk->magic == new_info->magic);
	ASSERT(old_blk->magic == new_blk->magic);

	switch (old_blk->magic) {
#ifndef SIM
	case XFS_ATTR_LEAF_MAGIC:
		before = xfs_attr_leaf_order(old_blk->bp, new_blk->bp);
		break;
#endif
	case XFS_DIR_LEAF_MAGIC:
		ASSERT(XFS_DIR_IS_V1(state->mp));
		before = xfs_dir_leaf_order(old_blk->bp, new_blk->bp);
		break;
	case XFS_DIR2_LEAFN_MAGIC:
		ASSERT(XFS_DIR_IS_V2(state->mp));
		before = xfs_dir2_leafn_order(old_blk->bp, new_blk->bp);
		break;
	case XFS_DA_NODE_MAGIC:
		before = xfs_da_node_order(old_blk->bp, new_blk->bp);
		break;
	}

	/*
	 * Link blocks in appropriate order.
	 */
	if (before) {
		/*
		 * Link new block in before existing block.
		 */
		new_info->forw = old_blk->blkno;
		new_info->back = old_info->back;
		if (old_info->back) {
			error = xfs_da_read_buf(args->trans, args->dp,
						old_info->back, -1, &bp,
						args->whichfork);
			if (error)
				return(error);
			ASSERT(bp != NULL);
			tmp_info = bp->data;
			ASSERT(tmp_info->magic == old_info->magic);
			ASSERT(tmp_info->forw == old_blk->blkno);
			tmp_info->forw = new_blk->blkno;
			xfs_da_log_buf(args->trans, bp, 0, sizeof(*tmp_info)-1);
			xfs_da_buf_done(bp);
		}
		old_info->back = new_blk->blkno;
	} else {
		/*
		 * Link new block in after existing block.
		 */
		new_info->forw = old_info->forw;
		new_info->back = old_blk->blkno;
		if (old_info->forw) {
			error = xfs_da_read_buf(args->trans, args->dp,
						old_info->forw, -1, &bp,
						args->whichfork);
			if (error)
				return(error);
			ASSERT(bp != NULL);
			tmp_info = bp->data;
			ASSERT(tmp_info->magic == old_info->magic);
			ASSERT(tmp_info->back == old_blk->blkno);
			tmp_info->back = new_blk->blkno;
			xfs_da_log_buf(args->trans, bp, 0, sizeof(*tmp_info)-1);
			xfs_da_buf_done(bp);
		}
		old_info->forw = new_blk->blkno;
	}

	xfs_da_log_buf(args->trans, old_blk->bp, 0, sizeof(*tmp_info) - 1);
	xfs_da_log_buf(args->trans, new_blk->bp, 0, sizeof(*tmp_info) - 1);
	return(0);
}

/*
 * Compare two intermediate nodes for "order".
 */
STATIC int
xfs_da_node_order(xfs_dabuf_t *node1_bp, xfs_dabuf_t *node2_bp)
{
	xfs_da_intnode_t *node1, *node2;

	node1 = node1_bp->data;
	node2 = node2_bp->data;
	ASSERT((node1->hdr.info.magic == XFS_DA_NODE_MAGIC) &&
	       (node2->hdr.info.magic == XFS_DA_NODE_MAGIC));
	if ((node1->hdr.count > 0) && (node2->hdr.count > 0) && 
	    ((node2->btree[ 0 ].hashval <
	      node1->btree[ 0 ].hashval) ||
	     (node2->btree[ node2->hdr.count-1 ].hashval <
	      node1->btree[ node1->hdr.count-1 ].hashval))) {
		return(1);
	}
	return(0);
}

/*
 * Pick up the last hashvalue from an intermediate node.
 */
STATIC uint
xfs_da_node_lasthash(xfs_dabuf_t *bp, int *count)
{
	xfs_da_intnode_t *node;

	node = bp->data;
	ASSERT(node->hdr.info.magic == XFS_DA_NODE_MAGIC);
	if (count)
		*count = node->hdr.count;
	if (node->hdr.count == 0)
		return(0);
	return(node->btree[ node->hdr.count-1 ].hashval);
}

#if defined(XFS_REPAIR_SIM) || !defined(SIM)
/*
 * Unlink a block from a doubly linked list of blocks.
 */
int							/* error */
xfs_da_blk_unlink(xfs_da_state_t *state, xfs_da_state_blk_t *drop_blk,
				 xfs_da_state_blk_t *save_blk)
{
	xfs_da_blkinfo_t *drop_info, *save_info, *tmp_info;
	xfs_da_args_t *args;
	xfs_dabuf_t *bp;
	int error;

	/*
	 * Set up environment.
	 */
	args = state->args;
	ASSERT(args != NULL);
	save_info = save_blk->bp->data;
	drop_info = drop_blk->bp->data;
	ASSERT(save_blk->magic == XFS_DA_NODE_MAGIC ||
	       save_blk->magic == XFS_DIRX_LEAF_MAGIC(state->mp) ||
	       save_blk->magic == XFS_ATTR_LEAF_MAGIC);
	ASSERT(save_blk->magic == save_info->magic);
	ASSERT(drop_blk->magic == drop_info->magic);
	ASSERT(save_blk->magic == drop_blk->magic);
	ASSERT((save_info->forw == drop_blk->blkno) ||
	       (save_info->back == drop_blk->blkno));
	ASSERT((drop_info->forw == save_blk->blkno) ||
	       (drop_info->back == save_blk->blkno));

	/*
	 * Unlink the leaf block from the doubly linked chain of leaves.
	 */
	if (save_info->back == drop_blk->blkno) {
		save_info->back = drop_info->back;
		if (drop_info->back) {
			error = xfs_da_read_buf(args->trans, args->dp,
						drop_info->back, -1, &bp,
						args->whichfork);
			if (error)
				return(error);
			ASSERT(bp != NULL);
			tmp_info = bp->data;
			ASSERT(tmp_info->magic == save_info->magic);
			ASSERT(tmp_info->forw == drop_blk->blkno);
			tmp_info->forw = save_blk->blkno;
			xfs_da_log_buf(args->trans, bp, 0,
						    sizeof(*tmp_info) - 1);
			xfs_da_buf_done(bp);
		}
	} else {
		save_info->forw = drop_info->forw;
		if (drop_info->forw) {
			error = xfs_da_read_buf(args->trans, args->dp,
						drop_info->forw, -1, &bp,
						args->whichfork);
			if (error)
				return(error);
			ASSERT(bp != NULL);
			tmp_info = bp->data;
			ASSERT(tmp_info->magic == save_info->magic);
			ASSERT(tmp_info->back == drop_blk->blkno);
			tmp_info->back = save_blk->blkno;
			xfs_da_log_buf(args->trans, bp, 0,
						    sizeof(*tmp_info) - 1);
			xfs_da_buf_done(bp);
		}
	}

	xfs_da_log_buf(args->trans, save_blk->bp, 0, sizeof(*save_info) - 1);
	return(0);
}
#endif /* XFS_REPAIR_SIM || !SIM */

/*
 * Move a path "forward" or "!forward" one block at the current level.
 *
 * This routine will adjust a "path" to point to the next block
 * "forward" (higher hashvalues) or "!forward" (lower hashvals) in the
 * Btree, including updating pointers to the intermediate nodes between
 * the new bottom and the root.
 */
int							/* error */
xfs_da_path_shift(xfs_da_state_t *state, xfs_da_state_path_t *path,
				 int forward, int release, int *result)
{
	xfs_da_state_blk_t *blk;
	xfs_da_blkinfo_t *info;
	xfs_da_intnode_t *node;
	xfs_da_args_t *args;
	xfs_dablk_t blkno;
	int level, error;

	/*
	 * Roll up the Btree looking for the first block where our
	 * current index is not at the edge of the block.  Note that
	 * we skip the bottom layer because we want the sibling block.
	 */
	args = state->args;
	ASSERT(args != NULL);
	ASSERT(path != NULL);
	ASSERT((path->active > 0) && (path->active < XFS_DA_NODE_MAXDEPTH));
	level = (path->active-1) - 1;	/* skip bottom layer in path */
	for (blk = &path->blk[level]; level >= 0; blk--, level--) {
		ASSERT(blk->bp != NULL);
		node = blk->bp->data;
		ASSERT(node->hdr.info.magic == XFS_DA_NODE_MAGIC);
		if (forward && (blk->index < node->hdr.count-1)) {
			blk->index++;
			blkno = node->btree[ blk->index ].before;
			break;
		} else if (!forward && (blk->index > 0)) {
			blk->index--;
			blkno = node->btree[ blk->index ].before;
			break;
		}
	}
	if (level < 0) {
		*result = XFS_ERROR(ENOENT);	/* we're out of our tree */
		ASSERT(args->oknoent);
		return(0);
	}

	/*
	 * Roll down the edge of the subtree until we reach the
	 * same depth we were at originally.
	 */
	for (blk++, level++; level < path->active; blk++, level++) {
		/*
		 * Release the old block.
		 * (if it's dirty, trans won't actually let go)
		 */
		if (release)
			xfs_da_brelse(args->trans, blk->bp);

		/*
		 * Read the next child block.
		 */
		blk->blkno = blkno;
		error = xfs_da_read_buf(args->trans, args->dp, blkno, -1,
						     &blk->bp, args->whichfork);
		if (error)
			return(error);
		ASSERT(blk->bp != NULL);
		info = blk->bp->data;
		ASSERT(info->magic == XFS_DA_NODE_MAGIC ||
		       info->magic == XFS_DIRX_LEAF_MAGIC(state->mp) ||
		       info->magic == XFS_ATTR_LEAF_MAGIC);
		blk->magic = info->magic;
		if (info->magic == XFS_DA_NODE_MAGIC) {
			node = (xfs_da_intnode_t *)info;
			blk->hashval = node->btree[ node->hdr.count-1 ].hashval;
			if (forward)
				blk->index = 0;
			else
				blk->index = node->hdr.count-1;
			blkno = node->btree[ blk->index ].before;
		} else {
			ASSERT(level == path->active-1);
			blk->index = 0;
			switch(blk->magic) {
#ifndef SIM
			case XFS_ATTR_LEAF_MAGIC:
				blk->hashval = xfs_attr_leaf_lasthash(blk->bp,
								      NULL);
				break;
#endif
			case XFS_DIR_LEAF_MAGIC:
				ASSERT(XFS_DIR_IS_V1(state->mp));
				blk->hashval = xfs_dir_leaf_lasthash(blk->bp,
								     NULL);
				break;
			case XFS_DIR2_LEAFN_MAGIC:
				ASSERT(XFS_DIR_IS_V2(state->mp));
				blk->hashval = xfs_dir2_leafn_lasthash(blk->bp,
								       NULL);
				break;
			default:
				ASSERT(blk->magic == XFS_ATTR_LEAF_MAGIC ||
				       blk->magic ==
				       XFS_DIRX_LEAF_MAGIC(state->mp));
				break;
			}
		}
	}
	*result = 0;
	return(0);
}


/*========================================================================
 * Utility routines.
 *========================================================================*/

/*
 * Implement a simple hash on a character string.
 * Rotate the hash value by 7 bits, then XOR each character in.
 * This is implemented with some source-level loop unrolling.
 */
xfs_dahash_t
xfs_da_hashname(char *name, int namelen)
{
	xfs_dahash_t hash;

#define	ROTL(x,y)	(((x) << (y)) | ((x) >> (32 - (y))))
#ifdef SLOWVERSION
	/*
	 * This is the old one-byte-at-a-time version.
	 */
	for (hash = 0; namelen > 0; namelen--) {
		hash = *name++ ^ ROTL(hash, 7);
	}
	return(hash);
#else
	/*
	 * Do four characters at a time as long as we can.
	 */
	for (hash = 0; namelen >= 4; namelen -= 4, name += 4) {
		hash = (name[0] << 21) ^ (name[1] << 14) ^ (name[2] << 7) ^
		       (name[3] << 0) ^ ROTL(hash, 7 * 4);
	}
	/*
	 * Now do the rest of the characters.
	 */
	switch (namelen) {
	case 3:
		return (name[0] << 14) ^ (name[1] << 7) ^ (name[2] << 0) ^
		       ROTL(hash, 7 * 3);
	case 2:
		return (name[0] << 7) ^ (name[1] << 0) ^ ROTL(hash, 7 * 2);
	case 1:
		return (name[0] << 0) ^ ROTL(hash, 7 * 1);
	case 0:
		return hash;
	}
	/* NOTREACHED */
#endif
#undef ROTL
}

/*
 * Add a block to the btree aread of the file.
 * Return the new block number to the caller.
 */
int
xfs_da_grow_inode(xfs_da_args_t *args, xfs_dablk_t *new_blkno)
{
	xfs_fileoff_t bno, b;
	xfs_bmbt_irec_t map;
	xfs_bmbt_irec_t	*mapp;
	xfs_inode_t *dp;
	int nmap, error, w, count, c, got, i, mapi;
	xfs_fsize_t size;
	xfs_trans_t *tp;
	xfs_mount_t *mp;

	dp = args->dp;
	mp = dp->i_mount;
	w = args->whichfork;
	tp = args->trans;
	/*
	 * For new directories adjust the file offset and block count.
	 */
	if (w == XFS_DATA_FORK && XFS_DIR_IS_V2(mp)) {
		bno = mp->m_dirleafblk;
		count = mp->m_dirblkfsbs;
	} else {
		bno = 0;
		count = 1;
	}
	/*
	 * Find a spot in the file space to put the new block.
	 */
	if (error = xfs_bmap_first_unused(tp, dp, count, &bno, w)) {
#pragma mips_frequency_hint NEVER
		return error;
	}
	if (w == XFS_DATA_FORK && XFS_DIR_IS_V2(mp))
		ASSERT(bno >= mp->m_dirleafblk && bno < mp->m_dirfreeblk);
	/*
	 * Try mapping it in one filesystem block.
	 */
	nmap = 1;
	ASSERT(args->firstblock != NULL);
	if (error = xfs_bmapi(tp, dp, bno, count,
			XFS_BMAPI_AFLAG(w)|XFS_BMAPI_WRITE|XFS_BMAPI_METADATA|
			XFS_BMAPI_CONTIG,
			args->firstblock, args->total, &map, &nmap,
			args->flist)) {
#pragma mips_frequency_hint NEVER
		return error;
	}
	ASSERT(nmap <= 1);
	if (nmap == 1) {
		mapp = &map;
		mapi = 1;
	}
	/*
	 * If we didn't get it and the block might work if fragmented,
	 * try without the CONTIG flag.  Loop until we get it all.
	 */
	else if (nmap == 0 && count > 1) {
#pragma mips_frequency_hint NEVER
		mapp = kmem_alloc(sizeof(*mapp) * count, KM_SLEEP);
		for (b = bno, mapi = 0; b < bno + count; ) {
			nmap = MIN(XFS_BMAP_MAX_NMAP, count);
			c = (int)(bno + count - b);
			if (error = xfs_bmapi(tp, dp, b, c,
					XFS_BMAPI_AFLAG(w)|XFS_BMAPI_WRITE|
					XFS_BMAPI_METADATA,
					args->firstblock, args->total,
					&mapp[mapi], &nmap, args->flist)) {
				kmem_free(mapp, sizeof(*mapp) * count);
				return error;
			}
			if (nmap < 1)
				break;
			mapi += nmap;
			b = mapp[mapi - 1].br_startoff +
			    mapp[mapi - 1].br_blockcount;
		}
	} else {
#pragma mips_frequency_hint NEVER
		mapi = 0;
		mapp = NULL;
	}
	/*
	 * Count the blocks we got, make sure it matches the total.
	 */
	for (i = 0, got = 0; i < mapi; i++)
		got += mapp[i].br_blockcount;
	if (got != count || mapp[0].br_startoff != bno ||
	    mapp[mapi - 1].br_startoff + mapp[mapi - 1].br_blockcount !=
	    bno + count) {
#pragma mips_frequency_hint NEVER
		if (mapp != &map)
			kmem_free(mapp, sizeof(*mapp) * count);
		return XFS_ERROR(ENOSPC);
	}
	if (mapp != &map)
		kmem_free(mapp, sizeof(*mapp) * count);
	*new_blkno = (xfs_dablk_t)bno;
	/*
	 * For version 1 directories, adjust the file size if it changed.
	 */
	if (w == XFS_DATA_FORK && XFS_DIR_IS_V1(mp)) {
		ASSERT(mapi == 1);
		if (error = xfs_bmap_last_offset(tp, dp, &bno, w))
			return error;
		size = XFS_FSB_TO_B(mp, bno);
		if (size != dp->i_d.di_size) {
			dp->i_d.di_size = size;
			xfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);
		}
	}
	return 0;
}

#if defined(XFS_REPAIR_SIM) || !defined(SIM)
/*
 * Ick.  We need to always be able to remove a btree block, even
 * if there's no space reservation because the filesystem is full.
 * This is called if xfs_bunmapi on a btree block fails due to ENOSPC.
 * It swaps the target block with the last block in the file.  The
 * last block in the file can always be removed since it can't cause
 * a bmap btree split to do that.
 */
STATIC int
xfs_da_swap_lastblock(xfs_da_args_t *args, xfs_dablk_t *dead_blknop,
		      xfs_dabuf_t **dead_bufp)
{
	xfs_dablk_t dead_blkno, last_blkno, sib_blkno, par_blkno;
	xfs_dabuf_t *dead_buf, *last_buf, *sib_buf, *par_buf;
	xfs_fileoff_t lastoff;
	xfs_inode_t *ip;
	xfs_trans_t *tp;
	xfs_mount_t *mp;
	int error, w, entno, level, dead_level;
	xfs_da_blkinfo_t *dead_info, *sib_info;
	xfs_da_intnode_t *par_node, *dead_node;
	xfs_dir_leafblock_t *dead_leaf;
	xfs_dir2_leaf_t *dead_leaf2;
	xfs_dahash_t dead_hash;

	dead_buf = *dead_bufp;
	dead_blkno = *dead_blknop;
	tp = args->trans;
	ip = args->dp;
	w = args->whichfork;
	ASSERT(w == XFS_DATA_FORK);
	mp = ip->i_mount;
	if (XFS_DIR_IS_V2(mp)) {
		lastoff = mp->m_dirfreeblk;
		error = xfs_bmap_last_before(tp, ip, &lastoff, w);
	} else
		error = xfs_bmap_last_offset(tp, ip, &lastoff, w);
	if (error)
		return error;
	if (lastoff == 0)
		return XFS_ERROR(EFSCORRUPTED);
	/*
	 * Read the last block in the btree space.
	 */
	last_blkno = (xfs_dablk_t)lastoff - mp->m_dirblkfsbs;
	if (error = xfs_da_read_buf(tp, ip, last_blkno, -1, &last_buf, w))
		return error;
	/*
	 * Copy the last block into the dead buffer and log it.
	 */
	bcopy(last_buf->data, dead_buf->data, mp->m_dirblksize);
	xfs_da_log_buf(tp, dead_buf, 0, mp->m_dirblksize - 1);
	dead_info = dead_buf->data;
	/*
	 * Get values from the moved block.
	 */
	if (dead_info->magic == XFS_DIR_LEAF_MAGIC) {
		ASSERT(XFS_DIR_IS_V1(mp));
		dead_leaf = (xfs_dir_leafblock_t *)dead_info;
		dead_level = 0;
		dead_hash =
			dead_leaf->entries[dead_leaf->hdr.count - 1].hashval;
	} else if (dead_info->magic == XFS_DIR2_LEAFN_MAGIC) {
		ASSERT(XFS_DIR_IS_V2(mp));
		dead_leaf2 = (xfs_dir2_leaf_t *)dead_info;
		dead_level = 0;
		dead_hash = dead_leaf2->ents[dead_leaf2->hdr.count - 1].hashval;
	} else {
		ASSERT(dead_info->magic == XFS_DA_NODE_MAGIC);
		dead_node = (xfs_da_intnode_t *)dead_info;
		dead_level = dead_node->hdr.level;
		dead_hash = dead_node->btree[dead_node->hdr.count - 1].hashval;
	}
	sib_buf = par_buf = NULL;
	/*
	 * If the moved block has a left sibling, fix up the pointers.
	 */
	if (sib_blkno = dead_info->back) {
		if (error = xfs_da_read_buf(tp, ip, sib_blkno, -1, &sib_buf, w))
			goto done;
		sib_info = sib_buf->data;
		if (sib_info->forw != last_blkno ||
		    sib_info->magic != dead_info->magic) {
			error = XFS_ERROR(EFSCORRUPTED);
			goto done;
		}
		sib_info->forw = dead_blkno;
		xfs_da_log_buf(tp, sib_buf,
			XFS_DA_LOGRANGE(sib_info, &sib_info->forw,
					sizeof(sib_info->forw)));
		xfs_da_buf_done(sib_buf);
		sib_buf = NULL;
	}
	/*
	 * If the moved block has a right sibling, fix up the pointers.
	 */
	if (sib_blkno = dead_info->forw) {
		if (error = xfs_da_read_buf(tp, ip, sib_blkno, -1, &sib_buf, w))
			goto done;
		sib_info = sib_buf->data;
		if (sib_info->back != last_blkno ||
		    sib_info->magic != dead_info->magic) {
			error = XFS_ERROR(EFSCORRUPTED);
			goto done;
		}
		sib_info->back = dead_blkno;
		xfs_da_log_buf(tp, sib_buf,
			XFS_DA_LOGRANGE(sib_info, &sib_info->back,
					sizeof(sib_info->back)));
		xfs_da_buf_done(sib_buf);
		sib_buf = NULL;
	}
	par_blkno = XFS_DIR_IS_V1(mp) ? 0 : mp->m_dirleafblk;
	level = -1;
	/*
	 * Walk down the tree looking for the parent of the moved block.
	 */
	for (;;) {
		if (error = xfs_da_read_buf(tp, ip, par_blkno, -1, &par_buf, w))
			goto done;
		par_node = par_buf->data;
		if (par_node->hdr.info.magic != XFS_DA_NODE_MAGIC ||
		    (level >= 0 && level != par_node->hdr.level + 1)) {
			error = XFS_ERROR(EFSCORRUPTED);
			goto done;
		}
		level = par_node->hdr.level;
		for (entno = 0;
		     entno < par_node->hdr.count &&
		     par_node->btree[entno].hashval < dead_hash;
		     entno++)
			continue;
		if (entno == par_node->hdr.count) {
			error = XFS_ERROR(EFSCORRUPTED);
			goto done;
		}
		par_blkno = par_node->btree[entno].before;
		if (level == dead_level + 1)
			break;
		xfs_da_brelse(tp, par_buf);
		par_buf = NULL;
	}
	/*
	 * We're in the right parent block.
	 * Look for the right entry.
	 */
	for (;;) {
		for (;
		     entno < par_node->hdr.count &&
		     par_node->btree[entno].before != last_blkno;
		     entno++)
			continue;
		if (entno < par_node->hdr.count)
			break;
		par_blkno = par_node->hdr.info.forw;
		xfs_da_brelse(tp, par_buf);
		par_buf = NULL;
		if (par_blkno == 0) {
			error = XFS_ERROR(EFSCORRUPTED);
			goto done;
		}
		if (error = xfs_da_read_buf(tp, ip, par_blkno, -1, &par_buf, w))
			goto done;
		par_node = par_buf->data;
		if (par_node->hdr.level != level ||
		    par_node->hdr.info.magic != XFS_DA_NODE_MAGIC) {
			error = XFS_ERROR(EFSCORRUPTED);
			goto done;
		}
		entno = 0;
	}
	/*
	 * Update the parent entry pointing to the moved block.
	 */
	par_node->btree[entno].before = dead_blkno;
	xfs_da_log_buf(tp, par_buf,
		XFS_DA_LOGRANGE(par_node, &par_node->btree[entno].before,
				sizeof(par_node->btree[entno].before)));
	xfs_da_buf_done(par_buf);
	xfs_da_buf_done(dead_buf);
	*dead_blknop = last_blkno;
	*dead_bufp = last_buf;
	return 0;
done:
	if (par_buf)
		xfs_da_brelse(tp, par_buf);
	if (sib_buf)
		xfs_da_brelse(tp, sib_buf);
	xfs_da_brelse(tp, last_buf);
	return error;
}

/*
 * Remove a btree block from a directory or attribute.
 */
int
xfs_da_shrink_inode(xfs_da_args_t *args, xfs_dablk_t dead_blkno,
		    xfs_dabuf_t *dead_buf)
{
	xfs_inode_t *dp;
	int done, error, w, count;
	xfs_fileoff_t bno;
	xfs_fsize_t size;
	xfs_trans_t *tp;
	xfs_mount_t *mp;

	dp = args->dp;
	w = args->whichfork;
	tp = args->trans;
	mp = dp->i_mount;
	if (w == XFS_DATA_FORK && XFS_DIR_IS_V2(mp))
		count = mp->m_dirblkfsbs;
	else
		count = 1;
	for (;;) {
		/*
		 * Remove extents.  If we get ENOSPC for a dir we have to move
		 * the last block to the place we want to kill.
		 */
		if ((error = xfs_bunmapi(tp, dp, dead_blkno, count,
				XFS_BMAPI_AFLAG(w)|XFS_BMAPI_METADATA,
				0, args->firstblock, args->flist,
				&done)) == ENOSPC) {
			if (w != XFS_DATA_FORK)
				goto done;
			if (error = xfs_da_swap_lastblock(args, &dead_blkno,
					&dead_buf))
				goto done;
		} else if (error)
			goto done;
		else
			break;
	}
	ASSERT(done);
	xfs_da_binval(tp, dead_buf);
	/*
	 * Adjust the directory size for version 1.
	 */
	if (w == XFS_DATA_FORK && XFS_DIR_IS_V1(mp)) {
		if (error = xfs_bmap_last_offset(tp, dp, &bno, w))
			return error;
		size = XFS_FSB_TO_B(dp->i_mount, bno);
		if (size != dp->i_d.di_size) {
			dp->i_d.di_size = size;
			xfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);
		}
	}
	return 0;
done:
	xfs_da_binval(tp, dead_buf);
	return error;
}
#endif /* XFS_REPAIR_SIM || !SIM */

/*
 * See if the mapping(s) for this btree block are valid, i.e.
 * don't contain holes, are logically contiguous, and cover the whole range.
 */
STATIC int
xfs_da_map_covers_blocks(
	int		nmap,
	xfs_bmbt_irec_t	*mapp,
	xfs_dablk_t	bno,
	int		count)
{
	int		i;
	xfs_fileoff_t	off;

	for (i = 0, off = bno; i < nmap; i++) {
		if (mapp[i].br_startblock == HOLESTARTBLOCK ||
		    mapp[i].br_startblock == DELAYSTARTBLOCK) {
#pragma mips_frequency_hint NEVER
			return 0;
		}
		if (off != mapp[i].br_startoff) {
#pragma mips_frequency_hint NEVER
			return 0;
		}
		off += mapp[i].br_blockcount;
	}
	return off == bno + count;
}

/*
 * Make a dabuf.
 * Used for get_buf, read_buf, read_bufr, and reada_buf.
 */
STATIC int
xfs_da_do_buf(
	xfs_trans_t	*trans,
	xfs_inode_t	*dp,
	xfs_dablk_t	bno,
	daddr_t		*mappedbnop,
	xfs_dabuf_t	**bpp,
	int		whichfork,
	int		caller,
	inst_t		*ra)
{
	buf_t		*bp = 0;
	buf_t		**bplist;
	int		error;
	int		i;
	xfs_bmbt_irec_t	map;
	xfs_bmbt_irec_t	*mapp;
	daddr_t		mappedbno;
	xfs_mount_t	*mp;
	int		nbplist;
	int		nfsb;
	int		nmap;
	xfs_dabuf_t	*rbp;

	mp = dp->i_mount;
	if (whichfork == XFS_DATA_FORK && XFS_DIR_IS_V2(mp))
		nfsb = mp->m_dirblkfsbs;
	else
		nfsb = 1;
	mappedbno = *mappedbnop;
	/*
	 * Caller doesn't have a mapping.  -2 means don't complain
	 * if we land in a hole.
	 */
	if (mappedbno == -1 || mappedbno == -2) {
		/*
		 * Optimize the one-block case.
		 */
		if (nfsb == 1) {
			xfs_fsblock_t	fsb;

			if (error =
			    xfs_bmapi_single(trans, dp, whichfork, &fsb,
				    (xfs_fileoff_t)bno)) {
#pragma mips_frequency_hint NEVER
				return error;
			}
			mapp = &map;
			if (fsb == NULLFSBLOCK) {
#pragma mips_frequency_hint NEVER
				nmap = 0;
			} else {
				map.br_startblock = fsb;
				map.br_startoff = (xfs_fileoff_t)bno;
				map.br_blockcount = 1;
				nmap = 1;
			}
		} else {
#pragma mips_frequency_hint NEVER
			xfs_fsblock_t	firstblock;

			firstblock = NULLFSBLOCK;
			mapp = kmem_alloc(sizeof(*mapp) * nfsb, KM_SLEEP);
			nmap = nfsb;
			if (error = xfs_bmapi(trans, dp, (xfs_fileoff_t)bno,
					nfsb,
					XFS_BMAPI_METADATA |
						XFS_BMAPI_AFLAG(whichfork),
					&firstblock, 0, mapp, &nmap, NULL))
				goto exit0;
		}
	} else {
		map.br_startblock = XFS_DADDR_TO_FSB(mp, mappedbno);
		map.br_startoff = (xfs_fileoff_t)bno;
		map.br_blockcount = nfsb;
		mapp = &map;
		nmap = 1;
	}
	if (!xfs_da_map_covers_blocks(nmap, mapp, bno, nfsb)) {
#pragma mips_frequency_hint NEVER
		error = mappedbno == -2 ? 0 : XFS_ERROR(EFSCORRUPTED);
		goto exit0;
	}
	if (caller != 3 && nmap > 1) {
#pragma mips_frequency_hint NEVER
		bplist = kmem_alloc(sizeof(*bplist) * nmap, KM_SLEEP);
		nbplist = 0;
	} else
		bplist = NULL;
	/*
	 * Turn the mapping(s) into buffer(s).
	 */
	for (i = 0; i < nmap; i++) {
		int	nmapped;

		mappedbno = XFS_FSB_TO_DADDR(mp, mapp[i].br_startblock);
		if (i == 0)
			*mappedbnop = mappedbno;
		nmapped = (int)XFS_FSB_TO_BB(mp, mapp[i].br_blockcount);
		switch (caller) {
		case 0:
			bp = xfs_trans_get_buf(trans, mp->m_ddev_targp,
				mappedbno, nmapped, 0);
			error = bp ? geterror(bp) : XFS_ERROR(EIO);
			break;
		case 1:
#ifdef XFS_REPAIR_SIM
		case 2:
#endif /* XFS_REPAIR_SIM */
			bp = NULL;
			error = xfs_trans_read_buf(mp, trans, mp->m_dev,
				mappedbno, nmapped, 0, &bp);
			break;
#ifndef SIM
		case 3:
			baread(mp->m_ddev_targp, mappedbno, nmapped);
			error = 0;
			bp = NULL;
			break;
#endif /* !SIM */
		}
		if (error) {
#pragma mips_frequency_hint NEVER
			if (bp)
				xfs_trans_brelse(trans, bp);
			goto exit1;
		}
		if (!bp)
			continue;
		if (caller == 1) {
			if (whichfork == XFS_ATTR_FORK) {
				bp->b_ref = XFS_ATTR_BTREE_REF;
				bp->b_bvtype = B_FS_ATTR_BTREE;
			} else {
				bp->b_ref = XFS_DIR_BTREE_REF;
				bp->b_bvtype = B_FS_DIR_BTREE;
			}
		}
		if (bplist) {
#pragma mips_frequency_hint NEVER
			bplist[nbplist++] = bp;
		}
	}
	/*
	 * Build a dabuf structure.
	 */
	if (bplist) {
#pragma mips_frequency_hint NEVER
		rbp = xfs_da_buf_make(nbplist, bplist, ra);
	} else if (bp)
		rbp = xfs_da_buf_make(1, &bp, ra);
	else
		rbp = NULL;
	/*
	 * For read_buf, check the magic number.
	 */
	if (caller == 1) {
		xfs_dir2_data_t		*data;
		xfs_dir2_free_t		*free;
		xfs_da_blkinfo_t	*info;

		info = rbp->data;
		data = rbp->data;
		free = rbp->data;
		if (XFS_TEST_ERROR((info->magic != XFS_DA_NODE_MAGIC) &&
				   (info->magic != XFS_DIR_LEAF_MAGIC) &&
				   (info->magic != XFS_ATTR_LEAF_MAGIC) &&
				   (info->magic != XFS_DIR2_LEAF1_MAGIC) &&
				   (info->magic != XFS_DIR2_LEAFN_MAGIC) &&
				   (data->hdr.magic != XFS_DIR2_BLOCK_MAGIC) &&
				   (data->hdr.magic != XFS_DIR2_DATA_MAGIC) &&
				   (free->hdr.magic != XFS_DIR2_FREE_MAGIC),
				mp, XFS_ERRTAG_DA_READ_BUF,
				XFS_RANDOM_DA_READ_BUF)) {
#pragma mips_frequency_hint NEVER
			buftrace("DA READ ERROR", rbp->bps[0]);
			error = XFS_ERROR(EFSCORRUPTED);
			xfs_da_brelse(trans, rbp);
			nbplist = 0;
			goto exit1;
		}
	}
	if (bplist) {
#pragma mips_frequency_hint NEVER
		kmem_free(bplist, sizeof(*bplist) * nmap);
	}
	if (mapp != &map) {
#pragma mips_frequency_hint NEVER
		kmem_free(mapp, sizeof(*mapp) * nfsb);
	}
	if (bpp)
		*bpp = rbp;
	return 0;
exit1:
	if (bplist) {
		for (i = 0; i < nbplist; i++)
			xfs_trans_brelse(trans, bplist[i]);
		kmem_free(bplist, sizeof(*bplist) * nmap);
	}
exit0:
	if (mapp != &map)
		kmem_free(mapp, sizeof(*mapp) * nfsb);
	if (bpp)
		*bpp = NULL;
	return error;
}

/*
 * Get a buffer for the dir/attr block.
 */
int
xfs_da_get_buf(
	xfs_trans_t	*trans,
	xfs_inode_t	*dp,
	xfs_dablk_t	bno,
	daddr_t		mappedbno,
	xfs_dabuf_t	**bpp,
	int		whichfork)
{
	return xfs_da_do_buf(trans, dp, bno, &mappedbno, bpp, whichfork, 0,
		(inst_t *)__return_address);
}

/*
 * Get a buffer for the dir/attr block, fill in the contents.
 */
int
xfs_da_read_buf(
	xfs_trans_t	*trans,
	xfs_inode_t	*dp,
	xfs_dablk_t	bno,
	daddr_t		mappedbno,
	xfs_dabuf_t	**bpp,
	int		whichfork)
{
	return xfs_da_do_buf(trans, dp, bno, &mappedbno, bpp, whichfork, 1,
		(inst_t *)__return_address);
}

#ifdef XFS_REPAIR_SIM
/*
 * Get a buffer for the dir/attr block, fill in the contents.
 * Don't check magic number, the caller will (it's xfs_repair).
 */
int
xfs_da_read_bufr(
	xfs_trans_t	*trans,
	xfs_inode_t	*dp,
	xfs_dablk_t	bno,
	daddr_t		mappedbno,
	xfs_dabuf_t	**bpp,
	int		whichfork)
{
	return xfs_da_do_buf(trans, dp, bno, &mappedbno, bpp, whichfork, 2,
		(inst_t *)__return_address);
}
#endif /* XFS_REPAIR_SIM */

#ifndef SIM
/*
 * Readahead the dir/attr block.
 */
daddr_t
xfs_da_reada_buf(
	xfs_trans_t	*trans,
	xfs_inode_t	*dp,
	xfs_dablk_t	bno,
	int		whichfork)
{
	daddr_t		rval;

	rval = -1;
	if (xfs_da_do_buf(trans, dp, bno, &rval, NULL, whichfork, 3,
			(inst_t *)__return_address))
		return -1;
	else
		return rval;
}
#endif	/* !SIM */

/*
 * Calculate the number of bits needed to hold i different values.
 */
uint
xfs_da_log2_roundup(uint i)
{
	uint rval;

	for (rval = 0; rval < NBBY * sizeof(i); rval++) {
		if ((1 << rval) >= i)
			break;
	}
	return(rval);
}

zone_t *xfs_da_state_zone;	/* anchor for state struct zone */
zone_t *xfs_dabuf_zone;		/* dabuf zone */

/*
 * Allocate a dir-state structure.
 * We don't put them on the stack since they're large.
 */
xfs_da_state_t *
xfs_da_state_alloc(void)
{
	return kmem_zone_zalloc(xfs_da_state_zone, KM_SLEEP);
}

/*
 * Kill the altpath contents of a da-state structure.
 */
void
xfs_da_state_kill_altpath(xfs_da_state_t *state)
{
	int	i;

	for (i = 0; i < state->altpath.active; i++) {
		if (state->altpath.blk[i].bp) {
			if (state->altpath.blk[i].bp != state->path.blk[i].bp)
				xfs_da_buf_done(state->altpath.blk[i].bp);
			state->altpath.blk[i].bp = NULL;
		}
	}
	state->altpath.active = 0;
}

/*
 * Free a da-state structure.
 */
void
xfs_da_state_free(xfs_da_state_t *state)
{
	int	i;

	xfs_da_state_kill_altpath(state);
	for (i = 0; i < state->path.active; i++) {
		if (state->path.blk[i].bp)
			xfs_da_buf_done(state->path.blk[i].bp);
	}
	if (state->extravalid && state->extrablk.bp)
		xfs_da_buf_done(state->extrablk.bp);
#ifdef DEBUG
	bzero((char *)state, sizeof(*state));
#endif /* DEBUG */
	kmem_zone_free(xfs_da_state_zone, state);
}

#ifdef XFS_DABUF_DEBUG
xfs_dabuf_t	*xfs_dabuf_global_list;
lock_t		xfs_dabuf_global_lock;
#endif

/*
 * Create a dabuf.
 */
/* ARGSUSED */
STATIC xfs_dabuf_t *
xfs_da_buf_make(int nbuf, buf_t **bps, inst_t *ra)
{
	buf_t		*bp;
	xfs_dabuf_t	*dabuf;
	int		i;
	int		off;

	if (nbuf == 1)
		dabuf = kmem_zone_alloc(xfs_dabuf_zone, KM_SLEEP);
	else
		dabuf = kmem_alloc(XFS_DA_BUF_SIZE(nbuf), KM_SLEEP);
	dabuf->dirty = 0;
#ifdef XFS_DABUF_DEBUG
	dabuf->ra = ra;
	dabuf->dev = bps[0]->b_edev;
	dabuf->blkno = bps[0]->b_blkno;
#endif
	if (nbuf == 1) {
		dabuf->nbuf = 1;
		bp = bps[0];
		dabuf->bbcount = (short)BTOBB(bp->b_bcount);
		dabuf->data = bp->b_un.b_addr;
		dabuf->bps[0] = bp;
	} else {
		dabuf->nbuf = nbuf;
		for (i = 0, dabuf->bbcount = 0; i < nbuf; i++) {
			dabuf->bps[i] = bp = bps[i];
			dabuf->bbcount += BTOBB(bp->b_bcount);
		}
		dabuf->data = kmem_alloc(BBTOB(dabuf->bbcount), KM_SLEEP);
		for (i = off = 0; i < nbuf; i++, off += bp->b_bcount) {
			bp = bps[i];
			bcopy(bp->b_un.b_addr, (char *)dabuf->data + off,
				bp->b_bcount);
		}
	}
#ifdef XFS_DABUF_DEBUG
	{
		int		s;
		xfs_dabuf_t	*p;

		s = mutex_spinlock(&xfs_dabuf_global_lock);
		for (p = xfs_dabuf_global_list; p; p = p->next) {
			ASSERT(p->blkno != dabuf->blkno ||
			       p->dev != dabuf->dev);
		}
		dabuf->prev = NULL;
		if (xfs_dabuf_global_list)
			xfs_dabuf_global_list->prev = dabuf;
		dabuf->next = xfs_dabuf_global_list;
		xfs_dabuf_global_list = dabuf;
		mutex_spinunlock(&xfs_dabuf_global_lock, s);
	}
#endif
	return dabuf;
}

/*
 * Un-dirty a dabuf.
 */
STATIC void
xfs_da_buf_clean(xfs_dabuf_t *dabuf)
{
	buf_t	*bp;
	int	i;
	int	off;

	if (dabuf->dirty) {
		ASSERT(dabuf->nbuf > 1);
		dabuf->dirty = 0;
		for (i = off = 0; i < dabuf->nbuf; i++, off += bp->b_bcount) {
			bp = dabuf->bps[i];
			bcopy((char *)dabuf->data + off, bp->b_un.b_addr,
				bp->b_bcount);
		}
	}
}

/*
 * Release a dabuf.
 */
void
xfs_da_buf_done(xfs_dabuf_t *dabuf)
{
	ASSERT(dabuf->nbuf && dabuf->data && dabuf->bbcount && dabuf->bps[0]);
	if (dabuf->dirty)
		xfs_da_buf_clean(dabuf);
	if (dabuf->nbuf > 1)
		kmem_free(dabuf->data, BBTOB(dabuf->bbcount));
#ifdef XFS_DABUF_DEBUG
	{
		int	s;

		s = mutex_spinlock(&xfs_dabuf_global_lock);
		if (dabuf->prev)
			dabuf->prev->next = dabuf->next;
		else
			xfs_dabuf_global_list = dabuf->next;
		if (dabuf->next)
			dabuf->next->prev = dabuf->prev;
		mutex_spinunlock(&xfs_dabuf_global_lock, s);
	}
	bzero(dabuf, XFS_DA_BUF_SIZE(dabuf->nbuf));
#endif
	if (dabuf->nbuf == 1)
		kmem_zone_free(xfs_dabuf_zone, dabuf);
	else
		kmem_free(dabuf, XFS_DA_BUF_SIZE(dabuf->nbuf));
}

/*
 * Log transaction from a dabuf.
 */
void
xfs_da_log_buf(xfs_trans_t *tp, xfs_dabuf_t *dabuf, uint first, uint last)
{
	buf_t	*bp;
	uint	f;
	int	i;
	uint	l;
	int	off;

	ASSERT(dabuf->nbuf && dabuf->data && dabuf->bbcount && dabuf->bps[0]);
	if (dabuf->nbuf == 1) {
		ASSERT(dabuf->data == (void *)dabuf->bps[0]->b_un.b_addr);
		xfs_trans_log_buf(tp, dabuf->bps[0], first, last);
		return;
	}
	dabuf->dirty = 1;
	ASSERT(first <= last);
	for (i = off = 0; i < dabuf->nbuf; i++, off += bp->b_bcount) {
		bp = dabuf->bps[i];
		f = off;
		l = f + bp->b_bcount - 1;
		if (f < first)
			f = first;
		if (l > last)
			l = last;
		if (f <= l)
			xfs_trans_log_buf(tp, bp, f - off, l - off);
		/* 
		 * B_DONE is set by xfs_trans_log buf.
		 * If we don't set it on a new buffer (get not read)
		 * then if we don't put anything in the buffer it won't
		 * be set, and at commit it it released into the cache,
		 * and then a read will fail.
		 */
		else if (!(bp->b_flags & B_DONE))
			bp->b_flags |= B_DONE;
	}
	ASSERT(last < off);
}

/*
 * Release dabuf from a transaction.
 * Have to free up the dabuf before the buffers are released,
 * since the synchronization on the dabuf is really the lock on the buffer.
 */
void
xfs_da_brelse(xfs_trans_t *tp, xfs_dabuf_t *dabuf)
{
	buf_t	*bp;
	buf_t	**bplist;
	int	i;
	int	nbuf;

	ASSERT(dabuf->nbuf && dabuf->data && dabuf->bbcount && dabuf->bps[0]);
	if ((nbuf = dabuf->nbuf) == 1) {
		bplist = &bp;
		bp = dabuf->bps[0];
	} else {
		bplist = kmem_alloc(nbuf * sizeof(*bplist), KM_SLEEP);
		bcopy(dabuf->bps, bplist, nbuf * sizeof(*bplist));
	}
	xfs_da_buf_done(dabuf);
	for (i = 0; i < nbuf; i++)
		xfs_trans_brelse(tp, bplist[i]);
	if (bplist != &bp)
		kmem_free(bplist, nbuf * sizeof(*bplist));
}

#ifdef XFS_REPAIR_SIM
/*
 * Write dabuf from simulation, no transaction.
 * Have to free up the dabuf before the buffers are released,
 * since the synchronization on the dabuf is really the lock on the buffer.
 */
void
xfs_da_bwrite(xfs_dabuf_t *dabuf)
{
	buf_t	*bp;
	buf_t	**bplist;
	int	i;
	int	nbuf;

	ASSERT(dabuf->nbuf && dabuf->data && dabuf->bbcount && dabuf->bps[0]);
	if ((nbuf = dabuf->nbuf) == 1) {
		bplist = &bp;
		bp = dabuf->bps[0];
	} else {
		bplist = kmem_alloc(nbuf * sizeof(*bplist), KM_SLEEP);
		bcopy(dabuf->bps, bplist, nbuf * sizeof(*bplist));
	}
	xfs_da_buf_done(dabuf);
	for (i = 0; i < nbuf; i++)
		bwrite(bplist[i]);
	if (bplist != &bp)
		kmem_free(bplist, nbuf * sizeof(*bplist));
}

/*
 * Hold dabuf at transaction commit.
 */
void
xfs_da_bhold(xfs_trans_t *tp, xfs_dabuf_t *dabuf)
{
	int	i;

	for (i = 0; i < dabuf->nbuf; i++)
		xfs_trans_bhold(tp, dabuf->bps[i]);
}

/*
 * Join dabuf to transaction.
 */
void
xfs_da_bjoin(xfs_trans_t *tp, xfs_dabuf_t *dabuf)
{
	int	i;

	for (i = 0; i < dabuf->nbuf; i++)
		xfs_trans_bjoin(tp, dabuf->bps[i]);
}
#endif	/* XFS_REPAIR_SIM */

/*
 * Invalidate dabuf from a transaction.
 */
void
xfs_da_binval(xfs_trans_t *tp, xfs_dabuf_t *dabuf)
{
	buf_t	*bp;
	buf_t	**bplist;
	int	i;
	int	nbuf;

	ASSERT(dabuf->nbuf && dabuf->data && dabuf->bbcount && dabuf->bps[0]);
	if ((nbuf = dabuf->nbuf) == 1) {
		bplist = &bp;
		bp = dabuf->bps[0];
	} else {
		bplist = kmem_alloc(nbuf * sizeof(*bplist), KM_SLEEP);
		bcopy(dabuf->bps, bplist, nbuf * sizeof(*bplist));
	}
	xfs_da_buf_done(dabuf);
	for (i = 0; i < nbuf; i++)
		xfs_trans_binval(tp, bplist[i]);
	if (bplist != &bp)
		kmem_free(bplist, nbuf * sizeof(*bplist));
}

/*
 * Get the first daddr from a dabuf.
 */
daddr_t
xfs_da_blkno(xfs_dabuf_t *dabuf)
{
	ASSERT(dabuf->nbuf);
	ASSERT(dabuf->data);
	return dabuf->bps[0]->b_blkno;
}
