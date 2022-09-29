#ifndef _USRSIGNAL_H_
#define _USRSIGNAL_H_

#include <signal.h>

typedef struct {
  int sig;
  void (*usa_handler)(int);
  int usa_flags;
} usr_sigaction_t;

int install_signal_handlers(usr_sigaction_t *);

#endif  
