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

/* Makes a database from a form-file */

#include "mysql_priv.h"
#ifndef NO_HASH
#include <hashdb.h>
#endif
#include <heap.h>
#include <nisam.h>
#include <merge.h>

	/* Functions in this file */

#ifndef NO_HASH
static int NEAR_F cre_hash(char *name,TABLE *form);
#endif
static int NEAR_F cre_isam(char *name,TABLE *form,uint options);


	/* Initiates form-file and calls apropriate database-creator */
	/* Returns 1 if something got wrong */

int cre_database(my_string name)
{
  int error;
  TABLE form;
  DBUG_ENTER("cre_database");

  error=0;					// Keep lint happy
  if (openfrm(name,"",0,(uint) (READ_ALL+GET_NAME_OF_INDEXFILE),&form))
    DBUG_RETURN(1);

  switch (form.db_type)
  {
#ifndef NO_HASH
    case DB_TYPE_HASH:		error=cre_hash(name,&form); break;
#endif
#ifndef NO_HEAP
    case DB_TYPE_HEAP:
      {						/* Remove old database */
	char buff[FN_REFLEN];
	error=heap_create(fn_format(buff,name,"","",1+2));
	break;
      }
#endif
    case DB_TYPE_ISAM:
      error=cre_isam(name,&form, form.db_create_options);
      break;
  case DB_TYPE_MRG_ISAM:
#ifndef NO_MERGE
      {
	char buff[FN_REFLEN];
	error=mrg_create(fn_format(buff,name,"","",2+4+16),0);
	break;
      }
#endif
    default: break;				/* impossible */
  }
  VOID(closefrm(&form));
  if (error)
    my_error(ER_CANT_CREATE_TABLE,MYF(ME_BELL+ME_WAITTANG),name,my_errno);
  DBUG_RETURN(error != 0);
} /* cre_database */


	/* makes a hash-database */

#ifndef NO_HASH

static int NEAR_F cre_hash(my_string name, register TABLE *form)
{
  register uint i,j;
  char buff[FN_REFLEN];
  KEY *pos;
  H_KEYDEF keydef[MAX_KEY];
  DBUG_ENTER("cre_hash");

  pos=form->key_info;
  for (i=0; i < form->keys ; i++, pos++)
  {
    keydef[i].hk_flag=	 pos->dupp_key ? 0 : HA_NOSAME;
    for (j=0 ; (int7) j < pos->key_parts ; j++)
    {
      uint flag=pos->key_part[j].key_type;
      if (!f_is_packed(flag) && f_packtype(flag) == (int) FIELD_TYPE_DECIMAL &&
	  !(flag & FIELDFLAG_BINARY))
	keydef[i].hk_keyseg[j].key_type= (int) HA_KEYTYPE_TEXT;
      else
	keydef[i].hk_keyseg[j].key_type= (int) HA_KEYTYPE_BINARY;
      keydef[i].hk_keyseg[j].start=  pos->key_part[j].offset;
      keydef[i].hk_keyseg[j].length= pos->key_part[j].length;
    }
    keydef[i].hk_keyseg[j].key_type= 0;
  }
  DBUG_RETURN(h_create(fn_format(buff,name,"","",2+4+16),i,
		       keydef,form->reclength,form->max_records,form->reloc,
		       0));
} /* cre_hash */
#endif


	/* makes a new-isam-database */

static int NEAR_F cre_isam(my_string name, register TABLE *form, uint options)
{
  int error;
  uint i,j,recpos,minpos,fieldpos,temp_length,length;
  enum ha_base_keytype type;
  char buff[FN_REFLEN];
  KEY *pos;
  N_KEYDEF keydef[MAX_KEY];
  N_RECINFO *recinfo,*recinfo_pos;
  DBUG_ENTER("cre_isam");

  type=HA_KEYTYPE_BINARY;				// Keep compiler happy
  if (!(recinfo= (N_RECINFO*) my_malloc((form->fields*2+2)*sizeof(N_RECINFO),
					MYF(MY_WME))))
    DBUG_RETURN(1);

  pos=form->key_info;
  for (i=0; i < form->keys ; i++, pos++)
  {
    keydef[i].base.flag= (pos->dupp_key ? 0 : HA_NOSAME);
    for (j=0 ; (int7) j < pos->key_parts ; j++)
    {
      keydef[i].seg[j].base.flag=pos->key_part[j].key_part_flag;
      Field *field=pos->key_part[j].field;
      type=field->key_type();

      if ((options & HA_OPTION_PACK_KEYS) && pos->key_part[j].length > 8 &&
	  (type == HA_KEYTYPE_TEXT ||
	   type == HA_KEYTYPE_NUM ||
	   (type == HA_KEYTYPE_BINARY && !field->zero_pack())))
      {
	if (j == 0)
	  keydef[i].base.flag|=HA_PACK_KEY;
	if (!(field->flags & ZEROFILL_FLAG) &&
	    (field->type() == FIELD_TYPE_STRING ||
	     field->type() == FIELD_TYPE_VAR_STRING ||
	     (pos->key_part[j].length - ((Field_num*) field)->decimals)
	     >= 4))
	  keydef[i].seg[j].base.flag|=HA_SPACE_PACK;
      }
      keydef[i].seg[j].base.type=(int) type;
      keydef[i].seg[j].base.start=  pos->key_part[j].offset;
      keydef[i].seg[j].base.length= pos->key_part[j].length;
    }
    keydef[i].seg[j].base.type=(int) HA_KEYTYPE_END;	/* End of key-parts */
  }

  recpos=0; recinfo_pos=recinfo;
  while (recpos < (uint) form->reclength)
  {
    Field **field,*found=0;
    minpos=form->reclength; length=0;

    for (field=form->field ; *field ; field++)
    {
      if ((fieldpos=(*field)->offset()) >= recpos &&
	  fieldpos <= minpos)
      {
	if (!(temp_length= (*field)->pack_length()))
	  continue;				/* Skipp null-fields */
	if (! found || fieldpos < minpos ||
	    (fieldpos == minpos && temp_length < length))
	{
	  minpos=fieldpos; found= *field; length=temp_length;
	}
      }
    }
    DBUG_PRINT("loop",("found: %lx  recpos: %d  minpos: %d  length: %d",
		       found,recpos,minpos,length));
    if (recpos != minpos)
    {						// Not used space
      recinfo_pos->base.type=(int) FIELD_NORMAL;
      recinfo_pos++->base.length= (uint16) (minpos-recpos);
    }
    if (! found)
      break;

    if (found->flags & BLOB_FLAG)
      recinfo_pos->base.type= (int) FIELD_BLOB;
    else if (!(options & HA_OPTION_PACK_RECORD))
      recinfo_pos->base.type= (int) FIELD_NORMAL;
    else if (found->zero_pack())
      recinfo_pos->base.type= (int) FIELD_SKIPP_ZERO;
    else
      recinfo_pos->base.type= (int) ((length <= 3 ||
				      (found->flags & ZEROFILL_FLAG)) ?
				     FIELD_NORMAL :
				     found->type() == FIELD_TYPE_STRING ||
				     found->type() == FIELD_TYPE_VAR_STRING ?
				     FIELD_SKIPP_ENDSPACE :
				     FIELD_SKIPP_PRESPACE);
    recinfo_pos++ ->base.length=(uint16) length;
    recpos=minpos+length;
    DBUG_PRINT("loop",("length: %d  type: %d",
		       recinfo_pos[-1].base.length,recinfo_pos[-1].base.type));
  }
  recinfo_pos->base.type= (int) FIELD_LAST;	/* End of fieldinfo */
  error=ni_create(fn_format(buff,name,"","",2+4+16),form->keys,keydef,
		  recinfo,form->max_records,form->reloc,0,0,0L);
  my_free((gptr) recinfo,MYF(0));
  DBUG_RETURN(error);
}
