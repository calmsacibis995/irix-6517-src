#include <sys/time.h>
#include <sys/param.h>
#include <sys/syssgi.h>
#include <sys/signal.h>
#include <stdio.h>

#define USEC_PER_SEC 1000000
int slpsec;
int count = -1;
int pid, cpid;
int maxadjsec = 1000;	/* max adj time in sec */
int adjinterval = 10000;/* adjtime in usec */
int adjdir;		/* reverse direction in adjtime */
int triminterval = 1000;	/* trim in nsec */
int maxtimetrim = (USEC_PER_SEC/HZ/10*3)*1000;	/* max timetrim allowed */
int trimdir;
int timetrim;
struct timeval adjtv;
int slpsec;
int maxsec = 10;
int verbose = 1;
int adjust;	/* do adjtime */
extern void adjtime_func();
extern void main_alarm();

void
main_alarm()
{
	struct timeval adjtv;

	if (cpid)
		kill(cpid, SIGKILL);
	adjtv.tv_sec = adjtv.tv_usec = 0;
	if (adjtime(&adjtv, 0) == -1) 
		perror("adjtime failed");
	if (syssgi(SGI_SETTIMETRIM, 0) == -1) 
		perror("settimetrim failed");
	exit(-1);
}

main(int argc, char **argv)
{
	struct timeval tv, lastv;
   	extern int		optind;
   	extern char		*optarg;
	int			c;
	int 			err = 0;
	struct itimerval itv;
	struct timeval *tv1p, *tv2p;

	tv2p = &tv;
	tv1p = &lastv;

	/*
	 * Parse arguments.
	 */
	setbuf(stdout, NULL);
	while ((c = getopt(argc, argv, "Ava:t:s:m:")) != EOF) {
	  	switch (c) {
	  	case 'a':
	       		if ((adjinterval = strtol(optarg, (char **)0, 0)) <0)
		   		err++;
			break;
	  	case 't':
	       		if ((triminterval = strtol(optarg, (char **)0, 0)) <0)
		   		err++;
			break;
	  	case 's':
	       		if ((slpsec = strtol(optarg, (char **)0, 0)) <0)
		   		err++;
			break;
	  	case 'm':
	       		if ((maxsec = strtol(optarg, (char **)0, 0)) <0)
		   		err++;
			break;
		case 'A':
			adjust++;
			break;
		case 'v':
			verbose++;
			break;
		default:
			break;
		}
	}
	if (err) {
		fprintf(stderr,"adjtime [-a adjust interval] [-t time trim interval] [-s sleep between gettimeofday] [-m test duration in sec ]\n");
		exit(-1);
	}

	/* 
	 * test to see if time goes backward
 	 * with various values of adjtime and timetrim
	 */
	pid = getpid();
	if (adjust) {
		cpid = fork();
		if (cpid == -1) {
			fprintf(stderr,"Can't fork\n");
			exit(-1);
		}
		if (cpid == 0) {
			adjtime_proc();
			exit();
		}
	}

	sighold(SIGALRM);
	sigset(SIGALRM, main_alarm);
	itv.it_interval.tv_sec = 0;	
	itv.it_interval.tv_usec = 0;	
	itv.it_value.tv_sec = maxsec;
	itv.it_value.tv_usec = 0;
	setitimer(ITIMER_REAL, &itv, 0);
	sigrelse(SIGALRM);

	gettimeofday(tv1p, 0);
	while (1) {
		gettimeofday(tv2p, 0);
		if (timercmp(tv2p, tv1p, <)) {
			fprintf(stderr,"Time goes backward\n");
			fprintf(stderr,"Lastv.tv_sec = %d, lastv.tv_usec = %d\n",lastv.tv_sec, lastv.tv_usec);
			fprintf(stderr,"tv.tv_sec = %d, tv.tv_usec = %d\n",tv.tv_sec, tv.tv_usec);
			fprintf(stderr,"adjtv.tv_sec = %d, adjtv.tv_usec = %d\n",adjtv.tv_sec, adjtv.tv_usec);
			fprintf(stderr,"Diff usec = %d\n",(tv.tv_sec*USEC_PER_SEC+tv.tv_usec-lastv.tv_sec*USEC_PER_SEC-lastv.tv_usec));
			fprintf(stderr,"timetrim = %d\n",timetrim);
		}
		*tv1p = *tv2p;
		if (slpsec)
			sleep(slpsec);
	}
}

void
adjtime_func()
{
	if (getpid() == pid)
		printf("Parent instead of child\n");
	adjtv.tv_usec += adjinterval;
	adjtv.tv_sec += adjtv.tv_usec/USEC_PER_SEC;
	adjtv.tv_usec = adjtv.tv_usec%USEC_PER_SEC;
	if (adjtv.tv_sec >= maxadjsec || adjtv.tv_sec <= -maxadjsec) {
		if (verbose) {
			fprintf(stdout,"adjtime changes direction\n");
			fprintf(stdout,"adjtv.tv_sec = %d, adjtv.tv_usec = %d\n",adjtv.tv_sec, adjtv.tv_usec);
		}
		if (adjinterval > 0)
			adjtv.tv_sec--;
		else
			adjtv.tv_sec++;
		adjinterval *= -1;
		adjdir++;
	}
	if (adjtime(&adjtv, 0) == -1) {
		perror("adjtime failed");
		kill(pid, SIGKILL);
		exit(-1);
	}
	timetrim += triminterval;
	if (timetrim >= maxtimetrim || timetrim <= -maxtimetrim) {
		if (verbose) {
			fprintf(stdout,"timetrim changes direction\n");
			fprintf(stdout,"timetrim = %d\n",timetrim);
		}
		triminterval *= -1;
		trimdir++;
	}
	if (syssgi(SGI_SETTIMETRIM, timetrim) == -1) {
		perror("settimetrim failed");
		kill(pid, SIGKILL);
		exit(-1);
	}
}
	
	
adjtime_proc()
{
	struct itimerval itv;

	sighold(SIGALRM);
	sigset(SIGALRM, adjtime_func);
	itv.it_interval.tv_sec = 0;	
	itv.it_interval.tv_usec = 10000;	
	itv.it_value = itv.it_interval;
	setitimer(ITIMER_REAL, &itv, 0);
	sigrelse(SIGALRM);

	while(1)
		pause();
}
