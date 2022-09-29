#include "common.h"
#include "events.h"
#include "dsm_events.h"
#include "dsm_events_private.h"
#include "dsm_rules.h"
#include "dsm.h"
#include "dsm_errors.h"
#include "sss_sysid.h"

/*
 * File contains code for all the rule related stuff.
 */
extern int verbose;
extern struct event_private *Pevent_head;
extern struct event_private *Pevent_tail;

/*
 * Counter for number of rules in memory.
 */
int IGtotal_rules=0;

/*
 * Pointers to all the rules in memory. This is a doubly linked list of all the
 * rules.
 */
struct rule *Prule_head=NULL,*Prule_tail=NULL;
static int num_rules_hash;
static struct rule **PPrules_hash;

/*
 * Hash table to find the rule given the objid. The objid refers to the value
 * in the action_id field of the "event_action" table for any rule.
 */
static __uint64_t dsm_create_rules_hash()
{
  __uint64_t err=DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_ALLOC_MEM);

  num_rules_hash = NUM_RULES_HASH;
  
  PPrules_hash=(struct rule **)
    sem_mem_alloc_perm(sizeof(struct rule *)*num_rules_hash);
  if (!PPrules_hash)
    return err;
  return 0;
}

/*
 * Hash table returns the key, given the objid.
 */
static int dsm_rule_hash_key(int objid)
{
  return objid%num_rules_hash;
}

/*
 * More hash table stuff. Find the rule in the hash table given the object id.
 * Return the rule pointer in PPrule.
 */
__uint64_t dsm_find_rule_hash(int objid,struct rule **PPrule)
{
  int index;
  struct rule *Prule_temp;

  if(!PPrule || !objid)
    {				/* silly check. */
      *PPrule=NULL;
      return 0;
    }

  if(!num_rules_hash)
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_FIND);

  index=dsm_rule_hash_key(objid); /* get the index into the hash table. */
  *PPrule=NULL;
  Prule_temp=PPrules_hash[index]; /* get the first rule at this index 
				   * in the hash table. 
				   */
  while(Prule_temp && (Prule_temp->rule_objid != objid))
    Prule_temp=Prule_temp->Phash;
  
  if(Prule_temp && Prule_temp->rule_objid == objid)
    {				/* found it!. */
      *PPrule=Prule_temp;
      (*PPrule)->refcnt++;	/* Increment reference count. */
      return 0;
    }
  
  return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_FIND);
}

/*
 * More hash table stuff. Add a new rule to the hash table.
 */
static __uint64_t dsm_add_rule_hash(struct rule *Prule)
{
  int index;
  struct rule *Prule_temp;

  if(!num_rules_hash || !Prule)
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_INVALID);

#if RULES_DEBUG
  assert(!Prule->Phash);
#endif

  index=dsm_rule_hash_key(Prule->rule_objid);
  Prule_temp=PPrules_hash[index];

  /*
   * Check for duplicate entries.
   */
  while(Prule_temp && Prule_temp->rule_objid != Prule->rule_objid)
    Prule_temp = Prule_temp->Phash;
  
  if(Prule_temp)
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_DUP);

  /*
   * Add the rule in the hash table at index.
   */
  Prule->Phash=PPrules_hash[index];
  PPrules_hash[index]=Prule;
  return 0;
}

/*
 * Delete a rule in the hash table.
 */
static __uint64_t dsm_delete_rule_hash(struct rule *Prule)
{
  int index;
  struct rule *Prule_temp,**PPev_rule;

  if(!Prule)			/* silly check. */
    return 0;

  index=dsm_rule_hash_key(Prule->rule_objid);

  Prule_temp=PPrules_hash[index];
  PPev_rule=&PPrules_hash[index];

  /*
   * Check for rule entry.
   */
  while(Prule_temp && Prule_temp->rule_objid != Prule->rule_objid)
    {
      /* Found it. */
      PPev_rule  = &Prule_temp->Phash;
      Prule_temp = Prule_temp->Phash;
    }

  if(!Prule_temp)
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_INVALID);

  /*
   * Finally add the rule to the hash table.   
   */
  *PPev_rule=Prule_temp->Phash;
  Prule_temp->Phash=NULL;

  return 0;
}

/*
 * Boolean function which returns true if the objid in Pevent_rule is the same
 * as the rule's objid.
 */
static int dsm_isevent_rule(struct event_rules *Pevent_rule,void *Pobjid)
{
  if(!Pobjid || !Pevent_rule)
    return 0;			/* silly check */

  return Pevent_rule->index_rule == *(int *)Pobjid;
}

/*
 * Function finds the event_rule structure pointing to the same objid as Prule
 */
static int dsm_isrule_in_event(struct event_private *Pev,
			struct rule *Prule)
{
  if(!Prule || !Pev || !RUN(Pev)) /* silly check */
    return 0;

  if(FINDEVENT(RUN(Pev)->Prule_head,NULL,dsm_isevent_rule,&Prule->rule_objid))
    return 1;
  return 0;
}

/*
 * Returns any rule having no events applicable to it in memory at present
 * and having the lowest refcnt.
 */
void dsm_is_noevent_rule(struct rule *Prule, struct rule **PPrule)
{
  struct event_rules *Pevr_temp=NULL;

  if(!PPrule || !Prule)
    return;			/* silly check. */

  /*
   * Look at all events.
   */
  if(FINDEVENT(Pevent_head,NULL,dsm_isrule_in_event,Prule))
    return;

  /*
   * Is this the first rule found ?.
   */
  if(!*PPrule)
    {
      *PPrule=Prule;
      return;
    }

  /*
   * Now check the usage reference counts. This just does a "least used"
   * algorithm.
   */
  if((*PPrule)->refcnt < Prule->refcnt)
    *PPrule=Prule;

  return;
}

/*
 * Assume whoever calls this function has also acquired the rules and event 
 * write locks.
 * Delete the rule<->event link.
 */
static __uint64_t dsm_delete_event_rule(struct event_rules *Pevr,
					struct event_private *Pev)
{
  int i;
  struct rule *Prule=NULL;
  __uint64_t err;

  if (!Pevr || !Pev || !RUN(Pev))
    return 0;
  
  DELETEEVENT(&RUN(Pev)->Prule_head,&RUN(Pev)->Prule_tail,Pevr);
  sem_mem_free(Pevr);  

  return 0;
}


/*
 * Free up the event_rules structure.
 */
void dsm_free_event_rule(struct event_private *Pev,int *Pobjid)
{
  int j;
  struct event_rules *Pevr_temp;
  
  if(!Pev || !RUN(Pev) || !Pobjid)
    return;

  if(Pevr_temp=(struct event_rules *)FINDEVENT(RUN(Pev)->Prule_head,NULL,
					       dsm_isevent_rule,Pobjid))
    dsm_delete_event_rule(Pevr_temp,Pev);

  return;
}

/*
 * Add a new event_rules structure to the event. If one already exists.... 
 * just update that.
 */
static __uint64_t dsm_init_event_rule(struct event_private *Pev,int objid)
{
  int j;
  struct event_rules *Pevr_temp;

  if(!Pev || !RUN(Pev) || !objid)
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_INVALID_ARG);

  /*
   * Find if one event_rule structure already exists for this objid.
   */
  if(!(Pevr_temp=(struct event_rules *)FINDEVENT(RUN(Pev)->Prule_head,NULL,
						 dsm_isevent_rule,&objid)))
    {    
      /*
       * Could not find any event_rule structure, so just allocate one.
       */
      Pevr_temp=sem_mem_alloc_temp(sizeof(struct event_rules));
      if(!Pevr_temp)
	return DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_ALLOC_MEM);
      ADDEVENT(RUN(Pev)->Prule_head,RUN(Pev)->Prule_tail,Pevr_temp);
      Pevr_temp->index_rule = objid;
      Pevr_temp->thresholding_window=FALSE;
      Pevr_temp->event_counts=Pevr_temp->event_times=0;
    }

  return 0;
}  
  
/*
 * Free the action_string structure in the rule. This is needed since the 
 * action_string structure is not kept in memory always. It is read in from
 * the ssdb when required and freed immediately after using it.
 */
void dsm_free_action_in_rule(struct rule *Prule)
{
  int j;

  if(!Prule || !Prule->Paction)	/* Silly check. */
    return;

  /*
   * Free the action.
   */
  if(Prule->Paction->action)
    sem_mem_free(Prule->Paction->action);

  /*
   * Free all the environment variables.
   */
  if(Prule->Paction->envp)
    {
      for(j=0;Prule->Paction->envp[j];j++)
	sem_mem_free(Prule->Paction->envp[j]);
      sem_mem_free(Prule->Paction->envp);
    }

  /* 
   * Free the user-id string if any.
   */
  if(Prule->Paction->user)
    string_free(Prule->Paction->user,strlen(Prule->Paction->user));

  /*
   * Free the action_string structure and null out the Paction field.
   */
  sem_mem_free(Prule->Paction);
  Prule->Paction=NULL;
}

void dsm_free_rule_contents(struct rule *Prule)
{
  /*
   * Free forward_hname in rule 
   */
  if ( Prule->forward_hname ) sem_mem_free(Prule->forward_hname);

  dsm_free_action_in_rule(Prule);
}

/*
 * Free up the rule structure. Also break the links from any events to the
 * rule. flag is an unused argument. The only reason it is around is because
 * this function can be used with the TRAVERSERULES macro.
 */
static void dsm_free_rule(struct rule *Prule,void *flag)
{
  int j;
  struct event_private *Pev;
  struct event_rules *Pevr;  

  /*
   * Free the event_rules structures.
   *
   * Since the pointers to the events are no longer available from the rule..
   * look at each event if the rule exists for the event.
   */
  dsm_check_delete_event_rules_ssdb(Prule->rule_objid);

  dsm_free_rule_contents(Prule);

  /*
   * Finally free the rule.
   */
  sem_mem_free(Prule);
}

/*
 * Initialize anything rule specific when running as threads.
 */
__uint64_t dsm_thread_initialize_rules()
{
  return 0;
}

/*
 * Return the rule from the set of rules having no events pointing to them
 * in memory and having the least usage so far.
 */
static __uint64_t dsm_find_noevent_rule(struct rule **PPrule)
{
  if(!PPrule)			/* silly check */
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_INVALID_ARG);
  
  *PPrule=NULL;

  if(Prule_head)
    TRAVERSERULES(Prule_head,NULL,dsm_is_noevent_rule,PPrule);

  if(!*PPrule)
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_FIND);

  return 0;
}

/* 
 * Link a new rule to the rest of the rules in memory. 
 */
__uint64_t dsm_add_rule(struct rule *Prule)
{
  struct rule *Prule_temp;
  __uint64_t err;

  if(!Prule)			/* silly check */
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_INVALID_ARG);

  /*
   * Make sure we don't already have this rule in memory.
   */
  if(!(err=dsm_find_rule_hash(Prule->rule_objid,&Prule_temp)) &&
       !(DSM_ERROR_MINOR(err) & DSM_MIN_RULE_INVALID))
    {
      if(err)
	return err;
      else
	return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_RULE_DUP);
    }

  /*
   * Link this rule to the rest of the rules.
   */
  ADDRULE(Prule_head,Prule_tail,Prule);
  IGtotal_rules++;		/* bump the total rules counter. */
  err=dsm_add_rule_hash(Prule);	/* add the rule to the hash table. */
  
  return err;
}

/* 
 *  Delete a rule. 
 *  Unlink the rule from the rest of the rules. Also, free up the memory 
 *  allocated to it.
 */
__uint64_t dsm_delete_rule(struct rule *Prule)
{
  struct rule *Prule_temp;
  __uint64_t err;

  if(!Prule)			/* silly check */
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_INVALID_ARG);

  /*
   * Find the pointer to the rule that is part of the list of rules.
   */
  if(err=dsm_find_rule_hash(Prule->rule_objid,&Prule_temp))
    {
      return err;
    }

  DELETERULE(&Prule_head,&Prule_tail,Prule);
  IGtotal_rules--;		   /* decrement global counter */
  err=dsm_delete_rule_hash(Prule); /* delete rule from hash table. */
  
  dsm_free_rule(Prule,NULL);	   /* free up the memory for this rule. */

  return err;
}

/*
 * Find the rule with the given objid. If not in our memory read it from the
 * ssdb.
 */
__uint64_t dsm_find_rule(int objid,struct rule **PPrule)
{
  __uint64_t err;

  if(!PPrule)			/* silly check! */
    return DSM_ERROR(DSM_MAJ_RULE_ERR,DSM_MIN_INVALID_ARG);

  /*
   * First look for the rule in memory.
   */
  if(!(err=dsm_find_rule_hash(objid,PPrule)))
    return 0;

#if CACHE_RULES
  /*
   * Rule was not found in memory. 
   * So see if any rules need to be thrown out and if we have any to throw out.
   */
  while((IGtotal_rules >= MAX_RULES_MEMORY) && !dsm_find_noevent_rule(PPrule)
	&& (*PPrule))
    {
#if (RULES_DEBUG > 1) 
      fprintf(stderr,"Throwing Rule O=%d Out!.\n",(*PPrule)->rule_objid);
#endif	      
      dsm_delete_rule(*PPrule);
    }
  /*
   * Read and initialize the rule.
   */
  if(!(err=dsm_init_rule(objid,PPrule)))
    return 0;
#endif

  return err;
}

/*
 * Assume whoever calls this function has also acquired the rules and event
 * write locks.
 * Add a new rule for an event.
 */
__uint64_t dsm_add_eventrule_to_event(int objid,struct event_private *Pev)
{
  /*
   * Find event first. If it is not in memory just ignore... since the rule
   * will be added to the event whenever the event gets read in later from
   * the ssdb. This assumes, that the event in the ssdb has been updated to 
   * reflect this.
   */
  if(Pev && RUN(Pev))
    dsm_init_event_rule(Pev,objid); /* add event_rule structure to event. */

  return 0;
}

/*
 * Release the event<->rule link for all the rules applicable to this event.
 */
void dsm_delete_event(struct event_private *Pev)
{
  if(!Pev || !RUN(Pev))			/* Silly check. */
    return;

  /*
   * Free up all the event_rules structures.
   */
  TRAVERSEEVENTS(RUN(Pev)->Prule_head,NULL,dsm_delete_event_rule,Pev);

  /*
   * Free up the event data if any.
   */
  if(Pev->DSMDATA.Pevent_data)
    {
      sem_mem_free(Pev->DSMDATA.Pevent_data);
    }

  /*
   * Free up the hostname field if any.
   */
  if(Pev->DSMDATA.hostname)
    {
      string_free(Pev->DSMDATA.hostname,SSSMAXHOSTNAMELEN);
    }

  if(RUN(Pev) && RUN(Pev)->action_taken)
  {
      sem_mem_free(RUN(Pev)->action_taken);
      RUN(Pev)->action_taken=NULL;
      RUN(Pev)->action_time=0;
  }
  Pev->DSMDATA.Pevent_data=Pev->DSMDATA.hostname=NULL;
}

/*
 * Free up all the allocated memory for the rules.
 */
void dsm_rules_clean()
{
				/* free each rule. */
  TRAVERSERULES(Prule_head,NULL,dsm_free_rule,NULL);
  sem_mem_free(PPrules_hash);	/* free hash table. */
}

#if RULES_DEBUG
/*
 * Dump the contents of the rules.
 */
void dump_rule(struct rule *Prule,void *flag)
{
  int i;

  printf("Objid=%d\nCount=%d,Time=%d,Timeout=%d,Retry=%d\nAction=%s\n",
	 Prule->rule_objid,Prule->event_threshold_counts,
	 Prule->event_threshold_times,Prule->Paction->timeout,
	 Prule->Paction->retry,Prule->Paction->action);
  
  
  printf("Host=%s\n",Prule->hostname);

  printf("User = %s\n",Prule->Paction->user);

  i=0;
  while(Prule->Paction->envp[i])
    printf("Env = %s\n",Prule->Paction->envp[i++]);
}
#endif

/*
 * Initialize and allocate data structures for rule specified by objid.
 */
__uint64_t dsm_init_rule(int objid,struct rule **PPrule)
{
  int i;
  __uint64_t err;
  struct rule *Prule;

  /*
   * Allocate the rule structure.
   */
  if(!(Prule=sem_mem_alloc_temp(sizeof(struct rule))))
    {
      return err=DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_ALLOC_MEM);
    }
  
  /*
   * Do any initialization for the rule.
   * Read rule specific stuff from SSDB.
   */
  bzero(Prule,sizeof(struct rule));
  err=dsm_init_rule_ssdb(Prule,objid);
  
  if(err)
    {
      sem_mem_free(Prule);
      if(verbose >= VERB_DEBUG_2)
	fprintf(stderr,"DSM_EVENT: Failed to initialize rule objid = %d\n",
		objid);
      
      return err;
    }
  /*
   * Link the rule to the rest of the rules.
   */
  dsm_add_rule(Prule);

  /* 
   * If the use wanted a pointer to the rule, pass the pointer back
   * to the rule.
   */
  if(PPrule)			
    *PPrule=Prule;

  return 0;
}

/*
 * Called at init time once for all rules.
 * Make the events point to the applicable rules.
 */
__uint64_t dsm_init_all_rules()
{
  __uint64_t err=0;
  int i;
  struct rule *Prule;

  /*
   * Init hash table.
   */
  if(err=dsm_create_rules_hash())
    return err;

  Prule_head=Prule_tail=NULL;

  return err;
}

