#include <assert.h>
#include <bstring.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/uart16550.h>
#include "sim.h"
#include "simSvr.h"
#include <string.h>
#include <sys/cpu.h>
#include <sys/time.h>
#include <unistd.h>

#define UNIMPLEMENTED 0
#define IMPOSSIBLE    0

#define SHDR " SIM 16550- "

/*
 * Registers codes for n16c550
 *
 * Note that because some registers are at the same address, we keep
 * separate copies of the overlapping registers.  All loadStore 
 * transactions are performed against the first 8 register locations and
 * the server code must perform appropriate copies to make this work out.
 * All reads are SIM_fwd_loadStore and all writes are SIM_loadStore_fwd
 * so the server code can do data staging as appropriate.
 */
enum regCode {
  r_tbuf = 0,			/* transmitter buffer */
  r_rbuf = 0,			/* receiver buffer */
  r_dlbl = 0,			/* divisor latch lsb */
  r_dlbh = 1,			/* divisor latch msb */
  r_ier  = 1,			/* interrupt enable */
  r_iir  = 2,			/* interrupt identification */
  r_fifo = 2,			/* fifo control */
  r_lctl = 3,			/* line control */
  r_mcr  = 4,			/* modem control */
  r_lsr  = 5,			/* line status */
  r_msr  = 6,			/* modem status */
  r_scr  = 7,			/* scratch pad register */
  r_tbufx= 8,			/* copy of reg: r_tbuf */
  r_rbufx= 9,			/* copy of reg: r_rbuf */
  r_dlblx=10,			/* copy of reg: r_dlbl */
  r_dlbhx=11,			/* copy of reg: r_dlbh */
  r_ierx =12,			/* copy of reg: r_ier */
  r_iirx =13,			/* Copy of reg: r_iir */
  r_fifox=14,			/* Copy of reg: r_fifo */
  r_count=15
};


#define DLAB(x) (regs[r_lctl] & LCR_DLAB)

static enum regCode regMap(int regnum, int store, unsigned char*);
static simMaceSerialVerbose;
static transmitted;

/* Port state */
typedef struct{
  int   output;			/* Output file descriptor */
  int   inputs[16];		/* Input file descriptors */
  int   inputCnt;		/* Open input file count */
} Port_t;

enum {				/* Codes returned by readInput */
  read_none,
  read_char,
  read_break,
  read_framingError,
  read_parityError,
  read_delay
};

/* Simulated 16550 state */
typedef struct{
  unsigned char regs0[r_count];
  unsigned char regs1[r_count];
  SimAddrMap_t  map0;
  SimAddrMap_t  map1;
  Port_t        ports[2];
} Serial_t;

Serial_t* serial;


/***********
 * readInput	Check for input
 */
static int
readInput(int port, char* cp){
  Port_t* p = &serial->ports[port];
  int     fd = p->inputs[p->inputCnt];
  int     cnt;

  /*
   * Check for input from console (nested).  Only direct console input
   * (inputCnt==0) is tested with select.
   */
  if (p->inputCnt==0){
    static struct timeval timeout = {0,0};
    fd_set r;
    FD_ZERO(&r);
    FD_SET(fd,&r);
    if (select(fd+1,&r,0,0,&timeout)<=0)
      return read_none;
  }
  
  /* Read the input */
  cnt = read(fd,cp,1);

  /* Check for eof and close nested file */
  if (cnt==0){
    if (p->inputCnt>0){
      close(fd);
      p->inputCnt--;
    }
    return read_none;
  }

  /* Check for special handling */
  if (*cp=='~'){
    char c = 0;
    cnt = read(fd,&c,1);

    /* EOF */
    if (cnt<=0)
      return read_none;

    /* B =>break */
    if (c=='B')
      return read_break;

    /* F => Framing error */
    if (c=='F')
      return read_framingError;

    /* P => Parity error */
    if (c=='P')
      return read_parityError;

    /* D => delay input a while (for scripts) */
    if (c=='D')
      return read_delay;

    /* . => filename */
    if (c=='.'){
      char fn[128];
      int  fnIdx = -1;
      int  fdNew;

      /* Collect filename */
      
      do
	cnt = read(fd,&fn[++fnIdx],1);
      while(cnt==1 && !isspace(fn[fnIdx]));
      if (cnt!=1)
	return read_none;

      /* Open the file */
      fn[fnIdx] = 0;
      fdNew = open(fn,O_RDONLY);
      if (fd>=0){
	p->inputs[++p->inputCnt] = fdNew;
      }
      return read_none;
    }      

    /* ~ => ~ */
    if (c=='~')
      *cp = c;
    else
      return read_none;
  }

  return read_char;
}

/************
 * lsrService
 */
static void
lsrService(unsigned char* regs, int isStore, int port){
  unsigned char v = regs[r_lsr] & ~(LSR_PARERR|LSR_FRMERR|LSR_BRKDET);
  char c;
  static int delay = 0;


  assert(port==0);

  if (delay==0)
    /* Check for input */
    switch(readInput(port,&c)){
    case read_delay:
      delay += 100;
    case read_none:
      v &= ~LSR_RCA;
      break;
    case read_char:
      regs[r_rbufx] = c;
      v |= LSR_RCA;
      break;
    case read_break:
      v |= LSR_BRKDET;
      break;
    case read_framingError:
      v |= LSR_RCA|LSR_FRMERR;
      break;
    case read_parityError:
      v |= LSR_RCA|LSR_PARERR;
      break;
    }
  else
    delay--;
    
	 
  /* Count down the transmit timer */
  if (transmitted)
    transmitted--;
  else
    v |= LSR_XHRE;
  
  regs[r_lsr] = v;
}


/***************
 * simMaceSerial
 */
static void
simMaceSerial(void){
  SimAddrMatch_t* am      = &simControl->addrTbl[simControl->matchIndex];
  int             isStore = simControl->matchCode==SIM_loadStore_fwd;
  unsigned char*  regs    = am->mapping->base;
  enum regCode    r       = regMap((simControl->paddr>>8)&0x7,isStore,regs);
  int             port    = am->mapping->base==serial->regs1;
  Port_t*         p       = &serial->ports[port];
  char            c;

  switch(r){

  case r_tbufx:
    /* Transmit buffer register */
    if (isStore){
      if (transmitted)
	printf(SHDR "transmitter overrun\n");
      regs[r_tbufx] = regs[r_tbuf];
      if (regs[r_lsr]&LSR_XHRE){
	write(p->output,&regs[r_tbuf],1);
	regs[r_lctl] &= ~LSR_XHRE;
	transmitted++;
      }
    }
    else
      regs[r_tbuf] = regs[r_tbufx];
    break;

  case r_rbufx:
    /* Receive register (filled on in lsr service) */
    if (isStore)
      regs[r_rbufx] = regs[r_rbuf];
    else
      regs[r_rbuf] = regs[r_rbufx];
    break;

  case r_dlblx:
    /* Baud rate clock register - lsb */
    if (isStore){
      regs[r_dlblx] = regs[r_dlbl];
      if (simMaceSerialVerbose)
	printf(SHDR "DLBL[%d]:%02x\n",port, regs[r_dlblx]);
    }
    else
      regs[r_dlbl] = regs[r_dlblx];
    break;

  case r_dlbhx:
    /* Baud rate clock register - msb */
    if (isStore){
      regs[r_dlbhx] = regs[r_dlbh];
      if (simMaceSerialVerbose)
	printf(SHDR "DLBL[%d]:%02x\n",port, regs[r_dlblx]);
    }
    else
      regs[r_dlbh] = regs[r_dlbhx];
    break;

  case r_ierx:
    /* Interrupt enable register*/
    if (isStore)
      regs[r_ierx] = regs[r_ier];
    else
      regs[r_ier] = regs[r_ierx];
    break;

  case r_iirx:
    /* Interrupt identification register */
    if (isStore)
      regs[r_iirx] = regs[r_iir];
    else
      regs[r_iir] = regs[r_iirx];
    if (simMaceSerialVerbose)
      printf(SHDR " IER[%d]:%02x, intrpt's not simulated\n",port,regs[r_iirx]);
    break;

  case r_fifox:
    /* Fifo register */
    if (isStore)
      regs[r_fifox] = regs[r_fifo];
    else
      regs[r_fifo] = regs[r_fifox];
    break;

  case r_lctl:
    /* Line control register */
    c = regs[r_lctl];
    if (simMaceSerialVerbose)
      printf(SHDR
	     "LCTL[%d]:%02x, bits:%d, stop:%d, "
	     "PE:%d, EvnP:%d, Brk:%d, DLAB:%d\n",
	     port,c,
	     (c&3)+5,(c>>2)&1,(c>>3)&1,(c>>4)&1,(c>>6)&1,(c>>7)&1);
    break;

  case r_mcr:
    /* Modem control register */
    c = regs[r_mcr];

    /* We don't normally trace the MCR because the sloader does it a lot */
#ifdef VERBOSE_MCR
    if (simMaceSerialVerbose)
      printf(SHDR
	     " MCR[%d]:%02x, dtr:%d, rts:%d\n",
	     port,c,c&1,(c>>1)&1);
#endif
    break;

  case r_scr:
    /* Scratch pad register */

  case r_msr:
    /* Modem status register */
    break;

  case r_lsr:
    /* Line status register */
    lsrService(regs,isStore,port);
    break;

  default:
    assert(IMPOSSIBLE);
  }
}

/********
 * regmap	Map hardware register index to register code.
 *-------
 * The mapping is sensitive to whether the access is read or write, and
 * to whether the DLAB bit is set in the LCTL.  Note that we map to the
 * unique register codes (r_????x) where there would otherwise be an
 * ambiguous code (e.g. r_rbuf and r_tbuf).
 */
static enum regCode
regMap(int index, int store, unsigned char* regs){
  switch(index){
  case REG_MSR:
    return r_msr;
  case REG_LSR:
    return r_lsr;
  case REG_MCR:
    return r_mcr;
  case REG_LCR:
    return r_lctl;
  case REG_ISR:
    if (store)
      return r_fifox;
    else
      return r_iirx;
  case REG_ICR:
    if (DLAB(regs))
      return r_dlbhx;
    else
      return r_ierx;
  case REG_DAT:
    if (DLAB(regs))
      return r_dlblx;
    else
      if (store)
	return r_tbufx;
      else
	return r_rbufx;
  }
}


/*******************
 * simMaceSerialInit
 */
void
simMaceSerialInit(void){
  int targetCode = simRegisterService(simMaceSerial);

  serial = usmalloc(sizeof(Serial_t),arena);
  assert(serial);
  memset(serial,0,sizeof(Serial_t));

  /*
   * Set up two maps, one for port0, one for port 1
   */
  serial->map0.base   = serial->regs0;
  serial->map0.stride = 1;
  serial->map0.mask   = 7<<8;
  serial->map0.shift  = 8;
  serial->map1.base   = serial->regs1;
  serial->map1.stride = 1;
  serial->map1.mask   = 7<<8;
  serial->map1.shift  = 8;

  /*
   * Register all 16550 writes as SIM_loadStore_fwd
   * Register all 16550 reads as SIM_fwd_loadStore
   *
   * Note that the base address has the big-endian offset which we must
   * strip out for the masked compares to work.
   */
  simRegisterAddrMatch(~0x7ff | ADDR_MATCH_STORE,
		       TO_PHYSICAL(SERIAL_PORT0_BASE & ~7) | ADDR_MATCH_STORE,
		       SIM_loadStore_fwd,
		       targetCode,
		       &serial->map0);
  simRegisterAddrMatch(~0x7ff | ADDR_MATCH_STORE,
		       TO_PHYSICAL(SERIAL_PORT0_BASE & ~7),
		       SIM_fwd_loadStore,
		       targetCode,
		       &serial->map0);
  simRegisterAddrMatch(~0x7ff | ADDR_MATCH_STORE,
		       TO_PHYSICAL(SERIAL_PORT1_BASE & ~7) | ADDR_MATCH_STORE,
		       SIM_loadStore_fwd,
		       targetCode,
		       &serial->map1);
  simRegisterAddrMatch(~0x7ff | ADDR_MATCH_STORE,
		       TO_PHYSICAL(SERIAL_PORT1_BASE & ~7),
		       SIM_fwd_loadStore,
		       targetCode,
		       &serial->map1);

  serial->regs0[r_iir] = 0;
  serial->regs1[r_iir] = 0;
  serial->regs0[r_lsr] = LSR_XSRE|LSR_XHRE;
  serial->regs1[r_lsr] = LSR_XSRE|LSR_XHRE;

  /* Initialize I/O file descriptors */
  serial->ports[0].output = 1;
  serial->ports[0].inputs[0] = 0;

  /* Be nice to make this a command line option */
  simMaceSerialVerbose = 0;
}





