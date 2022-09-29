#include "common.h"
#include "dsm_events.h"
#include "dsm_events_private.h"
#include "dsm_rules.h"
#include "dsm.h"
#include "dsm_errors.h"

extern int verbose;

/*
 * Split up the action string into its components..
 * At present its components can be
 *   -- user to execute action as.
 *   -- timeout value for action.
 *   -- number of times to retry action.
 *   -- environment variables for action.
 */
__uint64_t dsm_parse_action(char **action,int *timeout,int *retry,
			    char **user,char **envp[])
{
  if(!action || !envp || !timeout || !retry || !user) /* silly check. */
    return 0;

  *envp=NULL;
  *timeout=0;
  *retry=0;
  *user=NULL;

  return 0;
}

