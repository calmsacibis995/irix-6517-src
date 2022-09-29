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
  Lock databases against read or write.
*/

#include "mrgdef.h"

int mrg_lock_database(info,lock_type)
MRG_INFO *info;
int lock_type;
{
  int error,new_error;
  MRG_TABLE *file;

  error=0;
  for (file=info->open_tables ; file != info->end_table ; file++)
    if ((new_error=ni_lock_database(file->table,lock_type)))
      error=new_error;
  return(error);
}
