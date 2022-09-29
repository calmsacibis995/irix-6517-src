#include "common.h"
#include <stdio.h>
#include "pwd.h"
#include <syslog.h>
#include <stdarg.h>

#define WRAPPER_DEBUG 0

/*
 * Program is intended as a wrapper for any executable created by
 * the dsm. The functions of this wrapper are to 
 * check for timeout and then restart the executable created by the dsm.
 */
/* harvest terminated children without waiting */
void wrap_harvest_children()
{
  int status;

  while (waitpid(-1, &status, WNOHANG) > (pid_t)0)
    ;
}

/*
 * harvest and die if the children dies.
 */
void wrap_child_handler()
{
  wrap_harvest_children();
  exit(0);
}

/*
 * Log message into syslog.
 */
__uint64_t syslog_msg(char *msg,...)
{
  char *buf=calloc(MAX_MESSAGE_LENGTH,1);
  va_list args;
  int len=0;

  if(!msg)
    {
      free(buf);
      return 0;
    }

  va_start(args,msg);
  len=vsnprintf(buf,MAX_MESSAGE_LENGTH,msg,args);
  va_end(args);

  openlog("esp",LOG_PID|LOG_CONS|LOG_NOWAIT,LOG_USER);

  syslog(LOG_INFO,"%s : %m",buf);

  closelog();
  
  free(buf);

  return 0;
}

/* 
 *  Close all file descriptors. We do not want our descriptors
 * being used later by the exec'ed process.
 */
void wrap_close_fds()
{
  int i=0;

  for(;i<getdtablehi();i++)	/* get highest open fd. */
    close(i);
}

/*
 * Setup the uid and gid for the exec'ed process.
 */
int wrap_set_user(char *user)
{
   struct passwd *pw=NULL;

   if(!(pw=getpwnam(user)))	/* get password file entry. */
     return -1;

   if(setgid(pw->pw_gid))	/* set group-id first. */
     return -1;

   if(setuid(pw->pw_uid))	/* set user-id last. */
     return -1;


   return 0;
}

main(int argc,char *argv[],char *environ[])
{
  int i=0;
  int retry,timeout;
  pid_t cpid;
  struct sigaction sigact;

  assert(argc>=5);
  
#if WRAPPER_DEBUG
  printf("Fire action %s\n",argv[0]);
  
  for(i=1;i<argc;i++)
    printf("args[%d]=%s\n",i,argv[i]);

  for(i=0;environ[i];i++)
    printf("%s\n",environ[i]);
#endif

  wrap_close_fds();
  if(wrap_set_user(argv[3]))
    {
      syslog_msg("Unable to change user to %s",argv[3]);
      exit(-1);		
    }

  retry=atoi(argv[2]);
  timeout=atoi(argv[1]);

  do
    {
      sigemptyset(&sigact.sa_mask);
      sigact.sa_flags    = SA_RESTART;
      sigact.sa_handler=wrap_child_handler;
      sigaction(SIGCLD,&sigact,NULL);

      if(!(cpid=fork()))
	{			/* execv must be done immedately after the for.*/
	  execv(argv[4],&argv[4]);
	  exit(0);
	}

      if(!timeout)		/* just exit if timeout == 0. */
	exit(0);

      sleep(timeout);
      if(!kill(cpid,0))		/* check if child exists. */
	{
	   sigemptyset(&sigact.sa_mask);
	   sigact.sa_flags  =SA_RESTART;
	   sigact.sa_handler=SIG_IGN;
	   sigaction(SIGCLD,&sigact,NULL);
		 
	   kill(cpid,SIGKILL);
	   wrap_harvest_children();
	}
      else
	break;

      sleep(10);		/* sleep for some random time.. */
      if(!kill(cpid,0))		/* check if we have not been able */
	exit(-1);		/* to kill the child yet. if so, exit. */
    } while((retry--)>0);
}

