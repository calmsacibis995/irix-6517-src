#include <stdio.h>
#include "usrsignal.h"
#include "debug.h"

int install_signal_handlers(usr_sigaction_t *actions)
{
  sigaction_t act;
  usr_sigaction_t *sa;
  sigset_t mask_set, set;

  sigemptyset(&mask_set);
  for (sa = actions; sa->usa_handler != NULL; ++sa) {
#if 1
    sigemptyset(&set);
    sigaddset(&set, sa->sig);
    if (sigprocmask(SIG_UNBLOCK, &set, NULL))
      return(-1);
#endif

    if (sigaction(sa->sig, NULL, &act) == -1)
      return(-1);
    
    act.sa_handler = sa->usa_handler;
    act.sa_flags = sa->usa_flags;
    act.sa_mask = mask_set;
    if (sigaction(sa->sig, &act, NULL) == -1)
      return(-1);
  }
  return(0);
}
