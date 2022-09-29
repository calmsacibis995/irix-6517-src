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

/* logging of commands */

#include "mysql_priv.h"
#include "sql_acl.h"
#include <my_dir.h>
#include <stdarg.h>
#include <m_ctype.h>				// For test_if_number

MYSQL_LOG mysql_log,mysql_update_log;

static bool test_if_number(register my_string str,
			   long *res, bool allow_wildcards);

/****************************************************************************
** Find a uniq filename for 'filename.#'.
** Set # to a number as low as possible
** returns != 0 if not possible to get uniq filename
****************************************************************************/

static int find_uniq_filename(char *name)
{
  long		number;
  uint		i,length;
  char		buff[FN_REFLEN];
  struct my_dir *dir_info;
  reg1 struct fileinfo *file_info;
  ulong		max_found=0;
  DBUG_ENTER("find_uniq_filename");

  length=dirname_part(buff,name);
  char *start=name+length,*end=strend(start);
  *end='.';
  length=end-start+1;

  if (!(dir_info = my_dir(buff,MYF(MY_DONT_SORT))))
  {						// This shouldn't happen
    strmov(end,".1");				// use name+1
    DBUG_RETURN(0);
  }
  file_info= dir_info->dir_entry;
  for (i=dir_info->number_off_files ; i-- ; file_info++)
  {
    if (bcmp(file_info->name,start,length) == 0 &&
	test_if_number(file_info->name+length, &number,0))
    {
      set_if_bigger(max_found,(ulong) number);
    }
  }
  my_dirend(dir_info);

  *end++='.';
  sprintf(end,"%03d",max_found+1);
  DBUG_RETURN(0);
}


void MYSQL_LOG::open(const my_string log_name,type log_type_arg)
{
  char log_file_name[FN_REFLEN];
  log_type=log_type_arg;
  name=my_strdup(log_name,MYF(0));
  if (log_type == NORMAL)
    fn_format(log_file_name,log_name,mysql_data_home,"",4);
  else
  {
    fn_format(log_file_name,log_name,mysql_data_home,"",4);
    if (!fn_ext(log_name)[0])
    {
      if (find_uniq_filename(log_file_name))
      {
	sql_print_error(ER(ER_NO_UNIQUE_LOGFILE),log_file_name);
	return;
      }
    }
  }
  db[0]=0;
  file=my_fopen(log_file_name,O_APPEND | O_WRONLY,MYF(MY_WME | ME_WAITTANG));
  if (file && log_type == NORMAL)
  {
#ifdef __NT__
    fprintf( file, "%s, Version: %s, started with:\nTCP Port: %d, Named Pipe: %s\n", my_progname, server_version, mysql_port, mysql_unix_port);
#else
    fprintf(file,"%s, Version: %s, started with:\nTcp port: %d  Unix socket: %s\n", my_progname,server_version,mysql_port,mysql_unix_port);
#endif
    fprintf(file,"Time                Id Command    Argument\n");
    (void) fflush(file);
  }
  else if (file && log_type == NEW)
  {
    time_t skr=time(NULL);
    struct tm tm_tmp;
    localtime_r(&skr,&tm_tmp);

    fprintf(file,"# %s, Version: %s at %02d%02d%02d %2d:%02d:%02d\n",
	    my_progname,server_version,
	    tm_tmp.tm_year,
	    tm_tmp.tm_mon+1,
	    tm_tmp.tm_mday,
	    tm_tmp.tm_hour,
	    tm_tmp.tm_min,
	    tm_tmp.tm_sec);
    (void) fflush(file);
  }

  return;
}


void MYSQL_LOG::new_file()
{
  if (file)
  {
    VOID(pthread_mutex_lock(&LOCK_log));
    char *old_name=name;
    name=0;
    close();
    open(old_name,log_type);
    my_free(old_name,MYF(0));
    if (!file)
      log_type=CLOSED;
    last_time=query_start=0;
    last_insert_id=0;
    VOID(pthread_mutex_unlock(&LOCK_log));
  }
}


void MYSQL_LOG::write(enum enum_server_command command,
		      const my_string format,...)
{
  if (name && (what_to_log & (1L << (uint) command)))
  {
    va_list args;
    va_start(args,format);
    VOID(pthread_mutex_lock(&LOCK_log));
    if (log_type != CLOSED)
    {
      time_t skr;
      ulong thread_id;
      THD *thd=current_thd;
      if (thd)
      {						// Normal thread
	if (thd->options & OPTION_LOG_OFF && thd->master_access & PROCESS_ACL)
	{
	  VOID(pthread_mutex_unlock(&LOCK_log));
	  return;				// No logging
	}
	thread_id=thd->thread_id;
	if (thd->user_time || !(skr=thd->query_start()))
	  skr=time(NULL);			// Connected
      }
      else
      {						// Log from connect handler
	skr=time(NULL);
	thread_id=0;
      }
      if (skr != last_time)
      {
	last_time=skr;
	struct tm tm_tmp;
	struct tm *start_time;
	localtime_r(&skr,&tm_tmp);
	start_time=&tm_tmp;
	fprintf(file,"%02d%02d%02d %2d:%02d:%02d\t",
		start_time->tm_year % 100,
		start_time->tm_mon+1,
		start_time->tm_mday,
		start_time->tm_hour,
		start_time->tm_min,
		start_time->tm_sec);
      }
      else
	fputs("\t\t",file);
      fprintf(file,"%7ld %-10.10s",
	      thread_id,command_name[(uint) command]);
      if (format)
      {
	(void) fputc(' ',file);
	(void) vfprintf(file,format,args);
      }
      (void) fputc('\n',file);
      fflush(file);
    }
    va_end(args);
    VOID(pthread_mutex_unlock(&LOCK_log));
  }
}


/* Write update log in a format suitable for incremental backup */

void MYSQL_LOG::write(const char *query)
{
  if (name)
  {
    VOID(pthread_mutex_lock(&LOCK_log));
    if (file)
    {						// Safety agains reopen
      THD *thd=current_thd;
      char buff[80],*end;
      end=buff;
      if (!(thd->options & OPTION_UPDATE_LOG) &&
	  thd->master_access & PROCESS_ACL)
      {
	VOID(pthread_mutex_unlock(&LOCK_log));
	return;
      }
      if (specialflag & SPECIAL_LONG_LOG_FORMAT)
      {
	time_t skr=time(NULL);
	if (skr != last_time)
	{
	  last_time=skr;
	  struct tm tm_tmp;
	  struct tm *start_time;
	  localtime_r(&skr,&tm_tmp);
	  start_time=&tm_tmp;
	  fprintf(file,"# Time: %02d%02d%02d %2d:%02d:%02d\n",
		  start_time->tm_year,
		  start_time->tm_mon+1,
		  start_time->tm_mday,
		  start_time->tm_hour,
		  start_time->tm_min,
		  start_time->tm_sec);
	}
      }
      if (strcmp(thd->db,db))
      {						// Database changed
	fprintf(file,"use %s;\n",thd->db);
	strmov(db,thd->db);
      }
      if (thd->last_insert_id_used)
      {
	if (last_insert_id != thd->insert_id())
	{
	  last_insert_id=thd->insert_id();
	  end=strmov(end,",last_insert_id=");
	  end=longlong2str((longlong) last_insert_id,end,-10);
	}
      }
      // Save value if we do a insert.
      if (thd->insert_id_used)
      {
	last_insert_id=thd->insert_id();
	if (specialflag & SPECIAL_LONG_LOG_FORMAT)
	{
	  end=strmov(end,",insert_id=");
	  end=longlong2str((longlong) last_insert_id,end,-10);
	}
      }
      if (thd->query_start_used)
      {
	if (query_start != thd->query_start())
	{
	  query_start=thd->query_start();
	  end=strmov(end,",timestamp=");
	  end=int2str((long) query_start,end,10);
	}
      }
      if (end != buff)
      {
	*end++=';';
	*end++='\n';
	*end=0;
	(void) fputs("SET ",file);
	(void) fputs(buff+1,file);
      }
      (void) fputs(query,file);
      (void) fputc(';',file);
      (void) fputc('\n',file);
      fflush(file);
    }
    VOID(pthread_mutex_unlock(&LOCK_log));
  }
}


void MYSQL_LOG::flush()
{
  if (file)
    VOID(fflush(file));
}


void MYSQL_LOG::close()
{					// One can't set log_type here!
  if (file)
  {
    VOID(my_fclose(file,MYF(0)));
    file=0;
  }
  if (name)
  {
    my_free(name,MYF(0));
    name=0;
  }
}


	/* Check if a string is a valid number */
	/* Output: TRUE -> number */

static bool test_if_number(register my_string str,
			   long *res, bool allow_wildcards)
{
  reg2 int flag;
  my_string start;
  DBUG_ENTER("test_if_number");

  flag=0; start=str;
  while (*str++ == ' ') ;
  if (*--str == '-' || *str == '+')
    str++;
  while (isdigit(*str) || (allow_wildcards &&
			   (*str == wild_many || *str == wild_one)))
  {
    flag=1;
    str++;
  }
  if (*str == '.')
    for (str++ ;
	 isdigit(*str) ||
	   (allow_wildcards && (*str == wild_many || *str == wild_one)) ;
	 str++, flag=1) ;
  if (*str != 0 || flag == 0)
    DBUG_RETURN(0);
  if (res)
    *res=atol(start);
  DBUG_RETURN(1);			/* Number ok */
} /* test_if_number */

extern "C" {

void sql_print_error(const my_string format,...)
{
  va_list args;
  time_t skr;
  struct tm tm_tmp;
  struct tm *start_time;
  va_start(args,format);
  THD *thd=current_thd;
  DBUG_ENTER("sql_print_error");

  VOID(pthread_mutex_lock(&LOCK_log));
#ifndef DBUG_OFF
  {
    char buff[1024];
    vsprintf(buff,format,args);
    DBUG_PRINT("error",("%s",buff));
  }
#endif
  skr=time(NULL);
  localtime_r(&skr,&tm_tmp);
  start_time=&tm_tmp;
  fprintf(stderr,"%02d%02d%02d %2d:%02d:%02d  ",
	  start_time->tm_year % 100,
	  start_time->tm_mon+1,
	  start_time->tm_mday,
	  start_time->tm_hour,
	  start_time->tm_min,
	  start_time->tm_sec);
  (void) vfprintf(stderr,format,args);
  (void) fputc('\n',stderr);
  fflush(stderr);
  va_end(args);

  VOID(pthread_mutex_unlock(&LOCK_log));
  DBUG_VOID_RETURN;
}

}


void sql_perror(const my_string message)
{
#ifdef HAVE_STRERROR
  sql_print_error("%s: %s",message, strerror(errno));
#else
  perror(message);
#endif
}
