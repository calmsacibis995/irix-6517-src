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
 * Description: Convert a decimal number to a quad precision binary.
 */
#ident "$Id: atoq.c,v 1.3 1995/02/17 23:01:38 doucette Exp $"

#define _IEEE 1
#include "fpparts.h"
#undef 	_IEEE
#include <math.h>
#include "math_extern.h"


/* ====================================================================
 *
 * FunctionName: _atoq
 *
 * Description: Convert a decimal number to a quad precision binary.
 *
 * ====================================================================
 */

long double 
_atoq(
     char *buffer,		/* Values from 0 to 9, NOT ascii. 
				 * Leading zeros have been discarded, 
				 * except for the case of zero in which
				 * case there is a single 0 digit. 
				 */

     int  ndigit,		/* 1 <= ndigit <= 37.
				 * Log2(1e37) = 122.9 bits, so the 
				 * number can be converted to a 128 bit
				 * integer with room left over.
				 */

     int exp)			/* The minimum quad numbers are
				 *
				 * 4.9406564584124654e-324 (denorm) and 
				 * 2.2250738585072014e-308 (norm).
				 *
				 * The maximum value is 
				 *
				 * 1.7976931348623157e+308, 
				 *
				 * therefore 
				 * -324-ndigit = -361 <= exp <= 308.
				 */
{
  _ldblval value;		/* the result */
#define HI (value.fwords.hi)	/* top 64b of 124b integer */
#define LO (value.fwords.lo)	/* bottom 60b of 124b integer */
				/* later converted to 128b fraction */

  unsigned guard;		/* First guard bit */
  unsigned long rest;		/* Remaining 21 guard bits */
  __uint64_t lolo;		/* lower 60b of LO */
  unsigned int lohi;		/* upper  4b of LO */
  double z;			/* canonical HI */

  int exphi, explo;		/* binary exponent */
  int nzero;			/* number of non-zeros */
  int i;			/* loop index */
  int x;			/* temporary */
  int losign=0;			/* sign of LO */

  char *bufferend;		/* pointer to char after last digit */
  
  HI = 0;

  /* Check for zero and treat it as a special case */

  if ((int) *buffer == 0){
    return ( 0.0L );
  }

  /* Convert the decimal digits to a binary integer. */

  bufferend = buffer + ndigit;
  HI = 0;			
  LO = *buffer++;

  /* the first batch of digits fits in LO */
  for( i=1 ; buffer < bufferend && i<19; buffer++, i++ ){
    LO *= 10;
    LO += *buffer;
  }

#if DEBUG
  printf("halfway thru conversion: HI=0x%016llx\n",HI);
  printf("                         LO=0x%016llx\n",LO);
  printf("                         i=%d\n",i);
#endif

  if( buffer < bufferend ) {
    /* The second batch of digits affects both HI and LO */

    lolo = LO & (1Ull<<60)-1;	/* Split LO into 60b  */
    lohi = LO >> 60;		/*            and 4b pieces */

#if DEBUG
  printf("After LO split: lohi=0x%x\n",lohi);
  printf("                lolo=0x%016llx\n",lolo);
#endif

    for( ; buffer < bufferend; buffer++ ){
      lolo = 10*lolo + *buffer;	/* Multiply by 10 and add digit */
#if DEBUG
  printf("After 10*: lolo=0x%016llx\n",lolo);
#endif
      lohi = (unsigned int)(10*lohi + (lolo>>60));
      HI   = 10*HI   + (lohi>>4);
      lolo &= (1Ull<<60)-1;	/* discard carries already propagated */
#if DEBUG
  printf("After carry discard: lolo=0x%016llx\n",lolo);
#endif
      lohi &= (1U  << 4)-1;	/* Are these two statements needed??? */

#if DEBUG
  printf("After iteration: lohi=0x%x\n",lohi);
  printf("                 lolo=0x%016llx\n",lolo);
#endif

    }
    /* Reconstitute LO from its pieces. */
    LO = lolo | ((__uint64_t) lohi)<<60;

#if DEBUG
  printf("After reconstitution: HI=0x%016llx\n",HI);
  printf("                      LO=0x%016llx\n",LO);
#endif

  }

#if DEBUG
  printf("after conversion: HI=0x%016llx\n",HI);
  printf("                  LO=0x%016llx\n",LO);
#endif

  /* Normalize HI-LO */

  exphi = 128;			/* convert from 128b int to fraction */
  if (HI == 0){			/* 64 bit shift */
    HI = LO;
    LO = 0;
    exphi -= 64;
  }

  /* Count number of non-zeroes in HI */
  nzero = 0;
  if ( HI >> (32        ) ){ nzero  = 32; }
  if ( HI >> (16 + nzero) ){ nzero += 16; }
  if ( HI >> ( 8 + nzero) ){ nzero +=  8; }
  if ( HI >> ( 4 + nzero) ){ nzero +=  4; }
  if ( HI >> ( 2 + nzero) ){ nzero +=  2; }
  if ( HI >> ( 1 + nzero) ){ nzero +=  1; }
  if ( HI >> (     nzero) ){ nzero +=  1; }

  /* Normalize */
  HI = (HI << (64-nzero)) | (LO >> (nzero));
  LO <<= 64-nzero;
  exphi -= 64-nzero;

  /* At this point we have a 128b fraction and a binary exponent 
   * but have yet to incorporate the decimal exponent.
   */

#if DEBUG
  printf("after normalization: HI=0x%016llx\n",HI);
  printf("                     LO=0x%016llx\n",LO);
  printf("                     nzero=%d\n",nzero);
  printf("                     exphi=%d\n",exphi);
#endif

  /* multiply by 10^exp */

  __qtenscale(&value,exp,&x);
  exphi += x;

#if DEBUG
  printf("after multiplication: HI=0x%016llx\n",HI);
  printf("                      LO=0x%016llx\n",LO);
  printf("                     exphi=%d\n",exphi);
#endif
  /* Round to 107 bits */
  /* Take the 128 bits of HI and LO and divide them as follows
   *
   * before	HI	LO	guard	rest
   *            64b     64b
   * after	53b	54b	1b	20b
   *		       =11 of HI
   *		       +43 of LO
   */

  rest = (unsigned long)(LO & (1ULL<<20)-1);
  LO >>= 20;

#if DEBUG
  printf("during split: LO=0x%016llx\n",LO);
#endif

  guard = LO & 1;
  LO >>= 1;

#if DEBUG
  printf("during split: LO=0x%016llx\n",LO);
#endif

  LO |= (HI & (1ULL<<11)-1) << 43;
  HI >>= 11;

#if DEBUG
  printf("after split: HI=0x%016llx\n",HI);
  printf("             LO=0x%016llx\n",LO);
  printf("             guard=%d\n",guard);
  printf("             rest=%lx\n",rest);
#endif

  /*	LO&1	guard	rest	Action
   * 	
   * 	dc	0	dc	none
   * 	1	1	dc	round
   * 	0	1	0	none
   * 	0	1	!=0	round
   */

  if(guard) {
    if(LO&1 || rest) {
      LO++;			/* round */
      HI += LO>>54;
      LO &= (1ULL<<54)-1;
      if(HI>>53) {		/* carry all the way across */
	HI >>= 1;		/* renormalize */
	exphi ++;
      }
    }
  }

  explo = exphi-53;
  
#if DEBUG
  printf("after rounding: HI=0x%016llx\n",HI);
  printf("                LO=0x%016llx\n",LO);
  printf("                exphi=%d\n",exphi);
  printf("                explo=%d\n",explo);
#endif

  /* Apply Dekker's algorithm */
  /* Determine whether HI <- (double) HI+LO would change HI */
  if( LO & (1ULL<<53) ) {	/* high bit of LO on */
    if(HI & 1  ||  LO & ((1ULL<<53)-1)) { 
      HI++;			/* round */
      if(HI & (1ULL<<53)) {	/* ripple carry */
	HI >>= 1;
	exphi++;
      }
      LO = (1ULL<<54) - LO;	/* complement LO */
      losign = 1;

#if DEBUG
  printf("after dekker: HI=0x%016llx\n",HI);
  printf("              LO=0x%016llx\n",LO);
  printf("              exphi=%d\n",exphi);
  printf("              explo=%d\n",explo);
  printf("              losign=%d\n",losign);
#endif
      
    }
  }
  if( LO ) {
    /* normalize LO */
    if(LO & (1ULL<<53)) {	/* LO = 0x0020000000000000 */
      explo++;			/* in all other cases this bit's zero */
      LO >>=1;

#if DEBUG
  printf("after right shift: LO=0x%016llx\n",LO);
  printf("                   explo=%d\n",explo);
#endif
      
    }
    else {
      while( ! (LO & 0x0010000000000000ULL) ){

#if DEBUG
  printf("before left shift: LO=0x%016llx\n",LO);
  printf("                  explo=%d\n",explo);
#endif
      
	explo--;
	LO <<= 1;
      }
    }
    explo--;			/* we now have only 53 bits */
  }
  
#if DEBUG
  printf("after LO normalize: LO=0x%016llx\n",LO);
  printf("                    explo=%d\n",explo);
#endif

  /*
   * Check for overflow and denorm......
   * IEEE Double Precision Format
   * (From Table 7-8 of Kane and Heinrich)
   * 
   * Fraction bits               52
   * Emax                     +1023
   * Emin                     -1022
   * Exponent bias            +1023
   * Exponent bits               11
   * Integer bit             hidden
   * Total width in bits         64
   */
  
  if (exphi > 1024) {		/* overflow */
    value.qparts.hi.d = value.qparts.lo.d =  HUGE_VAL;

#if DEBUG
  printf("Overflow: value.qparts.hi.d=0x%016llx\n",value.qparts.hi.d);
  printf("          value.qparts.lo.d=0x%016llx\n",value.qparts.lo.d);
#endif

    return value.ldbl;
  }
  if (exphi <= -1022) {		/* HI denorm or underflow */

    value.qparts.lo.d = 0;	/* therefore LO is zero */
    exphi += 1022;
    if( exphi < -52 ) {		/* underflow */
      value.qparts.hi.d = 0;

#if DEBUG
  printf("HI underflow: HI=0x%016llx\n",HI);
  printf("              exphi=%d\n",exphi);
#endif

    }
    else {			/* denorm */
      rest = (unsigned long)(HI & (1UL << exphi-1)-1);
      guard = (HI & (1UL << exphi)) >> exphi;
      HI >>= 1-exphi;		/* exponent is zero */

#if DEBUG
  printf("HI denorm: HI=0x%016llx\n",HI);
  printf("           rest=0x%016llx\n",rest);
  printf("           guard=%d\n",guard);
  printf("           exphi=%d\n",exphi);
#endif

      /* Round */
      if( guard ) {
	if( HI&1 || rest ) {
	  HI++;
	  if( HI == (1ULL << 52) ) { /* carry created a normal number */
	    HI = 0;
	    EXPONENT(HI) = 1;
	  }

#if DEBUG
  printf("Round denorm: HI=0x%016llx\n",HI);
#endif

	}
      }
    }
  }
  else {			/* HI is normal */
    HI &= ~(1ULL << 52);	/* hide hidden bit */
    EXPONENT(HI) = exphi + 1022; /* add bias */

#if DEBUG
  printf("Normal HI: HI=0x%016llx\n",HI);
#endif

  }

  if( explo <= -1022 ) {	/* LO denorm or underflow */
    explo += 1022;
    if( explo < -52 ) {		/* underflow */
      value.qparts.lo.d = 0;

#if DEBUG
  printf("LO underflow: LO=0x%016llx\n",LO);
  printf("              explo=%d\n",explo);
#endif

    }
    else {			/* denorm */
      rest = (unsigned long)(LO & (1UL << explo-1)-1);
      guard = (LO & (1UL << explo)) >> explo;
      LO >>= 1-explo;		/* exponent is zero */

#if DEBUG
  printf("LO denorm: LO=0x%016llx\n",LO);
  printf("           explo=%d\n",explo);
  printf("           guard=%d\n",guard);
  printf("           rest=0x%016llx\n",rest);
#endif

      /* Round */
      if( guard ) {
	if( LO&1 || rest ) {
	  LO++;
	  if( LO == (1ULL << 52) ) { /* carry created normal number */
	    LO = 0;
	    EXPONENT(LO) = 1;	
  }

#if DEBUG
  printf("After LO round: LO=0x%016llx\n",LO);
#endif

	}
      }
    }
  }
  else {			/* LO is normal */
    if(LO) {
      LO &= ~(1ULL << 52);	/* hide hidden bit */
      EXPONENT(LO) = explo + 1022; /* add bias */
    }
#if DEBUG
  printf("Normal LO before making canonical: LO=0x%016llx\n",LO);
#endif
    /* Make representation canonical */
    z = value.qparts.lo.d + value.qparts.hi.d;
    value.qparts.lo.d -= (z - value.qparts.hi.d);
    value.qparts.hi.d = z;
#if DEBUG
  printf("After making canonical: HI=0x%016llx\n",HI);
  printf("                      : LO=0x%016llx\n",LO);
#endif

  }
    
  SIGNBIT(LO) = losign;
  return value.ldbl;
}
