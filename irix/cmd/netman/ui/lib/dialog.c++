/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	DialogBox
 *
 *	$Revision: 1.13 $
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
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <bstring.h>
#include <tu/tuCallBack.h>
#include <tuDialogBox.h>
#include <tu/tuTopLevel.h>
#include <tuBitmapTile.h>
#include <tuBitmaps.h>
#include <tu/tuRowColumn.h>
#include <tu/tuLabelButton.h>
#include <tu/tuDefaultButton.h>
#include <tu/tuPalette.h>
#include <tu/tuWindow.h>
#include <tu/tuAccelerator.h>
#include "dialog.h"

static const unsigned int maxMessageLength = 4096;
static const unsigned int maxTextLines = 16;

class DialogBoxCallBack : public tuCallBack
{
public:
	DialogBoxCallBack(void (DialogBox::*method)(tuGadget *));
	~DialogBoxCallBack();

	virtual void doit(tuGadget*);

protected:
	void (DialogBox::*method)(tuGadget *);
};

DialogBoxCallBack::DialogBoxCallBack(void (DialogBox::*m)(tuGadget *))
{
    method = m;
}

DialogBoxCallBack::~DialogBoxCallBack()
{
}

void
DialogBoxCallBack::doit(tuGadget *g)
{
    DialogBox *f = (DialogBox *) g->getTopLevel();
    (f->*method)(g);
}


DialogBox::DialogBox(const char* instanceName, tuColorMap* cmap,
		     tuResourceDB* db, char* progName,
		     char* progClass, Window transientFor)
: tuDialogBox(instanceName, cmap, db, progName, progClass, transientFor)
{
    initialize();
}

DialogBox::DialogBox(const char* instanceName,
		     tuTopLevel* othertoplevel,
		     tuBool transientForOtherTopLevel)
: tuDialogBox(instanceName, othertoplevel, transientForOtherTopLevel)
{
    initialize();
}

DialogBox::~DialogBox(void)
{
    if (text != 0)
	delete text;
}

void
DialogBox::initialize(void)
{
    dialogType = tuEmpty;
    hitCode = tuNone;
    mapping = False;
    lineLength = 60;
    wrapLength = 50;
    text = 0;
    textLength = 0;
    breakChars = " \t\n/";
    tuAccelerator *a = new tuAccelerator("Return");
    a->setCallBack(new DialogBoxCallBack(&DialogBox::accelerator));
    addAccelerator(a);
    catchDeleteWindow(new DialogBoxCallBack(&DialogBox::callBack));
}

void
DialogBox::map(tuBool propagate)
{
    tuButton *b;
    tuBitmapTile *t = getBitmapTile();

    if (mapping == True) {
	tuDialogBox::map();
	return;
    }
    mapping = True;

    setText(text);
    switch (dialogType) {
	case tuEmpty:
	    setName("Dialog");
	    tuDialogBox::map(propagate);
	    break;
	case tuInformation:
	    setName("Information");
	    t->setBitmap(tuInformationBitmap, tuBitmapWidth, tuBitmapHeight);
	    t->setBackground(tuPalette::getLightGrayBlue(getScreen()));
	    b = new tuDefaultButton(getButtonContainer(), "", "Continue");
	    b->setCallBack(new DialogBoxCallBack(&DialogBox::callBack));
	    b->map();
	    defaultGadget = b;
	    getButtonContainer()->updateLayout();
	    tuDialogBox::mapWithGadgetUnderMouse(b, propagate);
	    break;
	case tuProgress:
	    setName("Progress");
	    t->setBitmap(tuProgressBitmap, tuBitmapWidth, tuBitmapHeight);
	    t->setBackground(tuPalette::getLightGrayCyan(getScreen()));
	    defaultGadget = 0;
	    getButtonContainer()->updateLayout();
	    tuDialogBox::mapWithGadgetUnderMouse(getButtonContainer(),
						 propagate);
	    break;
	case tuQuestion:
	    setName("Question");
	    t->setBitmap(tuQuestionBitmap, tuBitmapWidth, tuBitmapHeight);
	    t->setBackground(tuPalette::getLightGrayGreen(getScreen()));
	    b = new tuDefaultButton(getButtonContainer(), "", "Yes");
	    b->setCallBack(new DialogBoxCallBack(&DialogBox::callBack));
	    b->map();
	    defaultGadget = b;
	    b = new tuLabelButton(getButtonContainer(), "", "No");
	    b->setCallBack(new DialogBoxCallBack(&DialogBox::callBack));
	    b->map();
	    b = new tuLabelButton(getButtonContainer(), "", "Cancel");
	    b->setCallBack(new DialogBoxCallBack(&DialogBox::callBack));
	    b->map();
	    getButtonContainer()->updateLayout();
	    tuDialogBox::mapWithGadgetUnderMouse(b, propagate);
	    //getWindow()->grabPointer();
	    break;
	case tuWarning:
	    setName("Warning");
	    t->setBitmap(tuWarningBitmap, tuBitmapWidth, tuBitmapHeight);
	    t->setBackground(tuPalette::getLightGrayMagenta(getScreen()));
	    b = new tuDefaultButton(getButtonContainer(), "", "Continue");
	    b->setCallBack(new DialogBoxCallBack(&DialogBox::callBack));
	    b->map();
	    defaultGadget = b;
	    getButtonContainer()->updateLayout();
	    tuDialogBox::mapWithGadgetUnderMouse(b, propagate);
	    //getWindow()->grabPointer();
	    break;
	case tuAction:
	    setName("Error");
	    t->setBitmap(tuActionBitmap, tuBitmapWidth, tuBitmapHeight);
	    t->setBackground(tuPalette::getLightGrayRed(getScreen()));
	    b = new tuDefaultButton(getButtonContainer(), "", "Continue");
	    b->setCallBack(new DialogBoxCallBack(&DialogBox::callBack));
	    b->map();
	    defaultGadget = b;
	    getButtonContainer()->updateLayout();
	    tuDialogBox::mapWithGadgetUnderMouse(b, propagate);
	    //getWindow()->grabPointer();
	    break;
    }
    mapping = False;
}

void
DialogBox::unmap(tuBool propagate)
{
    tuDialogBox::unmap(propagate);
    tuRowColumn *rc = getButtonContainer();
    for (int i = rc->getNumExternalChildren() - 1; i > 0; i--)
        rc->getExternalChild(i)->markDelete();
    rc->grow(1, 1);
}

void
DialogBox::setName(const char *title)
{
    char buf[1024];
    if (instanceName == 0)
	strcpy(buf, title);
    else
	sprintf(buf, "%s %s", instanceName, title);
    tuDialogBox::setName(buf);
}

void
DialogBox::setText(const char *buf)
{
    char *text[maxTextLines];
    unsigned int line = 0;

    for (char *p = (char *) buf; *p != '\0' && line < maxTextLines - 1; ) {
	unsigned int len = strlen(p);
	if (len < lineLength) {
	    text[line++] = p;
	    break;
	}

	// First look for a new line
	char *brk;
	char *b = strchr(p, '\n');
	if (b != 0 && b - p < lineLength)
	    brk = b;
	else {
	    // Find a line break character
	    brk = 0;
	    for (b = p + wrapLength; b < p + lineLength; brk = b++) {
		b = strpbrk(b, breakChars);
		if (b == 0)
		    break;
	    }

	    // If none found, extend the string longer
	    if (brk == 0) {
		text[line++] = p;
		break;
	    }
	}

	// Copy the text line
	if (*brk == '/')
	    len = brk - p + 1;
	else
	    len = brk - p;
	text[line] = new char[len + 1];
	bcopy(p, text[line], len);
	text[line++][len] = '\0';

	// Skip intervening space
	for ( ; ; brk = b) {
	    b = strpbrk(brk + 1, breakChars);
	    if (b == 0 || *b == '\n' || b != brk + 1)
	        break;
	}

	p = brk + 1;
    }

    text[line] = 0;
    tuDialogBox::setText(text);
}

void
DialogBox::setText(const char *list[])
{
    tuDialogBox::setText(list);
}

enum tuDialogType
DialogBox::getDialogType(void)
{
    return dialogType;
}

void
DialogBox::setDialogType(enum tuDialogType t)
{
    dialogType = t;
}

void
DialogBox::setCallBack(tuCallBack *c)
{
    if (callback != 0)
	callback->markDelete();
    callback = c;
}

void
DialogBox::setMultiMessage(tuBool m)
{
    multiMessage = m;
    if (multiMessage && textLength != 0)
	text[0] = '\0';
}

void
DialogBox::accelerator(tuGadget *)
{
    if (defaultGadget != 0)
	callBack(defaultGadget);
}

void
DialogBox::callBack(tuGadget *g)
{
    const char *s = g->getText();
    if (s == 0)
	s = "Cancel";
    else if (strcmp(s, "Help") == 0) {
	if (callback != 0)
	    callback->doit(this);
	return;
    }
    switch (dialogType) {
	case tuEmpty:
	    hitCode = tuNone;
	    break;
	case tuInformation:
	case tuProgress:
	    hitCode = tuOK;
	    break;
	case tuWarning:
	case tuAction:
	    hitCode = tuOK;
	    getWindow()->ungrabPointer();
	    break;
	case tuQuestion:
	    if (strcmp(s, "Yes") == 0)
		hitCode = tuYes;
	    else if (strcmp(s, "No") == 0)
		hitCode = tuNo;
	    else if (strcmp(s, "Cancel") == 0)
		hitCode = tuCancel;
	    getWindow()->ungrabPointer();
	    break;
    }
    unmap();
    if (callback != 0)
	callback->doit(this);
}

enum tuDialogHitCode
DialogBox::getHitCode(void)
{
    return hitCode;
}

void
DialogBox::information(const char *format, ...)
{
    char buf[4096];
    char *p = buf;

    // Print variable arguments
    va_list ap;
    va_start(ap, format);
    p += vsprintf(buf, format, ap);
    va_end(ap);
    *p++ = '.';
    *p = '\0';

    makeMessage(buf);
    dialogType = tuInformation;
}

void
DialogBox::progress(const char *format, ...)
{
    char buf[4096];
    char *p = buf;

    // Print variable arguments
    va_list ap;
    va_start(ap, format);
    p += vsprintf(buf, format, ap);
    va_end(ap);
    *p++ = '.';
    *p = '\0';

    makeMessage(buf);
    dialogType = tuProgress;
}

void
DialogBox::question(const char *format, ...)
{
    char buf[4096];
    char *p = buf;

    // Print variable arguments
    va_list ap;
    va_start(ap, format);
    p += vsprintf(buf, format, ap);
    va_end(ap);
    *p++ = '?';
    *p = '\0';

    makeMessage(buf);
    dialogType = tuQuestion;
}

void
DialogBox::warning(const char *format, ...)
{
    char buf[4096];
    char *p = buf;

    // Print variable arguments
    va_list ap;
    va_start(ap, format);
    p += vsprintf(buf, format, ap);
    va_end(ap);
    *p++ = '.';
    *p = '\0';

    makeMessage(buf);
    dialogType = tuWarning;
}

void
DialogBox::warning(int err, const char *format, ...)
{
    char buf[4096];
    char *p = buf;

    // Print variable arguments
    va_list ap;
    va_start(ap, format);
    p += vsprintf(buf, format, ap);
    va_end(ap);

    // Add error message
    if (err > 0 && err < sys_nerr) {
	*p++ = ':';
	*p++ = ' ';
	*p++ = ' ';
	p += sprintf(p, sys_errlist[err]);
    }
    *p++ = '.';
    *p = '\0';

    makeMessage(buf);
    dialogType = tuWarning;
}

void
DialogBox::error(const char *format, ...)
{
    char buf[4096];
    char *p = buf;

    // Print variable arguments
    va_list ap;
    va_start(ap, format);
    p += vsprintf(buf, format, ap);
    va_end(ap);
    *p++ = '.';
    *p = '\0';

    makeMessage(buf);
    dialogType = tuAction;
}

void
DialogBox::error(int err, const char *format, ...)
{
    char buf[4096];
    char *p = buf;

    // Print variable arguments
    va_list ap;
    va_start(ap, format);
    p += vsprintf(buf, format, ap);
    va_end(ap);

    // Add error message
    if (err > 0 && err < sys_nerr) {
	*p++ = ':';
	*p++ = ' ';
	*p++ = ' ';
	p += sprintf(p, sys_errlist[err]);
    }
    *p++ = '.';
    *p = '\0';

    makeMessage(buf);
    dialogType = tuAction;
}

void
DialogBox::makeMessage(char *buf)
{
    unsigned int len = strlen(buf) + 1;
    if (multiMessage && text != 0 && text[0] != '\0') {
	unsigned int l = strlen(text);
	if (textLength == 0 || len + l + 1 > textLength) {
	    char *t = new char[len + l + 1];
	    bcopy(text, t, l);
	    delete text;
	    text = t;
	    textLength = len + l + 1;
	}
	text[l++] = '\n';
	bcopy(buf, &text[l], len);
    } else {
	if (len > textLength) {
	    delete text;
	    textLength = len;
	    text = new char[textLength];
	}
	bcopy(buf, text, len);
    }
}
