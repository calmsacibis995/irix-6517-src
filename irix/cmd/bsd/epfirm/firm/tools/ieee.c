/* IEEE.C -- IEEE FLOATING-POINT SUPPORT FUNCTIONS              */

/* header file(s)               */

#include "asm29.h"

/* ------------------------------------------------------------ */

/* miscellaneous definitions    */

static long ref_x = 0x12345678L;

#define ref         ((char *) &ref_x)

/* ------------------------------------------------------------ */

/* "ieee_c()" is a  support function for  "ieee()",  "ieeed()", */
/* and "ieeex()".                                               */

/* ------------------------------------------------------------ */

static void
ieee_c(double value,		/* value to convert             */
       char *result,		/* pointer to result buffer     */
       long max_exp,		/* maximum exponent field value */
       int f1,			/* internal parameter           */
       int r_size)		/* size of result in bytes      */
{
    int b_size;                 /* size of result in bits       */
    long e_bias;                /* exponent bias                */
    ushort e_mask;              /* masks part of exponent       */
    char f_buf[16];             /* intermediate-result buffer   */
    int i,j,k,n;                /* scratch                      */
    double m;                   /* mantissa                     */

    int    exp_i;               /* (integer)        exponent    */
    long   exp_l;               /* (long   )        exponent    */
    ushort exp_u;               /* (unsigned short) exponent    */

    double frexp();

/* ------------------------------------------------------------ */

    for (i = 0; i < r_size; i++)
    {
        result[i] = f_buf[i] = 0;
    }

/* ------------------------------------------------------------ */

    if (value == 0.0) return;
    b_size = r_size * BPB;
    e_bias = max_exp >> 1;

/* ------------------------------------------------------------ */

    if (value < 0.0)
    {
        f_buf[0] = 0x80;
        value = (-value);
    }

/* ------------------------------------------------------------ */

    m = frexp (value,&exp_i);   /* get mantissa and exponent    */
    exp_l = ((long) exp_i) + e_bias;

    while ((m != 0.0) && (m < 1.0))
    {
        m += m;
        exp_l--;
    }

/* ------------------------------------------------------------ */

    if (exp_l < 1)
    {                           /* value is too small           */
        error (e_smflt);
        exp_l = 0;
        m = 0.0;
    }

    if (exp_l > max_exp-1)
    {                           /* value is too large           */
        error (e_lgflt);
        exp_l = max_exp;
        m = 0.0;
    }

/* ------------------------------------------------------------ */

    exp_u = (ushort) exp_l;
    if (m >= 1.0) m -= 1.0;
    i = exp_u >> f1;
    f_buf[0] |= i;

    for (e_mask = i = 0; i < f1; i++)
    {
        e_mask = (e_mask << 1) + 1;
    }

    i = exp_u & e_mask;
    j = BPB - f1;
    if (j > 0) i <<= j;
    f_buf[1] = i;

/* ------------------------------------------------------------ */

    if (r_size < 16)
    {
        j = f1 + 7;
    }
    else
    {
        f_buf[2] = 0x80;
        j = f1 + 8;
    }

/* ------------------------------------------------------------ */

    for (i = j; i < b_size; i++)
    {
        if (m >= 1.0)
        {
            f_buf[i/BPB] |= (1 << ((BPB-1) - (i % BPB)));
            m -= 1.0;
        }
        m += m;
    }

/* ------------------------------------------------------------ */

    if (ref[0] == 0x12)
    {                           /* MS byte first                */
        for (i = 0; i < r_size; i++)
        {
            result[i] = f_buf[i];
        }
    }
    else                        /* MS word first, but           */
    {                           /* LS byte first in each word   */
        k = r_size/4;

        for (j = 0; j < k; j++)
        {
            n = j * 4;

            for (i = 0; i < 4; i++)
            {
                result[n+i] = f_buf[n+3-i];
            }
        }
    }
}



/* "ieee(value,result)"  converts the  double  precision  value */
/* "value"  into  single-precision  IEEE format and  stores the */
/* result into the array "result[]".                            */
void
ieee(double value,			/* value to convert             */
     char *result)			/* pointer to result buffer     */
{
	ieee_c(value,result,255L,1,4);
}


/* "ieeed(value,result)"  converts the  double precision  value */
/* "value"  into  double-precision  IEEE format and  stores the */
/* result into the array "result[]".                            */
void
ieeed(double value,			/* value to convert             */
      char *result)			/* pointer to result buffer     */
{
	ieee_c (value,result,2047L,4,8);
}


/* "ieeex(value,result)"  converts the  double precision  value */
/* "value" into  extended-precision  IEEE format and stores the */
/* result into the array "result[]".                            */
void
ieeex(double value,			/* value to convert             */
      char *result)			/* pointer to result buffer     */
{
	ieee_c (value,result,32767L,8,16);
}
