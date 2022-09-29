#define _KMEMUSER 1
#ifndef _SGI_MP_SOURCE
#define _SGI_MP_SOURCE
#endif
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ulocks.h>
#include <task.h>
#include <signal.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/timers.h>				/* needs _KMEMUSER */
#include <sys/utsname.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/schedctl.h>
#include <sys/param.h>
#include <sys/prctl.h>				/* for PRDA */
#include <sys/procfs.h>				/* for reading from /proc */
#include <sys/sysmp.h>				/* for sysmp() call */

#include "mp.h"

#define RATE 3					/* minimum of 3 seconds between checking load */

extern	int	suggestednthreads;

struct timers_t {				/* keep track of running vs wait time */
  unsigned wall;				/* wall clock time in HZ ticks */
  unsigned run;					/* this cpu run time in HZ ticks */
  unsigned idle;				/* all cpus idle time in HZ ticks */
};

static struct status_t {			/* bounds and current number of processors */
  int32 min;
  int32 max;
  int32 now;
} status;

static struct timers_t t_now;			/* current times of interest */
static struct timers_t t_last;			/* previous, to calculate deltas */
static int verbose = FALSE;			/* TRUE to print informational messages */
static int frozen = FALSE;			/* TRUE if mp_numthreads or mp_set_numthreads */
static int clk_tck;				/* set in init routine, from limits.h */
static int fd;					/* for /proc of parent */
static int sugnumthd_pid = 0;			/* pid of mp_sugnumthd_thread */

static void get_timers(struct timers_t *timers)
{
	struct prpsinfo prpsinfo;		/* filled in from /proc ioctl */
        struct timeval tv;			/* for time of day */
	time_t cpu_time[6];			/* for cpu run, idle, wait etc. */
	int rc;

        BSD_getime(&tv);			/* like gettimeofday but faster */
	if (fd) {
	  rc = ioctl(fd,PIOCPSINFO,&prpsinfo);
          if (rc<0) {
            perror("mp_sugnumthd: ioctl of /proc failed");
            exit(1);
          }
        }
        rc = sysmp(MP_SAGET, MPSA_SINFO, &cpu_time, sizeof(cpu_time));
	if (rc < 0) {
          perror("mp_sugnumthd: sysmp MPSA_SINFO failed");
          exit(1);
	}
        timers->idle = cpu_time[0]+cpu_time[3];
	timers->run = prpsinfo.pr_time.tv_sec*100+prpsinfo.pr_time.tv_nsec/10000000;
	timers->wall = tv.tv_sec*100+tv.tv_usec/10000;
}

/*
 *	This routine wakes up every few seconds and checks to see what
 *	sort of load the master process has, as well as if ther are any
 *	idle processors in the system.
 */

static void mp_sugnumthd_examine(void)
{
	unsigned wall;
	unsigned run;
	unsigned idle;
	unsigned frac_run;
	unsigned frac_idle;
        int adjust = 0;
        
	if (frozen)				/* till we add our syssgi() call... */
	  return;

	get_timers(&t_now);			/* pick up current wall, run and idle times */

        wall = t_now.wall - t_last.wall;	/* compute deltas */
	run = t_now.run - t_last.run;
	idle = t_now.idle - t_last.idle;
	
	t_last = t_now;				/* copy whole structure */
	
	frac_run = 100*run/wall;		/* percent of time running vs wall time */
	frac_idle = 100*idle/wall;		/* total idle cpus (100 = one idle cpu) */

        if (frac_run < 90) {			/* less than ideal headway? */
          adjust = -1;
        } else {				/* go for it... */
          if (frac_idle > 100)
            adjust = 1+frac_idle/200;   	/* rather aggressive */
	}
	
	adjust += status.now;			/* desired adjustment */
	if (adjust > status.max)
	  adjust = status.max;			/* clip to max if needed */
	if (adjust < status.min)
	  adjust = status.min;			/* clip to min if needed */
	
	if (adjust != status.now) {
	  suggestednthreads = adjust;	/* inform the rest of the library */
	  if (verbose)				/* anything interesting happen? */
            printf("wall %3d run %3d idle %3d frac_run %3d frac_idle %3d now %3d was %3d\n",
              wall, run, idle, frac_run, frac_idle, adjust, status.now);
	  status.now = adjust;
	}
	
	PRDA->t_sys.t_hint = SGS_FREE;		/* we don't want to join the gang */
}

/*
 *	This is the thread to handle dynamic parallel jobs
 *	We set ourselves up to read the master thread state
 *	Then we sleep, with periodic wakeup every few seconds to
 *	  see if the system has become more or less busy.
 */

static void mp_sugnumthd_thread(uint_ptr spare)
{
	char path[20];				/* /proc file name for parent */
	int ppid;				/* parent pid */
	int nap;
	int rc;
	
	if (verbose)
		printf("mp_sugnumthd_thread => enter\n");
	PRDA->t_sys.t_hint = SGS_FREE;		/* we don't want to join the gang */
	
	ppid = getppid();			/* get main thread pid */
	
	sprintf(path,"/proc/pinfo/%05d",ppid);
	
	fd = open(path,O_RDONLY);		/* open parent's /proc/pinfo file */
	
	if (fd<0) {
	  perror("mp_sugnumthd: open of /proc failed");
	  exit(1);				/* application keeps going... */
	}
	
	rc =sysmp(MP_SASZ, MPSA_SINFO);
	if (rc < 0) {
          perror("mp_sugnumthd: sysmp MP_SASZ failed");
          exit(1);				/* application keeps going... */
	}

	get_timers(&t_last);			/* prime the timers */
	sugnumthd_pid = getpid();		/* so we can be killed at exit */
	
	while (1) {				/* forever (exit with other sprocs) */
          nap = clk_tck*RATE+(t_last.run+t_last.idle)%200;
	  while (nap = sginap(nap)) ;		/* wait for entire interval */
	  mp_sugnumthd_examine();		/* check system load */
	}
}

/*
 *	Called from mp.c after forking the computation sproc's
 *
 *	1. Checks to see of sugnumthd facility is wanted, returns if not.
 *	2. Checks to see if verbose messages are desired.
 *	3. Makes a static copy of min, max and current number of compute threads
 *         for later use (we make sure we stay within these bounds at all times)
 *	4. Creates an extra sproc so we can set an alarm clock & block on it
 */
 
void __mp_sugnumthd_exit()
{
	if (sugnumthd_pid)
	  kill(sugnumthd_pid,SIGTERM);		/* kill the sproc'd thread */
	sugnumthd_pid = 0;
}

/*
 *	If we are already frozen, just return.
 *	If user hasn't requested our service via MP_SUGNUMTHD or MPC_SUGNUMTHD, just return
 *	Check for user specified min & max number of processors
 *	Use sproc() call to make a child thread
 */

void __mp_sugnumthd_init(int32 min, int32 max,int32 now)
{
	char *charP;				/* for environment string */
	int n;					/* for environment integer */
	
	if (frozen)	{		/* might already be frozen... */
	  if (verbose)
		printf("__mp_sugnumthd_init => already frozen\n");
	  return;
	}
	  
	if ( getenv("MP_SUGNUMTHD_VERBOSE") || getenv("MPC_SUGNUMTHD_VERBOSE") )
	  verbose = TRUE;

	clk_tck = CLK_TCK;			/* needed by sginap, from limits.h */
	
	if ( ( (charP = getenv("MP_SUGNUMTHD_MIN")) != NULL) ||
	     ( (charP = getenv("MPC_SUGNUMTHD_MIN")) != NULL) )
	{
		n = atoi(charP);
		if ( (n>=min) && (n<=max) )
		  min = n;			/* honor user's min request if ok */
	}
	
	if ( ( (charP = getenv("MP_SUGNUMTHD_MAX")) != NULL) ||
	     ( (charP = getenv("MPC_SUGNUMTHD_MAX")) != NULL) )
	{
		n = atoi(charP);
		if ( (n>=min) && (n<=max) )
		  max = n;			/* honor user's min request if ok */
	}
	
	
	status.min = min;			/* keep local copies */
	status.max = max;
	status.now = now;
	if (verbose)
		printf("Creating mp_sugnumthd_thread\n");
	if (sproc((void(*)())mp_sugnumthd_thread, PR_SALL, 0) == -1) {
		perror("sproc for mp_sugnumthd_thread");
		exit(-1);
	}
}

/*
 *	Called from mp.c whenever the number of threads is frozen
 *
 *	1. If verbose is set and we haven't warned the user before, print a
 *	   warning message to stderr.
 *	2. If our child sproc is running, terminate it via a signal.
 */
 
void __mp_sugnumthd_freeze(char *why)
{
	if (frozen)				/* ignore after first time */
	  return;

	frozen = TRUE;				/* in case init called later */
	
	if ( !getenv("MP_SUGNUMTHD") && !getenv("MPC_SUGNUMTHD") )
	  return;

	if ( getenv("MP_SUGNUMTHD_VERBOSE") || getenv("MPC_SUGNUMTHD_VERBOSE") )
	  verbose = TRUE;
	
	if (verbose)
	  fprintf(stderr,"mp_sugnumthd warning: variable threads disabled due to %s call\n",why);

	/* remember the child pid from the sproc and do a kill here... */
}

