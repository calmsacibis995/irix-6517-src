/*
** this is a test program to check out sigaltstack
**
** 1. SIGSEGV on stack growth can now be caught with sigstack
** 2. can sigsetjmp/longjmp out of a sighandler that was handled on a sigstack
**    and the system will be able to maintain the sigstack state and mask
** 3. handle more than one signal handlers on the same sigstack at same time XX
** 4. make sure kernel can detect alternate stack overlflow XXX
*/
#include <malloc.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include "sys/siginfo.h"
#include "sys/ucontext.h"
#include <sys/time.h>
#include <setjmp.h>

void segvfunc();
void alarmfunc();
char *stackbuf;
int *sp;
sigjmp_buf env;
struct rlimit slimit;
struct sigaction sigact, osigact;
struct sigaltstack ss;
volatile int alarmcnt = 0;

main()
{
	int i;
	
	sp = &i;
	/* check out SIGSEGV */
	printf("Checking SIGSEGV on sigstack\n");
	getrlimit(RLIMIT_STACK, &slimit);

	sigact.sa_handler = segvfunc;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_ONSTACK|SA_SIGINFO;
	if (sigaction(SIGSEGV, &sigact, (struct sigaction *)NULL) == -1) {
		perror("sigaltstack: fail sigaction");
		exit(-1);
	}
	sigaction(SIGSEGV, (struct sigaction *)NULL, &osigact);
	printf("osigact: sa_handler = 0x%x sa_flags = 0x%x\n",osigact.sa_handler, osigact.sa_flags);
	
	stackbuf = (char *)malloc(SIGSTKSZ);
	ss.ss_sp = stackbuf + SIGSTKSZ - 1;
	ss.ss_size = SIGSTKSZ;
	ss.ss_flags = 0;
	if (sigaltstack(&ss, (struct sigaltstack *)NULL) == -1) {
		perror("sigaltstack: fail sigaltstack");
		exit(-1);
	}

	sp = (int *)((char *)sp - slimit.rlim_cur);
	/* this should cause a SIGSEGV */
	if (sigsetjmp(env, 1)) {
		/* back from segv handler */
		moresigtest();	
	}
	else {
printf("DO stack reference!\n");
		*sp = 0;
		/* shouldn't get here */
		printf("SYSTEM ERROR: Fail sigaltstack test.\nNo SIGSEGV received when stack growth exceeds rlimit!!!\n");
		exit(-1);
	}
	checksigstate();
}
		
void	
segvfunc(int sig, siginfo_t *sip, ucontext_t *ucp)
{
	int i;
	int err = 0;
	unsigned long pmask = ~(getpagesize() - 1);
	/* should be on sigstack now */	
printf("in segvfunc\n");
	if (!sip) {
		printf("No siginfo for SIGSEGV\n");
		goto segvout;
	}
	if ((char *)sip < stackbuf || 
		(char *)sip > (stackbuf + SIGSTKSZ - 1)) {
		printf("SYSTEM ERROR: Fail sigstack test.\nSignal handler is not delivered on the alternate stack\nsip = 0x%x, ucp = 0x%x\n",sip, ucp);
		exit(-1);
	}
	/* check out the siginfo stuff */
	if (sip->si_signo != SIGSEGV) {
		printf("Received signal is %d not signal %d, sig is %d\n",sip->si_signo, SIGSEGV, sig);
		err++;
	}
	if (sip->si_code != SEGV_MAPERR) {
		printf("Received si_code is %d not SEGV_MAPERR\n",sip->si_code);
		err++;
	}
	if (sip->si_errno != 0) {
		printf("Recevied si_errno is %d not 0\n",sip->si_errno);
		err++;
	}
	if (((unsigned long)sip->si_addr & pmask) != ((unsigned long)sp & pmask)) {
		printf("si_addr is wrong: %#x should be %#x\n",
		    sip->si_addr, sp);
		err++;
	}
	if (err) {
		printf("siginfo is wrong for SIGSEGV handler\n");
		exit(-1);
	}
segvout:
	/* now test the setjmp/longjmp feature */
printf("longjmp out of segvfunc\n");
	siglongjmp(env, 1);
}

moresigtest()
{
	struct itimerval tv;

	checksigstate();
	/* now setup for itimer, and SIGALRM */
	sigact.sa_handler = alarmfunc;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_ONSTACK;
	if (sigaction(SIGALRM, &sigact, (struct sigaction *)NULL) == -1) {
		perror("sigaltstack: fail sigaction on SIGALRM");
		exit(-1);
	}
	sighold(SIGALRM);
	tv.it_value.tv_sec = 0;
	tv.it_value.tv_usec = 10000;
	tv.it_interval.tv_sec = 0;
	tv.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &tv, (struct itimerval *)NULL);
printf("Waiting for SIGALRM\n");
	sigrelse(SIGALRM);
	while (alarmcnt == 0) {
		printf("alarmcnt=%d\n",alarmcnt);
	}
	checksigstate();
}	

void
alarmfunc()
{
	int i;
	/* should be on sigstack now */	
printf("in alarmfunc\n");
	if ((char *)&i < stackbuf || 
		(char *)&i > (stackbuf + SIGSTKSZ - 1)) {
		printf("SYSTEM ERROR: Fail sigaltstack test.\nSignal handler is not delivered on the alternate stack\n");
		exit(-1);
	}

	alarmcnt = 1;
printf("return from alarmfunc\n");	
	
}	

checksigstate()
{	
	struct sigaltstack oss;
	int omask;
	struct itimerval tv;
	int i;

	/* first lets make sure we're on user stack */
	if ((char *)&i >= stackbuf && (char *)&i < (stackbuf + SIGSTKSZ)) {
		printf("SYSTEM ERROR: Fail sigstack test.\nLongjmp out of a signal handler, but still running on the alternate stack\n");
		exit(-1);
	}
	/* lets check to make sure that signal states are still sane */
	if ((omask = sigsetmask(0)) == -1) {
		perror("sigaltstack: fail sigsetmask\n");
		exit(-1);
	}
	if (omask != 0) {
		printf("SYSTEM ERROR: Fail sigaltstack test.\nLongjmp out of a signal handler, but signal mask is not restored! Expected 0x%x, Actual 0x%x\n",0, omask);
		exit(-1);
	}
	if (sigaltstack((struct sigaltstack *)NULL, &oss) == -1) {
		perror("sigaltstack: fail sigaltstack\n");
		exit(-1);
	}
	if (oss.ss_flags != 0) {
		printf("SYSTEM ERROR: Fail sigaltstack test.\nLongjmp out of a signal handler, but ss_flags is still set!\n");
		exit(-1);
	}
}
