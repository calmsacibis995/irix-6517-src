/*      Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. */
/*      Copyright (c) 1988 AT&T */
/*        All Rights Reserved   */

/*      THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF          */
/*      UNIX System Laboratories, Inc.                          */
/*      The copyright notice above does not evidence any        */
/*      actual or intended publication of such source code.     */

/*
 *
 * Copyright 1995, Silicon Graphics, Inc.
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

/*  
 * Reimplementation of drand48 using 64-bit arithmetic.  Drand48 is 
 *  a 48-bit linear congruential random number generator, 
 *
 * Assumptions:
 *     Arithmetic is twos complement.
 *     short is exactly 16 bits.
 *     long is at least 32 bits.
 *     __int64_t is at least 64 bits.
 *     __int32_t is exactly 32 bits.
 *
 * For o32, we also make assumptions about the byte order in __int64_t.
 *   (This is because, in o32, bit manipulations are much faster than
 *   __int64_t arithmetic.)
 */

#ident "$Revision"

#include <sgidefs.h>

#ifdef __STDC__
        #pragma weak drand48 = _drand48
        #pragma weak erand48 = _erand48
        #pragma weak lrand48 = _lrand48
        #pragma weak mrand48 = _mrand48
        #pragma weak srand48 = _srand48
        #pragma weak seed48  = _seed48
        #pragma weak lcong48 = _lcong48
        #pragma weak nrand48 = _nrand48
        #pragma weak jrand48 = _jrand48
#endif
#include "synonyms.h"

static const __uint64_t mask48 = ((__uint64_t) 1 << 48) - 1;
static const unsigned int mask16u = 0xffff; 
static const unsigned int mask32u = 0xffffffffu;
static const double two48d = 1.0 / (1 << 16) / (1 << 16) / (1 << 16);


static __inline __uint64_t unpack48(unsigned short xsubi[3])
{
#if _MIPS_SIM == _ABIO32
  __uint64_t tmp;
  unsigned short* alias = (unsigned short*) (&tmp);
  alias[0] = 0;
  alias[1] = xsubi[2];
  alias[2] = xsubi[1];
  alias[3] = xsubi[0];
  return tmp;
#else
  return (((__uint64_t) xsubi[2]) << 32) +
         (((__uint64_t) xsubi[1]) << 16) +
          (__uint64_t) xsubi[0];
#endif  
}

static __inline void pack48(__uint64_t x, unsigned short xsubi[3])
{
#if _MIPS_SIM == _ABIO32
  unsigned short* alias = (unsigned short*) (&x);
  xsubi[0] = alias[3];
  xsubi[1] = alias[2];
  xsubi[2] = alias[1];
#else
  xsubi[0] = (unsigned short) (x & mask16u);
  xsubi[1] = (unsigned short) ((x >> 16) & mask16u);
  xsubi[2] = (unsigned short) ((x >> 32) & mask16u);
#endif  
}
  

/* The 48-bit seed. */
static __uint64_t X = 
  ((__uint64_t) 0x1234 << 32) + ((__uint64_t) 0xabcd << 16) + 0x330e;

static __uint64_t a =
  ((__uint64_t) 0x5 << 32) + ((__uint64_t) 0xdeec << 16) + 0xe66d;
static __uint64_t c = 0xb;

static const __uint64_t a_init =
  ((__uint64_t) 0x5 << 32) + ((__uint64_t) 0xdeec << 16) + 0xe66d;
static const __uint64_t c_init = 0xb;



static unsigned short seed48_buf[3];

double drand48(void)
{
  X = (a * X + c) & mask48;
  return (double) X * two48d;
}

long lrand48(void)
{
  X = (a * X + c) & mask48;
  return (long) (X >> (48 - 31));
}

long mrand48(void)
{
  __int32_t tmp;
  X = (a * X + c) & mask48;
  tmp = (__int32_t) ((X >> 16) & mask32u);
  return tmp;
}

long jrand48(unsigned short xsubi[3])
{
  __int32_t tmp;
  __uint64_t x = unpack48(xsubi);
  x = a * x + c;
  pack48(x, xsubi);

  tmp = (__int32_t) (((__uint32_t) xsubi[2] << 16) + (__uint32_t) xsubi[1]);
  return tmp;
}

double erand48(unsigned short xsubi[3])
{
  __uint64_t x = unpack48(xsubi);
  x = a * x + c;
  pack48(x, xsubi);

  return (double) (x & mask48) * two48d;
}

long nrand48(unsigned short xsubi[3])
{
  __uint64_t x = unpack48(xsubi);
  x = a * x + c;
  pack48(x, xsubi);

  return (long)(xsubi[2] << 15) + (long) (xsubi[1] >> 1);
}

void srand48(long seedval)
{
  X = ((unsigned int) (seedval >> 16)) & mask16u;
  X <<= 16;
  X += seedval & mask16u;
  X <<= 16;
  X += 0x330e;
  a = a_init;
  c = c_init;
}

unsigned short* seed48(unsigned short seed16v[3])
{
  pack48(X, seed48_buf);
  X = ((__uint64_t) seed16v[2] << 32) + 
      ((__uint64_t) seed16v[1] << 16) +
      seed16v[0];
  a = a_init;
  c = c_init;
  return seed48_buf;
}

void lcong48(unsigned short param[7])
{
  X = ((__uint64_t) param[2] << 32) +
      ((__uint64_t) param[1] << 16) +
      param[0];
  a = ((__uint64_t) param[5] << 32) +
      ((__uint64_t) param[4] << 16) +
      param[3];
  c = param[6];
}

#ifdef DRIVER
/*
        This should print the sequences of integers in Tables 2
                and 1 of the TM:
        1623, 3442, 1447, 1829, 1305, ...
        657EB7255101, D72A0C966378, 5A743C062A23, ...
 */
#include <stdio.h>

main()
{
        int i;

        for (i = 0; i < 80; i++) {
                printf("%4d ", (int)(4096 * drand48()));
                printf("%.4X%.4X%.4X\n", 
                       (unsigned short) ((X >> 32) & mask16u),
                       (unsigned short) ((X >> 16) & mask16u),
                       (unsigned short) ((X >> 0) & mask16u));
        }
}
#endif
