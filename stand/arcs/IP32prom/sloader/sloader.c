/*
 * Serial loader.
 */

#include <trace.h>
#include <flash.h>
#include <sys/cpu.h>
#include <sys/IP32flash.h>
#include <sys/uart16550.h>
#include <sys/types.h>
#include <stddef.h>

struct __fb;
typedef struct __fb FlashBuffer;

static int initSerial(void);
static getNextRecord(FlashBuffer*);
static int flushBuffer(FlashBuffer*,char*);
static int programFlash(FlashBuffer*);
static void sendChar(char c);
static void sendStr(char* s);
static void sendNum(u_long);
static int  recvChar(void);
static void ctrlInput(int);
static void flashWrite(long*,long);
static void flashWriteFlush(void);
static void flashWriteEnable(int);
static void loadFirmware(void);
static void drain(void);

#define enableInput()  ctrlInput(1)
#define disableInput() ctrlInput(0)


/*
 * Flash programming buffer.
 */
struct __fb{
  char*		 base;			/* Base address of current page  */
  char   	 buf[FLASH_PAGE_SIZE];	/* Buffer of current page data */
  long           recNum;		/* Current hex record number */
  long           bufNum;		/* Buffer counter */
  unsigned long  checksum;		/* Accumulated checksum */
  int            cnt;			/* Current field byte count */
  struct _rec{				/* Hex record data */
             long  xsum;		/* Hex record checksum */
    unsigned short addr;		/* Hex record address field */
    unsigned short oaddr;		/* Hex record offset address field */
    unsigned char  bcnt;		/* Hex record byte count */
    unsigned char  rtype;		/* Hex record type */
             char  xsumd;      		/* Hex record checksum dummy */
    unsigned char  data[80];		/* Hex record data */
  } record;
  unsigned long esa;			/* Bits <19:4> of effective addr */
  unsigned long ela;			/* Bits <31:16> of effective addr */
  int           protocol;		/* Download protocol  */
};


/* Intel hex record codes */
enum{
  DataRecord  = 0,
  EOFRecord   = 1,
  ESARecord   = 2,
  ELARecord   = 4
};
  

/* Message codes */
enum{
  Prompt      = '>',
  ParityErr   = 'P',
  FrameErr    = 'F',
  OvrrunErr   = 'O',
  ChkSumErr   = 'X',
  AddrErr     = 'A',
  EofRecords  = '$'
};


/* Return status */
enum{
  LineBreak    = Prompt    << 8,
  FramingError = FrameErr  << 8,
  ParityError  = ParityErr << 8,
  OverrunError = OvrrunErr << 8,
  ChkSumError  = ChkSumErr << 8,
  AddrError    = AddrErr   << 8,
  EofInput     = EofRecords<< 8
};


/* Protocol codes */
enum{
  DumbProtocol   = 0,
  HostedProtocol = 1
};


/* Flash ROM definition */

static const FlashROM slFlashROM = {
  (FlashSegment*)PHYS_TO_K1(FLASH_PHYS_BASE),
  (FlashSegment*)PHYS_TO_K1(FLASH_PHYS_BASE+FLASH_SIZE),
  FLASH_PAGE_SIZE,
  FLASH_SIZE,
  flashWrite,
  flashWriteFlush,
  flashWriteEnable
};


/*****************************************************************************
 *                       Top level sloader routines                          *
 *****************************************************************************/

typedef int (*Post1Rtn_t)(int,FlashSegment*,int);


/*********
 * sloader
 *--------
 * Locate and validate entire POST1 segment.
 * Locate and validate firmware segment header
 * If either invalid, or if post1() returns, load firmware and then restart
 */
void sloader(int functionCode,int resetCount){
  FlashSegment* post1Seg = findFlashSegment(&slFlashROM, POST1_SEGMENT,0);
  FlashSegment* fwSeg = findFlashSegmentHdr(&slFlashROM, FW_SEGMENT,0);
  Post1Rtn_t    post1rtn = (Post1Rtn_t)post1Seg;
  volatile unsigned long long *LEDreg =
    (unsigned long long *) PHYS_TO_K1(ISA_FLASH_NIC_REG) ; 

  /*
   * A debugging flag to force this code to fail to find a valid segment
   * and thereby force us into the serial loader.
   */
/* #define DEBUG_SLOADER */
#ifdef DEBUG_SLOADER
  post1Seg=0;
#endif

#ifdef DEBUG_POST1
  {extern int post1(); post1rtn = post1; }
#endif

  /*
   * Make sure LED is amber.
   */
  *LEDreg = *LEDreg & ~(ISA_LED_RED|ISA_LED_GREEN) ;

  /*
   * Note that return from post1 indicates the firmware segment is invalid.
   * Post1 shouldn't return under any other circumstances.
   */
  if (post1Seg==0 || fwSeg==0 || post1rtn(functionCode,fwSeg,resetCount))
    /*
     * Execute loadFirmware forever.
     * The operator must reset the machine to get out.
     */
    *LEDreg = *LEDreg & ~(ISA_LED_RED|ISA_LED_GREEN) ;
    while(1)
      loadFirmware();
}


/**************
 * loadFirmware		Load Firmware from serial line.
 */
void
loadFirmware(){
  FlashBuffer buf;
  int sts;

  buf.protocol = initSerial();
  buf.recNum=0;
  buf.bufNum = 0;
  buf.checksum = 0;
  buf.base = 0;
  buf.esa = 0;
  buf.ela = 0;

  flashWriteEnable(1);

  /* Input and process each record */
  while ((sts = getNextRecord(&buf))==0){
    if (sts=programFlash(&buf))
      break;
    buf.recNum++;
  }
  flushBuffer(&buf,0);

  flashWriteEnable(0);

  /* Send final status $<number-of-records>-<checksum> */
  sendChar(sts>>8);
  sendNum(buf.recNum);
  sendChar('-');
  sendNum(buf.checksum);
  sendChar('\n');
  drain();
}


/*****************************************************************************
 *                          FLASH access routines                            *
 *****************************************************************************/

/************
 * flashWrite	Write long to flash
 */
static void
flashWrite(long* ptr, long data){
  char* dst = (char*)ptr;
  char* src = (char*)&data;
  *dst++ = *src++;
  *dst++ = *src++;
  *dst++ = *src++;
  *dst++ = *src++;
}

/*****************
 * flashWriteFlush	Flush write to flash
 */
static void
flashWriteFlush(void){
}

/******************
 * flashWriteEnable	Enable (disable) flash writes
 */
static void
flashWriteEnable(int enable){
  long long* fe = (long long*)PHYS_TO_K1(ISA_FLASH_NIC_REG);
  if (enable)
    *fe |= ISA_FLASH_WE;
  else
    *fe &= ~ISA_FLASH_WE;
}


/*****************************************************************************
 *                          Record processing routines                       *
 *****************************************************************************/


/* Record processing state */
typedef enum{
  StartRecord,
  ByteCount,
  Address,
  OffsetAddress,
  RecordType,
  Checksum,
  Data,
  FinishRecord,
  Error
} State;

/* Record type to next state map */
const static u_char rtMap[8] = {
  Data,			/* 0 */
  Checksum,		/* 1 */
  OffsetAddress,	/* 2 */
  Error,		/* 3 */
  OffsetAddress,	/* 4 */
  Error,		/* 5 */
  Error,		/* 6 */
  Error			/* 7 */
};

/* Data for record input FSM */

#define OFF(x) offsetof(FlashBuffer,record.x)

const struct _fsmData{
  short offset;			/* Offset in FlashBuffer.record */
  short digits;			/* # digits of input */
  State nextState;		/* Next state following completion */
} fsmMap[]={
  {0,0,0},			/* StartRecord */
  {OFF(bcnt),2,Address},	/* ByteCount */
  {OFF(addr),4,RecordType},	/* Address */
  {OFF(oaddr),4,Checksum},	/* OffsetAddress */
  {OFF(rtype),2,0},		/* Record Type */
  {OFF(xsumd),2,FinishRecord},	/* Checksum */
  {OFF(data),0,Checksum},	/* Data */
  {0,0,0},			/* Error */
  {0,0,0}			/* FinishRecord */
};




/***************
 * getNextRecord	Inputs one Intel Hex-32 record
 */
int
getNextRecord(FlashBuffer* fb){
  int   c;
  int   cnt;
  int   sts = 0;
  State state;

  /* Repeat until the record is received (or an error) */
  while(1){
    /* the hosted protocol requires a prompt */
    if (fb->protocol==HostedProtocol){
      sendNum(fb->recNum);
      sendChar(Prompt);
    }
    enableInput();

    /* Input one record state machine */
    for (state=StartRecord;state<FinishRecord;)
      if ((c=recvChar())&0xff00){
	sts = c>>8;
	state = Error;
      }
      else
	/* The StartRecord state is special, it waits for the ':' character */
	if (state==StartRecord){
	  if (c==':'){
	    state = ByteCount;
	    fb->cnt = 0;
	    fb->record.xsum = 0;
	  }
	}
	else{
	  /*
	   * Most of the input state machine is here.
	   *
	   * This code decodes the input hex character (and exits if not hex).
	   * It deposits the hex digit in the input buffer, building the
	   * checksum as it goes.  When the field is filled, it advances
	   * to the next state.
	   */
	  int   hc;
	  int   max;
	  char* bp = (char*)fb + fsmMap[state].offset + (fb->cnt++)/2;
	  
	  /* Convert character to number */
	  if (c>='0' && c<='9')
	    hc = c - '0';
	  else
	    if (c>='A' && c<='F')
	      hc = c - 'A' + 10;
	    else
	      if (c>='a' && c<= 'f')
		hc = c - 'a' + 10;
	  
	  /*
	   * Update the target byte.
	   * When the byte is complete, update the checksum
	   */
	  if (fb->cnt & 1)
	    *bp = hc<< 4;
	  else{
	    *bp |= hc;
	    fb->record.xsum += (*bp)<<24;
	  }
	  
	  /*
	   * Check progress through the hex digits.  When we've maxed the
	   * digits, advance to the next state.  The next state is in
	   * fsmMap[].nextState except when the current state is RecordType
	   * when it is rtMap[fb->record.rtype].
	   */
	  max = fsmMap[state].digits;
	  if (max==0)
	    max = fb->record.bcnt*2;
	  if (fb->cnt>= max){
	    fb->cnt = 0;
	    if (state == RecordType) 
	      state = rtMap[fb->record.rtype & 7];
	    else
	      state = fsmMap[state].nextState;
	  }
	} 
    disableInput();

    if (fb->record.xsum)
      sts = ChkSumError;
    
    /*
     * If not hosted or if no error, exit, otherwise repeat.
     */
    if (fb->protocol!=HostedProtocol || sts!=0)
      break;
  }
  if (sts==0 && fb->record.rtype == EOFRecord)
    sts = EofInput;
  return sts;
}


/*************
 * flushBuffer
 */
static int
flushBuffer(FlashBuffer* fb, char* addr){
  int i;

  /*
   * Programming attempts outside of the flash are an error.
   * Programming attempts to within the serial loader segment are an error.
   */
  if ((addr < (char*)PHYS_TO_K1(FLASH_PHYS_BASE+FLASH_PROTECTED)) || 
      (addr >= (char*)PHYS_TO_K1(FLASH_PHYS_BASE+FLASH_SIZE)))
    return AddrError;
  
  /* Write out the current buffer */
  if (fb->base){
    for (i=0;i<FLASH_PAGE_SIZE/4;i++)
      flashWrite(&((long*)fb->base)[i],((long*)fb->buf)[i]);
    flashWriteFlush();
    if (fb->protocol!=HostedProtocol){
      if ((++fb->bufNum & 63)==0)
	sendChar('\n');
      sendChar('.');
    }
  }

  /* Load the page for the next buffer */
  if (addr){
    fb->base = (char*)(((unsigned long)addr + FLASH_PAGE_SIZE-1)
		       & ~(FLASH_PAGE_SIZE-1));
    for (i=0;i<FLASH_PAGE_SIZE/4;i++)
      fb->buf[i] = fb->base[i];
  }
  return 0;
}


#define EADDR(fb,i) (char*)(fb->esa+fb->ela+fb->record.addr+i)
/**************
 * programFlash
 */
static int
programFlash(FlashBuffer* fb){
  long i,idx;
  int  sts = 0;
  switch(fb->record.rtype){

  case DataRecord:
    /* Program one record */
    for (i=0;i<fb->record.bcnt;i++){
      idx = EADDR(fb,i) - fb->base;
      if (idx <0 || idx>=FLASH_PAGE_SIZE){
	if (sts = flushBuffer(fb,EADDR(fb,i)))
	  return sts;
	idx = EADDR(fb,i) - fb->base;
      }
      fb->buf[idx]= fb->record.data[i];
    }
    break;

  case ESARecord:
    /* Set the Extended Segment Address */
    fb->esa = fb->record.oaddr<<4;
    break;

  case ELARecord:
    /* Set the Extended Linear Address */
    fb->ela = fb->record.oaddr<<16;
    break;
  }
  return sts;
}


/*****************************************************************************
 *                         Serial device routines                            *
 *****************************************************************************/

/*
 * The n16c550 info structure and supporting macros is adapted from that in
 * mace_16c550.c
 */

#define NPORTS N16c550PORTS

#define __ad(p,R) (u_char* const)(SERIAL_PORT##p##_BASE+R*256)

#define __ads(p) __ad(p,REG_DAT),__ad(p,REG_ICR),\
                 __ad(p,REG_ISR), __ad(p,REG_LCR),\
                 __ad(p,REG_MCR), __ad(p,REG_LSR),\
                 __ad(p,REG_MSR)


/*
 * Port data
 *
 * Not supporting DMA interface.
 */
static const struct n16c550info{
  u_char* const data;		/* Data register (R/W) */
  u_char* const ier;		/* Interrupt enable (W) */
  u_char* const iir;		/* Interrupt identification (R) */
  u_char* const lctl;		/* Line control (R/W) */
  u_char* const mcr;		/* Modem control (R/W) */
  u_char* const lsr;		/* Line status register (R/W) */
  u_char* const msr;		/* Modem status register (R/W) */
} slport= {__ads(0)};


#define dlbl data		/* dlbl is at data offset */
#define dlbh ier		/* llbh is at ier offset */
#define fifo iir		/* fifo is at iir offset */
#define cfcr lctl		/* cfcr is at lctl offset */

typedef const struct n16c550info P16550;


#define mapBaud(r) (SERIAL_CLOCK_FREQ/(r*16))

/************
 * initSerial	Initialize serial port
 *===========
 *
 * Set up the serial port.  Announces the baud rate to the host
 * and issues the prompt
 */
int
initSerial(){
  P16550* pi = &slport;
  int     bIdx = 1;
  static const int baudTable[]={4800,9600,19200,38400,57600,115200,0};

  while(1){
    int     c;
    int     cmd = 0;
    int     cmdIdx=0;

    /* Wrap the baud table index if the rate is 0 */
    if (baudTable[bIdx]==0)
      bIdx = 0;

    /* Configure line */
    *pi->lctl = LCR_DLAB;
    *pi->dlbl = mapBaud(baudTable[bIdx])&0xff;
    *pi->dlbh = mapBaud(baudTable[bIdx])>>8 & 0xff;
    *pi->lctl = LCR_BITS8|LCR_PAREVN|LCR_PAREN;
    *pi->ier = 0;
    *pi->mcr = MCR_DTR|MCR_RTS;
    
    /* Clear the FIFO */
    *pi->fifo = FCR_FIFOEN|FCR_RxFIFO|FCR_TxFIFO;

    /* Send the startup prompt */
    sendStr("\n\rSL-");
    sendNum(baudTable[bIdx]);
    sendStr("-8E");
    sendChar(Prompt);

    /*
     * Input three characters.
     * Check for BREAK.  If true, increment the baud rate index and repeat.
     * Check for errors. If true, output error code and repeat.
     */
    enableInput();
    for (cmdIdx=0;cmdIdx!=3;cmdIdx++){
      c = recvChar();
      if (c == LineBreak){
	bIdx++;
	goto repeat;
      }
      if (c&0xff00){
	sendChar(c>>8);
	goto repeat;
      }
      cmd = cmd<<8 | c;
    }
    disableInput();

    /*
     * Check result.
     * 'SLD' == DumbProtocol.
     * 'SLH' == HostedProtocol
     */
    if (cmd == 'SLD')
      return DumbProtocol;
    if (cmd == 'SLH')
      return HostedProtocol;
  repeat:
    continue;
  }
}


/**********
 * recvChar	Input one character, checking for errors and/or BREAK.
 */
int
recvChar(){
  P16550* pi = &slport;

  while(1){
    int     lsr = *pi->lsr;

    if (lsr & LSR_BRKDET)
      return *pi->data,LineBreak;
    
    if (lsr & (LSR_FRMERR | LSR_PARERR | LSR_OVRRUN)){
      *pi->data;
      if (lsr & LSR_FRMERR)
	return FramingError;
      if (lsr & LSR_PARERR)
	return ParityError;
      return OverrunError;
    }

    if (lsr & LSR_RCA)
      break;
  }
  return *pi->data;
}


/**********
 * sendChar	Send one character.
 */
void
sendChar(char c){
  P16550* pi = &slport;
  while(!(*pi->lsr & LSR_XHRE))
    continue;
  *pi->data = c;
}


/*********
 * sendStr	Send a zero terminated character string.
 */
void
sendStr(char* s){
  char c;
  while (  c= *s++)
    sendChar(c);
}


/*********
 * sendNum	Send a decimal number.
 */
void
sendNum(u_long n){
  if (n>9)
    sendNum(n/10);
  sendChar('0'+n%10);
}


/*************
 * controlInput
 */
void
ctrlInput(int e){
  P16550* pi = &slport;
  u_char mcr = *pi->mcr;
  
  *pi->mcr = e ? (mcr | MCR_DTR) : (mcr & ~MCR_DTR);
}

/*******
 * drain	Drain the input stream
 */
void
drain(){
}


/*****************************************************************************
 * The following two string routines are reproduced here because the         *
 * libsc versions pull in much other unused code.  The problem could be fixed*
 * in libsc but it's being worked around here for now.                       *
 *****************************************************************************/

/********
 * strlen	Return string length
 */
size_t
strlen(const char* s){
  unsigned int n = 0;
  while(*s++)
    n++;
  return n;
}

/*********
 * strncmp	Compare n bytes (maximum) of two strings
 */
int
strncmp(const char *s1, const char *s2, unsigned int sn){
  int n = sn;
  
  while (--n >= 0 && *s1 == *s2++)
    if (*s1++ == '\0')
      return(0);
  return(n<0 ? 0 : *s1 - *--s2);
}



