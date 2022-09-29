#ifndef __DSM_H
#define __DSM_H

extern void dsm_events_alarm_handler();
extern void dsm_events_child_handler();

/*
 * event stuff.
 */
extern __uint64_t dsm_throttle_rules_event(struct event_private *Pev);


/*
 * SSDB stuff.
 */
extern __uint64_t dsm_create_action_taken_record_ssdb(struct rule *Prule,
						      struct event_private *Pev);
extern __uint64_t dsm_init_all_event_rules_ssdb();
extern __uint64_t dsm_init_rule_ssdb(struct rule *rule,int Iobjid);
     
/*
 * SEH stuff.
 */
extern __uint64_t dsm_get_event_seh(struct event_private *Pev);

/*
 * Rules stuff.
 */
extern void dsm_rules_clean();
extern __uint64_t dsm_thread_initialize_rules();
extern __uint64_t dsm_write_all_actions_ssdb(struct event_private *Pev);
extern __uint64_t dsm_parse_action(char **action,int *timeout,int *retry,
			    char **user,char **envp[]);
extern __uint64_t dsm_add_event_rule(struct rule *Prule,
				     int class,int type);
extern __uint64_t dsm_reconfig_rule(struct event_private *Pev);
extern void dsm_free_action_in_rule(struct rule *Prule);
extern __uint64_t dsm_init_rule(int objid,struct rule **PPrule);
extern __uint64_t dsm_find_rule_hash(int objid,struct rule **PPrule);

/*
 * Execute action stuff..
 */
extern __uint64_t dsm_take_action(struct rule *Prule,struct event_private *Pev);
void   dsm_init_children();

/*
 * Init stuff.
 */
extern void dsm_clean_everyting();
extern __uint64_t dsm_thread_initialize(struct event_private *Pev);
extern void dsm_loop_init(struct event_private *Pev,__uint64_t *err);
     
/*
 * string stuff.
 */
extern __uint64_t stringtab_clean();
extern __uint64_t stringtab_init();
extern __uint64_t string_free(char *s,int maxlen);
extern char *string_dup(char *s,int maxlen);

/* 
 * sysid stuff.
 */
extern __uint64_t convertsysid(char *sysID);
#include <wait.h>
#include <pwd.h>

#endif
