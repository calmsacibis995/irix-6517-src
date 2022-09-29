#ifndef _SYS_NODEMASK_H
#define _SYS_NODEMASK_H
/******************************************************************************
 * nodemask.h
 *
 * 	Define Node mask and associated operations. 
 * 	node mask : Bit mask of the compact node id's in a system.
 * 	Node id's represented by these masks are expected to be in a 
 * 	contiguous space. (which is true for compact node id).
 * 
 * 	Make node mask to be a 64 bit, since that's the immediate requirement.
 *	But all operations on the nodemask should be done via the macros,
 *	to allow the internal implementation to be changed when we need
 *	larger number of bits (> 64) to represent node masks.
 *
 * Copyright 1995 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 ******************************************************************************/

#ident	"$Revision: 1.13 $"

#if defined(_KERNEL) || defined(_STANDALONE) || defined(_KMEMUSER)

#if (SN0)
#include <sys/SN/SN0/arch.h> 	/* needed for MAX_COMPACT_NODES */
#endif
#if (SN1)
#include <sys/SN/SN1/arch.h> 	/* needed for MAX_COMPACT_NODES */
#endif

#define CNODEMASK_BOOTED_MASK		boot_cnodemask
#define CNODEMASK_BIPW    64

#ifndef SN0XXL	/* MAXCPUS 128p (64 nodes) or less */

#define CNODEMASK_SIZE    1
typedef __uint64_t cnodemask_t;

#define CNODEMASK_WORD(p,w)     (p)
#define CNODEMASK_SET_WORD(p,w,val)     (p) = val
#define CNODEMASK_CLRALL(p)     (p) = 0
#define CNODEMASK_SETALL(p)     (p) = ~((cnodemask_t)0)
#define CNODEMASK_IS_ZERO(p)	((p) == 0)
#define CNODEMASK_IS_NONZERO(p)	((p) != 0)
#define CNODEMASK_NOTEQ(p, q)	((p) != (q))
#define CNODEMASK_EQ(p, q)      ((p) == (q))
#define CNODEMASK_LSB_ISONE(p)  ((p) & 0x1ULL)

#define CNODEMASK_ZERO()        ((cnodemask_t)0)
#define CNODEMASK_CVTB(bit)     (1ULL << (bit))
#define CNODEMASK_SETB(p, bit)	((p) |= 1ULL << (bit))
#define CNODEMASK_CLRB(p, bit)	((p) &= ~(1ULL << (bit)))
#define CNODEMASK_TSTB(p, bit)	((p) & (1ULL << (bit)))

#define CNODEMASK_SETM(p, q)	((p) |= (q))
#define CNODEMASK_CLRM(p, q)	((p) &= ~(q))
#define CNODEMASK_ANDM(p, q)	((p) &= (q))
#define CNODEMASK_TSTM(p, q)	((p) & (q))

#define CNODEMASK_CPYNOTM(p, q)	((p) = ~(q))
#define CNODEMASK_CPY(p, q)     ((p) = (q))
#define CNODEMASK_ORNOTM(p, q)	((p) |= ~(q))
#define CNODEMASK_SHIFTL(p)     ((p) <<= 1)
#define CNODEMASK_SHIFTR(p)     ((p) >>= 1)
#define CNODEMASK_SHIFTL_PTR(p)     (*(p) <<= 1)
#define CNODEMASK_SHIFTR_PTR(p)     (*(p) >>= 1)

/* Atomically set or clear a particular bit */
#define CNODEMASK_ATOMSET_BIT(p, bit) atomicSetUlong((cnodemask_t *)&(p), (1ULL<<(bit))) 
#define CNODEMASK_ATOMCLR_BIT(p, bit) atomicClearUlong((cnodemask_t *)&(p), (1ULL<<(bit)))

/* Atomically set or clear a collection of bits */
#define CNODEMASK_ATOMSET(p, q)  atomicSetUlong((cnodemask_t *)&(p), q)
#define CNODEMASK_ATOMCLR(p, q)  atomicClearUlong((cnodemask_t *)&(p), q)

/* Atomically set or clear a collection of bits, returning the old value */
#define CNODEMASK_ATOMSET_MASK(__old, p, q)	{ \
		(__old) = atomicSetUlong((cnodemask_t *)&(p), q); \
}
#define CNODEMASK_ATOMCLR_MASK(__old, p, q)	{ \
		(__old) = atomicClearUlong((cnodemask_t *)&(p),q); \
}

#define CNODEMASK_FROM_NUMNODES(n)	((~(cnodemask_t)0)>>(CNODEMASK_BIPW-(n)))

#else  /* SN0XXL - MAXCPUS > 128 */

#define CNODEMASK_SIZE    (MAX_COMPACT_NODES / CNODEMASK_BIPW)

typedef struct {
        __uint64_t _bits[CNODEMASK_SIZE];
} cnodemask_t;

#define CNODEMASK_WORD(p,w)  \
	((w >= 0 && w < CNODEMASK_SIZE) ? (p)._bits[(w)] : 0)
#define CNODEMASK_SET_WORD(p,w,val)  { 				\
	if (w >= 0 && w < CNODEMASK_SIZE) 			\
		(p)._bits[(w)] = val;				\
}

#define CNODEMASK_CLRALL(p)       {                             \
        int i;                                                  \
                                                                \
        for (i = 0 ; i < CNODEMASK_SIZE ; i++)                  \
                (p)._bits[i] = 0;                               \
}

#define CNODEMASK_SETALL(p)       {                             \
        int i;                                                  \
                                                                \
        for (i = 0 ; i < CNODEMASK_SIZE ; i++)                  \
                (p)._bits[i] = ~(0);                            \
}

#define CNODEMASK_LSB_ISONE(p)  ((p)._bits[0] & 0x1ULL)


#define CNODEMASK_SETM(p,q)       {                             \
        int i;                                                  \
                                                                \
        for (i = 0 ; i < CNODEMASK_SIZE ; i++)                  \
                (p)._bits[i] |= ((q)._bits[i]);                 \
}

#define CNODEMASK_CLRM(p,q)       {                             \
        int i;                                                  \
                                                                \
        for (i = 0 ; i < CNODEMASK_SIZE ; i++)                  \
                (p)._bits[i] &= ~((q)._bits[i]);                \
}

#define CNODEMASK_ANDM(p,q)       {                             \
        int i;                                                  \
                                                                \
        for (i = 0 ; i < CNODEMASK_SIZE ; i++)                  \
                (p)._bits[i] &= ((q)._bits[i]);                 \
}

#define CNODEMASK_CPY(p, q)  {					\
        int i;                                                  \
                                                                \
        for (i = 0 ; i < CNODEMASK_SIZE ; i++)                  \
                (p)._bits[i] = (q)._bits[i];	                \
}

#define CNODEMASK_CPYNOTM(p,q)    {                             \
        int i;                                                  \
                                                                \
        for (i = 0 ; i < CNODEMASK_SIZE ; i++)                  \
                (p)._bits[i] = ~((q)._bits[i]);                 \
}

#define CNODEMASK_ORNOTM(p,q)     {                             \
        int i;                                                  \
                                                                \
        for (i = 0 ; i < CNODEMASK_SIZE ; i++)                  \
                (p)._bits[i] |= ~((q)._bits[i]);                \
}

#define CNODEMASK_INDEX(bit)      ((bit) >> 6)
#define CNODEMASK_SHFT(bit)       ((bit) & 0x3f)


#define CNODEMASK_SETB(p, bit)	 				\
	(p)._bits[CNODEMASK_INDEX(bit)] |= (1ULL << CNODEMASK_SHFT(bit))


#define CNODEMASK_CLRB(p, bit)					\
	(p)._bits[CNODEMASK_INDEX(bit)] &= ~(1ULL << CNODEMASK_SHFT(bit)) 


#define CNODEMASK_TSTB(p, bit)		\
	((p)._bits[CNODEMASK_INDEX(bit)] & (1ULL << CNODEMASK_SHFT(bit))) 

/** Probably should add atomic update for entire cnodemask_t struct **/

/* Atomically set or clear a particular bit */
#define CNODEMASK_ATOMSET_BIT(p, bit) \
        (atomicSetUlong((unsigned long *)&(p)._bits[CNODEMASK_INDEX(bit)], (1ULL << CNODEMASK_SHFT(bit))));
#define CNODEMASK_ATOMCLR_BIT(__old, p, bit) \
        (atomicClearUlong((unsigned long *)&(p)._bits[CNODEMASK_INDEX(bit)], (1ULL << CNODEMASK_SHFT(bit))));

/* Atomically set or clear a collection of bits */
#define CNODEMASK_ATOMSET(p, q) { \
        int i;				\
					\
        for (i = 0 ; i < CNODEMASK_SIZE ; i++) { \
	      atomicSetUlong((unsigned long *)&(p)._bits[i], (q)._bits[i]);  \
        }				\
}
#define CNODEMASK_ATOMCLR(p, q) { \
        int i;				\
                        		\
        for (i = 0 ; i < CNODEMASK_SIZE ; i++) {	\
	      atomicClearUlong((unsigned long *)&(p)._bits[i], (q)._bits[i]); \
        }				\
}

/* Atomically set or clear a collection of bits, returning the old value */
#define CNODEMASK_ATOMSET_MASK(__old, p, q)  { \
        int i;				\
					\
        for (i = 0 ; i < CNODEMASK_SIZE ; i++) { \
           (__old)._bits[i] =	 \
	      atomicSetUlong((unsigned long *)&(p)._bits[i], (q)._bits[i]);  \
        }				\
}
#define CNODEMASK_ATOMCLR_MASK(__old, p, q) {					\
        int i;				\
                        		\
        for (i = 0 ; i < CNODEMASK_SIZE ; i++) {	\
           (__old)._bits[i] =				\
	      atomicClearUlong((unsigned long *)&(p)._bits[i], (q)._bits[i]); \
        }				\
}

__inline cnodemask_t CNODEMASK_CVTB(int bit) 
{
	cnodemask_t __tmp;
	CNODEMASK_CLRALL(__tmp);
	CNODEMASK_SETB(__tmp,bit);
	return(__tmp);
}


__inline cnodemask_t CNODEMASK_ZERO(void)
{
	cnodemask_t __tmp;
	CNODEMASK_CLRALL(__tmp);
	return(__tmp);
}

__inline int CNODEMASK_IS_ZERO (cnodemask_t p)
{
        int i;

        for (i = 0 ; i < CNODEMASK_SIZE ; i++)
                if (p._bits[i] != 0)
                        return 0;
        return 1;
}

__inline int CNODEMASK_IS_NONZERO (cnodemask_t p)
{
        int i;

        for (i = 0 ; i < CNODEMASK_SIZE ; i++)
                if (p._bits[i] != 0)
                        return 1;
        return 0;
}

__inline int CNODEMASK_NOTEQ (cnodemask_t p, cnodemask_t q)
{
        int i;

        for (i = 0 ; i < CNODEMASK_SIZE ; i++)
                if (p._bits[i] != q._bits[i])
                        return 1;
        return 0;
}

__inline int CNODEMASK_EQ (cnodemask_t p, cnodemask_t q)
{
        int i;

        for (i = 0 ; i < CNODEMASK_SIZE ; i++)
                if (p._bits[i] != q._bits[i])
                        return 0;
        return 1;
}


__inline int CNODEMASK_TSTM (cnodemask_t p, cnodemask_t q)
{
        int i;

        for (i = 0 ; i < CNODEMASK_SIZE ; i++)
                if (p._bits[i] & q._bits[i])
                        return 1;
        return 0;
}

__inline void CNODEMASK_SHIFTL_PTR (cnodemask_t *p)
{
        int i;
        __uint64_t upper;

        /*
         * shift words starting with the last word
         * of the vector and work backward to the first
         * word updating the low order bits with the
         * high order bit of the prev word.
         */
        for (i=(CNODEMASK_SIZE-1); i > 0; --i) {
	   upper = (p->_bits[i-1] & (1ULL<<(CNODEMASK_BIPW-1))) ? 1 : 0;
           p->_bits[i] <<= 1;
           p->_bits[i] |= upper;
        }
        p->_bits[i] <<= 1;
}

__inline void CNODEMASK_SHIFTR_PTR (cnodemask_t *p)
{
        int i;
        __uint64_t lower;

        /*
         * shift words starting with the first word
         * of the vector and work forward to the last
         * word updating the high order bit with the
         * low order bit of the next word.
         */
        for (i=0; i < (CNODEMASK_SIZE-2); ++i) {
	   lower = (p->_bits[i+1] & (0x1)) ? 1 : 0;
           p->_bits[i] >>= 1;
           p->_bits[i] |= (lower<<((CNODEMASK_BIPW-1)));
        }
        p->_bits[i] >>= 1;
}

__inline cnodemask_t CNODEMASK_FROM_NUMNODES(int n)
{
	cnodemask_t __tmp;
	int i;
	CNODEMASK_CLRALL(__tmp);
	for (i=0; i<n; i++) {
		CNODEMASK_SETB(__tmp, i);
	}
	return(__tmp);
}

#endif /* SN0XXL */

extern cnodemask_t boot_cnodemask;

#endif /* _KERNEL || _STANDALONE || _KMEMUSER */

#endif /* _SYS_NODEMASK_H */
