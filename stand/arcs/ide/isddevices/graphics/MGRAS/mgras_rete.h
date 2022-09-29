/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef MGRAS_RETE_H

#define	MGRAS_RETE_H

/* Address range per RSS */
#define RSS_RANGE 0x1000

/* RE4 Status register bits */
#define RE4COUNT_BITS 	0x3
#define RE4NUMBER_BITS 	0x6
#define RE4VERSION_BITS 0xf0
#define RE4CMDFIFOEMPTY_BITS 0x100
#define RE4BUSY_BITS 0x200
#define RE4PP1BUSY_BITS 0x400

/* Bit mask defs */
#define THIRTYTWOBIT_MASK 	0xffffffff
#define THIRTYONEBIT_MASK 	0x7fffffff
#define THIRTYBIT_MASK 		0x3fffffff
#define TWENTYNINEBIT_MASK 	0x1fffffff
#define TWENTYEIGHTBIT_MASK 	0xfffffff
#define TWENTYSIXBIT_MASK 	0x3ffffff
#define TWENTYFIVEBIT_MASK 	0x1ffffff
#define TWENTYFOURBIT_MASK 	0xffffff
#define TWENTYTHREEBIT_MASK 	0x7fffff
#define TWENTYONEBIT_MASK 	0x1fffff
#define TWENTYBIT_MASK 		0xfffff
#define NINETEENBIT_MASK 	0x7ffff
#define EIGHTEENBIT_MASK 	0x3ffff
#define SEVENTEENBIT_MASK 	0x1ffff
#define SIXTEENBIT_MASK 	0xffff
#define TWELVEBIT_MASK 		0xfff
#define ELEVENBIT_MASK 		0x7ff
#define NINEBIT_MASK		0xff
#define EIGHTBIT_MASK		0xff
#define SEVENBIT_MASK		0x7f
#define SIXBIT_MASK		0x3f
#define FIVEBIT_MASK		0x1f
#define FOURBIT_MASK		0xf
#define ONEBIT_MASK		0x1


/* convert RSS register addresses to HQ3 space */

#define HQ3_RSS_SPACE(addr) \
	(addr << 2) | 1 << 11;

#endif /* MGRAS_RETE_H */
