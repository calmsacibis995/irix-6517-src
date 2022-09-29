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

/* drop and alter of tables */

#include "mysql_priv.h"
#include "handler.h"
#include <hash.h>
#ifdef __WIN32__
#include <io.h>
#endif

extern HASH open_cache;

static bool check_if_keyname_exists(const char *name,KEY *start, KEY *end);
static char *make_unique_key_name(char *field_name,KEY *start,KEY *end);
static int copy_data_between_tables(TABLE *from,TABLE *to,
				    List<create_field> &create,
				    enum enum_duplicates handle_duplicates,
				    ulong *copied,ulong *deleted);

/*****************************************************************************
** Remove all possbile tables and give a compact errormessage for all
** wrong tables.
** This will wait for all users to free the table before dropping it
*****************************************************************************/

int mysql_rm_table(THD *thd,TABLE_LIST *tables, my_bool if_exists)
{
  char	path[FN_REFLEN];
  String wrong_tables;
  bool some_tables_deleted=0;
  uint error;
  DBUG_ENTER("mysql_rm_table");

  /* mark for close and remove all cached entries */

  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->mysys_var->current_mutex= &LOCK_open;
  thd->mysys_var->current_cond= &COND_refresh;
  VOID(pthread_mutex_lock(&LOCK_open));
  pthread_mutex_unlock(&thd->mysys_var->mutex);

  for (TABLE_LIST *table=tables ; table ; table=table->next)
  {
    char *db=table->db ? table->db : thd->db;
    abort_locked_tables(thd,db,table->real_name);
    while (remove_table_from_cache(db,table->real_name) && !thd->killed)
    {
      (void) pthread_cond_wait(&COND_refresh,&LOCK_open);
    }
    drop_locked_tables(thd,db,table->real_name);
    if (thd->killed)
    {
      VOID(pthread_mutex_unlock(&LOCK_open));
      pthread_mutex_lock(&thd->mysys_var->mutex);
      thd->mysys_var->current_mutex= 0;
      thd->mysys_var->current_cond= 0;
      pthread_mutex_unlock(&thd->mysys_var->mutex);
      DBUG_RETURN(-1);
    }
    /* remove form file and isam files */
    (void) sprintf(path,"%s/%s/%s%s",mysql_data_home,db,table->real_name,
		   reg_ext);
    (void) unpack_filename(path,path);
    error=0;
    if (access(path,F_OK))
    {
      if (!if_exists)
	error=1;				// Give error
    }
    else
    {
      error=my_delete(path,MYF(MY_WME));
      sprintf(path,"%s/%s/%s",mysql_data_home,db,table->real_name);
      if (ha_fdelete(DB_TYPE_ISAM,path))	// path is fixed here
	error=1;
      some_tables_deleted=1;
    }
    if (error)
    {
      if (wrong_tables.length())
	wrong_tables.append(',');
      wrong_tables.append(String(table->real_name));
    }
  }
  if (some_tables_deleted)
    mysql_update_log.write(thd->query);
  pthread_mutex_unlock(&LOCK_open);

  pthread_mutex_lock(&thd->mysys_var->mutex);
  thd->mysys_var->current_mutex= 0;
  thd->mysys_var->current_cond= 0;
  pthread_mutex_unlock(&thd->mysys_var->mutex);

  if (wrong_tables.length())
  {
    my_error(ER_BAD_TABLE_ERROR,MYF(0),wrong_tables.c_ptr());
    DBUG_RETURN(-1);
  }
  send_ok(&thd->net);
  DBUG_RETURN(0);
}


static int quick_rm_table(THD *thd,enum db_type base,const char *db,
			  const char *table_name)
{
  char path[FN_REFLEN];
  int error=0;
  (void) sprintf(path,"%s/%s/%s%s",mysql_data_home,db,table_name,reg_ext);
  unpack_filename(path,path);
  if (my_delete(path,MYF(0)))
    error=1; /* purecov: inspected */
  sprintf(path,"%s/%s/%s",mysql_data_home,db,table_name);
  return ha_fdelete(base,path) || error;
}


int mysql_create_table(THD *thd,const char *db, const char *table_name,
		       List<create_field> &fields,
		       List<Key> &keys,bool tmp_table)
{
  char		path[FN_REFLEN],*key_name;
  create_field	*sql_field,*dup_field;
  int		error;
  uint		db_options,field;
  ulong		pos;
  KEY	*key_info,*key_info_buffer;
  KEY_PART_INFO *key_part_info;
  int		auto_increment=0;
  DBUG_ENTER("mysql_create_table");

  /*
  ** Check for dupplicate fields and check type of table to create
  */

  if (!fields.elements)
  {
    my_error(ER_TABLE_MUST_HAVE_COLUMNS,MYF(0));
    DBUG_RETURN(-1);
  }
  List_iterator<create_field> it(fields),it2(fields);
  pos=1;					// Offset if fixed size record
  db_options=HA_OPTION_PACK_KEYS;
  while ((sql_field=it++))
  {
    if (sql_field->sql_type == FIELD_TYPE_BLOB ||
	sql_field->sql_type == FIELD_TYPE_VAR_STRING)
    {
      pos=0;					// Can use first pos in table
      db_options|=HA_OPTION_PACK_RECORD;
    }
    while ((dup_field=it2++) != sql_field)
    {
      if (my_strcasecmp(sql_field->field_name, dup_field->field_name) == 0)
      {
	my_error(ER_DUP_FIELDNAME,MYF(0),sql_field->field_name);
	DBUG_RETURN(-1);
      }
    }
    it2.rewind();
  }
  it.rewind();
  while ((sql_field=it++))
  {
    switch (sql_field->sql_type) {
    case FIELD_TYPE_BLOB:
    case FIELD_TYPE_MEDIUM_BLOB:
    case FIELD_TYPE_TINY_BLOB:
    case FIELD_TYPE_LONG_BLOB:
      sql_field->pack_flag=FIELDFLAG_BLOB |
	pack_length_to_packflag(sql_field->pack_length - sizeof(char*));
      if (sql_field->flags & BINARY_FLAG)
	sql_field->pack_flag|=FIELDFLAG_BINARY;
      sql_field->length=8;			// Unireg field length
      sql_field->unireg_check=Field::BLOB_FIELD;
      break;
    case FIELD_TYPE_VAR_STRING:
    case FIELD_TYPE_STRING:
      sql_field->pack_flag=0;
      if (sql_field->flags & BINARY_FLAG)
	sql_field->pack_flag|=FIELDFLAG_BINARY;
      break;
    case FIELD_TYPE_ENUM:
      sql_field->pack_flag=pack_length_to_packflag(sql_field->pack_length) |
	FIELDFLAG_INTERVAL;
      sql_field->unireg_check=Field::INTERVAL_FIELD;
      break;
    case FIELD_TYPE_SET:
      sql_field->pack_flag=pack_length_to_packflag(sql_field->pack_length) |
	FIELDFLAG_BITFIELD;
      sql_field->unireg_check=Field::BIT_FIELD;
      break;
    case FIELD_TYPE_DATE:			// Rest of string types
    case FIELD_TYPE_NEWDATE:
    case FIELD_TYPE_TIME:
    case FIELD_TYPE_DATETIME:
    case FIELD_TYPE_NULL:
      sql_field->pack_flag=f_settype((uint) sql_field->sql_type);
      break;
    default:
      sql_field->pack_flag=(FIELDFLAG_NUMBER |
			    (sql_field->flags & UNSIGNED_FLAG ? 0 :
			     FIELDFLAG_DECIMAL) |
			    (sql_field->flags & ZEROFILL_FLAG ?
			     FIELDFLAG_ZEROFILL : 0) |
			    f_settype((uint) sql_field->sql_type) |
			    (sql_field->decimals << FIELDFLAG_DEC_SHIFT));
      break;
    }
    if (!(sql_field->flags & NOT_NULL_FLAG))
      sql_field->pack_flag|=FIELDFLAG_MAYBE_NULL;
    sql_field->offset= pos;
    if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER)
      auto_increment++;
    pos+=sql_field->pack_length;
  }
  if (auto_increment > 1)
  {
    my_error(ER_WRONG_AUTO_KEY,MYF(0));
    DBUG_RETURN(-1);
  }

  /* Create keys */
  List_iterator<Key> key_iterator(keys);
  uint key_parts=0,key_count=keys.elements;
  bool primary_key=0;
  Key *key;

  if (key_count > MAX_KEY)
  {
    my_error(ER_TOO_MANY_KEYS,MYF(0),MAX_KEY);
    DBUG_RETURN(-1);
  }
  while ((key=key_iterator++))
  {
    if (key->columns.elements > MAX_REF_PARTS)
    {
      my_error(ER_TOO_MANY_KEY_PARTS,MYF(0),MAX_REF_PARTS);
      DBUG_RETURN(-1);
    }
    if (key->name() && strlen(key->name()) > NAME_LEN)
    {
      my_error(ER_TOO_LONG_IDENT, MYF(0), key->name());
      DBUG_RETURN(-1);
    }
    key_parts+=key->columns.elements;
  }
  key_info_buffer=key_info=(KEY*) sql_calloc(sizeof(KEY)*key_count);
  key_part_info=(KEY_PART_INFO*) sql_calloc(sizeof(KEY_PART_INFO)*key_parts);
  key_iterator.rewind();
  for (; (key=key_iterator++) ; key_info++)
  {
    uint key_length=0;
    key_part_spec *column;
    if (key->type == Key::PRIMARY)
    {
      if (primary_key)
      {
	my_error(ER_MULTIPLE_PRI_KEY,MYF(0));
	DBUG_RETURN(-1);
      }
      primary_key=1;
    }
    key_info->dupp_key=test(key->type == Key::MULTIPLE);
    key_info->key_parts=(uint8) key->columns.elements;
    key_info->key_part=key_part_info;

    if (key->type == Key::PRIMARY)
      key_name="PRIMARY";
    else if (!(key_name = key->name()))
      key_name=make_unique_key_name(key->columns.head()->field_name,
				    key_info_buffer,key_info);
    if (check_if_keyname_exists(key_name,key_info_buffer,key_info))
    {
      my_error(ER_DUP_KEYNAME,MYF(0),key_name);
      DBUG_RETURN(-1);
    }
    key_info->name=key_name;

    List_iterator<key_part_spec> cols(key->columns);
    for (uint column_nr=0 ; (column=cols++) ; column_nr++)
    {
      it.rewind();
      field=0;
      while ((sql_field=it++) &&
	     my_strcasecmp(column->field_name,sql_field->field_name))
	field++;
      if (!sql_field)
      {
	my_printf_error(ER_KEY_COLUMN_DOES_NOT_EXITS,
			ER(ER_KEY_COLUMN_DOES_NOT_EXITS),MYF(0),
			column->field_name);
	DBUG_RETURN(-1);
      }
      if (f_is_blob(sql_field->pack_flag))
      {
	my_printf_error(ER_BLOB_USED_AS_KEY,ER(ER_BLOB_USED_AS_KEY),MYF(0),
			column->field_name);
	DBUG_RETURN(-1);
      }
      if (!(sql_field->flags & NOT_NULL_FLAG))
      {
	my_printf_error(ER_NULL_COLUMN_IN_INDEX,ER(ER_NULL_COLUMN_IN_INDEX),
			MYF(0),column->field_name);
	DBUG_RETURN(-1);
      }
      if (MTYP_TYPENR(sql_field->unireg_check) == Field::NEXT_NUMBER &&
	  column_nr == 0)
	auto_increment--;			// Fields is used
      key_part_info->fieldnr= field;
      key_part_info->offset=  (uint16) sql_field->offset;
      key_part_info->key_type=sql_field->pack_flag;
      uint length=sql_field->pack_length;
      if (column->length)
      {
	if (column->length > length ||
	    (f_is_packed(sql_field->pack_flag) && column->length != length))
	{
	  my_error(ER_WRONG_SUB_KEY,MYF(0));
	  DBUG_RETURN(-1);
	}
	length=column->length;
      }
      key_part_info->length=(uint8) length;
      key_length+=length;
      key_part_info++;
    }
    key_info->key_length=(uint16) key_length;
    if (key_length > ha_max_key_length[DB_TYPE_ISAM])
    {
      my_error(ER_TOO_LONG_KEY,MYF(0),ha_max_key_length[DB_TYPE_ISAM]);
      DBUG_RETURN(-1);
    }
  }
  if (auto_increment > 0)
  {
    my_error(ER_WRONG_AUTO_KEY,MYF(0));
    DBUG_RETURN(-1);
  }

      /* Check if table exists */
  (void) sprintf(path,"%s/%s/%s%s",mysql_data_home,db,table_name,reg_ext);
  unpack_filename(path,path);
  VOID(pthread_mutex_lock(&LOCK_open));
  if (!tmp_table)
  {
    if (!access(path,F_OK))
    {
      VOID(pthread_mutex_unlock(&LOCK_open));
      my_error(ER_TABLE_EXISTS_ERROR,MYF(0),table_name);
      DBUG_RETURN(-1);
    }
  }
  thd->proc_info="creating table";
  if (rea_create_table(path,DB_TYPE_ISAM,db_options,0L,0L,
		       fields,key_count,key_info_buffer))
  {
    /* my_error(ER_CANT_CREATE_TABLE,MYF(0),table_name,my_errno); */
    error=-1;
    goto end;
  }
  if (!tmp_table)
    mysql_update_log.write(thd->query);	 // Must be written before unlock
  error=0;
end:
  VOID(pthread_mutex_unlock(&LOCK_open));
  thd->proc_info="After create";
  DBUG_RETURN(error);
}

/*
** Give the key name after the first field with an optional '_#' after
**/

static bool
check_if_keyname_exists(const char *name, KEY *start, KEY *end)
{
  for (KEY *key=start ; key != end ; key++)
    if (!my_strcasecmp(name,key->name))
      return 1;
  return 0;
}


static char *
make_unique_key_name(char *field_name,KEY *start,KEY *end)
{
  char buff[MAX_FIELD_NAME],*buff_end;

  if (!check_if_keyname_exists(field_name,start,end))
    return field_name;				// Use fieldname
  buff_end=strmake(buff,field_name,MAX_FIELD_NAME-4);
  for (uint i=2 ; ; i++)
  {
    sprintf(buff_end,"_%d",i);
    if (!check_if_keyname_exists(buff,start,end))
      return sql_strdup(buff);
  }
}


/****************************************************************************
** Alter a table definition
****************************************************************************/

static int
mysql_rename_table(THD *thd,enum db_type base,
		   const char *old_db,
		   const char * old_name,
		   const char *new_db,
		   const char * new_name)
{
  char from[FN_REFLEN],to[FN_REFLEN];
  DBUG_ENTER("mysql_rename_table");
  (void) sprintf(from,"%s/%s/%s",mysql_data_home,old_db,old_name);
  (void) sprintf(to,"%s/%s/%s",mysql_data_home,new_db,new_name);

  if (rename_file_ext(from,to,reg_ext) ||
      ha_frename(base,(const char*) from,(const char *) to) ||
      fix_frm_ref(to))
   DBUG_RETURN(1);
 DBUG_RETURN(0);
}

/*
  close table in this thread and force close + reopen in other threads
  This assumes that the calling thread has lock on LOCK_open
  Win32 clients must also have a WRITE LOCK on the table !
*/

static bool close_cached_table(THD *thd,TABLE *table)
{
  bool result=0;
  if (table)
  {
    mysql_lock_abort(table);			 // end threads waiting on lock
    VOID(ha_extra(table,HA_EXTRA_FORCE_REOPEN)); // Close all data files
    /* Mark all tables that are in use as 'old' */
    remove_table_from_cache(table->table_cache_key,table->table_name);
    if (thd->lock)
    {							// Normal lock
      mysql_unlock_tables(thd->lock); thd->lock=0;	// Start locked threads
    }
    // Close all copies of 'table'
    thd->open_tables=unlink_open_table(thd,thd->open_tables,table);
  }
  return result;
}


int mysql_alter_table(THD *thd,char *new_db, char *new_name,
		      TABLE_LIST *table_list,
		      List<create_field> &fields,
		      List<Key> &keys,List<Alter_drop> &drop_list,
		      List<Alter_column> &alter_list,
		      bool drop_primary,
		      enum enum_duplicates handle_duplicates)
{
  TABLE *table,*new_table;
  int error;
  char tmp_name[80],old_name[32],new_name_buff[FN_REFLEN],
    *table_name,*db;
  bool use_timestamp=0;
  ulong copied,deleted;
  uint save_time_stamp;
  DBUG_ENTER("mysql_alter_table");

  thd->proc_info="init";
  table_name=table_list->real_name;
  db=table_list->db;
  if (!new_db)
    new_db=db;
  /* Check that we are not trying to rename to an existing table */
  if (new_name)
  {
    strmov(new_name_buff,new_name);
    fn_same(new_name_buff,table_name,3);
    if (!strcmp(new_name_buff,table_name))	// Check if name changed
      new_name=table_name;			// No. Make later check easier
    else
    {
      if (!access(fn_format(new_name_buff,new_name_buff,new_db,reg_ext,0),
		  F_OK))
      {
	my_error(ER_TABLE_EXISTS_ERROR,MYF(0),new_name);
	DBUG_RETURN(-1);
      }
    }
  }
  else
    new_name=table_name;

  if (!(table=open_ltable(thd,table_list,TL_WRITE_ALLOW_READ)))
    DBUG_RETURN(-1);
  enum db_type db_type=table->db_type;

  /* Check if the user only wants to do a simple RENAME */

  thd->proc_info="setup";
  if (new_name != table_name &&
      !fields.elements && !keys.elements && ! drop_list.elements &&
      !alter_list.elements && !drop_primary)
  {
    thd->proc_info="rename";
    VOID(pthread_mutex_lock(&LOCK_open));
    /* Then do a 'simple' rename of the table */
    error=0;
    if (!access(new_name_buff,F_OK))
    {
      my_error(ER_TABLE_EXISTS_ERROR,MYF(0),new_name);
      error= -1;
    }
    else
    {
      *fn_ext(new_name)=0;
      close_cached_table(thd,table);
      if (mysql_rename_table(thd,db_type,db,table_name,new_db,new_name))
	error= -1;
    }
    VOID(pthread_cond_broadcast(&COND_refresh));
    VOID(pthread_mutex_unlock(&LOCK_open));
    if (!error)
      send_ok(&thd->net);
    DBUG_RETURN(error);
  }

  /* Full alter table */
  restore_record(table,2);			// Empty record for DEFAULT
  List_iterator<Alter_drop> drop_it(drop_list);
  List_iterator<create_field> def_it(fields);
  List_iterator<Alter_column> alter_it(alter_list);
  List<create_field> create_list;		// Add new fields here
  List<Key> key_list;				// Add new keys here

  /*
  ** First collect all fields from table which isn't in drop_list
  */

  create_field *def;
  Field **f_ptr,*field;
  for (f_ptr=table->field ; (field= *f_ptr) ; f_ptr++)
  {
    /* Check if field should be droped */
    Alter_drop *drop;
    drop_it.rewind();
    while ((drop=drop_it++))
    {
      if (drop->type == Alter_drop::COLUMN &&
	  !my_strcasecmp(field->field_name, drop->name))
	break;
    }
    if (drop)
    {
      drop_it.remove();
      continue;
    }
    /* Check if field is changed */
    def_it.rewind();
    while ((def=def_it++))
    {
      if (def->change && !my_strcasecmp(field->field_name, def->change))
	break;
    }
    if (def)
    {						// Field is changed
      def->field=field;
      if (def->sql_type == FIELD_TYPE_TIMESTAMP)
	use_timestamp=1;
      create_list.push_back(def);
      def_it.remove();
    }
    else
    {						// Use old field value
      create_list.push_back(def=new create_field(field));
      if (def->sql_type == FIELD_TYPE_TIMESTAMP)
	use_timestamp=1;

      alter_it.rewind();			// Change default if ALTER
      Alter_column *alter;
      while ((alter=alter_it++))
      {
	if (!my_strcasecmp(field->field_name, alter->name))
	  break;
      }
      if (alter)
      {
	def->def=alter->def;			// Use new default
	alter_it.remove();
      }
    }
  }
  def_it.rewind();
  List_iterator<create_field> find_it(create_list);
  while ((def=def_it++))			// Add new columns
  {
    if (def->change)
    {
      my_error(ER_BAD_FIELD_ERROR,MYF(0),def->change,table_name);
      DBUG_RETURN(-1);
    }
    if (!def->after)
      create_list.push_back(def);
    else if (def->after == first_keyword)
      create_list.push_front(def);
    else
    {
      create_field *find;
      find_it.rewind();
      while ((find=find_it++))			// Add new columns
      {
	if (!my_strcasecmp(def->after, find->field_name))
	  break;
      }
      if (!find)
      {
	my_error(ER_BAD_FIELD_ERROR,MYF(0),def->after,table_name);
	DBUG_RETURN(-1);
      }
      find_it.after(def);			// Put element after this
    }
  }
  if (alter_list.elements)
  {
    my_error(ER_BAD_FIELD_ERROR,MYF(0),alter_list.head()->name,table_name);
    DBUG_RETURN(-1);
  }
  if (!create_list.elements)
  {
    my_error(ER_CANT_REMOVE_ALL_FIELDS,MYF(0));
    DBUG_RETURN(-1);
  }

  /*
  ** Collect all keys which isn't in drop list. Add only those
  ** for which some fields exists.
  */

  List_iterator<Key> key_it(keys);
  List_iterator<create_field> field_it(create_list);
  List<key_part_spec> key_parts;

  KEY *key_info=table->key_info;
  for (uint i=0 ; i < table->keys ; i++,key_info++)
  {
    if (drop_primary && !key_info->dupp_key)
    {
      drop_primary=0;
      continue;
    }

    char *key_name=key_info->name;
    Alter_drop *drop;
    drop_it.rewind();
    while ((drop=drop_it++))
    {
      if (drop->type == Alter_drop::KEY &&
	  !my_strcasecmp(key_name, drop->name))
	break;
    }
    if (drop)
    {
      drop_it.remove();
      continue;
    }

    KEY_PART_INFO *key_part= key_info->key_part;
    key_parts.empty();
    for (uint j=0 ; j < key_info->key_parts ; j++,key_part++)
    {
      if (!key_part->field)
	continue;				// Wrong field (from UNIREG)
      char *key_part_name=key_part->field->field_name;
      create_field *field;
      field_it.rewind();
      while ((field=field_it++))
      {
	if (field->change)
	{
	  if (!my_strcasecmp(key_part_name, field->change))
	    break;
	}
	else if (!my_strcasecmp(key_part_name, field->field_name))
	    break;
      }
      if (!field)
	continue;				// Field is removed
      uint key_part_length=key_part->length;
      if (field->field)				// Not new field
      {						// Check if sub key
	if (field->field->pack_length() == key_part_length ||
	    field->length != field->pack_length)
	  key_part_length=0;			// Use whole field
      }
      key_parts.push_back(new key_part_spec(field->field_name,
					    key_part_length));
    }
    if (key_parts.elements)
      key_list.push_back(new Key(key_info->dupp_key ? Key::MULTIPLE :
				 Key::UNIQUE,
				 key_name,key_parts));
  }
  key_it.rewind();
  {
    Key *key;
    while ((key=key_it++))			// Add new keys
      key_list.push_back(key);
  }

  if (drop_list.elements)
  {
    my_error(ER_CANT_DROP_FIELD_OR_KEY,MYF(0),drop_list.head()->name);
    goto err;
  }
  if (alter_list.elements)
  {
    my_error(ER_CANT_DROP_FIELD_OR_KEY,MYF(0),alter_list.head()->name);
    goto err;
  }

  (void) sprintf(tmp_name,"A-%lx", thd->thread_id);
  if ((error=mysql_create_table(thd,new_db,tmp_name,create_list,key_list,1)))
    DBUG_RETURN(error);
  if (!(new_table=open_tmp_table(thd,new_db,tmp_name)))
  {
    VOID(quick_rm_table(thd,DB_TYPE_ISAM,new_db,tmp_name));
    goto err;
  }

  save_time_stamp=new_table->time_stamp;
  if (use_timestamp)
    new_table->time_stamp=0;
  new_table->next_number_field=new_table->found_next_number_field;
  thd->count_cuted_fields=1;			/* calc cuted fields */
  thd->cuted_fields=0L;
  thd->proc_info="copy to tmp table";
  error=copy_data_between_tables(table,new_table,create_list,handle_duplicates,
				 &copied,&deleted);
  thd->count_cuted_fields=0;			/* Don`t calc cuted fields */
  new_table->time_stamp=save_time_stamp;

  intern_close_table(new_table);		/* close temporary table */
  my_free((gptr) new_table,MYF(0));
  VOID(pthread_mutex_lock(&LOCK_open));
  if (error)
  {
    VOID(quick_rm_table(thd,DB_TYPE_ISAM,new_db,tmp_name));
    VOID(pthread_mutex_unlock(&LOCK_open));
    goto err;
  }
  /*
  ** Data is copied.  Now we rename the old table to a temp name,
  ** rename the new one to the old name, remove all entries from the old table
  ** from the cash, free all locks, close the old table and remove it.
  */

  thd->proc_info="rename result table";
  sprintf(old_name,"B-%lx", thd->thread_id);
  if (new_name != table_name)
  {
    if (!access(new_name_buff,F_OK))
    {
      error=1;
      my_error(ER_TABLE_EXISTS_ERROR,MYF(0),new_name_buff);
      VOID(quick_rm_table(thd,DB_TYPE_ISAM,new_db,tmp_name));
      VOID(pthread_mutex_unlock(&LOCK_open));
      goto err;
    }
  }

#ifdef __WIN32__
  // Win32 can't rename an open table, so we must close the org table!
  table_name=sql_strdup(table_name);		// must be saved
  if (close_cached_table(thd,table))
  {						// Aborted
    VOID(quick_rm_table(thd,DB_TYPE_ISAM,new_db,tmp_name));
    VOID(pthread_mutex_unlock(&LOCK_open));
    goto err;
  }
  table=0;					// Marker for win32 version
#endif

  error=0;
  if (mysql_rename_table(thd,db_type,db,table_name,db,old_name))
  {
    error=1;
    VOID(quick_rm_table(thd,DB_TYPE_ISAM,new_db,tmp_name));
  }
  else if (mysql_rename_table(thd,DB_TYPE_ISAM,new_db,tmp_name,new_db,
			      new_name))
  {						// Try to get everything back
    error=1;
    VOID(quick_rm_table(thd,DB_TYPE_ISAM,new_db,new_name));
    VOID(quick_rm_table(thd,DB_TYPE_ISAM,new_db,tmp_name));
    VOID(mysql_rename_table(thd,db_type,db,old_name,db,table_name));
  }
  if (error)
  {
    // This shouldn't happen.  We solve this the safe way by
    // closing the locked table.
    close_cached_table(thd,table);
    VOID(pthread_mutex_unlock(&LOCK_open));
    goto err;
  }
  if (thd->lock || new_name != table_name)	// True if WIN32
  {
    // Not table locking or alter table with rename
    // free locks and remove old table
    close_cached_table(thd,table);
    VOID(quick_rm_table(thd,db_type,db,old_name));
  }
  else
  {
    // Using LOCK TABLES without rename.
    // This code is never executed on WIN32!
    // Remove old renamed table, reopen table and get new locks
    if (table)
    {
      VOID(ha_extra(table,HA_EXTRA_FORCE_REOPEN)); // Use new file
      mysql_lock_abort(table);			 // end threads waiting on lock
      remove_table_from_cache(db,table_name);	 // Mark all in-use copies old
    }
    VOID(quick_rm_table(thd,db_type,db,old_name));
    if (close_data_tables(thd,db,table_name) ||
	reopen_tables(thd,1,0))
    {						// This shouldn't happen
      close_cached_table(thd,table);		// Remove lock for table
      VOID(pthread_mutex_unlock(&LOCK_open));
      goto err;
    }
  }

  thd->proc_info="end";
  mysql_update_log.write(thd->query);
  VOID(pthread_cond_broadcast(&COND_refresh));
  VOID(pthread_mutex_unlock(&LOCK_open));

  sprintf(tmp_name,ER(ER_INSERT_INFO),copied+deleted,deleted,
	  thd->cuted_fields);
  send_ok(&thd->net,copied+deleted,0L,tmp_name);
  thd->some_tables_deleted=0;
  DBUG_RETURN(0);

 err:
  DBUG_RETURN(-1);
}


static int
copy_data_between_tables(TABLE *from,TABLE *to,List<create_field> &create,
			 enum enum_duplicates handle_duplicates,
			 ulong *copied,ulong *deleted)
{
  int error;
  Copy_field *copy,*copy_end;
  ulong found_count,delete_count;
  DBUG_ENTER("copy_data_between_tables");

  ha_lock(to,F_WRLCK);
  ha_extra(to,HA_EXTRA_WRITE_CACHE);

  if (!(copy= new Copy_field[to->fields]))
    DBUG_RETURN(-1);				/* purecov: inspected */

  List_iterator<create_field> it(create);
  create_field *def;
  copy_end=copy;
  for (Field **ptr=to->field ; *ptr ; ptr++)
  {
    def=it++;
    if (def->field)
      (copy_end++)->set(*ptr,def->field,0);
  }

  READ_RECORD info;
  init_read_record(&info, from, (SQL_SELECT *) 0);

  TABLE *error_table=from;
  found_count=delete_count=0;
  Field *next_field=to->next_number_field;
  while ((error=info.read_record(&info)) <= 0)
  {
    if (!error)
    {
      if (next_field)
	next_field->reset();
      for (Copy_field *ptr=copy ; ptr != copy_end ; ptr++)
	ptr->do_copy(ptr);
      if ((error=ha_write(to,(byte*) to->record[0])))
      {
	if (handle_duplicates != DUP_IGNORE ||
	    error != HA_ERR_FOUND_DUPP_KEY)
	{
	  error_table=to;
	  break;
	}
	delete_count++;
      }
      else
	found_count++;
    }
  }
  end_read_record(&info);
  delete [] copy;
  uint tmp_error;
  if ((tmp_error=ha_extra(to,HA_EXTRA_NO_CACHE)))
  {
    error=tmp_error;
    error_table=to;
  }
  if (error != HA_ERR_END_OF_FILE)
  {
    ha_error(error_table,error,MYF(0));
    DBUG_RETURN(-1);
  }
  *copied= found_count;
  *deleted=delete_count;
  DBUG_RETURN(0);
}
