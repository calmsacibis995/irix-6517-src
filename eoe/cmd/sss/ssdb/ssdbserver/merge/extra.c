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
  Extra functions we want to do with a database
  - All flags, exept record-cache-flags, are set in all used databases
    record-cache-flags are set in mrg_rrnd when we are changing database.
*/

#include "mrgdef.h"

int mrg_extra(info,function)
MRG_INFO *info;
enum ha_extra_function function;
{
  MRG_TABLE *file;

  if (function == HA_EXTRA_CACHE)
    info->cache_in_use=1;
  else
  {
    if (function == HA_EXTRA_NO_CACHE)
      info->cache_in_use=0;
    if (function == HA_EXTRA_RESET)
    {
      info->current_table=0;
      info->last_used_table=info->open_tables;
    }
    for (file=info->open_tables ; file != info->end_table ; file++)
      ni_extra(file->table,function);
  }
  return 0;
}
