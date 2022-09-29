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
#if PCP_TRACE
  int pcp_trace;
#endif
  extern int exit_flag;

  /*
   * Set this thread's pointers to point to the ones set by dsm_initialize.
   */
  dsm_thread_initialize(&ev);

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

#if PCP_TRACE
      if((pcp_trace=pmtracepoint("dsmloop")) < 0)
	{
	  fprintf(stderr,"sehloop point trace failed(%d): %s.\n",
		  pcp_trace,pmtraceerrstr(pcp_trace));
	}
#endif

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

#if CONFIGURATION_BY_EVENT
      if(err=dsm_reconfig_rule(&ev))
	  goto DSM_ERROR;
#endif

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
