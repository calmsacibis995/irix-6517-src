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

/* remove current record in heap-database */

#include "heapdef.h"

int heap_delete(info,record)
HP_INFO *info;
const byte *record;
{
  uint key;
  byte *pos;

  DBUG_ENTER("heap_delete");
  DBUG_PRINT("enter",("info: %lx  record: %lx",info,record));

  test_active(info);

  if (info->opt_flag & READ_CHECK_USED && _hp_rectest(info,record))
    DBUG_RETURN(-1);				/* Record changed */
  info->changed=1;

  if ( --(info->records) < info->blength >> 1) info->blength>>=1;
  pos=info->current_ptr;

  for (key=0 ; key < info->keys ; key++)
  {
    if (_hp_delete_key(info,info->keydef+key,record,pos,
		       key == (uint) info->lastinx))
      goto err;
  }

  info->update=HA_STATE_DELETED;
  *((byte**) pos)=info->del_link;
  info->del_link=pos;
  pos[info->reclength]=0;		/* Record deleted */
  info->deleted++;
  DBUG_RETURN(0);
 err:
  if( ++(info->records) == info->blength) info->blength+= info->blength;
  DBUG_RETURN(-1);
}

	/* Remove one key from hash-table */
	/* Flag is set if we want's to correkt info->current_ptr */

int _hp_delete_key(info,keyinfo,record,recpos,flag)
HP_INFO *info;
register HP_KEYDEF *keyinfo;
const byte *record;
byte *recpos;
int flag;
{
  ulong blength,pos2,pos_hashnr,lastpos_hashnr;
  byte *last_record;
  HASH_INFO *lastpos,*gpos,*pos,*pos3,*empty;
  DBUG_ENTER("_hp_delete_key");

  blength=info->blength;
  if (info->records+1 == blength) blength+= blength;
  lastpos=hp_find_hash(&keyinfo->block,info->records);
  last_record=0;

  /* Search after record with key */
  pos= hp_find_hash(&keyinfo->block,
		    _hp_mask(_hp_rec_hashnr(keyinfo,record),blength,
			     info->records+1));
  gpos = pos3 = 0;

  while (pos->ptr_to_rec != recpos)
  {
    if (flag && !_hp_rec_key_cmp(keyinfo,record,pos->ptr_to_rec))
      last_record=pos->ptr_to_rec;	/* Previous same key */
    gpos=pos;
    if (!(pos=pos->next_key))
    {
      my_errno=HA_ERR_CRASHED;		/* This shouldn't happend */
      DBUG_RETURN(1);
    }
  }

  /* Remove link to record */

  if (flag)
    info->current_ptr = last_record;	/* Save for h_rnext */
  empty=pos;
  if (gpos)
    gpos->next_key=pos->next_key;	/* unlink current ptr */
  else if (pos->next_key)
  {
    empty=pos->next_key;
    pos->ptr_to_rec=empty->ptr_to_rec;
    pos->next_key=empty->next_key;
  }

  if (empty == lastpos)			/* deleted last hash key */
    DBUG_RETURN (0);

  /* Move the last key (lastpos) */
  lastpos_hashnr=_hp_rec_hashnr(keyinfo,lastpos->ptr_to_rec);
  /* pos is where lastpos should be */
  pos=hp_find_hash(&keyinfo->block,_hp_mask(lastpos_hashnr,info->blength,
					    info->records));
  if (pos == empty)			/* Move to empty position. */
  {
    empty[0]=lastpos[0];
    DBUG_RETURN(0);
  }
  pos_hashnr=_hp_rec_hashnr(keyinfo,pos->ptr_to_rec);
  /* pos3 is where the pos should be */
  pos3= hp_find_hash(&keyinfo->block,
		     _hp_mask(pos_hashnr,info->blength,info->records));
  if (pos != pos3)
  {					/* pos is on wrong posit */
    empty[0]=pos[0];			/* Save it here */
    pos[0]=lastpos[0];			/* This shold be here */
    _hp_movelink(pos,pos3,empty);	/* Fix link to pos */
    DBUG_RETURN(0);
  }
  pos2= _hp_mask(lastpos_hashnr,blength,info->records+1);
  if (pos2 == _hp_mask(pos_hashnr,blength,info->records+1))
  {					/* Identical key-positions */
    if (pos2 != info->records)
    {
      empty[0]=lastpos[0];
      _hp_movelink(lastpos,pos,empty);
      DBUG_RETURN(0);
    }
    pos3= pos;				/* Link pos->next after lastpos */
  }
  else pos3= 0;				/* Different positions merge */

  empty[0]=lastpos[0];
  _hp_movelink(pos3,empty,pos->next_key);
  pos->next_key=empty;
  DBUG_RETURN(0);
}
