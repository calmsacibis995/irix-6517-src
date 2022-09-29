/* m68153.h - Motorola 68153 Bus Interruptor Module header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01e,22sep92,rrr  added support for c++
01d,26may92,rrr  the tree shuffle
01c,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01b,05oct90,shl  added copyright notice.
                 made #endif ANSI style.
01a,03jun88,tja  extracted from frc21.h
*/

#ifndef __INCm68153h
#define __INCm68153h

#ifdef __cplusplus
extern "C" {
#endif


/* BIM addresses */

#define BIM_CR1(base)	((char *) ((base) + 0x01))	/* control reg 1 */
#define BIM_CR2(base)	((char *) ((base) + 0x03))	/* control reg 2 */
#define BIM_CR3(base)	((char *) ((base) + 0x05))	/* control reg 3 */
#define BIM_CR4(base)	((char *) ((base) + 0x07))	/* control reg 4 */
#define BIM_VR1(base)	((char *) ((base) + 0x09))	/* vector reg 1 */
#define BIM_VR2(base)	((char *) ((base) + 0x0b))	/* vector reg 2 */
#define BIM_VR3(base)	((char *) ((base) + 0x0d))	/* vector reg 3 */
#define BIM_VR4(base)	((char *) ((base) + 0x0f))	/* vector reg 4 */

/* BIM control reg bits */

#define BIM_IRE		0x10		/* interrupt enable */
#define BIM_IRAC	0x08		/* interrupt auto-clear */
#define BIM_XIN		0x20		/* don't supply vector */
#define BIM_F		0x80		/* user defined flag */
#define BIM_FAC		0x40		/* clear flag in interrupt */

#ifdef __cplusplus
}
#endif

#endif /* __INCm68153h */
