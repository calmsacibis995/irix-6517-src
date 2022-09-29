#ifndef _sim_h_
#define _sim_h_

#include <sys/types.h>
#include <signal.h>
#include <sys/inst.h>
#include <ulocks.h>

typedef struct sigcontext SigContext_t;
typedef union mips_instruction Inst_t;
typedef __uint64_t Saddr_t;


/* Define simulated serial number */
#define SERIAL0 0x08;
#define SERIAL1 0x00;
#define SERIAL2 0xaa;
#define SERIAL3 0xbb;
#define SERIAL4 0xcc;
#define SERIAL5 0xdd;

/* Simulator modes */
enum { simExit, simSloader, simFW };

/*
 * SIM interface
 *
 *       client				 server
 *       process			 process	
 *	+-------+     communication	+-------+ 
 *	|       |	  area		|	|
 *	|       |	+-------+	|	|
 *	|       |	|shared	|	|	|
 *	|       | <---->| memory|<---->	|	|
 *	|       |	|	|	|	|
 *	|       |	|	|	|	|
 *	|       |	+-------+	|	|
 *	|       |			|	|
 *	|       |			|	|
 *	|       |			|	|
 *	+-------+			+-------+
 *
 *
 * When the client code attempts to reference a simulated I/O space register
 * it traps.  Using the SIM interface, the trap code simulates the reference
 * and execution continues.  A coprocessor operation generates an illegal
 * instruction trap and is similarly simulated.  Also, some illegal
 * instructions (mips3) are simulated.
 *
 * Some simulations are simple and are performed directly in the client
 * code.  Others are more complex and are forwarded to the server process.
 * The shared memory communication area is used to pass information between
 * the client and the server process.
 */



#define SIM_ARENA_ADDR 0x1000000
#define SIM_ARENA_FILE "SIMARENA"
#define SIM_SERVER     "simServer"
typedef struct __sam_ SimAddrMap_t;


/*
 * Note that we are not using the sys/sbd.h memory macros here
 */
#define TO_PHYSICAL(x) ((x)&0x1FFFFFFF)


/*
 * Address match structure
 *
 * When an address faults, it is looked up using simMatchAddr().  If found,
 * an index into the address match table is returned.  The address match
 * table itself is located via the SimControl_t structure.
 *
 * Depending on the match code, the client code either directly handles the
 * trap or forwards it to the server process.
 *
 * Note that mask<ADDR_MATCH_STORE> and target<ADDR_MATCH_STORE> are
 * "store" bits.  That is, if the reference is a store to memory, this bit
 * is set in target and masked by the corresponding bit in mask.
 *
 * A target "matches" an entry in the match table if:
 *
 *      s = isStore(inst) ? 1 : 0;
 *	t = TO_PHYSICAL(addr) | ADDR_MATCH_STORE;
 *      match = (t & mask) == target;
 *
 * Note that because of the "store" bit, it is possible for the match
 * table to contain two separate entries for an address, one for reads
 * and one for writes.
 */
typedef struct{
  __uint64_t    mask;		/* Mask used in matching */
  __uint64_t    target;		/* Address match target */
  int           matchCode;	/* Match code - interpreted by client */
  int           targetCode;	/* Target code - interpreted by server */
  SimAddrMap_t*  mapping;	/* Used to map references */
} SimAddrMatch_t;

#define ADDR_MATCH_STORE ((long long)1<<40)

int simMatchAddr(Saddr_t addr, int load);

/*
 * Address mapping structure
 *
 * This structure is used by client and server code alike to map addresses
 * from simulated I/O space to representation of I/O registers.
 *
 * Given a physical address in simulated space, it is remapped to the
 * effective address of a representation by the following:
 *
 *      eaddr = base + ((physAddr & mask)>>shift)*stride;
 *
 * NOTE: The "ignoreIfZero" is a hack.  For now it makes it easy to silently
 *       discard transactions under the control of some "disable".  For
 *       example, write control logic on the flash.  If ignoreIfZero is
 *       itself zero, the transaction is always executed.
 */
struct __sam_{
  char*		base;		/* Points to base of concrete "registers" */
  int           stride;		/* Stride of "register" vector */
  __uint64_t    mask;
  int           shift;
  long*         ignoreIfZero;
};



/*
 * Cop0 match structure
 *
 * Similar to the Address match structure except entries correspond to
 * coprocessor registers, not memory addresses.
 *
 * Note that mask<COP0_MATCH_STORE> and target<COP0_MATCH_STORE> are
 * "store" bits.  That is, if the reference is a store to a coprocessor
 * registers, this bit is set in the target and masked by the corresponding
 * bit in mask.
 *
 * A target register "matches" an entry in the match table if:
 *
 *	s = isStore(inst) ? 1 : 0;
 *      t = reg | COP0_MATCH_STORE;
 *      match = (t & mask) == target;
 *
 * Note that because of the "store" bit, it is possible for the match
 * table to contain two separate entries for an address, one for reads
 * and one for writes.
 */
typedef struct{
  int           mask;		/* Mask used in matching */
  int           target;		/* Register match target */
  int           matchCode;	/* Match code -- interpreted by client */
  int           targetCode;	/* Target code -- Interpreted by server */
} SimCop0Match_t;

#define COP0_MATCH_STORE (1<<6)

int simMatchCop0(int reg, int load);


/*
 * Matchcode definitions
 *
 * The match codes returned in the SimAddrMatch_t and SimCop0Match_t 
 * structures specify what actions the clien code is to perform.
 */
enum{
  SIM_nomatch,			/* No match */
  SIM_abort,			/* Abort simulation */
  SIM_loadStore,		/* simLoadStore()   [r/w]*/
  SIM_fwd_loadStore,		/* simForward();simLoadStore()  [r] */
  SIM_loadStore_fwd,		/* simLoadStore();simForward()  [w] */
  SIM_cop0,			/* simCop0() */
  SIM_fwd_Cop0			/* simForwardCop0() */
};


/*
 * simulation communication area
 *
 * The communication area is a shared memory area.  The client code
 * attaches to this area with clSimInit().  The server attaches to
 * this area with svrSimInit().
 *
 * This area contains a control structure (accessed via usgetinfo())
 * that points to other structures in the shared memory area.
 *
 * NOTE: For now, we assume that the communication area gets mapped at the
 *       same virtual address in both the client and the server.  This doesn't
 *       really have to be the case but it's easier to do so.
 */
typedef struct{
  SigContext_t*		clSigCtx; 	/* Copy of client signal context. */
  SimAddrMatch_t* 	addrTbl;	/* Address of addr match table */
  SimCop0Match_t*	cop0Tbl;        /* Address of cop0 match table */
  long long*            cop0Regs;	/* Address of cop0 registers */
  int                   matchIndex;	/* Last match index */
  int                   matchCode;	/* Last match code*/
  usema_t*              sendSema;       /* Client "sends" to server */
    Inst_t              inst;           /* Trapping client instruction */
    Saddr_t             paddr;		/* Trapping client physical addr */
  usema_t*              recvSema;	/* Client "receives" from server */
  int                   refs;		/* Number of references */
  pid_t                 clientPid; 	/* PID of client */
} SimControl_t;
extern SimControl_t* simControl;


/* Sim objects may access the shared memory arena using "arena" as handle */ 
extern usptr_t* arena;

int clSimInit(int,char**);
int svrSimInit(int,char**);


/* Define table sizes for communication area */
#define ADDR_TBL_SIZE 256
#define COP0_TBL_SIZE 64



/*
 * Service functions
 */
void simLoadStore(Inst_t, SigContext_t*, Saddr_t, int);
void simCop0(SimCop0Match_t*);
void simForward(Inst_t, Saddr_t);
void simForwardCop0();

void simMessage(char*);
int  simIsStore(Inst_t);

/*
 * sim_assert is for client code.
 */
#define sim_assert(e)  ((e)?((void)0):simAssert(#e, __FILE__, __LINE__))
void simAssert(char*, char*, int);

#endif 


