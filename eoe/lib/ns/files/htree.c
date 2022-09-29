#include <stdlib.h>
#include <values.h>
#include "memory.h"
#include "htree.h"

/*
** This is the default hash function.  It assumes a null terminated string.
** The magic prime number was supplied by Landon Curt Noll.
*/
static uint32_t
hhash(void *vp)
{
	uint32_t val;
	char *p;

	for (p = vp, val = 0; *p; p++) {
		val *= 16777619;
		val ^= *p;
	}

	return val;
}

/*
** Exported version of above routine.
*/
uint32_t
ht_hash(void *vp, int len)
{
	char *p, *end;
	uint32_t val;
	
	for (p = vp, end = p + len, val = 0; p < end; p++) {
		val *= 16777619;
		val ^= *p;
	}

	return val;
}

/*
** This should always get called on a new tree to initialize all the
** operations and memory.
*/
int
ht_init(htree_t *hp, int (*cmp)(void *, void *), void (*kfree)(void *, void *),
    void (*vfree)(void *, void *), uint32_t (*hash)(void *),
    int (*lock)(abilock_t *), int (*unlock)(abilock_t *),
    void *arena, unsigned flags)
{
	hp->cmp = (cmp) ? cmp : (int (*)(void *, void *))strcmp;
	hp->kfree = (kfree) ? kfree : m_free;
	hp->vfree = (vfree) ? vfree : m_free;
	hp->hash = (hash) ? hash : hhash;
	hp->lock = (lock) ? lock : (int (*)(abilock_t *))spin_lock;
	hp->unlock = (unlock) ? unlock : release_lock;
	hp->arena = arena;
	hp->flags = flags;

	hp->top = m_malloc(sizeof(*hp->top), arena);
	if (! hp->top) {
		return 0;
	}

	hp->top->left = 0;
	hp->top->right = 0;

	hp->top->data = m_calloc(1, sizeof(*hp->top->data), arena);
	if (! hp->top->data) {
		m_free(hp->top, arena);
		return 0;
	}

	return 1;
}

/*
** Called from ht_insert below to split a filled page.
*/
static int
hsplit(htree_t *hp, hnode_t *np, unsigned hbit)
{
	hnode_t *l, *r;
	hpage_t *ld, *rd;
	int i;

	l = m_calloc(2, sizeof(*l), hp->arena);
	if (! l) {
		return 0;
	}
	r = l + 1;

	rd = m_calloc(1, sizeof(*rd), hp->arena);
	if (! rd) {
		m_free(l, hp->arena);
		return 0;
	}

	rd->count = 0;
	r->data = rd;

	ld = np->data;
	l->data = ld;

	hbit = 1 << hbit;
	for (i = 0, ld->count = 0; i < HT_PAGESIZE; i++) {
		if (ld->item[i].hash & hbit) {
			rd->item[rd->count] = ld->item[i];
			rd->count++;
		} else {
			ld->item[ld->count] = ld->item[i];
			ld->count++;
		}
	}

	np->left = l;
	np->right = r;
	np->data = 0;

	return 1;
}

/*
** This routine inserts a new item into the tree.  If the HT_CHECK_DUPS
** flag is set on the tree then we will check that this key is unique.
** If the key is not unique then we return an error, unless the HT_REPLACE
** flag is passed in.  In that case we free the old item and replace it
** with the new.
*/
int
ht_insert(htree_t *hp, void *k, void *v, unsigned flags)
{
	uint32_t hash;
	unsigned hbit;
	hnode_t *n;
	hpage_t *p;

	hash = (*hp->hash)(k);
	for (n = hp->top, hbit = 0; hbit < BITS(hash); hbit++) {
		p = n->data;
		if (n->data) {
			(*hp->lock)(&p->lock);
			if (! n->data) {
				(*hp->unlock)(&p->lock);
			} else if (p->count < HT_PAGESIZE) {
				break;
			} else if (hsplit(hp, n, hbit)) {
				(*hp->unlock)(&p->lock);
			} else {
				(*hp->unlock)(&p->lock);
				return 0;
			}
		}
		if (hash & (1 << hbit)) {
			n = n->right;
		} else {
			n = n->left;
		}
	}

	if (p) {
		if (hp->flags & HT_CHECK_DUPS) {
			int i;

			for (i = 0; i < p->count; i++) {
				if ((p->item[i].hash == hash) &&
				    ((*hp->cmp)(k, p->item[i].key) == 0)) {
					if (flags & HT_REPLACE) {
						(*hp->kfree)(p->item[i].key,
							     hp->arena);
						p->item[i].key = k;
						if (p->item[i].val) {
							(*hp->vfree)(
							     p->item[i].val,
							     hp->arena);
						}
						p->item[i].val = v;
						(*hp->unlock)(&p->lock);
						return 1;
					}
					(*hp->unlock)(&p->lock);
					return 0;
				}
			}
		}

		p->item[p->count].hash = hash;
		p->item[p->count].key = k;
		p->item[p->count].val = v;
		p->count++;
		(*hp->unlock)(&p->lock);
		return 1;
	}

	return 0;
}

/*
** This routine just returns the value for the named item.
*/
void *
ht_lookup(htree_t *hp, void *k)
{
	uint32_t hash;
	unsigned hbit;
	hnode_t *n;
	hpage_t *p;
	void *vp;

	hash = (*hp->hash)(k);
	for (n = hp->top, hbit = 0; hbit < BITS(hash); hbit++) {
		p = n->data;
		if (n->data) {
			(*hp->lock)(&p->lock);
			if (! n->data) {
				(*hp->unlock)(&p->lock);
			} else {
				break;
			}
		}
		if (hash & (1 << hbit)) {
			n = n->right;
		} else {
			n = n->left;
		}
	}

	if (p) {
		int i;

		for (i = 0; i < p->count; i++) {
			if ((p->item[i].hash == hash) &&
			    ((*hp->cmp)(k, p->item[i].key) == 0)) {
				vp = p->item[i].val;
				(*hp->unlock)(&p->lock);
				return vp;
			}
		}
	}

	(*hp->unlock)(&p->lock);
	return 0;
}

/*
** This routine removes the named item from the tree and frees the memory.
*/
int
ht_delete(htree_t *hp, void *k)
{
	uint32_t hash;
	unsigned hbit;
	hnode_t *n;
	hpage_t *p;

	hash = (*hp->hash)(k);
	for (n = hp->top, hbit = 0; hbit < BITS(hash); hbit++) {
		p = n->data;
		if (n->data) {
			(*hp->lock)(&p->lock);
			if (! n->data) {
				(*hp->unlock)(&p->lock);
			} else {
				break;
			}
		}
		if (hash & (1 << hbit)) {
			n = n->right;
		} else {
			n = n->left;
		}
	}

	if (p) {
		int i;

		for (i = 0; i < p->count; i++) {
			if ((p->item[i].hash == hash) &&
			    ((*hp->cmp)(k, p->item[i].key) == 0)) {
				(*hp->kfree)(p->item[i].key, hp->arena);
				if (p->item[i].val) {
					(*hp->vfree)(p->item[i].val, hp->arena);
				}
				for (; i < p->count; i++) {
					p->item[i] = p->item[i + 1];
				}
				p->count--;

				(*hp->unlock)(&p->lock);
				return 1;
			}
		}
	}

	(*hp->unlock)(&p->lock);
	return 0;
}

/*
** Called from ht_walk below.  We recursively walk the tree calling the
** work function on each element.  If the routine returns false we
** remove the item.
*/
static int
hwalk(htree_t *hp, hnode_t *np, void *vp, int (*work)(void *, void *, void *))
{
	hpage_t *pp;

	if (np->left) {
		hwalk(hp, np->left, vp, work);
	}

	if (np->right) {
		hwalk(hp, np->right, vp, work);
	}

	pp = np->data;
	if (pp) {
		int i;

		(*hp->lock)(&pp->lock);
		for (i = 0; i < pp->count;) {
			if ((*work)(pp->item[i].key, pp->item[i].val, vp)) {
				i++;
			} else {
				int j;

				(*hp->kfree)(pp->item[i].key, hp->arena);
				if (pp->item[i].val) {
					(*hp->vfree)(pp->item[i].val,
					    hp->arena);
				}

				for (j = i; j < pp->count; j++) {
					pp->item[j] = pp->item[j + 1];
				}
				pp->count--;
			}
		}
		(*hp->unlock)(&pp->lock);
	}
	
	return 1;
}

/*
** This routine walks the tree calling the work function on each item.
*/
int
ht_walk(htree_t *hp, void *vp, int (*work)(void *, void *, void *))
{
	if (hp && hp->top && work) {
		return hwalk(hp, hp->top, vp, work);
	}

	return 0;
}

/*
** Called from ht_clear below.  We recursively walk the tree freeing
** everything.
*/
void
hclear(htree_t *hp, hnode_t *np)
{
	if (np) {
		if (np->left) {
			/* left and right are allocated together */
			hclear(hp, np->left);
			hclear(hp, np->right);
			m_free(np->left, hp->arena);
		} else {
			int i;

			for (i = 0; i < np->data->count; i++) {
				(*hp->kfree)(np->data->item[i].key, hp->arena);
				if (np->data->item[i].val) {
					(*hp->vfree)(np->data->item[i].val,
						     hp->arena);
				}
			}
			m_free(np->data, hp->arena);
		}
	}
}

/*
** This function just frees every element in the tree.
*/
void
ht_clear(htree_t *hp)
{
	if (hp && hp->top) {
		hclear(hp, hp->top);
		m_free(hp->top, hp->arena);
		hp->top = 0;
	}
}
