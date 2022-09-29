/*
 * File is responsible for interfacing to the dsm. The intricacies of the
 * communication layer are encapsulated within this file.
 */
#include "common.h"
#include "events_private.h"
#include "dsm_events.h"
#include "dsm_events_private.h"
#include "dsm_rules.h"
#include "dsm.h"
#include "dsm_errors.h"
#include "msglib.h"
#include "sss_pthreads.h"

extern int verbose;

/*
 * Pointer to our internal q structure.. in case of pthreads.
 */
extern list_of_elements_t sehtodsmq;

/*
 * Initialize the interface to the SEH. In case of pthreads using the 
 * memory based queue do nothing.
 */
__uint64_t dsm_seh_setup_interface()
{
  __uint64_t err=DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_INIT_SEH);

  return 0;
}

/*
 * Read in any available event from the seh->dsm interface. Block if no event
 * is available.
 */
__uint64_t dsm_get_event_seh(struct event_private *Pev)
{
  int c;
  char *recvmsg;
  DSMEvent_t *Pevent;

  /*
   * Do the call to get an element of the internal memory queue. This function
   * blocks on the queue until an event is available. It is important to 
   * remember that in this case the buffer received has been allocated by the
   * SEH and it is this functions responsibility to free the buffer.
   */
  c=sss_pthreads_get_element(&sehtodsmq,(void **)&recvmsg);
      
  Pevent=(DSMEvent_t *)recvmsg;

  /*
   * Some basic checking of the received message.
   * Did we get any data at all ?.
   * Did we get garbage ?.
   * Did we get the correct version number ?.
   */
  if(Pevent->LEventMagicNumber!=DSM_EVENT_MAGIC_NUMBER)
    {
      /*
       * Free the receive buffer. 
       */
      free(recvmsg);
      return DSM_ERROR(DSM_MAJ_SEH_ERR,DSM_MIN_SEH_GRB);
    }
  
  if(Pevent->iVersion != EVENT_VERSION_1_1)
    {
      /*
       * Free the receive buffer. 
       */
      free(recvmsg);
      return DSM_ERROR(DSM_MAJ_SEH_ERR,DSM_MIN_SEH_VER);
    }


  /*
   * Now get all the event data.
   */
  Pev->sys_id=Pevent->LLsys_id;	        /* sys_id */
  Pev->event_class=Pevent->iEventClass;	/* class */
  Pev->event_type=Pevent->iEventType;   /* type */
  Pev->DSMDATA.number_events=Pevent->iEventCount; /* number of events */
  
  /* 
   * event id for the record which came in. 
   */
  Pev->DSMDATA.event_id=Pevent->LEvent_id; 
  RUN(Pev)->priority=Pevent->iPriority;	/* priority field for the event. */
  RUN(Pev)->time_stamp=Pevent->iTimeStamp; /* time stamp */
  if(Pev->DSMDATA.hostname)
    {
      /*
       * free up the hostname for the previous event that came in which is still
       * sitting in our memory.
       */
      string_free(Pev->DSMDATA.hostname,SSSMAXHOSTNAMELEN);
      Pev->DSMDATA.hostname=NULL;
    }

  Pev->DSMDATA.hostname=string_dup(Pevent->strHost,SSSMAXHOSTNAMELEN);
  if(!Pev->DSMDATA.hostname)
    {
      /*
       * Free the receive buffer. 
       */
      free(recvmsg);
      return DSM_ERROR(DSM_MAJ_SEH_ERR,DSM_MIN_ALLOC_MEM);
    }
  
  if(Pev->DSMDATA.Pevent_data)
    {
      /*
       * free up the event data for the previous event that came in which is 
       * still sitting in our memory.
       */
      sem_mem_free(Pev->DSMDATA.Pevent_data);
      Pev->DSMDATA.Pevent_data=NULL;
    }
  /*
   * get the event data now.
   *   o event data length.
   *   o event data.
   */
  Pev->DSMDATA.event_datalength=Pevent->iEventDataLength;
  if(Pevent->iEventDataLength > 0)
    {
#if STREAM_SOCK_MSGLIB
      /* 
       * In case of stream sockets we get a pointer to an already allocated
       * data buffer for free. Just reuse the pointer and avoid a memcpy.
       */
      Pev->DSMDATA.Pevent_data=Pevent->ptrEventData;
#else      
      Pev->DSMDATA.Pevent_data=(char *)
	sem_mem_alloc_temp(sizeof(char)*Pevent->iEventDataLength);
      if(!Pev->DSMDATA.Pevent_data)
	{
	  string_free(Pev->DSMDATA.hostname,SSSMAXHOSTNAMELEN);
	  Pev->DSMDATA.hostname=NULL;
	  /*
	   * Free the receive buffer. 
	   */
	  free(recvmsg);
	  return DSM_ERROR(DSM_MAJ_SEH_ERR,DSM_MIN_ALLOC_MEM);
	}
      memcpy(Pev->DSMDATA.Pevent_data,&Pevent->ptrEventData,
	     Pevent->iEventDataLength);
#endif /* Stream sockets. */
    }
#if EVENTS_DEBUG
  if(verbose > VERB_DEBUG_2)
    fprintf(stderr,"Record id %d\n",Pevent->LEvent_id);
#endif

  /*
   * Free the receive buffer. 
   */
  free(recvmsg);
  return 0;
}

/*
 * Free up any resources we allocated for the seh->dsm interface.
 */
void dsm_seh_clean()
{
}
