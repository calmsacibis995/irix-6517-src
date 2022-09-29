/* r68561.h - Rockwell 68561 Multi-Protocol Communications Controller */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01b,05oct90,shl  added copyright notice.
01a,10feb88,dnw  extracted from frc21.h
*/

#ifndef __INCr68561h
#define __INCr68561h

#ifdef __cplusplus
extern "C" {
#endif

/* receiver registers */

#define MPCC_RSR(base)		((char*) (base) + 0x00)	/* status reg */
#define MPCC_RCR(base)		((char*) (base) + 0x01)	/* control reg */
#define MPCC_RDR(base)		((char*) (base) + 0x02)	/* data reg */
#define MPCC_RIVNR(base)	((char*) (base) + 0x04)	/* int vec num reg */
#define MPCC_RIER(base)		((char*) (base) + 0x05)	/* int enable reg */

/* transmitter registers */

#define MPCC_TSR(base)		((char*) (base) + 0x08)	/* status reg */
#define MPCC_TCR(base)		((char*) (base) + 0x09)	/* control reg */
#define MPCC_TDR(base)		((char*) (base) + 0x0a)	/* data reg */
#define MPCC_TIVNR(base)	((char*) (base) + 0x0c)	/* int vec num reg */
#define MPCC_TIER(base)		((char*) (base) + 0x0d)	/* int enable reg */

/* serial registers */

#define MPCC_SISR(base)		((char*) (base) + 0x10)	/* intfc status reg */
#define MPCC_SICR(base)		((char*) (base) + 0x11)	/* intfc control reg */
#define MPCC_SIVNR(base)	((char*) (base) + 0x14)	/* intfc int vec num */
#define MPCC_SIER(base)		((char*) (base) + 0x15)	/* intfc int enable */

/* global registers */

#define MPCC_PSR1(base)		((char*) (base) + 0x18)	/* protocol sel reg 1 */
#define MPCC_PSR2(base)		((char*) (base) + 0x19)	/* protocol sel reg 2 */
#define MPCC_AR1(base)		((char*) (base) + 0x1a)	/* address reg 1 */
#define MPCC_AR2(base)		((char*) (base) + 0x1b)	/* address reg 2 */
#define MPCC_BRDR1(base)	((char*) (base) + 0x1c)	/* baud rate div reg 1*/
#define MPCC_BRDR2(base)	((char*) (base) + 0x1d)	/* baud rate div reg 2*/
#define MPCC_CCR(base)		((char*) (base) + 0x1e)	/* clock control reg */
#define MPCC_ECR(base)		((char*) (base) + 0x1f)	/* error control reg */


/* MPCC rcvr status reg bits */

#define MPCC_RSR_RDA		0x80            /* rcvr data available */
#define MPCC_RSR_EOF		0x40            /* end of frame */
#define MPCC_RSR_CPERR		0x10            /* crc/parity error */
#define MPCC_RSR_FRERR		0x08            /* framing error */
#define MPCC_RSR_ROVRN		0x04            /* rcvr overrun */
#define MPCC_RSR_RAB		0x02            /* rcvr abort/break */
#define MPCC_RSR_RIDLE		0x01            /* rcvr idle */

/* MPCC rcvr control reg bits */

#define MPCC_RCR_RDSREN		0x40            /* rcvr data srvc req enable */
#define MPCC_RCR_DONEEN		0x20            /* DONE output enable */
#define MPCC_RCR_RSYNEN		0x10            /* RSYNEN output enable */
#define MPCC_RCR_STRSYN		0x08            /* strip SYN char (COP only) */
#define MPCC_RCR_2ADCMP		0x04            /* 1/2 address compare (BOP) */
#define MPCC_RCR_RABTEN		0x02            /* rcvr abort enable (BOP) */
#define MPCC_RCR_RRES		0x01            /* rcvr reset command */

/* MPCC rcvr interrupt enable reg bits */

#define MPCC_RIER_RDA		0x80            /* rcvr data avail int enable */
#define MPCC_RIER_EOF		0x40            /* EOF interrupt enable */
#define MPCC_RIER_CPERR		0x20            /* crc/parity error int enable*/
#define MPCC_RIER_FRERR		0x08            /* frame error int enable */
#define MPCC_RIER_ROVRN		0x04            /* rcvr overrun int enable */
#define MPCC_RIER_RAB		0x02            /* rcvr abort/break int enable*/

/* MPCC xmitter status reg bits */

#define MPCC_TSR_TDRA		0x80            /* xmit data reg avail */
#define MPCC_TSR_TFC		0x40            /* xmitted frame complete */
#define MPCC_TSR_TUNRN		0x04            /* xmitter underrun */
#define MPCC_TSR_TFERR		0x02            /* xmitter frame error */

/* MPCC xmitter control reg bits */

#define MPCC_TCR_TEN		0x80            /* xmitter enable */
#define MPCC_TCR_TDSREN		0x40            /* xmit data srvc req enable */
#define MPCC_TCR_TICS		0x20            /* xmitter idle char select */
#define MPCC_TCR_THW		0x10            /* xmit half word (word mode) */
#define MPCC_TCR_TLAST		0x08            /* xmit last char */
#define MPCC_TCR_TSYN		0x04            /* xmit SYN */
#define MPCC_TCR_TABT		0x02            /* xmit ABORT */
#define MPCC_TCR_TRES		0x01            /* xmitter reset command */

/* MPCC xmitter interrupt enable reg bits */

#define MPCC_TIER_TDRA		0x80            /* xmit data reg avail int */
#define MPCC_TIER_TFC		0x40            /* xmit frame complete int */
#define MPCC_TIER_TUNRN		0x04            /* xmitter underrun int */
#define MPCC_TIER_TFERR		0x02            /* xmitter frame error int */

/* MPCC Serial Interface status reg bits */

#define MPCC_SISR_CTS		0x80           /* Clear to Send transition */
#define MPCC_SISR_DSR		0x40           /* Data Set Ready transition */
#define MPCC_SISR_DCD		0x20           /* Data Carrier Dectect trans */
#define MPCC_SISR_CTSLVL	0x10           /* Clear to Send level */
#define MPCC_SISR_DSRLVL	0x08           /* Data Set Ready level */
#define MPCC_SISR_DCDLVL	0x04           /* Data Carrier Dectect level */

/* MPCC Serial Interface control reg bits */

#define MPCC_SICR_RTSLVL	0x80           /* Request To Send level */
#define MPCC_SICR_DTRLVL	0x40           /* Data Terminal Ready level */
#define MPCC_SICR_ECHO		0x04           /* echo mode enable */
#define MPCC_SICR_TEST		0x02           /* self-test enable */
#define MPCC_SICR_NRZI		0x01           /* NRZI data format select */

/* MPCC Serial Interface interrupt enable reg bits */

#define MPCC_SIIE_CTS		0x80	/* Clear to Send interrupt enable */
#define MPCC_SIIE_DSR		0x40	/* Data Set Ready interrupt enable */
#define MPCC_SIIE_DCD		0x20	/* Data Carrier Detect int enable */

/* MPCC protocol select reg 1 bits */

#define MPCC_PSR1_CTLEX		0x02    /* Control field extend */
#define MPCC_PSR1_ADDEX		0x01    /* Address extend */

/* MPCC protocol select reg 2 bits */

#define MPCC_PSR2_WDBYTE	0x80    /* data bus word/byte */

#define MPCC_PSR2_1STOP		0x00    /* 1 stop bit */
#define MPCC_PSR2_12STOP	0x20    /* 1 1/2 stop bits */
#define MPCC_PSR2_2STOP		0x40    /* 2 stop bits */

#define MPCC_PSR2_5BITS		0x00    /* 5-bit chars */
#define MPCC_PSR2_6BITS		0x08    /* 6-bit chars */
#define MPCC_PSR2_7BITS		0x10    /* 7-bit chars */
#define MPCC_PSR2_8BITS		0x18    /* 8-bit chars */

#define MPCC_PSR2_ASYNC		0x06    /* async protocol */

/* MPCC clock control reg bits */

#define MPCC_CCR_PSCDIV		0x10    /* prescaler divider */
#define MPCC_CCR_TCLKO		0x08    /* xmitter clock output sel */
#define MPCC_CCR_RCLKIN		0x04    /* select external/internal rx clock */
#define MPCC_CCR_DIV1		0x00    /* external rx clock divide by 1 */
#define MPCC_CCR_DIV16          0x01    /* external rx clock divide by 16 */
#define MPCC_CCR_DIV32          0x02    /* external rx clock divide by 32 */
#define MPCC_CCR_DIV64          0x03    /* external rx clock divide by 64 */

/* MPCC error control reg bits */

#define MPCC_ECR_PAREN		0x80    /* parity enable */
#define MPCC_ECR_ODDPAR		0x40    /* odd/even parity select */
#define MPCC_ECR_CRCCTL		0x08    /* control field CRC enable */
#define MPCC_ECR_CRCPRE		0x04    /* crc generator preset select */
#define MPCC_ECR_CRCSEL1        0x00    /* crc polynomial 1 */
#define MPCC_ECR_CRCSEL2        0x01    /* crc polynomial 2 */
#define MPCC_ECR_CRCSEL3        0x02    /* crc polynomial 3 */
#define MPCC_ECR_CRCSEL4        0x03    /* crc polynomial 4 */

#ifdef __cplusplus
}
#endif

#endif /* __INCr68561h */
