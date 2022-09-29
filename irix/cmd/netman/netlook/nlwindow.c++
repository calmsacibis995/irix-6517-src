/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Generic Window
 *
 *	$Revision: 1.3 $
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

#include <tuGadget.h>
#include <tuTopLevel.h>
#include <tuWindow.h>
#include <X11/cursorfont.h>
#include <string.h>

#include "nlwindow.h"
#include "viewgadget.h"
#include "mapgadget.h"
#include "snoopgadget.h"
#include "netnodegadget.h"
#include "trafficgadget.h"
#include "hidegadget.h"
#include "selectiongadget.h"

nlWindow::nlWindow(NetLook *nl, const char *instanceName, tuColorMap *cmap,
		   tuResourceDB *db, char *progName, char *progClass,
		   Window transientFor)
	: tuTopLevel(instanceName, cmap, db, progName, progClass, transientFor)
{
    initialize(nl, instanceName);
}

nlWindow::nlWindow(NetLook *nl, const char *instanceName,
		   tuTopLevel *othertoplevel,
		   tuBool transientForOtherTopLevel)
	: tuTopLevel(instanceName, othertoplevel, transientForOtherTopLevel)
{
    initialize(nl, instanceName);
}

void
nlWindow::initialize(NetLook *nl, const char *instanceName)
{
    if (strcmp(instanceName, "netlook") == 0) {
	child = new ViewGadget(nl, this, instanceName);
	setName("NetLook");
	setIconName("NetLook");
    } else if (strcmp(instanceName, "map") == 0) {
	child = new MapGadget(nl, this, instanceName);
	setName("Map Control Panel");
	setIconName("Map");
    } else if (strcmp(instanceName, "snoop") == 0) {
	child = new SnoopGadget(nl, this, instanceName);
	setName("Snoop Control Panel");
	setIconName("Snoop");
    } else if (strcmp(instanceName, "netnode") == 0) {
	child = new NetNodeGadget(nl, this, instanceName);
	setName("NetNode Control Panel");
	setIconName("NetNode");
    } else if (strcmp(instanceName, "traffic") == 0) {
	child = new TrafficGadget(nl, this, instanceName);
	setName("Traffic Control Panel");
	setIconName("Traffic");
    } else if (strcmp(instanceName, "hide") == 0) {
	child = new HideGadget(nl, this, instanceName);
	setName("Hide Control Panel");
	setIconName("Hide");
    } else if (strcmp(instanceName, "select") == 0) {
	child = new SelectionGadget(nl, this, instanceName);
	setName("Selection Control Panel");
	setIconName("Selection");
    }
}

void
nlWindow::setNormalCursor(void)
{
    XDefineCursor(getDpy(), window->getWindow(), None);
}

void
nlWindow::setDragCursor(void)
{
    static Cursor dragCursor = 0;

    if (dragCursor == 0) {
	dragCursor = XCreateFontCursor(getDpy(), XC_hand2);
	XColor fore;
	fore.red = 255 << 8;
	fore.green = 0;
	fore.blue = 0;
	XColor back;
	back.red = 0;
	back.green = 0;
	back.blue = 0;
	XRecolorCursor(getDpy(), dragCursor, &fore, &back);
    }
    XDefineCursor(getDpy(), window->getWindow(), dragCursor);
}

void
nlWindow::setSweepCursor(void)
{
    static Cursor sweepCursor = 0;

    if (sweepCursor == 0) {
	sweepCursor = XCreateFontCursor(getDpy(), XC_hand1);
	XColor fore;
	fore.red = 255 << 8;
	fore.green = 0;
	fore.blue = 0;
	XColor back;
	back.red = 0;
	back.green = 0;
	back.blue = 0;
	XRecolorCursor(getDpy(), sweepCursor, &fore, &back);
    }
    XDefineCursor(getDpy(), window->getWindow(), sweepCursor);
}
