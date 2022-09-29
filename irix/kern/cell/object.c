/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Id: object.c,v 1.12 1997/07/01 20:12:39 cp Exp $"

#include <sys/types.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/cred.h>
#include <sys/systm.h>
#include <ksys/kqueue.h>
#include <ksys/cell/object.h>
#include <sys/atomic_ops.h>
#include <sys/idbgentry.h>

typedef struct {
	kqueue_t	*bp_blk;
	uint_t		bp_off;
} bag_posn_t;

/*
 * Control structure for object bag state.
 */
typedef	struct {
	kqueue_t	ob_blkhead;	/* Listhead for data blocks */
	obj_bag_size_t	ob_blksize;	/* Block size */
	mutex_t		ob_lock;	/* Padlock */
	bag_posn_t	ob_ppos;	/* Current position for puts */
	bag_posn_t	ob_gpos;	/* Current position for gets */
	int		ob_nalloc;	/* Number of data blocks allocated */
} obj_bag_state_t;
#define ob_gblk		ob_gpos.bp_blk
#define ob_goff		ob_gpos.bp_off
#define ob_pblk		ob_ppos.bp_blk
#define ob_poff		ob_ppos.bp_off

/************************************************************************
 * Object Baggage Handling Routines.					*
 ************************************************************************/

#ifdef DEBUG
int obj_lost_baggage = 0;
int obj_lost_content = 0;
#endif

/*
 * Allocate memory for data to be put into or gotten from a bag.
 */
void *
obj_bag_alloc(
	size_t	datasize)
{
#ifdef DEBUG
	atomicAddInt(&obj_lost_content, 1);
#endif
	return kmem_alloc(datasize, KM_SLEEP);
}

/*
 * Free dynamically allocated memory returned by obj_bag_get()
 * or obj_bag_alloc().
 */
void
obj_bag_free(
	void	*datap,
	size_t	datasize)
{
#ifdef DEBUG
	ASSERT(obj_lost_content > 0);
	atomicAddInt(&obj_lost_content, -1);
#endif
	kmem_free(datap, datasize);
}

obj_bag_t
obj_bag_create(
	obj_bag_size_t	blksize)		/* IN block allocation size */
{
	obj_bag_state_t	*bagp;

	if (blksize == 0)
		blksize = _PAGESZ;
	else
		blksize += sizeof(kqueue_t);

	bagp = kmem_alloc(sizeof(obj_bag_state_t), KM_SLEEP);
	ASSERT(bagp);

	kqueue_init(&bagp->ob_blkhead);
	bagp->ob_blksize = MIN(blksize, _PAGESZ);
	mutex_init(&bagp->ob_lock, MUTEX_DEFAULT, "obj_bag");
	bagp->ob_gblk = &bagp->ob_blkhead;
	bagp->ob_goff = bagp->ob_blksize;
	bagp->ob_ppos = bagp->ob_gpos;	/* causes block alloc on 1st put */

	bagp->ob_nalloc = 0;
#ifdef DEBUG
	atomicAddInt(&obj_lost_baggage, 1);
#endif
	return (obj_bag_t) bagp;
}

void
obj_bag_destroy(
	obj_bag_t	bag)			/* IN bag */
{
	obj_bag_state_t	*bagp = (obj_bag_state_t *) bag;
	kqueue_t	*listhead = &bagp->ob_blkhead;
	kqueue_t	*kbp = kqueue_first(listhead);

	mutex_lock(&bagp->ob_lock, PZERO);
	/*
	 * Discard the contents.
	 */
	for (; kbp != kqueue_end(listhead); kbp = kqueue_next(listhead)) {
		kqueue_remove(kbp);
		obj_bag_free((void *)kbp, bagp->ob_blksize);
		bagp->ob_nalloc--;
	}
	ASSERT(bagp->ob_nalloc == 0);
#ifdef DEBUG
	atomicAddInt(&obj_lost_baggage, -1);
#endif
	mutex_destroy(&bagp->ob_lock);
	kmem_free((void *)bagp, sizeof(obj_bag_t));
}

void
obj_bag_rewind(
	obj_bag_t	bag)			/* IN bag */
{
	obj_bag_state_t	*bagp = (obj_bag_state_t *) bag;
	mutex_lock(&bagp->ob_lock, PZERO);
	/*
	 * Reset the get pointers do that the contents of the bag
	 * will be re-read.
	 */
	bagp->ob_gblk = &bagp->ob_blkhead;
	bagp->ob_goff = bagp->ob_blksize;
	mutex_unlock(&bagp->ob_lock);
}

int
obj_bag_tell(
	obj_bag_t	bag,			/* IN bag */
	obj_bag_mode_t	mode,			/* IN mode */
	obj_bag_posn_t	*posn)			/* OUT position */
{
	obj_bag_state_t	*bagp = (obj_bag_state_t *) bag;
	bag_posn_t	*posp = (bag_posn_t *) posn;
	int		error = OBJ_SUCCESS;

	mutex_lock(&bagp->ob_lock, PZERO);

	switch (mode) {
	case OBJ_BAG_MODE_GET:
		*posp = bagp->ob_gpos;
		break;
	case OBJ_BAG_MODE_PUT:
		*posp = bagp->ob_ppos;
		break;
	default:
		error = EINVAL;
	}

	mutex_unlock(&bagp->ob_lock);
	return error;
}

int
obj_bag_is_empty(
	obj_bag_t	bag)			/* IN bag */
{
	int		is_empty;
	obj_bag_state_t	*bagp = (obj_bag_state_t *) bag;

	mutex_lock(&bagp->ob_lock, PZERO);
	is_empty = bagp->ob_ppos.bp_blk == &bagp->ob_blkhead &&
		   bagp->ob_ppos.bp_off == bagp->ob_blksize;
	mutex_unlock(&bagp->ob_lock);
	return is_empty;
}

static int
bag_pos_is_valid(
	obj_bag_state_t	*bagp,
	bag_posn_t	*posp)
{
	kqueue_t	*listhead = &bagp->ob_blkhead;
	kqueue_t	*kbp;
	int		max_offset;

	/* Special "empty bag" case */
	if (posp->bp_blk == listhead && posp->bp_off == bagp->ob_blksize)
		return 1;

	/*
	 * First check that the block is in the list.
	 */
	for (kbp = kqueue_first(listhead);
  	     kbp != kqueue_end(listhead) && kbp != posp->bp_blk;
	     kbp = kqueue_next(kbp))
	       ;
	if (kbp == kqueue_end(listhead))
		return 0;

	/*
	 * Now check that the offset is within the block
	 * - remembering about the last block.
	 */
	max_offset = (posp->bp_blk == bagp->ob_pblk) ?
				bagp->ob_poff : bagp->ob_blksize;
	return (sizeof(kqueue_t) <= posp->bp_off && posp->bp_off < max_offset);
}

static void
bag_truncate(
	obj_bag_state_t		*bagp)
{
	kqueue_t	*listhead = &bagp->ob_blkhead;
	kqueue_t	*kbp;

	/* Chew off any blocks after the last put block. */
	for (kbp = kqueue_next(bagp->ob_pblk);
	     kbp != kqueue_end(listhead);
	     kbp = kqueue_next(bagp->ob_pblk)) {
		kqueue_remove(kbp);
		obj_bag_free(kbp, bagp->ob_blksize);
		bagp->ob_nalloc--;
	}
}

int
obj_bag_seek(
	obj_bag_t	bag,			/* IN bag */
	obj_bag_mode_t	mode,			/* IN mode */
	obj_bag_posn_t	*posn)			/* OUT position */
{
	obj_bag_state_t	*bagp = (obj_bag_state_t *) bag;
	bag_posn_t	*posp = (bag_posn_t *) posn;
	int		error = OBJ_SUCCESS;

	mutex_lock(&bagp->ob_lock, PZERO);
	if (!bag_pos_is_valid(bagp, posp)) {
		mutex_unlock(&bagp->ob_lock);
		return EINVAL;
	}

	switch (mode) {
	case OBJ_BAG_MODE_GET:
		bagp->ob_gpos = *posp;
		break;
	case OBJ_BAG_MODE_PUT:
		bagp->ob_ppos = *posp;
		bag_truncate(bagp);
		break;
	default:
		error = EINVAL;
	}

	mutex_unlock(&bagp->ob_lock);
	return error;
}

static void
obj_bag_put_internal(
	obj_bag_state_t	*bagp,			/* IN bag state */
	char		*datap,			/* IN buffer pointer */
	obj_bag_size_t	datasize)		/* IN amount to put */
{
	obj_bag_size_t	blk_remaining = bagp->ob_blksize -  bagp->ob_poff;
	int		put_remaining = datasize;
	char		*chunkp = datap;
	int		chunksize;

	/*
	 * In general, adding data to a bag has to be done in chunks
	 * (1) completing the current block, (2) filling intermediate blocks
	 * and (3) putting any remaining in the last block. The space
	 * left in the current block and the overall size of the
	 * data being put will determine which have to be dealt with.
	 */
	while (put_remaining > 0) {
		if (blk_remaining == 0) {
			/*
			 * Allocate another block
			 */
			bagp->ob_pblk = (kqueue_t *) obj_bag_alloc(bagp->ob_blksize);
			bagp->ob_nalloc++;
			ASSERT(bagp->ob_pblk);
			kqueue_enter_last(&bagp->ob_blkhead, bagp->ob_pblk);
			bagp->ob_poff = sizeof(kqueue_t);
			blk_remaining = bagp->ob_blksize -  sizeof(kqueue_t);
		}
		chunksize = MIN(put_remaining, blk_remaining);
		bcopy(chunkp, ((char *) bagp->ob_pblk) + bagp->ob_poff,
		      chunksize);
		bagp->ob_poff += chunksize;
		put_remaining -= chunksize;
		blk_remaining -= chunksize;
		chunkp += chunksize;
	}
	ASSERT(put_remaining == 0);
}

/*
 * Put <tag, datap, datasize> into bag bagp.
 */
int
obj_bag_put(
	obj_bag_t	bag,			/* IN bag */
	obj_tag_t	tag,			/* IN tag */
	void		*datap,			/* IN data pointer */
	obj_bag_size_t	datasize)		/* IN size of data */
{
	obj_bag_state_t	*bagp = (obj_bag_state_t *) bag;
#ifdef DEBUG
	obj_tag_t	delimiter = OBJ_TAG_DELIMITER;
#endif
	if (!OBJ_TAG_IS_VALID(tag))
		return OBJ_BAG_ERR_TAG;

	mutex_lock(&bagp->ob_lock, PZERO);

#ifdef DEBUG
	/* Put an internal delimiter. */
	obj_bag_put_internal(bagp, (char *) &delimiter, sizeof(obj_tag_t));
#endif
	obj_bag_put_internal(bagp, (char *) &tag, sizeof(obj_tag_t));
	obj_bag_put_internal(bagp, (char *) &datasize, sizeof(obj_bag_size_t));

	/*
	 * Put the data itself in the bag.
	 */
	obj_bag_put_internal(bagp, (char *) datap, datasize);

	mutex_unlock(&bagp->ob_lock);
	return 0;
}

static void
obj_bag_get_internal(
	obj_bag_state_t	*bagp,			/* IN bag state */
	void		*datap,			/* IN buffer pointer */
	obj_bag_size_t	datasize)		/* IN amount to get */
{
	obj_bag_size_t	blk_remaining = bagp->ob_blksize - bagp->ob_goff;
	int		get_remaining = datasize;
	char		*chunkp = datap;
	int		chunksize;

	/*
	 * In general, reading data from a bag is done in chunks
	 * (1) from the current block, (2) from intermediate blocks
	 * and (3) any remaining from the last block. The data
	 * left in the current block and the overall size of the
	 * data being put will determine which have to be dealt with.
	 */
	while (get_remaining > 0) {
		if (blk_remaining == 0) {
			/*
			 * Advance to the next block in list.
			 * Noting that the last block (put) may not be full.
			 */
			bagp->ob_gblk = kqueue_next(bagp->ob_gblk);
			ASSERT(bagp->ob_gblk);
			bagp->ob_goff = sizeof(kqueue_t);
			blk_remaining = ((bagp->ob_gblk == bagp->ob_pblk) ?
					 bagp->ob_poff : bagp->ob_blksize)
						- sizeof(kqueue_t);
		}
		chunksize = MIN(get_remaining, blk_remaining);
		if (chunkp != NULL) {
			bcopy(((char *) bagp->ob_gblk) + bagp->ob_goff, chunkp,
			      chunksize);
			chunkp += chunksize;
		}
		bagp->ob_goff += chunksize;
		get_remaining -= chunksize;
		blk_remaining -= chunksize;
	}
	ASSERT(get_remaining == 0);
}

/*
 * Get the next <tag, data, datasize> from bag.
 * The given tag must match the tag value read - else error.
 * The caller can provide a buffer to be used or can request
 * one to be allocated by providing a NULL address.
 */
int
obj_bag_get(
	obj_bag_t	bag,			/* IN bag */
	obj_tag_t	*tagp,			/* IN/OUT tag */
	void		**datapp,		/* IN/OUT buffer ptr */
	obj_bag_size_t	*datasizep)		/* IN/OUT size */
{
	obj_bag_state_t	*bagp = (obj_bag_state_t *) bag;
#ifdef DEBUG
	obj_tag_t	delimiter;
#endif
	int		error = OBJ_BAG_SUCCESS;
	bag_posn_t	gpos;
	obj_tag_t	expected_tag;
	obj_bag_size_t	expected_size;

	ASSERT(tagp);
	ASSERT(datapp);
	ASSERT(datasizep);
	expected_tag = *tagp;
	expected_size = *datasizep;

	mutex_lock(&bagp->ob_lock, PZERO);

	/* save current position to be restored in case of error */
	gpos = bagp->ob_gpos;

	/*
	 * Check for attempted getting beyond the end.
	 */
	if (bagp->ob_gblk == bagp->ob_pblk && bagp->ob_goff == bagp->ob_poff) {
		*tagp = OBJ_TAG_EMPTY;
		error = OBJ_BAG_ERR_TAG;
		goto out;
	}

#ifdef DEBUG
	obj_bag_get_internal(bagp, (char *) &delimiter, sizeof(obj_tag_t));
	ASSERT(delimiter == OBJ_TAG_DELIMITER);
#endif
	obj_bag_get_internal(bagp, (char *) tagp, sizeof(obj_tag_t));
	ASSERT(OBJ_TAG_IS_VALID(*tagp));
	if (expected_tag != OBJ_TAG_ANY && *tagp != expected_tag) {
		error = OBJ_BAG_ERR_TAG;
		goto out;
	}
	obj_bag_get_internal(bagp, (char *) datasizep, sizeof(obj_bag_size_t));

	/*
	 * Check whether the caller wants a buffer allocated and
	 * if not that the supplied buffer is ample.
	 */
	if (*datapp == NULL && *datasizep > 0) {
		 *datapp = obj_bag_alloc(*datasizep);
	} else if (*datasizep > expected_size) {
		error = OBJ_BAG_ERR_SIZE;
		goto out;
	}

	/*
	 * Get the data.
	 */
	obj_bag_get_internal(bagp, (char *) *datapp, *datasizep);

out:
	if (error)
		bagp->ob_gpos = gpos;
	mutex_unlock(&bagp->ob_lock);
	return error;
}

/*
 * Return the next <tag, datasize> in a bag.
 */
void
obj_bag_peek(
	obj_bag_t	bag,			/* IN bag */
	obj_tag_t	*tagp,			/* OUT tag */
	obj_bag_size_t	*datasizep)		/* OUT size */
{
	obj_bag_state_t	*bagp = (obj_bag_state_t *) bag;
	bag_posn_t	gpos;
#ifdef DEBUG
	obj_tag_t	delimiter;
#endif

	ASSERT(tagp);
	ASSERT(datasizep);

	mutex_lock(&bagp->ob_lock, PZERO);

	/* save current position to be restored when done */
	gpos = bagp->ob_gpos;

	/*
	 * Check for attempted getting beyond the end.
	 */
	if (bagp->ob_gblk == bagp->ob_pblk && bagp->ob_goff == bagp->ob_poff) {
		*tagp = OBJ_TAG_EMPTY;
		goto out;
	}

#ifdef DEBUG
	obj_bag_get_internal(bagp, (char *) &delimiter, sizeof(obj_tag_t));
	ASSERT(delimiter == OBJ_TAG_DELIMITER);
#endif
	obj_bag_get_internal(bagp, (char *) tagp, sizeof(obj_tag_t));
	ASSERT(OBJ_TAG_IS_VALID(*tagp));
	obj_bag_get_internal(bagp, (char *) datasizep, sizeof(obj_bag_size_t));

	bagp->ob_gpos = gpos;
out:
	mutex_unlock(&bagp->ob_lock);
}

/*
 * Discard the next <tag, datasize, data> in the bag - but only if
 * the tag matches what's been given.
 */
int
obj_bag_skip(
	obj_bag_t	bag,			/* IN bag */
	obj_tag_t	tag)			/* IN tag */
{
	obj_bag_state_t	*bagp = (obj_bag_state_t *) bag;
	obj_tag_t	next_tag;
	obj_bag_size_t	datasize;
	bag_posn_t	gpos;
	int		error = OBJ_BAG_SUCCESS;
#ifdef DEBUG
	obj_tag_t	delimiter;
#endif

	mutex_lock(&bagp->ob_lock, PZERO);

	/* save current position to be restored if error */
	gpos = bagp->ob_gpos;

	/*
	 * Check for attempted skip beyond the end.
	 */
	if (bagp->ob_gblk == bagp->ob_pblk && bagp->ob_goff == bagp->ob_poff) {
		error = OBJ_BAG_ERR_TAG;
		goto out;
	}

#ifdef DEBUG
	obj_bag_get_internal(bagp, (char *) &delimiter, sizeof(obj_tag_t));
	ASSERT(delimiter == OBJ_TAG_DELIMITER);
#endif
	obj_bag_get_internal(bagp, (char *) &next_tag, sizeof(obj_tag_t));
	ASSERT(OBJ_TAG_IS_VALID(next_tag));
	if (tag != OBJ_TAG_ANY && next_tag != tag) {
		error = OBJ_BAG_ERR_TAG;
		goto out;
	}
	obj_bag_get_internal(bagp, (char *) &datasize, sizeof(obj_bag_size_t));
	obj_bag_get_internal(bagp, NULL, datasize);

out:
	if (error)
		bagp->ob_gpos = gpos;
	mutex_unlock(&bagp->ob_lock);
	return error;
}

/*
 * Read the next item but don't step over it.
 * If the tag matches, data will be returned. Partial data being returned
 * if the specified buffer is too small.
 */
int
obj_bag_look(
	obj_bag_t	bag,			/* IN bag */
	obj_tag_t	*tagp,			/* IN/OUT tag */
	void		*datap,			/* IN buffer ptr */
	obj_bag_size_t	*datasizep)		/* IN/OUT size */
{
	obj_bag_state_t	*bagp = (obj_bag_state_t *) bag;
#ifdef DEBUG
	obj_tag_t	delimiter;
#endif
	int		error = OBJ_BAG_SUCCESS;
	bag_posn_t	gpos;
	obj_tag_t	expected_tag;
	obj_bag_size_t	expected_size;

	ASSERT(tagp);
	ASSERT(datap);
	ASSERT(datasizep);
	expected_tag = *tagp;
	expected_size = *datasizep;

	mutex_lock(&bagp->ob_lock, PZERO);

	/* save current position */
	gpos = bagp->ob_gpos;

	/*
	 * Check for attempted getting beyond the end.
	 */
	if (bagp->ob_gblk == bagp->ob_pblk && bagp->ob_goff == bagp->ob_poff) {
		*tagp = OBJ_TAG_EMPTY;
		error = OBJ_BAG_ERR_TAG;
		goto out;
	}

#ifdef DEBUG
	obj_bag_get_internal(bagp, (char *) &delimiter, sizeof(obj_tag_t));
	ASSERT(delimiter == OBJ_TAG_DELIMITER);
#endif
	obj_bag_get_internal(bagp, (char *) tagp, sizeof(obj_tag_t));
	ASSERT(OBJ_TAG_IS_VALID(*tagp));
	if (expected_tag != OBJ_TAG_ANY && *tagp != expected_tag) {
		error = OBJ_BAG_ERR_TAG;
		goto out;
	}
	obj_bag_get_internal(bagp, (char *) datasizep, sizeof(obj_bag_size_t));

	/*
	 * Get as much of the data as the supply buffer permits.
	 */
	obj_bag_get_internal(bagp, (char *) datap,
			     MIN(expected_size, *datasizep));
	if (*datasizep > expected_size)
		error = OBJ_BAG_ERR_SIZE;

out:
	bagp->ob_gpos = gpos;
	mutex_unlock(&bagp->ob_lock);
	return error;
}

int
cred_bag_put(
	obj_bag_t	bag,			/* IN bag */
	cred_t		*cr)			/* IN cred pointer */
{
	/*
	 * Copy the entire fixed-size structure.
	 * XXX - This could be optimized by looking at the number of groups.
	 */
	return obj_bag_put(bag, OBJ_TAG_CREDENTIALS, (void *) cr, crgetsize());
}

int
cred_bag_get(
	obj_bag_t	bag,			/* IN bag */
	cred_t		**crp)			/* OUT cred pointer */
{
	int		error;
	cred_t		*cr;
	size_t		crsize = crgetsize();
	obj_tag_t	tag = OBJ_TAG_CREDENTIALS;

	/* Allocate temporary space. */
	cr = (cred_t *) kmem_alloc(crsize, KM_SLEEP);

	error = obj_bag_get(bag, &tag, (void **) &cr, &crsize);
	if (error == OBJ_BAG_SUCCESS) {
		ASSERT(crsize == crgetsize());
		/* Copy creds into official cred structure */
		*crp = crdup(cr);
		ASSERT(*crp);
	}
	kmem_free((void *) cr, crsize);
	return error;
}

obj_bag_t
obj_bag_duplicate(
	obj_bag_t	bag)
{
	obj_bag_t	new_bag;
	obj_bag_state_t	*new_bagp;
	obj_bag_size_t	datasize;
	obj_bag_state_t	*bagp = (obj_bag_state_t *) bag;
	kqueue_t	*listhead = &bagp->ob_blkhead;
	kqueue_t	*kbp = kqueue_first(listhead);

	/* Create empty bag */
	new_bag = obj_bag_create(bagp->ob_blksize);
	new_bagp = (obj_bag_state_t *) new_bag;

	mutex_lock(&bagp->ob_lock, PZERO);
	mutex_lock(&new_bagp->ob_lock, PZERO);
	/*
	 * Copy the contents a bloack at a time.
	 */
	datasize = bagp->ob_blksize - sizeof(kqueue_t);
	for (; kbp != kqueue_end(listhead); kbp = kqueue_next(kbp)) {
		if (kbp == bagp->ob_pblk)
			datasize = bagp->ob_poff - sizeof(kqueue_t);
		obj_bag_put_internal(new_bagp, (void *)(kbp + 1), datasize);
	}	

	mutex_unlock(&bagp->ob_lock);
	mutex_unlock(&new_bagp->ob_lock);
	return new_bag;
}

/*
 * Convert a bag into a <header, vector> form for inter-cell messaging.
 */
void
obj_bag_to_message(
	obj_bag_t		bag,
	obj_bag_header_t	*hdrp,		/* OUT: bag header */
	obj_bag_vector_t	**vecp,		/* OUT: data vector */
	size_t			*sizep)		/* OUT: data vector count */
{
	obj_bag_state_t		*bagp = (obj_bag_state_t *) bag;
	obj_bag_vector_t	*bag_vec;
	kqueue_t		*listhead = &bagp->ob_blkhead;
	kqueue_t		*kbp = kqueue_first(listhead);
	int			nblk;
	int			i;

	mutex_lock(&bagp->ob_lock, PZERO);

	/*
	 * Allocate vector array.
	 */
	nblk = bagp->ob_nalloc;
	bag_vec = (obj_bag_vector_t *) kmem_alloc(
				nblk * sizeof(obj_bag_vector_t) , KM_SLEEP);

	/*
	 * Extract useful header info.
	 * - data blksize; number of blocks allocated and logical end offset.
	 */
	hdrp->obh_blksize = bagp->ob_blksize;
	hdrp->obh_nalloc = nblk;
	hdrp->obh_poff = bagp->ob_poff;

	/*
	 * Point the vector elements at the bag data blocks.
	 */
	for (kbp = kqueue_first(listhead), i = 0;
	     kbp != kqueue_end(listhead);
	     kbp = kqueue_next(kbp), i++) {
		bag_vec[i].ptr = (char *) kbp;
		bag_vec[i].count = bagp->ob_blksize;
	}
	ASSERT(i == nblk);

	mutex_unlock(&bagp->ob_lock);

	*vecp = bag_vec;
	*sizep = nblk;
}

/*
 * Convert a bag from inter-cell messaging format.
 */
obj_bag_t
obj_bag_from_message(
	obj_bag_header_t	*hdrp,		/* IN: bag header */
	obj_bag_vector_t	*vecp,		/* IN: data vector */
	size_t			size)		/* IN: data vector count */
{
	obj_bag_state_t		*bagp;
	int			i;

	/*
	 * Create a new bag state structure.
	 * Since the bag is new and not visible, don't take the bag lock.
	 */
	bagp = (obj_bag_state_t *) obj_bag_create(hdrp->obh_blksize);
	ASSERT(hdrp->obh_nalloc == size);
	bagp->ob_nalloc = size;
	bagp->ob_poff = hdrp->obh_poff;

	/*
	 * Rebuild the list of data blocks.
	 * We add each block to the list anchored by the object state struct.
	 * We also steal the block from the messaging system by
	 * zeroing the count fields in the vector.
	 */
	for (i = 0; i < size; i++) {
		bagp->ob_pblk = (kqueue_t *) vecp[i].ptr;
		ASSERT(vecp[i].count == bagp->ob_blksize);
		kqueue_enter_last(&bagp->ob_blkhead, bagp->ob_pblk);
		vecp[i].count = 0;
	}
#ifdef DEBUG
	atomicAddInt(&obj_lost_content, size);
#endif

	return (obj_bag_t) bagp;
}

void
obj_bag_free_message(
	obj_bag_vector_t	*vecp,		/* IN: data vector */
	size_t			size)		/* IN: data vector count */
{
	kmem_free((void *) vecp, size * sizeof(obj_bag_vector_t));
}

#ifdef DEBUG
void
idbg_bag(__psint_t x)
{
	obj_bag_state_t	*bagp = (obj_bag_state_t *) x;
	obj_tag_t	tag;
	obj_tag_t	delimiter;
	obj_bag_size_t	datasize;
	bag_posn_t	gpos;

	if (x == -1) {
		qprintf("currently allocated: bags %d; content blocks %d\n",
			obj_lost_baggage, obj_lost_content);
		return;
	}

	qprintf("bag block size %x list 0x%x nalloc %d lock 0x%x\n",
		bagp->ob_blksize, &bagp->ob_blkhead, bagp->ob_nalloc,
		&bagp->ob_lock);
	qprintf("  put pos (0x%x,0x%x); get pos (0x%x,0x%x)\n",
		bagp->ob_pblk, bagp->ob_poff, bagp->ob_gblk, bagp->ob_goff);

	/* Save get position and rewind to start */
	gpos = bagp->ob_gpos;
	bagp->ob_gblk = &bagp->ob_blkhead;
	bagp->ob_goff = bagp->ob_blksize;

	/*
	 * Scan through printing the tags, sizes and positions.
	 */
	qprintf("  contents:\n");
	while (!(bagp->ob_gblk == bagp->ob_pblk &&
	         bagp->ob_goff == bagp->ob_poff)) {
		if ((gpos.bp_blk == bagp->ob_gblk) &&
		    (gpos.bp_off == bagp->ob_goff))
			qprintf("g-> ");
		else
			qprintf("    ");

		obj_bag_get_internal(bagp, (char *) &delimiter,
				     sizeof(obj_tag_t));
		obj_bag_get_internal(bagp, (char *) &tag,
				     sizeof(obj_tag_t));
		obj_bag_get_internal(bagp, (char *) &datasize,
			 	     sizeof(obj_bag_size_t));
		qprintf("tag 0x%x, size 0x%x at (0x%x+0x%x)\n",
			tag, datasize, bagp->ob_gblk, bagp->ob_goff);
		/* Step over data */
		obj_bag_get_internal(bagp, (char *) NULL, datasize);
	}

	/* Restore get position */
	bagp->ob_gpos = gpos;

}
#endif
