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

/*****************************************************************************
**
** This file implements classes defined in sql_class.h
** Especially the classes to handle a result from a select
**
*****************************************************************************/

#ifdef __GNUC__
#pragma implementation				// gcc: Class implementation
#endif

#include "mysql_priv.h"
#include "sql_acl.h"
#include <m_ctype.h>
#include <sys/stat.h>
#ifdef	__WIN32__
#include <io.h>
#endif

/*****************************************************************************
** Instansiate templates
*****************************************************************************/

#ifdef __GNUC__
/* Used templates */
template class List<Key>;
template class List_iterator<Key>;
template class List<key_part_spec>;
template class List_iterator<key_part_spec>;
template class List<Alter_drop>;
template class List_iterator<Alter_drop>;
template class List<Alter_column>;
template class List_iterator<Alter_column>;
#endif

/****************************************************************************
*** Thread specific functions
****************************************************************************/

THD::THD()
{
  host=user=db=query=ip=0;
  proc_info="login";
  locked=killed=count_cuted_fields=some_tables_deleted=no_errors=password=
    fatal_error=query_start_used=last_insert_id_used=insert_id_used=
    user_time=bootstrap=0;
  next_insert_id=last_insert_id=0;
  open_tables=0;
  lock=locked_tables=0;
  cuted_fields=0L;
  options=thd_startup_options;
  start_time=(time_t) 0;
  cond_count=0;
  command=COM_SLEEP;
  set_query_id=1;
  default_select_limit= HA_POS_ERROR;
  convert_set=0;
  mysys_var=0;
  db_access=NO_ACCESS;
  ull=0;
  bzero((char*) &alloc,sizeof(alloc));
#ifdef	__WIN32__
  handle_thread = NULL ;
  real_id = 0 ;
#endif
}

THD::~THD()
{
  if (locked_tables)
  {
    lock=locked_tables; locked_tables=0;
    close_thread_tables(this);
  }
  if (ull)
  {
    pthread_mutex_lock(&LOCK_user_locks);
    item_user_lock_release(ull);
    pthread_mutex_unlock(&LOCK_user_locks);
  }
  safeFree(host); safeFree(user); safeFree(db); safeFree(ip);
  mysys_var=0;
#ifdef	__WIN32__
  if (handle_thread)
    CloseHandle(handle_thread);
#endif
}


/*****************************************************************************
** Functions to provide a interface to select results
*****************************************************************************/

select_result::select_result()
{
  thd=current_thd;
}

static String default_line_term("\n"),default_escaped("\\"),
	      default_field_term("\t");

sql_exchange::sql_exchange(char *name) :file_name(name),opt_enclosed(0),
  skip_lines(0)
{
  field_term= &default_field_term;
  enclosed=   line_start= &empty_string;
  line_term=  &default_line_term;
  escaped=    &default_escaped;
}

bool select_send::send_fields(List<Item> &list,uint flag)
{
  return ::send_fields(thd,list,flag);
}


/* Send data to client. Returns 0 if ok */

bool select_send::send_data(List<Item> &items)
{
  List_iterator<Item> li(items);
  String *packet= &thd->packet;
  DBUG_ENTER("send_data");

  if (thd->offset_limit)
  {						// using limit offset,count
    thd->offset_limit--;
    DBUG_RETURN(0);
  }
  packet->length(0);				// Reset packet
  Item *item;
  while ((item=li++))
  {
    if (item->send(packet))
    {
      packet->free();				// Free used
      my_error(ER_OUTOFMEMORY,MYF(0));
      DBUG_RETURN(1);
    }
  }
  bool error=my_net_write(&thd->net,(char*) packet->ptr(),packet->length());
  DBUG_RETURN(error);
}


void select_send::send_error(uint errcode,char *err)
{
  ::send_error(&thd->net,errcode,err);
}

void select_send::send_eof()
{
  ::send_eof(&thd->net);
}


/***************************************************************************
** Export of select to textfile
***************************************************************************/


select_export::~select_export()
{
  if (file >= 0)
  {					// This only happens in case of error
    (void) end_io_cache(&cache);
    (void) my_close(file,MYF(0));
    file= -1;
  }
}

int
select_export::prepare(List<Item> &list)
{
  char path[FN_REFLEN];
  uint option=4;
#ifdef DONT_ALLOW_FULL_LOAD_DATA_PATHS
  option|=1;					// Force use of db directory
#endif
  if (strlen(exchange->file_name) + NAME_LEN >= FN_REFLEN)
    strmake(path,exchange->file_name,FN_REFLEN-1);
  (void) fn_format(path,exchange->file_name, thd->db ? thd->db : "", "",
		   option);
  if (!access(path,F_OK))
  {
    my_error(ER_FILE_EXISTS_ERROR,MYF(0),exchange->file_name);
    return 1;
  }
  /* Create the file world readable */
  if ((file=my_create(path, 0666, O_WRONLY, MYF(MY_WME))) < 0)
    return 1;
#ifdef HAVE_FCHMOD
  (void) fchmod(file,0666);			// Because of umask()
#else
  (void) chmod(path,0666);
#endif
  if (init_io_cache(&cache,file,0L,WRITE_CACHE,0L,1,MYF(MY_WME)))
  {
    my_close(file,MYF(0));
    file= -1;
    return 1;
  }
  field_term_length=exchange->field_term->length();
  if (!exchange->line_term->length())
    exchange->line_term=exchange->field_term;	// Use this if it exists
  field_sep_char= (exchange->enclosed->length() ? (*exchange->enclosed)[0] :
		   field_term_length ? (*exchange->field_term)[0] : INT_MAX);
  escape_char=	(exchange->escaped->length() ? (*exchange->escaped)[0] : -1);
  line_sep_char= (exchange->line_term->length() ?
		  (*exchange->line_term)[0] : INT_MAX);
  if (!field_term_length)
    exchange->opt_enclosed=0;
  if (!exchange->enclosed->length())
    exchange->opt_enclosed=1;			// A little quicker loop
  fixed_row_size= (!field_term_length && !exchange->enclosed->length());
  return 0;
}


bool select_export::send_data(List<Item> &items)
{
  List_iterator<Item> li(items);
  DBUG_ENTER("send_data");
  char buff[MAX_FIELD_WIDTH],null_buff[2],space[MAX_FIELD_WIDTH];
  String tmp(buff,sizeof(buff)),*res;
  bfill(space,sizeof(space),' ');
  tmp.length(0);

  if (thd->offset_limit)
  {						// using limit offset,count
    thd->offset_limit--;
    DBUG_RETURN(0);
  }
  row_count++;
  Item *item;
  char *buff_ptr=buff;
  uint used_length=0,items_left=items.elements;

  if (my_b_write(&cache,(byte*) exchange->line_start->ptr(),
		 exchange->line_start->length()))
    goto err;
  while ((item=li++))
  {
    Item_result result_type=item->result_type();
    res=item->str_result(&tmp);
    if (res && (!exchange->opt_enclosed || result_type == STRING_RESULT))
    {
      if (my_b_write(&cache,(byte*) exchange->enclosed->ptr(),
		     exchange->enclosed->length()))
	goto err;
    }
    if (!res)
    {						// NULL
      if (!fixed_row_size)
      {
	if (escape_char != -1)			// Use \N syntax
	{
	  null_buff[0]=escape_char;
	  null_buff[1]='N';
	  if (my_b_write(&cache,(byte*) null_buff,2))
	    goto err;
	}
	else if (my_b_write(&cache,(byte*) "NULL",4))
	  goto err;
      }
      else
      {
	used_length=0;				// Fill with space
      }
    }
    else
    {
      if (fixed_row_size)
	used_length=min(res->length(),item->max_length);
      else
	used_length=res->length();
      if (result_type == STRING_RESULT && escape_char != -1)
      {
	char *pos,*start,*end;

	for (start=pos=(char*) res->ptr(),end=pos+used_length ;
	     pos != end ;
	     pos++)
	{
#ifdef USE_MB
	  if (ismbchar(pos, end))
	  {
	    pos += ismbchar(pos, end)-1;
	    continue;
	  }
#endif
	  if ((int) *pos == escape_char || (int) *pos == field_sep_char ||
	      (int) *pos == line_sep_char || !*pos)
	  {
	    char tmp_buff[2];
	    tmp_buff[0]= escape_char;
	    tmp_buff[1]= *pos ? *pos : '0';
	    if (my_b_write(&cache,(byte*) start,(uint) (pos-start)) ||
		my_b_write(&cache,(byte*) tmp_buff,2))
	      goto err;
	    start=pos+1;
	  }
	}
	if (my_b_write(&cache,(byte*) start,(uint) (pos-start)))
	  goto err;
      }
      else if (my_b_write(&cache,(byte*) res->ptr(),used_length))
	goto err;
    }
    if (fixed_row_size)
    {						// Fill with space
      if (item->max_length > used_length)
      {
	if (my_b_write(&cache,(byte*) space,item->max_length-used_length))
	  goto err;
      }
    }
    buff_ptr=buff;				// Place separators here
    if (res && (!exchange->opt_enclosed || result_type == STRING_RESULT))
    {
      memcpy(buff_ptr,exchange->enclosed->ptr(),exchange->enclosed->length());
      buff_ptr+=exchange->enclosed->length();
    }
    if (--items_left)
    {
      memcpy(buff_ptr,exchange->field_term->ptr(),field_term_length);
      buff_ptr+=field_term_length;
    }
    if (my_b_write(&cache,(byte*) buff,(uint) (buff_ptr-buff)))
      goto err;
  }
  if (my_b_write(&cache,(byte*) exchange->line_term->ptr(),
		 exchange->line_term->length()))
    goto err;
  DBUG_RETURN(0);
err:
  DBUG_RETURN(1);
}


void select_export::send_error(uint errcode,char *err)
{
    ::send_error(&thd->net,errcode,err);
    (void) end_io_cache(&cache);
    (void) my_close(file,MYF(0));
    file= -1;
}


void select_export::send_eof()
{
  if (end_io_cache(&cache) | my_close(file,MYF(MY_WME)))
    ::send_error(&thd->net);
  else
    ::send_ok(&thd->net,row_count);
  file= -1;
}
