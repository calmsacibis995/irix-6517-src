#ifndef __HTREE_H__
#define __HTREE_H__

/*
** This file contains structure information for the hash tree.
*/

#include <time.h>

typedef struct {
	char *			key;
	int			len;
	struct spwd *		val;
} ht_node_t;

typedef struct {
	int			used;
	uint32_t		hashes[64];
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
int ht_insert(ht_tree_t *, ht_node_t *);
ht_node_t *ht_lookup(ht_tree_t *, char *, size_t);

#endif /* __HTREE_H__ */
