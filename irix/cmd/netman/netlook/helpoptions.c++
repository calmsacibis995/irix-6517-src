/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Help Options
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
#include <tuCallBack.h>
#include "helpoptions.h"
#include "help.h"

tuDeclareCallBackClass(HelpOptionsCallBack, HelpOptions);
tuImplementCallBackClass(HelpOptionsCallBack, HelpOptions);

HelpOptions *helpoptions;

HelpOptions::HelpOptions(NetLook *nl)
{
    // Set up callbacks
    (new HelpOptionsCallBack(this, HelpOptions::general))->
					registerName("Help_general");
    (new HelpOptionsCallBack(this, HelpOptions::main))->
					registerName("Help_main");
    (new HelpOptionsCallBack(this, HelpOptions::newfile))->
					registerName("Help_new");
    (new HelpOptionsCallBack(this, HelpOptions::datafile))->
					registerName("Help_data");
    (new HelpOptionsCallBack(this, HelpOptions::uifile))->
					registerName("Help_ui");
    (new HelpOptionsCallBack(this, HelpOptions::snoop))->
					registerName("Help_snoop");
    (new HelpOptionsCallBack(this, HelpOptions::map))->
					registerName("Help_map");
    (new HelpOptionsCallBack(this, HelpOptions::netnode))->
					registerName("Help_netnode");
    (new HelpOptionsCallBack(this, HelpOptions::traffic))->
					registerName("Help_traffic");
    (new HelpOptionsCallBack(this, HelpOptions::hide))->
					registerName("Help_hide");
    (new HelpOptionsCallBack(this, HelpOptions::info))->
					registerName("Help_info");
    (new HelpOptionsCallBack(this, HelpOptions::find))->
					registerName("Help_find");
    (new HelpOptionsCallBack(this, HelpOptions::ping))->
					registerName("Help_ping");
    (new HelpOptionsCallBack(this, HelpOptions::trace))->
					registerName("Help_trace");
    (new HelpOptionsCallBack(this, HelpOptions::home))->
					registerName("Help_home");
    (new HelpOptionsCallBack(this, HelpOptions::hideAction))->
					registerName("Help_hideAction");
    (new HelpOptionsCallBack(this, HelpOptions::del))->
					registerName("Help_delete");
    (new HelpOptionsCallBack(this, HelpOptions::tools))->
					registerName("Help_tools");

    // Save netlook pointer
    netlook = nl;

    // Save pointer to this
    helpoptions = this;
}

void
HelpOptions::close(void)
{
    HELPclose();
}

void
HelpOptions::general(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.general.help");
}

void
HelpOptions::main(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.main.help");
}

void
HelpOptions::newfile(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.new.help");
}

void
HelpOptions::datafile(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.datafile.help");
}

void
HelpOptions::uifile(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.uifile.help");
}

void
HelpOptions::snoop(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.snoop.help");
}

void
HelpOptions::map(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.map.help");
}

void
HelpOptions::netnode(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.netnode.help");
}

void
HelpOptions::traffic(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.traffic.help");
}

void
HelpOptions::hide(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.hide.help");
}

void
HelpOptions::info(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.info.help");
}

void
HelpOptions::find(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.find.help");
}

void
HelpOptions::ping(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.ping.help");
}

void
HelpOptions::trace(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.trace.help");
}

void
HelpOptions::home(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.home.help");
}

void
HelpOptions::hideAction(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.hideAction.help");
}

void
HelpOptions::del(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.delete.help");
}

void
HelpOptions::tools(tuGadget *)
{
    HELPdisplay("/usr/lib/HelpCards/NetLook.tools.help");
}
