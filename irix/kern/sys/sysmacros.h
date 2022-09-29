/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.32 $"

#ifndef __SYSMACROS_H__
#define __SYSMACROS_H__

#include <sys/param.h>

/*
 * Some macros for units conversion
 */

/* a click is a page.  A segment is area mapped by a click of ptes (4Meg)*/
/* Core clicks to segments and vice versa */

#ifdef CPSSHIFT
#define ctos(x)		(((x) + (NCPS-1)) >> CPSSHIFT)
#define	ctost(x)	((x) >> CPSSHIFT)
#define	stoc(x)		((x) << CPSSHIFT)
#else
#define ctos(x)		(((x) + (NCPS-1)) / NCPS)
#define	ctost(x)	((x) / NCPS)
#define	stoc(x)		((x) * NCPS)
#endif

/* bytes to truncated pages - same as btoct */
#define btotp(x)	(( __psunsigned_t)(x)>>BPTSHFT)

/* bytes to segments and vice-versa */
#ifdef BPSSHIFT
#define stob(x)		((__psunsigned_t)(x) << BPSSHIFT)
#define btos(x)		(((__psunsigned_t)(x) + (NBPS-1)) >> BPSSHIFT)
#define btost(x)	((__psunsigned_t)(x) >> BPSSHIFT)
#else
#define stob(x)		((__psunsigned_t)(x) * NBPS)
#define btos(x)		(((__psunsigned_t)(x) + (NBPS-1)) / NBPS)
#define btost(x)	(((__psunsigned_t)(x)) / NBPS)
#endif

/* true if 2 virtual addresses lie in the same segment */
#define samesegment(a,b)						\
	(((__psunsigned_t)(a) & ~SOFFMASK) == ((__psunsigned_t)(b) & ~SOFFMASK))

/* return # pages required to span a given addr/length */
#define numpages(v,l)	 btoc((((__psunsigned_t)(v)) & (NBPC - 1)) + (l))
#define io_numpages(v,l) io_btoc((((__psunsigned_t)(v)) & (IO_NBPC - 1)) + (l))

/* inumber to disk address */
#ifdef INOSHIFT
#define	itod(x)	(daddr_t)(((unsigned)(x)+(2*INOPB-1))>>INOSHIFT)
#else
#define	itod(x)	(daddr_t)(((unsigned)(x)+(2*INOPB-1))/INOPB)
#endif

/* inumber to disk offset */
#ifdef INOSHIFT
#define	itoo(x)	(int)(((unsigned)(x)+(2*INOPB-1))&(INOPB-1))
#else
#define	itoo(x)	(int)(((unsigned)(x)+(2*INOPB-1))%INOPB)
#endif

/* clicks to bytes */
#ifdef BPCSHIFT
#define	ctob(x)		((__psunsigned_t)(x)<<BPCSHIFT)
#define	ctob64(x)	((__uint64_t)(x)<<BPCSHIFT)
#define	io_ctob(x)	((__psunsigned_t)(x)<<IO_BPCSHIFT)
#else
#define	ctob(x)		((__psunsigned_t)(x)*NBPC)
#define	ctob64(x)	((__uint64_t)(x)*NBPC)
#define	io_ctob(x)	((__psunsigned_t)(x)*IO_NBPC)
#endif

/* bytes to clicks */
#ifdef BPCSHIFT
#define	btoc(x)		(((__psunsigned_t)(x)+(NBPC-1))>>BPCSHIFT)
#define	btoct(x)	((__psunsigned_t)(x)>>BPCSHIFT)
#define	btoc64(x)	(((__uint64_t)(x)+(NBPC-1))>>BPCSHIFT)
#define	btoct64(x)	((__uint64_t)(x)>>BPCSHIFT)
#define	io_btoc(x)	(((__psunsigned_t)(x)+(IO_NBPC-1))>>IO_BPCSHIFT)
#define	io_btoct(x)	((__psunsigned_t)(x)>>IO_BPCSHIFT)
#else
#define	btoc(x)		(((__psunsigned_t)(x)+(NBPC-1))/NBPC)
#define	btoct(x)	((__psunsigned_t)(x)/NBPC)
#define	btoc64(x)	(((__uint64_t)(x)+(NBPC-1))/NBPC)
#define	btoct64(x)	((__uint64_t)(x)/NBPC)
#define	io_btoc(x)	(((__psunsigned_t)(x)+(IO_NBPC-1))/IO_NBPC)
#define	io_btoct(x)	((__psunsigned_t)(x)/IO_NBPC)
#endif

/* off_t bytes to clicks */
#ifdef BPCSHIFT
#define	offtoc(x)	(((__uint64_t)(x)+(NBPC-1))>>BPCSHIFT)
#define	offtoct(x)	((off_t)(x)>>BPCSHIFT)
#else
#define	offtoc(x)	(((__uint64_t)(x)+(NBPC-1))/NBPC)
#define	offtoct(x)	((off_t)(x)/NBPC)
#endif

/* clicks to off_t bytes */
#ifdef BPCSHIFT
#define	ctooff(x)	((off_t)(x)<<BPCSHIFT)
#else
#define	ctooff(x)	((off_t)(x)*NBPC)
#endif

#define O_BITSMAJOR	7	/* # of SVR3 major device bits */
#define O_BITSMINOR	8	/* # of SVR3 minor device bits */
#define O_MAXMAJ	0x7f	/* SVR3 max major value */
#define O_MAXMIN	0xff	/* SVR3 max minor value */

#define L_BITSMAJOR     14      /* # of SVR4 major device bits */
#define L_BITSMINOR     18      /* # of SVR4 minor device bits */
#define L_MAXMAJ        0x1ff    /* Although 14 bits are reserved,
                                ** major numbers are currently restricted
                                ** to 9 bits.
                                */

#define L_MAXMIN        0x3ffff /* MAX minor */

#ifdef _KERNEL

/* major part of a device internal to the kernel */

/* #define NMAJORENTRY     512     Number of entries in major array 
 * XXXdh unresolved issue here: fixed or lboot-determined size?.... 
 */
extern short MAJOR[];
#define major(x)	(int)(MAJOR[(unsigned)((x)>>L_BITSMINOR) & L_MAXMAJ])
#define bmajor(x)	(int)(MAJOR[(unsigned)((x)>>L_BITSMINOR) & L_MAXMAJ])


#else

/* major part of a device external from the kernel */
#define	major(x)	(int)(((unsigned)(x)>>L_BITSMINOR) & L_MAXMAJ)

#endif	/* _KERNEL */

/* minor part of a device */
#define	minor(x)	(int)((x)&L_MAXMIN)

/* make a device number */
#define	makedev(x,y)	(dev_t)(((x)<<L_BITSMINOR) | (y&L_MAXMIN))

/*
 *   emajor() allows kernel/driver code to print external major numbers
 *   eminor() allows kernel/driver code to print external minor numbers
 */

#define	emajor(x)	(int)(((unsigned)(x)>>L_BITSMINOR) & L_MAXMAJ)
#define	eminor(x)	(int)((x)&L_MAXMIN)

#endif /* __SYSMACROS_H__ */
