/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * Description: 128b fraction * 10^exp => 128b fraction * 2^bexp.
 */
#ident "$Id: qtenscale.c,v 1.3 1994/10/04 19:37:43 jwag Exp $"

#include <sgidefs.h>
#include <values.h>
#include "math_extern.h"

#define DEBUG 0
#define min(x,y) ((x)<(y)? (x): (y))

static void qnorm_and_round(__uint64_t [2], int *, __uint64_t [4]);
/* Power of ten fractions */

static const __uint64_t tenpow[80][2] = {

0xa000000000000000ULL, 0x0000000000000000ULL, /* (10**1)/(2**4) */
0xc800000000000000ULL, 0x0000000000000000ULL, /* (10**2)/(2**7) */
0xfa00000000000000ULL, 0x0000000000000000ULL, /* (10**3)/(2**10) */
0x9c40000000000000ULL, 0x0000000000000000ULL, /* (10**4)/(2**14) */
0xc350000000000000ULL, 0x0000000000000000ULL, /* (10**5)/(2**17) */
0xf424000000000000ULL, 0x0000000000000000ULL, /* (10**6)/(2**20) */
0x9896800000000000ULL, 0x0000000000000000ULL, /* (10**7)/(2**24) */
0xbebc200000000000ULL, 0x0000000000000000ULL, /* (10**8)/(2**27) */
0xee6b280000000000ULL, 0x0000000000000000ULL, /* (10**9)/(2**30) */
0x9502f90000000000ULL, 0x0000000000000000ULL, /* (10**10)/(2**34) */
0xba43b74000000000ULL, 0x0000000000000000ULL, /* (10**11)/(2**37) */
0xe8d4a51000000000ULL, 0x0000000000000000ULL, /* (10**12)/(2**40) */
0x9184e72a00000000ULL, 0x0000000000000000ULL, /* (10**13)/(2**44) */
0xb5e620f480000000ULL, 0x0000000000000000ULL, /* (10**14)/(2**47) */
0xe35fa931a0000000ULL, 0x0000000000000000ULL, /* (10**15)/(2**50) */
0x8e1bc9bf04000000ULL, 0x0000000000000000ULL, /* (10**16)/(2**54) */
0xb1a2bc2ec5000000ULL, 0x0000000000000000ULL, /* (10**17)/(2**57) */
0xde0b6b3a76400000ULL, 0x0000000000000000ULL, /* (10**18)/(2**60) */
0x8ac7230489e80000ULL, 0x0000000000000000ULL, /* (10**19)/(2**64) */
0xad78ebc5ac620000ULL, 0x0000000000000000ULL, /* (10**20)/(2**67) */
0xd8d726b7177a8000ULL, 0x0000000000000000ULL, /* (10**21)/(2**70) */
0x878678326eac9000ULL, 0x0000000000000000ULL, /* (10**22)/(2**74) */
0xa968163f0a57b400ULL, 0x0000000000000000ULL, /* (10**23)/(2**77) */
0xd3c21bcecceda100ULL, 0x0000000000000000ULL, /* (10**24)/(2**80) */
0x84595161401484a0ULL, 0x0000000000000000ULL, /* (10**25)/(2**84) */
0xa56fa5b99019a5c8ULL, 0x0000000000000000ULL, /* (10**26)/(2**87) */
0xcecb8f27f4200f3aULL, 0x0000000000000000ULL, /* (10**27)/(2**90) */
0x813f3978f8940984ULL, 0x4000000000000000ULL, /* (10**28)/(2**94) */
0xa18f07d736b90be5ULL, 0x5000000000000000ULL, /* (10**29)/(2**97) */
0xc9f2c9cd04674edeULL, 0xa400000000000000ULL, /* (10**30)/(2**100) */
0xfc6f7c4045812296ULL, 0x4d00000000000000ULL, /* (10**31)/(2**103) */

0x9dc5ada82b70b59dULL, 0xf020000000000000ULL, /* (10**32)/(2**107) */
0xc2781f49ffcfa6d5ULL, 0x3cbf6b71c76b25fbULL, /* (10**64)/(2**213) */
0xefb3ab16c59b14a2ULL, 0xc5cfe94ef3ea101eULL, /* (10**96)/(2**319) */
0x93ba47c980e98cdfULL, 0xc66f336c36b10137ULL, /* (10**128)/(2**426) */
0xb616a12b7fe617aaULL, 0x577b986b314d6009ULL, /* (10**160)/(2**532) */
0xe070f78d3927556aULL, 0x85bbe253f47b1417ULL, /* (10**192)/(2**638) */
0x8a5296ffe33cc92fULL, 0x82bd6b70d99aaa70ULL, /* (10**224)/(2**745) */
0xaa7eebfb9df9de8dULL, 0xddbb901b98feeab8ULL, /* (10**256)/(2**851) */
0xd226fc195c6a2f8cULL, 0x73832eec6fff3112ULL, /* (10**288)/(2**957) */

0xccccccccccccccccULL, 0xcccccccccccccccdULL, /* (10**-1)/(2**-3) */
0xa3d70a3d70a3d70aULL, 0x3d70a3d70a3d70a4ULL, /* (10**-2)/(2**-6) */
0x83126e978d4fdf3bULL, 0x645a1cac083126e9ULL, /* (10**-3)/(2**-9) */
0xd1b71758e219652bULL, 0xd3c36113404ea4a9ULL, /* (10**-4)/(2**-13) */
0xa7c5ac471b478423ULL, 0x0fcf80dc33721d54ULL, /* (10**-5)/(2**-16) */
0x8637bd05af6c69b5ULL, 0xa63f9a49c2c1b110ULL, /* (10**-6)/(2**-19) */
0xd6bf94d5e57a42bcULL, 0x3d32907604691b4dULL, /* (10**-7)/(2**-23) */
0xabcc77118461cefcULL, 0xfdc20d2b36ba7c3dULL, /* (10**-8)/(2**-26) */
0x89705f4136b4a597ULL, 0x31680a88f8953031ULL, /* (10**-9)/(2**-29) */
0xdbe6fecebdedd5beULL, 0xb573440e5a884d1bULL, /* (10**-10)/(2**-33) */
0xafebff0bcb24aafeULL, 0xf78f69a51539d749ULL, /* (10**-11)/(2**-36) */
0x8cbccc096f5088cbULL, 0xf93f87b7442e45d4ULL, /* (10**-12)/(2**-39) */
0xe12e13424bb40e13ULL, 0x2865a5f206b06fbaULL, /* (10**-13)/(2**-43) */
0xb424dc35095cd80fULL, 0x538484c19ef38c94ULL, /* (10**-14)/(2**-46) */
0x901d7cf73ab0acd9ULL, 0x0f9d37014bf60a10ULL, /* (10**-15)/(2**-49) */
0xe69594bec44de15bULL, 0x4c2ebe687989a9b4ULL, /* (10**-16)/(2**-53) */
0xb877aa3236a4b449ULL, 0x09befeb9fad487c3ULL, /* (10**-17)/(2**-56) */
0x9392ee8e921d5d07ULL, 0x3aff322e62439fcfULL, /* (10**-18)/(2**-59) */
0xec1e4a7db69561a5ULL, 0x2b31e9e3d06c32e5ULL, /* (10**-19)/(2**-63) */
0xbce5086492111aeaULL, 0x88f4bb1ca6bcf584ULL, /* (10**-20)/(2**-66) */
0x971da05074da7beeULL, 0xd3f6fc16ebca5e03ULL, /* (10**-21)/(2**-69) */
0xf1c90080baf72cb1ULL, 0x5324c68b12dd6338ULL, /* (10**-22)/(2**-73) */
0xc16d9a0095928a27ULL, 0x75b7053c0f178294ULL, /* (10**-23)/(2**-76) */
0x9abe14cd44753b52ULL, 0xc4926a9672793543ULL, /* (10**-24)/(2**-79) */
0xf79687aed3eec551ULL, 0x3a83ddbd83f52205ULL, /* (10**-25)/(2**-83) */
0xc612062576589ddaULL, 0x95364afe032a819dULL, /* (10**-26)/(2**-86) */
0x9e74d1b791e07e48ULL, 0x775ea264cf55347eULL, /* (10**-27)/(2**-89) */
0xfd87b5f28300ca0dULL, 0x8bca9d6e188853fcULL, /* (10**-28)/(2**-93) */
0xcad2f7f5359a3b3eULL, 0x096ee45813a04330ULL, /* (10**-29)/(2**-96) */
0xa2425ff75e14fc31ULL, 0xa1258379a94d028dULL, /* (10**-30)/(2**-99) */
0x81ceb32c4b43fcf4ULL, 0x80eacf948770ced7ULL, /* (10**-31)/(2**-102) */

0xcfb11ead453994baULL, 0x67de18eda5814af2ULL, /* (10**-32)/(2**-106) */
0xa87fea27a539e9a5ULL, 0x3f2398d747b36224ULL, /* (10**-64)/(2**-212) */
0x88b402f7fd75539bULL, 0x11dbcb0218ebb414ULL, /* (10**-96)/(2**-318) */
0xddd0467c64bce4a0ULL, 0xac7cb3f6d05ddbdfULL, /* (10**-128)/(2**-425) */
0xb3f4e093db73a093ULL, 0x59ed216765690f57ULL, /* (10**-160)/(2**-531) */
0x91ff83775423cc06ULL, 0x7b6306a34627ddcfULL, /* (10**-192)/(2**-637) */
0xece53cec4a314ebdULL, 0xa4f8bf5635246428ULL, /* (10**-224)/(2**-744) */
0xc0314325637a1939ULL, 0xfa911155fefb5309ULL, /* (10**-256)/(2**-850) */
0x9becce62836ac577ULL, 0x4ee367f9430aec33ULL  /* (10**-288)/(2**-956) */
};

#ifdef NEVER
static const int tenexp[80] = {
   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,
  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,
  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31, 

  32,  64,  96, 128, 160, 192, 224, 256, 288, 

  -1,  -2,  -3,  -4,  -5,  -6,  -7,  -8,  -9, -10,
 -11, -12, -13, -14, -15, -16, -17, -18, -19, -20,
 -21, -22, -23, -24, -25, -26, -27, -28, -29, -30, -31, 

 -32, -64, -96,-128,-160,-192,-224,-256,-288
};
#endif

static const int twoexp[80] = {
   4,   7,  10,  14,  17,  20,  24,  27,  30,  34,
  37,  40,  44,  47,  50,  54,  57,  60,  64,  67,
  70,  74,  77,  80,  84,  87,  90,  94,  97, 100, 103, 

 107, 213, 319, 426, 532, 638, 745, 851, 957, 

  -3,  -6,  -9, -13, -16, -19, -23, -26, -29, -33,
 -36, -39, -43, -46, -49, -53, -56, -59, -63, -66,
 -69, -73, -76, -79, -83, -86, -89, -93, -96, -99,-102,

-106,-212,-318,-425,-531,-637,-744,-850,-956
};


#define TEN_1		0	/* offset to 10 **   1 */
#define TEN_32	 	31	/* offset to 10 **  32 */
#define TEN_M1		40	/* offset to 10 **  -1 */
#define TEN_M32		71	/* offset to 10 ** -32 */
#define TEN_LAST	80	/* offset one past last entry */

#define HIBITULL (1ULL << 63)

/* ====================================================================
 *
 * FunctionName: qnorm_and_round
 *
 * Description: normalize 256 bit fraction and round to 128 bits
 *
 * ====================================================================
 */
static void
qnorm_and_round(
     __uint64_t p[2],		/* 128b fraction	- out */
     int *norm,			/* bits shifted  	- out */
     __uint64_t prod[4])	/* 256b fraction 	- in  */
{
  *norm = 0;
  if( ! (prod[0] & HIBITULL) ) { 
				/* leading bit is a zero 
				 * may have to normalize 
				 */
    if(prod[0] == ~HIBITULL &&
       prod[1] == ~0ULL &&
       prod[2] >> 62 == 0x3 ) {	/* normalization followed by round
				 * would cause carry to create
				 * extra bit, so don't normalize 
				 */
      p[0] = HIBITULL;
      p[1] = 0ULL;
      return;
    }
    p[0] = prod[0]<<1 | prod[1]>>63; /* normalize */
    p[1] = prod[1]<<1 | prod[2]>>63;
    *norm=1;
    prod[2] <<= 1;
  }
  else {
    p[0] = prod[0];
    p[1] = prod[1];
  }

  if( prod[2] & HIBITULL ) {	/* first guard bit a one */
    if( (p[1] & 0x1ULL) ||	/* LSB on, round to even */
       prod[2] != HIBITULL ||	/* not borderline for round to even */
       prod[3] ) {

      /* round */
      p[1]++;
      if(p[1]==0)
	p[0]++;
    }
  }
  return;
}

/* ====================================================================
 *
 * FunctionName: __qtenscale
 *
 * Description: 128b fraction * 10^exp => 128b fraction * 2^bexp.
 *
 * ====================================================================
 */

void 
__qtenscale(
     _ldblval	*p,		/* 128b fraction 	 - inout */
     int 	exp,		/* power of ten exponent - in    */
     int 	*bexp)		/* power of two exponent - out   */
{
  __uint64_t prod[4];		/* 256b product */
  int exp_hi, exp_lo;		/* exp = exp_hi*32 + exp_lo */
  int hi, lo, t1, t32;		/* offsets in power of ten table */
  int norm;			/* number of bits of normalization */

  *bexp = 0;
  if(exp > 0) {			/* split exponent */
    exp_hi = exp >> 5;
    exp_lo = exp & 0x1F;
    t1 = TEN_1;
    t32 = TEN_32;
  }
  else if(exp < 0) {
#if DEBUG
    printf("exp is negative: exp = %d\n",exp);
#endif
    exp_hi = (-exp) >> 5;
    exp_lo = (-exp) & 0x1F;
    t1 = TEN_M1;
    t32 = TEN_M32;
#if DEBUG
    printf("                 exp_hi=%d\n",exp_hi);
    printf("                 exp_lo=%d\n",exp_lo);
#endif
  }
  else {			/* no scaling needed */
    return;
  }
  while(exp_hi) {		/* scale */
    hi = min(exp_hi,9);		/* only nine large powers of 10 */
    exp_hi -= hi;		/* could iterate in extreme case */
    hi += t32-1;
#if DEBUG
    printf("about to scale hi\n");
    printf("                 hi=%d\n",hi);
#endif
    _qwmultu(prod, p, (_ldblval *)tenpow[hi]);
#if DEBUG
    printf("After qwmultu: prod = %llx %llx %llx %llx\n", 
	   prod[0], prod[1], prod[2], prod[3]);
#endif
    qnorm_and_round((__uint64_t *)p, &norm, prod);
#if DEBUG
    printf("After qnorm_and_round: p = %llx %llx\n", 
	   p[0], p[1]);
#endif
    *bexp += twoexp[hi] - norm;
#if DEBUG
    printf("New *bexp = %d\n", *bexp);
#endif
  }
  if(exp_lo) {
    lo = t1 + exp_lo -1;
#if DEBUG
    printf("about to scale lo\n");
    printf("                 lo=%d\n",lo);
#endif
    _qwmultu(prod, p, (_ldblval *)tenpow[lo]);
#if DEBUG
    printf("After qwmultu: prod = %llx %llx %llx %llx\n", 
	   prod[0], prod[1], prod[2], prod[3]);
#endif
    qnorm_and_round((__uint64_t *)p, &norm, prod);
#if DEBUG
    printf("After qnorm_and_round: p = %llx %llx\n", 
	   p[0], p[1]);
#endif
    *bexp += twoexp[lo] - norm;
#if DEBUG
    printf("New *bexp = %d\n", *bexp);
#endif
  }
  return;
}

