#include "common.h"
#include "events.h"
#include "events_private.h"
#include "seh.h"
#include "seh_errors.h"
#include "sss_pthreads.h"
#include "sss_sysid.h"

extern int verbose,state;
#if EVENTS_DEBUG
unsigned int num_alarms=0;
#endif

void seh_thread_block_signals()
{
  sigset_t sigset;
  
  sigfillset(&sigset);
  sigdelset(&sigset,SIGSEGV);
  sigdelset(&sigset,SIGILL);
  sigdelset(&sigset,SIGABRT);
  sigdelset(&sigset,SIGKILL);
  sigdelset(&sigset,SIGBUS);
  sigdelset(&sigset,SIGFPE);
  pthread_sigmask(SIG_BLOCK,&sigset,NULL);
}

/*
 * Alarm handler for seh.
 * In the case of the seh and dsm being threads this alarm handler will call
 * the dsm's handler.
 * No locks should be held here when we enter this function.
 */
void seh_events_alarm_handler(int arg)
{
  /* 
   * Prefix of this function.
   *    o Try to get the lock.
   */
#if   SEM_NO_ALARM
  lock_ruleevent();
#else
  if(try_lock_ruleevent())
    {
      alarm(TIME_TO_WAKEUP_TO_THROTTLE);
      return;
    }
#endif

#if EVENTS_DEBUG
  if(verbose > VERB_DEBUG_1)	/* debug output */
    printf("ALRM:0\n");
  num_alarms++;
#endif

  /* 
   * Body of this function.
   *    o Call the seh and dsm functions for throtlling and 
   *      thresholding respectively.
   */
  seh_time_throttle_all_events();
  dsm_events_alarm_handler();

  /* 
   * Suffix of this function.
   *    o Release the locks.
   */
#if SEM_NO_ALARM
  unlock_ruleevent();
#else
  unlock_ruleevent();
  alarm(TIME_TO_WAKEUP_TO_THROTTLE);
#endif

#if EVENTS_DEBUG
  if(verbose > VERB_DEBUG_1)
    printf("ALRM:1\n");
#endif
  return;
}

#if SEM_NO_ALARM
/* 
 * Alarm thread if it is decided that it is better to have the 
 * alarm handler as a separate thread.
 *
 * Another reason for this may be PV: 646171 (no connection intermittently)
 */
void *seh_alarm(void *arg)
{
  unsigned int i=0;
  extern int exit_flag;

  seh_thread_block_signals();

  /* 
   * This thread is a very simple thread. It sits in a loop mostly sleeping.
   * Wakes up every few seconds to call the alarm handlers for the seh/dsm.
   */
  while(1)
    {
      if(exit_flag)
      {
	  sleep(TIME_TO_WAKEUP_TO_THROTTLE);
	  return NULL;
      }
      ALARM_CHECK_STATE();
      sleep(TIME_TO_WAKEUP_TO_THROTTLE);
      seh_events_alarm_handler(0);
      
    }
}
#endif

void seh_init_signals()
{
  struct sigaction sigact;

#if !SEM_NO_ALARM
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags    = SA_RESTART;
  sigact.sa_handler  = seh_events_alarm_handler;
  sigaction(SIGALRM,&sigact,NULL);
  alarm(TIME_TO_WAKEUP_TO_THROTTLE);
#endif

  /* 
   * Ignore any SIGPIPE received when dealing with sockets.
   * Sometimes a SIGPIPE is received when the tasks behave aberrantly and
   * close the socket (or do something funny with the socket) on which the
   * SEM and the task are communicating. The SEM treats any such situation
   * as a API failure.
   */
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags    = SA_RESTART;
  sigact.sa_handler  = SIG_IGN;
  sigaction(SIGPIPE,&sigact,NULL);
}

