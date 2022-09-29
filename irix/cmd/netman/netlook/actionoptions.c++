/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Action Options
 *
 *	$Revision: 1.4 $
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
#include <tuGadget.h>
#include <tuTextField.h>
#include <tuButton.h>
#include "netlook.h"
#include "prompt.h"
#include "network.h"
#include "node.h"
#include "hidegadget.h"
#include "actionoptions.h"

tuDeclareCallBackClass(ActionOptionsCallBack, ActionOptions);
tuImplementCallBackClass(ActionOptionsCallBack, ActionOptions);

extern HideGadget *hidegadget;

ActionOptions::ActionOptions(NetLook *nl)
{
    // Set up callbacks
    (new ActionOptionsCallBack(this, ActionOptions::info))->
					registerName("Action_info");
    (new ActionOptionsCallBack(this, ActionOptions::findAction))->
					registerName("Action_find");
    (new ActionOptionsCallBack(this, ActionOptions::pingAction))->
					registerName("Action_ping");
    (new ActionOptionsCallBack(this, ActionOptions::traceAction))->
					registerName("Action_trace");
    (new ActionOptionsCallBack(this, ActionOptions::homeAction))->
					registerName("Action_home");
    (new ActionOptionsCallBack(this, ActionOptions::hideAction))->
					registerName("Action_hide");
    (new ActionOptionsCallBack(this, ActionOptions::deleteAction))->
					registerName("Action_delete");
    (new ActionOptionsCallBack(this, ActionOptions::deleteAll))->
					registerName("Action_deleteAll");

    // Save netlook pointer
    netlook = nl;

    prompt = 0;
}

void
ActionOptions::addSpectrumCallBack(tuButton *b)
{
    b->setCallBack(new ActionOptionsCallBack(this, ActionOptions::spectrum));
}

void
ActionOptions::createPromptBox(void)
{
    prompt = new PromptBox("prompt", netlook->getTopLevel(), True);
    prompt->bind();
    prompt->setText("Enter the name or address of the object:");
}

void
ActionOptions::info(tuGadget *)
{
    netlook->openInfoBox();
}

void
ActionOptions::findAction(tuGadget *)
{
    if (prompt == 0)
	createPromptBox();
    else if (prompt->isMapped())
	prompt->unmap();
    prompt->setName("Find Prompt");
    prompt->resizeToFit();
    prompt->setCallBack(new ActionOptionsCallBack(this, ActionOptions::find));
    prompt->mapWithCancelUnderMouse();
}

void
ActionOptions::find(tuGadget *)
{
    if (prompt->getHitCode() != tuCancel) {
	const char *c = prompt->getTextField()->getText();
	if (netlook->find(c) == 0) {
	    netlook->openDialogBox();
	    return;
        }
    }
    prompt->unmap();
    prompt->getTextField()->setText("");
}

void
ActionOptions::pingAction(tuGadget *)
{
    if (prompt == 0)
	createPromptBox();
    else if (prompt->isMapped())
	prompt->unmap();
    prompt->setName("Ping Prompt");
    Node *n = netlook->getPickedNode();
    if (n != 0)
	prompt->getTextField()->setText(n->display);
    prompt->resizeToFit();
    prompt->setCallBack(new ActionOptionsCallBack(this, ActionOptions::ping));
    prompt->mapWithCancelUnderMouse();
}

void
ActionOptions::ping(tuGadget *)
{
    if (prompt->getHitCode() != tuCancel) {
	const char *c = prompt->getTextField()->getText();
	if (netlook->ping(c) == 0) {
	    netlook->openDialogBox();
	    return;
	}
    }
    prompt->unmap();
    prompt->getTextField()->setText("");
}

void
ActionOptions::traceAction(tuGadget *)
{
    if (prompt == 0)
	createPromptBox();
    else if (prompt->isMapped())
	prompt->unmap();
    prompt->setName("Trace Route Prompt");
    Node *n = netlook->getPickedNode();
    if (n != 0)
	prompt->getTextField()->setText(n->display);
    prompt->resizeToFit();
    prompt->setCallBack(new ActionOptionsCallBack(this, ActionOptions::trace));
    prompt->mapWithCancelUnderMouse();
}

void
ActionOptions::trace(tuGadget *)
{
    if (prompt->getHitCode() != tuCancel) {
	const char *c = prompt->getTextField()->getText();
	if (netlook->trace(c) == 0) {
	    netlook->openDialogBox();
	    return;
	}
    }
    prompt->unmap();
    prompt->getTextField()->setText("");
}

void
ActionOptions::homeAction(tuGadget *)
{
    netlook->home();
}

void
ActionOptions::hideAction(tuGadget *)
{
    Node *node = netlook->getPickedNode();
    if (node != 0)
	hidegadget->setText(node->display);
    else {
	Network *network = netlook->getPickedNetwork();
	if (network != 0)
	    hidegadget->setText(network->display);
    }
    netlook->openHideGadget();
}

void
ActionOptions::deleteAction(tuGadget *)
{
    if (prompt == 0)
	createPromptBox();
    else if (prompt->isMapped())
	prompt->unmap();
    prompt->setName("Delete Prompt");
    switch (netlook->getPickedObjectType()) {
	case pkNone:
	case pkConnection:
	    break;
	case pkNode:
	    prompt->getTextField()->setText(netlook->getPickedNode()->display);
	    break;
	case pkNetwork:
	    prompt->getTextField()->setText(
			netlook->getPickedNetwork()->display);
	    break;
    }
    prompt->resizeToFit();
    prompt->setCallBack(new ActionOptionsCallBack(this, ActionOptions::Delete));
    prompt->mapWithCancelUnderMouse();
}

void
ActionOptions::Delete(tuGadget *)
{
    if (prompt->getHitCode() != tuCancel) {
	const char *c = prompt->getTextField()->getText();
	if (netlook->Delete(c) == 0) {
	    netlook->openDialogBox();
	    return;
	}
    }
    prompt->unmap();
    prompt->getTextField()->setText("");
}

void
ActionOptions::deleteAll(tuGadget *)
{
    netlook->clear();
}

void
ActionOptions::spectrum(tuGadget *)
{
    char buf[128];

    switch (netlook->getPickedObjectType()) {
	case pkNetwork:
	    {
		SegmentNode *s = netlook->getPickedNetwork()->segment;
		char *n = s->getName();
		char *a = s->ipnum.getString();
		sprintf(buf, "%s network %s %s 0", SpectrumCommand,
						   n == 0 ? "0" : n,
						   a == 0 ? "0" : a);
	    }
	    break;

	case pkNode:
	    {
		InterfaceNode *i = netlook->getPickedNode()->interface;
		char *n = i->getName();
		char *ia = i->ipaddr.getString();
		char *pa = i->physaddr.getString();
		sprintf(buf, "%s node %s %s %s", SpectrumCommand,
						 n == 0 ? "0" : n,
						 ia == 0 ? "0" : ia,
						 pa == 0 ? "0" : pa);
	    }
	    break;

	default:
	    sprintf(buf, "%s 0 0 0 0", SpectrumCommand);
	    break;
    }

    system(buf);
}
