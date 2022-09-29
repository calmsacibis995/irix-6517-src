#include "common.h"
#include "events.h"
#include "events_private.h"
#include "seh.h"
#include "seh_errors.h"
#include "sss_pthreads.h"
#include "sss_sysid.h"

#define NUM_EVENTS_TO_FREE_SINGLE_INSTANCE 100

BOOLEAN global_logging_flag=TRUE;
extern int verbose;

struct event_private *Pevent_head=NULL;
struct event_private *Pevent_tail=NULL;
struct event_private *Perror_event=NULL;
int IGtotal_events=0,IGtotal_active_events=0;

static struct event_private **PPevent_hash=NULL;
static int num_entries_hash;
static BOOLEAN global_throttling_flag=TRUE;

#if EVENTS_DEBUG
unsigned int num_success_throttle_events=0;
unsigned int num_time_throttling_closed=0;
unsigned int num_regular_throttling_closed=0;
#endif

static int seh_event_check_fatal_error(__uint64_t err)
{
#if EVENTS_DEBUG
  assert(SEH_ERROR_MAJOR(err) == SEH_MAJ_EVENT_ERR);
#endif
  return 0;
}

/*
 * Function which returns a hash key.
 */
__uint64_t fnv_hash(uchar_t *key, int len)
{
          uint32_t  val;                    /* current hash value */
	  uchar_t *key_end = key + len;
	  
	  /*
	   * Fowler/Noll/Vo hash - hash each character in the string
	   *
	   * The basis of the hash algorithm was taken from an idea
	   * sent by Email to the IEEE Posix P1003.2 mailing list from
	   * Phong Vo (kpv@research.att.com) and Glenn Fowler
	   * (gsf@research.att.com).
	   * Landon Curt Noll (chongo@toad.com) later improved on their
	   * algorithm to come up with Fowler/Noll/Vo hash.
	   *
	   */
	  for (val = 0; key < key_end; ++key) {
	    val *= 16777619;
	    val ^= *key;
	  }
	  
	  /* our hash value, (was: mod the hash size) */
	  return (uint64_t) val;
}


/*
 * Actual hash key for seh. 
 */
static int seh_event_hash_key(int type, __uint64_t sys_id)
{
  __uint64_t key=sys_id<<32|type;
  return fnv_hash((uchar_t *)&key,sizeof(__uint64_t)) % num_entries_hash;
}

/*
 * Called at initialization time only -- don't need it later.
 * Create the hash table.
 */
static __uint64_t seh_create_event_hash()
{
  __uint64_t err=SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_ALLOC_MEM);

  num_entries_hash = NUM_EVENTS_HASH;

  PPevent_hash=(struct event_private **)
    sem_mem_alloc_perm(sizeof(struct event_private *)*num_entries_hash);
  memset(PPevent_hash,0,sizeof(struct event_private *)*num_entries_hash);
  if (!PPevent_hash)
    return err;
  return 0;
}


/*
 * Function to add event to hash table.
 * Underneath mechanism is irrelevant to the modules above.
 */
static __uint64_t seh_add_event_hash(struct event_private *Pev) 
{
  int index;
  __uint64_t err;
  struct event_private *Pev_temp=NULL;

  if(!num_entries_hash)
    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_INVALID);

#if EVENTS_DEBUG
  assert(!Pev->Pnext_hash);
#endif
  index=seh_event_hash_key(Pev->event_type, Pev->sys_id);
  Pev_temp=PPevent_hash[index];
  /*
   * Check for duplicate entries.
   */
  while(Pev_temp && (Pev_temp->event_type  != Pev->event_type  ||
		     Pev_temp->sys_id != Pev->sys_id))
    Pev_temp = Pev_temp->Pnext_hash;
	
  if(Pev_temp)		/* duplicate events with same class/type */
    {
      return err=SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_DUP);
    }
  Pev->Pnext_hash=PPevent_hash[index];
  PPevent_hash[index]=Pev;
  return 0;
}

/*
 * Function to delete event from hash table.
 * Underneath mechanism is irrelevant to the modules above.
 */
static __uint64_t seh_delete_event_hash(struct event_private *Pev) 
{
  int index=seh_event_hash_key(Pev->event_type, Pev->sys_id);
  __uint64_t err;
  struct event_private *Pev_temp,**PPev_temp;

  Pev_temp=PPevent_hash[index];
  PPev_temp=&PPevent_hash[index];
  /*
   * Check for event entry.
   */
  while(Pev_temp && (Pev_temp->event_type  != Pev->event_type  ||
		     Pev_temp->sys_id != Pev->sys_id))
    {
      PPev_temp=&Pev_temp->Pnext_hash;
      Pev_temp = Pev_temp->Pnext_hash;
    }
	
  if(!Pev_temp)		/* no event with same sys_id/type */
    return err=SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_INVALID);

  *PPev_temp=Pev_temp->Pnext_hash;
  Pev_temp->Pnext_hash=NULL;
  return 0;
}

/*
 * Function which throttles an event by time.
 */
static void seh_time_throttle_event(struct event_private *Pev,time_t *Ptime)
{
  if(!RUN(Pev))
    return;
  
  if(RUN(Pev)->throttling_window == TRUE && 
     (Pev->isthrottle == FALSE                                 ||
      (Pev->event_throttle_counts        &&
       (RUN(Pev)->event_counts >= Pev->event_throttle_counts)) || 
      (Pev->event_throttle_times         &&
       ((*Ptime-RUN(Pev)->event_times) >= Pev->event_throttle_times))))
    {
      /*
       * Close throttling window. 
       */
      RUN(Pev)->throttling_window=FALSE;
#if EVENTS_DEBUG
      num_time_throttling_closed++;
#endif      

    }
  
  return;
}

/*
 * Time throttle all the events.
 * When we get here, we assume we have the lock on the events and rules.
 */
__uint64_t seh_time_throttle_all_events()
{
  struct event_private *Pev;
  time_t cur_time;
  int i;

  cur_time=time(NULL);

  /*
   * Iterate through all the events and check if any event's
   * throttling window needs to be closed.
   */
  TRAVERSEEVENTS(Pevent_head,NULL,seh_time_throttle_event,&cur_time);

  return 0;
}

/*
 * Check if the event is within the thresholding window for this rule.
 */
static int  seh_is_thresholding_window(struct event_rules *Pevr,
				       void *unused_flag)
{
  if(Pevr->thresholding_window == TRUE)
    return 1;

  return 0;
}

/*
 * Check if the event has runtime data structure allocated to it.
 */
static int  seh_is_noruntime_event(struct event_private *Pev,void *unused_flag)
{
  if(!RUN(Pev))
    return 1;

  return 0;
}

/*
 * Return an event to throw out.. with no active windows. This function
 * only checks for events which have the runtime data structure allocated to
 * the event but are not having the throttling or thresholding window
 * active.
 */
void seh_is_noactivewindow_event(struct event_private *Pev,
				 struct event_private **PPev)
{
  
  if(!PPev)			/* Silly check. */
    return;

  if(Pev->event_class == SSS_INTERNAL_CLASS)
      return;

  if(!RUN(Pev))
    return;

  /*
   * Now check if the event has anything within a throttling or
   * a thresholding window.
   */
  if(RUN(Pev)->throttling_window==TRUE)
    return;
  if(FINDEVENT(RUN(Pev)->Prule_head,NULL,seh_is_thresholding_window,NULL))
    return;

  /*
   * If this is the first event return it.
   */
  if(!*PPev)
    {
      *PPev=Pev;
      return;
    }

  /* 
   * Now check the reference counts for event. Implements the lru algorithm.
   * The algorithm gives equal weight to 
   * o the time when the last throttling window was opened for the event.
   * o the refcnt (usage) so far for the event.
   * Not perfect!.
   */
  if((RUN(*PPev)->refcnt+RUN(*PPev)->event_times) > 
     (RUN(Pev)->refcnt+RUN(Pev)->event_times))
    *PPev=Pev;

  return;
}

/*
 * Top-level function for finding the event with no active windows and having
 * the least usage.
 */
static __uint64_t seh_find_nonactive_event(struct event_private **PPev)
{
  if(!PPev)			/* silly check. */
    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);

  *PPev=NULL;

  TRAVERSEEVENTS(Pevent_head,NULL,seh_is_noactivewindow_event,PPev);
  
  if(!*PPev)
    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_FIND);

  return 0;
}

/*
 * Top-level function for finding the event with no runtime data allocated to
 * it.
 */
static __uint64_t seh_find_noruntime_event(struct event_private **PPev)
{
  if(!PPev)			/* silly check. */
    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);
  
  *PPev=(struct event_private *)FINDEVENT(Pevent_head,NULL,
					  seh_is_noruntime_event,NULL);
  
  if(!*PPev)
    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_FIND);

  return 0;
}

/*
 * Function to find the event data based on type.
 * The underneath mechanism to do so is not important as long as it is 
 * quick and efficient.
 *
 * This function only finds the event if the event is already in memory.
 */
__uint64_t seh_find_event_hash(int type, __uint64_t system_id,
			       struct event_private **PPev) 
{
  int index;
  __uint64_t err;
  struct event_private *Pev_temp=NULL;

  if(!num_entries_hash)
    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_FIND);

  index=seh_event_hash_key(type,system_id);
  *PPev=NULL;
  Pev_temp=PPevent_hash[index];
  while(Pev_temp && (Pev_temp->event_type  != type  ||
		     Pev_temp->sys_id != system_id))
    {
      Pev_temp = Pev_temp->Pnext_hash;
    }
  if(Pev_temp && Pev_temp->event_type == type && Pev_temp->sys_id == system_id)
    {
      *PPev=Pev_temp;
      return 0;
    }

  return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_FIND);
}

void seh_free_runtime_event(struct event_private *Pev)
{
  if(!Pev || !RUN(Pev))
    return;
  /*
   * No need to free Pevent_data as that is never allocated by SEH.
   */
  if(Pev->SEHDATA.hostname)
    string_free(Pev->SEHDATA.hostname,SSSMAXHOSTNAMELEN);
  Pev->SEHDATA.hostname=NULL;

  if(Pev->SEHDATA.Pevent_data)
    sem_mem_free(Pev->SEHDATA.Pevent_data);
  Pev->SEHDATA.Pevent_data=NULL;

  dsm_delete_event(Pev);
  if(RUN(Pev))
    sem_mem_free(RUN(Pev));
  RUN(Pev)=NULL;
  IGtotal_active_events--;

  return;
}  

void seh_free_event(struct event_private *Pev,void *flag)
{
  if(!Pev)			/* Silly check. */
    return;

  seh_free_runtime_event(Pev);
  sem_mem_free(Pev);
}


/*
 * Called at init time for each event
 */
__uint64_t seh_init_each_event(struct event_private *Pev)
{
  time_t cur_time;

  if(!Pev)
    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_INVALID_ARG);

  RUN(Pev)=sem_mem_alloc_temp(sizeof(struct event_runtime));
  if(!RUN(Pev))
    {
      return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_ALLOC_MEM);
    }

  RUN(Pev)->throttling_window=FALSE;
  IGtotal_active_events++;

  return 0;
}


/*
 * Find the seh_event structure given an event class and a type.
 */
__uint64_t seh_find_event(int class,int type, __uint64_t sys_id,
			  struct event_private **PPev)
{
  __uint64_t err=0;
  int i=0;

  /*
   * First see if this event is already in memory.
   */
  if(!(err=seh_find_event_hash(type,sys_id,PPev)))
    {
      /*
       * Allocate run-time data structure if the event does not already have
       * one allocated to it.
       */
      if(*PPev && !RUN(*PPev))	
	{
	  seh_init_each_event(*PPev);
	  /* 
	   * if there is an error here, just ignore it. Because there is 
	   * nothing obvious that can be done about it. Just hope that
	   * dsm_init_event_ssdb will not leave any dangling pointers.
	   */
	  dsm_init_event_ssdb(*PPev); 
	}
      RUN(*PPev)->refcnt++;	/* Increment reference count. */      

      return 0;
    }

  /*
   * See if we have reached our limit on events in memory. If so,
   * Delete as many events as we can until we hit the highwater mark.
   */
  while((IGtotal_active_events >= MAX_EVENTS_MEMORY) && 
	!seh_find_nonactive_event(PPev) && (*PPev))
    {
      /* 
       * Stop freeing memory after NUM_EVENTS_TO_FREE_SINGLE_INSTANCE events. 
       * Otherwise it will take too long.
       */
      if(i++ > NUM_EVENTS_TO_FREE_SINGLE_INSTANCE)
	break;

#if EVENTS_DEBUG
      if(verbose > VERB_DEBUG_1)
	fprintf(stderr,"Freeing Runtime for Event C=%d,T=%d Out!.\n",
		(*PPev)->event_class,
		(*PPev)->event_type);
#endif	      
      seh_free_runtime_event(*PPev);
    }

  while((IGtotal_events >= (2*MAX_EVENTS_MEMORY)) && 
	!seh_find_noruntime_event(PPev) && (*PPev))
    {
      /* 
       * Stop freeing memory after NUM_EVENTS_TO_FREE_SINGLE_INSTANCE events. 
       * Otherwise it will take too long.
       */
      if(i++ > NUM_EVENTS_TO_FREE_SINGLE_INSTANCE)
	break;

      /*
       * Write out the contents of the event to the db before deleting
       * the event.
       */
#if EVENTS_DEBUG
      if(verbose > VERB_DEBUG_1)
	fprintf(stderr,"Throwing Event C=%d,T=%d Out!.\n",(*PPev)->event_class,
		(*PPev)->event_type);
#endif	      
      seh_delete_event(*PPev);
    }
  /*
   * then allocate an event, read it in from the SSDB and insert into our 
   * event list.
   */
#if EVENTS_DEBUG
  if(verbose > VERB_DEBUG_1)  
    fprintf(stderr,"Reading Event C=%d,T=%d.\n",class,type);
#endif	      
  if(err=seh_find_event_ssdb(class,type,sys_id,PPev))
    {
      return err;
    }

  /*
   * Check again in memory.
   */
  if(!(err=seh_find_event_hash((*PPev)->event_type,
			       (*PPev)->sys_id,
			       PPev)))
    {
      /*
       * Allocate run-time data structure if the event does not already have
       * one allocated to it.
       */
      if(*PPev && !RUN(*PPev))	
	seh_init_each_event(*PPev);
      RUN(*PPev)->refcnt++;	/* Increment reference count. */      

      return 0;
    }

  return err;
}

/*
 * Initialization required for the event on the stack if seh/dsm is a thread.
 */
__uint64_t seh_thread_initialize(struct event_private *Pev)
{
    
    if(Pev)
      {
	memset(Pev,0,sizeof(struct event_private));
	seh_init_each_event(Pev);
      }

    return 0;
}
      
/*
 * Function is responsible for initializing all events.
 */
__uint64_t seh_init_all_events()
{
  __uint64_t err;
  struct event_private *Pev;
  system_info_t *Psys = NULL;
  __uint64_t system_id = 0;
  
  /*
   * Clear event global pointers.
   */
  Pevent_head=Pevent_tail=NULL;

  if ( err=sgm_get_sysid(&Psys, NULL) ) 
      return err;
  else 
      system_id = Psys->system_id;

  /*
   * Init hash table.
   */
  if(err=seh_create_event_hash())
    return err;

  /*
   * Initialize the error event.
   */
  if(!(Pev=sem_mem_alloc_temp(sizeof(struct event_private))))
    {
      return err=SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_ALLOC_MEM);
    }
  seh_initialize_error_event(Pev, system_id);
  seh_add_event(Pev);  

  /*
   * Initialize the configuration event.
   */
  if(!(Pev=sem_mem_alloc_temp(sizeof(struct event_private))))
    {
      return err=SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_ALLOC_MEM);
    }
  seh_initialize_config_event(Pev, system_id);
  seh_add_event(Pev);

  return 0;
}

/*
 * Link the event to the rest of the events in memory.
 */
__uint64_t seh_add_event(struct event_private *Pev)
{
  struct event_private *Pev_temp;
  __uint64_t err;

  if(!seh_find_event_hash(Pev->event_type,Pev->sys_id, &Pev_temp))
    {
      return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_DUP);
    }
  
  ADDEVENT(Pevent_head,Pevent_tail,Pev);
  err=seh_add_event_hash(Pev);
  IGtotal_events++;
  return err;
}

/*
 * UnLink the event from the rest of the events in memory. Also, free the event.
 */
__uint64_t seh_delete_event(struct event_private *Pev)
{
  struct event_private *Pev_temp;
  __uint64_t err;

  if(!Pev)
    return 0;

  if(err=seh_find_event_hash(Pev->event_type,Pev->sys_id,&Pev_temp))
    return err;
  DELETEEVENT(&Pevent_head,&Pevent_tail,Pev_temp);
  err=seh_delete_event_hash(Pev_temp);
  seh_free_event(Pev_temp,NULL);
  IGtotal_events--;
  return err;
}

/*
 * Main task of this function....
 *     update throttle counts and set flags in the seh_event structure.
 */
__uint64_t seh_throttle_event(struct event_private *Pev, int localflag)
{
  struct event_private *Pev_temp=NULL;
  __uint64_t err=0;
  time_t cur_time;

  /*
   * User has logging turned off. In this case the SEH acts as a conduit for
   * the event to the DSM. It does not do anything else to or with the event.
   */
  if(global_logging_flag == FALSE)
    return 0;

  /*
   * Freak event ?. Should never really happen. Check it anyway.
   * Class can be empty now so don't check for class.
   */
  if(!Pev->event_type)
    return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_INVALID);

  lock_ruleevent();

  /*
   * Find the event. If it is not in memory, find it in the ssdb.
   */
  err=seh_find_event(Pev->event_class,Pev->event_type, Pev->sys_id,
		     &Pev_temp);

  if(err || !Pev_temp || !RUN(Pev_temp))
    {
      unlock_ruleevent();
      if(err)
	return err;
      else
	return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_EV_INVALID);	
    }

  /* 
   * The class which came in could be empty or invalid. So update it.
   * Should be done everytime seh_find_event is called. 
   */
  Pev->event_class=Pev_temp->event_class;

#if EVENTS_DEBUG
  assert(Pev_temp->event_type  == Pev->event_type);
#endif

  cur_time=time(NULL);
  /*
   * Open throttling window... if the hostname is localhost only.
   */
  if(RUN(Pev_temp)->throttling_window == FALSE)
  {
      /* 
       *  First event after we opened the throttling window.
       *  Init the count and time.
       */
      RUN(Pev_temp)->event_counts=0;
      RUN(Pev)->event_times=RUN(Pev_temp)->event_times=cur_time;
      /*
       * Actually open the throttling window.
       */
      RUN(Pev)->throttling_window=RUN(Pev_temp)->throttling_window=TRUE; 
      /*
       * Create a new event record for the event.
       */
      seh_create_event_record_ssdb(Pev,Pev_temp);
  }
  
  /*
   * These three assignments *SHOULD* be done after opening the
   * throttling window (of course opening the throttle window
   * is done only if necessary).
   */
  RUN(Pev)->event_times=cur_time;
  RUN(Pev_temp)->event_counts+=Pev->SEHDATA.number_events;
  RUN(Pev)->event_counts=RUN(Pev_temp)->event_counts;
  RUN(Pev_temp)->msgKey=RUN(Pev)->msgKey;
  
  Pev->SEHDATA.event_id=Pev_temp->SEHDATA.event_id;
  
  if(Pev_temp->SEHDATA.hostname != NULL)
  {
      string_free(Pev_temp->SEHDATA.hostname,SSSMAXHOSTNAMELEN);
      Pev_temp->SEHDATA.hostname=NULL;
  }
  if(Pev->SEHDATA.hostname && Pev->SEHDATA.hostname[0])
  {
      Pev_temp->SEHDATA.hostname=string_dup(Pev->SEHDATA.hostname,
					    SSSMAXHOSTNAMELEN);
      if(!Pev_temp->SEHDATA.hostname)
	  return SEH_ERROR(SEH_MAJ_EVENT_ERR,SEH_MIN_ALLOC_MEM);	    
  }

  /*
   * We are never in throttling window if the event should never be
   * throttled. Also if event is not from local host. Return immediately.
   * else
   * check throttle count parameters. 
   * Close throttling window if throttle count parameters are
   * met.
   */
  if((global_throttling_flag == FALSE)             ||
     (Pev_temp->isthrottle == FALSE)               || 
     (Pev_temp->event_throttle_counts              &&
      RUN(Pev_temp)->event_counts >= Pev_temp->event_throttle_counts))
    {
      /*
       * Actually close the throttling window.
       */
      RUN(Pev)->throttling_window=FALSE;
      RUN(Pev_temp)->throttling_window=FALSE;
#if EVENTS_DEBUG
      num_regular_throttling_closed++;
#endif      
    }

  unlock_ruleevent();

#if EVENTS_DEBUG
  num_success_throttle_events++;
#endif
  return 0;
}

/*
 * Clean up the event stuff.
 */
void seh_events_clean()
{
  TRAVERSEEVENTS(Pevent_head,NULL,seh_delete_event,NULL);
  if(PPevent_hash)
    sem_mem_free(PPevent_hash);
}

/* 
 * Delete an event only if the event is not an sss defined event -- config,
 * error and archive event class.
 */
__uint64_t seh_delete_non_sss_event(struct event_private *Pev)
{
    /* sss defined event ?. */
    if(Pev->event_class == SSS_INTERNAL_CLASS)
	return 0;

  /* 
   * Free only the runtime data for the event for now.
   */
  seh_free_runtime_event(Pev);

  return 0;
}

/*
 * Free all the event data. All information is lost. Equivalent to the SEH
 * restarting from scratch.
 */
void seh_events_flush()
{
  TRAVERSEEVENTS(Pevent_head,NULL,seh_delete_non_sss_event,NULL);
}

/*
 * Check if we can send the event that arrived to the DSM. This depends
 *       on whether an error occurred before in the seh and 
 *       where the error occurred.
 */
__uint64_t seh_dsm_sendable_event(struct event_private *Pev,
				  __uint64_t err)
{
  
  switch(SEH_ERROR_MAJOR(err))
    {
    case(SEH_MAJ_EVENT_ERR):
      if (SEH_ERROR_MINOR(err) & (SEH_MIN_EV_INVALID|SEH_MIN_EV_FIND))
	return 0;
    case(SEH_MAJ_API_ERR):
    case(SEH_MAJ_ARCHIVE_ERR):
      return 0;
    }
  if(SEH_ERROR_MINOR(err) & SEH_MIN_ALLOC_MEM)
    return 0;
  return 1;
}

/*
 * Set global flag values.
 */
void seh_set_throttling(BOOLEAN value)
{
  global_throttling_flag=value;
}

/*
 * Set global flag values.
 */
void seh_set_logging(BOOLEAN value)
{
  global_logging_flag=value;
}

/*
 * Tell if the hostname given by hname is the local host on which we are 
 * executing.
 */
static int seh_local_host(char *hname)
{
  system_info_t *Psys;

  if(!hname)			/* silly check. */
    return 1;

  if(sgm_get_sysid(&Psys,hname))
    return 0;
  if(!Psys || !Psys->local)
    return 0;

  return 1;
}

