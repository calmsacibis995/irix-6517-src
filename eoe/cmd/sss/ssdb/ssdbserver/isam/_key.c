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

/* Functions to handle keys */

#include "isamdef.h"
#include "m_ctype.h"

static void _ni_put_key_in_record(N_INFO *info,uint keynr,byte *record);

	/* Make a intern key from a record */
	/* If ascii key convert according to sortorder */
	/* Ret: Length of key */

uint _ni_make_key(register N_INFO *info, uint keynr, uchar *key, const char *record, ulong filepos)
{
  uint length;
  byte *pos,*end;
  uchar *start;
  reg1 N_KEYSEG *keyseg;
  enum ha_base_keytype type;
  DBUG_ENTER("_ni_make_key");

  start=key;
  for (keyseg=info->s->keyinfo[keynr].seg ; keyseg->base.type ;keyseg++)
  {
    type=(enum ha_base_keytype) keyseg->base.type;
    if (keyseg->base.flag & HA_SPACE_PACK)
    {
      pos= (byte*) record+keyseg->base.start; end=pos+keyseg->base.length;
      if (type != HA_KEYTYPE_NUM)
      {
	while (end > pos && end[-1] == ' ')
	  end--;
      }
      else
      {
	while (pos < end && pos[0] == ' ')
	  pos++;
      }
      *key++= (uchar) (length=(uint) (end-pos));
      memcpy((byte*) key,(byte*) pos,(size_t) length);
#ifndef USE_STRCOLL
      if (type == HA_KEYTYPE_TEXT)
	case_sort((byte*) key,length);
#endif
      key+=length;
    }
    else
    {
      memcpy((byte*) key,(byte*) record+keyseg->base.start,
	     (size_t) keyseg->base.length);
#ifndef USE_STRCOLL
      if (type == HA_KEYTYPE_TEXT)
	case_sort((byte*) key,(uint) keyseg->base.length);
#endif
#ifdef NAN_TEST
      else if (type == HA_KEYTYPE_FLOAT)
      {
	float nr;
	bmove((byte*) &nr,(byte*) key,sizeof(float));
	if (nr == (float) FLT_MAX)
	{
	  nr= (float) FLT_MAX;
	  bmove((byte*) key,(byte*) &nr,sizeof(float));
	}
      }
      else if (type == HA_KEYTYPE_DOUBLE)
      {
	double nr;
	bmove((byte*) &nr,(byte*) key,sizeof(double));
	if (nr == DBL_MAX)
	{
	  nr=DBL_MAX;
	  bmove((byte*) key,(byte*) &nr,sizeof(double));
	}
      }
#endif
      key+= keyseg->base.length;
    }
  }
  _ni_dpointer(info,key,filepos);
  DBUG_PRINT("exit",("keynr: %d",keynr));
  DBUG_DUMP("key",(byte*) start,(uint) (key-start)+keyseg->base.length);
  DBUG_EXECUTE("key",_ni_print_key(DBUG_FILE,info->s->keyinfo[keynr].seg,start););
  DBUG_RETURN((uint) (key-start));		/* Return keylength */
} /* _ni_make_key */


	/* Pack a key to intern format from given format (c_rkey) */
	/* if key_length is set returns new length of key */

uint _ni_pack_key(register N_INFO *info, uint keynr, uchar *key, uchar *old, uint key_length)



						/* Length of used key */
{
  int k_length;
  uint length;
  uchar *pos,*end;
  reg1 N_KEYSEG *keyseg;
  enum ha_base_keytype type;
  DBUG_ENTER("_ni_pack_key");

  if ((k_length=(int) key_length) <= 0)
    k_length=N_MAX_KEY_BUFF;

  for (keyseg=info->s->keyinfo[keynr].seg ;
       keyseg->base.type && k_length >0;
       k_length-=keyseg->base.length, old+=keyseg->base.length, keyseg++)
  {
    length=min((uint) keyseg->base.length,(uint) k_length);
    type=(enum ha_base_keytype) keyseg->base.type;
    if (keyseg->base.flag & HA_SPACE_PACK)
    {
      pos=old; end=pos+length;
      if (type != HA_KEYTYPE_NUM)
      {
	while (end > pos && end[-1] == ' ')
	  end--;
      }
      else
      {
	while (pos < end && pos[0] == ' ')
	  pos++;
      }
      *key++ = (uchar) (length=(uint) (end-pos));
      memcpy((byte*) key,pos,(size_t) length);
    }
    else
      memcpy((byte*) key,old,(size_t) length);
#ifndef USE_STRCOLL
    if (type == HA_KEYTYPE_TEXT)
      case_sort((byte*) key,length);
#endif
    key+= length;
  }
  if (!keyseg->base.type)
  {
    if (k_length >= 0)				/* Hole key */
      key_length=0;
  }
  else
  {						/* Part-key ; fill with null */
    length= (uint) -k_length;			/* unused part of last key */
    do
    {
      length+= (keyseg->base.flag & HA_SPACE_PACK) ? 1 :
	keyseg->base.length;
      keyseg++;
    } while (keyseg->base.type);
    bzero((byte*) key,length);
  }
  DBUG_RETURN(key_length);			/* Return part-keylength */
} /* _ni_pack_key */


	/* Put a key in record */
	/* Used when only-keyread is wanted */

static void _ni_put_key_in_record(register N_INFO *info, uint keynr, byte *record)
{
  uint length;
  reg2 byte *key;
  byte *pos;
  reg1 N_KEYSEG *keyseg;
  DBUG_ENTER("_ni_put_key_in_record");

  key=(byte*) info->lastkey;
  for (keyseg=info->s->keyinfo[keynr].seg ; keyseg->base.type ;keyseg++)
  {
    if (keyseg->base.flag & HA_SPACE_PACK)
    {
      length= (uint) (uchar) *key++;
      pos= record+keyseg->base.start;
      if (keyseg->base.type != (int) HA_KEYTYPE_NUM)
      {
	memcpy(pos,key,(size_t) length);
	bfill(pos+length,keyseg->base.length-length,' ');
      }
      else
      {
	bfill(pos,keyseg->base.length-length,' ');
	memcpy(pos+keyseg->base.length-length,key,(size_t) length);
      }
      key+=length;
    }
    else
    {
      memcpy(record+keyseg->base.start,(byte*) key,
	     (size_t) keyseg->base.length);
      key+= keyseg->base.length;
    }
  }
  DBUG_VOID_RETURN;
} /* _ni_put_key_in_record */


	/* Here when key reads are used */

int _ni_read_key_record(N_INFO *info, ulong filepos, byte *buf)
{
  VOID(_ni_writeinfo(info,0));
  if (filepos != NI_POS_ERROR)
  {
    if (info->lastinx >= 0)
    {				/* Read only key */
      _ni_put_key_in_record(info,(uint) info->lastinx,buf);
      info->update|= HA_STATE_AKTIV; /* We should find a record */
      return 0;
    }
    my_errno=HA_ERR_WRONG_INDEX;
    return(-1);
  }
  return(-1);				/* Wrong data to read */
}
