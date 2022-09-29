#if R4000 || R10000
/*
 * r4k.c - Generic R4000/R10000 routines
 *
 * $Revision: 1.15 $
 */

#include <arcs/types.h>
#include <arcs/spb.h>
#include <libsc.h>
#include <libsk.h>
#include <sys/sbd.h>
#include <sys/cpu.h>

extern __uint32_t set_watchlo(__uint32_t);	/* libsk/ml/r4kasm.s */
extern __uint32_t set_watchhi(__uint32_t);	/* libsk/ml/r4kasm.s */

extern __psunsigned_t set_watch(__psunsigned_t, int);

#if !IP30
/*
 * GetRelativeTime - return real time seconds
 *
 * The R4000 version uses the internal free running counter.  If
 * the time between calls is longer than a single overflow of the
 * counter, then this won't be accurate.  If you want to time
 * something that takes a real long time, then make sure this
 * gets called periodically, or use the GetTime() function.
 */
ULONG
GetRelativeTime(void)
{
	static int	lasttime;	/* last returned value */
	static unsigned	cummdelta;	/* cummulative fraction of a sec. */
	static unsigned	lasttick;	/* last value of count register */
	static unsigned	tickhz;		/* ticker speed in Hz */
	unsigned	thistick;	/* current value of count register */

	if ( ! lasttick ) {
		lasttick = r4k_getticker();
		tickhz = cpu_get_freq(cpuid());
		return 0;
	}

	thistick = r4k_getticker();

	if ( thistick < lasttick )
		cummdelta += (0xffffffff - lasttick) + thistick;
	else
		cummdelta += thistick - lasttick;
	
	lasttick = thistick;

	while ( cummdelta > tickhz ) {
		cummdelta -= tickhz;
		lasttime += 1;
	}

	return lasttime;
}
#endif

#if IP20 || IP22 || IP28
int
splerr(void)
{
	return spl(SR_BERRBIT|SR_IEC);
}
#endif	/* IP20 || IP22 || IP28 */

__psunsigned_t
set_watch(__psunsigned_t paddr, int trap_type)
{
	__uint32_t paddr_lo;
	__psunsigned_t prev;

	paddr_lo = (__uint32_t)((paddr & WATCHLO_ADDRMASK) | trap_type);
	prev = (__psunsigned_t)set_watchlo(paddr_lo);
#if (_MIPS_SZPTR == 64)
	paddr_lo = (__uint32_t)(paddr >> 32) & WATCHHI_VALIDMASK;
	prev = ((__psunsigned_t)set_watchhi(paddr_lo) << 32) | prev;
#else
	(void)set_watchhi(0x0);
#endif	/* _MIPS_SZPTR == 64 */

	return prev;
}

#endif
