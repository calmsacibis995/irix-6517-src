#ifndef __BNCH_H__
#define __BNCH_H__


#include <sys/time.h>

#include <Chk.h>

/*
	BnchDecl(t0);
	BnchDecl(t1);
	BnchDecl(tot);

	BnchZero(&tot);
	BnchStamp(&t0);
	BnchStamp(&t1);
	BnchDelta(&t0, &t0, &t1);
	BnchAccum(&tot, &t0);
	BnchPrint("result", &tot);
 */


/* WARNING: Avoid tests of period > 3 minutes
	mips mapped timer wraps after 193 secs although the delta code
	deals with this for single wraps
 */


#ifndef USPS
#define USPS	1000000
#endif

#if defined(mips) && !defined(GETTIMEOFDAY)

#define	BnchDecl(t)	struct timespec t

#define BnchZero(tv)	(tv)->tv_sec = (tv)->tv_nsec = 0

#define BnchUSasDouble(tv) \
	((tv)->tv_sec * USPS + (double)(tv)->tv_nsec/1000)

#define BnchPrint(label, tv) \
	BnchPrInfo("%-30s: %16.3lfus\n", label, BnchUSasDouble(tv))

#define	BnchStamp(tv)	(void)clock_gettime(CLOCK_SGI_CYCLE, tv)

void BnchDelta(struct timespec *, struct timespec *, struct timespec *);

#define BnchAccum(tot, tv) \
		(tot)->tv_nsec += (tv)->tv_nsec;	\
		if ((tot)->tv_nsec >= NSEC_PER_SEC) {	\
			(tot)->tv_nsec -= NSEC_PER_SEC;	\
			(tot)->tv_sec++;		\
		}					\
		(tot)->tv_sec += (tv)->tv_sec;

#define BnchDivide(tv, dsor) \
	{	long long _t;					\
		_t = (long long)((tv)->tv_sec) * NSEC_PER_SEC + (tv)->tv_nsec; \
		(tv)->tv_sec = (_t / (dsor)) / NSEC_PER_SEC;	\
		(tv)->tv_nsec = (_t / (dsor)) % NSEC_PER_SEC;	\
	}

#else /* mips && !GETTIMEOFDAY */

#define	BnchDecl(t)	struct timeval t

#define BnchZero(tv)	(tv)->tv_sec = (tv)->tv_usec = 0

#define BnchUSasDouble(tv) \
	((double)((tv)->tv_sec * USPS + (tv)->tv_usec))

#define BnchPrint(label, tv) \
	BnchPrInfo("%-30s: %16ld.000us\n", \
		   label, (tv)->tv_sec * USPS + (tv)->tv_usec)

#define	BnchStamp(tv)	(void)gettimeofday(tv, 0)

#define	BnchDelta(tv, tv0, tv1)	\
	if ((tv1)->tv_usec >= (tv0)->tv_usec) {				\
		(tv)->tv_sec = (tv1)->tv_sec - (tv0)->tv_sec;		\
		(tv)->tv_usec = (tv1)->tv_usec - (tv0)->tv_usec;	\
	} else {							\
		(tv)->tv_sec = (tv1)->tv_sec - (tv0)->tv_sec - 1;	\
		(tv)->tv_usec = (tv1)->tv_usec - (tv0)->tv_usec + USPS; \
	}

#define BnchAccum(tot, tv) \
		(tot)->tv_usec += (tv)->tv_usec;	\
		if ((tot)->tv_usec >= USPS) {		\
			(tot)->tv_usec -= USPS;		\
			(tot)->tv_sec++;		\
		}					\
		(tot)->tv_sec += (tv)->tv_sec;

#define BnchDivide(tv, dsor) \
	{	long long _t;					\
		_t = (long long)((tv)->tv_sec) * USPS + (tv)->tv_usec; \
		(tv)->tv_sec = (_t / (dsor)) / USPS;	\
		(tv)->tv_usec = (_t / (dsor)) % USPS;	\
	}

#endif /* mips && !GETTIMEOFDAY */


#define HUGE_BENCH_LOOP		5000000
#define LARGE_BENCH_LOOP	50000
#define MED_BENCH_LOOP		500
#define SMALL_BENCH_LOOP	10

#define BnchTries(S, N)	BnchPrInfo("%s averaged over %d tries\n", S, N)


#ifndef BnchPrInfo
#define BnchPrInfo	printf
#endif


#endif	/* __BNCH_H__ */
