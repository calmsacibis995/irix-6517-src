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

/* locking functions for mysql */

#include "mysql_priv.h"
#include <hash.h>
#ifndef MASTER
#include "../srclib/merge/mrgdef.h"		/* Includes isam & thr_lock */
#else
#include "../merge/mrgdef.h"
#endif

extern HASH open_cache;

static MYSQL_LOCK *get_lock_data(TABLE **table,uint count,bool unlock=0);
static int lock_databases(TABLE **table,uint count);
static int unlock_databases(TABLE **table,uint count);


MYSQL_LOCK *mysql_lock_tables(THD *thd,TABLE **tables,uint count)
{
  MYSQL_LOCK *sql_lock;
  thd->locked=1;
  for (;;)
  {
    if ((sql_lock = get_lock_data(tables,count)))
    {
      if (thr_multi_lock(sql_lock->locks,sql_lock->lock_count))
      {
	thd->some_tables_deleted=1;		// Try again
      }
      else
      {
	thd->proc_info="System lock";
	if (lock_databases(tables,count))
	{
	  thr_multi_unlock(sql_lock->locks,sql_lock->lock_count);
	  my_free((gptr) sql_lock,MYF(0));
	  sql_lock=0;
	  thd->proc_info=0;
	  break;
	}
	thd->proc_info=0;
      }
    }
    if (!thd->some_tables_deleted)
      break;
    /* some table was altered or deleted. reopen tables marked deleted */

    mysql_unlock_tables(sql_lock);
    sql_lock=0;
    if (wait_for_tables(thd))
      break;					// Couldn't open tables
  }
  thd->locked=0;
  if (thd->killed)
  {
    my_error(ER_SERVER_SHUTDOWN,MYF(0));
    if (sql_lock)
    {
      mysql_unlock_tables(sql_lock);
      sql_lock=0;
    }
  }
  return sql_lock;
}


static int lock_databases(TABLE **tables,uint count)
{
  reg1 uint i;
  int lock_type,error;
  DBUG_ENTER("lock_databases");

  for (i=1 ; i <= count ; i++, tables++)
  {
    lock_type=F_WRLCK;				/* Lock exclusive */
    if ((*tables)->db_stat & HA_READ_ONLY ||
	(*tables)->reginfo.lock_type == TL_READ ||
	(*tables)->reginfo.lock_type == TL_READ_HIGH_PRIORITY)
      lock_type=F_RDLCK;
    if ((error=ha_lock((*tables),lock_type)))
    {
      for ( ; i-- ; tables--)
	VOID(ha_lock((*tables),F_UNLCK));
      my_error(ER_CANT_LOCK,MYF(ME_BELL+ME_OLDWIN+ME_WAITTANG),error);
      DBUG_RETURN(error);
    }
    else
      (*tables)->db_stat &= ~ HA_BLOCK_LOCK;
  }
  DBUG_RETURN(0);
}

  
void mysql_unlock_tables(MYSQL_LOCK *sql_lock)
{
  VOID(unlock_databases(sql_lock->table,sql_lock->table_count));
  thr_multi_unlock(sql_lock->locks,sql_lock->lock_count);
  my_free((gptr) sql_lock,MYF(0));
}

/*
  Unlock some of the tables locked by mysql_lock_tables
  This will work even if get_lock_data fails (next unlock will free all)
  */

void mysql_unlock_some_tables(TABLE **table,uint count)
{
  MYSQL_LOCK *sql_lock;
  if ((sql_lock = get_lock_data(table,count,1)))
    mysql_unlock_tables(sql_lock);
  VOID(unlock_databases(table,count));
}


/*
** unlock all tables locked for read.
*/

void mysql_unlock_read_tables(MYSQL_LOCK *sql_lock)
{
  uint i,found;
  /* Move all write locked tables first */
  TABLE **table=sql_lock->table;
  
  for (i=found=0 ; i < sql_lock->table_count ; i++)
  {
    if ((uint) sql_lock->table[i]->reginfo.lock_type >= TL_WRITE_ALLOW_READ)
    {
      swap(TABLE *,*table,sql_lock->table[i]);
      table++;
      found++;
    }
  }
  /* Unlock all read locked tables */
  if (i != found)
  {
    VOID(unlock_databases(table,i-found));
    sql_lock->table_count-=found;
  }

  /* Do the same thing to MySQL memory locks */
  THR_LOCK_DATA **lock=sql_lock->locks;
  for (i=found=0 ; i < sql_lock->lock_count ; i++)
  {
    if (sql_lock->locks[i]->type >= TL_WRITE_ALLOW_READ)
    {
      swap(THR_LOCK_DATA *,*lock,sql_lock->locks[i]);
      lock++;
      found++;
    }
  }      
  if (i != found)
  {
    thr_multi_unlock(lock,sql_lock->lock_count-found);
    sql_lock->lock_count-=found;
  }
}



void mysql_lock_remove(MYSQL_LOCK *locked,TABLE *table)
{
  mysql_unlock_some_tables(&table,1);
  reg1 uint i;
  for (i=0; i < locked->table_count; i++)
  {
    if (locked->table[i] == table)
    {
      locked->table_count--;
      bmove((char*) (locked->table+i),
	    (char*) (locked->table+i+1),
	    (locked->table_count-i)* sizeof(TABLE*));
      break;
    }
  }
  THR_LOCK_DATA **prev=locked->locks;
  for (i=0 ; i < locked->lock_count ; i++)
  {
    if (locked->locks[i]->type != TL_UNLOCK)
      *prev++ = locked->locks[i];
  }
  locked->lock_count=(prev - locked->locks);
}

/* abort all other threads waiting to get lock in table */

void mysql_lock_abort(TABLE *table)
{
  MYSQL_LOCK *locked;
  if ((locked = get_lock_data(&table,1,1)))
  {
    for (uint i=0; i < locked->table_count; i++)
      thr_abort_locks(locked->locks[i]->lock);
    my_free((gptr) locked,MYF(0));
  }
}


MYSQL_LOCK *mysql_lock_merge(MYSQL_LOCK *a,MYSQL_LOCK *b)
{
  MYSQL_LOCK *sql_lock;
  DBUG_ENTER("mysql_lock_merge");
  if (!(sql_lock= (MYSQL_LOCK*)
	my_malloc(sizeof(*sql_lock)+
		  sizeof(THR_LOCK_DATA*)*(a->lock_count+b->lock_count)+
		  sizeof(TABLE*)*(a->table_count+b->table_count),MYF(MY_WME))))
    DBUG_RETURN(0);				// Fatal error
  sql_lock->lock_count=a->lock_count+b->lock_count;
  sql_lock->table_count=a->table_count+b->table_count;
  sql_lock->locks=(THR_LOCK_DATA**) (sql_lock+1);
  sql_lock->table=(TABLE**) (sql_lock->locks+sql_lock->lock_count);
  memcpy(sql_lock->locks,a->locks,a->lock_count*sizeof(*a->locks));
  memcpy(sql_lock->locks+a->lock_count,b->locks,
	 b->lock_count*sizeof(*b->locks));
  memcpy(sql_lock->table,a->table,a->table_count*sizeof(*a->table));
  memcpy(sql_lock->table+a->table_count,b->table,
	 b->table_count*sizeof(*b->table));
  my_free((gptr) a,MYF(0));
  my_free((gptr) b,MYF(0));
  return sql_lock;
}


	/* unlock a set of databases */

static int unlock_databases(TABLE **table,uint count)
{
  int error,error_code;
  DBUG_ENTER("unlock_databases");

  error_code=0;
  for (; count-- ; table++)
  {
    if ((error=ha_lock((*table),F_UNLCK)))
      error_code=error;
  }
  if (error_code)
    my_error(ER_CANT_LOCK,MYF(ME_BELL+ME_OLDWIN+ME_WAITTANG),error_code);
  DBUG_RETURN(error_code);
}


/*
** Get lock structures from table structs and initialize locks
*/


static MYSQL_LOCK *get_lock_data(TABLE **table,uint count,bool get_old_locks)
{
  uint i,tables;
  enum thr_lock_type lock_type;
  MYSQL_LOCK *sql_lock;
  THR_LOCK_DATA **locks;

  for (i=tables=0 ; i < count ; i++)
  {
    if (table[i]->db_type == DB_TYPE_MRG_ISAM)
    {
      MRG_INFO *db= (MRG_INFO*) table[i]->file;
      tables+=db->tables;
    }
    else if (table[i]->db_type == DB_TYPE_ISAM)
      tables++;
  }

  if (!(sql_lock= (MYSQL_LOCK*)
	my_malloc(sizeof(*sql_lock)+
		  sizeof(THR_LOCK_DATA*)*tables+sizeof(table)*count,MYF(0))))
    return 0;
  locks=sql_lock->locks=(THR_LOCK_DATA**) (sql_lock+1);
  sql_lock->table=(TABLE**) (locks+tables);
  sql_lock->table_count=count;
  sql_lock->lock_count=tables;
  memcpy(sql_lock->table,table,sizeof(table[0])*count);

  for (i=0 ; i < count ; i++)
  {
    lock_type= table[i]->reginfo.lock_type;

    if (table[i]->db_type == DB_TYPE_MRG_ISAM)
    {
      MRG_INFO *db= (MRG_INFO*) table[i]->file;
      MRG_TABLE *file;

      for (file=db->open_tables ; file != db->end_table ; file++)
      {
	*(locks++)= &file->table->lock;
	if (file->table->lock.type == TL_UNLOCK && !get_old_locks)
	  file->table->lock.type=lock_type;
      }
    }
    else if (table[i]->db_type == DB_TYPE_ISAM)
    {
      N_INFO *db=(N_INFO*) table[i]->file;
      *(locks++)= &db->lock;
      if (db->lock.type == TL_UNLOCK && !get_old_locks)
	db->lock.type=lock_type;
    }
  }
  return sql_lock;
}
