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
#ident "$Id: ltoa.c,v 1.4 1995/03/23 06:08:14 jwag Exp $"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UPPER16(x) (x >> 16)
#define LOWER16(x) (x & 0xffff)

#if (_MIPS_SZLONG == 64)
#define UPPER32(x) (x >> 32)
#define LOWER32(x) (x & 0xffffffff)
#endif /* (_MIPS_SZLONG == 64) */

/* constants to divide by 100, 10000 and 100000000 by multiplying by the
 * reciprocal.
 */

#define recip10power2         655
#define recip10power4      429496

#if (_MIPS_SZLONG == 64)
#define recip10power9 18446744073
#endif /* (_MIPS_SZLONG == 64) */

/* Tens digit for numbers from 0 to 99 */

static const char upper [100] = {
  '0', '0', '0', '0', '0', '0', '0', '0', '0', '0',
  '1', '1', '1', '1', '1', '1', '1', '1', '1', '1',
  '2', '2', '2', '2', '2', '2', '2', '2', '2', '2',
  '3', '3', '3', '3', '3', '3', '3', '3', '3', '3',
  '4', '4', '4', '4', '4', '4', '4', '4', '4', '4',
  '5', '5', '5', '5', '5', '5', '5', '5', '5', '5',
  '6', '6', '6', '6', '6', '6', '6', '6', '6', '6',
  '7', '7', '7', '7', '7', '7', '7', '7', '7', '7',
  '8', '8', '8', '8', '8', '8', '8', '8', '8', '8',
  '9', '9', '9', '9', '9', '9', '9', '9', '9', '9'
};

/* low order digit for numbers from 0 to 99 */

static const char lower [100] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'
};

/* Convert an unsigned long value to its equivalent ascii representation.
 * Leading 0 are placed if the width is more.
 * The address returned is one byte beyond the last character placed in
 * the buffer.
 */

static char *
_ultoa_pad_zero (unsigned long value, char *buf, long width)
{
  long i;

  if (value < 10) {
    for (i = 1; i < width; i++)
      *buf++ = '0';
    *buf++ = '0' + (char)value;
  }
  else
  if (value < 100) {
    for (i = 2; i < width; i++)
      *buf++ = '0';
    *buf++ = upper [value];
    *buf++ = lower [value];
  }
  else
  if (value < 100000) {

    /* compute value / 100 and value % 100 by multiplying by reciprocal */

    unsigned long d4_2;
    unsigned long d1_0;

    d4_2 = UPPER16(value * recip10power2);
    d1_0 = value - d4_2 * 100;
    if (d1_0 >= 100) {
      d4_2++;
      d1_0 -= 100;
    }

    if (d4_2 >= 100) {
      unsigned long d4;
      unsigned long d3_2;
      d4 = UPPER16(d4_2 * recip10power2);
      d3_2 = d4_2 - d4 * 100;
      if (d3_2 >= 100) {
        d4++;
        d3_2 -= 100;
      }
      for (i = 5; i < width; i++)
        *buf++ = '0';
      *buf++ = '0' + (char)d4;
      *buf++ = upper [d3_2];
      *buf++ = lower [d3_2];
      *buf++ = upper [d1_0];
      *buf++ = lower [d1_0];
    }
    else {
      for (i = 4; i < width; i++)
        *buf++ = '0';
      *buf++ = upper [d4_2];
      *buf++ = lower [d4_2];
      *buf++ = upper [d1_0];
      *buf++ = lower [d1_0];
    }
  }
  else
#if (_MIPS_SZLONG != 64)
  if (value < 1000000000) {

    /* compute value / 100000 and value % 100000
     * by multiplying by the reciprocal of 100000
     * This is achieved with cross multiplication and addition.
     * The quotient would be less than 10 ** 5.
     */

    unsigned long d8_4;
    unsigned long d3_0;
    unsigned long val16u, val16l;
    unsigned long vll;
    unsigned long vlu;
    unsigned long vul;
    unsigned long vuu;
    val16u = UPPER16(value);
    val16l = LOWER16(value);
    vll = val16l * LOWER16(recip10power4);
    vlu = val16l * UPPER16(recip10power4);
    vul = val16u * LOWER16(recip10power4);
    vuu = val16u * UPPER16(recip10power4);
    d8_4 =   UPPER16(UPPER16(vll) + LOWER16(vlu) + LOWER16(vul))
           + UPPER16(vlu) + UPPER16(vul) + vuu;
    d3_0 = value - d8_4 * 10000;
    if (d3_0 >= 10000) {
      d3_0 -= 10000;
      d8_4 += 1;
    }

    for (i = 9; i < width; i++)
      *buf++ = '0';
    buf = _ultoa_pad_zero (d8_4, buf, 5);
    buf = _ultoa_pad_zero (d3_0, buf, 4);
  }
#else /* (_MIPS_SZLONG != 64) */
  if (value < 1000000000) {

    /* compute value / 100000 and value % 100000
     * by multiplying by the reciprocal of 100000
     * The quotient would be less than 10 ** 5.
     */

    unsigned long d8_4;
    unsigned long d3_0;
    d8_4 = UPPER32(value * recip10power4);
    d3_0  = value - d8_4 * 10000;
    if (d3_0 >= 10000) {
      d8_4++;
      d3_0 -= 10000;
    }
    for (i = 9; i < width; i++)
      *buf++ = '0';
    buf = _ultoa_pad_zero (d8_4, buf, 5);
    buf = _ultoa_pad_zero (d3_0, buf, 4);
  }
  else
  if (value < 1000000000000000000ll) {

    /* compute value / 10 ** 9 and value % 10 ** 9
     * by multiplying by the reciprocal of 10 ** 9
     * This is achieved with cross multiplication and addition.
     */

    unsigned long d17_9;
    unsigned long d8_0;
    unsigned long val32u, val32l;
    unsigned long vll;
    unsigned long vlu;
    unsigned long vul;
    unsigned long vuu;
    val32u = UPPER32(value);
    val32l = LOWER32(value);
    vll = val32l * LOWER32(recip10power9);
    vlu = val32l * UPPER32(recip10power9);
    vul = val32u * LOWER32(recip10power9);
    vuu = val32u * UPPER32(recip10power9);
    d17_9 =   UPPER32(UPPER32(vll) + LOWER32(vlu) + LOWER32(vul))
            + UPPER32(vlu) + UPPER32(vul) + vuu;
    d8_0  = value - d17_9 * 1000000000;
    if (d8_0 >= 1000000000) {
      d8_0 -= 1000000000;
      d17_9 += 1;
    }

    for (i = 18; i < width; i++)
      *buf++ = '0';
    buf = _ultoa_pad_zero (d17_9, buf, 9);
    buf = _ultoa_pad_zero (d8_0, buf, 9);
  }
#endif /* (_MIPS_SZLONG != 64) */

  return buf;
}


/* Convert an unsigned long in value to an ascii string and place it in
 * the buffer specified by buf.
 * Return the number of characters in the converted string.
 */

int
_ultoa (unsigned long value, char *buf)
{
  char * start = buf;

  if (value < 10) {
    *buf++ = '0' + (char)value;
  }
  else
  if (value < 100) {
    *buf++ = upper [value];
    *buf++ = lower [value];
  }
  else
  if (value < 100000) {
    unsigned long d4_2;
    unsigned long d1_0;
    d4_2 = UPPER16(value * recip10power2);
    d1_0 = value - d4_2 * 100;
    if (d1_0 >= 100) {
      d4_2++;
      d1_0 -= 100;
    }
    if (d4_2 >= 100) {
      unsigned long d4;
      unsigned long d3_2;
      d4 = UPPER16(d4_2 * recip10power2);
      d3_2 = d4_2 - d4 * 100;
      if (d3_2 >= 100) {
        d4++;
        d3_2 -= 100;
      }
      *buf++ = '0' + (char)d4;
      *buf++ = upper [d3_2];
      *buf++ = lower [d3_2];
      *buf++ = upper [d1_0];
      *buf++ = lower [d1_0];
    }
    else {
      if ((*buf++ = upper [d4_2]) == '0')
        buf--;
      *buf++ = lower [d4_2];
      *buf++ = upper [d1_0];
      *buf++ = lower [d1_0];
    }
  }
  else
#if (_MIPS_SZLONG != 64)
  if (value < 1000000000) {
    unsigned long d8_4;
    unsigned long d3_0;
    unsigned long val16u, val16l;
    unsigned long vll;
    unsigned long vlu;
    unsigned long vul;
    unsigned long vuu;
    val16u = UPPER16(value);
    val16l = LOWER16(value);
    vll = val16l * LOWER16(recip10power4);
    vlu = val16l * UPPER16(recip10power4);
    vul = val16u * LOWER16(recip10power4);
    vuu = val16u * UPPER16(recip10power4);
    d8_4 =   UPPER16(UPPER16(vll) + LOWER16(vlu) + LOWER16(vul))
           + UPPER16(vlu) + UPPER16(vul) + vuu;
    d3_0 = value - d8_4 * 10000;
    if (d3_0 >= 10000) {
      d3_0 -= 10000;
      d8_4 += 1;
    }

    buf += _ultoa (d8_4, buf);
    buf = _ultoa_pad_zero (d3_0, buf, 4);
  }
  else {
    int d9 = 0;
    do {
      d9++;
      value -= 1000000000;
    } while (value >= 1000000000);
    *buf++ = '0' + d9;
    buf = _ultoa_pad_zero (value, buf, 9);
  }
#else /* (_MIPS_SZLONG != 64) */
  if (value < 1000000000) {
    unsigned long d8_4;
    unsigned long d3_0;
    d8_4 = UPPER32(value * recip10power4);
    d3_0  = value - d8_4 * 10000;
    if (d3_0 >= 10000) {
      d8_4++;
      d3_0 -= 10000;
    }
    buf += _ultoa (d8_4, buf);
    buf = _ultoa_pad_zero (d3_0, buf, 4);
  }
  else
  if (value < 1000000000000000000ll) {
    unsigned long d17_9;
    unsigned long d8_0;
    unsigned long val32u, val32l;
    unsigned long vll;
    unsigned long vlu;
    unsigned long vul;
    unsigned long vuu;
    val32u = UPPER32(value);
    val32l = LOWER32(value);
    vll = val32l * LOWER32(recip10power9);
    vlu = val32l * UPPER32(recip10power9);
    vul = val32u * LOWER32(recip10power9);
    vuu = val32u * UPPER32(recip10power9);
    d17_9 =   UPPER32(UPPER32(vll) + LOWER32(vlu) + LOWER32(vul))
            + UPPER32(vlu) + UPPER32(vul) + vuu;
    d8_0  = value - d17_9 * 1000000000;
    if (d8_0 >= 1000000000) {
      d8_0 -= 1000000000;
      d17_9 += 1;
    }

    buf += _ultoa (d17_9, buf);
    buf = _ultoa_pad_zero (d8_0, buf, 9);
  }
  else {
    unsigned long d19_18 = 0;
    do {
      d19_18++;
      value -= 1000000000000000000ll;
    } while (value >= 1000000000000000000ll);
    if (d19_18 >= 10)
      *buf++ = upper [d19_18];
    *buf++ = lower [d19_18];
    buf = _ultoa_pad_zero (value, buf, 18);
  }
#endif /* (_MIPS_SZLONG != 64) */

  *buf = '\0';

  return (int)(buf - start);
} /* _ultoa */


/* Convert a signed long in value to an ascii string and place it in
 * the buffer specified by buf.
 * Return the number of characters in the converted string.
 */

int
_ltoa (long value, char *buf)
{
  char * start = buf;

  if (value < 0) {
    value = 0 - value;
    if (value < 0) {
#if (_MIPS_SZLONG == 64)
      strcpy (buf, "-4611686018427387904");
      return 20;
#else
      strcpy (buf, "-2147483648");
      return 11;
#endif /* (_MIPS_SZLONG == 64) */
    }
    else
      *buf++ = '-';
  }

  if (value < 10) {
    *buf++ = '0' + (char)value;
  }
  else
  if (value < 100) {
    *buf++ = upper [value];
    *buf++ = lower [value];
  }
  else
  if (value < 100000) {
    unsigned long d4_2;
    unsigned long d1_0;
    d4_2 = UPPER16(value * recip10power2);
    d1_0 = value - d4_2 * 100;
    if (d1_0 >= 100) {
      d4_2++;
      d1_0 -= 100;
    }
    if (d4_2 >= 100) {
      unsigned long d4;
      unsigned long d3_2;
      d4 = UPPER16(d4_2 * recip10power2);
      d3_2 = d4_2 - d4 * 100;
      if (d3_2 >= 100) {
        d4++;
        d3_2 -= 100;
      }
      *buf++ = '0' + (char)d4;
      *buf++ = upper [d3_2];
      *buf++ = lower [d3_2];
      *buf++ = upper [d1_0];
      *buf++ = lower [d1_0];
    }
    else {
      if ((*buf++ = upper [d4_2]) == '0')
        buf--;
      *buf++ = lower [d4_2];
      *buf++ = upper [d1_0];
      *buf++ = lower [d1_0];
    }
  }
  else
#if (_MIPS_SZLONG != 64)
  if (value < 1000000000) {
    unsigned long d8_4;
    unsigned long d3_0;
    unsigned long val16u, val16l;
    unsigned long vll;
    unsigned long vlu;
    unsigned long vul;
    unsigned long vuu;
    val16u = UPPER16(value);
    val16l = LOWER16(value);
    vll = val16l * LOWER16(recip10power4);
    vlu = val16l * UPPER16(recip10power4);
    vul = val16u * LOWER16(recip10power4);
    vuu = val16u * UPPER16(recip10power4);
    d8_4 =   UPPER16(UPPER16(vll) + LOWER16(vlu) + LOWER16(vul))
           + UPPER16(vlu) + UPPER16(vul) + vuu;
    d3_0 = value - d8_4 * 10000;
    if (d3_0 >= 10000) {
      d3_0 -= 10000;
      d8_4 += 1;
    }

    buf += _ltoa ((long) d8_4, buf);
    buf = _ultoa_pad_zero (d3_0, buf, 4);
  }
  else {
    int d9 = 0;
    do {
      d9++;
      value -= 1000000000;
    } while (value >= 1000000000);
    *buf++ = '0' + d9;
    buf = _ultoa_pad_zero (value, buf, 9);
  }
#else /* (_MIPS_SZLONG != 64) */
  if (value < 1000000000) {
    unsigned long d8_4;
    unsigned long d3_0;
    d8_4 = UPPER32(value * recip10power4);
    d3_0  = value - d8_4 * 10000;
    if (d3_0 >= 10000) {
      d8_4++;
      d3_0 -= 10000;
    }
    buf += _ltoa ((long) d8_4, buf);
    buf = _ultoa_pad_zero (d3_0, buf, 4);
  }
  else
  if (value < 1000000000000000000ll) {
    unsigned long d17_9;
    unsigned long d8_0;
    unsigned long val32u, val32l;
    unsigned long vll;
    unsigned long vlu;
    unsigned long vul;
    unsigned long vuu;
    val32u = UPPER32(value);
    val32l = LOWER32(value);
    vll = val32l * LOWER32(recip10power9);
    vlu = val32l * UPPER32(recip10power9);
    vul = val32u * LOWER32(recip10power9);
    vuu = val32u * UPPER32(recip10power9);
    d17_9 =   UPPER32(UPPER32(vll) + LOWER32(vlu) + LOWER32(vul))
            + UPPER32(vlu) + UPPER32(vul) + vuu;
    d8_0  = value - d17_9 * 1000000000;
    if (d8_0 >= 1000000000) {
      d8_0 -= 1000000000;
      d17_9 += 1;
    }

    buf += _ltoa ((long) d17_9, buf);
    buf = _ultoa_pad_zero (d8_0, buf, 9);
  }
  else {
    unsigned long d19_18 = 0;
    do {
      d19_18++;
      value -= 1000000000000000000ll;
    } while (value >= 1000000000000000000ll);
    if (d19_18 >= 10)
      *buf++ = upper [d19_18];
    *buf++ = lower [d19_18];
    buf = _ultoa_pad_zero (value, buf, 18);
  }
#endif /* (_MIPS_SZLONG != 64) */

  *buf = '\0';

  return (int)(buf - start);
} /* _ltoa */
