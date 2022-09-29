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

/* Write a record to heap-databas */

#include "heapdef.h"
#ifdef __WIN32__
#include <fcntl.h>
#endif

#define LOWFIND 1
#define LOWUSED 2
#define HIGHFIND 4
#define HIGHUSED 8

static byte *next_free_record_pos(HP_INFO *info);
static HASH_INFO *_hp_find_free_hash(HP_BLOCK *block,ulong records);

int heap_write(info,record)
HP_INFO *info;
const byte *record;
{
  uint key;
  byte *pos;
  DBUG_ENTER("heap_write");

#ifndef DBUG_OFF
  if (info->mode && O_RDONLY)
  {
    my_errno=EACCES;
    DBUG_RETURN(-1);
  }
#endif
  if (!(pos=next_free_record_pos(info)))
    DBUG_RETURN(-1);
  info->changed=1;

  for (key=0 ; key < info->keys ; key++)
  {
    if (_hp_write_key(info,info->keydef+key,record,pos))
      goto err;
  }

  memcpy(pos,record,(size_t) info->reclength);
  pos[info->reclength]=1;		/* Mark record as not deleted */
  if (++info->records == info->blength)
    info->blength+= info->blength;
  info->current_ptr=pos;
  info->update|=HA_STATE_AKTIV;
  DBUG_RETURN(0);
err:
  info->errkey= key;
  do
  {
    if (_hp_delete_key(info,info->keydef+key,record,pos,0))
      break;
  } while (key-- > 0);

  *((byte**) pos)=info->del_link;
  info->del_link=pos;
  pos[info->reclength]=0;			/* Record deleted */
  DBUG_RETURN(-1);
} /* heap_write */


	/* Find where to place new record */

static byte *next_free_record_pos(info)
HP_INFO *info;
{
  int block_pos;
  byte *pos;
  DBUG_ENTER("next_free_record_pos");

  if (info->del_link)
  {
    pos=info->del_link;
    info->del_link= *((byte**) pos);
    DBUG_PRINT("exit",("Used old position: %lx",pos));
    DBUG_RETURN(pos);
  }
  if (!(block_pos=(info->records % info->block.records_in_block)))
  {
    if (info->records > info->max_records && info->max_records)
    {
      my_errno=HA_ERR_RECORD_FILE_FULL;
      DBUG_RETURN(NULL);
    }
    if (_hp_get_new_block(&info->block))
      DBUG_RETURN(NULL);
  }
  DBUG_PRINT("exit",("Used new position: %lx",
		     (byte*) info->block.level_info[0].last_blocks+block_pos*
		     info->block.recbuffer));
  DBUG_RETURN((byte*) info->block.level_info[0].last_blocks+
	      block_pos*info->block.recbuffer);
}


	/* Write a hash-key to the hash-index */

int _hp_write_key(info,keyinfo,record,recpos)
register HP_INFO *info;
HP_KEYDEF *keyinfo;
const byte *record;
byte *recpos;
{
  int flag;
  ulong halfbuff,hashnr,first_index;
  byte *ptr_to_rec,*ptr_to_rec2;
  HASH_INFO *empty,*gpos,*gpos2,*pos;
  DBUG_ENTER("hp_write_key");

  LINT_INIT(gpos); LINT_INIT(gpos2);
  LINT_INIT(ptr_to_rec); LINT_INIT(ptr_to_rec2);

  flag=0;
  if (!(empty= _hp_find_free_hash(&keyinfo->block,info->records)))
    DBUG_RETURN(-1);				/* No more memory */
  halfbuff= (long) info->blength >> 1;
  pos=	 hp_find_hash(&keyinfo->block,(first_index=info->records-halfbuff));

  if (pos != empty)				/* If some records */
  {
    do
    {
      hashnr=_hp_rec_hashnr(keyinfo,pos->ptr_to_rec);
      if (flag == 0)				/* First loop; Check if ok */
	if (_hp_mask(hashnr,info->blength,info->records) != first_index)
	  break;
      if (!(hashnr & halfbuff))
      {						/* Key will not move */
	if (!(flag & LOWFIND))
	{
	  if (flag & HIGHFIND)
	  {
	    flag=LOWFIND | HIGHFIND;
	    /* key shall be moved to the current empty position */
	    gpos=empty;
	    ptr_to_rec=pos->ptr_to_rec;
	    empty=pos;				/* This place is now free */
	  }
	  else
	  {
	    flag=LOWFIND | LOWUSED;		/* key isn't changed */
	    gpos=pos;
	    ptr_to_rec=pos->ptr_to_rec;
	  }
	}
	else
	{
	  if (!(flag & LOWUSED))
	  {
	    /* Change link of previous LOW-key */
	    gpos->ptr_to_rec=ptr_to_rec;
	    gpos->next_key=pos;
	    flag= (flag & HIGHFIND) | (LOWFIND | LOWUSED);
	  }
	  gpos=pos;
	  ptr_to_rec=pos->ptr_to_rec;
	}
      }
      else
      {						/* key will be moved */
	if (!(flag & HIGHFIND))
	{
	  flag= (flag & LOWFIND) | HIGHFIND;
	  /* key shall be moved to the last (empty) position */
	  gpos2 = empty; empty=pos;
	  ptr_to_rec2=pos->ptr_to_rec;
	}
	else
	{
	  if (!(flag & HIGHUSED))
	  {
	    /* Change link of previous hash-key and save */
	    gpos2->ptr_to_rec=ptr_to_rec2;
	    gpos2->next_key=pos;
	    flag= (flag & LOWFIND) | (HIGHFIND | HIGHUSED);
	  }
	  gpos2=pos;
	  ptr_to_rec2=pos->ptr_to_rec;
	}
      }
    }
    while ((pos=pos->next_key));

    if ((flag & (LOWFIND | LOWUSED)) == LOWFIND)
    {
      gpos->ptr_to_rec=ptr_to_rec;
      gpos->next_key=0;
    }
    if ((flag & (HIGHFIND | HIGHUSED)) == HIGHFIND)
    {
      gpos2->ptr_to_rec=ptr_to_rec2;
      gpos2->next_key=0;
    }
  }
  /* Check if we are at the empty position */

  pos=hp_find_hash(&keyinfo->block,_hp_mask(_hp_rec_hashnr(keyinfo,record),
					    info->blength,info->records+1));
  if (pos == empty)
  {
    pos->ptr_to_rec=recpos;
    pos->next_key=0;
  }
  else
  {
    /* Check if more records in same hash-nr family */
    empty[0]=pos[0];
    gpos=hp_find_hash(&keyinfo->block,
		      _hp_mask(_hp_rec_hashnr(keyinfo,pos->ptr_to_rec),
			       info->blength,info->records+1));
    if (pos == gpos)
    {
      pos->ptr_to_rec=recpos;
      pos->next_key=empty;
    }
    else
    {
      pos->ptr_to_rec=recpos;
      pos->next_key=0;
      _hp_movelink(pos,gpos,empty);
    }

    /* Check if dupplicated keys */
    if ((keyinfo->flag & HA_NOSAME) && pos == gpos)
    {
      pos=empty;
      do
      {
	if (! _hp_rec_key_cmp(keyinfo,record,pos->ptr_to_rec))
	{
	  my_errno=HA_ERR_FOUND_DUPP_KEY;
	  DBUG_RETURN(-1);
	}
      } while ((pos=pos->next_key));
    }
  }
  DBUG_RETURN(0);
}

	/* Returns ptr to block, and allocates block if neaded */

static HASH_INFO *_hp_find_free_hash(block,records)
HP_BLOCK *block;
ulong records;
{
  uint block_pos;

  if (records < block->last_allocated)
    return hp_find_hash(block,records);
  if (!(block_pos=(records % block->records_in_block)))
    if (_hp_get_new_block(block))
      return(NULL);
  block->last_allocated=records+1;
  return((HASH_INFO*) ((byte*) block->level_info[0].last_blocks+
		       block_pos*block->recbuffer));
}
