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

int seh_main(struct event_private *Pev,struct event_record *Pev_rec)
{
  system_info_t       *Psys;
  __uint64_t err=0,err1=0;

  extern void seh_thread_block_signals();
  extern int exit_flag;
  
  EVENT_STATE();

  lock_sehconfig();

  /* 
   * Do the appropriate thing depending on the state the seh is in.
   */
  SEH_CHECK_STATE(Pev);
  

  /*
   * Check sysid for SEH/SGH.  If we can't get a valid sysid, the
   * event can be ignored
   */
  
  Psys = NULL;
  
  if ( sgm_get_sysid(&Psys, Pev->SEHDATA.hostname) )
  {
      unlock_sehconfig();
      return 1;
  }
  
  if ( !Psys ) 
  {
      unlock_sehconfig();
      return 1;
  }

  
  /* Update license.status field */
  
  verify_license(&license, Psys->local);
  
  if ( (Psys->local != LOCAL) && (license.current_mode == RUN_AS_SEM) ) 
  {
      unlock_sehconfig();
      return 1;
  }

  Pev->sys_id = Psys->system_id;
  
  
  /*
   * Update throttling counters and check if we are in throttling window.
   * Also create an event record in ssdb if we are opening the
   * window.
   */
  if(err=seh_throttle_event(Pev, Psys->local))
      goto API;
  
  /* update event record in ssdb. */
  if(err=seh_update_event_record_ssdb(Pev,Pev_rec)) 
      goto API;
  
API:
  err1=0;
  /* time to tell dsm that event arrived. */
  if(seh_dsm_sendable_event(Pev,err))
      err1=seh_tell_event_dsm(Pev,Pev_rec);
  
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
  
  if(err && seh_main_check_fatal_error(Pev,err))
  {
      seh_syslog_event(Pev,"SEH_MAIN got a fatal error -- %llx\n",err);
      unlock_sehconfig();
      return 0;
  }

  unlock_sehconfig();
  return 1;
}
