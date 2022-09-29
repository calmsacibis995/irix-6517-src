/*
 * basic siginfo test
 * have two processes sending signal with SA_SIGINFO set
 * make sure that the siginfo pointer passed in by the kernel really work
 * make sure that can not queue more signals than the system limit(sysconf)
 * get EAGAIN otherwise.
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
int count= 1000;
int sa_onstack;
#define SIGSTACK_SIZE 8*1024
char *psigsp, *csigsp;
void childloop();
int sigqueue_test;
int max_sigusr2 = 10;
int actual_sigusr2;
int maxsigq;

#if defined(_ABI64) 
#define STKALIGN ~0xfLL
#elif defined(_ABIN32)
#define STKALIGN ~0xfL
#else
#define STKALIGN ~0x7L
#endif

main(argc, argv)
int argc;
char **argv;
{
   	extern int		optind;
   	extern char		*optarg;
	int			c;
 	setbuf(stdout, NULL);
	while ((c = getopt(argc, argv, "sc:q:")) != EOF) {
	  	switch (c) {
		case 'c':
	       		if ((count = strtol(optarg, (char **)0, 0)) <0) {
		   		usage();
				exit(1);
			}
			break;
		case 'q':
	       		if ((max_sigusr2 = strtol(optarg, (char **)0, 0)) <0) {
		   		usage();
				exit(1);
			}
			break;
	  	case 's':
			sa_onstack++;
			break;
		default:
			usage();
			exit(1);
		}
	}

	ppid = getpid();
	sighold(SIGUSR1);
	sighold(SIGUSR2);
	printf("Parent pid is %d\n",ppid);
	fflush(stdout);
	cpid = sproc(childloop, PR_SALL);
	if (cpid < 0) {
	    perror ("sproc failed");
	    return -1;
	}
	parentloop();
}

usage()
{
	fprintf(stderr,"siginfo [-s] [-c # of siginfo] [-q # of sigqueue]\n");
	exit(1);
}

parentloop()
{
	sigaction_t newact;
	int i;
	volatile double x = 2.4, y = 3.6;

	/* force save of fp context */
	x = x * y;

	/* get max sigq */
	maxsigq = sysconf(_SC_SIGQUEUE_MAX);
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

		psigsp = (char *)malloc(SIGSTACK_SIZE) + SIGSTACK_SIZE -1;
		psigsp = (char *)((unsigned long)psigsp & STKALIGN);
		ss.ss_sp = psigsp;
		ss.ss_onstack = 0;
		if (sigstack(&ss, (struct sigstack *)0) == -1) {
			perror("sigstack: fail sigstack");
			exit(-1);
		}
		newact.sa_flags |= SA_ONSTACK;
	}
	if (sigaction(SIGUSR1, &newact, (sigaction_t *)0)) {	
		perror("Failed sigaction");	
		exit(1);
	}
	printf("Doing siginfo check\n");
	sigrelse(SIGUSR1);
	
	while (count--) {
		/* wait for child to wake us up */
		printf("Number of SIGUSR1 signals remained %d\r",count);
		pause();
	}
	printf("Number of SIGUSR1 signals remained %d           \n",count);
	/* now for sigqueue test */
	sigqueue_test = 1;
	/* lets send 10 SIGUSR2 */
	printf("\nParent sends SIGUSR2\n");
	for (i=0; i<max_sigusr2; i++) {
		if ( sigqueue(cpid, SIGUSR2, actual_sigusr2)) {
			perror("siginfo: can't send SIGUSR2");
				exit(-1);
		} else {
			actual_sigusr2++;
		}
	}
printf("\nParent done sending %d SIGUSR2\n",actual_sigusr2);
	if (actual_sigusr2 > maxsigq) {
		fprintf(stderr,"Can queue more signal than system limit of %d\n", maxsigq);
		exit(-1);
	}
	/* now lets send SIGUSR1, when chld's sigusr1 sighandler
	 * detects that sigqueue_test is set, it will release SIGUSR2
	 * and get them all
	 */
printf("\nParent sends SIGUSR1\n");
	kill(cpid, SIGUSR1);

	/* now we just wait for another SIGUSR1 to terminate */
	while (sigqueue_test){
		pause();
		printf("After pause in parent...\n");
	}
	kill(cpid, SIGKILL);
}

void
parent_sigusr1(int sig, siginfo_t *sip, ucontext_t *scp)
{
	int err = 0;

	if (!sip) {
		fprintf(stderr,"\nsiginfo: NULL siginfo ptr in sig handler\n");
		fprintf(stderr,"siginfo: sig %d, sip %x scp %x\n", sig, sip, scp);
		err++;
		kill(cpid, SIGKILL);
		exit(-1);
	}
	if (psigsp) {
		siginfo_t *csip;
		ucontext_t *cscp;

		cscp = (ucontext_t *)(((unsigned long)psigsp -
		        sizeof(ucontext_t) - sizeof(siginfo_t)) & STKALIGN); 
		csip = (siginfo_t *)((unsigned long)cscp + sizeof(ucontext_t));
		if (cscp != scp) { 
			fprintf(stderr,"Address of scp is 0x%x should be 0x%x\n",scp, cscp);
			err++;
		}
		if (csip != sip) { 
			fprintf(stderr,"Address of sip is 0x%x should be 0x%x\n",sip, csip);
			err++;
		}
	}
	if (sip->si_signo != SIGUSR1) {
		printf("Received signal is %d not SIGUSR1, sig is %d\n",sip->si_signo, sig);
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
	/* now check to see if sigqueue test is on */
	if (sigqueue_test) {
		printf("\nChild unholds SIGUSR2\n");
		sigrelse(SIGUSR2);
	}
}	
	
void
child_sigusr2(int sig, siginfo_t *sip, ucontext_t *scp)
{
	static sigusr2_count = 0;
#if 1
	printf("Child received SIGUSR2 signal #%d with value %d\n",
	       sigusr2_count,
	       sip->si_value);
#endif
	sigusr2_count++;

	fflush(stdout);
	if (sigusr2_count == actual_sigusr2) {
		/* send parent this to terminate test */
		printf("\nTest done, and passes!\n");
		sigqueue_test = 0;
		kill(ppid, SIGUSR1);
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
	sighold(SIGUSR2);
/* 	setbuf(stdout, NULL); */
	printf("Child pid is %d\n\n\n", getpid());
	fflush(stdout);
	fflush(stdout);
	sleep(1); /* This sleep is needed so that the parent can set up
		   * handler with SA_INFO set before we queue up signals.
		   * Without the sleep the test fails on a uni. MP's
		   * did not seem to have a problem.
		   */
	/* set up sigaction */
	sighold(SIGUSR1);
	sigemptyset(&newact.sa_mask);
	newact.sa_flags = 0;
	newact.sa_handler = child_sigusr1;
	if (sa_onstack) {
		struct sigstack ss;

		csigsp = (char *)malloc(SIGSTACK_SIZE) + SIGSTACK_SIZE -1;
		csigsp = (char *)((unsigned long)csigsp & STKALIGN);
		ss.ss_sp = csigsp;
		ss.ss_onstack = 0;
		if (sigstack(&ss, (struct sigstack *)0) == -1) {
			perror("sigstack: fail sigstack");
			exit(-1);
		}
		newact.sa_flags |= SA_ONSTACK;
	}
	if (sigaction(SIGUSR1, &newact, (sigaction_t *)0)) {	
		perror("Failed sigaction");	
		exit(1);
	}
	/* set up sigaction */
	sigemptyset(&newact.sa_mask);
	newact.sa_flags = SA_SIGINFO;
	newact.sa_handler = child_sigusr2;
	if (sigaction(SIGUSR2, &newact, (sigaction_t *)0)) {	
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
	sigrelse(SIGUSR1);
	sigrelse(SIGALRM);
	while ( !sigqueue_test) {

		/* This section is used to send signal after signal
		 * until the arent gets count of them */
		kill(ppid, SIGUSR1);
		/* wait for itimer */
		pause();
		
	}
	/* Well time for the sigqueue test */
	/* First turn off the itimer as we do not need any more
	 * interupts */
	printf("\nchild turns off itimer\n");
	itv.it_interval.tv_sec = 0;
	itv.it_interval.tv_usec = 0;
	itv.it_value = itv.it_interval;
	setitimer(ITIMER_REAL, &itv, (struct itimerval *) 0);
	while(sigqueue_test) {
		pause();
	}
		
}	
