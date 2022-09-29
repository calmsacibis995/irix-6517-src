/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)printf:printf.c	1.2"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <locale.h>
#include <string.h>
#include <libgen.h>
#include <pfmt.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

static int  format_in (char *str, int, char**);
static void printbyte (char *fmt, char *);
static void printlong (char *fmt, char *, int);


#define CONV_CHARS	"diouxXcbs"		/* conversion character */

#define FLAG_CHARS	"$-+ #0123456789."	/* conversion flags */

#define CONS_CHARS	"\'\""			/* character constant leader */

static const char overflow[] =
	"\"%s\" arithmetic overflow\n";

static const char badchar[] =
	"\"%s\" not completely converted\n";

static const char badstr[] =
	"\"%s\" expected numeric value\n";

/* type of numeric value */
#define UNSIGNED 	1
#define SIGNED 		0

char	cmd_label[] = "UX:printf";

static int errcode = 0; /* exit status */
static int bincount = 0;

main(argc, argv)
int	argc;
char	**argv;
{
	char	*fmt, *cp;
	int 	n, i;

	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	if(argc == 1) {
	    _sgi_nl_usage(SGINL_USAGE, cmd_label,
		gettxt(_SGI_DMMX_printf_usage,
		    "printf format [[[arg1] arg2] ... argn]"));
	    exit(1);
	}

	if( !(fmt = malloc(strlen(argv[1]) + 1))) {
	    _sgi_nl_error(SGINL_NOSYSERR, cmd_label,
		gettxt(_SGI_DMMX_outofmem, "Out of memory"));
	    exit(1);
	}
	if( !strcmp(argv[1], "--") ) {
		if (argc > 0) {
			strccpy(fmt, argv[2]);
			argc -= 3;
			argv += 3;
		} else {
			exit(0);
		}
	} else {
		if ((strlen(argv[1]) == 0) && (argc == 2))
			exit(0);
		/*
		 *  Special case a 'printf \000\000\000' sequence
		 */
		cp = fmt;
		for (i=0; i<(strlen(argv[1]) + 1); i++)
			*cp++ = 0x7f;
		(void) strccpy(fmt, argv[1]);

		if ((strlen(fmt) == 0) && strlen(argv[1])) {
			/*
			 *  We are here because just binary zeroes are being
			 *  printed and nothing else.
			 */
			cp = fmt;
			for (i=0; i<strlen(argv[1]); i++) {
				if ((*cp == 0) && 
					(*cp == 0x7f) && (*(cp + 1) == 0x7f)) {
					exit(0);
				}
				if ((*cp == 0x7f) || (*(cp + 1) == 0x7f))
					exit(0);
				putchar(*cp++);
			}
			exit(0);
		}
		bincount = 0;
		if ((strlen(argv[1]) - strlen(fmt)) > 0) {
			if (strlen(fmt + strlen(fmt) + 1) == 0) {
				cp = (fmt + strlen(fmt));
				for (i=0;i<(strlen(argv[1])-strlen(fmt));i++) {
					if ((*cp == 0) && (*(cp + 1) == 0x7f))
						break;
					if (*cp == 0x7f)
						break;
					cp++;
					bincount++;
				}
			}
		}

		argc -= 2;
		argv += 2;
	}
	do {
		n = format_in(fmt, argc, argv) ;
		argc -= n;
		argv += n;
	} while (n != 0 && argc > 0);
	exit(errcode ? 2:0) ;
	/*NOTREACHED*/
}

static int
format_in(char *str, int argc, char **argv)
{
	register char 	*p;
	register char 	*s = str;
	static   char 	*tmpf = NULL;/* temporary format buffer */

	register int 	conv;	/* conversion character */
	register char  	*sval;	/* string pointer */
	int 		nargs = 0;

	int		looping = 1;	/* re-using format or not flag */
	int		posn, oldposn;	/* position specifiers */ 
	char		*remains;	/* remains of a converted number */
	char		*posnstr;	/* pos'n spec string */
	int		i;

	/*  allocate temporary format buffer */
	if (tmpf == NULL) 
		if ((tmpf = malloc(strlen(str) + 1)) == NULL) {
	    		_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				gettxt(_SGI_DMMX_outofmem, "Out of memory"));
			exit(1);
		}

	posn = 0;

	if (strlen(s) == 0) {
		(void) putchar((int)*s);
		return(++nargs);
	}
	while (*s) {

		if (*s != '%') {
			(void) putchar((int)*s++) ;
			continue;
		}
		if (*(s+1) == '\0') {
			(void) putchar((int)*s++) ;
			continue;
		}
		/* %% string */
		if (*(s+1) == '%') {
			(void) putchar((int)*s) ;
			s+=2;
			continue;
		}

		oldposn = posn;
		posn = 0;

		/*
		 * skip flags, grabbing and removing the pos'n
		 * specifier, if it exists
		 */
		for (p=tmpf; (*p++ = *s++) != '\0'; ) {
			if (*s == '$') {
				*p = '\0';
				for (posnstr=p; isdigit(*(--posnstr)););
				posnstr++;
				posn = strtol(posnstr, &remains, 10);
				if (*remains == '\0' && remains != posnstr) {
					/*
					 * don't re-use the format once we have
					 * found a position specifier
					 */
					looping = 0;

					/* remove pos'n spec */
					p = posnstr;
					s++;
				} else {
					break;
				}
			}
			if (strchr(FLAG_CHARS, (int) *s) == NULL)
				break;
		}
		/* conversion character */
		conv = *s++;
		if (conv == '\0' ||
				strchr(CONV_CHARS, conv) == NULL) {
			*p = '\0';
			(void) printf(tmpf) ;
			if (conv != '\0')
				(void) putchar(conv) ;
			posn = oldposn;
			continue ;
		}

		if (conv == 'b')
			*p++ = 's' ;
		else
			*p++ = conv ;
		*p = '\0';

		/* get argument */
		if (posn == 0) {
			posn = oldposn + 1;
		}
		if (posn > argc) {
			sval = "";
		} else {
			sval = argv[posn - 1];
		}
		nargs++;

		switch(conv) {
		case 'c':			/* char */
			if (*sval)
				(void) printf(tmpf, *sval);
			break ;
		case 's':			/* string %s */
			(void) printf(tmpf, sval) ;
			break ;
		case 'b':			/* string %b */
			printbyte(tmpf, sval) ;
			break ;
		case 'd': case 'i':		/* signed long */
			printlong(tmpf, sval, SIGNED) ;
			break ;
		case 'x': case 'X': case 'o': case 'u':	/* unsigned long*/
			printlong(tmpf, sval, UNSIGNED) ;
			break ;
		}
	}
	if (bincount) {
		for (i=0; i< bincount; i++)
			putchar(*(str + strlen(str) + i));
	}
	return(looping ? nargs : 0);
}

#define isodigit(c)	(c >= '0' && c <= '7')

static void
printbyte(char *fmt, char *str)
{
	register char 	*s, *p;
	register int	quit=0;
	register int	n;
	char odigit[4];

	for (s = p  = str  ; *p != '\0' && quit == 0 ; p++) {
		if (*p == '\\' ) {
			switch(*++p) {
			case '\\':	 	/* <backslash> */
				*s++ = '\\';
				continue;
			case 'a': 		/* <alert> */
				*s++ = '\007';	/* ='\a'*/
				continue;
			case 'b': 		/* <backspace> */
				*s++ = '\b';
				continue;
			case 'f': 		/* <form-feed> */
				*s++ = '\f';
				continue;
			case 'n': 		/* <newline> */
				*s++ = '\n';
				continue;
			case 'r': 		/* <carriage return> */
				*s++ = '\r';
				continue;
			case 't': 		/* <tab> */
				*s++ = '\t';
				continue;
			case 'v': 		/* <vertical tab> */
				*s++ = '\v';
				continue;
			case 'c':
				quit++;
				continue;
			case '0': 		/* \0ddd octal constant*/
				n = 0;
				while (*++p && isodigit(*p) && n < 3) {
					odigit[n++] = *p;
				}
				odigit[n] = '\0';
				*s++ = strtol(odigit, NULL, 8);
				--p;
				continue;
			default:
				--p;
			}
		}
		*s++ = *p;
	}

	*s = '\0';
	(void) printf(fmt, str) ;
	if (quit)
		exit(errcode ? 2:0);
	return;
}

static void
printlong(char *fmt, char *str, int type)
    {
	wchar_t 	wc = 0;
	unsigned long 	lval ;
	int		i;
	char 		*badnum;
	char		outmsg[256];

	/* print value of the character */
	if (str[0] == '\0')
		lval = 0;
	else if (strchr(CONS_CHARS, (int) str[0]) != NULL) {
		 if (mbtowc(&wc, &str[1], MB_CUR_MAX) <= 0) {
			for (i=0; i<sizeof(outmsg); i++)
				outmsg[i] = 0;
			i = sprintf(outmsg, "%s:%s", 
				_SGI_DMMX_printfbadc, badchar);
			pfmt(stderr, MM_WARNING, outmsg, str);
			errcode++;
		 }
		 lval = wc ;
	} else {
		if (type == UNSIGNED)
			lval = strtoul(str, &badnum, 0);
		else
			lval = strtol(str, &badnum, 0);
		
		if (*badnum != '\0') {
			for (i=0; i<sizeof(outmsg); i++)
				outmsg[i] = 0;
			if (lval) {
				i = sprintf(outmsg, "%s:%s", 
					_SGI_DMMX_printfbadc, badchar);
				pfmt(stderr, MM_WARNING, outmsg, str);
			} else {
				i = sprintf(outmsg, "%s:%s", 
					_SGI_DMMX_printfbads, badstr);
				pfmt(stderr, MM_WARNING, outmsg, str);
			}
			errcode++;
		} else if (errno == ERANGE) {
			for (i=0; i<sizeof(outmsg); i++)
				outmsg[i] = 0;
			i = sprintf(outmsg, "%s:%s", 
				_SGI_DMMX_printfoverf, overflow);
			pfmt(stderr, MM_WARNING, outmsg, str);
	    		setoserror(0);
			errcode++;
		}
	}
	(void) printf(fmt, lval);
}
