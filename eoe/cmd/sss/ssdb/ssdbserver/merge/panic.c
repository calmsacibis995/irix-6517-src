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

	/* if flag == HA_PANIC_CLOSE then all misam files are closed */
	/* if flag == HA_PANIC_WRITE then all misam files are unlocked and
	   all changed data in single user misam is written to file */
	/* if flag == HA_PANIC_READ then all misam files that was locked when
	   ni_panic(HA_PANIC_WRITE) was done is locked. A ni_readinfo() is
	   done for all single user files to get changes in database */


int mrg_panic(flag)
enum ha_panic_function flag;
{
  int error=0;
  LIST *list_element,*next_open;
  MRG_INFO *info;
  DBUG_ENTER("mrg_panic");

  for (list_element=mrg_open_list ; list_element ; list_element=next_open)
  {
    next_open=list_element->next;		/* Save if close */
    info=(MRG_INFO*) list_element->data;
    if (flag == HA_PANIC_CLOSE && mrg_close(info))
      error=my_errno;
  }
  if (mrg_open_list && flag != HA_PANIC_CLOSE)
    DBUG_RETURN(ni_panic(flag));
  if (!error) DBUG_RETURN(0);
  my_errno=error;
  DBUG_RETURN(-1);
}
