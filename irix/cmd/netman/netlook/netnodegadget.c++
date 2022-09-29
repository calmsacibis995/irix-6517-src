/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Network and Node Gadget
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
#include <string.h>
#include <tuCallBack.h>
#include <tuMultiChoice.h>
#include <tuUnPickle.h>
#include "netnodegadget.h"
#include "netlook.h"
#include "helpoptions.h"
#include "netnodegadgetlayout.h"

tuDeclareCallBackClass(NetNodeGadgetCallBack, NetNodeGadget);
tuImplementCallBackClass(NetNodeGadgetCallBack, NetNodeGadget);

extern HelpOptions *helpoptions;
NetNodeGadget *netnodegadget;

NetNodeGadget::NetNodeGadget(NetLook *nl, tuGadget *parent,
			     const char *instanceName)
{
    // Save name
    name = instanceName;

    // Set up callbacks
    (new NetNodeGadgetCallBack(this, NetNodeGadget::netIgnore))->
				registerName("Network_ignore");
    (new NetNodeGadgetCallBack(this, NetNodeGadget::netShow))->
				registerName("Network_show");
    (new NetNodeGadgetCallBack(this, NetNodeGadget::netLabel))->
				registerName("Network_label");
    (new NetNodeGadgetCallBack(this, NetNodeGadget::nodeIgnore))->
				registerName("Node_ignore");
    (new NetNodeGadgetCallBack(this, NetNodeGadget::nodeShow))->
				registerName("Node_show");
    (new NetNodeGadgetCallBack(this, NetNodeGadget::nodeLabel))->
				registerName("Node_label");
    (new NetNodeGadgetCallBack(this, NetNodeGadget::closeIt))->
				registerName("NetNode_close");
    (new NetNodeGadgetCallBack(this, NetNodeGadget::help))->
				registerName("NetNode_help");


    // Unpickle UI
    TUREGISTERGADGETS;
    ui = tuUnPickle::UnPickle(parent, layoutstr);

    // Save pointer to netlook
    netlook = nl;

    netnodegadget = this;
}

void
NetNodeGadget::update(void)
{
    tuGadget *g = ui->findGadget("Network_ignore");
    if (g == 0) {
	fprintf(stderr, "No Network_ignore\n");
	exit(-1);
    }
    g->setSelected(netlook->getNetIgnore() == IGNORE);

    g = ui->findGadget("Network_show");
    if (g == 0) {
	fprintf(stderr, "No Network_show\n");
	exit(-1);
    }
    g->setSelected(netlook->getNetShow() == ALL);

    char *s;
    switch (netlook->getNetDisplay()) {
	case DMFULLNAME:
	    s = "Network_name";
	    break;
	case DMINETADDR:
	    s = "Network_number";
	    break;
    }
    g = ui->findGadget(s);
    if (g == 0) {
	fprintf(stderr, "No %s\n", s);
	exit(-1);
    }
    tuMultiChoice *m = tuMultiChoice::Find("Network_label");
    if (m->getNumberOfButton() == 0) {
	fprintf(stderr, "No Network_label\n", s);
	exit(-1);
    }
    m->setCurrentButton((tuButton *) g, False);

    g = ui->findGadget("Node_ignore");
    if (g == 0) {
	fprintf(stderr, "No Node_ignore\n");
	exit(1);
    }
    g->setSelected(netlook->getNodeIgnore() == IGNORE);

    g = ui->findGadget("Node_show");
    if (g == 0) {
	fprintf(stderr, "No Node_show\n");
	exit(1);
    }
    g->setSelected(netlook->getNodeShow() == ALL);

    switch (netlook->getNodeDisplay()) {
	case DMFULLNAME:
	    s = "Node_full";
	    break;
	case DMNODENAME:
	    s = "Node_local";
	    break;
	case DMINETADDR:
	    s = "Node_ip";
	    break;
	case DMDECNETADDR:
	    s = "Node_dec";
	    break;
	case DMVENDOR:
	    s = "Node_vendor";
	    break;
	case DMPHYSADDR:
	    s = "Node_physical";
	    break;
    }
    g = ui->findGadget(s);
    if (g == 0) {
	fprintf(stderr, "No %s\n", s);
	exit(-1);
    }
    m = tuMultiChoice::Find("Node_label");
    if (m->getNumberOfButton() == 0) {
	fprintf(stderr, "No Node_label\n", s);
	exit(-1);
    }
    m->setCurrentButton((tuButton *) g, False);
}

void
NetNodeGadget::netIgnore(tuGadget *g)
{
    if (g->getSelected() == 0)
	netlook->setNetIgnore(ADD);
    else
	netlook->setNetIgnore(IGNORE);
}

void
NetNodeGadget::netShow(tuGadget *g)
{
    if (g->getSelected() == 0)
	netlook->setNetShow(ACTIVE);
    else
	netlook->setNetShow(ALL);
}

void
NetNodeGadget::netLabel(tuGadget *g)
{
    const char *s = g->getInstanceName();
    if (strcmp(s, "Network_name") == 0)
	netlook->setNetDisplay(DMFULLNAME);
    else if (strcmp(s, "Network_number") == 0)
	netlook->setNetDisplay(DMINETADDR);
    else
	puts("bad label");
}

void
NetNodeGadget::nodeIgnore(tuGadget *g)
{
    if (g->getSelected() == 0)
	netlook->setNodeIgnore(ADD);
    else
	netlook->setNodeIgnore(IGNORE);
}

void
NetNodeGadget::nodeShow(tuGadget *g)
{
    if (g->getSelected() == 0)
	netlook->setNodeShow(ACTIVE);
    else
	netlook->setNodeShow(ALL);
}

void
NetNodeGadget::nodeLabel(tuGadget *g)
{
    const char *s = g->getInstanceName();
    if (strcmp(s, "Node_full") == 0)
	netlook->setNodeDisplay(DMFULLNAME);
    else if (strcmp(s, "Node_local") == 0)
	netlook->setNodeDisplay(DMNODENAME);
    else if (strcmp(s, "Node_ip") == 0)
	netlook->setNodeDisplay(DMINETADDR);
    else if (strcmp(s, "Node_dec") == 0)
	netlook->setNodeDisplay(DMDECNETADDR);
    else if (strcmp(s, "Node_vendor") == 0)
	netlook->setNodeDisplay(DMVENDOR);
    else if (strcmp(s, "Node_physical") == 0)
	netlook->setNodeDisplay(DMPHYSADDR);
    else
	puts("bad label");
}

void
NetNodeGadget::closeIt(tuGadget *g)
{
    netlook->closeNetNodeGadget(g);
}

void
NetNodeGadget::help(tuGadget *g)
{
    helpoptions->netnode(g);
}
