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

#ifndef __HFS_AR_H__
#define __HFS_AR_H__

#ident "$Revision: 1.4 $"


typedef struct arec ar_t;
typedef struct afs afs_t;

#include "ancillary.h"

/*
 * .HSancillary record.  Contains pointers to all afs records for a
 *                       a directory hnode.
 */
#define ARPCACHEBINS 16
#define AR_SLOTS 32


/*
 * Slot record for the ancillary record.
 *
 * Note:  The a_fid field is used for processing of ancillary record
 *        reads but not for writes.  Likewise, the a_afs field is used
 *        for processing of ancillary record writes but not for reads.
 */
typedef struct arslot{
  fid_t	a_fid;			/* Fid of entry.  Zero if empty. */
  afs_t *a_afs;			/* Ptr to actual ancillary file slot rec's. */
} asl_t;

/*
 * Mount_hfs creates an ancillary record when an ancillary file is read.
 * Once created, it is never destroyed unless the corresponding directory
 * is removed.  When created, the directory is scanned and the contents
 * of the directory are entered into arec.a_slots.  If there are more
 * entries than there are slots, arec's are chained together.
 */
struct arec{
  hlnode_t a_hl;		/* Hash list overhead */ /** MUST BE FIRST **/
  hfs_t    *a_hfs;		/* File system link */
  hno_t    a_hno;		/* Directory hno */
  int	   a_refcnt;		/* Reference count */
  time_t   a_cdate;		/* Creation date */
  time_t   a_mdate;		/* Modification date */
  ar_t     *a_next;		/* arec chain */
  asl_t    a_slots[AR_SLOTS];	/* slot */
};


/*
 * Arec pool	- used to manage pool of arecs.
 */
#define ARP_COUNT 32

typedef struct arec_pool arp_t;
struct arec_pool{
  arp_t	   *ap_next;
  ar_t     ap_pool[ARP_COUNT];
};


/*
 * Slot pointer.  Designates a slot in an arec.
 */
typedef struct arslotp{
  ar_t  *ap;			/* Arec pointer. */
  ar_t  *rap;			/* Root arec pointer. */
  short idx;			/* Slot number in *ap */
  short aidx;			/* Arec # in chain. */
} sp_t;


/*
 * Ancillary file slot record.
 */
struct afs{
  sp_t    as_sp;		/* Slot pointer. */
  u_short as_modified;		/* Afs is modified */
  short   as_partial;		/* Block is partially modified. */
  ai_t    as_ai;		/* Ancillary file record */
};




/* Well known global ar_ routines */

ar_t *ar_incore(hfs_t *hfs, hno_t dhno);
int  ar_get(hfs_t *, hno_t, ar_t **);
int  ar_read(ar_t *);
int  ar_put(ar_t *);
void ar_new(hfs_t *, ar_t **);
void ar_free(ar_t *);
int  ar_init(hfs_t *, u_int cachebins);
int  ar_destroy(hfs_t *);
int  ar_remove(hfs_t *, hno_t, fid_t);
int  ar_add(hfs_t *, hno_t, fid_t);

/* Well known global ai_ routines */

int ai_read(hnode_t *, u_int offset, u_int count, char *data, u_int *nio);
int ai_write(hnode_t *, u_int offset, u_int count, char *data);

#endif



