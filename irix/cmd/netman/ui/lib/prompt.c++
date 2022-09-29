/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Prompt Box
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

#include <tu/tuCallBack.h>
#include <tu/tuRowColumn.h>
#include <tu/tuTextField.h>
#include <tu/tuDeck.h>
#include <tu/tuUnPickle.h>
#include <tu/tuLabel.h>
#include <tu/tuWindow.h>
#include "prompt.h"
#include "promptlayout.h"

class PromptBoxCallBack : public tuCallBack
{
public:
	PromptBoxCallBack(void (PromptBox::*method)(tuGadget *));
	~PromptBoxCallBack();

	virtual void doit(tuGadget*);

protected:
	void (PromptBox::*method)(tuGadget *);
};

PromptBoxCallBack::PromptBoxCallBack(void (PromptBox::*m)(tuGadget *))
{
    method = m;
}

PromptBoxCallBack::~PromptBoxCallBack()
{
}

void
PromptBoxCallBack::doit(tuGadget *g)
{
    PromptBox *f = (PromptBox *) g->getTopLevel();
    (f->*method)(g);
}


PromptBox::PromptBox(const char* instanceName, tuColorMap* cmap,
			 tuResourceDB* db, char* progName,
			 char* progClass, Window transientFor)
: tuTopLevel(instanceName, cmap, db, progName, progClass, transientFor)
{
    initialize();
}

PromptBox::PromptBox(const char* instanceName,
			 tuTopLevel* othertoplevel,
			 tuBool transientForOtherTopLevel)
: tuTopLevel(instanceName, othertoplevel, transientForOtherTopLevel)
{
    initialize();
}

PromptBox::~PromptBox()
{
}

void
PromptBox::initialize()
{
    static tuBool firsttime = True;
    if (firsttime) {
	firsttime = False;
	TUREGISTERGADGETS;
	(new PromptBoxCallBack(&PromptBox::ok))
			->registerName("__tuPromptBox_textfield");
	(new PromptBoxCallBack(&PromptBox::ok))
			->registerName("__tuPromptBox_ok");
	(new PromptBoxCallBack(&PromptBox::apply))
			->registerName("__tuPromptBox_apply");
	(new PromptBoxCallBack(&PromptBox::reset))
			->registerName("__tuPromptBox_reset");
	(new PromptBoxCallBack(&PromptBox::cancel))
			->registerName("__tuPromptBox_cancel");
	(new PromptBoxCallBack(&PromptBox::help))
			->registerName("__tuPromptBox_help");
    }
    tuGadget* stuff = tuUnPickle::UnPickle(this, layoutstr);
    textContainer = (tuRowColumn *) stuff->findGadget("_text_container");
    textField = (tuTextField *) stuff->findGadget("_text_field");
    deck = (tuDeck *) stuff->findGadget("_deck");
    hitCode = tuNone;
    showHelp = False;
    showApply = False;
    showReset = False;
    setDeck();

    catchDeleteWindow(new PromptBoxCallBack(&PromptBox::cancel));
}

void
PromptBox::map(tuBool propagate)
{
    tuTopLevel::map(propagate);
    XFlush(window->getDpy());
}

void
PromptBox::setCallBack(tuCallBack* c)
{
    if (callback) callback->markDelete();
    callback = c;
}

void
PromptBox::setText(const char *s)
{
    const char *list[2];
    list[0] = s;
    list[1] = 0;
    setText(list);
}

void
PromptBox::setText(const char *list[])
{
    for (int i = textContainer->getNumExternalChildren(); i != 0; i--) {
	tuGadget *g = textContainer->getExternalChild(0);
	g->markDelete();
    }
    textContainer->grow(1,1);

    for (i = 0; list[i] != 0; i++) {
	tuLabel *l = new tuLabel(textContainer, "text", list[i]);
	l->map();
    }
    resizeToFit();
}

void
PromptBox::setHelp(tuBool b)
{
    if (b == showHelp)
	return;
    showHelp = b;
    setDeck();
}

void
PromptBox::setApply(tuBool b)
{
    if (b == showApply)
	return;
    showApply = b;
    setDeck();
}

void
PromptBox::setReset(tuBool b)
{
    if (b == showReset || !showApply)
	return;
    showReset = b;
    setDeck();
}

void
PromptBox::setDeck(void)
{
    if (showReset) {
	if (showHelp)
	    deck->setVisibleChild(deck->findGadget("__reset_help"));
	else
	    deck->setVisibleChild(deck->findGadget("__reset"));
    } else if (showApply) {
	if (showHelp)
	    deck->setVisibleChild(deck->findGadget("__apply_help"));
	else
	    deck->setVisibleChild(deck->findGadget("__apply"));
    } else if (showHelp)
	deck->setVisibleChild(deck->findGadget("__ok_help"));
    else
	deck->setVisibleChild(deck->findGadget("__ok"));
}
    
void
PromptBox::mapWithCancelUnderMouse(void)
{
    tuGadget *g = deck->getVisibleChild();
    mapWithGadgetUnderMouse(g->findGadget("_map_button"));
}

void
PromptBox::ok(tuGadget *)
{
    if (!isMapped())
	return;
    hitCode = tuOK;
    if (callback) callback->doit(this);
}

void
PromptBox::apply(tuGadget *)
{
    if (!isMapped())
	return;
    hitCode = tuApply;
    if (callback) callback->doit(this);
}

void
PromptBox::reset(tuGadget *)
{
    if (!isMapped())
	return;
    hitCode = tuReset;
    if (callback) callback->doit(this);
}

void
PromptBox::cancel(tuGadget *)
{
    if (!isMapped())
	return;
    hitCode = tuCancel;
    if (callback) callback->doit(this);
}

void
PromptBox::help(tuGadget *)
{
    if (!isMapped())
	return;
    hitCode = tuHelp;
    if (callback) callback->doit(this);
}

enum PromptHitCode
PromptBox::getHitCode(void)
{
    return hitCode;
}
