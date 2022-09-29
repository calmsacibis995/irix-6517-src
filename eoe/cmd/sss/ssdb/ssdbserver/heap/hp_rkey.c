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

#include "heapdef.h"

int heap_rkey(info,record,inx,key)
HP_INFO *info;
byte *record;
int inx;
const byte *key;
{
  byte *pos;
  DBUG_ENTER("hp_rkey");
  DBUG_PRINT("enter",("base: %lx  inx: %d  key: '%s'",info,inx,key));

  if ((uint) inx >= info->keys)
  {
    my_errno=HA_ERR_WRONG_INDEX;
    DBUG_RETURN(-1);
  }
  info->lastinx=inx;
  info->current_record = (ulong) ~0L;

  if (!(pos=_hp_search(info,info->keydef+inx,key,0)))
  {
    info->update=0;
    DBUG_RETURN(-1);
  }
  memcpy(record,pos,(size_t) info->reclength);
  info->update=HA_STATE_AKTIV;
  if (!(info->keydef->flag & HA_NOSAME))
    memcpy(info->lastkey,key,(size_t) info->keydef[inx].length);
  DBUG_RETURN(0);
}


	/* Quick find of record */

gptr heap_find(info,inx,key)
HP_INFO *info;
int inx;
const byte *key;
{
  return _hp_search(info,info->keydef+inx,key,0);
}
