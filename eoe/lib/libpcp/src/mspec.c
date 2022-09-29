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
 */

/*
 * parse uniform metric spec syntax
 */

#ident "$Id: mspec.c,v 2.7 1997/08/21 04:50:20 markgw Exp $"

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <malloc.h>
#include "pmapi.h"
#include "impl.h"

/****************************************************************************
 * local functions
 ****************************************************************************/

/* memory allocation */
static void *
parseAlloc(size_t need)
{
    void    *tmp;

    if ((tmp = malloc(need)) == NULL)
	__pmNoMem("pmParseMetricSpec", need, PM_FATAL_ERR);
    return tmp;
}

/* Report syntax error */
static void
parseError(const char *spec, const char *point, char *msg, char **rslt)
{
    int		need = 2 * (int)strlen(spec) + 1 + 6 + (int)strlen(msg) + 2;
    const char	*p;
    char	*q;

    if ((q = (char *) malloc(need)) == NULL)
	__pmNoMem("pmParseMetricSpec", need, PM_FATAL_ERR);
    *rslt = q;
    for (p = spec; *p != '\0'; p++)
	*q++ = *p;
    *q++ = '\n';
    for (p = spec; p != point; p++)
	*q++ = isgraph(*p) ? ' ' : *p;
    sprintf(q, "^ -- %s\n", msg);
}


/****************************************************************************
 * exported functions
 ****************************************************************************/

int		/* 0 -> ok, PM_ERR_GENERIC -> error */
pmParseMetricSpec(
    const char *spec,		/* parse this string */
    int isarch,			/* default source: 0 -> host, 1 -> archive */
    char *source,		/* name of default host or archive */
    pmMetricSpec **rslt,	/* result allocated and returned here */
    char **errmsg)		/* error message */
{
    pmMetricSpec   *msp = NULL;
    const char	    *scan;
    const char	    *mark;
    const char	    *h_start = NULL;	/* host name */
    const char	    *h_end = NULL;
    const char	    *a_start = NULL;	/* archive name */
    const char	    *a_end;
    const char	    *m_start = NULL;	/* metric name */
    const char	    *m_end;
    const char	    *i_start = NULL;	/* instance names */
    const char	    *i_end;
    char	    *i_str = NULL;	/* temporary instance names */
    char	    *i_scan;
    int		    ninst;		/* number of instance names */
    char	    *push;
    const char	    *pull;
    int		    length;
    int		    i;

    scan = spec;
    while (isspace(*scan))
	scan++;
    mark = scan;

    /* delimit host name */
    while (*scan != ':' && ! isspace(*scan) && *scan != '\0' && *scan != '[') {
	if (*scan == '\\' && *(scan+1) != '\0')
	    scan++;
	scan++;
    }
    h_end = scan;
    while (isspace(*scan))
	scan++;
    if (*scan == ':') {
	h_start = mark;
	if (h_start == h_end) {
	    parseError(spec, scan, "host name expected", errmsg);
	    return PM_ERR_GENERIC;
	}
	scan++;
    }

    /* delimit archive name */
    else {
	scan = mark;
	while (*scan != '\0' && *scan != '[') {
	    if (*scan == '\\' && *(scan+1) != '\0')
		scan++;
	    else if  (*scan == '/') {
		a_start = mark;
		a_end = scan;
	    }
	    scan++;
	}
	if (a_start) {
	    if (a_start == a_end) {
		parseError(spec, a_start, "archive name expected", errmsg);
		return PM_ERR_GENERIC;
	    }
	    scan = a_end + 1;
	}
	else
	    scan = mark;
    }

    while (isspace(*scan))
	scan++;
    mark = scan;

    /* delimit metric name */
    m_start = scan;
    while (! isspace(*scan) && *scan != '\0' && *scan != '[') {
	if (*scan == ':' || *scan == '/' || *scan == ']' || *scan == ',') {
	    parseError(spec, scan, "unexpected character in metric name", errmsg);
	    return PM_ERR_GENERIC;
	}
	if (*scan == '\\' && *(scan+1) != '\0')
	    scan++;
	scan++;
    }
    m_end = scan;
    if (m_start == m_end) {
	parseError(spec, m_start, "performance metric name expected", errmsg);
	return PM_ERR_GENERIC;
    }

    while (isspace(*scan))
	scan++;

    /* delimit instance names */
    if (*scan == '[') {
	scan++;
	while (isspace(*scan))
	    scan++;
	i_start = scan;
	for (;;) {
	    if (*scan == '\\' && *(scan+1) != '\0')
		scan++;
	    else if (*scan == ']')
		break;
	    else if (*scan == '\0') {
		parseError(spec, scan, "closing ] expected", errmsg);
		return PM_ERR_GENERIC;
	    }
	    scan++;
	}
	i_end = scan;
	scan++;
    }

    /* check for rubbish at end of string */
    while (isspace(*scan))
	scan++;
    if (*scan != '\0') {
	parseError(spec, scan, "unexpected extra characters", errmsg);
	return PM_ERR_GENERIC;
    }

    /* count instance names and make temporary copy */
    ninst = 0;
    if (i_start != NULL) {
	i_str = (char *) parseAlloc(i_end - i_start + 1);

	/* count and copy instance names */
	scan = i_start;
	i_scan = i_str;
	while (scan < i_end) {

	    /* copy single instance name */
	    ninst++;
	    if (*scan == '\"') {
		scan++;
		for (;;) {
		    if (scan >= i_end) {
			parseError(spec, scan, "closing \" expected", errmsg);
			if (msp)
			    pmFreeMetricSpec(msp);
			if (i_str)
			    free(i_str);
			return PM_ERR_GENERIC;
		    }
		    if (*scan == '\\')
			scan++;
		    else if (*scan == '\"')
			break;
		    *i_scan++ = *scan++;
		}
		scan++;
	    }
	    else {
		while (! isspace(*scan) && *scan != ',' && scan < i_end) {
		    if (*scan == '\\')
			scan++;
		    *i_scan++ = *scan++;
		}
	    }
	    *i_scan++ = '\0';

	    /* skip delimiters */
	    while ((isspace(*scan) || *scan == ',') && scan < i_end)
		scan++;
	}
	i_start = i_str;
	i_end = i_scan;
    }

    /* single memory allocation for result structure */
    length = (int)(sizeof(pmMetricSpec) +
             ((ninst > 1) ? (ninst - 1) * sizeof(char *) : 0) +
	     ((h_start) ? h_end - h_start + 1 : 0) +
	     ((a_start) ? a_end - a_start + 1 : 0) +
	     ((m_start) ? m_end - m_start + 1 : 0) +
	     ((i_start) ? i_end - i_start + 1 : 0));
    msp = (pmMetricSpec *)parseAlloc(length);

    /* strings follow pmMetricSpec proper */
    push = ((char *) msp) +
	   sizeof(pmMetricSpec) + 
	   ((ninst > 1) ? (ninst - 1) * sizeof(char *) : 0);

    /* copy metric name */
    msp->metric = push;
    pull = m_start;
    while (pull < m_end) {
	if (*pull == '\\' && (pull+1) < m_end)
	    pull++;
	*push++ = *pull++;
    }
    *push++ = '\0';

    /* copy host name */
    if (h_start != NULL) {
	msp->isarch = 0;
	msp->source = push;
	pull = h_start;
	while (pull < h_end) {
	    if (*pull == '\\' && (pull+1) < h_end)
		pull++;
	    *push++ = *pull++;
	}
	*push++ = '\0';
    }

    /* copy archive name */
    else if (a_start != NULL) {
	msp->isarch = 1;
	msp->source = push;
	pull = a_start;
	while (pull < a_end) {
	    if (*pull == '\\' && (pull+1) < a_end)
		pull++;
	    *push++ = *pull++;
	}
	*push++ = '\0';
    }

    /* take default host or archive */
    else {
	msp->isarch = isarch;
	msp->source = source;
    }

    /* instance names */
    msp->ninst = ninst;
    pull = i_start;
    for (i = 0; i < ninst; i++) {
	msp->inst[i] = push;
	do
	    *push++ = *pull;
	while (*pull++ != '\0');
    }

    if (i_str)
	free(i_str);
    *rslt = msp;
    return 0;
}
