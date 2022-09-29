/* ====================================================================
 * ====================================================================
 *
 * Module: __tenscale:
 * $Revision: 1.3 $
 * $Date: 1993/10/06 22:51:01 $
 * $Author: wtk $
 * $Source: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/tenscalec.c,v $
 *
 * Revision history:
 *  29-jun-93 - Original Version
 *
 * Description: 64b fraction * 10^exp => 64b fraction * 2^bexp.
 *
 * ====================================================================
 * ====================================================================
 */

#if !defined(ling) && defined(SCCSIDS)
static char *source_file = __FILE__;
static char *rcs_id = "$Source: /proj/irix6.5.7m/isms/irix/lib/libc/src/math/RCS/tenscalec.c,v $ $Revision: 1.3 $";
#endif

#include <sgidefs.h>
#include <values.h>
#include "math_extern.h"

static void norm_and_round(__uint64_t *, int *, __uint64_t[2]);

#define DEBUG 0
#define min(x,y) ((x)<(y)? (x): (y))

/* Power of ten fractions */
/* The constants are factored so that at most two constants
 * and two multiplies are needed. Furthermore, one of the constants
 * is represented exactly - 10**n where 1<= n <= 27.
 */

static const __uint64_t tenpow[80] = {
0xa000000000000000ULL, /* tenpow[0]=(10**1)/(2**4) */
0xc800000000000000ULL, /* tenpow[1]=(10**2)/(2**7) */
0xfa00000000000000ULL, /* tenpow[2]=(10**3)/(2**10) */
0x9c40000000000000ULL, /* tenpow[3]=(10**4)/(2**14) */
0xc350000000000000ULL, /* tenpow[4]=(10**5)/(2**17) */
0xf424000000000000ULL, /* tenpow[5]=(10**6)/(2**20) */
0x9896800000000000ULL, /* tenpow[6]=(10**7)/(2**24) */
0xbebc200000000000ULL, /* tenpow[7]=(10**8)/(2**27) */
0xee6b280000000000ULL, /* tenpow[8]=(10**9)/(2**30) */
0x9502f90000000000ULL, /* tenpow[9]=(10**10)/(2**34) */
0xba43b74000000000ULL, /* tenpow[10]=(10**11)/(2**37) */
0xe8d4a51000000000ULL, /* tenpow[11]=(10**12)/(2**40) */
0x9184e72a00000000ULL, /* tenpow[12]=(10**13)/(2**44) */
0xb5e620f480000000ULL, /* tenpow[13]=(10**14)/(2**47) */
0xe35fa931a0000000ULL, /* tenpow[14]=(10**15)/(2**50) */
0x8e1bc9bf04000000ULL, /* tenpow[15]=(10**16)/(2**54) */
0xb1a2bc2ec5000000ULL, /* tenpow[16]=(10**17)/(2**57) */
0xde0b6b3a76400000ULL, /* tenpow[17]=(10**18)/(2**60) */
0x8ac7230489e80000ULL, /* tenpow[18]=(10**19)/(2**64) */
0xad78ebc5ac620000ULL, /* tenpow[19]=(10**20)/(2**67) */
0xd8d726b7177a8000ULL, /* tenpow[20]=(10**21)/(2**70) */
0x878678326eac9000ULL, /* tenpow[21]=(10**22)/(2**74) */
0xa968163f0a57b400ULL, /* tenpow[22]=(10**23)/(2**77) */
0xd3c21bcecceda100ULL, /* tenpow[23]=(10**24)/(2**80) */
0x84595161401484a0ULL, /* tenpow[24]=(10**25)/(2**84) */
0xa56fa5b99019a5c8ULL, /* tenpow[25]=(10**26)/(2**87) */
0xcecb8f27f4200f3aULL, /* tenpow[26]=(10**27)/(2**90) */

0xd0cf4b50cfe20766ULL, /* tenpow[27]=(10**55)/(2**183) */
0xd2d80db02aabd62cULL, /* tenpow[28]=(10**83)/(2**276) */
0xd4e5e2cdc1d1ea96ULL, /* tenpow[29]=(10**111)/(2**369) */
0xd6f8d7509292d603ULL, /* tenpow[30]=(10**139)/(2**462) */
0xd910f7ff28069da4ULL, /* tenpow[31]=(10**167)/(2**555) */
0xdb2e51bfe9d0696aULL, /* tenpow[32]=(10**195)/(2**648) */
0xdd50f1996b947519ULL, /* tenpow[33]=(10**223)/(2**741) */
0xdf78e4b2bd342cf7ULL, /* tenpow[34]=(10**251)/(2**834) */
0xe1a63853bbd26451ULL, /* tenpow[35]=(10**279)/(2**927) */
0xe3d8f9e563a198e5ULL, /* tenpow[36]=(10**307)/(2**1020) */

0xfd87b5f28300ca0eULL, /* tenpow[37]=(10**-28)/(2**-93) */
0xfb158592be068d2fULL, /* tenpow[38]=(10**-56)/(2**-186) */
0xf8a95fcf88747d94ULL, /* tenpow[39]=(10**-84)/(2**-279) */
0xf64335bcf065d37dULL, /* tenpow[40]=(10**-112)/(2**-372) */
0xf3e2f893dec3f126ULL, /* tenpow[41]=(10**-140)/(2**-465) */
0xf18899b1bc3f8ca2ULL, /* tenpow[42]=(10**-168)/(2**-558) */
0xef340a98172aace5ULL, /* tenpow[43]=(10**-196)/(2**-651) */
0xece53cec4a314ebeULL, /* tenpow[44]=(10**-224)/(2**-744) */
0xea9c227723ee8bcbULL, /* tenpow[45]=(10**-252)/(2**-837)     */
0xe858ad248f5c22caULL, /* tenpow[46]=(10**-280)/(2**-930) */
0xe61acf033d1a45dfULL, /* tenpow[47]=(10**-308)/(2**-1023)    */
0xe3e27a444d8d98b8ULL, /* tenpow[48]=(10**-336)/(2**-1116) */
0xe1afa13afbd14d6eULL  /* tenpow[49]=(10**-364)/(2**-1209) */
};

#ifdef NOT_USED
static const short tenexp[80] = {
1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,
55,83,111,139,167,195,223,250,279,307,
-28,-56,-84,-112,-140,-168,-196,-224,-252,-280,-308,-336,-364
};
#endif /* NOT_USED */

static const short twoexp[80] = {
4,7,10,14,17,20,24,27,30,34,37,40,44,47,50,54,57,60,64,67,70,74,77,80,84,87,90,
183,276,369,462,555,648,741,834,927,1020,
-93,-186,-279,-372,-465,-558,-651,-744,-837,-930,-1023,-1116,-1209
};



#define TEN_1		0	/* offset to 10 **   1 */
#define TEN_27	 	26	/* offset to 10 **  27 */
#define TEN_M28		37	/* offset to 10 ** -28 */
#define NUM_HI_P 11
#define NUM_HI_N 13

#define HIBITULL (1ULL << 63)

/* ====================================================================
 *
 * FunctionName: norm_and_round
 *
 * Description: normalize 128 bit fraction and round to 64 bits
 *
 * ====================================================================
 */
static void norm_and_round(p, norm, prod)
     __uint64_t *p;		/* 64b fraction		- out */
     int *norm;			/* bits shifted  	- out */
     __uint64_t prod[2];	/* 128b fraction 	- in  */
{

#if DEBUG
    printf("entering norm_and_round: prod = 0x%016llx 0x%016llx\n", 
	   prod[0], prod[1]);
#endif

  *norm = 0;
  if( ! (prod[0] & HIBITULL) ) { 
				/* leading bit is a zero 
				 * may have to normalize 
				 */
#if DEBUG
    printf("leading bit is a zero may have to normalize \n");
#endif

    if(prod[0] == ~HIBITULL &&
       prod[1] >> 62 == 0x3 ) {	/* normalization followed by round
				 * would cause carry to create
				 * extra bit, so don't normalize 
				 */
#if DEBUG
    printf("normalization followed by round would cause carry to create\nextra bit, so don't normalize. p=0x%016llx\n",
	   HIBITULL);
#endif

      *p = HIBITULL;
      return;
    }
    *p = prod[0]<<1 | prod[1]>>63; /* normalize */
    *norm=1;
    prod[1] <<= 1;

#if DEBUG
    printf("after normalization *p=0x%016llx\n", *p);
    printf("                    *norm=%d\n", *norm);
    printf("                    prod[1]=0x%016llx\n", prod[1]);
#endif

  }
  else {
    *p = prod[0];
#if DEBUG
    printf("normalization not needed *p=0x%016llx\n", *p);
#endif
  }

  if( prod[1] & HIBITULL ) {	/* first guard bit a one */
    if( (*p & 0x1ULL) ||		/* LSB on, round to even */
       prod[1] != HIBITULL) {	/* not borderline for round to even */

      /* round */
      *p++;
      if(*p==0)
	*p++;
#if DEBUG
    printf("needed round *p=0x%016llx\n", *p);
#endif
    }
  }

  return;
}

/* ====================================================================
 *
 * FunctionName: __tenscale
 *
 * Description: 64b fraction * 10^exp => 64b fraction * 2^bexp.
 *
 * ====================================================================
 */

void 
__tenscale(p,exp,bexp)
     __uint64_t *p;		/* 64b fraction 	 - inout */
     __int32_t 	exp;		/* power of ten exponent - in    */
     __int32_t 	*bexp;		/* power of two exponent - out   */
{
  __uint64_t prod[2];		/* 128b product */
  __int32_t exp_hi, exp_lo;	/* exp = exp_hi*32 + exp_lo */
  __int32_t hi, lo, tlo, thi;	/* offsets in power of ten table */
  __int32_t norm;		/* number of bits of normalization */
  __int32_t num_hi;		/* number of high exponent powers */


#if DEBUG
    printf("entering tenscale: exp = %d\n",exp);
    printf("                   *p = 0x%016llx\n", *p);
#endif

  *bexp = 0;
  if(exp > 0) {			/* split exponent */
    exp_lo = exp;
    exp_hi = 0;
    if(exp_lo>27) {
      exp_lo++;
      while(exp_lo>27) {
	exp_hi++;
	exp_lo-=28;
      }
    }
    tlo = TEN_1;
    thi = TEN_27;
    num_hi = NUM_HI_P;
#if DEBUG
    printf("exp is positive: exp_hi=%d\n",exp_hi);
    printf("                 exp_lo=%d\n",exp_lo);
#endif
  }
  else if(exp < 0) {
    exp_lo = exp;
    exp_hi = 0;
    while(exp_lo<0) {
      exp_hi++;
      exp_lo+=28;
    }
    tlo = TEN_1;
    thi = TEN_M28;
    num_hi = NUM_HI_N;
#if DEBUG
    printf("exp is negative: exp_hi=%d\n",exp_hi);
    printf("                 exp_lo=%d\n",exp_lo);
#endif
  }
  else {			/* no scaling needed */
    return;
  }
  while(exp_hi) {		/* scale */
    hi = min(exp_hi,num_hi);	/* only a few large powers of 10 */
    exp_hi -= hi;		/* could iterate in extreme case */
    hi += thi-1;
#if DEBUG
    printf("about to scale hi\n");
    printf("                 hi=%d\n",hi);
    printf("                 tenpow[hi]=0x%016llx\n",tenpow[hi]);
#endif
    __dwmultu(prod, *p, tenpow[hi]);
#if DEBUG
    printf("After dwmultu: prod = 0x%016llx 0x%016llx\n", 
	   prod[0], prod[1]);
#endif
    norm_and_round(p, &norm, prod);
#if DEBUG
    printf("After norm_and_round: *p = 0x%016llx\n", *p);
#endif
    *bexp += twoexp[hi] - norm;
#if DEBUG
    printf("New *bexp = %d\n", *bexp);
#endif
  }
  if(exp_lo) {
    lo = tlo + exp_lo -1;
#if DEBUG
    printf("about to scale lo\n");
    printf("               lo=%d\n",lo);
#endif
    __dwmultu(prod, *p, tenpow[lo]);
#if DEBUG
    printf("After dwmultu: prod = 0x%016llx 0x%016llx\n", 
	   prod[0], prod[1]);
#endif
    norm_and_round(p, &norm, prod);
#if DEBUG
    printf("After norm_and_round: *p = 0x%016llx\n", *p);
#endif
    *bexp += twoexp[lo] - norm;
#if DEBUG
    printf("New *bexp = %d\n", *bexp);
#endif
  }

  return;
}

