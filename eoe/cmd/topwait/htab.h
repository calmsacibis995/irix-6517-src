#ifndef __HTAB_H__
#define __HTAB_H__

#include <stddef.h>

#ifndef baseof
#define baseof(type, field, fieldp) \
	(type *)(void *)((char *)(fieldp) - offsetof(type, field))
#endif

typedef struct hashable_s {
	struct hashable_s *ha_next, *ha_prev;
	uint64_t ha_val;
} hashable;

typedef struct htab_s {
	uint64_t ht_nbuckets;
	hashable **ht_buckets;
} htab_t;

void htab_init(htab_t *htab, int nbuckets);
void htab_insert(htab_t *htab, uint64_t hval, uint64_t val, hashable *obj);
hashable *htab_find(htab_t *htab, uint64_t hval, uint64_t val);
void htab_remove(htab_t *htab, uint64_t hval, hashable *obj);
hashable *htab_begin(htab_t *htab);
hashable *htab_next(htab_t *htab, uint64_t hval, hashable *obj);
void htab_dump(htab_t *htab, FILE *fp);

#endif /* __HTAB_H__ */
