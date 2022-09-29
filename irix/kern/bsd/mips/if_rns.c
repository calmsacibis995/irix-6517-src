/* RNS PCI bus FDDI board
 *
 * Copyright 1996 Silicon Graphics, Inc.  All rights reserved.
 */

#ident "$Revision: 1.38 $"

#include <tcp-param.h>
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/debug.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/ddi.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kmem.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/invent.h>
#ifdef FDDI_BONSAI
typedef unsigned  micro_t;
#include <sys/edt.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/PCI_defs.h>
#else
#include <sys/iograph.h>
#include <sys/PCI/PCI_defs.h>
#include <sys/PCI/pciio.h>
#include <sys/PCI/ioc3.h>
#include <sys/PCI/bridge.h>
#include <sys/idbgentry.h>
#endif
#if defined(FDDI_FICUS) || defined(FDDI_BONSAI)
#if !defined(USE_PCI_PIO) && !defined(pciio_pio_read32)
#define pciio_pio_read32(a) *a
#define pciio_pio_write8(v,a) (*(a) = (v))
#define pciio_pio_write16(v,a) (*(a) = (v))
#define pciio_pio_write32(v,a) (*(a) = (v))
#else /* USE_PCI_PIO */
#if defined(FDDI_BONSAI)
#pragma weak pciio_pio_read32  = rns_pciio_pio_read32
#pragma weak pciio_pio_write8  = rns_pciio_pio_write8
#pragma weak pciio_pio_write16 = rns_pciio_pio_write16
#pragma weak pciio_pio_write32 = rns_pciio_pio_write32
uint32_t pciio_pio_read32(volatile uint32_t *addr);
void pciio_pio_write8(uint8_t val, volatile uint8_t *addr);
void pciio_pio_write16(uint16_t val, volatile uint16_t *addr);
void pciio_pio_write32(uint32_t val, volatile uint32_t *addr);
#endif	/* FDDI_BONSAI */
#endif	/* USE_PCI_PIO */
#endif

#include <net/soioctl.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/raw.h>
#include <net/multi.h>
#include <net/route.h>
#ifndef FDDI_BONSAI
#include <net/rsvp.h>
#endif
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <sys/dlsap_register.h>
#include <ether.h>
#include <sys/fddi.h>
#include <sys/smt.h>

extern time_t lbolt;
extern struct ifnet loif;
#if defined(FDDI_FICUS) || defined(FDDI_BONSAI)
extern struct ifqueue ipintrq;		/* IP packet input queue */
#endif

extern int rns_lat_void;
extern int rns_mtu;
extern int rns_low_latency;

#ifndef FDDI_BONSAI
int if_rnsdevflag = D_MP;
#else
int if_rnsdevflag = 0;
#endif

/* these machines have more incoherent caches */
#if defined(IP32SIM) || defined(IP32)
#define NOT_LEGO
/* This belongs in a header file,
 * but I'm tired of arguing about the PCI stuff--vjs */
#ifdef FDDI_BONSAI
extern int pciio_flush_buffers(vertex_hdl_t);
#else
extern int pcimh_flush_buffers(vertex_hdl_t);
#endif
#endif



/* *****************************************************************
 * These definitions could be put in a separate .h file if there is
 * ever another use of them.
 */

#define LOCAL_MEM_BASE	0		/* local memory on port B */
#define LOCAL_MEM_LEN	(128*1024)

struct rns_cfgregs {
    volatile u_int16_t	dev_id;		/* 0x00 device ID */
    volatile u_int16_t	vendor_id;	/* 0x02 vendor ID */
    volatile u_int16_t  status;		/* 0x04 status */
    volatile u_int16_t  cmd;		/* 0x06 command */
    volatile u_char     class_code[3];	/* 0x08 class code */
    volatile u_char     revision_id;	/* 0x0a revision ID */
    volatile u_char     bist;		/* 0x0f BIST */
    volatile u_char     hdr_type;	/* 0x0e header type */
    volatile u_char     latency_tmr;	/* 0x0d latency timer */
    volatile u_char     cache_line;	/* 0x0c cache line */
    volatile __uint32_t base_addr;	/* 0x10 base address */
    volatile __uint32_t base_addr1;	/* 0x14 base address */
    volatile __uint32_t reserved[9];	/* 0x18 reserved */
    volatile u_char     max_lat;	/* 0x3f max LAT */
    volatile u_char     min_gnt;	/* 0x3e min GNT */
    volatile u_char     intr_pin;	/* 0x3d interrupt pin */
    volatile u_char     intr_line;	/* 0x3c interrupt line */
};


typedef struct rns_regs {
    volatile __uint32_t   brd_ctl;	/* 0x00	board control register */
#     define BRD_LED0	    0x0001	/*  LED0 */
#     define BRD_LED1	    0x0002	/*  LED1 */
#     define BRD_LED2	    0x0004	/*  LED2 (not physically present) */
    /* LED0 LED1
     *  off off	    failure		adapter is not functional
     *					    or the driver is not loaded.
     *  off on	    ring is up		Normal operation.
     *  on  off	    cable error		Ring is not operational
     *  on  on	    ring_op/wrapped	DAS only.  WRAP_A or WRAP_B
     */
#     define BRD_RESET	    0x0008	/*  reset IFDDI and ELM */
#     define BRD_EN_2OBR    0x0010	/*  enable secondary OBR */
#     define BRD_EN_1OBR    0x0020	/*  enable primary OBR */
#     define BRD_EN_OBR	(BRD_EN_2OBR | BRD_EN_1OBR)
#     define BRD_ELM_ENCOFF 0x0040	/*  disable ELM encoding */
#     define BRD_TEST	    0x0080	/*  Test output */
#     define BRD_SEL_NVRAM  0x0100	/*  NVRAM bank select */
#     define	BRD_NVRAM_LEN	    8	/* length of each bank of NVRAM */
#     define	BRD_SEL_NVRAM_MASK  0x1f00
#     define	BRD_SEL_NVRAM_SHIFT 8
#     define BRD_ELM_INT_EN 0x2000	/*  enable ELM interrupt */
#     define BRD_CFG_INT_EN 0x4000	/*  enable config interrupt */
#     define BRD_OBR_PRESENT 0x8000	/*  OBR present */

    volatile __uint32_t    brd_int;	/* 0x04	board interrupt register */
#     define BRD_INT_ENABLE 0x0001	/*  enable interrupts */
#     define BRD_INT_CONFIG 0x0002	/*  config interrupt pending */
#     define BRD_INT_IFDDI  0x0004	/*  IFDDI interrupt pending */
#     define BRD_INT_ELM    0x0008	/*  ELM interrupt pending */
#     define BRD_INT_CAMEL  0x0010	/*  CAMEL interrupt pending */

    volatile __uint32_t   brd_fifo;	/* 0x08 FIFO threshold */
    volatile __uint32_t   brd_page;	/* 0x0c 64-bit page number */
    char		pad1[16];
    __uint32_t		brd_nvram[BRD_NVRAM_LEN];   /* 0x20 ID PROM */

    /* IFDDI registers */
    volatile __uint32_t	fsi_nop;	/* 0x40 */
    volatile __uint32_t	fsi_sr1;	/* 0x44 */
#	define	SR1_CDN		(1<<31)
#	define	SR1_CRF		(1<<30)
#	define	SR1_HER		(1<<29)
#	define	SR1_IOE		(1<<28)
#	define	SR1_ROV(r)	(1<<((r)+26-4))
#	define	SR1_POEB	(1<<25)
#	define	SR1_POEA	(1<<24)
#	define	SR1_CIN		(1<<23)
#	define	SR1_STE		(1<<22)
#	define	SR1_RER(r)	(1<<((r)+16))
#	define	SR1_RXC(r)	(1<<((r)+8))
#	define	SR1_RXC_ALL	(SR1_RXC(RX_RING_N) | SR1_RXC(PM_RING_N))
#	define	SR1_RCC(r)	(1<<((r)+8))
#	define	SR1_RNR(r)	(1<<(r))
#	define	SR1_ERRORS (SR1_HER | SR1_IOE | SR1_POEB | SR1_POEA	\
			    | SR1_RER(5) | SR1_RER(4) | SR1_RER(3)	\
			    | SR1_RER(2) | SR1_RER(1) | SR1_RER(0))
#	define	SR1_OUTPUT  (SR1_RNR(TX_RING_N) | SR1_RCC(TX_RING_N))
#	define  INIT_IMR1 (SR1_ERRORS | SR1_CIN				\
			    | SR1_RXC_ALL | SR1_ROV(RX_RING_N) | SR1_STE)
#	define  IFDDI_IMR1 (INIT_IMR1 | SR1_OUTPUT)
    volatile __uint32_t	fsi_dtr;	/* 0x48 */
    volatile __uint32_t	fsi_ior;	/* 0x4c */
    volatile __uint32_t	fsi_badr;	/* 0x50 */
    volatile __uint32_t	fsi_imr1;	/* 0x54 */
    volatile __uint32_t	fsi_rsvd1;	/* 0x58 */
    volatile __uint32_t	fsi_psr;	/* 0x5c */
    volatile __uint32_t	fsi_rsvd2;	/* 0x60 */
    volatile __uint32_t	fsi_cmr;	/* 0x64 */
    volatile __uint32_t	fsi_pnop;	/* 0x68 */
    volatile __uint32_t	fsi_cer;	/* 0x6c */
    volatile __uint32_t	fsi_imr2;	/* 0x70 */
    volatile __uint32_t	fsi_sr2;	/* 0x74 */
    volatile __uint32_t	fsi_rsvd3;	/* 0x78 */
    volatile __uint32_t	fsi_fcr;	/* 0x7c */

    /* external ELM registers for DAS board */
    volatile __uint32_t	ext_elm[32];	/* 0x80-0xdc */
} rns_regs_t;

#define RD_REG(ri, nm)	    pciio_pio_read32(&ri->ri_regs->nm)
#define WT_REG(ri, nm, v)   pciio_pio_write32(v, &ri->ri_regs->nm)


/* The data in the NVRAM on the board after being decoded from 4-bit
 * nibbles.
 *
 * According to the RNS driver, the NVRAM can be viewed as a packed structure
 *	48 bits of MAC address
 *	24 bits of part number
 *	12 bits of revision
 *	16 bits of customer ID
 *	24 bits of hardware part number
 * The SGI compiler insists on inserting padding between bit fields, so
 * the obvious solution is not available.
 */
struct ri_nvram {
    u_char  nvram_mac[6];		/* MAC address in canonical order */
    uint    nvram_part_no;
    uint    nvram_rev;
    uint    nvram_cust_id;
    uint    nvram_hw_part_no;
};


typedef	__uint64_t  fsi_cmd_t;		/* IFDDI command*/
typedef volatile struct {		/* ring descriptor */
    __uint32_t	    hi;			/* first,last,etc bits and length */
    __uint32_t	    a;
} fsi_desc_t;

/* FSI internal registers reached through the FCR */
#define	FSI_R	    (1<<31)
#define FCR_DATA    16
#define IRT_IRI(irt,iri,v) (((irt)<<27) | ((iri)<<24)	\
			    | (((v) & 0xff) << FCR_DATA))

#define IRT_IRI_MASK IRT_IRI(0xf,7,0)

#define	FSI_PCRA(v) IRT_IRI(0xb,6,v)	/* port A control */
#define	FSI_PCRB(v) IRT_IRI(0xb,7,v)	/* port B */
#define	    FSI_PCR_HPE	(1<<7)
#define	    FSI_PCR_PC	(1<<6)
#define	    FSI_PCR_SO	(1<<5)
#define	    FSI_PCR_DO	(1<<4)
#define	    FSI_PCR_DW	(1<<2)
#define	    FSI_PCR_PE	(1<<0)
#define	    FSI_PCR_OFF (FSI_PCR_SO)	/* 32-bits, no parity, sync */
#define	    FSI_PCR_ON (FSI_PCR_SO | FSI_PCR_PE)
#define	FSI_PMPA(v) IRT_IRI(0x8,6,v)	/* port A mem page */
#define	FSI_PMPB(v) IRT_IRI(0x8,7,v)	/* port B */
#define	FSI_RPR(r,v) IRT_IRI(0x6,r,v)	/* ring parameters registers */
#define	    FSI_RPR_RPE	(1<<7)		/*     Ring Parity Enable */
#define	    FSI_RPR_DPE	(1<<6)		/*     Data Parity Enable */
#define	    FSI_RPR_RPA (1<<5)		/*     Ring Port Assignment=port B */
#define	    FSI_RPR_RDA	(1<<4)		/*     Ring Data Assignment=port B */
#define FSI_PER(r,v) IRT_IRI(0x7,r,v)	/* parameter extension registers */
#define FSI_CPR(r,v) IRT_IRI(0x6,r,v)	/* command parameters registers */
#define	FSI_RSR(r)   IRT_IRI(0xe,r,0)	/* ring states */
#define	    FSI_RSR_EX	(1<<7)		/*     exists */
#define	    FSI_RSR_PER	(1<<6)		/*     parity error */
#define	    FSI_RSR_OER	(1<<5)		/*     operation error */
#define	    FSI_RSR_STP	(1<<3)		/*     stopped state */
#define	    FSI_RSR_RDY	(1<<2)		/*     ready state */
#define	    FSI_RSR_EMP	(1<<1)		/*     empty state */
#define	    FSI_RSR_CPL	(1<<0)		/*     complete state */
#define	FSI_FWR(r,v) IRT_IRI(0x4,r,v)	/* FIFO watermarks */
#define	    FSI_FWR_MIN 2
#define	    FSI_MEM_BLK 32
#define	FSI_LMT(r,v) IRT_IRI(0x5,r,v)	/* limit */
#define	FSI_RFR(r,v) IRT_IRI(0x1,r,v)	/* RX frame types */
#define	    FSI_RFR_DATA 0x3a		/*     OT,LS,LA,SF */
#define	    FSI_RFR_MAC  0x4		/*     MA */
#define	FSI_RBR(r,v) IRT_IRI(0x3,r,v)	/* RX buffer lengths */
#define	    FSI_RBR_MIN (1<<6)
#define	FSI_RMR(r,v) IRT_IRI(0xc,r,v)	/* RX mem space */
#define	    FSI_RMR_MIN 10
#define	    FSI_RMR_MAX 255
#define	FSI_MTR(v)  IRT_IRI(0x1,0,v)	/* MACIF TX control */
#define	    FSI_MTR_TE	(1<<0)
#define	FSI_MRR(v)  IRT_IRI(0x1,1,v)	/* MACIF RX control */
#define	    FSI_MRR_RPE	(1<<7)
#define	    FSI_MRR_RMI	(1<<5)
#define	    FSI_MRR_RAL	(1<<4)
#define	    FSI_MRR_RCM	(1<<3)
#define	    FSI_MRR_ROB	(1<<2)
#define	    FSI_MRR_RE5	(1<<1)
#define	    FSI_MRR_RE4	(1<<0)
#define	    FSI_MRR_RX  (1<<(RX_RING_N-4))
#define	    FSI_MRR_PM  (1<<(PM_RING_N-4))
#define	FSI_RDY(r)  IRT_IRI(0x0,r,0)	/* ring ready */
#define	FSI_IER	    IRT_IRI(0xf,0,0)	/* error status */
#define	    FSI_IER_IOE	(1<<7)
#define	    FSI_IER_IUE	(1<<6)
#define	    FSI_IER_TPE	(1<<5)
#define	    FSI_IER_MER	(1<<4)
#define	    FSI_IER_MOV	(1<<3)
#define	    FSI_IER_PBE	(1<<1)
#define	    FSI_IER_PAE	(1<<0)
#define	FSI_SWR	    IRT_IRI(0xf,7,0)	/* FSI reset */
#define	FSI_BLRA(v) IRT_IRI(0x3,6,v)	/* burst limit registers */
#define	FSI_BLRB(v) IRT_IRI(0x3,7,v)
#define	FSI_ICR(v)  IRT_IRI(0x1,2,v)	/* IFDDI configuration register */
#define	    FSI_ICR_DBM	(1<<0)
#define	    FSI_ICR_CEO	(1<<1)
#define	    FSI_ICR_CSM	(1<<2)
#define	    FSI_ICR_COM	(1<<3)
#define	    FSI_ICR_CDS	(1<<4)
#define	    FSI_ICR_CTE	(1<<5)
#define FSI_STR0    IRT_IRI(0x2,2,0)	/* SMT timer read */
#define FSI_STR1    IRT_IRI(0x2,3,0)
#define FSI_STL0(v) IRT_IRI(0x2,0,v)	/* SMT timer load */
#define FSI_STL1(v) IRT_IRI(0x2,1,v)
#define	FSI_IREV    IRT_IRI(0xf,3,0)	/* IFDDI revision */
#define	FSI_REV	    IRT_IRI(0xf,2,0)	/* FSI revision */

/* a CAMEL register through the FCR */
#define FCR_CAMEL(r,v) (IRT_IRI(0xa,0,r) | v)


#define CMR_OWN_BIT	(1<<31)		/* "own" bit in FSI_CMR */

#define FSI_CMD_RX	    0x8000	/* receive buffer descriptor */
#define FSI_CMD_TX1	    0xb000	/* transmit a one-buffer packet */
#define	FSI_CMD_NOP	    0xbf00
#define	FSI_CMD_CRW	    0xb600	/* control register write */
#define	FSI_CMD_DEFR	    0xbe00	/* define ring */
#define	FSI_CMD_DEFR_LOC    0xbe40	/* define ring with local memory */
#define	FSI_CMD_DEFR_TX_LOC 0xbe60	/* define tx ring with local memory */
#define	FSI_CMD_DEFRDY	    0xbe08	/* define and make ring ready */
#define	FSI_CMD_DEFRDY_LOC  0xbe48	/* define ready ring w/local memory */
#define FSI_CMD_SET_LOCAL   0xbe18	/* set local memory start */
#define	FSI_CMD_RSTOP	    0xbe20	/* stop ring */
#define	FSI_CMD_RPARM	    0xbe80	/* read ring parameters */
#define	FSI_CMD_RRESET	    0xbea0	/* ring reset */
#define	FSI_CMD_CLR_CAM     0xb900	/* clear CAM */
#define	FSI_CMD_SET_CAM     0xb980	/* set CAM */
#define	FSI_CMD_RD_CAM	    0xba00	/* read CAM */
#define	FSI_CMD_CMP_CAM	    0xbb80	/* probe CAM */

/* make a typical ring-related command
 * c=command, r=ring #, h=high address or count, p=pointer or address
 */
#define	FSI_CMD(c,r,h,p) ((((__uint64_t)((c) | (r)))<<48)	    \
			  | ((((__uint64_t)(h)) & 0xffff)<<32)	    \
			  | ((__psunsigned_t)(p)) & 0xffffffff)

#define FSI_DESC_HI(c,l)    (((c)<<16) | l)
#define FSI_DESC_A(a)	    ((__psunsigned_t)(a))

#define FSI_IND_OWN	    (1<<(63-32))
#define FSI_IND_FIRST	    (1<<(61-32))
#define FSI_IND_LAST	    (1<<(60-32))
#define	FSI_IND_LEN_MASK    0x1fff

#define FSI_IND_RCV_PEI	    (1<<(59-32))    /* receive port error indication */
#define FSI_IND_RCV_PEI_PER_MASK 0xf
#define	FSI_IND_RCV_ERR	    (1<<(58-32))    /* receive error */
#define	FSI_IND_RCV_OE	    (1<<(57-32))    /* overrun error in the FSI */
#define	FSI_IND_RCV_CRC	    (1<<(56-32))    /* CRC (or parity) error */
#define	FSI_IND_FNUM_MSB    (1<<(55-32))    /* # of flag bits */
#define	FSI_IND_FNUM_LSB    (1<<(53-32))
#define	FSI_IND_FNUM_MASK   (7*FSI_IND_FNUM_LSB)
#define	FSI_IND_FNUM_MIN    (3*FSI_IND_FNUM_LSB)
#define	FSI_IND_RCV_E	    (1<<(52-32))    /* E flag */
#define	FSI_IND_RCV_A	    (1<<(51-32))    /* A flag */
#define	FSI_IND_RCV_C	    (1<<(50-32))    /* C flag */
#define	FSI_IND_RCV_DA_MSB  (1<<(47-32))    /* dest address match field */
#define	FSI_IND_RCV_DA_LSB  (1<<(46-32))
#define	FSI_IND_RCV_LEN_MASK 0x1fff	/* frame length */

#define	FSI_IND_RCV_CNT (FSI_IND_RCV_CRC | FSI_IND_RCV_E \
			 | FSI_IND_RCV_A | FSI_IND_RCV_C)

#define	FSI_IND_TX_PER_MASK (7<<(51-32))    /* port error */
#define	FSI_IND_TX_AB	    (1<<(50-32))    /* TX Abort */
#define	FSI_IND_TX_UN	    (1<<(49-32))    /* underrun */
#define	FSI_IND_TX_PE	    (1<<(48-32))    /* party error */

			 /* frame was aborted */
#define	FSI_IND_TX_BAD	(FSI_IND_TX_PPE | FSI_IND_TX_PAE | FSI_IND_TX_AB \
			 | FSI_IND_TX_UN | FSI_IND_TX_PE)

/* CAM definitions */
#define FSI_CAM_LEN	32		/* 32 entries */
#define	FSI_CAM_CONST	0xb900		/* 48-bit */
#define FSI_CAM_VALID	0x0080		/* valid entry */
union fsi_cam {
    struct {
	ushort	id;
	ushort	entry[3];
    } s;
    __uint64_t	d;
};


/* ELM addresses */
#define	ELM_CTRL_A	0x00
#define	    ELM_CA_NOISE_TIMER  (1<<14)
#define	    ELM_CA_TNE_16BIT    (1<<13)
#define	    ELM_CA_TPC_16BIT    (1<<12)
#define	    ELM_CA_REQ_SCRUB    (1<<11)
#define	    ELM_CA_ENA_PAR_CHK  (1<<10)
#define	    ELM_CA_VSYM_CTR_INTRS (1<<9)
#define	    ELM_CA_MINI_CTR_INTRS (1<<8)
#define	    ELM_CA_FCG_LOC_LOOP (1<<7)
#define	    ELM_CA_FOT_OFF	(1<<6)
#define	    ELM_CA_EB_LOC_LOOP  (1<<5)
#define	    ELM_CA_LOC_LOOP	(1<<4)
#define	    ELM_CA_SC_BYPASS    (1<<3)
#define	    ELM_CA_REM_LOOP	(1<<2)
#define	    ELM_CA_RF_DISABLE   (1<<1)
#define	    ELM_CA_RUN_BIST	(1<<0)
#define	    ELM_CA_DEFAULT (ELM_CA_NOISE_TIMER | ELM_CA_VSYM_CTR_INTRS \
			    | ELM_CA_MINI_CTR_INTRS)
#define	ELM_CTRL_B	0x01
#define	    ELM_CB_CONFIG_CNTRL (1<<15)
#define	    ELM_CB_MATCH_LS_LSB (1<<11)
#define	    ELM_CB_MATCH_LS_MASK (0xf*ELM_CB_MATCH_LS_LSB)
#define	    ELM_CB_MATCH_QLS    (1<<14)
#define	    ELM_CB_MATCH_MLS    (1<<13)
#define	    ELM_CB_MATCH_HLS    (1<<12)
#define	    ELM_CB_MATCH_ILS    (1<<11)
#define	    ELM_CB_LS_TX_LSB    (1<<8)
#define	    ELM_CB_LS_TX_MASK   (7*ELM_CB_LS_TX_LSB)
#define	    ELM_CB_TX_QLS	(0*ELM_CB_LS_TX_LSB)
#define	    ELM_CB_TX_ILS	(1*ELM_CB_LS_TX_LSB)
#define	    ELM_CB_TX_HLS	(2*ELM_CB_LS_TX_LSB)
#define	    ELM_CB_TX_MLS	(3*ELM_CB_LS_TX_LSB)
#define	    ELM_CB_TX_ALS	(6*ELM_CB_LS_TX_LSB)
#define	    ELM_CB_CLASS_S	(1<<7)
#define	    ELM_CB_PC_LOOP_LSB  (1<<5)
#define	    ELM_CB_PC_LOOP_MASK (3*ELM_CB_PC_LOOP_LSB)
#define	    ELM_CB_PC_LOOP	(3*ELM_CB_PC_LOOP_LSB)
#define	    ELM_CB_PC_JOIN	(1<<4)
#define	    ELM_CB_LONG		(1<<3)
#define	    ELM_CB_PC_MAINT	(1<<2)
#define	    ELM_CB_PCM_CNTRL_LSB (1<<0)
#define	    ELM_CB_PCM_START    (1*ELM_CB_PCM_CNTRL_LSB)
#define	    ELM_CB_PCM_TRACE    (2*ELM_CB_PCM_CNTRL_LSB)
#define	    ELM_CB_PCM_STOP	(3*ELM_CB_PCM_CNTRL_LSB)
#define	    ELM_CB_LCT		(ELM_CB_PC_LOOP | ELM_CB_LONG)
#define	ELM_MASK	0x02
#define	ELM_XMIT_VEC	0x03
#define	ELM_XMIT_LEN	0x04
#define	ELM_LE_THRESH	0x05
#define	ELM_A_MAX	0x06
#define	ELM_LS_MAX	0x07
#define	ELM_TB_MIN	0x08
#define	ELM_T_OUT	0x09
#define	ELM_CIPHER	0x0a
#define	    ELM_CPI_PRO_TEST	    (1<<14)
#define	    ELM_CPI_SDNRZEN	    (1<<8)
#define	    ELM_CPI_SDONEN	    (1<<7)
#define	    ELM_CPI_SDOFFEN	    (1<<6)
#define	    ELM_CPI_RXDATA_ENABLE   (1<<5)
#define	    ELM_CPI_FOTOFF_SOURCE   (1<<4)
#define	    ELM_CPI_FOTOFF_CONTROL  (1<<2)
#define	    ELM_CPI_CIPHER_LBACK    (1<<1)
#define	    ELM_CPI_CIPHER_ENABLE   (1<<0)
#define	ELM_LC_SHORT	0x0b
#define	ELM_T_SCRUB	0x0c
#define	ELM_NS_MAX	0x0d
#define	ELM_TPC_LOAD	0x0e
#define	ELM_TNE_LOAD	0x0f
#define	ELM_STATUS_A	0x10
#define	    ELM_SA_REV_LSB  (1<<11)
#define	    ELM_SA_REV_MASK 0x1f
#define	    ELM_SA_REV_B    0x00    /* rev B */
#define	    ELM_SA_REV_D    0x11    /* rev D */
#define	    ELM_SA_SD	    (1<<10)
#define	    ELM_PREV_LS_LSB 8
#define	    ELM_PREV_LS_MASK (3<<ELM_PREV_LS_LSB)
#define	    ELM_PREV_LS_QLS (0<<ELM_PREV_LS_LSB)
#define	    ELM_PREV_LS_MLS (1<<ELM_PREV_LS_LSB)
#define	    ELM_PREV_LS_HLS (2<<ELM_PREV_LS_LSB)
#define	    ELM_PREV_LS_ILS (3<<ELM_PREV_LS_LSB)
#define	    ELM_LS_LSB	    5
#define	    ELM_LS_MASK	    (7<<ELM_LS_LSB)
#define	    ELM_LS_NLS	    (0<<ELM_LS_LSB)
#define	    ELM_LS_ALS	    (1<<ELM_LS_LSB)
#define	    ELM_LS_ILS4	    (3<<ELM_LS_LSB)
#define	    ELM_LS_QLS	    (4<<ELM_LS_LSB)
#define	    ELM_LS_MLS	    (5<<ELM_LS_LSB)
#define	    ELM_LS_HLS	    (6<<ELM_LS_LSB)
#define	    ELM_LS_ILS	    (7<<ELM_LS_LSB)
#define	    ELM_LSM_STATE_LSB (1<<4)
#define	    ELM_U_LS	    (1<<3)
#define	ELM_STATUS_B	0x11
#define	    ELM_SB_RF_STATE (1<<14)
#define	    ELM_SB_PCI_STATE (1<<12)
#define	    ELM_SB_PCI_SCRUB (1<<11)
#define	    ELM_SB_PCM_STATE (1<<7)
#define	    ELM_SB_PCM_STATE_MASK (0xf*ELM_SB_PCM_STATE)
#define	    ELM_SB_PCM_SIG  (1<<6)
#define	    ELM_SB_LSF	    (1<<5)
#define	    ELM_SB_RCF	    (1<<4)
#define	    ELM_SB_TCF	    (1<<3)
#define	    ELM_SB_BREAK    (1<<0)
#define	    ELM_SB_BREAK_MASK (7*ELM_SB_BREAK)
#define	    ELM_SB_BREAK_PC_START (1*ELM_SB_BREAK)
#define	ELM_TPC		0x12
#define	ELM_TNE		0x13
#define	ELM_CLK_DIV	0x14
#define	ELM_BIST_SIG	0x15
#define	ELM_RCV_VEC	0x16
#define	ELM_EVENT	0x17
#define	    ELM_EV_SD		(1<<14)
#define	    ELM_EV_LE_CTR	(1<<13)
#define	    ELM_EV_MINI_CTR	(1<<12)
#define	    ELM_EV_VSYM_CTR	(1<<11)
#define	    ELM_EV_PHYINV	(1<<10)
#define	    ELM_EV_EBUF		(1<<9)
#define	    ELM_EV_TNE		(1<<8)
#define	    ELM_EV_TPC		(1<<7)
#define	    ELM_EV_PCM_ENABLED  (1<<6)
#define	    ELM_EV_PCM_BREAK    (1<<5)
#define	    ELM_EV_PCM_ST	(1<<4)
#define	    ELM_EV_PCM_TRACE    (1<<3)
#define	    ELM_EV_PCM_CODE	(1<<2)
#define	    ELM_EV_LS		(1<<1)
#define	    ELM_EV_PARITY_ERR   (1<<0)
/*	    event mask during host PCM signaling */
#define	    ELM_EV_SIG (ELM_EV_LS | ELM_EV_TPC | ELM_EV_PCM_ENABLED \
			| ELM_EV_PCM_BREAK | ELM_EV_PCM_ST	    \
			| ELM_EV_PCM_TRACE | ELM_EV_PCM_CODE)
/*	    event mask while active */
#define	    ELM_EV_ACT (ELM_EV_SIG | ELM_EV_LE_CTR | ELM_EV_EBUF | ELM_EV_TNE)
#define	ELM_VIOL_SYM	0x18
#define	ELM_MIN_IDLE	0x19
#define	ELM_LINK_ERR	0x1a
#define	ELM_FOTOFF_ON	0x1e
#define	ELM_FOTOFF_OFF	0x1f

/* The "TPC-based timing parameters registers of the ELM timers are in 256*80
 * nsec units instead of microseconds.
 */
#define TPC_TIME(u) (-(((u)*100+8*256-1)/(8*256)))

/* There is also the noise timer, in 4*80 nsec units
 */
#define NOISE_TIME(u) (-(((u)*100)/(8*4)))


/* MAC registers */
#define	MAC_CTRL_A	0x40
#define	    MAC_CA_ON	    (1<<15)
#define	    MAC_CA_REV_ADDR	(1<<12)
#define	    MAC_CA_FLSH_SA47    (1<<11)
#define	    MAC_CA_COPY_ALL2    (1<<10)
#define	    MAC_CA_COPY_ALL1    (1<<9)
#define	    MAC_CA_COPY_OWN	(1<<8)
#define	    MAC_CA_COPY_ALLSMT  (1<<7)
#define	    MAC_CA_COPY_SMT	(1<<6)
#define	    MAC_CA_COPY_LLC	(1<<5)  /* promiscuous */
#define	    MAC_CA_COPY_GRP	(1<<4)  /* all-multicast */
#define	    MAC_CA_DSABL_BRDCST (1<<3)
#define	    MAC_CA_RUN_BIST	(1<<2)
#define	    MAC_CA_DEFAULT (MAC_CA_ON | MAC_CA_FLSH_SA47 | MAC_CA_COPY_SMT)
#define	    MAC_CA_OFF (MAC_CA_DEFAULT | MAC_CA_DSABL_BRDCST)
#define	    MAC_CA_PROM (MAC_CA_DEFAULT | MAC_CA_COPY_ALL2  \
			 | MAC_CA_COPY_ALLSMT		    \
			 | MAC_CA_COPY_LLC | MAC_CA_COPY_GRP)
#define	MAC_CTRL_B	0x41
#define	    MAC_CB_RING_PURGE   (1<<15)
#define	    MAC_CB_FDX		(1<<14)
#define	    MAC_CB_B_STRIP	(1<<13)
#define	    MAC_CB_TXPARITY	(1<<12)
#define	    MAC_CB_REPEAT	(1<<11)
#define	    MAC_CB_LOSE_CLAIM   (1<<10)
#define	    MAC_CB_RESET	(1<<8)
#define	    MAC_CB_RESET_BEACON (1<<9)
#define	    MAC_CB_RESET_CLAIM  (MAC_CB_RESET | MAC_CB_RESET_BEACON)
#define	    MAC_CB_FSI_BEACON   (1<<7)
#define	    MAC_CB_DELAY_TOK    (1<<6)
#define	    MAC_CB_NO_SCAM	(1<<5)
#define	    MAC_CB_DEFAULT (MAC_CB_TXPARITY | MAC_CB_NO_SCAM)
#define	    MAC_CB_OFF (MAC_CB_LOSE_CLAIM | MAC_CB_REPEAT | MAC_CB_RESET_CLAIM)
#define	    MAC_CB_OFF_BIT	MAC_CB_LOSE_CLAIM
#define	MAC_MASK_A	0x42
#define	    MAC_MASK_A_DEFAULT (MAC_EVA_FRAME_ERR | MAC_EVA_FRAME_RCVD \
				| MAC_EVA_DBL_OVFL | MAC_EVA_RNGOP_CHG \
				| MAC_EVA_TVX_EXPIR | MAC_EVA_LATE_TKN \
				| MAC_EVA_RCVRY_FAIL | MAC_EVA_DUPL_TKN \
				| MAC_EVA_DUPL_ADDR)
#define	MAC_MASK_B	0x43
#define	    MAC_MASK_B_BUG (MAC_EVB_SI_ERR | MAC_EVB_FDX_CHG)
#define	    MAC_MASK_B_DEFAULT (MAC_MASK_B_BUG | MAC_EVB_BAD_BID \
				| MAC_EVB_PURGE_ERR | MAC_EVB_WON_CLAIM \
				| MAC_EVB_NOT_COPIED)
#define	    MAC_MASK_B_MORE (MAC_EVB_MY_BEACON | MAC_EVB_OTR_BEACON \
			     | MAC_EVB_HI_CLAIM | MAC_EVB_LO_CLAIM \
			     | MAC_EVB_MY_CLAIM)
#define	MAC_MASK_C	0x44
#define	    MAC_MASK_C_DEFAULT (MAC_EVC_VOID_RDY | MAC_EVC_VOID_OVF \
				| MAC_EVC_TKN_CNT_OVF)
#define MAC_MSA		0x50
#define	MAC_MLA_A	0x51
#define	MAC_MLA_B	0x52
#define	MAC_MLA_C	0x53
#define	MAC_T_REQ	0x54
#define	MAC_TVX_T_MAX	0x55
#define MAC_REV_NO	0x5c
#define	MAC_EVNT_C	0x5d
#define	    MAC_EVC_VOID_RDY    (1<<2)
#define	    MAC_EVC_VOID_OVF    (1<<1)
#define	    MAC_EVC_TKN_CNT_OVF (1<<0)
#define	MAC_VOID_TIME	0x5e
#define	MAC_TOKEN_CT	0x5f
#define	MAC_FRAME_CT	0x60
#define	MAC_LOST_ERR	0x61
#define	MAC_EVNT_A	0x62
#define	    MAC_EVA_PH_INVAL    (1<<15)
#define	    MAC_EVA_UTOK_RCVD   (1<<14)
#define	    MAC_EVA_RTOK_RCVD   (1<<13)
#define	    MAC_EVA_TKN_CAP	(1<<12)
#define	    MAC_EVA_BEACON_RCVD (1<<11)
#define	    MAC_EVA_CLAIM_RCVD  (1<<10)
#define	    MAC_EVA_FRAME_ERR   (1<<9)
#define	    MAC_EVA_FRAME_RCVD  (1<<8)
#define	    MAC_EVA_DBL_OVFL    (1<<7)
#define	    MAC_EVA_RNGOP_CHG   (1<<6)
#define	    MAC_EVA_BAD_T_OPR   (1<<5)
#define	    MAC_EVA_TVX_EXPIR   (1<<4)
#define	    MAC_EVA_LATE_TKN    (1<<3)
#define	    MAC_EVA_RCVRY_FAIL  (1<<2)
#define	    MAC_EVA_DUPL_TKN    (1<<1)
#define	    MAC_EVA_DUPL_ADDR   (1<<0)
#define	MAC_EVNT_B	0x63
#define	    MAC_EVB_MY_BEACON   (1<<15)
#define	    MAC_EVB_OTR_BEACON  (1<<14)
#define	    MAC_EVB_HI_CLAIM    (1<<13)
#define	    MAC_EVB_LO_CLAIM    (1<<12)
#define	    MAC_EVB_MY_CLAIM    (1<<11)
#define	    MAC_EVB_BAD_BID	(1<<10)
#define	    MAC_EVB_PURGE_ERR   (1<<9)
#define	    MAC_EVB_BS_ERR	(1<<8)
#define	    MAC_EVB_WON_CLAIM   (1<<7)
#define	    MAC_EVB_SI_ERR	(1<<5)
#define	    MAC_EVB_NOT_COPIED  (1<<4)
#define	    MAC_EVB_FDX_CHG	(1<<3)
#define	    MAC_EVB_BIT4_I_SS   (1<<2)
#define	    MAC_EVB_BIT5_I_SS   (1<<1)
#define	    MAC_EVB_BAD_CRC_TX  (1<<0)
#define	MAC_RX_STATUS	0x60
#define	MAC_TX_STATUS	0x65
#define	    MAC_TX_ST_FSM_LSB   (1<<12)
#define	    MAC_TX_ST_BEACON    (0x6*MAC_TX_ST_FSM_LSB)
#define	    MAC_TX_ST_CLAIM	(0x7*MAC_TX_ST_FSM_LSB)
#define	    MAC_TX_ST_FSM_MASK  (0xf*MAC_TX_ST_FSM_LSB)
#define	    MAC_TX_ST_RNGOP	(1<<11)
#define	    MAC_TX_ST_PURGING   (1<<10)
#define	MAC_T_NEG_A	0x66
#define	MAC_T_NEG_B	0x67
#define	MAC_INFO_A	0x68
#define	MAC_INFO_B	0x69
#define	MAC_TVX_TIMER	0x6b
#define	MAC_TRT_TIMER_A	0x6c
#define	MAC_TRT_TIMER_B	0x6d
#define	MAC_THT_TIMER_A	0x6e
#define	MAC_THT_TIMER_B	0x6f


/* The 3 bytes in memory before the FC are the "packet request header" */
#define	PRH_CTOK_MSB	    (1<<(5+16))
#define	PRH_CTOK_LSB	    (1<<(4+16))
#define	PRH_CTOK_IMM	    (0*PRH_CTOK_LSB)
#define	PRH_CTOK_RES	    (1*PRH_CTOK_LSB)
#define	PRH_CTOK_UNR	    (2*PRH_CTOK_LSB)
#define	PRH_CTOK_ANY	    (3*PRH_CTOK_LSB)
#define	PRH_SYNCH	    (1<<(3+16))
#define	PRH_IMM_MODE	    (1<<(2+16))
#define	PRH_SEND_FIRST	    (1<<(1+16))
#define	PRH_BCN		    (1<<(0+16))
#define	PRH_SEND_LAST	    (1<<(6+8))
#define	PRH_CRC		    (1<<(5+8))
#define	PRH_STOK_LSB	    (1<<(3+8))
#define	PRH_STOK_NONE	    (0*PRH_STOK_LSB)
#define	PRH_STOK_UNR	    (1*PRH_STOK_LSB)
#define	PRH_STOK_RES	    (2*PRH_STOK_LSB)
#define	PRH_STOK_SAME	    (3*PRH_STOK_LSB)

#define MAC_DATA_HDR	(PRH_CTOK_ANY | PRH_CRC | PRH_STOK_UNR)
#define MAC_BEACON_HDR	(PRH_BCN | PRH_CRC)


/* CAMEL "core general registers" */
#define CAMEL_CNTRL	0x80
#define	    CAMEL_BY_CNTRL  0x0001
#define CAMEL_MASK	0x81
#define CAMEL_INTR	0x84
#define	    CAMEL_NP_ERR    0x0001
#define CAMEL_INT_LOC	0x86
#define	    CAMEL_CAMEL_INT 0x0004
#define	    CAMEL_MAC_INT   0x0002
#define	    CAMEL_ELM_INT   0x0001
#define CAMEL_REV_NO	0x87


/* *************************************************************** */

/* This is from the RNS product_tbl.h */

/*
 * The FDDI Product Table describes each adaptor according to part
 * number. An entry consists of three fields: the 6 digit part number;
 * an attributes field formed by combining the values for media type,
 * port count, connector type, and whether or not a cam is present;
 * and the product name.
 * Note: the attributes values are defined in getid.h
 */

typedef struct product_tbl {
    uint    p_part_no;			/* the adaptor's part no */
    u_char  p_attr;			/* the adaptor's attributes  */
    char    p_product[12];		/* the product's name */
} product_tbl_t;

#define RNS_UTP	0x1
#define RNS_DAS	0x2

static product_tbl_t product_tbl[] = {
    {0x401123,	RNS_DAS,"2200FD"},
    {0x401124,	0,	"2200FSS"},
    {0x401125,	0,	"2200FSM"},
    {0x401126,	RNS_UTP|RNS_DAS,"2200UTPD"},
    {0x401127,	RNS_UTP,"2200UTPS"},
    {0x401132,	0,	"2250FSS"},
    {0x401133,	0,	"2250FSM"},
    {0x401134,	RNS_DAS,"2250FD"},
    {0x401135,	RNS_UTP,"2250UTPS"},
    {0x401141,	RNS_DAS,"2200FDB"},
    {0x401142,	RNS_DAS,"2200DST"},
    {0x401143,	0,	"2200SST"},
    {0x401148,	0,	"2250FSSB"},
    {0x401149,	RNS_DAS,"2250FDB"},
    {0x401151,	0,	"2250SSTB"},
    {0x401152,	RNS_DAS,"2250DSTB"},
    {0x401153,	0,	"2250SST"},
    {0x401154,	RNS_DAS,"2250DST"},
    {0x401155,	0,	"2200FSSB"},
    {0x401156,	0,	"2200FSMB"},
    {0x401157,	RNS_DAS,"2200DSTB"},
    {0x401158,	0,	"2200SSTB"},
    {0x401159,	RNS_UTP,"2200UTPSB"},
    {0x401160,	RNS_UTP|RNS_DAS,"2200UTPDB"},
    {0,		0,	"unknown"},
};

/* ***************************************************************** */


/* -DRNS_DEBUG to generate some debugging code */

#if defined(RNS_DEBUG) || defined(ATTACH_DEBUG)
#define SHOWCONFIG 1
#else
#define SHOWCONFIG showconfig
#endif

/* Allow this many boards. */
#define RNS_MAXBD   8

static char *rns_name = "rns";

#define DEF_T_REQ (FDDI_DEF_T_REQ & ~0xff)
#define MIN_T_REQ MIN(FDDI_MIN_T_MIN & ~0xff, -0x100)
#define MAX_T_REQ (-0xffff00)
#define DEF_T_MAX MIN_T_MAX
#define MIN_T_MAX (FDDI_MIN_T_MAX & ~0xffff)
#define MAX_T_MAX (MAX_T_REQ & ~0xffff)
#define DEF_TVX (FDDI_DEF_TVX & ~0xff)
#define MIN_TVX (FDDI_MIN_TVX & ~0xff)
#define MAX_TVX MAX(FDDI_MAX_TVX & ~0xff, -0xff00)

#define CK_IFNET(ri) ASSERT(IFNET_ISLOCKED(&(ri)->ri_if))


#define SMT1(smt,ri)	(&(ri)->ri_smt1 == (smt))
#define OTHER_SMT(ri,smt) (SMT1(smt,ri) ? &(ri)->ri_smt2 : &(ri)->ri_smt1)

/* count something common to the station */
#define UPCNT(ri,cnt,n) (*(__uint64_t*)&(ri)->ri_smt1.smt_st.cnt += (n))
/* count for one PCM/PHY/etc */
#define UPCNT_PCM(ri,cnt,p,n) (*(__uint64_t*)&(ri)->ri_smt ## n.smt_st.cnt \
			       += (n))

#define IS_DAS(ri)	((ri)->ri_smt1.smt_conf.type == SM_DAS)
#define IS_SA(ri)	((ri)->ri_smt1.smt_conf.type == SAS)

#define ZAPBD(ri) {							\
	(ri)->ri_brd_ctl = ((ri)->ri_brd_ctl & ~(BRD_EN_OBR		\
						 | BRD_CFG_INT_EN	\
						 | BRD_ELM_INT_EN	\
						 | BRD_LED0 | BRD_LED1)); \
	WT_REG(ri, brd_ctl, (ri)->ri_brd_ctl | BRD_RESET);		\
	DELAY(4);							\
	WT_REG(ri, brd_ctl, (ri)->ri_brd_ctl);				\
	WT_REG(ri, brd_int, 0);						\
	(ri)->ri_state |= RI_ST_ZAPPED;					\
}



extern char *smt_pc_label[];
static char *str_ls[] = PCM_LS_LABELS;	/* printf labels for line states */

extern int smt_debug_lvl;

#define DEBUGPRINT(ri,a) {if ((ri)->ri_if.if_flags&IFF_DEBUG) printf a;}

extern void bitswapcopy(void *, void *, int);
#if defined(FDDI_FICUS) || defined(FDDI_BONSAI)
extern int looutput(struct ifnet *, struct mbuf *, struct sockaddr *);
#else /* !FDDI_FICUS && !FDDI_BONSAI */
extern int looutput(struct ifnet *, struct mbuf *, struct sockaddr *,
		    struct rtentry *);
#endif


struct rns_info;			/* forward reference */

int if_rnsattach(vertex_hdl_t);

static void rns_setvec(struct smt*, struct rns_info*, int);
static void rns_join(struct smt*, struct rns_info*);

static void rns_start(struct rns_info*, int);
static void rns_macinit(struct rns_info*);
static void rns_elm_mask(struct rns_info *, struct smt *, int);
static void rns_elm_int(struct rns_info *, struct smt *);
static int rns_mac_int(struct rns_info *);

static void rns_reset_tpc(struct smt*, struct rns_info*);
static time_t rns_get_tpc(struct smt*, struct rns_info*);
static void rns_pcm_off(struct smt*, struct rns_info*);
static void rns_setls(struct smt*, struct rns_info*, enum rt_ls);
static void rns_lct_on(struct smt*, struct rns_info*);
static int  rns_lct_off(struct smt*, struct rns_info*);
static void rns_cfm(struct smt*, struct rns_info*);
static int  rns_st_get(struct smt*, struct rns_info*);
static int  rns_st_set(struct smt*, struct rns_info*, struct smt_conf*);
static void rns_alarm(struct smt*, struct rns_info*, u_long);
static void rns_d_bec(struct smt*, struct rns_info*, struct d_bec_frame*,int);
static struct smt* rns_osmt(struct smt*, struct rns_info*);
static void rns_trace_req(struct smt*, struct rns_info*);
static void rns_fin_out(struct rns_info*);
static void rns_snd_out(struct rns_info *);


static struct smt_ops rns_ops = {
#define SOP struct smt*, void*
	(void (*)(SOP))			rns_reset_tpc,
	(time_t (*)(SOP))		rns_get_tpc,
	(void (*)(SOP))			rns_pcm_off,
	(void (*)(SOP, enum rt_ls))	rns_setls,
	(void (*)(SOP))			rns_lct_on,
	(int (*)(SOP))			rns_lct_off,
	(void (*)(SOP))			rns_cfm,
	(int (*)(SOP))			rns_st_get,
	(int (*)(SOP, struct smt_conf*))rns_st_set,
	(void (*)(SOP, u_long))		rns_alarm,
	(void (*)(SOP, struct d_bec_frame*, int))rns_d_bec,
	(struct smt* (*)(SOP))		rns_osmt,
	(void (*)(SOP))			rns_trace_req,

	(void (*)(SOP, int))		rns_setvec,
	(void (*)(SOP))			rns_join,
	(void (*)(struct smt*))		moto_pcm,
#undef SOP
};



/* counters for each board
 */
struct rns_dbg {
	uint	ifddi_dead;		/* IFDDI chip dead */
	uint	own_out;		/* heard our own output */
	uint	in_active;		/* input recursion prevented */
	uint	ints;			/* total interrupts */
};


/* FSI (IFDDI) descriptor rings
 */
#define ALIGN(x,a) (((__psunsigned_t)(x)+(a)-1) & ~((a)-1))

#define RX_RING_N	4		/* ring #4 for normal input */
#define RX_RING_LN	8		/* 256 descriptors in RX ring */
#define RX_RING_LEN	(1<<RX_RING_LN)
#define RX_BUF_LEN	1024
#define RX_DESC_HI	FSI_DESC_HI(FSI_CMD_RX, RX_BUF_LEN)
#define NEXT_RX_DI(di)	(((di)+1)&(RX_RING_LEN-1))

#define PM_RING_N	5		/* ring #5 for MAC frame input */
#define PM_RING_LN	2		/* 4 descriptors in the ring */
#define PM_RING_LEN	(1<<PM_RING_LN)
#define PM_BUF_LEN	FSI_RBR_MIN	/* 64 bytes/buffer */
#define PM_DESC_HI	FSI_DESC_HI(FSI_CMD_RX, PM_BUF_LEN)
#define NEXT_PM_DI(di)	(((di)+1)&(PM_RING_LEN-1))

#define TX_RING_N	1		/* ring #1 for normal output */
#define TX_RING_LN	6		/* 64 descriptors in main TX ring */
#define TX_RING_LEN	(1<<TX_RING_LN)
#define TX_MAX_JOBS	128		/* maximum packets in the queue */
#define TX_MAX_QLEN	(TX_MAX_JOBS-1)
#define	TX_ALIGN	8		/* to ease building headers */
#define TX_BUF_MAX	ALIGN(FDDI_MAXOUT,TX_ALIGN)
#define TX_LEN		(256*1024)
#define NEXT_TX_DI(di)	(((di)+1)&(TX_RING_LEN-1))

/* More than one cache-line of ~28 byte directed beacons, and so
 * 8 directed beacons descriptors, plus another 8 to dump the
 * FSI indications to make the infinite loop.  All of the descriptor
 * must be within 64K, and aligning for the RX and TX descriptors is
 * good enough.
 */
#define DB_RING_N	0		/* ring #0 for directed beaconing */
#define DB_RING_LN	3		/* 8 directed beacon frames/cycle */
#define DB_RING_LEN (1<<DB_RING_LN)


/* received line states, a superset of pcm_ls
 */
enum xi_pcm_ls {
    RCV_TRACE_PROP  =8,
    RCV_SELF_TEST   =9,
    RCV_BREAK	    =10,
    RCV_PCM_CODE    =11
};


/* to align the descriptors */
#define RNS_DESC_ALIGN	(8*RX_RING_LEN)

/* main control memory used by the board */
struct rns_dma {
    /* the descriptor rings must be aligned to their natural boundaries */
    fsi_desc_t	rx_desc[RX_RING_LEN];
    fsi_desc_t	tx_desc[TX_RING_LEN];
    fsi_desc_t	dbr_desc[DB_RING_LEN];
    fsi_desc_t	dbw_desc[DB_RING_LEN];
    fsi_desc_t	pm_desc[PM_RING_LEN];

    /* these buffers should be cache-line aligned */
    struct rx_buf {
	volatile u_char	b[RX_BUF_LEN];
    } rx[RX_RING_LEN];

    struct pm_buf {
	volatile u_char	b[PM_BUF_LEN];
    } pm[PM_RING_LEN];

    union tx_buf {
	u_char  b[TX_LEN];
	struct fddi hdr;
    } tx;

    struct d_bec_frame db[DB_RING_LEN]; /* directed beacons */
};


/* host info per board
 */
struct rns_info {
    struct arpcom   ri_ac;		/* common interface structures */
    struct fddi	    ri_hdr;		/* prototype MAC & LLC header */
    struct mfilter  ri_filter;
    struct rawif    ri_rawif;

    vertex_hdl_t    ri_vhdl;		/* for the board itself */
    vertex_hdl_t    ri_conn_vhdl;	/* the MACE chip or whatever */

#if defined(FDDI_FICUS) || defined(FDDI_BONSAI)
    struct rns_cfgregs *ri_cfgregs;	/* cfg registers */
#endif
    rns_regs_t	    *ri_regs;		/* the board */
    __uint32_t	    ri_brd_ctl;		/* shadow of board control register */
    struct ri_nvram ri_nvram;		/* copy of NVRAM from the board */
    product_tbl_t   *ri_product;

    u_char	    ri_macaddr[6];	/* MAC address from the board */

    union fsi_cam   ri_cam[FSI_CAM_LEN];
    __uint32_t	    ri_imr1;		/* shadow of the FSI IMR1 */

    __uint32_t	    ri_mac_mask_b;	/* shadow of MAC_MASK_B */

    micro_t	    ri_pm_usec;
    int		    ri_pm_len;		/* recent promiscuous mode MAC frame */
    u_char	    ri_pm[PM_BUF_LEN];

    u_long	    ri_state;
#    define RI_ST_BCHANGE   0x0001	/* optical bypass changed */
#    define RI_ST_MAC	    0x0002	/* MAC is on */
#    define RI_ST_ARPWHO    0x0004	/* IP address announcement needed */
#    define RI_ST_ZAPPED    0x0008	/* board has been naughty */
#    define RI_ST_WHINE	    0x0010	/* have whined about sick board */
#    define RI_ST_PROM	    0x0020	/* MAC is promiscuous */
#    define RI_ST_D_BEC	    0x0040	/* using directed beacon */
#ifndef FDDI_BONSAI
#    define RI_ST_PSENABLED 0x0080	/* RSVP packet scheduler enabled */
#endif

    struct rns_dbg  ri_dbg;		/* debugging counters */

    int		    ri_in_active;	/* input recursion preventer */

    uint	    ri_nuni;		/* # active unicast addresses */

    micro_t	    ri_usec;		/* free running usec timer */
    micro_t	    ri_tpc1;		/* PCM timer for smt1 */
    micro_t	    ri_tpc2;		/* PCM timer for smt2 */

    micro_t	    ri_lat_void;	/* when ring latency last measured */

    struct ri_pcm {
	u_short	    elm_sw;
#	    define	ELM_NOISE   1
#	    define	ELM_LEM_ON  2
	int	    ls_in, ls_out;	/* FIFO of line state changes */
	u_char	    ls[32], ls_prev;
#	 define ADV_LS_INX(i) ((i) = ((i)+1) & 31)
#	 define PUSH_LS(pp,st) ((pp)->ls[(pp)->ls_in] = (st),	\
			       ADV_LS_INX((pp)->ls_in))
#	 define NEW_LS(pp,st) (PUSH_LS(pp,st), (pp)->ls_prev = st)
	int	    smt_n;		/* bits being signaled */
	u_short	    elm_stb;		/* recent ELM state */
	u_short	    elm_ctrl_b;		/* shadow of ELM_CTRL_B */
	micro_t	    usec_prev;		/* usecs when LEM last measured */
	uint	    ct_start;		/* short term LEM started */
	uint	    ct_cur;		/* recent raw LEM count */
	uint	    ct2;		/* counts since LEM turned on */
	uint	    ct;			/* total count */
	micro_t	    time2;		/* usecs that LEM has been on now */
	micro_t	    time;		/* acccumulated time on in usec */
    } ri_pcm1, ri_pcm2;

    struct smt	ri_smt1;		/* PHY-S or PHY-B for DAS-SM */
    struct smt	ri_smt2;		/* PHY-A for DAS-SM */

    struct rns_dma *ri_hdma;		/* DMA area as seen by the host */
    struct rns_dma *ri_bdma;		/* as seen by the board */

    uint	ri_tx_desc_old;		/* oldest in use TX descriptor */
    uint	ri_tx_desc_nxt;		/* next free TX descriptor */
    uint	ri_tx_desc_free;	/* # of free TX descriptors */
    struct rns_tx_job {			/* This is locked with the interface */
	struct fddi job_hdr;
    } *ri_tx_job_head, *ri_tx_job_tail, ri_tx_jobs[TX_MAX_JOBS];
    union tx_buf *ri_tx_hbuf_nxt;
    u_char	*ri_tx_bbuf_nxt;
    uint	ri_tx_buf_free;		/* free bytes in buffer */

    uint	ri_rx_desc_nxt;
    uint	ri_pm_desc_nxt;
} *rns_infop[RNS_MAXBD];
#define ri_if	    ri_ac.ac_if     /* network-visible interface */
#define ri_unit	    ri_if.if_unit



#if !defined(FDDI_FICUS) && !defined(FDDI_BONSAI)
/*
 * RSVP
 * Called by packet scheduling functions to determine length of driver queue.
 */
static int
rns_txfree_len(struct ifnet *ifp)
{
	struct rns_info *ri = (struct rns_info *)ifp;
	/*
	 * must do garbage collection here, otherwise, fewer free
	 * spaces may be reported, which would cause packet scheduling
	 * to not send down packets.
	 */
	rns_fin_out(ri);
	return (ri->ri_tx_desc_free);
}


/*
 * RSVP
 * Called by packet scheduling functions to notify driver about queueing
 * state.  If setting is non-zero then there are queues and driver should
 * update the packet scheduler by calling ps_txq_stat() when the txq length
 * changes.  If setting is 0, then don't call packet scheduler.
 */
static void
rns_setstate(struct ifnet *ifp, int setting)
{
	struct rns_info *ri = (struct rns_info *)ifp;

	if (setting)
		ri->ri_state |= RI_ST_PSENABLED;
	else
		ri->ri_state &= ~RI_ST_PSENABLED;
}
#endif


#ifndef NOT_LEGO
#define rns_flush(ri)
#else
/* Flush the CPU memory write queues in the CRIME ASIC */
static void
rns_flush(struct rns_info *ri)
{
	/* flush the memory writes waiting in CRIME */
	wbflush();

	/* invalidate MACE prefetch buffers */
#ifdef FDDI_BONSAI
	(void)pciio_flush_buffers(ri->ri_conn_vhdl);
#else
	(void)pcimh_flush_buffers(ri->ri_conn_vhdl);
#endif
}
#endif


#define CMR_CHECK 5			/* check FCR this often in usec */
#define CMR_DELAY (20*1000)		/* be this patient */

static int				/* 0=ok */
cmr_sto(struct rns_info *ri,
	fsi_cmd_t cmd)
{
	int delay;

	/* Wait untill the FSI is finished with the previous command
	 * (if any) and then start the new one.
	 */
	delay = CMR_DELAY/CMR_CHECK;
	do {
		if (RD_REG(ri, fsi_sr1) & SR1_CDN) {
			WT_REG(ri, fsi_cer, cmd);
			WT_REG(ri, fsi_cmr, cmd >> 32);
			return 0;
		}
		DELAY(CMR_CHECK);
	} while (--delay != 0);

	/* complain & kill board if it does not answer */
	ri->ri_dbg.ifddi_dead++;
	if (!(ri->ri_state & RI_ST_WHINE)) {
		ZAPBD(ri);
		printf("rns%d: IFDDI CMR asleep\n", ri->ri_unit);
		ri->ri_state |= RI_ST_WHINE;
		rns_alarm(&ri->ri_smt1,ri, SMT_ALARM_SICK);
	}
	return -1;
}


#if 0
static fsi_cmd_t
cmr_fet(struct rns_info *ri)
{
	int delay;
	fsi_cmd_t cmd;

	/* Wait untill the FSI is finished with the previous command
	 * (if any) and then start the new one.
	 */
	delay = CMR_DELAY/CMR_CHECK;
	do {
		if (RD_REG(ri, fsi_sr1) & SR1_CDN
		    && !((cmd = RD_REG(ri, fsi_cmr)) & CMR_OWN_BIT)) {
			cmd = (cmd << 32) | RD_REG(ri, fsi_cer);
			return cmd;
		}
		DELAY(CMR_CHECK);
	} while (--delay != 0);

	/* complain & kill board if it does not answer */
	ri->ri_dbg.ifddi_dead++;
	if (!(ri->ri_state & RI_ST_WHINE)) {
		ZAPBD(ri);
		printf("rns%d: IFDDI CMR asleep\n", ri->ri_unit);
		ri->ri_state |= RI_ST_WHINE;
		rns_alarm(&ri->ri_smt1,ri, SMT_ALARM_SICK);
	}
	return -1;
}



static fsi_cmd_t			/* -1 if bad or result */
cmr_sto_fet(struct rns_info *ri,
	fsi_cmd_t cmd)
{
	int delay;

	if (cmr_sto(ri, cmd) < 0)
		return -1;

	delay = CMR_DELAY/CMR_CHECK;
	do {
		if (RD_REG(ri, fsi_sr1) & SR1_CDN)
			return ((RD_REG(ri, fsi_cmr) << 32)
				| RD_REG(ri, fsi_cer));
		DELAY(CMR_CHECK);
	} while (--delay != 0);

	/* complain & kill board if it does not answer */
	ri->ri_dbg.ifddi_dead++;
	if (!(ri->ri_state & RI_ST_WHINE)) {
		printf("rns%d: IFDDI CMR broken\n", ri->ri_unit);
		ri->ri_state |= RI_ST_WHINE;
		rns_alarm(&ri->ri_smt1,ri, SMT_ALARM_SICK);
	}

	return -1;
}
#endif


#define FCR_CHECK CMR_CHECK		/* check FCR this often in usec */
#define FCR_DELAY CMR_DELAY		/* be this patient */

/* Set an indirect IFDDI register
 *	It is too bad that Rockwell used the A port for the local memory,
 *	since that radically complicates and slows down this stuff.
 */
static void
fcr_sto(struct rns_info *ri,
	__uint32_t v)
{
	int delay;

	delay = FCR_DELAY/FCR_CHECK;
	do {
		if (RD_REG(ri, fsi_sr1) & SR1_CRF) {
			WT_REG(ri, fsi_fcr, v);
			return;
		}
		DELAY(FCR_CHECK);
	} while (--delay != 0);

	/* complain & kill board if it does not answer */
	ri->ri_dbg.ifddi_dead++;
	if (!(ri->ri_state & RI_ST_WHINE)) {
		printf("rns%d: IFDDI FCR sto asleep\n", ri->ri_unit);
		ri->ri_state |= RI_ST_WHINE;
		rns_alarm(&ri->ri_smt1,ri, SMT_ALARM_SICK);
	}
}


/* read an indirect IFDDI register
 */
static u_short
fcr_fet(struct rns_info *ri,
	__uint32_t v)
{
	int delay;
	uint val;

	delay = FCR_DELAY/FCR_CHECK;
	for (;;) {
		if (RD_REG(ri, fsi_sr1) & SR1_CRF)
			break;
		DELAY(FCR_CHECK);
		if (--delay == 0)
			goto bad;
	}

	WT_REG(ri, fsi_fcr, FSI_R | v);

	do {
		if (RD_REG(ri, fsi_sr1) & SR1_CRF) {
			val = RD_REG(ri, fsi_fcr);
			if (((v ^ FCR_CAMEL(0,0)) & IRT_IRI_MASK) != 0)
				val = (val >> FCR_DATA) & 0xff;
			return val;
		}
		DELAY(FCR_CHECK);
	} while (--delay != 0);


	/* complain & kill board if it does not answer */
bad:
	ri->ri_dbg.ifddi_dead++;
	if (!(ri->ri_state & RI_ST_WHINE)) {
		printf("rns%d: IFDDI FCR fetch asleep\n", ri->ri_unit);
		ri->ri_state |= RI_ST_WHINE;
		rns_alarm(&ri->ri_smt1,ri, SMT_ALARM_SICK);
	}

	return 0;
}


/* turn off board DMA while reading or writing the second ELM to
 * workaround the RNS "2200/2250 DAS Errata"
 * Turn both FSI ports off and on.
 * Finally read the status register to flush the Lego PIO pipeline.
 */
#define PCR_OFF(ri) (fcr_sto(ri, FSI_PCRA(FSI_PCR_OFF)),	\
		     fcr_sto(ri, FSI_PCRB(FSI_PCR_OFF)), RD_REG(ri, fsi_sr1))
#define PCR_ON(ri) (fcr_sto(ri, FSI_PCRA(FSI_PCR_ON)),		\
		    fcr_sto(ri, FSI_PCRB(FSI_PCR_ON)), RD_REG(ri, fsi_sr1))


static void
mac_sto(struct rns_info *ri,
	u_char reg,
	u_short v)
{
	fcr_sto(ri, FCR_CAMEL(reg,v));
}


static u_short
mac_fet(struct rns_info *ri,
	u_char reg)
{
	return fcr_fet(ri, FCR_CAMEL(reg,0));
}


/* store a value in a register in one of the ELMs */
static void
elm_sto(struct rns_info *ri,
	struct smt *smt,
	u_char reg,
	u_short v)
{
	if (SMT1(smt,ri)) {
		fcr_sto(ri, FCR_CAMEL(reg,v));
	} else {
		PCR_OFF(ri);
		WT_REG(ri, ext_elm[reg], v);
		PCR_ON(ri);
	}
}

/* store a value in a register in both of the ELMs */
static void
elm_sto2(struct rns_info *ri,
	 u_char reg,
	 u_short v)
{
	fcr_sto(ri, FCR_CAMEL(reg,v));
	if (IS_DAS(ri)) {
		PCR_OFF(ri);
		WT_REG(ri, ext_elm[reg], v);
		PCR_ON(ri);
	}
}


static u_short
elm_fet(struct rns_info *ri,
	struct smt *smt,
	u_char reg)
{
	u_short res;

	if (SMT1(smt,ri)) {
		return fcr_fet(ri, FCR_CAMEL(reg,0));
	} else {
		PCR_OFF(ri);
		res = RD_REG(ri, ext_elm[reg]);
		PCR_ON(ri);
		return res;
	}
}

static void
elm_pc_cmd(struct rns_info *ri,
	struct smt *smt,
	u_short v1)
{
	struct ri_pcm *pp = SMT1(smt,ri) ? &ri->ri_pcm1 : &ri->ri_pcm2;

	pp->elm_ctrl_b &= ~(ELM_CB_PC_MAINT | ELM_CB_LS_TX_MASK);
	elm_sto(ri,smt, ELM_CTRL_B, pp->elm_ctrl_b);
	elm_sto(ri,smt, ELM_CTRL_B, v1|pp->elm_ctrl_b);
}

static void
elm_pc_off(struct rns_info *ri,
	   struct smt *smt)
{
	rns_elm_mask(ri,smt, 0);	/* turn off noise interrupts */

	elm_pc_cmd(ri,smt,ELM_CB_PCM_STOP);
	elm_sto(ri,smt,ELM_CTRL_A, ELM_CA_DEFAULT);
	smt_new_st(smt, PCM_ST_OFF);

	if (smt->smt_st.tls != T_PCM_QLS) {
		smt->smt_st.tls = T_PCM_QLS;
		SMT_LSLOG(smt, T_PCM_QLS);
	}
}


/* Set the interrupt mask inside an ELM
 */
static void
rns_elm_mask(struct rns_info *ri,
	    struct smt *smt,
	    int on)			/* ELM_NOISE=watch for noise events */
{
	struct ri_pcm *pp = SMT1(smt,ri) ? &ri->ri_pcm1 : &ri->ri_pcm2;

	if (!iff_alive(ri->ri_if.if_flags)) {
		on = 0;
		elm_sto(ri,smt, ELM_MASK, 0);	/* all ELM interrupts off */
	} else {
		elm_sto(ri,smt, ELM_MASK,
			on ? ELM_EV_ACT	/* watch counters */
			: ELM_EV_SIG);	/* only line states */
		pp->elm_ctrl_b |= ELM_CB_MATCH_LS_MASK;
	}

	pp->elm_sw = (pp->elm_sw & ~ELM_NOISE) | on;
}


/* Set the line state in one of the ELMs
 */
static void
rns_elm_ls(struct rns_info *ri,
	   struct smt *smt,
	   enum pcm_ls ls)
{
	static u_short setls_tbl[] = {
		ELM_CB_PC_MAINT | ELM_CB_TX_QLS,    /* PCM_QLS */
		ELM_CB_PC_MAINT | ELM_CB_TX_ILS,    /* PCM_ILS */
		ELM_CB_PC_MAINT | ELM_CB_TX_MLS,    /* PCM_MLS */
		ELM_CB_PC_MAINT | ELM_CB_TX_HLS,    /* PCM_HLS */
	};
	struct ri_pcm *pp = SMT1(smt,ri) ? &ri->ri_pcm1 : &ri->ri_pcm2;

	pp->elm_ctrl_b = ((pp->elm_ctrl_b & ~ELM_CB_LS_TX_MASK)
			  | setls_tbl[ls]);
	elm_sto(ri,smt, ELM_CTRL_B, pp->elm_ctrl_b | ELM_CB_PCM_STOP);
	elm_sto(ri,smt, ELM_CTRL_A, (ELM_CA_DEFAULT | ELM_CA_SC_BYPASS
				     | ELM_CA_REM_LOOP));

	rns_elm_mask(ri,smt, 0);	/* turn off noise interrupts */
}


/* Set the MAC multicast mode, address filters, and so on
 *	Interrupts must be off.
 */
static void
rns_set_mmode(struct rns_info *ri)
{
	struct mfent *mfe;
	union fsi_cam cam[FSI_CAM_LEN];
	int i;
	__uint32_t mrr;
	u_short ca, cb;


	mrr = FSI_MRR(FSI_MRR_RX);
	ca = MAC_CA_DEFAULT;
	if (ri->ri_if.if_flags & IFF_ALLMULTI)
		ca |= MAC_CA_COPY_GRP;

	if (ri->ri_if.if_flags & IFF_PROMISC) {
		ca |= MAC_CA_PROM;
		mrr = FSI_MRR(FSI_MRR_RX | FSI_MRR_PM);
		ri->ri_state |= RI_ST_PROM;
	} else if (0 != ri->ri_nuni) {
		/* We cannot use the CAM to snoop on individual addresses,
		 * because it would cause the A bit to be set.
		 */
		ca |= MAC_CA_PROM;
		ri->ri_state |= RI_ST_PROM;
	} else {
		ri->ri_state &= ~RI_ST_PROM;
	}

	/* Copy addresses from the multicast filter to a list of commands
	 * to set the FSI CAM.  If there are too many addresses,
	 * tell the MAC to receive all multicasts.  They will always be
	 * filtered with the hash table.
	 */
	i = 0;
	mfe = ri->ri_filter.mf_base;
	for (;;) {
		if ((mfe->mfe_flags & MFE_ACTIVE)
		    && FDDI_ISGROUP_SW(mfe->mfe_key.mk_dhost)) {
			if (i >= FSI_CAM_LEN) {
				ca |= MAC_CA_COPY_GRP;
				break;
			}

			bcopy(&mfe->mfe_key.mk_dhost[0],
			      &cam[i].s.entry[0],
			      sizeof(cam[i].s.entry));
			cam[i].s.id = (FSI_CAM_CONST | FSI_CAM_VALID | i);
			i++;
		}
		if (++mfe >= ri->ri_filter.mf_limit) {
			while (i < FSI_CAM_LEN) {
				cam[i].s.id = (FSI_CAM_CONST | i);
				i++;
			}
			break;
		}
	}

	/* bother the board only if necessary, because changing the FSI
	 * CAM requires a MAC reset.
	 */
	if (bcmp(cam, ri->ri_cam, sizeof(ri->ri_cam))) {
		bcopy(cam,ri->ri_cam,sizeof(ri->ri_cam));
		/* turn off the MAC to change the CAM because of the FSI bug
		 */
		cb = mac_fet(ri, MAC_CTRL_B);
		mac_sto(ri, MAC_CTRL_A, ca & ~MAC_CA_ON);
		for (i = 0; i < FSI_CAM_LEN; i++)
			(void)cmr_sto(ri,ri->ri_cam[i].d);
		mac_sto(ri, MAC_CTRL_A, ca);
		mac_sto(ri, MAC_CTRL_B, cb | MAC_CB_RESET);
	} else {
		mac_sto(ri, MAC_CTRL_A, ca);
	}

	fcr_sto(ri, mrr);
}


/* set the MAC address
 */
static void
rns_set_addr(struct rns_info *ri,
	     u_char *addr)		/* in canonical bit order */
{
	bcopy(addr,ri->ri_ac.ac_enaddr,sizeof(ri->ri_ac.ac_enaddr));

	bitswapcopy(addr, ri->ri_smt1.smt_st.addr.b,
		    sizeof(ri->ri_smt1.smt_st.addr));
}



/* Add a destination address.
 */
static int
rns_add_da(struct rns_info *ri,
	   union mkey *key,
	   int ismulti)
{
	struct mfreq mfr;

	if (mfmatchcnt(&ri->ri_filter, 1, key, 0)) {
		return 0;
	}

	mfr.mfr_key = key;
	if (!mfadd(&ri->ri_filter, key, mfr.mfr_value)) {
		return ENOMEM;
	}

	if (!ismulti)
		++ri->ri_nuni;

	rns_set_mmode(ri);

	return 0;
}


/* Delete an address filter.  If key is unassociated, do nothing.
 * Otherwise delete software filter first, then hardware filter.
 */
static int
rns_del_da(struct rns_info *ri,
	   union mkey *key,
	   int ismulti)
{
	struct mfreq mfr;

	if (!mfmatchcnt(&ri->ri_filter, -1, key, &mfr.mfr_value))
		return 0;

	mfdel(&ri->ri_filter, key);

	if (!ismulti)
		--ri->ri_nuni;

	rns_set_mmode(ri);

	return 0;
}



/* accumulate Link Error Monitor experience
 *	This must be called at least every 500 seconds or the symbol count
 *	will be wrong.
 */
static void
rns_lem_accum(struct smt *smt,
	      struct rns_info *ri,
	      struct ri_pcm *pp)
{
	uint new_ler2;
	struct timeval tv;
	micro_t usec;
	uint ct;


	ct = pp->ct_cur - pp->ct_start;
	ct = (ct > 2) ? (ct-2) : 0;	/* forgive two errors */

	pp->ct2 = ct;
	usec = ri->ri_usec - pp->usec_prev;
	pp->usec_prev = ri->ri_usec;
	pp->time2 += usec;
	tv.tv_sec = pp->time2 / USEC_PER_SEC;
	tv.tv_usec = pp->time2 % USEC_PER_SEC;
	RNDTIMVAL(&tv);
	new_ler2 = smt_ler(&tv,ct);	/* short-term LER */
	if (new_ler2 <= smt->smt_conf.ler_cut
	    && new_ler2 < smt->smt_st.ler2)
		rns_alarm(smt,ri,SMT_ALARM_LEM);
	smt->smt_st.ler2 = new_ler2;

	ct += pp->ct;
	if (ct >= 0x80000000) {
		pp->ct /= 2;
		pp->time /= 2;
	}
	tv.tv_sec = (pp->time+pp->time2) / USEC_PER_SEC;
	tv.tv_usec = (pp->time+pp->time2) % USEC_PER_SEC;
	RNDTIMVAL(&tv);
	smt->smt_st.ler = smt_ler(&tv, ct); /* long-term LER */

	smt->smt_st.lem_count = ct;

	/* Age the short term estimate
	 *	This assumes the daemon will ask at least once a minute.
	 */
	if (pp->time2 > 2*LC_EXTENDED) {
		usec = pp->time2/4;
		pp->time2 -= usec;
		pp->time += usec;
		ct = pp->ct2 - (pp->ct2 / 4);	/* round up */
		pp->ct += ct;
		pp->ct_start += ct;
	}
}


/* Turn LEM off for a PCM
 */
static void
rns_lemoff(struct smt *smt,
	   struct rns_info *ri)
{
	struct ri_pcm *pp = SMT1(smt,ri) ? &ri->ri_pcm1 : &ri->ri_pcm2;

	if (pp->elm_sw & ELM_LEM_ON) {
		/* accumulate experience
		 */
		(void)rns_st_get(0,ri);

		pp->time += pp->time2;
		pp->ct += pp->ct2;
		if (pp->ct >= 0x80000000) {
			pp->ct /= 2;
			pp->time /= 2;
		}

		pp->elm_sw &= ~ELM_LEM_ON;
	}
}


/* Turn LEM on
 */
static void
rns_lemon(struct smt *smt,
	  struct rns_info *ri)
{
	struct ri_pcm *pp = SMT1(smt,ri) ? &ri->ri_pcm1 : &ri->ri_pcm2;

	if (!(pp->elm_sw & ELM_LEM_ON)) {
		(void)rns_st_get(0,ri);

		pp->time2 = 0;
		pp->ct_start = pp->ct_cur;
		pp->elm_sw |= ELM_LEM_ON;
	}
}


/* Set the output line state
 * The ELM that is upstream of the MAC when a DAS system is in THRU
 * must be PHY-A.  This means that ELM1 (in the IFDDI) must be PHY-S or
 * PHY-B.
 *
 * Configurations:
 *			SAS		DAS-SM
 *			--------	--------
 *			ELM1=quiet	ELM1=quiet
 *  iff_dead()				ELM2=quiet
 *			MAC=xx		MAC=xx
 *
 * THRU			ELM1=thru	ELM1=thru
 *  THRU=1				ELM2=thru
 *  JOIN=1
 *
 * WRAP_A				ELM1=loop
 *  THRU=0				ELM2=thru
 *  JOIN=1
 *
 * WRAP_B				ELM1=thru
 *  THRU=0				ELM2=loop
 *  JOIN=0
 */
static void
rns_setls(struct smt *smt,
	  struct rns_info *ri,
	  enum rt_ls ls)
{
	CK_IFNET(ri);

	switch (ls) {
	case T_PCM_REP:			/* not supported */
	case R_PCM_QLS:			/* send QLS */
		ls = T_PCM_QLS;
	case T_PCM_QLS:
		if (ls == smt->smt_st.tls)
			return;
		rns_elm_ls(ri,smt, PCM_QLS);
		break;

	case R_PCM_MLS:			/* send MLS */
		ls = T_PCM_MLS;
	case T_PCM_MLS:
		if (ls == smt->smt_st.tls)
			return;
		rns_elm_ls(ri,smt, PCM_MLS);
		break;

	case R_PCM_HLS:			/* send HLS */
		ls = T_PCM_HLS;
	case T_PCM_HLS:
		if (ls == smt->smt_st.tls)
			return;
		rns_elm_ls(ri,smt, PCM_HLS);
		break;

	case R_PCM_ILS:			/* send ILS */
		ls  = T_PCM_ILS;
	case T_PCM_ILS:
		if (ls == smt->smt_st.tls)
			return;
		rns_elm_ls(ri,smt, PCM_ILS);
		break;

	case T_PCM_THRU:		/* Put this PHY into the ring. */
		if (ls == smt->smt_st.tls)
			return;
		break;

	case T_PCM_WRAP:		/* Put MAC & PHY in ring alone. */
		if (ls == smt->smt_st.tls)
			return;
		break;

	case T_PCM_LCT:			/* start Link Confidence Test */
		if (ls == smt->smt_st.tls)
			return;
		elm_pc_cmd(ri,smt, ELM_CB_LCT);
		rns_elm_mask(ri,smt, ELM_NOISE);
		rns_lemon(smt,ri);
		break;

	case T_PCM_LCTOFF:		/* stop Link Confidence Test */
		rns_lemoff(smt,ri);	/* update LER counter */
		break;

	case T_PCM_SIG:
		ls = T_PCM_QLS;
		elm_pc_off(ri, smt);
		break;

#ifdef DEBUG
	default:
		panic("unknown line state");
#endif
	}

	smt->smt_st.tls = ls;
	SMT_LSLOG(smt, ls);
}



/* distill PCM state bits from ELM status
 */
static void
rns_elm_st(struct rns_info *ri,
	   struct smt *smt,
	   struct ri_pcm *pp)
{
	enum pcm_state pcm_state;
	uint vc, lc, elm_stb, rcv_vec;


	elm_stb = elm_fet(ri,smt, ELM_STATUS_B);
	rcv_vec = elm_fet(ri,smt, ELM_RCV_VEC);
	vc = elm_fet(ri,smt, ELM_VIOL_SYM);
	lc = elm_fet(ri,smt, ELM_LINK_ERR);

	SMTDEBUGPRINT(smt,3,("%sPC%d,%x,%x ",
			     PC_LAB(smt), smt->smt_st.pcm_state,
			     (long)rcv_vec, (long)elm_stb));

	/* pay attention to noise only when things are turned on */
	if (pp->elm_sw & ELM_NOISE) {
#ifndef FDDI_BONSAI
		UPCNT(ri,viol,vc);
#endif
		pp->ct_cur += lc;
	}

	smt->smt_st.r_val = ((smt->smt_st.r_val & (1 << smt->smt_st.n) - 1)
			     | ((rcv_vec << -smt->smt_st.n)
				& (1 << (smt->smt_st.n+pp->smt_n)) - 1));

	smt->smt_st.flags &= ~(PCM_LS_FLAG | PCM_RC_FLAG | PCM_TC_FLAG);
	if (elm_stb & ELM_SB_LSF)
		smt->smt_st.flags |= PCM_LS_FLAG;
	if (elm_stb & ELM_SB_RCF)
		smt->smt_st.flags |= PCM_RC_FLAG;
	if (elm_stb & ELM_SB_TCF)
		smt->smt_st.flags |= PCM_TC_FLAG;

	pcm_state = (enum pcm_state)((elm_stb & ELM_SB_PCM_STATE_MASK)
				     / ELM_SB_PCM_STATE);
	if (elm_stb & ELM_SB_PCM_SIG) {
		smt->smt_st.flags |= PCM_PHY_BUSY;
	} else if (pcm_state == PCM_ST_NEXT) {
		if (smt->smt_st.n < 10) {
			smt->smt_st.n += pp->smt_n;
			pp->smt_n = 0;
		}
	} else if (pcm_state == PCM_ST_MAINT) {
		if (smt->smt_st.pcm_state == PCM_ST_OFF)
			pcm_state = PCM_ST_OFF;
	}

	if (smt->smt_st.pcm_state != PCM_ST_BYPASS) {
		enum pcm_state set = ((pcm_state == PCM_ST_OFF
				       && PCM_SET(smt, PCM_PC_START))
				      ? PCM_ST_BREAK
				      : pcm_state);
		if (set != smt->smt_st.pcm_state) {
			smt_new_st(smt, set);

			/* remember to talk to software SMT state machine
			 * by forcing a call moto_pcm() */
			if (pp->ls_in == pp->ls_out)
				PUSH_LS(pp, PCM_LSU);
		}

		elm_stb &= (ELM_SB_BREAK_MASK/ELM_SB_BREAK);
		if (pcm_state == PCM_ST_BREAK
		    && elm_stb != pp->elm_stb) {
			if (elm_stb > ELM_SB_BREAK_PC_START/ELM_SB_BREAK)
				SMT_LSLOG(smt, SMT_LOG_X+elm_stb);
			pp->elm_stb = elm_stb;
		}
	}
}


/* Get LER and other counters
 *	come here with driver interrupts off--or things locked, whatever
 */
static int
rns_st_get(struct smt *smt1,	/* caller value not used */
	   struct rns_info *ri)
{
	int i, j, s;
	unsigned long long ust;

	CK_IFNET(ri);
	/* We need a clock good for at least microseconds.  It should
	 * not be fiddled by adjtime() and similar time-of-day things.
	 */
	s = splprof();
	update_ust();
	get_ust_nano(&ust);
	splx(s);
	ri->ri_usec = ust/1000LL;

	/* no work if the board is sick
	 */
	if (ri->ri_state & RI_ST_ZAPPED)
		return EIO;

	smt1 = &ri->ri_smt1;
	if (iff_alive(ri->ri_if.if_flags)) {
		rns_elm_st(ri, smt1, &ri->ri_pcm1);
		if (ri->ri_pcm1.elm_sw & ELM_LEM_ON)
			rns_lem_accum(smt1, ri, &ri->ri_pcm1);

		if (IS_DAS(ri)) {
			rns_elm_st(ri, &ri->ri_smt2, &ri->ri_pcm2);
			if (ri->ri_pcm2.elm_sw & ELM_LEM_ON)
				rns_lem_accum(&ri->ri_smt2, ri, &ri->ri_pcm2);
		}

		/* get MAC values if it is alive */
		if (ri->ri_state & RI_ST_MAC) {
			/* keep the ring latency measurement up to date
			 */
			if (rns_lat_void
			    && ri->ri_lat_void < ri->ri_usec-2*USEC_PER_SEC) {
				ri->ri_lat_void = ri->ri_usec;
				mac_sto(ri,MAC_CTRL_B, (mac_fet(ri, MAC_CTRL_B)
							| MAC_CB_RING_PURGE));
			}

			/* get token counts and so forth */
			(void)rns_mac_int(ri);

			/* get consistent T_Neg */
			for (i = 0;;) {
				j = ((mac_fet(ri,MAC_T_NEG_B) << 16)
				     | mac_fet(ri,MAC_T_NEG_A));
				if (i == j)
					break;
				i = j;
			}
			ri->ri_smt1.smt_st.t_neg = (-1<<24) | i;
		}
	}

	return 0;
}


/* Check a proposed configuration
 *	This only checks special restrictions imposed by this driver
 *	and hardware.  It does not do general sanity checks, which should
 *	be done by the SMT demon.
 */
/* ARGSUSED */
static int				/* 0, EINVAL or some other error # */
rns_st_set(struct smt *smt,
	   struct rns_info *ri,
	   struct smt_conf *conf)
{
	int recon;			/* need to reconnect */
#ifdef FDDI_BONSAI
	int s;
#endif

	CK_IFNET(ri);
	if (conf->type == SM_DAS	/* require needed DAS hardware */
	    && !(ri->ri_product->p_attr & RNS_DAS)) {
		return EINVAL;
	}

	if (conf->type == DM_DAS) {
		return EINVAL;		/* not implemented */
	}

#ifdef FDDI_BONSAI
	s = splimp();
#endif
	recon = 0;

	/* do the easy stuff first
	 */
	bcopy(&conf->mnfdat,
	      &smt->smt_conf.mnfdat,
	      sizeof(smt->smt_conf.mnfdat));
	smt->smt_conf.debug = conf->debug;

	/* clamp & round TVX for the MAC.  It wants a negative multiple
	 * of 256*BCLKs between 1 and 255.
	 */
	conf->tvx = conf->tvx & ~0xff;
	if (conf->tvx > MIN_TVX)
		conf->tvx = MIN_TVX;
	else if (conf->tvx < MAX_TVX)
		conf->tvx = MAX_TVX;

	/* The MAC wants a multiple of 256 for T_REQ
	 */
	conf->t_req &= ~0xff;
	if (conf->t_req > MIN_T_REQ)
		conf->t_req = MIN_T_REQ;
	else if (conf->t_req < MAX_T_REQ)
		conf->t_req = MAX_T_REQ;

	/* Clamp and round T_Max.  The MAC wants a positive
	 * multiple of 2**16 BCLKs.
	 */
	if (conf->t_max > conf->t_req)
		conf->t_max = conf->t_req;
	conf->t_max &= ~0xffff;
	if (conf->t_max < MAX_T_MAX)
		conf->t_max = MAX_T_MAX;
	if (conf->t_max > MIN_T_MAX)
		conf->t_max = MIN_T_MAX;

	/* do not change T_MIN */
	conf->t_min = smt->smt_conf.t_min;

	if (smt->smt_conf.t_max != conf->t_max
	    || smt->smt_conf.t_req != conf->t_req
	    || smt->smt_conf.tvx != conf->tvx) {
		recon = 1;
		smt->smt_conf.t_max = conf->t_max;
		smt->smt_conf.t_req = conf->t_req;
		smt->smt_conf.tvx = conf->tvx;
	}

	if (smt->smt_conf.ler_cut != conf->ler_cut) {
		if (smt->smt_st.ler2 > smt->smt_conf.ler_cut
		    && smt->smt_st.ler2 <= conf->ler_cut)
			rns_alarm(smt,ri,SMT_ALARM_LEM);
		smt->smt_conf.ler_cut = conf->ler_cut;
	}

	if (smt->smt_conf.type != conf->type
	    || smt->smt_conf.pc_type != conf->pc_type) {
		recon = 1;
		smt->smt_conf.type = conf->type;
		smt->smt_conf.pc_type = conf->pc_type;
	}

	if (smt->smt_conf.ip_pri != conf->ip_pri) {
		recon = 1;
		smt->smt_conf.ip_pri = conf->ip_pri;
	}

	if (recon) {
		/* force the MAC to be reset */
		smt_off(smt);
		if (IS_DAS(ri)) {
			smt_off(OTHER_SMT(ri,smt));
		}
	}
#ifdef FDDI_BONSAI
	splx(s);
#endif
	return 0;
}



/* drain a fake frame to awaken the SMT deamon
 */
static void
rns_alarm(struct smt *smt,
	  struct rns_info *ri,
	  u_long alarm)
{
	struct mbuf *m;
	FDDI_IBUFLLC *fhp;
#ifdef FDDI_BONSAI
	int s = splimp();
#endif

	smt = &ri->ri_smt1;		/* use the only MAC we have */

	CK_IFNET(ri);
	smt->smt_st.alarm |= alarm;
	m = smt->smt_alarm_mbuf;
	if (0 != m) {
		smt->smt_alarm_mbuf = 0;
		ASSERT(SMT_LEN_ALARM <= MLEN);
		m->m_len = SMT_LEN_ALARM;
		fhp = mtod(m,FDDI_IBUFLLC*);
		bzero(fhp, SMT_LEN_ALARM);
		IF_INITHEADER(fhp, &ri->ri_if, SMT_LEN_ALARM);
		if (RAWIF_DRAINING(&ri->ri_rawif)) {
			drain_input(&ri->ri_rawif,
				    FDDI_PORT_SMT,
				    (caddr_t)&fhp->fbh_fddi.fddi_mac.mac_sa,
				    m);
		} else {
			m_free(m);
		}
	}
#ifdef FDDI_BONSAI
	splx(s);
#endif
}



/* start directed beacon
 */
/* ARGSUSED */
static void
rns_d_bec(struct smt *smt,		/* not used */
	  struct rns_info *ri,
	  struct d_bec_frame *info,
	  int len)
{
	int i;
	u_char *p, *q;
	fsi_desc_t *descp;

	CK_IFNET(ri);

	/* turn off DB ring */
	if (ri->ri_state & RI_ST_D_BEC) {
		(void)cmr_sto(ri, FSI_CMD(FSI_CMD_RSTOP, DB_RING_N, 0, 0));
		ri->ri_state &= ~RI_ST_D_BEC;
	}

	if (len >= D_BEC_SIZE-sizeof(info->una)) {
		if (len > D_BEC_SIZE)
			len = D_BEC_SIZE;
		p = (u_char*)&ri->ri_hdma->db[0];
		q = (u_char*)&ri->ri_bdma->db[0];
		descp = ri->ri_hdma->dbr_desc;
		bcopy(info, p, len);
		ri->ri_hdma->db[0].hdr.filler[0] =(u_char)(MAC_BEACON_HDR>>16);
		ri->ri_hdma->db[0].hdr.filler[0] = (u_char)(MAC_BEACON_HDR>>8);
		ri->ri_hdma->db[0].hdr.filler[0] = (u_char)(MAC_BEACON_HDR);
		ri->ri_hdma->db[0].hdr.mac_fc = MAC_FC_BEACON;
		bcopy(&ri->ri_hdr.fddi_mac.mac_sa,
		      &ri->ri_hdma->db[0].hdr.mac_sa,
		      sizeof(ri->ri_hdma->db[0].hdr.mac_sa));

		/* install the descriptors and the beacon frames */
		descp->a = FSI_DESC_A(q);
		descp->hi = FSI_DESC_HI(FSI_CMD_TX1, len);

		/* pack the beacon frames so that the PCI bridge can
		 * hope to minimize its accesses to memory while spewing
		 * the frames.
		 */
		for (i = 1; i != DB_RING_LEN; i++) {
			p += len;
			q += len;
			descp++;
			bcopy(&ri->ri_hdma->db[0], p, len);
			descp->a = FSI_DESC_A(q);
			descp->hi = FSI_DESC_HI(FSI_CMD_TX1, len);
		}

		/* start FSI Directed-Beacon transmit ring */
		(void)cmr_sto(ri, FSI_CMD(FSI_CMD_DEFRDY, DB_RING_N,
					  ri->ri_bdma->dbr_desc,
					  ri->ri_bdma->dbw_desc));
		mac_sto(ri, MAC_CTRL_B,
			(mac_fet(ri, MAC_CTRL_B) | MAC_CB_RESET_BEACON
			 | MAC_CB_FSI_BEACON));
		ri->ri_state |= RI_ST_D_BEC;
	}
}



/* start PC-Trace
 *	Interrupts must be off.
 */
static void
rns_trace_req(struct smt *smt,
	      struct rns_info *ri)
{
	CK_IFNET(ri);
	TPC_RESET(smt);
	SMT_USEC_TIMER(smt, SMT_TRACE_MAX);
	smt->smt_st.flags &= ~PCM_PC_DISABLE;
	smt_new_st(smt, PCM_ST_TRACE);

	elm_pc_cmd(ri,smt, ELM_CB_PCM_TRACE);
}



/* start the clock ticking for PCM
 */
static void
rns_reset_tpc(struct smt *smt,
	      struct rns_info *ri)
{
	(void)rns_st_get(0,ri);

	if (SMT1(smt,ri)) {
		ri->ri_tpc1 = ri->ri_usec;
	} else {
		ri->ri_tpc2 = ri->ri_usec;
	}
}


/* Decide what time it is
 */
static time_t				/* elapsed time in usec */
rns_get_tpc(struct smt *smt,
	    struct rns_info *ri)
{
	micro_t t;

	(void)rns_st_get(0,ri);
	t = ri->ri_usec - (SMT1(smt,ri) ? ri->ri_tpc1 : ri->ri_tpc2);
	return MIN(t, 0x7fffffff);
}


/* Reset a MAC because we want to shut everything off or because we want
 *	to join the ring.
 */
static void
rns_macreset(struct rns_info *ri)
{
	rns_d_bec(0, ri, 0, 0);

	/* turn off the MAC interrupts */
	mac_sto(ri,MAC_CTRL_A,MAC_CA_OFF);
	mac_sto(ri,MAC_CTRL_B,MAC_CB_DEFAULT | MAC_CB_OFF);
	mac_sto(ri,MAC_MASK_A,0);
	mac_sto(ri,MAC_MASK_B,
		ri->ri_mac_mask_b = 0);
	mac_sto(ri,MAC_MASK_C,0);

	ri->ri_smt1.smt_st.rngbroke  = ri->ri_smt1.smt_st.rngop;
	if (PCM_SET(&ri->ri_smt1, PCM_RNGOP)) {
		ri->ri_smt1.smt_st.flags &= ~PCM_RNGOP;
		rns_alarm(&ri->ri_smt1,ri,SMT_ALARM_RNGOP);
	}

	ri->ri_state &= ~RI_ST_MAC;
	ri->ri_smt1.smt_st.mac_state = MAC_OFF;
}


/* initialize the MAC
 *	Interrupts must already be off.
 */
static void
rns_macinit(struct rns_info *ri)
{
	u_short ca;


	rns_macreset(ri);

	/* install 48-bit source address in prototype header
	 */
	ri->ri_hdr.fddi_mac.filler[0] = (u_char)(MAC_DATA_HDR>>16);
	ri->ri_hdr.fddi_mac.filler[1] = (u_char)(MAC_DATA_HDR>>8);
	ri->ri_hdr.fddi_mac.mac_bits = (u_char)(MAC_DATA_HDR);
	ri->ri_hdr.fddi_mac.mac_fc = MAC_FC_LLC(ri->ri_smt1.smt_conf.ip_pri);
	bcopy(&ri->ri_smt1.smt_st.addr,
	      &ri->ri_hdr.fddi_mac.mac_sa,
	      sizeof(ri->ri_hdr.fddi_mac.mac_sa));
	ri->ri_hdr.fddi_llc.llc_c1 = RFC1042_C1;
	ri->ri_hdr.fddi_llc.llc_c2 = RFC1042_C2;
	ri->ri_hdr.fddi_llc.llc_etype = htons(ETHERTYPE_IP);

	/* send MAC address and other parameters to the board
	 */
	rns_set_mmode(ri);
	ca = mac_fet(ri, MAC_CTRL_A);	/* get the control old values */

	/* turn off the MAC to change the control registers */
	mac_sto(ri, MAC_CTRL_A, ca & ~MAC_CA_ON);

	/* set the MAC address */
	mac_sto(ri,MAC_MLA_C, ((ri->ri_hdr.fddi_mac.mac_sa.b[0]<<8)
			       | ri->ri_hdr.fddi_mac.mac_sa.b[1]));
	mac_sto(ri,MAC_MLA_B, ((ri->ri_hdr.fddi_mac.mac_sa.b[2]<<8)
			       | ri->ri_hdr.fddi_mac.mac_sa.b[3]));
	mac_sto(ri,MAC_MLA_A, ((ri->ri_hdr.fddi_mac.mac_sa.b[4]<<8)
			       | ri->ri_hdr.fddi_mac.mac_sa.b[5]));
	mac_sto(ri,MAC_MSA, 0);

	/* set T_Req, TVX, and T_Max */
	mac_sto(ri,MAC_T_REQ, ri->ri_smt1.smt_conf.t_req >> 8);
	mac_sto(ri,MAC_TVX_T_MAX, ((ri->ri_smt1.smt_conf.tvx & ~0xff)
				   | (ri->ri_smt1.smt_conf.t_max>>16 & 0xff)));

	/* set the Mac interrupt masks */
	mac_sto(ri,MAC_MASK_A,MAC_MASK_A_DEFAULT);
	mac_sto(ri,MAC_MASK_B,
		ri->ri_mac_mask_b = MAC_MASK_B_DEFAULT | MAC_MASK_B_MORE);
	mac_sto(ri,MAC_MASK_C,MAC_MASK_C_DEFAULT);

	/* clear any old MAC interrupts */
	(void)mac_fet(ri,MAC_EVNT_A);
	(void)mac_fet(ri,MAC_EVNT_B);
	(void)mac_fet(ri,MAC_EVNT_C);

	/* turn on the MAC */
	mac_sto(ri,MAC_CTRL_B, MAC_CB_DEFAULT);
	mac_sto(ri,MAC_CTRL_A, MAC_CA_ON | ca);

	/* FDDI MAC reset */
	mac_sto(ri,MAC_CTRL_B, MAC_CB_DEFAULT | MAC_CB_RESET_CLAIM);

	ri->ri_state |= RI_ST_MAC;
}



/* Initialize the LEM and turn on PHY and PCM interrupts
 */
static void
rns_phyinit(struct smt *smt,
	    struct rns_info *ri)
{
	struct ri_pcm *pp = SMT1(smt,ri) ? &ri->ri_pcm1 : &ri->ri_pcm2;

	smt_clear(smt);

	bzero(pp, sizeof(*pp));
	pp->ls_prev = PCM_LSU;
	pp->usec_prev = ri->ri_usec;
	rns_lem_accum(smt,ri, pp);
	pp->elm_sw &= ~ELM_LEM_ON;
#if 0
	if (IS_SA(ri))
		pp->elm_ctrl_b |= ELM_CB_CLASS_S;
#endif

	rns_setls(smt, ri, T_PCM_SIG);	/* start line state interrupts */
	(void)rns_st_get(0,ri);
}


/* shut down the PCM
 *  This is also the first step in starting. If we are dual-attached
 *  and	not wrapped, then killing one PHY zaps both rings.
 *
 * Interrupts must already be off
 */
static void
rns_pcm_off(struct smt *smt,
	    struct rns_info *ri)
{
	struct smt *smt2;
	u_long bflag;


	CK_IFNET(ri);

	if (IS_SA(ri)) {
		ASSERT(smt == &ri->ri_smt1);
		smt2 = smt;
	} else {
		smt2 = OTHER_SMT(ri,smt);
	}

	rns_lemoff(smt,ri);
	smt->smt_st.flags &= ~(PCM_CF_JOIN | PCM_THRU_FLAG
			       | PCM_PHY_BUSY | PCM_PC_START);

	/* Just sit tight while the other side is waiting for a response
	 * to trace.
	 */
	if (smt2->smt_st.pcm_state == PCM_ST_TRACE
	    && !PCM_SET(smt2, PCM_SELF_TEST)
	    && iff_alive(ri->ri_if.if_flags)) {
		if (!PCM_SET(smt, PCM_PC_DISABLE))
			rns_setls(smt, ri, R_PCM_QLS);
		smt_new_st(smt, PCM_ST_OFF);
		return;
	}

	/* Reset PHY A if it had been withheld and this is PHY B
	 * being shut off.  (see CEM Isolated_Actions)
	 */
	if (SMT1(smt,ri) && IS_DAS(ri)) {
		if (PCM_SET(smt, PCM_WA_FLAG)) {
			smt->smt_st.flags &= ~(PCM_WA_FLAG
					       | PCM_WAT_FLAG);
			smt2->smt_st.flags &= ~(PCM_WA_FLAG
						| PCM_WAT_FLAG);
			smt_off(smt2);
		} else if (PCM_SET(smt, PCM_WAT_FLAG)) {
			smt->smt_st.flags &= ~PCM_WAT_FLAG;
			smt2->smt_st.flags &= ~PCM_WAT_FLAG;
			if (PC_MODE_T(smt2))
				smt_off(smt2);
		}
	}

	if (PCM_SET(smt2, PCM_CF_JOIN)) {
		/* If we are dual-attached and THRU or WRAP_AB,
		 * then we must WRAP.
		 */
		smt2->smt_st.flags &= ~PCM_THRU_FLAG;
		if (iff_alive(ri->ri_if.if_flags))
			rns_setls(smt2,ri,T_PCM_WRAP);
	} else {
		/* if neither PHY is inserted, shut off the MAC
		 */
		rns_macreset(ri);
	}


	/* compute new state for the optical bypass
	 */
	bflag = 0;
	if (iff_alive(ri->ri_if.if_flags)
	    && !PCM_SET(smt,PCM_SELF_TEST)
	    && !PCM_SET(smt2,PCM_SELF_TEST)) {
		bflag = PCM_INS_BYPASS;
	}

	/* Fiddle with the optical bypass.
	 * Note that this function is not the only code that changes
	 * the optical bypass.
	 */
	if (bflag != (smt->smt_st.flags & PCM_INS_BYPASS)) {
		if (bflag & PCM_INS_BYPASS)
			ri->ri_brd_ctl |= BRD_EN_OBR;
		else
			ri->ri_brd_ctl &= ~BRD_EN_OBR;
		WT_REG(ri, brd_ctl, ri->ri_brd_ctl);
		ri->ri_state |= RI_ST_BCHANGE;
		smt->smt_st.flags = ((smt->smt_st.flags & ~PCM_INS_BYPASS)
				     | bflag);
	}

	/* If needed, wait for the bypass to settle.
	 */
	if (0 != (ri->ri_state & RI_ST_BCHANGE)) {
		ri->ri_state &= ~RI_ST_BCHANGE;
		if (bflag & PCM_INS_BYPASS) {
			SMTDEBUGPRINT(smt,0,("rns%d: inserted\n",ri->ri_unit));
			rns_phyinit(smt,ri);
			if (!IS_SA(ri)) {
				rns_phyinit(smt2,ri);
			}
		} else {
			SMTDEBUGPRINT(smt,0,("rns%d: bypassed\n",ri->ri_unit));
		}
		/* send QLS to meet the TD_Min requirement */
		rns_setls(smt,ri,R_PCM_QLS);
		if (IS_DAS(ri))
			rns_setls(smt2,ri,R_PCM_QLS);

		/* Wait for the bypass to settle, and be sure
		 * something wakes up.
		 */
		smt_new_st(smt, PCM_ST_BYPASS);
		TPC_RESET(smt);
		if (smt != smt2) {
			smt_new_st(smt2,PCM_ST_BYPASS);
			TPC_RESET(smt2);
		}
		return;
	}

	if (!PCM_SET(smt, PCM_PC_DISABLE)) {
		if (PCM_ST_BYPASS == smt->smt_st.pcm_state) {
			SMT_USEC_TIMER(smt,SMT_I_MAX);
			if (smt != smt2)
				SMT_USEC_TIMER(smt2,SMT_I_MAX);
		} else {
			if (bflag & PCM_INS_BYPASS) {
				/* let the board start */
				smt->smt_st.flags |= PCM_PC_START;
				TPC_RESET(smt);
				SMT_USEC_TIMER(smt,SMT_TB_MIN);
			}
			elm_pc_off(ri,smt);
		}
	}
}



/* tell the ELM to signal some bits
 */
static void
rns_setvec(struct smt *smt,
	   struct rns_info *ri,
	   int n)
{
	smt->smt_st.flags |= PCM_PHY_BUSY;
	if (SMT1(smt,ri)) {
		ri->ri_pcm1.smt_n = n;
	} else {
		ri->ri_pcm2.smt_n = n;
	}

	elm_sto(ri,smt, ELM_XMIT_LEN, n-1);
	elm_sto(ri,smt, ELM_XMIT_VEC, smt->smt_st.t_val >> smt->smt_st.n);

	if (smt->smt_st.n == 0)
		elm_pc_cmd(ri,smt, ELM_CB_PCM_START);

	rns_elm_mask(ri,smt, 0);
}


/* Start Link Confidence Test
 *	Interrupts must be off
 */
static void
rns_lct_on(struct smt *smt,
	   struct rns_info *ri)
{
	rns_setls(smt,ri,T_PCM_LCT);
}


/* Stop LCT and return results
 *	Interrupts must be off
 */
static int				/* 1==LCT failed */
rns_lct_off(struct smt *smt,
	    struct rns_info *ri)
{
	rns_setls(smt, ri,T_PCM_LCTOFF);

	return (smt->smt_st.ler2	/* fail if LER bad */
		<= smt->smt_conf.ler_cut);
}


/* Tell the ELM to finish the PCM signaling
 *	Interrupts must be off
 */
static void
rns_join(struct smt *smt,
	 struct rns_info *ri)
{
	CK_IFNET(ri);
	smt->smt_st.flags |= PCM_PHY_BUSY;
	elm_pc_cmd(ri,smt, ELM_CB_PC_JOIN);
	rns_elm_mask(ri,smt, 0);
}



/* Join the ring
 *	Interrupts must be off
 *	This implements much of the Configuration Element Management state
 *	machine.
 */
static void
rns_cfm(struct smt *smt,
	 struct rns_info *ri)
{
	struct smt *smt2;
	int st;


	CK_IFNET(ri);
	if (PCM_SET(smt, PCM_CF_JOIN))
		return;

	if (IS_SA(ri)) {
		smt2 = smt;
	} else {
		smt2 = OTHER_SMT(ri,smt);
	}

	/* tell the MAC about itself */
	if (!(ri->ri_state & RI_ST_MAC))
		rns_macinit(ri);

	/* stop directed beacon */
	if (ri->ri_state & RI_ST_D_BEC)
		rns_d_bec(smt, ri, 0, 0);


	/* If we are dual-attached and the other PHY is wrapped, then
	 *	we have to unwrap it.  If the other PHY is not alive,
	 *	then we must wrap.
	 *
	 * Zap PHY A if this is B and we are in a tree and not
	 * single attached or dual-MAC.  (See CEM Insert_Actions)
	 */
	st = 0;
	if (SMT1(smt,ri) && IS_DAS(ri)) {
		if (PC_MODE_T(smt)) {
			smt->smt_st.flags |= PCM_WA_FLAG;
			smt2->smt_st.flags |= PCM_WA_FLAG;
			st = 1;
		} else {
			smt->smt_st.flags |= PCM_WAT_FLAG;
			smt2->smt_st.flags |= PCM_WAT_FLAG;
			if (PC_MODE_T(smt2))
				st = 1;
		}
	}

	smt->smt_st.flags |= PCM_CF_JOIN;
	if (st) {
		rns_setls(smt, ri, T_PCM_WRAP);
		smt_off(smt2);
	} else if (PCM_SET(smt2, PCM_CF_JOIN)
		   && (IS_SA(ri)
		       || (!PC_MODE_T(smt) && !PC_MODE_T(smt2)))) {
		smt->smt_st.flags |= PCM_THRU_FLAG;
		rns_setls(smt, ri, T_PCM_THRU);
		smt2->smt_st.flags |= PCM_THRU_FLAG;
		rns_setls(smt2, ri, T_PCM_THRU);
	} else {
		rns_setls(smt, ri, T_PCM_WRAP);
	}

	rns_lemon(smt,ri);

	if (ri->ri_state & RI_ST_ARPWHO) {
		ri->ri_state &= ~RI_ST_ARPWHO;
#if defined(FDDI_FICUS) || defined(FDDI_BONSAI)
		arpwhohas(&ri->ri_ac, &ri->ri_ac.ac_ipaddr);
#else
		ARP_WHOHAS(&ri->ri_ac, &ri->ri_ac.ac_ipaddr);
#endif
	}
}


/* help propagate TRACE upstream by finding the other PCM state machine
 */
static struct smt*
rns_osmt(struct smt *smt,
	  struct rns_info *ri)
{
	if (IS_DAS(ri))
		return OTHER_SMT(ri,smt);
	return smt;
}


/* process a new "line state"
 */
static void
rns_new_rls(struct smt *smt,
	    struct rns_info *ri,
	    unchar ls)
{
	struct smt *osmt;

	switch (ls) {
	case RCV_TRACE_PROP:
		/* PC(88c) and PC(82)
		 *
		 * If we are the MAC that is supposed to
		 * respond to PC_Trace, then do so.
		 * Otherwise, propagate it.
		 *
		 * If we are are single-MAC, dual-attached, "THRU,"
		 * on the primary ring, then we are not the
		 * PHY that is being "traced," and should
		 * ask the board to propagate the TRACE.
		 * Otherwise, we are the target and should
		 * respond with QLS.
		 *
		 * When the destination MAC responds with QLS,
		 * we will recycle.
		 *
		 * Delay the restart to allow the ring to
		 * do something useful in case we are the
		 * cause, and instead of a path test.
		 */

		if (smt->smt_st.pcm_state != PCM_ST_ACTIVE
		    || PCM_SET(smt, PCM_TRACE_OTR))
			break;

		osmt = rns_osmt(smt, ri);
		if (smt->smt_conf.type == SM_DAS
		    && smt->smt_conf.pc_type == PC_A
		    && PCM_SET(smt,PCM_THRU_FLAG)) {
			SMTDEBUGPRINT(smt,0,
				      ("%sMLS--trace prop\n", PC_LAB(smt)));
			smt->smt_st.flags |= PCM_TRACE_OTR;

			TPC_RESET(osmt);
			SMT_USEC_TIMER(osmt, SMT_TRACE_MAX);
			smt_new_st(osmt, PCM_ST_TRACE);
			osmt->smt_st.flags &= ~PCM_PC_DISABLE;
			elm_pc_cmd((struct rns_info*)osmt->smt_pdata,
				   osmt, ELM_CB_PCM_TRACE);

		} else {
			SMTDEBUGPRINT(smt,0, ("%sMLS--traced\n",PC_LAB(smt)));
			smt_fin_trace(smt,osmt);
		}
		break;

	case RCV_SELF_TEST:
		if (smt->smt_st.pcm_state == PCM_ST_TRACE)
			smt->smt_st.flags |= PCM_SELF_TEST;
		break;

	case RCV_BREAK:
		smt_off(smt);
		break;

	case RCV_PCM_CODE:
		smt->smt_st.flags &= ~PCM_PHY_BUSY;
		break;

	case PCM_NLS:
		/* avoid bogus noise as the light fades
		 */
		smt->smt_st.flags |= (PCM_NE | PCM_LEM_FAIL);
		SMT_LSLOG(smt,PCM_NLS);
		SMTDEBUGPRINT(smt,1, ("%sNoise Event\n", PC_LAB(smt)));
		break;

	case PCM_LSU:			/* ignore it */
		break;

	default:
		smt->smt_st.rls = (enum pcm_ls)ls;
		SMT_LSLOG(smt,ls);
		SMTDEBUGPRINT(smt,4, ("%sR=%s ",PC_LAB(smt), str_ls[ls]));
		break;
	}

	moto_pcm(smt);
}


/* initialize the board with this data
 */
static __uint32_t fsi_init[] = {
    FSI_PCRA(FSI_PCR_ON),
    FSI_PCRB(FSI_PCR_ON),
    FSI_PMPA(0xf),
    FSI_PMPB(0x3f),			/* 4K PCI page--why? */

    FSI_BLRA(0x3f),			/* 256 byte burst */
    FSI_BLRB(0x3f),			/* 256 byte PCI burst--why? */

    FSI_RPR(DB_RING_N, FSI_RPR_RPA|FSI_RPR_RDA|DB_RING_LN),
    FSI_RPR(TX_RING_N, FSI_RPR_RPA|FSI_RPR_RDA|TX_RING_LN),
    FSI_RPR(RX_RING_N, FSI_RPR_RPA|FSI_RPR_RDA|RX_RING_LN),
    FSI_RPR(PM_RING_N, FSI_RPR_RPA|FSI_RPR_RDA|PM_RING_LN),

    FSI_CPR(6, 0x10),			/* no parity, port B */
    FSI_CPR(7, 0x10),

    FSI_PER(TX_RING_N, 3),		/* no parity, 64K local mem */
    FSI_PER(RX_RING_N, 3),

    /* Since the TX ring uses local memory, it does not need much
     * FSI memory.
     * The DB ring is only 8 descriptors long for buffers less
     * than 32-bytes long.  It needs no more than 8 32-byte blocks
     * of FSI internal memory.
     * The promiscuous-mode MAC input ring is only 4 descriptors
     * long for packets about 27 bytes long.  It is expected
     * to be filled and then overflow.  It needs no more than
     * 4 32-byte blocks of FSI internal memory.
     */
    FSI_FWR(TX_RING_N, 512/FSI_MEM_BLK),
    FSI_FWR(DB_RING_N, 8),
    FSI_FWR(RX_RING_N, 512/FSI_MEM_BLK),
    FSI_FWR(PM_RING_N, FSI_FWR_MIN),

    FSI_LMT(TX_RING_N, (LOCAL_MEM_LEN/2)/FDDI_MAXOUT-1),
    FSI_LMT(DB_RING_N, 0),
    FSI_LMT(RX_RING_N, (LOCAL_MEM_LEN/2)/FDDI_MAXIN-1),
    FSI_LMT(PM_RING_N, 0),

    FSI_RFR(RX_RING_N, FSI_RFR_DATA),
    FSI_RFR(PM_RING_N, FSI_RFR_MAC),

    FSI_RMR(RX_RING_N, (FDDI_MAXIN+FSI_MEM_BLK-1)/FSI_MEM_BLK-1),
    FSI_RMR(PM_RING_N, FSI_RMR_MIN),

    FSI_RBR(RX_RING_N, RX_BUF_LEN/FSI_RBR_MIN),
    FSI_RBR(PM_RING_N, PM_BUF_LEN/FSI_RBR_MIN),

    /* double-buffer */
    FSI_ICR(FSI_ICR_DBM),

    FSI_MTR(FSI_MTR_TE),

    FSI_STL0(50),			/* 50*40.96 usec = 2.048 ms ticks */
    FSI_STL1(0),
};


/* reset a board because we do not want to play any more at all, because
 *  a chip just paniced, or because we need to change the MAC address.
 *
 *	If we are playing dual-attachment, then both PHYs must be reset.
 *	Interrupts must already be off.
 */
static void
rns_reset(struct rns_info *ri)
{
	int i;
	struct mbuf *m;


	CK_IFNET(ri);

	ri->ri_state &= ~RI_ST_WHINE;	/* allow more complaints */

	if ((ri->ri_state & RI_ST_ZAPPED)
	    || !iff_alive(ri->ri_if.if_flags)) {
		/* Reset ELM and IFDDI */
		ZAPBD(ri);

		/* Resetting the board turns things off.
		 * So mark everything off.
		 */
		bzero(ri->ri_cam, sizeof(ri->ri_cam));

		ri->ri_smt1.smt_st.tls = R_PCM_QLS;
		ri->ri_smt2.smt_st.tls = R_PCM_QLS;
		ri->ri_state &= ~RI_ST_MAC;
		ri->ri_smt1.smt_st.mac_state = MAC_OFF;

		ri->ri_smt1.smt_st.flags &= (PCM_SAVE_FLAGS & ~PCM_INS_BYPASS);
		ri->ri_smt2.smt_st.flags &= (PCM_SAVE_FLAGS & ~PCM_INS_BYPASS);
		ri->ri_state |= RI_ST_BCHANGE;

		bzero(&ri->ri_dbg, sizeof(ri->ri_dbg));

		/* Keep BRD_RESET on "800 nsec" according to RNS ASIC manual.
		 * Delay at least 24 usec after reset, or 600 FSICLK
		 * according to start of chapter 11 of IFDD manual.
		 */
		DELAY(4);
		rns_flush(ri);
		WT_REG(ri, brd_ctl, (ri->ri_brd_ctl &= ~BRD_RESET));
		DELAY(32);

		/* reset the FSI inside the IFDDI just in case,
		 * and since the RNS softare does.
		 */
		(void)fcr_fet(ri,FSI_SWR);
		WT_REG(ri, fsi_imr1, 0);
		WT_REG(ri, fsi_imr2, 0);

		rns_macreset(ri);

		/* clear the RX ring */
		ri->ri_rx_desc_nxt = 0;
		for (i = 0; i < RX_RING_LEN; i++) {
			ri->ri_hdma->rx_desc[i].a=FSI_DESC_A(ri->ri_bdma
							->rx[i].b);
			ri->ri_hdma->rx_desc[i].hi = RX_DESC_HI;
		}

		/* clear MAC promiscuous ring */
		ri->ri_pm_desc_nxt = 0;
		for (i = 0; i < PM_RING_LEN; i++) {
			ri->ri_hdma->pm_desc[i].a = FSI_DESC_A(ri->ri_bdma
							->pm[i].b);
			ri->ri_hdma->pm_desc[i].hi = PM_DESC_HI;
		}

		/* clear the TX ring */
		bzero((void *)ri->ri_hdma->tx_desc,
		      sizeof(ri->ri_hdma->tx_desc));
		ri->ri_tx_job_head = ri->ri_tx_job_tail = ri->ri_tx_jobs;
		ri->ri_tx_desc_old = 0;
		ri->ri_tx_desc_nxt = 0;
		ri->ri_tx_desc_free = TX_RING_LEN-1;
		ri->ri_tx_hbuf_nxt = &ri->ri_hdma->tx;
		ri->ri_tx_bbuf_nxt = ri->ri_bdma->tx.b;
		ri->ri_tx_buf_free = sizeof(ri->ri_hdma->tx)-FDDI_MAXOUT;
		for (;;) {
			IF_DEQUEUE(&ri->ri_if.if_snd, m);
			if (!m)
				break;
			m_freem(m);
		}

		/* use the 2nd ELM if present */
		if (IS_DAS(ri))
			fcr_sto(ri, FCR_CAMEL(CAMEL_CNTRL,CAMEL_BY_CNTRL));

		elm_sto2(ri,ELM_A_MAX, TPC_TIME(SMT_C_MIN));

		/* Use a long TL_Min to be heard and to ensure host
		 * hears line state changes */
		elm_sto2(ri,ELM_LS_MAX, TPC_TIME(1000));
		elm_sto2(ri,ELM_TB_MIN, TPC_TIME(SMT_TB_MIN));
		elm_sto2(ri,ELM_T_OUT,  TPC_TIME(SMT_T_OUT));
		elm_sto2(ri,ELM_T_SCRUB,TPC_TIME(SMT_T_SCRUB));
		elm_sto2(ri,ELM_NS_MAX, NOISE_TIME(SMT_NS_MAX));

		/* not really used */
		elm_sto2(ri,ELM_LE_THRESH,200);
		elm_sto2(ri,ELM_LC_SHORT,-16);

		/* set CDDI values if appropriate */
		if (ri->ri_product->p_attr & RNS_UTP) {
			elm_sto2(ri,ELM_FOTOFF_ON,0xfd76);
			elm_sto2(ri,ELM_FOTOFF_OFF,0);
			elm_sto2(ri,ELM_CIPHER, (ELM_CPI_SDONEN
						 | ELM_CPI_SDOFFEN
						 | ELM_CPI_CIPHER_ENABLE));
		}

		/* program the FSI */
		for (i = 0; i < sizeof(fsi_init)/sizeof(fsi_init[0]); i++)
			fcr_sto(ri, fsi_init[i]);

		/* define local memory */
		(void)cmr_sto(ri, FSI_CMD(FSI_CMD_SET_LOCAL,
					  RX_RING_N, 0,
					  LOCAL_MEM_BASE));
		(void)cmr_sto(ri, FSI_CMD(FSI_CMD_SET_LOCAL,
					  TX_RING_N, 0,
					  LOCAL_MEM_BASE+LOCAL_MEM_LEN/2));

		/* define and ready the RX and promiscuous MAC rings */
		(void)cmr_sto(ri, FSI_CMD(FSI_CMD_DEFRDY_LOC,
					  RX_RING_N,
					  ri->ri_bdma->rx_desc,
					  ri->ri_bdma->rx_desc));
		(void)cmr_sto(ri, FSI_CMD(FSI_CMD_DEFRDY,
					  PM_RING_N,
					  ri->ri_bdma->pm_desc,
					  ri->ri_bdma->pm_desc));

		/* define the TX ring, but do not make it ready */
		(void)cmr_sto(ri, FSI_CMD(FSI_CMD_DEFR_LOC,
					  TX_RING_N,
					  ri->ri_bdma->tx_desc,
					  ri->ri_bdma->tx_desc));

		ri->ri_state &= ~RI_ST_ZAPPED;
	}

	/* turn interrupts on or off */
	if (iff_alive(ri->ri_if.if_flags)) {
		fcr_sto(ri, FCR_CAMEL(CAMEL_MASK, CAMEL_NP_ERR));
		ri->ri_imr1 = INIT_IMR1;
		WT_REG(ri, brd_int, BRD_INT_ENABLE);
		if (IS_DAS(ri))
			ri->ri_brd_ctl |= BRD_ELM_INT_EN;
	} else {
		fcr_sto(ri, FCR_CAMEL(CAMEL_MASK, 0));
		ri->ri_imr1 = 0;
		WT_REG(ri, brd_int, 0);
	}
	WT_REG(ri, fsi_imr1, ri->ri_imr1);
	WT_REG(ri, brd_ctl, ri->ri_brd_ctl);
	rns_elm_mask(ri,&ri->ri_smt1, ri->ri_pcm1.elm_sw & ELM_NOISE);
	rns_elm_mask(ri,&ri->ri_smt2, ri->ri_pcm2.elm_sw & ELM_NOISE);

	/* Notice an optical bypass switch on a DAS board.
	 * A switch can be plugged in at any time.
	 */
	if (IS_DAS(ri)
	    && (RD_REG(ri, brd_ctl) & BRD_OBR_PRESENT)) {
		ri->ri_smt1.smt_st.flags |= PCM_HAVE_BYPASS;
		ri->ri_smt2.smt_st.flags |= PCM_HAVE_BYPASS;
	} else {
		ri->ri_smt1.smt_st.flags &= ~PCM_HAVE_BYPASS;
		ri->ri_smt2.smt_st.flags &= ~PCM_HAVE_BYPASS;
	}

	/* Reset the MAC, send QLS, and restart things.
	 */
	smt_off(&ri->ri_smt1);
	if (IS_DAS(ri))
		smt_off(&ri->ri_smt2);

	if (iff_dead(ri->ri_if.if_flags))
		if_down(&ri->ri_if);
}


/* initialize interface
 */
static void
rns_start(struct rns_info *ri,
	  int flags)
{
	int dflags;
#ifdef FDDI_BONSAI
	int s = splimp();
#endif

#if defined(FDDI_FICUS) || defined(FDDI_BONSAI)
	flags |= IFF_NOTRAILERS;
#endif
	if (flags & IFF_UP)
		flags |= IFF_ALIVE;
	else
		flags &= ~IFF_ALIVE;

	CK_IFNET(ri);
	if (ri->ri_if.in_ifaddr == 0)
		flags &= ~IFF_ALIVE;	/* dead if no address */

	dflags = flags ^ ri->ri_if.if_flags;
	ri->ri_if.if_flags = flags;	/* set here to allow overrides */
	if (0 != (dflags & IFF_ALIVE)) {
		ri->ri_state |= RI_ST_ARPWHO;
		rns_reset(ri);
		if (iff_alive(ri->ri_if.if_flags)) {
			moto_pcm(&ri->ri_smt1);	/* start joining ring */
			if (IS_DAS(ri))
				moto_pcm(&ri->ri_smt2);
		}
	}

	if (dflags & IFF_PROMISC)
		rns_set_mmode(ri);

	if (rns_mtu > 128 && rns_mtu < FDDIMTU)
		ri->ri_if.if_mtu = rns_mtu;
	else
		ri->ri_if.if_mtu = FDDIMTU;
#ifdef FDDI_BONSAI
	splx(s);
#endif
}


/* Process an ioctl request.
 */
static int				/* return 0 or error number */
rns_ioctl(struct ifnet *ifp,
	  int cmd,
	  void *data)
{
#define ri ((struct rns_info*)ifp)
	int flags;
	int error = 0;
#ifdef FDDI_BONSAI
	int s;
#endif

	ASSERT(ri->ri_if.if_name == rns_name);
	CK_IFNET(ri);

	switch (cmd) {
	case SIOCAIFADDR: {		/* Add interface alias address */
		struct sockaddr *addr = _IFADDR_SA(data);
#ifdef FDDI_BONSAI
		s = splimp();
#endif
#ifdef INET6
		/*
		 * arp is only for v4.  So we only call arpwhohas if there is
		 * an ipv4 addr.
		 */
		if (ifp->in_ifaddr != 0)
			arprequest(&ri->ri_ac, 
				&((struct sockaddr_in*)addr)->sin_addr,
				&((struct sockaddr_in*)addr)->sin_addr,
				(&ri->ri_ac)->ac_enaddr);
#else
		arprequest(&ri->ri_ac, 
			&((struct sockaddr_in*)addr)->sin_addr,
			&((struct sockaddr_in*)addr)->sin_addr,
			(&ri->ri_ac)->ac_enaddr);
#endif
#ifdef FDDI_BONSAI
		splx(s);
#endif
		
		break;
	}

	case SIOCDIFADDR:		/* Delete interface alias address */
		/* currently nothing needs to be done */
		break;

	case SIOCSIFADDR: {
		struct sockaddr *addr = _IFADDR_SA(data);
#ifdef FDDI_BONSAI
		s = splimp();
#endif
		switch (addr->sa_family) {
		case AF_INET:
			ri->ri_ac.ac_ipaddr = ((struct sockaddr_in*
						)addr)->sin_addr;
			ri->ri_if.if_flags &= ~IFF_RUNNING;
			rns_start(ri, ri->ri_if.if_flags|IFF_UP);
			break;

		case AF_RAW:
			/* It is not safe to change the address while the
			 * board is alive.
			 */
			if (!iff_dead(ri->ri_if.if_flags)) {
				error = EINVAL;
			} else {
				rns_set_addr(ri, (u_char*)addr->sa_data);
			}
			break;

		default:
			error = EINVAL;
			break;
		}
#ifdef FDDI_BONSAI
		splx(s);
#endif
		} break;

	case SIOCSIFFLAGS:
		flags = ((struct ifreq *)data)->ifr_flags;

		if ((flags & IFF_UP) != 0) {
			if (ri->ri_if.in_ifaddr == 0)
				return EINVAL;	/* quit if no address */
#ifdef FDDI_BONSAI
			s = splimp();
#endif
			rns_start(ri, flags);
		} else {
			flags &= ~IFF_RUNNING;
#ifdef FDDI_BONSAI
			s = splimp();
#endif
			ri->ri_if.if_flags = flags;
			rns_reset(ri);
		}
		if (0 != (ri->ri_if.if_flags & IFF_DEBUG)) {
			ri->ri_smt1.smt_st.flags |= PCM_DEBUG;
			ri->ri_smt2.smt_st.flags |= PCM_DEBUG;
		} else {
			ri->ri_smt1.smt_st.flags &= ~PCM_DEBUG;
			ri->ri_smt2.smt_st.flags &= ~PCM_DEBUG;
		}
		if (0 != ((flags ^ ri->ri_if.if_flags) & IFF_ALIVE)) {
			error = ENETDOWN;
		}
#ifdef FDDI_BONSAI
		splx(s);
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI: {
#define MKEY ((union mkey*)data)
		int allmulti;
		error = ether_cvtmulti((struct sockaddr *)data, &allmulti);
		if (0 == error) {
#ifdef FDDI_BONSAI
			s = splimp();
#endif
			if (allmulti) {
				if (SIOCADDMULTI == cmd) {
					ri->ri_if.if_flags |= IFF_ALLMULTI;
				} else {
					ri->ri_if.if_flags &= ~IFF_ALLMULTI;
				}
				rns_set_mmode(ri);
			} else {
				bitswapcopy(MKEY->mk_dhost, MKEY->mk_dhost,
					    sizeof(MKEY->mk_dhost));
				if (SIOCADDMULTI == cmd) {
					error = rns_add_da(ri, MKEY, 1);
				} else {
					error = rns_del_da(ri, MKEY, 1);
				}
			}
#ifdef FDDI_BONSAI
			splx(s);
#endif
		}
#undef MKEY
		} break;

	case SIOCADDSNOOP:
	case SIOCDELSNOOP: {
#define	SF(nm) ((struct lmac_hdr*)&(((struct snoopfilter *)data)->nm))
		u_char *a;
		union mkey key;

		a = &SF(sf_mask[0])->mac_da.b[0];
		if (!FDDI_ISBROAD_SW(a)) {
			/* cannot filter on board unless the mask is trivial.
			 */
			error = EINVAL;

		} else {
			/* Filter individual destination addresses.
			 *	Use a different address family to avoid
			 *	damaging an ordinary multi-cast filter.
			 */
			a = &SF(sf_match[0])->mac_da.b[0];
			key.mk_family = AF_RAW;
			bcopy(a, key.mk_dhost, sizeof(key.mk_dhost));
#ifdef FDDI_BONSAI
			s = splimp();
#endif
			if (cmd == SIOCADDSNOOP) {
				error = rns_add_da(ri, &key,
						   FDDI_ISGROUP_SW(a));
			} else {
				error = rns_del_da(ri, &key,
						   FDDI_ISGROUP_SW(a));
			}
#ifdef FDDI_BONSAI
			splx(s);
#endif
		}
#undef SF
		} break;

	default:
		error = smt_ioctl(&ri->ri_if, &ri->ri_smt1, &ri->ri_smt2,
				  cmd, (struct smt_sioc*)data);
		break;
	}

	return error;
#undef ri
}


/* See if a DLPI function wants a frame.
 */
static int
rns_dlp(struct rns_info *ri,
	int port,
	int encap,
	struct mbuf *m,
	int len)
{
	dlsap_family_t *dlp;
	struct mbuf *m2;
	FDDI_IBUFLLC *fhp;

	dlp = dlsap_find(port, encap);
	if (!dlp)
		return 0;

	/* The DLPI code wants to get the entire MAC and LLC headers.
	 * It needs the total length of the mbuf chain to reflect
	 * the actual data length, not to be extended to contain
	 * a fake, zeroed LLC header which keeps the snoop code from
	 * crashing.
	 */
	m2 = m_copy(m, 0, len+sizeof(FDDI_IBUF));
	if (!m2)
		return 0;
	if (M_HASCL(m2)) {
		m2 = m_pullup(m2, sizeof(*fhp));
		if (!m2)
			return 0;
	}
	fhp = mtod(m2, FDDI_IBUFLLC*);

	/* The DLPI code wants the MAC addresses in cannonical bit order.
	 */
	bitswapcopy(&fhp->fbh_fddi.fddi_mac.mac_da,
		    &fhp->fbh_fddi.fddi_mac.mac_da,
		    sizeof(fhp->fbh_fddi.fddi_mac.mac_da)
		    +sizeof(fhp->fbh_fddi.fddi_mac.mac_sa));

	/* The DLPI code wants the LLC header to not be hidden with the
	 * MAC header.
	 */
	fhp->fbh_ifh.ifh_hdrlen -= sizeof(struct llc);

	if ((*dlp->dl_infunc)(dlp, &ri->ri_if, m2,
			      &fhp->fbh_fddi.fddi_mac)) {
		m_freem(m);
		return 1;
	}
	m_freem(m2);
	return 0;
}


/* deal with an input packet
 */
static void
rns_input(struct rns_info *ri,
	  struct mbuf *m,
	  FDDI_IBUFLLC *fhp,
	  int totlen)			/* number of bytes from the board */
{
	int snoopflags = 0;
	uint port;


	IF_INITHEADER(fhp, &ri->ri_if, (int)sizeof(FDDI_IBUFLLC));

	/* keep the packet large enough to keep the snoop code from
	 * calling panic or going crazy.
	 */
	if (totlen < FDDI_MIN_LLC)
		bzero((char*)fhp+totlen+sizeof(FDDI_IBUF),
		      FDDI_MIN_LLC - totlen);

	ri->ri_if.if_ibytes += totlen-FDDI_FILLER;
	ri->ri_if.if_ipackets++;

	if (0 != (fhp->fbh_fddi.fddi_mac.mac_bits
		  & (MAC_ERROR_BIT | MAC_E_BIT))) {
		ri->ri_if.if_ierrors++;
		snoopflags = (SN_ERROR|SNERR_FRAME);

	} else if (totlen < sizeof(struct fddi)) {
		if (totlen < sizeof(struct lmac_hdr)) {
			ri->ri_if.if_ierrors++;
			snoopflags = (SN_ERROR|SNERR_TOOSMALL);
		}
		port = FDDI_PORT_LLC;

	} else {
#		define FSA fddi_mac.mac_sa.b
#		define FDA fddi_mac.mac_da.b
#		define MY_SA() (fhp->fbh_fddi.FSA[sizeof(LFDDI_ADDR)-1] \
				== ri->ri_hdr.FSA[sizeof(LFDDI_ADDR)-1]	\
				&& !bcmp(fhp->fbh_fddi.FSA,		\
					 ri->ri_hdr.FSA,		\
					 sizeof(LFDDI_ADDR)))
#		define OTR_DA() (fhp->fbh_fddi.FDA[sizeof(LFDDI_ADDR)-1] \
				 != ri->ri_hdr.FSA[sizeof(LFDDI_ADDR)-1] \
				 || bcmp(fhp->fbh_fddi.FDA,		\
					 ri->ri_hdr.FSA,		\
					 sizeof(LFDDI_ADDR)))

		/* Notice if it is a broadcast or multicast frame.
		 * Get rid of imperfectly filtered multicasts
		 */
		if (FDDI_ISGROUP_SW(fhp->fbh_fddi.FDA)) {
			/* Check for a packet from ourself.
			 */
			if (MY_SA()) {
				ri->ri_dbg.own_out++;
				m_freem(m);
				return;
			}
			if (FDDI_ISBROAD_SW(fhp->fbh_fddi.FDA)) {
				m->m_flags |= M_BCAST;
			} else {
				if (0 == (ri->ri_ac.ac_if.if_flags
					  & IFF_ALLMULTI)
				    && !mfethermatch(&ri->ri_filter,
						     fhp->fbh_fddi.FDA, 0)) {
					if (RAWIF_SNOOPING(&ri->ri_rawif)
					    && snoop_match(&ri->ri_rawif,
						     (caddr_t)&fhp->fbh_fddi,
							   totlen)) {
						snoopflags = SN_PROMISC;
					} else {
						m_freem(m);
						return;
					}
				}
				m->m_flags |= M_MCAST;
			}
			ri->ri_if.if_imcasts++;

		} else if ((ri->ri_state & RI_ST_PROM)
			   && OTR_DA()) {
			if (MY_SA()) {
				ri->ri_dbg.own_out++;
				m_freem(m);
				return;
			}

			if (RAWIF_SNOOPING(&ri->ri_rawif)
			    && snoop_match(&ri->ri_rawif,
					   (caddr_t)&fhp->fbh_fddi,
					   totlen)) {
				snoopflags = SN_PROMISC;
			} else {
				m_freem(m);
				return;
			}
		}


		if (MAC_FC_IS_LLC(fhp->fbh_fddi.fddi_mac.mac_fc)) {
			if (totlen >= FDDI_MIN_LLC
			    && fhp->fbh_fddi.fddi_llc.llc_c1 == RFC1042_C1
			    && fhp->fbh_fddi.fddi_llc.llc_c2 == RFC1042_C2) {
				port= ntohs(fhp->fbh_fddi.fddi_llc.llc_etype);
			} else {
				port = FDDI_PORT_LLC;
			}
		} else if (MAC_FC_IS_SMT(fhp->fbh_fddi.fddi_mac.mac_fc)) {
			port = FDDI_PORT_SMT;
		} else if (MAC_FC_IS_MAC(fhp->fbh_fddi.fddi_mac.mac_fc)) {
			port = FDDI_PORT_MAC;
		} else if (MAC_FC_IS_IMP(fhp->fbh_fddi.fddi_mac.mac_fc)) {
			port = FDDI_PORT_IMP;
		} else {
			snoopflags = SN_PROMISC;
		}
#undef FSA
#undef FDA
#undef MY_SA
	}

	/* do raw snooping
	 */
	if (RAWIF_SNOOPING(&ri->ri_rawif)) {
		if (!snoop_input(&ri->ri_rawif, snoopflags,
				 (caddr_t)&fhp->fbh_fddi,
				 m,
				 (totlen > sizeof(struct fddi)
				  ? totlen-sizeof(struct fddi) : 0))) {
		}
		if (0 != snoopflags)
			return;

	} else if (0 != snoopflags) {
#if defined(FDDI_FICUS) || defined(FDDI_BONSAI)
drop:
#endif
		/* if bad, count and skip it, and let snoopers know */
		m_freem(m);
		if (RAWIF_SNOOPING(&ri->ri_rawif))
			snoop_drop(&ri->ri_rawif, snoopflags,
				   (caddr_t)&fhp->fbh_fddi, totlen);
		if (RAWIF_DRAINING(&ri->ri_rawif))
			drain_drop(&ri->ri_rawif, port);
		return;
	}

	/* If it is a frame we understand, then give it to the
	 *	correct protocol code.
	 */
	switch (port) {
	case ETHERTYPE_IP:
#if defined(FDDI_FICUS) || defined(FDDI_BONSAI)
		/* Put it on the protocol queue.
		 */
		if (IF_QFULL(&ipintrq)) {
			ri->ri_if.if_iqdrops++;
			ri->ri_if.if_ierrors++;
			IF_DROP(&ipintrq);
			goto drop;
		}
		IF_ENQUEUE(&ipintrq, m);
		schednetisr(NETISR_IP);
#else
		if (network_input(m, AF_INET, 0)) {
			ri->ri_if.if_iqdrops++;
			ri->ri_if.if_ierrors++;
		}
#endif
		return;

	case ETHERTYPE_ARP:
		/* Assume that if_ether.c can understand arp_hrd
		 *	if we strip the LLC header.
		 */
		arpinput(&ri->ri_ac, m);
		return;

	case FDDI_PORT_LLC:
		if (totlen >= sizeof(struct lmac_hdr)+2
		    && rns_dlp(ri, fhp->fbh_fddi.fddi_llc.llc_dsap,
			       DL_LLC_ENCAP, m, totlen))
			return;
		break;

	default:
		if (rns_dlp(ri, port, DL_SNAP0_ENCAP, m, totlen))
			return;
		break;
	}

	/* if we cannot find a protocol queue, then flush it down the
	 *	drain, if it is open.
	 */
	if (RAWIF_DRAINING(&ri->ri_rawif)) {
		drain_input(&ri->ri_rawif, port,
			    (caddr_t)&fhp->fbh_fddi.fddi_mac.mac_sa, m);
	} else {
		ri->ri_if.if_noproto++;
		m_freem(m);
	}
}


/* deal with ordinary junk
 */
static void
rx_junk(struct rns_info *ri,
	int desc)
{
	UPCNT(ri,tot_junk,1);

	if (desc & FSI_IND_RCV_OE) {
		UPCNT(ri,rx_ovf,1);
		ri->ri_if.if_ierrors++;
	}
}


/* set MAC header bits for incoming packet
 */
static void
rns_set_mac_bits(struct rns_info *ri,
		 int desc,
		 FDDI_IBUFLLC *fhp)
{
	fhp->fbh_fddi.fddi_mac.filler[0] = 0;
	fhp->fbh_fddi.fddi_mac.filler[1] = 0;
	fhp->fbh_fddi.fddi_mac.mac_bits = ((desc & FSI_IND_RCV_DA_LSB)
					   ? MAC_DA_BIT : 0);
	if (0 != (desc & FSI_IND_RCV_CNT)) {
		if (desc & FSI_IND_RCV_E) {
			fhp->fbh_fddi.fddi_mac.mac_bits |= MAC_E_BIT;
			if (desc & FSI_IND_RCV_DA_LSB)
				UPCNT(ri, e,1);
		}
		if (desc & FSI_IND_RCV_A) {
			fhp->fbh_fddi.fddi_mac.mac_bits |= MAC_A_BIT;
			if (desc & FSI_IND_RCV_DA_LSB)
				UPCNT(ri, a,1);
		}
		if (desc & FSI_IND_RCV_C) {
			fhp->fbh_fddi.fddi_mac.mac_bits |= MAC_C_BIT;
			if (desc & FSI_IND_RCV_DA_LSB)
				UPCNT(ri, c,1);
		}
		if (desc & FSI_IND_RCV_CRC) {
			fhp->fbh_fddi.fddi_mac.mac_bits |= MAC_ERROR_BIT;
			if (desc & FSI_IND_RCV_DA_LSB)
				UPCNT(ri, error,1);
		}
	}
}


/* copy normal input to a string of mbufs
 */
static void
rns_copy_input(struct rns_info *ri,
	       int di,			/* index of first descriptor */
	       uint desc)		/* last descriptor */
{
	struct mbuf *m, *m1, *mt;
	int totlen, wraplen, tgtlen, curlen;
	FDDI_IBUFLLC *fhp;
	u_char *src, *tgt;


	if ((desc & (FSI_IND_RCV_ERR | FSI_IND_RCV_PEI)) != 0) {
		if (desc & FSI_IND_RCV_PEI) {
			ZAPBD(ri);
			cmn_err_tag(176,CE_ALERT,
				"rns%d: receive port error 0x%x\n",
				ri->ri_unit,
				desc & FSI_IND_RCV_PEI_PER_MASK);
		}
		rx_junk(ri, desc);
		return;
	}

	totlen = desc & FSI_IND_LEN_MASK;
	if (totlen < sizeof(struct smac_hdr)) {
		UPCNT(ri,shorterr,1);
		rx_junk(ri, desc);
		return;
	}

	/* Pick size of the first mbuf.
	 * If there will be more than one mbuf, then arrange to page flip.
	 * Ensure at least the MAC, IP, and UDP headers are in the first mbuf.
	 * Of course the TCP header is larger. Assume VCL_MAX is at least 4096.
	 */
	tgtlen = totlen+sizeof(FDDI_IBUF);
	if (tgtlen > VCL_MAX) {
		tgtlen = 1 + (tgtlen-1)%VCL_MAX;
		if (tgtlen < sizeof(FDDI_IBUFLLC)+28)
			tgtlen = sizeof(FDDI_IBUFLLC)+28;
	}
	m = m_vget(M_DONTWAIT, MAX(tgtlen,sizeof(FDDI_IBUFLLC)),
		   MT_DATA);
	if (!m) {
		UPCNT(ri,buf_ovf,1);
		return;
	}

	fhp = mtod(m,FDDI_IBUFLLC*);
	mt = m;
	tgt = (u_char *)fhp+sizeof(FDDI_IBUF);
	tgtlen -= sizeof(FDDI_IBUF);

	src = (u_char *)ri->ri_hdma->rx[di].b;
	wraplen = (RX_RING_LEN - di) * RX_BUF_LEN;
	for (;;) {
		curlen = MIN(wraplen, tgtlen);
		bcopy(src, tgt, curlen);

		totlen -= curlen;
		if (totlen <= 0)
			break;

		if ((wraplen -= curlen) != 0) {
			/* Just advance the source if we do not
			 * need to wrap to the start of the ring.
			 */
			src += curlen;
		} else {
			/* Wrap to the start of the ring at its end,
			 * and set to not wrap again.
			 */
			wraplen = RX_BUF_LEN*RX_RING_LEN;
			src = (u_char *)ri->ri_hdma->rx[0].b;
		}

		/* compute next place to which to copy data */
		if ((tgtlen -= curlen) != 0) {
			tgt += curlen;
		} else {
			tgtlen = MIN(totlen, VCL_MAX);
			m1 = m_vget(M_DONTWAIT, tgtlen, MT_DATA);
			if (!m1) {
				m_freem(m);
				UPCNT(ri,buf_ovf,1);
				return;
			}
			mt->m_next = m1;
			mt = m1;
			tgt = mtod(m1,u_char *);
		}
	}

	rns_set_mac_bits(ri,desc,fhp);
	rns_input(ri, m, fhp, desc & FSI_IND_LEN_MASK);
}


/* copy promiscuous mode MAC-frame input to an mbuf
 */
static void
rns_copy_pm(struct rns_info *ri,
	    int di,
	    uint desc)
{
	struct mbuf *m;
	int len, totlen;
	FDDI_IBUFLLC *fhp;


	if (desc & FSI_IND_RCV_ERR) {
		UPCNT(ri,tot_junk,1);
		return;
	}

	totlen = desc & FSI_IND_LEN_MASK;
	if (totlen < sizeof(struct smac_hdr)) {
		UPCNT(ri,shorterr,1);
		UPCNT(ri,tot_junk,1);
		return;
	}

	/* discard junk that the FSI likes to deliver */
	if (ri->ri_hdma->pm[di].b[0] != 0
	    || ri->ri_hdma->pm[di].b[1] != 0) {
		UPCNT(ri,tot_junk,1);
		return;
	}

	/* If this MAC frame is the same as the last one we saw, forget it.
	 */
	if (totlen == ri->ri_pm_len
	    && ri->ri_usec < ri->ri_pm_usec
	    && !bcmp((void *)ri->ri_hdma->pm[di].b, ri->ri_pm, totlen)) {
		UPCNT(ri,tot_junk,1);
		return;
	}
	/* Take a snapshot of this new MAC frame.
	 */
	ri->ri_pm_len = totlen;
	bcopy((void *)ri->ri_hdma->pm[di].b, ri->ri_pm, totlen);
	ri->ri_pm_usec = ri->ri_usec+USEC_PER_SEC/10;

	/* put it in an mbuf */
	len = totlen+sizeof(FDDI_IBUF);
	m = m_vget(M_DONTWAIT, MAX(len,FDDI_MIN_LLC+sizeof(FDDI_IBUFLLC)),
		   MT_DATA);
	if (!m) {
		UPCNT(ri,buf_ovf,1);
		return;
	}

	fhp = mtod(m,FDDI_IBUFLLC*);
	bcopy((void *)ri->ri_hdma->pm[di].b, (char *)fhp+sizeof(FDDI_IBUF),
	      len-sizeof(FDDI_IBUF));
	rns_set_mac_bits(ri,desc,fhp);
	rns_input(ri, m, fhp, totlen);
}


/* Finish ordinary input
 *	Interrupts must be off.
 */
static void
rns_fin_in(struct rns_info *ri)
{
	int worked;
	int di, di2;
	uint desc;


	/* If recursing, for example output->here->arp->output->here,
	 * then don't.
	 */
	if (ri->ri_in_active) {
		ri->ri_dbg.in_active++;
		return;
	}
	ri->ri_in_active = 1;

	/* Deal with PCM, line states, and so forth.
	 */
	while (ri->ri_pcm1.ls_in != ri->ri_pcm1.ls_out) {
		rns_new_rls(&ri->ri_smt1,ri,
			    ri->ri_pcm1.ls[ri->ri_pcm1.ls_out]);
		ADV_LS_INX(ri->ri_pcm1.ls_out);
	}
	while (ri->ri_pcm2.ls_in != ri->ri_pcm2.ls_out) {
		rns_new_rls(&ri->ri_smt2,ri,
			    ri->ri_pcm2.ls[ri->ri_pcm2.ls_out]);
		ADV_LS_INX(ri->ri_pcm2.ls_out);
	}

	/* There is no need for the currently pending input interrupts,
	 * because we are dealing with their input now, so forget them.
	 * We cannot forget the 'SMT timer' interrupt because
	 * it is also used to limit MAC interrupts.
	 */
	WT_REG(ri, fsi_sr1, SR1_RXC_ALL);

	/* Limit the interrupt rate by turning off input interrupts
	 * and turning on the "SMT timer" interrupts in the IFDDI chip.
	 */
	if ((ri->ri_imr1 & SR1_RXC_ALL) != 0 && !rns_low_latency) {
		ri->ri_imr1 = (ri->ri_imr1 & ~SR1_RXC_ALL) | SR1_STE;
		WT_REG(ri, fsi_imr1, ri->ri_imr1);
	}

	/* Deal with normal input descriptors
	 */
	worked = 0;
	for (di = ri->ri_rx_desc_nxt;
	     !((desc = ri->ri_hdma->rx_desc[di].hi) & FSI_IND_OWN);
	     di = NEXT_RX_DI(di)) {
		/* Scan the descriptors to the last one,
		 * looking for the "LAST" bit.
		 */
		di2 = di;
		while ((desc & (FSI_IND_LAST|FSI_IND_OWN)) != FSI_IND_LAST) {
			di = NEXT_RX_DI(di);
			desc = ri->ri_hdma->rx_desc[di].hi;

			/* bail out if not all of the packet is ready */
			if (desc & FSI_IND_OWN) {
				di = di2;
				goto unready;
			}
		}

		worked = 1;
		rns_copy_input(ri, di2, desc);

		/* Refresh the descriptors after copying the data from
		 * the buffers.
		 */
		for (;;) {
			ri->ri_hdma->rx_desc[di2].hi = RX_DESC_HI;
			if (di2 == di)
				break;
			di2 = NEXT_RX_DI(di2);
		}
	}
unready:
	if (worked) {
		/* we might have gone around the ring and so not changed
		 * ri->ri_rx_desc_nxt
		 */
		ri->ri_rx_desc_nxt = di;
		rns_flush(ri);
		worked = 0;
	}

	/* Deal with promiscuous mode MAC frame descriptors
	 */
	for (di = ri->ri_pm_desc_nxt;
	     !((desc = ri->ri_hdma->pm_desc[di].hi) & FSI_IND_OWN);
	     di = NEXT_PM_DI(di)) {
		worked = 1;
		rns_copy_pm(ri, di, desc);
		ri->ri_hdma->pm_desc[di].hi = PM_DESC_HI;
	}
	if (worked) {
		ri->ri_pm_desc_nxt = di;
		rns_flush(ri);
	}

	ri->ri_in_active = 0;
}


/* Finish ordinary output
 *	Interrupts must be off.
 */
static void
rns_fin_out(struct rns_info *ri)
{
	uint desc;
	int di;

	CK_IFNET(ri);			/* ri_tx_job* better be locked */

	/* If no output recently finished, bail out. */
	di = ri->ri_tx_desc_old;
	if (di == ri->ri_tx_desc_nxt
	    || ((desc = ri->ri_hdma->tx_desc[di].hi) & FSI_IND_OWN)) {
		rns_snd_out(ri);
		return;
	}

	do {
		if (0 != (desc & (FSI_IND_TX_PER_MASK | FSI_IND_TX_PE))) {
			DEBUGPRINT(ri,("rns%d: transmit error 0x%x\n",
				       ri->ri_unit,
				       desc & (FSI_IND_TX_PER_MASK
					       | FSI_IND_TX_PE)));
			ri->ri_if.if_oerrors++;
		} else if (desc & FSI_IND_TX_UN) {
			UPCNT(ri, tx_under,1);
			DEBUGPRINT(ri,("rns%d: TX underrun\n", ri->ri_unit));
			ri->ri_if.if_oerrors++;
		} else if (desc & FSI_IND_TX_AB) {
			UPCNT(ri, xmtabt,1);
		}

		ri->ri_tx_buf_free += ALIGN(desc & FSI_IND_LEN_MASK, TX_ALIGN);
		ri->ri_tx_desc_free++;

		di = NEXT_TX_DI(di);
	} while (di != ri->ri_tx_desc_nxt
		 && !((desc = ri->ri_hdma->tx_desc[di].hi) & FSI_IND_OWN));
	ri->ri_tx_desc_old = di;

	rns_snd_out(ri);
}


/* FDDI output.
 *	If the destination is this machine or broadcast, send the packet
 *	to the loop-back device.  We cannot talk to ourself.
 */
static int				/* 0 or error # */
#if defined(FDDI_FICUS) || defined(FDDI_BONSAI)
rns_output(struct ifnet *ifp,
	   struct mbuf *m0,
	   struct sockaddr *dst)
#else
rns_output(struct ifnet *ifp,
	   struct mbuf *m0,
	   struct sockaddr *dst,
	   struct rtentry *rte)
#endif
{
#define ri ((struct rns_info*)ifp)
	struct mbuf *mloop = 0;
	int totlen;
#ifdef FDDI_BONSAI
	struct rns_tx_job job;
#define JOB job.job_hdr
#else
	struct rns_tx_job *job;
#define JOB job->job_hdr
#endif
#if !defined(FDDI_FICUS) && !defined(FDDI_BONSAI)
	int error = 0;
#endif
#ifdef FDDI_BONSAI
	int s;
#endif

	ASSERT(ri->ri_if.if_name == rns_name);
	CK_IFNET(ri);

	/* If we are out of space, check for finished output.
	 * Handle output indications lazily, only when the timer fires
	 * or we really need the room.
	 */
#ifdef FDDI_BONSAI
	s = splimp();
#endif
	if (IF_QFULL(&ri->ri_if.if_snd)) {
		rns_fin_out(ri);
		if (IF_QFULL(&ri->ri_if.if_snd)) {
			m_freem(m0);
			ri->ri_if.if_odrops++;
			IF_DROP(&ri->ri_if.if_snd);
#ifdef FDDI_BONSAI
			splx(s);
#endif
			return ((ri->ri_smt1.smt_st.mac_state == MAC_ACTIVE)
				? ENOBUFS : ENETDOWN);
		}
	}
#ifdef FDDI_BONSAI
	splx(s);
#endif

#ifndef FDDI_BONSAI
	job = ri->ri_tx_job_head;
#endif
	totlen  = m_length(m0);
	if (totlen > ri->ri_if.if_mtu) {
		cmn_err_tag(177,CE_NOTE, "rns%d: attempt to send long frame (%d)\n",
			ri->ri_unit, totlen);
		m_freem(m0);
		return EMSGSIZE;
	}

	switch (dst->sa_family) {
	case AF_INET:
		JOB = ri->ri_hdr;
#if defined(FDDI_FICUS) || defined(FDDI_BONSAI)
		if (!ip_arpresolve(&ri->ri_ac, m0,
				   &((struct sockaddr_in *)dst)->sin_addr,
				   (u_char*)&JOB.fddi_mac.mac_da)) {
			return 0;	/* just wait if not yet resolved */
		}
#else
		if (!arpresolve(&ri->ri_ac, rte, m0,
				&((struct sockaddr_in *)dst)->sin_addr,
				(u_char*)&JOB.fddi_mac.mac_da,
				&error)) {
			return error;	/* just wait if not yet resolved */
		}
#endif
		bitswapcopy(&JOB.fddi_mac.mac_da,
			    &JOB.fddi_mac.mac_da,
			    sizeof(JOB.fddi_mac.mac_da));
		totlen += sizeof(JOB);

		/* Listen to ourself, if we are supposed to.
		 */
		if (FDDI_ISBROAD_SW(&JOB.fddi_mac.mac_da.b[0])) {
			mloop = m_copy(m0, 0, M_COPYALL);
			if (0 == mloop) {
				m_freem(m0);
				ri->ri_if.if_odrops++;
				IF_DROP(&ri->ri_if.if_snd);
				return ENOBUFS;
			}
		}
		break;


	case AF_UNSPEC:
#	    define EP ((struct ether_header *)&dst->sa_data[0])
		JOB = ri->ri_hdr;
		JOB.fddi_llc.llc_etype = EP->ether_type;
		bitswapcopy(&EP->ether_dhost[0],
			    &JOB.fddi_mac.mac_da,
			    sizeof(JOB.fddi_mac.mac_da));
		totlen += sizeof(JOB);
# undef EP
		break;

	case AF_RAW:
		/* The mbuf chain contains most of the frame, including the
		 *	MAC header but not the board header.
		 *	The board header is hidden in the start of the
		 *	supposed MAC header.
		 */
		if (totlen <= sizeof(JOB)) {
			m_freem(m0);
			return EAFNOSUPPORT;
		}
		(void)m_datacopy(m0,0, sizeof(JOB), (caddr_t)&JOB);
		m_adj(m0, sizeof(JOB));
		JOB.fddi_mac.filler[0] = (u_char)(MAC_DATA_HDR>>16);
		JOB.fddi_mac.filler[1] = (u_char)(MAC_DATA_HDR>>8);
		JOB.fddi_mac.mac_bits = (u_char)(MAC_DATA_HDR);
		break;

	case AF_SDL:
		/* send an 802 packet for DLPI
		 */
#		define SCKTP ((struct sockaddr_sdl *)dst)
#		define SCKTP_HLEN (sizeof(JOB) -sizeof(JOB.fddi_llc))

		/* choke if the MAC address is silly */
		if (SCKTP->ssdl_addr_len != sizeof(JOB.fddi_mac.mac_da)) {
			m_freem(m0);
			return EAFNOSUPPORT;
		}
		if (totlen <= sizeof(JOB)-SCKTP_HLEN) {
			m_freem(m0);
			return EAFNOSUPPORT;
		}
		/* The mbuf chain contains most of the frame, including the
		 *	LLC header but not the board or MAC headers.
		 */
		JOB = ri->ri_hdr;
		(void)m_datacopy(m0,0, sizeof(JOB.fddi_llc),
				 (caddr_t)&JOB.fddi_llc);
		m_adj(m0, sizeof(JOB.fddi_llc));
		totlen += SCKTP_HLEN;
		bitswapcopy(SCKTP->ssdl_addr, &JOB.fddi_mac.mac_da,
			    sizeof(JOB.fddi_mac.mac_da));
# undef SCKTP
# undef SCKTP_HLEN
		break;

	default:
		cmn_err_tag(178,CE_ALERT, "rns%d: cannot handle address family %u\n",
			    ri->ri_unit, dst->sa_family);
		m_freem(m0);
		return EAFNOSUPPORT;
	}

	/* Save the message, unless the board is dead.
	 */
	if (iff_dead(ri->ri_if.if_flags)
	    || (ri->ri_state & RI_ST_ZAPPED)) {
		ri->ri_if.if_oerrors++;
		IF_DROP(&ri->ri_if.if_snd);
		m_freem(m0);
		m_freem(mloop);
		return EHOSTDOWN;
	}

	/* Check whether snoopers want to copy this packet.
	 */
	if (RAWIF_SNOOPING(&ri->ri_rawif)
	    && snoop_match(&ri->ri_rawif, (caddr_t)&JOB, sizeof(JOB))) {
		struct mbuf *ms, *mt, *m1;
		int len;		/* mbuf bytes to copy */
		int lenoff;
		int curlen;
		FDDI_IBUFLLC *p;

		lenoff = 0;
		len = totlen;
		curlen = len+sizeof(*p);
		if (curlen > VCL_MAX)
			curlen = VCL_MAX;
		ms = m_vget(M_DONTWAIT, curlen, MT_DATA);
		if (0 != ms) {
			p = mtod(ms,FDDI_IBUFLLC*);
			IF_INITHEADER(p, &ri->ri_if, sizeof(FDDI_IBUFLLC));
			p->fbh_fddi = JOB;
			len -= sizeof(JOB);
			curlen = m_datacopy(m0,0, curlen-sizeof(*p),
					    (caddr_t)(p+1));
			mt = ms;
			for (;;) {
				lenoff += curlen;
				len -= curlen;
				if (len <= 0)
					break;
				curlen = MIN(len,VCL_MAX);
				m1 = m_vget(M_DONTWAIT,curlen,MT_DATA);
				if (0 == m1) {
					m_freem(ms);
					ms = 0;
					break;
				}
				mt->m_next = m1;
				mt = m1;
				curlen = m_datacopy(m0, lenoff, curlen,
						    mtod(m1, caddr_t));
				ASSERT(curlen > 0);
			}
		}
		if (!ms) {
			snoop_drop(&ri->ri_rawif, SN_PROMISC,
				   mtod(m0,caddr_t), m0->m_len);
		} else {
			(void)snoop_input(&ri->ri_rawif, SN_PROMISC,
					  (caddr_t)&p->fbh_fddi, ms, lenoff);
		}
	}

#ifdef FDDI_BONSAI
	s = splimp();
	*ri->ri_tx_job_head = job;
	if (++ri->ri_tx_job_head >= &ri->ri_tx_jobs[TX_MAX_JOBS])
		ri->ri_tx_job_head = ri->ri_tx_jobs;
#else
	if (++job >= &ri->ri_tx_jobs[TX_MAX_JOBS])
		job = ri->ri_tx_jobs;
	ri->ri_tx_job_head = job;
#endif
	IF_ENQUEUE(&ri->ri_if.if_snd,m0);
	ri->ri_if.if_obytes += totlen-FDDI_FILLER;
	ri->ri_if.if_opackets++;

	/* finally send to ourself */
	if (mloop != 0) {
		ri->ri_if.if_omcasts++;
#if defined(FDDI_FICUS) || defined(FDDI_BONSAI)
		(void)looutput(&loif, mloop, dst);
#else
		(void)looutput(&loif, mloop, dst, rte);
#endif
	} else if (FDDI_ISGROUP_SW(JOB.fddi_mac.mac_da.b)) {
		ri->ri_if.if_omcasts++;
	}

	/* Tell the board about the new job if there is room in the
	 * transmit ring.
	 */
	rns_snd_out(ri);

	/* check for new input */
	rns_fin_in(ri);

#ifdef FDDI_BONSAI
	splx(s);
#endif
	return 0;
#undef ri
#undef JOB
}


/* Tell the board about output it is supposed to do.
 *	The output queue must be locked and interrupts off.
 */
static void
rns_snd_out(struct rns_info *ri)
{
	uint desc_free, buf_free, len, di;
	struct rns_tx_job *job;
	fsi_desc_t *dp;
	union tx_buf *tx_p;
	struct mbuf *m;


	CK_IFNET(ri);			/* ri_tx_job* better be locked */

	/* Stop TX interrupts and quit if no more output waiting to be sent
	 * to the board.
	 */
	if ((job = ri->ri_tx_job_tail) == ri->ri_tx_job_head) {
		if ((ri->ri_imr1 & SR1_OUTPUT) != 0) {
			ri->ri_imr1 &= ~SR1_OUTPUT;
			WT_REG(ri, fsi_imr1, ri->ri_imr1);
		}
		return;
	}

	/* do nothing if not enough room */
	if ((desc_free = ri->ri_tx_desc_free) == 0
	    || (buf_free = ri->ri_tx_buf_free) < FDDI_MAXOUT) {
		return;
	}

	di = ri->ri_tx_desc_nxt;
	tx_p = ri->ri_tx_hbuf_nxt;
	for (;;) {
		IF_DEQUEUE(&ri->ri_if.if_snd, m);
		if (m == 0) {
			printf("rns%d: output queue ifq_len=%d"
			       " job_tail=0x%x head=0x%x\n",
			       ri->ri_unit, ri->ri_if.if_snd.ifq_len,
			       ri->ri_tx_job_tail, ri->ri_tx_job_head);
			ZAPBD(ri);
			return;
		}
		tx_p->hdr = job->job_hdr;
		len = (sizeof(tx_p->hdr)
		       + m_datacopy(m, 0, TX_BUF_MAX-sizeof(job->job_hdr),
				    (caddr_t)(&tx_p->hdr+1)));
		m_freem(m);

		/* install the TX descriptor */
		dp = &ri->ri_hdma->tx_desc[di];
		ASSERT(!(dp->hi & FSI_IND_OWN));
		dp->a = FSI_DESC_A(ri->ri_tx_bbuf_nxt);
		dp->hi = FSI_DESC_HI(FSI_CMD_TX1, len);

		len = ALIGN(len, TX_ALIGN);
		buf_free -= len;
		tx_p = (union tx_buf *)&tx_p->b[len];
		if (tx_p <= (union tx_buf *)&ri->ri_hdma->tx.b[TX_LEN
							-TX_BUF_MAX]) {
			ri->ri_tx_bbuf_nxt += len;
		} else {
			tx_p = &ri->ri_hdma->tx;
			ri->ri_tx_bbuf_nxt = ri->ri_bdma->tx.b;
		}

		desc_free--;
		di = NEXT_TX_DI(di);
		if (++job >= &ri->ri_tx_jobs[TX_MAX_JOBS])
			job = ri->ri_tx_jobs;
		if (job == ri->ri_tx_job_head)
			break;

		if (desc_free == 0 || buf_free < FDDI_MAXOUT) {
			/* turn on interrupts if we run out of board space */
			if (!(ri->ri_imr1 & SR1_OUTPUT)) {
				ri->ri_imr1 |= (SR1_STE | SR1_OUTPUT);
				WT_REG(ri, fsi_imr1, ri->ri_imr1);
			}
			break;
		}
	}
	ri->ri_tx_hbuf_nxt = tx_p;
	ri->ri_tx_desc_free = desc_free;
	ri->ri_tx_buf_free = buf_free;
	ri->ri_tx_job_tail = job;
	ri->ri_tx_desc_nxt = di;

	/* ensure timer is running to handle output indications. */
	if (!(ri->ri_imr1 & SR1_STE)) {
		ri->ri_imr1 |= SR1_STE;
		WT_REG(ri, fsi_imr1, ri->ri_imr1);
	}

	/* tell the FSI about the new packet(s) */
	rns_flush(ri);
	fcr_sto(ri, FSI_RDY(TX_RING_N));
}


/* work on an ELM either when it interrupts the host or when needed
 */
static void
rns_elm_int(struct rns_info *ri,
	    struct smt *smt)
{
	int event;
	struct ri_pcm *pp = SMT1(smt,ri) ? &ri->ri_pcm1 : &ri->ri_pcm2;

	static struct {
		uint	    match:16;
		enum pcm_ls prev:8;
		enum pcm_ls cur:8;
	} *p, rcv_ls_tbl[] = {
#   define MK_TBL(p,c) {					    \
		ELM_CB_MATCH_LS_MASK & ~ELM_CB_MATCH_ ## c ##LS,    \
		PCM_ ## p ## LS,				    \
		PCM_ ## c ## LS					    \
		}

		MK_TBL(Q,Q),		/* 0=QLS, 0=NLS */
		MK_TBL(Q,Q),		/* 0=QLS, 1=ALS */
		MK_TBL(Q,Q),		/* 0=QLS, 2=??? */
		MK_TBL(Q,Q),		/* 0=QLS, 3=ILS4 */
		MK_TBL(Q,Q),		/* 0=QLS, 4=QLS */
		MK_TBL(Q,M),		/* 0=QLS, 5=MLS */
		MK_TBL(Q,H),		/* 0=QLS, 6=HLS */
		MK_TBL(Q,I),		/* 0=QLS, 7=ILS16 */

		MK_TBL(M,M),		/* 1=MLS, 0=NLS */
		MK_TBL(M,M),		/* 1=MLS, 1=ALS */
		MK_TBL(M,M),		/* 1=MLS, 2=??? */
		MK_TBL(M,M),		/* 1=MLS, 3=ILS4 */
		MK_TBL(M,Q),		/* 1=MLS, 4=QLS */
		MK_TBL(M,M),		/* 1=MLS, 5=MLS */
		MK_TBL(M,H),		/* 1=MLS, 6=HLS */
		MK_TBL(M,I),		/* 1=MLS, 7=ILS16 */

		MK_TBL(H,H),		/* 2=HLS, 0=NLS */
		MK_TBL(H,H),		/* 2=HLS, 1=ALS */
		MK_TBL(H,H),		/* 2=HLS, 2=??? */
		MK_TBL(H,H),		/* 2=HLS, 3=ILS4 */
		MK_TBL(H,Q),		/* 2=HLS, 4=QLS */
		MK_TBL(H,M),		/* 2=HLS, 5=MLS */
		MK_TBL(H,H),		/* 2=HLS, 6=HLS */
		MK_TBL(H,I),		/* 2=HLS, 7=ILS16 */

		MK_TBL(I,I),		/* 3=ILS, 0=NLS */
		MK_TBL(I,I),		/* 3=ILS, 1=ALS */
		MK_TBL(I,I),		/* 3=ILS, 2=??? */
		MK_TBL(I,I),		/* 3=ILS, 3=ILS4 */
		MK_TBL(I,Q),		/* 3=ILS, 4=QLS */
		MK_TBL(I,M),		/* 3=ILS, 5=MLS */
		MK_TBL(I,H),		/* 3=ILS, 6=HLS */
		MK_TBL(I,I),		/* 3=ILS, 7=ILS16 */
	};


	event = elm_fet(ri,smt, ELM_EVENT);

	/* pay attention to noise only when things are turned on */
	if (pp->elm_sw & ELM_NOISE) {
#ifndef FDDI_BONSAI
		if (event & ELM_EV_VSYM_CTR)
			UPCNT(ri,viol,256);
#endif
		if (event & ELM_EV_EBUF)
			UPCNT_PCM(ri,eovf,1,1);
		if (event & ELM_EV_TNE)
			NEW_LS(pp,PCM_NLS);
	}

	p = &rcv_ls_tbl[(elm_fet(ri,smt,ELM_STATUS_A)
			 & (ELM_PREV_LS_MASK | ELM_LS_MASK))
			>> ELM_LS_LSB];
	if ((event & ELM_EV_LS) && p->prev != pp->ls_prev)
		NEW_LS(pp,p->prev);
	if (p->cur != pp->ls_prev)
		NEW_LS(pp,p->cur);

	/* Even if the line state does not seem to have changed,
	 * set the match field to cause an interrupt when it does change.
	 *
	 * The Motorola documentation on the ELM says that setting the
	 * match field to 0 should be equivalent.  But it does not seem
	 * to work.
	 */
	elm_sto(ri,smt, ELM_CTRL_B,
		pp->elm_ctrl_b = ((pp->elm_ctrl_b & ~ELM_CB_MATCH_LS_MASK)
				  | p->match));

	if (event & ELM_EV_PCM_ST)
		PUSH_LS(pp,RCV_SELF_TEST);
	if (event & ELM_EV_PCM_TRACE)
		PUSH_LS(pp,RCV_TRACE_PROP);
	if ((event & ELM_EV_PCM_BREAK)
	    && ((elm_fet(ri,smt, ELM_STATUS_B) & ELM_SB_BREAK_MASK)
		> ELM_SB_BREAK_PC_START))
		PUSH_LS(pp,RCV_BREAK);
	if (event & ELM_EV_PCM_CODE)
		PUSH_LS(pp,RCV_PCM_CODE);
}


/* work on the MAC when it asks or when necessary
 */
static int
rns_mac_int(struct rns_info *ri)
{
	int event, tx_status, j;
	__uint64_t c;

	event = mac_fet(ri, MAC_EVNT_C);

	/* count tokens */
	UPCNT(ri,tok_ct, mac_fet(ri, MAC_TOKEN_CT));
	if (event & MAC_EVC_TKN_CNT_OVF)
		UPCNT(ri,tok_ct, 1<<16);

	/* Deal with the Digital Equipment Corp 'purger'.  Get its value
	 * if good and turn it off whether good or ill.
	 */
	if (0 != (event & (MAC_EVC_VOID_OVF | MAC_EVC_VOID_RDY))) {
		if (event & MAC_EVC_VOID_RDY)
		    ri->ri_smt1.smt_st.ring_latency=mac_fet(ri,MAC_VOID_TIME);
		mac_sto(ri,MAC_CTRL_B, (mac_fet(ri, MAC_CTRL_B)
					& ~MAC_CB_RING_PURGE));
	}

	event = mac_fet(ri, MAC_EVNT_A);

	/* count frames */
	UPCNT(ri,frame_ct, mac_fet(ri, MAC_FRAME_CT));
	if (event & MAC_EVA_FRAME_RCVD)
		UPCNT(ri,frame_ct, 1<<16);

	j = mac_fet(ri, MAC_LOST_ERR);
	UPCNT(ri,lost_ct, (j>>8)&0xff);
	UPCNT(ri,err_ct, j&0xff);

	if (event & MAC_EVA_DBL_OVFL)
		DEBUGPRINT(ri,("rns%d: MAC count overflow\n", ri->ri_unit));

	/* Notice when RNGOP changes.  If our records imply it has not
	 * changed, then count an additional recovery and failure.
	 */
	tx_status = mac_fet(ri, MAC_TX_STATUS);
	if (event & MAC_EVA_RNGOP_CHG) {
		if (tx_status & MAC_TX_ST_RNGOP) {
			c = *(__uint64_t*)&ri->ri_smt1.smt_st.rngbroke;
			*(__uint64_t*)&ri->ri_smt1.smt_st.rngop = c+1;
			if (!PCM_SET(&ri->ri_smt1, PCM_RNGOP)) {
				ri->ri_smt1.smt_st.flags |= PCM_RNGOP;
				if (IS_DAS(ri)
				    && !PCM_SET(&ri->ri_smt1,PCM_THRU_FLAG))
				    ri->ri_brd_ctl |= BRD_LED0;
				else
				    ri->ri_brd_ctl &= ~BRD_LED0;
				ri->ri_brd_ctl |= BRD_LED1;
				WT_REG(ri, brd_ctl, ri->ri_brd_ctl);
				rns_alarm(&ri->ri_smt1,ri,SMT_ALARM_RNGOP);
			}

			if (rns_lat_void
			    && ri->ri_lat_void < ri->ri_usec-USEC_PER_SEC/4) {
				ri->ri_lat_void = ri->ri_usec;
				mac_sto(ri,MAC_CTRL_B, (mac_fet(ri, MAC_CTRL_B)
							| MAC_CB_RING_PURGE));
			}

		} else {
			c = *(__uint64_t*)&ri->ri_smt1.smt_st.rngop;
			*(__uint64_t*)&ri->ri_smt1.smt_st.rngbroke = c+1;
			if (PCM_SET(&ri->ri_smt1, PCM_RNGOP)) {
				ri->ri_smt1.smt_st.flags &= ~PCM_RNGOP;
				ri->ri_brd_ctl &= ~BRD_LED1;
				ri->ri_brd_ctl |= BRD_LED0;
				WT_REG(ri, brd_ctl, ri->ri_brd_ctl);
				rns_alarm(&ri->ri_smt1,ri,SMT_ALARM_RNGOP);
			}
		}
	}


	/* Generate counts of entering claim and beacon states.  These counts
	 * are not accurate, because claim or beacon states do not generate
	 * any interrupts.  If a MAC interrupt happens to occur while the
	 * MAC is (still) in one of those states, we can count it.
	 *
	 * This is unfortunate because it prevents detecting some kinds of
	 * non-operational rings caused by duplicate MAC addresses.
	 * We cannot tell if a stray beacon or claim frame was transmitted
	 * during an undetected beacon or claim state.
	 */
	switch (tx_status & MAC_TX_ST_FSM_MASK) {
	case MAC_TX_ST_BEACON:
		if (ri->ri_smt1.smt_st.mac_state == MAC_BEACONING) {
			ri->ri_smt1.smt_st.mac_state = MAC_BEACONING;
			UPCNT(ri,bec, 1);
		}
		break;
	case MAC_TX_ST_CLAIM:
		if (ri->ri_smt1.smt_st.mac_state == MAC_CLAIMING) {
			ri->ri_smt1.smt_st.mac_state = MAC_CLAIMING;
			UPCNT(ri,clm, 1);
		}
		break;
	default:
		ri->ri_smt1.smt_st.mac_state = MAC_ACTIVE;
		break;
	}

	if (event & MAC_EVA_TVX_EXPIR)
		UPCNT(ri,tvxexp, 1);
	if (event & MAC_EVA_LATE_TKN)
		UPCNT(ri,trtexp, 1);
	if (event & MAC_EVA_RCVRY_FAIL)
		UPCNT(ri,trtexp, 1);
	if (event & MAC_EVA_DUPL_TKN)
		UPCNT(ri,tkerr, 1);
	if (event & MAC_EVA_DUPL_ADDR)
		UPCNT(ri,pos_dup, 1);

	event = mac_fet(ri, MAC_EVNT_B);
	/* Turn off the extremely frequent interrupts so we do not drown
	 * in them.  Instead, use the periodic "SMT timer" interrupt.
	 */
	if (0 != (event & MAC_MASK_B_MORE)) {
		mac_sto(ri,MAC_MASK_B,
			ri->ri_mac_mask_b &= ~MAC_MASK_B_MORE);
		WT_REG(ri, fsi_imr1, (ri->ri_imr1 |= SR1_STE));
	}

	if (event & MAC_EVB_MY_BEACON)
		UPCNT(ri,mybec, 1);
	if (event & MAC_EVB_OTR_BEACON)
		UPCNT(ri,otrbec, 1);
	if (event & MAC_EVB_HI_CLAIM)
		UPCNT(ri,hiclm, 1);
	if (event & MAC_EVB_LO_CLAIM)
		UPCNT(ri,loclm, 1);
	if (event & MAC_EVB_MY_CLAIM)
		UPCNT(ri,myclm, 1);
	if (event & MAC_EVB_BAD_BID)
		UPCNT(ri,dup_mac, 1);
	if (event & MAC_EVB_PURGE_ERR) {
		UPCNT(ri,tkerr, 1);
		UPCNT(ri,pos_dup, 1);
	}
	if (event & MAC_EVB_BS_ERR)
		UPCNT(ri,tkerr, 1);
	if (event & MAC_EVB_WON_CLAIM)
		UPCNT(ri,tkiss, 1);
	if (event & MAC_EVB_NOT_COPIED)
		UPCNT(ri,misfrm, 1);

	if (0 != (event & MAC_MASK_B_BUG)) {
		ZAPBD(ri);
		cmn_err_tag(179,CE_ALERT,"rns%d: MAC programming error\n",ri->ri_unit);
                return 1;
        }
      
        return 0;
}


/* handle interrupts from the board
 */
#ifdef NOT_LEGO
/* ARGSUSED */
static void
if_rnsintr(eframe_t *ef, void *p)
#else
static void
if_rnsintr(void *p)
#endif
{
#define ri ((struct rns_info*)p)
	uint ci, sr1;
	int loop;
#ifdef FDDI_BONSAI
	int s;
#endif

	ASSERT(ri->ri_if.if_name == rns_name);

#if defined(FDDI_FICUS) || defined(FDDI_BONSAI)
	IFNET_LOCK(&ri->ri_if, s);
#else
	IFNET_LOCK(&ri->ri_if);
#endif
	ri->ri_dbg.ints++;

	do {
		if (ri->ri_state & RI_ST_ZAPPED)
			break;

		loop = 0;

		if (!iff_alive(ri->ri_if.if_flags)) {
#ifdef NOT_LEGO
			/* The moosehead PCI adapater interrupts everything
			 * indiscriminately
			 */
			if (!(ri->ri_state & RI_ST_ZAPPED))
				ZAPBD(ri);
#else
			ZAPBD(ri);
			cmn_err_tag(180,CE_ALERT, "rns%d: stray interrupt\n",
				ri->ri_unit);
#endif
			break;
		}

		sr1 = RD_REG(ri, fsi_sr1);
		if ((sr1 & SR1_ERRORS) != 0) {
                        ci = fcr_fet(ri, FSI_IER);
                        /*
                         * According to the Motorola documentation,
                         * do nothing if it is a MAC error.
                         */
                        if (ci == FSI_IER_MER) {
                                cmn_err(CE_WARN,
                                        "rns%d: MAC/FSI desynchronisation\n",
                                        ri->ri_unit);
                        } else {
				ZAPBD(ri);
				cmn_err_tag(181,CE_ALERT,
				    	    "rns%d: bad IFDDI status 0x%x\n",
				    	    ri->ri_unit, (u_long)sr1);
				break;
			}
		}

		if (IS_DAS(ri) && (RD_REG(ri, brd_int) & BRD_INT_ELM)) {
			loop = 1;
			rns_elm_int(ri, &ri->ri_smt2);
		}

		/* Acknowledge the IFDDI interrupt and turn off PCI interrupt.
		 */
		if ((sr1 & ~(SR1_CDN | SR1_CRF | SR1_CIN)) != 0) {
			loop = 1;
			WT_REG(ri, fsi_sr1, sr1);
		}

		if ((sr1 & IFDDI_IMR1) != 0) {
			loop = 1;
			if (sr1 & SR1_ROV(RX_RING_N)) {
				UPCNT(ri,rx_ovf,1);
				fcr_sto(ri, FSI_RDY(RX_RING_N));
			}
		}

		if (sr1 & SR1_STE) {
			loop = 1;
			/* turn off the timer interrupt */
			ri->ri_imr1 &= ~SR1_STE;

			/* Restore extra MAC interrupts several times a second,
			 * if the minimum MAC interrupts are already on.
			 * So if the timer interrupt happened, turn on the
			 * extra MAC interrupts.  The MAC interrupt called
			 * after here may turn them off.
			 */
			if (0 != ri->ri_mac_mask_b)
				mac_sto(ri,MAC_MASK_B,
					ri->ri_mac_mask_b |= MAC_MASK_B_MORE);
		}

		/* deal with output indications */
		if (sr1 & (SR1_RNR(TX_RING_N) | SR1_RCC(TX_RING_N))) {
			loop = 1;
			rns_fin_out(ri);
#if !defined(FDDI_FICUS) && !defined(FDDI_BONSAI)

			/* RSVP. Update packet scheduler with our q status */
			if (ri->ri_state & RI_ST_PSENABLED)
				ps_txq_stat(&(ri->ri_if), ri->ri_tx_desc_free);
#endif
		}

		if (sr1 & SR1_CIN) {
			loop = 1;
			ci = fcr_fet(ri, FCR_CAMEL(CAMEL_INT_LOC, 0));
			if ((ci & CAMEL_CAMEL_INT)
			    && (fcr_fet(ri, FCR_CAMEL(CAMEL_INTR,0))
				& CAMEL_NP_ERR)) {
				ZAPBD(ri);
				cmn_err_tag(182,CE_ALERT,
					    "rns%d: CAMEL NP error\n",
					    ri->ri_unit);
				break;
			}
			if (ci & CAMEL_ELM_INT)
				rns_elm_int(ri, &ri->ri_smt1);

			if (ci & CAMEL_MAC_INT) {
				if (rns_mac_int(ri))
					break;
			}	
		}

		/* finish setting the timer and input interrupts.
		 * Turn on input interrupts if the timer is off
		 */
		if (!(ri->ri_imr1 & SR1_STE))
			ri->ri_imr1 |= SR1_RXC_ALL;
		WT_REG(ri, fsi_imr1, ri->ri_imr1);

		/* Deal with new input */
		rns_fin_in(ri);

		/* Loop until no interrupts are found,
		 * since something is edge triggered.
		 */
	} while (loop);

#if defined(FDDI_FICUS) || defined(FDDI_BONSAI)
	IFNET_UNLOCK(&ri->ri_if, s);
#else
	IFNET_UNLOCK(&ri->ri_if);
#endif
#undef ri
}


#ifdef FDDI_BONSAI
/* ARGSUSED */
static int
if_rnsdetach(vertex_hdl_t vertex)
{
	return 0;
}


/* ARGSUSED */
static int
if_rnserror(vertex_hdl_t v, int e, ioerror_mode_t m, ioerror_t *ioe)
{
	return 0;
}


/* Tell the system we can handle RNS FDDI boards.
 */
int
if_rnsedtinit(struct edt *e)
{
	(void)pciio_add_attach(if_rnsattach, if_rnsdetach, if_rnserror,
			       "if_rns", -1);
	return pciio_driver_register(0x1112, 0x2200, "if_rns", 0);
}
#else
/* Tell the system we can handle RNS FDDI boards.
 */
void
if_rnsinit(void)
{
#if defined(RNS_DEBUG) || defined(ATTACH_DEBUG)
	cmn_err(CE_CONT, "if_rnsinit()\n");
#endif
	pciio_driver_register(0x1112, 0x2200,   /* vendor and device ID */
			      "if_rns", 0);
}
#endif


/* Capture the unit number that has been assigned to the vertex by ioconfig
 * and if_attach() the device.
 */
#ifndef FDDI_BONSAI
/* ARGSUSED */
int
if_rnsopen(dev_t *devp, int flag, int otyp, struct cred *crp)
#else
static void
if_rnsopen(struct rns_info *ri)
#endif
{
	static SMT_MANUF_DATA mnfdat = SGI_FDDI_MNFDATA(rnsx);
#	define MANUF_INDEX (sizeof(SGI_FDDI_MNFDATA_STR(rnsx))-2)
#	define MDATA smt_conf.mnfdat.manf_data
#ifndef FDDI_BONSAI
	struct ps_parms ps_params;
	vertex_hdl_t vhdl;
	struct rns_info *ri;
	int unit;

	vhdl = dev_to_vhdl(*devp);
	if (!(ri = (struct rns_info *)hwgraph_fastinfo_get(vhdl)))
		return EIO;

	if (vhdl != ri->ri_vhdl) {
		cmn_err_tag(183,CE_ALERT, "rns %v: wrong vhdl\n");
		return EIO;
	}

	/* if already if_attached, just return */
	if (ri->ri_unit != -1)
		return 0;

	unit = device_controller_num_get(vhdl);
	if (unit < 0 || unit >= RNS_MAXBD
	    || rns_infop[unit] != 0) {
		cmn_err_tag(184,CE_ALERT, "rns %v: bad unit number %d\n",
			    vhdl, unit);
		return EIO;
	}
	ri->ri_unit = unit;
#endif
	rns_infop[ri->ri_unit] = ri;

	bcopy(&mnfdat, &ri->ri_smt1.smt_conf.mnfdat,
	      sizeof(mnfdat));
	if (ri->ri_unit >= 10) {
		ri->ri_smt1.MDATA[MANUF_INDEX] = (ri->ri_unit/10)+'0';
		ri->ri_smt1.MDATA[MANUF_INDEX+1] = (ri->ri_unit%10)+'0';
	} else {
		ri->ri_smt1.MDATA[MANUF_INDEX] = ri->ri_unit+'0';
	}

	if_attach(&ri->ri_if);		/* create ('attach') the interface */

	/* Allocate a multicast filter table with an initial size of 10.
	 */
	if (!mfnew(&ri->ri_filter, 10))
		cmn_err_tag(185,CE_PANIC, 
			    "rns%d: no memory for frame filter\n",
			    ri->ri_unit);

	/* Initialize the raw ethernet socket interface.
	 *	Tell the snoop stuff to watch for our address in FDDI bit
	 *	order.
	 */
	rawif_attach(&ri->ri_rawif, &ri->ri_if,
		     (caddr_t)&ri->ri_smt1.smt_st.addr,
		     (caddr_t)&etherbroadcastaddr[0],
		     sizeof(ri->ri_smt1.smt_st.addr),
		     sizeof(struct fddi),
		     structoff(fddi, fddi_mac.mac_sa),
		     structoff(fddi, fddi_mac.mac_da));

#ifndef FDDI_BONSAI
#ifndef FDDI_FICUS
	/* create entry in /hw/net */
	if (vhdl != GRAPH_VERTEX_NONE) {
		char edge_name[8];
		(void)sprintf(edge_name, "%s%d", EDGE_LBL_RNS, ri->ri_unit);
		(void)if_hwgraph_alias_add(vhdl, edge_name);
	}

	/* RSVP.  Support admission control and packet scheduling */
	ps_params.bandwidth = 100000000;
	ps_params.txfree = TX_RING_LEN - 1;
	ps_params.txfree_func = rns_txfree_len;
	ps_params.state_func = rns_setstate;
	ps_params.flags = 0;
#else
	/* Only support admission control for now. */
	ps_params.bandwidth = 100000000;
	ps_params.txfree = 0;
	ps_params.txfree_func = NULL;
	ps_params.state_func = NULL;
	ps_params.flags = PS_DISABLED;
#endif
	(void)ps_init(&ri->ri_if, &ps_params);
#endif

#ifdef FDDI_BONSAI
	if (SHOWCONFIG)
		cmn_err(CE_CONT, "rns%d: MAC address %s,"
			" model %s, rev %d, cust %d, hw part 0x%x\n",
			ri->ri_unit,
			ether_sprintf(ri->ri_nvram.nvram_mac),
			ri->ri_product->p_product,
			(u_long)ri->ri_nvram.nvram_rev,
			(u_long)ri->ri_nvram.nvram_cust_id,
			(u_long)ri->ri_nvram.nvram_hw_part_no);
#else
	if (SHOWCONFIG)
		cmn_err(CE_CONT, "%v (rns%d): MAC address %s,"
			" model %s, rev %d, cust %d, hw part 0x%x\n",
			ri->ri_vhdl, ri->ri_unit,
			ether_sprintf(ri->ri_nvram.nvram_mac),
			ri->ri_product->p_product,
			(u_long)ri->ri_nvram.nvram_rev,
			(u_long)ri->ri_nvram.nvram_cust_id,
			(u_long)ri->ri_nvram.nvram_hw_part_no);

	return 0;
#endif
}


/* fetch nibbles from the NVRAM
 */
static __uint32_t
rns_get_nvram(struct rns_info *ri,
	      int *offp,
	      int bits)
{
	int off = *offp;
	__uint32_t res = 0;

	do {
		WT_REG(ri, brd_ctl, ((ri->ri_brd_ctl & ~BRD_SEL_NVRAM_MASK)
				     | ((off/BRD_NVRAM_LEN)*BRD_SEL_NVRAM)));
		res = ((res << 4)
		       | (RD_REG(ri, brd_nvram[off % BRD_NVRAM_LEN]) & 0xf));
		off++;
		bits -= 4;
	} while (bits != 0);

	*offp = off;
	return res;
}

/* This is called when the rest of the kernel has decided there is
 * a board we know about in a PCI slot somewhere.
 */
int
if_rnsattach(vertex_hdl_t conn_vhdl)
{
	int i;
	struct rns_info *ri;
	int nvram_off;
	device_desc_t dev_desc;
	pciio_intr_t intr_handle;
	product_tbl_t *pt;
	paddr_t pa;
	__psunsigned_t p;
#ifdef FDDI_BONSAI
	pciio_info_t info_p;
	static int unit = -1;
	inventory_t *inv_ptr;
	int s = splimp();
#else
	pciio_dmamap_t dma_map;
	vertex_hdl_t vhdl;
	graph_error_t rc;
#endif

#if defined(RNS_DEBUG) || defined(ATTACH_DEBUG)
	cmn_err(CE_CONT, "%v: if_rnsattach()\n", conn_vhdl);
#endif

#ifdef FDDI_BONSAI
	if (++unit >= RNS_MAXBD) {
		splx(s);
		cmn_err_tag(186,CE_ALERT, "rns%d: too many devices\n", unit);
		return ENOMEM;
	}
	info_p = pciio_info_get(conn_vhdl);
#else
	/* add the driver vertex to the graph */
	rc = hwgraph_char_device_add(conn_vhdl, EDGE_LBL_RNS,
				     "if_rns", &vhdl);
	if (rc != GRAPH_SUCCESS)
		cmn_err_tag(187,CE_ALERT, "rns %v: hwgraph_char_device_add"
			" error %d\n", conn_vhdl, rc);
#endif

	ri = (struct rns_info *)kmem_zalloc(sizeof(*ri), KM_SLEEP);
	if (!ri) {
#ifdef FDDI_BONSAI
		splx(s);
#endif
		cmn_err_tag(188,CE_ALERT, "rns %v: no memory\n", conn_vhdl);
		return ENOMEM;
	}
#ifdef FDDI_BONSAI
	ri->ri_vhdl = conn_vhdl;
	ri->ri_conn_vhdl = conn_vhdl;
	ri->ri_unit = unit;
#else
	ri->ri_vhdl = vhdl;
	ri->ri_conn_vhdl = conn_vhdl;
	ri->ri_unit = -1;
#endif

#ifdef NOT_LEGO
	p = (__psunsigned_t)kvpalloc(btoc(sizeof(*ri->ri_hdma))+RNS_DESC_ALIGN,
				     VM_UNCACHED | VM_PHYSCONTIG, 0);
#else
	p = (__psunsigned_t)kmem_alloc(sizeof(*ri->ri_hdma)+RNS_DESC_ALIGN,
				       KM_SLEEP | KM_PHYSCONTIG);
#endif
	if (!p) {
#ifdef FDDI_BONSAI
		splx(s);
#endif
		cmn_err_tag(189,CE_ALERT,
			    "rns %v: no memory for driver\n", conn_vhdl);
		kmem_free(ri, sizeof(*ri));
		return ENOMEM;
	}
	ri->ri_hdma = (struct rns_dma *)ALIGN(p, RNS_DESC_ALIGN);
#ifdef FDDI_BONSAI
	p = kvtophys(ri->ri_hdma) | PCI_NATIVE_VIEW;
#else
	dma_map = pciio_dmamap_alloc(conn_vhdl, 0, sizeof(*ri->ri_bdma),
				     PCIIO_DMA_DATA);
	pa = kvtophys((caddr_t)ri->ri_hdma);
	p = pciio_dmamap_addr(dma_map, pa, sizeof(*ri->ri_bdma));
#endif
#if _MIPS_SZPTR == 64
	if ((p >> 32) != 0)
		cmn_err_tag(190,CE_PANIC,"rns %v: mapped PCI address 0x%x > 32 bits\n",
			conn_vhdl, p);
#endif
	ri->ri_bdma = (struct rns_dma *)p;

	ri->ri_if.if_snd.ifq_maxlen = TX_MAX_QLEN;

	ri->ri_smt1.smt_pdata = (caddr_t)ri;
	ri->ri_smt1.smt_ops = &rns_ops;
	ri->ri_smt2.smt_pdata = (caddr_t)ri;
	ri->ri_smt2.smt_ops = &rns_ops;

	ri->ri_smt1.smt_conf.tvx = DEF_TVX;
	ri->ri_smt2.smt_conf.tvx = DEF_TVX;
	ri->ri_smt1.smt_conf.t_max = DEF_T_MAX;
	ri->ri_smt2.smt_conf.t_max = DEF_T_MAX;
	ri->ri_smt1.smt_conf.t_req = DEF_T_REQ;
	ri->ri_smt2.smt_conf.t_req = DEF_T_REQ;
	ri->ri_smt1.smt_conf.t_min = FDDI_MIN_T_MIN;
	ri->ri_smt2.smt_conf.t_min = FDDI_MIN_T_MIN;

	ri->ri_smt1.smt_conf.ip_pri = 1;
	ri->ri_smt2.smt_conf.ip_pri = 1;

	ri->ri_smt1.smt_conf.ler_cut = SMT_LER_CUT_DEF;
	ri->ri_smt2.smt_conf.ler_cut = SMT_LER_CUT_DEF;

	ri->ri_smt1.smt_conf.debug = smt_debug_lvl;
	ri->ri_smt2.smt_conf.debug = smt_debug_lvl;

	ri->ri_smt1.smt_conf.pcm_tgt = PCM_CMT;
	ri->ri_smt2.smt_conf.pcm_tgt = PCM_CMT;

	ri->ri_if.if_name = rns_name;
#if !defined(FDDI_FICUS) && !defined(FDDI_BONSAI)
	ri->ri_if.if_baudrate.ifs_value = 1000*1000*100;
#endif
	ri->ri_if.if_type = IFT_FDDI;
#if defined(FDDI_FICUS) || defined(FDDI_BONSAI)
	ri->ri_if.if_flags = IFF_BROADCAST|IFF_MULTICAST|IFF_NOTRAILERS;
#else
	ri->ri_if.if_flags = IFF_BROADCAST|IFF_MULTICAST|IFF_IPALIAS;
#endif
	ri->ri_if.if_output = rns_output;
	ri->ri_if.if_ioctl = rns_ioctl;
#if !defined(FDDI_FICUS) && !defined(FDDI_BONSAI)
	ri->ri_if.if_rtrequest = arp_rtrequest;

	pciio_config_set(conn_vhdl, PCI_CFG_COMMAND, 2, 0x145);
	pciio_config_set(conn_vhdl, PCI_CFG_LATENCY_TIMER, 1, 0x40);
#else
	/* map and set the config registers */
	ri->ri_cfgregs = (struct rns_cfgregs *)pciio_piotrans_addr(conn_vhdl,0,
							PCIIO_SPACE_CFG, 0,
							PCI_CFG_VEND_SPECIFIC,
							0);
	pciio_pio_write16(0x145, &ri->ri_cfgregs->cmd);
	pciio_pio_write8(0x40, &ri->ri_cfgregs->latency_tmr);
#endif

	/* map the board registers */
	ri->ri_regs = (rns_regs_t *)pciio_piotrans_addr(conn_vhdl, 0,
							PCIIO_SPACE_WIN(0), 0,
							sizeof(rns_regs_t), 0);

	/* leave the DMA and FDDI machinery reset just in case */
	ZAPBD(ri);

	/* copy the NVRAM from the board */
	for (i = 0, nvram_off = 0; i < sizeof(ri->ri_nvram.nvram_mac); i++)
		ri->ri_nvram.nvram_mac[i] = rns_get_nvram(ri, &nvram_off, 8);

	ri->ri_nvram.nvram_part_no = rns_get_nvram(ri, &nvram_off, 24);
	ri->ri_nvram.nvram_rev = rns_get_nvram(ri, &nvram_off, 12);
	ri->ri_nvram.nvram_cust_id = rns_get_nvram(ri, &nvram_off, 16);
	ri->ri_nvram.nvram_hw_part_no = rns_get_nvram(ri, &nvram_off, 24);

	for (pt = product_tbl; pt->p_part_no != 0; pt++) {
		if (pt->p_part_no == ri->ri_nvram.nvram_part_no)
			break;
	}
	ri->ri_product = pt;

	rns_set_addr(ri, ri->ri_nvram.nvram_mac);

	if (pt->p_attr & RNS_DAS) {
		ri->ri_smt1.smt_conf.type = SM_DAS;
		ri->ri_smt2.smt_conf.type = SM_DAS;
		ri->ri_smt1.smt_conf.pc_type = PC_B;
		ri->ri_smt2.smt_conf.pc_type = PC_A;
	} else {
		ri->ri_smt1.smt_conf.type = SAS;
		ri->ri_smt1.smt_conf.pc_type = PC_S;
	}

#ifdef FDDI_BONSAI
	dev_desc = (device_desc_t)kmem_zalloc(sizeof(*dev_desc), KM_SLEEP);
	if (!dev_desc) {
		cmn_err_tag(191,CE_ALERT, "rns%d: no memory for dev_desc\n", unit);
		splx(s);
		return 0;
	}
	dev_desc->intr_swlevel = splimp;
#else
	hwgraph_fastinfo_set(ri->ri_vhdl, (arbitrary_info_t)ri);

	dev_desc = device_desc_dup(ri->ri_vhdl);
	device_desc_intr_name_set(dev_desc, "RNS FDDI");
	device_desc_default_set(ri->ri_vhdl,dev_desc);
#endif
#ifdef NOT_LEGO
	/* seems to be required in order for pciio_intr_connect() to work */
	device_desc_intr_swlevel_set(dev_desc, (ilvl_t)splimp);
#endif
	intr_handle = pciio_intr_alloc(conn_vhdl, dev_desc,
				       PCIIO_INTR_LINE_A, ri->ri_vhdl);
	if (pciio_intr_connect(intr_handle, (intr_func_t)if_rnsintr, ri,0) < 0)
		cmn_err_tag(192,CE_ALERT, "rns %v: Can't install isr\n", conn_vhdl);

#ifdef FDDI_BONSAI
	inv_ptr = find_inventory(0, INV_PCI_NO_DRV, -1,
				 IP32_hinv_location(conn_vhdl), -1, -1);
	if (!inv_ptr) {
		cmn_err_tag(193,CE_ALERT, "rns%d: Can't find card in inventory", unit);
	} else {
		replace_in_inventory(inv_ptr,
				     INV_NETWORK, INV_NET_FDDI, INV_FDDI_RNS,
				     unit, ri->ri_nvram.nvram_rev);
	}
	if_rnsopen(ri);
	splx(s);
#else
	device_inventory_add(vhdl, INV_NETWORK, INV_NET_FDDI, INV_FDDI_RNS,
			     -1, ri->ri_nvram.nvram_rev);
#endif
	return 0;
}

#if defined(FDDI_BONSAI)
int rns_ip32_pio_predelay = 5;
int rns_ip32_pio_postdelay = 5;

/* CRIME 1.1 PIO read war routines.
 *
 * These routines add delays before and after PIO reads to addresses in MACE
 * and its subsystems.  They must be used with CRIME 1.1 parts to ensure that
 * any bogus reads generated by the hardware bug on that part have completed
 * prior to issuing the PIO read.  The delay after the read is required to
 * prevent subsequent back-to-back writes from hanging MACE.
 *
 * Note that this workaround does nothing to prevent the generation of bogus
 * reads in the first place.  Those may still cause problems for registers
 * with read side effects.
 */

/* function: pio_read32(uint32_t *)
 * purpose:  return result of pio word read
 */
uint32_t
rns_pciio_pio_read32(volatile uint32_t *addr)
{
      int s;
      uint32_t readval;

      s = spl7();
      us_delay(rns_ip32_pio_predelay);
      readval = *addr;
      us_delay(rns_ip32_pio_postdelay);
      splx(s);

      return readval;
}

/* function: pio_write8(uint8_t, uint8_t *)
 * purpose:  pio write byte to addr
 */
void
rns_pciio_pio_write8(uint8_t val, volatile uint8_t *addr)
{
      *addr = val;
}

/* function: pio_write16(uint16_t, uint16_t *)
 * purpose:  pio write halfword to addr
 */
void
rns_pciio_pio_write16(uint16_t val, volatile uint16_t *addr)
{
      *addr = val;
}

/* function: pio_write32(uint32_t, uint32_t *)
 * purpose:  pio write word to addr
 */
void
rns_pciio_pio_write32(uint32_t val, volatile uint32_t *addr)
{
      *addr = val;
}
#endif	/* FDDI_BONSAI */
