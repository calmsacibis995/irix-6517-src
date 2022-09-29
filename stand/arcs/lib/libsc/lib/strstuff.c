/* str* functions (except strn*, which are in a seperate file) */
#include <libsc.h>
#include <libsc_internal.h>

/* @(#)strcat.c	4.1 (Berkeley) 12/21/80 */
/*
 * Concatenate s2 on the end of s1.  S1's space must be large enough.
 * Return s1.
 */

char *
strcat(char *s1, const char *s2)
{
	register char *os1;

	os1 = s1;
	while (*s1++)
		;
	--s1;
	while (*s1++ = *s2++)
		;
	return(os1);
}

/* @(#)strcmp.c	4.1 (Berkeley) 12/21/80 */
/*
 * Compare strings:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 */

int
strcmp(const char *s1, const char *s2)
{

	while (*s1 == *s2++)
		if (*s1++ == '\0')
			return(0);
	return(*s1 - *--s2);
}

/* @(#)strcpy.c	4.1 (Berkeley) 10/5/82 */
/*
 * Copy string s2 to s1.  s1 must be large enough.
 * return s1
 */

char *
strcpy(char *s1, const char *s2)
{
	register char *os1;

	os1 = s1;
	while (*s1++ = *s2++)
		;
	return(os1);
}

/* @(#)strlen.c	4.1 (Berkeley) 12/21/80 */
/*
 * Returns the number of
 * non-NULL bytes in string argument.
 */

size_t
strlen(const char *s)
{
	register unsigned int n;

	n = 0;
	while (*s++)
		n++;
	return(n);
}

char *
strdup (const char *s)
{
	register int x = strlen(s)+1;
	register char *cp;

	if (!(cp = (char *)malloc(x)))
		return(0);
	strcpy(cp,s);
	return(cp);
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

/*
 * strchr
 */

char *
strchr(const char *s, int c)
{
    char d;

    for (; (d = *s) != 0; s++)
    if (d == c)
	return (char *) s;

    return ((char *)0);
}

