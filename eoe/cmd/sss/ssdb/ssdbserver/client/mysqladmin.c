/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* maintaince of mysql databases */


#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include <signal.h>
#include "mysql.h"
#include "mysql_version.h"
#include "errmsg.h"
#include "my_sys.h"
#include <getopt.h>
#ifdef THREAD
#include <my_pthread.h>				/* because of signal()	*/
#endif

#define ADMIN_VERSION "7.8"

static int interval=0;
static bool option_quick=0,interrupted=0,new_line=0,option_silent=0,
  opt_compress=0;
static uint tcp_port=0,option_wait=0;
static my_string unix_port=0;

static void print_version(void);
static void usage(void);
static bool execute_commands(MYSQL *mysql,int argc, char **argv);
static sig_handler endprog(int signal_number);
static int create_db(MYSQL *sock,char *db);
static int drop_db(MYSQL *sock,char *db);
static void nice_time(ulong sec,char *buff);
static void print_header(MYSQL_RES *result);
static void print_top(MYSQL_RES *result);
static void print_row(MYSQL_RES *result,MYSQL_ROW cur);

/* The order of commands must be the same as command_names, except ADMIN_ERROR*/
enum commands { ADMIN_ERROR, ADMIN_CREATE, ADMIN_DROP, ADMIN_SHUTDOWN,
		ADMIN_RELOAD, ADMIN_REFRESH, ADMIN_VER, ADMIN_PROCESSLIST,
		ADMIN_STATUS, ADMIN_KILL, ADMIN_DEBUG, ADMIN_VARIABLES,
		ADMIN_FLUSH_LOGS, ADMIN_FLUSH_HOSTS, ADMIN_FLUSH_TABLES,
		ADMIN_PASSWORD, ADMIN_PING, ADMIN_EXTENDED_STATUS, 
		ADMIN_FLUSH_STATUS, ADMIN_FLUSH_PRIVILEGES,
};
my_string command_names[]={"create","drop","shutdown","reload","refresh",
			   "version", "processlist","status","kill","debug",
			   "variables","flush-logs","flush-hosts",
			   "flush-tables","password","ping",
			   "extended-status","flush-status",
			   "flush-privileges", NullS};
TYPELIB commands={array_elements(command_names)-1,"commands",command_names};

static struct option long_options[] =
{
  
  {"compress",	no_argument,       0, 'C'},
  {"debug",	optional_argument, 0, '#'},
  {"force",	no_argument,	   0, 'f'},
  {"help",	no_argument,	   0, '?'},
  {"host",	required_argument, 0, 'h'},
  {"password",	optional_argument, 0, 'p'},
#ifdef __WIN32__
  {"pipe",	 no_argument,	   0, 'W'},
#endif
  {"port",	required_argument, 0, 'P'},
  {"silent",	no_argument,	   0, 's'},
  {"socket",	required_argument, 0, 'S'},
  {"sleep",	required_argument, 0, 'i'},
  {"timeout",	required_argument, 0, 't'},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user",	required_argument, 0, 'u'},
#endif
  {"version",	no_argument,	   0, 'V'},
  {"wait",      optional_argument, 0, 'w'},
  {0, 0, 0, 0}
};

static my_string load_default_groups[]= { "mysqladmin","client",0 };

int main(int argc,char *argv[])
{
  int	c, connect_tries, error = 0,option_index=0;
  MYSQL mysql;
  char	*host = NULL,*password=0,*user=0;
  bool tty_password=0;

  if(getuid() || geteuid())
  { fprintf(stderr,"Access denied\n");
    exit(1);
  }

  MY_INIT(argv[0]);
  mysql_init(&mysql);
  load_defaults("my",load_default_groups,&argc,&argv);

  while ((c=getopt_long(argc,argv,"h:i:p::u:#::P:sS:Ct:fq?Vw::W",long_options,
			&option_index)) != EOF)
  {
    switch(c) {
    case 'C':
      opt_compress=1;
      break;
    case 'h':
      host = optarg;
      break;
    case 'q':					/* Allow old 'q' option */
    case 'f':
      option_quick++;
      break;
    case 'p':
      if (optarg)
      {
	my_free(password,MYF(MY_ALLOW_ZERO_PTR));
	password=my_strdup(optarg,MYF(MY_FAE));
	while (*optarg) *optarg++= 'x';		/* Destroy argument */
      }
      else
	tty_password=1;
      break;
#ifndef DONT_ALLOW_USER_CHANGE
    case 'u':
      user=optarg;
      break;
#endif
    case 'i':
      interval=atoi(optarg);
      break;
    case 'P':
      tcp_port= (unsigned int) atoi(optarg);
      break;
    case 's':
      option_silent=1;
      break;
    case 'S':
      unix_port= optarg;
      break;
    case 'W':
#ifdef __WIN32__
      unix_port=MYSQL_NAMEDPIPE;
#endif
      break;
    case 't':
      {
	uint tmp=atoi(optarg);
	mysql_options(&mysql,MYSQL_OPT_CONNECT_TIMEOUT, (char*) &tmp);
	break;
      }
    case '#':
      DBUG_PUSH(optarg ? optarg : "d:t:o,/tmp/mysqladmin.trace");
      break;
    case 'V':
      print_version();
      exit(0);
      break;
    case 'w':
      if (optarg)
      {
	if ((option_wait=atoi(optarg)) <= 0)
	  option_wait=1;
      }
      else
	option_wait= ~0;
      break;
    default:
      fprintf(stderr,"Illegal option character '%c'\n",opterr);
      /* Fall throught */
    case '?':
    case 'I':					/* Info */
      error++;
      break;
    }
  }
  argc-=optind;
  argv+=optind;
  if (error || argc == 0)
  {
    usage();
    exit(1);
  }
  if (tty_password)
    password=get_tty_password(NullS);

  VOID(signal(SIGINT,endprog));			/* Here if abort */
  VOID(signal(SIGTERM,endprog));		/* Here if abort */

  mysql_init(&mysql);
  if (opt_compress)
    mysql_options(&mysql,MYSQL_OPT_COMPRESS,NullS);

  for (connect_tries=0; connect_tries < 2;)
  {
    if (!mysql_real_connect(&mysql,host,user,password,NullS,tcp_port,unix_port,
			    0))
    {
      if (!option_wait)
      {
	error=1;
	connect_tries=2;
	if (!option_silent)
	{
	  if (!host)
	    host=LOCAL_HOST;
	  my_printf_error(0,"connect to server at '%s' failed\nerror: '%s'",
			  MYF(ME_BELL), host, mysql_error(&mysql));
	  if (mysql_errno(&mysql) == CR_CONNECTION_ERROR)
	  {
	    fprintf(stderr,
		    "Check that mysqld is running and that the socket: '%s' exists!\n",
		    unix_port ? unix_port : mysql_unix_port);
	  }
	  else if (mysql_errno(&mysql) == CR_CONN_HOST_ERROR ||
		   mysql_errno(&mysql) == CR_UNKNOWN_HOST)
	  {
	    fprintf(stderr,"Check that mysqld is running on %s",host);
	    fprintf(stderr," and that the port is %d.\n",
		    tcp_port ? tcp_port: mysql_port);
	    fprintf(stderr,"You can check this by doing 'telnet %s %d'\n",
		    host, tcp_port ? tcp_port: mysql_port);
	  }
	}
      }
      else
      {
	option_wait--;				/* One less retry */
	if ((mysql_errno(&mysql) != CR_CONN_HOST_ERROR) &&
	    (mysql_errno(&mysql) != CR_CONNECTION_ERROR))
	{	 
	  fprintf(stderr,"Got error: %s\n", mysql_error(&mysql));
	  if (!option_quick)
	    exit(error);
	  sleep(5);
	}
	else if (!connect_tries)
	{
	  connect_tries=1;
	  fputs("Waiting",stderr);
	  (void) fflush(stderr);
	  sleep(5);
	}
	else
	{
	  putc('.',stderr); 
	  (void) fflush(stderr);
	  sleep(5);
	}
      }
    }
    else
    {
      connect_tries=2;
      error=0;
      while (!interrupted)
      {
	new_line=0;
	if (execute_commands(&mysql,argc,argv) && !option_quick)
	{
	  error=1;
	  break;
	}
	if (interval)
	{
	  sleep(interval);
	  if (new_line)
	    puts("");
	}
	else
	  break;
      }
      mysql_close(&mysql);
    }
  }
  my_free(password,MYF(MY_ALLOW_ZERO_PTR));
  my_end(0);
  exit(error);
  return 0;
}

static sig_handler endprog(int signal_number __attribute__((unused)))
{
  interrupted=1;
}


static bool execute_commands(MYSQL *mysql,int argc, char **argv)
{
  char *stat;

  for (; argc > 0 ; argv++,argc--)
  {
    switch (find_type(argv[0],&commands,2)) {
    case ADMIN_CREATE:
      if (argc < 2)
      {
	my_printf_error(0,"Too few arguments to create",MYF(ME_BELL));
	return 1;
      }
      else if (create_db(mysql,argv[1]))
	return 1;
      else
      {
	argc--; argv++;
      }
      break;
    case ADMIN_DROP:
      if (argc < 2)
      {
	my_printf_error(0,"Too few arguments to drop",MYF(ME_BELL));
	return 1;
      }
      else if (drop_db(mysql,argv[1]) > 0)
	return 1;
      else
      {
	argc--; argv++;
      }
      break;
    case ADMIN_SHUTDOWN:
      if (mysql_shutdown(mysql))
      {
	my_printf_error(0,"shutdown failed; error: '%s'",MYF(ME_BELL),
			mysql_error(mysql));
	return 1;
      }
      mysql_close(mysql); /* Close connection to avoid error messages */
      break;
    case ADMIN_FLUSH_PRIVILEGES:
    case ADMIN_RELOAD:
      if (mysql_refresh(mysql,REFRESH_GRANT) < 0)
      {
	my_printf_error(0,"reload failed; error: '%s'",MYF(ME_BELL),
			mysql_error(mysql));
	return 1;
      }
      break;
    case ADMIN_REFRESH:
      if (mysql_refresh(mysql,(uint) ~(REFRESH_GRANT | REFRESH_STATUS)) < 0)
      {
	my_printf_error(0,"refresh failed; error: '%s'",MYF(ME_BELL),
			mysql_error(mysql));
	return 1;
      }
      break;
    case ADMIN_VER:
      new_line=1;
      print_version();
      puts("TCX Datakonsult AB, by Monty\n");
      printf("Server version\t\t%s\n", mysql_get_server_info(mysql));
      printf("Protocol version\t%d\n", mysql_get_proto_info(mysql));
      printf("Connection\t\t%s\n",mysql_get_host_info(mysql));
      if (mysql->unix_socket)
	printf("UNIX socket\t\t%s\n", mysql->unix_socket);
      else
	printf("TCP port\t\t%d\n", mysql->port);
      stat=mysql_stat(mysql);
      {
	char *pos,buff[40];
	ulong sec;
	pos=strchr(stat,' ');
	*pos++=0;
	printf("%s\t\t\t",stat);			/* print label */
	if ((stat=str2int(pos,10,0,LONG_MAX,(long*) &sec)))
	{
	  nice_time(sec,buff);
	  puts(buff);				/* print nice time */
	  while (*stat == ' ') stat++;		/* to next info */
	}
      }
      putc('\n',stdout);
      if (stat)
	puts(stat);
      break;
    case ADMIN_PROCESSLIST:
    {
      MYSQL_RES *result;
      MYSQL_ROW row;

      if (!(result=mysql_list_processes(mysql)))
      {
	my_printf_error(0,"process list failed; error: '%s'",MYF(ME_BELL),
			mysql_error(mysql));
	return 1;
      }
      print_header(result);
      while ((row=mysql_fetch_row(result)))
	print_row(result,row);
      print_top(result);
      mysql_free_result(result);
      new_line=1;
      break;
    }
    case ADMIN_STATUS:
      stat=mysql_stat(mysql);
      if (stat)
	puts(stat);
      break;
    case ADMIN_KILL:
      {
	uint error=0;
	char *pos;
	if (argc < 2)
	{
	  my_printf_error(0,"Too few arguments to 'kill'",MYF(ME_BELL));
	  return 1;
	}
	pos=argv[1];
	for (;;)
	{
	  if (mysql_kill(mysql,(ulong) atol(pos)))
	  {
	    my_printf_error(0,"kill failed on %d; error: '%s'",MYF(ME_BELL),
			    atol(pos), mysql_error(mysql));
	    error=1;
	  }
	  if (!(pos=strchr(pos,',')))
	    break;
	  pos++;
	}
	argc--; argv++;
	if (error)
	  return error;
	break;
      }
    case ADMIN_DEBUG:
      if (mysql_dump_debug_info(mysql))
      {
	my_printf_error(0,"debug failed; error: '%s'",MYF(ME_BELL),
			mysql_error(mysql));
	return 1;
      }
      break;
    case ADMIN_VARIABLES:
    {
      MYSQL_RES *res;
      MYSQL_ROW row;

      new_line=1;
      if (mysql_query(mysql,"show variables") ||
	  !(res=mysql_store_result(mysql)))
      {
	my_printf_error(0,"unable to show variables; error: '%s'",MYF(ME_BELL),
			mysql_error(mysql));
	return 1;
      }
      print_header(res);
      while ((row=mysql_fetch_row(res)))
	print_row(res,row);
      print_top(res);
      mysql_free_result(res);
      break;
    }
    case ADMIN_EXTENDED_STATUS:
    {
      MYSQL_RES *res;
      MYSQL_ROW row;

      new_line=1;
      if (mysql_query(mysql,"show status") ||
	  !(res=mysql_store_result(mysql)))
      {
	my_printf_error(0,"unable to show status; error: '%s'",MYF(ME_BELL),
			mysql_error(mysql));
	return 1;
      }
      print_header(res);
      while ((row=mysql_fetch_row(res)))
	print_row(res,row);
      print_top(res);
      mysql_free_result(res);
      break;
    }
    case ADMIN_FLUSH_LOGS:
    {
      if (mysql_refresh(mysql,REFRESH_LOG))
      {
	my_printf_error(0,"refresh failed; error: '%s'",MYF(ME_BELL),
			mysql_error(mysql));
	return 1;
      }
      break;
    }
    case ADMIN_FLUSH_HOSTS:
    {
      if (mysql_refresh(mysql,REFRESH_HOSTS))
      {
	my_printf_error(0,"refresh failed; error: '%s'",MYF(ME_BELL),
			mysql_error(mysql));
	return 1;
      }
      break;
    }
    case ADMIN_FLUSH_TABLES:
    {
      if (mysql_refresh(mysql,REFRESH_TABLES))
      {
	my_printf_error(0,"refresh failed; error: '%s'",MYF(ME_BELL),
			mysql_error(mysql));
	return 1;
      }
      break;
    }
    case ADMIN_FLUSH_STATUS:
    {
      if (mysql_refresh(mysql,REFRESH_STATUS))
      {
	my_printf_error(0,"refresh failed; error: '%s'",MYF(ME_BELL),
			mysql_error(mysql));
	return 1;
      }
      break;
    }
    case ADMIN_PASSWORD:
    {
      char buff[128],crypted_pw[33];

      if(argc < 2)
      {
	my_printf_error(0,"Too few arguments to change password",MYF(ME_BELL));
	return 1;
      }
      if (argv[1][0])
	make_scrambled_password(crypted_pw,argv[1]);
      else
	crypted_pw[0]=0;			/* No password */
      sprintf(buff,"set OPTION password='%s',sql_log_off=0",crypted_pw);

      if (mysql_query(mysql,"set OPTION sql_log_off=1"))
      {
	my_printf_error(0, "Can't turn off logging; error: '%s'",
			MYF(ME_BELL),mysql_error(mysql));
	return 1;
      }
      if (mysql_query(mysql,buff))
      {
	my_printf_error(0,"unable to change password; error: '%s'",
			MYF(ME_BELL),mysql_error(mysql));
	return 1;
      }
      argc--; argv++;
      break;
    }
    case ADMIN_PING:
      mysql->reconnect=0;	/* We want to know of reconnects */
      if (!mysql_ping(mysql))
        puts("mysqld is alive");
      else
      {
	if (mysql_errno(mysql) == CR_SERVER_GONE_ERROR)
	{
	  mysql->reconnect=1;
	  if (!mysql_ping(mysql))
	    puts("connection was down, but mysqld is now alive");
	}
	else
	  my_printf_error(0,"mysqld doesn't answer to ping, error: '%s'",
			  MYF(ME_BELL),mysql_error(mysql));
	return 1;
      }
      mysql->reconnect=1;	/* Automatic reconnect is default */
      break;
    default:
      my_printf_error(0,"Unknown command: '%s'",MYF(ME_BELL),argv[0]);
      return 1;
    }
  }
  return 0;
}

static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s on %s\n",my_progname,ADMIN_VERSION,
	 MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}

static void usage(void)
{
  print_version();
  puts("TCX Datakonsult AB, by Monty");
  puts("This software comes with NO WARRANTY: see the file PUBLIC for details.\n");
  puts("Administer program for the mysqld demon");
  printf("Usage: %s [OPTIONS] command command....\n", my_progname);
  printf("\n\
  -#, --debug=...       Output debug log. Often this is 'd:t:o,filename`.\n\
  -f, --force		Don't ask for confirmation on drop table. Continue.\n\
			even if we get an error.\n\
  -?, --help		Display this help and exit.\n\
  -C, --compress        Use compression in server/client protocol.\n\
  -h, --host=#		Connect to host.\n\
  -p, --password[=...]	Password to use when connecting to server.\n\
			If password is not given it's asked from the tty.\n");
#ifdef __WIN32__
  puts("-W, --pipe		Use named pipes to connect to server");
#endif
  printf("\
  -P  --port=...	Port number to use for connection.\n\
  -i, --sleep=sec	Execute commands again and again with a sleep between\n\
  -s, --silent		Silently exit if one can't connect to server.\n\
  -S, --socket=...	Socket file to use for connection.\n\
  -t, --timeout=...	Timeout for connection.\n");
#ifndef DONT_ALLOW_USER_CHANGE
  printf("\
  -u, --user=#		User for login if not current user.\n");
#endif
  printf("\
  -V, --version		Output version information and exit.\n\
  -w, --wait[=retries]  Wait and retry if connection is down.\n");

  puts("\nWhere command is a one or more of: (Commands may be shortened)\n\
  create databasename	Create a new database.\n\
  drop databasename	Delete a database and all its tables.\n\
  extended-status       Gives an extended status message from the server.\n\
  flush-hosts           Flush all cached hosts.\n\
  flush-logs            Flush all logs.\n\
  flush-tables          Flush all tables.\n\
  flush-privileges      Reload grant tables (same as reload)\n\
  kill id,id,...	Kill mysql threads.");
#if MYSQL_VERSION_ID >= 32200
  puts("\
  password new-password Change old password to new-password");
#endif
  puts("\
  ping			Check if mysqld is alive\n\
  processlist		Show list of active threads in server\n\
  reload		Reload grant tables\n\
  refresh		Flush all tables and close and open logfiles\n\
  shutdown		Take server down\n\
  status		Gives a short status message from the server\n\
  variables             Prints variables available\n\
  version		Get version info from server");
}


static int create_db(MYSQL *mysql,char *db)
{
  if (mysql_create_db(mysql,db))
  {
    my_printf_error(0,"create of '%s' failed\nerror: '%s'",MYF(ME_BELL),
		    db,mysql_error(mysql));
    return 1;
  }
  printf("Database \"%s\" created.\n",db);
  return 0;
}


static int drop_db(MYSQL *mysql,char *db)
{
  char	buf[10];
  if (!option_quick)
  {
    puts("Dropping the database is potentially a very bad thing to do.");
    puts("Any data stored in the database will be destroyed.\n");
    printf("Do you really want to drop the '%s' database [y/N]\n",db);
    VOID(fgets(buf,sizeof(buf)-1,stdin));
    if ((*buf != 'y') && (*buf != 'Y'))
    {
      puts("\nOK, aborting database drop!");
      return -1;
    }
  }
  if (mysql_drop_db(mysql,db))
  {
    my_printf_error(0,"drop of '%s' failed;\nerror: '%s'",MYF(ME_BELL),
		    db,mysql_error(mysql));
    return 1;
  }
  printf("Database \"%s\" dropped\n",db);
  return 0;
}


static void nice_time(ulong sec,char *buff)
{
  ulong tmp;

  if (sec >= 3600L*24)
  {
    tmp=sec/(3600L*24);
    sec-=3600L*24*tmp;
    buff=int2str(tmp,buff,10);
    buff=strmov(buff,tmp > 1 ? " days " : " day ");
  }
  if (sec >= 3600L)
  {
    tmp=sec/3600L;
    sec-=3600L*tmp;
    buff=int2str(tmp,buff,10);
    buff=strmov(buff,tmp > 1 ? " hours " : " hour ");
  }
  if (sec >= 60)
  {
    tmp=sec/60;
    sec-=60*tmp;
    buff=int2str(tmp,buff,10);
    buff=strmov(buff," min ");
  }
  strmov(int2str(sec,buff,10)," sec");
}

static void print_header(MYSQL_RES *result)
{
  MYSQL_FIELD *field;

  print_top(result);
  mysql_field_seek(result,0);
  putchar('|');
  while ((field = mysql_fetch_field(result)))
  {
    printf(" %-*s|",field->max_length+1,field->name);
  }
  putchar('\n');
  print_top(result);
}


static void print_top(MYSQL_RES *result)
{
  uint i,length;
  MYSQL_FIELD *field;

  putchar('+');
  mysql_field_seek(result,0);
  while((field = mysql_fetch_field(result)))
  {
    if ((length=strlen(field->name)) > field->max_length)
      field->max_length=length;
    else
      length=field->max_length;
    for (i=length+2 ; i--> 0 ; )
      putchar('-');
    putchar('+');
  }
  putchar('\n');
}


static void print_row(MYSQL_RES *result,MYSQL_ROW cur)
{
  uint i,length;
  MYSQL_FIELD *field;
  putchar('|');
  mysql_field_seek(result,0);
  for (i=0 ; i < mysql_num_fields(result); i++)
  {
    field = mysql_fetch_field(result);
    length=field->max_length;
    printf(" %-*s|",length+1,cur[i] ? (char*) cur[i] : "");
  }
  putchar('\n');
}
