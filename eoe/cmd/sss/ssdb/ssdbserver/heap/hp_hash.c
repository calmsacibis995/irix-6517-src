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

/* The hash functions used for saveing keys */

#include "heapdef.h"
#include <m_ctype.h>

	/* Search after a record based on a key */
	/* Sets info->current_ptr to found record */

byte *_hp_search(info,keyinfo,key,nextflag)
reg3 HP_INFO *info;
HP_KEYDEF *keyinfo;
const byte *key;
uint nextflag;			/* Search=0, next=1, prev =2, same =3 */
{
  reg1 HASH_INFO *pos;
  int flag;
  uint old_nextflag;
  byte *prev_ptr_to_rec;
  DBUG_ENTER("_hp_search");

  old_nextflag=nextflag;
  flag=1;
  prev_ptr_to_rec=0;

  if (info->records)
  {
    pos=hp_find_hash(&keyinfo->block,_hp_mask(_hp_hashnr(keyinfo,key),
					      info->blength,info->records));
    do
    {
      if (!_hp_key_cmp(keyinfo,pos->ptr_to_rec,key))
      {
	switch (nextflag) {
	case 0:					/* Search after key */
	  DBUG_PRINT("exit",("found key at %d",pos->ptr_to_rec))
	  DBUG_RETURN (info->current_ptr= pos->ptr_to_rec);
	case 1:					/* Search next */
	  if (pos->ptr_to_rec == info->current_ptr)
	    nextflag=0;
	  break;
	case 2:					/* Search previous */
	  if (pos->ptr_to_rec == info->current_ptr)
	  {
	    my_errno=HA_ERR_KEY_NOT_FOUND;		/* Ifall gpos == 0 */
	    DBUG_RETURN (info->current_ptr=prev_ptr_to_rec);
	  }
	  prev_ptr_to_rec=pos->ptr_to_rec;	/* Prev. record found */
	  break;
	case 3:					/* Search same */
	  if (pos->ptr_to_rec == info->current_ptr)
	  {
	    DBUG_RETURN (info->current_ptr);
	  }
	}
      }
      if (flag)
      {
	flag=0;					/* Reset flag */
	if (hp_find_hash(&keyinfo->block,
			 _hp_mask(_hp_rec_hashnr(keyinfo,pos->ptr_to_rec),
				  info->blength,info->records)) != pos)
	  break;				/* Wrong link */
      }
    }
    while ((pos=pos->next_key));
  }
  my_errno=HA_ERR_KEY_NOT_FOUND;
  if (nextflag == 2 && ! info->current_ptr)
    DBUG_RETURN(info->current_ptr=prev_ptr_to_rec); /* Do a prev from end */

  if (old_nextflag && nextflag)
    my_errno=HA_ERR_RECORD_CHANGED;		/* Didn't find old record */
  DBUG_PRINT("exit",("Error: %d",my_errno));
  DBUG_RETURN ((info->current_ptr= 0));
}


	/* Calculate pos according to keys */

ulong _hp_mask(hashnr,buffmax,maxlength)
ulong hashnr,buffmax,maxlength;
{
  if ((hashnr & (buffmax-1)) < maxlength) return (hashnr & (buffmax-1));
  return (hashnr & ((buffmax >> 1) -1));
}


	/* Change link from pos to new_link */

void _hp_movelink(pos,next_link,newlink)
HASH_INFO *pos,*next_link,*newlink;
{
  HASH_INFO *old_link;
  do
  {
    old_link=next_link;
  }
  while ((next_link=next_link->next_key) != pos);
  old_link->next_key=newlink;
  return;
}


	/* Calc hashvalue for a key */

ulong _hp_hashnr(keydef,key)
register HP_KEYDEF *keydef;
register const byte *key;
{
  register ulong nr=1, nr2=4;
  HP_KEYSEG *seg,*endseg;
  byte *pos;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      for (pos=(byte*) key,key+=seg->length ; pos < key ; pos++)
      {
	nr^=(ulong) ((((uint) nr & 63)+nr2)*((uint) toupper(*pos)))+ (nr << 8);
	nr2+=3;
      }
    }
    else
    {
      for (pos=(byte*) key,key+=seg->length ; pos < key ; pos++)
      {
	nr^=(ulong) ((((uint) nr & 63)+nr2)*((uint) *pos))+ (nr << 8);
	nr2+=3;
      }
    }
  }
  return((ulong) nr);
}

	/* Calc hashvalue for a key in a record */

ulong _hp_rec_hashnr(keydef,rec)
register HP_KEYDEF *keydef;
register const byte *rec;
{
  register ulong nr=1, nr2=4;
  HP_KEYSEG *seg,*endseg;
  byte *pos,*end;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      for (pos=(byte*) rec+seg->start,end=pos+seg->length ; pos < end ; pos++)
      {
	nr^=(ulong) ((((uint) nr & 63)+nr2)*((uint) toupper(*pos)))+ (nr << 8);
	nr2+=3;
      }
    }
    else
    {
      for (pos=(byte*) rec+seg->start,end=pos+seg->length ; pos < end ; pos++)
      {
	nr^=(ulong) ((((uint) nr & 63)+nr2)*((uint) *pos))+ (nr << 8);
	nr2+=3;
      }
    }
  }
  return((ulong) nr);
}

	/* Compare keys for two records. Returns 0 if they are identical */

int _hp_rec_key_cmp(keydef,rec1,rec2)
HP_KEYDEF *keydef;
const byte *rec1;
const byte *rec2;
{
  HP_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      if (my_casecmp(rec1+seg->start,rec2+seg->start,seg->length))
	return 1;
    }
    else
    {
      if (bcmp(rec1+seg->start,rec2+seg->start,seg->length))
	return 1;
    }
  }
  return 0;
}

	/* Compare a key in a record to a hole key */

int _hp_key_cmp(keydef,rec,key)
HP_KEYDEF *keydef;
const byte *rec;
const byte *key;
{
  HP_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    if (seg->type == HA_KEYTYPE_TEXT)
    {
      if (my_casecmp(rec+seg->start,key,seg->length))
	return 1;
    }
    else
    {
      if (bcmp(rec+seg->start,key,seg->length))
	return 1;
    }
    key+=seg->length;
  }
  return 0;
}


	/* Copy a key from a record to a keybuffer */

void _hp_make_key(keydef,key,rec)
HP_KEYDEF *keydef;
byte *key;
const byte *rec;
{
  HP_KEYSEG *seg,*endseg;

  for (seg=keydef->seg,endseg=seg+keydef->keysegs ; seg < endseg ; seg++)
  {
    memcpy(key,rec+seg->start,(size_t) seg->length);
    key+=seg->length;
  }
}
