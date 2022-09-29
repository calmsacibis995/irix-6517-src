/*
 * Copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: FormUI.c++,v 1.8 1999/04/30 01:44:04 kenmcd Exp $"

#include <Xm/Form.h> 
#include <Xm/Label.h>
#include <Xm/LabelG.h>
#include <Xm/TextF.h>
#include <Xm/Text.h>
#include <Xm/DrawnB.h>

#include <Vk/VkResource.h>
#include <Vk/VkApp.h>
#include <Vk/VkComponent.h>

#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>

#include <Sgm/ThumbWheel.h>

#include "Inv.h"
#include "App.h"
#include "FormUI.h"
#include "ModList.h"
#include "View.h"

float INV_FormUI::theMultiplier = 20.0;

SoXtExaminerViewer	*theViewer;

String  INV_FormUI::_defaultFormResources[] = {
    "*scaleText.value:  1.0000", 
    "*scaleLabel.labelString:  Scale",
    "*scaleWheel.homePosition: 0",
    "*scaleWheel.maximum: 0",
    "*scaleWheel.minimum: 0",
    "*scaleWheel.angleRange: 240",
    "*scaleWheel.unitsPerRotation:  100",
    "*metricLabel.labelString:  \n",
    "*timeLabel.labelString: ",
    (char*)NULL
    };

INV_FormUI::INV_FormUI(const char *name) 
: VkComponent(name)
{ 
}

INV_FormUI::INV_FormUI(const char *name, Widget parent) 
: VkComponent(name)
{ 
    create(parent);
}

INV_FormUI::~INV_FormUI() 
{
    delete _viewer;
}

void 
INV_FormUI::create(Widget parent)
{
    Arg		args[32];
    Cardinal	count;
    Pixel	readonlybg = NULL;
    Pixel	readonlyfg = NULL;

#ifdef PCP_PROFILE
    __pmEventTrace("widget creation");
#endif

    // Load any class-defaulted resources for this object
    setDefaultResources(parent, _defaultFormResources);

    count = 0;
    XtSetArg(args[count], XmNresizePolicy, XmRESIZE_GROW); count++;
    _baseWidget = _form = XtCreateWidget(_name, xmFormWidgetClass,
                                                parent, args, count);

    installDestroyHandler();

    count = 0;
    XtSetArg(args[count], XmNlabelType, XmPIXMAP); count++;
    XtSetArg(args[count], XmNpushButtonEnabled, True); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNleftOffset, 5); count++;
    XtSetArg(args[count], XmNwidth, 52); count++;
    XtSetArg(args[count], XmNheight, 50); count++;
    XtSetArg(args[count], XmNtraversalOn, False); count++;
    _vcr = XtCreateWidget("vcrButton", xmDrawnButtonWidgetClass,
                                                _baseWidget, args, count);
    XtAddCallback(_vcr, XmNactivateCallback, 
		  &INV_FormUI::vcrActivateCBCallback, (XtPointer)this);

    if (!readonlybg) {
        readonlybg = (Pixel) VkGetResource(_vcr, "readOnlyBackground",
                                "ReadOnlyBackground", XmRPixel, "gray72");
        readonlyfg = (Pixel) VkGetResource(_vcr, "readOnlyForeground",
                                "ReadOnlyForeground", XmRPixel, "Black");
    }
    count = 0;
    XtSetArg(args[count], XmNalignment, XmALIGNMENT_BEGINNING); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNleftWidget, _vcr); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNresizePolicy, XmRESIZE_NONE); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNleftOffset, 5); count++;
    XtSetArg(args[count], XmNrightOffset, 5); count++;
    XtSetArg(args[count], XmNeditable, False); count++;
    XtSetArg(args[count], XmNbackground, readonlybg); count++;
    XtSetArg(args[count], XmNforeground, readonlyfg); count++;
    XtSetArg(args[count], XmNresizeHeight, True); count++;
    XtSetArg(args[count], XmNrows, 2); count++;
    XtSetArg(args[count], XmNtraversalOn, False); count++;
    XtSetArg(args[count], XmNcursorPositionVisible, False); count++;
    _label = XtCreateWidget("metricLabel", xmTextWidgetClass,
                                                _baseWidget, args, count);

    count = 0;
    XtSetArg(args[count], XmNalignment, XmALIGNMENT_END); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _vcr); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopOffset, 8); count++;
    XtSetArg(args[count], XmNleftOffset, 5); count++;
    _scaleLabel = XmCreateLabelGadget(_baseWidget, "scaleLabel", args, count);

    count = 0;
    XtSetArg(args[count], XmNorientation, XmHORIZONTAL); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _vcr); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNleftWidget, _scaleLabel); count++;
    XtSetArg(args[count], XmNtopOffset, 4); count++;
    XtSetArg(args[count], XmNleftOffset, 3); count++;
    XtSetArg(args[count], XmNhighlightThickness, 0); count++;
    XtSetArg(args[count], XmNtraversalOn, True); count++;
    _scaleWheel = XtCreateWidget("scaleWheel", sgThumbWheelWidgetClass,
                                                _baseWidget, args, count);

    XtAddCallback(_scaleWheel, XmNvalueChangedCallback,
		  &INV_FormUI::wheelChangedCBCallback, (XtPointer)this );
    XtAddCallback(_scaleWheel, XmNdragCallback,
		  &INV_FormUI::wheelDragCBCallback, (XtPointer)this );

    count = 0;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _vcr); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNleftWidget, _scaleWheel); count++;
    XtSetArg(args[count], XmNtopOffset, 0); count++;
    XtSetArg(args[count], XmNleftOffset, 2); count++;
    XtSetArg(args[count], XmNcolumns, 8); count++;
    XtSetArg(args[count], XmNtraversalOn, True); count++;
    XtSetArg(args[count], XmNhighlightThickness, 0); count++;
    XtSetArg(args[count], XmNcursorPositionVisible, True); count++;
    _scaleText = XtCreateWidget("scaleText", xmTextFieldWidgetClass,
                                                _baseWidget, args, count);

    XtAddCallback(_scaleText, XmNactivateCallback,
		  &INV_FormUI::scaleTextActivateCBCallback, (XtPointer)this );

    count = 0;
    XtSetArg(args[count], XmNalignment, XmALIGNMENT_END); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _vcr); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNresizePolicy, XmRESIZE_NONE); count++;
    XtSetArg(args[count], XmNtopOffset, -2); count++;
    XtSetArg(args[count], XmNrightOffset, 5); count++;
    XtSetArg(args[count], XmNcolumns, 24); count++;
    XtSetArg(args[count], XmNeditable, False); count++;
    XtSetArg(args[count], XmNbackground, readonlybg); count++;
    XtSetArg(args[count], XmNforeground, readonlyfg); count++;
    XtSetArg(args[count], XmNtraversalOn, False); count++;
    XtSetArg(args[count], XmNcursorPositionVisible, False); count++;
    _timeLabel = XtCreateWidget("timeLabel", xmTextFieldWidgetClass,
                                                _baseWidget, args, count);

#define FORM_CHILDREN   6
    XtManageChildren(&_label, FORM_CHILDREN);

    theViewer = _viewer = new SoXtExaminerViewer(_baseWidget, "viewer");

    // Change the cursor to be a pointer by default
    theViewer->setViewing(False);

    _viewer->show();

    count = 0;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNbottomAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopWidget, _scaleText); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNbottomPosition, 0); count++;
    XtSetArg(args[count], XmNrightPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNbottomOffset, 5); count++;
    XtSetArg(args[count], XmNleftOffset, 5); count++;
    XtSetArg(args[count], XmNrightOffset, 5); count++;
    XtSetValues(_viewer->getWidget(), args, count);

#ifdef PCP_PROFILE
    __pmEventTrace("end widgets");
#endif
}

const char * 
INV_FormUI::className()
{
    return ("INV_FormUI");
}

void 
INV_FormUI::vcrActivateCBCallback(Widget    w,
				  XtPointer clientData,
				  XtPointer callData)
{
    INV_FormUI* obj = (INV_FormUI *) clientData;
    obj->vcrActivateCB(w, callData);
}

void 
INV_FormUI::scaleTextActivateCBCallback(Widget    w,
					XtPointer clientData,
					XtPointer callData)
{
    INV_FormUI* obj = (INV_FormUI *) clientData;
    obj->scaleTextActivateCB(w, callData);
}

void 
INV_FormUI::wheelChangedCBCallback(Widget    w,
				   XtPointer clientData,
				   XtPointer callData)
{
    INV_FormUI* obj = (INV_FormUI *) clientData;
    obj->wheelChangedCB(w, callData);
}

void 
INV_FormUI::wheelDragCBCallback(Widget    w,
				XtPointer clientData,
				XtPointer callData)
{
    INV_FormUI* obj = (INV_FormUI *) clientData;
    obj->wheelDragCB(w, callData);
}

void 
INV_FormUI::scaleTextActivateCB(Widget w, XtPointer)
{
    char *val = XmTextGetString(w);
    float f = setScale(val);
    XtFree(val);

    int i = (int)rint(theMultiplier * log10(f));
    int max;
    int min;

    XtVaGetValues(_scaleWheel,
		  XmNmaximum, &max,
		  XmNminimum, &min,
		  NULL);

    if (max > min) {
	if (i > max)
	    i = max;
	else if (i < min)
	    i = min;
    }

    XtVaSetValues(_scaleWheel, XmNvalue, i, (XtPointer)NULL);
}

void 
INV_FormUI::wheelChangedCB(Widget, XtPointer callData)
{
    SgThumbWheelCallbackStruct *cbs = (SgThumbWheelCallbackStruct*) callData;
    setScale(calcScale(cbs->value));
}

void INV_FormUI::wheelDragCB(Widget, XtPointer callData)
{
    SgThumbWheelCallbackStruct *cbs = (SgThumbWheelCallbackStruct*) callData;
    setScale(calcScale(cbs->value));
}

float
INV_FormUI::calcScale(int value)
{
    return pow(10, (float)(value/theMultiplier));
}

void
INV_FormUI::setScale(float value)
{
    theScale = value;
    showScale();
}

float
INV_FormUI::setScale(char *value)
{
    char *endptr = NULL;
    float f = (float)strtod(value, &endptr);
    if (f <= 0.0 || endptr == value || 
	(endptr != NULL && endptr[0] != '\0')) {
	XBell(theApp->display(), 100);
	showScale();
	return theScale;
    }
    setScale(f);
    return f;
}

void
INV_FormUI::showScale()
{
    sprintf(theBuffer, "%.2g", theScale);
    XmTextSetString(_scaleText, theBuffer);
    theModList->refresh(OMC_false);
    theView->render((INV_View::RenderOptions)
		    (INV_View::metricLabel | INV_View::inventor), 0);
}
