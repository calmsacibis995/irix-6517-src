#ifndef lint
static char sccsid[] = 	"@(#)getname.c	1.2 88/05/18 4.0NFSSRC Copyr 1988 Sun Micro";
#endif

/*
 * Copyright (C) 1986, Sun Microsystems, Inc.
 */

#include <stdio.h>

extern char *index();

#define iseol(c)	(c == 0 || c == '\n' || index(com, c) != NULL)
#define issep(c)	(index(sep, c) != NULL)
#define isignore(c) (index(ignore, c) != NULL)

/*
 * is_all_ignore()
 * Returns 1 if all the characters on the line are to be ignored.
 * At present, this routine will ignore both tabs and spaces.
 * Note that this is not as general as getname which takes
 * the characters to be ignored as an input parameter.
 */
int is_all_ignore(line)
	char *line;
{
	char *ignore = "\t \n";

	while (*line) {
		if (!isignore(*line++))
			return (0);
	}
	return (1);
}

/*
 * getline()
 * Read a line from a file.
 * What's returned is a cookie to be passed to getname
 */
char **
getline(line, maxlinelen, f, lcount, com)
	char *line;
	int maxlinelen;
	FILE *f;
	int *lcount;
	char *com;
{
	char *p;
	static char *lp;
	do {
		if (! fgets(line, maxlinelen, f)) {
			return (NULL);
		}
		(*lcount)++;
	} while (iseol(line[0]) || is_all_ignore(line));
	p = line;
	for (;;) {	
		while (*p) {	
			p++;
		}
		if (*--p == '\n' && *--p == '\\') {
			if (! fgets(p, maxlinelen, f)) {
				break;	
			}
			(*lcount)++;
		} else {
			break;	
		}
	}
	lp = line;
	return (&lp);
}


/*
 * getname()
 * Get the next entry from the line.
 * You tell getname() which characters to ignore before storing them 
 * into name, and which characters separate entries in a line.
 * The cookie is updated appropriately.
 * return:
 *	  1: one entry parsed
 *	  0: partial entry parsed, ran out of space in name
 *  -1: no more entries in line
 */
getname(name, namelen, ignore, sep, linep, com)
	char *name;
	int namelen;
	char *ignore;	
	char *sep;
	char **linep;
	char *com;
{
	register char c;
	register char *lp;
	register char *np;

	lp = *linep;
	do {
		c = *lp++;
	} while (isignore(c) && !iseol(c));
	if (iseol(c)) {
		*linep = lp - 1;
		return (-1);
	}
	np = name;
	while (! issep(c) && ! iseol(c) && np - name < namelen) {	
		*np++ = c;	
		c = *lp++;
	} 
	lp--;
	if (np - name < namelen) {
		*np = 0;
		if (iseol(c)) {
			*lp = 0;
		} else {
			lp++; 	/* advance over separator */
		}
	} else {
		*linep = lp;
		return (0);
	}
	*linep = lp;
	return (1);
}
