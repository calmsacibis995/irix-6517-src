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

/* Functions to handle keys and fields in forms */

#include "mysql_priv.h"

	/* Search after with key field is. If no key starts with field test
	   if field is part of some key. */
	/* returns number of key. keylength is set to length of key up to and
	   including field */
	/* Used when calculating key for NEXT_NUMBER */

int find_ref_key(TABLE *form,Field *field)
{
  reg2 int i;
  reg3 KEY *key_info;
  uint j,fieldpos,length,fieldlength,tmp,*key_length=&tmp;

  fieldpos=    field->offset();
  fieldlength= field->pack_length();

	/* Test if some key starts as fieldpos */

  for (i=0, key_info=form->key_info ; i < (int) form->keys ; i++, key_info++)
  {
    if (key_info->key_part[0].offset == fieldpos)
    {						/* Found key. Calc keylength */
      *key_length=0;
      for (j=0 ;
	   (int7) j < key_info->key_parts &&
	   key_info->key_part[j].offset == fieldpos ;
	   j++)
      {
	if ((length=(int) key_info->key_part[j].length) >= fieldlength)
	{
	  (*key_length)+=fieldlength;	/* This field is enough */
	  break;
	}
	(*key_length)+=length;		/* We can at least use this part */ /* purecov: inspected */
	fieldpos+=length;		/* Test if field is over more parts */
	fieldlength-=length;
      }
      return(i);			/* Use this key */
    }
  }
  return(-1);					/* No key is ok */
}


	/* Copy a key from record to some buffer */
	/* if length == 0 then copy hole key */

void key_copy(byte *key,TABLE *form,uint index,uint key_length)
{
  uint length;
  KEY_PART_INFO *key_part;

  if (key_length == 0)
    key_length=form->key_info[index].key_length;
  for (key_part=form->key_info[index].key_part; key_length ; key_part++)
  {
    length=min(key_length,key_part->length);
    memcpy(key,form->record[0]+key_part->offset,(size_t) length);
    key+=length;
    key_length-=length;
  }
} /* key_copy */


	/* restore a key from some buffer to record */

void key_restore(TABLE *form,byte *key,uint index,uint key_length)
{
  uint length;
  KEY_PART_INFO *key_part;

  if (key_length == 0)
  {
    if (index == (uint) -1)
      return;
    key_length=form->key_info[index].key_length;
  }
  for (key_part=form->key_info[index].key_part; key_length ; key_part++)
  {
    length=min(key_length,key_part->length);
    memcpy(form->record[0]+key_part->offset,key,(size_t) length);
    key+=length;
    key_length-=length;
  }
} /* key_restore */


	/* Compare if a key has changed */

int key_cmp(TABLE *form,byte *key,uint index,uint key_length)
{
  uint length;
  KEY_PART_INFO *key_part;

  for (key_part=form->key_info[index].key_part; key_length ; key_part++)
  {
    length=min(key_length,key_part->length);
    if (!(key_part->key_type & (FIELDFLAG_NUMBER+FIELDFLAG_BINARY+
				FIELDFLAG_PACK)))
    {
      if (my_sortcmp((char*) key,(char*) form->record[0]+key_part->offset,
		     length))
	return 1;
    }
    else if (memcmp(key,form->record[0]+key_part->offset,length))
      return 1;
    key+=length;
    key_length-=length;
  }
  return 0;
}

	/* unpack key-fields from record to some buffer */
	/* This is used to get a good error message */

void key_unpack(String *to,TABLE *table,uint index)
{
  KEY_PART_INFO *key_part,*key_part_end;
  Field *field;
  String tmp;
  DBUG_ENTER("key_unpack");

  to->length(0);
  for (key_part=table->key_info[index].key_part,key_part_end=key_part+
	 table->key_info[index].key_parts ;
       key_part < key_part_end;
       key_part++)
  {
    if (to->length())
      to->append('-');
    if ((field=key_part->field))
    {
      field->val_str(&tmp,&tmp);
      if (key_part->length < field->pack_length())
	tmp.length(min(tmp.length(),key_part->length));
      to->append(tmp);
    }
    else
      to->append("???");
  }
  DBUG_VOID_RETURN;
}
