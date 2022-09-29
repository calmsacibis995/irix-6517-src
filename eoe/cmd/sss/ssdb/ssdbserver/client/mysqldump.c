/*	mysqldump.c  - Dump a tables contents and format to an ASCII file
**
**
** This software is provided "as is" without any expressed or implied warranty.
**
** The author's original notes follow :-
**
**		******************************************************
**		*						     *
**		* AUTHOR: Igor Romanenko (igor@frog.kiev.ua)	     *
**		* DATE:   December 3, 1994			     *
**		* WARRANTY: None, expressed, impressed, implied      *
**		*	    or other				     *
**		* STATUS: Public domain				     *
**		* Adapted and optimized for MySQL by		     *
**		* Michael Widenius, Sinisa Milivojevic		     *
**		* -w --where added 9/10/98 by Jim Faucette	     *
**		*						     *
**		******************************************************
*/

#define DUMP_VERSION "5.10"

#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include "mysql.h"
#include "mysql_version.h"
#include <getopt.h>

/* Exit codes */

#define EX_USAGE 1
#define EX_MYSQLERR 2
#define EX_CONSCHECK 3
#define EX_EOM 4

/* index into 'show fields from table' */

#define SHOW_FIELDNAME  0
#define SHOW_TYPE  1
#define SHOW_NULL  2
#define SHOW_DEFAULT  4
#define SHOW_EXTRA  5

static char *add_load_option(char *ptr, const char *object,
			     const char *statement);

static char *field_escape(char *to,const char *from,uint length);
static my_bool  verbose=0,tFlag=0,cFlag=0,dFlag=0,quick=0, extended_insert = 0,
		lock_tables=0,ignore_errors=0,flush_logs=0,replace=0,
		ignore=0,opt_drop=0,opt_keywords=0,opt_lock=0,opt_compress=0;
static MYSQL  mysql,*sock=0;
static char  insert_pat[12 * 1024],*password=0,*current_user=0,
                *current_host=0,*current_db=0,*path=0,*fields_terminated=0,
                *lines_terminated=0, *enclosed=0, *opt_enclosed=0, *escaped=0,
		*where=0;
static uint     opt_mysql_port=0;
static my_string opt_mysql_unix_port=0;
static int   first_error=0;
extern ulong net_buffer_length;
static DYNAMIC_STRING extended_row;

enum options {OPT_FTB=256, OPT_LTB, OPT_ENC, OPT_O_ENC, OPT_ESC, OPT_KEYWORDS,
	      OPT_LOCKS, OPT_DROP, OPT_OPTIMIZE };

static struct option long_options[] =
{
  {"add-drop-table",	no_argument,    0, OPT_DROP},
  {"add-locks",    	no_argument,    0, OPT_LOCKS},
  {"allow-keywords",	no_argument,    0, OPT_KEYWORDS},
  {"complete-insert",	no_argument,    0, 'c'},
  {"compress",          no_argument,          0, 'C'},
  {"debug",		optional_argument,  0, '#'},
  {"extended_insert-insert",    no_argument,    0, 'e'},
  {"fields-terminated-by", required_argument,    0, (int) OPT_FTB},
  {"fields-enclosed-by", required_argument,    0, (int) OPT_ENC},
  {"fields-optionally-enclosed-by", required_argument, 0, (int) OPT_O_ENC},
  {"fields-escaped-by", required_argument,    0, (int) OPT_ESC},
  {"flush-logs",	no_argument,    0, 'F'},
  {"force",    		no_argument,    0, 'f'},
  {"help",   		no_argument,    0, '?'},
  {"host",    		required_argument,  0, 'h'},
  {"lines-terminated-by", required_argument,    0, (int) OPT_LTB},
  {"lock-tables",  	no_argument,    0, 'l'},
  {"no-create-info", 	no_argument,    0, 't'},
  {"no-data",  		no_argument,    0, 'd'},
  {"opt",   		no_argument,    0, OPT_OPTIMIZE},
  {"password",  	optional_argument,  0, 'p'},
#ifdef __WIN32__
  {"pipe",		no_argument,	   0, 'W'},
#endif
  {"port",    		required_argument,  0, 'P'},
  {"quick",    		no_argument,    0, 'q'},
  {"set-variable",	required_argument,  0, 'O'},
  {"socket",   		required_argument,  0, 'S'},
  {"tab",    		required_argument,  0, 'T'},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user",    		required_argument,  0, 'u'},
#endif
  {"verbose",    	no_argument,    0, 'v'},
  {"version",    	no_argument,    0, 'V'},
  {"where",		required_argument, 0, 'w'},
  {0, 0, 0, 0}
};


CHANGEABLE_VAR changeable_vars[] = {
  { "max_allowed_packet", (long*) &max_allowed_packet,24*1024*1024,4096,
    24*1024L*1024L,MALLOC_OVERHEAD,1024},
  { "net_buffer_length", (long*) &net_buffer_length,1024*1024L-100,4096,
    24*1024L*1024L,MALLOC_OVERHEAD,1024},
  { 0, 0, 0, 0, 0, 0, 0}
};

static void safe_exit(int error);
static void write_heder(FILE *sql_file);


static void print_version(void)
{
  printf("%s  Ver %s Distrib %s, for %s (%s)\n",my_progname,DUMP_VERSION,
   MYSQL_SERVER_VERSION,SYSTEM_TYPE,MACHINE_TYPE);
}


static void usage(void)
{
  uint i;
  print_version();
  puts("By Igor Romanenko, Monty, Jani & Sinisa. This software is in public Domain");
  puts("This software comes with ABSOLUTELY NO WARRANTY\n");
  puts("Dumping definition and data mysql database or table");
  printf("Usage: %s [OPTIONS] database [tables]\n",my_progname);
  printf("\n\
  -#, --debug=...       Output debug log. Often this is 'd:t:o,filename`\n\
  -?, --help		Displays this help and exits.\n\
  -c, --compleat-insert Use complete insert statements.\n\
  -C, --compress        Use compression in server/client protocol\n\
  -e, --extended_insert-insert Allows utilization of the new, much faster\n\
			INSERT syntax\n\
  --add-drop-table	Add a 'drop table' before each create\n\
  --add-locks		Add a locks around insert statements\n\
  --allow-keywords	Allow creation of column names that are keywords\n\
  -F  --flush-logs	Flush logs file in server before starting dump\n\
  -f, --force		Continue even if we get an sql-error.\n\
  -h, --host=...	Connect to host.\n\
  -l, --lock-tables     Lock all tables for read.\n\
  -t, --no-create-info	Don't write table creation info.\n\
  -d, --no-data		No row information.\n\
  -O, --set-variable var=option\n\
			give a variable an value. --help lists variables\n\
  --opt			Same as --quick --add-drop-table --add-locks\n\
			--extended_insert-insert --use-locks\n\
  -p, --password[=...]	Password to use when connecting to server.\n\
			If password is not given it's asked from the tty.\n");
#ifdef __WIN32__
  puts("-W, --pipe		Use named pipes to connect to server");
#endif
  printf("\
  -P, --port=...	Port number to use for connection.\n\
  -q, --quick		Don't buffer query, dump directly to stdout.\n\
  -S, --socket=...	Socket file to use for connection.\n\
  -T, --tab=...         Creates tab separated textfile for each table to\n\
	                given path. (creates .sql and .txt files)\n.\n\
			NOTE: This only works if mysqldump is run on\n\
			      the same machine as the mysqld daemon\n");
#ifndef DONT_ALLOW_USER_CHANGE
  printf("\
  -u, --user=#		User for login if not current user.\n");
#endif
  printf("\
  -v, --verbose		Print info about the various stages.\n\
  -V, --version		Output version information and exit.\n\
  -w, --where=		dump only selected records; QUOTES mandatory!\n\
  EXAMPLES: \"--where=user=\'jimf\'\" \"-wuserid>1\" \"-wuserid<1\"\n\
  Use -T (--tab=...) with --fields-...\n\
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
  printf("\nPossible variables for option --set-variable (-O) are:\n");
  for (i=0 ; changeable_vars[i].name ; i++)
    printf("%-20s  current value: %ul\n",
     changeable_vars[i].name,
     (ulong) *changeable_vars[i].varptr);
}


static void write_heder(FILE *sql_file)
{
  fprintf(sql_file, "# MySQL dump %s\n#\n", DUMP_VERSION);
  fprintf(sql_file, "# Host: %s    Database: %s\n",
    current_host ? current_host : "localhost", current_db);
  fputs("#--------------------------------------------------------\n",
  sql_file);
  fprintf(sql_file, "# Server version\t%s\n", mysql_get_server_info(&mysql));
}

static my_string load_default_groups[]= { "mysqldump","client",0 };

static int get_options(int *argc,char ***argv)
{
  int c,option_index;
  bool tty_password=0;

  load_defaults("my",load_default_groups,argc,argv);
  set_all_changeable_vars(changeable_vars);
  while ((c=getopt_long(*argc,*argv,"#::p::h:u:O:P:S:T:cCdeflqtvVw:?I"
      ,long_options, &option_index)) != EOF)
  {
    switch(c) {
    case 'e':
      extended_insert=1;
      break;
    case 'f':
      ignore_errors=1;
      break;
    case 'F':
      flush_logs=1;
      break;
    case 'h':
      my_free(current_host,MYF(MY_ALLOW_ZERO_PTR));
      current_host=my_strdup(optarg,MYF(MY_WME));
      break;
#ifndef DONT_ALLOW_USER_CHANGE
    case 'u':
      current_user=optarg;
      break;
#endif
    case 'O':
      if (set_changeable_var(optarg, changeable_vars))
      {
  usage();
  return(1);
      }
      break;
    case 'p':
      if (optarg)
      {
  my_free(password,MYF(MY_ALLOW_ZERO_PTR));
  password=my_strdup(optarg,MYF(MY_FAE));
  while (*optarg) *optarg++= 'x';    /* Destroy argument */
      }
      else
  tty_password=1;
      break;
    case 'P':
      opt_mysql_port= (unsigned int) atoi(optarg);
      break;
    case 'S':
      opt_mysql_unix_port= optarg;
      break;
    case 'W':
#ifdef __WIN32__
      opt_mysql_unix_port=MYSQL_NAMEDPIPE;
#endif
      break;
    case 'T':
      path= optarg;
      break;
    case '#':
      DBUG_PUSH(optarg ? optarg : "d:t:o");
      break;
    case 'c': cFlag=1; break;
    case 'C':
      opt_compress=1;
      break;
    case 'd': dFlag=1; break;
    case 'l': lock_tables=1; break;
    case 'q': quick=1; break;
    case 't': tFlag=1;  break;
    case 'v': verbose=1; break;
    case 'V': print_version(); exit(0);
    case 'w':
      where=optarg;
      break;
    default:
      fprintf(stderr,"%s: Illegal option character '%c'\n",my_progname,opterr);
      /* Fall throught */
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
    case (int) OPT_DROP:
      opt_drop=1;
      break;
    case (int) OPT_KEYWORDS:
      opt_keywords=1;
      break;
    case (int) OPT_LOCKS:
      opt_lock=1;
      break;
    case (int) OPT_OPTIMIZE:
      extended_insert=opt_drop=opt_lock=lock_tables=quick=1;
      break;
    }
  }
  if (enclosed && opt_enclosed)
  {
    fprintf(stderr, "%s: You can't use ..enclosed.. and ..optionally-enclosed.. at the same time.\n", my_progname);
    return(1);
  }
  if (replace && ignore)
  {
    fprintf(stderr, "%s: You can't use --ignore (-i) and --replace (-r) at the same time.\n",my_progname);
    return(1);
  }
  (*argc)-=optind;
  (*argv)+=optind;
  if (*argc < 1)
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


/*
** DBerror -- prints mysql error message and exits the program.
*/

static void DBerror(MYSQL *mysql)
{
  DBUG_ENTER("DBerror");
  my_printf_error(0,"Got error: %d: %s", MYF(0), mysql_errno(mysql),
      mysql_error(mysql));
  safe_exit(EX_MYSQLERR);
  DBUG_VOID_RETURN;
}

static void safe_exit(int error)
{
  if (!first_error)
    first_error= error;
  if (ignore_errors)
    return;
  if (sock)
    mysql_close(sock);
  exit(error);
}
/*
** dbConnect -- connects to the host and selects DB.
**        Also checks whether the tablename is a valid table name.
*/

void dbConnect(char *host, char *database,char *user,char *passwd)
{
  DBUG_ENTER("dbConnect");
  if (verbose)
  {
    fprintf(stderr, "Connecting to %s...\n", host ? host : "localhost");
  }
  mysql_init(&mysql);
  if (opt_compress)
    mysql_options(&mysql,MYSQL_OPT_COMPRESS,NullS);
  if (!(sock= mysql_real_connect(&mysql,host,user,passwd,
         database,opt_mysql_port,opt_mysql_unix_port,
         0)))
  {
    ignore_errors=0;    /* NO RETURN FROM DBerror */
    DBerror(&mysql);
  }
  DBUG_VOID_RETURN;
}



/*
** dbDisconnect -- disconnects from the host.
*/

void dbDisconnect(char *host)
{
  if (verbose)
    fprintf(stderr, "Disconnecting from %s...\n", host ? host : "localhost");
  mysql_close(sock);
}

static void unescape(FILE *file,char *pos,uint length)
{
  char *tmp=(char*) my_malloc(length*2+1, MYF(MY_WME));
  if (!tmp)
  {
    ignore_errors=0;        /* Fatal error */
    safe_exit(EX_MYSQLERR);      /* Force exit */
  }
  mysql_escape_string(tmp, pos, length);
  fputc('\'', file);
  fputs(tmp, file);
  fputc('\'', file);
  my_free(tmp, MYF(MY_WME));
}


/*
** getStructure -- retrievs database structure, prints out corresponding
**       CREATE statement and fills out insert_pat.
** Return values:  number of fields in table, 0 if error
*/

uint getTableStructure(char *table)
{
  MYSQL_RES  *tableRes;
  MYSQL_ROW  row;
  my_bool    init=0;
  uint       numFields;
  char       *strpos;
  FILE       *sql_file = stdout;

  if (verbose)
    fprintf(stderr, "Retrieving table structure for table %s...\n", table);

  sprintf(insert_pat,"show fields from %s",table);
  if (mysql_query(sock,insert_pat) || !(tableRes=mysql_store_result(sock)))
  {
    fprintf(stderr, "%s: Can't get info about table: '%s'\nerror: %s\n",
	    my_progname, table, mysql_error(sock));
    if (sql_file != stdout)
      my_fclose(sql_file, MYF(MY_WME));
    safe_exit(EX_MYSQLERR);
    return 0;
  }

  /* Make an sql-file, if path was given iow. option -T was given */
  if (!tFlag)
  {
    if (path)
    {
      char filename[FN_REFLEN], tmp_path[FN_REFLEN];
      strmov(tmp_path,path);
      convert_dirname(tmp_path);
      sql_file= my_fopen(fn_format(filename, table, tmp_path, ".sql", 4),
			 O_WRONLY, MYF(MY_WME));
      if (!sql_file)			/* If file couldn't be opened */
      {
	safe_exit(EX_MYSQLERR);
	return 0;
      }
      write_heder(sql_file);
    }
    fprintf(sql_file, "\n#\n# Table structure for table '%s'\n#\n", table);
    if (opt_drop)
      fprintf(sql_file, "DROP TABLE IF EXISTS %s;\n", table);
    fprintf(sql_file, "CREATE TABLE %s (\n", table);
  }
  if (cFlag)
    sprintf(insert_pat, "INSERT INTO %s (", table);
  else
  {
    sprintf(insert_pat, "INSERT INTO %s VALUES ", table);
    if (!extended_insert)
      strcat(insert_pat,"(");
  }

  strpos=strend(insert_pat);
  while ((row=mysql_fetch_row(tableRes)))
  {
    ulong *lengths=mysql_fetch_lengths(tableRes);
    if (init)
    {
      if (!tFlag)
	fputs(",\n",sql_file);
      if (cFlag)
	strpos=strmov(strpos,", ");
    }
    init=1;
    if (cFlag)
      strpos=strmov(strpos,row[SHOW_FIELDNAME]);
    if (!tFlag)
    {
      if (opt_keywords)
	fprintf(sql_file, "  %s.%s %s", table, row[SHOW_FIELDNAME],
		row[SHOW_TYPE]);
      else
	fprintf(sql_file, "  %s %s", row[SHOW_FIELDNAME],row[SHOW_TYPE]);
      if (row[SHOW_DEFAULT])
      {
	fputs(" DEFAULT ", sql_file);
	unescape(sql_file,row[SHOW_DEFAULT],lengths[SHOW_DEFAULT]);
      }
      if (!row[SHOW_NULL][0])
	fputs(" NOT NULL", sql_file);
      if (row[SHOW_EXTRA][0])
	fprintf(sql_file, " %s",row[SHOW_EXTRA]);
    }
  }
  numFields = (uint) mysql_num_rows(tableRes);
  mysql_free_result(tableRes);
  if (!tFlag)
  {
    char buff[20+FN_REFLEN];
    uint keynr,primary_key;
    sprintf(buff,"show keys from %s",table);
    if (mysql_query(sock, buff))
    {
      fprintf(stderr, "%s: Can't get keys for table %s (%s)\n",
	      my_progname, table, mysql_error(sock));
      if (sql_file != stdout)
	my_fclose(sql_file, MYF(MY_WME));
      safe_exit(EX_MYSQLERR);
      return 0;
    }

    tableRes=mysql_store_result(sock);
    /* Find first which key is primary key */
    keynr=0;
    primary_key=INT_MAX;
    while ((row=mysql_fetch_row(tableRes)))
    {
      if (atoi(row[3]) == 1)
      {
	keynr++;
#ifdef FORCE_PRIMARY_KEY
	if (atoi(row[1]) == 0 && primary_key == INT_MAX)
	  primary_key=keynr;
#endif
	if (!strcmp(row[2],"PRIMARY"))
	{
	  primary_key=keynr;
	  break;
	}
      }
    }
    mysql_data_seek(tableRes,0);
    keynr=0;
    while ((row=mysql_fetch_row(tableRes)))
    {
      if (atoi(row[3]) == 1)
      {
	if (keynr++)
	  putc(')', sql_file);
	if (atoi(row[1]))       /* Test if dupplicate key */
	  fprintf(sql_file, ",\n  KEY %s (",row[2]); /* Dupplicate allowed */
	else if (keynr == primary_key)
	  fputs(",\n  PRIMARY KEY (",sql_file); /* First UNIQUE is primary */
	else
	  fprintf(sql_file, ",\n  UNIQUE %s (",row[2]); /* UNIQUE key */
      }
      else
	putc(',', sql_file);
      fputs(row[4], sql_file);
      if (row[7])
	fprintf(sql_file, "(%s)",row[7]);      /* Sub key */
    }
    if (keynr)
      putc(')', sql_file);
    fputs("\n);\n", sql_file);
  }
  if (cFlag)
  {
    strpos=strmov(strpos,") VALUES ");
    if (!extended_insert)
      strpos=strmov(strpos,"(");
  }
  return(numFields);
}


/*
** dumpTable saves database contents as a series of INSERT statements.
*/

char *add_load_option(char *ptr,const char *object,const char *statement)
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
	*to++= *from;      /* We want a duplicate of "'" for MySQL */
      end_backslashes=0;
    }
  }
  /* Add missing backslashes if user has specified odd number of backs.*/
  if (end_backslashes)
    *to++= '\\';
  return to;
}


void dumpTable(uint numFields, char *table)
{
  char query[1024], *end, buff[256];
  MYSQL_RES	*res;
  MYSQL_FIELD  *field;
  MYSQL_ROW    row;
  uint 		i;
  my_bool	init = 1;
  ulong		rownr=0L, row_length=0,  total_length = 0, row_break = 0;

  if (verbose)
    fprintf(stderr, "Sending SELECT query...\n");
  if (path)
  {
    char filename[FN_REFLEN], tmp_path[FN_REFLEN];
    strmov(tmp_path, path);
    convert_dirname(tmp_path);
    my_load_path(tmp_path, tmp_path, NULL);
    fn_format(filename, table, tmp_path, ".txt", 4);
    my_delete(filename, MYF(0)); /* 'INTO OUTFILE' doesn't work, if
				    filename wasn't deleted */
    to_unix_path(filename);
    sprintf(query, "SELECT * INTO OUTFILE '%s'", filename);
    end= strend(query);
    if (replace)
      end= strmov(end, " REPLACE");
    if (ignore)
      end= strmov(end, " IGNORE");

    if (fields_terminated || enclosed || opt_enclosed || escaped)
      end= strmov(end, " FIELDS");
    end= add_load_option(end, fields_terminated, " TERMINATED BY");
    end= add_load_option(end, enclosed, " ENCLOSED BY");
    end= add_load_option(end, opt_enclosed, " OPTIONALLY ENCLOSED BY");
    end= add_load_option(end, escaped, " ESCAPED BY");
    end= add_load_option(end, lines_terminated, " LINES TERMINATED BY");
    *end= '\0';

    sprintf(buff," FROM %s",table);
    end= strmov(end,buff);
    if (where)
      end= strxmov(end, " WHERE ",where,NullS);
    if (mysql_query(sock, query))
    {
      DBerror(sock);
      return;
    }
  }
  else
  {
    printf("\n#\n# Dumping data for table '%s'\n", table);
    sprintf(query, "SELECT * FROM %s", table);
    if (where)
    {
      printf("# WHERE:  %s\n",where);
      strxmov(strend(query), " WHERE ",where,NullS);
    }
    puts("#\n");

    if (mysql_query(sock, query))
    {
      DBerror(sock);
      return;
    }
    if (quick)
      res=mysql_use_result(sock);
    else
      res=mysql_store_result(sock);
    if (!res)
    {
      DBerror(sock);
      return;
    }
    if (verbose)
      fprintf(stderr, "Retrieving rows...\n");
    if (mysql_num_fields(res) != numFields)
    {
      fprintf(stderr,"%s: Error in field count for table: %s !  Aborting.\n",
	      my_progname,table);
      safe_exit(EX_CONSCHECK);
      return;
    }

    if (opt_lock)
      printf("LOCK TABLES %s WRITE;\n", table);

    total_length=net_buffer_length;		/* Force row break */

    while ((row=mysql_fetch_row(res)))
    {
      ulong *lengths=mysql_fetch_lengths(res);
      rownr++;
      init = 1;
      if (extended_insert)
      {
	row_length=strlen(insert_pat)+2;
	dynstr_set(&extended_row,"(");
      }
      else
	fputs(insert_pat,stdout);
      mysql_field_seek(res,0);
      for (i = 0; i < mysql_num_fields(res); i++)
      {
	if (!(field = mysql_fetch_field(res)))
	{
	  sprintf(query,"%s: Not enough fields from table %s! Aborting.\n",
		  my_progname,table);
	  fputs(query,stdout);
	  fputs(query,stderr);
	  safe_exit(EX_CONSCHECK);
	  return;
	}
	if (extended_insert)
	{
	  ulong length = lengths[i];
	  if (!init)
	    dynstr_append(&extended_row,",");
	  else
	    init=0;
	  if (row[i])
	  {
	    if (length)
	    {
	      if (!IS_NUM(field->type))
	      {
		if (dynstr_realloc(&extended_row,length * 2+2))
		{
		  puts("Aborting dump because of errors");
		  safe_exit(EX_EOM);
		}
		dynstr_append(&extended_row,"\'");
		extended_row.length += mysql_escape_string(&extended_row.str[extended_row.length],row[i],length);
		extended_row.str[extended_row.length]='\0';
		dynstr_append(&extended_row,"\'");
	      }
	      else
		dynstr_append(&extended_row,row[i]);
	    }
	    else
	      dynstr_append(&extended_row,"\'\'");
	  }
	  else if (dynstr_append(&extended_row,"NULL"))
	  {
	    puts("Aborting dump because of errors");
	    safe_exit(EX_EOM);
	  }
	}
	else
	{
	  if (!init)
	    printf(",");
	  else
	    init=0;
	  if (row[i])
	  {
	    if (!IS_NUM(field->type))
	      unescape(stdout, row[i], lengths[i]);
	    else
	      fputs(row[i],stdout);
	  }
	  else
	  {
	    fputs("NULL",stdout);
	  }
	}
      }
      if (extended_insert)
      {
	dynstr_append(&extended_row,")");
        row_length += 2 + extended_row.length; /* braces (both inside and outside) */
        if (total_length + row_length < net_buffer_length)
        {
	  total_length += row_length;
	  if (row_break)
	    fputs(",",stdout);
	  fputs(extended_row.str,stdout);
	  row_break++;
	}
        else
        {
	  if (row_break)
	    fputs (";\n",stdout);
	  row_break=1;				/* This is first row */
	  fputs (insert_pat,stdout);
	  fputs (extended_row.str,stdout);
	  total_length = row_length;
        }
      }
      else
      {
	fputs(")",stdout);
	puts(";");
      }
    }
    if (extended_insert && row_break)
      fputs (";\n",stdout);			/* If not empty table */
    if (mysql_errno(sock))
    {
      sprintf(query,"%s: Error %d: %s when dumping table %s at row: %ld\n",
	      my_progname,
	      mysql_errno(sock),
	      mysql_error(sock),
	      table,
	      rownr);
      fputs(query,stdout);
      fputs(query,stderr);
      safe_exit(EX_CONSCHECK);
      return;
    }
    if (opt_lock)
      puts("UNLOCK TABLES;");
    mysql_free_result(res);
  }
}



char *getTableName(int reset)
{
  static MYSQL_RES *res = NULL;
  MYSQL_ROW    row;

  if (!res)
  {
    if (!(res = mysql_list_tables(sock,NullS)))
      return(NULL);
  }
  if ((row = mysql_fetch_row(res)))
  {
    return((char*) row[0]);
  }
  else
  {
    if (reset)
      mysql_data_seek(res,0);      /* We want to read again */
    else
      mysql_free_result(res);
    return(NULL);
  }
}


int main(int argc, char **argv)
{
  uint  numRows;
  MY_INIT(argv[0]);

  /*
  ** Check out the args
  */
  if (get_options(&argc,&argv))
  {
    my_end(0);
    exit(EX_USAGE);
  }
  dbConnect(current_host,current_db,current_user,password);
  if (!path)
    write_heder(stdout);
  if (extended_insert)
    if (init_dynamic_string(&extended_row, "", 1024, 1024))
      exit(EX_EOM);

  if (argc)
  {
    if (lock_tables)
    {
      DYNAMIC_STRING query;
      int i;
      init_dynamic_string(&query, "LOCK TABLES ", 256, 1024);
      for (i=0 ; i < argc ; i++)
      {
	dynstr_append(&query, argv[i]);
	dynstr_append(&query, " READ,");
      }
      if (mysql_real_query(sock, query.str, query.length-1))
	DBerror(sock); /* We shall countinue here, if --force was given */
    }
    if (flush_logs)
    {
      if (mysql_refresh(sock, REFRESH_LOG))
	DBerror(sock); /* We shall countinue here, if --force was given */
    }
    for (; argc > 0 ; argc-- , argv++)
    {
      numRows = getTableStructure(*argv);
      if (!dFlag && numRows > 0)
	dumpTable(numRows,*argv);
    }
  }
  else
  {
    char *table;
    if (lock_tables)
    {
      DYNAMIC_STRING query;
      init_dynamic_string(&query, "LOCK TABLES ", 256, 1024);
      while ((table = getTableName(1)))
      {
	dynstr_append(&query, table);
	dynstr_append(&query, " READ,");
      }
      if (mysql_real_query(sock, query.str, query.length-1))
	DBerror(sock);  /* We shall countinue here, if --force was given */
    }
    while ((table = getTableName(0)))
    {
      numRows = getTableStructure(table);
      if (!dFlag && numRows > 0)
	dumpTable(numRows,table);
    }
  }
  dbDisconnect(current_host);
  puts("");
  my_free(password,MYF(MY_ALLOW_ZERO_PTR));
  if (extended_insert)
    dynstr_free(&extended_row);
  my_end(0);
  return(first_error);
}
