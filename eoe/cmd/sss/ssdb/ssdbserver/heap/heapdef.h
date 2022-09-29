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

/* This file is included in all heap-files */

#include <my_base.h>			/* This includes global */
#ifdef THREAD
#include <my_pthread.h>
#endif
#include "heap.h"			/* Structs & some defines */

	/* Some extern variables */

extern LIST *heap_open_list;

#define test_active(info) \
if (!(info->update & HA_STATE_AKTIV))\
{ errno=HA_ERR_NO_ACTIVE_RECORD; DBUG_RETURN(-1); }
#define hp_find_hash(A,B) ((HASH_INFO*) _hp_find_block((A),(B)))

typedef struct st_hash_info
{
  struct st_hash_info *next_key;
  byte *ptr_to_rec;
} HASH_INFO;

	/* Prototypes for intern functions */

extern HP_INFO *_hp_find_named_heap(const char *name);
extern int _hp_rectest(HP_INFO *info,const byte *old);
extern void _hp_delete_file_from_open_list(HP_INFO *info);
extern void _hp_find_record(HP_INFO *info,ulong pos);
extern byte *_hp_find_block(HP_BLOCK *info,ulong pos);
extern int _hp_get_new_block(HP_BLOCK *info);
extern void _hp_free(HP_INFO *info);
extern byte *_hp_free_level(HP_BLOCK *block,uint level,HP_PTRS *pos,
				byte *last_pos);
extern int _hp_write_key(HP_INFO *info,HP_KEYDEF *keyinfo,
			     const byte *record,byte *recpos);
extern int _hp_delete_key(HP_INFO *info,HP_KEYDEF *keyinfo,
			      const byte *record,byte *recpos,int flag);
extern HASH_INFO *_heap_find_hash(HP_BLOCK *block,ulong pos);
extern byte *_hp_search(HP_INFO *info,HP_KEYDEF *keyinfo,const byte *key,
			    uint nextflag);
extern ulong _hp_hashnr(HP_KEYDEF *keyinfo,const byte *key);
extern ulong _hp_rec_hashnr(HP_KEYDEF *keyinfo,const byte *rec);
extern ulong _hp_mask(ulong hashnr,ulong buffmax,ulong maxlength);
extern void _hp_movelink(HASH_INFO *pos,HASH_INFO *next_link,
			     HASH_INFO *newlink);
extern int _hp_rec_key_cmp(HP_KEYDEF *keydef,const byte *rec1,
			       const byte *rec2);
extern int _hp_key_cmp(HP_KEYDEF *keydef,const byte *rec,
			   const byte *key);
extern void _hp_make_key(HP_KEYDEF *keydef,byte *key,const byte *rec);

#ifdef THREAD
extern pthread_mutex_t THR_LOCK_open;
#else
#define pthread_mutex_lock(A)
#define pthread_mutex_unlock(A)
#endif
