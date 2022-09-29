#include <sys/cpu.h>
#include <assert.h>
#include <sys/mace_ec.h>
#include "sim.h"
#include "simSvr.h"
#include <bstring.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#undef _KERNEL
#include <sys/wait.h>


#ifndef MAC110_BASE
#define MAC110_BASE 0xBF280000
#endif

#define UNIMPLEMENTED 0

/*
 * NOTE: The RXFIFO only contains 16 entries but the accounting uses
 * 5 bits, not 4 so that we can have 16 active entries.  This all works
 * if we simply pretend there are 32 entries.
 */
#define RXFIFOSIZE 32
#define RXFIFOSIZE_ACTUAL 16
#define RXFIFOMAP(x) (x%RXFIFOSIZE_ACTUAL)

static void txService(void);
static void rxService(void);
static void* mapPhysical(Saddr_t);


/* Message format for communication with simEtherXR*/
typedef struct _t{
  int  code;
  int  len;
  char buf[2048];
}Mac110Msg_t;
#define MSGLEN(t) (t.len + sizeof(Mac110Msg_t)-2048)

typedef union{
  long long  csr;		/* Command/Status */
  long long  concatPtrs[4];	/* Concatented buffer pointers */
  char       buf[128];		/* FIFO buffer itself */
} TXfifo;


/* Mac register numbers */
enum{
  macControl=0,		/* 00 Control an config */
  interruptStatus=1, 	/* 08 Interrupt status */
  dmaCtrl=2,		/* 10 Tx and rx DMA control */
  rxIntrTimerCtrl=3, 	/* 18 Rx interrupt delay timer */
  txRingBufRWPtr=6, 	/* 30 Tx DMA ring buffer read/write ptr */
  rxMclFIFORWPtr=8,	/* 40 Rx message cluster read/write ptr */
  physicalAddress=20,	/* A0 Physical address */
  multicastFilter=22,   /* B0 Multicast filter */
  txRingBase=23,        /* B8 TX Ring base address */
  mclRxFifo=32		/* 100 RX ring fifo */
};


static void manageChild(void);

typedef struct mac110 Mac110;

typedef struct{
  Mac110        mac;		/* Simulated MAC110 registers */
  unsigned long rxFifo[RXFIFOSIZE_ACTUAL]; /* Receive "fifo" */
  SimAddrMap_t  map;
  TXfifo*       txFifo;
  pid_t         txRxServer;	/* Send/Receive server */
} Ether_t;

static Ether_t* ether;

static int txPipe[2];
static int rxPipe[2];


/*****************
 * simEtherService
 */
static void
simEtherService(void){
  int regNum = (simControl->paddr & 0x1ff)/8;
  int isStore  = simIsStore(simControl->inst);

  switch(regNum){


  case txRingBase:
    /* Record address of tx fifo area */
    ether->txFifo = mapPhysical(ether->mac.tx_ring_base);
    break;
  case txRingBufRWPtr:
    /* Check for transmit service */
    txService();
    break;
  case rxMclFIFORWPtr:
    /* Check for receive service */
    rxService();
    break;

  case multicastFilter:
  case physicalAddress:
  case dmaCtrl:
    /* Support read/ write with no side effects */
    break;
  
  case macControl:
    /* If changing mace_control, manage xmit/receive child */
    if (isStore)
      manageChild();
    break;
  
  case mclRxFifo:
    if (!isStore)
      /* Probably shouldn't see any reads */
      ether->mac.rx_fifo =
	ether->rxFifo[RXFIFOMAP(ether->mac.rx_fifo_rptr)];
    else{
      /* Enter a new fifo entry */
      ether->rxFifo[RXFIFOMAP(ether->mac.rx_fifo_wptr)] = ether->mac.rx_fifo;
      ether->mac.rx_fifo_wptr = (ether->mac.rx_fifo_wptr + 1) % RXFIFOSIZE;
    }
    break;

  default:
    printf(" SIM MAC110- reg: %x[%x] unimplemented\n", regNum, regNum*8);
    assert(UNIMPLEMENTED);
  }
}


/******************
 * simMaceEtherInit
 */
void
simMaceEtherInit(void){
  int targetCode = simRegisterService(simEtherService);

  assert(sizeof(TXfifo)==128);	/* Sanity check */
  
  ether = usmemalign(8,sizeof(Ether_t),arena);
  assert(ether);
  memset(ether,0,sizeof(Ether_t));

  /* Initialize the Mac110 map */
  ether->map.base   = (char*)&ether->mac;
  ether->map.stride = 1;
  ether->map.mask   = 0x1ff;
  ether->map.shift  = 0;
  ether->map.ignoreIfZero = 0;

  /* Register read server for ethernet */
  simRegisterAddrMatch(~0x1ff,
		       TO_PHYSICAL(MAC110_BASE),
		       SIM_fwd_loadStore,
		       targetCode,
		       &ether->map);

  /* Register write server for ethernet */
  simRegisterAddrMatch(~0x1ff,
		       TO_PHYSICAL(MAC110_BASE) | ADDR_MATCH_STORE,
		       SIM_loadStore_fwd,
		       targetCode,
		       &ether->map);

  /* "reset" the mac */
  ether->mac.mac_control |= MAC_RESET;
}


/*************
 * manageChild	Manage transmit/receive server
 */
static void
manageChild(void){
  long long control = ether->mac.mac_control;

  if (control & MAC_RESET){
    /* Shut down the server process */
    if (ether->txRxServer){
      kill(ether->txRxServer,1);
      waitpid(ether->txRxServer,0,0);
      close(txPipe[1]);
      close(rxPipe[0]);
      ether->txRxServer = 0;
    }
    /* Reset various registers */
    ether->mac.tx_ring_rptr = 0;
    ether->mac.tx_ring_wptr= 0;
    ether->mac.rx_fifo_rptr = 0;
    ether->mac.rx_fifo_wptr= 0;
  }
  else{
    /* Start the server process */
    int pipeOpen;
    pipeOpen = pipe(txPipe);
    assert(pipeOpen>=0);
    pipeOpen = pipe(rxPipe);
    assert(pipeOpen>=0);

    ether->txRxServer = fork();
    assert(ether->txRxServer!= -1);
    if (ether->txRxServer!=0){
      /* Parent process simply closes the unused pipes and returns */
      close(txPipe[0]);
      close(rxPipe[1]);
      
      return;
    }

    /* The child process rearranges the file descriptors and then exec's */
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
     * Start transmit/recv server 
     */
    execl("/usr/local/bin/simEtherXR",0);	/* Temporary */
    fprintf(stderr," SIM MAC110- failed to create SIM_MACE tx/rx process\n");
    fflush(stderr);
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
 *
 * Service means copying the tx data to the txPipe
 */
static void
txService(void){
  int         tmp;
  int         txFifoSize;
  Mac110Msg_t tbuf;

  /* Decode fifo size */
  tmp =  (ether->mac.dma_control & DMA_TX_RINGMSK)>>DMA_TX_RINGMSK_SHIFT;
  txFifoSize = 1<<(tmp+6);

  /* So long as everything is ready, send messages */
  while(ether->txRxServer!=0 &&
	kill(ether->txRxServer,0)==0 &&
	(ether->mac.mac_control & MAC_RESET)==0    &&
	(ether->mac.dma_control & DMA_TX_DMA_EN)!=0 &&
	ether->mac.tx_ring_rptr != ether->mac.tx_ring_wptr){
    
    TXfifo*    txf = ether->txFifo + ether->mac.tx_ring_rptr;
    char*      txConCatPtr,*tptr,*cptr;
    int        flen,tlen,skip;

    /* 
     * Set the message code.
     * Figure out the true length as well as the fragment lengths.
     * Write the header (code + len).
     */
    tbuf.code = 0;
    tmp = (txf->csr & TX_CMD_OFFSET)>> TX_CMD_OFFSET_SHIFT;
    flen = 128 - tmp;		/* # bytes in fifo entry itself */
    skip = tmp % 8;		/* # bytes ignored, but in the count */
    tbuf.len = (txf->csr & TX_CMD_LENGTH)+1-skip;

    /*
     * Write the data itself.  It consist of flen bytes from the tail
     * of the fifo entry itself, and (tbuf.len - flen) bytes from the 
     * concat buffer.  Copy the data to the temporary message buffer
     * first so that we do the write in one call.
     */
    bcopy(&txf->buf[128-flen],tbuf.buf,flen);
    tptr = &tbuf.buf[flen];
    if (txf->csr & TX_CMD_CONCAT_1) {
	tlen = txf->concatPtrs[1] >> 32;
	cptr = (char *)(txf->concatPtrs[1] & 0xFFFFFFFF);
        txConCatPtr =(char*)mapPhysical((Saddr_t)cptr);
        bcopy(txConCatPtr,tptr,tlen+1);
	tptr += tlen + 1;
    }
    if (txf->csr & TX_CMD_CONCAT_2) {
	tlen = txf->concatPtrs[2] >> 32;
	cptr = (char *)(txf->concatPtrs[2] & 0xFFFFFFFF);
        txConCatPtr =(char*)mapPhysical((Saddr_t)cptr);
        bcopy(txConCatPtr,tptr,tlen+1);
	tptr += tlen + 1;
    }
    if (txf->csr & TX_CMD_CONCAT_3) {
	tlen = txf->concatPtrs[3] >> 32;
	cptr = (char *)(txf->concatPtrs[3] & 0xFFFFFFFF);
        txConCatPtr =(char*)mapPhysical((Saddr_t)cptr);
        bcopy(txConCatPtr,tptr,tlen+1);
	tptr += tlen + 1;
    }
    tmp = write(txPipe[1],&tbuf,MSGLEN(tbuf));
    assert(tmp==MSGLEN(tbuf));

    /* Update TX status */
    txf->csr |= TX_VEC_FINISHED;
    
    ether->mac.tx_ring_rptr = (ether->mac.tx_ring_rptr+1) % txFifoSize;
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
  unsigned long long *fifoP;
  Mac110Msg_t        tbuf;
  int                rxOffset;

  rxOffset = (ether->mac.dma_control & DMA_RX_OFFSET)>>DMA_RX_OFFSET_SHIFT;

  while(ether->txRxServer!=0 &&
	kill(ether->txRxServer,0)==0 &&
	(ether->mac.mac_control & MAC_RESET)==0    &&
	(ether->mac.dma_control & DMA_RX_DMA_EN)!=0 &&
	ether->mac.rx_fifo_rptr != ether->mac.rx_fifo_wptr){
    /*
     * Hardware is willing, have we received any messages?
     * Use select with a zero timeout to check.
     */
    static struct timeval timeout = {0,0};
    fd_set                readfds;
    int                   tmp;

    FD_ZERO(&readfds);
    FD_SET(rxPipe[0],&readfds);
    tmp = select(rxPipe[0]+1,&readfds,0,0,&timeout);

    /* Exit loop if no received data available. */
    if (tmp==0)
      break;

    if (tmp>0){
      /* Received data available.  Read the code and the length */
      tmp = read(rxPipe[0],&tbuf.code,sizeof(tbuf.code));
      assert(tmp==sizeof(tbuf.code));

      /* *** If we every support "codes", dispatch here. *** */

      tmp = read(rxPipe[0],&tbuf.len,sizeof(tbuf.len));
      assert(tmp==sizeof(tbuf.len));
      
      /* Copy the data to the appropriate receive buffer */
      fifoP = (unsigned long long*)
	mapPhysical(ether->rxFifo[RXFIFOMAP(ether->mac.rx_fifo_rptr)]);
      tmp = read(rxPipe[0],(char*)(fifoP+rxOffset)+2,tbuf.len);
      assert(tmp==tbuf.len);
      
      /* Post status and then advance the rptr */
      fifoP[0] = tbuf.len | RX_VEC_FINISHED;
      ether->mac.rx_fifo_rptr = (ether->mac.rx_fifo_rptr+1) % RXFIFOSIZE;
    }
    else{
      printf(" SIM MAC110 - rxService: select error\n");
      exit(1);
    }
  }
}


/*************
 * mapPhysical	Returns local address of a client supplied "physical" address.
 *------------
 * Not really checked out for general use.  Works for finding blocks of
 * k0/k1 space because they have a simple mapping.  Not necessarily the case
 * for other entity types besides RAM.
 */
static void*
mapPhysical(Saddr_t paddr){
  SimAddrMatch_t* table = simControl->addrTbl+1;
  int             i;

  for (i=1;table->matchCode!=SIM_nomatch;i++,table++)
    if ((paddr & table->mask) == table->target){
      SimAddrMap_t * map = table->mapping;
      return
	(void*)(map->base + ((paddr & map->mask)>>map->shift) * map->stride);
    }
  return 0;
}

