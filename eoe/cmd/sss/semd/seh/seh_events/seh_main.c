/* 
 * File contains the main SEH thread. 
 */
#include "common.h"
#include "events.h"
#include "events_private.h"
#include "seh.h"
#include "seh_errors.h"
#include "sss_pthreads.h"
#include "sss_sysid.h"
#include <sys/select.h>
#if PCP_TRACE
#include <pcp/trace.h>
#endif

extern int verbose,state;
extern SGMLicense_t license;

/* 
 * Check for and return error in the main loop.
 */
static int seh_main_check_fatal_error(struct event_private *Pev,__uint64_t err)
{
  if((SEH_ERROR_MAJOR(err) == SEH_MAJ_MAIN_ERR) ||
     (SEH_ERROR_MAJOR(err) == SEH_MAJ_INIT_ERR))
    return 1;
  if(SEH_ERROR_MINOR(err) & SEH_MIN_ALLOC_MEM)
    {
      seh_syslog_event(NULL,"Cannot allocate memory!.\n");
      return 1;
    }
  if((SEH_ERROR_MAJOR(err) == SEH_MAJ_DSM_ERR) &&
     (SEH_ERROR_MINOR(err) & SEH_MIN_DSM_XMIT))
    {
      seh_syslog_event(NULL,"Failed to send messages to the DSM.\n");
      return 0;
    }
  if(SEH_ERROR_MAJOR(err) == SEH_MAJ_API_ERR)
    {
      seh_syslog_event(Pev,"Error with SEH API, err=%llx, pid=%d, time=%d.\n",
		       err,RUN(Pev)->msgKey,RUN(Pev)->event_times);
      return 0;
    }
  return 0;
}

void *seh_main(void *arg)
{
  struct event_private ev;
  struct event_record ev_rec;
  system_info_t       *Psys;
  __uint64_t err,err1;
#if PCP_TRACE
  int pcp_trace;
#endif
  extern void seh_thread_block_signals();
  extern int exit_flag;
  
  seh_thread_block_signals();	/* Block all signals which are irrelevant. */

  seh_thread_initialize(&ev);	/* Initialize seh before entering main loop. */

  EVENT_STATE();

  lock_sehconfig();
  while(1)
    {
      if(exit_flag)
      {
	  unlock_dsmconfig();
	  return (NULL);
      }
      /*
       * Loop forever -->
       * Get event.
       * Handle event.
       * Back to Get event.
       */
#if PCP_TRACE
      if((pcp_trace=pmtracepoint("sehloop")) < 0)
	{
	  fprintf(stderr,"sehloop point trace failed(%d): %s.\n",
		  pcp_trace,pmtraceerrstr(pcp_trace));
	}
      if((pcp_trace=pmtraceobs("sehdatasize",ev.SEHDATA.event_datalength)) 
	 < 0)
	{
	  fprintf(stderr,"sehdatasize observe trace failed(%d): %s.\n",
		  pcp_trace,pmtraceerrstr(pcp_trace));
	}
#endif
      seh_loop_init(&ev,&ev_rec,&err); /* initialize everytime in loop. */
      

      unlock_sehconfig();
      /*
       * Get the event from the API.
       */
      if(err=seh_get_event_api(&ev))
      {
	  lock_sehconfig();
	  goto API;
      }
      lock_sehconfig();

#if PCP_TRACE
      if((pcp_trace=pmtracebegin("sehloop")) < 0)
	fprintf(stderr,"sehthrottle begin trace failed(%d): %s.\n",
		pcp_trace,pmtraceerrstr(pcp_trace));
#endif

      /* 
       * Do the appropriate thing depending on the state the seh is in.
       */
      SEH_CHECK_STATE(&ev);

      /*
       * Check sysid for SEH/SGH.  If we can't get a valid sysid, the
       * event can be ignored
       */

      Psys = NULL;

      if ( sgm_get_sysid(&Psys, ev.SEHDATA.hostname) )
	continue;

      if ( !Psys ) continue;

      /* Update license.status field */

      verify_license(&license, Psys->local);

      if ( (Psys->local != LOCAL) && (license.current_mode == RUN_AS_SEM) ) 
	continue;
      ev.sys_id = Psys->system_id;


      /*
       * Update throttling counters and check if we are in throttling window.
       * Also create an event record in ssdb if we are opening the
       * window.
       */
      if(err=seh_throttle_event(&ev, Psys->local))
	goto API;

      /* update event record in ssdb. */
      if(err=seh_update_event_record_ssdb(&ev,&ev_rec)) 
	goto API;



#if CONFIGURATION_BY_EVENT
      /* check if it is a configuration event. */
      if(err=seh_reconfig_event(&ev))
	  goto API;
#endif

API:
      err1=0;
      /* time to tell dsm that event arrived. */
      if(seh_dsm_sendable_event(&ev,err))
	err1=seh_tell_event_dsm(&ev,&ev_rec);

      /* 
       * If no error occured prior to making the call to seh_tell_event_dsm 
       * and if an error happened during the call to send the message to the
       * dsm, then set the error flag equal to the error that
       * occured while sending to the dsm.
       * If an error did occur prior to making the call to seh_tell_event_dsm
       * then do not change the error flag, even if an error happened
       * during the call to seh_tell_event_dsm.
       */
      if(!err)			
	err=err1;

#if EVENTS_DEBUG
      err=seh_tell_result_api(&ev,&ev_rec,err);
#endif

      if(err && seh_main_check_fatal_error(&ev,err))
	{
	  seh_syslog_event(&ev,"SEH_MAIN got a fatal error -- %llx\n",err);
	  break;
	}
#if PCP_TRACE
      if((pcp_trace=pmtraceend("sehloop")) < 0)
	fprintf(stderr,"sehthrottle end trace failed(%d): %s.\n",
		pcp_trace,pmtraceerrstr(pcp_trace));
#endif
    }

  unlock_sehconfig();
  return sss_pthreads_exit();
}
