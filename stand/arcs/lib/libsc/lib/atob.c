/*
 * atoi.c - atob and atoi
 *
 * $Revision: 1.11 $
 */
#include <ctype.h>
#include <libsc.h>

/*
 * _digit -- convert the ascii representation of a digit to its
 * binary representation
 */
unsigned
_digit(char c)
{
	unsigned d;

	if (isdigit(c))
		d = (unsigned)(c - '0');
	else if (isalpha(c)) {
		if (isupper(c))
			c = _tolower(c);
		d = (unsigned)(c - 'a' + 10);
	} else
		d = 999999; /* larger than any base to break callers loop */

	return(d);
}

/*
 * _xdigit -- convert the ascii representation of a digit to its
 * binary representation. This one only looks at hex digits if passed in
 * the 'hex' flag.
 */
static unsigned
_xdigit(char c, int hex)
{
	unsigned d = 999999; /* larger than any base to break callers loop */

	if (isdigit(c))
		d = (unsigned)(c - '0');
	else if (hex && (c >= 'a' && c <= 'f'))
		d = (unsigned)(c - 'a' + 10);
	else if (hex && (c >= 'A' && c <= 'F'))
		d = (unsigned)(c - 'A' + 10);

	return(d);
}

/*
 * atob -- convert ascii to binary.  Accepts all C numeric formats.
 */
char *
atob(const char *cp, int *iptr)
{
	register unsigned base = 10;
	register int value = 0;
	register unsigned d;
	int minus = 0;
	int overflow = 0;

	*iptr = 0;
	if (!cp)
		return(0);

	while (isspace(*cp))
		cp++;

	while (*cp == '-') {
		cp++;
		minus = !minus;
	}

	/*
	 * Determine base by looking at first 2 characters
	 */
	if (*cp == '0') {
		switch (*++cp) {
		case 'X':
		case 'x':
			base = 16;
			cp++;
			break;

		case 'B':	/* a frill: allow binary base */
		case 'b':
			base = 2;
			cp++;
			break;
		
		default:
			base = 8;
			break;
		}
	}

	while ((d = _xdigit(*cp, base == 16)) < base) {
		if ((unsigned)value > ((unsigned)-1/base))
			overflow++;
		value *= base;
		if ((unsigned)value > ((unsigned)-1 - d))
			overflow++;
		value += d;
		cp++;
	}

	if (overflow || (((unsigned)value == 0x80000000) && minus))
		printf("WARNING: conversion overflow\n");
	if (minus)
		value = -value;

	*iptr = value;
	return((char*)cp);
}

char *
atob_ptr(const char *cp, __psint_t *iptr)
{
#if (_MIPS_SZPTR == 32)
	return atob(cp, (int *)iptr);
#elif (_MIPS_SZPTR == 64)
	return atob_L(cp, (long long *)iptr);
#else
	<<<BOMB>>>
#endif
}

/*
 * atob_L -- convert ascii to binary.  Accepts all C numeric formats.
 * Converted value is treated as a 64-bit long long.
 */
char *
atob_L(const char *cp, long long *iptr)
{
	register unsigned base = 10;
	register long long value = 0;
	register unsigned d;
	int minus = 0;
	int overflow = 0;

	*iptr = 0;
	if (!cp)
		return(0);

	while (isspace(*cp))
		cp++;

	while (*cp == '-') {
		cp++;
		minus = !minus;
	}

	/*
	 * Determine base by looking at first 2 characters
	 */
	if (*cp == '0') {
		switch (*++cp) {
		case 'X':
		case 'x':
			base = 16;
			cp++;
			break;

		case 'B':	/* a frill: allow binary base */
		case 'b':
			base = 2;
			cp++;
			break;
		
		default:
			base = 8;
			break;
		}
	}

	while ((d = _xdigit(*cp, base == 16)) < base) {
		if ((unsigned long long)value > ((unsigned long long)-1/base))
			overflow++;
		value *= base;
		if ((unsigned long long)value > (unsigned long long)((unsigned long long)-1 - d))
			overflow++;
		value += d;
		cp++;
	}

	if (overflow ||
	    (((unsigned long long)value == 0x8000000000000000LL) && minus))
		printf("WARNING: conversion overflow\n");
	if (minus)
		value = -value;

	*iptr = value;
	return((char*)cp);
}

int
atoi(const char *cp)
{
	int x;
	(void)atob(cp, &x);
	return x;
}

void 
atohex(const char *a, int *h)
{
	if (strncmp(a, "0x", 2) == 0 || strncmp(a, "0X", 2) == 0)
		atob(a, h);
	else {
		char ca[16];

		strcpy(ca, "0x");
		strcat(ca, a);
		atob(ca, h);
	}
}

void 
atohex_L(const char *a, long long *h)
{
	if (strncmp(a, "0x", 2) == 0 || strncmp(a, "0X", 2) == 0)
		atob_L(a, h);
	else {
		char ca[32];

		strcpy(ca, "0x");
		strcat(ca, a);
		atob_L(ca, h);
	}
}

void 
atohex_ptr(const char *a, __psunsigned_t *p)
{
#if (_MIPS_SZPTR == 32)
	atohex(a, (int *)p);
#elif (_MIPS_SZPTR == 64)
	atohex_L(a, (long long *)p);
#else
        <<<BOMB>>>
#endif

}

/*
 * strtoull
 */

#define DIGIT(x)	(isdigit(x) ? (x) - '0' : \
			islower(x) ? (x) + 10 - 'a' : (x) + 10 - 'A')
unsigned long long
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
