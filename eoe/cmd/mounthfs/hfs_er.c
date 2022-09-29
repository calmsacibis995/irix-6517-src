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

#ident "$Revision: 1.7 $"

#include <errno.h>
#include <sys/param.h>

#include "hfs.h"

/* #define TRACEER */

/* Forward routines */

typedef int extidx_t;		/* Extent index */

static ExtDescr_t *get_extp(ExtDescRec_t*, erec_t *, extidx_t extidx);
static int  add_extr(erec_t *ep, int i);
static int  delete_extr(erec_t *ep, int i);
static void er_cons_key(u_int fid, u_char fork, eofk_t *key, u_int alb);
static void er_new(hfs_t *hfs,erec_t **epp);
static extidx_t    last_ext(ExtDescRec_t*, erec_t *, u_int size);
static void set_alc(hfs_t *hfs, erec_t *ep, frec_t *fp, extidx_t eidx);
static int  update_extr(erec_t *ep, int i);


#define REMOVE_ACTV_ER(ep)  hl_remove((hlnode_t*)ep)
#define FREE_ER(hfs,ep)     hl_insert_free(&hfs->fs_erec_list,(hlnode_t*)ep)
#define GET_FREE_ER(hfs)    (erec_t*)hl_get_free(&hfs->fs_erec_list)
#define ACTV_ER(hfs,ep)     hl_insert(&hfs->fs_erec_list,(hlnode_t*)ep)
#define REMOVE_FREE_ER(ep)  hl_remove_free((hlnode_t*)ep)


/*****************************************************************************
 *                                                                           *
 *                            Global Routines                                *
 *                                                                           *
 *****************************************************************************/



/***********
 * erhashkey
 */
static int erhashkey(register erec_t *ep){
  return ERKEY(ep->e_fid,ep->e_fork);
}

/*********
 * er_init
 */
int er_init(hfs_t *hfs, int initial, int cachebins){
  int i;
  erec_t *ep;
  hlroot_t *hl = &hfs->fs_erec_list;
  
  /*
   * Init the hashlist.
   * Allocate all the hnodes.
   */
  hl_init(hl,cachebins,(int (*)(hlnode_t*))erhashkey);
  hfs->fs_erpool = ep = (erec_t*)safe_malloc(initial*sizeof(erec_t));
  bzero(hfs->fs_erpool, initial*sizeof(erec_t));
  
  /*
   * Place them all on the freelist.
   */
  hfs->fs_erec_count = initial;
  for (i=0;i<initial;ep++,i++){
    NULLHLNODE(ep->e_hl);
    hl_insert_free(hl,(hlnode_t*)ep);
  }
  return 0;
}


/************
 * er_destroy
 */
int er_destroy(hfs_t *hfs){
  
  /* Destroy the hashlist and free the erecs */

  hl_destroy(&hfs->fs_erec_list);
  if (hfs->fs_erpool)
    free(hfs->fs_erpool);
  return 0;
}


/*******
 * ercmp
 */
static int ercmp(erec_t *ep, u_int key){
  return ERKEY(ep->e_fid,ep->e_fork) == key;
}


/********
 * er_get
 */
int er_get(hfs_t *hfs, u_int fid, u_char fork, u_short albn, erec_t **erpp){

  int    error;
  erec_t *ep;


  /*
   * Try to fine the erec in the hashlist.
   */
  ep = (erec_t *) hl_find(&hfs->fs_erec_list,
			  ERKEY(fid,fork),
			  (void*)ERKEY(fid,fork),
			  (int (*)(hlnode_t*,void*))ercmp);

  if (ep){
    assert(ER_ISSET(ep,ACTIVE));
    REMOVE_FREE_ER(ep);
#ifdef TRACEER
    TRACE(("er_get.:     fid: %4d, fork:  %2x, refcnt: %d\n",
	  fid, fork, ep->e_refcnt));
#endif
    ep->e_refcnt++;
    *erpp = ep;
    return 0;
  }

  /*
   * If we didn't find the erec, get a free erec and then initialize
   * it from disk.
   */
  er_new(hfs,&ep);
  ep->e_fid = fid;
  ep->e_fork= fork;
  ep->e_hfs = hfs;
  if (error = er_read(ep,albn)){
    er_free(ep);
    return __chk(ep->e_hfs,error,er_get);
  }

  /*
   * Set the reference count.
   * Activate the erec and return.
   */
  ep->e_refcnt = 1;
  ACTV_ER(hfs,ep);
  ER_SET(ep,ACTIVE);
  *erpp = ep;
#ifdef TRACEER
    TRACE(("er_get:      fid: %4d, fork:  %2x, refcnt: %d\n",
	  fid, fork, ep->e_refcnt));
#endif
  return 0;
}


/********
 * er_put
 */
int er_put(erec_t *ep){
  assert(ep->e_refcnt!=0);
  if (--ep->e_refcnt == 0)
    FREE_ER(ep->e_hfs,ep);
#ifdef TRACEER
    TRACE(("er_put:      fid: %4d, fork:  %2x, refcnt: %d\n",
	  ep->e_fid, ep->e_fork, ep->e_refcnt));
#endif
  return 0;
}


/********
 * er_new	Get a free erec from the pool.
 */
static void er_new(hfs_t *hfs,erec_t **epp){
  *epp = GET_FREE_ER(hfs);
  if (*epp==NULL)
    panic("er_get: out of erecs");
  __chk(hfs,er_reclaim(*epp),er_get);
}


/*********
 * er_free	Free an erec.
 */
void er_free(erec_t *ep){
#ifdef TRACEER
    TRACE(("er_free:     fid: %4d, fork:  %2x, refcnt: %d\n",
	  ep->e_fid, ep->e_fork, ep->e_refcnt));
#endif
  ER_CLR(ep,ACTIVE);
  REMOVE_ACTV_ER(ep);
  FREE_ER(ep->e_hfs,ep);
}


/************
 * er_reclaim
 */
int er_reclaim(erec_t *ep){
  int error;
  REMOVE_ACTV_ER(ep);
  if (ER_ISCLR(ep,ACTIVE))
    return 0;
#ifdef TRACEER
  TRACE(("er_reclaim:  fid: %4d, fork:  %2x, refcnt: %d\n",
	ep->e_fid, ep->e_fork, ep->e_refcnt));
#endif
  error = er_update(ep,0);
  ER_CLR(ep,ACTIVE);
  return __chk(ep->e_hfs,error,er_reclaim);
}


#define UPDATE(ep,i) if (error=update_extr(ep,i)) break
#define DELETE(ep,i) if (error=delete_extr(ep,i)) break
#define ADD(ep,i) if (error=add_extr(ep,i)) break

/***********
 * er_update
 */
int er_update(erec_t *ep, u_int flags){
  
  int     error=0;
  int     i;
  
  /*
   * Scan all the extent descriptor records.
   */
  for(i=0;ep->e_mask && i<ERMAXEXTRECS;i++){
    albdata_t    *albd= &ep->e_albdat[i];
    
    /*
     * What we need to do depends on the relation between a_incore and
     * a_ondisk.
     */
    if (albd->a_ondisk)
      if (albd->a_incore)
	if (albd->a_ondisk == albd->a_incore){
	  UPDATE(ep,i);		/* Updates existing record */
	}
	else{
	  DELETE(ep,i);		/* Deletes old ondisk record */
	  ADD(ep,i);		/* Adds  incore record */
	}
      else{
	DELETE(ep,i);		/* Deletes old ondisk record */
      }
    else{
      if (albd->a_incore)
	ADD(ep,i);		/* Adds incore record */
    }
    ERM_CLR(ep,i);
    albd->a_ondisk = albd->a_incore;
  }
  ER_CLR(ep,DIRTY);
  FS_SET(ep->e_hfs,MDB_ATR);
  return __chk(ep->e_hfs,error,er_update);
}


/*********
 * er_read
 */
int er_read(erec_t *ep, u_short albidx){
  int     error;
  eofk_t  key;
  rdesc_t rd;
  bt_t    *bt = &ep->e_hfs->fs_ebt;
  int     i;

  /*
   * Zero out the albdata.
   */
  for (i=0;i<ERMAXEXTRECS;i++){
    ep->e_albdat[i].a_ondisk=0;
    ep->e_albdat[i].a_incore=0;
  }
  
  /*
   * Init the record to clean and empty.
   * Construct eofk key.
   */
  ER_CLR(ep,DIRTY);
  ep->e_mask = 0;
  ep->e_count= 0;
  er_cons_key(ep->e_fid, ep->e_fork, &key, albidx);

  /*
   * Read the record.
   * If ENOENT, we're done.
   */
  error = bt_rget(bt,(btk_t*)&key, &rd);
  if (error)
    if (error==ENOENT)
      goto done;
    else
      return __chk(bt->b_hfs,error,er_read);


  /*
   * Copy the extent record into the erec.
   * Update the albdata.
   * Advance to the next extent record.
   * Quit when we get more than ERMAXEXTRECS, run out of extents, 
   *   or we no longer match our file number.
   */
  while (ep->e_count<=ERMAXEXTRECS && EOFK_FNUM(&key)==ep->e_fid){
    albdata_t  *albd=&ep->e_albdat[ep->e_count];
    ExtDescRec_t *extr=&ep->e_erecs[ep->e_count];

    bcopy(rd.data,extr,sizeof(ExtDescRec_t));
    albd->a_incore=albidx;
    albd->a_ondisk=albidx;

    ep->e_count++;

    if (error=bt_rnext(bt, (btk_t*)&key, &rd)){
      if (error==ENOENT)
	goto done;
      return __chk(bt->b_hfs,error,er_read);
    }
    if (EOFK_FNUM(&key)!=ep->e_fid)
      goto done;

    albidx = (key.k_key.k_FABN[0]<<8) | key.k_key.k_FABN[1];
    assert(albidx == albd->a_ondisk + (extr->exts[0].ed_NumABlks +
				       extr->exts[1].ed_NumABlks +
				       extr->exts[2].ed_NumABlks));

  }
  return EFBIG;			/* Got too many extents */

 done:
  return 0;
}


/***********
 * er_toalbn	Map alb offset to abln.
 */
int er_toalbn(u_int n,
	      ExtDescRec_t *firstextr,
	      erec_t *ep,
	      u_int *albn,
	      u_int *albns){

  u_int i, t, residue=0;
  ExtDescr_t *exts = firstextr->exts;

  /*
   * Try the mapping in the first three extents first.
   */
  for (i=0;i<3;i++)
    if ((t=n-residue)<exts[i].ed_NumABlks)
      goto hit;
    else
      residue += exts[i].ed_NumABlks;

  /*
   * Try mapping in the erec extents next.
   */
  exts = ep->e_erecs[0].exts;
  for (i=0;i<ep->e_count*3;i++)
    if ((t=n-residue)<exts[i].ed_NumABlks)
      goto hit;
    else
      residue += exts[i].ed_NumABlks;

  return ENOSPC;

 hit:
  *albn = t + exts[i].ed_StABN;
  *albns= exts[i].ed_NumABlks - t;
  return 0;
}


/*********
 * er_sync	Synchronize all erecs with disk.
 *
 * NOTE:	As we attempt to write various components out, we
 *		don't quit on error.  We try to get as much done as
 * 		we can so a disk recovery program will have as much to
 *		work with as possible.
 */
int er_sync(hfs_t *hfs, u_int flags){
  int error=0,temperror;
  int i;
  erec_t *ep;


  /* If no flags are set, set them all */

  if (flags==0)
    flags = ALL;

  /* Note that the only kind of dirty an erec can be is allocation dirty */
    
  /*
   * Scan over all erecs looking for active ones.
   * If it's active and appropiately dirty, call er_update to write it out.
   */
  for (ep=hfs->fs_erpool,i=0;i<hfs->fs_erec_count;i++,ep++)
    if (ER_ISSET(ep,ACTIVE))
      if (( flags & ALC) && ER_ISSET(ep,ALC)){
	temperror = er_update(ep,flags);
	if (error==0)
	  error = temperror;
      }

  return __chk(hfs,error,er_sync);
}


/***********
 * er_expand	Expand a file.
 */
int er_expand(hfs_t        *hfs,
	      ExtDescRec_t *extr,
	      erec_t       *ep,
	      frec_t       *fp,
	      int          albs,
	      u_int	    clmpsz,
	      u_int 	    *size){
  int        exhausted=0, error=0;
  u_int      albstart,runsize;
  ExtDescr_t *ext;
  extidx_t   eidx;
  int        albidx;

  /*
   * Set albidx to the alb index of the "next" alb to be allocated.  When
   * we do allocate a group of alb's, this index will be the index of the
   * first alb in the group and is used as part of the extent overflow
   * record key for the group.  This index is also equal to the number
   * of alb's currently allocated to the file.
   */
  albidx=howmany(*size,hfs->fs_mdb.m_AlBlkSiz);


  /*
   * If the file is empty, eidx is -1 and we skip over the attempt to
   * extend current last extent.
   */
  eidx=last_ext(extr,ep,albidx);
  if (eidx>=0)
    ext = get_extp(extr,ep,eidx);
  else{
    eidx=0;
    goto rest;
  }


  /*
   * Check for a run of free blocks beginning immediately after
   *   the last extent.  We always grab them to squeeze out small allocation
   *   holes.
   */
  albstart = ext->ed_StABN + ext->ed_NumABlks;
  albs = roundup(albs,clmpsz);
  if (!bit_run(albstart, (char *)hfs->fs_vbm, hfs->fs_mdb.m_NmAlBlks, &runsize))
    if (runsize){
      runsize = MIN(runsize,albs);
      bit_run_set(albstart, (char *) hfs->fs_vbm,runsize);
      ext->ed_NumABlks += runsize;
      *size += runsize * hfs->fs_mdb.m_AlBlkSiz;
      albidx+= runsize;
      albs  -= runsize;
      hfs->fs_mdb.m_FreeBlks -= runsize;

      set_alc(hfs,ep,fp,eidx);
    }
  eidx++;

  
  /*
   * Fill the rest of the request.
   */
 rest:
  albstart = hfs->fs_mdb.m_AllocPtr;
  albs = roundup(albs,clmpsz);
  ext = get_extp(extr,ep,eidx);
  while(albs){
    if (ext==NULL)
      break;
    if (!bit_run(albstart, (char *) hfs->fs_vbm,
                 hfs->fs_mdb.m_NmAlBlks,&runsize)){
      if (runsize==0)
	if (exhausted++)
	  break;
	else
	  albstart=0;
      else
	if (runsize>=clmpsz){
	  runsize = MIN(runsize,albs);
	  bit_run_set(albstart,(char *) hfs->fs_vbm,runsize);
	  *size += runsize * hfs->fs_mdb.m_AlBlkSiz;
	  albs -= runsize;
	  ext->ed_NumABlks = runsize;
	  ext->ed_StABN = albstart;
	  if (eidx>=3){
	    if (eidx%3 == 0)
	      ep->e_albdat[(eidx-3)/3].a_incore = albidx;
	    ep->e_count++;
	  }
	  albidx+=runsize;
	  hfs->fs_mdb.m_FreeBlks -= runsize;
	  hfs->fs_mdb.m_AllocPtr = albstart+runsize;

	  set_alc(hfs,ep,fp,eidx++);

	  ext = get_extp(extr,ep,eidx);
	}
    }	
    albstart += runsize;
  }

  if (albs)
    error = ENOSPC;
  return error;
}


/***********
 * er_shrink	Shrink a file.
 */
int er_shrink(hfs_t        *hfs,
	      ExtDescRec_t *extr,
	      erec_t       *ep,
	      frec_t        *fp,
	      int          albs,
	      u_int        clmpsz,
	      u_int        *size){
  int        error;
  extidx_t   eidx;
  int        albidx;

  albidx = howmany(*size,hfs->fs_mdb.m_AlBlkSiz);

  eidx=last_ext(extr,ep,albidx);

  assert(*size);

  while (albs){
    ExtDescr_t *ext = get_extp(extr,ep,eidx);
    int delta = MIN(ext->ed_NumABlks,albs);

    /*
     * Clear the alb bits in the bitmap.
     * Mark the bitmap dirty.
     */
    bit_run_clr(ext->ed_StABN+ext->ed_NumABlks-delta ,
		(char *) hfs->fs_vbm,
		delta);

    FS_SET(hfs,VBM_ALC);

    /*
     * Update the block count and the allocation ptr in the MDB.
     * Update the extent itself.
     */
    hfs->fs_mdb.m_FreeBlks += delta;
    if (hfs->fs_mdb.m_AllocPtr == ext->ed_StABN + ext->ed_NumABlks)
	hfs->fs_mdb.m_AllocPtr -= delta;
    albs -= delta;
    ext->ed_NumABlks -= delta;
    if (ext->ed_NumABlks==0){
      ext->ed_StABN=0;
      if (eidx>=3 && eidx%3==0){
	ep->e_albdat[(eidx-3)/3].a_incore=0;
	ep->e_count--;
      }
      if (hfs->fs_mdb.m_AllocPtr == ext->ed_StABN)
	  hfs->fs_mdb.m_AllocPtr = 0;   /* Hack: next alloc at start of disk */
    }
    *size -= delta * hfs->fs_mdb.m_AlBlkSiz;
    set_alc(hfs,ep,fp,eidx);
    eidx--;
  }
  return 0;
}



/*****************************************************************************
 *                                                                           *
 *                             Local Routines                                *
 *                                                                           *
 *****************************************************************************/


/*************
 * er_cons_key	Construct an extent overflow file key.
 */
static void er_cons_key(u_int fid, u_char fork, eofk_t *k, u_int albidx){
  
  INVALIDATE_KHDR(k->k_hdr);
  
  k->k_key.k_KeyLen = 7;
  k->k_key.k_FABN[0] = albidx >>8;
  k->k_key.k_FABN[1] = albidx & 0xff;
  k->k_key.k_FkType = fork;
  bcopy((char*)&fid,k->k_key.k_FNum,4);
}


/*************
 * update_extr	Update an extent record in the extent overflow file.
 */
static int update_extr(erec_t *ep, int i){
  int error;
  eofk_t  key;
  rdesc_t rd;
  
  ExtDescRec_t *extr = &ep->e_erecs[i];
  albdata_t    *albd = &ep->e_albdat[i];

  er_cons_key(ep->e_fid,ep->e_fork, &key,albd->a_ondisk);
  rd.data = extr;
  rd.count= sizeof *extr;

  return __chk(ep->e_hfs,
	       bt_rupdate(&ep->e_hfs->fs_ebt,(btk_t*)&key,&rd),
	       update_extr);
}


/*************
 * delete_extr	Delete an extent record in the extent overflow file.
 */
static int delete_extr(erec_t *ep, int i){
  int error;
  eofk_t  key;
  rdesc_t rd;
  
  ExtDescRec_t *extr = &ep->e_erecs[i];
  albdata_t    *albd = &ep->e_albdat[i];

  er_cons_key(ep->e_fid,ep->e_fork, &key,albd->a_ondisk);

  return __chk(ep->e_hfs,
	       bt_rremove(&ep->e_hfs->fs_ebt,(btk_t*)&key),
	       delete_extr);
}


/**********
 * add_extr	Add an extent record to the extent overflow file.
 */
static int add_extr(erec_t *ep, int i){
  int error;
  eofk_t  key;
  rdesc_t rd;
  
  ExtDescRec_t *extr = &ep->e_erecs[i];
  albdata_t    *albd = &ep->e_albdat[i];

  er_cons_key(ep->e_fid,ep->e_fork, &key,albd->a_incore);
  rd.data = extr;
  rd.count= sizeof *extr;

  return __chk(ep->e_hfs,
	       bt_radd(&ep->e_hfs->fs_ebt,(btk_t*)&key,&rd),
	       add_extr);
}


/**********	
 * last_ext	Returns extent index to last extent.
 */
static extidx_t last_ext(ExtDescRec_t *extr, erec_t *ep, u_int sizea){
  extidx_t   idx=0;
  ExtDescr_t *ext;

  while (sizea){
    ext = get_extp(extr,ep,idx++);
    sizea-=ext->ed_NumABlks;
  }
  return idx-1;
}


/**********
 * get_extp	Returns ptr to ext.
 */
static ExtDescr_t *get_extp(ExtDescRec_t *extr,
			    erec_t       *ep,
			    extidx_t     extidx){
  if (extidx<3)
    return &extr->exts[extidx%3];
  return &ep->e_erecs[(extidx-3)/3].exts[extidx%3];
}



/*********
 * set_alc	Set ALC in appropriate location.
 */
static void set_alc(hfs_t *hfs, erec_t *ep, frec_t *fp, extidx_t eidx){
  
  /*
   * Mark the file system as VBM_ALC as the VBM changed
   *   and as MDB_ATR as the block counts in the mdb changed.
   * If fp is not null, we may mark the frec as dirty if eidx indicates
   *   that the extent descriptors in the frec were modified.  If fp is
   *   null then presumably this is a btree file change and the mdb is
   *   already marked dirty.
   * If eidx indicate the change is in the erec, we mark the erec dirty.
   */
  FS_SET(hfs,VBM_ALC);
  FS_SET(hfs,MDB_ATR);
  if (eidx<3){
    if (fp)
      FR_SET(fp,ALC);
  }
  else{
    ERM_SET(ep,(eidx-3)/3);
    ER_SET(ep,ALC);
  }
}

