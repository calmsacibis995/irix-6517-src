/* Copyright (C) 1979-1998 TcX AB & Monty Program KB & Detron HB

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
#include <nisam.h>
#include <thr_alarm.h>
#ifdef	__cplusplus
extern "C" {					// Because of SCO 3.2V4.2
#endif
#include <errno.h>
#include <sys/stat.h>
#ifndef __GNU_LIBRARY__
#define __GNU_LIBRARY__				// Skipp warnings in getopt.h
#endif
#include <getopt.h>
#ifdef HAVE_SYSENT
#include <sysent.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>				// For getpwent
#endif

#ifndef __WIN32__
#include <sys/resource.h>
#ifdef HAVE_SYS_UN_H
#  include <sys/un.h>
#endif
#include <netdb.h>
#ifdef HAVE_SELECT_H
#  include <select.h>
#endif
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <sys/utsname.h>
#else
#include <windows.h>
#endif // __WIN32__
#ifdef	__cplusplus
}
#endif

#if defined(HAVE_LINUXTHREADS) && !defined(GLIBC2_STYLE_GETHOSTBYNAME_R)
#include <gnu/types.h>
#define THR_KILL_SIGNAL SIGINT
#else
#include <my_pthread.h>			// For thr_setconcurency()
#define THR_KILL_SIGNAL SIGUSR2		// Can't use this with LinuxThreads
#endif
#if defined(HAVE_GETRLIMIT) && defined(RLIMIT_NOFILE) && !defined(__linux__) && !defined(HAVE_mit_thread)
#define SET_RLIMIT_NOFILE
#endif

#define MYSQL_KILL_SIGNAL SIGTERM

extern "C" ulong _my_cache_w_requests,_my_cache_write,_my_cache_r_requests,
                 _my_cache_read;
extern "C" uint	 _my_blocks_used,_my_blocks_changed;
extern "C" int	 my_file_opened,my_stream_opened;

#ifndef DBUG_OFF
#ifdef __WIN32__
static const char* default_dbug_option="d:t:i:O,\\mysqld.trace";
#else
static const char *default_dbug_option="d:t:i:o,/tmp/mysqld.trace";
#endif
#endif

#ifdef __NT__
static char szPipeName [ 257 ];
static SECURITY_ATTRIBUTES saPipeSecurity;
static SECURITY_DESCRIPTOR sdPipeDescriptor;
static HANDLE hPipe = INVALID_HANDLE_VALUE;
static pthread_cond_t COND_handler_count;
static uint handler_count;
#endif
#ifdef __WIN32__
static bool opt_console=0;
#endif
static Socket unix_sock= INVALID_SOCKET,ip_sock= INVALID_SOCKET;
static ulong back_log,connect_timeout;
static ulong opt_specialflag=SPECIAL_ENGLISH;
static ulong thread_id=1L;
static my_string opt_logname=0,opt_update_logname=0;
static char mysql_home[FN_REFLEN],pidfile_name[FN_REFLEN];
static pthread_cond_t COND_thread_count;
static pthread_attr_t connection_attrib;
static pthread_t select_thread;
static bool opt_log,opt_update_log,opt_noacl,opt_disable_networking=0,
       opt_bootstrap=0;
static bool kill_in_progress=FALSE;
static struct rand_struct sql_rand;
static int cleanup_done;
static char **defaults_argv;

uint mysql_port;
uint test_flags,thread_count=0,select_errors=0;
uint thd_startup_options=OPTION_UPDATE_LOG;
uint current_pid,protocol_version=PROTOCOL_VERSION;
ulong keybuff_size,sortbuff_size,max_item_sort_length,table_cache_size,
      max_join_size,join_buff_size,tmp_table_size,thread_stack,
      net_wait_timeout,what_to_log= ~ (1L << (uint) COM_TIME);
bool opt_endinfo,low_priority_updates;
bool volatile abort_loop,select_thread_in_use,grant_option;
ulong refresh_version=1L;		/* Increments on each reload */
ulong query_id=1L,long_query_count,long_query_time,aborted_threads,
      aborted_connects;
ulong specialflag=0,opened_tables=0,created_tmp_tables=0;
ulong max_connections,max_connect_errors;
char mysql_real_data_home[FN_REFLEN],language[LIBLEN],reg_ext[FN_EXTLEN],
     mysql_data_home[2],
     blob_newline,f_fyllchar,max_sort_char,*mysqld_user,*mysqld_chroot;
char server_version[40]=MYSQL_SERVER_VERSION;
my_string first_keyword="first";
char **errmesg;				/* Error messages */
byte last_ref[MAX_REFLENGTH];		/* Index ref of keys */
my_string mysql_unix_port=NULL,mysql_tmpdir=NULL;
ulong my_bind_addr;			/* the address we bind to */
DATE_FORMAT dayord;
double log_10[32];			/* 10 potences */
I_List<THD> threads;
time_t start_time;
pthread_key(MEM_ROOT*,THR_MALLOC);
pthread_key(THD*, THR_THD);
pthread_key(NET*, THR_NET);
pthread_mutex_t LOCK_mysql_create_db, LOCK_Acl, LOCK_open, LOCK_thread_count,
		LOCK_mapped_file, LOCK_status, LOCK_grant,LOCK_log;

pthread_cond_t COND_refresh;
pthread_t signal_thread;


#ifdef __WIN32__
#undef	 getpid
#include <process.h>
HANDLE hEventShutdown;
#include "nt_servc.h"
static	 NTService  Service;	      // Service object for WinNT
#endif

static void *signal_hand(void *arg);
static void get_options(int argc,char **argv);
static char *get_relative_path(char *path);
static void fix_paths(void);
static pthread_handler_decl(handle_connections_sockets,arg);
static int bootstrap(void);
#ifdef __NT__
static pthread_handler_decl(handle_connections_namedpipes,arg);
static int get_service_parameters();
#endif
#ifdef SET_RLIMIT_NOFILE
static uint set_maximum_open_files(uint max_file_limit);
#endif


/****************************************************************************
** Code to end mysqld
****************************************************************************/

static void close_connections(void)
{
  NET net;
  DBUG_ENTER("close_connections");

  /* kill first connection thread */
#if !defined(__WIN32__) && !defined(__EMX__)
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  DBUG_PRINT("quit",("waiting for select thread: %lx",select_thread));
  while (select_thread_in_use)
  {
    struct timespec abstime;
#ifndef DONT_USE_THR_ALARM
    if (pthread_kill(select_thread,THR_CLIENT_ALARM))
      break;					// allready dead
#endif
#ifdef HAVE_TIMESPEC_TS_SEC
    abstime.ts_sec=time(NULL)+1;		// Bsd 2.1
    abstime.ts_nsec=0;
#else
    abstime.tv_sec=time(NULL)+1;		// Linux or Solairs
    abstime.tv_nsec=0;
#endif
    VOID(pthread_cond_timedwait(&COND_thread_count,&LOCK_thread_count,
				&abstime));
#ifdef AIX_3_2
    if (ip_sock != INVALID_SOCKET)
    {
      VOID(shutdown(ip_sock,2));
      VOID(closesocket(ip_sock));
      VOID(shutdown(unix_sock,2));
      VOID(closesocket(unix_sock));
      VOID(unlink(mysql_unix_port));
      ip_sock=unix_sock= INVALID_SOCKET;
    }
#endif
  }
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
#endif /* __WIN32__ */

  /* Abort listening to new connections */
  DBUG_PRINT("quit",("Closing sockets"));
  if ( !opt_disable_networking )
  {
    if (ip_sock != INVALID_SOCKET)
    {
      VOID(shutdown(ip_sock,2));
      VOID(closesocket(ip_sock));
      ip_sock= INVALID_SOCKET;
    }
  }
#ifdef __NT__
  if ( hPipe != INVALID_HANDLE_VALUE )
  {
    HANDLE hTempPipe = hPipe;
    DBUG_PRINT( "quit", ("Closing named pipes") );
    hPipe = INVALID_HANDLE_VALUE;
    CancelIo( hTempPipe );
    DisconnectNamedPipe( hTempPipe );
    CloseHandle( hTempPipe );
  }
#endif
#ifdef HAVE_SYS_UN_H
  if (unix_sock != INVALID_SOCKET)
  {
    VOID(shutdown(unix_sock,2));
    VOID(closesocket(unix_sock));
    VOID(unlink(mysql_unix_port));
    unix_sock= INVALID_SOCKET;
  }
#endif
  end_thr_alarm();			 // Don't allow alarms

  /* First signal all threads that it's time to die */

  THD *tmp;
  VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list

  I_List_iterator<THD> it(threads);
  while ((tmp=it++))
  {
    DBUG_PRINT("quit",("Informing thread %ld that it's time to die",
		       tmp->thread_id));
    tmp->killed=1;
    if (tmp->mysys_var)
    {
      tmp->mysys_var->abort=1;
      if (tmp->mysys_var->current_mutex)
      {
	pthread_mutex_lock(tmp->mysys_var->current_mutex);
	pthread_cond_broadcast(tmp->mysys_var->current_cond);
	pthread_mutex_unlock(tmp->mysys_var->current_mutex);
      }
    }
  }
  VOID(pthread_mutex_unlock(&LOCK_thread_count)); // For unlink from list

  if (thread_count)
  {
    sleep(1);					// Give threads time to die
  }

  /* Force remaining threads to die by closing the connection to the client */

  VOID(my_net_init(&net, NET_TYPE_TCPIP, 0, (void*) 0));
  for (;;)
  {
    DBUG_PRINT("quit",("Locking LOCK_thread_count"));
    VOID(pthread_mutex_lock(&LOCK_thread_count)); // For unlink from list
    if (!(tmp=threads.get()))
    {
      DBUG_PRINT("quit",("Unlocking LOCK_thread_count"));
      VOID(pthread_mutex_unlock(&LOCK_thread_count));
      break;
    }
#ifndef __bsdi__				// Bug in BSDI kernel
    if ((net.fd=tmp->net.fd) != INVALID_SOCKET)
    {
      net.nettype=tmp->net.nettype;
#ifdef __NT__
      net.hPipe=tmp->net.hPipe;
#endif
      sql_print_error(ER(ER_FORCING_CLOSE),my_progname,
		      tmp->thread_id,tmp->user);
      close_connection(&net,0,0);
    }
#endif
    DBUG_PRINT("quit",("Unlocking LOCK_thread_count"));
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
  }
  net_end(&net);
  /* All threads has now been aborted */
  DBUG_PRINT("quit",("Waiting for threads to die (count=%u)",thread_count));
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  while (thread_count)
  {
    VOID(pthread_cond_wait(&COND_thread_count,&LOCK_thread_count));
    DBUG_PRINT("quit",("One thread died (count=%u)",thread_count));
  }
  VOID(pthread_mutex_unlock(&LOCK_thread_count));

  mysql_log.close();
  mysql_update_log.close();
  DBUG_PRINT("quit",("close_connections thread"));
  DBUG_VOID_RETURN;
}

	/* Force server down. kill all connections and threads and exit */

#ifndef __WIN32__
static void *kill_server(void *sig_ptr)
#define RETURN_FROM_KILL_SERVER return 0
#else
static void __cdecl kill_server(int sig_ptr)
#define RETURN_FROM_KILL_SERVER return
#endif
{
  int sig=(int) (long) sig_ptr;			// This is passed a int
  DBUG_ENTER("kill_server");

  // if there is a signal during the kill in progress, we do not need
  // another one
  if (kill_in_progress)				// Safety
    RETURN_FROM_KILL_SERVER;
  kill_in_progress=TRUE;
  abort_loop=1;
  signal(sig,SIG_IGN);
  if (sig == MYSQL_KILL_SIGNAL || sig == 0)
    sql_print_error(ER(ER_NORMAL_SHUTDOWN),my_progname);
  else
    sql_print_error(ER(ER_GOT_SIGNAL),my_progname,sig); /* purecov: inspected */

#if defined(USE_ONE_SIGNAL_HAND) && !defined(__WIN32__)
  my_thread_init();				// If this is a new thread
#endif
  close_connections();
  sql_print_error(ER(ER_SHUTDOWN_COMPLETE),my_progname);
  if (sig != MYSQL_KILL_SIGNAL && sig != 0)
    unireg_abort(1);				/* purecov: inspected */
  else
    unireg_end(0);
  pthread_exit(0);				/* purecov: deadcode */
  RETURN_FROM_KILL_SERVER;
}


#ifdef USE_ONE_SIGNAL_HAND
pthread_handler_decl(kill_server_thread,arg)
{
  my_thread_init();				// Initialize new thread
  kill_server(0);
  my_thread_end();				// Normally never reached
  return 0;
}
#endif

static sig_handler print_signal_warning(int sig)
{
  sql_print_error("Warning: Got signal %d from thread %d",
		  sig,my_thread_id());
#ifdef DONT_REMEMBER_SIGNAL
  sigset(sig,print_signal_warning);		/* int. thread system calls */
#endif
#ifndef __WIN32__
  if (sig == SIGALRM)
    alarm(2);					/* reschedule alarm */
#endif
}


void unireg_end(int signal_number)
{
  clean_up();
  exit(0);
}


void unireg_abort(int exit_code)
{
  sql_print_error("Aborting\n");
  clean_up(); /* purecov: inspected */
  exit(exit_code); /* purecov: inspected */
}


void clean_up(void)
{
  DBUG_PRINT("exit",("clean_up"));
  if (cleanup_done++)
    return; /* purecov: inspected */
  acl_free(1);
  grant_free();
  table_cache_free();
  hostname_cache_free();
  item_user_lock_free();
  lex_free();				/* Free some memory */
#ifdef HAVE_DLOPEN
  if (!opt_noacl)
    udf_free();
#endif
  end_key_cache();			/* This is usualy freed automaticly*/
  VOID(ha_panic(HA_PANIC_CLOSE));	/* close all hash,misam and p_isam */
  x_free((gptr) errmsg[ERRMAPP]);	/* Free messages */
  free_defaults(defaults_argv);
  my_free(mysql_tmpdir,MYF(0));
  my_end(opt_endinfo ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
} /* clean_up */



/****************************************************************************
** Init IP and UNIX socket
****************************************************************************/

static void set_ports()
{
  char	*env;
  if (!mysql_port)
  {					// Get port if not from commandline
    struct  servent *serv_ptr;
    mysql_port = MYSQL_PORT;
    if ((serv_ptr = getservbyname("mysql", "tcp")))
      mysql_port = ntohs((u_short) serv_ptr->s_port); /* purecov: inspected */
    if ((env = getenv("MYSQL_TCP_PORT")))
      mysql_port = (uint) atoi(env);		/* purecov: inspected */
  }
  if (!mysql_unix_port)
  {
#ifdef __WIN32__
    mysql_unix_port = MYSQL_NAMEDPIPE;
#else
    mysql_unix_port = MYSQL_UNIX_ADDR;
#endif
    if ((env = getenv("MYSQL_UNIX_PORT")))
      mysql_unix_port = env;			/* purecov: inspected */
  }
}

/* Change to run as another user if started with --user */

static void set_user(const char *user)
{
#ifndef __WIN32__
    struct passwd *ent;

  // don't bother if we aren't superuser
  if(geteuid())
    return;

  if(!(ent = getpwnam(user)))
  {
    sql_perror("getpwnam");
    unireg_abort(1);
  }

  if (setuid(ent->pw_uid) == -1)
  {
    sql_perror("setuid");
    unireg_abort(1);
  }
#endif
}

/* Change root user if started with  --chroot */

static void set_root(const char *path)
{
#if !defined(__WIN32__) && !defined(__EMX__)
  if (chroot(path) == -1)
  {
    sql_perror("chroot");
    unireg_abort(1);
  }
#endif
}

static void server_init(void)
{
  struct sockaddr_in	IPaddr;
#ifdef HAVE_SYS_UN_H
  struct sockaddr_un	UNIXaddr;
#endif
  int	arg=1;
  DBUG_ENTER("server_init");

#ifdef	__WIN32__
  if ( !opt_disable_networking )
  {
    WSADATA WsaData;
    if (SOCKET_ERROR == WSAStartup (0x0101, &WsaData))
    {
      my_message(0,"WSAStartup Failed\n",MYF(0));
      unireg_abort(1);
    }
  }
#endif /* __WIN32__ */

  set_ports();

  if (mysql_port != 0 && !opt_disable_networking && !opt_bootstrap)
  {
    DBUG_PRINT("general",("IP Socket is %d",mysql_port));
    ip_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (ip_sock == INVALID_SOCKET)
    {
      DBUG_PRINT("error",("Got error: %d from socket()",socket_errno));
      sql_perror(ER(ER_IPSOCK_ERROR));		/* purecov: tested */
      unireg_abort(1);				/* purecov: tested */
    }
    bzero((char*) &IPaddr, sizeof(IPaddr));
    IPaddr.sin_family = AF_INET;
    IPaddr.sin_addr.s_addr = my_bind_addr;
    IPaddr.sin_port = (unsigned short) htons((unsigned short) mysql_port);
    (void) setsockopt(ip_sock,SOL_SOCKET,SO_REUSEADDR,(char*)&arg,sizeof(arg));
    for(;;)
    {
      if (bind(ip_sock, (struct sockaddr *)&IPaddr, sizeof(IPaddr)) >= 0)
	break;
      DBUG_PRINT("error",("Got error: %d from bind",socket_errno));
      sql_perror("Can't start server: Bind on TCP/IP port");/* Had a loop here */
      sql_print_error("Do you already have another mysqld server running on port: %d ?",mysql_port);
      unireg_abort(1);
    }
    VOID(listen(ip_sock,(int) back_log));
  }

  if (mysqld_chroot)
    set_root(mysqld_chroot);
  if (mysqld_user)
    set_user(mysqld_user);

#ifdef __NT__
  /* create named pipe */
  if (Service.IsNT() && mysql_unix_port[0] && !opt_bootstrap)
  {
    sprintf( szPipeName, "\\\\.\\pipe\\%s", mysql_unix_port );
    ZeroMemory( &saPipeSecurity, sizeof(saPipeSecurity) );
    ZeroMemory( &sdPipeDescriptor, sizeof(sdPipeDescriptor) );
    if ( !InitializeSecurityDescriptor(&sdPipeDescriptor,
				       SECURITY_DESCRIPTOR_REVISION) )
    {
      sql_perror("Can't start server : Initialize security descriptor");
      unireg_abort(1);
    }
    if (!SetSecurityDescriptorDacl(&sdPipeDescriptor, TRUE, NULL, FALSE))
    {
      sql_perror("Can't start server : Set security descriptor");
      unireg_abort(1);
    }
    saPipeSecurity.nLength = sizeof( SECURITY_ATTRIBUTES );
    saPipeSecurity.lpSecurityDescriptor = &sdPipeDescriptor;
    saPipeSecurity.bInheritHandle = FALSE;
    if ((hPipe = CreateNamedPipe(szPipeName,
				 PIPE_ACCESS_DUPLEX,
				 PIPE_TYPE_BYTE |
				 PIPE_READMODE_BYTE |
				 PIPE_WAIT,
				 PIPE_UNLIMITED_INSTANCES,
				 (int) net_buffer_length,
				 (int) net_buffer_length,
				 NMPWAIT_USE_DEFAULT_WAIT,
				 &saPipeSecurity )) == INVALID_HANDLE_VALUE)
      {
	LPVOID lpMsgBuf;
	int error=GetLastError();
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		      FORMAT_MESSAGE_FROM_SYSTEM,
		      NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		      (LPTSTR) &lpMsgBuf, 0, NULL );
	MessageBox( NULL, (LPTSTR) lpMsgBuf, "Error from CreateNamedPipe",
		    MB_OK|MB_ICONINFORMATION );
	LocalFree( lpMsgBuf );
	unireg_abort(1);
      }
  }
#endif

#if defined(HAVE_SYS_UN_H) && !defined(HAVE_mit_thread)
  /*
  ** Create the UNIX socket
  */
  if (mysql_unix_port[0] && !opt_bootstrap)
  {
    DBUG_PRINT("general",("UNIX Socket is %s",mysql_unix_port));

    if ((unix_sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
      sql_perror("Can't start server : UNIX Socket "); /* purecov: inspected */
      unireg_abort(1);				/* purecov: inspected */
    }
    bzero((char*) &UNIXaddr, sizeof(UNIXaddr));
    UNIXaddr.sun_family = AF_UNIX;
    strmov(UNIXaddr.sun_path, mysql_unix_port);
    VOID(unlink(mysql_unix_port));
    (void) setsockopt(unix_sock,SOL_SOCKET,SO_REUSEADDR,(char*)&arg,sizeof(arg));
    umask(0);
    if (bind(unix_sock, (struct sockaddr *)&UNIXaddr, sizeof(UNIXaddr)) < 0)
    {
      sql_perror("Can't start server : Bind on unix socket"); /* purecov: tested */
      sql_print_error("Do you already have another mysqld server running on socket: %s ?",mysql_unix_port);
      unireg_abort(1);					/* purecov: tested */
    }
    umask(((~my_umask) & 0666));
#if defined(S_IFSOCK) && defined(SECURE_SOCKETS)
    VOID(chmod(mysql_unix_port,S_IFSOCK));	/* Fix solaris 2.6 bug */
#endif
    chmod(mysql_unix_port,0700);
    VOID(listen(unix_sock,(int) back_log));
  }
#endif
  DBUG_PRINT("info",("server started"));
  DBUG_VOID_RETURN;
}


void yyerror(char *s)
{
  NET *net=my_pthread_getspecific_ptr(NET*,THR_NET);
  char *yytext=(char*) current_lex->tok_start;
  net_printf(net,ER_PARSE_ERROR, s, yytext ? (char*) yytext : "",
	     current_lex->yylineno);
}


void close_connection(NET *net,uint errcode,bool lock)
{
  Socket fd;
  DBUG_ENTER("close_connection");
  DBUG_PRINT("enter",("fd: %ld  error: '%s'",
		      (ulong) net->fd,
		      errcode ? ER(errcode) : ""));
  if (lock)
    VOID(pthread_mutex_lock(&LOCK_thread_count));
  if ((fd=net->fd) != INVALID_SOCKET)
  {
    if (errcode)
      send_error(net,errcode,ER(errcode));	/* purecov: inspected */
    net->fd= INVALID_SOCKET;			// Marker
#ifdef __NT__
    if (net->nettype == NET_TYPE_NAMEDPIPE)
    {
      HANDLE hPipe = net->hPipe;
      if ( hPipe != INVALID_HANDLE_VALUE )
      {
	net->hPipe = INVALID_HANDLE_VALUE;
	CancelIo( hPipe );
	DisconnectNamedPipe( hPipe );
	CloseHandle( hPipe );
      }
    }
    else
#endif
    {
      VOID(shutdown(fd,2));
      VOID(closesocket(fd));
    }
    net_end(net);
  }
  if (lock)
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
  DBUG_VOID_RETURN;
}

	/* Called when a thread is aborted */
	/* ARGSUSED */

sig_handler end_thread(int sig __attribute__((unused)))
{
  THD *thd=current_thd;
  DBUG_ENTER("end_thread");
  if (thd)
  {
    VOID(pthread_mutex_lock(&LOCK_thread_count));
    free_root(&thd->alloc);
    thread_count--;
    delete thd;
    VOID(pthread_cond_signal(&COND_thread_count)); /* Tell main we are ready */
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
#ifndef DBUG_OFF
    if (!(test_flags & TEST_NO_THREADS))	// For debugging under Linux
#endif
    {
      my_thread_end();
      pthread_exit(0);
    }
  }
  DBUG_VOID_RETURN;				/* purecov: deadcode */
}

	/*
	** Aborts a thread nicely. Commes here on SIGPIPE
	** TODO: One should have to fix that thr_alarm know about this
	** thread too
	*/

#ifdef THREAD_SPECIFIC_SIGPIPE
static sig_handler abort_thread(int sig __attribute__((unused)))
{
  THD *thd=current_thd;
  DBUG_ENTER("abort_thread");
  if (thd)
    thd->killed=1;
  DBUG_VOID_RETURN;
}
#endif

/******************************************************************************
** Setup a signal thread with handles all signals
** Because linux doesn't support scemas use a mutex to check that
** the signal thread is ready before continuing
******************************************************************************/

#ifdef __WIN32__
static void init_signals(void)
{
  int signals[7] = {SIGINT,SIGILL,SIGFPE,SIGSEGV,SIGTERM,SIGABRT } ;
  for (uint i=0 ; i < 7 ; i++)
    signal( signals[i], kill_server) ;
  signal(SIGBREAK,SIG_IGN);	//ignore SIGBREAK for NT
}

#elif defined(__EMX__)
static void sig_reload(int signo)
{
  reload_acl_and_cache(~0);		// Flush everything
  signal(signo, SIG_ACK);
}

static void sig_kill(int signo)
{
  if (!abort_loop)
  {
    abort_loop=1;				// mark abort for threads
    kill_server((void*) signo);
  }
  signal(signo, SIG_ACK);
}

static void init_signals(void)
{
  signal(SIGQUIT, sig_kill);
  signal(SIGKILL, sig_kill);
  signal(SIGTERM, sig_kill);
  signal(SIGINT,  sig_kill);
  signal(SIGHUP,  sig_reload);	// Flush everything
  signal(SIGALRM, SIG_IGN);
  signal(SIGBREAK,SIG_IGN);
  signal_thread = pthread_self();
}
#else

static void init_signals(void)
{
  sigset_t set;
  pthread_attr_t thr_attr;
  int error;
  DBUG_ENTER("init_signals");

  (void) sigset(THR_KILL_SIGNAL,end_thread);
  (void) sigset(THR_SERVER_ALARM,print_signal_warning); // Should never be called!
#ifdef THREAD_SPECIFIC_SIGPIPE
  (void) sigset(SIGPIPE,abort_thread);
  sigaddset(&set,SIGPIPE);
#else
  VOID(signal(SIGPIPE,SIG_IGN));		// Can't know which thread
#endif
  (void) sigemptyset(&set);
  sigaddset(&set,SIGINT);
  sigaddset(&set,SIGQUIT);
  sigaddset(&set,SIGTERM);
  sigaddset(&set,SIGHUP);
  signal(SIGTERM,SIG_DFL);			// If it's blocked by parent
#ifdef SIGTSTP
  sigaddset(&set,SIGTSTP);
#endif
  sigaddset(&set,THR_SERVER_ALARM);
  sigdelset(&set,THR_KILL_SIGNAL);		// May be SIGINT
  sigdelset(&set,THR_CLIENT_ALARM);		// For alarms
  (void) pthread_sigmask(SIG_SETMASK,&set,NULL);

  VOID(pthread_attr_init(&thr_attr));
  pthread_attr_setscope(&thr_attr,PTHREAD_SCOPE_SYSTEM);
  VOID(pthread_attr_setdetachstate(&thr_attr,PTHREAD_CREATE_DETACHED));
  if (!(opt_specialflag & SPECIAL_NO_PRIOR))
    my_pthread_attr_setprio(&thr_attr,INTERRUPT_PRIOR);
  pthread_attr_setstacksize(&thr_attr,32768);

  VOID(pthread_mutex_lock(&LOCK_thread_count));
  if ((error=pthread_create(&signal_thread,&thr_attr,signal_hand,0)))
  {
    sql_print_error("Can't create interrupt-thread (error %d, errno: %d)",
		    error,errno);
    exit(1);
  }
  VOID(pthread_cond_wait(&COND_thread_count,&LOCK_thread_count));
  pthread_mutex_unlock(&LOCK_thread_count);

  VOID(pthread_attr_destroy(&thr_attr));
  DBUG_VOID_RETURN;
}


/*
** This threads handles all signals and alarms
*/

/* ARGSUSED */
static void *signal_hand(void *arg __attribute__((unused)))
{
  sigset_t set;
  int sig;
  my_thread_init();				// Init new thread
  DBUG_ENTER("signal_hand");

  init_thr_alarm(max_connections+3);		// Setup alarm handler
#if SIGINT != THR_KILL_SIGNAL
  (void) sigemptyset(&set);			// Setup up SIGINT for debug
  (void) sigaddset(&set,SIGINT);		// For debugging
  (void) pthread_sigmask(SIG_UNBLOCK,&set,NULL);
#endif
  (void) sigemptyset(&set);			// Setup up SIGINT for debug
#ifdef USE_ONE_SIGNAL_HAND
  (void) sigaddset(&set,THR_SERVER_ALARM);	// For alarms
#endif
  (void) sigaddset(&set,SIGQUIT);
  (void) sigaddset(&set,SIGTERM);
#if THR_CLIENT_ALARM != SIGHUP
  (void) sigaddset(&set,SIGHUP);
#endif
  (void) sigaddset(&set,SIGTSTP);

  /* Save pid to this process (or thread on Linux) */
  {
    FILE	*pidFile;
    if ((pidFile = my_fopen(pidfile_name,O_WRONLY,MYF(MY_WME))))
    {
      fprintf(pidFile,"%lu",(ulong) getpid());
      (void) my_fclose(pidFile,MYF(0));
      (void) chmod(pidfile_name,0644);
    }
  }
  (void) pthread_mutex_lock(&LOCK_thread_count);
  (void) pthread_cond_signal(&COND_thread_count); /* continue init_signals */
  pthread_mutex_unlock(&LOCK_thread_count);

  for (;;)
  {
    int error;
    while ((error=my_sigwait(&set,&sig)) == EINTR) ;
    if (cleanup_done)
      pthread_exit(0);				// Safety
    switch (sig) {
    case SIGTERM:
    case SIGQUIT:
    case SIGKILL:
      DBUG_PRINT("info",("Got signal: %d  abort_loop: %d",sig,abort_loop));
      if (!abort_loop)
      {
	abort_loop=1;				// mark abort for threads
#ifdef USE_ONE_SIGNAL_HAND
	pthread_t tmp;
	if (!(opt_specialflag & SPECIAL_NO_PRIOR))
	  my_pthread_attr_setprio(&connection_attrib,INTERRUPT_PRIOR);
	if (pthread_create(&tmp,&connection_attrib, kill_server_thread,
			   (void*) sig))
	  sql_print_error("Error: Can't create thread to kill server");
#else
	  kill_server((void*) sig);		// MIT THREAD has a alarm thread
#endif
      }
      break;
    case SIGHUP:
      reload_acl_and_cache(~0);			// Flush everything
      mysql_print_status();			// Send debug some info
      break;
#ifdef USE_ONE_SIGNAL_HAND
    case THR_SERVER_ALARM:
      process_alarm(sig);			// Trigger alarms.
      break;
#endif
    default:
#ifdef EXTRA_DEBUG
      sql_print_error("Warning: Got signal: %d, error: %d",sig,error); /* purecov: tested */
#endif
      break;					/* purecov: tested */
    }
  }
  return(0);					/* purecov: deadcode */
}

#endif	/* __WIN32__*/


	/* ARGSUSED */
static int my_message_sql(uint my_error, const char *str,
			  myf MyFlags __attribute__((unused)))
{
  NET *net;
  DBUG_ENTER("my_message_sql");
  DBUG_PRINT("error",("Message: '%s'",str));
  if ((net=my_pthread_getspecific_ptr(NET*,THR_NET)))
  {
    if (!net->last_error[0])			// Return only first message
    {
      strmake(net->last_error,str,sizeof(net->last_error)-1);
      net->last_errno=my_error ? my_error : ER_UNKNOWN_ERROR;
    }
  }
  else
    sql_print_error("%s: %s",my_progname,str); /* purecov: inspected */
  DBUG_RETURN(0);
}

#ifdef __WIN32__
#undef errno
#undef EINTR
#define errno WSAGetLastError()
#define EINTR WSAEINTR

struct utsname
{
  char nodename[FN_REFLEN];
};

int uname(struct utsname *a)
{
  return -1;
}
#endif


#ifdef __WIN32__
pthread_handler_decl(handle_shutdown,arg)
{
  MSG msg;
  my_thread_init();

  /* this call should create the message queue for this thread */
  PeekMessage(&msg, NULL, 1, 65534,PM_NOREMOVE);

  if (WaitForSingleObject(hEventShutdown,INFINITE)==WAIT_OBJECT_0)
     kill_server(MYSQL_KILL_SIGNAL);
  return 0;
}

int __stdcall handle_kill(ulong ctrl_type)
{
  if (ctrl_type == CTRL_CLOSE_EVENT ||
      ctrl_type == CTRL_SHUTDOWN_EVENT)
  {
    kill_server(MYSQL_KILL_SIGNAL);
    return TRUE;
  }
  return FALSE;
}
#endif

my_string load_default_groups[]= { "mysqld","server",0 };

#ifdef __WIN32__
int win_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
  DEBUGGER_OFF;
  char hostname[FN_REFLEN];

  if(getuid() || geteuid())
  { fprintf(stderr,"Access denied\n");
    exit(1);
  }

  my_umask=0660;		// Default umask for new files
  MY_INIT(argv[0]);		// init my_sys library & pthreads

  start_time=time((time_t*) 0);
  if (gethostname(hostname,sizeof(hostname)-4) < 0)
    strmov(hostname,"mysql");
  strxmov(pidfile_name,hostname,".pid",NullS);
#ifndef DBUG_OFF
  strcat(server_version,"-debug");
#endif
  load_defaults("my",load_default_groups,&argc,&argv);
  defaults_argv=argv;
  mysql_tmpdir=getenv("TMPDIR");        /* Use this if possible */
#ifdef __WIN32__
  if (!mysql_tmpdir)
    mysql_tmpdir=getenv("TEMP");
  if (!mysql_tmpdir)
    mysql_tmpdir=getenv("TMP");
#endif
  if (!mysql_tmpdir)
    mysql_tmpdir=P_tmpdir;		/* purecov: inspected */

#ifdef __NT__
  /* service parameters can be overwritten by options */
  if (get_service_parameters())
  {
    my_message( 0, "Can't read MySQL service parameters", MYF(0) );
    exit( 1 );
  }
#endif
  get_options(argc,argv);
  if (opt_log || opt_update_log)
    strcat(server_version,"-log");
  DBUG_PRINT("info",("%s  Ver %s for %s on %s\n",my_progname,
		     server_version, SYSTEM_TYPE,MACHINE_TYPE));

  if (!(opt_specialflag & SPECIAL_NO_PRIOR))
    my_pthread_setprio(pthread_self(),CONNECT_PRIOR);
  /* Parameter for threads created for connections */
  VOID(pthread_attr_init(&connection_attrib));
  VOID(pthread_attr_setdetachstate(&connection_attrib,
				   PTHREAD_CREATE_DETACHED));
  pthread_attr_setstacksize(&connection_attrib,thread_stack);

  if (!(opt_specialflag & SPECIAL_NO_PRIOR))
    my_pthread_attr_setprio(&connection_attrib,WAIT_PRIOR);
  pthread_attr_setscope(&connection_attrib, PTHREAD_SCOPE_SYSTEM);

  VOID(pthread_cond_init(&COND_thread_count,NULL));
  VOID(pthread_mutex_init(&LOCK_mysql_create_db,NULL));
  VOID(pthread_mutex_init(&LOCK_Acl,NULL));
  VOID(pthread_mutex_init(&LOCK_grant,NULL));
  VOID(pthread_mutex_init(&LOCK_open,NULL));
  VOID(pthread_mutex_init(&LOCK_thread_count,NULL));
  VOID(pthread_mutex_init(&LOCK_mapped_file,NULL));
  VOID(pthread_mutex_init(&LOCK_status,NULL));
  VOID(pthread_mutex_init(&LOCK_log,NULL));
  VOID(pthread_cond_init(&COND_refresh,NULL));

#ifdef SET_RLIMIT_NOFILE
  /* connections and databases neads lots of files */
  {
    uint wanted_files=10+(uint) max(max_connections*5,
				    max_connections+table_cache_size*2);
    uint files=set_maximum_open_files(wanted_files);
    if (files && files < wanted_files)		// Some systems return 0
    {
      max_connections=	(ulong) (files-10)/5;
      table_cache_size= (ulong) (files-10-max_connections)/2;
      DBUG_PRINT("warning",
		 ("Changed limits: max_connections: %ld  table_cache: %ld",
		  max_connections,table_cache_size));
      sql_print_error("Warning: Changed limits: max_connections: %ld  table_cache: %ld",max_connections,table_cache_size);
    }
  }
#endif
  unireg_init(opt_specialflag); /* Set up extern variabels */
  init_errmessage();		/* Read error messages from file */
  lex_init();
  item_init();
  mysys_uses_curses=0;
#ifdef USE_REGEX
  regex_init();
#endif
  select_thread=pthread_self();
  select_thread_in_use=1;

  /*
  ** We have enough space for fiddling with the argv, continue
  */
  umask(((~my_umask) & 0666));
  if (my_setwd(mysql_real_data_home,MYF(MY_WME)))
  {
    unireg_abort(1);				/* purecov: inspected */
  }
  mysql_data_home[0]=FN_CURLIB;		// all paths are relative from here
  mysql_data_home[1]=0;
  server_init();
  table_cache_init();
  hostname_cache_init();
  randominit(&sql_rand,(ulong) start_time,(ulong) start_time/2);

  /* Setup log files */
  if (opt_log)
  {
    char tmp[FN_REFLEN];
    if (!opt_logname)
    {
      strmov(strnmov(tmp,hostname,FN_REFLEN-5),".log");
      opt_logname=tmp;
    }
    mysql_log.open(opt_logname,MYSQL_LOG::NORMAL);
  }
  if (opt_update_log)
    mysql_update_log.open(opt_update_logname ? opt_update_logname :
			  hostname,MYSQL_LOG::NEW);
#ifdef __WIN32__
#define MYSQL_ERR_FILE "mysql.err"
  if (!opt_console)
  {
    freopen(MYSQL_ERR_FILE,"a+",stdout);
    freopen(MYSQL_ERR_FILE,"a+",stderr);
    FreeConsole();				// Remove window
  }
#endif

  /*
    init signals & alarm
    After this we can't quit by a simple unireg_abort
  */
  error_handler_hook = my_message_sql;
  if (pthread_key_create(&THR_THD,NULL) || pthread_key_create(&THR_NET,NULL) ||
      pthread_key_create(&THR_MALLOC,NULL))
  {
    sql_print_error(0,"Can't create thread-keys",MYF(0));
    exit(1);
  }
  init_signals();
  if (acl_init(opt_noacl))
  {
    select_thread_in_use=0;
    (void) pthread_kill(signal_thread,MYSQL_KILL_SIGNAL);
    (void) my_delete(pidfile_name,MYF(0));		// Not neaded anymore
    exit(1);
  }
  if (!opt_noacl && !(specialflag &  SPECIAL_NO_NEW_FUNC))
    (void) grant_init();

#ifdef HAVE_DLOPEN
  if (!opt_noacl)
    udf_init();
#endif

  if (opt_bootstrap)
    exit(bootstrap());
#ifdef HAVE_THR_SETCONCURRENCY
  VOID(thr_setconcurrency(20));			// We are iobound
#endif
#ifdef __WIN32__			       //IRENA
  {
    hEventShutdown=CreateEvent(0, FALSE, FALSE, "MySqlShutdown");
    pthread_t hThread;
    if (pthread_create(&hThread,&connection_attrib,handle_shutdown,0))
      sql_print_error("Warning: Can't create thread to handle shutdown requests");

    // On "Stop Service" we have to do regular shutdown
    Service.SetShutdownEvent(hEventShutdown);
  }
#endif

  printf(ER(ER_READY),my_progname,server_version,"");
  fflush(stdout);

#ifdef __NT__
  if (hPipe == INVALID_HANDLE_VALUE && !have_tcpip)
  {
    sql_print_error("TCP/IP must be installed on Win98 platforms");
  }
  else
  {
    pthread_mutex_lock(&LOCK_thread_count);
    VOID(pthread_cond_init(&COND_handler_count,NULL));
    {
      pthread_t hThread;
      handler_count=0;
      if ( hPipe != INVALID_HANDLE_VALUE )
      {
	handler_count++;
	if (pthread_create(&hThread,&connection_attrib,
			   handle_connections_namedpipes, 0))
	{
	  sql_print_error("Warning: Can't create thread to handle named pipes");
	  handler_count--;
	}
      }
      if (have_tcpip)
      {
	handler_count++;
	if (pthread_create(&hThread,&connection_attrib,
			   handle_connections_sockets, 0))
	{
	  sql_print_error("Warning: Can't create thread to handle named pipes");
	  handler_count--;
	}
      }
      while (handler_count > 0)
      {
	pthread_cond_wait(&COND_handler_count,&LOCK_thread_count);
      }
    }
    pthread_mutex_unlock(&LOCK_thread_count);
  }
#else
  handle_connections_sockets(0);
#endif /* __NT__ */

  VOID(pthread_attr_destroy(&connection_attrib));
  (void) my_delete(pidfile_name,MYF(0));	// Not neaded anymore

  DBUG_PRINT("quit",("Exiting main thread"));
  my_thread_end();

#ifndef __WIN32__
  VOID(pthread_mutex_lock(&LOCK_thread_count));
  select_thread_in_use=0;			// For close_connections
  VOID(pthread_cond_signal(&COND_thread_count));
  VOID(pthread_mutex_unlock(&LOCK_thread_count));
#else
  // remove the event, because it will not be valid anymore
  Service.SetShutdownEvent(0);
  if(hEventShutdown) CloseHandle(hEventShutdown);
  // if it was started as service on NT try to stop the service
  if(Service.IsNT())
     Service.Stop();
#endif

  pthread_exit(0);
  return(0);					/* purecov: deadcode */
}


#ifdef __WIN32__
/* ------------------------------------------------------------------------
   main and thread entry function for Win32
   (all this is needed only to run mysqld as a service on WinNT)
 -------------------------------------------------------------------------- */
int mysql_service(void *p)
{
  win_main(Service.my_argc, Service.my_argv);
  return 0;
}

int main(int argc, char **argv)
{
  // check  environment variable OS
  if (Service.GetOS())  // "OS" defined; Should be NT
  {
    if (argc == 2)
    {
      if (!strcmp(argv[1],"-install") || !strcmp(argv[1],"--install"))
      {
	char path[FN_REFLEN];
	my_path(path, argv[0], "");                // Find name in path
	fn_format(path,argv[0],path,"",1+4+16);	   // Force use of full path
	if (!Service.Install("MySql","MySql",path))
	  MessageBox(NULL,"Failed to install Service","MySql",
		     MB_OK|MB_ICONSTOP);
	return 0;
      }
      else if (!strcmp(argv[1],"-remove") || !strcmp(argv[1],"--remove"))
      {
	Service.Remove("MySql");
	return 0;
      }
    }
    else if (argc == 1)            // No arguments; start as a service
    {
      // init service
      long tmp=Service.Init("MySql",mysql_service);
      return 0;
    }
  }

  // This is a WIN95 machine or a start of mysqld as a standalone program
  // we have to pass the arguments, in case of NT-service this will be done
  // by ServiceMain()

  Service.my_argc=argc;
  Service.my_argv=argv;
  mysql_service(NULL);
  return 0;
}
/* ------------------------------------------------------------------------ */
#endif


static int bootstrap()
{
  THD *thd= new THD;
  int error;
  thd->bootstrap=1;
  thd->client_capabilities=0;
  my_net_init(&thd->net,NET_TYPE_TCPIP, -1, 0);
  thd->max_packet_length=thd->net.max_packet;
  thd->master_access=~0;
  thread_count++;
  thd->real_id=pthread_self();
  error=handle_bootstrap(thd);
  net_end(&thd->net);
  delete thd;
  end_thr_alarm();			 	// Don't allow alarms
  (void) my_delete(pidfile_name,MYF(0));	// Not neaded anymore
  clean_up();
  return error ? 1 : 0;
}


static void create_new_thread(THD *thd)
{
  DBUG_ENTER("create_new_thread");

  NET *net=&thd->net;				// For easy ref
  net->timeout = (uint) connect_timeout;	// Timeout for read
  if (protocol_version > 9)
    net->return_errno=1;

  /* don't allow too many connections */
  if (thread_count >= max_connections+1 || abort_loop)
  {
    DBUG_PRINT("error",("too many connections"));
    close_connection(net,ER_CON_COUNT_ERROR);
    delete thd;
    DBUG_VOID_RETURN;
  }

  if (pthread_mutex_lock(&LOCK_thread_count))
  {
    DBUG_PRINT("error",("Can't lock LOCK_thread_count"));
    close_connection(net,ER_OUT_OF_RESOURCES);
    delete thd;
    DBUG_VOID_RETURN;
  }
  thd->thread_id=thread_id++;
  for (uint i=0; i < 8 ; i++)			// Generate password teststring
    thd->scramble[i]= (char) (rnd(&sql_rand)*94+33);
  thd->scramble[8]=0;
  thd->rand=sql_rand;
  threads.append(thd);

  /* Start a new thread to handle connection */

#ifndef DBUG_OFF
  if (test_flags & TEST_NO_THREADS)		// For debugging under Linux
  {
    thread_count++;
    thd->real_id=pthread_self();
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
    handle_one_connection((void*) thd);
  }
  else
#endif
  {
    int error;
    if ((error=pthread_create(&thd->real_id,&connection_attrib,
			      handle_one_connection,
			      (void*) thd)))
    {
      DBUG_PRINT("error",
		 ("Can't create thread to handle request (error %d)",
		  error));
      thd->killed=1;				// Safety
      VOID(pthread_mutex_unlock(&LOCK_thread_count));
      net_printf(net,ER_CANT_CREATE_THREAD,error);
      VOID(pthread_mutex_lock(&LOCK_thread_count));
      close_connection(net,0,0);
      delete thd;
      VOID(pthread_mutex_unlock(&LOCK_thread_count));
      DBUG_VOID_RETURN;
    }
    thread_count++;
    VOID(pthread_mutex_unlock(&LOCK_thread_count));
  }
  DBUG_PRINT("info",(("Thread created")));
  DBUG_VOID_RETURN;
}


	/* Handle new connections and spawn new process to handle them */

pthread_handler_decl(handle_connections_sockets,arg)
{
  Socket sock,new_sock;
  uint error_count=0;
  uint max_used_connection= (uint) (max(ip_sock,unix_sock)+1);
  fd_set readFDs,clientFDs;
  THD *thd;
  struct sockaddr_in cAddr;
  int ip_flags=0,socket_flags=0,flags;
#ifdef __WIN32__
  my_thread_init();
#endif
  DBUG_ENTER("handle_connections_sockets");

  LINT_INIT(new_sock);

  (void) my_pthread_getprio(pthread_self());		// For debugging

  FD_ZERO(&clientFDs);
  if (ip_sock != INVALID_SOCKET)
  {
    FD_SET(ip_sock,&clientFDs);
#ifdef HAVE_FCNTL
    ip_flags = fcntl(ip_sock, F_GETFL, 0);
#endif
  }
#ifdef HAVE_SYS_UN_H
  FD_SET(unix_sock,&clientFDs);
  socket_flags=fcntl(unix_sock, F_GETFL, 0);
#endif

  DBUG_PRINT("general",("Waiting for connections."));
  while (!abort_loop)
  {
    memcpy(&readFDs,&clientFDs,sizeof(readFDs));
#ifdef HPUX
    if (select(max_used_connection,(int*) &readFDs,0,0,0) < 0)
      continue;
#else
    if (select((int) max_used_connection,&readFDs,0,0,0) < 0)
    {
      if (errno != EINTR)
      {
	if (!select_errors++ && !abort_loop)	/* purecov: inspected */
	  sql_print_error("mysqld: Got error %d from select",errno); /* purecov: inspected */
      }
      continue;
    }
#endif	/* HPUX */
    if (abort_loop)
      break;

    /*
    ** Is this a new connection request
    */

#ifdef HAVE_SYS_UN_H
    if (FD_ISSET(unix_sock,&readFDs))
    {
      sock = unix_sock;
      flags= socket_flags;
    }
    else
#endif
    {
      sock = ip_sock;
      flags= ip_flags;
    }

#if !defined(NO_FCNTL_NONBLOCK)
    if (!(test_flags & TEST_BLOCKING))
    {
#if defined(O_NONBLOCK)
      fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#elif defined(O_NDELAY)
      fcntl(sock, F_SETFL, flags | O_NDELAY);
#endif
    }
#endif /* NO_FCNTL_NONBLOCK */
    for (uint retry=0; retry < MAX_ACCEPT_RETRY; retry++)
    {
      size_socket length=sizeof(struct sockaddr_in);
      new_sock = accept(sock, (struct sockaddr *)&cAddr, &length);
      if (new_sock != INVALID_SOCKET || (errno != EINTR && errno != EAGAIN))
	break;
#if !defined(NO_FCNTL_NONBLOCK)
      if (!(test_flags & TEST_BLOCKING))
      {
	if (retry == MAX_ACCEPT_RETRY - 1)
	  fcntl(sock, F_SETFL, flags);		// Try without O_NONBLOCK
      }
#endif
    }
#if !defined(NO_FCNTL_NONBLOCK)
    if (!(test_flags & TEST_BLOCKING))
      fcntl(sock, F_SETFL, flags);
#endif
    if (new_sock < 0)
    {
      if ((error_count++ & 255) == 0)		// This can happen often
	sql_perror("Error in accept");
      if (errno == ENFILE || errno == EMFILE)
	sleep(1);				// Give other threads some time
      continue;
    }
    {
      size_socket dummyLen;
      struct sockaddr dummy;
      dummyLen = sizeof(struct sockaddr);
      if (getsockname(new_sock,&dummy, &dummyLen) < 0)
      {
	sql_perror("Error on new connection socket");
	VOID(shutdown(new_sock,2));
	VOID(closesocket(new_sock));
	continue;
      }
    }

    /*
    ** Don't allow too many connections
    */

    if (!(thd= new THD))
    {
      VOID(shutdown(new_sock,2)); VOID(closesocket(new_sock));
      continue;
    }
#ifdef __WIN32__
    if (my_net_init(&thd->net, NET_TYPE_TCPIP, new_sock, 0))
#else
    if (my_net_init(&thd->net,
		    new_sock == unix_sock ? NET_TYPE_SOCKET : NET_TYPE_TCPIP,
		    new_sock, 0))
#endif
    {
      VOID(shutdown(new_sock,2)); VOID(closesocket(new_sock));
      delete thd;
      continue;
    }
    if (sock == unix_sock)
    {
      if (!(thd->host=my_strdup(LOCAL_HOST,MYF(0))))
      {
	close_connection(&thd->net,ER_OUT_OF_RESOURCES);
	delete thd;
	continue;
      }
    }
    create_new_thread(thd);
  }

#ifdef __NT__
  pthread_mutex_lock(&LOCK_thread_count);
  handler_count--;
  pthread_cond_signal(&COND_handler_count);
  pthread_mutex_unlock(&LOCK_thread_count);
#endif
  DBUG_RETURN(0);
}


#ifdef __NT__
pthread_handler_decl(handle_connections_namedpipes,arg)
{
  HANDLE hConnectedPipe;
  BOOL fConnected;
  THD *thd;
  my_thread_init();
  DBUG_ENTER("handle_connections_namedpipes");
  (void) my_pthread_getprio(pthread_self());		// For debugging

  DBUG_PRINT("general",("Waiting for named pipe connections."));
  while (!abort_loop)
  {
    /* wait for named pipe connection */
    fConnected = ConnectNamedPipe( hPipe, NULL );
    if (abort_loop)
      break;
    if ( !fConnected )
      fConnected = GetLastError() == ERROR_PIPE_CONNECTED;
    if ( !fConnected )
    {
      CloseHandle( hPipe );
      if ((hPipe = CreateNamedPipe(szPipeName,
				   PIPE_ACCESS_DUPLEX,
				   PIPE_TYPE_BYTE |
				   PIPE_READMODE_BYTE |
				   PIPE_WAIT,
				   PIPE_UNLIMITED_INSTANCES,
				   (int) net_buffer_length,
				   (int) net_buffer_length,
				   NMPWAIT_USE_DEFAULT_WAIT,
				   &saPipeSecurity )) ==
	  INVALID_HANDLE_VALUE )
      {
	sql_perror("Can't create new named pipe!");
	continue;
      }
    }
    hConnectedPipe = hPipe;
    /* create new pipe for new connection */
    if ((hPipe = CreateNamedPipe(szPipeName,
				 PIPE_ACCESS_DUPLEX,
				 PIPE_TYPE_BYTE |
				 PIPE_READMODE_BYTE |
				 PIPE_WAIT,
				 PIPE_UNLIMITED_INSTANCES,
				 (int) net_buffer_length,
				 (int) net_buffer_length,
				 NMPWAIT_USE_DEFAULT_WAIT,
				 &saPipeSecurity)) ==
	INVALID_HANDLE_VALUE)
    {
      sql_perror("Can't create new named pipe!");
      DisconnectNamedPipe( hConnectedPipe );
      CloseHandle( hConnectedPipe );
      continue;					// We have to try again
    }

    if ( !(thd = new THD))
    {
      DisconnectNamedPipe( hConnectedPipe );
      CloseHandle( hConnectedPipe );
      continue;
    }
    if (my_net_init(&thd->net, NET_TYPE_NAMEDPIPE, (Socket) 0,
		    &hConnectedPipe))
    {
      close_connection(&thd->net,ER_OUT_OF_RESOURCES);
      delete thd;
      continue;
    }
    /* host name is unknown */
    thd->host = NULL;
    create_new_thread(thd);
  }

  pthread_mutex_lock(&LOCK_thread_count);
  handler_count--;
  pthread_cond_signal(&COND_handler_count);
  pthread_mutex_unlock(&LOCK_thread_count);
  DBUG_RETURN(0);
}
#endif /* __NT__ */



/******************************************************************************
** handle start options
******************************************************************************/

enum options {OPT_ISAM_LOG=256,OPT_SKIP_NEW,OPT_SKIP_GRANT,
	      OPT_SKIP_LOCK,OPT_ENABLE_LOCK,
	      OPT_USE_LOCKING,OPT_SOCKET,OPT_UPDATE_LOG,
	      OPT_SKIP_RESOLVE,OPT_SKIP_NETWORKING,
	      OPT_BIND_ADDRESS,OPT_PID_FILE,OPT_SKIP_PRIOR,OPT_BIG_TABLES,
	      OPT_STANDALONE,OPT_ONE_THREAD,OPT_CONSOLE,
	      OPT_LOW_PRIORITY_UPDATES,OPT_SKIP_HOST_CACHE,OPT_LONG_FORMAT,
	      OPT_FLUSH, OPT_SAFE, OPT_BOOTSTRAP};

static struct option long_options[] =
{
  {"basedir",	required_argument, 0, 'b'},
  {"big-tables", no_argument,0,(int) OPT_BIG_TABLES},
  {"bind-address", required_argument, 0, OPT_BIND_ADDRESS},
  {"bootstrap", no_argument,0,(int) OPT_BOOTSTRAP},
#ifdef __WIN32__
  {"console",	no_argument,	  0,  OPT_CONSOLE},
#endif
  {"chroot",	required_argument,0,  'r'},
  {"datadir",	required_argument, 0, 'h'},
#ifndef DBUG_OFF
  {"debug",	optional_argument, 0, '#'},
#endif
  {"enable-locking", no_argument,  0, (int) OPT_ENABLE_LOCK},
  {"exit-info", optional_argument, 0, 'T'},
#ifdef __WIN32__
  {"flush",	no_argument,	  0,  OPT_FLUSH},
#endif
  {"help",	no_argument,	   0, '?'},
  {"log",	optional_argument, 0, 'l'},
  {"language",	required_argument, 0, 'L'},
  {"log-isam",	optional_argument, 0, (int) OPT_ISAM_LOG},
  {"log-update",optional_argument, 0, (int) OPT_UPDATE_LOG},
  {"log-long-format",no_argument, 0, (int) OPT_LONG_FORMAT},
  {"low-priority-insert", no_argument, 0, (int) OPT_LOW_PRIORITY_UPDATES},
  {"new",	no_argument,	   0, 'n'},
  {"old-protocol", no_argument,    0, 'o'},
  {"one-thread", no_argument,	   0, OPT_ONE_THREAD},
  {"pid-file",	required_argument, 0, (int) OPT_PID_FILE},
  {"port",	required_argument, 0, 'P'},
  {"safe-mode", no_argument,	   0, (int) OPT_SAFE},
  {"socket",	required_argument, 0, (int) OPT_SOCKET},
  {"set-variable",required_argument, 0, 'O'},
  {"skip-grant-tables", no_argument,0,(int) OPT_SKIP_GRANT},
  {"skip-locking",	no_argument,0,(int) OPT_SKIP_LOCK},
  {"skip-host-cache", no_argument,0,(int) OPT_SKIP_HOST_CACHE},
  {"skip-name-resolve", no_argument,0,(int) OPT_SKIP_RESOLVE},
  {"skip-new",	 no_argument,	   0, (int) OPT_SKIP_NEW},
  {"skip-networking",	no_argument,0, (int) OPT_SKIP_NETWORKING},
  {"skip-thread-priority",	no_argument,0,(int) OPT_SKIP_PRIOR},
  {"standalone",	no_argument,0, (int) OPT_STANDALONE},
  {"tmpdir",	required_argument  ,0, 't'},
  {"use-locking",	no_argument,0,(int) OPT_USE_LOCKING},
  {"user",	required_argument,0,  'u'},
  {"version",	no_argument,	   0, 'V'},
  {0, 0, 0, 0}
};

CHANGEABLE_VAR changeable_vars[] = {
  { "back_log", (long*) &back_log,5,1,65535,0,1},
  { "connect_timeout", (long*) &connect_timeout,CONNECT_TIMEOUT,1,65535,0,1},
  { "join_buffer", (long*) &join_buff_size,128*1024L,
    IO_SIZE*2+MALLOC_OVERHEAD,~0L,MALLOC_OVERHEAD,IO_SIZE },
  { "key_buffer", (long*) &keybuff_size,KEY_CACHE_SIZE,MALLOC_OVERHEAD,
    (long) ~0, MALLOC_OVERHEAD, IO_SIZE },
  { "long_query_time", (long*) &long_query_time,10,1,~0L,0,1},
  { "max_allowed_packet",(long*) &max_allowed_packet,1024*1024L,16384,~0L,
    MALLOC_OVERHEAD,1024},
  { "max_connections", (long*) &max_connections,90,1,16384,0,1},
  { "max_connect_errors", (long*) &max_connect_errors,MAX_CONNECT_ERRORS,1,
    ~0L,0,1},
  { "max_join_size",(long*) &max_join_size,~0L,1,~0L,0,1},
  { "max_sort_length",(long*) &max_item_sort_length,1024,4,8192*1024L,0,1},
  { "net_buffer_length",(long*) &net_buffer_length,16384,1024,1024*1024L,
    MALLOC_OVERHEAD,1024},
  { "record_buffer", (long*) &my_default_record_cache_size,128*1024L,
    IO_SIZE*2+MALLOC_OVERHEAD,~0L,MALLOC_OVERHEAD,IO_SIZE },
  { "sort_buffer", (long*) &sortbuff_size,MAX_SORT_MEMORY,
    MIN_SORT_MEMORY+MALLOC_OVERHEAD,~0L,MALLOC_OVERHEAD,1 },
  { "table_cache",  (long*) &table_cache_size,64,1,16384,0,1},
  { "tmp_table_size",  (long*) &tmp_table_size,1024*1024L,1024,~0L,
    MALLOC_OVERHEAD,1},
  { "thread_stack", (long*) &thread_stack,1024*64,1024*32,~0L,0,1024},
  { "wait_timeout", (long*) &net_wait_timeout,NET_WAIT_TIMEOUT,1,~0L,0,1},
  { NullS,(long*) 0,0,0,0,0,0,} };


struct show_var_st init_vars[]= {
  {"back_log", (char*) &back_log, SHOW_LONG},
  {"connect_timeout", (char*) &connect_timeout, SHOW_LONG},
  {"basedir",  mysql_home,SHOW_CHAR},
  {"datadir",  mysql_real_data_home,SHOW_CHAR},
  {"join_buffer", (char*) &join_buff_size, SHOW_LONG},
#ifdef __WIN32__
  {"flush",	(char*) &nisam_flush, SHOW_MY_BOOL},
#endif
  {"key_buffer", (char*) &keybuff_size, SHOW_LONG},
  {"language", language, SHOW_CHAR},
  {"log", (char*) &opt_log, SHOW_BOOL},
  {"long_query_time", (char*) &long_query_time, SHOW_LONG},
  {"low_priority_updates", (char*) &low_priority_updates, SHOW_BOOL},
  {"max_allowed_packet", (char*) &max_allowed_packet, SHOW_LONG},
  {"max_connections", (char*) &max_connections, SHOW_LONG},
  {"max_connect_errors", (char*) &max_connect_errors, SHOW_LONG},
  {"max_join_size", (char*) &max_join_size, SHOW_LONG},
  {"max_sort_length", (char*) &max_item_sort_length,SHOW_LONG},
  {"net_buffer_length", (char*) &net_buffer_length, SHOW_LONG},
  {"port", (char*) &mysql_port, SHOW_INT},
  {"record_buffer", (char*) &my_default_record_cache_size,SHOW_LONG},
  {"skip_locking", (char*) &my_disable_locking, SHOW_MY_BOOL},
  {"socket", (char*) &mysql_unix_port, SHOW_CHAR_PTR},
  {"sort_buffer", (char*) &sortbuff_size,SHOW_LONG},
  {"table_cache", (char*) &table_cache_size,SHOW_LONG},
  {"thread_stack", (char*) &thread_stack,SHOW_LONG},
  {"tmp_table_size", (char*) &tmp_table_size,SHOW_LONG},
  {"tmpdir",(char*) &mysql_tmpdir,SHOW_CHAR_PTR},
  {"update_log",(char*) &opt_update_log,SHOW_BOOL},
  {"wait_timeout",(char*) &net_wait_timeout,SHOW_LONG},
  {NullS,NullS,SHOW_LONG}
};

struct show_var_st status_vars[]= {
  {"Aborted_clients", (char*) &aborted_threads, SHOW_LONG},
  {"Aborted_connects", (char*) &aborted_connects, SHOW_LONG},
  {"Created_tmp_tables", (char*) &created_tmp_tables, SHOW_LONG},
  {"Deletes", (char*) &ha_delete_count, SHOW_LONG},
  {"Flush_commands", (char*) &refresh_version, SHOW_LONG_CONST},
  {"Key_blocks_used", (char*) &_my_blocks_used, SHOW_LONG},
  {"Key_read_requests", (char*) &_my_cache_r_requests, SHOW_LONG},
  {"Key_reads", (char*) &_my_cache_read, SHOW_LONG},
  {"Key_write_requests", (char*) &_my_cache_w_requests, SHOW_LONG},
  {"Key_writes", (char*) &_my_cache_write, SHOW_LONG},
  {"Not_flushed_key_blocks", (char*) &_my_blocks_changed, SHOW_LONG_CONST},
  {"Open_tables", (char*) 0, SHOW_OPENTABLES},
  {"Open_files", (char*) &my_file_opened, SHOW_LONG_CONST},
  {"Open_streams", (char*) &my_stream_opened, SHOW_LONG_CONST},
  {"Opened_tables", (char*) &opened_tables, SHOW_LONG},
  {"Questions", (char*) 0, SHOW_QUESTION},
  {"Read_key", (char*) &ha_read_key_count, SHOW_LONG},
  {"Read_next", (char*) &ha_read_next_count, SHOW_LONG},
  {"Read_rnd", (char*) &ha_read_rnd_count, SHOW_LONG},
  {"Read_first", (char*) &ha_read_first_count, SHOW_LONG},
  {"Running_threads", (char*) &thread_count, SHOW_LONG_CONST},
  {"Slow_queries", (char*) &long_query_count, SHOW_LONG},
  {"Uptime", (char*) 0, SHOW_STARTTIME},
  {"Write", (char*) &ha_write_count, SHOW_LONG},
  {NullS,NullS,SHOW_LONG}
};

static void print_version(void)
{
  printf("%s  Ver %s for %s on %s\n",my_progname,
	 server_version,SYSTEM_TYPE,MACHINE_TYPE);
}

static void usage(void)
{
  print_version();
  puts("Copyright (C) 1979-1998 TcX AB & Monty Program KB & Detron HB.");
  puts("All rights reserved. Se the file PUBLIC for licence information.");
  puts("This software comes with ABSOLUTELY NO WARRANTY: see the file PUBLIC for details.\n");
  puts("Starts the mysql server\n");

  printf("Usage: %s [OPTIONS]\n", my_progname);
  puts("\n\
  -b, --basedir=path	Path to installation directory. All paths are\n\
			usually resolved relative to this\n\
  --big-tables		Allow big result sets by saving all temporary sets\n\
			on file (Solves most 'table full' errors)\n\
  --bind-address=IP	Ip address to bind to\n\
  --bootstrap		Used by mysql installation scripts\n\
  --chroot=path		Chroot mysqld daemon during startup\n\
  -h, --datadir=path	Path to the database root");
#ifndef DBUG_OFF
  printf("\
  -#, --debug[=...]     Debug log. Default is '%s'\n",default_dbug_option);
#endif
  puts("\
  --enable-locking	Enable system locking\n\
  -T, --exit-info	Print some debug info at exit\n\
  -?, --help		Display this help and exit\n\
  -L, --language=...	Client error messages in given language. May be\n\
			given as a full path\n\
  -l, --log[=file]	Log connections and queries to file\n\
  --log-update[=file]	Log updates to file.# where # is a unique number\n\
			if not given.\n\
  --log-isam[=file]	Log all isam changes to file\n\
  --log-long-format	Log some extra information to update log\n\
  --low-priorty-insert	Inserts has lower priority than selects\n\
  --pid-file=path	Pid file used by safe_mysqld\n\
  -P, --port=...	Port number to use for connection\n\
  -n, --new		Use very new possible 'unsafe' functions\n\
  -o, --old-protocol	Use the old (3.20) protocol\n\
  --one-thread		Only use one thread (for debugging under Linux)\n\
  -O, --set-variable var=option\n\
			Give a variable an value. --help lists variables\n\
  -Sg, --skip-grant-tables\n\
			Start without grant tables. This gives all users\n\
			FULL ACCESS to all tables!\n\
  --safe-mode		Skip some optimize stages (for testing)\n\
  --skip-locking	Don't use system locking. To use isamchk one has\n\
			to shut down the server.\n\
  --skip-name-resolve	Don't resolve hostnames.\n\
			All hostnames are IP's or 'localhost'\n\
  --skip-networking	Don't allow connection with TCP/IP.\n\
  --skip-new		Don't use new, possible wrong routines.\n\
  --skip-host-cache	Don't cache host names\n\
  --skip-thread-priority\n\
			Don't give threads different priorities.\n\
  --socket=...		Socket file to use for connection\n\
  -t, --tmpdir=path	Path for temporary files\n\
  --user=user_name	Run mysqld daemon as user\n\
  -V, --version		output version information and exit\n");
#ifdef __WIN32__
  puts("NT and Win32 specific options:\n\
  --console		Don't remove the console window\n\
  --flush		Flush tables to disk between SQL commands\n\
  --install		Install mysqld as a service (NT)\n\
  --remove		Remove mysqld from the service list (NT)\n\
  --standalone		Dummy option to start as a standalone program (NT)\n\
");
#endif

  fix_paths();
  set_ports();
  printf("\
To see what values a running MySQL server is using, type\n\
'mysqladmin variables' instead of 'mysqld --help'.\n\
The default values (after parsing the command line arguments) are:\n\n");

  printf("basedir:     %s\n",mysql_home);
  printf("datadir:     %s\n",mysql_real_data_home);
  printf("tmpdir:      %s\n",mysql_tmpdir);
  printf("language:    %s\n",language);
  printf("pid file:    %s\n",pidfile_name);
  if (opt_logname)
    printf("logfile:     %s\n",opt_logname);
  if (opt_update_logname)
    printf("update log:  %s\n",opt_update_logname);
  printf("TCP port:    %d\n",mysql_port);
#if defined(HAVE_SYS_UN_H) && !defined(HAVE_mit_thread)
  printf("Unix socket: %s\n",mysql_unix_port);
#endif
  if (my_disable_locking)
    puts("\nsystem locking is not in use");
  if (opt_noacl)
    puts("\nGrant tables are not used. All users have full access rights");
  printf("\nPossible variables for option --set-variable (-O) are:\n");
  for (uint i=0 ; changeable_vars[i].name ; i++)
    printf("%-20s  current value: %lu\n",
	   changeable_vars[i].name,
	   (ulong) *changeable_vars[i].varptr);
}


static void set_options(void)
{
  char* tmpenv;

  set_all_changeable_vars( changeable_vars );
#if !defined( my_pthread_setprio ) && !defined( HAVE_PTHREAD_SETSCHEDPARAM )
  opt_specialflag |= SPECIAL_NO_PRIOR;
#endif

  (void) strmov( language, LANGUAGE);
  (void) strmov( mysql_real_data_home, get_relative_path(DATADIR));
  if ( !(tmpenv = getenv("MY_BASEDIR_VERSION")) )
    tmpenv = DEFAULT_MYSQL_HOME;
  (void) strmov( mysql_home, tmpenv );
#if defined( HAVE_mit_thread ) || defined( __WIN32__ ) || defined( HAVE_LINUXTHREADS )
  my_disable_locking = 1;
#endif
  my_bind_addr = htonl( INADDR_ANY );
}

	/* Initiates DEBUG - but no debugging here ! */

static void get_options(int argc,char **argv)
{
  int c,option_index=0;

#ifndef __NT__
  set_options();
#endif

  while ((c=getopt_long(argc,argv,"b:h:#::T::?l::L:O:P:S::t:u:noVvI?",
			long_options, &option_index)) != EOF)
  {
    switch(c) {
#ifndef DBUG_OFF
    case '#':
      DBUG_PUSH(optarg ? optarg : default_dbug_option);
      opt_endinfo=1;				/* unireg: memory allocation */
      break;
#endif
    case 'b':
      strmov(mysql_home,optarg);
      break;
    case 'l':
      opt_log=1;
      opt_logname=optarg;			// Use hostname.log if null
      break;
    case 'h':
      strmov(mysql_real_data_home,optarg);
      break;
    case 'L':
      strmov(language,optarg);
      break;
    case 'n':
      opt_specialflag|= SPECIAL_NEW_FUNC;
      break;
    case 'o':
      protocol_version=PROTOCOL_VERSION-1;
      break;
    case 'O':
      if (set_changeable_var(optarg, changeable_vars))
      {
	usage();
	exit(1);
      }
      break;
    case 'P':
      mysql_port= (unsigned int) atoi(optarg);
      break;
    case OPT_SOCKET:
      mysql_unix_port= optarg;
      break;
    case 'r':
      mysqld_chroot=optarg;
      break;
    case 't':
      mysql_tmpdir=optarg;
      break;
    case 'u':
      mysqld_user=optarg;
      break;
    case 'v':
    case 'V':
      print_version();
      exit(0);
    case 'I':
    case '?':
      usage();
      exit(0);
    case 'T':
      test_flags= optarg ? (uint) atoi(optarg) : (uint) ~0;
      opt_endinfo=1;
      break;
    case 'S':
      if (!optarg)
	opt_specialflag|= SPECIAL_NO_NEW_FUNC | SPECIAL_SAFE_MODE;
      else if (!strcmp(optarg,"l"))
	my_disable_locking=1;
      else if (!strcmp(optarg,"g"))
	opt_noacl=1;
      else
      {
	usage();
	exit(1);
      }
      break;
    case (int) OPT_BIG_TABLES:
      thd_startup_options|=OPTION_BIG_TABLES;
      break;
    case (int) OPT_ISAM_LOG:
      if (optarg)
	nisam_log_filename=optarg;
      (void) ni_log(1);
      break;
    case (int) OPT_UPDATE_LOG:
      opt_update_log=1;
      opt_update_logname=optarg;		// Use hostname.# if null
      break;
    case (int) OPT_SKIP_NEW:
      opt_specialflag|= SPECIAL_NO_NEW_FUNC;
      break;
    case (int) OPT_SAFE:
      opt_specialflag|= SPECIAL_SAFE_MODE;
      break;
    case (int) OPT_SKIP_PRIOR:
      opt_specialflag|= SPECIAL_NO_PRIOR;
      break;
    case (int) OPT_SKIP_GRANT:
      opt_noacl=1;
      break;
    case (int) OPT_SKIP_LOCK:
      my_disable_locking=1;
      break;
    case (int) OPT_SKIP_HOST_CACHE:
      opt_specialflag|= SPECIAL_NO_HOST_CACHE;
      break;
    case (int) OPT_ENABLE_LOCK:
      my_disable_locking=0;
      break;
    case (int) OPT_USE_LOCKING:
      my_disable_locking=0;
      break;
    case (int) OPT_SKIP_RESOLVE:
      opt_specialflag|=SPECIAL_NO_RESOLVE;
      break;
    case (int) OPT_LONG_FORMAT:
      opt_specialflag|=SPECIAL_LONG_LOG_FORMAT;
      break;
    case (int) OPT_SKIP_NETWORKING:
      opt_disable_networking=1;
      mysql_port=0;
      break;
    case (int) OPT_ONE_THREAD:
      test_flags |= TEST_NO_THREADS;
      break;
    case (int) OPT_BIND_ADDRESS:
      if (optarg && isdigit(optarg[0]))
      {
	my_bind_addr = (ulong) inet_addr(optarg);
      }
      else
      {
	struct hostent *ent;
	if (!optarg || !optarg[0])
	  ent=gethostbyname(optarg);
	else
	{
	  char myhostname[255];
	  if (gethostname(myhostname,sizeof(myhostname)) < 0)
	  {
	    sql_perror("Can't start server: cannot get my own hostname!");
	    exit(1);
	  }
	  ent=gethostbyname(myhostname);
	}
	if (!ent)
	{
	  sql_perror("Can't start server: cannot resolve hostname!");
	  exit(1);
	}
	my_bind_addr = (ulong) ((in_addr*)ent->h_addr_list[0])->s_addr;
      }
      break;
    case (int) OPT_PID_FILE:
      strmov(pidfile_name,optarg);
      break;
    case (int) OPT_STANDALONE:		/* Dummy option for NT */
      break;
#ifdef __WIN32__
    case (int) OPT_CONSOLE:
      opt_console=1;
      break;
    case (int) OPT_FLUSH:
      nisam_flush=1;
      break;
#endif
    case OPT_LOW_PRIORITY_UPDATES:
      thd_startup_options|=OPTION_LOW_PRIORITY_UPDATES;
      low_priority_updates=1;
      break;
    case OPT_BOOTSTRAP:
      opt_noacl=opt_bootstrap=1;
      break;
    default:
      fprintf(stderr,"%s: Unrecognized option: %c\n",my_progname,c);
      usage();
      exit(1);
    }
  }
  // Skipp empty arguments (from shell)
  while (argc != optind && !argv[optind][0])
    optind++;
  if (argc != optind)
  {
    fprintf(stderr,"%s: Too many parameters\n",my_progname);
    usage();
    exit(1);
  }
  fix_paths();
}


#ifdef __NT__

#define KEY_SERVICE_PARAMETERS	"SYSTEM\\CurrentControlSet\\Services\\MySql\\Parameters"

#define COPY_KEY_VALUE(value) if (copy_key_value(hParametersKey,&(value),lpszValue)) return 1
#define CHECK_KEY_TYPE(type,name) if ( type != dwKeyValueType ) { key_type_error(hParametersKey,name); return 1; }
#define SET_CHANGEABLE_VARVAL(varname) if (set_varval(hParametersKey,varname,szKeyValueName,dwKeyValueType,lpdwValue)) return 1;

static void key_type_error(HKEY hParametersKey,const char *szKeyValueName)
{
 TCHAR szErrorMsg[512];
 RegCloseKey( hParametersKey );
 strxmov(szErrorMsg,TEXT("Value \""),
	 szKeyValueName,
	 TEXT("\" of registry key \"" KEY_SERVICE_PARAMETERS "\" has wrong type\n"),NullS);
 fprintf(stderr, szErrorMsg); /* not unicode compatible */
}

static bool copy_key_value(HKEY hParametersKey, char **var, const char *value)
{
  if (!(*var=my_strdup(value,MYF(MY_WME))))
  {
    RegCloseKey(hParametersKey);
    fprintf(stderr, "Couldn't allocate memory for registry key value\n");
    return 1;
  }
  return 0;
}

static bool set_varval(HKEY hParametersKey,const char *var, const char *szKeyValueName, DWORD dwKeyValueType,
		       LPDWORD lpdwValue)
{
  CHECK_KEY_TYPE(dwKeyValueType, szKeyValueName );
  if (set_changeable_varval(var, *lpdwValue, changeable_vars))
  {
    TCHAR szErrorMsg [ 512 ];
    RegCloseKey( hParametersKey );
    strxmov(szErrorMsg,
	    TEXT("Value \""),
	    szKeyValueName,
	    TEXT("\" of registry key \"" KEY_SERVICE_PARAMETERS  "\" is invalid\n"),NullS);
    fprintf( stderr, szErrorMsg ); /* not unicode compatible */
    return 1;
  }
  return 0;
}


static int get_service_parameters()
{
  DWORD dwLastError;
  HKEY hParametersKey;
  DWORD dwIndex;
  TCHAR szKeyValueName [ 256 ];
  DWORD dwKeyValueName;
  DWORD dwKeyValueType;
  BYTE bKeyValueBuffer [ 512 ];
  DWORD dwKeyValueBuffer;
  LPDWORD lpdwValue = (LPDWORD) &bKeyValueBuffer[0];
  LPCTSTR lpszValue = (LPCTSTR) &bKeyValueBuffer[0];

  set_options();

  /* open parameters of service */
  dwLastError = (DWORD) RegOpenKeyEx( HKEY_LOCAL_MACHINE,
				      TEXT(KEY_SERVICE_PARAMETERS), 0,
				      KEY_READ, &hParametersKey );
  if ( dwLastError == ERROR_FILE_NOT_FOUND ) /* no parameters available */
    return 0;
  if ( dwLastError != ERROR_SUCCESS )
  {
    fprintf(stderr,"Can't open registry key \"" KEY_SERVICE_PARAMETERS "\" for reading\n" );
    return 1;
  }

  /* enumerate all values of key */
  dwIndex = 0;
  dwKeyValueName = sizeof( szKeyValueName ) / sizeof( TCHAR );
  dwKeyValueBuffer = sizeof( bKeyValueBuffer );
  while ( (dwLastError = (DWORD) RegEnumValue(hParametersKey, dwIndex,
					      szKeyValueName, &dwKeyValueName,
					      NULL, &dwKeyValueType,
					      &bKeyValueBuffer[0],
					      &dwKeyValueBuffer))
	  != ERROR_NO_MORE_ITEMS )
  {
    /* check if error occured */
    if ( dwLastError != ERROR_SUCCESS )
    {
      RegCloseKey( hParametersKey );
      fprintf( stderr, "Can't enumerate values of registry key \"" KEY_SERVICE_PARAMETERS "\"\n" );
      return 1;
    }
    if ( lstrcmp(szKeyValueName, TEXT("BaseDir")) == 0 )
    {
      CHECK_KEY_TYPE( REG_SZ, szKeyValueName);
      strmov( mysql_home, lpszValue ); /* not unicode compatible */
    }
    else if ( lstrcmp(szKeyValueName, TEXT("BindAddress")) == 0 )
    {
      CHECK_KEY_TYPE( REG_SZ, szKeyValueName);

      my_bind_addr = (ulong) inet_addr( lpszValue );
      if ( my_bind_addr == (ulong) INADDR_NONE )
      {
	struct hostent* ent;

	if ( !(*lpszValue) )
	{
	  char szHostName [ 256 ];
	  if ( gethostname(szHostName, sizeof(szHostName)) == SOCKET_ERROR )
	  {
	    RegCloseKey( hParametersKey );
	    fprintf( stderr, "Can't get my own hostname\n" );
	    return 1;
	  }
	  ent = gethostbyname( szHostName );
	}
	else ent = gethostbyname( lpszValue );
	if ( !ent )
	{
	  RegCloseKey( hParametersKey );
	  fprintf( stderr, "Can't resolve hostname!\n" );
	  return 1;
	}
	my_bind_addr = (ulong) ((in_addr*)ent->h_addr_list[0])->s_addr;
      }
    }
    else if ( lstrcmp(szKeyValueName, TEXT("BigTables")) == 0 )
    {
      CHECK_KEY_TYPE( REG_DWORD, szKeyValueName);
      if ( *lpdwValue )
	thd_startup_options |= OPTION_BIG_TABLES;
      else
	thd_startup_options &= ~((ulong)OPTION_BIG_TABLES);
    }
    else if ( lstrcmp(szKeyValueName, TEXT("DataDir")) == 0 )
    {
      CHECK_KEY_TYPE( REG_SZ, szKeyValueName );
      strmov( mysql_real_data_home, lpszValue ); /* not unicode compatible */
    }
    else if ( lstrcmp(szKeyValueName, TEXT("Locking")) == 0 )
    {
      CHECK_KEY_TYPE( REG_DWORD, szKeyValueName );
      my_disable_locking = !(*lpdwValue);
    }
    else if ( lstrcmp(szKeyValueName, TEXT("LogFile")) == 0 )
    {
      CHECK_KEY_TYPE( REG_SZ, szKeyValueName );
      opt_log = 1;
      COPY_KEY_VALUE( opt_logname );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("UpdateLogFile")) == 0 )
    {
      CHECK_KEY_TYPE( REG_SZ, szKeyValueName );
      opt_update_log = 1;
      COPY_KEY_VALUE( opt_update_logname );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("ISAMLogFile")) == 0 )
    {
      CHECK_KEY_TYPE( REG_SZ, szKeyValueName );
      COPY_KEY_VALUE( nisam_log_filename );
      (void) ni_log( 1 );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("LongLogFormat")) == 0 )
    {
      CHECK_KEY_TYPE( REG_DWORD, szKeyValueName );
      if ( *lpdwValue )
	opt_specialflag |= SPECIAL_LONG_LOG_FORMAT;
      else
	opt_specialflag &= ~((ulong)SPECIAL_LONG_LOG_FORMAT);
    }
    else if ( lstrcmp(szKeyValueName, TEXT("LowPriorityUpdates")) == 0 )
    {
      CHECK_KEY_TYPE( REG_DWORD, szKeyValueName );
      if ( *lpdwValue )
      {
	thd_startup_options |= OPTION_LOW_PRIORITY_UPDATES;
	low_priority_updates = 1;
      }
      else
      {
	thd_startup_options &= ~((ulong)OPTION_LOW_PRIORITY_UPDATES);
	low_priority_updates = 0;
      }
    }
    else if ( lstrcmp(szKeyValueName, TEXT("Port")) == 0 )
    {
      CHECK_KEY_TYPE( REG_DWORD, szKeyValueName );
      mysql_port = (unsigned int) *lpdwValue;
    }
    else if ( lstrcmp(szKeyValueName, TEXT("OldProtocol")) == 0 )
    {
      CHECK_KEY_TYPE( REG_DWORD, szKeyValueName );
      protocol_version = *lpdwValue ? PROTOCOL_VERSION - 1 : PROTOCOL_VERSION;
    }
    else if ( lstrcmp(szKeyValueName, TEXT("HostnameResolving")) == 0 )
    {
      CHECK_KEY_TYPE( REG_DWORD, szKeyValueName );
      if ( !*lpdwValue )
	opt_specialflag |= SPECIAL_NO_RESOLVE;
      else
	opt_specialflag &= ~((ulong)SPECIAL_NO_RESOLVE);
    }
    else if ( lstrcmp(szKeyValueName, TEXT("Networking")) == 0 )
    {
      CHECK_KEY_TYPE( REG_DWORD, szKeyValueName );
      opt_disable_networking = !(*lpdwValue);
    }
    else if ( lstrcmp(szKeyValueName, TEXT("HostnameCaching")) == 0 )
    {
      CHECK_KEY_TYPE( REG_DWORD, szKeyValueName );
      if ( !*lpdwValue )
	opt_specialflag |= SPECIAL_NO_HOST_CACHE;
      else
	opt_specialflag &= ~((ulong)SPECIAL_NO_HOST_CACHE);
    }
    else if ( lstrcmp(szKeyValueName, TEXT("ThreadPriority")) == 0 )
    {
      CHECK_KEY_TYPE( REG_DWORD, szKeyValueName );
      if ( !(*lpdwValue) )
	opt_specialflag |= SPECIAL_NO_PRIOR;
      else
	opt_specialflag &= ~((ulong)SPECIAL_NO_PRIOR);
    }
    else if ( lstrcmp(szKeyValueName, TEXT("NamedPipe")) == 0 )
    {
      CHECK_KEY_TYPE( REG_SZ, szKeyValueName );
      COPY_KEY_VALUE( mysql_unix_port );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("TempDir")) == 0 )
    {
      CHECK_KEY_TYPE( REG_SZ, szKeyValueName );
      COPY_KEY_VALUE( mysql_tmpdir );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("FlushTables")) == 0 )
    {
      CHECK_KEY_TYPE( REG_DWORD, szKeyValueName );
      nisam_flush = *lpdwValue ? 1 : 0;
    }
    else if ( lstrcmp(szKeyValueName, TEXT("BackLog")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "back_log" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("ConnectTimeout")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "connect_timeout" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("JoinBufferSize")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "join_buffer" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("KeyBufferSize")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "key_buffer" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("LongQueryTime")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "long_query_time" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("MaxAllowedPacket")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "max_allowed_packet" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("MaxConnections")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "max_connections" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("MaxConnectErrors")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "max_connect_errors" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("MaxJoinSize")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "max_join_size" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("MaxSortLength")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "max_sort_length" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("NetBufferLength")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "net_buffer_length" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("RecordBufferSize")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "record_buffer" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("SortBufferSize")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "sort_buffer" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("TableCacheSize")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "table_cache" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("TmpTableSize")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "tmp_table_size" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("ThreadStackSize")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "thread_stack" );
    }
    else if ( lstrcmp(szKeyValueName, TEXT("WaitTimeout")) == 0 )
    {
      SET_CHANGEABLE_VARVAL( "wait_timeout" );
    }
    else
    {
      TCHAR szErrorMsg [ 512 ];
      RegCloseKey( hParametersKey );
      lstrcpy( szErrorMsg, TEXT("Value \"") );
      lstrcat( szErrorMsg, szKeyValueName );
      lstrcat( szErrorMsg, TEXT("\" of registry key \"" KEY_SERVICE_PARAMETERS "\" is not defined by MySQL\n") );
      fprintf( stderr, szErrorMsg ); /* not unicode compatible */
      return 1;
    }

    dwIndex++;
    dwKeyValueName = sizeof( szKeyValueName ) / sizeof( TCHAR );
    dwKeyValueBuffer = sizeof( bKeyValueBuffer );
  }
  RegCloseKey( hParametersKey );

  /* paths are fixed by method get_options() */
  return 0;
}
#endif


static char *get_relative_path(char *path)
{
  if (test_if_hard_path(path) && is_prefix(path,DEFAULT_MYSQL_HOME))
  {
    path+=strlen(DEFAULT_MYSQL_HOME);
    while (*path == FN_LIBCHAR)
      path++;
  }
  return path;
}


static void fix_paths(void)
{
  (void) fn_format(mysql_home,mysql_home,"","",16); // Remove symlinks
  convert_dirname(mysql_home);
  convert_dirname(mysql_real_data_home);
  convert_dirname(language);
  (void) my_load_path(mysql_home,mysql_home,""); // Resolve current dir
  (void) my_load_path(mysql_real_data_home,mysql_real_data_home,mysql_home);
  (void) my_load_path(pidfile_name,pidfile_name,mysql_real_data_home);

  char buff[FN_REFLEN],*sharedir=get_relative_path(SHAREDIR);
  if (test_if_hard_path(sharedir))
    strmov(buff,sharedir);			/* purecov: tested */
  else
    strxmov(buff,mysql_home,sharedir,NullS);
  convert_dirname(buff);
  (void) my_load_path(language,language,buff);

  /* Add '/' to TMPDIR if needed */
  char *tmp=my_malloc(FN_REFLEN,MYF(MY_FAE));
  if (tmp)
  {
    strmov(tmp,mysql_tmpdir);
    mysql_tmpdir=tmp;
    convert_dirname(mysql_tmpdir);
    mysql_tmpdir=my_realloc(mysql_tmpdir,strlen(mysql_tmpdir)+1,
			    MYF(MY_HOLD_ON_ERROR));
  }
}


#ifdef SET_RLIMIT_NOFILE
static uint set_maximum_open_files(uint max_file_limit)
{
  struct rlimit rlimit;
  ulong old_cur;

  if (!getrlimit(RLIMIT_NOFILE,&rlimit))
  {
    old_cur=rlimit.rlim_cur;
    if (rlimit.rlim_cur >= max_file_limit)	// Nothing to do
      return rlimit.rlim_cur;			/* purecov: inspected */
    rlimit.rlim_cur=rlimit.rlim_max=max_file_limit;
    if (setrlimit(RLIMIT_NOFILE,&rlimit))
    {
      sql_print_error("Warning: setrlimit couldn't increase number of open files to more than %ld",
	      old_cur);		/* purecov: inspected */
      max_file_limit=old_cur;
    }
    else
    {
      VOID(getrlimit(RLIMIT_NOFILE,&rlimit));
      if ((uint) rlimit.rlim_cur != max_file_limit)
	sql_print_error("Warning: setrlimit returned ok, but didn't change limits. Max open files is %ld",
			  (ulong) rlimit.rlim_cur); /* purecov: inspected */
      max_file_limit=rlimit.rlim_cur;
    }
  }
  return max_file_limit;
}
#endif


/*****************************************************************************
** Instantiate templates
*****************************************************************************/

#ifdef __GNUC__
/* Used templates */
template class I_List<THD>;
#endif
