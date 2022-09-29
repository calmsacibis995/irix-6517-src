#include "common.h"
#include "events.h"
#include "events_private.h"
#include "dsm_events.h"
#include "dsm_events_private.h"
#include "dsm_rules.h"
#include "dsm.h"
#include "dsm_errors.h"
#include "sss_pthreads.h"

/*
 * Alarm handler for the DSM.
 * It gets called by the alarm handler from the SEH when running as pthreads.
 * thread it may result in a deadlock.
 * 
 */
void dsm_events_alarm_handler(int arg)
{
#if (RULES_DEBUG > 1)
  printf("DSMALRM:0\n");
#endif

  /*
   * Locks get released in this function.
   */
  dsm_time_throttle_all_events();

  /* 
   * Suffix of this function.
   */
#if (RULES_DEBUG > 1)
  printf("DSMALRM:1\n");
#endif
  return;
}

void dsm_init_signals()
{
  struct sigaction sigact;

  /*
   * lastly, setup signals.
   * Handle the death of the children.
   */
  sigemptyset(&sigact.sa_mask);
  sigact.sa_flags    = SA_RESTART;
  sigact.sa_handler=dsm_events_child_handler;
  sigaction(SIGCLD,&sigact,NULL);
  
}
