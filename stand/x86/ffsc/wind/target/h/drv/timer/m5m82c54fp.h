/* m5m82c54fp.h - Mitsubishi 82c54 PIT chip header */

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
01a,23may91,hdn  written.
*/

#ifndef __INCm5m82c54fph
#define __INCm5m82c54fph

#ifdef __cplusplus
extern "C" {
#endif

#define	M5M82C54FP_REG_OFFSET		3

#ifndef _ASMLANGUAGE
typedef struct
    {
    UCHAR pad0[M5M82C54FP_REG_OFFSET];
    UCHAR counter0;
    UCHAR pad1[M5M82C54FP_REG_OFFSET];
    UCHAR counter1;
    UCHAR pad2[M5M82C54FP_REG_OFFSET];
    UCHAR counter2;
    UCHAR pad3[M5M82C54FP_REG_OFFSET];
    UCHAR control;
    } PIT;
#endif	/* _ASMLANGUAGE */

/* bit value for control register */
#define PIT_SELECT_COUNTER0	0x00
#define PIT_SELECT_COUNTER1	0x40
#define PIT_SELECT_COUNTER2	0x80
#define PIT_RL_LATCH		0x00
#define PIT_RL_LOW		0x10
#define PIT_RL_HIGH		0x20
#define PIT_RL_LOWHIGH		0x30
#define PIT_MODE0		0x00
#define PIT_MODE1		0x02
#define PIT_MODE2		0x04
#define PIT_MODE3		0x06
#define PIT_MODE4		0x08
#define PIT_MODE5		0x0a
#define PIT_BCD			0x01

#ifdef __cplusplus
}
#endif

#endif /* __INCm5m82c54fph */
