#include "options.h"

#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>		/* For varargs */
#include <stdlib.h>		/* For string conversion routines */
#include <string.h>
#include <bstring.h>
#include <ctype.h>		/* For character definitions */
#include <errno.h>
#include <fcntl.h>		/* for I/O functions */
#include <time.h>
#include <ulocks.h>
#include <invent.h>		/* for inventory functions */
#include <syslog.h>
#include <sys/signal.h>		/* for UNIX signals */
#include <sys/wait.h>		/* for UNIX signals */

#define XDR_SHORTCUT

#include "debug.h"
#include "fcagent.h"
#include "fcal.h"
#include "config.h"
#include "esi.h"
#include "scsi.h"
#include "fcagent_structs.h"
#include "fcagent_rpc.h"
#include "hash.h"
#include "usrsignal.h"
#include "dslib.h"

/*
 * Forward declarations
 */
channel_e_t agent_get_config();
channel_e_t agent_get_channels();
void agent_check_config(encl_e_t enc);
int agent_get_encl_config(channel_e_t ch, encl_e_t enc);
int agent_get_encl_status(channel_e_t ch, encl_e_t enc, int first_time);
int agent_set_drv_led(channel_e_t ch, fcid_bitmap_t ids, int op);
int agent_set_drv_bypass(channel_e_t ch, fcid_bitmap_t ids, int op);
int agent_set_drv_remove(channel_e_t ch, fcid_bitmap_t ids, int spindown);
int agent_set_drv_insert(channel_e_t ch, fcid_bitmap_t ids, int spinup);

void request_handler_proc(void *arg);
void event_handler_proc(void *arg);

void cleanup();
void signal_handler(int sig);

channel_e_t agent_find_ch_by_cid(int cid);
encl_e_t agent_find_encl_by_tid(channel_e_t ch, int tid);
encl_e_t agent_find_encl_by_eid(channel_e_t ch, int eid);

/* 
 * Global variables 
 */
app_data_struct_t app_data;
usr_sigaction_t actions[] = 
{
  {SIGINT,           signal_handler, SA_RESTART},
  {SIGCHLD,          signal_handler, SA_RESTART /*| SA_NOCLDSTOP*/},
  {SIGHUP,           signal_handler, SA_RESTART},
  {NULL,             NULL,           NULL}
};
thread_ctl_struct_t threads[] = {
  { NPID_TH,  request_handler_proc, -1, "Asynch. request handler/server" },
  { NPID_EH,  event_handler_proc,   -1, "Asynch. event hander" },
  { -1,       NULL,                 -1, NULL },
};
int sig_quit = 0;

/*
 ******************************************************************************
 * upid2npid() - UNIX pid to Normalized PID
 ****************************************************************************** 
*/
uint upid2npid(pid_t upid)
{
  thread_ctl_t th;

  for (th = threads; th->th_npid != -1; ++th) {
    if (upid == th->th_upid)
      return(th->th_npid);
  }
  return(-1);
}

/*
 ******************************************************************************
 * hfunc() - enclosure WWN hash function
 ****************************************************************************** 
*/
int hfunc(ht_key_t key)
{
  unsigned long long k = key;
  u_int k1, k2;

  k1 = k >> 32;
  k2 = k & 0xFFFFFFFF;
  return(k1 ^ k2);
}

/*
 ******************************************************************************
 * cfunc() - enclosure WWN compare function
 ****************************************************************************** 
*/
int cfunc(ht_key_t key1, ht_key_t key2)
{
  unsigned long long k1 = key1;
  unsigned long long k2 = key2;

  if (k1 < k2)
    return(-1);
  else
  if (k1 > k2)
    return(1);
  else
    return(0);
}

/*
 ******************************************************************************
 * ksfunc() - enclosure WWN key print function
 ****************************************************************************** 
*/
char *ksfunc(ht_key_t key)
{
  unsigned long long k = key;
  static char tmpString[100];

  sprintf(tmpString, "0x%llx", k);
  return(tmpString);
}

/*
 ******************************************************************************
 * vsfunc() - enclosure WWN val print function
 ****************************************************************************** 
*/
char *vsfunc(ht_val_t val)
{
  static char tmpString[100];

  sprintf(tmpString, "0x%x", val);
  return(tmpString);
}

/*
 ******************************************************************************
 * get_LIP_map() - for now just create a from drives found.
 ******************************************************************************
 */
alpa_bitmap_t get_LIP_map(channel_e_t ch)
{
  alpa_bitmap_t bm = calloc(1, sizeof(alpa_bitmap_struct_t));
  int i;

  {
    inventory_t *inv;

    /*
     * FCAgent does not handle fabric disk drives.  So when an inventory class
     * of INV_FCNODE is found, we want to skip the next inventory entry, which
     * would have a class corresponding to the type of fabric device.
     * Fabric devices always have two inventory entries, the first of which is
     * INV_FCNODE.  If that changes, this will have to change too.
     */
    setinvent();
    while (1) {
      inv = getinvent();
      if (inv) {
	if (inv->inv_class == INV_FCNODE) {
	  inv = getinvent();
	}
	else if ((inv->inv_class == INV_DISK) && 
	         (inv->inv_type == INV_SCSIDRIVE) &&
	         (inv->inv_controller == ch->id)) {
	  BM_SET(bm, inv->inv_unit);
	}
      }
      else {
	endinvent();
	break;
      }
    }
  }

  return(bm);
}

/*
 ******************************************************************************
 * cleanup()
 ******************************************************************************
 */
void cleanup()
{
  /*
   * Kill and wait for child threads to die
   */
  {
    sigset_t set, oset;
    thread_ctl_t th;

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &set, &oset) == -1)
      fatal("sigprocmask(SIG_BLOCK) failed : %s\n", strerror(errno));
    for (th = threads; th->th_npid != -1; ++th) {
      if (th->th_upid != -1) {
	sigsend(P_PID, th->th_upid, SIGSOFTKILL);
      }
    }
    if (sigprocmask(SIG_SETMASK, &oset, NULL) == -1)
      fatal("sigprocmask(SIG_SETMASK) failed : %s\n", strerror(errno));

    for (th = threads; th->th_npid != -1; ++th) {
      while (th->th_upid != -1) {
	printf("Waiting for '%s' to die.\n", th->th_string);
	sleep(1);
      }
    }	
  }
  
  /*
   * Event cleanup
   */
  ev_cleanup();

  /* 
   * Cleanup shared arena stuff
   */ 
  if (ARENA) {
    usdetach(ARENA);
    remove(ARENA_FN);
    free(ARENA_FN);
    ARENA = NULL;
    ARENA_FN = NULL;
  }

  /* 
   * Close syslog
   */
  syslog(LOG_INFO, "FCAGENT exiting\n");
  closelog();
}

/*
 ******************************************************************************
 * signal_handler()
 ******************************************************************************
 */
void signal_handler(int sig)
{
  int upid, npid, status, rc;
  sigset_t pend_set;

  switch(sig) {
  case SIGINT:
    DBG(D_SIGNAL, "AGENT received a SIGINT \n");
    sig_quit = 1;
    break;
  case SIGCHLD:
    do {
      if ((upid = waitpid(-1, &status, WNOHANG)) <= 0)
	break;
      npid = upid2npid(upid);
      if (WIFSTOPPED(status)) {
	DBG(D_SIGNAL, "AGENT received a SIGCHLD (STOPPED) from UPID %d, NPID %d\n", 
	    upid, npid);	
      }
      else 
      if (WIFEXITED(status)) {
	rc = WEXITSTATUS(status);
	DBG(D_SIGNAL, "AGENT received a SIGCHLD (EXITED) from UPID %d, NPID %d : RC = %d\n", 
	    upid, npid, rc);	
      }
      else
      if (WIFSIGNALED(status)) {
	rc = WTERMSIG(status);
	DBG(D_SIGNAL, "AGENT received a SIGCHLD (SIGNALED) from UPID %d, NPID %d : SIGNAL = %d\n", 
	    upid, npid, rc);	
      }
      else {
	DBG(D_SIGNAL, "AGENT received a SIGCHLD (UNKNOWN) from UPID %d, NPID %d\n", 
	    upid, npid);
      }
      threads[npid].th_upid = -1;
    } while (1);
    break;
  case SIGHUP:
    DBG(D_SIGNAL, "AGENT received a SIGHUP\n");
    /* 
     * Re-read the configuration file.
     */
    if (cfg_read(CFG_FN, (char *)CFG, agent_vt) == 0) {
      __debug = CFG->ac_debug;
      dsdebug = (CFG->ac_debug & D_DSDEBUG) ? 1 : 0;

#if 0
      printf("AGENT POLL PERIOD       - %d\n", CFG->ac_pollperiod);
      printf("AGENT AUTO LOOP RECOVER - %d\n", CFG->ac_autolooprecover);
      printf("AGENT PRE-REMOVAL CO    - %s\n", CFG->ac_preremovalco);
      printf("AGENT POST-REMOVAL CO   - %s\n", CFG->ac_postremovalco);
      printf("AGENT POST-INSERT CO    - %s\n", CFG->ac_postinsertco);
      printf("AGENT STAT CHANGED CO   - %s\n", CFG->ac_stat_change_co);
      printf("AGENT AUTO INTRO        - %d\n", CFG->ac_autointro);
      printf("AGENT DEBUG LEVEL       - %d\n", CFG->ac_debug);
      printf("AGENT REMOTE REQUESTS   - %d\n", CFG->ac_allow_remote);
#endif
    }
    break; 
  default:
    DBG(D_SIGNAL, "AGENT received a unknown signal [%d]\n", sig);
  }
}

main(int argc, char **argv)
{
  char **p;
  int rc;

  bzero(&app_data, sizeof(app_data_struct_t));

  if (argc < 2) {
    fprintf(stderr, "Usage: %s <configuration filename>\n", argv[0]);
    cleanup();
    exit(-1);
  }
  CFG_FN = argv[1];
  CFG = calloc(1, sizeof(agent_cfg_struct_t));

  /* 
   * Read the configuration file.
   */
  if (rc = cfg_read(CFG_FN, (char *)CFG, agent_vt)) {
    if (rc == -1)
      fatal("Configuration read failed for '%s': %s\n", CFG_FN, strerror(errno));
    cleanup();
    exit(1);
  }
  __debug = CFG->ac_debug;
  dsdebug = (CFG->ac_debug & D_DSDEBUG) ? 1 : 0;
  
#if 0
  printf("AGENT POLL PERIOD       - %d\n", CFG->ac_pollperiod);
  printf("AGENT AUTO LOOP RECOVER - %d\n", CFG->ac_autolooprecover);
  printf("AGENT PRE-REMOVAL CO    - %s\n", CFG->ac_preremovalco);
  printf("AGENT POST-REMOVAL CO   - %s\n", CFG->ac_postremovalco);
  printf("AGENT POST-INSERT CO    - %s\n", CFG->ac_postinsertco);
  printf("AGENT STAT CHANGED CO   - %s\n", CFG->ac_stat_change_co);
  printf("AGENT AUTO INTRO        - %d\n", CFG->ac_autointro);
  printf("AGENT DEBUG LEVEL       - %d\n", CFG->ac_debug);
#endif

  /*
   * Configure number of users and size of shared arena.
   */
  if (usconfig(CONF_INITUSERS, 5) == -1)
    fatal("usconfig() failed : %s\n", strerror(errno));
  if (usconfig(CONF_INITSIZE, 65536) == -1)
    fatal("usconfig() failed : %s\n", strerror(errno));

  ARENA_FN = malloc(200);
  sprintf(ARENA_FN, "%s_%d", ARENA_FILENAME_BASE, getpid());
  if ((ARENA = usinit(ARENA_FN)) == NULL) 
    fatal("usinit() failed : %s\n", strerror(errno));

  /*
   * Configure number of users and size of C arena (sproc).
   */
  if (usconfig(CONF_INITUSERS, 5) == -1)
    fatal("usconfig() failed : %s\n", strerror(errno));
  if (usconfig(CONF_INITSIZE, 65536) == -1)
    fatal("usconfig() failed : %s\n", strerror(errno));

  /*
   * Install signal handlers                                                   
   */
  if (install_signal_handlers(actions) == -1)
    fatal("install_signal_handlers() failed : %s\n", strerror(errno));

  /* 
   * Initialize the event handler 
   */
  ev_init();

  /*
   * Initialize the enclosure hash tables
   */
  ENCL_HT = hash_new(256, hfunc, cfunc, ksfunc, vsfunc);

  CH_HEAD = agent_get_config();
  if (CH_HEAD == NULL) {
    fprintf(stderr, "%s terminating: No FC channels found.\n", argv[0]);
    cleanup();
    exit(0);
  }

  /* 
   * Become process group leader to release shell
   */
#if 0
  if (fork() == 0) {
    if (setsid() == -1) {
      fatal("setsid() failed: %s\n", strerror(errno));
    }
  }
  else 
    exit(0);
#endif

  /*
   * Open syslog
   */
  openlog("fcagent", LOG_PID|LOG_CONS, LOG_DAEMON);
  syslog(LOG_INFO, "FCAGENT starting\n");

  /*
   * Check for non-redundant configurations
   */
  agent_check_config(NULL);

  /* Fork off listener/responder thread and autonomous event handler thread */
  if (1) {
    sigset_t set, oset;
    thread_ctl_t th;
    caddr_t sp;

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &set, &oset) == -1)
      fatal("sigprocmask(SIG_BLOCK) failed : %s\n", strerror(errno));

    for (th = threads; th->th_npid != -1; ++th) {
#if 0
      th->th_upid = sproc(th->th_proc, PR_SADDR, th->th_npid);
#else
      if ((sp = malloc(STACKSIZE)) == NULL)
	return(-1);
      sp += STACKSIZE;
      th->th_upid = sprocsp((void (*)(void *, size_t))th->th_proc, PR_SADDR, (void *)th->th_npid, sp, STACKSIZE);
#endif
      if (th->th_upid == -1)
	fprintf(stderr, "sproc() failed: %s\n", strerror(errno));
    }
    
    if (sigprocmask(SIG_SETMASK, &oset, NULL) == -1)
      fatal("sigprocmask(SIG_SETMASK) failed : %s\n", strerror(errno));
  }

  /* 
   * Loop forever polling enclosures
   */
  while (!sig_quit) {
    time_t read_gen_code=0;
    channel_e_t ch;
    encl_ref_t  enc_ref;
    encl_e_t    enc;

    read_gen_code = time(NULL);
    for (ch = CH_HEAD; ch; ch = ch->next) {
      for (enc_ref = ch->encl_ref_head; enc_ref; enc_ref = enc_ref->next) {
	enc = enc_ref->encl;
	if (enc->read_gen_code < read_gen_code)
	  agent_get_encl_status(ch, enc, 0);
	if (enc->check_gen_code != enc->status_gen_code)
	  agent_check_config(enc);
      }
    }
    sleep(CFG->ac_pollperiod);
  }
  
  cleanup();
  exit(0);
}

/*
 ******************************************************************************
 * agent_get_config() - the bootstrap function which gropes around for
 * connected controllers and builds the necessary agent data
 * structures.
 *
 * The ALGORITHM:
 * (1) channels = get_channel_list();
 * (2) For each ch in channels
 *       LIP_map = get_LIP_map(ch);
 *       For each tid in LIP_map
 *         if (IS_SET(LIP_map, tid) && IS_8067_CONNECTED(ch, tid)) 
 *            enc = new(encl_e_struct_t);
 *            enc->primdrv = tid;
 *            enc->secdrv = UNDEF;
 *            agent_get_encl_config(enc);            
 *            For all drives tid in enc
 *              UN_SET(LIP_map, tid);
 *              if (tid != enc->primdrv && enc->secdrv == UNDEF && IS_8067_CONNECTED(ch, tid))
 *                 enc->secdrv = tid;
 *              endif
 *            end
 *         endif
 *       end
 *     end
 ******************************************************************************
 */
channel_e_t agent_get_config()
{
  char devscsi_filename[256];
  channel_e_t head=NULL, ch=NULL;
  alpa_bitmap_t lip_map;
  int tid, i;
  encl_e_t enc=NULL, new_enc=NULL;
  encl_ref_t enc_ref;
  drv_e_t drv;
  inq_struct_t inqdata;
  char tmp_str[128];

  head = agent_get_channels();
  if (head == NULL)
    return(NULL);

  for (ch = head; ch; ch = ch->next) {
    lip_map = get_LIP_map(ch);
    for (tid = TID_LO; tid <= TID_HI; ++tid) {
      if (!BM_ISSET(lip_map, tid))
	continue;
      BM_CLR(lip_map, tid);
      if (inquiry(make_devscsi_path(ch->id, tid, 0, devscsi_filename), &inqdata))
	continue;
      strncpy(tmp_str, (char *)inqdata.vid, 8); 
      tmp_str[8] = ':';
      strncpy(tmp_str+9, (char *)inqdata.pid, 16); 
      tmp_str[25] = ' ';
      strncpy(tmp_str+26, (char *)inqdata.prl, 4); 
      tmp_str[30] = ' ';
      strncpy(tmp_str+31, (char *)inqdata.serial, 8); 
      tmp_str[39] = '\0';

      if (inqdata.pdt != DIRECT_ACCESS_DEV)
	continue;

#if !SG_WAR_1
      if (!inqdata.es)
	continue;
#endif

      /* 
       * Clariion specific implementation -- drives in slots 0 and 2
       *  communicate with one LCC, drives in slots 1 and 3 communicate
       *  with the other LCC.
       */
      if (!((inqdata.port == 0 && (tid%10 == 0 || tid%10 == 2)) ||
	    (inqdata.port == 1 && (tid%10 == 1 || tid%10 == 3))))
	continue;
      
      DBG(D_ESI, "  - Found primary ESI disk drive at ID %d - %s: ES = %d, PORT = %d\n", 
	         tid, tmp_str, inqdata.es, inqdata.port);

      new_enc = calloc(1, sizeof(encl_e_struct_t));
      new_enc->connect_port = inqdata.port;
      new_enc->esidrv[0] = tid;
      new_enc->esidrv[1] = TID_UNDEF;

      if (agent_get_encl_config(ch, new_enc) == 0) {
	enc = hash_lookup(ENCL_HT, new_enc->wwn);
	if (enc) {
	  free(new_enc);
	}
	else {
	  hash_enter(ENCL_HT, new_enc->wwn, new_enc);
	  enc = new_enc;

	  enc->stat_blen_alloc = STATUS_BUFLEN;
	  enc->stat_buf = (esi_pg2_t)malloc(STATUS_BUFLEN);
	  if ((new_enc->lock = usnewlock(ARENA)) == NULL)
	    fatal("usnewlock() failed: %s\n", strerror(errno));
	  agent_get_encl_status(ch, enc, 1);

	  for (i = 0, drv = enc->drv; i < enc->drv_count; ++i, ++drv) {
	    BM_CLR(lip_map, drv->phys_id);
	    if (drv->phys_id != enc->esidrv[0] && enc->esidrv[1] == TID_UNDEF) {
	      if (inquiry(make_devscsi_path(ch->id, drv->phys_id, 0, devscsi_filename), &inqdata))
		continue;
	      if (inqdata.pdt != DIRECT_ACCESS_DEV)
		continue;

#if !SG_WAR_1
	      if (!inqdata.es)
		continue;
#endif

	      /* 
	       * Clariion specific implementation -- drives in slots 0 and 2
	       *  communicate with one LCC, drives in slots 1 and 3 communicate
	       *  with the other LCC.
	       */
	      if (!((inqdata.port == 0 && (drv->phys_id%10 == 0 || drv->phys_id%10 == 2)) ||
		    (inqdata.port == 1 && (drv->phys_id%10 == 1 || drv->phys_id%10 == 3))))
		continue;

	      if (enc->connect_port != inqdata.port) {
		DBG(D_ESI, "agent_get_config(%d): Secondary drive (%d) not on same port as primary drive (%d).\n",
		           ch->id, enc->esidrv[0], drv->phys_id);
		continue;
	      }
	      strncpy(tmp_str, (char *)inqdata.vid, 8); 
	      tmp_str[8] = ':';
	      strncpy(tmp_str+9, (char *)inqdata.pid, 16); 
	      tmp_str[25] = ' ';
	      strncpy(tmp_str+26, (char *)inqdata.prl, 4); 
	      tmp_str[30] = ' ';
	      strncpy(tmp_str+31, (char *)inqdata.serial, 8); 
	      tmp_str[39] = '\0';
	      DBG(D_ESI, "  - Found secondary ESI disk drive at ID %d - %s: ES = %d, PORT = %d\n", 
		         drv->phys_id, tmp_str, inqdata.es, inqdata.port);
	      enc->esidrv[1] = drv->phys_id;
	    }
	  } /* for (i) */
	}
	enc_ref = calloc(1, sizeof(encl_ref_struct_t));
	enc_ref->encl = enc;
	if (ch->encl_ref_tail) {
	  ch->encl_ref_tail->next = enc_ref;
	  ch->encl_ref_tail = enc_ref;
	}
	else {
	  ch->encl_ref_head = ch->encl_ref_tail = enc_ref;
	}
      }
      else {
	free(new_enc);
	continue;
      }
    }
  }

  return(head);
}

/*
 ******************************************************************************
 * agent_check_config - check for illegal JBOD configurations.  Check
 * for presence of both odd (1 and 3) and even (0 and 2) ESI drives in
 * enclosure. If the parameter 'enc' is non-NULL, only that enclosure
 * is checked, otherwise all enclosures are checked.
 ******************************************************************************
 */
void agent_check_config(encl_e_t enc)
{
  channel_e_t ch;
  encl_ref_t  enc_ref;

  if (enc) {
    for (ch = CH_HEAD; ch; ch = ch->next) {
      for (enc_ref = ch->encl_ref_head; enc_ref; enc_ref = enc_ref->next) {
	if (enc_ref->encl == enc) {
	  if (enc->connect_port) {
	    if ((enc->drv[1].status != STS_OK) || (enc->drv[3].status != STS_OK))
	      syslog(LOG_WARNING, "Non-redundant JBOD config found: "
		     "One of drives %d and %d is missing on channel %d in enclosure %d\n", 
		     enc->drv[1].phys_id, enc->drv[3].phys_id, ch->id, enc->phys_id);
	  }
	  else {
	    if ((enc->drv[0].status != STS_OK) || (enc->drv[2].status != STS_OK))
	      syslog(LOG_WARNING, "Non-redundant JBOD config found: "
		     "One of drives %d and %d is missing on channel %d in enclosure %d\n", 
		     enc->drv[0].phys_id, enc->drv[2].phys_id, ch->id, enc->phys_id);
	  }
	  enc->check_gen_code = enc->status_gen_code;
	  return;
	}
      }
    }
  }
  else {
    for (ch = CH_HEAD; ch; ch = ch->next) {
      for (enc_ref = ch->encl_ref_head; enc_ref; enc_ref = enc_ref->next) {
	enc = enc_ref->encl;
	if (enc->connect_port) {
	  if ((enc->drv[1].status != STS_OK) || (enc->drv[3].status != STS_OK))
	    syslog(LOG_WARNING, "Non-redundant JBOD config found: "
		   "One of drives %d and %d is missing on channel %d in enclosure %d\n", 
		   enc->drv[1].phys_id, enc->drv[3].phys_id, ch->id, enc->phys_id);
	}
	else {
	  if ((enc->drv[0].status != STS_OK) || (enc->drv[2].status != STS_OK))
	    syslog(LOG_WARNING, "Non-redundant JBOD config found: "
		   "One of drives %d and %d is missing on channel %d in enclosure %d\n", 
		   enc->drv[0].phys_id, enc->drv[2].phys_id, ch->id, enc->phys_id);
	}
	enc->check_gen_code = enc->status_gen_code;
      }
    }
  }
}


/*
 ******************************************************************************
 * agent_get_encl_config() - return the configuration for an enclosure,
 * rooted at enc. The field 'esidrv[0]' is set.
 ****************************************************************************** 
*/
int  agent_get_encl_config(channel_e_t ch, encl_e_t enc)
{
  char devscsi_filename[256];
  u_char buf[CONFIG_BUFLEN], *p;
  u_int buflen;
  char   *devname;
  esi_pg1_t pg1;
  esi_pg1_type_desc_t tdesc;
  int i, j;

  devname = make_devscsi_path(ch->id, enc->esidrv[0], 0, devscsi_filename);

  /*
   * READ the configuration page
   */
  buflen = CONFIG_BUFLEN;
  if (recv_diagnostics(devname, RECV_ES_CONFIGURATION, buf, &buflen))
    return(-1);
  pg1 = (esi_pg1_t)buf;
  if (pg1->page_code != RECV_ES_CONFIGURATION)
    return(-1);
  enc->config_gen_code = pg1->gen_code;

#if DG_WAR_13
  pg1->enc_phys_id = enc->esidrv[0]/10;
#endif

#if DG_WAR_3
  enc->wwn = (ch->id << 16) | pg1->enc_phys_id;
#else
  bcopy(pg1->enc_wwn, &enc->wwn, sizeof(unsigned long long));
#endif

#if 0
  bcopy("SGI     ", enc->vid, 8);
  bcopy("FCHAJBOD", enc->pid, 8);
  bcopy("1.0     ", enc->prl, 8);
#else
  bcopy(pg1->enc_vid, enc->vid, 8);
  bcopy(pg1->enc_pid, enc->pid, 8);
  bcopy(pg1->enc_prl, enc->prl, 8);
#endif

  enc->phys_id = pg1->enc_phys_id;
  enc->type_count = pg1->num_types;
  enc->types = calloc(pg1->num_types, sizeof(esi_pg1_type_desc_struct_t));
/*
  bcopy(buf + 12 + pg1->glob_desc_len, (u_char *)enc->types, enc->type_count*sizeof(esi_pg1_type_desc_struct_t));
*/
  p = buf + 12 + pg1->glob_desc_len;

  for (i = 0, tdesc = enc->types;
       i < enc->type_count;
       ++i, ++tdesc) {
    bcopy(p, tdesc, sizeof(esi_pg1_type_desc_struct_t));
    switch(tdesc->type_code) {
    case E_TYPE_DISK:
      DBG(D_ESI, "agent_get_encl_config(): Found %d DISKs\n", tdesc->elem_count);
      enc->drv_count = tdesc->elem_count;
      enc->drv = calloc(enc->drv_count, sizeof(drv_e_struct_t));
      for (j = 0; j < enc->drv_count; ++j)
	enc->drv[j].status = STS_INVALID;
      break;
    case E_TYPE_PS:
      DBG(D_ESI, "agent_get_encl_config(): Found %d PSs\n", tdesc->elem_count);
      enc->ps_count = tdesc->elem_count;
      enc->ps = calloc(enc->ps_count, sizeof(ps_e_struct_t));
      for (j = 0; j < enc->ps_count; ++j)
	enc->ps[j].status = STS_INVALID;
      break;
    case E_TYPE_FAN:
      DBG(D_ESI, "agent_get_encl_config(): Found %d FANs\n", tdesc->elem_count);
      enc->fan_count = tdesc->elem_count;
      enc->fan = calloc(enc->fan_count, sizeof(fan_e_struct_t));
      for (j = 0; j < enc->fan_count; ++j)
	enc->fan[j].status = STS_INVALID;
      break;
    case E_TYPE_LCC:
      DBG(D_ESI, "agent_get_encl_config(): Found %d LCCs\n", tdesc->elem_count);
      enc->lcc_count = tdesc->elem_count;
      enc->lcc = calloc(enc->lcc_count, sizeof(lcc_e_struct_t));
      for (j = 0; j < enc->lcc_count; ++j)
	enc->lcc[j].status = STS_INVALID;
      enc->lcc_str_len = tdesc->text_len + 1;
      enc->lcc_str = calloc(1, enc->lcc_str_len);
      bcopy(p + sizeof(esi_pg1_type_desc_struct_t), enc->lcc_str, tdesc->text_len);
      break;
    case E_TYPE_UPS:
      DBG(D_ESI, "agent_get_encl_config(): Found %d UPSs\n", tdesc->elem_count);
#if DG_WAR_4
      tdesc->elem_count = 2;
      enc->types[i].elem_count = 2;
#endif
      break;
    default:
      DBG(D_ESI, "Unknown configuration element type %d, count %d\n", tdesc->type_code, tdesc->elem_count);
    }
    p += sizeof(esi_pg1_type_desc_struct_t) + tdesc->text_len;
  }

  return(0);
}

/*
 ******************************************************************************
 * agent_get_encl_status() - return the status for an enclosure, rooted at
 * enc. 
 ****************************************************************************** 
 */
int  agent_get_encl_status(channel_e_t ch, encl_e_t enc, int first_time)
{
  char devscsi_filename[256];
  int state=0, short_read=1;
  char   *devname;
  esi_pg2_t pg2;
  esi_pg2_gnrc_desc_t elem_desc;
  int type_index, elem_index;
  u_char new_status;
  event_t ev;
  u_int buflen;
  int rc=0;

  DBG(D_ESI, "Polling CH %d, ENCL %d\n", ch->id, enc->phys_id);
  LOCK(enc->lock);
  enc->read_gen_code = time(NULL);

  if (first_time)
    short_read = 0;
  
#if DG_WAR_2
  short_read = 0;
#endif

  /*
   * READ the status page, first trying the primary 8067 drive and then the
   * secondary.
   */
 get_status_retry:
  state = 0;
  while (1) {
    switch (state) {
    case 0:
    case 1:
      if (enc->esidrv[state & 0x3] != TID_UNDEF) {
	devname = make_devscsi_path(ch->id, enc->esidrv[state], 0, devscsi_filename);
	buflen = short_read ? sizeof(esi_pg2_struct_t) : enc->stat_blen_alloc;
	if (recv_diagnostics(devname, RECV_ES_ENCL_STATUS, (unchar *)enc->stat_buf, &buflen) == 0) {
	  /*
	   * If succeeded on secondary, swap primary and secondary
	   */
	  if (state) {
	    u_char drv; 
	    drv = enc->esidrv[0];
	    enc->esidrv[0] = enc->esidrv[1];
	    enc->esidrv[1] = drv;
	  }
	  goto get_status_success;
	}
	else {
	  if (state == 0) {
	    fprintf(stderr, "Primary ESI drive (%d) failure on FC channel %d: "
		            "trying secondary (%d)\n",
		            enc->esidrv[0], ch->id, enc->esidrv[1]);
	    syslog(LOG_CRIT, "Primary ESI drive (%d) failure on FC channel %d: "
		             "trying secondary (%d)\n",
		             enc->esidrv[0], ch->id, enc->esidrv[1]);
	  }
	  else {
	    fprintf(stderr, "Secondary ESI drive (%d) failure on FC channel %d: "
		            "Unable to determine enclosure status\n",
		            enc->esidrv[1], ch->id);
	    syslog(LOG_CRIT, "Secondary ESI drive (%d) failure on FC channel %d: "
		             "Unable to determine enclosure status\n",
		             enc->esidrv[1], ch->id);
	  }
	  ++state;
	}
      }
      else
	++state;
      break;
    default:
      enc->status = STS_INVALID; /* Mark the enclosure status as unknown */
      rc = -1;
      goto status_done;
    }
  }
 get_status_success:
  pg2 = enc->stat_buf;
  if (pg2->page_code != RECV_ES_ENCL_STATUS) {
    rc = -1;
    goto status_done;
  }
  if (short_read && pg2->gen_code != enc->status_gen_code) {
    short_read = 0;
    goto get_status_retry;
  }
  if (short_read)
    goto status_done;

  /*
   * Parse the raw status
   */
  if (first_time)
    enc->stat_blen_actual = buflen;
#if DG_WAR_2
  if (first_time || 1) {
#else
  if (first_time || pg2->gen_code != enc->status_gen_code) {
#endif
    /* Enclosure status */
    if (pg2->non_crit || pg2->crit || pg2->unrecov)
      new_status = STS_FAILED;
    else
      new_status = STS_OK;
    enc->status = new_status;

    elem_desc = (esi_pg2_gnrc_desc_t)((unchar *)enc->stat_buf + sizeof(esi_pg2_struct_t));
    for (type_index = 0; type_index < enc->type_count; ++type_index) {
      ++elem_desc;		/* Skip over global */
      for (elem_index = 0; elem_index < enc->types[type_index].elem_count; ++elem_index, ++elem_desc) {
	switch(enc->types[type_index].type_code) {

	case E_TYPE_DISK:
	  {
	    esi_pg2_drv_desc_t drv_desc = (esi_pg2_drv_desc_t)elem_desc;
	    drv_e_t drv = &enc->drv[elem_index];
	    if (first_time) {
	      drv->connect_port = enc->connect_port;
	    }
#if DG_WAR_1
	    drv->phys_id = enc->phys_id * enc->drv_count + elem_index;
#else
	    drv->phys_id = drv_desc->hard_address;
#endif
	    drv->drv_asserting_fault = drv_desc->sense_fault;
	    drv->enc_asserting_fault = drv_desc->fault_reqstd;
	    drv->drv_asserting_bypass = (drv->connect_port ? 
					 drv_desc->byp_B_enabled : drv_desc->byp_A_enabled);
	    drv->enc_asserting_bypass = (drv->connect_port ? 
					 drv_desc->enable_byp_B : drv_desc->enable_byp_A);
	    
	    if (IS_E_ELEM_STS_FAILED(drv_desc->status) || drv_desc->sense_fault)
	      new_status = STS_FAILED;
	    else
	    if (IS_E_ELEM_STS_NOT_PRESENT(drv_desc->status))
	      new_status = STS_NOT_PRESENT;
	    else
	    if (drv_desc->drive_off)
	      new_status = STS_OFF;
	    else
	    if (drv->drv_asserting_bypass || drv->enc_asserting_bypass)
	      new_status = STS_BYPASSED;
	    else
	      new_status = STS_OK;
	    if ((!first_time && drv->status != new_status) ||
		(first_time  && new_status == STS_FAILED)) {
	      ev = calloc(1, sizeof(event_struct_t));
	      ev->ev_ch_id = ch->id;
	      ev->ev_encl_id = enc->phys_id;
	      ev->ev_elem_type = EVT_ELEM_TYPE_DISK;
	      ev->ev_elem_id = elem_index;
	      ev->ev_old_status = drv->status;
	      ev->ev_new_status = new_status;
	      ev_enqueue(ev);
	    }

	    /*
	     * Potentially added another a secondary ESI drive. Check.
	     */
	    if (!first_time && 
		(enc->esidrv[1] == TID_UNDEF) && 
		(new_status == STS_OK) && (drv->status != new_status)) {
	      if (((drv->connect_port == 0) && (drv->phys_id%10 == 0 || drv->phys_id%10 == 2)) ||
		  ((drv->connect_port == 1) && (drv->phys_id%10 == 1 || drv->phys_id%10 == 3))) {
		if (enc->esidrv[0] != drv->phys_id)
		  enc->esidrv[1] = drv->phys_id;
	      }
	    }

	    drv->status = new_status;

	    DBG(D_ESI, "ENCL 0x%llx: DISK %d:\n", enc->wwn, elem_index);
	    DBG(D_ESI, "           status               = %d\n", drv->status);
	    DBG(D_ESI, "           phys_addr            = %d\n", drv->phys_id);
	    DBG(D_ESI, "           connect_port         = %d\n", drv->connect_port);
	    DBG(D_ESI, "           drv_asserting_fault  = %d\n", drv->drv_asserting_fault);
	    DBG(D_ESI, "           enc_asserting_fault  = %d\n", drv->enc_asserting_fault);
	    DBG(D_ESI, "           drv_asserting_bypass = %d\n", drv->drv_asserting_bypass);
	    DBG(D_ESI, "           enc_asserting_bypass = %d\n", drv->enc_asserting_bypass);
	  }
	  break;

	case E_TYPE_PS: 
	  {
	    esi_pg2_ps_desc_t ps_desc = (esi_pg2_ps_desc_t)elem_desc;
	    ps_e_t ps = &enc->ps[elem_index];
	    if (first_time) {
	      ;
	    }
	    if (IS_E_ELEM_STS_FAILED(ps_desc->status) || ps_desc->fail || ps_desc->overtemp_fail)
	      new_status = STS_FAILED;
	    else
	    if (IS_E_ELEM_STS_NOT_PRESENT(ps_desc->status))
	      new_status = STS_NOT_PRESENT;
	    else
	    if (!ps_desc->rqsted_on)
	      new_status = STS_OFF;
	    else
	      new_status = STS_OK;
	    if (ps_desc->overtemp_fail) {
	    	syslog(LOG_CRIT, "Power supply (%d) overheated in enclosure %d on FC channel %d: ",
			 elem_index, enc->phys_id, ch->id); 
		syslog(LOG_CRIT, "");
	    }
	    if (ps_desc->temp_warning) {
		syslog(LOG_CRIT, "Power supply (%d) temperate warning in enclosure %d on FC channel %d:", 
			elem_index, enc->phys_id, ch->id);
		syslog(LOG_CRIT, "");
	    }
	    if ((!first_time && ps->status != new_status) ||
		(first_time  && new_status == STS_FAILED)) {
	      ev = calloc(1, sizeof(event_struct_t));
	      ev->ev_ch_id = ch->id;
	      ev->ev_encl_id = enc->phys_id;
	      ev->ev_elem_type = EVT_ELEM_TYPE_PS;
	      ev->ev_elem_id = elem_index;
	      ev->ev_old_status = ps->status;
	      ev->ev_new_status = new_status;
	      ev_enqueue(ev);
	    }
	    ps->status = new_status;

	    DBG(D_ESI, "ENCL 0x%llx: PS %d:\n", enc->wwn, elem_index);
	    DBG(D_ESI, "           status               = %d\n", ps->status);
	    DBG(D_ESI, "           rqsted_on            = %d\n", ps_desc->status);
	    DBG(D_ESI, "           fail                 = %d\n", ps_desc->fail);

	  }
	  break;

	case E_TYPE_FAN:
	  {
	    esi_pg2_fan_desc_t fan_desc = (esi_pg2_fan_desc_t)elem_desc;
	    fan_e_t fan = &enc->fan[elem_index];
	    if (first_time) {
	      ;
	    }
	    if (IS_E_ELEM_STS_FAILED(fan_desc->status) || fan_desc->fail || fan_desc->speed_code != FAN_SPD_NORMAL)
	      new_status = STS_FAILED;
	    else
	    if (IS_E_ELEM_STS_NOT_PRESENT(fan_desc->status))
	      new_status = STS_NOT_PRESENT;
	    else
#if 0
	    if (!fan_desc->rqsted_on)
	      new_status = STS_OFF;
	    else
#endif
	      new_status = STS_OK;
	    if ((!first_time && fan->status != new_status) ||
		(first_time  && new_status == STS_FAILED)) {
	      ev = calloc(1, sizeof(event_struct_t));
	      ev->ev_ch_id = ch->id;
	      ev->ev_encl_id = enc->phys_id;
	      ev->ev_elem_type = EVT_ELEM_TYPE_FAN;
	      ev->ev_elem_id = elem_index;
	      ev->ev_old_status = fan->status;
	      ev->ev_new_status = new_status;
	      ev_enqueue(ev);
	    }
	    fan->status = new_status;

	    DBG(D_ESI, "ENCL 0x%llx: FAN %d:\n", enc->wwn, elem_index);
	    DBG(D_ESI, "           status               = %d\n", fan->status);

	  }
	  break;

	case E_TYPE_LCC:
	  {
	    esi_pg2_lcc_desc_t lcc_desc = (esi_pg2_lcc_desc_t)elem_desc;
	    lcc_e_t lcc = &enc->lcc[elem_index];
	    if (first_time) {
	      ;
	    }
	    lcc->connect_port = lcc_desc->LCC_slot;
	    lcc->position = lcc_desc->rack_mounted;
	    lcc->local_mode = lcc_desc->local_mode;
	    lcc->peer_present = lcc_desc->other_LCC_inserted;
	    lcc->peer_failed = lcc_desc->other_LCC_fault;
	    lcc->exp_port_open = lcc_desc->exp_port_open;
	    lcc->exp_shunt_closed = lcc_desc->exp_shunt_closed;
	    
	    if (IS_E_ELEM_STS_FAILED(lcc_desc->status) || lcc_desc->exp_MC_fault || lcc_desc->prim_MC_fault)
	      new_status = STS_FAILED;
	    else
	    if (lcc->peer_failed)
	      new_status = STS_PEER_FAILED;
	    else
	      new_status = STS_OK;
	    if ((!first_time && lcc->status != new_status) ||
		(first_time  && (new_status == STS_FAILED || 
				 new_status == STS_PEER_FAILED))) {
	      ev = calloc(1, sizeof(event_struct_t));
	      ev->ev_ch_id = ch->id;
	      ev->ev_encl_id = enc->phys_id;
	      ev->ev_elem_type = EVT_ELEM_TYPE_LCC;
	      ev->ev_elem_id = elem_index;
	      ev->ev_old_status = lcc->status;
	      ev->ev_new_status = new_status;
	      ev_enqueue(ev);
	    }
	    lcc->status = new_status;

	    DBG(D_ESI, "ENCL 0x%llx: LCC %d:\n", enc->wwn, elem_index);
	    DBG(D_ESI, "           status               = %d\n", lcc->status);
	    DBG(D_ESI, "           connect_port         = %d\n", lcc->connect_port);
	    DBG(D_ESI, "           position             = %d\n", lcc->position);
	    DBG(D_ESI, "           local mode           = %d\n", lcc->local_mode);
	    DBG(D_ESI, "           peer_present         = %d\n", lcc->peer_present);
	    DBG(D_ESI, "           peer_failed          = %d\n", lcc->peer_failed);
	    DBG(D_ESI, "           exp_port_open        = %d\n", lcc->exp_port_open);
	    DBG(D_ESI, "           exp_shunt_closed     = %d\n", lcc->exp_shunt_closed);

	  }
	  break;
	}
      }
    }
  }
  enc->status_gen_code = pg2->gen_code;
 status_done:
  UNLOCK(enc->lock);
  return(rc);
}

/*
 ******************************************************************************
 * agent_get_channels() - returns list of FC channels found.
 ****************************************************************************** 
 */
channel_e_t agent_get_channels()
{
  channel_e_t head=NULL, curr=NULL;
  inventory_t *inv;

  setinvent();
  while (1) {
    inv = getinvent();
    if (inv && (inv->inv_class == INV_DISK) && 
	       (inv->inv_type == INV_SCSICONTROL) &&
	       (inv->inv_state == INV_FCADP || 
		inv->inv_state == INV_QL_2100 ||
		inv->inv_state == INV_QL_2200A ||
		inv->inv_state == INV_QL_2200)) {
      if (head == NULL) {
	head = calloc(1, sizeof(channel_e_struct_t));
	curr = head;
      }
      else {
	curr->next = calloc(1, sizeof(channel_e_struct_t));
	curr = curr->next;
      }
      curr->next = NULL;
      curr->id = inv->inv_controller;
      curr->type = inv->inv_state;
    }
    else 
    if (inv == NULL) {
      endinvent();
      break;
    }
  }
  return(head);
}

/*
 ******************************************************************************
 * agent_find_ch_by_cid() - returns enclosure containing drive 'tid'.
 ****************************************************************************** 
 */
channel_e_t agent_find_ch_by_cid(int cid)
{
  channel_e_t ch;

  for (ch = CH_HEAD; ch; ch = ch->next) {
    if (ch->id == cid)
      return(ch);
  }

  /* If we get here, we've reached the end of the list */
  return(NULL);
}

/*
 ******************************************************************************
 * agent_find_encl_by_tid() - returns enclosure containing drive 'tid'.
 ****************************************************************************** 
 */
encl_e_t agent_find_encl_by_tid(channel_e_t ch, int tid)
{
  encl_ref_t  enc_ref;
  encl_e_t    enc;

  for (enc_ref = ch->encl_ref_head; enc_ref; enc_ref = enc_ref->next) {
    enc = enc_ref->encl;
    if ((tid >= enc->phys_id * enc->drv_count) && 
	(tid < (enc->phys_id + 1) * enc->drv_count))
      return(enc);
  }

  /* If we get here, we've reached the end of the list */
  return(NULL);
}

/*
 ******************************************************************************
 * agent_find_encl_by_eid() - returns enclosure with physical id 'eid'.
 ****************************************************************************** 
 */
encl_e_t agent_find_encl_by_eid(channel_e_t ch, int eid)
{
  encl_ref_t  enc_ref;
  encl_e_t    enc;

  for (enc_ref = ch->encl_ref_head; enc_ref; enc_ref = enc_ref->next) {
    enc = enc_ref->encl;
    if (eid == enc->wwn)
      return(enc);
  }

  /* If we get here, we've reached the end of the list */
  return(NULL);
}

esi_pg2_gnrc_desc_t agent_find_e_elem(encl_e_t enc, int type_code)
{
  esi_pg2_gnrc_desc_t elem_desc;
  int type_index;

  elem_desc = (esi_pg2_gnrc_desc_t)((unchar *)enc->stat_buf + sizeof(esi_pg2_struct_t));
  for (type_index = 0; type_index < enc->type_count; ++type_index) {
    if (enc->types[type_index].type_code == type_code)
      return((esi_pg2_gnrc_desc_t)elem_desc);
    else 
      elem_desc += enc->types[type_index].elem_count + 1;
  }
  return(NULL);
}


void set_drv_led_ca(encl_e_t enc, drv_e_t drv, esi_pg2_drv_desc_t e_drv, int op)
{
  ((char *)e_drv)[0] = 0x80;	/* Clear all but select bit */
  e_drv->fault_reqstd = (op ? 1 : 0);
  DBG(D_ESI, "Turning %s LED for drive %d in enclosure %d\n", (op ? "ON" : "OFF"), drv->phys_id, enc->phys_id);
}

void set_drv_bypass_ca(encl_e_t enc, drv_e_t drv, esi_pg2_drv_desc_t e_drv, int op)
{
  ((char *)e_drv)[0] = 0x80;	/* Clear all but select bit */
  if (enc->connect_port)
    e_drv->enable_byp_B = (op ? 1 : 0);
  else
    e_drv->enable_byp_A = (op ? 1 : 0);
  DBG(D_ESI, "Turning %s BYPASS for drive %d in enclosure %d\n", (op ? "ON" : "OFF"), drv->phys_id, enc->phys_id);
}

int agent_set_drv_cntrl(channel_e_t ch, fcid_bitmap_t ids, void (*ca)(), int op)
{
  char devscsi_filename[256];
  int tid, i; 
  encl_e_t enc;
  drv_e_t drv;
  esi_pg2_drv_desc_t e_drv;
  esi_pg2_lcc_desc_t e_lcc;
  int state=0;
  char   *devname;
  int rc=0;

  for (tid = TID_LO; tid <= TID_HI; ++tid) {
    if (!FCID_ISSET(ids, tid))
      continue;

    enc = agent_find_encl_by_tid(ch, tid);
    if (enc == NULL) {
      rc = -tid;
      goto set_control_done;
    }

    LOCK(enc->lock);

    /* 
     * Put LCC into remote mode 
     */
    e_lcc = (esi_pg2_lcc_desc_t)agent_find_e_elem(enc, E_TYPE_LCC);
    if (e_lcc == NULL)
      fatal("agent_set_drv_led(): agent_find_e_elem returns NULL.\n");

    /* Set select bit in global descriptor */
    ((char *)e_lcc)[0] = 0x80;	/* Clear all but select bit */
    ++e_lcc;			/* Skip over global */

    /* Set select bit in LCC descriptor */
    ((char *)e_lcc)[0] = 0x80;

    /* Reset all other fields */
    ((char *)e_lcc)[1] = ((char *)e_lcc)[2] = ((char *)e_lcc)[3] = 0x00;

    DBG(D_ESI, "Putting encl %d into remote mode.\n", enc->phys_id);
    e_lcc->local_mode = 0;

    /* 
     * Set the appropriate mode in DRV descriptor
     */
    e_drv = (esi_pg2_drv_desc_t)agent_find_e_elem(enc, E_TYPE_DISK);
    if (e_drv == NULL)
      fatal("agent_set_drv_led(): agent_find_e_elem returns NULL.\n");

    /* Set select bit in global descriptor */
    ((char *)e_drv)[0] = 0x80;	/* Clear all but select bit */
    ++e_drv;			/* Skip over global */    

    for (i = 0, drv = enc->drv; i < enc->drv_count; ++i, ++drv, ++e_drv) {
      if (FCID_ISSET(ids, drv->phys_id)) {
	FCID_CLR(ids, drv->phys_id);
	(*ca)(enc, drv, e_drv, op);
      }
    } /* for (i) */

#if DG_WAR_15
    /* Rotate drive statuses before writing them out */
    {
      esi_pg2_drv_desc_struct_t e_drv_tmp[10];

      printf("XXXX - rotating drive statuses \n");

      e_drv = (esi_pg2_drv_desc_t)agent_find_e_elem(enc, E_TYPE_DISK);
      ++e_drv;
      for (i = 0; i < enc->drv_count; ++i, ++e_drv) {
	bcopy(e_drv, &e_drv_tmp[i], sizeof(esi_pg2_drv_desc_struct_t));
      }
      e_drv = (esi_pg2_drv_desc_t)agent_find_e_elem(enc, E_TYPE_DISK);
      ++e_drv;
      for (i = 0; i < enc->drv_count; ++i, ++e_drv) {
	bcopy(&e_drv_tmp[9-i], e_drv, sizeof(esi_pg2_drv_desc_struct_t));
      }
    }
#endif

#if 1				/* XXX */
    /* Clear bytes 4 thru 7 */
    for (i = 4; i < 8; ++i)
      *((unchar *)enc->stat_buf + i) = 0;
#endif
    
#if DG_WAR_14
    *((unchar *)enc->stat_buf + 1) = 0;
#endif

    /*
     * WRITE the control page, first trying the primary 8067 drive and then the
     * secondary.
     */
  set_control_retry:
    state = 0;
    while (1) {
      switch (state) {
      case 0:
      case 1:
	if (enc->esidrv[state & 0x3] != TID_UNDEF) {
	  devname = make_devscsi_path(ch->id, enc->esidrv[state], 0, devscsi_filename);
	  enc->stat_buf->page_code = SEND_ES_ENCL_CONTROL;
	  enc->stat_buf->page_len = enc->stat_blen_actual-4;
	  if (send_diagnostics(devname, (unchar *)enc->stat_buf, enc->stat_blen_actual) == 0) {
	    /*
	     * If succeeded on secondary, swap primary and secondary
	     */
	    if (state) {
	      u_char drv;
	      drv = enc->esidrv[0];
	      enc->esidrv[0] = enc->esidrv[1];
	      enc->esidrv[1] = drv;
	    }
	    goto set_control_success;
	  }
	  else {
	    if (state == 0) {
	      fprintf(stderr, "Primary ESI drive (%d) failure on FC channel %d: "
		              "trying secondary (%d)\n",
		              enc->esidrv[0], ch->id, enc->esidrv[1]);
	      syslog(LOG_CRIT, "Primary ESI drive (%d) failure on FC channel %d: "
		               "trying secondary (%d)\n",
		               enc->esidrv[0], ch->id, enc->esidrv[1]);
	    }
	    else {
	      fprintf(stderr, "Secondary ESI drive (%d) failure on FC channel %d: "
		              "Unable to set enclosure state\n",
		              enc->esidrv[1], ch->id);
	      syslog(LOG_CRIT, "Secondary ESI drive (%d) failure on FC channel %d: "
		               "Unable to set enclosure state\n",
		               enc->esidrv[1], ch->id);
	    }
	    ++state;
	  }
	}
	else
	  ++state;
	break;
      default:
	UNLOCK(enc->lock);
	rc = 1;
	goto set_control_done;
      }
    }
  set_control_success:
    UNLOCK(enc->lock);
  } /* for (tid) */
 set_control_done:
  if (rc == 0) {
    agent_get_encl_status(ch, enc, 0);
  }
  return(rc);
}

/*
 ******************************************************************************
 * agent_set_drv_led() - set/reset fault one or more LEDs associated
 * with drives.
 ****************************************************************************** 
 */
int agent_set_drv_led(channel_e_t ch, fcid_bitmap_t ids, int op)
{
  return(agent_set_drv_cntrl(ch, ids, set_drv_led_ca, op));
}

/*
 ******************************************************************************
 * agent_set_drv_bypass() - enabled/disable the LRC associated with drives.
 ****************************************************************************** 
 */
int agent_set_drv_bypass(channel_e_t ch, fcid_bitmap_t ids, int op)
{
  return(agent_set_drv_cntrl(ch, ids, set_drv_bypass_ca, op));
}

/*
 ******************************************************************************
 * agent_set_drv_remove() - remove one or more drives from loop. This
 * happens in 3 steps. (1) if drive is not already bypassed, spin it down
 *                     (2) attempt bypass via LPB primitive
 *                     (3) read status and verify that drives are bypassed
 *                     (4) attempt bypass via ESI mechanism if bypass failed.
 ****************************************************************************** 
 */
int agent_set_drv_remove(channel_e_t ch, fcid_bitmap_t ids, int spindown)
{
  char devscsi_filename[256];
  int tid;
  char scsiha_filename[256];
  char   *devname;
  encl_e_t enc;
  drv_e_t drv;
  esi_pg2_drv_desc_t e_drv;
  fcid_bitmap_t ids_save, ids_not_bp;
  int check_not_bp;
  int rc=0;
  int i;

  ids_save = FCID_NEW();
  ids_not_bp = FCID_NEW();
  
  FCID_COPY(ids, ids_save);
  /*
   * (A) Spin down drives and attempt to bypass them by using the LPB primitive
   */
  for (tid = TID_LO; tid <= TID_HI; ++tid) {
    if (!FCID_ISSET(ids, tid))
      continue;
    enc = agent_find_encl_by_tid(ch, tid);
    if (enc == NULL) {
      rc = -tid;
      goto remove_done;
    }
    for (i = 0, drv = enc->drv; i < enc->drv_count; ++i, ++drv) {
      if (FCID_ISSET(ids, drv->phys_id)) {
	FCID_CLR(ids, drv->phys_id);
	if (drv->drv_asserting_bypass || drv->enc_asserting_bypass) 
	  continue;
	if (spindown) {
	  devname = make_devscsi_path(ch->id, drv->phys_id, 0, devscsi_filename);
	  startstop_drive(devname, 0);
	}
	devname = make_scsiha_path(ch->id, scsiha_filename);
	if (enable_pbc(devname, drv->phys_id) == -1)
	  fprintf(stderr, "FC LPB failed for %d on channel %d. Will attempt ESI bypass.\n", drv->phys_id, ch->id);
      }
    } /* for (i) */
  } /* for (tid) */
  
  FCID_COPY(ids_save, ids);

  /*
   * (B) Check if drives are in fact bypassed and if not, attempt to
   *     bypass using the ESI controls.
   */
  check_not_bp = 0;
  for (tid = TID_LO; tid <= TID_HI; ++tid) {
    if (!FCID_ISSET(ids, tid))
      continue;
    enc = agent_find_encl_by_tid(ch, tid);
    if (enc == NULL) {
      rc = -tid;
      goto remove_done;
    }
    agent_get_encl_status(ch, enc, 0);
    for (i = 0, drv = enc->drv; i < enc->drv_count; ++i, ++drv) {
      if (FCID_ISSET(ids, drv->phys_id)) {
	FCID_CLR(ids, drv->phys_id);
	if (drv->drv_asserting_bypass || drv->enc_asserting_bypass)
	  continue;
	else {
	  FCID_SET(ids_not_bp, drv->phys_id);
	  check_not_bp = 1;
	}
      }
    } /* for (i) */
  } /* for (tid) */  
  if (check_not_bp) {
    rc = agent_set_drv_cntrl(ch, ids_not_bp, set_drv_bypass_ca, 1);
  }

 remove_done:
  FCID_COPY(ids_save, ids);
  FCID_FREE(ids_save);
  FCID_FREE(ids_not_bp);
  return(rc);
}

int agent_set_drv_insert(channel_e_t ch, fcid_bitmap_t ids, int spinup)
{
  char devscsi_filename[256];
  int tid;
  char scsiha_filename[256];
  char   *devname;
  encl_e_t enc;
  drv_e_t drv;
  esi_pg2_drv_desc_t e_drv;
  fcid_bitmap_t ids_save, ids_not_ubp;
  int check_not_ubp;
  int rc=0;
  int i;

  ids_save = FCID_NEW();
  ids_not_ubp = FCID_NEW();

  FCID_COPY(ids, ids_save);

  /*
   * (A) Unbypass drives using the LPE primitive
   */
  for (tid = TID_LO; tid <= TID_HI; ++tid) {
    if (!FCID_ISSET(ids, tid))
      continue;
    enc = agent_find_encl_by_tid(ch, tid);
    if (enc == NULL) {
      rc = -tid;
      goto insert_done;
    }
    for (i = 0, drv = enc->drv; i < enc->drv_count; ++i, ++drv) {
      if (FCID_ISSET(ids, drv->phys_id)) {
	FCID_CLR(ids, drv->phys_id);
	if (!drv->drv_asserting_bypass && !drv->enc_asserting_bypass) 
	  continue;
	devname = make_scsiha_path(ch->id, scsiha_filename);
	if (disable_pbc(devname, drv->phys_id) == -1)
	  fprintf(stderr, "FC LPE failed for %d on channel %d. Will attempt ESI upbypass.\n", drv->phys_id, ch->id);
      }
    } /* for (i) */
  } /* for (tid) */
  
  FCID_COPY(ids_save, ids);

  /*
   * (B) Check if drives are in fact unbypassed and if not, attempt to
   *     unbypass using the ESI controls.
   */
  check_not_ubp = 0;
  for (tid = TID_LO; tid <= TID_HI; ++tid) {
    if (!FCID_ISSET(ids, tid))
      continue;
    enc = agent_find_encl_by_tid(ch, tid);
    if (enc == NULL) {
      rc = -tid;
      goto insert_done;
    }
    agent_get_encl_status(ch, enc, 0);
    for (i = 0, drv = enc->drv; i < enc->drv_count; ++i, ++drv) {
      if (FCID_ISSET(ids, drv->phys_id)) {
	FCID_CLR(ids, drv->phys_id);
	if (drv->drv_asserting_bypass || drv->enc_asserting_bypass) {
	  FCID_SET(ids_not_ubp, drv->phys_id);
	  check_not_ubp = 1;
	}
      }
    } /* for (i) */
  } /* for (tid) */  
  if (check_not_ubp) {
    rc = agent_set_drv_cntrl(ch, ids_not_ubp, set_drv_bypass_ca, 0);
  }

  FCID_COPY(ids_save, ids);

  /* 
   * (C) Spin up drives
   */
  for (tid = TID_LO; tid <= TID_HI; ++tid) {
    if (!FCID_ISSET(ids, tid))
      continue;
    enc = agent_find_encl_by_tid(ch, tid);
    for (i = 0, drv = enc->drv; i < enc->drv_count; ++i, ++drv) {
      if (FCID_ISSET(ids, drv->phys_id)) {
	FCID_CLR(ids, drv->phys_id);
	if (spinup) {
	  devname = make_devscsi_path(ch->id, drv->phys_id, 0, devscsi_filename);
	  startstop_drive(devname, 1);
	}
      }
    } /* for (i) */
  } /* for (tid) */
 insert_done:
  FCID_COPY(ids_save, ids);  
  FCID_FREE(ids_save);
  FCID_FREE(ids_not_ubp);
  return(rc);
}

