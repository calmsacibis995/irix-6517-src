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

/* read record through position and fix key-position */
/* As ni_rsame but supply a position */

#include "isamdef.h"


	/*
	** If inx >= 0 update index pointer
	** Returns one of the following values:
	**  0 = Ok.
	**  1 = Record deleted
	** -1 = EOF (or something similar. More information in my_errno)
	*/

int ni_rsame_with_pos(N_INFO *info, byte *record, int inx, ulong filepos)
{
  DBUG_ENTER("ni_rsame_with_pos");

  if (inx >= (int) info->s->state.keys || inx < -1)
  {
    my_errno=HA_ERR_WRONG_INDEX;
    DBUG_RETURN(-1);
  }

  info->update&= HA_STATE_CHANGED;
  if ((*info->s->read_rnd)(info,record,filepos,0))
  {
    if (my_errno == HA_ERR_RECORD_DELETED)
    {
      my_errno=HA_ERR_KEY_NOT_FOUND;
      DBUG_RETURN(1);
    }
    DBUG_RETURN(-1);
  }
  info->lastpos=filepos;
  info->lastinx=inx;
  if (inx >= 0)
  {
    VOID(_ni_make_key(info,(uint) inx,info->lastkey,record,info->lastpos));
    info->update|=HA_STATE_KEY_CHANGED;		/* Don't use indexposition */
  }
  DBUG_RETURN(0);
} /* ni_rsame_pos */
