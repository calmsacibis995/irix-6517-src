/*
 * Copyright 1989,1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.9 $
 */

#include <stdio.h>
#include <sm_log.h>

void
skipblank(char **lp)
{
	register char *l;

	l = *lp;
	while ((*l == ' ') || (*l == '\t'))
		l++;
	*lp = l;
}


#define iseol(c)     (c == '\0' || c == '\n' || c == '#')
/*
 * Read a line from a file.
 */
char **
getline(char *line, int maxlinelen, FILE *f)
{
	char *p;
	static char *lp;

	/* skip null and comment lines */
	do {
		if (!fgets(line, maxlinelen, f))
			return(NULL);
	} while (iseol(line[0]));

	/* handle long line */
	p = line;
	for (;;) {
		p += strlen(p);
		if (p >= line+maxlinelen-1)
			break;
		if (!(*--p == '\n' && *--p == '\\'))
			break;
		if (!fgets(p, maxlinelen-(p-line), f))
			break;
		}

	lp = line;
	return(&lp);
}


/* get a numeric value with an optional decimal point
 */
float
getval(char* str, char* msg, char* name, int min, int max, int def)
{
	float val;
	int len;

	if (1 != sscanf(str, "%f%n", &val, &len)
	    || len != strlen(str)
	    || (min <= max
		&& (val < min || val > max))) {
		sm_log(LOG_ERR,0, "%s: bad %s=%s", msg, name, str);
		return def;
	}
	return val;
}


/* get an integer
 */
int
getint(char* str, char* msg, char* name, int min, int max, int def)
{
	float val;
	int len;

	if (1 != sscanf(str, "%d%n", &val, &len)
	    || len != strlen(str)
	    || (min <= max
		&& (val < min || val > max))) {
		sm_log(LOG_ERR,0, "%s: bad %s=%s", msg, name, str);
		return def;
	}
	return val;
}


