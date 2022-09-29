/* m33242j.h - Mitsubishi 33242j IRC Interrupt Request Controller header file */

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

#ifndef __INCm33242jh
#define __INCm33242jh

#ifdef __cplusplus
extern "C" {
#endif

#define	M33242J_REG_OFFSET		3

#ifndef _ASMLANGUAGE
typedef struct
    {
    UCHAR pad0[M33242J_REG_OFFSET];
    UCHAR lmr1;
    UCHAR pad1[M33242J_REG_OFFSET];
    UCHAR lmr2;
    UCHAR pad2[M33242J_REG_OFFSET];
    UCHAR lmr3;
    UCHAR pad3[M33242J_REG_OFFSET];
    UCHAR lmr4;
    UCHAR pad4[M33242J_REG_OFFSET];
    UCHAR lmr5;
    UCHAR pad5[M33242J_REG_OFFSET];
    UCHAR lmr6;
    UCHAR pad6[M33242J_REG_OFFSET];
    UCHAR vmr;
    UCHAR pad7[M33242J_REG_OFFSET];
    UCHAR vsr;
    UCHAR pad8[M33242J_REG_OFFSET];
    UCHAR vnr;
    UCHAR pad9[M33242J_REG_OFFSET];
    UCHAR tmr;
    UCHAR pad10[M33242J_REG_OFFSET];
    UCHAR imr;
    UCHAR pad11[M33242J_REG_OFFSET];
    UCHAR irr;
    UCHAR pad12[M33242J_REG_OFFSET];
    UCHAR bmr;
    UCHAR pad13[M33242J_REG_OFFSET];
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
#define IRC_IMR_DISABLE		0xff
#define IRC_BMR_DISABLE		0xff
#define IRC_VMR_AUTOVEC		0xff
#define IRC_TMR_EDGE		0xff
#define IRC_IRR_RESET		0xff
#define IRC_VNR_AUTOVEC		0xff

#ifdef __cplusplus
}
#endif

#endif /* __INCm33242jh */
