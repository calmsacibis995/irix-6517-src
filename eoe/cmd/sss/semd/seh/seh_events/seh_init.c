#include "common.h"
#include "events.h"
#include "events_private.h"
#include "seh.h"
#include "seh_errors.h"
#include "msglib.h"
#include "seh_archive.h"
#include "ssdbapi.h"

extern int verbose;
extern int lockfd;

static int daemon=1;
void dsm_events_child_handler();

/* 
 * Return fatal error if any error in the init domain.
 */
static int seh_init_check_fatal_error(__uint64_t err)
{
  if(SEH_ERROR_MAJOR(err) != SEH_MAJ_INIT_ERR)
    {
      fprintf(stderr,"Assert failed in seh_init_check_fatal_error = 0x%llx\n",
	      err);
      return 1;
    }
  
  return (err & (SEH_MIN_ALLOC_MEM |SEH_MIN_INIT_API|
		 SEH_MIN_INIT_DB   |SEH_MIN_INIT_DSM|
		 SEH_MIN_INIT_EVNUM|SEH_MIN_INIT_EVENT)) 
    ? 1 : 0;
}


static void seh_usage(char *pname)
{
  fprintf(stderr,"Usage:\n"
	  "\t%s [-v verbose_level] [-h] [-d]\n\n"
	  "\t\t-v verbose_level                 : 1,2,4,8.\n"
	  "\t\t-h                               : for this help message.\n"
	  "\t\t-d                               : do not become a daemon.\n"
	  "\t\t-A                               : send \"START ARCHIVE\" message.\n"
	  "\t\t-U                               : send \"END ARCHIVE\" message.\n"
	  "\t\t-S                               : query and retrieve status.\n"
	  "\t\t-E \"UPDATE..\"|\"NEW..\"|\"DELETE..\" : configure events.\n"
	  "\t\t-R \"UPDATE..\"|\"NEW..\"|\"DELETE..\" : configure rules.\n"
	  "\t\t-G \"THROTTLE LOG ACTION\"             : set global flags (numbers).\n"
	  "\t\t-M \"SUBSCRIBE..\"|\"UNSUBSCRIBE..\"|\"MODE..\"\n"
	  "\t\t                                 : sgm specific operations.\n",
	  pname);
  return;
}


/*
 * Function does the things necessary to become a daemon.
 */
static __uint64_t seh_become_daemon()
{
  pid_t pid;

  if(!daemon)
    return 0;

#if 0
  if((pid=fork()) < 0)
    return -1;
  else if (pid != 0)
    exit(0);			/* exit from parent */
  
  setsid();
  if((pid=fork()) < 0)
    return -1;
  else if (pid != 0)
    exit(0);			/* exit from parent */
#else
  if(verbose > 0)
    _daemonize(0,0,1,2);	/* do not close stdin,stdout & stderr if  */
  else				/* verbose flag is set, else close them.  */
    _daemonize(0,-1,-1,-1);
#endif
  
  chdir(SSS_WORKING_DIRECTORY);
  umask(0);

  return 0;
}

/*
 * Setup the interfaces for the SEH. We don't care what they are -- message
 * queue's or shared memory or whatever.
 */
static __uint64_t seh_setup_interfaces()
{
  __uint64_t err=0;

  INITQ(2);			/* the fifth interface to the DSM is out */

  /* 
   * Initialize ssdb api.
   * Never call ssdbDone. So call atexit with ssdbDone.
   */
  ssdbInit();
  atexit((void (*)(void))ssdbDone);

  if(err=seh_api_setup_interfaces())      /* Init the API  */
    return err;
  if(err=seh_archive_setup_interfaces(1)) /* Init the archive port. */
    return err;
  if(err=seh_setup_interfaces_ssdb())     /* Init the SSDB */
    return err;
  if(err=seh_dsm_setup_interfaces())      /* Init the DSM */
    return err;
  return 0;
}

/*
 * Setup our memory for events, event counters, event types etc.
 * Read anything necessary from the SSDB to do the setup.
 */
static __uint64_t seh_setup_events(struct event_private *Pev)
{
  __uint64_t err=0;
  int num_events;

  memset(Pev,0,sizeof(struct event_private));

  /*
   * Call to do initialization for all events.
   */
  err=seh_init_all_events();

  return err;
}

/*
 * Externally visible function. Calls the init for each of these 
 * sub-systems --
 *   o seh generic stuff.
 *   o seh interfaces.
 *   o seh events.
 */     
__uint64_t seh_initialize(struct event_private *Pev)
{
  __uint64_t err=0;
  sigset_t sigset;

  sigemptyset(&sigset);
  sigaddset(&sigset,SIGINT);
  sigaddset(&sigset,SIGHUP);
  sigprocmask(SIG_BLOCK,&sigset,NULL);

  sem_mem_alloc_init();		/* init the memory pool. */
  stringtab_init();		/* init the string table. */
    
  /* 
   * become a daemon.
   */
  if((err=seh_become_daemon()) && seh_init_check_fatal_error(err))
    {
      seh_syslog_event(NULL,"SEH_INIT: Failed to become a daemon "
		       ":0x%llx.\n",err);
#if EVENTS_DEBUG
      error_parse(err);
#endif
      goto LOCAL_ERROR;
    }

  /* 
   *  lock the file only after we become a daemon.
   */
  
  LOCK_FILE();			

  /* 
   * Init all the interfaces coming in to the SEH.
   */
  if((err=seh_setup_interfaces()) && seh_init_check_fatal_error(err))
    {
      seh_syslog_event(NULL,"SEH_INIT: Failed to setup interfaces "
		       ":0x%llx.\n",err);
#if EVENTS_DEBUG
      error_parse(err);
#endif
      goto LOCAL_ERROR;
    }

  /* 
   * Read the globals.
   */
  if((err=seh_setup_globals_ssdb()) && seh_init_check_fatal_error(err))
    {
      if(verbose >= VERB_DEBUG_0)
	seh_syslog_event(NULL,"SEH_INIT: Failed to setup globals :0x%llx.\n",
			 err);
#if EVENTS_DEBUG
      error_parse(err);
#endif
      goto LOCAL_ERROR;
    }
  
  /* 
   * Init all the events.
   */
  if((err=seh_setup_events(Pev)) && seh_init_check_fatal_error(err))
    {
      seh_syslog_event(NULL,"SEH_INIT: Failed to setup events "
		       ":0x%llx.\n",err);
#if EVENTS_DEBUG
      error_parse(err);
#endif
      goto LOCAL_ERROR;
    }
  
  /* 
   * All the other initialization is done. Now time to start handling
   * signals.
   */
  seh_init_signals();

  return 0;

LOCAL_ERROR:
  return err;
}

/* 
 * Check the arguments on the command line.
 */
__uint64_t seh_check_arguments(int argc,char **argv)
{
  int c;

  while ( -1 != (c = getopt(argc,argv,"v:hdAUSqQR:E:G:M:")) )
    {
      switch(c)
	{
	case 'v':		/* verbose flag. */
	  verbose=atoi(optarg);
	  break;
	case 'd':		/* become a daemon. if this flag  */
	  daemon=0;		/* is specified, dp not become a daemon. */
	  break;
	case 'A':
	case 'U':
	case 'S':
	case 'R':
	case 'E':
	case 'G':
	case 'M':
	case 'q':
	case 'Q':
	  {
	    char *Pchar=NULL;
	    int len;
	    int pid;
	    char *Pcsend=NULL;

	    sem_mem_alloc_init(); /* init the memory pool. */
	    stringtab_init();	  /* init the string table. */
	    
	    INITQ(2);		  /* message queues. */
	    
	    lockfd=open(LOCKFILENAME,O_WRONLY|O_CREAT,0700);            
	    if(lockfd>0)                                                
	    {                                                         
		if(!flock(lockfd,LOCK_EX|LOCK_NB))                       
		{                                                     
		    if(verbose > 0)
			fprintf(stderr,"Daemon not running.\n");
		    exit(-1);
		}                                                     
	    }                                                         
	    else                                                        
	    {                          
		fprintf(stderr,"Failed to open lock file %s.\n ",LOCKFILENAME);
		exit(-1);                                               
	    }                                                         

	    if(seh_archive_setup_interfaces(0))
	      {
		fprintf(stderr,"Unable to setup archive port.\n");
		exit(-1);
	      }

	    if(c == 'A')
		Pcsend=(char *)ARCHIVE_START;
	    else if (c == 'U')
		Pcsend=(char *)ARCHIVE_END;
	    else if (c == 'S')
		Pcsend=(char *)ARCHIVE_STATUS;
	    else if (c == 'q' || c == 'Q')
		Pcsend=(char *)SEMD_QUIT;
	    else if (c == 'R' || c == 'E' || c == 'G' || c == 'M')
	    {
		Pcsend = (char *)
		    sem_mem_alloc_temp(MAX_MESSAGE_LENGTH+strlen(optarg));
		switch(c)
		{
		case 'R':
		    strcpy(Pcsend,RULE_CONFIG_STRING);
		    break;
		case 'E':
		    strcpy(Pcsend,EVENT_CONFIG_STRING);
		    break;
		case 'G':
		    strcpy(Pcsend,GLOBAL_CONFIG_STRING);
		    break;
		case 'M':
		    strcpy(Pcsend,SGM_CONFIG_STRING);
		    break;
		}
		strcat(Pcsend," ");
		strcat(Pcsend,optarg);
	    }

	    seh_send_archive_port(0,Pcsend,strlen(Pcsend)+1,getpid(),NULL);

	    if (c == 'R' || c == 'E' || c == 'G' || c == 'M')
		sem_mem_free(Pcsend);

	    if(seh_receive_archive_port(0,&Pchar,&len,&pid,NULL))
	      {
		if(Pchar)
		  sem_mem_free(Pchar);
		goto archive_error;
	      }

	    if(c == 'S' && len && Pchar)
	      {
		fprintf(stderr,Pchar);
	      }

	    if(Pchar)
	      sem_mem_free(Pchar);
	    seh_archive_clean();
	    exit(0);
	  archive_error:
	    seh_archive_clean();
	    exit(-1);
	  }
	case 'h':		  /* usage. */
	default:
	  seh_usage(argv[0]);
	  exit(0);
	  break;
	}
    }
  
  return 0;
}

/* 
 * Clean up everything the seh is responsible for.
 */
void seh_clean_everything()
{
  sigset_t sigmask;

  sigemptyset(&sigmask);
  sigaddset(&sigmask,SIGALRM);

  pthread_sigmask(SIG_BLOCK,&sigmask,NULL);
  seh_api_clean();		/* task api */
  seh_archive_clean();		/* archive  */
  seh_dsm_clean();		/* dsm api */
  seh_clean_ssdb();		/* ssdb interface */
  seh_events_clean();		/* event data structures. */
  stringtab_clean();		/* should always be last... as prior */
				/* modules may use the string table. */
  pthread_sigmask(SIG_UNBLOCK,&sigmask,NULL);

  UNLOCK_FILE();
}

/*
 * Initialization required everytime we iterate in the main loop.
 */
void seh_loop_init(struct event_private *Pev,
		   struct event_record *Pev_rec,
		   __uint64_t *err)
{
  *err=0;
  memset(Pev_rec,0,sizeof(struct event_record));
  Pev_rec->valid_record=FALSE;
  return;
}
