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
#ident "$Id: object.h,v 1.4 1997/07/01 20:13:28 cp Exp $"

#ifndef	_KSYS_OBJECT_H_
#define	_KSYS_OBJECT_H_	1

#ifndef	CELL
#error included by non-CELL configuration
#endif

#include <sys/types.h>
#include <ksys/cell/service.h>

/*
 * Abstract object container - the object bag.
 *
 * The contents of a bag are tuples of the form: <tag,size,data>.
 * A bag is allocated by a call to obj_bag_create() specifying
 * the block allocation size. No blocks are allocated until required. 
 * Data is added to a bag using obj_bag_put(). A tag value identifies
 * the type of data being inserted. Data is extracted from a bag using
 * obj_bag_get(). The tag of the expected data must match the tag for
 * the next tuple (baggy). The contents of a bag may be discarded using
 * obj_bag_reset(). A bag may be read over using obj_bag_rewind().
 * A bag and its contents are destroyed by obj_bag_destroy().
 * 
 * For R1, bags contain data pointers rather than copied
 * data since both putter and getter have direct access to the
 * data (which must therefore not be destroyed until a referencing
 * bag is destroyed).
 */
typedef size_t obj_bag_size_t;
typedef void * obj_bag_t;

/*
 * Recognized object types:
 */
typedef	uint_t			obj_tag_t;

#define	OBJ_SVC_SHIFT		(sizeof(obj_tag_t)*8/2)
#define	OBJ_TAG_ID_MAX		((1 << OBJ_SVC_SHIFT) - 1)
/*
 * Subsystem specific unique tags are defined using:
 */
#define	OBJ_SVC_TAG(svc, id)	(((svc) << OBJ_SVC_SHIFT) | (id))

/*
 * Internal tags are defined using:
 */
#define	OBJ_TAG_INTERNAL(tag)	OBJ_SVC_TAG(NUMSVCS,tag)

#define	OBJ_TAG_ANY		OBJ_TAG_INTERNAL(0)
#define	OBJ_TAG_NONE		OBJ_TAG_INTERNAL(1)
#define	OBJ_TAG_DELIMITER	OBJ_TAG_INTERNAL(2)
#define	OBJ_TAG_EMPTY		OBJ_TAG_INTERNAL(3)	/* EOF */

/* Reserved for use by KORE: */
#define	OBJ_TAG_SVCNUM		OBJ_TAG_INTERNAL(4)
#define	OBJ_TAG_CELL		OBJ_TAG_INTERNAL(5)
#define	OBJ_TAG_OBJID		OBJ_TAG_INTERNAL(6)
#define	OBJ_TAG_HANDLE		OBJ_TAG_INTERNAL(7)
#define	OBJ_TAG_RMODE		OBJ_TAG_INTERNAL(8)

/* Token Module internal codes: */
#define OBJ_TAG_TKS_NTOKENS	OBJ_TAG_INTERNAL(9)
#define OBJ_TAG_TKS_GBITS	OBJ_TAG_INTERNAL(10)
#define OBJ_TAG_TKS_STATE	OBJ_TAG_INTERNAL(11)

/* Standard kernel data structures */
#define OBJ_TAG_CREDENTIALS	OBJ_TAG_INTERNAL(12)

/*
 * Tags are cracked and checked using:
 */
#define OBJ_TAG_SVC(tag)	((tag) >> OBJ_SVC_SHIFT) 
#define OBJ_TAG_ID(tag)		((tag) && OBJ_TAG_ID_MAX)
#define OBJ_TAG_IS_VALID(tag)	(OBJ_TAG_SVC(tag) <= NUMSVCS)	/* Not great */

/*
 * Error codes returned by baggage routines:
 */
#define OBJ_SUCCESS		0	/* No error */
#define OBJ_BAG_SUCCESS		0	/* No error */
#define OBJ_BAG_ERR_TAG		1	/* Incorrect tag */
#define OBJ_BAG_ERR_SIZE	2	/* Buffer too small */

/*
 * Baggage handlers:
 */
extern obj_bag_t obj_bag_create( /* Create and return pointer to a bag */
		obj_bag_size_t);	/* IN block allocation size */

extern void obj_bag_destroy(	/* Destroy object bag */
		obj_bag_t);		/* IN bag */

extern obj_bag_t obj_bag_duplicate( /* Create copy of a bag */
		obj_bag_t);		/* IN bag */

extern int obj_bag_put(		/* Put tagged data in bag */
		obj_bag_t,		/* IN bag */
		obj_tag_t,		/* IN tag identifying data */
		void *,			/* IN pointer to data */
		obj_bag_size_t);	/* IN size of data */

extern int obj_bag_get(		/* Get next tagged data from bag */
		obj_bag_t,		/* IN bag */
		obj_tag_t *,		/* IN/OUT tag */
		void **,		/* IN/OUT data pointer */
		obj_bag_size_t *);	/* IN/OUT data size */

extern void obj_bag_peek(	/* Return next tag and size in bag */
		obj_bag_t,		/* IN bag */
		obj_tag_t *,		/* OUT tag */
		obj_bag_size_t *);	/* OUT data size */

extern int  obj_bag_look(	/* Return next item in bag */
		obj_bag_t,		/* IN bag */
		obj_tag_t *,		/* IN/OUT tag */
		void *,			/* IN pointer to data */
		obj_bag_size_t *);	/* IN/OUT data size */

extern int obj_bag_skip(	/* Discard the next tagged data from bag */
		obj_bag_t,		/* IN bag */
		obj_tag_t);		/* IN tag */

/*
 * obj_bag_get_here() is a wrapper for obj_bag_get() when a buffer
 * is being supplied (not to be dynamically allocated) and the
 * caller is not wanting to have the actual data size (or tag) returned.
 */
#define	obj_bag_get_here(bag, tag, datap, size, error)			\
	{								\
		obj_tag_t	_tag = tag;				\
		void		*_datap = (void *) datap;		\
		size_t		_size = size;				\
		ASSERT(datap || size == 0);				\
		error = obj_bag_get(bag, &_tag, &_datap, &_size);	\
	}

extern void *obj_bag_alloc(	/* Allocate memory to put in bag */
		obj_bag_size_t);	/* IN data size */

extern void obj_bag_free(	/* Free memory returned by bag get/alloc */
		void *,			/* IN data pointer */
		obj_bag_size_t);	/* IN data size */

extern void obj_bag_rewind(	/* Reopen the bag for gets */
		obj_bag_t);		/* IN pointer to bag */

typedef struct {
	void	*blk;
	uint_t	off;
} obj_bag_posn_t;

typedef enum {
	OBJ_BAG_MODE_GET,
	OBJ_BAG_MODE_PUT
} obj_bag_mode_t;

extern int  obj_bag_tell(	/* Return position in bag */
		obj_bag_t,		/* IN pointer to bag */
		obj_bag_mode_t,		/* IN GET or PUT */
		obj_bag_posn_t *);	/* OUT position */
	
extern int  obj_bag_seek(	/* Set position in bag */
		obj_bag_t,		/* IN pointer to bag */
		obj_bag_mode_t,		/* IN GET or PUT */
		obj_bag_posn_t *);	/* IN position */
	
extern int  obj_bag_is_empty(	/* Anything been put into the bag? */
		obj_bag_t);		/* IN pointer to bag */


/*
 * Routines to bag and unbag standard kernel structures.
 */
struct cred;
extern int  cred_bag_put(		/* Credentials bag */
		obj_bag_t,		/* IN bag */
		struct cred *);		/* IN pointer to creds */
extern int  cred_bag_get(		/* Credentials unbag */
		obj_bag_t,		/* IN bag */
		struct cred **);	/* OUT pointer to creds */

/*
 * This is the object baggage interface with the messaging system.
 * A bag is copied between cells as a header and a data vector.
 */
typedef struct {
	obj_bag_size_t	obh_blksize;	/* data block size */
	int		obh_nalloc;	/* number of data blocks alloc'ed */
	uint		obh_poff;	/* end of bag offset in last block */
} obj_bag_header_t;

typedef struct {
	char		*ptr;		/* char data pointer */
	size_t		count;		/* char count (blocksize) */
} obj_bag_vector_t;

typedef char obj_bag_data_t;

extern void  obj_bag_to_message(   /* Create message structures for bag */
		obj_bag_t,		/* IN pointer to bag */
		obj_bag_header_t *,	/* OUT bag header */
		obj_bag_vector_t **,	/* OUT data vector */
		size_t *);		/* OUT vector count */ 

extern obj_bag_t obj_bag_from_message( /* Create bag from message structure */
		obj_bag_header_t *,	/* IN bag header */
		obj_bag_vector_t *,	/* IN data vector */
		size_t);		/* IN vector count */ 

extern void obj_bag_free_message(  /* Free message structures */
		obj_bag_vector_t *,	/* IN data vector */
		size_t);		/* IN vector count */ 

#endif	/* _KSYS_OBJECT_H_ */
