/*
** this is a test program to check out sigstack
**
** 1. SIGSEGV on stack growth can now be caught with sigstack
** 2. can BSDsetjmp/longjmp out of a sighandler that was handled on a sigstack
**    and the system will be able to maintain the sigstack state and mask
** 3. handle more than one signal handlers on the same sigstack
*/
#define _BSD_COMPAT
#include <malloc.h>
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/time.h>
#include <setjmp.h>

void segvfunc();
void alarmfunc();
#define SIGSTACK_SIZE	4096*2
char *stackbuf;
int *sp;
jmp_buf env;
struct rlimit slimit;
struct sigvec vec;
struct sigstack ss;
volatile int alarmcnt = 0;
main()
{
	int i;
	
	sp = &i;
	/* check out SIGSEGV */
	printf("Checking SIGSEGV on sigstack\n");
	getrlimit(RLIMIT_STACK, &slimit);

	vec.sv_handler = segvfunc;
	vec.sv_mask = 0;
	vec.sv_flags = SV_ONSTACK;
	if (sigvec(SIGSEGV, &vec, (struct sigvec *)NULL) == -1) {
		perror("sigstack: fail sigvec");
		exit(-1);
	}
	
	stackbuf = (char *)malloc(SIGSTACK_SIZE);
	ss.ss_sp = stackbuf + SIGSTACK_SIZE - 1;
	ss.ss_onstack = 0;
	if (sigstack(&ss, (struct sigstack *)NULL) == -1) {
		perror("sigstack: fail sigstack");
		exit(-1);
	}

	sp = (int *)((char *)sp - slimit.rlim_cur);
	/* this should cause a SIGSEGV */
	if (setjmp(env)) {
		/* back from segv handler */
		moresigtest();	
	}
	else {
printf("DO stack reference!\n");
		*sp = 0;
		/* shouldn't get here */
		printf("SYSTEM ERROR: Fail sigstack test.\nNo SIGSEGV received when stack growth exceeds rlimit!!!\n");
		exit(-1);
	}
	checksigstate();
}
		
void	
segvfunc()
{
	int i;
	/* should be on sigstack now */	
printf("in segvfunc\n");
	if ((char *)&i < stackbuf || 
		(char *)&i > (stackbuf + SIGSTACK_SIZE - 1)) {
		printf("SYSTEM ERROR: Fail sigstack test.\nSignal handler is not delivered on the alternate stack\n");
		exit(-1);
	}

	/* now test the setjmp/longjmp feature */
printf("longjmp out of segvfunc\n");
	longjmp(env, 1);
}

moresigtest()
{
	struct itimerval tv;

	checksigstate();
	/* now setup for itimer, and SIGALRM */
	vec.sv_handler = alarmfunc;
	vec.sv_mask = 0;
	vec.sv_flags = SV_ONSTACK;
	if (sigvec(SIGALRM, &vec, (struct sigvec *)NULL) == -1) {
		perror("sigstack: fail sigvec on SIGALRM");
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
		(char *)&i > (stackbuf + SIGSTACK_SIZE - 1)) {
		printf("SYSTEM ERROR: Fail sigstack test.\nSignal handler is not delivered on the alternate stack\n");
		exit(-1);
	}

	alarmcnt = 1;
printf("return from alarmfunc\n");	
	
}	

checksigstate()
{	
	struct sigstack oss;
	int omask;
	struct itimerval tv;
	int i;

	/* first lets make sure we're on user stack */
	if ((char *)&i >= stackbuf && (char *)&i < (stackbuf + SIGSTACK_SIZE)) {
		printf("SYSTEM ERROR: Fail sigstack test.\nLongjmp out of a signal handler, but still running on the alternate stack\n");
		exit(-1);
	}
	/* lets check to make sure that signal states are still sane */
	if ((omask = sigsetmask(0)) == -1) {
		perror("sigstack: fail sigsetmask\n");
		exit(-1);
	}
	if (omask != 0) {
		printf("SYSTEM ERROR: Fail sigstack test.\nLongjmp out of a signal handler, but signal mask is not restored! Expected 0x%x, Actual 0x%x\n",0, omask);
		exit(-1);
	}
	if (sigstack((struct sigstack *)NULL, &oss) == -1) {
		perror("sigstack: fail sigstack\n");
		exit(-1);
	}
	if (oss.ss_onstack != 0) {
		printf("SYSTEM ERROR: Fail sigstack test.\nLongjmp out of a signal handler, but ss_onstack is still set!\n");
		exit(-1);
	}
}
