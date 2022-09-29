//
// pmrun - desktop launch utility for pcp programs
//
// Copyright 1997, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// UNPUBLISHED -- Rights reserved under the copyright laws of the United
// States.   Use of a copyright notice is precautionary only and does not
// imply publication or disclosure.
//
// U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
// in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
// in similar or successor clauses in the FAR, or the DOD or NASA FAR
// Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
// 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
//
// THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
// INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
// DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
// PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
// GRAPHICS, INC.
//

#include <libgen.h>
#include <locale.h>
#include <Vk/VkApp.h>
#include "RunDialog.h"
#include "pmapi_dev.h"

VkApp	*app = NULL;
char	*header = NULL;
char	*trailer = NULL;

static XrmOptionDescRec _options[] = {
    { "-a", "*pmArchive",	XrmoptionSkipArg,	NULL },
    { "-D", "*pmDebug",		XrmoptionSkipArg,	NULL },
    { "-h", "*pmHost",		XrmoptionSkipArg,	NULL },
    { "-S", "*pmStart",		XrmoptionSkipArg,	NULL },
    { "-t", "*pmInterval",	XrmoptionSkipArg,	NULL },
    { "-T", "*pmEnd",		XrmoptionSkipArg,	NULL },
};

static void null_warn(String) { }

static void
usage(void)
{
    pmprintf(
"Usage: pmrun [options ...] command [other args ...]\n\n\
Options\n\
  -a archive    archive name entry\n\
  -h host       host name entry\n\
  -S start      start time entry\n\
  -T finish     finish time entry\n\
  -t delta      sample interval entry, in pmParseInterval(3) format\n\
  -x text       prefix command line with text\n\
  -y text       append text to command line\n");
    pmflush();
}

void
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    char	*archive = NULL;
    char	*host = NULL;
    char	*start = NULL;
    char	*finish = NULL;
    double	delta = -1.0;
    char	*endnum;
    char	*displayenv;
    RunDialog	*dialog;
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;
    struct timeval	t;

    if (argc == 2 && strcmp(argv[1], "-?") == 0) {
	/* fast track the Usage message if that is all the punter wants */
	usage();
	exit(0);
    }

    pmProgname = basename(argv[0]);
    __pmSetAuthClient();

    // I18N
    setlocale(LC_ALL, "");
    XtSetLanguageProc(NULL, NULL, NULL);

    XtSetWarningHandler(null_warn);

    // Create an application object
    app = new VkApp("PmRun", &argc, argv, _options, XtNumber(_options)); 

    while ((c = getopt(argc, argv, "a:h:t:D:S:T:x:y:?")) != EOF) {
	switch(c) {

	case 'a':	// archive name
	    archive = optarg;
	    break;

	case 'h':	// host name
	    host = optarg;
	    break;

	case 't':	// delta seconds (double)
	    if (pmParseInterval(optarg, &t, &endnum) < 0) {
		pmprintf("%s: illegal -t argument\n%s", pmProgname, endnum);
		free(endnum);
		errflag++;
	    }
	    else
		delta = t.tv_sec + (t.tv_usec / 1000000.0);
	    break;

	case 'D':	// debug flag
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'S':	// start time
	    start = optarg;
	    break;

	case 'T':	// terminate time
	    finish = optarg;
	    break;

	case 'x':
	    header = optarg;
	    break;

	case 'y':
	    trailer = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (!errflag && optind == argc) {	// no run program name
	pmprintf("%s: too few arguments\n", pmProgname);
	errflag++;
    }

    if (errflag) {
	usage();
	exit(1);
    }

    if ((displayenv = DisplayString(app->display())) != NULL) {
	endnum = (char *)malloc(strlen(displayenv) + 16);
	sprintf(endnum, "DISPLAY=%s", displayenv);
	putenv(endnum);
    }

    // Create the top level window
    dialog = new RunDialog("PmRun", (const char *)host, (const char *)archive,
	delta, (const char *)start, (const char *)finish,
	argc-optind, (const char **)&argv[optind]);
    dialog->show();
    app->run();
}
