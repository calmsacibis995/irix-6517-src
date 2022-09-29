/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1995, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.2 $"

#ifdef	SPLDEBUG
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/systm.h>
#include <sys/sema.h>
#include <sys/splock.h>
#include <sys/kopt.h>
#include <sys/debug.h>
#include <sys/inst.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>

void spldebug_log_event(int);


#ifdef EVEREST
#define MAXCPUS EV_MAX_CPUS
int __spl6(void);
int __spl65(void);
int __splset(int);
#else /* !EVEREST */
#ifndef MAXCPUS
#define MAXCPUS 1
#endif
int __splhi_relnet(void);
int __splhi(void);
int __splprof(void);
#endif /* !EVEREST */


int __spl0(void);
int __spl7(void);
void __splx(int);
void do_splx_log(int, int);

int spldebug;

#endif /* SPLDEBUG */
#if defined(SPLDEBUG) || defined(SPLDEBUG_CPU_EVENTS)

#define	SPLX_LOG 256
struct {
	int	splx_s;
	uint	splx_ra;
	uint	splx_sr;
	uint	splx_cel;
	uint64_t	splx_rtc;
} splx_log[MAXCPUS][SPLX_LOG];

/* We only use the first value for each cpu to contain the current index.
 * Defined as 32 ints per cpu so each cpu has index on its' own cacheline
 * (we saw severe performance degradation on cpuaction events when all
 * cpus were trying to frequently update the log info).
 */
int splx_x[MAXCPUS][32];	/* each index on own cacheline */
int spldebug_log_off=0;

void
do_splx_log(s, ra)
{
	int mycpuid=cpuid(), myidx;

	if (spldebug_log_off)
		return;
	myidx = atomicIncWithWrap(&splx_x[mycpuid][0], SPLX_LOG);
#if CLOCK_CTIME_IS_ABSOLUTE
	splx_log[mycpuid][myidx].splx_rtc =  GET_LOCAL_RTC;
#else
	{
		uint64_t microsecs;
		time_t	secs;
		uint32_t	rtc;

		secs = time;
		rtc = get_timestamp();
		microsecs = 1000000*secs + rtc;
		
		splx_log[mycpuid][myidx].splx_rtc =  microsecs;
	}
#endif
	splx_log[mycpuid][myidx].splx_s = s;
	splx_log[mycpuid][myidx].splx_ra = ra;
	splx_log[mycpuid][myidx].splx_sr = getsr();
#ifdef EVEREST
	splx_log[mycpuid][myidx].splx_cel = load_double_lo((long long *)EV_CEL);
#else /* !EVEREST */
	splx_log[mycpuid][myidx].splx_cel = 0;	/* unused for now */

#endif 
}

void _prsymoff(void *, char *, char *);

/* idbg_splx_logtime takes the RTC timestamp and returns a value
 * (in microseconds) which is relative to the start of the current splx
 * log buffer.
 */
int
idbg_splx_logtime(uint64_t timer)
{
	int mycpuid=cpuid();
	uint64_t timebase, rettime;
	
	timebase = splx_log[mycpuid][splx_x[mycpuid][0]].splx_rtc;
#if EVEREST
	rettime = (21*(timer-timebase))/1000;
#else
	rettime = timer-timebase;
#endif
	return(rettime);
}

#if EVEREST
int
#else
void
#endif
idbg_splx_log(int arg1)
{
	int mycpuid=cpuid(), i, j, logoff;
	uint64_t timebase, lasttime;
	union mips_instruction inst;

	/* need to stop logging if we haven't already */
	logoff = spldebug_log_off;
	spldebug_log_off = 1;

	qprintf("splx_log for cpu %d (next index %d)\n", mycpuid,
		splx_x[mycpuid][0]);

	timebase = splx_log[mycpuid][splx_x[mycpuid][0]].splx_rtc;
	lasttime = 0;

	qprintf("timebase is 0x%llx (oldest log entry)\n", timebase);


	if (arg1) {	/* try decoding the info */
	  for (j=0, i=splx_x[mycpuid][0]; j<SPLX_LOG; j++,i++) {
		uint64_t rtc, pc, tmp, thistime;

		if (i >= SPLX_LOG)
			i = i - SPLX_LOG;

		rtc = splx_log[mycpuid][i].splx_rtc;

#if EVEREST
		thistime = ((rtc-timebase)*21)/1000;
#else
		thistime = rtc-timebase;
#endif /* !EVEREST */
		qprintf("%lld us\t(%lld)\t",
			thistime, thistime-lasttime );
		lasttime = thistime;

		pc = (long long)splx_log[mycpuid][i].splx_ra;

#if _MIPS_SIM == _ABI64
#ifdef MAPPED_KERNEL
		pc = MAPPED_KERN_RO_BASE + pc -8;
#else
		pc = K0BASE + pc -8;
#endif
#else
		pc -= 8;
#endif
		_prsymoff((void *)pc, 0, 0);

		inst.word = *(inst_t *)pc;

		tmp = (((__psint_t)(pc+4)&~((1<<28)-1))
			     | (inst.j_format.target<<2));

		if (inst.j_format.opcode != jal_op) {
			if ((inst.j_format.opcode == spec_op) &&
			    (inst.r_format.func == jalr_op))
				qprintf("\tjalr ");
			else
				qprintf("\tUNKNOWN");
		} else if (tmp == (uint64_t)&spldebug_log_event) {
			qprintf("\tjal spldebug_log_event 0x%x ",
				splx_log[mycpuid][i].splx_s);
		} else {
			_prsymoff((void *)tmp, ":\tjal ", " ");
			qprintf(" lvl 0x%x ", splx_log[mycpuid][i].splx_s);
		}

		qprintf(" (0x%8x 0x%8x)\n",
			splx_log[mycpuid][i].splx_sr,
			splx_log[mycpuid][i].splx_cel);
	  }
	} else {	/* we prefer our info "raw" */
	  for (i=0; i<SPLX_LOG; i++) {
		uint64_t rtc;

		rtc = splx_log[mycpuid][i].splx_rtc;

		qprintf("%2d: [0x%llx, %lld us] s 0x%8x ra 0x%8x sr 0x%8x cel 0x%8x\n", i, 
			rtc,
#if EVEREST
			((rtc-timebase)*21)/1000,
#else
			rtc-timebase,
#endif
			splx_log[mycpuid][i].splx_s,
			splx_log[mycpuid][i].splx_ra,
			splx_log[mycpuid][i].splx_sr,
			splx_log[mycpuid][i].splx_cel);
	  }
	}
	spldebug_log_off = logoff;
#if EVEREST
	return(0);
#endif
	
}

void
_spldebug_log_event( int label, int ra )
{
	do_splx_log( label, ra );
}
#endif /* SPLDEBUG || SPLDEBUG_CPU_EVENTS */
#ifdef SPLDEBUG

void
_splx(s, ra)
{
	do_splx_log(s, ra);

	__splx(s);
}

_spl0(ra)
{
	int s;

	do_splx_log(0, ra);

	s = __spl0();

	return s;
}


/* spl6, splhi */

#if EVEREST
_spl6(ra)
{
	int s;

	do_splx_log(6, ra);
	s = __spl6();
	return s;
}

_spl65(ra)
{
	int s;

	do_splx_log(5, ra);
	s = __spl65();
	return s;
}
#else
_splhi(ra)
{
	int s;

	do_splx_log(6, ra);
	s = __splhi();
	return s;
}

_splprof(ra)
{
	int s;

	do_splx_log(5, ra);
	s = __splprof();
	return s;
}
#endif

_spl7(ra)
{
	int s;

	do_splx_log(7, ra);
	s = __spl7();
	return s;
}


#if EVEREST
void
_splset(s, ra)
{
	do_splx_log(s, ra);

	s =__splset(s);
}
#endif /* EVEREST */

int __mutex_spinlock(int *);
int __mutex_spinlock_spl(int *, int *);
void __mutex_spinunlock(int *, int);
int __mutex_bitlock(int *, int);
void __mutex_bitunlock(int *, int, int);
#if EVEREST
int __mutex_spintrylock(int *);
int __mutex_spintrylock_spl(int *, int *);
int __mutex_bittrylock(int *, int);
int __mutex_bitlock_spl(int *, int, int *);
int __mutex_64bitlock(__uint64_t *, __uint64_t);
void __mutex_64bitunlock(__uint64_t *, __uint64_t, int);
#endif /* EVEREST */

int
_mutex_spinlock(int *lock, int ra)
{
	do_splx_log(6, ra );
	return(__mutex_spinlock(lock) );
}

int
_mutex_spinlock_spl(int *lock, int *spllvl, int ra)
{

	do_splx_log((int)((long long)spllvl), ra );
	return(__mutex_spinlock_spl(lock, spllvl) );
}

#if EVEREST
int
_mutex_spintrylock(int *lock, int ra)
{
	do_splx_log(6, ra );
	return(__mutex_spintrylock(lock) );
}


int
_mutex_spintrylock_spl(int *lock, int *spllvl, int ra)
{

	do_splx_log((int)((long long)spllvl), ra );
	return(__mutex_spintrylock_spl(lock, spllvl) );
}
#endif /* EVEREST */
void
_mutex_spinunlock(int *lock, int spllvl, int ra)
{
	do_splx_log(spllvl, ra );
	__mutex_spinunlock(lock, spllvl);
}

int
_mutex_bitlock(int *lock, int bit, int ra)
{
	do_splx_log(6, ra );
	return(__mutex_bitlock(lock, bit) );
}

void
_mutex_bitunlock(int *lock, int bit, int spllvl, int ra)
{
	do_splx_log(spllvl, ra );
	__mutex_bitunlock(lock, bit, spllvl);
}


#if EVEREST
int
_mutex_bittrylock(int *lock, int bit, int ra)
{
	do_splx_log(6, ra );
	return(__mutex_bittrylock(lock, bit) );
}

int
_mutex_bitlock_spl(int *lock, int bit, int *spllvl, int ra)
{
	do_splx_log((int)((long long)spllvl), ra );
	return(__mutex_bitlock_spl(lock, bit, spllvl) );
}

int
_mutex_64bitlock(__uint64_t *lock, __uint64_t bit, int ra)
{
	do_splx_log(6, ra );
	return(__mutex_64bitlock(lock, bit) );
}

void
_mutex_64bitunlock(__uint64_t *lock, __uint64_t bit, int spllvl, int ra)
{
	do_splx_log(spllvl, ra );
	__mutex_64bitunlock(lock, bit, spllvl);
}
#endif /* EVEREST */
#endif /* SPLDEBUG */
