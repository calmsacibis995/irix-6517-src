/*============================================================================*/
/*                               ssLogger.c                                   */
/*============================================================================*/
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>
#include <syslog.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <eventmonapi.h>

/* sslogger predefined parameters */
#define SSLOGGER_MAJOR_VNUM	1
#define SSLOGGER_MINOR_VNUM	0
#define MAXFILELENGTH		1024
#define MSG_BUFSIZE             (1024*128)

/* Possible error codes returned by sslogger */
#define SSLOGGER_SUCCESS	    (0)   /* sucessfull completion */
#define SSLOGGER_UNAUTHORIZEDUSER   (1)   /* unauthorized user */
#define SSLOGGER_INVALIDPARAM       (2)   /* one or more input parameter(s) are not valid, or missing */
#define SSLOGGER_EVMONAPIERR	    (3)   /* error returned by eventmon */
#define SSLOGGER_EVMONAPIOFF	    (4)   /* EventMon API is not installed or not running */

/* sslogger data structure */
typedef struct data_table {
        int           EventSeqNo;
        int           Priority;
        long          EventTime;
        char         *EventHost;
        int	      EventDataLength;
        char         *EventData;
} data_s;

static const char szAppName[] = "esplogger";  /* application name */
static char log_ssdb_msg[MSG_BUFSIZE];       /* default message string with data */
static data_s sdata;

/*============================================================================*/
/* function to initialize global variables				      */
/*============================================================================*/
static void init_var()
{ memset(&sdata,0,sizeof(sdata));
  memset(log_ssdb_msg,0,sizeof(log_ssdb_msg));
}

/*============================================================================*/
/* check if event sequence parameter is valid (indeed an integer or hex)      */
/*============================================================================*/
static int check_seq_param(const char *param_str, int *val)
{ int i,j;
  if(param_str && param_str[0] && (i = strlen(param_str)) < 16)
  { if(param_str[0] == '0' && param_str[1] == 'x')
    { for(j = 2;j < i;j++) if(!isxdigit(param_str[j])) break;
      if((j == i) && (i > 2) && (sscanf(&param_str[2],"%x",val) == 1) && (*val > 0)) return SSLOGGER_SUCCESS;
    }
    else
    { for(j = 0;j < i;j++) if(!isdigit(param_str[j])) break;
      if((j == i) && (sscanf(&param_str[0],"%d",val) == 1) && (*val > 0)) return SSLOGGER_SUCCESS;
    }
  }
  fprintf(stderr,"%s - Invalid parameter.\n",szAppName);
  return SSLOGGER_INVALIDPARAM;
}

/*============================================================================*/
/* get priority                                                               */
/*============================================================================*/
static void get_priority(const char *pri, const char *fac)
{ typedef struct _code {
        char    *c_myname;
	int      c_myval;
  } MYCODE;
  MYCODE myprioritynames[] = {
        "alert",        LOG_ALERT,
        "crit",         LOG_CRIT,
        "debug",        LOG_DEBUG,
        "emerg",        LOG_EMERG,
        "err",          LOG_ERR,
        "error",        LOG_ERR,                /* DEPRECATED */
        "info",         LOG_INFO,
        "notice",       LOG_NOTICE,
        "panic",        LOG_EMERG,              /* DEPRECATED */
        "warn",         LOG_WARNING,            /* DEPRECATED */
        "warning",      LOG_WARNING,
        NULL,           -1,
  };
  MYCODE myfacilitynames[] = {
        "audit",        LOG_AUDIT,
        "auth",         LOG_AUTH,
        "cron",         LOG_CRON,
        "daemon",       LOG_DAEMON,
        "kern",         LOG_KERN,
        "lpr",          LOG_LPR,
        "mail",         LOG_MAIL,
        "news",         LOG_NEWS,
        "sat",          LOG_AUDIT,
        "security",     LOG_AUTH,               /* DEPRECATED */
        "syslog",       LOG_SYSLOG,
        "user",         LOG_USER,
        "uucp",         LOG_UUCP,
        "local0",       LOG_LOCAL0,
        "local1",       LOG_LOCAL1,
        "local2",       LOG_LOCAL2,
        "local3",       LOG_LOCAL3,
        "local4",       LOG_LOCAL4,
        "local5",       LOG_LOCAL5,
        "local6",       LOG_LOCAL6,
        "local7",       LOG_LOCAL7,
        NULL,           -1,
  };
  int i = 0, priority = -1, facility = -1;
  MYCODE *ptrCode;
  if(!pri || !pri[0]) pri = "info";
  if(!fac || !fac[0] || fac == NULL) fac = "local0";
  for(ptrCode = myprioritynames;ptrCode->c_myname;ptrCode++)
    if(!strcmp(ptrCode->c_myname,pri)) { priority = ptrCode->c_myval; break; }
  for(ptrCode = myfacilitynames;ptrCode->c_myname;ptrCode++)
    if(!strcmp(ptrCode->c_myname,fac)) { facility = ptrCode->c_myval; break; }
  sdata.Priority = LOG_MAKEPRI(facility, priority);
}
  
/*============================================================================*/
/* verify if filename is valid (not greater then 1024)                        */
/*============================================================================*/
static void verify_filename(char *name_to_verify)
{ int i = 0;
  if(!name_to_verify || ((i = strlen(name_to_verify)) >= MAXFILELENGTH) || !i || !strcmp(name_to_verify,".") || !strcmp(name_to_verify,".."))
  { if(i) fprintf (stderr,"%s - Invalid filename - \"%s\"\n",szAppName,name_to_verify);
    else  fprintf (stderr,"%s - Invalid filename\n",szAppName);
    exit(SSLOGGER_INVALIDPARAM);
  }
}

/*============================================================================*/
/* function to check command line syntax                                      */
/*============================================================================*/
static void check_command_line_flag()
{ if(!sdata.EventSeqNo)
  { fprintf (stderr,"%s - Sequence number is not declared\n",szAppName);
    exit(SSLOGGER_INVALIDPARAM);
  }
  if(!sdata.EventDataLength)
  { fprintf(stderr,"%s - No data to log\n",szAppName);
    exit(SSLOGGER_INVALIDPARAM);
  }
}

/*============================================================================*/
/* check if specified data file exists and not zero length                    */
/*============================================================================*/
static void check_data_file_exist(char *datafile, char *filename)
{ struct stat buf;
  if(stat(datafile,&buf))
  { fprintf(stderr,"%s - Specified data file \"%s\" does not exist\n",szAppName,datafile);
    fprintf(stderr,"           Cannot continue...\n");
    exit(SSLOGGER_INVALIDPARAM);
  }
  if(!S_ISREG(buf.st_mode)) exit(SSLOGGER_INVALIDPARAM);
  if(!buf.st_size)
  { fprintf(stderr,"%s - Specified data file \"%s\" has zero length\n",szAppName,datafile);
    fprintf(stderr,"           Cannot continue...\n");
    exit(SSLOGGER_INVALIDPARAM);
  }
  if(buf.st_size >= EVMONAPI_MAXEVENTSIZE)
  { fprintf(stderr,"%s - Lenght of the specified data file \"%s\" is too big\n",szAppName,datafile);
    fprintf(stderr,"           Cannot continue...\n");
    exit(SSLOGGER_INVALIDPARAM);
  }
  strcpy(filename,datafile);
}

/*============================================================================*/
/* read data from the specified data file                                     */
/*============================================================================*/
static int read_data_file_contents(char *filename)
{ FILE *fp;
  int i,c;
  if((fp = fopen(filename, "r")) != NULL)
  { for(i = 0;i < sizeof(log_ssdb_msg)-1;i++)
    { if((c = fgetc(fp)) == EOF) break;
      log_ssdb_msg[i] = (char)c;
    }
    log_ssdb_msg[i] = 0;
    sdata.EventData = log_ssdb_msg;
    sdata.EventDataLength = i;
    fclose (fp);
    if(sdata.EventDataLength != strlen(sdata.EventData))
    { fprintf(stderr,"%s - Check data file \"%s\"\n",szAppName,filename);
      return SSLOGGER_INVALIDPARAM;
    }
    return SSLOGGER_SUCCESS;
  }
  fprintf(stderr,"%s - Cannot open specified with -f option file \"%s\"\n",szAppName,filename);
  return SSLOGGER_INVALIDPARAM;
}

/*============================================================================*/
/* fill output buffer                                                         */
/*============================================================================*/
static void make_data(char *buffer)
{ int i;
  if(!buffer || buffer[0] == 0 || (i = strlen(buffer)) == 0 ||
     i >= sizeof(log_ssdb_msg) || i >= EVMONAPI_MAXEVENTSIZE)
  { fprintf(stderr,"%s - Specified message has incorrect length\n",szAppName);
    fprintf(stderr,"           Cannot continue...\n");
    exit(SSLOGGER_INVALIDPARAM);
  }
  strcpy(log_ssdb_msg,buffer);
  log_ssdb_msg[i] = 0;
  sdata.EventData = log_ssdb_msg;
  sdata.EventDataLength = i;
}

/*============================================================================*/
/* function to determine time of event. Use either time provided by tool,     */
/* or generate current time in seconds since 00:00:00 UTC, Jan 1, 1970        */
/*============================================================================*/
static void check_event_time()
{ if(!sdata.EventTime)
    if((sdata.EventTime = time(NULL)) == (time_t)-1) sdata.EventTime = 0;
}

/*============================================================================*/
/* function to print usage of sslogger				              */
/*============================================================================*/
static void print_usage(int retcode)
{ printf ("\n%s - Usage:\n%s [-hV] [-s sequence number -f filename | -m message]\n",szAppName,szAppName);
  printf ("         [-t time] [-p priority] [-n hostname]\n\n");
  printf ("   -h                   print help and exit\n");
  printf ("   -V                   print %s version number and exit\n",szAppName);
  printf ("   -s sequence number   set the sequence number (required to login data)\n");
  printf ("   -f filename          get data from file\n");
  printf ("   -m \"message\"         use message string as a data\n");
  printf ("   -t time              time of event given in seconds since\n");
  printf ("                        00:00:00 UTC, Jan 1, 1970 (in decimal notation)\n");
  printf ("   -p priority          set priority for a particular message\n");
  printf ("   -n hostname          get hostname\n");
  printf ("\n");
  exit(retcode);
}

/*============================================================================*/
/* function to parse command line arguments                                   */
/*============================================================================*/
static void parse_input_args(int argc, char *argv[], char *recd_pri)
{ int c, err_code;
  char host_rcvd[1024];
  char filename[MAXFILELENGTH];
  memset(filename,0,sizeof(filename));
  if(argc < 2) print_usage(SSLOGGER_INVALIDPARAM);
  while((c = getopt(argc,argv,"s:f:m:t:p:n:hV")) != (-1))
  { switch (c) {
    case 's': if((err_code = (check_seq_param(optarg,&sdata.EventSeqNo))) == SSLOGGER_SUCCESS) break;
              exit(err_code);
    case 'p': strcpy(recd_pri,optarg);
              break;
    case 'V': printf("\nesplogger version %d.%d\n\n",SSLOGGER_MAJOR_VNUM,SSLOGGER_MINOR_VNUM);
              exit(SSLOGGER_SUCCESS);
    case 'h': print_usage(SSLOGGER_SUCCESS);
              exit(SSLOGGER_SUCCESS);
    case 'f': verify_filename(optarg);
              check_data_file_exist(optarg,filename);
              if((err_code = read_data_file_contents(filename)) != SSLOGGER_SUCCESS) exit(err_code);
              break;
    case 'm': make_data(optarg);
              break;
    case 't': sdata.EventTime = (optarg && optarg[0]) ? atol(optarg) : 0;
              break;
    case 'n': strcpy(host_rcvd,optarg);
              sdata.EventHost = host_rcvd;
              break;
    default:  print_usage(SSLOGGER_INVALIDPARAM);
    };
  }
}

/*============================================================================*/
/* check user's authorization                                                 */
/*============================================================================*/
static void check_permissions()
{ if(getuid() || geteuid())
  { fprintf(stderr,"\n%s - Unauthorized user!\n\n",szAppName);
    exit(SSLOGGER_UNAUTHORIZEDUSER);
  }
}

/*============================================================================*/
int main (int argc, char *argv[])
{ int status;
  char recd_pri[1024];
  char *pri, *fac, *ch;
  signal(SIGTERM,SIG_IGN);
  signal(SIGTTIN,SIG_IGN);
  signal(SIGTTOU,SIG_IGN);
  signal(SIGTSTP,SIG_IGN);
  signal(SIGINT,SIG_IGN);
  signal(SIGQUIT,SIG_IGN);
  check_permissions();
  init_var();
  parse_input_args(argc,argv,recd_pri);
  if(!(ch = strchr(recd_pri,'.'))) pri = recd_pri;
  else { *ch = 0; fac = recd_pri; pri = ch ? ++ch : 0; }
  get_priority(pri,fac);
  if(sdata.Priority < 0)
  { fprintf(stderr,"%s - Invalid priority\n",szAppName);
    exit(SSLOGGER_INVALIDPARAM);
  }
  check_command_line_flag();
  check_event_time();
  status = emapiIsDaemonInstalled();
  if(!status)
  { fprintf(stderr,"%s - EventMon API is not installed\n",szAppName);
    return SSLOGGER_EVMONAPIOFF;
  }
  status = emapiIsDaemonStarted();
  if(!status)
  { fprintf(stderr,"%s - EventMon API is not running\n",szAppName);
    return SSLOGGER_EVMONAPIOFF;
  }
  status = emapiSendEvent(sdata.EventHost,sdata.EventTime,sdata.EventSeqNo,sdata.Priority,sdata.EventData);
  if(status) return SSLOGGER_SUCCESS;
  else
  {  fprintf(stderr,"%s - EventMon API failure\n",szAppName);
     return SSLOGGER_EVMONAPIERR;
  }
}
