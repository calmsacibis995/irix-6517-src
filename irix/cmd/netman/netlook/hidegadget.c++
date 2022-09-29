/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Hide Gadget
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
#include <string.h>
#include <tuCallBack.h>
#include <tuGadget.h>
#include <tuList.h>
#include <tuTextField.h>
#include <tuUnPickle.h>
#include "extralabel.h"
#include "netlook.h"
#include "network.h"
#include "node.h"
#include "hidegadget.h"
#include "dialog.h"
#include "helpoptions.h"
#include "hidegadgetlayout.h"

tuDeclareCallBackClass(HideGadgetCallBack, HideGadget);
tuImplementCallBackClass(HideGadgetCallBack, HideGadget);

extern NetLook *netlook;
extern HelpOptions *helpoptions;
HideGadget *hidegadget;

HideGadget::HideGadget(NetLook *nl, tuGadget *parent,
			 const char *instanceName)
{
    // Save name
    name = instanceName;

    // Set up callbacks
    (new HideGadgetCallBack(this, HideGadget::select))->
				registerName("Hide_select");
    (new HideGadgetCallBack(this, HideGadget::pick))->
				registerName("Hide_pick");
    (new HideGadgetCallBack(this, HideGadget::text))->
				registerName("Hide_text");
    (new HideGadgetCallBack(this, HideGadget::text))->
				registerName("Hide_hide");
    (new HideGadgetCallBack(this, HideGadget::pick))->
				registerName("Hide_unhide");
    (new HideGadgetCallBack(this, HideGadget::closeIt))->
				registerName("Hide_close");
    (new HideGadgetCallBack(this, HideGadget::help))->
				registerName("Hide_help");

    // Unpickle UI
    TUREGISTERGADGETS;
    ui = tuUnPickle::UnPickle(parent, layoutstr);

    // Find gadgets
    list = (tuList *) ui->findGadget("Hide_list");
    if (list == 0) {
	fprintf(stderr, "No Hide_list\n");
	exit(1);
    }

    textfield = (tuTextField *) ui->findGadget("Hide_textfield");
    if (textfield == 0) {
	fprintf(stderr, "No Hide_textfield\n");
	exit(1);
    }

    // Save pointer to netlook
    netlook = nl;
    hidegadget = this;
}

void
HideGadget::add(const char *c)
{
    (void) new extraLabel(list, "", c);

    tuLayoutHints hints;
    ui->getTopLevel()->getLayoutHints(&hints);
    ui->getTopLevel()->resize(hints.prefWidth, hints.prefHeight);
}

void
HideGadget::remove(void *v)
{
    int n = list->getNumExternalChildren();
    for (unsigned int i = 0; i < n; i++) {
	extraLabel *l = (extraLabel *) list->getExternalChild(i);
	if (l->getExtra() == v) {
	    l->markDelete();
	    return;
	}
    }
}

char **
HideGadget::get(void)
{
    int n = list->getNumExternalChildren();
    if (n == 0)
	return 0;

    char **entry = new char*[n + 1];
    unsigned int c = 0;
    for (unsigned int i = 0; i < n; i++, c++) {
	tuGadget *g = list->getExternalChild(i);
	const char *s = g->getText();
	if (s == 0)
	    continue;
	unsigned int len = strlen(g->getText()) + 1;
	entry[c] = new char[len];
	bcopy(g->getText(), entry[i], len);
    }
    entry[c] = 0;
    return entry;
}

void
HideGadget::setText(const char *c)
{
    textfield->setText(c);
}

void
HideGadget::hide(void)
{
    int n = list->getNumExternalChildren();
    for (unsigned int i = 0; i < n; i++) {
	extraLabel *l = (extraLabel *) list->getExternalChild(i);
	const char *s = l->getText();
	void *v = netlook->hide(s, True);
	if (v == 0)
	    l->markDelete();
	else
	    l->setExtra(v);
    }
}

void
HideGadget::select(tuGadget *)
{
    extraLabel *l = (extraLabel *) list->getCurrentSelected();
    if (l == 0) {
	textfield->setText("");
	textfield->setEnabled(True);
    } else {
	textfield->setText(l->getText());
	textfield->setEnabled(False);
    }
}

void
HideGadget::pick(tuGadget *)
{
    extraLabel *l = (extraLabel *) list->getCurrentSelected();
    if (l == 0) {
	if (list->getNumExternalChildren() == 0) {
	    netlook->getDialogBox()->warning("No hidden objects");
	    netlook->setDialogBoxCallBack();
	    netlook->openDialogBox();
	    return;
	}
	l = (extraLabel *) list->getExternalChild(0);
    }

    if (netlook->unhide(l->getText()) == 0) {
	netlook->openDialogBox();
	return;
    }

    l->markDelete();
    textfield->setText("");
    textfield->setEnabled(True);
}

void
HideGadget::text(tuGadget *)
{
    extraLabel *l = (extraLabel *) list->getCurrentSelected();
    if (l == 0) {
	// If nothing is selected, hide the new item
	void *v;
	const char *t = textfield->getText();
	if (*t != '\0')
	    v = netlook->hide(t);
	else {
	    switch (netlook->getPickedObjectType()) {
		case pkNone:
		case pkConnection:
		    netlook->getDialogBox()->warning("No object to hide"); 
		    netlook->setDialogBoxCallBack();
		    netlook->openDialogBox();
		    return;
		case pkNetwork:
		    {
			Network *network = netlook->getPickedNetwork();
			textfield->setText(network->display);
			v = netlook->hide(network);
		    }
		    break;
		case pkNode:
		    {
			Node *node = netlook->getPickedNode();
			textfield->setText(node->display);
			v = netlook->hide(node);
		    }
		    break;
	    }
	}
	if (v == 0) {
	    netlook->openDialogBox();
	    return;
	}
	(void) new extraLabel(list, "", textfield->getText(), v);
	list->updateLayout();
	textfield->setText("");
    }
}

void
HideGadget::closeIt(tuGadget *g)
{
    netlook->closeHideGadget(g);
}

void
HideGadget::help(tuGadget *g)
{
    helpoptions->hide(g);
}
