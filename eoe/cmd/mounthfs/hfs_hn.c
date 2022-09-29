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


/* #define TRACEHN */

#include <errno.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "hfs.h"

#define REMOVE_ACTV_HN(hp) hl_remove((hlnode_t*)hp) 
#define FREE_HN(hfs,hp)    hl_insert_free(&hfs->fs_hnode_list,(hlnode_t*)hp)
#define GET_FREE_HN(hfs)   (hnode_t*)hl_get_free(&hfs->fs_hnode_list)
#define ACTV_HN(hfs,hp)    hl_insert(&hfs->fs_hnode_list,(hlnode_t*)hp)
#define REMOVE_FREE_HN(hp) hl_remove_free((hlnode_t*)hp)

#define MIN(a,b) (((a)<(b))?(a):(b))


extern uid_t uid;
extern gid_t gid;
extern mode_t mode;


/* Forward routines */

static int fetch_erec(hnode_t *hp);
static void hn_new(hfs_t *hfs, hnode_t **hnpp);

/*****************************************************************************
 *                                                                           *
 *                            Global Routines                                *
 *                                                                           *
 *****************************************************************************/


/***********
 * hnhashkey	Returns hno as hash key
 */
static int hnhashkey(hnode_t *hp){return hp->h_hno;}


/*********
 * hn_init	Initialize and populate hashlist of hnodes.
 */
int hn_init(hfs_t *hfs, u_int initial, u_int cachebins){
  int i;
  hnode_t *hp;
  hlroot_t *hl = &hfs->fs_hnode_list;

  /*
   * Init the hashlist.
   * Allocate all the hnodes.
   */
  hl_init(hl,cachebins,(int (*)(hlnode_t*))hnhashkey);
  hfs->fs_hnpool = hp = (hnode_t*)safe_malloc(initial*sizeof(hnode_t));
  bzero(hfs->fs_hnpool,initial*sizeof(hnode_t));

  /*
   * Place them all on the freelist.
   */
  hfs->fs_hnode_count = initial;
  for (i=0;i<hfs->fs_hnode_count;hp++,i++){
    NULLHLNODE(hp->h_hl);
    hl_insert_free(hl,(hlnode_t*)hp);
  }
  return 0;
}


/************
 * hn_destroy	Free all hnodes and destroy the hashlist.
 */
int hn_destroy(hfs_t *hfs){
  hl_destroy(&hfs->fs_hnode_list);
  if (hfs->fs_hnpool)
    free(hfs->fs_hnpool);
  return 0;
}


/*******
 * hncmp	Returns true if hnode matches target.
 */
static int hncmp(register hnode_t* hp, register hno_t hno){
  return hp->h_hno == hno;
}


/********
 * hn_get      Get an hnode by hno.
 */
int hn_get(hfs_t *hfs, hno_t hno, hnode_t **hpp){
  int error;
  hnode_t *hp;

  /*
   * Try to find the hnode in the hashlist.
   */
  hp = (hnode_t*) hl_find(&hfs->fs_hnode_list,
			  hno,
			  (void*)hno,
			  (int (*)(hlnode_t*,void*))hncmp);
  if (hp){
    assert(HN_ISSET(hp,ACTIVE));
    REMOVE_FREE_HN(hp);
    hp->h_refcnt++;
    *hpp = hp;
#ifdef TRACEHN
    TRACE(("hn_get.:     hno: %4d, refcnt: %d\n", hno, hp->h_refcnt));
#endif
    return 0;
  }

  /*
   * If we didn't find the hnode, get a free hnode and then
   * initialize it from disk.
   */
  hn_new(hfs,&hp);
  hp->h_frec = NULL;
  hp->h_erec = NULL;
  hp->h_hfs = hfs;
  hp->h_hno = hno;
  hp->h_uid = uid;
  hp->h_gid = gid;
  if (error = hn_read(hp)){
    hn_free(hp);
    return error;
  }

  /*
   * Set reference count.
   * Activate the node and return.
   */
  hp->h_refcnt = 1;
  ACTV_HN(hfs,hp);
  HN_SET(hp,ACTIVE);
  *hpp =hp;
#ifdef TRACEHN
    TRACE(("hn_get:      hno: %4d, refcnt: %d\n", hno, hp->h_refcnt));
#endif
  return 0;
}


/********
 * hn_put	Decrement reference count and free if zero.
 */
int hn_put(hnode_t *hp){
  assert(hp->h_refcnt!=0);
  if (--hp->h_refcnt == 0)
    FREE_HN(hp->h_hfs,hp);
#ifdef TRACEHN
    TRACE(("hn_put:      hno: %4d, refcnt: %d\n", hp->h_hno, hp->h_refcnt));
#endif
  return 0;
}


/********
 * hn_new	Get a new hn structure.
 */
static void hn_new(hfs_t *hfs, hnode_t **hnpp){
  *hnpp = GET_FREE_HN(hfs);
  if (*hnpp==NULL)
    panic("hn_get: out of hnodes");
  __chk(hfs,hn_reclaim(*hnpp),hn_get);
}

/*********
 * hn_free	Free an hnode.
 */
void hn_free(hnode_t *hp){
  HN_CLR(hp,ACTIVE);
  REMOVE_ACTV_HN(hp);
  FREE_HN(hp->h_hfs,hp);
}


/************
 * hn_reclaim	Reclaim a "pre-owned" hnode.
 */
int hn_reclaim(hnode_t *hp){
  int error,temperror;
  hfs_t *hfs = hp->h_hfs;

  if (HN_ISCLR(hp,ACTIVE))
    return 0;

#ifdef TRACEHN
    TRACE(("hn_reclaim:  hno: %4d\n", hp->h_hno));
#endif
  
  /*
   * Remove from the active list.
   */
  REMOVE_ACTV_HN(hp);
  
  /*
   * Cancel any preallocation on this node.
   */

  if (hp == hfs->fs_prealloced)
      fs_cancel_prealloc(hfs);

  /* Update before we reclaim it */
  
  error=hn_update(hp,0);
  
  /* Let go of any associated frec and erec */
  
  if (hp->h_frec){
    temperror =fr_put(hp->h_frec);
    if (!error)
      error=temperror;
  }
  hp->h_frec = NULL;
  if (hp->h_erec){
    temperror=er_put(hp->h_erec);
    if (!error)
      error=temperror;
  }
  hp->h_erec = NULL;
  
  HN_CLR(hp,ACTIVE);

 done:
  return __chk(hp->h_hfs,error,hn_reclaim);
}


/***********
 * hn_update	
 */
int hn_update(hnode_t *hp, u_int flags){
  int error=0, temperror=0;

  /*
   * The hnode doesn't have anything that ever gets written out
   * but the frec and erec do.
   */
  if (hp->h_frec)
    temperror=fr_update(hp->h_frec, flags);
  if (error==0)
    error = temperror;
  if (hp->h_erec)
    temperror=er_update(hp->h_erec, flags);
  if (error==0)
    error = temperror;

  HN_CLR(hp,DIRTY);
  return __chk(hp->h_hfs,error,hn_update);
}


/*********
 * hn_read	Read in the frec.
 *
 * We attempt to get the erec only when we need it.
 */
int hn_read(hnode_t *hp){
  int error;
  hnode_t *php;

  if (error = fr_get(hp->h_hfs, FID(hp->h_hno), &hp->h_frec))
    return error;

  /*
   * Set the type and mode of the hnode from the catalog data in the frec and
   * the mode of the mount point.  An exception is for .HSResource and
   * .HSancillary.  Take their modes from their parent directory.  We
   * fix that later, after we've figured out our parent id.
   */
  if (_h_cdr(hp).c_Type==CDR_DIRREC){
    hp->h_ftype = NFDIR;
    hp->h_fmode = (mode & S_IWRITE) ? HN_DIR_RW_ACCESS : HN_DIR_RO_ACCESS;
  }
  else{
    hp->h_ftype = NFREG;
    if (mode & S_IWRITE)
      hp->h_fmode = (_h_file(hp).Flags & FR_FILE_LOCK)
	? HN_FILE_RO_ACCESS
	: HN_FILE_RW_ACCESS;
    else
      hp->h_fmode = HN_FILE_RO_ACCESS;
  }


  /*
   * If we are an .HSancillary file, frec points to the associated directory
   * record *but* we are a plain file.
   */
  if (TAG(hp->h_hno)==nt_dotancillary)
    hp->h_ftype = NFREG;

  /*
   * Set our parent id.
   *
   * Note that we mark the parent of the root node to be itself.  Otherwise
   * the parent is a function of what type of node it is.
   */
  if (hp->h_hno == ROOTHNO)
    hp->h_phno = hp->h_hno;
  else
    switch(TAG(hp->h_hno)){
    case nt_dir: 
      hp->h_phno = HNO(hp->h_frec->f_pfid,0);
      break;
    case nt_resource:
      hp->h_phno = HNO(hp->h_frec->f_pfid,nt_dotresource);
      break;
    case nt_dotancillary:
    case nt_dotresource:
      hp->h_phno = HNO(FID(hp->h_hno),0);
      if (error=hn_get(hp->h_hfs,hp->h_phno,&php))
	return error;
      hp->h_fmode = php->h_fmode;
      hn_put(php);
      break;
    }

  HN_CLR(hp,DIRTY);
  return 0;
}


/*********
 * hn_sync	Synchronize hnodes with disk.
 *
 * NOTE:	At the present time this is a nop.  Hnodes contain
 *		no state, only pointers to frecs and hrecs.  They are
 *		sync'ed separately.
 *		
 */
int hn_sync(hfs_t *hfs, u_int flags){
  return 0;
}


/*************
 * hn_adj_size	Adjust file size.
 */
int hn_adj_size(hnode_t* hp, u_int newsize, u_int *oldsize){
  int          error;
  u_int        albsz;		/* Alb size in bytes */
  int          albs;		/* Size adjustment in albs */
  ExtDescRec_t *extr;		/* Ptr to first extent record */
  u_int        *sizep;		/* Address of file physical size */
  u_int	       *lsizep;		/* Address of file logical size */
  u_int	       clmpsz;		/* Clump size of file */

  assert(TAG(hp->h_hno)<2);
  assert(hp->h_frec->f_cdr.c_Type==CDR_FILREC);

  /*
   * Fetch this file's erec if it hasn't been done already.
   */
  if (hp->h_erec==NULL)
    __chkr(hp->h_hfs,
	   fetch_erec(hp),
	   hn_adj_size);
  
  /*
   * Gather basic size and extent parameters
   */
  albsz = _h_mdb(hp).m_AlBlkSiz;
  if (TAG(hp->h_hno)){
    sizep= &_h_file(hp).RPyLen;
    lsizep= &_h_file(hp).RLgLen;
    extr = &_h_file(hp).RExtRec;
  }else{
    sizep= &_h_file(hp).PyLen;
    lsizep= &_h_file(hp).LgLen;
    extr = &_h_file(hp).ExtRec;
  }
  albs  = howmany(newsize,albsz) - howmany(*sizep,albsz);

  clmpsz= _h_file(hp).ClpSize;
  if (clmpsz==0)
    clmpsz = _h_mdb(hp).m_ClpSiz;
  clmpsz /= _h_mdb(hp).m_AlBlkSiz;

  assert(clmpsz);


  if (oldsize)
    *oldsize = *sizep;

  /*
   * Dispatch according to the change in albs.
   */
  if (albs==0)
    return 0;

  if (albs>0)
    error = er_expand(hp->h_hfs, extr, hp->h_erec,
		      hp->h_frec, albs, clmpsz, sizep );
  else 
    error = er_shrink(hp->h_hfs, extr, hp->h_erec,
		      hp->h_frec, -albs, clmpsz, sizep );
  
  *lsizep = MIN(*lsizep,*sizep);

  if (!error)
    fs_sync(hp->h_hfs, ALL & ~MDB);

  return error;
}


/***********
 * hn_toalbn	Map alb index to alb.
 */
int hn_toalbn(hnode_t *hp, u_int albidx, u_int *albn, u_int *albcnt){
  int error;

  /*
   * Fetch this files erec if it hasn't been done already.
   */
  if (hp->h_erec==NULL)
    __chkr(hp->h_hfs,
	   fetch_erec(hp),
	   hn_toalbn);
  
  return er_toalbn(albidx,
		   TAG(hp->h_hno) ? &_h_file(hp).RExtRec
		                  : &_h_file(hp).ExtRec,
		   hp->h_erec,
		   albn,
		   albcnt);
}


/*****************************************************************************
 *                                                                           *
 *                            Local Routines                                 *
 *                                                                           *
 *****************************************************************************/


/************
 * fetch_erec	Fetch the erec for this hnode.
 */
static int fetch_erec(hnode_t *hp){
  ExtDescr_t *exts;
  int error;

  assert(TAG(hp->h_hno)<=nt_resource);

  exts = TAG(hp->h_hno) ?  (ExtDescr_t*)&_h_file(hp).RExtRec
                        :  (ExtDescr_t*)&_h_file(hp).ExtRec;
  /*
   * Fetch the erec.
   * Record its address.
   */
  __chkr(hp->h_hfs,
	 er_get(hp->h_hfs,
		FID(hp->h_hno),
		TAG(hp->h_hno) ? EOFK_RFORK : EOFK_DFORK,
		exts[0].ed_NumABlks
		+ exts[1].ed_NumABlks
		+ exts[2].ed_NumABlks,
		&hp->h_erec),
	 fetch_erec);

  return 0;
}

