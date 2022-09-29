/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.12 $"

/* RealTime Test Program */

/* Setup up application program with best case scheduling and test realtime */
/* performance. This is accomplished by locking both instruction and data   */
/* pages in memory and optionally boosting the priority to the highest      */
/* user priority. The internal work is simulated with a simple counter.     */
/* Chris Wagner & Jim Winget: May 7, 1989 */

#include "stdio.h"
#include "signal.h"
#include "getopt.h"
#include "unistd.h"
#include "stdlib.h"
#include "sys/lock.h"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/times.h"
#include "sys/time.h"
#include "sched.h"

#define PRIORITY 199

ulong		minacc;				/* uSecs of accuracy */
ulong		iter;				/* current work interval */
ulong		total_iters;	/* running total of elapsed seconds (approx) */
struct tms 	stime0,  stime1,  etime;	/* time structures */
long		stimes0, stimes, etimes;
struct timeval	stval, etval;
ulong 		interval	= 10000;	/* itimer interval (in uS) */
int		total_rep		= 0;	/* Total number of reports */
int		failed_rep		= 0;	/* Number of failed reports */
double		total_req		= 0.0;	/* Total us requested */
double		total_diff		= 0.0;	/* Total us of diff */
struct itimerval	rt, rtc;

void mwork(int);
double	work(int);
double	compute_speed(void);
double	compute_error(int);
long timesub(struct timeval *s, struct timeval *e);
ulong findmintime(void);
double second_ (void);

void quitit(), dumpit(void), alcatch(), summary(void);
void Usage(void);

int
main(int argc, char **argv)
{
	int c;
	int nwork	= 1000;		/* n us of work */
	int nospecpri	= 0;		/* up priority */
	int nospecpin	= 0;		/* pin pages */
	int report	= 10;		/* report every 10 seconds */
	int limit	= 5;		/* stop after 5 min */
	int limit_iter;
	int report_iter;

	while ((c = getopt(argc, argv, "r:l:i:w:pP")) != EOF)
	switch (c) {
	case 'r':
		report = atoi(optarg);
		break;
	case 'l':
		limit = atoi(optarg);
		break;
	case 'i':
		interval = atoi(optarg) * 1000;
		break;
	case 'w':
		nwork = atoi(optarg) * 1000;
		break;
	case 'p':
		nospecpri++;
		break;
	case 'P':
		nospecpin++;
		break;
	default:
		Usage();
		/* NOTREACHED */
	}
	printf( "\n" );
	printf( "Real Time Application Test Program\n" );
	printf( "----------------------------------\n" );

	/* print what we're going to do */
	minacc = findmintime();

	/* change interval to a multiple of minacc */
	if (interval % minacc)
		interval = ((interval + minacc - 1) / minacc) * minacc;

	if (nwork >= interval) {
		printf("Work quantum (%d) >= interval (%d)? Crazy!\n",
			nwork, interval);
		exit(-1);
	}
	if (nwork >= 1000)
		printf( "	Work %.2f mS out of every %.4f mS interval\n",
			(double)nwork/1000.0, (double)interval/1000.0 );
	else
		printf( "	Work %.2f uS out of every %.4f uS interval\n",
			(double)nwork, (double)interval );


	if (minacc >= 1000)
		printf("	Overall timing accuracy limited to %.4f mS\n",
			(double)minacc/1000.0);
	else
		printf("	Overall timing accuracy limited to %.4f uS\n",
			(double)minacc);

	if ( interval < minacc )
		printf(
	"	(may incorrectly report 0 times due to small interval)\n" );

	printf( "	Report every %d seconds Stop after %d minute(s)\n",
		report, limit);

	/* lock it all down */
	if (!nospecpin) {
		printf( "	Locking down all application pages\n" );
		if (plock(PROCLOCK) != 0)
			perror("*** Could not lock down pages - continuing");
	} else
		printf( "	Using standard unlocked pages\n" );

	/* give me good priority */
	if (!nospecpri) {
		struct sched_param param;
		param.sched_priority = PRIORITY;
		printf( "	Setting realtime priority\n");
		if (sched_setscheduler(0, SCHED_FIFO, &param) == -1)
			perror("*** Could not give good priority - continuing");
	} else
		printf( "	Using standard priority\n" );
	fflush(stdout);
	fflush(stderr);

	/* Setup signal catcher for exit and itimer */
	sigset( SIGALRM, alcatch);
	sigset( SIGINT,  quitit);
	sigset( SIGQUIT, dumpit);

	/* convert report to iteration count */
	report_iter = report*1000000.0/((double)interval);
	/* convert limit from seconds to iteration count */
	limit_iter = 60*limit*1000000.0/((double)interval);

	/* measure work quantum */
	mwork(nwork);

	/* setup real-itimer for interval */
	rtc.it_value.tv_sec = 0;
	rtc.it_value.tv_usec = 0;
	rtc.it_interval.tv_sec = 0;
	rtc.it_interval.tv_usec = 0;
	rt.it_value.tv_sec = 0;
	rt.it_value.tv_usec = (long)interval;
	rt.it_interval.tv_sec = 0;
	rt.it_interval.tv_usec = (long)interval;

	/*
	 * Since we restart the clock each reporting interval
	 * we would except that the 1st iteration will occur in 1/2
	 * the set interval time
	 */
	if (setitimer(ITIMER_REAL, &rt, NULL) < 0) {
		perror("*** Could not set real itimer");
		exit(2);
	}

	/* now do for real */
	stimes0 = times(&stime0);
	stimes  = times(&stime1);
	gettimeofday(&stval);

	while (total_iters < limit_iter) {
		work(nwork);
		pause();
		if (iter >= report_iter)
			dumpit();
	}
	summary();
	return 0;
}

void
summary(void)
{
	printf( "Summary:\n" );
	if (failed_rep > 0)
		printf("ERROR ");
	printf( "Failed %d reporting intervals out of %d (%.2f%%)\n",
		failed_rep, total_rep, (100.0*failed_rep)/total_rep );
	printf( "Lost a total of %.2f ms out of %.2f ms (%.2f%%)\n",
		total_diff/1000.0,
		total_req/1000.0,
		100.0*total_diff/total_req );
}

void
quitit()				/* Catch INT */
{
	dumpit();
	summary();
	exit(0);
}

void
alcatch()				/* Catch alarm */
{
	iter++;
}

void
dumpit(void)				/* Catch quit */
{
	double	diff, elr0, elr, elu, els, act, req;
	char	flag;

	total_rep++;
	gettimeofday(&etval);
	etimes = times(&etime);
	if (setitimer(ITIMER_REAL, &rtc, NULL) < 0) {
		perror("*** Could not clear real itimer");
		exit(2);
	}
	/* compute times in seconds */
	elr0 = (double)(etimes - stimes0) / (double)HZ;	/* from start of task*/
	elr  = (double)(etimes - stimes ) / (double)HZ;	/* from last dump */
	elu  = (double)(etime.tms_utime - stime1.tms_utime) / (double)HZ;
	els  = (double)(etime.tms_stime - stime1.tms_stime) / (double)HZ;

	req  = (double)iter * (double)interval;	/* requested us of time */
	act  = (double)timesub(&stval, &etval);
		
	/*act  = elr*1000000.0;*/		/* actual elapsed us of time */
	diff = act - req;			/* actual - requested time */
	printf(" requested %.2f uS actual %.2f uS diff %.2f uS\n", req, act, diff);

	total_req  += req;			/* Keep totals */
	total_diff += diff;			/* Allow +- cancelation */
	
	/* print a * if difference > than an interval */
	flag = (diff >= (double)interval || diff <= -(double)interval) ? '*' : ' ';
	if ( flag != ' ' )
		failed_rep++;

	printf(
"%cElapsed %.2f  Delta %.2f  User %.2f  System %.2f  Iter %d  %%lost %5.2f\n",
		flag, elr0, elr, elu, els, iter, 100.0*diff/req );

	/* prepare for next */
	total_iters += iter;
	iter = 0;
	if (setitimer(ITIMER_REAL, &rt, NULL) < 0) {
		perror("*** Could not set real itimer");
		exit(2);
	}
	stimes = times(&stime1);
	gettimeofday(&stval);
}

/*
 * findmintime - find minimum setitimer accuracy
 */
ulong
findmintime(void)
{
	ulong acc;

	signal(SIGALRM, SIG_IGN);

	rt.it_value.tv_sec = 0;
	rt.it_value.tv_usec = 1;
	rt.it_interval.tv_sec = 0;
	rt.it_interval.tv_usec = 1;
	if (setitimer(ITIMER_REAL, &rt, NULL) < 0) {
		perror("*** Could not set real itimer");
		exit(2);
	}
	if (getitimer(ITIMER_REAL, &rt) < 0) {
		perror("*** Could not get real itimer");
		exit(2);
	}
	acc = (ulong)rt.it_interval.tv_usec;
	/*
	printf("valsec:%d valusec:%d intsec:%d intusec:%d\n",
		rt.it_value.tv_sec,
		rt.it_value.tv_usec,
		rt.it_interval.tv_sec,
		rt.it_interval.tv_usec);
	*/


	rt.it_value.tv_sec = 0;
	rt.it_value.tv_usec = 0;
	rt.it_interval.tv_sec = 0;
	rt.it_interval.tv_usec = 0;
	if (setitimer(ITIMER_REAL, &rt, NULL) < 0) {
		perror("*** Could not set real itimer");
		exit(2);
	}
	return(acc);
}

void
Usage(void)
{
	fprintf(stderr,"Usage:rt [-p][-P][-r #sec][-l #min][-w #mS][-i #mS\n");
	fprintf(stderr, "\t-p     do not get special priority\n");
	fprintf(stderr, "\t-P     do not pin application pages\n");
	fprintf(stderr, "\t-r #   report every # of seconds\n");
	fprintf(stderr, "\t-l #   stop after # of minutes\n");
	fprintf(stderr, "\t-w #   do # of mS of work\n");
	fprintf(stderr, "\t-i #   interval in # mS for response\n");
	exit(1);
}

/* work --- routine to perform n microseconds of useless work */

#define MIN_DT 1.0		/* Min accepatble timing loop in seconds */

double	loops_per_micro = -1;	/* timing constant computed in compute_speed */
double	total_count	= 0;	/* total amount of work, int would overflow  */


/*--------------------------------------------------------------------------*/

double work( int n )	/* Perform n microseconds of work. Timing constants */
			/* must be initialized by calling compute_speed first*/
{
	/* convert from microseconds to loop count */
	int	count = n * loops_per_micro;

	if ( loops_per_micro < 0 )		/* Compute inital constants */
		(void) compute_speed();

	/* perform useful work */
	for( ; count > 0; count-- )
		;

	total_count += n;

	return total_count;
}

/*--------------------------------------------------------------------------*/

double compute_speed(void) 	/* Compute timing constants and return	*/
				/* loops of work per microsecond	*/
{
	int	n = 1;
	double	t1, t2, dt;

	/* Compute timing constants */
	loops_per_micro = 1;
	do {
		n *= 2;
		t1 = second_();
			(void) work( n );
		t2 = second_();
		dt = t2 - t1;
	} while ( dt < MIN_DT );

	loops_per_micro = n / (dt*1e6);
	total_count	= 0;

	return loops_per_micro;
}

/*--------------------------------------------------------------------------*/

double compute_error( int interval ) /* Compute percent error at user specifed */
				 /* time interval (microseconds)	   */
{
	int	limit = MIN_DT * 1e6 / interval;
	int	i;
	double	t1, t2, dt, expect = limit * interval / 1e6;

	if ( loops_per_micro < 0 )		/* Compute inital constants */
		(void) compute_speed();

	/* Validate computed constants */
	t1 = second_();
	for( i=0; i < limit; i++ )
		(void) work( interval );
	t2 = second_();
	dt = t2 - t1;

	return ( (expect-dt)/expect * 100 );	/* percent error at interval */
}

/*--------------------------------------------------------------------------*/

#ifdef TEST
#include <stdio.h>

main()
{
	int i;

	printf( "Min dt for timing loops = %f\n", MIN_DT );
	printf( "Loops per microsecond   = %f\n", compute_speed() );

	for( i = 10; i < 1e6; i*=2 )
		printf( "Timing error for %6d microseconds = %.2f%%\n",
			i, compute_error( i ) );
}
#else
void mwork(int n)
{
	printf( "\n" );
	printf( "Work Timing Information:\n" );
	printf( "	Min dt for timing loops = %f\n", MIN_DT );
	printf( "	Loops per microsecond   = %f\n", compute_speed() );
	printf( "	Timing error for %6d uS = %.2f%%\n",
		n, compute_error( n ) );
	printf( "\n" );
}
#endif

/*--------------------------------------------------------------------------*/

/* include param.h to define BSD or HZ depending on OS */
#include <sys/param.h>

#ifdef BSD

#include <sys/time.h>
#include <sys/resource.h>

double
second_ (void)
{
  struct rusage ru;
  getrusage (RUSAGE_SELF, &ru);
  return (double)ru.ru_utime.tv_sec + ((double)ru.ru_utime.tv_usec * 1.0e-6);
}

#else

double
second_ (void)
{
  struct tms tm;
  (void)times(&tm);
  return (double)tm.tms_utime / (double)HZ;
}

#endif

long
timesub(struct timeval *s, struct timeval *e)
{
	long anysecs, res;

	anysecs = e->tv_sec - s->tv_sec;
	if (anysecs == 0) {
		res = e->tv_usec - s->tv_usec;
	} else {
		res = (anysecs-1) * 1000000;
		res += e->tv_usec;
		res += (1000000 - s->tv_usec);
	}
	return(res);
}
