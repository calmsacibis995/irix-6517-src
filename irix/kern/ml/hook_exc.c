/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1992 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/* Copyright(C) 1986, MIPS Computer Systems */

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.58 $"

#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/immu.h>
#include <sys/pda.h>
#include <sys/sbd.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/traplog.h>
#if IP19 || IP21 || IP25
#include <sys/cpu.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/gda.h>
#include <sys/loaddrs.h>
#endif
#if SN
#include <sys/cpu.h>
#include <sys/SN/gda.h>
#include <sys/loaddrs.h>
#else
/*
 * SN0 flips the bottom 64k of a node's  memory depending on whether an
 * access comes from CPU 0 or CPU 1 on a node.  We must account for this.
 * Other architectures don't need it.
 */
#define UALIAS_FLIP_ADDR(_x)	(_x)
#endif
#if IP30
#include <sys/cpu.h>
#include <sys/RACER/gda.h>
#endif	/* IP30 */

#if R4600 || SW_FAST_CACHE_SYNCH
extern int	get_cpu_irr(void);
#endif
#if TFP || BEAST
extern	int trap_table(void);
#endif

int	utlbmiss_prolog_size;
extern volatile int	need_utlbmiss_patch;
extern volatile uint	utlbmiss_patched;
extern inst_t utlbmiss_prolog_patch[];

#if R4000 && (IP19 || IP22)
/*
 * Flag indicating the system has a 250MHz module, which requires
 * a workaround utlbmiss handler be used. For IP19's the flag is
 * set in cpu_probe() and for IP22's it's set in mlreset().
 */ 
int	has_250 = 0;
extern	int utlbmiss_250mhz[];
extern	int eutlbmiss_250mhz[];
#endif
#if DEBUG
int	_dbgintr_overwrite = 1;		/* strictly for debugging */
#endif

#if (R4000 || R10000) && _MIPS_SIM == _ABI64
#define	COMPAT_K0_TO_K1(x)		((x) | COMPAT_K1BASE)
#else
#define COMPAT_K0_TO_K1(x)		K0_TO_K1(x)
#endif
#ifdef _R4600_2_0_CACHEOP_WAR
extern	int utlbmiss_eret_0[];
extern	int utlbmiss_eret_1[];
extern	int utlbmiss_eret_2[];
extern	int utlbmiss_eret_3[];
extern	int locore_eret_0[];
extern	int locore_eret_1[];
extern	int locore_eret_2[];
#if !IP32
extern	int locore_eret_4[];
extern	int locore_eret_5[];
#endif
extern	int locore_eret_7[];
extern	int locore_eret_8[];
extern	int locore_eret_9[];
extern	int locore_eret_10[];
#if SW_FAST_CACHE_SYNCH
extern  int softwin_eret_0[];
#endif
extern	int _r4600_2_0_cacheop_eret_inst[];

#ifndef NULL
#define NULL 0
#endif

static int *r4600_2_0_eret_patch[] = {
	utlbmiss_eret_0,
	utlbmiss_eret_1,
	utlbmiss_eret_2,
	utlbmiss_eret_3,
	locore_eret_0,
	locore_eret_1,
	locore_eret_2,
#if !IP32
	locore_eret_4,
	locore_eret_5,
#endif
#if TLBKSLOTS == 1
	locore_eret_7,
#endif
	locore_eret_8,
	locore_eret_9,
	locore_eret_10,
#if SW_FAST_CACHE_SYNCH
	softwin_eret_0,
#endif
	NULL };

extern	int cacheops_refill_0[];
extern	int cacheops_refill_1[];
extern	int cacheops_refill_2[];
extern	int cacheops_refill_3[];
extern	int cacheops_refill_4[];
extern	int cacheops_refill_5[];
extern	int cacheops_refill_6[];
extern	int cacheops_refill_7[];

static int *r4600_2_0_cacheop_patch[] = {
	cacheops_refill_0,
	cacheops_refill_1,
	cacheops_refill_2,
	cacheops_refill_3,
	cacheops_refill_4,
	cacheops_refill_5,
	cacheops_refill_6,
	cacheops_refill_7,
	NULL };
#endif /* _R4600_2_0_CACHEOP_WAR */

#ifdef _R5000_BADVADDR_WAR
int	_r5000_badvaddr_war = 0;
#endif /* _R5000_BADVADDR_WAR */

/*
 * copy exception handlers code dwn to E_VEC, UT_VEC
 * basically trying to fetch the addresses of the actual handlers out of 
 * private data area
 * writing to uncached space, so we don't need to flush i/d caches
 */
void
_hook_exceptions(void)
{
#if !defined (TFP) && !defined (BEAST)
	extern	int _j_exceptnorm[];
	extern	int _j_endexceptnorm[];
	extern	int _j_exceptnorm_nosymmon[];
	extern	int _j_endexceptnorm_nosymmon[];
	extern	int _exceptnorm_prolog[];
	extern	int _endexceptnorm_prolog[];
	extern	int exception[];
	extern	int exception_endfast[];
	extern	int utlbmiss[], utlbmissj[];
	extern	int eutlbmiss[], eutlbmissj[];
	extern	char utlbmiss_prolog_up[];
	extern	char eutlbmiss_prolog_up[];
#ifdef R4600
	extern	char utlbmiss_prolog_r4600[];
	extern	char eutlbmiss_prolog_r4600[];
	extern	int utlbmiss_r4600[];
	extern	int eutlbmiss_r4600[];
#endif
#ifdef _R5000_BADVADDR_WAR
	extern	int utlbmiss_r5000[];
	extern	int eutlbmiss_r5000[];
#endif /* _R5000_BADVADDR_WAR */
#if MP || IP28 || TRAPLOG_DEBUG
	extern	char utlbmiss_prolog_mp[];
	extern	char eutlbmiss_prolog_mp[];
#endif
	register int *p1,*p2, *p2end, *p1patch;
	long	utlbmiss_size;
	int	*utlbmiss_start, *utlbmiss_end;
#if R4000 || R10000
	extern	int utlbmissj[];
	extern	int eutlbmissj[];
	extern	int _ecc_errorvec[];
	extern	int _ecc_end[];
#endif
#if R4000 && R10000
	extern	int _r10k_ecc_errorvec[];
	extern	int _r10k_ecc_end[];
#endif /* R4000 && R10000 */
#ifdef IP32
	extern	char bzero_cdx_entry[];
	extern	char bcopy_cdx_entry[];
	extern  char pciio_pio_read8[];
	extern  char pciio_pio_read16[];
	extern  char pciio_pio_read32[];
	extern  char pciio_pio_read64[];
#endif
#ifdef _R4600_2_0_CACHEOP_WAR
	int	**pi;
#endif
#ifdef R4600
	rev_id_t ri;
	int orion = 0;
#ifdef TRITON
	/*REFERENCED(IP22)*/
	int triton = 0;
#endif

	ri.ri_uint = get_cpu_irr();
	switch (ri.ri_imp) {
#ifdef TRITON
	case C0_IMP_TRITON:
#ifdef _R5000_BADVADDR_WAR
		_r5000_badvaddr_war = 1;
#endif /* _R5000_BADVADDR_WAR */
		triton = 1;
	case C0_IMP_RM5271:
		/*
		 * for RM5271, _r5000_badvaddr_war is enabled
		 * by setting _RM5271_BADVADDR_WAR nvram variable.
		 */
		triton = 1;
#endif /* TRITON */
	case C0_IMP_R4700:
	case C0_IMP_R4600:
		orion = 1;
		break;
	default:
		break;
	}
#endif	/* R4600 */

#if defined(EVEREST) && defined(MULTIKERNEL)
	/* on multi-cell kernels the locore code must ALWAYS
	 * be patched since we can't change it. Also, slave cells
	 * do not need to copy anything to locore since golden
	 * cell will take care of it.
	 */
	need_utlbmiss_patch++;

	if (evmk_cellid != evmk_golden_cellid)
		return;
#endif /* EVEREST && MULTIKERNEL */

#if MP
#if DEBUG
	/*
	 * On everest, the system debugger requires the kernel
	 * to jump through the PDA for exception vectors.
	 * EVNUMA kernels always patch the utlbmiss handler.
	 */
	need_utlbmiss_patch++;
#else /* !DEBUG */
	/*
	 * If not compiled debug, and kdebug is now true (ie, we loaded 
	 * symmon) then we patch the utlbmiss_prolog after it is
	 * copied. This allows things like "plist" etc to work on kernels
	 * compiled without -DDEBUG. Note that these guys are only patched
	 * in uncached space.
	 * NOTE: need_utlbmiss_patch will stay > 0 and always leave the
	 * prolog_patch intact.
	 */

	if (kdebug > 0)
		need_utlbmiss_patch++;
#endif /* !DEBUG */
#endif /* MP */

#if defined (SN0)
	need_utlbmiss_patch++;
#endif
#ifdef IP32
	/* enable Create Dirty Exclusive on triton systems */
	if (triton) {
		p1 = (int *)(K0_TO_K1(bzero_cdx_entry));
		*p1 = 0x3042001F;	/* andi v0,v0,31 */
		p1 = (int *)(K0_TO_K1(bcopy_cdx_entry));
		*p1 = 0x3042001F;	/* andi v0,v0,31 */
#if defined (R10000) && defined (R10000_MFHI_WAR)
		private.p_r10kwar_bits |= R10K_MFHI_WAR_DISABLE;
#endif /* R10000 && R10000_MFHI_WAR */
		
	}
#ifdef USE_PCI_PIO
	if ( !CRM_IS_REV_1_1 ) {
		  /*
		   * patch the pio read fct if the chip has the fix
		   * to not use the WAR's, we still have the overhead
		   * of the fct call ...
		   *
		   * 0x03e00008  jr       ra;
		   * and one of
		   * 0x90820000  lbu      v0,0(a0)
		   * 0x94820000  lhu      v0,0(a0)
		   * 0x8c820000  lw       v0,0(a0)
		   * 0xdc820000  ld       v0,0(a0)
		   */
		  p1 = (int *)(K0_TO_K1(pciio_pio_read8));
		  p1[0] = 0x03e00008;             /* jr ra */
		  p1[1] = 0x90820000;             /* lbu v0,0(a0) */
		  p1 = (int *)(K0_TO_K1(pciio_pio_read16));
		  p1[0] = 0x03e00008;             /* jr ra */
		  p1[1] = 0x94820000;             /* lhu v0,0(a0) */
		  p1 = (int *)(K0_TO_K1(pciio_pio_read32));
		  p1[0] = 0x03e00008;             /* jr ra */
		  p1[1] = 0x8c820000;             /* lw v0,0(a0) */
		  p1 = (int *)(K0_TO_K1(pciio_pio_read64));
		  p1[0] = 0x03e00008;             /* jr ra */
		  p1[1] = 0xdc820000;             /* ld v0,0(a0) */
         }
#endif /* USE_PCI_PIO */

#endif /* IP32 */

	/*
	 * TLB refill at 0x0000.
	 */

	/* First copy the utlbmiss prolog common to all handlers */

	p1patch = p1 = (int *)(COMPAT_K0_TO_K1(UALIAS_FLIP_ADDR(UT_VEC)));
#if MP || IP28 
	p2 = (int *)utlbmiss_prolog_mp;
	p2end = (int *)eutlbmiss_prolog_mp;
#else /* !MP */
#ifdef R4600
	if (orion) {
		p2 = (int *)(K0_TO_K1(utlbmiss_prolog_r4600));
		p2end = (int *)K0_TO_K1(eutlbmiss_prolog_r4600);
	} else 
#endif /* R4600 */
	{
		p2 = (int *)utlbmiss_prolog_up;
		p2end = (int *)eutlbmiss_prolog_up;
	}
#endif /* !MP */
	utlbmiss_prolog_size = (__psint_t)p2end - (__psint_t)p2;

	while (p2 < p2end)
		*p1++ = *p2++;

	if (need_utlbmiss_patch) {
		*p1patch = *(int *)utlbmiss_prolog_patch;
		utlbmiss_patched = 1;
	}

	/* Then copy the standard tlbmiss handler */

#if R4000 && (IP19 || IP22)
	if (has_250)
		utlbmiss_size = (__psint_t)eutlbmiss_250mhz -
			(__psint_t)utlbmiss_250mhz;
	else
#endif
	utlbmiss_size = (__psint_t)eutlbmiss - (__psint_t)utlbmiss;
	if ((utlbmiss_prolog_size + utlbmiss_size) <= SIZE_EXCVEC) {
#ifdef _R5000_BADVADDR_WAR
		if (_r5000_badvaddr_war) {
			utlbmiss_start = (int *)utlbmiss_r5000;
			utlbmiss_end = (int *)eutlbmiss_r5000;
		} else
#endif /* _R5000_BADVADDR_WAR */
#ifdef R4600
		if (orion) {
			utlbmiss_start = (int *)utlbmiss_r4600;
			utlbmiss_end = (int *)eutlbmiss_r4600;
		} else 
#endif /* R4600 */
		{
			utlbmiss_start = (int *)utlbmiss;
			utlbmiss_end = (int *)eutlbmiss;
#if R4000 && (IP19 || IP22)
			if (has_250) {
				utlbmiss_start = (int *)utlbmiss_250mhz;
				utlbmiss_end = (int *)eutlbmiss_250mhz;
			}
#endif
		}
	} else {
		utlbmiss_start = (int *)utlbmissj;
		utlbmiss_end = (int *)eutlbmissj;
	}

	ASSERT_ALWAYS((((__psint_t)utlbmiss_end - (__psint_t)utlbmiss_start) + \
		utlbmiss_prolog_size) <= SIZE_EXCVEC);

	p2 = (int *)utlbmiss_start;

	while (p2 < utlbmiss_end)
		*p1++ = *p2++;

#if R4000 || R10000

	/*
	 * XTLB, use TLB refill for now
	 */
	/* First copy the utlbmiss prolog common to all handlers */

	p1patch = p1 = (int *)(COMPAT_K0_TO_K1(UALIAS_FLIP_ADDR(XUT_VEC)));
	p2 = (int *)((__psint_t)p2end - utlbmiss_prolog_size);

	while (p2 < p2end)
		*p1++ = *p2++;

	if (need_utlbmiss_patch)
		*p1patch = *(int *)utlbmiss_prolog_patch;

	/* Then copy the standard tlbmiss handler */
	p2 = (int *)utlbmiss_start;

	while (p2 < (int *)utlbmiss_end)
		*p1++ = *p2++;

	/*
	 * Cache Error
	 * Since this will be read uncached, there's no need to use the
	 * UALIAS_FLIP_ADDR() macro.
	 */
	p1 = (int *)COMPAT_K0_TO_K1(ECC_VEC);

#if defined (SN) 

        {
#if 0
	/*
	 * temporary disable
	 */
	extern int cerr_flags;		/* system tuneable to switch to */
					/* new cache error handler	*/
#endif
	int cerr_flags = 0;
	extern int cache_error_exception_vec    [];
	extern int cache_error_exception_vec_end[];

	p2 = (int *) (cerr_flags & 0x0001         ?
			cache_error_exception_vec :
			_ecc_errorvec);
	while (p2 < (int *) (cerr_flags & 0x0001               ?
				 cache_error_exception_vec_end :
				 _ecc_end))
		*p1++ = *p2++;

	}

#else /* SN */

	p2 = (int *)_ecc_errorvec;
	while (p2 < (int *)_ecc_end)
		*p1++ = *p2++;

#endif /* SN */

#if R4000 && R10000
	if (IS_R10000()) {
		p1 = (int *)COMPAT_K0_TO_K1(ECC_VEC);
		p2 = (int *)K0_TO_K1(_r10k_ecc_errorvec);
		while (p2 < (int *)K0_TO_K1(_r10k_ecc_end))
			*p1++ = *p2++;
	}
#endif /* R4000 && R10000 */
#endif /* R4000 || R10000 */

	/*
	 * Others
	 */

#if DEBUG
	/* clearing this flag allows (very) early interrupt debugging */ 
	if (_dbgintr_overwrite)
#endif
	{
		p1 = (int *)COMPAT_K0_TO_K1(UALIAS_FLIP_ADDR(E_VEC));

/* IP28 and IP30 memory starts outside of COMPAT_K0 */
/* so they need to use jr instead of just j */

/* Origin has a similar problem, it needs to use jr instead of j. The
 * compat space belongs to node 0 and we dont want to be executing 
 * instructions from there at all times */
#if !IP28 && !IP30 && !SN0
		if (kdebug) {
#endif	/* !IP28 && !IP30 && !SN0*/
			p2 = (int *)_j_exceptnorm;
			while (p2 < (int *)_j_endexceptnorm)
				*p1++ = *p2++;
#if !IP28 && !IP30 && !SN0
		} else {
			/* If no symmon loaded, then jump directly to
			 * exception() since there's no need to jump
			 * indirect through the VPDA_EXCNORM field.
			 */
			p2 = (int *)_j_exceptnorm_nosymmon;
			while (p2 < (int *)_j_endexceptnorm_nosymmon)
				*p1++ = *p2++;
		}
#endif	/* !IP28 && !IP30 */
#if EVEREST || IP22
		/* NOTE: This optimization can be applied to other platforms
		 * as they are tested.
		 */

		/* We require that exception code copied to locore
		 * fit in two cachelines.  Note that no other vector
		 * follows the general exception vector so it's safe
		 * to overrun the single cacheline normally allocated
		 * for this vector, but we limit it to 2 lines.
		 */
		if (!kdebug) {
			/* We copy a new prolog which does NOT jump to the
			 * exception() code but "falls through" with any
			 * necessary HW specified delay instructions.
			 */
			p1 = (int *)COMPAT_K0_TO_K1(UALIAS_FLIP_ADDR(E_VEC));

			p2 = (int *)_exceptnorm_prolog;
			while (p2 < (int *)_endexceptnorm_prolog)
				*p1++ = *p2++;
		}

		/* Copy the main exception vectoring code immediately
		 * following the _j_exceptnorm code copied above.
		 * We'll simply fixup the private.common_excnorm
		 * address so it jumps to the same cacheline (except
		 * when symmon is active, of course).
		 */
		if (((char*)exception_endfast-(char*)exception) <256){
			p2 = (int *)exception;
			while (p2 < (int *)exception_endfast)
				*p1++ = *p2++;
		}
#endif /*  EVEREST */
	}

#else	/* TFP && BEAST*/
	set_trapbase((__psunsigned_t)trap_table);
#endif	/* !TFP && BEAST*/
#if EVEREST || SN0 || IP30
	GDA->g_hooked_norm = &private.common_excnorm;
	GDA->g_hooked_utlb = &private.p_utlbmisshndlr;
#endif

#ifdef _R4600_2_0_CACHEOP_WAR
	if ((get_cpu_irr() & 0xFFFF) != 0x2020) {
		/* replace jmp with a real eret */
		for (pi = r4600_2_0_eret_patch;
		     (p1 = *pi) != NULL;
		     pi++) 
			p1[0] = _r4600_2_0_cacheop_eret_inst[0]; /* copy eret */
		/* disable uncached load to save time */
		for (pi = r4600_2_0_cacheop_patch;
		     (p1 = *pi) != NULL;
		     pi++) {
			p1[2] = 0; /* nop */
			p1[1] = 0; /* nop */
			p1[0] = 0; /* nop */
		}
	}
	flush_cache(); /* sync caches with these changes */
#endif /* _R4600_2_0_CACHEOP_WAR */

#if R10000
	/*
	 * flush the caches... just in case the T5 speculatively
	 * loaded the exception handlers.
	 */
	flush_cache();
#endif

#ifdef SN0
	setsr(getsr() & ~SR_BEV);
#endif
}

#if SW_FAST_CACHE_SYNCH

/* patch to use 16 byte cache line size is only needed
 * if running CPU is not R4000/R4400.
 */
extern int sw_cachesynch_patch_insts[];
extern int sw_cachesynch_patch_insts_R4000[];

extern int sw_cachesynch_op1[];
extern int sw_cachesynch_op2[];
extern int sw_cachesynch_op3[];
extern int sw_cachesynch_op4[];
static int *sw_cachesynch_patch_locs[] = {
	sw_cachesynch_op1,
	sw_cachesynch_op2,
	sw_cachesynch_op3,
	NULL
};
#ifdef PROBE_WAR
static int *sw_cachesynch_patch_locs_R4000[] = {
	sw_cachesynch_op4,
	NULL
};
#endif

void
sw_cachesynch_patch()
{
	int **pi, *psrc, *pdst;
	rev_id_t ri;

	ri.ri_uint = get_cpu_irr();

	/* The patch is installed only on R4000/R4400 */
	if (ri.ri_imp != 0x04)
		return;
	for (pi = sw_cachesynch_patch_locs,
		 psrc = sw_cachesynch_patch_insts;
		(pdst = *pi) != NULL; pi++,psrc++)
		pdst[0] = psrc[0]; 

#ifdef PROBE_WAR
	/* for R4000 we also need to install patch for probe war */
	if (ri.ri_majrev >= C0_MAJREVMIN_R4400)
		return;
	for (pi = sw_cachesynch_patch_locs_R4000,
		 psrc = sw_cachesynch_patch_insts_R4000;
		(pdst = *pi) != NULL; pi++,psrc++)
		pdst[0] = psrc[0]; 
#endif
}
#endif
