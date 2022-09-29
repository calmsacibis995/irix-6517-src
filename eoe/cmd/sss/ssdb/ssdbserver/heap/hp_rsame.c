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

/* re-read current record */

#include "heapdef.h"

	/* If inx != -1 the new record is read according to index
	   (for next/prev). Record must in this case point to last record
	   Returncodes:
	   0 = Ok.
	   1 = Record was removed
	   -1 = Record not found with key
	*/

int heap_rsame(info,record,inx)
reg1 HP_INFO *info;
byte *record;
int inx;					/* Not used yet */
{
  DBUG_ENTER("heap_rsame");

  test_active(info);
  if (info->current_ptr[info->reclength])
  {
    if (inx < -1 || inx >= (int) info->keys)
    {
      my_errno=HA_ERR_WRONG_INDEX;
      DBUG_RETURN(-1);
    }
    else if (inx != -1)
    {
      info->lastinx=inx;
      _hp_make_key(info->keydef+inx,info->lastkey,record);
      if (!_hp_search(info,info->keydef+inx,info->lastkey,3))
      {
	info->update=0;
	DBUG_RETURN(-1);
      }
    }
    memcpy(record,info->current_ptr,(size_t) info->reclength);
    DBUG_RETURN(0);
  }
  info->update=0;

  my_errno=HA_ERR_RECORD_DELETED;			/* record deleted */
  DBUG_RETURN(1);
}
