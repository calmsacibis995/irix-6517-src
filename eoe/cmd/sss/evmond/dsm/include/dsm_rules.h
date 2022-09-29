#ifndef _DSM_RULES_H
#define _DSM_RULES_H

#include "Dlist.h"

/*
 * File contains definition of rules and all rules related structures.
 */
#define RULES_DEBUG    0	/* verbose messages for rules. */
#define NUM_RULES_HASH 128	/* size of hash table. */
#define CACHE_RULES    1        /* Do the caching for rules. */
#define MAX_ARGS 1024

#define DELETERULE(HEAD,TAIL,ELEM)          DELETE_DLIST(HEAD,TAIL,ELEM)      
#define ADDRULE(HEAD,TAIL,ELEM)             ADD_DLIST_AT_TAIL(HEAD,TAIL,ELEM) 
#define TRAVERSERULES(HEAD,END,FUNC,PKEY)   TRAVERSE_DLIST(HEAD,END,FUNC,PKEY)

/*
 * Each rule. 
 * Everything needed to execute the rule is here. 
 *
 */
struct rule {
  struct rule *Pnext;
  struct rule *Pbefore;
  struct rule *Phash;

  int     event_threshold_counts; /* Actual parameters of the */
  int     event_threshold_times;  /* threshold window. */

  int     rule_objid;		  /* object id for the rule. */
  int     refcnt;		  /* times the rule was accessed. */

  char    *forward_hname;         /* hostname to which event should */
				  /* be sent */

  BOOLEAN isthreshold;		  /* TO THRESHOLD OR NOT TO.. */
  BOOLEAN isdso;		  /* if action is a dso, this will be TRUE. */

  /*
   * Structure contains a lot of pointers to strings. As this means a lot
   * of memory space, it was decided to populate this structure only when 
   * needed.
   * The rest of the time the memory used by this structure is free'd up.
   * This is still a candidate to be thrown out of this structure completely,
   * if space requirements become dire.
   */
  struct action_string {
    char *user;			/* user to execute action as. */
    char **envp;		/* environment variables to set before */
                                /* executing action. */
    char *action;		/* action to take when event occured. */

    int timeout;		/* timeout value for action. */
    int retry;			/* number of times to retry action. */
  } *Paction;
};

#endif
