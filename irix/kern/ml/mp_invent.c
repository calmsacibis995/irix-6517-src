/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992-1994 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/*
 * Misc support routines for multiprocessor inventory operations.
 */

#ident "$Revision: 1.10 $"

#include <sys/types.h>
#include <sys/sema.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/pda.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/immu.h>
#include <sys/sbd.h>
#include <sys/sysinfo.h>
#include <sys/debug.h>
#include <sys/invent.h>

void
add_sidcache(void)
{
        register int scache;
        register pda_t *pda;
        register int i;

        scache = pdaindr[master_procid].pda->p_scachesize;
        for (i=0; i<maxcpus; i++) {
		if (!cpu_enabled(i))
			continue;
                if (pdaindr[i].CpuId != i)
                        continue;
                if (pdaindr[i].pda->p_scachesize != scache)
                        break;
        }

        /*
         * all processors have same 2nd cache size ?
         * if so, set unit = -1 to tell hinv.
         */
        if (i >= maxcpus) {
                add_to_inventory(INV_MEMORY, INV_SIDCACHE, 0, -1,
                                        scache);
                return;
        }

        /*
         * at this point, we have cpus with different 2nd cache sizes -
         * insert one record for each cpu.
         */
#ifdef COMMENT
        for (i = maxcpus-1; i>=0; i--) {
#else
        for (i = 0; i < maxcpus; i++) {
#endif
                if (!cpu_enabled(i) || pdaindr[i].pda->p_cpuid != i)
                        continue;

                pda = pdaindr[i].pda;
                add_to_inventory(INV_MEMORY, INV_SIDCACHE,
                                pda->p_cpuid, 0, pda->p_scachesize);
        }
}

#ifdef SN

#include <sys/iograph.h>
#include <sys/hwgraph.h>

/* Try to see if there is detailed inventory information hanging off a cpu vertex
 * in the hwgraph. If so read the cpufrequency from that information otherwise
 * pick the default frequency which is passed in to this routine.
 */
int
get_cpu_freq(pda_t *pda,int default_freq)
{
	invent_cpuinfo_t	*cpu_info = 0;

	/* Check if there is a detailed inventory info hanging
	 * off the cpu vertex
	 */
	hwgraph_info_get_LBL(pda->p_vertex,INFO_LBL_DETAIL_INVENT,
			     (arbitrary_info_t *)&cpu_info);

	/* If there is detailed info grab the exact cpu frequency 
	 * from this
	 */
	if (cpu_info)
		return cpu_info->ic_cpu_info.cpufq;	
	return default_freq;
}
#endif
/*
 * Add CPU to hardware inventory. Called from main()
 */
void
add_cpuboard(void)
{
	register int i;
	register pda_t *pda;
	register int cputype_word, fputype_word, freq;
	int fpucpu_samerev;
#if IP21
        for (i=0; i<maxcpus; i++) {
                if (pdaindr[i].CpuId != i)
                        continue;
		if ((pdaindr[i].pda->p_cputype_word & 0xff) < 0x22)
			cmn_err(CE_WARN, "CPU %d is downrev. Not for customer use.", i);
	}
#endif

	add_sidcache();
        freq = pdaindr[master_procid].pda->cpufreq;
	cputype_word = pdaindr[master_procid].pda->p_cputype_word;
        for (i=0; i<maxcpus; i++) {
                if (pdaindr[i].CpuId != i)
                        continue;
                if (pdaindr[i].pda->cpufreq != freq)
                        break;
                if (pdaindr[i].pda->p_cputype_word != cputype_word)
                        break;
	}

	/* all processors have same speed and rev level.
	 * just add one record, use -1 for unit, hinv cmd will understand
	 */

	if (i>=maxcpus) {
		cputype_word = private.p_cputype_word;
		fputype_word = private.p_fputype_word;
#ifdef COMMENT
		add_to_inventory(INV_PROCESSOR, INV_FPUCHIP, cpuid(),0,
				 fputype_word);
		add_to_inventory(INV_PROCESSOR, INV_CPUCHIP, cpuid(),0,
				 cputype_word);
#endif
		add_to_inventory(INV_PROCESSOR,INV_CPUBOARD,
#if R4000 || R10000
#ifdef SN0
				 get_cpu_freq(pdaindr[master_procid].pda,
					      2 *  private.cpufreq),
#else
				 2 *  private.cpufreq,
#endif /* SN0 */
#endif
#if TFP || PSEUDO_BEAST
				 private.cpufreq,
#endif
				-1, cpuboard());

		add_to_inventory(INV_PROCESSOR, INV_CPUCHIP, cpuid(),0,
				 cputype_word);

		add_to_inventory(INV_PROCESSOR, INV_FPUCHIP, cpuid(),0,
				 fputype_word);

		return;
	}

	cputype_word = pdaindr[master_procid].pda->p_cputype_word;
	fputype_word = pdaindr[master_procid].pda->p_fputype_word;

	fpucpu_samerev = 1;
	for (i=0; i<maxcpus; i++) {
		if (!cpu_isvalid(i))
			continue;
		if (pdaindr[i].pda->p_cputype_word  != cputype_word ||
		    pdaindr[i].pda->p_fputype_word  != fputype_word) {
			fpucpu_samerev = 0;
			break;
		}
	}

	/* if all fpu/cpu have same rev then just add one record */
	if (fpucpu_samerev) {
		add_to_inventory(INV_PROCESSOR, INV_CPUCHIP,
				 private.p_cpuid, 0,
				 private.p_cputype_word);
		add_to_inventory(INV_PROCESSOR, INV_FPUCHIP,
				 private.p_cpuid, 0,
				 private.p_fputype_word);
	}

	/* at this point, we have cpus with different speed or rev level */
#ifdef COMMENT
	for (i = maxcpus-1; i>=0; i--) {
#else
	for (i = 0; i < maxcpus; i++) {
#endif
		if (!pdaindr[i].pda || 
		    (pdaindr[i].pda->p_cpuid != i) || !cpu_enabled(i))
			continue;

		pda = pdaindr[i].pda;
#ifdef COMMENT
		/* must add CPUCHIP, FPUCHIP right before CPUBOARD */
		if (!fpucpu_samerev) {
			add_to_inventory(INV_PROCESSOR, INV_CPUCHIP,
					pda->p_cpuid, 0, pda->p_cputype_word);
			add_to_inventory(INV_PROCESSOR, INV_FPUCHIP,
					 pda->p_cpuid, 0, pda->p_fputype_word);

		}
#endif
		/* for type INV_CPUBOARD, ctrl is CPU board type, unit */
		/* is cpu id */
		/* Double the hinv cpu frequency for R4000s, R4400s */
		add_to_inventory(INV_PROCESSOR, INV_CPUBOARD,
#if R4000 || R10000
#ifdef SN0
				 get_cpu_freq(pdaindr[i].pda,
					      2 * pdaindr[i].pda->cpufreq),
#else
				 2 * pdaindr[i].pda->cpufreq,
#endif /* SN0 */

#endif
#if TFP || PSEUDO_BEAST
				 pdaindr[i].pda->cpufreq,
#endif
				pda->p_cpuid, cpuboard());
		/* must add CPUCHIP, FPUCHIP right before CPUBOARD */
		if (!fpucpu_samerev) {
			add_to_inventory(INV_PROCESSOR, INV_CPUCHIP,
					pda->p_cpuid, 0, pda->p_cputype_word);
			add_to_inventory(INV_PROCESSOR, INV_FPUCHIP,
					 pda->p_cpuid, 0, pda->p_fputype_word);

		}

	}
}
