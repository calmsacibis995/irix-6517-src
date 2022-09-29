/**************************************************************************
 * Copyright 1995, Silicon Graphics, Inc.
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
 * Supplement.	Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 **************************************************************************/

#ifndef __PCI_IOC3_H__
#define __PCI_IOC3_H__

/*
 * ioc3.h - IOC3 chip header file
 */

/* Notes:
 * The IOC3 chip is a 32-bit PCI device that provides most of the
 * base I/O for several PCI-based workstations and servers.
 * It includes a 10/100 MBit ethernet MAC, an optimized DMA buffer
 * management, and a store-and-forward buffer RAM.
 * All IOC3 registers are 32 bits wide.
 */
#if LANGUAGE_C
typedef __uint32_t ioc3reg_t;
#endif				/* LANGUAGE_C */

/*
 * PCI Configuration Space Register Address Map, use offset from IOC3 PCI
 * configuration base such that this can be used for multiple IOC3s
 */
#define IOC3_PCI_ID		0x0	/* ID */

#define IOC3_VENDOR_ID_NUM	0x10A9
#define IOC3_DEVICE_ID_NUM	0x0003
#define IOC3_ADDRSPACE_MASK	0xfff00000ULL

#define IOC3_PCI_SCR		0x4	/* Status/Command */
#define IOC3_PCI_REV		0x8	/* Revision */
#define IOC3_PCI_LAT		0xc	/* Latency Timer */
#define IOC3_PCI_ADDR		0x10	/* IOC3 base address */

#define IOC3_PCI_ERR_ADDR_L	0x40	/* Low Error Address */
#define PCI_ERR_ADDR_VLD	(0x1 << 0)
#define PCI_ERR_ADDR_MST_ID_MSK (0xf << 1)
#define PCI_ERR_ADDR_MUL_ERR	(0x1 << 5)
#define PCI_ERR_ADDR_ADDR_MSK	(0x3ffffff << 6)

/* Master IDs contained in PCI_ERR_ADDR_MST_ID_MSK */
#define IOC3_MST_ID_SA_TX		0
#define IOC3_MST_ID_SA_RX		1
#define IOC3_MST_ID_SB_TX		2
#define IOC3_MST_ID_SB_RX		3
#define IOC3_MST_ID_ECPP		4
#define IOC3_MST_ID_ETX_DESC_READ	8
#define IOC3_MST_ID_ETX_BUF1_READ	9
#define IOC3_MST_ID_ETX_BUF2_READ	10
#define IOC3_MST_ID_ERX_DESC_READ	11
#define IOC3_MST_ID_ERX_BUF_WRITE	12

#define IOC3_PCI_ERR_ADDR_H	0x44	/* High Error Address */

/*
 * PCI IO/Mem Space Register Address Map, use offset from IOC3 PCI
 * io/mem base such that this can be used for multiple IOC3s
 */
#define IOC3_SIO_IR	0x01C	/* SuperIO Interrupt Register */

#if _STANDALONE
#define IOC3_SIO_IES	0x020	/* SuperIO Interrupt Enable Set Reg. */
#define IOC3_SIO_IEC	0x024	/* SuperIO Interrupt Enable Clear Reg */
#else
/* these registers are read-only for general kernel code. To modify
 * them use the functions in ioc3.c
 */
#define IOC3_SIO_IES_RO 0x020	/* SuperIO Interrupt Enable Set Reg. */
#define IOC3_SIO_IEC_RO 0x024	/* SuperIO Interrupt Enable Clear Reg */
#endif

#define IOC3_SIO_CR	0x028	/* SuperIO Control Reg */
#define IOC3_INT_OUT	0x02C	/* INT_OUT Reg (realtime interrupt) */
#define IOC3_MCR	0x030	/* MicroLAN Control Reg */
#define IOC3_GPCR_S	0x034	/* GenericPIO Control Set Register */
#define IOC3_GPCR_C	0x038	/* GenericPIO Control Clear Register */
#define IOC3_GPDR	0x03C	/* GenericPIO Data Register */
#define IOC3_GPPR_0	0x040	/* GenericPIO Pin Register */
#define IOC3_GPPR_OFF	0x4
#define IOC3_GPPR(x)	(IOC3_GPPR_0+(x)*IOC3_GPPR_OFF)

/* Parallel Port Registers */
#define IOC3_PPBR_H_A	0x080	/* Par. Port Buffer Upper Address regA */
#define IOC3_PPBR_L_A	0x084	/* Par. Port Buffer Lower Address regA */
#define IOC3_PPCR_A	0x088	/* Parallel Port Count register A */
#define IOC3_PPCR	0x08C	/* Parallel Port DMA control register */
#define IOC3_PPBR_H_B	0x090	/* Par. Port Buffer Upper Address regB */
#define IOC3_PPBR_L_B	0x094	/* Par. Port Buffer Lower Address regB */
#define IOC3_PPCR_B	0x098	/* Parallel Port Count register B */

/* Keyboard and Mouse Registers */
#define IOC3_KM_CSR	0x09C	/* kbd and Mouse Control/Status reg. */
#define IOC3_K_RD	0x0A0	/* kbd Read Data Register */
#define IOC3_M_RD	0x0A4	/* mouse Read Data Register */
#define IOC3_K_WD	0x0A8	/* kbd Write Data Register */
#define IOC3_M_WD	0x0AC	/* mouse Write Data Register */

/* Serial Port Registers */
#define IOC3_SBBR_H	0x0B0	/* Serial Port Ring Buffers Base Reg.H */
#define IOC3_SBBR_L	0x0B4	/* Serial Port Ring Buffers Base Reg.L */

#define IOC3_SSCR_A	0x0B8	/* Serial Port A Control */
#define IOC3_STPIR_A	0x0BC	/* Serial Port A TX Produce */
#define IOC3_STCIR_A	0x0C0	/* Serial Port A TX Consume */
#define IOC3_SRPIR_A	0x0C4	/* Serial Port A RX Produce */
#define IOC3_SRCIR_A	0x0C8	/* Serial Port A RX Consume */
#define IOC3_SRTR_A	0x0CC	/* Serial Port A Receive Timer Reg. */
#define IOC3_SHADOW_A	0x0D0	/* Serial Port A 16550 Shadow Register */

#define IOC3_SSCR_B	0x0D4	/* Serial Port B Control */
#define IOC3_STPIR_B	0x0D8	/* Serial Port B TX Produce */
#define IOC3_STCIR_B	0x0DC	/* Serial Port B TX Consume */
#define IOC3_SRPIR_B	0x0E0	/* Serial Port B RX Produce */
#define IOC3_SRCIR_B	0x0E4	/* Serial Port B RX Consume */
#define IOC3_SRTR_B	0x0E8	/* Serial Port B Receive Timer Reg. */
#define IOC3_SHADOW_B	0x0EC	/* Serial Port B 16550 Shadow Register */

/* Ethernet Registers - see also struct ioc3_eregs defined below */

#define IOC3_REG_OFF	0x0F0	/* Ethernet register base - for ioc3_eregs */
#define IOC3_RAM_OFF	0x40000 /* SSRAM diagsnotic access */

#define IOC3_EMCR	0x0F0	/* Ethernet MAC Control Register */
#define IOC3_EISR	0x0F4	/* Ethernet Interrupt Status Register */
#define IOC3_EIER	0x0F8	/* Ethernet Interrupt Enable Register */

#define IOC3_ERCSR	0x0FC	/* Ethernet RX Control/Status Register */
#define IOC3_ERBR_H	0x100	/* Ethernet RX Base High Address Reg. */
#define IOC3_ERBR_L	0x104	/* Ethernet RX Base Low Addr. Register */
#define IOC3_ERBAR	0x108	/* Ethernet RX Barrier Register */
#define IOC3_ERCIR	0x10C	/* Ethernet RX Consume Index Register */
#define IOC3_ERPIR	0x110	/* Ethernet RX Produce Index Register */
#define IOC3_ERTR	0x114	/* Ethernet RX Timer Register */

#define IOC3_ETCSR	0x118	/* Ethernet TX Control/Status Register */
#define IOC3_ERSR	0x11C	/* Ethernet Random Seed Register */
#define IOC3_ETCDC	0x120	/* Ethernet TX Collision/Deferral Cntr */
#define IOC3_ETBR_H	0x128	/* Ethernet TX Base High Address Reg. */
#define IOC3_ETBR_L	0x12C	/* Ethernet TX Base Low Addr. Register */
#define IOC3_ETCIR	0x130	/* Ethernet TX Consume Index Register */
#define IOC3_ETPIR	0x134	/* Ethernet TX Produce Index Register */

#define IOC3_EBIR	0x124	/* Ethernet Buffer Index Register */
#define IOC3_EMAR_H	0x138	/* Ethernet MAC High Addr Register */
#define IOC3_EMAR_L	0x13C	/* Ethernet MAC Low Addr Register */
#define IOC3_EHAR_H	0x140	/* Ethernet Hashed High Addr Register */
#define IOC3_EHAR_L	0x144	/* Ethernet Hashed Low Addr Register */
#define IOC3_MICR	0x148	/* MII Management Interface Control */
#define IOC3_MIDR	0x14C	/* MMI Management Interface Data Reg */

/* private page address aliases for usermode mapping */
#define IOC3_INT_OUT_P	0x4000	/* INT_OUT Reg */
#define IOC3_SSCR_A_P	0x8000
#define IOC3_STPIR_A_P	0x8004
#define IOC3_STCIR_A_P	0x8008	(read-only)
#define IOC3_SRPIR_A_P	0x800C	(read-only)
#define IOC3_SRCIR_A_P	0x8010
#define IOC3_SRTR_A_P	0x8014
#define IOC3_SHADOW_A_P 0x8018	(read-only)

#define IOC3_SSCR_B_P	0xC000
#define IOC3_STPIR_B_P	0xC004
#define IOC3_STCIR_B_P	0xC008	(read-only)
#define IOC3_SRPIR_B_P	0xC00C	(read-only)
#define IOC3_SRCIR_B_P	0xC010
#define IOC3_SRTR_B_P	0xC014
#define IOC3_SHADOW_B_P 0xC018	(read-only)

#define IOC3_ALIAS_PAGE_SIZE	0x4000

#if IP22
#define UARTA_BASE	0x18
#define UARTB_BASE	0x20
#define PP_BASE		0x10
#else
#define UARTA_BASE	0x178
#define UARTB_BASE	0x170
#define PP_BASE		0x150
#endif

/* Superio Registers (PIO Access) */
#define IOC3_SIO_BASE		0x20000
#define IOC3_SIO_UARTC		(IOC3_SIO_BASE+0x141)	/* UART Config */
#define IOC3_SIO_KBDCG		(IOC3_SIO_BASE+0x142)	/* KBD Config */
#define IOC3_SIO_PP_BASE	(IOC3_SIO_BASE+PP_BASE)		/* Parallel Port */
#define IOC3_SIO_RTC_BASE	(IOC3_SIO_BASE+0x168)	/* Real Time Clock */
#define IOC3_SIO_UB_BASE	(IOC3_SIO_BASE+UARTB_BASE)	/* UART B */
#define IOC3_SIO_UA_BASE	(IOC3_SIO_BASE+UARTA_BASE)	/* UART A */

/* SSRAM Diagnostic Access */
#define IOC3_SSRAM	IOC3_RAM_OFF	/* base of SSRAM diagnostic access */
#define IOC3_SSRAM_LEN	0x40000 /* 256kb (address space size, may not be fully populated) */
#define IOC3_SSRAM_DM	0x0000ffff	/* data mask */
#define IOC3_SSRAM_PM	0x00010000	/* parity mask */

/* bitmasks for PCI_SCR */
#define PCI_SCR_PAR_RESP_EN	0x00000040	/* enb PCI parity checking */
#define PCI_SCR_SERR_EN		0x00000100	/* enable the SERR# driver */
#define PCI_SCR_DROP_MODE_EN	0x00008000	/* drop pios on parity err */
#define PCI_SCR_RX_SERR		(0x1 << 16)
#define PCI_SCR_DROP_MODE	(0x1 << 17)
#define PCI_SCR_SIG_PAR_ERR	(0x1 << 24)
#define PCI_SCR_SIG_TAR_ABRT	(0x1 << 27)
#define PCI_SCR_RX_TAR_ABRT	(0x1 << 28)
#define PCI_SCR_SIG_MST_ABRT	(0x1 << 29)
#define PCI_SCR_SIG_SERR	(0x1 << 30)
#define PCI_SCR_PAR_ERR		(0x1 << 31)

/* bitmasks for IOC3_KM_CSR */
#define KM_CSR_K_WRT_PEND 0x00000001	/* kbd port xmitting or resetting */
#define KM_CSR_M_WRT_PEND 0x00000002	/* mouse port xmitting or resetting */
#define KM_CSR_K_LCB	  0x00000004	/* Line Cntrl Bit for last KBD write */
#define KM_CSR_M_LCB	  0x00000008	/* same for mouse */
#define KM_CSR_K_DATA	  0x00000010	/* state of kbd data line */
#define KM_CSR_K_CLK	  0x00000020	/* state of kbd clock line */
#define KM_CSR_K_PULL_DATA 0x00000040	/* pull kbd data line low */
#define KM_CSR_K_PULL_CLK 0x00000080	/* pull kbd clock line low */
#define KM_CSR_M_DATA	  0x00000100	/* state of ms data line */
#define KM_CSR_M_CLK	  0x00000200	/* state of ms clock line */
#define KM_CSR_M_PULL_DATA 0x00000400	/* pull ms data line low */
#define KM_CSR_M_PULL_CLK 0x00000800	/* pull ms clock line low */
#define KM_CSR_EMM_MODE	  0x00001000	/* emulation mode */
#define KM_CSR_SIM_MODE	  0x00002000	/* clock X8 */
#define KM_CSR_K_SM_IDLE  0x00004000	/* Keyboard is idle */
#define KM_CSR_M_SM_IDLE  0x00008000	/* Mouse is idle */
#define KM_CSR_K_TO	  0x00010000	/* Keyboard trying to send/receive */
#define KM_CSR_M_TO	  0x00020000	/* Mouse trying to send/receive */
#define KM_CSR_K_TO_EN	  0x00040000	/* KM_CSR_K_TO + KM_CSR_K_TO_EN = cause
					   SIO_IR to assert */
#define KM_CSR_M_TO_EN	  0x00080000	/* KM_CSR_M_TO + KM_CSR_M_TO_EN = cause
					   SIO_IR to assert */
#define KM_CSR_K_CLAMP_ONE	0x00100000	/* Pull K_CLK low after rec. one char */
#define KM_CSR_M_CLAMP_ONE	0x00200000	/* Pull M_CLK low after rec. one char */
#define KM_CSR_K_CLAMP_THREE	0x00400000	/* Pull K_CLK low after rec. three chars */
#define KM_CSR_M_CLAMP_THREE	0x00800000	/* Pull M_CLK low after rec. three char */

/* bitmasks for IOC3_K_RD and IOC3_M_RD */
#define KM_RD_DATA_2	0x000000ff	/* 3rd char recvd since last read */
#define KM_RD_DATA_2_SHIFT 0
#define KM_RD_DATA_1	0x0000ff00	/* 2nd char recvd since last read */
#define KM_RD_DATA_1_SHIFT 8
#define KM_RD_DATA_0	0x00ff0000	/* 1st char recvd since last read */
#define KM_RD_DATA_0_SHIFT 16
#define KM_RD_FRAME_ERR_2 0x01000000	/*  framing or parity error in byte 2 */
#define KM_RD_FRAME_ERR_1 0x02000000	/* same for byte 1 */
#define KM_RD_FRAME_ERR_0 0x04000000	/* same for byte 0 */

#define KM_RD_KBD_MSE	0x08000000	/* 0 if from kbd, 1 if from mouse */
#define KM_RD_OFLO	0x10000000	/* 4th char recvd before this read */
#define KM_RD_VALID_2	0x20000000	/* DATA_2 valid */
#define KM_RD_VALID_1	0x40000000	/* DATA_1 valid */
#define KM_RD_VALID_0	0x80000000	/* DATA_0 valid */
#define KM_RD_VALID_ALL (KM_RD_VALID_0|KM_RD_VALID_1|KM_RD_VALID_2)

/* bitmasks for IOC3_K_WD & IOC3_M_WD */
#define KM_WD_WRT_DATA	0x000000ff	/* write to keyboard/mouse port */
#define KM_WD_WRT_DATA_SHIFT 0

/* bitmasks for serial RX status byte */
#define RXSB_OVERRUN	0x01	/* char(s) lost */
#define RXSB_PAR_ERR	0x02	/* parity error */
#define RXSB_FRAME_ERR	0x04	/* framing error */
#define RXSB_BREAK	0x08	/* break character */
#define RXSB_CTS	0x10	/* state of CTS */
#define RXSB_DCD	0x20	/* state of DCD */
#define RXSB_MODEM_VALID 0x40	/* DCD, CTS and OVERRUN are valid */
#define RXSB_DATA_VALID 0x80	/* data byte, FRAME_ERR PAR_ERR & BREAK valid */

/* bitmasks for serial TX control byte */
#define TXCB_INT_WHEN_DONE 0x20 /* interrupt after this byte is sent */
#define TXCB_INVALID	0x00	/* byte is invalid */
#define TXCB_VALID	0x40	/* byte is valid */
#define TXCB_MCR	0x80	/* data<7:0> to modem control register */
#define TXCB_DELAY	0xc0	/* delay data<7:0> mSec */

/* bitmasks for IOC3_SBBR_L */
#define SBBR_L_SIZE	0x00000001	/* 0 == 1KB rings, 1 == 4KB rings */
#define SBBR_L_BASE	0xfffff000	/* lower serial ring base addr */

/* bitmasks for IOC3_SSCR_<A:B> */
#define SSCR_RX_THRESHOLD 0x000001ff	/* hiwater mark */
#define SSCR_TX_TIMER_BUSY 0x00010000	/* TX timer in progress */
#define SSCR_HFC_EN	0x00020000	/* hardware flow control enabled */
#define SSCR_RX_RING_DCD 0x00040000	/* post RX record on delta-DCD */
#define SSCR_RX_RING_CTS 0x00080000	/* post RX record on delta-CTS */
#define SSCR_HIGH_SPD	0x00100000	/* 4X speed */
#define SSCR_DIAG	0x00200000	/* bypass clock divider for sim */
#define SSCR_RX_DRAIN	0x08000000	/* drain RX buffer to memory */
#define SSCR_DMA_EN	0x10000000	/* enable ring buffer DMA */
#define SSCR_DMA_PAUSE	0x20000000	/* pause DMA */
#define SSCR_PAUSE_STATE 0x40000000	/* sets when PAUSE takes effect */
#define SSCR_RESET	0x80000000	/* reset DMA channels */

/* all producer/comsumer pointers are the same bitfield */
#define PROD_CONS_PTR_4K 0x00000ff8	/* for 4K buffers */
#define PROD_CONS_PTR_1K 0x000003f8	/* for 1K buffers */
#define PROD_CONS_PTR_OFF 3

/* bitmasks for IOC3_SRCIR_<A:B> */
#define SRCIR_ARM	0x80000000	/* arm RX timer */

/* bitmasks for IOC3_SRPIR_<A:B> */
#define SRPIR_BYTE_CNT	0x07000000	/* bytes in packer */
#define SRPIR_BYTE_CNT_SHIFT 24

/* bitmasks for IOC3_STCIR_<A:B> */
#define STCIR_BYTE_CNT	0x0f000000	/* bytes in unpacker */
#define STCIR_BYTE_CNT_SHIFT 24

/* bitmasks for IOC3_SHADOW_<A:B> */
#define SHADOW_DR	0x00000001	/* data ready */
#define SHADOW_OE	0x00000002	/* overrun error */
#define SHADOW_PE	0x00000004	/* parity error */
#define SHADOW_FE	0x00000008	/* framing error */
#define SHADOW_BI	0x00000010	/* break interrupt */
#define SHADOW_THRE	0x00000020	/* transmit holding register empty */
#define SHADOW_TEMT	0x00000040	/* transmit shift register empty */
#define SHADOW_RFCE	0x00000080	/* char in RX fifo has an error */
#define SHADOW_DCTS	0x00010000	/* delta clear to send */
#define SHADOW_DDCD	0x00080000	/* delta data carrier detect */
#define SHADOW_CTS	0x00100000	/* clear to send */
#define SHADOW_DCD	0x00800000	/* data carrier detect */
#define SHADOW_DTR	0x01000000	/* data terminal ready */
#define SHADOW_RTS	0x02000000	/* request to send */
#define SHADOW_OUT1	0x04000000	/* 16550 OUT1 bit */
#define SHADOW_OUT2	0x08000000	/* 16550 OUT2 bit */
#define SHADOW_LOOP	0x10000000	/* loopback enabled */

/* bitmasks for IOC3_SRTR_<A:B> */
#define SRTR_CNT	0x00000fff	/* reload value for RX timer */
#define SRTR_CNT_VAL	0x0fff0000	/* current value of RX timer */
#define SRTR_CNT_VAL_SHIFT 16
#define SRTR_HZ		16000	/* SRTR clock frequency */

/* bitmasks for IOC3_SIO_IR, IOC3_SIO_IEC and IOC3_SIO_IES  */
#define SIO_IR_SA_TX_MT		0x00000001	/* Serial port A TX empty */
#define SIO_IR_SA_RX_FULL	0x00000002	/* port A RX buf full */
#define SIO_IR_SA_RX_HIGH	0x00000004	/* port A RX hiwat */
#define SIO_IR_SA_RX_TIMER	0x00000008	/* port A RX timeout */
#define SIO_IR_SA_DELTA_DCD	0x00000010	/* port A delta DCD */
#define SIO_IR_SA_DELTA_CTS	0x00000020	/* port A delta CTS */
#define SIO_IR_SA_INT		0x00000040	/* port A pass-thru intr */
#define SIO_IR_SA_TX_EXPLICIT	0x00000080	/* port A explicit TX thru */
#define SIO_IR_SA_MEMERR	0x00000100	/* port A PCI error */
#define SIO_IR_SB_TX_MT		0x00000200	/* */
#define SIO_IR_SB_RX_FULL	0x00000400	/* */
#define SIO_IR_SB_RX_HIGH	0x00000800	/* */
#define SIO_IR_SB_RX_TIMER	0x00001000	/* */
#define SIO_IR_SB_DELTA_DCD	0x00002000	/* */
#define SIO_IR_SB_DELTA_CTS	0x00004000	/* */
#define SIO_IR_SB_INT		0x00008000	/* */
#define SIO_IR_SB_TX_EXPLICIT	0x00010000	/* */
#define SIO_IR_SB_MEMERR	0x00020000	/* */
#define SIO_IR_PP_INT		0x00040000	/* P port pass-thru intr */
#define SIO_IR_PP_INTA		0x00080000	/* PP context A thru */
#define SIO_IR_PP_INTB		0x00100000	/* PP context B thru */
#define SIO_IR_PP_MEMERR	0x00200000	/* PP PCI error */
#define SIO_IR_KBD_INT		0x00400000	/* kbd/mouse intr */
#define SIO_IR_RT_INT		0x08000000	/* RT output pulse */
#define SIO_IR_GEN_INT1		0x10000000	/* RT input pulse */
#define SIO_IR_GEN_INT_SHIFT	28

/* per device interrupt masks */
#define SIO_IR_SA		(SIO_IR_SA_TX_MT | SIO_IR_SA_RX_FULL | \
				 SIO_IR_SA_RX_HIGH | SIO_IR_SA_RX_TIMER | \
				 SIO_IR_SA_DELTA_DCD | SIO_IR_SA_DELTA_CTS | \
				 SIO_IR_SA_INT | SIO_IR_SA_TX_EXPLICIT | \
				 SIO_IR_SA_MEMERR)
#define SIO_IR_SB		(SIO_IR_SB_TX_MT | SIO_IR_SB_RX_FULL | \
				 SIO_IR_SB_RX_HIGH | SIO_IR_SB_RX_TIMER | \
				 SIO_IR_SB_DELTA_DCD | SIO_IR_SB_DELTA_CTS | \
				 SIO_IR_SB_INT | SIO_IR_SB_TX_EXPLICIT | \
				 SIO_IR_SB_MEMERR)
#define SIO_IR_PP		(SIO_IR_PP_INT | SIO_IR_PP_INTA | \
				 SIO_IR_PP_INTB | SIO_IR_PP_MEMERR)
#define SIO_IR_RT		(SIO_IR_RT_INT | SIO_IR_GEN_INT1)

/* macro to load pending interrupts */
#define IOC3_PENDING_INTRS(mem) (PCI_INW(&((mem)->sio_ir)) & \
				 PCI_INW(&((mem)->sio_ies_ro)))

/* bitmasks for SIO_CR */
#define SIO_CR_SIO_RESET	0x00000001	/* reset the SIO */
#define SIO_CR_SER_A_BASE	0x000000fe	/* DMA poll addr port A */
#define SIO_CR_SER_A_BASE_SHIFT 1
#define SIO_CR_SER_B_BASE	0x00007f00	/* DMA poll addr port B */
#define SIO_CR_SER_B_BASE_SHIFT 8
#define SIO_SR_CMD_PULSE	0x00078000	/* byte bus strobe length */
#define SIO_CR_CMD_PULSE_SHIFT	15
#define SIO_CR_ARB_DIAG		0x00380000	/* cur !enet PCI requet (ro) */
#define SIO_CR_ARB_DIAG_TXA	0x00000000
#define SIO_CR_ARB_DIAG_RXA	0x00080000
#define SIO_CR_ARB_DIAG_TXB	0x00100000
#define SIO_CR_ARB_DIAG_RXB	0x00180000
#define SIO_CR_ARB_DIAG_PP	0x00200000
#define SIO_CR_ARB_DIAG_IDLE	0x00400000	/* 0 -> active request (ro) */

/* bitmasks for INT_OUT */
#define INT_OUT_COUNT	0x0000ffff	/* pulse interval timer */
#define INT_OUT_MODE	0x00070000	/* mode mask */
#define INT_OUT_MODE_0	0x00000000	/* set output to 0 */
#define INT_OUT_MODE_1	0x00040000	/* set output to 1 */
#define INT_OUT_MODE_1PULSE 0x00050000	/* send 1 pulse */
#define INT_OUT_MODE_PULSES 0x00060000	/* send 1 pulse every interval */
#define INT_OUT_MODE_SQW 0x00070000	/* toggle output every interval */
#define INT_OUT_DIAG	0x40000000	/* diag mode */
#define INT_OUT_INT_OUT 0x80000000	/* current state of INT_OUT */

/* time constants for INT_OUT */
#define INT_OUT_NS_PER_TICK (30 * 260)	/* 30 ns PCI clock, divisor=260 */
#define INT_OUT_TICKS_PER_PULSE 3	/* outgoing pulse lasts 3 ticks */
#define INT_OUT_US_TO_COUNT(x)		/* convert uS to a count value */ \
	(((x) * 10 + INT_OUT_NS_PER_TICK / 200) *	\
	 100 / INT_OUT_NS_PER_TICK - 1)
#define INT_OUT_COUNT_TO_US(x)		/* convert count value to uS */ \
	(((x) + 1) * INT_OUT_NS_PER_TICK / 1000)
#define INT_OUT_MIN_TICKS 3	/* min period is width of pulse in "ticks" */
#define INT_OUT_MAX_TICKS INT_OUT_COUNT		/* largest possible count */

/* bitmasks for GPCR */
#define GPCR_DIR	0x000000ff	/* tristate pin input or output */
#define GPCR_DIR_PIN(x) (1<<(x))	/* access one of the DIR bits */
#define GPCR_EDGE	0x000f0000	/* extint edge or level sensitive */
#define GPCR_EDGE_PIN(x) (1<<((x)+15))	/* access one of the EDGE bits */

/* values for GPCR */
#define GPCR_INT_OUT_EN 0x00100000	/* enable INT_OUT to pin 0 */
#define GPCR_MLAN_EN	0x00200000	/* enable MCR to pin 8 */
#define GPCR_DIR_SERA_XCVR 0x00000080	/* Port A Transceiver select enable */
#define GPCR_DIR_SERB_XCVR 0x00000040	/* Port B Transceiver select enable */
#define GPCR_DIR_PHY_RST   0x00000020	/* ethernet PHY reset enable */
#if IP30
#define	GPCR_DIR_MEM_DQM	0x00000010	/* DQM to DIMMs enable */
#define GPCR_DIR_LED1		0x00000002	/* LED 1 enable */
#define GPCR_DIR_LED0		0x00000001	/* LED 0 enable */
#endif				/* IP30 */

/* defs for some of the generic I/O pins */
#define GPCR_PHY_RESET		0x20	/* pin is output to PHY reset */
#define GPCR_UARTB_MODESEL	0x40	/* pin is output to port B mode sel */
#define GPCR_UARTA_MODESEL	0x80	/* pin is output to port A mode sel */

#define GPPR_PHY_RESET_PIN	5	/* GIO pin controlling phy reset */
#define GPPR_UARTB_MODESEL_PIN	6	/* GIO pin controlling uart b mode select */
#define GPPR_UARTA_MODESEL_PIN	7	/* GIO pin controlling uart a mode select */

#if IP30
#define GPCR_MEM_DQM		0x10	/* DQM to DIMMs */
#define	GPCR_OCTANE_CLASSIC	0x04	/* this is octane classic */
#define GPCR_LED0_ON		0x01	/* turn on LED 0 */
#define GPCR_LED1_ON		0x02	/* turn on LED 1 */
#endif				/* IP30 */

#if LANGUAGE_C
/*
 * 10/100 Mbit Ethernet related register layout and bit masks
 *
 * Updated to IOC3 ASIC Specification, Revision 4.4
 */

typedef volatile struct ioc3_eregs {	/* at 0x000F0 */
    ioc3reg_t		    emcr;	/* Ethernet MAC Control */
    ioc3reg_t		    eisr;	/* Ethernet Interrupt Status */
    ioc3reg_t		    eier;	/* Ethernet Interrupt Enable */
    ioc3reg_t		    ercsr;	/* Ethernet RX Control and Status */
    ioc3reg_t		    erbr_h;	/* Ethernet RX Base High Address */
    ioc3reg_t		    erbr_l;	/* Ethernet RX Base Low Address */
    ioc3reg_t		    erbar;	/* Ethernet RX Barrier */
    ioc3reg_t		    ercir;	/* Ethernet RX Consume Index */
    ioc3reg_t		    erpir;	/* Ethernet RX Produce Index */
    ioc3reg_t		    ertr;	/* Ethernet RX Timer Register */
    ioc3reg_t		    etcsr;	/* Ethernet TX Control and Status */
    ioc3reg_t		    ersr;	/* Ethernet Random Seed Register */
    ioc3reg_t		    etcdc;	/* Ethernet TX Collision and Deferral Counters */
    ioc3reg_t		    ebir;	/* Ethernet Buffer Index */
    ioc3reg_t		    etbr_h;	/* Ethernet TX Base High Address */
    ioc3reg_t		    etbr_l;	/* Ethernet TX Base Low Address */
    ioc3reg_t		    etcir;	/* Ethernet TX Consume Index */
    ioc3reg_t		    etpir;	/* Ethernet TX Produce Index */
    ioc3reg_t		    emar_h;	/* Ethernet MAC High Address */
    ioc3reg_t		    emar_l;	/* Ethernet MAC Low Address */
    ioc3reg_t		    ehar_h;	/* Ethernet Hashed High Address */
    ioc3reg_t		    ehar_l;	/* Ethernet Hashed Low Address */
    ioc3reg_t		    micr;	/* MII Mgmt Interface Control */
    ioc3reg_t		    midr_r;	/* MII Mgmt Interface Data (read-only) */
    ioc3reg_t		    midr_w;	/* MII Mgmt Interface Data (write-only) */
} ioc3_eregs_t;

/* serial port register map */
typedef volatile struct ioc3_serialregs {
    ioc3reg_t		    sscr;
    ioc3reg_t		    stpir;
    ioc3reg_t		    stcir;
    ioc3reg_t		    srpir;
    ioc3reg_t		    srcir;
    ioc3reg_t		    srtr;
    ioc3reg_t		    shadow;
} ioc3_sregs_t;

/* SUPERIO uart register map */
typedef volatile struct ioc3_uartregs {
    union {
	char			rbr;	/* read only, DLAB == 0 */
	char			thr;	/* write only, DLAB == 0 */
	char			dll;	/* DLAB == 1 */
    } u1;
    union {
	char			ier;	/* DLAB == 0 */
	char			dlm;	/* DLAB == 1 */
    } u2;
    union {
	char			iir;	/* read only */
	char			fcr;	/* write only */
    } u3;
    char		    iu_lcr;
    char		    iu_mcr;
    char		    iu_lsr;
    char		    iu_msr;
    char		    iu_scr;
} ioc3_uregs_t;

#define iu_rbr u1.rbr
#define iu_thr u1.thr
#define iu_dll u1.dll
#define iu_ier u2.ier
#define iu_dlm u2.dlm
#define iu_iir u3.iir
#define iu_fcr u3.fcr

/* PCI config space register map */
typedef volatile struct ioc3_configregs {
    ioc3reg_t		    pci_id;
    ioc3reg_t		    pci_scr;
    ioc3reg_t		    pci_rev;
    ioc3reg_t		    pci_lat;
    ioc3reg_t		    pci_addr;

    char		    fill[0x40 - 0x10 - 4];

    ioc3reg_t		    pci_err_addr_l;
    ioc3reg_t		    pci_err_addr_h;
} ioc3_cfg_t;

/* SUPERIO register map */
#if IP22
typedef volatile struct ioc3_sioregs {
    char		    fill[0x8];

    char		    pp_fifa;
    char		    pp_cfgb;
    char		    pp_ecr;

    char		    fill2[0x10 - 0x8 - 3];

    char		    pp_data;
    char		    pp_dsr;
    char		    pp_dcr;

    char		    fill3[0x18 - 0x10 - 3];

    struct ioc3_uartregs    uarta;
    struct ioc3_uartregs    uartb;
} ioc3_sioregs_t;
#else
typedef volatile struct ioc3_sioregs {
    char		    fill[0x141];	/* starts at 0x141 */

    char		    uartc;
    char		    kbdcg;

    char		    fill0[0x150 - 0x142 - 1];

    char		    pp_data;
    char		    pp_dsr;
    char		    pp_dcr;

    char		    fill1[0x158 - 0x152 - 1];

    char		    pp_fifa;
    char		    pp_cfgb;
    char		    pp_ecr;

    char		    fill2[0x168 - 0x15a - 1];

    char		    rtcad;
    char		    rtcdat;

    char		    fill3[0x170 - 0x169 - 1];

    struct ioc3_uartregs    uartb;
    struct ioc3_uartregs    uarta;
} ioc3_sioregs_t;
#endif

/* PCI IO/mem space register map */
typedef volatile struct ioc3_memregs {
    ioc3reg_t		    pci_id;
    ioc3reg_t		    pci_scr;
    ioc3reg_t		    pci_rev;
    ioc3reg_t		    pci_lat;
    ioc3reg_t		    pci_addr;
    ioc3reg_t		    pci_err_addr_l;
    ioc3reg_t		    pci_err_addr_h;

    ioc3reg_t		    sio_ir;
#if _STANDALONE
    ioc3reg_t		    sio_ies;
    ioc3reg_t		    sio_iec;
#else
    /* these registers are read-only for general kernel code. To
     * modify them use the functions in ioc3.c
     */
    ioc3reg_t		    sio_ies_ro;
    ioc3reg_t		    sio_iec_ro;
#endif
    ioc3reg_t		    sio_cr;
    ioc3reg_t		    int_out;
    ioc3reg_t		    mcr;
    ioc3reg_t		    gpcr_s;
    ioc3reg_t		    gpcr_c;
    ioc3reg_t		    gpdr;
    ioc3reg_t		    gppr_0;
    ioc3reg_t		    gppr_1;
    ioc3reg_t		    gppr_2;
    ioc3reg_t		    gppr_3;
    ioc3reg_t		    gppr_4;
    ioc3reg_t		    gppr_5;
    ioc3reg_t		    gppr_6;
    ioc3reg_t		    gppr_7;
    ioc3reg_t		    gppr_8;

    char		    fill1[0x80 - 0x60 - 4];

    /* parallel port registers */
    ioc3reg_t		    ppbr_h_a;
    ioc3reg_t		    ppbr_l_a;
    ioc3reg_t		    ppcr_a;
    ioc3reg_t		    ppcr;
    ioc3reg_t		    ppbr_h_b;
    ioc3reg_t		    ppbr_l_b;
    ioc3reg_t		    ppcr_b;

    /* keyboard and mouse registers */
    ioc3reg_t		    km_csr;
    ioc3reg_t		    k_rd;
    ioc3reg_t		    m_rd;
    ioc3reg_t		    k_wd;
    ioc3reg_t		    m_wd;

    /* serial port registers */
    ioc3reg_t		    sbbr_h;
    ioc3reg_t		    sbbr_l;

    struct ioc3_serialregs  port_a;
    struct ioc3_serialregs  port_b;

    /* ethernet registers */
    struct ioc3_eregs	    eregs;

    char		    fill2[0x20000 - 0x00154];

    /* superio registers */
    ioc3_sioregs_t	    sregs;
} ioc3_mem_t;

#define EMCR_DUPLEX		0x00000001
#define EMCR_PROMISC		0x00000002
#define EMCR_PADEN		0x00000004
#define EMCR_RXOFF_MASK		0x000001f8
#define EMCR_RXOFF_SHIFT	3
#define EMCR_RAMPAR		0x00000200
#define EMCR_BADPAR		0x00000800
#define EMCR_BUFSIZ		0x00001000
#define EMCR_TXDMAEN		0x00002000
#define EMCR_TXEN		0x00004000
#define EMCR_RXDMAEN		0x00008000
#define EMCR_RXEN		0x00010000
#define EMCR_LOOPBACK		0x00020000
#define EMCR_ARB_DIAG		0x001c0000
#define EMCR_ARB_DIAG_IDLE	0x00200000
#define EMCR_RST		0x80000000

#define EISR_RXTIMERINT		0x00000001
#define EISR_RXTHRESHINT	0x00000002
#define EISR_RXOFLO		0x00000004
#define EISR_RXBUFOFLO		0x00000008
#define EISR_RXMEMERR		0x00000010
#define EISR_RXPARERR		0x00000020
#define EISR_TXEMPTY		0x00010000
#define EISR_TXRTRY		0x00020000
#define EISR_TXEXDEF		0x00040000
#define EISR_TXLCOL		0x00080000
#define EISR_TXGIANT		0x00100000
#define EISR_TXBUFUFLO		0x00200000
#define EISR_TXEXPLICIT		0x00400000
#define EISR_TXCOLLWRAP		0x00800000
#define EISR_TXDEFERWRAP	0x01000000
#define EISR_TXMEMERR		0x02000000
#define EISR_TXPARERR		0x04000000

#define ERCSR_THRESH_MASK	0x000001ff	/* enet RX threshold */
#define ERCSR_RX_TMR		0x40000000	/* simulation only */
#define ERCSR_DIAG_OFLO		0x80000000	/* simulation only */

#define ERBR_ALIGNMENT		4096
#define ERBR_L_RXRINGBASE_MASK	0xfffff000

#define ERBAR_BARRIER_BIT	0x0100
#define ERBAR_RXBARR_MASK	0xffff0000
#define ERBAR_RXBARR_SHIFT	16

#define ERCIR_RXCONSUME_MASK	0x00000fff

#define ERPIR_RXPRODUCE_MASK	0x00000fff
#define ERPIR_ARM		0x80000000

#define ERTR_CNT_MASK		0x000007ff

#define ETCSR_IPGT_MASK		0x0000007f
#define ETCSR_IPGR1_MASK	0x00007f00
#define ETCSR_IPGR1_SHIFT	8
#define ETCSR_IPGR2_MASK	0x007f0000
#define ETCSR_IPGR2_SHIFT	16
#define ETCSR_NOTXCLK		0x80000000

#define ETCDC_COLLCNT_MASK	0x0000ffff
#define ETCDC_DEFERCNT_MASK	0xffff0000
#define ETCDC_DEFERCNT_SHIFT	16

#define ETBR_ALIGNMENT		(64*1024)
#define ETBR_L_RINGSZ_MASK	0x00000001
#define ETBR_L_RINGSZ128	0
#define ETBR_L_RINGSZ512	1
#define ETBR_L_TXRINGBASE_MASK	0xffffc000

#define ETCIR_TXCONSUME_MASK	0x0000ffff
#define ETCIR_IDLE		0x80000000

#define ETPIR_TXPRODUCE_MASK	0x0000ffff

#define EBIR_TXBUFPROD_MASK	0x0000001f
#define EBIR_TXBUFCONS_MASK	0x00001f00
#define EBIR_TXBUFCONS_SHIFT	8
#define EBIR_RXBUFPROD_MASK	0x007fc000
#define EBIR_RXBUFPROD_SHIFT	14
#define EBIR_RXBUFCONS_MASK	0xff800000
#define EBIR_RXBUFCONS_SHIFT	23

#define MICR_REGADDR_MASK	0x0000001f
#define MICR_PHYADDR_MASK	0x000003e0
#define MICR_PHYADDR_SHIFT	5
#define MICR_READTRIG		0x00000400
#define MICR_BUSY		0x00000800

#define MIDR_DATA_MASK		0x0000ffff

/*
 * Ethernet RX Buffer
 */
struct ioc3_erxbuf {
    __uint32_t		    w0; /* first word (valid,bcnt,cksum) */
    __uint32_t		    err;	/* second word various errors */
    /* next comes n bytes of padding */
    /* then the received ethernet frame itself */
};

#define ERXBUF_IPCKSUM_MASK	0x0000ffff
#define ERXBUF_BYTECNT_MASK	0x07ff0000
#define ERXBUF_BYTECNT_SHIFT	16
#define ERXBUF_V		0x80000000

#define ERXBUF_CRCERR		0x00000001	/* aka RSV15 */
#define ERXBUF_FRAMERR		0x00000002	/* aka RSV14 */
#define ERXBUF_CODERR		0x00000004	/* aka RSV13 */
#define ERXBUF_INVPREAMB	0x00000008	/* aka RSV18 */
#define ERXBUF_LOLEN		0x00007000	/* aka RSV2_0 */
#define ERXBUF_HILEN		0x03ff0000	/* aka RSV12_3 */
#define ERXBUF_MULTICAST	0x04000000	/* aka RSV16 */
#define ERXBUF_BROADCAST	0x08000000	/* aka RSV17 */
#define ERXBUF_LONGEVENT	0x10000000	/* aka RSV19 */
#define ERXBUF_BADPKT		0x20000000	/* aka RSV20 */
#define ERXBUF_GOODPKT		0x40000000	/* aka RSV21 */
#define ERXBUF_CARRIER		0x80000000	/* aka RSV22 */

/*
 * Ethernet TX Descriptor
 */
#define ETXD_DATALEN	104
struct ioc3_etxd {
    __uint32_t		    cmd;	/* command field */
    __uint32_t		    bufcnt;	/* buffer counts field */
    __uint64_t		    p1; /* buffer pointer 1 */
    __uint64_t		    p2; /* buffer pointer 2 */
    uchar_t		    data[ETXD_DATALEN];		/* opt. tx data */
};

#define ETXD_BYTECNT_MASK	0x000007ff	/* total byte count */
#define ETXD_INTWHENDONE	0x00001000	/* intr when done */
#define ETXD_D0V		0x00010000	/* data 0 valid */
#define ETXD_B1V		0x00020000	/* buf 1 valid */
#define ETXD_B2V		0x00040000	/* buf 2 valid */
#define ETXD_DOCHECKSUM		0x00080000	/* insert ip cksum */
#define ETXD_CHKOFF_MASK	0x07f00000	/* cksum byte offset */
#define ETXD_CHKOFF_SHIFT	20

#define ETXD_D0CNT_MASK		0x0000007f
#define ETXD_B1CNT_MASK		0x0007ff00
#define ETXD_B1CNT_SHIFT	8
#define ETXD_B2CNT_MASK		0x7ff00000
#define ETXD_B2CNT_SHIFT	20
#endif				/* LANGUAGE_C */

/*
 * Bytebus device space			  (used by IP30 assembly code)
 */
#define IOC3_BYTEBUS_DEV0		0x80000L
#define IOC3_BYTEBUS_DEV1		0xA0000L
#define IOC3_BYTEBUS_DEV2		0xC0000L
#define IOC3_BYTEBUS_DEV3		0xE0000L

#if LANGUAGE_C

#ifdef _STANDALONE
extern void		init_ioc3(__psunsigned_t, __psunsigned_t);
#elif defined(_KERNEL)		/* _KERNEL */

typedef enum ioc3_subdevs_e {
    ioc3_subdev_ether,
    ioc3_subdev_generic,
    ioc3_subdev_nic,
    ioc3_subdev_kbms,
    ioc3_subdev_ttya,
    ioc3_subdev_ttyb,
    ioc3_subdev_ecpp,
    ioc3_subdev_rt,
    ioc3_nsubdevs
} ioc3_subdev_t;

/* subdevice disable bits,
 * from the standard INFO_LBL_SUBDEVS
 */
#define IOC3_SDB_ETHER		(1<<ioc3_subdev_ether)
#define IOC3_SDB_GENERIC	(1<<ioc3_subdev_generic)
#define IOC3_SDB_NIC		(1<<ioc3_subdev_nic)
#define IOC3_SDB_KBMS		(1<<ioc3_subdev_kbms)
#define IOC3_SDB_TTYA		(1<<ioc3_subdev_ttya)
#define IOC3_SDB_TTYB		(1<<ioc3_subdev_ttyb)
#define IOC3_SDB_ECPP		(1<<ioc3_subdev_ecpp)
#define IOC3_SDB_RT		(1<<ioc3_subdev_rt)

#define IOC3_ALL_SUBDEVS	((1<<ioc3_nsubdevs)-1)

#define IOC3_SDB_SERIAL		(IOC3_SDB_TTYA|IOC3_SDB_TTYB)

#define IOC3_STD_SUBDEVS	IOC3_ALL_SUBDEVS

#define IOC3_INTA_SUBDEVS	IOC3_SDB_ETHER
#define IOC3_INTB_SUBDEVS	(IOC3_SDB_GENERIC|IOC3_SDB_KBMS|IOC3_SDB_SERIAL|IOC3_SDB_ECPP|IOC3_SDB_RT)

extern int		ioc3_subdev_enabled(vertex_hdl_t, ioc3_subdev_t);
extern void		ioc3_subdev_enables(vertex_hdl_t, ulong_t);
extern void		ioc3_subdev_enable(vertex_hdl_t, ioc3_subdev_t);
extern void		ioc3_subdev_disable(vertex_hdl_t, ioc3_subdev_t);

/* macros to read and write the SIO_IEC and SIO_IES registers (see the
 * comments in ioc3.c for details on why this is necessary
 */
#define W_IES	0
#define W_IEC	1
extern void		ioc3_write_ireg(void *, ioc3reg_t, int);

#define IOC3_WRITE_IES(ioc3, val)	ioc3_write_ireg(ioc3, val, W_IES)
#define IOC3_WRITE_IEC(ioc3, val)	ioc3_write_ireg(ioc3, val, W_IEC)

typedef void
ioc3_intr_func_f	(intr_arg_t, ioc3reg_t);

typedef void
ioc3_intr_connect_f	(vertex_hdl_t conn_vhdl,
			 ioc3reg_t,
			 ioc3_intr_func_f *,
			 intr_arg_t info,
			 vertex_hdl_t owner_vhdl,
			 vertex_hdl_t intr_dev_vhdl,
			 int (*)(intr_arg_t));

typedef void
ioc3_intr_disconnect_f	(vertex_hdl_t conn_vhdl,
			 ioc3reg_t,
			 ioc3_intr_func_f *,
			 intr_arg_t info,
			 vertex_hdl_t owner_vhdl);

ioc3_intr_disconnect_f	ioc3_intr_disconnect;
ioc3_intr_connect_f	ioc3_intr_connect;

extern int		ioc3_is_console(vertex_hdl_t conn_vhdl);

extern void		ioc3_mlreset(ioc3_cfg_t *, ioc3_mem_t *);

extern intr_func_f	ioc3_intr;

extern ioc3_mem_t      *ioc3_mem_ptr(void *ioc3_fastinfo);

typedef ioc3_intr_func_f *ioc3_intr_func_t;

#endif				/* _STANDALONE */
#endif				/* LANGUAGE_C */
#endif				/* __PCI_IOC3_H__ */
