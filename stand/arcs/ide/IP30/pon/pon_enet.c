/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/* 
 *   This file contains the Ethernet PON diag, which was ported from
 *   stand/arcs/lib/libsk/ml/diag_enet.c; only ioc3 internal loopback
 *   code was ported.
 * */

#ident  "$Revision: 1.10 $"

#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include <uif.h>
#include <setjmp.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/mii.h>

#define SPEED_100		1
#define IP_CHECKSUM_MASK	0x0000ffff
#define RSV22_MASK		0x80000000

#define NORMAL_NUM_PACKETS	10
#define RING			128
#define NORMAL_PACKET_LEN	301   /* Intentionally odd # */
 
#define _4K_			(4*1024)
#define _16K_			(16*1024)
#define ROUND64(x,size)		(((__uint64_t)(x) & ((__uint64_t)((size)-1))) ? (((__uint64_t)(x)+(__uint64_t)(size)) & ~((__uint64_t)((size)-1))):(__uint64_t)(x))
#define HI_WORD(x)		((__uint32_t)((x) >> 32))
#define LO_WORD(x)		((__uint32_t)((x)))
#define KVTOIOADDR(addr)	kv_to_bridge32_dirmap((void *)BRIDGE_K1PTR,(__psunsigned_t)(addr))

typedef struct etx_descriptor_s {
  /* Command field */
  volatile __uint32_t reserved_6:      5;  /* 31:27 */
  volatile __uint32_t chk_offset:      7;  /* 26:20 */
  volatile __uint32_t do_checksum:     1;  /*    19 */
  volatile __uint32_t buf_2_valid:     1;  /*    18 */
  volatile __uint32_t buf_1_valid:     1;  /*    17 */
  volatile __uint32_t data_0_valid:    1;  /*    16 */
  volatile __uint32_t reserved_5:      3;  /* 15:13 */
  volatile __uint32_t int_when_done:   1;  /*    12 */
  volatile __uint32_t reserved_4:      1;  /*    11 */
  volatile __uint32_t byte_cnt:       11;  /* 10:00 */
  /* Buffer count field */
  volatile __uint32_t reserved_3:      1;  /*    31 */
  volatile __uint32_t buf_2_cnt:      11;  /* 30:20 */
  volatile __uint32_t reserved_2:      1;  /*    19 */
  volatile __uint32_t buf_1_cnt:      11;  /* 18:08 */
  volatile __uint32_t reserved_1:      1;  /*     7 */
  volatile __uint32_t data_0_cnt:      7;  /*  6:00 */
  /* BUF_1 pointer field */
  volatile __uint64_t buf_ptr_1;
  /* BUF_2 pointer field */
  volatile __uint64_t buf_ptr_2;
  /* DATA_0 block */
  volatile uchar_t data0_byte[104];
} etx_descriptor_t;

typedef struct etx_ring_s {
  volatile etx_descriptor_t etx_descriptor[128];
} etx_ring_t;

typedef struct erx_buffer_s {
  /* First word */
  volatile __uint32_t valid:           1;  /*    31 */
  volatile __uint32_t reserved_3:      4;  /* 30:27 */
  volatile __uint32_t byte_cnt:       11;  /* 26:16 */
  volatile __uint32_t ip_checksum:    16;  /* 15:00 */
  /* Status field */
  volatile __uint32_t rsv22:           1;  /*    31 */
  volatile __uint32_t rsv21:           1;  /*    30 */
  volatile __uint32_t rsv20:           1;  /*    29 */
  volatile __uint32_t rsv19:           1;  /*    28 */
  volatile __uint32_t rsv17:           1;  /*    27 */
  volatile __uint32_t rsv16:           1;  /*    26 */
  volatile __uint32_t rsv12_3:        10;  /* 25:16 */
  volatile __uint32_t reserved_2:      1;  /*    15 */
  volatile __uint32_t rsv2_0:          3;  /* 14:12 */
  volatile __uint32_t reserved_1:      8;  /* 11:04 */
  volatile __uint32_t inv_preamble:    1;  /*     3 */
  volatile __uint32_t code_err:        1;  /*     2 */
  volatile __uint32_t frame_err:       1;  /*     1 */
  volatile __uint32_t crc_err:         1;  /*     0 */
  /* Pad, data, and unused bytes */
  volatile uchar_t pdu[1656];
} erx_buffer_t;

typedef struct erx_ring_s {
  volatile __uint64_t erx_buffer_pointer[128];
} erx_ring_t;

/* Static variables */
static int        num_packets, garbage_index;
static __uint64_t mac_address = 0xfefefefefefe;  /* Fake value used */ 
static __uint64_t *tx_ring_malloc_ptr;
static __uint64_t *tx_buf1_malloc_ptr[RING];
static __uint64_t *rx_ring_malloc_ptr;
static __uint64_t *rx_buf_malloc_ptr[RING];
static __uint64_t *garbage_pointers[RING];
static ioc3_mem_t *ioc3_bp;
static volatile __psunsigned_t ioc3_base;
static volatile ioc3reg_t *ioc3_sio_cr;

static int diag_loop_cleanup(void);

void
pon_us_delay(int timeout)
{
  register heart_piu_t    *heart_piu = HEART_PIU_K1PTR;
  register heartreg_t     term_count;

  term_count = heart_piu->h_count + timeout*(1000/HEART_COUNT_NSECS);

  while (heart_piu->h_count < term_count)
	  ;
}


/* DMA loopback test */ 
int
pon_enet(void)
{
  static char *testname = "IOC3 Ethernet internal loopback";
  int                     ii, packet, packet_len, dw_index, size;
  int                     shift_amt, byte, remaining_bytes, speed;
  int                     try_again, timeout, start_timeout; 
  __uint64_t              rx_buf_pointer[RING];
  __uint64_t              tx_buf1_pointer[RING];
  __uint64_t              tx_ring_base;
  __uint64_t              bigend_mac_address;
  __uint64_t              rx_ring_base;
  __uint64_t              cmd_buf_cnt;
  __uint32_t              rx_off;
  erx_buffer_t            scratch_rx_buf;
  erx_buffer_t            *actual;
  erx_buffer_t            *expected;

  msg_printf(VRB,"%s test.\n",testname);

  garbage_index = 0;
  
  /* Get IOC3 base pointer */
  ioc3_base = PHYS_TO_K1(IOC3_PCI_DEVIO_BASE); 
  ioc3_bp = (ioc3_mem_t *) ioc3_base;

  /* initialize IOC3 */
  *(volatile ioc3reg_t *)(IOC3_PCI_CFG_K1PTR + IOC3_PCI_SCR) |=
  	PCI_CMD_BUS_MASTER | PCI_CMD_PAR_ERR_RESP | PCI_CMD_SERR_ENABLE;

  /* reset the DMA interface to get back into poll mode */
  *(volatile ioc3reg_t *)(ioc3_base + IOC3_SSCR_A) = SSCR_RESET;
  *(volatile ioc3reg_t *)(ioc3_base + IOC3_SSCR_B) = SSCR_RESET;

  /* poll for DMA interface to idle */
  ioc3_sio_cr = (volatile ioc3reg_t *)(ioc3_base + IOC3_SIO_CR);
  while ((*ioc3_sio_cr & SIO_CR_ARB_DIAG_IDLE) == 0x0)
                ;

  /* unreset the serial ports */
  *(volatile ioc3reg_t *)(ioc3_base + IOC3_SSCR_A) = 0x0;
  *(volatile ioc3reg_t *)(ioc3_base + IOC3_SSCR_B) = 0x0;

  /* set command pulse width for serial port */
  *ioc3_sio_cr = 0x4 << SIO_CR_CMD_PULSE_SHIFT;

  /*
  * enable generic trisate pins for output
  * pins 7/6: serial ports transceiver select, 0=RS232, 1=RS422
  * pin 5: PHY reset, 0=reset, 1=unreset
  */

  *(volatile ioc3reg_t *)(IOC3_PCI_CFG_K1PTR + IOC3_PCI_LAT) = 40 << 8;
  *(volatile ioc3reg_t *)(ioc3_base + IOC3_GPCR_S) =
                        (0x1 << 7) | (0x1 << 6) | (0x1 << 5);
  *(volatile ioc3reg_t *)(ioc3_base + IOC3_GPPR(7)) = 0x0;
  *(volatile ioc3reg_t *)(ioc3_base + IOC3_GPPR(6)) = 0x0;
  *(volatile ioc3reg_t *)(ioc3_base + IOC3_GPPR(5)) = 0x1;


  /* number of packets  = 10 length of each packet = 301 */
  num_packets = NORMAL_NUM_PACKETS;
  packet_len = NORMAL_PACKET_LEN; 

  /* Allocate memory for the TX descriptor ring.  We use a small 
     ring size (128 tx descriptors) in pon_enet.  We need to
     over-allocate by 16K in order to assure ring alignment. */

  size = sizeof(etx_ring_t) + _16K_;

  if (!(tx_ring_malloc_ptr = 
        (__uint64_t *)align_malloc(size, MIN(size,ETBR_ALIGNMENT))) ) {
    goto memfailure;
  }

  tx_ring_base = (__uint64_t)tx_ring_malloc_ptr;

  /* Write the base address to the ETBR_H and ETBR_L registers.
     All other attribute bits are set to zero */

  ioc3_bp->eregs.etbr_h  = (__uint32_t)((KVTOIOADDR(tx_ring_base) & 0xffffffff00000000LL) >> 32);
  ioc3_bp->eregs.etbr_l  = (__uint32_t)(KVTOIOADDR(tx_ring_base) & 0xffffffff);
  
  
  /* Calculate TX descriptor Command and Buffer Count fields.  
     They are the same for all packets: 
     CHK_OFFSET    = 0   
     DO_CHECKSUM   = 0  
     BUF_2_VALID   = 0 
     BUF_1_VALID   = 1
     DATA_0_VALID  = 0
     INT_WHEN_DONE = 0
     BYTE_CNT      = packet_len
     DATA_0_CNT    = 0
     BUF_1_CNT     = packet_len
     BUF_1_CNT     = 0
  */
  cmd_buf_cnt = ((__uint64_t) 1 << 49) | /* BUF_1_VALID */
      (__uint64_t) packet_len << 32 | /* BYTE_CNT    */ 
          (__uint64_t) packet_len << 8; /* BUF_1_CNT   */

  /* Allocate TX buffers and initialize the TX descriptor ring entries */
  for (packet = 0; packet < num_packets; packet++) {
    
    /* Allocate memory for the TX buffer.  For simplicity, all of the
       data will be in a BUF_1 area (i.e. no DATA_0 or BUF_2).  The
       hardware only requires that BUF_1 buffers be halfword aligned,
       but we force doubleword alignment so we can do doubleword writes
       (overallocate and round up).  The hardware DOES require that the
       buffer not cross a 16K boundary, so we test for this case, and do
       another malloc if the first try did cross a 16K boundary. */
    do {
      
      if (!(tx_buf1_malloc_ptr[packet] = (__uint64_t *) malloc(packet_len + 8))) {
      	goto memfailure;
      }
      
      tx_buf1_pointer[packet] = ROUND64(tx_buf1_malloc_ptr[packet], 8);

      /* Test for 16K crossing */
      if ((tx_buf1_pointer[packet] & 0x3fff) + packet_len > 0x4000) {

	if (garbage_index >= RING - 1) {
	  msg_printf(ERR, "pon_enet: Too many garbage pointers (TX packet %d).\n", packet); 
	  return(-1);
	}
	
	garbage_pointers[garbage_index++] = tx_buf1_malloc_ptr[packet];
        try_again = 1;
      }
      
      else try_again = 0;
      
    } while (try_again);

    /* Write the first DW (the Command and Buffer Count fields) */
    (*((__uint64_t *)tx_ring_base + packet * 16)) = cmd_buf_cnt;
    
    /* Write the BUF_PTR_1 field. All other attribute bits are set to 0. */
    (*((__uint64_t *)tx_ring_base + packet * 16 + 1)) =
        KVTOIOADDR(tx_buf1_pointer[packet]);

  }
  
  /* Generate DMA data, and fill TX buffers. 
   * 
   * The first 6 bytes of data are the Target MAC address, and the next
   * 6 bytes are the Source MAC address.  Both addresses are the same
   * since we are doing loopback.  If the NIC test passed, we use the
   * MAC address from the NIC.  If the NIC test failed, we will use a
   * fabricated MAC address.  This should never make its way onto a real
   * network, but its value is chosen such that there is virtually no
   * chance of its being the address of another legitimate station if it
   * did.  The value is:
   * 
   *    0  1  2  3  4  5  
   * +-------------------+
   * | FE FE FE FE FE FE |  Fabricated MAC address (if NIC test fails)
   * +-------------------+
   * 
   * The MAC address returned by nic_get_eaddr is little endian as are
   * the addresses in the packet.  The value written to the EMAR_H and
   * EMAR_L registers are big endian.
   * 
   * The target and source MAC addresses take up the first 12 bytes of
   * the packet.  The 13th byte is the loopback type code (0 = IOC3).
   * The 14th byte is the speed (0 = 10Mb/s, 1 = 100 Mb/s).
   * The 15th and 16th bytes are the packet number.
   */
  

  for (packet = 0; packet < num_packets; packet++) {
    
    /* First DW */
    dw_index = 0;
    
    (*((__uint64_t *)tx_buf1_pointer[packet] + dw_index)) = 
        (mac_address << 16) | 
            (mac_address >> 32);
     
    /* Second DW (filled later - see while loop below) */
    /* Remaining DWs */
    dw_index = 2;

      /* Generate PCI stress pattern  Since this part of the packet is
	 identical for all PCI stress packets, I should just calculate the
	 contents once, stick it in an array, and then copy the array into
	 the buffer.  This would speed up checking too.  */
      shift_amt = 32;
    
      while (dw_index < packet_len/8) {

	(*((__uint64_t *)tx_buf1_pointer[packet] + dw_index)) = 
	    0xffffffff00000000 & 
		~(((__uint64_t) 1 << (shift_amt + 32))) |
		    (((__uint64_t) 1 << (shift_amt)));
        
	shift_amt = shift_amt ? --shift_amt : 32; /* 32->0, 32->0,  ... */

	dw_index++;
      }
    
      /* Fill the remaining partial DW with bytes of 0x5a */
      remaining_bytes = packet_len - dw_index*8;
    
      for (byte = 0; byte < remaining_bytes; byte++) {
      
	(*((uchar_t *)tx_buf1_pointer[packet] + dw_index*8 + byte)) = 0x5a;
      }
  } 

  /* Allocate memory for RX buffer pointer ring. */
  size = sizeof (erx_ring_t) + _4K_;
  if (!(rx_ring_malloc_ptr = (__uint64_t *)align_malloc(size, ERBR_ALIGNMENT))) {
    goto memfailure;
  } 
    

  rx_ring_base = (__uint64_t)rx_ring_malloc_ptr;

  /* Write the base address to the ERBR_H and ERBR_L registers.
     All other attribute bits are set to zero */
  ioc3_bp->eregs.erbr_h  = (__uint32_t)((KVTOIOADDR(rx_ring_base) & 0xffffffff00000000LL) >> 32);
  ioc3_bp->eregs.erbr_l  = (__uint32_t)(KVTOIOADDR(rx_ring_base) & 0xffffffff);
  

  for (packet = 0; packet < num_packets; packet++) {
    
    /* Allocate memory for RX buffers.  Buffers are 1664 bytes, but must
       be 128-byte aligned, so we overallocate and round up.  The
       hardware also requires that the buffer not cross a 16K boundary,
       so we test for this case, and do another malloc if the first try
       did cross a 16K boundary. */
    do {
      
      if (!(rx_buf_malloc_ptr[packet] = 
            (__uint64_t *) malloc(sizeof (erx_buffer_t) + 128))) {
        goto memfailure;
      }
      
      rx_buf_pointer[packet] = ROUND64(rx_buf_malloc_ptr[packet], 128); /* Round up to 128B boundary */

      /* Test for 16K crossing */
      if ((rx_buf_pointer[packet] & 0x3fff) + sizeof (erx_buffer_t) > 0x4000) {
        
	if (garbage_index >= RING - 1) {
	  msg_printf(ERR, "pon_enet: Too many garbage pointers (RX packet %d).\n", packet); 
	  goto failure;
	}
	
	garbage_pointers[garbage_index++] = rx_buf_malloc_ptr[packet];
        try_again = 1;
      }
      
      else try_again = 0;
      
    } while (try_again);
    
    /* Write the ring entry. All other attribute bits are set to 0. */
    (*((__uint64_t *)rx_ring_base + packet)) = KVTOIOADDR(rx_buf_pointer[packet]);
    
  }


  /* For IOC3 internal loopback, we only want to test 100Mb/s operation.
  */
  speed = SPEED_100; 
  
  for (packet = 0; packet < num_packets; packet++) {
    /* Write to second TX BUF1 DW.  This was deferred until now so we
       can indicate the speed. */
    dw_index = 1;

    (*((__uint64_t *)tx_buf1_pointer[packet] + dw_index)) = 
        mac_address << 32 | speed << 16 | packet;
      
    /* Clear the first DW in each RX buffer to clear valid bit. */
    (*(__uint64_t *)rx_buf_pointer[packet]) = 0;
  }

  /* Reset IOC3 Ethernet  */
  ioc3_bp->eregs.emcr = EMCR_RST;

  /* The ICS1892 PHY chip takes about 110-200 milliseconds to reset.  This
   * is longer than the ICS1890.  Because of this, garbage may still be in
   * in some of the registers while its resetting and cause us to bomb out
   * below like when checking that only TX_EMPTY has been set in the EISR.
   * So we take the worst case of 200 milliseconds and double it to be on 
   * the safe/paranoid side.
   */
  pon_us_delay(400000);          

  timeout = 10;

  while (!(ioc3_bp->eregs.emcr & EMCR_ARB_DIAG_IDLE)) {
    pon_us_delay(1000);         /* 1 ms */
    if (timeout-- <= 0) {
      msg_printf(ERR, "pon_enet: Timed out on EMCR_ARB_DIAG_IDLE trying to reset enet.\n");
      goto failure;
    }
  }
  
  ioc3_bp->eregs.emcr  = 0x0;
  ioc3_bp->eregs.etcir = 0x0;
  ioc3_bp->eregs.etpir = 0x0;
  ioc3_bp->eregs.ercir = 0x0;
  ioc3_bp->eregs.erpir = 0x0;

  /* Set appropriate EMCR bits */
  rx_off = 0x4 << EMCR_RXOFF_SHIFT; /* No pad bytes, for simplicity */
  ioc3_bp->eregs.emcr  = EMCR_DUPLEX | EMCR_PADEN | 
	rx_off | EMCR_RAMPAR | EMCR_BUFSIZ;
    
  /* Initialize ERCSR */
  ioc3_bp->eregs.ercsr  = ERCSR_THRESH_MASK; /* Max thresh value */
  
  /* Initialize ERBAR */
  ioc3_bp->eregs.erbar  = 0;
  
  /* Write big endian MAC address to EMAR_L and EMAR_H */
  bigend_mac_address =  (mac_address & 0x00000000000000ff) << 40;
  bigend_mac_address |= (mac_address & 0x000000000000ff00) << 24;
  bigend_mac_address |= (mac_address & 0x0000000000ff0000) <<  8;
  bigend_mac_address |= (mac_address & 0x00000000ff000000) >>  8;
  bigend_mac_address |= (mac_address & 0x000000ff00000000) >> 24;
  bigend_mac_address |= (mac_address & 0x0000ff0000000000) >> 40;
  ioc3_bp->eregs.emar_l  = LO_WORD(bigend_mac_address);
  ioc3_bp->eregs.emar_h  = HI_WORD(bigend_mac_address);

  /* Enable TX_EMPTY interrupt */
  ioc3_bp->eregs.eier  = EISR_TXEMPTY;
    
  /* Initialize ETCSR for full duplex.  Values from kern/master.d/if_ef */
  ioc3_bp->eregs.etcsr  =  21 |    /* IPGT  = 21 */ 
      21 << ETCSR_IPGR1_SHIFT |    /* IPGR1 = 21 */ 
      21 << ETCSR_IPGR2_SHIFT;     /* IPGR2 = 21 */
    
  /* Loopback type-specific configuration */
  /* Set the loopback bit in the EMCR */
  ioc3_bp->eregs.emcr |= EMCR_LOOPBACK;
      
  /* Enable RX and TX DMA */
  ioc3_bp->eregs.emcr |= (EMCR_RXEN | EMCR_RXDMAEN);
  ioc3_bp->eregs.emcr |= (EMCR_TXEN | EMCR_TXDMAEN);


  /* Initialize ETPIR */
  ioc3_bp->eregs.etpir = num_packets << 7;

  /* Wait for IOC3 to think it's done (TX_EMPTY).  Time out if it
     doesn't complete in the appropriate amount of time.  Check for
     unexpected interrupts in the polling loop. */
  timeout = (num_packets * packet_len)/100;  /* ~ 1us/byte @ 10Mb/s */
    
  timeout = (timeout * 100) / 667;	/* IOC3 loop is actually 66.67 Mb/s */
    
  timeout += 500;  /* Needed for small num_packets (increased from 3
		      to 500 to fix PV 409913.  */
    
  start_timeout = timeout;
    
  while (!(ioc3_bp->eregs.eisr & EISR_TXEMPTY)) {
      
    pon_us_delay(100);          /* 100 us */
    if (timeout-- <= 0) {
        msg_printf(ERR,"pon_enet: Timed out waiting for TX_EMPTY interrupt.\n");
	goto failure;
    }

    /* Check EISR for unexpected interrupts; the only expected
       interrupt is TX_EMPTY. */
    if (ioc3_bp->eregs.eisr & ~EISR_TXEMPTY) {
      msg_printf(ERR, "pon_enet: Unexpected EISR condition while waiting for TX_EMPTY.\n");
      msg_printf(ERR, "pon_enet:   EISR value = 0x%08x\n", ioc3_bp->eregs.eisr);
      msg_printf(ERR, "pon_enet:   Elapsed time = %d us\n", 
               (start_timeout - timeout) * 100);
      goto failure;
    }
  }

  /* Initialize ERPIR */
  ioc3_bp->eregs.erpir = 127 << 3; /* No need to be precise, this
                                        will work for all cases */

  /* Wait for the valid bit to be set in the RX buffer for the last
     packet.  */ 
  timeout = 10;         /* This is generous */

  while (!((*(__uint64_t *)rx_buf_pointer[num_packets - 1]) & 
           (__uint64_t) 0x8000000000000000)) {

    pon_us_delay(100);          /* .1 ms */
    if (timeout-- <= 0) {
      msg_printf(ERR, "pon_enet: Timed out polling RX buffer valid bit.\n");

      for (packet = 0; packet < num_packets; packet++) {
          
#if 0
          printf("   Packet %d  DW[0:2] : %016llx  %016llx  %016llx\n",
                 packet,
                 (*(__uint64_t *)rx_buf_pointer[packet]),
                 (*((__uint64_t *)rx_buf_pointer[packet] + 1)),
                 (*((__uint64_t *)rx_buf_pointer[packet] + 2)));
#endif

        if (!((*(__uint64_t *)rx_buf_pointer[packet]) & 
              (__uint64_t) 0x8000000000000000)) break;
          
      }
        
      msg_printf(VRB, "pon_enet: %d of %d packets were received.\n",
             packet, num_packets); 
      goto failure;
    }

    /* Check EISR for unexpected interrupts; the only expected
       interrupt is TX_EMPTY. Do error-return for any other interrupts.
     */
    if (ioc3_bp->eregs.eisr & ~EISR_TXEMPTY) {
        
      msg_printf(ERR, "pon_enet: Unexpected EISR condition while waiting \
for RX buffer valid bit.\n");
      msg_printf(ERR, "pon_enet: EISR value = 0x%08x\n", ioc3_bp->eregs.eisr);
      goto failure;
    }
  }

  /* For each TX descriptor, build an image of the expected RX buffer
   * from the TX buffer, and then compare with the actual contents.  If
   * there are discrepancies, report them.  Note that there are two
   * fields that are treated as "don't cares" in the comparison:
   *  
   *       - The IP checksum field in the first word
   *       - The four FCS (CRC) bytes at the end
   *
   * */

  for (packet = 0; packet < num_packets; packet++) { 

    /* Fill the RX first word */
    scratch_rx_buf.valid           = 1;
    scratch_rx_buf.reserved_3      = 0;

    /* No FCS on IOC3 loopback */
    scratch_rx_buf.byte_cnt = packet_len;
      
    scratch_rx_buf.ip_checksum = 0xFADE; /* Not compared */
      
    /* Fill the RX Status field */
    /* IOC3 loopback.  All MAC status bits will be zero. */
          
    scratch_rx_buf.rsv22        = 0;
    scratch_rx_buf.rsv21        = 0;
    scratch_rx_buf.rsv20        = 0;
    scratch_rx_buf.rsv19        = 0;
    scratch_rx_buf.rsv17        = 0;
    scratch_rx_buf.rsv16        = 0;
    scratch_rx_buf.rsv12_3      = 0;
    scratch_rx_buf.reserved_2   = 0;
    scratch_rx_buf.rsv2_0       = 0;
    scratch_rx_buf.reserved_1   = 0;
    scratch_rx_buf.inv_preamble = 0;
    scratch_rx_buf.code_err     = 0;
    scratch_rx_buf.frame_err    = 0;
    scratch_rx_buf.crc_err      = 0;
     

    bcopy((void *)tx_buf1_pointer[packet], (void *)scratch_rx_buf.pdu, packet_len);

    actual   = (erx_buffer_t *)rx_buf_pointer[packet];
    expected = &scratch_rx_buf;

    /* Check the first word, masking off IP checksum */
    if ( (*((__int32_t *)(actual))   & ~IP_CHECKSUM_MASK) !=
         (*((__int32_t *)(expected)) & ~IP_CHECKSUM_MASK)) {

      msg_printf(ERR, "pon_enet: RX data miscompare on packet %d first word.\n", packet);
      msg_printf(ERR, "pon_enet:     ==> exp: 0x%04x  recv: 0x%04x.\n", (*((__int32_t *)(expected))), (*((__int32_t *)(actual))) );
      goto failure;
    }
      
    /* Check the status word, masking off Carrier Event Previously Seen */
    if ((*((__int32_t *)(actual) + 1)   & ~RSV22_MASK) !=
        (*((__int32_t *)(expected) + 1) & ~RSV22_MASK)) {
      msg_printf(ERR, "pon_enet: RX data miscompare on packet %d status word.\n", packet);
      msg_printf(ERR, "pon_enet:    ==> exp: 0x%04x  recv: 0x%04x.\n", (*((__int32_t *)(expected)+1)), (*((__int32_t *)(actual)+1)) ); 
      goto failure;
    }
      
    /* Check the data */
    for (dw_index = 1; dw_index < packet_len/8 + 1; dw_index++) {
      if ( *((__int64_t *)(actual) + dw_index) !=
          (*((__int64_t *)(expected) + dw_index)) ) {
        msg_printf(ERR, "pon_enet: RX data miscompare on packet %d DW %d.\n", packet, dw_index);
        msg_printf(ERR, "pon_enet: ==> exp: 0x%08x  recv: 0x%08x.\n",
               (*((__int64_t *)(expected)+dw_index)),
		(*((__int64_t *)(actual)+dw_index)) );
        goto failure;
      }
    }

    remaining_bytes = packet_len - dw_index*8;
    for (byte = dw_index*8; byte < dw_index*8 + remaining_bytes; byte++) {
        
      if (actual->pdu[byte] != 0x5a) {

        msg_printf(ERR, "pon_enet: RX data miscompare on packet %d byte %d.\n", packet, byte);
        msg_printf(ERR, "pon_enet: ==> exp: 0x%02x  recv: 0x%02x.\n", 0x5a,
               actual->pdu[byte]);

	goto failure;
      }
    }
      
    /* Four bytes of FCS are tacked onto the end, but we don't check
       it */
  }

  diag_loop_cleanup();

  okydoky();

  return(0);
memfailure:		/* this can leak some memory, but shouldn't happen */
  msg_printf(SUM,"Cannot allocate memory for pon_enet!\n");
  sum_error(testname);
  return(1);
failure:
  diag_loop_cleanup();
  return(1);
}

static int
diag_loop_cleanup()
{
  int packet, ii;

  /* Free allocated memory. */
  free(tx_ring_malloc_ptr);
  free(rx_ring_malloc_ptr);
  for (packet = 0; packet < num_packets; packet++) {
    free(tx_buf1_malloc_ptr[packet]);
    free(rx_buf_malloc_ptr[packet]);
  }
  for (ii = 0; ii < garbage_index; ii++) {
    free(garbage_pointers[garbage_index]);
  }

  return(0);
}

#ifdef DEBUG
/* Dump IOC3 Enet registers.
 */
int
diag_enet_dump_regs()
{
  printf("\n Target IOC3 registers:\n");
  printf("   EMCR  : 0x%x   EISR  : 0x%x   EIER  : 0x%x\n",
         (__uint32_t) ioc3_bp->eregs.emcr,
         (__uint32_t) ioc3_bp->eregs.eisr,
         (__uint32_t) ioc3_bp->eregs.eier);
 
  printf("   ERCSR : 0x%x   ERBR_H: 0x%x   ERBR_L: 0x%x\n",
         (__uint32_t) ioc3_bp->eregs.ercsr,
         (__uint32_t) ioc3_bp->eregs.erbr_h,
         (__uint32_t) ioc3_bp->eregs.erbr_l);
 
  printf("   ERBAR : 0x%x   ERCIR : 0x%x   ERPIR : 0x%x\n",
         (__uint32_t) ioc3_bp->eregs.erbar,
         (__uint32_t) ioc3_bp->eregs.ercir,
         (__uint32_t) ioc3_bp->eregs.erpir);
 
  printf("   ETCSR : 0x%x   ERSR  : 0x%x   ETCDC : 0x%x\n",
         (__uint32_t) ioc3_bp->eregs.etcsr,
         (__uint32_t) ioc3_bp->eregs.ersr,
         (__uint32_t) ioc3_bp->eregs.etcdc);
 
  printf("   ETBR_H: 0x%x   ETBR_L: 0x%x   ETCIR : 0x%x\n",
         (__uint32_t) ioc3_bp->eregs.etbr_h,
         (__uint32_t) ioc3_bp->eregs.etbr_l,
         (__uint32_t) ioc3_bp->eregs.etcir);

  printf("   ETPIR : 0x%x   EBIR  : 0x%x   EMAR_H: 0x%x\n",
         (__uint32_t) ioc3_bp->eregs.etpir,
         (__uint32_t) ioc3_bp->eregs.ebir,
         (__uint32_t) ioc3_bp->eregs.emar_h);

  printf("   EMAR_L: 0x%x   EHAR_H: 0x%x   EHAR_L: 0x%x\n",
         (__uint32_t) ioc3_bp->eregs.emar_l,
         (__uint32_t) ioc3_bp->eregs.ehar_h,
         (__uint32_t) ioc3_bp->eregs.ehar_l);

  return(0);
}
#endif
