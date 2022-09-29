/*
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

/* @(#)strcasecmp.c	1.3 (Berkeley) 8/3/87 */

#include <ctype.h>

int
tolower(int c)
{
	return(isupper(c)?_tolower(c):c);
}

int
strcasecmp(char *s1, char *s2)
{
	while (tolower(*s1) == tolower(*s2++))
		if (*s1++ == '\0')
			return(0);
	return(tolower(*s1) - tolower(*--s2));
}

int
strncasecmp(char *s1, char *s2, unsigned un)
{
	int n = un;

	while (--n >= 0 && tolower(*s1) == tolower(*s2++))
		if (*s1++ == '\0')
			return(0);
	return(n < 0 ? 0 : tolower(*s1) - tolower(*--s2));
}

