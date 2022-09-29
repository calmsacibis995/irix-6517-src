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

#include <stddef.h>
#include <syslog.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/smfd.h>
#include "hfs.h"


/* Forward Routines */

static int albof(ExtDescRec_t *);
static int readmdb(hfs_t *hfs);
static int readvbm(hfs_t *hfs);
static int writemdb(hfs_t *hfs, u_int flags);
static int writevbm(hfs_t *hfs, u_int flags);
static int get_partition(hfs_t *hfs, u_int partition);
extern int remmedinfo( int fd, uint *status ); /* declared in libdisk */


/* Global timezone delta in seconds from gmt. */

u_int tzdelta;


/* Head of list of file systems */

static hfs_t *hfslist;
static fsid_t fsid;


/*****************************************************************************
 *                                                                           *
 *                            Global Routines                                *
 *                                                                           *
 *****************************************************************************/

/********
 * fs_add	Add a file system to the server.
 */
int fs_add(char *pathname, u_int flags, u_int partition, hfs_t **hfs){
  hfs_t *nhfs;
  int   error;


  /*
   * Allocate the file structure.
   */
  nhfs = (hfs_t*)safe_malloc(sizeof(hfs_t));
  bzero(nhfs, sizeof(hfs_t));


  /*
   * Initialize the physical IO.
   */
  nhfs->fs_flags = flags;
  if (error = io_open(pathname, nhfs))
    goto bad;


  /* 
   * Perform the physical mount.
   */
  if (error = fs_mount(nhfs,partition))
    goto bad;
  

  /*
   * Attach the file system to the list of known file systems
   * and return.
   */
  nhfs->fs_next = hfslist;
  nhfs->fs_fsid = fsid++;
  hfslist = nhfs;
  *hfs = nhfs;
  return 0;

 bad:
  fs_destroy(nhfs);
  return error;
}


/********
 * fs_get	Look up a file system by file system id.
 */
hfs_t *fs_get(fsid_t fs){
  hfs_t *hfs;
  
  for (hfs=hfslist;hfs;hfs=hfs->fs_next)
    if (fs == hfs->fs_fsid){
      if (FS_ISSET(hfs,FLOPPY)){
	/*
	 * Check media status of floppy.  It may have been manually
	 * ejected.  New media may have been inserted.  In either case,
	 * we destroy the incore filesystem data and return NULL.  The
	 * only thing the user can do is unmount the volume and start
	 * over.
	 */
	int status = 0;
	if ( (ioctl(hfs->fs_fd, SMFDMEDIA,&status) && 
              remmedinfo(hfs->fs_fd, (uint *)&status)) ||
              status==0 ||
              status & SMFDMEDIA_CHANGED){
              fs_destroy(hfs);
     	      return NULL;
	}
      }
      return hfs;
    }
  return NULL;
}


/************
 * fs_destroy		Reclaim all file system space.
 */
void fs_destroy(hfs_t *hfs){
  hfs_t *thfs;
  ar_destroy(hfs);		/* Destroy all the arecs */
  er_destroy(hfs);		/* Destroy all the erecs */
  fr_destroy(hfs);		/* Destroy all the frecs */
  hn_destroy(hfs);		/* Destory all the hnodes */
  bt_destroy(&hfs->fs_cbt);	/* Destroy the catalog btree */
  bt_destroy(&hfs->fs_ebt);	/* Destroy the extents btree */
  
  if (hfs->fs_buf)		/* Free the alb buffer */
    free(hfs->fs_buf);
  if (hfs->fs_vbm)		/* Free the volume bitmap */
    free(hfs->fs_vbm);

  /*
   * Scan the list of file systems and remove ourselve from the list if
   *  we are on it.  We might not be on it if we get called from a failed
   *  mount attempt.
   */
  if (hfslist==hfs)
    hfslist = hfs->fs_next;
  else{
    for (thfs=hfslist;thfs;thfs=thfs->fs_next)
      if (thfs->fs_next == hfs){
	thfs->fs_next = hfs->fs_next;
	goto done;
      }
  }
 done:
  free(hfs);
}


/***********
 * fs_remove		Dismount a file system.
 */
void fs_remove(hfs_t *hfs){
  TRACE(("fs_remove\n"));
  if (FS_ISCLR(hfs,RDONLY))
  {
    fs_cancel_prealloc(hfs);
    fs_sync(hfs,ALL);		/* Note: fs_sync(,all) marks vol unmounted. */
  }
  fs_destroy(hfs);
}


/***************
 * fs_remove_all	Dismount all file systems.
 */
void fs_remove_all(void){
  hfs_t *hfs = hfslist, *nexthfs;

  TRACE(("fs_remove_all\n"));

  while (hfs){
    nexthfs =hfs->fs_next;
    fs_remove(hfs);
    hfs = nexthfs;
  }
}

#define BAD_HFS(hfs)	\
	(hfs->fs_cbt.b_hdr.bh_NNodes == 0 || hfs->fs_cbt.b_hdr.bh_KeyLen == 0)


/**********
 * fs_mount
 */
int fs_mount(hfs_t *hfs, u_int partition){
  int error;

  /*
   * Process partition.
   */
  if (error=get_partition(hfs,partition))
    goto done;

  /*
   * Read MDB.
   * Check for software readonly file system.
   *
   * NOTE:  We don't yet mark r/w filesystems as mounted.  We let
   *        the io_write/fs_push_mdb routines handle that.
   */
  if (error = readmdb(hfs))
    goto done;
  if (hfs->fs_mdb.m_Atrb & Atrb_SW_Locked)
    FS_SET(hfs,RDONLY);

  /*
   * Read VBM.
   */
  if (error = readvbm(hfs))
    goto done;

  /*
   * Initialize the catalog and extent btrees.
   * Initialize the hnode, frec and erec sets.
   * Fetch the erec's for the extent and catalog btrees.
   */
  if (error = bt_init(hfs, &hfs->fs_cbt, CATALOG, CATNODEMAX))
    goto done;
  if (error = BAD_HFS(hfs)) {
    syslog(LOG_WARNING,"Bad HFS file system, no files or zero key size");
	goto done;
  }
  if (error = bt_init(hfs, &hfs->fs_ebt, EXTOVRF, EXTNODEMAX))
    goto done;
  if (error = hn_init(hfs, HNINITIAL, HNCACHEBINS))
    goto done;
  if (error = fr_init(hfs, HNINITIAL+32, FRCACHEBINS))
    goto done;
  if (error = er_init(hfs, HNINITIAL+16, ERCACHEBINS))
    goto done;
  if (error = ar_init(hfs, ARPCACHEBINS))
    goto done;

  if (error = er_get(hfs,
		     EXTOVRF_FILE,
		     0,
		     albof(&hfs->fs_mdb.m_XTExtRec),
		     &hfs->fs_ebt.b_erec))
    goto done;
  if (error = er_get(hfs,
		     CATALOG_FILE,
		     0,
		     albof(&hfs->fs_mdb.m_CTExtRec),
		     &hfs->fs_cbt.b_erec))
    goto done;


  /*
   * Allocate an alb sized buffer.
   * Set the current contents of the alb buffer to an impossible alb.
   * Set the albsize in BB's.
   * Set the starting logical block for alb'0.
   */
  hfs->fs_buf = (char*)safe_malloc(hfs->fs_mdb.m_AlBlkSiz);
  hfs->fs_alb = 0xffffffff;
  hfs->fs_albsize = BTOBB(hfs->fs_mdb.m_AlBlkSiz);

 done:
  return error;
}

/*********
 * fs_sync	Synchronize file system incore structures with disk.
 */
int fs_sync(hfs_t *hfs, u_int flags){
  int error;

  TRACEONCE("fs_sync\n");

  /*
   * Nothing to do if the file system is read only.
   */
  if (FS_ISSET(hfs,RDONLY))
    return 0;


  /* If nothing specific was requested, sync everything */

  if (flags==0)
    flags = ALL;

  if (error = writevbm(hfs,flags))
    goto bad;
  if (error = hn_sync(hfs,flags))
    goto bad;
  if (error = fr_sync(hfs,flags))
    goto bad;
  if (error = er_sync(hfs,flags))
    goto bad;
  if (error = bt_sync(&hfs->fs_cbt,flags))
    goto bad;
  if (error = bt_sync(&hfs->fs_ebt,flags))
    goto bad;

  if (flags==ALL && ((hfs->fs_mdb.m_Atrb & Atrb_UnMounted)==0)){
    /*
     * If flags == ALL then we have sync'ed everything and the
     * file system will be coherent as soon as we write the mdb.
     * Since the everything is coherent, set the unmounted bit in
     * the MDB and set MDB_ATR to force the MDB to  be written.  Note
     * that MDB_SYNC_ESCAPE must be set also to prevent fs_mdb_push()
     * from putting us into a loop.
     *
     * NOTE: Significant btree manipulations are supposed to be
     *       preceeded by a sync so that the whole btree transaction can
     *       be performed within the btree caches.  This isn't a well
     *       tested premise.
     */
    FS_SET(hfs,MDB_ATR);
    hfs->fs_mdb.m_Atrb |= Atrb_UnMounted;
    TRACE(("fs_sync - Atrb_UnMounted\n"));
  }

  FS_SET(hfs,MDB_SYNC_ESCAPE);
  error = writemdb(hfs,flags);
  FS_CLR(hfs,MDB_SYNC_ESCAPE);
  if (error)
    goto bad;
  return 0;
 bad:
  return __chk(hfs,error,fs_sync);
}


/*************
 * fs_sync_all	Sync all file systems.
 */
void fs_sync_all(void){
  hfs_t *hfs = hfslist;

  TRACEONCE("fs_sync_all\n");

  while (hfs){
    fs_cancel_prealloc(hfs);
    fs_sync(hfs,0);
    hfs=hfs->fs_next;
  }
}


/**************
 * fs_corrupted	Called to mark the file system as corrupted.
 */
void fs_corrupted(hfs_t *hfs, char *str){
  TRACE(("file system corrupted [%s]\n", str));
  if (FS_ISCLR(hfs,CORRUPTED))
    syslog(LOG_ALERT,"file system corrupted [%s]",str);
  FS_SET(hfs,CORRUPTED);
}



/*************
 * fs_push_mdb	Push the mdb to disk.
 *------------
 *
 * Called from the write routines to check if the mdb is marked as unmounted.
 * If so, mark it as mounted and write it to disk.  This routine "pushes"
 * the mdb to disk marked as mounted. On the otherhand, fs_sync pushes an
 * "unmounted" mdb to disk after a  full sync.  The result is that the mdb
 * becomes marked as mounted on any write, and becomes marked as unmounted by
 * and fs_sync(,ALL).  The goal is to keep the the mdb marked as unmounted so
 * long as there hasn't been any write activity to the volume.
 */
int fs_push_mdb(hfs_t *hfs){
  if (hfs->fs_mdb.m_Atrb & Atrb_UnMounted && FS_ISCLR(hfs,MDB_SYNC_ESCAPE)){
    hfs->fs_mdb.m_Atrb &= ~Atrb_UnMounted;
    FS_SET(hfs,MDB_ATR);
    TRACE(("fs_push_mdb - ~Atrb_UnMounted\n"));
    __chkr(hfs, writemdb(hfs,MDB), fs_push_mdb);
  }
  return 0;
}

int fs_cancel_prealloc(hfs_t *hfs)
{
    hnode_t *hp = hfs->fs_prealloced;

    if (hp)
    {
	u_int lsz;

	switch(TAG(hp->h_hno))
	{
	case nt_data:
	    lsz = _h_file(hp).LgLen;
	    break;
	case nt_resource:
	    lsz = _h_file(hp).RLgLen;
	    break;
	default:
	    return ENXIO;
	}
	__chkr(hfs, hn_adj_size(hp, lsz, NULL), "fs_cancel_prealloc");
	hfs->fs_prealloced = NULL;
    }
    return 0;
}

/*****************************************************************************
 *                                                                           *
 *                             Local Routines                                *
 *                                                                           *
 *****************************************************************************/


#define OLDPTNOFF(m) sizeof(((oldptn_t*)0)->m), offsetof(oldptn_t,m)

static u_short oldptnpack[]={
  OLDPTNOFF(p_Start),
  OLDPTNOFF(p_Size),
  OLDPTNOFF(p_FSID),
  0,0};
  
#define PTNOFF(m) sizeof(((ptn_t*)0)->m), offsetof(ptn_t,m)
#define PTNOFFn(m,n) n*sizeof(((ptn_t*)0)->m), offsetof(ptn_t,m)

static u_short ptnpack[]={
  PTNOFF(p_Sig),
  PTNOFF(p_SigPad),
  PTNOFF(p_BlkCnt),
  PTNOFF(p_PyPartStart),
  PTNOFF(p_PartBlkCnt),
  PTNOFFn(p_PartName[0],32),
  PTNOFFn(p_PartType[0],32),
  0,0
};

/***************
 * get_partition	Locate partition.
 */
static int get_partition(hfs_t *hfs, u_int partition){
  int error;
  char buf[BBSIZE];

  /*
   * Read the partition block.  If we can't read it, treat it as
   * an unpartitioned volume.  That allows us to read floppys with
   * a bad block in the partition block (which isn't really used
   * on floppies).  If it is a partitioned volume with a bad block,
   * then the partition base won't be zero and we will fail later.
   */
  if (error=io_pread(hfs, PTNBLK, BBSIZE, buf))
    buf[0]=0;  /* Makes it neither partition style */

  if (buf[0]==PTNSIG0 && buf[1]==PTNSIG1){
    /*
     * Process HFS partition.
     */
    ptn_t ptn;
    int   p=1,j;

    ptn.p_BlkCnt=1;
    for (j=1;j<=ptn.p_BlkCnt;j++){
      unpack(buf,&ptn,ptnpack);
      if (ptn.p_Sig!=PTNSIG)
	break;
      if (strncmp(ptn.p_PartType,PTNHFS,sizeof ptn.p_PartType)==0)
	if (partition==0 || p++ ==partition){
	  hfs->fs_ptnbase = ptn.p_PyPartStart;
	  hfs->fs_ptnsize = ptn.p_PartBlkCnt;
	  return 0;
	}
      if (error=io_pread(hfs,PTNBLK+j,BBSIZE,buf))
	return error;
    }
    return ENXIO;
  }
  else if (buf[0]==OLDPTNSIG0 && buf[1]==OLDPTNSIG1){
    /*
     * Process old style HFS partition.
     */
    oldptn_t ptn;
    char *b=&buf[2];
    int i=1;
    
    while(1){
      unpack(b,&ptn,oldptnpack);
      b +=12;
      if (ptn.p_Start==0 && ptn.p_Size==0 && ptn.p_FSID==0)
	return EIO;
      if (ptn.p_FSID==OLDPTNFSID && (partition==0 || i==partition)){
	hfs->fs_ptnbase = ptn.p_Start;
	hfs->fs_ptnsize = ptn.p_Size;
	return 0;
      }
      i++;
    }
  }
  else{
    /*
     * Default processing if no partition map blocks are found.
     */
    hfs->fs_ptnbase = 0;
    if (error=io_capacity(hfs,&hfs->fs_ptnsize))
      return error;
  }
  return 0;
}


/* Structure for packing/unpacking MDB */


#define MDBOFF(m) sizeof(((mdb_t*)0)->m), offsetof(mdb_t,m)
#define MDBOFFn(m,n) n*sizeof(((mdb_t*)0)->m), offsetof(mdb_t,m)


static u_short mdbpack[]={
  MDBOFF(m_SigWord),
  MDBOFF(m_CrDate),
  MDBOFF(m_LsMod),
  MDBOFF(m_Atrb),
  MDBOFF(m_NmFls),
  MDBOFF(m_VBMSt),
  MDBOFF(m_AllocPtr),
  MDBOFF(m_NmAlBlks),
  MDBOFF(m_AlBlkSiz),
  MDBOFF(m_ClpSiz),
  MDBOFF(m_AlBlSt),
  MDBOFF(m_NxtCNID),
  MDBOFF(m_FreeBlks),
  MDBOFFn(m_VN[0],28),
  MDBOFF(m_VolBkUp),
  MDBOFF(m_VSeqNum),
  MDBOFF(m_WrCnt),
  MDBOFF(m_XtClpSiz),
  MDBOFF(m_CtClpSiz),
  MDBOFF(m_NmRtDirs),
  MDBOFF(m_FilCnt),
  MDBOFF(m_DirCnt),
  MDBOFFn(m_FndrInfo[0],8),	/* MDBOFFn for array entries */
  MDBOFF(m_VCSize),
  MDBOFF(m_VBMCSize),
  MDBOFF(m_CtlCSize),
  MDBOFF(m_XTFlSize),
  MDBOFF(m_XTExtRec),
  MDBOFF(m_CTFlSize),
  MDBOFF(m_CTExtRec),
  0,0};


/*********
 * readmdb	Reads and unpacks mdb
 */
static int readmdb(hfs_t *hfs){
  char buf[BBSIZE];
  int error;
  
  /* Read the on-disk mdb into a buffer. */
  
  if (error = io_lread(hfs, MDBLBLK, BBSIZE, buf))
    return error;

  /* Unpack the buffer into the mdb */
  
  unpack(buf, &hfs->fs_mdb, mdbpack);

  /* Verify the signature */

  if (hfs->fs_mdb.m_SigWord != MDBSIGNATURE)
    return EINVAL;

  /* Verify that the volume was properly dismounted */

  if (hfs->fs_mdb.m_Atrb & Atrb_UnMounted)
    return 0;

  return ENOSPC;
}


/**********
 * writemdb	Packs and writes mdb
 *---------
 *
 * NOTE:	For the most part the MDB contains only attributes.
 *		However, it does contain the extent data for
 *		the catalog and the extent overflow files.  Anytime
 *		those are changed, the mdb must be marked dirty and
 *		a full sync must be performed as we cannot separately
 *		update mdb attributes from mdb allocation data.
 */
static int writemdb(hfs_t *hfs, u_int flags){
  char buf[BBSIZE];
  int error;

  /* Exit if not writing mdb */

  if (!(flags & MDB))
    return 0;

  /*
   * Exit if not dirty.
   */
  if (FS_ISCLR(hfs,MDB_DIRTY))
    return 0;

  /*
   * Pack the mdb into a buffer.
   * Write the buffer to disk.
   * If ALC dirty, write the second copy of the mdb.
   */
  bzero(buf, sizeof buf);
  pack(&hfs->fs_mdb, buf, mdbpack);
  error = io_lwrite(hfs, MDBLBLK, BBSIZE, buf);
  if (FS_ISSET(hfs,MDB_ALC))
    io_lwrite(hfs,PTOL(hfs,hfs->fs_ptnsize)-2,BBSIZE, buf);

  FS_CLR(hfs,MDB_DIRTY);

  return __chk(hfs,error,writemdb);
}


/*********
 * readvbm	Read VBM into memory from disk.
 */
static int readvbm(hfs_t *hfs){
  int vbmsize = BBTOB(hfs->fs_mdb.m_AlBlSt - hfs->fs_mdb.m_VBMSt);

  if (hfs->fs_vbm==NULL)
    hfs->fs_vbm = (vbm_t*)safe_malloc(vbmsize);
  return io_lread(hfs, hfs->fs_mdb.m_VBMSt, vbmsize, hfs->fs_vbm);
}


/**********
 * writevbm	Write VBM from memory to disk.
 */
static int writevbm(hfs_t *hfs, u_int flags){
  int error;
  int vbmsize = BBTOB(hfs->fs_mdb.m_AlBlSt - hfs->fs_mdb.m_VBMSt);

  if (FS_ISCLR(hfs,VBM_DIRTY))
    return 0;
  error = io_lwrite(hfs, hfs->fs_mdb.m_VBMSt, vbmsize,hfs->fs_vbm);
  
  FS_CLR(hfs,VBM_DIRTY);
  return __chk(hfs,error,writevbm);
}


/*******
 * albof	Returns address of next alb following an extent.
 */
static int albof(ExtDescRec_t *extr){
  int i,r;

  for (i=0,r=0;i<3;i++)
    r+= extr->exts[i].ed_NumABlks;
  return r;
}




