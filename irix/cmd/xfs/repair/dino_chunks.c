#ident "$Revision: 1.31 $"

#include "vsn.h"
#define _KERNEL 1
#include <sys/param.h>
#include <sys/buf.h>
#undef _KERNEL
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <bstring.h>
#if VERS <= V_64
#include <sys/avl.h>
#else
#include "avl.h"
#endif
#include <sys/uuid.h>
#if VERS >= V_64
#include <ksys/behavior.h>
#endif
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_bit.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_mount.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_da_btree.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_dir2_sf.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_ialloc.h>

#include "sim.h"
#include "globals.h"
#include "agheader.h"
#include "incore.h"
#include "protos.h"
#include "err_protos.h"
#include "dir.h"
#include "dinode.h"
#include "versions.h"

/*
 * validates inode block or chunk, returns # of good inodes
 * the dinodes are verified using verify_uncertain_dinode() which
 * means only the basic inode info is checked, no fork checks.
 */

int
check_aginode_block(xfs_mount_t	*mp,
			xfs_agnumber_t	agno,
			xfs_agblock_t	agbno)
{

	xfs_dinode_t	*dino_p;
        int		i;
        int		cnt = 0;
	buf_t		*bp;

	/*
	 * it's ok to read these possible inode blocks in one at
	 * a time because they don't belong to known inodes (if
	 * they did, we'd know about them courtesy of the incore inode
	 * tree and we wouldn't be here and we stale the buffers out
	 * so no one else will overlap them.
	 */
        bp = read_buf(mp->m_dev, XFS_AGB_TO_DADDR(mp, agno, agbno),
			(int)XFS_FSB_TO_BB(mp, 1), BUF_TRYLOCK);

	if (bp == NULL || geterror(bp)) {
		do_warn("cannot read agbno (%u/%u), disk block %lld\n",
			agno, agbno,
			(daddr_t) XFS_AGB_TO_DADDR(mp, agno, agbno));
		return(0);
	}
				
	for (i = 0; i < mp->m_sb.sb_inopblock; i++)  {
		dino_p = XFS_MAKE_IPTR(mp, bp, i);
		if (!verify_uncertain_dinode(mp, dino_p, agno,
				XFS_OFFBNO_TO_AGINO(mp, agbno, i)))
			cnt++;
	}

	bp->b_flags |= B_STALE;
	brelse(bp);

	return(cnt);
}

int
check_inode_block(xfs_mount_t		*mp,
			xfs_ino_t	ino)
{
        return(check_aginode_block(mp, XFS_INO_TO_AGNO(mp, ino),
					XFS_INO_TO_AGBNO(mp, ino)));
}

/*
 * tries to establish if the inode really exists in a valid
 * inode chunk.  returns number of new inodes if things are good
 * and 0 if bad.  start is the start of the discovered inode chunk.
 * routine assumes that ino is a legal inode number
 * (verified by verify_inum()).  If the inode chunk turns out
 * to be good, this routine will put the inode chunk into
 * the good inode chunk tree if required.
 *
 * the verify_(ag)inode* family of routines are utility
 * routines called by check_uncertain_aginodes() and
 * process_uncertain_aginodes().
 */
int
verify_inode_chunk(xfs_mount_t		*mp,
			xfs_ino_t	ino,
			xfs_ino_t	*start_ino)
{
	xfs_agnumber_t	agno;
	xfs_agino_t	agino;
	xfs_agino_t	start_agino;
	xfs_agblock_t	agbno;
	xfs_agblock_t	start_agbno;
	xfs_agblock_t	end_agbno;
	xfs_agblock_t	max_agbno;
	xfs_agblock_t	cur_agbno;
	xfs_agblock_t	chunk_start_agbno;
	xfs_agblock_t	chunk_stop_agbno;
	ino_tree_node_t *irec_before_p = NULL;
	ino_tree_node_t *irec_after_p = NULL;
	ino_tree_node_t *irec_p;
	ino_tree_node_t *irec_next_p;
	int		irec_cnt;
	int		ino_cnt = 0;
	int		num_blks;
	int		i;
	int		j;
	int		state;

        agno = XFS_INO_TO_AGNO(mp, ino);
        agino = XFS_INO_TO_AGINO(mp, ino);
	agbno = XFS_INO_TO_AGBNO(mp, ino);
	*start_ino = NULLFSINO;

	assert(XFS_IALLOC_BLOCKS(mp) > 0);

	if (agno == mp->m_sb.sb_agcount - 1)
		max_agbno = mp->m_sb.sb_dblocks -
			(xfs_drfsbno_t) mp->m_sb.sb_agblocks * agno;
	else
		max_agbno = mp->m_sb.sb_agblocks;

	/*
	 * is the inode beyond the end of the AG?
	 */
	if (agbno >= max_agbno)
		return(0);

	/*
	 * check for the easy case, inodes per block >= XFS_INODES_PER_CHUNK
	 * (multiple chunks per block)
	 */
	if (XFS_IALLOC_BLOCKS(mp) == 1)  {
		if (agbno > max_agbno)
			return(0);

		if (check_inode_block(mp, ino) == 0)
			return(0);

		switch (state = get_agbno_state(mp, agno, agbno))  {
		case XR_E_INO:
			do_warn("uncertain inode block %d/%d already known\n",
				agno, agbno);
			break;
		case XR_E_UNKNOWN:
		case XR_E_FREE1:
		case XR_E_FREE:
			set_agbno_state(mp, agno, agbno, XR_E_INO);
			break;
		case XR_E_MULT:
		case XR_E_INUSE:
		case XR_E_INUSE_FS:
		case XR_E_FS_MAP:
			/*
			 * if block is already claimed, forget it.
			 */
			do_warn(
			    "inode block %d/%d multiply claimed, (state %d)\n",
				agno, agbno, state);
			set_agbno_state(mp, agno, agbno, XR_E_MULT);
			return(0);
		default:
			do_warn("inode block %d/%d bad state, (state %d)\n",
				agno, agbno, state);
			set_agbno_state(mp, agno, agbno, XR_E_INO);
			break;
		}

		start_agino = XFS_OFFBNO_TO_AGINO(mp, agbno, 0);
		*start_ino = XFS_AGINO_TO_INO(mp, agno, start_agino);

		/*
		 * put new inode record(s) into inode tree
		 */
		for (j = 0; j < chunks_pblock; j++)  {
			if ((irec_p = find_inode_rec(agno, start_agino))
					== NULL)  {
				irec_p = set_inode_free_alloc(agno,
							start_agino);
				for (i = 1; i < XFS_INODES_PER_CHUNK; i++)
					set_inode_free(irec_p, i);
			}
			if (start_agino <= agino && agino <
					start_agino + XFS_INODES_PER_CHUNK)
				set_inode_used(irec_p, agino - start_agino);

			start_agino += XFS_INODES_PER_CHUNK;
			ino_cnt += XFS_INODES_PER_CHUNK;
		}

		return(ino_cnt);
	} else if (fs_aligned_inodes)  {
		/*
		 * next easy case -- aligned inode filesystem.
		 * just check out the chunk
		 */
		start_agbno = rounddown(XFS_INO_TO_AGBNO(mp, ino),
					fs_ino_alignment);
		end_agbno = start_agbno + XFS_IALLOC_BLOCKS(mp);

		/*
		 * if this fs has aligned inodes but the end of the
		 * chunk is beyond the end of the ag, this is a bad
		 * chunk
		 */
		if (end_agbno > max_agbno)
			return(0);

		/*
		 * check out all blocks in chunk
		 */
		ino_cnt = 0;
		for (cur_agbno = start_agbno; cur_agbno < end_agbno;
						cur_agbno++)  {
			ino_cnt += check_aginode_block(mp, agno, cur_agbno);
		}

		/*
		 * if we lose either 2 blocks worth of inodes or >25% of
		 * the chunk, just forget it.
		 */
		if (ino_cnt < XFS_INODES_PER_CHUNK - 2 * mp->m_sb.sb_inopblock
				|| ino_cnt < XFS_INODES_PER_CHUNK - 16)
			return(0);

		/*
		 * ok, put the record into the tree.  we know that it's
		 * not already there since the inode is guaranteed
		 * not to be in the tree.
		 */
		start_agino = XFS_OFFBNO_TO_AGINO(mp, start_agbno, 0);
		*start_ino = XFS_AGINO_TO_INO(mp, agno, start_agino);

		irec_p = set_inode_free_alloc(agno,
				XFS_OFFBNO_TO_AGINO(mp, start_agbno, 0));

		for (i = 1; i < XFS_INODES_PER_CHUNK; i++)
			set_inode_free(irec_p, i);

		assert(start_agino <= agino &&
				start_agino + XFS_INODES_PER_CHUNK > agino);

		set_inode_used(irec_p, agino - start_agino);

		return(XFS_INODES_PER_CHUNK);
	}

	/*
	 * hard case -- pre-6.3 filesystem.
	 * set default start/end agbnos and ensure agbnos are legal.
	 * we're setting a range [start_agbno, end_agbno) such that
	 * a discovered inode chunk completely within that range
	 * would include the inode passed into us.
	 */
	if (XFS_IALLOC_BLOCKS(mp) > 1)  {
		if (agino > XFS_IALLOC_INODES(mp))
			start_agbno = agbno - XFS_IALLOC_BLOCKS(mp) + 1;
		else
			start_agbno = 1;
	}

	end_agbno = agbno + XFS_IALLOC_BLOCKS(mp);

	if (end_agbno > max_agbno)
		end_agbno = max_agbno;

	/*
	 * search tree for known inodes within +/- 1 inode chunk range
	 */
	irec_before_p = irec_after_p = NULL;

	find_inode_rec_range(agno, XFS_OFFBNO_TO_AGINO(mp, start_agbno, 0),
		XFS_OFFBNO_TO_AGINO(mp, end_agbno, mp->m_sb.sb_inopblock - 1),
		&irec_before_p, &irec_after_p);

	/*
	 * if we have known inode chunks in our search range, establish
	 * their start and end-points to tighten our search range.  range
	 * is [start, end) -- e.g. max/end agbno is one beyond the
	 * last block to be examined.  the avl routines work this way.
	 */
	if (irec_before_p)  {
		/*
		 * only one inode record in the range, move one boundary in
		 */
		if (irec_before_p == irec_after_p)  {
			if (irec_before_p->ino_startnum < agino)
				start_agbno = XFS_AGINO_TO_AGBNO(mp,
						irec_before_p->ino_startnum +
						XFS_INODES_PER_CHUNK);
			else
				end_agbno = XFS_AGINO_TO_AGBNO(mp,
						irec_before_p->ino_startnum);
		}

		/*
		 * find the start of the gap in the search range (which
		 * should contain our unknown inode).  if the only irec
		 * within +/- 1 chunks starts after the inode we're
		 * looking for, skip this stuff since the end_agbno
		 * of the range has already been trimmed in to not
		 * include that irec.
		 */
		if (irec_before_p->ino_startnum < agino)  {
			irec_p = irec_before_p;
			irec_next_p = next_ino_rec(irec_p);

			while(irec_next_p != NULL &&
				irec_p->ino_startnum + XFS_INODES_PER_CHUNK ==
					irec_next_p->ino_startnum)  {
				irec_p = irec_next_p;
				irec_next_p = next_ino_rec(irec_next_p);
			}

			start_agbno = XFS_AGINO_TO_AGBNO(mp,
						irec_p->ino_startnum) +
						XFS_IALLOC_BLOCKS(mp);

			/*
			 * we know that the inode we're trying to verify isn't
			 * in an inode chunk so the next ino_rec marks the end
			 * of the gap -- is it within the search range?
			 */
			if (irec_next_p != NULL &&
					agino + XFS_IALLOC_INODES(mp) >=
						irec_next_p->ino_startnum)
				end_agbno = XFS_AGINO_TO_AGBNO(mp,
						irec_next_p->ino_startnum);
		}

		assert(start_agbno < end_agbno);
	}

	/*
	 * if the gap is too small to contain a chunk, we lose.
	 * this means that inode chunks known to be good surround
	 * the inode in question and that the space between them
	 * is too small for a legal inode chunk
	 */
	if (end_agbno - start_agbno < XFS_IALLOC_BLOCKS(mp))
		return(0);

	/*
	 * now grunge around the disk, start at the inode block and
	 * go in each direction until you hit a non-inode block or
	 * run into a range boundary.  A non-inode block is block
	 * with *no* good inodes in it.  Unfortunately, we can't
	 * co-opt bad blocks into inode chunks (which might take
	 * care of disk blocks that turn into zeroes) because the
	 * filesystem could very well allocate two inode chunks
	 * with a one block file in between and we'd zap the file.
	 * We're better off just losing the rest of the
	 * inode chunk instead.
	 */
	for (cur_agbno = agbno; cur_agbno >= start_agbno; cur_agbno--)  {
		/*
		 * if the block has no inodes, it's a bad block so
		 * break out now without decrementing cur_agbno so
		 * chunk start blockno will be set to the last good block
		 */
		if (!(irec_cnt = check_aginode_block(mp, agno, cur_agbno)))
			break;
		ino_cnt += irec_cnt;
	}

	chunk_start_agbno = cur_agbno + 1;

	for (cur_agbno = agbno + 1; cur_agbno < end_agbno; cur_agbno++)   {
		/*
		 * if the block has no inodes, it's a bad block so
		 * break out now without incrementing cur_agbno so
		 * chunk start blockno will be set to the block
		 * immediately after the last good block.
		 */
		if (!(irec_cnt = check_aginode_block(mp, agno, cur_agbno)))
			break;
		ino_cnt += irec_cnt;
	}

	chunk_stop_agbno = cur_agbno;

	num_blks = chunk_stop_agbno - chunk_start_agbno;

	if (num_blks < XFS_IALLOC_BLOCKS(mp) || ino_cnt == 0)
		return(0);

	/*
	 * XXX - later - if the entire range is selected and they're all
	 * good inodes, keep searching in either direction.
	 * until you the range of inodes end, then split into chunks
	 * for now, just take one chunk's worth starting at the lowest
	 * possible point and hopefully we'll pick the rest up later.
	 *
	 * XXX - if we were going to fix up an inode chunk for
	 * any good inodes in the chunk, this is where we would
	 * do it.  For now, keep it simple and lose the rest of
	 * the chunk
	 */

	if (num_blks % XFS_IALLOC_BLOCKS(mp) != 0)  {
		num_blks = rounddown(num_blks, XFS_IALLOC_BLOCKS(mp));
		chunk_stop_agbno = chunk_start_agbno + num_blks;
	}

	/*
	 * ok, we've got a candidate inode chunk.  now we have to
	 * verify that we aren't trying to use blocks that are already
	 * in use.  If so, mark them as multiply claimed since odds
	 * are very low that we found this chunk by stumbling across
	 * user data -- we're probably here as a result of a directory
	 * entry or an iunlinked pointer
	 */
	for (j = 0, cur_agbno = chunk_start_agbno;
			cur_agbno < chunk_stop_agbno; cur_agbno++)  {
		switch (state = get_agbno_state(mp, agno, cur_agbno))  {
		case XR_E_MULT:
		case XR_E_INUSE:
		case XR_E_INUSE_FS:
		case XR_E_FS_MAP:
			do_warn(
			    "inode block %d/%d multiply claimed, (state %d)\n",
				agno, cur_agbno, state);
			set_agbno_state(mp, agno, cur_agbno, XR_E_MULT);
			j = 1;
			break;
		case XR_E_INO:
			do_error(
		"uncertain inode block overlap, agbno = %d, ino = %llu\n",
				agbno, ino);
			break;
		default:
			break;
		}

		if (j)
			return(0);
	}

	/*
	 * ok, chunk is good.  put the record into the tree if required,
	 * and fill in the bitmap.  All inodes will be marked as "free"
	 * except for the one that led us to discover the chunk.  That's
	 * ok because we'll override the free setting later if the
	 * contents of the inode indicate it's in use.
	 */
	start_agino = XFS_OFFBNO_TO_AGINO(mp, chunk_start_agbno, 0);
	*start_ino = XFS_AGINO_TO_INO(mp, agno, start_agino);

	assert(find_inode_rec(agno, start_agino) == NULL);

	irec_p = set_inode_free_alloc(agno, start_agino);
	for (i = 1; i < XFS_INODES_PER_CHUNK; i++)
		set_inode_free(irec_p, i);

	assert(start_agino <= agino &&
			start_agino + XFS_INODES_PER_CHUNK > agino);

	set_inode_used(irec_p, agino - start_agino);

	for (cur_agbno = chunk_start_agbno;
			cur_agbno < chunk_stop_agbno; cur_agbno++)  {
		switch (state = get_agbno_state(mp, agno, cur_agbno))  {
		case XR_E_INO:
			do_error("uncertain inode block %llu already known\n",
				XFS_AGB_TO_FSB(mp, agno, cur_agbno));
			break;
		case XR_E_UNKNOWN:
		case XR_E_FREE1:
		case XR_E_FREE:
			set_agbno_state(mp, agno, cur_agbno, XR_E_INO);
			break;
		case XR_E_MULT:
		case XR_E_INUSE:
		case XR_E_INUSE_FS:
		case XR_E_FS_MAP:
			do_error(
			    "inode block %d/%d multiply claimed, (state %d)\n",
				agno, cur_agbno, state);
			break;
		default:
			do_warn("inode block %d/%d bad state, (state %d)\n",
				agno, cur_agbno, state);
			set_agbno_state(mp, agno, cur_agbno, XR_E_INO);
			break;
		}
	}

	return(ino_cnt);
}

/*
 * same as above only for ag inode chunks
 */
int
verify_aginode_chunk(xfs_mount_t	*mp,
			xfs_agnumber_t	agno,
			xfs_agino_t	agino,
			xfs_agino_t	*agino_start)
{
	xfs_ino_t	ino;
	int		res;

	res = verify_inode_chunk(mp, XFS_AGINO_TO_INO(mp, agno, agino), &ino);

	if (res)
		*agino_start = XFS_INO_TO_AGINO(mp, ino);
	else
		*agino_start = NULLAGINO;

	return(res);
}

/*
 * this does the same as the two above only it returns a pointer
 * to the inode record in the good inode tree
 */
ino_tree_node_t *
verify_aginode_chunk_irec(xfs_mount_t	*mp,
			xfs_agnumber_t	agno,
			xfs_agino_t	agino)
{
	xfs_agino_t start_agino;
	ino_tree_node_t *irec = NULL;

	if (verify_aginode_chunk(mp, agno, agino, &start_agino))
		irec = find_inode_rec(agno, start_agino);

	return(irec);
}



/*
 * processes an inode allocation chunk/block, returns 1 on I/O errors,
 * 0 otherwise
 *
 * *bogus is set to 1 if the entire set of inodes is bad.
 */
/* ARGSUSED */
int
process_inode_chunk(xfs_mount_t *mp, xfs_agnumber_t agno, int num_inos,
			ino_tree_node_t *first_irec, int ino_discovery,
			int check_dups, int extra_attr_check, int *bogus)
{
	xfs_ino_t		parent;
	ino_tree_node_t		*ino_rec;
	buf_t			*bp = NULL;
	xfs_dinode_t		*dino;
	int			icnt;
	int			status;
	int			is_used;
	int			state;
	int			done;
	int			ino_dirty;
	int			irec_offset;
	int			ibuf_offset;
	xfs_agino_t		agino;
	xfs_agblock_t		agbno;
	int			dirty = 0;
	int			cleared = 0;
	int			isa_dir = 0;

	assert(first_irec != NULL);
	assert(XFS_AGINO_TO_OFFSET(mp, first_irec->ino_startnum) == 0);

	*bogus = 0;
	assert(XFS_IALLOC_BLOCKS(mp) > 0);

	/*
	 * get all blocks required to read in this chunk (may wind up
	 * having to process more chunks in a multi-chunk per block fs)
	 */
	agbno = XFS_AGINO_TO_AGBNO(mp, first_irec->ino_startnum);
	bp = read_buf(mp->m_dev, XFS_AGB_TO_DADDR(mp, agno, agbno),
			(int)XFS_FSB_TO_BB(mp, XFS_IALLOC_BLOCKS(mp)),
			BUF_TRYLOCK);

	if (bp == NULL || geterror(bp))  {
		do_warn("cannot read inode %llu, disk block %lld, cnt %d\n",
			XFS_AGINO_TO_INO(mp, agno, first_irec->ino_startnum),
			XFS_AGB_TO_DADDR(mp, agno, agbno),
			(int)XFS_FSB_TO_BB(mp, XFS_IALLOC_BLOCKS(mp)));
		return(1);
	}

	/*
	 * set up first irec
	 */
	ino_rec = first_irec;
	/*
	 * initialize counters
	 */
	irec_offset = 0;
	ibuf_offset = 0;
	icnt = 0;
	status = 0;
	done = 0;

	/*
	 * verify inode chunk if necessary
	 */
	if (ino_discovery)  {
		while (!done)  {
			/*
			 * make inode pointer
			 */
			dino = XFS_MAKE_IPTR(mp, bp, icnt);
			agino = irec_offset + ino_rec->ino_startnum;

			/*
			 * we always think that the root and realtime
			 * inodes are verified even though we may have
			 * to reset them later to keep from losing the
			 * chunk that they're in
			 */
			if (verify_dinode(mp, dino, agno, agino) == 0 ||
					agno == 0 &&
					(mp->m_sb.sb_rootino == agino ||
					 mp->m_sb.sb_rsumino == agino ||
					 mp->m_sb.sb_rbmino == agino))
				status++;

			irec_offset++;
			icnt++;

			if (icnt == XFS_IALLOC_INODES(mp) &&
					irec_offset == XFS_INODES_PER_CHUNK)  {
				/*
				 * done! - finished up irec and block
				 * simultaneously
				 */
				brelse(bp);
				done = 1;
				break;
			} else if (irec_offset == XFS_INODES_PER_CHUNK)  {
				/*
				 * get new irec (multiple chunks per block fs)
				 */
				ino_rec = next_ino_rec(ino_rec);
				assert(ino_rec->ino_startnum == agino + 1);
				irec_offset = 0;
			}
		}

		/*
		 * if chunk/block is bad, blow it off.  the inode records
		 * will be deleted by the caller if appropriate.
		 */
		if (!status)  {
			*bogus = 1;
			return(0);
		}

		/*
		 * reset irec and counters
		 */
		ino_rec = first_irec;

		irec_offset = 0;
		ibuf_offset = 0;
		icnt = 0;
		status = 0;
		done = 0;

		/*
		 * get first block
		 */
		bp = read_buf(mp->m_dev, XFS_AGB_TO_DADDR(mp, agno, agbno),
			(int)XFS_FSB_TO_BB(mp, XFS_IALLOC_BLOCKS(mp)),
			BUF_TRYLOCK);

		if (bp == NULL || geterror(bp))  {
			do_warn(
			"can't read inode %llu, disk block %lld, cnt %d\n",
				XFS_AGINO_TO_INO(mp, agno, agino),
				XFS_AGB_TO_DADDR(mp, agno, agbno),
				(int)XFS_FSB_TO_BB(mp, XFS_IALLOC_BLOCKS(mp)));
			return(1);
		}
	}

	/*
	 * mark block as an inode block in the incore bitmap
	 */
	switch (state = get_agbno_state(mp, agno, agbno))  {
	case XR_E_INO:	/* already marked */
		break;
	case XR_E_UNKNOWN:
	case XR_E_FREE:
	case XR_E_FREE1:
		set_agbno_state(mp, agno, agbno, XR_E_INO);
		break;
	case XR_E_BAD_STATE:
		do_error("bad state in block map %d\n", state);
		break;
	default:
		set_agbno_state(mp, agno, agbno, XR_E_MULT);
		do_warn("inode block %llu multiply claimed, state was %d\n",
			XFS_AGB_TO_FSB(mp, agno, agbno), state);
		break;
	}

	while (!done)  {
		/*
		 * make inode pointer
		 */
		dino = XFS_MAKE_IPTR(mp, bp, icnt);
		agino = irec_offset + ino_rec->ino_startnum;

		is_used = 3;
		ino_dirty = 0;
		parent = 0;

		status = process_dinode(mp, dino, agno, agino,
				is_inode_free(ino_rec, irec_offset),
				&ino_dirty, &cleared, &is_used,
				ino_discovery, check_dups,
				extra_attr_check, &isa_dir, &parent);

		assert(is_used != 3);
		if (ino_dirty)
			dirty = 1;
		/*
		 * XXX - if we want to try and keep
		 * track of whether we need to bang on
		 * the inode maps (instead of just
		 * blindly reconstructing them like
		 * we do now, this is where to start.
		 */
		if (is_used)  {
			if (is_inode_free(ino_rec, irec_offset))  {
				if (verbose || no_modify ||
				    XFS_AGINO_TO_INO(mp, agno, agino) !=
							old_orphanage_ino)  {
					do_warn(
			"imap claims in-use inode %llu is free, ",
						XFS_AGINO_TO_INO(mp, agno,
								agino));
				}

				if (verbose || (!no_modify &&
				    XFS_AGINO_TO_INO(mp, agno, agino) !=
						old_orphanage_ino))
					do_warn("correcting imap\n");
				else
					do_warn("would correct imap\n");
			}
			set_inode_used(ino_rec, irec_offset);
		} else  {
			set_inode_free(ino_rec, irec_offset);
		}

		/*
		 * if we lose the root inode, or it turns into
		 * a non-directory, that allows us to double-check
		 * later whether or not we need to reinitialize it.
		 */
		if (isa_dir)  {
			set_inode_isadir(ino_rec, irec_offset);
			/*
			 * we always set the parent but
			 * we may as well wait until
			 * phase 4 (no inode discovery)
			 * because the parent info will
			 * be solid then.
			 */
			if (!ino_discovery)  {
				assert(parent != 0);
				set_inode_parent(ino_rec, irec_offset, parent);
				assert(parent ==
					get_inode_parent(ino_rec, irec_offset));
			}
		} else  {
			clear_inode_isadir(ino_rec, irec_offset);
		}

		if (status)  {
			if (mp->m_sb.sb_rootino ==
					XFS_AGINO_TO_INO(mp, agno, agino))  {
				need_root_inode = 1;

				if (!no_modify)  {
					do_warn("cleared root inode %llu\n",
						XFS_AGINO_TO_INO(mp, agno,
						agino));
				} else  {
					do_warn("would clear root inode %llu\n",
						XFS_AGINO_TO_INO(mp, agno,
						agino));
				}
			} else if (mp->m_sb.sb_rbmino ==
					XFS_AGINO_TO_INO(mp, agno, agino))  {
				need_rbmino = 1;

				if (!no_modify)  {
					do_warn(
				"cleared realtime bitmap inode %llu\n",
						XFS_AGINO_TO_INO(mp, agno,
						agino));
				} else  {
					do_warn(
				"would clear realtime bitmap inode %llu\n",
						XFS_AGINO_TO_INO(mp, agno,
						agino));
				}
			} else if (mp->m_sb.sb_rsumino ==
					XFS_AGINO_TO_INO(mp, agno, agino))  {
				need_rsumino = 1;

				if (!no_modify)  {
					do_warn(
				"cleared realtime summary inode %llu\n",
						XFS_AGINO_TO_INO(mp, agno,
						agino));
				} else  {
					do_warn(
				"would clear realtime summary inode %llu\n",
						XFS_AGINO_TO_INO(mp, agno,
						agino));
				}
			} else if (!no_modify)  {
				do_warn("cleared inode %llu\n",
					XFS_AGINO_TO_INO(mp, agno, agino));
			} else  {
				do_warn("would have cleared inode %llu\n",
					XFS_AGINO_TO_INO(mp, agno, agino));
			}
		}

		irec_offset++;
		ibuf_offset++;
		icnt++;

		if (icnt == XFS_IALLOC_INODES(mp) &&
				irec_offset == XFS_INODES_PER_CHUNK)  {
			/*
			 * done! - finished up irec and block simultaneously
			 */
			if (dirty && !no_modify)
				bwrite(bp);
			else
				brelse(bp);

			done = 1;
			break;
		} else if (ibuf_offset == mp->m_sb.sb_inopblock)  {
			/*
			 * mark block as an inode block in the incore bitmap
			 * and reset inode buffer offset counter
			 */
			ibuf_offset = 0;
			agbno++;

			switch (state = get_agbno_state(mp, agno, agbno))  {
			case XR_E_INO:	/* already marked */
				break;
			case XR_E_UNKNOWN:
			case XR_E_FREE:
			case XR_E_FREE1:
				set_agbno_state(mp, agno, agbno, XR_E_INO);
				break;
			case XR_E_BAD_STATE:
				do_error( "bad state in block map %d\n",
					state);
				break;
			default:
				set_agbno_state(mp, agno, agbno, XR_E_MULT);
				do_warn(
			"inode block %llu multiply claimed, state was %d\n",
					XFS_AGB_TO_FSB(mp, agno, agbno), state);
				break;
			}

		} else if (irec_offset == XFS_INODES_PER_CHUNK)  {
			/*
			 * get new irec (multiple chunks per block fs)
			 */
			ino_rec = next_ino_rec(ino_rec);
			assert(ino_rec->ino_startnum == agino + 1);
			irec_offset = 0;
		}
	}

	return(0);
}

/*
 * check all inodes mentioned in the ag's incore inode maps.
 * the map may be incomplete.  If so, we'll catch the missing
 * inodes (hopefully) when we traverse the directory tree.
 * check_dirs is set to 1 if directory inodes should be
 * processed for internal consistency, parent setting and
 * discovery of unknown inodes.  this only happens
 * in phase 3.  check_dups is set to 1 if we're looking for
 * inodes that reference duplicate blocks so we can trash
 * the inode right then and there.  this is set only in
 * phase 4 after we've run through and set the bitmap once.
 */
void
process_aginodes(xfs_mount_t *mp, xfs_agnumber_t agno,
		int ino_discovery, int check_dups, int extra_attr_check)
{
	int num_inos, bogus;
	ino_tree_node_t *ino_rec, *first_ino_rec, *prev_ino_rec;

	first_ino_rec = ino_rec = findfirst_inode_rec(agno);
	while (ino_rec != NULL)  {
		/*
		 * paranoia - step through inode records until we step
		 * through a full allocation of inodes.  this could
		 * be an issue in big-block filesystems where a block
		 * can hold more than one inode chunk.  make sure to
		 * grab the record corresponding to the beginning of
		 * the next block before we call the processing routines.
		 */
		num_inos = XFS_INODES_PER_CHUNK;
		while (num_inos < XFS_IALLOC_INODES(mp) && ino_rec != NULL)  {
			assert(ino_rec != NULL);
			/*
			 * inodes chunks will always be aligned and sized
			 * correctly
			 */
			if ((ino_rec = next_ino_rec(ino_rec)) != NULL)
				num_inos += XFS_INODES_PER_CHUNK;
		}

		assert(num_inos == XFS_IALLOC_INODES(mp));

		if (process_inode_chunk(mp, agno, num_inos, first_ino_rec,
				ino_discovery, check_dups, extra_attr_check, &bogus))  {
			/* XXX - i/o error, we've got a problem */
			abort();
		}

		if (!bogus)
			first_ino_rec = ino_rec = next_ino_rec(ino_rec);
		else  {
			/*
			 * inodes pointed to by this record are
			 * completely bogus, blow the records for
			 * this chunk out.
			 * the inode block(s) will get reclaimed
			 * in phase 4 when the block map is
			 * reconstructed after inodes claiming
			 * duplicate blocks are deleted.
			 */
			num_inos = 0;
			ino_rec = first_ino_rec;
			while (num_inos < XFS_IALLOC_INODES(mp) &&
					ino_rec != NULL)  {
				prev_ino_rec = ino_rec;

				if ((ino_rec = next_ino_rec(ino_rec)) != NULL)
					num_inos += XFS_INODES_PER_CHUNK;

				get_inode_rec(agno, prev_ino_rec);
				free_inode_rec(agno, prev_ino_rec);
			}

			first_ino_rec = ino_rec;
		}
	}
}

/*
 * verify the uncertain inode list for an ag.
 * Good inodes get moved into the good inode tree.
 * returns 0 if there are no uncertain inode records to
 * be processed, 1 otherwise.  This routine destroys the
 * the entire uncertain inode tree for the ag as a side-effect.
 */
void
check_uncertain_aginodes(xfs_mount_t *mp, xfs_agnumber_t agno)
{
	ino_tree_node_t		*irec;
	ino_tree_node_t		*nrec;
	xfs_agino_t		start;
	xfs_agino_t		i;
	xfs_agino_t		agino;
	int			got_some;

	nrec = NULL;
	got_some = 0;

	clear_uncertain_ino_cache(agno);

	if ((irec = findfirst_uncertain_inode_rec(agno)) == NULL)
		return;

	/*
	 * the trick here is to find a contiguous range
	 * of inodes, make sure that it doesn't overlap
	 * with a known to exist chunk, and then make
	 * sure it is a number of entire chunks.
	 * we check on-disk once we have an idea of what's
	 * going on just to double-check.
	 *
	 * process the uncertain inode record list and look
	 * on disk to see if the referenced inodes are good
	 */

	do_warn("found inodes not in the inode allocation tree\n");

	do {
		/*
		 * check every confirmed (which in this case means
		 * inode that we really suspect to be an inode) inode
		 */
		for (i = 0; i < XFS_INODES_PER_CHUNK; i++)  {
			if (!is_inode_confirmed(irec, i))
				continue;

			agino = i + irec->ino_startnum;

			if (verify_aginum(mp, agno, agino))
				continue;

			if (nrec != NULL && nrec->ino_startnum <= agino &&
					agino < nrec->ino_startnum +
					XFS_INODES_PER_CHUNK)
				continue;

			if ((nrec = find_inode_rec(agno, agino)) == NULL)
				if (!verify_aginum(mp, agno, agino))
					if (verify_aginode_chunk(mp, agno,
							agino, &start))
						got_some = 1;
		}

		get_uncertain_inode_rec(agno, irec);
		free_inode_rec(agno, irec);

		irec = findfirst_uncertain_inode_rec(agno);
	} while (irec != NULL);

	if (got_some)
		do_warn("found inodes not in the inode allocation tree\n");

	return;
}

/*
 * verify and process the uncertain inodes for an ag.
 * this is different from check_ in that we can't just
 * move the good inodes into the good inode tree and let
 * process_aginodes() deal with them because this gets called
 * after process_aginodes() has been run on the ag inode tree.
 * So we have to process the inodes as well as verify since
 * we don't want to rerun process_aginodes() on a tree that has
 * mostly been processed.
 *
 * Note that if this routine does process some inodes, it can
 * add uncertain inodes to any ag which would require that
 * the routine be called again to process those newly-added
 * uncertain inodes.
 *
 * returns 0 if no inodes were processed and 1 if inodes
 * were processed (and it is possible that new uncertain
 * inodes were discovered).
 *
 * as a side-effect, this routine tears down the uncertain
 * inode tree for the ag.
 */
int
process_uncertain_aginodes(xfs_mount_t *mp, xfs_agnumber_t agno)
{
	ino_tree_node_t		*irec;
	ino_tree_node_t		*nrec;
	xfs_agino_t		agino;
	int			i;
	int			bogus;
	int			cnt;
	int			got_some;

#ifdef XR_INODE_TRACE
	fprintf(stderr, "in process_uncertain_aginodes, agno = %d\n", agno);
#endif

	got_some = 0;

	clear_uncertain_ino_cache(agno);

	if ((irec = findfirst_uncertain_inode_rec(agno)) == NULL)
		return(0);

	nrec = NULL;

	do  {
		/*
		 * check every confirmed inode
		 */
		for (cnt = i = 0; i < XFS_INODES_PER_CHUNK; i++)  {
			if (!is_inode_confirmed(irec, i))
				continue;
			cnt++;
			agino = i + irec->ino_startnum;
#ifdef XR_INODE_TRACE
	fprintf(stderr, "ag inode = %d (0x%x)\n", agino, agino);
#endif
			/*
			 * skip over inodes already processed (in the
			 * good tree), bad inode numbers, and inode numbers
			 * pointing to bogus inodes
			 */
			if (verify_aginum(mp, agno, agino))
				continue;

			if (nrec != NULL && nrec->ino_startnum <= agino &&
					agino < nrec->ino_startnum +
					XFS_INODES_PER_CHUNK)
				continue;

			if ((nrec = find_inode_rec(agno, agino)) != NULL)
				continue;

			/*
			 * verify the chunk.  if good, it will be
			 * added to the good inode tree.
			 */
			if ((nrec = verify_aginode_chunk_irec(mp,
						agno, agino)) == NULL)
				continue;

			got_some = 1;

			/*
			 * process the inode record we just added
			 * to the good inode tree.  The inode
			 * processing may add more records to the
			 * uncertain inode lists.
			 */
			if (process_inode_chunk(mp, agno, XFS_IALLOC_INODES(mp),
						nrec, 1, 0, 0, &bogus))  {
				/* XXX - i/o error, we've got a problem */
				abort();
			}
		}

		assert(cnt != 0);
		/*
		 * now return the uncertain inode record to the free pool
		 * and pull another one off the list for processing
		 */
		get_uncertain_inode_rec(agno, irec);
		free_inode_rec(agno, irec);

		irec = findfirst_uncertain_inode_rec(agno);
	} while (irec != NULL);

	if (got_some)
		do_warn("found inodes not in the inode allocation tree\n");

	return(1);
}
