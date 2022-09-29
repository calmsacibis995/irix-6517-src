/*
** Copyright 1998, Silicon Graphics, Inc.
** All Rights Reserved.
**
** This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
** the contents of this file may not be disclosed to third parties, copied or
** duplicated in any form, in whole or in part, without the prior written
** permission of Silicon Graphics, Inc.
**
** RESTRICTED RIGHTS LEGEND:
** Use, duplication or disclosure by the Government is subject to restrictions
** as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
** and Computer Software clause at DFARS 252.227-7013, and/or in similar or
** successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
** rights reserved under the Copyright Laws of the United States.
**
*/

#include "sys/types.h"
#include "odsy_internals.h"

void OdsyInitCSet(odsy_chunked_set *cset,int csize)
{
	cset->head = 0;
	cset->nr_chunks = cset->nr_elem = 0;
	cset->c_size = csize;
}

#if !defined(_STANDALONE)

void OdsyDestroyCSet(odsy_chunked_set *cset,odsy_cset_iterator_fn destroy_elem_fn)
{
	int c_node_nr;
	odsy_chunk_node *c_node;
	int c_size = cset->c_size;
	ASSERT(c_size > 0); /*not already destroyed*/

	/* over all of the chunk nodes */
	for (c_node_nr=0,c_node=cset->head; c_node_nr < cset->nr_chunks; c_node_nr++){
		int c_node_elem_nr;
		odsy_chunk_node *zap_node;

		/* over all of the elements in the chunk */
		if ( c_node->chunk ) {
			for (c_node_elem_nr=0; c_node_elem_nr < (1<<c_size); c_node_elem_nr++) {
				void *cset_elem;
				if(cset_elem = c_node->chunk[c_node_elem_nr]){
					if (destroy_elem_fn) (*destroy_elem_fn)(cset_elem);
				}
			}
			kmem_free(c_node->chunk,sizeof(void*)*(1<<c_size));
		}
		c_node= ((zap_node=c_node)->s);
		kmem_free(zap_node,sizeof(odsy_chunk_node));
	}
	
	OdsyInitCSet(cset,0);
}


/* 
** Only front-back iterator so far.  The cursor becomes invalid after 
** any other CSet interface other than "Next" is called.
*/
void OdsyInitCSetCursor(odsy_chunked_set *cset)
{
	cset->cursor_node = cset->head;
	cset->cursor_elem_nr = 0;
	cset->cursor_flipped = 0;
}


/* only front-back iterator so far */
void *OdsyNextCSetElem(odsy_chunked_set *cset)
{

	/* find the first non-free element */
	void *found = 0;
	ASSERT(cset->c_size > 0); /*not already destroyed*/

	if (!cset->cursor_node || cset->cursor_flipped ) {
		cset->cursor_flipped = 0;
		return 0;
	}

	cset->cursor_elem_nr %= (1<<cset->c_size);

	do {
#ifdef ODSY_DEBUG_CSET_ADT
		cmn_err(CE_DEBUG,"odsy adt: NextCSetElem loop cursor_node:0x%x cursor_elem_nr:%d nr_elem:%d\n",
			cset->cursor_node,cset->cursor_elem_nr,cset->cursor_node->nr_elem);
#endif
		if (cset->cursor_node->nr_elem == 0) {
			cset->cursor_node = cset->cursor_node->s;
			continue;
		}

		for (;cset->cursor_elem_nr < (1<<cset->c_size); cset->cursor_elem_nr++ ) {
			if (found = cset->cursor_node->chunk[cset->cursor_elem_nr]) {
				cset->cursor_elem_nr++;
				// in case we found the last element, make sure next time
				// around our node is the right one.
				if (cset->cursor_elem_nr == (1<<cset->c_size)) {
					cset->cursor_elem_nr = 0;
					cset->cursor_node = cset->cursor_node->s;
					// Did we reach the end on a 'found' condition?
					if ( cset->cursor_node == cset->head ) {
						cset->cursor_flipped = 1;
					}
				}
#ifdef ODSY_DEBUG_CSET_ADT
				cmn_err(CE_DEBUG,"odsy adt: NextCSetElem found:0x%x\n",found);
#endif				
				return found;
			}
		}		

		cset->cursor_node = cset->cursor_node->s;

	} while (cset->cursor_node != cset->head);

#ifdef ODSY_DEBUG_CSET_ADT
	cmn_err(CE_DEBUG,"odsy adt: NextCSetElem found:0x%x\n",found);
#endif				
	return found;

	
}


/*
** for the non-zero elements, call the given fn and if it returns zero, consider
** it a match and return the element.  (if you need the id, well, stash it inside).
*/
void* OdsyFindMatchingCSetElem(odsy_chunked_set *cset,void *cmp_with, odsy_cset_match_fn match_fn)
{

	int c_node_nr;
	odsy_chunk_node *c_node;
	int c_size = cset->c_size;
	ASSERT(cset->c_size > 0); /*not already destroyed*/

	/* over all of the chunk nodes */
	for (c_node_nr=0,c_node=cset->head; c_node_nr < cset->nr_chunks; c_node_nr++,c_node=c_node->s){
		int c_node_elem_nr;
		odsy_chunk_node *zap_node;

		/* over all of the elements in the chunk */
		if ( c_node->nr_elem ) {
			for (c_node_elem_nr=0; c_node_elem_nr < (1<<c_size); c_node_elem_nr++) {
				void *cset_elem;
				if(cset_elem = c_node->chunk[c_node_elem_nr]){
					if ((*match_fn)(cset_elem,cmp_with)==0){
					return cset_elem;
					}
				}
			}
		}
	}

	return 0; /*didn't find it */
}




static odsy_chunk_node* NewChunkNode(int csize)
{
	odsy_chunk_node* cnode;

	/*XXX cache me */
	if ( ! (cnode = (odsy_chunk_node*)kmem_alloc(sizeof(odsy_chunk_node),KM_SLEEP)) ) {
		return 0;
	}

	bzero((caddr_t)cnode, sizeof(odsy_chunk_node));

	return cnode;
		
}

/* 
** allow the user to allocate a specific id.  
** if the chunk the id fits into hasn't been created, do that first.
** this means also allocating all chunks in between if they aren't already.
** the interface assumes the user wont allocknown an id that is already
** in use.  otherwise the nr_elem accounting mechanisms get messed up.
*/
int OdsyAllocSpecificCSetId(odsy_chunked_set *cset,int elemnr,void *elemp)
{
	odsy_chunk_node *c_node;
	int c_size = cset->c_size;
	int c_node_nr      = elemnr >> cset->c_size;
	int c_node_elem_nr = elemnr & ((1<<cset->c_size)-1);
	int i;
	ASSERT(cset->c_size > 0); /*not already destroyed*/

	
	if (!(c_node_nr < cset->nr_chunks)) {
		/* 
		** we have to make all the nodes in between the 
		** last one and the one this elem goes into.
		** might also be making the first one ever.
		*/
		int new_node_nr;
		int nr_nodes_to_make = (c_node_nr+1) - cset->nr_chunks ;
		

		for (new_node_nr=0; new_node_nr<nr_nodes_to_make; new_node_nr++){
			odsy_chunk_node *new_c_node;

			if(! (new_c_node = NewChunkNode(cset->c_size))) return -1;

			if(!cset->head){
				/* the first node ever */
				cset->head = new_c_node->p = new_c_node->s = new_c_node;
			}
			else {
				/* not the first node ever, add to the tail */
				new_c_node->s = cset->head;
				new_c_node->p = cset->head->p;
				cset->head->p = new_c_node;
				new_c_node->p->s = new_c_node;
			}

			new_c_node->nr_elem = 0;
			cset->nr_chunks++;
		}

	}


	/* (now) all the necessary c_nodes exist */	
	for (i=0,c_node=cset->head;i<c_node_nr;i++,c_node=c_node->s) ;
	
	if ( c_node->nr_elem == 0 ) {
		if ( ! (c_node->chunk = (void **)kmem_alloc(sizeof(void*)*(1<<c_size),KM_SLEEP)) ) {
			return -1;
		}
		bzero((caddr_t)c_node->chunk,sizeof(void*)*(1<<c_size));

	}

	c_node->chunk[c_node_elem_nr] = elemp;

	c_node->nr_elem++;
	cset->nr_elem++;

	return elemnr;

}

int OdsyAllocCSetId(odsy_chunked_set *cset,void *elemp)
{
	int c_node_nr;
	odsy_chunk_node *c_node;
	int c_size = cset->c_size;
	int c_elem_nr = 0;
	ASSERT(cset->c_size > 0); /*not already destroyed*/

	

	/* find the first chunk node with a free element */

	for (c_node_nr=0,c_node=cset->head;  c_node_nr < cset->nr_chunks;  c_node_nr++,c_node=c_node->s){
		int c_node_elem_nr;

		if(c_node->nr_elem == (1<<c_size)) continue;

		c_elem_nr = c_node_nr<<c_size;


		if (c_node->nr_elem == 0 ) {
			if ( !(c_node->chunk = (void**)kmem_alloc(sizeof(void*)*(1<<c_size),KM_SLEEP)) ) {
				return -1;
			}
			bzero((caddr_t)c_node->chunk,sizeof(void*)*(1<<c_size));
		}

		/* over all of the elements in the chunk */
		for (c_node_elem_nr=0; c_node_elem_nr < (1<<c_size); c_node_elem_nr++) {
			if( c_node->chunk[c_node_elem_nr] == 0 ) {
				c_node->chunk[c_node_elem_nr]=elemp;
				c_node->nr_elem++;
				cset->nr_elem++;
				return c_elem_nr;
			}
			c_elem_nr++;
		}
	}	


	/* there was no chunk node with a free element,  add a new chunk...*/

	if ( !(c_node = NewChunkNode(c_size))) return -1;
	if ( !(c_node->chunk = (void**)kmem_alloc(sizeof(void*)*(1<<c_size),KM_SLEEP)) ) {
		kmem_free(c_node,sizeof(odsy_chunk_node));
		return -1;
	}
	bzero((caddr_t)c_node->chunk,sizeof(void*)*(1<<c_size));
	c_node->nr_elem=1;
	c_elem_nr = cset->nr_elem++;
	cset->nr_chunks++;

	if(!cset->head){
		cset->head = c_node->p = c_node->s = c_node;
	}
	else {
		c_node->s = cset->head;
		c_node->p = cset->head->p;
		cset->head->p = c_node;
		c_node->p->s = c_node;
	}

	c_node->chunk[0]=elemp;
	return c_elem_nr;

}


/*
** XXX should shrink itself if possible...
*/
void OdsyDeallocCSetId(odsy_chunked_set *cset,int elemnr)
{
	odsy_chunk_node *c_node;
	int c_node_nr      = elemnr >> cset->c_size;
	int c_node_elem_nr = elemnr & ((1<<cset->c_size)-1);

	int i;
	ASSERT(cset->c_size > 0); /*not already destroyed*/
	ASSERT(c_node_nr < cset->nr_chunks);
	ASSERT(cset->nr_elem);
	for (i=0,c_node=cset->head;i<c_node_nr;i++,c_node=c_node->s) ;
	ASSERT(c_node->chunk);
	ASSERT(c_node->nr_elem);
	c_node->chunk[c_node_elem_nr] = 0;
	c_node->nr_elem--;
	cset->nr_elem--;
	if ( c_node->nr_elem == 0 ) {
		kmem_free(c_node->chunk,sizeof(void*)*(1<<cset->c_size));
		c_node->chunk = 0;
	}

}


void* OdsyLookupCSetId(odsy_chunked_set *cset,int elemnr)
{
	odsy_chunk_node *c_node;
	int c_node_nr      = elemnr >> cset->c_size;
	int c_node_elem_nr = elemnr & ((1<<cset->c_size)-1);

	int i;
	ASSERT(cset->c_size > 0); /*not already destroyed*/
	for (i=0,c_node=cset->head;(i<c_node_nr)&&c_node;i++,c_node=c_node->s) ;
	if (c_node == NULL) return NULL;
	if ( c_node->chunk) return c_node->chunk[c_node_elem_nr];
	return NULL;
}




void OdsyInit32kSlotMask(odsy_32k_slot_mask *smask,sema_t *lock)
{
#ifdef ODSY_DEBUG_32KSLOTS
	cmn_err(CE_DEBUG,"init 32k slots 0x%x\n",smask);
#endif
	bzero((caddr_t)smask,sizeof(odsy_32k_slot_mask));
	smask->first_free = 0;
	smask->lock = lock;
}

int OdsyAlloc32kSlot(odsy_32k_slot_mask *smask)
{
	__uint64_t mask_word,mask;
	int        slot_id;
	int        mask_pos;
	

	if (smask->lock) psema(smask->lock,PZERO);

	if(smask->first_free>=512 || smask->first_free<0) {
		if(smask->lock) vsema(smask->lock);
		return -1;
	}

	mask_word = smask->masks[smask->first_free];
	
	for (mask_pos=0,mask=1;  (mask&mask_word) && (mask_pos<64);   mask_pos++,mask<<=1) ;
	ASSERT(mask_pos != 64);
	ASSERT((mask&mask_word) == 0);

	slot_id = (smask->first_free * 64) + mask_pos;
	if (~(smask->masks[smask->first_free] = mask_word |  mask) == 0){
		/* this mask slot is out of bits.  find the next highest with free bits */
		int this_mask_nr = smask->first_free++;
		for (;smask->first_free<512 && (smask->masks[smask->first_free] == ~0L);smask->first_free++);
	}


#ifdef ODSY_DEBUG_32KSLOTS
	cmn_err(CE_DEBUG,"32K: alloc'd slot %d\n",slot_id);
#endif
	if (smask->lock) vsema(smask->lock);



	return slot_id;

}

void OdsyFree32kSlot(odsy_32k_slot_mask *smask, int slot_id)
{
	int mask_slot = slot_id>>6;
	int mask_bit  = slot_id&((1<<6) - 1);

	if (! smask ) {
		cmn_err(CE_WARN,"odsy: OdsyFree32kSlot given bogus smask!\n");
		return;
	}

	if ( mask_slot >= ((32<<10)/(sizeof(__uint64_t)*8)) ){
		cmn_err(CE_WARN,"odsy: OdsyFree32kSlot given bogus slot id!\n");
		return;
	}


	if (smask->lock) psema(smask->lock,PZERO);

	smask->masks[mask_slot] &= ~(1<<mask_bit);

	if (mask_slot < smask->first_free)
		smask->first_free = mask_slot;

#ifdef ODSY_DEBUG_32KSLOTS
	cmn_err(CE_DEBUG,"32K: freed slot %d: first free %d\n",slot_id,smask->first_free);
#endif

	if (smask->lock) vsema(smask->lock);
}


/*
 * Generic Hash Table
 */
odsy_hash_table *
odsyHashAlloc(int size)
{
    odsy_hash_table	*table;
    size_t		total_size;
    
    total_size = sizeof(odsy_hash_table) + (size-1) * sizeof(odsy_hash_elem);
    table = (odsy_hash_table *)kmem_alloc(total_size, KM_SLEEP);
    if (table) {
	int	i;

#ifdef DEBUG
	table->magic = ODSY_HASH_MAGIC;
#endif
	table->size = size;
	for (i = 0; i < size; i++) {
#ifdef DEBUG
	    table->heads[i].magic = ODSY_HASH_MAGIC;
#endif
	    table->heads[i].next = table->heads[i].prev = &table->heads[i];
	}
    }

    return table;
}

void
odsyHashDealloc(odsy_hash_table *table)
{
    int		i;
    size_t	total_size;

    ASSERT(table);
    ASSERT(table->size);
    ASSERT(table->magic == ODSY_HASH_MAGIC);

#ifdef DEBUG
    for (i = 0; i < table->size; i++) {
	ASSERT(table->heads[i].next == &table->heads[i]);
	ASSERT(table->heads[i].prev == &table->heads[i]);
    }
#endif
    total_size = sizeof(odsy_hash_table) + (table->size-1) * sizeof(odsy_hash_elem);
    kmem_free(table, total_size);
}

/*
 * Callers provide their own hash function and pass in the hash value in `hash`
 */
odsy_hash_elem *
odsyHashLookup(odsy_hash_table *table, int hash, int key)
{
    odsy_hash_elem	*ptr;

    ASSERT(hash < table->size);

    ptr = table->heads[hash].next;
    while (ptr != &table->heads[hash]) {
	if (ptr->key == key)
	    return ptr;
	ptr = ptr->next;
    }

    return NULL;
}

/*
 * Callers provide their own hash function and pass in the hash value in `hash`
 * Always insert at the head.
 */
void
odsyHashInsert(odsy_hash_table *table, int hash, odsy_hash_elem *new)
{
    ASSERT(hash < table->size);
    
    table->heads[hash].next->prev = new;
    new->next = table->heads[hash].next;
    new->prev = &table->heads[hash];
    table->heads[hash].next = new;
#ifdef DEBUG
    new->magic = ODSY_HASH_MAGIC;
#endif
}

void
odsyHashRemove(odsy_hash_elem *elem)
{
    ASSERT(elem->prev);
    ASSERT(elem->next);
    ASSERT(elem->prev->magic == ODSY_HASH_MAGIC);
    ASSERT(elem->next->magic == ODSY_HASH_MAGIC);

    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    elem->prev = elem->next = NULL;
#ifdef DEBUG
    elem->magic = 0xdeadbeef;
#endif
}

#endif	/* !_STANDALONE */
