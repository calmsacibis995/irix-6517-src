#ident "$Revision: 1.44 $"

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
#include <sys/cred.h>
#include <sys/uuid.h>
#include <sys/stat.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <bstring.h>
#include <errno.h>
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
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_dir2_sf.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_inode_item.h>
#include <sys/fs/xfs_inode.h>
#include <sys/fs/xfs_bmap.h>
#include <sys/fs/xfs_da_btree.h>
#include <sys/fs/xfs_dir_leaf.h>
#include <sys/fs/xfs_trans_space.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2_leaf.h>
#include <sys/fs/xfs_dir2_data.h>
#include <sys/fs/xfs_dir2_block.h>
#include <sys/fs/xfs_dir2_node.h>
#endif

#include "sim.h"
#include "globals.h"
#include "agheader.h"
#include "incore.h"
#include "dir.h"
#if VERS >= V_654
#include "dir2.h"
#endif
#include "dir_stack.h"
#include "protos.h"
#include "err_protos.h"
#include "dinode.h"
#include "versions.h"

static cred_t zerocr;
static int orphanage_entered;

#if VERS >= V_654
/*
 * Data structures and routines to keep track of directory entries
 * and whether their leaf entry has been seen
 */
typedef struct dir_hash_ent {
	struct dir_hash_ent	*next;	/* pointer to next entry */
	xfs_dir2_leaf_entry_t	ent;	/* address and hash value */
	short			junkit;	/* name starts with / */
	short			seen;	/* have seen leaf entry */
} dir_hash_ent_t;

typedef struct dir_hash_tab {
	int			size;	/* size of hash table */
	dir_hash_ent_t		*tab[1];/* actual hash table, variable size */
} dir_hash_tab_t;
#define	DIR_HASH_TAB_SIZE(n)	\
	(offsetof(dir_hash_tab_t, tab) + (sizeof(dir_hash_ent_t *) * (n)))
#define	DIR_HASH_FUNC(t,a)	((a) % (t)->size)

/*
 * Track the contents of the freespace table in a directory.
 */
typedef struct freetab {
	int			naents;
	int			nents;
	struct freetab_ent {
		xfs_dir2_data_off_t	v;
		short			s;
	} ents[1];
} freetab_t;
#define	FREETAB_SIZE(n)	\
	(offsetof(freetab_t, ents) + (sizeof(struct freetab_ent) * (n)))

#define	DIR_HASH_CK_OK		0
#define	DIR_HASH_CK_DUPLEAF	1
#define	DIR_HASH_CK_BADHASH	2
#define	DIR_HASH_CK_NODATA	3
#define	DIR_HASH_CK_NOLEAF	4
#define	DIR_HASH_CK_BADSTALE	5

static void
dir_hash_add(
	dir_hash_tab_t		*hashtab,
	xfs_dahash_t		hash,
	xfs_dir2_dataptr_t	addr,
	int			junk)
{
	int			i;
	dir_hash_ent_t		*p;

	i = DIR_HASH_FUNC(hashtab, addr);
	p = malloc(sizeof(*p));
	p->next = hashtab->tab[i];
	hashtab->tab[i] = p;
	if (!(p->junkit = junk))
		p->ent.hashval = hash;
	p->ent.address = addr;
	p->seen = 0;
}

static int
dir_hash_unseen(
	dir_hash_tab_t	*hashtab)
{
	int		i;
	dir_hash_ent_t	*p;

	for (i = 0; i < hashtab->size; i++) {
		for (p = hashtab->tab[i]; p; p = p->next) {
			if (p->seen == 0)
				return 1;
		}
	}
	return 0;
}

static int
dir_hash_check(
	dir_hash_tab_t	*hashtab,
	xfs_inode_t	*ip,
	int		seeval)
{
	static char	*seevalstr[] = {
		"ok",
		"duplicate leaf",
		"hash value mismatch",
		"no data entry",
		"no leaf entry",
		"bad stale count",
	};

	if (seeval == DIR_HASH_CK_OK && dir_hash_unseen(hashtab))
		seeval = DIR_HASH_CK_NOLEAF;
	if (seeval == DIR_HASH_CK_OK)
		return 0;
	do_warn("bad hash table for directory inode %llu (%s): ", ip->i_ino,
		seevalstr[seeval]);
	if (!no_modify)
		do_warn("rebuilding\n");
	else
		do_warn("would rebuild\n");
	return 1;
}

static void
dir_hash_done(
	dir_hash_tab_t	*hashtab)
{
	int		i;
	dir_hash_ent_t	*n;
	dir_hash_ent_t	*p;

	for (i = 0; i < hashtab->size; i++) {
		for (p = hashtab->tab[i]; p; p = n) {
			n = p->next;
			free(p);
		}
	}
	free(hashtab);
}

static dir_hash_tab_t *
dir_hash_init(
	xfs_fsize_t	size)
{
	dir_hash_tab_t	*hashtab;
	int		hsize;

	hsize = size / (16 * 4);
	if (hsize > 1024)
		hsize = 1024;
	else if (hsize < 16)
		hsize = 16;
	hashtab = calloc(DIR_HASH_TAB_SIZE(hsize), 1);
	hashtab->size = hsize;
	return hashtab;
}

static int
dir_hash_see(
	dir_hash_tab_t		*hashtab,
	xfs_dahash_t		hash,
	xfs_dir2_dataptr_t	addr)
{
	int			i;
	dir_hash_ent_t		*p;

	i = DIR_HASH_FUNC(hashtab, addr);
	for (p = hashtab->tab[i]; p; p = p->next) {
		if (p->ent.address != addr)
			continue;
		if (p->seen)
			return DIR_HASH_CK_DUPLEAF;
		if (p->junkit == 0 && p->ent.hashval != hash)
			return DIR_HASH_CK_BADHASH;
		p->seen = 1;
		return DIR_HASH_CK_OK;
	}
	return DIR_HASH_CK_NODATA;
}

static int
dir_hash_see_all(
	dir_hash_tab_t		*hashtab,
	xfs_dir2_leaf_entry_t	*ents,
	int			count,
	int			stale)
{
	int			i;
	int			j;
	int			rval;

	for (i = j = 0; i < count; i++) {
		if (ents[i].address == XFS_DIR2_NULL_DATAPTR) {
			j++;
			continue;
		}
		rval = dir_hash_see(hashtab, ents[i].hashval, ents[i].address);
		if (rval != DIR_HASH_CK_OK)
			return rval;
	}
	return j == stale ? DIR_HASH_CK_OK : DIR_HASH_CK_BADSTALE;
}
#endif /* VERS >= V_654 */

static void
res_failed(
	int	err)
{
	if (err == ENOSPC) {
		do_error("ran out of disk space!\n");
	} else
		do_error("xfs_trans_reserve returned %d\n", err);
}

/*
 * wrapper for xfs_trans_commit since the number of args to
 * it changed for 6.5
 */
int
xfsr_trans_commit(xfs_trans_t *tp, uint flags)
{
#if VERS >= V_65
	return(xfs_trans_commit(tp, flags, NULL));
#else
	return(xfs_trans_commit(tp, flags));
#endif
}

/*
 * stolen from xfs_mkfs.c
 *
 * Wrapper around call to xfs_ialloc. Takes care of committing and
 * allocating a new transaction as needed.
 */
static int
ialloc(
	xfs_trans_t	**tp,
	xfs_inode_t	*pip,
	mode_t		mode,
	ushort		nlink,
	dev_t		rdev,
	cred_t		*cr,
	xfs_inode_t	**ipp)
{
	boolean_t    	call_again;
	int		i;
	buf_t        	*ialloc_context;
	xfs_inode_t  	*ip;
	uint		log_count;
	uint		log_res;
	xfs_trans_t	*ntp;
	int		error;

	call_again = B_FALSE;
	ialloc_context = (buf_t *)0;
	error = xfs_ialloc(*tp, pip, mode, nlink, rdev, cr,
#ifdef XFS_QUOTAS
			(xfs_prid_t) 0,
#endif
#if VERS >= V_65
			1,
#endif
			&ialloc_context, &call_again, &ip);
	if (error) {
		return error;
	}
	if (call_again) {
		xfs_trans_bhold(*tp, ialloc_context);
		log_res = xfs_trans_get_log_res(*tp);
		log_count = xfs_trans_get_log_count(*tp);
		ntp = xfs_trans_dup(*tp);
		xfsr_trans_commit(*tp, XFS_TRANS_SYNC);
		*tp = ntp;
		if (i = xfs_trans_reserve(*tp, 0, log_res, 0,
					  XFS_TRANS_PERM_LOG_RES,
					  log_count))
			res_failed(i);
		xfs_trans_bjoin(*tp, ialloc_context);
		error = xfs_ialloc(*tp, pip, mode, nlink, rdev, cr,
#ifdef XFS_QUOTAS
				(xfs_prid_t) 0,
#endif
#if VERS >= V_65
				1,
#endif
				&ialloc_context, &call_again, &ip);
		if (error) {
			return error;
		}
	}
	*ipp = ip;
	return error;
}

void
mk_rbmino(xfs_mount_t *mp)
{
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_bmbt_irec_t	*ep;
	xfs_fsblock_t	first;
	int		i;
	int		nmap;
	int		committed;
	int		error;
	xfs_bmap_free_t	flist;
	xfs_dfiloff_t	bno;
	xfs_bmbt_irec_t	map[XFS_BMAP_MAX_NMAP];

	/*
	 * first set up inode
	 */
	tp = xfs_trans_alloc(mp, 0);

	if (i = xfs_trans_reserve(tp, 10, XFS_ICHANGE_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_MKDIR_LOG_COUNT))
		res_failed(i);

	error = xfs_trans_iget(mp, tp, mp->m_sb.sb_rbmino,
				XFS_ILOCK_EXCL, &ip);
	if (error) {
		do_error("couldn't iget realtime bitmap inode -- error - %d\n",
			error);
	}

	bzero(&ip->i_d, sizeof(xfs_dinode_core_t));

	ip->i_d.di_magic = XFS_DINODE_MAGIC;
	ip->i_d.di_mode = IFREG;
	ip->i_d.di_version = XFS_DINODE_VERSION_1;
	ip->i_d.di_format = XFS_DINODE_FMT_EXTENTS;
	ip->i_d.di_aformat = XFS_DINODE_FMT_EXTENTS;

	ip->i_d.di_nlink = 1;		/* account for sb ptr */

	/*
	 * now the ifork
	 */
	ip->i_df.if_flags = XFS_IFEXTENTS;
	ip->i_df.if_bytes = ip->i_df.if_real_bytes = 0;
	ip->i_df.if_u1.if_extents = NULL;

	ip->i_d.di_size = mp->m_sb.sb_rbmblocks * mp->m_sb.sb_blocksize;

	/*
	 * commit changes
	 */
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	xfs_trans_ihold(tp, ip);
	xfsr_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC);

	/*
	 * then allocate blocks for file and fill with zeroes (stolen
	 * from mkfs)
	 */
	tp = xfs_trans_alloc(mp, 0);
	if (error = xfs_trans_reserve(tp,
				  mp->m_sb.sb_rbmblocks +
				      (XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) - 1),
				  BBTOB(128), 0, XFS_TRANS_PERM_LOG_RES,
				  XFS_DEFAULT_PERM_LOG_COUNT))
		res_failed(error);

	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	bno = 0;
	XFS_BMAP_INIT(&flist, &first);
	while (bno < mp->m_sb.sb_rbmblocks) {
		nmap = XFS_BMAP_MAX_NMAP;
		error = xfs_bmapi(tp, ip, bno,
			  (xfs_extlen_t)(mp->m_sb.sb_rbmblocks - bno),
			  XFS_BMAPI_WRITE, &first, mp->m_sb.sb_rbmblocks,
			  map, &nmap, &flist);
		if (error) {
			do_error("couldn't allocate realtime bitmap - err %d\n",
				error);
		}
		for (i = 0, ep = map; i < nmap; i++, ep++) {
			if (!dev_zero(mp->m_dev,
				      XFS_FSB_TO_DADDR(mp, ep->br_startblock),
				      XFS_FSB_TO_BB(mp, ep->br_blockcount)))
				do_error("dev_zero of rtbitmap failed\n");
			bno += ep->br_blockcount;
		}
	}
	error = xfs_bmap_finish(&tp, &flist, first, &committed);
	if (error) {
		do_error(
		"allocation of the realtime bitmap failed, error = %d\n",
			error);
	}
	xfsr_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC);

	return;
}

int
fill_rbmino(xfs_mount_t *mp)
{
	xfs_ino_t	ino;
	buf_t		*bp;
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_rtword_t	*bmp;
	xfs_fsblock_t	first;
	int		nmap;
	int		error;
	xfs_dfiloff_t	bno;
	xfs_bmbt_irec_t	map;

	bmp = btmcompute;
	bno = 0;

	tp = xfs_trans_alloc(mp, 0);

	if (error = xfs_trans_reserve(tp, 10,
			128 + mp->m_sb.sb_rbmblocks * mp->m_sb.sb_blocksize,
			0, XFS_TRANS_PERM_LOG_RES,
			XFS_DEFAULT_PERM_LOG_COUNT))
		res_failed(error);

	error = xfs_trans_iget(mp, tp, mp->m_sb.sb_rbmino,
				XFS_ILOCK_EXCL, &ip);

	if (error) {
		do_error("couldn't iget realtime bitmap inode -- error - %d\n",
			error);
	}

	while (bno < mp->m_sb.sb_rbmblocks)  {
		/*
		 * fill the file one block at a time
		 */
		nmap = 1;
		error = xfs_bmapi(tp, ip, bno, 1, XFS_BMAPI_WRITE,
					&first, 1, &map, &nmap, NULL);
		if (error || nmap != 1) {
			do_error(
			"couldn't map realtime bitmap block %llu - err %d\n",
				bno, error);
		}

		assert(map.br_startblock != HOLESTARTBLOCK);

		error = xfs_trans_read_buf(
#if VERS >= V_65
				mp,
#endif
				tp, mp->m_dev,
				XFS_FSB_TO_DADDR(mp, map.br_startblock), 
				XFS_FSB_TO_BB(mp, 1), 1, &bp);

		if (error) {
			do_warn(
	"can't access block %llu (fsbno %llu) of realtime bitmap inode %llu\n",
				bno, map.br_startblock, ino);
			return(1);
		}

		bcopy(bmp, bp->b_un.b_addr, mp->m_sb.sb_blocksize);

		xfs_trans_log_buf(tp, bp, 0, mp->m_sb.sb_blocksize - 1);

		bmp = (xfs_rtword_t *)((__psint_t) bmp + mp->m_sb.sb_blocksize);
		bno++;
	}

	xfsr_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC);

	return(0);
}

int
fill_rsumino(xfs_mount_t *mp)
{
	xfs_ino_t	ino;
	buf_t		*bp;
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_suminfo_t	*smp;
	xfs_fsblock_t	first;
	int		nmap;
	int		error;
	xfs_dfiloff_t	bno;
	xfs_dfiloff_t	end_bno;
	xfs_bmbt_irec_t	map;

	smp = sumcompute;
	bno = 0;
	end_bno = mp->m_rsumsize >> mp->m_sb.sb_blocklog;

	tp = xfs_trans_alloc(mp, 0);

	if (error = xfs_trans_reserve(tp, 10, 
			128 + mp->m_rsumsize,
			0, XFS_TRANS_PERM_LOG_RES,
			XFS_DEFAULT_PERM_LOG_COUNT))
		res_failed(error);

	error = xfs_trans_iget(mp, tp, mp->m_sb.sb_rsumino,
				XFS_ILOCK_EXCL, &ip);

	if (error) {
		do_error("couldn't iget realtime summary inode -- error - %d\n",
			error);
	}

	while (bno < end_bno)  {
		/*
		 * fill the file one block at a time
		 */
		nmap = 1;
		error = xfs_bmapi(tp, ip, bno, 1, XFS_BMAPI_WRITE,
					&first, 1, &map, &nmap, NULL);
		if (error || nmap != 1) {
			do_error(
		"couldn't map realtime summary inode block %llu - err %d\n",
				bno, error);
		}

		assert(map.br_startblock != HOLESTARTBLOCK);

		error = xfs_trans_read_buf(
#if VERS >= V_65
				mp,
#endif
				tp, mp->m_dev,
				XFS_FSB_TO_DADDR(mp, map.br_startblock), 
				XFS_FSB_TO_BB(mp, 1), 1, &bp);

		if (error) {
			do_warn(
	"can't access block %llu (fsbno %llu) of realtime summary inode %llu\n",
				bno, map.br_startblock, ino);
			return(1);
		}

		bcopy(smp, bp->b_un.b_addr, mp->m_sb.sb_blocksize);

		xfs_trans_log_buf(tp, bp, 0, mp->m_sb.sb_blocksize - 1);

		smp = (xfs_suminfo_t *)((__psint_t)smp + mp->m_sb.sb_blocksize);
		bno++;
	}

	xfsr_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC);

	return(0);
}

void
mk_rsumino(xfs_mount_t *mp)
{
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_bmbt_irec_t	*ep;
	xfs_fsblock_t	first;
	int		i;
	int		nmap;
	int		committed;
	int		error;
	int		nsumblocks;
	xfs_bmap_free_t	flist;
	xfs_dfiloff_t	bno;
	xfs_bmbt_irec_t	map[XFS_BMAP_MAX_NMAP];

	/*
	 * first set up inode
	 */
	tp = xfs_trans_alloc(mp, 0);

	if (i = xfs_trans_reserve(tp, 10, XFS_ICHANGE_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_MKDIR_LOG_COUNT))
		res_failed(i);

	error = xfs_trans_iget(mp, tp, mp->m_sb.sb_rsumino,
				XFS_ILOCK_EXCL, &ip);
	if (error) {
		do_error("couldn't iget realtime summary inode -- error - %d\n",
			error);
	}

	bzero(&ip->i_d, sizeof(xfs_dinode_core_t));

	ip->i_d.di_magic = XFS_DINODE_MAGIC;
	ip->i_d.di_mode = IFREG;
	ip->i_d.di_version = XFS_DINODE_VERSION_1;
	ip->i_d.di_format = XFS_DINODE_FMT_EXTENTS;
	ip->i_d.di_aformat = XFS_DINODE_FMT_EXTENTS;

	ip->i_d.di_nlink = 1;		/* account for sb ptr */

	/*
	 * now the ifork
	 */
	ip->i_df.if_flags = XFS_IFEXTENTS;
	ip->i_df.if_bytes = ip->i_df.if_real_bytes = 0;
	ip->i_df.if_u1.if_extents = NULL;

	ip->i_d.di_size = mp->m_rsumsize;

	/*
	 * commit changes
	 */
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
	xfs_trans_ihold(tp, ip);
	xfsr_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC);

	/*
	 * then allocate blocks for file and fill with zeroes (stolen
	 * from mkfs)
	 */
	tp = xfs_trans_alloc(mp, 0);
	XFS_BMAP_INIT(&flist, &first);

	nsumblocks = mp->m_rsumsize >> mp->m_sb.sb_blocklog;
	if (error = xfs_trans_reserve(tp,
				  mp->m_sb.sb_rbmblocks +
				      (XFS_BM_MAXLEVELS(mp, XFS_DATA_FORK) - 1),
				  BBTOB(128), 0, XFS_TRANS_PERM_LOG_RES,
				  XFS_DEFAULT_PERM_LOG_COUNT))
		res_failed(error);

	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	bno = 0;
	XFS_BMAP_INIT(&flist, &first);
	while (bno < nsumblocks) {
		nmap = XFS_BMAP_MAX_NMAP;
		error = xfs_bmapi(tp, ip, bno,
			  (xfs_extlen_t)(nsumblocks - bno),
			  XFS_BMAPI_WRITE, &first, nsumblocks,
			  map, &nmap, &flist);
		if (error) {
			do_error(
			"couldn't allocate realtime summary inode - err %d\n",
				error);
		}
		for (i = 0, ep = map; i < nmap; i++, ep++) {
			if (!dev_zero(mp->m_dev,
				      XFS_FSB_TO_DADDR(mp, ep->br_startblock),
				      XFS_FSB_TO_BB(mp, ep->br_blockcount)))
				do_error("dev_zero of rtbitmap failed\n");
			bno += ep->br_blockcount;
		}
	}
	error = xfs_bmap_finish(&tp, &flist, first, &committed);
	if (error) {
		do_error(
		"allocation of the realtime summary ino failed, err = %d\n",
			error);
	}
	xfsr_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC);

	return;
}

/*
 * makes a new root directory.
 */
void
mk_root_dir(xfs_mount_t *mp)
{
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	int		i;
	int		error;
	const mode_t	mode = 0755;

	tp = xfs_trans_alloc(mp, 0);
	ip = NULL;

	if (i = xfs_trans_reserve(tp, 10, XFS_ICHANGE_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_MKDIR_LOG_COUNT))
		res_failed(i);

	error = xfs_trans_iget(mp, tp, mp->m_sb.sb_rootino,
				XFS_ILOCK_EXCL, &ip);
	if (error) {
		do_error("could not iget root inode -- error - %d\n", error);
	}

	/*
	 * take care of the core -- initialization from xfs_ialloc()
	 */
	bzero(&ip->i_d, sizeof(xfs_dinode_core_t));

	ip->i_d.di_magic = XFS_DINODE_MAGIC;
	ip->i_d.di_mode = (__uint16_t) mode|IFDIR;
	ip->i_d.di_version = XFS_DINODE_VERSION_1;
	ip->i_d.di_format = XFS_DINODE_FMT_EXTENTS;
	ip->i_d.di_aformat = XFS_DINODE_FMT_EXTENTS;

	ip->i_d.di_nlink = 1;		/* account for . */

	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	/*
	 * now the ifork
	 */
	ip->i_df.if_flags = XFS_IFEXTENTS;
	ip->i_df.if_bytes = ip->i_df.if_real_bytes = 0;
	ip->i_df.if_u1.if_extents = NULL;

	mp->m_rootip = ip;

	/*
	 * initialize the directory
	 */
	(void) XFS_DIR_INIT(mp, tp, ip, ip);

	xfsr_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC);
	return;
}

/*
 * orphanage name == lost+found
 */
xfs_ino_t
mk_orphanage(xfs_mount_t *mp)
{
	xfs_ino_t	ino;
	xfs_trans_t	*tp;
	xfs_inode_t	*ip;
	xfs_inode_t	*pip;
	xfs_fsblock_t	first;
	int		i;
	int		committed;
	int		error;
	xfs_bmap_free_t	flist;
	const int	mode = 0755;
	const int	uid = 0;
	const int	gid = 0;
	int		nres;

	tp = xfs_trans_alloc(mp, 0);
	XFS_BMAP_INIT(&flist, &first);

#if VERS >= V_653
	nres = XFS_MKDIR_SPACE_RES(mp, strlen(ORPHANAGE));
#else
	nres = XFS_MKDIR_SPACE_RES(mp, ORPHANAGE);
#endif
	if (i = xfs_trans_reserve(tp, nres, XFS_MKDIR_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_MKDIR_LOG_COUNT))
		res_failed(i);

	/*
	 * use iget/ijoin instead of trans_iget because the ialloc
	 * wrapper can commit the transaction and start a new one
	 */
	if (i = xfs_iget(mp, NULL, mp->m_sb.sb_rootino, XFS_ILOCK_EXCL,
			&pip, 0))
		do_error("%d - couldn't iget root inode to make %s\n",
			i, ORPHANAGE);

	error = ialloc(&tp, pip, mode|IFDIR, 1, mp->m_dev, &zerocr, &ip);

	if (error) {
		do_error("%s inode allocation failed %d\n",
			ORPHANAGE, error);
	}

	ip->i_d.di_uid = uid;
	ip->i_d.di_gid = gid;
	ip->i_d.di_nlink++;		/* account for . */

	/*
	 * now that we know the transaction will stay around,
	 * add the root inode to it
	 */
	xfs_trans_ijoin(tp, pip, XFS_ILOCK_EXCL);

	/*
	 * create the actual entry
	 */
	if (error = XFS_DIR_CREATENAME(mp, tp, pip, ORPHANAGE,
			strlen(ORPHANAGE), ip->i_ino, &first, &flist, nres)) {
		do_warn("can't make %s, createname error %d, will try later\n",
			ORPHANAGE, error);
		orphanage_entered = 0;
	} else
		orphanage_entered = 1;

	/* 
	 * bump up the link count in the root directory to account
	 * for .. in the new directory
	 */
	pip->i_d.di_nlink++;

	xfs_trans_log_inode(tp, pip, XFS_ILOG_CORE);
	XFS_DIR_INIT(mp, tp, ip, pip);
	xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

	error = xfs_bmap_finish(&tp, &flist, first, &committed);
	if (error) {
		do_error("%s directory creation failed -- bmapf error %d\n",
			ORPHANAGE, error);
	}

	ino = ip->i_ino;

	xfsr_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC);

	return(ino);
}

/*
 * move a file to the orphange.  the orphanage is guaranteed
 * at this point to only have file in it whose name == file inode #
 */
void
mv_orphanage(xfs_mount_t	*mp,
		xfs_ino_t	dir_ino,	/* orphange inode # */
		xfs_ino_t	ino,		/* inode # to be moved */
		int		isa_dir)	/* 1 if inode is a directory */
{
	xfs_ino_t	entry_ino_num;
	xfs_inode_t	*dir_ino_p;
	xfs_inode_t	*ino_p;
	xfs_trans_t	*tp;
	xfs_fsblock_t	first;
	xfs_bmap_free_t	flist;
	int		err;
	int		committed;
	char		fname[MAXPATHLEN + 1];
	int		nres;

	sprintf(fname, "%llu", ino);

	if (err = xfs_iget(mp, NULL, dir_ino, XFS_ILOCK_EXCL, &dir_ino_p, 0))
		do_error("%d - couldn't iget orphanage inode\n", err);

	tp = xfs_trans_alloc(mp, 0);

	if (err = xfs_iget(mp, NULL, ino, XFS_ILOCK_EXCL, &ino_p, 0))
		do_error("%d - couldn't iget disconnected inode\n", err);

	if (isa_dir)  {
#if VERS >= V_653
		nres = XFS_DIRENTER_SPACE_RES(mp, strlen(fname)) +
		       XFS_DIRENTER_SPACE_RES(mp, 2);
#else
		nres = XFS_DIRENTER_SPACE_RES(mp, fname) +
		       XFS_DIRENTER_SPACE_RES(mp, "..");
#endif
		if (err = XFS_DIR_LOOKUP(mp, tp, ino_p, "..", 2,
				&entry_ino_num))  {
			assert(err == ENOENT);

			if (err = xfs_trans_reserve(tp, nres,
					XFS_RENAME_LOG_RES(mp), 0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_RENAME_LOG_COUNT))
				do_error(
		"space reservation failed (%d), filesystem may be out of space\n",
					err);

			xfs_trans_ijoin(tp, dir_ino_p, XFS_ILOCK_EXCL);
			xfs_trans_ijoin(tp, ino_p, XFS_ILOCK_EXCL);

			XFS_BMAP_INIT(&flist, &first);
			if (err = XFS_DIR_CREATENAME(mp, tp, dir_ino_p, fname,
						strlen(fname), ino, &first,
						&flist, nres))
				do_error(
	"name create failed in %s (%d), filesystem may be out of space\n",
					ORPHANAGE, err);

			dir_ino_p->i_d.di_nlink++;
			xfs_trans_log_inode(tp, dir_ino_p, XFS_ILOG_CORE);

			if (err = XFS_DIR_CREATENAME(mp, tp, ino_p, "..", 2,
						dir_ino, &first, &flist, nres))
				do_error(
	"creation of .. entry failed (%d), filesystem may be out of space\n",
					err);

			ino_p->i_d.di_nlink++;
			xfs_trans_log_inode(tp, ino_p, XFS_ILOG_CORE);

			if (err = xfs_bmap_finish(&tp, &flist, first, &committed))
				do_error(
	"bmap finish failed (err - %d), filesystem may be out of space\n",
					err);

			xfsr_trans_commit(tp,
				XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC);
		} else  {
			if (err = xfs_trans_reserve(tp, nres,
					XFS_RENAME_LOG_RES(mp), 0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_RENAME_LOG_COUNT))
				do_error(
	"space reservation failed (%d), filesystem may be out of space\n",
					err);

			xfs_trans_ijoin(tp, dir_ino_p, XFS_ILOCK_EXCL);
			xfs_trans_ijoin(tp, ino_p, XFS_ILOCK_EXCL);

			XFS_BMAP_INIT(&flist, &first);

			if (err = XFS_DIR_CREATENAME(mp, tp, dir_ino_p, fname,
						strlen(fname), ino, &first,
						&flist, nres))
				do_error(
	"name create failed in %s (%d), filesystem may be out of space\n",
					ORPHANAGE, err);

			dir_ino_p->i_d.di_nlink++;
			xfs_trans_log_inode(tp, dir_ino_p, XFS_ILOG_CORE);

			/*
			 * don't replace .. value if it already points
			 * to us.  that'll pop a sim/kernel assert.
			 */
			if (entry_ino_num != dir_ino)  {
				if (err = XFS_DIR_REPLACE(mp, tp, ino_p, "..",
							2, dir_ino, &first,
							&flist, nres))
					do_error(
		"name replace op failed (%d), filesystem may be out of space\n",
						err);
			}

			if (err = xfs_bmap_finish(&tp, &flist, first,
							&committed))
				do_error(
		"bmap finish failed (%d), filesystem may be out of space\n",
					err);

			xfsr_trans_commit(tp,
				XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC);
		}
	} else  {
		/*
		 * use the remove log reservation as that's
		 * more accurate.  we're only creating the
		 * links, we're not doing the inode allocation
		 * also accounted for in the create
		 */
#if VERS >= V_653
		nres = XFS_DIRENTER_SPACE_RES(mp, strlen(fname));
#else
		nres = XFS_DIRENTER_SPACE_RES(mp, fname);
#endif
		if (err = xfs_trans_reserve(tp, nres, XFS_REMOVE_LOG_RES(mp), 0,
				XFS_TRANS_PERM_LOG_RES, XFS_REMOVE_LOG_COUNT))
			do_error(
	"space reservation failed (%d), filesystem may be out of space\n",
				err);

		xfs_trans_ijoin(tp, dir_ino_p, XFS_ILOCK_EXCL);
		xfs_trans_ijoin(tp, ino_p, XFS_ILOCK_EXCL);

		XFS_BMAP_INIT(&flist, &first);
		if (err = XFS_DIR_CREATENAME(mp, tp, dir_ino_p, fname,
				strlen(fname), ino, &first, &flist, nres))
			do_error(
	"name create failed in %s (%d), filesystem may be out of space\n",
				ORPHANAGE, err);
		assert(err == 0);

		ino_p->i_d.di_nlink = 1;
		xfs_trans_log_inode(tp, ino_p, XFS_ILOG_CORE);

		if (err = xfs_bmap_finish(&tp, &flist, first, &committed))
			do_error(
		"bmap finish failed (%d), filesystem may be out of space\n",
				err);

		xfsr_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC);
	}

	return;
}

/*
 * like get_first_dblock_fsbno only it uses the simulation code instead
 * of raw I/O.
 *
 * Returns the fsbno of the first (leftmost) block in the directory leaf.
 * sets *bno to the directory block # corresponding to the returned fsbno.
 */
xfs_dfsbno_t
map_first_dblock_fsbno(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_inode_t	*ip,
			xfs_dablk_t	*bno)
{
	xfs_fsblock_t		fblock;
	xfs_da_intnode_t	*node;
	buf_t			*bp;
	xfs_dablk_t		da_bno;
	xfs_dfsbno_t		fsbno;
	xfs_bmbt_irec_t		map;
	int			nmap;
	int			i;
	int			error;
	char			*ftype;

	/*
	 * traverse down left-side of tree until we hit the
	 * left-most leaf block setting up the btree cursor along
	 * the way.
	 */
	da_bno = 0;
	*bno = 0;
	i = -1;
	node = NULL;
	fblock = NULLFSBLOCK;
	ftype = "dir";

	nmap = 1;
	error = xfs_bmapi(NULL, ip, (xfs_fileoff_t) da_bno, 1,
			XFS_BMAPI_METADATA, &fblock, 0,
			&map, &nmap, NULL);
	if (error || nmap != 1)  {
		if (!no_modify)
			do_error(
"can't map block %d in %s inode %llu, xfs_bmapi returns %d, nmap = %d\n",
				da_bno, ftype, ino, error, nmap);
		else  {
			do_warn(
"can't map block %d in %s inode %llu, xfs_bmapi returns %d, nmap = %d\n",
				da_bno, ftype, ino, error, nmap);
			return(NULLDFSBNO);
		}
	}

	if ((fsbno = map.br_startblock) == HOLESTARTBLOCK)  {
		if (!no_modify)
			do_error("block %d in %s ino %llu doesn't exist\n",
				da_bno, ftype, ino);
		else  {
			do_warn("block %d in %s ino %llu doesn't exist\n",
				da_bno, ftype, ino);
			return(NULLDFSBNO);
		}
	}

	if (ip->i_d.di_size <= XFS_LBSIZE(mp))
		return(fsbno);

#if VERS >= V_654
	if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
		return(fsbno);
#endif

	do {

		/*
		 * walk down left side of btree, release buffers as you
		 * go.  if the root block is a leaf (single-level btree),
		 * just return it.
		 * 
		 */

		bp = read_buf(mp->m_dev, XFS_FSB_TO_DADDR(mp, fsbno),
				XFS_FSB_TO_BB(mp, 1), BUF_TRYLOCK);

		if (bp == NULL || geterror(bp)) {
			do_warn(
		"can't read block %u (fsbno %llu) for directory inode %llu\n",
					da_bno, fsbno, ino);
			return(NULLDFSBNO);
		}

		node = (xfs_da_intnode_t *)bp->b_un.b_addr;

		if (node->hdr.info.magic != XFS_DA_NODE_MAGIC)  {
			brelse(bp);
			do_warn(
"bad dir/attr magic number in inode %llu, file bno = %u, fsbno = %llu\n",
				ino, da_bno, fsbno);
			return(NULLDFSBNO);
		}

		if (i == -1)
			i = node->hdr.level;

		da_bno = node->btree[0].before;

		brelse(bp);
		bp = NULL;

		nmap = 1;
		error = xfs_bmapi(NULL, ip, (xfs_fileoff_t) da_bno, 1,
				XFS_BMAPI_METADATA, &fblock, 0,
				&map, &nmap, NULL);
		if (error || nmap != 1)  {
			if (!no_modify)
				do_error(
	"can't map block %d in %s ino %llu, xfs_bmapi returns %d, nmap = %d\n",
					da_bno, ftype, ino, error, nmap);
			else  {
				do_warn(
	"can't map block %d in %s ino %llu, xfs_bmapi returns %d, nmap = %d\n",
					da_bno, ftype, ino, error, nmap);
				return(NULLDFSBNO);
			}
		}
		if ((fsbno = map.br_startblock) == HOLESTARTBLOCK)  {
			if (!no_modify)
				do_error(
				"block %d in %s inode %llu doesn't exist\n",
					da_bno, ftype, ino);
			else  {
				do_warn(
				"block %d in %s inode %llu doesn't exist\n",
					da_bno, ftype, ino);
				return(NULLDFSBNO);
			}
		}

		i--;
	} while(i > 0);

	*bno = da_bno;
	return(fsbno);
}

/*
 * scan longform directory and prune first bad entry.  returns 1 if
 * it had to remove something, 0 if it made it all the way through
 * the directory.  prune_lf_dir_entry does all the necessary bmap calls.
 *
 * hashval is an in/out -- starting hashvalue in, hashvalue of the
 *			deleted entry (if there was one) out
 *
 * this routine can NOT be called if running in no modify mode
 */
int
prune_lf_dir_entry(xfs_mount_t *mp, xfs_ino_t ino, xfs_inode_t *ip,
			xfs_dahash_t *hashval)
{
	xfs_dfsbno_t		fsbno;
	int			i;
	int			index;
	int			error;
	int			namelen;
	xfs_bmap_free_t		free_list;
	xfs_fsblock_t		first_block;
	buf_t			*bp;
	xfs_dir_leaf_name_t	*namest;
	xfs_dir_leafblock_t	*leaf;
	xfs_dir_leaf_entry_t	*entry;
	xfs_trans_t		*tp;
	xfs_dablk_t		da_bno;
	xfs_fsblock_t		fblock;
	int			committed;
	int			nmap;
	xfs_bmbt_irec_t		map;
	char			fname[MAXNAMELEN + 1];
	char			*ftype;
	int			nres;

	/*
	 * ok, this is kind of a schizoid routine.  we use our
	 * internal bmapi routines to walk the directory.  when
	 * we find a bogus entry, we release the buffer so
	 * the simulation code doesn't deadlock and use the
	 * sim code to remove the entry.  That will cause an
	 * extra bmap traversal to map the block but I think
	 * that's preferable to hacking the bogus removename
	 * function to be really different and then trying to
	 * maintain both versions as time goes on.
	 *
	 * first, grab the dinode and find the right leaf block.
	 */

	ftype = "dir";
	da_bno = 0;
	bp = NULL;
	fblock = NULLFSBLOCK;

	fsbno = map_first_dblock_fsbno(mp, ino, ip, &da_bno);

	/*
	 * now go foward along the leaves of the btree looking
	 * for an entry beginning with '/'
	 */
	do {
		bp = read_buf(mp->m_dev, XFS_FSB_TO_DADDR(mp, fsbno),
				XFS_FSB_TO_BB(mp, 1), BUF_TRYLOCK);

		if (bp == NULL || geterror(bp))  {
			do_error(
	"can't read directory inode %llu (leaf) block %u (fsbno %llu)\n",
				ino, da_bno, fsbno);
			/* NOTREACHED */
		}

		leaf = (xfs_dir_leafblock_t *)bp->b_un.b_addr;
		assert(leaf->hdr.info.magic == XFS_DIR_LEAF_MAGIC);
		entry = &leaf->entries[0];

		for (index = -1, i = 0;
				i < leaf->hdr.count && index == -1;
				i++)  {
			namest = XFS_DIR_LEAF_NAMESTRUCT(leaf, entry->nameidx);
			if (namest->name[0] != '/')
				entry++;
			else
				index = i;
		}

		/*
		 * if we got a bogus entry, exit loop with a pointer to
		 * the leaf block buffer.  otherwise, keep trying blocks
		 */
		da_bno = leaf->hdr.info.forw;

		if (index == -1)  {
			if (bp != NULL)  {
				brelse(bp);
				bp = NULL;
			}

			/*
			 * map next leaf block unless we've run out
			 */
			if (da_bno != 0)  {
				nmap = 1;
				error = xfs_bmapi(NULL, ip,
						(xfs_fileoff_t) da_bno, 1,
						XFS_BMAPI_METADATA, &fblock, 0,
						&map, &nmap, NULL);
				if (error || nmap != 1)
					do_error(
"can't map block %d in directory %llu, xfs_bmapi returns %d, nmap = %d\n",
						da_bno, ino, error, nmap);
				if ((fsbno = map.br_startblock)
						== HOLESTARTBLOCK)  {
					do_error(
				"%s ino %llu block %d doesn't exist\n",
						ftype, ino, da_bno);
				}
			}
		}
	} while (da_bno != 0 && index == -1);

	/*
	 * if we hit the edge of the tree with no bad entries, we're done
	 * and the buffer was released.
	 */
	if (da_bno == 0 && index == -1)
		return(0);

	assert(index >= 0);
	assert(entry == &leaf->entries[index]);
	assert(namest == XFS_DIR_LEAF_NAMESTRUCT(leaf, entry->nameidx));

	/*
	 * snag the info we need out of the directory then release all buffers
	 */
	bcopy(namest->name, fname, entry->namelen);
	fname[entry->namelen] = '\0';
	*hashval = entry->hashval;
	namelen = entry->namelen;

	brelse(bp);

	/*
	 * ok, now the hard part, blow away the index'th entry in this block
	 *
	 * allocate a remove transaction for it.  that's not quite true since
	 * we're only messing with one inode, not two but...
	 */

	tp = xfs_trans_alloc(mp, XFS_TRANS_REMOVE);

	nres = XFS_REMOVE_SPACE_RES(mp);
	error = xfs_trans_reserve(tp, nres, XFS_REMOVE_LOG_RES(mp),
				    0, XFS_TRANS_PERM_LOG_RES,
				    XFS_REMOVE_LOG_COUNT);
	if (error)
		res_failed(error);

	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_ihold(tp, ip);

	XFS_BMAP_INIT(&free_list, &first_block);

	error = XFS_DIR_BOGUS_REMOVENAME(mp, tp, ip, fname,
		&first_block, &free_list, nres, *hashval, namelen);

	if (error)  {
		do_error(
"couldn't remove bogus entry \"%s\" in\n\tdirectory inode %llu, errno = %d\n",
			fname, ino, error);
		/* NOTREACHED */
	}

	error = xfs_bmap_finish(&tp, &free_list, first_block, &committed);

	assert(error == 0);

	error = xfsr_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES|XFS_TRANS_SYNC);

	return(1);
}

/*
 * process a leaf block, also checks for .. entry
 * and corrects it to match what we think .. should be
 */
void
lf_block_dir_entry_check(xfs_mount_t		*mp,
			xfs_ino_t		ino,
			xfs_dir_leafblock_t	*leaf,
			int			*dirty,
			int			*num_illegal,
			int			*need_dot,
			dir_stack_t		*stack,
			ino_tree_node_t		*current_irec,
			int			current_ino_offset)
{
	xfs_dir_leaf_entry_t	*entry;
	ino_tree_node_t		*irec;
	xfs_ino_t		lino;
	xfs_ino_t		parent;
	xfs_dir_leaf_name_t	*namest;
	int			i;
	int			junkit;
	int			ino_offset;
	int			nbad;
	char			fname[MAXNAMELEN + 1];

	entry = &leaf->entries[0];
	*dirty = 0;
	nbad = 0;

	/*
	 * look at each entry.  reference inode pointed to by each
	 * entry in the incore inode tree.
	 * if not a directory, set reached flag, increment link count
	 * if a directory and reached, mark entry as to be deleted.
	 * if a directory, check to see if recorded parent
	 *	matches current inode #,
	 *	if so, then set reached flag, increment link count
	 *		of current and child dir inodes, push the child
	 *		directory inode onto the directory stack.
	 *	if current inode != parent, then mark entry to be deleted.
	 *
	 * return
	 */
	for (i = 0; i < leaf->hdr.count; entry++, i++)  {
		/*
		 * snag inode #, update link counts, and make sure
		 * this isn't a loop if the child is a directory
		 */
		namest = XFS_DIR_LEAF_NAMESTRUCT(leaf, entry->nameidx);

		/*
		 * skip bogus entries (leading '/').  they'll be deleted
		 * later
		 */
		if (namest->name[0] == '/')  {
			nbad++;
			continue;
		}

		junkit = 0;

		XFS_DIR_SF_GET_DIRINO(&namest->inumber, &lino);
		bcopy(namest->name, fname, entry->namelen);
		fname[entry->namelen] = '\0';

		assert(lino != NULLFSINO);

		/*
		 * skip the '..' entry since it's checked when the
		 * directory is reached by something else.  if it never
		 * gets reached, it'll be moved to the orphanage and we'll
		 * take care of it then.
		 */
		if (entry->namelen == 2 && namest->name[0] == '.' &&
				namest->name[1] == '.')  {
			continue;
		}
		assert(no_modify || !verify_inum(mp, lino));

		/*
		 * special case the . entry.  we know there's only one
		 * '.' and only '.' points to itself because bogus entries
		 * got trashed in phase 3 if there were > 1.
		 * bump up link count for '.' but don't set reached
		 * until we're actually reached by another directory
		 * '..' is already accounted for or will be taken care
		 * of when directory is moved to orphanage.
		 */
		if (ino == lino)  {
			assert(namest->name[0] == '.' && entry->namelen == 1);
			add_inode_ref(current_irec, current_ino_offset);
			*need_dot = 0;
			continue;
		}

		/*
		 * special case the "lost+found" entry if pointing
		 * to where we think lost+found should be.  if that's
		 * the case, that's the one we created in phase 6.
		 * just skip it.  no need to process it and it's ..
		 * link is already accounted for.
		 */

		if (lino == orphanage_ino && strcmp(fname, ORPHANAGE) == 0)
			continue;

		/*
		 * skip entries with bogus inumbers if we're in no modify mode
		 */
		if (no_modify && verify_inum(mp, lino))
			continue;

		/*
		 * ok, now handle the rest of the cases besides '.' and '..'
		 */
		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, lino),
					XFS_INO_TO_AGINO(mp, lino));
		
		if (irec == NULL)  {
			nbad++;
			do_warn(
	"entry \"%s\" in dir inode %llu points to non-existent inode, ",
				fname, ino);

			if (!no_modify)  {
				namest->name[0] = '/';
				*dirty = 1;
				do_warn("marking entry to be junked\n");
			} else  {
				do_warn("would junk entry\n");
			}

			continue;
		}

		ino_offset = XFS_INO_TO_AGINO(mp, lino) - irec->ino_startnum;

		/*
		 * if it's a free inode, blow out the entry.
		 * by now, any inode that we think is free
		 * really is free.
		 */
		if (is_inode_free(irec, ino_offset))  {
			/*
			 * don't complain if this entry points to the old
			 * and now-free lost+found inode
			 */
			if (verbose || no_modify || lino != old_orphanage_ino)
				do_warn(
		"entry \"%s\" in dir inode %llu points to free inode %llu",
					fname, ino, lino);
			nbad++;

			if (!no_modify)  {
				if (verbose || lino != old_orphanage_ino)
					do_warn(", marking entry to be junked\n");

				else
					do_warn("\n");
				namest->name[0] = '/';
				*dirty = 1;
			} else  {
				do_warn(", would junk entry\n");
			}

			continue;
		}

		/*
		 * check easy case first, regular inode, just bump
		 * the link count and continue
		 */
		if (!inode_isadir(irec, ino_offset))  {
			add_inode_reached(irec, ino_offset);
			continue;
		}

		parent = get_inode_parent(irec, ino_offset);
		assert(parent != 0);

		/*
		 * bump up the link counts in parent and child
		 * directory but if the link doesn't agree with
		 * the .. in the child, blow out the entry.
		 * if the directory has already been reached,
		 * blow away the entry also.
		 */
		if (is_inode_reached(irec, ino_offset))  {
			junkit = 1;
			do_warn(
"entry \"%s\" in dir %llu points to an already connected dir inode %llu,\n",
				fname, ino, lino);
		} else if (parent == ino)  {
			add_inode_reached(irec, ino_offset);
			add_inode_ref(current_irec, current_ino_offset);

			if (!is_inode_refchecked(lino, irec, ino_offset))
				push_dir(stack, lino);
		} else  {
			junkit = 1;
			do_warn(
"entry \"%s\" in dir ino %llu not consistent with .. value (%llu) in ino %llu,\n",
				fname, ino, parent, lino);
		}

		if (junkit)  {
			junkit = 0;
			nbad++;

			if (!no_modify)  {
				namest->name[0] = '/';
				*dirty = 1;
				if (verbose || lino != old_orphanage_ino)
					do_warn("\twill clear entry \"%s\"\n",
						fname);
			} else  {
				do_warn("\twould clear entry \"%s\"\n", fname);
			}
		}
	}

	*num_illegal += nbad;
	return;
}

/*
 * succeeds or dies, inode never gets dirtied since all changes
 * happen in file blocks.  the inode size and other core info
 * is already correct, it's just the leaf entries that get altered.
 */
void
longform_dir_entry_check(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_inode_t	*ip,
			int		*num_illegal,
			int		*need_dot,
			dir_stack_t	*stack,
			ino_tree_node_t	*irec,
			int		ino_offset)
{
	xfs_dir_leafblock_t	*leaf;
	buf_t			*bp;
	xfs_dfsbno_t		fsbno;
	xfs_fsblock_t		fblock;
	xfs_dablk_t		da_bno;
	int			dirty;
	int			nmap;
	int			error;
	int			skipit;
	xfs_bmbt_irec_t		map;
	char			*ftype;

	da_bno = 0;
	fblock = NULLFSBLOCK;
	*need_dot = 1;
	ftype = "dir";

	fsbno = map_first_dblock_fsbno(mp, ino, ip, &da_bno);

	if (fsbno == NULLDFSBNO && no_modify)  {
		do_warn("cannot map block 0 of directory inode %llu\n", ino);
		return;
	}

	do {
		assert(fsbno != NULLDFSBNO);
		skipit = 0;

		bp = read_buf(mp->m_dev, XFS_FSB_TO_DADDR(mp, fsbno),
				XFS_FSB_TO_BB(mp, 1), BUF_TRYLOCK);

		if (bp == NULL || geterror(bp)) {
			do_error(
		"can't read block %u (fsbno %llu) for directory inode %llu\n",
					da_bno, fsbno, ino);
			/* NOTREACHED */
		}

		leaf = (xfs_dir_leafblock_t *)bp->b_un.b_addr;

		da_bno = leaf->hdr.info.forw;

		if (leaf->hdr.info.magic != XFS_DIR_LEAF_MAGIC)  {
			if (!no_modify)  {
				do_error(
	"bad magic # (0x%x) for dir ino %llu leaf block (bno %u fsbno %llu)\n",
					leaf->hdr.info.magic,
					ino, da_bno, fsbno);
				/* NOTREACHED */
			} else  {
				/*
				 * this block's bad but maybe the
				 * forward pointer is good...
				 */
				skipit = 1;
				dirty = 0;
			}
		}

		if (!skipit)
			lf_block_dir_entry_check(mp, ino, leaf, &dirty,
						num_illegal, need_dot, stack,
						irec, ino_offset);

		assert(dirty == 0 || dirty && !no_modify);

		if (dirty && !no_modify)
			bwrite(bp);
		else
			brelse(bp);
		bp = NULL;

		if (da_bno != 0)  {
			nmap = 1;
			error = xfs_bmapi(NULL, ip, (xfs_fileoff_t) da_bno, 1,
					XFS_BMAPI_METADATA, &fblock, 0,
					&map, &nmap, NULL);
			if (error || nmap != 1)  {
				if (!no_modify)
					do_error(
"can't map leaf block %d in dir %llu, xfs_bmapi returns %d, nmap = %d\n",
						da_bno, ino, error, nmap);
				else  {
					do_warn(
"can't map leaf block %d in dir %llu, xfs_bmapi returns %d, nmap = %d\n",
						da_bno, ino, error, nmap);
					return;
				}
			}
			if ((fsbno = map.br_startblock) == HOLESTARTBLOCK)  {
				if (!no_modify)
					do_error(
				"block %d in %s ino %llu doesn't exist\n",
						da_bno, ftype, ino);
				else  {
					do_warn(
				"block %d in %s ino %llu doesn't exist\n",
						da_bno, ftype, ino);
					return;
				}
			}
		}
	} while (da_bno != 0);

	return;
}

#if VERS >= V_654
/*
 * Kill a block in a version 2 inode.
 * Makes its own transaction.
 */
static void
dir2_kill_block(
	xfs_mount_t	*mp,
	xfs_inode_t	*ip,
	xfs_dablk_t	da_bno,
	xfs_dabuf_t	*bp)
{
	xfs_da_args_t	args;
	int		committed;
	int		error;
	xfs_fsblock_t	firstblock;
	xfs_bmap_free_t	flist;
	int		nres;
	xfs_trans_t	*tp;

	tp = xfs_trans_alloc(mp, 0);
	nres = XFS_REMOVE_SPACE_RES(mp);
	error = xfs_trans_reserve(tp, nres, XFS_REMOVE_LOG_RES(mp), 0,
			XFS_TRANS_PERM_LOG_RES, XFS_REMOVE_LOG_COUNT);
	if (error)
		res_failed(error);
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_ihold(tp, ip);
	xfs_da_bjoin(tp, bp);
	bzero(&args, sizeof(args));
	XFS_BMAP_INIT(&flist, &firstblock);
	args.dp = ip;
	args.trans = tp;
	args.firstblock = &firstblock;
	args.flist = &flist;
	args.whichfork = XFS_DATA_FORK;
	if (da_bno >= mp->m_dirleafblk && da_bno < mp->m_dirfreeblk)
		error = xfs_da_shrink_inode(&args, da_bno, bp);
	else
		error = xfs_dir2_shrink_inode(&args,
				XFS_DIR2_DA_TO_DB(mp, da_bno), bp);
	if (error)
		do_error("shrink_inode failed inode %llu block %u\n",
			ip->i_ino, da_bno);
	xfs_bmap_finish(&tp, &flist, firstblock, &committed);
	xfsr_trans_commit(tp, 0);
}

/*
 * process a data block, also checks for .. entry
 * and corrects it to match what we think .. should be
 */
static void
longform_dir2_entry_check_data(
	xfs_mount_t		*mp,
	xfs_inode_t		*ip,
	int			*num_illegal,
	int			*need_dot,
	dir_stack_t		*stack,
	ino_tree_node_t		*current_irec,
	int			current_ino_offset,
	xfs_dabuf_t		**bpp,
	dir_hash_tab_t		*hashtab,
	freetab_t		**freetabp,
	xfs_dablk_t		da_bno,
	int			isblock)
{
	xfs_dir2_dataptr_t	addr;
	xfs_dir2_leaf_entry_t	*blp;
	xfs_dabuf_t		*bp;
	xfs_dir2_block_tail_t	*btp;
	int			committed;
	xfs_dir2_data_t		*d;
	xfs_dir2_db_t		db;
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
	char			*endptr;
	int			error;
	xfs_fsblock_t		firstblock;
	xfs_bmap_free_t		flist;
	char			fname[MAXNAMELEN + 1];
	freetab_t		*freetab;
	int			i;
	int			ino_offset;
	ino_tree_node_t		*irec;
	int			junkit;
	int			lastfree;
	int			len;
	int			nbad;
	int			needlog;
	int			needscan;
	xfs_ino_t		parent;
	char			*ptr;
	xfs_trans_t		*tp;
	int			wantmagic;

	bp = *bpp;
	d = bp->data;
	ptr = (char *)d->u;
	nbad = 0;
	needscan = needlog = 0;
	freetab = *freetabp;
	if (isblock) {
		btp = XFS_DIR2_BLOCK_TAIL_P(mp, d);
		blp = XFS_DIR2_BLOCK_LEAF_P(btp);
		endptr = (char *)blp;
		if (endptr > (char *)btp)
			endptr = (char *)btp;
		wantmagic = XFS_DIR2_BLOCK_MAGIC;
	} else {
		endptr = (char *)d + mp->m_dirblksize;
		wantmagic = XFS_DIR2_DATA_MAGIC;
	}
	db = XFS_DIR2_DA_TO_DB(mp, da_bno);
	if (freetab->naents <= db) {
		struct freetab_ent e;

		*freetabp = freetab = realloc(freetab, FREETAB_SIZE(db + 1));
		e.v = NULLDATAOFF;
		e.s = 0;
		for (i = freetab->naents; i < db; i++)
			freetab->ents[i] = e;
		freetab->naents = db + 1;
	}
	if (freetab->nents < db + 1)
		freetab->nents = db + 1;
	while (ptr < endptr) {
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (dup->freetag == XFS_DIR2_DATA_FREE_TAG) {
			if (ptr + dup->length > endptr || dup->length == 0 ||
			    (dup->length & (XFS_DIR2_DATA_ALIGN - 1)))
				break;
			if (*XFS_DIR2_DATA_UNUSED_TAG_P(dup) != 
			    (char *)dup - (char *)d)
				break;
			ptr += dup->length;
		}
		dep = (xfs_dir2_data_entry_t *)ptr;
		if (ptr + XFS_DIR2_DATA_ENTSIZE(dep->namelen) > endptr)
			break;
		if (*XFS_DIR2_DATA_ENTRY_TAG_P(dep) != (char *)dep - (char *)d)
			break;
		ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
	}
	if (ptr != endptr) {
		do_warn("corrupt block %u in directory inode %llu: ",
			da_bno, ip->i_ino);
		if (!no_modify) {
			do_warn("junking block\n");
			dir2_kill_block(mp, ip, da_bno, bp);
		} else {
			do_warn("would junk block\n");
			xfs_da_brelse(NULL, bp);
		}
		freetab->ents[db].v = NULLDATAOFF;
		*bpp = NULL;
		return;
	}
	tp = xfs_trans_alloc(mp, 0);
	error = xfs_trans_reserve(tp, 0, XFS_REMOVE_LOG_RES(mp), 0,
		XFS_TRANS_PERM_LOG_RES, XFS_REMOVE_LOG_COUNT);
	if (error)
		res_failed(error);
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_ihold(tp, ip);
	xfs_da_bjoin(tp, bp);
	if (isblock)
		xfs_da_bhold(tp, bp);
	XFS_BMAP_INIT(&flist, &firstblock);
	if (d->hdr.magic != wantmagic) {
		do_warn("bad directory block magic # %#x for directory inode "
			"%llu block %d: ",
			d->hdr.magic, ip->i_ino, da_bno);
		if (!no_modify) {
			do_warn("fixing magic # to %#x\n", wantmagic);
			d->hdr.magic = wantmagic;
			needlog = 1;
		} else
			do_warn("would fix magic # to %#x\n", wantmagic);
	}
	lastfree = 0;
	ptr = (char *)d->u;
	/*
	 * look at each entry.  reference inode pointed to by each
	 * entry in the incore inode tree.
	 * if not a directory, set reached flag, increment link count
	 * if a directory and reached, mark entry as to be deleted.
	 * if a directory, check to see if recorded parent
	 *	matches current inode #,
	 *	if so, then set reached flag, increment link count
	 *		of current and child dir inodes, push the child
	 *		directory inode onto the directory stack.
	 *	if current inode != parent, then mark entry to be deleted.
	 */
	while (ptr < endptr) {
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (dup->freetag == XFS_DIR2_DATA_FREE_TAG) {
			if (lastfree) {
				do_warn("directory inode %llu block %u has "
					"consecutive free entries: ",
					ip->i_ino, da_bno);
				if (!no_modify) {
					do_warn("joining together\n");
					len = dup->length;
					xfs_dir2_data_use_free(tp, bp, dup,
						ptr - (char *)d, len, &needlog,
						&needscan);
					xfs_dir2_data_make_free(tp, bp,
						ptr - (char *)d, len, &needlog,
						&needscan);
				} else
					do_warn("would join together\n");
			}
			ptr += dup->length;
			lastfree = 1;
			continue;
		}
		addr = XFS_DIR2_DB_OFF_TO_DATAPTR(mp, db, ptr - (char *)d);
		dep = (xfs_dir2_data_entry_t *)ptr;
		ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
		lastfree = 0;
		dir_hash_add(hashtab,
			xfs_da_hashname((char *)dep->name, dep->namelen),
			addr, dep->name[0] == '/');
		/*
		 * skip bogus entries (leading '/').  they'll be deleted
		 * later
		 */
		if (dep->name[0] == '/')  {
			nbad++;
			continue;
		}
		junkit = 0;
		bcopy(dep->name, fname, dep->namelen);
		fname[dep->namelen] = '\0';
		assert(dep->inumber != NULLFSINO);
		/*
		 * skip the '..' entry since it's checked when the
		 * directory is reached by something else.  if it never
		 * gets reached, it'll be moved to the orphanage and we'll
		 * take care of it then.
		 */
		if (dep->namelen == 2 && dep->name[0] == '.' &&
		    dep->name[1] == '.')
			continue;
		assert(no_modify || !verify_inum(mp, dep->inumber));
		/*
		 * special case the . entry.  we know there's only one
		 * '.' and only '.' points to itself because bogus entries
		 * got trashed in phase 3 if there were > 1.
		 * bump up link count for '.' but don't set reached
		 * until we're actually reached by another directory
		 * '..' is already accounted for or will be taken care
		 * of when directory is moved to orphanage.
		 */
		if (ip->i_ino == dep->inumber)  {
			assert(dep->name[0] == '.' && dep->namelen == 1);
			add_inode_ref(current_irec, current_ino_offset);
			*need_dot = 0;
			continue;
		}
		/*
		 * special case the "lost+found" entry if pointing
		 * to where we think lost+found should be.  if that's
		 * the case, that's the one we created in phase 6.
		 * just skip it.  no need to process it and it's ..
		 * link is already accounted for.
		 */
		if (dep->inumber == orphanage_ino &&
		    strcmp(fname, ORPHANAGE) == 0)
			continue;
		/*
		 * skip entries with bogus inumbers if we're in no modify mode
		 */
		if (no_modify && verify_inum(mp, dep->inumber))
			continue;
		/*
		 * ok, now handle the rest of the cases besides '.' and '..'
		 */
		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, dep->inumber),
			XFS_INO_TO_AGINO(mp, dep->inumber));
		if (irec == NULL)  {
			nbad++;
			do_warn("entry \"%s\" in directory inode %llu points "
				"to non-existent inode, ",
				fname, ip->i_ino);
			if (!no_modify)  {
				dep->name[0] = '/';
				xfs_dir2_data_log_entry(tp, bp, dep);
				do_warn("marking entry to be junked\n");
			} else  {
				do_warn("would junk entry\n");
			}
			continue;
		}
		ino_offset =
			XFS_INO_TO_AGINO(mp, dep->inumber) - irec->ino_startnum;
		/*
		 * if it's a free inode, blow out the entry.
		 * by now, any inode that we think is free
		 * really is free.
		 */
		if (is_inode_free(irec, ino_offset))  {
			/*
			 * don't complain if this entry points to the old
			 * and now-free lost+found inode
			 */
			if (verbose || no_modify ||
			    dep->inumber != old_orphanage_ino)
				do_warn("entry \"%s\" in directory inode %llu "
					"points to free inode %llu",
					fname, ip->i_ino, dep->inumber);
			nbad++;
			if (!no_modify)  {
				if (verbose ||
				    dep->inumber != old_orphanage_ino)
					do_warn(", marking entry to be "
						"junked\n");
				else
					do_warn("\n");
				dep->name[0] = '/';
				xfs_dir2_data_log_entry(tp, bp, dep);
			} else  {
				do_warn(", would junk entry\n");
			}
			continue;
		}
		/*
		 * check easy case first, regular inode, just bump
		 * the link count and continue
		 */
		if (!inode_isadir(irec, ino_offset))  {
			add_inode_reached(irec, ino_offset);
			continue;
		}
		parent = get_inode_parent(irec, ino_offset);
		assert(parent != 0);
		/*
		 * bump up the link counts in parent and child
		 * directory but if the link doesn't agree with
		 * the .. in the child, blow out the entry.
		 * if the directory has already been reached,
		 * blow away the entry also.
		 */
		if (is_inode_reached(irec, ino_offset))  {
			junkit = 1;
			do_warn("entry \"%s\" in dir %llu points to an already "
				"connected directory inode %llu,\n", fname,
				ip->i_ino, dep->inumber);
		} else if (parent == ip->i_ino)  {
			add_inode_reached(irec, ino_offset);
			add_inode_ref(current_irec, current_ino_offset);
			if (!is_inode_refchecked(dep->inumber, irec,
					ino_offset))
				push_dir(stack, dep->inumber);
		} else  {
			junkit = 1;
			do_warn("entry \"%s\" in directory inode %llu not "
				"consistent with .. value (%llu) in ino "
				"%llu,\n",
				fname, ip->i_ino, parent, dep->inumber);
		}
		if (junkit)  {
			junkit = 0;
			nbad++;
			if (!no_modify)  {
				dep->name[0] = '/';
				xfs_dir2_data_log_entry(tp, bp, dep);
				if (verbose ||
				    dep->inumber != old_orphanage_ino)
					do_warn("\twill clear entry \"%s\"\n",
						fname);
			} else  {
				do_warn("\twould clear entry \"%s\"\n", fname);
			}
		}
	}
	*num_illegal += nbad;
	if (needscan)
		xfs_dir2_data_freescan(mp, d, &needlog, NULL);
	if (needlog)
		xfs_dir2_data_log_header(tp, bp);
	xfs_bmap_finish(&tp, &flist, firstblock, &committed);
	xfsr_trans_commit(tp, 0);
	freetab->ents[db].v = d->hdr.bestfree[0].length;
	freetab->ents[db].s = 0;
}

/*
 * Check contents of leaf-form block.
 */
int
longform_dir2_check_leaf(
	xfs_mount_t		*mp,
	xfs_inode_t		*ip,
	dir_hash_tab_t		*hashtab,
	freetab_t		*freetab)
{
	int			badtail;
	xfs_dir2_data_off_t	*bestsp;
	xfs_dabuf_t		*bp;
	xfs_dablk_t		da_bno;
	int			i;
	xfs_dir2_leaf_t		*leaf;
	xfs_dir2_leaf_tail_t	*ltp;
	int			seeval;

	da_bno = mp->m_dirleafblk;
	if (xfs_da_read_bufr(NULL, ip, da_bno, -1, &bp, XFS_DATA_FORK)) {
		do_error("can't read block %u for directory inode %llu\n",
			da_bno, ip->i_ino);
		/* NOTREACHED */
	}
	leaf = bp->data;
	ltp = XFS_DIR2_LEAF_TAIL_P(mp, leaf);
	bestsp = XFS_DIR2_LEAF_BESTS_P(ltp);
	if (leaf->hdr.info.magic != XFS_DIR2_LEAF1_MAGIC ||
	    leaf->hdr.info.forw || leaf->hdr.info.back ||
	    leaf->hdr.count < leaf->hdr.stale ||
	    leaf->hdr.count > XFS_DIR2_MAX_LEAF_ENTS(mp) ||
	    (char *)&leaf->ents[leaf->hdr.count] > (char *)bestsp) {
		do_warn("leaf block %u for directory inode %llu bad header\n",
			da_bno, ip->i_ino);
		xfs_da_brelse(NULL, bp);
		return 1;
	}
	seeval = dir_hash_see_all(hashtab, leaf->ents, leaf->hdr.count,
		leaf->hdr.stale);
	if (dir_hash_check(hashtab, ip, seeval)) {
		xfs_da_brelse(NULL, bp);
		return 1;
	}
	badtail = freetab->nents != ltp->bestcount;
	for (i = 0; !badtail && i < ltp->bestcount; i++) {
		freetab->ents[i].s = 1;
		badtail = freetab->ents[i].v != bestsp[i];
	}
	if (badtail) {
		do_warn("leaf block %u for directory inode %llu bad tail\n",
			da_bno, ip->i_ino);
		xfs_da_brelse(NULL, bp);
		return 1;
	}
	xfs_da_brelse(NULL, bp);
	return 0;
}

/*
 * Check contents of the node blocks (leaves)
 * Looks for matching hash values for the data entries.
 */
int
longform_dir2_check_node(
	xfs_mount_t		*mp,
	xfs_inode_t		*ip,
	dir_hash_tab_t		*hashtab,
	freetab_t		*freetab)
{
	xfs_dabuf_t		*bp;
	xfs_dablk_t		da_bno;
	xfs_dir2_db_t		fdb;
	xfs_dir2_free_t		*free;
	int			i;
	xfs_dir2_leaf_t		*leaf;
	xfs_fileoff_t		next_da_bno;
	int			seeval;
	int			used;

	for (da_bno = mp->m_dirleafblk, next_da_bno = 0;
	     next_da_bno != NULLFILEOFF && da_bno < mp->m_dirfreeblk;
	     da_bno = (xfs_dablk_t)next_da_bno) {
		next_da_bno = da_bno + mp->m_dirblkfsbs - 1;
		if (xfs_bmap_next_offset(NULL, ip, &next_da_bno, XFS_DATA_FORK))
			break;
		if (xfs_da_read_bufr(NULL, ip, da_bno, -1, &bp,
				XFS_DATA_FORK)) {
			do_error("can't read block %u for directory inode "
				 "%llu\n",
				da_bno, ip->i_ino);
			/* NOTREACHED */
		}
		leaf = bp->data;
		if (leaf->hdr.info.magic != XFS_DIR2_LEAFN_MAGIC) {
			if (leaf->hdr.info.magic == XFS_DA_NODE_MAGIC) {
				xfs_da_brelse(NULL, bp);
				continue;
			}
			do_warn("unknown magic number %#x for block %u in "
				"directory inode %llu\n",
				leaf->hdr.info.magic, da_bno, ip->i_ino);
			xfs_da_brelse(NULL, bp);
			return 1;
		}
		if (leaf->hdr.count < leaf->hdr.stale ||
		    leaf->hdr.count > XFS_DIR2_MAX_LEAF_ENTS(mp)) {
			do_warn("leaf block %u for directory inode %llu bad "
				"header\n",
				da_bno, ip->i_ino);
			xfs_da_brelse(NULL, bp);
			return 1;
		}
		seeval = dir_hash_see_all(hashtab, leaf->ents, leaf->hdr.count,
			leaf->hdr.stale);
		xfs_da_brelse(NULL, bp);
		if (seeval != DIR_HASH_CK_OK)
			return 1;
	}
	if (dir_hash_check(hashtab, ip, seeval))
		return 1;
	for (da_bno = mp->m_dirfreeblk, next_da_bno = 0;
	     next_da_bno != NULLFILEOFF;
	     da_bno = (xfs_dablk_t)next_da_bno) {
		next_da_bno = da_bno + mp->m_dirblkfsbs - 1;
		if (xfs_bmap_next_offset(NULL, ip, &next_da_bno, XFS_DATA_FORK))
			break;
		if (xfs_da_read_bufr(NULL, ip, da_bno, -1, &bp,
				XFS_DATA_FORK)) {
			do_error("can't read block %u for directory inode "
				 "%llu\n",
				da_bno, ip->i_ino);
			/* NOTREACHED */
		}
		free = bp->data;
		fdb = XFS_DIR2_DA_TO_DB(mp, da_bno);
		if (free->hdr.magic != XFS_DIR2_FREE_MAGIC ||
		    free->hdr.firstdb !=
			(fdb - XFS_DIR2_FREE_FIRSTDB(mp)) *
			XFS_DIR2_MAX_FREE_BESTS(mp) ||
		    free->hdr.nvalid < free->hdr.nused) {
			do_warn("free block %u for directory inode %llu bad "
				"header\n",
				da_bno, ip->i_ino);
			xfs_da_brelse(NULL, bp);
			return 1;
		}
		for (i = used = 0; i < free->hdr.nvalid; i++) {
			if (i + free->hdr.firstdb >= freetab->nents ||
			    freetab->ents[i + free->hdr.firstdb].v !=
			    free->bests[i]) {
				do_warn("free block %u entry %i for directory "
					"ino %llu bad\n",
					da_bno, i, ip->i_ino);
				xfs_da_brelse(NULL, bp);
				return 1;
			}
			used += free->bests[i] != NULLDATAOFF;
			freetab->ents[i + free->hdr.firstdb].s = 1;
		}
		if (used != free->hdr.nused) {
			do_warn("free block %u for directory inode %llu bad "
				"nused\n",
				da_bno, ip->i_ino);
			xfs_da_brelse(NULL, bp);
			return 1;
		}
		xfs_da_brelse(NULL, bp);
	}
	for (i = 0; i < freetab->nents; i++) {
		if (freetab->ents[i].s == 0) {
			do_warn("missing freetab entry %u for directory inode "
				"%llu\n",
				i, ip->i_ino);
			return 1;
		}
	}
	return 0;
}

/*
 * Rebuild a directory: set up.
 * Turn it into a node-format directory with no contents in the
 * upper area.  Also has correct freespace blocks.
 */
void
longform_dir2_rebuild_setup(
	xfs_mount_t		*mp,
	xfs_ino_t		ino,
	xfs_inode_t		*ip,
	freetab_t		*freetab)
{
	xfs_da_args_t		args;
	int			committed;
	xfs_dir2_data_t		*data;
	xfs_dabuf_t		*dbp;
	int			error;
	xfs_dir2_db_t		fbno;
	xfs_dabuf_t		*fbp;
	xfs_fsblock_t		firstblock;
	xfs_bmap_free_t		flist;
	xfs_dir2_free_t		*free;
	int			i;
	int			j;
	xfs_dablk_t		lblkno;
	xfs_dabuf_t		*lbp;
	xfs_dir2_leaf_t		*leaf;
	int			nres;
	xfs_trans_t		*tp;

	tp = xfs_trans_alloc(mp, 0);
	nres = XFS_DAENTER_SPACE_RES(mp, XFS_DATA_FORK);
	error = xfs_trans_reserve(tp,
		nres, XFS_CREATE_LOG_RES(mp), 0, XFS_TRANS_PERM_LOG_RES,
		XFS_CREATE_LOG_COUNT);
	if (error)
		res_failed(error);
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_ihold(tp, ip);
	XFS_BMAP_INIT(&flist, &firstblock);
	if (xfs_da_read_buf(tp, ip, mp->m_dirdatablk, -2, &dbp,
			XFS_DATA_FORK)) {
		do_error("can't read block %u for directory inode %llu\n",
			mp->m_dirdatablk, ino);
		/* NOTREACHED */
	}
	if (dbp && (data = dbp->data)->hdr.magic == XFS_DIR2_BLOCK_MAGIC) {
		xfs_dir2_block_t	*block;
		xfs_dir2_leaf_entry_t	*blp;
		xfs_dir2_block_tail_t	*btp;
		int			needlog;
		int			needscan;

		data->hdr.magic = XFS_DIR2_DATA_MAGIC;
		block = (xfs_dir2_block_t *)data;
		btp = XFS_DIR2_BLOCK_TAIL_P(mp, block);
		blp = XFS_DIR2_BLOCK_LEAF_P(btp);
		needlog = needscan = 0;
		xfs_dir2_data_make_free(tp, dbp, (char *)blp - (char *)block,
			(char *)block + mp->m_dirblksize - (char *)blp,
			&needlog, &needscan);
		if (needscan)
			xfs_dir2_data_freescan(mp, data, &needlog, NULL);
		xfs_da_log_buf(tp, dbp, 0, mp->m_dirblksize - 1);
	}
	bzero(&args, sizeof(args));
	args.trans = tp;
	args.dp = ip;
	args.whichfork = XFS_DATA_FORK;
	args.firstblock = &firstblock;
	args.flist = &flist;
	args.total = nres;
	if ((error = xfs_da_grow_inode(&args, &lblkno)) ||
	    (error = xfs_da_get_buf(tp, ip, lblkno, -1, &lbp, XFS_DATA_FORK))) {
		do_error("can't add btree block to directory inode %llu\n",
			ino);
		/* NOTREACHED */
	}
	leaf = lbp->data;
	bzero(leaf, mp->m_dirblksize);
	leaf->hdr.info.magic = XFS_DIR2_LEAFN_MAGIC;
	xfs_da_log_buf(tp, lbp, 0, mp->m_dirblksize - 1);
	xfs_bmap_finish(&tp, &flist, firstblock, &committed);
	xfsr_trans_commit(tp, 0);

	for (i = 0; i < freetab->nents; i += XFS_DIR2_MAX_FREE_BESTS(mp)) {
		tp = xfs_trans_alloc(mp, 0);
		nres = XFS_DAENTER_SPACE_RES(mp, XFS_DATA_FORK);
		error = xfs_trans_reserve(tp,
			nres, XFS_CREATE_LOG_RES(mp), 0, XFS_TRANS_PERM_LOG_RES,
			XFS_CREATE_LOG_COUNT);
		if (error)
			res_failed(error);
		xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
		xfs_trans_ihold(tp, ip);
		XFS_BMAP_INIT(&flist, &firstblock);
		bzero(&args, sizeof(args));
		args.trans = tp;
		args.dp = ip;
		args.whichfork = XFS_DATA_FORK;
		args.firstblock = &firstblock;
		args.flist = &flist;
		args.total = nres;
		if ((error = xfs_dir2_grow_inode(&args, XFS_DIR2_FREE_SPACE,
						 &fbno)) ||
		    (error = xfs_da_get_buf(tp, ip, XFS_DIR2_DB_TO_DA(mp, fbno),
					    -1, &fbp, XFS_DATA_FORK))) {
			do_error("can't add free block to directory inode "
				 "%llu\n",
				ino);
			/* NOTREACHED */
		}
		free = fbp->data;
		bzero(free, mp->m_dirblksize);
		free->hdr.magic = XFS_DIR2_FREE_MAGIC;
		free->hdr.firstdb = i;
		free->hdr.nvalid = XFS_DIR2_MAX_FREE_BESTS(mp);
		if (i + free->hdr.nvalid > freetab->nents)
			free->hdr.nvalid = freetab->nents - i;
		for (j = 0; j < free->hdr.nvalid; j++) {
			free->bests[j] = freetab->ents[i + j].v;
			if (free->bests[j] != NULLDATAOFF)
				free->hdr.nused++;
		}
		xfs_da_log_buf(tp, fbp, 0, mp->m_dirblksize - 1);
		xfs_bmap_finish(&tp, &flist, firstblock, &committed);
		xfsr_trans_commit(tp, 0);
	}
}

/*
 * Rebuild the entries from a single data block.
 */
void
longform_dir2_rebuild_data(
	xfs_mount_t		*mp,
	xfs_ino_t		ino,
	xfs_inode_t		*ip,
	xfs_dablk_t		da_bno)
{
	xfs_dabuf_t		*bp;
	xfs_dir2_block_tail_t	*btp;
	int			committed;
	xfs_dir2_data_t		*data;
	xfs_dir2_db_t		dbno;
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
	char			*endptr;
	int			error;
	xfs_dir2_free_t		*fblock;
	xfs_dabuf_t		*fbp;
	xfs_dir2_db_t		fdb;
	int			fi;
	xfs_fsblock_t		firstblock;
	xfs_bmap_free_t		flist;
	int			needlog;
	int			needscan;
	int			nres;
	char			*ptr;
	xfs_trans_t		*tp;

	if (xfs_da_read_buf(NULL, ip, da_bno, da_bno == 0 ? -2 : -1, &bp,
			XFS_DATA_FORK)) {
		do_error("can't read block %u for directory inode %llu\n",
			da_bno, ino);
		/* NOTREACHED */
	}
	if (da_bno == 0 && bp == NULL)
		/*
		 * The block was punched out.
		 */
		return;
	assert(bp);
	dbno = XFS_DIR2_DA_TO_DB(mp, da_bno);
	fdb = XFS_DIR2_DB_TO_FDB(mp, dbno);
	if (xfs_da_read_buf(NULL, ip, XFS_DIR2_DB_TO_DA(mp, fdb), -1, &fbp,
			XFS_DATA_FORK)) {
		do_error("can't read block %u for directory inode %llu\n",
			XFS_DIR2_DB_TO_DA(mp, fdb), ino);
		/* NOTREACHED */
	}
	data = malloc(mp->m_dirblksize);
	bcopy(bp->data, data, mp->m_dirblksize);
	ptr = (char *)data->u;
	if (data->hdr.magic == XFS_DIR2_BLOCK_MAGIC) {
		btp = XFS_DIR2_BLOCK_TAIL_P(mp, (xfs_dir2_block_t *)data);
		endptr = (char *)XFS_DIR2_BLOCK_LEAF_P(btp);
	} else
		endptr = (char *)data + mp->m_dirblksize;
	fblock = fbp->data;
	fi = XFS_DIR2_DB_TO_FDINDEX(mp, dbno);
	tp = xfs_trans_alloc(mp, 0);
	error = xfs_trans_reserve(tp, 0, XFS_CREATE_LOG_RES(mp), 0,
		XFS_TRANS_PERM_LOG_RES, XFS_CREATE_LOG_COUNT);
	if (error)
		res_failed(error);
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_ihold(tp, ip);
	xfs_da_bjoin(tp, bp);
	xfs_da_bhold(tp, bp);
	xfs_da_bjoin(tp, fbp);
	xfs_da_bhold(tp, fbp);
	XFS_BMAP_INIT(&flist, &firstblock);
	needlog = needscan = 0;
	bzero(((xfs_dir2_data_t *)(bp->data))->hdr.bestfree,
		sizeof(data->hdr.bestfree));
	xfs_dir2_data_make_free(tp, bp, (xfs_dir2_data_aoff_t)sizeof(data->hdr),
		mp->m_dirblksize - sizeof(data->hdr), &needlog, &needscan);
	assert(needscan == 0);
	xfs_dir2_data_log_header(tp, bp);
	fblock->bests[fi] =
		((xfs_dir2_data_t *)(bp->data))->hdr.bestfree[0].length;
	xfs_dir2_free_log_bests(tp, fbp, fi, fi);
	xfs_bmap_finish(&tp, &flist, firstblock, &committed);
	xfsr_trans_commit(tp, 0);

	while (ptr < endptr) {
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (dup->freetag == XFS_DIR2_DATA_FREE_TAG) {
			ptr += dup->length;
			continue;
		}
		dep = (xfs_dir2_data_entry_t *)ptr;
		ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
		if (dep->name[0] == '/')
			continue;
		tp = xfs_trans_alloc(mp, 0);
		nres = XFS_CREATE_SPACE_RES(mp, dep->namelen);
		error = xfs_trans_reserve(tp, nres, XFS_CREATE_LOG_RES(mp), 0,
			XFS_TRANS_PERM_LOG_RES, XFS_CREATE_LOG_COUNT);
		if (error)
			res_failed(error);
		xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
		xfs_trans_ihold(tp, ip);
		xfs_da_bjoin(tp, bp);
		xfs_da_bhold(tp, bp);
		xfs_da_bjoin(tp, fbp);
		xfs_da_bhold(tp, fbp);
		XFS_BMAP_INIT(&flist, &firstblock);
		error = XFS_DIR_CREATENAME(mp, tp, ip, (char *)dep->name,
			dep->namelen, dep->inumber, &firstblock, &flist, nres);
		assert(error == 0);
		xfs_bmap_finish(&tp, &flist, firstblock, &committed);
		xfsr_trans_commit(tp, 0);
	}
	xfs_da_brelse(NULL, bp);
	xfs_da_brelse(NULL, fbp);
	free(data);
}

/*
 * Finish the rebuild of a directory.
 * Stuff / in and then remove it, this forces the directory to end 
 * up in the right format.
 */
void
longform_dir2_rebuild_finish(
	xfs_mount_t		*mp,
	xfs_ino_t		ino,
	xfs_inode_t		*ip)
{
	int			committed;
	int			error;
	xfs_fsblock_t		firstblock;
	xfs_bmap_free_t		flist;
	int			nres;
	xfs_trans_t		*tp;

	tp = xfs_trans_alloc(mp, 0);
	nres = XFS_CREATE_SPACE_RES(mp, 1);
	error = xfs_trans_reserve(tp, nres, XFS_CREATE_LOG_RES(mp), 0,
		XFS_TRANS_PERM_LOG_RES, XFS_CREATE_LOG_COUNT);
	if (error)
		res_failed(error);
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_ihold(tp, ip);
	XFS_BMAP_INIT(&flist, &firstblock);
	error = XFS_DIR_CREATENAME(mp, tp, ip, "/", 1, ino,
			&firstblock, &flist, nres);
	assert(error == 0);
	xfs_bmap_finish(&tp, &flist, firstblock, &committed);
	xfsr_trans_commit(tp, 0);

	/* could kill trailing empty data blocks here */

	tp = xfs_trans_alloc(mp, 0);
	nres = XFS_REMOVE_SPACE_RES(mp);
	error = xfs_trans_reserve(tp, nres, XFS_REMOVE_LOG_RES(mp), 0,
		XFS_TRANS_PERM_LOG_RES, XFS_REMOVE_LOG_COUNT);
	if (error)
		res_failed(error);
	xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
	xfs_trans_ihold(tp, ip);
	XFS_BMAP_INIT(&flist, &firstblock);
	error = XFS_DIR_REMOVENAME(mp, tp, ip, "/", 1, ino,
			&firstblock, &flist, nres);
	assert(error == 0);
	xfs_bmap_finish(&tp, &flist, firstblock, &committed);
	xfsr_trans_commit(tp, 0);
}

/*
 * Rebuild a directory.
 * Remove all the non-data blocks.
 * Re-initialize to (empty) node form.
 * Loop over the data blocks reinserting each entry.
 * Force the directory into the right format.
 */
void
longform_dir2_rebuild(
	xfs_mount_t	*mp,
	xfs_ino_t	ino,
	xfs_inode_t	*ip,
	int		*num_illegal,
	freetab_t	*freetab,
	int		isblock)
{
	xfs_dabuf_t	*bp;
	xfs_dablk_t	da_bno;
	xfs_fileoff_t	next_da_bno;

	do_warn("rebuilding directory inode %llu\n", ino);
	for (da_bno = mp->m_dirleafblk, next_da_bno = isblock ? NULLFILEOFF : 0;
	     next_da_bno != NULLFILEOFF;
	     da_bno = (xfs_dablk_t)next_da_bno) {
		next_da_bno = da_bno + mp->m_dirblkfsbs - 1;
		if (xfs_bmap_next_offset(NULL, ip, &next_da_bno, XFS_DATA_FORK))
			break;
		if (xfs_da_get_buf(NULL, ip, da_bno, -1, &bp, XFS_DATA_FORK)) {
			do_error("can't get block %u for directory inode "
				 "%llu\n",
				da_bno, ino);
			/* NOTREACHED */
		}
		dir2_kill_block(mp, ip, da_bno, bp);
	}
	longform_dir2_rebuild_setup(mp, ino, ip, freetab);
	for (da_bno = mp->m_dirdatablk, next_da_bno = 0;
	     da_bno < mp->m_dirleafblk && next_da_bno != NULLFILEOFF;
	     da_bno = (xfs_dablk_t)next_da_bno) {
		next_da_bno = da_bno + mp->m_dirblkfsbs - 1;
		if (xfs_bmap_next_offset(NULL, ip, &next_da_bno, XFS_DATA_FORK))
			break;
		longform_dir2_rebuild_data(mp, ino, ip, da_bno);
	}
	longform_dir2_rebuild_finish(mp, ino, ip);
	*num_illegal = 0;
}

/*
 * succeeds or dies, inode never gets dirtied since all changes
 * happen in file blocks.  the inode size and other core info
 * is already correct, it's just the leaf entries that get altered.
 * XXX above comment is wrong for v2 - need to see why it matters
 */
void
longform_dir2_entry_check(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_inode_t	*ip,
			int		*num_illegal,
			int		*need_dot,
			dir_stack_t	*stack,
			ino_tree_node_t	*irec,
			int		ino_offset)
{
	xfs_dir2_block_t	*block;
	xfs_dir2_leaf_entry_t	*blp;
	xfs_dabuf_t		*bp;
	xfs_dir2_block_tail_t	*btp;
	xfs_dablk_t		da_bno;
	freetab_t		*freetab;
	dir_hash_tab_t		*hashtab;
	int			i;
	int			isblock;
	int			isleaf;
	xfs_fileoff_t		next_da_bno;
	int			seeval;
	int			fixit;

	*need_dot = 1;
	freetab = malloc(FREETAB_SIZE(ip->i_d.di_size / mp->m_dirblksize));
	freetab->naents = ip->i_d.di_size / mp->m_dirblksize;
	freetab->nents = 0;
	for (i = 0; i < freetab->naents; i++) {
		freetab->ents[i].v = NULLDATAOFF;
		freetab->ents[i].s = 0;
	}
	xfs_dir2_isblock(NULL, ip, &isblock);
	xfs_dir2_isleaf(NULL, ip, &isleaf);
	hashtab = dir_hash_init(ip->i_d.di_size);
	for (da_bno = 0, next_da_bno = 0;
	     next_da_bno != NULLFILEOFF && da_bno < mp->m_dirleafblk;
	     da_bno = (xfs_dablk_t)next_da_bno) {
		next_da_bno = da_bno + mp->m_dirblkfsbs - 1;
		if (xfs_bmap_next_offset(NULL, ip, &next_da_bno, XFS_DATA_FORK))
			break;
		if (xfs_da_read_bufr(NULL, ip, da_bno, -1, &bp,
				XFS_DATA_FORK)) {
			do_error("can't read block %u for directory inode "
				 "%llu\n",
				da_bno, ino);
			/* NOTREACHED */
		}
		longform_dir2_entry_check_data(mp, ip, num_illegal, need_dot,
			stack, irec, ino_offset, &bp, hashtab, &freetab, da_bno,
			isblock);
		/* it releases the buffer unless isblock is set */
	}
	fixit = (*num_illegal != 0) || dir2_is_badino(ino);
	if (isblock) {
		assert(bp);
		block = bp->data;
		btp = XFS_DIR2_BLOCK_TAIL_P(mp, block);
		blp = XFS_DIR2_BLOCK_LEAF_P(btp);
		seeval = dir_hash_see_all(hashtab, blp, btp->count, btp->stale);
		if (dir_hash_check(hashtab, ip, seeval))
			fixit |= 1;
		xfs_da_brelse(NULL, bp);
	} else if (isleaf) {
		fixit |= longform_dir2_check_leaf(mp, ip, hashtab, freetab);
	} else {
		fixit |= longform_dir2_check_node(mp, ip, hashtab, freetab);
	}
	dir_hash_done(hashtab);
	if (!no_modify && fixit)
		longform_dir2_rebuild(mp, ino, ip, num_illegal, freetab,
			isblock);
	free(freetab);
}
#endif	/* VERS >= V_654 */

/*
 * shortform directory processing routines -- entry verification and
 * bad entry deletion (pruning).
 */
void
shortform_dir_entry_check(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_inode_t	*ip,
			int		*ino_dirty,
			dir_stack_t	*stack,
			ino_tree_node_t	*current_irec,
			int		current_ino_offset)
{
	xfs_ino_t		lino;
	xfs_ino_t		parent;
	xfs_dir_shortform_t	*sf;
	xfs_dir_sf_entry_t	*sf_entry, *next_sfe, *tmp_sfe;
	xfs_ifork_t		*ifp;
	ino_tree_node_t		*irec;
	int			max_size;
	int			ino_offset;
	int			i;
	int			junkit;
	int			tmp_len;
	int			tmp_elen;
	int			bad_sfnamelen;
	int			namelen;
	int			bytes_deleted;
	char			fname[MAXNAMELEN + 1];

	ifp = &ip->i_df;
	sf = (xfs_dir_shortform_t *) ifp->if_u1.if_data;
	*ino_dirty = 0;
	bytes_deleted = 0;

	max_size = ifp->if_bytes;
	assert(ip->i_d.di_size <= ifp->if_bytes);

	/*
	 * no '.' entry in shortform dirs, just bump up ref count by 1
	 * '..' was already (or will be) accounted for and checked when
	 * the directory is reached or will be taken care of when the
	 * directory is moved to orphanage.
	 */
	add_inode_ref(current_irec, current_ino_offset);

	/*
	 * now run through entries, stop at first bad entry, don't need
	 * to skip over '..' since that's encoded in its own field and
	 * no need to worry about '.' since it doesn't exist.
	 */
	sf_entry = next_sfe = &sf->list[0];
	if (sf == NULL) { 
		junkit = 1;
		do_warn("shortform dir inode %llu has null data entries \n", ino);

		}
	else {
	   for (i = 0; i < sf->hdr.count && max_size >
					(__psint_t)next_sfe - (__psint_t)sf;
			sf_entry = next_sfe, i++)  {
		junkit = 0;
		bad_sfnamelen = 0;
		tmp_sfe = NULL;

		XFS_DIR_SF_GET_DIRINO(&sf_entry->inumber, &lino);

		namelen = sf_entry->namelen;

		assert(no_modify || namelen > 0);

		if (no_modify && namelen == 0)  {
			/*
			 * if we're really lucky, this is
			 * the last entry in which case we
			 * can use the dir size to set the
			 * namelen value.  otherwise, forget
			 * it because we're not going to be
			 * able to find the next entry.
			 */
			bad_sfnamelen = 1;

			if (i == sf->hdr.count - 1)  {
				namelen = ip->i_d.di_size -
					((__psint_t) &sf_entry->name[0] -
					 (__psint_t) sf);
			} else  {
				/*
				 * don't process the rest of the directory,
				 * break out of processing looop
				 */
				break;
			}
		} else if (no_modify && (__psint_t) sf_entry - (__psint_t) sf +
				+ XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry)
				> ip->i_d.di_size)  {
			bad_sfnamelen = 1;

			if (i == sf->hdr.count - 1)  {
				namelen = ip->i_d.di_size -
					((__psint_t) &sf_entry->name[0] -
					 (__psint_t) sf);
			} else  {
				/*
				 * don't process the rest of the directory,
				 * break out of processing looop
				 */
				break;
			}
		}

		bcopy(sf_entry->name, fname, sf_entry->namelen);
		fname[sf_entry->namelen] = '\0';

		assert(no_modify || lino != NULLFSINO);
		assert(no_modify || !verify_inum(mp, lino));

		/*
		 * special case the "lost+found" entry if it's pointing
		 * to where we think lost+found should be.  if that's
		 * the case, that's the one we created in phase 6.
		 * just skip it.  no need to process it and its ..
		 * link is already accounted for.  Also skip entries
		 * with bogus inode numbers if we're in no modify mode.
		 */

		if (lino == orphanage_ino && strcmp(fname, ORPHANAGE) == 0
				|| no_modify && verify_inum(mp, lino))  {
			next_sfe = (xfs_dir_sf_entry_t *)
				((__psint_t) sf_entry +
				XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry));
			continue;
		}

		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, lino),
					XFS_INO_TO_AGINO(mp, lino));

		if (irec == NULL && no_modify)  {
			do_warn(
"entry \"%s\" in shortform dir %llu references non-existent ino %llu\n",
				fname, ino, lino);
			do_warn("would junk entry\n");
			continue;
		}

		assert(irec != NULL);

		ino_offset = XFS_INO_TO_AGINO(mp, lino) - irec->ino_startnum;

		/*
		 * if it's a free inode, blow out the entry.
		 * by now, any inode that we think is free
		 * really is free.
		 */
		if (is_inode_free(irec, ino_offset))  {
			/*
			 * don't complain if this entry points to the old
			 * and now-free lost+found inode
			 */
			if (verbose || no_modify || lino != old_orphanage_ino)
				do_warn(
	"entry \"%s\" in shortform dir inode %llu points to free inode %llu\n",
					fname, ino, lino);

			if (!no_modify)  {
				junkit = 1;
			} else  {
				do_warn("would junk entry \"%s\"\n",
					fname);
			}
		} else if (!inode_isadir(irec, ino_offset))  {
			/*
			 * check easy case first, regular inode, just bump
			 * the link count and continue
			 */
			add_inode_reached(irec, ino_offset);

			next_sfe = (xfs_dir_sf_entry_t *)
				((__psint_t) sf_entry +
				XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry));
			continue;
		} else  {
			parent = get_inode_parent(irec, ino_offset);

			/*
			 * bump up the link counts in parent and child.
			 * directory but if the link doesn't agree with
			 * the .. in the child, blow out the entry
			 */
			if (is_inode_reached(irec, ino_offset))  {
				junkit = 1;
				do_warn(
	"entry \"%s\" in dir %llu references already connected dir ino %llu,\n",
					fname, ino, lino);
			} else if (parent == ino)  {
				add_inode_reached(irec, ino_offset);
				add_inode_ref(current_irec, current_ino_offset);

				if (!is_inode_refchecked(lino, irec,
						ino_offset))
					push_dir(stack, lino);
			} else  {
				junkit = 1;
				do_warn(
"entry \"%s\" in dir %llu not consistent with .. value (%llu) in dir ino %llu,\n",
					fname, ino, parent, lino);
			}
		}

		if (junkit)  {
			if (!no_modify)  {
				tmp_elen = XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry);
				tmp_sfe = (xfs_dir_sf_entry_t *)
					((__psint_t) sf_entry + tmp_elen);
				tmp_len = max_size - ((__psint_t) tmp_sfe
							- (__psint_t) sf);
				max_size -= tmp_elen;
				bytes_deleted += tmp_elen;

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
				 * accurate comparisons in the loop test
				 */
				i--;

				*ino_dirty = 1;

				if (verbose || lino != old_orphanage_ino)
					do_warn(
			"junking entry \"%s\" in directory inode %llu\n",
						fname, lino);
			} else  {
				do_warn("would junk entry \"%s\"\n", fname);
			}
		}

		/*
		 * go onto next entry unless we've just junked an
		 * entry in which the current entry pointer points
		 * to an unprocessed entry.  have to take into entries
		 * with bad namelen into account in no modify mode since we
		 * calculate size based on next_sfe.
		 */
		assert(no_modify || bad_sfnamelen == 0);

		next_sfe = (tmp_sfe == NULL)
			? (xfs_dir_sf_entry_t *) ((__psint_t) sf_entry
				+ ((!bad_sfnamelen)
					? XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry)
					: sizeof(xfs_dir_sf_entry_t) - 1
						+ namelen))
			: tmp_sfe;
	    }
	}

	/*
	 * sync up sizes if required
	 */
	if (*ino_dirty)  {
		assert(bytes_deleted > 0);
		assert(!no_modify);
		xfs_idata_realloc(ip, -bytes_deleted, XFS_DATA_FORK);
		ip->i_d.di_size -= bytes_deleted;
	}

	if (ip->i_d.di_size != ip->i_df.if_bytes)  {
		assert(ip->i_df.if_bytes == (xfs_fsize_t)
				((__psint_t) next_sfe - (__psint_t) sf));
		ip->i_d.di_size = (xfs_fsize_t)
				((__psint_t) next_sfe - (__psint_t) sf);
		do_warn(
		"setting size to %lld bytes to reflect junked entries\n",
				ip->i_d.di_size);
		*ino_dirty = 1;
	}

	return;
}

/* ARGSUSED */
void
prune_sf_dir_entry(xfs_mount_t *mp, xfs_ino_t ino, xfs_inode_t *ip)
{
				/* REFERENCED */
	xfs_ino_t		lino;
	xfs_dir_shortform_t	*sf;
	xfs_dir_sf_entry_t	*sf_entry, *next_sfe, *tmp_sfe;
	xfs_ifork_t		*ifp;
	int			max_size;
	int			i;
	int			tmp_len;
	int			tmp_elen;
	int			bytes_deleted;
	char			fname[MAXNAMELEN + 1];

	ifp = &ip->i_df;
	sf = (xfs_dir_shortform_t *) ifp->if_u1.if_data;
	bytes_deleted = 0;

	max_size = ifp->if_bytes;
	assert(ip->i_d.di_size <= ifp->if_bytes);

	/*
	 * now run through entries and delete every bad entry
	 */
	sf_entry = next_sfe = &sf->list[0];

	for (i = 0; i < sf->hdr.count && max_size >
					(__psint_t)next_sfe - (__psint_t)sf;
			sf_entry = next_sfe, i++)  {
		tmp_sfe = NULL;

		XFS_DIR_SF_GET_DIRINO(&sf_entry->inumber, &lino);

		bcopy(sf_entry->name, fname, sf_entry->namelen);
		fname[sf_entry->namelen] = '\0';

		if (sf_entry->name[0] == '/')  {
			if (!no_modify)  {
				tmp_elen = XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry);
				tmp_sfe = (xfs_dir_sf_entry_t *)
					((__psint_t) sf_entry + tmp_elen);
				tmp_len = max_size - ((__psint_t) tmp_sfe
							- (__psint_t) sf);
				max_size -= tmp_elen;
				bytes_deleted += tmp_elen;

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
				 * accurate comparisons in the loop test
				 */
				i--;
			}
		}
		next_sfe = (tmp_sfe == NULL)
			? (xfs_dir_sf_entry_t *) ((__psint_t) sf_entry +
				XFS_DIR_SF_ENTSIZE_BYENTRY(sf_entry))
			: tmp_sfe;
	}

	/*
	 * sync up sizes if required
	 */
	if (bytes_deleted > 0)  {
		xfs_idata_realloc(ip, -bytes_deleted, XFS_DATA_FORK);
		ip->i_d.di_size -= bytes_deleted;
	}

	if (ip->i_d.di_size != ip->i_df.if_bytes)  {
		assert(ip->i_df.if_bytes == (xfs_fsize_t)
				((__psint_t) next_sfe - (__psint_t) sf));
		ip->i_d.di_size = (xfs_fsize_t)
				((__psint_t) next_sfe - (__psint_t) sf);
		do_warn(
		"setting size to %lld bytes to reflect junked entries\n",
				ip->i_d.di_size);
	}

	return;
}

#if VERS >= V_654
/*
 * shortform directory v2 processing routines -- entry verification and
 * bad entry deletion (pruning).
 */
void
shortform_dir2_entry_check(xfs_mount_t	*mp,
			xfs_ino_t	ino,
			xfs_inode_t	*ip,
			int		*ino_dirty,
			dir_stack_t	*stack,
			ino_tree_node_t	*current_irec,
			int		current_ino_offset)
{
	xfs_ino_t		lino;
	xfs_ino_t		parent;
	xfs_dir2_sf_t		*sfp;
	xfs_dir2_sf_entry_t	*sfep, *next_sfep, *tmp_sfep;
	xfs_ifork_t		*ifp;
	ino_tree_node_t		*irec;
	int			max_size;
	int			ino_offset;
	int			i;
	int			junkit;
	int			tmp_len;
	int			tmp_elen;
	int			bad_sfnamelen;
	int			namelen;
	int			bytes_deleted;
	char			fname[MAXNAMELEN + 1];
	int			i8;

	ifp = &ip->i_df;
	sfp = (xfs_dir2_sf_t *) ifp->if_u1.if_data;
	*ino_dirty = 0;
	bytes_deleted = i8 = 0;

	max_size = ifp->if_bytes;
	assert(ip->i_d.di_size <= ifp->if_bytes);

	/*
	 * no '.' entry in shortform dirs, just bump up ref count by 1
	 * '..' was already (or will be) accounted for and checked when
	 * the directory is reached or will be taken care of when the
	 * directory is moved to orphanage.
	 */
	add_inode_ref(current_irec, current_ino_offset);

	/*
	 * now run through entries, stop at first bad entry, don't need
	 * to skip over '..' since that's encoded in its own field and
	 * no need to worry about '.' since it doesn't exist.
	 */
	sfep = next_sfep = XFS_DIR2_SF_FIRSTENTRY(sfp);

	for (i = 0; i < sfp->hdr.count && max_size >
					(__psint_t)next_sfep - (__psint_t)sfp;
			sfep = next_sfep, i++)  {
		junkit = 0;
		bad_sfnamelen = 0;
		tmp_sfep = NULL;

		lino = XFS_DIR2_SF_GET_INUMBER(sfp, XFS_DIR2_SF_INUMBERP(sfep));

		namelen = sfep->namelen;

		assert(no_modify || namelen > 0);

		if (no_modify && namelen == 0)  {
			/*
			 * if we're really lucky, this is
			 * the last entry in which case we
			 * can use the dir size to set the
			 * namelen value.  otherwise, forget
			 * it because we're not going to be
			 * able to find the next entry.
			 */
			bad_sfnamelen = 1;

			if (i == sfp->hdr.count - 1)  {
				namelen = ip->i_d.di_size -
					((__psint_t) &sfep->name[0] -
					 (__psint_t) sfp);
			} else  {
				/*
				 * don't process the rest of the directory,
				 * break out of processing loop
				 */
				break;
			}
		} else if (no_modify && (__psint_t) sfep - (__psint_t) sfp +
				+ XFS_DIR2_SF_ENTSIZE_BYENTRY(sfp, sfep)
				> ip->i_d.di_size)  {
			bad_sfnamelen = 1;

			if (i == sfp->hdr.count - 1)  {
				namelen = ip->i_d.di_size -
					((__psint_t) &sfep->name[0] -
					 (__psint_t) sfp);
			} else  {
				/*
				 * don't process the rest of the directory,
				 * break out of processing loop
				 */
				break;
			}
		}

		bcopy(sfep->name, fname, sfep->namelen);
		fname[sfep->namelen] = '\0';

		assert(no_modify || (lino != NULLFSINO && lino != 0));
		assert(no_modify || !verify_inum(mp, lino));

		/*
		 * special case the "lost+found" entry if it's pointing
		 * to where we think lost+found should be.  if that's
		 * the case, that's the one we created in phase 6.
		 * just skip it.  no need to process it and its ..
		 * link is already accounted for.  Also skip entries
		 * with bogus inode numbers if we're in no modify mode.
		 */

		if (lino == orphanage_ino && strcmp(fname, ORPHANAGE) == 0
				|| no_modify && verify_inum(mp, lino))  {
			next_sfep = (xfs_dir2_sf_entry_t *)
				((__psint_t) sfep +
				XFS_DIR2_SF_ENTSIZE_BYENTRY(sfp, sfep));
			continue;
		}

		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, lino),
					XFS_INO_TO_AGINO(mp, lino));

		if (irec == NULL && no_modify)  {
			do_warn("entry \"%s\" in shortform directory %llu "
				"references non-existent inode %llu\n",
				fname, ino, lino);
			do_warn("would junk entry\n");
			continue;
		}

		assert(irec != NULL);

		ino_offset = XFS_INO_TO_AGINO(mp, lino) - irec->ino_startnum;

		/*
		 * if it's a free inode, blow out the entry.
		 * by now, any inode that we think is free
		 * really is free.
		 */
		if (is_inode_free(irec, ino_offset))  {
			/*
			 * don't complain if this entry points to the old
			 * and now-free lost+found inode
			 */
			if (verbose || no_modify || lino != old_orphanage_ino)
				do_warn("entry \"%s\" in shortform directory "
					"inode %llu points to free inode "
					"%llu\n",
					fname, ino, lino);

			if (!no_modify)  {
				junkit = 1;
			} else  {
				do_warn("would junk entry \"%s\"\n",
					fname);
			}
		} else if (!inode_isadir(irec, ino_offset))  {
			/*
			 * check easy case first, regular inode, just bump
			 * the link count and continue
			 */
			add_inode_reached(irec, ino_offset);

			next_sfep = (xfs_dir2_sf_entry_t *)
				((__psint_t) sfep +
				XFS_DIR2_SF_ENTSIZE_BYENTRY(sfp, sfep));
			continue;
		} else  {
			parent = get_inode_parent(irec, ino_offset);

			/*
			 * bump up the link counts in parent and child.
			 * directory but if the link doesn't agree with
			 * the .. in the child, blow out the entry
			 */
			if (is_inode_reached(irec, ino_offset))  {
				junkit = 1;
				do_warn("entry \"%s\" in directory inode %llu "
					"references already connected inode "
					"%llu,\n",
					fname, ino, lino);
			} else if (parent == ino)  {
				add_inode_reached(irec, ino_offset);
				add_inode_ref(current_irec, current_ino_offset);

				if (!is_inode_refchecked(lino, irec,
						ino_offset))
					push_dir(stack, lino);
			} else  {
				junkit = 1;
				do_warn("entry \"%s\" in directory inode %llu "
					"not consistent with .. value (%llu) "
					"in inode %llu,\n",
					fname, ino, parent, lino);
			}
		}

		if (junkit)  {
			if (!no_modify)  {
				tmp_elen = XFS_DIR2_SF_ENTSIZE_BYENTRY(sfp, sfep);
				tmp_sfep = (xfs_dir2_sf_entry_t *)
					((__psint_t) sfep + tmp_elen);
				tmp_len = max_size - ((__psint_t) tmp_sfep
							- (__psint_t) sfp);
				max_size -= tmp_elen;
				bytes_deleted += tmp_elen;

				memmove(sfep, tmp_sfep, tmp_len);

				sfp->hdr.count--;
				bzero((void *) ((__psint_t) sfep + tmp_len),
						tmp_elen);

				/*
				 * set the tmp value to the current
				 * pointer so we'll process the entry
				 * we just moved up
				 */
				tmp_sfep = sfep;

				/*
				 * WARNING:  drop the index i by one
				 * so it matches the decremented count for
				 * accurate comparisons in the loop test
				 */
				i--;

				*ino_dirty = 1;

				if (verbose || lino != old_orphanage_ino)
					do_warn("junking entry \"%s\" in "
						"directory inode %llu\n",
						fname, lino);
			} else  {
				do_warn("would junk entry \"%s\"\n", fname);
			}
		} else if (lino > XFS_DIR2_MAX_SHORT_INUM)
			i8++;

		/*
		 * go onto next entry unless we've just junked an
		 * entry in which the current entry pointer points
		 * to an unprocessed entry.  have to take into entries
		 * with bad namelen into account in no modify mode since we
		 * calculate size based on next_sfep.
		 */
		assert(no_modify || bad_sfnamelen == 0);

		next_sfep = (tmp_sfep == NULL)
			? (xfs_dir2_sf_entry_t *) ((__psint_t) sfep
				+ ((!bad_sfnamelen)
					? XFS_DIR2_SF_ENTSIZE_BYENTRY(sfp, sfep)
					: XFS_DIR2_SF_ENTSIZE_BYNAME(sfp, namelen)))
			: tmp_sfep;
	}

	if (sfp->hdr.i8count != i8) {
		if (no_modify) {
			do_warn("would fix i8count in inode %llu\n", ino);
		} else {
			if (i8 == 0) {
				tmp_sfep = next_sfep;
				process_sf_dir2_fixi8(sfp, &tmp_sfep);
				bytes_deleted +=
					(__psint_t)next_sfep -
					(__psint_t)tmp_sfep;
				next_sfep = tmp_sfep;
			} else
				sfp->hdr.i8count = i8;
			*ino_dirty = 1;
			do_warn("fixing i8count in inode %llu\n", ino);
		}
	}

	/*
	 * sync up sizes if required
	 */
	if (*ino_dirty)  {
		assert(bytes_deleted > 0);
		assert(!no_modify);
		xfs_idata_realloc(ip, -bytes_deleted, XFS_DATA_FORK);
		ip->i_d.di_size -= bytes_deleted;
	}

	if (ip->i_d.di_size != ip->i_df.if_bytes)  {
		assert(ip->i_df.if_bytes == (xfs_fsize_t)
				((__psint_t) next_sfep - (__psint_t) sfp));
		ip->i_d.di_size = (xfs_fsize_t)
				((__psint_t) next_sfep - (__psint_t) sfp);
		do_warn("setting size to %lld bytes to reflect junked "
			"entries\n",
			ip->i_d.di_size);
		*ino_dirty = 1;
	}

	return;
}
#endif	/* VERS >= V_654 */

/*
 * processes all directories reachable via the inodes on the stack
 * returns 0 if things are good, 1 if there's a problem
 */
void
process_dirstack(xfs_mount_t *mp, dir_stack_t *stack)
{
	xfs_bmap_free_t		flist;
	xfs_fsblock_t		first;
	xfs_ino_t		ino;
	xfs_inode_t		*ip;
	xfs_trans_t		*tp;
	xfs_dahash_t		hashval;
	ino_tree_node_t		*irec;
	int			ino_offset, need_dot, committed;
	int			dirty, num_illegal, error, nres;

	/*
	 * pull directory inode # off directory stack
	 *
	 * open up directory inode, check all entries,
	 * then call prune_dir_entries to remove all
	 * remaining illegal directory entries.
	 */

	while ((ino = pop_dir(stack)) != NULLFSINO)  {
		irec = find_inode_rec(XFS_INO_TO_AGNO(mp, ino),
					XFS_INO_TO_AGINO(mp, ino));
		assert(irec != NULL);

		ino_offset = XFS_INO_TO_AGINO(mp, ino) - irec->ino_startnum;

		assert(!is_inode_refchecked(ino, irec, ino_offset));

		if (error = xfs_iget(mp, NULL, ino, XFS_ILOCK_EXCL, &ip, 0))  {
			if (!no_modify)
				do_error("couldn't map inode %llu, err = %d\n",
					ino, error);
			else  {
				do_warn("couldn't map inode %llu, err = %d\n",
					ino, error);
				/*
				 * see below for what we're doing if this
				 * is root.  Why do we need to do this here?
				 * to ensure that the root doesn't show up
				 * as being disconnected in the no_modify case.
				 */
				if (mp->m_sb.sb_rootino == ino)  {
					add_inode_reached(irec, 0);
					add_inode_ref(irec, 0);
				}
			}

			add_inode_refchecked(ino, irec, 0);
			continue;
		}

		need_dot = dirty = num_illegal = 0;

		if (mp->m_sb.sb_rootino == ino)  {
			/*
			 * mark root inode reached and bump up
			 * link count for root inode to account
			 * for '..' entry since the root inode is
			 * never reached by a parent.  we know
			 * that root's '..' is always good --
			 * guaranteed by phase 3 and/or below.
			 */
			add_inode_reached(irec, ino_offset);
			/*
			 * account for link for the orphanage
			 * "lost+found".  if we're running in
			 * modify mode and it already existed,
			 * we deleted it so it's '..' reference
			 * never got counted.  so add it here if
			 * we're going to create lost+found.
			 *
			 * if we're running in no_modify mode,
			 * we never deleted lost+found and we're
			 * not going to create it so do nothing.
			 *
			 * either way, the counts will match when
			 * we look at the root inode's nlinks
			 * field and compare that to our incore
			 * count in phase 7.
			 */
			if (!no_modify)
				add_inode_ref(irec, ino_offset);
		}

		add_inode_refchecked(ino, irec, ino_offset);

		/*
		 * look for bogus entries
		 */
		switch (ip->i_d.di_format)  {
		case XFS_DINODE_FMT_EXTENTS:
		case XFS_DINODE_FMT_BTREE:
			/*
			 * also check for missing '.' in longform dirs.
			 * missing .. entries are added if required when
			 * the directory is connected to lost+found. but
			 * we need to create '.' entries here.
			 */
#if VERS >= V_654
			if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
				longform_dir2_entry_check(mp, ino, ip,
							&num_illegal, &need_dot,
							stack, irec,
							ino_offset);
			else
#endif
				longform_dir_entry_check(mp, ino, ip,
							&num_illegal, &need_dot,
							stack, irec,
							ino_offset);
			break;
		case XFS_DINODE_FMT_LOCAL:
			tp = xfs_trans_alloc(mp, 0);
			/*
			 * using the remove reservation is overkill
			 * since at most we'll only need to log the
			 * inode but it's easier than wedging a
			 * new define in ourselves.
			 */
			nres = no_modify ? 0 : XFS_REMOVE_SPACE_RES(mp);
			error = xfs_trans_reserve(tp, nres,
					XFS_REMOVE_LOG_RES(mp), 0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_REMOVE_LOG_COUNT);
			if (error)
				res_failed(error);

			xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
			xfs_trans_ihold(tp, ip);

#if VERS >= V_654
			if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
				shortform_dir2_entry_check(mp, ino, ip, &dirty,
							stack, irec,
							ino_offset);
			else
#endif
				shortform_dir_entry_check(mp, ino, ip, &dirty,
							stack, irec,
							ino_offset);

			assert(dirty == 0 || dirty && !no_modify);
			if (dirty)  {
				xfs_trans_log_inode(tp, ip,
					XFS_ILOG_CORE | XFS_ILOG_DDATA);
				xfsr_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES
						|XFS_TRANS_SYNC);
			} else  {
				xfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES);
			}
			break;
		default:
			break;
		}

		hashval = 0;

		if (!no_modify && !orphanage_entered &&
		    ino == mp->m_sb.sb_rootino) {
			do_warn("re-entering %s into root directory\n",
				ORPHANAGE);
			tp = xfs_trans_alloc(mp, 0);
#if VERS >= V_653
			nres = XFS_MKDIR_SPACE_RES(mp, strlen(ORPHANAGE));
#else
			nres = XFS_MKDIR_SPACE_RES(mp, ORPHANAGE);
#endif
			error = xfs_trans_reserve(tp, nres,
					XFS_MKDIR_LOG_RES(mp), 0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_MKDIR_LOG_COUNT);
			if (error)
				res_failed(error);
			xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
			xfs_trans_ihold(tp, ip);
			XFS_BMAP_INIT(&flist, &first);
			if (error = XFS_DIR_CREATENAME(mp, tp, ip, ORPHANAGE,
						strlen(ORPHANAGE),
						orphanage_ino, &first, &flist,
						nres))
				do_error("can't make %s entry in root inode "
					 "%llu, createname error %d\n",
					ORPHANAGE, ino, error);
			xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);
			error = xfs_bmap_finish(&tp, &flist, first, &committed);
			assert(error == 0);
			xfsr_trans_commit(tp,
				XFS_TRANS_RELEASE_LOG_RES | XFS_TRANS_SYNC);
			orphanage_entered = 1;
		}

		/*
		 * if we have to create a .. for /, do it now *before*
		 * we delete the bogus entries, otherwise the directory
		 * could transform into a shortform dir which would
		 * probably cause the simulation to choke.  Even
		 * if the illegal entries get shifted around, it's ok
		 * because the entries are structurally intact and in
		 * in hash-value order so the simulation won't get confused
		 * if it has to move them around.
		 */
		if (!no_modify && need_root_dotdot &&
				ino == mp->m_sb.sb_rootino)  {
			assert(ip->i_d.di_format != XFS_DINODE_FMT_LOCAL);

			do_warn("recreating root directory .. entry\n");

			tp = xfs_trans_alloc(mp, 0);
			assert(tp != NULL);

#if VERS >= V_653
			nres = XFS_MKDIR_SPACE_RES(mp, 2);
#else
			nres = XFS_MKDIR_SPACE_RES(mp, "..");
#endif
			error = xfs_trans_reserve(tp, nres,
					XFS_MKDIR_LOG_RES(mp),
					0,
					XFS_TRANS_PERM_LOG_RES,
					XFS_MKDIR_LOG_COUNT);

			if (error)
				res_failed(error);

			xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
			xfs_trans_ihold(tp, ip);

			XFS_BMAP_INIT(&flist, &first);

			if (error = XFS_DIR_CREATENAME(mp, tp, ip, "..", 2,
					ip->i_ino, &first, &flist, nres))
				do_error(
"can't make \"..\" entry in root inode %llu, createname error %d\n",
					ino, error);

			xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

			error = xfs_bmap_finish(&tp, &flist, first,
					&committed);
			assert(error == 0);
			xfsr_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES
					|XFS_TRANS_SYNC);

			need_root_dotdot = 0;
		} else if (need_root_dotdot && ino == mp->m_sb.sb_rootino)  {
			do_warn("would recreate root directory .. entry\n");
		}

		/*
		 * delete any illegal entries -- which should only exist
		 * if the directory is a longform directory.  bogus
		 * shortform directory entries were deleted in phase 4.
		 */
		if (!no_modify && num_illegal > 0)  {
			assert(ip->i_d.di_format != XFS_DINODE_FMT_LOCAL);
#if VERS >= V_654
			assert(!XFS_SB_VERSION_HASDIRV2(&mp->m_sb));
#endif

			while (num_illegal > 0 && ip->i_d.di_format !=
					XFS_DINODE_FMT_LOCAL)  {
				prune_lf_dir_entry(mp, ino, ip, &hashval);
				num_illegal--;
			}

			/*
			 * handle case where we've deleted so many
			 * entries that the directory has changed from
			 * a longform to a shortform directory.  have
			 * to allocate a transaction since we're working
			 * with the incore data fork.
			 */
			if (num_illegal > 0)  {
				assert(ip->i_d.di_format ==
					XFS_DINODE_FMT_LOCAL);
				tp = xfs_trans_alloc(mp, 0);
				/*
				 * using the remove reservation is overkill
				 * since at most we'll only need to log the
				 * inode but it's easier than wedging a
				 * new define in ourselves.  10 block fs
				 * space reservation is also overkill but
				 * what the heck...
				 */
				nres = XFS_REMOVE_SPACE_RES(mp);
				error = xfs_trans_reserve(tp, nres,
						XFS_REMOVE_LOG_RES(mp), 0,
						XFS_TRANS_PERM_LOG_RES,
						XFS_REMOVE_LOG_COUNT);
				if (error)
					res_failed(error);

				xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
				xfs_trans_ihold(tp, ip);

				prune_sf_dir_entry(mp, ino, ip);

				xfs_trans_log_inode(tp, ip,
						XFS_ILOG_CORE | XFS_ILOG_DDATA);
				assert(error == 0);
				xfsr_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES
						|XFS_TRANS_SYNC);
			}
		}

		/*
		 * if we need to create the '.' entry, do so only if
		 * the directory is a longform dir.  it it's been
		 * turned into a shortform dir, then the inode is ok
		 * since shortform dirs have no '.' entry and the inode
		 * has already been committed by prune_lf_dir_entry().
		 */
		if (need_dot)  {
			/*
			 * bump up our link count but don't
			 * bump up the inode link count.  chances
			 * are good that even though we lost '.'
			 * the inode link counts reflect '.' so
			 * leave the inode link count alone and if
			 * it turns out to be wrong, we'll catch
			 * that in phase 7.
			 */
			add_inode_ref(irec, ino_offset);

			if (no_modify)  {
				do_warn(
	"would create missing \".\" entry in dir ino %llu\n",
					ino);
			} else if (ip->i_d.di_format != XFS_DINODE_FMT_LOCAL)  {
				/*
				 * need to create . entry in longform dir.
				 */
				do_warn(
	"creating missing \".\" entry in dir ino %llu\n",
					ino);

				tp = xfs_trans_alloc(mp, 0);
				assert(tp != NULL);

#if VERS >= V_653
				nres = XFS_MKDIR_SPACE_RES(mp, 1);
#else
				nres = XFS_MKDIR_SPACE_RES(mp, ".");
#endif
				error = xfs_trans_reserve(tp, nres,
						XFS_MKDIR_LOG_RES(mp),
						0,
						XFS_TRANS_PERM_LOG_RES,
						XFS_MKDIR_LOG_COUNT);

				if (error)
					res_failed(error);

				xfs_trans_ijoin(tp, ip, XFS_ILOCK_EXCL);
				xfs_trans_ihold(tp, ip);

				XFS_BMAP_INIT(&flist, &first);

				if (error = XFS_DIR_CREATENAME(mp, tp, ip, ".",
						1, ip->i_ino, &first, &flist,
						nres))
					do_error(
	"can't make \".\" entry in dir ino %llu, createname error %d\n",
						ino, error);

				xfs_trans_log_inode(tp, ip, XFS_ILOG_CORE);

				error = xfs_bmap_finish(&tp, &flist, first,
						&committed);
				assert(error == 0);
				xfsr_trans_commit(tp, XFS_TRANS_RELEASE_LOG_RES
						|XFS_TRANS_SYNC);
			}
		}

		xfs_iput(ip, XFS_ILOCK_EXCL);
	}

	return;
}

/*
 * mark realtime bitmap and summary inodes as reached.
 * quota inode will be marked here as well
 */
void
mark_standalone_inodes(xfs_mount_t *mp)
{
	ino_tree_node_t		*irec;
	int			offset;

	irec = find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rbmino),
			XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rbmino));

	assert(irec != NULL);

	offset = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rbmino) -
			irec->ino_startnum;

	add_inode_reached(irec, offset);

	irec = find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rsumino),
			XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rsumino));

	offset = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rsumino) - 
			irec->ino_startnum;

	assert(irec != NULL);

	add_inode_reached(irec, offset);

	if (fs_quotas)  {
		if (mp->m_sb.sb_uquotino
				&& mp->m_sb.sb_uquotino != NULLFSINO)  {
			irec = find_inode_rec(XFS_INO_TO_AGNO(mp,
						mp->m_sb.sb_uquotino),
				XFS_INO_TO_AGINO(mp, mp->m_sb.sb_uquotino));
			offset = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_uquotino)
					- irec->ino_startnum;
			add_inode_reached(irec, offset);
		}
		if (mp->m_sb.sb_pquotino
				&& mp->m_sb.sb_pquotino != NULLFSINO)  {
			irec = find_inode_rec(XFS_INO_TO_AGNO(mp,
						mp->m_sb.sb_pquotino),
				XFS_INO_TO_AGINO(mp, mp->m_sb.sb_pquotino));
			offset = XFS_INO_TO_AGINO(mp, mp->m_sb.sb_pquotino)
					- irec->ino_startnum;
			add_inode_reached(irec, offset);
		}
	}

	return;
}

void
phase6(xfs_mount_t *mp)
{
	xfs_ino_t		ino;
	ino_tree_node_t		*irec;
	dir_stack_t		stack;
	int			i;
	int			j;

	bzero(&zerocr, sizeof(cred_t));

	do_log("Phase 6 - check inode connectivity...\n");

	if (!no_modify)
		teardown_bmap_finish(mp);
	else
		teardown_bmap(mp);

	incore_ext_teardown(mp);

	add_ino_backptrs(mp);

	/*
	 * verify existence of root directory - if we have to
	 * make one, it's ok for the incore data structs not to
	 * know about it since everything about it (and the other
	 * inodes in its chunk if a new chunk was created) are ok
	 */
	if (need_root_inode)  {
		if (!no_modify)  {
			do_warn("reinitializing root directory\n");
			mk_root_dir(mp);
			need_root_inode = 0;
			need_root_dotdot = 0;
		} else  {
			do_warn("would reinitialize root directory\n");
		}
	}

	if (need_rbmino)  {
		if (!no_modify)  {
			do_warn("reinitializing realtime bitmap inode\n");
			mk_rbmino(mp);
			need_rbmino = 0;
		} else  {
			do_warn("would reinitialize realtime bitmap inode\n");
		}
	}

	if (need_rsumino)  {
		if (!no_modify)  {
			do_warn("reinitializing realtime summary inode\n");
			mk_rsumino(mp);
			need_rsumino = 0;
		} else  {
			do_warn("would reinitialize realtime summary inode\n");
		}
	}

	if (!no_modify)  {
		do_log(
	"        - resetting contents of realtime bitmap and summary inodes\n");
		if (fill_rbmino(mp))  {
			do_warn(
			"Warning:  realtime bitmap may be inconsistent\n");
		}

		if (fill_rsumino(mp))  {
			do_warn(
			"Warning:  realtime bitmap may be inconsistent\n");
		}
	}

	/*
	 * make orphanage (it's guaranteed to not exist now)
	 */
	if (!no_modify)  {
		do_log("        - ensuring existence of %s directory\n",
			ORPHANAGE);
		orphanage_ino = mk_orphanage(mp);
	}

	dir_stack_init(&stack);

	mark_standalone_inodes(mp);

	/*
	 * push root dir on stack, then go
	 */
	if (!need_root_inode)  {
		do_log("        - traversing filesystem starting at / ... \n");

		push_dir(&stack, mp->m_sb.sb_rootino);
		process_dirstack(mp, &stack);

		do_log("        - traversal finished ... \n");
	} else  {
		assert(no_modify != 0);

		do_log(
"        - root inode lost, cannot make new one in no modify mode ... \n");
		do_log(
"        - skipping filesystem traversal from / ... \n");
	}

	do_log("        - traversing all unattached subtrees ... \n");

	irec = find_inode_rec(XFS_INO_TO_AGNO(mp, mp->m_sb.sb_rootino),
				XFS_INO_TO_AGINO(mp, mp->m_sb.sb_rootino));

	/*
	 * we always have a root inode, even if it's free...
	 * if the root is free, forget it, lost+found is already gone
	 */
	if (is_inode_free(irec, 0) || !inode_isadir(irec, 0))  {
		need_root_inode = 1;
	}

	/*
	 * then process all unreached inodes
	 * by walking incore inode tree
	 *
	 *	get next unreached directory inode # from
	 *		incore list
	 *	push inode on dir stack
	 *	call process_dirstack
	 */
	for (i = 0; i < glob_agcount; i++)  {
		irec = findfirst_inode_rec(i);

		if (irec == NULL)
			continue;

		while (irec != NULL)  {
			for (j = 0; j < XFS_INODES_PER_CHUNK; j++)  {
				if (!is_inode_confirmed(irec, j))
					continue;
				/*
				 * skip directories that have already been
				 * processed, even if they haven't been
				 * reached.  If they are reachable, we'll
				 * pick them up when we process their parent.
				 */
				ino = XFS_AGINO_TO_INO(mp, i,
						j + irec->ino_startnum);
				if (inode_isadir(irec, j) &&
						!is_inode_refchecked(ino,
							irec, j)) {
					push_dir(&stack, ino);
					process_dirstack(mp, &stack);
				}
			}
			irec = next_ino_rec(irec);
		}
	}

	do_log("        - traversals finished ... \n");
	do_log("        - moving disconnected inodes to lost+found ... \n");

	/*
	 * move all disconnected inodes to the orphanage
	 */
	for (i = 0; i < glob_agcount; i++)  {
		irec = findfirst_inode_rec(i);

		if (irec == NULL)
			continue;

		while (irec != NULL)  {
			for (j = 0; j < XFS_INODES_PER_CHUNK; j++)  {
				assert(is_inode_confirmed(irec, j));
				if (is_inode_free(irec, j))
					continue;
				if (!is_inode_reached(irec, j)) {
					assert(inode_isadir(irec, j) ||
						num_inode_references(irec, j)
						== 0);
					ino = XFS_AGINO_TO_INO(mp, i,
						j + irec->ino_startnum);
					if (inode_isadir(irec, j))
						do_warn(
						"disconnected dir inode %llu, ",
							ino);
					else
						do_warn(
						"disconnected inode %llu, ",
							ino);
					if (!no_modify)  {
						do_warn("moving to %s\n",
							ORPHANAGE);
						mv_orphanage(mp, orphanage_ino,
							ino,
							inode_isadir(irec, j));
					} else  {
						do_warn("would move to %s\n",
							ORPHANAGE);
					}
					/*
					 * for read-only case, even though
					 * the inode isn't really reachable,
					 * set the flag (and bump our link
					 * count) anyway to fool phase 7
					 */
					add_inode_reached(irec, j);
				}
			}
			irec = next_ino_rec(irec);
		}
	}

	return;
}
