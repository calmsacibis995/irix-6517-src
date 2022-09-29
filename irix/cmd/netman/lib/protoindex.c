/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 * Protocol index implementation.
 */
#include <stdio.h>
#include <time.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "heap.h"
#include "debug.h"
#include "protocol.h"
#include "protoindex.h"

typedef struct discriminant {
	long	d_typecode;		/* typecode field contents */
	long	d_qualifier;		/* optional typecode qualifier */
} Discriminant;

#define	D(k)	((Discriminant *) (k))
#define	PR(v)	((Protocol *) (v))

/* ARGSUSED */
static u_int
pin_hashkey(void *key, u_int keysize)
{
	return D(key)->d_typecode ^ D(key)->d_qualifier;
}

/* ARGSUSED */
static int
pin_cmpkeys(void *key1, void *key2, u_int keysize)
{
	return D(key1)->d_typecode != D(key2)->d_typecode
	    || D(key1)->d_qualifier != D(key2)->d_qualifier;
}

/* ARGSUSED */
static bool_t
xdr_protocol(XDR *xdr, void **valp, void *key)
{
	int protoid;

	switch (xdr->x_op) {
	  case XDR_ENCODE:
		return xdr_int(xdr, &PR(*valp)->pr_id);
	  case XDR_DECODE:
		if (!xdr_int(xdr, &protoid))
			return FALSE;
		*valp = findprotobyid(protoid);
		return (*valp != 0);
	}
	return TRUE;
}

/* ARGSUSED */
static void
pin_dumpent(void *key, void *value, time_t exptime)
{
	printf("(%#x, %#x) -> %s expires on %s",
		D(key)->d_typecode, D(key)->d_qualifier, PR(value)->pr_name,
		ctime(&exptime));
}

struct cacheops pinops = {
	{ pin_hashkey, pin_cmpkeys, xdr_protocol },
	0, pin_dumpent
};

static void
pin_add(ProtoIndex *pin, Discriminant *d, Protocol *pr, Entry **ep)
{
	Entry *ent;

	ent = *ep;
	if (ent)
		LRU_REMOVE(ent);
	else if (pin->pin_entries < pin->pin_maxsize)
		pin->pin_entries++;
	else {
		METER(pin->pin_index.in_meter.im_recycles++);
		ent = pin->pin_lruhead;
		if (ep == &ent->ent_next) {
			/*
			 * Entry removed from lruhead was were the add was
			 * going to take place.  Mark ep and recalculate
			 * after removing entry from index.
			 */
			ep = 0;
		}
		pin_remove(pin, D(ent->ent_key)->d_typecode,
				D(ent->ent_key)->d_qualifier);
		pin->pin_entries++;
		if (ep == 0) {
			/*
			 * Recalculate ep.  If adding another entry with the
			 * same key, move to the end of the chain.
			 */
			ep = in_lookup(&pin->pin_index, d);
			for (ent = *ep; ent != 0; ent = *ep)
				ep = &ent->ent_next;
		}
	}
	ent = in_add(&pin->pin_index, d, pr, ep);
	LRU_INSERT(ent, pin->pin_lrutail);
	ent->ent_exptime = _now + pin->pin_freshtime;
}

void
pin_create(char *name, int freshtime, int count, ProtoIndex **self)
{
	c_create(name, count, sizeof(Discriminant), freshtime, &pinops, self);
}

Protocol *
pin_match(ProtoIndex *pin, long typecode, long qualifier)
{
	Discriminant d;
	Entry *ent, *best;
	long staletime;

	d.d_typecode = typecode;
	d.d_qualifier = qualifier;
	ent = *in_lookup(&pin->pin_index, &d);
	if (ent == 0)
		return 0;
	if (ent->ent_exptime == 0)
		return ent->ent_value;
	best = 0;
	staletime = _now + pin->pin_freshtime;
	for (;;) {
		if (ent->ent_exptime <= staletime) {
			best = ent;
			if (_now < ent->ent_exptime)
				break;
		}
		do {
			ent = ent->ent_next;
			if (ent == 0) {
				if (best == 0)
					return 0;
				PR(best->ent_value)->pr_flags |= PR_DECODESTALE;
				goto found;
			}
		} while (in_cmpkeys(&pin->pin_index, ent->ent_key, &d));
	}
found:
	LRU_REMOVE(best);
	LRU_INSERT(best, pin->pin_lrutail);
	return best->ent_value;
}

void
pin_enter(ProtoIndex *pin, long typecode, long qualifier, Protocol *pr)
{
	Discriminant d;
	Entry **ep, *ent;
	long staletime;

	d.d_typecode = typecode;
	d.d_qualifier = qualifier;
	ep = in_lookup(&pin->pin_index, &d);
	ent = *ep;
	if (ent == 0 || ent->ent_exptime == 0) {
		/*
		 * From no entry or uncached, to cached or uncached.
		 */
		if (pr->pr_flags & PR_EMBEDCACHED)
			pin_add(pin, &d, pr, ep);
		else {
			ent = in_add(&pin->pin_index, &d, pr, ep);
			LRU_NULL(ent);
			ent->ent_exptime = 0;
		}
		return;
	}
	if (!(pr->pr_flags & PR_EMBEDCACHED)) {
		/*
		 * From cached to uncached: purge all cached entries.
		 */
		pin_remove(pin, typecode, qualifier);
		ent = in_add(&pin->pin_index, &d, pr,
			     in_lookup(&pin->pin_index, &d));
		LRU_NULL(ent);
		ent->ent_exptime = 0;
		return;
	}
	staletime = _now + pin->pin_freshtime;
	for (;;) {
		/*
		 * Append a cached entry (replace an existing one if
		 * its stale time matches).
		 */
		if (ent->ent_exptime == staletime) {
			pin_add(pin, &d, pr, ep);
			return;
		}
		do {
			ep = &ent->ent_next;
			ent = *ep;
			if (ent == 0) {
				pin_add(pin, &d, pr, ep);
				return;
			}
		} while (in_cmpkeys(&pin->pin_index, ent->ent_key, &d));
	}
	/* NOTREACHED */
}

void
pin_remove(ProtoIndex *pin, long typecode, long qualifier)
{
	Discriminant d;
	Entry **ep, *ent;
	long staletime;

	d.d_typecode = typecode;
	d.d_qualifier = qualifier;
	ep = in_lookup(&pin->pin_index, &d);
	ent = *ep;
	if (ent == 0)
		return;
	if (ent->ent_exptime == 0) {
		in_delete(&pin->pin_index, ep);
		return;
	}
	staletime = _now + pin->pin_freshtime;
	for (;;) {
		if (ent->ent_exptime <= staletime) {
			c_delete(pin, ep);
			goto next;
		}
		do {
			ep = &ent->ent_next;
next:
			ent = *ep;
			if (ent == 0)
				return;
		} while (in_cmpkeys(&pin->pin_index, ent->ent_key, &d));
	}
	/* NOTREACHED */
}
