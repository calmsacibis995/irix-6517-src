//
// Copyright 1997, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// UNPUBLISHED -- Rights reserved under the copyright laws of the United
// States.   Use of a copyright notice is precautionary only and does not
// imply publication or disclosure.
//
// U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
// in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
// in similar or successor clauses in the FAR, or the DOD or NASA FAR
// Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
// 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
//
// THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
// INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
// DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
// PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
// GRAPHICS, INC.
//

#include <stdio.h>
#include <stdlib.h>
#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/LabelG.h> 
#include <Xm/TextF.h> 
#include <Vk/VkApp.h>
#include <Vk/VkResource.h>

#include "pmapi.h"
#include "impl.h"
#include "FlagList.h"


#define MAX_FLAGS	32

extern Widget	bottom;


FlagList::FlagList() :
	_flaglist(NULL),
	_flagcount(0),
	_flagbound(MAX_FLAGS)	// a moving target
{
}

FlagList::~FlagList(void)
{
    if (_flaglist)
	free(_flaglist);
    _flaglist = NULL;
    _flagcount = 0;
}

int
FlagList::queryType(const char *spec, int *offset)
{
    if (strncasecmp(spec, "text|", *offset=5) == 0)
	return PMRUN_TEXTFIELD;
    else if (strncasecmp(spec, "finder|", *offset=7) == 0)
	return PMRUN_FINDER;
    else if (strncasecmp(spec, "radio|", *offset=6) == 0)
	return PMRUN_RADIO;
    else if (strncasecmp(spec, "check|", *offset=6) == 0)
	return PMRUN_CHECK;

    return PMRUN_UNKNOWN;
}

Widget
FlagList::createTextField(
	Widget parent,
	const char *labelname,
	const char *textname,
	const char *flagstr,
	const char *labelstr,
	const char *fieldstr,
	Boolean editable)
{
    Arg		args[16];
    Cardinal	count = 0;
    static Pixel readonly = NULL;

    count = 0;
    XtSetArg(args[count], XmNalignment,	XmALIGNMENT_BEGINNING); count++;
    if (bottom) {
	XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
	XtSetArg(args[count], XmNtopWidget, bottom); count++;
	XtSetArg(args[count], XmNtopOffset, 13); count++;
    }
    else {
	XtSetArg(args[count], XmNtopAttachment,	XmATTACH_FORM); count++;
	XtSetArg(args[count], XmNtopOffset, 5); count++;
    }
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNleftOffset, 2); count++;
    XtSetArg(args[count], XmNwidth, 95); count++;
    XtSetArg(args[count], XmNheight, 20); count++;
    Widget label = XmCreateLabelGadget(parent,
				(char *)labelname, args, count);
    XtManageChild(label);
    if (labelstr != NULL && labelstr[0] != '\0') {
	XmString labelString = XmStringCreateLocalized((char *)labelstr);
	count = 0;
	XtSetArg(args[count], XmNlabelString, labelString); count++;
	XtSetValues(label, args, count);
	XmStringFree(labelString);
    }

    count = 0;
    XtSetArg(args[count], XmNeditable, editable); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_OPPOSITE_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, label); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopOffset, -5); count++;
    XtSetArg(args[count], XmNleftOffset, 100); count++;
    XtSetArg(args[count], XmNrightOffset, 10); count++;
    XtSetArg(args[count], XmNwidth, 289); count++;
    XtSetArg(args[count], XmNheight, 33); count++;
    bottom = XtCreateWidget(textname, xmTextFieldWidgetClass,
					parent, args, count);
    XtManageChild(bottom);
    if (fieldstr != NULL && fieldstr[0] != '\0')
	XmTextFieldSetString(bottom, (char*)fieldstr);

    if (!editable) {
	if (!readonly) {
	    readonly = (Pixel) VkGetResource(bottom,
		"readOnlyBackground", "ReadOnlyBackground", XmRPixel, "Black");
	}
	count = 0;
	XtSetArg(args[count], XmNbackground, readonly); count++;
	XtSetArg(args[count], XmNcursorPositionVisible, False); count++;
	XtSetArg(args[count], XmNtraversalOn, False); count++;
	XtSetValues(bottom, args, count);
    }

    if (flagstr != NULL)
	newFlagListEntry(PMRUN_TEXTFIELD, bottom, flagstr);

    return bottom;
}

void
FlagList::newFlagListEntry(int type, Widget w, const char *flag)
{
    if (_flaglist == NULL || (_flagcount >= _flagbound))
	_flaglist = (FlagListEntry *)realloc((void*)_flaglist,
			(_flagbound+=MAX_FLAGS)*sizeof(FlagListEntry));

    _flaglist[_flagcount].specType   = type;
    _flaglist[_flagcount].specWidget = w;
    _flaglist[_flagcount].specFlag   = strdup(flag);
    _flagcount++;
}

char *
FlagList::getFlag(int index)
{
    if (index >= 0 && index < _flagcount) {
	return this->_flaglist[index].specFlag;
    }
    return NULL;
}

int
FlagList::getType(int index)
{
    if (index >= 0 && index < _flagcount) {
	return this->_flaglist[index].specType;
    }
    return PMRUN_UNKNOWN;
}

Widget
FlagList::getWidget(int index)
{
    if (index >= 0 && index < _flagcount) {
	return this->_flaglist[index].specWidget;
    }
    return NULL;
}

