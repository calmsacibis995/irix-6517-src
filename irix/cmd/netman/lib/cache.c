/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * Dynamic index cache constructor, destructor, and operations.
 */
#include <limits.h>
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
struct dom_binding;
#include <rpcsvc/ypclnt.h>
#include <sys/stat.h>
#include <sys/file.h>
#include "cache.h"
#include "debug.h"
#include "heap.h"
#include "index.h"
#include "macros.h"
#include "strings.h"

#define	CACHEFILE_MAGIC	0x943EDF85	/* magic version number */

long	_now;				/* current time, approximately */

/*
 * LRU list operations.
 */
#ifdef DEBUGGING
#undef	LRU_INSERT
#define	LRU_INSERT(ent, list) { \
	Entry *e; \
	for (e = c->c_lru.el_next; e != (Entry*)&c->c_lru; e = e->ent_lrunext) \
		if (e == ent) \
			lru_botch(e, "pre-insert", "already in lru"); \
	lru_check(c, (Entry *)list, "pre-insert"); \
	(ent)->ent_lrunext = ((EntryList *)(list))->el_next; \
	(ent)->ent_lruprev = (Entry *)(list); \
	(ent)->ent_lrunext->ent_lruprev = (ent); \
	(ent)->ent_lruprev->ent_lrunext = (ent); \
	lru_check(c, ent, "post-insert"); \
}

#undef	LRU_REMOVE
#define	LRU_REMOVE(ent) \
	(lru_check(c, ent, "remove"), \
	 (ent)->ent_lrunext->ent_lruprev = (ent)->ent_lruprev, \
	 (ent)->ent_lruprev->ent_lrunext = (ent)->ent_lrunext, \
	 LRU_NULL(ent))

#define	MAXENTRIES	10000

static void
lru_botch(Entry *ent, char *message, char *reason)
{
	fprintf(stderr, "LRU BOTCH: ent @%#x next %#x prev %#x\n",
		ent, ent->ent_lrunext, ent->ent_lruprev);
	fprintf(stderr, "%s: %s\n", message, reason);
	abort();
}

static void
lru_check(Cache *c, Entry *thisent, char *message)
{
	unsigned brk, cnt;
	int inreverse, lruvisits;
	Entry *ent, *link;
	extern unsigned end;

	brk = sbrk(0);
	cnt = 0;
	inreverse = lruvisits = 0;
	ent = thisent;
	for (;;) {
		if (++cnt >= MAXENTRIES)
			lru_botch(ent, message, "probable loop");
		if (ent == (Entry *) &c->c_lru)
			lruvisits++;
		else if ((((unsigned *)ent)[-1] & 01) == 0)
			lru_botch(ent, message, "not busy in malloc arena");
		if (ent->ent_lrunext->ent_lruprev != ent)
			lru_botch(ent, message, "next doesn't point to prev");
		if (ent->ent_lruprev->ent_lrunext != ent)
			lru_botch(ent, message, "prev doesn't point to next");
		link = (!inreverse) ? ent->ent_lrunext : ent->ent_lruprev;
		if ((unsigned)link <= end || brk < (unsigned)(link + 1))
			lru_botch(ent, message, "link outside malloc arena");
		ent = link;
		if (ent == thisent) {
			if (inreverse)
				break;
			inreverse = 1;
		}
	}
	if (lruvisits != 2) {
		lru_botch((Entry *)&c->c_lru, message,
			  "list head not fully linked");
	}
}
#endif	/* DEBUGGING */

/*
 * Format of pathname to use for cache save/restore.
 */
static char	pathformat[] = "/tmp/.%s.cache";

static void	c_dump(Cache *, int);
static bool_t	xdrstdio_cache(FILE *, enum xdr_op, Cache *);

Cache *
cache(char *name, int maxsize, int keysize, int timeout, struct cacheops *ops,
      Cache **self)
{
	Cache *c;
	char path[_POSIX_PATH_MAX];
	FILE *file;
	time_t now;
	struct stat sb;

	c = new(Cache);
	c->c_info.ci_name = name;
	c->c_info.ci_purge = c_destroy;
	c->c_info.ci_dump = c_dump;
	ci_register(&c->c_info);
	in_init(&c->c_index, maxsize, keysize, &ops->co_indexops);
	LRU_NULL((Entry *) &c->c_lru);
	c->c_maxsize = maxsize;
	c->c_entries = 0;
	c->c_timeout = timeout;
	c->c_ops = ops;
	c->c_self = self;
	if (timeout <= 0 || ops->co_indexops.ino_xdrvalue == 0)
		return c;

	/*
	 * Check for a file from which to restore c's contents.
	 */
	(void) nsprintf(path, sizeof path, pathformat, name);
	file = fopen(path, "r");
	if (file == 0)
		return c;
	if (time(&now) >= 0 && fstat(fileno(file), &sb) == 0) {
		if (now >= sb.st_mtime + timeout)
			(void) unlink(path);
		else if (flock(fileno(file), LOCK_SH) == 0) {
			if (!xdrstdio_cache(file, XDR_DECODE, c))
				(void) unlink(path);
			(void) flock(fileno(file), LOCK_UN);
		}
	}
	fclose(file);
	return c;
}

static void
c_dump(Cache *c, int flags)
{
	if (flags & DUMP_SUMMARY) {
		printf("%s:\n", c->c_info.ci_name);
		printf("\thashsize %u maxsize %u entries %u\n",
			1 << c->c_index.in_shift, c->c_maxsize, c->c_entries);
	}
#ifdef METERING
	if (flags & DUMP_METERS) {
		printf("\tsearches %lu probes %lu hits %lu misses %lu\n",
			c->c_index.in_meter.im_searches,
			c->c_index.in_meter.im_probes,
			c->c_index.in_meter.im_hits,
			c->c_index.in_meter.im_misses);
		printf("\tenters %lu recycles %lu removes %lu flushes %lu\n",
			c->c_index.in_meter.im_enters,
			c->c_index.in_meter.im_recycles,
			c->c_index.in_meter.im_removes,
			c->c_index.in_meter.im_flushes);
	}
#endif
	if (flags & DUMP_ENTRIES) {
		Entry *ent;

		for (ent = c->c_lrutail; ent != (Entry *) &c->c_lru;
		     ent = ent->ent_lruprev) {
			(*c->c_ops->co_dumpent)(ent->ent_key, ent->ent_value,
						ent->ent_exptime);
		}
		putchar('\n');
	}
}

void
c_create(char *name, int maxsize, int keysize, int timeout,
	 struct cacheops *ops, Cache **self)
{
	if (*self)
		c_destroy(*self);
	*self = cache(name, maxsize, keysize, timeout, ops, self);
}

void
c_destroy(Cache *c)
{
	ci_unregister(&c->c_info);
	in_finish(&c->c_index);
	*c->c_self = 0;
	delete(c);
}

/*
 * Flush old entries from c.
 */
void
c_flush(Cache *c)
{
	Entry *ent, *lrunext;

	METER(c->c_index.in_meter.im_flushes++);
	for (ent = c->c_lruhead; ent != (Entry *) &c->c_lru; ent = lrunext) {
		lrunext = ent->ent_lrunext;
		if (ent->ent_exptime <= _now) {
			LRU_REMOVE(ent);
			in_remove(&c->c_index, ent->ent_key);
			--c->c_entries;
		}
	}
}

void
c_save(Cache *c)
{
	char path[_POSIX_PATH_MAX];
	FILE *file;

	if (c->c_timeout <= 0 || c->c_ops->co_indexops.ino_xdrvalue == 0)
		return;
	(void) nsprintf(path, sizeof path, pathformat, c->c_info.ci_name);
	file = fopen(path, "w");
	if (file == 0 || flock(fileno(file), LOCK_EX) < 0)
		return;
	if (!xdrstdio_cache(file, XDR_ENCODE, c))
		(void) unlink(path);
	(void) flock(fileno(file), LOCK_UN);
	fclose(file);
}

int
xdr_validate_ypfile(XDR *xdr, char *map, char *path)
{
	bool_t newbound, bound;
	int neworder;
	u_long order;
	struct stat sb;
	static char *domain;

#ifdef sun
	return 0;
#else
	newbound = bound = !_yp_disabled && _yp_is_bound;
	if (!xdr_bool(xdr, &bound) || bound != newbound)
		return 0;
	if (bound) {
		if (domain == 0 && yp_get_default_domain(&domain) != 0
		    || yp_order(domain, map, &neworder) != 0) {
			return 0;
		}
	} else {
		if (stat(path, &sb) < 0)
			return 0;
		neworder = sb.st_mtime;
	}
	order = neworder;
	return xdr_u_long(xdr, &order) && order == neworder;
#endif
}

Entry **
c_lookup(Cache *c, void *key)
{
	Entry **ep, *ent;

	ep = in_lookup(&c->c_index, key);
	ent = *ep;
	if (ent) {
		LRU_REMOVE(ent);
		if (ent->ent_exptime > _now) {
			LRU_INSERT(ent, c->c_lrutail);
		} else {
			in_delete(&c->c_index, ep);
			ep = in_lookup(&c->c_index, key);
		}
	}
	return ep;
}

void
c_add(Cache *c, void *key, void *value, Entry **ep)
{
	Entry *ent;

	ent = *ep;
	if (ent)
		LRU_REMOVE(ent);
	else if (c->c_entries < c->c_maxsize)
		c->c_entries++;
	else {
		METER(c->c_index.in_meter.im_recycles++);
		ent = c->c_lruhead;
		LRU_REMOVE(ent);
		if (ep == &ent->ent_next) {
			/*
			 * Entry removed from lruhead was were the add was
			 * going to take place.  Mark ep and recalculate
			 * after removing entry from index.
			 */
			ep = 0;
		}
		in_remove(&c->c_index, ent->ent_key);
		if (ep == 0) {
			/*
			 * Recalculate ep.  If adding another entry with the
			 * same key, move to the end of the chain.
			 */
			ep = in_lookup(&c->c_index, key);
			for (ent = *ep; ent != 0; ent = *ep)
				ep = &ent->ent_next;
		}
	}
	ent = in_add(&c->c_index, key, value, ep);
	LRU_INSERT(ent, c->c_lrutail);
	ent->ent_exptime = _now + c->c_timeout;
}

void
c_delete(Cache *c, Entry **ep)
{
	Entry *ent;

	ent = *ep;
	if (ent == 0)
		return;
	LRU_REMOVE(ent);
	in_delete(&c->c_index, ep);
	--c->c_entries;
}

void *
c_match(Cache *c, void *key)
{
	Entry *ent;

	ent = *c_lookup(c, key);
	if (ent == 0)
		return 0;
	return ent->ent_value;
}

void
c_enter(Cache *c, void *key, void *value)
{
	c_add(c, key, value, c_lookup(c, key));
}

void
c_remove(Cache *c, void *key)
{
	c_delete(c, c_lookup(c, key));
}

static bool_t	xdr_cache_entries(XDR *, Cache *);

static bool_t
xdrstdio_cache(FILE *file, enum xdr_op op, Cache *c)
{
	XDR xdr;
	u_long magic;
	bool_t ok;

	xdrstdio_create(&xdr, file, op);
	magic = CACHEFILE_MAGIC;
	ok = xdr_u_long(&xdr, &magic) && magic == CACHEFILE_MAGIC
	    && (c->c_ops->co_validate == 0 || (*c->c_ops->co_validate)(&xdr))
	    && xdr_cache_entries(&xdr, c);
	xdr_destroy(&xdr);
	return ok;
}

static bool_t
xdr_cache_entries(XDR *xdr, Cache *c)
{
	u_int keysize;
	Entry *ent;
	void *key, *value;
	long exptime;

	keysize = c->c_index.in_keysize;
	switch (xdr->x_op) {
	  case XDR_ENCODE:
		for (ent = c->c_lrutail; ent != (Entry *) &c->c_lru;
		     ent = ent->ent_lruprev) {
			value = ent->ent_value;
			if (value == 0)
				continue;
			key = ent->ent_key;
			if (!xdr_bytes(xdr, (char **)&key, &keysize, keysize)
			    || !in_xdrvalue(&c->c_index, xdr, &value, key)
			    || !xdr_long(xdr, &ent->ent_exptime)) {
				return FALSE;
			}
		}
		break;

	  case XDR_DECODE:
		key = value = 0;
		while (xdr_bytes(xdr, (char **)&key, &keysize, keysize)) {
			if (keysize != c->c_index.in_keysize
			    || !in_xdrvalue(&c->c_index, xdr, &value, key)
			    || !xdr_long(xdr, &exptime)) {
				delete(key);
				return FALSE;
			}
			ent = in_enter(&c->c_index, key, value);
			LRU_INSERT(ent, &c->c_lru);
			ent->ent_exptime = exptime;
			if (++c->c_entries == c->c_maxsize)
				break;
			value = 0;
		}
		delete(key);
	}
	return TRUE;
}
