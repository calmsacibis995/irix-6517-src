#ifndef __FREELIST_H__
#define __FREELIST_H__

/*
 * FILE: freelist.h
 * DESC: generic free-list implementation
 */

/*
	Usage: (for some user struct `MY')

	... declare a free-list
	struct freelist afreelist;

	... implement the allocation functions
	FREELIST_DECL_FUNCS(struct MY, MY);

	... allocate an object
	struct MY *obj = MY_alloc(&afreelist);

	... free an object
	_MY_free(&afreelist, obj);

	Note that a single freelist can only hold objects of a
	single type.
*/

typedef struct freelist {
	struct freelist *fl_next;
} freelist;

#define FREELIST_INIT(flp) { (flp)->fl_next = NULL; }

#define FREELIST_DECL_FUNCS(type, name)				\
	type * name ## _alloc (freelist *flp);			\
	void name ## _free (freelist *flp, type *)

#define FREELIST_IMPL_FUNCS(type, name)				\
	type * name ## _alloc (freelist *flp) {			\
		type *retval;					\
		if (flp->fl_next == NULL)			\
			retval = (type*) malloc(sizeof(type));	\
		else {						\
			retval = (type*) flp->fl_next;		\
			flp->fl_next = flp->fl_next->fl_next;	\
		}						\
		return retval;					\
	}							\
	void name ## _free (freelist *flp, type *goner) {	\
		freelist *node = (freelist*) goner;		\
		node->fl_next = flp->fl_next;			\
		flp->fl_next = node;				\
	}

#endif /* __FREELIST_H__ */
