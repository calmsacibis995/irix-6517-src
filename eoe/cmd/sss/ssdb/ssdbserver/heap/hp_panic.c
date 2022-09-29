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

	/* if flag == HA_PANIC_CLOSE then all files are removed for more
	   memory */

int heap_panic(flag)
enum ha_panic_function flag;
{
  reg2 HP_INFO *info;
  LIST *element,*next_open;
  DBUG_ENTER("heap_panic");

  pthread_mutex_lock(&THR_LOCK_open);
  for (element=heap_open_list ; element ; element=next_open)
  {
    next_open=element->next;	/* Save if close */
    info=(HP_INFO*) element->data;
    switch (flag) {
    case HA_PANIC_CLOSE:
      _hp_free(info);
      break;
    default:
      break;
    }
  }
  pthread_mutex_unlock(&THR_LOCK_open);
  DBUG_RETURN(0);
} /* heap_panic */
