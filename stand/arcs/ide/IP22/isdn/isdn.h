/*
 * These can be used from plp.h without change.
 */

#define MS1			1000		/* 1 ms */
#define MS10			(10 * MS1)	/* 10 ms */
#define S1			1000 * MS1	/* 1 sec */
#define	S5			(5 * S1)	/* 5 sec */
#define S30			(30 * S1)	/* 30 sec */

#define NCHARS	200
/* See page 74 of HSCX.  If the last part of a message is 16-32 bytes,
 * HSCX requests 32 bytes of transfer. */
#define RECV_BUF_SIZE	256
#define NUM_DESCRIPTORS 4

/*
 * For the pbus.dmactl register, tx
 * Fifo end = 1
 * fifo beg = 0
 *
 * For rx,
 * Fifo end = 3
 * Fifo beg = 2
 * Also, must set bit 3 on lowest byte
 * 
 * highwater = 1 -> service at 2 bytes
 * then for START , activate and enable.
 * for      STOP, activate = 0 and enable.
 */
#define B1_DMA_TX_START	0x01000130
#define B1_DMA_TX_STOP	0x01000120
#define B1_DMA_RX_START	0x03020134
#define B1_DMA_RX_STOP	0x03020124

/* 
 * For tx. fifo beg. 4, ends at 5
 * For rx. fifo beg. 6, ends at 7
 */
#define B2_DMA_TX_START	0x05040130
#define B2_DMA_TX_STOP	0x04040120
#define B2_DMA_RX_START	0x07060134
#define B2_DMA_RX_STOP	0x07060124

#define HPC3_XIE	0x20000000
#define HPC3_EOX	0x80000000

#define INT3_BASE	0x1fbd9800
#define L0MR		0x21
#define L1SR		0x22
#define L1MR		0x23
#define MM0R		0x25
#define MM1R		0x26

#define K1_INT3_L0_MASK_ADDR	(unsigned char *) \
     				PHYS_TO_K1(INT3_BASE + (L0MR * 4) | 0x3)
#define K1_INT3_MM0R_MASK_ADDR	(unsigned char *) \
     				PHYS_TO_K1(INT3_BASE + (MM0R * 4) | 0x3)
#define K1_INT3_MM1R_MASK_ADDR	(unsigned char *) \
     				PHYS_TO_K1(INT3_BASE + (MM1R * 4) | 0x3)

#define K1_INT3_L1_MASK_ADDR	(unsigned char *) \
     				PHYS_TO_K1(INT3_BASE + (L1MR * 4) | 0x3)
#define K1_INT3_L1_ISR_ADDR	(unsigned char *) \
				PHYS_TO_K1(INT3_BASE + (L1SR * 4) | 0x3)

#define HSCX_RECV_FIFO_BASE	(unsigned char *) PHYS_TO_K1(HSCX_BASE | 0x3) 
#define HSCX_FIFO_SIZE 32
/*
 * HSCX and ISAC share the Local1<2> interrupt on the IOC.  This is then
 * mapped to Local1 bit 2 of R4k
 */
#define LIO_SAB_MASK	0x4
#define LIO_SAB		0x4
#define LIO_PEB_MASK	0x1
#define LIO_PEB		0x1
#undef LIO_HPC3
#define LIO_HPC3	0x10
#define LIO_HPC3_MASK	0x10

/*
 * These sets the direction to output.
 * If General control register bit 1 is on, then B2 tx DMA is enabled,
 * if off, then B1 tx DMA is.
 */
#define IOC_GENSEL_SHAREDDMA	0x7
#define IOC_GENCTL_SHAREDTXB2	0x2


/* memory descriptor structure 
 */
struct md {
    unsigned cbp;
    unsigned bc;
    unsigned nbdp;
    unsigned pad;
};

#define ISAC_BASE       0xbfbd9400
#define HSCX_BASE	0xbfbd9000
#define ISAC_REG_SP	4
#define ISAC_FIFO_BASE     (unsigned char *) PHYS_TO_K1(ISAC_BASE | 0x3)
/* Macro to define registers + Endien offset of 3
*/
#define ISAC_REG(OFF) ((ISAC_BASE+(OFF*ISAC_REG_SP)) | 0x3)

/* Register addresses.
*  R_ prefix means it is a read only register
*  W_ prefix means it is a write only register
*  X_ prefix means it is a read/write register
*/
#define R_ISTA ISAC_REG(0x20)                           /* interrupt status */
#define W_MASK ISAC_REG(0x20)                            /* interrupt masks */
#define R_STAR ISAC_REG(0x21)                                     /* status */
#define W_CMDR ISAC_REG(0x21)                                    /* command */
#define X_MODE ISAC_REG(0x22)                               /* mode control */
#define X_TIMR ISAC_REG(0x23)                              /* timer control */
#define R_EXIR ISAC_REG(0x24)                  /* extended interrupt status */
#define W_XAD1 ISAC_REG(0x24)                         /* transmit address 1 */
#define R_RBCL ISAC_REG(0x25)                 /* rcv frame byte count (low) */
#define W_XAD2 ISAC_REG(0x25)                         /* transmit address 2 */
#define R_SAPR ISAC_REG(0x26)                              /* received SAPI */
#define W_SAP1 ISAC_REG(0x26)                             /* SAPI address 1 */
#define R_RSTA ISAC_REG(0x27)                                 /* rcv status */
#define W_SAP2 ISAC_REG(0x27)                             /* SAPI address 2 */
#define W_TEI1 ISAC_REG(0x28)                              /* TEI address 1 */
#define R_RHCR ISAC_REG(0x29)                      /* received HDLC control */
#define W_TEI2 ISAC_REG(0x29)                              /* TEI address 2 */
#define R_RBCH ISAC_REG(0x2a)                   /* rx frame byte count high */
#define X_SPCR ISAC_REG(0x30)                        /* serial port control */
#define R_CIR0 ISAC_REG(0x31)                   /* command/indication rcv 0 */
#define W_CIX0 ISAC_REG(0x31)                   /* command/indication xmt 0 */
#define R_MOR0 ISAC_REG(0x32)                              /* monitor rcv 0 */
#define W_MOX0 ISAC_REG(0x32)                              /* monotor xmt 0 */
#define R_CIR1 ISAC_REG(0x33)                   /* command/indication rcv 1 */
#define W_CIX1 ISAC_REG(0x33)                   /* command/indication trn 1 */
#define R_MOR1 ISAC_REG(0x34)                              /* monitor rcv 1 */
#define W_MOX1 ISAC_REG(0x34)                              /* monotor xmt 1 */
#define X_C1R  ISAC_REG(0x35)                             /* channel 1 data */
#define X_C2R  ISAC_REG(0x36)                             /* channel 2 data */
#define R_B1CR ISAC_REG(0x37)                            /* B1 channel data */
#define W_STCR ISAC_REG(0x37)               /* synchronous transfer control */
#define R_B2CR ISAC_REG(0x38)                            /* B2 channel data */
#define W_ADF1 ISAC_REG(0x38)                   /* additional feature reg 1 */
#define X_ADF2 ISAC_REG(0x39)                   /* additional feature reg 2 */
#define R_MOSR ISAC_REG(0x3a)                     /* monitor channel status */
#define W_MOCR ISAC_REG(0x3a)                    /* monitor channel control */
#define R_SQRR ISAC_REG(0x3b)                  /* S,Q chan. rcv status/data */
#define W_SQXR ISAC_REG(0x3b)               /* S,Q chan. trans control/data */

/* ISAC registers */

#define  ISTA_CISQ   0x04

#define  CIR0_CIC0	0x02
#define  CIR0_CODR0_MASK	0x3c
#define  CIR0_TI    	0x28
#define  CIR0_DR	0x00
#define  CIR0_EI	0x18
#define  CIR0_PU	0x1c
#define  CIR0_RSY	0x10
#define  CIR0_ARD	0x20
#define  CIR0_AI8	0x30
#define  CIR0_AI10	0x34
#define  CIR0_DID	0x3c
#define  CIR0_SQC	0x80

#define  SQRR_SYN	0x10

#define  ISAC_VERS_MASK	0x7f
#define  ISAC_VERS	0x60


/* HSCX base address and channel offsets */

#define  HSCX_CH_A      0x00
#define  HSCX_CH_B      0x40
#define  B_CHAN1_OFFSET	0x00
#define  B_CHAN2_OFFSET	0x40

#define  HSCXREG(x, c)	((HSCX_BASE + \
			 ((x) + ((c) == 1 ? B_CHAN1_OFFSET : B_CHAN2_OFFSET)) \
			 * ISAC_REG_SP | 0x03))
/* Register offsets */

#define  FIFO_ADR       (HSCX_BASE+0x00)            /* RFIFO and XFIFO */
#define  ISTA_ADR(c)    HSCXREG(0x20, (c))  /* Interrupt Status Register */
#define  MASK_ADR(c)    HSCXREG(0x20, (c))    /* Interrupt Mask Register */
#define  STAR_ADR(c)    HSCXREG(0x21, (c))            /* status register */
#define  CMDR_ADR(c)    HSCXREG(0x21, (c))           /* Command Register */
#define  MODE_ADR(c)    HSCXREG(0x22, (c))              /* Mode Register */
#define  TIMR_ADR(c)    HSCXREG(0x23, (c))             /* Timer Register */
#define  EXIR_ADR(c)    HSCXREG(0x24, (c)) /* extended interrupt register */
#define  XAD1_ADR(c)    HSCXREG(0x24, (c))               /* TX address 1 */
#define  RBCL_ADR(c)    HSCXREG(0x25, (c))     /* receive byte count low */
#define  XAD2_ADR(c)    HSCXREG(0x25, (c))               /* TX address 2 */
#define  RAH1_ADR(c)    HSCXREG(0x26, (c))     /* receive address high 1 */
#define  RAH2_ADR(c)    HSCXREG(0x27, (c))     /* receive address high 2 */
#define  RSTA_ADR(c)    HSCXREG(0x27, (c))             /* receive status */
#define  RAL1_ADR(c)    HSCXREG(0x28, (c))      /* receive address low 1 */
#define  XBCL_ADR(c)    HSCXREG(0x2A, (c))    /* tx byte count low */
#define  CCR2_ADR(c)    HSCXREG(0x2C, (c))      /* channel config reg. 2 */
#define  RBCH_ADR(c)    HSCXREG(0x2D, (c))    /* receive byte count high */
#define  XBCH_ADR(c)    HSCXREG(0x2D, (c))    /* tx byte count high */
#define  VSTR_ADR(c)    HSCXREG(0x2E, (c))    /* version status register */
#define  CCR1_ADR(c)    HSCXREG(0x2F, (c))      /* channel config reg. 1 */
#define  TSAX_ADR(c)    HSCXREG(0x30, (c)) /* timeslot assignment transm. */
#define  TSAR_ADR(c)    HSCXREG(0x31, (c))   /* timeslot assignment rec. */
#define  XCCR_ADR(c)    HSCXREG(0x32, (c))  /* channel capacity transmit */
#define  RCCR_ADR(c)    HSCXREG(0x33, (c))   /* channel capacity receive */


/* register bits */

#define  ISTA_RME    0x80                       /* Receive Message End */
#define  ISTA_RPF    0x40                         /* Receive Pool Full */
#define  ISTA_XPR    0x10                       /* Transmit Pool Ready */
#define  ISTA_TIN    0x08                                 /* timer int */
#define  ISTA_ICA    0x04                  /* Interrupt ISTA channel A */
#define  ISTA_EXA    0x02                  /* Interrupt EXIR channel A */
#define  ISTA_EXB    0x01                  /* Interrupt EXIR channel B */

#define  MASK_RME    0x80
#define  MASK_RSC    0x20
#define  MASK_TIN    0x08
#define  MASK_XPR    0x10

#define  EXIR_RFO    0x10                    /* Receive Frame Overflow */
#define  EXIR_XDU    0x40		     /* Xmit Data Underrun */

#define  RSTA_VFR    0x80                               /* Valid Frame */
#define  RSTA_RDO    0x40                     /* Receive Data Overflow */
#define  RSTA_CRC    0x20                                 /* CRC check */
#define  RSTA_RAB    0x10                   /* Receive Message Aborted */

#define  CMDR_RMC    0x80                  /* Receive Message Complete */
#define  CMDR_RHR    0x40                       /* Reset HDLC Receiver */
#define  CMDR_XTF    0x08                /* Transmit Transparent Frame */
#define  CMDR_XME    0x02                      /* Transmit Message End */
#define  CMDR_XRES   0x01                            /* Transmit Reset */

#define  STAR_XFW    0x40                      /* TX fifo write enable */
#define  STAR_RLI    0x08                               /* RX Inactive */
#define  STAR_CEC    0x04                         /* Command Executing */

#define  MODE_MDS1   0x80                             /* Mode Select 1 */
#define  MODE_ADM    0x20                              /* Address Mode */
#define  MODE_RAC    0x08                           /* Receiver Active */
#define  MODE_TRS    0x02                          /* timer resolution */
#define  MODE_TLP    0x01                                 /* Test-Loop */

#define  CCR1_PU     0x80                                  /* Power Up */
#define  CCR1_SC1    0x40                                /* SC1 - NRZI */
#define  CCR1_ODS    0x10                      /* Output driver select */
#define  CCR1_ITF    0x08             /* Interframe Time Fill (FLAGS ) */
#define  CCR1_CM0    0x01                                /* Clock Mode */
#define  CCR1_CM2    0x04                                 /*    "    " */

#define  CCR2_XCS0   0x20                      /* TX clock shift bit 0 */
#define  CCR2_RCS0   0x10                      /* RC clock shift bit 0 */
#define  CCR2_TIO    0x08        /* Transmit Clock Input Output Switch */

