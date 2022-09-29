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

#ident "$Revision: 1.3 $"

#include "hfs.h"

/********
 * isfree
 */
static int isfree(blist_t *n){
  return (n->f==n && n->b==n);
}


/**********
 * coherent
 */
static int coherent(blist_t *start){
#ifdef COHERENCE
  blist_t *b;
  int     i;

  for (i=0,b=start->f;i<3000 && b!=start;i++,b=b->f);
  if (b!=start)
    return 0;
  for (i=0,b=start->b;i<3000 && b!=start;i++,b=b->b);
  if (b!=start)
    return 0;
#endif
  return 1;
}


/*********
 * hl_init	Initialize a hashlist.
 */
void hl_init(hlroot_t *rp, int size, int (*hf)(hlnode_t*)){
  int i;

  NULLHLNODE(rp->freelist);
  rp->count = size;
  rp->hf = hf;
  rp->hashlist = (blist_t*)safe_malloc(sizeof(blist_t)*size);
  for (i=0;i<size;i++)
    NULLBLIST(rp->hashlist[i]);
}


/************
 * hl_destroy	Destroy a hashlist.
 */
void hl_destroy(hlroot_t *rp){
  if (rp->hashlist)
    free(rp->hashlist);
}


/***********
 * hl_insert	Insert a node at the front of the available list on this bin.
 */
void hl_insert(register hlroot_t *rp, register hlnode_t *np){
  register blist_t *hn = &rp->hashlist[(rp->hf)(np) % rp->count];

  assert(coherent(hn));
  assert(coherent(&np->avail));

  np->avail.f = hn->f;
  np->avail.b = hn;
  np->avail.f->b = &np->avail;
  hn->f = &np->avail;

  assert(coherent(hn));
  assert(coherent(&np->avail));
  
  assert(hn==hn->f->b);
  assert(hn==hn->b->f);
}


/***********
 * hl_remove	Remove a node from the available list.
 */
void hl_remove(register hlnode_t *np){
  blist_t *f = np->avail.f;
  blist_t *b = np->avail.b;

  assert(f->b==&np->avail);
  assert(b->f==&np->avail);
  assert(coherent(&np->avail));

  f->b = b;
  b->f = f;

  assert(coherent(f));
  assert(coherent(b));

  NULLBLIST(np->avail);
}


/**********
 * *hl_find	Find an entry with hashkey k.
 *
 * NOTE: This code works because the hlnode is the first element of an entry
 *       and because the avail blist is the first element of the hlnode.
 */
hlnode_t *hl_find(hlroot_t *rp, int k, void *p, int (*cmp)(hlnode_t*,void*)){
  register blist_t *hn = &rp->hashlist[k % rp->count];
  register hlnode_t *np = (hlnode_t *)hn->f;

  while (hn != (blist_t*)np){
    if ((cmp)(np,p))
      return np;
    np = (hlnode_t*)np->avail.f;
  }
  return NULL;
}


/****************
 * hl_insert_free
 */
void hl_insert_free(hlroot_t *rp, hlnode_t *np){
  blist_t *hn = &rp->freelist.free;

  assert(isfree(&np->free));
  assert(coherent(hn));

  np->free.f = hn->f;
  np->free.b = hn;
  np->free.f->b = &np->free;
  hn->f = &np->free;

  assert(coherent(hn));
}


/****************
 * hl_remove_free
 */
void hl_remove_free(hlnode_t *np){
  blist_t *f=np->free.f;
  blist_t *b=np->free.b;

  assert(coherent(f));

  f->b = b;
  b->f = f;

  assert(coherent(f));

  NULLBLIST(np->free);
}


/*************
 * hl_get_free
 *
 * NOTE: For the free list, our chain is offset from the base of the
 *	 hlnode so we must compensate for the offset.
 */
hlnode_t *hl_get_free(hlroot_t *rp){
  hlnode_t *np = (hlnode_t*)rp->freelist.free.b;
  if (np == (hlnode_t*)&rp->freelist.free)
    return NULL;

  assert(coherent(&rp->freelist.free));

  np = (hlnode_t*)(((char*)np)-HLFBOFFSET);
  hl_remove_free(np);

  assert(coherent(&rp->freelist.free));
  return np;
}





