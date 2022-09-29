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
#include "synonyms.h"
#include "shlib.h"
#include "mplib.h"

#include <limits.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <locale.h>
#include <stdlib.h>
#include <nl_types.h>
#include <langinfo.h>
#include <stdarg.h>
#include <values.h>
#include <monetary.h>
#include <errno.h>

#include <stdio.h>
#pragma weak strfmon = _strfmon
#define TRUE -1
#define FALSE 0

static char     *input;
static char     *output;
static char     *save;
static const char     null = {0};
static ssize_t count;
static ssize_t max_cnt;
static struct  lconv *fmon;

static int process_conv(double val) ;
static int number(int);
static double dint (double);
static ssize_t do_fmon(va_list);

static int put( char * , int );
#define OUTPUT( A, n )       if (put(( A) ,( n)) < 0) return (TRUE)

/* 	
 *	State flags for parser:
 *		strict ordering of optional elements.
 *		some flags in contrastive distribution.
 *		promotion style restriction logic.
 */
#define P_INITIAL	FALSE		/* general initial state  */
#define N_INITIAL	TRUE		/* numeric form initial state */
#define	P_FLAG		1
#define P_WIDTH		2
#define P_LEFT		3
#define P_RIGHT		4

#define MARGIN_BUF	10			/* format overhead */
#define P_BUF_SZ	(DBL_DIG + MARGIN_BUF)     /* from limits.h */

/* max transform output size for a given format specificier */
#define T_BUF_SZ	((DBL_DIG * 3) + MARGIN_BUF)

/* per _doprnt.c:
 * stva_list is used to subvert C's restriction that a variable with an
 * array type can not appear on the left hand side of an assignment operator.
 * By putting the array inside a structure, the functionality of assigning to
 * the whole array through a simple assignment is achieved..
 */


typedef struct stva_list {
	va_list ap;
} stva_list;

void _mkarglst(char *, stva_list, stva_list []);
void _getarg(char *, stva_list *, int);


/* ARGSUSED */
ssize_t 
strfmon(char *s, size_t maxsize, const char *format, ...)
{
	va_list ap;
	ssize_t ret;
	LOCKDECLINIT(l, LOCKLOCALE);

	va_start(ap, format);
/*
 * strfmon() provides XPG4 conforming output according to
 * its input format paramters.  
 *
 * This embodiment uses a simplified interface modeled from 
 * sprintf to handle variable arguments.  Only DOUBLE argument
 * types are allowed.  The do_fmon procedure manages the 
 * argument list.
 *
 * Two error conditions are described in the XPG4 specification,
 * ENOSYS for unsupported feature and E2BIG for size limitation.
 * The latter case, as well as exceptions not addressed by the
 * specification are handled at lower levels.  The only error
 * identifiable at this level is the case where localeconv()
 * returns NULL, the processing for which is according to the
 * specification:
 *
 */

	if((fmon = localeconv()) == (struct lconv *)NULL) {
		UNLOCKLOCALE(l);
		setoserror(ENOSYS);
		return(TRUE);
	}

/* 
 * Once the locale dependent formatting rules are captured
 * we only have a little setup before processing the conversion.
 *
 * Here we globalize a couple of paramters, which really is 
 * needed to handle the unsigned nature of an SGI size_t
 * as much as to simplify paramter passing.
 *
 */

	input = (char *) format;
	output = (char *) s;
	strcpy (s, &null); 		/* XPG4: need to NULL the output string 
					to handle uninitialized param cases */
	count = 0;
	max_cnt = (ssize_t) maxsize;

/* 
 * do_fmon() and the code below it do the actual work
 */

	ret = do_fmon(ap);

	va_end(ap);
	UNLOCKLOCALE(l);

	if (ret >= 0)
		s[ret] = '\0';
	return(ret);
}

/*
 * Scan format as input, outputing plain characters 
 * to the output s.  The '%%' conversion specifier 
 * requires no argument, and is processed directly at
 * this layer.  
 *
 * Other specifiers requiring an argument are dispatched
 * to process_conv(val) if val exists and is of the correct
 * type (DOUBLE).  In that the insufficient arguments case 
 * is an undefined state within the XPG4 specification (and
 * the invalid argument condition is not even mentioned), 
 * the treatment must be 'implementor defined'. This code 
 * passes the remaining format text as plain characters,
 * returning TRUE and EINVAL in such conditions.
 *
 * Normal conditions return the count of bytes transfered
 * to output.
 *
 */
static ssize_t 
do_fmon(va_list in_args)
{
	register int fcode;

	/* The value being converted, if real */
	/* This is the ONLY type allowed in strfmon() */
	double  dval;
	int need_arg = TRUE;
	stva_list	args;

        /* Initialize args and sargs to the start of the argument list.
         * Note that ANSI guarantees that the address of the first member of
         * a structure will be the same as the address of the structure.
	 */

	args =  *(struct stva_list *)&in_args;


	/* 	The main loop -- this loop goes through one iteration
         *      for each string of ordinary characters or format specification.
         */
	for ( ; ; ) {
		register int n;

		if ((fcode = *input) != '\0' && fcode != '%') {
			save = (char *)input;
			do {
				input++;
			} while ((fcode = *input) != '\0' && fcode != '%');

			n = ((int) (input - save)); /* n = no. of non-% chars */
			OUTPUT( save, n);
		}
		if (fcode == '\0')
			goto empty;

		input++; /* a format specifier begins -- look ahead one char */
		if ((fcode = *input) ==  '%') {
			/* just to handle the '%%' case */
			OUTPUT(input, 1); /* '%%' produces single % on output */
			input++;
			continue;
		}  /* input is advanced to first element after '%' specifier */

		if (need_arg)
			dval = va_arg(args.ap, double); /* expect double */

		if ((n = process_conv(dval)) == TRUE)
			return (n); /* errno was set */
		if (n == ( TRUE * EINVAL)) {
			need_arg = 0; /* reuse the paramater */
			continue;
		}
		need_arg = TRUE;
		if (n != (TRUE * EIO ))
			continue;

empty:	/* end of format; return output count unless too big */
		OUTPUT((char *)&null, 0);
		return (count);
	}
}


/*
 * common output routine invoked via OUTPUT(A, n) macro:
 * In cases where the output generated would exceed the 
 * caller designated maximum, no output is produced: 
 * instead the put routine sets errno to E2BIG and  
 * returns TRUE (-1) per the XPG4 specification.
 *
 */
static int 
put(char *from, int n)
{
	if ((count+n) < max_cnt) {
		strncat (output, from, n);
		count += n;
		return ((int) count);
	}
	setoserror(E2BIG);
	return (TRUE);
}

/*
 * Parse a number given by the specification.
 * Allow at most length digits.
 */
static int
number(int length)
{
	int     val;
	unsigned char c;

	val = 0;
	if(((c = *input)) == '\0')
		return -2;
	if(!isdigit(c))
		return TRUE;
	while (length--) {
		if((c = *input) == '\0')
			return -2;
		if(!isdigit(c))
			return val;
		val = 10*val + c - '0';
		input++;
	}
	return val;
}

/*
 * function to process a conversion specification 
 * having an existing monetary (DOUBLE) argument.  
 *
 * State errors are returned as (TRUE  * error code):
 *
 * If the specification is well-formed and there is room, 
 * output the string generated and return current count. 
 *
 * A truncated input (0 byte occuring before the converison
 * character) is ill-formed.       
 *
 * If the input is ill-formed as a specifier, no conversion
 * is done. An attempt is made to output the ill-formed
 * string as ordinary text via OUTPUT(A,n). If output is 
 * successful, either (TRUE * EINVAL), notifying the upper 
 * layer that the argument was not used and should not be 
 * discarded, or (TRUE * EIO), indicating the input was 
 * exhausted, is returned.
 *
 */
static int 
process_conv(double val)
{
	int p_state 	=	P_INITIAL;
	int f_fill_char	= 	P_INITIAL;
	int f_no_delim	=	P_INITIAL;
	int f_neg_rep 	=	P_INITIAL;
	int f_no_symb 	=	P_INITIAL;
	int f_l_align 	=	P_INITIAL;
	int w_min 	=	N_INITIAL;
	int l_prec	=	N_INITIAL;
	int r_prec	=	N_INITIAL;
	int c_type  	=	P_INITIAL;
	int code, temp_d;
	int negative;
	char *s_dec = NULL;
	char *s_frac = NULL;
	char *grouping = NULL;
	char *cur_sep = NULL;
	char *cur_sym = NULL;
	char *sign = NULL;
	char *thou_sep = NULL;
	char *radix = NULL;
	int  sign_posn ;
	int i, j, d_digits;
	int cur_prec;
	int sep_by_space;
	int need_pad = FALSE;

	static const char flag_values [] = {
		'=','^','+','(','!','-','\0'	};
	static const char conv_types [] = {
		'i','n','\0'	};
	static const char blank [] = {
		' ','\0'	};
	static const char l_par [] = {
		'(','\0'        };
	static const char r_par [] = {
                ')','\0'        };

	char *l_bracket;
	char *r_bracket;
	char fill [8] ;
	char tbuf[T_BUF_SZ]; /* used to queue output */
	char fmt [T_BUF_SZ]; /* general scratch area */
	char nbuf[P_BUF_SZ]; /* from sprintf: strings for decimal & fraction */

	l_bracket = (char *)&l_par[0];
	r_bracket = (char *)&r_par[0];

	(void) memset ((void *) tbuf, '\0', T_BUF_SZ);
	(void) memset ((void *) fmt, '\0', T_BUF_SZ);
	(void) memset ((void *) nbuf, '\0', P_BUF_SZ);

	save = (char *) input; /* remember the input position */
	save--; 	/* back up to the initial '%' that led here */
	goto check_token;

/*
 *	Promotion style parser, restrictively allowing all and only
 *	the permitted format conversion specification elements in 
 *	the order designated.  All non-matches are processed as 
 *	regular character strings.
 */

next_token:
	input++;
check_token:
	if ((code = *input) == '\0' )
		goto parse_empty;

	if (strchr(flag_values, code) != NULL) {
		if (p_state > P_FLAG)
			goto parse_fail;
		switch(code) {
		case '=':
			if (f_fill_char != P_INITIAL)
				goto parse_fail; /* only 1 occurence */
			input++; /* following char is target */
			if ((f_fill_char = *input) == '\0' )
				goto parse_empty;
			break;

		case '^':
			if (f_no_delim != P_INITIAL)
				goto parse_fail;
			f_no_delim = TRUE;
			break;

		case '+':
		case '(':
			if (f_neg_rep != P_INITIAL)
				goto parse_fail;
			f_neg_rep = code;
			break;

		case '!':
			if (f_no_symb != P_INITIAL)
				goto parse_fail;
			f_no_symb = TRUE;
			break;

		case '-':
			if (f_l_align != P_INITIAL)
				goto parse_fail;
			f_l_align = TRUE;
			break;

		default:
			goto parse_fail; /* never taken */
		}
		p_state = P_FLAG;
		goto next_token;
	}

	if (isdigit (code) ) {
		if (w_min != N_INITIAL || p_state > P_WIDTH)
			goto parse_fail;
		if ((w_min = number (2)) >= 0) {
			if (w_min  >= (T_BUF_SZ))
				goto parse_fail;
			p_state = P_WIDTH;
			goto check_token;
		};
		if (w_min == N_INITIAL)
			goto parse_fail;
		goto parse_empty;
	}

	if (code == '#') {
		if (l_prec != N_INITIAL || p_state > P_LEFT)
			goto parse_fail;

		input++;
		if ((l_prec  = number (2)) >= 0) {
			p_state = P_LEFT;
			goto check_token;
		}
		if (l_prec == N_INITIAL)
			goto parse_fail;
		goto parse_empty;
	}


	if (code == '.') {
		if (r_prec != N_INITIAL || p_state > P_RIGHT)
			goto parse_fail;
		input++;
		if ((r_prec  = number (2)) >= 0) {
			p_state = P_RIGHT;
			goto check_token;
		};
		if (r_prec == N_INITIAL)
			goto parse_fail;
		goto parse_empty;
	}


	if (strchr(conv_types, code) != NULL)
		goto format_ok;


parse_fail:	
	c_type = EINVAL;
	goto fail_return;

parse_empty:	
	c_type = EIO;

fail_return:	 
	c_type *= TRUE;
	p_state  = ((int) (input - save));

	OUTPUT(save , p_state);
	return (c_type);

format_ok:	
/* 
 *	Parse completed and acceptable conversion specification found.
 */
	c_type = code; 
	input++;
	if (f_fill_char == P_INITIAL)
		f_fill_char = ' '; /* space default */

	fill[0] = f_fill_char;
	fill[1] = '\0';

	if (f_neg_rep == P_INITIAL)
		f_neg_rep  = '+'; /* + default */
	if (w_min == N_INITIAL)
		w_min = 0;
	switch (c_type) {
	case 'i': /* international defaults */

		if (r_prec == N_INITIAL)
			r_prec = fmon->int_frac_digits;
		cur_sym = fmon->int_curr_symbol;
		break;

	case 'n': /* national defaults */
		if (r_prec == N_INITIAL)
			r_prec = fmon->frac_digits;
		cur_sym = fmon->currency_symbol;
		break;
	}
	if (f_no_symb )
		cur_sym = (char *)&blank[1];
	if (r_prec < 0 )
		r_prec = 0;
	grouping = fmon->mon_grouping;
	thou_sep = fmon->mon_thousands_sep;

	/* handle rounding before conversion */
	temp_d = 1;
	if (r_prec > 0)  /* set pwer to desired precision */
		for (p_state = r_prec; p_state > 0 ; p_state--)
			temp_d *= 10;
	val *= temp_d;
	if ((val > 0.0) && ((val - dint(val)) > .5 ))
		val += .5;

	if (val < 0.0) {
		negative = TRUE;
		if ((val - dint(val)) < .5 ) val -= .5;
	} else
		negative = FALSE;

	val =  (dint (val)) / temp_d;
	p_state = i = 0;
	if (negative) {
		sep_by_space = fmon->n_sep_by_space ;
		cur_prec = fmon->n_cs_precedes;
		sign = fmon->negative_sign;
		sign_posn = fmon->n_sign_posn;
	} else {
		sep_by_space = fmon->p_sep_by_space ;
		cur_prec = fmon->p_cs_precedes;
		sign = fmon->positive_sign;
		sign_posn = fmon->p_sign_posn;

		if (f_neg_rep == '(' && sign_posn != 0) {
			l_bracket = (char *)&blank[1];
			r_bracket = (char *)&blank[1];
		}
	}
	if (sep_by_space == 0 || f_no_symb)
		p_state = 1;
	cur_sep = (char *)&blank[p_state];
	radix = fmon->mon_decimal_point;
	if (r_prec == 0)
		radix = (char *)&blank[1];

/* 
 *	at this point conversion specification parsing 
 *	has completed successfully, the val argument
 *	is accessible, and the formating parameters are
 *	captured.  It only remains to generate the specified 
 *	output. The conversion method includes sprintf: 
 *
 *	%f yields: (<neg_sign>) <decimal> <radix> <fraction> 
 *	of which the <decimal> and <fraction> strings are useful:
 */
	(void) sprintf(nbuf, "%f",   val);
/*
 *	ignore the leading sign char if any:
 */
	p_state = (negative)? 1: 0;
/*
 *	decimal string is first:
 */
	s_dec = &nbuf[p_state];
	for (i = p_state; nbuf [i] !=  '\0'; i++) {
		if (nbuf [i] == *fmon->decimal_point) {
			nbuf [i] = '\0';
			i++;
			break;
		}
	}
	d_digits = (int ) strlen (s_dec);
/*
 *	fraction string may need added zeros depending on speficied
 *	right precision:
 */
	s_frac = & nbuf[i];
	if ((i = (int) (strlen(s_frac) - r_prec)) > 0)
		do {
			strcat (s_frac, "0");
			i--;
		} while (i > 0);
	s_frac[r_prec] = '\0';
/*
 *	now handle fill characters and separator insertions for
 *	decimal component of value:
 */

	l_prec -= d_digits;

/*
 *	in no case will there be 'negative padding'
 */
	l_prec = (l_prec < 0)? 0: l_prec;
	need_pad = (l_prec >  0)? TRUE:FALSE;
	p_state = d_digits + l_prec;
	j = 0;
	tbuf[0] = '\0';
	do {
		fmt[0] = '\0';
		if (((i = grouping [j] ) != CHAR_MAX) &&
			(i != 0) && (f_no_delim == FALSE)) 
		{ 	
			/* grouping applies */
			if (grouping[j+1] != 0)
				j++; /* a successor grouping */

			if (d_digits > i) {
				strcat (fmt, thou_sep);
				d_digits -=  i;
				p_state -= i;
				strncat (fmt, &s_dec[d_digits], i);
			} else {
				/* less significant digits than grouping calls.
				 * maybe enough l_prec to require a fill char ? 
				 */
				if ( p_state  > i )
					strncat (fmt, fill,1);
				else
					i = p_state; /* this is final group */
				/* 
				 * supply leading fill char digits as needed:
				 */
				for (f_fill_char = i- d_digits; f_fill_char > 0; 
					f_fill_char--)
				{
					if (l_prec <= 0)
						break;
					strncat (fmt, fill,1);
					l_prec--;
					p_state--;
				}
				/* 
				 * complete any remaining significant digits
				 */
				if (d_digits > 0) {
					strcat (fmt, s_dec);
					p_state -= d_digits;
					d_digits = 0;
				}
			}
			s_dec [d_digits] = '\0';
			strcat (fmt, tbuf); /* append any saved text */
			if (p_state > 0)
				strcpy (tbuf, fmt); /* save if needed */
		} else { 
			/* 
			 * this path is reached because 
 			 * the grouping rules are either BLOCKED or exhausted 
 			 * or there are no groupings for the locale.
			 * process as below:
			 */
			for (f_fill_char=l_prec; f_fill_char > 0; f_fill_char--)
                                        strncat (fmt, fill,1);
			strcat (fmt, s_dec);
			strcat (fmt, tbuf);
			p_state = TRUE;
		}	
	} while (p_state > 0);

/*	
 *	at this point, a properly formatted decimal expression 
 *	in fmt is used to present a correct numeric image in tbuf.
 *	[nbuf is now free for other uses].
 */
	(void) sprintf(tbuf, "%s%s%s", fmt, radix, s_frac);
/*
 *	for now we just put the target into fmt:
 */
	strcpy (fmt, tbuf); /* save numeric image for later */
/*
 *	Two general flavors of output depending on negative 
 *	representation mode:
 */
	p_state = (sep_by_space == 2)? 0:1; 
	s_dec = (char *) &blank[p_state];
	strcpy(nbuf, "%s%s%s%s%s");

	if (f_neg_rep == '(' || sign_posn == 0 ) {
		/* Either explicit or implied parenthetic */
		if (cur_prec == 1)
			sprintf(fmt, nbuf, l_bracket, cur_sym, 
				cur_sep, tbuf, r_bracket);
		else
			sprintf(fmt, nbuf, l_bracket, tbuf,  
				cur_sep, cur_sym, r_bracket);
	} else {
		/* explicit positive and negative tokens */
		switch(sign_posn) {
			case 1: /* sign precedes {value, denomination } */ 
				if (cur_prec == 0) {
					strcpy(nbuf, "%s%s%s%s");
					sprintf(fmt, nbuf, sign, tbuf, 
							cur_sep, cur_sym);
				} else 
					sprintf(fmt, nbuf, sign, s_dec, 
							cur_sym, cur_sep, tbuf);
				break;

			case 2: /* sign follows {value, denomination } */
                                if (cur_prec == 1) {
					strcpy(nbuf, "%s%s%s%s");
					sprintf(fmt, nbuf, cur_sym, 
						cur_sep, tbuf, sign);
                                } else 
                                        sprintf(fmt, nbuf, tbuf, cur_sep, 
						cur_sym, s_dec, sign);
                                break;

			case 3: /* sign immediately precedes denomination */
				if (cur_prec == 0) {
					strcpy(nbuf, "%s%s%s%s");
                                        sprintf(fmt, nbuf, tbuf, sign, 
						s_dec, cur_sym);
				} else
                                        sprintf(fmt, nbuf, sign, s_dec, 
						cur_sym, cur_sep, tbuf);
				break;

			case 4: /* sign immediately follows denomination */
                                if (cur_prec == 1) {
                                        strcpy(nbuf, "%s%s%s%s");
                                        sprintf(fmt, nbuf, cur_sym, 
						s_dec, sign,tbuf);
				} else
                                        sprintf(fmt, nbuf, tbuf, cur_sep,
						cur_sym, s_dec, sign);
				break;
		}
	}

/*
 *	padding oriented fill character?
 */
	if (need_pad  && negative == FALSE && 
			f_neg_rep == '+' && fmon->p_cs_precedes == 1 &&
			fmon->n_cs_precedes == 1 && fmon->p_sign_posn == 3 &&
			fmon->n_sign_posn == 3 && 
			fmon->p_sep_by_space == fmon->n_sep_by_space && 
			((strlen(fmon->negative_sign)) - 
				(strlen(fmon->positive_sign))) > 0) 
	{
		need_pad = TRUE;	
	} else {
		need_pad = FALSE;	
	}
/*
 *	padding depends on localeconv results:
 */
	p_state = (need_pad)? 0:1;
	s_dec = (char *) &blank[p_state];


/* 	FINALLY send the numeric to output aligned according to the
 *	specification passed
 */
	strcpy (tbuf, s_dec);
	strcat (tbuf, fmt);
	strcpy (fill, "%");
	if (f_l_align)
		strcat (fill, "-");

	sprintf(nbuf, "%s%d%s",&fill, w_min, "s");

	c_type = sprintf(fmt, nbuf, tbuf);
	
	OUTPUT( fmt , c_type);
	return ((int) count);
}

static double
dint (double x)
{
	register int tmp = (int) x;
	return ((double) tmp);
}
