/* z8530Sio.h - header file for binary interface z8530 driver */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,13oct95,ms  added comments for what fields need to be set by the BSP
01b,15jun95,ms	updated for new driver structure
01a,20dec94,ms  written (from z8530Serial.h).
*/

#ifndef __INCwdbz8530h
#define __INCwdbz8530h

#include "siolib.h"

#ifdef __cplusplus
extern "C" {
#endif

/* device and channel structures */

/* The BSP must initilize all Z8530_CHAN fields which say "BSP:" */

typedef struct				/* Z8530_CHAN */
    {
    /* always goes first */

    SIO_DRV_FUNCS *	pDrvFuncs;	/* driver functions */

    /* callbacks */

    STATUS	(*getTxChar) ();
    STATUS	(*putRcvChar) ();
    void *	getTxArg;
    void *	putRcvArg;

    /* control and data register addresses */

    volatile char * cr;         /* BSP: control register I/O address */
    volatile char * dr;         /* BSP: data port I/O address */

    /* misc values */

    int         baudFreq;       /* BSP: baud clock frequency */
    int		baudRate;	/* baud rate */
    char        writeReg11;     /* BSP: write register 11 value */
    char        writeReg14;     /* BSP: write register 14 value */

    /* interrupt/polled mode configuration info */

    uint_t      mode;           /* SIO_MODE_[INT | POLL] */
    char        intVec;         /* BSP: interrupt vector address */
    char        intType;        /* BSP: type of vector to supply from 8530 */
    uint_t      channel;        /* channel number (0 or 1) */
    } Z8530_CHAN;

typedef struct
    {
    Z8530_CHAN	portA;          /* port A device descriptor */
    Z8530_CHAN	portB;          /* port B device descriptor */
    } Z8530_DUSART;

/* channels */

#define SCC_CHANNEL_A	0
#define SCC_CHANNEL_B	1

/* bit values for write register 0 */
/* command register */

#define	SCC_WR0_SEL_WR0 	0x00
#define	SCC_WR0_SEL_WR1 	0x01
#define	SCC_WR0_SEL_WR2 	0x02
#define	SCC_WR0_SEL_WR3 	0x03
#define	SCC_WR0_SEL_WR4 	0x04
#define	SCC_WR0_SEL_WR5 	0x05
#define	SCC_WR0_SEL_WR6 	0x06
#define	SCC_WR0_SEL_WR7 	0x07
#define	SCC_WR0_SEL_WR8 	0x08
#define	SCC_WR0_SEL_WR9		0x09
#define	SCC_WR0_SEL_WR10	0x0a
#define	SCC_WR0_SEL_WR11	0x0b
#define	SCC_WR0_SEL_WR12	0x0c
#define	SCC_WR0_SEL_WR13	0x0d
#define	SCC_WR0_SEL_WR14	0x0e
#define	SCC_WR0_SEL_WR15	0x0f
#define	SCC_WR0_NULL_CODE	0x00
#define	SCC_WR0_RST_INT		0x10
#define	SCC_WR0_SEND_ABORT	0x18
#define	SCC_WR0_EN_INT_RX	0x20
#define	SCC_WR0_RST_TX_INT	0x28
#define	SCC_WR0_ERR_RST		0x30
#define	SCC_WR0_RST_HI_IUS	0x38
#define	SCC_WR0_RST_RX_CRC	0x40
#define	SCC_WR0_RST_TX_CRC	0x80
#define	SCC_WR0_RST_TX_UND	0xc0

/* write register 2 */
/* interrupt vector */

/* bit values for write register 1 */
/* tx/rx interrupt and data transfer mode definition */

#define	SCC_WR1_EXT_INT_EN	0x01
#define	SCC_WR1_TX_INT_EN	0x02
#define	SCC_WR1_PARITY		0x04
#define	SCC_WR1_RX_INT_DIS	0x00
#define	SCC_WR1_RX_INT_FIR	0x08
#define	SCC_WR1_INT_ALL_RX	0x10
#define	SCC_WR1_RX_INT_SPE	0x18
#define	SCC_WR1_RDMA_RECTR	0x20
#define	SCC_WR1_RDMA_FUNC	0x40
#define	SCC_WR1_RDMA_EN		0x80

/* bit values for write register 3 */
/* receive parameters and control */

#define	SCC_WR3_RX_EN		0x01
#define	SCC_WR3_SYNC_CHAR	0x02
#define	SCC_WR3_ADR_SEARCH	0x04
#define	SCC_WR3_RX_CRC_EN	0x08
#define	SCC_WR3_ENTER_HUNT	0x10
#define	SCC_WR3_AUTO_EN		0x20
#define	SCC_WR3_RX_5_BITS	0x00
#define	SCC_WR3_RX_7_BITS	0x40
#define	SCC_WR3_RX_6_BITS	0x80
#define	SCC_WR3_RX_8_BITS	0xc0

/* bit values for write register 4 */
/* tx/rx misc parameters and modes */

#define	SCC_WR4_PAR_EN		0x01
#define	SCC_WR4_PAR_EVEN	0x02
#define	SCC_WR4_SYNC_EN		0x00
#define	SCC_WR4_1_STOP		0x04
#define	SCC_WR4_2_STOP		0x0c
#define	SCC_WR4_8_SYNC		0x00
#define	SCC_WR4_16_SYNC		0x10
#define	SCC_WR4_SDLC		0x20
#define	SCC_WR4_EXT_SYNC	0x30
#define	SCC_WR4_1_CLOCK		0x00
#define	SCC_WR4_16_CLOCK	0x40
#define	SCC_WR4_32_CLOCK	0x80
#define	SCC_WR4_64_CLOCK	0xc0

/* bit values for write register 5 */
/* transmit parameter and controls */

#define	SCC_WR5_TX_CRC_EN	0x01
#define	SCC_WR5_RTS		0x02
#define	SCC_WR5_SDLC		0x04
#define	SCC_WR5_TX_EN		0x08
#define	SCC_WR5_SEND_BRK	0x10

#define	SCC_WR5_TX_5_BITS	0x00
#define	SCC_WR5_TX_7_BITS	0x20
#define	SCC_WR5_TX_6_BITS	0x40
#define	SCC_WR5_TX_8_BITS	0x60
#define	SCC_WR5_DTR		0x80

/* write register 6 */
/* sync chars or sdlc address field */

/* write register 7 */
/* sync char or sdlc flag */

/* write register 8 */
/* transmit buffer */

/* bit values for write register 9 */
/* master interrupt control */

#define	SCC_WR9_VIS		0x01
#define	SCC_WR9_NV		0x02
#define	SCC_WR9_DLC		0x04
#define	SCC_WR9_MIE		0x08
#define	SCC_WR9_STATUS_HI	0x10
#define	SCC_WR9_NO_RST		0x00
#define	SCC_WR9_CH_B_RST	0x40
#define	SCC_WR9_CH_A_RST	0x80
#define	SCC_WR9_HDWR_RST	0xc0

/* bit values for write register 10 */
/* misc tx/rx control bits */

#define	SCC_WR10_6_BIT_SYNC	0x01
#define	SCC_WR10_LOOP_MODE	0x02
#define	SCC_WR10_ABORT_UND	0x04
#define	SCC_WR10_MARK_IDLE	0x08
#define	SCC_WR10_ACT_POLL	0x10
#define	SCC_WR10_NRZ		0x00
#define	SCC_WR10_NRZI		0x20
#define	SCC_WR10_FM1		0x40
#define	SCC_WR10_FM0		0x60
#define	SCC_WR10_CRC_PRESET	0x80

/* bit values for write register 11 */
/* clock mode control */

#define	SCC_WR11_OUT_XTAL	0x00
#define	SCC_WR11_OUT_TX_CLK	0x01
#define	SCC_WR11_OUT_BR_GEN	0x02
#define	SCC_WR11_OUT_DPLL	0x03
#define	SCC_WR11_TRXC_OI	0x04
#define	SCC_WR11_TX_RTXC	0x00
#define	SCC_WR11_TX_TRXC	0x08
#define	SCC_WR11_TX_BR_GEN	0x10
#define	SCC_WR11_TX_DPLL	0x18
#define	SCC_WR11_RX_RTXC	0x00
#define	SCC_WR11_RX_TRXC	0x20
#define	SCC_WR11_RX_BR_GEN	0x40
#define	SCC_WR11_RX_DPLL	0x60
#define	SCC_WR11_RTXC_XTAL	0x80

/* write register 12 */
/* lower byte of baud rate generator time constant */

/* write register 13 */
/* upper byte of baud rate generator time constant */

/* bit values for write register 14 */
/* misc control bits */

#define	SCC_WR14_BR_EN		0x01
#define	SCC_WR14_BR_SRC		0x02
#define	SCC_WR14_DTR_FUNC	0x04
#define	SCC_WR14_AUTO_ECHO 	0x08
#define	SCC_WR14_LCL_LOOP	0x10
#define	SCC_WR14_NULL		0x00
#define	SCC_WR14_SEARCH		0x20
#define	SCC_WR14_RST_CLK	0x40
#define	SCC_WR14_DIS_DPLL	0x60
#define	SCC_WR14_SRC_BR		0x80
#define	SCC_WR14_SRC_RTXC	0xa0
#define	SCC_WR14_FM_MODE	0xc0
#define	SCC_WR14_NRZI		0xe0

/* bit values for write register 15 */
/* external/status interrupt control */

#define	SCC_WR15_ZERO_CNT	0x02
#define	SCC_WR15_CD_IE		0x08
#define	SCC_WR15_SYNC_IE	0x10
#define	SCC_WR15_CTS_IE		0x20
#define	SCC_WR15_TX_UND_IE	0x40
#define	SCC_WR15_BREAK_IE	0x80

/* bit values for read register 0 */
/* tx/rx buffer status and external status  */

#define	SCC_RR0_RX_AVAIL	0x01
#define	SCC_RR0_ZERO_CNT	0x02
#define	SCC_RR0_TX_EMPTY	0x04
#define	SCC_RR0_CD		0x08
#define	SCC_RR0_SYNC		0x10
#define	SCC_RR0_CTS		0x20
#define	SCC_RR0_TX_UND		0x40
#define	SCC_RR0_BREAK		0x80

/* bit values for read register 1 */

#define	SCC_RR1_ALL_SENT	0x01
#define	SCC_RR1_RES_CD_2	0x02
#define	SCC_RR1_RES_CD_1	0x01
#define	SCC_RR1_RES_CD_0	0x08
#define	SCC_RR1_PAR_ERR		0x10
#define	SCC_RR1_RX_OV_ERR	0x20
#define	SCC_RR1_CRC_ERR		0x40
#define	SCC_RR1_END_FRAME	0x80

/* read register 2 */
/* interrupt vector */

/* bit values for read register 3 */
/* interrupt pending register */

#define	SCC_RR3_B_EXT_IP	0x01
#define	SCC_RR3_B_TX_IP		0x02
#define	SCC_RR3_B_RX_IP		0x04
#define	SCC_RR3_A_EXT_IP	0x08
#define	SCC_RR3_A_TX_IP		0x10
#define	SCC_RR3_A_RX_IP		0x20

/* read register 8 */
/* receive data register */

/* bit values for read register 10 */
/* misc status bits */

#define	SCC_RR10_ON_LOOP	0x02
#define	SCC_RR10_LOOP_SEND	0x10
#define	SCC_RR10_2_CLK_MIS	0x40
#define	SCC_RR10_1_CLK_MIS	0x80

/* read register 12 */
/* lower byte of time constant */

/* read register 13 */
/* upper byte of time constant */

/* bit values for read register 15 */
/* external/status ie bits */

#define	SCC_RR15_ZERO_CNT	0x02
#define	SCC_RR15_CD_IE		0x08
#define	SCC_RR15_SYNC_IE	0x10
#define	SCC_RR15_CTS_IE		0x20
#define	SCC_RR15_TX_UND_IE	0x40
#define	SCC_RR15_BREAK_IE	0x80

#if defined(__STDC__) || defined(__cplusplus)

extern void 	z8530DevInit	(Z8530_DUSART *pDusart); 

extern void	z8530IntWr	(Z8530_CHAN * pChan);
extern void	z8530IntRd	(Z8530_CHAN * pChan);
extern void	z8530IntEx	(Z8530_CHAN * pChan);
extern void	z8530Int	(Z8530_DUSART *pDusart);

#else   /* __STDC__ */

extern void 	z8530DevInit	();

extern void	z8530IntWr	();
extern void	z8530IntRd	();
extern void	z8530IntEx	();
extern void	z8530Int	();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif  /* __INCwdbz8530h */

