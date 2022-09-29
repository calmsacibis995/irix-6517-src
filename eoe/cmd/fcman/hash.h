#ifndef _HASH_H_
#define _HASH_H_

typedef unsigned long long ht_key_t;
typedef void              *ht_val_t;

typedef struct {
  ht_key_t hte_key;
  ht_val_t hte_val;
  void *hte_next, *hte_prev;
} ht_elem_struct_t;
typedef ht_elem_struct_t *ht_elem_t;

typedef struct {
  uint        ht_size;
  ht_elem_t  *ht;
  int       (*ht_hfunc)(ht_key_t);            /* Hash function */
  int       (*ht_cfunc)(ht_key_t, ht_key_t);  /* Compare function */
  char     *(*ht_ksfunc)(ht_key_t);           /* Key print function */
  char     *(*ht_vsfunc)(ht_val_t);           /* Value print function */
} ht_struct_t;

typedef ht_struct_t *ht_t;

ht_t hash_new(int size,
	      int (*hfunc)(ht_key_t), int (*cfunc)(ht_key_t, ht_key_t),
	      char *(*ht_ksfunc)(ht_key_t), char *(*ht_vsfunc)(ht_val_t));
void hash_free(ht_t ht);
int hash_enter(ht_t ht, ht_key_t key, ht_val_t val);
ht_val_t hash_lookup(ht_t ht, ht_key_t key);
void hash_dump(ht_t ht);

#endif

