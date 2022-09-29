/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Numeric index implementation.
 */
#include <bstring.h>
#include <values.h>
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include "address.h"
#include "debug.h"
#include "heap.h"
#include "index.h"
#include "macros.h"

Index *
index(int count, int keysize, struct indexops *ops)
{
	Index *in;

	in = new(Index);
	in_init(in, count, keysize, ops);
	return in;
}

void
in_destroy(Index *in)
{
	in_finish(in);
	delete(in);
}

void
in_create(int count, int keysize, struct indexops *ops, Index **inp)
{
	if (*inp)
		in_destroy(*inp);
	*inp = index(count, keysize, ops);
}

/*
 * Hashkey, cmpkeys, and xdrvalue operations used if an index creator doesn't
 * specify its own virtual operations.
 */
#define	A(cp)	((Address *)(cp))
#define	UL(cp)	(*(unsigned long *)(cp))
#define	US(cp)	(*(unsigned short *)(cp))

static unsigned int
default_hashkey(unsigned char *key, int keysize)
{
	unsigned int hash;

	switch (keysize) {
	  case sizeof(Address):
		return A(key)->a_high + A(key)->a_low;
	  case sizeof(long):
		return UL(key);
	  case sizeof(short):
		return US(key);
	  case sizeof(char):
		return *key;
	}
	hash = 0;
	while (--keysize >= 0)
		hash = (hash << 4) + (hash >> BITS(unsigned int) - 4) + *key++;
	return hash;
}

static int
default_cmpkeys(unsigned char *key1, unsigned char *key2, int keysize)
{
	switch (keysize) {
	  case sizeof(Address):
		return A(key1)->a_high != A(key2)->a_high
		    || A(key1)->a_low != A(key2)->a_low;
	  case sizeof(long):
		return UL(key1) != UL(key2);
	  case sizeof(short):
		return US(key1) != US(key2);
	  case sizeof(char):
		return *key1 != *key2;
	}
	return bcmp(key1, key2, keysize);
}

#undef	A
#undef	UL
#undef	US

static struct indexops default_indexops =
    { default_hashkey, default_cmpkeys, 0 };

/*
 * Initialize a numeric-key hash table.
 */
static XDR	freeing_xdr;
static char	junk;

void
in_init(Index *in, int count, int keysize, struct indexops *ops)
{
	LOG2CEIL(count, in->in_shift);
	count = 1 << in->in_shift;
	in->in_hashmask = count - 1;
	in->in_buckets = count;
	in->in_keysize = keysize;
	in->in_hash = vnew(count, Entry *);
	in->in_ops = (ops == 0) ? &default_indexops : ops;
	bzero(in->in_hash, count * sizeof in->in_hash[0]);
#ifdef METERING
	bzero(&in->in_meter, sizeof in->in_meter);
#endif
	if (freeing_xdr.x_private == 0)
		xdrmem_create(&freeing_xdr, &junk, sizeof junk, XDR_FREE);
}

void
in_finish(Index *in)
{
	int count;
	Entry **ep, *ent;

	count = in->in_buckets;
	for (ep = in->in_hash; --count >= 0; ep++) {
		while ((ent = *ep) != 0) {
			if (ent->ent_value)
				in_freeval(in, ent);
			*ep = ent->ent_next;
			delete(ent);
		}
	}
	delete(in->in_hash);
}

void
in_freeval(Index *in, Entry *ent)
{
	in_xdrvalue(in, &freeing_xdr, &ent->ent_value, ent->ent_key);
}

/*
 * Return a pointer to a pointer to the entry matching key if it exists,
 * otherwise to the pointer with which key should be installed.
 */
Entry **
in_lookup(Index *in, void *key)
{
	Entry **head, **ep, *ent;

	METER(in->in_meter.im_searches++);
	head = &in->in_hash[in_hashkey(in, key) & in->in_hashmask];
	ep = head;
	while ((ent = *ep) != 0) {
		if (!in_cmpkeys(in, ent->ent_key, key)) {
			/*
			 * Move ent to the front of its hash chain.
			 */
			*ep = ent->ent_next;
			ent->ent_next = *head;
			*head = ent;
			METER(in->in_meter.im_hits++);
			return head;
		}
		METER(in->in_meter.im_probes++);
		ep = &ent->ent_next;
	}
	METER(in->in_meter.im_misses++);
	return ep;
}

/*
 * Add an entry for a key-value pair, using an entry pointer-pointer returned
 * by a call to in_lookup(in, key).  If an entry exists already, replace its
 * value, otherwise create a new one.
 */
Entry *
in_add(Index *in, void *key, void *value, Entry **ep)
{
	Entry *ent;
	int keysize, extra;

	ent = *ep;
	if (ent) {
		if (ent->ent_value)
			in_freeval(in, ent);
		ent->ent_value = value;
		return ent;
	}
	METER(in->in_meter.im_enters++);
	keysize = in->in_keysize;
	extra = keysize - sizeof ent->ent_key;
	ent = xnew(Entry, extra < 0 ? 0 : extra);
	ent->ent_next = *ep;
	ent->ent_value = value;
	bcopy(key, ent->ent_key, keysize);
	*ep = ent;
	return ent;
}

/*
 * Remove an entry given a pointer to the hash chain link that points at it.
 */
void
in_delete(Index *in, Entry **ep)
{
	Entry *ent;

	ent = *ep;
	if (ent == 0)
		return;
	METER(in->in_meter.im_removes++);
	if (ent->ent_value)
		in_freeval(in, ent);
	*ep = ent->ent_next;
	delete(ent);
}

/*
 * Return the value associated with key, or null no entry for key exists.
 */
void *
in_match(Index *in, void *key)
{
	Entry *ent;

	ent = *in_lookup(in, key);
	if (ent)
		return ent->ent_value;
	return 0;
}

/*
 * Add an entry for key and value, or update key's existing entry with value.
 */
Entry *
in_enter(Index *in, void *key, void *value)
{
	return in_add(in, key, value, in_lookup(in, key));
}

/*
 * Remove key's entry if it exists.
 */
void
in_remove(Index *in, void *key)
{
	in_delete(in, in_lookup(in, key));
}
