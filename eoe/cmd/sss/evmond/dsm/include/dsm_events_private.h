#ifndef _DSM_EVENTS_PRIVATE_H
#define _DSM_EVENTS_PRIVATE_H

#include <events_private.h>
#define SEPARATE_FLAGS 0
#if SEPARATE_FLAGS
#define EVENTS_DEBUG 0
#endif
typedef unsigned char BOOLEAN;

/*
 * This structure contains all the data necessary to update the record in
 * the ACTION_TAKEN table of the SSDB.
 */
struct action_taken {
  int event_class;
  int event_type;
  
  int event_id;	    /* the event record-id which came from SSDB */
  char *hostname;   /* hostname of machine from which event originated. */
  char *action;     /* action which was taken. */
};

#endif


