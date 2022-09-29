/*
** This file contains some simple routines to maintain a hash tree.
** This is more or less like a Btree.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "htree.h"

/*
** This routine just sets the initial state of the tree structure.
*/
void
ht_init(ht_tree_t *ht)
{
	ht->top = 0;
}

/*
** This is the hash routine for all the elements in the tree.
*/
static uint32_t
ht_hash(char *key, int len)
{
	uint32_t val;
	char *end = key + len;

	for (val = 0; key < end; key++) {
		val *= 16777619;
		val ^= *key;
	}

	return val;
}

static int
ht_split(ht_branch_t *bp, uint32_t hbit)
{
	int i;
	uint32_t nbit;
	ht_page_t *pp, *lpp=0, *rpp=0;

	pp = bp->data;

	nbit = 1 << hbit;
	for (i = 0; i < pp->used; i++) {
		if (pp->hashes[i] & nbit) {
			if (! rpp) {
				rpp = (ht_page_t *)calloc(1, sizeof(*rpp));
				if (! rpp) {
					if (lpp) {
						free(lpp);
					}
					return 0;
				}
			}
			rpp->hashes[rpp->used] = pp->hashes[i];
			rpp->nodes[rpp->used++] = pp->nodes[i];
		} else {
			if (! lpp) {
				lpp = (ht_page_t *)calloc(1, sizeof(*lpp));
				if (! lpp) {
					if (rpp) {
						free(rpp);
					}
					return 0;
				}
			}
			lpp->hashes[lpp->used] = pp->hashes[i];
			lpp->nodes[lpp->used++] = pp->nodes[i];
		}
	}

	if (lpp) {
		bp->left = (ht_branch_t *)calloc(1, sizeof(*bp->left));
		if (! bp->left) {
			free(lpp);
			if (rpp) {
				free(rpp);
			}
			return 0;
		}
		bp->left->data = lpp;
	}

	if (rpp) {
		bp->right = (ht_branch_t *)calloc(1, sizeof(*bp->right));
		if (! bp->right) {
			free(rpp);
			if (lpp) {
				free(lpp);
				free(bp->left);
				bp->left = 0;
			}
			return 0;
		}
		bp->right->data = rpp;
	}

	free(bp->data);
	bp->data = 0;

	return 1;
}

/*
** This routine will insert an item into the tree.
*/
int
ht_insert(ht_tree_t *ht, ht_node_t *hn)
{
	ht_branch_t *bp;
	ht_page_t *pp;
	uint32_t hbit, hash;
	int i;

	if (! ht) {
		return 0;
	}
	if (! ht->top) {
		ht->top = (ht_branch_t *)calloc(1, sizeof(*ht->top));
		if (! ht->top) {
			return 0;
		}
	}

	hash = ht_hash(hn->key, hn->len);
	hbit = 0;
	bp = ht->top;
	while (bp && (hbit < HT_HASHSIZE)) {
		if (bp->data) {
			if (bp->data->used < HT_PAGESIZE) {
				break;
			}
			if (! ht_split(bp, hbit)) {
				return 0;
			}
		}

		if (! (bp->left || bp->right)) {
			bp->data = (ht_page_t *)calloc(1,
			    sizeof(*bp->data));
			if (! bp->data) {
				return 0;
			}
			break;
		}

		if (hash & (1 << hbit++)) {
			if (! bp->right) {
				bp->right = (ht_branch_t *)calloc(1,
				    sizeof(*bp->right));
				if (! bp->right) {
					return 0;
				}
			}
			bp = bp->right;
		} else {
			if (! bp->left) {
				bp->left = (ht_branch_t *)calloc(1,
				    sizeof(*bp->left));
				if (! bp->left) {
					return 0;
				}
			}
			bp = bp->left;
		}
	}

	pp = bp->data;
	if (pp && (pp->used < HT_PAGESIZE)) {
		for (i = 0; i < pp->used; i++) {
			if ((hn->key == pp->nodes[i]->key) &&
			    (memcmp(hn->key, &pp->nodes[i]->key,
				    hn->len) == 0)) {
				return 0;
			}
		}
		pp->hashes[pp->used] = hash;
		pp->nodes[pp->used++] = hn;
		return 1;
	}

	return 0;
}

/*
** This routine will lookup an item in the tree.
*/
ht_node_t *
ht_lookup(ht_tree_t *ht, char *key, size_t len)
{
	ht_branch_t *bp;
	ht_page_t *pp;
	uint32_t i, hbit, hash;

	if (! ht) {
		return 0;
	}

	hash = ht_hash(key, len);
	hbit = 0;
	bp = ht->top;
	while (bp && ! bp->data && (hbit < HT_HASHSIZE)) {
		if (hash & (1 << hbit++)) {
			bp = bp->right;
		} else {
			bp = bp->left;
		}
	}

	if (! bp || ! bp->data) {
		return 0;
	}

	pp = bp->data;
	for (i = 0; i < pp->used; i++) {
		if ((len == pp->nodes[i]->len) &&
		    (memcmp(pp->nodes[i]->key, key, len) == 0)) {
			return pp->nodes[i];
		}
	}

	return 0;
}
