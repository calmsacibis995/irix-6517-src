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
  Rename name of database
*/

#include "heapdef.h"

int heap_rename(old_name,new_name)
const char *old_name;
const char *new_name;
{
  reg1 HP_INFO *info;
  char *name_buff;
  DBUG_ENTER("heap_rename");

  if ((info=_hp_find_named_heap(old_name)))
  {
    if (!(name_buff=(char*) my_strdup(new_name,MYF(MY_WME))))
      DBUG_RETURN(-1);
    my_free(info->name,MYF(0));
    info->name=name_buff;
  }
  DBUG_RETURN(0);
}
