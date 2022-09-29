#ifndef HEAP_H
#define HEAP_H
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * An allocator interface which hides type casting and error checking,
 * and which enables the user to tag objects with their type names.
 */

#define	new(type)		((type *) _new(sizeof(type)))
#define	tnew(type, size)	((type *) _new(size))
#define	xnew(type, extra)	((type *) _new(sizeof(type) + (extra)))
#define	vnew(len, type)		((type *) _new((len) * sizeof(type)))
#define	renew(p, len, type)	((type *) _renew((p), (len) * sizeof(type)))
#define	znew(type)		((type *) _znew(sizeof(type)))

void	*_new(int);
void	*_renew(void *, int);
void	*_znew(int);
void	delete(void *);

/*
 * Cache registry.  Private caches which hang onto dynamic memory
 * should register themselves so that they can be purged in the event
 * of a memory shortage.
 */
typedef struct cacheinfo {
	char			*ci_name;
	void			(*ci_purge)();
	void			(*ci_dump)();
	struct cacheinfo	*ci_next;
	struct cacheinfo	*ci_prev;
} CacheInfo;

void	ci_register(struct cacheinfo *);
void	ci_unregister(struct cacheinfo *);
void	purgecaches();
void	dumpcaches(int);
void	savecaches();

#define	DUMP_SUMMARY	01
#define	DUMP_METERS	02
#define	DUMP_ENTRIES	04

/*
 * Memory shortage exception handler.  The default handler purges all
 * registered caches.
 */
typedef	void *(*handler_t)(int);

handler_t outofmemhandler(handler_t);

#endif
