/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Gadget Options
 *
 *	$Revision: 1.1 $
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
#include <tuCallBack.h>

#include "gadgetoptions.h"
#include "netlook.h"

tuDeclareCallBackClass(GadgetOptionsCallBack, GadgetOptions);
tuImplementCallBackClass(GadgetOptionsCallBack, GadgetOptions);

GadgetOptions::GadgetOptions(NetLook *nl)
{
    // Set up callbacks
    (new GadgetOptionsCallBack(this, GadgetOptions::snoop))->
					registerName("Gadget_snoop");
    (new GadgetOptionsCallBack(this, GadgetOptions::map))->
					registerName("Gadget_map");
    (new GadgetOptionsCallBack(this, GadgetOptions::netnode))->
					registerName("Gadget_netnode");
    (new GadgetOptionsCallBack(this, GadgetOptions::traffic))->
					registerName("Gadget_traffic");
    (new GadgetOptionsCallBack(this, GadgetOptions::hide))->
					registerName("Gadget_hide");

    // Save netlook pointer
    netlook = nl;
}

void
GadgetOptions::snoop(tuGadget *)
{
    netlook->openSnoopGadget();
}

void
GadgetOptions::map(tuGadget *)
{
    netlook->openMapGadget();
}

void
GadgetOptions::netnode(tuGadget *)
{
    netlook->openNetNodeGadget();
}

void
GadgetOptions::traffic(tuGadget *)
{
    netlook->openTrafficGadget();
}

void
GadgetOptions::hide(tuGadget *)
{
    netlook->openHideGadget();
}
