/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
static char *sccsid = "@(#)head.c	5.2 (Berkeley) 6/21/85";
#endif /* not lint */

#include "glob.h"
#include <ctype.h>

/*
 * Mail -- a mail program
 *
 * Routines for processing and detecting headlines.
 */

/*
 * See if the passed line buffer is a mail header.
 * Return true if yes.  Note the extreme pains to
 * accomodate all funny formats.
 */

ishead(linebuf)
	char linebuf[];
{
	register char *cp;
	struct headline hl;
	char parbuf[BUFSIZ];

	cp = linebuf;
	if (strncmp("From ", cp, 5) != 0)
		return(0);
	return(parse(cp, &hl, parbuf));
}


/*
 * Split a headline into its useful components.
 * Copy the line into dynamic string space, then set
 * pointers into the copied line in the passed headline
 * structure.  Actually, it scans.
 *
 * Return 0 if headline unparseable (invalid).
 */

parse(line, hl, pbuf)
	char line[], pbuf[];
	struct headline *hl;
{
	register char *cp, *dp, *rdp;
	char *sp;
	char word[LINESIZE];

	hl->l_from = NOSTR;
	hl->l_tty = NOSTR;
	hl->l_date = NOSTR;
	cp = line;
	sp = pbuf;

	/*
	 * Skip the first "word" of the line, which should be "From"
	 * anyway.
	 */

	word[0] = '\0';
	cp = nextword(cp, word);
	rdp = cp; 
	dp = nextword(cp, word);

	if (!equal(word, ""))
		hl->l_from = copyin(word, &sp);
	if (dp != NOSTR && strncmp(dp, "tty", 3) == 0) {
		cp = nextword(dp, word);
		hl->l_tty = copyin(word, &sp);
		if (cp != NOSTR)
			hl->l_date = copyin(cp, &sp);
	}
	else
		if (dp != NOSTR)
			hl->l_date = copyin(dp, &sp);

	if (isdate(hl->l_date))
		return(1);
	else {
		hl->l_from = NOSTR;
	        if (rdp != NOSTR && strncmp(rdp, "tty", 3) == 0) {
			cp = nextword(rdp, word);
                	hl->l_tty = copyin(word, &sp);
                	if (cp != NOSTR)
                       		hl->l_date = copyin(cp, &sp);
        	}
        	else {
                	if (rdp != NOSTR)
                        hl->l_date = copyin(rdp, &sp);
		}
        	if (isdate(hl->l_date))
                	return(1);
		else
			return(0);
	}
}

/*
 * Copy the string on the left into the string on the right
 * and bump the right (reference) string pointer by the length.
 * Thus, dynamically allocate space in the right string, copying
 * the left string into it.
 */

char *
copyin(src, space)
	char src[];
	char **space;
{
	register char *cp, *top;
	register int s;

	s = strlen(src);
	cp = *space;
	top = cp;
	strcpy(cp, src);
	cp += s + 1;
	*space = cp;
	return(top);
}

/*
 * Test to see if the passed string is a ctime(3) generated
 * date string as documented in the manual.  The template
 * below is used as the criterion of correctness.
 * Also, we check for a possible trailing time zone using
 * the auxtype template.
 */

#define	L	1		/* A lower case char */
#define	S	2		/* A space */
#define	D	3		/* A digit */
#define	O	4		/* An optional digit or space */
#define	C	5		/* A colon */
#define	N	6		/* A new line */
#define U	7		/* An upper case char */
#define A	8		/* A alphabetic (upper or lower) */
#define G	9		/* GMT offset direction +/- */

struct datefmt {
	char fmtstr[128];
} datefmts[] = {

/* No timezone */

/* S u n   A u g     5   2 3 : 4 8 : 5 1           1 9 8 4 */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,C,D,D,S,        D,D,D,D,0},
/* S u n   A u g     5   2 3 : 4 8 : 5 1  */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,C,D,D,0},
/* S u n   A u g     5   2 3 : 4 8                 1 9 8 4 */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,      S,        D,D,D,D,0},
/* S u n   A u g     5   2 3 : 4 8 */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,0},

/* Alpha timezone, no qualifier */

/* S u n   A u g     5   2 3 : 4 8 : 5 1   P S T   1 9 8 4 */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,C,D,D,S,A,A,A,S,D,D,D,D,0},
/* S u n   A u g     5   2 3 : 4 8 : 5 1   P S T */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,C,D,D,S,A,A,A,0},
/* S u n   A u g     5   2 3 : 4 8         P S T   1 9 8 4 */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,      S,A,A,A,S,D,D,D,D,0},
/* S u n   A u g     5   2 3 : 4 8         P S T */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,      S,A,A,A,0},

/* Alpha timezone, with qualifier (daylight savings time). */

/* S u n   A u g     5   2 3 : 4 8 : 5 1   M E T   D S T   1 9 8 4 */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,C,D,D,S,A,A,A,S,A,A,A,S,D,D,D,D,0},
/* S u n   A u g     5   2 3 : 4 8 : 5 1   M E T   D S T */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,C,D,D,S,A,A,A,S,A,A,A,0},
/* S u n   A u g     5   2 3 : 4 8         M E T   D S T   1 9 8 4 */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,      S,A,A,A,S,A,A,A,S,D,D,D,D,0},
/* S u n   A u g     5   2 3 : 4 8         M E T   D S T */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,      S,A,A,A,S,A,A,A,0},

/* GMT offset */

/* S u n   A u g     5   2 3 : 4 8 : 5 1   - 0 7 0 0   1 9 8 4 */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,C,D,D,S,G,D,D,D,D,S,D,D,D,D,0},
/* S u n   A u g     5   2 3 : 4 8 : 5 1   - 0 7 0 0 */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,C,D,D,S,G,D,D,D,D,0},
/* S u n   A u g     5   2 3 : 4 8         - 0 7 0 0   1 9 8 4 */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,      S,G,D,D,D,D,S,D,D,D,D,0},
/* S u n   A u g     5   2 3 : 4 8         - 0 7 0 0 */
  {U,L,L,S,U,L,L,S,O,D,S,D,D,C,D,D,      S,G,D,D,D,D,0},

/* END */
  0
};

isdate(date)
	char date[];
{
	register char *cp;
	register int idx;

	cp = date;
	if (cp == NOSTR)
		return(0);

	for (idx = 0; datefmts[idx].fmtstr[0]; idx++) {
		if (cmatch(cp, datefmts[idx].fmtstr))
			return(1);
	}
	return(0);
}

/*
 * Match the given string against the given template.
 * Return 1 if they match, 0 if they don't
 */

cmatch(str, temp)
	char str[], temp[];
{
	register char *cp, *tp;
	register int c;

	cp = str;
	tp = temp;
	while (*cp != '\0' && *tp != 0) {
		c = *cp++;
		switch (*tp++) {
		case L:
			if (c < 'a' || c > 'z')
				return(0);
			break;

		case U:
			if (c < 'A' || c > 'Z')
				return(0);
			break;

		case A:
			if (c < 'A' || c > 'Z')
				if (c < 'a' || c > 'z')
					return(0);
			break;

		case S:
			if (c != ' ')
				return(0);
			break;

		case D:
			if (!isdigit(c))
				return(0);
			break;

		case O:
			if (c != ' ' && !isdigit(c))
				return(0);
			break;

		case C:
			if (c != ':')
				return(0);
			break;

		case N:
			if (c != '\n')
				return(0);
			break;

		case G:
			if (c != '+' && c != '-')
				return(0);
			break;
		}
	}
#ifdef sgi
	/* 
	 * allow additional fields (like timezone offset)
	 * at the end of the string
	 */
	if (*tp != 0)
#else
	if (*cp != '\0' || *tp != 0)
#endif
		return(0);
	return(1);
}

/*
 * Collect a liberal (space, tab delimited) word into the word buffer
 * passed.  Also, return a pointer to the next word following that,
 * or NOSTR if none follow.
 */

char *
nextword(wp, wbuf)
	char wp[], wbuf[];
{
	register char *cp, *cp2;

	if ((cp = wp) == NOSTR) {
		copy("", wbuf, LINESIZE - strlen(wbuf));
		return(NOSTR);
	}
	cp2 = wbuf;
	while (!any(*cp, " \t") && *cp != '\0')
		if (*cp == '"') {
 			*cp2++ = *cp++;
 			while (*cp != '\0' && *cp != '"')
 				*cp2++ = *cp++;
 			if (*cp == '"')
 				*cp2++ = *cp++;
 		} else
 			*cp2++ = *cp++;
	*cp2 = '\0';
	while (any(*cp, " \t"))
		cp++;
	if (*cp == '\0')
		return(NOSTR);
	return(cp);
}

/*
 * Copy str1 to str2, return pointer to null in str2.
 *
 * The size2 argument is the size in bytes of the 'str2' buffer,
 * including the last byte of NULL.
 */

char *
copy(char *str1, char *str2, int size2)
{
	register char *s1, *s2;

	s1 = str1;
	s2 = str2;
	while (*s1 && size2) {
		if((s2 + 1) >= (str2 + (size2 - 1)))
			break;
		*s2++ = *s1++;
	}
	*s2 = 0;
	return(s2);
}

/*
 * Is ch any of the characters in str?
 */

any(ch, str)
	char *str;
{
	register char *f;
	register c;

	f = str;
	c = ch;
	while (*f)
		if (c == *f++)
			return(1);
	return(0);
}

