/* Copyright Abandoned 1997 TCX DataKonsult AB & Monty Program KB & Detron HB
** This file is public domain and comes with NO WARRANTY of any kind */

/*
**	   mysqlimport.c  - Imports all given files
**			    into a table(s).
**
**			   *************************
**			   *			   *
**			   * AUTHOR: Monty & Jani  *
**			   * DATE:   June 24, 1997 *
**			   *			   *
**			   *************************
*/
#define IMPORT_VERSION "1.2"

#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include "mysql.h"
#include "mysql_version.h"
#include <getopt.h>


static void db_error_with_table(MYSQL *mysql, char *table);
static void db_error(MYSQL *mysql);
static char *field_escape(char *to,const char *from,uint length);
static char *add_load_option(char *ptr,const char *object,
			     const char *statement);

static bool	verbose=0,lock_tables=0,ignore_errors=0,delete=0,
		replace=0,silent=0,ignore=0,opt_compress=0,opt_local_file=0;

static MYSQL	mysql;
static char	*password=0, *current_user=0,
		*current_host=0, *current_db=0, *fields_terminated=0,
		*lines_terminated=0, *enclosed=0, *opt_enclosed=0,
		*escaped=0;
static uint     opt_mysql_port=0;
static my_string opt_mysql_unix_port=0;

enum options {OPT_FTB=256, OPT_LTB, OPT_ENC, OPT_O_ENC, OPT_ESC};

static struct option long_options[] =
{
  {"compress",	        no_argument,	        0, 'C'},
  {"debug",		optional_argument,	0, '#'},
  {"delete",		no_argument,		0, 'd'},
  {"fields-terminated-by", required_argument,	  0, (int) OPT_FTB},
  {"fields-enclosed-by", required_argument,	  0, (int) OPT_ENC},
  {"fields-optionally-enclosed-by", required_argument, 0, (int) OPT_O_ENC},
  {"fields-escaped-by",  required_argument,	  0, (int) OPT_ESC},
  {"force",		no_argument,		0, 'f'},
  {"help",		no_argument,		0, '?'},
  {"host",		required_argument,	0, 'h'},
  {"ignore",		no_argument,		0, 'i'},
  {"lines-terminated-by", required_argument,	0, (int) OPT_LTB},
  {"local",		no_argument,		0, 'L'},
  {"lock-tables",	no_argument,		0, 'l'},
  {"password",		optional_argument,	0, 'p'},
#ifdef __WIN32__
  {"pipe",	    	no_argument,	   	0, 'W'},
#endif
  {"port",		required_argument,	0, 'P'},
  {"replace",		no_argument,		0, 'r'},
  {"silent",		no_argument,		0, 's'},
  {"socket",		required_argument,	0, 'S'},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user",		required_argument,	0, 'u'},
#endif
  {"verbose",		no_argument,		0, 'v'},
  {"version",		no_argument,		0, 'V'},
  {0, 0, 0, 0}
};



static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n" ,my_progname,
	  IMPORT_VERSION, MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}



static void usage(void)
{
  print_version();
  puts("Monty & Jani. This software is in public Domain");
  puts("This software comes with ABSOLUTELY NO WARRANTY\n");
  printf("\
Loads tables from text files in various formats.  The base name of the\n\
text file must be the name of the table that should be used.\n\
If one uses sockets to connect to the MySQL server, the server will open and\n\
read the text file directly. In other cases the client will open the text\n\
file. The SQL command 'LOAD DATA INFILE' is used to import the rows.\n");

  printf("\nUsage: %s [OPTIONS] database textfile...",my_progname);
  printf("\n\
  -#, --debug[=...]     Output debug log. Often this is 'd:t:o,filename`\n\
  -?, --help		Displays this help and exits.\n\
  -C, --compress        Use compression in server/client protocol\n\
  -d, --delete          Deletes first all rows from table.\n\
  -f, --force		Continue even if we get an sql-error.\n\
  -h, --host=...	Connect to host.\n\
  -i, --ignore          If duplicate unique key was found, keep old row.\n\
  -l, --lock-tables     Lock all tables for write.\n\
  -L, --local		Read all files through the client\n\
  -p, --password[=...]	Password to use when connecting to server.\n\
			If password is not given it's asked from the tty.\n");
#ifdef __WIN32__
  puts("-W, --pipe              Use named pipes to connect to server");
#endif
  printf("\
  -P, --port=...	Port number to use for connection.\n\
  -r, --replace         If duplicate unique key was found, replace old row.\n\
  -s, --silent		Be more silent.\n\
  -S, --socket=...	Socket file to use for connection.\n");
#ifndef DONT_ALLOW_USER_CHANGE
  printf("\
  -u, --user=#		User for login if not current user.\n");
#endif
  printf("\
  -v, --verbose		Print info about the various stages.\n\
  -V, --version		Output version information and exit.\n\
  --fields-terminated-by=...\n\
                        Fields in the textfile are terminated by ...\n\
  --fields-enclosed-by=...\n\
                        Fields in the importfile are enclosed by ...\n\
  --fields-optionally-enclosed-by=...\n\
                        Fields in the i.file are opt. enclosed by ...\n\
  --fields-escaped-by=...\n\
                        Fields in the i.file are escaped by ...\n\
  --lines-terminated-by=...\n\
                        Lines in the i.file are terminated by ...\n\
");
}

static int get_options(int *argc, char ***argv)
{
  int c, option_index;
  bool tty_password=0;

  while ((c=getopt_long(*argc,*argv,"#::p::h:u:P:S:CdfilLrsvV?IW",long_options,
			&option_index)) != EOF)
  {
    switch(c) {
    case 'C':
      opt_compress=1;
      break;
    case 'd':
      delete= 1;
      break;
    case 'f':
      ignore_errors= 1;
      break;
    case 'h':
      current_host= optarg;
      break;
    case 'i':
      ignore= 1;
      break;
#ifndef DONT_ALLOW_USER_CHANGE
    case 'u':
      current_user= optarg;
      break;
#endif
    case 'p':
      if (optarg)
      {
	my_free(password,MYF(MY_ALLOW_ZERO_PTR));
	password= my_strdup(optarg,MYF(MY_FAE));
	while (*optarg) *optarg++= 'x';		/* Destroy argument */
      }
      else
	tty_password= 1;
      break;
    case 'P':
      opt_mysql_port= (unsigned int) atoi(optarg);
      break;
    case 'r':
      replace= 1;
      break;
    case 's':
      silent= 1;
      break;
    case 'S':
      opt_mysql_unix_port= optarg;
      break;
#ifdef __WIN32__
    case 'W':
      opt_mysql_unix_port=MYSQL_NAMEDPIPE;
      opt_local_file=1;
      break;
#endif
    case '#':
      DBUG_PUSH(optarg ? optarg : "d:t:o");
      break;
    case 'l': lock_tables= 1;   break;
    case 'L': opt_local_file=1; break;
    case 'v': verbose= 1;	break;
    case 'V': print_version(); exit(0);
    case 'I':
    case '?':
      usage();
      exit(0);
    case (int) OPT_FTB:
      fields_terminated= optarg;
      break;
    case (int) OPT_LTB:
      lines_terminated= optarg;
      break;
    case (int) OPT_ENC:
      enclosed= optarg;
      break;
    case (int) OPT_O_ENC:
      opt_enclosed= optarg;
      break;
    case (int) OPT_ESC:
      escaped= optarg;
      break;
    }
  }
  if (enclosed && opt_enclosed)
  {
    fprintf(stderr, "You can't use ..enclosed.. and ..optionally-enclosed.. at the same time.\n");
    return(1);
  }
  if (replace && ignore)
  {
    fprintf(stderr, "You can't use --ignore (-i) and --replace (-r) at the same time.\n");
    return(1);
  }
  (*argc)-=optind;
  (*argv)+=optind;
  if (*argc < 2)
  {
    usage();
    return 1;
  }
  current_db= *((*argv)++);
  (*argc)--;
  if (tty_password)
    password=get_tty_password(NullS);
  return(0);
}



static int write_to_table(char *filename, MYSQL *sock)
{
  char tablename[FN_REFLEN], hard_path[FN_REFLEN],
       sql_statement[FN_REFLEN*2+256], *end;
  my_bool local_file;
  DBUG_ENTER("write_to_table");
  DBUG_PRINT("enter",("filename: %s",filename));

  local_file= sock->unix_socket == 0 || opt_local_file;

  fn_format(tablename, filename, "", "", 1 | 2); /* removes path & ext. */
  if (local_file)
    strmov(hard_path,filename);
  else
    my_load_path(hard_path, filename, NULL); /* filename includes the path */

  if (delete)
  {
    if (verbose)
      fprintf(stdout, "Deleting the old data from table %s\n", tablename);
    sprintf(sql_statement, "DELETE FROM %s", tablename);
    if (mysql_query(sock, sql_statement))
    {
      db_error_with_table(sock, tablename);
      DBUG_RETURN(1);
    }
  }
  to_unix_path(hard_path);
  if (verbose)
  {
    if (local_file)
      fprintf(stdout, "Loading data from LOCAL file: %s into %s\n",
	      hard_path, tablename);
    else
      fprintf(stdout, "Loading data from SERVER file: %s into %s\n",
	      hard_path, tablename);
  }
  sprintf(sql_statement, "LOAD DATA %s INFILE '%s'",
	  local_file ? "LOCAL" : "", hard_path);
  end= strend(sql_statement);
  if (replace)
    end= strmov(end, " REPLACE");
  if (ignore)
    end= strmov(end, " IGNORE");
  end= strmov(strmov(end, " INTO TABLE "), tablename);
  
  if (fields_terminated || enclosed || opt_enclosed || escaped)
      end= strmov(end, " FIELDS");
  end= add_load_option(end, fields_terminated, " TERMINATED BY");
  end= add_load_option(end, enclosed, " ENCLOSED BY");
  end= add_load_option(end, opt_enclosed,
		       " OPTIONALLY ENCLOSED BY");
  end= add_load_option(end, escaped, " ESCAPED BY");
  end= add_load_option(end, lines_terminated, " LINES TERMINATED BY");
  *end= '\0';

  if (mysql_query(sock, sql_statement))
  {
    db_error_with_table(sock, tablename);
    DBUG_RETURN(1);
  }
  if (!silent)
  {
    if (mysql_info(sock)) /* If NULL-pointer, print nothing */
    {
      fprintf(stdout, "%s.%s: %s\n", current_db, tablename,
	      mysql_info(sock));
    }
  }
  DBUG_RETURN(0);
}



void lock_table(MYSQL *sock, int tablecount, char **raw_tablename)
{
  DYNAMIC_STRING query;
  int i;
  char tablename[FN_REFLEN];

  if (verbose)
    fprintf(stdout, "Locking tables for read\n");
  init_dynamic_string(&query, "LOCK TABLES ", 256, 1024);
  for (i=0 ; i < tablecount ; i++)
  {
    fn_format(tablename, raw_tablename[i], "", "", 1 | 2);
    dynstr_append(&query, tablename);
    dynstr_append(&query, " WRITE,");
  }
  if (mysql_real_query(sock, query.str, query.length-1))
    db_error(sock); /* We shall countinue here, if --force was given */
}




MYSQL *db_connect(char *host, char *database, char *user, char *passwd)
{
  MYSQL *sock;
  if (verbose)
    fprintf(stdout, "Connecting to %s\n", host ? host : "localhost");
  mysql_init(&mysql);
  if (opt_compress)
    mysql_options(&mysql,MYSQL_OPT_COMPRESS,NullS);
  if (!(sock= mysql_real_connect(&mysql,host,user,passwd,
				 database,opt_mysql_port,opt_mysql_unix_port,
				 0)))
  {
    ignore_errors=0;	  /* NO RETURN FROM db_error */
    db_error(&mysql);
  }
  if (verbose)
    fprintf(stdout, "Selecting database %s\n", database);
  if (mysql_select_db(sock, database))
  {
    ignore_errors=0;
    db_error(&mysql);
  }
  return sock;
}



void db_disconnect(char *host, MYSQL *sock)
{
  if (verbose)
    fprintf(stdout, "Disconnecting from %s\n", host ? host : "localhost");
  mysql_close(sock);
}



static void safe_exit(int error, MYSQL *sock)
{
  if (ignore_errors)
    return;
  if (sock)
    mysql_close(sock);
  exit(error);
}



static void db_error_with_table(MYSQL *mysql, char *table)
{
  my_printf_error(0,"Error: %s, when using table: %s",
		  MYF(0), mysql_error(mysql), table);
  safe_exit(1, mysql);
}



static void db_error(MYSQL *mysql)
{
  my_printf_error(0,"Error: %s", MYF(0), mysql_error(mysql));
  safe_exit(1, mysql);
}

static my_string load_default_groups[]= { "mysqlimport","client",0 };


static char *add_load_option(char *ptr,const char *object,const char *statement)
{
  if (object)
  {
    ptr= strxmov(ptr," ",statement," '",NullS);
    ptr= field_escape(ptr,object,strlen(object));
    *ptr++= '\'';
  }
  return ptr;
}

/*
** Allow the user to specify field terminator strings like:
** "'", "\", "\\" (escaped backslash), "\t" (tab), "\n" (newline)
** This is done by doubleing ' and add a end -\ if needed to avoid
** syntax errors from the SQL parser.
*/ 
 
static char *field_escape(char *to,const char *from,uint length)
{
  const char *end;
  uint end_backslashes=0; 

  for (end= from+length; from != end; from++)
  {
    *to++= *from;
    if (*from == '\\')
      end_backslashes^=1;    /* find odd number of backslashes */
    else 
    {
      if (*from == '\'' && !end_backslashes)
	*to++= *from;      /* We want a dublicate of "'" for MySQL */
      end_backslashes=0;
    }
  }
  /* Add missing backslashes if user has specified odd number of backs.*/
  if (end_backslashes)
    *to++= '\\';          
  return to;
}
  


int main(int argc, char **argv)
{
  int exitcode=0, error=0;
  MYSQL *sock=0;
  MY_INIT(argv[0]);

  load_defaults("my",load_default_groups,&argc,&argv);
  if (get_options(&argc, &argv))
    return(1);
  if (!(sock= db_connect(current_host,current_db,current_user,password)))
    return(1); /* purecov: deadcode */
  if (lock_tables)
    lock_table(sock, argc, argv);
  for (; *argv != NULL; argv++)
    if ((error=write_to_table(*argv, sock)))
      if (exitcode == 0)
	exitcode = error;
  db_disconnect(current_host, sock);
  my_end(0);
  return(exitcode);
}
