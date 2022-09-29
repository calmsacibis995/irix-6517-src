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

/* Read a record from any position, or next record */

#include "heapdef.h"

/*
	   IF pos == -1 read next record
	   Returns one of following values:
	   0 = Ok.
	   1 = Record is deleted.
	  -1 = EOF.
*/

int heap_rrnd(info,record,pos)
reg1 HP_INFO *info;
byte *record;
ulong pos;
{
  DBUG_ENTER("heap_rrnd");
  DBUG_PRINT("enter",("info: %lx  pos: %ld",info,pos));

  info->lastinx= -1;
  if (pos == (ulong) -1)
  {
    pos= ++info->current_record;
    if (pos % info->block.records_in_block &&	/* Quick next record */
	pos < info->records+info->deleted &&
	(info->update & HA_STATE_PREV_FOUND))
    {
      info->current_ptr+=info->block.recbuffer;
      goto end;
    }
  }
  else
    info->current_record=pos;

  if (pos >= info->records+info->deleted)
  {
    my_errno= HA_ERR_END_OF_FILE;
    info->update= 0;
    DBUG_RETURN(-1);
  }

	/* Find record number pos */
  _hp_find_record(info,pos);
end:
  info->update= HA_STATE_PREV_FOUND | HA_STATE_NEXT_FOUND;
  if (!info->current_ptr[info->reclength])
    DBUG_RETURN(1);					/* Record deleted */
  info->update|=HA_STATE_AKTIV;
  memcpy(record,info->current_ptr,(size_t) info->reclength);
  DBUG_PRINT("exit",("found record at %lx",info->current_ptr));
  DBUG_RETURN(0);
} /* heap_rrnd */


	/* Find pos for record and update it in info->current_ptr */

void _hp_find_record(info,pos)
HP_INFO *info;
ulong pos;
{
  info->current_ptr= _hp_find_block(&info->block,pos);
}
