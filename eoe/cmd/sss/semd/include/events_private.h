#ifndef __EVENTS_PRIVATE_H
#define __EVENTS_PRIVATE_H 1

#include <common_events.h>

#define EVENTS_DEBUG       2
#define MAX_EVENTS_MEMORY  (10+NUM_SSS_EVENTS)
#define MAX_RULES_MEMORY   (20+NUM_SSS_RULES)
#define NUM_EVENTS_HASH    128
#define CACHE_EVENTS       1

/*
 * Few macros to make the code more intelligible.
 */
#define RUN(X)  ((X)->run)
#define SEHDATA run->sehdsm_data[0]
#define DSMDATA run->sehdsm_data[1]

/* 
 * Run-time variables per event. The RUN(X) macro above points to this.
 */
struct event_runtime {

  struct event_rules *Prule_head;/* rules for each event. */
  struct event_rules *Prule_tail;/* rules for each event. */

  char     *action_taken;	/* last action taken string. */
  int       action_time;	/* time last action was taken. */
  int       time_stamp;		/* time the event occurred. */
  int       refcnt;		/* total number of events we have processed. */

  int       event_counts;	/* current event counts and time when we are */
  int       event_times;	/* within a throttling window. */
  unsigned  msgKey;		/* last message key sent by sender */
  unsigned  priority;		/* the new priority field. */
  BOOLEAN   throttling_window;	/* Are we in throttling window ?. */

  /*
   * This structure, both the SEH and DSM need separate copies.
   * The SEHDATA and the DSMDATA macro's above point to this structure.
   */
  struct sehdsm_data_s {
    char      *hostname;	/* hostname where event occured */
    char      *Pevent_data;	/* Pointer to event data. */
    unsigned   event_id;	/* current ssdb record-id if we are in a */
				/* throttling window. */
    int        number_events;	/* number of events which this event */
				/* applies to. */
    int        event_datalength;/* Length of event data. */
  } sehdsm_data[2];
};

/* 
 *  This is the actual event structure. This points to anything else which
 *  is of interest to the event.
 *  This is also a node on the doubly-linked global list starting from
 *  Pevent_head and ending at Pevent_tail.
 */
struct event_private {
		/* Written to at the time of initialization only. */
  struct  event_private *Pnext;	
  struct  event_private *Pbefore; 

  struct  event_private *Pnext_hash; /* Pointer to next event in hash table. */
  struct  event_runtime *run;	     /* Pointer to run-time data structure. */

  int     event_class;
  int     event_type;		
  int     event_throttle_counts; /* Actual parameters of the throttle  */
  int     event_throttle_times;  /* window. */

  __uint64_t sys_id;	 	 /* the sys_id, from the system table (SSDB) */

  BOOLEAN isthrottle;		 /* TO THROTTLE OR NOT TO.. */
};

/*
 * Event specific record which gets written in the SSDB.
 * It's only use currently is to return status back to the sender of the
 * event.
 */
struct event_record {
  unsigned int event_id;
  int          event_class;
  int          event_type;
  int          event_count;
  BOOLEAN      valid_record;
};

/*
 * Event specific pointer to the rule(s). It is pointed to by the dsm_event
 * structure.
 */
#define DO_NO_ACTION 1
#define TAKE_ACTION  2
#define RECORD_SSDB  4

struct event_rules {
  struct event_rules *Pnext;
  struct event_rules *Pbefore; 

  int index_rule;		/* index of rule this struct points to. */
  /* Run-time variables per event. */
  int event_counts;		/* current event counts and time when */
				/* we are */
  int event_times;		/* within a throttling window. */
  BOOLEAN  thresholding_window; /* Are we in throttling window ?. */
};

#endif /* __EVENTS_PRIVATE_H */
