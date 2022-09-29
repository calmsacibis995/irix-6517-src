/*
 * (C) Copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND
 *
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the 
 * Rights in Technical Data and Computer Software clause at DFARS 252.227-7013 
 * and/or in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement. Unpublished -- rights reserved under the Copyright Laws 
 * of the United States. Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd., Mountain View, CA 94039-7311.
 *
 * THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF SGI
 *
 * The copyright notice above does not evidence any actual or intended 
 * publication or disclosure of this source code, which includes information 
 * that is the confidential and/or proprietary, and is a trade secret,
 * of Silicon Graphics, Inc. Any use, duplication or disclosure not 
 * specifically authorized in writing by Silicon Graphics is strictly 
 * prohibited. ANY DUPLICATION, MODIFICATION, DISTRIBUTION,PUBLIC PERFORMANCE,
 * OR PUBLIC DISPLAY OF THIS SOURCE CODE WITHOUT THE EXPRESS WRITTEN CONSENT 
 * OF SILICON GRAPHICS, INC. IS STRICTLY PROHIBITED.  THE RECEIPT OR POSSESSION
 * OF THIS SOURCE CODE AND/OR INFORMATION DOES NOT CONVEY ANY RIGHTS 
 * TO REPRODUCE, DISCLOSE OR DISTRIBUTE ITS CONTENTS, OR TO MANUFACTURE, USE, 
 * OR SELL ANYTHING THAT IT MAY DESCRIBE, IN WHOLE OR IN PART.
 */

/*	Copyright (c) 1990, 1991, 1992 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:stdio/doscan.c	2.41.1.1" 
/*LINTLIBRARY*/
#include "synonyms.h"
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdarg.h>
#include <values.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <sgidefs.h>
#include "_wchar.h"
#include "_locale.h"

#define SCRATCH_BUF_INIT_SIZE 128
typedef struct scratch_buf {
  char *ptr;
  int size;
} scratch_buf_t;
#define NCHARS	(1 << BITSPERBYTE)

/* if the _IOWRT flag is set, this must be a call from sscanf */
#define locgetc()	(chcount+=1,(iop->_flag & _IOWRT) ? \
				((*iop->_ptr == '\0') ? EOF : *iop->_ptr++) : \
				getc(iop))
#define locungetc(x)	(chcount-=1, (x == EOF) ? EOF : \
				((iop->_flag & _IOWRT) ? *(--iop->_ptr) : \
				  (++iop->_cnt, *(--iop->_ptr))))

static int              number(int, int, int, int, FILE *, int *, int *, scratch_buf_t *, va_list *);
static int 		readchar(FILE *, int *);
static unsigned char	*setup(unsigned char *, char *);
static int 		string(int, int, int, char *, FILE *, int *, int *, va_list *);
static int 		wstring(int, int, int, FILE *, int *, int *, va_list *);
static int 		nextwc(FILE *, wchar_t *, int *);

#define	MAXARGS	30	/* max. number of args for fast positional paramters */

/* stva_list is used to subvert C's restriction that a variable with an
 * array type can not appear on the left hand side of an assignment operator.
 * By putting the array inside a structure, the functionality of assigning to
 * the whole array through a simple assignment is achieved..
 */
typedef struct stva_list {
	va_list	ap;
} stva_list;

static int _mkarglst(char *, stva_list, stva_list []);

int
_doscan(register FILE *iop, 
	register unsigned char *fmt, 
	va_list va_alist)
{
	int chcount, flag_eof;
	char tab[NCHARS];
	register int ch;
	int nmatch = 0, len, inchar, stow, size;
	scratch_buf_t scratch_buf;

	/* variables for postional parameters */
	unsigned char	*sformat = fmt;	/* save the beginning of the format */
	int	fpos = 1;	/* 1 if first postional parameter */
	stva_list	args,	/* used to step through the argument list */
			sargs;	/* used to save the start of the argument list */
	stva_list	arglst[MAXARGS]; /* array giving the appropriate values
					  * for va_arg() to retrieve the
					  * corresponding argument:
					  * arglst[0] is the first argument
					  * arglst[1] is the second argument, etc.
					  */

	/* Check if readable stream */
	if (!(iop->_flag & (_IOREAD | _IORW))) {
		setoserror(EBADF);
		return(EOF);
	}

	/* Initialize args and sargs to the start of the argument list.
	 * Note that ANSI guarantees that the address of the first member of
	 * a structure will be the same as the address of the structure. */
	args = sargs = *(struct stva_list *)&va_alist;

	chcount=0; flag_eof=0;

	scratch_buf.ptr = NULL;

	/*******************************************************
	 * Main loop: reads format to determine a pattern,
	 *		and then goes to read input stream
	 *		in attempt to match the pattern.
	 *******************************************************/
	for ( ; ; ) 
	{
		if ( (ch = *fmt++) == '\0')
		  {
		        if (scratch_buf.ptr != NULL)
			  free(scratch_buf.ptr);
			return(nmatch); /* end of format */
		  }
		if (isspace(ch)) 
		{
		  	if (!flag_eof) 
			{
			   while (isspace(inchar = locgetc()))
	 		        ;
			   if (locungetc(inchar) == EOF)
				flag_eof = 1;
		        }
		  continue;
                }
		if (ch != '%' || (ch = *fmt++) == '%') 
                {
			if ( (inchar = locgetc()) == ch )
				continue;
			if (locungetc(inchar) != EOF)
			  {
			        if (scratch_buf.ptr != NULL)
				  free(scratch_buf.ptr);
				return(nmatch); /* failed to match input */
			  }
			break;
		}
	charswitch:
		if (ch == '*') 
		{
			stow = 0;
			ch = *fmt++;
		}
		else
			stow = 1;

		for (len = 0; isdigit(ch); ch = *fmt++)
			len = len * 10 + ch - '0';
		if (ch == '$') 
		{
			/* positional parameter handling - the number
			 * specified in len gives the argument to which
			 * the next conversion should be applied.
			 * WARNING: This implementation of positional
			 * parameters assumes that the sizes of all pointer
			 * types are the same. (Code similar to that
			 * in the portable doprnt.c should be used if this
			 * assumption does not hold for a particular
			 * port.) */
			if (fpos) 
			{
				if (_mkarglst((char *)sformat, sargs, arglst) != 0)
				  {
				        if (scratch_buf.ptr != NULL)
					  free(scratch_buf.ptr);
					return(EOF);
				  }
			}
			if (len <= MAXARGS) 
			{
				args = arglst[len - 1];
			} else {
				args = arglst[MAXARGS - 1];
				for (len -= MAXARGS; len > 0; len--)
					(void)va_arg(args.ap, void *);
			}
			len = 0;
			ch = *fmt++;
			goto charswitch;
		}

		if (len == 0)
			len = MAXINT;
		if ( (size = ch) == 'l' || (size == 'h') || (size == 'L') )
			ch = *fmt++;
		if ((size == 'l') && (ch == 'l')) {
			/* "%ll" indicates longlong */
			size = 'L';
			ch = *fmt++;
		}
#if (_MIPS_SIM == _MIPS_SIM_ABI32)
		/* If we are not supporting quadword floating-point, then
		 * default 'L' to 'l'.
		 */
		else if ( size == 'L' )
			/* ANSI - if the format char specifies floating-point,
			   'L' == 'l'.  Otherwise undefined.  We define
			   it to imply 'l'.  */
			size = 'l';
#endif
		if (ch == '\0' ||
		    ch == '[' && (fmt = setup(fmt, tab)) == NULL)
		  {
		        if (scratch_buf.ptr != NULL)
			  free(scratch_buf.ptr);
			return(EOF); /* unexpected end of format */
		  }
		if (isupper(ch))  /* no longer documented */
		{
                        /* ANSI specifies the following behavior for uppercase
                           format characters:
                                * E, G, X behave the same as e,g, and x.
                                * others produce undefined behavior
                        */
                        if ((ch != 'E')&&(ch != 'G')&&(ch != 'X'))
                                if (size != 'L') size = 'l';

#ifndef NO_MSE
			if (ch != 'C' && ch != 'S')
#endif
				ch = _tolower(ch);
		}
		if (ch!= 'n' && !flag_eof)
#ifndef NO_MSE
		  if (ch != 'c' && ch != '[' && ch != 'C')
#else
		  if (ch != 'c' && ch != '[')
#endif /*NO_MSE*/
		  {
			while (isspace(inchar = locgetc()))
				;
			if(locungetc(inchar) == EOF )
				break;
		  }
		switch(ch)
		{
#ifndef NO_MSE
		 case 'C':
		 case 'S':
			 size = wstring(stow,ch,len,iop,&chcount,&flag_eof,&args.ap);
			 break;
#endif /*NO_MSE*/
		 case 'c':
		 case 's':
		 case '[':
			  size = string(stow,ch,len,tab,iop,&chcount,&flag_eof,&args.ap);
			  break;
                 case 'n':
			  if (!stow)
				continue;
			  if (size == 'h')
				*va_arg(args.ap, short *) = (short) chcount;
		          else if (size == 'l')
				*va_arg(args.ap, long *) = (long) chcount;
			  else if (size == 'L')
				*va_arg(args.ap, long long *) = (long long) chcount;
			  else
			  	*va_arg(args.ap, int *) = (int) chcount;
			  continue;
		 case 'i':
                 default:
		         /* set up the scratch buf, if needed */ 
		         if (scratch_buf.ptr == NULL) {
			   if ((scratch_buf.ptr =
				(char *)malloc(SCRATCH_BUF_INIT_SIZE))
			       == NULL) {
			     setoserror(ENOMEM);
			     return(EOF);
			   }
			   scratch_buf.size = SCRATCH_BUF_INIT_SIZE;
			 }
			 size = number(stow, ch, len, size, iop, &chcount,
				       &flag_eof, &scratch_buf, &args.ap);
			 break;
                 }
		   if (size) 
		          nmatch += stow;
		   else {
		     if (scratch_buf.ptr != NULL)
		       free(scratch_buf.ptr);
		     return ((flag_eof && !nmatch) ? EOF : nmatch);
		   }
                continue;
	}
	if (scratch_buf.ptr != NULL)
	  free(scratch_buf.ptr);
	return (nmatch != 0 ? nmatch : EOF); /* end of input */
}

/***************************************************************
 * Functions to read the input stream in an attempt to match incoming
 * data to the current pattern from the main loop of _doscan().
 ***************************************************************/
static int
number(int stow, 
	int type, 
	int len, 
	int size, 
	register FILE *iop,
	int *pchcount, 
	int *flag_eof,
        scratch_buf_t *numbuf_s,
	va_list *listp)
{
        char *numbuf = numbuf_s->ptr;
	register char *np = numbuf;
	register int inchar, lookahead, c, base;
	int digitseen = 0, dotseen = 0, expseen = 0, floater = 0, negflg = 0;
	long lcval = 0;
	long long llcval = 0;
	int chcount = *pchcount;
	switch(type) 
	{
	case 'e':
	case 'f':
	case 'g':
		floater++;
		/* FALLTHROUGH */
	case 'd':
	case 'u':
        case 'i':
		base = 10;
		break;
	case 'o':
		base = 8;
		break;
	case 'x':
	case 'p':
		base = 16;
		break;
	default:
		return(0); /* unrecognized conversion character */
	}
	switch(c = locgetc()) 
	{
	case '-':
		negflg++;
		/* FALLTHROUGH */
	case '+':
		if (--len <= 0)
		   break;
		if ( (c = locgetc()) != '0')
		   break;
		/* FALLTHROUGH */
        case '0':
                if ( (type != 'i' && type != 'x') || (len <= 1) )  
		   break;
	        if ( ((inchar = locgetc()) == 'x') || (inchar == 'X') ) 
	        {
		   lookahead = readchar(iop, &chcount);
		   if ( isxdigit(lookahead) )
		   {
		       base =16;

		       if ( len <= 2)
		       {
			  locungetc(lookahead);
			  len -= 1;            /* Take into account the 'x'*/
                       }
		       else 
		       {
		          c = lookahead;
			  len -= 2;           /* Take into account '0x'*/
		       }
                   }
	           else
	           {
	               locungetc(lookahead);
	               locungetc(inchar);
                   }
		}
	        else
	        {
		    locungetc(inchar);
		    if (type == 'i')
	            	base = 8;
                }
	}
	for (; --len  >= 0 ; *np++ = (char)c, c = locgetc()) 
	{
	        /* resize the scratch buffer and update information */
	        if (np - numbuf > numbuf_s->size - 2) {
		  np = (char *)(np - numbuf); /* cache the relative position
						 of np */
		  numbuf_s->size += SCRATCH_BUF_INIT_SIZE;
		  numbuf = (char *)realloc(numbuf, numbuf_s->size);
		  numbuf_s->ptr = numbuf;
		  if (numbuf == NULL) {
		    setoserror(ERANGE);
		    *pchcount = chcount;
		    return(0);
		  }
		  np = &numbuf[(long)np];
		}
		if (isdigit(c) || base == 16 && isxdigit(c)) 
		{
			int digit = c - (isdigit(c) ? '0' :  
			    isupper(c) ? 'A' - 10 : 'a' - 10);
			if (digit >= base)
				break;
			if (stow && !floater)
				if (size == 'L')
					llcval = base * llcval + digit;
				else
					lcval = base * lcval + digit;
			digitseen++;


			continue;
		}
		if (!floater)
			break;
		if (c == _numeric[0] && !dotseen++)
			continue;
		if ( (c == 'e' || c == 'E') && digitseen && !expseen++) 
                {
			if (--len < 0)
				break;
			inchar = readchar(iop, &chcount);
			if (isdigit(inchar))
			{
				*np++ = (char)c;
				c = inchar;
				continue;
			}
			else if (inchar == '+' || inchar == '-' ||
                                (isspace(inchar) && (_lib_version == c_issue_4)))
			{
				if ((len-1) < 0)
				{
					locungetc(inchar);
					break;
				}
				--len;
				lookahead = readchar(iop, &chcount);
				if (isdigit(lookahead))
				{
					*np++ = (char)c;
					*np++ = (char)inchar;
					c = lookahead;
					continue;
				}
				else
				{
					locungetc(lookahead);
					locungetc(inchar);
				}
			}
			else
			{
				locungetc(inchar);
			}
		}
		break;
	}


	if (stow && digitseen)
		if(floater) 
		{
			register double dval;
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
			long double	ldval;
#endif
	
			*np = '\0';

#if (_MIPS_SIM != _MIPS_SIM_ABI32)
			if (size == 'L')
			{
				ldval = atold(numbuf);
				if (negflg)
					ldval = -ldval;
			}
			else
#endif
			{
				dval = atof(numbuf);
				if (negflg)
					dval = -dval;
			}

			if (size == 'l') 
				*va_arg(*listp, double *) = dval;
#if (_MIPS_SIM != _MIPS_SIM_ABI32)
			else if (size == 'L')
				*va_arg(*listp, long double *) = ldval;
#endif
			else
				*va_arg(*listp, float *) = (float)dval;
		}
		else
		{
			/* suppress possible overflow on 2's-comp negation */
			if (negflg && lcval != HIBITL)
				lcval = -lcval;
			if ((size == 'l')||(size == 'p'))
				*va_arg(*listp, long *) = lcval;
			else if (size == 'L') {
				if (negflg && llcval != HIBITLL)
					llcval = -llcval;
				*va_arg(*listp, long long * ) = llcval;
			}
			else if (size == 'h')
				*va_arg(*listp, short *) = (short)lcval;
			else
				*va_arg(*listp, int *) = (int)lcval;
		}
	if (locungetc(c) == EOF)
	    *flag_eof=1;
	*pchcount = chcount;
	return (digitseen); /* successful match if non-zero */
}

/* Get a character. If not using sscanf and at the buffer's end 
 * then do a direct read(). Characters read via readchar()
 * can be  pushed back on the input stream by locungetc()
 * since there is padding allocated at the end of the stream buffer. */
static int
readchar(FILE *iop, int *pchcount)
{
	char	inchar;
	register int chcount = *pchcount;

	if ((iop->_flag & _IOWRT) || (iop->_cnt != 0))
		inchar = (char)locgetc();
	else
	{
		if ( read(fileno(iop),&inchar,1) != 1)
			return(EOF);
		chcount += 1;
	}
	*pchcount = chcount;
	return(inchar);
}

static int
string(register int stow, 
	register int type, 
	register int len, 
	register char *tab,
	register FILE *iop,
	int *pchcount, 
	int *flag_eof,
	va_list *listp)
{
	register int chcount;
	register int ch;
	register char *ptr;
	char *start;

	chcount = *pchcount;
	start = ptr = stow ? va_arg(*listp, char *) : NULL;
	if (type == 'c' && len == MAXINT)
		len = 1;
	while ( (ch = locgetc()) != EOF &&
	    !(type == 's' && isspace(ch) || type == '[' && tab[ch])) 
        {
		if (stow) 
			*ptr = (char)ch;
		ptr++;
		if (--len <= 0)
			break;
	}
	if (ch == EOF ) 
	{
	       *flag_eof = 1;
	       chcount-=1;
        }
        else if (len > 0 && locungetc(ch) == EOF)
		*flag_eof = 1;
	*pchcount = chcount;
	if (ptr == start)
		return(0); /* no match */
	if (stow && type != 'c')
		*ptr = '\0';
	return (1); /* successful match */
}

static unsigned char *
setup(register unsigned char *fmt, register char *tab)
{
	register int b, c, d, t = 0;

	if (*fmt == '^') 
	{
		t++;
		fmt++;
	}
	(void) memset(tab, !t, NCHARS);
	if ( (c = *fmt) == ']' || c == '-')  /* first char is special */
	{
		tab[c] = (char)t;
		fmt++;
	}
	while ( (c = *fmt++) != ']') 
	{
		if (c == '\0')
			return(NULL); /* unexpected end of format */
		if (c == '-' && (d = *fmt) != ']' && (b = fmt[-2]) < d) 
		{
			(void) memset(&tab[b], t, (size_t)(d - b + 1));
			fmt++;
		} 
		else
			tab[c] = (char)t;
	}
	return (fmt);
}

#ifndef NO_MSE
	/*
	* Return EOF if first byte is EOF.
	* Otherwise, 0 if a bad byte is reached (and left unread).
	* Otherwise, n>0 if n bytes form a valid multibyte character.
	*/
static int
nextwc(register FILE *iop, wchar_t *wp, int *pchcount)
{
	register int ch;
	register int i;
	register int chcount = *pchcount;
	char str[MB_LEN_MAX + 1];

	if((ch = locgetc()) == EOF)
		return EOF;
	locungetc(ch);
	
	for(i=0; i<MB_CUR_MAX; i++) {
		if((ch = locgetc()) == EOF) {
			*pchcount = chcount;
			return 0;
		}
		str[i] = ch;
		if(mbtowc(wp, (const char *)str, i+1) > 0) {
			*pchcount = chcount;
			return i+1;
		}
	}
	while (i >= 0)
		locungetc((int)str[i--]);
	*pchcount = chcount;
	return 0;
}

static int
wstring(register int stow, 
	register int type, 
	register int len,
	register FILE *iop,
	int *pchcount, 
	int *flag_eof,
	va_list *listp)
{
	register wchar_t *wp;
	register int n;
	wchar_t wc;
	wchar_t *start;
	register int chcount = *pchcount;

	start = wp = stow ? va_arg(*listp, wchar_t *) : NULL;
	if (type == 'C' && len == MAXINT)
		len = 1;
	while ((n = nextwc(iop, &wc, pchcount)) > 0 &&
		!(type == 'S' && isascii(wc) && isspace(wc)))
        {
		if (stow) 
			*wp = wc;
		wp++;
		if (--len <= 0)
			break;
	}
	if (n == EOF) 
	{
	       *flag_eof = 1;
	       chcount--;
        }
	else if (len > 0 && n > 0 && locungetc(wc) == EOF)
		*flag_eof = 1;
	*pchcount = chcount;
	if (wp == start)
		return(0); /* no match */
	if (stow && type != 'C')
		*wp = 0;
	return (1); /* successful match */
}
#endif /*NO_MSE*/


/* This function initializes arglst, to contain the appropriate 
 * va_list values for the first MAXARGS arguments.
 * WARNING: this code assumes that the sizes of all pointer types
 * are the same. (Code similar to that in the portable doprnt.c
 * should be used if this assumption is not true for a
 * particular port.) */
static int
_mkarglst(char *fmt, stva_list args, stva_list arglst[])
{
	int maxnum, n, curargno;

	maxnum = -1;
	curargno = 0;
	while ((fmt = strchr(fmt, '%')) != 0)
	{
		fmt++;	/* skip % */
		if (*fmt == '*' || *fmt == '%')
			continue;
		if (fmt[n = (int)strspn(fmt, "0123456789")] == '$')
		{
			curargno = atoi(fmt) - 1;	/* convert to zero base */
			fmt += n + 1;
		}
		if (maxnum < curargno)
			maxnum = curargno;
		curargno++;	/* default to next in list */

		fmt += strspn(fmt, "# +-.0123456789hL$");
		if (*fmt == '[') {
			fmt++; /* has to be at least on item in scan list */
			if ((fmt = strchr(fmt, ']')) == NULL)
				return(-1); /* bad format */
		}
	}
	if (maxnum > MAXARGS)
		maxnum = MAXARGS;
	for (n = 0 ; n <= maxnum; n++)
	{
		arglst[n] = args;
		(void)va_arg(args.ap, void *);
	}
	return(0);
}
