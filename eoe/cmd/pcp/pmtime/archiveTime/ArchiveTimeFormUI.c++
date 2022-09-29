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
// Source file for ArchiveTimeFormUI
//
// $Id: ArchiveTimeFormUI.c++,v 1.6 1999/04/30 01:44:04 kenmcd Exp $
//
/////////////////////////////////////////////////////////////


#include "pmapi.h"
#include "impl.h"
#include "ArchiveTimeFormUI.h"
#include <Sgm/ThumbWheel.h> 
#include <Xm/DrawnB.h> 
#include <Xm/Form.h> 
#include <Xm/Label.h>
#include <Xm/LabelG.h> 
#include <Xm/Scale.h> 
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

String  ArchiveTimeFormUI::_defaultArchiveTimeFormUIResources[] = {
        "*vcrOption.labelString:  ",
        "*vcrOptionPlay.labelString:  Normal",
        "*vcrOptionStep.labelString:  Step",
        "*vcrOptionFast.labelString:  Fast",
        "*vcrLabel.labelString:  VCR Controls",
        "*positionLabel.labelString:  Position",
        "*speedLabel.labelString:  Speed",
        "*unitsOption.labelString:  ",
        "*intervalUnitsMilliseconds.labelString:  Milliseconds",
        "*intervalUnitsSeconds.labelString:  Seconds",
        "*intervalUnitsMinutes.labelString:  Minutes",
        "*intervalUnitsHours.labelString:  Hours",
        "*intervalUnitsDays.labelString:  Days",
        "*intervalUnitsWeeks.labelString:  Weeks",
        "*intervalLabel.labelString:  Interval",
#if !defined(sgi)
	"*buttonFontList: -*-helvetica-medium-r-*-*-*-120-75-75-*-*-iso8859-1",
	"*labelFontList: -*-helvetica-medium-r-*-*-*-140-75-75-*-*-iso8859-1",
#endif
        (char*)NULL
};

ArchiveTimeFormUI::ArchiveTimeFormUI(const char *name) : VkComponent(name) 
{ 
    // No widgets are created by this constructor.
    // If an application creates a component using this constructor,
    // It must explictly call create at a later time.
    // This is mostly useful when adding pre-widget creation
    // code to a derived class constructor.

}

ArchiveTimeFormUI::ArchiveTimeFormUI(const char *name, Widget parent) : VkComponent(name) 
{ 
    // Call creation function to build the widget tree.
    create(parent);

}

ArchiveTimeFormUI::~ArchiveTimeFormUI() 
{
}


void
ArchiveTimeFormUI::create(Widget parent)
{
    Arg		args[32];
    Cardinal	count;

#ifdef PCP_PROFILE
    __pmEventTrace("start widgets");
#endif

    // Load any class-defaulted resources for this object
    setDefaultResources(parent, _defaultArchiveTimeFormUIResources);

    count = 0;
    XtSetArg(args[count], XmNresizePolicy, XmRESIZE_GROW); count++;
    _baseWidget = _archiveTimeForm = XtCreateWidget(_name,
				xmFormWidgetClass, parent, args, count);

    // install a callback to guard against unexpected widget destruction
    installDestroyHandler();

    // Create widgets used in this component
    // All variables are data members of this class

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
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNleftPosition, 0); count++;
    XtSetArg(args[count], XmNrightPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNleftOffset, 70); count++;
    XtSetArg(args[count], XmNrightOffset, 165); count++;
    XtSetArg(args[count], XmNwidth, 90); count++;
    _interval = XtCreateWidget("interval", xmTextFieldWidgetClass,
					_baseWidget, args, count);
    XtAddCallback(_interval, XmNactivateCallback,
	&ArchiveTimeFormUI::intervalActivateCallback, (XtPointer) this);
    XtAddCallback(_interval, XmNvalueChangedCallback,
	&ArchiveTimeFormUI::intervalValueChangedCallback, (XtPointer) this);

    _unitsOption = new VkOptionMenu(_baseWidget);
    _intervalUnitsMilliseconds = _unitsOption->addAction(
	"intervalUnitsMilliseconds", 
	&ArchiveTimeFormUI::intervalUnitsActivateCallback, (XtPointer) this);
    _intervalUnitsSeconds = _unitsOption->addAction(
	"intervalUnitsSeconds", 
	&ArchiveTimeFormUI::intervalUnitsActivateCallback, (XtPointer) this);
    _intervalUnitsMinutes = _unitsOption->addAction(
	"intervalUnitsMinutes",
	&ArchiveTimeFormUI::intervalUnitsActivateCallback, (XtPointer) this);
    _intervalUnitsHours = _unitsOption->addAction(
	"intervalUnitsHours", 
	&ArchiveTimeFormUI::intervalUnitsActivateCallback, (XtPointer) this);
    _intervalUnitsDays = _unitsOption->addAction(
	"intervalUnitsDays", 
	&ArchiveTimeFormUI::intervalUnitsActivateCallback, (XtPointer) this);
    _intervalUnitsWeeks = _unitsOption->addAction(
	"intervalUnitsWeeks", 
	&ArchiveTimeFormUI::intervalUnitsActivateCallback, (XtPointer) this);

    count = 0;
    XtSetArg(args[count], XmNorientation, XmHORIZONTAL); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _interval); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNleftPosition, 0); count++;
    XtSetArg(args[count], XmNrightPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 10); count++;
    XtSetArg(args[count], XmNrightOffset, 5); count++;
    XtSetArg(args[count], XmNwidth, 152); count++;
    XtSetArg(args[count], XmNheight, 24); count++;
    _speedThumb = XtCreateWidget("speedThumb", sgThumbWheelWidgetClass,
					_baseWidget, args, count);
    XtAddCallback(_speedThumb, XmNvalueChangedCallback,
	&ArchiveTimeFormUI::speedThumbValueChangedCallback, (XtPointer) this);
    XtAddCallback(_speedThumb, XmNdragCallback,
	&ArchiveTimeFormUI::speedThumbDragCallback, (XtPointer) this);

    count = 0;
    XtSetArg(args[count], XmNalignment, XmALIGNMENT_BEGINNING); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _interval); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNleftOffset, 6); count++;
    XtSetArg(args[count], XmNwidth, 48); count++;
    XtSetArg(args[count], XmNheight, 30); count++;
    _speedLabel = XmCreateLabelGadget(_baseWidget, "speedLabel", args, count);

    count = 0;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _interval); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNleftPosition, 0); count++;
    XtSetArg(args[count], XmNrightPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 6); count++;
    XtSetArg(args[count], XmNleftOffset, 70); count++;
    XtSetArg(args[count], XmNrightOffset, 165); count++;
    XtSetArg(args[count], XmNwidth, 90); count++;
    _speed = XtCreateWidget("speed", xmTextFieldWidgetClass,
						_baseWidget, args, count);
    XtAddCallback(_speed, XmNactivateCallback,
	&ArchiveTimeFormUI::speedActivateCallback, (XtPointer) this);
    XtAddCallback(_speed, XmNvalueChangedCallback,
	&ArchiveTimeFormUI::speedValueChangedCallback, (XtPointer) this);

    count = 0;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _speed); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNleftPosition, 0); count++;
    XtSetArg(args[count], XmNrightPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNleftOffset, 70); count++;
    XtSetArg(args[count], XmNrightOffset, 5); count++;
    XtSetArg(args[count], XmNwidth, 250); count++;
    _position = XtCreateWidget("position", xmTextFieldWidgetClass,
					_baseWidget, args, count);
    XtAddCallback(_position, XmNactivateCallback,
	&ArchiveTimeFormUI::positionActivateCallback, (XtPointer) this);
    XtAddCallback(_position, XmNvalueChangedCallback,
	&ArchiveTimeFormUI::positionValueChangedCallback, (XtPointer) this);

    count = 0;
    XtSetArg(args[count], XmNorientation, XmHORIZONTAL); count++;
    XtSetArg(args[count], XmNshowValue, False); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _position); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNbottomPosition, 0); count++;
    XtSetArg(args[count], XmNrightPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 0); count++;
    XtSetArg(args[count], XmNleftOffset, 5); count++;
    XtSetArg(args[count], XmNrightOffset, 5); count++;
    XtSetArg(args[count], XmNwidth, 315); count++;
    XtSetArg(args[count], XmNheight, 20); count++;
    _positionScale = XtCreateWidget("positionScale", xmScaleWidgetClass,
					_baseWidget, args, count);
    XtAddCallback(_positionScale, XmNvalueChangedCallback,
	&ArchiveTimeFormUI::positionScaleValueChangedCallback, (XtPointer) this);
    XtAddCallback(_positionScale, XmNdragCallback,
	&ArchiveTimeFormUI::positionScaleDragCallback, (XtPointer) this);

    count = 0;
    XtSetArg(args[count], XmNalignment, XmALIGNMENT_BEGINNING); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _speed); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNleftOffset, 6); count++;
    XtSetArg(args[count], XmNwidth, 62); count++;
    XtSetArg(args[count], XmNheight, 30); count++;
    _positionLabel = XmCreateLabelGadget(_baseWidget, "positionLabel", args, count);

    count = 0;
    XtSetArg(args[count], XmNlabelType, XmPIXMAP); count++;
    XtSetArg(args[count], XmNpushButtonEnabled, True); count++;
    XtSetArg(args[count], XmNshadowType, XmSHADOW_OUT); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _positionScale); count++;
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
		&ArchiveTimeFormUI::vcrActivateCallback, (XtPointer) this);

    count = 0;
    XtSetArg(args[count], XmNlabelType, XmPIXMAP); count++;
    XtSetArg(args[count], XmNpushButtonEnabled, True); count++;
    XtSetArg(args[count], XmNshadowType, XmSHADOW_OUT); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _positionScale); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNbottomPosition, 0); count++;
    XtSetArg(args[count], XmNleftPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNleftOffset, 125); count++;
    XtSetArg(args[count], XmNwidth, 63); count++;
    XtSetArg(args[count], XmNheight, 52); count++;
    _vcrReverse = XtCreateWidget("vcrReverse", xmDrawnButtonWidgetClass,
					_baseWidget, args, count);
    XtAddCallback(_vcrReverse, XmNactivateCallback,
		&ArchiveTimeFormUI::vcrActivateCallback, (XtPointer) this);

    count = 0;
    XtSetArg(args[count], XmNlabelType, XmPIXMAP); count++;
    XtSetArg(args[count], XmNpushButtonEnabled, True); count++;
    XtSetArg(args[count], XmNshadowType, XmSHADOW_OUT); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _positionScale); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNleftWidget, _vcrReverse); count++;
    XtSetArg(args[count], XmNrightAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNrightWidget, _vcrForward); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNleftPosition, 0); count++;
    XtSetArg(args[count], XmNrightPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNleftOffset, 5); count++;
    XtSetArg(args[count], XmNrightOffset, 5); count++;
    XtSetArg(args[count], XmNwidth, 59); count++;
    XtSetArg(args[count], XmNheight, 52); count++;
    _vcrStop = XtCreateWidget("vcrStop", xmDrawnButtonWidgetClass,
						_baseWidget, args, count);
    XtAddCallback(_vcrStop, XmNactivateCallback,
		&ArchiveTimeFormUI::vcrActivateCallback, (XtPointer) this);

    count = 0;
    XtSetArg(args[count], XmNalignment, XmALIGNMENT_BEGINNING); count++;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _positionScale); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 5); count++;
    XtSetArg(args[count], XmNleftOffset, 5); count++;
    XtSetArg(args[count], XmNwidth, 102); count++;
    XtSetArg(args[count], XmNheight, 20); count++;
    _vcrLabel = XmCreateLabelGadget(_baseWidget, "vcrLabel", args, count);

    _vcrOption = new VkOptionMenu(_baseWidget);
    _vcrOptionPlay = _vcrOption->addAction("vcrOptionPlay",
	&ArchiveTimeFormUI::vcrOptionActivateCallback, (XtPointer) this);
    _vcrOptionStep = _vcrOption->addAction("vcrOptionStep",
	&ArchiveTimeFormUI::vcrOptionActivateCallback, (XtPointer) this);
    _vcrOptionFast = _vcrOption->addAction("vcrOptionFast",
	&ArchiveTimeFormUI::vcrOptionActivateCallback, (XtPointer) this);

    count = 0;
    XtSetArg(args[count], XmNtopAttachment, XmATTACH_WIDGET); count++;
    XtSetArg(args[count], XmNtopWidget, _vcrLabel); count++;
    XtSetArg(args[count], XmNleftAttachment, XmATTACH_FORM); count++;
    XtSetArg(args[count], XmNtopPosition, 0); count++;
    XtSetArg(args[count], XmNtopOffset, 1); count++;
    XtSetArg(args[count], XmNleftOffset, 0); count++;
    XtSetArg(args[count], XmNwidth, 124); count++;
    XtSetArg(args[count], XmNheight, 32); count++;
    XtSetValues(_vcrOption->baseWidget(), args, count);

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

#define FORM_CHILDREN	12
    XtManageChildren(&_interval, FORM_CHILDREN);

#ifdef PCP_PROFILE
    __pmEventTrace("end widgets");
#endif
}

const char * ArchiveTimeFormUI::className()
{
    return ("ArchiveTimeFormUI");
} // End className()




/////////////////////////////////////////////////////////////// 
// The following functions are static member functions used to 
// interface with Motif.
/////////////////////////////////// 

void ArchiveTimeFormUI::intervalActivateCallback ( Widget    w,
                                                   XtPointer clientData,
                                                   XtPointer callData ) 
{ 
    ArchiveTimeFormUI* obj = ( ArchiveTimeFormUI * ) clientData;
    obj->intervalActivate ( w, callData );
}

void ArchiveTimeFormUI::intervalUnitsActivateCallback ( Widget    w,
                                                        XtPointer clientData,
                                                        XtPointer callData ) 
{ 
    ArchiveTimeFormUI* obj = ( ArchiveTimeFormUI * ) clientData;
    obj->intervalUnitsActivate ( w, callData );
}

void ArchiveTimeFormUI::intervalValueChangedCallback ( Widget    w,
                                                       XtPointer clientData,
                                                       XtPointer callData ) 
{ 
    ArchiveTimeFormUI* obj = ( ArchiveTimeFormUI * ) clientData;
    obj->intervalValueChanged ( w, callData );
}

void ArchiveTimeFormUI::positionActivateCallback ( Widget    w,
                                                   XtPointer clientData,
                                                   XtPointer callData ) 
{ 
    ArchiveTimeFormUI* obj = ( ArchiveTimeFormUI * ) clientData;
    obj->positionActivate ( w, callData );
}

void ArchiveTimeFormUI::positionScaleDragCallback ( Widget    w,
                                                    XtPointer clientData,
                                                    XtPointer callData ) 
{ 
    ArchiveTimeFormUI* obj = ( ArchiveTimeFormUI * ) clientData;
    obj->positionScaleDrag ( w, callData );
}

void ArchiveTimeFormUI::positionScaleValueChangedCallback ( Widget    w,
                                                            XtPointer clientData,
                                                            XtPointer callData ) 
{ 
    ArchiveTimeFormUI* obj = ( ArchiveTimeFormUI * ) clientData;
    obj->positionScaleValueChanged ( w, callData );
}

void ArchiveTimeFormUI::positionValueChangedCallback ( Widget    w,
                                                       XtPointer clientData,
                                                       XtPointer callData ) 
{ 
    ArchiveTimeFormUI* obj = ( ArchiveTimeFormUI * ) clientData;
    obj->positionValueChanged ( w, callData );
}

void ArchiveTimeFormUI::speedActivateCallback ( Widget    w,
                                                XtPointer clientData,
                                                XtPointer callData ) 
{ 
    ArchiveTimeFormUI* obj = ( ArchiveTimeFormUI * ) clientData;
    obj->speedActivate ( w, callData );
}

void ArchiveTimeFormUI::speedThumbDragCallback ( Widget    w,
                                                 XtPointer clientData,
                                                 XtPointer callData ) 
{ 
    ArchiveTimeFormUI* obj = ( ArchiveTimeFormUI * ) clientData;
    obj->speedThumbDrag ( w, callData );
}

void ArchiveTimeFormUI::speedThumbValueChangedCallback ( Widget    w,
                                                         XtPointer clientData,
                                                         XtPointer callData ) 
{ 
    ArchiveTimeFormUI* obj = ( ArchiveTimeFormUI * ) clientData;
    obj->speedThumbValueChanged ( w, callData );
}

void ArchiveTimeFormUI::speedValueChangedCallback ( Widget    w,
                                                    XtPointer clientData,
                                                    XtPointer callData ) 
{ 
    ArchiveTimeFormUI* obj = ( ArchiveTimeFormUI * ) clientData;
    obj->speedValueChanged ( w, callData );
}

void ArchiveTimeFormUI::vcrActivateCallback ( Widget    w,
                                              XtPointer clientData,
                                              XtPointer callData ) 
{ 
    ArchiveTimeFormUI* obj = ( ArchiveTimeFormUI * ) clientData;
    obj->vcrActivate ( w, callData );
}

void ArchiveTimeFormUI::vcrOptionActivateCallback ( Widget    w,
                                                    XtPointer clientData,
                                                    XtPointer callData ) 
{ 
    ArchiveTimeFormUI* obj = ( ArchiveTimeFormUI * ) clientData;
    obj->vcrOptionActivate ( w, callData );
}



/////////////////////////////////////////////////////////////// 
// The following functions are called from the menu items 
// in this window.
/////////////////////////////////// 

void ArchiveTimeFormUI::intervalActivate ( Widget, XtPointer ) 
{
    // This virtual function is called from intervalActivateCallback.
    // This function is normally overriden by a derived class.

}

void ArchiveTimeFormUI::intervalUnitsActivate ( Widget, XtPointer ) 
{
    // This virtual function is called from intervalUnitsActivateCallback.
    // This function is normally overriden by a derived class.

}

void ArchiveTimeFormUI::intervalValueChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from intervalValueChangedCallback.
    // This function is normally overriden by a derived class.

}

void ArchiveTimeFormUI::positionActivate ( Widget, XtPointer ) 
{
    // This virtual function is called from positionActivateCallback.
    // This function is normally overriden by a derived class.

}

void ArchiveTimeFormUI::positionScaleDrag ( Widget, XtPointer ) 
{
    // This virtual function is called from positionScaleDragCallback.
    // This function is normally overriden by a derived class.

}

void ArchiveTimeFormUI::positionScaleValueChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from positionScaleValueChangedCallback.
    // This function is normally overriden by a derived class.

}

void ArchiveTimeFormUI::positionValueChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from positionValueChangedCallback.
    // This function is normally overriden by a derived class.

}

void ArchiveTimeFormUI::speedActivate ( Widget, XtPointer ) 
{
    // This virtual function is called from speedActivateCallback.
    // This function is normally overriden by a derived class.

}

void ArchiveTimeFormUI::speedThumbDrag ( Widget, XtPointer ) 
{
    // This virtual function is called from speedThumbDragCallback.
    // This function is normally overriden by a derived class.

}

void ArchiveTimeFormUI::speedThumbValueChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from speedThumbValueChangedCallback.
    // This function is normally overriden by a derived class.

}

void ArchiveTimeFormUI::speedValueChanged ( Widget, XtPointer ) 
{
    // This virtual function is called from speedValueChangedCallback.
    // This function is normally overriden by a derived class.

}

void ArchiveTimeFormUI::vcrActivate ( Widget, XtPointer ) 
{
    // This virtual function is called from vcrActivateCallback.
    // This function is normally overriden by a derived class.

}

void ArchiveTimeFormUI::vcrOptionActivate ( Widget, XtPointer ) 
{
    // This virtual function is called from vcrOptionActivateCallback.
    // This function is normally overriden by a derived class.

}

//---- End generated code section

