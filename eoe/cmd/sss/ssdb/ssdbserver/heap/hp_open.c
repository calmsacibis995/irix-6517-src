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

/* open a heap-database */

#include "heapdef.h"
#ifdef VMS
#include "hp_static.c"			/* Stupid vms-linker */
#endif

static void init_block(HP_BLOCK *block,uint reclength,ulong min_records,
		       ulong max_records);

	/* open a heap database. */

HP_INFO *heap_open(name,mode,keys,keydef,reclength,max_records,min_records)
const char *name;
int mode;
uint keys,reclength;
HP_KEYDEF *keydef;
ulong max_records,min_records;
{
  uint i,j,key_segs,max_length,length;
  HP_INFO *info;
  HP_KEYSEG *keyseg;
  DBUG_ENTER("heap_open");

  if (!(info=_hp_find_named_heap(name)))
  {
    for (i=key_segs=max_length=0 ; i < keys ; i++)
    {
      key_segs+= keydef[i].keysegs;
      bzero(&keydef[i].block,sizeof(keydef[i].block));
      for (j=length=0 ; j < keydef[i].keysegs; j++)
	length+=keydef[i].seg[j].length;
      keydef[i].length=length;
      if (length > max_length)
	max_length=length;
    }
    if (!(info = (HP_INFO*) my_malloc((uint) sizeof(HP_INFO)+
				      keys*sizeof(HP_KEYDEF)+
				      key_segs*sizeof(HP_KEYSEG)+max_length,
				      MYF(MY_ZEROFILL))))
      DBUG_RETURN(0);
    info->keydef=(HP_KEYDEF*) (info+1);
    keyseg=(HP_KEYSEG*) (info->keydef+keys);
    info->lastkey=(byte*) (keyseg+key_segs);
    init_block(&info->block,reclength+1,min_records,max_records);
	/* Fix keys */
    memcpy(info->keydef,keydef,(size_t) (sizeof(keydef[0])*keys));
    for (i=0 ; i < keys ; i++)
    {
      info->keydef[i].seg=keyseg;
      memcpy(keyseg,keydef[i].seg,
	     (size_t) (sizeof(keyseg[0])*keydef[i].keysegs));
      keyseg+=keydef[i].keysegs;
      init_block(&info->keydef[i].block,sizeof(HASH_INFO),min_records,
		 max_records);
    }

    info->min_records=min_records;
    info->max_records=max_records;
    info->reclength=reclength;
    info->blength=1;
    info->keys=keys;
    info->errkey= -1;
    if (!(info->name=my_strdup(name,MYF(0))))
    {
      my_free((gptr) info,MYF(0));
      DBUG_RETURN(0);
    }
    info->open_list.data=(void*) info;
    pthread_mutex_lock(&THR_LOCK_open);
    heap_open_list=list_add(heap_open_list,&info->open_list);
    pthread_mutex_unlock(&THR_LOCK_open);
  }
  info->mode=mode;
  info->current_record= (ulong) ~0L;		/* No current record */
  info->lastinx= -1;
  info->update=info->changed=0;
#ifndef DBUG_OFF
  info->opt_flag=READ_CHECK_USED;		/* Check when changing */
#endif
  DBUG_PRINT("exit",("heap: %lx  reclength: %d  records_in_block: %d",
		     info,info->reclength,info->block.records_in_block));
  DBUG_RETURN(info);
} /* heap_open */


	/* map name to a heap-nr. If name isn't found return 0 */

HP_INFO *_hp_find_named_heap(name)
const char *name;
{
  LIST *pos;
  HP_INFO *info;
  DBUG_ENTER("heap_find");
  DBUG_PRINT("enter",("name: %s",name));

  pthread_mutex_lock(&THR_LOCK_open);
  for (pos=heap_open_list ; pos ; pos=pos->next)
  {
    info=(HP_INFO*) pos->data;
    if (!strcmp(name,info->name))
    {
      DBUG_PRINT("exit",("Old heap_database: %lx",info));
      pthread_mutex_unlock(&THR_LOCK_open);
      DBUG_RETURN(info);
    }
  }
  pthread_mutex_unlock(&THR_LOCK_open);
  DBUG_RETURN((HP_INFO *)0);
}


static void init_block(block,reclength,min_records,max_records)
HP_BLOCK *block;
uint reclength;
ulong min_records,max_records;
{
  uint i,recbuffer,records_in_block;

  max_records=max(min_records,max_records);
  if (!max_records)
    max_records=1000;			/* As good as quess as anything */
  recbuffer=(uint) (reclength+sizeof(byte**)-1) & ~(sizeof(byte**)-1);
  records_in_block=max_records/10;
  if (records_in_block < 10 && max_records)
    records_in_block=10;
  if (!records_in_block || records_in_block*recbuffer >
      (RECORD_CACHE_SIZE-sizeof(HP_PTRS)*HP_MAX_LEVELS))
    records_in_block=(RECORD_CACHE_SIZE-sizeof(HP_PTRS)*HP_MAX_LEVELS)/
      recbuffer;
  block->records_in_block=records_in_block;
  block->recbuffer=recbuffer;
  block->last_allocated= 0L;

  for (i=0 ; i <= HP_MAX_LEVELS ; i++)
    block->level_info[i].records_under_level=
      (!i ? 1 : i == 1 ? records_in_block :
       HP_PTRS_IN_NOD * block->level_info[i-1].records_under_level);
}
