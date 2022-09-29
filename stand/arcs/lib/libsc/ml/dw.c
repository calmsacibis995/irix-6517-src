#ident "$Revision: 1.3 $"

#include <limits.h>

typedef union {
	struct __dw_s_internal {
#ifdef _MIPSEL
	unsigned lsw1;
	unsigned msw1;
#else	/* _MIPSEB */
	unsigned msw1;		/* most-significant-word */
	unsigned lsw1;		/* least-significant-word */
#endif
	} dw_s_internal;
	double dummy; /* for alignment */
} dw, udw;			/* dw == double-word == 64-bit integer */
#define msw dw_s_internal.msw1
#define lsw dw_s_internal.lsw1

#define dw_copy(d,e) 	{d = e;}			/* copy e to d */
#define dw_is_neg(d) 	((signed) d.msw < 0)		/* returns boolean */
#define dw_abs(d)	(dw_is_neg(d) ? dw_neg(d) : d)	/* returns dw */

/* relational operations; all take two dw and return boolean */
#define dw_eq(d,e)	(d.lsw == e.lsw && d.msw == e.msw) 
#define dw_neq(d,e)	(d.lsw != e.lsw || d.msw != e.msw) 
#define dw_lt(d,e)	((d.msw == e.msw) ? (d.lsw < e.lsw) : ((int)d.msw < (int)e.msw)) 
#define udw_lt(d,e)	((d.msw == e.msw) ? (d.lsw < e.lsw) : (d.msw < e.msw)) 
#define dw_le(d,e)	((d.msw == e.msw) ? (d.lsw <= e.lsw) : ((int)d.msw < (int)e.msw)) 
#define udw_le(d,e)	((d.msw == e.msw) ? (d.lsw <= e.lsw) : (d.msw < e.msw)) 
#define dw_gt(d,e)	((d.msw == e.msw) ? (d.lsw > e.lsw) : ((int)d.msw > (int)e.msw)) 
#define udw_gt(d,e)	((d.msw == e.msw) ? (d.lsw > e.lsw) : (d.msw > e.msw)) 
#define dw_ge(d,e)	((d.msw == e.msw) ? (d.lsw >= e.lsw) : ((int)d.msw > (int)e.msw)) 
#define udw_ge(d,e)	((d.msw == e.msw) ? (d.lsw >= e.lsw) : (d.msw > e.msw)) 

/* some constants */
extern dw __dw_zero;      /* 0 */
extern dw __dw_one;       /* 1 */
extern dw __dw_min;       /* -9223372036854775808 */
extern dw __dw_max;       /*  9223372036854775807 */
extern dw __udw_max;      /* 18446744073709551615 */

dw __dw_zero = {0,0};
#ifdef _MIPSEL
dw __dw_one = {1,0};
dw __dw_min = {UINT_MIN,INT_MIN};
dw __dw_max = {UINT_MAX,INT_MAX};
#else   /* _MIPSEB */
dw __dw_one = {0,1};
dw __dw_min = {INT_MIN,UINT_MAX};
dw __dw_max = {INT_MAX,UINT_MAX};
#endif
dw __udw_max = {UINT_MAX,UINT_MAX};



extern dw i_to_dw (int i);              /* int to dw */
extern dw ui_to_udw (unsigned int i);   /* unsigned int to unsigned dw */
extern int dw_to_i (dw d);              /* dw to int */
extern unsigned int udw_to_ui (dw d);   /* unsigned dw to unsigned int */
extern char * dw_to_s (dw d);           /* dw to string */
extern char * udw_to_s (dw d);          /* unsigned dw to string */
extern dw s_to_dw (char *s);            /* string to dw */
extern double dw_to_d (dw d);           /* dw to double */
extern double udw_to_d (dw d);          /* unsigned dw to double */
extern float dw_to_f (dw d);            /* dw to float */
extern float udw_to_f (dw d);           /* unsigned dw to float */
extern dw d_to_dw (double f);           /* double to dw */
extern dw d_to_udw (double f);          /* double to unsigned dw */
extern dw f_to_dw (float f);            /* float to dw */
extern dw f_to_udw (float f);           /* float to unsigned dw */


/* note:  arithmetic operations do no overflow checking */
extern dw dw_add (dw d, dw e);
extern dw dw_sub (dw d, dw e);
extern dw dw_mul (dw d, dw e);
extern dw dw_div (dw d, dw e);
extern dw dw_rem (dw d, dw e);
extern dw dw_mod (dw d, dw e);
extern dw dw_addi (dw d, unsigned int i);
extern dw dw_subi (dw d, unsigned int i);
extern dw dw_muli (dw d, int i);
extern dw dw_divi (dw d, int i);
extern dw dw_remi (dw d, int i);
extern dw dw_modi (dw d, int i);
extern dw udw_div (dw d, dw e);
extern dw udw_rem (dw d, dw e);
/* udw_mod is same as udw_rem */
extern dw udw_divi (dw d, unsigned int i);
extern dw udw_remi (dw d, unsigned int i);
extern void udw_div_rem (dw *aquo, dw *arem, dw num, dw denom);
/* operator-assign routines provided for easy interface to c. */
/* pass params in reverse order so calling convention matches. */
extern void dw_mula (dw e, dw *d);
extern void dw_diva (dw e, dw *d);
extern void dw_rema (dw e, dw *d);
extern void udw_diva (dw e, dw *d);
extern void udw_rema (dw e, dw *d);

extern dw __dw_neg (dw d);                /* return -d */
extern dw __dw_and (dw d, dw e);          /* bitwise and */
extern dw __dw_or (dw d, dw e);
extern dw __dw_xor (dw d, dw e);
extern dw __dw_not (dw d);
extern dw __dw_lshift (dw d, dw e);       /* left-shift d by e amount */
extern dw __dw_rshift (dw d, dw e);
extern dw __dw_rshifts (dw d, dw e);
extern dw __dw_lshifti (dw d, unsigned i);
extern dw __dw_rshifti (dw d, unsigned i);
extern void __dw_lshifta (dw e, dw *d);
extern void __dw_rshifta (dw e, dw *d);

/* externs to assembler routines */
extern void __dwmul(dw*, dw, dw);
extern void __dwdiv(dw*, dw *, dw, dw);
extern void __dwdivu(dw*, dw *, dw, dw);

extern
dw
__i_to_dw (int i) 
{
	dw d;
#ifdef sgi
	if(i < 0) {
		d.msw = -1;
		d.lsw = i;
	} else {
		d.msw = 0;
		d.lsw = i;
	}
	return d;
#else
	int j = i;

	if (i < 0) j = -i;
	else j = i;
	d.msw = 0; 
	d.lsw = j;
	if (i < 0) return __dw_neg(d);
	else return d;
#endif
}

extern
dw
__ui_to_udw (unsigned i) 
{
	dw d;
	d.msw = 0; 
	d.lsw = i;
	return d;
}

extern
int 
__dw_to_i (dw d)
{
#ifdef sgi
	/* no error possible: we truncate */
	int isneg = 0;
	if (dw_is_neg(d)) {
		d = __dw_neg(d);
		isneg = 1;
	}
	if (isneg) return -d.lsw;
	else return d.lsw;
#else
	int isneg = 0;
	if (dw_is_neg(d)) {
		d = __dw_neg(d);
		isneg = 1;
	}
	if (d.msw == 0) {
		if (isneg) return -d.lsw;
		else return d.lsw;
	} else {
		fprintf(stderr, "dw_to_i error: dw too big\n");
		return INT_MAX;
	}
#endif
}

extern
unsigned int 
__udw_to_ui (dw d)
{
#ifdef sgi
	/*
		This is not  an error: C always allows such casts
		silently.
	*/
	return d.lsw;
#else
	if (d.msw == 0) {
		return d.lsw;
	} else {
		fprintf(stderr, "udw_to_ui error: dw too big\n");
		return UINT_MAX;
	}
#endif
}


#ifdef _MIPSEL
static dw __dw_2_53 = {0,2097152};		/* 2^53 */
static dw __dw_n2_53 = {0,4292870144};		/* -2^53 */
static dw __dw_2_16 = {65536,0};		/* 2^16 */
static dw __dw_n2_16 = {4294901760,UINT_MAX};	/* -2^16 */
#else /* _MIPSEB */
static dw __dw_2_53 = {2097152,0};		/* 2^53 */
static dw __dw_n2_53 = {4292870144,0};		/* -2^53 */
static dw __dw_2_16 = {0,65536};		/* 2^16 */
static dw __dw_n2_16 = {UINT_MAX,4294901760};	/* -2^16 */
#endif

#define __dw_abs(d)	(dw_is_neg(d) ? __dw_neg(d) : d)  /* returns dw */

extern
dw
__dw_neg (dw d)
{
	dw r;
	r.msw = ~d.msw + (d.lsw == 0);
	r.lsw = -d.lsw;
	return r;
}
extern
dw
__dw_compl (dw d)
{
	dw r;
	r.msw = ~d.msw;
	r.lsw = ~d.lsw;
	return r;
}

extern
dw
__dw_add (dw d, dw e)
{
	dw r;
	r.lsw = d.lsw + e.lsw;
	r.msw = d.msw + e.msw + (r.lsw < e.lsw);
	return r;
}

extern
dw
__dw_addi (dw d, unsigned int i)
{
	dw r;
	r.lsw = d.lsw + i;
	r.msw = d.msw + (r.lsw < d.lsw);
	return r;
}

extern
dw
__dw_sub (dw d, dw e)
{
	dw r;
	r.msw = d.msw - e.msw - (d.lsw < e.lsw);
	r.lsw = d.lsw - e.lsw;
	return r;
}

extern
dw
__dw_subi (dw d, unsigned int i)
{
	dw r;
	r.msw = d.msw - (d.lsw < i);
	r.lsw = d.lsw - i;
	return r;
}


/* Given an unsigned64 number, return the number of left-shifts required
  to normalize it (causing high-order digit to be 1) */
static 
unsigned
__firstbit(udw number)
{
  unsigned bias = 0;

  if (number.msw == 0)
    {
    if (number.lsw != 0)
      {
      bias = 32;
      while ((number.lsw & 0x80000000) == 0)
	{
	bias++;
	number.lsw <<= 1;
	}
      }
    }
  else
    {
    while ((number.msw & 0x80000000) == 0)
      {
      bias++;
      number.msw <<= 1;
      }
    }

  return bias;
}

extern
dw
__dw_and (dw d, dw e)
{
	dw r;
	r.msw = d.msw & e.msw;
	r.lsw = d.lsw & e.lsw;
	return r;
}

extern
dw
__dw_or (dw d, dw e)
{
	dw r;
	r.msw = d.msw | e.msw;
	r.lsw = d.lsw | e.lsw;
	return r;
}

extern
dw
__dw_xor (dw d, dw e)
{
	dw r;
	r.msw = d.msw ^ e.msw;
	r.lsw = d.lsw ^ e.lsw;
	return r;
}

extern
dw
__dw_not (dw d)
{
	dw r;
	r.msw = ~ d.msw;
	r.lsw = ~ d.lsw;
	return r;
}

/* shift routines */

#define bitsperword WORD_BIT

extern
dw
__dw_lshift (dw d, dw e)
{
	dw r;
	if ((signed)e.msw < 0) {
		return __dw_rshift (d, __dw_neg(e));
	} else if (e.msw > 0 || e.lsw >= bitsperword*2) {
		/* e > 64, so everything shifted out */
		return __dw_zero;
	} else if (e.lsw >= bitsperword) {
		/* everything shifted to msw */
		r.msw = d.lsw << (e.lsw - bitsperword);
		r.lsw = 0;
	} else if (e.lsw > 0) {
		/* lsw shifted, then msw combines lsw and msw */
		r.msw = (d.msw << e.lsw) | (d.lsw >> (bitsperword - e.lsw));
		r.lsw = d.lsw << e.lsw;
	} else {
		/* e == 0 */
		return d;
	}
	return r;
}

extern
dw
__dw_rshift (dw d, dw e)
{
	dw r;
	if ((signed)e.msw < 0) {
		return __dw_lshift (d, __dw_neg(e));
	} else if (e.msw > 0 || e.lsw >= bitsperword*2) {
		/* e > 64, so everything shifted out */
		return __dw_zero;
	} else if (e.lsw >= bitsperword) {
		/* everything shifted to lsw */
		r.lsw = d.msw >> (e.lsw - bitsperword);
		r.msw = 0;
	} else if (e.lsw > 0) {
		/* msw shifted, then lsw combines lsw and msw */
		r.lsw = (d.lsw >> e.lsw) | (d.msw << (bitsperword - e.lsw));
		r.msw = d.msw >> e.lsw;
	} else {
		/* e == 0 */
		return d;
	}
	return r;
}
#ifdef sgi
/*lsw & msw are unsinged, so need a signed rshift */
extern dw
__dw_rshiftsi(dw d, int i)
{
	dw e;
	e.msw = 0;
	e.lsw = i;
	return __dw_rshifts(d,e);
}
extern
dw
__dw_rshifts(dw d, dw e)
{
	dw r;
	if ((signed)e.msw < 0) {
		return __dw_lshift (d, __dw_neg(e));
	} else if (e.msw > 0 || e.lsw >= bitsperword*2) {
		/* e > 64, so everything shifted out */
		return __dw_zero;
	} else if (e.lsw >= bitsperword) {
		/* everything shifted to lsw */
		r.lsw = (signed long)d.msw >> (e.lsw - bitsperword);
		r.msw = (((signed long)d.msw) < 0) ? -1 : 0;
	} else if (e.lsw > 0) {
		/* msw shifted, then lsw combines lsw and msw */
		r.lsw = (d.lsw >> e.lsw) | (d.msw << (bitsperword - e.lsw));
		r.msw = (signed long)d.msw >> e.lsw;
	} else {
		/* e == 0 */
		return d;
	}
	return r;
}
#endif /* sgi */
extern
dw
__dw_lshifti (dw d, unsigned i)
{
	dw r;
	if (i >= bitsperword*2) {
		/* will left-shift all bits out */
		return __dw_zero;
	} else if (i >= bitsperword) {
		/* everything shifted to msw */
		r.msw = d.lsw << (i - bitsperword);
		r.lsw = 0;
	} else if (i > 0) {
		/* lsw shifted, then msw combines lsw and msw */
		r.msw = (d.msw << i) | (d.lsw >> (bitsperword - i));
		r.lsw = d.lsw << i;
	} else {
		/* i == 0 */
		return d;
	}
	return r;
}

extern
dw
__dw_rshifti (dw d, unsigned i)
{
	dw r;
	if (i >= bitsperword*2) {
		/* will right-shift all bits out */
		return __dw_zero;
	} else if (i >= bitsperword) {
		/* everything shifted to lsw */
		r.lsw = d.msw >> (i - bitsperword);
		r.msw = 0;
	} else if (i > 0) {
		/* msw shifted, then lsw combines lsw and msw */
		r.lsw = (d.lsw >> i) | (d.msw << (bitsperword - i));
		r.msw = d.msw >> i;
	} else {
		/* i == 0 */
		return d;
	}
	return r;
}

extern
void
__dw_lshifta (dw e, dw *d)
{
	*d = __dw_lshift(*d, e);
}

extern
void
__dw_rshifta (dw e, dw *d)
{
	*d = __dw_rshift(*d, e);
}
#ifdef sgi
extern 
void
__dw_rshiftsia(int i, dw *d)
{

	*d = __dw_rshiftsi(*d,i);
}
extern void 
__dw_rshiftia(int i, dw *d)
{
	*d = __dw_rshifti(*d,i);
}
extern void 
__dw_lshiftia(int i, dw *d)
{
	*d = __dw_lshifti(*d,i);
}

#endif
void
__dw_adda( dw e, dw *d)
{
	*d = __dw_add(*d,e);
}
void
__dw_suba( dw e, dw *d)
{
	*d = __dw_sub(*d,e);
}
void
__dw_anda( dw e, dw *d)
{
	*d = __dw_and(*d,e);
}
#ifdef NEVER
void
__dw_moda( dw e, dw *d)
{
	*d = __dw_mod(*d,e);
}
#endif
void
__dw_ora( dw e, dw *d)
{
	*d = __dw_or(*d,e);
}
void
__dw_xora( dw e, dw *d)
{
	*d = __dw_xor(*d,e);
}
int
__dw_not_zero(dw d)
{
	if(d.msw == 0 && d.lsw == 0)
		return 0;
	return 1;
}

int
__dw_noti(dw d)
{
	if(d.msw == 0 && d.lsw == 0)
		return 1;
	return 0;
}

extern
dw
__dw_mul (dw d, dw e)
{
	dw r;
	__dwmul(&r,d,e);
	return r;
}

extern
void
__dw_mula (dw e, dw *d)
{
	__dwmul(d,*d,e);
}

/*
 * General (i.e., difficult) case of 64-bit unsigned division.
 * Use this to handle cases where values are greater than can be 
 * represented with 53-bits of double floats.  
 * Modified from pl1 library.
 */
static
void
__dwdivu64 (dw *aquo, dw *arem, dw num, dw denom)
{
  dw quo;
  int n_bias, d_bias;
  
  /* Shift denom left so its first bit lines up with that of numerator */
  n_bias = __firstbit(num);
  d_bias = __firstbit(denom);
  if ((d_bias -= n_bias) > 0)
	denom = __dw_lshifti(denom, d_bias);

  /*
    "Long division" just like you did in elementary school, except that
    by virtue of doing it in binary, we can guess the next digit simply
    by comparing numerator and divisor.

    quo = 0;
    repeat (1 + amount_we_shifted_denom_left)
      {
      quo <<= 1;
      if (!(num < denom))
	{
	num -= denom;
	quo |= 1;
	}
      denom >>= 1;
      }
  */
  quo.msw = quo.lsw = 0;
  while (d_bias-- >= 0)
    {
    int cmp = (num.msw > denom.msw) - (num.msw < denom.msw);

    quo = __dw_lshifti(quo, 1);

    if (!(cmp < 0 || (cmp == 0 && num.lsw < denom.lsw)))
      {
	num = __dw_sub(num, denom);
      quo.lsw |= 1;
      }
    denom = __dw_rshifti(denom, 1);
    }

  *aquo = quo;
  *arem = num;
}

extern
void
__udw_div_rem (dw *aquo, dw *arem, dw num, dw denom)
{
	if (udw_lt(denom,__dw_2_16) || (udw_le(num,__dw_2_53) && udw_le(denom,__dw_2_53))) {
		__dwdivu(aquo,arem,num,denom);
	} else {
		__dwdivu64(aquo,arem,num,denom);
	}
}

extern
dw
__dw_div (dw d, dw e)
{
	dw q, r;
#ifdef __sgi
	if ( ((dw_is_neg(e) ? dw_gt(e,__dw_n2_16) : dw_lt(e,__dw_2_16)) 
		&& (dw_is_neg(d) ? dw_ge(d,__dw_n2_53) : dw_le(d,__dw_2_53))
		)
#else
	if ( (dw_is_neg(e) ? dw_gt(e,__dw_n2_16) : dw_lt(e,__dw_2_16)) 
#endif
	|| ( (dw_is_neg(d) ? dw_ge(d,__dw_n2_53) : dw_le(d,__dw_2_53))
	   && (dw_is_neg(e) ? dw_ge(e,__dw_n2_53) : dw_le(e,__dw_2_53)) )) {
#ifdef sgi
		int neg_quo;
		neg_quo = (dw_is_neg(d) != dw_is_neg(e));
		__dwdiv(&q,&r,__dw_abs(d),__dw_abs(e));
		if (neg_quo) q = __dw_neg(q);
#else
		__dwdiv(&q,&r,d,e);
#endif
	} else {
		int neg_quo;
		neg_quo = (dw_is_neg(d) != dw_is_neg(e));
		__dwdivu64(&q,&r,__dw_abs(d),__dw_abs(e));
		if (neg_quo) q = __dw_neg(q);
	}
	return q;
}

extern
dw
__udw_div (dw d, dw e)
{
	dw q, r;
	if (udw_lt(e,__dw_2_16) || (udw_le(d,__dw_2_53) && udw_le(e,__dw_2_53))) {
		__dwdivu(&q,&r,d,e);
	} else {
		__dwdivu64(&q,&r,d,e);
	}
	return q;
}

extern
void
__dw_diva (dw e, dw *d)
{
	*d = __dw_div(*d, e);
}

extern
void
__udw_diva (dw e, dw *d)
{
	*d = __udw_div(*d, e);
}

extern
dw
__dw_divi (dw d, int i)
{
	return __dw_div(d, __i_to_dw(i));
}

extern
dw
__udw_divi (dw d, unsigned int i)
{
	return __udw_div(d, __ui_to_udw(i));
}

extern
dw
__udw_rem (dw d, dw e)
{
	dw q, r;
	if (udw_lt(e,__dw_2_16) || (udw_le(d,__dw_2_53) && udw_le(e,__dw_2_53))) {
		__dwdivu(&q,&r,d,e);
	} else {
		__dwdivu64(&q,&r,d,e);
	}
	return r;
}

