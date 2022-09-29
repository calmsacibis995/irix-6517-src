/*
 * These are the strn* routines.
 *
 */
#include <libsc.h>
#include <sys/types.h>

/* @(#)strncmp.c	4.1 (Berkeley) 12/21/80 */
/*
 * Compare strings (at most n bytes):  s1>s2: >0  s1==s2: 0  s1<s2: <0
 */

int
strncmp(const char *s1, const char *s2, size_t sn)
{
	int n = sn;

	while (--n >= 0 && *s1 == *s2++)
		if (*s1++ == '\0')
			return(0);
	return(n<0 ? 0 : *s1 - *--s2);
}

/* @(#)strncpy.c	4.1 (Berkeley) 12/21/80 */
/*
 * Copy s2 to s1, truncating or null-padding to always copy n bytes
 * return s1
 */

char *
strncpy(char *s1, const char *s2, size_t sn)
{
	register int i;
	register char *os1;
	int n = sn;

	os1 = s1;
	for (i = 0; i < n; i++)
		if ((*s1++ = *s2++) == '\0') {
			while (++i < n)
				*s1++ = '\0';
			return(os1);
		}
	return(os1);
}

/* @(#)strncat.c	4.1 (Berkeley) 12/21/80 */
/*
 * Concatenate s2 on the end of s1.  S1's space must be large enough.
 * At most n characters are moved.
 * Return s1.
 */

char *
strncat(char *s1, const char *s2, size_t sn)
{
	register char *os1;
	int n = sn;

	os1 = s1;
	while (*s1++)
		;
	--s1;
	while (*s1++ = *s2++)
		if (--n < 0) {
			*--s1 = '\0';
			break;
		}
	return(os1);
}


/* @(#)index.c	4.1 (Berkeley) 12/21/80 */
/*
 * Return the ptr in sp at which the character c appears;
 * NULL if not found
 */

#ifndef NULL
#define	NULL	0
#endif

char *
index(const char *sp, int c)
{
	do {
		if (*sp == c)
			return((char *)sp);
	} while (*sp++);
	return(NULL);
}

char *
rindex(const char *sp, int c)
{
	register const char *cp = NULL;

	do {
		if (*sp == c)
			cp = sp;
	} while (*sp++);
	return((char *)cp);
}
