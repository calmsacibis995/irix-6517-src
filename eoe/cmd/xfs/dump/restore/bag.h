#ifndef BAG_H
#define BAG_H

#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/xfs/dump/restore/RCS/bag.h,v 1.1 1995/06/21 22:45:46 cbullis Exp $"

/* bag.[hc] - bag abstraction
 *
 * user embeds a bagelem_t into items to be bagged. the element contains
 * an element key, and a user-specified pointer. items can be inserted
 * into the bag, removed from the bag, and searched for in the bag. the
 * user-embedded bagelem_t is soley owned by the bag: DON'T try to
 * reference bagelem_t members!
 */
struct bagelem {
	bool_t be_loaded;
	struct bag *be_bagp;
	size64_t be_key;
	void *be_payloadp;
	struct bagelem *be_nextp;
	struct bagelem *be_prevp;
};

typedef struct bagelem bagelem_t;

struct bag {
	bagelem_t *b_headp;
};

typedef struct bag bag_t;

/* creates a new bag
 */
extern bag_t *bag_alloc( void );

/* insert the item into the bag. the caller supplies a search key
 * and arbitrary payload.
 */
extern void bag_insert( bag_t *bagp,
			bagelem_t *bagelemp,
			size64_t key,
			void *payloadp );

/* remove the item from the bag. the key and payload originally supplied
 * to the insert operator are returned by reference.
 */
extern void bag_remove( bag_t *bagp,
			bagelem_t *bagelemp,
			size64_t *keyp,
			void **payloadpp );

/* search by key for an element in the bag.
 * returns the element pointer if a matching item is found, as well as
 * the payload (by reference). if the item is not in the bag, returns
 * a null pointer and (by reference) payload.
 */
extern bagelem_t *bag_find( bag_t *bagp,
			    size64_t key,
			    void **payloadpp );

/* private bag iterator
 */
struct bagiter {
	bag_t *bi_bagp;
	bagelem_t *bi_lastp;
	bagelem_t *bi_nextp;
};

typedef struct bagiter bagiter_t;

/* initializes a bag iterator
 */
extern void bagiter_init( bag_t *bagp, bagiter_t *iterp );

/* returns the next element in the bag. caller may remove the element
 * prior to the next call.
 */
extern bagelem_t * bagiter_next( bagiter_t *iterp, void **payloadpp );

/* destroys the bag.
 */
extern void bag_free( bag_t *bagp );

#endif /* BAG_H */
