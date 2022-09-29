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

/* L{ser p} basen av en isam_nyckel */

#include "isamdef.h"


	/* Read a record using key */
	/* Ordinary search_flag is 0 ; Give error if no record with key */

int ni_rkey(N_INFO *info, byte *buf, int inx, const byte *key, uint key_len, enum ha_rkey_function search_flag)
{
  uchar *key_buff;
  ISAM_SHARE *share=info->s;
  DBUG_ENTER("ni_rkey");
  DBUG_PRINT("enter",("base: %lx  inx: %d  search_flag: %d",
		      info,inx,search_flag));

  if ((inx = _ni_check_index(info,inx)) < 0)
    DBUG_RETURN(-1);
  DBUG_EXECUTE("key",_ni_print_key(DBUG_FILE,share->keyinfo[inx].seg,
				    (uchar*) key););

  info->update&= HA_STATE_CHANGED;
  if (key_len >= (share->keyinfo[inx].base.keylength - share->rec_reflength)
      && !(info->s->keyinfo[inx].base.flag & HA_SPACE_PACK_USED))
    key_len=USE_HOLE_KEY;
  key_buff=info->lastkey+info->s->base.max_key_length;
  key_len=_ni_pack_key(info,(uint) inx,key_buff,(uchar*) key,key_len);

#ifndef NO_LOCKING
  if (_ni_readinfo(info,F_RDLCK,1))
    goto err;
#endif

  VOID(_ni_search(info,info->s->keyinfo+inx,key_buff,key_len,
		  nisam_read_vec[search_flag],info->s->state.key_root[inx]));
  if ((*info->read_record)(info,info->lastpos,buf) >= 0)
  {
    info->update|= HA_STATE_AKTIV;		/* Record is read */
    DBUG_RETURN(0);
  }

  info->lastpos = NI_POS_ERROR;			/* Didn't find key */
  VOID(_ni_move_key(info->s->keyinfo+inx,info->lastkey,key_buff));
  if (search_flag == HA_READ_AFTER_KEY)
    info->update|=HA_STATE_NEXT_FOUND;		/* Previous gives last row */
err:
  DBUG_RETURN(-1);
} /* ni_rkey */
