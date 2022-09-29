#ident "$Revision: 1.3 $"

#if VERS <= V_53
#include <sys/signal.h>
#endif
#include <signal.h>
#include <stdio.h>
#include "sig.h"

static int	gotintr;
static sigset_t	intrset;

/* ARGSUSED */
static void
interrupt(int sig, siginfo_t *info, void *uc)
{
	gotintr = 1;
}

void
blockint(void)
{
	(void)sigprocmask(SIG_BLOCK, &intrset, NULL);
}

void
clearint(void)
{
	gotintr = 0;
}

void
init_sig(void)
{
	sigaction_t	sa;

	memset(&sa, 0, sizeof(sa));
#if VERS <= V_53
	sa.sa_handler = interrupt;
#else
	sa.sa_sigaction = interrupt;
#endif
	(void)sigaction(SIGINT, &sa, NULL);
	(void)sigemptyset(&intrset);
	(void)sigaddset(&intrset, SIGINT);
}

int
seenint(void)
{
	return gotintr;
}

void
unblockint(void)
{
	(void)sigprocmask(SIG_UNBLOCK, &intrset, NULL);
}
