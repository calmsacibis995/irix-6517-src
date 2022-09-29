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

/* Diverse funktioner som anv{nds allt som oftast i datanet */

#include "mysql_priv.h"
#include <errno.h>


	/* Functions defined in this file */

static void frm_error(int error,TABLE *form,const char *name,int errortype);
static int read_string(File file, gptr *to, uint length);
static void fix_type_pointers(my_string **array, TYPELIB *point_to_type,
			      uint types, my_string *names);
static uint find_field(TABLE *form,uint start,uint length);


static byte* get_field_name(Field *buff,uint *length,my_bool not_used)
{
  *length= strlen(buff->field_name);
  return (byte*) buff->field_name;
}

	/* Open a .frm file */

int openfrm(const char *name, const char *alias, uint db_stat, uint prgflag,
	    TABLE *outparam)
{
  reg1 uint i;
  reg2 uchar *strpos;
  int	 j,flag,funktpos,error;
  uint	 rec_buff_length,n_length,int_length,records,key_parts,keys,
    interval_count,interval_parts;
  ulong  pos;
  char	 index_file[FN_REFLEN],*names;
  uchar  head[288],*disk_buff,new_field_pack_flag;
  my_string record,keynames,*int_array;
  bool	 new_frm_ver,use_hash;
  File	 file;
  Field  **field,*reg_field;
  KEY	 *keyinfo;
  KEY_PART_INFO *key_part;
  uchar *null_pos;
  uint null_bit;
  DBUG_ENTER("openfrm");
  DBUG_PRINT("enter",("name: '%s'  form: %lx",name,outparam));

  bzero((char*) outparam,sizeof(*outparam));
  disk_buff=NULL; record= NULL; keynames=NullS;
  outparam->db_stat = db_stat;
  funktpos=0; key_parts=0;

  init_sql_alloc(&outparam->mem_root,1024);
  MEM_ROOT *old_root=my_pthread_getspecific_ptr(MEM_ROOT*,THR_MALLOC);
  my_pthread_setspecific_ptr(THR_MALLOC,&outparam->mem_root);

  outparam->real_name=strdup_root(&outparam->mem_root,
				  name+dirname_length(name));
  *fn_ext(outparam->real_name)='\0';		// Remove extension
  outparam->table_name=my_strdup(alias,MYF(MY_WME));

  flag= (prgflag & CHANGE_FRM) ? O_RDWR : O_RDONLY | O_SHARE;
  error=1;
  if ((file=my_open(fn_format(index_file,name,"",reg_ext,4),flag,
		    MYF(0)))
      < 0)
  {
    goto err_end; /* purecov: inspected */
  }
  error=4;
  if (my_read(file,(byte*) head,64,MYF(MY_NABP))) goto err;
  if (head[0] != (uchar) 254 || head[1] != 1 ||
      (head[2] != FRM_VER && head[2] != FRM_VER+1))
    goto err; /* purecov: inspected */
  new_field_pack_flag=head[27];
  new_frm_ver= (head[2] == FRM_VER+1);

  error=3;
  if (!(pos=get_form_pos(file,head,NullS,(TYPELIB*) 0)))
    goto err; /* purecov: inspected */
  *fn_ext(index_file)='\0';			// Remove .frm extension

  outparam->db_type=ha_checktype((enum db_type) (uint) *(head+3));
  outparam->db_create_options=uint2korr(head+30);
  outparam->db_record_offset=1;

  error=4;
  outparam->max_records=uint4korr(head+18);
  outparam->reloc=uint4korr(head+22);

  /* Read keyinformation */
  VOID(my_seek(file,(ulong) uint2korr(head+6),MY_SEEK_SET,MYF(0)));
  if (read_string(file,(gptr*) &disk_buff,(uint) uint2korr(head+28)))
    goto err; /* purecov: inspected */
  outparam->keys=keys=disk_buff[0];
  key_parts=disk_buff[1];
  n_length=keys*sizeof(KEY)+key_parts*sizeof(KEY_PART_INFO);
  if (!(keyinfo = (KEY*) alloc_root(&outparam->mem_root,
				    n_length+uint2korr(disk_buff+4))))
    goto err; /* purecov: inspected */
  bzero((char*) keyinfo,n_length);
  outparam->key_info=keyinfo;
  outparam->max_key_length=0;
  key_part= (KEY_PART_INFO*) (keyinfo+keys);
  strpos=disk_buff+6;
  for (i=0 ; i < keys ; i++, keyinfo++)
  {
    keyinfo->dupp_key=	 (uint) strpos[0];
    keyinfo->key_length= (uint) uint2korr(strpos+1);
    keyinfo->key_parts=  (uint) strpos[3];  strpos+=4;
    keyinfo->key_part=	 key_part;
    set_if_bigger(outparam->max_key_length,keyinfo->key_length);
    for (j=keyinfo->key_parts ; j-- ; key_part++)
    {
      key_part->fieldnr=	(uint16) (uint2korr(strpos) & FIELD_NR_MASK);
      key_part->offset= (uint) uint2korr(strpos+2)-1;
      key_part->key_type=	(uint) uint2korr(strpos+5);
      key_part->field=	(Field*) 0;	// Will be fixed later
      if (new_frm_ver)
      {
	key_part->key_part_flag=	*(strpos+4);
	key_part->length=	(uint) uint2korr(strpos+7);
	strpos+=9;
      }
      else
      {
	key_part->length=	*(strpos+4);
	key_part->key_part_flag=0;
	if (key_part->length > 128)
	{
	  key_part->length&=127; /* purecov: inspected */
	  key_part->key_part_flag=HA_REVERSE_SORT; /* purecov: inspected */
	}
	strpos+=7;
      }
    }
  }
  VOID(strmov(keynames= (my_string) key_part,(my_string) strpos));
  outparam->reclength = uint2korr((head+16));
  if (*(head+26))
    outparam->system=1;				/* one-record-database */

  error=2;
  if ((ha_open(outparam,index_file,
	       (db_stat & HA_READ_ONLY ? O_RDONLY : O_RDWR),
	       (db_stat & HA_WAIT_IF_LOCKED ||
		specialflag & SPECIAL_WAIT_IF_LOCKED ?
		HA_OPEN_WAIT_IF_LOCKED :
		(db_stat & (HA_ABORT_IF_LOCKED | HA_GET_INFO)) ?
		HA_OPEN_ABORT_IF_LOCKED : HA_OPEN_IGNORE_IF_LOCKED))))
    goto err; /* purecov: inspected */
  error=4; funktpos=1;
  outparam->reginfo.lock_type= TL_UNLOCK;
  if (db_stat & HA_OPEN_KEYFILE || (prgflag & DELAYED_OPEN)) records=2;
  else records=1;
  if (prgflag & (READ_ALL+EXTRA_RECORD)) records++;
  rec_buff_length=ALIGN_SIZE(outparam->reclength+1);
  if (!(outparam->record[0]= (byte*)
	(record = (char *) alloc_root(&outparam->mem_root,
					  rec_buff_length * records))))
    goto err;					/* purecov: inspected */
  record[outparam->reclength]=0;		// For purify and ->c_ptr()
  outparam->rec_buff_length=rec_buff_length;
  if (my_pread(file,(byte*) record,(uint) outparam->reclength,
	       (ulong) (uint2korr(head+6)+uint2korr(head+14)),
	       MYF(MY_NABP)))
    goto err; /* purecov: inspected */
  for (i=0 ; i < records ; i++, record+=rec_buff_length)
  {
    outparam->record[i]=(byte*) record;
    if (i)
      memcpy(record,record-rec_buff_length,(uint) outparam->reclength);
  }

  if (records == 2)
  {						/* fix for select */
    outparam->record[2]=outparam->record[1];
    if (db_stat & HA_READ_ONLY)
      outparam->record[1]=outparam->record[0]; /* purecov: inspected */
  }

  VOID(my_seek(file,pos,MY_SEEK_SET,MYF(0)));
  if (my_read(file,(byte*) head,288,MYF(MY_NABP))) goto err;
  outparam->fields= uint2korr(head+258);
  pos=uint2korr(head+260);			/* Length of all screens */
  n_length=uint2korr(head+268);
  interval_count=uint2korr(head+270);
  interval_parts=uint2korr(head+272);
  int_length=uint2korr(head+274);
  outparam->null_fields=uint2korr(head+282);
  VOID(my_seek(file,pos,MY_SEEK_CUR,MYF(0)));

  DBUG_PRINT("form",("i_count: %d  i_parts: %d  index: %d  n_length: %d  int_length: %d", interval_count,interval_parts, outparam->keys,n_length,int_length));

  if (!(field = (Field **)
	alloc_root(&outparam->mem_root,
		       (uint) ((outparam->fields+1)*sizeof(Field*)+
			       interval_count*sizeof(TYPELIB)+
			       (outparam->fields+interval_parts+
				keys+3)*sizeof(my_string)+
			       (n_length+int_length)))))
    goto err; /* purecov: inspected */

  outparam->field=field;
  if (read_string(file,(gptr*) &disk_buff,(uint) (outparam->fields*11)))
    goto err; /* purecov: inspected */

  outparam->intervals= (TYPELIB*) (outparam->field+outparam->fields+1);
  int_array= (my_string*) (outparam->intervals+interval_count);
  names= (char*) (int_array+outparam->fields+interval_parts+keys+3);
  if (!interval_count)
    outparam->intervals=0;			// For better debugging
  if (my_read(file, (byte*) names, (uint) (n_length+int_length),MYF(MY_NABP)))
    goto err; /* purecov: inspected */

  fix_type_pointers(&int_array,&outparam->fieldnames,1,&names);
  fix_type_pointers(&int_array,outparam->intervals,interval_count,
		    &names);
  if (keynames)
    fix_type_pointers(&int_array,&outparam->keynames,1,&keynames);
  VOID(my_close(file,MYF(MY_WME)));

  strpos= disk_buff;
  record=(char*) outparam->record[0]-1;		/* Fieldstart = 1 */
  outparam->null_flags=null_pos=
    (uchar*) (record+1+outparam->reclength-(outparam->null_fields+7)/8);
  null_bit=1;

  use_hash= outparam->fields >= MAX_FIELDS_BEFORE_HASH;
  if (use_hash)
    use_hash= !hash_init(&outparam->name_hash,
			 outparam->fields,0,0,
			 (hash_get_key) get_field_name,0,
			 HASH_CASE_INSENSITIVE);

  for (i=0 ; i < outparam->fields; i++, strpos+= 11, field++)
  {
    uint pack_flag= uint2korr(strpos+6);
    uint interval_nr= (uint) strpos[10];

    *field=reg_field=make_field(record+uint2korr(strpos+4),
				(uint32) strpos[3], // field_length
				null_pos,null_bit,
				pack_flag,
				(Field::utype) MTYP_TYPENR((uint) strpos[8]),
				(interval_nr ?
				 outparam->intervals+interval_nr-1 :
				 (TYPELIB*) 0),
				outparam->fieldnames.type_names[i],
				outparam);
    if (!(reg_field->flags & NOT_NULL_FLAG))
    {
      if ((null_bit<<=1) == 256)
      {
	null_pos++;
	null_bit=1;
      }
    }
    if (reg_field->unireg_check == Field::NEXT_NUMBER)
    {
      if ((int) (outparam->next_number_index= (uint)
		 find_ref_key(outparam,reg_field)) < 0)
	reg_field->unireg_check=Field::NONE;	/* purecov: inspected */
      else
      {
	outparam->found_next_number_field=reg_field;
	reg_field->flags|=AUTO_INCREMENT_FLAG;
      }
    }
    if (use_hash)
      (void) hash_insert(&outparam->name_hash,(byte*) *field); // Will never fail
  }
  outparam->key_parts=key_parts;
  *field=0;					// End marker

  /* Fix key->name and key_part->field */
  if (key_parts)
  {
    uint primary_key=(uint) (find_type("PRIMARY",&outparam->keynames,3)-1);
    uint ha_option=ha_option_flag[outparam->db_type];
    keyinfo=outparam->key_info;
    key_part=keyinfo->key_part;

    for (uint key=0 ; key < outparam->keys ; key++,keyinfo++)
    {
      uint usable_parts=0;
      keyinfo->name=outparam->keynames.type_names[key];
      if (primary_key >= MAX_KEY && !keyinfo->dupp_key)
	primary_key=key;

      for (i=0 ; i < keyinfo->key_parts ; key_part++,i++)
      {
	if (new_field_pack_flag <= 1)
	  key_part->fieldnr=(uint16) find_field(outparam,
						(uint) key_part->offset,
						(uint) key_part->length);
	if (key_part->fieldnr)
	{					// Should always be true !
	  key_part->field=outparam->field[key_part->fieldnr-1];
	  if (i == 0 && key != primary_key)
	    key_part->field->flags |=
	      (!keyinfo->dupp_key &&
	       key_part->field->pack_length() ==
	       keyinfo->key_length ? UNIQUE_KEY_FLAG : MULTIPLE_KEY_FLAG);
	  if (i == 0)
	    key_part->field->key_start|= ((key_map) 1 << key);
	  if (ha_option & HA_HAVE_KEYREAD_ONLY &&
	      key_part->field->pack_length() == key_part->length)

	  {
	    if (key_part->field->key_type() != HA_KEYTYPE_TEXT ||
		!(ha_option & HA_KEYREAD_WRONG_STR))
	      key_part->field->part_of_key|= ((key_map) 1 << key);
	  }
	  if (!key_part->key_part_flag && usable_parts == i)
	    usable_parts++;			// For FILESORT
	  key_part->field->flags|= PART_KEY_FLAG;
	  if (key == primary_key)
	    key_part->field->flags|= PRI_KEY_FLAG;

	  if (key_part->field->pack_length() != key_part->length)
	  {					// Create a new field
	    key_part->field=key_part->field->new_field(outparam);
	    key_part->field->field_length=key_part->length;
	  }
	}
	else
	{					// Error: shorten key
	  keyinfo->key_parts=usable_parts;
	  keyinfo->dupp_key=0;
	}
      }
      keyinfo->usable_key_parts=usable_parts; // Filesort
    }
  }
  x_free((gptr) disk_buff);
  if (new_field_pack_flag <= 1)
  {			/* Old file format with default null */
    uint null_length=(outparam->null_fields+7)/8;
    bfill(outparam->null_flags,null_length,255);
    bfill(outparam->null_flags+outparam->rec_buff_length,null_length,255);
    if (records > 2)
      bfill(outparam->null_flags+outparam->rec_buff_length*2,null_length,255);
  }
  my_pthread_setspecific_ptr(THR_MALLOC,old_root);
  opened_tables++;
#ifndef DBUG_OFF
  if (use_hash)
    (void) hash_check(&outparam->name_hash);
#endif
  DBUG_RETURN (0);

 err:
  if (funktpos)
  {
    VOID(ha_close(outparam));
    outparam->file=0;				// For easyer errorchecking
    outparam->fields=0;				// Probably safe
  }

  x_free((gptr) disk_buff);
  VOID(my_close(file,MYF(MY_WME)));

 err_end:					/* Here when no file */
  my_pthread_setspecific_ptr(THR_MALLOC,old_root);
  frm_error(error,outparam,name,ME_ERROR+ME_WAITTANG);
  free_root(&outparam->mem_root);
  my_free(outparam->table_name,MYF(0));
  DBUG_RETURN (error);
} /* openfrm */


	/* close a .frm file and it's tables*/

int closefrm(register TABLE *table)
{
  int error=0;
  DBUG_ENTER("closefrm");
  if (table->db_stat)
    error=ha_close(table);
#ifdef NOT_USED					// For old DIAB ISAM
  else if (table->dfile)
    error=my_close(table->dfile,MYF(MY_WME)); /* purecov: deadcode */
#endif
  if (table->table_name)
  {
    my_free(table->table_name,MYF(0));
    table->table_name=0;
  }
  if (table->fields)
  {
    for (Field **ptr=table->field ; *ptr ; ptr++)
      delete *ptr;
    table->fields=0;
  }
  free_root(&table->mem_root);
  hash_free(&table->name_hash);
  table->file=0;				/* For easyer errorchecking */
  DBUG_RETURN(error);
}


/* Deallocte temporary blob storage */

void free_blobs(register TABLE *table)
{
  for (Field **ptr=table->field ; *ptr ; ptr++)
  {
    if ((*ptr)->flags & BLOB_FLAG)
      ((Field_blob *) (*ptr))->free();
  }
}


	/* Find where a form starts */
	/* if formname is NullS then only formnames is read */

ulong get_form_pos(File file, uchar *head, my_string outname,
		   TYPELIB *save_names)
{
  uint a_length,names,length;
  uchar *pos,*buf;
  ulong ret_value=0;
  DBUG_ENTER("get_form_pos");

  names=uint2korr(head+8);
  a_length=(names+2)*sizeof(my_string);		/* Room for two extra */

  if (!save_names)
    a_length=0;
  else
    save_names->type_names=0;			/* Clear if error */

  if (outname && head[32])
    VOID(strmake((char*) outname,(char*) head+32,32));

  if (names)
  {
    length=uint2korr(head+4);
    VOID(my_seek(file,64L,MY_SEEK_SET,MYF(0)));
    if (!(buf= (uchar*) my_malloc((uint) length+a_length+names*4,
				  MYF(MY_WME))) ||
	my_read(file,(byte*) buf+a_length,(uint) (length+names*4),
		MYF(MY_NABP)))
    {						/* purecov: inspected */
      x_free((gptr) buf);			/* purecov: inspected */
      DBUG_RETURN(0L);				/* purecov: inspected */
    }
    pos= buf+a_length+length;
    ret_value=uint4korr(pos);
  }
  if (! save_names)
    my_free((gptr) buf,MYF(0));
  else if (!names)
    bzero((char*) save_names,sizeof(save_names));
  else
  {
    char *str;
    str=(my_string) (buf+a_length);
    fix_type_pointers((my_string**) &buf,save_names,1,&str);
  }
  DBUG_RETURN(ret_value);
}


	/* Read string from a file with malloc */

static int read_string(File file, gptr *to, uint length)
{
  DBUG_ENTER("read_string");

  x_free((gptr) *to);
  if (!(*to= (gptr) my_malloc(length+1,MYF(MY_WME))) ||
      my_read(file,(byte*) *to,length,MYF(MY_NABP)))
  {
    x_free((gptr) *to); /* purecov: inspected */
    *to= 0; /* purecov: inspected */
    DBUG_RETURN(1); /* purecov: inspected */
  }
  *((char*) *to+length)= '\0';
  DBUG_RETURN (0);
} /* read_string */


	/* Add a new form to a form file */

ulong make_new_entry(File file, uchar *fileinfo, TYPELIB *formnames,
		     my_string newname)
{
  uint i,bufflength,maxlength,n_length,length,names;
  ulong endpos,newpos;
  char buff[IO_SIZE];
  uchar *pos;
  DBUG_ENTER("make_new_entry");

  length=strlen(newname)+1;
  n_length=uint2korr(fileinfo+4);
  maxlength=uint2korr(fileinfo+6);
  names=uint2korr(fileinfo+8);
  newpos=uint4korr(fileinfo+10);

  if (64+length+n_length+(names+1)*4 > maxlength)
  {						/* Expand file */
    newpos+=IO_SIZE;
    int4store(fileinfo+10,newpos);
    endpos=my_seek(file,0L,MY_SEEK_END,MYF(0)); /* Copy from file-end */
    bufflength= (uint) (endpos & (IO_SIZE-1));	/* IO_SIZE is a power of 2 */

    while (endpos > maxlength)
    {
      VOID(my_seek(file,(ulong) (endpos-bufflength),MY_SEEK_SET,MYF(0)));
      if (my_read(file,(byte*) buff,bufflength,MYF(MY_NABP+MY_WME)))
	DBUG_RETURN(0L);
      VOID(my_seek(file,(ulong) (endpos-bufflength+IO_SIZE),MY_SEEK_SET,
		   MYF(0)));
      if ((my_write(file,(byte*) buff,bufflength,MYF(MY_NABP+MY_WME))))
	DBUG_RETURN(0);
      endpos-=bufflength; bufflength=IO_SIZE;
    }
    bzero(buff,IO_SIZE);			/* Null new block */
    VOID(my_seek(file,(ulong) maxlength,MY_SEEK_SET,MYF(0)));
    if (my_write(file,(byte*) buff,bufflength,MYF(MY_NABP+MY_WME)))
	DBUG_RETURN(0L);
    maxlength+=IO_SIZE;				/* Fix old ref */
    int2store(fileinfo+6,maxlength);
    for (i=names, pos= (uchar*) *formnames->type_names+n_length-1; i-- ;
	 pos+=4)
    {
      endpos=uint4korr(pos)+IO_SIZE;
      int4store(pos,endpos);
    }
  }

  if (n_length == 1 )
  {						/* First name */
    length++;
    VOID(strxmov(buff,"/",newname,"/",NullS));
  }
  else
    VOID(strxmov(buff,newname,"/",NullS)); /* purecov: inspected */
  VOID(my_seek(file,63L+(ulong) n_length,MY_SEEK_SET,MYF(0)));
  if (my_write(file,(byte*) buff,(uint) length+1,MYF(MY_NABP+MY_WME)) ||
      (names && my_write(file,(byte*) (*formnames->type_names+n_length-1),
			 names*4, MYF(MY_NABP+MY_WME))) ||
      my_write(file,(byte*) fileinfo+10,(uint) 4,MYF(MY_NABP+MY_WME)))
    DBUG_RETURN(0L); /* purecov: inspected */

  int2store(fileinfo+8,names+1);
  int2store(fileinfo+4,n_length+length);
  VOID(my_chsize(file,newpos,MYF(MY_WME)));	/* Append file with '\0' */
  DBUG_RETURN(newpos);
} /* make_new_entry */


	/* error message when opening a form file */

static void frm_error(int error, TABLE *form, const char *name, myf errortype)
{
  int er_no;
  char buff[FN_REFLEN],*form_dev="",*datext;
  DBUG_ENTER("frm_error");

  switch (error) {
  case 1:
    if (my_errno == ENOENT)
    {
      char *db;
      uint length=dirname_part(buff,name);
      buff[length-1]=0;
      db=buff+dirname_length(buff);
      my_error(ER_NO_SUCH_TABLE,MYF(0),db,form->real_name);
    }
    else
      my_error(ER_FILE_NOT_FOUND,errortype,
	       fn_format(buff,name,form_dev,reg_ext,0),my_errno);
    break;
  case 2:
  {
    datext=bas_ext[form->db_type][test(form->db_stat & HA_OPEN_KEYFILE)];
    er_no= (my_errno == ENOENT) ? ER_FILE_NOT_FOUND : (my_errno == EAGAIN) ?
      ER_FILE_USED : ER_CANT_OPEN_FILE;
    my_error(er_no,errortype,
	     fn_format(buff,form->real_name,form_dev,datext,2),my_errno);
    break;
  }
  default:				/* Better wrong error than none */
  case 4:
    my_error(ER_NOT_FORM_FILE,errortype,
	     fn_format(buff,name,form_dev,reg_ext,0));
    break;
  }
  DBUG_VOID_RETURN;
} /* frm_error */


	/*
	** fix a str_type to a array type
	** typeparts sepearated with some char. differents types are separated
	** with a '\0'
	*/

static void
fix_type_pointers(my_string **array, TYPELIB *point_to_type, uint types,
		  my_string *names)
{
  my_string type_name,ptr;
  char chr;

  ptr= *names;
  while (types--)
  {
    point_to_type->name=0;
    point_to_type->type_names= *array;

    if ((chr= *ptr))			/* Test if empty type */
    {
      while ((type_name=strchr(ptr+1,chr)) != NullS)
      {
	*((*array)++) = ptr+1;
	*type_name= '\0';		/* End string */
	ptr=type_name;
      }
      ptr+=2;				/* Skipp end mark and last 0 */
    }
    else
      ptr++;
    point_to_type->count= (uint) (*array - point_to_type->type_names);
    point_to_type++;
    *((*array)++)= NullS;		/* End of type */
  }
  *names=ptr;				/* Update end */
  return;
} /* fix_type_pointers */


TYPELIB *typelib(List<String> &strings)
{
  TYPELIB *result=(TYPELIB*) sql_alloc(sizeof(TYPELIB));
  result->count=strings.elements;
  result->name="";
  result->type_names=(my_string*) sql_alloc(sizeof(my_string**)*(result->count+1));

  List_iterator<String> it(strings);
  String *tmp;
  for (uint i=0; (tmp=it++) ; i++)
    result->type_names[i]=(char*) tmp->ptr();
  result->type_names[result->count]=0;		// End marker
  return result;
}


	/*
	** Search after a field with given start & length
	** If an exact field isn't found, return longest field with starts
	** at right position.
	** Return 0 on error, else field number+1
	** This is needed because in some .frm fields 'fieldnr' was saved wrong
	*/

static uint find_field(TABLE *form,uint start,uint length)
{
  Field **field;
  uint i,pos;

  pos=0;

  for (field=form->field, i=1 ; i<= form->fields ; i++,field++)
  {
    if ((*field)->offset() == start)
    {
      if ((*field)->pack_length() == length)
	return (i);
      if (!pos || form->field[pos-1]->pack_length() <
	  (*field)->pack_length())
	pos=i;
    }
  }
  return (pos);
}


	/* Kontrollerar att integern passar inom gr{nserna */

int set_zone(register int nr, int min_zone, int max_zone)
{
  if (nr<=min_zone)
    return (min_zone);
  if (nr>=max_zone)
    return (max_zone);
  return (nr);
} /* set_zone */

	/* Justerar ett tal till n{sta j{mna diskbuffer */

ulong next_io_size(register ulong pos)
{
  reg2 ulong offset;
  if ((offset= pos & (IO_SIZE-1)))
    return pos-offset+IO_SIZE;
  return pos;
} /* next_io_size */


void append_unescaped(String *res,const char *pos)
{
  for ( ; *pos ; pos++)
  {
    switch (*pos) {
    case 0:				/* Must be escaped for 'mysql' */
      res->append('\\');
      res->append('0');
      break;
    case '\n':				/* Must be escaped for logs */
      res->append('\\');
      res->append('n');
      break;
    case '\r':
      res->append('\\');		/* This gives better readbility */
      res->append('r');
      break;
    case '\\':
      res->append('\\');		/* Because of the sql syntax */
      res->append('\\');
      break;
    case '\'':
      res->append('\'');		/* Because of the sql syntax */
      res->append('\'');
      break;
    default:
      res->append(*pos);
      break;
    }
  }
}

	/* Create a .frm file */

File create_frm(register my_string name, uint reclength, uchar *fileinfo,
		enum db_type database, uint options, ulong records,
		ulong reloc,uint keys)
{
  register File file;
  uint key_length;
  ulong length;
  char fill[IO_SIZE];

  if ((file=my_create(name,CREATE_MODE,O_RDWR | O_TRUNC,MYF(MY_WME))) >= 0)
  {
    bzero((char*) fileinfo,64);
    fileinfo[0]=(uchar) 254; fileinfo[1]= 1; fileinfo[2]= FRM_VER+1; // Header
    fileinfo[3]= (uchar) ha_checktype(database);
    fileinfo[4]=1;
    int2store(fileinfo+6,IO_SIZE);		/* Next block starts here */
    key_length=keys*(7+NAME_LEN+MAX_REF_PARTS*9)+16;
    length=(ulong) next_io_size((ulong) (IO_SIZE+key_length+reclength));
    int4store(fileinfo+10,length);
    int2store(fileinfo+14,key_length);
    int2store(fileinfo+16,reclength);
    int4store(fileinfo+18,records);
    int4store(fileinfo+22,reloc);
    fileinfo[27]=2;				/* Use long pack-fields */
    int2store(fileinfo+30,options);
    VOID(fn_format((char*) fileinfo+32,name,"","",3));
    bzero(fill,IO_SIZE);
    for (; length > IO_SIZE ; length-= IO_SIZE)
    {
      if (my_write(file,(byte*) fill,IO_SIZE,MYF(MY_WME | MY_NABP)))
      {
	VOID(my_close(file,MYF(0)));
	VOID(my_delete(name,MYF(0)));
	return(-1);
      }
    }
  }
  return (file);
} /* create_frm */


int
rename_file_ext(const char * from,const char * to,const char * ext)
{
  char from_b[FN_REFLEN],to_b[FN_REFLEN];
  VOID(strxmov(from_b,from,ext,NullS));
  VOID(strxmov(to_b,to,ext,NullS));
  return (my_rename(from_b,to_b,MYF(MY_WME)));
}


/*
** Change the current ref in the .frm file to the current file name
*/

int fix_frm_ref(const char * name)
{
  char buff[FN_REFLEN];
  int error= -1;
  uint length;
  File file;
  DBUG_ENTER("fix_frm_ref");

  if ((file=my_open(fn_format(buff,name,"",reg_ext,2+4),O_RDWR,MYF_RW)) >= 0)
  {
    if (!(my_pread(file,(byte*) buff,32,32,MYF_RW | MY_WME)))
    {
      bzero(buff+32,32);				/* Remove old name */
      length=dirname_length(name);
      if (strlen(name)-length < 32)
	strmov(buff,name+length);
      if (!my_pwrite(file,(byte*) buff,32,32,MYF_RW | MY_WME))
	error=0;
    }
    VOID(my_close(file,MYF(MY_WME)));
  }
  DBUG_RETURN(error);
}

/*
  Alloc a value as a string and return it
  If field is empty, return NULL
*/

char *get_field(MEM_ROOT *mem, TABLE *table, uint fieldnr)
{
  Field *field=table->field[fieldnr];
  char buff[MAX_FIELD_WIDTH];
  String str(buff,sizeof(buff));
  field->val_str(&str,&str);
  uint length=str.length();
  if (!length)
    return NullS;
  char *to=alloc_root(mem,length+1);
  memcpy(to,str.ptr(),(uint) length);
  to[length]=0;
  return to;
}


/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef __GNUC__
template class List<String>;
template class List_iterator<String>;
#endif
