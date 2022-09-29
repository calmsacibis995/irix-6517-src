#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <signal.h>
#include <siginfo.h>
#include <ucontext.h>


void set_signals(struct sigaction *sig)
{
    sigaction(SIGINT, sig, NULL);
    sigaction(SIGPIPE, sig, NULL);
    sigaction(SIGABRT, sig, NULL);
    sigaction(SIGSEGV, sig, NULL);
    sigaction(SIGBUS, sig, NULL);

}

void signal_hndlr(int signl, siginfo_t *si,  void *u)
{
    switch(signl)
    {
	case SIGABRT:
	case SIGSEGV:
	case SIGBUS:
	    fprintf(stderr, "signal %d, si_signo %d, si_errno %d, si_code %d\n",
		    signl, si->si_signo, si->si_errno, si->si_code);
    }

    exit(1);
}

void signal_setup()
{
    struct sigaction sigact;

    sigact.sa_flags = (SA_RESETHAND | SA_SIGINFO);
    sigact.sa_sigaction = signal_hndlr;
    sigemptyset(&sigact.sa_mask);
    set_signals(&sigact);
}
