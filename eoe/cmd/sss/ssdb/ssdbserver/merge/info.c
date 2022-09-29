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

#include "mrgdef.h"

	/* If flag != 0 one only gets pos of last record */

int mrg_info(MRG_INFO *info,register MERGE_INFO *x,int flag)
{
  MRG_TABLE *current_table;
  DBUG_ENTER("mrg_info");

  if (!(current_table = info->current_table) &&
      info->open_tables != info->end_table)
    current_table = info->open_tables;
  x->recpos  = info->current_table ?
    info->current_table->table->lastpos + info->current_table->file_offset :
      (ulong) -1L;
  if (flag != 1)
  {
    x->records	 = info->records;
    x->deleted	 = info->del;
    x->data_file_length = info->data_file_length;
    x->reclength  = info->reclength;
    if (current_table)
      x->errkey  = current_table->table->errkey;
    else
    {						/* No tables in MRG */
      x->errkey=0;
    }
    x->options	 = info->options;
  }
  DBUG_RETURN(0);
}
