/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Traffic Gadget
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
#include <tuDial.h>
#include <tuMultiChoice.h>
#include <tuUnPickle.h>
#include "trafficgadget.h"
#include "gltilegadget.h"
#include "netlook.h"
#include "helpoptions.h"
#include "trafficgadgetlayout.h"

tuDeclareCallBackClass(TrafficGadgetCallBack, TrafficGadget);
tuImplementCallBackClass(TrafficGadgetCallBack, TrafficGadget);

TrafficGadget *trafficgadget;
extern HelpOptions *helpoptions;

TrafficGadget::TrafficGadget(NetLook *nl, tuGadget *parent,
			     const char *instanceName)
{
    // Save name
    name = instanceName;

    // Set up callbacks
    (new TrafficGadgetCallBack(this, TrafficGadget::setRescale))->
				registerName("Traffic_rescale");
    (new TrafficGadgetCallBack(this, TrafficGadget::setRescaleValue))->
				registerName("Traffic_rescalevalue");
    (new TrafficGadgetCallBack(this, TrafficGadget::setBasis))->
				registerName("Traffic_basis");
    (new TrafficGadgetCallBack(this, TrafficGadget::setMode))->
				registerName("Traffic_mode");
    (new TrafficGadgetCallBack(this, TrafficGadget::setTimeout))->
				registerName("Traffic_timeout");
    (new TrafficGadgetCallBack(this, TrafficGadget::setTimeoutValue))->
				registerName("Traffic_timeoutvalue");
    (new TrafficGadgetCallBack(this, TrafficGadget::setMinColorValue))->
				registerName("Traffic_mincolor");
    (new TrafficGadgetCallBack(this, TrafficGadget::setMaxColorValue))->
				registerName("Traffic_maxcolor");
    (new TrafficGadgetCallBack(this, TrafficGadget::setPacketColorStep))->
				registerName("Traffic_packetstep");
    (new TrafficGadgetCallBack(this, TrafficGadget::setByteColorStep))->
				registerName("Traffic_bytestep");
    (new TrafficGadgetCallBack(this, TrafficGadget::closeIt))->
				registerName("Traffic_close");
    (new TrafficGadgetCallBack(this, TrafficGadget::help))->
				registerName("Traffic_help");

    // Unpickle UI
    TUREGISTERGADGETS;
    ui = tuUnPickle::UnPickle(parent, layoutstr); 

    tuGadget *g = ui->findGadget("Traffic_tile_parent");
    if (g == 0) {
	fprintf(stderr, "No Traffic_tile_parent\n");
	exit(-1);
    }
    colorTile = new GLTileGadget(g, "colortile");

    minColorLabel = ui->findGadget("Traffic_mincolorlabel");
    if (g == 0) {
	fprintf(stderr, "No Traffic_mincolorlabel");
	exit(-1);
    }

    maxColorLabel = ui->findGadget("Traffic_maxcolorlabel");
    if (maxColorLabel == 0) {
	fprintf(stderr, "No Traffic_maxcolorlabel");
	exit(-1);
    }

    unitColorLabel = ui->findGadget("Traffic_unitcolorlabel");
    if (unitColorLabel == 0) {
	fprintf(stderr, "No Traffic_unitcolorlabel");
	exit(-1);
    }

    // Save pointer to netlook
    netlook = nl;

    trafficgadget = this;
}

void
TrafficGadget::update(void)
{
    char buf[32];
    char *s;

    // Rescale
    tuGadget *g = ui->findGadget("Traffic_rescale");
    if (g == 0) {
	fprintf(stderr, "No Traffic_rescale\n");
	exit(-1);
    }
    g->setSelected(netlook->getTrafficRescale());

    // Rescale value
    g = ui->findGadget("Traffic_rescalevalue");
    if (g == 0) {
	fprintf(stderr, "No Traffic_rescalevalue\n");
	exit(-1);
    }
    sprintf(buf, "%d", netlook->getTrafficRescaleValue());
    g->setText(buf);

    // Timeout
    g = ui->findGadget("Traffic_timeout");
    if (g == 0) {
	fprintf(stderr, "No Traffic_timeout\n");
	exit(-1);
    }
    g->setSelected(netlook->getTrafficTimeout());

    // Timeout value
    g = ui->findGadget("Traffic_timeoutvalue");
    if (g == 0) {
	fprintf(stderr, "No Traffic_timeoutvalue\n");
	exit(-1);
    }
    sprintf(buf, "%d", netlook->getTrafficTimeoutValue());
    g->setText(buf);

    // Packet step
    packetStepDial = (tuDial *) ui->findGadget("Traffic_packetstepdial");
    if (packetStepDial == 0) {
	fprintf(stderr, "No Traffic_packetstepdial\n");
	exit(-1);
    }
    packetStepDial->setPosition(netlook->getTrafficPacketColorStep());
    packetStepLabel = ui->findGadget("Traffic_packetsteplabel");
    if (packetStepLabel == 0) {
	fprintf(stderr, "No Traffic_packetsteplabel\n");
	exit(-1);
    }

    // Byte step
    byteStepDial = (tuDial *) ui->findGadget("Traffic_bytestepdial");
    if (byteStepDial == 0) {
	fprintf(stderr, "No Traffic_bytestepdial\n");
	exit(-1);
    }
    byteStepDial->setPosition(netlook->getTrafficByteColorStep());
    byteStepLabel = ui->findGadget("Traffic_bytesteplabel");
    if (byteStepLabel == 0) {
	fprintf(stderr, "No Traffic_bytesteplabel\n");
	exit(-1);
    }

    // Traffic basis and disable appropriate dial
    switch (netlook->getTrafficBasis()) {
	case PACKETS:
	    s = "Traffic_packets";
	    setEnabled(packetStepDial, True);
	    setEnabled(packetStepLabel, True);
	    setEnabled(byteStepDial, False);
	    setEnabled(byteStepLabel, False);
	    unitColorLabel->setText("packets / second");
	    activeDial = packetStepDial;
	    break;
	case BYTES:
	    s = "Traffic_bytes";
	    setEnabled(packetStepDial, False);
	    setEnabled(packetStepLabel, False);
	    setEnabled(byteStepDial, True);
	    setEnabled(byteStepLabel, True);
	    unitColorLabel->setText("bytes / second");
	    activeDial = byteStepDial;
	    break;
    }
    g = ui->findGadget(s);
    if (g == 0) {
	fprintf(stderr, "No %s\n", s);
	exit(-1);
    }
    tuMultiChoice *m = tuMultiChoice::Find("Traffic_basis");
    if (m->getNumberOfButton() == 0) {
	fprintf(stderr, "No Traffic_basis\n", s);
	exit(-1);
    }
    m->setCurrentButton((tuButton *) g, False);

    // Traffic mode
    switch (netlook->getTrafficMode()) {
	case ENDPOINT:
	    s = "Traffic_endpoint";
	    break;
	case HOP:
	    s = "Traffic_hop";
	    break;
    }
    g = ui->findGadget(s);
    if (g == 0) {
	fprintf(stderr, "No %s\n", s);
	exit(-1);
    }
    m = tuMultiChoice::Find("Traffic_mode");
    if (m->getNumberOfButton() == 0) {
	fprintf(stderr, "No Traffic_mode\n");
	exit(-1);
    }
    m->setCurrentButton((tuButton *) g, False);

    // Minimum color map value
    g = ui->findGadget("Traffic_mincolor");
    if (g == 0) {
	fprintf(stderr, "No Traffic_mincolor\n");
	exit(-1);
    }
    unsigned int i = netlook->getTrafficMinColorValue();
    sprintf(buf, "%d", i);
    g->setText(buf);

    // Maximum color map value
    g = ui->findGadget("Traffic_maxcolor");
    if (g == 0) {
	fprintf(stderr, "No Traffic_maxcolor\n");
	exit(-1);
    }
    sprintf(buf, "%d", netlook->getTrafficMaxColorValue());
    g->setText(buf);
    colorTile->setColor(i, netlook->getTrafficMaxColorValue());
    colorTile->setDoubleBuf(True);

    // Traffic volume labels
    setTrafficVolumeLabels();
}

void
TrafficGadget::setRescale(tuGadget *g)
{
    netlook->setTrafficRescale(g->getSelected());
}

void
TrafficGadget::setRescaleValue(tuGadget *g)
{
    const char *c = g->getText();
    int i = atoi(c);
    if (i < 0) {
	puts("bad rescale value");
	return;
    }
    netlook->setTrafficRescaleValue(i);
}

void
TrafficGadget::setBasis(tuGadget *g)
{
    const char *s = g->getInstanceName();
    if (strcmp(s, "Traffic_packets") == 0) {
	setEnabled(packetStepDial, True);
	setEnabled(packetStepLabel, True);
	setEnabled(byteStepDial, False);
	setEnabled(byteStepLabel, False);
	netlook->setTrafficBasis(PACKETS);
	unitColorLabel->setText("packets / second");
	activeDial = packetStepDial;
	setTrafficVolumeLabels();
    } else if (strcmp(s, "Traffic_bytes") == 0) {
	setEnabled(packetStepDial, False);
	setEnabled(packetStepLabel, False);
	setEnabled(byteStepDial, True);
	setEnabled(byteStepLabel, True);
	netlook->setTrafficBasis(BYTES);
	unitColorLabel->setText("bytes / second");
	activeDial = byteStepDial;
	setTrafficVolumeLabels();
    } else
	puts("bad label");
}

void
TrafficGadget::setMode(tuGadget *g)
{
    const char *s = g->getInstanceName();
    if (strcmp(s, "Traffic_endpoint") == 0)
	netlook->setTrafficMode(ENDPOINT);
    else if (strcmp(s, "Traffic_hop") == 0)
	netlook->setTrafficMode(HOP);
    else
	puts("bad label");
}

void
TrafficGadget::setTimeout(tuGadget *g)
{
    netlook->setTrafficTimeout(g->getSelected());
}

void
TrafficGadget::setTimeoutValue(tuGadget *g)
{
    const char *c = g->getText();
    int i = atoi(c);
    if (i < 0) {
	puts("bad timeout value");
	return;
    }
    netlook->setTrafficTimeoutValue(i);
}

void
TrafficGadget::setMinColorValue(tuGadget *g)
{
    const char *c = g->getText();
    int i = atoi(c);
    if (i < 0) {
	puts("bad min color value");
	return;
    }
    netlook->setTrafficMinColorValue(i);
    colorTile->setColor(i, netlook->getTrafficMaxColorValue());
    colorTile->render();
    setTrafficVolumeLabels();
}

void
TrafficGadget::setMaxColorValue(tuGadget *g)
{
    const char *c = g->getText();
    int i = atoi(c);
    if (i < 0) {
	puts("bad max color value");
	return;
    }
    netlook->setTrafficMaxColorValue(i);
    colorTile->setColor(netlook->getTrafficMinColorValue(), i);
    colorTile->render();
    setTrafficVolumeLabels();
}

void
TrafficGadget::setPacketColorStep(tuGadget *g)
{
    const float f = ((tuDial *) g)->getPosition();
    netlook->setTrafficPacketColorStep((unsigned int) f);
    setTrafficVolumeLabels();
}

void
TrafficGadget::setByteColorStep(tuGadget *g)
{
    const float f = ((tuDial *) g)->getPosition();
    netlook->setTrafficByteColorStep((unsigned int) f);
    setTrafficVolumeLabels();
}

void
TrafficGadget::setEnabled(tuGadget *g, tuBool v)
{
    g->setEnabled(v);
    for (int i = g->getNumExternalChildren() - 1; i >= 0; i--)
	setEnabled(g->getExternalChild(i), v);
}

void
TrafficGadget::closeIt(tuGadget *g)
{
    netlook->closeTrafficGadget(g);
}

void
TrafficGadget::help(tuGadget *g)
{
    helpoptions->traffic(g);
}

void
TrafficGadget::setTrafficVolumeLabels(void)
{
    char buf[32];
    const float f = activeDial->getPosition();
    sprintf(buf, "< %.0f", f);
    minColorLabel->setText(buf);
    sprintf(buf, ">= %.0f", f * (netlook->getTrafficColorValues() - 1));
    maxColorLabel->setText(buf);
}
