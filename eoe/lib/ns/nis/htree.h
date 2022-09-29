/*
** This file contains structure information for the hash tree.
*/

#ifndef __HTREE_H__
#define __HTREE_H__

#include <time.h>
#include "dstring.h"

typedef struct {
	uint32_t		hash;
	dstring_t		key;
	dstring_t		val;
} ht_node_t;

typedef struct {
	int			used;
	ht_node_t		*nodes[64];
} ht_page_t;

typedef struct ht_branch {
	struct ht_branch	*left;
	struct ht_branch	*right;
	ht_page_t		*data;
} ht_branch_t;

typedef struct {
	ht_branch_t		*top;
} ht_tree_t;

#define HT_PAGESIZE	64
#define HT_HASHSIZE	32

void ht_init(ht_tree_t *);
void ht_clear(ht_tree_t *);
int ht_insert(ht_tree_t *, ht_node_t *,
    int (*)(const char *, const char *, size_t));
ht_node_t *ht_lookup(ht_tree_t *, char *, size_t,
    int (*)(const char *, const char *, size_t));
void ht_walk(ht_tree_t *, void *, void (*)(void *, ht_node_t *));

#endif /* __HTREE_H__ */
