#include "common.h"
#include "events.h"
#include "events_private.h"
#include "dsm_events.h"
#include "dsm_events_private.h"
#include "dsm_rules.h"
#include "dsm.h"
#include "dsm_errors.h"
/*#include "msglib.h"*/

extern int verbose;

/*
 * Check if there was an error that would be considered fatal to the 
 * operation of the dsm.
 */
static int dsm_init_check_fatal_error(__uint64_t err)
{
  assert(DSM_ERROR_MAJOR(err) == DSM_MAJ_INIT_ERR);

  return (err & (DSM_MIN_ALLOC_MEM|DSM_MIN_INIT_SEH|
		 DSM_MIN_INIT_DB  |DSM_MIN_NUM_RULE|
		 DSM_MIN_DB_RULE  |DSM_MIN_INIT_RULE))
    ? 1 : 0;
}

static void dsm_usage(char *pname)
{
  fprintf(stderr,"Usage:\n"
	  "\t%s [-v verbose_level] [-h]\n\n"
	  "\t\t-v verbose_level: 1,2,4,8.\n"
	  "\t\t-h              : for this help message.\n",
	  pname);
  
  return;
}

/*
 * Initialize the ssdb interface, the pthreads/msglib interface, the seh 
 * interface.
 */
static __uint64_t dsm_setup_interfaces()
{
  __uint64_t err=0;

  sss_pthreads_init();

  if(err=dsm_seh_setup_interface())
    return err;
  if(err=dsm_ssdb_setup_interfaces())
    return err;
  return 0;
}

/*
 * Setup our memory for rules.
 * Read anything necessary from the SSDB to do the setup.
 */
static __uint64_t dsm_setup_events_rules(struct event_private *Pev)
{
  __uint64_t err=0;
  int num_rules;
  
  /*
   * Call to do initialization for all events and rules.
   */
  err=dsm_init_all_rules();  
  
  if(err)
    {
      return DSM_ERROR(DSM_MAJ_INIT_ERR,DSM_MIN_INIT_RULE);
    }
  
  return 0;
}

/*
 * Externally visible function. Calls the init for each of these
 * sub-systems --
 *   o dsm generic stuff.
 *   o dsm interfaces.
 *   o dsm events.
 */
__uint64_t dsm_initialize(struct event_private *Pev)
{
  __uint64_t err=0;

  /*
   * First the generic stuff.
   */
  dsm_init_children();

  /*
   * then the interfaces..
   */
  if((err=dsm_setup_interfaces()) && dsm_init_check_fatal_error(err))
    {
      if(verbose >= VERB_DEBUG_0)
	seh_syslog_event(NULL,"DSM_INIT: Failed to setup interfaces "
			 ":0x%llx.\n",err);
#if EVENTS_DEBUG
      error_parse(err);
#endif
      goto LOCAL_ERROR;
    }

  /* 
   * Read the globals.
   */
  if((err=dsm_setup_globals_ssdb()) && dsm_init_check_fatal_error(err))
    {
      if(verbose >= VERB_DEBUG_0)
	seh_syslog_event(NULL,"DSM_INIT: Failed to setup globals :0x%llx.\n",
			 err);
#if EVENTS_DEBUG
      error_parse(err);
#endif
      goto LOCAL_ERROR;
    }
  
  if((err=dsm_setup_events_rules(Pev)) &&
     dsm_init_check_fatal_error(err))
    {
      if(verbose >= VERB_DEBUG_0)
	seh_syslog_event(NULL,"DSM_INIT: Failed to setup rules "
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
 * Initialize every time we enter the beginning of the main dsm loop.
 */
void dsm_loop_init(struct event_private *Pev,__uint64_t *err)
{
  *err=0;

  return;
}

