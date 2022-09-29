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

#ident "$Id: pmtime.c++,v 2.24 1999/04/30 01:44:04 kenmcd Exp $"

#include <errno.h>
#include <locale.h>
#include "pmapi.h"
#include "impl.h"
#include "../common/tv.h"
#include "../common/timesrvr.h"

#include <Vk/VkApp.h>
#include <Vk/VkErrorDialog.h>
#if defined(sgi)
#include <helpapi/HelpBroker.h>
#endif

#include "../common/message.h"
#include "../archiveTime/VkPCParchiveTime.h"
#include "../liveTime/VkPCPliveTime.h"

static XrmOptionDescRec _cmdLineOptions[] = {
    { "-p", "*pmPortName",	XrmoptionSkipArg,	NULL },
    { "-a", "*pmArchiveMode",	XrmoptionSkipArg,	NULL },
    { "-h", "*pmLiveMode",	XrmoptionSkipArg,	NULL },
    { "-V", "*pmVerbose",	XrmoptionSkipArg,	NULL },
    { "-D", "*pmDebug",		XrmoptionSkipArg,	NULL },
};


static char *usage = "Usage: pmtime [options] -p portname\n\
\n\
Options:\n\
  -a           archive mode\n\
  -h           live mode [default]\n\
  -p portname  use portname for control port socket\n";

static int acceptFirstClient(int, pmTime *);

static char *portname = NULL;
static VkApp *app = NULL;

/*ARGSUSED*/
#ifdef IRIX5_3
void
handleSignal(...)
#else
void
handleSignal(int)
#endif
{
    if (app != NULL)
	app->quitYourself();
    if (portname != NULL)
	unlink(portname);
    exit(0);
}

void
main (int argc, char **argv)
{
    char	*p;
    int		c;
    int		sts = 0;
    int		errflag = 0;
    int		liveMode = 1;
    pmTime	initState;
    int		control;
    int		client;
    char	buf[1024];
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;

    if (argc == 2 && strcmp(argv[1], "-?") == 0) {
	/* fast track the Usage message if that is all the punter wants */
	pmprintf(usage);
	pmflush();
	exit(0);
    }

    /* in a process group of our own to isolate us from our parent's signals */
    setsid();

    signal(SIGINT, handleSignal);
    signal(SIGTERM, handleSignal);

    /* trim cmd name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    // I18N
    setlocale(LC_ALL, "");
    XtSetLanguageProc(NULL, NULL, NULL);

    app = new VkApp("PmTime", &argc, argv, _cmdLineOptions, XtNumber(_cmdLineOptions)); 
    sprintf(buf, "Performance Co-Pilot, Version %s\n\n\n"
		 "Email Feedback: pcp-info@sgi.com", PCP_VERSION);
    app->setVersionString(buf);

    memset(&initState, 0, sizeof(initState));
    while ((c = getopt(argc, argv, "ahD:p:?")) != EOF) {
	switch (c) {

	case 'a':
	    liveMode = 0;
	    break;

	case 'h':
	    liveMode = 1;
	    break;

	case 'p':
	    portname = optarg;
	    break;

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (portname == NULL) {
	pmprintf("%s: -p is a required argument\n", pmProgname);
	errflag++;
    }

    if (errflag) {
	pmprintf(usage);
	pmflush();
	exit(1);
    }

    // create the control port and init the internal state
    control = __pmTimeInit(portname, &initState);
    if (control < 0) {
	sts = control;
	sprintf(buf, 
	"Failed to initialize the time control port \"%s\",\n"
	"probably because the path already exists or a directory name\n"
	"in the path is invalid (the Irix error is : %s)", portname, pmErrStr(sts));
	theErrorDialog->postAndWait(buf);
	exit(1);
    }

    client = acceptFirstClient(liveMode, &initState);
    if (client == -EINVAL) {
	theErrorDialog->postAndWait(MSG_DOWN_REV);
	unlink(portname);
	exit(1);
    }
    else
    if (client < 0) {
	sprintf(buf, 
	"Failed to initialize the first client on control port \"%s\".\n"
	"The Irix error is : %s", portname, pmErrStr(client));
	theErrorDialog->postAndWait(buf);
	unlink(portname);
	exit(1);
    }

    if (initState.delta < 0)
	initState.delta *= -1;

#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL) {
    char a[64], b[64], c[64];
    fprintf(stderr, "pmtime initial settings:\n");

    fprintf(stderr, "\tdelta=%d showdialog=%d window(%.24s .. %.24s)\n",
	initState.delta, initState.showdialog,
	pmCtime(&initState.start.tv_sec, a), pmCtime(&initState.finish.tv_sec, b));

    fprintf(stderr, "\tposition=%.24s tz=\"%s\" tzlabel=\"%s\"\n",
	pmCtime(&initState.position.tv_sec, c),
	initState.tz, initState.tzlabel);
}
#endif

    if (liveMode) {
	//
	// Live mode
	//
	VkPCPliveTime *liveTime = new VkPCPliveTime("PCP Live Time Control");
	if (liveTime->initialize(control, client, &initState) < 0)
	    exit(1);
	liveTime->start();
    }
    else {
	//
	// Archive Mode
	//
	VkPCParchiveTime *archiveTime  = new VkPCParchiveTime("PCP Archive Time Control");
	if (archiveTime->initialize(control, client, &initState) < 0)
	    exit(1);
    }


    app->run();

    exit(0);
}


//
// accept the first client and get the pmTime init state
//
static int
acceptFirstClient(int liveMode, pmTime *initState)
{
    int sts = 0;
    int fromClient, toClient;

    if ((sts = __pmTimeAcceptClient(&toClient, &fromClient, initState)) < 0) {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "%s: failed to accept new client: %s\n", pmProgname, pmErrStr(sts));
#endif
	return sts;
    }

    if ((sts = __pmTimeAddClient(toClient, fromClient)) < 0) {
#ifdef PCP_DEBUG
if (pmDebug & DBG_TRACE_TIMECONTROL)
	fprintf(stderr, "%s: failed to add new client: %s\n", pmProgname, pmErrStr(sts));
#endif
	return sts;
    }

    if (liveMode) {
	// set the position to wall-clock time regardless
	gettimeofday(&initState->position, NULL);
	// .. and for consistency ..
	initState->start = initState->position;
	initState->finish = initState->position;
    }
    else {
	// archive mode
	if (initState->position.tv_sec == 0 && initState->position.tv_usec == 0)
	    initState->position = initState->start;
	else {
	    if (initState->position.tv_sec > initState->finish.tv_sec)
		initState->position = initState->finish;
	}
	initState->vcrmode = (initState->vcrmode & 0xffff0000) | PM_TCTL_VCRMODE_STOP;
    }

    // acknowledge that initialization is complete
    // and the client can start sending messages
    sts = __pmTimeSendPDU(toClient, PM_TCTL_ACK, initState);

    // Register the clients intial params, must be done after the ACK
    __timsrvrRegisterClient(fromClient, initState);

    // .. and tell the client the state of the world
    sts = __pmTimeBroadcast(PM_TCTL_VCRMODE, initState);
    sts = __pmTimeBroadcast(PM_TCTL_TZ, initState);
    sts = __pmTimeBroadcast(PM_TCTL_SET, initState);

    return sts < 0 ? sts : fromClient;
}

