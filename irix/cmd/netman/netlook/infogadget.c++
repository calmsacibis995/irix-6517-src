/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Information Gadget
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
#include <tuRowColumn.h>
#include <tuBitmapTile.h>
#include <tuBitmaps.h>
#include <tuLabel.h>
#include <tuFont.h>
#include <tuUnPickle.h>
#include <tuWindow.h>
#include <tuPalette.h>
#include <tuStyle.h>
#include "infogadget.h"
#include "infogadgetlayout.h"

tuDeclareCallBackClass(InfoGadgetCallBack, InfoGadget);
tuImplementCallBackClass(InfoGadgetCallBack, InfoGadget);

InfoGadget::InfoGadget(const char *instanceName, tuColorMap *cmap,
		       tuResourceDB *db, char *progName,
		       char *progClass, Window transientFor)
: tuTopLevel(instanceName, cmap, db, progName, progClass, transientFor)
{
    initialize();
}

InfoGadget::InfoGadget(const char *instanceName,
		       tuTopLevel *othertoplevel,
		       tuBool transientForOtherTopLevel)
: tuTopLevel(instanceName, othertoplevel, transientForOtherTopLevel)
{
    initialize();
}

InfoGadget::~InfoGadget(void)
{
    if (callback != 0)
	callback->markDelete();
}

void
InfoGadget::initialize(void)
{
    static tuBool firsttime = True;
    if (firsttime) {
        firsttime = False;
        TUREGISTERGADGETS;
    }
    tuGadget *stuff = tuUnPickle::UnPickle(this, layoutstr);
    textContainer = (tuRowColumn *) stuff->findGadget("_text_container");
    tuButton *b = (tuButton *) stuff->findGadget("_close_button");
    b->setCallBack(new InfoGadgetCallBack(this, InfoGadget::callBack));
    callback = 0;
}

void
InfoGadget::map(tuBool propagate)
{
    tuBitmapTile *bitmap = (tuBitmapTile *) child->findGadget("_bitmap");
    bitmap->setBitmap(tuInformationBitmap, tuBitmapWidth, tuBitmapHeight);
    bitmap->setBackground(tuPalette::getLightGrayBlue(getScreen()));
    resizeToFit();
    tuTopLevel::map(propagate);
    XFlush(window->getDpy());
}

void
InfoGadget::mapWithCloseUnderMouse(tuBool propagate)
{
    mapWithGadgetUnderMouse(child->findGadget("_close_button"), propagate);
}

void
InfoGadget::clearText(void)
{
    for (int i = textContainer->getNumExternalChildren(); i != 0; i--) {
        tuGadget *g = textContainer->getExternalChild(0);
        g->markDelete();
    }
    textContainer->grow(1,1);
}

void
InfoGadget::addText(const char *label, const char *item)
{
    int row, col, align;
    tuLabel *l = new tuLabel(textContainer, "text", label);
    tuFont *f = tuFont::getFont(getScreen(), tuStyle::labelBoldFont);
    l->setFont(f);
    textContainer->findChild(l, &row, &col, &align);
    textContainer->place(l, row, col, tuCenterRight);
    l->map();
    l = new tuLabel(textContainer, "item", item);
    l->map();
    resizeToFit();
}

void
InfoGadget::setCallBack(tuCallBack *c)
{
    if (callback != 0)
	callback->markDelete();
    callback = c;
}

void
InfoGadget::callBack(tuGadget *)
{
    if (callback != 0)
	callback->doit(this);
}
