#include <sys/types.h>
#include <signal.h>
#include <sys/siginfo.h>
#include <sys/prctl.h>
#include <sys/ucontext.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>

void
handsig(int sig, siginfo_t *sip, ucontext_t *scp)
{
/* do not a thing */

}
int count = 5;
main(argc, argv)
int argc;
char **argv;
{
	sigaction_t action;
	int i, rv, maxsigq;
	sigset_t set;
	siginfo_t info;
	extern char		*optarg;
	int			c;
	while ((c = getopt(argc, argv, "c:")) != EOF) {
	  	switch (c) {
		case 'c':
	       		if ((count = strtol(optarg, (char **)0, 0)) <0) {
		   		usage();
				exit(1);
			}
			break;
		default:
			usage();
			exit(1);
		}
	}

	/* get max sigq */
	maxsigq = sysconf(_SC_SIGQUEUE_MAX);
	printf("max queued signals are %d\n",maxsigq);
	if (maxsigq == -1) {
		perror("siginfo: failed sysconf(_SC_SIGQUEUE_MAX)");
		exit(-1);
	}
	sighold(SIGRTMIN);
	/* set up sigaction */
	sigemptyset(&action.sa_mask);
	action.sa_flags = SA_SIGINFO;
	action.sa_handler = handsig;
	if (sigaction(SIGRTMIN, &action, (sigaction_t *)0)) {	
		perror("Failed sigaction");	
		exit(1);
	}

	printf("Doing sigwait check\n");


	sigemptyset(&set);
	sigaddset(&set,SIGRTMIN );
	for (i = 0; i <= count; i++) {
		union sigval val;
		val.sival_int= i;
		sigqueue(getpid(), SIGRTMIN, val);
	}
	for (i = 0; i <= count; i++) {
		int rval;
		rval = sigwaitinfo(&set, &info);
		printf("Got a signal #%d code #%d and value %d with rval %d\n",
		       info.si_signo,
		       info.si_code,
		       info.si_value,
		       rval);
	}
}



usage()
{
	fprintf(stderr,"sigwait [-c # of rtsigs]\n");
	exit(1);
}


