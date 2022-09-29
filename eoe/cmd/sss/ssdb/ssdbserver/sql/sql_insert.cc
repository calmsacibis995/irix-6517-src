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

/* Insert of records */

#include "mysql_priv.h"
#include "sql_acl.h"

static int check_null_fields(THD *thd,TABLE *entry);

/*
  Check if insert fields are correct
  Resets form->time_stamp if a timestamp value is set
*/

static int
check_insert_fields(THD *thd,TABLE *table,List<Item> &fields,
		    List<Item> &values, ulong counter)
{
  if ((fields.elements == 0))
  {
    if (values.elements != table->fields)
    {
      my_printf_error(ER_WRONG_VALUE_COUNT_ON_ROW,
		      ER(ER_WRONG_VALUE_COUNT_ON_ROW),
		      MYF(0),counter);
      return -1;
    }
    if (grant_option &&
	check_grant_all_columns(thd,INSERT_ACL,table))
      return -1;
    table->time_stamp=0;			// This should be saved
  }
  else
  {						// Part field list
    if (fields.elements != values.elements)
    {
      my_printf_error(ER_WRONG_VALUE_COUNT_ON_ROW,
		      ER(ER_WRONG_VALUE_COUNT_ON_ROW),
		      MYF(0),counter);
      return -1;
    }
    TABLE_LIST table_list;
    bzero((char*) &table_list,sizeof(table_list));
    table_list.name=table->table_name;
    table_list.table=table;
    table_list.grant=table->grant;

    thd->dupp_field=0;
    if (setup_fields(thd,&table_list,fields,1,0))
      return -1;
    if (thd->dupp_field)
    {
      my_error(ER_FIELD_SPECIFIED_TWICE,MYF(0), thd->dupp_field->field_name);
      return -1;
    }
    if (table->timestamp_field &&	// Don't set timestamp if used
	table->timestamp_field->query_id == thd->query_id)
      table->time_stamp=0;		// This should be saved
  }
 // For the values we need select_priv
  table->grant.want_privilege=(SELECT_ACL & ~table->grant.privilege);
  return 0;
}


int mysql_insert(THD *thd,TABLE_LIST *table_list, List<Item> &fields,
		 List<List_item> &values_list,enum_duplicates dup,
		 bool low_priority)
{
  int error;
  uint value_count;
  ulong counter = 1;
  COPY_INFO info;
  ulonglong id;
  TABLE *table;
  uint save_time_stamp;
  List_iterator<List_item> its(values_list);
  List_item *values;
  DBUG_ENTER("mysql_insert");

  if (!(table = open_ltable(thd,table_list,
			    low_priority ? TL_WRITE_DELAYED : TL_WRITE)))
    DBUG_RETURN(-1);
  thd->proc_info="init";
  save_time_stamp=table->time_stamp;
  values= its++;
  if (check_insert_fields(thd,table,fields,*values,1) ||
      setup_fields(thd,table_list,*values,0,0))
  {
    table->time_stamp=save_time_stamp;
    DBUG_RETURN(-1);
  }
  value_count= values->elements;
  while (values = its++)
  {
    counter++;
    if (values->elements != value_count)
    {
      my_printf_error(ER_WRONG_VALUE_COUNT_ON_ROW,
		      ER(ER_WRONG_VALUE_COUNT_ON_ROW),
		      MYF(0),counter);
      table->time_stamp=save_time_stamp;
      DBUG_RETURN(-1);
    }
    if (setup_fields(thd,table_list,*values,0,0))
    {
      table->time_stamp=save_time_stamp;
      DBUG_RETURN(-1);
    }
  }
  its.rewind ();
  /*
  ** Fill in the given fields and dump it to the table file
  */

  info.records=info.deleted=info.copied=0;
  info.handle_duplicates=dup;
  // Don't count warnings for simple inserts 
  if (values_list.elements > 1 || (thd->options & OPTION_WARNINGS))
    thd->count_cuted_fields = 1;
  thd->cuted_fields = 0L;
  table->next_number_field=table->found_next_number_field;

  error=0;
  id=0;
  thd->proc_info="update";
  while (values = its++)
  {
    if (fields.elements)
    {
      restore_record(table,2);			// Get empty record
      if (fill_record(fields,*values) || check_null_fields(thd,table))
      {
	if (values_list.elements != 1)
	{
	  info.records++;
	  continue;
	}
	error=1;
	break;
      }
    }
    else
    {
      table->record[0][0]=table->record[2][0];	// Fix delete marker
      if (fill_record(table,*values))
      {
	if (values_list.elements != 1)
	{
	  info.records++;
	  continue;
	}
	error=1;
	break;
      }
    }
    if ((error=write_record(thd,table,&info)))
      break;
    /*
      If auto_increment values are used, save the first one
       for LAST_INSERT_ID() and for the update log.
    */
    if (! id && thd->insert_id_used)
    {						// Get auto increment value
      id= thd->insert_id();
    }
  }
  thd->proc_info="end";
  if (id && values_list.elements != 1)
    thd->insert_id(id);				// For update log
  else if (table->next_number_field)
    id=table->next_number_field->val_int();	// Return auto_increment value
  if (info.copied || info.deleted)
    mysql_update_log.write(thd->query);

  table->time_stamp=save_time_stamp;		// Restore auto timestamp pointer
  table->next_number_field=0;
  thd->count_cuted_fields=0;
  thd->next_insert_id=0;			// Reset this if wrongly used
  if (thd->lock)
  {
    mysql_unlock_tables(thd->lock);
    thd->lock=0;
  }
  if (error)
    DBUG_RETURN(-1);

  if (values_list.elements == 1 && (!(thd->options & OPTION_WARNINGS) ||
				    !thd->cuted_fields))
    send_ok(&thd->net,info.copied+info.deleted,id);
  else {
    char buff[160];
    if (dup == DUP_IGNORE)
      sprintf(buff,ER(ER_INSERT_INFO),info.records,info.records-info.copied,
	      thd->cuted_fields);
    else
      sprintf(buff,ER(ER_INSERT_INFO),info.records,info.deleted,
	      thd->cuted_fields);
    ::send_ok(&thd->net,info.copied+info.deleted,0L,buff);
  }
  DBUG_RETURN(0);
}


	/* Check if there is more uniq keys after field */

static int last_uniq_key(TABLE *table,uint keynr)
{
  while (++keynr < table->keys)
    if (!table->key_info[keynr].dupp_key)
      return 0;
  return 1;
}


/*
** Write a record to table with optional deleting of conflicting records
*/


int write_record(THD *thd,TABLE *table,COPY_INFO *info)
{
  int error,key_nr;
  char *key=0;

  info->records++;
  if (info->handle_duplicates == DUP_REPLACE)
  {
    while ((error=ha_write(table,table->record[0])))
    {
      if (error != HA_WRITE_SKIPP)
	goto err;
      if ((key_nr = ha_keyerror(table,error)) < 0)
      {
	error=HA_WRITE_SKIPP;			/* Database can't find key */
	goto err;
      }
      if (ha_extra(table,HA_EXTRA_FLUSH_CACHE)) /* Not neaded with NISAM */
      {
	error=my_errno;
	goto err;
      }
      if (!key)
	key=(char*) my_alloca(ha_max_key_length[table->db_type]);
      key_copy((byte*) key,table,(uint) key_nr,0);
      if ((error=ha_rkey(table,table->record[1],key_nr,(byte*) key,0,
			 HA_READ_KEY_EXACT)))
	goto err;
      if (last_uniq_key(table,key_nr))
      {
	if ((error=ha_update(table,table->record[1],table->record[0])))
	  goto err;
	info->deleted++;
	break;					/* Update logfile and count */
      }
      else if ((error=ha_delete(table,table->record[1])))
	goto err;
      info->deleted++;
    }
    info->copied++;
  }
  else if ((error=ha_write(table,table->record[0])))
  {
    if (info->handle_duplicates != DUP_IGNORE ||
	error != HA_ERR_FOUND_DUPP_KEY)
      goto err;
  }
  else
    info->copied++;
  if (key)
    my_afree(key);
  return 0;

err:
  my_afree(key);
  ha_error(table,error,MYF(0));
  return 1;
}


/******************************************************************************
	Check that all fields with arn't null_fields are used
	if DONT_USE_DEFAULT_FIELDS isn't defined use default value for not
	set fields.
******************************************************************************/

static int check_null_fields(THD *thd __attribute__((unused)),
			     TABLE *entry __attribute__((unused)))
{
#ifdef DONT_USE_DEFAULT_FIELDS
  for (Field **field=entry->field ; *field ; field++)
  {
    if ((*field)->query_id != thd->query_id && !(*field)->maybe_null() &&
	*field != entry->timestamp_field &&
	*field != entry->next_number_field)
    {
      my_printf_error(ER_BAD_NULL_ERROR, ER(ER_BAD_NULL_ERROR),MYF(0),
		      (*field)->field_name);
      return 1;
    }
  }
#endif
  return 0;
}


/***************************************************************************
** store records in INSERT ... SELECT *
***************************************************************************/

int
select_insert::prepare(List<Item> &values)
{
  TABLE *form=table;
  DBUG_ENTER("select_insert::prepare");

  if (check_insert_fields(thd,table,*fields,values,1))
    DBUG_RETURN(1);

  if (fields->elements)
  {
    restore_record(form,2);			// Get empty record
  }
  else
    form->record[0][0]=form->record[2][0];	// Fix delete marker
  form->next_number_field=form->found_next_number_field;
  thd->count_cuted_fields=1;			/* calc cuted fields */
  thd->cuted_fields=0;
  ha_extra(form,HA_EXTRA_WRITE_CACHE);
  DBUG_RETURN(0);
}

select_insert::~select_insert()
{
  table->time_stamp=save_time_stamp;
  table->next_number_field=0;
  thd->count_cuted_fields=0;
}


bool select_insert::send_data(List<Item> &values)
{
  if (thd->offset_limit)
  {						// using limit offset,count
    thd->offset_limit--;
    return 0;
  }
  if (fields->elements)
    fill_record(*fields,values);
  else
    fill_record(table,values);
  if (write_record(thd,table,&info))
    return 1;
  if (table->next_number_field)		// Clear for next record
    table->next_number_field->reset();
  return 0;
}


void select_insert::send_error(uint errcode,char *err)
{
  ::send_error(&thd->net,errcode,err);
  VOID(ha_extra(table,HA_EXTRA_NO_CACHE));
}


void select_insert::send_eof()
{
  int error;
  if ((error=ha_extra(table,HA_EXTRA_NO_CACHE)))
  {
    ha_error(table,error,MYF(0));
    ::send_error(&thd->net);
  }
  else
  {
    char buff[160];
    if (info.handle_duplicates == DUP_IGNORE)
      sprintf(buff,ER(ER_INSERT_INFO),info.records,info.records-info.copied,
	      thd->cuted_fields);
    else
      sprintf(buff,ER(ER_INSERT_INFO),info.records,info.deleted,
	      thd->cuted_fields);
    ::send_ok(&thd->net,info.copied,0L,buff);
    mysql_update_log.write(thd->query);
  }
}

/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef __GNUC__
template class List_iterator<List_item>;
#endif
