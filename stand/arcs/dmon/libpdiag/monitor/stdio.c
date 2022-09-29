#ident	"$Id: stdio.c,v 1.1 1994/07/21 01:16:17 davidl Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#include "sys/types.h"
#include <varargs.h>
/*
*/
#include <ctype.h>
#include "saioctl.h"

#ifndef DIAG_MONITOR
#include "sys/cmn_err.h"
#endif !DIAG_MONITOR

#define	CTRL(x)		('x' & 0x1f)

#define	DEL		0x7f
#define	INTR		CTRL(C)
#define	BELL		0x7
#define	LINESIZE	512
#define	PROMPT		"? "

#ifndef DIAG_MONITOR
void putc();
void putchar();
void puts();
#else
#define getc(x)		_getchar()
#define getchar(x)	_getchar()
#define putc(x)		_putchar(x)
#define putchar(x)	_putchar(x)
#define puts(x)		_puts(x)
#endif !DIAG_MONITOR

void showchar();
char *gets();
void printf();
void sprintf();
static void prf();
static char *printn();

#define PUTCHAR(c)		if (buf) { *(buf+1) = 0; *buf++ = c; } \
                                else putchar(c)

#define PUTS(str)		if (buf) { register int i; \
                                      for (i = 0; i < strlen(str); i++) \
				 	   PUTCHAR(str[i]); } else puts(str)
int zeropad;
int numpad;
int firsttime;


#ifdef DIAG_MONITOR
/*
 * _ctype_[] is copied from saio/libc.c since in the monitor libc.o
 * is not included. This char array is used by the macros in ctype.h
 */

/* @(#)ctype_.c	4.2 (Berkeley) 7/8/83 */

char _ctype_[] = {
	0,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_C,	_S,	_S,	_S,	_S,	_S,	_C,	_C,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_S|_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P,
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P,
	_N,	_N,	_N,	_N,	_N,	_N,	_N,	_N,
	_N,	_N,	_P,	_P,	_P,	_P,	_P,	_P,
	_P,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U,
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U,
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U,
	_U,	_U,	_U,	_P,	_P,	_P,	_P,	_P,
	_P,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L,
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L,
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L,
	_L,	_L,	_L,	_P,	_P,	_P,	_P,	_C
};
#endif DIAG_MONITOR


#ifndef DIAG_MONITOR
/*
 * Get a single character from enabled console devices blocks 
 * until character available, returns 7 bit ascii code.
 */
getchar()
{
	char c;

	read(0, &c, 1);
	return(c & 0x7f);
}

/*
 * Get character only from particular device blocks 
 * until character available, returns 8 bits.
 */
getc(fd)
int fd;
{
	char c;

	if (read(fd, &c, 1) <= 0)
		return(EOF);

	return(c & 0xff);
}

/*
 * Get an input line from user handles basic line editing buf 
 * assumed to be LINESIZE bytes.
 */
char *gets(buf)

register char *buf;
{
	register char c;
	register char *bufp;

	bufp = buf;
	for (;;) {
		c = getchar();

		switch (c) {

		case CTRL(V):			/* quote next char */
			c = getchar();

			/*
			 * Make sure there's room for this character
			 * plus a trailing \n and 0 byte
			 */
			if (bufp < &buf[LINESIZE - 3]) {
				*bufp++ = c;
				showchar(c);
			} else
				putchar(BELL);
			break;

		case '\n':
		case '\r':
			putchar('\n');
			*bufp = 0;
			return(buf);

		case CTRL(H):
		case DEL:
			/*
			 * Change this to a hardcopy erase????
			 */
			if (bufp > buf) {
				bufp--;
				putchar(CTRL(H));
				putchar(' ');
				putchar(CTRL(H));
			}
			break;

		case CTRL(U):
			if (bufp > buf) {
				printf("^U\n%s", PROMPT);
				bufp = buf;
			}
			break;

		case '\t':
			c = ' ';		/* simplifies CTRL(H) dramatically */
						/* fall through to default case */

		default:
			/*
			 * Make sure there's room for this character
			 * plus a trailing \n and 0 byte
			 */
			if (isprint(c) && bufp < &buf[LINESIZE - 3]) {
				*bufp++ = c;
				putchar(c);
			} else
				putchar(BELL);
			break;
		}
	}
}


static int column;	/* current output column for tab processing */

/*
 * Put a single character to enabled console devices 
 * handles simple character mappings.
 */
void putchar(c)

char c;

{
	extern int stdio_init;

	switch (c) {
	case '\n':
		putchar('\r');

	case '\r':
		column = 0;
		break;

	case '\t':
		do {
			putchar(' ');
		} while (column % 8);
		return;

	default:
		if (isprint(c))
			column++;
		break;
	}

	if (!stdio_init || write(1, &c, 1) != 1)
		_errputc(c);			/* attempt to print something */
}


/*
 * Put a character only to a single device.
 */
void putc(c, fd)

char c;
int fd;

{
	write(fd, &c, 1);
}


void puts(s)

register char *s;

{
	register c;

	if (s == NULL)
		s = "<NULL>";
	while (c = *s++)
		putchar(c);
}



void
cmn_err(severity, fmt, va_alist)
int severity;
char *fmt;
va_dcl
{
	va_list ap;

	va_start(ap);
	switch (severity)
	{
		case CE_CONT:
			printf(fmt,ap);
			break;
		case CE_NOTE:
			printf("\nNOTICE: ");
			printf(fmt,ap);
			printf("\n");
			break;
		case CE_WARN:
			printf("\nWARNING: ");
			printf(fmt,ap);
			printf("\n");
			break;
		case CE_PANIC:
			printf("\nPANIC: ");
			printf(fmt,ap);
			printf("\n");
			while (1) ;
	}
}
#endif !DIAG_MONITOR

/*
 * Scaled down version of C Library printf.
 *
 * One additional format: %b is supported to decode error registers.
 * Usage is:
 *	printf("reg=%b\n", regval, "<base><arg>*");
 * Where <base> is the output base expressed as a control character,
 * e.g. \10 gives octal; \20 gives hex.  Each arg is a sequence of
 * characters, the first of which gives the bit number to be inspected
 * (origin 1), and the next characters (up to a control character, i.e.
 * a character <= 32), give the name of the register.  Thus
 *	printf("reg=%b\n", 3, "\10\2BITTWO\1BITONE\n");
 * would produce output:
 *	reg=3<BITTWO,BITONE>
 *
 * See comments in saioctl.h regarding %r and %R formats
 */
void printf(fmt, va_alist)

char *fmt;
va_dcl

{
	va_list ap;

	va_start(ap);
	prf(0, fmt, ap);
	va_end(ap);
}


void sprintf(buf, fmt, va_alist)

char *buf, *fmt;
va_dcl

{
	va_list ap;

	va_start(ap);
	prf(buf, fmt, ap);
	va_end(ap);
}


static void prf(buf, fmt, adx)

register char *buf, *fmt;
va_list adx;

{
	int b, c, i;
	char *s;
	int any;

loop:
	while ((c = *fmt++) != '%') {
		if (c == '\0')
			return;
		PUTCHAR(c);
	}

	numpad = zeropad = firsttime = 0;	/* No justify, no pad */
again:
	c = *fmt++;
	switch (c) {

	case 'x':
		b = 16;
		goto number;

	case 'd':
		b = -10;
		goto number;

	case 'u':
		b = 10;
		goto number;

	case 'o':
		b = 8;

	case '0':
		if (!firsttime) {
			firsttime = 1;
			zeropad = 1;
			goto again;
		}

	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		numpad = (numpad * 10) + (c - '0');
		goto again;

number:
		buf = printn(va_arg(adx, int), b, buf);
		break;

	case 'c':
		b = va_arg(adx, int);
		PUTCHAR(b & 0x7f);
		break;

	case 'b':
		b = va_arg(adx, int);
		s = va_arg(adx, char *);
		buf = printn((u_int)b, *s++, buf);
		any = 0;
		if (b) {
			PUTCHAR('<');
			while (i = *s++) {
				if (b & (1 << (i - 1))) {
					if (any)
						PUTCHAR(',');

					any = 1;
					for (; (c = *s) > 32; s++)
						PUTCHAR(c);
				} else
					for (; *s > 32; s++)
						;
			}

			PUTCHAR('>');
		}
		break;

	case 'r':
	case 'R':
		b = va_arg(adx, int);
		s = va_arg(adx, char *);
		if (c == 'R') {
			PUTS("0x");
			buf = printn(b, 16, buf);
		}

		any = 0;
		if (c == 'r' || b) {
			register struct reg_desc *rd;
			register struct reg_values *rv;
			register u_int field;

			PUTCHAR('<');
			for (rd = (struct reg_desc *)s; rd->rd_mask; rd++) {
				field = b & rd->rd_mask;
				field = (rd->rd_shift > 0) ? field << rd->rd_shift : field >> -rd->rd_shift;
				if (any && (rd->rd_format || rd->rd_values || (rd->rd_name && field)))
					PUTCHAR(',');

				if (rd->rd_name) {
					if (rd->rd_format || rd->rd_values || field) {
						PUTS(rd->rd_name);
						any = 1;
					}

					if (rd->rd_format || rd->rd_values) {
						PUTCHAR('=');
						any = 1;
					}
				}

				if (rd->rd_format) {
					if (buf) {
						sprintf(buf, rd->rd_format, field);
						buf = &buf[strlen(buf)];
					} else
						printf(rd->rd_format, field);

					any = 1;
					if (rd->rd_values)
						PUTCHAR(':');
				}

				if (rd->rd_values) {
					any = 1;
					for (rv = rd->rd_values; rv->rv_name; rv++) {
						if (field == rv->rv_value) {
							PUTS(rv->rv_name);
							break;
						}
					}

					if (rv->rv_name == NULL)
						PUTS("???");
				}
			}

			PUTCHAR('>');
		}
		break;

	case 's':
		s = va_arg(adx, char *);
		PUTS(s);
		break;

	case '%':
		PUTCHAR('%');
		break;
	}
	goto loop;
}


/*
 * Printn prints a number n in base b.
 */
static char *printn(n, b, buf)

register u_int n;
register int b;
register char *buf;

{
	char prbuf[11];
	register char *cp;
	register int cnt;

	if (b < 0) {
		b = -b;
		if ((int)n < 0) {
			PUTCHAR('-');
			n = (u_int)(-(int)n);
		}
	}

	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
	} while (n);

	if ((zeropad || numpad) && ((cp - prbuf) < numpad)) {
		cnt = numpad - (cp - prbuf);
		while (cnt--)
			if (zeropad)
				*cp++ = '0';
			else
				*cp++ = ' ';
	}

	do
		PUTCHAR(*--cp);
	while (cp > prbuf);
	return(buf);
}


#ifndef DIAG_MONITOR
/*
 * Print character in visible manner.
 */
void showchar(c)

int c;

{
	c &= 0xff;
	if (isprint(c))
		putchar(c);
	else
		switch (c) {
		case '\b':
			puts("\\b");
			break;

		case '\f':
			puts("\\f");
			break;

		case '\n':
			puts("\\n");
			break;

		case '\r':
			puts("\\r");
			break;

		case '\t':
			puts("\\t");
			break;

		default:
			putchar('\\');
			putchar(((c & 0300) >> 6) + '0');
			putchar(((c & 070) >> 3) + '0');
			putchar((c & 07) + '0');
			break;
		}
}
#endif !DIAG_MONITOR
