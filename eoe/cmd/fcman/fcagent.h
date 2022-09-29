#ifndef _FCAGENT_H_
#define _FCAGENT_H_

#include <sys/types.h>
#include <ulocks.h>

#include "config.h"
#include "event.h"
#include "fcagent_structs.h"
#include "hash.h"

#define MAX_EVENTS 128
#define ARENA_FILENAME_BASE "/tmp/##fcagent"

typedef struct {
  char        *cfg_fn;	        /* Name of configuration file */
  agent_cfg_t  cfg;		/* Configuration structure */
  char        *arena_filename;	/* Shared Arena Filename */
  usptr_t     *arena;	        /* Shared Arena */
  channel_e_t  channel_head;	/* Head of channel struct list */
  ht_t         encl_ht;		/* Enclosure hash table */
  event_t      ev_array[MAX_EVENTS];
  uint         ev_head_index;	/* Head of event queue */
  uint         ev_tail_index;	/* Tail of event queue */
  ulock_t      ev_lock;		/* Event Q lock */
  usema_t     *ev_avail_sema4;	/* Event available sema4 */
  uint         flags;		/* Flags */
} app_data_struct_t;
extern app_data_struct_t app_data;

#define CFG_FN    (app_data.cfg_fn)
#define CFG       (app_data.cfg)
#define ARENA_FN  (app_data.arena_filename)
#define ARENA     (app_data.arena)
#define CH_HEAD   (app_data.channel_head)
#define ENCL_HT   (app_data.encl_ht)
#define EV_ARRAY  (app_data.ev_array)
#define EV_HEAD_I (app_data.ev_head_index)
#define EV_TAIL_I (app_data.ev_tail_index)
#define EV_LOCK   (app_data.ev_lock)
#define EV_AVAIL  (app_data.ev_avail_sema4)
#define FLGS      (app_data.flags)

#define F_TERMINATING 0x00000001

#define LOCK(_l_)     ussetlock(_l_)
#define UNLOCK(_l_)   usunsetlock(_l_)
#define P(_sema4_)    uspsema(_sema4_)
#define V(_sema4_)    usvsema(_sema4_)
#define TEST(_sema4_) ustestsema(_sema4_)

typedef struct {
  u_int  th_npid;
  void (*th_proc)(void *);
  pid_t  th_upid;
  char  *th_string;
} thread_ctl_struct_t;
typedef thread_ctl_struct_t *thread_ctl_t;

typedef struct {
  char  *ca_name;
  char  *ca_hostname;
  char  *ca_typename;
  time_t ca_timestamp;
  uint   ca_ch_id;
  uint   ca_encl_id;
  uint   ca_fru_type_id;
  uint   ca_fru_id;
  uint   ca_from_stat;
  uint   ca_to_stat;
} ca_struct_t;
typedef ca_struct_t *ca_t;

#define NPID_TH     0
#define NPID_EH     1

#define STACKSIZE 128000

#define SIGSOFTKILL             SIGUSR1

/* Defined in fcagent.c */
uint upid2npid(pid_t upid);
channel_e_t agent_find_ch_by_cid(int cid);
encl_e_t agent_find_encl_by_tid(channel_e_t ch, int tid);
encl_e_t agent_find_encl_by_eid(channel_e_t ch, int eid);

#endif
