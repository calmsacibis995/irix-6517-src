#include "common.h"
#include "events.h"
#include "events_private.h"
#include "seh.h"
#include "seh_errors.h"
#include "sss_sysid.h"
#include "seh_archive.h"

extern int verbose;
extern SGMLicense_t license;

#if EVENTS_DEBUG
unsigned int num_success_config_events=0;
unsigned int num_failed_config_events=0;
unsigned int num_config_globals=0;
#endif

__uint64_t seh_build_config_event(struct event_private *Pev,char *Pchar,int len)
{
    int i;
    system_info_t *Psys = NULL;
    __uint64_t err=0;

    if(!Pev)
	return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);

    if(len <=0)
	return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);

    Pev->event_class = SSS_INTERNAL_CLASS;
    Pev->event_type  = CONFIG_EVENT_TYPE;

    if(err=sgm_get_sysid(&Psys,"localhost")) 
    {
	return err;
    }
    if(!Psys)			/* Not sure which error to return here.. */
	return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);
    Pev->sys_id=Psys->system_id;

    if(!strncasecmp(Pchar,SGM_CONFIG_STRING+MINIMUM_CONFIG_LENGTH,SGM_LENGTH))
    {
	SGMConfig_t *Psgmconfig = 
	    (SGMConfig_t *)sem_mem_alloc_temp(sizeof(SGMConfig_t));
	Pev->SEHDATA.event_datalength = Pev->DSMDATA.event_datalength =
	    sizeof(SGMConfig_t);
	Pev->SEHDATA.Pevent_data = Pev->DSMDATA.Pevent_data = (char *)
	    Psgmconfig;
	
	Psgmconfig->hConfig.iConfigObject = SGM_OPS;

	Pchar += SGM_LENGTH;
	while(*Pchar && *Pchar == ' ')
	    Pchar++;		/* skip over spaces. */
	
	if (!strncasecmp(Pchar,MODE_CONFIG_STRING,MODE_LENGTH))
	{
	    Psgmconfig->hConfig.iConfigType = MODE_CHANGE;
	    Pchar += MODE_LENGTH;
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"MODE Change ");
#endif
	}
	else if(!strncasecmp(Pchar,SUBSCRIBE_CONFIG_STRING,SUBSCRIBE_LENGTH))
	{
	    Psgmconfig->hConfig.iConfigType = SUBSCRIBE;
	    Pchar += SUBSCRIBE_LENGTH;
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"Subscribe ");
#endif
	}
	else if (!strncasecmp(Pchar,UNSUBSCRIBE_CONFIG_STRING,
			      UNSUBSCRIBE_LENGTH))
	{
	    Psgmconfig->hConfig.iConfigType = UNSUBSCRIBE;
	    Pchar += UNSUBSCRIBE_LENGTH;
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"UnSubscribe ");
#endif
	}
	else
	{
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"Unknown Mode Change\n");
#endif
	    sem_mem_free(Psgmconfig);
	    Pev->SEHDATA.Pevent_data = Pev->DSMDATA.Pevent_data = NULL;
	    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);
	}

	CLEAR_ERRNO();
	Psgmconfig->configdata = strtoull(Pchar,NULL,16);
	if(errno)
	{
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"Bad conversion of Mode change data\n");
#endif
	    sem_mem_free(Psgmconfig);
	    Pev->SEHDATA.Pevent_data = Pev->DSMDATA.Pevent_data = NULL;
	    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);
	}

#if EVENTS_DEBUG
	if(verbose > VERB_INFO)
	    fprintf(stderr,"Data = %llx\n",Psgmconfig->configdata);
#endif
    }
    else if (!strncasecmp(Pchar,RULE_CONFIG_STRING+MINIMUM_CONFIG_LENGTH,
			  RULE_LENGTH))
    {
	RuleConfig_t *Pruleconfig = 
	    (RuleConfig_t *)sem_mem_alloc_temp(sizeof(RuleConfig_t));

        Pev->SEHDATA.event_datalength = Pev->DSMDATA.event_datalength =
	    sizeof(RuleConfig_t);
	Pev->SEHDATA.Pevent_data = Pev->DSMDATA.Pevent_data = (char *)
	    Pruleconfig;
	
	Pruleconfig->hConfig.iConfigObject = RULE_CONFIG;
	
	Pchar += RULE_LENGTH;
	while(*Pchar && *Pchar == ' ')
	    Pchar++;            /* skip over spaces. */

	if (!strncasecmp(Pchar,NEW_CONFIG_STRING,NEW_LENGTH))
	{
	    Pruleconfig->hConfig.iConfigType=NEW_CONFIG;
	    Pchar += NEW_LENGTH;
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"New Rule ");
#endif
	}
	else if (!strncasecmp(Pchar,DELETE_CONFIG_STRING,DELETE_LENGTH))
	{
	    Pruleconfig->hConfig.iConfigType=DELETE_CONFIG;
	    Pchar += DELETE_LENGTH;
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"Delete Rule ");
#endif
	}
	else if (!strncasecmp(Pchar,UPDATE_CONFIG_STRING,UPDATE_LENGTH))
	{
	    Pruleconfig->hConfig.iConfigType=UPDATE_CONFIG;
	    Pchar += UPDATE_LENGTH;
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"Update Rule ");
#endif
	}
	else
	{
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"Unknown Rule Config\n");
#endif
	    sem_mem_free(Pruleconfig);
	    Pev->SEHDATA.Pevent_data = Pev->DSMDATA.Pevent_data = NULL;
	    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);
	}

        CLEAR_ERRNO();
	Pruleconfig->iobjId = strtoull(Pchar,NULL,16);
	if(errno)
	{
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"Bad conversion of Mode change data\n");
#endif
	    sem_mem_free(Pruleconfig);
	    Pev->SEHDATA.Pevent_data = Pev->DSMDATA.Pevent_data = NULL;
	    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);
	}

#if EVENTS_DEBUG
	if(verbose > VERB_INFO)
	    fprintf(stderr,"Num = %lx\n",Pruleconfig->iobjId);
#endif
    }
    else if (!strncasecmp(Pchar,EVENT_CONFIG_STRING+MINIMUM_CONFIG_LENGTH,
			  EVENT_LENGTH))
    {
	EventConfig_t *Pevconfig = 
	    (EventConfig_t *)sem_mem_alloc_temp(sizeof(EventConfig_t));

        Pev->SEHDATA.event_datalength = Pev->DSMDATA.event_datalength =
	    sizeof(EventConfig_t);
	Pev->SEHDATA.Pevent_data = Pev->DSMDATA.Pevent_data = (char *)
	    Pevconfig;
	
	Pevconfig->hConfig.iConfigObject = EVENT_CONFIG;
	
	Pchar += EVENT_LENGTH;
	while(*Pchar && *Pchar == ' ')
	    Pchar++;            /* skip over spaces. */

	if (!strncasecmp(Pchar,NEW_CONFIG_STRING,NEW_LENGTH))
	{
	    Pevconfig->hConfig.iConfigType=NEW_CONFIG;

	    Pchar += NEW_LENGTH;
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"New Event ");
#endif
	}
	else if (!strncasecmp(Pchar,DELETE_CONFIG_STRING,DELETE_LENGTH))
	{
	    Pevconfig->hConfig.iConfigType=DELETE_CONFIG;
	    Pchar += DELETE_LENGTH;
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"Delete Event ");
#endif
	}
	else if (!strncasecmp(Pchar,UPDATE_CONFIG_STRING,UPDATE_LENGTH))
	{
	    Pevconfig->hConfig.iConfigType=UPDATE_CONFIG;
	    Pchar += UPDATE_LENGTH;
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"Update Event ");
#endif
	}
	else
	{
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"Unknown Event configuration %s\n",Pchar);
#endif
	    sem_mem_free(Pevconfig);
	    Pev->SEHDATA.Pevent_data = Pev->DSMDATA.Pevent_data = NULL;
	    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);
	}
	/*
	 * Get the hostname.
	 */
	if(sscanf(Pchar,"%s",Pevconfig->cHostName) != 1)
	{
	    sem_mem_free(Pevconfig);
	    Pev->SEHDATA.Pevent_data = Pev->DSMDATA.Pevent_data = NULL;
	    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);
	}

	while(*Pchar && *Pchar == ' ')
	    Pchar++;		/* skip over spaces. */
	Pchar += strlen(Pevconfig->cHostName);
	
	/* 
	 * Get the class, type, throttle, frequency and isthrottle flag.
	 */
	i=sscanf(Pchar,"%x%x%x%x%x",&Pevconfig->iEventClass,
		 &Pevconfig->iEventType,&Pevconfig->ithrottle_counts,
		 &Pevconfig->ithrottle_times,&Pevconfig->iIsenabled);
		 
	if((Pevconfig->hConfig.iConfigType == UPDATE_CONFIG ||
	    Pevconfig->hConfig.iConfigType == NEW_CONFIG) && i != 5)
	{
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
	    {
		fprintf(stderr,"Bad conversion of Event update/new data\n");
	    }
#endif
			
	    sem_mem_free(Pevconfig);
	    Pev->SEHDATA.Pevent_data = Pev->DSMDATA.Pevent_data = NULL;
	    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);
	}
	
	if(Pevconfig->hConfig.iConfigType == DELETE_CONFIG && i != 2)
	{
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"Bad conversion of Event Delete data\n");
#endif
	    sem_mem_free(Pevconfig);
	    Pev->SEHDATA.Pevent_data = Pev->DSMDATA.Pevent_data = NULL;
	    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);
	}
#if EVENTS_DEBUG
	fprintf(stderr,"H=%s, C=%d, T=%d, c=%d t=%d i=%d\n",
		Pevconfig->cHostName,
		Pevconfig->iEventClass,
		Pevconfig->iEventType,Pevconfig->ithrottle_counts,
		Pevconfig->ithrottle_times,Pevconfig->iIsenabled);
#endif	
    }
    else if (!strncasecmp(Pchar,GLOBAL_CONFIG_STRING+MINIMUM_CONFIG_LENGTH,
			  GLOBAL_LENGTH))
    {
	GlobalConfig_t *Pglobal = 
	    (GlobalConfig_t *)sem_mem_alloc_temp(sizeof(GlobalConfig_t));
	
        Pev->SEHDATA.event_datalength = Pev->DSMDATA.event_datalength =
	    sizeof(GlobalConfig_t);
	Pev->SEHDATA.Pevent_data = Pev->DSMDATA.Pevent_data = (char *)
	    Pglobal;
	
	Pglobal->hConfig.iConfigObject=GLOBAL_CONFIG;
	Pglobal->hConfig.iConfigType=UPDATE_CONFIG;

	Pchar += GLOBAL_LENGTH;

	i=sscanf(Pchar,"%x%x%x",
		 &Pglobal->global_throttling,
		 &Pglobal->global_logging,
		 &Pglobal->global_action);
	if(i!=3)
	{
#if EVENTS_DEBUG
	    if(verbose > VERB_INFO)
		fprintf(stderr,"Bad conversion of Global data\n");
#endif
	    sem_mem_free(Pglobal);
	    Pev->SEHDATA.Pevent_data = Pev->DSMDATA.Pevent_data = NULL;
	    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);
	}
#if EVENTS_DEBUG
	if(verbose > VERB_INFO)
	    fprintf(stderr,"Global data, T=%d, L=%d, A=%d\n",
		    Pglobal->global_throttling,
		    Pglobal->global_logging,
		    Pglobal->global_action);
#endif
		    
    }
    else
    {
	return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);
    }
    return 0;
}

/* 
 * Main function to handle Configuration events.
 */
__uint64_t seh_reconfig_event(struct event_private *Pev)
{
  int event_class, event_type;
  unsigned long throttle_counts, throttle_times;
  __uint64_t err=0;
  __uint64_t system_id = 0;
  struct event_private *Pev_temp;
  system_info_t *Psys = NULL;
  Config_t *Pconfig;
    
  /* 
   * Check if it is a configuration event.
   */
  if(Pev->event_class != SSS_INTERNAL_CLASS || 
     Pev->event_type  != CONFIG_EVENT_TYPE)
    return 0;

  /* 
   * If no data came along with the configuration event, just dispose it.
   */
  if(Pev->SEHDATA.event_datalength <= 0)
    {
      if(verbose >= VERB_INFO)
	seh_syslog_event(Pev,"SEH_CONFIG got bad data length = %d.\n",
			 Pev->SEHDATA.event_datalength);
      return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_GRB);
    }

  /* 
   * Now grab the event data and start examining it.
   */
  Pconfig=(Config_t *)Pev->SEHDATA.Pevent_data;
  if((Pconfig->iConfigObject != EVENT_CONFIG) &&
     (Pconfig->iConfigObject != RULE_CONFIG)  &&
     (Pconfig->iConfigObject != SGM_OPS) &&
     (Pconfig->iConfigObject != GLOBAL_CONFIG))
    {
      if(verbose >= VERB_INFO)
	seh_syslog_event(Pev,"SEH_CONFIG got bad config object = %d.\n",
			 Pconfig->iConfigObject);
      return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_GRB);
    }

  if((Pconfig->iConfigType != UPDATE_CONFIG) &&
     (Pconfig->iConfigType != NEW_CONFIG)    &&
     (Pconfig->iConfigType != DELETE_CONFIG) &&
     (Pconfig->iConfigType != MODE_CHANGE)    &&
     (Pconfig->iConfigType != SUBSCRIBE)    &&
     (Pconfig->iConfigType != UNSUBSCRIBE))
    {
      if(verbose >= VERB_INFO)
	seh_syslog_event(Pev,"SEH_CONFIG got bad config type = %d.\n",
			 Pconfig->iConfigType);
      return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_GRB);
    } 

  switch(Pconfig->iConfigObject)
    {
    case(EVENT_CONFIG):		/* change in some event. */
      {
	EventConfig_t *Pevconfig=(EventConfig_t *)Pconfig;
	
	if ( strlen(Pevconfig->cHostName) != 0 ) {
	    if ( sgm_get_sysid(&Psys, Pevconfig->cHostName) ) return 0;
	    system_id = Psys->system_id;
	} else {
	    if ( (system_id = Pev->sys_id) == 0 ) return 0;
	}

	/* 
	 * Check for the size of the data and ensure it is an update.
	 */
	if(Pev->SEHDATA.event_datalength != sizeof(EventConfig_t))
	  {
#if EVENTS_DEBUG
	    num_failed_config_events++;
#endif
	    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_GRB);
	  }
	
	event_class=Pevconfig->iEventClass;
	event_type =Pevconfig->iEventType;

	/* 
	 * Cannot do anything to config/error event.
	 */
	if(event_class == SSS_INTERNAL_CLASS)
	  {
#if EVENTS_DEBUG
	    num_failed_config_events++;
#endif
	    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_INVALID);
	  }

#if (EVENTS_DEBUG > 2)
	if(verbose > VERB_DEBUG_1)
	  fprintf(stderr,"Config Class = %d, Type = %d c=%d t=%d i=%d\n",
		  event_class,event_type,Pevconfig->Lthrottle_counts,
		  Pevconfig->Lthrottle_times,Pevconfig->iIsenabled);
#endif
	switch(Pconfig->iConfigType)
	  {
	  case UPDATE_CONFIG:	/* update some event. */
  	    /*
	     * In the new model the SEH will just read in an event when 
	     * necessary. So if the event to be updated is not in memory
	     * then just ignore the config event. This is ok, since the
	     * re-configured event will be read in if the event is received.
	     */
	    if(Pevconfig->iIsenabled      < 0  || 
	       Pevconfig->ithrottle_counts < 0  || 
	       Pevconfig->ithrottle_times  < 0)
	      {
		if(verbose >= VERB_INFO)
		  seh_syslog_event(Pev,
				   "SEH_CONFIG got bad throttle parameter.");
#if EVENTS_DEBUG
		num_failed_config_events++;
#endif
		return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_GRB);
	      }

	    lock_ruleevent();	/* enter critical section */

	    if(event_class == WILDCARD_EVENT && event_type == WILDCARD_EVENT)
	      {
		seh_update_all_events_ssdb(system_id);
	      }
	    else
	      if(event_type == WILDCARD_EVENT)
		{
		  seh_update_all_events_byclass_ssdb(system_id,event_class);
		}
	    else
	      if(event_class == WILDCARD_EVENT)
		{
		  seh_update_all_events_bytype_ssdb(system_id, event_type);
		}
	    else
	      {
		if(err=seh_find_event_hash(event_type,system_id,&Pev_temp))
		  {
		    unlock_ruleevent();
		    return 0;		
		  }
	    
		/*
		 * Initialize the throttling parameters.
		 */
		Pev_temp->event_throttle_counts = Pevconfig->ithrottle_counts;
		Pev_temp->event_throttle_times  = Pevconfig->ithrottle_times;
		Pev_temp->isthrottle            = TRUE;

		if(!Pevconfig->iIsenabled)
		    seh_delete_event(Pev_temp);
	      }

	    unlock_ruleevent();	/* exit critical section. */	

	    break;
	  case NEW_CONFIG:	/* add a new event. */
            /*
	     * Same class and type should not exist already in memory.
	     * Do not add it to memory as we will anyway pick it up later.
	     * If event does not exist in memory, do not bother to check if
	     * it already exists in ssdb.
	     */
	    if(!seh_find_event_hash(event_type,system_id, &Pev_temp))
	      {
#if EVENTS_DEBUG
		num_failed_config_events++;
#endif
		return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_DUP);
	      }
	    break;
	  case DELETE_CONFIG:	/* delete an exsting event. */
	    if(err=seh_find_event_hash(event_type,system_id, &Pev_temp))
	      return 0;

	    lock_ruleevent();	/* enter critical section */
	    err=seh_delete_event(Pev_temp);
	    unlock_ruleevent();	/* exit critical section. */	

	    break;
	  }
#if EVENTS_DEBUG
	if(!err)
	  num_success_config_events++;
	else
	  num_failed_config_events++;
#endif
	break;
      }
    case SGM_OPS:    /* Config Operations for SGM */
      {
	SGMConfig_t *Psgmconfig;

	if ( Pev->SEHDATA.event_datalength != sizeof(SGMConfig_t) ) 
	  return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_GRB);

	Psgmconfig = (SGMConfig_t *)Pconfig;

	switch (Pconfig->iConfigType)
	  {
	    case MODE_CHANGE:
	      {
                lock_ruleevent();
		if ( (license.current_mode == RUN_AS_SEM) &&
		     (Psgmconfig->configdata == RUN_AS_SGM) ) {
		  if ( (err = upgrade_sgm(&license)) != 0 ) {
		    unlock_ruleevent();
                    return(err);
		  }
                } else if ( (license.current_mode == RUN_AS_SGM) &&
			    (Psgmconfig->configdata == RUN_AS_SEM) ) {
		  if ( (err = degrade_sgm(&license)) != 0 ) {
		    unlock_ruleevent();
		    return (err);
		  }
                }
		unlock_ruleevent();
		break;
	      }
	    case SUBSCRIBE:
	      {
		if ( (license.current_mode != RUN_AS_SGM ) ||
		     (license.requested_mode != RUN_AS_SGM) ) 
                  return SEH_ERROR(SEH_MAJ_EVENT_ERR, SEH_MIN_EV_SUBSCR);
#if EVENTS_DEBUG
                if ( verbose > VERB_INFO ) 
                  fprintf(stderr, "Rereading sysinfo for mode = %d\n",
                          license.current_mode);
#endif
		lock_ruleevent();
		if ((err=seh_read_sysinfo(&license)) != 0) {
		  unlock_ruleevent();
                  return err;
		}
		unlock_ruleevent();
		break;
	      }
	    case UNSUBSCRIBE:
	      {
		if ( (license.current_mode != RUN_AS_SGM) ||
		     (license.requested_mode != RUN_AS_SGM) )
                  return SEH_ERROR(SEH_MAJ_EVENT_ERR, SEH_MIN_EV_USUBSCR);
#if EVENTS_DEBUG
                if ( verbose > VERB_INFO ) 
                  fprintf(stderr, "Rereading sysinfo for mode = %d\n",
                          license.current_mode);
#endif
		lock_ruleevent();
		if ((err=seh_read_sysinfo(&license)) != 0) {
		  unlock_ruleevent();
                  return err;
                }
		unlock_ruleevent();
		break;
	      }
	  }
	break;
      }
    case GLOBAL_CONFIG:		/* change in some global data. */
      {
	GlobalConfig_t *Pglobal;
	
	/* 
	 * Check for the size and ensure it is an update.
	 */
	if((Pev->SEHDATA.event_datalength != sizeof(GlobalConfig_t)) ||
	   (Pconfig->iConfigType != UPDATE_CONFIG))
	  return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_GRB);

	Pglobal=(GlobalConfig_t *)Pconfig;
	seh_set_logging(Pglobal->global_logging);
	seh_set_throttling(Pglobal->global_throttling);
#if EVENTS_DEBUG
	num_config_globals++;
#endif
	break;
      }
    }
  return err;
}

/* 
 * Initialize the sss configuration event. This is not user definable, nor
 * is this event dynamically configurable. It has no corresponding entry
 * in the event_type table of the ssdb.
 */
__uint64_t seh_initialize_config_event(struct event_private *Pev,
				       __uint64_t sys_id)
{
  int len=0;

  memset(Pev,0,sizeof(struct event_private));

  Pev->event_class=SSS_INTERNAL_CLASS; /* configuration class. */
  Pev->event_type=CONFIG_EVENT_TYPE;
  Pev->sys_id = sys_id;
  seh_init_each_event(Pev);

  return 0;
}

/*
 * Generate Data that needs to be inserted for a config event
 *
 */

char *seh_form_config_event_record(struct event_private *Pev)
{
  char *Pdata = NULL;
  static char cBuffer[MAX_MESSAGE_LENGTH];
  int  nobytes = 0;
  Config_t *Pconfig;

  if ( Pev->event_class != SSS_INTERNAL_CLASS ||
       Pev->event_type  != CONFIG_EVENT_TYPE )
    return Pdata;

  Pconfig=(Config_t *)Pev->SEHDATA.Pevent_data;
  switch(Pconfig->iConfigObject)
    {
      case EVENT_CONFIG:
        {
          EventConfig_t *Pevconfig = (EventConfig_t *)Pconfig;
          nobytes = snprintf(cBuffer, MAX_MESSAGE_LENGTH,
                              "E|%d|%d:%d:%d:%d:%d:%s",
                              Pconfig->iConfigType, Pevconfig->iEventClass,
                              Pevconfig->iEventType, Pevconfig->iIsenabled,
                              Pevconfig->ithrottle_counts,
                              Pevconfig->ithrottle_times, Pevconfig->cHostName);          break;
        }
      case SGM_OPS:
        {
          SGMConfig_t *Psgmconfig = (SGMConfig_t *) Pconfig;
          nobytes += snprintf(cBuffer, MAX_MESSAGE_LENGTH, "S|%d|%llX",
                     Pconfig->iConfigType, Psgmconfig->configdata);
          break;
        }
      case GLOBAL_CONFIG:
        {
          GlobalConfig_t *Pglobal = (GlobalConfig_t *) Pconfig;
          nobytes += snprintf(cBuffer, MAX_MESSAGE_LENGTH,
                              "G|%d|%d:%d:%d",
                              Pconfig->iConfigType, Pglobal->global_throttling,
                              Pglobal->global_logging, Pglobal->global_action);
          break;
        }
      case RULE_CONFIG:
        {
          RuleConfig_t *Prule = (RuleConfig_t *) Pconfig;
          nobytes += snprintf(cBuffer, MAX_MESSAGE_LENGTH, "R|%d|%d",
                              Pconfig->iConfigType, Prule->iobjId);
          break;
        }
      default:
          return Pdata;
    }
    cBuffer[nobytes++] = '\0';
    Pdata=sem_mem_alloc_temp(nobytes);
    if (Pdata)
        strcpy(Pdata, cBuffer);
    return Pdata;
}

