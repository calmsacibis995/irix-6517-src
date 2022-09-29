#include "common.h"
#include "events.h"
#include "events_private.h"
#include "seh.h"
#include "seh_errors.h"
#include "sss_pthreads.h"
#include "sss_sysid.h"
#include "seh_archive.h"
#if EVENTS_DEBUG
#include "../../dsm/include/dsm_rules.h"
#endif

extern int verbose;
extern BOOLEAN global_logging_flag;
extern int global_action_flag;
extern int IGtotal_rules;
extern int IGtotal_events;
extern int IGtotal_active_events;
extern struct event_private *Pevent_head;
extern struct rule *Prule_head;
extern int state;
extern SGMLicense_t license;

#if EVENTS_DEBUG
extern unsigned int num_errors_in_seh_ssdb;
extern unsigned int num_init_events_seh_ssdb;
extern unsigned int num_created_event_records_seh_ssdb;
extern unsigned int num_init_globals_seh_ssdb;
extern unsigned int num_success_throttle_events;
extern unsigned int num_time_throttling_closed;
extern unsigned int num_regular_throttling_closed;
extern unsigned int num_alarms;
extern unsigned int num_success_config_events;
extern unsigned int num_failed_config_events;
extern unsigned int num_config_globals;
extern unsigned int num_error_events_sent;
extern unsigned int num_syslog_entries;
extern unsigned int num_success_threshold_events;
extern unsigned int num_time_thresholding_closed;
extern unsigned int num_regular_thresholding_closed;
extern unsigned int num_actions_taken_records;
extern unsigned int num_errors_in_dsm_ssdb;
extern unsigned int num_init_events_dsm_ssdb;
extern unsigned int num_init_rules_dsm_ssdb;
extern unsigned int num_init_globals_dsm_ssdb;
extern char *last_proc_executed;
extern unsigned long time_last_proc_executed;
extern unsigned int num_success_config_rules;
extern unsigned int num_failed_config_rules;
extern unsigned int num_actions_taken;
#endif

int seh_make_status(char *Pstatus)
{
  char buffer[16*MAX_MESSAGE_LENGTH];
  struct event_private *Pev=NULL;
  struct rule *Prule=NULL;
  int  nobytes = 0;
  int  buflen = sizeof(buffer);
  char c = ' ';
  int  j = 0;

  nobytes += snprintf(&buffer[nobytes], buflen-nobytes, "Global Configuration :\n");
  nobytes += snprintf(&buffer[nobytes], buflen-nobytes, "----------------------\n");
  nobytes += snprintf(&buffer[nobytes], buflen-nobytes, 
		      "  Event Logging      : %s\n"
		      "  Take Actions       : %s\n"
		      "  Verbose            : %d\n"
#if EVENTS_DEBUG
                      "  No. of Alarms      : %d\n"
#endif
		      "  Running as         : %s\n"
		      "  License Expires on : %s\n",
		      (global_logging_flag == 0 ? "Disabled" : "Enabled"),
		      (global_action_flag  == 0 ? "Disabled" : "Enabled"),
		      verbose, 
#if EVENTS_DEBUG
                      num_alarms,
#endif
		      (license.current_mode == RUN_AS_SGM ? "Group Manager" : "Single System Manager"),
		      (license.license_expiry_date <= 0 ? "Not applicable" : ctime(&license.license_expiry_date))
		      );

  nobytes += snprintf(&buffer[nobytes], buflen-nobytes, "\nRecognized Systems   :\n");
  nobytes += snprintf(&buffer[nobytes], buflen-nobytes, "----------------------\n");
  nobytes += print_sysinfolist(&buffer[nobytes], buflen-nobytes);
  nobytes += snprintf(&buffer[nobytes], buflen-nobytes, "\nSystem Event Handler :\n");
  nobytes += snprintf(&buffer[nobytes], buflen-nobytes, "----------------------\n");
  nobytes += snprintf(&buffer[nobytes], buflen-nobytes,
                 "  Current State      : %s\n",
	  (state == EVENT_HANDLING_STATE) ?
	  "EVENT HANDLING"               : (state == INITIALIZATION_STATE) ?
	  "INITIALIZING"                 : (state == SEH_ARCHIVING_STATE) ?
	  "WAITING FOR SEH -- ARCHIVING" : (state == SSDB_ARCHIVING_STATE) ?
	  "ARCHIVING"      : "UNKNOWN STATE");
  nobytes += snprintf(&buffer[nobytes], buflen-nobytes,
                 "  Events in Memory   : %d\n"
                 "  Active Events      : %d\n"
		 "  Events added in DB : %d\n"
                 "  Event List         :",
                 IGtotal_events, IGtotal_active_events,
		 num_created_event_records_seh_ssdb);
  strcpy(Pstatus, buffer);

  lock_ruleevent();
  Pev=Pevent_head;
  j = 0;
  while(Pev)
    {
      sprintf(buffer,"%s(%llX,%d,%d,%c)",
              (j == 0 ? "\n   ":" "),
	      Pev->sys_id, Pev->event_class,Pev->event_type,
	      RUN(Pev) ? 'A' : '-');
      Pev=Pev->Pnext;
      if ( j < 1 ) j++;
      else j = 0;
      strcat(Pstatus,buffer);
    }

  nobytes = 0;

  nobytes += snprintf(&buffer[nobytes], buflen-nobytes, "\nDecision Support Mod :\n");
  nobytes += snprintf(&buffer[nobytes], buflen-nobytes, "----------------------\n");
  nobytes += snprintf(&buffer[nobytes], buflen-nobytes,
                      "  Rules in Memory    : %d\n"
		      "  Rule List          :\n   ",
                      IGtotal_rules);
  strcat(Pstatus,buffer);	   

  Prule=Prule_head;
  while(Prule)
    {
      sprintf(buffer,"%d ",Prule->rule_objid);
      Prule=Prule->Pnext;
      strcat(Pstatus,buffer);
    }
  unlock_ruleevent();

  nobytes = 0;
  nobytes += snprintf(&buffer[nobytes], buflen-nobytes,
		     "\n  Actions added in DB: %d\n"
		     "  # of actions taken : %d\n",
             num_actions_taken_records, num_actions_taken);

  if ( last_proc_executed ) {
      nobytes += snprintf(&buffer[nobytes], buflen-nobytes,
		     "  Time of last action: %s"
		     "  Last action executd:\n   %s\n",
		     ctime((time_t *)&time_last_proc_executed),
		     last_proc_executed);
  }

  strcat(Pstatus, buffer);

#ifdef EVENTS_DEBUG
  nobytes = 0;
  nobytes += snprintf(&buffer[nobytes], buflen-nobytes, "\nError Happened       :\n");
  nobytes += snprintf(&buffer[nobytes], buflen-nobytes, "----------------------\n");
  nobytes += snprintf(&buffer[nobytes], buflen-nobytes,
		      "  SEH/DB Interface   : %d\n"
		      "  DSM/DB Interface   : %d\n"
		      "  # of Error Events  : %d\n",
		      num_errors_in_seh_ssdb,
                      num_errors_in_dsm_ssdb,
		      num_error_events_sent);
#endif


  strcat(Pstatus,buffer);
  return strlen(Pstatus)+1;
}

/* 
 * The way this works is that as soon as archive mode is entered, the SEH
 * starts dropping all events. Then only error events may be sent to the DSM.
 * In archive mode error events are dropped silently by the DSM. 
 * See also SEH_CHECK_STATE and DSM_CHECK_STATE.
 * 
 * The approach taken here is to be as minimally intrusive on the other 
 * threads. This is the affect on the other threads.
 * SEH thread   -> ignore all events.
 * DSM thread   -> ignore only error events.
 * Alarm thread -> no change, except that it has no work to do, because
 *              -> no event has any active data in it.
 */
void seh_start_archive()
{
  /* 
   * Set state to archiving so both SEH and DSM know that we are about to
   * enter archiving. DSM immediately starts ignoring error events. SEH
   * sets state to SSDB_ARCHIVING and starts ignoring all events.
   */
  SEH_ARCHIVE_STATE();
  /* 
   * Wait for SEH to set state to SSDB_ARCHIVING.
   */
  ARCHIVE_CHECK_STATE();
  /* 
   * Now the SEH has signalled that it is no longer accepting events. It is 
   * safe to start waiting on the DSM for it to finish its work before
   * flushing all events.
   */
  seh_flush_sehdsmq();
  /* 
   * Flush all events.
   */
  lock_ruleevent();
  seh_events_flush();
  unlock_ruleevent();
}

void seh_end_archive()
{
  EVENT_STATE();
}

/* 
 * Archive thread, handles asynchronous (w.r.t events being received) requests
 * to start archiving. This thread receives archive requests on the 
 * SEM_ARCHIVE_PORT port. This port also doubles as the debug port for 
 * receiving and responding to queries for status on the SEM daemon.
 */
void *seh_archive(void *arg)
{
  char *buffer=NULL;
  int len;
  int pid;
  char status_buffer[16*MAX_MESSAGE_LENGTH];
  struct event_private ev;
  __uint64_t aullArchive_id[ARCHIVE_IDS];
  extern int exit_flag;

  while(1)
    {
      if(exit_flag)
	  return NULL;

      if(seh_receive_archive_port(1,&buffer,&len,&pid,aullArchive_id) 
	 || !buffer || !len)
	continue;
      
      /* 
       * Quit messages should always be honored first.
       */
      if(len == (strlen(SEMD_QUIT)+1) &&
	 !strncasecmp(SEMD_QUIT,buffer,len))
      {
	  exit_flag=1;
      }

      /* 
       * Somebody needs status.
       */
      if(len == (strlen(ARCHIVE_STATUS)+1) && 
	 !strncasecmp(ARCHIVE_STATUS,buffer,len))
	{
	  len=seh_make_status(status_buffer);
	  sem_mem_free(buffer);
	  buffer=status_buffer;
	}
      /* 
       * Start archive.
       */
      else if (len == (strlen(ARCHIVE_START)+1) && 
	      !strncasecmp(ARCHIVE_START,buffer,len))
	{
	  sem_mem_free(buffer);
	  buffer=NULL;
	  len=0;
	  seh_start_archive();
	}
      /* 
       * End archive.
       */
      else if (len == (strlen(ARCHIVE_END)+1) && 
	       !strncasecmp(ARCHIVE_END,buffer,len))
	{
	  sem_mem_free(buffer);
	  buffer=NULL;
	  len=0;
	  seh_end_archive();
	}
      else if (len > MINIMUM_CONFIG_LENGTH &&
	       !strncasecmp(GLOBAL_CONFIG_STRING,buffer,MINIMUM_CONFIG_LENGTH)) 
	  /*
	   * Could be any of global, event, rule or sgm config in the memcmp
	   * above.
	   */
      {
	  /* 
	   * it is an attempt at re-configuration.
	   */
	  bzero(&ev,sizeof(struct event_private));

	  ev.run=sem_mem_alloc_temp(sizeof(struct event_runtime));
	  if(!seh_build_config_event(&ev,buffer+MINIMUM_CONFIG_LENGTH,
				     len-MINIMUM_CONFIG_LENGTH))
	  {
	      lock_sehconfig();
	      seh_reconfig_event(&ev);
	      unlock_sehconfig();

	      lock_dsmconfig();
	      dsm_reconfig_rule(&ev);
	      unlock_dsmconfig();
	  }

	  if(ev.SEHDATA.Pevent_data)
	      sem_mem_free(ev.SEHDATA.Pevent_data);
	  sem_mem_free(ev.run);

	  sem_mem_free(buffer);
	  buffer=NULL;
	  len=0;
      }
      /* 
       * Got junk on the archive port. Don't even bother replying.
       */
      else
	{
	  sem_mem_free(buffer);
	  buffer=NULL;
	  len=0;
	}
	  
      seh_send_archive_port(1,buffer,len,pid,aullArchive_id);
      buffer=NULL;
      len=0;
    }
}



