#ifndef INDEX_H
#define INDEX_H
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Associations between fixed-size keys and opaque values.
 */
#define	index	netman_index	/* avoid string.h function name clash */

struct __xdr_s;

/*
 * Index entry structure.
 */
typedef struct entrylist {
	struct entry	*el_next;	/* next element in list */
	struct entry	*el_prev;	/* and previous */
} EntryList;

typedef struct entry {
	EntryList	ent_list;	/* LRU linkage (MUST BE FIRST) */
	struct entry	*ent_next;	/* hash table linkage */
	void		*ent_value;	/* value pointer */
	long		ent_qual;	/* optional key qualifier */
	char		ent_key[4];	/* actually, 4 or more */
} Entry;

/*
 * An index is an open hash table using a strength-reduced division hash.
 */
typedef struct index {
	unsigned int		in_shift;	/* log2(# of hash buckets) */
	unsigned int		in_hashmask;	/* hash mask based on shift */
	unsigned int		in_buckets;	/* number of hash buckets */
	unsigned int		in_keysize;	/* byte size of entry key */
	Entry			**in_hash;	/* base of hash buckets */
	struct indexops		*in_ops;	/* hash table operators */
#ifdef METERING
	struct indexmeter {
		unsigned long	im_searches;	/* hash table searches */
		unsigned long	im_probes;	/* linear probes on collision */
		unsigned long	im_hits;	/* lookups which found name */
		unsigned long	im_misses;	/* names not found */
		unsigned long	im_enters;	/* entries added */
		unsigned long	im_recycles;	/* cache entries recycled */
		unsigned long	im_removes;	/* entries deleted */
		unsigned long	im_flushes;	/* cache flushes */
	} in_meter;
#endif
} Index;

/*
 * Virtual index operations.  The ino_xdrvalue member may be null.
 */
struct indexops {
	unsigned int	(*ino_hashkey)();	/* hash a key to an integer */
	int		(*ino_cmpkeys)();	/* compare two keys a la bcmp */
	int		(*ino_xdrvalue)();	/* get/put/free a value */
};

/*
 * XXX carefully duplicated, typedef-free xdr_wrapstring prototype, so that
 *     we need not include <rpc/xdr.h>.
 */
int	xdr_wrapstring(struct __xdr_s *, char **);

#define	in_hashkey(in, key) \
	((*(in)->in_ops->ino_hashkey)(key, (in)->in_keysize))
#define	in_cmpkeys(in, key1, key2) \
	((*(in)->in_ops->ino_cmpkeys)(key1, key2, (in)->in_keysize))
#define	in_xdrvalue(in, xdr, valp, key) \
	(((in)->in_ops->ino_xdrvalue == 0) ? 1 \
	 : (*(in)->in_ops->ino_xdrvalue)(xdr, valp, key))

Index	*index(int, int, struct indexops *);
void	in_destroy(Index *);
void	in_create(int, int, struct indexops *, Index **);

void	in_init(Index *, int, int, struct indexops *);
void	in_finish(Index *);
void	in_freeval(Index *, Entry *);

Entry	**in_lookup(Index *, void *);
Entry	*in_add(Index *, void *, void *, Entry **);
void	in_delete(Index *, Entry **);

void	*in_match(Index *, void *);
Entry	*in_enter(Index *, void *, void *);
void	in_remove(Index *, void *);

void	*in_matchqual(Index *, void *, long);
void	*in_matchuniq(Index *, void *, long);
void	in_enterqual(Index *, void *, void *, long);
void	in_removequal(Index *, void *, long);

#endif
