/*
 * Procedures from libc
 * XXX plus some SVR4 utility functions, which ought to be cleaned up
 * XXX and unified with sprintf
 */
#ident "$Revision: 3.19 $"

#include <values.h>
#include <string.h>
#include <sys/debug.h>
#include <ctype.h>
#include <sys/types.h>

/*
 * Compare strings:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 */
int
strcmp(const char *s1, const char *s2)
{
	if (s1 == s2)
		return(0);
	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return (0);
	return (*s1 - *--s2);
}

int
strncmp(const char *s1, const char *s2, size_t n)
{
	n++;
	if((unsigned char *)s1 == (unsigned char *)s2)
		return(0);
	while(--n != 0 && (unsigned char)*s1 == (unsigned char)*s2++)
		if((unsigned char)*s1++ == '\0')
			return(0);
	return((n == 0)? 0: ((unsigned char)*s1 - (unsigned char)s2[-1]));
}

char *
strncpy(char *s1, const char *s2, size_t n)
{
	register int i;
	register char *os1;

	os1 = s1;
	for (i = 0; i < n; i++)
		if ((*s1++ = *s2++) == '\0') {
			while (++i < n)
				*s1++ = '\0';
			return(os1);
		}
	return(os1);
}

char *
strcpy(char *to, const char *from)
{
	char *oldto;

	oldto = to;
	while (*to++ = *from++)
		;
	return (oldto);
}

char *
strchr(const char *sp, int c)
{
	do {
		if (*sp == c)
			return((char *)sp);
	} while(*sp++);
	return(0);
}

/*
 * Concatenate s2 on the end of s1.  S1's space must be large enough.
 * Return s1.
 */

char *
strcat(char *s1, const char *s2)
{
	register char *os1;

	os1 = s1;
	while(*s1++)
		;
	--s1;
	while(*s1++ = *s2++)
		;
	return(os1);
}

char *
strstr(const char *instr1, const char *instr2)
{
	register const char *s1, *s2, *tptr;
	register char c;

	if (instr2 == (char *)0 || *instr2 == '\0')
		return((char *)instr1);

	s1 = instr1;
	s2 = instr2;
	c = *s2;

	while (*s1)
		if (*s1++ == c) {
			tptr = s1;
			while ((c = *++s2) == *s1++ && c)
				;
			if (c == 0)
				return((char *)tptr - 1);
			s1 = tptr;
			s2 = instr2;    /* reset search string */
			c = *s2;
		}
	return((char *)0);
}

size_t
strspn(string, charset)
const char *string;
const char *charset;
{
	register const char *p, *q;

	for(q=string; *q != '\0'; ++q) {
		for(p=charset; *p != '\0' && *p != *q; ++p)
			;
		if(*p == '\0')
			break;
	}
	return((size_t)(q-string));
}

char *
strpbrk(string, brkset)
register const char *string;
const char *brkset;
{
        register const char *p;

        do {
                for(p=brkset; *p != '\0' && *p != *string; ++p)
                        ;
                if(*p != '\0')
                        return((char *)string);
        }
        while(*string++);
        return(NULL);
}

char *
strtok_r(char *string, const char *sepset, char **lasts)
{
	register char	*q, *r;

	/*first or subsequent call*/
	if (string == NULL)
		string = *lasts;

	if(string == 0)		/* return if no tokens remaining */
		return(NULL);

	q = string + strspn(string, sepset);	/* skip leading separators */

	if(*q == '\0') {		/* return if no tokens remaining */
		*lasts = 0;	/* indicate this is last token */
		return(NULL);
	}

	if((r = strpbrk(q, sepset)) == NULL)	/* move past token */
		*lasts = 0;	/* indicate this is last token */
	else {
		*r = '\0';
		*lasts = r+1;
	}
	return(q);
}

#define xtod(c)		((c) <= '9' ? '0' - (c) : 'a' - (c) - 10)
long
atoi(register char *p)
{
	register long n;
	register int c, neg = 0;

	if (p == NULL)
		return 0;

	if (!isdigit(c = *p)) {
		while (isspace(c))
			c = *++p;
		switch (c) {
		case '-':
			neg++;
		case '+': /* fall-through */
			c = *++p;
		}
		if (!isdigit(c))
			return (0);
	}
	if (c == '0' && *(p + 1) == 'x') {
		p += 2;
		c = *p;
		n = xtod(c);
		while ((c = *++p) && isxdigit(c)) {
			n *= 16; /* two steps to avoid unnecessary overflow */
			n += xtod(c); /* accum neg to avoid surprises at MAX */
		}
	} else {
		n = '0' - c; 
		while ((c = *++p) && isdigit(c)) {
			n *= 10; /* two steps to avoid unnecessary overflow */
			n += '0' - c; /* accum neg to avoid surprises at MAX */
		}
	}
	return (neg ? n : -n);
}

/*
 * convert an integer to a decimal string
 * str must be big enough!
 */
void
numtos(int val, char *str)
{
	char 	mybuf[32];
	char	*cp, *rp;

	ASSERT(str != NULL);
	if (val == 0) {
		*str++ = '0';
		*str = '\0';
	}
	else {
		for (cp = mybuf; val; val /=10)
			*cp++ = "0123456789F"[val % 10];
		
		for (--cp, rp = str; cp >= mybuf; )
			*rp++ = *cp--;
		*rp = '\0';
	}
}


/*
 * This array is designed for mapping upper and lower case letter
 * together for a case independent comparison.  The mappings are
 * based upon ascii character sequences.
 */
static const char charmap[] = {
	'\000', '\001', '\002', '\003', '\004', '\005', '\006', '\007',
	'\010', '\011', '\012', '\013', '\014', '\015', '\016', '\017',
	'\020', '\021', '\022', '\023', '\024', '\025', '\026', '\027',
	'\030', '\031', '\032', '\033', '\034', '\035', '\036', '\037',
	'\040', '\041', '\042', '\043', '\044', '\045', '\046', '\047',
	'\050', '\051', '\052', '\053', '\054', '\055', '\056', '\057',
	'\060', '\061', '\062', '\063', '\064', '\065', '\066', '\067',
	'\070', '\071', '\072', '\073', '\074', '\075', '\076', '\077',
	'\100', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
	'\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
	'\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
	'\170', '\171', '\172', '\133', '\134', '\135', '\136', '\137',
	'\140', '\141', '\142', '\143', '\144', '\145', '\146', '\147',
	'\150', '\151', '\152', '\153', '\154', '\155', '\156', '\157',
	'\160', '\161', '\162', '\163', '\164', '\165', '\166', '\167',
	'\170', '\171', '\172', '\173', '\174', '\175', '\176', '\177',
	'\200', '\201', '\202', '\203', '\204', '\205', '\206', '\207',
	'\210', '\211', '\212', '\213', '\214', '\215', '\216', '\217',
	'\220', '\221', '\222', '\223', '\224', '\225', '\226', '\227',
	'\230', '\231', '\232', '\233', '\234', '\235', '\236', '\237',
	'\240', '\241', '\242', '\243', '\244', '\245', '\246', '\247',
	'\250', '\251', '\252', '\253', '\254', '\255', '\256', '\257',
	'\260', '\261', '\262', '\263', '\264', '\265', '\266', '\267',
	'\270', '\271', '\272', '\273', '\274', '\275', '\276', '\277',
	'\300', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
	'\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
	'\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
	'\370', '\371', '\372', '\333', '\334', '\335', '\336', '\337',
	'\340', '\341', '\342', '\343', '\344', '\345', '\346', '\347',
	'\350', '\351', '\352', '\353', '\354', '\355', '\356', '\357',
	'\360', '\361', '\362', '\363', '\364', '\365', '\366', '\367',
	'\370', '\371', '\372', '\373', '\374', '\375', '\376', '\377',
};

int
strcasecmp(const char *s1, const char *s2)
{
	register const char *cm = charmap;

	while (cm[*s1] == cm[*s2++])
		if (*s1++ == '\0')
			return(0);
	return(cm[*s1] - cm[*--s2]);
}

/*
 * strtoull
 */

/*
 * Random defines for string interpretation code.  Changed _N to _N|_X
 * for isxdigit().
 */
unsigned char __ctype[] = {
	0,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_C,	_C|_S,	_C|_S,	_C|_S,	_C|_S,	_C|_S,	_C,	_C,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_C,	_C,	_C,	_C,	_C,	_C,	_C,	_C,
	_S|_B,	_P,	_P,	_P,	_P,	_P,	_P,	_P,
	_P,	_P,	_P,	_P,	_P,	_P,	_P,	_P,
	_N|_X,	_N|_X,	_N|_X,	_N|_X,	_N|_X,	_N|_X,	_N|_X,	_N|_X,
	_N|_X,	_N|_X,	_P,	_P,	_P,	_P,	_P,	_P,
	_P,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U|_X,	_U,
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U,
	_U,	_U,	_U,	_U,	_U,	_U,	_U,	_U,
	_U,	_U,	_U,	_P,	_P,	_P,	_P,	_P,
	_P,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L|_X,	_L,
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L,
	_L,	_L,	_L,	_L,	_L,	_L,	_L,	_L,
	_L,	_L,	_L,	_P,	_P,	_P,	_P,	_C,
        0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,
};

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
