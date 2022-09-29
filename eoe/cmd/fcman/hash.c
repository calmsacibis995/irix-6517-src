#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <fcntl.h>		/* for I/O functions */
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "hash.h"
#include "debug.h"

ht_t hash_new(int     size,
	      int   (*hfunc)(ht_key_t), 
	      int   (*cfunc)(ht_key_t, ht_key_t),
	      char *(*ksfunc)(ht_key_t), 
	      char *(*vsfunc)(ht_val_t))
{
  ht_t ht;

  DBG(D_HASH, "hash_new() : size = %d\n", size);

  ht = (ht_t)malloc(sizeof(ht_struct_t));
  ht->ht_size = size;
  ht->ht = (ht_elem_t *)calloc(ht->ht_size, sizeof(ht_elem_t));
  ht->ht_hfunc = hfunc;
  ht->ht_cfunc = cfunc;
  ht->ht_ksfunc = ksfunc;
  ht->ht_vsfunc = vsfunc;

  return(ht);
}

void hash_free(ht_t ht)
{
  int i;
  ht_elem_t hte, hte_trash;

  DBG(D_HASH, "hash_free() : \n");

  for (i = 0; i < ht->ht_size; ++i) {
    if (hte = ht->ht[i]) {
      for (; hte; ) {
	hte_trash = hte;
	hte = hte->hte_next;
	free(hte_trash);
      }
    }
  }
  free(ht->ht);
  free(ht);
}

int hash_enter(ht_t ht, ht_key_t key, ht_val_t val)
{
  ht_elem_t hte_new, hte_p;
  int hash;

  DBG(D_HASH, "hash_enter() : key = %s : val = %s\n", 
      (*(ht->ht_ksfunc))(key),
      (*(ht->ht_vsfunc))(val));

  hte_new = (ht_elem_t)malloc(sizeof(ht_elem_struct_t));
  hte_new->hte_key = key;
  hte_new->hte_val = val;
  hte_new->hte_prev = hte_new->hte_next = NULL;

  hash = (*(ht->ht_hfunc))(key) % ht->ht_size;

  if (ht->ht[hash] == NULL) {
    ht->ht[hash] = hte_new;
    return(0);
  }

  hte_p = ht->ht[hash];
  while ((hte_p->hte_next != NULL) && ((*(ht->ht_cfunc))(key, hte_p->hte_key) > 0))
    hte_p = hte_p->hte_next;

  /* Is it the first ? */
  if (((*(ht->ht_cfunc))(key, hte_p->hte_key) < 0) && (hte_p->hte_prev == NULL)) {
    hte_p->hte_prev = hte_new;
    hte_new->hte_next = hte_p;
    ht->ht[hash] = hte_new;
    return(0);
  }

  /* Is it the last ? */
  if (((*(ht->ht_cfunc))(key, hte_p->hte_key) > 0) && (hte_p->hte_next == NULL)) {  
    hte_p->hte_next = hte_new;
    hte_new->hte_prev = hte_p;
    return(0);
  }
    
  /* Is it somewhere in the middle */
  if ((*(ht->ht_cfunc))(key, hte_p->hte_key) < 0) {
    hte_new->hte_next = hte_p;
    hte_new->hte_prev = hte_p->hte_prev;
    ((ht_elem_t)(hte_p->hte_prev))->hte_next = hte_new;
    hte_p->hte_prev = hte_new;
    return(0);
  }

  /* Otherwise it's already here */
  return(-1);
}    
  
ht_val_t hash_lookup(ht_t ht, ht_key_t key)
{
  int hash;
  ht_elem_t hte;
  ht_val_t val = NULL;

  DBG(D_HASH, "hash_lookup() : key = %s --> ", (*(ht->ht_ksfunc))(key));

  hash = (*(ht->ht_hfunc))(key) % ht->ht_size;

  for (hte = ht->ht[hash]; hte; hte = hte->hte_next) {
    if ((*(ht->ht_cfunc))(key, hte->hte_key) == 0) {
      val = hte->hte_val;
      break;
    }
  }

  DBG(D_HASH, "val = %s\n", (*(ht->ht_vsfunc))(val));

  return(val);
}

void hash_dump(ht_t ht)
{
  int i;
  ht_elem_t hte;

  printf("hash_dump() : \n");
  for (i = 0; i < ht->ht_size; ++i) {
    printf(" [%4d] : \n", i);
    if (ht->ht[i])
      for (hte = ht->ht[i]; hte; hte = hte->hte_next) 
	printf("        : %16s = %s\n", 
	       (*(ht->ht_ksfunc))(hte->hte_key),
	       (*(ht->ht_vsfunc))(hte->hte_val));
  }
}


