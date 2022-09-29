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

	/* Read prev record (in sec-order). Now only supported for inx == -1 */


int heap_rprev(info,record,inx)
HP_INFO *info;
byte *record;
int inx;					/* Not used yet */
{
  ulong pos;
  DBUG_ENTER("heap_rprev");

  if (inx != -1)
  {
    my_errno=HA_ERR_END_OF_FILE;
    DBUG_RETURN(-1);
  }
  if (info->current_record != (ulong) ~0L)
  {
    while ((pos= --info->current_record) != (ulong) -1)
    {
      if ((pos+1) % info->block.records_in_block &&
	  info->update & HA_STATE_NEXT_FOUND)
	info->current_ptr-=info->block.recbuffer;
      else
	_hp_find_record(info,pos);
      if (info->current_ptr[info->reclength])
      {
	info->update= HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND |
	  HA_STATE_AKTIV;
	memcpy(record,info->current_ptr,(size_t) info->reclength);
	DBUG_RETURN(0);
      }
      info->update= HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND;
    }
  }
  info->update= 0;
  my_errno= HA_ERR_END_OF_FILE;
  DBUG_RETURN(-1);
}
