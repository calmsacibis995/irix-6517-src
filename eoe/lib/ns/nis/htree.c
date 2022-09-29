/*
** This file contains some simple routines to maintain a hash tree.
** This is more or less like a Btree.
*/

#include <stdio.h>
#include <stdlib.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "htree.h"

/*
** This routine just sets the initial state of the tree structure.
*/
void
ht_init(ht_tree_t *ht)
{
	ht->top = 0;
}

static void
ht_nclear(ht_node_t **npp)
{
	ds_clear(&(*npp)->key);
	ds_clear(&(*npp)->val);

	free(*npp);
	*npp = 0;
}

static void
ht_dclear(ht_page_t **ppp)
{
	int i;

	for (i = 0; i < (*ppp)->used; i++) {
		if ((*ppp)->nodes[i]) {
			ht_nclear(&(*ppp)->nodes[i]);
		}
	}

	free(*ppp);
	*ppp = 0;
}

static void
ht_bclear(ht_branch_t **bpp)
{
	if ((*bpp)->left) {
		ht_bclear(&(*bpp)->left);
	}
	if ((*bpp)->right) {
		ht_bclear(&(*bpp)->right);
	}
	if ((*bpp)->data) {
		ht_dclear(&(*bpp)->data);
	}

	free(*bpp);
	*bpp = 0;
}

/*
** This routine just frees all allocated memory associated with a tree.
** It uses the recursive routine above to walk the tree deleting each
** node.
*/
void
ht_clear(ht_tree_t *ht)
{
	if (ht) {
		if (ht->top) {
			ht_bclear(&ht->top);
		}
		ht_init(ht);
	}
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
		if (pp->nodes[i]->hash & nbit) {
			if (! rpp) {
				rpp = (ht_page_t *)nsd_calloc(1, sizeof(*rpp));
				if (! rpp) {
					if (lpp) {
						free(lpp);
					}
					return 0;
				}
			}
			rpp->nodes[rpp->used++] = pp->nodes[i];
		} else {
			if (! lpp) {
				lpp = (ht_page_t *)nsd_calloc(1, sizeof(*lpp));
				if (! lpp) {
					if (rpp) {
						free(rpp);
					}
					return 0;
				}
			}
			lpp->nodes[lpp->used++] = pp->nodes[i];
		}
	}

	if (lpp) {
		bp->left = (ht_branch_t *)nsd_calloc(1, sizeof(*bp->left));
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
		bp->right = (ht_branch_t *)nsd_calloc(1, sizeof(*bp->right));
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
ht_insert(ht_tree_t *ht, ht_node_t *hn,
    int (*cmp)(const char *, const char *, size_t))
{
	ht_branch_t *bp;
	ht_page_t *pp;
	uint32_t hbit;
	int i;

	if (! ht) {
		return 0;
	}
	if (! ht->top) {
		ht->top = (ht_branch_t *)nsd_calloc(1, sizeof(*ht->top));
		if (! ht->top) {
			return 0;
		}
	}

	hn->hash = ht_hash(DS_STRING(&hn->key), DS_LEN(&hn->key));
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
			bp->data = (ht_page_t *)nsd_calloc(1,
			    sizeof(*bp->data));
			if (! bp->data) {
				return 0;
			}
			break;
		}

		if (hn->hash & (1 << hbit++)) {
			if (! bp->right) {
				bp->right = (ht_branch_t *)nsd_calloc(1,
				    sizeof(*bp->right));
				if (! bp->right) {
					return 0;
				}
			}
			bp = bp->right;
		} else {
			if (! bp->left) {
				bp->left = (ht_branch_t *)nsd_calloc(1,
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
			if ((DS_LEN(&hn->key) == DS_LEN(&pp->nodes[i]->key)) &&
			    ((*cmp)(DS_STRING(&hn->key),
			     DS_STRING(&pp->nodes[i]->key),
			     DS_LEN(&hn->key)) == 0)) {
				return 0;
			}
		}
		pp->nodes[pp->used++] = hn;
		return 1;
	}

	return 0;
}

/*
** This routine will lookup an item in the tree.
*/
ht_node_t *
ht_lookup(ht_tree_t *ht, char *key, size_t len,
    int (*cmp)(const char *, const char *, size_t))
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
		if ((len == DS_LEN(&pp->nodes[i]->key)) &&
		    ((*cmp)(DS_STRING(&pp->nodes[i]->key), key, len) == 0)) {
			return pp->nodes[i];
		}
	}

	return 0;
}

static void
ht_bwalk(ht_branch_t *bp, nsd_file_t *rq,
    void (*work)(void *, ht_node_t *))
{
	if (bp->left) {
		ht_bwalk(bp->left, rq, work);
	}
	if (bp->right) {
		ht_bwalk(bp->right, rq, work);
	}
	if (bp->data) {
		ht_page_t *pp;
		int i;

		pp = bp->data;
		for (i = 0; i < pp->used; i++) {
			if (pp->nodes[i]) {
				(*work)(rq, pp->nodes[i]);
			}
		}
	}
}

/*
** This routine just recursively calls a work routine on every item
** in the tree.  This is typically used to enumerate an entire table.
*/
void
ht_walk(ht_tree_t *ht, void *vp, void (*work)(void *, ht_node_t *))
{
	if (ht && ht->top) {
		ht_bwalk(ht->top, vp, work);
	}
}
