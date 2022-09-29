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

/* close a isam-database */

#include "mrgdef.h"

int mrg_close(info)
register MRG_INFO *info;
{
  int error=0,new_error;
  MRG_TABLE *file;
  DBUG_ENTER("mrg_close");

  for (file=info->open_tables ; file != info->end_table ; file++)
    if ((new_error=ni_close(file->table)))
      error=new_error;
  pthread_mutex_lock(&THR_LOCK_open);
  mrg_open_list=list_delete(mrg_open_list,&info->open_list);
  pthread_mutex_unlock(&THR_LOCK_open);
  my_free((gptr) info,MYF(0));
  if (error)
  {
    my_errno=error;
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}
