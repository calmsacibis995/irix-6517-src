#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include "htab.h"

void htab_init(htab_t *htab, int nbuckets)
{
	htab->ht_nbuckets = nbuckets;
	htab->ht_buckets = calloc(nbuckets, sizeof(hashable*));
}

void htab_insert(htab_t *htab, uint64_t hval, uint64_t val, hashable *obj)
{
	hashable **bucket = &htab->ht_buckets[hval % htab->ht_nbuckets];

	obj->ha_next = *bucket;
	obj->ha_prev = NULL;
	obj->ha_val = val;

	if (*bucket != NULL) {
		assert((*bucket)->ha_prev == NULL);
		(*bucket)->ha_prev = obj;
	}

	*bucket = obj;
}

hashable *htab_find(htab_t *htab, uint64_t hval, uint64_t val)
{
	hashable *chain = htab->ht_buckets[hval % htab->ht_nbuckets];

	while (chain != NULL && chain->ha_val != val)
		chain = chain->ha_next;

	return chain;
}

void htab_remove(htab_t *htab, uint64_t hval, hashable *obj)
{
	if (obj->ha_next != NULL)
		obj->ha_next->ha_prev = obj->ha_prev;

	if (obj->ha_prev == NULL)
		htab->ht_buckets[hval % htab->ht_nbuckets] = obj->ha_next;
	else
		obj->ha_prev->ha_next = obj->ha_next;
}

hashable *htab_begin(htab_t *htab)
{
	int bb;
	hashable *begin;

	for (bb = 0; bb < htab->ht_nbuckets; bb++)
		if ((begin = htab->ht_buckets[bb]) != NULL)
			break;

	return begin;
}

hashable *htab_next(htab_t *htab, uint64_t hval, hashable *obj)
{
	int bb;
	hashable *next;

	if ((next = obj->ha_next) != NULL) {
		return next;
	}
	else {
		for (bb = (hval % htab->ht_nbuckets) + 1;
			bb < htab->ht_nbuckets;
			bb++)
		{
			if ((next = htab->ht_buckets[bb]) != NULL)
				return next;
		}
	}

	return NULL;
}

void htab_dump(htab_t *htab, FILE *fp)
{
	int bb;
	int lastnull = 0;
	int firstnull;
	hashable *next;

	for (bb = 0; bb < htab->ht_nbuckets; bb++)
	{
		if (htab->ht_buckets[bb] == NULL) {
			if (!lastnull) {
				lastnull = 1;
				firstnull = bb;
			}
			continue;
		}

		if (lastnull) {
			lastnull = 0;
			fprintf(fp, "%d,%d:\tNULL\n",
				firstnull, bb-1);
		}

		fprintf(fp, "%d:\t", bb);

		for (next = htab->ht_buckets[bb]; next != NULL;
			next = next->ha_next)
		{
			fprintf(fp, " 0x%016llx", (uint64_t) next);
		}

		fprintf(fp, "\n");
	}
}
