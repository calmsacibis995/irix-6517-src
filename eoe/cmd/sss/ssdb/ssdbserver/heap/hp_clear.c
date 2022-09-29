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
  remove all records from database
  Identical as hp_create() and hp_open() but used HP_INFO* instead of name and
  database remains open.
*/

#include "heapdef.h"

void heap_clear(info)
HP_INFO *info;
{
  uint key;
  DBUG_ENTER("heap_clear");

  if (info->block.levels)
    VOID(_hp_free_level(&info->block,info->block.levels,info->block.root,
			(byte*) 0));
  info->block.levels=0;
  for (key=0 ; key < info->keys ; key++)
  {
    HP_BLOCK *block= &info->keydef[key].block;
    if (block->levels)
      VOID(_hp_free_level(block,block->levels,block->root,(byte*) 0));
    block->levels=0;
    block->last_allocated=0;
  }
  info->current_record= (ulong) ~0L;		/* No current record */
  info->update=0;
  info->records=info->deleted=0;
  DBUG_VOID_RETURN;
}
