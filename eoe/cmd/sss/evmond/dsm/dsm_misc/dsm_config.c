#include "common.h"
#include "events.h"
#include "dsm_events.h"
#include "dsm_events_private.h"
#include "dsm_rules.h"
#include "dsm.h"
#include "dsm_errors.h"

extern int verbose;

#if EVENTS_DEBUG
unsigned int num_success_config_rules=0;
unsigned int num_failed_config_rules=0;
#endif

/*
 * Reconfigure rule.
 *    Function at present will only delete or create a new rule.
 *    If rule with an object Id already exists and we are asked to
 *    create the rule again then just delete the rule and re-read it.
 */
__uint64_t dsm_reconfig_rule(struct event_private *Pev)
{
  __uint64_t err=0;
  int objid;
  Config_t *Pconfig;
  struct rule *Prule;


  /*
   * Check the incoming event for "sanity".
   */
  if(!Pev || Pev->event_class != SSS_INTERNAL_CLASS ||
     Pev->event_type != CONFIG_EVENT_TYPE)
    return 0;

  if(Pev->DSMDATA.event_datalength <= 0)
    {
      if(verbose >= VERB_INFO)
	seh_syslog_event(Pev,
			 "DSM_CONFIG got bad data length = %d.\n",
			 Pev->DSMDATA.event_datalength);
      return DSM_ERROR(DSM_MAJ_EVENT_ERR,DSM_MIN_EV_GRB);
    }

  /*
   * A write event lock is needed here as dsm_init_event_rule_ssdb may change
   * the event_rule member of some event structure.
   */
  lock_ruleevent();
#if CONFIGURATION_BY_EVENT
  /*
   * Get the in-memory pointer to the config event.
   */
  err=seh_find_event_hash(Pev->event_type,Pev->sys_id,&Pev);
#endif
  
  if(err || !RUN(Pev))
    {
      /*
       * Could not find the event.
       */
      unlock_ruleevent();
      return DSM_ERROR(DSM_MAJ_EVENT_ERR,DSM_MIN_EV_INVALID);
    }
  
  /*
   * Get a pointer to the configuration data.
   */
  Pconfig=(Config_t *)Pev->DSMDATA.Pevent_data;

  /*
   * Start looking at the configuration data now.. to see it is applicable to
   * the dsm. The dsm only is concerned with rules and global data.
   */
  switch(Pconfig->iConfigObject)
    {
    case RULE_CONFIG:
      {
	RuleConfig_t *Pruleconfig=(RuleConfig_t *)Pconfig;
	
	/* 
	 * Check for the size of the data and ensure it is an update.
	 */
	if(Pev->DSMDATA.event_datalength != sizeof(RuleConfig_t))
	  {
	    unlock_ruleevent();
#if EVENTS_DEBUG
	    num_failed_config_rules++;
#endif	
	    return DSM_ERROR(DSM_MAJ_EVENT_ERR,DSM_MIN_EV_GRB);
	  }

#if RULES_DEBUG
	fprintf(stderr,"Config Rule = %d\n",Pruleconfig->iobjId);
#endif
	if(Pconfig->iConfigType == NEW_CONFIG || Pconfig->iConfigType == UPDATE_CONFIG)
	  {
	    if(!dsm_find_rule_hash(Pruleconfig->iobjId,&Prule))
	      {
		/* 
		 * If rule already exists, assume user wanted to re-read 
		 * the rule.. so delete it first.
		 * As part of deleting, it also checks and breaks the link to any event
		 * it is no longer applicable to.
		 */
		dsm_delete_rule(Prule);
	      }
#if !CACHE_RULES	    
	    err=dsm_init_rule(Pruleconfig->iobjId,NULL);
	    if(err)
	      {
		/*
		 * Failed to initialize the rule.
		 */
		unlock_ruleevent();
		return err;
	      }
#endif
	    /*
	     * Initialize all the event <-> rule links. 
	     */
	    err=dsm_init_all_events_ssdb();
	    if(err)
	      {
		/*
		 * Failed to initialize the rule.
		 */
		unlock_ruleevent();
		return err;
	      }
	  }
	else if(Pconfig->iConfigType == DELETE_CONFIG)
	  {
	    if(err=dsm_find_rule(Pruleconfig->iobjId,&Prule))
	      {
		unlock_ruleevent();
#if EVENTS_DEBUG
		num_failed_config_rules++;
#endif	
		return err;
	      }
	    
	    err=dsm_delete_rule(Prule);
	  }
#if EVENTS_DEBUG
	if(!err)
	  num_success_config_rules++;
	else
	  num_failed_config_rules++;
#endif	
	break;
      }
    case GLOBAL_CONFIG:
      {
	GlobalConfig_t *Pglobal;
	/*
	 * Check for the size and ensure it is an update.
	 */
	if(Pconfig->iConfigType != UPDATE_CONFIG)
	{
	    unlock_ruleevent();
	    return DSM_ERROR(DSM_MAJ_EVENT_ERR,DSM_MIN_EV_GRB);
	}

	Pglobal=(GlobalConfig_t *)Pconfig;
	dsm_set_action(Pglobal->global_action);

	break;
      }
    }
  unlock_ruleevent();
  return err;
}
