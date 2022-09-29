/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident "sys/hpc3.h: $Revision: 1.49 $"

/*
  All HPC3 registers are 32 bits wide.
  hpc3.h is split in two parts. Part one (top part) contains register 
  base addresses and macros to be used by both "C" and assembly code.
  Part two contains structures for most hpc3 registers as well as macros 
  for thier base addresses to be used by all "C" files.
  Note: Registers with write_only bits were not assigned a structure.
	This was done to avoid writing garbage into them.
*/

#ifndef __SYS_HPC3_H__
#define	__SYS_HPC3_H__
/*******************************************************************
 * GIO64 Bus Interface Control Registers                           *
 *******************************************************************/ 

/* 
 * Interrupt status register's address.
 */
#define HPC3_INTSTAT_ADDR	0x1fbb0000 

/* 
 * Miscellaneous collection bit's address.
 */
#define HPC3_MISC_ADDR		0x1fbb0004 
#define HPC3_EN_REAL_TIME	0x1	/* Enable real time feature */
#define HPC3_DES_ENDIAN		0x2	/* 1=little endian */

/* 
 * serial eeprom data register's base address.
 */
#define HPC3_EEPROM_ADDR		0x1fbb0008 

/* 
 * Bus error interrupt status's address.
 */
#define HPC3_BUSERR_STAT_ADDR		0x1fbb0010 


/**************************************************************** 
 * ETHERNET Registers						*
 ****************************************************************/ 

/* 
 * Ethernet buffer structure's address for the receiver channel.
 */
#define HPC3_ETHER_RX_CBP		0x1fb94000

/* Ethernet next buffer descriptor pointer base address for the 
 * receiver channel.
 */
#define HPC3_ETHER_RX_NBDP		0x1fb94004      


/* Ethernet byte count info base address for the receiver channel.
 */
#define HPC3_ETHER_RX_BC_ADDR		0x1fb95000      

/* 
 * Ethernet control base address for the receiver channel.
 */
#define HPC3_ETHER_RX_CNTRL_ADDR	0x1fb95004      
/* Bit definitions for  Ethernet control receiver register */
#define RXRBO				0x0800		/* bit 11 */
#define RXCH_ACTIVE_MASK		0x0400		/* bit 10 */
#define RXCH_ACTIVE			0x0200		/* bit 9 */
#define RXENDIAN			0x0100		/* bit 8 */
#define RXSTATUS_7			0x0080		/* bit 7 */
#define RXSTATUS_6			0x0040		/* bit 6 */
#define RXSTATUS_5_0			0x003f		/* bit 5:0 */


/* 
 * Ethernet gio fifo pointer base address for the receiver channel.
 */
#define HPC3_ETHER_RX_GIO		0x1fb95008

/* Ethernet device fifo pointer base address for the receiver channel.
 */
#define HPC3_ETHER_RX_DEV		0x1fb9500c

/* 
 * Ethernet miscellaneous bits base address.
 */
#define HPC3_ETHER_MISC_ADDR		0x1fb95014	

/* 
 * Ethernet dma configuration base address.
 */
#define HPC3_ETHER_DMA_CFG_ADDR		0x1fb95018

/* 
 * Ethernet pio configuration base address.
 */
#define HPC3_ETHER_PIO_CFG_ADDR		0x1fb9501c

/* 
 * Ethernet buffer base address for the transmitter channel.
 */
#define HPC3_ETHER_TX_CBP		0x1fb96000	

/* 
 * Ethernet next buffer descriptor pointer base address for 
 * the transmitter channel.
 */
#define HPC3_ETHER_TX_NBDP		0x1fb96004      


/* 
 * Ethernet byte count info base address for 
 * the transmitter channel.
 */
#define HPC3_ETHER_TX_BC_ADDR		0x1fb97000      

/* 
 * Ethernet control base address for the transmitter channel.
 */
#define HPC3_ETHER_TX_CNTRL_ADDR	0x1fb97004      
/* Bit definitions for Ethernet control transmitter register */
#define	TXCH_ACTIVE_MASK		0x0400;		/* bit 10 */
#define	TXCH_ACTIVE			0x0200;		/* bit 9 */
#define TXENDIAN			0x0100;		/* bit 8 */
#define TXSTATUS_7_5			0x00e0;		/* bit 7:5 */
#define TXSTATUS_4			0x0010;		/* bit 4 */
#define TXSTATUS_3_0			0x000f;		/* bit 3:0 */

/* 
 * Ethernet gio fifo pointer base address for the transmitter channel.
 */
#define HPC3_ETHER_TX_GIO		0x1fb97008

/* 
 * Ethernet device fifo pointer base address for the transmitter channel.
 */
#define HPC3_ETHER_TX_DEV		0x1fb9700c

/*
 * ethernet channel, external register's base address
 */
#define HPC3_ETHER_REG 			0x1fbd4000

/*******************************************************************
 * SCSI Registers                                                  *
 *******************************************************************/ 

/* 
 * Determine scsi buffer base address  Where 0 =< x(channel) =< 1 
 */
#define HPC3_SCSI_BUFFER_PTR0		0x1fb90000 
#define HPC3_SCSI_BUFFER_PTR1		0x1fb92000
#define HPC3_SCSI_BUFFER_PTR_ADDR(x) \
				(x ? \
				HPC3_SCSI_BUFFER_PTR1 : HPC3_SCSI_BUFFER_PTR0)

/* 
 * scsi buffer base address  Where 0 =< x(channel) =< 1 
 */
#define HPC3_SCSI_BUFFER_NBDP0		0x1fb90004 
#define HPC3_SCSI_BUFFER_NBDP1		0x1fb92004
/* 
 * scsi byte count's base address  Where 0 =< x(channel) =< 1 
 */
#define HPC3_SCSI_BC0			0x1fb91000
#define HPC3_SCSI_BC1			0x1fb93000
#define HPC3_SCSI_BC_ADDR(x)	(x ? HPC3_SCSI_BC1 : HPC3_SCSI_BC0)
/* 
 * scsi control's base address  Where 0 =< x(channel) =< 1 
 */
#define HPC3_SCSI_CONTROL0		0x1fb91004
#define HPC3_SCSI_CONTROL1		0x1fb93004
#define HPC3_SCSI_CONTROL_ADDR(x) (x ? HPC3_SCSI_CONTROL1 : HPC3_SCSI_CONTROL0)
/* Byte definitions for scsi control register */
#define SCPARITY_ERR			0x80      /* bit 7 */
#define SCCH_RESET			0x40      /* bit 6 */
#define SCCH_ACTIVE_MASK		0x20      /* bit 5 */
#define SCCH_ACTIVE			0x10      /* bit 4 */
#define SCFLUSH				0x08      /* bit 3 */
#define SCDIROUT			0x04      /* bit 2 */
#define SCENDIAN			0x02      /* bit 1 */
#define SCINTERRUPT			0x01      /* bit 0 */

/* 
 * scsi channel 0,1's gio fifo ptr base address  
 */
#define HPC3_SCSI_GIO_FIFO0		0x1fb91008
#define HPC3_SCSI_GIO_FIFO1		0x1fb93008
/* 
 * scsi channel 0,1's device fifo ptr base address
 */
#define HPC3_SCSI_DEV_FIFO0		0x1fb9100c
#define HPC3_SCSI_DEV_FIFO1		0x1fb9300c
/* 
 * Determine scsi dma configuration's base address  Where 0 =< x(channel) =< 1 
 */
#define HPC3_SCSI_DMACFG0		0x1fb91010
#define HPC3_SCSI_DMACFG1		0x1fb93010
#define HPC3_SCSI_DMACFG_ADDR(x)\
			 	(x ? \
				HPC3_SCSI_DMACFG1 : HPC3_SCSI_DMACFG0)

/* scsi_dma_cfg register bit definitions are
   described in hpc3_scsi.c 
*/

/* 
 * Determine scsi pio configuration's address  Where 0 =< x(channel) =< 1 
 */
#define HPC3_SCSI_PIOCFG0		0x1fb91014
#define HPC3_SCSI_PIOCFG1		0x1fb93014
#define HPC3_SCSI_PIOCFG_ADDR(x)\
				(x ? \
				HPC3_SCSI_PIOCFG1 : HPC3_SCSI_PIOCFG0)

/* scsi_pio_cfg register bit definitions are
   described in hpc3_init.c
*/
	
/* 
 * scsi channel0, external register's base address
 */
#define HPC3_SCSI_REG0			0x1fbc0000
#define HPC3_SCSI_REG1			0x1fbc8000

/* scsi fifo base addresses -size=0x1fff each*/
#define HPC3_SCSI_FIFO0			0x1fba8000
#define HPC3_SCSI_FIFO1			0x1fbaa000

/*
 * Registers for second HPC3 (HPC31)
 */
#define HPC31_BASE			0x1fb00000

/* 
 * Determine scsi buffer base address  Where 0 =< x(channel) =< 1 
 */
#define HPC31_SCSI_BUFFER_PTR0		(HPC31_BASE + 0x10000)
#define HPC31_SCSI_BUFFER_PTR1		(HPC31_BASE + 0x12000)
#define HPC31_SCSI_BUFFER_PTR_ADDR(x) \
				(x ? \
				HPC31_SCSI_BUFFER_PTR1 : HPC31_SCSI_BUFFER_PTR0)

/* 
 * scsi buffer base address  Where 0 =< x(channel) =< 1 
 */
#define HPC31_SCSI_BUFFER_NBDP0		(HPC31_BASE + 0x10004)
#define HPC31_SCSI_BUFFER_NBDP1		(HPC31_BASE + 0x12004)
/* 
 * scsi byte count's base address  Where 0 =< x(channel) =< 1 
 */
#define HPC31_SCSI_BC0			(HPC31_BASE + 0x11000)
#define HPC31_SCSI_BC1			(HPC31_BASE + 0x13000)
#define HPC31_SCSI_BC_ADDR(x)	(x ? HPC31_SCSI_BC1 : HPC31_SCSI_BC0)
/* 
 * scsi control's base address  Where 0 =< x(channel) =< 1 
 */
#define HPC31_SCSI_CONTROL0		(HPC31_BASE + 0x11004)
#define HPC31_SCSI_CONTROL1		(HPC31_BASE + 0x13004)
#define HPC31_SCSI_CONTROL_ADDR(x) (x ? HPC31_SCSI_CONTROL1 : HPC31_SCSI_CONTROL0)

/* 
 * scsi channel 0,1's gio fifo ptr base address  
 */
#define HPC31_SCSI_GIO_FIFO0		(HPC31_BASE + 0x11008)
#define HPC31_SCSI_GIO_FIFO1		(HPC31_BASE + 0x13008)
/* 
 * scsi channel 0,1's device fifo ptr base address
 */
#define HPC31_SCSI_DEV_FIFO0		(HPC31_BASE + 0x1100c)
#define HPC31_SCSI_DEV_FIFO1		(HPC31_BASE + 0x1300c)
/* 
 * Determine scsi dma configuration's base address  Where 0 =< x(channel) =< 1 
 */
#define HPC31_SCSI_DMACFG0		(HPC31_BASE + 0x11010)
#define HPC31_SCSI_DMACFG1		(HPC31_BASE + 0x13010)
#define HPC31_SCSI_DMACFG_ADDR(x)\
			 	(x ? \
				HPC31_SCSI_DMACFG1 : HPC31_SCSI_DMACFG0)
/* 
 * Determine scsi pio configuration's address  Where 0 =< x(channel) =< 1 
 */
#define HPC31_SCSI_PIOCFG0		(HPC31_BASE + 0x11014)
#define HPC31_SCSI_PIOCFG1		(HPC31_BASE + 0x13014)
#define HPC31_SCSI_PIOCFG_ADDR(x)\
				(x ? \
				HPC31_SCSI_PIOCFG1 : HPC31_SCSI_PIOCFG0)
/* 
 * scsi channel0, external register's base address
 */
#define HPC31_SCSI_REG0			(HPC31_BASE + 0x40000)
#define HPC31_SCSI_REG1			(HPC31_BASE + 0x48000)
/*
 * scsi fifo base addresses -size=0x1fff each
 */
#define HPC31_SCSI_FIFO0		(HPC31_BASE + 0x28000)
#define HPC31_SCSI_FIFO1		(HPC31_BASE + 0x2a000)

/*
 * Address of interrupt/config register on Challenge S SCSI/E-net board.
 */
#define HPC31_INTRCFG			(HPC31_BASE + 0x58000)

/* 
 * Second HPC Ethernet miscellaneous bits base address.
 */
#define HPC31_ETHER_MISC_ADDR		(HPC31_BASE + 0x15014)

/*
 * Second HPC PBUS PIO Config 0
 */
#define HPC31_PBUS_PIO_CFG_0		(HPC31_BASE + 0x5d000)


/******************************************************************* 
 * PBUS Registers                                                  *
 *******************************************************************/

/* pbus fifo base addresses -size=0x0x7fff*/
#define HPC3_PBUS_FIFO			0x1fba0000

/* 
 * Addresses of audio subsystem registers dedicated to 
 * PBUS channels (0,3). 
 */
#define HPC3_AUDIO_SUB0			0x1fbd8000	/* 0,0x00 */
#define HPC3_AUDIO_SUB1			0x1fbd8400	/* 1,0x00 */
#define HPC3_AUDIO_SUB2			0x1fbd8800	/* 2,0x00 */
#define HPC3_AUDIO_SUB3			0x1fbd8c00	/* 3,0x00 */
/* 
 * Address of INT2/INT3 register dedicated to
 *      PBUS channel four(4,0x00) - IP22 (int2)
 *      PBUS channel six (6,0x20) - IP24 (int3)
 */
#if IP26 || IP28
#define	HPC3_INT_ADDR	HPC3_INT2_ADDR
#endif
#if IP22 && _LANGUAGE_C
#define	HPC3_INT_ADDR	(is_ioc1()? HPC3_INT3_ADDR : HPC3_INT2_ADDR)
#endif
#define HPC3_INT2_ADDR		0x1fbd9000	/* 4,0x00 XXX - IP22 */
#define HPC3_INT3_ADDR		0x1fbd9880	/* 6,0x20 XXX - IP24 */

/* 
 * Address of Full House's extended register.  PX at 0x1fbd9400.  (IP22 Only)
 */
#define HPC3_EXT_IO_ADDR		0x1fbd9900	/* 6,0x40 */

#define EXTIO_S0_IRQ_3			0x8000		/* S0: vid.vsync */
#define EXTIO_S0_IRQ_2			0x4000		/* S0: gfx.fifofull */
#define EXTIO_S0_IRQ_1			0x2000		/* S0: gfx.int */
#define EXTIO_S0_RETRACE		0x1000
#define EXTIO_SG_IRQ_3			0x0800		/* SG: vid.vsync */
#define EXTIO_SG_IRQ_2			0x0400		/* SG: gfx.fifofull */
#define EXTIO_SG_IRQ_1			0x0200		/* SG: gfx.int */
#define EXTIO_SG_RETRACE		0x0100
#define EXTIO_GIO_33MHZ			0x0080
#define EXTIO_EISA_BUSERR		0x0040
#define EXTIO_MC_BUSERR			0x0020
#define EXTIO_HPC3_BUSERR		0x0010
#define EXTIO_S0_STAT_1			0x0008
#define EXTIO_S0_STAT_0			0x0004
#define EXTIO_SG_STAT_1			0x0002
#define EXTIO_SG_STAT_0			0x0001

/*  IP22-006 splits EXTIO into two registers to support 3rd gio slot.
 */
#define EXTIO_S1_IRQ_3			0x0008		/* s2: vid.vsync */
#define EXTIO_S1_IRQ_2			0x0004		/* s2: gfx.fifofull */
#define EXTIO_S1_IRQ_1			0x0002		/* s2: gfx.int */
#define EXTIO_S1_RETRACE		0x0001
#define EXTIO_S1_STAT_1			0x0010
#define EXTIO_S1_STAT_0			0x0020

/* 
 * Addresses of 32 internal IO registers dedicated to PBUS channel six(6).
 * This are contained in the IOC1 chip on the IP24 board.
 */
#define HPC3_PAR_DATA			0x1fbd9800	/* 6,0x00 */
#define HPC3_PAR_CONTROL		0x1fbd9804	/* 6,0x01 */
#define HPC3_PAR_STAT			0x1fbd9808	/* 6,0x02 */
#define HPC3_PAR_DMA_CONTROL		0x1fbd980c	/* 6,0x03 */
#define HPC3_PAR_INT_STAT		0x1fbd9810	/* 6,0x04 */
#define HPC3_PAR_INT_MASK		0x1fbd9814	/* 6,0x05 */
#define HPC3_PAR_TIMER1			0x1fbd9818	/* 6,0x06 */
#define HPC3_PAR_TIMER2			0x1fbd981c	/* 6,0x07 */
#define HPC3_PAR_TIMER3			0x1fbd9820	/* 6,0x08 */
#define HPC3_PAR_TIMER4			0x1fbd9824	/* 6,0x09 */
#define HPC3_SERIAL1_CMD		0x1fbd9830	/* 6,0x0c */
#define HPC3_SERIAL1_DATA		0x1fbd9834	/* 6,0x0d */
#define HPC3_SERIAL0_CMD		0x1fbd9838	/* 6,0x0e */
#define HPC3_SERIAL0_DATA		0x1fbd983c	/* 6,0x0f */
#define HPC3_KBD_MOUSE0			0x1fbd9840	/* 6,0x10 */
#define HPC3_KBD_MOUSE1			0x1fbd9844	/* 6,0x11 */
#define HPC3_GC_SELECT			0x1fbd9848	/* 6,0x12 */
#define HPC3_GEN_CONTROL		0x1fbd984c	/* 6,0x13 */
#define HPC3_PANEL			0x1fbd9850	/* 6,0x14 */
#define HPC3_SYS_ID			0x1fbd9858	/* 6,0x16 */
#define HPC3_READ			0x1fbd9860	/* 6,0x18 */
#define	HPC3_DMA_SELECT			0x1fbd9868	/* 6,0x1a */
#define HPC3_WRITE1			0x1fbd9870	/* 6,0x1c IP22 */
#define HPC3_RESET			HPC3_WRITE1	/* (IP24) */
#define HPC3_WRITE2			0x1fbd9878	/* 6,0x1e IP22 */
#define HPC3_WRITE			HPC3_WRITE2	/* (IP24) */

/* 0x1fbd9880 .. 0x1fbd98ac are INT3 addresses - see sys/IP22.h */

#define	HPC3_INT3_MAP_POLARITY		0x1fbd989c	/* 6,0x27 */
#define	HPC3_INT3_TIMER_CLEAR		0x1fbd98a0	/* 6,0x28 */
#define	HPC3_INT3_ERROR_STATUS		0x1fbd98a4	/* 6,0x29 */
#define	HPC3_INT3_TIMER_COUNTER_0	0x1fbd98b0	/* 6,0x2c */
#define	HPC3_INT3_TIMER_COUNTER_1	0x1fbd98b4	/* 6,0x2d */
#define	HPC3_INT3_TIMER_COUNTER_2	0x1fbd98b8	/* 6,0x2e */
#define	HPC3_INT3_TIMER_CONTROL		0x1fbd98bc	/* 6,0x2f */

/* Error Status Bits (IP24 only) */
/* #define HPC3_ERRSTAT_EISA_BUSERR	0x0001		   Not Used */
#define HPC3_ERRSTAT_MC_BUSERR		0x0002
#define HPC3_ERRSTAT_HPC3_BUSERR	0x0004

/* INT3 MAP Polarity Bits
 * 0 = active low, 1 = active high
 */
#define	HPC3_MP_GRX_VERTSTAT		0x01
#define	HPC3_MP_PWD_DISABLE		0x02	/* passwd disable */
#define	HPC3_MP_ISDN_CONN_STAT		0x04
						/* 0x08, unused */
#define	HPC3_MP_KYBD_MSE		0x10
#define	HPC3_MP_SERIAL_PORTS		0x20
#define	HPC3_MP_GIO_EXP0_INT		0x40
#define	HPC3_MP_GIO_EXP1_INT		0x80

/* default initial value
 * We invert the pwd disable bit to be compatible with fullhouse
 */
#define	HPC3_MP_INIT			HPC3_MP_PWD_DISABLE

/* General Control register bits
 */ 
					/* 7..4 not used */
#define	GC_GIO_33MHZ		0x08	/*(in) gio speed hi=33MHz,lo=25   */
#define	GC_AUDIO_FT_N		0x04	/*(out) hal2 fast timer input     */
#define	GC_ISDN_TX_SEL		0x02	/*(out) ISDN TX select, lo=A hi=B */
#define	GC_GRX_EXPR_CLR		0x01	/*(out) clear EXPRESS interrupt   */

#define	GC_SELECT_INIT		0x07	/* select 7..3 input, 2..0 output */

/* bit definitions for internal IO registers defined above 
 */
/* POWER MANAGEMENT BITS (HPC3_PANEL) */
#define	PANEL_VOLUME_UP_ACTIVE		0x80 /* 0=>Volume UP is depressed */
#define	PANEL_VOLUME_UP_INT		0x40 /* 0=>Volume UP interrupt */
#define	PANEL_VOLUME_DOWN_ACTIVE	0x20 /* 0=>Volume DOWN is depressed */
#define	PANEL_VOLUME_DOWN_INT		0x10 /* 0=>Volume DOWN interrupt */
					     /* 0x08, 0x04 unused */
#define POWER_INT			0x02  /* Power interrupt */
#define POWER_SUP_INHIBIT		0x01  /* Low=supply off,Hi=supply on */

/* Current power on bits for guinness.  Extra bits are don't care
 * on fullhouse.
 */
#define	POWER_ON	(POWER_INT|POWER_SUP_INHIBIT|ALL_VOLUME_BUTTONS)
#define	ALL_VOLUME_BUTTONS	(PANEL_VOLUME_UP_ACTIVE|PANEL_VOLUME_UP_INT|PANEL_VOLUME_DOWN_ACTIVE|PANEL_VOLUME_DOWN_INT)

/* SYSTEM ID BITS (HPC3_SYS_ID) */
#define CHIP_REV_MASK			0xe0  /* Chip Revision */
#define	CHIP_REV_SHIFT			5
#define BOARD_REV_MASK			0x1e  /* Board Revision */
#define	BOARD_REV_SHIFT			1
#define	BOARD_ID_MASK			0x01  /* 1 = fullhouse, 0 = guinness */

#define BOARD_IP24			0
#define BOARD_IP22			1
#define CHIP_IOC1			0x20

/* READ (HPC3_READ) */
#define ETHERNET_LINK			0x80  /* low=TP link,Hi=no link */
#define ETHER_PWR_STAT			0x40  /* low=no power,hi=+12V power */
#define SCSI1_PWR_STAT			0x20  /* low=no pwr,hi=+5V pwr/FH */
#define SCSI0_PWR_STAT			0x10  /* low=no power,hi=+5V power */
#define UART1_DSR			0x08  /* Data Set Ready */
#define UART0_DSR			0x04
#define UART1_RI			0x02  /* Ring Indicator */
#define UART0_RI			0x01

/* WRITE1 (HPC3_WRITE1) (HPC_RESET on IOC1) */
#define PAR_RESET		0x01  /* low=reset parallel ,hi=norm */
#define KBD_MS_RESET		0x02  /* low=reset keybd/ms,hi=norm */
#define EISA_RESET		0x04  /* low=reset EISA,  hi=normal (IP22) */
#define VIDEO_RESET		0x04  /* low=reset VIDEO, hi=normal (IP24) */
#define ISDN_RESET		0x08  /* low=reset ISDN,  hi=normal (IP24) */
#define SPECIAL_GIO_RESET	0x08  /* for modified IP22 systems only; */
				      /*   noop on regular IP22s */
#define LED_GREEN		0x10  /* turn LED0 green off */
#define LED_AMBER		0x20  /* turn LED0 amber off */
#define LED_RED_OFF		0x10  /* turn red off (guinness) */
#define LED_GREEN_OFF		0x20  /* turn green off (guinness) */

/* DMA_SELECT */
				      /* bits 7:6, unused */
#define	SERIAL_CLOCK_10MHZ	0x00  /* bits 5:4 = 00: 10 Mhz */
#define	SERIAL_CLOCK_6_67MHZ	0x10  /* = 01: 6.67 Mhz */
#define	SERIAL_CLOCK_EXT	0x20  /* = 10: (or 11): external clock */
				      /* bit 3, unused */
#define	DMA_SELECT_PARALLEL	0x04  /* bit 2, low=off, high=enable PAR DMA */
#define	DMA_SELECT_ISDN_B	0x02  /* bit 1, low=off, high=enable ISDN B */
#define	DMA_SELECT_ISDN_A	0x01  /* bit 0, low=off, high=enable ISDN A */

#define	DMA_SELECT_INIT	(DMA_SELECT_PARALLEL|DMA_SELECT_ISDN_B|\
			DMA_SELECT_ISDN_A)

/* WRITE2 (HPC3_WRITE2) (HPC_WRITE on IOC1) */
#define MARGIN_HI			0x80  /* low=normal +5V op.,hi=5.25V */ 
#define MARGIN_LO			0x40  /* low=normal +5V op.,hi=4.75V */
#define UART1_ARC_MODE			0x20  /* low=RS422 MAC mode,hi=RS232 */
#define UART0_ARC_MODE			0x10  /* low=RS422 MAC MODE,hi=RS232 */
#define ETHER_AUTO_SEL			0x08  /* low=manual mode,hi=auto mode */
#define ETHER_PORT_SEL			0x04  /* low=TP,hi=AUI */
#define ETHER_UTP_STP			0x02  /* low=150ohm/TP,hi=100ohm/TP */
#define ETHER_NORM_THRESH		0x01  /* low=norm,hi=threshhold 4.5dB */

/*
 * Determine pbus pio configuration's base address  Where 0 =< x(channel) =< 9
 * for 10 pbus pio channels above.
 */
#define HPC3_PBUS_CFGPIO_OFFSET		0x100
#define HPC3_PBUS_CFGPIO_BASE		0x1fbdd000
#define HPC3_PBUS_CFG_ADDR(x)		(HPC3_PBUS_CFGPIO_BASE \
						+ (x*HPC3_PBUS_CFGPIO_OFFSET))
/* pbus pio configuration's bit definitions */
#define	HPC3_CFGPIO_RD_P2		0x00000001
#define	HPC3_CFGPIO_RD_P3		0x0000001e
#define	HPC3_CFGPIO_RD_P4		0x000001e0
#define	HPC3_CFGPIO_WR_P2		0x00000200
#define	HPC3_CFGPIO_WR_P3		0x00003c00
#define	HPC3_CFGPIO_WR_P4		0x0003c000
#define	HPC3_CFGPIO_DS_16		0x00040000
#define	HPC3_CFGPIO_EVEN_HI		0x00080000
/* 
 * Determine pbus configuration dma's base address  Where 0 =< x(channel) =< 7 
 * for 8 PBUS DMA channels.
 */
#define HPC3_PBUS_CFGDMA_OFFSET		0x200
#define HPC3_PBUS_CFGDAM_BASE		0x1fbdc000
#define HPC3_PBUS_CFGDMA_ADDR(x)\
					(HPC3_PBUS_CFGDAM_BASE \
						+ (x*HPC3_PBUS_CFGDMA_OFFSET))
/* 
 * Determine pbus buffer pointer's base address  Where 0 =< x(channel) =< 7 
 * for 8 pbus DMA channels.
 * Note: HPC3_PBUS_BP is included in MAIN PBUS CTRL STRUCTURE.
 * Use HPC3_PBUS_BP to determine the base address of the MAIN 
 * STRUCTURE pbus_ctrl.
 */
#define HPC3_PBUS_BP_OFFSET	 	0x2000	
#define HPC3_PBUS_BP_BASE	 	0x1fb80000
#define HPC3_PBUS_BP(x)   	 	(HPC3_PBUS_BP_BASE + \
						(x*HPC3_PBUS_BP_OFFSET))

/* 
 * Determine pbus descriptor pointer's base address  Where 0 =< x(channel) =< 7 
 * for 8 pbus DMA channels.
 * Note: HPC3_PBUS_DP is included in MAIN STRUCTURE pbus_ctrl structure.
 */
#define HPC3_PBUS_DP_OFFSET	 	0x2000	
#define HPC3_PBUS_DP_BASE	 	0x1fb80004
#define HPC3_PBUS_DP(x)   	 	(HPC3_PBUS_DP_BASE + \
						(x*HPC3_PBUS_DP_OFFSET))

/* 
 * Determine pbus control's base address  Where 0 =< x(channel) =< 7 
 * for 8 pbus DMA channels.
 * Note: HPC3_PBUS_CONTROL is included in MAIN STRUCTURE pbus_ctrl structure.
 */
#define HPC3_PBUS_CONTROL_OFFSET	0x2000
#define HPC3_PBUS_CONTROL_BASE		0x1fb81000
#ifdef _LANGUAGE_C
#define HPC3_PBUS_CONTROL_ADDR(x)	(HPC3_PBUS_CONTROL_BASE +	\
					 (__psunsigned_t)		\
					 ((x)*HPC3_PBUS_CONTROL_OFFSET))
#else
#define HPC3_PBUS_CONTROL_ADDR(x)	(HPC3_PBUS_CONTROL_BASE + \
					 ((x)*HPC3_PBUS_CONTROL_OFFSET))
#endif
/* PBUS CONTROL register bit definitions-WRITE */
#define PBUS_CTRL_ENDIAN	0x02 /* 1=little, 0=big */
#define PBUS_CTRL_DMADIR	0x04 /* 1=read, 0=write */
#define PBUS_CTRL_FLUSH		0x08 /* 1=flushes on dma read */
#define PBUS_CTRL_DMASTART	0x10 /* 1=starts dma, 0=stops dma */
#define PBUS_CTRL_LOAD_ENA	0x20 /* 1=enable write to PBUS_CTRL_DMASTART */
#define PBUS_CTRL_REALTIME	0x40 /* 1=enables realtime GIO bus service */
/* PBUS CONTROL register bit definitions-READ */
#define PBUS_CTRL_INT_PEND	0x01 /* interrupt pending on reads */
#define PBUS_CTRL_DMASTAT	0x02 /* DMA still active */

/*
 * various pbus addresses.
 */
#define HPC3_PBUS_PROM_WE	0x1fbde000	/* XXX: boot prom */
#define HPC3_PBUS_PROM_SWAP	0x1fbde800	/* XXX: prom swap */
#define HPC3_PBUS_GEN_OUT	0x1fbdf800	/* general output bit */
#define HPC3_PBUS_RTC_1286	0x1fbe0000	/* Dallas 1286/1386 */

/*******************************************************************
 * Everything below here relate to structures for hpc3 registers.  *
 * Base addresses of these structres are determined by macros      *
 * defined immediately before them.			 	   *
 *******************************************************************/ 
#ifdef _LANGUAGE_C
/* 
 * hpc3.h - Specific register structure and addresses
 * for IP22/HPC3. 
 */

/* HPC3/SCSI Direct memory Access format*/
#define HPC3_CBP_OFFSET		0
#define HPC3_BCNT_OFFSET	4
#define HPC3_NBP_OFFSET		8
#define HPC3_EOX_VALUE		0x80000000
typedef struct scdescr {
#ifdef _MIPSEB
    unsigned cbp;
    unsigned eox:1,pad1:1,xie:1,pad0:15,bcnt:14;
    unsigned nbp;
    unsigned word_pad;
#endif /* _MIPSEB */
#ifdef _MIPSEL
    unsigned cbp;
    unsigned bcnt:14,pad0:15,xie:1,pad1:1,eox:1;
    unsigned nbp;
    unsigned word_pad;
#endif /* _MIPSEL */
} scdescr_t;


/*******************************************************************
 * GIO64 Bus Interface Control Register structures                 *
 *******************************************************************/ 

/* 
 * Interrupt status register structure's address.
 * Note: HPC3_GIO64_INTSTAT is included in MAIN GIO64 STRUCTURE.
 * Use HPC3_GIO64_INTSTAT to determine the base
 * address of the MAIN STRUCTURE gio64_bus_control.
 */
#define HPC3_GIO64_INTSTAT	(struct gio64_intstat *)HPC3_INTSTAT_ADDR
typedef struct gio64_intstat {		/* Read/Write */
#ifdef _MIPSEB
	unsigned int pad0			:22;	/* bit 31:10 */
	unsigned int scsi_chan1_int_stat	:1;	/* bit 9 */
	unsigned int scsi_chan0_int_stat	:1;	/* bit 8 */
	unsigned int pbus_int_stat		:8;	/* bit 7:0 */
#endif /* _MIPSEB */
#ifdef _MIPSEL
	unsigned int pbus_int_stat		:8;	/* bit 7:0 */
	unsigned int scsi_chan0_int_stat	:1;	/* bit 8 */
	unsigned int scsi_chan1_int_stat	:1;	/* bit 9 */
	unsigned int pad0			:22;	/* bit 31:10 */
#endif /* _MIPSEL */
} gio64_intstat_t;

/* 
 * Determine miscellaneous collection bit structure's address.
 * Note: HPC3_GIO64_MISC is included in MAIN STRUCTURE
 * gio64_bus_control.
 */
#define HPC3_GIO64_MISC		(struct gio64_misc *)HPC3_MISC_ADDR 
typedef struct gio64_misc {	                /* Read/Write */
#ifdef _MIPSEB
	unsigned int pad0		:30;	/* bit 31:2 */
	unsigned int des_endian		:1;	/* bit 1 */
	unsigned int en_real_time	:1;	/* bit 0 */
#endif /* _MIPSEB */
#ifdef _MIPSEL
	unsigned int en_real_time	:1;	/* bit 0 */
	unsigned int des_endian		:1;	/* bit 1 */
	unsigned int pad0		:30;	/* bit 31:2 */
#endif /* _MIPSEL */
} gio64_misc_t;

/* 
 * Determine bus error interrupt status structure's address.
 * Note: HPC3_BUSERR_STAT_ADDR is included in MAIN STRUCTURE
 * gio64_bus_control.
 */
#define HPC3_GIO64_BUSERR_STAT	(struct gio64_buserr_stat *) \
					HPC3_BUSERR_STAT_ADDR 
typedef struct gio64_buserr_stat {	/* Read/Write */
#ifdef _MIPSEB
	unsigned int pad0	          :13;	/* bit 31:19 */
	unsigned int parity_id      	  :10;	/* bit 18:9 */
	unsigned int pio_dma	          :1;	/* bit 8 */
	unsigned int byte_lane_err     	  :8;	/* bit 7:0 */
#endif /* _MIPSEB */
#ifdef _MIPSEL
	unsigned int byte_lane_err     	  :8;	/* bit 7:0 */
	unsigned int pio_dma	          :1;	/* bit 8 */
	unsigned int parity_id      	  :10;	/* bit 18:9 */
	unsigned int pad0		  :13;	/* bit 31:19 */
#endif /* _MIPSEL */
} gio64_buserr_stat_t;

/* 
 * - MAIN GIO64 STRUCTURE 
 * GIO64 bus interface control register structure. 
 * Note: Use HPC3_GIO64_INTSTAT macro above for base
 * address of this structure.
 */
typedef struct gio64_bus_control {	
	gio64_intstat_t 	intstat;	/* DMA interrupt status */
	gio64_misc_t		misc;		/* Misc collection of bits */
	unsigned char		pad0[0xc];
	gio64_buserr_stat_t	bus_err_stat;	/* Bus err interrupt status */
} gio64_bus_control_t;


/*******************************************************************
 * ETHERNET Register structures                 		   *
 *******************************************************************/ 

/* 
 * Ethernet byte count info structure base address for the receiver channel.
 */
#define HPC3_ETHER_RX_BC	(struct ether_rx_bc *)HPC3_ETHER_RX_BC_ADDR
typedef struct ether_rx_bc {
#ifdef _MIPSEB
	unsigned int eox	:1;		/* bit 31 */
	unsigned int pad1	:1;
        unsigned int xie	:1;		/* bit 29 */
	unsigned int pad0	:15;
        unsigned int des_bc	:14;		/* bit 13:0 */
#endif /* _MIPSEB */
#ifdef _MIPSEL
        unsigned int des_bc	:14;		/* bit 13:0 */
	unsigned int pad0	:15;
        unsigned int xie	:1;		/* bit 29 */
	unsigned int pad1	:1;
	unsigned int eox	:1;		/* bit 31 */
#endif /* _MIPSEL */
} ether_rx_bc_t;

/* 
 * Ethernet miscellaneous bits structre base address.
 */
#define HPC3_ETHER_MISC		(struct ether_misc *)HPC3_ETHER_MISC_ADDR	
typedef struct ether_misc {
#ifdef _MIPSEB
	unsigned int pad0	:29;		/* bit 31:3 */
	unsigned int loopback	:1;		/* bit 2 */
	unsigned int clrint	:1;		/* bit 1 */
	unsigned int ch_reset	:1;		/* bit 0 */
#endif /* _MIPSEB */
#ifdef _MIPSEL
	unsigned int ch_reset	:1;		/* bit 0 */
	unsigned int clrint	:1;		/* bit 1 */
	unsigned int loopback	:1;		/* bit 2 */
	unsigned int pad0	:29;		/* bit 31:3 */
#endif /* _MIPSEL */
} ether_misc_t;

/* 
 * Ethernet dma configuration structre base address.
 */
#define HPC3_ETHER_DMA_CFG	(struct ether_dma_cfg *)HPC3_ETHER_DMA_CFG_ADDR
typedef struct ether_dma_cfg {
#ifdef _MIPSEB
	unsigned int pad0		:14;		/* bit 31:18 */
	unsigned int pgm_timeout	:2;		/* bit 17:16 */
	unsigned int fix_intr		:1;		/* bit 15 */
	unsigned int fix_eop		:1;		/* bit 14 */
	unsigned int fix_rxdc		:1;		/* bit 13 */
	unsigned int wr_ctrl		:1;		/* bit 12 */
	unsigned int dma_d3		:4;		/* bit 11:8 */
	unsigned int dma_d2		:4;		/* bit 7:4 */
	unsigned int dma_d1		:4;		/* bit 3:0 */
#endif /* _MIPSEB */
#ifdef _MIPSEL
	unsigned int dma_d1		:4;		/* bit 3:0 */
	unsigned int dma_d2		:4;		/* bit 7:4 */
	unsigned int dma_d3		:4;		/* bit 11:8 */
	unsigned int wr_ctrl		:1;		/* bit 12 */
	unsigned int fix_rxdc		:1;		/* bit 13 */
	unsigned int fix_eop		:1;		/* bit 14 */
	unsigned int fix_intr		:1;		/* bit 15 */
	unsigned int pgm_timeout	:2;		/* bit 17:16 */
	unsigned int pad0		:14;		/* bit 31:18 */
#endif /* _MIPSEL */
} ether_dma_cfg_t;

/*  
 * Ethernet pio configuration structre base address.
 */
#define HPC3_ETHER_PIO_CFG	(struct ether_pio_cfg *)HPC3_ETHER_PIO_CFG_ADDR
typedef struct ether_pio_cfg {
#ifdef _MIPSEB
	unsigned int pad0	:19;		/* bit 31:13 */
	unsigned int test_ram	:1;		/* bit 12 */
	unsigned int pio_p3	:4;		/* bit 11:8 */
	unsigned int pio_p2	:4;		/* bit 7:4 */
	unsigned int pio_p1	:4;		/* bit 3:0 */
#endif /* _MIPSEB */
#ifdef _MIPSEL
	unsigned int pio_p1	:4;		/* bit 3:0 */
	unsigned int pio_p2	:4;		/* bit 7:4 */
	unsigned int pio_p3	:4;		/* bit 11:8 */
	unsigned int test_ram	:1;		/* bit 12 */
	unsigned int pad0	:19;		/* bit 31:13 */
#endif /* _MIPSEL */
} ether_pio_cfg_t;

/* 
 * Ethernet byte count info structure base address for 
 * the transmitter channel.
 */
#define HPC3_ETHER_TX_BC	(struct ether_tx_bc *)HPC3_ETHER_TX_BC_ADDR
typedef struct ether_tx_bc {
#ifdef _MIPSEB
	unsigned int eox		:1;		/* bit 31 */
        unsigned int eop		:1;		/* bit 30 */
        unsigned int xie		:1;		/* bit 29 */
        unsigned int eox_sampled	:1;		/* bit 28 */
        unsigned int pad0		:14;		/* bit 27:14 */
        unsigned int des_bc		:14;		/* bit 13:0 */
#endif /* _MIPSEB */
#ifdef _MIPSEL
        unsigned int des_bc		:14;		/* bit 13:0 */
        unsigned int pad0		:14;		/* bit 27:14 */
        unsigned int eox_sampled	:1;		/* bit 28 */
        unsigned int xie		:1;		/* bit 29 */
        unsigned int eop		:1;		/* bit 30 */
	unsigned int eox		:1;		/* bit 31 */
#endif /* _MIPSEL */
} ether_tx_bc_t;

/* 
 * Ethernet control structure base address for the transmitter channel.
 */
#define HPC3_ETHER_TX_CNTRL	(struct ether_tx_cntrl *) \
					HPC3_ETHER_TX_CNTRL_ADDR      
typedef struct ether_tx_cntrl {
#ifdef _MIPSEB
        unsigned int pad0		:21;		/* bit 31:11 */
        unsigned int ch_active_mask	:1;		/* bit 10 */
	unsigned int ch_active		:1;		/* bit 9 */
        unsigned int endian		:1;		/* bit 8 */
        unsigned int status_7_5		:3;		/* bit 7:5 */
        unsigned int status_4		:1;		/* bit 4 */
        unsigned int status_3_0		:4;		/* bit 3:0 */
#endif /* _MIPSEB */
#ifdef _MIPSEL
        unsigned int status_3_0		:4;		/* bit 3:0 */
        unsigned int status_4		:1;		/* bit 4 */
        unsigned int status_7_5		:3;		/* bit 7:5 */
        unsigned int endian		:1;		/* bit 8 */
	unsigned int ch_active		:1;		/* bit 9 */
        unsigned int ch_active_mask	:1;		/* bit 10 */
        unsigned int pad0		:21;		/* bit 31:11 */
#endif /* _MIPSEL */
} ether_tx_cntrl_t;


/*******************************************************************
 * SCSI Register structures                 		   	   *
 *******************************************************************/ 

/* 
 * Determine scsi buffer structure's address  Where 0 =< x(channel) =< 1 
 */
#define HPC3_SCSI_BUFFER_PTR(x) (struct scsi_buffer_ptr *)\
					HPC3_SCSI_BUFFER_PTR_ADDR(x)
typedef struct scsi_buffer_ptr {
        unsigned int cbp;               /* Read only */
        unsigned int nbdp;              /* Read/Write */
} scsi_buffer_ptr_t;

/* 
 * Determine scsi byte count structure's address  Where 0 =< x(channel) =< 1 
 */
#define HPC3_SCSI_BC(x)		(struct scsi_bc *)HPC3_SCSI_BC_ADDR(x)
typedef struct scsi_bc {		/* Read only */
#ifdef _MIPSEB
	unsigned int eox      :1;	/* bit 31 */
	unsigned int pad0     :1;	/* bit 30 */
	unsigned int xie      :1;	/* bit 29 */
	unsigned int pad1     :15;	/* bit 28:14 */
	unsigned int des_bc   :14;	/* bit 13:0  */
#endif /* _MIPSEB */
#ifdef _MIPSEL
	unsigned int des_bc   :14;	/* bit 13:0  */
	unsigned int pad1     :15;	/* bit 28:14 */
	unsigned int xie      :1;	/* bit 29 */
	unsigned int pad0     :1;	/* bit 30 */
	unsigned int eox      :1;	/* bit 31 */
#endif /* _MIPSEL */
} scsi_bc_t;


/* 
 * Determine scsi dma configuration's structure address 
 * Where 0 =< x(channel) =< 1.
 */
#define HPC3_SCSI_DMACFG(x) 	(struct scsi_dma_cfg *) \
					HPC3_SCSI_DMACFG_ADDR(x)
typedef struct scsi_dma_cfg {			/* Read/Wite */
#ifdef _MIPSEB
	unsigned int pad0		:13;	/* bit 31:18 */
	unsigned int dreq_early		:2;	/* bit 17:16 */
	unsigned int dreq_pol		:1;	/* bit 15 */
	unsigned int dma_parity_en	:1;	/* bit 14 */
	unsigned int dma_swap		:1;	/* bit 13 */
	unsigned int dma_16		:1;	/* bit 12 */
	unsigned int hwm		:3;	/* bit 11:9 */
	unsigned int dma_d3		:3;	/* bit 8:6 */
	unsigned int dma_d2		:3;	/* bit 5:3 */
	unsigned int dma_d1		:3;	/* bit 2:1 */
	unsigned int half_clock		:1;	/* bit 0 */
#endif /* _MIPSEB */
#ifdef _MIPSEL
	unsigned int half_clock		:1;	/* bit 0 */
	unsigned int dma_d1		:3;	/* bit 2:1 */
	unsigned int dma_d2		:3;	/* bit 5:3 */
	unsigned int dma_d3		:3;	/* bit 8:6 */
	unsigned int hwm		:3;	/* bit 11:9 */
	unsigned int dma_16		:1;	/* bit 12 */
	unsigned int dma_swap		:1;	/* bit 13 */
	unsigned int dma_parity_en	:1;	/* bit 14 */
	unsigned int dreq_pol		:1;	/* bit 15 */
	unsigned int dreq_early		:2;	/* bit 17:16 */
	unsigned int pad0		:13;	/* bit 31:18 */
#endif /* _MIPSEL */
} scsi_dma_cfg_t;

/* 
 * Determine scsi pio configuration's structure base address  
  * Where 0 =< x(channel) =< 1.
 */
#define HPC3_SCSI_PIOCFG(x)	(struct scsi_pio_cfg *) \
					HPC3_SCSI_PIOCFG_ADDR(x)
typedef struct scsi_pio_cfg {			/* Read/Write */
#ifdef _MIPSEB
	unsigned int pad0		:16;	/* bit 16:31 */
	unsigned int pio_fuji_mode	:1;	/* bit 15 */
	unsigned int pio_parity_en	:1;	/* bit 14 */
	unsigned int pio_swap		:1;	/* bit 13 */
	unsigned int pio_16		:1;	/* bit 12 */
	unsigned int pio_p1		:3;	/* bit 11:9 */
	unsigned int pio_p2_rd		:4;	/* bit 8:5 */
	unsigned int pio_p2_wr		:3;	/* bit 4:2 */
	unsigned int pio_p3		:2;	/* bit 1:0 */
#endif /* _MIPSEB */
#ifdef _MIPSEL
	unsigned int pio_p3		:2;	/* bit 1:0 */
	unsigned int pio_p2_wr		:3;	/* bit 4:2 */
	unsigned int pio_p2_rd		:4;	/* bit 8:5 */
	unsigned int pio_p1		:3;	/* bit 11:9 */
	unsigned int pio_16		:1;	/* bit 12 */
	unsigned int pio_swap		:1;	/* bit 13 */
	unsigned int pio_parity_en	:1;	/* bit 14 */
	unsigned int pio_fuji_mode	:1;	/* bit 15 */
	unsigned int pad0		:16;	/* bit 16:31 */
#endif /* _MIPSEL */
} scsi_pio_cfg_t;

/*******************************************************************
 * PBUS Register structures                 		   	   *
 *******************************************************************/ 

typedef struct pbus_cfgpio {	                /* Read/Write */
#ifdef _MIPSEB
	unsigned int pad0           :12;    /* bit 31:20 */
	unsigned int even_high      :1;     /* bit 19 */
	unsigned int ds_16          :1;     /* bit 18 */
	unsigned int wr_p4          :4;     /* bit 17:14 */
	unsigned int wr_p3          :4;     /* bit 13:10 */
	unsigned int wr_p2          :1;     /* bit 9 */
	unsigned int rd_p4          :4;     /* bit 8:5 */
	unsigned int rd_p3          :4;     /* bit 4:1 */
	unsigned int rd_p2          :1;     /* bit 0 */
#endif /* _MIPSEB */
#ifdef _MIPSEL
	unsigned int rd_p2          :1;     /* bit 0 */
	unsigned int rd_p3          :4;     /* bit 4:1 */
	unsigned int rd_p4          :4;     /* bit 8:5 */
	unsigned int wr_p2          :1;     /* bit 9 */
	unsigned int wr_p3          :4;     /* bit 13:10 */
	unsigned int wr_p4          :4;     /* bit 17:14 */
	unsigned int ds_16          :1;     /* bit 18 */
	unsigned int even_high      :1;     /* bit 19 */
	unsigned int pad0           :12;    /* bit 31:20 */
#endif /* _MIPSEL */
} pbus_cfgpio_t;

typedef struct pbus_cfgdma {
#ifdef _MIPSEB
	unsigned int pad0          :4;	/* bit 31:28 */
	unsigned int drq_live      :1;	/* bit 27 */
	unsigned int burst_count   :5;	/* bit 26:22 */
	unsigned int real_time     :1;	/* bit 21 */
	unsigned int pad1          :1;	/* bit 20 */
	unsigned int even_high     :1;	/* bit 19 */
	unsigned int ds_16         :1;	/* bit 18 */
	unsigned int wr_d5         :4;	/* bit 17:14 */
	unsigned int wr_d4         :4;	/* bit 13:10 */
	unsigned int wr_d3         :1;	/* bit 9 */
	unsigned int rd_d5         :4;	/* bit 8:5 */
	unsigned int rd_d4         :4;	/* bit 4:1 */
	unsigned int rd_d3         :1;	/* bit 0 */
#endif /* _MIPSEB */
#ifdef _MIPSEL
	unsigned int rd_d3         :1;	/* bit 0 */
	unsigned int rd_d4         :4;	/* bit 4:1 */
	unsigned int rd_d5         :4;	/* bit 8:5 */
	unsigned int wr_d3         :1;	/* bit 9 */
	unsigned int wr_d4         :4;	/* bit 13:10 */
	unsigned int wr_d5         :4;	/* bit 17:14 */
	unsigned int ds_16         :1;	/* bit 18 */
	unsigned int even_high     :1;	/* bit 19 */
	unsigned int pad1          :1;	/* bit 20 */
	unsigned int real_time     :1;	/* bit 21 */
	unsigned int burst_count   :5;	/* bit 26:22 */
	unsigned int drq_live      :1;	/* bit 27 */
	unsigned int pad0          :4;	/* bit 31:28 */
#endif /* _MIPSEL */
} pbus_cfgdma_t;

#define HPC3_PBUS_CONTROL(x)		(struct pbus_control_write *) \
						HPC3_PBUS_CONTROL_ADDR(x)
typedef struct pbus_control_write {		/* Write */
#ifdef _MIPSEB
	unsigned int pad0		:2;     /* bit 31:30 */
	unsigned int fifo_end		:6;     /* bit 29:24 */
	unsigned int pad1		:2;     /* bit 23:22 */
	unsigned int fifo_beg		:6;     /* bit 21:16 */
	unsigned int highwater		:8;     /* bit 15:8 */
	unsigned int pad2		:1;     /* bit 7 */
	unsigned int real_time		:1;     /* bit 6 */
	unsigned int ch_act_ld		:1;     /* bit 5 */
	unsigned int ch_act		:1;     /* bit 4 */
	unsigned int flush		:1;     /* bit 3 */
	unsigned int receive		:1;     /* bit 2 */
	unsigned int little		:1;     /* bit 1 */
	unsigned int pad3		:1;     /* bit 0 */
#endif /* _MIPSEB */
#ifdef _MIPSEL
	unsigned int pad3		:1;     /* bit 0 */
	unsigned int little		:1;     /* bit 1 */
	unsigned int receive		:1;     /* bit 2 */
	unsigned int flush		:1;     /* bit 3 */
	unsigned int ch_act		:1;     /* bit 4 */
	unsigned int ch_act_ld		:1;     /* bit 5 */
	unsigned int real_time		:1;     /* bit 6 */
	unsigned int pad2		:1;     /* bit 7 */
	unsigned int highwater		:8;     /* bit 15:8 */
	unsigned int fifo_beg		:6;     /* bit 21:16 */
	unsigned int pad1		:2;     /* bit 23:22 */
	unsigned int fifo_end		:6;     /* bit 29:24 */
	unsigned int pad0		:2;     /* bit 31:30 */
#endif /* _MIPSEL */
} pbus_control_write_t;

typedef struct pbus_control_read {              /* Read */
#ifdef _MIPSEB
        unsigned int pad0               :30;    /* bit 31:2 */
        unsigned int ch_act             :1;     /* bit 1 */
        unsigned int intr               :1;     /* bit 0 */
#endif /* _MIPSEB */
#ifdef _MIPSEL
        unsigned int intr               :1;     /* bit 0 */
        unsigned int ch_act             :1;     /* bit 1 */
        unsigned int pad0               :30;    /* bit 31:2 */
#endif /* _MIPSEL */
} pbus_control_read_t;


typedef struct pbus_control{            /* Write/Read */
    union {
        pbus_control_write_t controlW;
        pbus_control_read_t  controlR;
          } controlRW;
} pbus_control_t;
#define control_wr controlRW.controlW
#define control_rd controlRW.controlR

/*
 * - MAIN PBUS CTRL STRUCTURE
 * Note: Use HPC3_PBUS_BP macro above for base
 * address of this structure.
 */
typedef struct pbus_ctrl {
        unsigned int          bp;               /* Read */
        unsigned int          dp;               /* Read/Write */
        unsigned char         pad0[0xff8];
        pbus_control_t        control;          /* Read/Write */
} pbus_ctrl_t;

#if _STANDALONE		/* ide stuff; hpc3plp in kernel has own version */
#define GETDESC(adr)		( (unsigned)(adr),		\
				(unsigned)(adr))
#endif  /* _STANDALONE */

#endif /* _LANGUAGE_C */
#endif /* __SYS_HPC3_H__ */
