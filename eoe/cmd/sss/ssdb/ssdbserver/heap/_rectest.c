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

/* Test if a record has changed since last read */
/* In heap this is only used when debugging */

#include "heapdef.h"

int _hp_rectest(info,old)
register HP_INFO *info;
register const byte *old;
{
  DBUG_ENTER("_hp_rectest");

  if (memcmp(info->current_ptr,old,(size_t) info->reclength))
  {
    my_errno=HA_ERR_RECORD_CHANGED; /* Record have changed */
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
} /* _heap_rectest */
