#include "options.h"

#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>		/* For varargs */
#include <stdlib.h>		/* For string conversion routines */
#include <string.h>
#include <bstring.h>
#include <ctype.h>		/* For character definitions */
#include <errno.h>
#include <ulocks.h>
#include <time.h>

#include "debug.h"
#include "fcagent.h"
#include "event.h"

/*
 ******************************************************************************
 * ev_init() 
 ******************************************************************************
 */
int ev_init()
{
  int i;

  for (i = 0; i < MAX_EVENTS; ++i)
    EV_ARRAY[i] = NULL;
  EV_HEAD_I = 0;
  EV_TAIL_I = 0;
  if ((EV_LOCK = usnewlock(ARENA)) == NULL)
    return(-1);
  if ((EV_AVAIL = usnewsema(ARENA, 0)) == NULL)
    return(-1);
  return(0);
}

/*
 ******************************************************************************
 * ev_cleanup() 
 ******************************************************************************
 */
void ev_cleanup()
{
  if (EV_LOCK)
    usfreelock(EV_LOCK, ARENA);
  if (EV_AVAIL)
    usfreesema(EV_AVAIL, ARENA);
}

/*
 ******************************************************************************
 * ev_count() 
 ******************************************************************************
 */
int ev_count()
{
  return((EV_TAIL_I <= EV_HEAD_I) ? 
           (EV_HEAD_I - EV_TAIL_I) :
	   (EV_HEAD_I + MAX_EVENTS - EV_TAIL_I));
}

/*
 ******************************************************************************
 * ev_enqueue() 
 ******************************************************************************
 */
void ev_enqueue(event_t ev)
{
  int i;

  /* 
   * Fill in remaining fields -- caller will have filled in
   * ev_cntrl_id, ev_encl_id, ev_elem_type, ev_elem_id, ev_old_status
   * and ev_new_status.  
   */
  ev->ev_id = EV_HEAD_I;
  ev->ev_timestamp = time(NULL);
  switch (ev->ev_new_status) {
  case EVT_ELEM_STS_OK:
    ev->ev_type = EVT_TYPE_INFO;
    break;
  case EVT_ELEM_STS_OFF:
  case EVT_ELEM_STS_NOT_PRESENT:
  case EVT_ELEM_STS_BYPASSED:
    ev->ev_type = EVT_TYPE_CONFIG;
    break;
  case EVT_ELEM_STS_FAILED:
  case EVT_ELEM_STS_PEER_FAILED:
    ev->ev_type = EVT_TYPE_FAILURE;
    break;
  }

  if (__debug && D_EVENT) {
    printf("ev_enqueue: --\n");
    printf(  "ev_ch_id       %d\n", ev->ev_ch_id);
    printf(  "ev_encl_id     %d\n", ev->ev_encl_id);
    printf(  "ev_elem_type   %d\n", ev->ev_elem_type);
    printf(  "ev_elem_id     %d\n", ev->ev_elem_id);
    printf(  "ev_old_status  %d\n", ev->ev_old_status);
    printf(  "ev_new_status  %d\n", ev->ev_new_status);
  }

  LOCK(EV_LOCK);
  
  i = EV_HEAD_I % MAX_EVENTS;
  if (EV_ARRAY[i])
    fatal("ev_enqueue(): EV_ARRAY[%d] not NULL -- tail=%d, head=%d\n", 
	  i, EV_HEAD_I, EV_TAIL_I);
  EV_ARRAY[i] = ev;
  
  i = (++EV_HEAD_I) % MAX_EVENTS;
  if (EV_ARRAY[i]) {
    free(EV_ARRAY[i]);
    EV_ARRAY[i] = NULL;
    if (i == EV_HEAD_I)
      EV_TAIL_I = (EV_TAIL_I+1) % MAX_EVENTS;
  }
  else 
    V(EV_AVAIL);    

  UNLOCK(EV_LOCK);
}

/*
 ******************************************************************************
 * ev_dequeue()
 ****************************************************************************** 
*/
event_t ev_dequeue()
{
  event_t ev;

  P(EV_AVAIL);
  LOCK(EV_LOCK);

  ev = EV_ARRAY[EV_TAIL_I];
  EV_ARRAY[EV_TAIL_I] = NULL;
  EV_TAIL_I = (EV_TAIL_I+1) % MAX_EVENTS;

  UNLOCK(EV_LOCK);

  if (__debug && D_EVENT) {
    printf("ev_dequeue: --\n");
    printf(  "ev_ch_id       %d\n", ev->ev_ch_id);
    printf(  "ev_encl_id     %d\n", ev->ev_encl_id);
    printf(  "ev_elem_type   %d\n", ev->ev_elem_type);
    printf(  "ev_elem_id     %d\n", ev->ev_elem_id);
    printf(  "ev_old_status  %d\n", ev->ev_old_status);
    printf(  "ev_new_status  %d\n", ev->ev_new_status);
  }

  return(ev);
}

