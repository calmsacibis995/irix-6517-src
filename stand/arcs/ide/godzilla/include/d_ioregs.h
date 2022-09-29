/**************************************************************************
 * Copyright 1996, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 **************************************************************************/

#ifndef __IOC3REG_H__
#define __IOC3REG_H__

#ident	"ide/godzilla/include/d_ioregs.h:  $Revision: 1.6 $"

/*
 * ioc3.h - IOC3REG chip header file
 */

#include "sys/RACER/IP30.h"

/* Notes:  
 * The IOC3REG chip is a 32-bit PCI device that provides most of the
 * base I/O for several PCI-based workstations and servers.
 * It includes a 10/100 MBit ethernet MAC, an optimized DMA buffer 
 * management, and a store-and-forward buffer RAM.
 * All IOC3REG registers are 32 bits wide.  
 */

/*
 * PCI Configuration Space Register Address Map, based on IP30.h (dwong) 
 * IOC3_PCI_CFG_BASE = 0x1f022000
 * IOC3_PCI_DEVIO_BASE = 0x1f600000
 */
#define IOC3REG_PCI_ID		IOC3_PCI_CFG_BASE
#define IOC3REG_PCI_SCR		IOC3_PCI_CFG_BASE+0x4		/* Status/Command */
#define IOC3REG_PCI_REV		IOC3_PCI_CFG_BASE+0x8		/* Revision */
#define IOC3REG_PCI_LAT		IOC3_PCI_CFG_BASE+0xc		/* Latency Timer */
#define IOC3REG_PCI_ADDR	IOC3_PCI_CFG_BASE+0x10		/* IOC3REG base address */
#define IOC3REG_PCI_ERR_ADDR_L	IOC3_PCI_CFG_BASE+0x40		/* Low Error Address */
#define IOC3REG_PCI_ERR_ADDR_H	IOC3_PCI_CFG_BASE+0x44		/* High Error Address */

/*
 * PCI IO/Mem Space Register Address Map, use offset from IOC3REG PCI 
 * io/mem base such that this can be used for multiple IOC3REGs
 */
#define IOC3REG_SIO_IR		IOC3_PCI_DEVIO_BASE+0x01C	/* SuperIO Interrupt Register */
#define IOC3REG_SIO_IES		IOC3_PCI_DEVIO_BASE+0x020	/* SuperIO Interrupt Enable Set Reg. */
#define IOC3REG_SIO_IEC		IOC3_PCI_DEVIO_BASE+0x024	/* SuperIO Interrupt Enable Clear Reg */
#define IOC3REG_SIO_CR		IOC3_PCI_DEVIO_BASE+0x028	/* SuperIO Control Reg */
#define IOC3REG_INT_OUT		IOC3_PCI_DEVIO_BASE+0x02C	/* INT_OUT Reg (realtime interrupt) */
#define IOC3REG_MCR		IOC3_PCI_DEVIO_BASE+0x030	/* MicroLAN Control Reg */
#define IOC3REG_GPCR_S		IOC3_PCI_DEVIO_BASE+0x034	/* GenericPIO Control Set Register */
#define IOC3REG_GPCR_C		IOC3_PCI_DEVIO_BASE+0x038	/* GenericPIO Control Clear Register */
#define IOC3REG_GPDR		IOC3_PCI_DEVIO_BASE+0x03C	/* GenericPIO Data Register */
#define IOC3REG_GPPR_0		IOC3_PCI_DEVIO_BASE+0x040	/* GenericPIO Pin Register */

/* Parallel Port Registers */
#define IOC3REG_PPBR_H_A	IOC3_PCI_DEVIO_BASE+0x080	/* Par. Port Buffer Upper Address regA*/
#define IOC3REG_PPBR_L_A	IOC3_PCI_DEVIO_BASE+0x084	/* Par. Port Buffer Lower Address regA*/
#define IOC3REG_PPCR_A		IOC3_PCI_DEVIO_BASE+0x088	/* Parallel Port Count register A */
#define IOC3REG_PPCR		IOC3_PCI_DEVIO_BASE+0x08C	/* Parallel Port DMA control register */
#define IOC3REG_PPBR_H_B	IOC3_PCI_DEVIO_BASE+0x090	/* Par. Port Buffer Upper Address regB*/
#define IOC3REG_PPBR_L_B	IOC3_PCI_DEVIO_BASE+0x094	/* Par. Port Buffer Lower Address regB*/
#define IOC3REG_PPCR_B		IOC3_PCI_DEVIO_BASE+0x098	/* Parallel Port Count register B */

/* Keyboard and Mouse Registers */
#define IOC3REG_KM_CSR		IOC3_PCI_DEVIO_BASE+0x09C	/* kbd and Mouse Control/Status reg. */
#define IOC3REG_K_RD		IOC3_PCI_DEVIO_BASE+0x0A0	/* kbd Read Data Register */
#define IOC3REG_M_RD		IOC3_PCI_DEVIO_BASE+0x0A4	/* mouse Read Data Register */
#define IOC3REG_K_WD		IOC3_PCI_DEVIO_BASE+0x0A8	/* kbd Write Data Register */
#define IOC3REG_M_WD		IOC3_PCI_DEVIO_BASE+0x0AC	/* mouse Write Data Register */

/* Serial Port Registers */
#define IOC3REG_SBBR_H		IOC3_PCI_DEVIO_BASE+0x0B0	/* Serial Port Ring Buffers Base Reg.H*/
#define IOC3REG_SBBR_L		IOC3_PCI_DEVIO_BASE+0x0B4	/* Serial Port Ring Buffers Base Reg.L*/

#define IOC3REG_SSCR_A		IOC3_PCI_DEVIO_BASE+0x0B8	/* Serial Port A Control */
#define IOC3REG_STPIR_A		IOC3_PCI_DEVIO_BASE+0x0BC	/* Serial Port A TX Produce */
#define IOC3REG_STCIR_A		IOC3_PCI_DEVIO_BASE+0x0C0	/* Serial Port A TX Consume */
#define IOC3REG_SRPIR_A		IOC3_PCI_DEVIO_BASE+0x0C4	/* Serial Port A RX Produce */
#define IOC3REG_SRCIR_A		IOC3_PCI_DEVIO_BASE+0x0C8	/* Serial Port A RX Consume */
#define IOC3REG_SRTR_A		IOC3_PCI_DEVIO_BASE+0x0CC	/* Serial Port A Receive Timer Reg. */
#define IOC3REG_SHADOW_A	IOC3_PCI_DEVIO_BASE+0x0D0	/* Serial Port A 16550 Shadow Register*/

#define IOC3REG_SSCR_B		IOC3_PCI_DEVIO_BASE+0x0D4	/* Serial Port B Control */
#define IOC3REG_STPIR_B		IOC3_PCI_DEVIO_BASE+0x0D8	/* Serial Port B TX Produce */
#define IOC3REG_STCIR_B		IOC3_PCI_DEVIO_BASE+0x0DC	/* Serial Port B TX Consume */
#define IOC3REG_SRPIR_B		IOC3_PCI_DEVIO_BASE+0x0E0	/* Serial Port B RX Produce */
#define IOC3REG_SRCIR_B		IOC3_PCI_DEVIO_BASE+0x0E4	/* Serial Port B RX Consume */
#define IOC3REG_SRTR_B		IOC3_PCI_DEVIO_BASE+0x0E8	/* Serial Port B Receive Timer Reg. */
#define IOC3REG_SHADOW_B	IOC3_PCI_DEVIO_BASE+0x0EC	/* Serial Port B 16550 Shadow Register*/

/* Ethernet Registers - see also struct ioc3_eregs defined below */
#define IOC3REG_REG_OFF		IOC3_PCI_DEVIO_BASE+0x0F0	/* Ethernet register base - for ioc3_eregs */
#define IOC3REG_RAM_OFF		IOC3_PCI_DEVIO_BASE+0x40000	/* SSRAM diagsnotic access */

#define IOC3REG_EMCR		IOC3_PCI_DEVIO_BASE+0x0F0	/* Ethernet MAC Control Register */
#define IOC3REG_EISR		IOC3_PCI_DEVIO_BASE+0x0F4	/* Ethernet Interrupt Status Register */
#define IOC3REG_EIER		IOC3_PCI_DEVIO_BASE+0x0F8	/* Ethernet Interrupt Enable Register */

#define IOC3REG_ERCSR		IOC3_PCI_DEVIO_BASE+0x0FC	/* Ethernet RX Control/Status Register*/
#define IOC3REG_ERBR_H		IOC3_PCI_DEVIO_BASE+0x100	/* Ethernet RX Base High Address Reg. */
#define IOC3REG_ERBR_L		IOC3_PCI_DEVIO_BASE+0x104	/* Ethernet RX Base Low Addr. Register*/
#define IOC3REG_ERBAR		IOC3_PCI_DEVIO_BASE+0x108	/* Ethernet RX Barrier Register */
#define IOC3REG_ERCIR		IOC3_PCI_DEVIO_BASE+0x10C	/* Ethernet RX Consume Index Register */
#define IOC3REG_ERPIR		IOC3_PCI_DEVIO_BASE+0x110	/* Ethernet RX Produce Index Register */
#define IOC3REG_ERTR		IOC3_PCI_DEVIO_BASE+0x114	/* Ethernet RX Timer Register */

#define IOC3REG_ETCSR		IOC3_PCI_DEVIO_BASE+0x118	/* Ethernet TX Control/Status Register*/
#define IOC3REG_ERSR		IOC3_PCI_DEVIO_BASE+0x11C	/* Ethernet Random Seed Register */
#define IOC3REG_ETCDC		IOC3_PCI_DEVIO_BASE+0x120	/* Ethernet TX Collision/Deferral Cntr*/
#define IOC3REG_ETBR_H		IOC3_PCI_DEVIO_BASE+0x128	/* Ethernet TX Base High Address Reg. */
#define IOC3REG_ETBR_L		IOC3_PCI_DEVIO_BASE+0x12C	/* Ethernet TX Base Low Addr. Register*/
#define IOC3REG_ETCIR		IOC3_PCI_DEVIO_BASE+0x130	/* Ethernet TX Consume Index Register */
#define IOC3REG_ETPIR		IOC3_PCI_DEVIO_BASE+0x134	/* Ethernet TX Produce Index Register */

#define IOC3REG_EBIR		IOC3_PCI_DEVIO_BASE+0x124	/* Ethernet Buffer Index Register */
#define IOC3REG_EMAR_H		IOC3_PCI_DEVIO_BASE+0x138	/* Ethernet MAC High Addr Register */
#define IOC3REG_EMAR_L		IOC3_PCI_DEVIO_BASE+0x13C	/* Ethernet MAC Low Addr Register */
#define IOC3REG_EHAR_H		IOC3_PCI_DEVIO_BASE+0x140	/* Ethernet Hashed High Addr Register */
#define IOC3REG_EHAR_L		IOC3_PCI_DEVIO_BASE+0x144	/* Ethernet Hashed Low Addr Register */
#define IOC3REG_MICR		IOC3_PCI_DEVIO_BASE+0x148	/* MII Management Interface Control */
#define IOC3REG_MIDR		IOC3_PCI_DEVIO_BASE+0x14C	/* MMI Management Interface Data Reg Read*/
#define IOC3REG_MIDW		IOC3_PCI_DEVIO_BASE+0x150	/* MMI Management Interface Data Reg Write*/

/* private page address aliases for usermode mapping */
#define IOC3REG_INT_OUT_P	IOC3_PCI_DEVIO_BASE+0x4000	/* INT_OUT Reg */
#define IOC3REG_SSCR_A_P	IOC3_PCI_DEVIO_BASE+0x8000
#define IOC3REG_STPIR_A_P	IOC3_PCI_DEVIO_BASE+0x8004
#define IOC3REG_STCIR_A_P	IOC3_PCI_DEVIO_BASE+0x8008  	/*(read-only)*/ 
#define IOC3REG_SRPIR_A_P	IOC3_PCI_DEVIO_BASE+0x800C  	/*(read-only)*/
#define IOC3REG_SRCIR_A_P	IOC3_PCI_DEVIO_BASE+0x8010
#define IOC3REG_SRTR_A_P	IOC3_PCI_DEVIO_BASE+0x8014
#define IOC3REG_SHADOW_A_P	IOC3_PCI_DEVIO_BASE+0x8018  	/*(read-only)*/

#define IOC3REG_SSCR_B_P	IOC3_PCI_DEVIO_BASE+0xC000
#define IOC3REG_STPIR_B_P	IOC3_PCI_DEVIO_BASE+0xC004
#define IOC3REG_STCIR_B_P	IOC3_PCI_DEVIO_BASE+0xC008  	/*(read-only)*/
#define IOC3REG_SRPIR_B_P	IOC3_PCI_DEVIO_BASE+0xC00C  	/*(read-only)*/
#define IOC3REG_SRCIR_B_P	IOC3_PCI_DEVIO_BASE+0xC010
#define IOC3REG_SRTR_B_P	IOC3_PCI_DEVIO_BASE+0xC014
#define IOC3REG_SHADOW_B_P	IOC3_PCI_DEVIO_BASE+0xC018  	/*(read-only)*/

/* SSRAM Diagnostic Access */
#define IOC3REG_SSRAM_BASE	IOC3_PCI_DEVIO_BASE+0x40000	
#define IOC3REG_SSRAM_LEN	0x40000		/* 256kb (address space size, may not be fully populated) */
#define IOC3REG_SSRAM_DM	0x0000ffff	/* data mask */
#define IOC3REG_SSRAM_PM	0x00010000	/* parity mask */

/*DUART Address Map*/
#define DU_SIO_UARTC			IOC3_PCI_DEVIO_BASE+0x20141
#define DU_SIO_KBDCG			IOC3_PCI_DEVIO_BASE+0x20142

/*Parallal port*/
#define DU_SIO_PP_DATA 			IOC3_PCI_DEVIO_BASE+0x20150		
#define DU_SIO_PP_DSR			IOC3_PCI_DEVIO_BASE+0x20151		
#define DU_SIO_PP_DCR			IOC3_PCI_DEVIO_BASE+0x20152		
#define DU_SIO_PP_FIFA			IOC3_PCI_DEVIO_BASE+0x20158		
#define DU_SIO_PP_CFGB			IOC3_PCI_DEVIO_BASE+0x20159		
#define DU_SIO_PP_ECR			IOC3_PCI_DEVIO_BASE+0x2015A

/*Real Time clock*/
#define DU_SIO_RTCAD			IOC3_PCI_DEVIO_BASE+0x20168
#define DU_SIO_RTCDAT			IOC3_PCI_DEVIO_BASE+0x20169

/*DUART Serial Port B*/
#define DU_SIO_UB_THOLD			IOC3_PCI_DEVIO_BASE+0x20170
#define DU_SIO_UB_RHOLD			IOC3_PCI_DEVIO_BASE+0x20170
#define DU_SIO_UB_DIV_LSB		IOC3_PCI_DEVIO_BASE+0x20170
#define DU_SIO_UB_DIV_MSB		IOC3_PCI_DEVIO_BASE+0x20171
#define DU_SIO_UB_IENB			IOC3_PCI_DEVIO_BASE+0x20171
#define DU_SIO_UB_IIDENT		IOC3_PCI_DEVIO_BASE+0x20172
#define DU_SIO_UB_FIFOC			IOC3_PCI_DEVIO_BASE+0x20172
#define DU_SIO_UB_LINEC			IOC3_PCI_DEVIO_BASE+0x20173
#define DU_SIO_UB_MODEMC		IOC3_PCI_DEVIO_BASE+0x20174
#define DU_SIO_UB_LINES			IOC3_PCI_DEVIO_BASE+0x20175
#define DU_SIO_UB_MODEMS		IOC3_PCI_DEVIO_BASE+0x20176
#define DU_SIO_UB_SCRPAD		IOC3_PCI_DEVIO_BASE+0x20177

/*DUART Serial Port A*/
#define DU_SIO_UA_THOLD			IOC3_PCI_DEVIO_BASE+0x20178
#define DU_SIO_UA_RHOLD			IOC3_PCI_DEVIO_BASE+0x20178
#define DU_SIO_UA_DIV_LSB		IOC3_PCI_DEVIO_BASE+0x20178
#define DU_SIO_UA_DIV_MSB		IOC3_PCI_DEVIO_BASE+0x20179
#define DU_SIO_UA_IENB			IOC3_PCI_DEVIO_BASE+0x20179
#define DU_SIO_UA_IIDENT		IOC3_PCI_DEVIO_BASE+0x2017A
#define DU_SIO_UA_FIFOC			IOC3_PCI_DEVIO_BASE+0x2017A
#define DU_SIO_UA_LINEC			IOC3_PCI_DEVIO_BASE+0x2017B
#define DU_SIO_UA_MODEMC		IOC3_PCI_DEVIO_BASE+0x2017C
#define DU_SIO_UA_LINES			IOC3_PCI_DEVIO_BASE+0x2017D
#define DU_SIO_UA_MODEMS		IOC3_PCI_DEVIO_BASE+0x2017E
#define DU_SIO_UA_SCRPAD		IOC3_PCI_DEVIO_BASE+0x2017F

#endif /* __IOC3REG_H__ */

