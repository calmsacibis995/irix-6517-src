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

#ifndef __HFS_HN_H__
#define __HFS_HN_H__

#ident "$Revision: 1.3 $"

#define HNINITIAL   256		/* Initial allocation of hnodes */
#define HNCACHEBINS  64		/* # of cache bins in fs_hnode_list */


/* Hnode */

struct hnode{
  hlnode_t	h_hl;		/* Hashlist overhead*/   /** MUST BE FIRST */
  hfs_t		*h_hfs;		/* Parent file system */
  hno_t		h_hno;		/* Hnode number */
  hno_t		h_phno;	        /* Parent hnode */
  u_long	h_flags;	/* Flags */
  u_short	h_ftype;	/* File type */
  mode_t	h_fmode;	/* File mode */
  uid_t		h_uid;		/* Owner */
  gid_t		h_gid;		/* Group */
  u_short	h_refcnt;	/* Hnode reference count */
  struct frec	*h_frec;	/* frec associated with this node */
  struct erec	*h_erec;	/* erec associated with this node */
};

#define HN_ACTIVE 0x8000
#define HN_DIRTY       0

#define HN_SET(h,m)	(h->h_flags |= HN_##m)
#define HN_CLR(h,m)	(h->h_flags &= ~HN_##m)
#define HN_ISSET(h,m)	(h->h_flags & FR_##m) /* T if *any* mask bits set */
#define HN_ISCLR(h,m)	((h->h_flags & FR_##m)==0)

/* File mode bits */

#define HN_DIR_RW_ACCESS       0777
#define HN_DIR_RO_ACCESS       0555
#define HN_DIR_EX_ACCESS       0111
#define HN_FILE_RW_ACCESS      0666
#define HN_FILE_RO_ACCESS      0444

/* Access macros */

#define _h_cdr(h)  (h->h_frec->f_cdr)
#define _h_file(h) (_h_cdr(h).c.c_Fil)
#define _h_dir(h)  (_h_cdr(h).c.c_Dir)
#define _h_mdb(h)  (h->h_hfs->fs_mdb)
#define _h_cbt(h)  (h->h_hfs->fs_cbt)
#define _h_ebt(h)  (h->h_hfs->fs_ebt)


/* Well known global hn_ routines */

int hn_init(hfs_t *hfs, u_int initial, u_int cachebins);
int hn_destroy(hfs_t *hfs);
int hn_get(hfs_t *hfs, hno_t hno, hnode_t **hpp);
int hn_put(hnode_t *hp);
void hn_free(hnode_t *hp);
int hn_reclaim(hnode_t *hp);
int hn_update(hnode_t *hp, u_int flags);
int hn_read(hnode_t *hp);
int hn_sync(hfs_t *hfs, u_int flags);
int hn_toalbn(hnode_t *hp, u_int albidx, u_int *albn, u_int *albncnt);
int hn_adj_size(hnode_t *hp, u_int newsize, u_int *oldsize);

#endif







