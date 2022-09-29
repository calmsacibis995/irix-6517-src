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

/////////////////////////////////////////////////////////////
//
// Source file for LiveTimeFormUI
//
// $Id: LiveTimeFormUI.c++,v 1.6 1999/04/30 01:44:04 kenmcd Exp $
//
/////////////////////////////////////////////////////////////


#include "pmapi.h"
#include "impl.h"
#include "LiveTimeFormUI.h"

#include <Xm/DrawnB.h> 
#include <Xm/Form.h> 
#include <Xm/LabelG.h> 
#include <Xm/TextF.h> 
#include <Vk/VkResource.h>
#include <Vk/VkOptionMenu.h>
#include <Vk/VkMenuItem.h>

#ifdef IRIX5_3
#include "VkPixmap.h"
#else
#include <Vk/VkPixmap.h>
#endif

#include "pixmaps.h"  // constant and pixmap descriptions


// These are default resources for widgets in objects of this class
// All resources will be prepended by *<name> at instantiation,
// where <name> is the name of the specific instance, as well as the
// name of the baseWidget. These are only defaults, and may be overriden
// in a resource file by providing a more specific resource name

String  LiveTimeFormUI::_defaultLiveTimeFormUIResources[] = {
        "*realtimeLabel.labelString:  Real Time Controls: ",
        "*timeLabel.labelString:  Time",
        "*unitsOption.labelString:  ",
        "*intervalUnitsMilliseconds.labelString:  Milliseconds",
        "*intervalUnitsSeconds.labelString:  Seconds",
        "*intervalUnitsMinutes.labelString:  Minutes",
        "*intervalUnitsHours.labelString:  Hours",
        "*intervalUnitsDays.labelString:  Days",
        "*intervalUnitsWeeks.labelString:  Weeks",
        "*intervalLabel.labelString:  Interval",
        (char*)NULL
};

LiveTimeFormUI::LiveTimeFormUI(const char *name) : VkComponent(name) 
{ 
    // No widgets are created by this constructor.
    // If an application creates a component using this constructor,
    // It must explictly call create at a later time.
    // This is mostly useful when adding pre-widget creation
    // code to a derived class constructor.

}

LiveTimeFormUI::LiveTimeFormUI(const char *name, Widget parent) : VkComponent(name) 
{ 
    // Call creation function to build the widget tree.
     create(parent);

}

LiveTimeFormUI::~LiveTimeFormUI() 
{
}


void
LiveTimeFormUI::create(Widget parent)
{
    Arg		args[32];
    Cardinal	count;

    // Load any class-defaulted resources for this object
    setDefaultResources(parent, _defaultLiveTimeFormUIResources);

#ifdef PCP_PROFILE
    __pmEventTrace("start widgets");
#endif

    // Create an unmanaged widget as the top of the widget hierarchy

    count = 0;
    XtSetArg(args[count], XmNresizePolicy, XmRESIZE_GROW); count++;
    _baseWidget = _liveTimeForm = XtCreateWidget(_name,
				xmFormWidgetClass, parent, args, count);

    // install a callback to guard against unexpected widget destruction
    installDestroyHandler();

    // Create widgets used in this component
    // All variables are data members of this class

    _unitsOption = new VkOptionMenu(_baseWidget);
    _intervalUnitsMilliseconds = _unitsOption->addAction(
	"intervalUnitsMilliseconds", 
	&LiveTimeFormUI::intervalUnitsActivateCallback, (XtPointer) this);
    _intervalUnitsSeconds = _unitsOption->addAction(
	"intervalUnitsSeconds", 
	&LiveTimeFormUI::intervalUnitsActivateCallback, (XtPointer) this);
    _intervalUnitsMinutes = _unitsOption->addAction(
	"intervalUnitsMinutes",
	&LiveTimeFormUI::intervalUnitsActivateCallback, (XtPointer) this);
    _intervalUnitsHours = _unitsOption->addAction(
	"intervalUnitsHours",
	&LiveTimeFormUI::intervalUnitsActivateCallback, (XtPointer) this);
    _intervalUnitsDays = _unitsOption->addAction(
	"intervalUnitsDays",
	&LiveTimeFormUI::intervalUnitsActivateCallback, (XtPointer) this);
    _intervalUnitsWeeks = _unitsOption->addAction(
	"intervalUnitsWeeks",
	&LiveTimeFormUI::intervalUnitsActivateCallback, (XtPointer) this);

    count = 0;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNleftPosition, 0); count++;
    XtSetArg(args[count], XmNrightPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNrightOffset, 4); count++;
    XtSetArg(args[count], XmNwidth, 161); count++;
    XtSetArg(args[count], XmNheight, 32); count++;
    XtSetValues(_unitsOption->baseWidget(), args, count);

    count = 0;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNleftPosition, 0); count++;
    XtSetArg(args[count], XmNrightPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNleftOffset, 70); count++;
    XtSetArg(args[count], XmNrightOffset, 165); count++;
    XtSetArg(args[count], XmNwidth, 94); count++;
    _interval = XtCreateWidget("interval", xmTextFieldWidgetClass,
						_baseWidget, args, count);
    XtAddCallback(_interval, XmNactivateCallback,
	&LiveTimeFormUI::intervalActivateCallback, (XtPointer) this);
    XtAddCallback (_interval, XmNvalueChangedCallback,
	&LiveTimeFormUI::intervalValueChangedCallback, (XtPointer) this);

    count = 0;
    XtSetArg(args[count], XmNalignment, XmALIGNMENT_BEGINNING); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNleftPosition, 0); count++;
    XtSetArg(args[count], XmNrightPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 2); count++;
    XtSetArg(args[count], XmNleftOffset, 5); count++;
    XtSetArg(args[count], XmNwidth, 84); count++;
    XtSetArg(args[count], XmNheight, 38); count++;
    _intervalLabel = XmCreateLabelGadget(_baseWidget, "intervalLabel", args, count);

    count = 0;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _interval); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNleftPosition, 0); count++;
    XtSetArg(args[count], XmNrightPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNleftOffset, 70); count++;
    XtSetArg(args[count], XmNrightOffset, 5); count++;
    XtSetArg(args[count], XmNwidth, 254); count++;
    XtSetArg(args[count], XmNeditable, False); count++;
    XtSetArg(args[count], XmNtraversalOn, False); count++;
    XtSetArg(args[count], XmNcursorPositionVisible, False); count++;
    _position = XtCreateWidget("position", xmTextFieldWidgetClass,
					_baseWidget, args, count);
    Pixel readonlybg = (Pixel) VkGetResource(_position,
	"readOnlyBackground", "ReadOnlyBackground", XmRPixel, "Gray72");
    Pixel readonlyfg = (Pixel) VkGetResource(_position,
	"readOnlyForeground", "ReadOnlyForeground", XmRPixel, "Black");
    count = 0;
    XtSetArg(args[count], XmNbackground, readonlybg); count++;
    XtSetArg(args[count], XmNforeground, readonlyfg); count++;
    XtSetValues(_position, args, count);
    XtAddCallback(_position, XmNactivateCallback,
	&LiveTimeFormUI::positionActivateCallback, (XtPointer) this);
    XtAddCallback(_position, XmNvalueChangedCallback,
	&LiveTimeFormUI::positionValueChangedCallback, (XtPointer) this); 

    count = 0;
    XtSetArg(args[count], XmNalignment, XmALIGNMENT_BEGINNING); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _interval); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNleftOffset, 5); count++;
    XtSetArg(args[count], XmNwidth, 40); count++;
    XtSetArg(args[count], XmNheight, 30); count++;
    _timeLabel = XmCreateLabelGadget(_baseWidget, "timeLabel", args, count);

    count = 0;
    XtSetArg(args[count], XmNalignment, XmALIGNMENT_BEGINNING); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _position); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 17); count++;
    XtSetArg(args[count], XmNleftOffset, 5); count++;
    XtSetArg(args[count], XmNwidth, 145); count++;
    XtSetArg(args[count], XmNheight, 20); count++;
    _realtimeLabel = XmCreateLabelGadget(_baseWidget, "realtimeLabel", args, count);

    count = 0;
    XtSetArg(args[count], XmNlabelType, XmPIXMAP); count++;
    XtSetArg(args[count], XmNpushButtonEnabled, True); count++;
    XtSetArg(args[count], XmNshadowType, XmSHADOW_OUT); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _position); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNleftPosition, 0); count++;
    XtSetArg(args[count], XmNrightPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNrightOffset, 5); count++;
    XtSetArg(args[count], XmNwidth, 63); count++;
    XtSetArg(args[count], XmNheight, 52); count++;
    _vcrForward = XtCreateWidget("vcrForward", xmDrawnButtonWidgetClass,
					_baseWidget, args, count);
    XtAddCallback(_vcrForward, XmNactivateCallback,
		&LiveTimeFormUI::vcrActivateCallback, (XtPointer) this); 

    count = 0;
    XtSetArg(args[count], XmNlabelType, XmPIXMAP); count++;
    XtSetArg(args[count], XmNpushButtonEnabled, True); count++;
    XtSetArg(args[count], XmNshadowType, XmSHADOW_OUT); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _position); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNrightWidget, _vcrForward); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNleftPosition, 0); count++;
    XtSetArg(args[count], XmNrightPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNleftOffset, 193); count++;
    XtSetArg(args[count], XmNrightOffset, 5); count++;
    XtSetArg(args[count], XmNwidth, 63); count++;
    XtSetArg(args[count], XmNheight, 52); count++;
    _vcrStop = XtCreateWidget("vcrStop", xmDrawnButtonWidgetClass,
					_baseWidget, args, count);
    XtAddCallback(_vcrStop, XmNactivateCallback,
		&LiveTimeFormUI::vcrActivateCallback, (XtPointer) this); 

#define FORM_CHILDREN	7
    XtManageChildren(&_interval, FORM_CHILDREN);

#ifdef PCP_PROFILE
    __pmEventTrace("end widgets");
#endif
}

const char *
LiveTimeFormUI::className()
{
    return ("LiveTimeFormUI");
}


/////////////////////////////////////////////////////////////// 
// The following functions are static member functions used to 
// interface with Motif.
/////////////////////////////////// 

void
LiveTimeFormUI::intervalActivateCallback(Widget w,
				XtPointer clientData, XtPointer callData)
{ 
    LiveTimeFormUI* obj = (LiveTimeFormUI *)clientData;
    obj->intervalActivate(w, callData);
}

void
LiveTimeFormUI::intervalUnitsActivateCallback(Widget w,
				XtPointer clientData, XtPointer callData)
{
    LiveTimeFormUI* obj = (LiveTimeFormUI *)clientData;
    obj->intervalUnitsActivate(w, callData);
}

void
LiveTimeFormUI::intervalValueChangedCallback(Widget w,
				XtPointer clientData, XtPointer callData ) 
{ 
    LiveTimeFormUI* obj = (LiveTimeFormUI *)clientData;
    obj->intervalValueChanged(w, callData);
}

void
LiveTimeFormUI::positionActivateCallback(Widget w,
				XtPointer clientData, XtPointer callData)
{ 
    LiveTimeFormUI* obj = (LiveTimeFormUI *)clientData;
    obj->positionActivate(w, callData);
}

void
LiveTimeFormUI::positionValueChangedCallback(Widget w,
				XtPointer clientData, XtPointer callData)
{ 
    LiveTimeFormUI* obj = (LiveTimeFormUI *)clientData;
    obj->positionValueChanged(w, callData);
}

void
LiveTimeFormUI::vcrActivateCallback(Widget w,
				XtPointer clientData, XtPointer callData)
{ 
    LiveTimeFormUI* obj = (LiveTimeFormUI *) clientData;
    obj->vcrActivate(w, callData);
}


/////////////////////////////////////////////////////////////// 
// The following functions are called from the menu items 
// in this window.
/////////////////////////////////// 

void LiveTimeFormUI::intervalActivate(Widget, XtPointer)
{
    // This virtual function is called from intervalActivateCallback.
    // This function is normally overriden by a derived class.
}

void LiveTimeFormUI::intervalUnitsActivate(Widget, XtPointer)
{
    // This virtual function is called from intervalUnitsActivateCallback.
    // This function is normally overriden by a derived class.
}

void LiveTimeFormUI::intervalValueChanged(Widget, XtPointer)
{
    // This virtual function is called from intervalValueChangedCallback.
    // This function is normally overriden by a derived class.
}

void LiveTimeFormUI::positionActivate(Widget, XtPointer)
{
    // This virtual function is called from positionActivateCallback.
    // This function is normally overriden by a derived class.
}

void LiveTimeFormUI::positionValueChanged(Widget, XtPointer)
{
    // This virtual function is called from positionValueChangedCallback.
    // This function is normally overriden by a derived class.
}

void LiveTimeFormUI::vcrActivate(Widget, XtPointer)
{
    // This virtual function is called from vcrActivateCallback.
    // This function is normally overriden by a derived class.
}

