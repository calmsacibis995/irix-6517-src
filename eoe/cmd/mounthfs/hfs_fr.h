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

#ifndef __HFS_FR_H__
#define __HFS_FR_H__

#ident "$Revision: 1.3 $"

#define FRCACHEBINS 64		/* # cache bins */


#define KEY_CNAME_LEN 31

/*************
 * Catalog key.
 */
typedef struct catk{
   khdr_t      k_hdr;		/* Standard key header */ 
  /*
   * This structure is an "image" of the canonical catalog key.
   *
   * NOTE: This image must immediately follow the standard key header.
   *
   * In-core and for index nodes, CNAME is padded to 32 and KeyLen reflects
   * that padding.  For leaf nodes, CName is trimmed to the actual name and
   * k_KeyLen is adjusted approriately.  Note that if k_KeyLen is even the
   * total key has an odd byte length. However, the key is padded at the
   * end to produce an on-disk key with an even number of bytes.  The
   * k_keylen field doesn't necessarily count this pad byte. 
   */
  struct{
    u_char      k_KeyLen;			/* Length of key - 1 */
    u_char	k_Resrv1;
    u_char	k_ParID[4];			/* Parent id */
    u_char	k_CName[KEY_CNAME_LEN+1];	/* Name [0]==strlen(name) */
  } k_key;
} catk_t;

#define CAT_KEYLEN  (sizeof( ((catk_t*)0)->k_key ))
#define CAT_PARENT(k) (((k)->k_key.k_ParID[0]<<24)|\
                       ((k)->k_key.k_ParID[1]<<16)|\
                       ((k)->k_key.k_ParID[2]<<8)|\
                       ((k)->k_key.k_ParID[3]))


/**************************
 * Primitive geometry types.
 */
typedef struct point{
  u_short v;
  u_short h;
} point_t;

typedef struct rect{
  u_short	top;
  u_short	left;
  u_short	bottom;
  u_short	right;
} rect_t;


/*********************************************
 * Finder info for folder (directory) records.
 */
typedef struct dinfo{
  rect_t	Rect;		/* Rectange for folder window */
  u_short	isAlias:1,
                isInvisible:1,	/* Hidden */
		unused:1,
		nameLocked:1,	/* Cannot be renamed */
		isStationary:1,
		hasCustomIcon:1,
		Reserved:1,
		hasBeenInited:1,
		unused1:7,
		isOnDesk:1;
  point_t	Location;	/* Folder location on desktop */
  u_short	View;		/* Folder's view */
} dinfo_t;

typedef struct dxinfo{
  point_t	Scroll;		/* Scroll position of folder window */
  fid_t		OpenChain;	/* Directory ID chain of open folders */
  char		Script;		/* Script flag and code */
  char		XFlags;		/* Reserved */
  u_short	Comment;	/* Comment id. */
  u_int		PutAway;	/* ID of folder containing folder*/
} dxinfo_t;


/******************************
 * Finder info for file records.
 */
typedef struct finfo{
  u_char	Type[4];	/* File type string */
  u_char	Creator[4];	/* File creator string*/
  u_short	isAlias:1,
		isInvisible:1,	/* Hidden */
		hasBundle:1,
		nameLocked:1,	/* Cannot be renamed */
		isStationery:1,
		hasCustomIcon:1,
		Reserved:1,
		hasBeenInited:1,
		hasNoINITS:1,
		isShared:1,
		requiresSwitchLaunch:1,
		colorReserved:1,
		color:3,
		isOnDesk:1;
  point_t	Location;	/* Location of file in window */
  u_short	Fldr;		/* Files window */
} finfo_t;

typedef struct fxinfo{
  u_short	IconId;		/* ID of icon associated with this file */
  u_char	Unused[6];
  char		Script;		/* Script flag and code */
  char		XFlags;		/* Reserved */
  u_short	Comment;	/* comment id. */
  u_int		PutAway;	/* ID of folder containing file */
} fxinfo_t;


/*********************
 * Catalog data record - used for files, directories and threads
 */
typedef struct catdatarec{
  u_char	c_Type;		/* Record type */
  u_char	c_Resrv2;
  union{
    struct{			/* Directory record */
      u_short Flags;		/* Directory Flags */
      u_short Val;		/* Directory valence (number of entries) */
      u_int   DirID;		/* Directory ID. */
      u_int   CrDat;		/* Creation date. */
      u_int   MdDat;		/* Modification date. */
      u_int   BkDat;		/* Backup date. */
      dinfo_t DInfo;		/* Finder info. */
      dxinfo_t DXinfo;		/* More finder info. */
      u_int   Resrv[4];		/* Reserved. */
    } c_Dir;

    struct{			/* File record */
      u_char  Flags;		/* File flags. */
      u_char  Typ;		/* File type. */
      finfo_t  UsrWds;		/* Finder info. */
      u_int   FlNum;		/* File number. */
      u_short StBlk;		/* Starting allocation block of data. */
      u_int   LgLen;		/* Logical EOF of data fork. */
      u_int   PyLen;		/* Physical EOF of data fork. */
      u_short RStBlk;		/* Starting allocation block of resource. */
      u_int   RLgLen;		/* Logical EOF of resource fork. */
      u_int   RPyLen;		/* Physical EOF of resource fork. */
      u_int   CrDat;		/* Creation date. */
      u_int   MdDat;		/* Modification date. */
      u_int   BkDat;		/* Backup date. */
      fxinfo_t FndrInfo;	/* More finder info. */
      u_short ClpSize;		/* Clump size. */
      ExtDescRec_t ExtRec;	/* First three data fork extent record. */
      ExtDescRec_t RExtRec;	/* First three resource fork extent record. */
      u_int    Resrv;		/* Reserved */
      } c_Fil;
       
    struct{			/* Directory thread record */
      u_int   Resrv[2];		/* Reserved. */
      u_int   ParID;		/* Parent ID for this directory. */
      char    CName[32];	/* Name of this directory.  [0]== length. */
    } c_Thd;

    struct{			/* File thread record */
      u_int   Resrv[2];		/* Reserved. */
      u_int   ParID;		/* Parent ID for this directory. */
      char    CName[32];	/* Name of this directory.  [0]== length. */
    } c_Fthd;
  } c;
} cdr_t;


enum {CDR_DIRREC=1,		/* Directory record */
	CDR_FILREC=2,		/* File record */
	CDR_THDREC=3,		/* Directory hread record */
	CDR_FTHDREC=4};		/* File thread record */

#define FR_FILE_LOCK 01		/* File flag bits */


/* Offset from hfs time base to Unix time base */

#define FR_TIME_OFFSET 2082844800

#define IS_FILE_OR_DIR(rd)  (*(u_char*)((rd)->data) <= CDR_FILREC)
#define IS_FILE(rd)  (*(u_char*)((rd)->data)==CDR_FILREC)
#define IS_DIR_THD(rd)  (*(u_char*)((rd)->data) == CDR_THDREC)
#define MATCHES(fd)  ((fd->f_cdr.c_Type == CDR_DIRREC)\
		      ? (fd->f_fid == fd->f_cdr.c.c_Dir.DirID)\
		      : (fd->f_fid == fd->f_cdr.c.c_Fil.FlNum))


/*************
 * File record
 */
struct frec{
  hlnode_t	f_hl;		/* Hashlist overhead */  /** MUST BE FIRST **/
  hfs_t		*f_hfs;		/* Ptr to file system data. */
  fid_t		f_fid;		/* File id. */
  fid_t		f_pfid;		/* Parent file id. */
  u_int		f_refcnt;	/* frec reference count */
  u_int		f_flags;	/* frec flags */
  catk_t	f_key;		/* Key for this frec */
  cdr_t		f_cdr;		/* Catalog data record. */
};


#define HIDDEN(fp) ((fp->f_cdr.c_Type==CDR_DIRREC)\
		    ? fp->f_cdr.c.c_Dir.DInfo.isInvisible \
		    :fp->f_cdr.c.c_Fil.UsrWds.isInvisible)

#define FR_ATR	0x0001
#define FR_ALC  0x0002
#define FR_CREATE 0x004		/* See hfs_create */
#define FR_ACTIVE 0x8000

#define FR_DIRTY (FR_ATR | FR_ALC)

#define FR_SET(f,m)   	(f->f_flags |= FR_##m)
#define FR_CLR(f,m)	(f->f_flags &= ~FR_##m)
#define FR_ISSET(f,m)   (f->f_flags & FR_##m) 	/* T if *any* mask bits set */
#define FR_ISCLR(f,m)   ((f->f_flags & FR_##m)==0)



/*****************
 * FID cache block
 */
typedef struct fidc{
  hlnode_t    fc_hl;		/* Hashlist overhead */ /** MUST BE FIRST **/
  u_int	      fc_fid;		/* Fid number */
  u_int	      fc_pfid;		/* Parent fid */
} fidc_t;

#define FID_CACHE_SIZE 1000


/* Access macros */

#define _f_cdr(f)  (f->f_cdr)
#define _f_file(f) (_f_cdr(f).c.c_Fil)
#define _f_dir(f)  (_f_cdr(f).c.c_Dir)


/* Well known global fr_ routines */

void fr_activate(frec_t *fp);
void fr_cons_key(fid_t fid, catk_t *key, char *name);
void fr_cons_thd(frec_t *fp, char *thd, rdesc_t *);
int fr_construct(hfs_t *hfs, catk_t *key, rdesc_t *rd, frec_t **frpp);
int fr_destroy(hfs_t *hfs);
frec_t *fr_incore(hfs_t *hfs, fid_t fid);
void fr_free(frec_t *frp);
int fr_get(hfs_t *hfs, fid_t fid, frec_t **frpp);
void fr_new(hfs_t *hfs, frec_t **frpp);
int fr_init(hfs_t *hfs, u_int initial, u_int cachebins);
int fr_put(frec_t *frp);
int fr_read(frec_t *frp);
int fr_reclaim(frec_t *frp);
int fr_sync(hfs_t *hfs, u_int flags);
int fr_update(frec_t *frp, u_int flags);

#endif





