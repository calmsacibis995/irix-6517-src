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

#ident "$Revision: 1.9 $"

#include <sys/param.h>
#include <errno.h>
#include "hfs.h"



static is_valid_name(char *string);

/* Forward routines */

static int arhashkey(ar_t *ap);
static int arcmp(ar_t *ap, hno_t hno);
static int ar_populate(hnode_t *dhp, frec_t *fp, ar_t *ap);
static int ar_extend(hfs_t *, ar_t*);

static void as_new(hfs_t *, afs_t **);
static void as_free(afs_t *);
static int as_get(ar_t *, u_int slot, afs_t **);
static void as_put(afs_t *);
static int as_update(afs_t *);
static int as_read(ar_t *, fid_t, afs_t *);


static int mark_partial(afs_t *asp, u_int off, u_int cnt);

static asl_t *sp_init(sp_t *, ar_t *, int slot);
static asl_t *sp_find_free(sp_t *);
static asl_t *sp_find_fid(sp_t *, fid_t);

#define sp_eq(sp1,sp2) ((sp1)->ap==(sp2)->ap && \
			(sp1)->rap==(sp2)->rap && \
			(sp1)->idx==(sp2)->idx)
#define sp_ref(sp)    (&(sp)->ap->a_slots[(sp)->idx])
#define sp_rewind(sp) (sp)->ap=(sp)->rap;(sp)->idx=0;(sp)->aidx=0
#define SP_SLOT(sp)   (((sp)->aidx*AR_SLOTS)+(sp)->idx)

static void toshortname(char *shortname, char *hfsname);


#define REMOVE_ACTV_AR(ap)  hl_remove((hlnode_t*)ap)
#define FREE_AR(hfs,ap)     hl_insert_free(&hfs->fs_arec_list,(hlnode_t*)ap)
#define GET_FREE_AR(hfs)    (ar_t*)hl_get_free(&hfs->fs_arec_list)
#define ACTV_AR(hfs,ap)     hl_insert(&hfs->fs_arec_list,(hlnode_t*)ap)
#define REMOVE_FREE_AR(ap)  hl_remove_free((hlnode_t*)ap)


/*****************************************************************************
 *                                                                           *
 *                            Global Routines                                *
 *                                                                           *
 *****************************************************************************/


/***********
 * ar_incore	Try to find arec incore.
 */
ar_t *ar_incore(hfs_t *hfs, hno_t hno){
  ar_t *ap = (ar_t*)hl_find(&hfs->fs_arec_list,
			    hno,
			    (void*)hno,
			    (int (*)(hlnode_t*,void*))arcmp);
  if (ap)
    ap->a_refcnt++;
  return ap;
}


/********
 * ar_get	Get an arec.
 */
int  ar_get(hfs_t *hfs, hno_t hno, ar_t **app){
  int     error;
  hnode_t *dhp=NULL;
  ar_t    *ap;
  
  ap = ar_incore(hfs,hno);
  if (ap){
    *app = ap;
    return 0;
  }

  assert(TAG(hno)==0);

  if (error=hn_get(hfs,hno,&dhp))
    return error;

  ar_new(hfs,&ap);
  ap->a_hfs = hfs;
  ap->a_hno = hno;
  ap->a_next= NULL;

  ap->a_cdate = _h_dir(dhp).CrDat;
  ap->a_mdate = _h_dir(dhp).MdDat;
  hn_put(dhp);

  if (error=ar_read(ap)){
    ar_free(ap);
    return __chk(hfs,error,ar_get);
  }
  ap->a_refcnt=1;
  ACTV_AR(hfs,ap);
  *app = ap;
  return 0;
}


/********
 * ar_put	Release an arec.
 */
int ar_put(ar_t *ap){
  assert(ap->a_refcnt>0);
  ap->a_refcnt--;
  return (0);
}


/********
 * ar_new	Get a new arec structure from the pool.
 */
void ar_new(hfs_t *hfs, ar_t **app){
  ar_t *ap;
  int i;

 again:

  ap = GET_FREE_AR(hfs);
  if (ap==NULL){
    arp_t *arp = (arp_t*)safe_malloc(sizeof(arp_t));

    bzero(arp,sizeof(arp_t));
    arp->ap_next = hfs->fs_arppool;
    hfs->fs_arppool = arp;
    for (i=0;i<ARP_COUNT;i++){
      ap = &arp->ap_pool[i];
      NULLHLNODE(ap->a_hl);
      FREE_AR(hfs,ap);
      hfs->fs_arec_count++;
    }
    goto again;
  }
  for (i=0;i<AR_SLOTS;i++){
    ap->a_slots[i].a_fid=0;
    ap->a_slots[i].a_afs=NULL;
  }
  *app = ap;
}


/*********
 * ar_free	Return an arec structure to the pool.
 */
void ar_free(ar_t *ap){
  while (ap){
    REMOVE_ACTV_AR(ap);
    FREE_AR(ap->a_hfs,ap);
    ap = ap->a_next;
  }
}


/*********
 * ar_read	Fill in arec from disk.
 */
int ar_read(ar_t *ap){
  int     error;
  hnode_t *dhp;
  u_int   offset=0;

  if (error=hn_get(ap->a_hfs,ap->a_hno,&dhp))
    return error;
  error=dir_iterate(dhp, &offset, (dir_callback_t)ar_populate, ap);
  if (error==-1)
    error = 0;
  hn_put(dhp);
  return error;
}


/*********
 * ar_init	Initialize the arec hashlist and pool.
 */
int  ar_init(hfs_t *hfs,u_int cachebins){
  arp_t *arp;

  
  hl_init(&hfs->fs_arec_list, cachebins, (int (*)(hlnode_t*))arhashkey);
  hfs->fs_arpool = NULL;
  hfs->fs_arppool= NULL;
  hfs->fs_arec_count = 0;
  return 0;
}


/************
 * ar_destroy	Destroy an arec hashlist and pool.
 */
int ar_destroy(hfs_t *hfs){
  arp_t *arp, *tarp;
  hl_destroy(&hfs->fs_arec_list);

  arp = hfs->fs_arppool;
  while (arp){
    tarp = arp;
    arp = arp->ap_next;
    free(tarp);
  }
  return 0;
}


/***********
 * ar_remove	Remove an entry from an arec.
 */
int ar_remove(hfs_t *hfs, hno_t dhno, fid_t fid){
  int    error=0;
  sp_t   sp;
  asl_t  *aslp;
  ar_t   *ap = ar_incore(hfs,dhno);

  if (ap==NULL)
    return 0;

  sp_init(&sp,ap,0);

  if ((aslp=sp_find_fid(&sp,fid))==NULL)
    error = ENOENT;
  else{
    aslp->a_fid=0;
    as_free(aslp->a_afs);
  }

  ar_put(ap);
  return error;
}


/********
 * ar_add	Add an entry to an arec.
 */
int ar_add(hfs_t *hfs, hno_t dhno, fid_t fid){
  sp_t  sp;
  asl_t *aslp;
  ar_t  *ap = ar_incore(hfs,dhno);

  if (ap==NULL)
    return 0;

  sp_init(&sp,ap,0);
  while ((aslp=sp_find_free(&sp))==NULL){
    ar_extend(hfs,ap);
    sp_rewind(&sp);
  }
  aslp->a_afs=NULL;
  aslp->a_fid=fid;

  ar_put(ap);
  return 0;
}


/*********
 * ai_read	Read from ancillary file.
 */
int ai_read(hnode_t *hp, u_int offset, u_int count, char *data, u_int *nio){
  int   error;
  afs_t *asp;
  ar_t  *ap;

  if (error=ar_get(hp->h_hfs, HNO(FID(hp->h_hno),0), &ap))
    return error;

  while (count){
    u_int slot = offset/AI_SIZE;
    u_int bcount = MIN((slot+1)*AI_SIZE - offset, count);
    u_int aisoff = offset - (slot*AI_SIZE);

    if (error=as_get(ap,slot,&asp))
      break;
      
    bcopy( (char*)&asp->as_ai + aisoff, data, bcount);

    as_put(asp);
    offset += bcount;
    data   += bcount;
    *nio   += bcount;
    count  -= bcount;
  }
  if (error==ENOENT)
    error = 0;
  ar_put(ap);
  return error;
}

#define LNAME_LEN       32
#define LNAME_LOWOFF    254
#define LNAME_HIGHOFF   285

/*
 * is_valid_name()
 * This routine scans a 32 byte string. Returns:
 * (1) if it's a non-null string.
 * (0) if it's a null string.
 */
static int is_valid_name(char *string)
{
  if (string[0] == 0)
    return (0);
  return 1;
}

/**********
 * ai_write	Write to ancillary file.
 */
int ai_write(hnode_t *hp, u_int offset, u_int count, char *data){
  int   error;
  int   error_flag = 0;
  u_int lowoff, highoff;
  char  lname[LNAME_LEN];
  afs_t *asp;
  ar_t  *ap;

  if (error=ar_get(hp->h_hfs, HNO(FID(hp->h_hno),0), &ap))
    return error;

  ap->a_mdate=time(NULL);

  while(error==0 && count!=0){
    u_int slot = offset/AI_SIZE;
    u_int bcount = MIN((slot+1)*AI_SIZE - offset, count);
    u_int aisoff = offset - (slot*AI_SIZE);

    while((error=as_get(ap,slot,&asp))==ENOENT)
      error=ar_extend(hp->h_hfs,ap);
    if (error)
      break;

    lowoff  = aisoff;
    highoff = aisoff+bcount-1;
    /* Each ai_t has a lname field between LNAME_LOWOFF and LNAME_HIGHOFF */
    /* This corresponds to the long name of the file about which*/
    /* information is stored in the ancillary file. There are   */
    /* bcount bytes between lowoff and highoff being copied     */
    /* This may contain the lname string. We need to check this */
    /* If lname string is contained and if it is a null, we have */
    /* to disallow the copy. */
    if (lowoff <= LNAME_LOWOFF && highoff >= LNAME_HIGHOFF){
      /* This data segment contains the lname */
      /* Extract it and place it in the array */
      /* If it is not a null string, copy the data in */
      bcopy((void *) &data[LNAME_LOWOFF-aisoff], (void *) lname, LNAME_LEN);
      if (is_valid_name(lname)){
        /* This lname is not a null string */
        bcopy(data, (char *) &asp->as_ai+aisoff, bcount);
        mark_partial(asp, aisoff, bcount);
        asp->as_modified = 1;
        as_put(asp);
      }
    }
    else {
      /* This data segment does not contain lname */
      /* Blindly copy it from source to destination */
      bcopy(data, (char *) &asp->as_ai+aisoff, bcount);
      mark_partial(asp, aisoff, bcount);
      asp->as_modified = 1;
      as_put(asp);
    }
    offset += bcount;
    data   += bcount;
    count  -= bcount;
  }
  ar_put(ap);
  if (error_flag)
    error = 1;
  return error;
}


/*****************************************************************************
 *                                                                           *
 *                             Local Routines                                *
 *                                                                           *
 *****************************************************************************/


/***********
 * arhashkey
 */
static int arhashkey(ar_t *ap){
  return ap->a_hno;
}


/*******
 * arcmp
 */
static int arcmp(ar_t *ap, hno_t hno){
  return ap->a_hno == hno;
}


/*************
 * ar_populate
 */
static int ar_populate(hnode_t *dhp, frec_t *fp, ar_t *ap){
  sp_t  sp;
  asl_t *aslp;

  sp_init(&sp,ap,0);
  while((aslp=sp_find_free(&sp))==NULL){
    ar_extend(dhp->h_hfs,ap);
    sp_rewind(&sp);
  }
  aslp->a_fid = fp->f_fid;
  aslp->a_afs = NULL;
  return 0;
}


/***********
 * ar_extend	Add arec to end of arec chain.
 */
static int ar_extend(hfs_t *hfs, ar_t *ap){
  ar_t *nap;

  while (ap->a_next)
    ap = ap->a_next;
  ar_new(hfs,&nap);
  ap->a_next = nap;
  nap->a_hfs = ap->a_hfs;
  nap->a_hno = ap->a_hno;
  nap->a_next = NULL;
  return 0;
}


/********
 * as_new	Get a new ais record.
 *=======
 * :randyh	Allocating globally, probably should allocate against hfs.
 */
static void as_new(hfs_t *hfs, afs_t **aspp){
  *aspp = (afs_t*)safe_malloc(sizeof(afs_t));
}


/*********
 * as_free	Free an ais record.
 *========
 * :randyh	Freeing globally, probaby should free to hfs.
 */
static void as_free(afs_t *asp){
  if (asp)
    free(asp);
}


/********
 * as_get	Get an ais record.
 */
static int as_get(ar_t *ap, u_int slot, afs_t **aspp){
  sp_t   sp;
  int    error;
  afs_t  *asp;
  asl_t  *aslp = sp_init(&sp,ap,slot);
  
  if (aslp==NULL)
    return ENOENT;

  asp=aslp->a_afs;
  if (asp==NULL){
    as_new(ap->a_hfs,&asp);
    if (error=as_read(ap,aslp->a_fid,asp)){
      as_free(asp);
      return error;
    }
    asp->as_sp = sp;
  }
  *aspp = asp;
  return 0;
}


/********
 * as_put	Put an ais record.
 */
static void as_put(afs_t *asp){
  asl_t  *aslp = sp_ref(&asp->as_sp);

  assert(aslp);

  if (asp->as_modified)
    if (asp->as_partial==0)
      as_update(asp);
    else{
      aslp->a_afs = asp;
      return;
    }

  aslp->a_afs = NULL;
  as_free(asp);
}


/***********
 * as_update	Update an ais record.
 */
static int as_update(afs_t *asp){
  int     error;
  hnode_t *hp =NULL;
  frec_t  *fp =NULL;
  char    tname[CNNBUFLEN+2];

  if (asp->as_ai.ai_version!=AI_VERSION)
    return EIO;

  /*
   * Lookup the longname to correlate the entry with a fid.
   * Next check that the entry is in the right arec slot and adjust the
   *   slots if it isn't.
   */
  if (error=hn_get(asp->as_sp.ap->a_hfs,asp->as_sp.ap->a_hno,&hp))
    goto done;
  strcpy(tname+1,asp->as_ai.ai_lname); tname[0]=strlen(tname+1);
  if (error=dir_lookup_m(hp,tname,&fp))
    goto done;

  /*
   * Update the frec.
   */
  if (_f_cdr(fp).c_Type==CDR_DIRREC){
    bcopy(&asp->as_ai.ai_finfo,
	  &_f_dir(fp).DInfo,
	  sizeof asp->as_ai.ai_finfo);
    _f_dir(fp).MdDat = to_hfstime(time(NULL));
    _f_dir(fp).CrDat = asp->as_ai.ai_createDate;
    _f_dir(fp).BkDat = asp->as_ai.ai_backupDate;
  }
  else{
    bcopy(&asp->as_ai.ai_finfo,
	  &_f_file(fp).UsrWds,
	  sizeof(finfo_t));
    _f_file(fp).MdDat = to_hfstime(time(NULL));
    _f_file(fp).CrDat = asp->as_ai.ai_createDate;
    _f_file(fp).BkDat = asp->as_ai.ai_backupDate;
  }
  FR_SET(fp,ATR);

  asp->as_modified=0;

 done:
  if (hp)
    hn_put(hp);
  if (fp)
    fr_put(fp);
  return error;
}


/*********
 * as_read	Read in an ais record.
 */
static int as_read(ar_t *ap, fid_t fid, afs_t *asp){
  int error;
  u_int creation, backup;

  frec_t *fp;

  if (fid==0){
    bzero(asp,sizeof(afs_t));
    return 0;
  }

  if (error = fr_get(ap->a_hfs, fid, &fp))
    return error;

  bzero(&asp->as_ai, sizeof asp->as_ai);
  if (_f_cdr(fp).c_Type==CDR_DIRREC){
    bcopy(&_f_dir(fp).DInfo,
	  &asp->as_ai.ai_finfo,
	  sizeof asp->as_ai.ai_finfo);
    creation = _f_dir(fp).CrDat;
    backup   = _f_dir(fp).BkDat;
  }
  else{
    bcopy(&_f_file(fp).UsrWds,
	  &asp->as_ai.ai_finfo,
	  sizeof asp->as_ai.ai_finfo);
    creation = _f_file(fp).CrDat;
    backup   = _f_file(fp).BkDat;
  }
  asp->as_ai.ai_version = AI_VERSION;
  strncpy(asp->as_ai.ai_lname,
	  &fp->f_key.k_key.k_CName[1],
	  fp->f_key.k_key.k_CName[0]);
  
  asp->as_ai.ai_createDate = creation;
  asp->as_ai.ai_backupDate = backup;

  toshortname(asp->as_ai.ai_sname,fp->f_key.k_key.k_CName);

  fr_put(fp);

  asp->as_modified = 0;
  asp->as_partial = 0;
  return 0;
}





/**************
 * mark_partial
 *=============
 *
 * Having modified the ancillary record, we have to cope with the
 * possibility that we only partially modified it.  Partially modified
 * ancillary records are retained in memory and the underlying catalog
 * record is not updated.  When the entire record is modified the
 * underlying catalog record is updated (by as_put()).  In the figure
 * below, the "...." portion of the block is the modified portion.  We
 * store the boundry in as_partial.  Note that all modifications must
 * 1) modify the entire record; or 2) partially modify an unmodified
 * record; or 3) update in place a partially modified record;
 * or 4) complete a partially modified record.
 * 
 * 	+----+	-off==0	+----+   -off==0 +----+
 * 	|....|	|	|....|	 |	 |    |
 * 	|....|	|	|....|	 |cnt	 |    |
 * 	|....|	|cnt	|....|	 |	 |    |
 * 	|....|	|	|....|	 v	 |    |
 * 	|....|	|	|....|	 - :p	 |....|   -off!=0     :p
 * 	|....|	|	|    |		 |....|   |cnt
 * 	|....|	v	|    |		 |....|   v
 * 	+----+	-	+----+		 +----+   -
 *
 *    as_partial=0;   as_partial=-p;   as_partial=p;
 *
 *  I.E. if as_partial is non-zero, the block is partially modified.
 */
static int mark_partial(afs_t *asp, u_int off, u_int cnt){
  int   error=0;
  u_int p = asp->as_partial;


  assert((off+cnt)<=AI_SIZE);

  if (!asp->as_modified){
    if (off==0)
      if (cnt==AI_SIZE)
	p = 0;
      else
	p = -cnt;
    else
      p = off;
  }
  else{
    if (off==0)
      if (-p==cnt)		/* Rewriting a previous segment */
	;
      else
	if (p==cnt)		/* Completing a previous segment */
	  p=0;
	else
	  error = EIO;
    else
      if (p==off)		/* Rewriting a previous segment */
	;
      else
	if (-p==off)		/* Completing a previous segment */
	  p=0;
	else
	  error = EIO;
  }
#ifdef XXXX
  if (off==0)
    if (cnt==AI_SIZE)
      p = 0;
    else
      if (!asp->as_modified)
	p = -cnt;
      else
	if (p==cnt)
	  p=0;
	else
	  error = EIO;
  else
    if (!asp->as_modified)
      p = off;
    else
      if (-p==off)
	p = 0;
      else
	error = EIO;
#endif

  if (error==0)
    asp->as_partial=p;

  return error;
}



/*********
 * sp_init	Initialize a slot pointer.
 */
static asl_t *sp_init(sp_t *sp, ar_t *ap, int slot){
  sp->ap = sp->rap = ap;
  sp->idx= slot;
  sp->aidx=0;
  while (sp->ap && sp->idx>=AR_SLOTS){
    sp->ap = sp->ap->a_next;
    sp->idx -= AR_SLOTS;
    sp->aidx++;
  }
  if (sp->ap==NULL)
    return NULL;
  return &sp->ap->a_slots[sp->idx];
}


/**************
 * sp_find_free	Find a free slot in an arec.
 */
static asl_t *sp_find_free(sp_t *sp){
  asl_t *asp;

  while (sp->ap)
    if (sp->idx >= AR_SLOTS){
      sp->idx=0;
      sp->aidx++;
      sp->ap = sp->ap->a_next;
    }
    else{
      asp = sp_ref(sp);
      if (asp->a_fid==0)
	break;
      sp->idx++;
    }
  if (sp->ap)
    return asp;
  return NULL;
}


/*************
 * sp_find_fid	Find the slot for a particular fid in an arec.
 */
static asl_t *sp_find_fid(sp_t *sp, fid_t fid){
  asl_t *asp;

  while (sp->ap)
    if (sp->idx >= AR_SLOTS){
      sp->idx=0;
      sp->aidx++;
      sp->ap = sp->ap->a_next;
    }
    else{
      asp = sp_ref(sp);
      if (asp->a_fid==fid)
	break;
      sp->idx++;
    }
  if (sp->ap)
    return asp;
  return NULL;
}


/*************
 * toshortname	Convert a long hfs filename to a shortname.
 */
static void toshortname(char *shortname, char *hfsname){
  int i;
  char *s = shortname;
  char *l = hfsname;
  char *lm= l + *l++ +1;

  for (i=0;i<12;l++){
    if (l==lm)
      break;
    if (i==8){
      *s++ = '.';
      i++;
      l--;
    }
    if (*l=='.'){
      if (i<8){
	*s++ = '.';
	i=9;
      }
      continue;
    }
    if (*l==' '){
      *s++ = '_';
      i++;
      continue;
    }
    if (!isalnum(*l))
      continue;
    if (islower(*l))
      *s++ = toupper(*l);
    else
      *s++ = *l;
    i++;
  }
  *s = 0;
}

