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
// Source file for LiveTimeForm
//
// $Id: LiveTimeForm.c++,v 1.4 1997/08/21 04:33:20 markgw Exp $
//
/////////////////////////////////////////////////////////////

#include "LiveTimeForm.h"
#include <Xm/DrawnB.h> 
#include <Xm/Form.h> 
#include <Xm/Label.h> 
#include <Xm/TextF.h> 
#include <Xm/ToggleB.h>
#include <Xm/Text.h>
#include <Vk/VkResource.h>
#include <Vk/VkWindow.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkInput.h>
#include <Vk/VkErrorDialog.h>

#include "../timer/VkPCPtimer.h"


///////////////////////////////////////////////////////////////////////////////
// The following non-container widgets are created by LiveTimeFormUI and are
// available as protected data members inherited by this class
//
//  XmLabelGadget	    _realtimeLabel
//  XmLabelGadget	    _timeLabel
//  XmDrawnButton	    _vcrForward
//  XmDrawnButton	    _vcrStop
//  XmTextField		    _position
//  XmTextField		    _interval
//  XmLabelGadget	    _intervalLabel
//
///////////////////////////////////////////////////////////////////////////////


LiveTimeForm::LiveTimeForm(const char *name, Widget parent) : 
                   LiveTimeFormUI(name, parent) 
{ 
    // This constructor calls LiveTimeFormUI(parent, name)
    // which calls LiveTimeFormUI::create() to create
    // the widgets for this component. Any code added here
    // is called after the component's interface has been built

    _runningSlow = 0;
}

LiveTimeForm::LiveTimeForm(const char *name) : 
                   LiveTimeFormUI(name) 
 { 
    // This constructor calls LiveTimeFormUI(name)
    // which does not create any widgets. Usually, this
    // constructor is not used

}

LiveTimeForm::~LiveTimeForm()
{
    // The base class destructors are responsible for
    // destroying all widgets and objects used in this component.
    // Only additional items created directly in this class
    // need to be freed here.

    close(_state->control);
    unlink(_state->port);
    free(_state);
}

const char * LiveTimeForm::className() // classname
{
    return ("LiveTimeForm");
} // End className()

/*ARGSUSED*/
void
LiveTimeForm::intervalActivate ( Widget w, XtPointer callData )
{
    char *val = XmTextGetString(w);
    setInterval(val);
    XtFree(val);
}

/*ARGSUSED*/
void
LiveTimeForm::intervalUnitsActivate ( Widget w, XtPointer callData )
{
    showInterval();
}

/*ARGSUSED*/
void
LiveTimeForm::intervalValueChanged ( Widget w, XtPointer callData )
{
}

/*ARGSUSED*/
void
LiveTimeForm::positionActivate ( Widget w, XtPointer callData )
{
}

/*ARGSUSED*/
void
LiveTimeForm::positionValueChanged ( Widget w, XtPointer callData )
{
}

/*ARGSUSED*/
void
LiveTimeForm::vcrActivate ( Widget w, XtPointer callData )
{
    if (w == _vcrStop) {
	setVcrMode(VCR_STOP);
    }
    else
    if (w == _vcrForward) {
	setVcrMode(VCR_LIVE);
    }
}

void LiveTimeForm::setParent( VkWindow  * parent )
{
    // Store a pointer to the parent VkWindow. This can
    // be useful for accessing the menubar from this class.

    _parent = parent;
}

/*ARGSUSED*/
void
LiveTimeForm::detailedPositionsValueChanged( Widget w, XtPointer callData )
{
    _detailedPositions = XmToggleButtonGetState(w);
    showPosition();
}

/*ARGSUSED*/
void
LiveTimeForm::hideButtonActivate( Widget w, XtPointer callData )
{
    showDialog(0);
}

/*ARGSUSED*/
void
LiveTimeForm::showYearValueChanged( Widget w, XtPointer callData )
{
    _showYear = XmToggleButtonGetState(w);
    showPosition();
}

/*ARGSUSED*/
void
LiveTimeForm::timezoneValueChanged( Widget w, XtPointer callData )
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


extern "C" VkComponent * CreateLiveTimeForm( const char *name, Widget parent ) 
{  
    VkComponent *obj =  new LiveTimeForm ( name, parent );
    obj->show();

    return ( obj );
}


///////////////////////////////////////////////////////////////////
// Function for importing this class into rapidapp
// This function is only used by RapidApp.
///////////////////////////////////////////////////////////////////


typedef void (LiveTimeForm::*Method) (void);
struct FunctionMap {
  char  *resource;  Method method;  char  *code;
  char  *type;
};


extern "C" void* RegisterLiveTimeFormInterface()
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
      // { "resourceName", (Method) &LiveTimeForm::setAttribute, "setAttribute", XmRString},
      { NULL }, // MUST be NULL terminated
    };

    return map;
}
