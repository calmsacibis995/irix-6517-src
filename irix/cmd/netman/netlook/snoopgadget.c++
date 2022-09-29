/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Snoop Gadget
 *
 *	$Revision: 1.8 $
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
#include <tuTopLevel.h>
#include <tuFrame.h>
#include <tuTextField.h>
#include <tuUnPickle.h>
#include "netlook.h"
#include "snoopgadget.h"
#include "dialog.h"
#include "helpoptions.h"
#include "snoopgadgetlayout.h"

tuDeclareCallBackClass(SnoopGadgetCallBack, SnoopGadget);
tuImplementCallBackClass(SnoopGadgetCallBack, SnoopGadget);

extern NetLook *netlook;
extern HelpOptions *helpoptions;
SnoopGadget *snoopgadget;

SnoopGadget::SnoopGadget(NetLook *nl, tuGadget *parent,
			 const char *instanceName)
{
    // Save name
    name = instanceName;

    // Set up callbacks
    (new SnoopGadgetCallBack(this, SnoopGadget::vcheck))->
				registerName("Snoop_check");
    (new SnoopGadgetCallBack(this, SnoopGadget::edit))->
				registerName("Snoop_edit");
    (new SnoopGadgetCallBack(this, SnoopGadget::doFilterText))->
				registerName("Snoop_dofiltertext");
    (new SnoopGadgetCallBack(this, SnoopGadget::archiver))->
				registerName("Snoop_archiver");
    (new SnoopGadgetCallBack(this, SnoopGadget::closeIt))->
				registerName("Snoop_close");
    (new SnoopGadgetCallBack(this, SnoopGadget::help))->
				registerName("Snoop_help");

    // Unpickle UI
    TUREGISTERGADGETS;
    ui = tuUnPickle::UnPickle(parent, layoutstr);

    // Find list
    container = (tuRowColumn *) ui->findGadget("Snoop_container");
    if (container == 0) {
	fprintf(stderr, "No Snoop_container\n");
	exit(1);
    }

    entryField = ui->findGadget("Snoop_entryfield");
    if (entryField == 0) {
	fprintf(stderr, "No Snoop_entryfield\n");
	exit(1);
    }

    // Find filter
    filterText = (tuTextField *) ui->findGadget("Snoop_filtertext");
    if (filterText == 0) {
	fprintf(stderr, "No Snoop_filtertext\n");
	exit(1);
    }

    // Save pointer to netlook
    netlook = nl;
    snoopgadget = this;
}

unsigned int
SnoopGadget::add(const char *name, tuBool activate)
{
    int r, c;
    if (container->findChild(entryField, &r, &c) == False)
	return 0;
    tuCheckBox *cb = new tuCheckBox(container, "", "");
    cb->setCallBack(new SnoopGadgetCallBack(this, SnoopGadget::vcheck));
    tuFrame *frame = new tuFrame(container, "");
    frame->setFrameType(tuFrame_Innie);
    tuTextField *tf = new tuTextField(frame, "");
    tf->setTerminatorCallBack(
		new SnoopGadgetCallBack(this, SnoopGadget::change));
    tf->setText(name);
    container->place(cb, r, 0);
    container->place(frame, r, 1);
    cb->map();
    frame->map();
    container->updateLayout();
    if (activate) {
	cb->setSelected(True);
	return check(cb);
    }
    return 1;
}

unsigned int
SnoopGadget::check(tuGadget *g)
{
    int r, c;
    if (container->findChild(g, &r, &c) == False)
	return 0;
    tuFrame *f = (tuFrame *) container->getChildAt(r, c + 1);
    if (f == 0)
	return 0;
    tuTextField *tf = (tuTextField *) f->getExternalChild(0);
    if (tf == 0)
	return 0;

    if (g->getSelected()) {
	Node *n = netlook->startSnoop(tf->getText());
	if (n == 0) {
	    g->setSelected(False);
	    netlook->openDialogBox();
	    return 0;
	}
	tf->setEnabled(False);
	checkbox.add(g);
	node.add(n);
    } else {
	r = checkbox.find(g);
	if (r < 0)
	    return 0;
	unsigned int rc = netlook->stopSnoop((Node *) node[r]);
	tf->setEnabled(True);
	if (rc == 0) {
	    netlook->openDialogBox();
	    return 0;
	}
    }
    return 1;
}

void
SnoopGadget::vcheck(tuGadget *g)
{
    (void) check(g);
}

void
SnoopGadget::edit(tuGadget *g)
{
    add(g->getText(), False);
    g->setText("");
}

void
SnoopGadget::change(tuGadget *g)
{
    const char *s = g->getText();
    if (s == 0 || *s == '\0') {
	int r, c;
	if (container->findChild(g->getParent(), &r, &c) == False)
	    return;
	tuCheckBox *cb = (tuCheckBox *) container->getChildAt(r, 0);
	g->getParent()->markDelete();
	cb->markDelete();
	if (container->findChild(entryField, &r, &c) == False)
	    return;
	cb = (tuCheckBox *) container->getChildAt(r, 0);
	if (cb == 0)
	    return;
	container->place(cb, r - 1, 0);
	container->place(entryField, r - 1, c);
	container->updateLayout();
    }
}

void
SnoopGadget::deactivate(Node *n)
{
    int i = node.find(n);
    if (i < 0)
	return;
    tuCheckBox *cb = (tuCheckBox *) checkbox[i];

    int r, c;
    if (container->findChild(cb, &r, &c) == False)
	return;

    checkbox.remove(i);
    node.remove(i);

    cb->setSelected(False);
    tuFrame *f = (tuFrame *) container->getChildAt(r, 1);
    if (f == 0)
	return;
    tuTextField *tf = (tuTextField *) f->getExternalChild(0);
    if (tf == 0)
	return;
    tf->setEnabled(True);
}

unsigned int
SnoopGadget::get(char **entry[])
{
    int r = container->getNumRows() - 1;
    if (r <= 0)
	return 0;

    *entry = new char*[r];
    unsigned int c = 0;
    for (unsigned int i = 0; i < r; i++) {
	tuFrame *f = (tuFrame *) container->getChildAt(i, 1);
	if (f == 0)
	    break;
	tuTextField *tf = (tuTextField *) f->getExternalChild(0);
	if (tf == 0)
	    break;
	const char *s = tf->getText();
	if (s == 0 || *s == '\0')
	    continue;
	tuCheckBox *cb = (tuCheckBox *) container->getChildAt(i, 0);
	if (cb == 0)
	    break;
	unsigned int len = strlen(s) + 1;
	(*entry)[c] = new char[len + 1];
	*((*entry)[c]) = (checkbox.find(cb) < 0) ? '-' : '+';
	bcopy(s, (*entry)[c] + 1, len);
	c++;
    }
    return c;
}

void
SnoopGadget::doFilterText(tuGadget *g)
{
    if (netlook->setFilter(g->getText()) == 0)
	netlook->openDialogBox();
}

void
SnoopGadget::setFilterText(const char *c)
{
    filterText->setText(c);
    filterText->movePointToHome(False);
}

void
SnoopGadget::archiver(tuGadget *)
{
    if (netlook->openArchiver() == 0)
	netlook->openDialogBox();
}

void
SnoopGadget::closeIt(tuGadget *g)
{
    netlook->closeSnoopGadget(g);
}

void
SnoopGadget::help(tuGadget *g)
{
    helpoptions->snoop(g);
}
