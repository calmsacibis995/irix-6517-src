#ident "$Revision: 1.14 $"

#define _SGI_MP_SOURCE

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
#include <sys/sema.h>
#include <sys/stat.h>
#include <sys/uuid.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <assert.h>
#if VERS >= V_64
#include <ksys/behavior.h>
#endif
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_bit.h>
#include <sys/fs/xfs_log.h>
#include <sys/fs/xfs_trans.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_ag.h>
#include <sys/fs/xfs_alloc_btree.h>
#include <sys/fs/xfs_bmap_btree.h>
#include <sys/fs/xfs_ialloc.h>
#include <sys/fs/xfs_ialloc_btree.h>
#include <sys/fs/xfs_btree.h>
#include <sys/fs/xfs_attr_sf.h>
#include <sys/fs/xfs_dir_sf.h>
#include <sys/fs/xfs_dir.h>
#if VERS >= V_654
#include <sys/fs/xfs_dir2.h>
#include <sys/fs/xfs_dir2_sf.h>
#endif
#include <sys/fs/xfs_dinode.h>
#include <sys/fs/xfs_mount.h>

#include "sim.h"
#include "globals.h"
#include "incore.h"
#include "agheader.h"
#include "protos.h"
#include "err_protos.h"

extern avlnode_t	*avl_firstino(avlnode_t *root);

/*
 * array of inode tree ptrs, one per ag
 */
static avltree_desc_t	**inode_tree_ptrs;

/*
 * ditto for uncertain inodes
 */
static avltree_desc_t	**inode_uncertain_tree_ptrs;

#define ALLOC_NUM_INOS		100

/* free lists -- inode nodes and extent nodes */

typedef struct ino_flist_s  {
	ino_tree_node_t		*list;
	ino_tree_node_t		*last;
	long long		cnt;
} ino_flist_t;

static ino_flist_t ino_flist;	/* free list must be initialized before use */

/*
 * next is the uncertain inode list -- a sorted (in ascending order)
 * list of inode records sorted on the starting inode number.  There
 * is one list per ag.
 */

/*
 * common code for creating inode records for use by trees and lists.
 * called only from add_inodes and add_inodes_uncertain
 *
 * IMPORTANT:  all inodes (inode records) start off as free and
 *		unconfirmed.
 */
/* ARGSUSED */
static ino_tree_node_t *
mk_ino_tree_nodes(xfs_agino_t starting_ino)
{
	int i;
	ino_tree_node_t *new;
	avlnode_t *node;

	if (ino_flist.cnt == 0)  {
		assert(ino_flist.list == NULL);

		if ((new = malloc(sizeof(ino_tree_node_t[ALLOC_NUM_INOS])))
					== NULL)
			do_error("inode map malloc failed\n");

		for (i = 0; i < ALLOC_NUM_INOS; i++)  {
			new->avl_node.avl_nextino =
				(avlnode_t *) ino_flist.list;
			ino_flist.list = new;
			ino_flist.cnt++;
			new++;
		}
	}

	assert(ino_flist.list != NULL);

	new = ino_flist.list;
	ino_flist.list = (ino_tree_node_t *) new->avl_node.avl_nextino;
	ino_flist.cnt--;
	node = &new->avl_node;
	node->avl_nextino = node->avl_forw = node->avl_back = NULL;

	/* initialize node */

	new->ino_startnum = 0;
	new->ino_confirmed = 0;
	new->ino_isa_dir = 0;
	new->ir_free = (xfs_inofree_t) - 1;
	new->ino_un.backptrs = NULL;

	return(new);
}

/*
 * return inode record to free list, will be initialized when
 * it gets pulled off list
 */
static void
free_ino_tree_node(ino_tree_node_t *ino_rec)
{
	ino_rec->avl_node.avl_nextino = NULL;
	ino_rec->avl_node.avl_forw = NULL;
	ino_rec->avl_node.avl_back = NULL;

	if (ino_flist.list != NULL)  {
		assert(ino_flist.cnt > 0);
		ino_rec->avl_node.avl_nextino = (avlnode_t *) ino_flist.list;
	} else  {
		assert(ino_flist.cnt == 0);
		ino_rec->avl_node.avl_nextino = NULL;
	}

	ino_flist.list = ino_rec;
	ino_flist.cnt++;

	if (ino_rec->ino_un.backptrs != NULL)  {
		if (full_backptrs && ino_rec->ino_un.backptrs->parents != NULL)
			free(ino_rec->ino_un.backptrs->parents);
		if (ino_rec->ino_un.plist != NULL)
			free(ino_rec->ino_un.plist);
	}

	return;
}

/*
 * last referenced cache for uncertain inodes
 */
static ino_tree_node_t **last_rec;

/*
 * ok, the uncertain inodes are a set of trees just like the
 * good inodes but all starting inode records are (arbitrarily)
 * aligned on XFS_CHUNK_PER_INODE boundaries to prevent overlaps.
 * this means we may have partials records in the tree (e.g. records
 * without 64 confirmed uncertain inodes).  Tough.
 *
 * free is set to 1 if the inode is thought to be free, 0 if used
 */
void
add_aginode_uncertain(xfs_agnumber_t agno, xfs_agino_t ino, int free)
{
	ino_tree_node_t		*ino_rec;
	xfs_agino_t		s_ino;
	int			offset;

	assert(agno < glob_agcount);
	assert(last_rec != NULL);

	s_ino = rounddown(ino, XFS_INODES_PER_CHUNK);

	/*
	 * check for a cache hit
	 */
	if (last_rec[agno] != NULL && last_rec[agno]->ino_startnum == s_ino)  {
		offset = ino - s_ino;
		if (free)
			set_inode_free(last_rec[agno], offset);
		else
			set_inode_used(last_rec[agno], offset);

		return;
	}

	/*
	 * check to see if record containing inode is already in the tree.
	 * if not, add it
	 */
	if ((ino_rec = (ino_tree_node_t *)
			avl_findrange(inode_uncertain_tree_ptrs[agno],
				s_ino)) == NULL)  {
		ino_rec = mk_ino_tree_nodes(s_ino);
		ino_rec->ino_startnum = s_ino;

		if (avl_insert(inode_uncertain_tree_ptrs[agno],
				(avlnode_t *) ino_rec) == NULL)  {
			do_error("xfs_repair:  duplicate inode range\n");
		}
	}

	if (free)
		set_inode_free(ino_rec, ino - s_ino);
	else
		set_inode_used(ino_rec, ino - s_ino);

	/*
	 * set cache entry
	 */
	last_rec[agno] = ino_rec;

	return;
}

/*
 * like add_aginode_uncertain() only it needs an xfs_mount_t *
 * to perform the inode number conversion.
 */
void
add_inode_uncertain(xfs_mount_t *mp, xfs_ino_t ino, int free)
{
	add_aginode_uncertain(XFS_INO_TO_AGNO(mp, ino),
				XFS_INO_TO_AGINO(mp, ino), free);
}

/*
 * pull the indicated inode record out of the uncertain inode tree
 */
void
get_uncertain_inode_rec(xfs_agnumber_t agno, ino_tree_node_t *ino_rec)
{
	assert(inode_tree_ptrs != NULL);
	assert(inode_tree_ptrs[agno] != NULL);

	avl_delete(inode_uncertain_tree_ptrs[agno], &ino_rec->avl_node);

	ino_rec->avl_node.avl_nextino = NULL;
	ino_rec->avl_node.avl_forw = NULL;
	ino_rec->avl_node.avl_back = NULL;
}

ino_tree_node_t *
findfirst_uncertain_inode_rec(xfs_agnumber_t agno)
{
	return((ino_tree_node_t *)
		inode_uncertain_tree_ptrs[agno]->avl_firstino);
}

void
clear_uncertain_ino_cache(xfs_agnumber_t agno)
{
	last_rec[agno] = NULL;

	return;
}


/*
 * next comes the inode trees.  One per ag.  AVL trees
 * of inode records, each inode record tracking 64 inodes
 */
/*
 * set up an inode tree record for a group of inodes that will
 * include the requested inode.
 *
 * does NOT error-check for duplicate records.  Caller is
 * responsible for checking that.
 *
 * ino must be the start of an XFS_INODES_PER_CHUNK (64) inode chunk
 *
 * Each inode resides in a 64-inode chunk which can be part
 * one or more chunks (MAX(64, inodes-per-block).  The fs allocates
 * in chunks (as opposed to 1 chunk) when a block can hold more than
 * one chunk (inodes per block > 64).  Allocating in one chunk pieces
 * causes us problems when it takes more than one fs block to contain
 * an inode chunk because the chunks can start on *any* block boundary.
 * So we assume that the caller has a clue because at this level, we
 * don't.
 */
static ino_tree_node_t *
add_inode(xfs_agnumber_t agno, xfs_agino_t ino)
{
	ino_tree_node_t *ino_rec;

	/* no record exists, make some and put them into the tree */

	ino_rec = mk_ino_tree_nodes(ino);
	ino_rec->ino_startnum = ino;

	if (avl_insert(inode_tree_ptrs[agno],
			(avlnode_t *) ino_rec) == NULL)  {
		do_error("xfs_repair:  duplicate inode range\n");
	}

	return(ino_rec);
}

/*
 * pull the indicated inode record out of the inode tree
 */
void
get_inode_rec(xfs_agnumber_t agno, ino_tree_node_t *ino_rec)
{
	assert(inode_tree_ptrs != NULL);
	assert(inode_tree_ptrs[agno] != NULL);

	avl_delete(inode_tree_ptrs[agno], &ino_rec->avl_node);

	ino_rec->avl_node.avl_nextino = NULL;
	ino_rec->avl_node.avl_forw = NULL;
	ino_rec->avl_node.avl_back = NULL;
}

/*
 * free the designated inode record (return it to the free pool)
 */
/* ARGSUSED */
void
free_inode_rec(xfs_agnumber_t agno, ino_tree_node_t *ino_rec)
{
	free_ino_tree_node(ino_rec);

	return;
}

/*
 * returns the inode record desired containing the inode
 * returns NULL if inode doesn't exist.  The tree-based find
 * routines do NOT pull records out of the tree.
 */
ino_tree_node_t *
find_inode_rec(xfs_agnumber_t agno, xfs_agino_t ino)
{
	return((ino_tree_node_t *)
		avl_findrange(inode_tree_ptrs[agno], ino));
}

void
find_inode_rec_range(xfs_agnumber_t agno, xfs_agino_t start_ino,
			xfs_agino_t end_ino, ino_tree_node_t **first,
			ino_tree_node_t **last)
{
	*first = *last = NULL;

	avl_findranges(inode_tree_ptrs[agno], start_ino,
		end_ino, (avlnode_t **) first, (avlnode_t **) last);
	return;
}

/*
 * if ino doesn't exist, it must be properly aligned -- on a
 * filesystem block boundary or XFS_INODES_PER_CHUNK boundary,
 * whichever alignment is larger.
 */
ino_tree_node_t *
set_inode_used_alloc(xfs_agnumber_t agno, xfs_agino_t ino)
{
	ino_tree_node_t *ino_rec;

	/*
	 * check alignment -- the only way to detect this
	 * is too see if the chunk overlaps another chunk
	 * already in the tree
	 */
	ino_rec = add_inode(agno, ino);

	assert(ino_rec != NULL);
	assert(ino >= ino_rec->ino_startnum &&
		ino - ino_rec->ino_startnum < XFS_INODES_PER_CHUNK);

	set_inode_used(ino_rec, ino - ino_rec->ino_startnum);

	return(ino_rec);
}

ino_tree_node_t *
set_inode_free_alloc(xfs_agnumber_t agno, xfs_agino_t ino)
{
	ino_tree_node_t *ino_rec;

	ino_rec = add_inode(agno, ino);

	assert(ino_rec != NULL);
	assert(ino >= ino_rec->ino_startnum &&
		ino - ino_rec->ino_startnum < XFS_INODES_PER_CHUNK);

	set_inode_free(ino_rec, ino - ino_rec->ino_startnum);

	return(ino_rec);
}

ino_tree_node_t *
findfirst_inode_rec(xfs_agnumber_t agno)
{
	return((ino_tree_node_t *) inode_tree_ptrs[agno]->avl_firstino);
}

void
print_inode_list_int(xfs_agnumber_t agno, int uncertain)
{
	ino_tree_node_t *ino_rec;

	if (!uncertain)  {
		fprintf(stderr, "good inode list is --\n");
		ino_rec = findfirst_inode_rec(agno);
	} else  {
		fprintf(stderr, "uncertain inode list is --\n");
		ino_rec = findfirst_uncertain_inode_rec(agno);
	}

	if (ino_rec == NULL)  {
		fprintf(stderr, "agno %d -- no inodes\n", agno);
		return;
	}

	printf("agno %d\n", agno);

	while(ino_rec != NULL)  {
		fprintf(stderr,
	"\tptr = 0x%x, start = 0x%x, free = 0x%llx, confirmed = 0x%llx\n",
			ino_rec,
			ino_rec->ino_startnum,
			ino_rec->ir_free,
			ino_rec->ino_confirmed);
		if (ino_rec->ino_startnum == 0)
			ino_rec = ino_rec;
		ino_rec = next_ino_rec(ino_rec);
	}
}

void
print_inode_list(xfs_agnumber_t agno)
{
	print_inode_list_int(agno, 0);
}

void
print_uncertain_inode_list(xfs_agnumber_t agno)
{
	print_inode_list_int(agno, 1);
}

/*
 * set parent -- use a bitmask and a packed array.  The bitmask
 * indicate which inodes have an entry in the array.  An inode that
 * is the Nth bit set in the mask is stored in the Nth location in
 * the array where N starts at 0.
 */
void
set_inode_parent(ino_tree_node_t *irec, int offset, xfs_ino_t parent)
{
	int		i;
	int		cnt;
	int		target;
	__uint64_t	bitmask;
	parent_entry_t	*tmp;

	assert(full_backptrs == 0);

	if (irec->ino_un.plist == NULL)  {
		if ((irec->ino_un.plist = malloc(sizeof(parent_list_t)))
						== NULL)  {
			do_error("couldn't malloc parent list table\n");
		}
		irec->ino_un.plist->pmask = 1LL << offset;
		irec->ino_un.plist->pentries = memalign(sizeof(xfs_ino_t),
							sizeof(xfs_ino_t));
#ifdef DEBUG
		irec->ino_un.plist->cnt = 1;
#endif
		irec->ino_un.plist->pentries[0] = parent;

		return;
	}

	if (irec->ino_un.plist->pmask & (1LL << offset))  {
		bitmask = 1LL;
		target = 0;

		for (i = 0; i < offset; i++)  {
			if (irec->ino_un.plist->pmask & bitmask)
				target++;
			bitmask <<= 1;
		}
#ifdef DEBUG
		assert(target < irec->ino_un.plist->cnt);
#endif
		irec->ino_un.plist->pentries[target] = parent;

		return;
	}

	bitmask = 1LL;
	cnt = target = 0;

	for (i = 0; i < XFS_INODES_PER_CHUNK; i++)  {
		if (irec->ino_un.plist->pmask & bitmask)  {
			cnt++;
			if (i < offset)
				target++;
		}

		bitmask <<= 1;
	}

#ifdef DEBUG
	assert(cnt == irec->ino_un.plist->cnt);
#endif
	assert(cnt >= target);

	tmp = memalign(sizeof(xfs_ino_t), (cnt + 1) * sizeof(xfs_ino_t));

	(void) bcopy(irec->ino_un.plist->pentries, tmp,
			target * sizeof(parent_entry_t));

	if (cnt > target)
		(void) bcopy(irec->ino_un.plist->pentries + target,
				tmp + target + 1,
				(cnt - target) * sizeof(parent_entry_t));

	free(irec->ino_un.plist->pentries);

	irec->ino_un.plist->pentries = tmp;

#ifdef DEBUG
	irec->ino_un.plist->cnt++;
#endif
	irec->ino_un.plist->pentries[target] = parent;
	irec->ino_un.plist->pmask |= (1LL << offset);

	return;
}

#if 0
/*
 * not needed for now since we don't set the parent info
 * until phase 4 -- at which point we know that the directory
 * inode won't be going away -- so we won't ever need to clear
 * directory parent data that we set.
 */
void
clear_inode_parent(ino_tree_node_t *irec, int offset)
{
	assert(full_backptrs == 0);
	assert(irec->ino_un.plist != NULL);

	return;
}
#endif

xfs_ino_t
get_inode_parent(ino_tree_node_t *irec, int offset)
{
	__uint64_t	bitmask;
	parent_list_t	*ptbl;
	int		i;
	int		target;

	if (full_backptrs)
		ptbl = irec->ino_un.backptrs->parents;
	else
		ptbl = irec->ino_un.plist;

	if (ptbl->pmask & (1LL << offset))  {
		bitmask = 1LL;
		target = 0;

		for (i = 0; i < offset; i++)  {
			if (ptbl->pmask & bitmask)
				target++;
			bitmask <<= 1;
		}
#ifdef DEBUG
		assert(target < ptbl->cnt);
#endif
		return(ptbl->pentries[target]);
	}

	return(0LL);
}

/*
 * code that deals with the inode descriptor appendages -- the back
 * pointers, link counts and reached bits for phase 6 and phase 7.
 */

void
add_inode_reached(ino_tree_node_t *ino_rec, int ino_offset)
{
	assert(ino_rec->ino_un.backptrs != NULL);

	ino_rec->ino_un.backptrs->nlinks[ino_offset]++;
	XFS_INO_RCHD_SET_RCHD(ino_rec, ino_offset);

	assert(is_inode_reached(ino_rec, ino_offset));

	return;
}

int
is_inode_reached(ino_tree_node_t *ino_rec, int ino_offset)
{
	assert(ino_rec->ino_un.backptrs != NULL);
	return(XFS_INO_RCHD_IS_RCHD(ino_rec, ino_offset));
}

void
add_inode_ref(ino_tree_node_t *ino_rec, int ino_offset)
{
	assert(ino_rec->ino_un.backptrs != NULL);

	ino_rec->ino_un.backptrs->nlinks[ino_offset]++;

	return;
}

void
drop_inode_ref(ino_tree_node_t *ino_rec, int ino_offset)
{
	assert(ino_rec->ino_un.backptrs != NULL);
	assert(ino_rec->ino_un.backptrs->nlinks[ino_offset] > 0);

	if (--ino_rec->ino_un.backptrs->nlinks[ino_offset] == 0)
		XFS_INO_RCHD_CLR_RCHD(ino_rec, ino_offset);

	return;
}

int
is_inode_referenced(ino_tree_node_t *ino_rec, int ino_offset)
{
	assert(ino_rec->ino_un.backptrs != NULL);
	return(ino_rec->ino_un.backptrs->nlinks[ino_offset] > 0);
}

__uint32_t
num_inode_references(ino_tree_node_t *ino_rec, int ino_offset)
{
	assert(ino_rec->ino_un.backptrs != NULL);
	return(ino_rec->ino_un.backptrs->nlinks[ino_offset]);
}

#if 0
static backptrs_t	*bptrs;
static int		bptrs_index;
#define BPTR_ALLOC_NUM	1000

backptrs_t *
get_backptr(void)
{
	backptrs_t *bptr;

	if (bptrs_index == BPTR_ALLOC_NUM)  {
		assert(bptrs == NULL);

		if ((bptrs = malloc(sizeof(backptrs_t[BPTR_ALLOC_NUM])))
				== NULL)  {
			do_error("couldn't malloc ino rec backptrs.\n");
		}

		bptrs_index = 0;
	}

	assert(bptrs != NULL);

	bptr = &bptrs[bptrs_index];
	bptrs_index++;

	if (bptrs_index == BPTR_ALLOC_NUM)
		bptrs = NULL;

	bzero(bptr, sizeof(backptrs_t));

	return(bptr);
}
#endif

backptrs_t *
get_backptr(void)
{
	backptrs_t *ptr;

	if ((ptr = malloc(sizeof(backptrs_t))) == NULL)
		do_error("could not malloc back pointer table\n");
	
	bzero(ptr, sizeof(backptrs_t));

	return(ptr);
}

void
add_ino_backptrs(xfs_mount_t *mp)
{
#ifdef XR_BCKPTR_DBG
	xfs_ino_t ino;
	int j, k;
#endif /* XR_BCKPTR_DBG */
	ino_tree_node_t *ino_rec;
	parent_list_t *tmp;
	xfs_agnumber_t i;

	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		ino_rec = findfirst_inode_rec(i);

		while (ino_rec != NULL)  {
			tmp = ino_rec->ino_un.plist;
			ino_rec->ino_un.backptrs = get_backptr(); 
			ino_rec->ino_un.backptrs->parents = tmp;

#ifdef XR_BCKPTR_DBG
			if (tmp != NULL)  {
				k = 0;
				for (j = 0; j < XFS_INODES_PER_CHUNK; j++)  {
					ino = XFS_AGINO_TO_INO(mp, i,
						ino_rec->ino_startnum + j);
					if (ino == 25165846)  {
						do_warn("THERE 1 !!!\n");
					}
					if (tmp->pentries[j] != 0)  {
						k++;
						do_warn(
						"inode %llu - parent %llu\n",
							ino,
							tmp->pentries[j]);
						if (ino == 25165846)  {
							do_warn("THERE!!!\n");
						}
					}
				}

				if (k != tmp->cnt)  {
					do_warn(
					"ERROR - count = %d, counted %d\n",
						tmp->cnt, k);
				}
			}
#endif /* XR_BCKPTR_DBG */
			ino_rec = next_ino_rec(ino_rec);
		}
	}

	full_backptrs = 1;

	return;
}

static __psunsigned_t
avl_ino_start(avlnode_t *node)
{
	return((__psunsigned_t) ((ino_tree_node_t *) node)->ino_startnum);
}

static __psunsigned_t
avl_ino_end(avlnode_t *node)
{
	return((__psunsigned_t) (
		((ino_tree_node_t *) node)->ino_startnum +
		XFS_INODES_PER_CHUNK));
}

avlops_t avl_ino_tree_ops = {
	avl_ino_start,
	avl_ino_end
};

void
incore_ino_init(xfs_mount_t *mp)
{
	int i;
	int agcount = mp->m_sb.sb_agcount;

	if ((inode_tree_ptrs = malloc(agcount *
					sizeof(avltree_desc_t *))) == NULL)
		do_error("couldn't malloc inode tree descriptor table\n");
	if ((inode_uncertain_tree_ptrs = malloc(agcount *
					sizeof(avltree_desc_t *))) == NULL)
		do_error("couldn't malloc uncertain ino tree descriptor table\n");

	for (i = 0; i < agcount; i++)  {
		if ((inode_tree_ptrs[i] =
				malloc(sizeof(avltree_desc_t))) == NULL)
			do_error("couldn't malloc inode tree descriptor\n");
		if ((inode_uncertain_tree_ptrs[i] =
				malloc(sizeof(avltree_desc_t))) == NULL)
			do_error(
			"couldn't malloc uncertain ino tree descriptor\n");
	}
	for (i = 0; i < agcount; i++)  {
		avl_init_tree(inode_tree_ptrs[i], &avl_ino_tree_ops);
		avl_init_tree(inode_uncertain_tree_ptrs[i], &avl_ino_tree_ops);
	}

	ino_flist.cnt = 0;
	ino_flist.list = NULL;

	if ((last_rec = malloc(sizeof(ino_tree_node_t *) * agcount)) == NULL)
		do_error("couldn't malloc uncertain inode cache area\n");

	bzero(last_rec, sizeof(ino_tree_node_t *) * agcount);

	full_backptrs = 0;

	return;
}

#ifdef XR_INO_REF_DEBUG
void
add_inode_refchecked(xfs_ino_t ino, ino_tree_node_t *ino_rec, int ino_offset)
{
	XFS_INOPROC_SET_PROC((ino_rec), (ino_offset));

	assert(is_inode_refchecked(ino, ino_rec, ino_offset));

	return;
}

int
is_inode_refchecked(xfs_ino_t ino, ino_tree_node_t *ino_rec, int ino_offset)
{
	return(XFS_INOPROC_IS_PROC(ino_rec, ino_offset) == 0LL ? 0 : 1);
}
#endif /* XR_INO_REF_DEBUG */
