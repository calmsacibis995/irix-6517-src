/*
 * Moosehead MACE 10/100 Mbit/s Fast Ethernet Interface Driver
 *
 * Copyright 1995, Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.4 $"

#include <sys/cpu.h>
#include <sys/sbd.h>
#include <libsc.h>
#include <libsk.h>

#include <arcs/cfgdata.h>
#include <arcs/errno.h>
#include <arcs/folder.h>
#include <arcs/hinv.h>

#include <net/in.h>
#include <net/socket.h>
#include <net/arp.h>
#include <net/ei.h>
#include <net/mbuf.h>
#include <saioctl.h>

#include <assert.h>
#include <mace_ec.h>

extern void *getTile(void);
extern void usecwait(int);
extern ULONG GetRelativeTime(void);

extern int Debug, Verbose;
#define VPRINTF if (Verbose) printf

extern int
_if_strat(struct component*, struct ioblock*);
extern long
RegisterDriverStrategy(struct component*,
		       STATUS(*)(struct component*, struct ioblock*));

static void
ec_poll(IOBLOCK* iob);

static void
setPhySpecifics(int);


/*
 * Fifo size definitions
 *
 * Note that the RXFIFO is actually 16 entries but we treat it as 32
 * entries.  The HW uses 5 bit indexes, not 4 bit indexes, allowing it
 * to manage 16 active RXFIFO entries.  
 */
#define TXFIFOSIZE		64
#define RXFIFOSIZE  		32
#define RXFIFOSIZE_ACTUAL	16

/*
 * BUFCNT is the number of TX and RX buffers.  The algorithm we use
 * to map read/write pointers to the appropriate buffer requires that
 * BUFCNT *2^n == xxFIFOSIZE.
 *
 * Note that although
 */
#define BUFCNT			16

#define	CRCLEN			4
#define	ETHERHDR		14
#define	ETHERMAXLEN		1536
#define	ETHERMINLEN		64
#define	MIN_TXPKT_LEN		(ETHERMINLEN - CRCLEN)

/* Advance a FIFO pointer  */
#define ADVANCE(x,s)		(x) = ((x)+1)&((s)-1) 

#define MAC_MASK (MAC_RESET|MAC_FULL_DUPLEX|MAC_LOOPBACK|\
		  MAC_100MBIT|MAC_NORMAL|MAC_ALL_MULTICAST)
#define RX_ERRORS (RX_VEC_INVALID_PREAMBLE|RX_VEC_CRC_ERROR|\
		   RX_VEC_DRIBBLE_NIBBLE|RX_VEC_CODE_VIOLATION)

/* typedef for struct mac110 */
typedef struct mac110 Mac110;

/* TX fifo entry */
typedef union{
  unsigned long long TXCmd;
  unsigned long long TXStatus;
  unsigned long long TXConcatPtr[4];
  char               buf[128];
} TXfifo;

/* RX and TX buffers */
typedef union{
  unsigned long long RXStatus;
  char               buf[2048];
} RXbuf;

typedef union{
  unsigned long long TXStatus;
  char               buf[2048];
} TXbuf;

#define RXBUF_OFFSET	1	/* # of dwords to skip when filling RX buf */
#define	DMA_RX_BOFFSET	2	/* hdwr RX padding at start of dword */
#define DMA_RX_PAD	2+8	/* RX padding at front of buffer */

#define	DEADPACKET	0x7E0001

/*
 * Per-interface data
 */
typedef struct{
  struct arpcom   ei_ac;		/* Common ethernet structures */
  char		  ref;			/* Open reference count */
  char		  rxRptr;		/* SW copy of rx wptr */
  short           txRptr, txWptr;	/* SW copy of tx rptr & wptr */
  short		  txPending;      	/* # of TX operations pending */
  volatile TXbuf  *txBufs[BUFCNT];	/* Pointers to TX buffers */
  volatile TXfifo *txFifo;		/* Ptr to HW TX fifo area */
  volatile RXbuf  *rxBufs[BUFCNT];	/* Pointers to RX buffers */
  volatile Mac110 *mac;			/* Ptr to MAC hw. */
  volatile char   *bufferPool;		/* Ptr to 64KB buffer pool */
  ULONG		  activity;		/* Transmit activity timer */
} EhInfo;
static EhInfo  eh_info;
static EhInfo* ei;
static unsigned char    ec_phy = 255, ec_prev, ec_mrev;
static int     ec_ptype;


#define ei_if   ei_ac.ac_if	/* Network visible interface */
#define ei_addr ei_ac.ac_enaddr	/* Current Ethernet address */
#define cei(x)  ((struct ether_info*)(x->DevPtr))

#define SGI_ETHER_ADDR 0x08	/* First byte of SGI ethernet addr's */

#define EC_ACTIVE (IFF_UP|IFF_RUNNING|IFF_BROADCAST)


/*****************************************************************************
 *                          Mace Driver routines                             *
 *****************************************************************************/


/**********
 * ec_reset	Reset ethernet interface
 */
static void
ec_reset(void){
  ei->mac->dma_control = 0;			/* Reset DMA */
  ei->mac->mac_control = MAC_RESET;	/* Reset everything else */
}

/**********
 * ec_mdio_rd		Read a phy register over the MDIO bus
 */
static int
ec_mdio_rd(volatile Mac110 *mac, int reg) {
  int    val;

  mac->phy_address = (ec_phy << 5) | (reg & 0x1f);
  mac->phy_read_start = reg;
  usecwait(25);
  while ((val = mac->phy_dataio) & MDIO_BUSY) {
    usecwait(25);
  }

  return val;
}

/**********
 * ec_mdio_wr		Write a phy register over the MDIO bus
 */
static int
ec_mdio_wr(volatile Mac110 *mac, int reg, int val) {

  mac->phy_address = (ec_phy << 5) | (reg & 0x1f);
  mac->phy_dataio = val;
  usecwait(25);
  while ((val = mac->phy_dataio) & MDIO_BUSY) {
    usecwait(25);
  }

  return val;
}

/**********
 * ec_mdio_rmw		Modify a phy register over the MDIO bus
 */
static void
ec_mdio_rmw(volatile Mac110 *mac, int fireg, int mask, int val)
{
  register int rval, wval;

  rval = ec_mdio_rd(mac, fireg);
  wval = (rval & ~mask) | (val & mask);
  (void) ec_mdio_wr(mac, fireg, wval);
}

/**********
 * ec_mdio_probe	Probe the management interface for PHYs
 */
static int
ec_mdio_probe(volatile Mac110 *mac) {
  int	i, val, p2, p3;

  /* already found the phy? */
  if (ec_phy < 32)
    return ec_ptype;

  /* probe all 32 slots for a known phy */
  for (i = 0; i < 32; ++i) {
    ec_phy = (char)i;
    p2 = ec_mdio_rd(mac, 2);
    p3 = ec_mdio_rd(mac, 3);
    if (Debug > 2) printf("ec0: ec_mdio_probe p2=0x%04x p3=0x%04x\n", p2, p3);
    val = (p2 << 12) | (p3 >> 4);
    switch (val) {
      case PHY_DP83840:
	ec_mdio_rmw(mac, 23, 1 << 5, 1 << 5);
      case PHY_QS6612X:
      case PHY_ICS1889:
      case PHY_ICS1890:
	ec_prev = p3 & 0xf;
	ec_ptype = val;
	setPhySpecifics(val);
	return val;
    }
  }
  ec_phy = 255;

  return -1;
}


/**********
 * ec_phy_speed		Probe the PHY for our negotiated speed
 */
static int
ec_phy_speed(volatile Mac110 *mac) {
  int	i, val = 0, ctl = 0, p5, p6;
  char  *mode;

  if (Debug) printf("ec0: phy=%x\n", ec_ptype);
  /* environment can force override mode (10, H10, F10, 100, H100, F100) */
  if ((mode = getenv("ec0mode")) != NULL) {
    /* note: if the user miss types, we don't fully check, who cares? */
    if ((mode[0] == 't') || (mode[0] == 'T')) {
	/* external phy loopback test mode */
	ctl |= PHY_PCTL_DUPLEX | PHY_PCTL_LOOPBACK;
	val |= MAC_FULL_DUPLEX;
    } else if ((mode[0] == 'f') || (mode[0] == 'F')) {
	/* full-duplex operating mode */
	ctl |= PHY_PCTL_DUPLEX;
	val |= MAC_FULL_DUPLEX;
    }
    if (((mode[1] == '1') && (mode[3] == '0')) ||
	((mode[0] == '1') && (mode[2] == '0'))) {
	/* 100Mbit operating speed */
	ctl |= PHY_PCTL_RATE;
	val |= MAC_100MBIT;
    }
    ec_mdio_wr(mac, 0, ctl);
    if (Debug) printf("ec0: write phy reg0 0x%04x\n", ctl);

  /* MACE rev 0, force 100Base-TX */
  } else if (!ec_mrev) {
    val = MAC_100MBIT | MAC_FULL_DUPLEX;		/* 100Mb-TX */
    ec_mdio_wr(mac, 0, PHY_PCTL_RATE | PHY_PCTL_DUPLEX);
    if (Debug) printf("ec0: MACE1.0 forcing 100Mb operation\n");

  /* Auto-negotiation path... */
  } else {
    /* always restart autonegotiation (do we really need to do this?) */
    if (Debug) printf("ec0: initiating autonegotiation\n");
    ec_mdio_wr(mac, 0, PHY_PCTL_AN_ENABLE );

    /* wait for autonegotiation progress monitor to indicate success? */
    usecwait(100000);
    for (i = 0; (i < 30) && (ec_mdio_rd(mac, 1) & PHY_PMSR_ANC) == 0; ++i)
      usecwait(100000);

    /* check for autonegotiation complete */
    p6 = ec_mdio_rd(mac, 6);
    if ((p6 & 1) == 0) {
    	if (Debug) printf("ec0: !p6 & 1\n");
    	switch (ec_ptype) {
    	case PHY_DP83840:
    		p6 = ec_mdio_rd(mac, 25);
    		if ((p6 & 0x40) == 0)
    			val |= MAC_100MBIT;
    		break;
    	case PHY_QS6612X:
    		p6 = ec_mdio_rd(mac, 31);
    		if (p6 & 0x8) {
    			val |= MAC_100MBIT;
    		}
    		if (p6 & 0x10) {
    			val |= MAC_FULL_DUPLEX;
    		}
    		break;
    	case PHY_ICS1889:
    	case PHY_ICS1890:
    		p6 = ec_mdio_rd(mac, 17);
    		if (p6 & 0x8000) {
    			val |= MAC_100MBIT;
    		}
    		if (p6 & 0x4000) {
    			val |= MAC_FULL_DUPLEX;
    		}
    		break;
    	default:
    		goto anl;
    	}
    } else {
anl:
      /* get negotiated mode from phy link partner ability register */
      p5 = ec_mdio_rd(mac, 4) & ec_mdio_rd(mac, 5);
      if (Debug) printf("ec0: read phy reg5 0x%04x\n", p5);
      if (p5 & PHY_PLPA_TAF4) 
	val |= MAC_100MBIT;				/* 100Mb-T4 */
      else if (p5 & PHY_PLPA_TAF3)
	val |= MAC_100MBIT | MAC_FULL_DUPLEX;		/* 100Mb-TX */
      else if (p5 & PHY_PLPA_TAF2)
	val |= MAC_100MBIT;				/* 100Mb-TX-HD */
      else if (p5 & PHY_PLPA_TAF1)
	val |= MAC_FULL_DUPLEX;				/* 10Mb-FD */
      /* don't need to set anything for 10Mb-HD */
      if (Debug) printf("ec0: val %x\n",val);
    }
  }
#if 0
  if (Verbose) {
	printf("ec0: %d Mb/s %s-duplex\n",
		(val & MAC_100MBIT) ? 100 : 10,
		(val & MAC_FULL_DUPLEX) ? "full" : "half");
  }
#endif

  /* PLL settling time */
  usecwait(100000);

  /* Discard currect latched contents of reg1 */
  (void) ec_mdio_rd(mac, 1);

  /* link settling time */
  usecwait(100000);

  return val;
}

/*********
 * ec_init	Set up ethernet interface for use
 */
static int
ec_init(void){
  int speed, i;
  volatile Mac110 *mac = ei->mac;
  union {
    char eaddr[8];
    long long laddr;
  } eau;
  char *cp;

  /*
   * Full reset
   */
  ec_reset();

  /*
   * Put the mac110 into a quite operating mode
   * (Also clears the reset bit set in ec_reset).
   */
  mac->mac_control = (mac->mac_control & ~MAC_MASK) |
	MAC_NORMAL | MAC_DEFAULT_IPG;

  /*
   * Probe MDIO bus for PHYs
   */
  if (ec_mdio_probe(mac) == -1) {
    if (Debug) printf("ec0: ec_mdio_probe failed\n");
    ec_reset();
    return -1;
  }

  /*
   * Set MAC110 100Mb/10Mb & Duplex mode to match PHY values
   */
  mac->mac_control |= ec_phy_speed(mac);

  /*
   * Check for valid link
   */
  if ((ec_mdio_rd(mac, 1) & PHY_PMSR_LINK) == 0) {
    printf("ec0: no carrier: check Ethernet cable\n");
    ec_reset();
    return -1;
  }

  /*
   * Allocate the TX FIFO if not done previously.
   *
   * NOTE: Because the TX FIFO must be aligned on an 8K boundry, we allocate
   *       it from a whole tile.  The rest of the tile isn't used. We could
   *       use the rest of the tile as the TX and RX buffers.  However, the
   *       current driver  algorithm relies on the number of buffers being
   *       a power of 2. By limiting the buffer count to 8, we could fit the
   *       buffers into this tile.  Because ethernet traffic comes in bursts,
   *	   and because we only check for traffic by polling, 8 receive buffers
   *       seems too low so we use a separate tile for the buffers. If we
   *       changed the algorithm, we could use 14 buffers from
   *       this tile -- that would probably be enough.
   */
  if (ei->txFifo == 0) {
    ei->txFifo = (TXfifo*)getTile();
    if (ei->txFifo == 0) {
      if (Debug) printf("ec0: getTile returned NULL for txFifo\n");
      return -1;
    }
  }
  bzero((char *)ei->txFifo, 65536);
  mac->tx_ring_base = K1_TO_PHYS(ei->txFifo);

  /*
   * Set the physical address
   * Clear the multicast mask
   */
  eau.eaddr[0] = eau.eaddr[1] = 0;
  cpu_get_eaddr((unsigned char*)&eau.eaddr[2]);
  mac->physaddr = eau.laddr;
  mac->secphysaddr = 0;
  mac->mlaf = 0;
  if (eau.eaddr[2] & 1) {
    printf("ec0: invalid ethernet station address\n");
    ec_reset();
    return -1;
  }

  /* Allocate and connect up the buffer pool if not done previously */
  if (ei->bufferPool == 0) {
    ei->bufferPool = getTile();
    if (ei->bufferPool == 0) {
      if (Debug) printf("ec0: getTile returned NULL for bufferPool\n");
      return -1;
    }

    /*
     * For each 4096 byte page in the tile, use the first 2048 bytes
     * for a receive buffer (it must be 4096 byte aligned) and the second
     * 2048 bytes for a transmit buffer (it doesn't have to be 4096 byte
     * aligned but cannot cross a 4096 boundry).
     */
    bzero((char *)ei->bufferPool, 65536);
    for (i = 0; i < BUFCNT; i++) {
      ei->rxBufs[i] = (RXbuf*)&ei->bufferPool[i*4096];
      ei->txBufs[i] = (TXbuf*)&ei->bufferPool[i*4096+2048];
    }
  }

  /* Pass the receive buffer pointers to the HW */
  for (i = 0; i < BUFCNT; i++) {
    ei->rxBufs[i]->RXStatus = 0;
    mac->rx_fifo = K1_TO_PHYS(ei->rxBufs[i]);
  }

  /* Init the SW copies of the RX and TX DMA FIFO indexes. */
  ei->rxRptr = 0;
  ei->txRptr = 0;
  ei->txWptr = 0;
  ei->txPending = 0;

  /* Set the DMA RX offset to 1 and Turn on DMA */
  mac->dma_control |=
    DMA_TX_DMA_EN | DMA_RX_DMA_EN | (RXBUF_OFFSET << DMA_RX_OFFSET_SHIFT);
  
  return 0;
}


/********
 * ec_put	 Enqueue a packet for transmission
 *-------
 * We copy 14 bytes of the mbuf to the TX fifo entry and the remainder
 * to the TX buffer.  This simplifies the accounting.
 *
 * Note: we have more fifo entries than we have buffers.  txPending counts
 * the number of packet's enqueued here, and it's decremented by ec_poll when
 * it determines that the packet has been sent.  We stall here until
 * txPending is less than BUFCNT. Since BUFCNT * 2^n == TXFIFOSIZE, and because
 * we never have more than BUFCNT messages queued, the wptr % BUFCNT can
 * be used to map tx_ring_wptr to a txBuf entry.
 *
 * Note: Why don't we just dma from the mbufs?  RX buffers must be
 * 4096 aligned and  TX buffers musn't cross 4096 boundries.  Mbufs don't
 * currently meet these requirements.  Fixing the mbuf alignment problems
 * is possible but seemed more complex than using a separate ethernet buffer
 * pool and simply copying the data to/from the separate buffers.  The 
 * performance hit seems minor for standalone purposes.
 */

static int
ec_txdone(void)
{
  volatile TXfifo *f = &ei->txFifo[ei->txRptr % TXFIFOSIZE];
  __uint64_t status;
  int length, prev = 0;

  /* Ring empty? */
  if (ei->txRptr == ei->txWptr) {
    /* If the hardware is out of sync, reset (harmless) */
    if (ei->mac->tx_ring_rptr != ei->mac->tx_ring_wptr)
      ec_init();
    return 0;
  }

  /* Packet done yet? */
  if (((status = f->TXStatus) & TX_VEC_FINISHED) == 0) {
    /* Transmitter stuck? */
    if ((GetRelativeTime() - ei->activity) > 5)
      ec_init();
    return 0;
  }

  /* Debug */
  if (Debug > 1) printf("ec0: xmt status 0x%016llx\n", f->TXStatus);

  /* Report errors? */
  if ((status & TX_VEC_COMPLETED_SUCCESSFULLY) == 0) {
#define TXERRMSG(msg) { \
    if (prev) { VPRINTF(","); } \
    VPRINTF(msg); \
    prev = 1; \
}
    length = status & TX_VEC_LENGTH;
    VPRINTF("ec0: tx error: length=%d flags=<", length);
    if (status & TX_VEC_ABORTED_TOO_LONG)
	TXERRMSG("giant")
    if (status & TX_VEC_ABORTED_UNDERRUN)
	TXERRMSG("underrun")
    if (status & TX_VEC_DROPPED_COLLISIONS)
	TXERRMSG("excess collisions")
    if (status & TX_VEC_CANCELED_DEFERRAL)
	TXERRMSG("excess deferral")
    if (status & TX_VEC_DROPPED_LATE_COLLISION)
	TXERRMSG("late collision")
    VPRINTF(">\n");
  }

  /* Could add error statistics checks here */
  ei->activity = GetRelativeTime();
  f->TXStatus = DEADPACKET;
  ADVANCE(ei->txRptr, TXFIFOSIZE);
  ei->txPending--;
  assert(ei->txPending>=0);

  return 1;
}

static int
ec_put(struct mbuf* m){
  volatile TXbuf   *txbuf = ei->txBufs[ei->txWptr % BUFCNT];
  volatile TXfifo  *f = &ei->txFifo[ei->txWptr];
  register int	   length, offset;

  /* Free TX slot? */
  if (ei->txPending >= BUFCNT) {
    _m_freem(m);
    return -1;
  }
  
  /* For small packets, just use the TX ring in KSEG1 */
  if (m->m_len < (sizeof(TXfifo) - sizeof(long long))) {
    /* Minimum offset is MIN_TXPKT_LEN bytes */
    length = m->m_len;
    if (length < MIN_TXPKT_LEN)
	length = MIN_TXPKT_LEN;
    offset = sizeof (TXfifo) - length;

    /* Set up the fifo block */
    f->TXCmd = (offset << TX_CMD_OFFSET_SHIFT) | (length - 1); 

    /* Copy bytes from mbuf to TXfifo entry */
    bcopy(mtod(m, const void *), (void *)&f->buf[offset], m->m_len);

  } else {
    /* Set up the fifo block */
    f->TXCmd =
      TX_CMD_CONCAT_1 |
	((sizeof(TXfifo)-ETHERHDR) << TX_CMD_OFFSET_SHIFT) |
	  (m->m_len - 1); 

    /* Copy 14 bytes from mbuf to TXfifo entry */
    bcopy(mtod(m,const void *),
	  (void *)&f->buf[sizeof(TXfifo) - ETHERHDR], ETHERHDR);
  
    /* Copy the rest to the buffer */
    m->m_len -= ETHERHDR;
    m->m_off += ETHERHDR;
    bcopy(mtod(m,const void *), (void *)txbuf, m->m_len);
    f->TXConcatPtr[1] = ((long long)(m->m_len - 1) << 32) | K1_TO_PHYS(txbuf);

  }
  if (Debug > 1) printf("ec0: put cmd 0x%016llx\n", f->TXCmd);
  wbflush();

  /* Free the enqueued mbuf */
  _m_freem(m);

  /* Update activity time */
  if (ei->txPending == 0)
    ei->activity = GetRelativeTime();

  /* Increment ring pointer */
  ADVANCE(ei->txWptr, TXFIFOSIZE);

  /* Update the enqueued message count */
  ei->txPending++;
  assert(ei->txPending > 0 && ei->txPending <= BUFCNT);

  /* Tell the HW about the new message (paranoid read-back) */
  ei->mac->tx_ring_wptr = ei->txWptr;
  while (ei->mac->tx_ring_wptr != ei->txWptr)
    ei->mac->tx_ring_wptr = ei->txWptr;

  return 0;
}


/***********
 * ec_output	Process a packet for transmit
 */
static int
ec_output(struct ifnet* ifn, struct mbuf* m, struct sockaddr* sa){
  ETHADDR  edst;
  struct   in_addr idst;
  struct   ether_header *eh;
  short    type;
  

  /* Construct the ethernet header */
  switch(sa->sa_family){
  case AF_INET:
    idst = ((struct sockaddr_in*)sa)->sin_addr;
    _arpresolve(&ei->ei_ac, &idst, (u_char*)&edst);
    type = ETHERTYPE_IP;
    break;
  case AF_UNSPEC:
    eh = (struct ether_header*)sa->sa_data;
    bcopy(eh->ether_dhost, &edst, sizeof edst);
    type = eh->ether_type;
    break;
  default:
    _m_freem(m);
    return(-1);
  }

  /* Move the header to the mbuf */
  assert(m->m_off >= sizeof *eh);
  if (m->m_off < sizeof *eh){
    _m_freem(m);
    return(-1);
  }
  m->m_off -= sizeof *eh;
  m->m_len += sizeof *eh;
  eh = mtod(m, struct ether_header *);
  eh->ether_type = htons(type);
  bcopy(&edst, eh->ether_dhost, sizeof edst);
  bcopy(ei->ei_addr, eh->ether_shost, sizeof(ETHADDR));
  
  /* Enqueue the packet */
  return ec_put(m);
}


/**********
 * ec_ioctl
 */
static int
ec_ioctl(IOBLOCK* iob){
  if (Debug) printf("ec0: ec_ioctl(iob=0x%x) cmd=0x%x\n", iob, iob->IoctlCmd);
  switch((__psint_t)(iob->IoctlCmd)){
  case NIOCRESET:
    ec_reset();
    return 0;
  default:
    return iob->ErrorNumber = EINVAL;
  }
}

/************
 * ec_forward	Dispatch received mbuf
 */
static void
ec_forward(struct mbuf* m){
  struct ether_header *eh = mtod(m,struct ether_header*);

  m->m_off += sizeof *eh;
  m->m_len -= sizeof *eh;
  
  switch(ntohs(eh->ether_type)){
  case ETHERTYPE_IP:
    _ip_input(&ei->ei_if,m);
    break;
  case ETHERTYPE_ARP:
    _arpinput(&ei->ei_ac,m);
    break;
  default:
    _m_freem(m);
  }
}


/************
 * ec_receive	Process a received buffer and refill receive queue.
 */
static int
ec_receive(void){
  volatile RXbuf *rxbuf = ei->rxBufs[ei->rxRptr % BUFCNT];
  register __uint64_t status;
  struct mbuf *m = 0;
  int length, prev = 0;

  /* Get status */
  if (((status = rxbuf->RXStatus) & RX_VEC_FINISHED) == 0)
    return 0;

  /* Get packet length from status vector */
  length = status & RX_VEC_LENGTH;
  if (length > ETHERMAXLEN)
	length = ETHERMAXLEN;
  if (length < ETHERMINLEN)
	length = ETHERMINLEN;
  length -= CRCLEN;

  /* Debug */
  if (Debug > 1) printf("ec0: rcv status 0x%016llx\n", rxbuf->RXStatus);

  /* If received packet not marked bad, process it */
  if ((status & RX_VEC_BAD_PACKET) == 0) {
    m = _m_get(M_DONTWAIT, MT_DATA);

    /* We might not be able to get a mbuf.  If we do, copy the data to it */
    if (m) {
      m->m_len = length;
      m->m_off = DMA_RX_BOFFSET;
/*    __r5000_cdx(mtod(m, char *), m->m_len);	*/
      bcopy((char *)rxbuf + DMA_RX_PAD, mtod(m, char *), m->m_len);
    }
  } else {
#define RXERRMSG(msg) { \
    if (prev) { VPRINTF(","); } \
    VPRINTF(msg); \
    prev = 1; \
}
    VPRINTF("ec0: rx error: length=%d flags=<", length);
    if (status & RX_VEC_CODE_VIOLATION)
	RXERRMSG("symbol")
    if (status & RX_VEC_DRIBBLE_NIBBLE)
	RXERRMSG("dribble")
    if (status & RX_VEC_CRC_ERROR)
	RXERRMSG("crc")
    if (status & RX_VEC_INVALID_PREAMBLE)
	RXERRMSG("preamble")
    if (status & RX_VEC_LONG_EVENT)
	RXERRMSG("giant")
    if (status & RX_VEC_CARRIER_EVENT)
	RXERRMSG("carrier")
    VPRINTF(">\n");
  }

  /* Prepare and enqueue the receive buffer again */
  rxbuf->RXStatus = 0;
  ei->mac->rx_fifo = K1_TO_PHYS(rxbuf);
  ADVANCE(ei->rxRptr, RXFIFOSIZE);
  
  /* Forward the received mbuf */
  if (m)
    ec_forward(m);

  return 1;
}


/*********
 * ec_poll
 */
static void
ec_poll(IOBLOCK* iob){
  static in_poll = 0;

  /* Don't recurse */
  if (in_poll)
    return;
  in_poll = 1;

  /* Scan for transmitted packets */
  while (ec_txdone());

  /* Scan for received packets */
  while (ec_receive());

  /* Safe */
  in_poll = 0;

  return;
}


/**************
 * mace_ec_open		Setup unit and initialize structures
 */
int
mace_ec_open(IOBLOCK* iob){

  if (Debug) printf("ec0: mace_ec_open(0x%08x)\n", iob);

  /* Not bound yet */
  cei(iob)->ei_registry = -1;
  cei(iob)->ei_acp = &ei->ei_ac;

  /* Should not be opening if already open */
  if (ei->ei_if.if_flags & EC_ACTIVE) {
    ei->ref++;
    return ESUCCESS;
  }

  /*
   * Get eaddr.
   * Try to check that it is a valid ethernet address.
   */
  cpu_get_eaddr(ei->ei_addr);
  assert(ei->ei_addr[0] == SGI_ETHER_ADDR);

  /* Initialize the hardware.  */
  if (ec_init() < 0)
    return iob->ErrorNumber = EIO;

  /* Fill in the various fields in the interface structure */
  ei->ei_if.if_unit   = 0;
  ei->ei_if.if_mtu    = ETHERMTU;
  ei->ei_if.if_output = ec_output;
  ei->ei_if.if_ioctl  = ec_ioctl;
  ei->ei_if.if_poll   = ec_poll;

  ei->ei_if.if_flags = EC_ACTIVE;
  ei->ref++;

  return ESUCCESS;
}


/***************
 * mace_ec_close	Close the ethernet interface.
 */
void
mace_ec_close(void){
  int i;
  ULONG mark = GetRelativeTime();

  /*
   * Close on last reference
   */
  if (--ei->ref != 0)
    return;

  /*
   * Stop all receives.  Doing this allows us to call ec_poll to "drain"
   * the queues.  If receives were still active, the received buffers could
   * continue to stimulate new transmits (Unlikely but possible.)
   */
  ei->mac->dma_control &= ~DMA_RX_DMA_EN;

  /*
   * Wait for the transmit queue to drain, then reset the interface.
   * Don't wait more than 5 seconds for the transmits to finish.
   * When transmit queue drained, stop everything.
   */
  while (ei->txRptr != ei->mac->tx_ring_wptr){
    if ((GetRelativeTime()-mark)>5)
      break;
    ec_poll(0);
  }
  ec_reset();

  /* Mark the interface inactive */
  ei->ei_if.if_flags &= ~EC_ACTIVE;

  if (Debug) printf("ec0: mace_ec_close()\n");
}


/**************
 * mace_ec_init		Clean up global state
 */
int
mace_ec_init(void) {

  if (Debug) printf("ec0: mace_ec_init()\n");

  /* Reset the SW and HW state */
  ei = &eh_info;
  bzero(ei,sizeof(EhInfo));
  ei->mac = (volatile Mac110 *)MACE_ETHER_ADDRESS;
  ec_reset();

  return 0;
}


/***************
 * mace_ec_probe	Probe for a unit.  (Unit 0 exists always, no others).
 */
int
mace_ec_probe(int unit) {
  return unit == 0 ? 1 : -1;
}


/*****************************************************************************
 *                      Mace ethernet configuration                          *
 *****************************************************************************/

/* ARCS configuration data templates */

static COMPONENT ectmpl = {
	ControllerClass,		/* Class */
	NetworkController,		/* Type */
	0,				/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	sizeof(struct net_data),	/* ConfigurationDataSize */
	21,				/* IdentifierLength */
	"110Base-TX revision 0"		/* Identifier */
};

static COMPONENT ecptmpl = {
	PeripheralClass,		/* Class */
	NetworkPeripheral,		/* Type */
	Input|Output,			/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	0,				/* IdentiferLength */
	NULL				/* Identifier */
};

/*****************
 * setDmaSpecifics
 * ---------------
 * set dma version
 */
static void
setDmaSpecifics(void)
{
  register volatile Mac110 *mac;
  char *revn = "0123456789ABCDEF";

  mac = (volatile Mac110 *)MACE_ETHER_ADDRESS;
  ec_mrev = mac->mac_control >> MAC_REV_SHIFT; 
  ectmpl.Identifier[ectmpl.IdentifierLength - 1] = revn[ec_mrev + 1];
}

/************
 * getPhyInfo
 * ----------
 * probe for PHY type at install time
 */
static int
getPhyInfo(void)
{
  register volatile Mac110 *mac;
  register int found;

  mac = (volatile Mac110 *)MACE_ETHER_ADDRESS;
  mac->mac_control = 0;
  found = ec_mdio_probe(mac);
  mac->mac_control = MAC_RESET;

  return found;
}

/*****************
 * setPhySpecifics
 * ---------------
 * set phy type for hinv
 */
static void
setPhySpecifics(int ptype)
{
  char *pname, *revn = "0123456789ABCDEF";

  switch(ptype) {
    case PHY_QS6612X:
      pname = "QS6612X-0";
      break;
    case PHY_ICS1889:
      pname = "ICS1889-0";
      break;
    case PHY_ICS1890:
      pname = "ICS1890-0";
      break;
    case PHY_DP83840:
      pname = "DP83840-0";
      break;
    default:
      pname = "phy????-0";
      sprintf(pname, "%07x-0", ec_ptype);
      break;
  }
  ecptmpl.Identifier = pname;
  ecptmpl.IdentifierLength = strlen(pname)+1;
  pname[8] = revn[ec_prev];
}

/*****************
 * setNetSpecifics
 * ---------------
 * Fill in net_data structure
 * ---------------
 * And yet another bug was uncovered,
 * The vendor name was missing, and this will leave whatever
 * in the memory as its value which is very bad ...
 */
static void
setNetSpecifics(struct net_data* n)
{
  n->Version     = SGI_ARCS_VERS;
  n->Revision    = SGI_ARCS_REV;
  n->Type        = NET_TYPE;
  n->Vendor      = "SGI-MH";
  n->ProductName = "MACE-EC";
  n->SerialNumber= 0;

  strcpy((char *)n->NetType,"Ethernet");
  n->NetMaxlength= ETHERMTU;
  n->NetAddressSize= 6;
  cpu_get_eaddr(n->NetAddress);

  n->NetMulticastCount= 0;
  n->NetMulticast = (MULTICAST*)NULL;
  n->NetFunctional[0] = 0;

  return ;
}

/*****************
 * mace_ec_install
 * ---------------
 * Add to configuration
 */
void
mace_ec_install(COMPONENT* p)
{
  struct net_data n;
  COMPONENT* net;
  COMPONENT* periph;
  int found;

  if (Debug) printf("ec0: mace_ec_install(0x%x)\n", p);

  /*
   * Add the MACE fast-ethernet controller to system inventory.
   */
  found = getPhyInfo();
  setDmaSpecifics();
  setNetSpecifics(&n);
  net = AddChild(p, &ectmpl, &n);
  assert(net);

  /*
   * If we found the transceiver, add a peripheral structure to
   * the configuration for it and register the strategy routine.
   */
  if (found > 0) {
    periph = AddChild(net, &ecptmpl, 0);
    assert(periph);
    RegisterDriverStrategy(periph, _if_strat);
  }

  return ;
}

static int
mace_ec_regs(void)
{
  register volatile Mac110 *mac = (volatile Mac110 *)MACE_ETHER_ADDRESS;

  return 0;
}

static int
mace_ec_mclfifo(void)
{
  register volatile Mac110 *mac = (volatile Mac110 *)MACE_ETHER_ADDRESS;
  register int i;

  for (i = 0; i < 16; ++i) {
    mac->rx_fifo = (0x55555 ^ (i * 23)) << 12;
    if (mac->rx_fifo_wptr != (i + 1))
      return -1;
  }
  for (i = 0; i < 16; ++i) {
    if (mac->rx_fifo_rptr != i)
      return -1;
    if (mac->rx_fifo != ((0x55555 ^ (i * 47)) << 12))
      return -1;
  }
  if (mac->rx_fifo_rptr != 16)
    return -1;

  return 0;
}

static int
mace_ec_loop(void)
{
  register volatile Mac110 *mac = (volatile Mac110 *)MACE_ETHER_ADDRESS;

  return 0;
}

/*****************
 * mace_ec_test
 * ---------------
 * PON hardware tests
 */
int
mace_ec_test(void)
{
  register volatile Mac110 *mac = (volatile Mac110 *)MACE_ETHER_ADDRESS;
  int errors = 0;

  if (Debug) printf("ec0: mace_ec_test()\n");

  /* Take ethernet out of reset */
  mac->mac_control = 0;
   
  /* Do register tests */
  if (mace_ec_regs() < 0)
	errors |= 0x1;

  /* Pattern check MCL fifo */
  if (mace_ec_mclfifo() < 0)
	errors |= 0x2;

  /* Probe for the transceiver */
  if (ec_mdio_probe(mac) < 0)
	errors |= 0x4;

  /* Internal DMA loopback? */
  if (mace_ec_loop() < 0)
	errors |= 0x8;

  /* Go back into reset state */
  mac->mac_control = MAC_RESET;

  return errors;
}
