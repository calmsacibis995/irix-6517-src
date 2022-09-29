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
#ident "$Id: atod.c,v 1.6 1996/07/24 00:12:02 vegas Exp $"

#define _IEEE 1
#include "fpparts.h"
#undef 	_IEEE
#include <math.h>
#include <sgidefs.h>
#include "math_extern.h"




/* ====================================================================
 *
 * FunctionName: _atod
 *
 * Description: Convert a decimal number to a double precision binary.
 *
 * ====================================================================
 */

double 
_atod(
     char *buffer,		/* Values from 0 to 9, NOT ascii. 
				 * Leading zeros have been discarded, 
				 * except for the case of zero in which
				 * case there is a single 0 digit. 
				 */

     __int32_t  ndigit,		/* The number of digits in buffer.
				 * 1 <= ndigit <= 17.
				 * Log2(1e17) = 56.5 bits, so the 
				 * number can be converted to a 64 bit
				 * integer with room left over.
				 */

     __int32_t dexp)		/* Decimal exponent.
				 * The minimum double numbers are
				 *
				 * 4.9406564584124654e-324 (denorm) and 
				 * 2.2250738585072014e-308 (norm).
				 *
				 * The maximum value is 
				 *
				 * 1.7976931348623157e+308, 
				 *
				 * therefore 
				 * -324-max_ndigit+1= -340 <=exp<= 308.
				 */
{
  __uint64_t value;		/* Value develops as follows:
				 * 1) decimal digits as an integer
				 * 2) left adjusted fraction
				 * 3) right adjusted fraction
				 * 4) exponent and fraction
				 */
  __uint32_t guard;		/* First guard bit */
  __uint64_t rest;		/* Remaining guard bits */

  __int32_t bexp;		/* binary exponent */
  __int32_t nzero;		/* number of non-zero bits */
  __int32_t sexp;		/* scaling exponent */

  char *bufferend;		/* pointer to char after last digit */
  
#ifdef _ATOD_DEBUG
  printf("initially: ndigit=%d\n",ndigit);
  printf("           dexp=%d\n",dexp);
#endif

  /* Check for zero and treat it as a special case */

  if (buffer == 0){
    return 0.0; 
  }

  /* Convert the decimal digits to a binary integer. */

  bufferend = buffer + ndigit;
  value = 0;			

  while( buffer < bufferend ){
    value *= 10;
    value += *buffer++;
  }

#ifdef _ATOD_DEBUG
  printf("after conversion: value=0x%016llx\n",value);
#endif

  /* Check for zero and treat it as a special case */

  if (value == 0){
    return 0.0; 
  }

  /* Normalize value */

  bexp = 64;			/* convert from 64b int to fraction */

  /* Count number of non-zeroes in value */
  nzero = 0;
  if ( value >> (32        ) ){ nzero  = 32; }
  if ( value >> (16 + nzero) ){ nzero += 16; }
  if ( value >> ( 8 + nzero) ){ nzero +=  8; }
  if ( value >> ( 4 + nzero) ){ nzero +=  4; }
  if ( value >> ( 2 + nzero) ){ nzero +=  2; }
  if ( value >> ( 1 + nzero) ){ nzero +=  1; }
  if ( value >> (     nzero) ){ nzero +=  1; }

  /* Normalize */
  value <<= (__uint64_t) (64-nzero);
  bexp -= 64-nzero;

  /* At this point we have a 64b fraction and a binary exponent 
   * but have yet to incorporate the decimal exponent.
   */

#ifdef _ATOD_DEBUG
  printf("after normalization: value=0x%016llx\n",value);
  printf("                     nzero=%d\n",nzero);
  printf("                     bexp=%d\n",bexp);
#endif

  /* multiply by 10^dexp */

  __tenscale(&value, dexp, &sexp);
  bexp += sexp;

#ifdef _ATOD_DEBUG
  printf("after scaling: value=0x%016llx\n",value);
  printf("               bexp=%d\n",bexp);
#endif

  if (bexp <= -1022) {		/* HI denorm or underflow */
    bexp += 1022;
    if( bexp < -53 ) {		/* guaranteed underflow */
      value = 0;
    
#ifdef _ATOD_DEBUG
  printf("underflow: value=0x%016llx\n",value);
  printf("           bexp=%d\n",bexp);
#endif

    }
    else {			/* denorm or possible underflow */
      __int32_t lead0;

      lead0 = 12-bexp;		/* 12 sign and exponent bits */

      /* we must special case right shifts of more than 63 */

      if ( lead0 > 64 )
      {
	   rest = value;
	   guard = 0;
	   value = 0;
      }
      else if ( lead0 == 64 )
      {
	   rest = value & (1ULL << 63)-1;
	   guard = (__uint32_t) ((value>> 63) & 1ULL);
	   value = 0;
      }
      else
      {
          rest = value & (1ULL << lead0-1)-1;
          guard = (__uint32_t) ((value>> lead0-1) & 1ULL);
          value >>= (__uint64_t) lead0; /* exponent is zero */
      }
      
#ifdef _ATOD_DEBUG
  printf("denorm: value=0x%016llx\n",value);
  printf("        guard=%d\n",guard);
  printf("        rest=0x%016llx\n",rest);
  printf("        bexp=%d\n",bexp);
#endif

      /* Round */
      if( guard ) {
	if( value&1 || rest ) {
	  value++;
      
#ifdef _ATOD_DEBUG
  printf("rounded denorm: value=0x%016llx\n",value);
#endif

	  if( value == (1ULL << 52) ) { /* carry created normal number */
	    value = 0;
	    EXPONENT(value) = 1;
      
#ifdef _ATOD_DEBUG
  printf("carry changed denorm to normal number\n",value);
#endif

	  }
	}
      }
    }
  }
  else {			/* not zero or denorm */
    /* Round to 53 bits */

    rest = value & (1ULL<<10)-1;
    value >>= 10;
    guard = (__uint32_t) value & 1;
    value >>= 1;
    
#ifdef _ATOD_DEBUG
  printf("after split: value=0x%016llx\n",value);
  printf("             guard=%d\n",guard);
  printf("             rest=%llx\n",rest);
#endif

    /*	value&1	guard	rest	Action
     * 	
     * 	dc	0	dc	none
     * 	1	1	dc	round
     * 	0	1	0	none
     * 	0	1	!=0	round
     */

    if(guard) {
      if(value&1 || rest) {
	value++;			/* round */
	if(value>>53) {		/* carry all the way across */
	  value >>= 1;		/* renormalize */
	  bexp ++;
	}
      }
    }

#ifdef _ATOD_DEBUG
  printf("after rounding: value=0x%016llx\n",value);
  printf("                bexp=%d\n",bexp);
#endif

    /*
     * Check for overflow
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
  
    if (bexp > 1024) {		/* overflow */

#ifdef _ATOD_DEBUG
  printf("Overflow\n",value);
#endif
      return HUGE_VAL;
    }
    else {			/* value is normal */
      value &= ~(1ULL << 52);	/* hide hidden bit */
      EXPONENT(value) = (__uint32_t) (bexp + 1022); /* add bias */

#ifdef _ATOD_DEBUG
  printf("Normal value: value=0x%016llx\n",value);
#endif
    
    }
  }
  return *((double *) &value);
}
