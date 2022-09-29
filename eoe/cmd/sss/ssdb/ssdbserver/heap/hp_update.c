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

/* Update current record in heap-database */

#include "heapdef.h"

int heap_update(info,old,new)
HP_INFO *info;
const byte *old;
const byte *new;
{
  uint key;
  byte *pos;
  DBUG_ENTER("heap_update");

  test_active(info);
  pos=info->current_ptr;

  if (info->opt_flag & READ_CHECK_USED && _hp_rectest(info,old))
    DBUG_RETURN(-1);				/* Record changed */
  if (--(info->records) < info->blength >> 1) info->blength>>= 1;
  info->changed=1;

  for (key=0 ; key < info->keys ; key++)
  {
    if (_hp_rec_key_cmp(info->keydef+key,old,new))
    {
      if (_hp_delete_key(info,info->keydef+key,old,pos,key ==
			 (uint) info->lastinx) ||
	  _hp_write_key(info,info->keydef+key,new,pos))
	goto err;
    }
  }

  memcpy(pos,new,(size_t) info->reclength);
  if (++(info->records) == info->blength) info->blength+= info->blength;
  DBUG_RETURN(0);
 err:
  if (my_errno == HA_ERR_FOUND_DUPP_KEY)
  {
    info->errkey=key;
    do
    {
      if (_hp_rec_key_cmp(info->keydef+key,old,new))
      {
	if (_hp_delete_key(info,info->keydef+key,new,pos,0) ||
	    _hp_write_key(info,info->keydef+key,old,pos))
	  break;
      }
    } while (key-- > 0);
  }
  if (++(info->records) == info->blength) info->blength+= info->blength;
  DBUG_RETURN(-1);
} /* heap_update */
