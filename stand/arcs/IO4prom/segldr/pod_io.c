#include <stdarg.h>
#include <sys/types.h>
#include <sys/cpu.h>
#include "sl.h"

/*VARARGS1*/
void loprintf(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	loprf(loputchar, fmt, ap);
	va_end(ap);
}

void lostrncpy(unsigned char *dst, unsigned char *src, int n)
{
	int i;

	for (i = 0; (i < n) && (*src != '\0'); i++)
		*dst++ = *src++;
	*dst = '\0';
}

void loprf(void (*putc)(char), char *fmt, va_list adx)
{
	char c;
	char *s;
	char *p;
	__scint_t i;
	char prbuf[32];

	for (;;) {
		while ((c = *fmt++) != '%') {
			if(c == '\0')
				return;
			(*putc)(c);
		}

		/*
		 * Ignore 'precision' for compatibility with 'dk'.
		 */
		do {
			c = *fmt++;
		} while (isdigit(c));

		switch (c) {
		case 'd':	/* Print a variable-length decimal number */
			lo_sprintn(prbuf, va_arg(adx, __scunsigned_t), 10, 0);
			loputs(putc, prbuf);
			break;
		case 'a':	/* Print a two digit decimal number */
			lo_sprintn(prbuf, va_arg(adx, __scunsigned_t), 10, 2);
			loputs(putc, prbuf);
			break;
		case 'h':	/* Print a hex halfword */
			lo_sprintn(prbuf, va_arg(adx, __scunsigned_t), 16, 4);
			loputs(putc, prbuf);
			break;
		case 'b':	/* Print a hex byte. */
			lo_sprintn(prbuf, va_arg(adx, __scunsigned_t), 16, 2);
			loputs(putc, prbuf);
			break;
		case 'x':	/* Print a variable-length hex number */
			lo_sprintn(prbuf, va_arg(adx, __scunsigned_t), 16, 8);
			loputs(putc, prbuf);
			break;
		case 'y':	/* Print a hex doubleword */
			lo_sprintn(prbuf, va_arg(adx, __scunsigned_t), 16, 16);
			loputs(putc, prbuf);
			break;
		case 's':	/* Print a string */
			s = (char *)va_arg(adx, char *);
			loputs(putc, s);
			break;
		case 'c':	/* print a character */
			i = va_arg(adx, __scint_t);
				(*putc)((char)i);
			break;	
		case 'p':	/* print a "prom string" (in an int array) */
			p = va_arg(adx, char *);
			while (*p != 0)
				(*putc)(*p++);
			break;	
		default:
			break;
	
		}
	}
}

void
loputs(void (*putc)(char), char *sp)
{
	if (sp == NULL) {
		loputs(putc, "<NULL>");
		return;
	}
	while (*sp)
		(*putc)(*sp++);
}

/*
 * lo_sprintn -- output a number n in base b.
 */

void lo_sprintn(char *tgt, __scunsigned_t n, int b, int digits)
{
	char prbuf[20];
	register char *cp;

	cp = prbuf;
	do {
		*cp++ = *("0123456789abcdef" + n%b);
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
	lo_putc(c);		/* attempt to print something */
}


/*
 * gets -- get an input line from user
 * handles basic line editing
 * buf assumed to be LINESIZE char, read into internal array of int
 * then convert them into char. Since using cache for stack.
 */

char *
logets(char *buf, int max_length)
{
	char c;
	char *bufp;

	bufp = buf;
	for (;;) {
		c = lo_getc();

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
	/*NOTREACHED*/
	return (char *)0;	/* to keep the compiler happy */
}

int lo_ishex(char c)
{
        if ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'F') ||
            (c >= 'a' && c <= 'f')) {
                return 1;
        }
        else
                return 0;
}


  
ulong lo_atoh(char *cp)
{
        register ulong i;

        /* Ignore leading 0x or 0X */
        if (*cp == '0' && (*(cp+1) == 'x' || *(cp+1) == 'X'))
                cp += 2;

        for (i = 0 ; lo_ishex(*cp) ;)
                if (*cp <= '9')
                        i = i*16 + *cp++ - '0';
                else {
                        if (*cp >= 'a')
                                i = i * 16 + *cp++ - 'a' + 10;
                        else
                                i = i * 16 + *cp++ - 'A' + 10;
                }
        return i;
}



