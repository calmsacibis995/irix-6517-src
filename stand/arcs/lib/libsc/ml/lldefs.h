/*
 * |-----------------------------------------------------------|
 * | Copyright (c) 1991 MIPS Computer Systems, Inc.            |
 * | All Rights Reserved                                       |
 * |-----------------------------------------------------------|
 * |          Restricted Rights Legend                         |
 * | Use, duplication, or disclosure by the Government is      |
 * | subject to restrictions as set forth in                   |
 * | subparagraph (c)(1)(ii) of the Rights in Technical        |
 * | Data and Computer Software Clause of DFARS 52.227-7013.   |
 * |         MIPS Computer Systems, Inc.                       |
 * |         950 DeGuigne Drive                                |
 * |         Sunnyvale, CA 94086                               |
 * |-----------------------------------------------------------|
 */
/* $Header: /proj/irix6.5.7m/isms/stand/arcs/lib/libsc/ml/RCS/lldefs.h,v 1.1 1992/10/02 14:39:46 len Exp $ */

/* define double-word (long long) simulation routines for 32-bit code. */

typedef long long longlong_t;
typedef unsigned long long ulonglong_t;

typedef struct {
#ifdef _MIPSEL
        unsigned lsw;
        unsigned msw;
#else   /* _MIPSEB */
        unsigned msw;           /* most-significant-word */
        unsigned lsw;           /* least-significant-word */
#endif
} dword;

typedef union {
	longlong_t	ll;
	ulonglong_t	ull;
	dword		dw;
} llvalue;			/* 64-bit integer values */

#define MSW(x) (x).dw.msw
#define LSW(x) (x).dw.lsw

#define LL_ISNEG(x) ((signed)MSW(x) < 0)	/* returns boolean */

/* relational operations; all take two llvalues and return boolean */
#define LL_EQ(x,y)	(LSW(x) == LSW(y) && MSW(x) == MSW(y)) 
#define LL_NEQ(x,y)	(LSW(x) != LSW(y) || MSW(x) != MSW(y)) 
#define MSW_EQ(x,y)	(MSW(x) == MSW(y))
#define LL_LT(x,y)	(MSW_EQ(x,y) ? (LSW(x) < LSW(y)) : ((int)MSW(x) < (int)MSW(y))) 
#define ULL_LT(x,y)	(MSW_EQ(x,y) ? (LSW(x) < LSW(y)) : (MSW(x) < MSW(y))) 
#define LL_LE(x,y)	(MSW_EQ(x,y) ? (LSW(x) <= LSW(y)) : ((int)MSW(x) < (int)MSW(y))) 
#define ULL_LE(x,y)	(MSW_EQ(x,y) ? (LSW(x) <= LSW(y)) : (MSW(x) < MSW(y))) 
#define LL_GT(x,y)	(MSW_EQ(x,y) ? (LSW(x) > LSW(y)) : ((int)MSW(x) > (int)MSW(y))) 
#define ULL_GT(x,y)	(MSW_EQ(x,y) ? (LSW(x) > LSW(y)) : (MSW(x) > MSW(y))) 
#define LL_GE(x,y)	(MSW_EQ(x,y) ? (LSW(x) >= LSW(y)) : ((int)MSW(x) > (int)MSW(y))) 
#define ULL_GE(x,y)	(MSW_EQ(x,y) ? (LSW(x) >= LSW(y)) : (MSW(x) > MSW(y))) 

/* these routines are actually statements! */
#define LL_AND(r,x,y)	MSW(r) = MSW(x) & MSW(y), LSW(r) = LSW(x) & LSW(y)
#define LL_OR(r,x,y)	MSW(r) = MSW(x) | MSW(y), LSW(r) = LSW(x) | LSW(y)
#define LL_XOR(r,x,y)	MSW(r) = MSW(x) ^ MSW(y), LSW(r) = LSW(x) ^ LSW(y)
#define LL_NOT(r,x)	MSW(r) = ~MSW(x), LSW(r) = ~LSW(x)
#define LL_ADD(r,x,y)	MSW(r) = MSW(x) + MSW(y) + ((LSW(x)+LSW(y)) < LSW(y)); LSW(r) = LSW(x) + LSW(y)
#define LL_SUB(r,x,y)	MSW(r) = MSW(x) - MSW(y) - (LSW(x) < LSW(y)); LSW(r) = LSW(x) - LSW(y)
#define LL_NEG(r,x)	MSW(r) = ~MSW(x) + (LSW(x) == 0); LSW(r) = -LSW(x)
#define SET_LL(x,i)	LSW(x) = i, MSW(x) = (i < 0 ? -1 : 0)

/* external routines */
extern longlong_t __ll_mul (longlong_t, longlong_t);
extern longlong_t __ll_lshift (longlong_t, longlong_t);
extern longlong_t __ll_rshift (longlong_t, longlong_t);
extern longlong_t __ull_rshift (ulonglong_t, longlong_t);
extern longlong_t __ll_div (longlong_t, longlong_t);
extern ulonglong_t __ull_div (ulonglong_t, ulonglong_t);
extern longlong_t __ll_rem (longlong_t, longlong_t);
extern ulonglong_t __ull_rem (ulonglong_t, ulonglong_t);
extern void __ull_divremi (ulonglong_t *quotient, ulonglong_t *remainder, ulonglong_t dividend, unsigned short divisor);
extern longlong_t __ll_mod (longlong_t, longlong_t);
extern longlong_t __d_to_ll (double);
extern longlong_t __f_to_ll (float);
extern ulonglong_t __d_to_ull (double);
extern ulonglong_t __f_to_ull (float);
extern double __ll_to_d (longlong_t);
extern float __ll_to_f (longlong_t);
extern double __ull_to_d (ulonglong_t);
extern float __ull_to_f (ulonglong_t);
extern ulonglong_t __ll_bit_extract (ulonglong_t *addr, 
			unsigned start_bit, unsigned length);
extern ulonglong_t __ll_bit_insert (ulonglong_t *addr, 
			unsigned start_bit, unsigned length, ulonglong_t val);
