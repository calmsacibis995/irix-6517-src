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

#ident "$Revision: 1.14 $"

#include <ctype.h>
#include <errno.h>
#include <sys/param.h>
#include "hfs.h"

/* #define TRACEREC */

/*
 * NOTE: Currently we don't use the get/put protocol for node buffers. Instead
 * we just do gets and never do puts.  We don't track reference counts at all.
 * In principle this could allow inconsistencies to develop as a node pointed
 * to by a node pointer could change unexpectedly.  We avoid this
 * for now by having a large node cache that operates under a LRU reuse
 * protocol.  So long as the node cache is bigger than the maximum number
 * of nodes that might be used during any gross btree transaction, we are
 * safe.  It would probably be better to convert to reference counting nodes
 * like just about everything else this program
 */

/* Forward routines */

static int  add_record(nd_t *, rdesc_t *, int match, lineage_t *);
static int  bt_expand(bt_t *bt);
static int  bt_update(bt_t *bt, u_int flags);
static int  cat_key_compare(catk_t *g, catk_t *t);
static void cons_ptr(nd_t *np, rdesc_t *rd);
static int  delete_record(bt_t *, lineage_t *);
static int  eof_key_compare(eofk_t *g, eofk_t *k);
static int  find_record(bt_t *, btk_t *, rdesc_t *, lineage_t *, int *);
static void insert_nth_record(nd_t *, u_int n, rdesc_t *rd);
static void merge_key(nd_t *, rdesc_t *, btk_t *, rdesc_t*);
static int  nd_dealloc(nd_t *np);
static void nd_dump(nd_t *, char *str);
static void nd_free(nd_t *np);
static int  nd_get(bt_t *bt, int node, nd_t **);
static int  nd_init_root(bt_t *bt);
static void nd_new(bt_t *bt, nd_t **nnp);
static int  nd_new_root(nd_t *rnp, nd_t *np, lineage_t *ln);
static int  nd_promote(nd_t *,lineage_t *);
static int  nd_read(nd_t *);
static int  nd_reclaim(bt_t *, nd_t *);
static int  nd_remove(nd_t *np, lineage_t *ln);
static int  nd_search(nd_t *, btk_t *, rdesc_t *, lineage_t *);
static int  nd_split(nd_t *, lineage_t *, nd_t **);
static int  nd_update(bt_t *, nd_t *, u_int flags);
static int  nd_update_parent(nd_t *np, lineage_t *ln);
static void nth_record(nd_t *, u_int n, rdesc_t *);
static int  ntolb(bt_t *, ExtDescRec_t *, erec_t *, u_int n, u_int *block, u_int *nblks);
static void remove_nth_record(nd_t *, u_int n);
static void separate_key(nd_t *, rdesc_t *, btk_t *);

/*
 * The following functions manipulate map-lists.
 */
int bmap_maplist_init(bt_t *bt);
int bmap_maplist_sync(bt_t *bt);
int bmap_maplist_clear(bt_t *bt);
int bmap_maplist_find_free_bit(bt_t *bt, int *map, int *bit);
int bmap_maplist_set_bit(bt_t *bt, int map, int bit);
int bmap_maplist_clr_bit(bt_t *bt, int map, int bit);
int bmap_maplist_is_bit_valid(bt_t *bt, int map, int bit);
int bmap_maplist_cvt_bit_to_node(int map, int bit);
int bmap_maplist_is_node_mapped(bt_t *bt, int node);
int bmap_maplist_add_mapent(bt_t *bt, mapent_t *me);
int bmap_maplist_create_mapent(bt_t *bt, mapent_t **me, int node);
int bmap_maplist_need_mapnodes(bt_t *bt);
mapent_t *bmap_maplist_get_mapent(bt_t *bt, int m_indx);
/*
 * The following functions manipulate map-entries.
 */
int bmap_mapent_free_bit(mapent_t *me);
int bmap_mapent_extract_node(mapent_t **me, nd_t *np, int nodenum);
int bmap_mapent_fill_node(bt_t *bt, mapent_t *me, nd_t **nnp);
int bmap_mapent_set_bit(mapent_t *me, int bit);
int bmap_mapent_clr_bit(mapent_t *me, int bit);
int bmap_mapent_get_mapindx(mapent_t *me);
/*
 * The following functions are used to manipulate nodes.
 * (i.e.) Writing, Reading nodes to disk and updating accounts.
 */
static int nd_get_map(bt_t *bt, nd_t **nnp, int node);
static int nd_write_map(bt_t *bt, nd_t *np);
static int nd_account(bt_t *bt, nd_t *np, int node);


/***************************************************************************
 *                                                                         *
 *                    	      Global Routines                              *
 *                                                                         *
 ***************************************************************************/


#define VALID_KEY(key,bt)  (key->ck.k_hdr.kh_gen == bt->b_gen)

/*********
 * bt_rget	Search for a record by its key.
 */
int bt_rget(bt_t *bt, btk_t *key, rdesc_t *rd){
  int match=0;
  nd_t *np;
  lineage_t ln;

  /*
   * Check for no records at all.
   */
  if (bt->b_hdr.bh_Depth==0)
    return ENOENT;

  /*
   * Check for a valid node/record/generation.  If so we can bypass the
   * lookup.
   */
  if (VALID_KEY(key,bt)){
    __chkr(bt->b_hfs,
	   nd_get(bt, key->ck.k_hdr.kh_node, &np),
	   bt_rget);
    nth_record(np,key->ck.k_hdr.kh_rec,rd);
  }
  else
    __chkr(bt->b_hfs,
	   find_record(bt,key,rd,&ln, &match),
	   bt_rget);
  if (match)
    return ENOENT;
  return 0;
}


/*********
 * bt_radd	Insert a record in the file by key.
 */
int bt_radd(bt_t *bt, btk_t *key, rdesc_t *rd){
  int error, match=0;
  lineage_t ln;
  nd_t  *np;
  char      tmp[sizeof(cdr_t)+sizeof(btk_t)];
  rdesc_t   trd;

  /*
   * Check for no nodes at all.
   * If so, create a new root node and skip the lookup.
   */
  if (bt->b_hdr.bh_Depth==0)
    if (error=nd_init_root(bt))
      return error;
    else{
      ln.depth=0;
      LN_NODE(&ln)=bt->b_hdr.bh_Root;
      LN_REC(&ln)=0;
      match=0;
      goto insert;
    }

  /*
   * Try to find the record.
   * If we actually find it, return an error.
   */
  if (error=find_record(bt,key,&trd,&ln,&match))
    return error;
  if (match==0)
    return EEXIST;
  
 insert:
  /*
   * Fetch the closest node.
   * Merge the key with the record and then insert the combined
   *   record in the tree.
   */
  if (error=nd_get(bt,LN_NODE(&ln),&np))
    return error;
  trd.data=tmp; trd.count=0;
  merge_key(np,rd,key,&trd);
  return add_record(np,&trd,match,&ln);
}


/**********
 * bt_rnext	Read the next record  from this key.
 *=========
 *
 * NOTE:  If we would run off the end of the btree, we return
 *	  an error but leave the current key unmodified.  On the other-
 *	  hand, if we advance to the next node and then encounter some
 *	  sort of error, the key is left pointing to the problem node.
 */
int bt_rnext(bt_t *bt, btk_t *key, rdesc_t *rd){
  int error, nextk, nextn;
  nd_t *np;

  /*
   * If the key is invalid (generation mismatch) resync.
   */
  if (!VALID_KEY(key,bt))
    if (error=bt_rget(bt,key,rd))
      return error;

  /*
   * Get the node.
   * Advance to the next record and check for exhaustion of this node
   *   in which case, advance to the next node.
   */
 again:
  __chkr(bt->b_hfs,
	 nd_get(bt, key->ck.k_hdr.kh_node, &np),
	 bt_rnext);
  nextk = key->ck.k_hdr.kh_rec+1;
  nextn = np->n_Flink;
  if (nextk >= np->n_NRecs){
    if (nextn){
      key->ck.k_hdr.kh_node = nextn;
      key->ck.k_hdr.kh_rec = -1;
      goto again;
    }
    else return ENOENT;
  }
  key->ck.k_hdr.kh_rec = nextk;

  /*
   * Locate the next record and separate it into the record and the key.
   */
  nth_record(np,nextk,rd);
  separate_key(np,rd,key);

  return 0;
}


/*********
 * nodekey	Returns node number for use as hash key.
 */
static int nodekey(register nd_t *nk){ return nk->n_num; }


/* Structure for packing/unpacking btree header record */

#define BTHOFF(m) sizeof(((bthdr_t*)0)->m), offsetof(bthdr_t,m)
#define BTHOFFn(m,n) n*sizeof(((bthdr_t*)0)->m), offsetof(bthdr_t,m)

static u_short hdrpack[]={
  BTHOFF(bh_Depth),
  BTHOFF(bh_Root),
  BTHOFF(bh_NRecs),
  BTHOFF(bh_FNode),
  BTHOFF(bh_LNode),
  BTHOFF(bh_NodeSize),
  BTHOFF(bh_KeyLen),
  BTHOFF(bh_NNodes),
  BTHOFF(bh_Free),
  BTHOFFn(bh_Resv[0],76),
  0,0};


/*********
 * bt_init
 */
int bt_init(hfs_t *hfs, bt_t *bt, int type, int nodes){
  int error;
  int i;
  nd_t *np;
  rdesc_t rd;

  assert(BT_ISCLR(bt,ACTIVE));

  /*
   * Set comparison routine.
   */
  bt->b_compare = (type==CATALOG)
    ? (int (*)())cat_key_compare
    : (int (*)())eof_key_compare;

  /*
   * Init hash root.
   * Allocate nodes and enter them into the free list.
   */
  hl_init(&bt->b_hl, 4,(int (*)(hlnode_t*))nodekey);
  bt->b_nodebase = np = (nd_t*)safe_malloc(nodes * sizeof(nd_t));
  bt->b_node_count = nodes;
  bzero(bt->b_nodebase,nodes*sizeof(nd_t));

  for (i=0;i<bt->b_node_count;np++,i++){
    np->n_bt = bt;
    NULLHLNODE(np->n_hl);
    hl_insert_free(&bt->b_hl,(hlnode_t*)np);
  }


  /*
   * Set the hfs link and set the starting generation number.
   * The generation number gets incremented whenever we rorganize the index
   * nodes.
   * Set the btree type (catalog or extents file).
   */
  bt->b_hfs = hfs;
  bt->b_gen = 0;
  bt->b_type = type;

  /*
   * Set the ptr to the extend record for this file.
   *
   * Note that we can't fetch the erec for this file yet as the btrees
   * are initialized before the erec logic is initialized.
   */
  bt->b_exts = (type==CATALOG) ? &hfs->fs_mdb.m_CTExtRec
                               : &hfs->fs_mdb.m_XTExtRec;
  bt->b_erec = NULL;

  /*
   * Locate and unpack the header record.
   * Locate and copy the map record.
   * Initialise the Map-Node list inside B-Tree.
   */
  bmap_maplist_init(bt);
  return 0;
}


/************
 * bt_destroy
 */
int bt_destroy(bt_t *bt){
  if (BT_ISSET(bt,ACTIVE))
    free(bt->b_nodebase);
  BT_CLR(bt,ACTIVE);
  hl_destroy(&bt->b_hl);
  return (0);
}


/*********
 * bt_sync	Synchronize btree with disk.
 *
 * NOTE:	As we attempt to write various components out, we
 *		don't quit on error.  We try to get as much done as
 * 		we can so a disk recovery program will have as much to
 *		work with as possible.
 */
int bt_sync(bt_t *bt, u_int flags){
  int error=0,temperr;
  int i;
  nd_t *np;
  u_int code;


  /* If no flags are set, set them all */

  if (!flags)
    flags = ALL;

  bmap_maplist_sync(bt);
  /*
   * Process the btree itself.
   * First check to see if the map-nodes need to be sync'd.
   */
  __chkr(bt->b_hfs,
	 update_tree(bt,flags),
	 bt_sync);
  /*
   * Scan over all nodes looking for active ones.
   */
  for(np = bt->b_nodebase,i=0;i<bt->b_node_count;i++,np++)
    if (N_ISSET(np,ACTIVE)){
      temperr=__chk(bt->b_hfs,
		    nd_update(bt,np,flags),
		    bt_sync);
      if (error==0)
	error = temperr;
    }
  return error;
}



/************
 * bt_rupdate	Update a record.
 *===========
 *
 * NOTE:	The new record must be the same size as the old record.
 */
int bt_rupdate(bt_t *bt, btk_t *key, rdesc_t *rd){
  int      error;
  rdesc_t  trd;
  nd_t *np;
  btk_t    lk;

  /*
   * First "validate" the key.
   * Next get the associated node.
   */
  if (error=bt_rget(bt,key,&trd))
    return error;
  __chkr(bt->b_hfs,
	 nd_get(bt,((catk_t*)key)->k_hdr.kh_node,&np),
	 bt_rupdate);

  /*
   * Locate the record.
   * Separate the key from the contents of the record so that the record
   *   descriptor points to the data portion of the record alone.
   * Check that the data record sizes are the same.
   */
  nth_record(np,((catk_t*)key)->k_hdr.kh_rec,&trd);
  separate_key(np,&trd,&lk);
  if (trd.count != rd->count)
    return EINVAL;

  /*
   * Copy the data to the node and set the node dirty.
   */
  bcopy(rd->data,trd.data,rd->count);
  N_SET(np,DIRTY);
  return 0;
}


/************
 * bt_rremove	Remove a record.
 */
int bt_rremove(bt_t *bt, btk_t *key){
  lineage_t ln;
  rdesc_t   rd;
  int       error, match;

  /*
   * Find the record to verify it exists and to build its lineage structure.
   * Delete_record does all the work.
   */
  if (error=find_record(bt,key,&rd,&ln,&match))
    return error;
  if (match)
    return ENOENT;

  /*
   * Update the bt generation number to invalidate force the revalidation
   *   of any stored keys.
   * Delete the record.
   */
  return __chk(bt->b_hfs,
	       delete_record(bt,&ln),
	       bt_remove);
}


/**********
 * bt_check	Check that record insertions will succeed.
 *=========
 * The btree code presently lacks the means of "backing out" of some
 * record insertions that fail due to lack of room.  Until such a capability
 * is added, we check in advance to be sure that the worst case tree growth
 * can be satisfied.  Note that if the transactions might fail, we go ahead
 * and expand the tree if we can.
 */
int bt_check(bt_t *bt, u_int transactions){
  int error=0;

  return error; /* b-tree expansion is handled separately in nd_alloc() */

#if 0
  while(error==0 && transactions*(bt->b_hdr.bh_Depth+1) > bt->b_hdr.bh_Free){
    bt_t *ebt = &bt->b_hfs->fs_ebt;
    if (ebt->b_hdr.bh_Depth+1 > ebt->b_hdr.bh_Free)
      error = bt_expand(ebt);
    else
      error = bt_expand(bt);
  }
  return error;
#endif
}


/***************************************************************************
 *                                                                         *
 *                    	      Local Routines                               *
 *                                                                         *
 ***************************************************************************/

	/*************************/
	/***** TREE ROUTINES *****/
	/*************************/

/***********
 * bt_expand	Expand a btree.
 */
static int bt_expand(bt_t *bt){
  int   error;
  u_int clmpsz, *sizep;
  u_int original, delta;
  hfs_t *hfs = bt->b_hfs;

  if (bt->b_type==CATALOG){
    clmpsz = hfs->fs_mdb.m_CtClpSiz;
    sizep= &hfs->fs_mdb.m_CTFlSize;
  }
  else{
    clmpsz = hfs->fs_mdb.m_XtClpSiz;
    sizep = &hfs->fs_mdb.m_XTFlSize;
  }
  clmpsz /= hfs->fs_mdb.m_AlBlkSiz;

  original = *sizep;
  error = er_expand(hfs, bt->b_exts, bt->b_erec, 0, clmpsz, clmpsz, sizep);

  /*
   * Btree allocation data is in the MDB.  Mark the MDB as allocation dirty.
   */
  FS_SET(hfs,MDB_ALC);

  delta = BTOBB(*sizep-original);

  bt->b_hdr.bh_NNodes += delta;
  bt->b_hdr.bh_Free   += delta;

  BT_SET(bt,ATR);

  return error;
}


/*************
 * update_tree	Update parts of btree as needed.
 *============
 *
 * NOTE: We don't necessarily sync everthing that's dirty because we wish
 *       to manage allocation updates separately from attribute updates.
 */
int update_tree(bt_t *bt, u_int flags){
  rdesc_t  rd;
  int      error = 0;
  nd_t *np   = 0;
  int      code  = 0;

  assert(flags & (ALC|ATR));

  /*
   * There are two control flags and two different types of dirt, for a
   * total of 16 possibilities although only 7 are interesting.  Construct a
   * switch code from (ALC,ATR,HDR_DIRTY,MAP_DIRTY) and dispatch.
   */

  if (flags & ALC)
    code |= 8;
  if (flags & ATR)
    code |= 4;
  if (BT_ISSET(bt,HDR_DIRTY))
    code |= 2;
  if (BT_ISSET(bt,MAP_DIRTY))
    code |= 1;

  switch(code){
  case 0:			/* */
  case 1:			/* MAP_DIRTY */
  case 2:			/* HDR_DIRTY */
  case 3:			/* HDR_DIRTY, MAP_DIRTY */
  case 4:			/* ATR */
  case 5:			/* ATR, MAP_DIRTY */
  case 8:			/* ALC */
  case 10:			/* ALC, HDR_DIRTY */
  case 12:			/* ALC, ATR */
    break;

  case 6:			/* ATR, HDR_DIRTY */
  case 7:			/* ATR, HDR_DIRTY, MAP_DIRTY */
  case 14:			/* ALC, ATR, HDR_DIRTY */
  case 15:			/* ALC, ATR, HDR_DIRTY, MAP_DIRTY*/
    TRACE(("bt_update:   updating hdr\n"));
    __chkr(bt->b_hfs,
	   nd_get(bt,0,&np),
	   bt_update);
    nth_record(np,0,&rd);
    /* The header record ought to have n_Flink set correctly */
    /* to point to the first Map node if it exists */
    np->n_Flink=(bt->b_maplist.ml_head == 0) ? 0: bt->b_maplist.ml_head->m_num;
    pack(&bt->b_hdr,rd.data,hdrpack);
    BT_CLR(bt,HDR_DIRTY);
    if (code==15)
      goto both;
    break;

  case 9:			/* ALC, MAP_DIRTY */
  case 11:			/* ALC, HDR_DIRTY, MAP_DIRTY*/
  case 13:			/* ALC, ATR, MAP_DIRTY */
    __chkr(bt->b_hfs,
	   nd_get(bt,0,&np),
	   bt_update);
  both:

    TRACE(("bt_update:   updating map\n"));

    nth_record(np,2,&rd);
    bcopy(&bt->b_map,rd.data,sizeof bt->b_map);
    BT_CLR(bt,MAP_DIRTY);
    break;
  }


 done:
  if (np)
    N_SET(np,DIRTY);
  return error;
}


	/***************************/
	/***** RECORD ROUTINES *****/
	/***************************/

/*************
 * find_record	Search btree for a record.
 */
static int find_record(bt_t *bt,
		       btk_t *key,
		       rdesc_t *rd,
		       lineage_t *ln,
		       int *match){
  int error;
  nd_t *np;

  INVALIDATE_KHDR(key->ck.k_hdr);

  ln->depth = 0;
  LN_NODE(ln) = (u_int)bt->b_hdr.bh_Root;
  LN_REC(ln) = 999;

  /*
   * Fetch the current node.
   * Call nd_search to look for the key and adjust the lineage structure.
   * If it's a leaf node then return.
   * Otherwise fetch the next node.
   */
  while(!(error = __chk(bt->b_hfs,
			nd_get(bt,LN_NODE(ln),&np),
			find_record))){
    *match = nd_search(np,key,rd,ln);
    if (LEAFNODE(np))
      break;
    else
      /*
       * With a corrupted file system, the tree can appear to decend
       * forever.  If we are about to overflow the lineage structure
       * break out and signal a corrupted file system.
       */
      if (ln->depth >= (LN_MAX-1)){
	__chk(bt->b_hfs,EIO,find_record);
	  break;
      }
  }

  /* If succesful, update the key header */

  if (!error){
    key->ck.k_hdr.kh_node = LN_NODE(ln);
    key->ck.k_hdr.kh_rec  = LN_REC(ln);
    key->ck.k_hdr.kh_gen  = bt->b_gen;
  }
  return error;
}


/***************
 * delete_record	Delete a record from the btree.
 */
static int delete_record(bt_t *bt, lineage_t *ln){
  int   error=0;
  nd_t  *np;

#ifdef TRACEREC
  TRACE(("delete_record: (%d,%d)\n", LN_NODE(ln), LN_REC(ln)));
#endif

  /*
   * Cancel all cached keys by incrementing the btree generation number.
   */
  bt->b_gen++;

  /*
   * Get the node with the record to delete.
   * Remove the record.
   * If a leaf node, decrement the record count in the btree header.
   * Make the btree header ATR dirty.

   */
  __chkr(bt->b_hfs,
	 nd_get(bt,LN_NODE(ln),&np),
	 delete_record);
  remove_nth_record(np,LN_REC(ln));
  if (LEAFNODE(np)){
    assert(bt->b_hdr.bh_NRecs!=0);
    bt->b_hdr.bh_NRecs--;
    BT_SET(bt,ATR);
  }

  /*
   * Tree restructuring checks.
   *
   * Right now we just check to see if we deleted the last record and
   * if so, we delete the node.  :randyh   We need to look into merging
   * nodes to keep the tree well balanced.
   */
  if (np->n_NRecs==0)
    return __chk(bt->b_hfs,
		 nd_remove(np,ln),
		 delete_record);
  
  /*
   * If this was the first record we propogate the new key upwards.
   */
  if (LN_REC(ln) == 0)
    __chkr(bt->b_hfs,
	   nd_update_parent(np,ln),
	   delete_record);

  /*
   * If we are a root index node and have only one child,
   *   we promote the child to be root.
   */
  if (ln->depth==0 && np->n_NRecs==1 && INDEXNODE(np))
    error = __chk(bt->b_hfs,
		  nd_promote(np,ln),
		  delete_record);
  return error;
}


/************
 * add_record	Add a record
 */
static int add_record(nd_t *np, rdesc_t *rp, int match, lineage_t *ln){
  int error=0;

#ifdef TRACEREC
  TRACE(("add_record: (%d,%d)%d\n", LN_NODE(ln), LN_REC(ln), match));
#endif

  /*
   * A positive "match" means that the record should follow the one
   *   specified in the lineage.
   */
  if (match>0)
    LN_REC(ln)++;

  /*
   * Cancel all cached keys by incrementing the btree generation number.
   */
  np->n_bt->b_gen++;

 again:
  /*
   * Check for enough immediate room.  If so, insert the node directly.
   */
  if (CAPACITY(np) > rp->count+2){
    /*
     * Insert the record.
     * If a leaf node, update the record count.
     * If record 0 and not root node, update the parents.
     */
    insert_nth_record(np,LN_REC(ln),rp);
    if (LEAFNODE(np)){
      np->n_bt->b_hdr.bh_NRecs++;
      BT_SET(np->n_bt,ATR);
    }
    if (LN_REC(ln)==0)
      error= __chk(np->n_bt->b_hfs,
		   nd_update_parent(np,ln),
		   add_record);
    return error;
  }

  /*
   * Otherwise split the node and try again.
   */
  __chkr(np->n_bt->b_hfs,
	 nd_split(np,ln,&np),
	 add_record);
  goto again;
}


/***************
 * nth_record	Locate the n'th record in a node.
 */
static void nth_record(nd_t *np, u_int n, rdesc_t *rp){
  u_short *bp = np->n_buf;
  assert(n<np->n_NRecs);
  rp->data = (void*)(( (char*)bp) + bp[255-n] );
  rp->count= bp[254-n] - bp[255-n];
}


/*******************
 * insert_nth_record	Insert record at n'th position.
 */
static void insert_nth_record(nd_t *np, u_int n, rdesc_t *rd){
  u_short *bp = (u_short*)np->n_buf;
  char    *from = (char*)bp+bp[255-n];
  char    *to   = from + (rd->count);
  int     cnt   = bp[255-np->n_NRecs] - bp[255-n];
  int     i;

  assert (((rd->count & 1)==0) && ( n <= np->n_NRecs));
  memmove(to,from,cnt);	
  memmove(from,rd->data,rd->count);
  for (i=np->n_NRecs;i>=(int)n;i--)
    bp[254-i]=bp[255-i]+rd->count;

  np->n_NRecs++;
  N_SET(np,DIRTY);
}


/*******************
 * remove_nth_record	Delete n'th record from a node
 */
static void remove_nth_record(nd_t *np, u_int n){
  u_short *bp   = np->n_buf;
  char    *to   = (char*)bp + bp[255-n];
  char    *from = (char*)bp + bp[254-n];
  char    *free = (char*)bp + bp[255 - np->n_NRecs];
  int i;
  
  assert(n<np->n_NRecs);
  
  memmove(to, from, free-from);
  
  for (i=n;i<np->n_NRecs;i++)
    bp[255-i]=bp[254-i]-(from-to);


  np->n_NRecs--;
  N_SET(np,DIRTY);
}


	/*************************/
	/***** NODE ROUTINES *****/
	/*************************/


/*************
 * nd_search	Search node for a target record by key.
 *============
 *
 * Returns:
 *       0:  T = R'n, lineage->rc[D].rec = n
 *       1:  T > R'n, lineage->rc[D].rec = n
 *	-1:  T < R'0, lineage->rc[D].rec = 0
 *
 *       For Index nodes only:
 *
 *       lineage->depth = D+1
 *       lineage->rc[D+1].node = N
 *
 *
 * where:
 *	D     is lineage->depth upon entry,
 *	T     is the target record
 *	R'n   is the n'th record in the node.
 *      N     is the contents of index node record R'n
 *
 */
static int nd_search(nd_t *np, btk_t *key, rdesc_t *rd, lineage_t *ln){
  btk_t lk;
  int   cc;
  u_int ptr,prevptr;
  u_int *rec = &LN_REC(ln);

  assert(np->n_NRecs!=0);

  for (*rec=0; *rec<np->n_NRecs; *rec+=1){
    nth_record(np,*rec,rd);
    separate_key(np,rd,&lk);

    if (!LEAFNODE(np)){
      u_char *p = (u_char*)rd->data;
      ptr = p[0]<<24 | p[1]<< 16 | p[2]<<8 | p[3];
    }

    switch(cc=(*np->n_bt->b_compare)(key,&lk)){
    case -1:
      if (LEAFNODE(np)){
	if (*rec){
	  *rec -= 1;
	  return 1;
	}
	return cc;
      }
      goto descend;

    case 0:
      if (LEAFNODE(np))
	return cc;
      goto descend;

    case 1:
      if (!LEAFNODE(np))
	prevptr = ptr;
    }
  }
  
  *rec-=1; ptr = prevptr;

  if (LEAFNODE(np))
    return cc;

 descend:

  if (cc<0 && *rec){
    *rec -= 1;
    ptr = prevptr;
    cc = 1;
  }
    
  ln->rc[++ln->depth].node = ptr;
  return cc;
}


/*********
 * dump_nn	Dump a node by number.
 */
static int dump_nn(bt_t *bt, u_int nn){
  nd_t *np;
  int      error;
  if (error=nd_get(bt,nn,&np))
    return error;
  nd_dump(np,"");
  return (0);
}


/*********
 * nd_dump	Dump the contents of a node.
 *
 * NOTE:	Really only works for catalog nodes.
 */
static void nd_dump(nd_t *np, char *str){
  int i,ptr;
  btk_t lk;
  rdesc_t rd;

  printf("Dump Node- %s\n\tNode: %4d, Type: %d, Height: %d, Records: %2d\n"
	 "\tFlink: %4d, Blink %4d, LBN: %4d\n",
	 str,
	 np->n_num,
	 np->n_Type,
	 np->n_NHeight,
	 np->n_NRecs,
	 np->n_Flink,
	 np->n_Blink,
	 np->n_lbn);

  if (np->n_num==0){
    printf("  Header node\n");
    return;
  }

  for (i=0;i<np->n_NRecs;i++){
    nth_record(np,i,&rd);
    separate_key(np,&rd,&lk);
    if (np->n_bt->b_type==CATALOG){
      catk_t *ck = (catk_t*)&lk;
      printf("  %2d:  (%2d-%6d-%-31.*s) ",
	     i,
	     ck->k_key.k_KeyLen, 
	     CAT_PARENT(ck),
	     &ck->k_key.k_CName[0],
	     &ck->k_key.k_CName[1]);
      
      if (!LEAFNODE(np)){
	u_char *p = (u_char*)rd.data;
	ptr = p[0]<<24 | p[1]<< 16 | p[2]<<8 | p[3];
	printf(" -> %d\n", ptr);
      }
      else
	printf(" :: %d\n",
	       rd.count);
    }
    else{
      eofk_t *ek = (eofk_t*)&lk;
      printf("  %2d:  (%2d-%6d,%4d)",
	     i,
	     ek->k_key.k_KeyLen,
	     EOFK_FNUM(ek),
	     EOFK_FABN(ek));
      if (!LEAFNODE(np)){
	u_char *p = (u_char*)rd.data;
	ptr = p[0]<<24 | p[1]<< 16 | p[2]<<8 | p[3];
	printf(" -> %d\n", ptr);
      }
      else{
	ExtDescr_t *ext = (ExtDescr_t*)rd.data;
	int j;
	for (j=0;j<3;j++,ext++)
	  printf(" (%d,%d)",
		 ext->ed_StABN,
		 ext->ed_NumABlks);
	printf("\n");
      }
    }
    
  }
  printf("\n");
}


#define REMOVE_ACTV_BTN(np) hl_remove((hlnode_t*)np)
#define FREE_BTN(bt,np)     hl_insert_free(&bt->b_hl,(hlnode_t*)np)
#define GET_FREE_BTN(bt)    (nd_t*)hl_get_free(&bt->b_hl)
#define ACTV_BTN(bt,np)	    hl_insert(&bt->b_hl,(hlnode_t*)np)
#define REMOVE_FREE_BTN(np) hl_remove_free((hlnode_t*)np)


/********
 * btncmp	Returns true if node matches
 */
static int btncmp(register nd_t *np, register u_int node){
  return np->n_num == node;
}


/********
 * nd_get	Get a btree node by node number.
 *
 * NOTE:	For nodes, we want to treat the hashlist like a pure cache.
 *		That is, nodes are entered on the hashlist for rapid
 *		Searching but they are also entered on the freelist so any
 *		node may be grabed as a free node in an LRU fashion.
 */
static int nd_get(bt_t *bt, int node, nd_t **npp){
  int error;
  nd_t *np;
  
  /*
   * Try to find the node in the hash list.
   */
  np = (nd_t*)hl_find(&bt->b_hl,
			  node,
			  (void*)node,
			  (int (*)(hlnode_t*,void*))btncmp);
  if (np){
    /*
     * Remove from free list and then put it back.  This moves it to
     * to the end of the LRU list.
     */
    assert(N_SET(np,ACTIVE));
    REMOVE_FREE_BTN(np);
    FREE_BTN(bt,np);
    *npp = np;
    return 0;
  }

  /* 
   * Get a free node structure.
   * Read in the node.
   * If we fail, give this entry a bogus node number.
   */
  nd_new(bt,&np);
  np->n_num = node;
  if (error = nd_read(np)){
    nd_free(np);
    return __chk(bt->b_hfs,error,nd_get);
  }

  /*
   * Activate the node and then free it.  That puts it in the hash
   * list but as a free node.
   */
  ACTV_BTN(bt,np);
  FREE_BTN(bt,np);
  N_SET(np,ACTIVE);
  *npp = np;
  return 0;
}



/*********
 * nd_free	Return a node structure to the free pool.
 */
static void nd_free(nd_t *np){
  N_CLR(np,ACTIVE);
  REMOVE_ACTV_BTN(np);
  FREE_BTN(np->n_bt,np);
}


/********
 * nd_new	Get a node structure from the pool.
 */
static void nd_new(bt_t *bt, nd_t **nnp){
  int error;
  nd_t *np = GET_FREE_BTN(bt);
  if (np==NULL)
    panic("bt-nd_read: out of btnodes\n");
  __chk(bt->b_hfs,
	nd_reclaim(bt,np),
	nd_get);
  N_CLR(np,ACTIVE);
  *nnp = np;
}




/* Structure for unpacking/packing node header */

#define BTNODEOFF(m)   sizeof(((nd_t*)0)->m), offsetof(nd_t,m)
#define BTNODEOFFn(m)  n*sizeof(((nd_t*)0)->m), offsetof(nd_t,m)

static u_short btnodepack[]={
  BTNODEOFF(n_Flink),
  BTNODEOFF(n_Blink),
  BTNODEOFF(n_Type),
  BTNODEOFF(n_NHeight),
  BTNODEOFF(n_NRecs),
  BTNODEOFF(n_Resv2),
  0,0
};


/***********
 * nd_read	Read a node in from disk.
 */
static int nd_read(nd_t *np){
  u_int lbn, dummy;
  int error;

  /*
   * Translate the node number into a logical block number.
   * Read the block.
   * Unpack the node header.
   *
   * NOTE:  The io_lread below "knows" that the node is one BB.
   */
  if (error=ntolb(np->n_bt,
		  np->n_bt->b_exts,
		  np->n_bt->b_erec,
		  np->n_num,
		  &lbn,
		  &dummy))
    goto err;
  if (error=io_lread(np->n_bt->b_hfs, lbn, sizeof np->n_buf, np->n_buf))
    goto err;
  np->n_lbn = lbn;
  unpack(np->n_buf,np,btnodepack);

  return 0;

 err:
  return __chk(np->n_bt->b_hfs,
	       error,
	       nd_read);
}


/**************
 * nd_reclaim	Reclaim a btree node.
 */
static int nd_reclaim(bt_t *bt, nd_t *np){
  int error;
  REMOVE_ACTV_BTN(np);
  if (N_ISCLR(np,ACTIVE))
    return 0;
  error = nd_update(bt,np,0);

  N_CLR(np,ACTIVE);
  return __chk(bt->b_hfs,error,nd_reclaim);
}


/*************
 * nd_update	Update node on disk
 */
static int nd_update(bt_t *bt, nd_t *np, u_int flags){
  int error;
  /*
   * If no flags are set, we check to see if the file system is allocation
   * "dirty".  If so, call fs_sync(,ALC) and exit.  fs_sync(,ALC) will
   * result in us being called again with the ALC flag set.  We will update
   * the node then.
   */
  if (flags==0)
    if (FS_ISSET(bt->b_hfs,VBM_ALC)){
      TRACE(("nd_update: calling fs_sync\n"));
      return __chk(bt->b_hfs,
		   fs_sync(bt->b_hfs,ALC),
		   nd_update);
    }

  /* If we're not dirty, don't do anything */

  if (N_ISCLR(np,DIRTY))
    return 0;


  /*
   * Pack the node header stuff into the buffer.
   * Write out the node.
   * Mark the node as clean.
   */
  pack(np,np->n_buf,btnodepack);
  error = io_lwrite(bt->b_hfs, np->n_lbn, sizeof np->n_buf, np->n_buf);
  N_CLR(np,DIRTY);
  return __chk(bt->b_hfs,
	       error,
	       nd_update);
}


/*************
 * nd_remove	Delete a node from the tree.
 */
static int nd_remove(nd_t *np, lineage_t *ln){
  int      error;
  nd_t *onp;
  bt_t     *bt = np->n_bt;

#ifdef TRACEREC
  TRACE(("nd_remove: (%d,-)\n", LN_NODE(ln)));
#endif

  /*
   * Update the Flink and Blink of any sibling nodes.
   * If our Flink and/or Blink is zero and we are a leaf node, update
   *   the first and/or last leaf node pointers in the btree header.
   */
  if (np->n_Flink){
    __chkr(bt->b_hfs,
	   nd_get(bt,np->n_Flink,&onp),
	   nd_remove);
    onp->n_Blink = np->n_Blink;
    N_SET(onp,DIRTY);
  }
  else
    if (np->n_NHeight==1 && bt->b_hdr.bh_LNode==np->n_num){
      bt->b_hdr.bh_LNode=np->n_Blink;
      BT_SET(bt,HDR_DIRTY);
    }

  if (np->n_Blink){
    __chkr(bt->b_hfs,
	   nd_get(bt,np->n_Blink,&onp),
	   nd_remove);
    onp->n_Flink = np->n_Flink;
    N_SET(onp,DIRTY);
  }
    if (np->n_NHeight==1  && bt->b_hdr.bh_FNode==np->n_num){
      bt->b_hdr.bh_FNode=np->n_Flink;
      BT_SET(bt,HDR_DIRTY);
    }

  /*
   * Unless we are the root node, we must delete the index record that
   * points to us.
   *
   * NOTE: The lineage structure may be altered by delete_record and must not
   *       be used after it is called.
   */
  if (ln->depth !=0){
    ln->depth--;
    __chkr(bt->b_hfs,
	   delete_record(bt,ln),
	   nd_remove);
  }


  /*
   * Return the node to the free node pool.
   */
  return __chk(bt->b_hfs,
	       nd_dealloc(np),
	       nd_remove);
}


/************
 * nd_dealloc	Return node to the free node pool.
 */
static int nd_dealloc(nd_t *np){
  u_int bit, map;
  mapent_t *me;
  bt_t *bt = np->n_bt;

  assert(N_ISSET(np,ACTIVE));
  assert(np->n_num<bt->b_hdr.bh_NNodes);

  /*
   * Update the node accounting.
   */
  if (np->n_num>=BT_MAX_DIRECT_NODE){
    /* The node that's being deallocated is mapped */
    /* in one of the map nodes. Find required map-entry */
    /* and clear the appropriate bit corresponding to it */
    map = (np->n_num-BT_MAX_DIRECT_NODE) / MAPNODE_BMAP_SIZE;
    bit = (np->n_num-BT_MAX_DIRECT_NODE) % MAPNODE_BMAP_SIZE;
    me  = bmap_maplist_get_mapent(bt, map);
    bmap_mapent_clr_bit(me, bit);
    BT_SET(bt, MAP_DIRTY);
    bt->b_hdr.bh_Free++;
    BT_SET(bt, ATR);
  }
  else{
    bit_clear(np->n_num,(char*)bt->b_map);
    BT_SET(bt,MAP_DIRTY);
    bt->b_hdr.bh_Free++;
    BT_SET(bt,ATR);
  }

  /*
   * Check to see if we are the root node.  If so, zero the depth
   *   and the root node fields in the header.
   */
  if (np->n_num==bt->b_hdr.bh_Root){
    bt->b_hdr.bh_Root=0;
    bt->b_hdr.bh_Depth=0;
  }


  /*
   * Zero incore node and leave it dirty and active.  Unless we reallocate
   * it, this will cause the zeroed node to eventually flush to disk.
   * Some disk checking programs  expect unused nodes to be zero.
   */
  np->n_Flink = np->n_Blink = 0;
  np->n_Type = np->n_NHeight = 0;
  np->n_NRecs = np->n_Resv2 = 0;

  bzero(np->n_buf,sizeof(np->n_buf));
  N_SET(np,DIRTY);

  return 0;
}


/**********
 * nd_alloc	Allocate a node from the node pool.
 */
static int nd_alloc(bt_t *bt, nd_t **nnp){
  nd_t *np;
  int error, nn, map, bit;
  mapent_t *me;
  u_int dummy, node;
  u_int oldnnodes, newnnodes;

  /*
   * Scan the node map for a free node.
   * Update the node accounting.
   */
retry:
  for (nn=0; nn < BT_MAX_DIRECT_NODE; nn++){
    if (ISCLR(((char*)bt->b_map),nn)){
      /* There exists a free bit in bit-map */
      /* Check to see if this points to a valid node */
      if (nn < bt->b_hdr.bh_NNodes){
        /* Free bit points to a valid node */
        /* Allocate a node, set the free bit */
        nd_new(bt,&np);
        nd_account(bt, np, nn);
        SET(((char*)bt->b_map),nn);
        BT_SET(bt,MAP_DIRTY);
        BT_SET(bt,ATR);
        *nnp = np;
        return 0;
      }
      else {
        /* Free bit points to an invalid node */
        /* Expand the B-Tree file and create Map Node if necessary */
        oldnnodes = bt->b_hdr.bh_NNodes;
        error = bt_expand(bt);
        if (error)
          return (error);
        newnnodes = bt->b_hdr.bh_NNodes;
        if (bmap_maplist_is_node_mapped(bt, newnnodes-1) == 0){
          /* Create new Map Node at location: oldnnodes */
          bmap_maplist_create_mapent(bt, &me, oldnnodes);
          bmap_maplist_add_mapent(bt, me);
        }
        /* Try to see if bit points to valid node */
        /* This may or may not succeed */
        goto retry;
      }
    }
  }
  /* There's no free bit in the header bit map */
  /* Search existing map nodes to see if there's */
  /* any free bit present somewhere */
  bmap_maplist_find_free_bit(bt, &map, &bit); 
  if (bit == -1){
    /* There is no free bit in any existing map */
    /* The B-Tree file has to be expanded       */
    oldnnodes = bt->b_hdr.bh_NNodes;
    error = bt_expand(bt);
    if (error)
      return (error);
    newnnodes  = bt->b_hdr.bh_NNodes;
    /* Create a new Map Node  */
    /* Allocate required node */
    /* In the new clump, first create the new Map Node */
    bmap_maplist_create_mapent(bt, &me, oldnnodes);
    bmap_maplist_add_mapent(bt, me);
    bit  = bmap_mapent_free_bit(me);
    map  = bmap_mapent_get_mapindx(me);
    node = bmap_maplist_cvt_bit_to_node(map, bit);
    bmap_mapent_set_bit(me, bit);
    /* Allocate this node */
    /* Accounting for this newly allocated node is to be done */
    nd_new(bt, &np);
    nd_account(bt, np, node);
    *nnp = np;
    return 0;
  }
  else {
    /* There is a free bit in some existing map */
    /* See if this free bit points to a valid node */
    if (bmap_maplist_is_bit_valid(bt, map, bit)){
      /* This bit points to a valid node */
      /* Set this bit and allocate the node */
      node = bmap_maplist_cvt_bit_to_node(map, bit); 
      bmap_maplist_set_bit(bt, map, bit); 
      /* Allocate a new node */
      /* Accounting for this newly allocated node is to be done */
      nd_new(bt, &np); 
      nd_account(bt, np, node);
      *nnp = np;
      return 0;
    }
    else {
      /* The free bit points to hyper-space */
      /* The B-Tree file has to be expanded */
      oldnnodes = bt->b_hdr.bh_NNodes;
      error = bt_expand(bt);
      if (error)
        return (error);
      newnnodes = bt->b_hdr.bh_NNodes;
      /* A new Map node might have to be created */
      if (bmap_maplist_is_node_mapped(bt, newnnodes-1) == 0){
        /* Create a new Map Node  */
        /* This new Map Node has all clear bits */
        /* The location of this is: oldnnodes+1 */
        bmap_maplist_create_mapent(bt, &me, oldnnodes+1);  
        bmap_maplist_add_mapent(bt, me);
      }
      /* There are enough Map nodes */
      /* There are enough valid nodes */
      /* The Free bit should be pointing to a valid node now */
      node = bmap_maplist_cvt_bit_to_node(map, bit);
      bmap_maplist_set_bit(bt, map, bit);
      /* Allocate a new node */
      /* Accounting for this newly allocated node is to be done */
      nd_new(bt, &np);
      nd_account(bt, np, node);
      *nnp = np;
      return 0;
    }
  }
}

/********************
 * nd_update_parent	Change index pointing to child node.
 */
static int nd_update_parent(nd_t *np, lineage_t *ln){
  int      error;
  rdesc_t  rd;
  catk_t   k;
  nd_t *pnp;

#ifdef TRACEREC
  TRACE(("update_parent: (%d,%d)\n", LN_NODE(ln), LN_REC(ln)));
#endif
  
  /* If this is the root node, then it has no parent. */

  if (ln->depth==0)
    return 0;

  /*
   * Extract the key we are propogating upwards.
   */
  nth_record(np,0,&rd);
  separate_key(np,&rd,(btk_t*)&k);

  /*
   * Move up one level and locate parent's record.
   */
  ln->depth--;

#ifdef TRACEREC
  TRACE(("nd_update_parent: (%d,%d)\n", LN_NODE(ln), LN_REC(ln)));
#endif


  __chkr(np->n_bt->b_hfs,
	 nd_get(np->n_bt,LN_NODE(ln),&pnp),
	 nd_update_parent);
  nth_record(pnp,LN_REC(ln),&rd);
        
  /*
   * Copy the key image.
   * 
   * NOTE:  We assume that this record is a catalog record. However, extent
   *        file keys have the same structure, differing only in
   * 	    the key length so this code works for them as well.
   */
  bcopy(&k.k_key,rd.data,k.k_key.k_KeyLen+1);

  /*
   * Make the parent node dirty and then check to see if we must
   *  propagate the change higher.
   */
  N_SET(pnp,DIRTY);
  if (LN_REC(ln)==0  && ln->depth)
    return __chk(pnp->n_bt->b_hfs,
		 nd_update_parent(pnp,ln),
		 nd_update_parent);
  /*
   * Restore the lineage depth.
   */
  ln->depth++;
  return 0;
}


/*******
 * ntolb	Map node number to logical block
 */
static int ntolb(bt_t         *bt,
		 ExtDescRec_t *exts,
		 erec_t       *ep,
		 u_int        n,
		 u_int        *block,
		 u_int        *nblks){
  u_int n_per_alb = bt->b_hfs->fs_mdb.m_AlBlkSiz/bt->b_hdr.bh_NodeSize;
  u_int lb_per_alb= bt->b_hfs->fs_mdb.m_AlBlkSiz/BBSIZE;
  u_int alb_lb_offset = bt->b_hfs->fs_mdb.m_AlBlSt;
  u_int albn, x, albns;
  int error;

  /*
   * First convert a node offset to an alb offset.
   * Next map the alb offset to an albn.
   */
  x = n/n_per_alb;
  __chkr(bt->b_hfs,
	 er_toalbn( x, exts, ep, &albn, &albns),
	 ntolb);
  /*
   * Convert the albn to a logical block number and adjust for n/per alb.
   */
     *block = (albn * lb_per_alb) + (n % n_per_alb) + alb_lb_offset;
     *nblks = albns * lb_per_alb - (n % n_per_alb);
     return 0;
}


/**************
 * nd_promote	Promotes a solitary child of root to root.
 */
static int nd_promote(nd_t *np, lineage_t *ln){
  int      error;
  u_char   *p;
  u_int	   newroot;
  btk_t    key;
  rdesc_t  rd;
  nd_t *nnp;
  bt_t     *bt = np->n_bt;

#ifdef TRACEREC
  TRACE(("promote: (%d,%d)\n", LN_NODE(ln), LN_REC(ln)));
#endif

  assert(ln->depth==0 &&
	 (INDEXNODE(np) || (LEAFNODE(np) && np->n_NHeight==1)) &&
	 np->n_NRecs==1 &&
	 np->n_Flink==0 &&
	 np->n_Blink==0 );

  /*
   * Extract the child node number.
   * Fetch the child node.
   * Remove the root node.
   */
  nth_record(np,0,&rd);
  separate_key(np,&rd,&key);
  p = (u_char*)rd.data;
  newroot = p[0]<<24 | p[1]<< 16 | p[2]<<8 | p[3];

#ifdef TRACEREC
  TRACE(("nd_promote: root: %d, newroot: %d\n", np->n_num, newroot));
#endif

  __chkr(bt->b_hfs,
	 nd_get(bt,newroot,&nnp),
	 nd_promote);

  assert(nnp->n_NHeight == np->n_NHeight-1);

  /*
   * Adjust btree depth and root pointer.
   * Call nd_remove to release the old root node.
   *
   * NOTE: We must adjust the depth and root pointer first or nd_remove
   *       will think it's deleting the rootnode and will set the
   *       tree depth and root pointer both to zero.
   */
  bt->b_hdr.bh_Depth--;
  bt->b_hdr.bh_Root=newroot;
  BT_SET(bt,HDR_DIRTY);

  __chkr(bt->b_hfs,
	 nd_remove(np,ln),
	 nd_promote);

  /*
   * The new root node may also only have a single child.  
   */
  if (nnp->n_NRecs==1)
    return nd_promote(nnp,ln);
  return 0;
}


/**********
 * nd_split	Split a node into two nodes.
 */
int nd_split(nd_t *np, lineage_t *ln, nd_t **nnpp){
  int error;
  nd_t *nnp, *tnp;
  rdesc_t  rd;
  char     tmp[sizeof(btk_t)+4];
  bt_t     *bt=np->n_bt;
  int      ln_node, ln_rec, newnodeflag;
  static int nd_split_recursion = 0;

#ifdef TRACEREC
  TRACE(("nd_split: (%d,%d)\n", LN_NODE(ln), LN_REC(ln)));
#endif

  /*
   * NOTE:  -- Potential race condition --
   *
   * Once we split a node, we have to clean up the btree before any further
   * btree operations are allowed.  During node juggling, a node may be
   * reclaimed and then may cause a fs_sync.  A fs_sync may in turn result
   * in frec or erec updates which may break if the tree isn't intact.  To
   * avoid this possibility, we call fs_sync first, flushing everything to
   * disk now.  This ensures that no frec or erec updates will be triggered
   * as a result of any node reclaims.
   */
  if (nd_split_recursion++)
      fs_sync(bt->b_hfs, 0);
  nd_split_recursion--;

 again:
  /*
   * Allocate a free node.
   * Initialize the node structure.
   */
	error = nd_alloc(bt, &nnp);

	if (error) {
		if (error != ENOSPC)
			fs_corrupted(bt->b_hfs, "nd_split");

		return error;
	}

  /*
   * Handle a split of the root node separately.  We create a new
   *   index node above the current root node and then split the
   *   current node normally.
   */
  if (ln->depth==0){
    __chkr(bt->b_hfs,
	   nd_new_root(nnp,np,ln),
	   add_record);
    goto again;
  }



  /*
   * Copy records from the original record until the capacity of
   * the new one is lower than the original.
   */
  nnp->n_Type = np->n_Type;
  nnp->n_NHeight = np->n_NHeight;
  while (CAPACITY(nnp)>CAPACITY(np)){
    nth_record(np,np->n_NRecs-1,&rd);
    insert_nth_record(nnp,0,&rd);
    remove_nth_record(np,np->n_NRecs-1);
  }
  
  /*
   * Update node links.
   */
  nnp->n_Flink = np->n_Flink;
  np->n_Flink = nnp->n_num;
  nnp->n_Blink = np->n_num;
  if (nnp->n_Flink){
    __chkr(bt->b_hfs,
	   nd_get(bt,
		  nnp->n_Flink,
		  &tnp),
	   nd_split);
    tnp->n_Blink = nnp->n_num;
    N_SET(tnp,DIRTY);
  }


  /*
   * Check and update the last leaf node pointer in the btree header if
   *   necessary.
   */
  if (LEAFNODE(np) && bt->b_hdr.bh_LNode==np->n_num){
    bt->b_hdr.bh_LNode=nnp->n_num;
    BT_SET(bt,ATR);
  }


  /*
   * Mark the initial and new node as modified.
   */
  N_SET(np,DIRTY);
  N_SET(nnp,DIRTY);
  N_SET(nnp,ACTIVE);

  /*
   * Now insert an index record in our parent.
   */
  rd.data = tmp;
  rd.count=0;
  cons_ptr(nnp,&rd);

  /*
   * NOTE: The act of inserting a record above the current level
   *       will modify lineage to point to that record, which is not
   *       necessarily the proper lineage of the leaf record pointed
   *       to by lineage now.  Save the lineage entry above us, insert
   *       the pointer to the new node and then restore the original
   *       lineage entry.  We will figure out what the proper
   *       lineage is below.
   */
  newnodeflag = LN_REC(ln)>=np->n_NRecs;
  ln->depth--;
  ln_node = LN_NODE(ln);
  ln_rec  = LN_REC(ln);
  __chkr(bt->b_hfs, nd_get(bt,LN_NODE(ln),&tnp), nd_split);
  error=add_record(tnp, &rd, (tnp->n_NRecs==0)?0 : 1, ln);

  /*
   * Adjust lineage to reflect the split.
   */
  if (newnodeflag){
    /*
     * Our original target is now in the new node.  Update the target
     * record and node index.
     */
    ln->depth++;
    LN_REC(ln) -= np->n_NRecs;
    LN_NODE(ln) = nnp->n_num;
    *nnpp = nnp;
  }
  else{
    /*
     * Our original target is still in the original node.  Restore our
     *   ancestral node and record index.
     */
    LN_NODE(ln)=ln_node;
    LN_REC(ln)=ln_rec;
    ln->depth++;
    *nnpp = np;
  }

  return __chk(bt->b_hfs,error,nd_split);
}


/**********
 * cons_ptr	Construct a ptr record to a node.
 */
static void cons_ptr(nd_t *np, rdesc_t *rd){
  btk_t   key;
  rdesc_t lrd;
  u_char  t;

  /*
   * Extract the first key from the record.
   * Merge the key with the node number.
   */
  nth_record(np,0,&lrd);
  separate_key(np,&lrd,&key);
  lrd.data=&np->n_num;
  lrd.count=4;
  
  /*
   * Temporarily make this node an index node so that
   *   merge_key will not compress the key.
   */
  t = np->n_Type;
  np->n_Type=IndexNode;
  merge_key(np,&lrd,&key,rd);
  np->n_Type=t;
}


/*************
 * nd_new_root	Create a new root node
 */
static int nd_new_root(nd_t *rnp, nd_t *np, lineage_t *ln){
  bt_t    *bt = np->n_bt;
  rdesc_t rd;
  int     error,i;
  char    tmp[sizeof(btk_t)+4];

  rnp->n_Type = IndexNode;
  rnp->n_NHeight = np->n_NHeight+1;
  rnp->n_Flink = rnp->n_Blink = 0;
  bt->b_hdr.bh_Root=rnp->n_num;
  bt->b_hdr.bh_Depth++;
  if (rnp->n_NHeight==2)
    bt->b_hdr.bh_FNode = bt->b_hdr.bh_LNode = np->n_num;
  BT_SET(bt,ATR);
  
  /*
   * Push our lineage down one.
   */
  for (i=LN_MAX-1;i>0;i--){
    ln->rc[i].node = ln->rc[i-1].node;
    ln->rc[i].rec  = ln->rc[i-1].rec;
  }
  ln->rc[0].node = rnp->n_num;
  ln->rc[0].rec=0;
  ln->depth++;

  /*
   * Insert a pointer record to the original node.
   */
  rd.data=tmp; rd.count=0;
  cons_ptr(np,&rd);
  ln->depth--;
  error = add_record(rnp,&rd,0,ln);
  ln->depth++;
  N_SET(rnp,ACTIVE);
  return __chk(bt->b_hfs, error, nd_new_root);
}


/**************
 * nd_init_root	Create a root node.
 *=============
 *
 * NOTE: This routine is called when there is no root in the btree.  This
 *       can happen only for the extent overflow file.
 */
static int nd_init_root(bt_t *bt){
  int  error;
  nd_t *np;

  if (error=nd_alloc(bt,&np))
    return error;

  np->n_Type = LeafNode;
  np->n_NHeight = 1;
  np->n_Flink = np->n_Blink = 0;

  bt->b_hdr.bh_Root=np->n_num;
  bt->b_hdr.bh_Depth++;
  bt->b_hdr.bh_FNode = np->n_num;
  bt->b_hdr.bh_LNode = np->n_num;

  N_SET(np,ACTIVE);
  N_CLR(np,DIRTY);

  BT_SET(bt,ATR);

  return 0;
}

	/************************/
	/***** KEY ROUTINES *****/
	/************************/


/***********
 * merge_key	Merge key with record.
 *==========
 * NOTE:	We merge by copying the key and the record into a new
 *              buffer.
 */
static void merge_key(nd_t *np, rdesc_t *rp, btk_t *k, rdesc_t*nrp){
  btk_t    key;
  int kl = k->ck.k_key.k_KeyLen+1;

  /*
   * Make copy of key.
   */
  bcopy(k,&key,sizeof *k);

  /*
   * For catalog record leaf nodes, the key is shortened to suppress trailing
   * nulls in the key.
   */
  if (CATALOGNODE(np) && LEAFNODE(np)){
    int kincr = KEY_CNAME_LEN-key.ck.k_key.k_CName[0];
    kl -= kincr;
    key.ck.k_key.k_KeyLen -= kincr;
  }

  /*
   * If the total key length is odd, increment it to "short" align it.
   * Copy the key to the new record.
   * Append the old record.
   */
  if (kl & 1)
    kl++;

  bcopy(&key.ck.k_key,nrp->data,kl);
  bcopy(rp->data,(char*)nrp->data+kl,rp->count);
  nrp->count = rp->count + kl;
}


/**************
 * separate_key	Extract a key from a record and update the rdesc.
 */
static void separate_key(nd_t *np, rdesc_t *rp, btk_t *key){

  /*
   * Extract the key length.
   * Copy the key from the record to the key structure.
   */
  int kl = *(u_char*)rp->data + 1;
  bcopy(rp->data, &key->ck.k_key, kl);


  /*
   * If the total key length is odd, increment it to "short" align it.
   * Adjust the record data pointer and count.
   */
  if (kl & 1)
    kl++;
  rp->data = (void*)((u_char*)rp->data + kl);
  rp->count -= kl;


  /*
   * A CATALOG leaf node is "compressed" on disk.  Uncompress it for
   * in-core use.
   */
  if (CATALOGNODE(np) && LEAFNODE(np)){
    int kp = key->ck.k_key.k_CName[0];
    int kincr = sizeof(key->ck.k_key.k_CName) - kp -1;
    bzero(&key->ck.k_key.k_CName[kp+1],kincr);
    key->ck.k_key.k_KeyLen += kincr;
    }
}


/*****************
 * cat_key_compare	Compare a catalog goal key against a test key.
 *================
 *
 * Return:	 0  	Keys match.
 *		-1	Goal key is less than test key.
 *		 1	Goal key is larger than test key.
 */
static int cat_key_compare(catk_t *g, catk_t *t){

  int r = CAT_PARENT(g) - CAT_PARENT(t);
  if (r==0)
    r = dir_mstr_cmp((char *) g->k_key.k_CName, (char *) t->k_key.k_CName);
  if (r<0)
    return -1;
  if (r>0) 
    return 1;
  return 0;
}


/*****************
 * eof_key_compare	Compare a eof goal key against a test key.
 *
 * Return:	 0  	Keys match.
 *		-1	Goal key is less than test key.
 *		 1	Goal key is larger than test key.
 */
static int eof_key_compare(eofk_t *g, eofk_t *t){

  int r;

  if (r=(EOFK_FNUM(g) - EOFK_FNUM(t)))
    goto done;
  
  if (r=(g->k_key.k_FkType - t->k_key.k_FkType))
    goto done;
  
  if (g->k_key.k_KeyLen==7)
    r = EOFK_FABN(g) - EOFK_FABN(t);

 done:
  if (r<0)
    return -1;
  if (r>0) 
    return 1;
  return 0;
}

/*
 *-----------------------------------------------------------------------------
 * bmap_mapist_sync()
 * This routine scans the maplist and writes out all dirty map-entries.
 *-----------------------------------------------------------------------------
 */
int bmap_maplist_sync(bt_t *bt)
{
  nd_t      *np;
  mapent_t  *me;

  for (me = BT_MLIST_HEAD(bt); me; me = me->next){
    if (me->m_dirty){
      /* This map-entry has been dirty'd */
      /* Pack it into a node-structure   */
      /* Write it out to disk, clear bit */ 
      bmap_mapent_fill_node(bt, me, &np);
      nd_write_map(bt, np); 
      me->m_dirty = 0;
      BT_SET(bt, HDR_DIRTY);
      BT_SET(bt, MAP_DIRTY);
    }
  }
  return 0;
}

/*
 *----------------------------------------------------------------------------
 * bmap_maplist_init()
 * This routine initializes the map node list in memory.
 *----------------------------------------------------------------------------
 */
int bmap_maplist_init(bt_t *bt)
{
  nd_t     *np;
  rdesc_t  rd;
  mapent_t *me;
  int      map_num;
  int      mapnode_num;
  int      total_map_nodes;
  int      total_mapped_nodes;
  int      num_mapnodes_needed;

  /*
   * Fetch node 0 which contains the header record and the map record.
   * Note: The nd_get code expects the node size to have already been set.
   *       However, that's set from the header record. Catch 22.  We set it
   *       to BBSIZE (512).  HFS always uses 512 anyway but should it change,
   *       it doesn't matter for node 0 but must be set properly for all
   *       other nodes.
   */
  bt->b_hdr.bh_NodeSize = BBSIZE;
  __chkr(bt->b_hfs, nd_get(bt, 0, &np), bmap_maplist_init);
  /* Clear the Map-list struct */
  /* Read in the header node */
  /* Read in the Map Nodes, one by one */
  /* Add Map Node data structures to the Map list */
  bmap_maplist_clear(bt);
  nth_record(np, 0, &rd);
  unpack(rd.data, &bt->b_hdr, hdrpack);
  nth_record(np, 2, &rd);
  bcopy(rd.data, &bt->b_map, sizeof(bt->b_map));
  BT_SET(bt,ACTIVE);
  BT_CLR(bt,DIRTY);
  mapnode_num = np->n_Flink;
  while (mapnode_num){
    __chk(bt->b_hfs, nd_get_map(bt, &np, mapnode_num), bmap_maplist_init);
    nth_record(np, 0, &rd);
    bmap_mapent_extract_node(&me, np, mapnode_num);
    bmap_maplist_add_mapent(bt, me);
    mapnode_num = me->m_Flink;
  }
  /* Check to see if this B-Tree file has nodes that */
  /* are unmapped. If so, go ahead and create the */
  /* required number of Map Nodes in the map-list */
  num_mapnodes_needed = bmap_maplist_need_mapnodes(bt); 
  if (num_mapnodes_needed){
    /* New Map Nodes need to be created to account for rest      */
    /* Created them one by one in the unmapped region            */
    /* new Map Node #0   - node location: total_mapped_nodes     */
    /* new Map Node #1   - node location: total_mapped_nodes+1   */
    /* new Map Node #2   - node location: total_mapped_nodes+2   */
    /* ......................................................    */
    /* new Map Node #x-1 - node location: total_mapped_nodes+x-1 */
    total_map_nodes = bt->b_maplist.ml_num;
    total_mapped_nodes = BT_MAX_DIRECT_NODE+total_map_nodes*MAPNODE_BMAP_SIZE;
    for (map_num = 0; map_num < num_mapnodes_needed; map_num++){
      bmap_maplist_create_mapent(bt, &me, total_mapped_nodes+map_num);
      bmap_maplist_add_mapent(bt, me);
    }
    /* We now have additional map nodes to govern entire file */
  }
  return 0;
}

/*
 *----------------------------------------------------------------------------
 * bmap_mapent_extract_node()
 * This routine allocates a map entry and copies the node into it.
 *----------------------------------------------------------------------------
 */
int bmap_mapent_extract_node(mapent_t **me, nd_t *np, int nodenum)
{
  mapent_t      *_me;
  rdesc_t       map_rd;

  _me = (mapent_t *) malloc(sizeof(mapent_t));
  _me->m_Flink   = np->n_Flink;
  _me->m_Blink   = np->n_Blink;
  _me->m_Type    = np->n_Type;
  _me->m_NHeight = np->n_NHeight;
  _me->m_num     = np->n_num;
  _me->m_lbn     = np->n_lbn;
  _me->m_NRecs   = 1;
  _me->m_Resv2   = 0;
  _me->m_dirty   = 1;
  _me->next      = 0;
  nth_record(np, 0, &map_rd);
  bcopy(map_rd.data, &_me->m_bmap, sizeof(_me->m_bmap));
  *me = _me;
  return 0;
}

/*
 *----------------------------------------------------------------------------
 * bmap_mapent_fill_node()
 * This routine allocates a node and copies a map entry into it.
 *----------------------------------------------------------------------------
 */
int bmap_mapent_fill_node(bt_t *bt, mapent_t *me, nd_t **nnp)
{
  nd_t  *np;
  u_short *bp;
  u_int   dummy;
  rdesc_t  map_rd;

  np = (nd_t *) safe_malloc(sizeof(nd_t));
  np->n_Flink   = me->m_Flink;
  np->n_Blink   = me->m_Blink;
  np->n_Type    = me->m_Type;
  np->n_NHeight = me->m_NHeight;
  np->n_num     = me->m_num;
  np->n_NRecs   = 1;
  np->n_Resv2   = 0;
  ntolb(bt, bt->b_exts, bt->b_erec, me->m_num, &np->n_lbn, &dummy);
  /* The Bit Map has to be copied into the Map Node */
  /* The offsets have to be set correctly in the Map Node */
  /* The map entry's bit-map is copied into the node */
  /* The two offsets have to be filled in */ 
  bzero((void *) np->n_buf, sizeof(np->n_buf));
  bp = np->n_buf;	/* Point it towards the node buffer */
  bp[254] = 508;        /* Record offset for #1: Points to itself  */
  bp[255] = 14;         /* Record offset for #0: Points to bit-map */
  nth_record(np, 0, &map_rd);
  bcopy(&me->m_bmap, (void *) map_rd.data, sizeof(me->m_bmap));
  *nnp = np;
  return 0;
}

/*
 *-----------------------------------------------------------------------------
 * bmap_maplist_add_mapent()
 * This routine is used to add a map entry to the existing map list.
 *-----------------------------------------------------------------------------
 */
int bmap_maplist_add_mapent(bt_t *bt, mapent_t *me)
{
  me->m_indx = bt->b_maplist.ml_num++;
  if (BT_MLIST_HEAD(bt) == 0){
    me->m_Flink = 0;
    me->m_Blink = 0;
    BT_MLIST_HEAD(bt) = me;
    BT_MLIST_TAIL(bt) = me;
  }
  else {
    me->m_Flink = 0;
    me->m_Blink = BT_MLIST_TAIL(bt)->m_num;
    BT_MLIST_TAIL(bt)->next    = me;
    BT_MLIST_TAIL(bt)->m_Flink = me->m_num;
    BT_MLIST_TAIL(bt)          = me;
  }
  return 0;
}

/*
 *-----------------------------------------------------------------------------
 * bmap_maplist_clear()
 * This routine is used to clear the maplist structure in the B-Tree.
 *-----------------------------------------------------------------------------
 */
int bmap_maplist_clear(bt_t *bt)
{
  bt->b_maplist.ml_num  = 0;
  BT_MLIST_HEAD(bt) = 0;
  BT_MLIST_TAIL(bt) = 0;
  return 0;
}

/*
 *----------------------------------------------------------------------------
 * bmap_maplist_find_free_bit()
 * This routine traverses map list and finds free bit.
 * returns: map = map number containing free bit.
 * returns: bit = bit position inside map that's free.
 *----------------------------------------------------------------------------
 */
int bmap_maplist_find_free_bit(bt_t *bt, int *map, int *bit)
{
  int _map, _bit;
  mapent_t *me;

  me = BT_MLIST_HEAD(bt);
  while (me){
    /* Search this map entry for free bit */
    _bit = bmap_mapent_free_bit(me);
    if (_bit != -1){
      /* This point reached when some free bit found */
      /* in a particular map entry */
      *map = me->m_indx;
      *bit = _bit;
      return 0;
    }
    /* Didn't find any free bit in this entry */
    /* Scan the next map entry for free bits  */
    me = me->next;
  }
  /* Didn't find any free bit in any map node */
  /* Return -1 for both map and bit as error  */
  *map = -1;
  *bit = -1;
  return 0;
}

/*
 *----------------------------------------------------------------------------
 * nd_get_map()
 * This routine is used to get a map node without caching it in the hashlist.
 * This routine is a variation of: nd_get()
 *----------------------------------------------------------------------------
 */
static int nd_get_map(bt_t *bt, nd_t **nnp, int node)
{
  int   error;
  nd_t  *np;
  u_int lbn, dummy;

  np = (nd_t *) safe_malloc(sizeof(nd_t));
  bzero((void *) np, sizeof(nd_t));
  np->n_num = node;
  ntolb(bt, bt->b_exts, bt->b_erec, node, &lbn, &dummy);
  error = io_lread(bt->b_hfs, lbn, sizeof(np->n_buf), np->n_buf);
  unpack(np->n_buf, np, btnodepack);
  np->n_num = node;
  np->n_lbn = lbn;
  *nnp = np; 
  return __chk(bt->b_hfs, error, nd_get_map);
}

/*
 *----------------------------------------------------------------------------
 * nd_write_map()
 * This routine is used to write out a map node to disk.
 *----------------------------------------------------------------------------
 */
static int nd_write_map(bt_t *bt, nd_t *np)
{
  int error;

  /* Pack header node stuff into buffer */
  /* Write buffer node to disk */
  pack(np, np->n_buf, btnodepack);
  error = io_lwrite(bt->b_hfs, np->n_lbn, sizeof(np->n_buf), np->n_buf);
  return __chk(bt->b_hfs, error, nd_write_map);
}

/*
 *----------------------------------------------------------------------------
 * nd_account()
 * This routine allocates a new node from the node pool and then performs
 * certain accounting functions for this node that's alloc'ed.
 *----------------------------------------------------------------------------
 */
static int nd_account(bt_t *bt, nd_t *np, int node)
{
  u_int dummy;

  /* Accounting for this newly allocated node is to be done */
  np->n_num   = node;
  np->n_NRecs = 0;
  np->n_Resv2 = 0;
  bzero((char*) &np->n_buf, sizeof np->n_buf);
  np->n_buf[255] = N_HDR_SIZE;
  __chkr(bt->b_hfs,
         ntolb(bt,bt->b_exts,bt->b_erec, node,&np->n_lbn,&dummy),
         nd_alloc);
  bt->b_hdr.bh_Free--;
  ACTV_BTN(bt,np);
  FREE_BTN(bt,np);
  N_SET(np,ACTIVE);
  return 0;
}

/*
 *-----------------------------------------------------------------------------
 * bmap_maplist_is_bit_valid()
 * This routine is used to see if a particular (map, bit) combination points
 * to a valid node in the B-Tree file or not.
 *-----------------------------------------------------------------------------
 */
int bmap_maplist_is_bit_valid(bt_t *bt, int map, int bit)
{
  int node;

  node = bmap_maplist_cvt_bit_to_node(map, bit);
  return (node < bt->b_hdr.bh_NNodes);
}

/*
 *-----------------------------------------------------------------------------
 * bmap_maplist_cvt_bit_to_node()
 * This routine is used to convert a (map, bit) combination to an appropriate
 * node number.
 *-----------------------------------------------------------------------------
 */
int bmap_maplist_cvt_bit_to_node(int map, int bit)
{
  return (BT_MAX_DIRECT_NODE+map*MAPNODE_BMAP_SIZE+bit);
}

/*
 *-----------------------------------------------------------------------------
 * bmap_maplist_set_bit()
 * This routine is used to set a bit in a particular map entry in map list.
 *-----------------------------------------------------------------------------
 */
int bmap_maplist_set_bit(bt_t *bt, int map, int bit)
{
  mapent_t *me;

  for (me = BT_MLIST_HEAD(bt); me; me = me->next){
    if (me->m_indx == map){
      /* Set bit in the map entry */ 
      bmap_mapent_set_bit(me, bit);
      return 0;
    }
  }
  return -1;
}

/*
 *-----------------------------------------------------------------------------
 * bmap_maplist_clr_bit()
 * This routine is used to clear a bit in a particular map entry in map list.
 *-----------------------------------------------------------------------------
 */
int bmap_maplist_clr_bit(bt_t *bt, int map, int bit)
{
  mapent_t *me;
  
  for (me = BT_MLIST_HEAD(bt); me; me = me->next)
    if (me->m_indx == map){
      /* Clear bit in the map entry */
      bmap_mapent_clr_bit(me, bit);
      return 0;
    }
  return -1;
}

/*
 *-----------------------------------------------------------------------------
 * bmap_maplist_is_node_mapped()
 * This routine is used to see if a particular node is already mapped or not.
 *-----------------------------------------------------------------------------
 */
int bmap_maplist_is_node_mapped(bt_t *bt, int node)
{
  int num_map_nodes;
  int num_nodes_mapped;

  num_map_nodes = bt->b_maplist.ml_num;
  num_nodes_mapped = (BT_MAX_DIRECT_NODE + MAPNODE_BMAP_SIZE*(num_map_nodes));
  return (node < num_nodes_mapped);
}

/*
 *-----------------------------------------------------------------------------
 * bmap_maplist_create_mapent()
 * This routine is used to create a map entry for a new Map Node.
 * New Map Node has the node number: node.
 *-----------------------------------------------------------------------------
 */
int bmap_maplist_create_mapent(bt_t *bt, mapent_t **me, int node)
{
  mapent_t *_me;
  mapent_t *_ml_tail;
  u_int    m_indx, m_bit, m_lbn, dummy;

  ntolb(bt, bt->b_exts, bt->b_erec, node, &m_lbn, &dummy);
  _ml_tail = BT_MLIST_TAIL(bt);
  _me = (mapent_t *) malloc(sizeof(mapent_t));
  _me->m_Blink   = (_ml_tail == 0) ? 0: _ml_tail->m_num;
  _me->m_Flink   = 0;
  _me->m_Type    = MapNode;
  _me->m_NHeight = 1;
  _me->m_NRecs   = 1;
  _me->m_Resv2   = 0;
  _me->m_indx    = bt->b_maplist.ml_num;
  _me->m_num     = node;
  _me->m_lbn     = m_lbn;
  _me->m_dirty   = 1;
  _me->next      = 0;
  bzero((void *) _me->m_bmap, MAPNODE_BMAP_SIZE/8);
  *me = _me;
  /* In addition to creating the map-entry, we also have to */
  /* set the bit that corresponds to the this particular node */
  if (node < BT_MAX_DIRECT_NODE){
    /* Set that particular bit in the bit-map */
    SET(((char*)bt->b_map), node);
    BT_SET(bt, MAP_DIRTY);
  }
  else {
    m_indx = (node-BT_MAX_DIRECT_NODE) / MAPNODE_BMAP_SIZE;
    m_bit  = (node-BT_MAX_DIRECT_NODE) % MAPNODE_BMAP_SIZE;
    /* Set that particular bit in the relevant Map Node */
    /* Check to see if this Map Node governs itself */
    if (_me->m_indx == m_indx){
      /* This Map Node is governed by itself */
      bmap_mapent_set_bit(_me, m_bit);
      BT_SET(bt, MAP_DIRTY);
    }
    else {
      /* This Map Node is governed by some other Map Node */
      bmap_maplist_set_bit(bt, m_indx, m_bit);
      BT_SET(bt, MAP_DIRTY);
    } 
  }
  return 0;
}

/*
 *-----------------------------------------------------------------------------
 * bmap_mapent_get_mapindx()
 * This routine is used to return the map number for a particular map entry.
 *-----------------------------------------------------------------------------
 */
int bmap_mapent_get_mapindx(mapent_t *me)
{
  return (me->m_indx);
}

/*
 *-----------------------------------------------------------------------------
 * bmap_mapent_set_bit() 
 * This routine is used to set a bit in a particular map entry.
 *-------------------------------- ---------------------------------------------
 */
int bmap_mapent_set_bit(mapent_t *me, int bit)
{

  me->m_dirty = 1; 
  bmap_set(me, bit);
  return 0;
}

/*
 *-----------------------------------------------------------------------------
 * bmap_mapent_clr_bit()
 * This routine is used to clear a bit in a particular map entry.
 *-----------------------------------------------------------------------------
 */
int bmap_mapent_clr_bit(mapent_t *me, int bit)
{
  me->m_dirty = 1;
  bmap_clr(me, bit);
  return 0;
}

/*
 *-----------------------------------------------------------------------------
 * bmap_mapent_free_bit()
 * This routine is used to find a free-bit in a particular map entry. 
 * If there is no free bit, -1 is returned.
 *-----------------------------------------------------------------------------
 */
int bmap_mapent_free_bit(mapent_t *me)
{
  u_int bit;

  for (bit = 0; bit < MAPNODE_BMAP_SIZE; bit++)
    if (ISCLR(((char*) me->m_bmap), bit))
      return (bit);
  return -1;
}

/*
 *-----------------------------------------------------------------------------
 * bmap_maplist_get_mapent()
 * This routine is used to return a pointer to a particular map entry for a 
 * given node.
 *-----------------------------------------------------------------------------
 */
mapent_t *bmap_maplist_get_mapent(bt_t *bt, int m_indx)
{
  mapent_t *me;

  for (me = BT_MLIST_HEAD(bt); me; me = me->next)
    if (me->m_indx == m_indx)
      return (me);
  return 0;
}

/*
 *----------------------------------------------------------------------------
 * bmap_maplist_need_mapnodes()
 * This routine is used to check if the existing B-Tree file needs any addit-
 * ional map nodes (during initialisation). Returns:
 * (=0) - No Map Nodes needed.
 * (>0) - Map Nodes needed, exact number returned.
 *----------------------------------------------------------------------------
 */
int bmap_maplist_need_mapnodes(bt_t *bt)
{
  int total_nodes;
  int total_map_nodes;
  int total_mapped_nodes;
  int total_unmapped_nodes;
  int num_needed_mapnodes;

  total_nodes          = bt->b_hdr.bh_NNodes;
  total_map_nodes      = bt->b_maplist.ml_num;
  total_mapped_nodes   = BT_MAX_DIRECT_NODE+total_map_nodes*MAPNODE_BMAP_SIZE;
  total_unmapped_nodes = total_nodes-total_mapped_nodes;
  if (total_unmapped_nodes > 0){
    num_needed_mapnodes = total_unmapped_nodes/MAPNODE_BMAP_SIZE+1;
    return (num_needed_mapnodes);
  }
  return 0;
}

