#ident	"lib/libsk/lib/skutils.c: $Revision: 1.15 $"

/*
 * skutils.c -- libsk utility routines
 */

#include <libsc.h>
#include <libsk.h>
#include <sys/param.h>

#if EVEREST
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evconfig.h>

#ifndef LARGE_CPU_COUNT_EVEREST
#define MAXCPU	EV_MAX_CPUS
#endif
#define	_SLOTS	EV_BOARD_MAX
#define	_SLICES	EV_MAX_CPUS_BOARD
#define MULTIPROCESSOR 1
#define DPRINT \
	if (EVCFGINFO->ecfg_debugsw & VDS_DEBUG_PROM) printf
#elif SN0
#include <sys/sbd.h>
#include <sys/cpu.h>

#ifndef MAXCPU
#define MAXCPU	MAXCPUS
#endif
#define	_SLOTS	1	/* fixme */
#define	_SLICES	2	/* fixme */
#define MULTIPROCESSOR 1
#define DPRINT \
	/* fixme if (EVCFGINFO->ecfg_debugsw & VDS_DEBUG_PROM) */ printf

#else /* !EVEREST && !SN0 */

#define DPRINT(a)
#define	_SLOTS	1
#define	_SLICES	1
#endif /* EVEREST */

#if !MULTIPROCESSOR
#define MAXCPU 1
#endif

extern int setup_idarr(int, int *, int *);

#if EVEREST
/*
 * Data structures to do logical <=> physical mapping of CPU IDs
 * These are mostly useful for EVEREST, 
 */
int vid_to_phys_idx[MAXCPU];    	/* vid --> phys_index */
int phys_idx_to_vid[MAXCPU];    	/* phys_index --> vid */
int vid_to_slot[MAXCPU];                /* vid --> slot */
int vid_to_slice[MAXCPU];               /* vid --> slice */
cpuid_t ss_to_vid[_SLOTS][_SLICES];     /* <slot,slice> --> vid */
#endif

int doass = 1;  /* for code compiled with ASSERT defined */

#ifdef DEBUG	/* so ASSERT's can be used */
void
assfail(char *a, char *f, int l)
{
	panic("assertion failed: %s, file: %s, line: %d",
		a, f, l);
}
#endif /* DEBUG */


void
delay(long ticks)
{
	DELAY(ticks * (1000000/HZ));
}


/*
 * Behaves similarly to get_cpu_freq, but returns its value in
 * Mhz rather than Hz, and converts from the profiling timebase
 * to the CPU rating ("double on R4k, R10k"; double before divide
 * so odd ratings like 133 and 175 are OK).
 */
int
cpufreq(int cpuid)
{
	int freq = cpu_get_freq(cpuid);
#if ((R4000 || R10000) && ! SN0)
	freq *= 2;
#endif
	return (freq + 500000) / 1000000;
}


#ifdef	EVEREST
/* convert <slot,slice> --> phys_index */
#define SS_TO_PHYS(_slot,_slice)	((_slot << 2) + _slice)
/*ARGSUSED*/
int
setup_idarr(int myid, int *maxid, int *cpucnt)
{
	evreg_t cpumask;
	int slot, slice, vid, phys_idx;
	
	*maxid = *cpucnt = 0;
	cpumask = (EV_GET_LOCAL(EV_SYSCONFIG) & EV_CPU_MASK) >> EV_CPU_SHFT;
    	for (slot=0; slot<_SLOTS; slot++) {
            if (cpumask & 1) {      /* Found slot with a processor board */
            	for (slice=0; slice<_SLICES; slice++) {
		    if (EVCFG_CPUDIAGVAL(slot,slice)) {
			ss_to_vid[slot][slice] = (cpuid_t)EV_CPU_NONE;
			continue;
		    }
                    vid = EVCFG_CPUID(slot,slice); /*logical-assigned by PROM */
#ifdef LARGE_CPU_COUNT_EVEREST
		    if ((vid & 0x01) && (vid < REAL_EV_MAX_CPUS))
		    	vid = EV_MAX_CPUS - vid;
#endif
                    phys_idx =  SS_TO_PHYS(slot, slice);	/* physical */
                    *cpucnt += 1;
                    /* processor-translation arrays */
                    vid_to_slot[vid] = slot;
                    vid_to_slice[vid] = slice;
                    vid_to_phys_idx[vid] = phys_idx;
                    phys_idx_to_vid[phys_idx] = vid;
                    ss_to_vid[slot][slice] = (cpuid_t)vid;

                    if (vid > *maxid)
                        *maxid = vid;
		}
            }
	    cpumask = cpumask >> 1;
        }
	return(*cpucnt);
}
#elif IP30
int
setup_idarr(int myid, int *maxid, int *cpucnt)
{
	extern int cpu_probe(void), cpu_probe_all(void);

        *cpucnt = cpu_probe();
	*maxid = cpu_probe_all()-1;	/* proc 0 may be disabled */
	return(*cpucnt);
}
#else
int
setup_idarr(int myid, int *maxid, int *cpucnt)
{
	*maxid = myid;
	*cpucnt = 1;
	return(*cpucnt);
}
#endif /* EVEREST */

