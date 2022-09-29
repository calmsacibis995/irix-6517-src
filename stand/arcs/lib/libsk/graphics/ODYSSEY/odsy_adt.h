#ifndef __ODSY_ADT_H
#define __ODSY_ADT_H
/*
** Copyright 1997, Silicon Graphics, Inc.
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

typedef struct odsy_chunked_set {
	struct odsy_chunk_node *head,*cursor_node;
	int nr_chunks,nr_elem, cursor_elem_nr, cursor_flipped;
	int c_size;
	
} odsy_chunked_set;



typedef struct odsy_chunk_node {
	void **chunk;
	int    nr_elem;
	struct odsy_chunk_node *s,*p;
} odsy_chunk_node;


typedef void (*odsy_cset_iterator_fn)(void *set_elem);
typedef int (*odsy_cset_match_fn)(void *set_elem,void *cmp_with);

void OdsyInitCSet(odsy_chunked_set *cset,int csize);
void OdsyInitCSetCursor(odsy_chunked_set *cset); 
void *OdsyNextCSetElem(odsy_chunked_set *cset);  /* you have to know when to stop */
void OdsyDestroyCSet(odsy_chunked_set *cset,odsy_cset_iterator_fn);
int OdsyAllocCSetId(odsy_chunked_set *cset,void *set_elem);
int OdsyAllocSpecificCSetId(odsy_chunked_set *cset,int elemnr,void *elemp);
void OdsyDeallocCSetId(odsy_chunked_set *cset,int elemnr);
void* OdsyLookupCSetId(odsy_chunked_set *cset,int elemnr);
int OdsySetCSetId(odsy_chunked_set *cset,int elemnr,void *set_elem);
void *OdsyFindMatchingCSetElem(odsy_chunked_set *cset,void *cmpwith, odsy_cset_match_fn matching_fn);

typedef struct odsy_32k_slot_mask {
	sema_t *lock;
	int first_free;
	__uint64_t masks[(32<<10)/(sizeof(__uint64_t)*8)];
	/*32k/(bytes in a 64bit thing*bits in a byte) = 512*/
} odsy_32k_slot_mask;

void OdsyInit32kSlotMask(odsy_32k_slot_mask *smask,sema_t *);
int OdsyAlloc32kSlot(odsy_32k_slot_mask *smask);
void OdsyFree32kSlot(odsy_32k_slot_mask *smask, int);

/*
 * Generic hash table
 */

#define ODSY_HASH_MAGIC		0x48415348	/* HASH */

typedef struct odsy_hash_elem {
#ifdef DEBUG
    unsigned int		magic;
#endif
    struct odsy_hash_elem	*next, *prev;
    int				key;
} odsy_hash_elem;

typedef struct odsy_hash_table {
#ifdef DEBUG
    unsigned int	magic;
#endif
    int			size;
    odsy_hash_elem	heads[1];
} odsy_hash_table;

odsy_hash_table *odsyHashAlloc(int);
void odsyHashDealloc(odsy_hash_table *);
odsy_hash_elem *odsyHashLookup(odsy_hash_table *, int, int);
void odsyHashInsert(odsy_hash_table *, int, odsy_hash_elem *);
void odsyHashRemove(odsy_hash_elem *);

#endif







