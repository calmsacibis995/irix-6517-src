/* Copyright (C) 1979-1996 TcX AB & Monty Program KB & Detron HB

   This software is distributed with NO WARRANTY OF ANY KIND.  No author or
   distributor accepts any responsibility for the consequences of using it, or
   for whether it serves any particular purpose or works at all, unless he or
   she says so in writing.  Refer to the Free Public License (the "License")
   for full details.

   Every copy of this file must include a copy of the License, normally in a
   plain ASCII text file named PUBLIC.	The License grants you the right to
   copy, modify and redistribute this file, but only under certain conditions
   described in the License.  Among other things, the License requires that
   the copyright notice and this notice be preserved on all copies. */

/* Dynamic hashing of record with different key-length */

#ifndef _hash_h
#define _hash_h
#ifdef	__cplusplus
extern "C" {
#endif

typedef byte *(*hash_get_key)(const byte *,uint*,my_bool);
typedef void (*hash_free_key)(void *);

  /* flags for hash_init */
#define HASH_CASE_INSENSITIVE	1

typedef struct st_hash {
  uint key_offset,key_length;		/* Length of key if const length */
  uint records,blength,current_record;
  uint flags;
  DYNAMIC_ARRAY array;				/* Place for hash_keys */
  hash_get_key get_key;
  void (*free)(void *);
  uint (*calc_hashnr)(const byte *key,uint length);
} HASH;

my_bool hash_init(HASH *hash,uint default_array_elements, uint key_offset,
		  uint key_length, hash_get_key get_key,
		  void (*free_element)(void*), uint flags);
void hash_free(HASH *tree);
byte *hash_element(HASH *hash,uint index);
gptr hash_search(HASH *info,const byte *key,uint length);
gptr hash_next(HASH *info,const byte *key,uint length);
my_bool hash_insert(HASH *info,const byte *data);
my_bool hash_delete(HASH *hash,byte *record);
my_bool hash_update(HASH *hash,byte *record,byte *old_key,uint old_key_length);
my_bool hash_check(HASH *hash);			/* Only in debug library */

#define hash_clear(H) bzero((char*) (H),sizeof(*(H)))

#ifdef	__cplusplus
}
#endif
#endif
