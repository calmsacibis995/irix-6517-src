#ident "$Revision: 1.13 $"

/* 
 * ARCS Signal handling functions 
 */

/****
	TODO:
		- reset signals to default after exit or return
			from a loaded program
		- if signal catching code doesn't do setjmp/longjmp,
			psignal will return to saio and cuurently
			scandevs doesn't return anything, so you might
			be screwed - do something.

****/

#include <arcs/types.h>
#include <arcs/errno.h>
#include <arcs/signal.h>
#include <libsc.h>
#include <libsk.h>

static void sigdefault(void);
static void sigignore(void);

int ffs(int);

struct sig {
	SIGNALHANDLER handler;
};

static struct sig sigs[NUM_SIG_TYPES];

static unsigned int cursig;

void
_init_signals(void)
{
    int sigcount;

    for (sigcount = 0; sigcount < NUM_SIG_TYPES; sigcount++)
	sigs[sigcount].handler = sigdefault;

    sigs[SIGHUP].handler = sigignore;		/* note different default */
    
    cursig = 0;
}

SIGNALHANDLER
Signal(LONG Sig, SIGNALHANDLER Handler) 
{
	struct sig *s;
	SIGNALHANDLER tmphandler;

	if ((Sig < 1) || (Sig > NUM_SIG_TYPES))
		return ((SIGNALHANDLER)(__psint_t)EINVAL);
	s = &sigs[Sig];
	tmphandler = s->handler;
	if (Handler == SIGDefault)
		s->handler = sigdefault;
	else if (Handler == SIGIgnore)
		s->handler = sigignore;
	else
		s->handler = Handler;
	return (tmphandler);	
}

static void 
sigdefault (void)
{
#ifdef	DEBUG
	printf ("sigdefault: Console Interrupt !!!\n");
#endif
	EnterInteractiveMode();
}

static void
sigignore (void)
{
#ifdef	DEBUG
	printf ("sigignore: I'm ignoring you, ha, ha, ha\n");
#endif
	return;
}

/*
 * psignal - assert signal
 */
int
psignal(sig)
{
	if ((sig < 1) || (sig > NUM_SIG_TYPES))
		return (EINVAL);
	cursig |= 1 << (sig - 1);
	return (ESUCCESS);
}

/*
 * issig - return number of next asserted signal, if any
 */
int
issig(void)
{
	int sig;

	if ((sig = ffs(cursig)) > 0)
		return sig;
	else
		return 0;
}

#if	defined(__CPQ)
	/* return non zero if the specified signal is pending. */
int	is_specific_sig(int signal)
{
	return (cursig & (1 << (signal -1)) );
}
#endif

/*
 * psig - execute the hander for signal 'sig'
 */
void
psig(sig)
{
	struct sig *s = &sigs[sig];

	/* 
	 * Most signal handlers that catch cntrl-c will setup a 
	 * setjmp/longjmp to return to appropriate place. Otherwise
	 * return to saio code.
	 */ 
	cursig &= ~(1 << (sig - 1));

	(*s->handler)();
}

/*
 * ffs - find first bit set in word
 */
int
ffs(int sig)
{
	register int count = 0;

	if (!sig)
		return 0;

	while (! ((1 << count) & sig))
	    count++;

	return count+1;
}
