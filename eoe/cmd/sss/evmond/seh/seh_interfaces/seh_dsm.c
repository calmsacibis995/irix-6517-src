/*
 * File which has code to interface with the dsm.
 * The seh<->dsm interface involves transferring control of the event to the
 * dsm from the seh.
 */
#include "common.h"
#include "events.h"
#include "events_private.h"
#include "seh.h"
#include "seh_errors.h"
/*#include "msglib.h" */
#include "sss_pthreads.h"

/*
 * Pointer to our internal queue structure.. in case of pthreads.
 */
list_of_elements_t sehtodsmq;

/* 
 * This is used only when SEM enters archive mode at present.
 * The way this works is that as soon as archive mode is entered, the SEH
 * starts dropping all events. Then only error events may be sent to the DSM.
 * In archive mode error events are dropped silently by the DSM. 
 * See also SEH_CHECK_STATE and DSM_CHECK_STATE.
 * 
 * Doing this also ensures that the alarm handler has no work to do.
 */
void seh_flush_sehdsmq()
{
  while(sss_pthreads_get_count(&sehtodsmq))
    {
      sginap(2);
    }
  sginap(5);			/* Give enough context switches */
  sginap(5);			/* so DSM can acquire the ruleevent lock */
  sginap(5);			/* if it is already handling an event. */
  sginap(5);
}

/*
 * Initialize the interface to the DSM. In case of pthreads using the 
 * memory based queue do nothing.
 */
__uint64_t seh_dsm_setup_interfaces()
{
  __uint64_t err=SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_INIT_DSM);

  /* 
   * Initialize the interface. In case of pthreads the interface is 
   * a structure containing the events and a read-write lock/mutex
   * to lock the relevant interface.
   */
  sss_pthreads_initq(&sehtodsmq);

  return 0;
}

/*
 * Tell the event to the DSM. 
 */
__uint64_t seh_tell_event_dsm(struct event_private *Pev,
			      struct event_record *Pev_rec)
{
  int c;
  char *sendmsg;
  DSMEvent_t *Pevent;	/* structure used to communicate between SEH & DSM. */

  if(!Pev)		/* Silly check. */
    return 0;

  sendmsg=calloc(sizeof(char)*MAX_MESSAGE_LENGTH,1);
  if(!sendmsg)
    return SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_ALLOC_MEM);

  /*
   * Fill the DSMEvent_t structure with 
   *   o magic number, version number and buffer pointer.
   *   o the information in the event structure.
   */
  Pevent=(DSMEvent_t *)sendmsg;	        /* send buffer */
  Pevent->LLsys_id=Pev->sys_id;
  Pevent->LEventMagicNumber=DSM_EVENT_MAGIC_NUMBER;
  Pevent->iEventClass=Pev->event_class;	/* class */
  Pevent->iEventType=Pev->event_type;   /* type */
  Pevent->iEventCount=Pev->SEHDATA.number_events;
  Pevent->iPriority=RUN(Pev)->priority;	/* priority value. */
  Pevent->iVersion=EVENT_VERSION_1_1;
  Pevent->iTimeStamp=RUN(Pev)->time_stamp;

  /* 
   * record id in the "event" table.
   */
  if(Pev_rec && Pev_rec->valid_record == TRUE)
    Pevent->LEvent_id=Pev->SEHDATA.event_id;
  else
    Pevent->LEvent_id=0;

  /*
   * hostname from which event originated.
   */
  if(Pev->SEHDATA.hostname)
    memcpy(Pevent->strHost,Pev->SEHDATA.hostname,SSSMAXHOSTNAMELEN);
  Pevent->iEventDataLength=Pev->SEHDATA.event_datalength;
  /*
   * Event data.
   */
  assert(!Pev->SEHDATA.event_datalength || Pev->SEHDATA.Pevent_data);

  if(Pev->SEHDATA.Pevent_data)
#if STREAM_SOCK_MSGLIB
    {				
      /*
       * Data is not a static buffer in case of stream sockets..
       * It is an allocated buffer. Just pass the pointer to the
       * allocated buffer in this case. Thus save on some cpu cycles by
       * avoiding the memory to memory copy.
       */
      Pevent->ptrEventData=Pev->SEHDATA.Pevent_data;
      Pev->SEHDATA.Pevent_data=NULL;
    }
#else
    {
      assert((Pev->SEHDATA.event_datalength+sizeof(DSMEvent_t)-sizeof(char *)) 
	     <=  MAX_MESSAGE_LENGTH);
      memcpy((char *)&Pevent->ptrEventData,Pev->SEHDATA.Pevent_data,
	     Pev->SEHDATA.event_datalength);
    }
#endif

  /*
   * As mentioned before in case of pthreads the message passing is done by
   * means of an queue in memory. This is possible since both SEH and DSM 
   * share the same address space.
   */
  c=sss_pthreads_insert_element(&sehtodsmq,sendmsg);
  if(c<0)
    {
      free(sendmsg);
      return SEH_ERROR(SEH_MAJ_DSM_ERR,SEH_MIN_DSM_XMIT);
    }

  return 0;
}

/* 
 * Free up anything allocated for this interface.
 */
void seh_dsm_clean()
{
  sss_pthreads_exitq(&sehtodsmq);
}
