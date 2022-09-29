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

#ifndef __HFS_FS_H__
#define __HFS_FS_H__


#include "cdrom.h"

#ident "$Revision: 1.8 $"


/*
 * Block mapping macros.
 *
 * PTOL - physical to logical
 * LTOP - logical to physical
 * ATOL - allocation to logical
 *
 * LOGICALRANGECHECK - verifies that range of blocks lies within partition.
 *
 * Note: The tests of the physical mappings is somewhat weak as the only media
 *       used for testing involves 512 byte physical sectors.
 */
#define PTOL(hfs,blk) (blk*(hfs->fs_pbsize/BBSIZE))
#define LOGICALRANGECHECK(hfs,blk,count)\
  ((blk + BTOBB(count)) > PTOL(hfs,hfs->fs_ptnsize))
#define LTOP(hfs,blk)\
  (blk*(hfs->fs_pbsize/BBSIZE) + hfs->fs_ptnbase)
#define ATOL(hfs, blk)\
  (hfs->fs_mdb.m_AlBlSt + blk * hfs->fs_albsize)



/* New style hfs partition blocks */

#define PTNBLK 1

typedef struct ptn{
  u_short	p_Sig;		/* Signature */
  u_short	p_SigPad;
  u_int		p_BlkCnt;	/* Number of blocks in partition map */
  u_int		p_PyPartStart;	/* First physical block of partition */
  u_int		p_PartBlkCnt;	/* Number of blocks in partition */
  char		p_PartName[32];	/* Partition name */
  char		p_PartType[32];	/* Partition type */
} ptn_t;

#define PTNSIG  0x504d
#define PTNSIG0 (PTNSIG>>8)
#define PTNSIG1 (PTNSIG&0xff)

#define PTNHFS "Apple_HFS"


/* Old style hfs partition blocks */

typedef struct oldptn{
  u_int		p_Start;	/* Starting block address of partition */
  u_int		p_Size;		/* Number of blocks in partition */
  u_int		p_FSID;		/* File system id ("TFS1") */
} oldptn_t;

#define OLDPTNSIG0 0x54
#define OLDPTNSIG1 0x53
#define OLDPTNFSID 'TFS1'

/* Master directory block */

typedef struct mdb{
  u_short	m_SigWord;	/* Signature. */
  u_int		m_CrDate;	/* Creation date and type. */
  u_int		m_LsMod;	/* Date and time of last modification. */
  u_short	m_Atrb;		/* Volume attributes. */
  u_short	m_NmFls;	/* Number of files in root directory. */
  u_short	m_VBMSt;	/* First block of volume bitmap. */
  u_short	m_AllocPtr;	/* Start of next allocation search. */
  u_short	m_NmAlBlks;	/* Number of allocation blocks in volume. */
  u_int		m_AlBlkSiz;	/* Size of allocation blocks in bytes. */
  u_int		m_ClpSiz;	/* Default clump size. */
  u_short	m_AlBlSt;	/* First allocation block in volume */
  u_int		m_NxtCNID;	/* Next unused catalog node id. */
  u_short	m_FreeBlks;	/* Number of unused allocation blocks. */
  char		m_VN[28];	/* Volume name. */
  u_int		m_VolBkUp;	/* Date and time of last backup. */
  u_short	m_VSeqNum;	/* Volume backup sequenct number; */
  u_int		m_WrCnt;	/* Volume write count. */
  u_int		m_XtClpSiz;	/* Extents overflow file clump size. */
  u_int		m_CtClpSiz;	/* Clump size for catalog file. */
  u_short	m_NmRtDirs;	/* Number of directories in root directory. */
  u_int		m_FilCnt;	/* Number of files in volume. */
  u_int		m_DirCnt;	/* Number of directories in volume. */
  u_int		m_FndrInfo[8];	/* Finder info. */
  u_short	m_VCSize;	/* Size in blocks of volume cache. */
  u_short	m_VBMCSize;	/* Size in blocks of volume bitmap cache. */
  u_short	m_CtlCSize;	/* Size in blocks of common volume cache. */
  u_int		m_XTFlSize;	/* Size of extents overflow file (albs) */
  ExtDescRec_t	m_XTExtRec;	/* Extent records for extents overflow file. */
  u_int		m_CTFlSize;	/* Size of catalog file (albs) */
  ExtDescRec_t	m_CTExtRec;	/* Extent records for catalog file. */
} mdb_t;

#define MDBSIGNATURE 0x4244
#define MDBLBLK 2

#define Atrb_HW_Locked  (1<<7)	/* Locked by HW */
#define Atrb_UnMounted  (1<<8)	/* Volume unmounted */
#define Atrb_SparedBad  (1<<9)	/* Volume has had bad blocks spared */
#define Atrb_SW_Locked  (1<<15)	/* Locked by SW */


/* Volume bitmap */

typedef u_char vbm_t;

#define VBMLBLK 3



typedef u_short fsid_t;

/* File system structure */


struct hfs{
  struct hfs   	*fs_next;	/* Link to next file system */
  u_int		fs_flags;	/* File system flags */
  int		fs_fd;		/* File system file descriptor */
  CDROM		*fs_cd;		/* Ptr to CDROM block. */
  fsid_t 	fs_fsid;	/* File system identifier */
  hlroot_t	fs_hnode_list;	/* Hnode hash list */
  hlroot_t	fs_frec_list;	/* Frec hash list */
  hlroot_t	fs_erec_list;	/* Erec hash list */
  hlroot_t	fs_fidc_list;	/* Fidc hash list */
  hlroot_t      fs_arec_list;	/* Arec hash list */
  mdb_t		fs_mdb;		/* Master directory block */
  bt_t		fs_cbt;		/* Catalog btree */
  bt_t		fs_ebt;		/* Extents overflot file btree */
  vbm_t		*fs_vbm;	/* Pointer to volume bitmap */
  u_int		fs_pbsize;      /* Physical block size */
  u_int		fs_ptnbase;	/* Beginning of partition (physical block #) */
  u_int		fs_ptnsize;	/* Size of partition (physical blocks) */
  u_int		fs_albsize;	/* Size of allocation blocks (BB's) */
  hnode_t 	*fs_hnpool;	/* Pool of all hnodes */
  u_int		fs_hnode_count;	/* Number of hnodes allocated */
  frec_t	*fs_frpool;	/* Pool of all frecs */
  u_int		fs_frec_count;	/* Number of frecs allocated */
  fidc_t	*fs_fcpool;	/* Pool of fidc nodes */
  u_int		fs_fidc_count;  /* Number of fidc nodes */
  erec_t	*fs_erpool;	/* Pool of all erecs */
  u_int		fs_erec_count;	/* Number of erecs allocated */
  ar_t		*fs_arpool;	/* Pool of all arecs */
  arp_t		*fs_arppool;	/* Pool of all arp's */
  u_int		fs_arec_count;	/* Number of all arecs */
  
  char		*fs_buf;	/* Pointer to alb buffer */
  u_int		fs_alb;		/* Alb number of data in alb buffer */
  hnode_t	*fs_prealloced;	/* Fork that has preallocated space */
};


/* Definitions for hfs.flags */

#define FS_RDONLY 	0x0001
#define FS_CORRUPTED    0x0002
#define FS_MDB_ATR      0x0004
#define FS_MDB_ALC	0x0008
#define FS_VBM_ALC      0x0010
#define FS_CDROM        0x0020
#define FS_FLOPPY	0x0040
#define FS_STALE        0x0080
#define FS_MDB_SYNC_ESCAPE  0x0100  /* See fs_sync/fs_mdb_push/io_pwrite */
#define FS_NFS_MAP      0x0200      /* See fhtohp and fr_get */

#define FS_MDB_DIRTY	(FS_MDB_ATR | FS_MDB_ALC)
#define FS_VBM_DIRTY	(FS_VBM_ALC)
#define FS_DIRTY        (FS_MDB_DIRTY|FS_VBM_DIRTY)

#define FS_SET(f,m)   	(f->fs_flags |= FS_##m)
#define FS_CLR(f,m)	(f->fs_flags &= ~FS_##m)
#define FS_ISSET(f,m)   (f->fs_flags & FS_##m) 	/* T if *any* mask bits set */
#define FS_ISCLR(f,m)   ((f->fs_flags & FS_##m)==0)


/* flags for fs_sync and its minions */

#define SYNC 0x01		/* Set by fs_sync before calling minions */
#define ATR  0x02		/* Syncing attributes */
#define ALC  0x04		/* Syncing allocation */
#define MDB  0x08		/* Sync the MDB */
#define ALL  (SYNC|ATR|ALC|MDB) /* Sync everything */


/* Well known global fs_ routines */

int fs_add(char *pathname, u_int flags, u_int partition, struct hfs **hfs);
struct hfs * fs_get(fsid_t fs);
void fs_destroy(hfs_t *hfs);
void fs_remove(hfs_t *hfs);
void fs_remove_all();
int fs_mount(hfs_t *hfs, u_int partition);
int fs_sync(hfs_t *hfs, u_int flags);
void fs_corrupted(hfs_t *hfs, char *str);
void fs_sync_all();
int fs_cancel_prealloc(hfs_t *hfs);

#endif








