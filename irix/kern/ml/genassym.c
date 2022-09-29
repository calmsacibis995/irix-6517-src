/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1996, Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#include <stdio.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/cmn_err.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/sysmacros.h>
#include <sys/pcb.h>
#include <sys/reg.h>
#include <sys/syscall.h>
#include <ksys/exception.h>
#include <sys/kthread.h>
#include <ksys/xthread.h>
#include <sys/proc.h>
#include <sys/loaddrs.h>
#include <sys/errno.h>
#include <sys/fpu.h>
#include <sys/softfp.h>
#include <sys/pda.h>
#include <os/as/pmap.h>
#include <sys/sema_private.h>
#include <sys/callo.h>
#include <sys/calloinfo.h>
#include <sys/time.h>
#include <sys/cpu.h>
#include <sys/profil.h>
#include <sys/splock.h>
#include <sys/var.h>
#include <sys/arcs/spb.h>
#include <sys/arcs/debug_block.h>
#if EVEREST
#include <sys/EVEREST/everest.h>
#endif /* EVEREST */
#if IP22 || IP26 || IP28
#include <sys/eisa.h>
#endif
#if IP20 || IP22
#include <sys/mc.h>
#endif
#ifdef ULI
#define __SYS_SYSTM_H__
#include <ksys/uli.h>
#ifdef _KERNEL
#undef _KERNEL
#endif
#include <sys/syssgi.h>
#endif
#include <sys/par.h>
#include <sys/kusharena.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>

#if defined(SN)
#include <sys/nodepda.h>
#endif
#if defined(EVEREST) && defined(MULTIKERNEL)
#include <sys/EVEREST/evconfig.h>
#endif

#define HEXFORMAT	16
#define DECFORMAT	10
#define NULLFORMAT	0
#define ENDFORMAT	-1
#define PHEX(str, val)		str, val, HEXFORMAT,
#define PDEC(str, val)		str, val, DECFORMAT,
#define PNULLVAL(str)		str, 0, NULLFORMAT,
#define PEND()			"DONTCARE", 0, ENDFORMAT,

/*
** this program generates the needed defines for the assembly language
** files.  This is done so that the nasty '#ifdef LOCORE' that
** is rampant through many include files does not have to be done for
** this port.
*/

struct {
	char *name;
	size_t v;
	int format;
} addrs[] = {

	/* 
	** ARCS debug addresses 
	*/
	PHEX("SPB_DEBUGADDR", SPB_DEBUGADDR)
	PHEX("DB_BPOFF", DB_BPOFF)

	/*
	** various offsets and defines for the ublock
	*/
	PHEX("U_EFRAME", offsetof(struct exception, u_eframe))
	PHEX("U_PCB", offsetof(struct exception, u_pcb))

#if !NO_WIRED_SEGMENTS
	PHEX("U_UBPTBL", offsetof(struct exception, u_ubptbl[0]))
	PHEX("U_TLBHI_TBL", offsetof(struct exception, u_tlbhi_tbl[0]))

	/* defines for the user's wired entries corresponding to
	 * the exception page and kernel stack entries
	 */
#if R4000 || R10000
#if TLBKSLOTS == 1
	PHEX("U_USLKSTKLO", offsetof(struct exception, u_ubptbl[UKSTKTLBINDEX-TLBWIREDBASE].pde_low))
	PHEX("U_USLKSTKLO_1", offsetof(struct exception, u_ubptbl[UKSTKTLBINDEX-TLBWIREDBASE].pde_hi))
	PHEX("U_USLKSTKHI", offsetof(struct exception, u_tlbhi_tbl[UKSTKTLBINDEX-TLBWIREDBASE]))
#endif
	PHEX("U_NEXTTLB", offsetof(struct exception, u_nexttlb))
#elif BEAST /* !R4000 || R10000 */
#if TLBKSLOTS == 1
	PHEX("U_USLKSTKLO", offsetof(struct exception, u_ubptbl[UKSTKTLBINDEX-TLBWIREDBASE]))
	PHEX("U_USLKSTKHI", offsetof(struct exception, u_tlbhi_tbl[UKSTKTLBINDEX-TLBWIREDBASE]))
#endif
	PHEX("U_NEXTTLB", offsetof(struct exception, u_nexttlb))
#endif /* !BEAST */

#endif	/* !NO_WIRED_SEGMENTS */

	PHEX("PDAPAGE", (size_t)PDAPAGE)

	/*
	** the nofault index defines
	*/
	PDEC("NF_BADADDR", NF_BADADDR)
	PDEC("NF_COPYIO", NF_COPYIO)
	PDEC("NF_ADDUPC", NF_ADDUPC)
	PDEC("NF_FSUMEM", NF_FSUMEM)
	PDEC("NF_BZERO", NF_BZERO)
	PDEC("NF_SOFTFP", NF_SOFTFP)
	PDEC("NF_REVID", NF_REVID)
	PDEC("NF_SOFTFPI", NF_SOFTFPI)
	PDEC("NF_EARLYBADADDR", NF_EARLYBADADDR)
	PDEC("NF_IDBG", NF_IDBG)
	PDEC("NF_FIXADE", NF_FIXADE)
	PDEC("NF_SUERROR", NF_SUERROR)
	PDEC("NF_BADVADDR", NF_BADVADDR)
	PDEC("NF_DUMPCOPY", NF_DUMPCOPY)
	/*
	** various offsets and defines for the kthread structure
	*/
	PHEX("K_EFRAME", offsetof(struct kthread, k_eframe))
	PHEX("K_STACK", offsetof(struct kthread, k_stack))
	PHEX("K_STACKSIZE", offsetof(struct kthread, k_stacksize))
	PHEX("K_NOFAULT", offsetof(struct kthread, k_nofault))
	PHEX("K_SQSELF", offsetof(struct kthread, k_sqself))
	PDEC("KT_XTHREAD", KT_XTHREAD)
	PHEX("K_TYPE", offsetof(struct kthread, k_type))
#if CELL
	PHEX("K_BLA_LOCKP", offsetof(struct kthread, k_bla.kb_lockp))
#endif

	/*
	** various offsets and defines for the xthread_s structure
	*/
#if CELL
	PHEX("X_INFO", offsetof(struct xthread_s, xt_info))
#endif

	/*
	** various offsets and defines for the proc structure
	** and uthread structure
	*/
	PDEC("P_FP_SIGINTR1", P_FP_SIGINTR1)
	PDEC("P_FP_SIGINTR2", P_FP_SIGINTR2)
	PHEX("P_FP_IMPRECISE_EXCP", P_FP_IMPRECISE_EXCP)
	PHEX("PSEG_TRILEVEL", PSEG_TRILEVEL)
	PHEX("PRXY_FPFLAGS", offsetof(struct proc_proxy_s, prxy_fp)
				+ offsetof(struct proc_fp_s, pfp_fpflags))
	PHEX("PRXY_FP", offsetof(struct proc_proxy_s, prxy_fp)
				+ offsetof(struct proc_fp_s, pfp_fp))
	PHEX("PRXY_ABI", offsetof(struct proc_proxy_s, prxy_abi))

	PHEX("UT_PPROXY", offsetof(struct uthread_s, ut_pproxy))
	PHEX("UT_EPCINST", offsetof(struct uthread_s, ut_epcinst))
	PHEX("UT_BDINST", offsetof(struct uthread_s, ut_bdinst))
	PHEX("UT_UKSTK", offsetof(struct uthread_s, ut_kstkpgs[KSTKIDX]))
	PHEX("UT_EXCEPTION", offsetof(struct uthread_s, ut_exception))
	PHEX("UT_FLAGS", offsetof(struct uthread_s, ut_flags))
	PHEX("UT_RSA_RUNABLE", offsetof(struct uthread_s, ut_rsa_runable))
	PHEX("UTAS_SEGTBL", offsetof(struct uthread_s, ut_as.utas_segtbl_wired))
	PHEX("UTAS_SHRDSEGTBL", offsetof(struct uthread_s, ut_as.utas_shrdsegtbl_wired))
	PHEX("UTAS_SEGFLAGS", offsetof(struct uthread_s, ut_as.utas_segflags))

#ifdef USE_PTHREAD_RSA
	PHEX("UT_SHARENA", offsetof(struct uthread_s, ut_sharena))
	PHEX("UT_MAXRSAID", offsetof(struct uthread_s, ut_maxrsaid))
	PHEX("UT_RSA_LOCORE", offsetof(struct uthread_s, ut_rsa_locore))
	PHEX("UT_PRDA", offsetof(struct uthread_s, ut_prda))
	  
	PHEX("PRDA_NID", offsetof(struct prda, sys_prda.prda_sys.t_nid))
	PHEX("KUS_RSA", offsetof(struct kusharena, rsa))
	PHEX("KUS_NIDTORSA", offsetof(struct kusharena, nid_to_rsaid))
	PDEC("RSA_SIZE", sizeof(rsa_t))
	PDEC("PADDED_RSA_SIZE", sizeof(padded_rsa_t))
	PDEC("NT_MAXNIDS", NT_MAXNIDS )
	PDEC("NT_MAXRSAS", NT_MAXRSAS )

	PHEX("RSA_FPC_CSR",offsetof(struct rsa, rsa_context.__fpregs.__fp_csr))
	PHEX("RSA_FPREGS", offsetof(struct rsa, rsa_context.__fpregs))
	PDEC("RSA_A0", offsetof(struct rsa, rsa_context.__gregs[CTX_A0]))
	PDEC("RSA_A1", offsetof(struct rsa, rsa_context.__gregs[CTX_A1]))
	PDEC("RSA_A2", offsetof(struct rsa, rsa_context.__gregs[CTX_A2]))
	PDEC("RSA_A3", offsetof(struct rsa, rsa_context.__gregs[CTX_A3]))
	PDEC("RSA_AT", offsetof(struct rsa, rsa_context.__gregs[CTX_AT]))
	PDEC("RSA_FP", offsetof(struct rsa, rsa_context.__gregs[CTX_S8]))
	PDEC("RSA_GP", offsetof(struct rsa, rsa_context.__gregs[CTX_GP]))
	PDEC("RSA_RA", offsetof(struct rsa, rsa_context.__gregs[CTX_RA]))
	PDEC("RSA_S0", offsetof(struct rsa, rsa_context.__gregs[CTX_S0]))
	PDEC("RSA_S1", offsetof(struct rsa, rsa_context.__gregs[CTX_S1]))
	PDEC("RSA_S2", offsetof(struct rsa, rsa_context.__gregs[CTX_S2]))
	PDEC("RSA_S3", offsetof(struct rsa, rsa_context.__gregs[CTX_S3]))
	PDEC("RSA_S4", offsetof(struct rsa, rsa_context.__gregs[CTX_S4]))
	PDEC("RSA_S5", offsetof(struct rsa, rsa_context.__gregs[CTX_S5]))
	PDEC("RSA_S6", offsetof(struct rsa, rsa_context.__gregs[CTX_S6]))
	PDEC("RSA_S7", offsetof(struct rsa, rsa_context.__gregs[CTX_S7]))
	PDEC("RSA_SP", offsetof(struct rsa, rsa_context.__gregs[CTX_SP]))
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	PDEC("RSA_T0", offsetof(struct rsa, rsa_context.__gregs[CTX_T0]))
	PDEC("RSA_T1", offsetof(struct rsa, rsa_context.__gregs[CTX_T1]))
	PDEC("RSA_T2", offsetof(struct rsa, rsa_context.__gregs[CTX_T2]))
	PDEC("RSA_T3", offsetof(struct rsa, rsa_context.__gregs[CTX_T3]))
	PDEC("RSA_T4", offsetof(struct rsa, rsa_context.__gregs[CTX_T4]))
	PDEC("RSA_T5", offsetof(struct rsa, rsa_context.__gregs[CTX_T5]))
	PDEC("RSA_T6", offsetof(struct rsa, rsa_context.__gregs[CTX_T6]))
	PDEC("RSA_T7", offsetof(struct rsa, rsa_context.__gregs[CTX_T7]))
#endif
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	PDEC("RSA_A4", offsetof(struct rsa, rsa_context.__gregs[CTX_A4]))
	PDEC("RSA_A5", offsetof(struct rsa, rsa_context.__gregs[CTX_A5]))
	PDEC("RSA_A6", offsetof(struct rsa, rsa_context.__gregs[CTX_A6]))
	PDEC("RSA_A7", offsetof(struct rsa, rsa_context.__gregs[CTX_A7]))
	PDEC("RSA_T0", offsetof(struct rsa, rsa_context.__gregs[CTX_T0]))
	PDEC("RSA_T1", offsetof(struct rsa, rsa_context.__gregs[CTX_T1]))
	PDEC("RSA_T2", offsetof(struct rsa, rsa_context.__gregs[CTX_T2]))
	PDEC("RSA_T3", offsetof(struct rsa, rsa_context.__gregs[CTX_T3]))
#endif
	PDEC("RSA_T8", offsetof(struct rsa, rsa_context.__gregs[CTX_T8]))
	PDEC("RSA_T9", offsetof(struct rsa, rsa_context.__gregs[CTX_T9]))
	PDEC("RSA_V0", offsetof(struct rsa, rsa_context.__gregs[CTX_V0]))
	PDEC("RSA_V1", offsetof(struct rsa, rsa_context.__gregs[CTX_V1]))

	PDEC("RSA_MDHI", offsetof(struct rsa, rsa_context.__gregs[CTX_MDHI]))
	PDEC("RSA_MDLO", offsetof(struct rsa, rsa_context.__gregs[CTX_MDLO]))
	PDEC("RSA_EPC", offsetof(struct rsa, rsa_context.__gregs[CTX_EPC]))
#endif /* USE_PTHREAD_RSA */

#if TFP
	PHEX("EF_FP4", offsetof(eframe_t, ef_fp4))
#endif

#if defined(IP20) || defined(IP22) || defined(IP28) || defined(SN) || defined(IPMHSIM)
	PHEX("CAUSE_BERRINTR", CAUSE_IP7)
#endif
#if IP32
	PHEX("CAUSE_BERRINTR", CRM_INT_MEMERR|CRM_INT_CRMERR|CRM_INT_MACE(7))
#endif

	/*
	** offsets for various fields in the PCB
	*/
	PDEC("PCB_FP", PCB_FP)
	PDEC("PCB_PC", PCB_PC)
	PDEC("PCB_S0", PCB_S0)
	PDEC("PCB_S1", PCB_S1)
	PDEC("PCB_S2", PCB_S2)
	PDEC("PCB_S3", PCB_S3)
	PDEC("PCB_S4", PCB_S4)
	PDEC("PCB_S5", PCB_S5)
	PDEC("PCB_S6", PCB_S6)
	PDEC("PCB_S7", PCB_S7)
	PDEC("PCB_SP", PCB_SP)
	PDEC("PCB_SR", PCB_SR)
	PHEX("PCB_FPREGS", offsetof(struct pcb, pcb_fpregs))
	PDEC("PCB_OWNEDFP", offsetof(struct pcb, pcb_ownedfp))
	PDEC("PCB_FPC_CSR", offsetof(struct pcb, pcb_fpc_csr))
	PDEC("PCB_FPC_EIR", offsetof(struct pcb, pcb_fpc_eir))
	PDEC("PCB_BD_EPC", offsetof(struct pcb, pcb_bd_epc))
	PDEC("PCB_BD_CAUSE", offsetof(struct pcb, pcb_bd_cause))
	PDEC("PCB_BD_RA", offsetof(struct pcb, pcb_bd_ra))
	PDEC("PCB_BD_INSTR", offsetof(struct pcb, pcb_bd_instr))
	PDEC("PCB_RESCHED", offsetof(struct pcb, pcb_resched))

	/*
	** offsets to the various registers in a jump buffer
	*/
	PDEC("JB_FP", JB_FP)
	PDEC("JB_PC", JB_PC)
	PDEC("JB_S0", JB_S0)
	PDEC("JB_S1", JB_S1)
	PDEC("JB_S2", JB_S2)
	PDEC("JB_S3", JB_S3)
	PDEC("JB_S4", JB_S4)
	PDEC("JB_S5", JB_S5)
	PDEC("JB_S6", JB_S6)
	PDEC("JB_S7", JB_S7)
	PDEC("JB_SP", JB_SP)
	PDEC("JB_SR", JB_SR)
#if EVEREST
	PDEC("JB_CEL", JB_CEL)
#endif

	/*
	 * Various relative offsets for profiling info in the prof structure
	 */
	PHEX("PR_BASE", offsetof(struct prof, pr_base))
	PHEX("PR_OFF", offsetof(struct prof, pr_off))
	PHEX("PR_SCALE", offsetof(struct prof, pr_scale))
	PHEX("PR_SIZE", offsetof(struct prof, pr_size))

	/*
	** defines for the exception frame offsets for registers
	*/
	PDEC("EF_A0", offsetof(eframe_t, ef_a0))
	PDEC("EF_A1", offsetof(eframe_t, ef_a1))
	PDEC("EF_A2", offsetof(eframe_t, ef_a2))
	PDEC("EF_A3", offsetof(eframe_t, ef_a3))
	PDEC("EF_AT", offsetof(eframe_t, ef_at))
	PDEC("EF_BADVADDR", offsetof(eframe_t, ef_badvaddr))
	PDEC("EF_CAUSE", offsetof(eframe_t, ef_cause))
	PDEC("EF_EPC", offsetof(eframe_t, ef_epc))
	PDEC("EF_EXACT_EPC", offsetof(eframe_t, ef_exact_epc))
	PDEC("EF_FP", offsetof(eframe_t, ef_fp))
	PDEC("EF_GP", offsetof(eframe_t, ef_gp))
	PDEC("EF_K0", offsetof(eframe_t, ef_k0))
	PDEC("EF_K1", offsetof(eframe_t, ef_k1))
	PDEC("EF_MDHI", offsetof(eframe_t, ef_mdhi))
	PDEC("EF_MDLO", offsetof(eframe_t, ef_mdlo))
	PDEC("EF_RA", offsetof(eframe_t, ef_ra))
	PDEC("EF_S0", offsetof(eframe_t, ef_s0))
	PDEC("EF_S1", offsetof(eframe_t, ef_s1))
	PDEC("EF_S2", offsetof(eframe_t, ef_s2))
	PDEC("EF_S3", offsetof(eframe_t, ef_s3))
	PDEC("EF_S4", offsetof(eframe_t, ef_s4))
	PDEC("EF_S5", offsetof(eframe_t, ef_s5))
	PDEC("EF_S6", offsetof(eframe_t, ef_s6))
	PDEC("EF_S7", offsetof(eframe_t, ef_s7))
	PDEC("EF_SIZE", sizeof(eframe_t))
	PDEC("EF_SP", offsetof(eframe_t, ef_sp))
	PDEC("EF_SR", offsetof(eframe_t, ef_sr))
	PDEC("EF_CONFIG", offsetof(eframe_t, ef_config))
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
	PDEC("EF_T0", offsetof(eframe_t, ef_t0))
	PDEC("EF_T1", offsetof(eframe_t, ef_t1))
	PDEC("EF_T2", offsetof(eframe_t, ef_t2))
	PDEC("EF_T3", offsetof(eframe_t, ef_t3))
	PDEC("EF_T4", offsetof(eframe_t, ef_t4))
	PDEC("EF_T5", offsetof(eframe_t, ef_t5))
	PDEC("EF_T6", offsetof(eframe_t, ef_t6))
	PDEC("EF_T7", offsetof(eframe_t, ef_t7))
#endif
#if (_MIPS_SIM == _MIPS_SIM_ABI64 || _MIPS_SIM == _MIPS_SIM_NABI32)
	PDEC("EF_A4", offsetof(eframe_t, ef_a4))
	PDEC("EF_A5", offsetof(eframe_t, ef_a5))
	PDEC("EF_A6", offsetof(eframe_t, ef_a6))
	PDEC("EF_A7", offsetof(eframe_t, ef_a7))
	PDEC("EF_T0", offsetof(eframe_t, ef_t0))
	PDEC("EF_T1", offsetof(eframe_t, ef_t1))
	PDEC("EF_T2", offsetof(eframe_t, ef_t2))
	PDEC("EF_T3", offsetof(eframe_t, ef_t3))
#endif
	PDEC("EF_T8", offsetof(eframe_t, ef_t8))
	PDEC("EF_T9", offsetof(eframe_t, ef_t9))
	PDEC("EF_V0", offsetof(eframe_t, ef_v0))
	PDEC("EF_V1", offsetof(eframe_t, ef_v1))
#if EVEREST
	PDEC("EF_CEL", offsetof(eframe_t, ef_cel))
#endif
#if IP32
	PDEC("EF_CRMMSK", offsetof(eframe_t, ef_crmmsk))
#endif
	PDEC("EF_CPUID", offsetof(eframe_t, ef_cpuid))
	PDEC("EF_BUSERR_INFO", offsetof(eframe_t, ef_buserr_info))
	PDEC("EF_BUSERR_SPL", offsetof(eframe_t, ef_buserr_spl))

	/*
	** tlb defines
	*/
#if defined(R4000) && !defined(EVEREST)
	PHEX("TLBLO_HWBITS", TLBLO_HWBITS)
	PHEX("TLBLO_HWBITSHIFT", TLBLO_HWBITSHIFT)
	PDEC("TLBLO_PFNTOKDMSHFT", TLBLO_PFNTOKDMSHFT)
#endif

#if IP26
	PHEX("TLBLO_HWBITSHIFT", TLBLO_HWBITSHIFT)
#endif

#if IP30
	PHEX("MAXCPU",MAXCPU)
	PHEX("IOC3_PCI_DEVIO_K1PTR",IOC3_PCI_DEVIO_K1PTR)
	PHEX("IOC3_GPCR_C",IOC3_GPCR_C)
	PHEX("GPCR_INT_OUT_EN",GPCR_INT_OUT_EN)
#endif

	/*
	** misc defines
	*/
	PDEC("NBPW", sizeof(int))
	PHEX("USRCODE", USRCODE)
	PHEX("USRDATA", USRDATA)
	PDEC("NBPDE", sizeof (pde_t))
	PDEC("PDESHFT", PDESHFT)
	PDEC("PTE_PFNSHFT", PTE_PFNSHFT)
	PHEX("SHRD_SENTINEL", SHRD_SENTINEL)
#if R4000 || R10000
#ifdef MCCHIP
	PHEX("TLBLO_SHRD_SENTINEL", SHRD_SENTINEL)
#else
	PHEX("TLBLO_SHRD_SENTINEL", SHRD_SENTINEL>>TLBLO_HWBITSHIFT)
#endif
#endif

#if defined (PTE_64BIT) && (defined (R4000) || defined (R10000))
	PDEC("PAGE_MASK_INDEX_SHIFT", PAGE_MASK_INDEX_SHIFT)
	PHEX("PAGE_MASK_INDEX_BITMASK", PAGE_MASK_INDEX_BITMASK)
#endif

	PDEC("SIZEOF_CPUMASK", sizeof(cpumask_t))
	PHEX("PG_ADDR", PG_ADDR)
	PDEC("PCOM_TSHIFT", PCOM_TSHIFT)
	PHEX("PG_NONCOHRNT", PG_NONCOHRNT)
	PHEX("PG_COHRNT_EXLWR", PG_COHRNT_EXLWR)
	PHEX("PG_M", PG_M)
	PHEX("PG_G", PG_G)
	PHEX("PG_VR", PG_VR)
	PHEX("PG_NDREF", PG_NDREF)
	PHEX("PG_SV", PG_SV)
	PDEC("NBPP", NBPP)
	PHEX("POFFMASK", POFFMASK)
	PDEC("PIDLE", PIDLE)
	PDEC("PNUMSHFT", PNUMSHFT)
	PDEC("MIN_PNUMSHFT", MIN_PNUMSHFT)
   	PDEC("MIN_POFFMASK", MIN_POFFMASK)
	PDEC("CPSSHIFT", CPSSHIFT)
	PDEC("BASETABSHIFT", BASETABSHIFT)
	PDEC("BASETABSIZE", BASETABSIZE)
	PHEX("SEGTABMASK", SEGTABMASK)
	PHEX("PTOFFMASK", PTOFFMASK)
	PDEC("PGFCTR", PGFCTR)
	PDEC("PGSHFTFCTR", PGSHFTFCTR)
	PHEX("KERNELSTACK", (size_t)KERNELSTACK)
	PHEX("KSTACKPAGE", (size_t)KSTACKPAGE)
	PHEX("PRDAADDR", PRDAADDR)
	PHEX("KSTKIDX", KSTKIDX)
	PHEX("KPTEBASE", KPTEBASE)
	PHEX("KPTE_SHDUBASE", KPTE_SHDUBASE)
	PHEX("KPTE_USIZE", KPTE_USIZE)
	PDEC("KPTE_TLBPRESHIFT", KPTE_TLBPRESHIFT)
	PHEX("KPTE_KBASE", KPTE_KBASE)
#if (R4000 || R10000) && _MIPS_SIM == _ABI64
	PHEX("KPTE_REGIONMASK", KPTE_REGIONMASK)
	PHEX("KPTE_REGIONSHIFT", KPTE_REGIONSHIFT)
	PHEX("KPTE_VPNMASK", KPTE_VPNMASK)
#endif
#ifdef R10000
	PHEX("FRAMEMASK_MASK", FRAMEMASK_MASK)
#endif
	PDEC("PDATLBINDEX", PDATLBINDEX)

#if TLBKSLOTS == 1 && (R4000 || R10000)
	PDEC("UKSTKTLBINDEX", UKSTKTLBINDEX)
	PDEC("TLBKSLOTS", TLBKSLOTS)
#endif /* TLBKSLOTS */

#if TFP
	PDEC("TFP_UKSTKTLBINDEX", TLBINDEX(KSTACKPAGE, 0))
#endif	/* TFP */

	/* Some cache flushing flags */

	PHEX("CACH_DCACHE", CACH_DCACHE)
	PHEX("CACH_INVAL", CACH_INVAL)
	PHEX("CACH_WBACK", CACH_WBACK)
	PHEX("CACH_IO_COHERENCY", CACH_IO_COHERENCY)

	PDEC("SYS_exece", SYS_execve)
	PDEC("SYS_exit", SYS_exit)
	PDEC("CE_PANIC", CE_PANIC)
	PDEC("EFAULT", EFAULT)
	PDEC("BPCSHIFT", BPCSHIFT)
	PHEX("MAXHIUSRATTACH", MAXHIUSRATTACH)

#ifdef MCCHIP
	PDEC("MCCHIP", 1)
	PHEX("SEG0_BASE", SEG0_BASE)
	PHEX("CPU_ERR_STAT", CPU_ERR_STAT)
	PHEX("GIO_ERR_STAT", GIO_ERR_STAT)
	PHEX("GIO_ERR_STAT_TIME", GIO_ERR_STAT_TIME)
	PHEX("GIO_ERR_STAT_PROM", GIO_ERR_STAT_PROM)
	PHEX("CPU_ERR_ADDR", CPU_ERR_ADDR)
	PHEX("GIO_ERR_ADDR", GIO_ERR_ADDR)
	PHEX("CPUCTRL0", CPUCTRL0)
	PHEX("LB_TIME", LB_TIME)
	PHEX("CPU_TIME", CPU_TIME)
	PHEX("CPU_MEMACC", CPU_MEMACC)
#if IP20
	PHEX("LIO_0_MASK_ADDR", LIO_0_MASK_ADDR)
	PHEX("LIO_0_ISR_ADDR", LIO_0_ISR_ADDR)
#elif IP22 || IP26  || IP28
	PHEX("LIO_0_MASK_OFFSET", LIO_0_MASK_OFFSET)
	PHEX("LIO_0_ISR_OFFSET", LIO_0_ISR_OFFSET)
#endif
	PHEX("LIO_FIFO", LIO_FIFO)
	PHEX("K0_RAMBASE", K0_RAMBASE)
	PHEX("DMA_MEMADR", DMA_MEMADR)
	PHEX("DMA_MODE", DMA_MODE)
	PHEX("DMA_SIZE", DMA_SIZE)
	PHEX("DMA_STRIDE", DMA_STRIDE)
#ifdef IP28
	PHEX("RT_CLOCK_ADDR", (__psunsigned_t)RT_CLOCK_ADDR)
	PHEX("MEMCFG1",MEMCFG1)
	PHEX("ECC_MEMCFG",ECC_MEMCFG)
	PHEX("ECC_DEFAULT",ECC_DEFAULT)
	PHEX("ECC_CTRL_BASE",ECC_CTRL_BASE)
#endif
#endif	/* MCCHIP */
#if IPMHSIM
	PHEX("K0_RAMBASE", K0_RAMBASE)
	PHEX("CPUCTRL0", CPUCTRL0)
	PHEX("CPU_ERR_STAT", CPU_ERR_STAT)
	PHEX("GIO_ERR_STAT", GIO_ERR_STAT)
	PHEX("CPU_ERR_ADDR", CPU_ERR_ADDR)
	PHEX("GIO_ERR_ADDR", GIO_ERR_ADDR)
#endif
#if IP22 || IP26 || IP28
	PHEX("NMIStatus", EISAIO_TO_PHYS(NMIStatus))
	PHEX("EISA_SPK_ENB", EISA_SPK_ENB)
	PHEX("HPC3_SYS_ID", HPC3_SYS_ID)
	PHEX("HPC3_PBUS_CONTROL_BASE", HPC3_PBUS_CONTROL_BASE)
	PHEX("HPC3_PBUS_CONTROL_OFFSET", HPC3_PBUS_CONTROL_OFFSET)
	PHEX("HPC3_BUSERR_STAT_ADDR", HPC3_BUSERR_STAT_ADDR)
	PHEX("HPC3_EXT_IO_ADDR", HPC3_EXT_IO_ADDR)
	PHEX("HPC3_INT2_ADDR", HPC3_INT2_ADDR)
	PHEX("HPC3_INT3_ADDR", HPC3_INT3_ADDR)
	PHEX("CHIP_REV_MASK", CHIP_REV_MASK)
	PHEX("BOARD_ID_MASK", BOARD_ID_MASK)
	PHEX("BOARD_REV_MASK", BOARD_REV_MASK)
	PHEX("BOARD_REV_SHIFT", BOARD_REV_SHIFT)
	PHEX("BOARD_IP22", BOARD_IP22)
	PHEX("BOARD_IP24", BOARD_IP24)
	PHEX("CHIP_IOC1", CHIP_IOC1)
#endif
#if IP26
	PHEX("TCC_BASE", TCC_BASE)
	PHEX("TCC_FIFO", TCC_FIFO)
	PHEX("TCC_GCACHE", TCC_GCACHE)
	PHEX("TCC_INTR", TCC_INTR)
	PHEX("TCC_ERROR", TCC_ERROR)
	PHEX("TCC_COUNT", TCC_COUNT)
	PHEX("TCC_PARITY", TCC_PARITY)
	PHEX("TCC_BE_ADDR", TCC_BE_ADDR)
	PHEX("TCC_CACHE_OP", TCC_CACHE_OP)
	PHEX("TCC_INVALIDATE", TCC_INVALIDATE)
	PHEX("TCC_DIRTY_WB", TCC_DIRTY_WB)
	PHEX("TCC_INDEX_OP", TCC_INDEX_OP)
	PHEX("TCC_CACHE_INDEX", TCC_CACHE_INDEX)
	PHEX("TCC_CACHE_INDEX_SHIFT", TCC_CACHE_INDEX_SHIFT)
	PHEX("TCC_CACHE_SET_SHIFT", TCC_CACHE_SET_SHIFT)
	PHEX("TCC_LINESIZE", TCC_LINESIZE)
	PHEX("TCC_PHYSADDR", TCC_PHYSADDR)
	PHEX("TCC_PREFETCH", TCC_PREFETCH)
	PHEX("PRE_INV", PRE_INV)
	PHEX("PRE_DEFAULT", PRE_DEFAULT)
	PHEX("INTR_FIFO_HW_EN", INTR_FIFO_HW_EN)
	PHEX("INTR_FIFO_HW", INTR_FIFO_HW)
	PHEX("INTR_FIFO_LW", INTR_FIFO_LW)
	PHEX("INTR_TIMER", INTR_TIMER)
	PHEX("INTR_BUSERROR", INTR_BUSERROR)
	PHEX("INTR_MACH_CHECK", INTR_MACH_CHECK)
	PHEX("ERROR_NESTED_BE", ERROR_NESTED_BE)
	PHEX("ERROR_NESTED_MC", ERROR_NESTED_MC)
	PHEX("GCACHE_RR_FULL", GCACHE_RR_FULL)
	PHEX("GCACHE_RR_FULL_SHIFT", GCACHE_RR_FULL_SHIFT)
	PHEX("GCACHE_WB_RESTART", GCACHE_WB_RESTART)
	PHEX("GCACHE_REL_DELAY", GCACHE_REL_DELAY)
	PHEX("ERROR_TYPE", ERROR_TYPE)
	PHEX("ERROR_TYPE_SHIFT", ERROR_TYPE_SHIFT)
	PHEX("ERROR_TBUS_UFO", ERROR_TBUS_UFO)
#endif
#if IP26 || IP28
	PHEX("IP26_ECCSYSID", IP26_ECCSYSID)
	PHEX("CPU_MEMACC_NORMAL_IP26", CPU_MEMACC_NORMAL_IP26)
	PHEX("CPU_MEMACC_NORMAL", CPU_MEMACC_NORMAL)
	PHEX("CPU_MEMACC_SLOW_IP26", CPU_MEMACC_SLOW_IP26)
	PHEX("CPU_MEMACC_SLOW", CPU_MEMACC_SLOW)
	PHEX("CPU_MEMACC_BIGALIAS", CPU_MEMACC_BIGALIAS)
	PHEX("ECC_CTRL_REG", ECC_CTRL_REG)
	PHEX("ECC_CTRL_DISABLE", ECC_CTRL_DISABLE)
#ifdef IP28
	PHEX("ECC_SPRING_BOARD", 0x500)
#endif
#endif
#if IP17
	PHEX("VME_RMW_ADDR", VME_RMW_ADDR)
	PHEX("MPBERR0_ADDR", MPBERR0_ADDR)
	PHEX("RMPBERR_CLEAR", RMPBERR_CLEAR)
	PHEX("RMI_CONTROL", RMI_CONTROL)
	PHEX("RMI_READECC", RMI_READECC)
#endif
#if HEART_CHIP
	PDEC("HEART_CHIP", 1)
	PHEX("K0_RAMBASE", K0_RAMBASE)
	PHEX("PHYS_RAMBASE", PHYS_RAMBASE)
	PHEX("SR_IBIT_PROF", SR_IBIT_PROF)
	PHEX("SR_IBIT_BERR", SR_IBIT_BERR)
	PHEX("CAUSE_BERRINTR", SR_IBIT_BERR)
	PHEX("SR_HI_MASK", SR_HI_MASK)
	PHEX("SR_SCHED_MASK", SR_SCHED_MASK)
	PHEX("SR_PROF_MASK", SR_PROF_MASK)
	PHEX("SR_ALL_MASK", SR_ALL_MASK)
	PHEX("SR_BERR_MASK", SR_BERR_MASK)
	PHEX("HEART_BASE", HEART_BASE)
	PHEX("HEART_WID_ID", HEART_WID_ID)
	PHEX("HEART_WID_ERR_CMDWORD", HEART_WID_ERR_CMDWORD)
	PHEX("HEART_WID_ERR_TYPE", HEART_WID_ERR_TYPE)
	PHEX("HEART_PIU_BASE", HEART_PIU_BASE)
	PHEX("HEART_IMR0", HEART_IMR0)
	PHEX("HEART_INT_EXC", HEART_INT_EXC)
	PHEX("HEART_INT_L4SHIFT", HEART_INT_L4SHIFT)
	PHEX("HEART_INT_CPUPBERRSHFT", HEART_INT_CPUPBERRSHFT)
	PHEX("HEART_CAUSE", HEART_CAUSE)
	PHEX("HC_WIDGET_ERR", HC_WIDGET_ERR)
	PHEX("HEART_COUNT", HEART_COUNT)
	PHEX("HEART_PRID", HEART_PRID)
	PHEX("HEART_COMPARE", HEART_COMPARE)
	PHEX("HEART_CLR_ISR", HEART_CLR_ISR)
	PHEX("HEART_INT_TIMER", HEART_INT_TIMER)
	PHEX("HEART_INT_LEVEL0",HEART_INT_LEVEL0)
	PHEX("HEART_INT_LEVEL1",HEART_INT_LEVEL1)
	PHEX("HEART_INT_LEVEL2",HEART_INT_LEVEL2)
	PHEX("HEART_INT_LEVEL4",HEART_INT_LEVEL4)
	PHEX("HEART_SYNC", HEART_SYNC)
#endif
#if IP32
	PHEX("K0_RAMBASE", K0_RAMBASE)
	PHEX("CRM_ID", CRM_ID)
	PHEX("CRM_INTMASK", CRM_INTMASK)
	PHEX("CRM_INTSTAT", CRM_INTSTAT)
	PHEX("CRM_HARDINT", CRM_HARDINT)
	PHEX("CRM_MEM_ERROR_STAT", CRM_MEM_ERROR_STAT)
	PHEX("CRM_CPU_ERROR_STAT", CRM_CPU_ERROR_STAT)
	PHEX("CRM_CONTROL", CRM_CONTROL)
	PHEX("CRM_CPU_ERROR_ADDR", CRM_CPU_ERROR_ADDR)
	PHEX("CRM_MEM_ERROR_ADDR", CRM_MEM_ERROR_ADDR)
	PHEX("CRM_VICE_ERROR_ADDR", CRM_VICE_ERROR_ADDR)
	PHEX("CRM_TIME", CRM_TIME)
	PHEX("DNS_PER_TICK", DNS_PER_TICK)
#endif

	/*
	 * CPU bootstrap stuff
	 */
	PHEX("VPDA_KSTACK", offsetof(pda_t, p_kstackflag) + (size_t)PDAADDR)
	PHEX("VPDA_INTSTACK", offsetof(pda_t, p_intstack) + (size_t)PDAADDR)
	PHEX("VPDA_LINTSTACK", offsetof(pda_t, p_intlastframe) + (size_t)PDAADDR)
	PHEX("VPDA_BOOTSTACK", offsetof(pda_t, p_bootstack) + (size_t)PDAADDR)
	PHEX("VPDA_LBOOTSTACK", offsetof(pda_t, p_bootlastframe) + (size_t)PDAADDR)
	PDEC("PDAINDRSZ",	 sizeof(pdaindr_t))
	PDEC("PDAOFF",	 offsetof(pdaindr_t, pda))
	PHEX("VPDA_CURKTHREAD", offsetof(pda_t, p_curkthread) + (size_t)PDAADDR)
	PHEX("VPDA_CURUTHREAD", offsetof(pda_t, p_curuthread) + (size_t)PDAADDR)
	PHEX("VPDA_FPOWNER", offsetof(pda_t, p_fpowner) + (size_t)PDAADDR)
	PHEX("VPDA_NOFAULT", offsetof(pda_t, p_nofault) + (size_t)PDAADDR)
	PHEX("VPDA_ATSAVE", offsetof(pda_t, p_atsave) + (size_t)PDAADDR)
	PHEX("VPDA_K1SAVE", offsetof(pda_t, p_k1save) + (size_t)PDAADDR)
	PHEX("VPDA_T0SAVE", offsetof(pda_t, p_t0save) + (size_t)PDAADDR)
#if SW_FAST_CACHE_SYNCH
	PHEX("VPDA_SWINSAVE_TMP", offsetof(pda_t, p_swinsave_tmp) + (size_t)PDAADDR)
#endif
#ifdef _R5000_CVT_WAR
	PHEX("VPDA_FP0SAVE", offsetof(pda_t, p_fp0save) + (size_t)PDAADDR)
#endif
	PHEX("VPDA_EXCNORM", offsetof(pda_t, common_excnorm) + (size_t)PDAADDR)
	PHEX("VPDA_CPUID", offsetof(pda_t, p_cpuid) + (size_t)PDAADDR)
	PHEX("VPDA_RUNRUN", offsetof(pda_t, p_runrun) + (size_t)PDAADDR)
	PHEX("VPDA_IDLSTKDEPTH", offsetof(pda_t, p_idlstkdepth) + (size_t)PDAADDR)
	PHEX("VPDA_SWITCHING", offsetof(pda_t, p_switching) + (size_t)PDAADDR)
	PHEX("VPDA_CURLOCK", offsetof(pda_t, p_curlock) + (size_t)PDAADDR)
	PHEX("VPDA_CURLOCKCPC", offsetof(pda_t, p_curlockcpc) + (size_t)PDAADDR)
	PHEX("VPDA_LASTLOCK", offsetof(pda_t, p_lastlock) + (size_t)PDAADDR)
	PHEX("VPDA_PDALO", offsetof(pda_t, p_pdalo) + (size_t)PDAADDR)
	PHEX("VPDA_UKSTKLO", offsetof(pda_t, p_ukstklo) + (size_t)PDAADDR)
#ifdef CELL
	PHEX("VPDA_BLAPTR", offsetof(pda_t, p_blaptr) + (size_t)PDAADDR)
#endif
#ifdef	TLBDEBUG
	PHEX("VPDA_SV1LO", offsetof(pda_t, p_sv1lo) + (size_t)PDAADDR)
	PHEX("VPDA_SV1LO_1", offsetof(pda_t, p_sv1lo_1) + (size_t)PDAADDR)
	PHEX("VPDA_SV1HI", offsetof(pda_t, p_sv1hi) + (size_t)PDAADDR)
	PHEX("VPDA_SV2LO", offsetof(pda_t, p_sv2lo) + (size_t)PDAADDR)
	PHEX("VPDA_SV2LO_1", offsetof(pda_t, p_sv2lo_1) + (size_t)PDAADDR)
	PHEX("VPDA_SV2HI", offsetof(pda_t, p_sv2hi) + (size_t)PDAADDR)
#endif /* TLBDEBUG */
	PHEX("VPDA_UTLBMISSES", offsetof(pda_t, p_utlbmisses) + (size_t)PDAADDR)
	PHEX("VPDA_KTLBMISSES", offsetof(pda_t, p_ktlbmisses) + (size_t)PDAADDR)
	PHEX("VPDA_UTLBMISS", offsetof(pda_t, p_utlbmissswtch) + (size_t)PDAADDR)
	PHEX("VPDA_UTLBMISSHNDLR", offsetof(pda_t, p_utlbmisshndlr) + (size_t)PDAADDR)
#ifdef EVEREST
	PHEX("VPDA_CELHW", offsetof(pda_t, p_CEL_hw) + (size_t)PDAADDR)
#endif
#if defined(EVEREST)
	PHEX("VPDA_CELSHADOW", offsetof(pda_t, p_CEL_shadow) + (size_t)PDAADDR)
#endif
	PHEX("VPDA_INTR_RESUMEIDLE", offsetof(pda_t, p_intr_resumeidle) + (size_t)PDAADDR)
#if SN
	PHEX("VPDA_SLICE", offsetof(pda_t, p_slice) + (size_t)PDAADDR)
#if SN0
	PHEX("VPDA_MASKS0", offsetof(pda_t, p_intmasks) + (size_t)PDAADDR +
				offsetof(hub_intmasks_t, intpend0_masks))
	PHEX("VPDA_MASKS1", offsetof(pda_t, p_intmasks) + (size_t)PDAADDR +
				offsetof(hub_intmasks_t, intpend1_masks))
#endif /* SN0 */
#endif /* SN */

#if _MIPS_SIM == _MIPS_SIM_ABI64
	PHEX("VPDA_CAUSEVEC", offsetof(pda_t, p_causevec) + (size_t)PDAADDR)
#endif
#if EVEREST || SN
	PHEX("VPDA_KVFAULT", offsetof(pda_t, p_kvfault) + (size_t)PDAADDR)
#if R4000 || R10000
	PHEX("VPDA_CLRKVFLT", offsetof(pda_t, p_clrkvflt[0]) + (size_t)PDAADDR)
#endif
#endif

#if IP32
	PHEX("VPDA_CURSPL", offsetof(pda_t, p_curspl) + (size_t)PDAADDR)
	PHEX("VPDA_SPLMASKS", offsetof(pda_t, p_splmasks[0]) + (size_t)PDAADDR)
#endif

#if R4000
	PHEX("VPDA_VCELOG_OFFSET", offsetof(pda_t, p_vcelog_offset) + (size_t)PDAADDR)
	PHEX("VPDA_VCELOG", offsetof(pda_t, p_vcelog) + (size_t)PDAADDR)
	PHEX("VPDA_VCECOUNT", offsetof(pda_t, p_vcecount) + (size_t)PDAADDR)
#endif
#if EXTKSTKSIZE == 1
	/* For kernel stack extension setup */
	PHEX( "VPDA_STACKEXT", offsetof(pda_t, p_stackext) + (size_t)PDAADDR)
	PHEX( "VPDA_MAPSTACKEXT", offsetof(pda_t, p_mapstackext) + (size_t)PDAADDR)
	/* For EXTKSTKSIZE */
	PHEX( "EXTKSTKSIZE", EXTKSTKSIZE )
#endif

	/* stack manipulation */
	PHEX("PDA_CURUSRSTK", PDA_CURUSRSTK)
	PHEX("PDA_CURKERSTK", PDA_CURKERSTK)
	PHEX("PDA_CURINTSTK", PDA_CURINTSTK)
	PHEX("PDA_CURIDLSTK", PDA_CURIDLSTK)
#ifdef ULI
	PHEX("PDA_CURULISTK", PDA_CURULISTK)
#endif

	PHEX("VPDA_PSCACHESIZE", offsetof(pda_t, p_scachesize) + (size_t)PDAADDR)
#if defined (R10000)
	PHEX("VPDA_R10KWAR_BITS", offsetof(pda_t, p_r10kwar_bits) + (size_t)PDAADDR)
#endif /* R10000 */
#if defined (R10000) && defined (R10000_MFHI_WAR)

	PHEX("VPDA_MFHI_BRCNT", offsetof(pda_t, p_mfhi_brcnt) + (size_t)PDAADDR)
	PHEX("VPDA_MFHI_CNT", offsetof(pda_t, p_mfhi_cnt) + (size_t)PDAADDR)
	PHEX("VPDA_MFHI_SKIP", offsetof(pda_t, p_mfhi_skip) + (size_t)PDAADDR)
	PHEX("VPDA_MFHI_REG", offsetof(pda_t, p_mfhi_reg) + (size_t)PDAADDR)
	PHEX("VPDA_MFLO_REG", offsetof(pda_t, p_mflo_reg) + (size_t)PDAADDR)
	PHEX("VPDA_MFHI_PATCH_BUF", offsetof(pda_t, p_mfhi_patch_buf) + (size_t)PDAADDR)
#endif /* defined (R10000) && defined (R10000_MFHI_WAR) */

	/*
	** CLOCK STUFF
	*/
	PHEX("VPDA_PFLAGS", offsetof(pda_t, p_flags) + (size_t)PDAADDR)
	PHEX("PDAF_FASTCLOCK", PDAF_FASTCLOCK)
	PHEX("PDAF_NONPREEMPTIVE", PDAF_NONPREEMPTIVE)
	PHEX("C_NEXT", offsetof(callout_info_t, ci_todo.c_next))
	PHEX("TVUSECOFF", offsetof(struct timeval, tv_usec))
#if defined(IP20) || defined(IP22) || defined(IP26)  || defined(IP28)
	/* timer clock */
	PDEC("MODE_RG", 2)	
	PDEC("MODE_STS", 4)

	PHEX("ACK_TIMER1", ACK_TIMER1)

	PDEC("PT_COUNTER2", 11)
	PDEC("PT_CONTROL", 15)

#if defined(IP20)
	PHEX("TIMER_ACK_ADDR", TIMER_ACK_ADDR)
	PHEX("PT_CLOCK_ADDR", (__psint_t)PT_CLOCK_ADDR)
#elif defined(IP22) 
	PHEX("TIMER_ACK_OFFSET", TIMER_ACK_OFFSET)
	PHEX("PT_CLOCK_OFFSET", PT_CLOCK_OFFSET)
#elif defined(IP26) || defined(IP28)
	PHEX("TIMER_ACK_ADDR", TIMER_ACK_ADDR)
	PHEX("PT_CLOCK_OFFSET", PT_CLOCK_OFFSET)
#endif
#endif
	/* delay calibration */
	PHEX("VPDA_DECINSPERLOOP", offsetof(pda_t, decinsperloop) + (size_t)PDAADDR)

	/* mrlock */
	PHEX("MR_LBITS", offsetof(mrlock_t, mr_lbits))
	PHEX("MR_QBITS", offsetof(mrlock_t, mr_qbits))
	PHEX("MR_ACCINC", MR_ACCINC)
	PHEX("MR_ACC", MR_ACC)
	PHEX("MR_ACCMAX", MR_ACCMAX)
	PHEX("MR_UPD", MR_UPD)
	PHEX("MR_BARRIER", MR_BARRIER)

	PHEX("MR_WAIT", MR_WAIT)
	PHEX("MR_WAITINC", MR_WAITINC)

	PHEX("KT_WUPD", KT_WUPD)

#if R10000
#if !defined(IP25) && !defined(IP28) && !defined(IP30)
	/* Some stuff for the Ecc handler */
	PHEX("CACHE_ERR_EFRAME", CACHE_ERR_EFRAME)
	PHEX("CACHE_ERR_ECCFRAME", CACHE_ERR_ECCFRAME)
#endif	/* ndef IP25 */
#if defined (SN0)
	PHEX("CACHE_ERR_SP_PTR", CACHE_ERR_SP_PTR)
	PHEX("CACHE_ERR_IBASE_PTR", CACHE_ERR_IBASE_PTR)
	PHEX("CACHE_ERR_SP", CACHE_ERR_SP)
	PHEX("NPDA", (size_t)PDAADDR + offsetof(pda_t,p_nodepda))
	PHEX("NPDA_DUMP_STACK_OFFSET",offsetof(nodepda_t,dump_stack))
	PHEX("NPDA_DUMP_COUNT_OFFSET",offsetof(nodepda_t,dump_count))
#endif
	/* Cache error exception handling */
#ifndef	R4000
	PHEX("ECCF_CACHE_ERR", offsetof(eccframe_t, eccf_cache_err))
	PHEX("ECCF_TAG", offsetof(eccframe_t, eccf_tag))
	PHEX("ECCF_TAGLO", offsetof(eccframe_t, eccf_taglo))
	PHEX("ECCF_TAGHI", offsetof(eccframe_t, eccf_taghi))
	PHEX("ECCF_ECC", offsetof(eccframe_t, eccf_ecc))
	PHEX("ECCF_STATUS", offsetof(eccframe_t, eccf_status))
	PHEX("ECCF_ERROREPC", offsetof(eccframe_t, eccf_errorEPC))
#else /* R4000 & R10000 */
	/* IP32 R5k/R10k mixed kernels */
	PHEX("ECCF_R10K_CACHE_ERR", offsetof(eccframe_t, eccf_cache_err))
	PHEX("ECCF_R10K_TAGLO", offsetof(eccframe_t, eccf_taglo))
	PHEX("ECCF_R10K_ECC", offsetof(eccframe_t, eccf_ecc))
	PHEX("ECCF_R10K_ERROREPC", offsetof(eccframe_t, eccf_errorEPC))
	PHEX("ECCF_TAG", offsetof(eccframe_t, eccf_tag))
	PHEX("ECCF_TAGHI", offsetof(eccframe_t, eccf_taghi))
	PHEX("ECCF_STATUS", offsetof(eccframe_t, eccf_status))
#endif	/* !R4000 */

	/* General cacheop routine defines */
	PHEX("COP_ADDRESS", offsetof(cacheop_t, cop_address))
	PHEX("COP_OP", offsetof(cacheop_t, cop_operation))
	PHEX("COP_TAGHI", offsetof(cacheop_t, cop_taghi))
        PHEX("COP_TAGLO", offsetof(cacheop_t, cop_taglo))
	PHEX("COP_ECC", offsetof(cacheop_t, cop_ecc))

#ifdef IP28
	PHEX("CACHE_TMP_EFRAME1", CACHE_TMP_EFRAME1)
	PHEX("CACHE_TMP_EFRAME2", CACHE_TMP_EFRAME2)
	PHEX("CACHE_TMP_EMASK", CACHE_TMP_EMASK)
#endif
#if IP30
	PHEX("CACHE_ERR_FRAMEPTR", CACHE_ERR_FRAMEPTR)
#endif
#endif	/* R10000 */

#if defined (BEAST)
	/* General cacheop routine defines */
	PHEX("COP_ADDRESS", offsetof(cacheop_t, cop_address))
	PHEX("COP_OP", offsetof(cacheop_t, cop_operation))
        PHEX("COP_TAG", offsetof(cacheop_t, cop_tag))

#endif /* BEAST */

#if R4000
	/* Some stuff for the Ecc handler */
	PHEX("CACHE_ERR_EFRAME", CACHE_ERR_EFRAME)
	PHEX("CACHE_ERR_ECCFRAME", CACHE_ERR_ECCFRAME)
#if defined (IP19) || (IP21)
	PHEX("CACHE_ERR_SP_PTR", CACHE_ERR_SP_PTR)
#else
	PHEX("CACHE_ERR_SP", CACHE_ERR_SP)
#endif /* (IP19) || (IP21) || (IP25) */
	PHEX("ECCF_CACHE_ERR", ECCF_CACHE_ERR)
	PHEX("ECCF_TAGLO", ECCF_TAGLO)
	PHEX("ECCF_ECC", ECCF_ECC)
	PHEX("ECCF_ERROREPC", ECCF_ERROREPC)
	PHEX("ECCF_PADDR", ECCF_PADDR)
#if IP19 || IP32
	PHEX("ECCF_PADDRHI", ECCF_PADDRHI)
#endif
	PHEX("ECCF_SIZE", ECCF_SIZE)
#ifdef _MEM_PARITY_WAR
	PHEX("ECCF_BUSY", ECCF_BUSY)
	PHEX("ECCF_STACK_BASE", ECCF_STACK_BASE)
	PHEX("ECCF_ECCINFO", ECCF_ECCINFO)
	PHEX("ECCF_SECOND_PHASE", ECCF_SECOND_PHASE)

	PHEX("NF_NENTRIES",NF_NENTRIES)

#endif /* _MEM_PARITY_WAR */
#endif

#if defined (SN)
	/*
	 * new cache error handler for SN
	 */
	PHEX("SN_ECCF_ERROREPC" , offsetof(sn_eccframe_t, sn_eccf_errorepc  ))
	PHEX("SN_ECCF_TAGHI"    , offsetof(sn_eccframe_t, sn_eccf_taghi	    ))
	PHEX("SN_ECCF_TAGLO"    , offsetof(sn_eccframe_t, sn_eccf_taglo	    ))
	PHEX("SN_ECCF_CACHE_ERR", offsetof(sn_eccframe_t, sn_eccf_cache_err ))
	PHEX("SN_ECCF_FAIL_CPU" , offsetof(sn_eccframe_t, sn_eccf_fail_cpu  ))
	PHEX("SN_ECCF_HAND_CPU" , offsetof(sn_eccframe_t, sn_eccf_hand_cpu  ))
#endif /* SN */

	PHEX("K_CURTIMER",
	     offsetof(kthread_t, k_timers)+offsetof(ktimerpkg_t, kp_curtimer))
	PHEX("TIMER_MAX", TIMER_MAX)

	PHEX("ABI_IRIX5_64", ABI_IRIX5_64)
	PHEX("ABI_IRIX5_N32", ABI_IRIX5_N32)

#ifdef ULI
	PHEX("VPDA_CURULI", offsetof(pda_t, p_curuli) + PDAADDR)
	PDEC("EINVAL", EINVAL)
	PDEC("SGI_ULI", SGI_ULI)
	PDEC("ULI_RETURN", ULI_RETURN)
	PDEC("SYS_syssgi", SYS_syssgi)
	PHEX("ULI_SP", offsetof(struct uli, sp))
	PHEX("ULI_PC", offsetof(struct uli, pc))
	PHEX("ULI_GP", offsetof(struct uli, gp))
	PHEX("ULI_SR", offsetof(struct uli, sr))
	PHEX("ULI_CPU", offsetof(struct uli, ulicpu))
	PHEX("ULI_INTSTK", offsetof(struct uli, saved_intstack))
	PHEX("ULI_FUNCARG", offsetof(struct uli, funcarg))
	PHEX("ULI_SAVED_CURUTHREAD", offsetof(struct uli, uli_saved_curuthread))
	PHEX("ULI_UTHREAD", offsetof(struct uli, uli_uthread))
	PHEX("ULI_TSTMP", offsetof(struct uli, tstamp))

	PHEX("ULI_NEW_ASIDS", offsetof(struct uli, new_asids))
	PHEX("ULI_SAVED_ASIDS", offsetof(struct uli, saved_asids))

	PDEC("HZ", HZ)
	PDEC("ULI_EF_SIZE", sizeof(struct ULIeframe))
	PDEC("ULI_EF_RETURN", offsetof(struct ULIeframe, ef_return))
	PDEC("ULI_EF_A1", offsetof(struct ULIeframe, ef_a1))
	PDEC("ULI_EF_A2", offsetof(struct ULIeframe, ef_a2))
	PDEC("ULI_EF_A3", offsetof(struct ULIeframe, ef_a3))
	PDEC("ULI_EF_GP", offsetof(struct ULIeframe, ef_gp))
	PDEC("ULI_EF_SP", offsetof(struct ULIeframe, ef_sp))
	PDEC("ULI_EF_FP", offsetof(struct ULIeframe, ef_fp))
	PDEC("ULI_EF_RA", offsetof(struct ULIeframe, ef_ra))
	PDEC("ULI_EF_SR", offsetof(struct ULIeframe, ef_sr))
	PDEC("ULI_EF_EPC", offsetof(struct ULIeframe, ef_epc))
#ifdef TFP
	PHEX("ULI_CONFIG", offsetof(struct uli, config))
	PHEX("ULI_SAVED_CONFIG", offsetof(struct uli, saved_config))
	PDEC("ULI_EF_CONFIG", offsetof(struct ULIeframe, ef_config))
#endif
#ifdef EPRINTF
	PHEX("ULI_BUNK", offsetof(struct uli, bunk))
#endif
#ifdef ULI_TSTAMP1
	PDEC("ULITS_SIZE", sizeof(int))
	PDEC("TSTAMP_CPU", TSTAMP_CPU)
	PDEC("TS_GENEX", TS_GENEX)
	PDEC("TS_EXCEPTION", TS_EXCEPTION)
	PDEC("TS_INTRNORM", TS_INTRNORM)
	PDEC("TS_ERET", TS_ERET)
#endif
	PDEC("ULI_EFRAME_VALID", offsetof(struct uli, uli_eframe_valid))
	PDEC("ULI_EFRAME", offsetof(struct uli, uli_eframe))

	PDEC("NSEC_PER_SEC", NSEC_PER_SEC)

#endif
#if DEBUG
	PHEX("VPDA_SWITCHUTHREAD", offsetof(pda_t, p_switchuthread) + (size_t)PDAADDR)
#endif
	PHEX("PDA_RUNANYWHERE", PDA_RUNANYWHERE)
	PHEX("RESCHED_KP", RESCHED_KP)
	PDEC("ARGSAVEFRM", ARGSAVEFRM)
#if defined(EVEREST) && defined(MULTIKERNEL)
	/* ARCS debug addresses */

	PHEX("DB_EXCOFF", DB_EXCOFF)

	/* various defined required for MULTIKERNEL */

	PHEX("ECFGINFO_ADDR", EVCFGINFO_ADDR)
	PHEX("ECFG_MEMSIZE", offsetof(evcfginfo_t, ecfg_memsize))
	PHEX("ECFG_NUMCELLS", offsetof(evcfginfo_t, ecfg_numcells))
	PHEX("ECFG_CELLMASK", offsetof(evcfginfo_t, ecfg_cellmask))
	PHEX("ECFG_CELL", offsetof(evcfginfo_t, ecfg_cell))
	PHEX("SIZEOF_CELLINFO", sizeof(evcellinfo_t))
	PHEX("ECFG_CELL_MEMBASE", offsetof(evcellinfo_t, cell_membase))
	PHEX("ECFG_CELL_MEMSIZE", offsetof(evcellinfo_t, cell_memsize))

	PHEX("EVMK_CELL_K0BASE", offsetof(evmk_t, cell_k0base))
	PHEX("EVMK_CELLID", offsetof(evmk_t, cellid))
	PHEX("EVMK_CELLMASK", offsetof(evmk_t, cellmask))
	PHEX("EVMK_NCELLS", offsetof(evmk_t, numcells))
#endif
	/* DO NOT ADD ANYTHING AFTER THIS LINE */
	PEND()
};

main()
{
	exit(0);
}
