
/**************************************************************************
 *                                                                        *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * MACE serial driver
 *
 * This code lacks conditional support for serial mouse and keyboard.
 * It is also lacks conditional support for locking and other non-IP32
 * platforms.  It can be added as needed.
 */

#include <sys/uart16550.h>

#include <saio.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/mace.h>
#include <arcs/hinv.h>
#include <arcs/folder.h>
#include <arcs/errno.h>
#include <saioctl.h>
#include <tty.h>
#include <libsc.h>
#include <libsk.h>
#include <assert.h>


#define UNIMPLEMENTED       0
#define ISA_RING_BASE_REG   0xbf310000
#define USEDTRRTS           1
#define USEFIFO             1
#if 0
#define USEAFE              1
#endif


static void ip32_disable_serial_dma(void);
static void ip32_restore_serial_dma(void);    /* for use by symmon */

static void getBaud(int unit);
static void configurePort(int unit);
static char* checkSerialBaudRate(char* baud);
static int  nextBaud(int baud);
static int mapBaud(int brate);

extern long RegisterDriverStrategy(struct component*,
			    STATUS(*)(struct component*, struct ioblock*));


#define NPORTS N16550PORTS
static int nports;		/* # ports supported (and init flag). */


/*
 * For now, we can staticly define the port register addresses. 
 * These macros define all the right addresses in the right order.
 * Note that the sequence of definitions in __ads must agree with the
 * sequence of pointers in n16c550info.  To support more dynamic device
 * configuration, move the configuration nc16c550Init. 
 */
#define __ad(p,R) (u_char*)(SERIAL_PORT##p##_BASE+R*256)

#define __ads(p) __ad(p,REG_DAT), __ad(p,REG_ICR),\
                 __ad(p,REG_ISR), __ad(p,REG_LCR),\
                 __ad(p,REG_MCR), __ad(p,REG_LSR),\
                 __ad(p,REG_MSR), __ad(p,REG_SCR),\
                 &dbufs[p]

static struct device_buf dbufs[NPORTS];
/*
 * Port data
 *
 * Not supporting DMA interface.
 */
const static volatile struct n16c550info{
  u_char* data;        /* Data register (R/W) */
  u_char* ier;		/* Interrupt enable (W) */
  u_char* iir;		/* Interrupt identification (R) */
  u_char* lctl;	/* Line control (R/W) */
  u_char* mcr;		/* Modem control (R/W) */
  u_char* lsr;		/* Line status register (R/W) */
  u_char* msr;		/* Modem status register (R/W) */
  u_char* scr;		/* Scratch register.     (R/W) */
  struct device_buf *dbuf;	/* Character device buffer */
} ports[NPORTS] = {
  {__ads(0)},
  {__ads(1)},
};

#define dlbl data		/* dlbl is at data offset */
#define dlbh ier		/* llbh is at ier offset */
#define fifo iir		/* fifo is at iir offset */
#define cfcr lctl		/* cfcr is at lctl offset */


/*
 * Port speed changes depending on the open options.  The port speed data 
 * can't be incorporated into "ports" above because "ports" is statically
 * initialized and will be in ROM and thereby unchangable.
 */
static int portSpeed[NPORTS];


/* NOTE: This probably shouldn't be here */
#define CONSOLE_PORT 0
#define REMOTE_PORT0 1



/*****************************************************************************
 *                     MACE 16C550 Driver Routines                           *
 *****************************************************************************/

/*************
 * n16c550Open
 */
static int
n16c550Open(IOBLOCK* iob){
  int unit = iob->Controller;

  /* Verify unit is valid */
  if ((unit<0) || (unit>=nports))
    return iob->ErrorNumber = ENXIO;

  /* We only support console(0), F_ECONS indicates console(1) */
  if (iob->Flags & F_ECONS)
    return iob->ErrorNumber = EINVAL;

  /* Enable scanning for this iob */
  iob->Flags |= F_SCAN;

  /* Configure the port */
  getBaud(unit);
  configurePort(unit);

  return ESUCCESS;
}


/*************
 * n16c550Poll
 */
static int
n16c550Poll(IOBLOCK* iob){
  struct device_buf *db = ports[iob->Controller].dbuf;
  char c;

  /* Drain the input fifo of characters and put them in dbuf */
  while (c = consgetc(iob->Controller))
    _ttyinput(db,c);

  iob->ErrorNumber = ESUCCESS;
}


/**************
 * n16c550Ioctl
 */
static int
n16c550Ioctl(IOBLOCK* iob){
  struct device_buf *db = ports[iob->Controller].dbuf;
  int               sts = ESUCCESS;

  switch((long)iob->IoctlCmd){
  case TIOCRAW:
    if (iob->IoctlArg)
      db->db_flags |= DB_RAW;
    else
      db->db_flags &= ~DB_RAW;
    break;
  case TIOCRRAW:
    if (iob->IoctlArg)
      db->db_flags |= DB_RRAW;
    else
      db->db_flags &= ~DB_RRAW;
    break;
  case TIOCFLUSH:
    CIRC_FLUSH(db);
    break;
  case TIOCREOPEN:
    sts = n16c550Open(iob);
    break;
  case TIOCCHECKSTOP:
    while (CIRC_STOPPED(db))
      _scandevs();
    break;
  default:  
    assert(UNIMPLEMENTED);
  }

  return sts;
}


/**************
 * n16c550Strat
 */
static int
n16c550Strategy(IOBLOCK* iob, int func){
  int          c;
  int ocnt =   iob->Count;
  char* addr = (char*)iob->Address;

  /*
   * Perform IO.
   */
  if (func == READ){
    /* Input */
    struct device_buf * db = ports[iob->Controller].dbuf;
    
    /* Loop until the request is satisfied */
    while(iob->Count >0){

      /* Drain the hardware */
      while(c = consgetc(iob->Controller)){
	_ttyinput(db,c);
      }
      
      /* If blocking is ok and no input, wait for input */
      if ((iob->Flags & F_NBLOCK)==0)
	while(CIRC_EMPTY(db))
	  _scandevs();
      
      /* If still empty, exit */
      if (CIRC_EMPTY(db)){
	iob->Count = ocnt - iob->Count;
	return ESUCCESS;
      }
      
      /* Transfer one character and update the count */
      *addr++ = _circ_getc(db);
      iob->Count--;
    }
  }
  else
    /* Output */
    if (func == WRITE){
      /* Loop until the request is satisfied */
      while(iob->Count-- >0)
	consputc(*addr++,iob->Controller);
    }
    else
      return iob->ErrorNumber = EINVAL;

  iob->Count = ocnt;
  return ESUCCESS;
}


/*************
 * n16c550Init
 */
static int
n16c550Init(IOBLOCK* iob){
  int i;
  static int pass;

  /*
   * Initialization is a bit strange.
   *
   * 1) The init_devices can be called more than once.  A "pass" count is
   *    passed in iob->Count which we use to suppress redundant init's.  (It's
   *    not clear that there's that much need to suppress them but prior
   *    code did.
   * 2) This all depends on this code being called at least once prior to
   *    all of that with iob==0 as it's on this path that nports gets set.
   *    This call is guaranteed from install.
   */
  if (iob){
    if (iob->Count==pass)
      return ESUCCESS;
    pass = iob->Count;
  }
  else if (nports==0)		/* nc16c550Init(0), early init */
    nports = NPORTS;


  /* zero the device buf's */
  bzero(dbufs,sizeof dbufs);

  /* For each port, set the baud and configure */
  for (i=0;i<nports;i++){
    getBaud(i);
    configurePort(i);
  }
  return ESUCCESS;
}


/*****************
 * n16c550ReadStat	Return success if any characters waiting.
 */
static int
n16c550ReadStat(IOBLOCK* iob){
  struct device_buf *db = ports[iob->Controller].dbuf;
  iob->Count = _circ_nread(db);
  if (iob->Count)
    return ESUCCESS;
  return iob->ErrorNumber = EAGAIN;
}


/***************
 * n16c550_strat	Driver strategy routine
 *--------------
 * Note: I'm not sure why there is a "_strat" routine for all driver functions
 * and a "strategy" routine for reads and writes.  This code is modeled
 */
static int
n16c550_strat(COMPONENT* self, IOBLOCK* iob){

  switch (iob->FunctionCode) {
    
  case	FC_INITIALIZE:
    return(n16c550Init(iob));
    
  case	FC_OPEN:
    return (n16c550Open (iob));
    
  case	FC_READ:
    return (n16c550Strategy (iob, READ));
    
  case	FC_WRITE:
    return (n16c550Strategy (iob, WRITE)); 
    
  case	FC_CLOSE:
    return (0);
    
  case	FC_IOCTL:
    return (n16c550Ioctl(iob));
    
  case	FC_GETREADSTATUS:
    return (n16c550ReadStat(iob));
    
  case	FC_POLL:
    n16c550Poll(iob);
    return 0;
    
  default:
    return(iob->ErrorNumber = EINVAL);
  }
}


/*********
 * getBaud	Get the default baud rate for the port
 */
static void
getBaud(int unit){
  char* abaud = 0;
  char* cev = getenv("console");

  assert(unit>=0 && unit<NPORTS);

  /* Determine the baud from the EV's if possible.  */

  switch (unit) {
  case CONSOLE_PORT:
    /* If "console" is "d2" then baud rate from "rbaud", otherwise "dbaud". */
    if (cev && !strcmp(cev, "d2")) {
      if (!(abaud = getenv("rbaud")))
	abaud = "19200";
    } 
    else 
      abaud = getenv("dbaud");
    break;
    
  case REMOTE_PORT0:
    /* If "console" is "d2" then baud rate from "dbaud", otherwise "rbaud". */
    if (cev && !strcmp(cev, "d2")) 
      abaud = getenv("dbaud");
    else
      if (!(abaud = getenv("rbaud")))
	abaud = "19200";
    break;
  }

  abaud = checkSerialBaudRate(abaud);
  
  if (!abaud ||
      *atob(abaud, &portSpeed[unit]) ||
      (mapBaud(portSpeed[unit]) <= 0)) {
    if (unit == CONSOLE_PORT)
      portSpeed[unit] = 9600;
    else
      portSpeed[unit] = 1200;
    if (environ)
      printf("\nIllegal %s value: %s\n", unit ? "rbaud" : "dbaud", abaud);
  }

}


/***************
 * configurePort
 */
static void
configurePort(int unit){
  const volatile struct n16c550info* pi;

  assert(unit>=0 && unit<NPORTS);

  pi = &ports[unit];

  /* Make sure ISA bus was out of reset.    */
  * (unsigned long long *) ISA_RING_BASE_REG = 0x0 ;
  
  /* Configure line */
  *pi->lctl = LCR_DLAB;
  *pi->scr  = 0x18 ; /* Configuration require some special care about
                        the scr register, right now we only care about the
                        9600 baud, and I'll come back fix it for all rest
                        of the baud rate table. */
  *pi->dlbl = mapBaud(portSpeed[unit])&0xff;
  *pi->dlbh = mapBaud(portSpeed[unit])>>8 & 0xff;
  *pi->lctl = LCR_BITS8;
  *pi->ier = 0;
#if defined(USEDTRRTS) && !defined(USEAFE)
  *pi->mcr = MCR_DTR|MCR_RTS ;
#else
#if defined(USEDTRRTS) && defined(USEAFE)
  *pi->mcr = MCR_DTR|MCR_RTS|MCR_AFE ; /* Enable HW Flow control */
#else
  *pi->mcr = 0;
#endif  
#endif  

  /* Clear the FIFO */
#if defined(USEFIFO)
  *pi->fifo = FCR_FIFOEN|FCR_RxFIFO|FCR_TxFIFO;
#else
  *pi->fifo = 0;
#endif  

  /* Flush the device buf structure */
  CIRC_FLUSH(&dbufs[unit]);
}


static int baudTable[]={
  110,300,1200,2400,4800,9600,19200,38400,115200
};
#define NBAUD (sizeof(baudTable)/sizeof(int))

/**********
 * nextBaud	Return next baud rate
 */
static int
nextBaud(int baud){
  int i;
  for (i=1;i<NBAUD;i++)
    if (baudTable[i]>baud)
      return baudTable[i];
  return baudTable[0];
}

/*********************
 * checkSerialBaudRate
 */
static char*
checkSerialBaudRate(char* baud){
  int i;
  
  if (baud){
    int ibaud = atoi(baud);
    for( i=0;i<NBAUD;i++)
      if (ibaud == baudTable[i])
	return baud;
  }
  return 0;
}


/*********
 * mapBaud	Converts baud rate to proper value for chip rate generator.
 *--------
 * Could probably be a macro.
 */
static int
mapBaud(int brate){
  return SERIAL_CLOCK_FREQ/(brate*16);
}


/**********
 * consgetc	Input a character
 */
int
consgetc(int unit){
  const volatile struct n16c550info* pi = &ports[unit];
  int lsr = *pi->lsr;		/* Read the line status register */
  u_char c;

  /* Check for a break */
  if (lsr & LSR_BRKDET){
    char buf[16];
    /* Cycle through the baud rates */
    portSpeed[unit] = nextBaud(portSpeed[unit]);
    configurePort(unit);
    sprintf(buf,"\n\n<%d-8N>\n",portSpeed[unit]);
    _errputs(buf);
  }

  /* Check for errors */
  if (lsr & (LSR_FRMERR|LSR_PARERR|LSR_OVRRUN)){
    c = *pi->data;			/* Discard the broken character */
    if (Verbose){
      if (lsr & LSR_FRMERR)
	_errputs("\nn16c550 framing error\n");
      if (lsr & LSR_PARERR)
	_errputs("\nn16c550 parity error\n");
      if (lsr & LSR_OVRRUN)
	_errputs("\nn16c550 overrun error\n");
    }
    return 0;
  }
    
  /* Check for *NO* character ready */
  if (!(lsr & LSR_RCA))
    return 0;

  /* Return the character (|0x100 to allow reading of nulls */
  return *pi->data | 0x100;
}


/**********
 * consputc	Output a character
 */
void
consputc(u_char c, int unit){
  const struct device_buf* db = ports[unit].dbuf;
  volatile u_char*           ctlr = ports[unit].lsr;

  /* If output is suspended, wait */
  while(CIRC_STOPPED(db))
    _scandevs();
  
  /* If not ready to transmit, wait */
  while(!(*ports[unit].lsr & LSR_XHRE))
    _scandevs();

  *ports[unit].data = c;
  
  
}

/**************
 * mace_errputc		Emergency output
 */
void
mace_errputc(char c){
  
  /* If not configured, do so now */
  if (nports==0)
    n16c550Init(0);

  while (!(*ports[CONSOLE_PORT].lsr & LSR_XHRE))
    continue;
  *ports[CONSOLE_PORT].data = c;
}


/*****************************************************************************
 *                     MACE 16C550 configuration                             *
 *****************************************************************************/

static COMPONENT ttytmpl = {
  	ControllerClass,		/* Class */
	SerialController,		/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	10,				/* IdentifierLength */
	"MACE-16550"			/* Identifier */
      };

static COMPONENT ttyptmpl = {
	PeripheralClass,		/* Class */
	LinePeripheral,			/* Controller */
	ConsoleIn|ConsoleOut|Input|Output,/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentifierLength */
	NULL				/* Identifier */
};

/**********************
 * mace_n16c550_install
 */
int
mace_n16c550_install(COMPONENT* top){
  COMPONENT* n16c550Ctlr;
  COMPONENT* n16c550Per;
  int i;

  n16c550Init(0);		/* Mandatory startup initialization */

  /*
   * Add Controller and peripheral entry for each port.
   * Register the driver strategy routine with each peripheral entry
   */
  for (i=0; i< NPORTS;i++){
    n16c550Ctlr = AddChild(top,&ttytmpl,0);
    assert(n16c550Ctlr);
    n16c550Ctlr->Key = i;

    n16c550Per = AddChild(n16c550Ctlr,&ttyptmpl,0);
    assert(n16c550Per);

    RegisterDriverStrategy(n16c550Per,n16c550_strat);
  }
  return 0;
}
