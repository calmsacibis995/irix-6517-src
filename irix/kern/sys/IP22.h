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

#ident "sys/IP22.h: $Revision: 1.104 $"

/*
 * IP22.h -- cpu board specific defines for IP22
 *
 * The IP22 is equipped with an R4000, MC, HPC3, and INT2 chip.  HPC3 and MC
 * definitions are contained in header files that are included here.
 *
 * The IP24 cpu board is architecturally the same as the IP22 with the
 * following exceptions:
 *   1. Uses an IOC1 chip containing the following parts:
 *        INT3      - replaces INT2
 *        VTI85CX30 - replaces Z85230
 *        VTI8042   - replaces Intel 8242
 *   2. Sys_ID register identifies IP24 cpu board and ioc1 presence.
 *   3. Other register name and offset changes as noted in this file and
 *      sys/hpc3.h.
 *
 *  The IP26 is a TFP based processor module that uses a derivative
 * of IP22 base board that contains ECC memory.
 */

#ifndef __SYS_IP22_H__
#define	__SYS_IP22_H__

#define _ARCSPROM

#ifdef _MIPSEB			/* IP22 byte offset for pio */
#define IP22BOFF(X)		((X)|0x3)
#else
#define IP22BOFF(X)
#endif

#if IP22			/* shouldn't need this on TFP */
#include "sys/hpc1.h"
#endif
#include "sys/hpc3.h"
#include "sys/mc.h"
#include "sys/ds1286.h"

/* On IP22-class machines (Indigo2, Indy, Power Indigo2), the maximum
 * number of memory banks is 3 (or less).
 */
#define MAX_MEM_BANKS	3

/* 
 * Local I/O Interrupt Status Register  (INT2/INT3)
 */
#define LIO_0_ISR_OFFSET	IP22BOFF(0x0)	/* Local IO intr status (ro) */
#define LIO_1_ISR_OFFSET	IP22BOFF(0x8)	/* Local IO intr status (ro) */
#define	LIO_2_3_ISR_OFFSET	IP22BOFF(0x10)	/* Local IO intr status (ro) */

#ifdef LANGUAGE_C		/* HPC3_INT_ADDR contains c reference */
#define LIO_0_ISR_ADDR		(HPC3_INT_ADDR+LIO_0_ISR_OFFSET)
#define LIO_1_ISR_ADDR		(HPC3_INT_ADDR+LIO_1_ISR_OFFSET)
#define	LIO_2_3_ISR_ADDR	(HPC3_INT_ADDR+LIO_2_3_ISR_OFFSET)
#endif

/* LIO 0 status bits */
#define LIO_FIFO	0x01		/* FIFO full / GIO-0  interrupt */
#define LIO_GIO_0	0x01
#define LIO_SCSI_0	0x02		/* SCSI controller 0 interrupt */
#define LIO_SCSI_1	0x04		/* SCSI controller 1 interrupt */
#define LIO_ENET	0x08		/* Ethernet interrupt */
#define LIO_GDMA	0x10		/* Graphics dma done interrupt */
#define LIO_CENTR	0x20		/* Parallel port interrupt */
#define LIO_GIO_1	0x40		/* IP22: GE/GIO-1/2nd-HPC interrupt */
#define LIO_LIO2	0x80		/* LIO2 interrupt */

/* LIO 1 status bits */
#define LIO_ISDN_ISAC	0x01		/* IP24: ISDN isac */
#define LIO_POWER	0x02		/* Power switch pressed interrupt */
#define LIO_ISDN_HSCX	0x04		/* IP24: ISDN hscx */
#define LIO_LIO3	0x08		/* LIO3 interrupt */
#define LIO_HPC3	0x10		/* HPC3 interrupt */
#define LIO_AC		0x20		/* ACFAIL interrupt */
#define LIO_VIDEO	0x40		/* Video option interrupt */
#define LIO_GIO_2	0x80		/* Vert retrace / GIO-2 interrupt */

/* Mapped LIO interrupts.  These interrupts may be mapped to either 0, or 1 */
#define LIO_VERT_STAT	0x01		/* INT3: newport vertical status */
#define	LIO_PASSWD	0x02		/* Pasword checking enable */
#define	LIO_ISDN_POWER	0x04		/* IP24: ISDN power */
#define	LIO_EISA	0x08		/* IP22: EISA interrupt */
#define	LIO_KEYBD_MOUSE	0x10		/* Keyboard/Mouse interrupt */
#define	LIO_DUART	0x20		/* Duart interrupt */
#define LIO_DRAIN0	0x40		/* IP22: gfx fifo not full */
#define LIO_DRAIN1	0x80		/* IP22: gfx fifo not full */
#define LIO_GIO_EXP0	0x40		/* IP24: gio expansion 0 */
#define LIO_GIO_EXP1	0x80		/* IP24: gio expansion 1 */

/* 
 * Local I/O Interrupt Mask Register  (INT2/INT3)
 */
#define LIO_0_MASK_OFFSET	IP22BOFF(0x4)	/* Local 0 intr mask (b) */
#define LIO_1_MASK_OFFSET	IP22BOFF(0xc)	/* Local 1 intr mask (b) */
#define	LIO_2_MASK_OFFSET	IP22BOFF(0x14)	/* Mapped LIO 0 (LIO2) mask */
#define	LIO_3_MASK_OFFSET	IP22BOFF(0x18)	/* Mapped LIO 1 (LIO3) mask */

#ifdef LANGUAGE_C		/* HPC3_INT_ADDR contains c reference */
#define LIO_0_MASK_ADDR		(HPC3_INT_ADDR+LIO_0_MASK_OFFSET)
#define LIO_1_MASK_ADDR		(HPC3_INT_ADDR+LIO_1_MASK_OFFSET)
#define	LIO_2_MASK_ADDR		(HPC3_INT_ADDR+LIO_2_MASK_OFFSET)
#define	LIO_3_MASK_ADDR		(HPC3_INT_ADDR+LIO_3_MASK_OFFSET)
#endif

/* LIO 0 mask bits */
#define LIO_FIFO_MASK		0x01	/* FIFO full/GIO-0 interrupt mask */
#define LIO_SCSI_0_MASK		0x02	/* SCSI 0 interrupt mask */
#define LIO_SCSI_1_MASK		0x04	/* SCSI 1 interrupt mask */
#define LIO_ENET_MASK		0x08	/* Ethernet interrupt mask */
#define LIO_GDMA_MASK		0x10	/* Graphics DMA interrupt mask */
#define LIO_CENTR_MASK		0x20	/* Centronics Printer Interrupt mask */
#define LIO_GE_MASK		0x40	/* GE/GIO-1 interrupt mask */
#define LIO_LIO2_MASK		0x80	/* LIO2 interrupt mask */

/* LIO 1 mask bits */
#define LIO_MASK_BIT0_UNUSED	0x01	/* IP22: Unused */
#define LIO_ISDN_ISAC_MASK	0x01	/* IP24: ISDN isac */
#define LIO_MASK_POWER		0x02	/* Panel on IP24 */
#define LIO_MASK_BIT2_UNUSED	0x04	/* IP22: Unused */
#define LIO_ISDN_HSCX_MASK	0x04	/* IP24: ISDN hscx */
#define LIO_LIO3_MASK		0x08	/* LIO3 interrupt */
#define LIO_HPC3_MASK		0x10	/* HPC3 interrupt */
#define LIO_AC_MASK		0x20	/* AC fail interrupt */
#define LIO_VIDEO_MASK		0x40	/* Video option interrupt */
#define LIO_VR_MASK		0x80	/* Vert retrace/GIO-2 interrupt */

/* Mapped interrupt mask bits */
#define LIO_DUART_MASK		0x20	/* Duart interrupts mask*/

/* Local I/O vector numbers for setlclvector().
 *	- local 1 starts at 8
 *	- local 2 starts at 16 (VME register)
 */
#define VECTOR_GIO0		0		/* fifo full */
#define VECTOR_SCSI		1
#define VECTOR_SCSI1		2
#define VECTOR_ENET		3
#define VECTOR_GDMA		4		/* mc dma complete */
#define VECTOR_PLP		5
#define VECTOR_GIO1		6		/* graphics interrupt */
#define VECTOR_LCL2		7
#define VECTOR_ISDN_ISAC	8
#define VECTOR_POWER		9
#define VECTOR_ISDN_HSCX	10
#define VECTOR_LCL3		11
#define VECTOR_HPCDMA		12
#define VECTOR_ACFAIL		13
#define VECTOR_VIDEO		14
#define VECTOR_GIO2		15		/* vertical retrace */
#define VECTOR_EISA		19
#define VECTOR_KBDMS		20
#define VECTOR_DUART		21
#define VECTOR_DRAIN0		22		/* IP22: fifo not full */
#define VECTOR_DRAIN1		23		/* IP22: fifo not full */
#define VECTOR_GIOEXP0		22		/* IP24: gio slot 0 */
#define VECTOR_GIOEXP1		23		/* IP24: gio slot 1 */
#define	NUM_LCL_VEC		24

/* GIO Interrupts */
#define GIO_INTERRUPT_0         0
#define GIO_INTERRUPT_1         1
#define GIO_INTERRUPT_2         2

/* GIO Slots */
#define GIO_SLOT_0              0
#define GIO_SLOT_1              1		/* Indy only */
#define GIO_SLOT_GFX		2

/* Port Configuration
 *
 * On the (fullhouse) IP22 board, the PORT_CONFIG register controls gio
 * slot reset and the vertical retrace interrupt.
 *
 * On the (guinness) IP24 board, PORT_CONFIG is not used. Instead:
 *   IOC1/LOCAL1 STATUS/MASK is for vertical retrace functions; and
 *   IOC1/WRITE2 is for ETHERNET and SERIAL configurations.
 */
#define	PORT_CONFIG		HPC3_INT2_ADDR+PORT_CONFIG_OFFSET
#define	PORT_CONFIG_OFFSET	IP22BOFF(0x1c)
#define PCON_DMA_SYNC_SEL	0x01	/* DMA SYNC TO MC- 1=SLOT1 0=SLOT0 */
#define PCON_SG_RESET_N		0x02	/* reset slot 0 */
#define PCON_S0_RESET_N		0x04	/* reset slot 1 */
#define PCON_CLR_SG_RETRACE_N	0x08	/* clear slot 0 retrace */
#define PCON_CLR_S0_RETRACE_N	0x10	/* clear slot 1 retrace */

/* 
 * Clock and timer  (INT2/INT3)
 */

/* 8254 timer */
#define	PT_CLOCK_ADDR		PHYS_TO_COMPATK1(HPC3_INT_ADDR+PT_CLOCK_OFFSET)
#define	PT_CLOCK_OFFSET		0x30

/* clear timer 2 bits (ws) */
#define	TIMER_ACK_ADDR		PHYS_TO_COMPATK1(HPC3_INT_ADDR+TIMER_ACK_OFFSET)
#define	TIMER_ACK_OFFSET	IP22BOFF(0x20)
#define ACK_TIMER0	0x1	/* write strobe to clear timer 0 */
#define ACK_TIMER1	0x2	/* write strobe to clear timer 1 */

/*
 * HPC ID register
 */
#define HPC_2_ID_ADDR	0x1f980000	/* (HPC1.5) #2 exists */
#define HPC_1_ID_ADDR	0x1fb00000	/* (HPC1.5 or 3) #1 exists */
#define HPC_0_ID_ADDR	0x1fb80000	/* HPC3 #0 exists (primary HPC) */
#define HPC_0_ID_PROM0	0x1fc00000	/* HPC3 #0, PROM #0 exists */
#define HPC_0_ID_PROM1	0x1fe00000	/* HPC3 #0, PROM #1 exists */

/* 
 * SCSI Control (on HPC3 channel 0).  Always present.
 */
#define SCSI0_BC_ADDR	HPC3_SCSI_BC0	       /* SCSI byte count register */
#define SCSI0_CBP_ADDR	HPC3_SCSI_BUFFER_PTR0  /* SCSI current buffer pointer */
#define SCSI0_NBDP_ADDR	HPC3_SCSI_BUFFER_NBDP0 /* SCSI next buffer pointer */
#define SCSI0_CTRL_ADDR	HPC3_SCSI_CONTROL0     /* SCSI control register */

/* wd33c93 chip registers accessed through HPC3 */
#define SCSI0A_ADDR IP22BOFF(HPC3_SCSI_REG0)	/* address register (b) */
#define SCSI0D_ADDR IP22BOFF(HPC3_SCSI_REG0+0x4)/* data register (b) */

/* 
 * 2nd SCSI Control (on fullhouse always present).
 */
#define SCSI1_BC_ADDR	HPC3_SCSI_BC1	       /* SCSI byte count register */
#define SCSI1_CBP_ADDR	HPC3_SCSI_BUFFER_PTR1  /* SCSI current buffer pointer */
#define SCSI1_NBDP_ADDR	HPC3_SCSI_BUFFER_NBDP1 /* SCSI next buffer pointer */
#define SCSI1_CTRL_ADDR	HPC3_SCSI_CONTROL1     /* SCSI control register */

/* wd33c93 chip registers accessed through HPC3 */
#define SCSI1A_ADDR IP22BOFF(HPC3_SCSI_REG1)	/* address register (b) */
#define SCSI1D_ADDR IP22BOFF(HPC3_SCSI_REG1+0x4)/* data register (b) */

/* 
 * 3rd SCSI Control (optional GIO board on Indy).
 */
#define SCSI2_BC_ADDR	HPC1_SCSI_BC2	       /* SCSI byte count register */
#define SCSI2_CBP_ADDR	HPC1_SCSI_BUFFER_PTR2  /* SCSI current buffer pointer */
#define SCSI2_NBDP_ADDR	HPC1_SCSI_BUFFER_NBDP2 /* SCSI next buffer pointer */
#define SCSI2_CTRL_ADDR	HPC1_SCSI_CONTROL2     /* SCSI control register */

/* wd33c93 chip registers accessed through HPC1 */
#define SCSI2A_ADDR	HPC1_SCSI_REG_A2       /* address register (b) */
#define SCSI2D_ADDR	HPC1_SCSI_REG_D2       /* data register (b) */

/* 
 * 4th SCSI Control (optional GIO board on Indy).
 */
#define SCSI3_BC_ADDR	HPC1_SCSI_BC3	       /* SCSI byte count register */
#define SCSI3_CBP_ADDR	HPC1_SCSI_BUFFER_PTR3  /* SCSI current buffer pointer */
#define SCSI3_NBDP_ADDR	HPC1_SCSI_BUFFER_NBDP3 /* SCSI next buffer pointer */
#define SCSI3_CTRL_ADDR	HPC1_SCSI_CONTROL3     /* SCSI control register */

/* wd33c93 chip registers accessed through HPC1 */
#define SCSI3A_ADDR	HPC1_SCSI_REG_A3       /* address register (b) */
#define SCSI3D_ADDR	HPC1_SCSI_REG_D3       /* data register (b) */

/*
 * 5th SCSI Controller (optional GIO-64 board on Indy server)
 */
#define SCSI4_BC_ADDR   HPC31_SCSI_BC0
#define SCSI4_CBP_ADDR  HPC31_SCSI_BUFFER_PTR0
#define SCSI4_NBDP_ADDR HPC31_SCSI_BUFFER_NBDP0
#define SCSI4_CTRL_ADDR HPC31_SCSI_CONTROL0

/* host adapter (wd95) chip registers accessed through HPC3 */
#define SCSI4A_ADDR IP22BOFF(HPC31_SCSI_REG0)
#define SCSI4D_ADDR IP22BOFF(HPC31_SCSI_REG0+0x8)

/*
 * 6th SCSI Controller (option GIO-64 board on Indy server)
 */
#define SCSI5_BC_ADDR   HPC31_SCSI_BC1
#define SCSI5_CBP_ADDR  HPC31_SCSI_BUFFER_PTR1
#define SCSI5_NBDP_ADDR HPC31_SCSI_BUFFER_NBDP1
#define SCSI5_CTRL_ADDR HPC31_SCSI_CONTROL1

/* host adapter (wd95) chip registers accessed through HPC3 */
#define SCSI5A_ADDR IP22BOFF(HPC31_SCSI_REG1)
#define SCSI5D_ADDR IP22BOFF(HPC31_SCSI_REG1+0x4)


#define SCSI_RESET	SCCH_RESET	/* reset WD33C93 chip */
#define SCSI_FLUSH	SCFLUSH		/* flush SCSI buffers */
#define SCSI_STARTDMA	SCCH_ACTIVE	/* start SCSI dma transfer */

/*
 * values for SCSI DMA and PIO config registers
 */
#define WD93_DMA_CFG_25MHZ   0x34801
#define WD93_DMA_CFG_33MHZ   0x34809
#define WD93_PIO_CFG_25MHZ   0x4288
#define WD93_PIO_CFG_33MHZ   0x42cd
#define WD95_DMA_CFG_25MHZ   0x1801
#define WD95_DMA_CFG_33MHZ   0x1841
#define WD95_PIO_CFG_25MHZ   0xd4
#define WD95_PIO_CFG_33MHZ   0x318

/* 
 * Parallel control
 */
#define PAR_OFFSET	0x8e000			/* offset to par regs */
#define PAR_BC_ADDR	0x1fb800a8		/* byte count and IE (w) */
#define PAR_CBP_ADDR    (HPC3_PBUS_BP(7))	/* current buf ptr (w) */
#define PAR_NBDP_ADDR   (HPC3_PBUS_DP(7))	/* next buf desc (w) */
#define PAR_CTRL_ADDR   IP22BOFF(HPC3_PAR_CONTROL)	/* control reg (w) */
#define PAR_SR_ADDR     IP22BOFF(HPC3_PAR_STAT)	/* status/remote reg (b) */

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
 * DUART Registers (on pbus)
 */
#define CHNA_DATA_OFFSET    PHYS_TO_K1(IP22BOFF(HPC3_SERIAL0_DATA))
#define CHNA_CNTRL_OFFSET   PHYS_TO_K1(IP22BOFF(HPC3_SERIAL0_CMD))
#define CHNB_DATA_OFFSET    PHYS_TO_K1(IP22BOFF(HPC3_SERIAL1_DATA))
#define CHNB_CNTRL_OFFSET   PHYS_TO_K1(IP22BOFF(HPC3_SERIAL1_CMD))

#define DUART1A_DATA    CHNA_DATA_OFFSET
#define DUART1B_DATA    CHNB_DATA_OFFSET
#define DUART1A_CNTRL   CHNA_CNTRL_OFFSET
#define DUART1B_CNTRL   CHNB_CNTRL_OFFSET

/* 
 * Battery backup (real time) clock.
 */
#ifdef LANGUAGE_C
#define RT_CLOCK_ADDR (struct ds1286_clk *)PHYS_TO_COMPATK1(HPC3_PBUS_RTC_1286)
#else 
#define RT_CLOCK_ADDR	PHYS_TO_K1(HPC3_PBUS_RTC_1286)
#endif
#define RTC_RAM_ADDR(off)	IP22BOFF(HPC3_PBUS_RTC_1286+DS1286_RAMOFFSET+ \
					 (4*(off)))
#define RT_RAM_FLAGS		0
#define RT_FLAGS_INVALID	0x01
#define IP26_GCACHE_SIZE	1		/* IP26 GCache size */
#define IP26_GCACHE_SIZE_ADDR	RTC_RAM_ADDR(IP26_GCACHE_SIZE)
#define	MC_IN_RESET		2		/* reseting the system */

/*
 * Define start of the NVRAM on dallas clock.  Used by IP24 as main NVRAM.
 */
#define	NVRAM_DATA(offset)	((ds1386_clk_t *)RT_CLOCK_ADDR)->nvram[offset]
#define	NVRAM_ADRS(offset)	&NVRAM_DATA(offset)


/* 
 * CPU aux control register (HPC3).  Used for NVRAM control on IP22.
 */
#define	CPU_AUX_CONTROL	IP22BOFF(HPC3_EEPROM_ADDR)
#define SER_TO_CPU	0x10		/* Data from serial memory */
#define CPU_TO_SER	0x08		/* Data to serial memory */
#define SERCLK		0x04		/* Serial Clock */
#define	CONSOLE_CS	0x02		/* EEPROM (nvram) chip select */
#define	NVRAM_PRE	0x01		/* EEPROM (nvram) PRE pin signal */


/*
 * EISA bitmasks used for keyboard bell on IP22 system (regs defined
 * in "sys/eisa.h").
 */
#define EISA_CTL_RG	0x04		/* Mode 2: rate generator */
#define EISA_CTL_SQ	0x06		/* Mode 3: square wave */
#define EISA_CTL_LM	0x30		/* R/W LSB then MSB */
#define EISA_CTL_C1	0x40		/* Select counter 1 */
#define EISA_CTL_C2	0x80		/* Select counter 2 */
#define EISA_C2_ENB	0x01		/* NMI STAT Counter 2 Enable */
#define EISA_SPK_ENB	0x02		/* NMI STAT Speaker Enable */

/*
 * EISA IP22 specific defines
 */
#define EISAIO_TO_PHYS(reg)	(0x00080000|reg)
#define EISAIO_TO_K1(reg)	PHYS_TO_K1(EISAIO_TO_PHYS(reg))
#define EISAIO_TO_COMPATK1(reg)	PHYS_TO_COMPATK1(EISAIO_TO_PHYS(reg))
#define EISAMEM_TO_PHYS(reg)	(reg)
#define EISAMEM_TO_K1(reg)	PHYS_TO_K1(reg)

#define EIU_MODE_REG		0x0009ffc0	/* modes: swap, addr inc, etc */
#define EIU_STAT_REG		0x0009ffc4	/* status: error type */
#define EIU_PREMPT_REG		0x0009ffc8	/* cntdown to preempt master */
#define EIU_QUIET_REG		0x0009ffcc	/* cntdown to request bus */
#define EIU_INTRPT_ACK		0x00090004	/* interrupt ack addr */

/* see the #definition of HSCXREG in comm/isdn/kern/sys/isdnchan.h */
#define HSCX_VSTR_B1REG	0xbfbd90bb
#define HSCX_VSTR_B2REG	0xbfbd91bb

/*
 *  Vino asic physical base addresses
 */
#define VINO_PHYS_BASE1		0x00080000	/* vino is in EISA address space */
#define VINO_CHIP_ID		0xB
#define VINO_REV_NUM(reg)	((reg) & 0x0F)
#define VINO_ID_VALUE(reg)	(((reg) & 0xF0) >> 4)

#ifdef LANGUAGE_C

/* chip interface structure for IP22 / HPC / WD33C93 */
typedef struct scuzzy {
	volatile unsigned char	*d_addr;	/* address register */
	volatile unsigned char	*d_data;	/* data register */
	volatile unsigned int 	*d_ctrl;	/* control address */
	volatile unsigned int	*d_bcnt;	/* byte count register */
	volatile unsigned int	*d_curbp;	/* current buffer pointer */
	volatile unsigned int	*d_nextbp;	/* next buffer pointer */
	volatile unsigned int	*d_dmacfg;	/* fifo pointer register */
	volatile unsigned int	*d_piocfg;	/* fifo data register */
	unsigned char d_initflags;	/* initial flags for d_flags */
	unsigned char d_clock;	/* value for clock register on WD chip */
#if IP22			/* For Indy GIO SCSI board */
	unsigned char d_reset;	/* reset code for HPC SCSI port */
	unsigned char d_flush;	/* fifo flush code for HPC SCSI port */
	unsigned char d_busy;	/* busy code for HPC SCSI port */
	unsigned char d_rstart;	/* read start code for HPC SCSI port */
	unsigned char d_wstart;	/* write start code for HPC SCSI port */
	unsigned char d_cbpoff;	/* offset of cbp value in dma descr */
	unsigned char d_cntoff;	/* offset of bcnt value in dma descr */
	unsigned char d_nbpoff;	/* offset of nbp value in dma descr */
#endif
	unsigned char d_vector;	/* hardware interrupt vector */
} scuzzy_t;

/* Map up to 256Kb per transfer.  This is sufficient
 * for almost all disk and tape acesses.  Things like scanners or
 * printers often transfer far more than a Mb per cmd, so they will
 * have to re-map anyway, no matter how many we allocate.
 */
#define NSCSI_DMA_PGS 64

/* this saves a lot of hard to read and type code in various places,
	and also ensures that we get the register size correct */
#define K1_LIO_0_MASK_ADDR \
	((volatile unchar *)PHYS_TO_COMPATK1(LIO_0_MASK_ADDR))
#define K1_LIO_1_MASK_ADDR \
	((volatile unchar *)PHYS_TO_COMPATK1(LIO_1_MASK_ADDR))
#define K1_LIO_2_MASK_ADDR \
	((volatile unchar *)PHYS_TO_COMPATK1(LIO_2_MASK_ADDR))
#define K1_LIO_3_MASK_ADDR \
	((volatile unchar *)PHYS_TO_COMPATK1(LIO_3_MASK_ADDR))
#define K1_LIO_0_ISR_ADDR \
	((volatile unchar *)PHYS_TO_COMPATK1(LIO_0_ISR_ADDR))
#define K1_LIO_1_ISR_ADDR \
	((volatile unchar *)PHYS_TO_COMPATK1(LIO_1_ISR_ADDR))
#define K1_LIO_2_ISR_ADDR \
	((volatile unchar *)PHYS_TO_COMPATK1(LIO_2_3_ISR_ADDR))

/* function prototypes for IP22 functions */

extern int spl1(void);
extern int splnet(void);
extern int spl3(void);
extern int splgio2(void);
extern int spllintr(void);
extern int splhintr(void);
extern int splretr(void);

#if defined(_KERNEL) && !defined(_STANDALONE)
extern void sethpcdmaintr(int,void (*)(__psint_t,struct eframe_s *),__psint_t);
extern void setgiogfxvec(const uint, lcl_intr_func_t *, int);
extern void hpcdma_intr(__psint_t, struct eframe_s *);
void kbdbell_init(void);
int pckbd_bell(int, int, int);
void eisa_refresh_on(void);
void eisa_init(void);
#endif

#ifdef R4600SC
void _r4600sc_enable_scache(void);
int _r4600sc_disable_scache(void);
#endif

extern int is_fullhouse(void),is_ioc1(void),board_rev(void),is_indyelan(void);
int ip22_addr_to_bank(unsigned int);

#endif /* LANGUAGE_C */

#if defined(_STANDALONE) || defined(PROM) || defined(LOCORE)
#include <sys/IP22nvram.h>

/* use:  IS_FULLHOUSE(tmpreg); bnez tmpreg, yes_it_is_an_fullhouse */
#if !defined(IS_IOC1)
#define	IS_FULLHOUSE(t)				\
	lw	t, HPC3_SYS_ID|K1BASE;		\
	andi	t, BOARD_IP22

/* use:  IS_IOC1(tmpreg); bnez tmpreg, yes_it_is_an_ioc1 */
#define	IS_IOC1(t)				\
	lw	t, HPC3_SYS_ID|K1BASE;		\
	andi	t, CHIP_IOC1;
#endif

#if IP22
#if _MIPS_SIM == _ABI64
#define SR_PROMBASE     SR_CU0|SR_CU1|SR_KX
#else
#define SR_PROMBASE     SR_CU0|SR_CU1
#endif
#elif IP28
#define SR_PROMBASE     SR_CU0|SR_CU1|SR_FR|SR_KX|SR_IE|SR_BERRBIT
#endif

#endif /* _STANDALONE */

/*
 * Commands is used by Guinness's front panel buttons
 * to drive the audio driver for volume level control.
 */
#define VOLUMEINC	0x01
#define VOLUMEDEC	0x02
#define VOLUMEMUTE	0x03

#if IP26 || IP28
/*
 * 'IP26' ECC baseboard, which works with IP26 or IP28 CPUs.
 */
#define ECC_CTRL_BASE	0x60000000	/* Configure SIMM bank 4 here for ECC
					   control registers on IP26 */
#define ECC_CTRL_REG	0x60000000	/* Address of control register. */
#define	ECC_MEMCFG	(0x2000|(ECC_CTRL_BASE>>24))

/* Values for IDT 49C466 ECC parts */
#define ECC_ECC_FLOW		0x2	/* Error detection, no correction */
#define	ECC_ECC_NORMAL		0x3	/* Normal ECC calc mode */

/* Values for ECC board control register */
#define	ECC_CTRL_ENABLE		0x00000	/* "Normal mode"  Enable normal ecc */
#define	ECC_CTRL_DISABLE	0x10000	/* "Slow mode" Allow uncached writes */
#define ECC_CTRL_WR_ECC		0x20000	/* Value in low bits goes to 466 parts*/
#define ECC_CTRL_CLR_INT	0x30000	/* Clear pending interrupts */
#define ECC_CTRL_CHK_ON		0x50000	/* IP28 Enable ECC error generation */
#define ECC_CTRL_CHK_OFF	0x60000	/* IP28 Disables ECC error generation */

#define ECC_DEFAULT	(ECC_ECC_NORMAL | ECC_CTRL_WR_ECC)
#define ECC_FLOWTHRU	(ECC_ECC_FLOW	| ECC_CTRL_WR_ECC)

#ifdef IP28			/* only on IP28 motherboards */
#define	ECC_STATUS		HPC3_READ
#define	K1_ECC_STATUS		((volatile uint *)PHYS_TO_COMPATK1(HPC3_READ))
#define ECC_STATUS_BANK		0x0300	/* SIMM bank for Multi-bit error */
#define ECC_BANK_SHIFT		8	/* SIMM bank shift value */
#define ECC_STATUS_LOW		0x0400	/* SIMM bank low side */
#define ECC_STATUS_HIGH		0x0800	/* SIMM bank high side */
#define ECC_STATUS_UC_WR	0x1000	/* Uncached write or Multi-bit err */
#define ECC_STATUS_GIO		0x2000	/* Multi-bit err on GIO or CPU side */
#define ECC_STATUS_SSRAMx36	0x4000	/* SSRAM is x36 or x18 */
#define ECC_STATUS_MASK		0xff00	/* Bits to examine for ECC status */
#endif

#define IP26_UC_WRITE_MSG	\
"IRIX detected illegal write to uncached memory.  This indicates\n\
presence of kernel device driver which makes use of uncached\n\
memory without using ip26_enable_ucmem().  See /usr/include/sys/IP26.h\n\
for more details."

/* definitions for egun (bad ECC generator) */
#define EGUN_MODE	1		/* Switch to slow mode, and back */
#define EGUN_UC_WRITE	2		/* Do fast-mode uncached write */
#define EGUN_UC_WRITE_I	3		/* Same, but have err intr ignore it*/
#define EGUN_1BIT_INT	4		/* Call ip26_ecc_1bit_intr() */
#define EGUN_UC_WRITE_D	5		/* Fast mode uncached write (IP28) */

/* Settings of CPU_ERR_STAT/GIO_ERR_STAT bits with ECC board.
 */
#define ECC_FAST_UC_WRITE	0x01	/* Byte lane 0->uc write in fast mode */
#define ECC_MULTI_BIT_ERR	0x02	/* Lane 1 -> multi-bit ECC error */
#define ECC_MUST_BE_ZERO	0xfc	/* Rest of byte lanes should be 0 */

/* Macro to determine if baseboard has ECC or parity.
 * Don't make this a macro for kernels, since it's used by the cachectl()
 * syscall.
 */
#define IP26P_ECCSYSID	0x12		/* IP26 + fast ECC parts board */
#define IP26_ECCSYSID	0x18		/* normal IP26 board */
#define IP28_ECCSYSID	0x19		/* real IP28 board */

#if _STANDALONE
#if IP26
#define ip26_isecc()	(ip22_board_rev() >= (IP26_ECCSYSID>>BOARD_REV_SHIFT))
#else
#define ip26_isecc()	1	/* IP28 should always have ECC baseboard */
#endif

#endif /* _STANDALONE */

#if _LANGUAGE_C
/* IP26 ECC baseboard mode switch prototypes */
long ip26_enable_ucmem(void);			/* exported */
void ip26_return_ucmem(long previous);		/* exported */
long ip26_disable_ucmem(void);			/* internal */
/* IP28 ECC baseboard mode switch prototypes */
long ip28_enable_ucmem(void);                   /* exported */
void ip28_return_ucmem(long previous);          /* exported */
long ip28_disable_ucmem(void);                  /* internal */
#if IP28
#ifdef _STANDALONE
unsigned long ip28_ssram_swizzle(unsigned long data);
char *ip28_ssram(void *, unsigned long, unsigned long);
#endif
/* flavor of system board for IP28 */
int is_ip26bb(void);
int is_ip26pbb(void);
#define is_ip28bb()	(!is_ip26bb())
void ip28_enable_eccerr(void);
void ip28_disable_eccerr(void);
void ip28_ecc_error(void);
void ip28_ecc_flowthru(void);
void ip28_ecc_correct(void);
#endif

void __dcache_line_wb_inval(void *);
void __dcache_inval(void *, int);
#ifdef R10000_SPECULATION_WAR
extern int spec_udma_war_on;
int specwar_fast_userdma(caddr_t, size_t, int *);
void specwar_fast_undma(caddr_t, size_t, int *);
#endif
#endif

/* MC settings for normal/slow ECC mode (IP28 turns on alias movement) */
#ifdef SEG1_BASE
#define CPU_MEMACC_BIGALIAS	0
#else
#define CPU_MEMACC_BIGALIAS	MEMACC_BIGALIAS
#endif
#define CPU_MEMACC_NORMAL_IP26	(0x11453444|CPU_MEMACC_BIGALIAS)
#define CPU_MEMACC_SLOW_IP26	(0x11453446|CPU_MEMACC_BIGALIAS)
#ifdef IP28
#define CPU_MEMACC_NORMAL	(0x11453434|CPU_MEMACC_BIGALIAS)
#define CPU_MEMACC_SLOW		(0x11453436|CPU_MEMACC_BIGALIAS)
#else
#define CPU_MEMACC_NORMAL	CPU_MEMACC_NORMAL_IP26
#define CPU_MEMACC_SLOW		CPU_MEMACC_SLOW_IP26
#endif
#endif	/* IP26 || IP28 */

/*
 * The following is a mechanism to share GIO_SLOT_0 between two boards on
 * the Indigo2 at GIO_INTERRUPT_1.
 *
 * Using setgiovector() with GIO_INTERRUPT_1 the ISRs for the boards
 * cannot be distinguished.  A fanout function is introduced that will
 * invoke both of the registered ISRs.
 *
 * Both drivers must register using setgiosharedvector().  Also, there
 * is no way for a probe routine to determine which of the physical
 * slots the board resides (GIO_SLOT_0 is it), so whether the upper or
 * lower index is selected must be hardcoded.
 *
 * This sharing arrangement was born of necessity for the Galileo 1.5
 * (Impact Video) video board which can share the slot with either the
 * Cosmo2 (Impact Compression) board or the Ciprico UltraScsi adapter.
 *
 * Impact Video is assigned GIO_SLOT_0_LOWER, as it must be physically
 * close to graphics in order to be joined to it with flex cables.
 * All other boards must choose GIO_SLOT_0_UPPER.  This will still
 * function properly even if the Impact Video board is not present,
 * and the other board is actually plugged into the lower slot.
 */

#define GIO_SLOT_0_UPPER 1	/* Register ISR for phyiscally uppermost slot */
#define GIO_SLOT_0_LOWER 0	/* Register ISR for phyiscally lowermost slot */

#if defined(_KERNEL) && !defined(_STANDALONE) && defined(LANGUAGE_C)

/*
 *  Register an ISR for the shared slot (GIO_SLOT_0) at GIO_INTERRUPT_1.
 *
 *  position		GIO_SLOT_0_LOWER or GIO_SLOT_0_UPPER
 *  func		Pointer to ISR, NULL to de-register
 *  arg			Value to be provided to ISR when invoked.
 */
extern void
setgiosharedvector(int position, void (*func)(__psint_t, struct eframe_s *),
 __psint_t arg);

#endif /* LANGUAGE_C _KERNEL !_STANDALONE*/

#endif /* __SYS_IP22_H__ */
