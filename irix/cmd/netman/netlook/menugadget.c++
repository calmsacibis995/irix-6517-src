/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Menu Gadget
 *
 *	$Revision: 1.2 $
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

#include <stdio.h>
#include <unistd.h>
#include <tuGadget.h>
#include <tuMenuBar.h>
#include <tuUnPickle.h>
#include "menugadget.h"
#include "fileoptions.h"
#include "gadgetoptions.h"
#include "actionoptions.h"
#include "tooloptions.h"
#include "helpoptions.h"
#include "menugadgetlayout.h"
#include "netlook.h"

ToolOptions *tooloptions;

MenuGadget::MenuGadget(NetLook *nl, tuGadget *parent, const char *instanceName)
{
    // Save name
    name = instanceName;

    // Create option handlers
    fileHandler = new FileOptions(nl);
    gadgetHandler = new GadgetOptions(nl);
    actionHandler = new ActionOptions(nl);
    helpHandler = new HelpOptions(nl);

    // This is generic for all netvis apps
    tooloptions = toolHandler = new ToolOptions;

    // Unpickle the UI
    TUREGISTERGADGETS;
    menu = (tuMenuBar *) tuUnPickle::UnPickle(parent, layoutstr);

    // Check for Spectrum
    if (access(SpectrumCommand, X_OK) == 0) {
	tuGadget *m = menu->findGadget("Menu_Actions");
	if (m == 0)
	    return;
	(void) new tuSeparator(m, "", True);
	tuLabelMenuItem *l = new tuLabelMenuItem(m, "", "Spectrum");
	actionHandler->addSpectrumCallBack(l);
    }
}
