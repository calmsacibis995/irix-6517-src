#ident "$Revision: 1.36 $"

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
#include <bstring.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <assert.h>
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
#include <sys/fs/xfs_inode.h>
#include <sys/fs/xfs_dir_leaf.h>
#include <sys/fs/xfs_attr_leaf.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2_data.h>
#include <sys/fs/xfs_dir2_leaf.h>
#include <sys/fs/xfs_dir2_block.h>
#endif

#include "sim.h"
#include "globals.h"
#include "agheader.h"
#include "incore.h"
#include "protos.h"
#include "err_protos.h"
#include "dinode.h"
#include "dir.h"
#include "bmap.h"
#include "versions.h"
#if VERS >= V_654
#include "dir2.h"
#endif


/* ARGSUSED */
int
lf_block_delete_orphanage(xfs_mount_t		*mp,
			xfs_ino_t		ino,
			xfs_dir_leafblock_t	*leaf,
			int			*dirty,
			buf_t			*rootino_bp,
			int			*rbuf_dirty)
{
	xfs_dir_leaf_entry_t	*entry;
	xfs_dinode_t		*dino;
	buf_t			*bp;
	ino_tree_node_t		*irec;
	xfs_ino_t		lino;
	xfs_dir_leaf_name_t	*namest;
	xfs_agino_t		agino;
	xfs_agnumber_t		agno;
	xfs_agino_t		root_agino;
	xfs_agnumber_t		root_agno;
	int			i;
	int			ino_offset;
	int			ino_dirty;
	int			use_rbuf;
	int			len;
	char			fname[MAXNAMELEN + 1];
	int			res;

	entry = &leaf->entries[0];
	*dirty = 0;
	use_rbuf = 0;
	res = 0;
	root_agno = XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino);
	root_agino = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rootino);

	for (i = 0; i < leaf->hdr.count; entry++, i++)  {
		namest = XFS_DIR_LEAF_NAMESTRUCT(leaf, entry->nameidx);
		XFS_DIR_SF_GET_DIRINO(&namest->inumber, &lino);
		bcopy(namest->name, fname, entry->namelen);
		fname[entry->namelen] = '\0';

		if (fname[0] != '/' && !strcmp(fname, ORPHANAGE))  {
			agino = XFS_INO_TO_AGINO(mp, lino);
			agno = XFS_INO_TO_AGNO(mp, lino);

			old_orphanage_ino = lino;

			irec = find_inode_rec(agno, agino);

			/*
			 * if the orphange inode is in the tree,
			 * get it, clear it, and mark it free.
			 * the inodes in the orphanage will get
			 * reattached to the new orphanage.
			 */
			if (irec != NULL)  {
				ino_offset = agino - irec->ino_startnum;

				/*
				 * check if we have to use the root inode
				 * buffer or read one in ourselves.  Note
				 * that the root inode is always the first
				 * inode of the chunk that it's in so there
				 * are two possible cases where lost+found
				 * might be in the same buffer as the root
				 * inode.  One case is a large block
				 * filesystem where the two inodes are
				 * in different inode chunks but wind
				 * up in the same block (multiple chunks
				 * per block) and the second case (one or
				 * more blocks per chunk) is where the two
				 * inodes are in the same chunk. Note that
				 * inodes are allocated on disk in units
				 * of MAX(XFS_INODES_PER_CHUNK,sb_inopblock).
				 */
				if (XFS_INO_TO_FSB(mp, mp->m_sb.sb_rootino)
						== XFS_INO_TO_FSB(mp, lino) ||
				    (agno == root_agno &&
				     agino < root_agino + XFS_INODES_PER_CHUNK)) {
					use_rbuf = 1;
					bp = rootino_bp;
					dino = XFS_MAKE_IPTR(mp, bp, agino -
						XFS_INO_TO_AGINO(mp,
							mp->m_sb.sb_rootino));
				} else  {
					len = (int)XFS_FSB_TO_BB(mp,
						MAX(1, XFS_INODES_PER_CHUNK/
							inodes_per_block));
					bp = read_buf(mp->m_dev,
						XFS_AGB_TO_DADDR(mp, agno,
						    XFS_AGINO_TO_AGBNO(mp,
							irec->ino_startnum)),
						len, BUF_TRYLOCK);
					if (bp == NULL || geterror(bp))
						do_error(
						"couldn't read %s inode %llu\n",
							ORPHANAGE, lino);

					/*
					 * get the agbno containing the first
					 * inode in the chunk.  In multi-block
					 * chunks, this gets us the offset
					 * relative to the beginning of a
					 * properly aligned buffer.  In
					 * multi-chunk blocks, this gets us
					 * the correct block number.  Then
					 * turn the block number back into
					 * an agino and calculate the offset
					 * from there to feed to make the iptr.
					 * the last term in effect rounds down
					 * to the first agino in the buffer.
					 */
					dino = XFS_MAKE_IPTR(mp, bp,
						agino - XFS_OFFBNO_TO_AGINO(mp,
							XFS_AGINO_TO_AGBNO(mp,
							irec->ino_startnum),
							0));
				}

				do_warn(
			"        - clearing existing \"%s\" inode\n",
					ORPHANAGE);

				ino_dirty = clear_dinode(mp, dino, lino);

				if (!use_rbuf)  {
					assert(ino_dirty == 0 ||
						ino_dirty && !no_modify);

					if (ino_dirty && !no_modify)
						bwrite(bp);
					else
						brelse(bp);
				} else  {
					if (ino_dirty)
						*rbuf_dirty = 1;
				}
				
				if (inode_isadir(irec, ino_offset))
					clear_inode_isadir(irec, ino_offset);

				set_inode_free(irec, ino_offset);

			}

			/*
			 * regardless of whether the inode num is good or
			 * bad, mark the entry to be junked so the
			 * createname in phase 6 will succeed.
			 */
			namest->name[0] = '/';
			*dirty = 1;
			do_warn(
			"        - marking entry \"%s\" to be deleted\n",
						fname);
			res++;
		}
	}

	return(res);
}

int
longform_delete_orphanage(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_dinode_t	*dino,
			buf_t		*rootino_bp,
			int		*rbuf_dirty)
{
	xfs_dir_leafblock_t	*leaf;
	buf_t			*bp;
	xfs_dfsbno_t		fsbno;
	xfs_dablk_t		da_bno;
	int			dirty;
	int			res;

	da_bno = 0;
	*rbuf_dirty = 0;

	if ((fsbno = get_first_dblock_fsbno(mp, ino, dino)) == NULLDFSBNO)  {
		do_error(
		"couldn't map first leaf block of directory inode %llu\n",
			ino);
		exit(1);
	}

	/*
	 * cycle through the entire directory looking to delete
	 * every "lost+found" entry.  make sure to catch duplicate
	 * entries.
	 *
	 * We could probably speed this up by doing a smarter lookup
	 * to get us to the first block that contains the hashvalue
	 * of "lost+found" but what the heck.  that would require a
	 * double lookup for each level.  and how big can '/' get???
	 * It's probably not worth it.
	 */
	res = 0;

	do {
		assert(fsbno != NULLDFSBNO);
		bp = read_buf(mp->m_dev, (daddr_t)XFS_FSB_TO_DADDR(mp, fsbno),
				XFS_FSB_TO_BB(mp, 1), BUF_TRYLOCK);

		if (bp == NULL || geterror(bp)) {
			do_error(
		"can't read block %u (fsbno %llu) for directory inode %llu\n",
					da_bno, fsbno, ino);
			exit(1);
		}

		leaf = (xfs_dir_leafblock_t *)bp->b_un.b_addr;

		if (leaf->hdr.info.magic != XFS_DIR_LEAF_MAGIC)  {
			do_error(
	"bad magic # (0x%x) for directory leaf block (bno %u fsbno %llu)\n",
				leaf->hdr.info.magic, da_bno, fsbno);
			exit(1);
		}

		da_bno = leaf->hdr.info.forw;

		res += lf_block_delete_orphanage(mp, ino, leaf, &dirty,
					rootino_bp, rbuf_dirty);

		assert(dirty == 0 || dirty && !no_modify);

		if (dirty && !no_modify)
			bwrite(bp);
		else
			brelse(bp);

		if (da_bno != 0)
			fsbno = get_bmapi(mp, dino, ino, da_bno,XFS_DATA_FORK);

	} while (da_bno != 0);

	return(res);
}

/*
 * returns 1 if a deletion happened, 0 otherwise.
 */
/* ARGSUSED */
int
shortform_delete_orphanage(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_dinode_t	*root_dino,
			buf_t		*rootino_bp,
			int		*ino_dirty)
{
	xfs_dir_shortform_t	*sf;
	xfs_dinode_t		*dino;
	xfs_dir_sf_entry_t	*sf_entry, *next_sfe, *tmp_sfe;
	buf_t			*bp;
	xfs_ino_t		lino;
	xfs_agino_t		agino;
	xfs_agino_t		root_agino;
	int			max_size;
	xfs_agnumber_t		agno;
	xfs_agnumber_t		root_agno;
	int			ino_dir_size;
	ino_tree_node_t		*irec;
	int			ino_offset;
	int			i;
	int			dirty;
	int			tmp_len;
	int			tmp_elen;
	int			len;
	int			use_rbuf;
	char			fname[MAXNAMELEN + 1];
	int			res;

	sf = &root_dino->di_u.di_dirsf;
	*ino_dirty = 0;
	res = 0;
	irec = NULL;
	ino_dir_size = root_dino->di_core.di_size;
	max_size = XFS_DFORK_DSIZE(root_dino, mp);
	use_rbuf = 0;
	root_agno = XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino);
	root_agino = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rootino);

	/*
	 * run through entries looking for "lost+found".
	 */
	sf_entry = next_sfe = &sf->list[0];
	for (i = 0; i < sf->hdr.count && ino_dir_size >
			(__psint_t)next_sfe - (__psint_t)sf; i++)  {
		tmp_sfe = NULL;
		sf_entry = next_sfe;
		XFS_DIR_SF_GET_DIRINO(&sf_entry->inumber, &lino);
		bcopy(sf_entry->name, fname, sf_entry->namelen);
		fname[sf_entry->namelen] = '\0';

		if (!strcmp(ORPHANAGE, fname))  {
			agno = XFS_INO_TO_AGNO(mp, lino);
			agino = XFS_INO_TO_AGINO(mp, lino);

			irec = find_inode_rec(agno, agino);

			/*
			 * if the orphange inode is in the tree,
			 * get it, clear it, and mark it free.
			 * the inodes in the orphanage will get
			 * reattached to the new orphanage.
			 */
			if (irec != NULL)  {
				do_warn(
				"        - clearing existing \"%s\" inode\n",
					ORPHANAGE);

				ino_offset = agino - irec->ino_startnum;

				/*
				 * check if we have to use the root inode
				 * buffer or read one in ourselves.  Note
				 * that the root inode is always the first
				 * inode of the chunk that it's in so there
				 * are two possible cases where lost+found
				 * might be in the same buffer as the root
				 * inode.  One case is a large block
				 * filesystem where the two inodes are
				 * in different inode chunks but wind
				 * up in the same block (multiple chunks
				 * per block) and the second case (one or
				 * more blocks per chunk) is where the two
				 * inodes are in the same chunk. Note that
				 * inodes are allocated on disk in units
				 * of MAX(XFS_INODES_PER_CHUNK,sb_inopblock).
				 */
				if (XFS_INO_TO_FSB(mp, mp->m_sb.sb_rootino)
						== XFS_INO_TO_FSB(mp, lino) ||
				    (agno == root_agno &&
				     agino < root_agino + XFS_INODES_PER_CHUNK)) {
					use_rbuf = 1;
					bp = rootino_bp;

					dino = XFS_MAKE_IPTR(mp, bp, agino -
						XFS_INO_TO_AGINO(mp,
							mp->m_sb.sb_rootino));
				} else  {
					len = (int)XFS_FSB_TO_BB(mp,
						MAX(1, XFS_INODES_PER_CHUNK/
							inodes_per_block));
					bp = read_buf(mp->m_dev,
						XFS_AGB_TO_DADDR(mp, agno,
						    XFS_AGINO_TO_AGBNO(mp,
							irec->ino_startnum)),
						len, BUF_TRYLOCK);
					if (bp == NULL || geterror(bp))
						do_error(
					"could not read %s inode %llu\n",
							ORPHANAGE, lino);
					/*
					 * get the agbno containing the first
					 * inode in the chunk.  In multi-block
					 * chunks, this gets us the offset
					 * relative to the beginning of a
					 * properly aligned buffer.  In
					 * multi-chunk blocks, this gets us
					 * the correct block number.  Then
					 * turn the block number back into
					 * an agino and calculate the offset
					 * from there to feed to make the iptr.
					 * the last term in effect rounds down
					 * to the first agino in the buffer.
					 */
					dino = XFS_MAKE_IPTR(mp, bp,
						agino - XFS_OFFBNO_TO_AGINO(mp,
							XFS_AGINO_TO_AGBNO(mp,
							irec->ino_startnum),
							0));
				}

				dirty = clear_dinode(mp, dino, lino);

				assert(dirty == 0 || dirty && !no_modify);

				/*
				 * if we read the lost+found inode in to
				 * it, get rid of it here.  if the lost+found
				 * inode is in the root inode buffer, the
				 * buffer will be marked dirty anyway since
				 * the lost+found entry in the root inode is
				 * also being deleted which makes the root
				 * inode buffer automatically dirty.
				 */
				if (!use_rbuf)  {
					dino = NULL;
					if (dirty && !no_modify)
						bwrite(bp);
					else
						brelse(bp);
				}
				

				if (inode_isadir(irec, ino_offset))
					clear_inode_isadir(irec, ino_offset);

				set_inode_free(irec, ino_offset);
			}

			do_warn("        - deleting existing \"%s\" entry\n",
				ORPHANAGE);

			/*
			 * note -- exactly the same deletion code as in
			 * process_shortform_dir()
			 */
			tmp_elen = XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry);
			root_dino->di_core.di_size -= tmp_elen;

			tmp_sfe = (xfs_dir_sf_entry_t *)
				((__psint_t) sf_entry + tmp_elen);
			tmp_len = max_size - ((__psint_t) tmp_sfe
					- (__psint_t) sf);

			memmove(sf_entry, tmp_sfe, tmp_len);

			sf->hdr.count--;

			bzero((void *) ((__psint_t) sf_entry + tmp_len),
				tmp_elen);

			/*
			 * set the tmp value to the current
			 * pointer so we'll process the entry
			 * we just moved up
			 */
			tmp_sfe = sf_entry;

			/*
			 * WARNING:  drop the index i by one
			 * so it matches the decremented count for
			 * accurate comparisons in the loop test.
			 * mark root inode as dirty to make deletion
			 * permanent.
			 */
			i--;

			*ino_dirty = 1;
			res++;

		}
		next_sfe = (tmp_sfe == NULL)
			? (xfs_dir_sf_entry_t *) ((__psint_t) sf_entry +
				XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry))
			: tmp_sfe;
	}

	return(res);
}

#if VERS >= V_654
/* ARGSUSED */
int
lf2_block_delete_orphanage(xfs_mount_t		*mp,
			xfs_ino_t		ino,
			xfs_dir2_data_t		*data,
			int			*dirty,
			buf_t			*rootino_bp,
			int			*rbuf_dirty)
{
	xfs_dinode_t		*dino;
	buf_t			*bp;
	ino_tree_node_t		*irec;
	xfs_ino_t		lino;
	xfs_agino_t		agino;
	xfs_agnumber_t		agno;
	xfs_agino_t		root_agino;
	xfs_agnumber_t		root_agno;
	int			ino_offset;
	int			ino_dirty;
	int			use_rbuf;
	int			len;
	char			fname[MAXNAMELEN + 1];
	int			res;
	char			*ptr;
	char			*endptr;
	xfs_dir2_block_tail_t	*btp;
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;

	ptr = (char *)data->u;
	if (data->hdr.magic == XFS_DIR2_BLOCK_MAGIC) {
		btp = XFS_DIR2_BLOCK_TAIL_P(mp, (xfs_dir2_block_t *)data);
		endptr = (char *)XFS_DIR2_BLOCK_LEAF_P(btp);
	} else
		endptr = (char *)data + mp->m_dirblksize;
	*dirty = 0;
	use_rbuf = 0;
	res = 0;
	root_agno = XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino);
	root_agino = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rootino);

	while (ptr < endptr) {
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (dup->freetag == XFS_DIR2_DATA_FREE_TAG) {
			if (ptr + dup->length > endptr || dup->length == 0 ||
			    (dup->length & (XFS_DIR2_DATA_ALIGN - 1)))
				break;
			ptr += dup->length;
			continue;
		}
		dep = (xfs_dir2_data_entry_t *)ptr;
		lino = dep->inumber;
		bcopy(dep->name, fname, dep->namelen);
		fname[dep->namelen] = '\0';

		if (fname[0] != '/' && !strcmp(fname, ORPHANAGE))  {
			agino = XFS_INO_TO_AGINO(mp, lino);
			agno = XFS_INO_TO_AGNO(mp, lino);

			old_orphanage_ino = lino;

			irec = find_inode_rec(agno, agino);

			/*
			 * if the orphange inode is in the tree,
			 * get it, clear it, and mark it free.
			 * the inodes in the orphanage will get
			 * reattached to the new orphanage.
			 */
			if (irec != NULL)  {
				ino_offset = agino - irec->ino_startnum;

				/*
				 * check if we have to use the root inode
				 * buffer or read one in ourselves.  Note
				 * that the root inode is always the first
				 * inode of the chunk that it's in so there
				 * are two possible cases where lost+found
				 * might be in the same buffer as the root
				 * inode.  One case is a large block
				 * filesystem where the two inodes are
				 * in different inode chunks but wind
				 * up in the same block (multiple chunks
				 * per block) and the second case (one or
				 * more blocks per chunk) is where the two
				 * inodes are in the same chunk. Note that
				 * inodes are allocated on disk in units
				 * of MAX(XFS_INODES_PER_CHUNK,sb_inopblock).
				 */
				if (XFS_INO_TO_FSB(mp, mp->m_sb.sb_rootino)
						== XFS_INO_TO_FSB(mp, lino) ||
				    (agno == root_agno &&
				     agino < root_agino + XFS_INODES_PER_CHUNK)) {
					use_rbuf = 1;
					bp = rootino_bp;
					dino = XFS_MAKE_IPTR(mp, bp, agino -
						XFS_INO_TO_AGINO(mp,
							mp->m_sb.sb_rootino));
				} else  {
					len = (int)XFS_FSB_TO_BB(mp,
						MAX(1, XFS_INODES_PER_CHUNK/
							inodes_per_block));
					bp = read_buf(mp->m_dev,
						XFS_AGB_TO_DADDR(mp, agno,
						    XFS_AGINO_TO_AGBNO(mp,
							irec->ino_startnum)),
						len, BUF_TRYLOCK);
					if (bp == NULL || geterror(bp))
						do_error(
						"couldn't read %s inode %llu\n",
							ORPHANAGE, lino);

					/*
					 * get the agbno containing the first
					 * inode in the chunk.  In multi-block
					 * chunks, this gets us the offset
					 * relative to the beginning of a
					 * properly aligned buffer.  In
					 * multi-chunk blocks, this gets us
					 * the correct block number.  Then
					 * turn the block number back into
					 * an agino and calculate the offset
					 * from there to feed to make the iptr.
					 * the last term in effect rounds down
					 * to the first agino in the buffer.
					 */
					dino = XFS_MAKE_IPTR(mp, bp,
						agino - XFS_OFFBNO_TO_AGINO(mp,
							XFS_AGINO_TO_AGBNO(mp,
							irec->ino_startnum),
							0));
				}

				do_warn(
			"        - clearing existing \"%s\" inode\n",
					ORPHANAGE);

				ino_dirty = clear_dinode(mp, dino, lino);

				if (!use_rbuf)  {
					assert(ino_dirty == 0 ||
						ino_dirty && !no_modify);

					if (ino_dirty && !no_modify)
						bwrite(bp);
					else
						brelse(bp);
				} else  {
					if (ino_dirty)
						*rbuf_dirty = 1;
				}
				
				if (inode_isadir(irec, ino_offset))
					clear_inode_isadir(irec, ino_offset);

				set_inode_free(irec, ino_offset);

			}

			/*
			 * regardless of whether the inode num is good or
			 * bad, mark the entry to be junked so the
			 * createname in phase 6 will succeed.
			 */
			dep->name[0] = '/';
			*dirty = 1;
			do_warn(
			"        - marking entry \"%s\" to be deleted\n",
						fname);
			res++;
		}
		ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
	}

	return(res);
}

int
longform2_delete_orphanage(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_dinode_t	*dino,
			buf_t		*rootino_bp,
			int		*rbuf_dirty)
{
	xfs_dir2_data_t		*data;
	xfs_dabuf_t		*bp;
	xfs_dfsbno_t		fsbno;
	xfs_dablk_t		da_bno;
	int			dirty;
	int			res;
	bmap_ext_t		*bmp;
	int			i;

	da_bno = 0;
	*rbuf_dirty = 0;
	bmp = malloc(mp->m_dirblkfsbs * sizeof(*bmp));

	/*
	 * cycle through the entire directory looking to delete
	 * every "lost+found" entry.  make sure to catch duplicate
	 * entries.
	 *
	 * We could probably speed this up by doing a smarter lookup
	 * to get us to the first block that contains the hashvalue
	 * of "lost+found" but what the heck.  that would require a
	 * double lookup for each level.  and how big can '/' get???
	 * It's probably not worth it.
	 */
	res = 0;

	for (da_bno = 0;
	     da_bno < XFS_B_TO_FSB(mp, dino->di_core.di_size);
	     da_bno += mp->m_dirblkfsbs) {
		for (i = 0; i < mp->m_dirblkfsbs; i++) {
			fsbno = get_bmapi(mp, dino, ino, da_bno + i,
					  XFS_DATA_FORK);
			if (fsbno == NULLDFSBNO)
				break;
			bmp[i].startoff = da_bno + i;
			bmp[i].startblock = fsbno;
			bmp[i].blockcount = 1;
			bmp[i].flag = 0;
		}
		if (fsbno == NULLDFSBNO)
			continue;
		bp = da_read_buf(mp, BUF_TRYLOCK, mp->m_dirblkfsbs, bmp);
		if (bp == NULL) {
			do_error(
		"can't read block %u (fsbno %llu) for directory inode %llu\n",
					da_bno, bmp[0].startblock, ino);
			exit(1);
		}

		data = (xfs_dir2_data_t *)bp->data;

		if (data->hdr.magic != XFS_DIR2_DATA_MAGIC &&
		    data->hdr.magic != XFS_DIR2_BLOCK_MAGIC)  {
			do_error(
	"bad magic # (0x%x) for directory data block (bno %u fsbno %llu)\n",
				data->hdr.magic, da_bno, bmp[0].startblock);
			exit(1);
		}

		res += lf2_block_delete_orphanage(mp, ino, data, &dirty,
					rootino_bp, rbuf_dirty);

		assert(dirty == 0 || dirty && !no_modify);

		if (dirty && !no_modify)
			da_bwrite(bp);
		else
			da_brelse(bp);
	}
	free(bmp);

	return(res);
}

/*
 * returns 1 if a deletion happened, 0 otherwise.
 */
/* ARGSUSED */
int
shortform2_delete_orphanage(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_dinode_t	*root_dino,
			buf_t		*rootino_bp,
			int		*ino_dirty)
{
	xfs_dir2_sf_t		*sf;
	xfs_dinode_t		*dino;
	xfs_dir2_sf_entry_t	*sf_entry, *next_sfe, *tmp_sfe;
	buf_t			*bp;
	xfs_ino_t		lino;
	xfs_agino_t		agino;
	xfs_agino_t		root_agino;
	int			max_size;
	xfs_agnumber_t		agno;
	xfs_agnumber_t		root_agno;
	int			ino_dir_size;
	ino_tree_node_t		*irec;
	int			ino_offset;
	int			i;
	int			dirty;
	int			tmp_len;
	int			tmp_elen;
	int			len;
	int			use_rbuf;
	char			fname[MAXNAMELEN + 1];
	int			res;

	sf = &root_dino->di_u.di_dir2sf;
	*ino_dirty = 0;
	irec = NULL;
	ino_dir_size = root_dino->di_core.di_size;
	max_size = XFS_DFORK_DSIZE(root_dino, mp);
	use_rbuf = 0;
	res = 0;
	root_agno = XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino);
	root_agino = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rootino);

	/*
	 * run through entries looking for "lost+found".
	 */
	sf_entry = next_sfe = XFS_DIR2_SF_FIRSTENTRY(sf);
	for (i = 0; i < sf->hdr.count && ino_dir_size >
			(__psint_t)next_sfe - (__psint_t)sf; i++)  {
		tmp_sfe = NULL;
		sf_entry = next_sfe;
		lino = XFS_DIR2_SF_GET_INUMBER(sf,
			XFS_DIR2_SF_INUMBERP(sf_entry));
		bcopy(sf_entry->name, fname, sf_entry->namelen);
		fname[sf_entry->namelen] = '\0';

		if (!strcmp(ORPHANAGE, fname))  {
			agno = XFS_INO_TO_AGNO(mp, lino);
			agino = XFS_INO_TO_AGINO(mp, lino);

			irec = find_inode_rec(agno, agino);

			/*
			 * if the orphange inode is in the tree,
			 * get it, clear it, and mark it free.
			 * the inodes in the orphanage will get
			 * reattached to the new orphanage.
			 */
			if (irec != NULL)  {
				do_warn(
				"        - clearing existing \"%s\" inode\n",
					ORPHANAGE);

				ino_offset = agino - irec->ino_startnum;

				/*
				 * check if we have to use the root inode
				 * buffer or read one in ourselves.  Note
				 * that the root inode is always the first
				 * inode of the chunk that it's in so there
				 * are two possible cases where lost+found
				 * might be in the same buffer as the root
				 * inode.  One case is a large block
				 * filesystem where the two inodes are
				 * in different inode chunks but wind
				 * up in the same block (multiple chunks
				 * per block) and the second case (one or
				 * more blocks per chunk) is where the two
				 * inodes are in the same chunk. Note that
				 * inodes are allocated on disk in units
				 * of MAX(XFS_INODES_PER_CHUNK,sb_inopblock).
				 */
				if (XFS_INO_TO_FSB(mp, mp->m_sb.sb_rootino)
						== XFS_INO_TO_FSB(mp, lino) ||
				    (agno == root_agno &&
				     agino < root_agino + XFS_INODES_PER_CHUNK)) {
					use_rbuf = 1;
					bp = rootino_bp;

					dino = XFS_MAKE_IPTR(mp, bp, agino -
						XFS_INO_TO_AGINO(mp,
							mp->m_sb.sb_rootino));
				} else  {
					len = (int)XFS_FSB_TO_BB(mp,
						MAX(1, XFS_INODES_PER_CHUNK/
							inodes_per_block));
					bp = read_buf(mp->m_dev,
						XFS_AGB_TO_DADDR(mp, agno,
						    XFS_AGINO_TO_AGBNO(mp,
							irec->ino_startnum)),
						len, BUF_TRYLOCK);
					if (bp == NULL || geterror(bp))
						do_error(
					"could not read %s inode %llu\n",
							ORPHANAGE, lino);
					/*
					 * get the agbno containing the first
					 * inode in the chunk.  In multi-block
					 * chunks, this gets us the offset
					 * relative to the beginning of a
					 * properly aligned buffer.  In
					 * multi-chunk blocks, this gets us
					 * the correct block number.  Then
					 * turn the block number back into
					 * an agino and calculate the offset
					 * from there to feed to make the iptr.
					 * the last term in effect rounds down
					 * to the first agino in the buffer.
					 */
					dino = XFS_MAKE_IPTR(mp, bp,
						agino - XFS_OFFBNO_TO_AGINO(mp,
							XFS_AGINO_TO_AGBNO(mp,
							irec->ino_startnum),
							0));
				}

				dirty = clear_dinode(mp, dino, lino);

				assert(dirty == 0 || dirty && !no_modify);

				/*
				 * if we read the lost+found inode in to
				 * it, get rid of it here.  if the lost+found
				 * inode is in the root inode buffer, the
				 * buffer will be marked dirty anyway since
				 * the lost+found entry in the root inode is
				 * also being deleted which makes the root
				 * inode buffer automatically dirty.
				 */
				if (!use_rbuf)  {
					dino = NULL;
					if (dirty && !no_modify)
						bwrite(bp);
					else
						brelse(bp);
				}
				

				if (inode_isadir(irec, ino_offset))
					clear_inode_isadir(irec, ino_offset);

				set_inode_free(irec, ino_offset);
			}

			do_warn("        - deleting existing \"%s\" entry\n",
				ORPHANAGE);

			/*
			 * note -- exactly the same deletion code as in
			 * process_shortform_dir()
			 */
			tmp_elen = XFS_DIR2_SF_ENTSIZE_BYENTRY(sf, sf_entry);
			root_dino->di_core.di_size -= tmp_elen;

			tmp_sfe = (xfs_dir2_sf_entry_t *)
				((__psint_t) sf_entry + tmp_elen);
			tmp_len = max_size - ((__psint_t) tmp_sfe
					- (__psint_t) sf);

			memmove(sf_entry, tmp_sfe, tmp_len);

			sf->hdr.count--;
			if (lino > XFS_DIR2_MAX_SHORT_INUM)
				sf->hdr.i8count--;

			bzero((void *) ((__psint_t) sf_entry + tmp_len),
				tmp_elen);

			/*
			 * set the tmp value to the current
			 * pointer so we'll process the entry
			 * we just moved up
			 */
			tmp_sfe = sf_entry;

			/*
			 * WARNING:  drop the index i by one
			 * so it matches the decremented count for
			 * accurate comparisons in the loop test.
			 * mark root inode as dirty to make deletion
			 * permanent.
			 */
			i--;

			*ino_dirty = 1;

			res++;
		}
		next_sfe = (tmp_sfe == NULL)
			? (xfs_dir2_sf_entry_t *) ((__psint_t) sf_entry +
				XFS_DIR2_SF_ENTSIZE_BYENTRY(sf, sf_entry))
			: tmp_sfe;
	}

	return(res);
}
#endif	/* VERS >= V_654 */

void
delete_orphanage(xfs_mount_t *mp)
{
	xfs_ino_t ino;
	xfs_dinode_t *dino;
	buf_t *dbp;
	int dirty, res, len;

	assert(!no_modify);

	dbp = NULL;
	ino = mp->m_sb.sb_rootino;

	/*
	 * we know the root is in use or we wouldn't be here
	 */
	len = (int)XFS_FSB_TO_BB(mp,
			MAX(1, XFS_INODES_PER_CHUNK/inodes_per_block));
	dbp = read_buf(mp->m_dev,
		XFS_FSB_TO_DADDR(mp, XFS_INO_TO_FSB(mp, ino)),
		len, BUF_TRYLOCK);

	if (dbp == NULL || geterror(dbp))  {
		do_error(
	"could not read buffer for root inode %llu (daddr %lld, size %d)\n",
			ino,
			XFS_FSB_TO_DADDR(mp, XFS_INO_TO_FSB(mp, ino)),
			XFS_FSB_TO_BB(mp, 1));
	}

	/*
	 * we also know that the root inode is always the first inode
	 * allocated in the system, therefore it'll be at the beginning
	 * of the root inode chunk
	 */
	dino = XFS_MAKE_IPTR(mp, dbp, 0);

	switch (dino->di_core.di_format)  {
	case XFS_DINODE_FMT_EXTENTS:
	case XFS_DINODE_FMT_BTREE:
#if VERS >= V_654
		if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
			res = longform2_delete_orphanage(mp, ino, dino, dbp,
				&dirty);
		else
#endif
			res = longform_delete_orphanage(mp, ino, dino, dbp,
				&dirty);
		break;
	case XFS_DINODE_FMT_LOCAL:
#if VERS >= V_654
		if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
			res = shortform2_delete_orphanage(mp, ino, dino, dbp,
				&dirty);
		else
#endif
			res = shortform_delete_orphanage(mp, ino, dino, dbp,
				&dirty);
		assert(res == 0 && dirty == 0 || res == 1 && dirty == 1);
		break;
	default:
		break;
	}

	if (res)  {
		switch (dino->di_core.di_version)  {
		case XFS_DINODE_VERSION_1:
			dino->di_core.di_onlink--;
			dino->di_core.di_nlink = dino->di_core.di_onlink;
			break;
		case XFS_DINODE_VERSION_2:
			dino->di_core.di_nlink--;
			break;
		default:
			do_error("unknown version #%d in root inode\n",
					dino->di_core.di_version);
		}

		dirty = 1;
	}

	if (dirty)
		bwrite(dbp);
	else
		brelse(dbp);

	return;
}

/*
 * null out quota inode fields in sb if they point to non-existent inodes.
 * this isn't as redundant as it looks since it's possible that the sb field
 * might be set but the imap and inode(s) agree that the inode is
 * free in which case they'd never be cleared so the fields wouldn't
 * be cleared by process_dinode().
 */
void
quotino_check(xfs_mount_t *mp)
{
	ino_tree_node_t *irec;

	if (mp->m_sb.sb_uquotino != NULLFSINO && mp->m_sb.sb_uquotino != 0)  {
		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_uquotino),
			XFS_INO_TO_AGINO(mp, mp->m_sb.sb_uquotino));

		if (irec == NULL || is_inode_free(irec,
				mp->m_sb.sb_uquotino - irec->ino_startnum))  {
			mp->m_sb.sb_uquotino = NULLFSINO;
			lost_uquotino = 1;
		} else
			lost_uquotino = 0;
	}

	if (mp->m_sb.sb_pquotino != NULLFSINO && mp->m_sb.sb_pquotino != 0)  {
		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_pquotino),
			XFS_INO_TO_AGINO(mp, mp->m_sb.sb_pquotino));

		if (irec == NULL || is_inode_free(irec,
				mp->m_sb.sb_pquotino - irec->ino_startnum))  {
			mp->m_sb.sb_pquotino = NULLFSINO;
			lost_pquotino = 1;
		} else
			lost_pquotino = 0;
	}

	return;
}

void
quota_sb_check(xfs_mount_t *mp)
{
	/*
	 * if the sb says we have quotas and we lost both,
	 * signal a superblock downgrade.  that will cause
	 * the quota flags to get zeroed.  (if we only lost
	 * one quota inode, do nothing and complain later.)
	 *
	 * if the sb says we have quotas but we didn't start out
	 * with any quota inodes, signal a superblock downgrade.
	 *
	 * The sb downgrades are so that older systems can mount
	 * the filesystem.
	 *
	 * if the sb says we don't have quotas but it looks like
	 * we do have quota inodes, then signal a superblock upgrade.
	 *
	 * if the sb says we don't have quotas and we have no
	 * quota inodes, then leave will enough alone.
	 */

	if (fs_quotas &&
	    (mp->m_sb.sb_uquotino == NULLFSINO || mp->m_sb.sb_uquotino == 0) &&
	    (mp->m_sb.sb_pquotino == NULLFSINO || mp->m_sb.sb_pquotino == 0))  {
		lost_quotas = 1;
		fs_quotas = 0;
	} else if (!verify_inum(mp, mp->m_sb.sb_uquotino) &&
			!verify_inum(mp, mp->m_sb.sb_uquotino))  {
		fs_quotas = 1;
	}

	return;
}


void
phase4(xfs_mount_t *mp)
{
	ino_tree_node_t		*irec;
	xfs_drtbno_t		bno;
	xfs_drtbno_t		rt_start;
	xfs_extlen_t		rt_len;
	xfs_agnumber_t		i;
	xfs_agblock_t		j;
	xfs_agblock_t		ag_end;
	xfs_agblock_t		extent_start;
	xfs_extlen_t		extent_len;
	int			ag_hdr_len = 4 * mp->m_sb.sb_sectsize;
	int			ag_hdr_block;
	int			bstate;
	int			count_bcnt_extents(xfs_agnumber_t agno);
	int			count_bno_extents(xfs_agnumber_t agno);
	
	ag_hdr_block = howmany(ag_hdr_len, mp->m_sb.sb_blocksize);

	printf("Phase 4 - check for duplicate blocks...\n");
	printf("        - setting up duplicate extent list...\n");

	irec = find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino),
				XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rootino));

	/*
	 * we always have a root inode, even if it's free...
	 * if the root is free, forget it, lost+found is already gone
	 */
	if (is_inode_free(irec, 0) || !inode_isadir(irec, 0))  {
		need_root_inode = 1;
		if (no_modify)
			do_warn("root inode would be lost\n");
		else
			do_warn("root inode lost\n");
	}

	/*
	 * have to delete lost+found first so that blocks used
	 * by lost+found don't show up as used
	 */
	if (!no_modify)  {
		printf("        - clear lost+found (if it exists) ...\n");
		if (!need_root_inode)
			delete_orphanage(mp);
	}

	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		ag_end = (i < mp->m_sb.sb_agcount - 1) ? mp->m_sb.sb_agblocks :
			mp->m_sb.sb_dblocks -
				(xfs_drfsbno_t) mp->m_sb.sb_agblocks * i;
		extent_start = extent_len = 0;
		/*
		 * set up duplicate extent list for this ag
		 */
		for (j = ag_hdr_block; j < ag_end; j++)  {

			bstate = get_agbno_state(mp, i, j);

			switch (bstate)  {
			case XR_E_BAD_STATE:
			default:
				do_warn("unknown block state, ag %d, \
block %d\n",
					i, j);
				/* fall through .. */
			case XR_E_UNKNOWN:
			case XR_E_FREE1:
			case XR_E_FREE:
			case XR_E_INUSE:
			case XR_E_INUSE_FS:
			case XR_E_INO:
			case XR_E_FS_MAP:
				if (extent_start == 0)
					continue;
				else  {
					/*
					 * add extent and reset extent state
					 */
					add_dup_extent(i, extent_start,
							extent_len);
					extent_start = 0;
					extent_len = 0;
				}
				break;
			case XR_E_MULT:
				if (extent_start == 0)  {
					extent_start = j;
					extent_len = 1;
				} else if (extent_len == MAXEXTLEN)  {
					add_dup_extent(i, extent_start,
							extent_len);
					extent_start = j;
					extent_len = 1;
				} else
					extent_len++;
				break;
			}
		}
		/*
		 * catch tail-case, extent hitting the end of the ag
		 */
		if (extent_start != 0)
			add_dup_extent(i, extent_start, extent_len);
	}

	/*
	 * initialize realtime bitmap
	 */
	rt_start = 0;
	rt_len = 0;

	for (bno = 0; bno < mp->m_sb.sb_rextents; bno++)  {

		bstate = get_rtbno_state(mp, bno);

		switch (bstate)  {
		case XR_E_BAD_STATE:
		default:
			do_warn("unknown rt extent state, extent %llu\n", bno);
			/* fall through .. */
		case XR_E_UNKNOWN:
		case XR_E_FREE1:
		case XR_E_FREE:
		case XR_E_INUSE:
		case XR_E_INUSE_FS:
		case XR_E_INO:
		case XR_E_FS_MAP:
			if (rt_start == 0)
				continue;
			else  {
				/*
				 * add extent and reset extent state
				 */
				add_rt_dup_extent(rt_start, rt_len);
				rt_start = 0;
				rt_len = 0;
			}
			break;
		case XR_E_MULT:
			if (rt_start == 0)  {
				rt_start = bno;
				rt_len = 1;
			} else if (rt_len == MAXEXTLEN)  {
				/*
				 * large extent case
				 */
				add_rt_dup_extent(rt_start, rt_len);
				rt_start = bno;
				rt_len = 1;
			} else
				rt_len++;
			break;
		}
	}

	/*
	 * catch tail-case, extent hitting the end of the ag
	 */
	if (rt_start != 0)
		add_rt_dup_extent(rt_start, rt_len);

	/*
	 * initialize bitmaps for all AGs
	 */
	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		ag_end = (i < mp->m_sb.sb_agcount - 1) ? mp->m_sb.sb_agblocks :
			mp->m_sb.sb_dblocks -
				(xfs_drfsbno_t) mp->m_sb.sb_agblocks * i;
		/*
		 * now reset the bitmap for all ags
		 */
		bzero(ba_bmap[i], roundup(mp->m_sb.sb_agblocks*(NBBY/XR_BB),
						sizeof(__uint64_t)));
		for (j = 0; j < ag_hdr_block; j++)
			set_agbno_state(mp, i, j, XR_E_INUSE_FS);
	}
	set_bmap_rt(mp->m_sb.sb_rextents);
	set_bmap_log(mp);
	set_bmap_fs(mp);

	printf("        - check for inodes claiming duplicate blocks...\n");
	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		/*
		 * ok, now process the inodes -- signal 2-pass check per inode.
		 * first pass checks if the inode conflicts with a known
		 * duplicate extent.  if so, the inode is cleared and second
		 * pass is skipped.  second pass sets the block bitmap
		 * for all blocks claimed by the inode.  directory
		 * and attribute processing is turned OFF since we did that 
		 * already in phase 3.
		 */
		do_log("        - agno = %d\n", i);
		process_aginodes(mp, i, 0, 1, 0);

		/*
		 * now recycle the per-AG duplicate extent records
		 */
		release_dup_extent_tree(i);
	}

	/*
	 * free up memory used to track trealtime duplicate extents
	 */
	if (rt_start != 0)
		free_rt_dup_extent_tree(mp);

	/*
	 * ensure consistency of quota inode pointers in superblock,
	 * make sure they point to real inodes
	 */
	quotino_check(mp);
	quota_sb_check(mp);

	return;
}
