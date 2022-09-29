#ident "$Revision: 1.37 $"

#include "versions.h"
#include <sys/param.h>
#include <sys/time.h>
#include <sys/uuid.h>
#include <assert.h>
#if VERS <= V_63
#include <math.h>
#endif
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_ag.h>
#if VERS >= V_62
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2_sf.h>
#endif
#else
#include <sys/fs/xfs_dir.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_bit.h>
#if VERS >= V_62
#include <sys/fs/xfs_da_btree.h>
#include <sys/fs/xfs_dir_leaf.h>
#include <sys/fs/xfs_quota.h>
#include <sys/fs/xfs_dqblk.h>
#endif
#include <sys/fs/xfs_rtalloc.h>
#include <sys/fs/xfs_ialloc.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir.h>
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_dir2_data.h>
#include <sys/fs/xfs_dir2_leaf.h>
#include <sys/fs/xfs_dir2_block.h>
#include <sys/fs/xfs_dir2_node.h>
#endif
#include "sim.h"
#include "bmap.h"
#include "check.h"
#include "command.h"
#include "data.h"
#include "io.h"
#include "output.h"
#include "type.h"
#include "mount.h"
#include "malloc.h"

typedef enum {
	DBM_UNKNOWN,	DBM_AGF,	DBM_AGFL,	DBM_AGI,
	DBM_ATTR,	DBM_BTBMAPA,	DBM_BTBMAPD,	DBM_BTBNO,
	DBM_BTCNT,	DBM_BTINO,	DBM_DATA,	DBM_DIR,
	DBM_FREE1,	DBM_FREE2,	DBM_FREELIST,	DBM_INODE,
	DBM_LOG,	DBM_MISSING,	DBM_QUOTA,	DBM_RTBITMAP,
	DBM_RTDATA,	DBM_RTFREE,	DBM_RTSUM,	DBM_SB,
	DBM_SYMLINK,
	DBM_NDBM
} dbm_t;

typedef struct inodata {
	struct inodata	*next;
	nlink_t		link_set;
	nlink_t		link_add;
	char		isdir;
	char		security;
	char		ilist;
	xfs_ino_t	ino;
	struct inodata	*parent;
	char		*name;
} inodata_t;
#define	MIN_INODATA_HASH_SIZE	256
#define	MAX_INODATA_HASH_SIZE	65536
#define	INODATA_AVG_HASH_LENGTH	8

#if VERS >= V_62
typedef struct qinfo {
	xfs_qcnt_t	bc;
	xfs_qcnt_t	ic;
	xfs_qcnt_t	rc;
} qinfo_t;

#define	QDATA_HASH_SIZE	256
typedef	struct qdata {
	struct qdata	*next;
	xfs_dqid_t	id;
	qinfo_t		count;
	qinfo_t		dq;
} qdata_t;
#endif

typedef struct blkent {
	xfs_fileoff_t	startoff;
	int		nblks;
	xfs_fsblock_t	blks[1];
} blkent_t;
#define	BLKENT_SIZE(n)	\
	(offsetof(blkent_t, blks) + (sizeof(xfs_fsblock_t) * (n)))

typedef	struct blkmap {
	int		naents;
	int		nents;
	blkent_t	*ents[1];
} blkmap_t;
#define	BLKMAP_SIZE(n)	\
	(offsetof(blkmap_t, ents) + (sizeof(blkent_t *) * (n)))

#if VERS >= V_654
typedef struct freetab {
	int			naents;
	int			nents;
	xfs_dir2_data_off_t	ents[1];
} freetab_t;
#define	FREETAB_SIZE(n)	\
	(offsetof(freetab_t, ents) + (sizeof(xfs_dir2_data_off_t) * (n)))

typedef struct dirhash {
	struct dirhash		*next;
	xfs_dir2_leaf_entry_t	entry;
	int			seen;
} dirhash_t;
#define	DIR_HASH_SIZE	1024
#define	DIR_HASH_FUNC(h,a)	(((h) ^ (a)) % DIR_HASH_SIZE)
#endif

static xfs_extlen_t	agffreeblks;
static xfs_extlen_t	agflongest;
static xfs_agino_t	agicount;
static xfs_agino_t	agifreecount;
static xfs_fsblock_t	*blist;
static int		blist_size;
static char		**dbmap;	/* really dbm_t:8 */
#if VERS >= V_654
static dirhash_t	**dirhash;
#endif
static int		error;
static __uint64_t	fdblocks;
static __uint64_t	frextents;
static __uint64_t	icount;
static __uint64_t	ifree;
static inodata_t	***inodata;
static int		inodata_hash_size;
static inodata_t	***inomap;
static int		nflag;
static int		pflag;
#if VERS >= V_62
static qdata_t		**qpdata;
static int		qpdo;
static qdata_t		**qudata;
static int		qudo;
static unsigned		sbversion;
#endif
static int		sbver_err;
static int		serious_error;
static int		sflag;
static xfs_suminfo_t	*sumcompute;
static xfs_suminfo_t	*sumfile;
static const char	*typename[] = {
	"unknown",
	"agf",
	"agfl",
	"agi",
	"attr",
	"btbmapa",
	"btbmapd",
	"btbno",
	"btcnt",
	"btino",
	"data",
	"dir",
	"free1",
	"free2",
	"freelist",
	"inode",
	"log",
	"missing",
	"quota",
	"rtbitmap",
	"rtdata",
	"rtfree",
	"rtsum",
	"sb",
	"symlink",
	NULL
};
static int		verbose;

/*
 * bit map related macros
 */
#define isset(a,i)      ((a)[(i)/XFS_NBWORD] & (1<<((i)%XFS_NBWORD)))

#define	CHECK_BLIST(b)	(blist_size && check_blist(b))
#define	CHECK_BLISTA(a,b)	\
	(blist_size && check_blist(XFS_AGB_TO_FSB(mp, a, b)))

typedef void	(*scan_lbtree_f_t)(xfs_btree_lblock_t	*block,
				   int			level,
				   dbm_t		type,
				   xfs_fsblock_t	bno,
				   inodata_t		*id,
				   xfs_drfsbno_t	*totd,
				   xfs_drfsbno_t	*toti,
				   xfs_extnum_t		*nex,
				   blkmap_t		**blkmapp,
				   int			isroot,
				   typnm_t		btype);

typedef void	(*scan_sbtree_f_t)(xfs_btree_sblock_t	*block,
				   int			level,
				   xfs_agf_t		*agf,
				   xfs_agblock_t	bno,
				   int			isroot);

static void		add_blist(xfs_fsblock_t	bno);
static void		add_ilist(xfs_ino_t ino);
static void		addlink_inode(inodata_t *id);
static void		addname_inode(inodata_t *id, char *name, int namelen);
static void		addparent_inode(inodata_t *id, xfs_ino_t parent);
static void		blkent_append(blkent_t **entp, xfs_fsblock_t b,
				      xfs_extlen_t c);
static blkent_t		*blkent_new(xfs_fileoff_t o, xfs_fsblock_t b,
				    xfs_extlen_t c);
static void		blkent_prepend(blkent_t **entp, xfs_fsblock_t b,
				       xfs_extlen_t c);
static blkmap_t		*blkmap_alloc(xfs_extnum_t);
static void		blkmap_free(blkmap_t *blkmap);
static xfs_fsblock_t	blkmap_get(blkmap_t *blkmap, xfs_fileoff_t o);
#if VERS >= V_654
static int		blkmap_getn(blkmap_t *blkmap, xfs_fileoff_t o, int nb,
				    bmap_ext_t **bmpp);
#endif
static void		blkmap_grow(blkmap_t **blkmapp, blkent_t **entp,
				    blkent_t *newent);
static xfs_fileoff_t	blkmap_next_off(blkmap_t *blkmap, xfs_fileoff_t o,
					int *t);
static void		blkmap_set_blk(blkmap_t **blkmapp, xfs_fileoff_t o,
				       xfs_fsblock_t b);
static void		blkmap_set_ext(blkmap_t **blkmapp, xfs_fileoff_t o,
				       xfs_fsblock_t b, xfs_extlen_t c);
static void		blkmap_shrink(blkmap_t *blkmap, blkent_t **entp);
static int		blockfree_f(int argc, char **argv);
static int		blockget_f(int argc, char **argv);
#ifdef DEBUG
static int		blocktrash_f(int argc, char **argv);
#endif
static int		blockuse_f(int argc, char **argv);
static int		check_blist(xfs_fsblock_t bno);
static void		check_dbmap(xfs_agnumber_t agno, xfs_agblock_t agbno,
				    xfs_extlen_t len, dbm_t type);
static int		check_inomap(xfs_agnumber_t agno, xfs_agblock_t agbno,
				     xfs_extlen_t len, xfs_ino_t c_ino);
static void		check_linkcounts(xfs_agnumber_t agno);
static int		check_range(xfs_agnumber_t agno, xfs_agblock_t agbno,
				    xfs_extlen_t len);
static void		check_rdbmap(xfs_drfsbno_t bno, xfs_extlen_t len,
				     dbm_t type);
static int		check_rinomap(xfs_drfsbno_t bno, xfs_extlen_t len,
				      xfs_ino_t c_ino);
static void		check_rootdir(void);
static int		check_rrange(xfs_drfsbno_t bno, xfs_extlen_t len);
static void		check_set_dbmap(xfs_agnumber_t agno,
					xfs_agblock_t agbno, xfs_extlen_t len,
					dbm_t type1, dbm_t type2,
					xfs_agnumber_t c_agno,
					xfs_agblock_t c_agbno);
static void		check_set_rdbmap(xfs_drfsbno_t bno, xfs_extlen_t len,
					 dbm_t type1, dbm_t type2);
static void		check_summary(void);
static void		checknot_dbmap(xfs_agnumber_t agno, xfs_agblock_t agbno,
				       xfs_extlen_t len, int typemask);
static void		checknot_rdbmap(xfs_drfsbno_t bno, xfs_extlen_t len,
					int typemask);
#if VERS >= V_654
static void		dir_hash_add(xfs_dahash_t hash,
				     xfs_dir2_dataptr_t addr);
static void		dir_hash_check(inodata_t *id, int v);
static void		dir_hash_done(void);
static void		dir_hash_init(void);
static int		dir_hash_see(xfs_dahash_t hash,
				     xfs_dir2_dataptr_t addr);
#endif
static inodata_t	*find_inode(xfs_ino_t ino, int add);
static void		free_inodata(xfs_agnumber_t agno);
static int		init(int argc, char **argv);
static char		*inode_name(xfs_ino_t ino, inodata_t **ipp);
static int		ncheck_f(int argc, char **argv);
static char		*prepend_path(char *oldpath, char *parent);
#if VERS >= V_654
static xfs_ino_t	process_block_dir_v2(blkmap_t *blkmap, int *dot,
					     int *dotdot, inodata_t *id);
#endif
static void		process_bmbt_reclist(xfs_bmbt_rec_32_t *rp, int numrecs,
					     dbm_t type, inodata_t *id,
					     xfs_drfsbno_t *tot,
					     blkmap_t **blkmapp);
static void		process_btinode(inodata_t *id, xfs_dinode_t *dip,
					dbm_t type, xfs_drfsbno_t *totd,
					xfs_drfsbno_t *toti, xfs_extnum_t *nex,
					blkmap_t **blkmapp, int whichfork);
#if VERS >= V_654
static xfs_ino_t	process_data_dir_v2(int *dot, int *dotdot,
					    inodata_t *id, int v,
					    xfs_dablk_t dabno,
					    freetab_t **freetabp);
static xfs_dir2_data_free_t
			*process_data_dir_v2_freefind(xfs_dir2_data_t *data,
					           xfs_dir2_data_unused_t *dup);
#endif
static void		process_dir(xfs_dinode_t *dip, blkmap_t *blkmap,
				    inodata_t *id);
static int		process_dir_v1(xfs_dinode_t *dip, blkmap_t *blkmap,
				       int *dot, int *dotdot, inodata_t *id,
				       xfs_ino_t *parent);
#if VERS >= V_654
static int		process_dir_v2(xfs_dinode_t *dip, blkmap_t *blkmap,
				       int *dot, int *dotdot, inodata_t *id,
				       xfs_ino_t *parent);
#endif
static void		process_exinode(inodata_t *id, xfs_dinode_t *dip,
					dbm_t type, xfs_drfsbno_t *totd,
					xfs_drfsbno_t *toti, xfs_extnum_t *nex,
					blkmap_t **blkmapp, int whichfork);
static void		process_inode(xfs_agf_t *agf, xfs_agino_t agino,
				      xfs_dinode_t *dip, int isfree);
static void		process_lclinode(inodata_t *id, xfs_dinode_t *dip,
					 dbm_t type, xfs_drfsbno_t *totd,
					 xfs_drfsbno_t *toti, xfs_extnum_t *nex,
					 blkmap_t **blkmapp, int whichfork);
static xfs_ino_t	process_leaf_dir_v1(blkmap_t *blkmap, int *dot,
					    int *dotdot, inodata_t *id);
static xfs_ino_t	process_leaf_dir_v1_int(int *dot, int *dotdot,
						inodata_t *id);
#if VERS >= V_654
static xfs_ino_t	process_leaf_node_dir_v2(blkmap_t *blkmap, int *dot,
						 int *dotdot, inodata_t *id,
						 xfs_fsize_t dirsize);
static void		process_leaf_node_dir_v2_free(inodata_t *id, int v,
						      xfs_dablk_t dbno,
						      freetab_t *freetab);
static void		process_leaf_node_dir_v2_int(inodata_t *id, int v,
						     xfs_dablk_t dbno,
						     freetab_t *freetab);
#endif
static xfs_ino_t	process_node_dir_v1(blkmap_t *blkmap, int *dot,
					    int *dotdot, inodata_t *id);
#if VERS >= V_62
static void		process_quota(int isproj, inodata_t *id,
				      blkmap_t *blkmap);
#endif
static void		process_rtbitmap(blkmap_t *blkmap);
static void		process_rtsummary(blkmap_t *blkmap);
#if VERS >= V_654
static xfs_ino_t	process_sf_dir_v2(xfs_dinode_t *dip, int *dot,
					  int *dotdot, inodata_t *id);
#endif
static xfs_ino_t	process_shortform_dir_v1(xfs_dinode_t *dip, int *dot,
						 int *dotdot, inodata_t *id);
#if VERS >= V_62
static void		quota_add(xfs_dqid_t projid, xfs_dqid_t userid,
				  int dq, xfs_qcnt_t bc, xfs_qcnt_t ic,
				  xfs_qcnt_t rc);
static void		quota_add1(qdata_t **qt, xfs_dqid_t id, int dq,
				   xfs_qcnt_t bc, xfs_qcnt_t ic,
				   xfs_qcnt_t rc);
static void		quota_check(char *s, qdata_t **qt);
static void		quota_init(void);
#endif
static void		scan_ag(xfs_agnumber_t agno);
static void		scan_freelist(xfs_agf_t *agf);
static void		scan_lbtree(xfs_fsblock_t root, int nlevels,
				    scan_lbtree_f_t func, dbm_t type,
				    inodata_t *id, xfs_drfsbno_t *totd,
				    xfs_drfsbno_t *toti, xfs_extnum_t *nex,
				    blkmap_t **blkmapp, int isroot,
				    typnm_t btype);
static void		scan_sbtree(xfs_agf_t *agf, xfs_agblock_t root,
				    int nlevels, int isroot,
				    scan_sbtree_f_t func, typnm_t btype);
static void		scanfunc_bmap(xfs_btree_lblock_t *ablock, int level,
				      dbm_t type, xfs_fsblock_t bno,
				      inodata_t *id, xfs_drfsbno_t *totd,
				      xfs_drfsbno_t *toti, xfs_extnum_t *nex,
				      blkmap_t **blkmapp, int isroot,
				      typnm_t btype);
static void		scanfunc_bno(xfs_btree_sblock_t *ablock, int level,
				     xfs_agf_t *agf, xfs_agblock_t bno,
				     int isroot);
static void		scanfunc_cnt(xfs_btree_sblock_t *ablock, int level,
				     xfs_agf_t *agf, xfs_agblock_t bno,
				     int isroot);
static void		scanfunc_ino(xfs_btree_sblock_t *ablock, int level,
				     xfs_agf_t *agf, xfs_agblock_t bno,
				     int isroot);
static void		set_dbmap(xfs_agnumber_t agno, xfs_agblock_t agbno,
				  xfs_extlen_t len, dbm_t type,
				  xfs_agnumber_t c_agno, xfs_agblock_t c_agbno);
static void		set_inomap(xfs_agnumber_t agno, xfs_agblock_t agbno,
				   xfs_extlen_t len, inodata_t *id);
static void		set_rdbmap(xfs_drfsbno_t bno, xfs_extlen_t len,
				   dbm_t type);
static void		set_rinomap(xfs_drfsbno_t bno, xfs_extlen_t len,
				    inodata_t *id);
static void		setlink_inode(inodata_t *id, nlink_t nlink, int isdir,
				       int security);

static const cmdinfo_t	blockfree_cmd = 
	{ "blockfree", NULL, blockfree_f, 0, 0, 0,
	  NULL, "free block usage information", NULL };
static const cmdinfo_t	blockget_cmd = 
	{ "blockget", "check", blockget_f, 0, -1, 0,
	  "[-s|-v] [-n] [-b bno]... [-i ino] ...",
	  "get block usage and check consistency", NULL };
#ifdef DEBUG
static const cmdinfo_t	blocktrash_cmd = 
	{ "blocktrash", NULL, blocktrash_f, 0, -1, 0,
	  "[-n count] [-x minlen] [-y maxlen] [-s seed] [-0123] [-t type] ...",
	  "trash randomly selected block(s)", NULL };
#endif
static const cmdinfo_t	blockuse_cmd = 
	{ "blockuse", NULL, blockuse_f, 0, 3, 0,
	  "[-n] [-c blockcount]",
	  "print usage for current block(s)", NULL };
static const cmdinfo_t	ncheck_cmd = 
	{ "ncheck", NULL, ncheck_f, 0, -1, 0,
	  "[-s] [-i ino] ...",
	  "print inode-name pairs", NULL };


static void
add_blist(
	xfs_fsblock_t	bno)
{
	blist_size++;
	blist = xrealloc(blist, blist_size * sizeof(bno));
	blist[blist_size - 1] = bno;
}

static void
add_ilist(
	xfs_ino_t	ino)
{
	inodata_t	*id;

	id = find_inode(ino, 1);
	if (id == NULL) {
		dbprintf("-i %lld bad inode number\n", ino);
		return;
	}
	id->ilist = 1;
}

static void
addlink_inode(
	inodata_t	*id)
{
	id->link_add++;
	if (verbose || id->ilist)
		dbprintf("inode %lld add link, now %u\n", id->ino,
			id->link_add);
}

static void
addname_inode(
	inodata_t	*id,
	char		*name,
	int		namelen)
{
	if (!nflag || id->name)
		return;
	id->name = xmalloc(namelen + 1);
	memcpy(id->name, name, namelen);
	id->name[namelen] = '\0';
}

static void 
addparent_inode(
	inodata_t	*id,
	xfs_ino_t	parent)
{
	inodata_t	*pid;

	pid = find_inode(parent, 1);
	id->parent = pid;
	if (verbose || id->ilist || (pid && pid->ilist))
		dbprintf("inode %lld parent %lld\n", id->ino, parent);
}

static void
blkent_append(
	blkent_t	**entp,
	xfs_fsblock_t	b,
	xfs_extlen_t	c)
{
	blkent_t	*ent;
	int		i;

	ent = *entp;
	*entp = ent = xrealloc(ent, BLKENT_SIZE(c + ent->nblks));
	for (i = 0; i < c; i++)
		ent->blks[ent->nblks + i] = b + i;
	ent->nblks += c;
}

static blkent_t *
blkent_new(
	xfs_fileoff_t	o,
	xfs_fsblock_t	b,
	xfs_extlen_t	c)
{
	blkent_t	*ent;
	int		i;

	ent = xmalloc(BLKENT_SIZE(c));
	ent->nblks = c;
	ent->startoff = o;
	for (i = 0; i < c; i++)
		ent->blks[i] = b + i;
	return ent;
}

static void
blkent_prepend(
	blkent_t	**entp,
	xfs_fsblock_t	b,
	xfs_extlen_t	c)
{
	int		i;
	blkent_t	*newent;
	blkent_t	*oldent;

	oldent = *entp;
	newent = xmalloc(BLKENT_SIZE(oldent->nblks + c));
	newent->nblks = oldent->nblks + c;
	newent->startoff = oldent->startoff - c;
	for (i = 0; i < c; i++)
		newent->blks[i] = b + c;
	for (; i < oldent->nblks + c; i++)
		newent->blks[i] = oldent->blks[i - c];
	xfree(oldent);
	*entp = newent;
}

static blkmap_t *
blkmap_alloc(
	xfs_extnum_t	nex)
{
	blkmap_t	*blkmap;

	if (nex < 1)
		nex = 1;
	blkmap = xmalloc(BLKMAP_SIZE(nex));
	blkmap->naents = nex;
	blkmap->nents = 0;
	return blkmap;
}

static void
blkmap_free(
	blkmap_t	*blkmap)
{
	blkent_t	**entp;
	xfs_extnum_t	i;

	for (i = 0, entp = blkmap->ents; i < blkmap->nents; i++, entp++)
		xfree(*entp);
	xfree(blkmap);
}

static xfs_fsblock_t
blkmap_get(
	blkmap_t	*blkmap,
	xfs_fileoff_t	o)
{
	blkent_t	*ent;
	blkent_t	**entp;
	int		i;

	for (i = 0, entp = blkmap->ents; i < blkmap->nents; i++, entp++) {
		ent = *entp;
		if (o >= ent->startoff && o < ent->startoff + ent->nblks)
			return ent->blks[o - ent->startoff];
	}
	return NULLFSBLOCK;
}

#if VERS >= V_654
static int
blkmap_getn(
	blkmap_t	*blkmap,
	xfs_fileoff_t	o,
	int		nb,
	bmap_ext_t	**bmpp)
{
	bmap_ext_t	*bmp;
	blkent_t	*ent;
	xfs_fileoff_t	ento;
	blkent_t	**entp;
	int		i;
	int		nex;

	for (i = nex = 0, bmp = NULL, entp = blkmap->ents;
	     i < blkmap->nents;
	     i++, entp++) {
		ent = *entp;
		if (ent->startoff >= o + nb)
			break;
		if (ent->startoff + ent->nblks <= o)
			continue;
		for (ento = ent->startoff;
		     ento < ent->startoff + ent->nblks && ento < o + nb;
		     ento++) {
			if (ento < o)
				continue;
			if (bmp &&
			    bmp[nex - 1].startoff + bmp[nex - 1].blockcount ==
				    ento &&
			    bmp[nex - 1].startblock + bmp[nex - 1].blockcount ==
				    ent->blks[ento - ent->startoff])
				bmp[nex - 1].blockcount++;
			else {
				bmp = realloc(bmp, ++nex * sizeof(*bmp));
				bmp[nex - 1].startoff = ento;
				bmp[nex - 1].startblock =
					ent->blks[ento - ent->startoff];
				bmp[nex - 1].blockcount = 1;
				bmp[nex - 1].flag = 0;
			}
		}
	}
	*bmpp = bmp;
	return nex;
}
#endif

static void
blkmap_grow(
	blkmap_t	**blkmapp,
	blkent_t	**entp,
	blkent_t	*newent)
{
	blkmap_t	*blkmap;
	int		i;
	int		idx;

	blkmap = *blkmapp;
	idx = (int)(entp - blkmap->ents);
	if (blkmap->naents == blkmap->nents) {
		blkmap = xrealloc(blkmap, BLKMAP_SIZE(blkmap->nents + 1));
		*blkmapp = blkmap;
		blkmap->naents++;
	}
	for (i = blkmap->nents; i > idx; i--)
		blkmap->ents[i] = blkmap->ents[i - 1];
	blkmap->ents[idx] = newent;
	blkmap->nents++;
}

#if VERS >= V_654
static xfs_fileoff_t
blkmap_last_off(
	blkmap_t	*blkmap)
{
	blkent_t	*ent;

	if (!blkmap->nents)
		return NULLFILEOFF;
	ent = blkmap->ents[blkmap->nents - 1];
	return ent->startoff + ent->nblks;
}
#endif

static xfs_fileoff_t
blkmap_next_off(
	blkmap_t	*blkmap,
	xfs_fileoff_t	o,
	int		*t)
{
	blkent_t	*ent;
	blkent_t	**entp;

	if (!blkmap->nents)
		return NULLFILEOFF;
	if (o == NULLFILEOFF) {
		*t = 0;
		ent = blkmap->ents[0];
		return ent->startoff;
	}
	entp = &blkmap->ents[*t];
	ent = *entp;
	if (o < ent->startoff + ent->nblks - 1)
		return o + 1;
	entp++;
	if (entp >= &blkmap->ents[blkmap->nents])
		return NULLFILEOFF;
	(*t)++;
	ent = *entp;
	return ent->startoff;
}

static void
blkmap_set_blk(
	blkmap_t	**blkmapp,
	xfs_fileoff_t	o,
	xfs_fsblock_t	b)
{
	blkmap_t	*blkmap;
	blkent_t	*ent;
	blkent_t	**entp;
	blkent_t	*nextent;

	blkmap = *blkmapp;
	for (entp = blkmap->ents; entp < &blkmap->ents[blkmap->nents]; entp++) {
		ent = *entp;
		if (o < ent->startoff - 1) {
			ent = blkent_new(o, b, 1);
			blkmap_grow(blkmapp, entp, ent);
			return;
		}
		if (o == ent->startoff - 1) {
			blkent_prepend(entp, b, 1);
			return;
		}
		if (o >= ent->startoff && o < ent->startoff + ent->nblks) {
			ent->blks[o - ent->startoff] = b;
			return;
		}
		if (o > ent->startoff + ent->nblks)
			continue;
		blkent_append(entp, b, 1);
		if (entp == &blkmap->ents[blkmap->nents - 1])
			return;
		ent = *entp;
		nextent = entp[1];
		if (ent->startoff + ent->nblks < nextent->startoff)
			return;
		blkent_append(entp, nextent->blks[0], nextent->nblks);
		blkmap_shrink(blkmap, &entp[1]);
		return;
	}
	ent = blkent_new(o, b, 1);
	blkmap_grow(blkmapp, entp, ent);
}

static void
blkmap_set_ext(
	blkmap_t	**blkmapp,
	xfs_fileoff_t	o,
	xfs_fsblock_t	b,
	xfs_extlen_t	c)
{
	blkmap_t	*blkmap;
	blkent_t	*ent;
	blkent_t	**entp;
	xfs_extnum_t	i;

	blkmap = *blkmapp;
	if (!blkmap->nents) {
		blkmap->ents[0] = blkent_new(o, b, c);
		blkmap->nents = 1;
		return;
	}
	entp = &blkmap->ents[blkmap->nents - 1];
	ent = *entp;
	if (ent->startoff + ent->nblks == o) {
		blkent_append(entp, b, c);
		return;
	}
	if (ent->startoff + ent->nblks < o) {
		ent = blkent_new(o, b, c);
		blkmap_grow(blkmapp, &blkmap->ents[blkmap->nents], ent);
		return;
	}
	for (i = 0; i < c; i++)
		blkmap_set_blk(blkmapp, o + i, b + i);
}

static void
blkmap_shrink(
	blkmap_t	*blkmap,
	blkent_t	**entp)
{
	int		i;
	int		idx;

	xfree(*entp);
	idx = (int)(entp - blkmap->ents);
	for (i = idx + 1; i < blkmap->nents; i++)
		blkmap->ents[i] = blkmap->ents[i - 1];
	blkmap->nents--;
}

/* ARGSUSED */
static int
blockfree_f(
	int		argc,
	char		**argv)
{
	xfs_agnumber_t	c;
	int		rt;

	if (!dbmap) {
		dbprintf("block usage information not allocated\n");
		return 0;
	}
	rt = mp->m_sb.sb_rextents != 0;
	for (c = 0; c < mp->m_sb.sb_agcount; c++) {
		xfree(dbmap[c]);
		xfree(inomap[c]);
		free_inodata(c);
	}
	if (rt) {
		xfree(dbmap[c]);
		xfree(inomap[c]);
		xfree(sumcompute);
		xfree(sumfile);
		sumcompute = sumfile = NULL;
	}
	xfree(dbmap);
	xfree(inomap);
	xfree(inodata);
	dbmap = NULL;
	inomap = NULL;
	inodata = NULL;
	return 0;
}

/*
 * Check consistency of xfs filesystem contents.
 */
static int
blockget_f(
	int		argc,
	char		**argv)
{
	xfs_agnumber_t	agno;
	int		oldprefix;
	int		sbyell;

	if (dbmap) {
		dbprintf("already have block usage information\n");
		return 0;
	}
	if (!init(argc, argv))
		return 0;
	oldprefix = dbprefix;
	dbprefix |= pflag;
	for (agno = 0, sbyell = 0; agno < mp->m_sb.sb_agcount; agno++) {
		scan_ag(agno);
		if (sbver_err > 4 && !sbyell && sbver_err >= agno) {
			sbyell = 1;
			dbprintf("WARNING: this may be a newer XFS "
				 "filesystem.\n");
		}
	}
	if (blist_size) {
		xfree(blist);
		blist = NULL;
		blist_size = 0;
	}
	if (serious_error) {
		exitcode = 2;
		dbprefix = oldprefix;
		return 0;
	}
	check_rootdir();
	for (agno = 0; agno < mp->m_sb.sb_agcount; agno++) {
		/*
		 * Check that there are no blocks either
		 * a) unaccounted for or 
		 * b) bno-free but not cnt-free
		 */
		checknot_dbmap(agno, 0, mp->m_sb.sb_agblocks,
			(1 << DBM_UNKNOWN) | (1 << DBM_FREE1));
		check_linkcounts(agno);
	}
	if (mp->m_sb.sb_rblocks) {
		checknot_rdbmap(0,
			(xfs_extlen_t)(mp->m_sb.sb_rextents *
				       mp->m_sb.sb_rextsize),
			1 << DBM_UNKNOWN);
		check_summary();
	}
	if (mp->m_sb.sb_icount != icount) {
		if (!sflag)
			dbprintf("sb_icount %lld, counted %lld\n",
				mp->m_sb.sb_icount, icount);
		error++;
	}
	if (mp->m_sb.sb_ifree != ifree) {
		if (!sflag)
			dbprintf("sb_ifree %lld, counted %lld\n",
				mp->m_sb.sb_ifree, ifree);
		error++;
	}
	if (mp->m_sb.sb_fdblocks != fdblocks) {
		if (!sflag)
			dbprintf("sb_fdblocks %lld, counted %lld\n",
				mp->m_sb.sb_fdblocks, fdblocks);
		error++;
	}
	if (mp->m_sb.sb_frextents != frextents) {
		if (!sflag)
			dbprintf("sb_frextents %lld, counted %lld\n",
				mp->m_sb.sb_frextents, frextents);
		error++;
	}
#if VERS >= V_62
	if ((sbversion & XFS_SB_VERSION_ATTRBIT) &&
	    !XFS_SB_VERSION_HASATTR(&mp->m_sb)) {
		if (!sflag)
			dbprintf("sb versionnum missing attr bit %x\n",
				XFS_SB_VERSION_ATTRBIT);
		error++;
	}
	if ((sbversion & XFS_SB_VERSION_NLINKBIT) &&
	    !XFS_SB_VERSION_HASNLINK(&mp->m_sb)) {
		if (!sflag)
			dbprintf("sb versionnum missing nlink bit %x\n",
				XFS_SB_VERSION_NLINKBIT);
		error++;
	}
	if ((sbversion & XFS_SB_VERSION_QUOTABIT) &&
	    !XFS_SB_VERSION_HASQUOTA(&mp->m_sb)) {
		if (!sflag)
			dbprintf("sb versionnum missing quota bit %x\n",
				XFS_SB_VERSION_QUOTABIT);
		error++;
	}
	if (!(sbversion & XFS_SB_VERSION_ALIGNBIT) &&
	    XFS_SB_VERSION_HASALIGN(&mp->m_sb)) {
		if (!sflag)
			dbprintf("sb versionnum extra align bit %x\n",
				XFS_SB_VERSION_ALIGNBIT);
		error++;
	}
	if (qudo)
		quota_check("user", qudata);
	if (qpdo)
		quota_check("project", qpdata);
#endif
	if (sbver_err > mp->m_sb.sb_agcount / 2)
		dbprintf("WARNING: this may be a newer XFS filesystem.\n");
	if (error)
		exitcode = 3;
	dbprefix = oldprefix;
	return 0;
}

#ifdef DEBUG
typedef struct ltab {
	int	min;
	int	max;
} ltab_t;

static void
blocktrash_b(
	xfs_agnumber_t	agno,
	xfs_agblock_t	agbno,
	dbm_t		type,
	ltab_t		*ltabp,
	int		mode)
{
	int		bit;
	int		bitno;
	char		*buf;
	int		byte;
	int		len;
	int		mask;
	int		newbit;
	int		offset;
	static char	*modestr[] = {
		"zeroed", "set", "flipped", "randomized"
	};

	len = (int)((random() % (ltabp->max - ltabp->min + 1)) + ltabp->min);
	offset = (int)(random() % (int)(mp->m_sb.sb_blocksize * NBBY));
	push_cur();
	set_cur(&typtab[DBM_UNKNOWN],
		XFS_AGB_TO_DADDR(mp, agno, agbno), blkbb, DB_RING_IGN, NULL);
	if ((buf = iocur_top->data) == NULL) {
		dbprintf("can't read block %u/%u for trashing\n", agno, agbno);
		pop_cur();
		return;
	}
	for (bitno = 0; bitno < len; bitno++) {
		bit = (offset + bitno) % (mp->m_sb.sb_blocksize * NBBY);
		byte = bit / NBBY;
		bit %= NBBY;
		mask = 1 << bit;
		switch (mode) {
		case 0:
			newbit = 0;
			break;
		case 1:
			newbit = 1;
			break;
		case 2:
			newbit = (buf[byte] & mask) == 0;
			break;
		case 3:
			newbit = (int)random() & 1;
			break;
		}
		if (newbit)
			buf[byte] |= mask;
		else
			buf[byte] &= ~mask;
	}
	write_cur();
	pop_cur();
	printf("blocktrash: %u/%u %s block %d bit%s starting %d:%d %s\n",
		agno, agbno, typename[type], len, len == 1 ? "" : "s",
		offset / NBBY, offset % NBBY, modestr[mode]);
}

int
blocktrash_f(
	int		argc,
	char		**argv)
{
	xfs_agblock_t	agbno;
	xfs_agnumber_t	agno;
	xfs_drfsbno_t	bi;
	xfs_drfsbno_t	blocks;
	int		c;
	int		count;
	int		done;
	int		goodmask;
	int		i;
	ltab_t		*lentab;
	int		lentablen;
	int		max;
	int		min;
	int		mode;
	struct timeval	now;
	char		*p;
	xfs_drfsbno_t	randb;
	uint		seed;
	int		sopt;
	int		tmask;

	if (!dbmap) {
		dbprintf("must run blockget first\n");
		return 0;
	}
	optind = 0;
	count = 1;
	min = 1;
	max = 128 * NBBY;
	mode = 2;
	gettimeofday(&now, NULL);
	seed = (unsigned int)(now.tv_sec ^ now.tv_usec);
	sopt = 0;
	tmask = 0;
	goodmask = (1 << DBM_AGF) |
		   (1 << DBM_AGFL) |
		   (1 << DBM_AGI) |
		   (1 << DBM_ATTR) |
		   (1 << DBM_BTBMAPA) |
		   (1 << DBM_BTBMAPD) |
		   (1 << DBM_BTBNO) |
		   (1 << DBM_BTCNT) |
		   (1 << DBM_BTINO) |
		   (1 << DBM_DIR) |
		   (1 << DBM_INODE) |
		   (1 << DBM_QUOTA) |
		   (1 << DBM_RTBITMAP) |
		   (1 << DBM_RTSUM) |
		   (1 << DBM_SB);
	while ((c = getopt(argc, argv, "0123n:s:t:x:y:")) != EOF) {
		switch (c) {
		case '0':
			mode = 0;
			break;
		case '1':
			mode = 1;
			break;
		case '2':
			mode = 2;
			break;
		case '3':
			mode = 3;
			break;
		case 'n':
			count = (int)strtol(optarg, &p, 0);
			if (*p != '\0' || count <= 0) {
				dbprintf("bad blocktrash count %s\n", optarg);
				return 0;
			}
			break;
		case 's':
			seed = (uint)strtoul(optarg, &p, 0);
			sopt = 1;
			break;
		case 't':
			for (i = 0; typename[i]; i++) {
				if (strcmp(typename[i], optarg) == 0)
					break;
			}
			if (!typename[i] || (((1 << i) & goodmask) == 0)) {
				dbprintf("bad blocktrash type %s\n", optarg);
				return 0;
			}
			tmask |= 1 << i;
			break;
		case 'x':
			min = (int)strtol(optarg, &p, 0);
			if (*p != '\0' || min <= 0 ||
			    min > mp->m_sb.sb_blocksize * NBBY) {
				dbprintf("bad blocktrash min %s\n", optarg);
				return 0;
			}
			break;
		case 'y':
			max = (int)strtol(optarg, &p, 0);
			if (*p != '\0' || max <= 0 ||
			    max > mp->m_sb.sb_blocksize * NBBY) {
				dbprintf("bad blocktrash max %s\n", optarg);
				return 0;
			}
			break;
		default:
			dbprintf("bad option for blocktrash command\n");
			return 0;
		}
	}
	if (min > max) {
		dbprintf("bad min/max for blocktrash command\n");
		return 0;
	}
	if (tmask == 0)
		tmask = goodmask;
	lentab = xmalloc(sizeof(ltab_t));
	lentab->min = lentab->max = min;
	lentablen = 1;
	for (i = min + 1; i <= max; i++) {
		if ((i & (i - 1)) == 0) {
			lentab = xrealloc(lentab,
				sizeof(ltab_t) * (lentablen + 1));
			lentab[lentablen].min = lentab[lentablen].max = i;
			lentablen++;
		} else
			lentab[lentablen - 1].max = i;
	}
	for (blocks = 0, agno = 0; agno < mp->m_sb.sb_agcount; agno++) {
		for (agbno = 0, p = dbmap[agno];
		     agbno < mp->m_sb.sb_agblocks;
		     agbno++, p++) {
			if ((1 << *p) & tmask)
				blocks++;
		}
	}
	if (blocks == 0) {
		dbprintf("blocktrash: no matching blocks\n");
		return 0;
	}
	if (!sopt)
		dbprintf("blocktrash: seed %u\n", seed);
	srandom(seed);
	for (i = 0; i < count; i++) {
		randb = (xfs_drfsbno_t)((((__int64_t)random() << 32) |
					 random()) % blocks);
		for (bi = 0, agno = 0, done = 0;
		     !done && agno < mp->m_sb.sb_agcount;
		     agno++) {
			for (agbno = 0, p = dbmap[agno];
			     agbno < mp->m_sb.sb_agblocks;
			     agbno++, p++) {
				if (!((1 << *p) & tmask))
					continue;
				if (bi++ < randb)
					continue;
				blocktrash_b(agno, agbno, (dbm_t)*p,
					&lentab[random() % lentablen], mode);
				done = 1;
				break;
			}
		}
	}
	xfree(lentab);
	return 0;
}
#endif

int
blockuse_f(
	int		argc,
	char		**argv)
{
	xfs_agblock_t	agbno;
	xfs_agnumber_t	agno;
	int		c;
	int		count;
	xfs_agblock_t	end;
	xfs_fsblock_t	fsb;
	inodata_t	*i;
	char		*p;
	int		shownames;

	if (!dbmap) {
		dbprintf("must run blockget first\n");
		return 0;
	}
	optind = 0;
	count = 1;
	shownames = 0;
	fsb = XFS_DADDR_TO_FSB(mp, iocur_top->off >> BBSHIFT);
	agno = XFS_FSB_TO_AGNO(mp, fsb);
	end = agbno = XFS_FSB_TO_AGBNO(mp, fsb);
	while ((c = getopt(argc, argv, "c:n")) != EOF) {
		switch (c) {
		case 'c':
			count = (int)strtol(optarg, &p, 0);
			end = agbno + count - 1;
			if (*p != '\0' || count <= 0 ||
			    end >= mp->m_sb.sb_agblocks) {
				dbprintf("bad blockuse count %s\n", optarg);
				return 0;
			}
			break;
		case 'n':
			if (!nflag) {
				dbprintf("must run blockget -n first\n");
				return 0;
			}
			shownames = 1;
			break;
		default:
			dbprintf("bad option for blockuse command\n");
			return 0;
		}
	}
	while (agbno <= end) {
		p = &dbmap[agno][agbno];
		i = inomap[agno][agbno];
		dbprintf("block %llu (%u/%u) type %s",
			(xfs_dfsbno_t)XFS_AGB_TO_FSB(mp, agno, agbno),
			agno, agbno, typename[(dbm_t)*p]);
		if (i) {
			dbprintf(" inode %lld", i->ino);
			if (shownames && (p = inode_name(i->ino, NULL))) {
				dbprintf(" %s", p);
				xfree(p);
			}
		}
		dbprintf("\n");
		agbno++;
	}
	return 0;
}

static int
check_blist(
	xfs_fsblock_t	bno)
{
	int		i;

	for (i = 0; i < blist_size; i++) {
		if (blist[i] == bno)
			return 1;
	}
	return 0;
}

static void
check_dbmap(
	xfs_agnumber_t	agno,
	xfs_agblock_t	agbno,
	xfs_extlen_t	len,
	dbm_t		type)
{
	xfs_extlen_t	i;
	char		*p;

	for (i = 0, p = &dbmap[agno][agbno]; i < len; i++, p++) {
		if ((dbm_t)*p != type) {
			if (!sflag || CHECK_BLISTA(agno, agbno + i))
				dbprintf("block %u/%u expected type %s got "
					 "%s\n",
					agno, agbno + i, typename[type],
					typename[(dbm_t)*p]);
			error++;
		}
	}
}

void
check_init(void)
{
	add_command(&blockfree_cmd);
	add_command(&blockget_cmd);
#ifdef DEBUG
	add_command(&blocktrash_cmd);
#endif
	add_command(&blockuse_cmd);
	add_command(&ncheck_cmd);
}

static int
check_inomap(
	xfs_agnumber_t	agno,
	xfs_agblock_t	agbno,
	xfs_extlen_t	len,
	xfs_ino_t	c_ino)
{
	xfs_extlen_t	i;
	inodata_t	**idp;
	int		rval;

	if (!check_range(agno, agbno, len))  {
		dbprintf("blocks %u/%u..%u claimed by inode %lld\n",
			agno, agbno, agbno + len - 1, c_ino);
		return 0;
	}
	for (i = 0, rval = 1, idp = &inomap[agno][agbno]; i < len; i++, idp++) {
		if (*idp) {
			if (!sflag || (*idp)->ilist ||
			    CHECK_BLISTA(agno, agbno + i))
				dbprintf("block %u/%u claimed by inode %lld, "
					 "previous inum %lld\n",
					agno, agbno + i, c_ino, (*idp)->ino);
			error++;
			rval = 0;
		}
	}
	return rval;
}

static void
check_linkcounts(
	xfs_agnumber_t	agno)
{
	inodata_t	*ep;
	inodata_t	**ht;
	int		idx;
	char		*path;

	ht = inodata[agno];
	for (idx = 0; idx < inodata_hash_size; ht++, idx++) {
		ep = *ht;
		while (ep) {
			if (ep->link_set != ep->link_add || ep->link_set == 0) {
				path = inode_name(ep->ino, NULL);
				if (!path && ep->link_add)
					path = xstrdup("?");
				if (!sflag || ep->ilist) {
					if (ep->link_add)
						dbprintf("link count mismatch "
							 "for inode %lld (name "
							 "%s), nlink %d, "
							 "counted %d\n",
							ep->ino, path,
							ep->link_set,
							ep->link_add);
					else if (ep->link_set)
						dbprintf("disconnected inode "
							 "%lld, nlink %d\n",
							ep->ino, ep->link_set);
					else
						dbprintf("allocated inode %lld "
							 "has 0 link count\n",
							ep->ino);
				}
				if (path)
					xfree(path);
				error++;
			} else if (verbose || ep->ilist) {
				path = inode_name(ep->ino, NULL);
				if (path) {
					dbprintf("inode %lld name %s\n",
						ep->ino, path);
					xfree(path);
				}
			}
			ep = ep->next;
		}
	}
		
}

static int
check_range(
	xfs_agnumber_t	agno,
	xfs_agblock_t	agbno,
	xfs_extlen_t	len)
{
	xfs_extlen_t	i;

	if (agno >= mp->m_sb.sb_agcount ||
	    agbno + len - 1 >= mp->m_sb.sb_agblocks) {
		for (i = 0; i < len; i++) {
			if (!sflag || CHECK_BLISTA(agno, agbno + i))
				dbprintf("block %u/%u out of range\n",
					agno, agbno + i);
		}
		error++;
		return 0;
	}
	return 1;
}

static void
check_rdbmap(
	xfs_drfsbno_t	bno,
	xfs_extlen_t	len,
	dbm_t		type)
{
	xfs_extlen_t	i;
	char		*p;

	for (i = 0, p = &dbmap[mp->m_sb.sb_agcount][bno]; i < len; i++, p++) {
		if ((dbm_t)*p != type) {
			if (!sflag || CHECK_BLIST(bno + i))
				dbprintf("rtblock %llu expected type %s got "
					 "%s\n",
					bno + i, typename[type],
					typename[(dbm_t)*p]);
			error++;
		}
	}
}

static int
check_rinomap(
	xfs_drfsbno_t	bno,
	xfs_extlen_t	len,
	xfs_ino_t	c_ino)
{
	xfs_extlen_t	i;
	inodata_t	**idp;
	int		rval;

	if (!check_rrange(bno, len)) {
		dbprintf("rtblocks %llu..%llu claimed by inode %lld\n",
			bno, bno + len - 1, c_ino);
		return 0;
	}
	for (i = 0, rval = 1, idp = &inomap[mp->m_sb.sb_agcount][bno];
	     i < len;
	     i++, idp++) {
		if (*idp) {
			if (!sflag || (*idp)->ilist || CHECK_BLIST(bno + i))
				dbprintf("rtblock %llu claimed by inode %lld, "
					 "previous inum %lld\n",
					bno + i, c_ino, (*idp)->ino);
			error++;
			rval = 0;
		}
	}
	return rval;
}

static void
check_rootdir(void)
{
	inodata_t	*id;

	id = find_inode(mp->m_sb.sb_rootino, 0);
	if (id == NULL) {
		if (!sflag)
			dbprintf("root inode %lld is missing\n",
				mp->m_sb.sb_rootino);
		error++;
	} else if (!id->isdir) {
		if (!sflag || id->ilist)
			dbprintf("root inode %lld is not a directory\n",
				mp->m_sb.sb_rootino);
		error++;
	}
}

static int
check_rrange(
	xfs_drfsbno_t	bno,
	xfs_extlen_t	len)
{
	xfs_extlen_t	i;

	if (bno + len - 1 >= mp->m_sb.sb_rblocks) {
		for (i = 0; i < len; i++) {
			if (!sflag || CHECK_BLIST(bno + i))
				dbprintf("rtblock %llu out of range\n",
					bno + i);
		}
		error++;
		return 0;
	}
	return 1;
}

static void
check_set_dbmap(
	xfs_agnumber_t	agno,
	xfs_agblock_t	agbno,
	xfs_extlen_t	len,
	dbm_t		type1,
	dbm_t		type2,
	xfs_agnumber_t	c_agno,
	xfs_agblock_t	c_agbno)
{
	xfs_extlen_t	i;
	int		mayprint;
	char		*p;

	if (!check_range(agno, agbno, len))  {
		dbprintf("blocks %u/%u..%u claimed by block %u/%u\n", agno,
			agbno, agbno + len - 1, c_agno, c_agbno);
		return;
	}
	check_dbmap(agno, agbno, len, type1);
	mayprint = verbose | blist_size;
	for (i = 0, p = &dbmap[agno][agbno]; i < len; i++, p++) {
		*p = (char)type2;
		if (mayprint && (verbose || CHECK_BLISTA(agno, agbno + i)))
			dbprintf("setting block %u/%u to %s\n", agno, agbno + i,
				typename[type2]);
	}
}

static void
check_set_rdbmap(
	xfs_drfsbno_t	bno,
	xfs_extlen_t	len,
	dbm_t		type1,
	dbm_t		type2)
{
	xfs_extlen_t	i;
	int		mayprint;
	char		*p;

	if (!check_rrange(bno, len))
		return;
	check_rdbmap(bno, len, type1);
	mayprint = verbose | blist_size;
	for (i = 0, p = &dbmap[mp->m_sb.sb_agcount][bno]; i < len; i++, p++) {
		*p = (char)type2;
		if (mayprint && (verbose || CHECK_BLIST(bno + i)))
			dbprintf("setting rtblock %llu to %s\n",
				bno + i, typename[type2]);
	}
}

static void
check_summary(void)
{
	xfs_drfsbno_t	bno;
	xfs_suminfo_t	*csp;
	xfs_suminfo_t	*fsp;
	int		log;

	csp = sumcompute;
	fsp = sumfile;
	for (log = 0; log < mp->m_rsumlevels; log++) {
		for (bno = 0;
		     bno < mp->m_sb.sb_rbmblocks;
		     bno++, csp++, fsp++) {
			if (*csp != *fsp) {
				if (!sflag)
					dbprintf("rt summary mismatch, size %d "
						 "block %llu, file: %d, "
						 "computed: %d\n",
						log, bno, *fsp, *csp);
				error++;
			}
		}
	}
}

static void
checknot_dbmap(
	xfs_agnumber_t	agno,
	xfs_agblock_t	agbno,
	xfs_extlen_t	len,
	int		typemask)
{
	xfs_extlen_t	i;
	char		*p;

	if (!check_range(agno, agbno, len))
		return;
	for (i = 0, p = &dbmap[agno][agbno]; i < len; i++, p++) {
		if ((1 << *p) & typemask) {
			if (!sflag || CHECK_BLISTA(agno, agbno + i))
				dbprintf("block %u/%u type %s not expected\n",
					agno, agbno + i, typename[*p]);
			error++;
		}
	}
}

static void
checknot_rdbmap(
	xfs_drfsbno_t	bno,
	xfs_extlen_t	len,
	int		typemask)
{
	xfs_extlen_t	i;
	char		*p;

	if (!check_rrange(bno, len))
		return;
	for (i = 0, p = &dbmap[mp->m_sb.sb_agcount][bno]; i < len; i++, p++) {
		if ((1 << *p) & typemask) {
			if (!sflag || CHECK_BLIST(bno + i))
				dbprintf("rtblock %llu type %s not expected\n",
					bno + i, typename[*p]);
			error++;
		}
	}
}

#if VERS >= V_654
static void
dir_hash_add(
	xfs_dahash_t		hash,
	xfs_dir2_dataptr_t	addr)
{
	int			i;
	dirhash_t		*p;

	i = DIR_HASH_FUNC(hash, addr);
	p = malloc(sizeof(*p));
	p->next = dirhash[i];
	dirhash[i] = p;
	p->entry.hashval = hash;
	p->entry.address = addr;
	p->seen = 0;
}

static void
dir_hash_check(
	inodata_t	*id,
	int		v)
{
	int		i;
	dirhash_t	*p;

	for (i = 0; i < DIR_HASH_SIZE; i++) {
		for (p = dirhash[i]; p; p = p->next) {
			if (p->seen)
				continue;
			if (!sflag || id->ilist || v)
				dbprintf("dir ino %lld missing leaf entry for "
					 "%x/%x\n",
					id->ino, p->entry.hashval,
					p->entry.address);
			error++;
		}
	}
}

static void
dir_hash_done(void)
{
	int		i;
	dirhash_t	*n;
	dirhash_t	*p;

	for (i = 0; i < DIR_HASH_SIZE; i++) {
		for (p = dirhash[i]; p; p = n) {
			n = p->next;
			free(p);
		}
		dirhash[i] = NULL;
	}
}

static void
dir_hash_init(void)
{
	if (!dirhash)
		dirhash = calloc(DIR_HASH_SIZE, sizeof(*dirhash));
}

static int
dir_hash_see(
	xfs_dahash_t		hash,
	xfs_dir2_dataptr_t	addr)
{
	int			i;
	dirhash_t		*p;

	i = DIR_HASH_FUNC(hash, addr);
	for (p = dirhash[i]; p; p = p->next) {
		if (p->entry.hashval == hash && p->entry.address == addr) {
			if (p->seen)
				return 1;
			p->seen = 1;
			return 0;
		}
	}
	return -1;
}
#endif

static inodata_t *
find_inode(
	xfs_ino_t	ino,
	int		add)
{
	xfs_agino_t	agino;
	xfs_agnumber_t	agno;
	inodata_t	*ent;
	inodata_t	**htab;
	xfs_agino_t	ih;

	agno = XFS_INO_TO_AGNO(mp, ino);
	agino = XFS_INO_TO_AGINO(mp, ino);
	if (agno >= mp->m_sb.sb_agcount ||
	    XFS_AGINO_TO_INO(mp, agno, agino) != ino)
		return NULL;
	htab = inodata[agno];
	ih = agino % inodata_hash_size;
	ent = htab[ih];
	while (ent) {
		if (ent->ino == ino)
			return ent;
		ent = ent->next;
	}
	if (!add)
		return NULL;
	ent = xcalloc(1, sizeof(*ent));
	ent->ino = ino;
	ent->next = htab[ih];
	htab[ih] = ent;
	return ent;
}

static void
free_inodata(
	xfs_agnumber_t	agno)
{
	inodata_t	*hp;
	inodata_t	**ht;
	int		i;
	inodata_t	*next;

	ht = inodata[agno];
	for (i = 0; i < inodata_hash_size; i++) {
		hp = ht[i];
		while (hp) {
			next = hp->next;
			if (hp->name)
				xfree(hp->name);
			xfree(hp);
			hp = next;
		}
	}
	xfree(ht);
}

static int
init(
	int		argc,
	char		**argv)
{
	xfs_fsblock_t	bno;
	int		c;
	xfs_ino_t	ino;
	int		rt;

	if (mp->m_sb.sb_magicnum != XFS_SB_MAGIC) {
		dbprintf("bad superblock magic number %x, giving up\n",
			mp->m_sb.sb_magicnum);
		return 0;
	}
	rt = mp->m_sb.sb_rextents != 0;
	dbmap = xmalloc((mp->m_sb.sb_agcount + rt) * sizeof(*dbmap));
	inomap = xmalloc((mp->m_sb.sb_agcount + rt) * sizeof(*inomap));
	inodata = xmalloc(mp->m_sb.sb_agcount * sizeof(*inodata));
	inodata_hash_size =
		(int)MAX(MIN(mp->m_sb.sb_icount /
				(INODATA_AVG_HASH_LENGTH * mp->m_sb.sb_agcount),
			     MAX_INODATA_HASH_SIZE),
			 MIN_INODATA_HASH_SIZE);
	for (c = 0; c < mp->m_sb.sb_agcount; c++) {
		dbmap[c] = xcalloc(mp->m_sb.sb_agblocks, sizeof(**dbmap));
		inomap[c] = xcalloc(mp->m_sb.sb_agblocks, sizeof(**inomap));
		inodata[c] = xcalloc(inodata_hash_size, sizeof(**inodata));
	}
	if (rt) {
		dbmap[c] = xcalloc(mp->m_sb.sb_rblocks, sizeof(**dbmap));
		inomap[c] = xcalloc(mp->m_sb.sb_rblocks, sizeof(**inomap));
		sumfile = xcalloc(mp->m_rsumsize, 1);
		sumcompute = xcalloc(mp->m_rsumsize, 1);
	}
	nflag = sflag = verbose = optind = 0;
	while ((c = getopt(argc, argv, "b:i:npsv")) != EOF) {
		switch (c) {
		case 'b':
			bno = atoll(optarg);
			add_blist(bno);
			break;
		case 'i':
			ino = atoll(optarg);
			add_ilist(ino);
			break;
		case 'n':
			nflag = 1;
			break;
		case 'p':
			pflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			dbprintf("bad option for blockget command\n");
			return 0;
		}
	}
	error = sbver_err = serious_error = 0;
	fdblocks = frextents = icount = ifree = 0;
#if VERS >= V_62
	sbversion = XFS_SB_VERSION_4;
	if (mp->m_sb.sb_inoalignmt)
		sbversion |= XFS_SB_VERSION_ALIGNBIT;
	if ((mp->m_sb.sb_uquotino && mp->m_sb.sb_uquotino != NULLFSINO) ||
	    (mp->m_sb.sb_pquotino && mp->m_sb.sb_pquotino != NULLFSINO))
		sbversion |= XFS_SB_VERSION_QUOTABIT;
	quota_init();
#endif
	return 1;
}

static char *
inode_name(
	xfs_ino_t	ino,
	inodata_t	**ipp)
{
	inodata_t	*id;
	char		*npath;
	char		*path;

	id = find_inode(ino, 0);
	if (ipp)
		*ipp = id;
	if (id == NULL)
		return NULL;
	if (id->name == NULL)
		return NULL;
	path = xstrdup(id->name);
	while (id->parent) {
		id = id->parent;
		if (id->name == NULL)
			break;
		npath = prepend_path(path, id->name);
		xfree(path);
		path = npath;
	}
	return path;
}

static int
ncheck_f(
	int		argc,
	char		**argv)
{
	xfs_agnumber_t	agno;
	int		c;
	inodata_t	*hp;
	inodata_t	**ht;
	int		i;
	inodata_t	*id;
	xfs_ino_t	*ilist;
	int		ilist_size;
	xfs_ino_t	*ilp;
	xfs_ino_t	ino;
	char		*p;
	int		security;

	if (!inodata || !nflag) {
		dbprintf("must run blockget -n first\n");
		return 0;
	}
	security = optind = ilist_size = 0;
	ilist = NULL;
	while ((c = getopt(argc, argv, "i:s")) != EOF) {
		switch (c) {
		case 'i':
			ino = atoll(optarg);
			ilist = xrealloc(ilist, (ilist_size + 1) *
				sizeof(*ilist));
			ilist[ilist_size++] = ino;
			break;
		case 's':
			security = 1;
			break;
		default:
			dbprintf("bad option -%c for ncheck command\n", c);
			return 0;
		}
	}
	if (ilist) {
		for (ilp = ilist; ilp < &ilist[ilist_size]; ilp++) {
			ino = *ilp;
			if (p = inode_name(ino, &hp)) {
				dbprintf("%11llu %s", ino, p);
				if (hp->isdir)
					dbprintf("/.");
				dbprintf("\n");
				xfree(p);
			}
		}
		xfree(ilist);
		return 0;
	}
	for (agno = 0; agno < mp->m_sb.sb_agcount; agno++) {
		ht = inodata[agno];
		for (i = 0; i < inodata_hash_size; i++) {
			hp = ht[i];
			for (hp = ht[i]; hp; hp = hp->next) {
				ino = XFS_AGINO_TO_INO(mp, agno, hp->ino);
				p = inode_name(ino, &id);
				if (!p || !id)
					continue;
				if (!security || id->security) {
					dbprintf("%11llu %s", ino, p);
					if (hp->isdir)
						dbprintf("/.");
					dbprintf("\n");
				}
				xfree(p);
			}
		}
	}
	return 0;
}

static char *
prepend_path(
	char	*oldpath,
	char	*parent)
{
	int	len;
	char	*path;

	len = (int)(strlen(oldpath) + strlen(parent) + 2);
	path = xmalloc(len);
	sprintf(path, "%s/%s", parent, oldpath);
	return path;
}

#if VERS >= V_654
static xfs_ino_t
process_block_dir_v2(
	blkmap_t	*blkmap,
	int		*dot,
	int		*dotdot,
	inodata_t	*id)
{
	xfs_fsblock_t	b;
	bbmap_t		bbmap;
	bmap_ext_t	*bmp;
	int		nex;
	xfs_ino_t	parent;
	int		v;
	int		x;

	nex = blkmap_getn(blkmap, 0, mp->m_dirblkfsbs, &bmp);
	v = id->ilist || verbose;
	if (nex == 0) {
		if (!sflag || v)
			dbprintf("block 0 for directory inode %lld is "
				 "missing\n",
				id->ino);
		error++;
		return 0;
	}
	push_cur();
	if (nex > 1)
		make_bbmap(&bbmap, nex, bmp);
	set_cur(&typtab[TYP_DIR], XFS_FSB_TO_DADDR(mp, bmp->startblock),
		mp->m_dirblkfsbs * blkbb, DB_RING_IGN, nex > 1 ? &bbmap : NULL);
	for (x = 0; !v && x < nex; x++) {
		for (b = bmp[x].startblock;
		     !v && b < bmp[x].startblock + bmp[x].blockcount;
		     b++)
			v = CHECK_BLIST(b);
	}
	free(bmp);
	if (iocur_top->data == NULL) {
		if (!sflag || id->ilist || v)
			dbprintf("can't read block 0 for directory inode "
				 "%lld\n",
				id->ino);
		error++;
		return 0;
	}
	dir_hash_init();
	parent = process_data_dir_v2(dot, dotdot, id, v, mp->m_dirdatablk,
		NULL);
	dir_hash_check(id, v);
	dir_hash_done();
	pop_cur();
	return parent;
}
#endif

static void
process_bmbt_reclist(
	xfs_bmbt_rec_32_t	*rp,
	int			numrecs,
	dbm_t			type,
	inodata_t		*id,
	xfs_drfsbno_t		*tot,
	blkmap_t		**blkmapp)
{
	xfs_agblock_t		agbno;
	xfs_agnumber_t		agno;
	xfs_fsblock_t		b;
	xfs_dfilblks_t		c;
	xfs_dfilblks_t		cp;
	int			f;
	int			i;
	xfs_agblock_t		iagbno;
	xfs_agnumber_t		iagno;
	xfs_dfiloff_t		o;
	xfs_dfiloff_t		op;
	xfs_dfsbno_t		s;
	int			v;

	v = verbose || id->ilist;
	iagno = XFS_INO_TO_AGNO(mp, id->ino);
	iagbno = XFS_INO_TO_AGBNO(mp, id->ino);
	for (i = 0; i < numrecs; i++, rp++) {
		convert_extent(rp, &o, &s, &c, &f);
		if (v)
			dbprintf("inode %lld extent [%lld,%lld,%lld,%d]\n",
				id->ino, o, s, c, f);
		if (!sflag && i > 0 && op + cp > o)
			dbprintf("bmap rec out of order, inode %lld entry %d\n",
				id->ino, i);
		op = o;
		cp = c;
		if (type == DBM_RTDATA) {
			if (!sflag && s >= mp->m_sb.sb_rblocks) {
				dbprintf("inode %lld bad rt block number %lld, "
					 "offset %lld\n",
					id->ino, s, o);
				continue;
			}
		} else if (!sflag) {
			agno = XFS_FSB_TO_AGNO(mp, s);
			agbno = XFS_FSB_TO_AGBNO(mp, s);
			if (agno >= mp->m_sb.sb_agcount ||
			    agbno >= mp->m_sb.sb_agblocks) {
				dbprintf("inode %lld bad block number %lld "
					 "[%d,%d], offset %lld\n",
					id->ino, s, agno, agbno, o);
				continue;
			}
			if (agbno + c - 1 >= mp->m_sb.sb_agblocks) {
				dbprintf("inode %lld bad block number %lld "
					 "[%d,%d], offset %lld\n",
					id->ino, s + c - 1, agno,
					agbno + (xfs_agblock_t)c - 1, o);
				continue;
			}
		}
		if (blkmapp && *blkmapp)
			blkmap_set_ext(blkmapp, (xfs_fileoff_t)o,
				(xfs_fsblock_t)s, (xfs_extlen_t)c);
		if (type == DBM_RTDATA) {
			set_rdbmap((xfs_fsblock_t)s, (xfs_extlen_t)c,
				DBM_RTDATA);
			set_rinomap((xfs_fsblock_t)s, (xfs_extlen_t)c, id);
			for (b = (xfs_fsblock_t)s;
			     blist_size && b < s + c;
			     b++, o++) {
				if (CHECK_BLIST(b))
					dbprintf("inode %lld block %lld at "
						 "offset %lld\n",
						id->ino, (xfs_dfsbno_t)b, o);
			}
		} else {
			agno = XFS_FSB_TO_AGNO(mp, (xfs_fsblock_t)s);
			agbno = XFS_FSB_TO_AGBNO(mp, (xfs_fsblock_t)s);
			set_dbmap(agno, agbno, (xfs_extlen_t)c, type, iagno,
				iagbno);
			set_inomap(agno, agbno, (xfs_extlen_t)c, id);
			for (b = (xfs_fsblock_t)s;
			     blist_size && b < s + c;
			     b++, o++, agbno++) {
				if (CHECK_BLIST(b))
					dbprintf("inode %lld block %lld at "
						 "offset %lld\n",
						id->ino, (xfs_dfsbno_t)b, o);
			}
		}
		*tot += c;
	}
}

static void
process_btinode(
	inodata_t		*id,
	xfs_dinode_t		*dip,
	dbm_t			type,
	xfs_drfsbno_t		*totd,
	xfs_drfsbno_t		*toti,
	xfs_extnum_t		*nex,
	blkmap_t		**blkmapp,
	int			whichfork)
{
	xfs_bmdr_block_t	*dib;
	int			i;
	xfs_bmbt_ptr_t		*pp;
	xfs_bmbt_rec_32_t	*rp;

	dib = (xfs_bmdr_block_t *)XFS_DFORK_PTR(dip, whichfork);
	if (dib->bb_level >= XFS_BM_MAXLEVELS(mp, whichfork)) {
		if (!sflag || id->ilist)
			dbprintf("level for ino %lld %s fork bmap root too "
				 "large (%u)\n",
				id->ino,
				whichfork == XFS_DATA_FORK ? "data" : "attr",
				dib->bb_level);
		error++;
		return;
	}
	if (dib->bb_numrecs >
	    XFS_BTREE_BLOCK_MAXRECS(XFS_DFORK_SIZE(dip, mp, whichfork),
		    xfs_bmdr, dib->bb_level == 0)) {
		if (!sflag || id->ilist)
			dbprintf("numrecs for ino %lld %s fork bmap root too "
				 "large (%u)\n",
				id->ino, 
				whichfork == XFS_DATA_FORK ? "data" : "attr",
				dib->bb_numrecs);
		error++;
		return;
	}
	if (dib->bb_level == 0) {
		rp = (xfs_bmbt_rec_32_t *)XFS_BTREE_REC_ADDR(
			XFS_DFORK_SIZE(dip, mp, whichfork),
			xfs_bmdr, dib, 1,
			XFS_BTREE_BLOCK_MAXRECS(XFS_DFORK_SIZE(dip, mp,
					whichfork),
				xfs_bmdr, 1));
		process_bmbt_reclist(rp, dib->bb_numrecs, type, id, totd,
			blkmapp);
		*nex += dib->bb_numrecs;
		return;
	} else {
		pp = XFS_BTREE_PTR_ADDR(XFS_DFORK_SIZE(dip, mp, whichfork),
			xfs_bmdr, dib, 1,
			XFS_BTREE_BLOCK_MAXRECS(XFS_DFORK_SIZE(dip, mp,
							       whichfork),
						xfs_bmdr, 0));
		for (i = 0; i < dib->bb_numrecs; i++)
			scan_lbtree((xfs_fsblock_t)pp[i], dib->bb_level,
				scanfunc_bmap, type, id, totd, toti, nex,
				blkmapp, 1,
				whichfork == XFS_DATA_FORK ?
					TYP_BMAPBTD : TYP_BMAPBTA);
	}
	if (*nex <=
	    XFS_DFORK_SIZE(dip, mp, whichfork) / sizeof(xfs_bmbt_rec_t)) {
		if (!sflag || id->ilist)
			dbprintf("extent count for ino %lld %s fork too low "
				 "(%d) for file format\n",
				id->ino,
				whichfork == XFS_DATA_FORK ? "data" : "attr",
				*nex);
		error++;
	}
}

#if VERS >= V_654
static xfs_ino_t
process_data_dir_v2(
	int			*dot,
	int			*dotdot,
	inodata_t		*id,
	int			v,
	xfs_dablk_t		dabno,
	freetab_t		**freetabp)
{
	xfs_dir2_dataptr_t	addr;
	xfs_dir2_data_free_t	*bf;
	int			bf_err;
	xfs_dir2_block_t	*block;
	xfs_dir2_block_tail_t	*btp;
	inodata_t		*cid;
	int			count;
	xfs_dir2_data_t		*data;
	xfs_dir2_db_t		db;
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_free_t	*dfp;
	xfs_dir2_data_unused_t	*dup;
	char			*endptr;
	int			freeseen;
	freetab_t		*freetab;
	xfs_dahash_t		hash;
	int			i;
	int			lastfree;
	int			lastfree_err;
	xfs_dir2_leaf_entry_t	*lep;
	xfs_ino_t		lino;
	xfs_ino_t		parent = 0;
	char			*ptr;
	int			stale;
	int			tag_err;
	xfs_dir2_data_off_t	*tagp;

	data = iocur_top->data;
	block = iocur_top->data;
	if (block->hdr.magic != XFS_DIR2_BLOCK_MAGIC &&
	    data->hdr.magic != XFS_DIR2_DATA_MAGIC) {
		if (!sflag || v)
			dbprintf("bad directory data magic # %#x for dir ino "
				 "%lld block %d\n",
				data->hdr.magic, id->ino, dabno);
		error++;
		return NULLFSINO;
	}
	db = XFS_DIR2_DA_TO_DB(mp, dabno);
	bf = data->hdr.bestfree;
	ptr = (char *)data->u;
	if (block->hdr.magic == XFS_DIR2_BLOCK_MAGIC) {
		btp = XFS_DIR2_BLOCK_TAIL_P(mp, block);
		lep = XFS_DIR2_BLOCK_LEAF_P(btp);
		endptr = (char *)lep;
		if (endptr <= ptr || endptr > (char *)btp) {
			endptr = (char *)data + mp->m_dirblksize;
			lep = NULL;
			if (!sflag || v)
				dbprintf("bad block directory tail for dir ino "
					 "%lld\n",
					id->ino);
			error++;
		}
	} else
		endptr = (char *)data + mp->m_dirblksize;
	bf_err = lastfree_err = tag_err = 0;
	count = lastfree = freeseen = 0;
	if (bf[0].length == 0) {
		bf_err += bf[0].offset != 0;
		freeseen |= 1 << 0;
	}
	if (bf[1].length == 0) {
		bf_err += bf[1].offset != 0;
		freeseen |= 1 << 1;
	}
	if (bf[2].length == 0) {
		bf_err += bf[2].offset != 0;
		freeseen |= 1 << 2;
	}
	bf_err += bf[0].length < bf[1].length;
	bf_err += bf[1].length < bf[2].length;
	if (freetabp) {
		freetab = *freetabp;
		if (freetab->naents <= db) {
			*freetabp = freetab =
				realloc(freetab, FREETAB_SIZE(db + 1));
			for (i = freetab->naents; i < db; i++)
				freetab->ents[i] = NULLDATAOFF;
			freetab->naents = db + 1;
		}
		if (freetab->nents < db + 1)
			freetab->nents = db + 1;
		freetab->ents[db] = bf[0].length;
	}
	while (ptr < endptr) {
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (dup->freetag == XFS_DIR2_DATA_FREE_TAG) {
			lastfree_err += lastfree != 0;
			if ((dup->length & (XFS_DIR2_DATA_ALIGN - 1)) ||
			    dup->length == 0 ||
			    (char *)(tagp = XFS_DIR2_DATA_UNUSED_TAG_P(dup)) >=
			    endptr) {
				if (!sflag || v)
					dbprintf("dir %lld block %d bad free "
						 "entry at %d\n",
						id->ino, dabno,
						(int)((char *)dup -
						      (char *)data));
				error++;
				break;
			}
			tag_err += *tagp != (char *)dup - (char *)data;
			dfp = process_data_dir_v2_freefind(data, dup);
			if (dfp) {
				i = (int)(dfp - bf);
				bf_err += (freeseen & (1 << i)) != 0;
				freeseen |= 1 << i;
			} else
				bf_err += dup->length > bf[2].length;
			ptr += dup->length;
			lastfree = 1;
			continue;
		}
		dep = (xfs_dir2_data_entry_t *)dup;
		if (dep->namelen == 0) {
			if (!sflag || v)
				dbprintf("dir %lld block %d zero length entry "
					 "at %d\n",
					id->ino, dabno,
					(int)((char *)dep - (char *)data));
			error++;
		}
		tagp = XFS_DIR2_DATA_ENTRY_TAG_P(dep);
		if ((char *)tagp >= endptr) {
			if (!sflag || v)
				dbprintf("dir %lld block %d bad entry at %d\n",
					id->ino, dabno,
					(int)((char *)dep - (char *)data));
			error++;
			break;
		}
		tag_err += *tagp != (char *)dep - (char *)data;
		addr = XFS_DIR2_DB_OFF_TO_DATAPTR(mp, db,
			(char *)dep - (char *)data);
		hash = xfs_da_hashname((char *)dep->name, dep->namelen);
		dir_hash_add(hash, addr);
		ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
		count++;
		lastfree = 0;
		lino = dep->inumber;
		cid = find_inode(lino, 1);
		if (v)
			dbprintf("dir %lld block %d entry %*.*s %lld\n",
				id->ino, dabno, dep->namelen, dep->namelen,
				dep->name, lino);
		if (cid)
			addlink_inode(cid);
		else {
			if (!sflag || v)
				dbprintf("dir %lld block %d entry %*.*s bad "
					 "inode number %lld\n",
					id->ino, dabno, dep->namelen,
					dep->namelen, dep->name, lino);
			error++;
		}
		if (dep->namelen == 2 && dep->name[0] == '.' &&
		    dep->name[1] == '.') {
			if (parent) {
				if (!sflag || v)
					dbprintf("multiple .. entries in dir "
						 "%lld (%lld, %lld)\n",
						id->ino, parent, lino);
				error++;
			} else
				parent = cid ? lino : NULLFSINO;
			(*dotdot)++;
		} else if (dep->namelen != 1 || dep->name[0] != '.') {
			if (cid != NULL) {
				if (!cid->parent)
					cid->parent = id;
				addname_inode(cid, (char *)dep->name,
					dep->namelen);
			}
		} else {
			if (lino != id->ino) {
				if (!sflag || v)
					dbprintf("dir %lld entry . inode "
						 "number mismatch (%lld)\n",
						id->ino, lino);
				error++;
			}
			(*dot)++;
		}
	}
	if (data->hdr.magic == XFS_DIR2_BLOCK_MAGIC) {
		endptr = (char *)data + mp->m_dirblksize;
		for (i = stale = 0; lep && i < btp->count; i++) {
			if ((char *)&lep[i] >= endptr) {
				if (!sflag || v)
					dbprintf("dir %lld block %d bad count "
						 "%u\n",
						id->ino, dabno, btp->count);
				error++;
				break;
			}
			if (lep[i].address == XFS_DIR2_NULL_DATAPTR)
				stale++;
			else if (dir_hash_see(lep[i].hashval, lep[i].address)) {
				if (!sflag || v)
					dbprintf("dir %lld block %d extra leaf "
						 "entry %x %x\n",
						id->ino, dabno, lep[i].hashval,
						lep[i].address);
				error++;
			}
		}
	}
	bf_err += freeseen != 7;
	if (bf_err) {
		if (!sflag || v)
			dbprintf("dir %lld block %d bad bestfree data\n",
				id->ino, dabno);
		error++;
	}
	if (data->hdr.magic == XFS_DIR2_BLOCK_MAGIC &&
	    count != btp->count - btp->stale) {
		if (!sflag || v)
			dbprintf("dir %lld block %d bad block tail count %d "
				 "(stale %d)\n",
				id->ino, dabno, btp->count, btp->stale);
		error++;
	}
	if (data->hdr.magic == XFS_DIR2_BLOCK_MAGIC && stale != btp->stale) {
		if (!sflag || v)
			dbprintf("dir %lld block %d bad stale tail count %d\n",
				id->ino, dabno, btp->stale);
		error++;
	}
	if (lastfree_err) {
		if (!sflag || v)
			dbprintf("dir %lld block %d consecutive free entries\n",
				id->ino, dabno);
		error++;
	}
	if (tag_err) {
		if (!sflag || v)
			dbprintf("dir %lld block %d entry/unused tag "
				 "mismatch\n",
				id->ino, dabno);
		error++;
	}
	return parent;
}

static xfs_dir2_data_free_t *
process_data_dir_v2_freefind(
	xfs_dir2_data_t		*data,
	xfs_dir2_data_unused_t	*dup)
{
	xfs_dir2_data_free_t	*dfp;
	xfs_dir2_data_aoff_t	off;

	off = (xfs_dir2_data_aoff_t)((char *)dup - (char *)data);
	if (dup->length < data->hdr.bestfree[XFS_DIR2_DATA_FD_COUNT - 1].length)
		return NULL;
	for (dfp = &data->hdr.bestfree[0];
	     dfp < &data->hdr.bestfree[XFS_DIR2_DATA_FD_COUNT];
	     dfp++) {
		if (dfp->offset == 0)
			return NULL;
		if (dfp->offset == off)
			return dfp;
	}
	return NULL;
}
#endif

static void
process_dir(
	xfs_dinode_t	*dip,
	blkmap_t	*blkmap,
	inodata_t	*id)
{
	xfs_fsblock_t	bno;
	int		dot;
	int		dotdot;
	xfs_ino_t	parent;

	dot = dotdot = 0;
#if VERS >= V_654
	if (XFS_DIR_IS_V2(mp)) {
		if (process_dir_v2(dip, blkmap, &dot, &dotdot, id, &parent))
			return;
	} else
#endif
	{
		if (process_dir_v1(dip, blkmap, &dot, &dotdot, id, &parent))
			return;
	}
	bno = XFS_INO_TO_FSB(mp, id->ino);
	if (dot == 0) {
		if (!sflag || id->ilist || CHECK_BLIST(bno))
			dbprintf("no . entry for directory %lld\n", id->ino);
		error++;
	}
	if (dotdot == 0) {
		if (!sflag || id->ilist || CHECK_BLIST(bno))
			dbprintf("no .. entry for directory %lld\n", id->ino);
		error++;
	} else if (parent == id->ino && id->ino != mp->m_sb.sb_rootino) {
		if (!sflag || id->ilist || CHECK_BLIST(bno))
			dbprintf(". and .. same for non-root directory %lld\n",
				id->ino);
		error++;
	} else if (id->ino == mp->m_sb.sb_rootino && id->ino != parent) {
		if (!sflag || id->ilist || CHECK_BLIST(bno))
			dbprintf("root directory %lld has .. %lld\n", id->ino,
				parent);
		error++;
	} else if (parent != NULLFSINO && id->ino != parent)
		addparent_inode(id, parent);
}

static int
process_dir_v1(
	xfs_dinode_t	*dip,
	blkmap_t	*blkmap,
	int		*dot,
	int		*dotdot,
	inodata_t	*id,
	xfs_ino_t	*parent)
{
	if (dip->di_core.di_size <= XFS_DFORK_DSIZE(dip, mp) &&
	    dip->di_core.di_format == XFS_DINODE_FMT_LOCAL)
		*parent =
			process_shortform_dir_v1(dip, dot, dotdot, id);
	else if (dip->di_core.di_size == XFS_LBSIZE(mp) &&
		 (dip->di_core.di_format == XFS_DINODE_FMT_EXTENTS ||
		  dip->di_core.di_format == XFS_DINODE_FMT_BTREE))
		*parent = process_leaf_dir_v1(blkmap, dot, dotdot, id);
	else if (dip->di_core.di_size >= XFS_LBSIZE(mp) &&
		  (dip->di_core.di_format == XFS_DINODE_FMT_EXTENTS ||
		   dip->di_core.di_format == XFS_DINODE_FMT_BTREE))
		*parent = process_node_dir_v1(blkmap, dot, dotdot, id);
	else  {
		dbprintf("bad size (%lld) or format (%d) for directory inode "
			 "%lld\n",
			dip->di_core.di_size, (int)dip->di_core.di_format,
			id->ino);
		error++;
		return 1;
	}
	return 0;
}

#if VERS >= V_654
static int
process_dir_v2(
	xfs_dinode_t	*dip,
	blkmap_t	*blkmap,
	int		*dot,
	int		*dotdot,
	inodata_t	*id,
	xfs_ino_t	*parent)
{
	xfs_fileoff_t	last;

	if (blkmap)
		last = blkmap_last_off(blkmap);
	if (dip->di_core.di_size <= XFS_DFORK_DSIZE(dip, mp) &&
	    dip->di_core.di_format == XFS_DINODE_FMT_LOCAL)
		*parent = process_sf_dir_v2(dip, dot, dotdot, id);
	else if (last == mp->m_dirblkfsbs &&
		 (dip->di_core.di_format == XFS_DINODE_FMT_EXTENTS ||
		  dip->di_core.di_format == XFS_DINODE_FMT_BTREE))
		*parent = process_block_dir_v2(blkmap, dot, dotdot, id);
	else if (last >= mp->m_dirleafblk + mp->m_dirblkfsbs &&
		 (dip->di_core.di_format == XFS_DINODE_FMT_EXTENTS ||
		  dip->di_core.di_format == XFS_DINODE_FMT_BTREE))
		*parent = process_leaf_node_dir_v2(blkmap, dot, dotdot, id,
			dip->di_core.di_size);
	else  {
		dbprintf("bad size (%lld) or format (%d) for directory inode "
			 "%lld\n",
			dip->di_core.di_size, (int)dip->di_core.di_format,
			id->ino);
		error++;
		return 1;
	}
	return 0;
}
#endif

/* ARGSUSED */
static void
process_exinode(
	inodata_t		*id,
	xfs_dinode_t		*dip,
	dbm_t			type,
	xfs_drfsbno_t		*totd,
	xfs_drfsbno_t		*toti,
	xfs_extnum_t		*nex,
	blkmap_t		**blkmapp,
	int			whichfork)
{
	xfs_bmbt_rec_32_t	*rp;

	rp = (xfs_bmbt_rec_32_t *)XFS_DFORK_PTR(dip, whichfork);
	*nex = XFS_DFORK_NEXTENTS(dip, whichfork);
	if (*nex < 0 ||
	    *nex >
	    XFS_DFORK_SIZE(dip, mp, whichfork) / sizeof(xfs_bmbt_rec_32_t)) {
		if (!sflag || id->ilist)
			dbprintf("bad number of extents %d for inode %lld\n",
				*nex, id->ino);
		error++;
		return;
	}
	process_bmbt_reclist(rp, *nex, type, id, totd, blkmapp);
}

static void
process_inode(
	xfs_agf_t		*agf,
	xfs_agino_t		agino,
	xfs_dinode_t		*dip,
	int			isfree)
{
	blkmap_t		*blkmap;
	xfs_fsblock_t		bno;
	xfs_dinode_core_t	*dic;
	inodata_t		*id;
	xfs_ino_t		ino;
	xfs_extnum_t		nextents = 0;
	int			nlink;
	int			security;
	xfs_drfsbno_t		totblocks;
	xfs_drfsbno_t		totdblocks = 0;
	xfs_drfsbno_t		totiblocks = 0;
	dbm_t			type;
#if VERS >= V_62
	xfs_extnum_t		anextents = 0;
	xfs_drfsbno_t		atotdblocks = 0;
	xfs_drfsbno_t		atotiblocks = 0;
	xfs_qcnt_t		bc = 0;
	xfs_qcnt_t		ic = 0;
	xfs_qcnt_t		rc = 0;
#endif
	static char		okfmts[] = {
		0,				/* type 0 unused */
		1 << XFS_DINODE_FMT_DEV,	/* FIFO */
		1 << XFS_DINODE_FMT_DEV,	/* CHR */
		0,				/* type 3 unused */
		(1 << XFS_DINODE_FMT_LOCAL) |
		(1 << XFS_DINODE_FMT_EXTENTS) |
		(1 << XFS_DINODE_FMT_BTREE),	/* DIR */
		0,				/* type 5 unused */
		1 << XFS_DINODE_FMT_DEV,	/* BLK */
		0,				/* type 7 unused */
		(1 << XFS_DINODE_FMT_EXTENTS) |
		(1 << XFS_DINODE_FMT_BTREE),	/* REG */
		0,				/* type 9 unused */
		(1 << XFS_DINODE_FMT_LOCAL) |
		(1 << XFS_DINODE_FMT_EXTENTS),	/* LNK */
		0,				/* type 11 unused */
		1 << XFS_DINODE_FMT_DEV,	/* SOCK */
		0,				/* type 13 unused */
		1 << XFS_DINODE_FMT_UUID,	/* MNT */
		0				/* type 15 unused */
	};
	static char		*fmtnames[] = {
		"dev", "local", "extents", "btree", "uuid"
	};

	dic = &dip->di_core;
	ino = XFS_AGINO_TO_INO(mp, agf->agf_seqno, agino);
	if (!isfree) {
		id = find_inode(ino, 1);
		bno = XFS_INO_TO_FSB(mp, ino);
		blkmap = NULL;
	}
	if (dic->di_magic != XFS_DINODE_MAGIC) {
		if (!sflag || isfree || id->ilist || CHECK_BLIST(bno))
			dbprintf("bad magic number %#x for inode %lld\n",
				dic->di_magic, ino);
		error++;
		return;
	}
	if (!XFS_DINODE_GOOD_VERSION(dic->di_version)) {
		if (!sflag || isfree || id->ilist || CHECK_BLIST(bno))
			dbprintf("bad version number %#x for inode %lld\n",
				dic->di_version, ino);
		error++;
		return;
	}
	if (isfree) {
		if (dic->di_nblocks != 0) {
			if (!sflag || id->ilist || CHECK_BLIST(bno))
				dbprintf("bad nblocks %lld for free inode "
					 "%lld\n",
					dic->di_nblocks, ino);
			error++;
		}
#if VERS >= V_62
		if (dic->di_version == XFS_DINODE_VERSION_1)
			nlink = dic->di_onlink;
		else
			nlink = dic->di_nlink;
#else
		nlink = dic->di_nlink;
#endif
		if (nlink != 0) {
			if (!sflag || id->ilist || CHECK_BLIST(bno))
				dbprintf("bad nlink %d for free inode %lld\n",
					nlink, ino);
			error++;
		}
		if (dic->di_mode != 0) {
			if (!sflag || id->ilist || CHECK_BLIST(bno))
				dbprintf("bad mode %#o for free inode %lld\n",
					dic->di_mode, ino);
			error++;
		}
		return;
	}
	/*
	 * di_mode is a 16-bit uint so no need to check the < 0 case
	 */
	if ((((dic->di_mode & IFMT) >> 12) > 15) ||
	    (!(okfmts[(dic->di_mode & IFMT) >> 12] & (1 << dic->di_format)))) {
		if (!sflag || id->ilist || CHECK_BLIST(bno))
			dbprintf("bad format %d for inode %lld type %#o\n",
				dic->di_format, id->ino, dic->di_mode & IFMT);
		error++;
		return;
	}
	if ((unsigned int)XFS_DFORK_ASIZE(dip, mp) >= XFS_LITINO(mp))  {
		if (!sflag || id->ilist)
			dbprintf("bad fork offset %d for inode %lld\n",
				dic->di_forkoff, id->ino);
		error++;
		return;
	}
#if VERS >= V_62
	if ((unsigned int)dic->di_aformat > XFS_DINODE_FMT_BTREE)  {
		if (!sflag || id->ilist)
			dbprintf("bad attribute format %d for inode %lld\n",
				dic->di_aformat, id->ino);
		error++;
		return;
	}
#endif
	if (verbose || id->ilist || CHECK_BLIST(bno))
		dbprintf("inode %lld mode %#o fmt %s "
#if VERS >= V_62
			 "afmt %s "
#endif
			 "nex %d anex %d nblk %lld sz %lld%s%s\n",
			id->ino, dic->di_mode, fmtnames[dic->di_format],
#if VERS >= V_62
			fmtnames[dic->di_aformat],
#endif
			dic->di_nextents,
#if VERS >= V_62
			dic->di_anextents,
#else
			dic->di_nattrextents,
#endif
			dic->di_nblocks, dic->di_size,
			dic->di_flags & XFS_DIFLAG_REALTIME ? " rt" : "",
#if VERS >= V_61
			dic->di_flags & XFS_DIFLAG_PREALLOC ? " pre" : ""
#else
			""
#endif
				);
	security = 0;
	switch (dic->di_mode & IFMT) {
	case IFDIR:
		type = DBM_DIR;
		if (dic->di_format == XFS_DINODE_FMT_LOCAL)
			break;
		blkmap = blkmap_alloc(dic->di_nextents);
		break;
	case IFREG:
		if (dic->di_flags & XFS_DIFLAG_REALTIME)
			type = DBM_RTDATA;
		else if (id->ino == mp->m_sb.sb_rbmino) {
			type = DBM_RTBITMAP;
			blkmap = blkmap_alloc(dic->di_nextents);
			addlink_inode(id);
		} else if (id->ino == mp->m_sb.sb_rsumino) {
			type = DBM_RTSUM;
			blkmap = blkmap_alloc(dic->di_nextents);
			addlink_inode(id);
		}
#if VERS >= V_62
		else if (id->ino == mp->m_sb.sb_uquotino ||
			 id->ino == mp->m_sb.sb_pquotino) {
			type = DBM_QUOTA;
			blkmap = blkmap_alloc(dic->di_nextents);
			addlink_inode(id);
		}
#endif
		else
			type = DBM_DATA;
		if (dic->di_mode & (ISUID | ISGID))
			security = 1;
		break;
	case IFLNK:
		type = DBM_SYMLINK;
		break;
	default:
		security = 1;
		type = DBM_UNKNOWN;
		break;
	}
#if VERS >= V_62
	if (dic->di_version == XFS_DINODE_VERSION_1)
		setlink_inode(id, dic->di_onlink, type == DBM_DIR, security);
	else {
		sbversion |= XFS_SB_VERSION_NLINKBIT;
		setlink_inode(id, dic->di_nlink, type == DBM_DIR, security);
	}
#else
	setlink_inode(id, dic->di_nlink, type == DBM_DIR, security);
#endif
	switch (dic->di_format) {
	case XFS_DINODE_FMT_LOCAL:
		process_lclinode(id, dip, type, &totdblocks, &totiblocks,
			&nextents, &blkmap, XFS_DATA_FORK);
		break;
	case XFS_DINODE_FMT_EXTENTS:
		process_exinode(id, dip, type, &totdblocks, &totiblocks,
			&nextents, &blkmap, XFS_DATA_FORK);
		break;
	case XFS_DINODE_FMT_BTREE:
		process_btinode(id, dip, type, &totdblocks, &totiblocks,
			&nextents, &blkmap, XFS_DATA_FORK);
		break;
	}
#if VERS >= V_62
	if (XFS_DFORK_Q(dip)) {
		sbversion |= XFS_SB_VERSION_ATTRBIT;
		switch (dic->di_aformat) {
		case XFS_DINODE_FMT_LOCAL:
			process_lclinode(id, dip, DBM_ATTR, &atotdblocks,
				&atotiblocks, &anextents, NULL, XFS_ATTR_FORK);
			break;
		case XFS_DINODE_FMT_EXTENTS:
			process_exinode(id, dip, DBM_ATTR, &atotdblocks,
				&atotiblocks, &anextents, NULL, XFS_ATTR_FORK);
			break;
		case XFS_DINODE_FMT_BTREE:
			process_btinode(id, dip, DBM_ATTR, &atotdblocks,
				&atotiblocks, &anextents, NULL, XFS_ATTR_FORK);
			break;
		}
	}
	if (qpdo || qudo) {
		switch (type) {
		case DBM_DATA:
		case DBM_DIR:
		case DBM_RTBITMAP:
		case DBM_RTSUM:
		case DBM_SYMLINK:
		case DBM_UNKNOWN:
			bc = totdblocks + totiblocks +
			     atotdblocks + atotiblocks;
			ic = 1;
			break;
		case DBM_RTDATA:
			bc = totiblocks + atotdblocks + atotiblocks;
			rc = totdblocks;
			ic = 1;
			break;
		case DBM_QUOTA:
			break;
		}
		if (ic)
			quota_add(dic->di_version >= XFS_DINODE_VERSION_2 ?
					dic->di_projid : -1,
				  dic->di_uid, 0, bc, ic, rc);
	}
	totblocks = totdblocks + totiblocks + atotdblocks + atotiblocks;
#else
	totblocks = totdblocks + totiblocks;
#endif
	if (totblocks != dic->di_nblocks) {
		if (!sflag || id->ilist || CHECK_BLIST(bno))
			dbprintf("bad nblocks %lld for inode %lld, counted "
				 "%lld\n",
				dic->di_nblocks, id->ino, totblocks);
		error++;
	}
	if (nextents != dic->di_nextents) {
		if (!sflag || id->ilist || CHECK_BLIST(bno))
			dbprintf("bad nextents %d for inode %lld, counted %d\n",
				dic->di_nextents, id->ino, nextents);
		error++;
	}
#if VERS >= V_62
	if (anextents != dic->di_anextents) {
		if (!sflag || id->ilist || CHECK_BLIST(bno))
			dbprintf("bad anextents %d for inode %lld, counted "
				 "%d\n",
				dic->di_anextents, id->ino, anextents);
		error++;
	}
#endif
	if (type == DBM_DIR)
		process_dir(dip, blkmap, id);
	else if (type == DBM_RTBITMAP)
		process_rtbitmap(blkmap);
	else if (type == DBM_RTSUM)
		process_rtsummary(blkmap);
#if VERS >= V_62
	/*
	 * If the CHKD flag is not set, this can legitimately contain garbage;
	 * xfs_repair may have cleared that bit.
	 */
	else if (type == DBM_QUOTA && (mp->m_sb.sb_qflags & XFS_UQUOTA_CHKD))
		process_quota(id->ino == mp->m_sb.sb_pquotino, id, blkmap);
#endif
	if (blkmap)
		blkmap_free(blkmap);
}

/* ARGSUSED */
static void
process_lclinode(
	inodata_t		*id,
	xfs_dinode_t		*dip,
	dbm_t			type,
	xfs_drfsbno_t		*totd,
	xfs_drfsbno_t		*toti,
	xfs_extnum_t		*nex,
	blkmap_t		**blkmapp,
	int			whichfork)
{
#if VERS >= V_62
	xfs_attr_shortform_t	*asf;
#endif
	xfs_fsblock_t		bno;
	xfs_dinode_core_t	*dic;

	dic = &dip->di_core;
	bno = XFS_INO_TO_FSB(mp, id->ino);
	if (whichfork == XFS_DATA_FORK &&
	    dic->di_size > XFS_DFORK_DSIZE(dip, mp)) {
		if (!sflag || id->ilist || CHECK_BLIST(bno))
			dbprintf("local inode %lld data is too large (size "
				 "%lld)\n",
				id->ino, dic->di_size);
		error++;
	}
#if VERS >= V_62
	else if (whichfork == XFS_ATTR_FORK) {
		asf = (xfs_attr_shortform_t *)XFS_DFORK_PTR(dip, whichfork);
		if (asf->hdr.totsize > XFS_DFORK_ASIZE(dip, mp)) {
			if (!sflag || id->ilist || CHECK_BLIST(bno))
				dbprintf("local inode %lld attr is too large "
					 "(size %d)\n",
					id->ino, asf->hdr.totsize);
			error++;
		}
	}
#endif
}

static xfs_ino_t
process_leaf_dir_v1(
	blkmap_t	*blkmap,
	int		*dot,
	int		*dotdot,
	inodata_t	*id)
{
	xfs_fsblock_t	bno;
	xfs_ino_t	parent;

	bno = blkmap_get(blkmap, 0);
	if (bno == NULLFSBLOCK) {
		if (!sflag || id->ilist)
			dbprintf("block 0 for directory inode %lld is "
				 "missing\n",
				id->ino);
		error++;
		return 0;
	}
	push_cur();
	set_cur(&typtab[TYP_DIR], XFS_FSB_TO_DADDR(mp, bno), blkbb, DB_RING_IGN,
		NULL);
	if (iocur_top->data == NULL) {
		if (!sflag || id->ilist || CHECK_BLIST(bno))
			dbprintf("can't read block 0 for directory inode "
				 "%lld\n",
				id->ino);
		error++;
		return 0;
	}
	parent = process_leaf_dir_v1_int(dot, dotdot, id);
	pop_cur();
	return parent;
}

/* ARGSUSED */
static xfs_ino_t
process_leaf_dir_v1_int(
	int			*dot,
	int			*dotdot,
	inodata_t		*id)
{
	xfs_fsblock_t		bno;
	inodata_t		*cid;
	xfs_dir_leaf_entry_t	*entry;
	int			i;
	xfs_dir_leafblock_t	*leaf;
	xfs_ino_t		lino;
	xfs_dir_leaf_name_t	*namest;
	xfs_ino_t		parent = 0;
	int			v;

	bno = XFS_DADDR_TO_FSB(mp, iocur_top->bb);
	v = verbose || id->ilist || CHECK_BLIST(bno);
	leaf = iocur_top->data;
	if (leaf->hdr.info.magic != XFS_DIR_LEAF_MAGIC) {
		if (!sflag || id->ilist || CHECK_BLIST(bno))
			dbprintf("bad directory leaf magic # %#x for dir ino "
				 "%lld\n",
				leaf->hdr.info.magic, id->ino);
		error++;
		return NULLFSINO;
	}
#if VERS >= V_62
	entry = &leaf->entries[0];
#else
	entry = &leaf->leaves[0];
#endif
	for (i = 0; i < leaf->hdr.count; entry++, i++) {
		namest = XFS_DIR_LEAF_NAMESTRUCT(leaf, entry->nameidx);
		memcpy((char *)&lino, (char *)&namest->inumber, sizeof(lino));
		cid = find_inode(lino, 1);
		if (v)
			dbprintf("dir %lld entry %*.*s %lld\n", id->ino,
				entry->namelen, entry->namelen, namest->name,
				lino);
		if (cid)
			addlink_inode(cid);
		else {
			if (!sflag)
				dbprintf("dir %lld entry %*.*s bad inode "
					 "number %lld\n",
					id->ino, entry->namelen, entry->namelen,
					namest->name, lino);
			error++;
		}
		if (entry->namelen == 2 && namest->name[0] == '.' &&
		    namest->name[1] == '.') {
			if (parent) {
				if (!sflag || id->ilist || CHECK_BLIST(bno))
					dbprintf("multiple .. entries in dir "
						 "%lld (%lld, %lld)\n",
						id->ino, parent, lino);
				error++;
			} else
				parent = cid ? lino : NULLFSINO;
			(*dotdot)++;
		} else if (entry->namelen != 1 || namest->name[0] != '.') {
			if (cid != NULL) {
				if (!cid->parent)
					cid->parent = id;
				addname_inode(cid, (char *)namest->name,
					entry->namelen);
			}
		} else {
			if (lino != id->ino) {
				if (!sflag)
					dbprintf("dir %lld entry . inode "
						 "number mismatch (%lld)\n",
						id->ino, lino);
				error++;
			}
			(*dot)++;
		}
	}
	return parent;
}

#if VERS >= V_654
/* ARGSUSED */
static xfs_ino_t
process_leaf_node_dir_v2(
	blkmap_t		*blkmap,
	int			*dot,
	int			*dotdot,
	inodata_t		*id,
	xfs_fsize_t		dirsize)
{
	xfs_fsblock_t		b;
	bbmap_t			bbmap;
	bmap_ext_t		*bmp;
	xfs_fileoff_t		dbno;
	freetab_t		*freetab;
	int			i;
	xfs_ino_t		lino;
	int			nex;
	xfs_ino_t		parent;
	int			t;
	int			v;
	int			v2;
	int			x;

	v2 = verbose || id->ilist;
	parent = 0;
	dbno = NULLFILEOFF;
	freetab = malloc(FREETAB_SIZE(dirsize / mp->m_dirblksize));
	freetab->naents = (int)(dirsize / mp->m_dirblksize);
	freetab->nents = 0;
	for (i = 0; i < freetab->naents; i++)
		freetab->ents[i] = NULLDATAOFF;
	dir_hash_init();
	while ((dbno = blkmap_next_off(blkmap, dbno, &t)) != NULLFILEOFF) {
		nex = blkmap_getn(blkmap, dbno, mp->m_dirblkfsbs, &bmp);
		assert(nex > 0);
		for (v = v2, x = 0; !v && x < nex; x++) {
			for (b = bmp[x].startblock;
			     !v && b < bmp[x].startblock + bmp[x].blockcount;
			     b++)
				v = CHECK_BLIST(b);
		}
		if (v)
			dbprintf("dir inode %lld block %u=%llu\n", id->ino,
				(__uint32_t)dbno,
				(xfs_dfsbno_t)bmp->startblock);
		push_cur();
		if (nex > 1)
			make_bbmap(&bbmap, nex, bmp);
		set_cur(&typtab[TYP_DIR], XFS_FSB_TO_DADDR(mp, bmp->startblock),
			mp->m_dirblkfsbs * blkbb, DB_RING_IGN,
			nex > 1 ? &bbmap : NULL);
		free(bmp);
		if (iocur_top->data == NULL) {
			if (!sflag || v)
				dbprintf("can't read block %u for directory "
					 "inode %lld\n",
					(__uint32_t)dbno, id->ino);
			error++;
			pop_cur();
			dbno += mp->m_dirblkfsbs - 1;
			continue;
		}
		if (dbno < mp->m_dirleafblk) {
			lino = process_data_dir_v2(dot, dotdot, id, v,
				(xfs_dablk_t)dbno, &freetab);
			if (lino) {
				if (parent) {
					if (!sflag || v)
						dbprintf("multiple .. entries "
							 "in dir %lld\n",
							id->ino);
					error++;
				} else
					parent = lino;
			}
		} else if (dbno < mp->m_dirfreeblk) {
			process_leaf_node_dir_v2_int(id, v, (xfs_dablk_t)dbno,
				freetab);
		} else {
			process_leaf_node_dir_v2_free(id, v, (xfs_dablk_t)dbno,
				freetab);
		}
		pop_cur();
		dbno += mp->m_dirblkfsbs - 1;
	}
	dir_hash_check(id, v);
	dir_hash_done();
	for (i = 0; i < freetab->nents; i++) {
		if (freetab->ents[i] != NULLDATAOFF) {
			if (!sflag || v)
				dbprintf("missing free index for data block %d "
					 "in dir ino %lld\n",
					XFS_DIR2_DB_TO_DA(mp, i), id->ino);
			error++;
		}
	}
	free(freetab);
	return parent;
}

static void
process_leaf_node_dir_v2_free(
	inodata_t		*id,
	int			v,
	xfs_dablk_t		dabno,
	freetab_t		*freetab)
{
	xfs_dir2_data_off_t	ent;
	xfs_dir2_free_t		*free;
	int			i;
	int			maxent;
	int			used;

	free = iocur_top->data;
	if (free->hdr.magic != XFS_DIR2_FREE_MAGIC) {
		if (!sflag || v)
			dbprintf("bad free block magic # %#x for dir ino %lld "
				 "block %d\n",
				free->hdr.magic, id->ino, dabno);
		error++;
		return;
	}
	maxent = XFS_DIR2_MAX_FREE_BESTS(mp);
	if (free->hdr.firstdb !=
	    XFS_DIR2_DA_TO_DB(mp, dabno - mp->m_dirfreeblk) * maxent) {
		if (!sflag || v)
			dbprintf("bad free block firstdb %d for dir ino %lld "
				 "block %d\n",
				free->hdr.firstdb, id->ino, dabno);
		error++;
		return;
	}
	if (free->hdr.nvalid > maxent || free->hdr.nvalid < 0 ||
	    free->hdr.nused > maxent || free->hdr.nused < 0 ||
	    free->hdr.nused > free->hdr.nvalid) {
		if (!sflag || v)
			dbprintf("bad free block nvalid/nused %d/%d for dir "
				 "ino %lld block %d\n",
				free->hdr.nvalid, free->hdr.nused, id->ino,
				dabno);
		error++;
		return;
	}
	for (used = i = 0; i < free->hdr.nvalid; i++) {
		if (freetab->nents <= free->hdr.firstdb + i)
			ent = NULLDATAOFF;
		else
			ent = freetab->ents[free->hdr.firstdb + i];
		if (ent != free->bests[i]) {
			if (!sflag || v)
				dbprintf("bad free block ent %d is %d should "
					 "be %d for dir ino %lld block %d\n",
					i, free->bests[i], ent, id->ino, dabno);
			error++;
		}
		if (free->bests[i] != NULLDATAOFF)
			used++;
		if (ent != NULLDATAOFF)
			freetab->ents[free->hdr.firstdb + i] = NULLDATAOFF;
	}
	if (used != free->hdr.nused) {
		if (!sflag || v)
			dbprintf("bad free block nused %d should be %d for dir "
				 "ino %lld block %d\n",
				free->hdr.nused, used, id->ino, dabno);
		error++;
	}
}

static void
process_leaf_node_dir_v2_int(
	inodata_t		*id,
	int			v,
	xfs_dablk_t		dabno,
	freetab_t		*freetab)
{
	int			i;
	xfs_dir2_data_off_t	*lbp;
	xfs_dir2_leaf_t		*leaf;
	xfs_dir2_leaf_entry_t	*lep;
	xfs_dir2_leaf_tail_t	*ltp;
	xfs_da_intnode_t	*node;
	int			stale;

	leaf = iocur_top->data;
	switch (leaf->hdr.info.magic) {
	case XFS_DIR2_LEAF1_MAGIC:
		if (leaf->hdr.info.forw || leaf->hdr.info.back) {
			if (!sflag || v)
				dbprintf("bad leaf block forw/back pointers "
					 "%d/%d for dir ino %lld block %d\n",
					leaf->hdr.info.forw,
					leaf->hdr.info.back, id->ino, dabno);
			error++;
		}
		if (dabno != mp->m_dirleafblk) {
			if (!sflag || v)
				dbprintf("single leaf block for dir ino %lld "
					 "block %d should be at block %d\n",
					id->ino, dabno,
					(xfs_dablk_t)mp->m_dirleafblk);
			error++;
		}
		ltp = XFS_DIR2_LEAF_TAIL_P(mp, leaf);
		lbp = XFS_DIR2_LEAF_BESTS_P(ltp);
		for (i = 0; i < ltp->bestcount; i++) {
			if (freetab->nents <= i || freetab->ents[i] != lbp[i]) {
				if (!sflag || v)
					dbprintf("bestfree %d for dir ino %lld "
						 "block %d doesn't match table "
						 "value %d\n",
						freetab->nents <= i ?
							NULLDATAOFF :
							freetab->ents[i],
						id->ino,
						XFS_DIR2_DB_TO_DA(mp, i),
						lbp[i]);
			}
			if (freetab->nents > i)
				freetab->ents[i] = NULLDATAOFF;
		}
		break;
	case XFS_DIR2_LEAFN_MAGIC:
		/* if it's at the root location then we can check the 
		 * pointers are null XXX */
		break;
	case XFS_DA_NODE_MAGIC:
		node = iocur_top->data;
		if (node->hdr.level < 1 ||
		    node->hdr.level > XFS_DA_NODE_MAXDEPTH) {
			if (!sflag || v)
				dbprintf("bad node block level %d for dir ino "
					 "%lld block %d\n",
					node->hdr.level, id->ino, dabno);
			error++;
		}
		return;
	default:
		if (!sflag || v)
			dbprintf("bad directory data magic # %#x for dir ino "
				 "%lld block %d\n",
				leaf->hdr.info.magic, id->ino, dabno);
		error++;
		return;
	}
	lep = leaf->ents;
	for (i = stale = 0; i < leaf->hdr.count; i++) {
		if (lep[i].address == XFS_DIR2_NULL_DATAPTR)
			stale++;
		else if (dir_hash_see(lep[i].hashval, lep[i].address)) {
			if (!sflag || v)
				dbprintf("dir %lld block %d extra leaf entry "
					 "%x %x\n",
					id->ino, dabno, lep[i].hashval,
					lep[i].address);
			error++;
		}
	}
	if (stale != leaf->hdr.stale) {
		if (!sflag || v)
			dbprintf("dir %lld block %d stale mismatch "
				 "%d/%d\n",
				 id->ino, dabno, stale,
				 leaf->hdr.stale);
		error++;
	}
}
#endif

static xfs_ino_t
process_node_dir_v1(
	blkmap_t		*blkmap,
	int			*dot,
	int			*dotdot,
	inodata_t		*id)
{
	xfs_fsblock_t		bno;
	xfs_fileoff_t		dbno;
	xfs_ino_t		lino;
#if VERS >= V_62
	xfs_da_intnode_t	*node;
#else
	xfs_dir_intnode_t	*node;
#endif
	xfs_ino_t		parent;
	int			t;
	int			v;
	int			v2;

	v = verbose || id->ilist;
	parent = 0;
	dbno = NULLFILEOFF;
	while ((dbno = blkmap_next_off(blkmap, dbno, &t)) != NULLFILEOFF) {
		bno = blkmap_get(blkmap, dbno);
		v2 = bno != NULLFSBLOCK && CHECK_BLIST(bno);
		if (bno == NULLFSBLOCK && dbno == 0) {
			if (!sflag || v)
				dbprintf("can't read root block for directory "
					 "inode %lld\n",
					id->ino);
			error++;
		}
		if (v || v2)
			dbprintf("dir inode %lld block %u=%llu\n", id->ino,
				(__uint32_t)dbno, (xfs_dfsbno_t)bno);
		if (bno == NULLFSBLOCK)
			continue;
		push_cur();
		set_cur(&typtab[TYP_DIR], XFS_FSB_TO_DADDR(mp, bno), blkbb,
			DB_RING_IGN, NULL);
		if ((node = iocur_top->data) == NULL) {
			if (!sflag || v || v2)
				dbprintf("can't read block %u for directory "
					 "inode %lld\n",
					(__uint32_t)dbno, id->ino);
			error++;
			continue;
		}
#if VERS >= V_62
		if (node->hdr.info.magic == XFS_DA_NODE_MAGIC)
#else
		if (node->hdr.info.magic == XFS_DIR_NODE_MAGIC)
#endif
		{
			pop_cur();
			continue;
		}
		lino = process_leaf_dir_v1_int(dot, dotdot, id);
		if (lino) {
			if (parent) {
				if (!sflag || v || v2)
					dbprintf("multiple .. entries in dir "
						 "%lld\n",
						id->ino);
				error++;
			} else
				parent = lino;
		}
		pop_cur();
	}
	return parent;
}

#if VERS >= V_62
static void
process_quota(
	int		isproj,
	inodata_t	*id,
	blkmap_t	*blkmap)
{
	xfs_fsblock_t	bno;
	int		cb;
	xfs_dqblk_t	*dqb;
	xfs_dqid_t	dqid;
	u_int8_t	exp_flags;
	int		i;
	int		perblock;
	xfs_fileoff_t	qbno;
	char		*s;
	int		scicb;
	int		t;

	perblock = (int)(mp->m_sb.sb_blocksize / sizeof(*dqb));
	s = isproj ? "project" : "user";
	exp_flags = isproj ? XFS_DQ_PROJ : XFS_DQ_USER;
	dqid = 0;
	qbno = NULLFILEOFF;
	while ((qbno = blkmap_next_off(blkmap, qbno, &t)) !=
	       NULLFILEOFF) {
		bno = blkmap_get(blkmap, qbno);
		dqid = (xfs_dqid_t)qbno * perblock;
		cb = CHECK_BLIST(bno);
		scicb = !sflag || id->ilist || cb;
		push_cur();
		set_cur(&typtab[TYP_DQBLK], XFS_FSB_TO_DADDR(mp, bno), blkbb,
			DB_RING_IGN, NULL);
		if ((dqb = iocur_top->data) == NULL) {
			pop_cur();
			if (scicb)
				dbprintf("can't read block %lld for %s quota "	
					 "inode (fsblock %lld)\n",
					(xfs_dfiloff_t)qbno, s,
					(xfs_dfsbno_t)bno);
			error++;
			continue;
		}
		for (i = 0; i < perblock; i++, dqid++, dqb++) {
			if (verbose || id->ilist || cb)
				dbprintf("%s dqblk %lld entry %d id %d bc "
					 "%lld ic %lld rc %lld\n",
					s, (xfs_dfiloff_t)qbno, i, dqid,
					dqb->dd_diskdq.d_bcount,
					dqb->dd_diskdq.d_icount,
					dqb->dd_diskdq.d_rtbcount);
			if (dqb->dd_diskdq.d_magic != XFS_DQUOT_MAGIC) {
				if (scicb)
					dbprintf("bad magic number %#x for %s "	
						 "dqblk %lld entry %d id %d\n",
						dqb->dd_diskdq.d_magic, s,
						(xfs_dfiloff_t)qbno, i, dqid);
				error++;
				continue;
			}
			if (dqb->dd_diskdq.d_version != XFS_DQUOT_VERSION) {
				if (scicb)
					dbprintf("bad version number %#x for "
						 "%s dqblk %lld entry %d id "
						 "%d\n",
						dqb->dd_diskdq.d_version, s,
						(xfs_dfiloff_t)qbno, i, dqid);
				error++;
				continue;
			}
			if (dqb->dd_diskdq.d_flags != exp_flags) {
				if (scicb)
					dbprintf("bad flags %#x for %s dqblk "
						 "%lld entry %d id %d\n",
						dqb->dd_diskdq.d_flags, s,
						(xfs_dfiloff_t)qbno, i, dqid);
				error++;
				continue;
			}
			if (dqb->dd_diskdq.d_id != dqid) {
				if (scicb)
					dbprintf("bad id %d for %s dqblk %lld "
						 "entry %d id %d\n",
						dqb->dd_diskdq.d_id, s,
						(xfs_dfiloff_t)qbno, i, dqid);
				error++;
				continue;
			}
			quota_add(isproj ? dqid : -1, isproj ? -1 : dqid, 1,
				  dqb->dd_diskdq.d_bcount,
				  dqb->dd_diskdq.d_icount,
				  dqb->dd_diskdq.d_rtbcount);
		}
		pop_cur();
	}
}
#endif

static void
process_rtbitmap(
	blkmap_t	*blkmap)
{
	int		bit;
	int		bitsperblock;
	xfs_fileoff_t	bmbno;
	xfs_fsblock_t	bno;
	xfs_drtbno_t	extno;
	int		len;
	int		log;
	int		offs;
	int		prevbit;
	xfs_drfsbno_t	rtbno;
	int		start_bmbno;
	int		start_bit;
	int		t;
	xfs_rtword_t	*words;

	bitsperblock = mp->m_sb.sb_blocksize * NBBY;
	prevbit = 0;
	extno = 0;
	bmbno = NULLFILEOFF;
	while ((bmbno = blkmap_next_off(blkmap, bmbno, &t)) !=
	       NULLFILEOFF) {
		bno = blkmap_get(blkmap, bmbno);
		if (bno == NULLFSBLOCK) {
			if (!sflag)
				dbprintf("block %lld for rtbitmap inode is "
					 "missing\n",
					(xfs_dfiloff_t)bmbno);
			error++;
			continue;
		}
		push_cur();
		set_cur(&typtab[TYP_RTBITMAP], XFS_FSB_TO_DADDR(mp, bno), blkbb,
			DB_RING_IGN, NULL);
		if ((words = iocur_top->data) == NULL) {
			pop_cur();
			if (!sflag)
				dbprintf("can't read block %lld for rtbitmap "
					 "inode\n",
					(xfs_dfiloff_t)bmbno);
			error++;
			continue;
		}
		for (bit = 0;
		     bit < bitsperblock && extno < mp->m_sb.sb_rextents;
		     bit++, extno++) {
			if (isset(words, bit)) {
				rtbno = extno * mp->m_sb.sb_rextsize;
				set_rdbmap(rtbno, mp->m_sb.sb_rextsize,
					DBM_RTFREE);
				frextents++;
				if (prevbit == 0) {
					start_bmbno = (int)bmbno;
					start_bit = bit;
					prevbit = 1;
				}
			} else if (prevbit == 1) {
				len = ((int)bmbno - start_bmbno) *
					bitsperblock + (bit - start_bit);
				log = XFS_RTBLOCKLOG(len);
				offs = XFS_SUMOFFS(mp, log, start_bmbno);
				sumcompute[offs]++;
				prevbit = 0;
			}
		}
		pop_cur();
		if (extno == mp->m_sb.sb_rextents)
			break;
	}
	if (prevbit == 1) {
		len = ((int)bmbno - start_bmbno) * bitsperblock +
			(bit - start_bit);
		log = XFS_RTBLOCKLOG(len);
		offs = XFS_SUMOFFS(mp, log, start_bmbno);
		sumcompute[offs]++;
	}
}

static void
process_rtsummary(
	blkmap_t	*blkmap)
{
	xfs_fsblock_t	bno;
	char		*bytes;
	xfs_fileoff_t	sumbno;
	int		t;

	sumbno = NULLFILEOFF;
	while ((sumbno = blkmap_next_off(blkmap, sumbno, &t)) !=
	       NULLFILEOFF) {
		bno = blkmap_get(blkmap, sumbno);
		if (bno == NULLFSBLOCK) {
			if (!sflag)
				dbprintf("block %lld for rtsummary inode is "
					 "missing\n",
					(xfs_dfiloff_t)sumbno);
			error++;
			continue;
		}
		push_cur();
		set_cur(&typtab[TYP_RTSUMMARY], XFS_FSB_TO_DADDR(mp, bno),
			blkbb, DB_RING_IGN, NULL);
		if ((bytes = iocur_top->data) == NULL) {
			if (!sflag)
				dbprintf("can't read block %lld for rtsummary "
					 "inode\n",
					(xfs_dfiloff_t)sumbno);
			error++;
			continue;
		}
		memcpy((char *)sumfile + sumbno * mp->m_sb.sb_blocksize, bytes,
			mp->m_sb.sb_blocksize);
		pop_cur();
	}
}

#if VERS >= V_654
/* ARGSUSED */
static xfs_ino_t
process_sf_dir_v2(
	xfs_dinode_t		*dip,
	int			*dot,
	int			*dotdot,
	inodata_t		*id)
{
	inodata_t		*cid;
	int			i;
	int			i8;
	xfs_ino_t		lino;
	int			offset;
	xfs_dir2_sf_t		*sf;
	xfs_dir2_sf_entry_t	*sfe;
	int			v;

	sf = &dip->di_u.di_dir2sf;
	addlink_inode(id);
	v = verbose || id->ilist;
	if (v)
		dbprintf("dir %lld entry . %lld\n", id->ino, id->ino);
	(*dot)++;
	sfe = XFS_DIR2_SF_FIRSTENTRY(sf);
	offset = XFS_DIR2_DATA_FIRST_OFFSET;
	for (i = sf->hdr.count - 1, i8 = 0; i >= 0; i--) {
		if ((__psint_t)sfe + XFS_DIR2_SF_ENTSIZE_BYENTRY(sf, sfe) -
		    (__psint_t)sf > dip->di_core.di_size) {
			if (!sflag)
				dbprintf("dir %llu bad size in entry at %d\n",
					id->ino,
					(int)((char *)sfe - (char *)sf));
			error++;
			break;
		}
		lino = XFS_DIR2_SF_GET_INUMBER(sf, XFS_DIR2_SF_INUMBERP(sfe));
		if (lino > XFS_DIR2_MAX_SHORT_INUM)
			i8++;
		cid = find_inode(lino, 1);
		if (cid == NULL) {
			if (!sflag)
				dbprintf("dir %lld entry %*.*s bad inode "
					 "number %lld\n",
					id->ino, sfe->namelen, sfe->namelen,
					sfe->name, lino);
			error++;
		} else {
			addlink_inode(cid);
			if (!cid->parent)
				cid->parent = id;
			addname_inode(cid, (char *)sfe->name, sfe->namelen);
		}
		if (v)
			dbprintf("dir %lld entry %*.*s offset %d %lld\n",
				id->ino, sfe->namelen, sfe->namelen, sfe->name,
				XFS_DIR2_SF_GET_OFFSET(sfe), lino);
		if (XFS_DIR2_SF_GET_OFFSET(sfe) < offset) {
			if (!sflag)
				dbprintf("dir %lld entry %*.*s bad offset %d\n",
					id->ino, sfe->namelen, sfe->namelen,
					sfe->name, XFS_DIR2_SF_GET_OFFSET(sfe));
			error++;
		}
		offset =
			XFS_DIR2_SF_GET_OFFSET(sfe) +
			XFS_DIR2_DATA_ENTSIZE(sfe->namelen);
		sfe = XFS_DIR2_SF_NEXTENTRY(sf, sfe);
	}
	if (i < 0 && (__psint_t)sfe - (__psint_t)sf != dip->di_core.di_size) {
		if (!sflag)
			dbprintf("dir %llu size is %lld, should be %u\n",
				id->ino, dip->di_core.di_size,
				(uint)((char *)sfe - (char *)sf));
		error++;
	}
	if (offset + (sf->hdr.count + 2) * sizeof(xfs_dir2_leaf_entry_t) +
	    sizeof(xfs_dir2_block_tail_t) > mp->m_dirblksize) {
		if (!sflag)
			dbprintf("dir %llu offsets too high\n", id->ino);
		error++;
	}
	lino = XFS_DIR2_SF_GET_INUMBER(sf, &sf->hdr.parent);
	if (lino > XFS_DIR2_MAX_SHORT_INUM)
		i8++;
	cid = find_inode(lino, 1);
	if (cid)
		addlink_inode(cid);
	else {
		if (!sflag)
			dbprintf("dir %lld entry .. bad inode number %lld\n",
				id->ino, lino);
		error++;
	}
	if (v)
		dbprintf("dir %lld entry .. %lld\n", id->ino, lino);
	if (i8 != sf->hdr.i8count) {
		if (!sflag)
			dbprintf("dir %lld i8count mismatch is %d should be "
				 "%d\n",
				id->ino, sf->hdr.i8count, i8);
		error++;
	}
	(*dotdot)++;
	return cid ? lino : NULLFSINO;
}
#endif

/* ARGSUSED */
static xfs_ino_t
process_shortform_dir_v1(
	xfs_dinode_t		*dip,
	int			*dot,
	int			*dotdot,
	inodata_t		*id)
{
	inodata_t		*cid;
	int			i;
	xfs_ino_t		lino;
	xfs_dir_shortform_t	*sf;
	xfs_dir_sf_entry_t	*sfe;
	int			v;

	sf = &dip->di_u.di_dirsf;
	addlink_inode(id);
	v = verbose || id->ilist;
	if (v)
		dbprintf("dir %lld entry . %lld\n", id->ino, id->ino);
	(*dot)++;
	sfe = &sf->list[0];
	for (i = sf->hdr.count - 1; i >= 0; i--) {
		memcpy((char *)&lino, (char *)&sfe->inumber, sizeof(lino));
		cid = find_inode(lino, 1);
		if (cid == NULL) {
			if (!sflag)
				dbprintf("dir %lld entry %*.*s bad inode "
					 "number %lld\n",
					id->ino, sfe->namelen, sfe->namelen,
					sfe->name, lino);
			error++;
		} else {
			addlink_inode(cid);
			if (!cid->parent)
				cid->parent = id;
			addname_inode(cid, (char *)sfe->name, sfe->namelen);
		}
		if (v)
			dbprintf("dir %lld entry %*.*s %lld\n", id->ino,
				sfe->namelen, sfe->namelen, sfe->name, lino);
		sfe = XFS_DIR_SF_NEXTENTRY(sfe);
	}
	if ((__psint_t)sfe - (__psint_t)sf != dip->di_core.di_size)
		dbprintf("dir %llu size is %lld, should be %d\n",
			id->ino, dip->di_core.di_size,
			(int)((char *)sfe - (char *)sf));
	memcpy((char *)&lino, (char *)&sf->hdr.parent, sizeof(lino));
	cid = find_inode(lino, 1);
	if (cid)
		addlink_inode(cid);
	else {
		if (!sflag)
			dbprintf("dir %lld entry .. bad inode number %lld\n",
				id->ino, lino);
		error++;
	}
	if (v)
		dbprintf("dir %lld entry .. %lld\n", id->ino, lino);
	(*dotdot)++;
	return cid ? lino : NULLFSINO;
}

#if VERS >= V_62
static void
quota_add(
	xfs_dqid_t	projid,
	xfs_dqid_t	userid,
	int		dq,
	xfs_qcnt_t	bc,
	xfs_qcnt_t	ic,
	xfs_qcnt_t	rc)
{
	if (qudo && userid != -1)
		quota_add1(qudata, userid, dq, bc, ic, rc);
	if (qpdo && projid != -1)
		quota_add1(qpdata, projid, dq, bc, ic, rc);
}

static void
quota_add1(
	qdata_t		**qt,
	xfs_dqid_t	id,
	int		dq,
	xfs_qcnt_t	bc,
	xfs_qcnt_t	ic,
	xfs_qcnt_t	rc)
{
	qdata_t		*qe;
	int		qh;
	qinfo_t		*qi;

	qh = (int)((__uint32_t)id % QDATA_HASH_SIZE);
	qe = qt[qh];
	while (qe) {
		if (qe->id == id) {
			qi = dq ? &qe->dq : &qe->count;
			qi->bc += bc;
			qi->ic += ic;
			qi->rc += rc;
			return;
		}
		qe = qe->next;
	}
	qe = xmalloc(sizeof(*qe));
	qe->id = id;
	qi = dq ? &qe->dq : &qe->count;
	qi->bc = bc;
	qi->ic = ic;
	qi->rc = rc;
	qi = dq ? &qe->count : &qe->dq;
	qi->bc = qi->ic = qi->rc = 0;
	qe->next = qt[qh];
	qt[qh] = qe;
}

static void
quota_check(
	char	*s,
	qdata_t	**qt)
{
	int	i;
	qdata_t	*next;
	qdata_t	*qp;

	for (i = 0; i < QDATA_HASH_SIZE; i++) {
		qp = qt[i];
		while (qp) {
			next = qp->next;
			if (qp->count.bc != qp->dq.bc ||
			    qp->count.ic != qp->dq.ic ||
			    qp->count.rc != qp->dq.rc) {
				if (!sflag) {
					dbprintf("%s quota id %d, have/exp",
						s, qp->id);
					if (qp->count.bc != qp->dq.bc)
						dbprintf(" bc %lld/%lld",
							qp->dq.bc,
							qp->count.bc);
					if (qp->count.ic != qp->dq.ic)
						dbprintf(" ic %lld/%lld",
							qp->dq.ic,
							qp->count.ic);
					if (qp->count.rc != qp->dq.rc)
						dbprintf(" rc %lld/%lld",
							qp->dq.rc,
							qp->count.rc);
					dbprintf("\n");
				}
				error++;
			}
			xfree(qp);
			qp = next;
		}
	}
	xfree(qt);
}

static void
quota_init(void)
{
	qudo = mp->m_sb.sb_uquotino != 0 &&
	       mp->m_sb.sb_uquotino != NULLFSINO &&
	       (mp->m_sb.sb_qflags & XFS_UQUOTA_CHKD);
	qpdo = mp->m_sb.sb_pquotino != 0 &&
	       mp->m_sb.sb_pquotino != NULLFSINO &&
	       (mp->m_sb.sb_qflags & XFS_PQUOTA_CHKD);
	if (qudo)
		qudata = xcalloc(QDATA_HASH_SIZE, sizeof(qdata_t *));
	if (qpdo)
		qpdata = xcalloc(QDATA_HASH_SIZE, sizeof(qdata_t *));
}
#endif

static void
scan_ag(
	xfs_agnumber_t	agno)
{
	xfs_agf_t	*agf;
	xfs_agi_t	*agi;
	int		i;
	xfs_sb_t	*sb;

	agffreeblks = agflongest = 0;
	agicount = agifreecount = 0;
	push_cur();
	set_cur(&typtab[TYP_SB], XFS_AG_DADDR(mp, agno, XFS_SB_DADDR), 1,
		DB_RING_IGN, NULL);
	if ((sb = iocur_top->data) == NULL) {
		dbprintf("can't read superblock for ag %u\n", agno);
		pop_cur();
		serious_error++;
		return;
	}
	if (sb->sb_magicnum != XFS_SB_MAGIC) {
		if (!sflag)
			dbprintf("bad sb magic # %#x in ag %u\n",
				sb->sb_magicnum, agno);
		error++;
	}
#if VERS >= V_62
	if (!XFS_SB_GOOD_VERSION(sb))
#else
	if (!XFS_SB_GOOD_VERSION(sb->sb_versionnum))
#endif
	{
		if (!sflag)
			dbprintf("bad sb version # %#x in ag %u\n",
				sb->sb_versionnum, agno);
		error++;
		sbver_err++;
	}
	if (agno == 0 && sb->sb_inprogress != 0) {
		if (!sflag)
			dbprintf("mkfs not completed successfully\n");
		error++;
	}
	set_dbmap(agno, XFS_SB_BLOCK(mp), 1, DBM_SB, agno, XFS_SB_BLOCK(mp));
	if (sb->sb_logstart && XFS_FSB_TO_AGNO(mp, sb->sb_logstart) == agno)
		set_dbmap(agno, XFS_FSB_TO_AGBNO(mp, sb->sb_logstart),
			sb->sb_logblocks, DBM_LOG, agno, XFS_SB_BLOCK(mp));
	push_cur();
	set_cur(&typtab[TYP_AGF], XFS_AG_DADDR(mp, agno, XFS_AGF_DADDR), 1,
		DB_RING_IGN, NULL);
	if ((agf = iocur_top->data) == NULL) {
		dbprintf("can't read agf block for ag %u\n", agno);
		pop_cur();
		pop_cur();
		serious_error++;
		return;
	}
	if (agf->agf_magicnum != XFS_AGF_MAGIC) {
		if (!sflag)
			dbprintf("bad agf magic # %#x in ag %u\n",
				agf->agf_magicnum, agno);
		error++;
	}
	if (!XFS_AGF_GOOD_VERSION(agf->agf_versionnum)) {
		if (!sflag)
			dbprintf("bad agf version # %#x in ag %u\n",
				agf->agf_versionnum, agno);
		error++;
	}
	if (XFS_SB_BLOCK(mp) != XFS_AGF_BLOCK(mp))
		set_dbmap(agno, XFS_AGF_BLOCK(mp), 1, DBM_AGF, agno,
			XFS_SB_BLOCK(mp));
	if (sb->sb_agblocks > agf->agf_length)
		set_dbmap(agno, agf->agf_length,
			sb->sb_agblocks - agf->agf_length, DBM_MISSING,
			agno, XFS_SB_BLOCK(mp));
	push_cur();
	set_cur(&typtab[TYP_AGI], XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR), 1,
		DB_RING_IGN, NULL);
	if ((agi = iocur_top->data) == NULL) {
		dbprintf("can't read agi block for ag %u\n", agno);
		serious_error++;
		pop_cur();
		pop_cur();
		pop_cur();
		return;
	}
	if (agi->agi_magicnum != XFS_AGI_MAGIC) {
		if (!sflag)
			dbprintf("bad agi magic # %#x in ag %u\n",
				agi->agi_magicnum, agno);
		error++;
	}
	if (!XFS_AGI_GOOD_VERSION(agi->agi_versionnum)) {
		if (!sflag)
			dbprintf("bad agi version # %#x in ag %u\n",
				agi->agi_versionnum, agno);
		error++;
	}
	if (XFS_SB_BLOCK(mp) != XFS_AGI_BLOCK(mp) &&
	    XFS_AGF_BLOCK(mp) != XFS_AGI_BLOCK(mp))
		set_dbmap(agno, XFS_AGI_BLOCK(mp), 1, DBM_AGI, agno,
			XFS_SB_BLOCK(mp));
	scan_freelist(agf);
	fdblocks--;
	scan_sbtree(agf, agf->agf_roots[XFS_BTNUM_BNO],
		agf->agf_levels[XFS_BTNUM_BNO], 1, scanfunc_bno, TYP_BNOBT);
	fdblocks--;
	scan_sbtree(agf, agf->agf_roots[XFS_BTNUM_CNT],
		agf->agf_levels[XFS_BTNUM_CNT], 1, scanfunc_cnt, TYP_CNTBT);
	scan_sbtree(agf, agi->agi_root, agi->agi_level, 1, scanfunc_ino,
		TYP_INOBT);
	if (agf->agf_freeblks != agffreeblks) {
		if (!sflag)
			dbprintf("agf_freeblks %u, counted %u in ag %u\n",
				agf->agf_freeblks, agffreeblks, agno);
		error++;
	}
	if (agf->agf_longest != agflongest) {
		if (!sflag)
			dbprintf("agf_longest %u, counted %u in ag %u\n",
				agf->agf_longest, agflongest, agno);
		error++;
	}
	if (agi->agi_count != agicount) {
		if (!sflag)
			dbprintf("agi_count %u, counted %u in ag %u\n",
				agi->agi_count, agicount, agno);
		error++;
	}
	if (agi->agi_freecount != agifreecount) {
		if (!sflag)
			dbprintf("agi_freecount %u, counted %u in ag %u\n",
				agi->agi_freecount, agifreecount, agno);
		error++;
	}
	for (i = 0; i < XFS_AGI_UNLINKED_BUCKETS; i++) {
		if (agi->agi_unlinked[i] != NULLAGINO) {
			if (!sflag)
				dbprintf("agi unlinked bucket %d is %u in ag "
					 "%u\n",
					i, agi->agi_unlinked[i], agno);
			error++;
		}
	}
	pop_cur();
	pop_cur();
	pop_cur();
}

static void
scan_freelist(
	xfs_agf_t	*agf)
{
	xfs_agfl_t	*agfl;
	xfs_agblock_t	bno;
	uint		count;
	int		i;

	if (XFS_SB_BLOCK(mp) != XFS_AGFL_BLOCK(mp) &&
	    XFS_AGF_BLOCK(mp) != XFS_AGFL_BLOCK(mp) &&
	    XFS_AGI_BLOCK(mp) != XFS_AGFL_BLOCK(mp))
		set_dbmap(agf->agf_seqno, XFS_AGFL_BLOCK(mp), 1, DBM_AGFL,
			agf->agf_seqno, XFS_SB_BLOCK(mp));
	if (agf->agf_flcount == 0)
		return;
	push_cur();
	set_cur(&typtab[TYP_AGFL],
		XFS_AG_DADDR(mp, agf->agf_seqno, XFS_AGFL_DADDR), 1,
		DB_RING_IGN, NULL);
	if ((agfl = iocur_top->data) == NULL) {
		dbprintf("can't read agfl block for ag %u\n", agf->agf_seqno);
		serious_error++;
		return;
	}
	i = agf->agf_flfirst;
	count = 0;
	for (;;) {
		bno = agfl->agfl_bno[i];
		set_dbmap(agf->agf_seqno, bno, 1, DBM_FREELIST, agf->agf_seqno,
			XFS_AGFL_BLOCK(mp));
		count++;
		if (i == agf->agf_fllast)
			break;
		if (++i == XFS_AGFL_SIZE)
			i = 0;
	}
	if (count != agf->agf_flcount) {
		if (!sflag)
			dbprintf("freeblk count %u != flcount %u in ag %u\n",
				count, agf->agf_flcount, agf->agf_seqno);
		error++;
	}
	fdblocks += count;
	pop_cur();
}

static void
scan_lbtree(
	xfs_fsblock_t	root,
	int		nlevels,
	scan_lbtree_f_t	func,
	dbm_t		type,
	inodata_t	*id,
	xfs_drfsbno_t	*totd,
	xfs_drfsbno_t	*toti,
	xfs_extnum_t	*nex,
	blkmap_t	**blkmapp,
	int		isroot,
	typnm_t		btype)
{
	push_cur();
	set_cur(&typtab[btype], XFS_FSB_TO_DADDR(mp, root), blkbb, DB_RING_IGN,
		NULL);
	if (iocur_top->data == NULL) {
		if (!sflag)
			dbprintf("can't read btree block %u/%u\n",
				XFS_FSB_TO_AGNO(mp, root),
				XFS_FSB_TO_AGBNO(mp, root));
		error++;
		return;
	}
	(*func)(iocur_top->data, nlevels - 1, type, root, id, totd, toti, nex,
		blkmapp, isroot, btype);
	pop_cur();
}

static void
scan_sbtree(
	xfs_agf_t	*agf,
	xfs_agblock_t	root,
	int		nlevels,
	int		isroot,
	scan_sbtree_f_t	func,
	typnm_t		btype)
{
	push_cur();
	set_cur(&typtab[btype], XFS_AGB_TO_DADDR(mp, agf->agf_seqno, root),
		blkbb, DB_RING_IGN, NULL);
	if (iocur_top->data == NULL) {
		if (!sflag)
			dbprintf("can't read btree block %u/%u\n",
				agf->agf_seqno, root);
		error++;
		return;
	}
	(*func)(iocur_top->data, nlevels - 1, agf, root, isroot);
	pop_cur();
}

static void
scanfunc_bmap(
	xfs_btree_lblock_t	*ablock,
	int			level,
	dbm_t			type,
	xfs_fsblock_t		bno,
	inodata_t		*id,
	xfs_drfsbno_t		*totd,
	xfs_drfsbno_t		*toti,
	xfs_extnum_t		*nex,
	blkmap_t		**blkmapp,
	int			isroot,
	typnm_t			btype)
{
	xfs_agblock_t		agbno;
	xfs_agnumber_t		agno;
	xfs_bmbt_block_t	*block = (xfs_bmbt_block_t *)ablock;
	int			i;
	xfs_bmbt_ptr_t		*pp;
	xfs_bmbt_rec_32_t	*rp;

	agno = XFS_FSB_TO_AGNO(mp, bno);
	agbno = XFS_FSB_TO_AGBNO(mp, bno);
	if (block->bb_magic != XFS_BMAP_MAGIC) {
		if (!sflag || id->ilist || CHECK_BLIST(bno))
			dbprintf("bad magic # %#x in inode %lld bmbt block "
				 "%u/%u\n",
				block->bb_magic, id->ino, agno, agbno);
		error++;
	}
	if (block->bb_level != level) {
		if (!sflag || id->ilist || CHECK_BLIST(bno))
			dbprintf("expected level %d got %d in inode %lld bmbt "
				 "block %u/%u\n",
				level, block->bb_level, id->ino, agno, agbno);
		error++;
	}
	set_dbmap(agno, agbno, 1, type, agno, agbno);
	set_inomap(agno, agbno, 1, id);
	(*toti)++;
	if (level == 0) {
		if (block->bb_numrecs > mp->m_bmap_dmxr[0] ||
		    isroot == 0 && block->bb_numrecs < mp->m_bmap_dmnr[0])  {
			if (!sflag || id->ilist || CHECK_BLIST(bno))
				dbprintf("bad btree nrecs (%u, min=%u, max=%u) "
					 "in inode %lld bmap block %lld\n",
					block->bb_numrecs, mp->m_bmap_dmnr[0],
					mp->m_bmap_dmxr[0], id->ino,
					(xfs_dfsbno_t)bno);
			error++;
			return;
		}
		rp = (xfs_bmbt_rec_32_t *)
			XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt,
			block, 1, mp->m_bmap_dmxr[0]);
		*nex += block->bb_numrecs;
		process_bmbt_reclist(rp, block->bb_numrecs, type, id, totd,
			blkmapp);
		return;
	}
	if (block->bb_numrecs > mp->m_bmap_dmxr[1] ||
	    isroot == 0 && block->bb_numrecs < mp->m_bmap_dmnr[1])  {
		if (!sflag || id->ilist || CHECK_BLIST(bno))
			dbprintf("bad btree nrecs (%u, min=%u, max=%u) in "
				 "inode %lld bmap block %lld\n",
				block->bb_numrecs, mp->m_bmap_dmnr[1],
				mp->m_bmap_dmxr[1], id->ino, (xfs_dfsbno_t)bno);
		error++;
		return;
	}
	pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt, block, 1,
		mp->m_bmap_dmxr[0]);
	for (i = 0; i < block->bb_numrecs; i++)
		scan_lbtree(pp[i], level, scanfunc_bmap, type, id, totd, toti,
			nex, blkmapp, 0, btype);
}

static void
scanfunc_bno(
	xfs_btree_sblock_t	*ablock,
	int			level,
	xfs_agf_t		*agf,
	xfs_agblock_t		bno,
	int			isroot)
{
	xfs_alloc_block_t	*block = (xfs_alloc_block_t *)ablock;
	int			i;
	xfs_alloc_ptr_t		*pp;
	xfs_alloc_rec_t		*rp;

	if (block->bb_magic != XFS_ABTB_MAGIC) {
		dbprintf("bad magic # %#x in btbno block %u/%u\n",
			block->bb_magic, agf->agf_seqno, bno);
		serious_error++;
		return;
	}
	fdblocks++;
	if (block->bb_level != level) {
		if (!sflag)
			dbprintf("expected level %d got %d in btbno block "
				 "%u/%u\n",
				level, block->bb_level, agf->agf_seqno, bno);
		error++;
	}
	set_dbmap(agf->agf_seqno, bno, 1, DBM_BTBNO, agf->agf_seqno, bno);
	if (level == 0) {
		if (block->bb_numrecs > mp->m_alloc_mxr[0] ||
		    isroot == 0 && block->bb_numrecs < mp->m_alloc_mnr[0]) {
			dbprintf("bad btree nrecs (%u, min=%u, max=%u) in "
				 "btbno block %u/%u\n",
				block->bb_numrecs, mp->m_alloc_mnr[0],
				mp->m_alloc_mxr[0], agf->agf_seqno, bno);
			serious_error++;
			return;
		}
		rp = XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize, xfs_alloc, block,
			1, mp->m_alloc_mxr[0]);
		for (i = 0; i < block->bb_numrecs; i++) {
			set_dbmap(agf->agf_seqno, rp[i].ar_startblock,
				rp[i].ar_blockcount, DBM_FREE1,
				agf->agf_seqno, bno);
		}
		return;
	}
	if (block->bb_numrecs > mp->m_alloc_mxr[1] ||
	    isroot == 0 && block->bb_numrecs < mp->m_alloc_mnr[1]) {
		dbprintf("bad btree nrecs (%u, min=%u, max=%u) in btbno block "
			 "%u/%u\n",
			block->bb_numrecs, mp->m_alloc_mnr[1],
			mp->m_alloc_mxr[1], agf->agf_seqno, bno);
		serious_error++;
		return;
	}
	pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_alloc, block, 1,
		mp->m_alloc_mxr[1]);
	for (i = 0; i < block->bb_numrecs; i++)
		scan_sbtree(agf, pp[i], level, 0, scanfunc_bno, TYP_BNOBT);
}

static void
scanfunc_cnt(
	xfs_btree_sblock_t	*ablock,
	int			level,
	xfs_agf_t		*agf,
	xfs_agblock_t		bno,
	int			isroot)
{
	xfs_alloc_block_t	*block = (xfs_alloc_block_t *)ablock;
	int			i;
	xfs_alloc_ptr_t		*pp;
	xfs_alloc_rec_t		*rp;

	if (block->bb_magic != XFS_ABTC_MAGIC) {
		dbprintf("bad magic # %#x in btcnt block %u/%u\n",
			block->bb_magic, agf->agf_seqno, bno);
		serious_error++;
		return;
	}
	fdblocks++;
	if (block->bb_level != level) {
		if (!sflag)
			dbprintf("expected level %d got %d in btcnt block "
				 "%u/%u\n",
				level, block->bb_level, agf->agf_seqno, bno);
		error++;
	}
	set_dbmap(agf->agf_seqno, bno, 1, DBM_BTCNT, agf->agf_seqno, bno);
	if (level == 0) {
		if (block->bb_numrecs > mp->m_alloc_mxr[0] ||
		    isroot == 0 && block->bb_numrecs < mp->m_alloc_mnr[0])  {
			dbprintf("bad btree nrecs (%u, min=%u, max=%u) in "
				 "btbno block %u/%u\n",
				block->bb_numrecs, mp->m_alloc_mnr[0],
				mp->m_alloc_mxr[0], agf->agf_seqno, bno);
			serious_error++;
			return;
		}
		rp = XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize, xfs_alloc, block,
			1, mp->m_alloc_mxr[0]);
		for (i = 0; i < block->bb_numrecs; i++) {
			check_set_dbmap(agf->agf_seqno, rp[i].ar_startblock,
				rp[i].ar_blockcount, DBM_FREE1, DBM_FREE2,
				agf->agf_seqno, bno);
			fdblocks += rp[i].ar_blockcount;
			agffreeblks += rp[i].ar_blockcount;
			if (rp[i].ar_blockcount > agflongest)
				agflongest = rp[i].ar_blockcount;
		}
		return;
	}
	if (block->bb_numrecs > mp->m_alloc_mxr[1] ||
	    isroot == 0 && block->bb_numrecs < mp->m_alloc_mnr[1])  {
		dbprintf("bad btree nrecs (%u, min=%u, max=%u) in btbno block "
			 "%u/%u\n",
			block->bb_numrecs, mp->m_alloc_mnr[1],
			mp->m_alloc_mxr[1], agf->agf_seqno, bno);
		serious_error++;
		return;
	}
	pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_alloc, block, 1,
		mp->m_alloc_mxr[1]);
	for (i = 0; i < block->bb_numrecs; i++)
		scan_sbtree(agf, pp[i], level, 0, scanfunc_cnt, TYP_CNTBT);
}

static void
scanfunc_ino(
	xfs_btree_sblock_t	*ablock,
	int			level,
	xfs_agf_t		*agf,
	xfs_agblock_t		bno,
	int			isroot)
{
	xfs_agino_t		agino;
	xfs_inobt_block_t	*block = (xfs_inobt_block_t *)ablock;
	int			i;
	int			isfree;
	int			j;
	int			nfree;
	int			off;
	xfs_inobt_ptr_t		*pp;
	xfs_inobt_rec_t		*rp;

	if (block->bb_magic != XFS_IBT_MAGIC) {
		dbprintf("bad magic # %#x in inobt block %u/%u\n",
			block->bb_magic, agf->agf_seqno, bno);
		serious_error++;
		return;
	}
	if (block->bb_level != level) {
		if (!sflag)
			dbprintf("expected level %d got %d in inobt block "
				 "%u/%u\n",
				level, block->bb_level, agf->agf_seqno, bno);
		error++;
	}
	set_dbmap(agf->agf_seqno, bno, 1, DBM_BTINO, agf->agf_seqno, bno);
	if (level == 0) {
		if (block->bb_numrecs > mp->m_inobt_mxr[0] ||
		    isroot == 0 && block->bb_numrecs < mp->m_inobt_mnr[0]) {
			dbprintf("bad btree nrecs (%u, min=%u, max=%u) in "
				 "inobt block %u/%u\n",
				block->bb_numrecs, mp->m_inobt_mnr[0],
				mp->m_inobt_mxr[0], agf->agf_seqno, bno);
			serious_error++;
			return;
		}
		rp = XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize, xfs_inobt, block,
			1, mp->m_inobt_mxr[0]);
		for (i = 0; i < block->bb_numrecs; i++) {
			agino = rp[i].ir_startino;
			off = XFS_INO_TO_OFFSET(mp, agino);
			if (off == 0) {
#if VERS >= V_62
				if ((sbversion & XFS_SB_VERSION_ALIGNBIT) &&
				    mp->m_sb.sb_inoalignmt &&
				    (XFS_INO_TO_AGBNO(mp, agino) %
				     mp->m_sb.sb_inoalignmt))
					sbversion &= ~XFS_SB_VERSION_ALIGNBIT;
#endif
				set_dbmap(agf->agf_seqno,
					XFS_AGINO_TO_AGBNO(mp, agino),
					(xfs_extlen_t)MAX(1,
						XFS_INODES_PER_CHUNK >>
						mp->m_sb.sb_inopblog),
					DBM_INODE, agf->agf_seqno, bno);
			}
			icount += XFS_INODES_PER_CHUNK;
			agicount += XFS_INODES_PER_CHUNK;
			ifree += rp[i].ir_freecount;
			agifreecount += rp[i].ir_freecount;
			push_cur();
			set_cur(&typtab[TYP_INODE],
				XFS_AGB_TO_DADDR(mp, agf->agf_seqno,
						 XFS_AGINO_TO_AGBNO(mp, agino)),
				(int)XFS_FSB_TO_BB(mp, XFS_IALLOC_BLOCKS(mp)),
				DB_RING_IGN, NULL);
			if (iocur_top->data == NULL) {
				if (!sflag)
					dbprintf("can't read inode block "
						 "%u/%u\n",
						agf->agf_seqno,
						XFS_AGINO_TO_AGBNO(mp, agino));
				error++;
				continue;
			}
			for (j = 0, nfree = 0; j < XFS_INODES_PER_CHUNK; j++) {
				if (isfree = XFS_INOBT_IS_FREE(&rp[i], j))
					nfree++;
				process_inode(agf, agino + j,
					(xfs_dinode_t *)((char *)iocur_top->data + ((off + j) << mp->m_sb.sb_inodelog)),
						isfree);
			}
			if (nfree != rp[i].ir_freecount) {
				if (!sflag)
					dbprintf("ir_freecount/free mismatch, "
						 "inode chunk %u/%u, freecount "
						 "%d nfree %d\n",
						agf->agf_seqno, agino,
						rp[i].ir_freecount, nfree);
				error++;
			}
			pop_cur();
		}
		return;
	}
	if (block->bb_numrecs > mp->m_inobt_mxr[1] ||
	    isroot == 0 && block->bb_numrecs < mp->m_inobt_mnr[1]) {
		dbprintf("bad btree nrecs (%u, min=%u, max=%u) in inobt block "
			 "%u/%u\n",
			block->bb_numrecs, mp->m_inobt_mnr[1],
			mp->m_inobt_mxr[1], agf->agf_seqno, bno);
		serious_error++;
		return;
	}
	pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_inobt, block, 1,
		mp->m_inobt_mxr[1]);
	for (i = 0; i < block->bb_numrecs; i++)
		scan_sbtree(agf, pp[i], level, 0, scanfunc_ino, TYP_INOBT);
}

static void
set_dbmap(
	xfs_agnumber_t	agno,
	xfs_agblock_t	agbno,
	xfs_extlen_t	len,
	dbm_t		type,
	xfs_agnumber_t	c_agno,
	xfs_agblock_t	c_agbno)
{
	check_set_dbmap(agno, agbno, len, DBM_UNKNOWN, type, c_agno, c_agbno);
}

static void
set_inomap(
	xfs_agnumber_t	agno,
	xfs_agblock_t	agbno,
	xfs_extlen_t	len,
	inodata_t	*id)
{
	xfs_extlen_t	i;
	inodata_t	**idp;
	int		mayprint;

	if (!check_inomap(agno, agbno, len, id->ino))
		return;
	mayprint = verbose | id->ilist | blist_size;
	for (i = 0, idp = &inomap[agno][agbno]; i < len; i++, idp++) {
		*idp = id;
		if (mayprint &&
		    (verbose || id->ilist || CHECK_BLISTA(agno, agbno + i)))
			dbprintf("setting inode to %lld for block %u/%u\n",
				id->ino, agno, agbno + i);
	}
}

static void
set_rdbmap(
	xfs_drfsbno_t	bno,
	xfs_extlen_t	len,
	dbm_t		type)
{
	check_set_rdbmap(bno, len, DBM_UNKNOWN, type);
}

static void
set_rinomap(
	xfs_drfsbno_t	bno,
	xfs_extlen_t	len,
	inodata_t	*id)
{
	xfs_extlen_t	i;
	inodata_t	**idp;
	int		mayprint;

	if (!check_rinomap(bno, len, id->ino))
		return;
	mayprint = verbose | id->ilist | blist_size;
	for (i = 0, idp = &inomap[mp->m_sb.sb_agcount][bno];
	     i < len;
	     i++, idp++) {
		*idp = id;
		if (mayprint && (verbose || id->ilist || CHECK_BLIST(bno + i)))
			dbprintf("setting inode to %lld for rtblock %llu\n",
				id->ino, bno + i);
	}
}

static void
setlink_inode(
	inodata_t	*id,
	nlink_t		nlink,
	int		isdir,
	int		security)
{
	id->link_set = nlink;
	id->isdir = isdir;
	id->security = security;
	if (verbose || id->ilist)
		dbprintf("inode %lld nlink %u %s dir\n", id->ino, nlink,
			isdir ? "is" : "not");
}
