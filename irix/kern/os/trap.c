   
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1994 Silicon Graphics, Inc.		  *
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

#include <sys/types.h>
#include <sys/atomic_ops.h>
#include <ksys/as.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/dump.h>
#include <sys/errno.h>
#include <sys/fpu.h>
#include <sys/immu.h>
#include <sys/inst.h>
#include <sys/par.h>
#include <sys/param.h>
#include <sys/pcb.h>
#include <sys/pda.h>
#include <sys/rtmon.h>
#include <sys/proc.h>
#include <sys/profil.h>
#include <sys/reg.h>
#include <sys/resource.h>
#include <sys/runq.h>
#include <sys/sat.h>
#include <sys/ksignal.h>
#include <sys/signal.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <ksys/fdt.h>
#include <sys/var.h>
#include <sys/watch.h>
#include <procfs/prsystm.h>
#include <sys/syssgi.h>
#include <sys/softfp.h>
#include <sys/vmereg.h>
#include <sys/hwperfmacros.h>
#include <sys/lpage.h>
#ifdef _NEW_UME_SIG
#include <sys/lpage.h>
#endif /* _NEW_UME_SIG */
#ifdef _SHAREII
#include	<sys/shareIIstubs.h>
#endif /* _SHAREII */
#include <ksys/exception.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */
#include <sys/prctl.h>
#include <sys/kusharena.h>
#include <ksys/vm_pool.h>

#ifdef IP22
/* 
 * An important medical-equipment manufacturing customer uses IP22s.  
 * This define enables support for a feature they require:
 * On IP22s, if the "special_bus_error_handler" variable is set (by the 
 * customer's driver), a customer-provided bus error callback function 
 * is invoked, and can prevent the system from panicing.  On regular 
 * systems, the code has no affect because "special_bus_error_handler" is null.
 *
 * See also kern/ml/IP22.c
 *
 */
#define SPECIAL_BUS_ERROR_HANDLER
#endif

#ifdef _MEM_PARITY_WAR
extern int ecc_exception_recovery(eframe_t * /* ep */);
#endif

#ifdef DEBUG
#define WATCHDEBUG 0
int zerodebug = 0;		/* go into debugger on zero access */
#endif

/*
 * Used for decoding break instructions.  There is an old standing bug in the
 * assembler which encoded the break code right justified to bit 16 not to
 * bit 6 (that's why the BRK_SHIFT is 16 not 6 as would be obvious).
 */
#define BRK_MASK	0xfc00003f
#define BRK_SHIFT	16
#define BRK_SUBCODE(x)	(((x) & ~BRK_MASK) >> BRK_SHIFT)

static inst_t *branch_target(inst_t, inst_t *);
static void profileregion(inst_t *, int);
static void trapstart(uthread_t *, eframe_t *, uint);
static void trapend(uthread_t *, machreg_t, eframe_t *, time_t);
static void trapend_profile(uthread_t *, time_t);
static void install_bp(k_machreg_t);
static void set_bp(inst_t *);
static void remove_bp(void);
static int  epc_in_pcb(k_machreg_t);
static int emulate_ovflw(eframe_t *, inst_t);
static int fixade(eframe_t *, machreg_t);
static void k_trap(eframe_t *, uint, machreg_t, machreg_t);
static void panicregs(eframe_t *, uint, machreg_t, machreg_t, char *);
int bad_badva(eframe_t *);
int badva_isbogus(eframe_t *);

extern int	addupc(inst_t *, struct prof *, int, int);

#ifdef R4000
extern int	get_cpu_irr(void);
#endif

#if (defined R10000) && (defined T5TREX_IBE_WAR)
int ibe_isbogus(eframe_t *);
int bogus_ibe;
#endif

/*
 * before u areas are set up,
 * use global nofault index integer
 */
int nofault;
/*
 * This must be declared as an int array to keep the assembler from
 * making it gp relative.
 */
extern int sstepbp[];
extern int (*nofault_pc[])();

/*
 * Code names for exceptions.
 */
char *code_names[] = {
	"interrupt ",				/* 0 */
	"TLB mod ",
	"Read TLB Miss ",
	"Write TLB Miss ",
	"Read Address Error ",
	"Write Address Error ",
#if R4000 || R10000
	"Instruction Bus Error ",
	"Data Bus Error ",
#else
	"unknown",
	"unknown",
#endif
	"SYSCALL ",				/* 8 */
	"BREAKpoint ",
	"Illegal Instruction ",
	"CoProcessor Unusable ",
	"Overflow ",
	"Trap Instruction",
#ifdef R4000
	"Instruction Virtual Coherency Exception",
#else
	"unknown",
#endif
#if R4000 || R10000
	"Floating Point Exception",
#else
	"unknown",
#endif
	"unknown",				/* 16 */
	"unknown",
	"unknown",
	"unknown",
	"unknown",
	"unknown",
#ifdef R10000
	"Event Trap",
#else
	"unknown",
#endif
#if R4000 || R10000
	"Watchpoint Reference",
#else
	"unknown",
#endif
	"unknown",				/* 24 */
	"unknown",
	"unknown",
	"unknown",
	"unknown",
	"unknown",
#ifdef TFP
	"TLB Duplicate Entry Exception",
#else
	"unknown",
#endif
#ifdef R4000
	"Data Virtual Coherency Exception",
#else
	"unknown",
#endif
	"Software detected SEGV ",		/* 32 */
	"resched request ",
	"page-in request ",
	"coprocessor unusable ",
	"software detected bus error ",
	"bad page in process (vfault) ",
	"watchpoint ",
#ifdef R4000
	"end-of-page problem",			/* 39 */
#else
	"unknown",
#endif
#ifdef _MEM_PARITY_WAR
	"unrecoverable memory error"		/* 40 */
#else /* _MEM_PARITY_WAR */
	"unknown"				/* 40 */  
#endif /* _MEM_PARITY_WAR */
};

extern struct reg_desc sr_desc[], cause_desc[];

eframe_t *panicefr = NULL;

static void
panicregs(eframe_t *ep,
	  uint code,
	  machreg_t sr,
	  machreg_t cause,
	  char *panic_msg)
{
	/* REFERENCED */
	int s = splhi();	/* prevent preemption from now on */

	set_leds(LED_PATTERN_FAULT);
	if( panicefr == 0 )
		panicefr = ep;

	/* This will save the hardware error state and helps diagnose 
	 * if there were any hardware errors signalled. */
	machine_error_dump("kernel fault");	

#if TFP
	cmn_err(CE_PANIC,
"%s\nPC: 0x%x ep: 0x%x\nEXC code:%d, `%s'\nBad addr: 0x%x, cause: %R\nsr: %R\n",
	    panic_msg, ep->ef_epc, ep, code,
	    code_names[code>>CAUSE_EXCSHIFT], 
	    ep->ef_badvaddr, cause, cause_desc, sr, sr_desc);
#endif	/* TFP */
#if R4000 || R10000
	cmn_err(CE_PANIC,
"%s\nPC: 0x%x ep: 0x%x\nEXC code:%d, `%s'\nBad addr: 0x%x, cause: %R\nsr: %R\n",
	    panic_msg, (__psunsigned_t)ep->ef_epc, ep, code,
	    code_names[code>>CAUSE_EXCSHIFT], 
	    (__psunsigned_t)ep->ef_badvaddr, cause, cause_desc, sr, sr_desc);
#endif	/* R4000 */
	/* NOTREACHED */
}

#ifdef SPECIAL_BUS_ERROR_HANDLER
extern int (*special_bus_error_handler)(void*,void**);
extern caddr_t special_bus_error_ef_epc;
#endif /* SPECIAL_BUS_ERROR_HANDLER */

/*
 * Handle kernel mode traps.
 */
static void
k_trap(register eframe_t *ep, uint code, machreg_t sr, machreg_t cause)
{
	unsigned inst;
	register int i;

	ASSERT(((private.p_flags & PDAF_ISOLATED) == 0) || \
				!(private.p_flags & PDAF_BROADCAST_OFF));
	switch (code) {
	case EXC_BREAK:
		if (kdebug) {
			cmn_err(CE_CONT|CE_CPUID,"k_trap: BRKPT (window?)\n");
			debug("ring");
			return;
		}
		break;
	case SEXC_PAGEIN:
		/*
		 * pagein during copyin is currently ignored
		 */
		return;
	case EXC_OV:	/* overflow in kernel - try to wrap and continue */
		cmn_err(CE_CONT, "Overflow in kernel @ pc:%x\n", (inst_t *)ep->ef_epc);
		if (cause & CAUSE_BD) {
			inst = *(inst_t *)(ep->ef_epc + 4);
			if (emulate_ovflw(ep, inst)) {
				inst = *(inst_t *)(ep->ef_epc);
				/*
				 * Need to cast 32-bit pointers to a 32-bit
				 * signed type and _then_ cast them to a 64-bit
				 * signed type.  Zero-extended 32-bit kernel
				 * addresses don't work.  (Cause address errors)
				 */
				ep->ef_epc = (k_smachreg_t)
						((__psint_t)branch_target(inst,
						 (inst_t *)(ep->ef_epc)));
				return;
			}
		} else {
			inst = *(inst_t *)(ep->ef_epc);
			if (emulate_ovflw(ep, inst)) {
				ep->ef_epc += 4;
				return;
			}
		}
		goto trynofault;

#if R4000 && JUMP_WAR
	case SEXC_EOP:
		{
		int error;
		if (sexc_eop(ep, code, &error)) {
			KT_TO_UT(curthreadp)->ut_code = error;
			goto trynofault;
		}
		}
		return;
#endif
        case EXC_DBE:
        case EXC_IBE:
	{
		/* Won't return if kernel memory error */
		register int nofaultset = 0;
		register int nofaultflag = 0;
		register int nofaultset_k_nofault = 0;
#if IP21
		int ghostdbe = 0;
		extern int probe_cpu, probe_in_progress;
#endif
#ifdef SPECIAL_BUS_ERROR_HANDLER
		int rv;
#endif
#if (defined R10000) && (defined T5TREX_IBE_WAR)
		if ((code == EXC_IBE) && ibe_isbogus(ep)) {
#ifdef DEBUG
			cmn_err(CE_NOTE, "Hit bogus IBE vaddr 0x%lx\n",
				ep->ef_epc);
#endif
			bogus_ibe++;
			return;
		}
#endif

		if (private.p_nofault) {
			nofaultset = 1;
		} else if (curthreadp && curthreadp->k_nofault) {
			nofaultset = 1;
			nofaultflag = curthreadp->k_nofault;
			nofaultset_k_nofault = 1;
		}
#if IP21
		/*
		 * Workaround for IP21 bug: if another cpu is
		 * doing a probe, we may get a spurious bus
		 * error. Ignore it.
		 */
		if (probe_in_progress)
			nofaultset |= (probe_cpu != cpuid());
		else if (uvme_async_read_error(0) == VME_DBE_IGNORE) {
			nofaultset |= 1;
			ghostdbe = 1;
		}
#endif
#ifdef SPECIAL_BUS_ERROR_HANDLER
		/* if we're !nofault, and this gets handled inside dobuserre */
		/* then we set the ef_epc to what they want and return from  */
		/* this exception */
		special_bus_error_ef_epc = (caddr_t)ep->ef_epc;
		rv = dobuserre(ep, (inst_t *)ep->ef_epc, nofaultset);
		/* 
		 * dobuserre returns 2 if we weren't in USER mode and
		 * the special handler decided to handle it.
		 *
		 * Otherwise, a non-zero return value indicates that the error
		 * has been handled.  We should just return and retry.
		 * This will always happen on systems that don't use 
		 * the special_bus_error feature.
		 */
		if (rv == 2) {
			if ((special_bus_error_ef_epc != NULL) &&
			    (special_bus_error_ef_epc != (caddr_t)ep->ef_epc))
			    ep->ef_epc = (k_machreg_t)special_bus_error_ef_epc;
		        cmn_err(CE_CONT, "k_trap(%s): handled!  epc:0x%X\n",
				(code==EXC_DBE)?"EXC_DBE":"EXC_IBE",ep->ef_epc);
			return;
		} else if (rv) {
		        return;
		}
#else /* not SPECIAL_BUS_ERROR_HANDLER */
		/* 
		 * A non-zero return value indicates that the error has 
		 * been handled.  We should just return and retry.
		 */
		if (dobuserre(ep, (inst_t *)ep->ef_epc, nofaultset))
			return;
#endif /* not SPECIAL_BUS_ERROR_HANDLER */
#if IP21
		if ((probe_in_progress && probe_cpu != cpuid()) || ghostdbe)
			return;
#endif
		if (nofaultset_k_nofault) {
			curthreadp->k_nofault = nofaultflag;
		}

		goto trynofault;
	}

#ifdef _MEM_PARITY_WAR
	case SEXC_ECC_EXCEPTION:
		if (! ecc_exception_recovery(ep))
			goto trynofault;
		 return;
#endif
	case SEXC_SEGV:
#ifdef MP_R4000_BADVA_WAR
		/* We've seen cases where we get here with code==SEXC_SEGV
		 * but cause == EXC_RMISS and a bad badvaddr, probably due
		 * to an exception processing the original RMISS in locore
		 * with a bad BADVADDR.  Let's recover if we determine that
		 * we've hit the known chip problem.
		 */
		if (((cause & CAUSE_EXCMASK) == EXC_RMISS) &&
		    (IS_KSEG2(ep->ef_epc)) &&
		    bad_badva(ep)) {
			
			static int k2badvacount = 0;

			/*
			 * This threshold should probably
			 * be a tuneable
			 */
			if (++k2badvacount == 100) {
				cmn_err_tag(1752,CE_WARN|CE_CPUID,
	     			"excessive R4000 badvaddr(2) occurrances for K2 addresses - may be impacting performance!\n");
				k2badvacount = 0;
			}
			return;
		}
#endif /* MP_R4000_BADVA_WAR */
		/* Fall through */
	case EXC_RADE:
	case EXC_WADE:
	case SEXC_KILL:
	case SEXC_BUS:
trynofault:
		/*
		 * if in a 'global' nofault section handle that now
		 */
		if (private.p_nofault) {
			i = private.p_nofault;
			private.p_nofault = 0;
			if (i < 1 || i >= NF_NENTRIES)
				cmn_err(CE_PANIC, "bad nofault");
			/*
			 * Need to cast 32-bit pointers to a 32-bit
			 * signed type and _then_ cast them to a 64-bit
			 * signed type.  Zero-extended 32-bit kernel
			 * addresses don't work.  (Cause address errors)
			 */
#ifdef SN0
			/*
			 * Until the compiler can put this array in read-only
			 * data so it can be replicated, we have it in the PDA.
			 */
			ep->ef_epc = (k_smachreg_t)((__psint_t)private.p_nofault_pc[i]);
#else
			ep->ef_epc = (k_smachreg_t)((__psint_t)nofault_pc[i]);
#endif
			return;
		}

		/* better not be on IDLE stack! */
		if (private.p_kstackflag == PDA_CURIDLSTK)
			break;

		if (curthreadp && curthreadp->k_nofault) {
			/*
			 * It's possible a user passed us an address that
			 * requires us to grow the stack - this is only true
			 * if in a nofault handler.
			 *
			 * If so, then try to grow the stack as a last ditch
			 * effort. Note that we're NEVER on the user stack!
			 *
			 * Also note that *fault are the only routines
			 * that return SEXC_SEGV and they always set the
			 * ut_code to EFAULT if it's out of the process's
			 * space.  This is the only case where we grow.
			 *
			 * This avoids deadlocks in core()
			 */
			if (code == SEXC_SEGV &&
			    KT_TO_UT(curthreadp)->ut_code == EFAULT) {
				register caddr_t bvaddr = (caddr_t)ep->ef_badvaddr;
				if (IS_KUSEG(bvaddr) && bvaddr != 0 &&
				    (grow(bvaddr) == 0)) {
					/* it worked */
					return;
				}
			}

			if (curprocp) {
				curuthread->ut_flt_cause = cause;
				curuthread->ut_flt_badvaddr = ep->ef_badvaddr;
			}
			i = curthreadp->k_nofault;
			if (i < 1 || i >= NF_NENTRIES) {
				set_leds(LED_PATTERN_BOTCH);
				cmn_err(CE_PANIC,"bad nofault: %d",i);
			}
			curthreadp->k_nofault = 0;
			/*
			 * Need to cast 32-bit pointers to a 32-bit
			 * signed type and _then_ cast them to a 64-bit
			 * signed type.  Zero-extended 32-bit kernel
			 * addresses don't work.  (Cause address errors)
			 */
			ep->ef_epc = (k_smachreg_t)((__psint_t)nofault_pc[i]);
			return;
		}

#ifdef SPECIAL_BUS_ERROR_HANDLER
		/* now check if the customer's driver might want to handle this.
		 * Two cases we want to be able to handle here:
		 * 1) DBE - called dobuserre and handled above
		 * 2) BUS - call the special callback - giving it the
		 *          address that caused the buserr.
		 * In either case we set the ef_epc to what they give us.
		 *
		 * If special_bus_error feature isn't used, it just falls 
		 * through to panicregs() below as before.
		 */
		if (code == SEXC_BUS) {
		    if (special_bus_error_handler != NULL) {
		        special_bus_error_ef_epc = (caddr_t)ep->ef_epc;
		        if (special_bus_error_handler((void*)ldst_addr(ep),
		            (void**)&special_bus_error_ef_epc) != 0) {
		            if ((special_bus_error_ef_epc != NULL) &&
		                (special_bus_error_ef_epc != (caddr_t)ep->ef_epc))
		                ep->ef_epc = (k_machreg_t)special_bus_error_ef_epc;
			    cmn_err(CE_CONT, "k_trap(SEXC_BUS): handled!  epc:0x%X\n",ep->ef_epc);
		            return;
		        }
		    }
		}
#endif /* SPECIAL_BUS_ERROR_HANDLER */

		/* fall through to panic */
	}
	panicregs(ep, code, sr, cause, "KERNEL FAULT");
	/* NOTREACHED */
}

/*
 * User is executing in speculative mode.
 * See if he can continue.
 */
static int
check_speculative(register eframe_t *ep)
{
	union mips_instruction	inst;
	int	br, br_taken;

	/*
	 * If user can't access where his pc points, we give up.
	 * They'll be getting a SIGSEGV shortly anyway!
	 */

	/*
	 * We use the ef_exact_epc to point to the epc at exception time. 
	 * this could sometimes be different from ef_epc since ef_epc might
	 * have been changed by the signal delivery code.
	 */
	/** The 7.2 dbx has a problem with n32 program and sets bit 63 in
  	** the ra which will cause RADE.
	** When 64-bit kernel does read (fuiword) on this address, 
	** ep->ef_exact_epc, fuiword returns -1 indicating a user error;
	** things are okay.
	** However, when 32-bit kernel(O2 and Indigo machines) reads it,
	** fuiword thinks it is valid since bit 63 of ep->ef_exact_epc is
	** missing; fuiword is looking at lower 32 bits of this user addr.
	** This is no good. Hence, we check for 64-bit user and 32-bit
	** kernel, and then look at 64-bit EPC to check whether EPC is
	** within the user address range.
	*/
	if ( ((ep->ef_sr & SR_KX) == 0) && (ep->ef_sr & SR_UX) ) {
		if ( ep->ef_exact_epc >= (KUBASE + KUSIZE) )
			return 1;
	}

	if ((inst.word = fuiword((caddr_t)ep->ef_exact_epc)) == (inst_t)-1)
		return 1;

	/*
	 * It better be a load instruction...
	 */
	br = is_branch(inst.word);
	if (br) {
		inst_t	i2;

		if ((i2 = fuiword((caddr_t)ep->ef_exact_epc + 4)) == (inst_t)-1)
			return 1;
		if (!is_load(i2))
			return 1;
	}
	else if (!is_load(inst.word))
		return 1;

	/*
	 * At this point, if ef_epc is not the same as ef_exact_epc, do 
	 * nothing. This should happen only when ef_epc has been changed 
	 * by the signal handling code to point to the signal handler. Once 
	 * the signal handling has been taken care of, we will get this
	 * exception again at ef_exact_epc: for now just ignore this 	
	 * exception and dont mess with the epc.
	 */
	if (ep->ef_epc == ep->ef_exact_epc) {
		/*
		 * Compute the new PC...
		 */
		if (br) {
			if (inst.i_format.opcode == cop1_op)
			    /* unload fpc_csr into pcb */
			    checkfp(curuthread, 0);
			ep->ef_epc = emulate_branch( ep, inst.word,
						    curexceptionp->u_pcb.pcb_fpc_csr,
						    &br_taken);
		} else
		    ep->ef_epc += 4;
	}
	atomicAddInt(&curuthread->ut_pproxy->prxy_fp.pfp_dismissed_exc_cnt, 1);
	return 0;
}

/*
 * Generate siginfo struct for trap.
 */
static k_siginfo_t *
setsiginfo(
	k_siginfo_t	*sip,
	__int32_t	sig,
	__int32_t	code,
	__int32_t	err,
	caddr_t		addr)
{
	sigvec_t *sigvp;

	/*
	 * No need to get the fully tokenized sigvec struct -- if we're
	 * wrong here about sainfo w.r.t. this sig, it'll just get
	 * dropped when the signal gets delivered.
	 */
	VPROC_GET_SIGVEC(curvprocp, VSIGVEC_PEEK, sigvp);
	if (sigismember(&sigvp->sv_sainfo, sig)) {
		bzero((caddr_t)sip, sizeof(*sip));
		sip->si_signo = sig;
		sip->si_code = code;
		sip->si_errno = err;
		sip->si_addr = addr;
	} else
		sip = NULL;
	return sip;
}

/*
 * tracing
 * all refer to a particular pid: -1 implies all
 */
#if defined(DEBUG) || defined(SABLE)
int syscalltrace = 0;		/* trace syscalls */
static int pagefaulttrace = 0;	/* trace page faults */
int	signaltrace = 0;	/* trace all signals sent to pid */
static int traptrace = 0;	/* trace signals sent by trap */
static int syserrtrace = 0;	/* trace syscall errors */
static int syserrnotrace = 0;	/* trace a particular sys error */
int tracedrop = 0;		/* drop into debugger when tracing pid */
static int segvtrace = 0;	/* trace segvs/gus/addr errors */
static int segvdrop = 0;
#endif /* DEBUG */

#if SW_FAST_CACHE_SYNCH
extern int cachesynch(caddr_t, uint);
#endif

/*
 * trap - general trap interface
 */
void
trap(register eframe_t *ep, uint code, machreg_t sr, machreg_t cause)
{
	int	sig;
	time_t	syst;
	proc_t	*pp;
	uthread_t *ut;
	__psint_t vaddr;
	int	i;
	/* REFERENCED */
	int	error;
	int	forcedb = 0;
	inst_t brki;
	k_siginfo_t	info, *sip;
	sigvec_t *sigvp;
#if IP21
	int	vmedbe;
#endif

	extern int R4000_badva_war;

#if TFP
	/* TFP returns an imprecise "interrupt" rather than a precise 
	 * exception.  So let's just modify the code to EXC_DBE and
	 * let the normal DBE path report the error.
	 */

	if ((code == EXC_INT) && (cause & CAUSE_BE))
		code = EXC_DBE;
#endif /* TFP */

	if (!USERMODE(sr)) {
		k_trap(ep, code, sr, cause);
		return;
	}
	CHECK_DELAY_TLBFLUSH(ENTRANCE);

	ut = curuthread;
	pp = UT_TO_PROC(ut);

	/*
	 * Get address of local sigvec structure.  Will only want to
	 * take a peek at it... since the lock would be dropped before
	 * any signal was actually sent, there's nothing to prevent
	 * the sigvec structure from changing.  (Only a multi-threaded app
	 * could change the sigvec actions on us, but that's another story.)
	 */
	VPROC_GET_SIGVEC(curvprocp, VSIGVEC_PEEK, sigvp);

	syst = ut->ut_prftime;

	trapstart(ut, ep, code);
	if (ut->ut_flags & UT_BLOCK) {
		ut_flagclr(ut, UT_BLOCK);
		blockme();
	}

	sig = 0;
	sip = (k_siginfo_t *)0;

	switch (code) {

	case SEXC_RESCHED:
		/*
		 * Reschedule the processor
		 * softfp has called sigtouthread and needs to have
		 * curuthread process the signal
		 */
		if (PCB(pcb_resched)) {
			PCB(pcb_resched) = 0;
			if (ut->ut_flags & UT_OWEUPC) {
				pwatch_t *pw;

				ut_flagclr(ut, UT_OWEUPC);
				if ((pw = ut->ut_watch) != NULL)
					pw->pw_flag |= WIGN;
				if (pp->p_flag & SPROFFAST) {
#ifdef DEBUG
					if (pp->p_fastprofcnt > PROF_FAST_CNT+1)
					{
						cmn_err(CE_WARN, "fast prof overflow for [%s] pid %d cnt %d\n",

							get_current_name(),
							pp->p_pid,
							pp->p_fastprofcnt);
						pp->p_fastprofcnt = PROF_FAST_CNT+1;
					}
#endif
					for (i = 0; i < pp->p_fastprofcnt; i++)
						profileregion(pp->p_fastprof[i], 1);
					pp->p_fastprofcnt = 0;
				} else {
					profileregion((inst_t *)ep->ef_epc, 1);
				}
				if (pw)
					pw->pw_flag &= ~WIGN;
			}
		}
		user_resched(RESCHED_P);
		break;
	case SEXC_UTINTR : /* uthread needs flags processed before return to umode */
		break;


#if R4000 && JUMP_WAR
	case SEXC_EOP:
		if (sexc_eop(ep, code, &error)) {
			sig = SIGILL;
			sip = setsiginfo(&info, sig, ILL_ILLADR, 0,
					(caddr_t) ((ep->ef_cause & CAUSE_BD) ?
					((uint)ep->ef_epc)+4 : ep->ef_epc));
		}
				
		break;
#endif

	case SEXC_KILL:		/* due to bad page read in vfault */
		cmn_err_tag(1753, CE_ALERT,
			"Process [%s] pid %d killed due to %s (errno %d)",
			pp->p_comm, pp->p_pid,
			(ut->ut_code == ENOSPC) ? "no more swap space"
							: "bad page read",
			ut->ut_code);

		sig = SIGKILL;
		sip = setsiginfo(&info, sig, 0, 0, 0);
		break;

#ifdef _MEM_PARITY_WAR
	case SEXC_ECC_EXCEPTION:
		if (ecc_exception_recovery(ep))
			goto done;

		/* failed; ep->ef_badvaddr has been fixed up */
		ut->ut_code = code;
		/* Fall through to SEXC_BUS */
#endif

	case SEXC_BUS:
		sig = SIGBUS;
		sip = setsiginfo(&info, sig, BUS_OBJERR, ut->ut_code,
				 (caddr_t) ep->ef_badvaddr);
		break;

	case EXC_RMISS:		/* from softfp nofault code */
	case EXC_WMISS:
		ut->ut_code = EFAULT;
		/* Fall Through */
	case SEXC_SEGV:
#if MP_R4000_BADVA_WAR
		/* We've seen cases where we get here with code==SEXC_SEG
		 * but cause == EXC_RMISS and a bad badvaddr, probably due
		 * to an exception processing the original RMISS in locore
		 * with a bad BADVADDR.  Let's recover if we determine tha
		 * we've hit the know chip problem.
		 */
		if (((cause & CAUSE_EXCMASK) == EXC_RMISS) &&
		    (!IS_KUSEG((caddr_t)ep->ef_badvaddr)) &&
		    bad_badva(ep)) {

			/* Need additional check when program is N32.
			 * An N32 program can generate an access to a non-32 bit
			 * address with ADE, and we can end up here with the
			 * second level tlbmiss address.  Check for this case
			 * before calling it the MP_R4000_BAD_BADVADDR_WAR.
			 *
			 * The same holds true for O32 apps, since the
			 * SR_UX bit is now on for those as well.
			 */
			if ( !ABI_IS_IRIX5_64(ut->ut_pproxy->prxy_abi)) {
				vaddr = ep->ef_badvaddr;

				ep->ef_badvaddr = (((caddr_t)vaddr - (caddr_t)KPTEBASE)
					/ sizeof(pde_t)) << PNUMSHFT;

				if (bad_badva(ep)) {
					ep->ef_badvaddr = vaddr;
					break;
				}
				/* NOT the bad_badva problem */
				ep->ef_badvaddr = vaddr;
			} else
				break;
		}
#endif
		if (ut->ut_code == EFAULT) {
			caddr_t bvaddr = (caddr_t)ep->ef_badvaddr;
			int rv = 0;

			if (R4000_badva_war && code == SEXC_SEGV &&
			    IS_KUSEG(bvaddr) && bad_badva(ep))
				break;

			if (IS_KUSEG(ep->ef_sp) &&
					(rv = grow((caddr_t)ep->ef_sp)) == 0)
				break;
			if (IS_KUSEG(bvaddr) && bvaddr && grow(bvaddr) == 0)
				break;

			/*
			 * If the process is in speculative mode, let it
			 * go through.
			 */
			if ((ut->ut_pproxy->prxy_fp.pfp_fpflags & P_FP_SPECULATIVE) &&
			    (check_speculative(ep) == 0))
				break;

			/*
			 * failed to grow - if we ran out of process space,
			 * print message.  Print message iff the signal is
			 * set to default action.  If the process is handling
			 * or ignoring the signal, then assume it knows what
			 * it's doing and don't print the message.  This is
			 * just in case the program tries to resume the
			 * faulting instruction without fixing the SEGV
			 * condition.  This leads to an infinite SIGSEGV
			 * loop which would flood the console and log daemon
			 * with thousands of these messages per second.
			 */
			ut->ut_code = FAULTEDSTACK;

			if (rv == ENOSPC
			    && sigvp->sv_hndlr[SIGSEGV-1] == SIG_DFL)
			{
				cmn_err_tag(1754, CE_ALERT,
		"Process [%s] pid %d killed: process or stack limit exceeded",
				    pp->p_comm, pp->p_pid);
			}
			else if (rv == ENOMEM)
			{
				ut->ut_code = ENOMEM;
				
				if (sigvp->sv_hndlr[SIGSEGV-1] == SIG_DFL) {
				  	cmn_err_tag(1755, CE_ALERT,
		"Process [%s] pid %d killed: not enough memory to lock stack",
					pp->p_comm, pp->p_pid);
				}
			}
			else if (rv == EAGAIN
				 && sigvp->sv_hndlr[SIGSEGV-1] == SIG_DFL)
			{
                                cmn_err_tag(1756, CE_ALERT,
                "Process [%s] pid %d killed: not enough memory to grow stack", 
				    pp->p_comm, pp->p_pid);
                                sig = SIGKILL;
				sip = setsiginfo(&info, sig, 0, 0, 0);
				break;
			}
		}

		sig = SIGSEGV;
		sip = setsiginfo(&info, SIGSEGV,
			(ut->ut_code == EACCES) ? SEGV_ACCERR : SEGV_MAPERR, 0,
			(caddr_t) ep->ef_badvaddr);
		break;

	case EXC_II:
	case SEXC_CPU:
		/* attempt to emulate some new MIPS II instructions
		 * like ldc1/sdc1 by calling fixade
		 * XXX note that unaligned ldc1 on Mips I will succeed
		 */
		if (fixade(ep, cause))
			break;
		ut->ut_code = code;
		sig = SIGILL;
		sip = setsiginfo(&info, sig,
				code == EXC_II ? ILL_ILLOPC : ILL_COPROC,
				0,
				(caddr_t) ((ep->ef_cause & CAUSE_BD) ?
				((uint)ep->ef_epc)+4 : ep->ef_epc));
		break;

	case EXC_OV:
		ut->ut_code = BRK_OVERFLOW;
		sig = SIGFPE;
		sip = setsiginfo(&info, sig, FPE_INTOVF, 0,
				(caddr_t) ((ep->ef_cause & CAUSE_BD) ?
				((uint)ep->ef_epc)+4 : ep->ef_epc));
		break;

	case EXC_DBE:
	case EXC_IBE:
	{
#if IP21
		/*
		 * Workaround for IP21 bug: if another cpu is
		 * doing a probe, we may get a spurious bus
		 * error. Clear and ignore it.
		 */
		extern int probe_cpu, probe_in_progress;

		if (probe_in_progress && probe_cpu != cpuid()) {
			(void)dobuserre(ep, (inst_t *)ep->ef_epc, 1);
			break;
		}
		vmedbe = uvme_async_read_error(1);
		if (vmedbe == VME_DBE_IGNORE) {
			(void)dobuserre(ep, (inst_t *)ep->ef_epc, 1);
			break;
		}
		(void)dobuserre(ep, (inst_t *)ep->ef_epc, 
			(vmedbe == VME_DBE_NOTFND) ? 2 : 1);
#else /* !IP21 */
#ifdef _NEW_UME_SIG
		pfn_t	pfn; 
#endif /* _NEW_UME_SIG */
		int rv;

#if (defined R10000) && (defined T5TREX_IBE_WAR)
		if ((code == EXC_IBE) && ibe_isbogus(ep)) {
#ifdef DEBUG
			cmn_err(CE_NOTE, "Hit bogus IBE vaddr 0x%lx\n",
				ep->ef_epc);
#endif
			bogus_ibe++;
			goto done;
		}
#endif

		/*
		 * For IP5:
		 *	Return value of < 0 means that the faulting instruction
		 *	has been emulated successfully.  The user PC address
		 *	has been adjusted to skip the faulted instruction.
		 */
		rv = dobuserre(ep, (inst_t *)ep->ef_epc, 2);
#ifdef _MEM_PARITY_WAR
		if (rv > 0) 
			goto done; /* fixed up somehow--retry */
#endif /* _MEM_PARITY_WAR */
#if defined (SN0)
		/*
		 * on SN0 systems, a return value of > 0 indicates
		 * non-fatal error. Retry current operation.
		 */ 
		if (rv > 0)
		        goto done;
#endif /* SN0 */
#endif /* !IP21 */
#ifndef TFP
		/* On TFP this isn't valid -- we get an imprecise Bus Error
		 * and can't really distinguish IBE from DBE.
		 */
		if (code == EXC_DBE) {
			if (rv < 0)
				break;
			/*
		  	 * interpret instruction to calculate faulting address
		 	 */
			ep->ef_badvaddr = ldst_addr(ep);
		}
#endif
		ut->ut_code = code;

#ifdef _NEW_UME_SIG
		vtop((uvaddr_t)ep->ef_badvaddr, 1, &pfn, 1);
		if (pfdat_ishwbad(pfntopfdat(pfn))) { 
			sig = SIGUME;
			sip = setsiginfo(&info, sig, UME_ECCERR, 0,
                                         (caddr_t) ep->ef_badvaddr);
		} else {
			sig = SIGBUS;
			sip = setsiginfo(&info, sig, BUS_ADRERR, 0,
					 (caddr_t) ep->ef_badvaddr);
		}
#else
		sig = SIGBUS;
		sip = setsiginfo(&info, sig, BUS_ADRERR, 0,
				 (caddr_t) ep->ef_badvaddr);
#endif /* _NEW_UME_SIG */
		break;
	}

	case EXC_RADE:
	case EXC_WADE:
		if ((ut->ut_flags & UT_FIXADE) && fixade(ep, cause))
			break;
		if ((ut->ut_pproxy->prxy_fp.pfp_fpflags & P_FP_SPECULATIVE) &&
		    (check_speculative(ep) == 0))
			break;
		ut->ut_code = code;
		sig = SIGBUS;
		sip = setsiginfo(&info, sig, BUS_ADRALN, 0,
				 (caddr_t) ep->ef_badvaddr);
		break;

	case SEXC_PAGEIN:
		/*
		 * Yuck! We use forcedb as a flag to force a call to
		 * prfault() since sig will not be set.
		 */
		forcedb = 1;
		ut->ut_code = FAULTEDPAGEIN;
		break;

	case SEXC_WATCH:
		/* watchpoint detected */
		ASSERT(ut->ut_code == FAULTEDWATCH || ut->ut_code == FAULTEDSCWATCH);
		sig = SIGTRAP;
		sip = setsiginfo(&info, sig, TRAP_TRACE, 0,
				 (caddr_t) ep->ef_epc);
#if WATCHDEBUG
		{ pwatch_t *pw = ut->ut_watch;
		  ASSERT((pw->pw_flag & (WSTEP|WINTSYS)) == 0);
		}
#endif
		break;

	case EXC_BREAK:
		{
		int subcode;
		union mips_instruction inst;

		/* breakpoints are also used to detect certain arithmetic
		 * anomalies
		 */
		/*
		 * To re-instate a particular compatability with the
		 * MIPS kernel, we now set up to save the trap code
		 * for later delivery to the user via the code argument
		 * to the trap handler.  The related "u_trapcause" field
		 * is only used when tracing programs, which nobody
		 * (not even MIPS) seems currently to use or support, thus
		 * I am leaving it out for now.
		 */
		vaddr = ep->ef_epc;
		if (ep->ef_cause & CAUSE_BD)
			vaddr += 4;
		inst.word = brki = fuiword((caddr_t)vaddr);
		/*
		 * For a pthread program, while the processor took a brk
		 * exception and came here, another pthread might already
		 * have replaced the break with the real instruction. This
		 * happens when the kernel plants brkpts (watchpt code)
		 */
		if ((ut->ut_flags & UT_PTHREAD) 
				&& ((inst.f_format.opcode != spec_op) 
				|| (inst.f_format.func != break_op))) {
			ASSERT((ut->ut_flags & UT_SRIGHT) == 0);
			goto done;
		}
		subcode = ut->ut_code = BRK_SUBCODE(brki);
#if WATCHDEBUG
		if (brki == 0xffffffff || (brki & BRK_MASK) != 0xd) {
			cmn_err(CE_CONT, "bad brki 0x%x\n", brki);
		}
#endif
		/* now remove any steppoints */
		remove_bp();
		if (ut->ut_flags & UT_WSYS) {
			if (clrsyswatch()) {
				cmn_err(CE_PANIC,
					"EXC_BREAK Interested watchpoint!");
			}
		}

		switch (subcode) {
		case BRK_SSTEPBP:	/* user bp (used by debuggers) */
			if (ut->ut_flags & UT_STEP) {
                                int oldf;
                                int s = ut_lock(ut);

				/* done stepping */
				oldf = ut->ut_flags;
				ut->ut_flags &= ~(UT_STEP | 
						  UT_SRIGHT | UT_PTSTEP);
				ut_unlock(ut, s);

				if (oldf & UT_SRIGHT) 
					prsright_release(ut->ut_pproxy);

				if (oldf & UT_PTSTEP)
					goto done;
				  
				/*
				 * if this was a watchpoint step, no one should
				 * see it - clear it and continue
				 */
				if (ut->ut_watch && 
				    (ut->ut_watch->pw_flag & WSTEP)) {
					if (stepoverwatch())
						goto done;
				}
			} else {
   			        if (ut->ut_flags & UT_PTHREAD) {
				  	ut_flagset(ut, UT_STEP | UT_PTSTEP);
					goto done;
				}
				/* treak like a generic breakpoint instr */
				subcode = BRK_USERBP;
			}
			/* FALL THROUGH */

		case BRK_USERBP:	/* user bp (used by debuggers) */
			sig = SIGTRAP;
			sip = setsiginfo(&info, sig,
					(subcode == BRK_SSTEPBP) ? TRAP_TRACE :
								   TRAP_BRKPT,
					0, (caddr_t) NULL);
			break;
			
		/* 
		 * For the new few cases try and keep old applications working
		 * but also follow the SVID and MIPS ABI.
		 * The goal is to return SIGFPE for a integer
		 * divide by zero to applications that 
		 * expect it and to return SIGTRAP to 
		 * early IRIX5 applications that do not have a 
		 * SIGFPE handler.
		 */
		case BRK_DIVZERO:	/* divide by zero check */
		case BRK_OVERFLOW:	/* overflow check */
		case BRK_MULOVF:	/* multiply overflow detected */
		case BRK_RANGE:		/* range error check */
		{
			int sicode;

			if (sigismember(&sigvp->sv_sigcatch, SIGFPE)) {
				sig = SIGFPE;

				switch (subcode) {
				case BRK_DIVZERO:
					sicode = FPE_INTDIV;
					break;

				case BRK_OVERFLOW:
				case BRK_MULOVF:
					sicode = FPE_INTOVF;
					break;

				case BRK_RANGE:
					sicode = FPE_FLTSUB;
					break;
				}
			} else {
				sig = SIGTRAP;
				sicode = -subcode;
			}

			sip = setsiginfo(&info, sig, sicode, 0,
					 (caddr_t) ep->ef_epc);
		}
			break;
#if SW_FAST_CACHE_SYNCH
		case BRK_CACHE_SYNC:
			ep->ef_epc += 4;  /* skip past break instruction */
			ep->ef_v0 = cachesynch((caddr_t)ep->ef_a0,  ep->ef_a1);
			break;
#endif
		default:
			sig = SIGTRAP;
			sip = setsiginfo(&info, sig, -subcode, 0, NULL);
			break;
		}
		}
		break;

	case EXC_TRAP:
		/*
		 * Catch user programs executing trap instruction with no
		 * debugger attached. We don't want to just return to the
		 * program and let it spin forever.
		 */
		if (USERMODE(sr) && !(ut->ut_flags & UT_PRSTOPBITS))
			sig = SIGTRAP;
		break;

	default:
		panicregs(ep, code, sr, cause, "trap");
	}

	/* give /proc a chance to catch faults: */
	if ((pp->p_flag & SPROCTR) &&
	    (sig || forcedb) &&
	    (private.p_kstackflag != PDA_CURINTSTK)) {
#if WATCHDEBUG
		{ pwatch_t *pw = ut->ut_watch;
		  ASSERT(!USERMODE(sr) || pw == NULL  || forcedb \
		      || (pw->pw_flag & (WSTEP|WINTSYS)) == 0);
		}
#endif
		prfault(ut, code, &sig);
#if WATCHDEBUG
		{ pwatch_t *pw = ut->ut_watch;
		  ASSERT(!USERMODE(sr) || pw == NULL  || forcedb \
		      || (pw->pw_flag & (WSTEP|WINTSYS)) == 0);
		}
#endif
	}

	if (sig != 0) {
#ifdef DEBUG
		if (traptrace == -1 || traptrace == pp->p_pid) {
			cmn_err(CE_CONT, "sending sig %d to pid %d [%s]\n",
				sig, pp->p_pid, pp->p_comm);
			if (kdebug && (tracedrop == -1 ||
			    tracedrop == pp->p_pid))
				debug("ring");
		}
		if ((sig == SIGBUS || sig == SIGSEGV) &&
		    (segvtrace == -1 || segvtrace == pp->p_pid) &&
		    !sigismember(&sigvp->sv_sigcatch, sig)) {
			cmn_err(CE_CONT, "sending sig %d to pid %d [%s]\n",
				sig, pp->p_pid, pp->p_comm);
			if (kdebug && (segvdrop == -1 || segvdrop == pp->p_pid))
				debug("ring");
		}
#endif

		/*
		 * If the signal we want to send is being ignored or held,
		 * then we send the process SIGKILL instead.  Otherwise, the
		 * process will simply repeat the operation that caused the
		 * trap and sit in an infinite loop.
		 */

		if (sigismember(&sigvp->sv_sigign, sig) ||
		    sigismember(ut->ut_sighold, sig)) {
			cmn_err_tag(1757, CE_ALERT, "Process [%s] %d generated trap, but has signal %d held or ignored",
				pp->p_comm, pp->p_pid, sig);
			cmn_err(CE_CONT,
				"\tepc 0x%x ra 0x%x badvaddr 0x%x\n",
				ep->ef_epc, ep->ef_ra,ep->ef_badvaddr);

			if (sip) {
				cmn_err(CE_CONT,
				    "si_addr 0x%x, si_code %d, si_errno %d\n",
					sip->si_addr, sip->si_code, 
					sip->si_errno);
				sip = NULL;
			}

			if (sig == SIGSEGV && ut->ut_code == FAULTEDSTACK &&
			    !sigismember(ut->ut_sighold, sig)) {
				/*
				 * Here if rlimit_stack exhausted.
				 * X/Open requires SIGSEGV disposition to
				 * be reset to SIG_DFL - which will kill the
				 * process. We do this only if they don't
				 * have SEGV held - in that case, the process
				 * will get infinite exceptions - exactly
				 * what we are trying to avoid.
				 */
				CURVPROC_SET_SIGACT(sig, SIG_DFL, 0, 0, NULL);
			} else {
				cmn_err(CE_CONT, "Process has been killed to prevent infinite loop\n");
				sig = SIGKILL;
			}
		}

		if (sip) {
			i = ut_lock(ut);
			sigdelset(ut->ut_sighold, sip->si_signo);
			ut_unlock(ut, i);
		}

		/*
		 * Deliver the signal (and, if we have siginfo, add it
		 * to this thread's sigqueue).
		 */
		sigtouthread(ut, sig, sip);
		
		if (PCB(pcb_bd_ra)) {
			ep->ef_epc = PCB(pcb_bd_epc);
			ep->ef_cause = PCB(pcb_bd_cause);
			PCB(pcb_bd_ra) = 0;
		}
	}

done:
	/*
	 * If we got a bus error interrupt, just return here.
	 * We certainly don't want to do a qswtch()!
	 */
	if (private.p_kstackflag != PDA_CURINTSTK)
		trapend(ut, ep->ef_sr, ep, syst);

	CHECK_DELAY_TLBFLUSH(EXIT);
}

/*
 * trapstart - common start
 */
/* ARGSUSED */
static void
trapstart(
	register uthread_t *ut,
	register eframe_t *ep,
	uint code)
{
#if TFP
	/*
	 * Can't enable interrupts if it was bus error on TFP since
	 * we'll just get another bus error.
	 */
	if (!(ep->ef_cause & CAUSE_BE))
#endif
	{ /* braces needed for non-debug TFP kernels */
		ASSERT(!issplhi(getsr()));
	}
	ASSERT(ut->ut_cursig == 0);

	if (ut->ut_flags & UT_PTHREAD)
		ut_flagset(ut, UT_INKERNEL);

	/*
	 * we use ssi_cnt to determine whether to remove breakpoints or
	 * not - UT_STEP - is used to signify our desire to
	 * set them - however, it is under the control of /proc so
	 * any event that could cause /proc to believe that we're stopped
	 * (like getting a fault while trying to remove breakpoints!)
	 * allows it to turn on/off the UT_STEP flag - so we
	 * cannot trust it
	 */
	if ((PCB(pcb_ssi.ssi_cnt) != 0) && (code != EXC_BREAK)) {

		int	numbrks = 0;
		int	save = 0;

		/*
		 * If we are getting a trap just before executing the
		 * sstep brk pt, remember where we have the brk pts
		 * installed, so that we do not calculate new brk pt
		 * addresses while returning from this trap. This
		 * prevents multistepping when we want to single step.
		 * Remembering is done by keeping ssi_cnt set. Bug 522347.
		 */
		numbrks = PCB(pcb_ssi.ssi_cnt);
		if (epc_in_pcb(ep->ef_epc)) save = 1;
		remove_bp();
		if (save) PCB(pcb_ssi.ssi_cnt) = numbrks;
	}
	if (ut->ut_flags & (UT_SRIGHT|UT_WSYS)) {
		if ((ut->ut_flags & UT_SRIGHT) && code != EXC_BREAK) {
			ut_flagclr(ut, UT_SRIGHT);
			prsright_release(ut->ut_pproxy);
		}
		
		if (ut->ut_flags & UT_WSYS) {
			if (clrsyswatch()) {
			        cmn_err(CE_PANIC, 
					"trapstart Interested watchpoint!");
			}
		}
	}
}

/*
 * trapend - common end (except for system calls)
 *	1) check for signals
 *	2) check for dispatch
 *	3) check for graphics requirements
 * WARNING - make sure that its ok to do the operation whether the process
 *	is returning to user mode or kernel mode - if not then user USERMODE()
 *	to protect the code.
 * Return 1 if we processed any signals
 */
static void
trapend(register uthread_t *ut, machreg_t sr, eframe_t *ep, time_t syst)
{
	register proc_t *pp = UT_TO_PROC(ut);
	int junk;

	/*
	 * Handle end of trap checking for low-probability events.
	 * NOTE: we don't bother to lock siglck unless we're changing it.
	 *
	 * Several things along the path can sleep and add to the
	 * p_flag/ut_flags fields, and if this happens, we want to catch it.
	 */

	if (USERMODE(sr)) {
		if (ut->ut_flags & (UT_PRSTOPBITS|UT_BLOCK)) {

			if (ut->ut_flags & UT_PRSTOPBITS) {
				/* /proc requested */
				stop(ut, REQUESTED, 0, 0);
			}
			if (ut->ut_flags & UT_BLOCK) {
				ut_flagclr(ut, UT_BLOCK);
				blockme();
			}
		}
		if (pp->p_flag & SPROF)
			trapend_profile(ut, syst);

		/* signals anyone? */
dosigs:

		/*
		 * If this is a pthreaded process, tell signallers that this
		 * uthread is out of the kernel, and no longer available to
		 * receive signals.
		 */
		if (ut->ut_flags & UT_INKERNEL) {
			if (ut->ut_update & UT_UPDSIG) {
				/*
			 	 * handle pthread group synchronization --
			 	 * only turn off those we caught
			 	 */
				ut_updclr(ut, UT_UPDSIG);
				dousync(UT_UPDSIG);
			}
			ut_flagclr(ut, UT_INKERNEL);
		}

		if (thread_interrupted(UT_TO_KT(ut)) || sigisready()) {
 			junk = 0;
			psig(&junk, NULL);
		}

		if (ut->ut_flags & UT_CKPT) {
			if (ut->ut_flags & UT_PTHREAD)
				ut_flagset(ut, UT_INKERNEL);
			stop(ut, CHECKPOINT, 0, 0);
			goto dosigs;
		}
		/*
		 * We may have stopped due to a /proc debugger
		 * We install steppoints at this late date so that we
		 * can figure out where the process will really start executing
		 * (signal handlers and /proc can change the EPC)
		 * Also, we may be trying to step over a watchpoint
		 * and /proc may have changed the state of the process
		 * when it was stopped.
		 *
		 * We always disable any set steppoints upon entering
		 * the system, since it is possible that the process re-enter
		 * the system BEFORE having actually stepped - at that time
		 * it may get a signal or be stopped via /proc, etc
		 * and where we should place the steppoints can change
		 * The UT_STEP bit goes off only when we have successfully
		 * stepped, or no longer wish to do so.
		 *
		 * A tricky scenerio is:
		 * We have a watchpoint on the page that we wish to
		 * step over. There are 2 cases - either we are interested
		 * in the watchpoint or not (the watchpoint could be the 4
		 * bytes we are stepping over).
		 * Both cases:
		 *	the install_bp below will do a suiword which will
		 *	cause us to hit the watchpoint - since WIGN is on
		 *	any we're in the system, we will simply turn on
		 *	UT_WSYS, validate the page and continue.
		 *	The clrsyswatch will clear UT_WSYS and re-active
		 *	the watchpoint.
		 *	Now the process will run in user space and
		 *	get a TLBMISS. The tlbmiss code will remove the
		 *	steppoints which will again cause a TLBMISS
		 *	this time from the system - where we will
		 *	have WIGN set and will silently validate
		 *	the page and continue removing the steppoints.
		 *	At this point, we need to remove the fact that
		 *	we got the watchpoint in the system and re-set
		 *	the watchpoint again so that when we
		 *	return to the TLBMISS handler for the user
		 *	it will check if the watchpoint is of interest.
		 */
		if (ut->ut_watch && (ut->ut_watch->pw_flag & WSTEP))
			checkwatchstep(ep);
		/*
		 * if either checkwatchstep() or /proc wants to single
		 * step process, insert steppoints
		 */
		if (ut->ut_flags & UT_STEP) {
		  	if (prsright_needed()) {
				ut_flagset(ut, UT_INKERNEL);
				prsright_obtain_self();
				goto dosigs;
			}
			install_bp(ut->ut_exception->u_eframe.ef_epc);
		}

		/*
		 * if we can (i.e. not stepping) reinstate
		 * any watchpoints we by mistake hit while in the system
		 */
		if (ut->ut_flags & UT_WSYS) {
			if (clrsyswatch()) {
				cmn_err(CE_PANIC,
					"trapend Interested watchpoint!");
			}
		}
	}
}

static void
trapend_profile(uthread_t *ut, time_t syst)
{
	proc_t *pp = UT_TO_PROC(ut);
	pwatch_t *pw;
	int fast_prof_mult;
	int i;

	/*
	 * If we're doing fast profiling, multiply tick count by
	 * number of fast ticks per slow tick to get an accurate count.
	 * DON'T clear the SPROF flag!!
	 */
	if (pp->p_flag & SPROFFAST) {
		fast_prof_mult = PROF_FAST_CNT;
#ifdef DEBUG
		if (pp->p_fastprofcnt > PROF_FAST_CNT+1) {
			cmn_err(CE_WARN,
			  "fast prof overflow (TRAP) for [%s] pid %d cnt %d\n",
				get_current_name(),
				pp->p_pid,
				pp->p_fastprofcnt);
			pp->p_fastprofcnt = PROF_FAST_CNT+1;
		}
#endif
		for (i = 0; i < pp->p_fastprofcnt; i++)
			profileregion(pp->p_fastprof[i], 1);
		pp->p_fastprofcnt = 0;
	} else
		fast_prof_mult = 1;

	if ((pw = ut->ut_watch) != NULL)
		pw->pw_flag |= WIGN;

	profileregion((inst_t *)ut->ut_exception->u_eframe.ef_epc,
			fast_prof_mult * (int)(ut->ut_prftime - syst));
	if (pw)
		pw->pw_flag &= ~WIGN;
}

static int
epc_in_pcb(k_machreg_t epc)
{
	int	numbrks = PCB(pcb_ssi).ssi_cnt;

	ASSERT((numbrks > 0) && (numbrks <= 2));
	if (PCB(pcb_ssi).ssi_bp[0].bp_addr == (inst_t *)epc)
		return(1);
	if (numbrks == 2)
		return((PCB(pcb_ssi).ssi_bp[1].bp_addr == (inst_t *)epc) ? 
							1 : 0);
	return(0);
}

/*
 * install_bp -- install breakpoints to implement single stepping
 */
static void
install_bp(k_machreg_t epc)
{
	inst_t inst;
	inst_t *target_pc;
	uthread_t *ut = curuthread;
	register pwatch_t *pw;
	caddr_t a;
	inst_t *addr0, *addr1;
	int numbrks = 0;

	ASSERT((ut->ut_flags & (UT_PTHREAD|UT_SRIGHT)) != UT_PTHREAD);

	/*
	 * The only reason this can be set is if we took an exception/
	 * intr just before executing a sstep brk (epc pointing to the
	 * brk pt instruction). Do not recompute brk pt addrs if we are
	 * going back to same place. Bug 522347.
	 */
	if (numbrks = PCB(pcb_ssi.ssi_cnt)) {
		if (epc_in_pcb(epc)) {
			/*
			 * Skip brk pt address computation.
			 */
			addr0 = PCB(pcb_ssi).ssi_bp[0].bp_addr;
			addr1 = (numbrks == 1) ? 
				NULL : PCB(pcb_ssi).ssi_bp[1].bp_addr;
		}
		else {
			/*
			 * We are going to go to a signal handler.
			 */
			addr0 = (inst_t *)epc;
			addr1 = NULL;
		}
		PCB(pcb_ssi.ssi_cnt) = 0;
	} else
		addr0 = addr1 = NULL;

	/* ignore any watchpoints these accesses may cause */
	if ((pw = ut->ut_watch) != NULL)
		pw->pw_flag |= WIGN;

	/*
	 * If user can't access where his pc points, we give up.
	 * They'll be getting a SIGSEGV shortly anyway!
	 */
	a = (caddr_t)epc;
	if ((inst = fuiword(a)) == (inst_t)-1)
		goto out;
	if (addr0 != NULL) {
		set_bp(addr0);
		if (addr1 != NULL) set_bp(addr1);
	} 
	else if (is_branch(inst)) {
		target_pc = branch_target(inst, (inst_t *)epc);
		/*
		 * Can't single step self-branches, so just wait
		 * until they fall through
		 */
		if (target_pc != (inst_t *)epc)
			set_bp(target_pc);
		set_bp((inst_t *)epc+2);
	} else
		set_bp((inst_t *)epc+1);
out:
	if (pw)
		pw->pw_flag &= ~WIGN;
}

static void
set_bp(inst_t *addr)
{
	register struct ssi_bp *ssibp;

	ssibp = &PCB(pcb_ssi).ssi_bp[PCB(pcb_ssi).ssi_cnt];
	ssibp->bp_addr = addr;
	/*
	 * Assume that if the fuiword fails, the write_utext will also
	 */
	ssibp->bp_inst = fuiword(addr);
	if (write_utext(addr, *sstepbp)) {
		PCB(pcb_ssi).ssi_cnt++;
	}
}

/*
 * remove_bp -- remove single step breakpoints from current process
 */
static void
remove_bp(void)
{
	register struct ssi_bp *ssibp;
	register pwatch_t *pw;
        uthread_t *ut = curuthread;

	if ((pw = ut->ut_watch) != NULL)
		pw->pw_flag |= WIGN;
	while (PCB(pcb_ssi).ssi_cnt > 0) {
                ASSERT((ut->ut_flags & (UT_PTHREAD|UT_SRIGHT)) != UT_PTHREAD);
		PCB(pcb_ssi).ssi_cnt--;
		ssibp = &PCB(pcb_ssi).ssi_bp[PCB(pcb_ssi).ssi_cnt];
		if (!write_utext(ssibp->bp_addr, ssibp->bp_inst))
			cmn_err(CE_CONT, "couldn't remove breakpoint\n");
	}
	if (pw)
		pw->pw_flag &= ~WIGN;
}

#if WATCHDEBUG
/*
 * check_bp -- check breakpoints to implement single stepping
 */
int
check_bp(vaddr)
unsigned *vaddr;
{
	unsigned inst;
	unsigned *target_pc;
	register int s;
	register struct ssi_bp *ssibp;

	if (PCB(pcb_ssi.ssi_cnt) == 0)
		return(0);

	/*
	 * If user can't access where his pc points, we give up.
	 * They'll be getting a SIGSEGV shortly anyway!
	 */
	if ((inst = fuiword(vaddr)) == (inst_t)-1)
		return(0);
	if (is_branch(inst)) {
		target_pc = branch_target(inst, vaddr);
		/*
		 * Can't single step self-branches, so just wait
		 * until they fall through
		 */
		s = 0;
		if (target_pc != vaddr) {
			ssibp = &PCB(pcb_ssi).ssi_bp[s++];
			if (ssibp->bp_addr != target_pc)
				return(0);
		}
		ssibp = &PCB(pcb_ssi).ssi_bp[s];
		if (ssibp->bp_addr == vaddr + 2)
			return(1);
	} else {
		ssibp = &PCB(pcb_ssi).ssi_bp[0];
		if (ssibp->bp_addr == vaddr + 1)
			return(1);
	}
	return(0);
}
#endif

int
is_branch(inst_t inst)
{
	union mips_instruction i;

	i.word = inst;
	switch (i.j_format.opcode) {
	case spec_op:
		switch (i.r_format.func) {
		case jr_op:
		case jalr_op:
			return(1);
		}
		return(0);

	case bcond_op:
		switch (i.i_format.rt) {
		case bltz_op:
		case bgez_op:
		case bltzal_op:
		case bgezal_op:
		case bltzl_op:
		case bgezl_op:
		case bltzall_op:
		case bgezall_op:
			return(1);
		}
		return(0);

	case j_op:
	case jal_op:
	case beq_op:
	case bne_op:
	case blez_op:
	case bgtz_op:
	case beql_op:
	case bnel_op:
	case blezl_op:
	case bgtzl_op:
		return(1);

	case cop0_op:
	case cop1_op:
	case cop2_op:
#if 0
	/*
	 * cop3_op is obsolete and reused in MIPS IV to mean cop1x_op.
	 * If we check for it here, we end up thinking that some cop1x
	 * instructions are branches. Since cop3 instructions have never
	 * really been used, I just ifdef'ed it out to avoid that problem.
	 */
	case cop3_op:
#endif
		switch (i.r_format.rs) {
		case bc_op:
			return(1);
		}
		return(0);
	}
	return(0);
}

/*
 * branch_target - return address where branch will go
 */
#define	REGVAL(x)	((x)?USER_REG((x)+EF_AT-1):0)
static inst_t *
branch_target(inst_t inst, inst_t *pc)
{
	union mips_instruction i;
	register short simmediate;

	i.word = inst;
	switch (i.j_format.opcode) {
	case spec_op:
		switch (i.r_format.func) {
		case jr_op:
		case jalr_op:
			return((inst_t *)(REGVAL(i.r_format.rs)));
		}
		break;

	case bcond_op:
		switch (i.i_format.rt) {
		case bltz_op:
		case bgez_op:
		case bltzal_op:
		case bgezal_op:
		case bltzl_op:
		case bgezl_op:
		case bltzall_op:
		case bgezall_op:
			/*
			 * assign to temp since compiler currently
			 * doesn't handle signed bit fields
			 */
			simmediate = i.i_format.simmediate;
			return((inst_t *)((__psunsigned_t)pc+4+(simmediate<<2)));
		}
		break;

	case j_op:
	case jal_op:
		return((inst_t *)((((__psunsigned_t)pc+4)&~((1<<28)-1)) | (i.j_format.target<<2)));

	case beq_op:
	case bne_op:
	case blez_op:
	case bgtz_op:
	case beql_op:
	case bnel_op:
	case blezl_op:
	case bgtzl_op:
		/*
		 * assign to temp since compiler currently
		 * doesn't handle signed bit fields
		 */
		simmediate = i.i_format.simmediate;
		return((inst_t *)((__psunsigned_t)pc+4+(simmediate<<2)));

	case cop0_op:
	case cop1_op:
	case cop2_op:
#if 0
	/*
	 * cop3_op is obsolete and reused in MIPS IV to mean cop1x_op.
	 * If we check for it here, we end up thinking that some cop1x
	 * instructions are branches. Since cop3 instructions have never
	 * really been used, I just ifdef'ed it out to avoid that problem.
	 */
	case cop3_op:
#endif
		switch (i.r_format.rs) {
		case bc_op:
			/*
			 * kludge around compiler deficiency
			 */
			simmediate = i.i_format.simmediate;
			return((inst_t *)((__psunsigned_t)pc+4+(simmediate<<2)));
		}
		break;
	}
#ifdef DEBUG
	cmn_err(CE_WARN, "illegal instruction in branch_target");
#endif
	return pc;
}

/*
 * Checks for any load instruction.  Works only for 
 * MIPS I and II opcodes.
 */
int
is_load(inst_t instruction)
{
	union mips_instruction inst;

	inst.word = instruction;
	switch(inst.i_format.opcode) {
	case lb_op:
	case lbu_op:
	case lh_op:
	case lhu_op:
	case lw_op:
	case lwl_op:
	case lwr_op:
	case ll_op:
	case lwu_op:
	case ld_op:
	case ldl_op:
	case ldr_op:
	case lld_op:
	case lwc1_op:
	case ldc1_op:
	case pref_op:
		return(1);

	case cop1x_op:
		if ((inst.r_format.func == lwxc1_op) ||
		    (inst.r_format.func == ldxc1_op))
			return 1;
	}
	return(0);
}

/*
 * Checks for any store instruction.  Works only for 
 * MIPS I and II opcodes.
 */
int
is_store(inst_t instruction)
{
	union mips_instruction inst;

	inst.word = instruction;
	switch(inst.i_format.opcode) {
	case sb_op:
	case sh_op:
	case sw_op:
	case swl_op:
	case swr_op:
	case sc_op:
	case sd_op:
	case sdl_op:
	case sdr_op:
	case scd_op:
	case swc1_op:
	case sdc1_op:
		return(1);

	case cop1x_op:
		if ((inst.r_format.func == swxc1_op) ||
		    (inst.r_format.func == sdxc1_op))
			return 1;
	}
	return(0);
}

#if defined(BADVA_WAR) || defined(MP_R4000_BADVA_WAR)
/*
 * ldst_addr - compute data address instruction at EF_EPC would reference
 * 2 versions - layered-up to allow caller to avoid
 * assumptions that the outer layer makes.
 */
/* ARGSUSED */
static int
bad_ldst_register(eframe_t *ep, inst_t inst)
{
	union mips_instruction i;

	i.word = inst;
	return(i.i_format.rs == (EF_K0 - (EF_AT - 1)) ||
	       i.i_format.rs == (EF_K1 - (EF_AT - 1)));
}

/*
 * valid_store_addr- if the store addr is a kernel address and it
 * is mapped, then it is a valid store addr. If it is a user addr-
 * forget it, just shoot the user.
 */
static int
valid_store_addr(eframe_t *ep)
{
	uint	st_addr;

	if (IS_KUSEG(ep->ef_epc))
		return (0);
	st_addr = ep->ef_badvaddr & TLBHI_VPNMASK;

	if (IS_KSEGDM(st_addr))
		return (1);

	if ((st_addr < KERNELSTACK) && (st_addr >= KSTACKPAGE))
		/*
		 * XXX- The above test will have to be 
		 * expanded to include the stack extension 
		 * page when it goes in.
		 */
		 return (1);

	if (IS_KSEG2(st_addr)) {
		
		pde_t *pde;

		pde = kvtokptbl(st_addr);
		if (pde && pg_isvalid(pde))
			return (1);
		return (0);
	}
	return (0);
}
#endif /* BADVA_WAR || MP_R4000_BADVA_WAR */

/*
 * ldst_addr - compute data address instruction at EF_EPC would reference
 * 2 versions - layered-up to allow caller to avoid
 * assumptions that the outer layer makes.
 */
static k_machreg_t
_ldst_addr(eframe_t *ep, inst_t inst)
{
	k_machreg_t base;
	union mips_instruction i;
	k_machreg_t *efr = (k_machreg_t *)ep;
	k_machreg_t offset;

	i.word = inst;
	base = (i.i_format.rs == 0) ? 0 : efr[EF_AT + i.i_format.rs - 1];

#if MIPS4_ISA
	if (i.i_format.opcode == cop1x_op) 
		offset = ((i.r_format.rt == 0) ? 
			  0 : efr[EF_AT + i.r_format.rt - 1]);
	else 
#endif
	        offset = i.i_format.simmediate;
	
	base = base + offset;
#if _MIPS_SIM == _ABI64
	/* An o32 program (Ux == 0) will never see the upper 32-bits of
	 * our 64-bit registers.  Make sure we have proper sign extension
	 * for the load/store address since the kernel has actually been
	 * performing 64-bit operations.
	 *
	 * Note that the SR_UX bit is now on for o32 programs - so this
	 * test should always fail...
	 */
	if ((IS_KUSEG(ep->ef_epc)) && (!(ep->ef_sr & SR_UX)))
		base = (k_machreg_t)(((long long)(base << 32)) >> 32);
#endif	
	return(base);
}

/*
 * ldst_addr - compute data address instruction at exception PC would reference
 */
k_machreg_t
ldst_addr(eframe_t *ep)
{
	register inst_t *pc;
	union mips_instruction i;

	pc = (inst_t *)ep->ef_epc;
	if (ep->ef_cause & CAUSE_BD)
		pc++;
	i.word = *pc;	/* theoretically can't fault */

	/*
	 * In MIPS4, cop1x_op has some indexed load store operations, so
	 * compute address in that case too.
	 */
	if ((i.i_format.opcode < ldl_op) && (i.i_format.opcode != cop1x_op))
#if IP30
		panicregs(ep, ep->ef_cause & CAUSE_EXCMASK,
			ep->ef_sr, ep->ef_cause,
			"Bus Error Exception on non-load/store instruction");
#else
		/* Not a load or store, so give up */
		return (0xbad00bad);
#endif	/* IP30 */
	return (_ldst_addr(ep, i.word));
}

/*
 * Return data being stored by a store {byte, word, long} instruction
 *
 * Warning: this routine will assertbotch if passed anything other than
 * a sb, sh, sw or swl instruction
 */
k_machreg_t
store_data(register eframe_t *ep)
{
	k_machreg_t *efr = (k_machreg_t *)ep;
	register inst_t *pc;
	union mips_instruction i;
	k_machreg_t data;

	pc = (inst_t *)ep->ef_epc;
	if (ep->ef_cause & CAUSE_BD)
		pc++;
	i.word = *pc;	/* theoretically can't fault */
	ASSERT(i.i_format.opcode >= sb_op && i.i_format.opcode <= sw_op);

	data = (i.i_format.rt == 0) ? 0 : efr[EF_AT + i.i_format.rt - 1];
	return (data);
}

/*
 * This trap routine is used during startup by wbadmemaddr() if it memory
 * faults.  Once the system is ``running'' this is no longer used.
 * Since this gets used before the user area exists we have to
 * use a global, nofault, to handle the fault catching.
 */
void
trap_nofault(register eframe_t *ep, uint code, machreg_t sr, machreg_t cause)
{
	register int i;

	if (earlynofault(ep, code) && nofault) {
		/* nofault handler */
		i = nofault;
		if (i < 1 || i >= NF_NENTRIES) {
			set_leds(LED_PATTERN_BOTCH);
			cmn_err(CE_PANIC, "trap_nofault: bad nofault");
		}
		nofault = 0;
		ep->ef_epc = (k_machreg_t)nofault_pc[i];
		return;
	}
	panicregs(ep, code, sr, cause, "trap_nofault");
	/* NOTREACHED */

}

#define	BITSPERWORD	(sizeof(__uint32_t)*NBBY)
		
/*
 * Low event syscall pre processing
 *
 * Don't over-optimize this function - many flags may change due to
 * the functions called - e.g. if we sleep, a debugger may turn on the
 * PRSTOP bit. We really need to reload the flags oftgen.
 */
static enum actions { action_none, action_stopped, action_abort }
presysactions(uthread_t *ut)
{
	proc_t *pp = UT_TO_PROC(ut);
	int utflag;
	enum actions action = action_none;

	if ((utflag = ut->ut_flags) & UT_STEP) {
		/*
		 * We can use UT_STEP test here rather than looking at
		 * ssi_cnt since there is NOTHING above this that
		 * can cause us to sleep and therefore look like
		 * a stopped process
		 */
		remove_bp();
		if (utflag & UT_SRIGHT) {
			ut_flagclr(ut, UT_SRIGHT);
			prsright_release(ut->ut_pproxy);
		}
		if (ut->ut_watch && (ut->ut_watch->pw_flag & WSTEP)) {
			/*
			 * we were stepping over a watchpoint that
			 * was right at a syscall -
			 * since we're in effect past the syscall
			 * instruction now, lets clean up the
			 * watchpoints
			 */
			(void) stepoverwatch();
		}
	}

	if (utflag & UT_PTHREAD)
		ut_flagset(ut, UT_INKERNEL);

	if (utflag = ut->ut_update & UTSYNCFLAGS) {
		/*
		 * handle share group synchronization --
		 * only turn off those we caught
		 */
		ut_updclr(ut, utflag);
		dousync(utflag);
	}

	if (ut->ut_flags & UT_BLOCK) {
		ut_flagclr(ut, UT_BLOCK);
#ifdef CKPT
		ut_flagset(ut, UT_BLKONENTRY);
#endif
		blockme();
#ifdef CKPT
		ut_flagclr(ut, UT_BLKONENTRY);
#endif
	}

	ASSERT(!(pp->p_flag & SPROCTR) || pp->p_trmasks);

	if ((pp->p_flag & SPROCTR) && pp->p_trmasks->pr_systrap) {
		/* do stop-on-syscall-entry test */
		__uint32_t m;

		ut_flagclr(ut, UT_SYSABORT);
		ut->ut_errno = 0;
		ut->ut_rval1 = 0;
		ut->ut_rval2 = 0;
		m = pp->p_trmasks->pr_entrymask.word
					[ut->ut_syscallno / BITSPERWORD];
		if (m & (1 << (ut->ut_syscallno % BITSPERWORD))) {
			stop(ut, SYSENTRY, ut->ut_syscallno, 1);
			action = action_stopped;
		}
		if (ut->ut_flags & UT_SYSABORT) {
			/*
			 * UT_SYSABORT may have been set by a debugger
			 * while the process was stopped.
			 * If so, don't execute the syscall code.
			 */
		        ut_flagclr(ut, UT_SYSABORT);
			return action_abort;
		}
	}

	return action;
}

/*
 * Low event syscall post processing
 *
 * Don't over-optimize this function - many flags may change due to
 * the functions called - e.g. if we sleep, a debugger may turn on the
 * PRSTOP bit. We really need to reload the flags oftgen.
 */
static void
postsysactions(
	struct sysent *callp,
	time_t syst,
	int error,
	rval_t *rvp)
{
	uthread_t	*ut = curuthread;
	struct proc	*pp = UT_TO_PROC(ut);
	int		utflag;
	int		backup_pc;

	utflag = ut->ut_update & UTSYNCFLAGS;
	if (utflag) {
		/* only turn off those we caught */
		ut_updclr(ut, utflag);
		dousync(utflag);
	}

	utflag = ut->ut_flags;
	if (ut->ut_flags & UT_BLOCK) {
		ut_flagclr(ut, UT_BLOCK);
		blockme();
	}

	/*
	 * Check for watchpoints in system -- before syscall exit check.
	 * Note that we can in fact be currently in the middle of
	 * stepping over an uninterested watchpoint - suppose
	 * the page where the syscall is located is being watched.
	 */
	if (utflag & UT_WSYS) {
		if (clrsyswatch()) {
			int sig = SIGTRAP;
			/* interested watchpoint */
			if (pp->p_flag & SPROCTR) {
				ut->ut_code = FAULTEDKWATCH;
				prfault(ut, SEXC_WATCH, &sig);
			}
			if (sig)
				sigtopid(current_pid(), sig, SIG_ISKERN,
					 0, 0, 0);
		}
	}

	ASSERT(!(pp->p_flag & SPROCTR) || pp->p_trmasks != NULL);

	/*
	 * Do stop-on-syscall-exit test.  If a ptrace(2) exit is in
	 * progress, don't stop the process.
	 */
	if ((pp->p_flag & SPROCTR) &&
	    (ut->ut_syscallno < ut->ut_pproxy->prxy_syscall->sc_nsysent) &&
	     pp->p_trmasks->pr_systrap) {
		__uint32_t m;

		m = pp->p_trmasks->pr_exitmask.word
					[ut->ut_syscallno / BITSPERWORD];
		/* record any errors, and return values */
		ut->ut_errno = error;
		ut->ut_rval1 = rvp->r_val1;
		ut->ut_rval2 = rvp->r_val2;

		if (m & (1 << (ut->ut_syscallno % BITSPERWORD))) {
			stop(ut, SYSEXIT, ut->ut_syscallno, 1);
		}
	}

	if (ut->ut_flags & UT_PRSTOPBITS) {
		stop(ut, REQUESTED, 0, 0);
	}

	if (pp->p_flag & SPROF) {
		/*
		 * If rval2 == 1, then we are the child
		 * returning from a fork system call.  In this
		 * case, ignore syst since our time was reset
		 * in fork.
		 */
		register pwatch_t *pw;
		register int child = 0;
		int fast_prof_mult = 1;

		if (callp && (callp->sy_flags & SY_SPAWNER) && rvp->r_val2)
			child = 1;
		/*
		 * If we're doing fast profiling, multiply
		 * tick count by number of fast ticks per
		 * slow tick to get an accurate count.
		 * Also, handle any accumulated fast prof ticks
		 */
		if (pp->p_flag & SPROFFAST) {
			int i;
			fast_prof_mult = PROF_FAST_CNT;
#ifdef DEBUG
			if (pp->p_fastprofcnt > PROF_FAST_CNT+1) {
				cmn_err(CE_WARN, "fast prof overflow (SYS) for [%s] pid %d cnt %d\n",

					get_current_name(),
					pp->p_pid,
					pp->p_fastprofcnt);
				pp->p_fastprofcnt = PROF_FAST_CNT+1;
			}
#endif
			for (i = 0; i < pp->p_fastprofcnt; i++)
				profileregion(pp->p_fastprof[i], 1);
			pp->p_fastprofcnt = 0;
		}
		if ((pw = ut->ut_watch) != NULL)
			pw->pw_flag |= WIGN;
		profileregion((inst_t *)ut->ut_exception->u_eframe.ef_epc,
			      fast_prof_mult * (child ? (int)ut->ut_prftime :
						(int)(ut->ut_prftime - syst)));
		if (pw)
			pw->pw_flag &= ~WIGN;
	}

	/*
	 * Deliver signals now (after all possible sigtoproc calls)
	 *
	 * If a signal interrupted a system call, then it is possible
	 * that we may want to restart the system call - this is done
	 * in psig, and occurs in 2 cases: 1) the process issued a
	 * sigaction with the SA_RESTART flag, or 2) the system call
	 * was interrupted by a signal that was entirely processed
	 * by the kernel and has gone away - e.g. the process was
	 * stopped by a job control signal that the process is not
	 * catching, or the process is being debugged, and procfs
	 * is tracing a signal, and procfs canceled the signal.
	 */
	backup_pc = error == EINTR && thread_interrupted(UT_TO_KT(ut));

	if (ut->ut_flags & UT_CKPT)
		stop(ut, CHECKPOINT, backup_pc, 0);
dosigs:
	/*
	 * If this is a pthreaded process, tell signallers that this uthread
	 * is out of the kernel, and no longer available to receive signals.
	 */
	if (utflag & UT_INKERNEL)
		ut_flagclr(ut, UT_INKERNEL);

	if (thread_interrupted(UT_TO_KT(ut)) || sigisready() || backup_pc)
		psig(&backup_pc, callp);

	/*
	 * Check for installation of steppoints LAST - after any possible
	 * signal handling
	 */
	if (ut->ut_flags & (UT_STEP|UT_WSYS)) {
		/*
		 * if single stepping this process, install breakpoints before
		 * returning to user mode.  Do this here
		 * so single stepping will work when signals are delivered.
		 */
	        if (ut->ut_flags & UT_STEP) {
		        if (prsright_needed()) {
			        ut_flagset(ut, UT_INKERNEL);
				prsright_obtain_self();
				goto dosigs;
			}
			install_bp(curexceptionp->u_eframe.ef_epc);
		}
		/*
		 * handle watchpoint side affects
		 * Do this last since install_bp and addupc can generate
		 * watchpoint traps. (and therefore turn UT_WSYS back on)
		 */
		if (ut->ut_flags & UT_WSYS) {
			if (clrsyswatch()) {
				cmn_err(CE_PANIC, "Interested watchpoint!");
			}
		}
	}
}

/*
 * Low event syscall error processing
 */
static int
postsyserrors(
	register struct sysent *callp,
	register int error)
{
	uthread_t *ut = curuthread;

	/* error handling code - infrequently executed */
	if (error == EFBIG && !(callp->sy_flags & SY_NOXFSZ))
		sigtopid(current_pid(), SIGXFSZ, SIG_ISKERN, 0, 0, 0);

	/* An illegal system call, let the process know. */
	if (callp == NULL)
		sigtopid(current_pid(), SIGSYS, SIG_ISKERN, 0, 0, 0);

	/* If an error, and we are not attempting to restart the
	 * system call, then indicate to the user-level syscall
	 * wrapper that an error occurred.
	 */
	if (error == EMEMRETRY) {
		if (vm_pool_retry_syscall(ut->ut_syscallno))
			ut->ut_exception->u_eframe.ef_epc -= 8;
		 else
			error = EAGAIN;
	}

	if (!(error == EINTR && thread_interrupted(UT_TO_KT(ut))))
		ut->ut_exception->u_eframe.ef_a3 = 1;

#ifdef DEBUG
	if (error < 0 || error > 2000 ||
	    syserrnotrace == -1 || syserrnotrace == error ||
	    syserrtrace == -1 || syserrtrace == curvprocp->vp_pid) {
		cmn_err(CE_CONT, "pid %d [%s] got error %d on syscall %d\n",
			current_pid(), get_current_name(),
			error, ut->ut_syscallno);
		if (kdebug && (tracedrop == -1 || tracedrop == current_pid()))
			debug("ring");
	}
#endif
	if (error == EWOULDBLOCK)
		error = EAGAIN;		/* ABI kludge !! */
	curexceptionp->u_eframe.ef_v0 = error;

	return error;
}

#if (_MIPS_SIM == _ABIO32) || (_MIPS_SIM == _ABIN32)
/*
 * If this syscall has a 64 bit argument, return a bitmap indicating
 * where that argument is.  Otherwise, return 0.
 */
int
syscall_has_64bit_arg(int sysnum, sysarg_t cmd)
{
	switch(sysnum) {
	case 40:		/* syssgi SGI_READB SGI_WRITEB */
		if (cmd == SGI_READB || cmd == SGI_WRITEB)
			return (1<<3);
		else if (cmd == SGI_RT_TSTAMP_MASK)
			return (1<<2);
		else
			return 0;
	case 19:		/* lseek */
		return (1<<1);	/* arg 1 is 64 bit off_t */

	case 53:		/* semsys */
		return (1<<4);	/* arg 4 to semctl is left justified union */

	case 134:		/* mmap */
		return (1<<5);	/* arg 5 if 64 bit off_t */

	case 187:		/* pread, pwrite */
	case 188:
		return (1<<3);	/* arg 3 is 64 bit off_t */

	case 112:		/* truncate, ftruncate */
	case 113:
		return (1<<1);	/* arg 1 is 64 bit off_t */
	}

	return 0;
}

/*
 * Repack the 64 bit arguments in syscallargs the way the compiler packs
 * them on the stack for o32 apps; that is, add a 32 bit pad where
 * the 64 bit argument is not aligned.
 */
static void
reload_args(
	struct sysent	*callp,
	k_machreg_t	*ap,
	sysarg_t	*syscallargs,
	uint		argmask)
{
	int oldarg=0, newarg=0;

	while (newarg < callp->sy_narg) {
		if (argmask & (1 << newarg)) {
			if (newarg & 1)
				oldarg++;
			syscallargs[oldarg++] = (sysarg_t) (ap[newarg] >> 32);
			syscallargs[oldarg++] = (sysarg_t) ap[newarg++];
		} else {
			syscallargs[oldarg++] = (sysarg_t) ap[newarg++];
		}
		ASSERT(oldarg < 9);
	}
}
#endif /* _ABIO32 || _ABIN32 */

#define DEFAULT_REGARGS 4 /* default no. of arguments to pickup, regardless
			   * of nargs specified in sysent of syscallsw
			   */

#ifdef DEBUG
struct sysent *__syscall_ent;
#endif
/*
 * syscall - system call entry point
 */
/*ARGSUSED1*/
int
syscall(uint sysnum)
{
	/*
	 * Use u_eframe and not the passed in arg from systrap.  Our
	 * compiler generates far better code when it knows that we are
	 * doing references off the U area vs an arg pointer dereference.
	 */
	uthread_t	*ut = curuthread;
	struct proc	*pp = UT_TO_PROC(ut);
	eframe_t 	*ep = &ut->ut_exception->u_eframe;
	struct sysent	*callp;
	k_machreg_t	*ap;
	int		regargs;
	int		nargs;
	time_t		syst;
	int		error;
	unsigned 	nsysent;
	int		fullrestore;
	int		refreshingargs = 0;
	int		indirect;
	rval_t		rv;

	ASSERT(!issplhi(getsr()));
	ASSERT(!KT_ISKB(UT_TO_KT(ut)));
	ASSERT(ut->ut_cursig == 0);
#if defined(DEBUG) || defined(SABLE)
	if (syscalltrace == -1 || syscalltrace == pp->p_pid)
		cmn_err(CE_CONT, "syscall %d by [%s,%d,%x] <%x,%x,%x,%x>\n",
			sysnum - SYSVoffset, get_current_name(),
			current_pid(), pp,
			ep->ef_a0, ep->ef_a1, ep->ef_a2, ep->ef_a3);
#endif

#ifdef _SHAREII
	/*
	 *	ShareII potentially charges for system call activity.
	 */
	SHR_SYSCALL();
#endif /* _SHAREII */

	ASSERT(UT_TO_KT(ut)->k_timers.kp_curtimer == AS_SYS_RUN);

	SYSINFO.syscall++;
	ut->ut_acct.ua_sysc++;
	ut->ut_syscallno = -1;
	_SAT_INIT_SYSCALL();

	ASSERT((ut->ut_flags & UT_WSYS) == 0);
	syst = ut->ut_prftime;
	error = 0;

	/*
	 * if was isolated then must do delay action
	 * to maintain sanity
	 */
	CHECK_DELAY_TLBFLUSH(ENTRANCE);

	/*
	 * Check done in exec.c to ensure that we have valid pointers here
	 * note: sysnum is unsigned
	 */
	nsysent = ut->ut_pproxy->prxy_syscall->sc_nsysent;
	callp = &ut->ut_pproxy->prxy_syscall->sc_sysent[0];
	sysnum -= SYSVoffset;
	if (sysnum >= nsysent) {
		#pragma mips_frequency_hint NEVER
		error = EINVAL;
		callp = NULL;
		goto bad;
	}
	ap = &ep->ef_a0;

	/*
	 * handle indirect system call (#0)
	 */
	if (sysnum == 0) {
		#pragma mips_frequency_hint NEVER
		/*
		 * indirect system call (syscall), first argument is
		 * sys call number
		 */
		indirect = 1;
		sysnum = (int)ep->ef_a0 - SYSVoffset;
		ap = &ep->ef_a1;
		regargs = DEFAULT_REGARGS - 1;

		if (sysnum >= nsysent) {
			error = EINVAL;
			/* Note: leave callp pointing at syscall #0 */
			goto bad;
		}
	} else {
		#pragma mips_frequency_hint FREQUENT
		indirect = 0;
		regargs = DEFAULT_REGARGS;
	}

	/*
	 * Record valid syscall index and advance call table pointer
	 */
	ut->ut_syscallno = sysnum;
	callp += sysnum;

	/*
	 * Get arguments for system call (time critical!)
	 */
refreshargs:
	nargs = callp->sy_narg - regargs;
	ut->ut_scallargs[0] = (sysarg_t)ap[0];
	ut->ut_scallargs[1] = (sysarg_t)ap[1];
	ut->ut_scallargs[2] = (sysarg_t)ap[2];
	ut->ut_scallargs[3] = (sysarg_t)ap[3];

	if (ABI_HAS_8_AREGS(ut->ut_pproxy->prxy_abi)) {
		#pragma mips_frequency_hint FREQUENT
		ut->ut_scallargs[4] = (sysarg_t)ap[4];
		ut->ut_scallargs[5] = (sysarg_t)ap[5];
		ut->ut_scallargs[6] = (sysarg_t)ap[6];
		ut->ut_scallargs[7] = (sysarg_t)ap[7];
	} else {
		#pragma mips_frequency_hint NEVER
		if (nargs > 0) {
			app32_int_t *ip = ((app32_int_t *)ep->ef_sp) + 4;

			/* Note: only executed for old 32-bit ABI apps! */
			do {
				ut->ut_scallargs[regargs++] =
					(sysarg_t)sfu32(&error, ip++);
			    } while (--nargs != 0);
		}
	}		

#if (_MIPS_SIM == _ABIO32) || (_MIPS_SIM == _ABIN32)
	if ((ABI_IS_IRIX5_N32(ut->ut_pproxy->prxy_abi)) &&
	    (callp->sy_flags & SY_64BIT_ARG)) {
		int argmask;
		if (argmask = syscall_has_64bit_arg(sysnum,
							ut->ut_scallargs[0]))
			reload_args(callp, ap, ut->ut_scallargs, argmask);
	}
#endif

	/*
	 * Handle start of system call checking for low-probability events.
	 */
	if (!refreshingargs 
	  && ((pp->p_flag & SPROCTR) ||
	      (ut->ut_flags & (UT_PTHREAD|UT_STEP|UT_BLOCK)) ||
	      (ut->ut_update & UTSYNCFLAGS))) {
		#pragma mips_frequency_hint NEVER
		switch (presysactions(ut)) {

		case action_abort:
			error = EINTR;
			goto bad;

		case action_stopped:
			/*
			 * Presysactions may result in this process stopping.
			 * For example, if this process is under a debugger.
			 * While this process is stopped, the debugger
			 * is allowed to set registers, and change
			 * arguments to system call (typically using
			 * ioctl(PIOCSREG ...)). Refresh arguments in
			 * ut->ut_scallargs with potentially new values
			 * in eframe.
			 */
			refreshingargs = 1;
			regargs = indirect ? DEFAULT_REGARGS -1
					   : DEFAULT_REGARGS;
			goto refreshargs;

		case action_none:
			break;
		}
	}
	if (IS_TSTAMP_EVENT_ACTIVE(RTMON_SYSCALL)) {
		#pragma mips_frequency_hint NEVER
		/*
		 * Each system call generates two trace events,
		 * one at entry and one at exit.  If the process
		 * is marked for par tracing, then arguments and
		 * return values are included.
		 */
		if (pp->p_flag & SPARSYS)
			fawlty_sys(ut->ut_syscallno, callp, ut->ut_scallargs);
		else if (utrace_bufsize && (utrace_mask & RTMON_SYSCALL))
			utrace(UTN('sysc', 'allE'),
			       UTPACK(current_pid(), sysnum), ep->ef_a0);
	}
	ASSERT(PCB(pcb_ssi.ssi_cnt) == 0);
	ASSERT(ut->ut_watch == NULL || (ut->ut_watch->pw_flag & WSTEP) == 0);

	/*
	 * Catch argument copyin errors at this point after presysactions
	 */
	if (error) {
		#pragma mips_frequency_hint NEVER
		goto bad;
	}
	rv.r_val1 = 0;
	rv.r_val2 = ep->ef_v1;
	fullrestore = callp->sy_flags & SY_FULLRESTORE;

	/* perform the system call */
#ifdef DEBUG
	__syscall_ent = callp;
#endif
	error = (*(sy_call_t)(callp->sy_call))(ut->ut_scallargs, &rv, sysnum);

	/*
	 * after fork both processes come back through
	 * here so the uthread and proc pointers can be different
	 */
	ut = curuthread;
	pp = UT_TO_PROC(ut);

#if USE_PTHREAD_RSA
	/* this is a hack in case this was a dyield syssgi call which switched
	 * RSAs.
	 */
	if ((ut->ut_sharena) &&
	    (ut->ut_prda) &&
	    (ut->ut_prda->sys_prda.prda_sys.t_nid != NULL_NID) &&
	    (ut->ut_rsa_runable)) {
		fullrestore = 1;
	}
#endif
	/*
	 * If error, call syscall error handling code (rare)
	 */
	if (error) {
		#pragma mips_frequency_hint NEVER
bad:
		error = postsyserrors(callp, error);
		fullrestore = 0;
	} else {
		/*
		 * if the system call is a sigreturn/setcontext we do not
		 * update the return value becase it would overwrite the
		 * return value of an interrupted system call.
		 */
		if (!fullrestore) {
			ep = &ut->ut_exception->u_eframe;
			ep->ef_v0 = rv.r_val1;
			ep->ef_v1 = rv.r_val2;
			ep->ef_a3 = 0;
		}
	}

	/*
	 * clear inuse fds
	 * Must be called in the error case also ..
	 */
	if (ut->ut_fdinuse)
		fdt_release();
	ASSERT((ut->ut_fdinuse == 0) && (ut->ut_fdmanysz == 0));

	/*
	 * handle end of system call checking for low-probability events
	 */
	if (ut->ut_flags & (UT_WSYS|UT_INKERNEL|
			    UT_PRSTOPBITS|UT_STEP|UT_BLOCK|UT_CKPT) ||
	    thread_interrupted(UT_TO_KT(ut)) ||
	    ut->ut_update ||
	    pp->p_flag & (SPROF|SPROCTR|SCKPT) ||
	    sigisready()) {
		#pragma mips_frequency_hint NEVER
		postsysactions(callp, syst, error, &rv);
	}

	/* NB: callp == NULL for illegal system calls */
	if (IS_TSTAMP_EVENT_ACTIVE(RTMON_SYSCALL)) {
		#pragma mips_frequency_hint NEVER
		/*
		 * Send syscall parameter information if
		 * process is traced by the user (via par).
		 */
		if ((pp->p_flag & SPARSYS) && callp)
			fawlty_sysend(ut->ut_syscallno, callp, ut->ut_scallargs,
				      rv.r_val1, rv.r_val2, error);
		else if (utrace_bufsize && (utrace_mask & RTMON_SYSCALL))
			utrace(UTN('sysc', 'allX'), 
			       UTPACK(current_pid(), sysnum), error);
	}

	/* Check if sat audit record should be created */
	_SAT_CHECK_FLAGS(error);

#ifdef DEBUG
	{
		int s = kt_lock(UT_TO_KT(ut));
		ASSERT(UT_TO_KT(ut)->k_mustrun == PDA_RUNANYWHERE ||
			UT_TO_KT(ut)->k_mustrun == private.p_cpuid);
		kt_unlock(UT_TO_KT(ut), s);
	}
#endif
	ASSERT(ut->ut_cursig == 0);

#if defined(DEBUG) || defined(SABLE)
	if (error &&
	    (syscalltrace == -1 || syscalltrace == pp->p_pid))
		cmn_err(CE_CONT, "syscall %d failed, err=%d\n", sysnum, error);
	if (error == EFAULT &&
	    (traptrace == -1 || traptrace == pp->p_pid)) {
		cmn_err(CE_CONT, "giving EFAULT to pid %d [%s]\n",
		    pp->p_pid, get_current_name());
		if (kdebug &&
		    (tracedrop == -1 || tracedrop == pp->p_pid))
			debug("ring");
	}
#endif

	CHECK_DELAY_TLBFLUSH(EXIT);
	ASSERT(!KT_ISKB(UT_TO_KT(ut)));
	ASSERT(!issplhi(getsr()));
#ifdef DEBUG
	__syscall_ent = NULL;
#endif
	ut->ut_syscallno = -1;

	/* return non-zero for resched */
	return (fullrestore|private.p_runrun);
}

/*
 * nonexistent system call -- signal bad system call.
 */
int
nosys(void)
{
	sigtopid(current_pid(), SIGSYS, SIG_ISKERN, 0, 0, 0);
	return EINVAL;
}

/*
 * package not installed -- return ENOPKG error  (STUBS support)
 */
int
nopkg(void)
{
	return ENOPKG;
}


/*
 * internal function call for uninstalled package -- panic system.  If the
 * system ever gets here, it means that an internal routine was called for
 * an optional package, and the OS is in trouble.  (STUBS support)
 */
void
noreach(void)
{
	set_leds(LED_PATTERN_PKG);
	cmn_err(CE_PANIC,"Call to internal routine of uninstalled package");
}

/*
 * Ignored system call
 */
int
nullsys(void)
{
	return 0;
}

/*
 * Interrupt for asynchronous bus errors.
 */
void
buserror_intr(eframe_t *ep)
{
	/*
	 * Bus errors on writes are asynchronous because of the write
	 * buffer.  Because the error is asynchronous, it's impossible
	 * to know whether it was the kernel or the user that caused
	 * the error (for example:  graphics programs can generate VME
	 * write cycles since the graphics FIFO is mapped into their
	 * address spaces).  So we really need to panic here, but
	 * during hardware bringup it's nice not to have to reboot.
	 *
	 * Addendum -- it would be very, very difficult to generate a
	 * write request that causes a bus error and go from kernel
	 * to user mode before the actual trap.  As long as no bus
	 * errors are caused late in syscall, we are safe in assuming
	 * that bus errors that are detected while in user mode are
	 * actually caused by users.
	 *
	 * Call dobuserr() for machine dependent bus error handling.
	 * In certain unusual conditions, we ignore the bus error.
	 */
	if (USERMODE(ep->ef_sr)) {
		uthread_t *ut = curuthread;
 
		if (dobuserr(ep, (inst_t *)ep->ef_epc, 2))
			return;
		if (ut) {
			k_siginfo_t info, *sip;

			/*
			 * If siginfo is turned on for SIGBUS, generate the
			 * info structure and add pass it along as we
			 * deliver the signal.
			 */
			sip = setsiginfo(&info, SIGBUS, BUS_ADRERR, 0,
					 (caddr_t) ep->ef_epc);
			if (sip)
				sigdelset(ut->ut_sighold, SIGBUS);
			sigtouthread(ut, SIGBUS, sip);

			PCB(pcb_resched) = 1;
		}
	} else {
		int nofaultset = private.p_nofault ||
			(curthreadp &&
			 (curthreadp->k_nofault == NF_BADADDR ||
			  curthreadp->k_nofault == NF_EARLYBADADDR));
		if (dobuserr(ep, (inst_t *)ep->ef_epc, nofaultset))
			return;
		k_trap(ep, SEXC_BUS, ep->ef_sr, ep->ef_cause);
	}
}

#ifdef MP_R4000_BADVA_WAR
__psint_t	tlbmiss_badbadva_vaddr;
__psint_t	tlbmiss_badbadva_epc;
pid_t		tlbmiss_badbadva_pid;
int		tlbmiss_badbadva_count=0;
#endif /* MP_R4000_BADVA_WAR */

/*
 * tlbmiss for kuseg (present, not valid)
 * 	vfault() returns 0 or SEXC_SEGV or SEXC_KILL or SEXC_PAGEIN (when
 *		the process is being PTRACED) or SEXC_EOP on pages that
 *		an R4000 can't handle properly (bug in R4000)
 * 	(vfault does tlbdropin)
 * utlbmiss access to k2seg (double tlb miss)
 * 	tfault() returns 0 or SEXC_SEGV
 * 	(tfault does tlbdropin for both pages )
 * 	(tfault could check validity of pte and call vfault,
 * 	 but for now I'll let it fault again if valid bit off)
 */
/* XXX doesn't deal w/ KPTE_SHDUBASE */
#define kptetova(k) (stob(((k) - KPTEBASE))
#define vatokpte(v) (btost(v) + KPTEBASE)

/* ARGSUSED3 */
int
tlbmiss(register eframe_t *ep, uint code, __psint_t vaddr, machreg_t cause) 
{
	extern int utlbfault[], utlbmiss[];
	int rval, type;
	time_t	syst;
	uthread_t *ut;
	/*REFERENCED*/
	proc_t *pp;
#if R4000 || R10000
	pde_t *pd;
#endif

	if (curthreadp) {
		ut = curuthread;
		if (KT_ISUTHREAD(curthreadp) && ut) {
			pp = UT_TO_PROC(ut);
		}
	}
	else {	/* XXX BADVA_WAR not taken into account ... */
#ifdef MP_R4000_BADVA_WAR
		ASSERT_ALWAYS(IS_KSEG2(vaddr) || bad_badva(ep));
#else
		ASSERT_ALWAYS(IS_KSEG2(vaddr));
#endif
	}

#ifdef DEBUG
	if (zerodebug && (vaddr == 0 || vaddr == KPTEBASE) && kdebug) {
		if (USERMODE(ep->ef_sr))
			cmn_err(CE_CONT, "process %d[%s] accessed 0! ep:0x%x\n",
			    pp->p_pid, pp->p_comm, ep);
		else
			cmn_err(CE_CONT,
			    "kernel thread %lld accessed 0! ep:0x%x\n",
			    curthreadp->k_id, ep);
		if (tracedrop == -1 || tracedrop == pp->p_pid)
			debug("ring");
	}
#endif

#if R4000 || R10000
#ifndef MP_R4000_BADVA_WAR
	/*
	 * This should really be an inline handler.
	 * The dual entry tlb of the R4000 causes us to get tlb invalid
	 * faults where previously we would have gotten a tlbmiss.
	 */
	if (IS_KSEG2(vaddr)) {
		if (pnum(vaddr - K2SEG) < syssegsz) {
			pd = kvtokptbl(vaddr);
			if (pg_isvalid(pd)) {
#ifdef MH_R10000_SPECULATION_WAR
                                /* pg_vr could be turned off prior to a DMA read
                                 * on the Moosehead R10000.  Turn it back on at
                                 * the first reference.
                                 */
                                if (IS_R10000() &&
                                    ! pg_ishrdvalid(pd))
                                        pg_sethrdvalid(pd);
#endif /* MH_R10000_SPECULATION_WAR */
				/*
				 * Tell tlbdropin to call tlbgetpid() by
				 * passing 1
				 */
				tlbdropin((unsigned char *)1L, (caddr_t)vaddr,
								pd->pte);
				return 0;
			} else if (! USERMODE(ep->ef_sr))
				panic("tlbmiss: invalid kptbl entry");
		} else if (! USERMODE(ep->ef_sr))
			panic("tlbmiss: bad KSEG2 address");
	}
#else	/* MP_R4000_BADVA_WAR */
	/*
	 * If get here with K0 or K1 address, then we've hit the bug.
	 * Note that we might hit the bug in KUSEG addresses as
	 * well. Just let these go through and generate a segv for the
	 * kernel. We'll look for SEGV in k_trap and catch all addresses
	 * which might have slipped through and check for the chip bug there.
	 */
	if (IS_KSEGDM(vaddr)) {
	  	/* Verify the bug conditions */
		if ((((ep->ef_epc & POFFMASK) == (NBPP - 4)) ||
		    ((ep->ef_epc & POFFMASK) == 0x000)) &&
		    bad_badva(ep)) {
			/* MP R4000 bug with bad badvaddr
			 * Bug requires MP intervention when code
			 * tlbmisses when executing through a
			 * page boundary.
			 */

			 static int k0badvacount = 0;

			/*
			 * This threshold should probably
			 * be a tuneable
			 */
			if (++k0badvacount == 100) {
				cmn_err_tag(1758, CE_WARN|CE_CPUID,
	     		"excessive R4000 badvaddr occurrances for K0 addresses - may be impacting performance!\n");
				k0badvacount = 0;
			}

		} else if (USERMODE(ep->ef_sr))
			/* This case should NOT occur */
			cmn_err(CE_WARN|CE_CPUID,
"tlbmiss(USER): invalid badvaddr 0x%x epc 0x%llx sr 0x%llx, process %d[%s]\n",
				vaddr, ep->ef_epc, ep->ef_sr,
				pp->p_pid, pp->p_comm);
		else {
			cmn_err_tag(1759, CE_WARN|CE_CPUID,
	    "tlbmiss(KERNEL): invalid badvaddr 0x%x epc 0x%llx sr 0x%llx\n",
				vaddr, ep->ef_epc, ep->ef_sr);
#ifdef DEBUG
			debug("ring");
#endif
		}
		return(0);
	}

	/* This should really be an inline handler.
	 * The dual entry tlb of the R4000 causes us to
	 * get tlb invalid faults where previously we would
	 * have gotten a tlbmiss.
	 */
	if (IS_KSEG2(vaddr)) {
		/* Check that we have a valid K2 address before indexing
		 * off the end of the kvtokptbl.
		 */
#ifdef MAPPED_KERNEL
		/* For some reason, mapped kernel addresses cause many
		 * occurances of this HW problem.
		 */
		if ((__psunsigned_t)vaddr < (K2BASE+MAPPED_KERN_SIZE)) {
			static int k2badwiredcount;
			int oldcnt;

			oldcnt = atomicAddInt( &k2badwiredcount, 1);
			if ((oldcnt % 500) == 0)
				cmn_err_tag(1760, CE_WARN|CE_CPUID,
				"excessive R4000 badvaddr occurrances (%d) for K2 wired kernel addresses - may be affecting performance!\n", oldcnt);
			return(0);
		}
#endif /* MAPPED_KERNEL */
		if ((pnum(vaddr - K2SEG) >= syssegsz) ||
		     USERMODE(ep->ef_sr)) {
			if (bad_badva(ep)) {
				/* MP R4000 bug with bad badvaddr
				 * Bug requires MP intervention when code
				 * tlbmisses when executing through a
				 * page boundary.
				 */

				static int k2badvacount = 0;

				/*
				 * This threshold should probably
				 * be a tuneable
				 */
				if (++k2badvacount == 100) {
					cmn_err_tag(1761, CE_WARN|CE_CPUID,
				"excessive R4000 badvaddr(1) occurrances for K2 addresses - may be affecting performance!\n");
					k2badvacount = 0;
				}
			} else if (USERMODE(ep->ef_sr))
				/* This case should NOT occur */
				cmn_err_tag(1762, CE_WARN|CE_CPUID,
     "tlbmiss(2): invalid badvaddr 0x%x epc 0x%llx sr 0x%llx, process %d[%s]\n",
					vaddr, ep->ef_epc, ep->ef_sr,
					pp->p_pid, pp->p_comm);
			else
				panicregs(ep, code, ep->ef_sr, cause,
					"TLBMISS: KERNEL FAULT (syssegsz)");
			return(0);
		}
		pd = kvtokptbl(vaddr);
		if ((!pg_isvalid(pd)) || USERMODE(ep->ef_sr)) {
			if (bad_badva(ep))
				/* MP R4000 bug with bad badvaddr
				 * Bug requires MP intervention when code
				 * tlbmisses when executing through a
				 * page boundary.
				 */
				/* NOP */ ;
			else if (USERMODE(ep->ef_sr))
				cmn_err_tag(1763, CE_WARN|CE_CPUID,
	    "tlbmiss(USER): invalid badvaddr 0x%x epc 0x%llx, process %d[%s]\n",
					vaddr, ep->ef_epc,
					pp->p_pid, pp->p_comm);
			else
				panicregs(ep, code, ep->ef_sr, cause,
					"TLBMISS: KERNEL FAULT");
		} else {
			/* Tell tlbdropin to call tlbgetpid() by passing 1 */
			tlbdropin((unsigned char *)1L, (caddr_t)vaddr, pd->pte);
		}
		return 0;
	}
#endif	/* MP_R4000_BADVA_WAR */
#endif	/* R4000 || R10000 */

	if (!curthreadp || !curuthread) {
		panicregs(ep, code, ep->ef_sr, cause,
						"TLBMISS: KERNEL FAULT");
		/* NOTREACHED */
	}

	/* Catch kernel references through errant pointers */
	ASSERT(private.p_ukstklo.pgi != 0);

	syst = ut->ut_prftime;

	if (IS_KUREGION(vaddr)) {
#if !TFP
		/*
		 * can't make this assertion on tfp since fp_intr loads the
		 * instruction at EPC and that can cause a tlbmiss if the
		 * text page is not mapped in the tlb...
		 */
		ASSERT(private.p_kstackflag != PDA_CURINTSTK);
#endif

		/*
		 * came from user mode
		 */
		if (USERMODE(ep->ef_sr)) {
			CHECK_DELAY_TLBFLUSH(ENTRANCE);
			trapstart(ut, ep, 0);
		}
#ifdef DEBUG
		if (pagefaulttrace == -1 || pagefaulttrace == pp->p_pid)
			cmn_err(CE_CONT,
				"page fault at 0x%x by 0x%x [%s] epc 0x%x\n",
				vaddr, pp, pp->p_comm, ep->ef_epc);
#endif
		rval = vfault((caddr_t)vaddr, (code == EXC_WMISS) ? W_WRITE :
			      (vaddr == ep->ef_epc) ? W_EXEC : W_READ,
			      ep, &type);
	} else {
#if NO_WIRED_SEGMENTS || FAST_LOCORE_TFAULT
#if MP_R4000_BADVA_WAR
	  /*
	   * check for beyond end of KPTESEG - that
	   * can't happen, so must be bogus BADVA
	   */
	  if (USERMODE(ep->ef_sr) && ((caddr_t)vaddr >= (caddr_t)KPTE_KBASE)) {
		goto badva;

          /* No Error if address is outside range for 32-bit process -
	   * probably due to bad BADVA    
	   */
	  } else if ((!ABI_IS_IRIX5_64(ut->ut_pproxy->prxy_abi) &&
	       ((caddr_t)vaddr >= (caddr_t) (KPTEBASE + (KUSIZE_32>>(PNUMSHFT-2)))))) {
		/* Following check is needed since N32 programs can actually
		 * generate a 64-bit address without generating an  ADE.
		 * If EPC is out of bounds it can't possibly be a BAD BADVADDR.
		 */
	  	if (ep->ef_epc >= KUSIZE_32)
			return(SEXC_SEGV);

		/* Following check is needed for N32 programs which perform a
		 * data reference outside the range of allowable 32-bit
		 * addresses.  We convert what might be a second level tlb
		 * badvaddr into the original address and see if it is a
		 * valid address for this epc.  If so, it's not the
		 * MP_R4000_BAD_BADVADDR_WAR problem.
		 *
		 * Note that since the SR_UX bit is turned on for o32
		 * programs as well, we need to perform the same tests
		 * for o32.
		 */
		ep->ef_badvaddr = (((caddr_t)vaddr - (caddr_t)KPTEBASE)
				/ sizeof(pde_t)) << PNUMSHFT;
		if (!bad_badva(ep)) {
			ep->ef_badvaddr = vaddr;
			return(SEXC_SEGV);
		}
badva:
#ifdef DEBUG
		/* this is not really an error (well, it is a HW bug in the
		 * MP R4000 chip which caused us to get an invalid BADVADDR)
		 * but it is "interesting", so we'll print a warning in DEBUG
		 * kernels if we see it occur.
		 */
		cmn_err(CE_WARN|CE_CPUID,
		"Entered tlbmiss with invalid vaddr 0x%x (bad BADVA) abi 0x%x\n",
			vaddr, ut->ut_pproxy->prxy_abi);
		cmn_err(CE_CONT|CE_CPUID,
		"process %d[%s] epc 0x%llx fpflags 0x%x\n",
			pp->p_pid, pp->p_comm, ep->ef_epc,
			ut->ut_pproxy->prxy_fp.pfp_fpflags);
		cmn_err(CE_CONT|CE_CPUID,"bad_badva 0x%x\n",bad_badva(ep));
#endif /* DEBUG */

		if ((tlbmiss_badbadva_count == 0) ||
		    (tlbmiss_badbadva_vaddr != vaddr) ||
		    (tlbmiss_badbadva_epc != ep->ef_epc) ||
		    (tlbmiss_badbadva_pid != pp->p_pid)) {
			tlbmiss_badbadva_count = 1;
			tlbmiss_badbadva_vaddr = vaddr;
			tlbmiss_badbadva_epc = ep->ef_epc;
			tlbmiss_badbadva_pid = pp->p_pid;
		} else {
			tlbmiss_badbadva_count++;
		}
		if (tlbmiss_badbadva_count == 32) {
			tlbmiss_badbadva_count = 0;
			cmn_err_tag(1764, CE_WARN|CE_CPUID,
			"Too many bad BADVA process %d[%s] epc 0x%llx\n",
				pp->p_pid, pp->p_comm, ep->ef_epc,
				ut->ut_pproxy->prxy_fp.pfp_fpflags);
			return(SEXC_SEGV);
		}
		return(0);
	  } else
#endif /* MP_R4000_BADVA_WAR */
		{
		/*
		 * If the process is in speculative mode, let it
		 * go through.
		 */
		if ((ut->ut_pproxy->prxy_fp.pfp_fpflags & P_FP_SPECULATIVE) &&
		    (check_speculative(ep) == 0))
			return(0);

		/*
		 * A user program referencing a virt addr > KUSIZE (currently
		 * 2^40) but <= the real hardware supported KUSIZE (2^44 on T5
		 * and 2^48 on TFP) will come here. Don't panic the system,
		 * just kill the process. For the !FAST_LOCORE_TFAULT case,
		 * tfault() will catch the problem.
		 */
		if (USERMODE(ep->ef_sr)) {
			cmn_err_tag(1765, CE_WARN|CE_CPUID,
	 			"Process %s pid %d EPC 0x%x "
				"referenced bad addr 0x%x",
				get_current_name(),
				pp->p_pid, ep->ef_epc, vaddr);
			return(SEXC_SEGV);
		}

		/*
		 * Check for idbg user level commands causing bogus references.
		 * Don't want to panic the system...
		 * In fact, if k_nofault is set, return sexc_segv so that
		 * locore drops into trap, which will invoke k_trap to do
		 * no fault handling.
		 */
		if (curthreadp->k_nofault)
			return(SEXC_SEGV);

		cmn_err(CE_PANIC,"Entered tlbmiss with invalid vaddr 0x%x\n",
				vaddr);
		}

#else /* !NO_WIRED_SEGMENTS */
		if (USERMODE(ep->ef_sr)) {
			CHECK_DELAY_TLBFLUSH(ENTRANCE);
			trapstart(ut, ep, 0);
		}
		rval = tfault(ep, code, (caddr_t)vaddr, &type);

		/*
		 * If the process is in speculative mode (and tfault
		 * failed), let it go through anyway.
		 */
		if ((rval != 0) &&
		    (ut->ut_pproxy->prxy_fp.pfp_fpflags & P_FP_SPECULATIVE) &&
		    (check_speculative(ep) == 0))
			rval = 0;

#endif /* !NO_WIRED_SEGMENTS */
	}

	/*
	 * If we are going to be doing trap(SEXC_*), delay trapend till 
	 * we are done with that. Bug 533989.
	 */
	if (rval == 0) trapend(ut, ep->ef_sr, ep, syst);
	if (USERMODE(ep->ef_sr))
		CHECK_DELAY_TLBFLUSH(EXIT);

	/* 
	 * set ut_code only if we're returning an error AND after
	 * trapend that can change it
	 */
	if (rval)
		ut->ut_code = type;

#if !TFP
	/* timer assert if tlbmiss succeeds */
	/*
	 * can't make this assertion on tfp since the fp interrupt handler
	 * loads the instruction at EPC and that can cause a tlbmiss if the
	 * text page is not mapped in the tlb...
	 */
        ASSERT(rval != 0 || UT_TO_KT(ut)->k_timers.kp_curtimer == AS_SYS_RUN);
#endif

	return(rval);
}

/*
 * Handle a write fault.  For users, just ask pfault to handle the fault.
 * For the kernel, this is a fatal error.
 */
int
tlbmod(register eframe_t *ep, uint code, __psint_t vaddr, machreg_t cause) 
{
	int rval, type;
	register time_t	syst;
	register uthread_t *ut = curuthread;

#ifdef MH_R10000_SPECULATION_WAR
        if (IS_KSEG2(vaddr) &&
            (! USERMODE(ep->ef_sr)) &&
            (pnum(vaddr - K2SEG) < syssegsz)) {
                /*
                 *      pg_m was turned off to prevent speculative stores
                 *      during a DMA read operation:  turn it back on
                 */
                pde_t *pd = kvtokptbl(vaddr);

                if (pg_isvalid(pd)) {
                        pg_setmod(pd);
                        tlbdropin(0,(caddr_t) vaddr,pd->pte);
                        return(0);
                }
        }
#endif /* MH_R10000_SPECULATION_WAR */

	syst = ut->ut_prftime;
	ASSERT(UT_TO_KT(ut)->k_timers.kp_curtimer == AS_SYS_RUN);

	if (USERMODE(ep->ef_sr)) {
		CHECK_DELAY_TLBFLUSH(ENTRANCE);
		trapstart(ut, ep, 0);
	}

	if (IS_KUSEG(vaddr)) {
		rval = pfault((caddr_t)vaddr, ep, &type, W_WRITE);
#ifdef TLBMOD_BADVADDR_WAR
		{
		  int workaround_r4600;
		  int workaround_r5000;
		  union rev_id ri;
		  extern int is_r4600_flag;
		  static __psint_t last_tlbmod_vaddr = -1;
		  static time_t    last_tlbmod_time  = -1;

		  /* 
		   * workaround_r4600
		   * Bug present in all R4600's and R4700 rev1.0.
		   * (Fixed in R4700 rev2.1 and R5000.)
		   * From R4700 errata list:
		   *  3. This specific sequence causes incorrect values to be 
		   *     seen in the BadVaddr and EnHi CP0 registers:
		   *
		   *     lw Addr1 - Addr1 causes a dcache miss
		   *
		   *     sw Addr2 - Addr2 causes a Data TLB Mod exception
		   *                Addr2 is a virtual address which is
		   *                currently mapped in a DTLB entry.
		   *                There is no DTLB miss stall for this
		   *                instruction.
		   *                
		   *     inst3    - last instruction on a mapped page
		   *                any instruction that doesn't cause
		   *                any slip or stall cycles
		   *
		   *     When the Data TLB Mod exception is taken, both BadVaddr
		   *     and EnHi registers hold incorrect virtual page number
		   *     values.
		   */
		  /*
		   * workaround_r5000:
		   * Bug present in R5000's, revs: 1.0, 2.0, 2.1
		   * No description in errata list yet.
	     	   * Scenario (One instruction sequence leading to this bug):
		   * 
		   *      lw Addr1 - Addr1 causes a dcache miss 
		   *      sb Addr2 - Addr2 causes a Data TLB Mod exception
		   *                 Addr2 corresponds to a mapped DTLB entry
		   * (The sb instr is at a 16byte aligned offset on even page
		   * Here again, after a spurious TLB mod exception is taken,
		   * BadVaddr holds incorrect values.
		   */
		  /* We ignore the SEGV and retry the instruction iff:
		   *  it's a different vaddr than last time, or it's the same
		   *  vaddr, but the system clock has changed (so we assume 
		   *  it's a different instance of the same bug).
		   */
		  
		  workaround_r4600 = (rval == SEXC_SEGV && is_r4600_flag && 
		      ((ep->ef_epc & 0xfff) == 0xff8) &&
		      (vaddr != last_tlbmod_vaddr || 
		       lbolt != last_tlbmod_time));
		  ri.ri_uint = get_cpu_irr();
		  workaround_r5000 = (rval == SEXC_SEGV &&
		  		      (ri.ri_imp == C0_IMP_R5000) &&
				      ((ep->ef_epc & 0xFFF) == 0xFF0) &&
				      (vaddr != last_tlbmod_vaddr ||
				       lbolt != last_tlbmod_time));
		  if (workaround_r4600 || workaround_r5000) {
			  last_tlbmod_vaddr = vaddr;
			  last_tlbmod_time = lbolt;
			  rval = 0;
		  }
	        }
#endif /* TLBMOD_BADVADDR_WAR */

		/*
	 	 * If we are going to be doing trap(SEXC_*), delay
	 	 * trapend till we are done with that. Bug 533989.
	 	 */
		if (rval == 0) trapend(ut, ep->ef_sr, ep, syst);
		if (USERMODE(ep->ef_sr))
			CHECK_DELAY_TLBFLUSH(EXIT);

		/*
		 * only set ut_code after trapend - which can effectively
		 * call back into here which would overwrite code
		 */
		if (rval)
			ut->ut_code = type;
		return(rval);
	}

#ifdef MAPPED_KERNEL
	if (IS_MAPPED_KERN_RO(vaddr)) {
		if (USERMODE(ep->ef_sr) ||
		    (curthreadp && curthreadp->k_nofault))
			return(SEXC_SEGV);
		else
			panicregs(ep, code, ep->ef_sr, cause,
				  "Tried to write read-only kernel space");
	}
#endif

	panicregs(ep, code, ep->ef_sr, cause, "BAD TLBMOD");
	/* NOTREACHED */
}

/*
 * Masks and constants for the rs field of "coprocessor instructions" (25-21)
 * which are branch on coprocessor condition instructions.
 */
#define	COPz_BC_MASK	0x1a
#define COPz_BC		0x08

/*
 * Masks and constants for the rt field of "branch on coprocessor condition
 * instructions" (20-16).
 */
#define	COPz_BC_CC_SHFT	2		/* only used by mips4 */

#define	COPz_BC_TF_MASK	0x01
#define	COPz_BC_TRUE	0x01
#define	COPz_BC_FALSE	0x00


/*
 * emulate_branch is used by fp_intr() to calculate the resulting pc of a
 * branch instruction.  It is passed a pointer to the exception frame,
 * the branch instruction and the floating point control and status register.
 * The routine returns the resulting pc.  Since called only when BD is set,
 * this routine gives SIGILL to the offending process if called with an
 * instruction it does not know how to emulate.
 */
k_machreg_t
emulate_branch(eframe_t *ep, inst_t instr, __int32_t fpcsr, int *branch_taken)
{
	k_machreg_t *efr = (k_machreg_t *)ep;
	union fpc_csr fpc_csr;
	union mips_instruction cpu_instr;
	long condition;
#if MIPS4_ISA
	long fcc;
#endif
	k_smachreg_t rs, rt;	/* these need to be signed */

	fpc_csr.fc_word = fpcsr;
	cpu_instr.word = instr;

	/*
	 * The values for the rs and rt registers are taken from the exception
	 * frame so we bias the load register by the location of AT which is
	 * register 1.
	 */
	rs = (cpu_instr.r_format.rs == 0) ? 0 :
		efr[cpu_instr.r_format.rs + EF_AT-1];
	rt = (cpu_instr.r_format.rt == 0) ? 0 :
		efr[cpu_instr.r_format.rt + EF_AT-1];

	switch (cpu_instr.i_format.opcode) {

	case spec_op:
		switch (cpu_instr.r_format.func) {
		case jalr_op:
			/* r31 has already been updated by the hardware */
		case jr_op:
			return((k_machreg_t)rs);
		}
		break;

	case jal_op:
		/* r31 has already been updated by the hardware */
	case j_op:
		return (((ep->ef_epc + 4) & PC_JMP_MASK) |
			(cpu_instr.j_format.target << 2));

	case beq_op:
	case beql_op:
		condition = rs == rt;
		goto conditional;

	case bne_op:
	case bnel_op:
		condition = rs != rt;
		goto conditional;

	case blez_op:
	case blezl_op:
		condition = rs <= 0;
		goto conditional;

	case bgtz_op:
	case bgtzl_op:
		condition = rs > 0;
		goto conditional;

	case bcond_op:
		switch (cpu_instr.r_format.rt) {
		case bltzal_op:
			/* r31 has already been updated by the hardware */
		case bltz_op:
		case bltzall_op:
		case bltzl_op:
			condition = rs < 0;
			goto conditional;

		case bgezal_op:
			/* r31 has already been updated by the hardware */
		case bgez_op:
		case bgezall_op:
		case bgezl_op:
			condition = rs >= 0;
			goto conditional;
		}
		break;

		/* The following code works for the R4000 unchanged */
	case cop1_op:
		if ((cpu_instr.r_format.rs & COPz_BC_MASK) == COPz_BC) {
#if MIPS4_ISA
			/*
			 * For mips4, there are 8 condition bits which are not
			 * all contiguous in the CSR and 3 bits in the instr
			 * that tell us which bit in the CSR to look at. All
			 * these new bits are set to 0 in old instructions and
			 * CSR on all FPU's, so it's safe to look at mips2/3
			 * instructions using the following code.
			 */
			fcc =  (fpc_csr.fc_struct.fcc << 1) |
				fpc_csr.fc_struct.condition;
			condition = cpu_instr.r_format.rt >> COPz_BC_CC_SHFT;
			condition = fcc & (1 << condition);
#else
			condition = fpc_csr.fc_struct.condition;
#endif
			if ((cpu_instr.r_format.rt & COPz_BC_TF_MASK)
			    != COPz_BC_TRUE)
				condition = !condition;
			goto conditional;
		}

	}
	/*
	 * For all other instructions (including branch on co-processor 2 & 3)
	 * we used to panic because this routine is only called when in the
	 * branch delay slot (as indicated by the hardware).  However, an
	 * evil program can put an interrupting fpu instruction in the delay
	 * slot of an illegal branch as a way to panic the system.  So
	 * we'll send SIGILL to such processes.
	 */
#ifdef DEBUG
	cmn_err_tag(1766, CE_WARN, "Unknown branch instruction = 0x%x", instr);
#endif
	sigtouthread(curuthread, SIGILL, (k_siginfo_t *)NULL);
	return ep->ef_epc + 8;

conditional:
	*branch_taken = condition;
	if (condition)
		return(ep->ef_epc + 4 + (cpu_instr.i_format.simmediate << 2));
	return(ep->ef_epc + 8);
}

/*
 * emulate_ovflw is used by k_trap() to handle an overflow in kernel space
 *	it simply wraps the register, prints a message and continues
 * returns: 1 if could handle instruction
 *	    0 if not
 */
static int
emulate_ovflw(eframe_t *ep, inst_t inst)
{
	k_machreg_t *efr = (k_machreg_t *)ep;
	union mips_instruction cpu_instr;
	k_machreg_t sum, rs, rt;

	cpu_instr.word = inst;

	/*
	 * The values for the rs and rt registers are taken from the exception
	 * frame and since there is space for the 4 argument save registers and
	 * doesn't save register zero this is accounted for (the +3).
	 */
	rt = rs = 0;
	if (cpu_instr.r_format.rs)
		rs = efr[cpu_instr.r_format.rs + 3];
	if (cpu_instr.r_format.rt)
		rt = efr[cpu_instr.r_format.rt + 3];

	switch (cpu_instr.i_format.opcode) {

	case addi_op:
		/* sign extend, then treat as unsigned */
		sum = rs + (unsigned)(long)(cpu_instr.i_format.simmediate);
		efr[cpu_instr.r_format.rt + 3] = sum;
		break;
	case spec_op:
		switch (cpu_instr.r_format.func) {
		case add_op:
			sum = rs + rt;
			efr[cpu_instr.r_format.rd + 3] = sum;
			break;
		case sub_op:
			sum = rs - rt;
			efr[cpu_instr.r_format.rd + 3] = sum;
			break;
		default:
			return(0);
		}
		break;
	default:
		return(0);
	}
	return(1);
}

extern int uload_half(caddr_t, k_machreg_t *);
extern int uload_uhalf(caddr_t, k_machreg_t *);
extern int uload_word(caddr_t, k_machreg_t *);
extern int uload_uword(caddr_t, k_machreg_t *);
extern int uload_double(caddr_t, k_machreg_t *);
extern int ustore_double(caddr_t, k_machreg_t);
extern int ustore_word(caddr_t, k_machreg_t);
extern int ustore_half(caddr_t, k_machreg_t);
/*
 * Fixade() is called to fix unaligned loads and stores.  It returns a
 * zero value if can't fix it and non-zero if it can fix it.  It modifies
 * the destination register (general or floating-point) for loads or the
 * destination memory location for stores.  Also the epc is advanced past
 * the instruction (possibly to the target of a branch).
 */
/*ARGSUSED*/
static int
fixade(register eframe_t *ep, machreg_t cause)
{
	k_machreg_t *efr = (k_machreg_t *)ep;
	union mips_instruction inst, branch_inst;
	__psint_t addr;
	k_machreg_t word, word2;
	k_machreg_t new_epc;
	__psint_t ip = ep->ef_epc;
	int error;
	pcb_t	*pcbp = &curexceptionp->u_pcb;

	/* Ensure EPC is from user mode.  The 7.1 dbx has a problem
	 * with n32 programs and can set bit 63 in the ra [#501779],
	 * which will cause a RADE, and let us get here with a bogus
	 * epc.  On IP30, the read (fuiword) can return bogus data,
	 * then the store will cause a bus error.
	 */
	/* if 32-bit kernel and 64-bit user, make sure we look at
	** 64-bit EPC. 
	*/
	if ( ((ep->ef_sr & SR_KX) == 0) && (ep->ef_sr & SR_UX) ) {
		/* EPC is out of user address range. Return;can't fix it */
		if ( ep->ef_epc >= (KUBASE + KUSIZE) )
			return 0;
	}
	if (IS_KUSEG(ip) == 0)
		return 0;

	if (cause & CAUSE_BD) {
		int branch_taken;

		branch_inst.word = fuiword((caddr_t)ip);
		inst.word = fuiword((caddr_t)(ip+4));
		if (branch_inst.i_format.opcode == cop1_op)
			checkfp(curuthread, 0);
		new_epc = emulate_branch(ep, branch_inst.word,
					 pcbp->pcb_fpc_csr, &branch_taken);
	} else {
		inst.word = fuiword((caddr_t)ip);
		new_epc = (k_machreg_t)(ip+4);
	}

#if MIPS4_ISA
	/* Emulate MIPS4 unaligned cop1x instructions */

	if (inst.r_format.opcode == cop1x_op) {
		__psint_t base, index;

	  	if ((cause & CAUSE_EXCMASK) == EXC_II)
			return(0);	/* Don't emulate for mips1 process */
		base = (__psint_t)REGVAL(inst.r_format.rs);
		index = (__psint_t)REGVAL(inst.r_format.rt);
		addr = base + index;
#ifndef TRITON
		if ((addr & KREGION_MASK) != (base & KREGION_MASK))
			return(0); /* must match or ADE not for alignment */
#endif /* TRITON */
		if ((addr >= K0BASE) || ((addr+3) >= K0BASE))
			return(0);
		checkfp(curuthread, 0);
		error = 0;

		if ((inst.r_format.func == lwxc1_op) ||
		    (inst.r_format.func == swxc1_op)) {
			if (inst.r_format.func == lwxc1_op) {
				error = uload_word((caddr_t)addr, &word);
				pcbp->pcb_fpregs[inst.r_format.re] = word;
			} else {
				error = ustore_word((caddr_t)addr,
					pcbp->pcb_fpregs[inst.r_format.rd]);
			}
		} else if ((inst.r_format.func == ldxc1_op) ||
			   (inst.r_format.func == sdxc1_op)) {
			if ((addr+7) >= K0BASE)
				return(0);
			if (inst.r_format.func == ldxc1_op) {
				error = uload_double((caddr_t)addr, &word);
				pcbp->pcb_fpregs[inst.r_format.re] = word;
			} else {
				error = ustore_double((caddr_t)addr,
					pcbp->pcb_fpregs[inst.r_format.rd]);
			}
		} else
			return(0);

		if (error)
			return(0);
		ep->ef_epc = new_epc;
		return(1);
	}	/* cop1x_op */
#endif /* MIPS4_ISA */

	addr = (__psint_t)REGVAL(inst.i_format.rs) + inst.i_format.simmediate;

	/*
	 * The addresses of both the left and right parts of the reference
	 * have to be checked.  If either is a kernel address it is an
	 * illegal reference.
	 */
	if (addr >= K0BASE || addr+3 >= K0BASE)
		return(0);

	error = 0;

	switch (inst.i_format.opcode) {
	case ld_op:
	  	if ((cause & CAUSE_EXCMASK) == EXC_II)
			return(0);	/* Don't emulate for mips1 process */
		error = uload_double((caddr_t)addr, &word);
		if(inst.i_format.rt == 0)
			break;
		efr[inst.r_format.rt + EF_AT-1] = word;
		break;
	case lwu_op:
	  	if ((cause & CAUSE_EXCMASK) == EXC_II)
			return(0);	/* Don't emulate for mips1 process */
		error = uload_uword((caddr_t)addr, &word);
		if(inst.i_format.rt == 0)
			break;
		efr[inst.r_format.rt + EF_AT-1] = word;
		break;
	case lw_op:
		error = uload_word((caddr_t)addr, &word);
		if (inst.i_format.rt == 0)
			break;
		efr[inst.r_format.rt + EF_AT-1] = word;
		break;
	case lh_op:
		error = uload_half((caddr_t)addr, &word);
		if (inst.i_format.rt == 0)
			break;
		efr[inst.r_format.rt + EF_AT-1] = word;
		break;
	case lhu_op:
		error = uload_uhalf((caddr_t)addr, &word);
		if (inst.i_format.rt == 0)
			break;
		efr[inst.r_format.rt + EF_AT-1] = word;
		break;
	case lwc1_op:
		checkfp(curuthread, 0);
		error = uload_word((caddr_t)addr, &word);
		pcbp->pcb_fpregs[inst.i_format.rt] = word;
		break;
	case ldc1_op:
		checkfp(curuthread, 0);
		if (ABI_HAS_64BIT_REGS(get_current_abi())) {
			error = uload_double((caddr_t)addr, &word);
			pcbp->pcb_fpregs[inst.i_format.rt] = word;
			break;
		}
		if (inst.i_format.rt & 1) {
			error++;
			break;
		}
		error = uload_word((caddr_t)addr, &word);
		error += uload_word((caddr_t)(addr+4), &word2);
		pcbp->pcb_fpregs[inst.i_format.rt+1] = word;
		pcbp->pcb_fpregs[inst.i_format.rt] = word2;
		break;

	case sd_op:
	  	if ((cause & CAUSE_EXCMASK) == EXC_II)
			return(0);	/* Don't emulate for mips1 process */
		error = ustore_double((caddr_t)addr, REGVAL(inst.i_format.rt));
		break;
	case sw_op:
		error = ustore_word((caddr_t)addr, REGVAL(inst.i_format.rt));
		break;
	case sh_op:
		error = ustore_half((caddr_t)addr, REGVAL(inst.i_format.rt));
		break;
	case swc1_op:
		checkfp(curuthread, 0);
		error = ustore_word((caddr_t)addr,
					pcbp->pcb_fpregs[inst.i_format.rt]);
		break;
	case sdc1_op:
		checkfp(curuthread, 0);
		if (ABI_HAS_64BIT_REGS(get_current_abi())) {
			error = ustore_double((caddr_t)addr,
					pcbp->pcb_fpregs[inst.i_format.rt]);
			break;
		}
		if (inst.i_format.rt & 1) {
			error++;
			break;
		}
		error = ustore_word((caddr_t)addr,
					pcbp->pcb_fpregs[inst.i_format.rt+1]);
		error += ustore_word((caddr_t)(addr+4),
					pcbp->pcb_fpregs[inst.i_format.rt]);
		break;

	default:
		return(0);
	}

	if (error)
		return(0);
	ep->ef_epc = new_epc;
	return(1);
}

/*
 * sizememaccess - return size in bytes of a memory access
 */
int
sizememaccess(eframe_t *ep, int *issc)
{
	union mips_instruction inst;
	int size;
	caddr_t vaddr;

	*issc = 0;
	/*
	 * check if it's an instruction. If its a branch at end of page
	 * it could be the instruction after the epc
	 */
	if ((ep->ef_badvaddr == ep->ef_epc) ||
	    ((ep->ef_cause & CAUSE_BD) &&
	     (ep->ef_badvaddr == (k_machreg_t)((__psunsigned_t)ep->ef_epc + 4))))
		return(4);

#if TFP
	/*
	 * On tfp, badvaddr points to the beginning of the icache line
	 * if we fault on the pc.
	 */
	if ((ep->ef_badvaddr & ~ICACHE_LINEMASK) ==
	    (ep->ef_epc & ~ICACHE_LINEMASK))
		return(4);
#endif

	/* XXX will EPC always be in another region
	 * from faulted one (which we may have locked?)
	 */
	if (ep->ef_cause & CAUSE_BD) {
		/* XXXX CC bug */
		vaddr = (caddr_t)((__psunsigned_t)ep->ef_epc + 4);
	} else {
		vaddr = (caddr_t)(ep->ef_epc);
	}
	if (IS_KUSEG(vaddr)) {
		inst.word = fuiword(vaddr);
	} else {
		inst.word = *(uint *)vaddr;
	}
	if (inst.word == 0xffffffff) {
		cmn_err(CE_PANIC, "sizememaccess bad read? \n");
		return(0);
	}

	switch (inst.i_format.opcode) {
	case scd_op:
		*issc = 1;
	case ldc1_op:
	case ldc2_op:
	case ld_op:
	case lld_op:
	case sdc1_op:
	case sdc2_op:
	case sd_op:
	case ldr_op:
	case sdr_op:
	case ldl_op:
	case sdl_op:
	case cache_op:		/* lie about cacheop size */
		size = 8;
		break;

	case sc_op:
		*issc = 1;
	case lw_op:
	case lwc1_op:
	case lwc2_op:
	case pref_op:
	case sw_op:
	case swc1_op:
	case swc2_op:
	case ll_op:
	case lwr_op:
	case swr_op:
	case lwl_op:
	case swl_op:
	case lwu_op:
		size = 4;
		break;

	case lh_op:
	case lhu_op:
	case sh_op:
		size = 2;
		break;

	case lb_op:
	case lbu_op:
	case sb_op:
		size = 1;
		break;

	case cop1x_op:
		if ((inst.r_format.func == lwxc1_op) ||
		    (inst.r_format.func == swxc1_op))
			size = 4;
		else if ((inst.r_format.func == ldxc1_op) ||
			 (inst.r_format.func == sdxc1_op))
			size = 8;
		else
			size = 0;
		break;

	default:
#if defined(BADVA_WAR) || defined(MP_R4000_BADVA_WAR) || (defined(R4000) && _MIPS_SIM == _ABI64)
		/*
		 * shouldn't really happen - but with BADVA WARS it can.
		 */
		if (badva_isbogus(ep))
			return -1;
#endif
		size = 0;
		break;
	}

	return(size);
}

#if defined(BADVA_WAR) || defined(MP_R4000_BADVA_WAR) || (defined(R4000) && _MIPS_SIM == _ABI64)
/* badva_isbogus
 * This routine simply examines the eframe and determines if the value in the
 * BADVA cop0 register makes sense for the instruction indicated by the EPC.
 * Note that the caller must insure that the exception taken should have
 * set a meaningful value into the BADVA (we're currently interested in either
 * RMISS exceptions or ADE exceptions).
 * There are two known bugs in the R4000 based HW requiring use of this routine:
 * (1) the MP bad BADVA bug - on MP systems, when an MP intervention occurs
 *     while a TLBMISS is take, the BADVA may end up with a completely random
 *     value
 * (2) a 32-bit user process may end up with a particular (partially bogus)
 *     BADVA value when referencing addresses in the range 0x7fff8000-0x7fffffff.
 *     In particular, the code sequence:
 *		lui	Rx,0x8000
 *		lw	Ry,0xfffc(Rx)
 *     will end up with the value 0x3fffff007ffffffc in the BADVA instead of the
 *     expected 0x000000007ffffffc.  Note that all "valid" bits are OK, only the
 *     filler bits are bogus.  This is a consistent value.
 *     This may occur on either a TLBMISS, or also occurs on a VCED on this address.
 *     In the VCED case, the VCED handler will itself get a RADE exception when
 *     it tries to flush this address from the cache.
 */
int
badva_isbogus(eframe_t *ep)
{
	inst_t inst;
	k_machreg_t vaddr;

	vaddr = ep->ef_epc;

	/* Did they go out of their text? */
	/*
	 * The epc could either be a user pc or a kernel KSEG2 
	 * address (loadable driver). Better check, because fuiword
	 * fails for kernel addresses.
	 */
	inst = (IS_KUSEG(vaddr)) ? fuiword((caddr_t)vaddr) : fkiword((caddr_t)vaddr);
	if (inst == (inst_t) -1)
		return 0;

	/* The instruction causing the segv can be in a branch-delay
	 * slot, but we will not see the BD bit, since a tlbmiss
	 * inside the utlbmiss handler causes the loss of the BD bit.
	 * So we have to look at the instruction. On a tlb invalid
	 * exception, the BD bit will be preserved.
	 * Note that segv's caused by bad branches are caught by the
	 * fuiword above.
	 */
	if ((ep->ef_cause & CAUSE_BD) || is_branch(inst)) {
		vaddr += sizeof(union mips_instruction);
		inst = (IS_KUSEG(vaddr)) ? fuiword((caddr_t)vaddr) 
					 : fkiword((caddr_t)vaddr);
		if (inst == (inst_t) -1)
			return 0;
	}

	/* We mask both BADVADDR and the load/store address with
	 * TLBHI_VPNMASK, which chops off the bottom 13 bits of the
	 * address. This is because, on a utlbmiss exception, the
	 * bottom 13 bits are lost from the address. On a tlb invalid
	 * exception, all the bits are retained, but there is no way
	 * at this level to differentiate between a utlb miss and a
	 * tlb invalid exception.
	 */
	if ((is_load(inst) || is_store(inst)) &&
	    (_ldst_addr(ep, inst) & TLBHI_VPNMASK) ==
	    (ep->ef_badvaddr & TLBHI_VPNMASK) &&
	    !bad_ldst_register(ep, inst) &&
	    !valid_store_addr(ep))
		return 0;

	/* Well, I can't see why they should get a SEGV, so cancel it */

	return 1;
}
#endif

/*ARGSUSED*/
int
bad_badva(register eframe_t *ep)
{
#if !defined(BADVA_WAR) && !defined(MP_R4000_BADVA_WAR)
	/* should never get called for non-R4000 */
	return 0;
#else
	static int bad_badvacount;
	k_machreg_t vaddr;
#ifdef R4600
	extern int is_r4600_flag;

	/* bug not present on the R4600 */
	if (is_r4600_flag)
		return(0);
#endif /* R4600 */

	/* The bug gives a tlb read miss */
	if ((ep->ef_cause & CAUSE_EXCMASK) != EXC_RMISS)
		return 0;

	if (!badva_isbogus(ep))
		return(0);
#if _MIPS_SIM == _ABI64 && defined(R4000)
	/* It appears that the R4000 chips consistently generate a bogus
	 * BADVADDR value for the following type of instruction sequence:
	 *	lui	rX,0x8000
	 *	lw	rY,0xfffc(rX)
	 * and we end up with BADVADDR == 0x3fffff007ffffffc instead
	 * of the expected 0x000000007ffffffc
	 * So we don't want to erroneously return that we've hit the
	 * bad BADVA chip bug (which is a truly random address) since the
	 * BADVA value doesn't make sense for the instruction sequence.
	 * So we check for this case and see if we still think there is
	 * a bad BADVA with the "corrected" address.
	 * Bug appears to only affect values in the range of 0x7ffff8000
	 * through 0x7fffffff.
	 */
	vaddr = ep->ef_badvaddr;
	if ((vaddr & 0xffffffffffff0000) == 0x3fffff007fff0000) {
		/* turn off bogus filler bits, and check again */
		ep->ef_badvaddr &= 0x000000ffffffffff;
		if (!badva_isbogus(ep)) {
			ep->ef_badvaddr = vaddr;
			return(0);
		}
	}
#endif /* _ABI64 */	  
	bad_badvacount++;
	return 1;
#endif
}

#if (defined R10000) && (defined T5TREX_IBE_WAR)
#if EVEREST || SN
#define	BADADDR_VAL	_badaddr_val
#else
#define	BADADDR_VAL	badaddr_val
#endif

int
ibe_isbogus(eframe_t *ep)
{
#if EVEREST || SN
	extern int BADADDR_VAL(volatile void *, int , volatile int *);
#endif
	__psunsigned_t  vaddr, chkaddr;
	volatile int    val;

	/*
	 * ef_badvaddr is set manually in VEC_ibe
	 */

	vaddr = ep->ef_badvaddr;

#if _MIPS_SIM == _ABI64
       	if (IS_XKPHYS(vaddr) || IS_COMPAT_PHYS(vaddr)) {
#else
       	if (IS_KSEGDM(vaddr)) {
#endif
		chkaddr = vaddr;
	}
	else {
		pde_t		pd_info;
		pfn_t		pfn;
		uint		poff;
		__psunsigned_t	page_size;

		tlb_vaddr_to_pde(tlbgetpid(), (caddr_t) vaddr, &pd_info);

		if (!pg_isvalid(&pd_info)) {
			if (!curvprocp)
				cmn_err(CE_PANIC, "Cannot determine paddr for "
					"vaddr 0x%lx with IBE\n", vaddr);

				vaddr_to_pde((caddr_t) vaddr, &pd_info);

			/*
			 * If the page was swapped out what can we do ?
			 */
			if (!pg_isvalid(&pd_info))
				return 1;
		}
		pfn = pg_getpfn(&pd_info);
		page_size = PAGE_MASK_INDEX_TO_SIZE(pg_get_page_mask_index(
			&pd_info));
		poff = vaddr & (page_size - 1);
		chkaddr = ((__psunsigned_t) pfn << PNUMSHFT) | poff;
		chkaddr = PHYS_TO_K0(chkaddr);
	}

	ASSERT(chkaddr);

	return ! BADADDR_VAL((volatile void *) chkaddr, 4, &val);
}
#endif

/*
 * ktlbfix - fix up a tlb miss in K2SEG - used by debuggers to allow
 * printing of dynamic space
 * Returns: 0 if couldn't fix it
 *	    1 if Okey-Dokey
 */
int
ktlbfix(caddr_t vaddr)
{
#if R4000 || R10000
	vaddr = (caddr_t)((__psint_t)vaddr & ~(NBPP*2-1));
	if (iskvir(vaddr)) {
		register pde_t *pdptr1 = kvtokptbl(vaddr);
		register pde_t *pdptr2 = kvtokptbl(vaddr+NBPP);
		if (pdptr1->pte.pg_vr || pdptr2->pte.pg_vr) {
			tlbdrop2in(0, vaddr, pdptr1->pte, pdptr2->pte);
			return(1);
		} 
	}
#else /* !R4000 */
	vaddr = vfntova(vatovfn(vaddr));
	if (iskvir(vaddr)) {
		register pde_t *pdptr = kvtokptbl(vaddr);
		if (pdptr->pte.pg_vr) {
			tlbdropin(0, vaddr, pdptr->pte);
			return(1);
		} 
	}
#endif
	return(0);
}

/*
 * Fast user level profiling support: If we were in user mode
 * and the user wants fast profiling, save his pc in the u-area.
 * The resched code will call addupc. This routine is called from
 * the fast tick interrupt handler.
 *
 * Note that we can keep n+1 ticks here since it appears its very possible
 * that in between the time that clock() has set OWEUPC and trap(), we
 * can get 1 additional fast tick..
 * Should we notify someone when there is an overflow?
 */
void
fast_user_prof(eframe_t *ep)
{
	proc_t	*curp = curprocp;

#if ULI
	/* If there is currently a ULI running, we aren't interrupting
	 * a real user process even though USERMODE is true
	 */
	if (private.p_curuli)
	    return;
#endif

	if (USERMODE(ep->ef_sr) && (curp->p_flag & SPROFFAST)) {
		if (curp->p_fastprofcnt <= PROF_FAST_CNT)
			curp->p_fastprof[curp->p_fastprofcnt++] =
							(inst_t *)ep->ef_epc;
#ifdef DEBUG
		else {
			/* keep track of how much overflow */
			curp->p_fastprofcnt++;
		}
#endif
	}
}

/*
 * profileregion - this is used by profiling since sprofil(2) has been
 * implemented. This gives us the capability of profiling shared libraries.
 * This selects the u_prof structure that has the largest offset which is
 * smaller than the program counter value being passed in. Then it calls
 * addupc() to increment the counter associated with the text region
 * specified by profp. If addupc() (which now returns a success/failure) cannot
 * increment because the pc value does not fall in range, then the overflow
 * region is incremented, if present.
 */
static void
profileregion(inst_t *pc, int ticks)
{
	struct prof *cprp, *prp;
	int use_32bit, i;
	uthread_t *ut = curuthread;
	proc_t *p = curprocp;
	int s;

	if (ut->ut_flags & UT_PTHREAD) {
		/*
		 * For processes pthread application we can have more than one
		 * uthread at a time trying to access/update the process
		 * profiling control information.  Thus we have to participate
		 * in the locking protocol for the process profiling control
		 * information.  We don't have this problem when we only have
		 * one uthread since the lock is a spinlock which prevents an
		 * interrupt of an update of the process profiling control
		 * information and since there's only one uthread, no other
		 * thread can attempt to update the information while we're
		 * accessing it here ...
		 */
		s = p_lock(p);
		if (!(p->p_flag & SPROF)) {
			/*
			 * Some other uthread must have torn down the
			 * profiling information before we could use it.
			 */
			p_unlock(p, s);
			return;
		}
	}

	ASSERT((p->p_profp != NULL) && (p->p_profn > 0));

	use_32bit = (p->p_flag & SPROF32);
	cprp = prp = p->p_profp;

	for (prp++, i=1; i < p->p_profn; i++, prp++)
		if ((prp->pr_off < (__psunsigned_t)pc) &&
		    (prp->pr_off > cprp->pr_off))
			cprp = prp;

	/*
	 * If addupc() fails because the pc is outside 
	 * the text region, try and update the overflow
	 * buffer if it is present. If present, the 
	 * overflow bin has to be the last one in the list.
	 */
	if ((cprp->pr_scale & ~1) && (addupc(pc, cprp, ticks, use_32bit))) {
		prp = (p->p_profp + (p->p_profn - 1));
		if ((p->p_profn > 1) && (prp != cprp) &&
		    (prp->pr_scale == 0x0002) && (prp->pr_off == 0))
			(void) addupc(pc, prp, ticks, use_32bit);
	}

	if (ut->ut_flags & UT_PTHREAD)
		p_unlock(p, s);
}


/*
 * called by softfp interrupt
 * to generate a needed siginfo for SIGFPE, SIGILL
 */
void
softfp_psignal(int sig, int fp_csr, eframe_t *ep)
{
	uthread_t *ut = curuthread;
        k_siginfo_t     info;
	sigvec_t *sigvp;
	int s;

	/* softfp_psignal is used to send a synchronous signal, so
	 * make sure the signal fits in that catagory.
	 */
	ASSERT(sig == SIGFPE || sig == SIGILL);

	/* Set pcb_resched to force entry into trap, so
	 * signal will be posted before returning to user mode.
	 */
	PCB(pcb_resched) = 1;

	VPROC_GET_SIGVEC(curvprocp, VSIGVEC_PEEK, sigvp);
        if (!sigismember(&sigvp->sv_sainfo, sig)) {
		sigtouthread(ut, sig, (k_siginfo_t *)NULL);
                return;
	}

        bzero(&info,sizeof(info));
        info.si_signo = sig;
        switch (sig) {
        case SIGILL:
                ut->ut_code = EXC_II;
                info.si_code = ILL_COPROC;
                info.si_addr = (caddr_t) ep->ef_epc;
                break;
        case SIGFPE:
                ut->ut_code = 0;
                if (fp_csr & (INVALID_EXC & CSR_EXCEPT))
                        info.si_code = FPE_FLTINV;
                else if (fp_csr & (DIVIDE0_EXC & CSR_EXCEPT)) {
                        ut->ut_code = BRK_DIVZERO;
                        info.si_code = FPE_FLTDIV;
                } else if (fp_csr & (OVERFLOW_EXC & CSR_EXCEPT)) {
                        ut->ut_code = BRK_OVERFLOW;
                        info.si_code = FPE_FLTOVF;
                } else if (fp_csr & (UNDERFLOW_EXC & CSR_EXCEPT))
                        info.si_code = FPE_FLTUND;
                else if (fp_csr & (INEXACT_EXC & CSR_EXCEPT))
                        info.si_code = FPE_FLTRES;
                info.si_addr = (caddr_t) ep->ef_epc;
                break;
        default:
                ASSERT(NULL);   /* only two cases for now */
                break;
        }
	s = ut_lock(ut);
        sigdelset(ut->ut_sighold, info.si_signo);
	ut_unlock(ut, s);

	/*
	 * Queue up the siginfo structure and deliver the signal
	 */
        sigtouthread(ut, sig, &info);
}
