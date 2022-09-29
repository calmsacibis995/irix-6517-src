#ifndef STEST__HEADER
#define STEST__HEADER
# include	<sys/types.h>
# include	<sys/param.h>
# include	<sys/times.h>
# include	<sys/time.h>
# include	<sys/ipc.h>
# include	<sys/prctl.h>
# include	<sys/lock.h>
# include	<sys/syssgi.h>
# include	<sys/immu.h>
# include	<sys/mman.h>
# include	<sys/shm.h>
#include <sys/schedctl.h>
#include <sys/sysmp.h>
# include	<signal.h>
# include 	<stdio.h>
# include	<string.h>
# include	<stdarg.h>
# include	<setjmp.h>
# include 	<fcntl.h>
# include	<math.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" int getpagesize();
extern "C" int wait(int*);
#endif

/***************************************************************
  MACROS
***************************************************************/

# define	mS(t)                ((float)t * (1000) / HZ) 
# define	tmS(t)          	(((float)(t)->tv_sec * 1000) + \
			((float)(t)->tv_nsec / 1000000))
#define timestruc_to_timeval(ts, tv)	\
	{ \
		(tv)->tv_sec = (ts)->tv_sec; \
		(tv)->tv_usec = (ts)->tv_nsec/1000; \
	}

/************************************************************** 
  CONSTANTS
***************************************************************/
# define        NSEC_PER_SEC    1000000000 
# define SHMMIN		0x1000
# define NCLD 200

/**************************************************************
  USER DEFINED TYPES
**************************************************************/

typedef struct {
   char			dl_valid; /* flag indicating whether deadline or not */
   timestruc_t		dl_interval; /* deadline frame */
   timestruc_t		dl_alloc; /* allocation in frame */
} dlinfo_t;

/* 
  sharea : shared memory structure
*/
struct shinfo {
	volatile int	begin;  /* condition variable for all spin process to wait on */
	volatile int	cease;  /* condition variable that causes all process to stop */
        /* done : set to true for each process that terminates */
	int		done[NCLD]; 
	clock_t		elapsed[NCLD]; /* elapsed time reported by times */
	struct tms	tms[NCLD]; /* elasped sys/utime reported by times */
	int		acount[NCLD]; /* action counts used when testing release/block */
	int		lcount[NCLD]; /* number of deadline sequences */
	int		spthrash[NCLD]; 
	double		realtime[NCLD]; /* realtime reported by getabstime*/
	double		grealtime[NCLD]; /* time reported by gettimeofday */
};
 /*
                           XXX iotimer_t
                           for some systems eg the EVEREST needs to be defined 
                           as long long, but only if you are using a 32 bit compiler
*/
typedef long iotimer_t;

/***************************************************************
  GLOBALS
***************************************************************/
struct shinfo	*sharea;
int		shmid;
char		*config = "./stest.config";

int		unice[NCLD];
int		undpri[NCLD];
dlinfo_t	udlinfo[NCLD];
char		*prog[NCLD];
int		block[NCLD];
int		release[NCLD];
int		only[NCLD];
int		cpid[NCLD];
int		thrash[NCLD];
int		usleep = 10;
int		nseen = 0;
int		chktime = 0;
int		chkgtime = 0;
double		localt = 0;
volatile struct prda_sys	*prda = 0;
__psunsigned_t cycleval;
volatile iotimer_t *iotimer_addr;

/***************************************************************
 FUNCTION PROTOTYPES
***************************************************************/
void		vperror(char *, ...);
void basicTest1(int nprocs,
	   int *nice,
	   int *ndpri,
	   dlinfo_t *deadline,
	   int slp);

/*********
   getconfig()  reads in the input file either stest.config or the one specified
   by the user and sets up the configuration of the test.
*/
void getConfig(); 

/*********  
  makeCommArea() sets up the shared memory 
*/
void makeCommArea();

/*********
 getabstime():
 returns the absolute value of elapsed time from the last time this function was called.
 It uses times and the internal clock timer. The clock timer is used if time elapsed, but the 
 process did not actually execute.
*/
double getabstime();

/*********
 sumabstime();
 increments localt by the amount returned by getabstime();
*/
void sumabstime();

/*********
 trapSignals();
 sets up the signal handler for the programs
*/
void trapSignals();

/*********
  chkblock
  action : to block or release
  alloc : time allocation for a deadline process
  stop : shared memory variable
  acount : counts how many blocks and releases took place
  lcount : counts how many deadline sequences were reported by the kernel
  
  Tests the blocking or release.
  It is possible in principal to block or release once for every sequence.
  If you want to test release in this manner, the process also has to
  be in only mode.
  
*/
void chkblock(int action, timestruc_t alloc, volatile int *stop, int *acount,
	int *lcount);

/********* 
  sets the variable kiddied to 1, which is used by spin to know if
  a child process terminated abnormally.
*/
void deadkid();

/********* 
  spin -- basic test function
  The purpose of spin is to execute the various scheduler tests. It supports
  the so called candy shop interface.

  int* start : pointer to shared memory variable
  int* stop  : pointer to shared memory variable
  volatile int* done : pointer to shared memory variable, set when
                       function finished executing
  volatile clock_t *elapsed : pointer to shared memory variable
                              reports time elapsed of a process 
                              as reported by the 'times' syscall
  volatile struct tms *tmp : pointer to shared memory variable
                             reports user and systime time elapsed as
                             returned by the  'times' syscalls  
  char* pr : what kind of deadline scheduling is desired
      pr == 0 test deadline scheduler using a loop
      pr == 1 test deadline scheduler using block
      pr == 2 test deadline scheduler using release
      pr > 10 --> being that we are using a program to test the 
             dl scheduler, a process is forked, and the time for that
             process is measured.
  int place : which process we are dealing with.

  What can spin do:
  1. It is set up to start and end at approximately the same time across
     different processes.
  2. It tests the deadline scheduler in different ways
     a. by just running in a loop and measuring actual time executed
     b. by running in a loop that blocks and unblocks
     c. by executing another program and testing how long that executes
  
  How does it do it:
   It first waits on start so that execution begins when all threads are ready
   and at the same time as all other spin threads.
   The start variable acts as a condition variable with broadcast activation.
   It then tests the deadline scheduler using the mechanism specified by pr. 
   When stop == true it stops executing.   
   spin reports three kinds of elapsed time 
   - > the values returned by times
   - > the values returned by gettimeofday
   - > using getabstime
*/

void spin(volatile int *start, volatile int *stop, volatile int *done,
	volatile clock_t *elapsed, volatile struct tms *tmp, char *pr,
	int place);

/*********
 crisis(int sig)
 Handles SIGCLD. Reports which child process died and then kills all remaining
 processes
*/
  
void crisis(int sig);


#endif





