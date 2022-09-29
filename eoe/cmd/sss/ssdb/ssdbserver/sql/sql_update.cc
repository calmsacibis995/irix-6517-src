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

/* Update of records */

#include "mysql_priv.h"
#include "sql_acl.h"

int mysql_update(THD *thd,TABLE_LIST *table_list,List<Item> &fields,
		 List<Item> &values, COND *conds,bool low_priority)
{
  TABLE		*table;
  int		error;
  uint		save_time_stamp;
  SQL_SELECT	*select;
  READ_RECORD	info;
  DBUG_ENTER("mysql_update");

  if (!(table = open_ltable(thd,table_list,
			    low_priority ? TL_WRITE_DELAYED : TL_WRITE)))
    DBUG_RETURN(-1);
  save_time_stamp=table->time_stamp;
  ha_info(table,2);			// sync record count
  thd->proc_info="init";

  /*
  ** Find the offsets of the given fields and condition
  */

  if (setup_fields(thd,table_list,fields,1,0))
    DBUG_RETURN(-1);
  table->grant.want_privilege=(SELECT_ACL & ~table->grant.privilege);
  if (table->timestamp_field &&			// Don't set timestamp if used
      table->timestamp_field->query_id == thd->query_id)
    table->time_stamp=0;
  if (setup_fields(thd,table_list,values,0,0) ||
      setup_conds(thd,table_list,conds))
  {
    table->time_stamp=save_time_stamp;		// Restore timestamp pointer
    DBUG_RETURN(-1);				/* purecov: inspected */
  }
  table->used_keys=0;				// Can't use 'only index'
  select=make_select(&table,1,0,0,conds,&error);
  if (error || (select && select->check_quick()))	/* No records */
  {
    delete select;
    table->time_stamp=save_time_stamp;		// Restore timestamp pointer
    if (error)
    {
      DBUG_RETURN(-1);				// Error in where
    }
    send_ok(&thd->net);				// No matching records
    DBUG_RETURN(0);
  }
  if (!(test_flags & TEST_READCHECK))		/* For debugging */
    VOID(ha_extra(table,HA_EXTRA_NO_READCHECK));
  init_read_record(&info,table,select,0);

  ulong updated=0L,found=0L;
  thd->count_cuted_fields=1;			/* calc cuted fields */
  thd->cuted_fields=0L;
  thd->proc_info="updating";
  while ((error=info.read_record(&info)) <= 0 && !thd->killed)
  {
    if (error == 0 && !(select && select->skipp_record()))
    {
      store_record(table,1);
      if (fill_record(fields,values))
	break;
      found++;
      if (cmp_record(table,1))
      {
	if (!(error=ha_update(table,
			      (byte*) table->record[1],
			      (byte*) table->record[0])))
	  updated++;
	else
	{
	  ha_error(table,error,MYF(0));
	  error= -1;
	  break;
	}
      }
    }
  }
  thd->proc_info="end";
  end_read_record(&info);
  VOID(ha_extra(table,HA_EXTRA_READCHECK));
  table->time_stamp=save_time_stamp;	// Restore auto timestamp pointer
  if (updated)
    mysql_update_log.write(thd->query);
  if (thd->lock)
  {
    mysql_unlock_tables(thd->lock);
    thd->lock=0;
  }
  delete select;
  if (error <= 0)
    send_error(&thd->net,thd->killed ? ER_SERVER_SHUTDOWN : 0); /* purecov: inspected */
  else
  {
    char buff[80];
    sprintf(buff,ER(ER_UPDATE_INFO), found, updated, thd->cuted_fields);
    send_ok(&thd->net,
	    (thd->client_capabilities & CLIENT_FOUND_ROWS) ? found : updated,
	    thd->insert_id_used ? thd->insert_id() : 0L,buff);
    DBUG_PRINT("info",("%d records updated",updated));
  }
  thd->count_cuted_fields=0;			/* calc cuted fields */
  DBUG_RETURN(0);
}
