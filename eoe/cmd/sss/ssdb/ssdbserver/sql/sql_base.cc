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

/* Basic functions neaded by many modules */

#include "mysql_priv.h"
#include "sql_acl.h"
#include <m_ctype.h>
#include <my_dir.h>
#include <hash.h>
#include <nisam.h>
#ifdef	__WIN32__
#include <io.h>
#endif

#define MAX_DBKEY_LENGTH (FN_LEN*2+2)

static int key_cache_used=0;
TABLE *unused_tables;				/* Used by mysql_test */
HASH open_cache;				/* Used by mysql_test */


static int open_unireg_entry(TABLE *entry,const char *db,const char *name,
			     const char *alias);
static bool insert_fields(THD *thd,TABLE_LIST *tables, const char *table_name,
			  List_iterator<Item> *it);
static void free_cache_entry(TABLE *entry);
static void mysql_rm_tmp_tables(void);

static byte *cache_key(const byte *record,uint *length,my_bool not_used)
{
  TABLE *entry=(TABLE*) record;
  *length=entry->key_length;
  return (byte*) entry->table_cache_key;
}

void table_cache_init(void)
{
  VOID(hash_init(&open_cache,table_cache_size,0,0,cache_key,
		 (void (*)(void*)) free_cache_entry,0));
  mysql_rm_tmp_tables();
}


void table_cache_free(void)
{
  DBUG_ENTER("table_cache_free");
  close_cached_tables(0);
  if (!open_cache.records)			// Safety first
    hash_free(&open_cache);
  DBUG_VOID_RETURN;
}


uint cached_tables(void)
{
  return open_cache.records;
}

#ifdef EXTRA_DEBUG
static void check_unused(void)
{
  uint count=0,index=0;
  TABLE *link,*start_link;

  if ((start_link=link=unused_tables))
  {
    do
    {
      if (link != link->next->prev || link != link->prev->next)
      {
	DBUG_PRINT("error",("Unused_links aren't linked properly")); /* purecov: inspected */
	return; /* purecov: inspected */
      }
    } while (count++ < open_cache.records && (link=link->next) != start_link);
    if (link != start_link)
    {
      DBUG_PRINT("error",("Unused_links aren't connected")); /* purecov: inspected */
    }
  }
  for (index=0 ; index < open_cache.records ; index++)
  {
    TABLE *entry=(TABLE*) hash_element(&open_cache,index);
    if (!entry->in_use)
      count--;
  }
  if (count != 0)
  {
    DBUG_PRINT("error",("Unused_links dosen't match open_cache: diff: %d", /* purecov: inspected */
			count)); /* purecov: inspected */
  }
}
#else
#define check_unused()
#endif


/******************************************************************************
** Send name and type of result to client.
** Sum fields has table name empty and field_name.
******************************************************************************/

bool
send_fields(THD *thd,List<Item> &list,uint flag)
{
  List_iterator<Item> it(list);
  Item *item;
  char buff[80];
  String tmp((char*) buff,sizeof(buff)),*res,*packet= &thd->packet;

  if (flag & 1)
  {				// Packet with number of elements
    char *pos=net_store_length(buff,(ulonglong) list.elements);
    (void) my_net_write(&thd->net, buff,(uint) (pos-buff));
  }
  while ((item=it++))
  {
    char *pos;
    Send_field field;
    item->make_field(&field);
    packet->length(0);

    if (net_store_data(packet,field.table_name) ||
	net_store_data(packet,field.col_name) ||
	packet->realloc(packet->length()+10))
      goto err; /* purecov: inspected */
    pos= (char*) packet->ptr()+packet->length();

    if (!(thd->client_capabilities & CLIENT_LONG_FLAG))
    {
      packet->length(packet->length()+9);
      pos[0]=3; int3store(pos+1,field.length);
      pos[4]=1; pos[5]=field.type;
      pos[6]=2; pos[7]=(char) field.flags; pos[8]= (char) field.decimals;
    }
    else
    {
      packet->length(packet->length()+10);
      pos[0]=3; int3store(pos+1,field.length);
      pos[4]=1; pos[5]=field.type;
      pos[6]=3; int2store(pos+7,field.flags); pos[9]= (char) field.decimals;
    }
    if (flag & 2)
    {						// Send default value
      if (!(res=item->str(&tmp)))
      {
	if (net_store_null(packet))
	  goto err;
      }
      else if (net_store_data(packet,res->ptr(),res->length()))
	goto err;
    }
    if (my_net_write(&thd->net, (char*) packet->ptr(),packet->length()))
      break;					/* purecov: inspected */
  }
  send_eof(&thd->net,(test_flags & TEST_MIT_THREAD) ? 0: 1);
  return 0;
 err:
  send_error(&thd->net,ER_OUTOFMEMORY);		/* purecov: inspected */
  return 1;					/* purecov: inspected */
}


/*****************************************************************************
 *	 Functions to free open table cache
 ****************************************************************************/


void intern_close_table(TABLE *table)
{						// Free all structures
  free_io_cache(table);
  if (table->file)
    VOID(closefrm(table));			// close file
  x_free(table->table_cache_key);
}


static void free_cache_entry(TABLE *table)
{
  DBUG_ENTER("free_cache_entry");

  intern_close_table(table);
  if (!table->in_use)
  {
    table->next->prev=table->prev;		/* remove from used chain */
    table->prev->next=table->next;
    if (table == unused_tables)
    {
      unused_tables=unused_tables->next;
      if (table == unused_tables)
	unused_tables=0;
    }
    check_unused();				// consisty check
  }
  my_free((gptr) table,MYF(0));
  DBUG_VOID_RETURN;
}


void free_io_cache(TABLE *table)
{
  if (table->io_cache)
  {
    close_cacheed_file(table->io_cache);
    my_free((gptr) table->io_cache,MYF(0));
    table->io_cache=0;
  }
  if (table->record_pointers)
  {
    my_free((gptr) table->record_pointers,MYF(0));
    table->record_pointers=0;
  }
}

	/* Close all tables which aren't in use by any thread */

bool close_cached_tables(bool wait_for_refresh)
{
  bool result=0;
  DBUG_ENTER("close_cached_tables");

  VOID(pthread_mutex_lock(&LOCK_open));
  while (unused_tables)
#ifdef EXTRA_DEBUG
    if (hash_delete(&open_cache,(byte*) unused_tables))
      printf("Warning: Couldn't delete open table from hash\n");
#else
    VOID(hash_delete(&open_cache,(byte*) unused_tables));
#endif
  if (!open_cache.records)
  {
    end_key_cache();				/* No tables in memory */
    key_cache_used=0;
  }
  refresh_version++;				// Force close of open tables
  if (wait_for_refresh)
  {
    /*
      If there is any table that has a lower refresh_version, wait until
      this is closed (or this thread is killed) before returning
    */
    THD *thd=current_thd;
    pthread_mutex_lock(&thd->mysys_var->mutex);
    thd->mysys_var->current_mutex= &LOCK_open;
    thd->mysys_var->current_cond= &COND_refresh;
    thd->proc_info="Flushing tables";
    pthread_mutex_unlock(&thd->mysys_var->mutex);

    close_old_data_files(thd,thd->open_tables,1);
    bool found=1;
    while (found && ! thd->killed)
    {
      found=0;
      for (uint index=0 ; index < open_cache.records ; index++)
      {
	TABLE *table=(TABLE*) hash_element(&open_cache,index);
	if ((table->version) < refresh_version && table->db_stat)
	{
	  found=1;
	  pthread_cond_wait(&COND_refresh,&LOCK_open);
	  break;
	}
      }
    }
    result=reopen_tables(thd,1,1);
  }
  VOID(pthread_mutex_unlock(&LOCK_open));
  if (wait_for_refresh)
  {
    THD *thd=current_thd;
    pthread_mutex_lock(&thd->mysys_var->mutex);
    thd->mysys_var->current_mutex= 0;
    thd->mysys_var->current_cond= 0;
    thd->proc_info=0;
    pthread_mutex_unlock(&thd->mysys_var->mutex);
  }
  DBUG_RETURN(result);
}


/* Put all tables used by thread in free list */

void close_thread_tables(THD *thd)
{
  DBUG_ENTER("close_thread_tables");

  if (thd->locked_tables)
    DBUG_VOID_RETURN;				// LOCK TABLES in use

  TABLE *table,*next;
  bool found_old_table=0;

  if (thd->lock)
  {
    mysql_unlock_tables(thd->lock);
    thd->lock=0;
  }
  /* VOID(pthread_sigmask(SIG_SETMASK,&thd->block_signals,NULL)); */
  VOID(pthread_mutex_lock(&LOCK_open));

  for (table=thd->open_tables ; table ; table=next)
  {
    next=table->next;
    if (table->version != refresh_version ||
	thd->version != refresh_version)
    {
      VOID(hash_delete(&open_cache,(byte*) table));
      found_old_table=1;
    }
    else
    {
      table->in_use=0;
      if (unused_tables)
      {
	table->next=unused_tables;		/* Link in last */
	table->prev=unused_tables->prev;
	unused_tables->prev=table;
	table->prev->next=table;
      }
      else
	unused_tables=table->next=table->prev=table;
    }
  }
  thd->open_tables=0;
  /* Free tables to hold down open files */
  while (open_cache.records >= table_cache_size && unused_tables)
    VOID(hash_delete(&open_cache,(byte*) unused_tables)); /* purecov: tested */
  check_unused();
  if (found_old_table)
  {
    /* Tell threads waiting for refresh that something has happened */
    VOID(pthread_cond_broadcast(&COND_refresh));
  }
  thd->version=refresh_version;
  VOID(pthread_mutex_unlock(&LOCK_open));
  /*  VOID(pthread_sigmask(SIG_SETMASK,&thd->signals,NULL)); */
  DBUG_VOID_RETURN;
}

	/* move table first in unused links */

static void relink_unused(TABLE *table)
{
  if (table != unused_tables)
  {
    table->prev->next=table->next;		/* Remove from unused list */
    table->next->prev=table->prev;
    table->next=unused_tables;			/* Link in unused tables */
    table->prev=unused_tables->prev;
    unused_tables->prev->next=table;
    unused_tables->prev=table;
    unused_tables=table;
    check_unused();
  }
}


/*
  Remove all instances of table from the current open list
  Free all locks on tables that are done with LOCK TABLES
 */

TABLE *unlink_open_table(THD *thd, TABLE *list, TABLE *find)
{
  char key[MAX_DBKEY_LENGTH];
  uint key_length=find->key_length;
  TABLE *start=list,**prev,*next;
  prev= &start;
  memcpy(key,find->table_cache_key,key_length);
  for (; list ; list=next)
  {
    next=list->next;
    if (list->key_length == key_length &&
	!memcmp(list->table_cache_key,key,key_length))
    {
      if (thd->locked_tables)
	mysql_lock_remove(thd->locked_tables,list);
      VOID(hash_delete(&open_cache,(byte*) list)); // Close table
    }
    else
    {
      *prev=list;				// put in use list
      prev= &list->next;
    }
  }
  *prev=0;
  // Notify any 'refresh' threads
  pthread_cond_broadcast(&COND_refresh);
  return start;
}


/******************************************************************************
** open a table
** Uses a cache of open tables to find a table not in use.
******************************************************************************/


TABLE *open_table(THD *thd,const char *db,const char *table_name,
		  const char *alias,bool *refresh)
{
  reg1	TABLE *table;
  char	key[MAX_DBKEY_LENGTH];
  uint	key_length;
  DBUG_ENTER("open_table");

  /* find a unused table in the open table cache */
  *refresh=0;
  if (thd->killed)
    DBUG_RETURN(0);
  key_length=(uint) (strmov(strmov(key,db)+1,table_name)-key)+1;
  if (thd->locked_tables)
  {						// Using table locks
    for (table=thd->open_tables; table ; table=table->next)
    {
      if (table->key_length == key_length &&
	  !memcmp(table->table_cache_key,key,key_length) &&
	  !my_strcasecmp(table->table_name,alias))
	goto reset;
    }
    my_printf_error(ER_TABLE_NOT_LOCKED,ER(ER_TABLE_NOT_LOCKED),MYF(0),alias);
    DBUG_RETURN(0);
  }
  VOID(pthread_mutex_lock(&LOCK_open));

  if (!thd->open_tables)
    thd->version=refresh_version;
  else if (thd->version != refresh_version)
  {
    /* Someone did a refresh while thread was opening tables */
    *refresh=1;
    VOID(pthread_mutex_unlock(&LOCK_open));
    DBUG_RETURN(0);
  }

  for (table=(TABLE*) hash_search(&open_cache,(byte*) key,key_length) ;
       table && table->in_use ;
       table = (TABLE*) hash_next(&open_cache,(byte*) key,key_length))
  {
    if (table->version != refresh_version)
    {
      /*
      ** There is a refresh in progress for this table
      ** Wait until the table is freed or the thread is killed.
      */
      close_old_data_files(thd,thd->open_tables,0);
      if (table->in_use != thd)
      {
	/* Wait until the current table is up to date */
	char *proc_info;
	pthread_mutex_lock(&thd->mysys_var->mutex);
	thd->mysys_var->current_mutex= &LOCK_open;
	thd->mysys_var->current_cond= &COND_refresh;
	proc_info=thd->proc_info;
	thd->proc_info="Waiting for table";
	pthread_mutex_unlock(&thd->mysys_var->mutex);
	(void) pthread_cond_wait(&COND_refresh,&LOCK_open);

	pthread_mutex_unlock(&LOCK_open);	// Must be unlocked first
	pthread_mutex_lock(&thd->mysys_var->mutex);
	thd->mysys_var->current_mutex= 0;
	thd->mysys_var->current_cond= 0;
	thd->proc_info= proc_info;
	pthread_mutex_unlock(&thd->mysys_var->mutex);
      }
      else
	VOID(pthread_mutex_unlock(&LOCK_open));
      *refresh=1;
      DBUG_RETURN(0);
    }
  }
  if (table)
  {
    if (table == unused_tables)
    {						// First unused
      unused_tables=unused_tables->next;	// Remove from link
      if (table == unused_tables)
	unused_tables=0;
    }
    table->prev->next=table->next;		/* Remove from unused list */
    table->next->prev=table->prev;
    if (strcmp(table->table_name,alias))
    {
      uint i;
      my_free((gptr) table->table_name,MYF(0));
      table->table_name=my_strdup(alias,MYF(MY_WME));
      for (i=0 ; i < table->fields ; i++)
	table->field[i]->table_name=table->table_name;
    }
  }
  else
  {
    /* Free cache if too big */
    while (open_cache.records >= table_cache_size && unused_tables)
      VOID(hash_delete(&open_cache,(byte*) unused_tables)); /* purecov: tested */

    /* make a new table */
    if (!(table=(TABLE*) my_malloc(sizeof(*table),MYF(MY_WME))))
      DBUG_RETURN(NULL);
    if (open_unireg_entry(table,db,table_name,alias))
    {
      table->next=table->prev=table;
      free_cache_entry(table);
      VOID(pthread_mutex_unlock(&LOCK_open));
      DBUG_RETURN(NULL);
    }
    table->table_cache_key=my_memdup((byte*) key,key_length+1,MYF(MY_FAE));
    table->key_length=key_length;
    table->version=refresh_version;
    if (!key_cache_used)
    {
      key_cache_used=1;
      ha_key_cache();
    }
    VOID(hash_insert(&open_cache,(byte*) table));
  }

  table->in_use=thd;
  check_unused();
  VOID(pthread_mutex_unlock(&LOCK_open));
  table->next=thd->open_tables;		/* Link into simple list */
  thd->open_tables=table;
  table->reginfo.lock_type=TL_READ;		/* Assume read */

 reset:
  /* These variables are also set in reopen_table() */
  table->tablenr=thd->current_tablenr++;
  table->tmp_table = 0;
  table->used_fields=0;
  table->reginfo.ref_fields=0;		/* Some precation */
  table->const_table=0;
  table->outer_join=table->null_row=table->maybe_null=0;
  table->status=STATUS_NO_RECORD;
  table->used_keys= ((key_map) 1 << table->keys) - (key_map) 1;
  DBUG_RETURN(table);
}

/****************************************************************************
** Reopen an table because the definition has changed. The date file for the
** table is already closed.
** Returns 0 if ok.
** If table can't be reopened, the entry is unchanged.
****************************************************************************/

bool reopen_table(THD *thd,TABLE *table,bool locked)
{
  TABLE tmp;
  char *db=table->table_cache_key;
  char *table_name=table->real_name;
  DBUG_ENTER("reopen_table");

#ifdef EXTRA_DEBUG
  if (table->db_stat)
    sql_print_error("Table %s had a open data handler in reopen_table",
		    table->table_name);
#endif
  if (!locked)
    VOID(pthread_mutex_lock(&LOCK_open));

  if (open_unireg_entry(&tmp,db,table_name,table->table_name))
  {
    if (!locked)
     VOID(pthread_mutex_unlock(&LOCK_open));
    DBUG_RETURN(1);
  }
  free_io_cache(table);

  tmp.table_cache_key=db;
  tmp.key_length=table->key_length;
  tmp.in_use=table->in_use;
  tmp.reginfo.lock_type=table->reginfo.lock_type;
  tmp.version=refresh_version;
  tmp.next=table->next;
  tmp.prev=table->prev;

  /* This list copies varibles set by open_table */
  tmp.tablenr=		table->tablenr;
  tmp.tmp_table=	table->tmp_table;
  tmp.used_fields=	table->used_fields;
  tmp.reginfo.ref_fields=table->reginfo.ref_fields;
  tmp.const_table=	table->const_table;
  tmp.outer_join=	table->outer_join;
  tmp.null_row=		table->null_row;
  tmp.status=		table->status;
  tmp.used_keys=	table->used_keys;
  tmp.grant=		table->grant;

  if (table->file)
    VOID(closefrm(table));		// close file, free everything

  memcpy((char*) table,(char*) &tmp,sizeof(tmp));

  for (Field **field=table->field ; *field ; field++)
  {
    (*field)->table=table;
    (*field)->table_name=table->table_name;
  }
  for (uint key=0 ; key < table->keys ; key++)
    for (uint part=0 ; part < table->key_info[key].usable_key_parts ; part++)
      table->key_info[key].key_part[part].field->table=table;

  VOID(pthread_cond_broadcast(&COND_refresh));
  if (!locked)
    VOID(pthread_mutex_unlock(&LOCK_open));
  DBUG_RETURN(0);
}


/*
  Used with ALTER TABLE:
  Close all instanses of table when LOCK TABLES is in used;
  Close first all instances of table and then reopen them
 */

bool close_data_tables(THD *thd,const char *db, const char *table_name)
{
  TABLE *table;
  for (table=thd->open_tables; table ; table=table->next)
  {
    if (!strcmp(table->real_name,table_name) &&
	!strcmp(table->table_cache_key,db))
    {
      mysql_lock_remove(thd->locked_tables,table);
      ha_close(table);
      table->db_stat=0;
    }
  }
  return 0;					// For the future
}


/*
  Reopen all tables with closed data files
  One should have lock on LOCK_open when calling this
*/

bool reopen_tables(THD *thd,bool get_locks,bool in_refresh)
{
  DBUG_ENTER("reopen_tables");
  if (!thd->open_tables)
    DBUG_RETURN(0);

  TABLE *table,*next,**prev;
  TABLE **tables,**tables_ptr;			// For locks
  bool error=0;
  if (get_locks)
  {
    /* The ptr is checked later */
    tables= (TABLE**)
      my_malloc(sizeof(TABLE*)*thd->current_tablenr,MYF(0));
  }
  else
    tables= &thd->open_tables;
  tables_ptr =tables;

  prev= &thd->open_tables;
  for (table=thd->open_tables; table ; table=next)
  {
    uint db_stat=table->db_stat;
    next=table->next;
    if (!tables || (!db_stat && reopen_table(thd,table,1)))
    {
      VOID(hash_delete(&open_cache,(byte*) table));
      my_error(ER_CANT_REOPEN_TABLE,MYF(0),table->table_name);
      error=1;
    }
    else
    {
      *prev= table;
      prev= &table->next;
      if (get_locks && !db_stat)
	*tables_ptr++= table;			// need new lock on this
      if (in_refresh)
	table->version=0;
    }
  }
  if (tables != tables_ptr)
  {
    MYSQL_LOCK *lock;
    /* We should always get these locks */
    thd->some_tables_deleted=0;
    if ((lock=mysql_lock_tables(thd,tables,(uint) (tables_ptr-tables))))
    {
      thd->locked_tables=mysql_lock_merge(thd->locked_tables,lock);
    }
    else
      error=1;
  }
  if (get_locks)
    my_free((gptr) tables,MYF(MY_ALLOW_ZERO_PTR));
  VOID(pthread_cond_broadcast(&COND_refresh)); // Signal to refresh
  *prev=0;
  DBUG_RETURN(error);
}


void close_old_data_files(THD *thd, TABLE *table, bool abort_locks)
{
  bool found=0;
  for (; table ; table=table->next)
  {
    if (table->version != refresh_version)
    {
      if (!abort_locks)
	table->version = refresh_version;
      if (table->db_stat)
      {
	found=1;
	if (abort_locks)
	{
	  mysql_lock_abort(table);		// Close waiting threads
	  mysql_lock_remove(thd->locked_tables,table);
	}
	ha_close(table);
	table->db_stat=0;
      }
    }
  }
  if (found)
    VOID(pthread_cond_broadcast(&COND_refresh)); // Signal to refresh
}

/* Wait until all used tables are refreshed */

bool wait_for_tables(THD *thd)
{
  DBUG_ENTER("wait_for_tables");

  pthread_mutex_lock(&LOCK_open);
  thd->some_tables_deleted=0;
  close_old_data_files(thd,thd->open_tables,0);
  bool result=reopen_tables(thd,0,0);
  pthread_mutex_unlock(&LOCK_open);
  DBUG_RETURN(result);
}


/* drop tables from locked list */

bool drop_locked_tables(THD *thd,const char *db, const char *table_name)
{
  TABLE *table,*next,**prev;
  bool found=0;
  prev= &thd->open_tables;
  for (table=thd->open_tables; table ; table=next)
  {
    next=table->next;
    if (!strcmp(table->real_name,table_name) &&
	!strcmp(table->table_cache_key,db))
    {
      mysql_lock_remove(thd->locked_tables,table);
      VOID(hash_delete(&open_cache,(byte*) table));
      found=1;
    }
    else
    {
      *prev=table;
      prev= &table->next;
    }
  }
  *prev=0;
  if (found)
    VOID(pthread_cond_broadcast(&COND_refresh)); // Signal to refresh
  return found;
}


/* lock table to force abort of any threads trying to use table */

void abort_locked_tables(THD *thd,const char *db, const char *table_name)
{
  TABLE *table;
  for (table=thd->open_tables; table ; table=table->next)
  {
    if (!strcmp(table->real_name,table_name) &&
	!strcmp(table->table_cache_key,db))
      mysql_lock_abort(table);
  }
}

/****************************************************************************
	open_unireg_entry
**	Purpose : Load a table definition from file and open unireg table
**	Args	: entry with DB and table given
**	Returns : 0 if ok
*/

static int open_unireg_entry(TABLE *entry,const char *db,const char *name,
			     const char *alias)
{
  char path[FN_REFLEN];
  DBUG_ENTER("open_unireg_entry");

  (void)sprintf(path,"%s/%s/%s",mysql_data_home,db,name);
  if (openfrm(path,alias,
	      (uint) (HA_OPEN_KEYFILE | HA_OPEN_RNDFILE | HA_GET_INDEX |
		      HA_TRY_READ_ONLY),
	      READ_KEYINFO | COMPUTE_TYPES | EXTRA_RECORD,
	      entry))
  {
    DBUG_RETURN(1);
  }
  VOID(ha_extra(entry,HA_EXTRA_NO_READCHECK));		// Not needed in SQL
  DBUG_RETURN(0);
}


/*****************************************************************************
** open all tables in list
*****************************************************************************/

int open_tables(THD *thd,TABLE_LIST *start)
{
  TABLE_LIST *tables;
  bool refresh;
  int result=0;
  DBUG_ENTER("open_tables");

 restart:
  thd->proc_info="Opening tables";
  for (tables=start ; tables ; tables=tables->next)
  {
    if (!tables->table &&
	!(tables->table=open_table(thd,
				   tables->db ? tables->db : thd->db,
				   tables->real_name,
				   tables->name, &refresh)))
    {
      if (refresh)				// Refresh in progress
      {
	/* close all 'old' tables used by this thread */
	pthread_mutex_lock(&LOCK_open);
	thd->version=refresh_version;
	TABLE **prev_table= &thd->open_tables;
	for (TABLE_LIST *tmp=start ; tmp ; tmp=tmp->next)
	{
	  if (tmp->table)
	  {
	    if (tmp->table->version != refresh_version ||
		! tmp->table->db_stat)
	    {
	      VOID(hash_delete(&open_cache,(byte*) tmp->table));
	      tmp->table=0;
	    }
	    else
	    {
	      *prev_table= tmp->table;		// Relink open list
	      prev_table= &tmp->table->next;
	    }
	  }
	}
	*prev_table=0;
	pthread_mutex_unlock(&LOCK_open);
	goto restart;
      }
      result= -1;				// Fatal error
      break;
    }
    if (tables->lock_type != TL_UNLOCK)
      tables->table->reginfo.lock_type=tables->lock_type;
    tables->table->grant= tables->grant;
  }
  thd->proc_info=0;
  DBUG_RETURN(result);
}


TABLE *open_ltable(THD *thd, TABLE_LIST *table_list, thr_lock_type lock_type)
{
  TABLE *table;
  bool refresh;
  DBUG_ENTER("open_ltable");

#ifdef __WIN32__
  /* Win32 can't drop a file that is open */
  if (lock_type == TL_WRITE_ALLOW_READ)
    lock_type= TL_WRITE;
#endif
  thd->proc_info="Opening table";
  while (!(table=open_table(thd,table_list->db ? table_list->db : thd->db,
			    table_list->real_name,table_list->name,
			    &refresh)) && refresh) ;
  if (table)
  {
    table_list->table=table;
    table->grant= table_list->grant;
    if (thd->locked_tables)
    {
      thd->proc_info=0;
      if ((int) lock_type >= (int) TL_WRITE_ALLOW_READ &&
	  (int) table->reginfo.lock_type < (int) TL_WRITE_ALLOW_READ)
      {
	my_printf_error(ER_TABLE_NOT_LOCKED_FOR_WRITE,
			ER(ER_TABLE_NOT_LOCKED_FOR_WRITE),
			MYF(0),table_list->name);
	DBUG_RETURN(0);
      }
      thd->proc_info=0;
      DBUG_RETURN(table);
    }
    if ((table->reginfo.lock_type=lock_type) != TL_UNLOCK)
      if (!(thd->lock=mysql_lock_tables(thd,&table_list->table,1)))
	DBUG_RETURN(0);
  }
  thd->proc_info=0;
  DBUG_RETURN(table);
}

/*
** Open all tables in list and locks them for read.
** The lock will automaticly be freed by the close_thread_tables
*/

int open_and_lock_tables(THD *thd,TABLE_LIST *tables)
{
  if (open_tables(thd,tables) || lock_tables(thd,tables))
    return -1;					/* purecov: inspected */
  return 0;
}

int lock_tables(THD *thd,TABLE_LIST *tables)
{
  if (tables && !thd->locked_tables)
  {
    uint count=0;
    TABLE_LIST *table;
    for (table = tables ; table ; table=table->next)
      count++;
    TABLE **start,**ptr;
    ptr=start=(TABLE**) sql_alloc(sizeof(TABLE*)*count);
    for (table = tables ; table ; table=table->next)
      *(ptr++)= table->table;
    if (!(thd->lock=mysql_lock_tables(thd,start,count)))
      return -1;				/* purecov: inspected */
  }
  return 0;
}

/*
** Open a single table without table caching and don't set it in open_list
** Used by alter_table to open a temporary table
*/

TABLE *open_tmp_table(THD *thd,const char *db,const char *table_name)
{
  TABLE *tmp_table;
  DBUG_ENTER("open_tmp_table");
  if (!(tmp_table=(TABLE*) my_malloc(sizeof(*tmp_table),MYF(MY_WME))))
    DBUG_RETURN(0);				/* purecov: inspected */
  if (open_unireg_entry(tmp_table,db,table_name,table_name))
  {
    my_free((gptr) tmp_table,MYF(0));		/* purecov: inspected */
    DBUG_RETURN(0);				/* purecov: inspected */
  }
  tmp_table->reginfo.lock_type=TL_WRITE;
  tmp_table->tmp_table = 1;			// For debugging
  tmp_table->table_cache_key=0;
  DBUG_RETURN(tmp_table);
}


/*****************************************************************************
** find field in list or tables. if field is unqualifed and unique,
** return unique field
******************************************************************************/

#define WRONG_GRANT (Field*) -1

Field *find_field_in_table(THD *thd,TABLE *table,const char *name,uint length,
			   bool check_grant)
{
  Field *field;
  if (table->name_hash.records)
  {
    if ((field=(Field*) hash_search(&table->name_hash,(byte*) name,
				    length)))
      goto found;
  }
  else
  {
    Field **ptr=table->field;
    while ((field = *ptr++))
    {
      if (!my_strcasecmp(field->field_name, name))
	goto found;
    }
  }
  return (Field*) 0;

 found:
  if (thd->set_query_id)
  {
    if (field->query_id != thd->query_id)
    {
      field->query_id=thd->query_id;
      field->table->used_fields++;
      field->table->used_keys&=field->part_of_key;
    }
    else
      thd->dupp_field=field;
  }
  if (check_grant && check_grant_column(thd,table,name,length))
    return WRONG_GRANT;
  return field;
}


Field *
find_field_in_tables(THD *thd,Item_field *item,TABLE_LIST *tables)
{
  Field *found=0;
  const char *db=item->db_name;
  const char *table_name=item->table_name;
  const char *name=item->field_name;
  uint length=strlen(name);

  if (table_name)
  {						/* Qualified field */
    bool found_table=0;
    for (; tables ; tables=tables->next)
    {
      if (!strcmp(tables->name,table_name) &&
	  (!db ||
	   (tables->db && !strcmp(db,tables->db)) ||
	   (!tables->db && !strcmp(db,thd->db))))
      {
	found_table=1;
	Field *find=find_field_in_table(thd,tables->table,name,length,
					grant_option);
	if (find)
	{
	  if (find == WRONG_GRANT)
	    return (Field*) 0;
	  if (db || !thd->where)
	    return find;
	  if (found)
	  {
	    my_printf_error(ER_NON_UNIQ_ERROR,ER(ER_NON_UNIQ_ERROR),MYF(0),
			    item->full_name(),thd->where);
	    return (Field*) 0;
	  }
	  found=find;
	}
      }
    }
    if (found)
      return found;
    if (!found_table)
    {
      char buff[NAME_LEN*2+1];
      if (db)
      {
	strxmov(buff,db,".",table_name,NullS);
	table_name=buff;
      }
      my_printf_error(ER_UNKNOWN_TABLE,ER(ER_UNKNOWN_TABLE),MYF(0),table_name,
		      thd->where);
    }
    else
      my_printf_error(ER_BAD_FIELD_ERROR,ER(ER_BAD_FIELD_ERROR),MYF(0),
		      item->full_name(),thd->where);
    return (Field*) 0;
  }
  for (; tables ; tables=tables->next)
  {
    Field *field=find_field_in_table(thd,tables->table,name,length,
				     grant_option);
    if (field)
    {
      if (field == WRONG_GRANT)
	return (Field*) 0;
      if (found)
      {
	if (!thd->where)			// Returns first found
	  break;
	my_printf_error(ER_NON_UNIQ_ERROR,ER(ER_NON_UNIQ_ERROR),MYF(0),
			name,thd->where);
	return (Field*) 0;
      }
      found=field;
    }
  }
  if (found)
    return found;
  my_printf_error(ER_BAD_FIELD_ERROR,ER(ER_BAD_FIELD_ERROR),
		  MYF(0),item->full_name(),thd->where);
  return (Field*) 0;
}

Item **
find_item_in_list(Item *find,List<Item> &items)
{
  List_iterator<Item> li(items);
  Item **found=0,*item;
  const char *field_name=0;
  const char *table_name=0;
  if (find->type() == Item::FIELD_ITEM	|| find->type() == Item::REF_ITEM)
  {
    field_name= ((Item_ident*) find)->field_name;
    table_name= ((Item_ident*) find)->table_name;
  }

  while ((item=li++))
  {
    if (field_name && item->type() == Item::FIELD_ITEM)
    {
      if (!my_strcasecmp(((Item_field*) item)->name,field_name))
      {
	if (!table_name)
	{
	  if (found)
	  {
	    if ((*found)->eq(item))
	      continue;				// Same field twice (Access?)
	    if (current_thd->where)
	      my_printf_error(ER_NON_UNIQ_ERROR,ER(ER_NON_UNIQ_ERROR),MYF(0),
			      find->full_name(), current_thd->where);
	    return (Item**) 0;
	  }
	  found=li.ref();
	}
	else if (!strcmp(((Item_field*) item)->table_name,table_name))
	{
	  found=li.ref();
	  break;
	}
      }
    }
    else if (!table_name && !my_strcasecmp(item->name,find->name))
    {
      found=li.ref();
      break;
    }
  }
  if (!found && current_thd->where)
    my_printf_error(ER_BAD_FIELD_ERROR,ER(ER_BAD_FIELD_ERROR),MYF(0),
		    find->full_name(),current_thd->where);
  return found;
}


/****************************************************************************
**	Check that all given fields exists and fill struct with current data
****************************************************************************/

int setup_fields(THD *thd, TABLE_LIST *tables, List<Item> &fields,
		 bool set_query_id, List<Item> *sum_func_list)
{
  reg2 Item *item;
  List_iterator<Item> it(fields);
  DBUG_ENTER("setup_fields");

  thd->set_query_id=set_query_id;
  thd->allow_sum_func= test(sum_func_list);
  thd->where="field list";

  /* Remap table numbers if INSERT ... SELECT */
  uint tablenr=0;
  for (TABLE_LIST *table=tables ; table ; table=table->next,tablenr++)
  {
    table->table->tablenr=tablenr;
    table->table->map= (table_map) 1 << tablenr;
    if (table->natural_join || table->on_expr)
      table->table->maybe_null=1;		// LEFT JOIN ...
  }
  if (tablenr > MAX_TABLES)
  {
    my_error(ER_TOO_MANY_TABLES,MYF(0),MAX_TABLES);
    DBUG_RETURN(-1);
  }
  while ((item=it++))
  {
    if (item->type() == Item::FIELD_ITEM &&
	((Item_field*) item)->field_name[0] == '*')
    {
      if (insert_fields(thd,tables,((Item_field*) item)->table_name,&it))
	DBUG_RETURN(-1); /* purecov: inspected */
    }
    else
    {
      if (item->fix_fields(thd,tables))
	DBUG_RETURN(-1); /* purecov: inspected */
      if (item->with_sum_func && item->type() != Item::SUM_FUNC_ITEM)
	item->split_sum_func(*sum_func_list);
    }
  }
  DBUG_RETURN(test(thd->fatal_error));
}


/****************************************************************************
**	This just drops in all fields instead of current '*' field
**	Returns pointer to last inserted field if ok
****************************************************************************/

static bool
insert_fields(THD *thd,TABLE_LIST *tables, const char *table_name,
	      List_iterator<Item> *it)
{
  TABLE_LIST *table;
  uint found;
  DBUG_ENTER("insert_fields");

  found=0;
  for (table=tables ; table ; table=table->next)
  {
    if (grant_option &&
	check_grant_all_columns(thd,SELECT_ACL,table->table))
      DBUG_RETURN(-1);
    if (!table_name || !strcmp(table_name,table->name))
    {
      Field **ptr=table->table->field,*field;
      while ((field = *ptr++))
      {
	Item_field *item= new Item_field(field);
	if (!found++)
	  (void) it->replace(item);
	else
	  it->after(item);
	if (field->query_id == thd->query_id)
	  thd->dupp_field=field;
	field->query_id=thd->query_id;
	field->table->used_keys&=field->part_of_key;
      }
      /* All fields are used */
      table->table->used_fields=table->table->fields;
    }
  }
  if (!found)
  {
    if (!table_name)
      my_error(ER_NO_TABLES_USED,MYF(0));
    else
      my_error(ER_BAD_TABLE_ERROR,MYF(0),table_name);
  }
  DBUG_RETURN(!found);
}


/*
** Fix all conditions and outer join expressions
*/

int setup_conds(THD *thd,TABLE_LIST *tables,COND *conds)
{
  DBUG_ENTER("setup_conds");
  thd->set_query_id=1;
  thd->cond_count=0;
  thd->allow_sum_func=0;
  if (conds)
  {
    thd->where="where clause";
    if (conds->fix_fields(thd,tables))
      DBUG_RETURN(1);
  }

  /* Check if we are using outer joins */
  for (TABLE_LIST *table=tables ; table ; table=table->next)
  {
    if (table->natural_join)
    {
      /* Make a join of all fields with have the same name */
      TABLE *t1=table->table;
      TABLE *t2=table->natural_join->table;
      Item_cond_and *cond_and=new Item_cond_and();
      if (!cond_and)				// If not out of memory
	DBUG_RETURN(1);

      uint i,j;
      for (i=0 ; i < t1->fields ; i++)
      {
	for (j=0 ; j < t2->fields ; j++)
	{
	  if (!my_strcasecmp(t1->field[i]->field_name,t2->field[j]->field_name))
	  {
	    Item_func_eq *tmp=new Item_func_eq(new Item_field(t1->field[i]),
					       new Item_field(t2->field[j]));
	    if (tmp)				// If not out of memory
	    {
	      tmp->fix_length_and_dec();	// Update cmp_type
	      cond_and->list.push_back(tmp);
	    }
	    break;
	  }
	}
      }
      table->on_expr=cond_and;
      cond_and->used_tables_cache= t1->map | t2->map;
      thd->cond_count+=cond_and->list.elements;
    }
    else if (table->on_expr)
    {
      /* Make a join an a expression */
      thd->where="on clause";
      if (table->on_expr->fix_fields(thd,tables))
	DBUG_RETURN(1);
      thd->cond_count++;
    }
  }
  DBUG_RETURN(test(thd->fatal_error));
}


/******************************************************************************
** Fill a record with data (for INSERT or UPDATE)
** Returns : 1 if some field has wrong type
******************************************************************************/

int
fill_record(List<Item> &fields,List<Item> &values)
{
  List_iterator<Item> f(fields),v(values);
  Item *value;
  Item_field *field;
  DBUG_ENTER("fill_record");

  while ((field=(Item_field*) f++))
  {
    value=v++;
    if (value->save_in_field(field->field))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


int
fill_record(TABLE *table,List<Item> &values)
{
  List_iterator<Item> v(values);
  Item *value;
  DBUG_ENTER("fill_record");

  Field **ptr=table->field,*field;
  while ((field = *ptr++))
  {
    value=v++;
    if (value->save_in_field(field))
      DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
}


static void mysql_rm_tmp_tables(void)
{
  uint index;
  char	filePath[FN_REFLEN];
  MY_DIR *dirp;
  FILEINFO *file;
  DBUG_ENTER("mysql_rm_tmp_tables");

  /* See if the directory exists */
  if (!(dirp = my_dir(mysql_tmpdir,MYF(MY_WME | MY_DONT_SORT))))
    DBUG_VOID_RETURN; /* purecov: inspected */

  /*
  ** Remove all SQLxxx tables from directory
  */

  for (index=2 ; index < (uint) dirp->number_off_files	; index++)
  {
    file=dirp->dir_entry+index;
    if (!bcmp(file->name,"SQL",3))
    {
      sprintf(filePath,"%s/%s",mysql_tmpdir,file->name); /* purecov: inspected */
      VOID(my_delete(filePath,MYF(MY_WME))); /* purecov: inspected */
    }
  }
  my_dirend(dirp);
  DBUG_VOID_RETURN;
}


/*
** CREATE INDEX and DROP INDEX are implemented by calling ALTER TABLE with
** the right arguments.  This isn't very fast but it should work for most cases.
** One should normally create all indexes with CREATE TABLE or ALTER TABLE.
*/

int mysql_create_index(THD *thd, TABLE_LIST *table_list, List<Key> &keys)
{
  List<create_field> fields;
  List<Alter_drop> drop;
  List<Alter_column> alter;
  DBUG_ENTER("mysql_create_index");
  DBUG_RETURN(mysql_alter_table(thd,table_list->db,table_list->real_name,
				table_list,
				fields, keys, drop, alter, FALSE, DUP_ERROR));
}


int mysql_drop_index(THD *thd, TABLE_LIST *table_list, List<Alter_drop> &drop)
{
  List<create_field> fields;
  List<Key> keys;
  List<Alter_column> alter;
  DBUG_ENTER("mysql_drop_index");
  DBUG_RETURN(mysql_alter_table(thd,table_list->db,table_list->real_name,
				table_list,
				fields, keys, drop, alter, FALSE, DUP_ERROR));
}

/*****************************************************************************
	unireg support functions
*****************************************************************************/

	/* make a select from mysql info
	   Error is set as following:
	   0 = ok
	   1 = Got some error (out of memory?)
	   */

SQL_SELECT *make_select(TABLE **table,uint table_count,uint head,
			uint const_tables, COND *conds, int *error)
{
  SQL_SELECT *select;
  DBUG_ENTER("make_select");

  *error=0;
  if (!conds)
    DBUG_RETURN(0);
  if (!(select= new SQL_SELECT))
  {
    *error= 1;
    DBUG_RETURN(0);				/* purecov: inspected */
  }
  if (head == table_count)
    head--;					// All const tables
  select->read_tables=select->const_tables=PREV_BITS(const_tables);
  select->tables=table;
  select->head=table[head];
  select->cond=conds;

  if (table[head]->io_cache)
  {
    memcpy((gptr) &select->file,(gptr) table[head]->io_cache,sizeof(IO_CACHE));
    select->records=(select->file.end_of_file/
		     select->head->keyfile_info.ref_length);
    my_free((gptr) (table[head]->io_cache),MYF(0));
    table[head]->io_cache=0;
  }
  DBUG_RETURN(select);
}


/*
** Invalidate any cache entries that are for some DB
** We can't use hash_delete when looping hash_elements, so mark them and
** delete what's unused.
*/

void remove_db_from_cache(const my_string db)
{
  for (uint index=0 ; index < open_cache.records ; index++)
  {
    TABLE *table=(TABLE*) hash_element(&open_cache,index);
    if (!strcmp(table->table_cache_key,db))
    {
      table->version=0L;			/* Free when thread is ready */
      if (!table->in_use)
	relink_unused(table);
    }
  }
  while (unused_tables && !unused_tables->version)
    VOID(hash_delete(&open_cache,(byte*) unused_tables));
}



/*
** Mark all entries with the table as deleted to force an reopen of the table
** Returns true if the table is in use
*/

bool remove_table_from_cache(const char *db,const char *table_name)
{
  char key[MAX_DBKEY_LENGTH];
  uint key_length;
  TABLE *table;
  bool result=0;
  THD *thd=current_thd;
  DBUG_ENTER("remove_table_from_cache");

  key_length=(uint) (strmov(strmov(key,db)+1,table_name)-key)+1;
  for (table=(TABLE*) hash_search(&open_cache,(byte*) key,key_length) ;
       table;
       table = (TABLE*) hash_next(&open_cache,(byte*) key,key_length))
  {
    table->version=0L;			/* Free when thread is ready */
    if (!table->in_use)
      relink_unused(table);
    else if (table->in_use != thd)
    {
      table->in_use->some_tables_deleted=1;
      if (table->db_stat)
	result=1;
    }
  }
  while (unused_tables && !unused_tables->version)
    VOID(hash_delete(&open_cache,(byte*) unused_tables));
  DBUG_RETURN(result);
}
