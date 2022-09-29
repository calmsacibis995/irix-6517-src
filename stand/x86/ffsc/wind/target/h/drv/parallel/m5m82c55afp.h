/* m5m82c55afp.h - Mitsubishi 82c55 PPI chip header */

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

#ifndef __INCm5m82c55afph
#define __INCm5m82c55afph

#ifdef __cplusplus
extern "C" {
#endif

#define	M5M82C55AFP_REG_OFFSET		3

#ifndef _ASMLANGUAGE
typedef struct
    {
    UCHAR pad0[M5M82C55AFP_REG_OFFSET];
    UCHAR portA;
    UCHAR pad1[M5M82C55AFP_REG_OFFSET];
    UCHAR portB;
    UCHAR pad2[M5M82C55AFP_REG_OFFSET];
    UCHAR portC;
    UCHAR pad3[M5M82C55AFP_REG_OFFSET];
    UCHAR control;
    } PPI;
#endif	/* _ASMLANGUAGE */

/* bit value for control register */
#define PPI_PC_MODE		0x80
#define PPI_PA_MODE1		0x20
#define PPI_PA_MODE2		0x40
#define PPI_PA07_INPUT		0x10
#define PPI_PC47_INPUT		0x08
#define PPI_PB_MODE		0x04
#define PPI_PB07_INPUT		0x02
#define PPI_PC03_INPUT		0x01

#ifdef __cplusplus
}
#endif

#endif /* __INCm5m82c55afph */
