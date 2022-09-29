/* m68340.h - Motorola MC68340 CPU control registers */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,06aug92,caf	 added TY_CO_DEV and function declarations for 5.1 upgrade.
		 for 5.0.x compatibility, define INCLUDE_TY_CO_DRV_50 when
		 including this header.
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01a,03sep91,jcf	 based on m68332.h.
*/

/*
This file contains I/O address and related constants for the MC68340.
*/

#ifndef __INCm68340h
#define __INCm68340h

#ifdef __cplusplus
extern "C" {
#endif

#ifndef	_ASMLANGUAGE
#ifndef	INCLUDE_TY_CO_DRV_50

#include "tylib.h"

typedef struct          /* TY_CO_DEV */
    {
    TY_DEV tyDev;
    char numChannels;	/* number of serial channel to support */
    BOOL created;	/* true if this device has really been created */
    UINT8 *dr;		/* data port */
    UINT8 *csr;		/* clock select register */
    UINT8 *cr;		/* control reg */
    UINT8 *isr;		/* interrupt status/mask */
    UINT8 *acr;		/* auxiliary control */
    UINT8 *mr1;		/* mode 1 */
    UINT8 *mr2;		/* mode 2 */
    UINT8 *sop;
    UINT8 *ier;
    UINT8 rem;		/* bit for receive-enable mask */
    UINT8 tem;		/* bit for transmit-enable mask */
    } TY_CO_DEV;

#endif	/* INCLUDE_TY_CO_DRV_50 */
#endif	/* _ASMLANGUAGE */

#define M340_ADRS(offset)	((int)M340_BASE_ADRS + offset)

/* MC68340 parameter register addresses */

#define M340_SIM_MCR		((UINT16 *) M340_ADRS(0x000))
#define M340_SIM_SYNCR		((UINT16 *) M340_ADRS(0x004))
#define M340_SIM_AVR		((UINT8  *) M340_ADRS(0x006))
#define M340_SIM_RSR		((UINT8  *) M340_ADRS(0x007))
#define M340_SIM_PORTA		((UINT8  *) M340_ADRS(0x011))
#define M340_SIM_DDRA		((UINT8  *) M340_ADRS(0x013))
#define M340_SIM_PPARA1		((UINT8  *) M340_ADRS(0x015))
#define M340_SIM_PPARA2		((UINT8  *) M340_ADRS(0x017))
#define M340_SIM_PORTB		((UINT8  *) M340_ADRS(0x019))
#define M340_SIM_DDRB		((UINT8  *) M340_ADRS(0x01d))
#define M340_SIM_PPARB		((UINT8  *) M340_ADRS(0x01f))
#define M340_SIM_SWIV		((UINT8  *) M340_ADRS(0x020))
#define M340_SIM_SYPCR		((UINT8  *) M340_ADRS(0x021))
#define M340_SIM_PICR		((UINT16 *) M340_ADRS(0x022))
#define M340_SIM_PITR		((UINT16 *) M340_ADRS(0x024))
#define M340_SIM_SWSR		((UINT8  *) M340_ADRS(0x027))
#define M340_SIM_CSAM0		((UINT32 *) M340_ADRS(0x040))
#define M340_SIM_CSBAR0		((UINT32 *) M340_ADRS(0x044))
#define M340_SIM_CSAM1		((UINT32 *) M340_ADRS(0x048))
#define M340_SIM_CSBAR1		((UINT32 *) M340_ADRS(0x04c))
#define M340_SIM_CSAM2		((UINT32 *) M340_ADRS(0x050))
#define M340_SIM_CSBAR2		((UINT32 *) M340_ADRS(0x054))
#define M340_SIM_CSAM3		((UINT32 *) M340_ADRS(0x058))
#define M340_SIM_CSBAR3		((UINT32 *) M340_ADRS(0x05c))
#define M340_TMR1_MCR		((UINT16 *) M340_ADRS(0x600))
#define M340_TMR1_IR		((UINT16 *) M340_ADRS(0x604))
#define M340_TMR1_CR		((UINT16 *) M340_ADRS(0x606))
#define M340_TMR1_SR		((UINT16 *) M340_ADRS(0x608))
#define M340_TMR1_CNTR		((UINT16 *) M340_ADRS(0x60a))
#define M340_TMR1_PREL1		((UINT16 *) M340_ADRS(0x60c))
#define M340_TMR1_PREL2		((UINT16 *) M340_ADRS(0x60e))
#define M340_TMR1_COM		((UINT16 *) M340_ADRS(0x610))
#define M340_TMR2_MCR		((UINT16 *) M340_ADRS(0x640))
#define M340_TMR2_IR		((UINT16 *) M340_ADRS(0x644))
#define M340_TMR2_CR		((UINT16 *) M340_ADRS(0x646))
#define M340_TMR2_SR		((UINT16 *) M340_ADRS(0x648))
#define M340_TMR2_CNTR		((UINT16 *) M340_ADRS(0x64a))
#define M340_TMR2_PREL1		((UINT16 *) M340_ADRS(0x64c))
#define M340_TMR2_PREL2		((UINT16 *) M340_ADRS(0x64e))
#define M340_TMR2_COM		((UINT16 *) M340_ADRS(0x650))
#define M340_DUART_MCR_MSB	((UINT8  *) M340_ADRS(0x700))
#define M340_DUART_MCR_LSB	((UINT8  *) M340_ADRS(0x701))
#define M340_DUART_ILR		((UINT8  *) M340_ADRS(0x704))
#define M340_DUART_IVR		((UINT8  *) M340_ADRS(0x705))
#define M340_DUART_MR1A		((UINT8  *) M340_ADRS(0x710))
#define M340_DUART_SRA		((UINT8  *) M340_ADRS(0x711))
#define M340_DUART_CSRA		((UINT8  *) M340_ADRS(0x711))
#define M340_DUART_CRA		((UINT8  *) M340_ADRS(0x712))
#define M340_DUART_RBA		((UINT8  *) M340_ADRS(0x713))
#define M340_DUART_TBA		((UINT8  *) M340_ADRS(0x713))
#define M340_DUART_IPCR		((UINT8  *) M340_ADRS(0x714))
#define M340_DUART_ACR		((UINT8  *) M340_ADRS(0x714))
#define M340_DUART_ISR		((UINT8  *) M340_ADRS(0x715))
#define M340_DUART_IER		((UINT8  *) M340_ADRS(0x715))
#define M340_DUART_MR1B		((UINT8  *) M340_ADRS(0x718))
#define M340_DUART_SRB		((UINT8  *) M340_ADRS(0x719))
#define M340_DUART_CSRB		((UINT8  *) M340_ADRS(0x719))
#define M340_DUART_CRB		((UINT8  *) M340_ADRS(0x71a))
#define M340_DUART_RBB		((UINT8  *) M340_ADRS(0x71b))
#define M340_DUART_TBB		((UINT8  *) M340_ADRS(0x71b))
#define M340_DUART_IP		((UINT8  *) M340_ADRS(0x71d))
#define M340_DUART_OPCR		((UINT8  *) M340_ADRS(0x71d))
#define M340_DUART_SOP		((UINT8  *) M340_ADRS(0x71e))
#define M340_DUART_ROP		((UINT8  *) M340_ADRS(0x71f))
#define M340_DUART_MR2A		((UINT8  *) M340_ADRS(0x720))
#define M340_DUART_MR2B		((UINT8  *) M340_ADRS(0x721))
#define M340_DMA1_MCR		((UINT16 *) M340_ADRS(0x780))
#define M340_DMA1_IR		((UINT16 *) M340_ADRS(0x784))
#define M340_DMA1_CCR		((UINT16 *) M340_ADRS(0x788))
#define M340_DMA1_CSR		((UINT8  *) M340_ADRS(0x78a))
#define M340_DMA1_FCR		((UINT8  *) M340_ADRS(0x78b))
#define M340_DMA1_SAR		((UINT32 *) M340_ADRS(0x78c))
#define M340_DMA1_DAR		((UINT32 *) M340_ADRS(0x790))
#define M340_DMA1_BTC		((UINT32 *) M340_ADRS(0x794))
#define M340_DMA2_MCR		((UINT16 *) M340_ADRS(0x7a0))
#define M340_DMA2_IR		((UINT16 *) M340_ADRS(0x7a4))
#define M340_DMA2_CCR		((UINT16 *) M340_ADRS(0x7a8))
#define M340_DMA2_CSR		((UINT8  *) M340_ADRS(0x7aa))
#define M340_DMA2_FCR		((UINT8  *) M340_ADRS(0x7ab))
#define M340_DMA2_SAR		((UINT32 *) M340_ADRS(0x7ac))
#define M340_DMA2_DAR		((UINT32 *) M340_ADRS(0x7b0))
#define M340_DMA2_BTC		((UINT32 *) M340_ADRS(0x7b4))


/* SIM - Register definitions for the System Integration Module */

/* SIM_MCR - Module Configuration Register */

#define SIM_MCR_SUPV		0x0080	/* supervisor/unrestricted data space */
#define SIM_MCR_X		0x0000	/* no show cycle, ext. arbitration */
#define SIM_MCR_SH		0x0100	/* show cycle enabled, no ext. arb. */
#define SIM_MCR_SH_X		0x0200	/* show cycle enabled, ext. arb. */
#define SIM_MCR_SH_X_BG		0x0300	/* show cycle/ext arb; int halt w/ BG */
#define SIM_MCR_FIRQ		0x1000	/* full interrupt request mode */
#define SIM_MCR_FRZ0		0x2000	/* freeze periodic interrupt timer */
#define SIM_MCR_FRZ1		0x4000	/* freeze software enable */

/* SIM_SYNCR - Clock Synthesis Control Register */

#define SIM_SYNCR_STEXT		0x0001	/* stop mode external clock */
#define SIM_SYNCR_STSIM		0x0002	/* stop mode system integration clock */
#define SIM_SYNCR_RSTEN		0x0004	/* reset enable */
#define SIM_SYNCR_SLOCK		0x0008	/* synthesizer lock */
#define SIM_SYNCR_SLIMP		0x0010	/* limp mode */
#define SIM_SYNCR_Y_MASK 	0x3f00	/* Y - frequency control bits */
#define SIM_SYNCR_X    		0x4000	/* X - frequency control bit */
#define SIM_SYNCR_W    		0x8000	/* W - frequency control bit */

/* SIM_AVR - Auto Vector Register */

#define SIM_AVR_LVL1		0x02	/* auto vector on IRQ1 */
#define SIM_AVR_LVL2		0x04	/* auto vector on IRQ2 */
#define SIM_AVR_LVL3		0x08	/* auto vector on IRQ3 */
#define SIM_AVR_LVL4		0x10	/* auto vector on IRQ4 */
#define SIM_AVR_LVL5		0x20	/* auto vector on IRQ5 */
#define SIM_AVR_LVL6		0x40	/* auto vector on IRQ6 */
#define SIM_AVR_LVL7		0x80	/* auto vector on IRQ7 */

/* SIM_RSR - Reset Status Register */

#define SIM_RSR_SYS		0x02	/* system reset (CPU reset) */
#define SIM_RSR_LOC		0x04	/* loss of clock reset */
#define SIM_RSR_HLT		0x10	/* halt monitor reset */
#define SIM_RSR_SW		0x20	/* software watchdog reset */
#define SIM_RSR_POW		0x40	/* power up reset */
#define SIM_RSR_EXT		0x80	/* external reset */

/* M340_SIM_PPARA1 - Port A Pin Assignment Register 1 */

#define SIM_PPARA1_PRTA0	0x01	/* port function */
#define SIM_PPARA1_PRTA1	0x02	/* port function */
#define SIM_PPARA1_PRTA2	0x04	/* port function */
#define SIM_PPARA1_PRTA3	0x08	/* port function */
#define SIM_PPARA1_PRTA4	0x10	/* port function */
#define SIM_PPARA1_PRTA5	0x20	/* port function */
#define SIM_PPARA1_PRTA6	0x40	/* port function */
#define SIM_PPARA1_PRTA7	0x80	/* port function */

/* M340_SIM_PPARA2 - Port A Pin Assignment Register 2 */

#define SIM_PPARA2_IACK1	0x02	/* iack function */
#define SIM_PPARA2_IACK2	0x04	/* iack function */
#define SIM_PPARA2_IACK3	0x08	/* iack function */
#define SIM_PPARA2_IACK4	0x10	/* iack function */
#define SIM_PPARA2_IACK5	0x20	/* iack function */
#define SIM_PPARA2_IACK6	0x40	/* iack function */
#define SIM_PPARA2_IACK7	0x80	/* iack function */

/* M340_SIM_PPARB - Port B Pin Assignment Register */

#define SIM_PPARB_MODCK		0x01	/* modck function */
#define SIM_PPARB_IRQ1		0x02	/* irq function (SIM_MCR:FIRQ=1) */
#define SIM_PPARB_IRQ2		0x04	/* irq function (SIM_MCR:FIRQ=1) */
#define SIM_PPARB_IRQ3		0x08	/* irq function */
#define SIM_PPARB_IRQ4		0x10	/* irq function (SIM_MCR:FIRQ=1) */
#define SIM_PPARB_IRQ5		0x20	/* irq function */
#define SIM_PPARB_IRQ6		0x40	/* irq function */
#define SIM_PPARB_IRQ7		0x80	/* irq function */

/* SIM_SYPCR - System Protection Control (Write Once Only) */

#define SIM_SYPCR_BMT_64	0x00	/* 64 clk cycle bus monitor timeout */
#define SIM_SYPCR_BMT_32	0x01	/* 32 clk cycle bus monitor timeout */
#define SIM_SYPCR_BMT_16	0x02	/* 16 clk cycle bus monitor timeout */
#define SIM_SYPCR_BMT_8		0x03	/* 8 clk cycle bus monitor timeout */
#define SIM_SYPCR_BME		0x04	/* bus monitor enable */
#define SIM_SYPCR_HME		0x08	/* halt monitor enable */
#define SIM_SYPCR_SWT_9		0x00	/* 2^9  xtal cycles (SIM_PITR:SWP=0) */
#define SIM_SYPCR_SWT_11 	0x10	/* 2^11 xtal cycles (SIM_PITR:SWP=0) */
#define SIM_SYPCR_SWT_13	0x20	/* 2^13 xtal cycles (SIM_PITR:SWP=0) */
#define SIM_SYPCR_SWT_15	0x30	/* 2^15 xtal cycles (SIM_PITR:SWP=0) */
#define SIM_SYPCR_SWT_18	0x00	/* 2^18 xtal cycles (SIM_PITR:SWP=1) */
#define SIM_SYPCR_SWT_20	0x10	/* 2^20 xtal cycles (SIM_PITR:SWP=1) */
#define SIM_SYPCR_SWT_22	0x20	/* 2^22 xtal cycles (SIM_PITR:SWP=1) */
#define SIM_SYPCR_SWT_24	0x30	/* 2^24 xtal cycles (SIM_PITR:SWP=1) */
#define SIM_SYPCR_SWRI		0x40	/* software watchdog resets CPU */
#define SIM_SYPCR_SWE		0x80	/* software watchdog enable */

/* SIM_PICR - Periodic Interrupt Control Register */

#define SIM_PICR_PIV_MASK	0x00ff	/* periodic interrupt vector */
#define SIM_PICR_PIRQL_MASK	0x0700	/* periodic interrupt request level */

/* SIM_PITR - Periodic Interrupt Timer */

#define SIM_PITR_PITR_MASK	0x00ff	/* periodic interrupt timing register */
#define SIM_PITR_PTP		0x0100	/* periodic timer prescale bit */
#define SIM_PITR_SWP		0x0200	/* software watchdog prescale bit */

/* SIM_SWSR - Software Service Register */

#define SIM_SWSR_ACK1		0x55	/* software watchdog ack. part 1 */
#define SIM_SWSR_ACK2		0xaa	/* software watchdog ack. part 2 */

/* SIM_CSAM - Chip Select Address Mask Register */

#define SIM_CSAM_WORD		0x00000001	/* 16b port dsack generation */
#define SIM_CSAM_BYTE		0x00000002	/* 8b port dsack generation */
#define SIM_CSAM_WAIT_EXT	0x00000003	/* external dsack generation */
#define SIM_CSAM_WAIT0		0x00000000	/* dsack with 0 wait states */
#define SIM_CSAM_WAIT1		0x00000004	/* dsack with 1 wait states */
#define SIM_CSAM_WAIT2		0x00000008	/* dsack with 2 wait states */
#define SIM_CSAM_WAIT3		0x0000000c	/* dsack with 3 wait states */
#define SIM_CSAM_FC_MASK	0x000000f0	/* function code to match */
#define SIM_CSAM_ADRS_MASK	0xffffff00	/* address to match */

/* SIM_CSBAR - Chip Select Base Address Register */

#define SIM_CSBAR_VALID		0x00000001	/* register is valid */
#define SIM_CSBAR_FTE		0x00000004	/* fast termination enabled */
#define SIM_CSBAR_WP		0x00000008	/* write protect */
#define SIM_CSBAR_FC		0x000000f0	/* function code to match */
#define SIM_CSBAR_ADRS		0xffffff00	/* address to match */


/* DUART - Register definitions for the DUART serial module */

/* M340_DUART_MCR - Module Configuration Register */

#define DUART_MCR_SUPV		0x0080		/* supervisor reg access only */
#define DUART_MCR_ICCS		0x1000		/* SCLK is input capture clk */
#define DUART_MCR_FREEZE	0x4000		/* execution freeze */
#define DUART_MCR_STOP		0x8000		/* stops the serial clocks */

/* M340_DUART_MR1 - Mode Register 1 (a,b) */

#define	DUART_MR1_5BITS		0x00		/* 5 bits */
#define	DUART_MR1_6BITS		0x01		/* 6 bits */
#define	DUART_MR1_7BITS		0x02		/* 7 bits */
#define	DUART_MR1_8BITS		0x03		/* 8 bits */
#define	DUART_MR1_PAR_EVEN	0x00		/* even parity */
#define	DUART_MR1_PAR_ODD	0x04		/* odd parity */
#define	DUART_MR1_PAR_SPACE	0x08		/* space parity */
#define	DUART_MR1_PAR_MARK	0x0c		/* mark parity */
#define	DUART_MR1_PAR_NONE	0x10		/* no parity */
#define	DUART_MR1_MDROP_DATA	0x18		/* multi_drop data */
#define	DUART_MR1_MDROP_ADRS	0x1c		/* multi_drop address */
#define	DUART_MR1_ERR		0x20		/* 0 = char, 1 = block */
#define	DUART_MR1_RX_FULL	0x40		/* 0 = RxRDY, 1 = FFULL */
#define	DUART_MR1_RX_RTS	0x80		/* 0 = no, 1 = yes */

/* M340_DUART_SR - Status Register (a,b) */

#define	DUART_SR_RXRDY		0x01		/* 0 = no, 1 = yes */
#define	DUART_SR_FFULL		0x02		/* 0 = no, 1 = yes */
#define	DUART_SR_TXRDY		0x04		/* 0 = no, 1 = yes */
#define	DUART_SR_TXEMP		0x08		/* 0 = no, 1 = yes */
#define	DUART_SR_OVERRUN_ERR	0x10		/* 0 = no, 1 = yes */
#define	DUART_SR_PARITY_ERR	0x20		/* 0 = no, 1 = yes */
#define	DUART_SR_FRAMING_ERR	0x40		/* 0 = no, 1 = yes */
#define	DUART_SR_RXD_BREAK	0x80		/* 0 = no, 1 = yes */

/* M340_DUART_CSR - Clock Select Register (a,b) */

#define	DUART_CSR_TX_CLK_75	0x00		/* 75 */
#define	DUART_CSR_TX_CLK_110	0x01		/* 110 */
#define	DUART_CSR_TX_CLK_134_5	0x02		/* 134.5 */
#define	DUART_CSR_TX_CLK_150	0x03		/* 150 */
#define	DUART_CSR_TX_CLK_300	0x04		/* 300 */
#define	DUART_CSR_TX_CLK_600	0x05		/* 600 */
#define	DUART_CSR_TX_CLK_1200	0x06		/* 1200 */
#define	DUART_CSR_TX_CLK_2000	0x07		/* 2000 */
#define	DUART_CSR_TX_CLK_2400	0x08		/* 2400 */
#define	DUART_CSR_TX_CLK_4800	0x09		/* 4800 */
#define	DUART_CSR_TX_CLK_1800	0x0a		/* 1800 */
#define	DUART_CSR_TX_CLK_9600	0x0b		/* 9600 */
#define	DUART_CSR_TX_CLK_19200	0x0c		/* 19200 */
#define	DUART_CSR_TX_CLK_38400	0x0d		/* 38400 */
#define	DUART_CSR_TX_CLK_SCLK16	0x0e		/* SCLK/16 */
#define	DUART_CSR_TX_CLK_SCLK	0x0f		/* SCLK */
#define	DUART_CSR_RX_CLK_75	0x00		/* 75 */
#define	DUART_CSR_RX_CLK_110	0x10		/* 110 */
#define	DUART_CSR_RX_CLK_134_5	0x20		/* 134.5 */
#define	DUART_CSR_RX_CLK_150	0x30		/* 150 */
#define	DUART_CSR_RX_CLK_300	0x40		/* 300 */
#define	DUART_CSR_RX_CLK_600	0x50		/* 600 */
#define	DUART_CSR_RX_CLK_1200	0x60		/* 1200 */
#define	DUART_CSR_RX_CLK_2000	0x70		/* 2000 */
#define	DUART_CSR_RX_CLK_2400	0x80		/* 2400 */
#define	DUART_CSR_RX_CLK_4800	0x90		/* 4800 */
#define	DUART_CSR_RX_CLK_1800	0xa0		/* 1800 */
#define	DUART_CSR_RX_CLK_9600	0xb0		/* 9600 */
#define	DUART_CSR_RX_CLK_19200	0xc0		/* 19200 */
#define	DUART_CSR_RX_CLK_38400	0xd0		/* 38400 */
#define	DUART_CSR_RX_CLK_SCLK16	0xe0		/* SCLK/16 */
#define	DUART_CSR_RX_CLK_SCLK	0xf0		/* SCLK */

/* M340_DUART_CR - Command Register (a,b) */

#define	DUART_CR_RX_ENABLE	0x01		/* 0 = no, 1 = yes */
#define	DUART_CR_RX_DISABLE	0x02		/* 0 = no, 1 = yes */
#define	DUART_CR_TX_ENABLE	0x04		/* 0 = no, 1 = yes */
#define	DUART_CR_TX_DISABLE	0x08		/* 0 = no, 1 = yes */
#define	DUART_CR_RST_MR_PTR	0x10		/* reset mr pointer command */
#define	DUART_CR_RST_RX		0x20		/* reset receiver command */
#define	DUART_CR_RST_TX		0x30		/* reset transmitter command */
#define	DUART_CR_RST_ERR_STS	0x40		/* reset error status command */
#define	DUART_CR_RST_BRK_INT	0x50		/* reset break int. command */
#define	DUART_CR_STR_BREAK	0x60		/* start break command */
#define	DUART_CR_STP_BREAK	0x70		/* stop break command */
#define	DUART_CR_RTS_ON		0x80		/* assert rts */
#define	DUART_CR_RTS_OFF	0x90		/* deassert rts */

/* M340_DUART_IPCR - Input Port Change Register */

#define	DUART_IPCR_CTSA		0x01		/* 0 = low, 1 = high */
#define	DUART_IPCR_CTSB		0x02		/* 0 = low, 1 = high */
#define	DUART_IPCR_COSA		0x10		/* 1 = look for cos */
#define	DUART_IPCR_COSB		0x20		/* 1 = look for cos */

/* M340_DUART_ACR - Auxiliary Control Register */

#define	DUART_ACR_IECA		0x01		/* interrupt enable change a */
#define	DUART_ACR_IECB		0x02		/* interrupt enable change b */
#define	DUART_ACR_BRG		0x80		/* baudrate set select */

/* M340_DUART_ISR - Interrupt Status/Mask Register */

#define	DUART_ISR_TX_RDY_A	0x01		/* 0 = no, 1 = yes */
#define	DUART_ISR_RX_RDY_A	0x02		/* 0 = no, 1 = yes */
#define	DUART_ISR_BREAK_A	0x04		/* 0 = no, 1 = yes */
#define	DUART_ISR_XTAL_UNSTABLE	0x08		/* 0 = no, 1 = yes */
#define	DUART_ISR_TX_RDY_B	0x10		/* 0 = no, 1 = yes */
#define	DUART_ISR_RX_RDY_B	0x20		/* 0 = no, 1 = yes */
#define	DUART_ISR_BREAK_B	0x40		/* 0 = no, 1 = yes */
#define	DUART_ISR_INPUT_DELTA	0x80		/* 0 = no, 1 = yes */

/* M340_DUART_IP - Input Port Register */

#define	DUART_IP_CTSA		0x01		/* 0 = low, 1 = high */
#define	DUART_IP_CTSB		0x02		/* 0 = low, 1 = high */

/* M340_DUART_OPCR - Output Port Control Register */

#define	DUART_OPCR_RTSA		0x00		/* 0 = OP[0], 1 = RTSA */
#define	DUART_OPCR_RTSB		0x01		/* 0 = OP[1], 1 = RTSB */
#define	DUART_OPCR_RXRDY	0x10		/* 0 = OP[4], 1 = RxRDYA */
#define	DUART_OPCR_TXRDY	0x40		/* 0 = OP[6], 1 = TxRDYA */

/* M340_DUART_MR2 - Mode Register 2 (a,b) */

#define	DUART_MR2_STOP_BITS_1	0x07		/* 1 */
#define	DUART_MR2_STOP_BITS_2	0x0f		/* 2 */
#define	DUART_MR2_CTS_ENABLE	0x10		/* 0 = no, 1 = yes */
#define	DUART_MR2_TX_RTS	0x20		/* 0 = no, 1 = yes */
#define	DUART_MR2_NORM		0x00		/* normal */
#define	DUART_MR2_ECHO		0x40		/* auto echo */
#define	DUART_MR2_LOOPBACK	0x80		/* local loop */
#define	DUART_MR2_R_LOOPBACK	0xc0		/* remote loop */


/* TIMER - Register definitions for the general purpose timer module */

/* M340_TMR_MCR - Module Configuration Register */

#define TMR_MCR_SUPV		0x0080		/* supervisor reg access only */
#define TMR_MCR_FREEZE		0x4000		/* execution freeze */
#define TMR_MCR_STOP		0x8000		/* stops the serial clocks */

/* M340_TMR_CR - Control Register */

#define TMR_CR_TOGGLE		0x0001		/* tout toggles */
#define TMR_CR_ZERO		0x0002		/* tout goes to zero */
#define TMR_CR_ONE		0x0003		/* tout goes to one */
#define TMR_CR_CAPTURE		0x0000		/* input/output capture mode */
#define TMR_CR_SQUARE		0x0004		/* square wave generator */
#define TMR_CR_V_SQUARE		0x0008		/* variable duty square wave */
#define TMR_CR_V_SHOT		0x000c		/* variable width single shot */
#define TMR_CR_PWM		0x0010		/* pulse width measurement */
#define TMR_CR_PM		0x0014		/* period measurement */
#define TMR_CR_EVT		0x0018		/* event count */
#define TMR_CR_BYPASS		0x001c		/* bypass */
#define TMR_CR_X2		0x0020		/* divide by 2 */
#define TMR_CR_X4		0x0040		/* divide by 4 */
#define TMR_CR_X8		0x0060		/* divide by 8 */
#define TMR_CR_X16		0x0080		/* divide by 16 */
#define TMR_CR_X32		0x00a0		/* divide by 32 */
#define TMR_CR_X64		0x00c0		/* divide by 64 */
#define TMR_CR_X128		0x00e0		/* divide by 128 */
#define TMR_CR_X256		0x0000		/* divide by 256 */
#define TMR_CR_CLK_TIN		0x0100		/* clock source is tin */
#define TMR_CR_CPE		0x0200		/* clock prescaler enable */
#define TMR_CR_PSE		0x0400		/* prescaler select enable */
#define TMR_CR_TGE		0x0800		/* timer gate enable */
#define TMR_CR_TC		0x1000		/* tc enabled */
#define TMR_CR_TG		0x2000		/* tg enabled */
#define TMR_CR_TO		0x4000		/* to enabled */
#define TMR_CR_SWR		0x8000		/* software reset */

/* M340_TMR_SR - Status/Prescaler Register */

#define TMR_SR_COM		0x0100		/* counter value matches com */
#define TMR_SR_OUT		0x0200		/* output level */
#define TMR_SR_ON		0x0400		/* counter enabled */
#define TMR_SR_TGL		0x0800		/* tgate level */
#define TMR_SR_TC		0x1000		/* timer compare interrupt */
#define TMR_SR_TG		0x2000		/* timer gate interrupt */
#define TMR_SR_TO		0x4000		/* timeout interrupt */
#define TMR_SR_INT		0x8000		/* interrupt request */


/* DMA - Register definitions for the DMA module */

/* DMA_MCR - Module Configuration Register */

#define DMA_MCR_MAID_MASK	0x0070		/* master arbitration id */
#define DMA_MCR_SUPV		0x0080		/* supervisor reg access only */
#define DMA_MCR_ISM_MASK	0x0700		/* int service mask */
#define DMA_MCR_SE		0x1000		/* single address enable */
#define DMA_MCR_FREEZE		0x4000		/* execution freeze */
#define DMA_MCR_STOP		0x8000		/* stops the serial clocks */

/* DMA_CCR - Channel Control Register */

#define DMA_CCR_START		0x0001		/* start dma transfer */
#define DMA_CCR_SNGL		0x0002		/* single address transfer */
#define DMA_CCR_BB25		0x0000		/* 25% bus utilization */
#define DMA_CCR_BB50		0x0004		/* 50% bus utilization */
#define DMA_CCR_BB75		0x0008		/* 75% bus utilization */
#define DMA_CCR_BB100		0x000c		/* 100% bus utilization */
#define DMA_CCR_INT_REQ		0x0000		/* internal request */
#define DMA_CCR_EXT_REQ		0x0020		/* external request */
#define DMA_CCR_CYC_STEAL	0x0030		/* external cycle steal */
#define DMA_CCR_DS_LONG		0x0000		/* long destination */
#define DMA_CCR_DS_WORD		0x0040		/* word destination */
#define DMA_CCR_DS_BYTE		0x0080		/* byte destination */
#define DMA_CCR_SS_LONG		0x0000		/* long source */
#define DMA_CCR_SS_WORD		0x0100		/* word source */
#define DMA_CCR_SS_BYTE		0x0200		/* byte source */
#define DMA_CCR_DAPI		0x0400		/* increment destination ptr */
#define DMA_CCR_SAPI		0x0800		/* increment source ptr */
#define DMA_CCR_READ		0x1000		/* direction in SAM */
#define DMA_CCR_ECO		0x1000		/* external control option */
#define DMA_CCR_INT_ERR		0x2000		/* error interrupt */
#define DMA_CCR_INT		0x4000		/* normal interrupt */
#define DMA_CCR_INT_BRK		0x8000		/* breakpoint interrupt */

/* DMA_CSR - Channel Status Register */

#define DMA_CSR_BRKP		0x04		/* breakpoint */
#define DMA_CSR_CONF		0x08		/* configuration error */
#define DMA_CSR_BED		0x10		/* destination bus error */
#define DMA_CSR_BES		0x20		/* source bus error */
#define DMA_CSR_DONE		0x40		/* dma done */
#define DMA_CSR_IRQ		0x80		/* interrupt request */

/* function declarations */

#ifndef	_ASMLANGUAGE
#ifndef	INCLUDE_TY_CO_DRV_50
#if defined(__STDC__) || defined(__cplusplus)

IMPORT	void	tyCoInt (void);

#else	/* __STDC__ */

IMPORT	void	tyCoInt ();

#endif	/* __STDC__ */
#endif	/* INCLUDE_TY_CO_DRV_50 */
#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCm68340h */
