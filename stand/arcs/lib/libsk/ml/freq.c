/* cpu frequency determination
 */
#ident	"$Revision: 1.54 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <libsc.h>
#include <libsk.h>

#ifdef EVEREST
#ifndef LARGE_CPU_COUNT_EVEREST
#define MAXCPU 64
#endif
#else
#ifndef MAXCPU
#define MAXCPU 1
#endif
#endif

#ifdef IP30		/* on cluster bus all cpus are the same */
#undef MAXCPU
#define MAXCPU 1
#endif

#if IP26

#define SYSAD_COUNT	20000
#define SYSAD_FREQ	50000000

static unsigned int cpu_freq[MAXCPU];

#define abs(x)		((x)>0 ? (x) : ((x)*-1))

int CountTccVsSysAdCycles(int);

unsigned
cpu_get_freq(int cpuid)
{
    unsigned count;
    unsigned freq;
    char *cp;
    static unsigned freq_table[] = {
	65000000, 70000000, 75000000, 80000000, 84000000, 90000000, 0
    };

    if (cpu_freq[cpuid])
	return cpu_freq[cpuid]; /* Assume it doesn't change */

    count = (unsigned) CountTccVsSysAdCycles(SYSAD_COUNT);

    /* if the count is less than the minimum, return SysAd freq */
    /*  this shouldn't happen */
    if (count < SYSAD_COUNT)
	return SYSAD_FREQ;

    /* calculate true frequency */
    freq = count * (SYSAD_FREQ/SYSAD_COUNT);

    /* if we are in diagmode, display the true frequency, otherwise look */
    /* it up in a real grainy table */
    if ((cp = getenv("diagmode")) && *cp == 'f') {

        /* round the freq to the near 1 MHz */
        freq = ((freq + 500000)/1000000)*1000000;

    } else {
	int i = 0;
	unsigned val = 0;

	/* look up the frequency in a table */
	for (; freq_table[i]; i++)
	    if (abs(freq_table[i]-freq) < abs(freq_table[i]-val))
		val = freq;

	freq = val;
    }

    cpu_freq[cpuid] = freq;

    return freq;
}

#elif IP20 || IP22 || IP28 || IP30

#include <sys/immu.h>
#include <sys/i8254clock.h>

#ifdef DEBUG
extern int Debug;
#endif

#if IP22
/* IOC1 & IOC2 has a broken 8254 programmable clock.  */
#define _USE_DALLAS_TIMEBASE 1
#define	getticker	r4k_getticker
#define	setticker	r4k_setticker
#include <sys/ds1286.h>
#elif IP20
#define _USE_TICKSPER1024 1
static int ticksper1024_mem[160];
#elif IP28
#define _USE_TICKSPER80MS 1
#elif IP30
#define _USE_TICKSPER10MS 1
#endif

#define INST_COUNT	1024

#ifndef	DD
#define DD
#endif

/* Private */
static unsigned int cpu_freq[MAXCPU];

/* this is the table of known cpu freq that we support 
** table is in the order of increasing freq 
**
** XXX- please add comments for supported systems
** XXX- note: 133mhz R4k reported as 132; 175mhz as 174.
*/
static int freq[] = {
     30000000,
     33000000,
     36000000,
     40000000,
     50000000,		/* 100mhz R4400 */
     66666000,		/* 133mhz R4400 */
     75000000,		/* 150mhz R4400; 75mhz R8000 (IP26) */
     87370000,		/* 175mhz R4400/T5 (IP22 PM6) */
     90000000,		/* 180mhz R5000 */
#ifdef R10000
     95000000,		/* 190mhz T5 */
     97500000,		/* 195mhz T5 (T5I2, RACER + SN0 MR) */
#endif
    100000000,		/* 200mhz R4400 (IP22 PM3, PM7) */
#if R10000
    112500000,		/* 225mhz 3.[12] T5 */
#endif
    125000000,		/* 250mhz R4400 (IP22 PM5) */
    135000000,		/* 270mhz TREX */
    137500000,		/* 275mhz TREX */
    150000000,
    157500000,
    166500000,		/* 333mhz TREX */
    175000000,		/* 350mhz TREX */
    180000000,		/* 360mhz TREX Shrink */
    187500000,		/* 375mhz TREX */
    192500000,		/* 385mhz TREX Shrink */
    195000000,		/* 390mhz TREX Shrink */
    200000000,		/* 400mhz TREX */
    250000000,	      
    300000000,
    350000000,
    400000000,
    500000000
};

unsigned
cpu_get_freq(int cpuid)
{
	unsigned ticks;
	int i, closest,tmp,clock;
#if _USE_DALLAS_TIMEBASE
	volatile struct ds1286_clk *rt_clock =
		(struct ds1286_clk *) PHYS_TO_K1(RT_CLOCK_ADDR);
	unsigned tickstart;
	int sec;
#elif _USE_TICKSPER10MS
	extern unsigned _ticksper10ms(void);
#elif _USE_TICKSPER80MS
	extern unsigned _ticksper80ms(void);
#else
	extern int _ticksper1024inst(), _delayend();
	int (*fp)(void);
	int fcount;
#endif

#if IP30
	cpuid = 0;		/* on cluster bus all cpus are the same */
#endif

	if (cpuid)
		return(0);		/* invalid cpuid (no MP support) */

	if ( cpu_freq[cpuid] )
		return cpu_freq[cpuid]; /* Assume it doesn't change */

#if _USE_DALLAS_TIMEBASE
	cpu_restart_rtc();		/* make sure clock is running */
	setticker(0);			/* avoid rollover */
	sec = rt_clock->sec;
	while (rt_clock->sec == sec)	/* wait for first rollover */
	    ;
	tickstart = getticker();
	sec = rt_clock->sec;
	while (rt_clock->sec == sec)	/* now wait for 2nd */
	    ;

	/* Get the approximate count during that second
	 *	- r4k timer is 1/2 cpu frequency.
	 *	- tfp timer matchs cpu frequency.
	 */
	ticks = getticker() - tickstart;

#if DEBUG
	if (Debug)
	    printf ("We got %d instructions per real time second.\n", ticks);
#endif
#elif _USE_TICKSPER10MS
	ticks = _ticksper10ms() * 100;
#elif _USE_TICKSPER80MS
#ifdef IP28
	cpu_restart_rtc();		/* make sure clock is running */
#endif
	ticks = (_ticksper80ms() * 100) >> 3;
#else
	fcount = 0;
	bcopy((char *)_ticksper1024inst, ticksper1024_mem,
	      (char *)_delayend - (char *)_ticksper1024inst + 4);
	flush_cache();

freq_loop:
	fp = (int (*)())K1_TO_K0(ticksper1024_mem);
	ticks = fp();			/* # of ticks to execute 1024 inst */

	/* these limits are valid for counters running at 1 and 3.6864 MHz
	 * and clock rates between 10 and 50 MHz
	 */
#if DEBUG
	if (Debug)
	    printf("Ticks/1024 instructions = %d\n", ticks);
#endif
	if((ticks > 400 || ticks < 5) && fcount<10 ) {
		fcount++;
		goto freq_loop;
	}
	if (fcount >=10) {
#ifdef IP20
		/* Ugly hack.  Set cpu speed to 75 MHz if
		 * we can't figure it out.
		 */
		return cpu_freq[cpuid] = 75000000;
#else
		printf("Can't determine CPU speed\n");
		return cpu_freq[cpuid] = freq[(sizeof freq/sizeof freq[0])-1];
#endif
	}

	/* convert to Hz 
	 */
	ticks = INST_COUNT * (MASTER_FREQ / ticks);	/* leave parens alone */
#endif /* _USE_DALLAS_TIMEBASE */

	closest = -1;
	for (i=0; i<(sizeof(freq)/sizeof(int)); i++) {
		if ( ticks > freq[i] )
			tmp = ticks - freq[i];
		else
			tmp = freq[i] - ticks;
#if DEBUG
	if (Debug)
		printf ("At frequency %9d the difference is now %9d.\n",
			freq [i], tmp);
#endif
		if ( closest == -1) {
			closest = tmp;
			clock = i;
			continue;
  		}
		if ( tmp < closest ) {
			closest = tmp;
			clock = i;
#if DEBUG
	if (Debug)
			printf ("Therefore the closest frequency is now %9d\n",
				freq [i]);
#endif
  		}
		
	}

#if MCCHIP
	{
		/*
		 * use the following to calculate the mc memory refresh counter
		 * preload
		 *	62500 / (1000 / ((freq / 1000000) * 2 / sysad_divisor))
		 *
		 * 62500 is the fixed memory refresh period in nanoseconds
		 */
		uint ctrld;
#ifdef R4000
		uint sysad_divisor;
		int divisor_index = (r4k_getconfig() & CONFIG_EC) >> 28;
		static uint r4000_divisor_tbl[] = {2, 3, 4, 6, 8};
#ifdef R4600
		static uint r4600_divisor_tbl[] = {2, 3, 4, 5, 6, 7, 8};

		if (((get_prid() & 0xFF00) >> 8) == 0x20 ||
			((get_prid() & 0xFF00) >> 8) == 0x21 ||
			((get_prid() & 0xFF00) >> 8) == 0x23)
			sysad_divisor = r4600_divisor_tbl[divisor_index];
		else
#endif	/* R4600 */
		     {
			sysad_divisor = r4000_divisor_tbl[divisor_index];
		}

		/*
		 * do the division first to allow some margin of error,
		 * over-refreshing is probably better than under-refreshing
		 */

#ifdef IP22
		/* PM5 runs MC @ 50Mhz */
		if (sysad_divisor == 4 && freq[clock] == 125000000)
			ctrld = ((100000000 / 1000000) / 4) * 125;
		else
#endif
		ctrld = ((freq[clock] / 1000000) / sysad_divisor) * 125;
#endif /* R4000 */
#ifdef R10000
		static char sysclk_div_n[] = {  1, 1, 2, 1, 2, 1, 2, 1,
						2, 1, 2, 1, 2, 1, 2, 1 };
		static char sysclk_div_d[] = {  1, 1, 3, 2, 5, 3, 7, 4,
						9, 5,11, 6,13, 7,15, 8 };
		unsigned int ec = (r4k_getconfig()&CONFIG_EC) >> CONFIG_EC_SHFT;

		ctrld = (((freq[clock] / 1000000) * sysclk_div_n[ec]) * 125) /
			sysclk_div_d[ec];
#endif /* R10000 */

		*(volatile uint *)PHYS_TO_K1(CTRLD) = ctrld;
	}
#endif	/* MCCHIP */

	return cpu_freq[cpuid] = freq[clock];
}

#endif	/* !IP26*/

/* this is used only from nvram.c to set $cpufreq, which is
 * how hinv gets the cpu frequency.  Pretty gross all around.
 * Apparently the IP19 series do this differently...
 * For R4K systems, we now want hinv to show the internal speed.
 */
char *
cpu_get_freq_str(int cpuid)
{
    static char cpu_freq_str[MAXCPU][4]; /* Seems big enough for now! */
    int freq = cpu_get_freq(cpuid);

#if EVEREST || MCCHIP || IP30		/* still count the old way */
    freq *= 2;
#endif

    /* convert to Mhz after scaling for odd frequencies */
    freq /= 1000000;

    if (!cpu_freq_str[cpuid][0])
	    sprintf(cpu_freq_str[cpuid],"%d", freq);

    return cpu_freq_str[cpuid];
}
