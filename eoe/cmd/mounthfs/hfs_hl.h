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

#ifndef __HFS_HL__H__
#define __HFS_HL__H__

#ident "$Revision: 1.2 $"

#include <stddef.h>

/*
 * Hashlists
 *
 * The hashlist structures and routines here formalize the unix
 * double threaded storage management protocol.
 */ 

/* Two-Way List */

typedef struct blist{
  struct blist *f, *b;		/* Forward and backward links */
} blist_t;


/* Node */

typedef struct hlnode{
  blist_t  avail, free;		/* Available and free chains */
} hlnode_t;

#define HLFBOFFSET (offsetof(hlnode_t,free))
#define NULLBLIST(l) l.f = l.b = &l
#define NULLHLNODE(n) NULLBLIST(n.avail); NULLBLIST(n.free)


/* Root structure */

typedef struct hlroot{
  hlnode_t freelist;		/* Freelist */
  blist_t *hashlist;		/* Hash table. */
  int (*hf)(hlnode_t *);	/* Hash function */
  u_int count;			/* Number of hashbins */
} hlroot_t;


/* Well known global hl_ routines */

void hl_init(hlroot_t *rp, int size, int (*hf)(hlnode_t*));
void hl_destroy(hlroot_t *rp);
void hl_insert(hlroot_t *rp, hlnode_t *np);
void hl_remove(hlnode_t *np);
hlnode_t *hl_find(hlroot_t *rp, int hk, void *p, int (*cmp)(hlnode_t*,void*));
void hl_insert_free(hlroot_t *rp, hlnode_t *np);
void hl_remove_free(hlnode_t *np);
hlnode_t *hl_get_free(hlroot_t *rp);

#endif
