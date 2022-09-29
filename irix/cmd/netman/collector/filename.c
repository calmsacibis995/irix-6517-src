/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	File name constructor
 *
 *	$Revision: 1.1 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include <time.h>
#include <strings.h>
#include <errno.h>

char *
getpath(time_t *start, time_t *end)
{
	static char fn[64];
	struct tm *s, *e, ss;

	s = &ss;
	e = localtime(start);
	*s = *e;
	e = localtime(end);
	if (s->tm_year != e->tm_year) {
		sprintf(fn, "%4d.%02d.%02d@%02d:%02d-%4d.%02d.%02d@%02d:%02d",
			s->tm_year + 1900, s->tm_mon + 1, s->tm_mday,
			s->tm_hour, s->tm_min,
			e->tm_year + 1900, e->tm_mon + 1, e->tm_mday,
			e->tm_hour, e->tm_min);
		return fn;
	}
	if (s->tm_mon != e->tm_mon) {
		sprintf(fn, "%4d/%02d.%02d@%02d:%02d-%02d.%02d@%02d:%02d",
			s->tm_year + 1900,
			s->tm_mon + 1, s->tm_mday, s->tm_hour, s->tm_min,
			e->tm_mon + 1, e->tm_mday, e->tm_hour, e->tm_min);
		return fn;
	}
	if (s->tm_mday != e->tm_mday) {
		sprintf(fn, "%4d/%02d/%02d@%02d:%02d-%02d@%02d:%02d",
			s->tm_year + 1900, s->tm_mon + 1,
			s->tm_mday, s->tm_hour, s->tm_min,
			e->tm_mday, e->tm_hour, e->tm_min);
		return fn;
	}
	sprintf(fn, "%4d/%02d/%02d/%02d:%02d-%02d:%02d",
			s->tm_year + 1900, s->tm_mon + 1, s->tm_mday,
			s->tm_hour, s->tm_min,
			e->tm_hour, e->tm_min);
	return fn;
}

char *
getdirectory(time_t *start, time_t *end)
{
	char *e, *p;

	p = getpath(start, end);
	e = strrchr(p, '/');
	if (e == 0)
		return 0;
	*e = '\0';
	return p;
}

char *
getfile(time_t *start, time_t *end)
{
	char *e, *p;

	p = getpath(start, end);
	e = strrchr(p, '/');
	if (e == 0)
		return p;
	return e;
}

int
createdirectory(char *dir)
{
	unsigned int err;
	char *d;

	if (dir == 0)
		return 1;

	if (*dir == '/')
		d = dir + 1;
	else
		d = dir;
	for (d = strchr(d, '/'); d != 0; d = strchr(d, '/')) {
		*d = '\0';
		err = mkdir(dir, 0755) != 0 && errno != EEXIST;
		*d++ = '/';
		if (err)
			return 0;
	}

	return mkdir(dir, 0755) == 0 || errno == EEXIST;
}
