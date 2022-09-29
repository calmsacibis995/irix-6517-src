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
#include "hfs.h"



/* #define TRACEFR */

#define min(a,b) (a>b ? b : a)

/* Forward routines */

static void cache_fid(frec_t *fp);
static int find_fid(frec_t *fp);


/*****************************************************************************
 *                                                                           *
 *                            Global Routines                                *
 *                                                                           *
 *****************************************************************************/


/***********
 * frhashkey	Returns hfs fid as hash key
 */
static int frhashkey(register frec_t *rp){
  return rp->f_fid;
}


/***********
 * fchashkey	Returns fid as hash key.
 */
static int fchashkey(register fidc_t *cp){
  return cp->fc_fid;
}


/*********
 * fr_init	Initialize and populate hashlist of frec's.
 */
int fr_init(hfs_t *hfs, u_int initial, u_int cachebins){
  int i;
  fidc_t *cp;
  frec_t *rp;
  hlroot_t *fl = &hfs->fs_frec_list;
  hlroot_t *cl = &hfs->fs_fidc_list;

  /*
   * Initialize the hashlist.
   * Allocate all the frec's.
   */
  hl_init(fl,cachebins,(int (*)(hlnode_t*))frhashkey);
  hfs->fs_frpool = rp = (frec_t*)safe_malloc(initial*sizeof(frec_t));
  bzero(rp,initial*sizeof(frec_t));

  /*
   * Place the frec's on the freelist.
   */
  hfs->fs_frec_count = initial;
  for (i=0;i<hfs->fs_frec_count;rp++,i++){
    NULLHLNODE(rp->f_hl);
    hl_insert_free(fl,(hlnode_t*)rp);
  }

  /*
   * Initialize the fidc cache.
   */
  hl_init(cl,cachebins,(int (*)(hlnode_t*))fchashkey);
  hfs->fs_fcpool = cp = (fidc_t*)safe_malloc(FID_CACHE_SIZE*sizeof(fidc_t));
  bzero(cp,FID_CACHE_SIZE*sizeof(fidc_t));
  
  /*
   * Place the fidc's on the freelist.
   */
  hfs->fs_fidc_count = FID_CACHE_SIZE;
  for (i=0;i<hfs->fs_fidc_count;cp++,i++){
    NULLHLNODE(cp->fc_hl);
    hl_insert_free(cl,(hlnode_t*)cp);
  }

  return 0;
}


/************
 * fr_destroy
 */
int fr_destroy(hfs_t *hfs){
  
  /* Destroy the frec hashlist and free the frecs */
  
  hl_destroy(&hfs->fs_frec_list);
  if (hfs->fs_frpool)
    free(hfs->fs_frpool);

  /* Destroy the fidc hashlist and free the fidcs */

  hl_destroy(&hfs->fs_fidc_list);
  if (hfs->fs_fcpool)
    free(hfs->fs_fcpool);

  return 0;
}


#define REMOVE_ACTV_FR(fp)  hl_remove((hlnode_t*)fp)
#define FREE_FR(hfs,fp)     hl_insert_free(&hfs->fs_frec_list,(hlnode_t*)fp)
#define GET_FREE_FR(hfs)    (frec_t*)hl_get_free(&hfs->fs_frec_list)
#define ACTV_FR(hfs,fp)     hl_insert(&hfs->fs_frec_list,(hlnode_t*)fp)
#define REMOVE_FREE_FR(fp)  hl_remove_free((hlnode_t*)fp)


/*******
 * frcmp	Returns true if frec.f_pid matches target.
 */
static int frcmp(register frec_t *fp, register u_int fid){
  return fp->f_fid == fid;
}


/********
 * fr_get	Fetch a frec by fid.
 */
int fr_get(hfs_t *hfs, fid_t fid, frec_t **frpp){
  
  int error;
  frec_t *fp;
  
  
  /*
   * Try to find the frec in the hashlist.
   */
  fp = fr_incore(hfs,fid);
  if (fp){
    assert(FR_ISSET(fp,ACTIVE));
#ifdef TRACEFR
    TRACE(("fr_get.:     fid: %4d, refcnt: %d\n", fid, fp->f_refcnt));
#endif
    *frpp = fp;
    return 0;
  }

  /*
   * If we didn't find the frec, get a free one and then initialize
   * it from disk.
   */
  fr_new(hfs,&fp);
  fp->f_fid = fid;
  fp->f_hfs = hfs;
  if (error=fr_read(fp)){
    fr_free(fp);
    /*
     * HACK: When we are mapping NFS filehandles to files, it is
     *       acceptable to not find a file associated with a FID.  This
     *       is because NFS file caching occasionally results in flushes
     *       to files that have already been deleted.  Under other
     *	     circumstances, such lookup failures are indications that the
     *       file system structures are corrupted.  So, when the NFS_MAP
     *       flag is set, not finding a file is a simple error, otherwise
     *       it's a FS_CORRUPTED condition as well.
     */
    if (FS_ISCLR(hfs,NFS_MAP))
      return __chk(hfs,error,fr_get);
    return error;
  }
  fp->f_refcnt = 1;
  ACTV_FR(hfs,fp);
  FR_SET(fp,ACTIVE);
  *frpp = fp;

#ifdef TRACEFR
  TRACE(("fr_get:      fid: %4d, refcnt: %d\n", fid, fp->f_refcnt));
#endif

  return 0;
}


/********
 * fr_put	Decrement the refcount and free the frec if the count is zero.
 */
int fr_put(frec_t *fp){
  assert(fp->f_refcnt!=0);
  if (--fp->f_refcnt == 0)
    FREE_FR(fp->f_hfs,fp);
#ifdef TRACEFR
  TRACE(("fr_put:      fid: %4d, refcnt: %d\n", fp->f_fid, fp->f_refcnt));
#endif
  return 0;
}


/*********
 * fr_free	Free an frec.
 */
void fr_free(frec_t *fp){
  FR_CLR(fp,ACTIVE);
  REMOVE_ACTV_FR(fp);
  FREE_FR(fp->f_hfs,fp);
}


/********
 * fr_new	Get a free frec structure.
 */
void fr_new(hfs_t *hfs, frec_t **frpp){
  *frpp = GET_FREE_FR(hfs);
  if (*frpp==NULL)
    panic("fr_get: out of frec");
  __chk(hfs,fr_reclaim(*frpp),fr_get);
  
  /* Make sure we don't wrongfully inherit a CREATE flags.  */
  FR_CLR((*frpp),CREATE);
}


/************
 * fr_reclaim	Reclaim a "pre-owned" frec.
 */
int fr_reclaim(frec_t *fp){
  int error;
  REMOVE_ACTV_FR(fp);
  if (FR_ISCLR(fp,ACTIVE))
    return 0;
#ifdef TRACEFR
  TRACE(("fr_reclaim:  fid: %4d\n", fp->f_fid));
#endif
  error = fr_update(fp, 0);

  /*
   * Enter (fid,pfid) in the fid cache.  That way, if we ever want to
   *   find the fid again, we know what the parent directory is and we
   *   will be able to accelerate the search for it.
   */
  cache_fid(fp);

  FR_CLR(fp,ACTIVE);
  return __chk(fp->f_hfs,error,fr_reclaim);
}


/*************
 * fr_activate	Activate an frec.
 *============
 *
 * NOTE: This is a hack.  At present, frec's are constructed by other
 *       modules and need a means of activating them.  Rather than
 *       exporting the fid cache code, this hack routine is used to
 *       activate an frec.
 */
void fr_activate(frec_t *fp){
  FR_SET(fp,ACTIVE);
  fp->f_refcnt=1;
  find_fid(fp);
  ACTV_FR(fp->f_hfs,fp);
}


/* Structure for unpacking/packing frec */

#define FRECOFF(m) sizeof(((cdr_t*)0)->m), offsetof(cdr_t,m)
#define FRECOFFn(m,n) n*sizeof(((cdr_t*)0)->m), offsetof(cdr_t,m)

u_short cdrdirpack[]={
  FRECOFF(c_Type),
  FRECOFF(c_Resrv2),
  FRECOFF(c.c_Dir.Flags),
  FRECOFF(c.c_Dir.Val),
  FRECOFF(c.c_Dir.DirID),
  FRECOFF(c.c_Dir.CrDat),
  FRECOFF(c.c_Dir.MdDat),
  FRECOFF(c.c_Dir.BkDat),
  FRECOFF(c.c_Dir.DInfo),
  FRECOFF(c.c_Dir.DXinfo),
  FRECOFFn(c.c_Dir.Resrv[0],4),
  0,0
    
};

u_short cdrfilepack[]={
  FRECOFF(c_Type),
  FRECOFF(c_Resrv2),
  FRECOFF(c.c_Fil.Flags),
  FRECOFF(c.c_Fil.Typ),
  FRECOFF(c.c_Fil.UsrWds),
  FRECOFF(c.c_Fil.FlNum),
  FRECOFF(c.c_Fil.StBlk),
  FRECOFF(c.c_Fil.LgLen),
  FRECOFF(c.c_Fil.PyLen),
  FRECOFF(c.c_Fil.RStBlk),
  FRECOFF(c.c_Fil.RLgLen),
  FRECOFF(c.c_Fil.RPyLen),
  FRECOFF(c.c_Fil.CrDat),
  FRECOFF(c.c_Fil.MdDat),
  FRECOFF(c.c_Fil.BkDat),
  FRECOFF(c.c_Fil.FndrInfo),
  FRECOFF(c.c_Fil.ClpSize),
  FRECOFF(c.c_Fil.ExtRec),
  FRECOFF(c.c_Fil.RExtRec),
  FRECOFF(c.c_Fil.Resrv),
  0,0
};

u_short cdrthdpack[]={
  FRECOFF(c_Type),
  FRECOFF(c_Resrv2),
  FRECOFFn(c.c_Thd.Resrv[0],2),
  FRECOFF(c.c_Thd.ParID),
  FRECOFFn(c.c_Thd.CName[0],32),
  0,0
};


/***********
 * fr_update	Update the on-disk copy of the frec.
 */
int fr_update(frec_t *fp, u_int flags){
  int error=0;
  char buf[sizeof(cdr_t)];
  rdesc_t rd;

  /* If not flag bits are set, set them all */

  if (flags==0)
    flags = ALL;


  /* Check for appropriate dirtyness */

  if ( ( (flags & ALC) && FR_ISSET(fp,ALC) ) ||
       ( (flags & ATR) && FR_ISSET(fp,ATR))){

    /*
     * Pack the data into a buffer and then write it to the
     * btree.
     */
    rd.data = buf;
    rd.count= (fp->f_cdr.c_Type == CDR_DIRREC)
      ? pack(&fp->f_cdr, buf, cdrdirpack)
	: pack(&fp->f_cdr, buf, cdrfilepack);

    error = bt_rupdate(&fp->f_hfs->fs_cbt, (btk_t*)&fp->f_key, &rd);
		
    FR_CLR(fp,DIRTY);
  }

  return __chk(fp->f_hfs,error,fr_update);
}



/*******
 * fr_read	Read in the frec from disk.
 */
int fr_read(frec_t *fp){
  int 		error;
  rdesc_t	rd;
  bt_t		*bt = &fp->f_hfs->fs_cbt;
  
  fp->f_key.k_hdr.kh_node=bt->b_hdr.bh_FNode;
  fp->f_key.k_hdr.kh_rec=-1;
  fp->f_key.k_hdr.kh_gen=bt->b_gen;

  if (find_fid(fp) && fp->f_fid!=ROOTFID){
    /*
     * Lookup the fid in the fid cache.  If the fid isn't the root fid,
     * set the frec key to that of the fid's parent thread record.  (The
     * fid cache contains the fid's parent's fid.)  If the fid is the root
     * fid, we fall through and wind up doing a sequential scan for the
     * entry from the first record because we set the key above to point
     * to the first record.  We also do a sequential scan if the fid
     * isn't in the cache.  (This should be quite rare.)
     *
     * NOTE:  The reason the root fid is handled differently is that the
     *        is no parent fid thread record preceeding it.
     * 
     * NOTE:  We lookup the fid before checking if the fid is the root fid.
     *        The reason is we need the side effect of purging the root
     *        fid from the cache.
     */
    fr_cons_key(fp->f_pfid,&fp->f_key,"");
    if (error=bt_rget(bt,(btk_t*)&fp->f_key,&rd))
      return error;
  }
  else
    TRACE(("fr_read - unoptimized (%d)\n", fp->f_fid));
  
  
  /*
   * Search sequentially for the matching file record.
   */
  while(1){
    if (error=bt_rnext(bt,(btk_t*)&fp->f_key,&rd))
      return error;
    if (!IS_FILE_OR_DIR(&rd))
      continue;
    unpack(rd.data,&fp->f_cdr,
	   IS_FILE(&rd) ? cdrfilepack : cdrdirpack);
    if (MATCHES(fp))
      break;
  }
  
  /* Set the parent fid from the key */

  fp->f_pfid =
    fp->f_key.k_key.k_ParID[0]<<24 |
      fp->f_key.k_key.k_ParID[1]<<16 |
	fp->f_key.k_key.k_ParID[2]<< 8 |
	  fp->f_key.k_key.k_ParID[3];

  FR_CLR(fp,DIRTY);

  return error;
}


/*************
 * fr_cons_key	Construct a catalog key
 */
void fr_cons_key(u_int fid, catk_t *k, char *name){
  int len = min(KEY_CNAME_LEN,strlen(name));

  INVALIDATE_KHDR(k->k_hdr);
  k->k_key.k_CName[0]=len;
  bcopy(name,&k->k_key.k_CName[1],len);
  bzero(&k->k_key.k_CName[len+1], KEY_CNAME_LEN-len);
  k->k_key.k_KeyLen = CAT_KEYLEN-1;
  bcopy(&fid,&k->k_key.k_ParID,4);
}



/*************
 * fr_cons_thd	Construct a thread record.
 */
void fr_cons_thd(frec_t *fp, char *thd, rdesc_t *rd){
  cdr_t cdr;

  bzero(&cdr,sizeof cdr);
  cdr.c_Type = CDR_THDREC;
  cdr.c.c_Thd.ParID = fp->f_pfid;
  bcopy(fp->f_key.k_key.k_CName,
	cdr.c.c_Thd.CName,
	sizeof fp->f_key.k_key.k_CName);
  rd->data = thd;
  rd->count= pack(&cdr,thd,cdrthdpack);
}


/**************
 * fr_construct	Construct an frec from a cdr record.
 */
int fr_construct(hfs_t *hfs, catk_t *key, rdesc_t *rd, frec_t **frpp){
  frec_t *fp;
  cdr_t  tcdr;
  fid_t  tfid;

  /*
   * Unpack the cdr from the record and see if we already have this
   * frec in the cache.  If so, return it.
   */
  unpack(rd->data,&tcdr,
	 IS_FILE(rd) ? cdrfilepack : cdrdirpack);
  if (tcdr.c_Type==CDR_DIRREC)
    tfid = tcdr.c.c_Dir.DirID;
  else
    tfid = tcdr.c.c_Fil.FlNum;
  fp = fr_incore(hfs,tfid);
  if (fp){
    assert(FR_ISSET(fp,ACTIVE));
#ifdef TRACEFR
    TRACE(("fr_cons.:    fid: %4d, refcnt: %d\n", tfid, fp->f_refcnt));
#endif
    REMOVE_FREE_FR(fp);
    *frpp = fp;
    return 0;
  }


  /*
   * We don't have it in the cache. We have to put it there.
   * We call find_fid() just for its side effect of removing any fid cache
   *   entry.  This is required to satisify the assertion
   *   that an fid is never in the fid cache if there is an incore frec
   *   for that fid.
   */
  fr_new(hfs,&fp);
  fp->f_hfs  = hfs;
  fp->f_fid  = tfid;
  find_fid(fp);
  fp->f_pfid = CAT_PARENT(key);
  FR_CLR(fp,DIRTY);
  FR_SET(fp,ACTIVE);

  bcopy(&tcdr,&fp->f_cdr,sizeof(cdr_t));
  bcopy(key,&fp->f_key, sizeof(catk_t));

  fp->f_refcnt = 1;

#ifdef TRACEFR
  TRACE(("fr_cons:     fid: %4d, refcnt: %d\n", tfid, fp->f_refcnt));
#endif

  ACTV_FR(hfs,fp);

  *frpp = fp;
  return 0;
}


/*********** 
 * fr_incore	Look for a cached frec by fid.
 */
frec_t *fr_incore(hfs_t *hfs, fid_t fid){
  frec_t *fp = (frec_t*)hl_find(&hfs->fs_frec_list,
				fid,
				(void*)fid,
				(int (*)(hlnode_t*,void*))frcmp);
  if (fp){
    REMOVE_FREE_FR(fp);
    fp->f_refcnt++;
  }
  return fp;
}


/*********
 * fr_sync	Synchronize with disk.
 *
 * NOTE:	As we attempt to write various components out, we
 *		don't quit on error.  We try to get as much done as
 * 		we can so a disk recovery program will have as much to
 *		work with as possible.
 */
int fr_sync(hfs_t *hfs, u_int flags){
  int error=0,temperror;
  int i;
  frec_t *fp;

  /* If no flags are set, set them all */

  if (flags==0)
    flags = ALL;

  /*
   * Scan over all frecs looking for active ones.
   * If it's active and appropriately dirty, call fr_update to write it out.
   */
  for (fp = hfs->fs_frpool,i=0; i<hfs->fs_frec_count;fp++,i++)
    if (FR_ISSET(fp,ACTIVE)){
      if ((flags & ALC) && FR_ISSET(fp,ALC)){
	temperror = fr_update(fp,flags);
	if (error==0)
	  error = temperror;
      }
      if ((flags & (ATR|SYNC)) && FR_ISSET(fp,ATR)){
	temperror = fr_update(fp,flags);
	if (error==0)
	  error = temperror;
      }
    }
  return __chk(hfs,error,fr_sync);
}

#define REMOVE_ACTV_FC(cp)  hl_remove((hlnode_t*)cp)
#define FREE_FC(hfs,cp)     hl_insert_free(&hfs->fs_fidc_list,(hlnode_t*)cp)
#define GET_FREE_FC(hfs)    (fidc_t*)hl_get_free(&hfs->fs_fidc_list)
#define ACTV_FC(hfs,cp)     hl_insert(&hfs->fs_fidc_list,(hlnode_t*)cp)
#define REMOVE_FREE_FC(cp)  hl_remove_free((hlnode_t*)cp)


/*******
 * fccmp	Returns true if fidc.f_fid matches target.
 */
static int fccmp(register fidc_t *cp, register u_int fid){
  return cp->fc_fid == fid;
}


/***********
 * cache_fid	Enter a fid and its parent fid in the fid cache.
 *==========
 *
 * Fid caching follows a non-standard protocol.  An fid is cached
 * only when a frec is reclaimed.  This allows us a compact memory of
 * who the parent is.  On the otherhand, when a fid is fetched from the
 * cache, the cache entry is always freed since the fid is now in the
 * frec.  
 */
static void cache_fid(frec_t *fp){
  hfs_t  *hfs = fp->f_hfs;
  fidc_t *cp;

  assert(FR_ISSET(fp,ACTIVE));

  /*
   * Check to see if it's in the cache already.  It shouldn't be.
   */
  assert(NULL==(fidc_t*)hl_find(&hfs->fs_fidc_list,
			fp->f_fid,
			(void*)fp->f_fid,
			(int (*)(hlnode_t*,void*))fccmp));

  /*
   * Get a free fidc block.
   * Remove it from the active list.
   * Fill in the fid and pfid fields.
   * Put it on the active list and the free list (i.e. it's a simple cache).
   */
  cp = GET_FREE_FC(hfs);
  if (cp==NULL)
    panic("cache_fid: out of fidc");
  if (cp->fc_fid)
    REMOVE_ACTV_FC(cp);

#ifdef TRACEFR
  TRACE(("cache_fid:   fid: %4d, pfid: %4d, ofid: %4d\n",
	fp->f_fid, fp->f_pfid, cp->fc_fid));
#endif

  cp->fc_fid = fp->f_fid;
  cp->fc_pfid= fp->f_pfid;
  ACTV_FC(hfs,cp);
  FREE_FC(hfs,cp);
}


/**********
 * find_fid	Lookup a fid in the fid cache.
 */
static int find_fid(frec_t *fp){
  hfs_t  *hfs = fp->f_hfs;
  fidc_t *cp;

  /*
   * Look it up in the cache.
   */
  cp = (fidc_t*)hl_find(&hfs->fs_fidc_list,
			fp->f_fid,
			(void*)fp->f_fid,
			(int (*)(hlnode_t*,void*))fccmp);

  /*
   * If it's there, copy the parent fid to the frec.
   */
  if (cp){
    fp->f_pfid = cp->fc_pfid;
#ifdef TRACEFR
    TRACE(("find_fid:    fid: %4d, pfid: %4d\n", fp->f_fid, fp->f_pfid));
#endif
    REMOVE_ACTV_FC(cp);
    return 1;
  }

#ifdef TRACEFR
  TRACE(("find_fid.:   fid: %4d\n", fp->f_fid));
#endif
  return 0;
}


