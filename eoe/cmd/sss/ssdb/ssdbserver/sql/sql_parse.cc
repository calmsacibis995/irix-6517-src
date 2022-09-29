/* Copyright (C) 1979-1997 TcX AB & Monty Program KB & Detron HB

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

#include "mysql_priv.h"
#include "sql_acl.h"
#include <m_ctype.h>
#include <thr_alarm.h>

extern int yyparse(void);
extern "C" pthread_mutex_t THR_LOCK_keycache;

static bool check_table_access(THD *thd,uint want_access,TABLE_LIST *tables);
static bool check_dup(THD *thd,char *db,char *name,TABLE_LIST *tables);
static void mysql_init_query(THD *thd);
static void remove_escape(char *name);
static void kill_one_thread(THD *thd, ulong thread);
static void refresh_status(void);

static char *any_db="*any*";		// Special symbol for check_access

char *command_name[]={
  "Sleep", "Quit", "Init DB", "Query", "Field List", "Create DB",
  "Drop DB", "Refresh", "Shutdown", "Statistics", "Processes",
  "Connect","Kill","Debug","Ping",
};

#ifdef __WIN32__
static void  test_signal(int sig_ptr)
{
#ifndef DBUG_OFF
  MessageBox(NULL,"Test signal","DBUG",MB_OK);
#endif
}
static void init_signals(void)
{
  int signals[7] = {SIGINT,SIGILL,SIGFPE,SIGSEGV,SIGTERM,SIGBREAK,SIGABRT } ;
  for(int i=0 ; i < 7 ; i++)
    signal( signals[i], test_signal) ;
}
#endif

/*
** check connnetion and get priviliges
** returns 0 on ok.
*/

static int
check_connections(THD *thd)
{
  uint connect_errors=0;
  NET *net= &thd->net;
  /*
  ** store the connection details
  */
#ifdef WIN32
  if ( net->nettype == NET_TYPE_NAMEDPIPE )
  {
    DBUG_PRINT( "general", ("New connection received on named pipe") );
    /* host is unknown */
    thd->host=my_strdup("localhost",MYF(0));
    thd->ip = 0;
    bzero( (char*) &thd->local, sizeof(struct sockaddr) );
    bzero( (char*) &thd->remote, sizeof(struct sockaddr) );
  }
  else
#endif
  {
    DBUG_PRINT("general",("New connection received on %d", net->fd));
    if (!thd->host)				// If TCP/IP connection
    {
      size_socket addrLen= sizeof(struct sockaddr);
      /* check for win32 */
      if (getpeername(net->fd, (struct sockaddr *) &thd->remote, &addrLen))
      {
	return (ER_BAD_HOST_ERROR);
      }
      thd->ip=my_strdup(inet_ntoa(thd->remote.sin_addr),MYF(0));
#if !defined(HAVE_SYS_UN_H) || defined(HAVE_mit_thread)
      /* Fast local hostname resolve for Win32 */
      if (!strcmp(thd->ip,"127.0.0.1"))
	thd->host=my_strdup("localhost",MYF(0));
      else
#endif
      if (specialflag & SPECIAL_NO_RESOLVE)
	thd->host=0;
      else
      {
	thd->host=ip_to_hostname(&thd->remote.sin_addr,&connect_errors);
	if (connect_errors > max_connect_errors)
	  return(ER_HOST_IS_BLOCKED);
      }
      DBUG_PRINT("general",("Host: %s  ip: %s",
			    thd->host ? thd->host : "unknown host",
			    thd->ip ? thd->ip : "unknown ip"));
      if (acl_check_host(thd->host,thd->ip))
	return(ER_HOST_NOT_PRIVILEGED);
    }
    else
    {
      DBUG_PRINT("general",("Host: localhost"));
      thd->ip=0;
      bzero((char*) &thd->local, sizeof(struct sockaddr));
      bzero((char*) &thd->remote,sizeof(struct sockaddr));
    }

    uint opt=1;
    VOID(setsockopt(net->fd,SOL_SOCKET,SO_KEEPALIVE, (char *) &opt,
		    sizeof(opt)));
  }
  {
    char buff[60],*end;
    uint pkt_len;
    LINT_INIT(pkt_len);

    end=strmov(buff,server_version)+1;
    int4store((uchar*) end,thd->thread_id);
    end+=4;
    memcpy(end,thd->scramble,9);
    end+=9;
#ifdef HAVE_COMPRESS
    int2store(end,CLIENT_LONG_FLAG | CLIENT_CONNECT_WITH_DB | CLIENT_COMPRESS);
#else
    int2store(end,CLIENT_LONG_FLAG | CLIENT_CONNECT_WITH_DB);
#endif
    end+=2;
    if (net_write_command(net,protocol_version, buff,
			  (uint) (end-buff)) ||
	(pkt_len=my_net_read(net)) == packet_error || pkt_len < 6)
    {
      inc_host_errors(&thd->remote.sin_addr);
      return(ER_HANDSHAKE_ERROR);
    }
  }
  if (connect_errors)
    reset_host_errors(&thd->remote.sin_addr);
  if (thd->packet.alloc(net_buffer_length))
    return(ER_OUTOFMEMORY);

  thd->client_capabilities=uint2korr(net->read_pos);
  thd->max_packet_length=uint3korr(net->read_pos+2);
  if (!(thd->user = my_strdup((char*) net->read_pos+5, MYF(MY_FAE))))
    return(ER_OUTOFMEMORY);
  char *passwd= strend((char*) net->read_pos+5)+1;
  thd->master_access=acl_getroot(thd->host, thd->ip, thd->user,
				 passwd, thd->scramble, &thd->priv_user,
				 protocol_version == 9 ||
				 !(thd->client_capabilities &
				   CLIENT_LONG_PASSWORD));
  thd->password=test(passwd[0]);
  if (thd->client_capabilities & CLIENT_CONNECT_WITH_DB)
  {
    if (!(thd->db=my_strdup(strend(passwd)+1,MYF(MY_FAE))))
      return(ER_OUTOFMEMORY);
  }

  DBUG_PRINT("general",
	     ("Capabilities: %d  packet_length: %d  Host: %s  User: %s  Using password: %s  Access: %u  db: %s",
	      thd->client_capabilities, thd->max_packet_length,
	      thd->host ? thd->host : thd->ip, thd->priv_user,
	      passwd[0] ? "yes": "no",
	      thd->master_access, thd->db ? thd->db : ""));
  if (thd->master_access & NO_ACCESS)
  {
    net_printf(net, ER_ACCESS_DENIED_ERROR,
	       thd->user,
	       thd->host ? thd->host : thd->ip,
	       passwd[0] ? ER(ER_YES) : ER(ER_NO));
    mysql_log.write(COM_CONNECT,ER(ER_ACCESS_DENIED_ERROR),
		    thd->user,
		    thd->host ? thd->host : thd->ip,
		    passwd[0] ? ER(ER_YES) : ER(ER_NO));
    return(-1);					// Error already given
  }
  if (thread_count >= max_connections && !(thd->master_access & PROCESS_ACL))
    return(ER_CON_COUNT_ERROR);			// Too many connections

  net->timeout=NET_READ_TIMEOUT;
  mysql_log.write(COM_CONNECT,
		  (const my_string) (thd->priv_user == thd->user ?
				     "%s@%s on %s" :
				     "%s@%s as anonymous on %s"),
		  thd->user,
		  thd->host ? thd->host : thd->ip,
		  thd->db ? thd->db : (char*) "");
  return 0;
}


pthread_handler_decl(handle_one_connection,arg)
{
  THD *thd=(THD*) arg;
  NET *net= &thd->net;
#ifndef __WIN32__       /* Win32 calls this in pthread_create */
  if (my_thread_init())
  {
    close_connection(&thd->net,ER_OUT_OF_RESOURCES);
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    aborted_connects++;
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
    end_thread(0);
    return 0;
  }
#endif
  thd->thread_stack= (char*) &thd;

#ifdef __WIN32__
  init_signals();				// IRENA; testing ?
#endif
  pthread_detach_this_thread();
  if (init_thr_lock() ||
      my_pthread_setspecific_ptr(THR_THD,  thd) ||
      my_pthread_setspecific_ptr(THR_MALLOC, &thd->alloc) ||
      my_pthread_setspecific_ptr(THR_NET,  &thd->net))
  {
    close_connection(&thd->net,ER_OUT_OF_RESOURCES);
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    aborted_connects++;
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
    end_thread(0);
    return 0;
  }
  thd->mysys_var=my_thread_var;
  thd->dbug_thread_id=my_thread_id();
#ifndef __WIN32__
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif

  int error;
  if ((error=check_connections(thd)))
  {						// Wrong permissions
    if (error > 0)
      net_printf(net,error,thd->host ? thd->host : thd->ip);
#ifdef __NT__
    if (net->nettype == NET_TYPE_NAMEDPIPE)
      sleep(1);					/* must wait after eof() */
#endif
    close_connection(net,0);
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    aborted_connects++;
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
    end_thread(0);
    return(0);
  }

  thd->alloc.free=thd->alloc.used=0;
  if (max_join_size == (ulong) ~0L)
    thd->options |= OPTION_BIG_SELECTS;

  if (thd->db)
  {						// If login with db
    char *db_name=thd->db;
    thd->db=0;
    if (mysql_change_db(thd,db_name))
      thd->killed=1;				// Could not change db
    my_free(db_name,MYF(0));
  }
  else
    send_ok(net);				// Ready to handle questions

  if (thd->client_capabilities & CLIENT_COMPRESS)
    net->compress=1;				// Use compression

  thd->proc_info=0;
  thd->version=refresh_version;
  while (!net->error && net->fd != INVALID_SOCKET && !thd->killed)
  {
    if (do_command(thd))
      break;
  }
  if (net->error && net->fd != INVALID_SOCKET)
  {
    sql_print_error("Aborted connection %ld to db: '%s' user: '%s'",
		    thd->thread_id,(thd->db ? thd->db : "unconnected"),
		    thd->user);
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    aborted_threads++;
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
  }
  close_connection(net);
  end_thread(0);
  return(0);					/* purecov: deadcode */
}


int handle_bootstrap(THD *thd)
{
  thd->thread_stack= (char*) &thd;

  if (init_thr_lock() ||
      my_pthread_setspecific_ptr(THR_THD,  thd) ||
      my_pthread_setspecific_ptr(THR_MALLOC, &thd->alloc) ||
      my_pthread_setspecific_ptr(THR_NET,  &thd->net))
  {
    close_connection(&thd->net,ER_OUT_OF_RESOURCES);
    end_thread(0);
    return 0;
  }
  thd->mysys_var=my_thread_var;
  thd->dbug_thread_id=my_thread_id();
#ifndef __WIN32__
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif

  thd->alloc.free=thd->alloc.used=0;
  if (max_join_size == (ulong) ~0L)
    thd->options |= OPTION_BIG_SELECTS;

  thd->proc_info=0;
  thd->version=refresh_version;

  while (fgets((char*) thd->net.buff, thd->net.max_packet, stdin))
  {
    uint length=strlen((char*) thd->net.buff);
    while (length && isspace(thd->net.buff[length-1]))
      length--;
    thd->net.buff[length]=0;
    init_sql_alloc(&thd->alloc,8192);
    thd->current_tablenr=0;
    thd->tmp_table=0;
    thd->query=sql_strdup((gptr) thd->net.buff);
    thd->query_id=query_id++;
    mysql_parse(thd,thd->query,length);
    close_thread_tables(thd);			// Free tables
    if (thd->fatal_error)
    {
      return(-1);
    }
    free_root(&thd->alloc);
  }
  return 0;
}


static inline void free_items(THD *thd)
{
    /* This works because items are allocated with sql_alloc() */
  for (Item *item=thd->free_list ; item ; item=item->next)
    delete item;
}

	/* Execute one command from socket (query or simple command) */

bool do_command(THD *thd)
{
  char *packet;
  uint old_timeout,packet_length;
  bool	error=0,tables_used=0;
  NET *net;
  enum enum_server_command command;
  DBUG_ENTER("do_command");

  init_sql_alloc(&thd->alloc,8192);
  net= &thd->net;
  thd->current_tablenr=0;
  thd->tmp_table=0;

  packet=0;
  old_timeout=net->timeout;
  net->timeout=net_wait_timeout;		/* Wait max for 8 hours */
  net->last_error[0]=0;				// Clear error message

  net_new_transaction(net);
  if ((packet_length=my_net_read(net)) == packet_error)
  {
    command = COM_QUIT;
    DBUG_PRINT("general",("Got error reading command from socket %d",net->fd));
  }
  else
  {
    packet=(char*) net->read_pos;
    command = (enum enum_server_command) (uchar) packet[0];
    DBUG_PRINT("general",("Command on socket %d = %d (%s)",
			  net->fd, command, command_name[command]));
  }
  net->timeout=old_timeout;			/* Timeout */
  thd->command=command;
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  thd->query_id=query_id;
  if (command != COM_STATISTICS)
    query_id++;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  thd->set_time();
  switch(command) {
  case COM_INIT_DB:
    if (!mysql_change_db(thd,packet+1))
      mysql_log.write(command,"%s",thd->db);
    break;
  case COM_QUERY:
    thd->query=sql_memdup((gptr) (packet+1),packet_length);
    tables_used=1;
    if (!(specialflag & SPECIAL_NO_PRIOR))
      my_pthread_setprio(pthread_self(),QUERY_PRIOR);
#ifdef FIX_LOG					// Done in client
    if (opt_log)
    {						// Remove linefeed for log
      for (char *pos=packet+1; pos=strchr(pos,'\n'); pos++)
	*pos=' ';
    }
#endif
    mysql_log.write(command,"%s",packet+1);
    DBUG_PRINT("query",("%s",packet+1));
    mysql_parse(thd,thd->query,packet_length-1);
    if (!(specialflag & SPECIAL_NO_PRIOR))
      my_pthread_setprio(pthread_self(),WAIT_PRIOR);
    DBUG_PRINT("info",("query ready"));
    break;
  case COM_FIELD_LIST:				// This isn't actually neaded
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(&thd->net,ER_NOT_ALLOWED_COMMAND);	/* purecov: inspected */
    break;
#else
  {
    tables_used=1;
    char *fields;
    TABLE_LIST table_list;
    bzero((char*) &table_list,sizeof(table_list));
    if (!(table_list.db=thd->db))
    {
      send_error(net,ER_NO_DB_ERROR);
      break;
    }
    thd->free_list=0;
    table_list.name=table_list.real_name=sql_strdup(packet+1);
    thd->query=fields=sql_strdup(strend(packet+1)+1);
    mysql_log.write(command,"%s %s",table_list.real_name,fields);
    remove_escape(table_list.real_name);	// This can't have wildcards

    if (check_access(thd,SELECT_ACL,table_list.db,&thd->col_access))
      break;
    table_list.grant.privilege=thd->col_access;
    if (grant_option && check_grant(thd,SELECT_ACL,&table_list,2))
      break;
    mysqld_list_fields(thd,&table_list,fields);
    free_items(thd);
    break;
  }
#endif
  case COM_QUIT:
    mysql_log.write(command,NullS);
    net->error=0;				// Don't give 'abort' message
    error=TRUE;					// End server
    break;

  case COM_CREATE_DB:
    {
      char *db=sql_strdup(packet+1);
      if (check_access(thd,CREATE_ACL,db,0,1))
	break;
      mysql_log.write(command,packet+1);
      mysql_create_db(thd,db);
      break;
    }
  case COM_DROP_DB:
    {
      char *db=sql_strdup(packet+1);
      if (check_access(thd,DROP_ACL,db,0,1))
	break;
      mysql_log.write(command,db);
      mysql_rm_db(thd,db,0);
      break;
    }
  case COM_REFRESH:
    {
      uint options=(uchar) packet[1];
      if (check_access(thd,RELOAD_ACL,any_db))
	break;
      mysql_log.write(command,NullS);
      if (reload_acl_and_cache(options))
	send_error(net,0);
      else
	send_eof(net);
      break;
    }
  case COM_SHUTDOWN:
    if (check_access(thd,SHUTDOWN_ACL,any_db))
      break; /* purecov: inspected */
    DBUG_PRINT("quit",("Got shutdown command"));
    mysql_log.write(command,NullS);
    send_eof(net);
#ifdef __WIN32__
    sleep(1);					/* must wait after eof() */
#endif
    send_eof(net);
    close_connection(net);
    close_thread_tables(thd);			/* Free before kill */
    free_root(&thd->alloc);
#ifdef __WIN32__
    {
      extern HANDLE hEventShutdown;
      if (!SetEvent(hEventShutdown))
	{
	  DBUG_PRINT("error",("Got error: %ld from SetEvent",GetLastError()));
	}
      // or:
      // HANDLE hEvent=OpenEvent(0, FALSE, "MySqlShutdown");
      // SetEvent(hEventShutdown);
      // CloseHandle(hEvent);
    }
#else
    if (pthread_kill(signal_thread,SIGTERM))	/* End everything nicely */
    {
      DBUG_PRINT("error",("Got error %d from pthread_kill",errno)); /* purecov: inspected */
    }
#endif
    DBUG_PRINT("quit",("After pthread_kill"));
    error=TRUE;
    break;

  case COM_STATISTICS:
  {
    mysql_log.write(command,NullS);
    char buff[200];
    sprintf((char*) buff,
	    "Uptime: %ld  Threads: %d  Questions: %ld  Slow queries: %d  Opens: %ld  Flush tables: %ld  Open tables: %d",
	    (ulong) (time((time_t*) 0) - start_time),
	    (int) thread_count,thd->query_id,long_query_count,
	    opened_tables,refresh_version, cached_tables());
#ifdef SAFEMALLOC
    if (lCurMemory)				// Using SAFEMALLOC
      sprintf(strend(buff), "  Memory in use: %ldK  Max memory used: %ldK",
	      (lCurMemory+1023L)/1024L,(lMaxMemory+1023L)/1024L);
 #endif
    VOID(my_net_write(net, buff,strlen(buff)));
    VOID(net_flush(net));
    break;
  }
  case COM_PING:
    send_ok(net);				// Tell client we are alive
    break;
  case COM_PROCESS_INFO:
    if (!thd->priv_user[0] && check_access(thd,PROCESS_ACL,any_db))
      break;
    mysql_log.write(command,NullS);
    mysqld_list_processes(thd,thd->master_access & PROCESS_ACL ? NullS :
			  thd->priv_user);
    break;
  case COM_PROCESS_KILL:
  {
    ulong thread_id=(ulong) uint4korr(packet+1);
    kill_one_thread(thd,thread_id);
    break;
  }
  case COM_DEBUG:
    if (check_access(thd,PROCESS_ACL,any_db))
      break;					/* purecov: inspected */
    mysql_print_status();
    mysql_log.write(command,NullS);
    send_eof(net);
    break;
  case COM_SLEEP:
  case COM_CONNECT:				// Impossible here
  case COM_TIME:				// Impossible from client
  default:
    send_error(net, ER_UNKNOWN_COM_ERROR);
    break;
  }
  thd->proc_info="cleaning up";
  if (tables_used)
    close_thread_tables(thd);			/* Free tables */

  if (thd->fatal_error)
    send_error(net,0);				// End of memory ?
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For process list
  thd->proc_info=0;
  thd->command=COM_SLEEP;
  thd->query=0;
  time_t start_of_query=thd->start_time;
  thd->set_time();
  if ((ulong) (thd->start_time - start_of_query) > long_query_time)
    long_query_count++;
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  thd->packet.shrink(net_buffer_length);	// Reclaim some memory
  free_root(&thd->alloc);
  DBUG_RETURN(error);
}

/****************************************************************************
** mysql_execute_command
** Execute command saved in thd and current_lex->sql_command
****************************************************************************/

void
mysql_execute_command(void)
{
  int	res=0;
  THD	*thd=current_thd;
  LEX	*lex=current_lex;
  TABLE_LIST *tables=(TABLE_LIST*) thd->table_list.first;
  DBUG_ENTER("mysql_execute_command");

  switch(lex->sql_command) {
  case SQLCOM_SELECT:
  {
    select_result *result;
    if (lex->options & SELECT_DESCRIBE)
      lex->exchange=0;
    if (tables)
    {
      res=check_table_access(thd,
			     lex->exchange ? SELECT_ACL | FILE_ACL :
			     SELECT_ACL,
			     tables);
    }
    else
      res=check_access(thd, lex->exchange ? SELECT_ACL | FILE_ACL : SELECT_ACL,
		       any_db);
    if (res)
    {
      res=0;
      break;					// Error message is given
    }

    thd->offset_limit=lex->offset_limit;
    thd->select_limit=lex->select_limit+lex->offset_limit;
    if (thd->select_limit < lex->select_limit)
      thd->select_limit= HA_POS_ERROR;		// no limit

    if (lex->exchange)
    {
      if (!(result=new select_export(lex->exchange)))
      {
	res= -1;
#ifdef DELETE_ITEMS
	delete lex->having;
	delete lex->where;
#endif
	break;
      }
    }
    else if (!(result=new select_send()))
    {
      res= -1;
#ifdef DELETE_ITEMS
      delete lex->having;
      delete lex->where;
#endif
      break;
    }

    if (lex->options & SELECT_HIGH_PRIORITY)
    {
      TABLE_LIST *table;
      for (table = tables ; table ; table=table->next)
	table->lock_type=TL_READ_HIGH_PRIORITY;
    }

    if (!(res=open_and_lock_tables(thd,tables)))
    {
      res=mysql_select(thd,tables,thd->item_list,
		       lex->where,
		       (ORDER*) thd->order_list.first,
		       (ORDER*) thd->group_list.first,
		       lex->having,
		       (ORDER*) thd->proc_list.first,
		       lex->options | thd->options,
		       result);
    }
    delete result;
#ifdef DELETE_ITEMS
    delete lex->having;
    delete lex->where;
#endif
    break;
  }
  case SQLCOM_CREATE_TABLE:
    if (check_access(thd,CREATE_ACL,tables->db,&tables->grant.privilege))
      goto error;				/* purecov: inspected */
    if (grant_option && check_grant(thd,CREATE_ACL,tables))
      goto error;

    if (strlen(tables->name) > NAME_LEN)
    {
      net_printf(&thd->net,ER_WRONG_TABLE_NAME,tables->name);
      res=0;
      break;
    }
    res = mysql_create_table(thd,tables->db ? tables->db : thd->db,
			     tables->name, lex->create_list,
			     lex->key_list,0);
    if (!res)
      send_ok(&thd->net);
    break;
  case SQLCOM_CREATE_INDEX:
    if (!tables->db)
      tables->db=thd->db;
    if (check_access(thd,INDEX_ACL,tables->db,&tables->grant.privilege))
      goto error; /* purecov: inspected */
    if (grant_option && check_grant(thd,INDEX_ACL,tables))
      goto error;
    res = mysql_create_index(thd, tables, lex->key_list);
    break;
  case SQLCOM_ALTER_TABLE:
    {
      uint priv=0;
      if (lex->name && strlen(lex->name) > NAME_LEN)
      {
	net_printf(&thd->net,ER_WRONG_TABLE_NAME,lex->name);
	res=0;
	break;
      }
      if (!lex->db)
	lex->db=tables->db;
      if (check_access(thd,ALTER_ACL,tables->db,&tables->grant.privilege) ||
	  check_access(thd,INSERT_ACL | CREATE_ACL,lex->db,&priv))
	goto error; /* purecov: inspected */
      if (!tables->db)
	tables->db=thd->db;
      if (grant_option)
      {
	if (check_grant(thd,ALTER_ACL,tables))
	  goto error;
	if (lex->name && !test_all_bits(priv,INSERT_ACL | CREATE_ACL))
	{					// Rename of table
	  TABLE_LIST tmp_table;
	  bzero((char*) &tmp_table,sizeof(tmp_table));
	  tmp_table.real_name=lex->name;
	  tmp_table.db=lex->db;
	  tmp_table.grant.privilege=priv;
	  if (check_grant(thd,INSERT_ACL | CREATE_ACL,tables))
	    goto error;
	}
      }
      res= mysql_alter_table(thd, lex->db, lex->name, tables, lex->create_list,
			     lex->key_list, lex->drop_list, lex->alter_list,
			     lex->drop_primary, lex->duplicates);
      break;
    }
  case SQLCOM_OPTIMIZE:
    /* This is now done with ALTER TABLE, but should be done with isamchk */
    if (!tables->db)
      tables->db=thd->db;
    if (check_access(thd,SELECT_ACL | INSERT_ACL,tables->db,
		     &tables->grant.privilege))
      goto error; /* purecov: inspected */
    if (grant_option && check_grant(thd,SELECT_ACL | INSERT_ACL,tables))
      goto error;

    lex->create_list.empty();
    lex->key_list.empty();
    lex->col_list.empty();
    lex->drop_list.empty();
    lex->alter_list.empty();
    res= mysql_alter_table(thd, NullS, NullS, tables, lex->create_list,
			   lex->key_list, lex->drop_list, lex->alter_list,
			   0,DUP_ERROR);
    break;
  case SQLCOM_UPDATE:
    if (check_access(thd,UPDATE_ACL,tables->db,&tables->grant.privilege))
      goto error;
    if (grant_option && check_grant(thd,UPDATE_ACL,tables))
      goto error;
    if (thd->item_list.elements != thd->value_list.elements)
    {
      send_error(&thd->net,ER_WRONG_VALUE_COUNT);
      DBUG_VOID_RETURN;
    }
    res = mysql_update(thd,tables,
		       thd->item_list,
		       thd->value_list,
		       lex->where,
		       ((thd->options & OPTION_LOW_PRIORITY_UPDATES) ||
			lex->low_priority));
#ifdef DELETE_ITEMS
    delete lex->where;
#endif
    break;
  case SQLCOM_INSERT:
    if (check_access(thd,INSERT_ACL,tables->db,&tables->grant.privilege))
      goto error; /* purecov: inspected */
    if (grant_option && check_grant(thd,INSERT_ACL,tables))
      goto error;
    res = mysql_insert(thd,tables,thd->field_list,lex->many_values,
		       lex->duplicates,
		       ((thd->options & OPTION_LOW_PRIORITY_UPDATES) ||
			lex->low_priority));
    break;
  case SQLCOM_REPLACE:
    if (check_access(thd,INSERT_ACL | UPDATE_ACL | DELETE_ACL,
		     tables->db,&tables->grant.privilege))
      goto error; /* purecov: inspected */
    if (grant_option && check_grant(thd,INSERT_ACL | UPDATE_ACL | DELETE_ACL,
				    tables))

      goto error;
    res = mysql_insert(thd,tables,thd->field_list,lex->many_values,
		       DUP_REPLACE,
		       ((thd->options & OPTION_LOW_PRIORITY_UPDATES) ||
			lex->low_priority));
    break;
  case SQLCOM_REPLACE_SELECT:
  case SQLCOM_INSERT_SELECT:
  {
    // Check that we have modify privileges for the first table and
    // select privileges for the rest
    uint privilege= (lex->sql_command == SQLCOM_INSERT_SELECT ?
		     INSERT_ACL : INSERT_ACL | UPDATE_ACL | DELETE_ACL);
    TABLE_LIST *save_next=tables->next;
    tables->next=0;
    if (check_access(thd, privilege,
		     tables->db,&tables->grant.privilege) ||
	(grant_option && check_grant(thd, privilege, tables)))
      goto error;
    tables->next=save_next;
    if ((res=check_table_access(thd, SELECT_ACL, save_next)))
      goto error;

    select_result *result;
    thd->offset_limit=lex->offset_limit;
    thd->select_limit=lex->select_limit+lex->offset_limit;
    if (thd->select_limit < lex->select_limit)
      thd->select_limit= HA_POS_ERROR;		// No limit

    if (check_dup(thd,tables->db,tables->real_name,tables->next))
    {
      net_printf(&thd->net,ER_INSERT_TABLE_USED,tables->real_name);
      DBUG_VOID_RETURN;
    }
    tables->lock_type=TL_WRITE;				// update first table
    if (!(res=open_and_lock_tables(thd,tables)))
    {
      if ((result=new select_insert(tables->table,&thd->field_list,
				    lex->sql_command == SQLCOM_REPLACE_SELECT ?
				    DUP_REPLACE : DUP_IGNORE)))
      {
	res=mysql_select(thd,tables->next,thd->item_list,
			 lex->where,
			 (ORDER*) thd->order_list.first,
			 (ORDER*) thd->group_list.first,
			 lex->having,
			 (ORDER*) thd->proc_list.first,
			 lex->options,
			 result);
	delete result;
      }
      else
	res= -1;
    }
#ifdef DELETE_ITEMS
    delete lex->having;
    delete lex->where;
#endif
    break;
  }
  case SQLCOM_DELETE:
  {
    if (check_access(thd,DELETE_ACL,tables->db,&tables->grant.privilege))
      goto error; /* purecov: inspected */
    if (grant_option && check_grant(thd,DELETE_ACL,tables))
      goto error;
    // Set privilege for the WHERE clause
    tables->grant.want_privilege=(SELECT_ACL & ~tables->grant.privilege);
    res = mysql_delete(thd,tables,lex->where,lex->select_limit,
		       ((thd->options & OPTION_LOW_PRIORITY_UPDATES) ||
			lex->low_priority));
#ifdef DELETE_ITEMS
    delete lex->where;
#endif
    break;
  }
  case SQLCOM_DROP_TABLE:
    {
      if (check_table_access(thd,DROP_ACL,tables))
	goto error;				/* purecov: inspected */
      res = mysql_rm_table(thd,tables,lex->drop_if_exists);
    }
    break;
  case SQLCOM_DROP_INDEX:
    if (!tables->db)
      tables->db=thd->db;
    if (check_access(thd,INDEX_ACL,tables->db,&tables->grant.privilege))
      goto error;				/* purecov: inspected */
    if (grant_option && check_grant(thd,INDEX_ACL,tables))
      goto error;
    res = mysql_drop_index(thd, tables, lex->drop_list);
    break;
  case SQLCOM_SHOW_DATABASES:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(&thd->net,ER_NOT_ALLOWED_COMMAND);	/* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    res= mysqld_show_dbs(thd, (lex->wild ? lex->wild->ptr() : NullS));
    break;
#endif
  case SQLCOM_SHOW_PROCESSLIST:
    if (!thd->priv_user[0] && check_access(thd,PROCESS_ACL,any_db))
      break;
    mysqld_list_processes(thd,thd->master_access & PROCESS_ACL ? NullS :
			  thd->priv_user);
    break;
  case SQLCOM_SHOW_STATUS:
    pthread_mutex_lock(&THR_LOCK_keycache);
    pthread_mutex_lock(&LOCK_status);
    res= mysqld_show(thd,NullS,status_vars);
    pthread_mutex_unlock(&LOCK_status);
    pthread_mutex_unlock(&THR_LOCK_keycache);
    break;
  case SQLCOM_SHOW_VARIABLES:
    pthread_mutex_lock(&THR_LOCK_keycache);
    pthread_mutex_lock(&LOCK_status);
    res= mysqld_show(thd, (lex->wild ? lex->wild->ptr() : NullS),
		     init_vars);
    pthread_mutex_unlock(&LOCK_status);
    pthread_mutex_unlock(&THR_LOCK_keycache);
    break;
  case SQLCOM_SHOW_TABLES:
    {
      char *db=lex->db ? lex->db : thd->db;
      if (!db)
      {
	send_error(&thd->net,ER_NO_DB_ERROR);	/* purecov: inspected */
	goto error;				/* purecov: inspected */
      }
      remove_escape(db);				// Fix escaped '_'
      if (strlen(db) > NAME_LEN)
      {
	net_printf(&thd->net,ER_WRONG_DB_NAME, db);
	goto error;
      }
      if (check_access(thd,SELECT_ACL,db,&thd->col_access))
	goto error;				/* purecov: inspected */
      /* grant is checked in mysqld_show_tables */
      res= mysqld_show_tables(thd,db,
			      (lex->wild ? lex->wild->ptr() : NullS));
      break;
    }
  case SQLCOM_SHOW_FIELDS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(&thd->net,ER_NOT_ALLOWED_COMMAND);	/* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    {
      char *db=tables->db ? tables->db : thd->db;
      if (!db)
      {
	send_error(&thd->net,ER_NO_DB_ERROR);	/* purecov: inspected */
	goto error;				/* purecov: inspected */
      }
      remove_escape(db);			// Fix escaped '_'
      remove_escape(tables->name);
      if (!tables->db)
	tables->db=thd->db;
      if (check_access(thd,SELECT_ACL,db,&thd->col_access))
	goto error;				/* purecov: inspected */
      tables->grant.privilege=thd->col_access;
      if (grant_option && check_grant(thd,SELECT_ACL,tables,2))
	goto error;
      res= mysqld_show_fields(thd,tables,
			      (lex->wild ? lex->wild->ptr() : NullS));
      break;
    }
#endif
  case SQLCOM_SHOW_KEYS:
#ifdef DONT_ALLOW_SHOW_COMMANDS
    send_error(&thd->net,ER_NOT_ALLOWED_COMMAND);/* purecov: inspected */
    DBUG_VOID_RETURN;
#else
    {
      char *db=tables->db ? tables->db : thd->db;
      if (!db)
      {
	send_error(&thd->net,ER_NO_DB_ERROR);	/* purecov: inspected */
	goto error;				/* purecov: inspected */
      }
      remove_escape(db);			// Fix escaped '_'
      remove_escape(tables->name);
      if (!tables->db)
	tables->db=thd->db;
      if (check_access(thd,SELECT_ACL,db,&thd->col_access))
	goto error; /* purecov: inspected */
      tables->grant.privilege=thd->col_access;
      if (grant_option && check_grant(thd,SELECT_ACL,tables,2))
	goto error;
      res= mysqld_show_keys(thd,tables);
      break;
    }
#endif
  case SQLCOM_CHANGE_DB:
    mysql_change_db(thd,lex->db);
    break;
  case SQLCOM_LOAD:
    if (!(lex->local_file && (thd->client_capabilities & CLIENT_LOCAL_FILES)))
    {
      if (check_access(thd,INSERT_ACL | FILE_ACL,tables->db))
	goto error;
    }
    else
    {
      if (check_access(thd,INSERT_ACL,tables->db,&tables->grant.privilege) ||
	 grant_option && check_grant(thd,INSERT_ACL,tables))
	goto error;
    }
    res=mysql_load(thd, lex->exchange, tables, thd->field_list,
		   lex->duplicates, (bool) lex->local_file);
    break;
  case SQLCOM_SET_OPTION:
    thd->options=lex->options;
    thd->default_select_limit=lex->select_limit;
    DBUG_PRINT("info",("options: %ld  limit: %ld",
		       thd->options,(long) thd->default_select_limit));
    send_ok(&thd->net);
    break;
  case SQLCOM_UNLOCK_TABLES:
    if (thd->locked_tables)
    {
      thd->lock=thd->locked_tables;
      thd->locked_tables=0;			// Will be automaticly closed
    }
    send_ok(&thd->net);
    break;
  case SQLCOM_LOCK_TABLES:
    if (thd->locked_tables)
    {
      thd->lock=thd->locked_tables;
      thd->locked_tables=0;			// Will be automaticly closed
      close_thread_tables(thd);
    }
    if (!(res=open_and_lock_tables(thd,tables)))
    {
      thd->locked_tables=thd->lock;
      thd->lock=0;
      send_ok(&thd->net);
    }
    break;
  case SQLCOM_CREATE_DB:
    {
      if (check_access(thd,CREATE_ACL,lex->name,0,1))
	break;
      mysql_create_db(thd,lex->name);
      break;
    }
  case SQLCOM_DROP_DB:
    {
      if (check_access(thd,DROP_ACL,lex->name,0,1))
	break;
      mysql_rm_db(thd,lex->name,lex->drop_if_exists);
      break;
    }
  case SQLCOM_CREATE_FUNCTION:
    if (check_access(thd,INSERT_ACL,"mysql",0,1))
      break;
#ifdef HAVE_DLOPEN
    if (!(res = mysql_create_function(thd,&lex->udf)))
      send_ok(&thd->net);
#else
    res= -1;
#endif
    break;
  case SQLCOM_DROP_FUNCTION:
    if (check_access(thd,DELETE_ACL,"mysql",0,1))
      break;
#ifdef HAVE_DLOPEN
    if (!(res = mysql_drop_function(thd,lex->udf.name)))
      send_ok(&thd->net);
#else
    res= -1;
#endif
    break;
 case SQLCOM_REVOKE:
 case SQLCOM_GRANT:
   {
     if (tables && !tables->db)
       tables->db=thd->db;
     if (check_access(thd, lex->grant | lex->grant_tot_col | GRANT_ACL,
		      tables && tables->db ? tables->db : NullS,
		      tables ? &tables->grant.privilege : 0,
		      tables ? 0 : 1))
       goto error;
     if (tables)
     {
       if (grant_option && check_grant(thd,
				       (lex->grant | lex->grant_tot_col |
					GRANT_ACL),
				       tables))
	 goto error;
       res = mysql_table_grant(thd,tables,lex->users_list, lex->columns,
			       lex->grant, lex->sql_command == SQLCOM_REVOKE);
     }
     else
     {
       if (lex->columns.elements)
       {
	 net_printf(&thd->net,ER_ILLEGAL_GRANT_FOR_TABLE);
	 res=1;
       }
       else
	 res = mysql_grant(thd, lex->db, lex->users_list, lex->grant,
			   lex->sql_command == SQLCOM_REVOKE);
     }
     break;
   }
  case SQLCOM_FLUSH:
    if (check_access(thd,RELOAD_ACL,any_db))
      goto error;
    if (reload_acl_and_cache(lex->type))
      send_error(&thd->net,0);
    else
      send_ok(&thd->net);
    break;
  case SQLCOM_KILL:
    kill_one_thread(thd,lex->thread_id);
    break;
  default:					/* Impossible */
    send_ok(&thd->net);
    break;
  }
  thd->proc_info="query end";			// QQ
  if (res < 0)
    send_error(&thd->net,thd->killed ? ER_SERVER_SHUTDOWN : 0);

error:
  DBUG_VOID_RETURN;
}


/****************************************************************************
** Get the user (global) and database privileges for all used tables
** Return false (error) if we can't get the privileges and we don't use
** table/column grants.
****************************************************************************/

bool
check_access(THD *thd,uint want_access,const char *db, uint *save_priv,
	     bool no_grant)
{
  uint access,dummy;
  if (save_priv)
    *save_priv=0;
  else
    save_priv= &dummy;

  if (!db && !thd->db && !no_grant)
  {
    send_error(&thd->net,ER_NO_DB_ERROR);	/* purecov: tested */
    return TRUE;				/* purecov: tested */
  }

  if ((thd->master_access & want_access) == want_access)
  {
    *save_priv=thd->master_access;
    return FALSE;
  }
  if ((want_access & ~thd->master_access) & ~DB_ACLS ||
      ! db && no_grant)
  {						// We can never grant this
    net_printf(&thd->net,ER_ACCESS_DENIED_ERROR,
	       thd->priv_user,
	       thd->host ? thd->host : (thd->ip ? thd->ip : "unknown"),
	       thd->password ? ER(ER_YES) : ER(ER_NO));/* purecov: tested */
    return TRUE;				/* purecov: tested */
  }

  if (db == any_db)
    return FALSE;				// Allow select on anything
  if (db && (!thd->db || strcmp(db,thd->db)))
    access=acl_get(thd->host, thd->ip, (char*) &thd->remote.sin_addr,
		   thd->priv_user, db); /* purecov: inspected */
  else
    access=thd->db_access;
  access= (*save_priv=(access | thd->master_access)) & want_access;
  if (access == want_access ||
      ((grant_option && !no_grant) && !(want_access & ~TABLE_ACLS)))
    return FALSE;				/* Ok */
  net_printf(&thd->net,ER_DBACCESS_DENIED_ERROR,
	     thd->priv_user,
	     thd->host ? thd->host : (thd->ip ? thd->ip : "unknown"),
	     db ? db : thd->db ? thd->db : "unknown"); /* purecov: tested */
  return TRUE;					/* purecov: tested */
}


/*
** Check the privilege for all used tables.  Table privileges are cached
** in the table list for GRANT checking
*/

static bool
check_table_access(THD *thd,uint want_access,TABLE_LIST *tables)
{
  uint found=0,found_access=0;
  TABLE_LIST *org_tables=tables;
  for (; tables ; tables=tables->next)
  {
    if ((thd->master_access & want_access) == want_access && thd->db)
      tables->grant.privilege= want_access;
    else if (tables->db)
    {
      if (found && !grant_option)		// db already checked
	tables->grant.privilege=found_access;
      else
      {
	if (check_access(thd,want_access,tables->db,&tables->grant.privilege))
	  return TRUE;				// Access denied
	found_access=tables->grant.privilege;
      }
    }
    else if (check_access(thd,want_access,tables->db,&tables->grant.privilege))
      return TRUE;				// Access denied
  }
  if (grant_option)
    return check_grant(thd,want_access,org_tables);
  return FALSE;
}

/****************************************************************************
	Check stack size; Send error if there isn't enough stack to continue
****************************************************************************/

#define STACK_MIN_SIZE 2048	/* We will need this much stack later */
#if STACK_DIRECTION < 0
#define used_stack(A,B) (long) (A - B)
#else
#define used_stack(A,B) (long) (B - A)
#endif

bool check_stack_overrun(THD *thd,char *buf __attribute__((unused)))
{
  long stack_used;
  if ((stack_used=used_stack(thd->thread_stack,(char*) &stack_used)) >=
      (long) (thread_stack-STACK_MIN_SIZE))
  {
    sprintf(errbuff[0],ER(ER_STACK_OVERRUN),stack_used,thread_stack);
    my_message(ER_STACK_OVERRUN,errbuff[0],MYF(0));
    thd->fatal_error=1;
    return 1;
  }
  return 0;
}

#define MY_YACC_INIT 1000			// Start with big alloc
#define MY_YACC_MAX  32000			// Because of 'short'

bool my_yyoverflow(short **yyss, YYSTYPE **yyvs, int *yystacksize)
{
  LEX	*lex=current_lex;
  int  old_info=0;
  if ((uint) *yystacksize >= MY_YACC_MAX)
    return 1;
  if (!lex->yacc_yyvs)
    old_info= *yystacksize;
  *yystacksize= set_zone((*yystacksize)*2,MY_YACC_INIT,MY_YACC_MAX);
  if (!(lex->yacc_yyvs=
	my_realloc((gptr) lex->yacc_yyvs,
		   *yystacksize*sizeof(**yyvs),
		   MYF(MY_ALLOW_ZERO_PTR | MY_FREE_ON_ERROR))) ||
      !(lex->yacc_yyss=
	my_realloc((gptr) lex->yacc_yyss,
		   *yystacksize*sizeof(**yyss),
		   MYF(MY_ALLOW_ZERO_PTR | MY_FREE_ON_ERROR))))
    return 1;
  if (old_info)
  {						// Copy old info from stack
    memcpy(lex->yacc_yyss, (gptr) *yyss, old_info*sizeof(**yyss));
    memcpy(lex->yacc_yyvs, (gptr) *yyvs, old_info*sizeof(**yyvs));
  }
  *yyss=(short*) lex->yacc_yyss;
  *yyvs=(YYSTYPE*) lex->yacc_yyvs;
  return 0;
}


/****************************************************************************
	Initialize global thd variables neaded for query
****************************************************************************/

static void
mysql_init_query(THD *thd)
{
  DBUG_ENTER("mysql_init_query");
  thd->net.last_error[0]=0;
  thd->item_list.empty();
  thd->value_list.empty();
  thd->order_list.elements=thd->table_list.elements= thd->group_list.elements=0;
  thd->free_list=0;

  thd->order_list.first=0;
  thd->order_list.next= (byte**) &thd->order_list.first;
  thd->table_list.first=0;
  thd->table_list.next= (byte**) &thd->table_list.first;
  thd->group_list.first=0;
  thd->group_list.next= (byte**) &thd->group_list.first;
  thd->proc_list.first=0;			// Needed by sql_select
  thd->fatal_error=0;				// Safety
  thd->last_insert_id_used=thd->query_start_used=thd->insert_id_used=0;
  DBUG_VOID_RETURN;
}


void
mysql_parse(THD *thd,char *inBuf,uint length)
{
  DBUG_ENTER("mysql_parse");

  mysql_init_query(thd);
  LEX *lex=lex_start(thd, (uchar*) inBuf, length);
  if (!yyparse() && ! thd->fatal_error)
    mysql_execute_command();
  thd->proc_info="freeing items";
  free_items(thd);  /* Free strings used by items */
  lex_end(lex);
  DBUG_VOID_RETURN;
}


inline static void
link_in_list(SQL_LIST *list,byte *element,byte **next)
{
  list->elements++;
  (*list->next)=element;
  list->next=next;
  *next=0;
}


/*****************************************************************************
** Store field definition for create
** Return 0 if ok
******************************************************************************/

bool add_field_to_list(char *field_name, enum_field_types type,
		       char *length, char *decimals,
		       uint type_modifier, Item *default_value,char *change,
		       TYPELIB *interval)
{
  register create_field *new_field;
  THD	*thd=current_thd;
  LEX  *lex= &thd->lex;
  DBUG_ENTER("add_field_to_list");

  if (strlen(field_name) > NAME_LEN)
  {
    net_printf(&thd->net, ER_TOO_LONG_IDENT, field_name); /* purecov: inspected */
    DBUG_RETURN(1);				/* purecov: inspected */
  }
  if (type_modifier & PRI_KEY_FLAG)
  {
    lex->col_list.push_back(new key_part_spec(field_name,0));
    lex->key_list.push_back(new Key(Key::PRIMARY,NullS,
				    lex->col_list));
    lex->col_list.empty();
  }

  if (default_value && default_value->type() == Item::NULL_ITEM)
  {
    if (type_modifier & NOT_NULL_FLAG)
    {
      net_printf(&thd->net,ER_INVALID_DEFAULT,field_name);
      DBUG_RETURN(1);
    }
    default_value=0;
  }
  /* change FLOAT(precision) to FLOAT or DOUBLE */
  if (type == FIELD_TYPE_FLOAT && length && !decimals)
  {
    uint tmp_length=atoi(length);
    if (tmp_length > sizeof(double))
    {
      net_printf(&thd->net,ER_WRONG_FIELD_SPEC,field_name); /* purecov: inspected */
      DBUG_RETURN(1); /* purecov: inspected */
    }
    else if (tmp_length > sizeof(float))
      type=FIELD_TYPE_DOUBLE;
    length=0;				// Use default format
  }
  if (!(new_field=new create_field()))
    DBUG_RETURN(1);
  new_field->field=0;
  new_field->field_name=field_name;
  new_field->def= (type_modifier & AUTO_INCREMENT_FLAG ? 0 : default_value);
  new_field->flags= type_modifier;
  new_field->sql_type=type;
  new_field->unireg_check= (type_modifier & AUTO_INCREMENT_FLAG ?
			    Field::NEXT_NUMBER : Field::NONE);
  new_field->decimals= decimals ? (uint) set_zone(atoi(decimals),0,30) : 0;
  new_field->length=0;
  new_field->change=change;
  new_field->interval=0;
  new_field->pack_length=0;
  if (length)
    if (!(new_field->length= (uint) atoi(length)))
      length=0; /* purecov: inspected */
  uint sign_len=type_modifier & UNSIGNED_FLAG ? 0 : 1;
  if (new_field->length && new_field->decimals &&
      new_field->length < new_field->decimals+2)
    new_field->length=new_field->decimals+2; /* purecov: inspected */

  switch (type) {
  case FIELD_TYPE_TINY:
    if (!length) new_field->length=3+sign_len;
    break;
  case FIELD_TYPE_SHORT:
    if (!length) new_field->length=5+sign_len;
    break;
  case FIELD_TYPE_INT24:
    if (!length) new_field->length=8+sign_len;
    break;
  case FIELD_TYPE_LONG:
    if (!length) new_field->length=10+sign_len;
    break;
  case FIELD_TYPE_LONGLONG:
    if (!length) new_field->length=20+sign_len;
    break;
  case FIELD_TYPE_STRING:
  case FIELD_TYPE_VAR_STRING:
  case FIELD_TYPE_DECIMAL:
  case FIELD_TYPE_NULL:
    break;
  case FIELD_TYPE_BLOB:
  case FIELD_TYPE_TINY_BLOB:
  case FIELD_TYPE_LONG_BLOB:
  case FIELD_TYPE_MEDIUM_BLOB:
    if (default_value)				// Allow empty as default value
    {
      String str,*res;
      res=default_value->str(&str);
      if (res->length())
      {
	net_printf(&thd->net,ER_BLOB_CANT_HAVE_DEFAULT,field_name); /* purecov: inspected */
	DBUG_RETURN(1); /* purecov: inspected */
      }
      new_field->def=0;
    }
    new_field->flags|=BLOB_FLAG;
    break;
  case FIELD_TYPE_YEAR:
    if (!length || new_field->length != 2)
      new_field->length=4;			// Default length
    new_field->flags|= ZEROFILL_FLAG | UNSIGNED_FLAG;
    break;
  case FIELD_TYPE_FLOAT:
    if (!length)
    {
      new_field->length = 8+2;			// Default 2 decimals
      new_field->decimals=2;
    }
    break;
  case FIELD_TYPE_DOUBLE:
    if (!length)
    {
      new_field->length = 12+4;
      new_field->decimals=4;
    }
    break;
  case FIELD_TYPE_TIMESTAMP:
    if (!length)
      new_field->length= 14;			// Full date YYYYMMDDHHMMSS
    else
    {
      new_field->length=((new_field->length+1)/2)*2; /* purecov: inspected */
      new_field->length= min(new_field->length,14); /* purecov: inspected */
    }
    new_field->flags|= ZEROFILL_FLAG | UNSIGNED_FLAG | NOT_NULL_FLAG;
    new_field->unireg_check=Field::TIMESTAMP_FIELD;
    break;
  case FIELD_TYPE_DATE:				// Old date type
    if (protocol_version != PROTOCOL_VERSION-1)
      new_field->sql_type=FIELD_TYPE_NEWDATE;
    /* fall trough */
  case FIELD_TYPE_NEWDATE:
    new_field->length=10;
    break;
  case FIELD_TYPE_TIME:
    new_field->length=10;
    break;
  case FIELD_TYPE_DATETIME:
    new_field->length=19;
    break;
  case FIELD_TYPE_SET:
    {
      if (interval->count > sizeof(longlong)*8)
      {
	net_printf(&thd->net,ER_TOO_BIG_SET,field_name); /* purecov: inspected */
	DBUG_RETURN(1); /* purecov: inspected */
      }
      new_field->pack_length=(interval->count+7)/8;
      if (new_field->pack_length > 4)
	new_field->pack_length=8;
      new_field->interval=interval;
      new_field->length=strlen(interval->type_names[0]);
      for (char **pos=interval->type_names+1; *pos ; pos++)
      {
	uint length=strlen(*pos);
	set_if_bigger(new_field->length,length);
      }
      set_if_smaller(new_field->length,MAX_FIELD_WIDTH-1);
      if (default_value)
      {
	thd->cuted_fields=0;
	String str,*res;
	res=default_value->str(&str);
	(void) find_set(interval,res->ptr(),res->length());
	if (thd->cuted_fields)
	{
	  net_printf(&thd->net,ER_INVALID_DEFAULT,field_name);
	  DBUG_RETURN(1);
	}
      }
    }
    break;
  case FIELD_TYPE_ENUM:
    {
      new_field->interval=interval;
      new_field->pack_length=interval->count < 256 ? 1 : 2; // Should be safe
      new_field->length=0;
      for (char **pos=interval->type_names; *pos ; pos++)
	new_field->length+=strlen(*pos)+1;
      new_field->length--;
      set_if_smaller(new_field->length,MAX_FIELD_WIDTH-1);
      if (default_value)
      {
	String str,*res;
	res=default_value->str(&str);
	if (!find_enum(interval,res->ptr(),res->length()))
	{
	  net_printf(&thd->net,ER_INVALID_DEFAULT,field_name);
	  DBUG_RETURN(1);
	}
      }
      break;
    }
  }
  if (new_field->length >= MAX_FIELD_WIDTH ||
      (!new_field->length && !(new_field->flags & BLOB_FLAG)))
  {
    net_printf(&thd->net,ER_TOO_BIG_FIELDLENGTH,field_name,
	       MAX_FIELD_WIDTH-1);		/* purecov: inspected */
    DBUG_RETURN(1);				/* purecov: inspected */
  }
  if (!new_field->pack_length)
    new_field->pack_length=calc_pack_length(new_field->sql_type,
					    new_field->length);
  lex->create_list.push_back(new_field);
  thd->last_field=new_field;
  DBUG_RETURN(0);
}

/* Store position for column in ALTER TABLE .. ADD column */

void store_position_for_column(my_string name)
{
  current_thd->last_field->after=name;
}

bool
add_proc_to_list(Item *item)
{
  ORDER *order;
  Item	**item_ptr;

  if (!(order = (ORDER *) sql_alloc(sizeof(ORDER)+sizeof(Item*))))
    return 1;
  item_ptr = (Item**) (order+1);
  *item_ptr= item;
  order->item=item_ptr;
  order->free_me=0;
  link_in_list(&current_thd->proc_list,(byte*) order,(byte**) &order->next);
  return 0;
}


/* Fix escaping of _, % and \ in database and table names (for ODBC) */

static void remove_escape(char *name)
{
  char *to;
  for (to=name; *name ; name++)
  {
#ifdef USE_BIG5CODE
    if (name[1] && isbig5code(*name,*(name+1)))
    {
      *to++= *name++;
      *to++= *name;
      continue;
    }
#endif
#ifdef USE_MB
    int l;
    if ((l = ismbchar(name, name+MBMAXLEN))) {
	while (l--)
	    *to++ = *name++;
	name--;
	continue;
    }
#endif
    if (*name == '\\' && name[1])
      name++;					// Skipp '\\'
    *to++= *name;
  }
  *to=0;
}

/****************************************************************************
** save order by and tables in own lists
****************************************************************************/


bool add_to_list(SQL_LIST &list,Item *item,bool asc)
{
  ORDER *order;
  Item	**item_ptr;
  DBUG_ENTER("add_to_list");
  if (!(order = (ORDER *) sql_alloc(sizeof(ORDER)+sizeof(Item*))))
    DBUG_RETURN(1);
  item_ptr = (Item**) (order+1);
  *item_ptr=item;
  order->item= item_ptr;
  order->asc = asc;
  order->free_me=0;
  link_in_list(&list,(byte*) order,(byte**) &order->next);
  DBUG_RETURN(0);
}


TABLE_LIST *add_table_to_list(Table_ident *table, LEX_STRING *alias,
			      thr_lock_type flags)
{
  register TABLE_LIST *ptr;
  THD	*thd=current_thd;
  char *alias_str;
  const char *current_db;
  DBUG_ENTER("add_table_to_list");

  if (!table)
    DBUG_RETURN(0);				// End of memory
  alias_str= alias ? alias->str : table->table.str;
  if (table->table.length > NAME_LEN ||
      table->db.str && table->db.length > NAME_LEN)
  {
    net_printf(&thd->net,ER_WRONG_TABLE_NAME,table->table.str);
    DBUG_RETURN(0);
  }
#ifdef FN_LOWER_CASE
  if (!alias)					/* Alias is case sensitive */
    if (!(alias_str=sql_strmake(alias_str,table->table.length)))
      DBUG_RETURN(0);
  casedn_str(table->table.str);
#endif
  if (!(ptr = (TABLE_LIST *) sql_calloc(sizeof(TABLE_LIST))))
    DBUG_RETURN(0); /* purecov: inspected */
  ptr->db= table->db.str;
  ptr->real_name=table->table.str;
  ptr->name=alias_str;
  ptr->lock_type=flags;

  /* check that used name is unique */
  current_db=thd->db ? thd->db : "";
  for (TABLE_LIST *tables=(TABLE_LIST*) thd->table_list.first ; tables ;
       tables=tables->next)
  {
    if (!strcmp(alias_str,tables->name) &&
	!strcmp(ptr->db ? ptr->db : current_db,
		tables->db ? tables->db : current_db))
    {
      net_printf(&thd->net,ER_NONUNIQ_TABLE,alias_str); /* purecov: tested */
      DBUG_RETURN(0);				/* purecov: tested */
    }
  }
  link_in_list(&thd->table_list,(byte*) ptr,(byte**) &ptr->next);
  DBUG_RETURN(ptr);
}

void add_left_join_on(TABLE_LIST *a __attribute__((unused)),
		      TABLE_LIST *b,Item *expr)
{
  b->on_expr=expr;
}


void add_left_join_natural(TABLE_LIST *a,TABLE_LIST *b)
{
  b->natural_join=a;
}

	/* Check if name is used in table list */

static bool check_dup(THD *thd,char *db,char *name,TABLE_LIST *tables)
{
  char *thd_db=thd->db ? thd->db : any_db;
  for (; tables ; tables=tables->next)
    if (!strcmp(name,tables->real_name) &&
	!strcmp(db ? db : thd_db, tables->db ? tables->db : thd_db))
      return 1;
  return 0;
}

bool reload_acl_and_cache(uint options)
{
  bool result=0;

  select_errors=0;				/* Write if more errors */
  mysql_log.flush();				// Flush log
  if (options & REFRESH_GRANT)
  {
    acl_reload();
    if (!(specialflag &  SPECIAL_NO_NEW_FUNC))
      grant_reload();
  }
  if (options & REFRESH_LOG)
  {
    mysql_log.new_file();
    mysql_update_log.new_file();
  }
  if (options & REFRESH_TABLES)
  {
    my_disable_flush_key_blocks=0;
    result=close_cached_tables((options & REFRESH_FAST) ? 0 : 1);
  }
  if (options & REFRESH_HOSTS)
    hostname_cache_refresh();
  if (options & REFRESH_STATUS)
    refresh_status();
  return result;
}


static void kill_one_thread(THD *thd, ulong thread_id)
{
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list
  I_List_iterator<THD> it(threads);
  THD *tmp;
  uint error=ER_NO_SUCH_THREAD;
  while ((tmp=it++))
  {
    if (tmp->thread_id == thread_id)
    {
      if ((thd->master_access & PROCESS_ACL) ||
	  !strcmp(thd->user,tmp->user))
      {
	thr_alarm_kill(tmp->real_id);
	tmp->killed=1;
	error=0;
	if (tmp->mysys_var)
	{
	  pthread_mutex_lock(&tmp->mysys_var->mutex);
	  tmp->mysys_var->abort=1;
	  if (tmp->mysys_var->current_mutex)
	  {
	    pthread_mutex_lock(tmp->mysys_var->current_mutex);
	    pthread_cond_broadcast(tmp->mysys_var->current_cond);
	    pthread_mutex_unlock(tmp->mysys_var->current_mutex);
	  }
	  pthread_mutex_unlock(&tmp->mysys_var->mutex);
	}
      }
      else
	error=ER_KILL_DENIED_ERROR;
      break;					// Found thread
    }
  }
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
  if (!error)
    send_ok(&thd->net);
  else
    net_printf(&thd->net,error,thread_id);
}

/* Clear most status variables */

static void refresh_status(void)
{
  pthread_mutex_lock(&THR_LOCK_keycache);
  pthread_mutex_lock(&LOCK_status);
  for (struct show_var_st *ptr=status_vars; ptr->name; ptr++)
  {
    if (ptr->type == SHOW_LONG)
      *(ulong*) ptr->value=0;
  }
  pthread_mutex_unlock(&LOCK_status);
  pthread_mutex_unlock(&THR_LOCK_keycache);
}

