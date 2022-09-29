/* m5m82c51afp.h - Mitsubishi 82c51 USART chip header */

/* Copyright 1991-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01d,22sep92,rrr  added support for c++
01c,26may92,rrr  the tree shuffle
01b,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed ASMLANGUAGE to _ASMLANGUAGE
		  -changed copyright notice
01a,16may91,hdn  written.
*/

#ifndef __INCm5m82c51afph
#define __INCm5m82c51afph

#ifdef __cplusplus
extern "C" {
#endif

#define	M5M82C51AFP_REG_OFFSET		3

#ifndef _ASMLANGUAGE
typedef struct
    {
    UCHAR pad0[M5M82C51AFP_REG_OFFSET];
    UCHAR data;
    UCHAR pad1[M5M82C51AFP_REG_OFFSET];
    UCHAR command;
    } USART;
#endif	/* _ASMLANGUAGE */

/* bit value for mode register */
#define USART_1STOPBIT		0x40
#define USART_15STOPBIT		0x80
#define USART_2STOPBIT		0xc0
#define USART_PARITY_EVEN	0x20
#define USART_PARITY_ON		0x10
#define USART_6BIT		0x04
#define USART_7BIT		0x08
#define USART_8BIT		0x0c
#define USART_X1		0x01
#define USART_X16		0x02
#define USART_X64		0x03

/* bit value for command register */
#define USART_ENTER_HUNT_MODE	0x80
#define USART_INTERNAL_RESET	0x40
#define USART_RTS		0x20
#define USART_ERROR_RESET	0x10
#define USART_SEND_BREAK	0x08
#define USART_RX_ENABLE		0x04
#define USART_DTR		0x02
#define USART_TX_ENABLE		0x01
#define USART_ON		(USART_DTR | USART_RX_ENABLE)

/* bit value for status register */
#define USART_DSR		0x80
#define USART_SYNDET		0x40
#define USART_FE		0x20
#define USART_OE		0x10
#define USART_PE		0x08
#define USART_TXE		0x04
#define USART_RXRDY		0x02
#define USART_TXRDY		0x01
#define USART_ERROR		(USART_FE | USART_OE | USART_PE)

#ifdef __cplusplus
}
#endif

#endif /* __INCm5m82c51afph */
