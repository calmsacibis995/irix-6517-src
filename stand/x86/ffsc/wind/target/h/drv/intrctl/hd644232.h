/* hd644232.h - Hitachi 644232 IRC Interrupt Request Controller header */

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
01b,07jun91,hdn  defined HD644232_REG_OFFSET.
01a,09feb91,hdn  written.
*/

#ifndef __INChd644232h
#define __INChd644232h

#ifdef __cplusplus
extern "C" {
#endif

#define	HD644232_REG_OFFSET		3

#ifndef _ASMLANGUAGE
typedef struct
    {
    UCHAR pad0[HD644232_REG_OFFSET];
    UCHAR lmr1;
    UCHAR pad1[HD644232_REG_OFFSET];
    UCHAR lmr2;
    UCHAR pad2[HD644232_REG_OFFSET];
    UCHAR lmr3;
    UCHAR pad3[HD644232_REG_OFFSET];
    UCHAR lmr4;
    UCHAR pad4[HD644232_REG_OFFSET];
    UCHAR lmr5;
    UCHAR pad5[HD644232_REG_OFFSET];
    UCHAR lmr6;
    UCHAR pad6[HD644232_REG_OFFSET];
    UCHAR vmr;
    UCHAR pad7[HD644232_REG_OFFSET];
    UCHAR vsr;
    UCHAR pad8[HD644232_REG_OFFSET];
    UCHAR vnr;
    UCHAR pad9[HD644232_REG_OFFSET];
    UCHAR tmr;
    UCHAR pad10[HD644232_REG_OFFSET];
    UCHAR imr;
    UCHAR pad11[HD644232_REG_OFFSET];
    UCHAR irr;
    UCHAR pad12[HD644232_REG_OFFSET];
    UCHAR bmr;
    UCHAR pad13[HD644232_REG_OFFSET];
    UCHAR brr;
    } IRC;
#endif	/* _ASMLANGUAGE */

/* Local Interrupt Request */
#define IRC_LIR0		0x01
#define IRC_LIR1		0x02
#define IRC_LIR2		0x04
#define IRC_LIR3		0x08
#define IRC_LIR4		0x10
#define IRC_LIR5		0x20
#define IRC_LIR6		0x40

/* default values */
#define IRC_VMR_AUTOVEC		0xff
#define IRC_TMR_EDGE		0xff
#define IRC_IMR_DISABLE		0x7f
#define IRC_BMR_DISABLE		0xff
#define IRC_BMR_ENABLE		0x00

#ifdef __cplusplus
}
#endif

#endif /* __INChd644232h */
