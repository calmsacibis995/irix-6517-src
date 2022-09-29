/*
 * pollrestart.c
 * check out sigwaitinfo, sigtimedwait(), sigqueue() and restartable system call
 * 1) The test calls sigtimedwait() with timeout 0 mask = 0 and expect
 * an EAGAIN.
 * 2) Repeatedly calls sigtimedwait with 0 timeout and mask = signal
 * Have another process sends it a matching signal via sigqueue. sigtimedwait 
 * should return with the right siginfo
 * and the signal handler should not be invoked.
 * 3) sigtimedwait() again with timeout and mask = 0 and expect EAGAIN
 * within that timeout period. DOuble check by using gettimeofday()
 * 4) sigtimedwait() again with timeout of 5 sec and mask = signal 
 * set an alarm at 1 sec, with SA_RESTART on for SIGALRM
 * at 3 sec mark, have another process sends it a matching signal.
 * sigtimedwait() should return with the right info.
 * 5) call sigwaitinfo() with mask = 0, set itimer to break out of the wait.
 */
#include <sys/types.h>
#include <signal.h>
#include <getopt.h>
#include <sys/siginfo.h>
#include <sys/prctl.h>
#include <sys/ucontext.h>
#include <sys/time.h>
#include <timers.h>
#include <sys/time.h>
#include <errno.h>
#include <stdio.h>
#include "stress.h"

#define _POSIX_4SOURCE	1
#define POLLRESTART	0xdeadbeef
int target_sig = SIGRTMAX-1;
int test2sig;
int test4alarm;
int test4sig;
pid_t cpid;
pid_t ppid;
int nloops = 100;
int verbose;
void test1(void), test2(void), test4(void), test5(void);
void test3(time_t, long);

main(int argc, char **argv)
{
	int c, i;
	long nsec;

	errinit(argv[0]);
	setbuf(stdout, NULL);
	sighold(target_sig);

	while ((c = getopt(argc, argv, "vl:")) != EOF) {
	  	switch (c) {
		case 'l':
			nloops = atoi(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			exit(1);
		}
	}

	test1();
	test2();
	printf("test3\n");
	test3(1, 20000000);
	for (i = 0; i < nloops; i++) {
		nsec = rand() * 1000; /* 0-32K usecs */
		test3(0, nsec);
	}
	test4();
	test5();
}

void
test1(void)
{
	sigset_t set;
	struct timespec ts;

	printf("test1\n");		
	ts.tv_sec = 0;
	ts.tv_nsec = 0;
	sigemptyset(&set);
	if (sigtimedwait(&set, NULL, &ts) == -1) {
		if (errno != EAGAIN) {
			errprintf(1, "test1");
		}
	} else {
		errprintf(0,"pollrestart: succeeded sigtimedwait with zeroed sigmask\n");
	}
}

void
test2handlr(sig)
{
	test2sig++;
	kill(cpid, SIGKILL);
	errprintf(0,"test2: Handler for signal %d was called during sigtimedwait\n",sig);
}

void
test2alrm()
{
	return;
}

/*
 * there's a race in this code, because parent can be context switched out
 * while chile sends a signal causing parent sig handler to run before sigpoll
 * is called
 */
void
test2(void)
{
	sigset_t set;
	int rv;
	struct sigaction act;
	struct siginfo info;
	struct timespec ts;
	extern void test2handlr();
	extern void test2alrm();
		
	printf("test2\n");		
	ppid = getpid();
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	act.sa_handler = test2handlr;
	if (sigaction(target_sig, &act, (struct sigaction *)0)) {
		errprintf(1,"test2");
	}
	sigemptyset(&set);
	sigaddset(&set, target_sig);
	ts.tv_sec = 0;
	ts.tv_nsec = 0;

	if (cpid = fork()) {
		/* parent */
		for (;;) {
			rv = sigtimedwait(&set, &info, &ts);
			if (rv == -1) {
				if (errno != EAGAIN) {
					errprintf(1,"test2");
				}
			}
			else {
				if (info.si_value.sival_int != POLLRESTART || 
					info.si_code != SI_QUEUE ||
					info.si_signo != target_sig) {
					errprintf(1, "Wrong return status from sigpoll\n");	
					errprintf(0,"Polled signal %d , value = %d code = %d. Expected signal %d, value %d code %d\n",info.si_signo, info.si_value.sival_int, target_sig, POLLRESTART, SI_QUEUE);
				}
				else {
					kill(cpid, SIGKILL);
					break;	/* worked */
				}
			}
		}
	}
	else { /* child */
		union sigval si;

		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		act.sa_handler = test2alrm;
		if (sigaction(SIGALRM, &act, (struct sigaction *)0)) {
			errprintf(1,"test2");
		}
		alarm(1);
		pause();
		si.sival_int = POLLRESTART;
		if (sigqueue(ppid, target_sig, si) == -1) {
			errprintf(1,"test2");
		}
		exit(0);
	}
}

void
test3(time_t sec, long nsec)
{
	sigset_t set;
	struct timespec ts;
	int rv;
	struct sigaction act;
	struct timeval oldtv, newtv;
	int usec1, usec2, retusec;
	
	if (verbose)
		printf("test3a\n");		
	ts.tv_sec = sec;
	ts.tv_nsec = nsec;
	sigemptyset(&set);
	gettimeofday(&oldtv, 0);
	rv = sigtimedwait(&set, NULL, &ts); 
	gettimeofday(&newtv, 0);
	/* expect rv = -1 and error = EGAIN */
	if (rv == -1) {
		if (errno != EAGAIN) {
			errprintf(1,"test3");
		}
	} else {
		errprintf(0,"pollrestart: succeeded sigpoll with zeroed sigmask\n");
	}
	usec1 = newtv.tv_sec * 1000000 + newtv.tv_usec;
	usec2 = oldtv.tv_sec * 1000000 + oldtv.tv_usec;
	retusec = ts.tv_sec*1000000 + ts.tv_nsec/1000;
	if ((usec1-usec2) < retusec) 
		errprintf(2, "test3: sigpoll returns sooner than expected: actual %d expected %d\n",
			usec1 - usec2, retusec);
	if (verbose)
		fprintf(stdout, "pollrestart:Actual duration is %d usec. Expected %d \n", usec1-usec2, retusec);

	if (verbose)
		printf("test3b\n");		
	ts.tv_sec = sec;			/* 1 sec */
	ts.tv_nsec = nsec;		/* 10ms */
	sigemptyset(&set);
	gettimeofday(&oldtv, 0);
	rv = sigtimedwait(&set, NULL, &ts); 
	gettimeofday(&newtv, 0);
	/* expect rv = -1 and error = EGAIN */
	if (rv == -1) {
		if (errno != EAGAIN) {
			errprintf(1,"test3");
		}
	} else {
		errprintf(0,"pollrestart: succeeded sigpoll with zeroed sigmask\n");
	}
	usec1 = newtv.tv_sec * 1000000 + newtv.tv_usec;
	usec2 = oldtv.tv_sec * 1000000 + oldtv.tv_usec;
	if ((usec1-usec2) < (ts.tv_sec*1000000 + ts.tv_nsec/1000)) 
		errprintf(2, "test3: sigpoll returns sooner than expected: actual %d expected %d\n",
			usec1 - usec2, retusec);
	if (verbose)
		fprintf(stdout, "Actual duration is %d usec. Expected %d \n",usec1-usec2, (ts.tv_sec*1000000 + ts.tv_nsec/1000));
}

/*
 * indicate that the alarm has occured
 * send self target_sig to get out of sigpoll
 */		
void
test4alrm()
{
	test4alarm++;
}

void
test4handlr(int sig)
{
	test4sig++;
	errprintf(0,"test4: Handler for signal %d was called during sigpoll\n",sig);
}
	
						
void
test4(void)
{	
	sigset_t set;
	int rv;
	struct timespec ts;
	struct sigaction act;
	int error = 0;
	siginfo_t info;

	printf("test4\n");		
	ppid = getpid();
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_SIGINFO;
	act.sa_handler = test4handlr;
	if (sigaction(target_sig, &act, (struct sigaction *)0)) {
		errprintf(1,"test4");
	}
	act.sa_flags = SA_RESTART;
	act.sa_handler = test4alrm;
	if (sigaction(SIGALRM, &act, (struct sigaction *)0)) {
		errprintf(1,"test4");
	}
	cpid = fork();
	if (cpid == 0) { /* child */
		union sigval si;

		sigemptyset(&act.sa_mask);
		act.sa_flags = 0;
		act.sa_handler = test4alrm;
		if (sigaction(SIGALRM, &act, (struct sigaction *)0)) {
			errprintf(1,"test4");
		}
		alarm(3);
		pause();
		si.sival_int = POLLRESTART;
		if (sigqueue(ppid, target_sig, si) == -1) {
			errprintf(1,"test4");
		}
		exit(0);
	}
	ts.tv_sec = 5;			/* 1 sec */
	ts.tv_nsec = 10000000;		/* 10ms */
	/* parent */
	sigemptyset(&set);
	sigaddset(&set, target_sig);
	alarm(1);
	rv = sigtimedwait(&set, &info, &ts); 
	if (rv == -1) {
		if (errno == EAGAIN) {
			errprintf(1,"No signal %d polled\n",target_sig);
		}
		else {
			errprintf(1,"test4 sigpoll return -1");
		}
	} else {
		if (info.si_value.sival_int != POLLRESTART || 
			info.si_signo != target_sig ||
			info.si_code != SI_QUEUE) {
			errprintf(2, "Wrong return status from sigpoll\n");
			errprintf(0,"Polled signal %d , value = %d code = %d. Expected signal %d, value %d code %d\n",info.si_signo, info.si_value.sival_int, target_sig, POLLRESTART, SI_QUEUE);
		}
		if (test4alarm == 0) {
			errprintf(0,"Test4 alarm handler wasn't called\n");
		}
		/* worked */
	}
}

void
test5alarm()
{
	exit(0);
}

void
test5(void)
{
	sigset_t set;
	int rv;
	struct sigaction act;
	siginfo_t info;
	
	printf("test5\n");		
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = test5alarm;
	if (sigaction(SIGALRM, &act, (struct sigaction *)0)) {
		errprintf(1,"test5");
	}
	alarm(1);
	rv = sigwaitinfo(&set, &info); 
	if (rv == -1)
		errprintf(1,"test5 sigwaitinfo() unexpectedly return -1");
}
