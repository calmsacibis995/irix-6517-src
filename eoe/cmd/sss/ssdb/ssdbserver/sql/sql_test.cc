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

/* Write some debug info */


#include "mysql_priv.h"
#include "sql_select.h"
#include <hash.h>

/* Intern key cache variables */
extern "C" pthread_mutex_t THR_LOCK_keycache;
extern "C" ulong _my_cache_w_requests,_my_cache_write,_my_cache_r_requests,
		_my_cache_read;
extern "C" uint	_my_blocks_used,_my_blocks_changed;
extern "C" int	my_file_opened,my_stream_opened;

#ifndef DBUG_OFF
extern "C" {
extern pthread_mutex_t THR_LOCK_dbug;
}

void
print_where(COND *cond,char *info)
{
  if (cond)
  {
    char buff[256];
    String str(buff,(uint32) sizeof(buff));
    str.length(0);
    cond->print(&str);
    str.append('\0');
    pthread_mutex_lock(&THR_LOCK_dbug);
    (void) fprintf(DBUG_FILE,"\nWHERE:(%s) ",info);
    (void) fputs(str.ptr(),DBUG_FILE);
    (void) fputc('\n',DBUG_FILE);
    pthread_mutex_unlock(&THR_LOCK_dbug);
  }
}

	/* This is for debugging purposes */

extern HASH open_cache;
extern TABLE *unused_tables;

void print_cached_tables(void)
{
  uint index,count,unused;
  TABLE *start_link,*link;

  VOID(pthread_mutex_lock(&LOCK_open));
  puts("DB         Table                             Version  Thread  L.thread  Open");

  for (index=unused=0 ; index < open_cache.records ; index++)
  {
    TABLE *entry=(TABLE*) hash_element(&open_cache,index);
    printf("%-10s %-34s%6ld%8ld%10ld%6d\n",
	   entry->table_cache_key,entry->real_name,entry->version,
	   entry->in_use ? entry->in_use->thread_id : 0L,
	   entry->in_use ? entry->in_use->dbug_thread_id : 0L,
	   entry->db_stat ? 1 : 0);
    if (!entry->in_use)
      unused++;
  }
  count=0;
  if ((start_link=link=unused_tables))
  {
    do
    {
      if (link != link->next->prev || link != link->prev->next)
      {
	printf("unused_links isn't linked properly\n");
	return;
      }
    } while (count++ < open_cache.records && (link=link->next) != start_link);
    if (link != start_link)
    {
      printf("Unused_links aren't connected\n");
    }
  }
  if (count != unused)
    printf("Unused_links (%d) dosen't match open_cache: %d\n", count,unused);
  printf("\nCurrent refresh version: %ld\n",refresh_version);
  if (hash_check(&open_cache))
    printf("Error: File hash table is corrupted\n");
  fflush(stdout);
  VOID(pthread_mutex_unlock(&LOCK_open));
  return;
}


void TEST_filesort(TABLE **table,SORT_FIELD *sortorder,uint s_length,
		   ulong special)
{
  char buff[256],buff2[256];
  String str(buff,sizeof(buff)),out(buff2,sizeof(buff2));
  my_string sep;
  DBUG_ENTER("TEST_filesort");

  out.length(0);
  for (sep=""; s_length-- ; sortorder++, sep=" ")
  {
    out.append(sep);
    if (sortorder->reverse)
      out.append('-');
    if (sortorder->field)
    {
      if (sortorder->field->table_name)
      {
	out.append(sortorder->field->table_name);
	out.append('.');
      }
      out.append(sortorder->field->field_name);
    }
    else
    {
      str.length(0);
      sortorder->item->print(&str);
      out.append(str);
    }
  }
  out.append('\0');				// Purify doesn't like c_ptr()
  pthread_mutex_lock(&THR_LOCK_dbug);
  VOID(fputs("\nInfo about FILESORT\n",DBUG_FILE));
  if (special)
    fprintf(DBUG_FILE,"Records to sort: %ld\n",special);
  fprintf(DBUG_FILE,"Sortorder: %s\n",out.ptr());
  pthread_mutex_unlock(&THR_LOCK_dbug);
  DBUG_VOID_RETURN;
}


void
TEST_join(JOIN *join)
{
  uint i,ref;
  DBUG_ENTER("TEST_join");

  pthread_mutex_lock(&THR_LOCK_dbug);
  VOID(fputs("\nInfo about JOIN\n",DBUG_FILE));
  for (i=0 ; i < join->tables ; i++)
  {
    TABLE *form=join->join_tab[i].table;

    fprintf(DBUG_FILE,"%-16.16s  type: %-7s  q_keys: %4d  refs: %d  key: %d  len: %d\n",
	    form->table_name,
	    join_type_str[join->join_tab[i].type],
	    join->join_tab[i].keys,
	    form->reginfo.ref_fields,
	    form->reginfo.ref_key,
	    form->reginfo.ref_length);
    if (join->join_tab[i].select)
    {
      if (join->join_tab[i].use_quick == 2)
	fprintf(DBUG_FILE,
		"                  quick select checked for each record (keys: %d)\n",
		(int) join->join_tab[i].select->quick_keys);
      else if (join->join_tab[i].select->quick)
	fprintf(DBUG_FILE,"                  quick select used on key %s\n",
		form->key_info[join->join_tab[i].select->quick->index].name);
      else
	VOID(fputs("                  select used\n",DBUG_FILE));
    }
    if (form->reginfo.ref_fields)
    {
      VOID(fputs("                  refs: ",DBUG_FILE));
      for (ref=0 ; ref < form->reginfo.ref_fields ; ref++)
      {
	Field *field=form->reginfo.ref_field[ref].field;
	if (field)
	{
	  if (field->table_name)
	  {
	    fputs(field->table_name,DBUG_FILE);
	    VOID(fputc('.',DBUG_FILE));
	  }
	  fprintf(DBUG_FILE,"%s  ", field->field_name);
	}
	else
	{
	  char buff[MAX_FIELD_WIDTH];
	  String tmp(buff,sizeof(buff));
	  field->val_str(&tmp,&tmp);
	  fprintf(DBUG_FILE,"'%s' ",tmp.c_ptr());
	}
      }
      VOID(fputc('\n',DBUG_FILE));
    }
  }
  pthread_mutex_unlock(&THR_LOCK_dbug);
  DBUG_VOID_RETURN;
}

#endif

void mysql_print_status(void)
{
  printf("\nStatus information:\n\n");
  thr_print_locks();				// Write some debug info
#ifndef DBUG_OFF
  print_cached_tables();
#endif
  /* Print key cache status */
  pthread_mutex_lock(&THR_LOCK_keycache);
  printf("key_cache status:\n\
blocks used:%10u\n\
not flushed:%10u\n\
w_requests: %10lu\n\
writes:     %10lu\n\
r_requests: %10lu\n\
reads:      %10lu\n",
	 _my_blocks_used,_my_blocks_changed,_my_cache_w_requests,
	 _my_cache_write,_my_cache_r_requests,_my_cache_read);
  pthread_mutex_unlock(&THR_LOCK_keycache);

  pthread_mutex_lock(&LOCK_status);
  printf("\nhandler status:\n\
read_key:   %10lu\n\
read_next:  %10lu\n\
read_rnd    %10lu\n\
read_first: %10lu\n\
write:      %10lu\n\
delete      %10lu\n\
update:     %10lu\n",
	 ha_read_key_count, ha_read_next_count,
	 ha_read_rnd_count, ha_read_first_count,
	 ha_write_count, ha_delete_count, ha_update_count);
  pthread_mutex_unlock(&LOCK_status);
  printf("\nTable status:\n\
Opened tables: %10lu\n\
Open tables:   %10lu\n\
Open files:    %10lu\n\
Open streams:  %10lu\n",
	 opened_tables,
	 cached_tables(),
	 my_file_opened,
	 my_stream_opened);
  fflush(stdout);
  TERMINATE(stdout);				// Write malloc information
}
