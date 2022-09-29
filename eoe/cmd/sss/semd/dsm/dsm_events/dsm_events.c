#include "common.h"
#include "events.h"
#include "events_private.h"
#include "dsm_events.h"
#include "dsm_events_private.h"
#include "dsm_rules.h"
#include "dsm.h"
#include "dsm_errors.h"
#include "sss_pthreads.h"

extern int verbose;
extern struct event_private *Pevent_head;
extern struct event_private *Pevent_tail;

int global_action_flag=TRUE;

#if EVENTS_DEBUG
unsigned int num_success_threshold_events=0;
unsigned int num_time_thresholding_closed=0;
unsigned int num_regular_thresholding_closed=0;
#endif

/*
 * The resolution of the time checking is always =
 *                     2*TIME_TO_WAKEUP_TO_THROTTLE + (a few milliseconds)
 *                     let's say to be on the safe side..
 *                     2*TIME_TO_WAKEUP_TO_THROTTLE + 1
 * Also there may be a variance in the resolution because of time taken to 
 * acquire the rule lock. This may or may not take too much time.... 
 * depending on whether configuration of rules is taking place and how much
 * time it takes to do the re-configuration of rules.
 *
 */

/* 
 * Function checks if error is a fatal error. If so, returns a 1.
 */
static int dsm_event_check_fatal_error(__uint64_t err)
{
  assert(DSM_ERROR_MAJOR(err) == DSM_MAJ_EVENT_ERR);
  return 0;
}

/*
 * Needed if DSM is running as a thread. 
 *        Function initializes rules and clears the fields in event Pev.
 */
__uint64_t dsm_thread_initialize(struct event_private *Pev)
{
  __uint64_t err=0;
  
  /*
   * Initialize the event.
   */
  seh_thread_initialize(Pev);
  /*
   * Initialize the rules.
   */
  dsm_thread_initialize_rules();

  return err;
}

/*
 * Throttle event of type Pev, given the current time.
 */
static void dsm_time_throttle_event(struct event_private *Pev,time_t *Ptime)
{
  int i,j;
  struct event_rules *Pevr;
  struct rule *Prule;
  __uint64_t err=0;
  struct action_taken act_rec;

  if(!Pev || !RUN(Pev) || !Ptime) /* Silly check */
    return;

  Pevr=RUN(Pev)->Prule_head;	/* Get rule head for event. */
  while(Pevr)
    {
      /*
       * Find rule applicable... as we need the thresholding parameters.
       * This may go out and re-read the rule from the disk.
       */
      err=dsm_find_rule(Pevr->index_rule,&Prule);
      if(err || !Prule)
	return;

      /*
       * Check thresholding parameters.
       */
      if((Pevr->thresholding_window == TRUE)  &&
	 (Prule->isthreshold == FALSE                                    ||
	  (Prule->event_threshold_times       &&
	   ((*Ptime-Pevr->event_times) >= Prule->event_threshold_times)) ||
	  (Prule->event_threshold_counts      &&
	   (Pevr->event_counts >= Prule->event_threshold_counts))))
	{
#if RULES_DEBUG
	  fprintf(stderr,"Closing C=%d T=%d, time=%d %d\n",
		  Pev->event_class,Pev->event_type,
		  *Ptime,Pevr->event_times);
#endif
	  Pevr->thresholding_window=FALSE;
#if EVENTS_DEBUG
	  num_time_thresholding_closed++;
#endif
	  /*
	   * Take the action.
	   */
	  dsm_take_action(Prule,Pev);
	}
      Pevr=Pevr->Pnext;
    }
}

/*
 * Time throttle the rest of the events.
 * *** Assumption when lock for the rules and events is held before we get 
 * *** here.
 */
__uint64_t dsm_time_throttle_all_events()
{
  struct event_private *Pev;
  time_t cur_time;
  
  /*
   * Silly case.
   */
  if(!Pevent_head || !Pevent_tail)
    {
      return 0;
    }

  /*
   * Get the current time. Just get it once.
   */
  cur_time=time(NULL);
  TRAVERSEEVENTS(Pevent_head,NULL,dsm_time_throttle_event,&cur_time);

  dsm_events_child_handler();

  return 0;
}

/*
 * Our main task ...
 *     throttle event and set flags in the dsm_event structure.
 */
__uint64_t dsm_throttle_rules_event(struct event_private *Pev)
{
  struct event_private *Pev_temp=NULL;
  struct event_rules *Pevr;
  struct rule *Prule;
  __uint64_t err=0;
  time_t cur_time;
  int i;

  /*
   * Silly check ?.
   */
  if(!Pev || !RUN(Pev) || (!Pev->event_class && !Pev->event_type))
    return DSM_ERROR(DSM_MAJ_EVENT_ERR,DSM_MIN_EV_INVALID);

  /* 
   * The event data written to by the two threads is separate and a copy exists
   * for each thread --- SEHDATA and DSMDATA (macros).
   */
  lock_ruleevent();	

  err=seh_find_event(Pev->event_class,Pev->event_type,Pev->sys_id,&Pev_temp);

  if(err || !Pev_temp || !RUN(Pev_temp))
  {
      unlock_ruleevent();
      return DSM_ERROR(DSM_MAJ_EVENT_ERR,DSM_MIN_EV_INVALID);
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
   * Open thresholding window for each rule applicable to this event.
   */
  Pevr=RUN(Pev_temp)->Prule_head;
  while(Pevr)
  {
      Prule=NULL;
      err=dsm_find_rule(Pevr->index_rule,&Prule);
      if(err || !Prule)
      {
	  unlock_ruleevent();
	  return err;
      }

      if(Pevr->thresholding_window == FALSE)
      {
	  /* 
	   * First event after the thresholding window for this rule is opened.
	   */
	  Pevr->event_counts=0;
	  Pevr->event_times=cur_time;
          /*
	   * Actually open the thresholding window.
	   */
	  Pevr->thresholding_window=TRUE;
      }
      Pevr->event_counts+=Pev->DSMDATA.number_events;
      Pevr=Pevr->Pnext;
  }

  Pev_temp->DSMDATA.event_id=Pev->DSMDATA.event_id;

  if(Pev_temp->DSMDATA.hostname)
  {
      string_free(Pev_temp->DSMDATA.hostname,SSSMAXHOSTNAMELEN);
      Pev_temp->DSMDATA.hostname=NULL;
  }
  Pev_temp->DSMDATA.hostname=Pev->DSMDATA.hostname;
  Pev->DSMDATA.hostname=NULL;	/* 
				 * this clear is important so this field is 
				 * not freed later. ..in dsm_get_event_seh..
				 */

  if(Pev_temp->DSMDATA.Pevent_data)
  {
      sem_mem_free(Pev_temp->DSMDATA.Pevent_data);
      Pev_temp->DSMDATA.Pevent_data=NULL;
  }

  Pev_temp->DSMDATA.Pevent_data=Pev->DSMDATA.Pevent_data;
  Pev_temp->DSMDATA.event_datalength=Pev->DSMDATA.event_datalength;  
  Pev->DSMDATA.Pevent_data=NULL;  /* 
  Pev->DSMDATA.event_datalength=0; * this clear is important so this field is 
                                   * not freed later. ..in dsm_get_event_seh..
				   */
  RUN(Pev_temp)->priority=RUN(Pev)->priority; /* Get the priority now. */
  RUN(Pev_temp)->time_stamp=RUN(Pev)->time_stamp;


  /*
   * Close thresholding window for each rule applicable to this event if 
   * threshold count parameters are met. Similar thing needs to be done for
   * the thresholding by time case too.
   */
  Pevr=RUN(Pev_temp)->Prule_head;
  while(Pevr)
  {
      err=dsm_find_rule(Pevr->index_rule,&Prule);
      if(err)
      {
	  unlock_ruleevent();
	  return err;
      }

      /*
       * Never close this window if the event should never be thresholded.
       */
      if(Prule->event_threshold_counts && 
	 Pevr->event_counts >= Prule->event_threshold_counts)
      {
	  /* 
	   * Close the thresholding window.
	   */
	  Pevr->thresholding_window=FALSE;
#if EVENTS_DEBUG
	  num_regular_thresholding_closed++;
#endif
	  dsm_take_action(Prule,Pev_temp);
      }
      Pevr=Pevr->Pnext;
  }
  
  unlock_ruleevent();

#if EVENTS_DEBUG
  num_success_threshold_events++;
#endif

  return 0;
}

/*
 * Clean anything which is local to this file.
 */
void dsm_events_clean()
{
  return;
}

/*
 * Harvest any dead or dying children. Do not wait for them though.
 */
void dsm_events_child_handler()
{
  dsm_harvest_children();
}

/*
 * No actions will be initiated if this function is called.
 */
void dsm_set_action(int value)
{
  if(value)
    global_action_flag=TRUE;
  else
    global_action_flag=FALSE;
}
