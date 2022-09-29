#include <fcntl.h>
#include <unistd.h>
#include <sys/sbd.h>
#include <stdlib.h>
#include <stdio.h>
#include <fault.h>
#include <genpda.h>
#include <IP32.h>
#include <flash.h>
#include "SIM.h"


extern void _exit(int);

typedef long long longlong_t;
extern longlong_t __ll_lshift (longlong_t, longlong_t);
extern longlong_t __ll_rshift (longlong_t, longlong_t);
extern longlong_t __ull_rshift (longlong_t, longlong_t);

void forwardException(int sig, int code, Ctx_t* sc);


/***************************************************************************
 *                  Simulated Machine State Storage                        *
 ***************************************************************************/

/*
 * Coprocessor registers
 */
static  k_machreg_t cpr0s[32];


/***************************************************************************
 *                  K0 and K1 address matching                             *
 ***************************************************************************/

/*
 * ADDRESS sign extends x to 64 bits (when I need it and figure out how to
 * do it).
 */
#define ADDRESS(x) (x)


/* Address match structure */
typedef struct{	
  __uint64_t	mask;
  __uint64_t    addr;
  simFunc       func;
  int           arg;
} AddrMatch;

/* Address match database */
static AddrMatch matchList[]={
  {~0x7ff,ADDRESS(SERIAL_PORT0_BASE),SIM_mace_serial,0},
  {~0x7ff,ADDRESS(SERIAL_PORT1_BASE),SIM_mace_serial,1},
  {~MEMBLOCK_MASK,ADDRESS(0x80000000),SIM_k0_k1,0},
  {~MEMBLOCK_MASK,ADDRESS(0xa0000000),SIM_k0_k1,1},
  {~flashblock_MASK,ADDRESS(0xbfc00000),SIM_flash_ram,0},
  {ADDRESS(-1),ADDRESS(FLASH_WENABLE),SIM_flash_we,0},
  {~0xffff,ADDRESS(RTC_BASE),SIM_rtc,0},
  {-0x1ff,ADDRESS(MAC110_BASE),SIM_mac110},
  {0,0,0}
};



#define PC63_28(p) (p & ADDRESS(0xf0000000))

/******************
 * getEffectiveAddr
 */
Saddr_t getEffectiveAddress(Instr_t inst,Ctx_t* sc){
  return inst.i_format.simmediate + sc->sc_regs[inst.i_format.rs];
}


/************
 * addr_match	Check to see if the address matches a handled address.
 */
AddrMatch*
addr_match(Instr_t inst, __uint64_t eadr, Ctx_t *sc){
  AddrMatch* am = matchList;

  /* NOTE: Only look at bottom 32 bits for now */
  while(am->mask)
    if ((eadr & am->mask & 0xffffffff) == (am->addr & 0xffffffff))
      return am;
    else
      am++;
  return 0;
}


/*************
 * nextAddress	Figure out what the next address is.
 *------------
 * NOTE: I decide what cases I need to support by "discovery".  That is,
 * I execute the code and let the assertions catch cases I haven't implemented.
 * That means there may still be unimplemented cases that might be triggered
 * by code I haven't executed yet.
 */
static __uint64_t
nextAddress(Instr_t inst, Ctx_t *sc){
  __uint64_t temp;
  __uint64_t pc;
  __int64_t rs,rd,rt;

  /*
   * If not in the branch delay slot, the next address is simply the pc + 4.
   * Otherwise we have to look at the branch.
   */
  if ((sc->sc_cause & CAUSE_BD)==0)
    return sc->sc_pc + 4;

  /*
   * Fetch the Instr_t.
   */
  inst = *(Instr_t*)(sc->sc_pc);

  rs = sc->sc_regs[inst.i_format.rs];
  rt = sc->sc_regs[inst.i_format.rt];
  pc = sc->sc_pc + 8;

  /*
   * Decode and then update the pc appropriately
   */
  switch(inst.j_format.opcode){

  case beq_op:
    if (rs==rt)
      pc = sc->sc_pc + (inst.i_format.simmediate<<2) + 4;
    break;
  case bne_op:
    if (rs!=rt)
      pc = sc->sc_pc + (inst.i_format.simmediate<<2) + 4;
    break;
  case blez_op:
    if (rs<=0)
      pc = sc->sc_pc + (inst.i_format.simmediate<<2) + 4;
  case bgtz_op:
    if (rs>0)
      pc = sc->sc_pc + (inst.i_format.simmediate<<2) + 4;

  case bcond_op:
    switch(inst.i_format.rt){
    case bltz_op:
    case bgez_op:
    case bltzal_op:
    case bgezal_op:
      sim_assert(UNIMPLEMENTED);	/* Fail all of these for now */
    }

  case j_op:
    temp = inst.j_format.target<<2;
    pc = PC63_28(sc->sc_pc) | temp;
    break;

  case jal_op:
    temp = inst.j_format.target<<2;
    sc->sc_regs[31] = sc->sc_pc + 8;
    pc = PC63_28(sc->sc_pc) | temp;
    break;

  case spec_op:
    temp = sc->sc_regs[rs];

    switch(inst.r_format.func){
    case jalr_op:
      sc->sc_regs[rd] = sc->sc_pc + 8;
    case jr_op:
      break;
    default:
      sim_assert(UNIMPLEMENTED);	/* Fail any other spec_op's */
    }
    pc = temp;
    break;

  default:
    sim_assert(UNIMPLEMENTED);	/* Fail everything else */
  }
  return pc;
}


/***********
 * advancePC
 */
void
advancePC(Instr_t inst, Ctx_t* sc){
  sc->sc_pc = nextAddress(inst,sc);
}

/*********
 * getInst	Fetch faulting Instr_t
 */
static Instr_t
getInst(Ctx_t* sc){
  Instr_t* pc = (Instr_t*)sc->sc_pc;
  if (sc->sc_cause & CAUSE_BD)
    pc++;
  return *pc;
}

/***************
 * loadstoreSize	Returns loadstore size in bytes.
 */
int
loadstoreSize(Instr_t inst){
  switch(inst.i_format.opcode){
  case lb_op:
    return 1;
  case lbu_op:
    return 1;
  case lh_op:
    return 2;
  case lhu_op:
    return 2;
  case lw_op:
    return 4;
  case lwl_op:
    return 4;
  case lwr_op:
    return 4;
  case ll_op:
    return 4;
  case ld_op:
    return 8;
  case ldl_op:
    return 8;
  case ldr_op:
    return 8;
  case lld_op:
    return 8;
  case sb_op:
    return 1;
  case sh_op:
    return 2;
  case sw_op:
    return 4;
  case swl_op:
    return 4;
  case swr_op:
    return 4;
  case sc_op:
    return 4;
  case sd_op:
    return 8;
  case sdl_op:
    return 8;
  case sdr_op:
    return 8;
  case scd_op:
    return 8;
  default:
    sim_assert(UNIMPLEMENTED);
  }
  sim_assert(IMPOSSIBLE);
}


/*********
 * isStore	Returns 1 if Instr_t is a store Instr_t.
 */
int
isStore(Instr_t inst){
  switch(inst.i_format.opcode){
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
    return 1;
  }
  return 0;
}    

/***********
 * loadstore	Processes a load or a store operation
 *----------
 * Used to handle remapped load/stores, the register source/destination
 * is determined via sc, and the source/destination is in addr.
 *
 * NOTE: I decide what cases I need to support by "discovery".  That is,
 * I execute the code and let the assertions catch cases I haven't implemented.
 * That means there may still be unimplemented cases that might be triggered
 * by code I haven't executed yet.
 */

void
loadstore(Instr_t inst, Ctx_t *sc, void* addr){
  __uint64_t*       srcdst;

  srcdst = &sc->sc_regs[inst.i_format.rt];

  /* Dispatch and process according to the Instr_t type */
  switch(inst.i_format.opcode){
  case lb_op:
    *srcdst = *((char*)addr);
    break;
  case lbu_op:
    *srcdst = *((unsigned char*)addr);
    break;
  case lh_op:
    *srcdst = *((short*)addr);
    break;
  case lhu_op:
    *srcdst = *((unsigned short*)addr);
    break;
  case lw_op:
    *srcdst = *((long*)addr);
    break;
  case lwl_op:
    sim_assert(lwl_op==0);
    break;
  case lwr_op:
    sim_assert(lwr_op==0);
    break;
  case ll_op:
    sim_assert(ll_op==0);
    break;
  case ld_op:
   *srcdst = *((long long*)(addr));
    break;
  case ldl_op:
    sim_assert(ldl_op==0);
    break;
  case ldr_op:
    sim_assert(ldr_op==0);
    break;
  case lld_op:
    sim_assert(lld_op==0);
    break;
  case sb_op:
    *((unsigned char*)addr) = *srcdst;
    break;
  case sh_op:
    *((unsigned short*)addr) = *srcdst;
    break;
  case sw_op:
    *((unsigned long*)addr) = *srcdst;
    break;
  case swl_op:
    sim_assert(swl_op==0);
    break;
  case swr_op:
    sim_assert(swr_op==0);
    break;
  case sc_op:
    sim_assert(sc_op==0);
    break;
  case sd_op:
    *((long long*)addr) = *srcdst;
    break;
  case sdl_op:
    sim_assert(sdl_op==0);
    break;
  case sdr_op:
    sim_assert(sdr_op==0);
    break;
  case scd_op:
    sim_assert(scd_op==0);
  default:
    sim_assert(UNIMPLEMENTED);
  }

  /* Advance the PC to the next Instr_t */
  advancePC(inst,sc);
}

/*********
 * SIM_led
 *--------
 * The led went away for now so just skip this.  Leave the hook in for now.
 */
void
SIM_led(Instr_t inst, Saddr_t addr, Ctx_t* sc,int i){
  advancePC(inst,sc);
}

/***************************************************************************
 *                        	MIPS3 stuff                                *
 ***************************************************************************/

/********
 * dshift	Handle double shift Instr_ts
 */
void
dshift( Instr_t inst, Ctx_t* sc){
  __int64_t rt = sc->sc_regs[inst.r_format.rt];
  __int64_t sa = inst.r_format.re;

  switch(inst.r_format.func){
  case dsll32_op:
    sc->sc_regs[inst.r_format.rd] = __ll_lshift(rt,sa+32);
    break;
  case dsrl32_op:
    sc->sc_regs[inst.r_format.rd] = __ull_rshift(rt,sa+32);
    break;
  case dsra32_op:
    sc->sc_regs[inst.r_format.rd] = __ll_rshift(rt,sa+32);
    break;
  default:
    sim_assert(UNIMPLEMENTED);
  }
  advancePC(inst,sc);
}


/***************************************************************************
 *                   	Coprocessor registers                              *
 ***************************************************************************/

/******
 * cop0	Process cop0 registers.
 */
void 
cop0(int writeAccess, k_machreg_t* gpr, int cprNum){
  char buf[128];

  /*
   * Dispatch to the appropriate coprocessor register service rtn.
   */
  switch(cprNum){
  case C0_COUNT:
    cpr0s[cprNum] += 5000000;	/* .1 sec for 50MHZ processor*/

  default:
    /*
     * For registers with no side effect, just treat them as load/store.
     */
   if (writeAccess)
     cpr0s[cprNum] = *gpr;
   else
     *gpr = cpr0s[cprNum];
  }
}


/***************************************************************************
 *                   Exception service                                     *
 ***************************************************************************/

/*************
 * sbusHandler
 */
static void
sbusHandler(int sig, int code, Ctx_t *sc){
  int ec = sc->sc_cause & CAUSE_EXCMASK;
  int addr_idx;
  char*    msg;
  char     buf[128];
  AddrMatch* ap;
  Instr_t inst = getInst(sc);
  Saddr_t eadr = getEffectiveAddress(inst,sc);

  /* Must be a RADE or WADE exception */
  sim_assert(ec==EXC_RADE || ec == EXC_WADE);

  /*
   * Check to see if it matches a serviced address.  If
   * so service it.  Otherwise fall through and forward
   * the exception.
   */
  if (ap = addr_match(inst,eadr,sc)){
    (*ap->func)(inst,eadr,sc,ap->arg);
    return;
  }
  forwardException(sig,code,sc);
}


/**************
 * ssegvHandler
 */
static void
ssegvHandler(int sig, int code, Ctx_t *sc){
  
  /*
   * We doen't service any translated addresses so simply forward
   * the exception.
   */
  forwardException(sig,code,sc);
}


/*************
 * sillHandler	Service SIGILL signal
 */
static void
sillHandler(int sig, int code, Ctx_t *sc){
  Instr_t inst = getInst(sc);
  int gprNum;
  int cprNum;
  AddrMatch* ap;
  Saddr_t eadr;

  /* Decode the opcode and process appropriately */
  switch(inst.r_format.opcode){
  case cop0_op:
    cop0(inst.r_format.rs,&sc->sc_regs[inst.r_format.rt],inst.r_format.rd);
    advancePC(inst,sc);
    break;

    /* Cache ops */
  case cache_op:
    advancePC(inst,sc);
    break;

    /* double word load/store op's */
  case ld_op:
  case ldl_op:
  case ldr_op:
  case lld_op:
  case sd_op:
  case sdl_op:
  case sdr_op:
  case scd_op:
    eadr = getEffectiveAddress(inst,sc);
    if (ap = addr_match(inst,eadr,sc)){
      (*ap->func)(inst,eadr,sc,ap->arg);
      return;
    }
    forwardException(sig,code,sc);
    break;

    /* double word shifts */
  case spec_op:
    switch(inst.r_format.func){
    case dsll32_op:
    case dsrl32_op:
    case dsra32_op:
      dshift(inst,sc);
      break;
    case daddu_op:
      sc->sc_regs[inst.r_format.rd] =
	sc->sc_regs[inst.r_format.rs] + sc->sc_regs[inst.r_format.rt];
      advancePC(inst,sc);
      break;
    default:
      sim_assert(UNIMPLEMENTED);
    }

    break;

    /* Some double word arithmetic */
  case daddiu_op:
    sc->sc_regs[inst.i_format.rt] =
      sc->sc_regs[inst.i_format.rs] + inst.i_format.simmediate;
    advancePC(inst,sc);
    break;

  default:
    forwardException(sig,code,sc);
  }
}



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
 */
void forwardException(int sig, int code, Ctx_t* sc){
  extern k_machreg_t excep_regs[];
  extern void bcopy(void*, void*, int);
  extern void _exception_handler(void);

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
  if (cpr0s[C0_SR] & SR_BEV){
    char buf[256];
    sprintf(buf,
	    "exception (%d,%d) with sr<BEV> set not supported at pc: %0x\n",
	    sig,code,sc->sc_pc);
    SIM_message(buf);
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


/***************************************************************************
 *			Initialize simulation                              *
 ***************************************************************************/

extern void initSimFlash(void);
extern void flushSimFlash(void);
extern void initSimRTC(void);
extern void initSimMac110(void);

/*********
 * simInit	Establish simulation hooks.
 */
void
simInit(void){
  sigset(SIGBUS, sbusHandler);	/* Hook bus error */
  sigset(SIGSEGV,ssegvHandler);	/* Hook segv error */
  sigset(SIGILL,sillHandler);	/* Hook ill error */

  /* Set up aligned pointers to memory SIMblock and flash SIMblocks */
  SIMmemblock = (char*)((long)(SIMblock+4095) & ~4095);
  SIMflashblock = (char*)((long)(SIMflash+4095) & ~4095);
  initSimFlash();

  /* Initialize cpr's */
  cpr0s[C0_SR] = SR_BEV;

  /* Init RTC and MAC110 */
  initSimRTC();
  initSimMac110();

}


/*************
 * SIM_message
 */
void
SIM_message(const char* str){
  extern size_t strlen(const char*);
  write(1,str,strlen(str));
}

/************
 * SIM_assert	Special assert as normal asserts go through simulated IO.
 */
void 
SIM_assert(char* ex, char* f, int l){
  char buf[128];
  sprintf(buf," sim_assert failure: %s at line %d in file %s\n",ex, l, f);
  SIM_message(buf);
  _exit(1);
}




