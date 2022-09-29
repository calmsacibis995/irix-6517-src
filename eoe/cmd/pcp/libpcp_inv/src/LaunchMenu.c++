
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

#ident "$Id: LaunchMenu.c++,v 1.3 1997/10/24 05:59:45 markgw Exp $"

#include <stdlib.h>
#include <string.h>
#include <iostream.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>
#include <Vk/VkMenuItem.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkErrorDialog.h>

#include "pmapi.h"
#include "impl.h"
#include "Inv.h"
#include "Window.h"
#include "Launch.h"
#include "LaunchMenu.h"
#include "View.h"

#define GLOBAL_RC	"pmlaunchrc"
#define LOCAL_RC	".pcp/pmlaunch/pmlaunchrc"

extern int errno;

INV_LaunchMenu::~INV_LaunchMenu()
{
}

INV_LaunchMenu::INV_LaunchMenu(VkSubMenu *menu)
: _menuItems(), _menu(menu)
{
    char const	*path = INV_Launch::launchPath();
    char const	*home = getenv("HOME");
    char	*globalpath = NULL;
    char	*localpath = NULL;
    struct stat	buf;
    int		sts = 0;

    if (path != NULL) {
	globalpath = (char *)malloc(strlen(path) + strlen(GLOBAL_RC) + 4);
	sprintf(globalpath, "%s/%s", path, GLOBAL_RC);
	sts = stat(globalpath, &buf);
	
	if (sts >= 0) {
	    parseConfig(globalpath);
	}
#ifdef PCP_DEBUG
	else
	    if (pmDebug & DBG_TRACE_APPL1)
	    	cerr <<"INV_LaunchMenu::INV_LaunchMenu: " << globalpath
		     << ": " << strerror(errno) << endl;
#endif
	free(globalpath);
    }

    if (home != NULL) {
	localpath = (char *)malloc(strlen(home) + strlen(LOCAL_RC) + 4);
	sprintf(localpath, "%s/%s", home, LOCAL_RC);
	sts = stat(localpath, &buf);

	if (sts >= 0) {
	    parseConfig(localpath);
	}
#ifdef PCP_DEBUG
	else
	    if (pmDebug & DBG_TRACE_APPL1)
	    	cerr <<"INV_LaunchMenu::INV_LaunchMenu: " << localpath
		     << ": " << strerror(errno) << endl;
#endif
	free(localpath);
    }

    // we add the actual VK menu items after creating menu list
    _menuItems.addVkMenuItems(menu);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_LaunchMenu::INV_LaunchMenu: menu is: " 
	     << _menuItems << endl;
#endif
}


void
INV_LaunchMenu::parseConfig(char const* path)
{
    INV_MenuItem	*item = NULL;
    FILE		*file = NULL;
    char		*p = NULL;
    int			line = 0;
    int			length = 0;
    
    file = fopen(path, "r");

    if (file == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_LaunchMenu::parseConfig: Unable to open "
	    	 << path << ": " << strerror(errno) << endl;
#endif
	return;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	cerr << "INV_LaunchMenu::parseConfig: About to parse " << path << endl;
#endif

    while (!feof(file)) {
	line++;
	fgets(theBuffer, theBufferLen, file);
	length = strlen(theBuffer);
	if (length == 0)
	    continue;
	if (theBuffer[0] == '\n' || theBuffer[0] == '#')
	    continue;

	// Find end of menu item
	for (p = theBuffer; *p != ' ' && *p != '\t' && *p != '\n' && *p != '\0'; p++);
	if (*p == '\0')
	    continue;
	*p = '\0';

	// Find start of script name
	for (p++; *p == ' ' && *p == '\t'; p++);
	if (*p == '\n' || *p == '\0') {
	    INV_warningMsg(_POS_, 
		       "Line %d of \"%s\": Menu text \"%s\" has no script name",
		       line, path, theBuffer);
	    continue;
	}
	
	item = new INV_MenuItem(theBuffer, p, OMC_false);

	_menuItems.append(item);

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    cerr << "INV_LaunchMenu::parseConfig: Added item: " 
	         << *item << endl;
#endif
    }
    
}

void 
INV_LaunchMenu::callback(Widget, XtPointer clientdata, XtPointer)
{
    int			sts;
    INV_MenuItem	*item = (INV_MenuItem *)clientdata;

    sts = theView->launch(item->script());

    if (sts < 0)
	theErrorDialog->postAndWait(strerror(-sts));
}
