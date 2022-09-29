/*
 * Copyright 1997, Silicon Graphics, Inc.
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

#ident "$Id: oview.c++,v 1.18 1999/04/30 04:03:45 kenmcd Exp $"

#include <iostream.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <Vk/VkApp.h>
#include <Vk/VkErrorDialog.h>
#include <Inventor/Xt/SoXt.h>

#include<signal.h>

#include "pmapi_dev.h"
#include "oview.h"
#include "App.h"
#include "View.h"
#include "ModList.h"
#include "Record.h"
#include "Scene.h"
#include "Sprout.h"

#ifndef APPNAME
#define APPNAME	"OView"
#endif

void usage();

OMC_String	theConfigFile;
uint_t		theDetailLevel = 3;

static XrmOptionDescRec theCmdLineOptions[] = {
    { "-A", "*pmAlignment",	XrmoptionSkipArg,	NULL },
    { "-a", "*pmArchive",	XrmoptionSkipArg,	NULL },
    { "-c", "*pmConfigFile",	XrmoptionSkipArg,	NULL },
    { "-D", "*pmDebug",		XrmoptionSkipArg,      	NULL },
    { "-h", "*pmHost",		XrmoptionSkipArg,	NULL },
    { "-l", "*pmDetail",	XrmoptionSkipArg,	NULL },
    { "-n", "*pmNamespace",	XrmoptionSkipArg,	NULL },
    { "-O", "*pmOffset",	XrmoptionSkipArg,	NULL },
    { "-p", "*pmPortName",	XrmoptionSkipArg,	NULL },
    { "-S", "*pmStart",		XrmoptionSkipArg,	NULL },
    { "-t", "*pmInterval",	XrmoptionSkipArg,	NULL },
    { "-T", "*pmEnd",		XrmoptionSkipArg,	NULL },
    { "-V", "*pmVersion",	XrmoptionSkipArg,	NULL },
    { "-Z", "*pmTimezone",	XrmoptionSkipArg,	NULL },
    { "-title", "*title",	XrmoptionSepArg,	NULL },
};

int
parseArgs()
{
    int		c;
    int		sts = 0;
    char	*endnum = NULL;

    while ((c = theView->parseArgs()) != EOF) {
	switch (c) {
	case 'c':
	    theConfigFile = optarg;
	    break;
	case 'l':
	    theDetailLevel = (uint_t)strtol(optarg, &endnum, 10);
	    if ((endnum != NULL && *endnum != '\0') || (theDetailLevel > 2)) {
	    	pmprintf("%s: Error: -l requires either 0, 1 or 2\n",
			 pmProgname);
		sts--;
	    }
	    break;
	default:
	    sts--;
	}
    }

    if (sts == 0) {
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    cerr << "parseaArgs: Successful, configFile = " << theConfigFile
		 << ", detail = " << theDetailLevel << endl;
#endif
	return theView->status();
    }
    else
	return sts;
}

static int
genInventor()
{
    theScene = new Scene(theAppName, theApp->baseWidget());
    theSprout = new Sprout;
    theSprout->addMenu(theView->optionsMenu());
    return theScene->genInventor();
}

static void
genLogConfig(INV_Record &rec)
{
    const char *src = theSource.which()->host().ptr();
    rec.addOnce(src, "hinv");
    rec.add(src, "hw.router");
    rec.add(src, "origin.node");
    rec.add(src, "kernel.percpu.cpu");
    rec.add(src, "origin.numa.migr.intr.total");

    if (theScene && 
	strcmp(theScene->metricNode(), "origin.numa.migr.intr.total") != 0) 
	rec.add(src, theScene->metricNode());
}

// Exit properly when interrupted, allows tools like pixie to work properly
#if defined(IRIX5_3)
static void
catch_int(...)
#else
/*ARGSUSED*/
static void
catch_int(int sig)
#endif
{
    pmflush();
    theApp->terminate(1);
}

void 
main (int argc, char *argv[])
{
    OMC_String	verStr;
    OMC_String	flags = theDefaultFlags;

    if (argc == 2 && strcmp(argv[1], "-?") == 0) {
	/* fast track the Usage message if that is all the punter wants */
	usage();
	pmflush();
	exit(0);
    }

    flags.append("c:l:");

    __pmSetAuthClient();

    if (INV_setup(APPNAME, &argc, argv, theCmdLineOptions, 
		  XtNumber(theCmdLineOptions), NULL) < 0) {
	usage();
	pmflush();
	exit(1);
    }

    signal(SIGINT, catch_int);

    // Create the top level windows
    theView = new INV_View(argc, argv, flags, NULL, 
			   genLogConfig, OMC_false, OMC_false, OMC_false);

    if (theView->status() < 0) {
	if (theView->checkConfigOnly() == OMC_false)
	    usage();
	pmflush();
	theApp->terminate(1);
    }

    theModList = new INV_ModList(theView->viewer(), 
			     	 &INV_View::selectionCB,
			         &Sprout::selCB,
				 &Sprout::deselCB);

    if (parseArgs() < 0) {
	if (theView->checkConfigOnly() == OMC_false)
	    usage();
	pmflush();
	theApp->terminate(1);
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "main: Args and config parsed, about to show scene" << endl;
#endif

    theView->parseConfig(genInventor);
    if (theView->status() < 0) {
    	pmflush();
	theApp->terminate(1);
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	cerr << "main: Modulated Object List:" << endl;
        cerr << *theModList << endl;
    }
#endif

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	cerr << "main: Displaying window" << endl;
#endif

    if (theView->view() == OMC_false) {
	pmflush();
	theApp->terminate(1);
    }

    pmflush();
    if (theView->checkConfigOnly())
	theApp->terminate(0);

//
// Specify product information
//

    verStr.append("Performance Co-Pilot, Version ");
    verStr.append(PCP_VERSION);
    verStr.append("\n\n\nEmail Feedback: pcp-info@sgi.com");
    theApp->setVersionString(verStr.ptr());

//
// Forked processes (ie. from the launch menu) should not keep the X file
// descriptor open
//
    fcntl(ConnectionNumber(theApp->display()), F_SETFD, 1);

    theApp->run();
}

void
usage()
{
    pmprintf(
"Usage: oview [options]\n\
\n\
Options:\n\
  -A align       align sample times on natural boundaries\n\
  -a archive     metrics source is a PCP log archive\n\
  -C             check configuration file and exit\n\
  -c configfile  topology configuration file\n\
  -h host        metrics source is PMCD on host\n\
  -l level       level of detail, either 0, 1 or 2 [default varies]\n\
  -n pmnsfile    use an alternative PMNS\n\
  -O offset      initial offset into the time window\n\
  -p port        port name for connection to existing time control\n\
  -S starttime   start of the time window\n\
  -T endtime     end of the time window\n\
  -t interval    sample interval [default 2 seconds]\n\
  -x version     use pmlaunch(5) version [default 2.0]\n\
  -Z timezone    set reporting timezone\n\
  -z             set reporting timezone to local time of metrics source\n\n\
  -display  display-string\n\
  -geometry geometry-string\n\
  -name     name-string\n\
  -xrm      resource [-xrm ...]\n");
}
