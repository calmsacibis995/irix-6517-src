#include "common.h"
#include "events.h"
#include "events_private.h"
#include "dsm_events.h"
#include "dsm_events_private.h"
#include "dsm_rules.h"
#include "dsm.h"
#include "dsm_errors.h"
#include "sss_pthreads.h"
extern int verbose,state;
#if PCP_TRACE
#include <pcp/trace.h>
#endif


/*
 * Check if this is a fatal error for the dsm making it inoperable.
 */
static int dsm_main_check_fatal_error(__uint64_t err)
{
  if((DSM_ERROR_MAJOR(err) == DSM_MAJ_MAIN_ERR) ||
     (DSM_ERROR_MAJOR(err) == DSM_MAJ_INIT_ERR))
    return 1;

  if(DSM_ERROR_MINOR(err) & (DSM_MIN_ALLOC_MEM))
    return 1;
  return 0;
}

/* 
 * Main entry point for the dsm thread.
 */
void *dsm_main(void *arg)
{
  struct event_private ev;
  __uint64_t err;
  extern int exit_flag;

  /*
   * Set this thread's pointers to point to the ones set by dsm_initialize.
   */
  dsm_thread_initialize(&ev);

  /*
   * To this point SEH is already initialised so we can 
   * safely turn on event handling
   */
  EVENT_STATE();

  lock_dsmconfig();
  while(1)
    {
      if(exit_flag)
      {
	  unlock_dsmconfig();
	  return NULL;
      }
      /*
       * Forever loop -> Main loop for dsm thread.
       * Get event.
       * Handle event.
       * Log action taken.
       * Back to Get event.
       */

      /*
       * Clear the ev structure.
       */
      dsm_loop_init(&ev,&err);

      /* 
       * Do the appropriate thing depending on the state the dsm is in.
       */
      DSM_CHECK_STATE(&ev);

      unlock_dsmconfig();
      /*
       * Get the event from the SEH.
       */
      if(err=dsm_get_event_seh(&ev))
      {
	  lock_dsmconfig();
	  goto DSM_ERROR;
      }
      lock_dsmconfig();

      /*
       * update throttling counters for the rule(s) applicable to this 
       * event.
       */
      if(err=dsm_throttle_rules_event(&ev))
	goto DSM_ERROR;

    DSM_ERROR:
      if(err && dsm_main_check_fatal_error(err))
	{
	  seh_syslog_event(&ev,"DSM_MAIN got a fatal error -- %llx\n",err);
	  break;
	}
    }

  unlock_dsmconfig();
  return sss_pthreads_exit();
}
