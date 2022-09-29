#include "common.h"
#include "dsm_events.h"
#include "dsm_events_private.h"
#include "dsm_rules.h"
#include "dsm.h"
#include "dsm_errors.h"
#include "limits.h"
#include <sys/stat.h>

#if EVENTS_DEBUG
extern unsigned int num_actions_taken;
#endif
extern int global_action_flag;

/*
 * Start the dso, which will initiate the action to execute.
 * Make sure the arguments are given in the correct order to the dso.
 */
__uint64_t dsm_start_dso(struct rule *Prule,struct event_private *Pev)
{
  char **args=NULL,*dso=NULL;
  __uint64_t err=DSM_ERROR(DSM_MAJ_ACTION_ERR,DSM_MIN_ALLOC_MEM);
  int i=0;
  struct stat statbuf;
  char *action;

  /* 
   * Silly checks.
   */
  if(!Prule || !Prule->Paction)
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_INVALID_ARG);
  action=Prule->Paction->action;
  if(!action)
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_INVALID_ARG);
  if(!Pev || !RUN(Pev))
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_INVALID_ARG);

  /* 
   * check if the action can be executed.
   */
  if(!global_action_flag)
    return 0;

  /* 
   * array of all the arguments.
   */
  args=(char **)sem_mem_alloc_temp(sizeof(char *)*MAX_ARGS);

  if(!args)
    goto local_error;

  /* 
   * split the action string into its component arguments.
   */
  err=dsm_action_string_into_args(action,Pev,Prule,&args[0]);
  if(err)
    {
      goto local_error;
    }

  /* 
   * Start the dso here.
   */
  dso=args[0];
  if(!dso || stat(dso,&statbuf))
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_INVALID_ARG);

  /*
   * Record that the action was taken in the ssdb.
   * Do the write to the ssdb before the action is actually taken, since there
   * is no saying how long the action can take.
   */
  dsm_create_action_taken_record_ssdb(Prule,Pev);
  
#if EVENTS_DEBUG
  num_actions_taken++;
#endif

  if(exec_dso(dso,Pev->event_class,Pev->event_type,RUN(Pev)->priority,
	      RUN(Pev)->event_counts,Pev->DSMDATA.Pevent_data,
	      Pev->DSMDATA.event_datalength,Pev->DSMDATA.hostname,&args[1]))
    err= DSM_ERROR(DSM_MAJ_ACTION_ERR,DSM_MIN_RULE_DSO);

local_error:
  /* 
   * Free the memory allocated for the arguments.
   */
  if(args)
    {
      i=0;
      while(args[i])
	sem_mem_free(args[i++]);
      sem_mem_free(args);
    }

  return err;
}
