/*
 * File: 	r10k_cacherr.c
 * Purpose: 	Process r10k cache error exception.
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#if R10000

#ifdef R4000
#define ecc_handler r10k_ecc_handler
#define ecc_panic r10k_ecc_panic
#endif /* R4000 */

#include	<sys/types.h>
#include	<sys/sbd.h>
#include	<sys/cmn_err.h>
#include	<sys/reg.h>
#include	<sys/kmem.h>
#include 	<sys/debug.h>
#include	<sys/pda.h>
#include	<sys/systm.h>

#if	EVEREST

#include	<sys/EVEREST/everest.h>
#include	<sys/EVEREST/IP25.h>
#include	<sys/EVEREST/IP25addrs.h>
#include 	<sys/EVEREST/everror.h>
#include 	<sys/EVEREST/evconfig.h>

#elif IP28 || IP30 || IP32

#include 	<sys/cpu.h>

#elif 	SN0

#include        <sys/nodepda.h>
#include	<sys/SN/error.h>
#include 	"SN/error_private.h"
#include 	<sys/mapped_kernel.h>
#include 	<sys/SN/SN0/arch.h>

#endif

typedef	struct cef {
    eframe_t	cef_eframe;		/* eframe */
    eccframe_t	cef_eccf;		/* ecc frame */
    eframe_t	cef_panic;		/* eframe at time of panic */
#   define 	STACK_SIZE (ECCF_STACK_SIZE-(2 * sizeof(eframe_t)) - \
			    sizeof(eccframe_t))
    char	cef_stack[STACK_SIZE];
    cpuid_t	errhandcpu;
} cef_t;

#if EVEREST

cef_t **cacheErr_frames;

#elif IP28 || IP30 || IP32

cef_t *cacheErr_frames[MAXCPUS];

#endif

struct reg_values type_values[] = {
    0,	"Icache",
    1, 	"Dcache",
    2,	"Scache",
    3,	"Interface",
    0,	NULL
};

struct	reg_desc cache_desc[] = {	/* cache description */
    {CE_TYPE_MASK, 	-CE_TYPE_SHFT, "Type", NULL, type_values},
    {CE_EW, 		0, 	"EW", 	NULL, 	NULL},
    {CE_EE,		0,	"EE", 	NULL, 	NULL},
    {CE_D_WAY1,		0,	"D1",	NULL, 	NULL},
    {CE_D_WAY0,		0,	"D0",	NULL,	NULL},
    {CE_TA_WAY1,	0,	"TA1",	NULL,	NULL},	
    {CE_TA_WAY0,	0,	"TA[0]",NULL,	NULL},
    {CE_TS_WAY1,	0,	"TS[1]",NULL,	NULL},
    {CE_TS_WAY0,	0,	"TS[0]",NULL,	NULL},
    {CE_TM_WAY1,	0,	"TM[1]",NULL,	NULL},
    {CE_TM_WAY0,	0,	"TM[0]",NULL, 	NULL},
    {0,			0,	NULL,	NULL,	NULL}
};

struct	reg_desc interface_desc[] = {	/* interface description */
    {CE_TYPE_MASK, 	-CE_TYPE_SHFT, "Type", NULL, type_values},
    {CE_EW, 		0, 	"EW", 	NULL, 	NULL},
    {CE_EE,		0,	"EE", 	NULL, 	NULL},
    {CE_D_WAY1,		0,	"D1",	NULL, 	NULL},
    {CE_D_WAY0,		0,	"D0",	NULL,	NULL},
    {CE_SA,		0,	"SysAd",NULL,	NULL},
    {CE_SC,		0,	"SysCmd",NULL,	NULL},	
    {CE_SR,		0,	"SysRsp",NULL,	NULL},
    {0,			0,	NULL,	NULL,	NULL}
};
	

#if defined (SN)

/*
 * temporary disable the new cache error handler
 * this is from mtune/kernel

*
* 0x0001              enable new cache error handler
* 0x0002              diagnose problem (not yet supported)
* 0x0004              try to recover from cache error by
*                     killing user application (not yet supported)
* 0x0008              try to isolate CPU after recovered error
*                     to prevent subsequent fatal failures
*
cerr_flags            0x0
 
 */
int cerr_flags = 0;




/*
 * the flag declarations should be documented in mtune/kernel
 * for cerr_flags
 *
 * they must match the declaration in ml/hook_exc.c and
 * ml/SN/mp.c
 */
#define	CERR_FLAG_NEW_HANDLER			0x0001
#define	CERR_FLAG_RUN_DIAG			0x0002
#define	CERR_FLAG_RECOVER			0x0004
#define	CERR_FLAG_ISOLATE_FAILED_CPU		0x0008


#define	SN_ECCF(cpu)	(sn_eccframe_t *) (TO_NODE_UNCAC(cputonasid(cpu),\
				CACHE_ERR_ECCFRAME + 			 \
				(cputoslice(cpu) ? UALIAS_FLIP_BIT : 0)))

#define	LED_FLIP_COUNT		100000	/* loop count to flip LEDs. this
					 * is about 1 sec running uncached
					 * on a R12K 300 MHZ
					 */

#define	CERR_RETRIES		10	/* nr of retries to reserve error
					 * handling CPU
					 */
			

#define	CERR_ACTIVE		0x01
#define	CERR_WAIT_FOR_HANDLER   0x02	/* wait for handling CPU */
#define CERR_BADLINE_INVALIDATED  0x04	/* failing CPU was able to invalidate
					 * the failing cache line
					 */	
#define	CERR_PANIC		0x08	/* set for CPU which caused the panic */
#define	CERR_DIAG		0x10	/* run micro-diagnostics to nail down */
					/* the error			      */
#define	CERR_DIAG_FAILED	0x20	/* the requested diagnostics failed   */
#define	CERR_RECOVER		0x40
#define	CERR_RECOVER_FAILED	0x80
#define CERR_NOWAY 		0x100	/* error indicator */
#define CERR_TAG   		0x200	/* error indicator */
#define	CERR_DATA		0x400	/* error indicator */

#define CERR_PENDING   		((__uint64_t) 1<<63)
					/* use the high bit, so we can or in
					 * the low bits to the LEDs to indicate
					 * status
					 */

/*
 * for idbg 
 */
struct {
	__uint64_t bit;
	char      *name;
} eccf_flg_names[] = {
	{ CERR_ACTIVE 	 	   , "CERR_ACTIVE" 		},
	{ CERR_WAIT_FOR_HANDLER    , "CERR_WAIT_FOR_HANDLER"	},
	{ CERR_BADLINE_INVALIDATED , "CERR_BADLINE_INVALIDATED"	},
	{ CERR_PANIC		   , "CERR_PANIC",		},
	{ CERR_DIAG		   , "CERR_DIAG",		},
	{ CERR_DIAG_FAILED	   , "CERR_DIAG_FAILED",	},
	{ CERR_RECOVER		   , "CERR_RECOVER",		},
	{ CERR_RECOVER_FAILED	   , "CERR_RECOVER_FAILED"	},
	{ CERR_NOWAY		   , "CERR_NOWAY"		},
	{ CERR_TAG		   , "CERR_TAG"			},
	{ CERR_DATA		   , "CERR_DATA"		},
	{ CERR_PENDING		   , "CERR_PENDING"		},
	{ 0			   , NULL			},
};
#endif
    

/*
 * Macros: XLINE_ADDR
 * Purpose: Compute A address to index the cache from the cache error
 *	    register, including the LSB (way) bit.
 * Parameters: cer - cache error register.
 * Notes: THese macros currently assume that the least significant bits of
 *	  the PIDX/SIDX fields in the CER register are 0 if the line sizes
 *	  are larger than the minimum.
 * 
 */
#define	SLINE_WAY0	(CE_D_WAY0|CE_TA_WAY0)
#define	SLINE_WAY1	(CE_D_WAY1|CE_TA_WAY1)
#define	SLINE_ADDR(cer)	(((cer) & CE_SINDX_MASK) + K0BASE)

#define	ILINE_WAY0	(CE_TA_WAY0|CE_TS_WAY0|CE_D_WAY0)
#define	ILINE_WAY1	(CE_TA_WAY1|CE_TS_WAY1|CE_D_WAY1)
#define	ILINE_ADDR(cer)	(((cer) & CE_PINDX_MASK) + K0BASE)

#define	DLINE_WAY0	(CE_TA_WAY0|CE_TS_WAY0|CE_TM_WAY0|CE_D_WAY0)
#define	DLINE_WAY1	(CE_TA_WAY1|CE_TS_WAY1|CE_TM_WAY1|CE_D_WAY1)
#define	DLINE_ADDR(cer)	(((cer) & CE_PINDX_MASK) + K0BASE)

extern void cacheOP(cacheop_t *);

#if IP25
static	void
ecc_clearSCCTag(__uint64_t cer, int way)
/*
 * Function: 	ecc_clearSCCTag
 * Purpose:	To avoid coherence problems caused by an SCC chip bug, we must
 *		make sure that cache line just invalidated is NOT mark valid
 * 		in the SCC duplicate tags.
 * Parameters:	cer - cache error register - assumes SIE or SCACHE error.
 *		way - 0 for way 0, 1 for way 1.
 * Returns:	nothing
 */
{
    __uint32_t	idx;

    idx = (cer & CE_SINDX_MASK) >> 7;
    *(__uint64_t *)(EV_BUSTAG_BASE+(idx<<4)|(way ? EV_BTRAM_WAY : 0)) = 0;
}

#endif

static	__uint64_t 
ecc_cacheOp(__uint32_t op, __uint64_t addr)
/*
 * Function: 	ecc_cacheOp
 * Purpose:	Perform the specified cache operation and return the
 *		value remaining in the taghi/taglo registers as a 64-bit
 *		value.
 * Parameters:	op    - cahe operation to perform, including cache selector.
 *		addr  - address, either virtual or physical with low bit
 *		        indicating way.
 * Returns:	64-bit cache tag (taghi/taglo registers concatenated).
 */
{
    cacheop_t	cop;

    cop.cop_address 	= addr;
    cop.cop_operation  	= op;
    
    cacheOP(&cop);

    return(((__uint64_t)cop.cop_taghi << 32) | (__uint64_t)cop.cop_taglo);
}

/*
 * Function	: ecc_cacheop_get
 * Parameters	: type -> what operation do we want to perform.
 *		  addr -> address to perform operation upon.
 *		  cop  -> pointer to cacheop structure.
 * Purpose	: to perform the necessary cache op.
 * Assumptions	: None.
 * Returns	: None (cop will be filled with relevant details)
 */

void
ecc_cacheop_get(__uint32_t type, __psunsigned_t addr, cacheop_t *cop)
{
	extern	void	cacheOP(cacheop_t *);

	cop->cop_operation = type;
	cop->cop_address   = addr;

	cacheOP(cop);
	return;
}


static	void
ecc_display(int flags, char *s, __uint32_t cer, 
	    __uint64_t errorEPC, __uint64_t tag[])
/*
 * Function: ecc_display
 * Purpose: 	To print information about a cache error given the cache error
 *		register.
 * Parameters:	flags - cmn_err flags to add in.
 *		s - pointer to a string indicating reason.
 *		cer - cache error register value.
 *		tag - cache tag read that decision was based on.
 * Returns: nothing
 */
{
    static char template[] = "Cache Error (%s) %R errorEPC=0x%x tag=0x%x paddr=0x%x %s\n";
    char *tag_valid = "(Tag Valid)";
    char *tag_invalid = "(Tag Invalid)";
    int invalid, way;

    switch(cer & CE_TYPE_MASK) {
    case CE_TYPE_I:
	for (way = 0; way <= 1; way++) {
	    if (tag[way] == 0)
		continue;

	    invalid = ((tag[way] & CTP_STATE_MASK) == 0);
	    if (tag[way])
		cmn_err(flags, template, s, (__uint64_t)cer, cache_desc,
			errorEPC, tag[way], PCACHE_ERROR_ADDR(cer, tag[way]),
			invalid ? tag_invalid : tag_valid);
	}
	break;
    case CE_TYPE_D:
	for (way = 0; way <= 1; way++) {
	    if (tag[way] == 0)
		continue;
	    invalid = (((tag[way] & 
			 CTP_STATE_MASK) >> CTP_STATE_SHFT) == CTP_STATE_I);

	    cmn_err(flags,template,s,(__uint64_t)cer,cache_desc,errorEPC,
		    tag[way], PCACHE_ERROR_ADDR(cer, tag[way]),
		    invalid ? tag_invalid : tag_valid);
	}
	break;

    case CE_TYPE_S:
	for (way = 0; way <= 1; way++) {
	    if (tag[way] == 0)
		continue;
	    invalid = (((tag[way] & 
			 CTS_STATE_MASK) >> CTS_STATE_SHFT) == CTS_STATE_I);

	    cmn_err(flags,template,s,(__uint64_t)cer,cache_desc,errorEPC,
		    tag[way], SCACHE_ERROR_ADDR(cer, tag[way]),
		    invalid ? tag_invalid : tag_valid);
	}
	break;

    case CE_TYPE_SIE:
	for (way = 0; way <= 1; way++) {
	    if (tag[way] == 0)
		continue;
#ifndef SN0
	    cmn_err(flags,template,s,(__uint64_t)cer,interface_desc,errorEPC,
		    tag[way], SCACHE_ERROR_ADDR(cer, tag[way]), tag_valid);
#else /* SN0 */
	{
	    paddr_t paddr;
	    int dimm;
	    int cnode;

	    paddr = SCACHE_ERROR_ADDR(cer, tag[way]);
	    dimm = (paddr & MEM_DIMM_MASK) >> MEM_DIMM_SHFT;
	    cnode = NASID_TO_COMPACT_NODEID(NASID_GET(paddr));
	    cmn_err(flags, 
		    "Interface Error. Suspect MEMORY BANK %d on %s\n",
		    dimm, (cnode == INVALID_CNODEID) ? "a remote partition" :
		    NODEPDA(cnode)->hwg_node_name);

	    if (error_page_discard(paddr, PERMANENT_ERROR,UNCORRECTABLE_ERROR))
		cmn_err(CE_NOTE, "Recovered from memory error by discarding the page");
	}
#endif
	}
	break;    
    }
}    

static void
ecc_cleanRegion(void *p, int l)
/*
 * Function: ecc_cleanRegion
 * Purpose:  To be sure a specified region is not in the cache.
 * Parameters:  p - address of base of region.
 *		l - length in bytes of region
 * Returns: nothing
 * Notes: Cleans multiples of line.
 */
{
    __uint64_t	pp = (__uint64_t)p;

    while (l > 0) {
	ecc_cacheOp(CACH_S+C_HWBINV, pp);
#if IP25
	/*
	 * Doing a SCACHE invalidate on IP25 can cause a problem in the 
	 * CC chip. It can cause a lose of coherence on the cache line 
	 * that is invalidated - if it gets pulled back and placed in 
	 * the other "way". It is not a problem here because the region
	 * we are cleaning should not be modified cached.
	 */
#endif
	pp += CACHE_SLINE_SIZE;
	l -= CACHE_SLINE_SIZE;
    }
}

void
ecc_init(void)
/*
 * Function: ecc_init
 * Purpose: Set up stacks etc for ecc handler for THIS cpu.
 * Parameters: 	none
 * Returns:	nothing
 * Notes: For EVEREST, we use the physical CPUID to index into the array.
 *	  It is easiest in locore.
 */
{
    /* REFERENCED */
    uint	physid;
    /* REFERENCED */
    __uint64_t	a;

#if EVEREST
    physid = EV_GET_LOCAL(EV_SPNUM) & EV_SPNUM_MASK;
#elif IP28 || IP32
    physid = 0;
#elif IP30
    physid = cpuid();
#endif

#if EVEREST
    if (!EVERROR_EXT->eex_ecc) {
	a = (__uint64_t)kmem_zalloc(sizeof(eccframe_t *) * EV_SPNUM_MASK, 
				   VM_CACHEALIGN | VM_DIRECT);
	if (!a) {
	    cmn_err(CE_PANIC,
		    "Unable to allocate memory for cache error handler");
	}
	ecc_cleanRegion((void *)a, sizeof(eccframe_t *) * EV_SPNUM_MASK);
	cacheErr_frames = (void *)K0_TO_K1(a);
	ecc_cleanRegion(&cacheErr_frames, sizeof(cacheErr_frames));
	EVERROR_EXT->eex_ecc = (cef_t **)K0_TO_K1(a);
    }

    a = (__uint64_t)kmem_zalloc(ECCF_STACK_SIZE, VM_CACHEALIGN | VM_DIRECT);
    if (!a) { 
	cmn_err(CE_PANIC,"Unable to allocate ECCF");
    }
    ecc_cleanRegion((void *)a, ECCF_STACK_SIZE);

    ((cef_t **)EVERROR_EXT->eex_ecc)[physid] = (cef_t *)K0_TO_K1(a);
#elif IP28 || IP32
    a = (__uint64_t)kmem_zalloc(ECCF_STACK_SIZE,VM_CACHEALIGN|VM_DIRECT);
    if (!a) {
	cmn_err(CE_PANIC,"Unable to allocate ECCF");
    }

    /* make sure low handler and real memory base are consistant */
    ecc_cleanRegion((void *)a, sizeof(struct cef));

    cacheErr_frames[physid] = (cef_t *)K0_TO_K1(a);
    ecc_cleanRegion(&cacheErr_frames, sizeof(cacheErr_frames));
#elif IP30
    a = (__uint64_t)kmem_zalloc(ECCF_STACK_SIZE, VM_CACHEALIGN|VM_DIRECT);
    if (!a) { 
	cmn_err(CE_PANIC,"Unable to allocate ECCF");
    }
    ecc_cleanRegion((void *)a, ECCF_STACK_SIZE);

    cacheErr_frames[physid] = (cef_t *)K0_TO_K1(a);
    *(volatile __psunsigned_t *)PHYS_TO_K1(CACHE_ERR_FRAMEPTR + (physid<<3)) =
			K0_TO_K1(a);
#elif SN0

#if defined (HUB_ERR_STS_WAR)
   if (WAR_ERR_STS_ENABLED) {
	    
	__uint64_t start_off, end_off;
	/*
	 * Allocate cache_err stack and store it in the CACHE_ERR_SP_PTR.
	 */
	a = (__uint64_t)kmem_zalloc(2 * ECCF_STACK_SIZE, VM_CACHEALIGN | VM_DIRECT);
	start_off = (__psunsigned_t)a & 0x3ffff;
	end_off = (__psunsigned_t)(a + ECCF_STACK_SIZE) & 0x3ffff;
	if (!a) { 
		return;
	}

	if ((start_off < 0x480)  || 
	    ((end_off > 0x400) && (end_off < ECCF_STACK_SIZE))) 
		a += ECCF_STACK_SIZE;
    } else
#endif /* HUB_ERR_STS_WAR */
    {
	/*
	 * Allocate cache_err stack and store it in the CACHE_ERR_SP_PTR.
	 */
	 a = (__uint64_t)kmem_zalloc(ECCF_STACK_SIZE, VM_CACHEALIGN | VM_DIRECT);
	if (!a) { 
	    cmn_err(CE_PANIC,"Unable to allocate ECCF");
	    return;
	}
    }

    ecc_cleanRegion((void *)a, ECCF_STACK_SIZE);
    *(__psunsigned_t *)(UALIAS_BASE + CACHE_ERR_SP_PTR) = 
				      K0_TO_K1(a + ECCF_STACK_SIZE - 0x10);
    *(__psunsigned_t *)(UALIAS_BASE + CACHE_ERR_IBASE_PTR) = 
	                              PHYS_TO_K1(TO_PHYS(MAPPED_KERN_RO_PHYSBASE(cnodeid())));
#endif
}

#if IP28 || IP32
/* use a constant for early ECC errors until malloc works */
void
early_ecc_init(char *addr)
{
	cacheErr_frames[0] = (cef_t *)addr;
}
#endif

#if defined (EVEREST) || defined (SN0)
/* ARGSUSED */
int
eccf_recoverable_threshold(eframe_t *ef, eccframe_t *eccf, uint cer)
{
	k_machreg_t lapse;
	k_machreg_t currtc = GET_LOCAL_RTC;
	
	ushort *cnt_p;

	switch(cer & CE_TYPE_MASK) {
	case CE_TYPE_I:
		cnt_p = &(eccf->ecct_recover_icache_count);
		break;
	case CE_TYPE_D:
		cnt_p = &(eccf->ecct_recover_dcache_count);
		break;
	case CE_TYPE_S:
		cnt_p = &(eccf->ecct_recover_scache_count);
		break;
	case CE_TYPE_SIE:
		cnt_p = &(eccf->ecct_recover_sie_count);
		break;
	default:
		return 1;
	}
	
	if (eccf->ecct_recover_rtc) {
		lapse = currtc - eccf->ecct_recover_rtc;
		if (lapse < CERR_RECOVER_TIME) {
			if (++(*cnt_p) > CERR_RECOVER_COUNT)
			    return 0;
			else
			    return 1;
		}
	}
	eccf->ecct_recover_rtc = currtc;
	*cnt_p = 0;
	
	return 1;
}
#endif /* defined (EVEREST) || defined (SN0) */


void
ecc_force_cache_line_invalid(__psunsigned_t addr, int way, int type)
{
	cacheop_t	cop;

	cop.cop_taglo = cop.cop_taghi = cop.cop_ecc = 0;

	cop.cop_address = addr | way;
	cop.cop_operation = type + C_IST;
	cacheOP(&cop);
	cop.cop_operation = type + C_ISD;
	cacheOP(&cop);
}    

/*ARGSUSED1*/
char *
ecc_handler(eframe_t *ef, eccframe_t *eccf)
/*
 * Function: 	ecc_handler
 * Purpose:	To process an r10k cache error exception.
 * Parameters:	Cache error control block for this CPU.
 */
{
    __uint64_t	cer;			/* u64 so not sign extended */
    __uint64_t	t;			/* tag */
    __uint64_t	addr;			/* address temp */
    __uint64_t	way;			/* way 0 or 1 */
    int		idx;
    char	*result = NULL;

#if defined (CACHE_DEBUG)
    extern int  r10k_halt_1st_cache_err;
    extern int  r10k_cache_debug;
    extern void qprintf(char *f, ...);
#endif /* CACHE_DEBUG */

#if defined (SN0)
    int		err_save = 0;

#if defined (CACHE_DEBUG)
#pragma weak icache_save
    extern void icache_save(int);
#pragma weak dcache_save
    extern void dcache_save(int);
#pragma weak scache_save
    extern void scache_save(int);
#endif /* CACHE_DEBUG */
#endif /* SN0 */

    int 	limit_exceeded = 0;

    cer = (__uint64_t)eccf->eccf_cache_err;

#if defined (CACHE_DEBUG)
#if defined (SN0)
    if (r10k_halt_1st_cache_err || r10k_cache_debug) {
#pragma	weak	print_cacherr
	  extern void print_cacherr(__uint32_t);

	if (print_cacherr) {
	  print_cacherr(cer);
	}
    }

    if (r10k_cache_debug) {
	  int cpu = cpuid();
	
	if (icache_save) {
	  icache_save(cpu);
	}
	if (dcache_save) {
	  dcache_save(cpu);
	}
	if (scache_save) {
	  scache_save(cpu);
	}

	  qprintf("saved cache state on CPU %d after ECC error\n",cpu);
    }
#else 
    if (r10k_halt_1st_cache_err)
          qprintf("Cache Error register 0x%x\n",cer);
#endif /* SN0 */

    if (r10k_halt_1st_cache_err)
           cmn_err(CE_PANIC,"Forced panic on first cache error\n");

#endif /* CACHE_DEBUG */

    /* Check if we can trace the current exception */

    if (ECCF_ADD(eccf->eccf_trace_idx,1) != eccf->eccf_putbuf_idx) {
	idx = eccf->eccf_trace_idx;

	/*
         * make sure that the index is always within array bounds.
	 * no reason why it should not be, but this was not initialized
	 * anywhere and the kernel would take an exception accessing some
	 * random index into the array. That problem has been fixed, but 
	 * this is an additional safegaurd.
         */
	if (idx > ECCF_TRACE_CNT)
	    idx = idx % ECCF_TRACE_CNT;

	eccf->eccf_trace[idx].ecct_cer = (__uint32_t)cer;
	eccf->eccf_trace[idx].ecct_errepc  = eccf->eccf_errorEPC;
	eccf->eccf_trace_idx = ECCF_ADD(eccf->eccf_trace_idx, 1);
#if defined (EVEREST) || defined (SN0)
	eccf->eccf_trace[idx].ecct_rtc = GET_LOCAL_RTC;
#endif /* EVEREST || SN0 */
    } else {
	idx = -1;			/* no tracing this time */
    }
#if defined (EVEREST) || defined (SN0)
    if (!ECCF_RECOVERABLE_THRESHOLD(ef, eccf, cer))
	limit_exceeded = 1;
#endif /* EVEREST || SN0 */

	eccf->eccf_tag[0] = eccf->eccf_tag[1] = 0;

	switch(cer & CE_TYPE_MASK) {
	case CE_TYPE_I:
#if defined (SN0)
	    err_save = 1;
#endif
	    eccf->eccf_icount++;
	    /*
	     * For Icache, we can just invalidate both ways, and continue 
	     * running.
	     */
	    addr = ILINE_ADDR(cer);
	    for (way = 0; way <= 1; way++) {
		    __uint32_t way_mask = (way) ? DLINE_WAY1 : DLINE_WAY0;
		    if ((cer & way_mask) == 0)
			continue;
		    t = ecc_cacheOp(CACH_PI + C_ILT, addr + way);
		    eccf->eccf_tag[way] = t;
	    }

	    (void)ecc_cacheOp(CACH_PI+C_IINV, addr); /* Invalidate and continue */
	    (void)ecc_cacheOp(CACH_PI+C_IINV, addr^1);/* Invalidate and continue */

	    ecc_force_cache_line_invalid(addr, 0, CACH_PI);
	    ecc_force_cache_line_invalid(addr, 1, CACH_PI);
	    if (limit_exceeded)
		result = "too many primary instruction cache error exceptions";
	    break;

    case CE_TYPE_D:
#if defined (SN0)
	err_save = 1;
#endif
	eccf->eccf_dcount++;
	addr = DLINE_ADDR(cer);

	if (!(cer & DLINE_WAY0) && !(cer & DLINE_WAY1)) {
	    if (cer & CE_EE) {
		/*
		 * Tag error on inconsistent block. * SHOULD NOT HIT THIS *
		 */
		result = "unrecoverable - dcache tag on inconsistent block";
		break;
	    }
	    result = "unrecoverable dcache error, neither way";
	    break;
	}

	for (way = 0; way <= 1; way++) {
	    __uint32_t way_mask = (way) ? DLINE_WAY1 : DLINE_WAY0;

	    if ((cer & way_mask) == 0)
		continue;

	    t = ecc_cacheOp(CACH_PD+C_ILT, addr + way);
	    eccf->eccf_tag[way] = t;

	    if (cer & CE_TM_MASK) {
		/*
		 * If a tag mod array error occured, we can not use the 
		 * STATEMOD to figure out if we can recover. In this case, 
		 * all we can do is invalidate the line if it is clean, 
		 * and panic if it is dirty.
		 */
		if (((t & CTP_STATE_MASK) >> CTP_STATE_SHFT) == CTP_STATE_DE) {
		    result = "unrecoverable - dcache tag mod";
		    break;	
		}
	    } 
	    if (cer & (CE_TA_MASK + CE_TS_MASK)) {
		/*
		 * Tag array/State array error are recoverable if line 
		 * is consistent with Scache.
		 */
		if (((t & CTP_STATEMOD_MASK) >> CTP_STATEMOD_SHFT) 
		                                       == CTP_STATEMOD_I) {
		    result = "unrecoverable - dcache tag";
		    break;	
		} 
	    } 
	    if (cer & CE_D_MASK) {
		if (((t & CTP_STATE_MASK) >> CTP_STATE_SHFT) == CTP_STATE_DE) {
		    /* 
		     * If dirty, panic - if not, invalidate and continue 
		     */
		    if (((t & CTP_STATEMOD_MASK) >> CTP_STATEMOD_SHFT) 
			                                == CTP_STATEMOD_I) {
			result = "unrecoverable - dcache data";
			break;
		    }
		} 
	    }

	    (void)ecc_cacheOp(CACH_PD+C_IINV, addr+way);
	}
	if (cer & CE_EE) {
	    /*
	     * Tag error on inconsistent block. * SHOULD NOT HIT THIS *
	     */
	    result = "unrecoverable - dcache tag on inconsistent block";
	    break;
	}
        if (limit_exceeded)
	   result = "too many primary data cache error exceptions";

	break;

    case CE_TYPE_S:
	eccf->eccf_scount++;
	addr = SLINE_ADDR(cer);

	if (!(cer & SLINE_WAY0) && !(cer & SLINE_WAY1)) {
#if defined (SN0)
		err_save = 1;
#endif
		result = "unrecoverable scache error, neither way";
		break;
	}

	for (way = 0; way <= 1; way++) {
	    __uint32_t way_mask = (way) ? SLINE_WAY1 : SLINE_WAY0;
	    
	    if ((cer & way_mask) == 0)
		continue;
	    
	    t = ecc_cacheOp(CACH_S + C_ILT, addr + way);
	    eccf->eccf_tag[way] = t;

	    if (((t & CTS_STATE_MASK) >> CTS_STATE_SHFT) == CTS_STATE_I) {
#if defined (CACHE_ERR_DEBUG)
		cmn_err(CE_WARN | CE_CPUID, "Scache error on invalid line");
		ecc_scache_line_dump(addr, way, printf);
#endif /* CACHE_ERR_DEBUG */

		continue;
	    }
#if defined (SN0)
	    err_save = 1;
#endif
	    if (cer & CE_TA_MASK) {
		result = "unrecoverable - scache tag";
		break;
	    } 
	    /*
	     * For now, turn this on only for SN0.
	     */

#if defined (SN0)
	{
	    int syn_check;
	    syn_check = ecc_scache_line_check_syndrome(addr, way);
	    if (syn_check > 1) {
#if defined (CACHE_ERR_DEBUG)
		cmn_err(CE_WARN | CE_CPUID, "Scache error: bad syndrome");
		ecc_scache_line_dump(addr, way, printf);
#endif /* CACHE_ERR_DEBUG */
	    }
	    else {
		result = "Transient scache error";
		break;
	    }
	}
#endif /* SN0 */
	    
	    if (((t & CTS_STATE_MASK) >> CTS_STATE_SHFT) == CTS_STATE_DE) {
		result = "unrecoverable scache data";
		break;
	    }
	    (void)ecc_cacheOp(CACH_S + C_IINV, addr + way);
#if IP25
	    ecc_clearSCCTag(addr, way);
#endif
	}
        if (limit_exceeded)
	   result = "too many secondary cache error exceptions";

	break;

    case CE_TYPE_SIE:
#if defined (SN0)
	err_save = 1;
#endif
	eccf->eccf_sicount++;
	addr = SLINE_ADDR(cer);

#if IP30
	if (cer & (CE_SA | CE_EE)) {
		static char	err_buf[160];
		heartreg_t	h_cause;
		heartreg_t	h_memerr_addr;
		heart_piu_t	*heart;
		extern char	*maddr_to_dimm(paddr_t);

		heart = HEART_PIU_K1PTR;
		h_cause = heart->h_cause;
		if (h_cause & HC_NCOR_MEM_ERR) {
			h_memerr_addr = heart->h_memerr_addr & HME_PHYS_ADDR;
			if ((h_memerr_addr & CE_SINDX_MASK) ==
			    (cer & CE_SINDX_MASK)) {
				sprintf(err_buf,
					"unrecoverable SIE caused by multi bit "
					"ECC error at physaddr 0x%x, DIMM %s",
					h_memerr_addr,
					maddr_to_dimm(h_memerr_addr));
				result = err_buf;
				break;
			}
		}
	}
#endif	/* IP30 */

	if (cer & (CE_SA | CE_SC | CE_SR)) {
		result = "unrecoverable SIE";
		break;
	}

	if (!(cer & CE_D_WAY0) && !(cer & CE_D_WAY1)) {
		result = "unrecoverable SIE, neither way";
		break;
	}

	for (way = 0; way <= 1; way++) {
	    __uint32_t way_mask = (way) ? CE_D_WAY1 : CE_D_WAY0;

	    if ((cer & way_mask) == 0)
		continue;

	    t = ecc_cacheOp(CACH_S + C_ILT, addr + way);
	    eccf->eccf_tag[way] = t;

	    if (((t & CTS_STATE_MASK) >> CTS_STATE_SHFT) == CTS_STATE_I) {
		continue;
	    }
	    
#if defined (SN0)
	    if (memerror_handle_uce(cer, t, ef, eccf)) {
		    (void)ecc_cacheOp(CACH_S + C_IINV, addr + way);
		    ecc_force_cache_line_invalid(addr, way, CACH_S);
		    continue;
	    }
#endif /* SN0 */	    

	    if (((t & CTS_STATE_MASK) >> CTS_STATE_SHFT) == CTS_STATE_DE) {
		result = "unrecoverable SIE - dirty scache";
		break;			/* for */
	    } 
	    (void)ecc_cacheOp(CACH_S + C_IINV, addr + way);
#if IP25
	    ecc_clearSCCTag(addr, way);
#endif
	    ecc_force_cache_line_invalid(addr, way, CACH_S);
	}

        if (limit_exceeded) {
#if defined (SN0) 
	{
	    cnodeid_t cnode = NASID_TO_COMPACT_NODEID(NASID_GET(addr));
	    cmn_err(CE_WARN, "Unrecoverable Interface error: Suspect memory address 0x%x At %s Bank %d", addr, (cnode == INVALID_CNODEID) ? 
		    "a remote partition" : NODEPDA(cnode)->hwg_node_name,
		    (addr & MEM_DIMM_MASK) >> MEM_DIMM_SHFT);
	}
#endif
	    result = "too many interface error exceptions.";
	}
	
	break;    

    default:
	result = "unrecoverable - unknown cache error";
	break;
    }

    if (idx >= 0) {
	eccf->eccf_trace[idx].ecct_tag[0]  = eccf->eccf_tag[0];
	eccf->eccf_trace[idx].ecct_tag[1]  = eccf->eccf_tag[1];
    }

    private.p_cacherr = 1;

    if (result) {
	/*
	 * If we are going to panic, and we are not already panicing 
	 * (which can occur if we get into symmon and then a symmon 
	 * command causes another cache error), we save the eframe
	 * and update the status.
	 */
	if (eccf->eccf_status != ECCF_STATUS_PANIC) {
	    eccf->eccf_status = ECCF_STATUS_PANIC;
	    ((cef_t *)ef)->cef_panic = *ef;
	}
    }
#if defined (SN0)
    if (err_save)
	    save_cache_error(cer, eccf->eccf_tag[0], eccf->eccf_tag[1]);
#endif

#if IP28 || IP30 || IP32
    /* Ensure we are consistant wrt to the cache in the face of
     * speculation that could touch our data.
     */
     for (idx = 0 ; idx < sizeof(struct cef); idx += CACHE_SLINE_SIZE) {
	ecc_cacheOp(CACH_S|C_HINV,PHYS_TO_K0((__psunsigned_t)eccf+idx));
     }
#endif
    return(result);
}



void
ecc_panic(char *s, eframe_t *ef, eccframe_t *e)
/*
 * Function: 	ecc_panic
 * Purpose:	Panic on a non-recoverable cache error.
 * Parameters:	s - panic string to print.
 *		ef - pointer to current eframe.
 *		s - pointer to current ecc frame.
 * Returns:	Does not return, PANICS
 */

{
#if defined (SN0)
    machine_error_dump(s);
#endif
    ecc_display(CE_PHYSID+CE_WARN, s, 
		e->eccf_cache_err,e->eccf_errorEPC, e->eccf_tag);
    cmn_err(CE_PHYSID+CE_PANIC, "Cache Error (%s) Eframe = 0x%x\n", s,
	    &((cef_t *)ef)->cef_panic);
}

void
ecc_log(void)
/*
 * Function: ecc_log
 * Purpose: To log any pending exceptions on THIS cpu only.
 */
{
#if EVEREST || IP28 || SN0 || IP32
    eccframe_t	*e;
    /* REFERENCED */
    uint	spnum;
    uint	i;

#if EVEREST
    spnum = EV_GET_LOCAL(EV_SPNUM);
#elif IP28 || IP32
    spnum = 0;
#endif

    /* 
     * Clear PDA cache error indication before checking, so that if a new
     * cache error arrives, we will call here again.
     */

    private.p_cacherr = 0;
#if !defined (SN0)    
    e = &cacheErr_frames[spnum]->cef_eccf;
#else
    e = (eccframe_t *)(UALIAS_BASE + CACHE_ERR_ECCFRAME);
#endif
    if (e->eccf_trace_idx == ECCF_ADD(e->eccf_putbuf_idx,ECCF_TRACE_CNT-1)){
	cmn_err(CE_WARN+CE_PHYSID, 
		"Some recoverable cache errors may not have been logged\n");
    }
    for (i = e->eccf_putbuf_idx; i != e->eccf_trace_idx; i = ECCF_ADD(i, 1)) {
	ecc_display(CE_WARN | CE_PHYSID | CE_TOOKACTIONS, "recoverable",
		    e->eccf_trace[i].ecct_cer, 
		    e->eccf_trace[i].ecct_errepc,
		    e->eccf_trace[i].ecct_tag);
    }
    e->eccf_putbuf_idx = i;		/* mark updated pointer */
#endif
}


#if EVEREST
void
ecc_pending(int slot, int slice)
/*
 * Function: 	ecc_pending
 * Purpose:	To display any recorded by un-displayed cache errors
 *		on the specified CPU.
 * Parameters:	slot - slot # of CPU
 *		slice - slice # of CPU
 * Returns: nothing
 */
{
    eccframe_t	*e;
    uint	i;

    e = &((cef_t **)EVERROR_EXT->eex_ecc)[(slot<<EV_SLOTNUM_SHFT)+
					  (slice<<EV_PROCNUM_SHFT)]->cef_eccf;

    if (e->eccf_trace_idx != e->eccf_trace_idx) {
	cmn_err(CE_WARN, "CPU %d: Pending cache errors\n", 
		EVCFG_CPUID(slot, slice));
	
	for (i = e->eccf_putbuf_idx;i!=e->eccf_trace_idx;i=ECCF_ADD(i, 1)) {
	    ecc_display(0, "recovered", e->eccf_trace[i].ecct_cer, 
			e->eccf_trace[i].ecct_errepc,
			e->eccf_trace[i].ecct_tag);
	}
    }
}
#endif /* EVEREST */

#if EVEREST || IP28 || IP30 || IP32 || SN
void
ecc_printPending(void (*printf)(char *, ...))
/*
 * Function: 	ecc_printPending
 * Purpose:	To dump pending cache errors
 * Parameters:	printf - pointer to printf style routine
 *			used to print out the results.
 * Notes: This routine called from symmon to dump the
 *	   cache error logs.
 */
{
    eccframe_t	*e;
    int		i, cpu;

#if EVEREST
    for (cpu = 0; cpu < EV_SPNUM_MASK; cpu++) {
	if (cacheErr_frames[cpu]) {
	    e = &cacheErr_frames[cpu]->cef_eccf;
	    printf("CPU [0x%x/0x%x]: Total Errors ic(%d) dc(%d) sc(%d) in(%d)\n",	
		   (cpu & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT, 
		   (cpu & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT,
#elif IP28 || IP30 || IP32
    for (cpu = 0; cpu < MAXCPUS; cpu++) {
	if (cacheErr_frames[cpu]) {
	    e = &cacheErr_frames[cpu]->cef_eccf;
	    printf("Total Errors [cpu %d] ic(%d) dc(%d) sc(%d) in(%d)\n",
		   cpu,
#elif SN

	{
	extern int cerr_flags;

	if (cerr_flags & CERR_FLAG_NEW_HANDLER) {
		int pending_errcnt = 0;

		for (cpu = 0; cpu < MAXCPUS; cpu++) {
			sn_eccframe_t *sn_eccf;

			if (pdaindr[cpu].pda == NULL) 
				continue;

			sn_eccf= SN_ECCF(cpu);

			printf("CPU %d ECCFrame @0x%x cpu=%d hand=%d\n",
				cpu,
				sn_eccf,	
				sn_eccf->sn_eccf_fail_cpu,
				sn_eccf->sn_eccf_hand_cpu);

			printf("\tflags @0x%x = 0x%x\n",
				 sn_eccf->sn_eccf_flags,
				*sn_eccf->sn_eccf_flags);
			if (*sn_eccf->sn_eccf_flags) {
				__uint64_t i,j,bit;

				printf("\t      ");
				
				for (i = 0 ; i <= 63 ; i++) {
					bit = (__uint64_t) 1 << i;

					if (*sn_eccf->sn_eccf_flags & bit) {
						for (j = 0 ; eccf_flg_names[j].name != NULL ; j++) {
							if (eccf_flg_names[j].bit == bit) {
								printf("%s ",eccf_flg_names[j].name);
								break;
							}
							if (eccf_flg_names[j].name == NULL) 
								printf("[0x%x ?] ",bit);
						}
					}
				}
				printf("\n");
			}


                	if (*sn_eccf->sn_eccf_flags & CERR_PENDING)
                        	pending_errcnt++;

			if (sn_eccf->sn_eccf_cnt[R10K_CACHERR_PIC  ] ||
			    sn_eccf->sn_eccf_cnt[R10K_CACHERR_PDC  ] ||
			    sn_eccf->sn_eccf_cnt[R10K_CACHERR_SC   ] ||
			    sn_eccf->sn_eccf_cnt[R10K_CACHERR_SYSAD]) {

				printf(
					"\tTotal Errors : [ic %d] [dc %d] [sc %d] [sysad %d]\n",
					sn_eccf->sn_eccf_cnt[R10K_CACHERR_PIC  ],
					sn_eccf->sn_eccf_cnt[R10K_CACHERR_PDC  ],
					sn_eccf->sn_eccf_cnt[R10K_CACHERR_SC   ],
					sn_eccf->sn_eccf_cnt[R10K_CACHERR_SYSAD]);
			}

			if (sn_eccf->sn_eccf_cnt_recov_i[R10K_CACHERR_PIC  ] ||
			    sn_eccf->sn_eccf_cnt_recov_i[R10K_CACHERR_PDC  ] ||
			    sn_eccf->sn_eccf_cnt_recov_i[R10K_CACHERR_SC   ] ||
			    sn_eccf->sn_eccf_cnt_recov_i[R10K_CACHERR_SYSAD]) {
				printf("\tRecovered Errors by invalidating cacheline :\n");
				printf("\t\t[ic %d] [dc %d] [sc %d] [sysad %d]\n",
					sn_eccf->sn_eccf_cnt_recov_i[R10K_CACHERR_PIC  ],
					sn_eccf->sn_eccf_cnt_recov_i[R10K_CACHERR_PDC  ],
					sn_eccf->sn_eccf_cnt_recov_i[R10K_CACHERR_SC   ],
					sn_eccf->sn_eccf_cnt_recov_i[R10K_CACHERR_SYSAD]);
			}

			if (sn_eccf->sn_eccf_cnt_recov_k[R10K_CACHERR_PIC  ] ||
			    sn_eccf->sn_eccf_cnt_recov_k[R10K_CACHERR_PDC  ] ||
			    sn_eccf->sn_eccf_cnt_recov_k[R10K_CACHERR_SC   ] ||
			    sn_eccf->sn_eccf_cnt_recov_k[R10K_CACHERR_SYSAD] ) {
				
				printf("\tRecovered Errors by killing application :\n");
				printf("\t\t[ic %d] [dc %d] [sc %d] [sysad %d]\n",
					sn_eccf->sn_eccf_cnt_recov_k[R10K_CACHERR_PIC  ],
					sn_eccf->sn_eccf_cnt_recov_k[R10K_CACHERR_PDC  ],
					sn_eccf->sn_eccf_cnt_recov_k[R10K_CACHERR_SC   ],
					sn_eccf->sn_eccf_cnt_recov_k[R10K_CACHERR_SYSAD]);
			}

#if 0
			printf("\tSingle Bit Errors :\n");
			printf("\t\tNOT YET IMPLEMENETED\n");

#endif
			printf("\n");
					
		}

        	if (pending_errcnt) {
			sn_eccframe_t *sn_eccf;

                	printf("CPUs with pending Cache Errors :  ");

                	for (cpu = 0 ; cpu < maxcpus ; cpu++) {
                        	if (pdaindr[cpu].pda == NULL)
                                	continue;

                        	sn_eccf = SN_ECCF(cpu);
                        	if (*sn_eccf->sn_eccf_flags & CERR_PENDING) 
					printf("%d ",cpu);
                        }
			printf("\n");
                }

		return;
	}

	}

    for (cpu = 0; cpu < MAXCPUS; cpu++) {
	nasid_t nasid;
	paddr_t ualias_offset;

	if (pdaindr[cpu].pda != NULL) {
	    ualias_offset  = cputoslice(cpu) ? UALIAS_SIZE : 0;
	    nasid = cputonasid(cpu);
	    e = (eccframe_t *)
		TO_NODE_UNCAC(nasid, (ualias_offset + CACHE_ERR_ECCFRAME));
	    printf("Exception frame @0x%x\n",e);
	    printf("Total Errors [cpu %d] ic(%d) dc(%d) sc(%d) in(%d)\n",
		   cpu,
#endif
		   e->eccf_icount, e->eccf_dcount, 
		   e->eccf_scount, e->eccf_sicount);

	    for (i = e->eccf_putbuf_idx;
		 i != e->eccf_trace_idx; i = ECCF_ADD(i, 1)) {
		printf("\tcer=0x%x errorEPC=0x%x tag [0]=0x%x [1]=0x%x\n",
		       (__uint64_t)e->eccf_trace[i].ecct_cer, 
		       (__uint64_t)e->eccf_trace[i].ecct_errepc, 
		       e->eccf_trace[i].ecct_tag[0], 
		       e->eccf_trace[i].ecct_tag[1]);
	    } 
	}
    }
}
#endif /* EVEREST || IP28 || IP30 || IP32 || SN*/

int
ecc_bitcount(__uint64_t wd)
{
    int numbits = 0;

    while (wd) {
	wd &= (wd - 1);	   /* This has the effect of resetting 
			    * the rightmost set bit */
	numbits++;
    }
    return numbits;
}




struct scache_data_ecc {
        __uint64_t e_mask;      /* check bit      */
        __uint64_t d_emaskhi;   /* high data word */
        __uint64_t d_emasklo;   /* low  data word */
};


struct scache_data_ecc scache_data_ecc[SCDATA_ECC_BITS] = {
        { 0x00 , 0xff03ffe306ff0721, 0X62d0b0d000202080 },
        { 0X80 , 0Xffff030fff0703b2, 0X100848c802909020 },
        { 0X40 , 0X83ffff030503ff5c, 0X808404c438484850 },
        { 0X20 , 0X42808012fcfbfc3f, 0Xfd6202228504c408 },
        { 0X10 , 0X214043ff808084ff, 0Xff210101ffc20284 },
        { 0X08 , 0X10232081444046bf, 0Xfc3fdf3f4c010142 },
        { 0X04 , 0X0a12121823202100, 0Xbaffc0a0d0ffffc1 },
        { 0X02 , 0X0409094413121008, 0X4dc0e0ffe0c0ffff },
        { 0X01 , 0X010404200b0d0b47, 0X04e0ff60c3ffc0ff },
};



struct sctag_ecc_mask {
	__uint64_t t_emask;
};


struct sctag_ecc_mask sctag_eccm[SCTAG_ECC_BITS] = {
	E7_6W_ST,
	E7_5W_ST,
	E7_4W_ST,
	E7_3W_ST,
	E7_2W_ST,
	E7_1W_ST,
	E7_0W_ST,
};



char *cache_valid_str[] = {
	"invalid",
	"shared",
	"clean-ex",
	"dirty-ex"
};
#define BYTESPERQUADWD	(sizeof(int) * 4)
#define BYTESPERDBLWD	(sizeof(int) * 2)
#define BYTESPERWD	(sizeof(int) * 1)


    

/*
 * Function	: ecc_compute_sctag_ecc
 * Parameters	: tag -> the tag to compute ecc on.
 * Purpose	: to generate the ecc of the secondary cache tag word.
 * Assumptions	: None. 
 * Returns	: Returns the ecc check_bits.
 */

uint
ecc_compute_sctag_ecc(__uint64_t tag)
{
	uint	check_bits = 0;
	int	i;	
	
	for (i = 0; i < SCTAG_ECC_BITS; i++) {
		check_bits <<= 1;
		if (ecc_bitcount(tag & sctag_eccm[i].t_emask) & 1)
			check_bits |= 1;
	}
	return check_bits;
}


/*
 * Function	: ecc_compute_scdata_ecc
 * Parameters	: data_hi -> the upper 64 bits of the data.
 *		  data_lo -> the lower 64 bits of the data.
 * Purpose	: to generate the ecc of the secondary cache data word.
 * Assumptions	: None. 
 * Returns	: Returns the ecc check_bits of the sc data.
 */

uint
ecc_compute_scdata_ecc(__uint64_t data_hi, __uint64_t data_lo)
{
    	uint	   check_bits;
    	int        i;				 
    	__uint64_t ecc_word1, ecc_word2; 

    	check_bits = 0;

    	for (i = 0; i < SCDATA_ECC_BITS; i++) {
		check_bits <<= 1;
		ecc_word1 = data_hi & scache_data_ecc[i].d_emaskhi;
		ecc_word2 = data_lo & scache_data_ecc[i].d_emasklo;
	
		/*
	 	 * Get the xor of the 2 words. Since all we need to know 
	 	 * is if number of bits are odd or even, the xor works 
	 	 * out pretty well in giving us one word with all the info
	 	 */
		 if ((ecc_bitcount(ecc_word1 ^ ecc_word2)) & 1)
	    		  check_bits |= 1;
    	}
    	if (ecc_bitcount(check_bits) & 1)
		check_bits |= SC_ECC_PARITY_MASK;

    	return check_bits;
}


void
ecc_scache_line_data(__psunsigned_t k0addr, int way, t5_cache_line_t *sc)
{
    cacheop_t cop;	
    __uint64_t c_tag, c_data;
    int i, j, s;
    paddr_t paddr;
    int idx;
	
    s = splhi();

    idx = (k0addr & ((private.p_scachesize >> 1) - 1));

    ecc_cacheop_get(CACH_SD + C_ILT, k0addr | way, &cop);
    c_tag =(((__uint64_t)cop.cop_taghi << 32) | (__uint64_t)cop.cop_taglo);
    sc->c_tag = c_tag;
    paddr = ((c_tag & CTS_TAG_MASK) << 4) | idx;
    sc->c_addr = PHYS_TO_K0(paddr);

    for (i = 0; i < SCACHE_LINE_FRAGMENTS; i++) {
	for (j = 0; j < 2; j++) {
	    ecc_cacheop_get(CACH_SD + C_ILD, k0addr | way, &cop);
	    c_data =(((__uint64_t)cop.cop_taglo << 32)  |
		     (__uint64_t)cop.cop_taghi);
	    sc->c_data.sc_bits[i].sc_data[j] = c_data;
	    k0addr += BYTESPERDBLWD;
	}
	sc->c_data.sc_bits[i].sc_ecc = cop.cop_ecc;
    }
    splx(s);
    sc->c_idx = idx / CACHE_SLINE_SIZE;
    sc->c_type = CACH_SD;
}


void
ecc_scache_line_get(__psunsigned_t k0addr, int way, t5_cache_line_t *sc)
{
    int i;

    ecc_scache_line_data(k0addr, way, sc);

    sc->c_state = (sc->c_tag & CTS_STATE_MASK) >> CTS_STATE_SHFT;
    sc->c_way = way;

    for (i = 0; i < SCACHE_LINE_FRAGMENTS; i++) {
	sc->c_data.sc_bits[i].sc_cb = 
	    ecc_compute_scdata_ecc(sc->c_data.sc_bits[i].sc_data[0],
				   sc->c_data.sc_bits[i].sc_data[1]);
	sc->c_data.sc_bits[i].sc_syn =  ((sc->c_data.sc_bits[i].sc_ecc) ^
					 (sc->c_data.sc_bits[i].sc_cb));
    }
}


ecc_scache_bits_in_error(__uint32_t syn)
{
    int count;

    if (syn == 0) return 0;

    count  = ecc_bitcount(syn);

    if ((count == 1) || (count == 3) || (count == 5))
	return 1;

    return 2;
}

int
ecc_scache_line_check_syndrome(__psunsigned_t k0addr, int way)
{
    t5_cache_line_t sc;
    int i;
    int size = private.p_scachesize;
    __uint32_t syn;
    int error, max_error = 0;

    if (k0addr <= (size / (CACHE_SLINE_SIZE * SCACHE_WAYS)))
	k0addr = PHYS_TO_K0(k0addr * CACHE_SLINE_SIZE);
    else
	k0addr = PHYS_TO_K0(K0_TO_PHYS(k0addr)) & CACHE_SLINE_MASK;

    ecc_scache_line_get(k0addr, way, &sc);
    for (i = 0; i < SCACHE_LINE_FRAGMENTS; i++) 
	if (syn = sc.c_data.sc_bits[i].sc_syn) {
	    error = ecc_scache_bits_in_error(syn & 0x1ff);
	    if (error > max_error) max_error = error;
	}

    return max_error;
}


void
ecc_scache_line_show(t5_cache_line_t *sc, void (*prf)(char *, ...))
{
    int i;

    (*prf)("Addr %16x tag  %16x (Tag ecc %1x) Vindx %1x MRU %1x\n",
	   sc->c_addr, sc->c_tag, sc->c_tag & CTS_ECC_MASK,
	   (sc->c_tag & CTS_VIDX_MASK) >> CTS_VIDX_SHFT,
	   ((sc->c_tag & CTS_MRU) == 1));


    (*prf)("Index %8x  Way %1d State %8s\n",
	   sc->c_idx, sc->c_way, cache_valid_str[sc->c_state]);

	       
    for (i = 0; i < SCACHE_LINE_FRAGMENTS; i++) {
	(*prf)("Off %2x Data %16x Data %16x CB %3x ECC %3x Syn %3x BitErr %1x\n",
	       i * 16, 
	       sc->c_data.sc_bits[i].sc_data[0],
	       sc->c_data.sc_bits[i].sc_data[1],
	       sc->c_data.sc_bits[i].sc_cb,
	       sc->c_data.sc_bits[i].sc_ecc,
	       sc->c_data.sc_bits[i].sc_syn,
	       ecc_scache_bits_in_error(sc->c_data.sc_bits[i].sc_syn & 0x1ff));
    }
    (*prf)("\n");
}


void
ecc_picache_line_data(__psunsigned_t k0addr, int way, t5_cache_line_t *pic)
{
    cacheop_t cop;	
    __uint64_t c_tag, c_data;
    int i, s;
    paddr_t paddr;
    int idx;
	
    s = splhi();

    idx = (k0addr & ((R10K_MAXPCACHESIZE >> 1) - 1));

    ecc_cacheop_get(CACH_PI + C_ILT, k0addr | way, &cop);
    c_tag =(((__uint64_t)cop.cop_taghi << 32) | (__uint64_t)cop.cop_taglo);
    pic->c_tag = c_tag;
    paddr = ((c_tag & CTP_TAG_MASK) << 4) | idx;
    pic->c_addr = PHYS_TO_K0(paddr);

    for (i = 0; i < PICACHE_LINE_FRAGMENTS; i++) {
	ecc_cacheop_get(CACH_PI + C_ILD, k0addr | way, &cop);
	c_data =(((__uint64_t)cop.cop_taghi << 32)  |
		 (__uint64_t)cop.cop_taglo);
	pic->c_data.pic_bits[i].pic_data = c_data;
	pic->c_data.pic_bits[i].pic_ecc = cop.cop_ecc;
	k0addr += BYTESPERWD;
    }
    splx(s);
    pic->c_idx = idx / CACHE_ILINE_SIZE;
    pic->c_type = CACH_PI;
}


void
ecc_picache_line_get(__psunsigned_t k0addr, int way, t5_cache_line_t *pic)
{
    int i, invalid;

    ecc_picache_line_data(k0addr, way, pic);

    invalid = ((pic->c_tag & CTP_STATE_MASK) == 0);

    pic->c_state = !invalid;
    pic->c_way = way;

    for (i = 0; i < PICACHE_LINE_FRAGMENTS; i++) {
	pic->c_data.pic_bits[i].pic_cb = 
	    ecc_bitcount(pic->c_data.pic_bits[i].pic_data) & 1;
	pic->c_data.pic_bits[i].pic_syn = 
	    (pic->c_data.pic_bits[i].pic_ecc ^
	     pic->c_data.pic_bits[i].pic_cb);
    }
}


int
ecc_picache_line_check_syndrome(__psunsigned_t k0addr, int way)
{
    t5_cache_line_t pic;
    int i;

    if (k0addr < R10K_ICACHE_LINES)
	k0addr = PHYS_TO_K0(k0addr * CACHE_ILINE_SIZE);
    else
	k0addr = PHYS_TO_K0(K0_TO_PHYS(k0addr)) & CACHE_ILINE_MASK;

    ecc_picache_line_get(k0addr, way, &pic);
    for (i = 0; i < PICACHE_LINE_FRAGMENTS; i++) 
	if (pic.c_data.pic_bits[i].pic_syn) break;

    return (i == PICACHE_LINE_FRAGMENTS) ? 0 : 1;
}

void
ecc_picache_line_show(t5_cache_line_t *pic, void (*prf)(char *, ...))
{
    int i;

    (*prf)("Addr %16x tag  %16x tagparity %1x\n",
	   pic->c_addr, pic->c_tag, pic->c_tag & 1);

    (*prf)("Index %8x  Way %1d valid %1d\n",
	   pic->c_idx, pic->c_way, pic->c_state);
	       
    for (i = 0; i < PICACHE_LINE_FRAGMENTS; i++) {
	(*prf)("Off %2x Data %9x CB %1x ECC %1x Syn %1x\n",
	       i * 4,
	       pic->c_data.pic_bits[i].pic_data,
	       pic->c_data.pic_bits[i].pic_cb,
	       pic->c_data.pic_bits[i].pic_ecc,
	       pic->c_data.pic_bits[i].pic_syn);
    }
    (*prf)("\n");
}


void
ecc_pdcache_line_data(__psunsigned_t k0addr, int way, t5_cache_line_t *pdc)
{
    cacheop_t cop;	
    __uint64_t c_tag;
    int i, s;
    paddr_t paddr;
    int idx;
	
    s = splhi();

    idx = (k0addr & ((R10K_MAXPCACHESIZE >> 1) - 1));
	
    ecc_cacheop_get(CACH_PD + C_ILT, k0addr | way, &cop);
    c_tag =(((__uint64_t)cop.cop_taghi << 32) | (__uint64_t)cop.cop_taglo);
    pdc->c_tag = c_tag;
    paddr = ((c_tag & CTP_TAG_MASK) << 4) | idx;
    pdc->c_addr = PHYS_TO_K0(paddr);

    for (i = 0; i < PDCACHE_LINE_FRAGMENTS; i++) {
	ecc_cacheop_get(CACH_PD + C_ILD, k0addr | way, &cop);
	pdc->c_data.pdc_bits[i].pdc_data = cop.cop_taglo;
	pdc->c_data.pdc_bits[i].pdc_ecc = cop.cop_ecc;
	k0addr += BYTESPERWD;
    }
    splx(s);
    pdc->c_idx = idx / CACHE_DLINE_SIZE;
    pdc->c_type = CACH_PD;
}




void
ecc_pdcache_line_get(__psunsigned_t k0addr, int way, t5_cache_line_t *pdc)
{
    int i, j, count;
    __uint32_t data;

    ecc_pdcache_line_data(k0addr, way, pdc);

    pdc->c_state = (pdc->c_tag & CTP_STATE_MASK) >> CTP_STATE_SHFT;
    pdc->c_way = way;

    for (i = 0; i < PDCACHE_LINE_FRAGMENTS; i++) {
	data = pdc->c_data.pdc_bits[i].pdc_data;
	pdc->c_data.pdc_bits[i].pdc_cb = 0;
	for (j = 0; j < 4; j++) {
	    count = ecc_bitcount(data & (0xff << (j * 8)));
	    pdc->c_data.pdc_bits[i].pdc_cb |= (count & 1) << j;
	}
	pdc->c_data.pdc_bits[i].pdc_syn = 
	    (pdc->c_data.pdc_bits[i].pdc_ecc ^
	     pdc->c_data.pdc_bits[i].pdc_cb);
    }
}


int
ecc_pdcache_line_check_syndrome(__psunsigned_t k0addr, int way)
{
    t5_cache_line_t pdc;
    int i;

    if (k0addr < R10K_DCACHE_LINES)
	k0addr = PHYS_TO_K0(k0addr * CACHE_DLINE_SIZE);
    else
	k0addr = PHYS_TO_K0(K0_TO_PHYS(k0addr)) & CACHE_DLINE_MASK;

    ecc_pdcache_line_get(k0addr, way, &pdc);

    for (i = 0; i < PDCACHE_LINE_FRAGMENTS; i++) 
	if (pdc.c_data.pdc_bits[i].pdc_syn) break;

    return (i == PDCACHE_LINE_FRAGMENTS) ? 0 : 1;
}


void
ecc_pdcache_line_show(t5_cache_line_t *pdc, void (*prf)(char *, ...))
{
    int i;

    (*prf)("Addr %16x tag  %16x tagparity %1x statmod 0x%1x\n",
	   pdc->c_addr, pdc->c_tag,pdc->c_tag & 1,
	   (pdc->c_tag & CTP_STATEMOD_MASK) >> CTP_STATEMOD_SHFT);

    (*prf)("Index %8x  Way %1d State %7s\n",
	   pdc->c_idx, pdc->c_way, cache_valid_str[pdc->c_state]);

    for (i = 0; i < PDCACHE_LINE_FRAGMENTS; i++) {
	(*prf)("Off %2x Data %w328x CB %1x ECC %1x Syn %1x\n",
	       i * 4, 
	       pdc->c_data.pdc_bits[i].pdc_data,
	       pdc->c_data.pdc_bits[i].pdc_cb,
	       pdc->c_data.pdc_bits[i].pdc_ecc,
	       pdc->c_data.pdc_bits[i].pdc_syn);
    }
    (*prf)("\n");
}



void
ecc_picache_line_dump(__psunsigned_t k0addr, int way, void (*prf)(char *, ...))
{
    t5_cache_line_t pic;

    if (k0addr < R10K_ICACHE_LINES)
	k0addr = PHYS_TO_K0(k0addr * CACHE_ILINE_SIZE);
    else
	k0addr = PHYS_TO_K0(K0_TO_PHYS(k0addr)) & CACHE_ILINE_MASK;

    ecc_picache_line_get(k0addr, way, &pic);
    ecc_picache_line_show(&pic, prf);
}



void
ecc_pdcache_line_dump(__psunsigned_t k0addr, int way, void (*prf)(char *, ...))
{
    t5_cache_line_t pdc;

    if (k0addr < R10K_DCACHE_LINES)
	k0addr = PHYS_TO_K0(k0addr * CACHE_DLINE_SIZE);
    else
	k0addr = PHYS_TO_K0(K0_TO_PHYS(k0addr)) & CACHE_DLINE_MASK;

    ecc_pdcache_line_get(k0addr, way, &pdc);
    ecc_pdcache_line_show(&pdc, prf);
}


void
ecc_scache_line_dump(__psunsigned_t k0addr, int way, void (*prf)(char *, ...))
{
    t5_cache_line_t sc;
    int size = getcachesz(cpuid());

    if (k0addr <= (size / 256))
	k0addr = PHYS_TO_K0(k0addr * CACHE_SLINE_SIZE);
    else
	k0addr = PHYS_TO_K0(K0_TO_PHYS(k0addr)) & CACHE_SLINE_MASK;

    ecc_scache_line_get(k0addr, way, &sc);
    ecc_scache_line_show(&sc, prf);
}



void
ecc_scache_dump(void (*prf)(char *, ...), int check)
{	
    int line, size, way;

    size = private.p_scachesize;

    for (line = 0; 
	 line < (size / (CACHE_SLINE_SIZE * SCACHE_WAYS)); line++) {
	for (way = 0; way < SCACHE_WAYS; way++) 
	    if (!check || ecc_scache_line_check_syndrome(line, way))
		ecc_scache_line_dump(line, way, prf);
    }
}


void
ecc_picache_dump(void (*prf)(char *, ...), int check)
{	
    int line, way;
    
    for (line = 0; line < R10K_ICACHE_LINES; line++) {
	for (way = 0; way < PCACHE_WAYS; way++)
	    if (!check || ecc_picache_line_check_syndrome(line, way))
		ecc_picache_line_dump(line, way, prf);
    }
}


void
ecc_pdcache_dump(void (*prf)(char *, ...), int check)
{	
    int line, way;

    for (line = 0; line < R10K_DCACHE_LINES; line++) {
	for (way = 0; way < PCACHE_WAYS; way++)
	    if (!check || ecc_pdcache_line_check_syndrome(line, way))
		ecc_pdcache_line_dump(line, way, prf);
    }
}
    

/*
 * returns the bad data bit from the syndrome for a secondary
 * cache line data error
 */
int
scache_data_bit_error(int syndrome)
{
        int i,j,r;

        syndrome &= 0777;

        /*
         * 128 data bits + 9 ECC bits + 1 parity bit
         */
        for (i = 0 ; i < 136 ; i++) {

                for (r = j = 0 ; j < SCDATA_ECC_BITS ; j++) {
                        if (i <= 63) {
                                if (((__uint64_t) 1 << i) &
                                                scache_data_ecc[j].d_emasklo)
                                        r |= (1 << (SCDATA_ECC_BITS - j - 1));
                        } else if (i <= 127) {
                                if (((__uint64_t) 1 << (i - 64)) &
                                                scache_data_ecc[j].d_emaskhi)
                                        r |= (1 << (SCDATA_ECC_BITS - j - 1));
                        } else {
                                if (((__uint64_t) 1 << (i - 128)) &
                                                scache_data_ecc[j].e_mask)
                                        r |= (1 << (SCDATA_ECC_BITS - j - 1));
                        }
                }
                if (syndrome == r)
                        return i;
        }

        return -1;
}

#endif /* R10000 */


#if defined (SN)

/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */
/* ************************************************************************* */

#include <sys/rmap.h>
#include <sys/proc.h>
#include <sys/errno.h>
#include <os/proc/pproc_private.h>

#include <sys/SN/SN0/ip27log.h>
#include <sys/SN//kldiag.h>

#define SC_QUADWORD_BITS        (128 + 9 + 1)

/*
 * Error information for secondary cache
 *
 * Tag  is 26-bit  protected by 7-bit ECC
 *
 * Data is 128-bit protected by 9-bit ECC + even parity
 */

typedef struct r10k_scache_errinfo_s {
        __uint32_t tag_sbe_err;
        __uint32_t tag_uce_err;
        __uint32_t tag_bit_in_err[26];

        __uint32_t data_par_err;
        __uint32_t data_sbe_err;
        __uint32_t data_uce_err;
        __uint32_t data_bit_err   [SC_QUADWORD_BITS];
        __uint32_t data_bit_err_lo[SC_QUADWORD_BITS];
        __uint32_t data_bit_err_hi[SC_QUADWORD_BITS];
} r10k_scache_errinfo_t;


void    cache_error_handle_remote_cpu	  	(void *, void *, void *,void *);
void 	park_cpu			(__uint64_t  *);
void    cache_error_exception_handler	(eframe_t *, sn_eccframe_t *);
void	cache_err_init			(void);

void	dcache_error_exception		(sn_eccframe_t *);
void	icache_error_exception		(sn_eccframe_t *);
void 	scache_error_exception		(sn_eccframe_t *);
void	sysad_error_exception		(sn_eccframe_t *);
void	prom_disable_cpu		(cpuid_t );


void	ecc_force_cache_line_invalid	(__psunsigned_t,int,int);

extern void qprintf       (char *, ...);
extern void cn_flush	  (void);
extern void uncached      (void);
extern void runcached     (void);
extern void print_cacherr (r10k_cacherr_t );

char * tag_state_ascii[4] = {
	"Invalid",
	"Shared",
	"Clean Exclusive",
	"Dirty Exclusive",
};


char * dir_state_ascii[] = {
	"MD_DIR_SHARED",
	"MD_DIR_POISONED",
	"MD_DIR_EXCLUSIVE",
	"MD_DIR_BUSY_SHARED",
	"MD_DIR_BUSY_EXCL",
	"MD_DIR_WAIT",
	"??????????",
	"MD_DIR_UNOWNED",
};


char *errtype_ascii[4] = {
	"Primary Data Cache",
	"Primary Instruction Cache",
	"Secondary Cache",
	"SYSAD"
};




void
cache_err_init()
{
	cpuid_t        cpu;
        sn_eccframe_t *sn_eccf,*sn_eccf_cpu0;
	extern int     cerr_flags;
	int	       not_supported = 0;
	
	if (cerr_flags & CERR_FLAG_NEW_HANDLER) {
		cmn_err(CE_WARN,"new cache error handler not yet supported\n");
		not_supported++;
	}
	if (cerr_flags & CERR_FLAG_RUN_DIAG) {
		cmn_err(CE_WARN,"cache error micro diagnostics not yet supported\n");
		not_supported++;
	}
	if (cerr_flags & CERR_FLAG_RECOVER) {
		cmn_err(CE_WARN,"cache error recovery not yet supported\n");
		not_supported++;
	}
	if (cerr_flags & CERR_FLAG_RECOVER) {
		cmn_err(CE_WARN,"cache error isolating of failing CPUsnot yet supported\n");
		not_supported++;
	}
	if (not_supported)
		cmn_err(CE_WARN,
		"unsupported usage of features may lead to hangs and/or data corruption\n");
	

	for (cpu = 0 ; cpu < maxcpus ; cpu++) {
		if (pdaindr[cpu].pda == NULL)
			continue;

		sn_eccf = SN_ECCF(cpu);

		/*
		 * keep a pointer to CPU 0s ECC frame so we can 
		 * update the error handling CPU to be different
		 */
		if (cpu == 0)
			sn_eccf_cpu0 = sn_eccf;

		bzero((void *) sn_eccf,sizeof (sn_eccframe_t));

		sn_eccf->sn_eccf_flags = (__uint64_t *)
				TO_UNCAC((paddr_t ) &pdaindr[cpu].pda->p_cerr_flags);
                sn_eccf->sn_eccf_fail_cpu = cpu;
                sn_eccf->sn_eccf_hand_cpu = 0;

		if (cpu_enabled(cpu))
			sn_eccf_cpu0->sn_eccf_hand_cpu = cpu;
	}

}

#define	CERR_LED(c)	{  volatile __uint64_t i;			\
									\
			   SET_MY_LEDS(c);				\
			   for (i = 0 ; i < LED_FLIP_COUNT ; i++)	\
				;					\
			}


/*ARGSUSED*/
void
cache_error_exception_handler(eframe_t *ef, sn_eccframe_t *sn_eccf_fail)
{
	sn_eccframe_t *sn_eccf_hand;
	int	       i;

	SET_MY_LEDS(0xf0);    

	sn_eccf_fail = SN_ECCF(sn_eccf_fail->sn_eccf_fail_cpu);
	sn_eccf_hand = SN_ECCF(sn_eccf_fail->sn_eccf_hand_cpu);

	*sn_eccf_fail->sn_eccf_flags = CERR_PENDING;

	switch(sn_eccf_fail->sn_eccf_cache_err.error_type.err) {
	case R10K_CACHERR_SC:
		scache_error_exception(sn_eccf_fail);
		break;
	case R10K_CACHERR_PDC:
		dcache_error_exception(sn_eccf_fail);
		break;
	case R10K_CACHERR_PIC:
		icache_error_exception(sn_eccf_fail);
		break;
	/*
	 * need to add SYSAD
	 */

	default :
		break;
	}

	/*
	 * lock the error handling CPU in case we have multiple
	 * requests going to the same handling CPU
	 * We will be stuck here if something is wrong with the
	 * handling CPU
	 *
	 * set state bit in our flags indicating that we
	 * wait for the handler. Used for debugging
	 */
	*sn_eccf_fail->sn_eccf_flags |= CERR_WAIT_FOR_HANDLER;

	for (i = 0 ; i < CERR_RETRIES ; i++) {

		if (! (*sn_eccf_hand->sn_eccf_flags & CERR_ACTIVE)) 
			break;

		/*
		 * indicate that we are stuck here. 
		 */
		CERR_LED(0x00);
		CERR_LED(0xff);
	}

	if (i == CERR_RETRIES)
		panic("Cache Error CPU %d, Hand CPU %d stuck err=0x%x f_flgs=0x%x h_flgs=x%x\n",
			sn_eccf_fail->sn_eccf_fail_cpu,
                        sn_eccf_fail->sn_eccf_hand_cpu,
			sn_eccf_fail->sn_eccf_cache_err.error_type.err,
			*sn_eccf_fail->sn_eccf_flags,
			*sn_eccf_hand->sn_eccf_flags);

				
	*sn_eccf_hand->sn_eccf_flags |=  CERR_ACTIVE;
	*sn_eccf_fail->sn_eccf_flags &= ~CERR_WAIT_FOR_HANDLER;

	/*
	 * before we dispatch the error handling to another CPU,
	 * check that the error handling CPU has not a cache error pending 
	 * itself. Otherwise the cpuaction might not work and we will hang
	 */
	if (*sn_eccf_hand->sn_eccf_flags & CERR_PENDING) {

		panic("Cache Error Handling deadlock, CPUs : fail=%d hand=%d handflags=0x%x\n",
				sn_eccf_fail->sn_eccf_fail_cpu,
				sn_eccf_fail->sn_eccf_hand_cpu,
				*sn_eccf_hand->sn_eccf_flags);
	}

	cpuaction(sn_eccf_fail->sn_eccf_hand_cpu,
		  cache_error_handle_remote_cpu,A_NOW,(void *) sn_eccf_fail);

	/*
	 * wait for handler
	 */
	while (*sn_eccf_hand->sn_eccf_flags & CERR_ACTIVE)
		;

	/*
	 * if the problem already has been fixed earlier by discarding
	 * the line, we are done.
	 */
	if (*sn_eccf_fail->sn_eccf_flags & CERR_BADLINE_INVALIDATED) {
        	*sn_eccf_fail->sn_eccf_flags &= ~CERR_PENDING;
		return;
	}

	if ((*sn_eccf_fail->sn_eccf_flags & CERR_RECOVER) 	     && 
	    (! (*sn_eccf_fail->sn_eccf_flags & CERR_RECOVER_FAILED))) {

		/*
	 	 * handling CPU did try successful to recover, 
		 * so we need to remove the bad line
	 	 *
	 	 * NOT YET READY
	 	 */
		*sn_eccf_fail->sn_eccf_flags &= ~CERR_PENDING;
		return;
	}



	if (*sn_eccf_fail->sn_eccf_flags & CERR_DIAG) {
		/*
		 * simulate something
		 */
		CERR_LED(0x00); CERR_LED(0xff);
		CERR_LED(0x00); CERR_LED(0xff);
		CERR_LED(0x00); CERR_LED(0xff);
		CERR_LED(0x00); CERR_LED(0xff);
		CERR_LED(0x00); CERR_LED(0xff);
		CERR_LED(0x00); CERR_LED(0xff);
		CERR_LED(0x00); CERR_LED(0xff);
		CERR_LED(0x00); CERR_LED(0xff);

		*sn_eccf_fail->sn_eccf_flags |=  CERR_DIAG_FAILED;
		
                *sn_eccf_hand->sn_eccf_flags |=  CERR_ACTIVE;
                *sn_eccf_fail->sn_eccf_flags &= ~CERR_ACTIVE;
	}
		

        *sn_eccf_fail->sn_eccf_flags &= ~CERR_PENDING;
}




#define SLINE_ADDR(cer) (((cer) & CE_SINDX_MASK) + K0BASE)


void
cerr_isolate_cpu(cpuid_t cpu)
{
	int        ret;
	extern int cerr_flags;
	extern int mp_isolate(int);

	if ( ! (cerr_flags & CERR_FLAG_ISOLATE_FAILED_CPU))
		return;

	if ((ret = mp_isolate(cpu)) == 0) 
		cmn_err(CE_WARN,
			"CPU %d isolated after recovered cache error\n",
			cpu);
	else 
		cmn_err(CE_WARN,"CPU %d isolation failed, errno=%d\n",
			cpu,ret);	
}


/*ARGSUSED*/
void cache_error_handle_remote_cpu(void *arg1, void *arg2, void *arg3,void *arg4)
{
	sn_eccframe_t *sn_eccf_fail = (sn_eccframe_t *)arg1;
        sn_eccframe_t *sn_eccf_hand = SN_ECCF(cpuid());
	char nodename[80];
	char msg     [80];
	char *errtype;
	char *tagstate;
	int  pending_errcnt;
	int  cpu;

	extern int cerr_flags;

        vertex_to_name(cpuid_to_vertex(sn_eccf_fail->sn_eccf_fail_cpu),
                nodename,sizeof nodename);
	errtype = errtype_ascii[sn_eccf_fail->sn_eccf_cache_err.error_type.err & 0x3];

	switch(sn_eccf_fail->sn_eccf_cache_err.error_type.err & 0x3) {
		case R10K_CACHERR_PIC :
			tagstate = tag_state_ascii[sn_eccf_fail->sn_eccf_il_tag.t.state];
			break;
		case R10K_CACHERR_PDC :
			tagstate = tag_state_ascii[sn_eccf_fail->sn_eccf_dl_tag.t.state];
			break;
		case R10K_CACHERR_SC  :
			tagstate = tag_state_ascii[sn_eccf_fail->sn_eccf_sl_tag.t.state];
			break;
		default:
			tagstate = NULL;
			break;
	}

	cmn_err(CE_WARN,"CPU %d (%s) %s Error\n",
		sn_eccf_fail->sn_eccf_fail_cpu,
		nodename,
		errtype);
	cmn_err(CE_WARN,"CPU %d  [%s %s] paddr=0x%x\n",
		 sn_eccf_fail->sn_eccf_fail_cpu,
		*sn_eccf_fail->sn_eccf_flags & CERR_TAG  ? "TagError" : "",
		*sn_eccf_fail->sn_eccf_flags & CERR_DATA ? "DataError": "",
		 sn_eccf_fail->sn_eccf_addr);
	if (tagstate != NULL)
		cmn_err(CE_WARN,"CPU %d  Tag State = %s\n",
			sn_eccf_fail->sn_eccf_fail_cpu,
			tagstate);

	/*
	 * if the cache error handler has invalidated the line
	 * since it was not dirty-excl, we simply report the 
	 * error.
	 * Since a cache error is not really expected, see if 
	 * we should offload the CPU from any new work. The next
	 * cache error might be fatal to the system ...
	 */
	if (*sn_eccf_fail->sn_eccf_flags & CERR_BADLINE_INVALIDATED) {

		cmn_err(CE_WARN,
			"CPU %d  Cache Error recoverd by invalidating line\n",
			 sn_eccf_fail->sn_eccf_fail_cpu);


		/*
		 * let the failing CPU run now, we must be able to be
		 * scheduled on this CPU via mustrun and there is no
		 * need to hold up the CPU any longer 
		 */
		*sn_eccf_hand->sn_eccf_flags &= ~CERR_ACTIVE;

		cerr_isolate_cpu(sn_eccf_fail->sn_eccf_fail_cpu);
		return;
	}


	/*
	 * should we attempt to recover from a cache error by killing
	 * the user application(s) ?
	 */
	if (cerr_flags & CERR_FLAG_RECOVER) {

        	pfn_t  pfn   = sn_eccf_fail->sn_eccf_addr >> 14;
        	pfd_t *pfdat = pfntopfdat(pfn);
		
		*sn_eccf_fail->sn_eccf_flags |= CERR_RECOVER;

        	if ((sn_eccf_fail->sn_eccf_pid != 0) && 
		    (pfdat->pf_use == 1)) {

                	/*
                 	 * For the moment we only will do this 
			 * for the simplest case
                 	 */
			sigtopid(sn_eccf_fail->sn_eccf_pid, 
				 SIGKILL, 
				 SIG_ISKERN|SIG_NOSLEEP, 0, 0, 0);

			cmn_err(CE_WARN,"Cache Error on CPU %d [node %s]\n",
				sn_eccf_fail->sn_eccf_fail_cpu,nodename);
			cmn_err(CE_WARN,
				"recovered by killing PID %d\n",
				sn_eccf_fail->sn_eccf_pid);

			*sn_eccf_hand->sn_eccf_flags &= ~CERR_ACTIVE;

	                cerr_isolate_cpu(sn_eccf_fail->sn_eccf_fail_cpu);

			return;

		} else {
			*sn_eccf_fail->sn_eccf_flags |= CERR_RECOVER_FAILED;

			cmn_err(CE_WARN,"Cache Error recovery failed, pid=%d pf_use=%d\n",
				sn_eccf_fail->sn_eccf_pid,
				pfdat->pf_use);
			
		}
			
	}

	/*
	 * once we get here, we are going to panic
	 */

	*sn_eccf_fail->sn_eccf_flags |= CERR_PANIC;

	/*
	 * should we try to run some micro-diagnostics in order
	 * to isolate the problem ?
	 */
	if (cerr_flags & CERR_FLAG_RUN_DIAG) {

		cmn_err(CE_WARN,"requesting micro-diag on CPU %d\n",
			sn_eccf_fail->sn_eccf_fail_cpu);
		*sn_eccf_fail->sn_eccf_flags |=  CERR_DIAG;
		*sn_eccf_fail->sn_eccf_flags |=  CERR_ACTIVE;
		*sn_eccf_hand->sn_eccf_flags &= ~CERR_ACTIVE; 

		while (*sn_eccf_fail->sn_eccf_flags & CERR_ACTIVE)
			;

		cmn_err(CE_WARN,"micro-diag on CPU %d %s\n",
			sn_eccf_fail->sn_eccf_fail_cpu,
			*sn_eccf_fail->sn_eccf_flags & CERR_DIAG_FAILED ?
				"** FAILED **" :
				"terminated ok");
	}

	sprintf(msg,"%d: CPU disabled by cache error handler",
			KLDIAG_CPU_DISABLED);

        ip27log_setenv(pdaindr[sn_eccf_fail->sn_eccf_fail_cpu].pda->p_nasid,
                       pdaindr[sn_eccf_fail->sn_eccf_fail_cpu].pda->p_slice ?
                                DISABLE_CPU_A :
                                DISABLE_CPU_B ,
                       msg,0);


        /*
         * check if we have other CPUs which have cache errors pending
         */
        for (pending_errcnt = cpu = 0 ; cpu < maxcpus ; cpu++) {
		sn_eccframe_t *f;
		
                if (pdaindr[cpu].pda          == NULL ||
                    sn_eccf_fail->sn_eccf_fail_cpu == cpu)
                        continue;


                f = SN_ECCF(cpu);
                if (*f->sn_eccf_flags & CERR_PENDING)
                        pending_errcnt++;
        }

        if (pending_errcnt) {
                cmn_err(CE_WARN,"Other CPUs with pending Cache Errors :\n");

                for (cpu = 0 ; cpu < maxcpus ; cpu++) {
			sn_eccframe_t *f;

                        if (pdaindr[cpu].pda          == NULL ||
                            sn_eccf_fail->sn_eccf_fail_cpu == cpu)
                                continue;

                        f = SN_ECCF(cpu);
                        if (*f->sn_eccf_flags & CERR_PENDING) {
                                vertex_to_name(cpuid_to_vertex(cpu),
                                               nodename,sizeof nodename);

                                cmn_err(CE_WARN,"CPU %d [%s]\n",cpu,nodename);
                        }
                }
        }

	*sn_eccf_hand->sn_eccf_flags &= ~CERR_ACTIVE;
	cmn_err(CE_PANIC,"CPU %d (%s) Unrecovered %s Error\n",
		sn_eccf_fail->sn_eccf_fail_cpu,nodename,errtype);
}

void
prom_disable_cpu(cpuid_t cpu)
{
	char    msg[32];

	if (numcpus == 1)
		return;

	sprintf(msg,"CPU disabled by cache error handler\n");

	ip27log_setenv(pdaindr[cpu].pda->p_nasid,
		       pdaindr[cpu].pda->p_slice ? 
				DISABLE_CPU_A :
				DISABLE_CPU_B ,
		       msg,0);
}




/*
 * L2$ is protected by ECC and even parity.
 *
 * The tag field including address, state and vindex is protected by a 7 bit ECC.
 *
 *
 * Errors are detected on
 *
 *	- data read
 *		primary cache refill
 *		secondary cache WB
 *
 *	- tag read additonal
 *		cacheop execution
 *		external events
 */
void
scache_error_exception(sn_eccframe_t *sn_eccf)
{
	__uint64_t way ;
	__uint64_t addr;

	SET_MY_LEDS(0xf1);

	/*
	 * we expect that the error has occured either Way0 and/or Way1.
 	 * If non of the way bits is set, something is really wrong
	 */
	if ((sn_eccf->sn_eccf_cache_err.sc.data_array_err == 0) && 
            (sn_eccf->sn_eccf_cache_err.sc.tag_array_err  == 0)) {
		*sn_eccf->sn_eccf_flags |= CERR_NOWAY;
		return;	
	}

	/*
	 * if there is a UCE in either Way0 or Way1, we will be unable
	 * to handle the error safely, as the tag address and/or state
	 * might be bad.
	 */
	if (sn_eccf->sn_eccf_cache_err.sc.tag_array_err) {
		*sn_eccf->sn_eccf_flags |= CERR_TAG;
		return;
	}

	/*
	 * 	so this must be a data error
	 */
	*sn_eccf->sn_eccf_flags |= CERR_DATA;

	/*
	 * If we made it all the way to here, we have a data error.
	 *
	 *
	 * we now need to read the tag to find out if and how we
	 * can recover from this error.
	 *
	 */
	way  = sn_eccf->sn_eccf_cache_err.sc.data_array_err == 2 ? 1 : 0;
	addr =  (sn_eccf->sn_eccf_cache_err.sc.sidx << 6) | way | K0BASE;

	sn_eccf->sn_eccf_pid = curprocp->p_pid; /* CACHED ????? */

	/*
	 * we now need the tag information to decide on recovery.
	 * this is tricky, external coherency requests might change
	 * the tag state while we are executing in the cache error
	 * handler
	 * This requires more detailed analysis. I've so far checked
	 * that a read request from another CPU will cause a SYSAD
	 * error on the requesting CPU
	 */
	sn_eccf->sn_eccf_sl_tag.v = ecc_cacheOp(CACH_S + C_ILT, addr);

#if 0
        {

        /*
         * Example to monitor tag for changes due to external coherency
         * requests
         * experiemental code, will be removed
         */
        __uint64_t newtag;
        __uint64_t vec_ptr;
        hubreg_t   elo;
        int        state;

        get_dir_ent((paddr_t)c->addr,&state,&vec_ptr,&elo);
        qprintf("addr=0x%x dir_state=0x%x [%s]\n",
                        c->addr,state,dir_state_ascii[state]);

        set_dir_state((paddr_t)c->addr,MD_DIR_POISONED);

        get_dir_ent((paddr_t)c->addr,&state,&vec_ptr,&elo);
        qprintf("addr=0x%x dir_state=0x%x [%s]\n",
                        c->addr,state,dir_state_ascii[state]);

        qprintf("looping\n");
        while (*sn_eccf->sn_eccf_flags &) {
                newtag = ecc_cacheOp(CACH_S + C_ILT,addr);

                if (c->sl_tag.v != newtag) {

                        qprintf("*** TAG CHANGED *** old=0x%x new=0x%x\n",
                                c->sl_tag.v,
                                newtag);
                }
        }
        /*NOTREACHED*/

        }

#endif

	/*
	 * note that the cacheop does NO ECC, e.g. we could have a SBE
	 * or UCE pending, so we need to recheck this
	 */
#if 0
	if (sw_error_correction_scache_tag(&sl_tag) == -1) {

	}
#endif


	sn_eccf->sn_eccf_addr = (sn_eccf->sn_eccf_sl_tag.t.tag << 18) | 
				(sn_eccf->sn_eccf_cache_err.sc.sidx << 6);

	/*
	 * no checking for the famous SN0 "transient cache error", let's
	 * first observe if this is still a real problem now that we avoid
	 * all cached references in the handler
	 */

	switch (sn_eccf->sn_eccf_sl_tag.t.state) {
	case CTS_STATE_I :	/* invalid 	*/
	case CTS_STATE_S :	/* shared	*/
	case CTS_STATE_CE:	/* clean-excl	*/

		ecc_cacheOp(CACH_S + C_IINV, addr + way);
		*sn_eccf->sn_eccf_flags |= CERR_BADLINE_INVALIDATED;
		
		break;
	case CTS_STATE_DE :	/* dirty excl	*/
		break;
	}



#if 0
	if (! (*sn_eccf->sn_eccf_flags & CERR_BADLINE_INVALIDATED)) {

		cacheop_t cop;
		sl_tag_t  sl_tag;

		cop.cop_address   = addr;
		/*
		 * we could not fix the problem by discarding the line.
		 * Before control is handled of the error handling CPU
		 * we need to make sure that this line is invalid, the
		 * application might be killed
		 */
		cop.cop_operation = CACH_S + C_ILT;
		cacheOP(&cop);
		sl_tag.v  = cop.cop_taghi;
		sl_tag.v <<= 32; 
		sl_tag.v |= cop.cop_taglo;

		sl_tag.t.state = CTS_STATE_I;
	
		cop.cop_taglo     = (__uint32_t)  sl_tag.v;
		cop.cop_taghi     = (__uint32_t) (sl_tag.v >> 32);
		cop.cop_operation = CACH_S + C_ILT;

		cacheOP(&cop);
	}

#endif

}



void
icache_error_exception(sn_eccframe_t *sn_eccf)
{
        __uint64_t addr;

        if ((sn_eccf->sn_eccf_cache_err.pic.data_array_err == 0) &&
            (sn_eccf->sn_eccf_cache_err.pic.tag_addr_err   == 0) &&
            (sn_eccf->sn_eccf_cache_err.pic.tag_state_err  == 0)) 
                *sn_eccf->sn_eccf_flags |= CERR_NOWAY;

	addr = ILINE_ADDR(sn_eccf->sn_eccf_cache_err.val);

        ecc_cacheOp(CACH_PI+C_IINV, addr);
        ecc_cacheOp(CACH_PI+C_IINV, addr^1);
        ecc_force_cache_line_invalid(addr, 0, CACH_PI);
        ecc_force_cache_line_invalid(addr, 1, CACH_PI);

        *sn_eccf->sn_eccf_flags |= CERR_BADLINE_INVALIDATED;
}


/*ARGSUSED*/
void
dcache_error_exception(sn_eccframe_t *sn_eccf)
{
	cmn_err(CE_PANIC,"unsupopred dcache error exception\n");

}


/*ARGSUSED*/
void
sysad_error_exception(sn_eccframe_t *sn_eccf)
{

	cmn_err(CE_PANIC,"unsupopred sysad error exception\n");
}



int
l2_data_sbe(r10k_sl_t *sl)
{
        __uint16_t    ecc_rcv;
        __uint16_t    ecc_exp;
        int           i,j;
        int           syndrome = -1;

        for (i = j = 0 ; j < SL_ENTRIES  ; i += 2 , j++)  {

                ecc_rcv  = sl->sl_ecc[j].v & 0x1ff;
                ecc_exp  = ecc_compute_scdata_ecc(sl->sl_data[i],
                                                  sl->sl_data[i + 1]) & 0x1ff;
                syndrome = ecc_rcv ^ ecc_exp;

                if (syndrome != 0)
                        break;
        }

        return syndrome;
}



int
l2_cache_check(void)
{
        r10k_scache_errinfo_t scache_errinfo;
        r10k_sl_t             sl;
        int                   i;
	int		      leds = 0;
	int 		      syndrome = -1;

        bzero((void *) &scache_errinfo,sizeof scache_errinfo);


	set_leds(leds);

        for (i = 0 ; i < R10K_SCACHELINES ; i += 2) {
                sLine(SCACHE_ADDR(i,0),&sl);
		if ((syndrome = l2_data_sbe(&sl)) != -1) 
			break;

                sLine(SCACHE_ADDR(i,1),&sl);
		if ((syndrome = l2_data_sbe(&sl)) != -1) 
			break;

		if (! (i % 512)) {
			leds--;
			set_leds(leds);
		}

        }

	return syndrome;
}

char *
scache_data_bit_location(int bit)
{
	static char  result[32];
	extern char *r10k_scache_pinout[];

	char *pin = "unknown";
	char *ram = "unknown";

	if (bit <= 127) {

		/*
		 * Data Error
		 */

		/*
		 * IP27 and IP31 scache RAM mapping
		 *
	 	 * SCD3   SCD2   SCD7     SCD6    SCD1   SCD0  SCD5   SCD4  
	 	 * 95:80  79:64  127:112  111:96  31:16  15:0  63:48  47:32
		 */
		if (bit >=   0 && bit <=  15) ram = "SCD0";
		if (bit >=  16 && bit <=  31) ram = "SCD1";
		if (bit >=  32 && bit <=  47) ram = "SCD4";
		if (bit >=  48 && bit <=  63) ram = "SCD5";
		if (bit >=  64 && bit <=  79) ram = "SCD2";
		if (bit >=  80 && bit <=  95) ram = "SCD3";
		if (bit >=  96 && bit <= 111) ram = "SCD6";
		if (bit >= 112 && bit <= 127) ram = "SCD7";

		pin = r10k_scache_pinout[bit];

	} else if (bit >= 128 && bit <= 137) {

		/* 
		 * ECC error
		 */

		int          ecc_bit = bit - 128;
		extern char *ip27_ip31_scache_sbe_ram_ecc_map[];

		if (ecc_bit >= 0 && ecc_bit < 10)
			ram = ip27_ip31_scache_sbe_ram_ecc_map[ecc_bit];

		pin = r10k_scache_pinout[bit];
	}

	sprintf(result,"[Pin %s / RAM %s]",pin,ram);

	return result;
}


char *ip27_ip31_scache_sbe_ram_ecc_map[10] = {
	"SCD1",	/* SCDC0 */
	"SCD5",	/* SCDC1 */
	"SCD0", /* SCDC2 */
	"SCD4",	/* SCDC3 */
	"SCD3", /* SCDC4 */
	"SCD7", /* SCDC5 */
	"SCD2", /* SCDC6 */
	"SCD6", /* SCDC7 */
	"SCD2", /* SCDC8 */
	"SCD3", /* SCDC9 */
};


char *r10k_scache_pinout[138] = {
	"R31",		/* SCData[  0]  */
	"N34",		/* SCData[  1]  */
	"P33",		/* SCData[  2]  */
	"M35",		/* SCData[  3]  */
	"P32",		/* SCData[  4]  */
	"M34",		/* SCData[  5]  */
	"N33",		/* SCData[  6]  */
	"L35",		/* SCData[  7]  */
	"N31",		/* SCData[  8]  */
	"L33",		/* SCData[  9]  */

	"M32",		/* SCData[ 10]  */
	"K34",		/* SCData[ 11]  */
	"M31",		/* SCData[ 12]  */
	"J35",		/* SCData[ 13]  */
	"L32",		/* SCData[ 14]  */
	"J34",		/* SCData[ 15]  */
	"K33",		/* SCData[ 16]  */
	"H35",		/* SCData[ 17]  */
	"K31",		/* SCData[ 18]  */
	"G34",		/* SCData[ 19]  */

	"J32",		/* SCData[ 20]  */
	"G33",		/* SCData[ 21]  */
	"J31",		/* SCData[ 22]  */
	"F35",		/* SCData[ 23]  */
	"H32",		/* SCData[ 24]  */
	"F34",		/* SCData[ 25]  */
	"G31",		/* SCData[ 26]  */
	"E35",		/* SCData[ 27]  */
	"F32",		/* SCData[ 28]  */
	"D34",		/* SCData[ 29]  */

	"F31",		/* SCData[ 30]  */
	"E32",		/* SCData[ 31]  */
	"AA32",		/* SCData[ 32]  */
	"AB35",		/* SCData[ 33]  */
	"AC34",		/* SCData[ 34]  */
	"AB32",		/* SCData[ 35]  */
	"AD35",		/* SCData[ 36]  */
	"AC33",		/* SCData[ 37]  */
	"AD34",		/* SCData[ 38]  */
	"AC31",		/* SCData[ 39]  */

	"AE35",		/* SCData[ 40]  */
	"AD32",		/* SCData[ 41]  */
	"AE33",		/* SCData[ 42]  */
	"AD31",		/* SCData[ 43]  */
	"AF34",		/* SCData[ 44]  */
	"AE32",		/* SCData[ 45]  */
	"AG35",		/* SCData[ 46]  */
	"AF33",		/* SCData[ 47]  */
	"AG34",		/* SCData[ 48]  */
	"AF31",		/* SCData[ 49]  */

	"AH35",		/* SCData[ 50]  */
	"AG32",		/* SCData[ 51]  */
	"AJ34",		/* SCData[ 52]  */
	"AG31",		/* SCData[ 53]  */
	"AJ33",		/* SCData[ 54]  */
	"AH32",		/* SCData[ 55]  */
	"AK35",		/* SCData[ 56]  */
	"AJ31",		/* SCData[ 57]  */
	"AK34",		/* SCData[ 58]  */
	"AK32",		/* SCData[ 59]  */

	"AL35",		/* SCData[ 60]  */
	"AK31",		/* SCData[ 61]  */
	"AM34",		/* SCData[ 62]  */
	"AM33",		/* SCData[ 63]  */
	"D28",		/* SCData[ 64]  */
	"B29",		/* SCData[ 65]  */
	"E27",		/* SCData[ 66]  */
	"C28",		/* SCData[ 67]  */
	"D27",		/* SCData[ 68]  */
	"E26",		/* SCData[ 69]  */

	"A28",		/* SCData[ 70]  */
	"C26",		/* SCData[ 71]  */
	"B15",		/* SCData[ 72]  */
	"D15",		/* SCData[ 73]  */
	"A14",		/* SCData[ 74]  */
	"C14",		/* SCData[ 75]  */
	"A12",		/* SCData[ 76]  */
	"D14",		/* SCData[ 77]  */
	"B12",		/* SCData[ 78]  */
	"C13",		/* SCData[ 79]  */

	"E9",		/* SCData[ 80]  */
	"C8",		/* SCData[ 81]  */
	"D8",		/* SCData[ 82]  */
	"B7",		/* SCData[ 83]  */
	"C7",		/* SCData[ 84]  */
	"A6",		/* SCData[ 85]  */
	"E7",		/* SCData[ 86]  */
	"B6",		/* SCData[ 87]  */
	"D6",		/* SCData[ 88]  */
	"A5",		/* SCData[ 89]  */

	"E6",		/* SCData[ 90]  */
	"C5",		/* SCData[ 91]  */
	"D5",		/* SCData[ 92]  */
	"B4",		/* SCData[ 93]  */
	"C4",		/* SCData[ 94]  */
	"B3",		/* SCData[ 95]  */
	"AN29",		/* SCData[ 96]  */
	"AP29",		/* SCData[ 97]  */
	"AM28",		/* SCData[ 98]  */
	"AN28",		/* SCData[ 99]  */

	"AL27",		/* SCData[100]  */
	"AR28",		/* SCData[101]  */
	"AM27",		/* SCData[102]  */
	"AP27",		/* SCData[103]  */
	"AL16",		/* SCData[104]  */
	"AP15",		/* SCData[105]  */
	"AL15",		/* SCData[106]  */
	"AP13",		/* SCData[107]  */
	"AN14",		/* SCData[108]  */
	"AN13",		/* SCData[109]  */

	"AM14",		/* SCData[110]  */
	"AR12",		/* SCData[111]  */
	"AM9",		/* SCData[112]  */
	"AR8",		/* SCData[113]  */
	"AL9",		/* SCData[114]  */
	"AN8",		/* SCData[115]  */
	"AM8",		/* SCData[116]  */
	"AP7",		/* SCData[117]  */
	"AN7",		/* SCData[118]  */
	"AR6",		/* SCData[119]  */

	"AL7",		/* SCData[120]  */
	"AP6",		/* SCData[121]  */
	"AM6",		/* SCData[122]  */
	"AR5",		/* SCData[123]  */
	"AL6",		/* SCData[124]  */
	"AN5",		/* SCData[125]  */
	"AM5",		/* SCData[126]  */
	"AP4",		/* SCData[127]  */
	"D33",		/* SCDataChk[0] */
	"AL32",		/* SCDataChk[1] */

	"C29",		/* SCDataChk[2] */
	"AR30",		/* SCDataChk[3] */
	"A30",		/* SCDataChk[4] */
	"AP3",		/* SCDataChk[5] */
	"E4",		/* SCDataChk[6] */
	"AN4",		/* SCDataChk[7] */
	"D3",		/* SCDataChk[8] */
	"B27",		/* SCDataChk[9] */
};


#endif /* SN */
