/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Map Gadget
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

#include <tuCallBack.h>
#include <tuGLGadget.h>
#include <tuUnPickle.h>
#include <gl/gl.h>
#include <macros.h>
#include <stdio.h>
#include "mapgadget.h"
#include "glmapgadget.h"
#include "netlook.h"
#include "helpoptions.h"
#include "mapgadgetlayout.h"

tuDeclareCallBackClass(MapGadgetCallBack, MapGadget);
tuImplementCallBackClass(MapGadgetCallBack, MapGadget);

extern HelpOptions *helpoptions;

MapGadget::MapGadget(NetLook *nl, tuGadget *parent, const char *instanceName)
{
    // Store name
    name = instanceName;

    // Set up callbacks
    (new MapGadgetCallBack(this, MapGadget::closeIt))->
					registerName("Map_close");
    (new MapGadgetCallBack(this, MapGadget::help))->registerName("Map_help");

    // Unpickle the UI
    TUREGISTERGADGETS;
    ui = tuUnPickle::UnPickle(parent, layoutstr);

    // Put in the Map gadget
    tuGadget *p = ui->findGadget("mapparent");
    if (p == 0) {
	fprintf(stderr, "No mapparent!\n");
	exit(-1);
    }
    glmg = new GLMapGadget(nl, p, "glmapgadget");

    // Store pointer to netlook
    netlook = nl;
}

void
MapGadget::closeIt(tuGadget *g)
{
    netlook->closeMapGadget(g);
}

void
MapGadget::help(tuGadget *g)
{
    helpoptions->map(g);
}
