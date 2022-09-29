#include "common.h"
#include "semapi.h"
#include "events.h"

/* 
 * Some global variables.
 */
#define INFINITE_LOOP 0
/* 
 * Signal handler for SIGINT, SIGQUIT...
 */
int verbose=0;
void signal_handler(int i)
{
  semapiDone();
  exit(-1);
}

main(int argc, char **argv)
{
  int c=0;
  unsigned length=0,count=0,send=0;
  int ev_class=0,ev_type=0;
  struct sigaction sigact;
  char *ev_data=NULL;
  char hostname[256];

  strcpy(hostname, "localhost");
  while ( -1 != (c = getopt(argc,argv,"H:C:T:D:N:")))
    {
      switch (c)
	{
	case 'H':
	  strcpy(hostname, optarg);
          break;
	case 'C':
	  ev_class = atoi(optarg);
	  break;
	case 'T':
	  ev_type = atoi(optarg);
	  break;
	case 'N':
	  count = atoi(optarg);
	  break;
	case 'D':
	  ev_data = optarg;
	  break;
	default: /* unknown or missing argument */
	  fprintf(stderr,
		  "Usage: %s [-C event class] \n"
		  "          [-T event type]  \n"
		  "          [-H hostname  ]  \n"
		  "          [-N event count] \n"
		  "          [-D event data]  \n",argv[0]);
	  return -1;
	} /* switch */
    } /* while */

  init_mempool();

  /* 
   * Create  the communication streams.
   */
  if(semapiInit(SEMAPI_VERMAJOR,SEMAPI_VERMINOR) != SEMERR_SUCCESS)
    {
      printf("Can't initialize SEM API\n");
      exit(1);
    }
  length=strlen(ev_data);
  /* 
   * Let's just assume a count of 1 if user does not specify count explicitly.
   */
  if(!count)
    count = 1;

#if INFINITE_LOOP
  while (1)
    {
#endif
      sigemptyset(&sigact.sa_mask);
      sigact.sa_flags    = SA_RESTART;
      sigact.sa_handler=signal_handler;
      sigaction(SIGINT,&sigact,NULL);
      sigaction(SIGQUIT,&sigact,NULL);
      sigaction(SIGHUP,&sigact,NULL);
      sigaction(SIGABRT,&sigact,NULL);

      sigemptyset(&sigact.sa_mask);
      sigact.sa_flags    = SA_RESTART;
      sigact.sa_handler=SIG_IGN;
      sigaction(SIGPIPE,&sigact,NULL);
#if PCP_TRACE
      pmtracebegin("event");
#endif
      send = semapiDeliverEvent(hostname,time(NULL),ev_class,ev_type,0,
				count,length,ev_data);
      if(send == SEMERR_SUCCESS)
	printf("Success      :%d\n",length);
      else if(send == SEMERR_FATALERROR) 
	printf("Fatal Error  :%d\n",length);
      else if(send == SEMERR_ERROR)      
	printf("Error        :%d\n",length);
      
#if PCP_TRACE
      pmtraceend("event");
#endif
#if INFINITE_LOOP
    }
#endif
  semapiDone();
}

