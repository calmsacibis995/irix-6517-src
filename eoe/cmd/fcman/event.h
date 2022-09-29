#ifndef _EVENT_H_
#define _EVENT_H_

#include "fcagent_structs.h"
#include "esi.h"

/* 
 * Event handler definitions 
 */
typedef struct {
  uint   ev_id;			/* Event ID */
  uint   ev_type;		/* Event type */
  time_t ev_timestamp;		/* Event timestamp */
  uint   ev_ch_id;		/* Channel ID */
  uint   ev_encl_id;		/* Enclosure ID */
  uint   ev_elem_type;		/* Event element type code */
  uint   ev_elem_id;		/* Event element ID */
  u_char ev_old_status;		/* Old status of element */
  u_char ev_new_status;		/* New status of element */
} event_struct_t;
typedef event_struct_t *event_t;

int  ev_init();
void ev_cleanup();
int  ev_count();
void ev_enqueue(event_t ev);
event_t ev_dequeue();

#define EVT_TYPE_INFO            0x00 /* Informational */
#define EVT_TYPE_CONFIG          0x01 /* Configuration change */
#define EVT_TYPE_FAILURE         0x02 /* Failure */

#define EVT_ELEM_TYPE_DISK       E_TYPE_DISK
#define EVT_ELEM_TYPE_PS         E_TYPE_PS
#define EVT_ELEM_TYPE_FAN        E_TYPE_FAN
#define EVT_ELEM_TYPE_UPS        E_TYPE_UPS
#define EVT_ELEM_TYPE_LCC        E_TYPE_LCC

#define EVT_ELEM_STS_OK          STS_OK
#define EVT_ELEM_STS_OFF         STS_OFF
#define EVT_ELEM_STS_FAILED      STS_FAILED
#define EVT_ELEM_STS_NOT_PRESENT STS_NOT_PRESENT
#define EVT_ELEM_STS_BYPASSED    STS_BYPASSED
#define EVT_ELEM_STS_PEER_FAILED STS_PEER_FAILED

#endif
