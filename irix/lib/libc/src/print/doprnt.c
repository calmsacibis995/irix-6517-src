/*	Copyright (c) 1990, 1991, 1992 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:print/doprnt.c	3.34.1.3"

/*LINTLIBRARY*/
/*
 *	_doprnt: common code for printf, fprintf, sprintf
 */
#include "synonyms.h"
#include "shlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <values.h>
#include <nan.h>
#include <memory.h>
#include <string.h>
#include <stddef.h>
#include "print.h"	/* parameters & macros for doprnt */
#include "../stdio/stdiom.h"
#include "_locale.h"
#include <sgidefs.h>

typedef  union {
	struct {
		unsigned  sign	:1;
		unsigned  exp	:11;
		unsigned  hi	:20;
		unsigned  lo	:32;
	} fparts;
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
	struct {
		unsigned  sign	:1;
		unsigned long long	rest	:63;
	} dparts;
#else
        struct {
                unsigned  sign  :1;
                unsigned  hi    :31;
                unsigned  lo    :32;
        } dparts;
#endif
	struct {
		unsigned hi;
		unsigned lo;
	} fwords;
	double	d;
} _dval;

#if (_MIPS_SIM != _MIPS_SIM_ABI32)
#define	IsZero(x)	( x.dparts.rest == 0 )
#else
#define	IsZero(x)	( (x.dparts.hi == 0) && (x.dparts.lo == 0) )
#endif

#define PUT(p, n)     { register unsigned char *newbufptr; \
	              if ((newbufptr = bufptr + n) > bufferend) { \
		              if (!_dowrite(p, n, iop, &bufptr)) return EOF; \
                      } else { \
		              (void) memcpy((char *) bufptr, p, (size_t) n); \
			      bufptr = newbufptr; \
			      if (iop->_flag & _IOREAD) { \
				      iop->_cnt -= (n); \
			      } \
                      } \
		    } 
#define PAD(s,n)    { register int nn; \
		    for (nn = n; nn > 20; nn -= 20) \
			       if (!_dowrite(s, 20, iop, &bufptr)) return EOF; \
                    PUT(s, nn); \
                   }

#define SNLEN     5    	/* Length of string used when printing a NaN */
#define	NDIG 82	/* from ecvt.c */

/* bit positions for flags used in doprnt */

#define LENGTH 	  1	/* l */
#define FPLUS     2	/* + */
#define FMINUS	  4	/* - */
#define FBLANK	  8	/* blank */
#define FSHARP	 16	/* # */
#define PADZERO  32	/* padding zeroes requested via '0' */
#define DOTSEEN  64	/* dot appeared in format specification */
#define SUFFIX	128	/* a suffix is to appear in the output */
#define RZERO	256	/* there will be trailing zeros in output */
#define LZERO	512	/* there will be leading zeroes in output */
#define SHORT  1024	/* h */
#define LLONG  2048	/* ll */
#define LDOUBLE 4096	/* L */
#define FQUOTE 0x10000  /* Single quote Inc# 267923 */

/*
 *	C-Library routines for floating conversion
 */

/*
 *	Positional Parameter information
 */
#define MAXARGS	30	/* max. number of args for fast positional paramters */

static const long long table10_ll[18] = {
			    10LL,
			    100LL, 
			    1000LL,
			    10000LL,
			    100000LL,
			    1000000LL,
			    10000000LL,
			    100000000LL,
			    1000000000LL,
			    10000000000LL,
			    100000000000LL,
			    1000000000000LL,
			    10000000000000LL,
			    100000000000000LL,
			    1000000000000000LL,
			    10000000000000000LL,
			    100000000000000000LL,
			    1000000000000000000LL
};

static const long table10_l[9] = {
			    10,
			    100, 
			    1000,
			    10000,
			    100000,
			    1000000,
			    10000000,
			    100000000,
			    1000000000
};

static long decimal_digits_l(long);
static long decimal_digits_ll(long long);

extern void __do_group(char *, double, int, int, int, int);

/* stva_list is used to subvert C's restriction that a variable with an
 * array type can not appear on the left hand side of an assignment operator.
 * By putting the array inside a structure, the functionality of assigning to
 * the whole array through a simple assignment is achieved..
 */
typedef struct stva_list {
	va_list	ap;
} stva_list;

void _mkarglst(char *, stva_list, stva_list []);
void _getarg(char *, stva_list *, int);

static int _dowrite(const char *, ssize_t, FILE *, unsigned char **);
static int _lowdigit(long long *);

static int
_lowdigit(long long *valptr)
{	/* This function computes the decimal low-order digit of the number */
	/* pointed to by valptr, and returns this digit after dividing   */
	/* *valptr by ten.  This function is called ONLY to compute the */
	/* low-order digit of a long whose high-order bit is set. */

	int lowbit = (int) *valptr & 1;
	long long value = (*valptr >> 1) & ~HIBITLL;

	*valptr = value / 5;
	return((int) (value % 5 * 2 + lowbit + '0'));
}

static int
_lowdigit_l(long *valptr)
{	/* This function computes the decimal low-order digit of the number */
	/* pointed to by valptr, and returns this digit after dividing   */
	/* *valptr by ten.  This function is called ONLY to compute the */
	/* low-order digit of a long whose high-order bit is set. */

	int lowbit = (int) *valptr & 1;
	long value = (*valptr >> 1) & ~HIBITL;

	*valptr = value / 5;
	return((int) (value % 5 * 2 + lowbit + '0'));
}

/* The function _dowrite carries out buffer pointer bookkeeping surrounding */
/* a call to fwrite.  It is called only when the end of the file output */
/* buffer is approached or in other unusual situations. */
static int
_dowrite(register const char *p, register ssize_t n, register FILE *iop, 
	register unsigned char **ptrptr)
{
	if (!(iop->_flag & _IOREAD)) {
		iop->_cnt -= (*ptrptr - iop->_ptr);
		iop->_ptr = *ptrptr;
		_bufsync(iop, _bufend(iop));
		if (fwrite(p, 1, (size_t) n, iop) != (size_t) n) {
			return 0;
		}
		*ptrptr = iop->_ptr;
	} else {
		n = n > iop->_cnt ? iop->_cnt : n;
		*ptrptr = (unsigned char *) memcpy((char *) *ptrptr, p, (size_t) n) + n;
		iop->_cnt -= n;
		/* fall through and return 1 here, either way */
	}

	return 1;
}
	static const char _blanks[] = "                    ";
	static const char _zeroes[] = "00000000000000000000";
	static const char uc_digs[] = "0123456789ABCDEF";
	static const char lc_digs[] = "0123456789abcdef";
	static const char  lc_nan[] = "nan0x";
	static const char  uc_nan[] = "NAN0X";
	static const char  lc_inf[] = "inf";
	static const char  uc_inf[] = "INF";
	static const wchar_t wnull_buf[] = {0};

int
_doprnt(register const char *format, va_list in_args, register FILE *iop)
{
                
	/* bufptr is used inside of doprnt instead of iop->_ptr; */
	/* bufferend is a copy of _bufend(iop), if it exists.  For */
	/* dummy file descriptors (iop->_flag & _IOREAD), bufferend */
	/* may be meaningless, unless the snprintf() variants are in use. */
	/* Dummy file descriptors are used so that */
        /* sprintf and vsprintf may share the _doprnt routine with the */
	/* rest of the printf family. */

	unsigned char *bufptr, *bufferend;

	int just32 = 0;

	/* This variable counts output characters. */
	int	count = 0;

	/* Starting and ending points for value to be printed */
	register char	*bp;
	char *p;

	/* Field width and precision */
	int	width, prec;

	/* Format code */
	register int	fcode;

	/* Number of padding zeroes required on the left and right */
	int	lzero, rzero;

	/* Flags - bit positions defined by LENGTH, FPLUS, FMINUS, FBLANK, */
	/* and FSHARP are set if corresponding character is in format */
	/* Bit position defined by PADZERO means extra space in the field */
	/* should be padded with leading zeroes rather than with blanks */
	register int	flagword;

#pragma set woff 1209
	/* Values are developed in this buffer */
	char	buf[max(MAXDIGS, 1+max(MAXFCVT+MAXEXP, 2*MAXECVT))];
#pragma reset woff 1209

	/* Pointer to sign, "0x", "0X", or empty */
	char	*prefix;

	/* Exponent or empty */
	char	*suffix;

	/* Buffer to create exponent */
	char	expbuf[MAXESIZ + 1];

	/* Length of prefix and of suffix */
	int	prefixlength, suffixlength;

	/* Combined length of leading zeroes, trailing zeroes, and suffix */
	int 	otherlength;

	/* The value being converted, if integer */
	long long 	val;
	long		val_l;

	/* The value being converted, if real */
	/* Also, the high part of a long double */
	double	dval;
	_dval	dvalue; /* used to look at bit fields of dval */
	

#if (_MIPS_SIM != _MIPS_SIM_ABI32)
	/* The value being converted, if long double */
	long double	ldval;
#endif

	/* Output values from fcvt and ecvt */
	int	decpt, sign;

	/* Pointer to a translate table for digits of whatever radix */
	const char	*tab;

	/* Work variables */
	int	k, lradix, mradix;

	/* Variables used to flag an infinities and nans, resp. */
	/* Nan_flg is used with two purposes: to flag a NaN and */
	/* as the length of the string ``NAN0X'' (``nan0x'') */
	 int	 inf_nan = 0, NaN_flg = 0 ;

	/* Pointer to string "NAN0X" or "nan0x" */
	 const char	 *SNAN ;

        /* Flag for negative infinity or NaN */
	 int neg_in = 0;

	/* variables for positional parameters */
	char	*sformat = (char *)format;  /* save the beginning of the format */
	int	fpos = 1;		/* 1 if first positional parameter */
	stva_list	args,	/* used to step through the argument list */
			sargs;	/* used to save the start of the argument list */
	stva_list	bargs;	/* used to restore args if positional width
				 * or precision */
	stva_list	arglst[MAXARGS];/* array giving the appropriate values
					 * for va_arg() to retrieve the
					 * corresponding argument:	
					 * arglst[0] is the first argument
					 * arglst[1] is the second argument, etc.
					 */
	int	starflg = 0;		/* set to 1 if * format specifier seen */
	char cvtbuf[NDIG+2];		/* for [ef]cvt return values */

	/* maximum significant figures in a floating point number */
	int	maxfsig;

	/* support for b/B formats, see below */
	int	divisor = 0;
	int	tag;
#define		tags	" kmgtpezy"
#define		TAGS	" KMGTPEZY"

	/* Initialize args and sargs to the start of the argument list.
	 * Note that ANSI guarantees that the address of the first member of
	 * a structure will be the same as the address of the structure. */
	args = sargs = *(struct stva_list *)&in_args;

	/* if first I/O to the stream get a buffer */
	/* Note that iop->_base should not equal 0 for sprintf and vsprintf */
	if (iop->_base == 0 && _findbuf(iop) == 0)
		return(EOF);

	/* initialize buffer pointer and buffer end pointer */
	bufptr = iop->_ptr;
	if (iop->_flag & _IOREAD) {
		if (iop->_cnt == MAXINT) {
			bufferend = (unsigned char *)((long) bufptr | 
				(-1L & ~HIBITL));
		} else {
			/* assumes caller has left room for NULL */
			bufferend = (unsigned char *)(bufptr + iop->_cnt);
		}
	} else {
		bufferend = _bufend(iop);
	}

	/*
	 *	The main loop -- this loop goes through one iteration
	 *	for each string of ordinary characters or format specification.
	 */
	for ( ; ; ) {
		register ssize_t n;

		if ((fcode = *format) != '\0' && fcode != '%') {
			bp = (char *)format;
			do {
				format++;
			} while ((fcode = *format) != '\0' && fcode != '%');
		
			count += (n = format - bp); /* n = no. of non-% chars */
			PUT(bp, n);
		}
		if (fcode == '\0') {  /* end of format; return */
			register ptrdiff_t nn = bufptr - iop->_ptr;
			iop->_cnt -= nn;
			iop->_ptr = bufptr;
			if (iop->_flag & _IOREAD) {
				return (int)nn;
			}
			if (bufptr + iop->_cnt > bufferend) {
				/*
				 * in case of interrupt during
				 * last several lines
				 */
				_bufsync(iop, bufferend);
			}
			if (iop->_flag & (_IONBF | _IOLBF) &&
				    (iop->_flag & _IONBF ||
				     memchr((char *)(bufptr-nn), '\n', (size_t) nn) != NULL))
				(void) _xflsbuf(iop);
			return(ferror(iop) ? EOF : count);
		}

		/*
		 *	% has been found.
		 *	The following switch is used to parse the format
		 *	specification and to perform the operation specified
		 *	by the format letter.  The program repeatedly goes
		 *	back to this switch until the format letter is
		 *	encountered.
		 */
		width = prefixlength = otherlength = flagword = suffixlength = 0;
		format++;

	charswitch:

		switch (fcode = *format++) {

		case '\'':
			flagword |= FQUOTE;
			goto charswitch;
		case '+':
			flagword |= FPLUS;
			goto charswitch;
		case '-':
			flagword |= FMINUS;
			flagword &= ~PADZERO; /* ignore 0 flag */
			goto charswitch;
		case ' ':
			flagword |= FBLANK;
			goto charswitch;
		case '#':
			flagword |= FSHARP;
			goto charswitch;

		/* Scan the field width and precision */
		case '.':
			flagword |= DOTSEEN;
			prec = 0;
			goto charswitch;

		case '*':
			if (isdigit(*format)) {
				starflg = 1;
				bargs = args;
				goto charswitch;
			}
			if (!(flagword & DOTSEEN)) {
				width = va_arg(args.ap, int);
				if (width < 0) {
					width = -width;
					/* ANSI 4.9.6.1, " a negative field 
				argument is taken as a - flag followed by a 
				positive field width " (where did AT&T come up 
				with the idea of toggling the - flag ?) 
					flagword ^= FMINUS;
					*/
					flagword |= FMINUS;
				}
			} else {
				prec = va_arg(args.ap, int);
				if (prec < 0) {
					prec = 0;
					flagword &= ~DOTSEEN;
				}
			}
			goto charswitch;

		case '$':
			{
			int		position;
			stva_list	targs;
			if (fpos) {
				_mkarglst(sformat, sargs, arglst);
				fpos = 0;
			}
			if (flagword & DOTSEEN) {
				position = prec;
				prec = 0;
			} else {
				position = width;
				width = 0;
			}
			if (position <= 0) {
				/* illegal position */
				format--;
				continue;
			}
			if (position <= MAXARGS) {
				targs = arglst[position - 1];
			} else {
				targs = arglst[MAXARGS - 1];
				_getarg(sformat, &targs, position);
			}
			if (!starflg)
				args = targs;
			else {
				starflg = 0;
				args = bargs;
				if (flagword & DOTSEEN) {
					prec = va_arg(targs.ap, int);
					if (prec < 0) {
						prec = 0;
						flagword &= ~DOTSEEN;
					}
				} else {
					width = va_arg(targs.ap, int);
					if (width < 0) {
						width = -width;
						/* same as above
						flagword ^= FMINUS;
						*/
						flagword |= FMINUS;
					}
				}
			}
			goto charswitch;
			}

		case '0':	/* obsolescent spec:  leading zero in width */
				/* means pad with leading zeros */
			if (!(flagword & (DOTSEEN | FMINUS)))
				flagword |= PADZERO;
			/* FALLTHROUGH */
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		      { register int num = fcode - '0';
			while (isdigit(fcode = *format)) {
				num = num * 10 + fcode - '0';
				format++;
			}
			if (flagword & DOTSEEN)
				prec = num;
			else
				width = num;
			goto charswitch;
		      }

		/* Scan the length modifier */
		case 'l':
			if (flagword & LENGTH) {
				flagword &= ~LENGTH;
				flagword |= LLONG;
			} else
				flagword |= LENGTH;
			goto charswitch; 
		case 'h':
			flagword |= SHORT;
			goto charswitch; 
		case 'L':
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
			flagword |= LDOUBLE;
#endif
			goto charswitch; 

		/*
		 *	The character addressed by format must be
		 *	the format letter -- there is nothing
		 *	left for it to be.
		 *
		 *	The status of the +, -, #, and blank
		 *	flags are reflected in the variable
		 *	"flagword".  "width" and "prec" contain
		 *	numbers corresponding to the digit
		 *	strings before and after the decimal
		 *	point, respectively. If there was no
		 *	decimal point, then flagword & DOTSEEN
		 *	is false and the value of prec is meaningless.
		 *
		 *	The following switch cases set things up
		 *	for printing.  What ultimately gets
		 *	printed will be padding blanks, a
		 *	prefix, left padding zeroes, a value,
		 *	right padding zeroes, a suffix, and
		 *	more padding blanks.  Padding blanks
		 *	will not appear simultaneously on both
		 *	the left and the right.  Each case in
		 *	this switch will compute the value, and
		 *	leave in several variables the informa-
		 *	tion necessary to construct what is to
		 *	be printed.  
		 *
		 *	The prefix is a sign, a blank, "0x",
		 *	"0X", or null, and is addressed by
		 *	"prefix".
		 *
		 *	The suffix is either null or an
		 *	exponent, and is addressed by "suffix".
		 *	If there is a suffix, the flagword bit
		 *	SUFFIX will be set.
		 *
		 *	The value to be printed starts at "bp"
		 *	and continues up to and not including
		 *	"p".
		 *
		 *	"lzero" and "rzero" will contain the
		 *	number of padding zeroes required on
		 *	the left and right, respectively.
		 *	The flagword bits LZERO and RZERO tell
		 *	whether padding zeros are required.
		 *
		 *	The number of padding blanks, and
		 *	whether they go on the left or the
		 *	right, will be computed on exit from
		 *	the switch.
		 */



		
		/*
		 *	decimal fixed point representations
		 *
		 *	HIBITL is 100...000
		 *	binary, and is equal to	the maximum
		 *	negative number.
		 *	We assume a 2's complement machine
		 */

		case 'i':
		case 'd':
			{
			just32 = 0;
			/* Fetch the argument to be printed */
			if (flagword & LLONG)
				val = va_arg(args.ap, long long);
			else if (flagword & LENGTH) {

/* 
 *	Looking for %ld ... in the 32bit world its 32bits while
 *	in the 64bit world its 64bits so we must account for this here.
 */
#if (_MIPS_SZLONG == 64)
                                val = va_arg(args.ap, long);
#else
				val_l = va_arg(args.ap, long);
				just32 = 1;
#endif
			} else {
				val_l = va_arg(args.ap, int);
				just32 = 1;			    
			}
			if (flagword & SHORT)
				val = (short)val;

			if ((flagword & PADZERO) && (flagword & DOTSEEN))
				flagword &= ~PADZERO; /* ignore 0 flag */

			/* Set buffer pointer to last digit */
	                p = bp = buf + MAXDIGS;

			if (just32) {
			/* If signed conversion, make sign */
			if (val_l < 0) {
				prefix = "-";
				prefixlength = 1;
				/*
				 * Negate, checking in
				 * advance for possible
				 * overflow.
				 */
				if (val_l != HIBITL)
					val_l = -val_l;
				else     /* number is -HIBITL; convert last */
					 /* digit now and get positive number */
					*--bp = (char) _lowdigit_l(&val_l);
			} else if (flagword & FPLUS) {
				prefix = "+";
				prefixlength = 1;
			} else if (flagword & FBLANK) {
				prefix = " ";
				prefixlength = 1;
			}

			{ register long  qval = val_l, tab;
				if (qval <= 9) {
					if (qval != 0 || !(flagword & DOTSEEN))
						*--bp = (char) (qval + '0');
				} else {
                                /* modified for Inc# 267923 */
                                if (flagword & FQUOTE) {
                                        /* Make it like buf */
#pragma set woff 1209
                                        char    str[max(MAXDIGS, 
						1+max(MAXFCVT+MAXEXP, 
							2*MAXECVT))];
#pragma reset woff 1209
					long	ndigs;
					int	strsize;

					ndigs = decimal_digits_l(qval);
					strsize = (sizeof(str) - 1);
					str[strsize] = '\0';

#if (_MIPS_SIM != _MIPS_SIM_ABI32)
					if(flagword & LDOUBLE) 
						maxfsig = 2*MAXFSIG;
					else
#endif
						maxfsig = MAXFSIG;

                                        __do_group((char *)str, (double) qval, 
					   prec, (int)ndigs, strsize, maxfsig);

					if (*str == NULL) goto NOGROUPL;
                                        bp = bp - strlen(str);
                                        strcpy(bp, str);
                                } else {
NOGROUPL: {				long ndigs, i,j;
					ndigs = decimal_digits_l(qval);
					bp = bp - ndigs;
				        for (i=ndigs-2;i>=0;i--) {
					    tab = table10_l[i];
					    for (j=0;qval>=tab;j++) {
						qval -= tab;
					    }
					    *bp++ = (char) (j + '0');
					}
					*bp++ = (char) (qval + '0');
					bp = bp - ndigs;
	 }
				}
				}
			}
			} else {	/* !just_32 */
			/* If signed conversion, make sign */
			if (val < 0) {
				prefix = "-";
				prefixlength = 1;
				/*
				 * Negate, checking in
				 * advance for possible
				 * overflow.
				 */
				if (val != HIBITLL)
					val = -val;
				else     /* number is -HIBITLL; convert last */
					 /* digit now and get positive number */
					*--bp = (char)_lowdigit(&val);
			} else if (flagword & FPLUS) {
				prefix = "+";
				prefixlength = 1;
			} else if (flagword & FBLANK) {
				prefix = " ";
				prefixlength = 1;
			}

		decimal:
			{ register long long qval = val, tab;
				if (qval <= 9) {
					if (qval != 0 || !(flagword & DOTSEEN))
						*--bp = qval + '0';
				} else {
                                if (flagword & FQUOTE) {
                                        /* Make it like buf */
#pragma set woff 1209
                                        char    str[max(MAXDIGS, 
						1+max(MAXFCVT+MAXEXP, 
							2*MAXECVT))];
#pragma reset woff 1209
                                        long    ndigs;
					int	strsize;

					ndigs = decimal_digits_ll(qval);
					strsize = (sizeof(str) - 1);
					str[strsize] = '\0';

#if (_MIPS_SIM != _MIPS_SIM_ABI32)
					if(flagword & LDOUBLE) 
						maxfsig = 2*MAXFSIG;
					else
#endif
						maxfsig = MAXFSIG;

                                        __do_group((char *)str, (double) qval, 
					   prec, (int)ndigs, strsize, maxfsig);

					if (*str == NULL) goto NOGROUP;
                                        bp = bp - strlen(str);
                                        strcpy(bp, str);
                                } else {
NOGROUP:{				long ndigs, i,j;
					ndigs = decimal_digits_ll(qval);
					bp = bp - ndigs;
				        for (i=ndigs-2;i>=0;i--) {
					    tab = table10_ll[i];
					    for (j=0;qval>=tab;j++) {
						qval -= tab;
					    }
					    *bp++ = (char) (j + '0');
					}
					*bp++ = (char) (qval + '0');
					bp = bp - ndigs;
	 }
				}
				}
			}
			}		
			/* Calculate minimum padding zero requirement */
			if (flagword & DOTSEEN) {
				register int leadzeroes = (int)(prec - (p - bp));
				if (leadzeroes > 0) {
					otherlength = lzero = leadzeroes;
					flagword |= LZERO;
				}
			}

			break;
			}
		case 'u':
			/* Fetch the argument to be printed */
			if (flagword & LLONG)
				val = va_arg(args.ap, unsigned long long);
			else if (flagword & LENGTH)
				val = (long long) va_arg(args.ap, unsigned long);
			else
				val = va_arg(args.ap, unsigned);

			if (flagword & SHORT)
				val = (unsigned short)val;

			if ((flagword & PADZERO) && (flagword & DOTSEEN))
				flagword &= ~PADZERO; /* ignore 0 flag */

			p = bp = buf + MAXDIGS;

			if (val & HIBITLL)
				*--bp = (char) _lowdigit(&val);

			goto decimal;

		/*
		 *	non-decimal fixed point representations
		 *	for radix equal to a power of two
		 *
		 *	"mradix" is one less than the radix for the conversion.
		 *	"lradix" is one less than the base 2 log
		 *	of the radix for the conversion. Conversion is unsigned.
		 *	HIBITL is 100...000
		 *	binary, and is equal to	the maximum
		 *	negative number.
		 *	We assume a 2's complement machine
		 */

		case 'o':
			mradix = 7;
			lradix = 2;
			goto fixed;

		case 'p':
#if (_MIPS_SZLONG == _MIPS_SZPTR)
			flagword |= LENGTH;
#endif
		case 'X':
		case 'x':
			mradix = 15;
			lradix = 3;

		fixed:
			/* Fetch the argument to be printed */
			if (flagword & LLONG)
				val = va_arg(args.ap, long long);
			else if (flagword & LENGTH)
				val = va_arg(args.ap, long);
			else
				val = va_arg(args.ap, unsigned);

			if (flagword & SHORT)
				val = (unsigned short)val;

			if ((flagword & PADZERO) && (flagword & DOTSEEN))
				flagword &= ~PADZERO; /* ignore 0 flag */

			/* Set translate table for digits */
			  tab = (fcode == 'X') ? uc_digs : lc_digs;

			/* Entry point when printing a double which is a NaN */
		put_pc:
			/* Develop the digits of the value */
			p = bp = buf + MAXDIGS;
			{ register long long qval = val;
				if (flagword & LENGTH)
					qval = (long long)(unsigned long)qval;
				if (qval == 0) {
					if (!(flagword & DOTSEEN)) {
						otherlength = lzero = 1;
						flagword |= LZERO;
					}
				} else
					do {
						*--bp = tab[qval & mradix];
						qval = ((qval >> 1) & ~HIBITLL)
								 >> lradix;
					} while (qval != 0);
			}

			/* Calculate minimum padding zero requirement */
			if (flagword & DOTSEEN) {
				register int leadzeroes = prec - (int)(p - bp);
				if (leadzeroes > 0) {
					otherlength = lzero = leadzeroes;
					flagword |= LZERO;
				}
			}

			/* Handle the # flag */
			if (flagword & FSHARP && val != 0)
				switch (fcode) {
				case 'o':
					if (!(flagword & LZERO)) {
						otherlength = lzero = 1;
						flagword |= LZERO;
					}
					break;
				case 'x':
					prefix = "0x";
					prefixlength = 2;
					break;
				case 'X':
					prefix = "0X";
					prefixlength = 2;
					break;
				}

			break;

		case 'E':
		case 'e':
			/*
			 * E-format.  The general strategy
			 * here is fairly easy: we take
			 * what ecvt gives us and re-format it.
			 */

			/* Establish default precision */
			if (!(flagword & DOTSEEN))
				prec = 6;

			/* Fetch the value */
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
			if(flagword & LDOUBLE) {
				ldval = va_arg(args.ap, long double);
				/* temporarily treat long double as double */
				dval = *((double *) &ldval); 
			} else
#endif
				dval = va_arg(args.ap, double);

			dvalue.d = dval;

                        /* Check for NaNs and Infinities */
			if (IsNANorINF(dval))  {
			   if (IsINF(dval)) {
			      if (IsNegNAN(dval)) 
				neg_in = 1;
			      inf_nan = 1;
			      bp = (char *)((fcode == 'E')? uc_inf: lc_inf);
			      p = bp + 3;
			      break;
                           }
			   else {
				if (IsNegNAN(dval)) 
					neg_in = 1;
				inf_nan = 1;
			      bp = (char *)((fcode == 'E')? uc_nan: lc_nan);
			      p = bp + 3;
			      break;
                           }
			}
			/* Develop the mantissa */
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
			if(flagword & LDOUBLE) {
			  bp = qecvt_r(ldval, min(prec + 1, 2*MAXECVT), 
				      &decpt, &sign, cvtbuf);
			} else
#endif
			  bp = ecvt_r(dval, min(prec + 1, MAXECVT), 
				    &decpt, &sign, cvtbuf);

			/* Determine the prefix */
		e_merge:
			if (sign) {
				prefix = "-";
				prefixlength = 1;
			} else if (flagword & FPLUS) {
				prefix = "+";
				prefixlength = 1;
			} else if (flagword & FBLANK) {
				prefix = " ";
				prefixlength = 1;
			}

			/* Place the first digit in the buffer*/
			p = &buf[0];
			*p++ = (char) ((*bp != '\0') ? *bp++ : '0');

			/* Put in a decimal point if needed */
			if (prec != 0 || (flagword & FSHARP))
				*p++ = _numeric[0];

			/* Create the rest of the mantissa */
			{ register int rz = prec;
				for ( ; rz > 0 && *bp != '\0'; --rz)
					*p++ = *bp++;
				if (rz > 0) {
					otherlength = rzero = rz;
					flagword |= RZERO;
				}
			}

			bp = &buf[0];

			/* Create the exponent */
			*(suffix = &expbuf[MAXESIZ]) = '\0';
			if ( !IsZero(dvalue) )
			{
				register int nn = decpt - 1;
				if (nn < 0)
				    nn = -nn;
				for ( ; nn > 9; nn /= 10)
					*--suffix = (char) todigit(nn % 10);
				*--suffix = (char) todigit(nn);
			}

			/* Prepend leading zeroes to the exponent */
			while (suffix > &expbuf[MAXESIZ - 2])
				*--suffix = '0';

			/* Put in the exponent sign */
			*--suffix = (char) ((decpt > 0 || IsZero(dvalue)) ? '+' : '-');

			/* Put in the e */
			*--suffix = (char) (isupper(fcode) ? 'E' : 'e');

			/* compute size of suffix */
			otherlength += (suffixlength = (int)(&expbuf[MAXESIZ]
								 - suffix));
			flagword |= SUFFIX;

			break;

		case 'b':
		case 'B': 
			/*
			 * Convert sizes to human readable blocks.
			 * Assume input == "1024.0" (yes, that's a float).
			 * %b divides by 1024 until we get something 
			 * less than 1000 and then prints it as 1.000K.
			 * %B divides by 1000 until <1000 and then
			 * prints it as 1.024K.
			 * The default precision is 3.
			 * Bugs to lm@sgi.com.
			 */
			divisor = fcode == 'b' ? 1024 : 1000;
			tag = 0;
			if (!(flagword & DOTSEEN)) {
				prec = 3;
				flagword |= DOTSEEN;	/* fool %f */
			}
			/* FALL THROUGH */

		case 'f':
			/*
			 * F-format floating point.  This is a
			 * good deal less simple than E-format.
			 * The overall strategy will be to call
			 * fcvt, reformat its result into buf,
			 * and calculate how many trailing
			 * zeroes will be required.  There will
			 * never be any leading zeroes needed.
			 */

			/* Establish default precision */
			if (!(flagword & DOTSEEN))
				prec = 6;

			/* Fetch the value */
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
			if(flagword & LDOUBLE) {
				ldval = va_arg(args.ap, long double);
				/* temporarily treat long double as double */
				dval = *((double *) &ldval); 
			} else
#endif
				dval = va_arg(args.ap, double);

			dvalue.d = dval;

                        /* Check for NaNs and Infinities  */
			if (IsNANorINF(dval)) {
			   if (IsINF(dval)) {
			      if (IsNegNAN(dval))
				neg_in = 1;
                              inf_nan = 1;
			      bp = (char *)lc_inf;
			      p = bp + 3;
			      break;
                           }
                           else {
				if (IsNegNAN(dval)) 
					neg_in = 1;
				inf_nan = 1;
			        val  = GETNaNPC(dval);
				NaN_flg = SNLEN;
				mradix = 15;
				lradix = 3;
				tab =  lc_digs;
				SNAN = lc_nan;
				goto put_pc;
                           }
                        } 

			/* support for %b/%B */
			if (divisor) {
				while ((dval >= divisor) && TAGS[tag+1]) {
					dval /= divisor;
					tag++;
				}
			}

			/* Do the conversion */
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
			if(flagword & LDOUBLE) {
			  bp = qfcvt_r(ldval, min(prec, MAXFCVT), 
				      &decpt, &sign, cvtbuf);
			} else
#endif
			  bp = fcvt_r(dval, min(prec, MAXFCVT), 
				    &decpt, &sign, cvtbuf);

			/* Determine the prefix */
		f_merge:
			if (sign && decpt > -prec && *bp != '0') {
				prefix = "-";
				prefixlength = 1;
			} else if (flagword & FPLUS) {
				prefix = "+";
				prefixlength = 1;
			} else if (flagword & FBLANK) {
				prefix = " ";
				prefixlength = 1;
			}

			/* Initialize buffer pointer */
			p = &buf[0];

			{ register int nn = decpt;

#if (_MIPS_SIM != _MIPS_SIM_ABI32)
				if(flagword & LDOUBLE) 
					maxfsig = 2*MAXFSIG;
				else
#endif
					maxfsig = MAXFSIG;

				/* Emit the digits before the decimal point */
				k = 0;
 
				/* modified for Inc# 267923 */
                                if (flagword & FQUOTE) {
					/* Make it like buf */
#pragma set woff 1209
					char	str[max(MAXDIGS, 
						1+max(MAXFCVT+MAXEXP, 
							2*MAXECVT))];
#pragma reset woff 1209
					long    ndigs;
					int	strsize;

                                        ndigs = decimal_digits_ll(dval);
					strsize = (sizeof(str) - 1);
					str[strsize] = '\0';

					__do_group((char *)str, dval, prec, 
						(int)ndigs, strsize, maxfsig);

					if (*str == NULL) goto NOGROUPF;
					strcpy(p, str);
					p += strlen(str); /* omit the NULL*/
					bp += decpt;

				} else {
NOGROUPF: {				  do {
					*p++ = (char) ((nn <= 0 || *bp == '\0' 
						|| k >= maxfsig) ?
				    		'0' : (k++, *bp++));
					     } while (--nn > 0);
	  }
				}
				/* Decide whether we need a decimal point */
				if ((flagword & FSHARP) || prec > 0)
					*p++ = _numeric[0];

				/* Digits (if any) after the decimal point */
				nn = min(prec, MAXFCVT);
				if (prec > nn) {
					flagword |= RZERO;
					otherlength = rzero = prec - nn;
				}
				while (--nn >= 0)
					*p++ = (char) ((++decpt <= 0 || *bp == '\0' ||
				   	    k >= maxfsig) ? '0' : (k++, *bp++));
			}

			bp = &buf[0];

			/* more support for %b/%B */
			if (divisor) {
				*p++ = fcode == 'b' ? tags[tag] : TAGS[tag];
				divisor = 0;
			}

			break;

		case 'G':
		case 'g':
			/*
			 * g-format.  We play around a bit
			 * and then jump into e or f, as needed.
			 */
		
			/* Establish default precision */
			if (!(flagword & DOTSEEN))
				prec = 6;
			else if (prec == 0)
				prec = 1;

			/* Fetch the value */
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
			if(flagword & LDOUBLE) {
				ldval = va_arg(args.ap, long double);
				/* temporarily treat long double as double */
				dval = *((double *) &ldval); 
			} else
#endif
				dval = va_arg(args.ap, double);

			dvalue.d = dval;

			/* Check for NaN and Infinities  */
			if (IsNANorINF(dval)) {
			   if (IsINF(dval)) {
			      if (IsNegNAN(dval)) 
				neg_in = 1;
			   bp = (char *)((fcode == 'G') ? uc_inf : lc_inf);
			   p = bp + 3;
                           inf_nan = 1;
			   break;
                           }
                           else {
				if (IsNegNAN(dval)) 
					neg_in = 1;
				inf_nan = 1;
			        val  = GETNaNPC(dval);
				NaN_flg = SNLEN;
				mradix = 15;
				lradix = 3;
				if ( fcode == 'G') {
					SNAN = uc_nan;
					tab = uc_digs;
				}
				else {
					SNAN = lc_nan;
					tab =  lc_digs;
				}
				goto put_pc;
                           }
                        }

			/* Do the conversion */
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
			if(flagword & LDOUBLE) {
			  bp = qecvt_r(ldval, min(prec, 2*MAXECVT), 
				      &decpt, &sign, cvtbuf);
			} else
#endif
			  bp = ecvt_r(dval, min(prec, MAXECVT), 
				    &decpt, &sign, cvtbuf);
			if ( IsZero(dvalue) )
				decpt = 1;

			{ register int kk = prec;
				if (!(flagword & FSHARP)) {
					n = (int) strlen(bp);
					if (n < kk)
						kk = (int)n;
					while (kk >= 1 && bp[kk-1] == '0')
						--kk;
				}
				
				if (decpt < -3 || decpt > prec) {
					prec = kk - 1;
					goto e_merge;
				}
				prec = kk - decpt;
				goto f_merge;
			}

		case '%':
			buf[0] = (char)fcode;
			goto c_merge;

		case 'c':
			buf[0] = va_arg(args.ap, int);
		c_merge:
			p = (bp = &buf[0]) + 1;
			break;

#ifndef NO_MSE
		case 'C':
		{
			wchar_t wc;

			wc = va_arg(args.ap, wchar_t);
			if ((k = wctomb(buf, wc)) < 0)	/* bogus wc */
				k = 0;
			bp = buf;
			p = buf + k;
			break;
		}
#endif /*NO_MSE*/

		case 's':
			bp = va_arg(args.ap, char *);
			if (bp == NULL) {       /* robustness addition */
				bp = "(null)";
				flagword &= ~DOTSEEN;
			}
			if (!(flagword & DOTSEEN))
				p = bp + strlen(bp);
			else { /* a strnlen function would  be useful here! */
				register char *qp = bp;
				while (*qp++ != '\0' && --prec >= 0)
					;
				p = qp - 1;
			}
			break;

#ifndef NO_MSE
		case 'S':
		{
			wchar_t *wp, *wp0;
			int n, tot;

			/*
			* Handle S conversion entirely here, not below.
			* Only prescan the input if there's a chance
			* for left padding.
			*/
			wp0 = va_arg(args.ap, wchar_t *);
			if (wp0 == NULL)
				wp0 = (wchar_t *)wnull_buf;
			if (width != 0 && (flagword & FMINUS) == 0)
			{
				n = 0;
				for (wp = wp0; *wp != 0; wp++)
				{
					if ((k = wctomb(buf, *wp)) > 0)
					{
						n += k;
						if (flagword & DOTSEEN && n > prec)
						{
							n -= k;
							break;
						}
					}
				}
				if (width > n)
				{
					n = width - n;
					count += n;
					PAD((char *)_blanks, n);
					width = 0;
				}
			}
			if ((flagword & DOTSEEN) == 0)
				prec = MAXINT;
			/*
			* Convert one wide character at a time into the
			* local buffer and then copy it to the stream.
			* This isn't very efficient, but it's correct.
			*/
			tot = 0;
			wp = wp0;
			while (*wp != 0)
			{
				if ((k = wctomb(buf, *wp++)) <= 0)
					continue;
				if ((prec -= 1) < 0)
					break;
				PUT(buf, k);
				tot += 1;
			}
			count += tot;
			if (width > tot)
			{
				tot = width - tot;
				count += tot;
				PAD(_blanks, tot);
			}
			continue;
		}
#endif /*NO_MSE*/

		case 'n':
		      {
			if (flagword & LLONG) {
				long long *svcount;
				svcount = va_arg(args.ap, long long *);
				*svcount = count;
			} else if (flagword & LENGTH) {
				long *svcount;
				svcount = va_arg(args.ap, long *);
				*svcount = count;
			} else if (flagword & SHORT) {
				short *svcount;
				svcount = va_arg(args.ap, short *);
				*svcount = (short)count;
			} else {
				int *svcount;
				svcount = va_arg(args.ap, int *);
				*svcount = count;
			}
			continue;
		      }

		default: /* this is technically an error; what we do is to */
			/* back up the format pointer to the offending char */
			/* and continue with the format scan */
			format--;
			continue;

		}
       
		if (inf_nan) {
		   if (neg_in) {
			prefix = "-";
			prefixlength = 1;
			neg_in = 0;
                   }
		   else if (flagword & FPLUS) {
			prefix = "+";
			prefixlength = 1;
			}
		   else if (flagword & FBLANK) {
			prefix = " ";
			prefixlength = 1;
		   }
		   inf_nan = 0;
		}
		 
		/* Calculate number of padding blanks */
		k = (int)(n = p - bp) + prefixlength + otherlength + NaN_flg;
		if (width <= k)
			count += k;
		else {
			count += width;

			/* Set up for padding zeroes if requested */
			/* Otherwise emit padding blanks unless output is */
			/* to be left-justified.  */

			if (flagword & PADZERO) {
				if (!(flagword & LZERO)) {
					flagword |= LZERO;
					lzero = width - k;
				}
				else
					lzero += width - k;
				k = width; /* cancel padding blanks */
			} else
				/* Blanks on left if required */
				if (!(flagword & FMINUS))
					PAD(_blanks, width - k);
		}

		/* Prefix, if any */
		if (prefixlength != 0)
			PUT(prefix, prefixlength);

		/* If value is NaN, put string NaN*/
		if (NaN_flg) {
			PUT(SNAN,SNLEN);
			NaN_flg = 0;
                }

		/* Zeroes on the left */
		if (flagword & LZERO)
			PAD(_zeroes, lzero);
		
		/* The value itself */
		if (n > 0)
			PUT(bp, n);

		if (flagword & (RZERO | SUFFIX | FMINUS)) {
			/* Zeroes on the right */
			if (flagword & RZERO)
				PAD(_zeroes, rzero);

			/* The suffix */
			if (flagword & SUFFIX)
				PUT(suffix, suffixlength);

			/* Blanks on the right if required */
			if (flagword & FMINUS && width > k)
				PAD(_blanks, width - k);
		}
	}
}

/* This function initializes arglst, to contain the appropriate va_list values
 * for the first MAXARGS arguments. */
void
_mkarglst(char *fmt, stva_list args, stva_list arglst[])
{
	static const char digits[] = "01234567890", skips[] = "# +-.0123456789h$'";

	enum types {INT = 1, LONG, CHAR_PTR, DOUBLE, LONG_DOUBLE, VOID_PTR,
		LONG_PTR, INT_PTR, LONGLONG_PTR, LONG_LONG, WCHAR, WCHAR_PTR};
	enum types typelst[MAXARGS], curtype;
	int maxnum, n, curargno, flags;
	/* flags values: 1=l, 2=*, 4=ll, 8=L */

	/*
	* Algorithm	1. set all argument types to zero.
	*		2. walk through fmt putting arg types in typelst[].
	*		3. walk through args using va_arg(args.ap, typelst[n])
	*		   and set arglst[] to the appropriate values.
	* Assumptions:	Cannot use %*$... to specify variable position.
	*/

	(void)memset((VOID *)typelst, 0, sizeof(typelst));
	maxnum = -1;
	curargno = 0;
	while ((fmt = strchr(fmt, '%')) != 0)
	{
		fmt++;	/* skip % */
		if (fmt[n = (int) strspn(fmt, digits)] == '$')
		{
			curargno = atoi(fmt) - 1;	/* convert to zero base */
			if (curargno < 0)
				continue;
			fmt += n + 1;
		}
		flags = 0;
	again:;
		fmt += strspn(fmt, skips);
		switch (*fmt++)
		{
		case '%':	/*there is no argument! */
			continue;
		case 'l':
			if (flags & 0x1) {
				flags ^= 0x1;
				flags |= 0x4;
			} else
				flags |= 0x1;
			goto again;
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
		case 'L':
			flags |= 0x8;
			goto again;
#endif
		case '*':	/* int argument used for value */
			/* check if there is a positional parameter */
			if (isdigit(*fmt)) {
				int	targno;
				targno = atoi(fmt) - 1;
				fmt += strspn(fmt, digits);
				if (*fmt == '$')
					fmt++; /* skip '$' */
				if (targno >= 0 && targno < MAXARGS) {
					typelst[targno] = INT;
					if (maxnum < targno)
						maxnum = targno;
				}
				goto again;
			}
			flags |= 0x2;
			curtype = INT;
			break;
		case 'e':
		case 'E':
		case 'f':
		case 'g':
		case 'G':
			if(flags & 0x8) 
			  curtype = LONG_DOUBLE;
			else
			  curtype = DOUBLE;
			break;
		case 's':
			curtype = CHAR_PTR;
			break;
		case 'p':
			curtype = VOID_PTR;
			break;
		case 'n':
			if (flags & 0x4)
				curtype = LONGLONG_PTR;
			else if (flags & 0x1)
				curtype = LONG_PTR;
			else
				curtype = INT_PTR;
			break;
#ifndef NO_MSE
		case 'C':
			curtype = WCHAR;
			break;
		case 'S':
			curtype = WCHAR_PTR;
			break;
#endif /*NO_MSE*/
		default:
			if (flags & 0x4)
				curtype = LONG_LONG;
			else if (flags & 0x1)
				curtype = LONG;
			else
				curtype = INT;
			break;
		}
		if (curargno >= 0 && curargno < MAXARGS)
		{
			typelst[curargno] = curtype;
			if (maxnum < curargno)
				maxnum = curargno;
		}
		curargno++;	/* default to next in list */
		if (flags & 0x2)	/* took care of *, keep going */
		{
			flags ^= 0x2;
			goto again;
		}
	}
	for (n = 0 ; n <= maxnum; n++)
	{
		arglst[n] = args;
		if (typelst[n] == 0)
			typelst[n] = INT;
		
		switch (typelst[n])
		{
		case INT:
			(void) va_arg(args.ap, int);
			break;
#ifndef NO_MSE
		case WCHAR:
			(void) va_arg(args.ap, wchar_t);
			break;
		case WCHAR_PTR:
			(void) va_arg(args.ap, wchar_t *);
			break;
#endif /*NO_MSE*/
		case LONG:
			(void) va_arg(args.ap, long);
			break;
		case CHAR_PTR:
			(void) va_arg(args.ap, char *);
			break;
		case DOUBLE:
			(void) va_arg(args.ap, double);
			break;
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
		case LONG_DOUBLE:
			(void) va_arg(args.ap, long double);
			break;
#endif
		case VOID_PTR:
			(void) va_arg(args.ap, VOID *);
			break;
		case LONG_PTR:
			(void) va_arg(args.ap, long *);
			break;
		case INT_PTR:
			(void) va_arg(args.ap, int *);
			break;
		case LONG_LONG:
			(void) va_arg(args.ap, long long);
			break;
		case LONGLONG_PTR:
			(void) va_arg(args.ap, long long *);
			break;
		}
	}
}

/*
 * This function is used to find the va_list value for arguments whose
 * position is greater than MAXARGS.  This function is slow, so hopefully
 * MAXARGS will be big enough so that this function need only be called in
 * unusual circumstances.
 * pargs is assumed to contain the value of arglst[MAXARGS - 1].
 */
void
_getarg(char *fmt, stva_list *pargs, int argno)
{
	static const char digits[] = "0123456789", skips[] = "# +-.0123456789h$";
	int i, n, curargno, flags;
	char	*sfmt = fmt;
	int	found = 1;

	i = MAXARGS;
	curargno = 1;
	while (found)
	{
		fmt = sfmt;
		found = 0;
		while ((i != argno) && (fmt = strchr(fmt, '%')) != 0)
		{
			fmt++;	/* skip % */
			if (fmt[n = (int) strspn(fmt, digits)] == '$')
			{
				curargno = atoi(fmt);
				if (curargno <= 0)
					continue;
				fmt += n + 1;
			}

			/* find conversion specifier for next argument */
			if (i != curargno)
			{
				curargno++;
				continue;
			} else
				found = 1;
			flags = 0;
		again:;
			fmt += strspn(fmt, skips);
			switch (*fmt++)
			{
			case '%':	/*there is no argument! */
				continue;
			case 'l':
				if (flags & 0x1) {
					flags ^= 0x1;
					flags |= 0x4;
				} else
					flags |= 0x1;
				goto again;
			case 'L':
				flags |= 8;
				goto again;
			case '*':	/* int argument used for value */
				/* check if there is a positional parameter;
				 * if so, just skip it; its size will be
				 * correctly determined by default */
				if (isdigit(*fmt)) {
					fmt += strspn(fmt, digits);
					if (*fmt == '$')
						fmt++; /* skip '$' */
					goto again;
				}
				flags |= 0x2;
				(void)va_arg((*pargs).ap, int);
				break;
			case 'e':
			case 'E':
			case 'f':
			case 'g':
			case 'G':
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
				if (flags & 0x8)
				        (void)va_arg((*pargs).ap, long double);
				else
#endif
					(void)va_arg((*pargs).ap, double);
				break;
			case 's':
				(void)va_arg((*pargs).ap, char *);
				break;
			case 'p':
				(void)va_arg((*pargs).ap, void *);
				break;
			case 'n':
				if (flags & 0x4)
					(void)va_arg((*pargs).ap, long long *);
				else if (flags & 0x1)
					(void)va_arg((*pargs).ap, long *);
				else
					(void)va_arg((*pargs).ap, int *);
				break;
#ifndef NO_MSE
			case 'C':
				(void)va_arg((*pargs).ap, wchar_t);
				break;
			case 'S':
				(void)va_arg((*pargs).ap, wchar_t *);
				break;
#endif /*NO_MSE*/
			default:
				if (flags & 0x4)
					(void)va_arg((*pargs).ap, long long);
				else if (flags & 0x1)
					(void)va_arg((*pargs).ap, long int);
				else
					(void)va_arg((*pargs).ap, int);
				break;
			}
			i++;
			curargno++;	/* default to next in list */
			if (flags & 0x2)	/* took care of *, keep going */
			{
				flags ^= 0x2;
				goto again;
			}
		}

		/* missing specifier for parameter, assume parameter is an int */
		if (!found && i != argno) {
			(void)va_arg((*pargs).ap, int);
			i++;
			curargno = i;
			found = 1;
		}
	}
}

static long
decimal_digits_ll(long long qval)
{
    long n1,n,n2;

    n1 = 0; n2 = 17;
    if (qval >= table10_ll[n2]) return(n2+2);

    while (n2-n1 > 1){
	n = (n2-n1)>>1;
	n += n1;
	if (qval < table10_ll[n]) n2 = n;
	else n1 = n;
    }
    return (n1+2); 
}

static long
decimal_digits_l(long qval)
{
    long n1,n,n2;

    n1 = 0; n2 = 8;
    if (qval >= table10_l[n2]) return(n2+2);

    while (n2-n1 > 1){
	n = (n2-n1)>>1;
	n += n1;
	if (qval < table10_l[n]) n2 = n;
	else n1 = n;
    }
    return (n1+2); 
}
