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

/* close a heap-database */

#include "heapdef.h"

	/* Close a database open by hp_open() */
	/* Data is not deallocated */

int heap_close(info)
register HP_INFO *info;
{
  DBUG_ENTER("heap_close");
#ifndef DBUG_OFF
  if (info->changed && heap_check_heap(info))
  {
    info->changed=info->update=0;
    my_errno=HA_ERR_CRASHED;
    DBUG_RETURN(1);
  }
#endif
  info->changed=info->update=0;
  DBUG_RETURN(0);
} /* heap_close */
