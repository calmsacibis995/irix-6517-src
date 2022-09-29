
#include <sys/types.h>

/*
 * The __XXXX definitions work around the problem that arcs/include/setjmp
 * defines jmpbufptr that is referenced in genpda.h.  However, for the sim
 * code, "$ROOT/usr/include" preceeds  "arcs/include" on the search list so
 * the definition of jmpbufptr isn't seen
 */
#ifndef __XXXX
#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2) 
typedef int *           jmpbufptr;
#else
typedef __uint64_t *    jmpbufptr;
#endif
#endif


#include <genpda.h>
#include "sim.h"
#include <sys/sbd.h>

extern k_machreg_t excep_regs[];
extern void bcopy(void*, void*, int);
extern void _exception_handler(void);

#define BOGUS(x) regs[x] = 0xdeadface



/******************
 * forwardException	Passes exception to "firmware"
 *-----------------
 * It's not possible to simulate the low level exception handling because
 * the low level exception processing must use the K0 and K1 cpu registers
 * but in user mode, those registers are volatile because the IRIX kernel
 * modifies them.  Therefore we simulate the "result" of the low level
 * exception processing.  Namely, the low level code copies the registers
 * into the register save area and then passes control to _exception_handler.
 * The _exception_handler code is at a higher level so the K0/K1 problem
 * doesn't exist.  We create the register save area here and then just pass
 * control to _exception_handler.
 *
 * This is really pretty flimsy but it works well enough (mainly because
 * interrupts are always masked and because in the firmware nobody ever
 * returns from an exception).
 *
 * This code may be fragile as well because this code must access the
 * firmware code namespace and that's definitely fragile as the firmware
 * namespace and the host namespace overlap. 
 *
 * NOTE: This code is pretty client specific so it's separated out from
 *       even the reset of the client code.
 */
void forwardException(int sig, int code, SigContext_t* sc){
  int i;

  /*
   * pda contains pointer to register save area.  If it's zero, we will
   * use &excep_regs instead.
   */
  volatile struct generic_pda_s *gp =
    (volatile struct generic_pda_s*)&gen_pda_tab;
  k_machreg_t* regs = gp->regs;
  k_machreg_t sp = (k_machreg_t)gp->pda_fault_sp;
  if (sp==0)
    sp = (k_machreg_t)_fault_sp;
  if (regs==0)
    regs = excep_regs;


  /* If the BEV bit is set, just announce the error and halt. */
  if (simControl->cop0Regs[C0_SR] & SR_BEV){
    char buf[256];
    sprintf(buf,
	    "exception (%d,%d) with sr<BEV> set not supported at pc: %08llx\n",
	    sig,code,sc->sc_pc);
    simMessage(buf);
   /*
    * Hang so that we can stop the process and look at the call stack.
    */
    while (1);
  }


  /* If not on the fault stack, save all the registers */
  if (gp->stack_mode != MODE_FAULT){

    /*
     * Save various control registers.  Note that two copies are made,
     * one directly in the pda fields, one in the register save area.
     * In a real platform, the register save area is not updated if this
     * is a nested exception.  We don't handle nested exceptions.
     */
    gp->epc_save = regs[R_ERREPC] = sc->sc_pc;
    BOGUS(R_CACHERR);
    gp->sr_save = regs[R_SR] = sc->sc_status;
    gp->exc_save = regs[R_EXCTYPE] = sig==SIGBUS ? EXCEPT_NORM : EXCEPT_UTLB;
    gp->badvaddr_save = regs[R_BADVADDR] = sc->sc_badvaddr;
    gp->cause_save = regs[R_CAUSE] = sc->sc_cause;

    /*
     * GPR's
     *
     * Note that we copy registers one at a time from sc_regs to regs because
     * varcs_reg_t (regs[]) might be different than k_machreg_t (sc_regs[]) and
     * 32/64 conversions might be required.
     */
    for (i=0;i<32;i++)
      regs[i] = sc->sc_regs[i];
    regs[R_K0]=0xbad00bad;
    regs[R_K1]=0xbad11bad;

    /* MDxx */
    regs[R_MDLO] = sc->sc_mdlo;
    regs[R_MDHI] = sc->sc_mdhi;

    /* Bogus processor registers */
    BOGUS(R_INX);
    BOGUS(R_RAND);
    BOGUS(R_CTXT);
    BOGUS(R_TLBLO);
    BOGUS(R_TLBHI);
    BOGUS(R_TLBLO1);
    BOGUS(R_PGMSK);
    BOGUS(R_WIRED);
    BOGUS(R_COUNT);
    BOGUS(R_COMPARE);
    BOGUS(R_LLADDR);
    BOGUS(R_WATCHLO);
    BOGUS(R_WATCHHI);
    BOGUS(R_EXTCTXT);
    BOGUS(R_ECC);
    BOGUS(R_CACHERR);
    BOGUS(R_TAGLO);
    BOGUS(R_TAGHI);
    BOGUS(R_ERREPC);
    BOGUS(R_CONFIG);

    /* FPU registers */
    for(i=0;i<32;i++)
      regs[R_F0+i] = sc->sc_fpregs[i];
    BOGUS(R_C1_EIR);
    BOGUS(R_C1_SR);
  }

  /*
   * Set the exception type
   */
  gp->exc_save = sig==SIGBUS ? EXCEPT_NORM : EXCEPT_UTLB;

  /*
   * Return to code that passes control to the exception decode code.
   * Switch to the fault stack.  (Note that we use our own as the 
   */
  sc->sc_pc = (__uint64_t)(&_exception_handler);
  sc->sc_regs[29] = sp;
}

