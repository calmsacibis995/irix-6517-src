/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.303 $"

#include <string.h>
#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <sys/buf.h>
#include <sys/capability.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/getpages.h>
#include <sys/pfdat.h>
#include <sys/page.h>
#include <sys/immu.h>
#include <sys/map.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <ksys/vpgrp.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <sys/ksignal.h>
#include <sys/kthread.h>
#include <sys/runq.h>
#include <sys/sat.h>
#include <sys/sema.h>
#include <ksys/vsession.h>
#include <sys/signal.h>
#include <sys/splock.h>
#include <sys/sysmacros.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/tuneable.h>
#include <sys/var.h>
#include <sys/xlate.h>
#include <ksys/vproc.h>
#include "os/proc/pproc.h"
#if CELL
#include <ksys/cell.h>
#endif
#include <procfs/prsystm.h>
#include "ksys/vm_pool.h"

extern int gpgslo;
extern int gpgshi;
extern int gpgsmsk;
extern int maxsc;
extern int maxfc;
extern int maxdc;
extern int bdflushr;
extern int minarmem;
extern int minasmem;
extern int maxlkmem;
extern int rsshogfrac;
extern int maxup;
extern int dwcluster;
extern int shmmni;
extern int nlog;
extern int boot_sdcache_size;
extern int boot_sicache_size;
extern int boot_sidcache_size;

/*
 * These structures provide values for constants that may be used by
 * administrative applications.
 */

struct pteconst _pteconst = {
	/* pt_ptesize */	sizeof(pte_t),
	/* pt_pdesize */	sizeof(pde_t),
	/* pt_pfnwidth */	PFNWIDTH,
	/* pt_pfnshift */	PFNSHIFT,
	/* pt_pfnoffset */	PFNOFFSET
};

struct pageconst _pageconst = {
	/* p_pagesz */		_PAGESZ,
	/* p_pnumshft */	PNUMSHFT,
	/* p_nbpp */		NBPP
};

struct paramconst _paramconst = {
	/* p_usize */		KSTKSIZE,
	/* p_extusize */	EXTKSTKSIZE
};

int
get_pteconst(void *user_address, int insize, int *outsize)
{
	int error = 0;
	int size = MIN(insize, sizeof(_pteconst));
	
	error = copyout(&_pteconst, user_address, size);
	if (outsize) {
		error = copyout(&size, outsize, sizeof(int));
	}
	return error;
}
int
get_pageconst(void *user_address, int insize, int *outsize)
{
	int error = 0;
	int size = MIN(insize, sizeof(_pageconst));
	
	error = copyout(&_pageconst, user_address, size);
	if (outsize) {
		error = copyout(&size, outsize, sizeof(int));
	}
	return error;
}
int
get_paramconst(void *user_address, int insize, int *outsize)
{
	int error = 0;
	int size = MIN(insize, sizeof(_paramconst));
	
	error = copyout(&_paramconst, user_address, size);
	if (outsize) {
		error = copyout(&size, outsize, sizeof(int));
	}
	return error;
}

/* get_scachesetassoc
 *
 * Used by syssgi() to retrieve the secondary cache set associativity.
 */
int get_scachesetassoc(int *user_address)
{
	int error = 0;
	int setassoc = 1; /* Assume to be 1 */

	/* If the set associativity has been defined, return that
	 * value or else return the default. Similar to what we see
	 * in irix/kern/os/startup.c. The associativity is defined
	 * in klocaldefs, if at all.
	 */
#ifdef SCACHE_SET_ASSOC
	setassoc = SCACHE_SET_ASSOC;
#endif

	error = copyout(&setassoc, user_address, sizeof(int));
	return error;
}

/* get_scachesize
 *
 * Used by syssgi() to retrieve the secondary cache size.
 */
int get_scachesize(int *user_address)
{
	int error = 0;
	int scachesize;

	/* Similar to what we see in irix/kern/os/startup.c */
	scachesize = MAX(boot_sdcache_size, boot_sicache_size);
	scachesize = MAX(scachesize, boot_sidcache_size);

	error = copyout(&scachesize, user_address, sizeof(int));
	return error;
}

/*
 * Routine which sets a user error; placed in
 * illegal entries in the bdevsw and cdevsw tables.
 */
int
nodev(void)
{
	return ENODEV;
}

/*
 * Null routine; placed in insignificant entries
 * in the bdevsw and cdevsw tables.
 */
int
nulldev(void)
{
	return 0;
}

/*
 * Generate an unused major device number.
 */

int
getudev(void)
{
	extern short MAJOR[];
	static int next = 1;	/* never want hwgraph major allocated; see 572126 */
	int maxdevcnt = MAX(bdevcnt, cdevcnt);

	for ( ; next < NMAJORENTRY; next++)
		if (MAJOR[next] >= maxdevcnt)
			return(next++);
	return(-1);
}

int
freeudev(int maj)
{
	int maxdevcnt = MAX(bdevcnt, cdevcnt);

	if (MAJOR[maj] < maxdevcnt)
		return (-1);
	MAJOR[maj] = (maxdevcnt + 1);
	return 0;
}

int 		pghashmask;	/* process group hash table mask */
int 		sesshashmask;	/* session hash table mask */

sv_t		proctracewait;	/* sleepers waiting for process to stop */
mutex_t		proctracelock;	/* associated lock held during vsema of above */

int npalloc;			/* Number of proc structs allocated */
static void prochashtables(void);

/*
 * procinit - initialize process management locks.
 */
void
procinit()
{
	extern void 	pproc_init(void);
	extern void 	kstack_pool_init(void);

	pproc_init();
	vproc_init();
	pid_init();
	prochashtables();
#if KSTACKPOOL
	kstack_pool_init();
#endif

	npalloc = 1;			/* Count proc 0 */

	sv_init(&proctracewait, SV_DEFAULT, "ptracewait");
	mutex_init(&proctracelock, MUTEX_DEFAULT, "ptracelock");
}

zone_t *ecb_zone;

void
exit_callback_init()
{
	ecb_zone = kmem_zone_init(sizeof(struct exit_callback),"exit callback");
}

/*	Do sanity checking on tune structure.
 *	Set tune elements to sane state, in case tune
 *	points to the ``real'' system tune structure.
 *	Return non-zero value if any values are insane.
 *	For many elements, a zero value implies auto-tune.
 */
#define VHNDFRAC	12	/* try to keep from 4% to 8% of mem free */
#define DEFRSSHOGFRAC	75	/* allow 75% of memory to be used be RSS
				 * hogs */

/*
 * Cap gpgshi at 100m. This is so that the memory does not get to some 
 * ridiculously high value on large memory machines (> 10G).
 */
#define	GPGSHI_CAP	btoc(50*1024*1024) /* Max cap val (50mb) for gpgshi */

int
tune_sanity(struct tune	*tune)
{
	register int rtn = 0;
	register int npgs;

	/*	Since the page cache resides in free memory,
	 *	make sure that there is enough to provide
	 *	a minimum of buffering: on small memory systems,
	 *	keep ``free pages'' between 1Mb and 1.6 Mb; on
	 *	larger systems, between 4% and 8% of memory.
	 *	XXX this has been removed for small systems since
	 *	it seems liek small systems really need all the memory
	 *	they can get for applications.
	 */

	if (tune->t_gpgshi == 0) {
		npgs = maxmem/VHNDFRAC;
		gpgshi = tune->t_gpgshi = npgs;
		if (gpgshi > GPGSHI_CAP)
			gpgshi = tune->t_gpgshi = GPGSHI_CAP;
	} else if (tune->t_gpgshi >= maxmem) {
		gpgshi = tune->t_gpgshi = maxmem/VHNDFRAC;
		rtn++;
	}

	if (tune->t_gpgslo == 0) {
		npgs = tune->t_gpgshi/2;
		gpgslo = tune->t_gpgslo = npgs;
	} else if (tune->t_gpgslo >= tune->t_gpgshi) {
		gpgslo = tune->t_gpgslo = tune->t_gpgshi/2;
		rtn++;
	}

#ifdef JUMP_WAR
	gpgsmsk = tune->t_gpgsmsk = 0;
#endif
	if (tune->t_gpgsmsk > NDREF) {
		gpgsmsk = tune->t_gpgsmsk = NDREF;
		rtn++;
	}

	if (tune->t_maxsc == 0) {
		maxsc = tune->t_maxsc = maxpglst;
	} else if (tune->t_maxsc > maxpglst) {
		maxsc = tune->t_maxsc = maxpglst;
		rtn++;
	}

	if (tune->t_maxdc == 0) {
		maxdc = tune->t_maxdc = maxpglst;
	} else if (tune->t_maxdc > maxpglst) {
		maxdc = tune->t_maxdc = maxpglst;
		rtn++;
	}

	if (tune->t_maxfc == 0) {
		maxfc = tune->t_maxfc = maxpglst;
	} else if (tune->t_maxfc > maxpglst) {
		maxfc = tune->t_maxfc = maxpglst;
		rtn++;
	}

	if (tune->t_bdflushr == 0) {
		bdflushr = tune->t_bdflushr = 5;
	}

	if (tune->t_minarmem == 0) {
		minarmem = tune->t_minarmem = 50;
	} else if (tune->t_minarmem > tune->t_gpgshi) {
		minarmem = tune->t_minarmem = tune->t_gpgshi;
		rtn++;
	}

	if (tune->t_minasmem == 0) {
		minasmem = tune->t_minasmem = 50;
	} else if (tune->t_minasmem > tune->t_gpgshi) {
		minasmem = tune->t_minasmem = tune->t_gpgshi;
		rtn++;
	}

	/* need fractional tune or var value here */
	if (tune->t_maxlkmem == 0)
		maxlkmem = tune->t_maxlkmem = maxmem - maxmem/4;
	else if (tune->t_maxlkmem > maxmem - maxmem/4) {
		maxlkmem = tune->t_maxlkmem = maxmem - maxmem/4;
		rtn++;
	}

	/* t_tlbdrop - no restriction */

	/*
	 * RSS tunables
	 */
	if (tune->t_rsshogfrac > 100) {
		rsshogfrac = tune->t_rsshogfrac = DEFRSSHOGFRAC;
		rtn++;
	}

	/* t_rsshogslop - no restriction */

	/*
	 * Do not let dwcluster go past maxdmasz, we can get
	 * silent file corruption - (see pv 608092)
	 */
	if (tune->t_dwcluster >= v.v_maxdmasz) {
		dwcluster = tune->t_dwcluster = v.v_maxdmasz - 1;
		rtn++;
	}

	return(rtn);
}


/*
 * Create hash tables for proc and process group.
 * Also create table for uid hash list, the size of
 * which is based on v_proc.
 */

static void
prochashtables(void)
{
	int m;

        /* compute hash table size - (power of 2 > maxusers) / 16 */

        m = v.v_proc;

        while (m & (m - 1))
                 m = (m | (m - 1)) + 1;
        m = m >> 4;

        if (m == 0)
                m = 1;
        pghashmask = m;
	sesshashmask = (m > 1) ? (m >> 1) : m;
 
	uidacthash_init(m);
}

typedef struct {
	pid_t	pr_pid;
	time_t	pr_start;
} procref_t;

/*
 * void *proc_ref(void)
 *	Establish a reference upon the currently executing process.
 *
 * 	This function is to be called only by device drivers.
 */
void *
proc_ref(void)
{
	procref_t	*pr;
	vp_get_attr_t	attr;

	pr = kern_malloc(sizeof(procref_t));
	
	VPROC_GET_ATTR(curvprocp, VGATTR_SIGNATURE, &attr);

	pr->pr_pid = current_pid();
	pr->pr_start = attr.va_signature;

	return(pr);
}

/*
 * int proc_signal(void *pref, int sig)
 *	Post the specified signal to the designated process previously
 *      referenced via a proc_ref(D3DK) call.
 * 	This function returns 0 if the designated process exists and 
 *	we post the signal.  Otherwise, -1 is returned.
 *
 * 	This function is to be called only by device drivers.
 */
int
proc_signal(void *pref, int sig)
{
	procref_t	*pr;
	vproc_t		*vpr;
	vp_get_attr_t	attr;
	int error;

	if (sigismember(&jobcontrol, sig))
		return -1;

	pr = (procref_t *)pref;

	vpr = VPROC_LOOKUP(pr->pr_pid);
	if (vpr == VPROC_NULL)
		return(-1);

	VPROC_GET_ATTR(vpr, VGATTR_SIGNATURE, &attr);

	if (attr.va_signature != pr->pr_start) {
		VPROC_RELE(vpr);
		return(-1);
	}

	VPROC_SENDSIG(vpr, sig, SIG_ISKERN, 0, 0, 0, error);

	VPROC_RELE(vpr);
	return error;
}

/*
 * void proc_unref(void *pref)
 *	Remove the reference to the process previously established 
 *	by a proc_ref(D3DK) call.
 *
 *	This function is to be called only by device drivers.
 */
void
proc_unref(void *pref)
{
	kern_free(pref);
}

int
activeproccount(void)
{
	return npalloc;
}

/*
 * This is called both at system startup (with an address == 0) and
 * for runtime changing of parameters. All users of these variables
 * reference the ones in tune.
 */
int
_paging_sanity(int *address, int newvalue)
{
	tune_t	newtune;
	int	olds, oldr;
	extern int tlbdrop, rsshogslop, dwcluster, autoup;

	if (address == NULL) {
		if (maxmem != 0)
			tune_sanity(&tune);
	} else {
		if (address == (int *)&(minarmem))
			oldr = *address;
		else
			oldr = tune.t_minarmem;
		if (address == (int *)&(minasmem))
			olds = *address;
		else
			olds = tune.t_minasmem;

		/* set new value into address */
		*address = newvalue;
		newtune.t_gpgslo = gpgslo;
		newtune.t_gpgshi = gpgshi;
		newtune.t_gpgsmsk = gpgsmsk;
		newtune.t_maxsc = maxsc;
		newtune.t_maxfc = maxfc;
		newtune.t_maxdc = maxdc;
		newtune.t_bdflushr = bdflushr;
		newtune.t_minarmem = minarmem;
		newtune.t_minasmem = minasmem;
		newtune.t_maxlkmem = maxlkmem;
		newtune.t_tlbdrop = tlbdrop;
		newtune.t_rsshogfrac = rsshogfrac;
		newtune.t_rsshogslop = rsshogslop;
		newtune.t_dwcluster = dwcluster;
		newtune.t_autoup = autoup;
		if (tune_sanity(&newtune))
			return EINVAL;

		/*
		 * Readjust availrmem.  These counters are always
		 * adjusted by tune.t_minarmem so that availrmem
		 * can always be compared to 0 instead having to go to
		 * memory to compare with tune.t_minarmem.
		 * Also, don't change tune while vhand is running.
		 */
		reservemem(GLOBAL_POOL, (olds - newtune.t_minasmem),
					(oldr - newtune.t_minarmem), 0);
		bcopy(&newtune, &tune, sizeof(tune_t));

		return(0);
	}
	return(0);
}

/* The minimum breathing space between maxup and nproc, so one process
 * cannot consume all the system processes.
 */
#define MAXUP_UNDER_NPROC 20

int
_numproc_sanity(void)
{
	register int numprocs;
	extern int nproc, ndquot, ncsize, maxup;
	extern int ncallout;
	
#ifdef SABLE_RTL
	if (nproc == 0)
		nproc = 30;		/* keep stuff small for sable */
#endif

	if (nproc == 0) {
#define MBYTE		(1024 * 1024)
#define GBYTE		(1024ll * 1024ll * 1024ll)
#define B_TO_MB(b)	(b / MBYTE)

		/* Autoconfig nproc - this algorithm yields:
		 * 4 processes per MB for the 1st GB
		 * 2 processes per MB for the next GB
		 * 1 process per MB for 2-4 GB,
		 * 1 process per 2 MB for 4-8 GB,
		 * 1 process per 4 MB for 8-16 GB
		 * etc.
		 * Plus, we throw on a fudge of 40 to give a nicer
		 * function at the bottom end.
		 */

		long long mbytes = ctob(physmem) / MBYTE;

		/* GB's expressed in MB chunks */
		long long gbs_in_mbs = GBYTE / MBYTE;

		int nproc_per_mbyte = 4;
		int normalize_shift = 1;

		/* Fudge to make this nicer at the bottom end. */
		nproc = 40;

		while (mbytes > 0) {
			if (nproc_per_mbyte == 0) {
				mbytes >>= normalize_shift;
				normalize_shift <<= 1;
				nproc_per_mbyte = 1;
			}
			nproc += MIN(mbytes, gbs_in_mbs) * nproc_per_mbyte;
			mbytes -= gbs_in_mbs;
			gbs_in_mbs *= 2;
			nproc_per_mbyte >>= 1;
		}
		
		/* Can't exceed 32K since pid_slot_t has short fields */

		nproc = MIN(nproc, 32767);
#undef MBYTE
#undef GBYTE
#undef B_TO_MB
	}

	/*
	 *  nproc is constrained by the number of available spinlocks.
	 *  Exhausting spinlocks will cause a panic in initlock().
	 */
	numprocs = nproc;


	if (ndquot == 0)
		ndquot = 200 + 2 * numprocs;

	if (ncsize == 0)
		ncsize = 200 + 2 * numprocs;

	if (ncallout == 0)
		ncallout = numprocs/2;

	/* don't allow one process to consume all the nproc slots */
	if (maxup == 0)
		maxup = MAX(150, numprocs/4);
	maxup = MIN(maxup, numprocs-MAXUP_UNDER_NPROC);

	/*
	 * This should really be set in _shm_sanity,
	 * but nproc is not set up yet at that point.
	 */
	if (shmmni == 0) {
		shmmni = nproc;
	}

	if (nlog == 0) {
		nlog = nproc;
	}

	return(0);
}

int 
_actions_sanity()
{

	extern int nactions;

	if (nactions == 0)
		/* Allows every cpu to queue at least one action to every
		 * other cpu.  Each cpu has its' own queue of action block
		 * which must be used when sending a cpuaction to that
		 * cpu. Two entries come "free" in the PDA, nactions specifies
		 * additional entries for each cpu.
		 * Empiracal evidence suggests that one block per cpu is
		 * sufficient for "normal" operation.
		 */
		nactions = maxcpus;
	return(0);
}

int
_memsize_sanity()
{
	register int s;
	extern int nproc, syssegsz, maxdmasz;
	int cmapsz = 0;
	int m;

	if ((s = syssegsz) == 0)
		s = physmem / 2;

	s = MAX(s, 0x2000);
	s = MIN(s, btoct(KSEGSIZE));
	syssegsz = s;

	if (maxdmasz > s / 2)
		maxdmasz = s / 2;

#if R4000
	if (nproc == 0)
		_numproc_sanity();

	/*
	 * Add enough syssegsz to give one virtual page color per
	 * process.  Limit this to ~1/8 of syssegsz
	 *
	 * mktables will handle overflowing max syssegsz,
	 * and also page table page alignment.
	 */
	{
		extern int colormap_size;
		colormap_size = nproc * CACHECOLORSIZE;
		colormap_size = MIN(colormap_size, syssegsz/8);
		cmapsz = colormap_size;
	}
#endif /* R4000 */
	syssegsz += cmapsz;

	/*
	 * Round up to occupy a full page of ptes.
	 */
	m = syssegsz * sizeof(pde_t);
	syssegsz = ctob(btoc(m)) / sizeof(pde_t);

	if (syssegsz > btoct(KSEGSIZE))
		syssegsz = btoct(KSEGSIZE);

	/* Bitmap code requires this. */
	ASSERT((syssegsz & ~(BITSPERLONG - 1)) == syssegsz);

	return(0);
}

int
_bufcache_sanity()
{
	extern int	nbuf;
	extern int	ncsize;
	extern int	_bhash_shift;

	register int m, b;
#if _MIPS_SIM != _ABI64
#define	NBUF_LIMIT	6000
#else
#define	NBUF_LIMIT	600000	
#endif

	if ((b = nbuf) == 0) {
#ifndef SABLE_RTL
		b = 100 + (ctob(physmem)/MIN_NBPC) / 40;

		/*
		 * Keep the number of buffers in line.  The limit
		 * used here is pretty arbitrary, but seems to work
		 * OK.  On SN0 machines we need to limit this so
		 * that we don't run out of low memory when allocating
		 * the array of buf structures.
		 */
		if (b > NBUF_LIMIT)
			b = NBUF_LIMIT;
		nbuf = b;
#else
		nbuf = b = 100;		/* keep things small on simulator */
#endif /* SABLE_RTL */
	}

	m = b / 16;
	/*
	 * minimum value for nbuf is 75, therefore minimum m is 4 
	 */
	for (b = 2; (1 << b) < m; b++)
		;
	v.v_hbuf = 1 << b;
	v.v_hmask = v.v_hbuf - 1;
	_bhash_shift = b;

	return(0);
}

int
_resource_sanity(void)
{
	extern rlim_t rlimit_data_cur;
	extern rlim_t rlimit_data_max;
	extern rlim_t rlimit_vmem_cur;
	extern rlim_t rlimit_vmem_max;

	/*
	 * handle rlimit_rss_cur in main after basic kernel tables have
	 * been set up. This may need to change if 'resources' become
	 * dynamically changeable
	 */

#if EVEREST || SN0
#define DEFAULT_SIZE_LIMIT	RLIM_INFINITY
#elif IP26 || IP28 || IP30 || IP32
#define DEFAULT_SIZE_LIMIT	0x80000000
#else
#define DEFAULT_SIZE_LIMIT	0x40000000
#endif

	if (rlimit_data_cur == 0)
		rlimit_data_cur = DEFAULT_SIZE_LIMIT;
	if (rlimit_data_max == 0)
		rlimit_data_max = DEFAULT_SIZE_LIMIT;

	if (rlimit_vmem_cur == 0)
		rlimit_vmem_cur = DEFAULT_SIZE_LIMIT;
	if (rlimit_vmem_max == 0)
		rlimit_vmem_max = DEFAULT_SIZE_LIMIT;

#undef DEFAULT_SIZE_LIMIT

	return(0);
}

#if IP20 || IP22 || IP26 || IP28 || IP32
#define	COUNTER2_FREQ	5000
#endif
#if !defined(COUNTER2_FREQ)
#define	COUNTER2_FREQ	3600
#endif
int
_timer_sanity()
{
	extern int fasthz;
	register int f = fasthz;

#if defined (IP27)
#define DEF_FASTHZ	1250
	if (f != DEF_FASTHZ)
	    fasthz = DEF_FASTHZ;
#elif !(IP19 || IP20 || IP22 || IP26 || IP28 || IP30 || IP32)
#define MIN_FASTHZ	900
#define DEF_FASTHZ	1200
#define MAX_FASTHZ	1800
	if (!f) {
		fasthz = DEF_FASTHZ;		/* default it if not set */
	} else if (f != MIN_FASTHZ && f != DEF_FASTHZ && f != MAX_FASTHZ) {
		fasthz = DEF_FASTHZ;
	}
#else
#define MIN_FASTHZ	500
#define DEF_FASTHZ	1000
#define MAX_FASTHZ	2500	/* 5000 causes the counter1 value to be 1, */
				/* which is not valid in mode 2 */
	if (!f) {
		fasthz = DEF_FASTHZ;		/* default it if not set */
	} else if (f < MIN_FASTHZ) {		/* clamp to range */
		fasthz = MIN_FASTHZ;
	} else if (f > MAX_FASTHZ) {
		fasthz = MAX_FASTHZ;
#if !defined(IP19)
	} else if ((COUNTER2_FREQ/f) * f != COUNTER2_FREQ) {
		/* Make sure fast rate gives us an integer divide */
		fasthz = DEF_FASTHZ;
#endif /* !defined(IP19) */
	}
#endif
	return(0);
}

/*
 * Tune the size of shmmax to be 80% of the system memory if the
 * default value is 0. 
 * If default is non-zero, leave it at that.
 * Calculation is done as (total - 0.2*total) = 0.8total.
 *
 * Tune shmmni to nproc.
 */
extern size_t shmmax;
int
_shm_sanity()
{
	if (shmmax == 0) {
		shmmax = ctob(physmem) - ctob(physmem)/5;
	}

	/* shmmni is set in _numproc_sanity,
	 * since nproc is not set up yet.
	 */

	return 0;
}

int
_debug_sanity(void *address, uint64_t arg)
{
	extern int kmem_do_poison;
	extern int r12k_bdiag;
	extern int _utrace_sanity(void *, uint64_t);

	/* check against special debug subsets */
	if (_utrace_sanity(address, arg) == 0)
		return 0;

#ifdef R10000

	/*
	 * setting of Branch Diag Register for configuring R12K features
	 */
	if (address == &r12k_bdiag) {
		cpuid_t      cpu;
		__uint32_t   bdiag;
		__uint32_t   mask = (C0_BRDIAG_GHISTORY_MASK | C0_BRDIAG_BTAC_MASK);
		cpu_cookie_t cookie;

		extern __uint32_t read_branch_diag  (void);
		extern void       write_branch_diag (__uint32_t);

		for (cpu = 0 ; cpu < maxcpus ; cpu++) {

			if (! cpu_enabled(cpu))
				continue;

			cookie = setmustrun(cpu);
			if (IS_R12000()) {

				bdiag  = read_branch_diag();

				bdiag &= ~mask;
				bdiag |= (arg & mask);

				write_branch_diag(bdiag);

			}
			restoremustrun(cookie);
		}
		*((uint32_t *)address) = (int)arg;
		return 0;	
	}
#endif



	/* simple variables now, skip if address == NULL as in early boot */
	if (address == NULL)
		return 0;

	/* check for 64-bit variables ... */
	/* none currently */

	/* otherwise assume it's a simple 32-bit variable */
	*((uint32_t *)address) = (int)arg;
	return 0;
}

int
_io_sanity()
{
	extern int graph_vertex_per_group;
	extern int graph_vertex_per_group_log2;
	extern int io_init_node_threads;
	extern int io_init_async;
	int i, c;

	/* Don't let big systems init. too much at once, or they may hang. */
	/* The hardcoded values below seem to work well. */
	if (io_init_node_threads == 0 && maxcpus > 32) {
	    io_init_async = 0;
	    io_init_node_threads = 16;
	    if (graph_vertex_per_group == 128) /* default value */
		graph_vertex_per_group = 16;
	}

	/* Make sure graph_vertex_per_group is a power of 2 */
	for (c = 0, i = 1; i << 1 != 0; i <<=1, c++) {
		if (i >= graph_vertex_per_group) {
			break;
		}
	}
	graph_vertex_per_group = i;
	graph_vertex_per_group_log2 = c;

	return 0;
}

/* this is only used by the SGI_TUNE syssgi() call.  Since the
 * same group name can occur more than once, and the order of
 * N_RUN and N_STATIC when this happens was never written down,
 * people will get it wrong.  So only return success if the
 * flag is N_RUN, else keep searching.  The table is small, so
 * the extra loop iterations when only an N_STATIC entry is
 * found are negligible.  See bug #346319.
*/
int
tunetablefind(char *s)
{
	struct tunetable *kt;
	int index = 0;
	extern struct tunetable tunetable[];

	for (kt = tunetable, index = 0; *(kt->t_name) != NULL; kt++, index++) {
		if (!strcmp(s, kt->t_name) && (kt->t_flag & N_RUN))
			return(index);
        }
        return(-1);
}

#define	CPBUFSIZE	200

int
xlate_copyout(
	void *from,
	void *to,
	int size,
	xlate_out_func_t xlatefunc,
	int user_abi,
	int kern_abi,
	int count)
{
	char smallbuf[CPBUFSIZE];
	xlate_info_t info;
	int error;

	if (user_abi & kern_abi) {
		if (copyout(from, to, size)) {
			return EFAULT;
		} else {
			return 0;
		}
	}

	info.inbufsize = CPBUFSIZE;
	info.smallbuf = smallbuf;
	info.copybuf = NULL;
	info.copysize = 0;
	info.abi = user_abi;

	if (error = (*xlatefunc)(from, count, &info)) {
		ASSERT(info.copybuf == NULL);
		return error;
	}

	ASSERT(info.copybuf != NULL);
	ASSERT(info.copysize != 0);

	/*
	 * This assert is not valid with the lv driver.  Once the lv
	 * driver is done away with, the assert can be reenabled.
	ASSERT((info.copysize > CPBUFSIZE &&			\
		info.copybuf != (void *)smallbuf) ||		\
		(info.copysize <= CPBUFSIZE &&			\
		info.copybuf == (void *)smallbuf));
	 */

	if (copyout(info.copybuf, to, info.copysize)) {
		error = EFAULT;
	}

	if (info.copybuf != (void *)smallbuf)
		kern_free(info.copybuf);

	return error;
}

int
copyin_xlate(
	void *from,
	void *to,
	int size,
	xlate_in_func_t xlatefunc,
	int user_abi,
	int kern_abi,
	int count)
{
	char smallbuf[CPBUFSIZE];
	xlate_info_t info;
	int error;

	if (user_abi & kern_abi) {
		if (copyin(from, to, size))
			return EFAULT;
		return 0;
	}

	info.inbufsize = CPBUFSIZE;
	info.smallbuf = smallbuf;
	info.copybuf = NULL;
	info.copysize = 0;
	info.abi = user_abi;

	if (error = (*xlatefunc)(SETUP_BUFFER, NULL, count, &info)) {
		ASSERT(info.copybuf == NULL);
		return error;
	}

	ASSERT(info.copybuf != NULL);
	ASSERT(!count || info.copysize != 0);
	ASSERT((info.copysize > CPBUFSIZE &&			\
		info.copybuf != (void *)smallbuf) ||		\
		(info.copysize <= CPBUFSIZE &&			\
		info.copybuf == (void *)smallbuf));

	if (copyin(from, info.copybuf, info.copysize)) {
		if (info.copybuf != (void *)smallbuf)
			kern_free(info.copybuf);
		return EFAULT;
	}

	error = (*xlatefunc)(DO_XLATE, to, count, &info);

	if (info.copybuf != (void *)smallbuf)
		kern_free(info.copybuf);

	return error;
}

/*
 * Retrieve cell id.  Returns 0 for non-cell configurations.
 */
cell_t
cellid(void)
{
#if CELL
#if MULTIKERNEL
	return evmk_cellid;
#else
	return my_cellid;
#endif
#else
	return 0;
#endif
}

dev_t
blocktochar(dev_t dev)
{
	vertex_hdl_t	connect_vhdl,char_dev_vhdl;

	connect_vhdl = hwgraph_connectpt_get(dev);
	char_dev_vhdl = hwgraph_char_device_get(connect_vhdl);
	hwgraph_vertex_unref(connect_vhdl);
	return(char_dev_vhdl);
}

dev_t
chartoblock(dev_t dev)
{
	return(hwgraph_block_device_get(hwgraph_connectpt_get(dev)));
}
static async_vec_t	*async_queue;
static int		async_size;
static int		async_in;
static int		async_out;
static lock_t		async_lock;
static sv_t 		async_sv;
#ifdef DEBUG
static int		async_count;
static int		async_maxcount;
#endif

void
async_call_daemon(void)
{
	async_vec_t	async_vec;

	spinlock_init(&async_lock, "async_call");
	sv_init(&async_sv, SV_DEFAULT, "async_call");

	ASSERT(async_queue == NULL && async_size == 0);
	async_size = 32 + maxcpus * 8;
	async_queue = kmem_zalloc(async_size * sizeof(async_vec_t), KM_SLEEP);

	for ( ; ; ) {
		int s = mutex_spinlock(&async_lock);

		if (async_out >= async_size)
			async_out = 0;

		if (async_queue[async_out].async_func) {
			async_vec = async_queue[async_out];
			async_queue[async_out].async_func = NULL;
			async_out++;
#ifdef DEBUG
			async_count--;
#endif
			mutex_spinunlock(&async_lock, s);

			(*async_vec.async_func)(&async_vec);

		} else {
			sv_wait(&async_sv, 0, &async_lock, s);
		}
	}
}

int
async_call(async_vec_t *asv)
{
	int s;

	ASSERT(async_queue);

	/* 
	 * 498683: if async_queue isn't set up yet, then we can't
	 *         send a signal yet ... fail.  this can happen if
	 *         the power button is pressed before the system
	 *         has gotten up.
	 */
	if (!async_queue) return EAGAIN;

	s = mutex_spinlock(&async_lock);
	if (async_in >= async_size)
		async_in = 0;

	if (async_queue[async_in].async_func) {
		mutex_spinunlock(&async_lock, s);
		return EAGAIN;
	}

	async_queue[async_in] = *asv;
	async_in++;

#ifdef DEBUG
	async_count++;
	if (async_count > async_maxcount)
		async_maxcount = async_count;
#endif
	sv_signal(&async_sv);
	mutex_spinunlock(&async_lock, s);
	return 0;
}
