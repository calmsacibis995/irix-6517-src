#include "options.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <stdarg.h>		/* For varargs */
#include <stdlib.h>		/* For string conversion routines */
#include <string.h>
#include <bstring.h>
#include <ctype.h>		/* For character definitions */
#include <errno.h>
#include <syslog.h>
#include <sys/signal.h>		/* for UNIX signals */
#include <sys/wait.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>

#define XDR_SHORTCUT

#include "debug.h"
#include "fcagent.h"
#include "fcagent_vers.h"
#include "fcagent_structs.h"
#include "fcagent_rpc.h"
#include "usrsignal.h"

void th_priv_cleanup();
void th_fatal(char *format, ...);
void th_signal_handler(int sig);
int  th_callout(ca_t ca);
extern void agent_prog_1(struct svc_req *rqstp, SVCXPRT *transp);
char *frutype2str(uint type);
char *stat2str(uint type);

usr_sigaction_t th_actions[] = 
{
  {SIGINT,      th_signal_handler, SA_RESTART},
  {SIGHUP,      th_signal_handler, SA_RESTART},
  {SIGSOFTKILL, th_signal_handler, SA_RESTART},
  {NULL,        NULL,              NULL}
};

/*
 ******************************************************************************
 * th_priv_cleanup()
 * Thread cleanup routine - must be called prior to exiting from a thread
 ******************************************************************************
*/
void th_priv_cleanup()
{
  uint npid = upid2npid(getpid());

  DBG(D_THREAD, "th_priv_cleanup() : Thread %d : CLEANING UP\n", npid);
}

/*
 ******************************************************************************
 * th_fatal()
 ******************************************************************************
*/
void th_fatal(char *format, ...)
{
  uint npid = upid2npid(getpid());
  va_list ap;
  char tmpString[200];

  va_start(ap, format);
  vsprintf(tmpString, format, ap);
  printf("THREAD FATAL : NPID = 0x%08x : %s\n", npid, tmpString);
  va_end(ap);
  th_priv_cleanup();
  exit(1);
}

/*
 ******************************************************************************
 * th_signal_handler()
 ******************************************************************************
*/
void th_signal_handler(int sig)
{
  int npid = upid2npid(getpid());

  switch(sig) {
  case SIGINT:
    DBG(D_SIGNAL, "Thread NPID = %d received a SIGINT\n", npid);
    break;
  case SIGHUP:
    DBG(D_SIGNAL, "Thread NPID = %d received a SIGHUP\n", npid);
    break;
  case SIGSOFTKILL:
    DBG(D_SIGNAL, "Thread NPID = %d received a SIGSOFTKILL(%d)\n", npid, SIGSOFTKILL);
    th_priv_cleanup();
    exit(0);
    break;
  default:
    DBG(D_SIGNAL, "Thread NPID = %d received a unknown signal [%d]\n", npid, sig);
  }
}

/*
 ******************************************************************************
 * th_callout() - execv a callout function
 ******************************************************************************
*/
int th_callout(ca_t ca)
{
  char *argv[15];
  char tmpString[1024];
  char *p = tmpString;
  sigset_t set, oset;
  pid_t pid;
  int status, rc;

  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);
  sigprocmask(SIG_BLOCK, &set, &oset);

  pid = fork();
  if (pid == -1)
    th_fatal("fork() failed : %s\n", strerror(errno));
  else 
    if (pid == 0) {
      bzero(argv, sizeof(argv));
      argv[0] = ca->ca_name;
      argv[1] = ca->ca_hostname;
      argv[2] = ca->ca_typename;
      sprintf(tmpString, "%s", ctime(&ca->ca_timestamp)); argv[3] = p; p[strlen(p)-1] = '\0'; p += strlen(p) + 1;
      sprintf(p, "%d", ca->ca_ch_id); argv[4] = p; p += strlen(p) + 1;
      sprintf(p, "%d", ca->ca_encl_id); argv[5] = p; p += strlen(p) + 1;
      sprintf(p, "%s", frutype2str(ca->ca_fru_type_id)); argv[6] = p; p += strlen(p) + 1;
      sprintf(p, "%d", ca->ca_fru_id); argv[7] = p; p += strlen(p) + 1;
      sprintf(p, "%s", stat2str(ca->ca_from_stat)); argv[8] = p; p += strlen(p) + 1;
      sprintf(p, "%s", stat2str(ca->ca_to_stat)); argv[9] = p; p += strlen(p) + 1;
      if (execvp(argv[0], argv) == -1)
	exit(errno);
    } 
    else {
      wait(&status);
      if (WIFSTOPPED(status)) {
	th_fatal("th_callout() failed: Received a SIGCHLD (STOPPED).\n");
      }
      else 
      if (WIFEXITED(status)) {
	rc = WEXITSTATUS(status);
	if (rc) {
	  DBG(D_CO, "FCAGENT callout (%s) failed: %s\n", ca->ca_name, strerror(rc));
	  syslog(LOG_ERR, "FCAGENT callout (%s) failed: %s\n", ca->ca_name, strerror(rc));
	}
	return(rc);
      }
      else
      if (WIFSIGNALED(status)) {
	rc = WTERMSIG(status);
	if (rc) {
	  DBG(D_CO, "FCAGENT callout (%s) terminated on signal %d\n", ca->ca_name, rc);	
	  syslog(LOG_ERR, "FCAGENT callout (%s) terminated on signal %d\n", ca->ca_name, rc);	
	}
	return(-1);
      }
      else 
	return(0);
    }
  /* NOTREACHED */
}

/*
 ******************************************************************************
 * EVENT HANDLER THREAD
 ******************************************************************************
*/
void event_handler_proc(void *arg)
{
  int npid = (int) arg;
  sigset_t oset, set;
  ca_struct_t ca;
  event_t ev;
  char hostname[MAXHOSTNAMELEN];
  
  if (install_signal_handlers(th_actions) == -1)
    th_fatal("install_signal_handlers() failed : %s\n", strerror(errno));
  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);
  if (sigprocmask(SIG_UNBLOCK, &set, &oset))
    th_fatal("sigprocmask() failed : %s\n", strerror(errno));

  if (gethostname(hostname, MAXHOSTNAMELEN))
    th_fatal("gethostname() failed: %s\n", strerror(errno));

  while (1) {
    ev = ev_dequeue();

    switch (ev->ev_type) {
    case EVT_TYPE_INFO:
      ca.ca_typename = "INFO";
      syslog(LOG_INFO, "FRU state change: %s #%d in enclosure %d on channel %d: %s --> %s\n",
	     frutype2str(ev->ev_elem_type), 
	     ev->ev_elem_id, 
	     ev->ev_encl_id, 
	     ev->ev_ch_id,
	     stat2str(ev->ev_old_status),
	     stat2str(ev->ev_new_status));
      break;
    case EVT_TYPE_CONFIG:
      ca.ca_typename = "RECONFIG";
      syslog(LOG_NOTICE, "FRU reconfiguration: %s #%d in enclosure %d on channel %d: %s --> %s\n",
	     frutype2str(ev->ev_elem_type), 
	     ev->ev_elem_id, 
	     ev->ev_encl_id, 
	     ev->ev_ch_id,
	     stat2str(ev->ev_old_status),
	     stat2str(ev->ev_new_status));
      break;
    case EVT_TYPE_FAILURE:
      ca.ca_typename = "FAILURE";
      syslog(LOG_CRIT, "FRU failure: %s #%d in enclosure %d on channel %d\n",
	     frutype2str(ev->ev_elem_type), 
	     ev->ev_elem_id, 
	     ev->ev_encl_id, 
	     ev->ev_ch_id);
      syslog(LOG_CRIT, "");
      break;
    }
    ca.ca_name = CFG->ac_stat_change_co; 
    ca.ca_hostname = hostname;
    ca.ca_timestamp = ev->ev_timestamp;
    ca.ca_ch_id = ev->ev_ch_id;
    ca.ca_encl_id = ev->ev_encl_id;
    ca.ca_fru_type_id = ev->ev_elem_type;
    ca.ca_fru_id = ev->ev_elem_id;
    ca.ca_from_stat = ev->ev_old_status;
    ca.ca_to_stat = ev->ev_new_status;
    th_callout(&ca);
  }
}

/*
 ******************************************************************************
 * RCP REQUEST HANDLER THREAD
 ******************************************************************************
*/
void request_handler_proc(void *arg)
{
  int npid = (int) arg;
  register SVCXPRT *transp;

  if (install_signal_handlers(th_actions) == -1)
    th_fatal("install_signal_handlers() failed : %s\n", strerror(errno));
  {
    sigset_t oset, set;

    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    if (sigprocmask(SIG_UNBLOCK, &set, &oset))
      th_fatal("sigprocmask() failed : %s\n", strerror(errno));
  }

  (void) pmap_unset(AGENT_PROG, AGENT_VERS);

#if 0
	transp = svcudp_create(RPC_ANYSOCK);
	if (transp == NULL) {
		fprintf(stderr, "cannot create udp service.");
		exit(1);
	}
	if (!svc_register(transp, AGENT_PROG, AGENT_VERS, agent_prog_1, IPPROTO_UDP)) {
		fprintf(stderr, "unable to register (AGENT_PROG, AGENT_VERS, udp).");
		exit(1);
	}
#endif

#if 1
	transp = svctcp_create(RPC_ANYSOCK, 0, 0);
	if (transp == NULL) {
		fprintf(stderr, "cannot create tcp service.");
		exit(1);
	}
	if (!svc_register(transp, AGENT_PROG, AGENT_VERS, agent_prog_1, IPPROTO_TCP)) {
		fprintf(stderr, "unable to register (AGENT_PROG, AGENT_VERS, tcp).");
		exit(1);
	}
#endif

	svc_run();
	fprintf(stderr, "svc_run returned");
	exit(1);
	/* NOTREACHED */
}

/*
 ******************************************************************************
 * agent_get_vers_1() - string AGENT_GET_VERS(void) = 1;
 ******************************************************************************
 */
char **agent_get_vers_1()
{
  static char *VERSION=VERS;

  return(&VERSION);
}

/*
 ******************************************************************************
 * agent_get_config_1() - cfg_t AGENT_GET_CONFIG(void) = 2;
 ******************************************************************************
 */
cfg_t *agent_get_config_1()
{
  static cfg_t *config = NULL;
  int ch_count, encl_count;
  channel_e_t ch;
  encl_ref_t eref;
  int i,j;

  if (config == NULL) {
    /*
     * Walk down channel list and count the number available.
     */
    for (ch_count = 0, ch = CH_HEAD; ch; ++ch_count, ch = ch->next)
      ;
    config = calloc(1, sizeof(cfg_t));
    config->cfg_t_len = ch_count;
    if ((config->cfg_t_val = calloc(ch_count, sizeof(ch_cfg_t))) == NULL)
      th_fatal("agent_get_config_1() failed: %s\n", strerror(errno));
    for (i = 0, ch = CH_HEAD; i < ch_count; ++i, ch = ch->next) {
      config->cfg_t_val[i].ch_id = ch->id;
    }

    /*
     * For each channel walk down the enclosure reference list to see
     * if anything has changed 
     */
    for (i = 0, ch = CH_HEAD; i < ch_count; ++i, ch = ch->next) {
      for (encl_count = 0, eref = ch->encl_ref_head; eref; ++encl_count, eref = eref->next)
	;
      if (config->cfg_t_val[i].encl_id_val && config->cfg_t_val[i].encl_id_len != encl_count) {
	free(config->cfg_t_val[i].encl_id_val);
	free(config->cfg_t_val[i].stat_gencode_val);
	config->cfg_t_val[i].encl_id_val = NULL;
	config->cfg_t_val[i].encl_id_len = 0;
	config->cfg_t_val[i].stat_gencode_val = NULL;
	config->cfg_t_val[i].stat_gencode_len = 0;
      }
      if (config->cfg_t_val[i].encl_id_val == NULL) {
	config->cfg_t_val[i].encl_id_len = encl_count;
	if ((config->cfg_t_val[i].encl_id_val = calloc(encl_count, sizeof(u_int))) == NULL)
	  th_fatal("agent_get_config_1() failed: %s\n", strerror(errno));
	config->cfg_t_val[i].stat_gencode_len = encl_count;
	if ((config->cfg_t_val[i].stat_gencode_val = calloc(encl_count, sizeof(u_int))) == NULL)
	  th_fatal("agent_get_config_1() failed: %s\n", strerror(errno));
      }
      for (j = 0, eref = ch->encl_ref_head; j < encl_count; ++j, eref = eref->next) {
	config->cfg_t_val[i].encl_id_val[j] = eref->encl->wwn;
	config->cfg_t_val[i].stat_gencode_val[j] = eref->encl->status_gen_code;
      }
    }
  }
  return(config);
}

/*
 ******************************************************************************
 * agent_get_status_1() - enc_stat_t AGENT_GET_STATUS(enc_addr_t) = 3;
 ******************************************************************************
 */
enc_stat_t *agent_get_status_1(enc_addr_t *in_addr)
{
  static enc_stat_t *out_stat;
  channel_e_t ch;
  encl_e_t encl;
  int i;

  ch = agent_find_ch_by_cid(in_addr->ch_id);
  if (ch == NULL)
    th_fatal("agent_get_status_1() failed: Cannot find channel %d\n", in_addr->ch_id);
  encl = agent_find_encl_by_eid(ch, in_addr->encl_id);
  if (encl == NULL)
    th_fatal("agent_get_status_1() failed: Cannot find enclosure %d on channel %d\n",
	     in_addr->encl_id, in_addr->ch_id);

  /*
   * Free structs from previous query
   */
  if (out_stat) {
    free(out_stat->drv_val);
    free(out_stat->fan_val);
    free(out_stat->ps_val);
    free(out_stat->lcc_val);
    free(out_stat);
    out_stat = NULL;
  }
    
  out_stat = calloc(1, sizeof(enc_stat_t));
  out_stat->encl_id = in_addr->encl_id;
  out_stat->encl_status = encl->status;
  out_stat->encl_phys_id = encl->phys_id;
  bcopy(encl->vid, out_stat->encl_vid, sizeof(encl->vid));
  bcopy(encl->pid, out_stat->encl_pid, sizeof(encl->pid));
  bcopy(encl->prl, out_stat->encl_prl, sizeof(encl->prl));
  bcopy(&encl->wwn, out_stat->encl_wwn, sizeof(encl->wwn));

  out_stat->encl_lccstr_len = encl->lcc_str_len;
  out_stat->encl_lccstr_val = encl->lcc_str;

  out_stat->drv_len = encl->drv_count;
  out_stat->drv_val = calloc(encl->drv_count, sizeof(drv_stat_t));
  for (i = 0; i < encl->drv_count; ++i) {
    out_stat->drv_val[i].drv_stat_t_len = sizeof(drv_e_struct_t);
    out_stat->drv_val[i].drv_stat_t_val = (char *)&encl->drv[i];
  }

  out_stat->ps_len = encl->ps_count;
  out_stat->ps_val = calloc(encl->ps_count, sizeof(ps_stat_t));
  for (i = 0; i < encl->ps_count; ++i) {
    out_stat->ps_val[i].ps_stat_t_len = sizeof(ps_e_struct_t);
    out_stat->ps_val[i].ps_stat_t_val = (char *)&encl->ps[i];
  }

  out_stat->fan_len = encl->fan_count;
  out_stat->fan_val = calloc(encl->fan_count, sizeof(fan_stat_t));
  for (i = 0; i < encl->fan_count; ++i) {
    out_stat->fan_val[i].fan_stat_t_len = sizeof(fan_e_struct_t);
    out_stat->fan_val[i].fan_stat_t_val = (char *)&encl->fan[i];
  }

  out_stat->lcc_len = encl->lcc_count;
  out_stat->lcc_val = calloc(encl->lcc_count, sizeof(lcc_stat_t));
  for (i = 0; i < encl->lcc_count; ++i) {
    out_stat->lcc_val[i].lcc_stat_t_len = sizeof(lcc_e_struct_t);
    out_stat->lcc_val[i].lcc_stat_t_val = (char *)&encl->lcc[i];
  }
  return(out_stat);
}

/*
 ******************************************************************************
 * agent_set_drv_remove_1() - unsigned int AGENT_SET_DRV_REMOVE(drive_op_t) = 4;
 ******************************************************************************
 */
int *agent_set_drv_remove_1(drive_op_t *dop)
{
  channel_e_t ch;
  u_int ch_id = dop->ch_id;
  u_int op = dop->op;
  fcid_bitmap_t fcid_bm = (fcid_bitmap_t)dop->fcid_bm.fcid_t_val;
  int i;
  char hostname[MAXHOSTNAMELEN];
  ca_struct_t ca;
  static int result=0;


  if (__debug & D_ESI) {
    printf("DRV_REMOVE: CH = %d, OP = %d, FCIDS = ", ch_id, op);
    for (i = 0; i < NFCIDBITS; ++i)
      if (FCID_ISSET(fcid_bm, i))
	printf("%d ", i);
    printf("\n");
  }

  if (gethostname(hostname, MAXHOSTNAMELEN))
    th_fatal("gethostname() failed: %s\n", strerror(errno));

  /* 
   * Execute pre-remove callout for all devices to be removed
   */
  for (i = 0; i < NFCIDBITS; ++i)
    if (FCID_ISSET(fcid_bm, i)) {
      ca.ca_name = CFG->ac_preremovalco;
      ca.ca_typename = "PRE-REMOVE";
      ca.ca_hostname = hostname;
      ca.ca_timestamp = time(NULL);
      ca.ca_ch_id = ch_id;
      ca.ca_encl_id = -1;
      ca.ca_fru_type_id = EVT_ELEM_TYPE_DISK;
      ca.ca_fru_id = i;
      ca.ca_from_stat = -1;
      ca.ca_to_stat = -1;
      th_callout(&ca);
    }

  ch = agent_find_ch_by_cid(ch_id);
  if (ch == NULL)
    th_fatal("agent_find_ch_by_cid() failed: Cannot find channel %d\n", ch_id);
  result = agent_set_drv_remove(ch, fcid_bm, 1);
  DBG(D_ESI, "return value = %d\n", result);

  if (result)
    return(&result);

  /* 
   * Execute post-remove callout for all devices to be removed
   */
  for (i = 0; i < NFCIDBITS; ++i)
    if (FCID_ISSET(fcid_bm, i)) {
      ca.ca_name = CFG->ac_postremovalco;
      ca.ca_typename = "POST-REMOVE";
      ca.ca_hostname = hostname;
      ca.ca_timestamp = time(NULL);
      ca.ca_ch_id = ch_id;
      ca.ca_encl_id = -1;
      ca.ca_fru_type_id = EVT_ELEM_TYPE_DISK;
      ca.ca_fru_id = i;
      ca.ca_from_stat = -1;
      ca.ca_to_stat = -1;
      th_callout(&ca);
    }
  return(&result);
}

/*
 ******************************************************************************
 * agent_set_drv_insert_1() - unsigned int AGENT_SET_DRV_INSERT(drive_op_t) = 5;
 ******************************************************************************
 */
int *agent_set_drv_insert_1(drive_op_t *dop)
{
  channel_e_t ch;
  u_int ch_id = dop->ch_id;
  u_int op = dop->op;
  fcid_bitmap_t fcid_bm = (fcid_bitmap_t)dop->fcid_bm.fcid_t_val;
  int i;
  char hostname[MAXHOSTNAMELEN];
  ca_struct_t ca;
  static int result=0;

  if (__debug & D_ESI) {
    printf("DRV_INSERT: CH = %d, OP = %d, FCIDS = ", ch_id, op);
    for (i = 0; i < NFCIDBITS; ++i)
      if (FCID_ISSET(fcid_bm, i))
	printf("%d ", i);
    printf("\n");
  }

  ch = agent_find_ch_by_cid(ch_id);
  if (ch == NULL)
    th_fatal("agent_find_ch_by_cid() failed: Cannot find channel %d\n", ch_id);
  result = agent_set_drv_insert(ch, fcid_bm, 1);
  DBG(D_ESI, "return value = %d\n", result);

  if (result)
    return(&result);
  
  if (gethostname(hostname, MAXHOSTNAMELEN))
    th_fatal("gethostname() failed: %s\n", strerror(errno));
  
  /* 
   * Execute post-insert callout for all devices to be removed
   */
  for (i = 0; i < NFCIDBITS; ++i)
    if (FCID_ISSET(fcid_bm, i)) {
      ca.ca_name = CFG->ac_postinsertco;
      ca.ca_typename = "POST-INSERT";
      ca.ca_hostname = hostname;
      ca.ca_timestamp = time(NULL);
      ca.ca_ch_id = ch_id;
      ca.ca_encl_id = -1;
      ca.ca_fru_type_id = EVT_ELEM_TYPE_DISK;
      ca.ca_fru_id = i;
      ca.ca_from_stat = -1;
      ca.ca_to_stat = -1;
      th_callout(&ca);
    }
  return(&result);
}

/*
 ******************************************************************************
 * agent_set_drv_led_1() - unsigned int AGENT_SET_DRV_LED(drive_op_t) = 6;
 ******************************************************************************
 */
int *agent_set_drv_led_1(drive_op_t *dop)
{
  channel_e_t ch;
  u_int ch_id = dop->ch_id;
  u_int op = dop->op;
  fcid_bitmap_t fcid_bm = (fcid_bitmap_t)dop->fcid_bm.fcid_t_val;
  int i;
  static int result=0;

  if (__debug & D_ESI) {
    printf("DRV_LED: CH = %d, OP = %d, FCIDS = ", ch_id, op);
    for (i = 0; i < NFCIDBITS; ++i)
      if (FCID_ISSET(fcid_bm, i))
	printf("%d ", i);
    printf("\n");
  }

  ch = agent_find_ch_by_cid(ch_id);
  if (ch == NULL)
    th_fatal("agent_find_ch_by_cid() failed: Cannot find channel %d\n", ch_id);
  result = agent_set_drv_led(ch, fcid_bm, op);
  DBG(D_ESI, "return value = %d\n", result);

  return(&result);
}

/*
 ******************************************************************************
 * agent_set_drv_bypass_1() - unsigned int AGENT_SET_DRV_BYPASS(drive_op_t) = 7;
 ******************************************************************************
 */
int *agent_set_drv_bypass_1(drive_op_t *dop)
{
  channel_e_t ch;
  u_int ch_id = dop->ch_id;
  u_int op = dop->op;
  fcid_bitmap_t fcid_bm = (fcid_bitmap_t)dop->fcid_bm.fcid_t_val;
  int i;
  static int result=0;

  if (__debug & D_ESI) {
    printf("DRV_BYPASS: CH = %d, OP = %d, FCIDS = ", ch_id, op);
    for (i = 0; i < NFCIDBITS; ++i)
      if (FCID_ISSET(fcid_bm, i))
	printf("%d ", i);
    printf("\n");
  }

  ch = agent_find_ch_by_cid(ch_id);
  if (ch == NULL)
    th_fatal("agent_find_ch_by_cid() failed: Cannot find channel %d\n", ch_id);
  result = agent_set_drv_bypass(ch, fcid_bm, op);
  DBG(D_ESI, "return value = %d\n", result);

  return(&result);
}

char *frutype2str(uint type)
{
  switch(type) {
  case EVT_ELEM_TYPE_DISK:
    return("DISK");
  case EVT_ELEM_TYPE_PS:
    return("PS");
  case EVT_ELEM_TYPE_FAN:
    return("FAN");
  case EVT_ELEM_TYPE_UPS:
    return("UPS");
  case EVT_ELEM_TYPE_LCC:
    return("LCC");
  default:
    return("Unknown");
  }
}

char *stat2str(uint stat)
{
  switch(stat) {
  case EVT_ELEM_STS_OK:
    return("OK");
  case EVT_ELEM_STS_OFF:
    return("OFF");
  case EVT_ELEM_STS_FAILED:
    return("FAILED");
  case EVT_ELEM_STS_NOT_PRESENT:
    return("NOT-PRESENT");
  case EVT_ELEM_STS_BYPASSED:
    return("BYPASSED");
  case EVT_ELEM_STS_PEER_FAILED:
    return("PEER-FAILED");
  default:
    return("Unknown");
  }
}
