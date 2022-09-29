/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993 Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __HFS_BT_H__
#define __HFS_BT_H__

#ident "$Revision: 1.4 $"

/*
 * Catalog and Extent overflow btree definitions.
 */
#define CATALOG 1
#define EXTOVRF 0
#define CATNODEMAX 64
#define EXTNODEMAX 8

#define EXTOVRF_FILE   3	/* File id for extent overflow file */
#define CATALOG_FILE   4	/* File id for catalog file */



/*
 * Bree node
 */
typedef struct nd{
  hlnode_t	n_hl;		/* Hashlist overhead */
  struct bt	*n_bt;		/* Owner btree */
  int		n_num;		/* Node number */
  u_int		n_flags;	/* Node flags */
  u_int		n_lbn;		/* Node logical block address */

  /* The following are aligned copies of the on-disk fields in n_buf */

  u_int		n_Flink;	/* Link to next node */
  u_int		n_Blink;	/* Link to previous node */
  u_char	n_Type;		/* Node type */
  u_char	n_NHeight;	/* Node level */
  u_short	n_NRecs;	/* Number of records in node. */
  u_short	n_Resv2;	/* Reserved */

  u_int		n_align;	/* Force work alignment for n_buf */
  u_short	n_buf[256];	/* buffer */
} nd_t;

#define N_HDR_SIZE 14

#define CAPACITY(np) (( (255-np->n_NRecs)*2 -\
			((u_short*)(np->n_buf))[255-np->n_NRecs])-2)

enum {IndexNode=0,
	HdrNode=1,
	MapNode=2,
	LeafNode=0xff};

#define INDEXNODE(n)   (n->n_Type==IndexNode)
#define LEAFNODE(n)    (n->n_Type==LeafNode)
#define CATALOGNODE(n) (n->n_bt->b_type==CATALOG)

#define N_ACTIVE  0x0001
#define N_DIRTY   0x0002

#define N_SET(n,m)	(n->n_flags |= N_##m)
#define N_CLR(n,m)	(n->n_flags &= ~N_##m)
#define N_ISSET(n,m)	(n->n_flags & N_##m)     /* T if *any* bit set */
#define N_ISCLR(n,m)	((n->n_flags & N_##m)==0)

#define	MAPNODE_BMAP_SIZE	(3952)
#define	bmap_set(m, p)		SET((char *) m->m_bmap, p)
#define	bmap_clr(m, p)		CLR((char *) m->m_bmap, p)

typedef struct mapent {
  u_int   m_Flink;	/* next map node number */
  u_int   m_Blink;	/* prev map node number */
  u_char  m_Type;	/* MapNode */
  u_char  m_NHeight;	/* just to conform */
  u_short m_NRecs;	/* will always be 1 */
  u_short m_Resv2;	/* will always be 0 */
  u_int   m_align;	/* Force buffer to be aligned */
  u_char  m_bmap[MAPNODE_BMAP_SIZE/8];
  /* 
   * The above part will align to the first 504 bytes 
   */
  u_int   m_num;	/* node number in the file */
  u_int   m_indx;	/* index for map: 0, 1, 2..*/
  u_int	  m_lbn;	/* node's logical block number */
  u_short m_dirty;	/* Indicates map node dirty*/
  struct  mapent *next; /* Next map in map list */
} mapent_t;

typedef struct maplist {
  u_int     ml_num;	/* Number of map nodes */
  mapent_t  *ml_head;	/* Ptr to head of map list */
  mapent_t  *ml_tail;	/* Ptr to tail of map list */
} maplist_t;


/*
 * Btree header
 */
typedef struct bthdr{
  u_short	bh_Depth;	/* Depth of tree */
  u_int		bh_Root;	/* Root node number */
  u_int		bh_NRecs;	/* Number of leaf records in tree */
  u_int		bh_FNode;	/* Number of first leaf node */
  u_int		bh_LNode;	/* Number of last leaf node */
  u_short	bh_NodeSize;	/* Size of a node (512) */
  u_short	bh_KeyLen;	/* Maximum key length */
  u_int		bh_NNodes;	/* Number of nodes in tree */
  u_int		bh_Free;	/* Number of free nodes in tree */
  u_char	bh_Resv[76];	/* Reserved */
} bthdr_t;


/*
 * Btree root structure
 */

#define BT_MAX_DIRECT_NODE 2048

typedef struct bt {
  hlroot_t	b_hl;		/* Hashlist root */
  int		(*b_compare)();	/* Key comparison routine */
  nd_t 		*b_nodebase;	/* Pool of nodes */
  u_int		b_node_count;	/* Number of nodes allocated */
  u_int		b_flags;	/* Btree flags */
  u_int		b_gen;		/* Btree generation number */
  hfs_t		*b_hfs;		/* Link to file system structure */
  u_int		b_type;		/* Btree type */
  erec_t	*b_erec;	/* Ptr to erec for this file. */
  ExtDescRec_t  *b_exts;	/* Ptr to first three extents */
  bthdr_t	b_hdr;		/* Btree header record */
  u_char	b_map[BT_MAX_DIRECT_NODE/8]; /* Btree node map */
  maplist_t	b_maplist;	/*Btree maplist struct */
} bt_t;

#define	BT_MAP_DIRTY	0x0001	/* Map dirty */
#define BT_MAPN_DIRTY	0x0002  /* Map nodes dirty */
#define BT_HDR_DIRTY	0x0004  /* Header dirty */
#define BT_ACTIVE	0x0008	/* Btree is alive */

#define BT_DIRTY	(BT_HDR_DIRTY | BT_MAP_DIRTY | BT_MAPN_DIRTY)
#define BT_ALC		(BT_HDR_DIRTY | BT_MAP_DIRTY | BT_MAPN_DIRTY)
#define BT_ATR		BT_HDR_DIRTY


#define BT_SET(b,m)	(b->b_flags |= BT_##m)
#define BT_CLR(b,m)	(b->b_flags &= ~BT_##m)
#define BT_ISSET(b,m)	(b->b_flags & BT_##m)     /* T if *any* bit set */
#define BT_ISCLR(b,m)	((b->b_flags & BT_##m)==0)

/* Map-Node-list Macros */
#define	BT_MLIST_HEAD(b)	((b->b_maplist).ml_head)
#define	BT_MLIST_TAIL(b)	((b->b_maplist).ml_tail)
#define BT_MLIST_NUM(b)		((b->b_maplist).ml_num)


/* Btree key */

typedef union{
  eofk_t ek;			/* Extent overflow file key */
  catk_t ck;			/* Catalog file key */
} btk_t;


/* Record coordinate */

typedef struct rcord{
  u_int node;			/* Node number */
  u_int rec;			/* Record number within node */
} rcord_t;


/*
 * Lineage structure - Records lineage of a record back to root.
 */
#define LN_MAX 9
typedef struct lineage{
  u_int	  depth;		/* Points to last entry in use in rc */
  rcord_t rc[LN_MAX];		/* Record coordinates (rc[0] == root)*/
} lineage_t;

#define LN_NODE(l) ((l)->rc[(l)->depth].node)
#define LN_REC(l) ((l)->rc[(l)->depth].rec)

/* Well known global bt_ routines */

int bt_rget(bt_t *bt, btk_t *key, rdesc_t *rd);
int bt_radd(bt_t *bt, btk_t *key, rdesc_t *rd);
int bt_rnext(bt_t *bt, btk_t *key, rdesc_t *rd);
int bt_rupdate(bt_t *bt, btk_t *key, rdesc_t *rd);
int bt_rremove(bt_t *bt, btk_t *key);

int bt_init(hfs_t *hfs, bt_t *bt, int type, int nodes);
int bt_destroy(bt_t *bt);

int bt_sync(bt_t *bt, u_int flags);

int bt_check(bt_t *bt, u_int transactions);

#endif










