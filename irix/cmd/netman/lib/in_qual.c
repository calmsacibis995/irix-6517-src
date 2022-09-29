/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 */
#include "index.h"

/*
 * Look for a key with a certain qualifier.
 */
void *
in_matchqual(Index *in, void *key, long qual)
{
	Entry *ent;
	void *unqual;

	ent = *in_lookup(in, key);
	if (ent == 0)
		return 0;
	unqual = 0;
	for (;;) {
		if (!ent->ent_qual)
			unqual = ent->ent_value;
		if (ent->ent_qual == qual)
			break;
		do {
			ent = ent->ent_next;
			if (ent == 0)
				return unqual;
		} while (in_cmpkeys(in, ent->ent_key, key));
	}
	return ent->ent_value;
}

void *
in_matchuniq(Index *in, void *key, long qual)
{
	Entry *ent;

	ent = *in_lookup(in, key);
	if (ent == 0)
		return 0;
	while (ent->ent_qual != qual) {
		do {
			ent = ent->ent_next;
			if (ent == 0)
				return 0;
		} while (in_cmpkeys(in, ent->ent_key, key));
	}
	return ent->ent_value;
}

/*
 * Enter a key distinguished by a qualifier.
 */
void
in_enterqual(Index *in, void *key, void *value, long qual)
{
	Entry **ep, *ent;

	ep = in_lookup(in, key);
	ent = *ep;
	if (ent == 0)
		goto install;
	for (;;) {
		if (ent->ent_qual == qual) {
			if (ent->ent_value)
				in_freeval(in, ent);
			ent->ent_value = value;
			return;
		}
		do {
			ep = &ent->ent_next;
			ent = *ep;
			if (ent == 0)
				goto install;
		} while (in_cmpkeys(in, ent->ent_key, key));
	}
install:
	ent = in_add(in, key, value, ep);
	ent->ent_qual = qual;
}

/*
 * Remove the key distinguished by a qualifier if it exists.
 */
void
in_removequal(Index *in, void *key, long qual)
{
	Entry **ep, *ent;

	for (ep = in_lookup(in, key); (ent = *ep) != 0; ep = &ent->ent_next) {
		if (!in_cmpkeys(in, ent->ent_key, key)
		    && ent->ent_qual == qual) {
			in_delete(in, ep);
			return;
		}
	}
}
