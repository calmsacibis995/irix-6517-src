/***********************************************************************\
*	File:		libc.c						*
*									*
*	PROM C Library							*
*									*
*	I/O is multiplexed to a serial device selected through		*
*	a pointer in the boot status register.				*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/cpu.h>

#include "ip27prom.h"
#include "hub.h"
#include "libasm.h"
#include "libc.h"
#include "symbol.h"

#define CTRL(x)			((x) & 0x1f)
#define DEL			0x7f
#define INTR			CTRL('C')
#define BEL			0x7

#define MAXINT			((~(unsigned int) 0) >> 1)

#define DEV			((libc_device_t *) get_libc_device())

/*
 * libc_device
 *
 *   Select I/O device by supplying a pointer to a libc_device_t structure.
 *   The libc_device_t structure should be permanent (in the PROM).
 */

libc_device_t *
libc_device(libc_device_t *dev)
{
    if (dev)
	set_libc_device((__uint64_t) dev);

    return (libc_device_t *) get_libc_device();
}

/*
 * libc_init
 *
 *   Initialize the selected device
 */

void
libc_init(void *init_data)
{
    if (DEV->init)
	DEV->init(init_data);
}

/*
 * poll
 *
 *   Returns 1 if a character is available on the selected device,
 *   0 otherwise.
 */

int
poll(void)
{
    return DEV->poll();
}

/*
 * readc
 *
 *   Returns a character from the selected device.
 *   May only be called if poll indicated a character was pending
 *   on the selected device.
 */

int
readc(void)
{
    return DEV->readc();
}

/*
 * putc
 *
 *   Writes a single character to the selected device when ready.
 *   Returns 0 if successful, -1 if error.
 *
 *   Device routines convert \n to \r\n (can add 256 to quote a plain \n).
 */

int
putc(int c)
{
    return DEV->putc(c);
}

int
putchar(int c)
{
    return DEV->putc(c);
}

/*
 * puts
 *
 *   Writes a string to the selected device.
 *   For some drivers, more efficient than calling putc for each character.
 *   Returns 0 if successful, -1 if error.
 */

int
puts(char *s)
{
    return DEV->puts(s);
}

/*
 * flush
 *
 *   Flushes the input buffer.
 */

int
flush(void)
{
    return DEV->flush ? DEV->flush() : 0;
}

/*
 * kbintr
 *
 *   Returns 1 if control-C has been pressed (and prints ^C).
 *   Only polls a few times a second since it may be very expensive.
 */

int
kbintr(rtc_time_t *next)
{
    rtc_time_t		now	= rtc_time();

    if (now > *next) {
	*next = now + 250000;

	if (poll() && getc() == 3) {
	    printf("^C\n");
	    return -1;
	}
    }

    return 0;
}

/*
 * more
 *
 *   Checks to see if the number of lines has been exceeded, and if so,
 *   prompts for the user to type SPACE for another screenful or ENTER
 *   for one more line.  Returns 0 if the user aborts by pressing q, Q,
 *   or Control-C; otherwise returns 1.  Should be called BEFORE each
 *   line is printed.
 */

int
more(int *lines, int max)
{
    int		j, c;

    if (++(*lines) == max) {
	printf("--More--");
	while ((c = getc()) != '\r' && c != '\n' &&
	       c != 'q' && c != 'Q' && c != ' ' && c != 3)
	    ;
	putc('\r');
	for (j = 0; j < 10; j++)
	    putc(32);
	putc('\r');
	if (c == 'q' || c == 'Q' || c == 3)
	    return 0;
	*lines = (c == '\r' || c == '\n') ? max - 1 : 0;
    }

    return 1;
}

/*
 * device
 *
 *   Returns the device name.
 */

char *
device(void)
{
    return DEV->dev_name;
}

/*
 * getc_timeout (see also: getc)
 *
 *   Waits for a character on the selected device and returns it.
 *   Flashes hub LEDs while waiting.  The flashing pattern
 *   indicates which device is accepting input.  Returns -1 if
 *   the user-supplied timeout expires.
 */

#define BLINK_PERIOD	1000000		/* microseconds */

int
getc_timeout(__uint64_t usec)
{
    libc_device_t	*dev	= DEV;
    int			leds, i;
    rtc_time_t		now;
    rtc_time_t		blink_expire;
    rtc_time_t		user_expire;

    flush();

    blink_expire = rtc_time();
    user_expire	 = usec ? rtc_time() + usec : ~0ULL;

    for (leds = 0x80; ; leds ^= dev->led_pattern) {
	hub_led_set(leds);

	blink_expire += BLINK_PERIOD;

	while ((now = rtc_time()) < blink_expire && now < user_expire)
	    if ((*dev->poll)()) {
		hub_led_set(0xff);
		return (*dev->readc)();
	    }

	if (now >= user_expire)
	    return -1;
    }
}

/*
 * getc
 *
 *   Like getc_timeout, but without a timeout.
 */

int
getc(void)
{
    return getc_timeout(0);
}

/*
 * gets_timeout (see also: gets)
 *
 *   Reads an input line from the current device.
 *   Handles basic line editing.
 *   If timeout expires, uses 'defl' string (and prints it too).
 *   If a blank line is entered and 'defl' is non-empty, uses 'defl'.
 *   Uses empty buffer and returns NULL if user presses ^C.
 */

#ifdef SABLE
#define ECHO(c)		/* Sable does the echoing */
#else
#define ECHO(c)		putc(c)
#endif

char *
gets_timeout(char *buf, int max_length, __uint64_t usec, char *defl)
{
    int			c;
    char	       *bufp;

    if (defl == 0)
	defl = "";

    bufp = buf;

    for (;;) {
	if ((c = getc_timeout(usec)) < 0)
	    break;	/* Timed out */

	usec = 0;	/* Disable time-out once any key is typed */

	switch (c) {

	case CTRL('C'):
	    ECHO('^');
	    ECHO('C');
	    ECHO('\n');

	    buf[0] = 0;
	    return 0;

	case CTRL('H'):
	case DEL:
	    if (bufp > buf) {
		bufp--;
		ECHO('\b');
		ECHO(' ');
		ECHO('\b');
	    } else
		ECHO(BEL);
	    break;

	case CTRL('U'):
	    while (bufp > buf) {
		ECHO('\b');
		ECHO(' ');
		ECHO('\b');
		bufp--;
	    }
	    break;

	case CTRL('R'):
	    ECHO('\n');
	    for (c = 0; c < bufp - buf; c++)
		ECHO(buf[c]);
	    break;

	case '\t':
	    do {
		if (bufp >= buf + max_length - 1) {
		    ECHO(BEL);
		    break;
		}
		*bufp++ = ' ';
		ECHO(' ');
	    } while ((bufp - buf) & 7);
	    break;

	case '\n':
	case '\r':
	    if (bufp > buf && bufp[-1] == '\\') {
		bufp[-1] = '\n';
		ECHO('\n');
		break;
	    }

	    *bufp = 0;

	    if (buf[0] == 0 && LBYTE(defl) != 0)
		goto do_defl;

	    ECHO('\n');

	    return buf;

	default:
	    /*
	     * Make sure there's room for this character plus a
	     * trailing \n and 0 byte, and that it's printable.
	     */

	    if (bufp >= buf + max_length - 1 || c < 32 || c > 126)
		ECHO(BEL);
	    else {
		*bufp++ = c;
		ECHO(c);
	    }
	    break;
	}
    }

 do_defl:

    printf("%s\n", defl);
    strcpy(buf, defl);
    return buf;
}

/*
 * gets
 *
 *   Like gets_timeout, but without a timeout.
 */

char *
gets(char *buf, int max_length)
{
    return gets_timeout(buf, max_length, 0, 0);
}

/*
 * puthex
 *
 *   Writes a 64-bit value as ASCII hexadecimal.
 *   Returns 0 if successful, -1 if putc failed.
 */

int
puthex(__uint64_t v)
{
    int		i, c;

    for (i = 0; i < 16; i++, v <<= 4) {
	c = (int) (v >> 60);
	c += (c < 10) ? '0' : 'a' - 10;

	if (putc(c) < 0)
	    return -1;
    }

    return 0;
}

/*
 * strlen
 */

size_t
strlen(const char *s)
{
    size_t len;

    for (len = 0; LBYTE(s); s++)
	len++;

    return len;
}

/*
 * strcpy
 */

char *
strcpy(char *dst, const char *src)
{
    int		i;

    for (i = 0; ; i++)
	if ((dst[i] = LBYTE(&src[i])) == 0)
	    break;

    return dst;
}

/*
 * strncpy
 */

char *
strncpy(char *dst, const char *src, size_t n)
{
    size_t	i;

    for (i = 0; i < n; i++)
	if ((dst[i] = LBYTE(&src[i])) == 0)
	    break;

    return dst;
}

/*
 * strncmp
 */

int
strncmp(const char *s1, const char *s2, size_t n)
{
    int		c1, c2;

    while (n--) {
	c1 = LBYTE(s1);
	s1++;
	c2 = LBYTE(s2);
	s2++;

	if (c1 == 0 && c2 == 0)
	    break;

	if (c1 < c2)
	    return -1;

	if (c1 > c2)
	    return 1;
    }

    return 0;
}

/*
 * strcmp
 */

int
strcmp(const char *s1, const char *s2)
{
    return strncmp(s1, s2, MAXINT);
}

/*
 * strchr
 */

char *
strchr(const char *s, int c)
{
    int		d;

    for (; (d = LBYTE(s)) != 0; s++)
	if (d == c)
	    return (char *) s;

    return 0;
}

/*
 * strcat
 */

char *strcat(char *dst, const char *src)
{
    int		i;

    for (i = 0; dst[i]; i++)
	;

    strcpy(dst + i, src);

    return dst;
}

/*
 * strstr
 */

char *strstr(const char *s, const char *substr)
{
    int		slen	= strlen(substr);
    char       *se;

    for (se = (char *) s + strlen(s) - slen; s <= se; s++)
	if (strncmp(s, substr, slen) == 0)
	    return (char *) s;

    return 0;
}

/*
 * strrepl
 */

char *strrepl(char *s, int start, int len, const char *repstr)
{
    memcpy(s + start + strlen(repstr),
	   s + start + len,
	   strlen(s + start + len) + 1);

    memcpy(s + start, repstr, strlen(repstr));

    return s;
}

/*
 * memset
 */

void *
memset(void *base, int value, size_t len)
{
    char       *dst	= base;

    if (len >= 8) {
	__uint64_t	rep;

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

/*
 * memcmp8
 *
 *   A version of memcmp that requires 8-byte alignment of pointers and size.
 */

int memcmp8(__uint64_t *s1, __uint64_t *s2, size_t n)
{
    __uint64_t		a1, a2;

    while (n > 7) {
	a1 = *s1++;
	a2 = *s2++;

	if (a1 < a2)
	    return -1;
	else if (a1 > a2)
	    return 1;

	n -= 8;
    }

    return 0;
}

/*
 * bzero
 */

void
bzero(void *base, size_t len)
{
    memset(base, 0, len);
}

/*
 * memcpy
 *
 *   Optimized memcpy that can be used on PROM data.
 */

void *
memcpy(void *dest, const void *source, size_t len)
{
    char       *dst	= dest;
    char       *src	= (char *) source;

    if (dst < src || src + len < dst) {
	/*
	 * Forward copy
	 */

	if ((((__uint64_t) dst ^ (__uint64_t) src) & 7) != 0)
	    goto residual_1;	/* Totally unaligned */

	while (len > 0 && ((__uint64_t) dst & 7) != 0) {
	    *dst++ = LBYTE(src);
	    src++;
	    len--;
	}

	while (len >= 64) {
	    ((__uint64_t *) dst)[0] = ((__uint64_t *) src)[0];
	    ((__uint64_t *) dst)[1] = ((__uint64_t *) src)[1];
	    ((__uint64_t *) dst)[2] = ((__uint64_t *) src)[2];
	    ((__uint64_t *) dst)[3] = ((__uint64_t *) src)[3];
	    ((__uint64_t *) dst)[4] = ((__uint64_t *) src)[4];
	    ((__uint64_t *) dst)[5] = ((__uint64_t *) src)[5];
	    ((__uint64_t *) dst)[6] = ((__uint64_t *) src)[6];
	    ((__uint64_t *) dst)[7] = ((__uint64_t *) src)[7];
	    dst += 64;
	    src += 64;
	    len -= 64;
	}

	while (len >= 8) {
	    *(__uint64_t *) dst = *(__uint64_t *) src;
	    dst += 8;
	    src += 8;
	    len -= 8;
	}

    residual_1:

	while (len-- > 0) {
	    *dst++ = LBYTE(src);
	    src++;
	}
    } else {
	/*
	 * Backward copy
	 */

	dst += len;
	src += len;

	if ((((__uint64_t) dst ^ (__uint64_t) src) & 7) != 0)
	    goto residual_2;	/* Totally unaligned */

	while (len > 0 && ((__uint64_t) dst & 7) != 0) {
	    --src;
	    *--dst = LBYTE(src);
	    len--;
	}

	while (len >= 64) {
	    dst -= 64;
	    src -= 64;
	    len -= 64;
	    ((__uint64_t *) dst)[7] = ((__uint64_t *) src)[7];
	    ((__uint64_t *) dst)[6] = ((__uint64_t *) src)[6];
	    ((__uint64_t *) dst)[5] = ((__uint64_t *) src)[5];
	    ((__uint64_t *) dst)[4] = ((__uint64_t *) src)[4];
	    ((__uint64_t *) dst)[3] = ((__uint64_t *) src)[3];
	    ((__uint64_t *) dst)[2] = ((__uint64_t *) src)[2];
	    ((__uint64_t *) dst)[1] = ((__uint64_t *) src)[1];
	    ((__uint64_t *) dst)[0] = ((__uint64_t *) src)[0];
	}

	while (len >= 8) {
	    dst -= 8;
	    src -= 8;
	    len -= 8;
	    *(__uint64_t *) dst = *(__uint64_t *) src;
	}

    residual_2:

	while (len-- > 0) {
	    --src;
	    *--dst = LBYTE(src);
	}
    }

    return dest;
}

/*
 * memsum
 *
 *   Efficiently computes the sum of bytes in a memory range.
 *   Can be used on PROM data.
 */

__uint64_t
memsum(void *base, size_t len)
{
    uchar_t    *src	= base;
    __uint64_t	sum, part;
    int		i;

    sum = 0;

    while (len > 0 && (__psunsigned_t) src & 7) {
	sum += LBYTEU(src);
	src++;
	len--;
    }

    while (len >= 128 * 8) {
	part = qs128((ulong) src);

	sum += part	  & 0xffff;
	sum += part >> 16 & 0xffff;
	sum += part >> 32 & 0xffff;
	sum += part >> 48 & 0xffff;

	src += 128 * 8;
	len -= 128 * 8;
    }

    while (len > 0) {
	sum += LBYTEU(src);
	src++;
	len--;
    }

    return sum;
}

/*
 * atoi
 */

int
atoi(const char *cp)
{
    int		i, c, neg;

    if (LBYTE(cp) == '-') {
	cp++;
	neg = 1;
    } else
	neg = 0;

    /* Calculate in the negative so -MAXINT works */

    for (i = 0; isdigit(c = LBYTE(cp)); cp++)
	i = i * 10 - (c - '0');

    return neg ? i : -i;
}

#define DIGIT(x)	(isdigit(x) ? (x) - '0' : \
			islower(x) ? (x) + 10 - 'a' : (x) + 10 - 'A')

/*
 * htol
 */

__uint64_t
htol(char *cp)
{
    __uint64_t		i;

    /* Ignore leading 0x or 0X */

    if (*cp == '0' && (cp[1] == 'x' || cp[1] == 'X'))
	cp += 2;

    i = 0;

    while (isxdigit(*cp)) {
	i = i * 16 + DIGIT(*cp);
	cp++;
    }

    return i;
}

/*
 * strtoull
 */

__uint64_t
strtoull(const char *str, char **nptr, int base)
{
	unsigned long long	val;
	int			c;
	int			xx;
	const char		**ptr = (const char **)nptr;
	int			neg = 0;

	if (ptr != (const char **)0)
		*ptr = str; /* in case no number is formed */
	c = *str;
	if (! isalnum(c)) {
		while (isspace(c))
			c = *++str;
		switch (c) {
		case '-':
			neg++;
			/* FALLTHROUGH */
		case '+':
			c = *++str;
		}
	}
	if (base == 0)
		if (c != '0')
			base = 10;
		else if (str[1] == 'x' || str[1] == 'X')
			base = 16;
		else if (str[1] == 'b' || str[1] == 'B')
			base = 2;
		else
			base = 8;
	/*
	 * for any base > 10, the digits incrementally following
	 *	9 are assumed to be "abc...z" or "ABC...Z"
	 */
	if (!isalnum(c) || (xx = DIGIT(c)) >= base)
		return (0); /* no number formed */
	if (base == 16 && c == '0' && (str[1] == 'x' || str[1] == 'X') &&
		isxdigit(str[2]))
		c = *(str += 2); /* skip over leading "0x" or "0X" */
	if (base == 2 && c == '0' && (str[1] == 'b' || str[1] == 'B') &&
		(str[2] == '0' || str[2] == '1'))
		c = *(str += 2); /* skip over leading "0b" or "0B" */

	val = (unsigned long long) DIGIT(c);
	for (c = *++str; isalnum(c) && (xx = DIGIT(c)) < base; ) {
		val *= (unsigned long long) base;
		val += (unsigned long long) xx;
		c = *++str;
	}
	if (ptr != (const char **)0)
		*ptr = str;
	return neg ? ((unsigned long long) -(__int64_t) val) : val;
}

/*
 * sprintnu -- output an unsigned number n in base b.
 */

void
sprintnu(char *tgt, __scunsigned_t n, int b, int digits)
{
    char	prbuf[72];
    char       *cp;
    int		d;

    cp = prbuf;

    do {
	d = (int) (n % b);
	*cp++ = (d < 10) ? (d + '0') : (d - 10 + 'a');
	n /= b;
	if (digits)
	    digits--;
    } while (n || digits);

    do
	*tgt++ = *--cp;
    while (cp > prbuf);

    *tgt++ = 0;
}

/*
 * sprintns -- output a signed number n in base b.
 */

void
sprintns(char *tgt, __scint_t n, int b, int digits)
{
    if (n < 0) {
	*tgt++ = '-';
	n = -n;
    }

    sprintnu(tgt, (__scunsigned_t) n, b, digits);
}

/*
 * sputs -- output a string with fill
 */

static void
sputs(int (*putc)(), __uint64_t putc_data,
      char *s, int fw, int len, char fillc)
{
    register int i;

    if (s == NULL)
	s = "(null)";
    if (len <= 0)
	i = strlen(s);
    else
	for (i = 0; i < len; i++)
	    if (LBYTE(&s[i]) == 0)
		break;
    len = i;

    if (fw > 0)
	for (i = fw - len; --i >= 0;)
	    (*putc)(fillc, putc_data);
    for (i = len; --i >= 0; s++)
	(*putc)(LBYTE(s), putc_data);
    if (fw < 0)
	for (i = -fw - len; --i >= 0;)
	    (*putc)(fillc, putc_data);
    return;
}

void
prf(int (*putc)(), __uint64_t putc_data, const char *fmt, va_list adx)
{
    int		c, i;
    char       *s;
    char	prbuf[72];	/* large enough for base 2 numbers */
    int		sgnd, base, fillc, fw, len, ljust;
    int		altform, islong, isshort;

    for (;;) {
	while (1) {
	    c = LBYTE(fmt);
	    fmt++;
	    if (c == 0)
		return;
	    if (c == '%')
		break;
	    (*putc)(c, putc_data);
	}

	fillc = ' ';
	fw = 0;
	len = 0;
	islong = 0;
	isshort = 0;

	c = LBYTE(fmt);
	fmt++;

	if (altform = (c == '#')) {
	    c = LBYTE(fmt);
	    fmt++;
	}
	if (ljust = (c == '-')) {
	    c = LBYTE(fmt);
	    fmt++;
	}
	if (c == '0') {
	    fillc = c;
	    c = LBYTE(fmt);
	    fmt++;
	}
	if (isdigit(c)) {
	    i = 0;
	    while (isdigit(c)) {
		i = i * 10 + c - '0';
		c = LBYTE(fmt);
		fmt++;
	    }
	    fw = i;
	} else if (c == '*') {	/* precision from arg */
	    /*CONSTCOND*/
	    fw = va_arg(adx, int);
	    c = LBYTE(fmt);
	    fmt++;
	}
	if (c == '.') {
	    i = 0;
	    c = LBYTE(fmt);
	    fmt++;
	    if (c == '*') {	/* precision from arg */
		/*CONSTCOND*/
		i = va_arg(adx, int);
		c = LBYTE(fmt);
		fmt++;
	    } else
		while (isdigit(c)) {
		    i = i * 10 + c - '0';
		    c = LBYTE(fmt);
		    fmt++;
		}
	    len = i;
	}
	if (ljust)
	    fw = -fw;

	if (c == 'l' || c == 'L') {
	    islong = 1;
	    c = LBYTE(fmt);
	    fmt++;
	    if (c == 'l' || c == 'L') {
		c = LBYTE(fmt);
		fmt++;
	    }
	} else if (c == 'h') {
	    isshort = 1;
	    c = LBYTE(fmt);
	    fmt++;
	}

	switch (c) {
	case 'y':		/* Short for 0x%016llx */
	    fw = 16;
	    fillc = '0';
	    islong = 1;
	    c = 'x';
	    altform = 1;
	    /* fall through */
	case 'x':
	case 'X':
	    sgnd = 0;
	    base = 16;
	    if (altform) {
		(*putc)('0', putc_data);
		(*putc)(c, putc_data);
	    }
	    goto number;
	case 'd':
	case 'i':
	    sgnd = 1;
	    base = 10;
	    goto number;
	case 'u':
	    sgnd = 0;
	    base = 10;
	    goto number;
	case 'o':
	    sgnd = 0;
	    base = 8;
	    if (altform)
		(*putc)('0', putc_data);
	    goto number;
	case 'b':
	    sgnd = 0;
	    base = 2;
	    if (altform) {
		(*putc)('0', putc_data);
		(*putc)(c, putc_data);
	    }
	number:
	    if (isshort) {
		if (sgnd)
		    sprintns(prbuf, va_arg(adx, short), base, 0);
		else
		    sprintnu(prbuf, va_arg(adx, ushort_t), base, 0);
	    } else if (islong) {
		if (sgnd)
		    sprintns(prbuf, va_arg(adx, long), base, 0);
		else
		    sprintnu(prbuf, va_arg(adx, ulong), base, 0);
	    } else {
		if (sgnd)
		    sprintns(prbuf, va_arg(adx, __scint_t), base, 0);
		else
		    sprintnu(prbuf, va_arg(adx, __scunsigned_t), base, 0);
	    }
	    sputs(putc, putc_data, prbuf, fw, len, fillc);
	    break;
	case 'c':		/* print a character */
	    /*CONSTCOND*/
	    prbuf[0] = va_arg(adx, int);
	    prbuf[1] = 0;
	    sputs(putc, putc_data, prbuf, fw, len, fillc);
	    break;
	case 's':		/* Print a string */
	    s = va_arg(adx, char *);
	    sputs(putc, putc_data, s, fw, len, fillc);
	    break;
	case '%':
	    (*putc)(c, putc_data);
	    break;
	case 'C':	/* PROM SPECIAL: CPU letter */
	    (*putc)('A' + (int) hub_cpu_get(), putc_data);
	    break;
	case 'P':	/* PROM SPECIAL: display address as symbol+offset */
	    symbol_name_off((void *) va_arg(adx, ulong), prbuf);
	    sputs(putc, putc_data, prbuf, fw, len, fillc);
	    break;
	case 'S':	/* PROM SPECIAL: /hw/module/%d/slot/n%d */
	    strcpy(prbuf, "/hw/module/");
	    sprintns(prbuf + 11, va_arg(adx, __scint_t), 10, 0);
	    strcat(prbuf, "/slot/");
	    if (SN00)
		strcat(prbuf, "MotherBoard");
	    else {
		i = strlen(prbuf);
		prbuf[i++] = 'n';
		sprintns(prbuf + i, va_arg(adx, __scint_t), 10, 0);
	    }
	    sputs(putc, putc_data, prbuf, fw, len, fillc);
	    break;
	default:
	    break;
	}
    }
}

/*VARARGS1*/
int
printf(const char *fmt, ...)
{
    va_list	ap;

    va_start(ap, fmt);
    prf(putc, 0, fmt, ap);
    va_end(ap);

    return 0;
}

int
vprintf(const char *fmt, va_list ap)
{
    prf(putc, 0, fmt, ap);

    return 0;
}

int db_printf(const char *fmt, ...)
{
    va_list	ap;

    if (get_libc_verbosity()) {
	va_start(ap, fmt);
	prf(putc, 0, fmt, ap);
	va_end(ap);
    }

    return 0;
}

int db_vprintf(const char *fmt, va_list ap)
{
    if (get_libc_verbosity())
	prf(putc, 0, fmt, ap);

    return 0;
}

static int
sprint_putc(int c, __uint64_t data)
{
    *(*((char **) data))++ = c;
    return 0;
}

/*VARARGS2*/
int
sprintf(char *buf, const char *fmt, ...)
{
    va_list	ap;

    va_start(ap, fmt);
    prf(sprint_putc, (__uint64_t) &buf, fmt, ap);
    va_end(ap);

    *buf = 0;

    return 0;
}

int
vsprintf(char *buf, const char *fmt, va_list ap)
{
    prf(sprint_putc, (__uint64_t) &buf, fmt, ap);

    *buf = '\0';
    return 0;
}

#if 0	/* Commented out to save room */
/*VARARGS1*/
static int
length_putc(int c, __uint64_t data)
{
    return (*(int *) data)++;
}

int
sprintf_length(const char *fmt, ...)
{
    va_list	ap;
    int		length = 1;

    va_start(ap, fmt);
    prf(length_putc, (__uint64_t) &length, fmt, ap);
    va_end(ap);

    return length;
}
#endif

int majority(int n, int *votes)
{
    int         i, ayes = 0, nos = 0;

    for (i = 0; i < n; i++)
        if (votes[i])
            ayes++;
        else
            nos++;

    if (ayes > nos)
        return 1;
    else
        return 0;
}
