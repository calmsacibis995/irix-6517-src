#include "SIM.h"
#include <mace.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>

extern void bcopy(const void*, void*, int);


#define _FIFO x.hdr

/* Message format for communication with client/server */

typedef struct _t{
  int  code;
  int  len;
  char buf[2048];
} Mac110Msg_t;


static pid_t child;		/* Child process */
static int txPipe[2];
static int rxPipe[2];
static Mac110 mac110;
static long rxFifo[64];

enum{
  macControl,		/* 00 Control an config */
  interruptStatus, 	/* 08 Interrupt status */
  dmaCtrl,		/* 10 Tx and rx DMA control */
  rxIntrTimerCtrl, 	/* 18 Rx interrupt delay timer */
  txIntrAlias,		/* 20 Alias of tx interrupt bits */
  rxIntrAlias,		/* 28 Alias of rx interrupt bits */
  txRingBufReadPtr, 	/* 30 Tx DMA ring buffer read ptr */
  txRingBufWritePtr, 	/* 38 Tx DMA ring buffer write ptr*/
  txRingBufProducerPtr, /* 40 diag */
  txPingPongProducerPtr,/* 48 diag */
  txPingPongConsumerPtr,/* 50 diag */
  txPingPongMemWritePtr,/* 58 diag */
  rxMclFIFOReadPtr,	/* 60 Rx message cluster read ptr */
  rxMclFIFOWritePtr,	/* 68 Rx message cluster write ptr */
  rxMclPingPongReadPtr,	/* 70 diag */
  rxMclPingPongWritePtr,/* 78 diag */
  phyDataInOut,		/* 80 */
  phyAddress,		/* 88 */
  phyReadStart,		/* 90 */
  backoff,		/* 98 Random number seed for backoff */
  intReq_lastTxVec,	/* a0 diag */
  txRingBase,		/* a8 Transmit ring base addr (phys) */
  multicastFilter,	/* b0 Multicast logical address mask */
  physicalAddress,	/* b8 Ethernet physical address */
  txPkt1CmdHdr,		/* c0 diag */
  txPkt1CatBuf1,	/* c8 diag */
  txPkt1CatBuf2,	/* d0 diag */
  txPkt1CatBuf3,	/* d8 diag */
  txPkt2CmdHdr,		/* e0 diag */
  txPkt2CatBuf1,	/* e8 diag */
  txPkt2CatBuf2,	/* f0 diag */
  txPkt2CatBuf3,	/* f8 diag */
  mclRxFifo		/*100 */
};


/***************
 * initSimMac110
 */
void
initSimMac110(){
  memset((void*)&mac110,0,sizeof mac110);
  mac110.macControl |= MAC_RESET;
}



/************
 * txFifoSize	Returns number of entries in tx fifo
 */
int
txFifoSize(void){
  int fs = mac110.dmaCtrl & DMA_TX_RINGMSK;
  switch(fs){
  case DMA_TX_16K : return 128;
  case DMA_TX_32K : return 256;
  case DMA_TX_64K : return 512;
  default: sim_assert(IMPOSSIBLE);
  }
}

/*************
 * manageChild	Manage ethernet simulation child helper
 *------------
 * Simulating ethernet transmit/receive is complicated enough to warrant
 * using a child process.  We fork a child process and exec SIM-MACE.  We
 * transmit/receive by writing and reading the pipes that connect us to the
 * child process.
 */
static void
manageChild(long long control){
  extern int wait(int*);
  if (control & MAC_RESET){
    if (child){
      kill(child,1);		/* Tell child to quit */
      wait(0);			/* Wait for it to exit */
      close(txPipe[1]);		/* Close pipes to/from TX/RX server */
      close(rxPipe[0]);
    }

    /* Reset various registers */
    mac110.txRingBufReadPtr = 0;
    mac110.txRingBufWritePtr= 0;
    mac110.rxMclFIFOReadPtr = 0;
    mac110.rxMclFIFOWritePtr= 0;
  }
  else{
    /* Create pipes */
    int pipeOpen;
    pipeOpen = pipe(txPipe);
    sim_assert(pipeOpen>=0);
    pipeOpen = pipe(rxPipe);
    sim_assert(pipeOpen>=0);

    /* Verify that I/O assigments are as we expect */
    sim_assert(txPipe[0]==3);
    sim_assert(txPipe[1]==4);
    sim_assert(rxPipe[0]==5);
    sim_assert(rxPipe[1]==6);

    child = fork();
    sim_assert(child!=-1);
    if (child!=0){
      close(txPipe[0]);
      close(rxPipe[1]);
      return;
    }
    close(0);
    dup(txPipe[0]);
    close(1);
    dup(rxPipe[1]);
    close(txPipe[0]);
    close(txPipe[1]);
    close(rxPipe[0]);
    close(rxPipe[1]);
    /*
     * 0 => tx input  pipe from client
     * 1 => rx output pipe to client
     * 2 => standard error output
     */

    /*
     * Exec the server process.  
     */
    execl("/tmp/SIM_MACE",0);
    exit(1);
  }
}


/***********
 * txService	Service "transmit"
 *----------
 *
 * Check for transmitter service.  Service occurs if:
 *
 *   1) MAC is not reset.
 *   2) TX DMA is enabled
 *   3) TX wptr!= TX rptr
 *   4) txServiceCtr %3 == 0
 *
 * Service means copying the tx data to the txPipe
 */
static void
txService(void){
  static txServiceCtr;
  Mac110Msg_t t;

  if ((mac110.macControl & MAC_RESET) == 0 &&
      (mac110.dmaCtrl & DMA_TX_DMAENAB) &&
      (mac110.txRingBufReadPtr != mac110.txRingBufWritePtr) &&
      (txServiceCtr = (txServiceCtr+1)%3) == 0){

    /*
     * Data is distributed between the fifo entry and the concatenation
     * buffer as shown below:
     *
     * 	+-----+	-		<- fifo entry
     *  | hdr | |
     *     :    |...............fifo.byteOffset
     *  |     | |
     *  |=====| | <-+ <---+	<- dword aligned "start" of buffer
     *  |     | |   |     |
     *  |     | |   |      >.....skip
     *  |     | |   |     |
     *  |-----| v   | <---+	<- actual start of buffer
     *  |  X  |     |
     *     X         >...........fifo.requestLength
     *  |  X  |     |
     *  +-----+     |		<- end of fifo entry
     *              |
     *  +-----+     |		<- start of concatenation buf
     *  |  X  |     |
     *  |  X  |     |
     *     X        |
     *  |  X  |     |
     *  /-----/  <--+
     *  |     |
     *     :  
     *  |     |
     *  +-----+			<- end of concatenation buf
     *
     * Note: The data in the fifo entry must be shifted towards the end
     *         of the fifo entry.
     *       The requestLength in the fifo header includes the "skipped" bytes.
     *       The number of "skipped" bytes is the difference between the
     *         byte offset and the byte offset rounded back to the nearest
     *         dword aligned offset.
     *
     * We must figure out where the actual data is and copy it into the 
     * message we send to the server.
     */
    TXfifo* txf = &((TXfifo*)mac110.txRingBase)[mac110.txRingBufReadPtr];
    char*   cp = t.buf;
    int     reqLen, flen, skip;

    /* Compute # bytes in fifo entry itself */
    flen = sizeof(txf->fifoBuf) - txf->_FIFO.cmd.byteOffset;

    /*
     * Compute the "skip" value so we can figure out the true request length
     * from the fifo.requestLength field.  (This should equal m->m_len from
     * the originating mbuf).
     */
    skip = txf->_FIFO.cmd.byteOffset % 8 ;
    reqLen = txf->_FIFO.cmd.requestLength + 1 - skip;

    t.code = 0;
    t.len = reqLen;

    /* Copy the data from the fifo entry to the buffer */
    bcopy((void*)&txf->fifoBuf[txf->_FIFO.cmd.byteOffset],cp,flen);
    cp += flen;
    if (reqLen>flen){
      sim_assert(txf->_FIFO.cmd.concatPtr0);
      bcopy((void*)txf->x.ptr[0],cp,reqLen-flen);
    }
    write(txPipe[1],&t,t.len+2*sizeof(int));

    /*
     * Update the status to mark the buffer as sent, then
     * Advance the read pointer for the buffer
     */
    txf->_FIFO.ll = 0;
    txf->_FIFO.sts.finished = 1;
    txf->_FIFO.sts.transmitLength = t.len;
    mac110.txRingBufReadPtr = (mac110.txRingBufReadPtr+1) % txFifoSize();
  }
}


/***********
 * rxService	Service "receive"
 *----------
 *
 * Check for receiver service. Service occurs if:
 *
 *   1) MAC is not reset
 *   2) RX DMA is enabled
 *   3) RX wptr != RX rptr
 *   4) select( rxPipe) is true
 *
 * Service means copying data from rxPipe to rx buffer
 */
static void
rxService(void){
  static struct timeval timeout = {0,0};
  Mac110Msg_t t;
  int         tmp;

  fd_set readfds;
  FD_ZERO(&readfds);
  FD_SET(rxPipe[0],&readfds);

  while ((mac110.macControl & MAC_RESET) == 0 &&
	 (mac110.dmaCtrl & DMA_RX_DMAENAB) &&
	 (mac110.rxMclFIFOReadPtr != mac110.rxMclFIFOWritePtr) &&
	 (tmp = select(rxPipe[0]+1,&readfds,0,0,&timeout))){
    tmp = read(rxPipe[0],&t.code,sizeof(t.code));
    
    /* Note, an EOF from the pipe will trigger the following assertion */
    sim_assert(tmp == sizeof(t.code));
    sim_assert(t.code==0);
    {
      /*
       * Get the "physical" address of the buffer. 
       * (NOTE: For this simulation, we needn't distinguish between physical
       *  and virtual as all addresses passed to us are umode.
       */
      long long* rxb= (long long*)rxFifo[mac110.rxMclFIFOReadPtr];
      
      /* Read and check the message length */
      tmp = read(rxPipe[0],&t.len,sizeof(t.len));
      sim_assert(tmp == sizeof(t.len));

      /*
       * Set the receive status.
       * Copy the data to the receive buffer.
       */
      *rxb = 0;
      ((RXbuf*)rxb)->receiveLength = t.len;
      ((RXbuf*)rxb)->finished = 1;

      rxb += (mac110.dmaCtrl & DMA_RX_OFFSET)>>DMA_RX_OFFSET_SHF;
      tmp = read(rxPipe[0],((char*)rxb)+2,t.len);
      sim_assert(tmp == t.len);
    }
    /* Advance the read pointer */
    mac110.rxMclFIFOReadPtr = 
      (mac110.rxMclFIFOReadPtr+1) % RXFIFOSIZE;
  }
}

/************
 * SIM_mac110
 */
void
SIM_mac110(Instr_t inst, Saddr_t eadr, Ctx_t* sc, int reg){
  int regNum = (eadr & 0x1ff)/8;
  char* regPtr = ((char*)(&mac110))+(regNum*8);

  txService();
  rxService();

  /* Dispatch to appropriate register */
  switch(regNum){
    /*
     * Note: for now the side effects of accessing these registers aren't
     * being handled.
     */
  case macControl:
    loadstore(inst,sc,regPtr);
    if (isStore(inst))
      manageChild(mac110.macControl);
    break;

  case dmaCtrl:
  case txRingBase:		/* Pointer to TX fifo */

  case multicastFilter:
  case physicalAddress:
    loadstore(inst,sc,regPtr);
    break;

  case txRingBufWritePtr:	/* TX fifo wptr */
    loadstore(inst,sc,regPtr);
    break;
  case rxMclFIFOWritePtr:	/* Read rx wptr */
  case rxMclFIFOReadPtr:	/* Read rx rptr */
  case txRingBufReadPtr:	/* Read rx rptr */
    if (!isStore(inst))
      loadstore(inst,sc,regPtr);
    else
      advancePC(inst,sc);	/* Ignore writes */
    break;

  case mclRxFifo:
    /*
     * Access the RX fifo window.
     */
    if (!isStore(inst))
      /* If read, load from rxFifo[rptr] */
      mac110.mclRxFifo = rxFifo[mac110.rxMclFIFOReadPtr];

    loadstore(inst,sc,regPtr);

    if (isStore(inst)){
      /* If write, store to rxFifo[wptr++] */
      rxFifo[mac110.rxMclFIFOWritePtr] = mac110.mclRxFifo;
      mac110.rxMclFIFOWritePtr = (mac110.rxMclFIFOWritePtr+1) % 64;
    }
    break;

  default:
    /* Report the offending register then quit */
    {
      extern void sprintf(const char*,...);
      char buf[132];
      sprintf(buf," sim_mac110: reg: %x[%x}\n", regNum, regNum*8);
      SIM_message(buf);
    }
    sim_assert(UNIMPLEMENTED);
  }
}
