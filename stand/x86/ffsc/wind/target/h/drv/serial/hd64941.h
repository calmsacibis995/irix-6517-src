/* hd64941.h - Hitachi 64941 Serial Communications Controller header */

/* Copyright 1991-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01b,07jun91,hdn  defined HD64941_REG_OFFSET.
01a,09feb91,hdn  written.
*/

#ifndef __INChd64941h
#define __INChd64941h

#ifdef __cplusplus
extern "C" {
#endif

#define	HD64941_REG_OFFSET		3

#ifndef _ASMLANGUAGE
typedef struct
    {			/* Read		Write */
    UCHAR pad0[HD64941_REG_OFFSET];
    UCHAR dr;		/* RDR		TDR */
    UCHAR pad1[HD64941_REG_OFFSET];
    UCHAR sr;		/* SR		- */
    UCHAR pad2[HD64941_REG_OFFSET];
    UCHAR mr;		/* MR1,2	MR1,2 */
    UCHAR pad3[HD64941_REG_OFFSET];
    UCHAR cr;		/* CR		CR */
    } ACI;

typedef struct
    {
    int rate;
    UCHAR timeConstant;
    } BAUD;
#endif	/* _ASMLANGUAGE */

/* bit values for status register */
#define ACI_TxRDY		0x01
#define ACI_RxRDY		0x02
#define ACI_TxEMT		0x04
#define ACI_ERR_PARITY		0x08
#define ACI_ERR_OVERRUN		0x10
#define ACI_ERR_FRAMING		0x20
#define ACI_ERRS		(ACI_ERR_PARITY|ACI_ERR_OVERRUN|ACI_ERR_FRAMING)
#define ACI_DO_WORK		(ACI_TxRDY|ACI_TxEMT|ACI_RxRDY)

/* bit values for mode register 1 */
#define ACI_BR_CONTROL_1	0x01
#define ACI_BR_CONTROL_16	0x02
#define ACI_BR_CONTROL_64	0x03
#define ACI_7BIT		0x08
#define ACI_8BIT		0x0c
#define ACI_PARITY_ON		0x10
#define ACI_PARITY_EVEN		0x20
#define ACI_STOP_1		0x40
#define ACI_STOP_2		0xc0

/* bit values for mode register 2 */
#define ACI_INTERNAL_BRG	0x30
#define ACI_BR_9600		0x0e

/* bit values for command register */
#define ACI_TxEN		0x01
#define ACI_0X02		0x02
#define ACI_RxEN		0x04
#define ACI_BREAK		0x08
#define ACI_ERR_RESET		0x10
#define ACI_RTS_ON		0x20
#define ACI_AUTO_ECHO		0x40
#define ACI_L_LOOPBACK		0x80
#define ACI_R_LOOPBACK		0xc0

#ifdef __cplusplus
}
#endif

#endif /* __INChd64941h */
