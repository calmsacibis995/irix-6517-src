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

	/* Read first record (in sec-order).
	   Now only supported for inx == -1 */


int heap_rfirst(info,record,inx)
HP_INFO *info;
byte *record;
int inx;					/* Not used yet */
{
  DBUG_ENTER("heap_rfirst");

  info->current_record = (ulong) ~0L;
  info->update=0;
  DBUG_RETURN(heap_rnext(info,record,inx));
}
