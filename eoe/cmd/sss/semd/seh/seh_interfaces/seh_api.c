#include "common.h"
#include "events.h"
#include "events_private.h"
#include "seh.h"
#include "seh_errors.h"
#include "semapi.h"
#include "semapisrv.h"

/*
 * setup the interfaces with the api to the tasks. The seh initializes the
 * interface and just waits on input from the task. The input is in the form
 * of events.
 */
__uint64_t seh_api_setup_interfaces()
{
    __uint64_t err=SEH_ERROR(SEH_MAJ_INIT_ERR,SEH_MIN_INIT_API);

    if(!semsrvInit())
	return err;

    return 0;
}

/*
 * Function receives the event to be registered from the API.
 */
__uint64_t seh_get_event_api(struct event_private *Pev)
{
    int c;
    static char recvmsg[MAX_MESSAGE_LENGTH*20];
    static EVENTBLK ev;

    do 
	{
	    memset(&ev,0,sizeof(EVENTBLK));
	    ev.struct_size = sizeof(EVENTBLK);
	    ev.msgsize = sizeof(recvmsg);
	    ev.msgbuffer = recvmsg;
	}
    while (!semsrvGetEvent(&ev));

  /*
   * Some basic checking of the received message.
   */
  if(ev.eclass == SSS_INTERNAL_CLASS && ev.etype == ERROR_EVENT_TYPE)
      return SEH_ERROR(SEH_MAJ_API_ERR,SEH_MIN_API_GRB);

  Pev->event_class           = ev.eclass; /* class */
  Pev->event_type            = ev.etype;  /* type */
  RUN(Pev)->priority         = ev.epri;   /* priority */
  RUN(Pev)->time_stamp       = ev.timestamp;
  Pev->SEHDATA.number_events = ev.msgcnt;
  /*
   * Maybe later, it will be better to do a malloc and then do a 
   * strcpy... until then be lazy and save some cycles and just
   * reuse the  memory in recvmsg for the hostname and event 
   * data. This is ok, since this will get copied to the 
   * in-memory copy of the event structure soon.
   */
  Pev->SEHDATA.hostname=ev.hostname;
  if(ev.hostname == NULL || strlen(ev.hostname) == 0)
  {
      Pev->SEHDATA.hostname=string_dup("localhost",strlen("localhost"));
  }
  
  if(Pev->SEHDATA.Pevent_data)
    {
      /*
       * Free previously allocated memory for data.
       */
      sem_mem_free(Pev->SEHDATA.Pevent_data);
      Pev->SEHDATA.Pevent_data=NULL;
    }
    
  if(ev.msgsize > 0)
    {
      Pev->SEHDATA.event_datalength=ev.msgsize;
      Pev->SEHDATA.Pevent_data=sem_mem_alloc_temp(ev.msgsize+1);
      if(!Pev->SEHDATA.Pevent_data)
	{
	  return SEH_ERROR(SEH_MAJ_API_ERR,SEH_MIN_ALLOC_MEM);
	}
      memcpy(Pev->SEHDATA.Pevent_data,recvmsg,ev.msgsize);
    }
  else
    {
      Pev->SEHDATA.event_datalength=0;
      Pev->SEHDATA.Pevent_data=NULL;
    }

  return 0;
}

#if EVENTS_DEBUG

__uint64_t seh_tell_result_api(struct event_private *Pev,
			       struct event_record *Pev_rec,
			       __uint64_t err)
{
  int c;

  printf("%5d %8d %7d %8d \n",
	 Pev_rec->event_id,Pev_rec->event_class,
	 Pev_rec->event_type,Pev_rec->event_count);
  return 0;
}

#endif

	  
void seh_api_clean()
{
    semsrvDone();;
}
