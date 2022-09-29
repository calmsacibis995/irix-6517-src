#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>

#if 0
#define OUR_CLOCK CLOCK_REALTIME
#else
#define OUR_CLOCK CLOCK_SGI_FAST
#endif

#define TIMER_CNT 5
char *progname;
struct timespec now, before;

/*ARGSUSED*/
void real_timer_intr(int sig, siginfo_t *info, void *cruft)
{
	int noverflow;
	timer_t timer = *(timer_t *)info->si_value.sival_ptr;
	if(noverflow = timer_getoverrun(timer)) {
		printf("real_timer_intr: timer overran %d times\n",noverflow);
	}
	if(clock_gettime(CLOCK_REALTIME, &now) < 0 ) {
		perror("ptimer: ERROR - clock_gettime(CLOCK_REALTIME) failed");
		exit(1);
	}
	before.tv_sec = now.tv_sec - before.tv_sec;
	if (now.tv_nsec >= before.tv_nsec) {
		before.tv_nsec = now.tv_nsec - before.tv_nsec;
	} else {
		if (before.tv_sec > 0)
			before.tv_sec--;
		before.tv_nsec = (now.tv_nsec - before.tv_nsec) + 1000000000;
	}

	printf("%-18s %-19s %2d sec %12d nsec\n","real_timer_intr:",
				"elapsed time", before.tv_sec, before.tv_nsec);
	before = now;
	/*
	fflush(stdout);
	*/

	return;
}

/*ARGSUSED*/
void fast_timer_intr(int sig, siginfo_t *info, void *cruft)
{
	int noverflow;
	timer_t timer = *(timer_t *)info->si_value.sival_ptr;
	if(noverflow = timer_getoverrun(timer)) {
		printf("fast_timer_intr: timer overran %d times\n",noverflow);
	}
	/*
	 * use CLOCK_SGI_CYCLE because CLOCK_SGI_FAST cannot be used with
	 * clock_gettime
	 */
	if(clock_gettime(CLOCK_SGI_CYCLE, &now) < 0 ) {
		perror("ptimer: ERROR - clock_gettime(CLOCK_SGI_CYCLE) failed");
		exit(1);
	}
	before.tv_sec = now.tv_sec - before.tv_sec;
	if (now.tv_nsec >= before.tv_nsec) {
		before.tv_nsec = now.tv_nsec - before.tv_nsec;
	} else {
		if (before.tv_sec > 0)
			before.tv_sec--;
		before.tv_nsec = (now.tv_nsec - before.tv_nsec) + 1000000000;
	}

	printf("%-18s %-19s %2d sec %12d nsec\n","fast_timer_intr:",
				"elapsed time", before.tv_sec, before.tv_nsec);
	before = now;
	/*
	fflush(stdout);
	*/

	return;
}

/*ARGSUSED*/
void abs_timer_intr(int sig, siginfo_t *info, void *cruft)
{

	if(clock_gettime(CLOCK_REALTIME, &now) < 0 ) {
		perror("ptimer: ERROR - clock_gettime(CLOCK_SGI_CYCLE) failed");
		exit(1);
	}
	printf("%-14s %-15s %-8s %-3s %10d sec %12d nsec\n",
			"CLOCK_REALTIME", "absolute timer", "received","at",
				now.tv_sec, now.tv_nsec);
	return;
}

timer_t mytimer;

main(int argc, char **argv)
{
	int cnt;
	struct itimerspec i;
	struct timespec res,now;
	struct sigaction sa;
	sigset_t allsigs;
	struct sigevent timer_event;

	progname = argv[0];

	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = real_timer_intr;

	if (sigaction(SIGRTMIN, &sa, NULL) < 0) {
		perror("ptimer: ERROR - sigaction");
		exit(1);
	}

	/*
	 * Clock functions - clock_getres, clock_gettime
	 */
	if(clock_getres(CLOCK_REALTIME, &res)<0) {
		perror("ptimer: ERROR - clock_getres(CLOCK_REALTIME) failed");
		exit(1);
	}
	printf("%-15s %-12s\t%d sec %d nsec\n","CLOCK_REALTIME",
				"resolution", res.tv_sec, res.tv_nsec);
	if(clock_gettime(CLOCK_REALTIME, &res) < 0 ) {
		perror("ptimer: ERROR - clock_gettime(CLOCK_REALTIME) failed");
		exit(1);
	}
	printf("%-15s %-12s\t%d sec %d nsec\n"," ", "value", res.tv_sec,
							res.tv_nsec);

	if(clock_getres(CLOCK_SGI_CYCLE, &res)<0) {
		perror("ptimer: ERROR - clock_getres(CLOCK_SGI_CYCLE) failed");
		exit(1);
	}
	printf("%-15s %-12s\t%d sec %d nsec\n","CLOCK_SGI_CYCLE",
				"resolution", res.tv_sec, res.tv_nsec);
	if(clock_gettime(CLOCK_SGI_CYCLE, &res) < 0 ) {
		perror("ptimer: ERROR - clock_gettime(CLOCK_SGI_CYCLE) failed");
		exit(1);
	}
	printf("%-15s %-12s\t%d sec %d nsec\n"," ", "value", res.tv_sec,
						res.tv_nsec);

	if(clock_getres(CLOCK_SGI_FAST, &res)<0) {
		perror("ptimer: ERROR - clock_getres(CLOCK_SGI_FAST) failed");
		exit(1);
	}
	printf("%-15s %-12s\t%d sec %d nsec\n\n","CLOCK_SGI_FAST",
				"resolution", res.tv_sec, res.tv_nsec);


	/*
	 * timer funtions
	 *
	 * timer with CLOCK_REALTIME
	 *
	 * absolute timer
	 */
	timer_event.sigev_notify = SIGEV_SIGNAL;
	timer_event.sigev_signo = SIGRTMIN;
	timer_event.sigev_value.sival_ptr = (void *) &mytimer;
	if (timer_create(CLOCK_REALTIME, &timer_event, &mytimer)< 0){
		perror("ptimer: ERROR - timer_create");
		exit(1);
	}
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = abs_timer_intr;

	if (sigaction(SIGRTMIN, &sa, NULL) < 0) {
		perror("ptimer: ERROR - sigaction");
		exit(1);
	}
	if(clock_gettime(CLOCK_REALTIME, &now) < 0 ) {
		perror("ptimer: ERROR - clock_gettime(CLOCK_REALTIME) failed");
		exit(1);
	}
	i.it_value = now;
	i.it_value.tv_sec += 1;
	i.it_interval.tv_sec = 0;
	i.it_interval.tv_nsec = 0;
	printf("\n%-14s %-15s %-8s %-3s %10d sec %12d nsec\n",
			"CLOCK_REALTIME", "value", "now"," ",
				now.tv_sec, now.tv_nsec);
	printf("%-14s %-15s %-8s %-3s %10d sec %12d nsec\n",
			"CLOCK_REALTIME", "absolute timer", "expected","at",
				i.it_value.tv_sec, i.it_value.tv_nsec);
	if(timer_settime(mytimer, TIMER_ABSTIME, &i, NULL) < 0) {
		perror("ptimer: ERROR - timer_settime");
		exit(1);
	}
	/*
	 * wait for timer intr
	 */
	sigemptyset(&allsigs);
	sigsuspend(&allsigs);
	if (timer_delete(mytimer) < 0) {
		perror("ptimer: ERROR - timer_delete");
		exit(1);
	}
	

	i.it_value.tv_sec = 2;
	i.it_value.tv_nsec = 0;
	i.it_interval.tv_sec = 1;
	i.it_interval.tv_nsec = 800000;

	timer_event.sigev_notify = SIGEV_SIGNAL;
	timer_event.sigev_signo = SIGRTMIN;
	timer_event.sigev_value.sival_ptr = (void *) &mytimer;

	if (timer_create(CLOCK_REALTIME, &timer_event, &mytimer)< 0){
		perror("ptimer: ERROR - timer_create");
		exit(1);
	}
	printf("\n%-15s %-5s %-5s %-10s %2d sec %12d nsec\n",
			"CLOCK_REALTIME", "timer", "start", "value",
					i.it_value.tv_sec, i.it_value.tv_nsec);
	printf("%-15s %-5s %-5s %-10s %2d sec %12d nsec\n",
			" ", " ", " ", "interval",
				i.it_interval.tv_sec, i.it_interval.tv_nsec);
	if(timer_settime(mytimer, 0, &i, NULL) < 0) {
		perror("ptimer: ERROR - timer_settime");
		exit(1);
	}
	if(clock_gettime(CLOCK_REALTIME, &before) < 0 ) {
		perror("ptimer: ERROR - clock_gettime(CLOCK_REALTIME) failed");
		exit(1);
	}

	sigemptyset(&allsigs);
	for (cnt = 0; cnt < TIMER_CNT; cnt++) {
		sigsuspend(&allsigs);
	}
	sigfillset(&allsigs);
	if (sigprocmask(SIG_BLOCK, &allsigs, NULL) < 0) {
		perror("ptimer: ERROR - sigprocmask");
		exit(1);
	}
	if(timer_gettime(mytimer, &i) < 0) {
		perror("ptimer: ERROR - timer_gettime");
		exit(1);
	}
	/*
	 * Stop the timer
	 */
	i.it_value.tv_sec = 0;
	i.it_value.tv_nsec = 0;
	if(timer_settime(mytimer, 0, &i, NULL) < 0) {
		perror("ptimer: ERROR - timer_settime");
		exit(1);
	}
	printf("%-14s %-5s %-7s %-9s %2d sec %12d nsec\n",
			"CLOCK_REALTIME", "timer", "gettime","value",
				i.it_value.tv_sec, i.it_value.tv_nsec);
	printf("%-14s %-5s %-7s %-9s %2d sec %12d nsec\n",
			" ", " ", " ","interval",
				i.it_interval.tv_sec, i.it_interval.tv_nsec);
	if (timer_delete(mytimer) < 0) {
		perror("ptimer: ERROR - timer_delete");
		exit(1);
	}
	/*
	 * if privileged user
	 * run timer with CLOCK_SGI_FAST
	 */
	if (geteuid() != 0) {
		return(0);
	}
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = fast_timer_intr;

	if (sigaction(SIGRTMIN, &sa, NULL) < 0) {
		perror("ptimer: ERROR - sigaction");
		exit(1);
	}

	i.it_value.tv_sec = 1;
	i.it_value.tv_nsec = 0;
	i.it_interval.tv_sec = 0;
	i.it_interval.tv_nsec = 10000000;

	timer_event.sigev_notify = SIGEV_SIGNAL;
	timer_event.sigev_signo = SIGRTMIN;
	timer_event.sigev_value.sival_ptr = (void *) &mytimer;

	if (timer_create(CLOCK_SGI_FAST, &timer_event, &mytimer)< 0){
		perror("ptimer: ERROR - timer_create");
		exit(1);
	}
	printf("\n%-15s %-5s %-5s %-10s %2d sec %12d nsec\n",
			"CLOCK_SGI_FAST", "timer", "start", "value",
					i.it_value.tv_sec, i.it_value.tv_nsec);
	printf("%-15s %-5s %-5s %-10s %2d sec %12d nsec\n",
			" ", " ", " ", "interval",
				i.it_interval.tv_sec, i.it_interval.tv_nsec);
	if(timer_settime(mytimer, 0, &i, NULL) < 0) {
		perror("ptimer: ERROR - timer_settime");
		exit(1);
	}

	if(clock_gettime(CLOCK_SGI_CYCLE, &before) < 0 ) {
		perror("ptimer: ERROR - clock_gettime(CLOCK_SGI_CYCLE) failed");
		exit(1);
	}

	sigemptyset(&allsigs);
	for (cnt = 0; cnt < TIMER_CNT; cnt++) {
		sigsuspend(&allsigs);
	}
	sigfillset(&allsigs);
	if (sigprocmask(SIG_BLOCK, &allsigs, NULL) < 0) {
		perror("ptimer: ERROR - sigprocmask");
		exit(1);
	}
	if(timer_gettime(mytimer, &i) < 0) {
		perror("ptimer: ERROR - timer_gettime");
		exit(1);
	}
	/*
	 * Stop the timer
	 */
	i.it_value.tv_sec = 0;
	if(timer_settime(mytimer, 0, &i, NULL) < 0) {
		perror("ptimer: ERROR - timer_settime");
		exit(1);
	}
	printf("%-14s %-5s %-7s %-9s %2d sec %12d nsec\n",
			"CLOCK_SGI_FAST", "timer", "gettime","value",
				i.it_value.tv_sec, i.it_value.tv_nsec);
	printf("%-14s %-5s %-7s %-9s %2d sec %12d nsec\n",
			" ", " ", " ","interval",
				i.it_interval.tv_sec, i.it_interval.tv_nsec);
	if (timer_delete(mytimer) < 0) {
		perror("ptimer: ERROR - timer_delete");
		exit(1);
	}

	/*
	 * clock_settime
	 */
	if(clock_gettime(CLOCK_REALTIME, &now) < 0 ) {
		perror("ptimer: ERROR - clock_gettime(CLOCK_REALTIME) failed");
		exit(1);
	}
	printf("\n%-14s %-5s %-7s %-9s %2d sec %12d nsec\n",
			"CLOCK_REALTIME", "timer", "settime","value",
				now.tv_sec, now.tv_nsec);
	if(clock_settime(CLOCK_REALTIME, &now) < 0 ) {
		perror("ptimer: ERROR - clock_settime(CLOCK_REALTIME) failed");
		exit(1);
	}


	fflush(stdout);
	return(0);
}
