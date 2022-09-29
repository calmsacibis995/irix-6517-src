#ident        "lib/libsc/lib/stdio.c:  $Revision: 1.65 $"

/*
 * stdio.c -- standalone stdio routines
 */


#include <sys/types.h>
#include <sys/systm.h>
#include <ctype.h>
#include <stdarg.h>
#include <arcs/io.h>
#include <alocks.h>
#include <libsc.h>

/* For edit_gets() */
#include <arcs/spb.h>
#include <parser.h>
#include <saioctl.h>

#define CTRL(x)		((x)&0x1f)
#define	DEL		0x7f
#define	INTR		CTRL('C')
#define	BELL		0x7
#ifndef LINESIZE		/* parser.h defines this larger */
#define	LINESIZE	128
#endif
#define	PROMPT		"? "
#define	SCALABLE	'I'

extern void _errputc(char);
extern int  _prom;
static int  _Setscalable=0;

void showchar(int);
void sprintn(char *, __scunsigned_t, int);
void sprintL(char *, __uint64_t, int);

static void sputs(void (*)(int), char *, int, int, char);
static void putstr(void (*)(int), const char *);
static void prf(void (*)(int), const char *, ...);
static void vprf(void (*)(int), const char *, va_list);

int stdio_init;

/*
 * pointers used for buffering
 */
#define fputbp		(LIBSC_PRIVATE(fputbp))
#define fputbb		(LIBSC_PRIVATE(fputbb))
#define fputbs		(LIBSC_PRIVATE(fputbs))
#define fputcn		(LIBSC_PRIVATE(fputcn))
#define column		(LIBSC_PRIVATE(column))
#define sprintf_cp	(LIBSC_PRIVATE(sprintf_cp))

/*
 * libsc is generic, so must protect against worst-case: mp competition
 * for a single duart.  Lock it even for SP machines.
 * Note that initializing the locks (actually ptrs to salock_t structs)
 * to zero to prevent premature use by locking macros is safe ONLY if
 * the locks are initialized before launching any children.
 */
int arcs_ui_spl;

/*
 * getchar -- get a single character from enabled console devices
 * blocks until character available, returns 7 bit ascii code
 */
int
getchar(void)
{
	ULONG cnt;
	char c;

#if DEBUG
	{ extern Debug; if (Debug) printf ("getchar()\n"); }
#endif
	if(Read(0, &c, 1, &cnt) || (cnt != 1))
		return EOF;
	return c;
}

/*
 * getc -- get character only from particular device
 * blocks until character available, returns 8 bits
 */
int
getc(ULONG fd)
{
	ULONG cnt;
	char c;

	if(Read(fd, &c, 1, &cnt) || (cnt != 1))
		return(EOF);
	return c;
}


/* like stdio fgets, but fd instead of FILE ptr, and has all
 * the echo'ing, etc. that is needed for standalone.
 */
char *
fgets(char *buf, int len, ULONG fd)
{
	int c;
	char *bufp, *rvalp=NULL;

	bufp = buf;

	for (;;) {
		c = getc(fd);

		switch (c) {

		case CTRL('V'):		/* quote next char */
			c = getc(fd);
			/*
			 * Make sure there's room for this character
			 * plus a trailing \n and 0 byte
			 */
			if (bufp < &buf[len-3]) {
				*bufp++ = (char)c;
				showchar(c);
			} else
				putchar(BELL);
			break;

		case CTRL('D'):		/* handle ^D for eof */
		case EOF:
			if(bufp == buf) {
				buf[0] = '\0';
				rvalp = NULL;
				break;
			}
			/* else fall through */

		case '\n':
		case '\r':
			putchar('\n');
			*bufp = 0;
			rvalp = buf;
			break;

		case CTRL('H'):
		case DEL:
			/*
			 * Change this to a hardcopy erase????
			 */
			if (bufp > buf) {
				if (*(bufp-1) == '\t') {
					bufp--;
					goto ctrl_R;
				}
				*--bufp = 0;
				putchar(CTRL('H'));
				putchar(' ');
				putchar(CTRL('H'));
			}
			break;

		case CTRL('R'):
ctrl_R:
			*bufp = '\0';
			printf("^R\n%s%s",PROMPT, buf);
			break;

		case CTRL('U'):
			if (bufp > buf) {
				printf("^U\n%s", PROMPT);
				bufp = buf;
			}
			break;

		case '\t':
			*bufp++ = (char)c;
			putchar(c);
			break;

		default:
			/*
			 * Make sure there's room for this character
			 * plus a trailing \n and 0 byte
			 */
			if (isprint(c) && bufp < &buf[len-3]) {
				*bufp++ = (char)c;
				putchar(c);
			} else
				putchar(BELL);
			break;
		}
		if (rvalp)
		    break;
	}

	return(rvalp);
}

/* like stdio fgets, but fd instead of FILE ptr, and has all
 * the echo'ing, etc. that is needed for standalone.
 */
char *
edit_gets(struct edit_linebuf *elinebuf)
{
	char *buf = elinebuf->lines[elinebuf->current];
	char *bufp, *bufc, *tmpp, *rvalp=NULL;
	int len = LINESIZE;
	int current = elinebuf->current;
	extern int symmon;
	ULONG fd = 0;
	int tmp;
	int c;

	bufp = bufc = buf;

	/* Want to get ^D straight through, but then have to handle
	 * this and ^A (for ide.DBG) like libsk.
	 */
	ioctl(fd,TIOCRAW,1);

	for (;;) {
		c = getc(fd);

		switch (c) {
		case CTRL('N'):		/* move down */
			if (current != elinebuf->current) {
				tmp = (current + 1) % EDIT_HISTORY;
				if (elinebuf->lines[tmp][0] != '\0' ||
				    tmp == elinebuf->current) {
					current = tmp;
					goto newbuffer;
				}
			}
			putchar(BELL);
			break;

		case CTRL('P'):		/* move up */
			tmp = current - 1;
			if (tmp < 0) tmp = EDIT_HISTORY - 1;
			if (tmp != elinebuf->current) {
				if (elinebuf->lines[tmp][0] != '\0') {
					current = tmp;
					goto newbuffer;
				}
			}
			putchar(BELL);
			break;

		case CTRL('V'):		/* quote next char */
			c = getc(fd);
			/*
			 * Make sure there's room for this character
			 * plus a trailing \n and 0 byte
			 */
			/* XXX editing over this does not work */
			if (bufp < &buf[len-3]) {
				*bufp++ = (char)c;
				bufc++;
				showchar(c);
			} else
				putchar(BELL);
			break;

		/* ^D in _ttyinput() goes straight back to the prom.  We
		 * have to maintain that if it's the only thing on the line
		 * as well as delteing in line.
		 */
		case CTRL('D'):		/* handle ^D for eof */
		case CTRL('X'):		/* if ioctl does not work */
		case EOF:
			if (bufc != bufp) {
				bcopy(bufc+1,bufc,bufp-bufc);
				*--bufp = '\0';
				printf("%s ",bufc);
				tmpp = bufp;
				while (tmpp >= bufc) {
					putchar(CTRL('H'));
					tmpp--;
				}
			}
			else if (bufp == buf)
				EnterInteractiveMode();
			break;

		case '\n':
		case '\r':
			putchar('\n');
			*bufp = '\0';
			rvalp = buf;
			break;

		case CTRL('H'):
		case DEL:
			if (bufc > buf) {
				if (bufc == bufp) {
					*--bufp = 0;
					bufc--;
					putchar(CTRL('H'));
					putchar(' ');
					putchar(CTRL('H'));
				}
				else {
					putchar(CTRL('H'));
					bcopy(bufc,bufc-1,bufp-bufc);
					bufc--;
					*--bufp = '\0';
					printf("%s ",bufc);
					tmpp = bufp;
					while (tmpp >= bufc) {
						putchar(CTRL('H'));
						tmpp--;
					}
				}
			}
			break;

		case CTRL('E'):			/* end of line */
			while (bufc < bufp) {	/* move to eol */
				putchar(*bufc);
				bufc++;
			}
			break;

		case CTRL('A'):			/* beginning of line */
			/* drop into debugger if it's set-up. */
			if (SPB->DebugBlock && !symmon) {
				debug("ring");
				continue;
			}
			while (bufc > buf) {
				putchar(CTRL('H'));
				bufc--;
			}
			break;

		case CTRL('F'):		/* move forward */
			if (bufc < bufp) {
				putchar(*bufc);
				bufc++;
			}
			break;

		case CTRL('B'):		/* move back */
			if (bufc > buf) {
				bufc--;
				putchar(CTRL('H'));
			}
			break;

		case CTRL('K'):		/* kill to eol */
			if (bufc != bufp) {
				tmpp = bufc;
				while (tmpp < bufp) {	/* move to eol */
					putchar(' ');
					tmpp++;
				}
				while (bufc < bufp) {
					putchar(CTRL('H'));
					*bufp = '\0';
					bufp--;
				}
			}
			break;

		case CTRL('R'):
			if ((bufp-buf) > 65) {
				/* handle wrapped lines the old way */
				printf("^R\n%s",PROMPT);
			}
			else {
				while (bufc < bufp) {	/* move to eol */
					putchar(*bufc);
					bufc++;
				}
				bufc = bufp;
				while (bufc > buf) {
					putchar(CTRL('H'));
					putchar(' ');
					putchar(CTRL('H'));
					bufc--;
				}
			}
			*bufp = '\0';
			bufc = bufp;
			printf("%s",buf);
			break;

		case CTRL('U'):
			while (bufc < bufp) {		/* move to eol */
				putchar(*bufc);
				bufc++;
			}
			while (bufp-- > buf) {
				putchar(CTRL('H'));
				putchar(' ');
				putchar(CTRL('H'));
			}
			bufp = bufc = buf;
			break;

		/* It's hard to manage tabs, so just a space.  It looks
		 * like the libsk code does not always do the right
		 * thing if we try to manage tab stops at 8 with it.
		 * XXX - we should always print as spaces?
		 */
		case '\t':
			c = ' ';
			/*FALLSTHROUGH*/

		default:
			/*
			 * Make sure there's room for this character
			 * plus a trailing \n and 0 byte
			 */
			if (isprint(c) && bufp < &buf[len-3]) {
				putchar(c);

				if (bufc != bufp)
					bcopy(bufc,bufc+1,bufp-bufc);

				*bufc++ = (char)c;
				bufp++;

				if (bufc != bufp) {
					*bufp = '\0';
					printf("%s",bufc);
					tmpp = bufp;
					while (tmpp > bufc) {
						putchar(CTRL('H'));
						tmpp--;
					}
				}
			} else
				putchar(BELL);
			break;
newbuffer:
			while (bufc < bufp) {		/* move to eol */
				putchar(*bufc);
				bufc++;
			}
			while (bufp-- > buf) {
				putchar(CTRL('H'));
				putchar(' ');
				putchar(CTRL('H'));
			}
			buf = elinebuf->lines[current];
			bufp = buf + strlen(buf);
			*bufp = '\0';
			bufc = bufp;
			printf("%s",buf);
			break;
		}

		if (rvalp)
			break;
	}

	ioctl(fd,TIOCRAW,0);

	/* csh style 'bang' history */
	if (rvalp && rvalp[0] == '!') {

		tmp = elinebuf->current;

		if (rvalp[1] == '!' && rvalp[2] == '\0') {	/* !! */
			if (--tmp < 0) tmp = EDIT_HISTORY - 1;
		}

		if (tmp != elinebuf->current && 
		    elinebuf->lines[tmp][0] != '\0') {
			/* echo revised command */
			current = tmp;
			printf("%s\n",elinebuf->lines[current]);
		}
	}

	/* do not remember empty commands */
	if (rvalp && *rvalp != '\0') {
		/* update current with history command */
		if (current != elinebuf->current) {
			bcopy(elinebuf->lines[current],
			      elinebuf->lines[elinebuf->current],LINESIZE);
		}
		elinebuf->current = (elinebuf->current + 1) % EDIT_HISTORY;
		elinebuf->lines[elinebuf->current][0] = '\0';
	}

	return(rvalp);
}

/* assume max len of LINESIZE, which is correct for almost all callers */
char *
gets(char *buf)
{
	return fgets(buf, LINESIZE, 0);
}

/*
 * putchar -- put a single character to enabled console devices
 * handles simple character mappings
 */
int
putchar(int ci)
{
	/* column --> LIBSC_PRIVATE(column) */

	int *col = &column;
	char c = ci;
	ULONG cnt;

	switch (c) {
	case '\n':
		if (putchar('\r') != '\r')
			return EOF;
		*col = 0;
		break;

	case '\t':
		do {
			if (putchar(' ') != ' ')
				return EOF;
		} while (*col % 8);
		return c;

	default:
		if (isprint(c))
			(*col)++;
		break;
	}
	if (_prom && !stdio_init)
		_errputc(c);
	else
	if (Write(1, &c, 1, &cnt) || cnt != 1) {
		_errputc(c);		/* attempt to print something */
		return EOF;
	}
	return c;
}

static int
fputflush(void)
{
	ULONG cnt;
	int rc;

	if ( fputbp == fputbb )
		return 0;

	if (Write(1, fputbb, (ULONG)(fputbp-fputbb), &cnt) ||
	    (cnt != (ULONG)(fputbp-fputbb))) {
		char *cp = fputbb;
		rc = EOF;
		while (cp != fputbp)
			_errputc(*cp++);	/* attempt to print something */
	} else {
		fputcn += cnt;
		rc = 0;
	}
	fputbp = fputbb;

	return rc;
}

static void
fputchar(int c)
{
	int needflush = 0;

	int *col = &column;

	if ( _prom && !stdio_init ) {
		putchar(c);
		return;
	}

	switch (c) {
		case '\n':
			*fputbp++ = '\n';
			*fputbp++ = '\r';
			*col = 0;
			needflush = 1;
			break;

		case '\t':
			do {
				*fputbp++ = ' ';
			} while ((*col)++ % 8);
			break;

		default:
			if (isprint(c))
				(*col)++;
			*fputbp++ = (char)c;
			break;
	}

	if ( fputbp >= fputbb+fputbs-10 || needflush )
		(void)fputflush();
}

#if 0		/* currently unused */
/*
 * putc -- put a character only to a single device
 */
int
putc(int c, int fd)
{
	ULONG cnt;

	if (Write(fd, &c, 1, &cnt) || cnt != 1)
		return EOF;

	return c;
}
#endif

/*
 * puts should add a newline
 */
int
puts(const char *s)
{
	char sbuf[LINESIZE];

	fputbs = sizeof(sbuf);
	fputbb = fputbp = sbuf;
	putstr(fputchar, s);

	return fputflush() == EOF ? EOF : 0;
}

static void
putstr(void (*putcf)(int), const char *s)
{
	register int c;

	if (s == NULL)
		s = "<NULL>";
	while (c = *s++)
		(*putcf)(c);
}


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
 * See comments in libsc.h regarding %r and %R formats
 */
/*VARARGS1*/
int
printf(const char *fmt, ...)
{
	char sbuf[LINESIZE];
	va_list ap;

	fputbs = sizeof(sbuf);
	fputbb = fputbp = sbuf;
	fputcn = 0;

	/*CONSTCOND*/
	va_start(ap,fmt);
	vprf(fputchar, fmt, ap);
	va_end(ap);

	return fputflush() == EOF ? EOF : fputcn;
}

/*
 * sa version of libc's vprintf
 *
 * just like printf, except it takes exactly two arguments: the format
 * string and an argument vector (i.e., ap is really va_list).
 *
 */
int
vprintf(const char *fmt, char *ap)
{
	char sbuf[LINESIZE];

	fputbs = sizeof(sbuf);
	fputbb = fputbp = sbuf;
	fputcn = 0;
	vprf(fputchar, fmt, ap);

	return fputflush() == EOF ? EOF : fputcn;
}

/*
 * 'putcf' function for sprintf
 */
static void
poke(int c)
{
	/* sprintf_cp --> LIBSC_PRIVATE(sprintf_cp) */

	*sprintf_cp++ = (char)c;
}

/*VARARGS2*/
int
sprintf(char *buf, const char *fmt, ...)
{
	va_list ap;

	/*CONSTCOND*/
	va_start(ap,fmt);
	sprintf_cp = buf;
	vprf(poke, fmt, ap);
	poke('\0');	/* make sure result string is null terminated */
	va_end(ap);

	return (int)strlen(buf);
}

/* ttyprintf() is like printf, but aways goes to the tty port (if ttyputc
 * has been set).
 */
static void (*ttyputc)();
void _init_ttyputc(void (*func)()) { ttyputc = func; }
/*VARARGS1*/
int
ttyprintf(const char *fmt, ...)
{
	char sbuf[LINESIZE];
	va_list ap;

	fputbs = sizeof(sbuf);
	fputbb = fputbp = sbuf;
	fputcn = 0;

	/*CONSTCOND*/
	va_start(ap,fmt);
	vprf(ttyputc ? ttyputc : fputchar, fmt, ap);
	va_end(ap);

	return fputflush() == EOF ? EOF : fputcn;
}

/*
 * vttyprintf() is just like printf, except it takes exactly two arguments: 
 * the format string and an argument vector (i.e., ap is really va_list).
 */
int
vttyprintf(const char *fmt, char *ap)
{
	char sbuf[LINESIZE];

	fputbs = sizeof(sbuf);
	fputbb = fputbp = sbuf;
	fputcn = 0;

	vprf(ttyputc ? ttyputc : fputchar, fmt, ap);

	return fputflush() == EOF ? EOF : fputcn;
}

static void
prf(void (*putcf)(int), const char *fmt, ...)
{
	va_list ap;

	va_start(ap,fmt);
	vprf(putcf, fmt, ap);
	va_end(ap);
}

static void
vprf(void (*putcf)(int), const char *fmt, va_list adx)
{
	__scint_t b;
	int c, i;
	char *s;
	int any;
	char prbuf[65];	/* paranoia: large enough for base 2 numbers */
	int fw, len, ljust; char fillc;
	int altform;
	int Iflag, llflag;
	
loop:
	LOCK_ARCS_UI();
	while ((c = *fmt++) != '%') {
		if(c == '\0') {
			goto unlockit;
		}
		(*putcf)(c);
	}

	fillc = ' ';
	fw = 0;
	len = 0;
	llflag = 0;
	Iflag = 0;
	c = *fmt++;
	if (altform = c=='#')
		c = *fmt++;
	if (ljust = c=='-')
		c = *fmt++;
	if (isdigit(c)) {
		i = 0;
		if (c == '0')
			fillc = (char)c;
		while (isdigit(c)) {
			i = i * 10 + c - '0';
			c = *fmt++;
		}
		fw = i;
	}
	else if(c == '*') {	/* precision from arg */
		/*CONSTCOND*/
		fw = va_arg(adx, int);
		c = *fmt++;
	}
	if (c == '.') {
		i = 0;
		c = *fmt++;
		if(c == '*') {	/* precision from arg */
			/*CONSTCOND*/
			i = va_arg(adx, int);
			c = *fmt++;
		}
		else while (isdigit(c)) {
			i = i * 10 + c - '0';
			c = *fmt++;
		}
		len = i;
	}
	if (ljust)
		fw = -fw;

	if (c == 'l') { 
		if ((c = *fmt++) == 'l') {
			llflag = 1;
			c = *fmt++;
		}
		/* %l? prints Long whatever its size is */
	} else if (c == 'w') {
		if (*fmt == '3' && *(fmt + 1) == '2') {
			Iflag = 1;
			fmt += 2;
			c = *fmt++;
		}
	} else if (c == SCALABLE) {
	    c = *fmt++; 
	    if (_Setscalable)
		llflag = 1;
	} else if (c == 'L') { /* Support %Lx also */
		c = *fmt++;
		llflag = 1;
	}

	switch (c) {

	case 'p':/* pointer - same as 'long' which is always %x */
	case 'x':
	case 'X':
		b = 16;
		if ( altform ) {
			(*putcf)('0');
			(*putcf)('x');
		}
		goto number;

	case 'd':
		b = -10;
		goto number;

	case 'u':
		b = 10;
		goto number;

	case 'o':
		b = 8;
		if ( altform )
			(*putcf)('0');
number:
		/*
		 * Note that unlike in libc (but similar to kernel)
		 * standard numbers print as if they were all 'long'
		 * i.e. in 32 bit mode %x prints a 32 bit quantity
		 * but in 64 bit mode it prints a 64 bit quantity!
		 * So, sprintn is the same as sprintL for 64 bit compiles
		 */
		if (llflag)
			sprintL(prbuf, va_arg(adx, __uint64_t), b);
		else if (Iflag)
			sprintn(prbuf, (__scunsigned_t) va_arg(adx, uint_t), b);
		else
			sprintn(prbuf, va_arg(adx, __scunsigned_t), b);

		sputs(putcf, prbuf, fw, len, fillc);
		break;

	case 'c':
		/*CONSTCOND*/
		b = va_arg(adx, int);
		prbuf[0] = b & 0xFF; prbuf[1] = '\0';
		sputs(putcf, prbuf, fw, len, fillc);
		break;

	case 'b':
		/*CONSTCOND*/
		b = va_arg(adx, __scunsigned_t);
		/*CONSTCOND*/
		s = va_arg(adx, char *);
		sprintn(prbuf, (__scunsigned_t)b, *s++);
		putstr(putcf, prbuf);
		any = 0;
		if (b) {
			(*putcf)('<');
			while (i = *s++) {
				if (b & (1 << (i-1))) {
					if (any)
						(*putcf)(',');
					any = 1;
					for (; (c = *s) > 32; s++)
						(*putcf)(c);
				} else
					while (*s > 32)
						s++;
			}
			(*putcf)('>');
		}
		break;

	case 'r':
	case 'R':
		if (llflag)
			/*CONSTCOND*/
			b = va_arg(adx, __uint64_t);
		else
			/*CONSTCOND*/
			b = va_arg(adx, __scunsigned_t);
		/*CONSTCOND*/
		s = va_arg(adx, char *);
		if (c == 'R') {
			putstr(putcf, "0x");
			sprintn(prbuf, b, 16);
			putstr(putcf, prbuf);
		}
		if (c == 'r' || b) {
			register struct reg_desc *rd;
			register struct reg_values *rv;
			__psunsigned_t field;

			(*putcf)('<');
			any = 0;
			for (rd = (struct reg_desc *)s; rd->rd_mask; rd++) {
				field = b & rd->rd_mask;
				field = (rd->rd_shift > 0)
				    ? field << rd->rd_shift
				    : field >> -rd->rd_shift;
				if (any &&
				      (rd->rd_format || rd->rd_values
				         || (rd->rd_name && field)
				      )
				)
					(*putcf)(',');
				if (rd->rd_name) {
					if (rd->rd_format || rd->rd_values
					    || field) {
						putstr(putcf, rd->rd_name);
						any = 1;
					}
					if (rd->rd_format || rd->rd_values) {
						(*putcf)('=');
						any = 1;
					}
				}
				if (rd->rd_format) {
					prf(putcf, rd->rd_format, field);
					any = 1;
					if (rd->rd_values)
						(*putcf)(':');
				}
				if (rd->rd_values) {
					any = 1;
					for (rv = rd->rd_values;
					    rv->rv_name;
					    rv++) {
						if (field == rv->rv_value) {
							putstr(putcf, rv->rv_name);
							break;
						}
					}
					if (rv->rv_name == NULL)
						putstr(putcf, "???");
				}
			}
			(*putcf)('>');
		}
		break;

	case 's':
		/*CONSTCOND*/
		s = va_arg(adx, char *);
		sputs(putcf, s, fw, len, fillc);
		break;

	case '%':
		prbuf[0] = '%'; prbuf[1] = '\0';
		sputs(putcf, prbuf, fw, len, fillc);
		break;
	}
	goto loop;

unlockit:
	/* Release any locks held */
	
	UNLOCK_ARCS_UI();
	return;

} /* prf */

/*
 * sprintL -- output a number n in base b.
 * Works with a 64-bit long long argument.
 */
void
sprintL(char *tgt, __uint64_t n, int b)
{
	char prbuf[65];	/* paranoia: large enough for base 2 numbers */
	register char *cp;

	if (b < 0) {
		b = -b;
		if ((__int64_t)n < 0) {
			*tgt++ = ('-');
			n = (__uint64_t)(-(__int64_t)n);
		}
	}

	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
	} while (n);
	do
		*tgt++ = (*--cp);
	while (cp > prbuf);
	*tgt++ = ('\0');
}

/*
 * sprintn -- output a number n in base b.
 */
/*ARGSUSED*/
void
sprintn(char *tgt, __scunsigned_t n, int b)
{
#if (_MIPS_SZLONG == 64)
	sprintL(tgt, n, b);
#else
	char prbuf[33];	/* paranoia: large enough for base 2 numbers */
	register char *cp;

	if (b < 0) {
		b = -b;
		if ((int)n < 0) {
			*tgt++ = ('-');
			n = (unsigned)(-(int)n);
		}
	}

	cp = prbuf;
	do {
		*cp++ = "0123456789abcdef"[n%b];
		n /= b;
	} while (n);
	do
		*tgt++ = (*--cp);
	while (cp > prbuf);
	*tgt++ = ('\0');
#endif	/* _MIPS_SZLONG == 64 */
}

/*
 * sputs -- output a string with fill
 */
static void
sputs(void (*putcf)(int), char *s, int fw, int len, char fillc)
{
	register int i;

	if (s == NULL)
		s = "<NULL>";
	if (len <= 0)
		i = strlen(s);
	else
		for (i = 0; i < len; i++)
			if (s[i] == '\0')
				break;
	len = i;

	if (fw > 0)
		for (i = fw - len; --i >= 0;)
			(*putcf)(fillc);
	for (i = len; --i >= 0;)
		(*putcf)(*s++);
	if (fw < 0)
		for (i = -fw - len; --i >= 0;)
			(*putcf)(fillc);
	return;
}

/*
 * showchar -- print character in visible manner
 */
void
showchar(int c)
{
	c &= 0xff;
	if (isprint(c))
		putchar(c);
	else switch (c) {
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
		putchar(((c&0300) >> 6) + '0');
		putchar(((c&070) >> 3) + '0');
		putchar((c&07) + '0');
		break;
	}
}


/* N.B. ap is really va_list. */

int
vsprintf(char *buf, const char *fmt, char *ap)
{
	sprintf_cp = buf;
	vprf(poke, fmt, ap);
	poke('\0');	/* make sure result string is null terminated */

	return strlen(sprintf_cp);
}

/*
 * _min -- SIGNED minimum function (C really needs inline functions!
 * macro's screwup when args have side-effects)
 */
int
_min(int a, int b)
{
	return(a < b ? a : b);
}

/*
 * _max -- SIGNED maximum function
 */
int
_max(int a, int b)
{
	return(a > b ? a : b);
}

void
setscalable(int size)
{
    	if (size == sizeof(long long))
		_Setscalable = 1;
    	else
		_Setscalable = 0;
}

int
getscalable(void)
{
	return((_Setscalable) ? sizeof(long long) : sizeof(long));
}

/*
 * memset
 */

void *
memset(void *base, int value, size_t len)
{
    char       *dst     = base;

    if (len >= 8) {
        __uint64_t      rep;

        rep  = (__uint64_t) value & 0xff;
        rep |= rep << 8;
        rep |= rep << 16;
        rep |= rep << 32;

        while (len > 0 && (__psunsigned_t) dst & 7) {
            *dst++ = value;
            len--;
        }

        while (len >= 32) {
            ((__uint64_t *) dst)[0] = rep;
            ((__uint64_t *) dst)[1] = rep;
            ((__uint64_t *) dst)[2] = rep;
            ((__uint64_t *) dst)[3] = rep;

            dst += 32;
            len -= 32;
        }

        while (len >= 8) {
            *(__uint64_t *) dst = rep;
            dst += 8;
            len -= 8;
        }
    }

    while (len--)
        *dst++ = value;

    return base;
}

