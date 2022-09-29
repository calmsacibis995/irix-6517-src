#pragma once

#include <sys/types.h>
#include <abi_mutex.h>

#define HT_PAGESIZE	64

typedef struct {
	void			*key;
	void			*val;
	uint32_t		hash;
} helem_t;

typedef struct {
	abilock_t		lock;
	unsigned		count;
	helem_t			item[HT_PAGESIZE];
} hpage_t;

typedef struct hnode {
	struct hnode		*left;
	struct hnode		*right;
	hpage_t			*data;
} hnode_t;

typedef struct {
	int			(*cmp)(void *, void *);
	void			(*kfree)(void *, void *);
	void			(*vfree)(void *, void *);
	uint32_t		(*hash)(void *);
	int			(*lock)(abilock_t *);
	int			(*unlock)(abilock_t *);
	void			*arena;
	hnode_t			*top;
	unsigned		flags;
} htree_t;

/* htree flags */
#define HT_CHECK_DUPS		(1<<0)

/* ht_insert flags */
#define HT_INSERT		0
#define HT_REPLACE		(1<<0)

uint32_t ht_hash(void *, int);
int ht_init(htree_t *, int (*)(void *, void *), void (*)(void *, void *),
    void (*)(void *, void *), uint32_t (*)(void *), int (*)(abilock_t *),
    int (*)(abilock_t *), void *, unsigned);
int ht_insert(htree_t *, void *, void *, unsigned);
void *ht_lookup(htree_t *, void *);
int ht_delete(htree_t *, void *);
int ht_walk(htree_t *, void *, int (*)(void *, void *, void *));
void ht_clear(htree_t *);
