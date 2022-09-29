#include "sys/types.h"
#include <stdio.h>
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "uif.h"
#include <setjmp.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/PCI_defs.h>
#include "d_godzilla.h"
#include "d_pci.h"
#include "d_enet.h"
#include <sys/mii.h>

#define SPEED_10		0
#define SPEED_100		1
#define RX_FCS_SIZE		4
#define IP_CHECKSUM_MASK	0x0000ffff
#define RSV22_MASK		0x80000000
#define NORMAL_NUM_PACKETS	10	/* Must be <= 495 */
#define MAX_PACKET_LEN		1518
#define STALL_THRESH		1000

#define FULL_RING		512
#define HEAVY_NUM_PACKETS	495	/* Must be <= 495 */
#define HEAVY_PACKET_LEN	1501	/* Intentionally odd # */


#define _4K_			(4*1024)
#define _64K_			(64*1024)

#define ROUND64(x,size)	(((__uint64_t)(x) & ((__uint64_t)((size)-1))) ? (((__uint64_t)(x)+(__uint64_t)(size)) & ~((__uint64_t)((size)-1))) : (__uint64_t)(x))
#define HI_WORD(x)		((__uint32_t)((x) >> 32))
#define LO_WORD(x)		((__uint32_t)((x)))
#define KVTOIOADDR(addr)	kv_to_bridge32_dirmap((void *)BRIDGE_K1PTR,addr)

#if HEART_COHERENCY_WAR
extern void     __dcache_wb_inval(void *, int);
extern void     __dcache_inval(void *, int);
#endif  /* HEART_COHERENCY_WAR */

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

typedef struct etx_64k_ring_s {
  volatile etx_descriptor_t etx_descriptor[512];
} etx_64k_ring_t;


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
  volatile __uint64_t erx_buffer_pointer[512];
} erx_ring_t;


/* Static variables */
static int        num_packets, garbage_index = 0, enet_rc = 0;
static __uint64_t mac_address = 0xfefefefefefe;  /* Fake value used if
                                                    NIC test fails */
static __uint64_t *tx_ring_malloc_ptr;
static __uint64_t *tx_buf1_malloc_ptr[FULL_RING];
static __uint64_t *rx_ring_malloc_ptr;
static __uint64_t *rx_buf_malloc_ptr[FULL_RING];
static __uint64_t *garbage_pointers[FULL_RING];

static bridgereg_t old_bridge_int_enable;
static ioc3_mem_t *ioc3_bp;
static volatile __psunsigned_t ioc3_base;

/* Static functions */
static int loop_cleanup(int type);
static int check_pci_scr(void);

/* Extern function declarations */
extern int  nic_get_eaddr(__int32_t *mcr, __int32_t *gpcr_s, char *eaddr);

static int
check_pci_scr()
{

  int pci_error = 0;
  __uint32_t pci_scr, pci_err_addr_l, pci_err_addr_h;
  
  /* Get IOC3 base pointer */
  ioc3_base = PHYS_TO_K1(IOC3_PCI_DEVIO_BASE);
  ioc3_bp = (ioc3_mem_t *) ioc3_base;

  pci_scr = ioc3_bp->pci_scr;

  if (pci_scr & (PCI_SCR_RX_SERR | 
                 PCI_SCR_DROP_MODE | 
                 PCI_SCR_SIG_PAR_ERR |
                 PCI_SCR_SIG_TAR_ABRT |
                 PCI_SCR_RX_TAR_ABRT |
                 PCI_SCR_SIG_MST_ABRT |
                 PCI_SCR_SIG_SERR |
                 PCI_SCR_PAR_ERR)) {
    pci_error = 1;
  }
      
  if (pci_scr & PCI_SCR_RX_SERR)
      msg_printf(ERR,"   ==>    PCI_SCR<RX_SERR> is \
active:  The IOC3 detected SERR# from another agent while it was master.\n");
  
  if (pci_scr & PCI_SCR_DROP_MODE)
      msg_printf(ERR,"   ==>    PCI_SCR<DROP_MODE> is \
active:  The IOC3 detected a parity error on an address/command phase \
or it detected a data phase parity error on a PIO write.\n");
  
  if (pci_scr & PCI_SCR_SIG_PAR_ERR)
      msg_printf(ERR,"   ==>    PCI_SCR<SIG_PAR_ERR> is \
active:  The IOC3, as a master, either detected or was signalled a \
parity error.\n");
  
  if (pci_scr & PCI_SCR_SIG_TAR_ABRT)
      msg_printf(ERR,"   ==>    PCI_SCR<SIG_TAR_ABRT> is \
active:  The IOC3 signalled a target abort.\n");
  
  if (pci_scr & PCI_SCR_RX_TAR_ABRT)
      msg_printf(ERR,"   ==>    PCI_SCR<RX_TAR_ABRT> is \
active:  The IOC3 received a target abort.\n");
  
  if (pci_scr & PCI_SCR_SIG_MST_ABRT)
      msg_printf(ERR,"   ==>    PCI_SCR<SIG_MST_ABRT> is \
active:  The IOC3 issued a master abort.\n");
  
  if (pci_scr & PCI_SCR_SIG_SERR)
      msg_printf(ERR,"   ==>    PCI_SCR<SIG_SERR> is \
active:  The IOC3 asserted SERR# (address/command parity error).\n");
  
  if (pci_scr & PCI_SCR_PAR_ERR)
      msg_printf(ERR,"   ==>    PCI_SCR<PAR_ERR> is \
active:  The IOC3 detected a parity error.\n");
  
  if (pci_error) {
    pci_err_addr_h = ioc3_bp->pci_err_addr_h;
    pci_err_addr_l = ioc3_bp->pci_err_addr_l;
  }
  
  if ((pci_err_addr_l & PCI_ERR_ADDR_VLD) && pci_error) {
    
    msg_printf(SUM,"   ==>    PCI_ERR_ADDR_L<VALID> is active: info is valid.\n");
    msg_printf(SUM,"   ==>    PCI_ERR_ADDR_H value is 0x%08x\n",pci_err_addr_h);
    msg_printf(SUM,"   ==>    PCI_ERR_ADDR_L value is 0x%08x\n",pci_err_addr_l);
    
    switch ((pci_err_addr_l & PCI_ERR_ADDR_MST_ID_MSK) >> 1) {
    case 0:
      msg_printf(SUM,"   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 0   (Serial A TX)\n");
      break;
      
    case 1:
      msg_printf(SUM,"   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 1   (Serial A RX)\n");
      break;
      
    case 2:
      msg_printf(SUM,"   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 2   (Serial B TX)\n");
      break;
      
    case 3:
      msg_printf(SUM,"   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 3   (Serial B RX)\n");
      break;
      
    case 4:
      msg_printf(SUM,"   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 4   (Parallel port)\n");
      break;
      
    case 8:
      msg_printf(SUM,"   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 8   (Enet TX descriptor read)\n");
      break;
      
    case 9:
      msg_printf(SUM,"   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 9   (Enet TX BUF1 read)\n");
      break;
      
    case 10:
      msg_printf(SUM,"   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 10  (Enet TX BUF2 read)\n");
      break;
      
    case 11:
      msg_printf(SUM,"   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 11  (Enet RX buffer pointer ring read)\n");
      break;
      
    case 12:
      msg_printf(SUM,"   ==>    PCI_ERR_ADDR_L<MASTER_ID> = 12  (Enet RX buffer write)\n");
      break;
      
    default:
      
      break;
      
    }
    
    if (pci_err_addr_l & PCI_ERR_ADDR_MUL_ERR)
        msg_printf(SUM,"   ==>    PCI_ERR_ADDR_L<PCI_ERR_ADDR_MUL_ERR> is active: Another master had \
an error after this info froze.\n");
    
    msg_printf(SUM,"   ==>    Error address:  0x%08x%08x \n",
           pci_err_addr_h, pci_err_addr_l & 0xffffff80);
  }
  
  else if (pci_error)
      msg_printf(SUM,"   ==>    PCI_ERR_ADDR_L<VALID> is not active.\n"); 
  
  return(0);
}


/* Dump IOC3 Enet registers.  This is for debugging.  We may want to
   ifdef it out of the production code. */
int
enet_dump_regs()
{

   /* Get IOC3 base pointer */
  ioc3_base = PHYS_TO_K1(IOC3_PCI_DEVIO_BASE);
  ioc3_bp = (ioc3_mem_t *) ioc3_base;
  
  msg_printf(SUM,"\n Target IOC3 registers:\n"); 
  msg_printf(SUM,"   EMCR  : 0x%08lx   EISR  : 0x%08lx   EIER  : 0x%08lx\n", 
         (__uint64_t) ioc3_bp->eregs.emcr, 
	 (__uint64_t) ioc3_bp->eregs.eisr, 
	 (__uint64_t) ioc3_bp->eregs.eier);
  
  msg_printf(SUM,"   ERCSR : 0x%08lx   ERBR_H: 0x%08lx   ERBR_L: 0x%08lx\n", 
         (__uint64_t) ioc3_bp->eregs.ercsr, 
	 (__uint64_t) ioc3_bp->eregs.erbr_h, 
	 (__uint64_t) ioc3_bp->eregs.erbr_l);
  
  msg_printf(SUM,"   ERBAR : 0x%08lx   ERCIR : 0x%08lx   ERPIR : 0x%08lx\n", 
         (__uint64_t) ioc3_bp->eregs.erbar, 
	 (__uint64_t) ioc3_bp->eregs.ercir, 
	 (__uint64_t) ioc3_bp->eregs.erpir);
  
  msg_printf(SUM,"   ETCSR : 0x%08lx   ERSR  : 0x%08lx   ETCDC : 0x%08lx\n", 
         (__uint64_t) ioc3_bp->eregs.etcsr, 
	 (__uint64_t) ioc3_bp->eregs.ersr, 
	 (__uint64_t) ioc3_bp->eregs.etcdc);
  
  msg_printf(SUM,"   ETBR_H: 0x%08lx   ETBR_L: 0x%08lx   ETCIR : 0x%08lx\n", 
         (__uint64_t) ioc3_bp->eregs.etbr_h, 
	 (__uint64_t) ioc3_bp->eregs.etbr_l, 
	 (__uint64_t) ioc3_bp->eregs.etcir);
  
  msg_printf(SUM,"   ETPIR : 0x%08lx   EBIR  : 0x%08lx   EMAR_H: 0x%08lx\n", 
         (__uint64_t) ioc3_bp->eregs.etpir, 
	 (__uint64_t) ioc3_bp->eregs.ebir, 
	 (__uint64_t) ioc3_bp->eregs.emar_h);
  
  msg_printf(SUM,"   EMAR_L: 0x%08lx   EHAR_H: 0x%08lx   EHAR_L: 0x%08lx\n", 
         (__uint64_t) ioc3_bp->eregs.emar_l, 
	 (__uint64_t) ioc3_bp->eregs.ehar_h, 
	 (__uint64_t) ioc3_bp->eregs.ehar_l);
  
  return(0);
}



/* DMA loopback tests. */ 
int
enet_loop(int type)
{
  int                     ii, size, packet, packet_len, dw_index;
  int                     shift_amt, byte, remaining_bytes, speed;
  int                     try_again, timeout, start_timeout, dma_hung, retry_count; 
  int                     xtalk_stress_loop, rx_stall_counter, tx_stall_counter, rc;
  __uint64_t              rx_buf_pointer[FULL_RING];
  __uint64_t              tx_buf1_pointer[FULL_RING];
  __uint64_t              tx_ring_base;
  __uint64_t              bigend_mac_address;
  __uint64_t              rx_ring_base;
  __uint64_t              cmd_buf_cnt;
  __uint64_t              nasid;
  __uint32_t              rx_off, reg_value, reg0_value, reg1_value;
  __uint32_t              reg4_value, reg5_value, reg6_value;
  __uint32_t              br_isr, eisr, ercir, etcir, prev_ercir, prev_etcir;
  char                    type_string[128];
  erx_buffer_t            scratch_rx_buf;
  erx_buffer_t            *actual;
  erx_buffer_t            *expected;
  __uint64_t              start_time, stress_time;
  bridge_t                *bridge_bp;
  
  static __uint64_t       xtalk_pattern[8] = { 0x6955555555555555,
					       0x5569555555555555,
					       0x5555695555555555,
					       0x5555556955555555,
					       0x5555555569555555,
					       0x5555555555695555,
					       0x5555555555556955,
					       0x5555555555555569  };

  garbage_index = 0;
  
  /* Get IOC3 base pointer */
  ioc3_base = PHYS_TO_K1(IOC3_PCI_DEVIO_BASE);
  ioc3_bp = (ioc3_mem_t *) ioc3_base;
 
  switch (type) {
  case IOC3_LOOP:
    msg_printf(SUM,"Testing IOC3 internal ethernet loopback ....\n");
    strcpy(type_string, "IOC3 LOOPBACK");
    break;
  case PHY_LOOP:
    msg_printf(SUM,"Testing PHY chip internal ethernet loopback ....\n");
    strcpy(type_string, "PHY LOOPBACK");
    break;
  case EXTERNAL_LOOP:
    msg_printf(SUM,"Testing external ethernet loopback (10 and 100 Mb/s full duplex) ....\n");
    strcpy(type_string, "EXTERNAL LOOPBACK");
    break;
  case EXTERNAL_LOOP_10:
    msg_printf(SUM,"Testing external ethernet loopback (10Mb/s full duplex) ....\n");
    strcpy(type_string, "EXTERNAL LOOPBACK");
    break;
  case EXTERNAL_LOOP_100:
    msg_printf(SUM,"Testing external ethernet loopback (100Mb/s full duplex) ....\n");
    strcpy(type_string, "EXTERNAL LOOPBACK");
    break;
  case XTALK_STRESS:
    msg_printf(SUM,"Testing external ethernet loopback (XTALK stress mode) ....\n");
    strcpy(type_string, "XTALK STRESS");
    break;
  default:
    msg_printf(SUM,"enet_loop: ERROR illegal loopback type parm (%d).\n", type);
    return(1);
  }
                                
  init_ide_malloc(2*1024*1024);		/* intialize enet buffer area */

  /* Determine the number of packets to be generated and the length of
     each packet, based on the mode parameter (normal vs heavy). */
  num_packets =  HEAVY_NUM_PACKETS;
  
  if (type == XTALK_STRESS) num_packets = FULL_RING;
  
  packet_len = HEAVY_PACKET_LEN; 

  size = sizeof(etx_64k_ring_t)+_64K_;
  /* Allocate memory for the TX descriptor ring.  For simplicity we'll
     use the large ring size even in normal mode.  We need to
     over-allocate by 64K in order to assure ring alignment. */
  if (!(tx_ring_malloc_ptr = 
        (__uint64_t *)ide_align_malloc(size,  MIN(size,ETBR_ALIGNMENT)))) {
    
    msg_printf(ERR,"_enet_loop: Couldn't allocate memory for TX descriptor ring.\n");
    return(1);
  }

#if 0
  msg_printf(SUM,"tx_ring_malloc_ptr = 0x%016llx\n",tx_ring_malloc_ptr);
#endif
  
  tx_ring_base = ROUND64(tx_ring_malloc_ptr, _64K_); /* Round up to 64K boundary */

  /* Write the base address to the ETBR_H and ETBR_L registers.  Since
     we are using 64K rings, the RING_SIZE bit (bit 0) is set.
     All other attribute bits are set to zero. */
  ioc3_bp->eregs.etbr_h  = 
	(__uint32_t)((KVTOIOADDR(tx_ring_base) & 0xffffffff00000000LL) >> 32);

  ioc3_bp->eregs.etbr_l  = 
	(__uint32_t)(KVTOIOADDR(tx_ring_base) & 0xffffffff) | 0x01;
  
  
  /* Calculate TX descriptor Command and Buffer Count fields.  
     They are the same for all packets: 
     CHK_OFFSET    = 0   
     DO_CHECKSUM,   = 0  
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
      
      if (!(tx_buf1_malloc_ptr[packet] = (__uint64_t *)ide_malloc(packet_len + 8))) {
        
        msg_printf(ERR,"enet_loop: Couldn't allocate memory for TX packet %d buffer.\n", packet);
        return(1);
      }
      
      tx_buf1_pointer[packet] = ROUND64(tx_buf1_malloc_ptr[packet], 8);

      /* Test for 16K crossing */
      if ((tx_buf1_pointer[packet] & 0x3fff) + packet_len > 0x4000) {

	if (garbage_index >= FULL_RING - 1) {
	  msg_printf(ERR,"enet_loop: Too many garbage pointers (TX packet %d).\n", 
		 packet);
	  return(1);
	}
	
	garbage_pointers[garbage_index++] = tx_buf1_malloc_ptr[packet];
        try_again = 1;
      }
      
      else try_again = 0;
      
    } while (try_again);

    /* Write the first DW (the Command and Buffer Count fields) */
    (*((__uint64_t *)tx_ring_base + packet * 16)) = cmd_buf_cnt;
    
    /* Write the BUF_PTR_1 field.  Bits 63:60 are the node WID.  All
       other attribute bits are set to 0. */
    (*((__uint64_t *)tx_ring_base + packet * 16 + 1)) = 
	KVTOIOADDR(tx_buf1_pointer[packet]);
    
    /* It should be OK to leave the rest of the TX ring entry (BUF_PTR_2
       and DATA_0) uninitialized. */
  }
  
#if HEART_COHERENCY_WAR
  __dcache_wb_inval((void *)tx_ring_base, sizeof(etx_64k_ring_t));
#endif

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
   * the packet.  The 13th byte is the loopback type code (0 = IOC3, 1 =
   * PHY, 3 = EXT).  The 14th byte is the speed (0 =
   * 10Mb/s, 1 = 100 Mb/s).  The 15th and 16th bytes are the packet number.
   * 
   * The data patterns are chosen to find SSO problems on the PCI bus
   * and XTALK LLP.  PCI stress paterns are the default, but XTALK LLP
   * stress patterns are used in the xtalk stress mode.  To stress PCI,
   * the data alternates between words of all ones and one walking zero,
   * and the complement of this, i.e. a typical double-word would be
   * 0xfffeffff00010000.
   * 
   * e.g. The first 8 DWs of a PCI stress packet might look like
   * 
   * fefefefefefefefe fefefefe020101ac
   * ffffffff00000000 7fffffff80000000
   * bfffffff40000000 dfffffff20000000
   * efffffff10000000 f7ffffff08000000
   * 
   * The bytes in the last partial DW of the packet are written with
   * 0x5a.
   *
   * The pattern for LLP stress is a "walking 69" (seriously!), and is
   * intended to generate a similar effect on the LLP SSD. */
  
  for (packet = 0; packet < num_packets; packet++) {
    
    /* First DW */
    dw_index = 0;
    
    (*((__uint64_t *)tx_buf1_pointer[packet] + dw_index)) = 
        (mac_address << 16) | 
            (mac_address >> 32);
     
    /* Second DW (filled later - see while loop below) */
    
    /* Remaining DWs */
    dw_index = 2;

    
    if(type != XTALK_STRESS) {
      /* Generate PCI stress pattern  XXX: since this part of the packet is
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
    else {
      /* Fill data with xtalk stress pattern */
      while (dw_index < packet_len/8) {

	(*((__uint64_t *)tx_buf1_pointer[packet] + dw_index)) =
	    xtalk_pattern[dw_index % 8];
        
	dw_index++;
      }
    
      /* Fill the remaining partial DW with bytes of 0x55 */
      remaining_bytes = packet_len - dw_index*8;
    
      for (byte = 0; byte < remaining_bytes; byte++) {
      
	(*((uchar_t *)tx_buf1_pointer[packet] + dw_index*8 + byte)) = 0x55;
      }
    }

  }
  
  /* Allocate memory for RX buffer pointer ring. */
  if (!(rx_ring_malloc_ptr = (__uint64_t *)ide_align_malloc( (sizeof(erx_ring_t)+_4K_), ERBR_ALIGNMENT))) {
    msg_printf(ERR,"enet_loop: Couldn't allocate memory for RX pointer ring.\n");
    return(1);
  } 
    

  rx_ring_base = ROUND64(rx_ring_malloc_ptr, _4K_); /* Round up to 4K boundary */

  /* Write the base address to the ERBR_H and ERBR_L registers.  Bits
     63:60 are the node WID.  All other attribute bits are set to
     zero. */
  ioc3_bp->eregs.erbr_h  = 
	(__uint32_t)((KVTOIOADDR(rx_ring_base) & 0xffffffff00000000LL) >> 32);

  ioc3_bp->eregs.erbr_l  = (__uint32_t)(KVTOIOADDR(rx_ring_base) & 0xffffffff); 
  
  for (packet = 0; packet < num_packets; packet++) {
    
    /* Allocate memory for RX buffers.  Buffers are 1664 bytes, but must
       be 128-byte aligned, so we overallocate and round up.  The
       hardware also requires that the buffer not cross a 16K boundary,
       so we test for this case, and do another malloc if the first try
       did cross a 16K boundary. */
    do {
      
      if (!(rx_buf_malloc_ptr[packet] = 
            (__uint64_t *)ide_malloc(sizeof (erx_buffer_t) + 128))) {
        
        msg_printf(ERR,"enet_loop: Couldn't allocate memory for RX packet %d buffer.\n", packet);
        return(1);
      }
      
      rx_buf_pointer[packet] = ROUND64(rx_buf_malloc_ptr[packet], 128); /* Round up to 128B boundary */

      /* Test for 16K crossing */
      if ((rx_buf_pointer[packet] & 0x3fff) + sizeof (erx_buffer_t) > 0x4000) {
        
	if (garbage_index >= FULL_RING - 1) {
	  msg_printf(ERR,"enet_loop: Too many garbage pointers (RX packet %d).\n", 
		 packet);
	  return(1);
	}
	
	garbage_pointers[garbage_index++] = rx_buf_malloc_ptr[packet];
        try_again = 1;
      }
      
      else try_again = 0;
      
    } while (try_again);
    
    /* Write the ring entry.  Bits 63:60 are the node WID.  All other
       attribute bits are set to 0. */
    (*((__uint64_t *)rx_ring_base + packet)) = 
	KVTOIOADDR(rx_buf_pointer[packet]);
    
  }

#if HEART_COHERENCY_WAR
  __dcache_wb_inval((void *)rx_ring_base, sizeof(erx_ring_t));
#endif  /* HEART_COHERENCY_WAR */

  /* Enable Bridge interrupts */
  bridge_bp = (bridge_t *)PHYS_TO_K1(BRIDGE_BASE);
  old_bridge_int_enable = bridge_bp->b_int_enable;
  
  bridge_bp->b_int_rst_stat = 0x7f;      /* Clear all error interrupts  */
  bridge_bp->b_int_enable = 0xffc1ff00;  /* Enable all interrupts,
					    except for LLP errors */

  /* For IOC3 internal loopback, we only want to test 100Mb/s operation.
     For PHY chip internal loopback and external loopback, we want to
     test both 10 and 100 Mb/s operation. */
  if (type == IOC3_LOOP || 
      type == EXTERNAL_LOOP_100 || 
      type == XTALK_STRESS) 
      speed = SPEED_100; 
  else speed = SPEED_10;
  
  while (speed <= SPEED_100) {

    if (type == EXTERNAL_LOOP_10 && speed == SPEED_100) break; 

#if 1
    msg_printf(SUM,"   enet_loop: Testing %s at %s.\n", type_string, speed ? "100Mb/s" : "10Mb/s");
#endif
    
    for (packet = 0; packet < num_packets; packet++) {
      
      /* Write to second TX BUF1 DW.  This was deferred until now so we
         can indicate the speed. */
      dw_index = 1;
    
      (*((__uint64_t *)tx_buf1_pointer[packet] + dw_index)) = 
          mac_address << 32 |
              type << 24 |
                  speed << 16 |
                      packet;
      
      /* Clear the first DW in each RX buffer to clear valid bit. */
      (*(__uint64_t *)rx_buf_pointer[packet]) = 0;
#if HEART_COHERENCY_WAR
      __dcache_wb_inval((void *)tx_buf1_pointer[packet], packet_len);
      __dcache_wb_inval((void *)rx_buf_pointer[packet], sizeof(erx_buffer_t));
#endif  /* HEART_COHERENCY_WAR */

    }
  
    /* Reset IOC3 Ethernet  */
    ioc3_bp->eregs.emcr = EMCR_RST;
  
    timeout = 2000;
  
    while (!(ioc3_bp->eregs.emcr & EMCR_ARB_DIAG_IDLE)) {
    
      DELAY(1000);         /* 1 ms */
      if (timeout-- <= 0) {
      
        msg_printf(ERR,"enet_loop: Timed out on EMCR_ARB_DIAG_IDLE trying to reset enet.\n");
	loop_cleanup(type);
        return(1);
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

    /* Initialize ETCSR.  Values from kern/master.d/if_ef */
    ioc3_bp->eregs.etcsr  =  21 |    /* IPGT  = 21 */ 
	21 << ETCSR_IPGR1_SHIFT |    /* IPGR1 = 21 */ 
	    21 << ETCSR_IPGR2_SHIFT; /* IPGR2 = 21 */
    
    /* Loopback type-specific configuration */
    if (type == IOC3_LOOP) {
      
      /* Set the loopback bit in the EMCR */
      ioc3_bp->eregs.emcr |= EMCR_LOOPBACK;
    }
    
    else {
      
      /* Reset the PHY */
      if (rc = (enet_phy_reset())) {
	  msg_printf(ERR,"enet_loop: Phy reset failed, abandoning test.\n");
        return(rc);
      }

      if (type == EXTERNAL_LOOP) {  /* Note that we skip testing AN if
        			       the loopback type is one of the
        			       specified speed/duplex types */
        
        /* Test auto-negotiation */
        if (rc = enet_phy_or(0, (u_short) MII_R0_RESTARTAUTO)) return(rc);
        
        timeout = 2000;  /* 2 seconds */
	
	reg1_value = 0;

        while (!(reg1_value & MII_R1_AUTODONE)) {
	  
          DELAY(1000);
          
          if(timeout-- <= 0) {
            
            msg_printf(ERR,"enet_loop: Phy did not complete auto-negotiation\n");
            msg_printf(SUM,"                with itself via external loopback cable.\n");
	      msg_printf(SUM,"                  ==>   Reg 0 = 0x%04x\n", reg0_value);
	      msg_printf(SUM,"                  ==>   Reg 1 = 0x%04x\n", reg1_value);
	      msg_printf(SUM,"                  ==>   Reg 4 = 0x%04x\n", reg4_value);
	      msg_printf(SUM,"                  ==>   Reg 5 = 0x%04x\n", reg5_value);
	      msg_printf(SUM,"                  ==>   Reg 6 = 0x%04x\n", reg6_value);
	      msg_printf(SUM,"                  \n");
	      msg_printf(SUM,"            *** Are you sure there is an external loopback? ***\n");
	    loop_cleanup(type);
            return(1);
            
          }
	  
	  if (rc = enet_phy_read(0, &reg0_value)) return(rc);
	  
	  if (rc = enet_phy_read(1, &reg1_value)) return(rc);
	  
	  if (rc = enet_phy_read(4, &reg4_value)) return(rc);
	  
	  if (rc = enet_phy_read(5, &reg5_value)) return(rc);
	  
	  if (rc = enet_phy_read(6, &reg6_value)) return(rc);
	  
        }
               
        if (!(reg6_value & MII_R6_LPNWABLE)) {
	  
	  msg_printf(ERR,"enet_loop: PHY reg 6 LP_AN_ABLE bit not set after auto-negotiation.\n");
	    msg_printf(SUM,"                  ==>   Reg 0 = 0x%04x\n", reg0_value);
	    msg_printf(SUM,"                  ==>   Reg 1 = 0x%04x\n", reg1_value);
	    msg_printf(SUM,"                  ==>   Reg 4 = 0x%04x\n", reg4_value);
	    msg_printf(SUM,"                  ==>   Reg 5 = 0x%04x\n", reg5_value);
	    msg_printf(SUM,"                  ==>   Reg 6 = 0x%04x\n", reg6_value);
	    msg_printf(SUM,"                  \n");
	    msg_printf(SUM,"            *** Are you sure there is an external loopback? ***\n");
	  loop_cleanup(type);
	  return(1);
	}
        
        if ((reg4_value & 0x3e0) != (reg5_value & 0x3e0)) {
	  
 	  msg_printf(ERR,"enet_loop: PHY reg 5 mode support bits (9:5) do not\n");
 	  msg_printf(SUM,"                not match reg 4 mode support bits after\n");
 	  msg_printf(SUM,"                autonegotiation.  Reg 4: 0x%04x  Reg 5: 0x%04x\n",
		 reg4_value, reg5_value);

	  loop_cleanup(type);
	  return(1);
	}
        
        if (rc = (enet_phy_reset())) {
          msg_printf(ERR,"enet_loop: Phy reset failed after autonegotiation, \
abandoning test.\n");
          return(rc);
        }
      } 

      /* Set speed and duplex in PHY register 0 */
      if (type == PHY_LOOP && speed == SPEED_10) {
        
        /* Set the 10BT_LPBK bit in the register 18 */
        if (rc = enet_phy_and(18, ~(0x0010)) ) return(rc);
        
        reg_value = MII_R0_LOOPBACK | MII_R0_DUPLEX;
      }
      else if (type == PHY_LOOP && speed == SPEED_100)
          reg_value = MII_R0_LOOPBACK | MII_R0_SPEEDSEL | MII_R0_DUPLEX;

      else if (((type == XTALK_STRESS || 
		 type == EXTERNAL_LOOP) && speed == SPEED_100) ||
	       type == EXTERNAL_LOOP_100)
          reg_value = MII_R0_SPEEDSEL | MII_R0_DUPLEX;
      
      else if ((type == EXTERNAL_LOOP && speed == SPEED_10) ||
	       type == EXTERNAL_LOOP_10)
          reg_value = MII_R0_DUPLEX;
      
      else {
        msg_printf(ERR,"enet_loop: PROGRAMMING ERROR!!!!");
      }
      
      if (rc = enet_phy_write(0, (u_short) reg_value)) return(rc);

      /* Set F_CONNECT (Force disconnect func. bypass) bit in reg 18 */
      if (rc = enet_phy_or(18, (u_short) 0x0020)) return(rc);
      
      /* The PHY seems to need some time to adjust to changes, so we
         insert a delay here.  The problem is worse when there is an
         external loop cable.  This is presumably due to
         autonegotiation.  */
      if (type == EXTERNAL_LOOP      ||
          type == EXTERNAL_LOOP_10   ||
          type == EXTERNAL_LOOP_100)
          DELAY(2000000);  /* 2 seconds */
      else
	  DELAY(10000);  /* 10 ms */
      
    }
    
    /* Initialize ERPIR and ETPIR */
    ioc3_bp->eregs.erpir = 511 << 3; /* No need to be precise, this
                                        will work for all cases */
    
    if (type == XTALK_STRESS) 
	ioc3_bp->eregs.etpir = 495 << 7;
    else 
	ioc3_bp->eregs.etpir = num_packets << 7;
        
    /* Enable RX DMA */
    ioc3_bp->eregs.emcr |= 
        (EMCR_RXEN    | 
         EMCR_RXDMAEN);
    
    /* Enable TX DMA */
    ioc3_bp->eregs.emcr |= 
        (EMCR_TXEN    | 
         EMCR_TXDMAEN);

    /* For the Xtalk stress test, we just sit in a loop checking the
       EISR and Bridge ISR for errors and chasing the consume pointers.
       There is no result checking of the data, and checking the xtalk
       error registers on the Heart, Xbow, and Bridge is left either to an
       exception handler or the external caller of this function.
       We try to sustain this for about five seconds.
     */
    if (type == XTALK_STRESS) {

      stress_time = 50000;
      prev_ercir = 0xffffffff;
      prev_etcir = 0xffffffff;
      dma_hung = 0;
      xtalk_stress_loop = 0;
      rx_stall_counter = 0;
      tx_stall_counter = 0;

      /* Clear error bits in PCI SCR */
      ioc3_bp->pci_scr |= 0xffff0000;
      flushbus();

      do {

        ercir = ioc3_bp->eregs.ercir;
	etcir = ioc3_bp->eregs.etcir;

	if (ercir != prev_ercir) rx_stall_counter = 0;
	else rx_stall_counter++;
	
	if (etcir != prev_etcir) tx_stall_counter = 0;
	else tx_stall_counter++;
	
	if ((ercir == prev_ercir) && (rx_stall_counter > STALL_THRESH)) {
	  
	  msg_printf(ERR,"enet_loop: RX CONSUME did not increment from Xtalk stress loop %d to %d.\n",
		 xtalk_stress_loop - STALL_THRESH, xtalk_stress_loop);

	  dma_hung = 1;
	}
    
	if ((etcir == prev_etcir) && (tx_stall_counter > STALL_THRESH)) {
	  if (!dma_hung)  
	  msg_printf(ERR,"enet_loop: TX CONSUME did not increment from Xtalk stress loop %d to %d.\n",
		 xtalk_stress_loop - STALL_THRESH, xtalk_stress_loop);
	  dma_hung = 1;
	}

	if (dma_hung) {
	    msg_printf(ERR,"Bridge response buffer status: 0x%08x\n", bridge_bp->b_resp_status);
	    check_pci_scr();
	    enet_dump_regs();
	    loop_cleanup(type);
	    return(1); 
	}
	
      
	/* Check EISR and Bridge ISR for unexpected interrupts; none is
           expected here */
	eisr = ioc3_bp->eregs.eisr;
	br_isr = bridge_bp->b_int_status;
	
	if (eisr) {
        
	  msg_printf(ERR,"enet_loop: Unexpected EISR condition in Xtalk stress loop %d.\n",
		 xtalk_stress_loop);
	  msg_printf(SUM,"enet_loop:   EISR value = 0x%08x\n", eisr);

	  loop_cleanup(type);
	  return(1);
	}

	if (br_isr) {
        
	  msg_printf(ERR,"enet_loop: Unexpected Bridge ISR condition in Xtalk stress loop %d.\n",
		 xtalk_stress_loop);
	  msg_printf(SUM,"enet_loop:   Bridge ISR value = 0x%08x\n", br_isr);

	  loop_cleanup(type);
	  return(1);
	}
	
	prev_ercir = ercir;
	prev_etcir = etcir;
	
	ioc3_bp->eregs.erpir = ioc3_bp->eregs.ercir + (495 << 3);
	flushbus();
	ioc3_bp->eregs.etpir = ioc3_bp->eregs.etcir + (495 << 7);
	flushbus();
	DELAY(100);

	xtalk_stress_loop++;
	
      } while (stress_time--);
      
    }
    
    
    /* Wait for IOC3 to think it's done (TX_EMPTY).  Time out if it
       doesn't complete in the appropriate amount of time.  Check for
       unexpected interrupts in the polling loop. */
    timeout = (num_packets * packet_len)/100;  /* ~ 1us/byte @ 10Mb/s */
    
    if (type == IOC3_LOOP)
	 timeout = (timeout*100)/667; /* IOC3 loop is actually 66.67 Mb/s */
    
    else timeout = (speed == SPEED_100) ? timeout/10 : timeout;

    timeout += 500;  /* Needed for small num_packets (increased from 3
    			to 500 to fix PV 409913.  */
    
    start_timeout = timeout;
    
    while (!(ioc3_bp->eregs.eisr & EISR_TXEMPTY)) {
      
      /* In xtalk stress mode, we have to keep kicking RX_PRODUCE so we
         don't get an RX OFLO */
      if (type == XTALK_STRESS) 
	  ioc3_bp->eregs.erpir = ioc3_bp->eregs.ercir + (495 << 3);
      
      DELAY(100);          /* 100 us */
      if (timeout-- <= 0) {
        
        msg_printf(ERR,"enet_loop: Timed out waiting for TX_EMPTY \
interrupt (%s:  %s).\n", type_string, speed ? "100Mb/s" : "10Mb/s");
        
	check_pci_scr();
	enet_dump_regs();
	loop_cleanup(type);
        return(1);
      }

      /* Check EISR for unexpected interrupts; the only expected
         interrupt is TX_EMPTY.  XXX: this will be enhanced to diagnose
         the problem. */
      if (ioc3_bp->eregs.eisr & ~EISR_TXEMPTY) {
        
        msg_printf(ERR,"enet_loop: Unexpected EISR condition while waiting \
for TX_EMPTY (%s:  %s).\n", type_string, speed ? "100Mb/s" : "10Mb/s");
        msg_printf(SUM,"enet_loop:   EISR value = 0x%08x\n", ioc3_bp->eregs.eisr);
        msg_printf(SUM,"_enet_loop:   Elapsed time = %d us\n", 
               (start_timeout - timeout) * 100);
        
	loop_cleanup(type);
        return(1);
      }
    }

    /* Check Bridge ISR for interrupt  */
    if (!(bridge_bp->b_int_status & (1 << 2))) {  /* Not sure if this
							is valid for all board types */
        msg_printf(ERR,"enet_loop: Bridge ISR bit 2 did not set after TX_EMPTY \
interrupt (%s:  %s).\n", type_string, speed ? "100Mb/s" : "10Mb/s");
	msg_printf(SUM,"                ==> Bridge ISR = 0x%08x\n", bridge_bp->b_int_status);
        
	loop_cleanup(type);
        return(1);
    }

    /* Wait for the valid bit to be set in the RX buffer for the last
       packet.  XXX also should set up for an RX_THRESH_INT when the
       last packet is received. */
    timeout = 10;         /* This is generous */

#if HEART_COHERENCY_WAR
      __dcache_inval((void *)rx_buf_pointer[num_packets - 1], sizeof(erx_buffer_t));
#endif  /* HEART_COHERENCY_WAR */

    while (!((*(__uint64_t *)rx_buf_pointer[num_packets - 1]) & 
             (__uint64_t) 0x8000000000000000)) {
      
      DELAY(100);          /* 100 us */
      if (timeout-- <= 0) {
        
        msg_printf(ERR,"enet_loop: Timed out polling RX buffer valid \
bit. (%s:  %s).\n", type_string, speed ? "100Mb/s" : "10Mb/s");
        
        for (packet = 0; packet < num_packets; packet++) {
          
#if 0
          msg_printf(SUM,"   Packet %d  DW[0:2] : %016llx  %016llx  %016llx\n",
                 packet,
                 (*(__uint64_t *)rx_buf_pointer[packet]),
                 (*((__uint64_t *)rx_buf_pointer[packet] + 1)),
                 (*((__uint64_t *)rx_buf_pointer[packet] + 2)));
#endif

          /* XXX - could add analysis here of whether a packet was
             dropped. */

#if HEART_COHERENCY_WAR
      __dcache_inval((void *)rx_buf_pointer[packet], sizeof(erx_buffer_t));
#endif  /* HEART_COHERENCY_WAR */

          if (!((*(__uint64_t *)rx_buf_pointer[packet]) & 
                (__uint64_t) 0x8000000000000000)) break;
          
        }
        
        msg_printf(SUM,"enet_loop: %d of %d packets were received.\n",
               packet, num_packets); 
        
	check_pci_scr();
	enet_dump_regs();

	loop_cleanup(type);
        return(1);
      }

      /* Check EISR for unexpected interrupts; the only expected
         interrupt is TX_EMPTY.  XXX: this will be enhanced to diagnose
         the problem. */
      if (ioc3_bp->eregs.eisr & ~EISR_TXEMPTY) {
        
        msg_printf(ERR,"enet_loop: Unexpected EISR condition while waiting \
for RX buffer valid bit (%s:  %s).\n", type_string, speed ? "100Mb/s" : "10Mb/s");
        msg_printf(SUM,"_enet_loop: EISR value = 0x%08x\n", ioc3_bp->eregs.eisr);
        
	loop_cleanup(type);
        return(1);
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
    for (packet = 0; 
	 packet < num_packets && type != XTALK_STRESS; 
	 packet++) {
      
      /* Fill the RX first word */
      scratch_rx_buf.valid           = 1;
      scratch_rx_buf.reserved_3      = 0;
      
      if (type != IOC3_LOOP) 
          scratch_rx_buf.byte_cnt = packet_len + RX_FCS_SIZE;
      else 
          /* No FCS on IOC3 loopback */
          scratch_rx_buf.byte_cnt = packet_len;
      
      scratch_rx_buf.ip_checksum = 0xFADE; /* Not compared */
      
      /* Fill the RX Status field */
      if (type != IOC3_LOOP) { 
        
        scratch_rx_buf.rsv22        = 0; /* Carrier event previously seen */ 
        scratch_rx_buf.rsv21        = 1; /* Good packet received */
        scratch_rx_buf.rsv20        = 0; /* Bad packet received */
        scratch_rx_buf.rsv19        = 0; /* Long event previously seen */
        scratch_rx_buf.rsv17        = 0; /* Broadcast  */
        scratch_rx_buf.rsv16        = 0; /* Broadcast/Multicast  */
        scratch_rx_buf.rsv12_3      = scratch_rx_buf.byte_cnt >> 3;
        scratch_rx_buf.reserved_2   = 0;
        scratch_rx_buf.rsv2_0       = scratch_rx_buf.byte_cnt;
        scratch_rx_buf.reserved_1   = 0;
        scratch_rx_buf.inv_preamble = 0;
        scratch_rx_buf.code_err     = 0;
        scratch_rx_buf.frame_err    = 0;
        scratch_rx_buf.crc_err      = 0;
      }
        
      else {                    /* IOC3 loopback.  All MAC status bits will be zero. */
          
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
      }
     

      /* Fill the data part a DW at a time */      
      for (dw_index = 0; dw_index < packet_len/8; dw_index++) {
        
        (*((__uint64_t *)scratch_rx_buf.pdu + dw_index))  = 
            (*((__uint64_t *) tx_buf1_pointer[packet] + dw_index));
      }

      
      /* Fill the remaining partial DW with bytes of 0x5a */
      remaining_bytes = packet_len - dw_index*8;
    
      for (byte = 0; byte < remaining_bytes; byte++) {
        
        scratch_rx_buf.pdu[dw_index*8 + byte] = 0x5a;
      
      }

#if HEART_COHERENCY_WAR
      __dcache_inval((void *)rx_buf_pointer[packet], sizeof(erx_buffer_t));
#endif  /* HEART_COHERENCY_WAR */

      actual   = (erx_buffer_t *) rx_buf_pointer[packet];
      expected = &scratch_rx_buf;

      /* Check the first word, masking off IP checksum */
      if ((*((__int32_t *)(actual))   & ~IP_CHECKSUM_MASK) !=
          (*((__int32_t *)(expected)) & ~IP_CHECKSUM_MASK)) {
        msg_printf(ERR,"enet_loop: RX data miscompare on packet %d first word.\n", packet);
        msg_printf(SUM,"enet_loop:     ==> exp: 0x%04x  recv: 0x%04x (%s:  %s).\n", (*((__int32_t *)(expected))), 
               (*((__int32_t *)(actual))),
               type_string, speed ? "100Mb/s" : "10Mb/s");
        
	loop_cleanup(type);
        return(1);
      }
      
      /* Check the status word, masking off Carrier Event Previously Seen */
      if ((*((__int32_t *)(actual) + 1)   & ~RSV22_MASK) !=
          (*((__int32_t *)(expected) + 1) & ~RSV22_MASK)) {
        msg_printf(ERR,"enet_loop: RX data miscompare on packet %d status word.\n", packet);
        msg_printf(SUM,"enet_loop:     ==> exp: 0x%04x  recv: 0x%04x (%s:  %s).\n", (*((__int32_t *)(expected)+1)), 
               (*((__int32_t *)(actual)+1)),
               type_string, speed ? "100Mb/s" : "10Mb/s");
        
	loop_cleanup(type);
        return(1);
      }
      
      /* Check the data */
      for (dw_index = 1; dw_index < packet_len/8 + 1; dw_index++) {
        if ( *((__int64_t *)(actual) + dw_index) !=
            (*((__int64_t *)(expected) + dw_index)) ) {
          msg_printf(ERR,"enet_loop: RX data miscompare on packet %d DW %d.\n", packet, dw_index);
          msg_printf(SUM,"enet_loop:     ==> exp: 0x%08x  recv: 0x%08x (%s:  %s).\n",
                 (*((__int64_t *)(expected)+dw_index)), 
                 (*((__int64_t *)(actual)+dw_index)),
                 type_string, speed ? "100Mb/s" : "10Mb/s");

	  loop_cleanup(type);
          return(1);
        }
      }

      remaining_bytes = packet_len - dw_index*8;
      for (byte = dw_index*8; byte < dw_index*8 + remaining_bytes; byte++) {
        
        if (actual->pdu[byte] != 0x5a) {
          msg_printf(ERR,"enet_loop: RX data miscompare on packet %d byte %d.\n", packet, byte);
          msg_printf(SUM,"enet_loop:     ==> exp: 0x%02x  recv: 0x%02x (%s:  %s).\n", 0x5a,
                 actual->pdu[byte],
                 type_string, speed ? "100Mb/s" : "10Mb/s");

	  loop_cleanup(type);
          return(1);
        }
      }
      
      /* Four bytes of FCS are tacked onto the end, but we don't check
         it */
    }

    speed++;
  }

  loop_cleanup(type); 

  msg_printf(SUM,"%s test passed\n", type_string);
  
  return(0);
}

static int
loop_cleanup(int type)
{

  int packet, ii, rc;
  bridge_t *bridge_bp;
  
#if 0
  int *junk;
#endif

  /* Free allocated memory. */
  ide_free(tx_ring_malloc_ptr);
  ide_free(rx_ring_malloc_ptr);
  for (packet = 0; packet < num_packets; packet++) {
    ide_free(tx_buf1_malloc_ptr[packet]);
    ide_free(rx_buf_malloc_ptr[packet]);
  }
  for (ii = 0; ii < garbage_index; ii++) {
    ide_free(garbage_pointers[garbage_index]);
  }

  /* Restore the Bridge INT_ENABLE register */
  bridge_bp = (bridge_t *)PHYS_TO_K1(BRIDGE_BASE);
  bridge_bp->b_int_enable = old_bridge_int_enable;

  /* Reset the PHY */
  if (type != IOC3_LOOP) 
      if (rc = enet_phy_reset())
	  return(rc);

  return(0);
}
