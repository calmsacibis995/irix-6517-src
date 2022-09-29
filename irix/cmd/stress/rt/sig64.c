/*
 * Test realtime signal delivery order.
 *
 * Parent queues up maxsigq/2 sigusr3 signals followed by maxsigq/2 sigusr2
 * signals and then tests that it gets them in the correct order.
 *
 * sig64 [-c count] [-R sig#1] [-S sig#2]
 */
#include <sys/types.h>
#include <sys/signal.h>
#include <sys/siginfo.h>
#include <sys/prctl.h>
#include <sys/ucontext.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <malloc.h>

void parent_sigusr1();
int cpid, ppid;
int count = 1000;
int sa_onstack;
#define SIGSTACK_SIZE 8*1024
char *psigsp, *csigsp;
void childloop();
int sigqueue_test;
int maxsigq;
int actual_sigusr2, actual_sigusr3;
int sigusr1 = 33;
int sigusr2 = SIGRTMIN;
int sigusr3 = SIGRTMAX;	/* derived from sigusr2-1 */
#define MAX_SIGQUEUE 100
int verbose;

main(argc, argv)
int argc;
char **argv;
{
   	extern int		optind;
   	extern char		*optarg;
	int			c;

	while ((c = getopt(argc, argv, "sS:T:c:v")) != EOF) {
	  	switch (c) {
		case 'v':
			verbose++;
			break;
		case 'c':
	       		if ((count = strtol(optarg, (char **)0, 0)) <0) {
		   		usage();
				exit(1);
			}
			break;
	  	case 's':
			sa_onstack++;
			break;
		case 'R':
			if ((sigusr1 = strtol(optarg, (char **)0, 0)) <0) {
				usage();
				exit(1);
			}
			break;
		case 'S':
			if ((sigusr3 = strtol(optarg, (char **)0, 0)) <0) {
				usage();
				exit(1);
			}
			break;
		default:
			usage();
			exit(1);
		}
	}

	sigusr2 = sigusr3-1;
	ppid = getpid();
	sighold(sigusr1);
	if ((cpid = sproc(childloop, PR_SALL)) == -1) {
		perror("sproc");
		exit(1);
	}
	parentloop();
}

usage()
{
	fprintf(stderr,"sig64 [-s] [-c # of siginfo] [-q # of sigqueue]\n");
	exit(1);
}

parentloop()
{
	sigaction_t newact;
	int i, rv;
	volatile double x = 2.4, y = 3.6;

	/* force save of fp context */
	x = x * y;

	/* get max sigq */
	maxsigq = sysconf(_SC_SIGQUEUE_MAX);
	printf("max queued signals are %d\n",maxsigq);
	if (maxsigq == -1) {
		perror("siginfo: failed sysconf(_SC_SIGQUEUE_MAX)");
		exit(-1);
	}
	/* set up sigaction */
	sigemptyset(&newact.sa_mask);
	newact.sa_flags = SA_SIGINFO;
	newact.sa_handler = parent_sigusr1;
	if (sa_onstack) {
		struct sigstack ss;

		psigsp = (char *)malloc(SIGSTACK_SIZE);
		psigsp = (char *)((__psunsigned_t)psigsp & ~7);
		ss.ss_sp = psigsp;
		ss.ss_onstack = 0;
		if (sigstack(&ss, (struct sigstack *)0) == -1) {
			perror("sigstack: fail sigstack");
			exit(-1);
		}
		newact.sa_flags |= SA_ONSTACK;
	}
	if (sigaction(sigusr1, &newact, (sigaction_t *)0)) {	
		perror("Failed sigaction");	
		exit(1);
	}
	printf("Doing siginfo check\n");
	sigrelse(sigusr1);
	
	while (count--) {
		/* wait for child to wake us up */
		if (verbose)
			printf("Number of signal %d remained %d\r",sigusr1,count);
		pause();
	}
	/* now for sigqueue test */
	sigqueue_test = 1;
	/* lets send some SIGUSR3 which will be queued by child */
	printf("\nParent sends signal %d\n",sigusr3);
	for (i=0; i<maxsigq/2; i++) {
		if (sigqueue(cpid, sigusr3, i)) {
			printf("siginfo: can't send signal %d",sigusr3);
			perror("");
			exit(-1);
		} else
			actual_sigusr3++;
	}
	printf("\nParent done sending %d signal %d\n",actual_sigusr3, sigusr3);
	/* lets send some SIGUSR2 which will be queued by child */
	printf("\nParent sends signal %d\n",sigusr2);
	for (i=0; i<maxsigq/2; i++) {
		if (sigqueue(cpid, sigusr2, i)) {
			printf("siginfo: can't send signal %d",sigusr2);
			perror("");
			exit(-1);
		} else
			actual_sigusr2++;
	}
	printf("\nParent done sending %d signal %d\n",actual_sigusr2, sigusr2);
	if ((actual_sigusr2 + actual_sigusr3) > MAX_SIGQUEUE) {
		fprintf(stderr,"Can queue more signal than system limit\n");
		exit(-1);
	}
	/* now lets send SIGUSR1, when chld's sigusr1 sighandler
	 * detects that sigqueue_test is set, it will release SIGUSR2
	 * and get them all
	 */
	printf("\nParent sends signal %d\n",sigusr1);
	kill(cpid, sigusr1);

	/* now we just wait for another SIGUSR1 to terminate */
	pause();
	kill(cpid, SIGKILL);
}

void
parent_sigusr1(int sig, siginfo_t *sip, ucontext_t *scp)
{
	int err = 0;
	if (!sip) {
		fprintf(stderr,"sig64: NULL siginfo ptr in sig handler\n");
		fprintf(stderr,"sig64: sig %d, sip %x scp %x\n", sig, sip, scp);
		err++;
		kill(cpid, SIGKILL);
		exit(-1);
	}
	if (psigsp) {
		siginfo_t *csip;
		ucontext_t *cscp;

		cscp = (ucontext_t *)psigsp ;
		csip = (siginfo_t *)((__psunsigned_t)psigsp +
				sizeof(ucontext_t));
		if (cscp != scp) { 
			fprintf(stderr,"Address of scp is 0x%x should be 0x%x\n",scp, cscp);
			err++;
		}
		if (csip != sip) { 
			fprintf(stderr,"Address of sip is 0x%x should be 0x%x\n",sip, csip);
			err++;
		}
	}
	if (sip->si_signo != sigusr1) {
		printf("Received signal is %d not signal %d, sig is %d\n",sip->si_signo, sigusr1, sig);
		err++;
	}
	if (sip->si_code != SI_USER) {
		printf("Recevied si_code is %d not SI_USER\n",sip->si_code);
		err++;
	}
	if (sip->si_errno != 0) {
		printf("Recevied si_errno is %d not 0\n",sip->si_errno);
		err++;
	}
	if (sip->si_pid != cpid) {
		printf("Sender pid is %d not %d\n",sip->si_pid, cpid);
		err++;
	}
	if (err) {
		fprintf(stderr,"Parent: %x %x %x\n",sip->si_signo, sip->si_code, sip->si_errno);
		kill(cpid, SIGKILL);
		exit(-1);
	}
	/* XXX what about _cld._utime,... and _fault._addr, _file._fd.. */

	/* check out ucontext */
	if ((scp->uc_flags & UC_ALL) != UC_ALL) {
		fprintf(stderr,"uc_flag is 0x%x not 0x%x\n",scp->uc_flags, UC_ALL);
		err++;
	}
	/* if the kernel can resume the program then uc_mcontext work */
	/* check out uc_stack? */
		
}	

void
child_sigusr1(int sig)
{
	sigset_t set;

	sigemptyset(&set);
	/* now check to see if sigqueue test is on */
	if (sigqueue_test) {
	    if (verbose)
		printf("\nChild unholds all signals\n");
		sigprocmask(SIG_SETMASK, &set, (sigset_t *)NULL);
	}

		
}	
	
int sigusr2_count;
int sigusr3_count;
void
child_sigusr2(int sig, siginfo_t *sip, ucontext_t *scp)
{

	sigusr2_count++;
	if (verbose)
		printf("Child received %d signal %d\r",sigusr2_count, sigusr2);
	if (sigusr3_count > 0) {
		printf("sigusr2 - sig %d count: %d %d, sig %d count %d %d\n",
		       sigusr2, sigusr2_count, actual_sigusr2,
		       sigusr3, sigusr3_count, actual_sigusr3);
		exit(1);
	}
	if (sigusr2_count == actual_sigusr2)
		if (verbose)
			printf("\n");
}

void
child_sigusr3(int sig, siginfo_t *sip, ucontext_t *scp)
{

	sigusr3_count++;
	if (verbose)
		printf("Child received %d signal %d\r",sigusr3_count, sigusr3);
	if (sigusr2_count != actual_sigusr2) {
		printf("sigusr3 - sig %d count: %d %d, sig %d count %d %d\n",
		       sigusr2, sigusr2_count, actual_sigusr2,
		       sigusr3, sigusr3_count, actual_sigusr3);
	}
	if (sigusr3_count == actual_sigusr3) {
		/* send parent this to terminate test */
		if (verbose)
		    printf("\nTest done!\n");
		kill(ppid, sigusr1);
	}
}
	

void
child_sigalrm()
{
}

void
childloop()
{
	sigaction_t newact;
	struct itimerval	itv;

	sleep(1); /* This sleep is needed so that the parent can set up
		   * handler with SA_INFO set before we queue up signals.
		   * Without the sleep the test fails on a uni. MP's
		   * did not seem to have a problem.
		   */
	/* set up sigaction */
	sighold(sigusr1);
	sigemptyset(&newact.sa_mask);
	newact.sa_flags = 0;
	newact.sa_handler = child_sigusr1;
	if (sa_onstack) {
		struct sigstack ss;

		csigsp = (char *)malloc(SIGSTACK_SIZE);
		csigsp = (char *)((__psunsigned_t)csigsp & ~7);
		ss.ss_sp = csigsp;
		ss.ss_onstack = 0;
		if (sigstack(&ss, (struct sigstack *)0) == -1) {
			perror("sigstack: fail sigstack");
			exit(-1);
		}
		newact.sa_flags |= SA_ONSTACK;
	}
	if (sigaction(sigusr1, &newact, (sigaction_t *)0)) {	
		perror("Failed sigaction");	
		exit(1);
	}
	/* set up sigaction */
	sighold(sigusr2);
	sigemptyset(&newact.sa_mask);
	sigaddset(&newact.sa_mask, sigusr3);
	newact.sa_flags = SA_SIGINFO;
	newact.sa_handler = child_sigusr2;
	if (sigaction(sigusr2, &newact, (sigaction_t *)0)) {	
		perror("Failed sigaction");	
		exit(1);
	}
	/* set up sigaction */
	sighold(sigusr3);
	sigemptyset(&newact.sa_mask);
	newact.sa_flags = SA_SIGINFO;
	newact.sa_handler = child_sigusr3;
	if (sigaction(sigusr3, &newact, (sigaction_t *)0)) {	
		perror("Failed sigaction");	
		exit(1);
	}
	sighold(SIGALRM);
     	itv.it_interval.tv_sec = 0;
     	itv.it_interval.tv_usec = 10000;
     	itv.it_value = itv.it_interval;
     	setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0);
	sigemptyset(&newact.sa_mask);
	newact.sa_flags = 0;
	newact.sa_handler = child_sigalrm;
	if (sigaction(SIGALRM, &newact, (sigaction_t *)0)) {	
		perror("Failed sigaction");	
		exit(1);
	}
	sigrelse(sigusr1);
	sigrelse(SIGALRM);
	
	kill(ppid, sigusr1);
	while (1) {
		/* wait for itimer */
		pause();
		if (sigqueue_test == 0)
			kill(ppid, sigusr1);
		else {
			if (verbose)
			    printf("\nchild turns off itimer\n");
     			itv.it_interval.tv_sec = 0;
     			itv.it_interval.tv_usec = 0;
     			itv.it_value = itv.it_interval;
     			setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0);
		}
	}
		
}	
