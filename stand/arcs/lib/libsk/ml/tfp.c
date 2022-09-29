/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <sys/types.h>
#include <sys/sbd.h>
#include <arcs/types.h>
#include <arcs/spb.h>
#include <libsc.h>
#include <libsk.h>

int	_icache_size;		/* bytes of icache */
int	_picache_size;		/* bytes of primary icache */
int	_dcache_size;		/* bytes of dcache */
int	_pdcache_size;		/* bytes of primary dcache */
int	cachewrback;		/* writeback secondary cache? */

int	_sidcache_size;		/* bytes of secondary cache */
int	_scache_linemask;	/* secondary cache line mask */

void	 invaltlb_in_set(int, int);
void	 tfp_flush_icache(__psint_t, unsigned int);
void	 __dcache_inval(void *, int);
void	 __dcache_wb_inval(void *, int);
void	 __dcache_wb_inval_all(void);
unsigned tfp_getticker(void);

void invaltlb(int index)
{
	int set;
	
	for (set = 0; set < NTLBSETS; set++)
		invaltlb_in_set(index, set);
}

/*
 * flush_tlb(index)
 *
 * On non-TFP platforms, the parameter specifies the start index of the TLB 
 * operation.  On those platforms we never flush the PDA, typically starting
 * the flush after the UPAGE/KSTACK wired entries or at the start
 * of the random entries.  During initialization is the only time we actually
 * flush the UPAGE/KSTACK entries.
 *
 * On TFP systems, a compatible set of operations is defined by allowing a
 * flag value of 1 to indicate that the UPAGE/KSTACK tlb should be flushed.
 * All other flag values result in the flushing of only random entries (i.e.
 * the PDA/UPAGE/KSTACK are not flushed).
 */
/*ARGSUSED*/
void flush_tlb(int flag)
{
	int index;

	for (index=0; index < NTLBENTRIES; index++)
		invaltlb(index);
}


/*
 * void clean_icache(void *addr, unsigned int len)
 *
 */
void clean_icache(void *addr, unsigned int len)
{
	__psint_t real_addr;

	/* Round addr down to the nearest cacheline strat */
	real_addr = ((__psint_t)addr) & ~(__psunsigned_t)ICACHE_LINEMASK;

	/* Adjust len for starting at a lower address */
	len += (__psint_t)addr - real_addr;

	/*
	 * Convert len to cache lines, rounding up,
	 * convert real_addr into an icache index,
	 * and call tfp_flushicache
	 */
	tfp_flush_icache(real_addr & (ICACHE_SIZE - 1),
			 (len + (ICACHE_LINESIZE - 1)) / ICACHE_LINESIZE);
}


/*
 * void clean_dcache(void *addr, unsigned int len)
 *
 */
/*ARGSUSED*/
void clean_dcache(void *addr, unsigned int len)
{
#if EVEREST
	__dcache_inval((void *)NULL, _pdcache_size);
	/* Take off the bottom bits and nail those lines. */
#endif
#if IP26
	__dcache_wb_inval(addr,len);
#endif
}

void clear_cache(void *addr, unsigned int len)
{
	clean_dcache(addr, len);
	clean_icache(addr, len);
}

/*
 * void flush_cache()
 *
 *	Flush both the i and dcache (don't need dcache on EVEREST TFP).
 *
 */
void flush_cache(void)
{
#if EVEREST
	/* Just flush out the icache here. */
	tfp_flush_icache(0, ICACHE_SIZE / ICACHE_LINESIZE);
#endif
#if IP26
	tfp_flush_icache(0,_picache_size / ICACHE_LINESIZE);
	__dcache_wb_inval_all();
#endif
}

/*
 * GetRelativeTime - return real time seconds
 *
 * The TFP version uses the internal free running counter.  If
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
		lasttick = tfp_getticker();
		tickhz = cpu_get_freq(0);
		return 0;
	}

	thistick = (int) tfp_getticker();

	if ( thistick < lasttick )
		cummdelta += (0xffffffff - lasttick) + thistick;
	else
		cummdelta += thistick - lasttick;
	
	lasttick = thistick;

	while ( cummdelta > tickhz ) {
		cummdelta -= tickhz;
		lasttime += 1;
	}

	return (ULONG) lasttime;
}
