/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Heap implementation.
 */
#include <bstring.h>
#include <stdlib.h>
#include "cache.h"
#include "exception.h"
#include "heap.h"

static CacheInfo cacheinfo =
	{ "cacheinfo", 0, 0, &cacheinfo, &cacheinfo };

void
ci_register(CacheInfo *ci)
{
	ci->ci_next = &cacheinfo;
	ci->ci_prev = cacheinfo.ci_prev;
	ci->ci_next->ci_prev = ci->ci_prev->ci_next = ci;
}

void
ci_unregister(CacheInfo *ci)
{
	ci->ci_prev->ci_next = ci->ci_next;
	ci->ci_next->ci_prev = ci->ci_prev;
	ci->ci_next = ci->ci_prev = ci;
}

void
purgecaches()
{
	CacheInfo *ci, *next;

	for (ci = cacheinfo.ci_next; ci != &cacheinfo; ci = next) {
		next = ci->ci_next;
		(*ci->ci_purge)((Cache *) ci);
	}
}

void
dumpcaches(int flags)
{
	CacheInfo *ci;

	for (ci = cacheinfo.ci_next; ci != &cacheinfo; ci = ci->ci_next)
		(*ci->ci_dump)((Cache *) ci, flags);
}

void
savecaches()
{
	CacheInfo *ci;

	for (ci = cacheinfo.ci_next; ci != &cacheinfo; ci = ci->ci_next)
		c_save((Cache *) ci);
}

/* ARGSUSED */
static void *
default_outofmem(int size)
{
	purgecaches();
	return malloc(size);
}

static char outofmem_mesg[] = "Out of memory";
static handler_t outofmem = default_outofmem;

handler_t
outofmemhandler(handler_t nh)
{
	handler_t oh;

	oh = outofmem;
	if (nh)
		outofmem = nh;
	return oh;
}

void *
_new(int size)
{
	void *p;

	p = malloc(size);
	if (p == 0) {
		p = (*outofmem)(size);
		if (p == 0) {
			exc_perror(0, outofmem_mesg);
			exit(1);
		}
	}
	return p;
}

static int max_renew_size = 4096;

void *
_renew(void *p, int size)
{
	void *q;
	int oldsize, newsize;

	if ((unsigned)size > max_renew_size)
		return 0;
	if (p == 0) {
		size += sizeof(int);
		p = _znew(size);
		*(int *)p = size;
	} else {
		p = (char *)p - sizeof(int);
		size += sizeof(int);
		oldsize = *(int *)p;
		if (size != oldsize) {
			q = realloc(p, size);
			if (q == 0) {
				q = (*outofmem)(size);
				if (q == 0) {
					exc_perror(0, outofmem_mesg);
					exit(1);
				}
				bcopy((char *)p + sizeof(int),
				      (char *)q + sizeof(int),
				      oldsize);
				delete(p);
			}
			p = q;
			*(int *)p = size;
			newsize = size - oldsize;
			if (newsize > 0)
				bzero((char *)p + oldsize, newsize);
		}
	}
	return (char *)p + sizeof(int);
}

void *
_znew(int size)
{
	void *p;

	p = _new(size);
	bzero(p, size);
	return p;
}

void
delete(void *p)
{
	if (p)
		free(p);
}
