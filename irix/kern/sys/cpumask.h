/*
 * Copyright 1995 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ifndef _SYS_CPUMASK_H
#define _SYS_CPUMASK_H

#ident	"$Revision: 1.9 $"

#if defined(_KERNEL) || defined(_STANDALONE) || defined(_KMEMUSER)

#if (SN0)
#include <sys/SN/SN0/arch.h>
#endif
#if (SN1)
#include <sys/SN/SN1/arch.h>
#endif

#ifndef MAXCPUS			/* handle the various flavors of MAXCPUS */
#ifdef MAXCPU
#define  MAXCPUS MAXCPU
#else
#define  MAXCPUS 128
#endif
#endif	/* MAXCPUS */

#if LARGE_CPU_COUNT

#define	CPUMASK_SIZE	(MAXCPUS / _MIPS_SZLONG)

typedef struct {
	long		_bits[CPUMASK_SIZE];
} cpumask_t;

/*
 * These should really be functions, but we define them as macros
 * so the compiler will generate better code. This code currently
 * only works for 64 bit kernels. It would be trivial to add support
 * for 32 bit kernels, but then again it's very unlikely that we'll
 * ever want to run this code on 32 bit kernels.
 */
#if ((_MIPS_SZLONG == 32) && (CPUMASK_SIZE < 2))
#error macros currently only defined for 64 bit kernels
#endif

#if (MAXCPUS <= 128) 
#define	CPUMASK_CLRALL(p)	(p)._bits[0] = 0, (p)._bits[1] = 0
#define	CPUMASK_IS_ZERO(p)	((p)._bits[0] == 0 && (p)._bits[1] == 0)
#define	CPUMASK_IS_NONZERO(p)	((p)._bits[0] != 0 || (p)._bits[1] != 0)
#define CPUMASK_NOTEQ(p, q)	(((p)._bits[0] != (q)._bits[0]) ||	\
				((p)._bits[1] != (q)._bits[1]))
#define CPUMASK_SETM(p, q)	(p)._bits[0] |= (q)._bits[0],		\
				(p)._bits[1] |= (q)._bits[1]
#define CPUMASK_CLRM(p, q)	(p)._bits[0] &= ~((q)._bits[0]),	\
				(p)._bits[1] &= ~((q)._bits[1])
#define CPUMASK_ANDM(p, q)	(p)._bits[0] &= ((q)._bits[0]),		\
				(p)._bits[1] &= ((q)._bits[1])
#define CPUMASK_TSTM(p, q)	(((p)._bits[0] & (q)._bits[0]) ||	\
				 ((p)._bits[1] & (q)._bits[1]))
#define CPUMASK_CPYNOTM(p, q)	(p)._bits[0] = ~((q)._bits[0]),		\
				(p)._bits[1] = ~((q)._bits[1])
#define CPUMASK_ORNOTM(p, q)	(p)._bits[0] |= ~((q)._bits[0]),	\
				(p)._bits[1] |= ~((q)._bits[1])

/*
 * This macro assumes there is only one bit set in the second
 * argument. This is an ok assumption for now since it is only
 * used to OR in private.p_cpumask. Yuck!
 */
#define CPUMASK_ATOMSET(p, q)					\
{								\
	ASSERT(!(q)._bits[0] || !(q)._bits[1]);			\
	if ((q)._bits[0])					\
		atomicSetCpumask((cpumask_t *)&(p)._bits[0],	\
				 (cpumask_t *)&(q)._bits[0]);	\
	else							\
		atomicSetCpumask((cpumask_t *)&(p)._bits[1],	\
				 (cpumask_t *)&(q)._bits[1]);	\
}

#define CPUMASK_ATOMCLR(p, q)					\
{								\
	ASSERT(!(q)._bits[0] || !(q)._bits[1]);			\
	if ((q)._bits[0])					\
		atomicClearCpumask((cpumask_t *)&(p)._bits[0],	\
				   (cpumask_t *)&(q)._bits[0]);	\
	else							\
		atomicClearCpumask((cpumask_t *)&(p)._bits[1],	\
				   (cpumask_t *)&(q)._bits[1]);	\
}



#else /* MAXCPUS > 128 */

#if (_MIPS_SZLONG != 32)

#define	CPUMASK_CLRALL(p)	{				\
	int i;							\
								\
	for (i = 0 ; i < CPUMASK_SIZE ; i++)			\
		(p)._bits[i] = 0;				\
}

#define CPUMASK_SETM(p,q)	{				\
	int i;							\
								\
	for (i = 0 ; i < CPUMASK_SIZE ; i++)			\
		(p)._bits[i] |= ((q)._bits[i]);			\
}

#define CPUMASK_CLRM(p,q)	{				\
	int i;							\
								\
	for (i = 0 ; i < CPUMASK_SIZE ; i++)			\
		(p)._bits[i] &= ~((q)._bits[i]);		\
}

#define CPUMASK_ANDM(p,q)	{				\
	int i;							\
								\
	for (i = 0 ; i < CPUMASK_SIZE ; i++)			\
		(p)._bits[i] &= ((q)._bits[i]);			\
}

#define CPUMASK_CPYNOTM(p,q)	{				\
	int i;							\
								\
	for (i = 0 ; i < CPUMASK_SIZE ; i++)			\
		(p)._bits[i] = ~((q)._bits[i]);			\
}

#define CPUMASK_ORNOTM(p,q)	{				\
	int i;							\
								\
	for (i = 0 ; i < CPUMASK_SIZE ; i++)			\
		(p)._bits[i] |= ~((q)._bits[i]);		\
}

#define	CPUMASK_ATOMSET(p,q)	{					\
	int i;								\
									\
	for (i = 0 ; i < CPUMASK_SIZE ; i++) {				\
		if ((q)._bits[i]) {					\
			atomicSetCpumask((cpumask_t *)&(p)._bits[i],	\
				 	 (cpumask_t *)&(q)._bits[i]);	\
			break;						\
		}							\
	}								\
}


#define	CPUMASK_ATOMCLR(p,q)	{					\
	int i;								\
									\
	for (i = 0 ; i < CPUMASK_SIZE ; i++) {				\
		if ((q)._bits[i]) {					\
			atomicClearCpumask((cpumask_t *)&(p)._bits[i],	\
					   (cpumask_t *)&(q)._bits[i]);	\
			break;						\
		}							\
	}								\
}
	


__inline int CPUMASK_IS_ZERO (cpumask_t p)
{
	int i;

	for (i = 0 ; i < CPUMASK_SIZE ; i++)
		if (p._bits[i] != 0)
			return 0;
	return 1;
}

__inline int CPUMASK_IS_NONZERO (cpumask_t p)
{	
	int i;

	for (i = 0 ; i < CPUMASK_SIZE ; i++)
		if (p._bits[i] != 0)
			return 1;
	return 0;
}

__inline int CPUMASK_NOTEQ (cpumask_t p, cpumask_t q)
{
	int i;

	for (i = 0 ; i < CPUMASK_SIZE ; i++)
		if (p._bits[i] != q._bits[i])
			return 1;
	return 0;
}

__inline int CPUMASK_TSTM (cpumask_t p, cpumask_t q)
{
	int i;

	for (i = 0 ; i < CPUMASK_SIZE ; i++)
		if (p._bits[i] & q._bits[i])
			return 1;
	return 0;
}


#endif /* (_MIPS_SZLONG != 32) */
#endif /* MAXCPUS > 128 */


#if TFP
/* Only needed to allow debug of LARGE_CPU_COUNT on TFPs.
 * Problem is addressing array structures in the PDA without causing
 * TFP processor to generate Address Error exceptions.
 */
#define CPUMASK_SETB(p, bit)	{					\
	if (CPUMASK_INDEX(bit))						\
		(p)._bits[1] |= (1ULL << CPUMASK_SHFT(bit)); else \
		(p)._bits[0] |= (1ULL << CPUMASK_SHFT(bit));}

#define CPUMASK_CLRB(p, bit)	{					\
	if (CPUMASK_INDEX(bit))						\
		(p)._bits[1] &= ~(1ULL << CPUMASK_SHFT(bit)); else \
		(p)._bits[0] &= ~(1ULL << CPUMASK_SHFT(bit));}

#define CPUMASK_TSTB(p, bit)						\
	(CPUMASK_INDEX(bit) ?						\
		(p)._bits[1] & (1ULL << CPUMASK_SHFT(bit)) :	\
		(p)._bits[0] & (1ULL << CPUMASK_SHFT(bit)))

#else	/* !TFP */

#define CPUMASK_SETB(p, bit)	(p)._bits[CPUMASK_INDEX(bit)] |=	\
				   (1ULL << CPUMASK_SHFT(bit))
#define CPUMASK_CLRB(p, bit)	(p)._bits[CPUMASK_INDEX(bit)] &=	\
				  ~(1ULL << CPUMASK_SHFT(bit))
#define CPUMASK_TSTB(p, bit)	((p)._bits[CPUMASK_INDEX(bit)] &	\
				   (1ULL << CPUMASK_SHFT(bit)))
#endif


#define CPUMASK_INDEX(bit)	((bit) >> 6)
#define	CPUMASK_SHFT(bit)	((bit) & 0x3f)

/*
 * Atomically set/clear a bit in a cpumask_t. 
 * xxxTSTxx macros return previous values of the bit.
 */
#define CPUMASK_ATOMTSTSETB(p, bit)				\
	(atomicSetUint64(&(p)._bits[CPUMASK_INDEX(bit)], 	\
			(1ULL<<CPUMASK_SHFT(bit))) & 		\
		(1ULL<<CPUMASK_SHFT(bit)))
	
#define CPUMASK_ATOMTSTCLRB(p, bit)				\
	(atomicClearUint64(&(p)._bits[CPUMASK_INDEX(bit)], 	\
			(1ULL<<CPUMASK_SHFT(bit))) &		\
		(1ULL<<CPUMASK_SHFT(bit)))
	
#define CPUMASK_ATOMSETB(p, bit)				\
	atomicSetUint64(&(p)._bits[CPUMASK_INDEX(bit)], 	\
			(1ULL<<CPUMASK_SHFT(bit)))
	
#define CPUMASK_ATOMCLRB(p, bit)				\
	atomicClearUint64(&(p)._bits[CPUMASK_INDEX(bit)], 	\
			(1ULL<<CPUMASK_SHFT(bit)))
	
#else	/* LARGE_CPU_COUNT */

#ifdef EVEREST
typedef long long	cpumask_t;	/* bit position == cpuid */
#define CPUMASK_SETB(p, bit)	(p) |= 1ULL << (bit)
#define CPUMASK_CLRB(p, bit)	(p) &= ~(1ULL << (bit))
#define CPUMASK_TSTB(p, bit)	((p) & (1ULL << (bit)))

#define CPUMASK_ATOMTSTSETB(p, bit)					\
	(atomicSetUint64(&(p), (1ULL<<(bit))) & (1ULL<<(bit)))
	
#define CPUMASK_ATOMTSTCLRB(p, bit)					\
	(atomicClearUint64(&(p), (1ULL<<(bit)) & (1ULL<<(bit))))
	
#define CPUMASK_ATOMSETB(p, bit)					\
	atomicSetUint64(&(p), (1ULL<<(bit)))
	
#define CPUMASK_ATOMCLRB(p, bit)					\
	atomicClearUint64(&(p), (1ULL<<(bit)))
	
#else
typedef unsigned int	cpumask_t;	/* bit position == cpuid */
#define CPUMASK_SETB(p, bit)	(p) |= 1 << (bit)
#define CPUMASK_CLRB(p, bit)	(p) &= ~(1 << (bit))
#define CPUMASK_TSTB(p, bit)	((p) & (1 << (bit)))

#define CPUMASK_ATOMTSTSETB(p, bit)					\
	(atomicSetUint(&(p), (1<<(bit))) & (1<<(bit)))
	
#define CPUMASK_ATOMTSTCLRB(p, bit)					\
	(atomicClearUint(&(p), (1<<(bit))) & (1<<(bit)))
	
#define CPUMASK_ATOMSETB(p, bit)					\
	atomicSetUint(&(p), (1<<(bit)))
	
#define CPUMASK_ATOMCLRB(p, bit)					\
	atomicClearUint(&(p), (1<<(bit)))
	
#endif

#define	CPUMASK_CLRALL(p)	(p) = 0
#define	CPUMASK_IS_ZERO(p)	((p) == 0)
#define	CPUMASK_IS_NONZERO(p)	((p) != 0)
#define CPUMASK_NOTEQ(p, q)	((p) != (q))

#define CPUMASK_SETM(p, q)	(p) |= (q)
#define CPUMASK_CLRM(p, q)	(p) &= ~(q)
#define CPUMASK_ANDM(p, q)	(p) &= (q)
#define CPUMASK_TSTM(p, q)	((p) & (q))

#define CPUMASK_CPYNOTM(p, q)	(p) = ~(q)
#define CPUMASK_ORNOTM(p, q)	(p) |= ~(q)

#define CPUMASK_ATOMSET(p, q)	atomicSetCpumask((cpumask_t *)&(p),	\
						 (cpumask_t *)&(q))
#define CPUMASK_ATOMCLR(p, q)	atomicClearCpumask((cpumask_t *)&(p),	\
						   (cpumask_t *)&(q))

#endif	/* LARGE_CPU_COUNT */

extern cpumask_t allclr_cpumask;
extern cpumask_t allset_cpumask;

#endif /* _KERNEL || _STANDALONE || _KMEMUSER */

#endif /* _SYS_CPUMASK_H */
