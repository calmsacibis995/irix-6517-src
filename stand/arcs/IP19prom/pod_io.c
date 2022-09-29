#include <stdarg.h>
#include <biendian.h>
#include <sys/types.h>
#include <sys/cpu.h>
#include "ip19prom.h"
#include "pod.h"

void loprf(void (*putc)(char), char *, va_list);
void lo_sprintn(char *, unsigned, int, int);
int *logets(int *, int);
void loprintf(char *, ...);
void loputs(void (*putc)(char), char *, int);
void loputchar(char);

/*VARARGS1*/
void loprintf(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	loprf(loputchar, fmt, ap);
	va_end(ap);
}

/*VARARGS1*/
void ccloprintf(char *fmt, ...)
{
	va_list ap;

	if (!(get_BSR() & BS_MANUMODE))
		return;

	va_start(ap, fmt);
	loprf(loputchar, fmt, ap);
	va_end(ap);
}


lostrncpy(char *dst, char *src, int n)
{
	int i;

	for (i = 0; (i < n) && (*src != '\0'); i++)
		*dst++ = get_char(src++);
	*dst = '\0';
}

void loprf(void (*putc)(char), char *fmt, va_list adx)
{
	char c;
	char *s;
	int *p;
	int i;
	char prbuf[32];
	FLIPINIT(dosw, fmt);

	for (;;) {
		while ((c = EVCFLIP(dosw, fmt++)) != '%') {
			if(c == '\0')
				return;
			(*putc)(c);
		}
		c = EVCFLIP(dosw, fmt++);
		switch (c) {
		case 'd':
			/* Print a variable-length decimal number */
			lo_sprintn(prbuf, va_arg(adx, int), 10, 0);
			loputs(putc, prbuf, 0);
			break;
		case 'a':
			/* Print a two digit decimal number */
			lo_sprintn(prbuf, va_arg(adx, int), 10, 2);
			loputs(putc, prbuf, 0);
			break;
		case 'h':
			/* Print a hex halfword */
			lo_sprintn(prbuf, va_arg(adx, int), 16, 4);
			loputs(putc, prbuf, 0);
			break;
		case 'b':
			/* Print a hex byte. */
			lo_sprintn(prbuf, va_arg(adx, int), 16, 2);
			loputs(putc, prbuf, 0);
			break;
		case 'x':
			/* Print a variable-length hex number */
			lo_sprintn(prbuf, va_arg(adx, int), 16, 8);
			loputs(putc, prbuf, 0);
			break;
		case 'y':
			/* Print a hex doubleword */
			lo_sprintn(prbuf, va_arg(adx, int), 16, 8);
			loputs(putc, prbuf, 0);
			lo_sprintn(prbuf, va_arg(adx, int), 16, 8);
			loputs(putc, prbuf, 0);
			break;
		case 's':
			/* Print a string */
			s = (char *)va_arg(adx, char *);
			loputs(putc, s, getendian() && INBOOTPROM(s));
			break;
		case 'c':
			/* print a character */
			i = va_arg(adx, int);
				(*putc)(i);
			break;	
		case 'p':
			/* print a "prom string" (in an int array) */
			p = (int *)va_arg(adx, int *);
			while (*p != 0)
				(*putc)(*p++);
			break;	
		default:
			break;
	
		}
	}
}

void
loputs(void (*putc)(char), char *sp, int dosw)
{
	if (sp == NULL) {
		loputs(putc, "<NULL>", getendian());
		return;
	}
	while (EVCFLIP(dosw, sp))
		(*putc)(EVCFLIP(dosw, sp++));
}

/*
 * lo_sprintn -- output a number n in base b.
 */

void lo_sprintn(char *tgt, unsigned n, int b, int digits)
{
	int prbuf[18];
	register int *cp;
	int i = 0;
	int j;

	cp = prbuf;
	do {
		if (getendian())	/* 1 = little */
			*cp++ = get_char("32107654ba98fedc" + n%b);
		else
			*cp++ = get_char("0123456789abcdef" + n%b);
		n /= b;
		if (digits)
			digits--;
	} while (n || digits);
	do {
		*tgt++ = (*--cp);
	} while (cp > prbuf);
	*tgt++ = '\0';
}


void
loputchar(char c)
{
	int column = 0;
	switch (c) {
	case '\n':
		loputchar('\r');
		break;

	default:
		if (isprint(c))
			column++;
		break;
	}
	pod_putc(c);		/* attempt to print something */
}


/*
 * gets -- get an input line from user
 * handles basic line editing
 * buf assumed to be LINESIZE char, read into internal array of int
 * then convert them into char. Since using cache for stack.
 */

int *
logets(int *buf, int max_length)
{
	char c;
	int *bufp;

	bufp = buf;
	for (;;) {
		c = pod_getc();

		switch (c) {

		case '\n':
		case '\r':
			loputchar('\n');
			*bufp = 0;
			return(buf);

		case CTRL('H'):
		case DEL:
			/*
			 * Change this to a hardcopy erase????
			 */
			if (bufp > buf) {
				bufp--;
				loputchar(CTRL('H'));
				loputchar(' ');
				loputchar(CTRL('H'));
			}
			break;

		case CTRL('U'):
			if (bufp > buf) {
				loprintf("^U\n? ");
				bufp = buf;
			}
			break;

		case '\t':
			c = ' ';	/* simplifies CTRL(H) dramatically */
			/* fall through to default case */

		default:
			/*
			 * Make sure there's room for this character
			 * plus a trailing \n and 0 byte
			 */
			if (bufp >= buf + max_length - 1) {
				loprintf("\n*** POD: Line > %d chars.\n", max_length);
				/* Don't return a partial line to the
					parser. */
				*buf = 0;
				return(buf);
			}
			*bufp++ = c;
			loputchar(c);
			break;
		}
	}
}


loyn(char *prompt)
{
	int answer = 0;

	loprintf("%s", prompt);
	if (pod_getc() == 'y')
		answer = 1;
	loprintf("\r\n");
	return answer;
}

