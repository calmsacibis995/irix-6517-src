/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.64 $"

#include "limits.h"
#include "sys/types.h"
#include "sys/systm.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/debug.h"
#include "sys/i8254clock.h"
#include "sys/immu.h"
#include "sys/pda.h"
#include "sys/cmn_err.h"
#if IPMHSIM
#include "sys/kopt.h"
#endif

#if IP22			/* for extra hacks for bad IOC1 timer */
#include <sys/cpu.h>
#endif

extern unsigned int _cpuclkper100ticks(unsigned int *);
extern int _ticksper1024inst(void);

#ifndef IP26	/* uses seperate counter for us_delay() */
/*
 * Instructions per DELAY loop.  The loop body is a branch with a decrement
 * in the delay slot:
 *	1:	bgtz	s0,1b
 *		subu	s0,1
 */
#define	IPL	2
#ifndef IP32	/* uses seperate delay_calibrate */
void
delay_calibrate(void)
{
	register int tpi;

#ifdef IPMHSIM
	private.decinsperloop = 10;
#else /* IPMHSIM */
#if IP22
	if (!is_fullhouse() && is_ioc1() < 2)
	    tpi = 0;	/* Early Indies had a broken 8254 */
			/* (but they also only had 100Mhz cpus) */
	else
#endif
	tpi = _ticksper1024inst();

	if (tpi==0) {
		/*
		** The timer's broken.  Just use some reasonable
		** value that's on the large side.
		**
		** 4 == ~50Mhz R4000.
		*/
		private.decinsperloop = 10 * 4;
		return;
	}

	/*
	** Calculate the 100 * number of deci-nanoseconds per loop.
	** This is used to determine how high us_delay should
	** count.
	** 
	** We now calculate 10 * deci-nanoseconds since as processors
	** got faster we lost significant digits (50Mhz R4000 varies
	** is usually 3, sometimes 4) making the rounding of the
	** calculation more significant.
	**
	** dns/loop = (dns/sec)/(ticks/sec) * inst/loop * ticks/inst
	** 
	** Since we can have processors with different speed running 
	** on the same system, decinsperloop is in the pda.
	*/
	private.decinsperloop = 
		(10 * (100000000/MASTER_FREQ) * IPL * tpi)/1024;
#endif /* IPMHSIM */
}
#endif /* !IP32 */

#ifdef R4600SC
int early_delay_flag = 0;
#endif

/*
** Spin in a tight loop for "us" microseconds.  It is expected that
** decinsperloop will be properly calibrated (via a call to
** delay_calibrate) sometime before us_delay is called.
**
** XXX: for R4600SC we need to call this early, before PDA is set up
**      so we set the flag and use the value of 40, which is taken
**      from delay_calibrate() above.  early_delay_flag is set and 
**	unset in size_2nd_cache() [cacheops.s].  This should probably
**	really be done in mlsetup() but I didn't want to screw w/it
**	since size_2nd_cache is the only thing in the kernel which 
**	needs this support.
**
** XXX: oops, I got this wrong... the value of decinsperloop goes DOWN
**      as the CPU gets faster -- I thought it represented the number
**      of times to go through the loop directly (as it does in the
**      USL routines in SVR4).  This value is 17 on a 4600PC 100Mhz,
**	and is 13 on a 4600SC 133Mhz.
*/
void
us_delay(register uint us)
{
	void	delayloop(int,int);
	int	i;
#ifdef R4600SC
	extern int early_delay_flag;
	int n;
	int d = early_delay_flag ? 7 : private.decinsperloop;

	ASSERT(d!=0);
#else
	int n;
	int d = private.decinsperloop;

	ASSERT(private.decinsperloop!=0);
#endif

	/*
	 * delayloop wants nanoseconds so we need to do several of them
	 * if the delay is greater than ~2 seconds.  Why is it ever more
	 * than 2 seconds?  Only the sleep gods know.
	 */
	for (i = 1; us > INT_MAX/1000; i++, us >>= 1)
		;
	n = (1000) * us;
	while (i--)
		delayloop(n,d);
}
#endif	/* ndef IP26 */

/*
 * delay, but first make sure bus is clear, so delay is guaranteed
 * to be relative to bus, not CPU
 */
void us_delay(register uint us);
void
us_delaybus(register uint us)
{
	flushbus();
	if (us > 0)
		us_delay(us-1);	/* sub off time for flushbus (approx) */
	return;
}

#ifndef TFP
/*
** fig out max # of passes in clean_dcache() clean_icache()
** before allowing intr back on
** assuming 3.6MHZ crystal --> 277ns per tick
** want to preempt every 250us
**
*/
#define INST_PER_PASS	15	/* the observed number via disassembler */
#define CACHE_PREEMPT_LIMIT	500000	/* 500 us */
void
cache_preempt_limit(void)
{
	int tmp;

#ifdef IPMHSIM
	if (is_specified(arg_cpufreq) &&
	    (tmp = atoi(arg_cpufreq)) > 0) {
		tmp *= 1000000;
		tmp /= (1000000000 / CACHE_PREEMPT_LIMIT);
	} else
		tmp = 120000; /* assume 240 MHZ */
	tmp /= 2; /* assume 2 cycles per instruction */
#else /* IPMHSIM */
#if IP22
	if (!is_fullhouse() && is_ioc1() < 2)
	    tmp = 19;	/* Early Indies had a broken 8254 */
			/* (but they also only had 100Mhz cpus) */
	else
#endif
	tmp = _ticksper1024inst();
	/* time in nano sec per cached inst */	
#ifndef IP32
	tmp = (tmp*(1000000000/MASTER_FREQ))/1024;
#else
	tmp = (DNS_PER_TICK * IPL * tmp)/1024;
#endif
	tmp = CACHE_PREEMPT_LIMIT/tmp;
					/* max # of insts that can be executed
					   before turning intr back on */
#endif /* IPMHSIM */
}
#endif	/* TFP */


#ifdef IP26

#define SYSAD_COUNT     20000
#define SYSAD_FREQ      50000000

#define abssub(x,y)          ((x)>(y) ? (x)-(y) : (y)-(x))

int CountTccVsSysAdCycles(int);

/*  Teton always has a 50.0Mhz SysAD bus and a counter on TCC running at this
 * speed.  We can use the ratio of C0_COUNT to this to calculate the processor
 * frequency.
 */
int
findcpufreq_raw(void)
{
    unsigned count = (unsigned) CountTccVsSysAdCycles(SYSAD_COUNT);
    unsigned freq;
    int i, entry = 0;
    unsigned val = SYSAD_FREQ;
    static unsigned freq_table[] = {
        65000000, 70000000, 75000000, 80000000, 84000000, 90000000, 0
    };

    /* if the count is less than the minimum, return SysAd freq */
    /*  this shouldn't happen */
    if (count < SYSAD_COUNT)
        return SYSAD_FREQ/1000000;

    /* calculate true frequency */
    freq = count * (SYSAD_FREQ/SYSAD_COUNT);

    /* look up the frequency in a table */
    for (i = 0; freq_table[i] > 0; i++) 
        if (abssub(freq_table[i],freq) < val)
            val = abssub(freq_table[i], freq), entry = i;
 
    return freq_table[entry];
}

#else /* !IP26 */

/* this is the table of known cpu freq that we support 
** table is in the order of increasing freq
**
** XXX- please add comments for supported systems
** round to 1000 so we get integer divide
*/

#ifndef IP32
static int freq[] = {
#if 0
	20000000,
	25000000,
	30000000,
#endif
	33000000,
	36000000,
	40000000,
	50000000,		/* 100mhz R4000 */
#ifdef IP28
	60000000,		/* 120mhz T5 prototype */
	70000000,		/* 140mhz T5 prototype */
#else
	66666000,		/* 133mhz R4600 */
#endif
	75000000,		/* 150mhz R4400 */
	80000000,
	85000000,
#if IP22
	87370000,		/* 175mhz R4400 (PM6) */
#else
	87500000,		/* 175mhz T5 prototype */
#endif
	90000000,
#ifdef R10000
	95000000,		/* 190mhz T5 prototype */
	96000000,		/* 192mhz T5 prototype */
	97500000,		/* 195mhz T5 (T5I2 MR) */
#else
	94339000,
	95250000,
#endif /* R10000 */
       100000000,		/* 200mhz R4400 (PM3; PM7) */
       120000000,               /* 240mhz R5000 (Triton) */
       125000000		/* 250mhz R4400 (PM5) */
};

#else /* IP32 */
/* see ml/MOOSEHEAD/README.clocks */
static int freq[] = {
	66580000,		/* R4600 PROTOTYPE 133 MHz */
	75121000,		/* FPGA R10K 150 MHz */
	75170000,		/* R5K 150 MHz */
	87552000,		/* JUICE R10K 175 MHz */
	87641000,               /* FPGA R10K 175 MHz */
	90000000,		/* R5K 180 MHz */
	97953000,		/* JUICE R10K 195 MHz */
       100227000,		/* R5K 200 MHz (100.227260*2 Mhz) */
       112500000,		/* R10K 225 */
       125284000,		/* R5K/R10K 250 MHz (125.284075*2 Mhz) */
       135000000,		/* R12K 270 MHz */
       137750000,		/* R10k 275 Mhz (275.500 Mhz) */
       150345000,		/* R10k 300 Mhz (300.690 Mhz) */
       150339000,		/* R5k  300 Mhz (300.678 Mhz) */
       175000000,		/* R12K Shrink - 350 MHz */
       180000000,		/* R12K Shrink - 360 MHz */
       187500000,		/* R12K Shrink - 390 MHz */
       192500000,		/* R12K Shrink - 385 MHz */
       195000000,		/* R12K Shrink - 390 MHz */
       200000000,		/* R12K Shrink - 400 MHz */
};
#endif /* IP32 */

#if IP22 /* XXX DEBUG - figure out why cpu count is not repeatable! */
int _findcpufreq_c[8],
    _findcpufreq_x = 0,
    _findcpufreq_repeat = 0;
#endif

/*
** find the frequency of the running CPU
** should only be invoked on the master
**
** findcpufreq_raw() returns the frequency of the processor's master
** clock, in Hz. It should be used wherever knowing this value to any
** decent precision is required.
**
** findcpufreq() returns the frequency of the processor's master
** clock, in MHz.
**
** cpu_mhz_rating() returns the "rated" speed of the processor, which
** should always match the numbers in our marketing literature. For
** instance, a "PM5" processor module has a 125MHz crystal, and
** findcpufreq() will return 125, but this is really a "250MHz"
** R4400. This function should be used wherever the findcpufreq()
** number would have been doubled, as it handles things like 133MHz
** and 175MHz correctly.
*/
#if (defined(IP22) && defined(TRITON))
extern int get_cpu_irr(void);
#endif /* (defined(IP22) && defined(TRITON)) */

int
findcpufreq_raw(void)
{
	int ticks;
	int i, closest,tmp,clock, freq_ret;
	int freqtbl_siz = sizeof(freq)/ sizeof(int);
#if (defined(IP22) && defined(TRITON)) || defined(IP32)
	rev_id_t ri;

	ri.ri_uint = get_cpu_irr();
#endif

#ifdef IPMHSIM
	if (! (is_specified(arg_cpufreq) &&
	       (ticks = atoi(arg_cpufreq)) > 0))
		ticks = 240; /* assume 240 MHZ */
	return(ticks * 1000000);
#elif IP28
	extern unsigned int _ticksper80ms(void);
	ticks = (_ticksper80ms() * 100) >> 3;
#else /* IPMHSIM */
#if !(defined(IP20) || defined(IP22) || defined(IP28) || defined(IP32))
	extern int _ticksper1024inst();
	int fcount= 0;

freq_loop:
	ticks = _ticksper1024inst();	/* # of ticks to execute 1024 inst */
	/*
	** 8MHZ takes about 472 ticks for 1024 NOPs
	** 12MHZ   ""	    315		""
	** 16MHZ   ""	    236		""
	** 25MHZ   ""	    151		""
	** 50MHZ  R4000      20         ""
	** 75MHZ  R4400     ~15 (est)   ""
        ** 100Mhz R5000     ~10 (est)   ""
	*/
	if ((ticks > 1000 || ticks < 10) && fcount<10 ) {
		fcount++;
		goto freq_loop;
	}
	if (fcount >=10)
		return(0);
	/* based on MASTER_FREQ */
	/* time in ns to execute 1 NOP inst , 1 tick is about 271ns */
	ticks = (ticks*(1000000000/MASTER_FREQ))/1024;	
	ticks = 1000000000/ticks;	/* convert to HZ */
#else
#if IP22
	/* Early Indies can't call _cpuclkper100ticks() because of a
	 * broken i8254 timer.
	 */
	if (!is_fullhouse() && is_ioc1() < 2)
		ticks = 50000000;
	else
#endif	/* IP22 */
	{
#if IP22 /* XXX DEBUG - figure out why cpu count is not repeatable! */
		static
#endif  /* IP22 */
		unsigned int cpu_count, tick_count;

#if IP22 /* XXX DEBUG - figure out why cpu count is not repeatable! */
		if (_findcpufreq_repeat || _findcpufreq_x == 0)
#endif  /* IP22 */
		cpu_count = _cpuclkper100ticks(&tick_count);
		/* do division first to avoid overflow */
		ticks = cpu_count * (MASTER_FREQ / tick_count);	/* to HZ */

#if IP22 /* XXX DEBUG - figure out why cpu count is not repeatable! */
		_findcpufreq_c[(_findcpufreq_x++) % 8] = cpu_count;
		_findcpufreq_c[(_findcpufreq_x++) % 8] = tick_count;
#endif  /* IP22 */
	}
#endif	/* !(IP20 || IP22 || IP28 || IP32) */
#endif  /* IPMHSIM */

	closest = -1;
	for (i=0; i<(sizeof(freq)/sizeof(int)); i++) {
		if ( ticks > freq[i] )
			tmp = ticks - freq[i];
		else
			tmp = freq[i] - ticks;
		if ( closest == -1 ) {
			clock = i;
			closest = tmp;	
			continue;
		}
		if ( tmp < closest ) {
			closest = tmp;
			clock = i;
		}
		
	}
	freq_ret = freq[clock];

#if defined(IP22) && defined(TRITON)
	/*
	 * There isn't enough difference in the clock speeds
	 * between the 175Mhz R4400 and the 180Mhz R5000 for
	 * this code to reliably differentiate between them.
	 * Check to see if we have a 180Mhz R4400 or a 175Mhz
	 * R5000.
	 */
	if (freq_ret == 90000000 && ri.ri_imp == C0_IMP_R4400)
	    freq_ret = 87370000;

	if (freq_ret == 87370000 && ri.ri_imp == C0_IMP_TRITON)
	    freq_ret = 90000000;
#elif defined(IP32)
	/*
	 * Similarly, make sure that clock speeds of O2 (IP32)
	 * 175Mhz R10000 vs 180Mhz R5000 and
	 * 195Mhz R10000 vs 200Mhz R5000 
	 * are reliably differentiated.
	 */
	if (ri.ri_imp == C0_IMP_R10000 || ri.ri_imp == C0_IMP_R12000) {
	    /*
	     * there is no R10K 180, give the higher of 175 Mhz (FPGA)
	     * since we went over
	     */
	    if (freq_ret == 90000000)
		freq_ret = 87641000;
	    /*
	     * there is no R10K 200, return 195
	     */
	    else if (freq_ret == 100227000)
		freq_ret = 97953000;
	    /*
	     * Make sure R10K/R12K does NOT get R5k 300 MHz
	     */
	    else if (freq_ret == 150339000)
		freq_ret = 150345000;
	}

	if (ri.ri_imp == C0_IMP_TRITON) {
	    /*
	     * there is no R5k 175, return 180
	     */
	    if (freq_ret > 87000000 && freq_ret < 88000000)
		freq_ret = 90000000;
	    /*
	     * there is no R5k 195, return 200
	     */
	    else if (freq_ret == 97953000)
		freq_ret = 100227000;
	    /*
	     * Make sure R5K does NOT get R10k/R12k 300 MHz
	     */
	    else if (freq_ret == 150345000)
		freq_ret = 150339000;
	}
#endif

	/*
	** if the calculated freq is greater than the max that we know about
	** and if the margin is more than 4Mhz then lets use the calculated
	** freq instead of rounding it down to what's in the table
	*/
	if (ticks > freq[freqtbl_siz-1] && 
			(ticks - freq[freqtbl_siz-1]) > 4000000)
		freq_ret = ticks;

	return(freq_ret);
}

#endif /* !IP26 */

#define	ROUND(v,r)	(((v)+(r/2))/r)

int
findcpufreq(void)
{
    return ROUND(findcpufreq_raw(), 1000000);
}

int
cpu_mhz_rating(void)
{
    int		cpu_rate = findcpufreq_raw();

#if IP32
    if (cpu_rate >= 97000000 && cpu_rate <= 98000000)  /* 195MHz */
	    cpu_rate = 97500000;
    else if (cpu_rate >= 124000000 && cpu_rate <= 126000000) /* 250 MHz */
	    cpu_rate = 125000000;
    else if (cpu_rate >= 149000000 && cpu_rate <= 151000000) /* 300 MHz */
	    cpu_rate = 150000000;
#endif /* IP32 */
#if R4000 || R10000
    cpu_rate *= 2;
#endif
    cpu_rate = ROUND(cpu_rate, 1000000);
    return cpu_rate;
}
