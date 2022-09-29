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

/* L{ser n{sta post med samma isam-nyckel */

#include "isamdef.h"

	/*
	   L{ser n{sta post med samma isamnyckel som f|reg}ende l{sning.
	   Man kan ha gjort write, update eller delete p} f|reg}ende post.
	   OBS! [ven om man {ndrade isamnyckeln p} f|reg}ende post l{ses
	   posten i avseende p} f|reg}ende isam-nyckel-l{sning !!
	*/

int ni_rnext(N_INFO *info, byte *buf, int inx)
{
  int error;
  uint flag;
  DBUG_ENTER("ni_rnext");

  if ((inx = _ni_check_index(info,inx)) < 0)
    DBUG_RETURN(-1);
  flag=SEARCH_BIGGER;				/* Read next */
  if (info->lastpos == NI_POS_ERROR && info->update & HA_STATE_PREV_FOUND)
    flag=0;					/* Read first */

#ifndef NO_LOCKING
  if (_ni_readinfo(info,F_RDLCK,1)) DBUG_RETURN(-1);
#endif
  if (!flag)
    error=_ni_search_first(info,info->s->keyinfo+inx,
			   info->s->state.key_root[inx]);
  else if (_ni_test_if_changed(info) == 0)
    error=_ni_search_next(info,info->s->keyinfo+inx,info->lastkey,flag,
			  info->s->state.key_root[inx]);
  else
    error=_ni_search(info,info->s->keyinfo+inx,info->lastkey,0,flag,
		     info->s->state.key_root[inx]);

	/* Don't clear if database-changed */
  info->update&= (HA_STATE_CHANGED | HA_STATE_BUFF_SAVED);
  info->update|= HA_STATE_NEXT_FOUND;

  if (error && my_errno == HA_ERR_KEY_NOT_FOUND)
    my_errno=HA_ERR_END_OF_FILE;
  if ((*info->read_record)(info,info->lastpos,buf) >=0)
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    DBUG_RETURN(0);
  }
  DBUG_RETURN(-1);
} /* ni_rnext */
