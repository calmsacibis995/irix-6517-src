/* m68562.h - Motorola M68562 Serial I/O Chip header */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01k,22sep92,rrr  added support for c++
01j,02jul92,caf  added TY_CO_DEV and function declarations for 5.1 upgrade.
		 for 5.0.x compatibility, define INCLUDE_TY_CO_DRV_50 when
		 including this header.
01i,26may92,rrr  the tree shuffle
01h,04oct91,rrr  passed through the ansification filter
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01g,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01f,11jun90,gae  added _ASMLANGUAGE version.
		 simplified DUSCC macros, but kept 4.0.2 version temporarily.
		 fixed def'n of DUSCC_OMR_GPO1_1 to be O (oh) not 0 (zero).
01e,01mar89,jcc  incorporated DUSCC_REG_ADDR_INTERVAL into definitions.
01d,28nov88,jcf  change M562 to DUSCC because thats easier to remember.
01c,10aug88,gae  got rid of ^L which are unloved by some assemblers.
01b,21jun88,mcl  changed to be generic, not just mz7122; adds documentation.
01a,26jan88,sah  written.
*/

#ifndef __INCm68562h
#define __INCm68562h

#ifdef __cplusplus
extern "C" {
#endif

/*
 * The macro's DUSCC_BASE_ADRS and DUSCC_REG_INTERVAL must be defined
 * when including this header.
 *
 * define INCLUDE_TY_CO_DRV_50 when using this header if you are using
 * a VxWorks 5.0-style serial driver (tyCoDrv.c).
 */

#ifdef	_ASMLANGUAGE

#define	CAST

#else	/* _ASMLANGUAGE */

#define	CAST	(char *)

#include "tylib.h"

#ifndef  INCLUDE_TY_CO_DRV_50

typedef struct          /* TY_CO_DEV */
    {
    TY_DEV	    tyDev;
    BOOL	    created;	    /* true if this device has been created */
    char	    numChannels;    /* number of channels to support */
    volatile char * rx_data;        /* receiver data port */
    volatile char * tx_data;        /* transmitter data port */
    volatile char * ttr;            /* transmitter timer register */
    volatile char * rtr;            /* receiver timer register */
    volatile char * ier;            /* interrupt enable register */
    volatile char * gsr;            /* general status register */
    volatile char * rsr;            /* receiver status register */
    volatile char * cmr1;
    volatile char * cmr2;
    volatile char * tpr;
    volatile char * rpr;
    volatile char * omr;
    volatile char * ccr;
    char	    rx_rdy;         /* receiver ready bit */
    char	    tx_rdy;         /* transmitter ready bit */
    } TY_CO_DEV;

#endif	/* INCLUDE_TY_CO_DRV_50 */

#endif	/* _ASMLANGUAGE */

#define DUSCC_ADRS(reg)   (CAST (DUSCC_BASE_ADRS+(reg*DUSCC_REG_INTERVAL)))

/* port registers */

#define DUSCC_CMR1A	DUSCC_ADRS(0x00)	/* channel mode reg. 1    */
#define DUSCC_CMR2A	DUSCC_ADRS(0x01)	/* channel mode reg. 2    */
#define DUSCC_S1RA	DUSCC_ADRS(0x02)	/* secondary adrs reg1    */
#define DUSCC_S2RA	DUSCC_ADRS(0x03)	/* secondary adrs reg2    */
#define DUSCC_TPRA	DUSCC_ADRS(0x04)	/* tx parameter reg.      */
#define DUSCC_TTRA	DUSCC_ADRS(0x05)	/* tx timing reg.         */
#define DUSCC_RPRA	DUSCC_ADRS(0x06)	/* rx parameter reg.      */
#define DUSCC_RTRA	DUSCC_ADRS(0x07)	/* rx timing reg.         */
#define DUSCC_CTPRHA	DUSCC_ADRS(0x08)	/* counter preset high    */
#define DUSCC_CTPRLA	DUSCC_ADRS(0x09)	/* counter preset low     */
#define DUSCC_CTCRA	DUSCC_ADRS(0x0a)	/* counter control reg.   */
#define DUSCC_OMRA	DUSCC_ADRS(0x0b)	/* output/misc. reg.      */
#define DUSCC_CTHA	DUSCC_ADRS(0x0c)	/* counter/timer high     */
#define DUSCC_CTLA	DUSCC_ADRS(0x0d)	/* counter/timer low      */
#define DUSCC_PCRA	DUSCC_ADRS(0x0e)	/* pin configuration      */
#define DUSCC_CCRA	DUSCC_ADRS(0x0f)	/* channel command reg.   */
#define DUSCC_TXFIFOA	DUSCC_ADRS(0x10)	/* tx FIFO                */
#define DUSCC_RXFIFOA	DUSCC_ADRS(0x14)	/* rx FIFO                */
#define DUSCC_RSRA	DUSCC_ADRS(0x18)	/* rx status reg.         */
#define DUSCC_TRSRA	DUSCC_ADRS(0x19)	/* tx/rx status reg.      */
#define DUSCC_ICTSRA	DUSCC_ADRS(0x1a)	/* counter status reg.    */
#define DUSCC_GSR	DUSCC_ADRS(0x1b)	/* general status         */
#define DUSCC_IERA	DUSCC_ADRS(0x1c)	/* interrupt enable       */
#define DUSCC_IVR	DUSCC_ADRS(0x1e)	/* interrupt vector       */
#define DUSCC_ICR	DUSCC_ADRS(0x1f)	/* interrupt control      */
#define DUSCC_CMR1B	DUSCC_ADRS(0x20)	/* channel mode reg. 1    */
#define DUSCC_CMR2B	DUSCC_ADRS(0x21)	/* channel mode reg. 2    */
#define DUSCC_S1RB	DUSCC_ADRS(0x22)	/* secondary adrs reg 1   */
#define DUSCC_S2RB	DUSCC_ADRS(0x23)	/* secondary adrs reg 2   */
#define DUSCC_TPRB	DUSCC_ADRS(0x24)	/* tx parameter reg.      */
#define DUSCC_TTRB	DUSCC_ADRS(0x25)	/* tx timing reg.         */
#define DUSCC_RPRB	DUSCC_ADRS(0x26)	/* rx parameter reg.      */
#define DUSCC_RTRB	DUSCC_ADRS(0x27)	/* rx timing reg.         */
#define DUSCC_CTPRHB	DUSCC_ADRS(0x28)	/* counter preset high    */
#define DUSCC_CTPRLB	DUSCC_ADRS(0x29)	/* counter preset low     */
#define DUSCC_CTCRB	DUSCC_ADRS(0x2a)	/* counter control reg.   */
#define DUSCC_OMRB	DUSCC_ADRS(0x2b)	/* output/misc. reg.      */
#define DUSCC_CTHB	DUSCC_ADRS(0x2c)	/* counter/timer high     */
#define DUSCC_CTLB	DUSCC_ADRS(0x2d)	/* counter/timer low      */
#define DUSCC_PCRB	DUSCC_ADRS(0x2e)	/* pin configuration      */
#define DUSCC_CCRB	DUSCC_ADRS(0x2f)	/* channel command reg.   */
#define DUSCC_TXFIFOB	DUSCC_ADRS(0x30)	/* tx FIFO                */
#define DUSCC_RXFIFOB	DUSCC_ADRS(0x34)	/* rx FIFO                */
#define DUSCC_RSRB	DUSCC_ADRS(0x38)	/* rx status reg.         */
#define DUSCC_TRSRB	DUSCC_ADRS(0x39)	/* tx/rx status reg.      */
#define DUSCC_ICTSRB	DUSCC_ADRS(0x3a)	/* count/timer status     */
#define DUSCC_IERB	DUSCC_ADRS(0x3c)	/* interrupt enable       */
#define DUSCC_IVRM	DUSCC_ADRS(0x3e)	/* int. vec. modified     */


/* vector number offsets */

#define DUSCC_INT_A_RXRDY		0x00	/* RxRDY interrupt */
#define DUSCC_INT_A_TXRDY		0x01	/* TxRDY interrupt */
#define DUSCC_INT_A_RX_TX_ERROR		0x02	/* rx/tx error interrupt */
#define DUSCC_INT_A_EXT			0x03	/* ext. & cnt/timer interrupt */
#define DUSCC_INT_B_RXRDY		0x04	/* RxRDY interrupt */
#define DUSCC_INT_B_TXRDY		0x05	/* TxRDY interrupt */
#define DUSCC_INT_B_RX_TX_ERROR		0x06	/* rx/tx error interrupt */
#define DUSCC_INT_B_EXT			0x07	/* ext. & cnt/timer interrupt */

#define N_CHANNELS	2	/* number of channels per chip */

/* channel mode register one (CMR1A, CMR1B) */

#define DUSCC_CMR1_BOP_PRIMARY		0x00	/* channel protocol mode */
#define DUSCC_CMR1_BOP_SECONDARY	0x01	/* channel protocol mode */
#define DUSCC_CMR1_BOP_LOOP		0x02	/* channel protocol mode */
#define DUSCC_CMR1_BOP_LOOP_NO_ADR_CMP	0x03	/* channel protocol mode */
#define DUSCC_CMR1_BOP_8BIT_ADRS	0x00	/* single octet address */
#define DUSCC_CMR1_BOP_EXT_ADRS		0x08	/* extended address */
#define DUSCC_CMR1_BOP_16BIT_ADRS	0x10	/* dual octet address */
#define DUSCC_CMR1_BOP_16BIT_GRP_ADRS	0x18	/* dual octet adrs with group */
#define DUSCC_CMR1_BOP_1_OCTET_CNT	0x00	/* 1 ctrl field follows adrs */
#define DUSCC_CMR1_BOP_2_OCTET_CNT	0x20	/* 2 ctrl fields follow adrs */

#define DUSCC_CMR1_COP_DUAL_SYN		0x04	/* channel protocol mode */
#define DUSCC_CMR1_COP_BISYNC		0x05	/* channel protocol mode */
#define DUSCC_CMR1_COP_SINGLE_SYN	0x06	/* channel protocol mode */
#define DUSCC_CMR1_COP_NO_PARITY	0x00	/* channel parity none */
#define DUSCC_CMR1_COP_WITH_PARITY	0x10	/* channel parity odd/even */
#define DUSCC_CMR1_COP_FORCE_PARITY	0x18	/* channel parity hi/lo */
#define DUSCC_CMR1_COP_EVEN_PARITY	0x00	/* channel parity even */
#define DUSCC_CMR1_COP_ODD_PARITY	0x20	/* channel parity odd */
#define DUSCC_CMR1_COP_BISYNC_EBCDIC	0x00	/* comparisons in EBCDIC */
#define DUSCC_CMR1_COP_BISYNC_ASCII	0x20	/* comparisons in ASCII */

#define DUSCC_CMR1_ASYNC		0x07	/* chnl protocol mode: async. */

#define DUSCC_CMR1_NRZ			0x00	/* non-return-to-zero */
#define DUSCC_CMR1_NRZI			0x40	/* non-return-to-zero, inv */
#define DUSCC_CMR1_FM0			0x80	/* bi-phase space */
#define DUSCC_CMR1_FM1			0xc0	/* bi-phase mark */


/* equates for channel mode register two (CMR2A, CMR2B) */

#define DUSCC_CMR2_FCS_SEL_NONE		0x00	/* frame check seq. select */
#define DUSCC_CMR2_FCS_SEL_LRC_8_PRS0	0x02	/* frame check seq. select */
#define DUSCC_CMR2_FCS_SEL_LRC_8_PRS1	0x03	/* frame check seq. select */
#define DUSCC_CMR2_FCS_SEL_CRC_16_PRS0	0x04	/* frame check seq. select */
#define DUSCC_CMR2_FCS_SEL_CRC_16_PRS1	0x05	/* frame check seq. select */
#define DUSCC_CMR2_FCS_SEL_CRC_CC_PRS0	0x06	/* frame check seq. select */
#define DUSCC_CMR2_FCS_SEL_CRC_CC_PRS1	0x07	/* frame check seq. select */

#define DUSCC_CMR2_DTI_HLFDUP_SADRDMA	0x00	/* data transfer interface */
#define DUSCC_CMR2_DTI_HLFDUP_DADRDMA	0x08	/* data transfer interface */
#define DUSCC_CMR2_DTI_FULDUP_SADRDMA	0x10	/* data transfer interface */
#define DUSCC_CMR2_DTI_FULDUP_DADRDMA	0x18	/* data transfer interface */
#define DUSCC_CMR2_DTI_WAIT_RX		0x20	/* data transfer interface */
#define DUSCC_CMR2_DTI_WAIT_TX		0x28	/* data transfer interface */
#define DUSCC_CMR2_DTI_WAIT_RX_TX	0x30	/* data transfer interface */
#define DUSCC_CMR2_DTI_POLL_OR_INT	0x38	/* data transfer interface */

#define DUSCC_CMR2_CHN_CON_NORNAL	0x00	/* channel connection */
#define DUSCC_CMR2_CHN_CON_AUTO_ECHO	0x40	/* channel connection */
#define DUSCC_CMR2_CHN_CON_LOCAL_LOOP	0x80	/* channel connection */


/* equates for transmit parameter register (TPRA, TPRB) */

#define DUSCC_TPR_5BITS			0x00	/* tx 5 bits per character */
#define DUSCC_TPR_6BITS			0x01	/* tx 6 bits per character */
#define DUSCC_TPR_7BITS			0x02	/* tx 7 bits per character */
#define DUSCC_TPR_8BITS			0x03	/* tx 8 bits per character */

#define DUSCC_TPR_CTS_EN_TX_ENABLE	0x04	/* CTS_N affects tx */

#define DUSCC_TPR_TX_RTS_CONT_YES	0x08	/* RTS_N affected by tx */

#define DUSCC_TPR_ASYNC_9_16		0x00	/* async: 9/16 stop bits */
#define DUSCC_TPR_ASYNC_10_16		0x10	/* async: 10/16 stop bits */
#define DUSCC_TPR_ASYNC_11_16		0x20	/* async: 11/16 stop bits */
#define DUSCC_TPR_ASYNC_12_16		0x30	/* async: 12/16 stop bits */
#define DUSCC_TPR_ASYNC_13_16		0x40	/* async: 13/16 stop bits */
#define DUSCC_TPR_ASYNC_14_16		0x50	/* async: 14/16 stop bits */
#define DUSCC_TPR_ASYNC_15_16		0x60	/* async: 15/16 stop bits */
#define DUSCC_TPR_ASYNC_1		0x70	/* async: 16/16 stop bits */
#define DUSCC_TPR_ASYNC_1_9_16		0x80	/* async: 1 stop bit */
#define DUSCC_TPR_ASYNC_1_10_16		0x90	/* async: 10/16 stop bits */
#define DUSCC_TPR_ASYNC_1_11_16		0xa0	/* async: 11/16 stop bits */
#define DUSCC_TPR_ASYNC_1_12_16		0xb0	/* async: 12/16 stop bits */
#define DUSCC_TPR_ASYNC_1_13_16		0xc0	/* async: 13/16 stop bits */
#define DUSCC_TPR_ASYNC_1_14_16		0xd0	/* async: 14/16 stop bits */
#define DUSCC_TPR_ASYNC_1_15_16		0xe0	/* async: 15/16 stop bits */
#define DUSCC_TPR_ASYNC_2		0xf0	/* async: 2 stop bits */

#define DUSCC_TPR_COP_TEOM_NO		0x00	/* no EOM on zero or done */
#define DUSCC_TPR_COP_TEOM_YES		0x10	/* tx EOM on zero or done */
#define DUSCC_TPR_COP_IDLE_MARK		0x00	/* idle in marking condition */
#define DUSCC_TPR_COP_IDLE_SYN		0x20	/* idle sending SYNs */
#define DUSCC_TPR_COP_UNDR_FCS		0x00	/* EOM termination */
#define DUSCC_TPR_COP_UNDR_MARK		0x80	/* TxD in marking condition */
#define DUSCC_TPR_COP_UNDR_SYN		0xc0	/* send SYNs */

#define DUSCC_TPR_BOP_TEOM_NO		0x00	/* no EOM on zero or done */
#define DUSCC_TPR_BOP_TEOM_YES		0x10	/* tx EOM on zero or done */
#define DUSCC_TPR_BOP_IDLE_MARK		0x00	/* idle in marking condition */
#define DUSCC_TPR_BOP_IDLE_SYN		0x20	/* idle sending FLAGs */
#define DUSCC_TPR_BOP_UNDR_FCS		0x00	/* EOM termination */
#define DUSCC_TPR_BOP_UNDR_ABORT_MARK	0x80	/* TxD in marking condition */
#define DUSCC_TPR_BOP_UNDR_ABORT_FLAG	0xc0	/* send ABORTs */


/* equates for transmitter timing register (TTRA, TTRB) */

#define DUSCC_TTR_BAUD_50		0x00	/* transmit speed */
#define DUSCC_TTR_BAUD_75		0x01	/* transmit speed */
#define DUSCC_TTR_BAUD_110		0x02	/* transmit speed */
#define DUSCC_TTR_BAUD_134_5		0x03	/* transmit speed */
#define DUSCC_TTR_BAUD_150		0x04	/* transmit speed */
#define DUSCC_TTR_BAUD_200		0x05	/* transmit speed */
#define DUSCC_TTR_BAUD_300		0x06	/* transmit speed */
#define DUSCC_TTR_BAUD_600		0x07	/* transmit speed */
#define DUSCC_TTR_BAUD_1050		0x08	/* transmit speed */
#define DUSCC_TTR_BAUD_1200		0x09	/* transmit speed */
#define DUSCC_TTR_BAUD_2000		0x0a	/* transmit speed */
#define DUSCC_TTR_BAUD_2400		0x0b	/* transmit speed */
#define DUSCC_TTR_BAUD_4800		0x0c	/* transmit speed */
#define DUSCC_TTR_BAUD_9600		0x0d	/* transmit speed */
#define DUSCC_TTR_BAUD_19200		0x0e	/* transmit speed */
#define DUSCC_TTR_BAUD_38400		0x0f	/* transmit speed */

#define DUSCC_TTR_CLK_1x_EXT		0x00	/* ext. clock, 1x baud rate */
#define DUSCC_TTR_CLK_16x_EXT		0x10	/* ext. clock, 16x, async */
#define DUSCC_TTR_CLK_DPLL		0x20	/* int., DPLL, half duplex */
#define DUSCC_TTR_CLK_BRG		0x30	/* int., BRG, 32x, baud rate */
#define DUSCC_TTR_CLK_2x_OTHER_CHAN	0x40	/* int., C/T, 2x baud rate */
#define DUSCC_TTR_CLK_32x_OTHER_CHAN	0x50	/* int., C/T, 32x baud rate */
#define DUSCC_TTR_CLK_2x_OWN_CHAN	0x60	/* int., C/T, 2x baud rate */
#define DUSCC_TTR_CLK_32x_OWN_CHAN	0x70	/* int., C/T, 32x baud rate */

#define DUSCC_TTR_EXT_SOURCE_RTXC	0x00	/* ext. input from RTxC */
#define DUSCC_TTR_EXT_SOURCE_TRXC	0x80	/* ext. input from TRxC */

/* equates for receiver parameter register (RPRA, RPRB) */

#define DUSCC_RPR_5BITS			0x00	/* rx 5 bits per character */
#define DUSCC_RPR_6BITS			0x01	/* rx 6 bits per character */
#define DUSCC_RPR_7BITS			0x02	/* rx 7 bits per character */
#define DUSCC_RPR_8BITS			0x03	/* rx 8 bits per character */

#define DUSCC_RPR_DCD_EN_RX_DISABLE	0x00	/* not used to enable rx */
#define DUSCC_RPR_DCD_EN_RX_ENABLE	0x04	/* used to enable receiver */

#define DUSCC_RPR_ASYNC_STRIP_PARITY_NO	0x00	/* do not strip RX parity */
#define DUSCC_RPR_ASYNC_STRIP_PARITY_YES 0x08	/* strip RX parity */
#define DUSCC_RPR_ASYNC_RX_RTS_CONT_NO	0x00	/* rx can't negate RTS_N */
#define DUSCC_RPR_ASYNC_RX_RTS_CONT_YES	0x10	/* async: rx can negate RTS_N */

#define DUSCC_RPR_COP_STRIP_PARITY_NO	0x00	/* don't strip RX parity */
#define DUSCC_RPR_COP_STRIP_PARITY_YES	0x08	/* strip RX parity */
#define DUSCC_RPR_COP_EXT_SYNC_NO	0x00	/* external SYNC disable */
#define DUSCC_RPR_COP_EXT_SYNC_YES	0x10	/* external SYNC enable */
#define DUSCC_RPR_COP_AUTO_HUNT_NO	0x00	/* disable auto-hunt/PAD check*/
#define DUSCC_RPR_COP_AUTO_HUNT_YES	0x20	/* enable auto-hunt/PAD check */
#define DUSCC_RPR_COP_FCS_FIFO_NO	0x00	/* don't trans to RxFIFO */
#define DUSCC_RPR_COP_FCS_FIFO_YES	0x40	/* trans. FCS to RxFIFO */
#define DUSCC_RPR_COP_SYN_STRIP_LEADING	0x00	/* strip leading SYNs */
#define DUSCC_RPR_COP_SYN_STRIP_ALL	0x80	/* strip all SYNs */

#define DUSCC_RPR_BOP_ALL_PARITY_ADR_NO	0x00	/* BOP: don't recognize all */
#define DUSCC_RPR_BOP_ALL_PARITY_ADR_YES 0x08	/* BOP: recognize all parties */
#define DUSCC_RPR_BOP_OVRUN_HUNT	0x00	/* BOP: hunt for FLAG on over */
#define DUSCC_RPR_BOP_OVRUN_CONT	0x20	/* BOP: continue on overrun */
#define DUSCC_RPR_BOP_FCS_FIFO_NO	0x00	/* BOP: don't trans. FCS */
#define DUSCC_RPR_BOP_FCS_FIFO_YES	0x40	/* BOP: trans. FCS to RxFIFO */

/* equates for receiver timing register (RTRA, RTRB) */

#define DUSCC_RTR_BAUD_50		0x00	/* receiver speed */
#define DUSCC_RTR_BAUD_75		0x01	/* receiver speed */
#define DUSCC_RTR_BAUD_110		0x02	/* receiver speed */
#define DUSCC_RTR_BAUD_134_5		0x03	/* receiver speed */
#define DUSCC_RTR_BAUD_150		0x04	/* receiver speed */
#define DUSCC_RTR_BAUD_200		0x05	/* receiver speed */
#define DUSCC_RTR_BAUD_300		0x06	/* receiver speed */
#define DUSCC_RTR_BAUD_600		0x07	/* receiver speed */
#define DUSCC_RTR_BAUD_1050		0x08	/* receiver speed */
#define DUSCC_RTR_BAUD_1200		0x09	/* receiver speed */
#define DUSCC_RTR_BAUD_2000		0x0a	/* receiver speed */
#define DUSCC_RTR_BAUD_2400		0x0b	/* receiver speed */
#define DUSCC_RTR_BAUD_4800		0x0c	/* receiver speed */
#define DUSCC_RTR_BAUD_9600		0x0d	/* receiver speed */
#define DUSCC_RTR_BAUD_19200		0x0e	/* receiver speed */
#define DUSCC_RTR_BAUD_38400		0x0f	/* receiver speed */

#define DUSCC_RTR_CLK_1x_EXT		0x00	/* ext. clock, 1x */
#define DUSCC_RTR_CLK_16x_EXT		0x10	/* ext. clock, 16x, async */
#define DUSCC_RTR_CLK_BRG		0x20	/* int. BRG clock, 32x, async */
#define DUSCC_RTR_CLK_CT		0x30	/* int. C/T clock, 32x, async */
#define DUSCC_RTR_CLK_DPLL_64x		0x40	/* int. clock, 64x */
#define DUSCC_RTR_CK_DPLL_32x_EXT	0x50	/* ext. clock, 32x baud */
#define DUSCC_RTR_CK_DPLL_32x_BRG	0x60	/* int. BRG clock, 32x baud */
#define DUSCC_RTR_CK_DPLL_32x_CT	0x70	/* int. C/T clock, 32x baud */

#define DUSCC_RTR_EXT_SOURCE_RTXC	0x00	/* external input from RTxC */
#define DUSCC_RTR_EXT_SOURCE_TRXC	0x80	/* external input from TRxC */

/* equates for counter/timer control register (CTCRA, CTCRB) */

#define DUSCC_CTCR_CLK_RTXC		0x00	/* clock source is RTxC pin */
#define DUSCC_CTCR_CLK_TRXC		0x01	/* clock source is TRxC pin */
#define DUSCC_CTCR_CLK_X1_CLK		0x02	/* clock source is clock/4 */
#define DUSCC_CTCR_CLK_X1_CLK_RXD	0x03	/* clk is clk/4 gated by RxD */
#define DUSCC_CTCR_CLK_RX_BRG		0x04	/* clock source is BRG */
#define DUSCC_CTCR_CLK_TX_BRG		0x05	/* clock source is BRG */
#define DUSCC_CTCR_CLK_RX_CHAR		0x06	/* clock source is rxload */
#define DUSCC_CTCR_CLK_TX_CHAR		0x07	/* clock source is txload */

#define DUSCC_CTCR_PRESCALER_DIV_1	0x00	/* prescale by 1 */
#define DUSCC_CTCR_PRESCALER_DIV_16	0x08	/* prescale by 16 */
#define DUSCC_CTCR_PRESCALER_DIV_32	0x10	/* prescale by 32 */
#define DUSCC_CTCR_PRESCALER_DIV_64	0x18	/* prescale by 64 */

#define DUSCC_CTCR_OUTPUT_CONT_SQUARE	0x00	/* count roll toggles output */
#define DUSCC_CTCR_OUTPUT_CONT_PULSE	0x20	/* counter roll pulses output */

#define DUSCC_CTCR_ZERO_DET_CONT_PRESET	0x00	/* counter is preset to value */
#define DUSCC_CTCR_ZERO_DET_CONT_CONT	0x40	/* counter is preset to -1 */

#define DUSCC_CTCR_ZERO_DET_INTR_DISABLE 0x00	/* disable zero detect int. */
#define DUSCC_CTCR_ZERO_DET_INTR_ENABLE	0x80	/* enable zero detect int. */

/* equates for output and miscellaneous register (OMRA, OMRB) */

#define DUSCC_OMR_RTS_0			0x00	/* request-to-send lo */
#define DUSCC_OMR_RTS_1			0x01	/* request-to-send hi */

#define DUSCC_OMR_GPO1_0		0x00	/* gen. purpose output 1 lo */
#define DUSCC_OMR_GPO1_1		0x02	/* gen. purpose output 1 hi */

#define DUSCC_OMR_GPO2_0		0x00	/* gen. purpose output 2 lo */
#define DUSCC_OMR_GPO2_1		0x04	/* gen. purpose output 2 hi */

#define DUSCC_OMR_RXRDY_FIFO_NOT_EMPTY	0x00	/* set TxRDY until FIFO empty */
#define DUSCC_OMR_RXRDY_FIFO_FULL	0x08	/* set TxRDY if FIFO full */

#define DUSCC_OMR_TXRDY_FIFO_NOT_FULL	0x00	/* set TxRDY until FIFO full */
#define DUSCC_OMR_TXRDY_FIFO_EMPTY	0x10	/* set TxRDY if FIFO empty */

#define DUSCC_OMR_TX_RES_CHAR_LENGTH_1	0x00	/* BOP: last char is 1 bits */
#define DUSCC_OMR_TX_RES_CHAR_LENGTH_2	0x20	/* BOP: last char is 2 bits */
#define DUSCC_OMR_TX_RES_CHAR_LENGTH_3	0x40	/* BOP: last char is 3 bits */
#define DUSCC_OMR_TX_RES_CHAR_LENGTH_4	0x60	/* BOP: last char is 4 bits */
#define DUSCC_OMR_TX_RES_CHAR_LENGTH_5	0x80	/* BOP: last char is 5 bits */
#define DUSCC_OMR_TX_RES_CHAR_LENGTH_6	0xa0	/* BOP: last char is 6 bits */
#define DUSCC_OMR_TX_RES_CHAR_LENGTH_7	0xc0	/* BOP: last char is 7 bits */
#define DUSCC_OMR_TX_RES_CHAR_LENGTH_TPR 0xe0	/* BOP: last is TPR[1:0] bits */


/* equates for pin configuration register (PCRA, PCRB) */

#define DUSCC_PCR_TRXC_INPUT		0x00	/* TRxC function: input */
#define DUSCC_PCR_TRXC_XTAL_2		0x01	/* TRxC function: XTAL/2 */
#define DUSCC_PCR_TRXC_DPLL		0x02	/* TRxC function: DPLL output */
#define DUSCC_PCR_TRXC_CT		0x03	/* TRxC function: count/timer */
#define DUSCC_PCR_TRXC_TXCLK_16x	0x04	/* TRxC function: TTRx16 */
#define DUSCC_PCR_TRXC_RXCLK_16x	0x05	/* TRxC function: RTRx16 */
#define DUSCC_PCR_TRXC_TXCLK_1x		0x06	/* TRxC function: TXC output */
#define DUSCC_PCR_TRXC_RXCLK_1x		0x07	/* TRxC function: RXC output */

#define DUSCC_PCR_RTXC_INPUT		0x00	/* RTxC function: input */
#define DUSCC_PCR_RTXC_CT		0x08	/* RTxC function: count/timer */
#define DUSCC_PCR_RTXC_TXCLK_1x		0x10	/* RTxC function: TXC output */
#define DUSCC_PCR_RTXC_RXCLK_1x		0x18	/* RTxC function: RXC output */

#define DUSCC_PCR_SYNOUT_RTS_SYNOUT	0x00	/* RTS is a SYNOUT */
#define DUSCC_PCR_SYNOUT_RTS_RTS	0x20	/* RTS is an RTS */

#define DUSCC_PCR_GPO2_RTS_GPO2		0x00	/* GPO2 is a GPO2 */
#define DUSCC_PCR_GPO2_RTS_RTS		0x40	/* GPO2 is an RTS */

#define DUSCC_PCR_X2_IDC_X2		0x00	/* X2 is crystal connection */
#define DUSCC_PCR_X2_IDC_IDC		0x80	/* X2 is int. daisy chain */

/* equates for channel command register (CCRA, CCRB) commands */

#define DUSCC_CCR_TX_RESET_TX		0x00	/* reset transmitter */
#define DUSCC_CCR_TX_RESET_TXCRC	0x01	/* reset transmit CRC */
#define DUSCC_CCR_TX_ENABLE_TX		0x02	/* enable transmitter */
#define DUSCC_CCR_TX_DISABLE_TX		0x03	/* disable transmitter */
#define DUSCC_CCR_TX_TX_TSOM		0x04	/* transmit start of message */
#define DUSCC_CCR_TX_TX_TSOMP		0x05	/* tx start of msg with pad */
#define DUSCC_CCR_TX_TX_TEOM		0x06	/* transmit end-of-message */
#define DUSCC_CCR_TX_TX_TABRK		0x07	/* transmit async break */
#define DUSCC_CCR_TX_TX_TABORT		0x07	/* transmit BOP abort */
#define DUSCC_CCR_TX_TX_DLE		0x08	/* transmit DLE */
#define DUSCC_CCR_TX_ACT_ON_POLL	0x09	/* go active on poll */
#define DUSCC_CCR_TX_RESET_ON_POLL	0x0a	/* reset go active on poll */
#define DUSCC_CCR_TX_GO_ON_LOOP		0x0b	/* go on-loop */
#define DUSCC_CCR_TX_GO_OFF_LOOP	0x0c	/* go off-loop */
#define DUSCC_CCR_TX_EXCLD_CRC		0x0d	/* exclude from CRC */

#define DUSCC_CCR_RX_RESET_RX		0x40	/* reset receiver */
#define DUSCC_CCR_RX_ENABLE_RX		0x42	/* enable receiver */
#define DUSCC_CCR_RX_DISABLE_RX		0x43	/* disable receiver */

#define DUSCC_CCR_CT_START		0x80	/* start counter/timer */
#define DUSCC_CCR_CT_STOP		0x81	/* stop counter/timer */
#define DUSCC_CCR_CT_PRE_FFFF		0x82	/* preset counter/timer to -1 */
#define DUSCC_CCR_CT_PRE_CTPRHL		0x83	/* counter/timer CTPRH/CTPRL */

#define DUSCC_CCR_DPLL_ENTR_SEARCH	0xc0	/* DPLL: enter search mode */
#define DUSCC_CCR_DPLL_DISABLE_DPLL	0xc1	/* DPLL: disable */
#define DUSCC_CCR_DPLL_SET_FM		0xc2	/* DPLL: set FM mode */
#define DUSCC_CCR_DPLL_SET_NRZ1		0xc3	/* DPLL: set NRZI mode */

/* equates for receiver status register (RSRA, RSRB) */

#define DUSCC_RSR_ASYNC_PARITY_ERROR	0x01	/* parity error detected */
#define DUSCC_RSR_ASYNC_FRAMING_ERR	0x02	/* framing error detected */
#define DUSCC_RSR_ASYNC_BRK_STR_DET	0x04	/* break start detected */
#define DUSCC_RSR_ASYNC_BRK_END_DET	0x08	/* break detected */
#define DUSCC_RSR_ASYNC_OVERN_ERROR	0x20	/* overrun error */
#define DUSCC_RSR_ASYNC_RTS_NEGATED	0x40	/* RTS negated when FIFO full */
#define DUSCC_RSR_ASYNC_CHAR_COMP	0x80	/* character compared to S1R */

#define DUSCC_RSR_COP_PARITY_ERROR	0x01	/* parity error detected */
#define DUSCC_RSR_COP_CRC_ERROR		0x02	/* CRC error detected */
#define DUSCC_RSR_COP_SYN_DETET		0x04	/* SYN detected */
#define DUSCC_RSR_COP_OVERN_ERROR	0x20	/* overrun error */
#define DUSCC_RSR_COP_PAD_ERROR		0x40	/* PAD error detected */
#define DUSCC_RSR_COP_EOM_DETET		0x80	/* end-of-message detected */

#define DUSCC_RSR_BOP_RCL_NOT_ZERO	0x01	/* non-zero character len. */
#define DUSCC_RSR_BOP_FCS_ERROR		0x02	/* FCS error detected */
#define DUSCC_RSR_BOP_FLAG_DETECT	0x04	/* FLAG sequence detected */
#define DUSCC_RSR_BOP_IDLE_DETECT	0x08	/* idle sequence detected */
#define DUSCC_RSR_BOP_SHORT_FRAM_DET	0x10	/* missing fields */
#define DUSCC_RSR_BOP_OVERN_ERROR	0x20	/* overrun error */
#define DUSCC_RSR_BOP_ABORT_DETECT	0x40	/* abort sequence detected */
#define DUSCC_RSR_BOP_EOM_DETECT	0x80	/* end-of-message detected */

#define DUSCC_RSR_BOP_LOOP_RCL_NOT_ZERO	0x01	/* non-zero char len. */
#define DUSCC_RSR_BOP_LOOP_FCS_ERROR	0x02	/* FCS error */
#define DUSCC_RSR_BOP_LOOP_FLAG_DETECT	0x04	/* FLAG sequence */
#define DUSCC_RSR_BOP_LOOP_TURN_DETECT	0x08	/* turnaround seq. */
#define DUSCC_RSR_BOP_LOOP_SHORT_FRAM_DET 0x10	/* missing fields */
#define DUSCC_RSR_BOP_LOOP_OVERN_ERROR	0x20	/* overrun error */
#define DUSCC_RSR_BOP_LOOP_ABORT_DETECT	0x40	/* abort sequence */
#define DUSCC_RSR_BOP_LOOP_EOM_DETECT	0x80	/* end-of-message */

/* equates for transmitter and receiver status register (TRSRA, TRSRB) */

#define DUSCC_TRSR_ASYNC_DPLL_ERROR	0x08	/* async: DPLL lost lock */
#define DUSCC_TRSR_ASYNC_SND_BRK_ACK	0x10	/* async: transmitting break */
#define DUSCC_TRSR_ASYNC_CTS_UNDERRUN	0x40	/* async: CTS underrun */
#define DUSCC_TRSR_ASYNC_TX_EMPTY	0x80	/* async: transmit empty */

#define DUSCC_TRSR_COP_RX_XPRNT		0x01	/* COP: rx in transparent mode*/
#define DUSCC_TRSR_COP_RX_HUNT		0x02	/* COP: rx hunting for SYN */
#define DUSCC_TRSR_COP_DPLL_ERROR	0x08	/* COP: DPLL lost lock */
#define DUSCC_TRSR_COP_SND_ACK		0x10	/* COP: xmit now sending ACK */
#define DUSCC_TRSR_COP_FRAME_COMP	0x20	/* COP: end of message */
#define DUSCC_TRSR_COP_CTS_UNDERRUN	0x40	/* COP: CTS underrun */
#define DUSCC_TRSR_COP_TX_EMPTY		0x80	/* COP: transmit empty */

#define DUSCC_TRSR_BOP_RESDL_RX_0BIT	0x00	/* BOP: received residual len */
#define DUSCC_TRSR_BOP_RESDL_RX_1BIT	0x01	/* BOP: received residual len */
#define DUSCC_TRSR_BOP_RESDL_RX_2BIT	0x02	/* BOP: received residual len */
#define DUSCC_TRSR_BOP_RESDL_RX_3BIT	0x03	/* BOP: received residual len */
#define DUSCC_TRSR_BOP_RESDL_RX_4BIT	0x04	/* BOP: received residual len */
#define DUSCC_TRSR_BOP_RESDL_RX_5BIT	0x05	/* BOP: received residual len */
#define DUSCC_TRSR_BOP_RESDL_RX_6BIT	0x06	/* BOP: received residual len */
#define DUSCC_TRSR_BOP_RESDL_RX_7BIT	0x07	/* BOP: received residual len */

#define DUSCC_TRSR_BOP_DPLL_ERROR	0x08	/* BOP: DPLL lost lock */
#define DUSCC_TRSR_BOP_SND_ABRT_ACK	0x10	/* BOP: tx now sending ABORT */
#define DUSCC_TRSR_BOP_FRAME_COMPLETE	0x20	/* BOP: end of msg */
#define DUSCC_TRSR_BOP_CTS_UNDERRUN	0x40	/* BOP: CTS underrun */
#define DUSCC_TRSR_BOP_TX_EMPTY		0x80	/* BOP: transmit empty */

#define DUSCC_TRSR_LOOP_LOOP_SND	0x40	/* LOOP: EOP sequence found */


/* equates for input and counter/timer status register (ICTSRA, ICTSRB) */

#define DUSCC_ICTSR_GPI1		0x01	/* state of GPI1 pin */
#define DUSCC_ICTSR_GPI2		0x02	/* state of GPI2 pin */
#define DUSCC_ICTSR_CTSLC		0x04	/* state of CTLSC pin */
#define DUSCC_ICTSR_DCD			0x08	/* state of DCD pin */

#define DUSCC_ICTSR_DELTA_CTSLC		0x10	/* CTS/LC changed */
#define DUSCC_ICTSR_DELTA_DCD		0x20	/* DCD changed */
#define DUSCC_ICTSR_CT_ZERO_COUNT	0x40	/* 1 = zero detected */
#define DUSCC_ICTSR_CT_RUNNING		0x80	/* counter/timer is running */


/* equates for general status register (GSR) */

#define DUSCC_GSR_A_RXRDY		0x01	/* received a character on A */
#define DUSCC_GSR_A_TXRDY		0x02	/* ready to tx character on A */
#define DUSCC_GSR_A_RXTX_STATUS		0x04	/* RSRA[7:0], TRSRA[7:3] */
#define DUSCC_GSR_A_EXTERNAL_STATUS	0x08	/* refer to ICTSRA[6:4] */
#define DUSCC_GSR_A_ALL			0x0f	/* mask for above */

#define DUSCC_GSR_B_RXRDY		0x10	/* received a character on B */
#define DUSCC_GSR_B_TXRDY		0x20	/* ready to tx character on B */
#define DUSCC_GSR_B_RXTX_STATUS		0x40	/* RSRB[7:0], TRSRB[7:3] */
#define DUSCC_GSR_B_EXTERNAL_STATUS	0x80	/* refer to ICTSRB[6:4] */
#define DUSCC_GSR_B_ALL			0xf0	/* mask for above */

/* equates for interrupt enable register (IERA, IERB) */

#define DUSCC_IER_RSR_1_0		0x01	/* interrupt if RSR [0 or 1] */
#define DUSCC_IER_RSR_3_2		0x02	/* interrupt if RSR [2 or 3] */
#define DUSCC_IER_RSR_5_4		0x04	/* interrupt if RSR [4 or 5] */
#define DUSCC_IER_RSR_7_6		0x08	/* interrupt if RSR [6 or 7] */

#define DUSCC_IER_RXRDY			0x10	/* interrupt if RxRDY */
#define DUSCC_IER_TRSR			0x20	/* interrupt if TRSR bits 3-7 */
#define DUSCC_IER_TXRDY			0x40	/* interrupt if TxRDY */
#define DUSCC_IER_DCD_CTS		0x80	/* interrupt if DCD or CTS */


/* equates for interrupt control register (ICR) */

#define DUSCC_ICR_CHAN_B_MAST_INT_EN	0x01	/* chnl B master int. enable */
#define DUSCC_ICR_CHAN_A_MAST_INT_EN	0x02	/* chnl A master int. enable */

#define DUSCC_ICR_VECT_INCLDS_STATUS	0x04	/* 1 = use IVRM, 0 = use IVR */

#define DUSCC_ICR_BITS_MOD_2_0		0x00	/* modify vector bits 2:0 */
#define DUSCC_ICR_BITS_MOD_4_2		0x08	/* modify vector bits 4:2 */

#define DUSCC_ICR_VECT_MODE_VECT	0x00	/* interrupts are vectored */
#define DUSCC_ICR_VECT_MODE_NOT_VECT	0x30	/* interrupts non-vectored */

#define DUSCC_ICR_CHN_AB_INT_PRI_A	0x00	/* channel A highest priority */
#define DUSCC_ICR_CHN_AB_INT_PRI_B	0x40	/* interleaved pri, A favored */
#define DUSCC_ICR_CHN_AB_INT_PRI_INTL_A	0x80	/* channel B highest priority */
#define DUSCC_ICR_CHN_AB_INT_PRI_INTL_B	0xc0	/* interleaved pri, B favored */


/* equates for interrupt vector - as modified by IVRM */

#define DUSCC_IVRM_MASK			0x07	/* bits that can be modified */
#define DUSCC_IVRM_MASK_CHANNEL		0x04	/* 1 = chnl B, 0 = chnl A */
#define DUSCC_IVRM_RXRDY		0x00	/* RxRDY interrupt */
#define DUSCC_IVRM_TXRDY		0x01	/* TxRDY interrupt */
#define DUSCC_IVRM_RX_TX_EXCEPTION	0x02	/* rx/tx error interrupt */
#define DUSCC_IVRM_EXTERN_OR_CT		0x03	/* ext. & cnt/timer interrupt */

/* function declarations */

#ifndef INCLUDE_TY_CO_DRV_50
#ifndef _ASMLANGUAGE
#if defined(__STDC__) || defined(__cplusplus)

IMPORT  void    tyCoTxInt (FAST TY_CO_DEV * pTyCoDv);
IMPORT  void    tyCoRxInt (FAST TY_CO_DEV * pTyCoDv);
IMPORT  void    tyCoRxTxErrInt (FAST TY_CO_DEV * pTyCoDv);

#else	/* __STDC__ */

IMPORT  void    tyCoTxInt ();
IMPORT  void    tyCoRxInt ();
IMPORT  void    tyCoRxTxErrInt ();

#endif	/* __STDC__ */
#endif	/* _ASMLANGUAGE */
#endif	/* INCLUDE_TY_CO_DRV_50 */

#ifdef __cplusplus
}
#endif

#endif /* __INCm68562h */
