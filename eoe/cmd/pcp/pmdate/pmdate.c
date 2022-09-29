/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 *
 * Display offset date
 *
 * $Id: pmdate.c,v 1.1 1999/04/28 10:06:17 kenmcd Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <time.h>

#define usage "Usage: pmdate { +valueS | -valueS } ... format\n\
\n\
where the scale \"S\" is one of: S (seconds), M (minutes), H (hours), \n\
d (days), m (months) or y (years)\n"

main(int argc, char *argv[])
{
    time_t	now;
    int		need;
    char	*buf;
    char	*p, *pmProgname;
    char	*pend;
    struct tm	*tmp;
    int		sgn;
    int		val;
    int		mo_delta = 0;
    int		yr_delta = 0;

    /* trim command name of leading directory components */
    for (p = pmProgname = argv[0]; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    if (argc < 2) {
	fprintf(stderr, usage);
	exit(1);
    }

    if (strcmp(argv[1], "-?") == 0) { 
	fprintf(stderr, usage);
	exit(1);
    }

    time(&now);

    while (argc > 2) {
	p = argv[1];
	if (*p == '+')
	    sgn = 1;
	else if (*p == '-')
	    sgn = -1;
	else {
	    fprintf(stderr, "%s: incorrect leading sign for offset (%s), must be \"+\" or \"-\"\n",
		       pmProgname, argv[1]);
	    exit(1);
	}
	p++;

	val = (int)strtol(p, &pend, 10);
	switch (*pend) {
	    case 'S':
		now += sgn * val;
		break;
	    case 'M':
		now += sgn * val * 60;
		break;
	    case 'H':
		now += sgn * val * 60 * 60;
		break;
	    case 'd':
		now += sgn * val * 24 * 60 * 60;
		break;
	    case 'm':
		mo_delta += sgn*val;
		break;
	    case 'y':
		yr_delta += sgn*val;
		break;
	    case '\0':
		fprintf(stderr, "%s: missing scale after offset (%s)\n", pmProgname, argv[1]);
		exit(1);
	    case '?':
		fprintf(stderr, usage);
		exit (1);
	    default:
		fprintf(stderr, "%s: unknown scale after offset (%s)\n", pmProgname, argv[1]);
		exit(1);
	}

	argv++;
	argc--;
    }

    tmp = localtime(&now);

    if (yr_delta) {
	/*
	 * tm_year is years since 1900 and yr_delta is relative (not
	 * absolute), so this is Y2K safe
	 */
	tmp->tm_year += yr_delta;
    }
    if (mo_delta) {
	/*
	 * tm_year is years since 1900 and the tm_year-- and
	 * tm_year++ is adjusting for underflow and overflow in
	 * tm_mon as a result of relative month delta, so this
	 * is Y2K safe
	 */
	tmp->tm_mon += mo_delta;
	while (tmp->tm_mon < 0) {
	    tmp->tm_mon += 12;
	    tmp->tm_year--;
	}
	while (tmp->tm_mon > 12) {
	    tmp->tm_mon -= 12;
	    tmp->tm_year++;
	}
    }

    /*
     * Note:    256 is _more_ than enough to accommodate the longest
     *		value for _every_ %? lexicon that strftime() understands
     */
    need = strlen(argv[1]) + 256;
    if ((buf = (char *)malloc(need)) == NULL) {
	fprintf(stderr, "%s: malloc failed\n", pmProgname);
	exit(1);
    }

    if (strftime(buf, need, argv[1], tmp) == 0) {
	fprintf(stderr, "%s: format too long\n", pmProgname);
	exit(1);
    }
    else {
	buf[need-1] = '\0';
	printf("%s\n", buf);
	exit(0);
    }

    /*NOTREACHED*/
}
