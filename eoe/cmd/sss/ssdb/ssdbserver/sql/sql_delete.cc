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

/* Delete of records */

#include "mysql_priv.h"

/*
  Optimize delete of all rows by doing a full generate of the table
  This will work even if the .ISM and .ISD tables are destroyed
*/

static int generate_table(THD *thd, TABLE_LIST *table_list,
			  TABLE *locked_table)
{
  char path[FN_REFLEN];
  int error;
  DBUG_ENTER("generate_table");

  thd->proc_info="generate_table";
  (void) sprintf(path,"%s/%s/%s%s",mysql_data_home,table_list->db,
		 table_list->real_name,reg_ext);

  VOID(pthread_mutex_lock(&LOCK_open));
  if (locked_table)
    mysql_lock_abort(locked_table);		 // end threads waiting on lock
  // close all copies in use
  if (remove_table_from_cache(table_list->db,table_list->real_name))
  {
    if (!locked_table)
    {
      VOID(pthread_mutex_unlock(&LOCK_open));
      DBUG_RETURN(1);				// We must get a lock on table
    }
  }
  if (locked_table)
    VOID(ha_extra(locked_table,HA_EXTRA_FORCE_REOPEN)); // Close all NISAM files
  close_data_tables(thd,table_list->db,table_list->real_name);
  error= cre_database(path) ? -1 : 0;
  if (reopen_tables(thd,1,0))
    error= -1;
  VOID(pthread_mutex_unlock(&LOCK_open));
  if (!error)
  {
    send_ok(&thd->net);		// This should return record count
    mysql_update_log.write(thd->query);
  }
  DBUG_RETURN(error ? -1 : 0);
}


int mysql_delete(THD *thd,TABLE_LIST *table_list,COND *conds,ha_rows limit,
		 bool low_priority)
{
  int		error;
  TABLE		*table;
  SQL_SELECT	*select;
  READ_RECORD	info;
  DBUG_ENTER("mysql_delete");

  if (!table_list->db)
    table_list->db=thd->db;
  if (!conds && ! thd->open_tables)
  {
    error=generate_table(thd,table_list,(TABLE*) 0);
    if (error <= 0)
      DBUG_RETURN(error);			// Error or ok
  }
  if (!(table = open_ltable(thd,table_list,
			    low_priority || limit != HA_POS_ERROR ?
			    TL_WRITE_DELAYED : TL_WRITE)))
    DBUG_RETURN(-1);
  thd->proc_info="init";
  if (!conds)
    DBUG_RETURN(generate_table(thd,table_list,table));
  table->map=1;
  if (setup_conds(thd,table_list,conds))
    DBUG_RETURN(-1);

  table->used_keys=0;				// Can't use 'only index'
  select=make_select(&table,1,0,1,conds,&error);
  if (error)
    DBUG_RETURN(-1);
  if (select && select->check_quick())
  {
    delete select;
    send_ok(&thd->net,0L);
    DBUG_RETURN(0);
  }

  VOID(ha_extra(table,HA_EXTRA_NO_READCHECK));
  init_read_record(&info,table,select,0);
  ulong deleted=0L;
  thd->proc_info="updating";
  while ((error=info.read_record(&info)) <= 0 && !thd->killed)
  {
    if (error == 0 && !(select && select->skipp_record()))
    {
      if (!(error=ha_delete(table,table->record[0])))
      {
	deleted++;
	if (limit != HA_POS_ERROR && !--limit)
	{
	  error=HA_ERR_END_OF_FILE;
	  break;
	}
      }
      else
      {
	ha_error(table,error,MYF(0));
	error=0;
	break;
      }
    }
  }
  thd->proc_info="end";
  end_read_record(&info);
  VOID(ha_extra(table,HA_EXTRA_READCHECK));
  if (deleted)
    mysql_update_log.write(thd->query);
  if (thd->lock)
  {
    mysql_unlock_tables(thd->lock);
    thd->lock=0;
  }
  delete select;
  if (error != HA_ERR_END_OF_FILE)
  {
    ha_error(table,error,MYF(0));	/* purecov: inspected */
    error= -1;				/* purecov: inspected */
  }
  if (error <= 0)
    send_error(&thd->net,thd->killed ? ER_SERVER_SHUTDOWN: 0);
  else
  {
    send_ok(&thd->net,deleted);
    DBUG_PRINT("info",("%d records deleted",deleted));
  }
  DBUG_RETURN(0);
}
