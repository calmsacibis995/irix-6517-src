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
// Source file for ArchiveTimeForm
//
// $Id: ArchiveTimeForm.c++,v 1.7 1997/08/21 04:29:14 markgw Exp $
//
/////////////////////////////////////////////////////////////

#include "ArchiveTimeForm.h"
#include <Sgm/ThumbWheel.h> 
#include <Xm/DrawnB.h> 
#include <Xm/Form.h> 
#include <Xm/Label.h> 
#include <Xm/Scale.h> 
#include <Xm/TextF.h> 
#include <Xm/ToggleB.h>
#include <Xm/Text.h>
#include <Vk/VkResource.h>
#include <Vk/VkWindow.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkOptionMenu.h>
#include <Vk/VkMenuItem.h>
#include <Vk/VkInput.h>
#include <Vk/VkErrorDialog.h>

#ifdef IRIX5_3
#include "VkPixmap.h"
#else
#include <Vk/VkPixmap.h>
#endif

#include <stdio.h>
#include "pixmaps.h"
#include "../timer/VkPCPtimer.h"

///////////////////////////////////////////////////////////////////////////////
// The following non-container widgets are created by ArchiveTimeFormUI and are
// available as protected data members inherited by this class
//
//  XmLabelGadget	    _vcrLabel
//  XmLabelGadget	    _positionLabel
//  XmLabelGadget	    _speedLabel
//  XmDrawnButton	    _vcrForward
//  XmDrawnButton	    _vcrStop
//  XmDrawnButton	    _vcrReverse
//  XmScale		    _positionScale
//  XmTextField		    _position
//  SgThumbWheel	    _speedThumb
//  XmTextField		    _speed
//  XmTextField		    _interval
//  XmLabelGadget	    _intervalLabel
//
///////////////////////////////////////////////////////////////////////////////


ArchiveTimeForm::ArchiveTimeForm(const char *name, Widget parent) : 
                   ArchiveTimeFormUI(name, parent) 
{ 
    // This constructor calls ArchiveTimeFormUI(parent, name)
    // which calls ArchiveTimeFormUI::create() to create
    // the widgets for this component. Any code added here
    // is called after the component's interface has been built

    _archiveBounds = new ArchiveBounds("Archive Time Bounds", parent);
    _archiveBounds->addCallback(ArchiveBounds::boundsChanged,
	this, (VkCallbackMethod) &ArchiveTimeForm::boundsChangedCB);

    _runningSlow = 0;

}


ArchiveTimeForm::ArchiveTimeForm(const char *name) : 
                   ArchiveTimeFormUI(name) 
 { 
    // This constructor calls ArchiveTimeFormUI(name)
    // which does not create any widgets. Usually, this
    // constructor is not used

}


ArchiveTimeForm::~ArchiveTimeForm()
{
    // The base class destructors are responsible for
    // destroying all widgets and objects used in this component.
    // Only additional items created directly in this class
    // need to be freed here.

    close(_state->control);
    unlink(_state->port);
    free(_state);
}


const char *
ArchiveTimeForm::className()
{
    return ("ArchiveTimeForm");
}

/*ARGSUSED*/
void
ArchiveTimeForm::intervalActivate(Widget w, XtPointer callData)
{
    char *val = XmTextGetString(w);
    setInterval(val);
    XtFree(val);
}

/*ARGSUSED*/
void
ArchiveTimeForm::intervalUnitsActivate(Widget w, XtPointer callData)
{
    showInterval();
}

/*ARGSUSED*/
void
ArchiveTimeForm::intervalValueChanged(Widget w, XtPointer callData)
{
}


/*ARGSUSED*/
void
ArchiveTimeForm::positionActivate(Widget w, XtPointer callData)
{
    char *val = XmTextGetString(w);
    setPosition(val);
    if (val)
	XtFree(val);
}

/*ARGSUSED*/
void
ArchiveTimeForm::positionScaleDrag(Widget w, XtPointer callData)
{
    XmScaleCallbackStruct *cbs = (XmScaleCallbackStruct*) callData;

    setIndicatorState(PM_TCTL_VCRMODE_DRAG); // start dragging
    setPosition(cbs->value);
}

/*ARGSUSED*/
void
ArchiveTimeForm::positionScaleValueChanged(Widget w, XtPointer callData)
{
    XmScaleCallbackStruct *cbs = (XmScaleCallbackStruct*) callData;

    setIndicatorState(PM_TCTL_VCRMODE_STOP); // stop dragging
    setPosition(cbs->value);
}

/*ARGSUSED*/
void
ArchiveTimeForm::positionValueChanged(Widget w, XtPointer callData)
{
}

/*ARGSUSED*/
void
ArchiveTimeForm::speedActivate(Widget w, XtPointer callData)
{
    char *val = XmTextGetString(w);
    setSpeed(val);
    XtFree(val);
}

static double
calculateSpeed(int setting)
{
    double lv = 1.0;

    if (setting < 0) {
	// 1/100th decrements for less than real-time
	lv += setting * 0.01;
    }
    else {
	// increments of 1.0 for faster than real-time
	lv += setting;
    }
    return lv;
}

/*ARGSUSED*/
void
ArchiveTimeForm::speedThumbDrag(Widget w, XtPointer callData)
{
    SgThumbWheelCallbackStruct *cbs = (SgThumbWheelCallbackStruct*) callData;

    setSpeed(calculateSpeed(cbs->value));
}

/*ARGSUSED*/
void
ArchiveTimeForm::speedThumbValueChanged(Widget w, XtPointer callData)
{
    SgThumbWheelCallbackStruct *cbs = (SgThumbWheelCallbackStruct*) callData;

    setSpeed(calculateSpeed(cbs->value));
}

/*ARGSUSED*/
void
ArchiveTimeForm::speedValueChanged(Widget w, XtPointer callData)
{
}

void
ArchiveTimeForm::vcrActivate(Widget w, XtPointer callData)
{
    XmDrawnButtonCallbackStruct *cbs = (XmDrawnButtonCallbackStruct *) callData;
    VkMenuItem *i = _vcrOption->getItem();

    if (w == _vcrReverse) {
	if (i == _vcrOptionFast) {
	    setVcrMode(VCR_REV_FAST);	  // fast rewind
	}
	else if (i == _vcrOptionPlay) {
	    if (cbs->click_count == 1) {
		setVcrMode(VCR_REV_PLAY); // slow rewind
	    }
	    else {
		// Double click: promote to FAST
		_vcrOption->set(_vcrOptionFast);
		VkSetHighlightingPixmap (_vcrForward, ff_off);
		setVcrMode(VCR_REV_FAST);
	    }
	}
	else if (i == _vcrOptionStep) {
	    vcrStep(VCR_REVERSE);	  // reverse step
	}
    }
    else if (w == _vcrStop) {
	setVcrMode(VCR_STOP);		  // stop
    }
    else if (w == _vcrForward) {
	if (i == _vcrOptionFast) {
	    // fast forward
	    setVcrMode(VCR_FOR_FAST);
	}
	else if (i == _vcrOptionPlay) {
	    if (cbs->click_count == 1) {
		setVcrMode(VCR_FOR_PLAY); // slow forward
	    }
	    else {
		// Double click: promote to FAST
		_vcrOption->set(_vcrOptionFast);
		VkSetHighlightingPixmap (_vcrReverse, rw_off);
		setVcrMode(VCR_FOR_FAST);
	    }
	}
	else if (i == _vcrOptionStep) {
	    vcrStep(VCR_FORWARD);	  // step forward
	}
    }
}


/*ARGSUSED*/
void
ArchiveTimeForm::vcrOptionActivate(Widget w, XtPointer callData)
{
    vcrActivate (_vcrStop, NULL);
    if (w == _vcrOptionFast->baseWidget()) {
	VkSetHighlightingPixmap (_vcrReverse, rw_off);
	VkSetHighlightingPixmap (_vcrForward, ff_off);
    }
    else if (w == _vcrOptionStep->baseWidget()) {
	VkSetHighlightingPixmap (_vcrReverse, rstep_off);
	VkSetHighlightingPixmap (_vcrForward, step_off);
    }
    else if (w == _vcrOptionPlay->baseWidget()) {
	VkSetHighlightingPixmap (_vcrReverse, rev_off);
	VkSetHighlightingPixmap (_vcrForward, play_off);
    }

}

void
ArchiveTimeForm::setParent(VkWindow *parent)
{
    // Store a pointer to the parent VkWindow. This can
    // be useful for accessing the menubar from this class.
    _parent = parent;
}

/*ARGSUSED*/
void
ArchiveTimeForm::boundsActivate(Widget w, XtPointer callData)
{
    _archiveBounds->setTitle("Archive Time Bounds");
    _archiveBounds->show();
    _archiveBounds->showDetail(_detailedPositions, _showYear);
    // _archiveBounds->addBounds(&_state->data.start, &_state->data.finish, True);
}

/*ARGSUSED*/
void
ArchiveTimeForm::detailedPositionsValueChanged(Widget w, XtPointer callData)
{
    _detailedPositions = XmToggleButtonGetState(w);
    showPosition();
    if (_archiveBounds->isCreated() == True)
	_archiveBounds->showDetail(_detailedPositions, _showYear);
}

/*ARGSUSED*/
void
ArchiveTimeForm::hideButtonActivate(Widget w, XtPointer callData)
{
    showDialog(0);
}

/*ARGSUSED*/
void
ArchiveTimeForm::showYearValueChanged(Widget w, XtPointer callData)
{
    _showYear = XmToggleButtonGetState(w);
    showPosition();
    if (_archiveBounds->isCreated() == True)
	_archiveBounds->showDetail(_detailedPositions, _showYear);
}

/*ARGSUSED*/
void
ArchiveTimeForm::timezoneValueChanged(Widget w, XtPointer callData)
{
    XmString s;
    XtVaGetValues(w, XmNlabelString, &s, NULL);
    char *val;
    XmStringGetLtoR(s, XmFONTLIST_DEFAULT_TAG, &val);
    setTimezone(val);
}


///////////////////////////////////////////////////////////////////
// C-callable creation function, for importing class into rapidapp
///////////////////////////////////////////////////////////////////


extern "C" VkComponent * CreateArchiveTimeForm( const char *name, Widget parent ) 
{  
    VkComponent *obj =  new ArchiveTimeForm ( name, parent );
    obj->show();

    return ( obj );
} // End CreateArchiveTimeForm


///////////////////////////////////////////////////////////////////
// Function for importing this class into rapidapp
// This function is only used by RapidApp.
///////////////////////////////////////////////////////////////////


typedef void (ArchiveTimeForm::*Method) (void);
struct FunctionMap {
  char  *resource;  Method method;  char  *code;
  char  *type;
};


extern "C" void* RegisterArchiveTimeFormInterface()
{ 
    // This structure registers information about this class
    // that allows RapidApp to create and manipulate an instance.
    // Each entry provides a resource name that will appear in the
    // resource manager palette when an instance of this class is
    // selected, the relative address of the member function that
    // should be invoked when the resource is set, the name of the
    // member function as a string, and the type of the single 
    // argument to this function. All functions must have the form
    //  
    //  void memberFunction(Type);
    //
    // where "Type" is one of const char *, Boolean, or int. Use
    // XmRString, XmRBoolean, or XmRInt to describe the argument type


    static FunctionMap map[] = {
      // { "resourceName", (Method) &ArchiveTimeForm::setAttribute, "setAttribute", XmRString},
      { NULL }, // MUST be NULL terminated
    };

    return map;
} // End RegisterArchiveTimeFormInterface()


