/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	MIB Browser Main
 *
 *	$Revision: 1.11 $
 *	$Date: 1996/02/26 01:28:06 $
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

#include <tuCallBack.h>
#include <tuFilePrompter.h>
#include <tuGadget.h>
#include <tuResourceDB.h>
#include <tuScreen.h>
#include <tuWindow.h>
#include <tuVisual.h>
#include <tuXExec.h>
#include <dialog.h>

#include <helpapi/HelpBroker.h>

#include "browser.h"

#define X_ORIGIN	350
#define Y_ORIGIN	33

Browser *browser;
tuXExec *exec;
tuFilePrompter *prompter;


static tuResourceItem opts[] = {
    { "node", 0, tuResString, "", offsetof(AppResources, node), },
    { "community", 0, tuResString, "public", offsetof(AppResources, community), },
    { "retries", 0, tuResLong, "3", offsetof(AppResources, retries), },
    { "timeout", 0, tuResLong, "5", offsetof(AppResources, timeout), },
    { "help", 0, tuResBool, "False", offsetof(AppResources, help), },
    0,
};



void
main(int argc, char* argv[])
{
    tuResourceDB *rdb = new tuResourceDB;
    AppResources *ar = new AppResources;

    char* instanceName = 0;
    char* className = "Browser"; // xxx
 
    tuScreen* screen = rdb->getAppResources(ar, args, nargs, opts,
			&instanceName, className,
			&argc, argv);

    // Initialize SGIHelp
    SGIHelpInit (rdb->getDpy(), className, ".");

    tuColorMap* cmap = screen->getDefaultColorMap();


    // Create a browser main window
    browser = new Browser("Browser", cmap, rdb, ar, instanceName, className);
    exec = browser->getExec();

    if (browser->getDialogBox()->getDialogType() == tuAction)
	browser->run();

    if (ar->help) {
	    browser->usage();
    } else {
	browser->bind();
	browser->resizeToFit();
   
	prompter = new tuFilePrompter("prompter", (tuTopLevel*)browser);
	prompter->setName("Save As");
	prompter->bind();
	prompter->resizeToFit();

        browser->setInitialOrigin(X_ORIGIN, Y_ORIGIN);
	browser->map();
    }
    
    browser->run();
}




