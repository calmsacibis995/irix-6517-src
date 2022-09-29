/*
 * Simulate serial chip.
 *
 * This code uses stdin and stdout to simulate serial channel 0 in and out.
 *
 * It would be nice to use an ioctl to set stdin into unbuffered,
 * noecho, but the name "ioctl" is defined by the firmware so we don't
 * have access to the system "ioctl".
 */
#include <IP32.h>
#include "SIM.h"
#include <n16c550.h>
#include <libsc.h>
#include <libsk.h>

#include <sys/select.h>
#include <sys/time.h>

extern int select(int,fd_set*,fd_set*,fd_set*,struct timeval*);
extern int read(int,void*,int);
extern int write(int,void*,int);
extern int open(const char*, int);
#define O_RDONLY 0

#include <assert.h>
#define IMPOSSIBLE 0
#define UNIMPLEMENTED 0

/*extern select(int,void*,void*,void*,void*); */

void lsrService(unsigned char* r, int store, int ctlr);
void switchInput(void);

/*
 * Registers for n16c550
 */
enum regCode {
  r_tbuf,			/* transmitter buffer */
  r_rbuf,			/* receiver buffer */
  r_dlbl,			/* divisor latch lsb */
  r_dlbh,			/* divisor latch msb */
  r_ier,			/* interrupt enable */
  r_iir,			/* interrupt identification */
  r_fifo,			/* fifo control */
  r_lctl,			/* line control */
  r_mcr,			/* modem control */
  r_lsr,			/* line status */
  r_msr,			/* modem status */
  r_count};
static unsigned char regs[r_count][N16c550PORTS];

#define DLAB(ctlr) (regs[r_lctl][ctlr] & CFCR_DLAB)


static transmitted=0;		/* Set to non-zero when a transmit occurs */

static simInput = 0;		/* Default simulated input fd */
static simOutput= 1;		/* Default simulated output fd */
static simInputDepth=0;		/* Counts "pushed" input fd's */
static mcrTrace = 0;		/* Controls MCR register tracing */

/********
 * regmap
 */
static enum regCode
regMap(int regnum, int store, int ctlr){
  switch(regnum){
  case MSR:
    return r_msr;
  case LSR:
    return r_lsr;
  case MCR:
    return r_mcr;
  case LCTL:
    return r_lctl;
  case IIR:
    if (store)
      return r_fifo;
    else
      return r_iir;
  case IER:
    if (DLAB(ctlr))
      return r_dlbh;
    else
      return r_ier;
  case DATA:
    if (DLAB(ctlr))
      return r_dlbl;
    else
      if (store)
	return r_tbuf;
      else
	return r_rbuf;
  }
}

/*****************
 * SIM_mace_serial
 */
void
SIM_mace_serial(Instr_t inst,Saddr_t eadr, Ctx_t* sc, int ctlr){
  char buf[128];
  int store = isStore(inst);
  int regnum = (eadr>>8) & 7;
  enum regCode reg = regMap(regnum,store,ctlr);
  unsigned char* r=&regs[reg][ctlr] ;

  assert(loadstoreSize(inst)==1);

  switch(reg){

  case r_tbuf:			/* Transmit buffer */
    loadstore(inst,sc,r);
    if (regs[r_lsr][ctlr]&LSR_TXRDY){
      static long history=0;
      history = history<<8 | *r;
      write(simOutput,r,1);
      transmitted += 1;
      regs[r_lctl][ctlr] &= ~LSR_TXRDY;
      if (history == '-8E>')
	switchInput();
    }
    break;

  case r_rbuf:			/* Receive buffer */
    loadstore(inst,sc,r);
    break;

  case r_dlbl:			/* Baud rate divisors */
  case r_dlbh:
    loadstore(inst,sc,r);
    break;

  case r_ier:			/* Interrupt enable registers */
    loadstore(inst,sc,r);

#ifdef VERBOSE
    if (*r){
      sprintf(buf," 16c550, IER[%d]:%02x, interrupts not simulated\n",ctlr,*r);
      SIM_message(buf);
    }
#endif
    break;

  case r_iir:			/* Interrupt identification register */
    assert(UNIMPLEMENTED);

  case r_fifo:
    loadstore(inst,sc,r);
#ifdef VERBOSE
    sprintf(buf,
	    " 16c550, FIFO[%d]:%02x, ENB: %d, RST_RCV: %d, RST_XMT: %d\n",
	    ctlr,*r,(*r)&1,(*r>>1)&1,(*r>>2)&1);
    SIM_message(buf);
#endif
    break;

  case r_lctl:			/* Line control register */
    if (store){
      loadstore(inst,sc,r);
#ifdef VERBOSE
      sprintf(buf,
	      " 16c550, LCTL[%d]:%02x, bits: %d, "
	      "stop: %d, PE: %d, EvnP: %d Brk: %d, DLAB: %d\n",
	      ctlr, *r,
	      (*r&3)+5,(*r>>2)&1,(*r>>3)&1,(*r>>4)&1,(*r>>6)&1,(*r>>7)&1);
      SIM_message(buf);
#endif
    }
    break;

  case r_mcr:			/* Modem control register */
    loadstore(inst,sc,r);
    
    /*
     * During sloader tests, we use HW flow control which would generate
     * too many messages so MCR messages are gated.
     */
    if (!mcrTrace){
#ifdef VERBOSE
      sprintf(buf, " 16c550, MCR [%d]:%02x, dtr: %d, rts: %d\n",
	      ctlr, *r,
	      (*r)&1,(*r>>1)&1);
      SIM_message(buf);
#endif
    }
    break;

  case r_lsr:			/* Line status register */
    lsrService(r,store,ctlr);
    loadstore(inst,sc,r);
    if (*r & (LSR_FE|LSR_PE|LSR_OE))
      *r &= ~(LSR_FE|LSR_PE|LSR_OE);
    break;

  case r_msr:			/* Modem status register */
    assert(UNIMPLEMENTED);

  default:
    assert(IMPOSSIBLE);
  }
}



/************
 * lsrService	Coordinate important control bits
 */
void
lsrService(unsigned char* r, int store, int ctlr){
  static struct timeval timeout = {0,0};
  unsigned char v= *r;
  fd_set readfs;

  FD_ZERO(&readfs);
  FD_SET(simInput,&readfs);

  assert(ctlr==0);
  
  /* Break check here */
  v &= ~LSR_BI;

  /* Check for input */
  if (select(4,&readfs,0,0,&timeout)){
    v |= LSR_RXRDY;
    read(simInput,&regs[r_rbuf][ctlr],1);
  }
  else
    v &= ~LSR_RXRDY;
      

  /* Can generate errors here */

  /* Set transmit ready here */
  if (transmitted){
    transmitted--;
    v &= ~LSR_TXRDY;
  }
  else
    v |= LSR_TXRDY;

  *r = v;
}

/*************
 * switchInput	Switch the input stream
 *============
 * A string of "*-8E>" has been output.  This is tail of the serial loader
 * prompt.  We want to switch to an alternate input to test the serial loader
 * code.
 */
void
switchInput(){
  int fd = open("fw.hex",O_RDONLY);
  extern void exit(int);

  if (simInputDepth!=0){
    char* s = "\nSIM: simInputDepth!=0\n";
    write(1,s,strlen(s));
    exit(1);
  }
  if (fd<0){
    char* s = "\nSIM: Unable to open fw.hex\n";
    write(1,s,strlen(s));
    exit(1);
  }
  simInputDepth++;
  simInput = fd;
  mcrTrace++;
}

