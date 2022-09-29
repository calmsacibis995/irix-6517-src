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

//
// $Id: VkPCParchiveTime.c++,v 1.5 1997/08/21 04:30:26 markgw Exp $
//

//////////////////////////////////////////////////////////////
//
// Source file for VkPCParchiveTime
//
//    This class is a ViewKit VkWindow subclass
//
//
// Normally, very little in this file should need to be changed.
// Create/add/modify menus using the builder.
//
// Try to restrict any changes to the bodies of functions
// corresponding to menu items, the constructor and destructor.
//
// Add any new functions below the "//--- End generated code"
// markers
//
// Doing so will allow you to make changes using the builder
// without losing any changes you may have made manually
//
// Avoid gratuitous reformatting and other changes that might
// make it difficult to integrate changes made using the builder
//////////////////////////////////////////////////////////////
#include "VkPCParchiveTime.h"
#include <Vk/VkApp.h>
#include <Vk/VkFileSelectionDialog.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkRadioSubMenu.h>
#include <Vk/VkMenuItem.h>
#include <Xm/MwmUtil.h>
#include "ArchiveTimeForm.h"
//---- End generated headers


// These are default resources for widgets in objects of this class
// All resources will be prepended by *<name> at instantiation,
// where <name> is the name of the specific instance, as well as the
// name of the baseWidget. These are only defaults, and may be overriden
// in a resource file by providing a more specific resource name

String  VkPCParchiveTime::_defaultVkPCParchiveTimeResources[] = {
        "*title:  PCP Archive Time Controls",
        "*filePane2.labelString:  File",
        "*filePane2.mnemonic:  F",
        "*hideButton.labelString:  Hide",
        "*hideButton.mnemonic:  H",
        "*hideButton.accelerator:  ;",
        "*hideButton.acceleratorText:  ",
        "*optionsPane2.labelString:  Options",
        "*optionsPane2.mnemonic:  O",
        "*timezoneRadioPane.labelString:  Timezone",
        "*timezoneRadioPane.mnemonic:  T",
        "*timezoneUTC.labelString:  UTC",
        "*bounds.labelString:  Show Bounds ...",
        "*bounds.mnemonic:  B",
        "*bounds.acceleratorText:  ",
        "*menuPane.labelString:  Detail",
        "*menuPane.mnemonic:  D",
        "*detailedPositions.labelString:  Show Milliseconds",
        "*detailedPositions.mnemonic:  M",
        "*showYear.labelString:  Show Year",
        "*showYear.mnemonic:  Y",
        "*helpPane2.labelString:  Help",
        "*helpPane2.mnemonic:  H",
        "*help_click_for_help2.labelString:  Click For Help",
        "*help_click_for_help2.mnemonic:  C",
        "*help_click_for_help2.accelerator:  Shift<Key>F1",
        "*help_click_for_help2.acceleratorText:  Shift+F1",
        "*help_overview2.labelString:  Overview",
        "*help_overview2.mnemonic:  O",
        "*help_index2.labelString:  Index",
        "*help_index2.mnemonic:  I",
        "*help_keys_and_short2.labelString:  Keys and Shortcuts",
        "*help_keys_and_short2.mnemonic:  K",
        "*help_prod_info2.labelString:  Product Information",
        "*help_prod_info2.mnemonic:  P",
        (char*)NULL
};


//---- Class declaration

VkPCParchiveTime::VkPCParchiveTime( const char *name,
                                    ArgList args,
                                    Cardinal argCount) : 
                              VkWindow (name, args, argCount) 
{
    // Load any class-default resources for this object

    setDefaultResources(baseWidget(), _defaultVkPCParchiveTimeResources  );


    // Create the view component contained by this window

    _archiveTimeForm= new ArchiveTimeForm("archiveTimeForm",mainWindowWidget());


    XtVaSetValues ( _archiveTimeForm->baseWidget(),
                    XmNwidth, 325, 
#ifdef IRIX5_3
                    XmNheight, 198, 
#else
                    XmNheight, 205, 
#endif
                    (XtPointer) NULL );

    // Add the component as the main view

    addView (_archiveTimeForm);
    _archiveTimeForm->setParent(this);

    // Create the panes of this window's menubar. The menubar itself
    //  is created automatically by ViewKit


    // The filePane2 menu pane

    _filePane2 =  addMenuPane("filePane2");

    _hideButton =  _filePane2->addAction ( "hideButton", 
                                        &VkPCParchiveTime::hideButtonActivateCallback, 
                                        (XtPointer) this );

    // The optionsPane2 menu pane

    _optionsPane2 =  addMenuPane("optionsPane2");

    // The timezoneRadioPane menu pane

    _timezoneRadioPane =  _optionsPane2->addRadioSubmenu ( "timezoneRadioPane" );

    _bounds =  _optionsPane2->addAction ( "bounds", 
                                        &VkPCParchiveTime::boundsActivateCallback, 
                                        (XtPointer) this );

    // The menuPane menu pane

    _menuPane =  _optionsPane2->addSubmenu ( "menuPane" );

    _detailedPositions =  _menuPane->addToggle ( "detailedPositions", 
                                        &VkPCParchiveTime::detailedPositionsValueChangedCallback, 
                                        (XtPointer) this );

    _showYear =  _menuPane->addToggle ( "showYear", 
                                        &VkPCParchiveTime::showYearValueChangedCallback, 
                                        (XtPointer) this, True );


    //--- End generated code section



} // End Constructor


VkPCParchiveTime::~VkPCParchiveTime()
{
    delete _archiveTimeForm;
} // End destructor


const char *VkPCParchiveTime::className()
{
    return ("VkPCParchiveTime");
} // End className()


/////////////////////////////////////////////////////////////// 
// The following functions are static member functions used to 
// interface with Motif.
/////////////////////////////////// 

void VkPCParchiveTime::boundsActivateCallback ( Widget    w,
                                                XtPointer clientData,
                                                XtPointer callData ) 
{ 
    VkPCParchiveTime* obj = ( VkPCParchiveTime * ) clientData;
    obj->boundsActivate ( w, callData );
}

void VkPCParchiveTime::detailedPositionsValueChangedCallback ( Widget    w,
                                                               XtPointer clientData,
                                                               XtPointer callData ) 
{ 
    VkPCParchiveTime* obj = ( VkPCParchiveTime * ) clientData;
    obj->detailedPositionsValueChanged ( w, callData );
}

void VkPCParchiveTime::hideButtonActivateCallback ( Widget    w,
                                                    XtPointer clientData,
                                                    XtPointer callData ) 
{ 
    VkPCParchiveTime* obj = ( VkPCParchiveTime * ) clientData;
    obj->hideButtonActivate ( w, callData );
}

void VkPCParchiveTime::showYearValueChangedCallback ( Widget    w,
                                                      XtPointer clientData,
                                                      XtPointer callData ) 
{ 
    VkPCParchiveTime* obj = ( VkPCParchiveTime * ) clientData;
    obj->showYearValueChanged ( w, callData );
}

void VkPCParchiveTime::timezoneValueChangedCallback ( Widget    w,
                                                      XtPointer clientData,
                                                      XtPointer callData ) 
{ 
    VkPCParchiveTime* obj = ( VkPCParchiveTime * ) clientData;
    obj->timezoneValueChanged ( w, callData );
}





/////////////////////////////////////////////////////////////// 
// The following functions are called from the menu items 
// in this window.
/////////////////////////////////// 

void VkPCParchiveTime::boundsActivate ( Widget w, XtPointer callData ) 
{

    // This virtual function is called from boundsActivateCallback.
    // By default this member function passes control
    // to the component contained by this window

    _archiveTimeForm->boundsActivate( w, callData);



} // End VkPCParchiveTime::boundsActivate()


void VkPCParchiveTime::detailedPositionsValueChanged ( Widget w, XtPointer callData ) 
{

    // This virtual function is called from detailedPositionsValueChangedCallback.
    // By default this member function passes control
    // to the component contained by this window

    _archiveTimeForm->detailedPositionsValueChanged( w, callData);



} // End VkPCParchiveTime::detailedPositionsValueChanged()


void VkPCParchiveTime::hideButtonActivate ( Widget w, XtPointer callData ) 
{

    // This virtual function is called from hideButtonActivateCallback.
    // By default this member function passes control
    // to the component contained by this window

    _archiveTimeForm->hideButtonActivate( w, callData);



} // End VkPCParchiveTime::hideButtonActivate()


void VkPCParchiveTime::showYearValueChanged ( Widget w, XtPointer callData ) 
{

    // This virtual function is called from showYearValueChangedCallback.
    // By default this member function passes control
    // to the component contained by this window

    _archiveTimeForm->showYearValueChanged( w, callData);



} // End VkPCParchiveTime::showYearValueChanged()


void VkPCParchiveTime::timezoneValueChanged ( Widget w, XtPointer callData ) 
{

    // This virtual function is called from timezoneValueChangedCallback.
    // By default this member function passes control
    // to the component contained by this window

    _archiveTimeForm->timezoneValueChanged( w, callData);



} // End VkPCParchiveTime::timezoneValueChanged()





//---- End of generated code


int
VkPCParchiveTime::initialize(int control, int client, pmTime *state)
{
    return _archiveTimeForm->initialize(control, client, state);
}

int
VkPCParchiveTime::getDetailSetting()
{
    return _detailedPositions->getState();
}

void
VkPCParchiveTime::showDialog(int show)
{
    _archiveTimeForm->showDialog(show);
}


void
VkPCParchiveTime::addTimezone(char *label, char *tz, Boolean setItem)
{
    if (_timezoneRadioPane->findNamedItem(label) != NULL) {
        // ignore
        return;
    }

    VkMenuToggle *newtz = _timezoneRadioPane->addToggle(label,
        &VkPCParchiveTime::timezoneValueChangedCallback, (XtPointer)this);
    if (setItem)
	newtz->activate();
    __pmTimeAddTimezone(label, tz);
}

void
VkPCParchiveTime::setUpWindowProperties()
{
    Arg         args[2];
    Cardinal    count;
    int decor = MWM_DECOR_BORDER|MWM_DECOR_TITLE|MWM_DECOR_MENU|MWM_DECOR_MINIMIZE;
    int funcs = MWM_FUNC_MOVE|MWM_FUNC_MINIMIZE|MWM_FUNC_CLOSE;

    count = 0;
    XtSetArg(args[count], XmNmwmDecorations, decor); count++;
    XtSetArg(args[count], XmNmwmFunctions, funcs); count++;
    XtSetValues(baseWidget(), args, count);
}

