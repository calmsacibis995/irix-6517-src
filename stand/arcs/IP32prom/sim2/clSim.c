#include "sim.h"
#include <sys/sbd.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <bstring.h>

/*
 * Client simulation support code
 *
 * NOTE: This code can be fragile because of namespace overlaps.  In
 *       particular, the shared memory code (usxxxxx) is library code that
 *       calls other library routines.  The client define a similarly named
 *       routine that implements a different function and cause the shared
 *       memory code to fail.  It's a good idea when problems arise to trace
 *       through all of the client simulation code and verify that some 
 *       name overlay isn't causing a problem.
 */



#define UNIMPLEMENTED 0
#define IMPOSSIBLE    0

extern long long __ll_lshift (long long, long long);
extern long long __ll_rshift (long long, long long);
extern long long __ull_rshift (long long, long long);

static void    advancePC(SigContext_t*);
static void    busHandler(int, int, SigContext_t*);\
static void    dshift(Inst_t, SigContext_t*);
extern void    forwardException(int sig, int code, SigContext_t*);
static Saddr_t getEffectiveAddress(Inst_t, SigContext_t*);
static Inst_t  getInst(SigContext_t*);
static void    illHandler(int, int, SigContext_t*);
static void    finish(void);
static void    send(void);
static void    recv(void);

#define ADDRESS(x) x
#define PC63_28(p) (p & ADDRESS(0xf0000000))

SimControl_t* simControl;
usptr_t* arena;

static pid_t serverPid;
static int   recvFd,sendFd;


/***********
 * clSimInit	Initialize client simulation interface
 */
int
clSimInit(int argc, char** argv){
  int           i;
  int           simMode = simFW;
  char*         arenaFile = SIM_ARENA_FILE;
  unsigned long arenaAddress = SIM_ARENA_ADDR;
 
  /*
   * Scan any arguments
   *
   * Only a few are handled here.  More could be handled by the server.
   */
  for (i=1;i<argc;i++)
    if (argv[i][0]=='-'){
      char c = argv[i][1];

      switch(c){
      case 'S':
	simMode = simSloader;
	break;

      case 'a':
	arena = argv[++i];
	break;

      case 'A':
	arenaAddress = strtoul(argv[++i],0,0);
	break;
      }
    }
  
  /*
   * Configure and initialize the communications area
   */
  usconfig(CONF_ATTACHADDR,arenaAddress);
  usconfig(CONF_INITSIZE, 1024*1024*10);
  arena = usinit(SIM_ARENA_FILE);
  sim_assert(arena);

  /*
   * Allocate and initialize the SimControl structure.
   */
  simControl = usmalloc(sizeof(SimControl_t),arena);
  sim_assert(simControl);
  usputinfo(arena,simControl);
  memset(simControl,0,sizeof(SimControl_t));

  simControl->recvSema = usnewpollsema(arena,0);
  simControl->sendSema = usnewpollsema(arena,0);

  simControl->clientPid = getpid();

  /*
   * Launch the server
   * Wait for it.
   */
  serverPid = fork();
  if (serverPid==0){
    execl(SIM_SERVER,SIM_SERVER,SIM_ARENA_FILE,0);

    /*
     * If the server doesn't launch, we get here.
     * Post the semaphore our parent is waiting on, then die.
     */
    usvsema(simControl->recvSema);
    exit(0);
  }
  recvFd = usopenpollsema(simControl->recvSema,0777);
  sendFd = usopenpollsema(simControl->sendSema,0777);
  recv();

  /*
   * Set up exit handler.
   * If the server didn't launch, exit
   */
  atexit(finish);
  if (simControl->refs==0)
    exit(0);


  /* Hook the signals */
  sigset(SIGBUS,busHandler);
  sigset(SIGILL,illHandler);

  return simMode;
}

/********
 * finish	Called when process exits
 */
static void finish(void){
  usdetach(arena);
  remove(SIM_ARENA_FILE);
}

/******************
 * loadStoreHandler
 *=================
 * Implements load/store common code for busHandler and illHandler.
 * Returns 1 if successful, returns 0 if exception has been forwarded
 * to the client exception handler.
 */
static int
loadStoreHandler(int sig, int code, SigContext_t* sc){
  Inst_t    	  inst = getInst(sc);
  Saddr_t 	  eaddr= getEffectiveAddress(inst,sc);
  int             idx;
  SimAddrMatch_t* am;

  am = &simControl->addrTbl[idx = simMatchAddr(eaddr,simIsStore(inst))];
  
  switch(am->matchCode){
  case SIM_nomatch:
    forwardException(sig,code,sc);
    return 0;
  case SIM_fwd_loadStore:
    simForward(inst,TO_PHYSICAL(eaddr));
  case SIM_loadStore:
    simLoadStore(inst,sc, TO_PHYSICAL(eaddr),idx);
    break;
  case SIM_loadStore_fwd:
    simLoadStore(inst,sc, TO_PHYSICAL(eaddr),idx);
    simForward(inst,TO_PHYSICAL(eaddr));
    break;

  default:
    sim_assert(IMPOSSIBLE);
  }
  return 1;
}


/************
 * busHandler	Service bus error signal
 */
static void
busHandler(int sig, int code, SigContext_t* sc){
  if (loadStoreHandler(sig,code,sc))
    advancePC(sc);
}


/************
 * illHandler	Service SIGILL signals
 */
static void
illHandler(int sig, int code, SigContext_t* sc){
  Inst_t 	  inst = getInst(sc);
  int    	  gprNum;
  int    	  cprNum;
  SimCop0Match_t* cpm;
  Saddr_t         eaddr;

  /* Decode the opcode and process */
  switch(inst.r_format.opcode){

    /* Coprocessor 0 operations */
  case cop0_op:
    cprNum = inst.r_format.rd;
    gprNum = inst.r_format.rt;
    cpm = &simControl->cop0Tbl[simMatchCop0(cprNum,simIsStore(inst))];
    switch(cpm->matchCode){
    case SIM_fwd_Cop0:
      simForward(inst,cprNum);
    case SIM_cop0:
      if (simIsStore(inst))
	simControl->cop0Regs[cprNum] = sc->sc_regs[gprNum];
      else
	sc->sc_regs[gprNum] = simControl->cop0Regs[cprNum];
      break;
    default:
      forwardException(sig,code,sc);
      return;
    }
    break;

    /* Cache ops */
  case cache_op:
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
    if (!loadStoreHandler(sig,code,sc))
      return;
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
      break;
    default:
      sim_assert(UNIMPLEMENTED);
    }

    break;

    /* Some double word arithmetic */
  case daddiu_op:
    sc->sc_regs[inst.i_format.rt] =
      sc->sc_regs[inst.i_format.rs] + inst.i_format.simmediate;
    break;
  }
  advancePC(sc);
}



/*********
 * getInst	Fetch instruction from client address space
 */
static Inst_t
getInst(SigContext_t* sc){
  Inst_t* pc = (Inst_t*)sc->sc_pc;
  if (sc->sc_cause & CAUSE_BD)
    pc++;
  return *pc;
}


/******************
 * getEffectiveAddr	Determine effective address of an instruction
 */
static Saddr_t
getEffectiveAddress(Inst_t inst,SigContext_t* sc){
  return inst.i_format.simmediate + sc->sc_regs[inst.i_format.rs];
}


/***********
 * advancePC
 */
static void
advancePC(SigContext_t* sc){
  Saddr_t temp;
  Saddr_t pc, rs,rd,rt;
  Inst_t  inst;

  /*
   * If not in the branch delay slot, the next address is simply the pc + 4.
   * Otherwise we have to look at the branch.
   */
  if ((sc->sc_cause & CAUSE_BD)==0)
    pc = sc->sc_pc + 4;
  else{

    inst = *(Inst_t*)sc->sc_pc;
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

    case bnel_op:
    case bne_op:
      if (rs!=rt)
	pc = sc->sc_pc + (inst.i_format.simmediate<<2) + 4;
      break;

    case blez_op:
      if (rs<=0)
	pc = sc->sc_pc + (inst.i_format.simmediate<<2) + 4;
      break;

    case bgtz_op:
      if (rs>0)
	pc = sc->sc_pc + (inst.i_format.simmediate<<2) + 4;
      break;

    case bcond_op:
      switch(inst.i_format.rt){
      case bltz_op:
      case bgez_op:
      case bltzal_op:
      case bgezal_op:
	sim_assert(UNIMPLEMENTED);	/* Fail all of these for now */
      }
      break;

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
      switch(inst.r_format.func){
      case jalr_op:
	temp = sc->sc_regs[inst.r_format.rs];
	sc->sc_regs[inst.r_format.rd] = sc->sc_pc + 8;
      case jr_op:
        temp = sc->sc_regs[inst.i_format.rs];
	break;
      default:
	sim_assert(UNIMPLEMENTED);	/* Fail any other spec_op's */
      }
      pc = temp;
      break;
      
    default:
      sim_assert(UNIMPLEMENTED);	/* Fail everything else */
    }
  }

  sc->sc_pc = pc;
}

/********
 * dshift	Handle double shift instruction
 */
void
dshift( Inst_t inst, SigContext_t* sc){
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
}


/************
 * simMessage
 */
void
simMessage(char* msg){
  write(2,msg,strlen(msg));
}

/***********
 * simAssert
 */
void
simAssert(char* ex, char* f, int l){
  char buf[128];
  sprintf(buf,"  SIM Assertion failure: %s at line %d, in file %s\n",ex,l,f);
  simMessage(buf);
  _exit(1);
}


/************
 * simForward	Forward request to server
 */
void
simForward(Inst_t inst, Saddr_t paddr){
  simControl->inst = inst;
  simControl->paddr= paddr;
  send();
  recv();
}


/******
 * recv		Wait post from server
 */
static void
recv(void){
  int    i;
  fd_set r;
  FD_ZERO(&r);
  FD_SET(recvFd,&r);
  i = uspsema(simControl->recvSema);
  if (i==0)
    i = select(recvFd+1,&r,0,0,0);
  sim_assert(i==1);
}


/******
 * send		Post to server
 */
static void
send(void){
  usvsema(simControl->sendSema);
}
