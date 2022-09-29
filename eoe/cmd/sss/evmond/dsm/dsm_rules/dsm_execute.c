#include "common.h"
#include "events.h"
#include "dsm_events.h"
#include "dsm_events_private.h"
#include "dsm_rules.h"
#include "dsm.h"
#include "dsm_errors.h"

extern int verbose;
extern int global_action_flag;
extern struct event_private *Pevent_head;
extern struct event_private *Pevent_tail;
#define WRAPPER_EXECUTABLE SSS_HOME_DIRECTORY ## "./dsm_wrap"

/*
 * verifies permission/security etc...
 * XXX: for now assume everything is ok.
 */
__uint64_t dsm_is_executable_action(char *action)
{	
  return 1;
}

/*
 * Main function which initiates the action for the DSM.
 */
__uint64_t dsm_take_action(struct rule *Prule,struct event_private *Pev)
{

  __uint64_t err=0;
  extern void dsm_harvest_children();

#if PCP_TRACE
  pmtracepoint("dsmaction");
  pmtracebegin("dsmaction");
#endif

  dsm_harvest_children();

  if(!global_action_flag)
    goto local_error;

  if(!Prule || !Pev)		/* silly check. */
    goto local_error;

  /*
   * Read in the action from the ssdb. According to the new policy the rule
   * is read in from the ssdb when required and free'd up after the action 
   * is taken.
   */
  if(err=dsm_init_action_rule_ssdb(Prule,Prule->rule_objid,NULL))
    {
      goto local_error;
    }

  /*
   * No action structure then just return.
   */
  if(!Prule->Paction)
    goto local_error;

  /*
   * Again, return if the action string is missing.
   */
  if(!Prule->Paction->action)
    {
      dsm_free_action_in_rule(Prule);
      goto local_error;
    }

  /*
   * Initiate the action. There is no turning back now.
   */
  if(Prule->isdso == TRUE)
    dsm_start_dso(Prule,Pev);
  else
    dsm_start_wrapper(WRAPPER_EXECUTABLE,Prule,Pev);


  /*
   * Free the action. If it is found later that there is a lot of thrashing for
   * freeing and reading the action, then maybe this should be cached too.
   */
  dsm_free_action_in_rule(Prule);

local_error:
#if PCP_TRACE
  pmtraceend("dsmaction");
#endif
  return err;
}
  
