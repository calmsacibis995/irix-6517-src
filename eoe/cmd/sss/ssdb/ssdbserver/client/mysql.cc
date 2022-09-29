/* Copyright Abandoned 1996 TCX DataKonsult AB & Monty Program KB & Detron HB
   This file is public domain and comes with NO WARRANTY of any kind */

/* mysql command tool
 * Commands compatible with mSQL by David J. Hughes
 *
 * Written by:
 *   Michael 'Monty' Widenius
 *   Andi Gutmans <andi@vipe.technion.ac.il>
 *   Zeev Suraski <bourbon@netvision.net.il>
 *
 **/

#include <global.h>
#include <my_sys.h>
#include <m_string.h>
#include <m_ctype.h>
#include "mysql.h"
#include "errmsg.h"
#include <my_dir.h>
#ifndef __GNU_LIBRARY__
#define __GNU_LIBRARY__				// Skipp warnings in getopt.h
#endif
#include <getopt.h>
#include "mysql_version.h"
#include <signal.h>

gptr sql_alloc(unsigned size);			// Don't use mysqld alloc for theese
void sql_element_free(void *ptr);
#include "sql_string.h"

extern "C" {
#if defined(HAVE_TERMIOS)
#include <termios.h>
#include <unistd.h>
#elif defined(HAVE_TERMBITS_H)
#include <termbits.h>
#elif defined(HAVE_ASM_TERMBITS_H)
#include <asm/termbits.h>			// Standard linux
#endif
#undef VOID
#ifdef HAVE_TERMCAP_H
#include <termcap.h>
#else
#ifdef HAVE_CURSES_H
#include <curses.h>
#endif
#undef SYSV					// hack to avoid syntax error
#ifdef HAVE_TERM_H
#include <term.h>
#endif
#endif
#undef bcmp					// Fix problem with new readline
#undef bzero
#ifdef __WIN32__
#include <conio.h>
#else
#include <readline/readline.h>
#define HAVE_READLINE
#endif
  //int vidattr(long unsigned int attrs);	// Was missing in sun curses
}

#if defined(__linux__) || !defined(HAVE_VIDATTR)
#undef vidattr
#define vidattr(A) {}				// Can't get this to work
#endif

#include "completion_hash.h"

static HashTable ht;

enum enum_info_type { INFO_INFO,INFO_ERROR,INFO_RESULT};
typedef enum enum_info_type INFO_TYPE;

static MYSQL mysql;			/* The connection */
static bool info_flag=0,batch=0,ignore_errors=0,wait_flag=0,quick=0,
	    connected=0,opt_raw_data=0,unbuffered=0,output_tables=0,
	    no_rehash=0,skip_updates=0,one_database=0,opt_compress=0,
            vertical=0,skip_line_numbers=0;
static uint verbose=0,silent=0,opt_mysql_port=0;
static my_string opt_mysql_unix_port=0;
static int exit_status=1;
static char *current_host,*current_db,*current_user=0,*password=0;
static char *histfile;
static String buffer,old_buffer;
static ulong line_number=0L,query_start_line=0L;

#ifndef DBUG_OFF
char *default_dbug_option="d:t:o,/tmp/mysql.trace";
#endif

/* The names of functions that actually do the manipulation. */
static int get_options(int argc,char **argv);
static int com_quit(String *str,char*), 
	   com_go(String *str,char*), com_ego(String *str,char*),
	   com_edit(String *str,char*), com_print(String *str,char*),
	   com_help(String *str,char*), com_clear(String *str,char*),
	   com_connect(String *str,char*), com_status(String *str,char*),
	   com_use(String *str,char*),
	   com_rehash(String *str, char*);

static int sql_connect(char *host,char *database,char *user,char *password,
		       uint silent);
static int put_info(const char *str,INFO_TYPE info,uint error=0);

/* A structure which contains information on the commands this program
   can understand. */

typedef struct {
  const char *name;		/* User printable name of the function. */
  char cmd_char;		/* msql command character */
  int (*func)(String *str,char *); /* Function to call to do the job. */
  bool takes_params;		/* Max parameters for command */
  const char *doc;		/* Documentation for this function.  */
} COMMANDS;

static COMMANDS commands[] = {
  { "help", 'h',com_help, 0,"Display this text" },
  { "?",    'h',com_help, 0,"Synonym for `help'" },
  { "clear",'c',com_clear,0,"Clear command"},
  { "connect",'r',com_connect,1,
    "Reconnect to the server. Optional arguments are db and host" },
  { "edit", 'e',com_edit, 0,"Edit command with $EDITOR"},
  { "exit", 0, com_quit,  0,"Exit mysql. Same as quit"},
  { "go",   'g',com_go,   0,"Send command to mysql server" },
  { "ego",  'G',com_ego,  0,
    "Send command to mysql server; Display result verically"},
  { "print",'p',com_print,0,"print current command" },
  { "quit", 'q',com_quit, 0,"Quit mysql" },
  { "rehash", '#', com_rehash, 0, "Rebuild completion hash" },
  { "status",'s',com_status,0,"Get status information from the server"},
  { "use",'u',com_use,1,
    "Use another database. Takes database name as argument" },

  { "create table",0,0,0,""},	/* Get bash expansion for some commmands */
  { "create database",0,0,0,""},
  { "drop",0,0,0,""},
  { "select",0,0,0,""},
  { "insert",0,0,0,""},
  { "replace",0,0,0,""},
  { "update",0,0,0,""},
  { "delete",0,0,0,""},
  { "explain",0,0,0,""},
  { "show databases",0,0,0,""},
  { "show fields from",0,0,0,""},
  { "show keys from",0,0,0,""},
  { "show tables",0,0,0,""},
  { "load data from",0,0,0,""},
  { "alter table",0,0,0,""},
  { "set option",0,0,0,""},
  { "lock tables",0,0,0,""},
  { "unlock tables",0,0,0,""},
  { (char *)NULL, 0,0,0,""},
};

static my_string load_default_groups[]= { "mysql","client",0 };

extern my_bool batch_readline_init(ulong max_size);
extern char *batch_readline(void);
extern void batch_readline_end(void);
extern void batch_readline_command(char *str);
#ifdef HAVE_READLINE
extern "C" void add_history(char *command); /* From readline directory */
extern "C" int read_history(char *command);
extern "C" int write_history(char *command);
static void initialize_readline (char *name);
#endif

static COMMANDS *find_command (char *name,char cmd_name);
static bool add_line(String &buffer,char *line,char *in_string);
static void remove_cntrl(String &buffer);
static void print_table_data(MYSQL_RES *result);
static void print_tab_data(MYSQL_RES *result);
static void print_table_data_vertically(MYSQL_RES *result);
static ulong start_timer(void);
static void end_timer(ulong start_time,char *buff);
static void mysql_end_timer(ulong start_time,char *buff);
static void nice_time(double sec,char *buff,bool part_second);
static sig_handler mysql_end(int sig);


int main(int argc,char *argv[])
{
#ifdef __WIN32__
  char linebuffer[254];
#endif
  char	*line;
  char	in_string=0;
  COMMANDS *com;
  MY_INIT(argv[0]);
  DBUG_ENTER("main");
  DBUG_PROCESS(argv[0]);

  if (!isatty(0) || !isatty(1))
  {
    batch=1; silent=1;
    ignore_errors=0;
  }
  load_defaults("my",load_default_groups,&argc,&argv);
  if (get_options(argc,(char **) argv))
  {
    my_end(0);
    exit(1);
  }
  free_defaults(argv);
  if (batch && batch_readline_init(max_allowed_packet+512))
    exit(1);
  buffer.realloc(512);
  completion_hash_init(&ht,50);
  if (sql_connect(current_host,current_db,current_user,password,silent))
  {
    if (connected)
      mysql_close(&mysql);
    buffer.free();
    old_buffer.free();
    my_end(0);
    exit(1);
  }
  if (!batch)
    ignore_errors=1;				// Don't abort monitor
  signal(SIGINT, mysql_end);			// Catch SIGINT to clean up

 /*
  **  Run in interactive mode like the ingres/postgres monitor
  */

  put_info("Welcome to the MySQL monitor.  Commands end with ; or \\g.",
	   INFO_INFO);
  sprintf((char*) buffer.ptr(),"Your MySQL connection id is %d to server version: %s\n",
	  mysql_thread_id(&mysql),mysql_get_server_info(&mysql));
  put_info((char*) buffer.ptr(),INFO_INFO);

#ifdef HAVE_READLINE
  initialize_readline(my_progname);
  if (!batch && !quick)
  {
    /*read-history from file, default ~/.mysql_history*/
    if (getenv("MYSQL_HISTFILE"))
      histfile=my_strdup(getenv("MYSQL_HISTFILE"),MYF(MY_WME));
    else if (getenv("HOME"))
    {
      histfile=my_malloc(strlen(getenv("HOME")) + strlen("/.mysql_history")+2,
			 MYF(MY_WME));
      if (histfile)
	sprintf(histfile,"%s/.mysql_history",getenv("HOME"));
    }
    if (histfile)
    {
      if (verbose)
	printf("Reading history-file %s\n",histfile);
      read_history(histfile);
    }
  }
#endif
  put_info("Type 'help' for help.\n",INFO_INFO);

  for (;;)
  {
    if (batch)
    {
      line=batch_readline();
      line_number++;
      if (!buffer.length())
	query_start_line=line_number;
    }
    else
#ifdef __WIN32__
    {
      printf(buffer.is_empty() ? "mysql> " :
	     !in_string ? "    -> " :
	     in_string == '\'' ?
	     "    '> " : "    \"> ");
      linebuffer[0]=(char) sizeof(linebuffer);
      line=_cgets(linebuffer);
    }
#else
      line=readline((char*) (buffer.is_empty() ? "mysql> " :
			     !in_string ? "    -> " :
			     in_string == '\'' ?
			     "    '> " : "    \"> "));
#endif
    if (!line)					// End of file
    {
      exit_status=0;
      break;
    }
    if (!in_string && (line[0] == '#' ||
		       (line[0] == '-' && line[1] == '-') ||
		       line[0] == 0))
      continue;					// Skipp comment lines

    /* Check if line is a mysql command line */
    /* (We want to allow help, print and clear anywhere at line start */

    if (!in_string && (com=find_command(line,0)))
    {
      if ((*com->func)(&buffer,line) > 0)
	break;
      if (buffer.is_empty())			// If buffer was emptied
	in_string=0;
#ifdef HAVE_READLINE
      if (!batch)
	add_history(line);
#endif
      continue;
    }
    if (add_line(buffer,line,&in_string))
      break;
  }
  /* if in batch mode, send last query even if it doesn't end with \g or go */

  if (batch && !exit_status)
  {
    remove_cntrl(buffer);
    if (!buffer.is_empty())
    {
      exit_status=1;
      if (com_go(&buffer,line) <= 0)
	exit_status=0;
    }
  }
  mysql_end(0);
#ifndef _lint
  DBUG_RETURN(0);				/* Keep compiler happy */
#endif
}

sig_handler mysql_end(int sig)
{
  if (connected)
    mysql_close(&mysql);
#ifdef HAVE_READLINE
  if (!batch && !quick)
  {
    /* write-history */
    if (verbose)
      printf("Writing history-file %s\n",histfile);
    write_history(histfile);
  }
  batch_readline_end();
  completion_hash_free(&ht);
#endif
  put_info(sig ? "Aborted" : "Bye", INFO_RESULT);
  buffer.free();
  old_buffer.free();
  my_free(password,MYF(MY_ALLOW_ZERO_PTR));
  my_free(opt_mysql_unix_port,MYF(MY_ALLOW_ZERO_PTR));
  my_free(histfile,MYF(MY_ALLOW_ZERO_PTR));
  my_free(current_host,MYF(MY_ALLOW_ZERO_PTR));
  my_free(current_user,MYF(MY_ALLOW_ZERO_PTR));
  my_end(info_flag ? MY_CHECK_ERROR | MY_GIVE_INFO : 0);
  exit(exit_status);
}


static struct option long_options[] =
{
  {"batch",	    no_argument,	   0, 'B'},
  {"compress",	    no_argument,	   0, 'C'},
#ifndef DBUG_OFF
  {"debug",	    optional_argument,	   0, '#'},
#endif
  {"debug-info",    no_argument,	   0, 'T'},
  {"execute",	    required_argument,	   0, 'e'},
  {"force",	    no_argument,	   0, 'f'},
  {"help",	    no_argument,	   0, '?'},
  {"host",	    required_argument,	   0, 'h'},
  {"no-auto-rehash",no_argument,	   0, 'A'},
  {"one-database",  no_argument,	   0, 'o'},
  {"password",	    optional_argument,	   0, 'p'},
#ifdef __WIN32__
  {"pipe",	    no_argument,	   0, 'W'},
#endif
  {"port",	    required_argument,	   0, 'P'},
  {"quick",	    no_argument,	   0, 'q'},
  {"set-variable",  required_argument,	   0, 'O'},
  {"raw",	    no_argument,	   0, 'r'},
  {"silent",	    no_argument,	   0, 's'},
  {"skip-line-numbers",no_argument,	   0, 'L'},
  {"socket",	    required_argument,	   0, 'S'},
  {"table",	    no_argument,	   0, 't'},
#ifndef DONT_ALLOW_USER_CHANGE
  {"user",	    required_argument,	   0, 'u'},
#endif
  {"unbuffered",    no_argument,	   0, 'n'},
  {"verbose",	    no_argument,	   0, 'v'},
  {"version",	    no_argument,	   0, 'V'},
  {"vertical",      no_argument,           0, 'E'},
  {"wait",	    no_argument,	   0, 'w'},
  {0, 0, 0, 0}
};


CHANGEABLE_VAR changeable_vars[] = {
  { "max_allowed_packet", (long*) &max_allowed_packet,24*1024L*1024L,4096,
    24*1024L*1024L, MALLOC_OVERHEAD,1024},
  { "net_buffer_length",(long*) &net_buffer_length,16384,1024,24*1024*1024L,
    MALLOC_OVERHEAD,1024},
  { 0, 0, 0, 0, 0, 0, 0}
};


static void usage(int version)
{
  printf("%s  Ver 9.27 Distrib %s, for %s (%s)\n",
	 my_progname, MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE);
  if (version)
    return;
  puts("By TCX Datakonsult AB, by Monty");
  puts("This software comes with ABSOLUTELY NO WARRANTY.\n");
  printf("Usage: %s [OPTIONS] [database]\n", my_progname);
  printf("\n\
  -A, --no-auto-rehash  No automatic rehashing. One has to use 'rehash' to\n\
			get table and field completion. This gives a quicker\n\
			start of mysql.\n\
  -B, --batch		Print results with a tab as separator, each row on\n\
			a new line. Doesn't use history file\n\
  -C, --compress	Use compression in server/client protocol\n");
#ifndef DBUG_OFF
  printf("\
  -#, --debug[=...]     Debug log. Default is '%s'\n",default_dbug_option);
#endif
  printf("\
  -T, --debug-info	Print some debug info at exit\n\
  -e, --execute=...	Execute command and quit.(--batch is implicit)\n\
  -f, --force		Continue even if we get an sql error.\n\
  -?, --help		Display this help and exit\n\
  -h, --host=...	Connect to host\n\
  -n, --unbuffered	Flush buffer after each query\n\
  -O, --set-variable var=option\n\
			Give a variable an value. --help lists variables\n\
  -o, --one-database	Only update the default database. This is useful\n\
			for skipping updates to other database in the update\n\
			log.\n\
  -p[password], --password[=...]\n\
			Password to use when connecting to server\n\
			If password is not given it's asked from the tty.\n");
#ifdef __WIN32__
  puts("  -W, --pipe		Use named pipes to connect to server");
#endif
  printf("\n\
  -P  --port=...	Port number to use for connection\n\
  -q, --quick		Don't cache result, print it row by row. This may\n\
			slow down the server if the output is suspended.\n\
			Doesn't use history file\n\
  -r, --raw		Write fields without conversion. Used with --batch\n\
  -s, --silent		Be more silent.\n\
  -L, --skip-line-numbers  Don't write line number for errors\n\
  -S  --socket=...	Socket file to use for connection\n\
  -t  --table=...	Output in table format\n");

#ifndef DONT_ALLOW_USER_CHANGE
  printf("\
  -u, --user=#		User for login if not current user\n");
#endif
  printf("\
  -v, --verbose		Write more (-v -v -v gives the table output format)\n\
  -V, --version		Output version information and exit\n\
  -E, --vertical        Print the output of a query (rows) vertically\n\
  -w, --wait		Wait and retry if connection is down\n");
  printf("\nPossible variables for option --set-variable (-O) are:\n");
  for (uint i=0 ; changeable_vars[i].name ; i++)
    printf("%-20s  current value: %ul\n",
	   changeable_vars[i].name,
	   (ulong) *changeable_vars[i].varptr);
}


static int get_options(int argc, char **argv)
{
  int c,option_index=0;
  bool tty_password=0;

  set_all_changeable_vars(changeable_vars);
  while ((c=getopt_long(argc,argv,"?ABCLfnoqrstTvVwWEe:h:O:P:S:u:#::p::",
			long_options, &option_index)) != EOF)
  {
    switch(c) {
    case 'e':
      batch=1;
      batch_readline_command(optarg);
      ignore_errors=0;
      break;
    case 'f':
      ignore_errors=1;
      break;
    case 'h':
      my_free(current_host,MYF(MY_ALLOW_ZERO_PTR));
      current_host=my_strdup(optarg,MYF(MY_WME));
      break;
#ifndef DONT_ALLOW_USER_CHANGE
    case 'u':
      current_user= my_strdup(optarg,MYF(0));
      break;
#endif
    case 'o':
      one_database=1;
      break;
    case 'O':
      if (set_changeable_var(optarg, changeable_vars))
      {
	usage(0);
	return(1);
      }
      break;
    case 'p':
      if (optarg)
      {
	my_free(password,MYF(MY_ALLOW_ZERO_PTR));
	password=my_strdup(optarg,MYF(MY_FAE));
	while (*optarg) *optarg++= 'x';		// Destroy argument
      }
      else
	tty_password=1;
      break;
    case 't':
      output_tables=1;
      break;
    case 'r':
      opt_raw_data=1;
      break;
    case '#':
      DBUG_PUSH(optarg ? optarg : default_dbug_option);
      info_flag=1;
      break;
    case 'q': quick=1; break;
    case 's': silent++; break;
    case 'T': info_flag=1; break;
    case 'n': unbuffered=1; break;
    case 'v': verbose++; break;
    case 'E': vertical=1; break;
    case 'w': wait_flag=1; break;
    case 'A': no_rehash=1; break;
    case 'B':
      if (!batch)
      {
	batch=1;
	silent++;				// more silent
      }
      break;
    case 'C':
      opt_compress=1;
      break;
    case 'L':
      skip_line_numbers=1;
      break;
    case 'P':
      opt_mysql_port= (unsigned int) atoi(optarg);
      break;
    case 'S':
      my_free(opt_mysql_unix_port,MYF(MY_ALLOW_ZERO_PTR));
      opt_mysql_unix_port= my_strdup(optarg,MYF(0));
      break;
    case 'W':
#ifdef __WIN32__
      opt_mysql_unix_port=MYSQL_NAMEDPIPE;
#endif
      break;
    case 'V': usage(1); exit(0);
    case 'I':
    case '?':
      usage(0);
      exit(0);
    default:
      fprintf(stderr,"illegal option: -%c\n",opterr);
      usage(0);
      exit(1);
    }
  }
  argc-=optind;
  argv+=optind;
  if (argc > 1)
  {
    usage(0);
    exit(1);
  }
  if (argc == 1)
    current_db= my_strdup(*argv,MYF(MY_WME));
  if (!current_host)
  {	/* If we don't have a hostname have a look at MYSQL_HOST */
    char *tmp=(char *) getenv("MYSQL_HOST");
    if (tmp)
      current_host = my_strdup(tmp,MYF(MY_WME));
  }
  if (tty_password)
    password=get_tty_password(NullS);
  return(0);
}


static COMMANDS *find_command (char *name,char cmd_char)
{
  uint len;
  char *end;

  if (!name)
  {
    len=0;
    end=0;
  }
  else
  {
    while (isspace(*name))
      name++;
    if (strchr(name,';') || strinstr(name,"\\g") != 0)
      return ((COMMANDS *) 0);
    if ((end=strcont(name," \t")))
    {
      len=(uint) (end - name);
      while (isspace(*end))
	end++;
      if (!*end)
	end=0;					// no arguments to function
    }
    else
      len=strlen(name);
  }

  for (uint i= 0; commands[i].name; i++)
  {
    if (commands[i].func &&
	((name && !my_casecmp(name,commands[i].name,len) &&
	  !commands[i].name[len] &&
	  (!end || (end && commands[i].takes_params))) ||
	 !name && commands[i].cmd_char == cmd_char))
      return (&commands[i]);
  }
  return ((COMMANDS *) 0);
}


static bool add_line(String &buffer,char *line,char *in_string)
{
  uchar inchar;
  char buff[80],*pos,*out;
  COMMANDS *com;

  if (!line[0] && buffer.is_empty())
    return 0;
#ifdef HAVE_READLINE
  if (!batch && line[0])
    add_history(line);
#endif

  for (pos=out=line ; (inchar= (uchar) *pos) ; pos++)
  {
    if (isspace(inchar) && out == line && buffer.is_empty())
      continue;
#ifdef USE_BIG5CODE
    if (*(pos+1) && isbig5code(inchar,((uchar) *(pos+1))))
    {
      *out++= (char) inchar;
      *out++= *++pos;
      continue;
    }
#endif
#ifdef USE_MB
    int l;
    if ((l = ismbchar(pos, pos+MBMAXLEN))) {
	while (l--)
	    *out++ = *pos++;
	pos--;
	continue;
    }
#endif
    if (inchar == '\\')
    {					// mSQL or postgreSQL style command ?
      if (!(inchar = (uchar) *++pos))
	break;				// readline adds one '\'
      if (*in_string || inchar == 'N')
      {					// Don't allow commands in string
	*out++='\\';
	*out++= (char) inchar;
	continue;
      }
      if ((com=find_command(NullS,(char) inchar)))
      {
	const String tmp(line,(uint) (out-line));
	buffer.append(tmp);
	if ((*com->func)(&buffer,line) > 0)
	  return 1;				// Quit
	out=line;
      }
      else
      {
	sprintf(buff,"Unknown command '\\%c'.",inchar);
	if (put_info(buff,INFO_ERROR) > 0)
	  return 1;
	*out++='\\';
	*out++=(char) inchar;
	continue;
      }
    }
    else if (inchar == ';' && !*in_string)
    {						// ';' is end of command
      const String tmp(line,(uint) (out-line));
      buffer.append(tmp);			// Add this line
      if ((com=find_command(buffer.c_ptr(),0)))
      {
	if ((*com->func)(&buffer,buffer.c_ptr()) > 0)
	  return 1;				// Quit
      }
      else
      {
	int error=com_go(&buffer,0);
	if (error)
	{
	  return error < 0 ? 0 : 1;		// < 0 is not fatal
	}
      }
      buffer.length(0);
      out=line;
    }
    else if (inchar == '#' && !*in_string)
      break;					// comment to end of line
    else
    {						// Add found char to buffer
      if (inchar == *in_string)
	*in_string=0;
      else if (!*in_string && (inchar == '\'' || inchar == '"'))
	*in_string=(char) inchar;
      *out++ = (char) inchar;
    }
  }
  if (out != line || !buffer.is_empty())
  {
    *out++='\n';
    String tmp(line,(uint) (out-line));
    buffer.append(tmp);
  }
  return 0;
}

/* **************************************************************** */
/*								    */
/*		    Interface to Readline Completion		    */
/*								    */
/* **************************************************************** */

#ifdef HAVE_READLINE

static char *new_command_generator(char *text, int);
static char **new_mysql_completion (char *text, int start, int end);

/* Tell the GNU Readline library how to complete.  We want to try to complete
   on command names if this is the first word in the line, or on filenames
   if not. */

char **no_completion (char *text __attribute__ ((unused)),
		      char *word __attribute__ ((unused)))
{
  return 0;					/* No filename completion */
}

static void initialize_readline (char *name)
{
  /* Allow conditional parsing of the ~/.inputrc file. */
  rl_readline_name = name;

  /* Tell the completer that we want a crack first. */
  /* rl_attempted_completion_function = (CPPFunction *)mysql_completion;*/
  rl_attempted_completion_function = (CPPFunction *) new_mysql_completion;
  rl_completion_entry_function=(Function *) no_completion;
}

/* Attempt to complete on the contents of TEXT.  START and END show the
   region of TEXT that contains the word to complete.  We can use the
   entire line in case we want to do some simple parsing.  Return the
   array of matches, or NULL if there aren't any. */


static char **new_mysql_completion (char *text, int start, int end)
{
  if (!batch && !quick)
    return completion_matches(text, (CPFunction*) new_command_generator);
  else
    return (char**) 0;
}

static char *new_command_generator(char *text,int state)
{
  static int textlen;
  char *ptr;
  static Bucket *b;
  static entry *e;
  static uint i;

  if (!state) {
    textlen=strlen(text);
  }

  if (textlen>0) { /* lookup in the hash */
    if (!state) {
      uint len;

      b = find_all_matches(&ht,text,strlen(text),&len);
      if (!b) {
	return NullS;
      }
      e = b->pData;
    }

    while (e) {
      ptr=strdup(e->str);
      e = e->pNext;
      return ptr;
    }
  } else { /* traverse the entire hash, ugly but works */

    if (!state) {
      i=0;
      /* find the first used bucket */
      while (i<ht.nTableSize) {
	if (ht.arBuckets[i]) {
	  b = ht.arBuckets[i];
	  e = b->pData;
	  break;
	}
	i++;
      }
    }
    ptr= NullS;
    while (e && !ptr) { /* find valid entry in bucket */
      if (strlen(e->str)==b->nKeyLength) {
	ptr = strdup(e->str);
      }
      /* find the next used entry */
      e = e->pNext;
      if (!e) { /* find the next used bucket */
	b = b->pNext;
	if (!b) {
	  i++;
	  while (i<ht.nTableSize) {
	    if (ht.arBuckets[i]) {
	      b = ht.arBuckets[i];
	      e = b->pData;
	      break;
	    }
	    i++;
	  }
	} else {
	  e = b->pData;
	}
      }
    }
    if (ptr) {
      return ptr;
    }
  }
  return NullS;
}


/* Build up the completion hash */

static void build_completion_hash(bool skip_rehash,bool write_info)
{
  COMMANDS *cmd=commands;
  static MYSQL_RES *databases=0,*tables=0,*fields;
  static char ***field_names= 0;
  MYSQL_ROW database_row,table_row;
  MYSQL_FIELD *sql_field;
  char buf[NAME_LEN*2+2];		 // table name plus field name plus 2
  int i,j,num_fields;
  DBUG_ENTER("build_completion_hash");

  if (batch || quick)
    DBUG_VOID_RETURN;			// We don't need completion in batches

  completion_hash_clean(&ht);
  if (tables)
  {
    mysql_free_result(tables);
    tables=0;
  }
  if (databases) {
    mysql_free_result(databases);
    databases=0;
  }

  /* hash SQL commands */
  while (cmd->name) {
    add_word(&ht,(char*) cmd->name);
    cmd++;
  }
  if (skip_rehash)
    DBUG_VOID_RETURN;

  /* hash MySQL functions (to be implemented) */

  /* hash all database names */
  if (mysql_query(&mysql,"show databases")==0) {
    if (!(databases = mysql_store_result(&mysql)))
      put_info(mysql_error(&mysql),INFO_INFO);
    else
    {
      while ((database_row=mysql_fetch_row(databases))) {
	add_word(&ht,(char*) database_row[0]);
      }
    }
  }
  /* hash all table names */
  if (mysql_query(&mysql,"show tables")==0)
  {
    if (!(tables = mysql_store_result(&mysql)))
      put_info(mysql_error(&mysql),INFO_INFO);
    else
    {
      if (mysql_num_rows(tables) > 0 && !silent && write_info)
      {
	printf("\
Reading table information for completion of table and column names\n\
You can turn off this feature to get a quicker startup with -A\n\n");
      }
      while ((table_row=mysql_fetch_row(tables)))
      {
	if (!completion_hash_exists(&ht,(char*) table_row[0],
				    strlen((const char*) table_row[0])))
	  add_word(&ht,table_row[0]);
      }
    }
  }
  if (field_names) {
    for (i=0; field_names[i]; i++) {
      for (j=0; field_names[i][j]; j++) {
	my_free(field_names[i][j],MYF(0));
      }
      my_free((gptr) field_names[i],MYF(0));
    }
    my_free((gptr) field_names,MYF(0));
  }



  /* hash all field names, both with the table prefix and without it */
  if (!tables) { /* no tables */
    DBUG_VOID_RETURN;
  }
  mysql_data_seek(tables,0);
  field_names = (char ***) my_malloc(sizeof(char **) *
				     (mysql_num_rows(tables)+1),
				     MYF(MY_WME));
  if (!field_names)
    DBUG_VOID_RETURN;
  field_names[mysql_num_rows(tables)]='\0';
  i=0;
  while ((table_row=mysql_fetch_row(tables)))
  {
    if ((fields=mysql_list_fields(&mysql,(const char*) table_row[0],NullS)))
    {
      num_fields=mysql_num_fields(fields);
      field_names[i] = (char **) my_malloc(sizeof(char *)*(num_fields*2+1),
					   MYF(0));
      if (!field_names[i])
      {
	continue;
      }
      field_names[i][num_fields*2]='\0';
      j=0;
      while ((sql_field=mysql_fetch_field(fields)))
      {
	sprintf(buf,"%s.%s",table_row[0],sql_field->name);
	field_names[i][j] = my_strdup(buf,MYF(0));
	add_word(&ht,field_names[i][j]);
	field_names[i][num_fields+j] = my_strdup(sql_field->name,MYF(0));
	if (!completion_hash_exists(&ht,field_names[i][num_fields+j],
				    strlen(field_names[i][num_fields+j])))
	  add_word(&ht,field_names[i][num_fields+j]);
	j++;
      }
    }
    else
      printf("Didn't find any fields in table '%s'\n",table_row[0]);
    i++;
  }
  DBUG_VOID_RETURN;
}


	/* for gnu readline */

#ifndef HAVE_INDEX
#ifdef	__cplusplus
extern "C" {
#endif
extern char *index(const char *,pchar c),*rindex(const char *,pchar);

char *index(const char *s,pchar c)
{
  for (;;)
  {
     if (*s == (char) c) return (char*) s;
     if (!*s++) return NullS;
  }
}

char *rindex(const char *s,pchar c)
{
  reg3 char *t;

  t = NullS;
  do if (*s == (char) c) t = (char*) s; while (*s++);
  return (char*) t;
}
#ifdef	__cplusplus
}
#endif
#endif
#endif /* HAVE_READLINE */

static int reconnect(void)
{
  if (!batch)
  {
    put_info("No connection. Trying to reconnect...",INFO_INFO);
    (void) com_connect((String *) 0, 0);
  }
  if (!connected)
    return put_info("Can't connect to the server\n",INFO_ERROR);
  return 0;
}


/***************************************************************************
 The different commands
***************************************************************************/

static int
com_help (String *buffer __attribute__((unused)),
	  char *line __attribute__((unused)))
{
  reg1 int i;

  put_info("\nMySQL commands:",INFO_INFO);
  for (i = 0; commands[i].name; i++)
  {
    if (commands[i].func)
      printf("%s\t(\\%c)\t%s\n", commands[i].name,commands[i].cmd_char,
	     commands[i].doc);
  }
  if (connected)
    printf("\nConnection id: %d  (Can be used with mysqladmin kill)\n\n",
	   mysql_thread_id(&mysql));
  else
    printf("Not connected!  Reconnect with 'connect'!\n\n");
  return 0;
}


	/* ARGSUSED */
static int
com_clear(String *buffer,char *line __attribute__((unused)))
{
  buffer->length(0);
  return 0;
}


/*
** Execute command
** Returns: 0  if ok
**	    -1 if not fatal error
**	    1  if fatal error
*/


static int
com_go(String *buffer,char *line __attribute__((unused)))
{
  char		buff[160],time_buff[32];
  MYSQL_RES	*result;
  ulong		timer;
  uint		error=0;

  if (!batch)
  {
    old_buffer= *buffer;			// Save for edit command
    old_buffer.copy();
  }

	/* Remove garbage for nicer messages */
  LINT_INIT(buff[0]);
  remove_cntrl(*buffer);

  if (buffer->is_empty())
  {
    return put_info("No query specified\n",INFO_ERROR);
  }
  if (!connected && ! reconnect())
  {
    return batch ? 1 : -1;			// Fatal error
  }
  if (verbose)
    (void) com_print(buffer,0);

  if (skip_updates &&
      (buffer->length() < 4 || my_sortcmp(buffer->ptr(),"SET ",4)))
  {
    (void) put_info("Ignoring query to other database",INFO_INFO);
    return 0;
  }

  timer=start_timer();
  for (uint retry=0;; retry++)
  {
    if (!mysql_real_query(&mysql,buffer->ptr(),buffer->length()))
      break;
    error=put_info(mysql_error(&mysql),INFO_ERROR, mysql_errno(&mysql));
    if (mysql_errno(&mysql) != CR_SERVER_GONE_ERROR || retry > 1 || batch)
    {
      buffer->length(0);			// Remove query on error
      return error;
    }
    put_info("Trying to reconnect...",INFO_INFO);
    (void) com_connect((String *) 0, 0);
    if (!connected)
    {
      buffer->length(0);			// Remove query on error
      return error;
    }
  }
  error=0;
  buffer->length(0);

  if (quick)
  {
    if (!(result=mysql_use_result(&mysql)) && mysql_num_fields(&mysql))
    {
      return put_info(mysql_error(&mysql),INFO_ERROR,mysql_errno(&mysql));
    }
  }
  else
  {
    if (!(result=mysql_store_result(&mysql)))
    {
      if (mysql_error(&mysql)[0])
      {
	return put_info(mysql_error(&mysql),INFO_ERROR,mysql_errno(&mysql));
      }
    }
  }

  if (verbose >= 3 || !silent)
    mysql_end_timer(timer,time_buff);
  else
    time_buff[0]=0;
  if (result)
  {
    if (!mysql_num_rows(result) && ! quick)
    {
      sprintf(buff,"Empty set%s",time_buff);
    }
    else
    {
      if (vertical)
	print_table_data_vertically(result);
      else if (silent && verbose <= 2 && !output_tables)
	print_tab_data(result);
      else
	print_table_data(result);
      sprintf(buff,"%ld %s in set%s",
	      (long) mysql_num_rows(result),
	      (long) mysql_num_rows(result) == 1 ? "row" : "rows",
	      time_buff);
    }
  }
  else if (mysql_affected_rows(&mysql) == ~(ulonglong) 0)
    sprintf(buff,"Query OK%s",time_buff);
  else
    sprintf(buff,"Query OK, %ld %s affected%s",
	    (long) mysql_affected_rows(&mysql),
	    (long) mysql_affected_rows(&mysql) == 1 ? "row" : "rows",
	    time_buff);
  put_info(buff,INFO_RESULT);
  if (mysql_info(&mysql))
    put_info(mysql_info(&mysql),INFO_RESULT);
  put_info("",INFO_RESULT);			// Empty row

  if (result && !mysql_eof(result))	/* Something wrong when using quick */
    error=put_info(mysql_error(&mysql),INFO_ERROR,mysql_errno(&mysql));
  else if (unbuffered)
    fflush(stdout);
  mysql_free_result(result);
  return error;				/* New command follows */
}

static int
com_ego(String *buffer,char *line)
{
  int result;
  bool oldvertical=vertical;
  vertical=1;
  result=com_go(buffer,line);
  vertical=oldvertical;
  return result;
}


static void
print_table_data(MYSQL_RES *result)
{
  String separator(256);
  MYSQL_ROW	cur;
  uint		length;
  MYSQL_FIELD	*field;
  bool		*num_flag;

  num_flag=(bool*) my_alloca(sizeof(bool)*mysql_num_fields(result));
  separator.copy("+",1);
  while((field = mysql_fetch_field(result)))
  {
    uint length=strlen(field->name);
    if (quick)
      length=max(length,field->length);
    else
      length=max(length,field->max_length);
    if (length < 4 && !IS_NOT_NULL(field->flags))
      length=4;					// Room for "NULL"
    field->max_length=length+1;
    separator.fill(separator.length()+length+2,'-');
    separator.append('+');
  }
  puts(separator.c_ptr());

  mysql_field_seek(result,0);
  (void) fputs("|",stdout);
  for (uint off=0; (field = mysql_fetch_field(result)) ; off++)
  {
    printf(" %-*s|",field->max_length,field->name);
    num_flag[off]= IS_NUM(field->type);
  }
  (void) fputc('\n',stdout);
  puts(separator.c_ptr());

  while ((cur = mysql_fetch_row(result)))
  {
    (void) fputs("|",stdout);
    mysql_field_seek(result,0);
    for (uint off=0 ; off < mysql_num_fields(result); off++)
    {
      field = mysql_fetch_field(result);
      length=field->max_length;
      printf(num_flag[off] ? "%*s |" : " %-*s|",
	     length,cur[off] ? (char*) cur[off] : "NULL");
    }
    (void) fputc('\n',stdout);
  }
  puts(separator.c_ptr());
  my_afree((gptr) num_flag);
}



static void
print_table_data_vertically(MYSQL_RES *result)
{
  MYSQL_ROW	cur;
  uint		max_length=0;
  MYSQL_FIELD	*field;

  while (field = mysql_fetch_field(result))
  {
    uint length=strlen(field->name);
    if (length > max_length)
      max_length= length;
    field->max_length=length;
  }

  mysql_field_seek(result,0);
  for (uint row_count=1; (cur= mysql_fetch_row(result)); row_count++)
  {
    mysql_field_seek(result,0);
    printf("*************************** %d. row ***************************\n",
	   row_count);
    for (uint off=0; off < mysql_num_fields(result); off++)
    {
      field= mysql_fetch_field(result);
      printf("%+*s: ",max_length,field->name);
      printf("%s\n",cur[off] ? (char*) cur[off] : "NULL");
    }
  }
}


static void
safe_put_field(const char *pos,uint length)
{
  if (!pos)
    fputs("NULL",stdout);
  else
  {
    if (opt_raw_data)
      fputs(pos,stdout);
    else for (const char *end=pos+length ; pos != end ; pos++)
    {
#ifdef USE_BIG5CODE
      if (pos+1 != end	&& isbig5code(*pos,*(pos+1)))
      {
	putchar(*pos++);
	putchar(*pos);
	continue;
      }
      else
#endif
#ifdef USE_MB
      int l;
      if ((l = ismbchar(pos, end))) {
	  while (l--)
	      putchar(*pos++);
	  pos--;
	  continue;
      }
#endif
      if (!*pos)
	fputs("\\0",stdout);		// This makes everything hard
      else if (*pos == '\t')
	fputs("\\t",stdout);		// This would destroy tab format
      else if (*pos == '\n')
	fputs("\\n",stdout);		// This too
      else if (*pos == '\\')
	fputs("\\\\",stdout);
      else
	putchar(*pos);
    }
  }
}


static void
print_tab_data(MYSQL_RES *result)
{
  MYSQL_ROW	cur;
  MYSQL_FIELD	*field;
  ulong		*lengths;

  if (silent < 2)
  {
    int first=0;
    while ((field = mysql_fetch_field(result)))
    {
      if (first++)
	(void) fputc('\t',stdout);
      (void) fputs(field->name,stdout);
    }
    (void) fputc('\n',stdout);
  }
  while ((cur = mysql_fetch_row(result)))
  {
    lengths=mysql_fetch_lengths(result);
    safe_put_field(cur[0],lengths[0]);
    for (uint off=1 ; off < mysql_num_fields(result); off++)
    {
      (void) fputc('\t',stdout);
      safe_put_field(cur[off],lengths[off]);
    }
    (void) fputc('\n',stdout);
  }
}


static int
com_edit(String *buffer,char *line __attribute__((unused)))
{
#ifdef __WIN32__
  put_info("Sorry, you can't send the result to an editor in Win32",
	   INFO_ERROR);
#else
  char	*filename,buff[160];
  int	fd,tmp;
  const char *editor;

  filename = my_tempnam(NullS,"sql",MYF(MY_WME));
  if ((fd = my_create(filename,0,O_CREAT | O_WRONLY, MYF(MY_WME))) < 0)
    goto err;
  if (buffer->is_empty() && !old_buffer.is_empty())
    (void) my_write(fd,(byte*) old_buffer.ptr(),old_buffer.length(),
		    MYF(MY_WME));
  else
    (void) my_write(fd,(byte*) buffer->ptr(),buffer->length(),MYF(MY_WME));
  (void) my_close(fd,MYF(0));

  if (!(editor = (char *)getenv("EDITOR")) &&
      !(editor = (char *)getenv("VISUAL")))
    editor = "vi";
  strxmov(buff,editor," ",filename,NullS);
  (void) system(buff);

  MY_STAT stat;
  if (!my_stat(filename,&stat,MYF(MY_WME)))
    goto err;
  if ((fd = my_open(filename,O_RDONLY, MYF(MY_WME))) < 0)
    goto err;
  (void) buffer->alloc((uint) stat.st_size);
  if ((tmp=read(fd,(char*) buffer->ptr(),buffer->alloced_length())) >= 0L)
    buffer->length((uint) tmp);
  else
    buffer->length(0);
  (void) my_close(fd,MYF(0));
  (void) my_delete(filename,MYF(MY_WME));
err:
  free(filename);
#endif
  return 0;
}

/* If arg is given, exit without errors. This happens on command 'quit' */

static int
com_quit(String *buffer __attribute__((unused)),
	 char *line __attribute__((unused)))
{
  exit_status=0;
  return 1;
}

static int
com_rehash(String *buffer __attribute__((unused)),
	 char *line __attribute__((unused)))
{
#ifdef HAVE_READLINE
  build_completion_hash(0,0);
#endif
  return 0;
}

static int
com_print(String *buffer,char *line __attribute__((unused)))
{
  puts("--------------");
  (void) fputs(buffer->c_ptr(),stdout);
  if (!buffer->length() || (*buffer)[buffer->length()-1] != '\n')
    putchar('\n');
  puts("--------------\n");
  return 0;					/* If empty buffer */
}

	/* ARGSUSED */
static int
com_connect(String *buffer __attribute__((unused)),
	    char *line)
{
  char *tmp,buff[80];
  bool save_rehash=no_rehash;
  int error;

  if (buffer)
  {
    while (isspace(*line))
      line++;
    tmp=(char *) strtok(line," \t");	// Skipp connect command
    if (tmp && (tmp=(char *) strtok(NullS," \t")))
    {
      my_free(current_db,MYF(MY_ALLOW_ZERO_PTR));
      current_db=my_strdup(tmp,MYF(MY_WME));
      if ((tmp=(char *) strtok(NullS," \t")))
      {
	my_free(current_host,MYF(0));
	current_host=my_strdup(tmp,MYF(MY_WME));
      }
    }
    else
      no_rehash=1;				// Quick re-connect
  }
  else
    no_rehash=1;
  error=sql_connect(current_host,current_db,current_user,password,0);
  no_rehash=save_rehash;

  if (connected)
  {
    sprintf(buff,"Connection id:    %d",mysql_thread_id(&mysql));
    put_info(buff,INFO_INFO);
    sprintf(buff,"Current database: %s\n",
	    current_db ? current_db : "*** NO ONE ***");
    put_info(buff,INFO_INFO);
  }
  return error;
}

	/* ARGSUSED */
static int
com_use(String *buffer __attribute__((unused)), char *line)
{
  char *tmp;
  while (isspace(*line))
    line++;
  tmp=(char *) strtok(line," \t");		// Skipp connect command
  if (!tmp || !(tmp=(char *) strtok(NullS," \t")))
  {
    put_info("USE must be followed by a database name",INFO_ERROR);
    return 0;
  }
  if (!current_db || strcmp(current_db,tmp))
  {
    if (one_database)
      skip_updates=1;
    else
    {
      /*
	reconnect once if connection is down or if connection was found to
	be down during query
      */
      if (!connected && reconnect())
	return batch ? 1 : -1;			// Fatal error
      if (mysql_select_db(&mysql,tmp) < 0)
      {
	if (mysql_errno(&mysql) != CR_SERVER_GONE_ERROR)
	  return put_info(mysql_error(&mysql),INFO_ERROR,mysql_errno(&mysql));

	if (reconnect())
	  return batch ? 1 : -1;			// Fatal error
	if (mysql_select_db(&mysql,tmp) < 0)
	  return put_info(mysql_error(&mysql),INFO_ERROR,mysql_errno(&mysql));
      }
#ifdef HAVE_READLINE
      build_completion_hash(no_rehash,1);
#endif
      my_free(current_db,MYF(MY_ALLOW_ZERO_PTR));
      current_db=my_strdup(tmp,MYF(MY_WME));
    }
  }
  else
    skip_updates=0;
  put_info("Database changed",INFO_INFO);
  return 0;
}


static int
sql_real_connect(char *host,char *database,char *user,char *password,
		 uint silent)
{
  if (connected)
  {					/* if old is open, close it first */
    mysql_close(&mysql);
    connected= 0;
  }
  mysql_init(&mysql);
  if (opt_compress)
    mysql_options(&mysql,MYSQL_OPT_COMPRESS,NullS);

  if (!mysql_real_connect(&mysql,host,user,password,
			  database,opt_mysql_port,opt_mysql_unix_port,
			  0))
  {
    if (!silent ||
	(mysql_errno(&mysql) != CR_CONN_HOST_ERROR &&
	 mysql_errno(&mysql) != CR_CONNECTION_ERROR))
    {
      put_info(mysql_error(&mysql),INFO_ERROR,mysql_errno(&mysql));
      (void) fflush(stdout);
      return ignore_errors ? -1 : 1;		// Abort
    }
    return -1;					// Retryable
  }
  connected=1;
  mysql.reconnect=info_flag ? 1 : 0; // We want to know if this happens
#ifdef HAVE_READLINE
  build_completion_hash(no_rehash,1);
#endif
  return 0;
}


static int
sql_connect(char *host,char *database,char *user,char *password,uint silent)
{
  bool message=0;
  uint count=0;
  int error;
  for (;;)
  {
    if ((error=sql_real_connect(host,database,user,password,wait_flag)) >= 0)
    {
      if (count)
      {
	fputs("\n",stderr);
	(void) fflush(stderr);
      }
      return error;
    }
    if (!wait_flag)
      return ignore_errors ? -1 : 1;
    if (!message && !silent)
    {
      message=1;
      fputs("Waiting",stderr); (void) fflush(stderr);
    }
    (void) sleep(5);
    if (!silent)
    {
      putc('.',stderr); (void) fflush(stderr);
      count++;
    }
  }
}



static int
com_status(String *buffer __attribute__((unused)),
	   char *line __attribute__((unused)))
{
  char *stat;
  puts("--------------");
  usage(1);					/* Print version */
  if (connected)
  {
    MYSQL_RES *result;
    LINT_INIT(result);
    printf("\nConnection id:\t\t%d\n",mysql_thread_id(&mysql));
    if (!mysql_query(&mysql,"select DATABASE(),USER()") &&
	(result=mysql_use_result(&mysql)))
    {
      MYSQL_ROW cur=mysql_fetch_row(result);
      printf("Current database:\t%s\n",cur[0]);
      printf("Current user:\t\t%s\n",cur[1]);
      (void) mysql_fetch_row(result);		// Read eof
    }
  }
  else
  {
    vidattr(A_BOLD);
    printf("\nNo connection\n");
    vidattr(A_NORMAL);
    return 0;
  }
  if (skip_updates)
  {
    vidattr(A_BOLD);
    printf("\nAll updates ignored to this database\n");
    vidattr(A_NORMAL);
  }
  printf("Server version\t\t%s\n", mysql_get_server_info(&mysql));
  printf("Protocol version\t%d\n", mysql_get_proto_info(&mysql));
  printf("Connection\t\t%s\n",	   mysql_get_host_info(&mysql));
  if (strstr(mysql_get_host_info(&mysql),"TCP/IP"))
    printf("TCP port\t\t%d\n",	   mysql.port);
  else
    printf("UNIX socket\t\t%s\n",  mysql.unix_socket);
  if ((stat=mysql_stat(&mysql)) && !mysql_error(&mysql)[0])
  {
    char *pos,buff[40];
    ulong sec;
    pos=strchr(stat,' ');
    *pos++=0;
    printf("%s\t\t\t",stat);			/* print label */
    if ((stat=str2int(pos,10,0,LONG_MAX,(long*) &sec)))
    {
      nice_time((double) sec,buff,0);
      puts(buff);				/* print nice time */
      while (*stat == ' ') stat++;		/* to next info */
    }
    if (stat)
    {
      putchar('\n');
      puts(stat);
    }
  }
  puts("--------------\n");
  return 0;
}


static int
put_info(const char *str,INFO_TYPE info_type,uint error)
{
  static int inited=0;

  if (batch)
  {
    if (info_type == INFO_ERROR)
    {
      (void) fflush(stdout);
      fprintf(stderr,"ERROR ");
      if (error)
	(void) fprintf(stderr,"%d",error);
      if (query_start_line && ! skip_line_numbers)
	(void) fprintf(stderr," at line %lu",query_start_line);
      (void) fprintf(stderr,": %s\n",str);
      (void) fflush(stderr);
      if (!ignore_errors)
	return 1;
    }
    else if (info_type == INFO_RESULT && verbose > 1)
      puts(str);
    if (unbuffered)
      fflush(stdout);
    return info_type == INFO_ERROR ? -1 : 0;
  }
  if (!silent || info_type == INFO_ERROR)
  {
    if (!inited)
    {
      inited=1;
#ifdef HAVE_SETUPTERM
      (void) setupterm((char *)0, 1, (int *) 0);
#endif
    }
    if (info_type == INFO_ERROR)
    {
      putchar('\007');				/* This should make a bell */
      vidattr(A_STANDOUT);
      if (error)
	(void) fprintf(stderr,"ERROR %d: ",error);
      else
	fputs("ERROR: ",stdout);
    }
    else
      vidattr(A_BOLD);
    (void) puts(str);
    vidattr(A_NORMAL);
  }
  if (unbuffered)
    fflush(stdout);
  return info_type == INFO_ERROR ? -1 : 0;
}

static void remove_cntrl(String &buffer)
{
  char *start,*end;
  end=(start=(char*) buffer.ptr())+buffer.length();
  while (start < end && !isgraph(end[-1]))
    end--;
  buffer.length((uint) (end-start));
}


#ifdef __WIN32__
#include <time.h>
#else
#include <sys/times.h>
#undef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC (sysconf(_SC_CLK_TCK))
#endif

static ulong start_timer(void)
{
#ifdef __WIN32__
 return clock();
#else
  struct tms tms;
  return times(&tms);
#endif
}


static void nice_time(double sec,char *buff,bool part_second)
{
  ulong tmp;
  if (sec >= 3600.0*24)
  {
    tmp=(ulong) floor(sec/(3600.0*24));
    sec-=3600.0*24*tmp;
    buff=int2str((long) tmp,buff,10);
    buff=strmov(buff,tmp > 1 ? " days " : " day ");
  }
  if (sec >= 3600.0)
  {
    tmp=(ulong) floor(sec/3600.0);
    sec-=3600.0*tmp;
    buff=int2str((long) tmp,buff,10);
    buff=strmov(buff,tmp > 1 ? " hours " : " hour ");
  }
  if (sec >= 60.0)
  {
    tmp=(ulong) floor(sec/60.0);
    sec-=60.0*tmp;
    buff=int2str((long) tmp,buff,10);
    buff=strmov(buff," min ");
  }
  if (part_second)
    sprintf(buff,"%.2f sec",sec);
  else
    sprintf(buff,"%d sec",(int) sec);
}


static void end_timer(ulong start_time,char *buff)
{
  nice_time((double) (start_timer() - start_time) /
	    CLOCKS_PER_SEC,buff,1);
}


static void mysql_end_timer(ulong start_time,char *buff)
{
  buff[0]=' ';
  buff[1]='(';
  end_timer(start_time,buff+2);
  strmov(strend(buff),")");
}

/* Keep sql_string library happy */

gptr sql_alloc(unsigned int Size)
{
  return my_malloc(Size,MYF(MY_WME));
}

void sql_element_free(void *ptr)
{
  my_free((gptr) ptr,MYF(0));
}
