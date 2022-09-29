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

/* Returns info about database status */

#include "heapdef.h"

	/* If flag == 1 one only gets pos of last record */
	/* if flag == 2 one get current info (no sync from database */

int heap_info(info,x,flag)
reg1 HP_INFO *info;
reg2 HEAPINFO *x;
int flag;
{
  DBUG_ENTER("heap_info");

  x->current_record=(info->update & HA_STATE_AKTIV) ? info->current_record :
    (ulong) ~0L;
  if (flag != 1)
  {
    x->records	= info->records;
    x->deleted	= info->deleted;
    x->reclength= info->reclength;
    x->errkey	= info->errkey;
  }
  DBUG_RETURN(0);
} /* heap_info */
