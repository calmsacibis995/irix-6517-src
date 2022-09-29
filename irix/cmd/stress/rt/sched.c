/*
 * turn on fast clock
 * sproc 4 processes(each has differnt priority)
 * each call blockproc on itself
 * main process is woken up every 10ms by itimer
 * main process then unblockproc all the childs
 * the child snap shot the time after the blockproc then go to sleep again
 * when the lowest priority child is woken up, it will check to make sure
 * that higher priority process is dispatched first.
 */
#include <sys/schedctl.h>
#include <sys/sysmp.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/prctl.h>
#include <stdio.h>
#include <errno.h>
#define DEBUG 1
#define OWN_SCHED 1

#define COUNT_DELAY 10000

int pri = NDPHIMAX;
int rtproc = 1;
int mproc;
int mainpid;
int pid[4];
int cnt;
struct timeval  wake[4];
extern void cproc1(), cproc2(), cproc3(), cproc4();
extern int handler();
extern int schedreport();
extern int schedsig();

main()
{
	int i = 0;
	struct itimerval newdata, olddata;
	
	mainpid = getpid();

#ifndef OWN_SCHED
	if (schedctl(NDPRI, 0, pri) == -1) {
		printf("Failed schedctl\n");
		exit(-1);
	}
#endif
	if (sysmp(MP_MUSTRUN, rtproc) == -1) {
		printf("Failed MP_MUSTRUN on %d\n",rtproc);
		exit(-1);
	}
	if (sysmp(MP_RESTRICT, rtproc) == -1) {
		printf("Failed MP_RESTRICT on %d\n",rtproc);
		exit(-1);
	}
	if(prctl(PR_SETEXITSIG,SIGKILL)==-1){
		perror("prctl");
		exit(1);
	}
	if (system("/etc/ftimer -f on") == -1) {
		perror("system");
		exit(-1);
	}
	if (sigset(SIGALRM, handler) == -1) {
		printf("Failed sigset\n");
		exit(0);
	}
	setbuf(stdout, NULL);
	if ((pid[i++] = sproc(cproc1, PR_SALL)) == -1) {
		printf("Failed cproc1\n");
		exit(-1);
	}
#ifndef OWN_SCHED
	if (schedctl(NDPRI, 0, pri+i) == -1) {
		printf("Failed schedctl\n");
		exit(-1);
	}
#endif
	if ((pid[i++] = sproc(cproc2, PR_SALL)) == -1) {
		printf("Failed cproc2\n");
		exit(-1);
	}
#ifndef OWN_SCHED
	if (schedctl(NDPRI, 0, pri+i) == -1) {
		printf("Failed schedctl\n");
		exit(-1);
	}
#endif
	if ((pid[i++] = sproc(cproc3, PR_SALL)) == -1) {
		printf("Failed cproc3\n");
		exit(-1);
	}
#ifndef OWN_SCHED
	if (schedctl(NDPRI, 0, pri+i) == -1) {
		printf("Failed schedctl\n");
		exit(-1);
	}
#endif
	if ((pid[i] = sproc(cproc4, PR_SALL)) == -1) {
		printf("Failed cproc4\n");
		exit(-1);
	}
	/* main process goes to 0 */
	if (sysmp(MP_MUSTRUN, mproc) == -1) {
		printf("Failed MP_MUSTRUN on %d\n",mproc);
		exit(-1);
	}
#ifndef OWN_SCHED
	if (schedctl(NDPRI, 0, 0) == -1) {
		printf("Failed schedctl\n");
		exit(-1);
	}
#endif
	if (sigset(SIGINT, schedreport) == -1) {
		printf("Failed sigset\n");
		exit(0);
	}
	if (sighold(SIGALRM) == -1) {
		printf("Failed sighold\n");
		exit();
	}
	newdata.it_interval.tv_sec = 0;
	newdata.it_value.tv_sec = 0;
	newdata.it_interval.tv_usec = 20000;
	newdata.it_value.tv_usec = 20000;
	if (setitimer (ITIMER_REAL,&newdata, &olddata) != 0)
	{
		printf ("setitimer errno = %d\n",errno);
	}
	printf ("setimer and signal established\n");
	if (setblockproccnt(mainpid, 0) == 1) {
		printf("Failed setblockproccnt of main process\n");
		exit();
	}
	if (sigrelse(SIGALRM) == -1) {
		printf("Failed sigrelse\n");
		exit();
	}
loop:
	cnt++;
/*
	if (cnt >= 100) {
		printf("Done\n");
		schedreport(0);
	}
*/
	for (i=0; i<4; i++)
		timerclear(&wake[i]);
/*
	if (blockproc(mainpid) == -1) {
		printf("Failed blockproc of main process\n");
		schedexit();
	}
*/
	pause();
	for (i=0; i<4; i++)
		unblockproc(pid[i]);
	/* wait til they all woke up */
	while (!timerisset(&wake[0]) ||
	       !timerisset(&wake[1]) ||
	       !timerisset(&wake[2]) ||
	       !timerisset(&wake[3]))
	{
		printf(".");
	}
	printf("\r");
	if (timercmp(&wake[0], &wake[1], >) ||
		timercmp(&wake[0], &wake[2], >) ||
		timercmp(&wake[0], &wake[3], >)) {
#ifdef DEBUG
			printf("Failed 1\n");
#endif
			schedreport();
	}
	if (timercmp(&wake[1], &wake[0], <) ||
		timercmp(&wake[1], &wake[2], >) ||
		timercmp(&wake[1], &wake[3], >)) {
#ifdef DEBUG
			printf("Failed 2\n");
#endif
			schedreport();
	}
	if (timercmp(&wake[2], &wake[0], <) ||
		timercmp(&wake[2], &wake[1], <) ||
		timercmp(&wake[2], &wake[3], >)) {
#ifdef DEBUG
			printf("Failed 3\n");
#endif
			schedreport();
	}
	if (timercmp(&wake[3], &wake[0], <) ||
		timercmp(&wake[3], &wake[1], <) ||
		timercmp(&wake[3], &wake[2], <)) {
#ifdef DEBUG
			printf("Failed 4\n");
#endif
			schedreport();
	}
	goto loop;
}

handler()
{
/*
	static int i;
	if (unblockproc(mainpid) == -1) {
		printf("Failed unblockproc of main process\n");
		schedexit();
	}
*/
}

void
cproc1()
{
	int mypid = getpid();
	int count;
/*
	printf("cproc1 started\n");
*/
#ifdef OWN_SCHED
	if (schedctl(NDPRI, 0, pri) == -1) {
		printf("Failed schedctl\n");
		exit(-1);
	}
#endif
	if (setblockproccnt(mypid, 0) == 1) {
		printf("Failed setblockproccnt of cproc1\n");
		exit();
	}
loop:
	if (blockproc(mypid) == -1) {
		printf("Failed blockproc of cproc1\n");
		exit();
	}
	gettimeofday(&wake[0], (struct timezone *)0);
	count = COUNT_DELAY;
	while (count--) {}
	goto loop;
}

void
cproc2()
{
	int mypid = getpid();
	int count;
/*
	printf("cproc2 started\n");
*/
#ifdef OWN_SCHED
	if (schedctl(NDPRI, 0, pri+1) == -1) {
		printf("Failed schedctl\n");
		exit(-1);
	}
#endif
	if (setblockproccnt(mypid, 0) == 1) {
		printf("Failed setblockproccnt of cproc2\n");
		exit();
	}
loop:
	if (blockproc(mypid) == -1) {
		printf("Failed blockproc of cproc2\n");
		exit();
	}
	gettimeofday(&wake[1], (struct timezone *)0);
	count = COUNT_DELAY;
	while (count--) {}
	goto loop;
}

void
cproc3()
{
	int mypid = getpid();
	int count;
/*
	printf("cproc3 started\n");
*/
#ifdef OWN_SCHED
	if (schedctl(NDPRI, 0, pri+2) == -1) {
		printf("Failed schedctl\n");
		exit(-1);
	}
#endif
	if (setblockproccnt(mypid, 0) == 1) {
		printf("Failed setblockproccnt of cproc3\n");
		exit();
	}
loop:
	if (blockproc(mypid) == -1) {
		printf("Failed blockproc of cproc3\n");
		exit();
	}
	gettimeofday(&wake[2], (struct timezone *)0);
	count = COUNT_DELAY;
	while (count--) {}
	goto loop;
}

void
cproc4()
{
	int mypid = getpid();
	int count;
/*
	printf("cproc4 started\n");
*/
#ifdef OWN_SCHED
	if (schedctl(NDPRI, 0, pri+3) == -1) {
		printf("Failed schedctl\n");
		exit(-1);
	}
#endif
	if (setblockproccnt(mypid, 0) == 1) {
		printf("Failed setblockproccnt of cproc4\n");
		exit();
	}
loop:
	if (blockproc(mypid) == -1) {
		printf("Failed blockproc of cproc4\n");
		exit();
	}
	gettimeofday(&wake[3], (struct timezone *)0);
	count = COUNT_DELAY;
	while (count--) {}
	goto loop;
}

schedsig()
{
	printf("Got SIGINT!!\n");
}

schedreport()
{
	int i;

	printf("Scheduling report: Loop %d\n",cnt);
	printf("cproc1: pri=%d, tv_sec=%d, tv_usec=%d\n",pri+1,wake[0].tv_sec,wake[0].tv_usec);
	printf("cproc2: pri=%d, tv_sec=%d, tv_usec=%d\n",pri+2,wake[1].tv_sec,wake[1].tv_usec);
	printf("cproc3: pri=%d, tv_sec=%d, tv_usec=%d\n",pri+3,wake[2].tv_sec,wake[2].tv_usec);
	printf("cproc4: pri=%d, tv_sec=%d, tv_usec=%d\n",pri+4,wake[3].tv_sec,wake[3].tv_usec);
	sysmp(MP_EMPOWER, rtproc);
	sleep(1);
	exit(0);
}
