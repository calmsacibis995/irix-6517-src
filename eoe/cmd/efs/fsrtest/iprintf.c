/*
 * ktst info printf
 *
 * proposed usage conventions
 *
 *	vlevel=	0	Always print.
 *		1	Informative output, same for each run. Give interactive
 *			unfamiliar user warm fuzzies that something's happening.
 *		2	More detailed output, not necessarily same each run.
 *			Print values, times particular to this test run.
 *		3	Even more detailed.  Probably not useful unless
 *			the user has source.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "ktst.h"

/*
 * fprintf to VFILE[0-9] if VERBOSE >= 'vlevel'
 * VFILE* defaults to stdout
 * VERBOSE defaults to 0
 * XXX no error checking on 'vlevel', 'vchan', 'VERBOSE' or 'VFILE[0-9]'
 */

static venv = -1;
static FILE *vfp[10];

void
iprintf(int vlevel, int vchan, char *fmt, ... )
{
	va_list ap;
	char *cp, *cp0;

	if (venv == -1) {
		char *cp = getenv("VERBOSE");
		if (cp == NULL)
			venv = 0;
		else
			venv = atoi(cp);
	}

	if (vfp[vchan] == 0) {
		cp0 = "VFILEX";
		cp0[5] = '0' + vchan;	/* X is [0-9] */
		cp = getenv(cp0);
		if (cp == NULL)
			vfp[vchan] = stdout;
		else {
			vfp[vchan] = fopen(cp, "w");
			if (vfp[vchan] == NULL) {
				fprintf(stderr,
				"WARNING: fopen(%s) failed, using stdout\n",cp);
				vfp[vchan] = stdout;
			}
		}
	}

	if (venv >= vlevel) {
		va_start(ap, fmt);
		vfprintf(vfp[vchan], fmt, ap);
		va_end(ap);
	}
}
