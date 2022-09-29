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

#ident "$Revision: 1.15 $"

#include <sys/errno.h>
#include <rpc/rpc.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "hfs.h"

extern u_short cdrfilepack;
extern u_short cdrdirpack;

/* Directory code - used by readdir. */

typedef enum{dc_dot=0,
	       dc_dotdot=1,
	       dc_entries=2,
	       dc_resource=3,
	       dc_ancillary=4} dirc_t;



/* Forward routines */

static int remove_file(hnode_t *hp);
static int protected(hnode_t *hp, char *name);
static int create(hnode_t *hp, char *name, u_char type, frec_t **frp);
static int rename_file(hnode_t *hp, hnode_t *xp, hnode_t *thp, char *name);
static int rename_dir(hnode_t *hp, hnode_t *xp, hnode_t *thp, char *name);
static int getattr_ai(hnode_t *hp, fattr *fa);
static int readdir_cback(hnode_t *dhp, frec_t *fp, dirs_t *ds);

static dirc_t dcode(dirs_t *dsp);

#define NOTWRITEABLE(hp) ((hp->h_fmode & S_IWRITE)==0)

#define ROOTDIR(hp) (FID(hp->h_hno)==FID(ROOTHNO))

/* amount to preallocate when growing a file */
static const int PREALLOCATION_SIZE = 1 << 18; /* 256 Kb */

/****************************************************************************
 *									    *
 *				Global Routines				    *
 *									    *
 ****************************************************************************/

/***********
 *hfs_statfs	Return gross file system statistics
 */
int hfs_statfs(hfs_t *hfs, statfsokres *sfr){

  /*
   * cancel preallocation so blocks are accurate.
   */
  (void)fs_cancel_prealloc(hfs);

  sfr->tsize = NFS_MAXDATA;		/* Maximum transfer size */
  sfr->bsize = hfs->fs_mdb.m_AlBlkSiz;	/* "Block size" */
  sfr->blocks= hfs->fs_mdb.m_NmAlBlks;	/* Total "blocks" */
  sfr->bfree = hfs->fs_mdb.m_FreeBlks;	/* Total free blocks */
  sfr->bavail= sfr->bfree;		/* Available = free */
  return 0;
}


/************
 * hfs_access	Test access to a file or directory.
 */
int hfs_access(hnode_t *hp, struct authunix_parms *aup, int amode){

  if ((aup->aup_uid == 0) ||
      (hp->h_uid == aup->aup_uid && ((hp->h_fmode >> 6) & amode) == amode) ||
      (hp->h_gid == aup->aup_gid && ((hp->h_fmode >> 3) & amode) == amode) ||
      ((hp->h_fmode & amode) == amode))
    return 0;

  return EACCES;
}


/*************
 * hfs_getattr	Get file or directory attributes.
 */
int hfs_getattr(hnode_t *hp, fattr *fa){

  TRACE(("hfs_getattr: hno: %4d, name: \"%-16.16s\"\n",
	 hp->h_hno,
	 &hp->h_frec->f_key.k_key.k_CName[1]));
  
  fa->type = hp->h_ftype;
  fa->mode = hp->h_fmode;


  /*
   * HACK:  If the file was just created we return write access so that
   *        a readonly file can be initialized.  It would be nice to figure
   *        a way around this hack.  Note that the CREATE flag lasts only
   *        until the frec is flushed from the cache.  If/When it is
   *        reloaded the flag will be cleared since only hfs_create sets it.
   */
  if (FR_ISSET(hp->h_frec,CREATE))
    fa->mode = HN_FILE_RW_ACCESS;

  
  switch(fa->type){

  case NFREG:

    /* Regular file */

    if (TAG(hp->h_hno)==nt_dotancillary)
      getattr_ai(hp,fa);
    else{
      /* Plain file */
      fa->mode |= NFSMODE_REG;
      if (TAG(hp->h_hno) == nt_resource){
	fa->size = _h_file(hp).RLgLen;
	fa->blocks = BTOBB(_h_file(hp).RPyLen);
      }
      else{
	fa->size = _h_file(hp).LgLen;
	fa->blocks = BTOBB(_h_file(hp).PyLen);
      }
      fa->ctime.seconds = to_unixtime(_h_file(hp).CrDat);
      fa->ctime.useconds= 0;
      fa->mtime.seconds = to_unixtime(_h_file(hp).MdDat);
      fa->mtime.useconds= 0;
    }
    break;

  case NFDIR:

    /* Directory file */

    /*
     * We use the directory valence + 2 as an approximation of the directory
     * size where n is 2 accounts for "." and "..".  We don't account
     * for .HSResource and .HSAncillary.  We don't want to return zero as
     * some programs will then not look inside to discover the .Resource
     * and .HSAncillary files.
     */
    fa->size = hp->h_frec->f_cdr.c.c_Dir.Val + 2;
    fa->mode |= NFSMODE_DIR;
    fa->blocks = BTOBB(fa->size);
    fa->ctime.seconds = to_unixtime(_h_dir(hp).CrDat);
    fa->ctime.useconds= 0;
    fa->mtime.seconds = to_unixtime(_h_dir(hp).MdDat);
    fa->mtime.useconds= 0;
    break;
    

  default:
    return EIO;
  }

  fa->nlink = 1;
  fa->uid = hp->h_uid;
  fa->gid = hp->h_gid;
  fa->blocksize = BBSIZE;
  fa->rdev = 0;
  fa->fsid = hp->h_hfs->fs_fsid;
  fa->fileid = hp->h_hno;

  /* Finish the time settings */

  fa->atime.seconds = time(NULL);
  fa->atime.useconds= 0;
  fa->ctime.useconds= 0;
  fa->mtime.useconds= 0;


  return 0;
}


/*************
 * hfs_setattr	Set file or directory attributes.
 *============
 * NOTE: We ignore uid and gid attributes as they are not stored in the
 *       file system.  We will always see the gid and uid of the mount
 *       point.
 */
int hfs_setattr(hnode_t *hp, sattr *sa){
  int error=0;

  TRACE(("hfs_setattr: hno: %4d, name: \"%-16.16s\"\n",
	 hp->h_hno,
	 &hp->h_frec->f_key.k_key.k_CName[1]));

  /*
   * :randyh  ?We don't set any attributes of the ancillary file?
   */
  if (TAG(hp->h_hno)==nt_dotancillary)
    return 0;

  if (sa->mode!= -1){
    extern mode_t mode;
    if (mode & S_IWRITE){
      if (sa->mode & S_IWRITE)
	hp->h_fmode = HN_FILE_RW_ACCESS;
      else
	hp->h_fmode = HN_FILE_RO_ACCESS;
    }
    else{
      if (sa->mode & S_IWRITE)
	return EROFS;
      hp->h_fmode = HN_FILE_RO_ACCESS;
    }
    /* We always grant execute (traverse) access to directories. */
    if (_h_cdr(hp).c_Type==CDR_DIRREC)
      hp->h_fmode |= HN_DIR_EX_ACCESS;
    FR_SET(hp->h_frec,ATR);
  }

  if (sa->mtime.seconds != -1){
    if (_h_cdr(hp).c_Type==CDR_DIRREC)
      _h_dir(hp).MdDat = to_hfstime(sa->mtime.seconds);
    else
      _h_file(hp).MdDat = to_hfstime(sa->mtime.seconds);
    FR_SET(hp->h_frec,ATR);
  }

  if (sa->size != -1 && _h_cdr(hp).c_Type!=CDR_DIRREC){
    error = hn_adj_size(hp,sa->size,NULL);
  }

  /*
   * If the node is a normal directory, we have to propogate the fmode to any
   * associated .HSResource and .HSAncillary nodes that are resident.
   */
  if (TAG(hp->h_hno)==0 && sa->mode!= -1 && _h_cdr(hp).c_Type==CDR_DIRREC){
    hnode_t *chp;
    if (error=hn_get(hp->h_hfs,HNO(FID(hp->h_hno),nt_dotresource),&chp))
      return error;
    chp->h_fmode = hp->h_fmode;
    hn_put(chp);
    if (error=hn_get(hp->h_hfs,HNO(FID(hp->h_hno),nt_dotancillary),&chp))
      return error;
    chp->h_fmode = hp->h_fmode;
    hn_put(chp);
  }

  return error;
}


/************
 * hfs_lookup	Lookup a file by name in a directory node
 */
int hfs_lookup(hnode_t *hp, char *name, hnode_t **chp){
  int error;
  hno_t  hno;
  frec_t *fp=NULL;
  hfs_t  *hfs = hp->h_hfs;
  
  TRACE(("hfs_lookup:  hno: %4d, name: \"%-16.16s\"\n", hp->h_hno, name));

  /* Check for ".", ".." */

  if (strcmp(name,dot)==0)
    return hn_get(hfs, hp->h_hno, chp);
  if (strcmp(name,dotdot)==0)
    return hn_get(hfs, hp->h_phno, chp);

  /*
   * If this is a plain directory node, check for ".HSResource" and
   * ".HSAncillary" nodes.
   */
  if (TAG(hp->h_hno)==0){
    if (strcmp(name,dotresource)==0)
      return hn_get(hfs,HNO(FID(hp->h_hno),nt_dotresource),chp);
    if (strcmp(name,dotancillary)==0)
      return hn_get(hfs,HNO(FID(hp->h_hno),nt_dotancillary),chp);
  }
  else 
    /*
     * If it isn't a plain directory, i.e. it is a .HSResource directory,
     * we filter out the resource and ancillary files as they don't appear
     * in a .HSResource directory. 
     */
    if (strcmp(name,dotresource)==0 || strcmp(name,dotancillary)==0)
      return ENOENT;
  
  /*
   * Lookup the name in the directory.
   */
  if (error=dir_lookup(hp,name,&fp))
    return error;

  /*
   * If it's  a directory but our hnode is a .HSResource directory,
   * suppress the result.  We don't show directories in .HSResource.
   */
  if ((TAG(hp->h_hno)==nt_dotresource) && (fp->f_cdr.c_Type==CDR_DIRREC)){
    fr_put(fp);
    return ENOENT;
  }

  /*
   * Figure out what our hnode number is.
   * Release the frec.
   * Get the hnode.
   */
  hno = HNO(fp->f_fid,0);
  if (TAG(hp->h_hno)==nt_dotresource)
    hno = HNO(fp->f_fid,nt_resource);

  fr_put(fp);
  return hn_get(hfs,hno,chp);
}


/**********
 * hfs_link	Create a link.
 */
int hfs_link(){ return ENXIO;} 


/**************
 * hfs_readlink
 */
int hfs_readlink(){ return ENXIO; }



/*
 * Macros to "tidy" up the read/write code.
 *
 * NOTE: These macros presume various variable definitions.
 */
#define ALBMAP\
  if (error = hn_toalbn(hp, offset/albsize,&alb,&albcnt))\
    return error;

#define ADVANCE\
  offset += bcount; *nio += bcount; if (count -= bcount) ALBMAP;

#define READALB\
  if (hfs->fs_alb != alb){\
    if (error = io_aread(hfs,alb,albsize,hfs->fs_buf))\
      return error;\
    hfs->fs_alb = alb;\
    }
#define FLUSHALB\
    if (error = io_awrite(hfs,alb,albsize,hfs->fs_buf))\
      return error;

/**********
 * hfs_read
 */
int hfs_read(hnode_t *hp, u_int offset, u_int count, char *data, u_int *nio){
  int          error;
  u_int        sz, bcount, alb, albcnt, albsize, head;
  hfs_t	       *hfs=hp->h_hfs;

  TRACE(("hfs_read:    hno: %4d, offset: %4d, count: %4d\n",
	hp->h_hno,
	offset,
	count));

  /*
   * Initialize.
   */
  *nio = 0;
  albsize = hfs->fs_mdb.m_AlBlkSiz;
  switch(TAG(hp->h_hno)){
  case nt_data:
    if (_h_cdr(hp).c_Type != CDR_FILREC)
      return ENXIO;
    sz = _h_file(hp).LgLen;
    break;
  case nt_resource:
    if (_h_cdr(hp).c_Type != CDR_FILREC)
      return ENXIO;
    sz = _h_file(hp).RLgLen;
    break;
  case nt_dotancillary:
    return ai_read(hp, offset, count, data, nio);
  default:
    return ENXIO;
  }

  /*
   * Preliminary size checks.
   * Perform initial block mapping.
   */
  if (offset > sz)
    return 0;
  if (offset + count > sz)
    count = sz - offset;
  if (count==0)
    return 0;
  ALBMAP;


  /*
   * If offset isn't ALB aligned, we have to process the initial unaligned
   * fragment.
   */
  if ((head = offset % albsize)!=0){
    READALB;
    bcount = MIN(albsize - head, count);
    bcopy(&hfs->fs_buf[head], &data[*nio], bcount);
    ADVANCE;
  }

  /*
   * Process remainder.
   */
  while(count){
    /*
     * Process a trailing partial alb read.
     */
    if (count<albsize){
      READALB;
      bcopy(hfs->fs_buf,&data[*nio],count);
      *nio += count;
      return 0;
    }

    /*
     * Process as many albs as we have mapped.
     */
    bcount = MIN(count, albsize * MIN(albcnt,count/albsize) );
    if (error = io_aread(hfs,alb,bcount,&data[*nio]))
      return error;
    ADVANCE;
  }

  return 0;
}


/***********
 * hfs_write
 */
int hfs_write(hnode_t *hp, u_int offset, u_int count, char *data){
  int          error;
  u_int        *lsz, *psz, bcount, alb, albcnt, albsize, head;
  hfs_t	       *hfs=hp->h_hfs;
  u_int        nwrite;
  u_int        *nio = &nwrite;

  TRACE(("hfs_write:   hno: %4d, offset: %4d, count: %4d\n",
	hp->h_hno,
	offset,
	count));

  /*
   * cancel preallocation of other file/fork
   */
  
  if (hp != hfs->fs_prealloced)
  {   error = fs_cancel_prealloc(hfs);
      if (error)
	  return error;
  }

  /*
   * Initialize.
   */
  *nio = 0;
  albsize = hfs->fs_mdb.m_AlBlkSiz;
  switch(TAG(hp->h_hno)){
  case nt_data:
    if (_h_cdr(hp).c_Type != CDR_FILREC)
      return ENXIO;
    lsz = &_h_file(hp).LgLen;
    psz = &_h_file(hp).PyLen;
    break;
  case nt_resource:
    if (_h_cdr(hp).c_Type != CDR_FILREC)
      return ENXIO;
    lsz = &_h_file(hp).RLgLen;
    psz = &_h_file(hp).RPyLen;
    break;
  case nt_dotancillary:
    return ai_write(hp, offset, count, data);
  default:
    return ENXIO;
  }
  

  /*
   * Adjust file size if necessary.
   * Perform initial block mapping.
   * Update modification time.
   */
  if (offset + count > *lsz){
    if (offset + count > *psz) {
	u_int new_psize;			/* physical size */
	new_psize = offset + PREALLOCATION_SIZE;
	if (new_psize < offset + count)
	    new_psize = offset + count;

        if (hn_adj_size(hp, new_psize, NULL)) {
	    if (error = hn_adj_size(hp, offset + count, NULL))
		return error;
	}
	hfs->fs_prealloced = hp;
    }
    *lsz = offset+count;
    FR_SET(hp->h_frec,ATR);
  }
  ALBMAP;
  _h_file(hp).MdDat = to_hfstime(time(NULL));


  /*
   * If offset isn't ALB aligned, we have to process the initial unaligned
   * fragment.
   */
  if ((head = offset % albsize)!=0){
    bcount = MIN(albsize - head, count);
    io_awrite_partial(hfs, alb, head, bcount, &data[*nio]);
    ADVANCE;
  }

  /*
   * Process remainder.
   */
  while(count){
    /*
     * Process a trailing partial alb read.
     */
    if (count<albsize){
      io_awrite_partial(hfs, alb, 0, count, &data[*nio]);
      return 0;
    }

    /*
     * Process as many albs as we have mapped.
     */
    bcount = MIN(count, albsize * MIN(albcnt,count/albsize) );
    if (error = io_awrite(hfs,alb,bcount,&data[*nio]))
      return error;
    ADVANCE;
  }

  return 0;
}

/************
 * hfs_create
 */
int hfs_create(hnode_t *hp, char *name, sattr *sa,
	       hnode_t **chp, struct authunix_parms *aup){
  int error;
  frec_t *fp=NULL;
  int nt;

  TRACE(("hfs_create:  hno: %4d, name: \"%-16.16s\"\n", hp->h_hno, name));

  /* Check for ancillary file. */

  if (TAG(hp->h_hno)==0 && strcmp(name,dotancillary)==0)
    return hn_get(hp->h_hfs,HNO(FID(hp->h_hno),nt_dotancillary),chp);


  /*
   * Filter out protected files.
   * Check for write access to the directory.
   */
  if (protected(hp,name) || NOTWRITEABLE(hp))
    return EACCES;


  nt = TAG(hp->h_hno) ? nt_resource : nt_data;


  /* Check for file already existing */

  if ((error=dir_lookup(hp,name,&fp))==0){

    /* File exists */

    if (fp->f_cdr.c_Type==CDR_DIRREC){
      error = EISDIR;
      goto done;
    }
    
    /*
     * Fetch the hnode.
     * Set the file attributes.
     */
    if (error=hn_get(hp->h_hfs,HNO(fp->f_fid,nt),chp))
      goto done;
    
    error = hfs_setattr(*chp,sa);
    goto done;
  }
  if (error!=ENOENT)
    return error;

  /*
   * cancel preallocation
   */
  
  if (error = fs_cancel_prealloc(hp->h_hfs))
    return error;

  /*
   * Verify that the btree insertion will succeed.
   */
  if (error=bt_check(&hp->h_hfs->fs_cbt,1))
    return error;


  /* Create the files frec and enter it in the directory */

  if (error=create(hp,name,CDR_FILREC,&fp))
    goto done;

  /*
   * Create the hnode.
   * Add the file to the ancillary file.
   * Set the file attributes.
   */
  if (error=hn_get(hp->h_hfs,HNO(fp->f_fid,nt),chp))
    goto done;
  if (error=ar_add(hp->h_hfs, HNO_NR(hp->h_hno), (*chp)->h_frec->f_fid))
    goto done;
  hfs_setattr(*chp,sa);

  /*
   * HACK: We set the FR_CREATE flag to mark the frec as freshly created.
   *       This flag is used by the write code to allow write access to
   *       a readonly file.  This is needed when a readonly file is created.
   *       Note that this flag will be lost when the frec is purged from
   *       the cache but the writes will probably be done by then.  We will
   *	   replace this with something better when we think of it.
   *
   *       Note also, we are marking the frec instead of the hnode.  This
   *       is because the frec is common to the resource and data fork
   *       hnodes.  **HOWEVER** it's a moot point because the client side
   *       NFS code will block attempts to create the other fork of a 
   *       readonly file because it thinks of the other fork as a separate
   *	   file and therefore rejects the create attempt because lookup
   *       will tell it that the file already exists and is readonly.
   */
  FR_SET((*chp)->h_frec,CREATE);

  /*
   * Sync everything, that guarantees the directory entry makes it to
   * the disk or we get an error now.
   */
  error = fs_sync((*chp)->h_hfs, ALL);

 done:
  if (fp)
    fr_put(fp);
  return error;

}


/************
 * hfs_remove	Remove a file.
 */
int hfs_remove(hnode_t *hp, char *name){
  int      error;
  frec_t   *fp = NULL;
  hnode_t  *chp = NULL;
  fid_t    fid;

  TRACE(("hfs_remove:  hno: %4d, name: \"%-16.16s\"\n", hp->h_hno, name));

  /*
   * Filter out attempts to modify protected files.
   */
  if (protected(hp,name) || TAG(hp->h_hno) || NOTWRITEABLE(hp))
    return EACCES;

  /*
   * Lookup the file in the directory.  Fail if we don't find it.
   * Fail if it's a directory.
   * Fetch the hnode.
   */
  if (error=dir_lookup(hp,name,&fp))
    return error;
  if (fp->f_cdr.c_Type == CDR_DIRREC){
    error = EISDIR;
    goto done;
  }
  if (error=hn_get(hp->h_hfs,HNO(fp->f_fid,0),&chp))
    goto done;

  /*
   * Release the frec as we are going to delete it below.
   * Remove the file.
   * Remove any ancillary file entry.
   */
  fid = fp->f_fid;
  fr_put(fp);
  fp=NULL;
  if (error=remove_file(chp))
    goto done;
  chp=NULL;
  if (error=ar_remove(hp->h_hfs,hp->h_hno,fid))
    goto done;

  /*
   * Update the modification time of the directory.
   * Decrement the directory valence.
   * Mark the directory frec as attribute dirty.
   */
  _h_dir(hp).MdDat=to_hfstime(time(NULL));
  _h_dir(hp).Val--;
  FR_SET(hp->h_frec,ATR);

  /*
   * Update the modification time in the mdb.
   * Update the file count in the MDB.
   * If this is the root directory, update the root file count in the MDB.
   * Make the MDB attribute dirty.
   */
  _h_mdb(hp).m_LsMod = _h_dir(hp).MdDat;
  _h_mdb(hp).m_FilCnt--;
  if (ROOTDIR(hp))
    _h_mdb(hp).m_NmFls--;
  FS_SET(hp->h_hfs,MDB_DIRTY);

 done:
  if (fp)
    fr_put(fp);
  if (chp)
    hn_put(chp);
  return __chk(hp->h_hfs,error,hfs_remove);
}


/************
 * hfs_rename
 */
int hfs_rename(hnode_t *hp, char *name, hnode_t *thp, char *tname){
  hnode_t *xp=NULL;
  frec_t  *fp;
  fid_t   fid;
  int     error;

  /*
   * Check access.
   */
  if (protected(hp,name) || TAG(hp->h_hno) || NOTWRITEABLE(hp) ||
      protected(thp,tname) || TAG(thp->h_hno) || NOTWRITEABLE(thp))
    return EACCES;

  /*
   * Verify that the destination doesn't already exist.
   */
  if ((error=dir_lookup(thp,tname,&fp))==0){
    /*
     * Because HFS defines AAA and aaa to be the same filename, if the
     * destination exists and is in the same directory, we change the file's
     * btree key case so that readdir displays the right key.  We don't have
     * to deal with thread records as they are never displayed and will
     * match anyway.
     *
     * Unimplemented.
    if (hp==thp){
      change_key_case(fp,tname);
      fr_put(fp);
      return 0;
    }
    */
    fr_put(fp);
    return EEXIST;
  }
  if (error!=ENOENT)
    return error;

  /*
   * Verify that we can make the btree insertions.
   */
  if (error=bt_check(&hp->h_hfs->fs_cbt,2))
    return error;

  /*
   * Lookup the file.
   * If found, save the fid number and release the frec.
   * Fetch the file's node.
   */
  if (error=dir_lookup(hp,name,&fp))
    return error;
  fid = fp->f_fid;
  fr_put(fp);
  if (error=hn_get(hp->h_hfs,HNO(fid,0),&xp))
    return error;

  /*
   * If the xp equals thp, we are trying to rename a directory into itself!
   */
  if (xp==thp){
    error = EINVAL;
    goto done; 
  }
  
  /*
   * Dispatch according to type.
   */
  if (_h_cdr(xp).c_Type==CDR_DIRREC)
    error = rename_dir(hp,xp,thp,tname);
  else
    error = rename_file(hp,xp,thp,tname);

  /*
   * If we renamed to another directory, update the ancillary file.
   */
  if (error==0 && hp!=thp){
    __chkr(hp->h_hfs,
	  ar_remove(hp->h_hfs,hp->h_hno,fid),
	  hfs_rename);
    __chkr(thp->h_hfs,
	  ar_add(thp->h_hfs,thp->h_hno,fid),
	  hfs_rename);
  }

  done:
  if (xp)
    hn_put(xp);
  return error;
}


/*************
 * hfs_symlink
 */
int hfs_symlink(){ return ENXIO;}


/***********
 * hfs_mkdir
 */
int hfs_mkdir(hnode_t *hp, char *name, sattr *sa,
	      hnode_t **chp, struct authunix_parms *aup){
  int       error;
  rdesc_t   rd;
  frec_t    *fp;
  hfs_t     *hfs = hp->h_hfs;
  char      buf[sizeof(cdr_t)];
  catk_t    key;

  TRACE(("hfs_mkdir:   hno: %4d, name: \"%-16.16s\"\n", hp->h_hno, name));

  /*
   * Filter out protected files.
   * Check that the parent directory is writable.
   */
  if (protected(hp,name) || TAG(hp->h_hno) || NOTWRITEABLE(hp))
    return EACCES;

  /*
   * Check that it doesn't already exist.
   */
  if ((error=dir_lookup(hp,name,&fp))==0){
    fr_put(fp);
    return EEXIST;
  }
  if (error!=ENOENT)
    return error;

  /*
   * Verify that the btree insertion will succeed.
   * Create the directory entry.
   */
  if (error=bt_check(&hp->h_hfs->fs_cbt,2))
    return error;
  if (error=create(hp,name,CDR_DIRREC,&fp))
    goto done;


  /*
   * Create thread record key.
   * Create thread record.
   * Insert thread record in file.
   */
  fr_cons_key(fp->f_fid,&key,"");
  fr_cons_thd(fp,buf,&rd);
  if (error= bt_radd(&hfs->fs_cbt,(btk_t*)&key,&rd)){
    __chk(hfs,error,hfs_mkdir);
    goto done;
  }


  /*
   * Create the hnode.
   * Set the hnode mode data.
   * Set the directory attributes.
   */
  if (error = hn_get(hfs,HNO(fp->f_fid,0),chp))
    goto done;

  /*
   * Add entry to the ancillary file.
   */
  if (error=ar_add(hp->h_hfs,hp->h_hno,fp->f_fid))
    goto done;

  hfs_setattr(*chp,sa);

 done:
  if (fp)
    fr_put(fp);
  return error;
}


/***********
 * hfs_rmdir
 */
int hfs_rmdir(hnode_t *hp, char *name){
  int     error;
  hnode_t *chp=NULL, *rhp=NULL, *ahp=NULL;
  frec_t  *fp=NULL;
  catk_t  k;
  bt_t    *bt=&hp->h_hfs->fs_cbt;
  ar_t    *ap=NULL;

  TRACE(("hfs_rmdir:   hno: %4d, name: \"%-16.16s\"\n", hp->h_hno, name));

  /*
   * Access checks.
   */
  if (protected(hp,name) || TAG(hp->h_hno) || NOTWRITEABLE(hp))
    return EACCES;

  /*
   * Lookup the file in the directory.  Fail if we don't find it.
   * Fail if it isn't a directory.
   * Fetch the hnode.
   * Free the frec.
   */
  if (error=dir_lookup(hp,name,&fp))
    goto done;
  if (fp->f_cdr.c_Type != CDR_DIRREC){
    error = ENOTDIR;
    goto done;
  }
  if (error=hn_get(hp->h_hfs,HNO(fp->f_fid,0),&chp))
    goto err;
  fr_put(fp);
  fp=NULL;

  assert(TAG(chp->h_hno)==0);

  /*
   * Fail if the directory isn't empty.
   */
  if (_h_dir(chp).Val!=0){
    error = EEXIST;
    goto done;
  }
    
  /*
   * Get a reference to the directory frec as we need to free it.
   * Get a reference to the ancillary file ar if it's incore.
   */
  fp = chp->h_frec;
  ap = ar_incore(chp->h_hfs, chp->h_hno);

  /*
   * At this point, we can have up to 3 references to the directory frec, one
   * from the directory itself, one from its resource subdirectory, and one
   * from its ancillary file.  We must fetch each of these hnodes.  The
   * exit code will release them all.
   */
  if (error= hn_get(hp->h_hfs,HNO(fp->f_fid,nt_dotresource),&rhp))
    goto err;
  if (error= hn_get(hp->h_hfs,HNO(fp->f_fid,nt_dotancillary),&ahp))
    goto err;

  assert(fp->f_refcnt==3);
  assert(chp->h_refcnt==1);
  assert(rhp->h_refcnt==1);
  assert(ahp->h_refcnt==1);
  assert(ap==NULL || ap->a_refcnt==1);

  /*
   * Construct a thread record key for the directory.
   * Delete the directory record.
   * Delete the thread record.
   * Delete any parent directory ancillary record for this directory.
   */
  fr_cons_key(FID(chp->h_hno),&k,"");
  if (error=bt_rremove(bt, (btk_t*)&fp->f_key))
    goto err;
  if (error=bt_rremove(bt,(btk_t*)&k))
    goto err;
  if (error=ar_remove(hp->h_hfs, hp->h_hno, fp->f_fid))
    goto err;

  /*
   * We now have three incore orphan hnodes, one orphan frec and one
   *   orphan arec.
   * Free them all.
   */
  fr_free(fp);  fp=NULL;
  hn_free(chp); chp=NULL;
  hn_free(rhp); rhp=NULL;
  hn_free(ahp); ahp=NULL;
  if (ap)
    ar_free(ap);
  ap=NULL;


  /*
   * Update the modification time of the parent directory.
   * Decrement the parent directory valence.
   * Mark the parent directory frec as attribute dirty.
   */
  _h_dir(hp).MdDat=to_hfstime(time(NULL));
  _h_dir(hp).Val--;
  FR_SET(hp->h_frec,ATR);
  
  /*
   * Update the modification time in the mdb.
   * Update the directory count in the MDB.
   * If this is the root directory, update the root file count in the MDB.
   * Make the MDB attribute dirty.
   */
  _h_mdb(hp).m_LsMod = _h_dir(hp).MdDat;
  _h_mdb(hp).m_DirCnt--;
  if (ROOTDIR(hp))
    _h_mdb(hp).m_NmRtDirs--;
  FS_SET(hp->h_hfs,MDB_DIRTY);

  goto done;

 err:
  /*
   * We come here in response to "impossible" errors.  Signal file
   *   system corrupt.
   */
  __chk(bt->b_hfs,error,hfs_rmdir);
  
 done:
  if (fp)
    fr_put(fp);
  if (chp)
    hn_put(chp);
  if (rhp)
    hn_put(rhp);
  if (ahp)
    hn_put(ahp);
  if (ap)
    ar_put(ap);
  return error;
}



/*************
 * hfs_readdir	Read the directory entries of this node.
 *============
 *
 * If this node is a ".HSResource" node, then we ignore entries for directories
 * in this node.
 *
 * The nfscookie is cast to an unsigned integer offset interpreted as follows:
 *
 *        0: Offset of "." entry.			dc_dot
 *        1: Offset of ".." entry.			dc_dotdot
 * 2..Val+2: Files and directories in directory.	dc_entries
 *    Val+3: .HSResource entry.				dc_resource
 *    Val+4: .HSancillary entry.			dc_ancillary
 *
 * where Val is the directory "Valence".
 *
 * If between subsequent calls to this routine a new entry is made in the
 * directory, it is unpredictable whether it will be returned.
 *
 */
int hfs_readdir(hnode_t *hp,
		nfscookie cookie,
		u_int count,
		entry *entries,
		u_int *eof,
		u_int *nread){
  int 	  error=0;
  int     valence = _h_dir(hp).Val;
  dirs_t  ds;
  hnode_t *thp=NULL;

  ds.cur = entries;
  ds.last= NULL;
  ds.free= count;
  ds.hp  = hp;
  ds.offset = *(u_int*)cookie;

  *eof = 0;

  while (ds.free && ds.offset < valence+4){
    u_int  cnt;
    u_int  old_offset=ds.offset;
    switch(dcode(&ds)){

    case dc_dot:
      if (dir_fr_to_entry(hp->h_frec, &ds, dot, TAG(hp->h_hno)))
	goto done;
      break;

    case dc_dotdot:
      if (error=hn_get(hp->h_hfs,hp->h_phno,&thp))
	goto done;
      if (dir_fr_to_entry(thp->h_frec, &ds, dotdot, TAG(thp->h_hno)))
	goto done;
      hn_put(thp);
      thp=NULL;
      break;

    case dc_entries:
      if (valence!=0){
	cnt = ds.offset-2;
	if (error = dir_iterate(hp,&cnt, (dir_callback_t)readdir_cback, &ds))
	  goto done;
	if (ds.offset<=old_offset){
	  fs_corrupted(hp->h_hfs,"hfs_readdir");
	  error = EIO;
	  goto done;
	}
      }
      else
	ds.offset++;
      break;

    case dc_resource:
      if (TAG(hp->h_hno)!=0)
	ds.offset++;
      else
	if (dir_fr_to_entry(hp->h_frec, &ds, dotresource, nt_dotresource))
	  goto done;
      break;

    case dc_ancillary:
      if (TAG(hp->h_hno)!=0)
	ds.offset++;
      else{
	if (dir_fr_to_entry(hp->h_frec, &ds, dotancillary, nt_dotancillary))
	  goto done;
        /* successfully read the ancillary entry (which is always last?) */
        *eof = 1;
      }
      break;
    }
  }

 done:
  if (error<0)
    error = 0;
  if (thp)
    hn_put(thp);
  if (ds.last)
    ds.last->nextentry=NULL;
  *nread = (ds.cur-entries) * sizeof(entry);

  TRACE(("hfs_readdir: hno: %4d, count: %d,"
        " cookie: %d, nread: %d, last: %d\n\n",
	hp->h_hno,count, *(u_int*)cookie, *nread,
	(ds.last!=NULL) ? (int)ds.last->nextentry : -1));

  return error;
}


/*************
 * to_unixtime	Convert hfs time to unix time.
 */
u_int to_unixtime(u_int mtime){
  tzset();
  return mtime - FR_TIME_OFFSET + timezone;
}


/************
 * to_hfstime	Convert unix time to hfs time.
 */
u_int to_hfstime(u_int utime){
  tzset();
  return utime + FR_TIME_OFFSET - timezone;
}


/****************************************************************************
 *									    *
 *				Local Routines				    *
 *									    *
 ****************************************************************************/


/*************
 * remove_file	Remove a file
 */
static int remove_file(hnode_t *hp){
  hnode_t *rhp = NULL;
  int     error;
  hfs_t *hfs = hp->h_hfs;

  assert(hp->h_refcnt==1);
  assert(TAG(hp->h_hno)==0);

  /*
   * cancel preallocation
   */
  
  if (error = fs_cancel_prealloc(hfs))
      goto done;

  /*
   * Get the hnode for the resource fork.
   */
  if (error=hn_get(hp->h_hfs,HNO(FID(hp->h_hno),nt_resource), &rhp))
    goto done;
  
  assert(rhp->h_refcnt==1);
  assert(rhp->h_frec->f_refcnt==2);

  /*
   * Release all space in each fork.
   */
  if (error=hn_adj_size(rhp,0,NULL))
    goto done;
  if (error=hn_adj_size(hp,0,NULL))
    goto done;

  /*
   * Mark everything clean as the er_update may trigger a sync.
   * Update the erecs so we can free them.
   */
  FR_CLR(hp->h_frec,DIRTY);
  HN_CLR(hp,DIRTY);
  HN_CLR(rhp,DIRTY);
  if (error=er_update(hp->h_erec,0))
    goto done;
  if (error=er_update(rhp->h_erec,0))
    goto done;

  /*
   * Remove the frec from the tree.
   */
  if (error=bt_rremove(&hp->h_hfs->fs_cbt, (btk_t*)&hp->h_frec->f_key))
    goto done;
  
  /*
   * The hnodes now point to an orphan frec and erecs and the hnodes themselves
   * are now orphans.  Free them all.
   */
  fr_free(hp->h_frec);
  er_free(hp->h_erec);
  er_free(rhp->h_erec);
  hn_free(hp);
  hn_free(rhp);
  rhp=NULL;

 done:
  if (rhp)
    hn_put(rhp);
  return __chk(hp->h_hfs,error,remove_file);
}


/*********
 * hfs_chk	Performs error checking
 */
int hfs_chk(hfs_t *hfs, int sts, char *str){
  if (sts)
    fs_corrupted(hfs,str);
  return sts;
}


/***********
 * protected	Checks for protected files.
 */
static int protected(hnode_t *hp, char *name){
  /*
   * Protect ".", "..", ".HCResource" and ".HCAncillary".
   */
  if (strcmp(name,dot)==0 ||
      strcmp(name,dotdot)==0 ||
      strcmp(name,dotresource)==0 ||
      strcmp(name,dotancillary)==0)
    return EACCES;

  return 0;
}


/********
 * create	Create a file or a directory record.
 */
static int create(hnode_t *hp, char *name, u_char type, frec_t **frp){
  int     error;
  frec_t  *fp;
  char    buf[sizeof(cdr_t)];
  rdesc_t rd;
  hfs_t   *hfs=hp->h_hfs;

  *frp=0;

  /*
   * Get a free frec structure and then initialize it.
   */
  fr_new(hfs,&fp);
  fp->f_hfs = hfs;
  fp->f_fid = hfs->fs_mdb.m_NxtCNID;
  fp->f_pfid = FID(hp->h_hno);
  if (error=dir_init_fr(fp,name,to_hfstime(time(NULL)),type))
    return error;


  TRACEONCE("hfs_mkdir - frec mode not being handled\n");

  /*
   * Set up the record descriptor.
   * Pack the cdr into the record.
   * Insert the directory record into the btree.
   * Mark the frec active as it's real after its inserted into the btree.
   */
  rd.data = buf;
  rd.count= pack(&fp->f_cdr,
		 rd.data,
		 (type==CDR_DIRREC) ? &cdrdirpack : &cdrfilepack);
  if (error=bt_radd(&hfs->fs_cbt,(btk_t*)&fp->f_key,&rd)){
    fr_free(fp);
    return error;
  }
  fr_activate(fp);

  /*
   * Increment the CNID field in the mdb, we are committed now.
   * Increment the directory/file count.
   * If in the root directory, increment the root directory/file count.
   * Increment the directory valence and update the modification time.
   */
  hfs->fs_mdb.m_NxtCNID++;
  if (type==CDR_DIRREC){
    hfs->fs_mdb.m_DirCnt++;
    if (ROOTDIR(hp))
      hfs->fs_mdb.m_NmRtDirs++;
  }
  else{
    hfs->fs_mdb.m_FilCnt++;
    if (ROOTDIR(hp))
      hfs->fs_mdb.m_NmFls++;
  }
  hp->h_frec->f_cdr.c.c_Dir.Val++;
  hp->h_frec->f_cdr.c.c_Dir.MdDat=to_hfstime(time(NULL));
  FR_SET(hp->h_frec,ATR);
  FS_SET(hfs,MDB_ATR);

  *frp = fp;

  return 0;
}


/*************
 * rename_file
 *============
 *
 * NOTE: While intended to rename a file, this code is called by rename_dir
 *       to rename directory frec's. Rename_dir then handles renaming the
 *       thread record.
 */
static int rename_file(hnode_t *dhp, hnode_t *hp, hnode_t *thp, char *name){
  char    buf[sizeof(cdr_t)];
  rdesc_t rd;
  int     error;

  /*
   * Check that the new name is valid.  We don't want dir_rename_fr to fail
   * after we have deleted the current entry.
   */
  if (error=dir_validate_uname(name))
    return error;

  /*
   * Delete the existing frec from the btree.
   * Decrement the valence of the source directory.
   * Adjust the modify time of the source directory.
   */
  if (error=bt_rremove(&_h_cbt(hp),(btk_t*)&hp->h_frec->f_key))
    return error;
  _h_dir(dhp).Val--;
  _h_dir(dhp).MdDat=to_hfstime(time(NULL));
  FR_SET(dhp->h_frec,ATR);
  

  /*
   * Change the name and parent fid of the file's frec.
   */
  dir_rename_fr(hp->h_frec,FID(thp->h_hno),name);

  /*
   * Construct a new cdr record for insertion.
   * As this code is also called to rename directory frecs, check the record
   *   type when calling pack.
   */
  rd.data  = buf;
  rd.count = pack(&_h_cdr(hp),
		  buf,
		  _h_cdr(hp).c_Type==CDR_FILREC ? &cdrfilepack : &cdrdirpack);

  /*
   * Insert the record at its new position.
   * Increment the valence of the target directory.
   * Adjust the modification time of the target directory.
   */
  __chkr(hp->h_hfs,
	 bt_radd(&_h_cbt(hp),(btk_t*)&hp->h_frec->f_key,&rd),
	 rename_file);
  _h_dir(thp).Val++;
  _h_dir(thp).MdDat=to_hfstime(time(NULL));
  FR_SET(thp->h_frec,ATR);

  return 0;
}


/************
 * rename_dir
 */
static int rename_dir(hnode_t *dhp, hnode_t *hp, hnode_t *thp, char *name){
  char    buf[sizeof(cdr_t)+sizeof(btk_t)];
  btk_t   key;
  rdesc_t rd;
  int     error;

  /*
   * Rename the frec itself.
   */
  if (error = rename_file(dhp,hp,thp,name))
    return error;

  /*
   * Delete the thread record.
   */
  fr_cons_key(hp->h_frec->f_fid, (catk_t*)&key,"");
  __chkr(hp->h_hfs,
	 bt_rremove(&_h_cbt(hp),&key),
	 rename_dir);

  /*
   * Add a new thread record.
   */
  fr_cons_thd(hp->h_frec,buf,&rd);
  return __chk(hp->h_hfs,
	       bt_radd(&_h_cbt(hp),&key,&rd),
	       rename_dir);
}


/************
 * getattr_ai	Get attributes for ancillary file.
 */
static int getattr_ai(hnode_t *hp, fattr *fa){
  hnode_t *dhp;
  ar_t    *ap;

  __chkr(hp->h_hfs,
	 hn_get(hp->h_hfs,HNO(FID(hp->h_hno),0),&dhp),
	 hfs_getattr);

  ap=ar_incore(hp->h_hfs, hp->h_hno);

  fa->mode |= NFSMODE_REG;
  fa->size = _h_dir(dhp).Val*AI_SIZE;
  fa->blocks = BTOBB(fa->size);
  if (ap){
    fa->ctime.seconds = to_unixtime(ap->a_cdate);
    fa->mtime.seconds = to_unixtime(ap->a_mdate);
  }
  else{
    fa->ctime.seconds = to_unixtime(_h_dir(hp).CrDat);
    fa->mtime.seconds = to_unixtime(_h_dir(hp).MdDat);
  }
  fa->ctime.useconds = 0;
  fa->mtime.useconds = 0;
  hn_put(dhp);
  if (ap)
    ar_put(ap);

  return 0;
}


/***************
 * readdir_cback	Iterator callback for readdir.
 */
static int readdir_cback(hnode_t *dhp, frec_t *fp, dirs_t *dsp){
  int tag = TAG(dsp->hp->h_hno)==nt_dotresource ? 1 : 0;
  if (dir_fr_to_entry(fp,dsp,NULL,tag))
    return -1;
  return 0;
}


/********
 * dcode	Generate directory code for an entry.
 *=======
 *
 * NOTE:	This routine relies on the numeric constants associated with
 *		the enum dirc_t.
 */
static dirc_t dcode(dirs_t *dsp){
  dirc_t c = dsp->offset;
  u_int  vp2 = _h_dir(dsp->hp).Val + 2;
  
  if (c>=2)
    if (c<vp2)
      c = dc_entries;
    else
      c = (c-vp2) + dc_resource;
  
  return c;
}

