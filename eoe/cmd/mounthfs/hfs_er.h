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

#ifndef __HFS_ER_H__
#define __HFS_ER_H__

#ident "$Revision: 1.2 $"

#define ERINITIAL    16		/* Initial allocation of erec's */
#define ERCACHEBINS  16		/* Cache bins */
#define ERMAXEXTRECS 32		/* Maximum number of extent records */

/* Extent descriptor */

typedef struct ExtDescr{
  u_short ed_StABN;		/* First allocation block */
  u_short ed_NumABlks;		/* Number of allocation blocks */
} ExtDescr_t;

/* Extent descriptor "record" */

typedef struct ExtDescRec{
  ExtDescr_t exts[3];
} ExtDescRec_t;


/*
 * The erec contains an array of albdata structures, an array the same
 * size as the array of extent descriptor records.  When the extent
 * descriptor records are read in, the starting alocation block index
 * for each record is recorded in the ondisk and incore fields.  When
 * the allocation is modified in core, the incore field is adjusted to
 * reflect the change but the ondisk field is not modified until the
 * extents are written to disk.
 */
typedef struct albdata{
  u_short a_ondisk;
  u_short a_incore;
} albdata_t;


/* Extent overflow file key */

typedef struct eofk{
  khdr_t	k_hdr;		/* Standard key header */
  /*
   * This structure is an "image" of the extent overflow file key.
   *
   * NOTE: This image must immediately follow the standard key header.
   */
  struct{
    u_char	k_KeyLen;	/* Length of key -1 */
    u_char	k_FkType;	/* Fork type */
    u_char	k_FNum[4];	/* File number */
    u_char	k_FABN[2];	/* Starting file allocation block number */
  } k_key;
} eofk_t;

#define EOFK_RFORK 0xff		/* Fork type for resource fork */
#define EOFK_DFORK 0x00		/* Fork type for data fork */

#define EOFK_FNUM(k) (((k)->k_key.k_FNum[0]<<24) |\
		     ((k)->k_key.k_FNum[1]<<16) |\
		     ((k)->k_key.k_FNum[2]<< 8) |\
		     ((k)->k_key.k_FNum[3]))

#define EOFK_FABN(k) (((k)->k_key.k_FABN[0]<<8) |\
		      ((k)->k_key.k_FABN[1]))


/* Extent overflow file record */

typedef struct erec{
  hlnode_t    e_hl;		/* Hashlist overhead */  /** MUST BE FIRST **/
  u_int	      e_fid;		/* File id */
  u_char      e_fork;		/* Fork type of file */
  hfs_t       *e_hfs;		/* Pointer to file system */
  u_int	      e_refcnt;		/* Number of references */
  u_int       e_flags;		/* Erec flags */
  u_int	      e_count;		/* Number of extent records */
  u_int	      e_mask;		/* Mask of "dirty" extent records */
  ExtDescRec_t e_erecs[ERMAXEXTRECS];	/* Extent records. */
  albdata_t   e_albdat[ERMAXEXTRECS];   /* alb addresses of each extent */
} erec_t;

#define ERKEY(fid,fork) ((fid<<1)|(fork&1))


#define ER_ALC		0x0001
#define ER_ACTIVE	0x8000

#define ER_DIRTY	ER_ALC

#define ER_SET(e,m)   	(e->e_flags |= ER_##m)
#define ER_CLR(e,m)	(e->e_flags &= ~ER_##m)
#define ER_ISSET(e,m)   (e->e_flags & ER_##m) 	/* T if *any* mask bits set */
#define ER_ISCLR(e,m)   ((e->e_flags & ER_##m)==0)
#define ERM_SET(e,m)    (e->e_mask |= 1<<(m))
#define ERM_CLR(e,m)    (e->e_mask &= ~(1<<(m)))


/* Well known global er_ routines */

int  er_destroy(hfs_t *hfs);
int  er_expand(hfs_t *, ExtDescRec_t *, erec_t *, frec_t *, int albs, u_int clmpsz, u_int    *sizep);
void er_free(erec_t *ep);
int  er_get(hfs_t *hfs, u_int fid, u_char fork, u_short albn, erec_t **erpp);
int  er_init(hfs_t *hfs, int initial, int cachebins);
int  er_put(erec_t *erp);
int  er_read(erec_t *erp, u_short albn);
int  er_reclaim(erec_t *erp);
int  er_shrink(hfs_t *, ExtDescRec_t *, erec_t *, frec_t *, int albs, u_int clmpsz, u_int *sizep);
int  er_sync(hfs_t *hfs, u_int flags);
int  er_toalbn(u_int n, ExtDescRec_t *, erec_t *, u_int *albn,u_int *albns);
int  er_update(erec_t *erp, u_int flags);

#endif



