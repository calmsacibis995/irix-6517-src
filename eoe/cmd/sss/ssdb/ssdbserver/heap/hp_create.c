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

/*
  Create is done by simply remove the database from memory if it exists.
  Open creates the database when neaded
*/

#include "heapdef.h"


int heap_create(name)
const char *name;
{
  reg1 HP_INFO *info;
  DBUG_ENTER("heap_create");
  if ((info=_hp_find_named_heap(name)))
  {
    pthread_mutex_lock(&THR_LOCK_open);
    _hp_free(info);
    pthread_mutex_unlock(&THR_LOCK_open);
  }
  DBUG_RETURN(0);
}

void _hp_free(info)
HP_INFO *info;
{
  heap_open_list=list_delete(heap_open_list,&info->open_list);
  heap_clear(info);			/* Remove blocks from memory */
  my_free((gptr) info->name,MYF(0));
  my_free((gptr) info,MYF(0));
  return;
}
