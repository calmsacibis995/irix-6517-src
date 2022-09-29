/**************************************************************************
 *									  *
 * 		 Copyright (C) 1994-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 1.20 $"

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/sbd.h>
#include <sys/pda.h>
#include <sys/fpu.h>
#include <sys/reg.h>
#include "fpparams.h"
#include "fpexterns.h"
#include <sys/uthread.h>
#include <ksys/exception.h>
#include <sys/proc.h>		/* proxy definition */
#include <sys/debug.h>
#include <sys/kabi.h>
#include <sys/cmn_err.h>
#ifdef _R5000_CVT_WAR
extern int R5000_cvt_war;
#endif /* _R5000_CVT_WAR */

static int illfpinst(int, eframe_t *);
static int softfp_dispatch(struct eframe_s *, inst_t);

/*
 * Floating-point coprocessor interrupt/exception handler
 */

/* SOME ASSUMPTIONS that this code makes:
 * For R4000 and R10000 (or any processor that delivers precise
 * fp exceptions): Since the exception is precise, curuthread must
 * indicate the faulting process thread - hence we use CURUTHREAD through
 * this code for those processors. (note that R8K in precise mode
 * fits into this catagory.)
 *
 * For R8000 (or processors that deliver imprecise fp interrupts):
 * p_fpowner is assumed to point to the faulting process uthread.
 * curuthread is not usable - the interrupt could occur after a context
 * switch - for example, as a result of calling checkfp() to save
 * the fp context, since on R8K, reading the fp_csr causes all fp
 * instructions to complete, and for any interrupts to occur.
 *
 * However, if the kernel preempts, then p_fpowner gets
 * clobbered. R8K protects against this by entering this code
 * with interrupts disabled - this is necessary also because R8K
 * will immediately get another fp interrupt if interrupts are enabled.
 * Other processors need to find their own solution here.
 */

static void
strayfp_intr(void)
{
	/* got a floating point exception, but no process was using
	 * the unit.
	 */
	setsr(getsr() | SR_CU1);
	set_fpc_csr(0);		/* clear the exception/interrupt */
	setsr(getsr() & ~SR_CU1);

	cmn_err(CE_NOTE, "stray fp exception\n");
}

static void
sendSIGFPE(uthread_t *ut)
{
	if (ut == curuthread) {
		PCB(pcb_resched) = 1;
	}

#ifdef IP26
        /* Read comments for 636955 below. */
        /* sigtouthread may call splhi() */

        checkfp(ut, 0);
#endif

	sigtouthread(ut, SIGFPE, 0);

	/* We must clear the coprocessor interrupt without losing fp
	 * state, we do this by calling checkfp which will unload
	 * the fp to the pcb and clear the fp csr.  A signal is
	 * pending, sendsig will clear the csr in the pcb after
	 * saving the fp state from the pcb into the sigcontext and
	 * before calling the signal handler
	 */
#ifndef IP26
	checkfp(ut, 0);
#endif
}

void
fp_intr(eframe_t *ep)
{
	k_machreg_t epc;
	uthread_t *ut;
	int fp_sig;
	inst_t flt_inst;
	int junk;
#if TFP
	k_machreg_t sr;
#endif

#if TFP
	ut = private.p_fpowner;
#else
	ut = curuthread;
#endif

	if (ut == NULL) {
		strayfp_intr();
		return;
	}

#if TFP
	sr = ep->ef_sr;
	if (!(sr & SR_DM)) {
		int32_t csr;

		/* TFP Performance Mode Floating Point
		 * If we get a denorm interrupt, we need to clear the E bit
		 * and return. The processor has already flushed the result
		 * to zero. Some users may want to be notified when this
		 * interrupt occurs, but for now we just ignore it.
		 */

		/* For TFP in performance mode, get the csr
		 * directly from the fpunit, not potentially from the
		 * pcb. This works because we are running at splhi.
		 */
		csr = get_fpc_csr();

		if ((csr &  FPCSR_FLUSH_ZERO) || ! (csr & FPCSR_UNIMP)) {
			sendSIGFPE(ut);
			return;
		}

		/* Clear the E bit, set that into the fpunit's fpc_csr,
		 * and return
		 */
		set_fpc_csr(csr & ~FPCSR_UNIMP);
		return;
	}

#ifdef IP26
        /*
         * Unload FP state for IP26 precise/debug mode. The rest
         * of softfp is going to call splhi. splhi for IP26 will
         * enable interrupts, and it will immedeately take the FP
         * exception again. Note that checkfp is already fixed
         * by not calling splhi for IP26. Eventhough the comment there
         * does not make it too clear. PV # 636955
         * This bug also has a change in sendSIGFPE
         */
        checkfp(ut, 0) ;
#endif

#endif	/* TFP */

	/* The idiotic sysmips(MIPS_FPSIGINTR) interface:
	 * if ut->ut_pproxy->prxy_fp.pfp_fp is P_FP_SIGINTR1, then
	 * a SIGFPE is generated on every floating-point interrupt
	 * before each instruction and is then set to P_FP_SIGINTR2.
	 * If pfp_fp is P_FP_SIGINTR2, then no SIGFPE is generated
	 * but pfp_fp is set to P_FP_SIGINTR1.
	 */
	if ((fp_sig = ut->ut_pproxy->prxy_fp.pfp_fp) != 0) {
		if (fp_sig == P_FP_SIGINTR1) {
			ut->ut_pproxy->prxy_fp.pfp_fp = P_FP_SIGINTR2;
			sendSIGFPE(ut);
			return;
		} else {
			ut->ut_pproxy->prxy_fp.pfp_fp = P_FP_SIGINTR1;
		}
	}

#ifdef TFP
	/* TFP may TLB fault loading the instruction at EPC since we may
	 * be executing from the icache.  So we load it here where it's
	 * OK to fault (we think).
	 */
	/* Load instruction at epc */
	ut->ut_epcinst = fuword((void *)ep->ef_epc);
	if (ep->ef_cause & CAUSE_BD)
		ut->ut_bdinst = fuword((void *)(ep->ef_epc + 4));
#endif

	if (ep->ef_cause & CAUSE_BD) {
		epc = emulate_branch(ep, ut->ut_epcinst, load_fpcsr(), &junk);
		flt_inst = ut->ut_bdinst;
	} else {
		flt_inst = ut->ut_epcinst;
		epc = ep->ef_epc + 4;
	}

	/*
	 * Check to see if the instruction to be emulated is a floating-point
	 * instruction.  If it is not then this interrupt must have been caused
	 * by writing to the fpc_csr a value which caused the interrupt.
	 * It is possible however that when writing to the fpc_csr the
	 * instruction that is to be "emulated" when the interrupt is handled
	 * looks like a floating-point instruction and will incorrectly be
	 * emulated and a SIGFPE will not be sent.  This is the user's problem
	 * because he shouldn't write a value into the fpc_csr which should
	 * cause an interrupt.
	 */
	if ( (flt_inst >> OPCODE_SHIFT == OPCODE_C1 &&
	      flt_inst >> OPCODE_25_SHIFT == OPCODE_C1_25)
	    ||
	      flt_inst >> OPCODE_SHIFT == OPCODE_C1X) {

		/* Looks like an fp instruction - call softfp
		 * to work on it.
		 * softfp returns non-zero if a signal is posted
		 * to the process.
		 */
		if (softfp_dispatch(ep, flt_inst) != 0) {
			/*
			 * A modified checkfp protocol - an argument of 2
			 * indicates that the fpc_csr has already been
			 * stored into the PCB for this process, so don't
			 * do it again in checkfp. The reason is this: At
			 * the end of softfp, if a SIGFPE is being posted,
			 * softfp wants to put the cause of the SIGFPE
			 * into fpc_csr. Then, checkfp retrieves fpc_csr
			 * from the fpu and saves it in the PCB. This does
			 * not work for the R4000 and subsequent, since the
			 * ctc1 at the end of softfp would cause an immediate
			 * exception. So, if softfp is returning 1, it saves
			 * fpc_csr in the PCB rather than in the fpu.
			 * Similar problem with TFP, except it would cause
			 * an immediate interrupt. The only other way to
			 * avoid the FP interrupt is to either keep ALL
			 * interrupts disabled (IE bit in SR) or to clear
			 * the corresponding Enable bit in the fpc_csr.  So
			 * we use the R4000 approach and don't reload the
			 * fpc_csr.
			 */
			checkfp(ut, 2);
		} else {
			/* The instruction was emulated by softfp
			 * without a signal being posted so now change
			 * the epc to the target pc.
			 */
			ep->ef_epc = epc;
			ep->ef_exact_epc = epc;
		}
	} else {
		sendSIGFPE(ut);
	}
	return;
		
}

#define DEBUG_SOFTFP_PREEMPTION

#ifdef DEBUG_SOFTFP_PREEMPTION
int force_checkfp;
#endif

static int
softfp_dispatch(struct eframe_s *ep, inst_t inst)
{
	int extregmode;
	int fp_csr;
	int fp_enables;
	int flush = 0;
	inst_t opcode;
	inst_t cop1_fmt;
	inst_t cop1_func;
	inst_t rs_num, rt_num, rd_num;	/* register number for s,t and d reg */
	int64_t rs, rt; 		/* s and t registers */
	caddr_t epc;			/* exception pc */
	int iscop1x = 0;
#if MIPS4_ISA
	inst_t rr_num;			/* register number for r reg */
	int64_t rr;			/* r register */
#endif

#ifdef DEBUG_SOFTFP_PREEMPTION
	if (force_checkfp)
		checkfp(curuthread, 0);
#endif

	ASSERT(curuthread != NULL);
	extregmode = ABI_IS(ABI_IRIX5_64 | ABI_IRIX5_N32, get_current_abi());

	fp_csr = load_fpcsr() & ~CSR_EXCEPT;	/* All except the cause bits */

	if (fp_csr & CSR_FSBITSET)
		flush = FLUSH_INPUT_DENORMS|FLUSH_OUTPUT_DENORMS;

	rd_num = (inst >> RD_SHIFT) & RD_FPRMASK;

	/* Test for pc in range of u_nofpefrom/u_nofpeto */
	epc = (caddr_t)(__psint_t)ep->ef_epc;
	if (epc >= curuthread->ut_pproxy->prxy_fp.pfp_nofpefrom &&
	    epc <= curuthread->ut_pproxy->prxy_fp.pfp_nofpeto) {
		/* The pc is in the range of addresses where
		 * exceptions will not deliver a sigfpe to
		 * the user. The algorithm for this is:
		 * 1. clear the enables, so we won't call psignal.
		 * 2. do the normal emulation.
		 * 3. Restore the enables at the end of the operation.
		 *    be sure to mask off any cause bits that are also
		 *    enabled, so we don't trigger an exception when
		 *    storing back to the fp_csr register.
		 */
		fp_enables = fp_csr & CSR_ENABLE;
		fp_csr &= ~CSR_ENABLE;
	} else
		fp_enables = 0;

	fp_init_glob(ep, extregmode, fp_csr, fp_enables, flush, rd_num);

	opcode = inst >> OPCODE_SHIFT;

#if MIPS4_ISA
	if (opcode == COP1X)
		iscop1x = 1;
	else
#endif
	if (opcode != COP1)
		return illfpinst(fp_csr, ep);

	cop1_fmt = (iscop1x == 0) ? ((inst >> C1_FMT_SHIFT) & C1_FMT_MASK)
				  : (inst & C1X_FMT_MASK);	
	rs_num = (inst >> RS_SHIFT) & RS_FPRMASK;
	rt_num = (inst >> RT_SHIFT) & RT_FPRMASK;

	/* check the 1st source register - if not in extended
	 * register mode, don't allow an odd register.
	 */
	if (!extregmode && (rs_num & 1))
		return illfpinst(fp_csr, ep);
#ifdef _R5000_CVT_WAR
	if(	R5000_cvt_war &&
		((cop1_fmt == (C1_FMT_WORD + 2)) ||
		(cop1_fmt == (C1_FMT_LONG + 2))))
		cop1_fmt -= 2;
#endif /* _R5000_CVT_WAR */

	switch (cop1_fmt) {
	default:
		return illfpinst(fp_csr, ep);
	case C1_FMT_SINGLE:
	case C1_FMT_WORD:
		rs = fpload_s(rs_num, extregmode);
		break;
	case C1_FMT_DOUBLE:
	case C1_FMT_LONG:
		rs = fpload_d(rs_num, extregmode);
		break;
	}

	cop1_func = inst & C1_FUNC_MASK;

	if (iscop1x)
		goto cop1x;

	if (cop1_func <= C1_FUNC_DIV ||
	   (cop1_func >= C1_FUNC_1stCMP && cop1_func <= C1_FUNC_lastCMP)) {

		/* A binary op - check 2nd register specification */
		if (!extregmode && (rt_num & 1))
			return illfpinst(fp_csr, ep);

		switch (cop1_fmt) {
		case C1_FMT_SINGLE:
			rt = fpload_s(rt_num, extregmode);
			break;
		case C1_FMT_DOUBLE:
			rt = fpload_d(rt_num, extregmode);
			break;
		default:
			return illfpinst(fp_csr, ep);
		}
	}

	if ( ! (cop1_func >= C1_FUNC_1stCMP && cop1_func <= C1_FUNC_lastCMP)) {
		/* Not a comparison opcode, so this op has a destination
		 * register - check for a legal specification.
		 */
		if (!extregmode && (rd_num & 1))
			return illfpinst(fp_csr, ep);
	}

	switch (cop1_fmt) {
	case C1_FMT_SINGLE:
		if (cop1_func >= C1_FUNC_1stCMP &&
		    cop1_func <= C1_FUNC_lastCMP) {
#if MIPS4_ISA
			_comp_sd(SINGLE, rs, rt, inst & COND_MASK,
				     (inst >>  CC_SHIFT) & CC_MASK);
#else
			_comp_sd(SINGLE, rs, rt, inst & COND_MASK, 0);
#endif
			return ( GET_SOFTFP_FLAGS() & SIG_POSTED );
		}

		switch (cop1_func) {
		case C1_FUNC_ADD:
			_add_sd(SINGLE, rs, rt);
			break;
		case C1_FUNC_SUB:
			_sub_sd(SINGLE, rs, rt);
			break;
		case C1_FUNC_MUL:
			_mul_sd(SINGLE, rs, rt);
			break;
		case C1_FUNC_DIV:
			_div_sd(SINGLE, rs, rt);
			break;
		case C1_FUNC_SQRT:
			_sqrt_sd(SINGLE, rs);
			break;
		case C1_FUNC_ABS:
			_abs_sd(SINGLE, rs);
			break;
		case C1_FUNC_NEG:
			_neg_sd(SINGLE, rs);
			break;
		case C1_FUNC_MOV:
#ifdef _FPTRACE

			fp_trace1_sd( SINGLE, rs, MOV_SD );
#endif
			FPSTORE_S(rs);
			store_fpcsr();
			break;
		case C1_FUNC_ROUNDL:
#ifdef _FPTRACE

			fp_trace1_sd( SINGLE, rs, ROUNDL_SD );
#endif
			/* set rounding mode to round-to-nearest */

			SET_FP_CSR_RM(CSR_RM_RN);

			_cvtl_sd( SINGLE, rs );
			break;
		case C1_FUNC_TRUNCL:
#ifdef _FPTRACE
			fp_trace1_sd( SINGLE, rs, TRUNCL_SD );
#endif

			/* set rounding mode to round-to-zero */

			SET_FP_CSR_RM(CSR_RM_RZ);

			_cvtl_sd( SINGLE, rs );
			break;
		case C1_FUNC_CEILL:
#ifdef _FPTRACE

			fp_trace1_sd( SINGLE, rs, CEILL_SD );
#endif
			/* set rounding mode to round-to-plus-infinity */

			SET_FP_CSR_RM(CSR_RM_RPI);

			_cvtl_sd( SINGLE, rs );
			break;
		case C1_FUNC_FLOORL:
#ifdef _FPTRACE

			fp_trace1_sd( SINGLE, rs, FLOORL_SD );
#endif
			/* set rounding mode to round-to-minus-infinity */

			SET_FP_CSR_RM(CSR_RM_RMI);

			_cvtl_sd( SINGLE, rs );
			break;
		case C1_FUNC_ROUNDW:
#ifdef _FPTRACE

			fp_trace1_sd( SINGLE, rs, ROUNDW_SD );
#endif
			/* set rounding mode to round-to-nearest */

			SET_FP_CSR_RM(CSR_RM_RN);

			_cvtw_sd( SINGLE, rs );
			break;
		case C1_FUNC_TRUNCW:
#ifdef _FPTRACE
			fp_trace1_sd( SINGLE, rs, TRUNCW_SD );
#endif

			/* set rounding mode to round-to-zero */

			SET_FP_CSR_RM(CSR_RM_RZ);

			_cvtw_sd( SINGLE, rs );
			break;
		case C1_FUNC_CEILW:
#ifdef _FPTRACE

			fp_trace1_sd( SINGLE, rs, CEILW_SD );
#endif
			/* set rounding mode to round-to-plus-infinity */

			SET_FP_CSR_RM(CSR_RM_RPI);

			_cvtw_sd( SINGLE, rs );
			break;
		case C1_FUNC_FLOORW:
#ifdef _FPTRACE

			fp_trace1_sd( SINGLE, rs, FLOORW_SD );
#endif
			/* set rounding mode to round-to-minus-infinity */

			SET_FP_CSR_RM(CSR_RM_RMI);

			_cvtw_sd( SINGLE, rs );
			break;
#if MIPS4_ISA
		case C1_FUNC_RECIP:
#ifdef _FPTRACE

			fp_trace1_sd( SINGLE, rs, RECIP_SD );
#endif
			_div_sd(SINGLE, STWOP0, rs);
			break;
		case C1_FUNC_RSQRT:
			_rsqrt_sd(SINGLE, rs);
			break;
#endif
		case C1_FUNC_CVTD:
			_cvtd_s(rs);
			break;
		case C1_FUNC_CVTW:
			_cvtw_sd(SINGLE, rs);
			break;
		case C1_FUNC_CVTL:
			_cvtl_sd(SINGLE, rs);
			break;
		default:
			return illfpinst(fp_csr, ep);
		}
		break;

	case C1_FMT_DOUBLE:
		if (cop1_func >= C1_FUNC_1stCMP &&
		    cop1_func <= C1_FUNC_lastCMP) {
#if MIPS4_ISA
			_comp_sd(DOUBLE, rs, rt, inst & COND_MASK,
				     (inst >>  CC_SHIFT) & CC_MASK);
#else
			_comp_sd(DOUBLE, rs, rt, inst & COND_MASK, 0);
#endif
			return ( GET_SOFTFP_FLAGS() & SIG_POSTED );
		}

		switch (cop1_func) {
		case C1_FUNC_ADD:
			_add_sd(DOUBLE, rs, rt);
			break;
		case C1_FUNC_SUB:
			_sub_sd(DOUBLE, rs, rt);
			break;
		case C1_FUNC_MUL:
			_mul_sd(DOUBLE, rs, rt);
			break;
		case C1_FUNC_DIV:
			_div_sd(DOUBLE, rs, rt);
			break;
		case C1_FUNC_SQRT:
			_sqrt_sd(DOUBLE, rs);
			break;
		case C1_FUNC_ABS:
			_abs_sd(DOUBLE, rs);
			break;
		case C1_FUNC_NEG:
			_neg_sd(DOUBLE, rs);
			break;
		case C1_FUNC_MOV:
#ifdef _FPTRACE

			fp_trace1_sd( DOUBLE, rs, MOV_SD );
#endif
			FPSTORE_D(rs);
			store_fpcsr();
			break;
		case C1_FUNC_ROUNDL:
#ifdef _FPTRACE

			fp_trace1_sd( DOUBLE, rs, ROUNDL_SD );
#endif
			/* set rounding mode to round-to-nearest */

			SET_FP_CSR_RM(CSR_RM_RN);

			_cvtl_sd( DOUBLE, rs );
			break;
		case C1_FUNC_TRUNCL:
#ifdef _FPTRACE
			fp_trace1_sd( DOUBLE, rs, TRUNCL_SD );
#endif

			/* set rounding mode to round-to-zero */

			SET_FP_CSR_RM(CSR_RM_RZ);

			_cvtl_sd( DOUBLE, rs );
			break;
		case C1_FUNC_CEILL:
#ifdef _FPTRACE

			fp_trace1_sd( DOUBLE, rs, CEILL_SD );
#endif
			/* set rounding mode to round-to-plus-infinity */

			SET_FP_CSR_RM(CSR_RM_RPI);

			_cvtl_sd( DOUBLE, rs );
			break;
		case C1_FUNC_FLOORL:
#ifdef _FPTRACE

			fp_trace1_sd( DOUBLE, rs, FLOORL_SD );
#endif
			/* set rounding mode to round-to-minus-infinity */

			SET_FP_CSR_RM(CSR_RM_RMI);

			_cvtl_sd( DOUBLE, rs );
			break;
		case C1_FUNC_ROUNDW:
#ifdef _FPTRACE

			fp_trace1_sd( DOUBLE, rs, ROUNDW_SD );
#endif
			/* set rounding mode to round-to-nearest */

			SET_FP_CSR_RM(CSR_RM_RN);

			_cvtw_sd( DOUBLE, rs );
			break;
		case C1_FUNC_TRUNCW:
#ifdef _FPTRACE
			fp_trace1_sd( DOUBLE, rs, TRUNCW_SD );
#endif

			/* set rounding mode to round-to-zero */

			SET_FP_CSR_RM(CSR_RM_RZ);

			_cvtw_sd( DOUBLE, rs );
			break;
		case C1_FUNC_CEILW:
#ifdef _FPTRACE

			fp_trace1_sd( DOUBLE, rs, CEILW_SD );
#endif
			/* set rounding mode to round-to-plus-infinity */

			SET_FP_CSR_RM(CSR_RM_RPI);

			_cvtw_sd( DOUBLE, rs );
			break;
		case C1_FUNC_FLOORW:
#ifdef _FPTRACE

			fp_trace1_sd( DOUBLE, rs, FLOORW_SD );
#endif
			/* set rounding mode to round-to-minus-infinity */

			SET_FP_CSR_RM(CSR_RM_RMI);

			_cvtw_sd( DOUBLE, rs );
			break;
#if MIPS4_ISA
		case C1_FUNC_RECIP:
#ifdef _FPTRACE

			fp_trace1_sd( DOUBLE, rs, RECIP_SD );
#endif
			_div_sd(DOUBLE, DTWOP0, rs);
			break;
		case C1_FUNC_RSQRT:
			_rsqrt_sd(DOUBLE, rs);
			break;
#endif
		case C1_FUNC_CVTS:
			_cvts_d(rs);
			break;
		case C1_FUNC_CVTW:
			_cvtw_sd(DOUBLE, rs);
			break;
		case C1_FUNC_CVTL:
			_cvtl_sd(DOUBLE, rs);
			break;
		default:
			return illfpinst(fp_csr, ep);
		}
		break;

	case C1_FMT_WORD:
		switch (cop1_func) {
		case C1_FUNC_CVTS:
			_cvtsd_w(SINGLE, rs);
			break;
		case C1_FUNC_CVTD:
			_cvtsd_w(DOUBLE, rs);
			break;
		default:
			return illfpinst(fp_csr, ep);
		}
		break;

	case C1_FMT_LONG:
		switch (cop1_func) {
		case C1_FUNC_CVTS:
			_cvtsd_l(SINGLE, rs);
			break;
		case C1_FUNC_CVTD:
			_cvtsd_l(DOUBLE, rs);
			break;
		default:
			return illfpinst(fp_csr, ep);
		}
		break;

	default:
		/* it is not a cop1x, so must be an illfpinst */

		return illfpinst(fp_csr, ep);
	}

	return ( GET_SOFTFP_FLAGS() & SIG_POSTED );

cop1x:

#if MIPS4_ISA

	rr_num = (inst >> RR_SHIFT) & RR_FPRMASK;
	if (!extregmode && ((rt_num & 1) || (rr_num & 1) || (rd_num & 1)))
		return illfpinst(fp_csr, ep);

	switch (cop1_fmt) {
	case C1_FMT_SINGLE:
		rt = fpload_s(rt_num, extregmode);
		rr = fpload_s(rr_num, extregmode);
		break;
	case C1_FMT_DOUBLE:
		rt = fpload_d(rt_num, extregmode);
		rr = fpload_d(rr_num, extregmode);
		break;
	default:
		return illfpinst(fp_csr, ep);
	}

	switch (cop1_func) {
	default:
		return illfpinst(fp_csr, ep);
	case C1X_FUNC_MADD_S:
		_madd_sd(SINGLE, rs, rt, rr);
		break;

	case C1X_FUNC_MSUB_S:
		_msub_sd(SINGLE, rs, rt, rr);
		break;

	case C1X_FUNC_NMADD_S:
		_nmadd_sd(SINGLE, rs, rt, rr);
		break;

	case C1X_FUNC_NMSUB_S:
		_nmsub_sd(SINGLE, rs, rt, rr);
		break;

	case C1X_FUNC_MADD_D:
		_madd_sd(DOUBLE, rs, rt, rr);
		break;

	case C1X_FUNC_MSUB_D:
		_msub_sd(DOUBLE, rs, rt, rr);
		break;

	case C1X_FUNC_NMADD_D:
		_nmadd_sd(DOUBLE, rs, rt, rr);
		break;

	case C1X_FUNC_NMSUB_D:
		_nmsub_sd(DOUBLE, rs, rt, rr);
		break;

	}
#endif	/* MIPS4_ISA */

	return ( GET_SOFTFP_FLAGS() & SIG_POSTED );
}

static int
illfpinst(int fp_csr, eframe_t *ep)
{
	softfp_psignal(SIGILL, fp_csr, ep);
        checkfp(curuthread, 0);
	return 1;
}
