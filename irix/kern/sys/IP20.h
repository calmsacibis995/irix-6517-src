/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "sys/IP20.h: $Revision: 1.50 $"

/*
 * IP20.h -- cpu board specific defines for HP2
 */

#ifndef __SYS_IP20_H__
#define	__SYS_IP20_H__

#define _ARCSPROM

#define MCREGWBUG 
#include "sys/mc.h"
#if !defined(_STANDALONE)
#include "sys/types.h"
#endif

/* 
 * Local I/O Interrupt Status Register  (INT2)
 */
#ifdef	_MIPSEB
#define LIO_0_ISR_ADDR	0x1fb801c3	/* Local IO interrupt status (b,ro) */
#define LIO_1_ISR_ADDR	0x1fb801cb	/* Local IO interrupt status (b,ro) */
#else	/* _MIPSEL */
#define LIO_0_ISR_ADDR	0x1fb801c0	/* Local IO interrupt status (b,ro) */
#define LIO_1_ISR_ADDR	0x1fb801c8	/* Local IO interrupt status (b,ro) */
#endif	/* _MIPSEL */

/* LIO 0 status bits */
#define LIO_FIFO	0x01		/* FIFO full / GIO-0  interrupt */
#define LIO_GIO_0	0x01
#define LIO_CENTR	0x02		/* Centronics Printer Interrupt */
#define LIO_SCSI	0x04		/* SCSI interrupt */
#define LIO_ENET	0x08		/* Ethernet interrupt */
#define LIO_GDMA	0x10		/* Graphics dma done interrupt */
#define LIO_DUART	0x20		/* Duart interrupts (OR'd) */
#define LIO_GIO_1	0x40		/* GE/GIO-1/second HPC1 interrupt */
#define LIO_VME0	0x80		/* VME-0 interrupt */

/* LIO 1 status bits */
#define LIO_BIT0_UNUSED	0x01
#define LIO_GR1MODE	0x02		/* GR1 Mode(IP20)/Case Interlock(HP1) */
#define LIO_BIT2_UNUSED	0x04
#define LIO_VME1	0x08		/* VME-1 interrupt */
#define LIO_DSP		0x10		/* DSP interrupt */
#define LIO_AC		0x20		/* ACFAIL interrupt */
#define LIO_VIDEO	0x40		/* Video option interrupt */
#define LIO_GIO_2	0x80		/* Vert retrace / GIO-2 interrupt */

/* 
 * Local I/O Interrupt Mask Register  (INT2)
 */
#ifdef	_MIPSEB
#define LIO_0_MASK_ADDR	0x1fb801c7	/* Local IO register 0 mask (b) */
#define LIO_1_MASK_ADDR	0x1fb801cf	/* Local IO register 1 mask (b) */
#else	/* _MIPSEL */
#define LIO_0_MASK_ADDR	0x1fb801c4	/* Local IO register 0 mask (b) */
#define LIO_1_MASK_ADDR	0x1fb801cc	/* Local IO register 1 mask (b) */
#endif	/* _MIPSEL */

/* LIO 0 mask bits */
#define LIO_FIFO_MASK		0x01	/* FIFO full/GIO-0 interrupt mask */
#define LIO_CENTR_MASK		0x02	/* Centronics Printer Interrupt mask */
#define LIO_SCSI_MASK		0x04	/* SCSI interrupt mask */
#define LIO_ENET_MASK		0x08	/* Ethernet interrupt mask */
#define LIO_GDMA_MASK		0x10	/* Graphics DMA interrupt mask */
#define LIO_DUART_MASK		0x20	/* Duart interrupts mask*/
#define LIO_GE_MASK		0x40	/* GE/GIO-1 interrupt mask */
#define LIO_VME0_MASK		0x80	/* VME-0 interrupt mask */

/* LIO 1 mask bits */
#define LIO_MASK_BIT0_UNUSED	0x01
#define LIO_MASK_BIT1_UNUSED	0x02
#define LIO_MASK_BIT2_UNUSED	0x04
#define LIO_VME1_MASK		0x08	/* VME-1 interrupt */
#define LIO_DSP_MASK		0x10	/* DSP interrupt */
#define LIO_AC_MASK		0x20	/* AC fail interrupt */
#define LIO_VIDEO_MASK		0x40	/* Video option interrupt */
#define LIO_VR_MASK		0x80	/* Vert retrace/GIO-2 interrupt */

/* Local I/O vector numbers for setlclvector() (local 1 starts at 8) */
#define VECTOR_GIO0             0
#define VECTOR_PLP              1
#define VECTOR_SCSI             2
#define VECTOR_ENET             3
#define VECTOR_GDMA             4
#define VECTOR_DUART            5
#define VECTOR_GIO1             6
#define VECTOR_VME0             7
#define VECTOR_ILOCK            9
#define VECTOR_VME1             11
#define VECTOR_DSP              12
#define VECTOR_ACFAIL           13
#define VECTOR_VIDEO            14
#define VECTOR_GIO2             15

/* Local IO Segment */
#define LIO_ADDR        0x1f000000
#define LIO_GFX_SIZE    0x00400000      /* 4 Meg for graphics boards */
#define LIO_GIO_SIZE    0x00600000      /* 6 Meg for GIO Slots */

/* GIO Interrupts */
#define GIO_INTERRUPT_0         0
#define GIO_INTERRUPT_1         1
#define GIO_INTERRUPT_2         2

/* GIO Slots */
#define GIO_SLOT_0              0
#define GIO_SLOT_1              1
#define GIO_SLOT_GFX            2

/* Serial Port Configuration/LEDs
 *
 * On the HP1 board, the PORT_CONFIG register controls the configuration
 * of the serial ports and ethernet.  On IP20 it controls the LEDs.
 */
#ifdef	_MIPSEB
#define	PORT_CONFIG	0x1fb801df	/* LEDs/serial port config (5 bits) */
#else	/* _MIPSEL */
#define	PORT_CONFIG	0x1fb801dc	/* LEDs/serial port config (5 bits) */
#endif	/* _MIPSEL */

#define PCON_PARITY	0x01		/* IP20 cache parity error */
#define PCON_SERTCLK	0x01		/* External clock on for HP1 port 0 */
#define PCON_SER0RTS	0x02		/* RTS/TX+ for HP1 port 0 (RTS=1) */
#define PCON_SER1RTS	0x04		/* RTS/TX+ for HP1 port 1 (RTS=1) */
#define PCON_CLEARVRI	0x08		/* Disable/enable vert. retrace intr. */
#define PCON_POWER	0x10		/* Power off */

#define	LED_MASK	0x0e		/* led bits (3) */
#define	LED_HEART	0x02		/* 1 HZ Heartbeat */
#define LED_IDLE	0x04		/* off when cpu is idle */
#define LED_GFX		0x08

/* 
 * Clock and timer  (INT2)
 */
#define	PT_CLOCK_ADDR	PHYS_TO_K1(0x1fb801f0)	/* 8254 timer */
#ifdef	_MIPSEB
#define	TIMER_ACK_ADDR	PHYS_TO_K1(0x1fb801e3)	/* clear timer 2 bits (ws) */
#else	/* _MIPSEL */
#define	TIMER_ACK_ADDR	PHYS_TO_K1(0x1fb801e0)	/* clear timer 2 bits (ws) */
#endif	/* _MIPSEL */

#define ACK_TIMER0	0x1	/* write strobe to clear timer 0 */
#define ACK_TIMER1	0x2	/* write strobe to clear timer 1 */

/*
 * HPC ID register
 */
#define HPC_0_ID_ADDR	0x1fb80000	/* HPC 0 exists (primary HPC) */
#define HPC_1_ID_ADDR	0x1fb00000	/* HPC 1 exists */
#define HPC_2_ID_ADDR	0x1f980000	/* HPC 2 exists */
#define HPC_3_ID_ADDR	0x1f900000	/* HPC 3 exists */

/*
 * HPC Endianness register
 */
#ifdef	_MIPSEB
#define HPC_ENDIAN   	0x1fb800c3	/* dma endian settings (b) */
#else	/* _MIPSEL */
#define HPC_ENDIAN   	0x1fb800c0
#endif	/* _MIPSEL */

#define HPC_CPU_LITTLE	0x1		/* CPU little endian on reset */
#define HPC_ENET_LITTLE	0x2		/* Ethernet little endian */
#define HPC_SCSI_LITTLE	0x4		/* SCSI little endian */
#define HPC_PAR_LITTLE	0x8		/* Parallel little endian */
#define HPC_DSP_LITTLE	0x10		/* DSP little endian */
#define HPC_REV_MASK	0xc0		/* Rev = 01 for HPC1.5 */
#define HPC_REV_SHIFT	6

#define HPC_ALL_LITTLE  (HPC_CPU_LITTLE|HPC_ENET_LITTLE|HPC_SCSI_LITTLE| \
                         HPC_PAR_LITTLE|HPC_DSP_LITTLE)
#define HPC_ALL_BIG     (0)

/* 
 * Ethernet control (HPC1) - see bsd/misc/seeq.h
 */

/* 
 * SCSI Control (on HPC 0).  Always present.
 */
#define SCSI0_BC_ADDR	0x1fb80088	/* SCSI byte count register */
#define SCSI0_CBP_ADDR	0x1fb8008c	/* SCSI current buffer pointer */
#define SCSI0_NBDP_ADDR	0x1fb80090	/* SCSI next buffer pointer */
#define SCSI0_CTRL_ADDR	0x1fb80094	/* SCSI control register */

/* wc33c93 chip registers accessed through HPC1 */
#ifdef	_MIPSEB
#define SCSI0A_ADDR	0x1fb80122	/* SCSI WD33C93 address register (b) */
#define SCSI0D_ADDR	0x1fb80126	/* SCSI WD33C93 data register (b) */
#else	/* _MIPSEL */
#define SCSI0A_ADDR	0x1fb80121	/* SCSI WD33C93 address register (b) */
#define SCSI0D_ADDR	0x1fb80125	/* SCSI WD33C93 data register (b) */
#endif	/* _MIPSEL */

/* HPC1 registers for SCSI fifo */
#define SCSI0_PNTR_ADDR	0x1fb80098	/* SCSI pointer register */
#define SCSI0_FIFO_ADDR	0x1fb8009c	/* SCSI fifo data register */

/* 
 * 2nd SCSI Control (on HPC 1).  May not be present.
 */
#define SCSI1_BC_ADDR	0x1fb00088	/* SCSI byte count register */
#define SCSI1_CBP_ADDR	0x1fb0008c	/* SCSI current buffer pointer */
#define SCSI1_NBDP_ADDR	0x1fb00090	/* SCSI next buffer pointer */
#define SCSI1_CTRL_ADDR	0x1fb00094	/* SCSI control register */

/* wc33c93 chip registers accessed through HPC1 */
#ifdef	_MIPSEB
#define SCSI1A_ADDR	0x1fb00122	/* SCSI WD33C93 address register (b) */
#define SCSI1D_ADDR	0x1fb00126	/* SCSI WD33C93 data register (b) */
#else	/* _MIPSEL */
#define SCSI1A_ADDR	0x1fb00121	/* SCSI WD33C93 address register (b) */
#define SCSI1D_ADDR	0x1fb00125	/* SCSI WD33C93 data register (b) */
#endif	/* _MIPSEL */

/* HPC1 registers for SCSI fifo */
#define SCSI1_PNTR_ADDR	0x1fb00098	/* SCSI pointer register */
#define SCSI1_FIFO_ADDR	0x1fb0009c	/* SCSI fifo data register */

/* 
 * 3nd SCSI Control (on HPC 2).  May not be present.
 */
#define SCSI2_BC_ADDR	0x1f980088	/* SCSI byte count register */
#define SCSI2_CBP_ADDR	0x1f98008c	/* SCSI current buffer pointer */
#define SCSI2_NBDP_ADDR	0x1f980090	/* SCSI next buffer pointer */
#define SCSI2_CTRL_ADDR	0x1f980094	/* SCSI control register */

/* wc33c93 chip registers accessed through HPC1 */
#ifdef	_MIPSEB
#define SCSI2A_ADDR	0x1f980122	/* SCSI WD33C93 address register (b) */
#define SCSI2D_ADDR	0x1f980126	/* SCSI WD33C93 data register (b) */
#else	/* _MIPSEL */
#define SCSI2A_ADDR	0x1f980121	/* SCSI WD33C93 address register (b) */
#define SCSI2D_ADDR	0x1f980125	/* SCSI WD33C93 data register (b) */
#endif	/* _MIPSEL */

/* HPC1 registers for SCSI fifo */
#define SCSI2_PNTR_ADDR	0x1f980098	/* SCSI pointer register */
#define SCSI2_FIFO_ADDR	0x1f98009c	/* SCSI fifo data register */


#define SCSI_RESET	0x01		/* reset WD33C93 chip */
#define SCSI_FLUSH	0x02		/* flush SCSI buffers */
#define SCSI_TO_MEM	0x10		/* transfer in */
#define SCSI_STARTDMA	0x80		/* start SCSI dma transfer */

/* 
 * Parallel control (HPC1)
 */
#define PAR_OFFSET	0xa8		/* offset to par regs from HPC1 */
#define PAR_BC_ADDR	0x1fb800a8	/* parallel byte count and IE (w) */
#define PAR_CBP_ADDR	0x1fb800ac	/* parallel current buf ptr (w) */
#define PAR_NBDP_ADDR	0x1fb800b0	/* parallel next buf desc (w) */
#define PAR_CTRL_ADDR	0x1fb800b4	/* parallel control reg (w) */
#ifdef	_MIPSEB
#define PAR_SR_ADDR	0x1fb80135	/* parallel status/remote reg (b) */
#else	/* _MIPSEL */
#define PAR_SR_ADDR	0x1fb80136	/* parallel status/remote reg (b) */
#endif	/* _MIPSEL */

/* parallel control register bits */
#define PAR_CTRL_RESET		0x01	/* reset parallel port */
#define PAR_CTRL_INT  		0x02	/* interrupt pending */
#define PAR_CTRL_CLRINT		0x02	/* clear pending interrupt */
#define PAR_CTRL_POLARITY	0x04	/* strobe polarity */
#define PAR_CTRL_SOFTACK	0x08	/* soft acknowledge */
#define PAR_CTRL_PPTOMEM	0x10	/* dma parallel port to mem */
#define PAR_CTRL_MEMTOPP	0x00	/* dma mem to parallel port */
#define PAR_CTRL_IGNACK		0x20	/* ignore ack input */
#define PAR_CTRL_FLUSH		0x40	/* flush HPC PP buffers */
#define PAR_CTRL_STRTDMA	0x80	/* start dma */

#define PAR_STROBE_MIN_MASK	0x7f000000  /* mask for strobe duty cycle */
#define PAR_STROBE_LEAD_MASK	0x007f0000  /* mask for strobe lead edge */
#define PAR_STROBE_TRAIL_MASK	0x00007f00  /* mask for strobe tailing edge */
#define PAR_STROBE_MASK		0x7f7f7f00  /* mask for strobe */

/*
 * DUART Registers (accessed through HPC1)
 */
#ifdef	_MIPSEB
#define DUART0  (0x1fb80d03 + K1BASE)
#define DUART1  (0x1fb80d13 + K1BASE)
#define DUART2  (0x1fb80d23 + K1BASE)
#define DUART3  (0x1fb80d33 + K1BASE)
#else	/* _MIPSEL */
#define DUART0  (0x1fb80d00 + K1BASE)
#define DUART1  (0x1fb80d10 + K1BASE)
#define DUART2  (0x1fb80d20 + K1BASE)
#define DUART3  (0x1fb80d30 + K1BASE)
#endif	/* _MIPSEL */

#define CHNA_DATA_OFFSET        0xc
#define CHNA_CNTRL_OFFSET       0x8
#define CHNB_DATA_OFFSET        0x4
#define CHNB_CNTRL_OFFSET       0x0

#define DUART0A_DATA    (DUART0 + CHNA_DATA_OFFSET)
#define DUART0B_DATA    (DUART0 + CHNB_DATA_OFFSET)
#define DUART1A_DATA    (DUART1 + CHNA_DATA_OFFSET)
#define DUART1B_DATA    (DUART1 + CHNB_DATA_OFFSET)
#define DUART2A_DATA    (DUART2 + CHNA_DATA_OFFSET)
#define DUART2B_DATA    (DUART2 + CHNB_DATA_OFFSET)

#define DUART0A_CNTRL   (DUART0 + CHNA_CNTRL_OFFSET)
#define DUART0B_CNTRL   (DUART0 + CHNB_CNTRL_OFFSET)
#define DUART1A_CNTRL   (DUART1 + CHNA_CNTRL_OFFSET)
#define DUART1B_CNTRL   (DUART1 + CHNB_CNTRL_OFFSET)
#define DUART2A_CNTRL   (DUART2 + CHNA_CNTRL_OFFSET)
#define DUART2B_CNTRL   (DUART2 + CHNB_CNTRL_OFFSET)

/*
 * Board revision register - modem SRAM memory space
 */
#define BOARD_REV_ADDR	0x1fbd0000
#define REV_HP1		0x8000
#define REV_MASK	0x7000
#define REV_SHIFT	12

/*
 * Headphone Gain MDACS (accessed like they're DUART3)
 */
#define HEADPHONE_MDAC_L	(DUART3 + CHNB_CNTRL_OFFSET)
#define HEADPHONE_MDAC_R	(DUART3 + CHNB_DATA_OFFSET)

/* 
 * Battery backup (real time) clock (accessed through HPC1)
 */
#ifdef LANGUAGE_C
#define RT_CLOCK_ADDR	(struct dp8573_clk *)PHYS_TO_K1(0x1fb80e00)
#else 
#define RT_CLOCK_ADDR	PHYS_TO_K1(0x1fb80e00)
#endif

/* 
 * CPU aux control register  (HPC1)
 */
#ifdef	_MIPSEB
#define	CPU_AUX_CONTROL	0x1fb801bf	/* Serial ctrl and LED (b) */
#else	/* _MIPSEL */
#define	CPU_AUX_CONTROL	0x1fb801bc	/* Serial ctrl and LED (b) */
#endif	/* _MIPSEL */

#define SER_TO_CPU	0x10		/* Data from serial memory */
#define CPU_TO_SER	0x08		/* Data to serial memory */
#define SERCLK		0x04		/* Serial Clock */
#define	CONSOLE_CS	0x02		/* EEPROM (nvram) chip select */
#define	CONSOLE_LED	0x01		/* Console led */
#define	NVRAM_PRE	0x01		/* EEPROM (nvram) PRE pin signal */

/* Locations of HDSP-HPC1 registers in the MIPS address space. */
#define HPC1DMAWDCNT	0x1fb80180	/* DMA transfer size (SRAM words) */
#define HPC1GIOADDL	0x1fb80184	/* GIO-bus address, LSB (16 bit) */
#define HPC1GIOADDM	0x1fb80188	/* GIO-bus address, MSB (16 bit) */
#define HPC1PBUSADD	0x1fb8018c	/* PBUS address (16 bit) */
#define HPC1DMACTRL	0x1fb80190	/* DMA Control (2 bit) */
#define HPC1COUNTER	0x1fb80194	/* Counter (24 bits) (ro) */
#define HPC1HANDTX	0x1fb80198	/* Handshake transmit (16 bit) */
#define HPC1HANDRX	0x1fb8019c	/* Handshake receive (16 bit) */
#define HPC1CINTSTAT	0x1fb801a0	/* CPU Interrupt status (3 bit) */
#define HPC1CINTMASK	0x1fb801a4	/* CPU Interrupt masks (3 bit) */
#define HPC1MISCSR	0x1fb801b0	/* Misc. control and status (8 bit) */
#define HPC1BURSTCTL	0x1fb801b4	/* DMA Ballistics register (16 bit)*/

/* Bits in HPC1DMACTRL */
#define HPC1DMA_GO	0x1		/* start dma */
#define HPC1DMA_DIR	0x2		/* direction: 0 = MIPS->DSP */
#define HPC1DMA_MODE	0x4		/* mode: 0 = normal, 1 = folded */

/* Bits in HPC1CINTSTAT */
#define HPC1CINT_DMA	0x1		/* end of dma */
#define HPC1CINT_TX	0x2		/* dsp has written to HANDTX */
#define HPC1CINT_RX	0x4		/* dsp has read HANDRX */

/* Bits in HPC1MISCSR */
#define HPC1MISC_RESET	0x1		/* force hard reset to DSP */
#define HPC1MISC_IRQA	0x2		/* force /IRQA with polarity below */
#define HPC1MISC_POL	0x4		/* polarity of IRQA (1 = high) */
#define HPC1MISC_32K	0x8		/* SRAM size (1=32K, 0=8K) */
/* Sign extension mode bits in HPC1MISCSR */
#define HPC1MISC_8TO32	0x70		/* PBus 8 -> GIO 32 signed */
#define HPC1MISC_16TO32	0x60		/* PBus 16 -> GIO 32 signed */
#define HPC1MISC_24TO32 0x40		/* PBus 24 -> GIO 32 signed */
#define HPC1MISC_NOSIGN 0x0		/* no sign extension */
/* 
 * DSP RAM access (accessed through HPC1)
 */
#define HPC1MEMORY	0x1fbe0000	/* DSP Memory 24 bits x 32k */

/*
 * VME interrupt control registers (INT2)
 */
/* VME Interrupt Status/Mask Registers */
#ifdef	_MIPSEB
#define VME_ISR_ADDR	0x1fb801d3	/* VME interrupt status (b, ro) */
#define VME_ISR		VME_ISR_ADDR	/* VME interrupt status (b) */
#define VME_0_MASK_ADDR	0x1fb801d7	/* VME interrupt mask for LIO 0 (b) */
#define VME_1_MASK_ADDR	0x1fb801db	/* VME interrupt mask for LIO 1 (b) */
#else	/* _MIPSEL */
#define VME_ISR_ADDR	0x1fb801d0	/* VME interrupt status (b, ro) */
#define VME_ISR		VME_ISR_ADDR	/* VME interrupt status (b) */
#define VME_0_MASK_ADDR	0x1fb801d4	/* VME interrupt mask for LIO 0 (b) */
#define VME_1_MASK_ADDR	0x1fb801d8	/* VME interrupt mask for LIO 1 (b) */
#endif	/* _MIPSEL */

/* 
 * Vertical retrace status (INT2)
 */
#define VRSTAT_ADDR	VME_ISR_ADDR	/* VR status read from vme reg */
#define VRSTAT_MASK	0x01		/* Vert retrace status: no interrupt */

#ifdef LANGUAGE_C

/* chip interface structure for IP20 / HPC / WD33C93 */
typedef struct scuzzy {
	volatile unsigned char	*d_addr;	/* address register */
	volatile unsigned char	*d_data;	/* data register */
	volatile unsigned long	*d_ctrl;	/* control address */
	volatile unsigned long	*d_bcnt;	/* byte count register */
	volatile unsigned long	*d_curbp;	/* current buffer pointer */
	volatile unsigned long	*d_nextbp;	/* next buffer pointer */
	volatile unsigned long	*d_pntr;	/* fifo pointer register */
	volatile unsigned long	*d_fifo;	/* fifo data register */
	unsigned char d_initflags;	/* initial flags for d_flags */
	unsigned char d_clock;	/* value for clock register on WD chip */
} scuzzy_t;

/* descriptor structure for IP20 SCSI / HPC interface */
#ifdef	_MIPSEB
typedef struct scsi_descr {
	unsigned fill:19, bcnt:13;
	unsigned eox:1, efill:3, cbp:28;
	unsigned nfill:4, nbp:28;
	unsigned ophysaddr;	/* used if > 128 Mbytes of RAM */
} scdescr_t;
#else	/* _MIPSEL */
typedef struct scsi_descr {
	unsigned bcnt:13, fill:19;
	unsigned cbp:28, efill:3, eox:1;
	unsigned nbp:28, nfill:4;
	unsigned ophysaddr;	/* used if > 128 Mbytes of RAM */
} scdescr_t;
#endif	/* _MIPSEL */


#define NSCSI_DMA_PGS 64	/* map up to 256Kb per transfer.  This is sufficient
	for almost all disk and tape acesses.  Things like scanners or
	printers often transfer far more than a Mb per cmd, so they will
	have to re-map anyway, no matter how many we allocate */


/* this saves a lot of hard to read and type code in various places,
	and also ensures that we get the register size correct */
#define K1_LIO_0_MASK_ADDR ((volatile unchar *)PHYS_TO_K1(LIO_0_MASK_ADDR))
#define K1_LIO_1_MASK_ADDR ((volatile unchar *)PHYS_TO_K1(LIO_1_MASK_ADDR))
#define K1_LIO_0_ISR_ADDR ((volatile unchar *)PHYS_TO_K1(LIO_0_ISR_ADDR))
#define K1_LIO_1_ISR_ADDR ((volatile unchar *)PHYS_TO_K1(LIO_1_ISR_ADDR))
#define K1_VME_ISR_ADDR ((volatile unchar *)PHYS_TO_K1(VME_ISR_ADDR))
#define K1_VME_0_MASK_ADDR ((volatile unchar *)PHYS_TO_K1(VME_0_MASK_ADDR))
#define K1_VME_1_MASK_ADDR ((volatile unchar *)PHYS_TO_K1(VME_1_MASK_ADDR))

/* function prototypes for IP20 functions */

extern int spllintr(void);
extern int splhintr(void);
extern int splretr(void);
extern int splgio2(void);
extern int is_v50(void);
#endif /* LANGUAGE_C */


#if defined(_STANDALONE) || defined(PROM) || defined(LOCORE)
#include <sys/IP20nvram.h>
#define SR_PROMBASE     SR_CU0|SR_CU1
#endif /* _STANDALONE */

#endif /* __SYS_IP20_H__ */
