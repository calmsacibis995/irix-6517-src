#ifndef CACHE_H
#define CACHE_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * Dynamic, persistent index caches.
 */
#include "heap.h"
#include "index.h"

struct __xdr_s;

typedef struct cache {
	CacheInfo	c_info;		/* cache registry info */
	Index		c_index;	/* opaque key hash table */
	EntryList	c_lru;		/* LRU list of all entries */
	unsigned int	c_maxsize;	/* maximum number of entries */
	unsigned int	c_entries;	/* current number of entries */
	long		c_timeout;	/* entry expiration time limit */
	struct cacheops	*c_ops;		/* virtual operations */
	struct cache	**c_self;	/* pointer to self ptr */
} Cache;

#define	c_lruhead	c_lru.el_next	/* list pointer nicknames */
#define	c_lrutail	c_lru.el_prev
#define	ent_lrunext	ent_list.el_next
#define	ent_lruprev	ent_list.el_prev
#define	ent_exptime	ent_qual	/* entry expiration time */

/*
 * LRU list operations.
 */
#define	LRU_NULL(ent) \
	((ent)->ent_lrunext = (ent)->ent_lruprev = (ent))

#define	LRU_INSERT(ent, list) \
	((ent)->ent_lrunext = ((EntryList *)(list))->el_next, \
	 (ent)->ent_lruprev = (Entry *)(list), \
	 (ent)->ent_lrunext->ent_lruprev = (ent), \
	 (ent)->ent_lruprev->ent_lrunext = (ent))

#define	LRU_REMOVE(ent) \
	((ent)->ent_lrunext->ent_lruprev = (ent)->ent_lruprev, \
	 (ent)->ent_lruprev->ent_lrunext = (ent)->ent_lrunext, \
	 LRU_NULL(ent))

/*
 * Cache operations, based on indexops.
 */
struct cacheops {
	struct indexops	co_indexops;		/* base class operations */
	int		(*co_validate)();	/* check validity of tmp file */
	void		(*co_dumpent)();	/* dump entry to stdout */
};

Cache	*cache(char *, int, int, int, struct cacheops *, Cache **);
void	c_create(char *, int, int, int, struct cacheops *, Cache **);
void	c_destroy(Cache *);
void	c_flush(Cache *);
void	c_save(Cache *);
int	xdr_validate_ypfile(struct __xdr_s *, char *, char *);

Entry	**c_lookup(Cache *, void *);
void	c_add(Cache *, void *, void *, Entry **);
void	c_delete(Cache *, Entry **);
void	*c_match(Cache *, void *);
void	c_enter(Cache *, void *, void *);
void	c_remove(Cache *, void *);

/*
 * Expression fragment macros used as a "units" suffix for the constant
 * timeout argument to cache/c_create.
 */
#define	MINUTES	* 60
#define	HOURS	* 60 MINUTES
#define	DAYS	* 24 HOURS
#define	MINUTE	MINUTES
#define	HOUR	HOURS
#define	DAY	DAYS

/*
 * It is up to calling code to set _now to the "current" time in seconds.
 */
extern long	_now;

#endif
