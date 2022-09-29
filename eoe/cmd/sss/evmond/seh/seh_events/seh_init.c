#include "common.h"
#include "events.h"
#include "events_private.h"
#include "seh.h"
#include "seh_errors.h"
/*#include "msglib.h"*/
#include "seh_archive.h"
#include "ssdbapi.h"

extern int verbose;

/* 
 * Return fatal error if any error in the init domain.
 */
static int seh_init_check_fatal_error(__uint64_t err)
{
  if(SEH_ERROR_MAJOR(err) != SEH_MAJ_INIT_ERR)
    {
      fprintf(stderr,"Assert failed in seh_init_check_fatal_error = 0x%llx\n",
	      err);
      return 1;
    }
  
  return (err & (SEH_MIN_ALLOC_MEM |SEH_MIN_INIT_API|
		 SEH_MIN_INIT_DB   |SEH_MIN_INIT_DSM|
		 SEH_MIN_INIT_EVNUM|SEH_MIN_INIT_EVENT)) 
    ? 1 : 0;
}

/*
 * Setup the interfaces for the SEH. We don't care what they are -- message
 * queue's or shared memory or whatever.
 */
static __uint64_t seh_setup_interfaces()
{
  __uint64_t err=0;

  /* 
   * Initialize ssdb api.
   */
  if(err=seh_setup_interfaces_ssdb())     /* Init the SSDB */
    return err;
  if(err=seh_dsm_setup_interfaces())      /* Init the DSM */
    return err;

  return 0;
}

/*
 * Setup our memory for events, event counters, event types etc.
 * Read anything necessary from the SSDB to do the setup.
 */
static __uint64_t seh_setup_events(struct event_private *Pev)
{
  __uint64_t err=0;
  int num_events;

  memset(Pev,0,sizeof(struct event_private));

  /*
   * Call to do initialization for all events.
   */
  err=seh_init_all_events();

  return err;
}

/*
 * Externally visible function. Calls the init for each of these 
 * sub-systems --
 *   o seh generic stuff.
 *   o seh interfaces.
 *   o seh events.
 */     
__uint64_t seh_initialize(struct event_private *Pev)
{
  __uint64_t err=0;

  sem_mem_alloc_init();		/* init the memory pool. */
  stringtab_init();		/* init the string table. */
    
  /* 
   * Init all the interfaces coming in to the SEH.
   */
  if((err=seh_setup_interfaces()) && seh_init_check_fatal_error(err))
    {
      seh_syslog_event(NULL,"SEH_INIT: Failed to setup interfaces "
		       ":0x%llx.\n",err);
#if EVENTS_DEBUG
      error_parse(err);
#endif
      goto LOCAL_ERROR;
    }

  /* 
   * Read the globals.
   */
  if((err=seh_setup_globals_ssdb()) && seh_init_check_fatal_error(err))
    {
      if(verbose >= VERB_DEBUG_0)
	seh_syslog_event(NULL,"SEH_INIT: Failed to setup globals :0x%llx.\n",
			 err);
#if EVENTS_DEBUG
      error_parse(err);
#endif
      goto LOCAL_ERROR;
    }
  
  /* 
   * Init all the events.
   */
  if((err=seh_setup_events(Pev)) && seh_init_check_fatal_error(err))
    {
      seh_syslog_event(NULL,"SEH_INIT: Failed to setup events "
		       ":0x%llx.\n",err);
#if EVENTS_DEBUG
      error_parse(err);
#endif
      goto LOCAL_ERROR;
    }
  
  return 0;

LOCAL_ERROR:
  return err;
}

/*
 * Initialization required everytime we iterate in the main loop.
 */
void seh_loop_init(struct event_private *Pev,
		   struct event_record *Pev_rec)
{
  memset(Pev_rec,0,sizeof(struct event_record));
  Pev_rec->valid_record=FALSE;
  return;
}
