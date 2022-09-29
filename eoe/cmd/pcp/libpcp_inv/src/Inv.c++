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

#ident "$Id: Inv.c++,v 1.7 1998/03/10 01:39:12 kenmcd Exp $"

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <libgen.h>
#include <locale.h>
#include <iostream.h>
#include <Vk/VkApp.h>
#include <Vk/VkWarningDialog.h>
#include <Vk/VkErrorDialog.h>
#include <Vk/VkFatalErrorDialog.h>
#include <Inventor/Xt/SoXt.h>

#include "pmapi.h"
#include "impl.h"
#include "Inv.h"
#include "String.h"
#include "App.h"

//
// Globals
//

const uint_t		theBufferLen = 2048;
char			theBuffer[theBufferLen];
float			theScale = 1.0;
OMC_String		theAppName = "Undefined";
const OMC_String	theDefaultFlags = "A:a:CD:h:l:n:O:p:S:t:T:x:Z:z?";
OMC_Bool		theStderrFlag = OMC_false;
INV_App			*theApp = NULL;
OMC_String		theLoggerLaunch = "/var/pcp/config/pmlaunch/_pmlogger";

int
INV_setup(const char *appname, int *argc, char **argv, 
	  XrmOptionDescRec *cmdopts, int numopts, INV_TermCB termCB)
{
    char	*env;
    int		dbg;
    int		i;

    pmProgname = basename(argv[0]);
    theAppName = appname;

    // Find if pmDebug should be set
    for (i = 1; i < *argc; i++) {
	if (strcmp(argv[i], "-D") == 0)
	    if (i < *argc - 1)
		env = argv[i+1];
	    else {
	    	pmprintf("%s: Error: -D requires an argument\n", pmProgname);
		return -1;
		/*NOTREACHED*/
	    }
	else if (strncmp(argv[i], "-D", 2) == 0)
	    env = argv[i] + 2;
	else
	    continue;

	dbg = __pmParseDebug(env);
	if (dbg < 0) {
	    pmprintf("%s: Error: Unrecognized debug flag specification \"%s\"\n",
		     pmProgname, env);
	    return -1;
	    /*NOTREACHED*/
	}
	
	pmDebug |= dbg;
    }

    // I18N
    setlocale(LC_ALL, "");
    XtSetLanguageProc(NULL, NULL, NULL);

    theApp = new INV_App((char *)appname, argc, argv, cmdopts, numopts, termCB);
    SoXt::init(theApp->baseWidget());

    env = getenv("PCP_STDERR");
    if (env == NULL) {
    	env = getenv("PCP_USE_STDERR");
	if (env == NULL)
	    theStderrFlag = OMC_false;
       	else
    	    theStderrFlag = OMC_true;
    }
    else if (strcmp(env, "DISPLAY") == 0)
	theStderrFlag = OMC_false;
    else {
	theStderrFlag = OMC_true;
    	if (strlen(env))
	    cerr << pmProgname 
	    	 << ": Warning: PCP_STDERR is set to a file" << endl
		 << pmProgname
	         << ": Error messages after initial setup will go to stderr"
		 << endl;
    }

    return 0;
}

int
INV_warningMsg(const char *file, int line, const char *msg, ...)
{
    int sts = VkDialogManager::OK;

    va_list arg;
    va_start(arg, msg);

    int pos = sprintf(theBuffer, "%s: Warning: ", pmProgname);
    pos += vsprintf(theBuffer+pos, msg, arg);
    sprintf(theBuffer+pos, "\n");

#ifdef PCP_DEBUG
    if (pmDebug && !theStderrFlag)
    	cerr << file << ":" << line << ": " << theBuffer << endl;
#endif

    if (theStderrFlag)
    	cerr << theBuffer;
    else
	sts = theWarningDialog->postAndWait(theBuffer, TRUE);
    va_end(arg);

    return sts;
}

int
INV_errorMsg(const char *file, int line, const char *msg, ...)
{
    int sts = VkDialogManager::OK;

    va_list arg;
    va_start(arg, msg);

    int pos = sprintf(theBuffer, "%s: Error: ", pmProgname);
    pos += vsprintf(theBuffer+pos, msg, arg);
    sprintf(theBuffer+pos, "\n");

#ifdef PCP_DEBUG
    if (pmDebug && !theStderrFlag)
    	cerr << file << ":" << line << ": " << theBuffer << endl;
#endif

    if (theStderrFlag)
    	cerr << theBuffer;
    else
	sts = theErrorDialog->postAndWait(theBuffer, TRUE);
    va_end(arg);

    return sts;
}

int
INV_fatalMsg(const char *file, int line, const char *msg, ...)
{
    int sts = VkDialogManager::OK;

    va_list arg;
    va_start(arg, msg);

    int pos = sprintf(theBuffer, "%s: Fatal: ", pmProgname);
    pos += vsprintf(theBuffer+pos, msg, arg);
    sprintf(theBuffer+pos, "\n");

#ifdef PCP_DEBUG
    if (pmDebug && !theStderrFlag)
    	cerr << file << ":" << line << ": " << theBuffer << endl;
#endif

    if (theStderrFlag) {
    	cerr << theBuffer;
	exit(1);
    }
    else
	sts = theFatalErrorDialog->postAndWait(theBuffer, TRUE);
    va_end(arg);

    return sts;
}
