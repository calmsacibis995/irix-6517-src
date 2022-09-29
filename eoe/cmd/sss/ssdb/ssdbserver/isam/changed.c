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

/* Check if somebody has changed table since last check. */

#include "isamdef.h"

       /* Return 0 if table isn't changed */

int ni_is_changed(N_INFO *info)
{
  int result;
  DBUG_ENTER("ni_is_changed");
#ifndef NO_LOCKING
  if (_ni_readinfo(info,F_RDLCK,1)) DBUG_RETURN(-1);
  VOID(_ni_writeinfo(info,0));
#endif
  result=(int) info->data_changed;
  info->data_changed=0;
  DBUG_PRINT("exit",("result: %d",result));
  DBUG_RETURN(result);
}
