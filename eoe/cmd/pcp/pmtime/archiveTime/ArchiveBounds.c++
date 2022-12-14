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


//////////////////////////////////////////////////////////////
//
// Source file for ArchiveBounds
//
//    This file is generated by RapidApp 1.2
//
//    This class is a ViewKit VkDialogManager subclass
//    See the VkDialogManager man page for info on the API
//
//    Restrict changes to those sections between
//    the "//--- Start/End editable code block" markers
//    This will allow RapidApp to integrate changes more easily
//
//    This class is a ViewKit user interface "component".
//    For more information on how ViewKit dialogs are used, see the
//    "ViewKit Programmers' Manual"
//
//////////////////////////////////////////////////////////////
#include "ArchiveBounds.h"


#include <Vk/VkApp.h>
#include <Vk/VkResource.h>
#include <Vk/VkSimpleWindow.h>


// Externally defined classes referenced by this class: 

#include "ArchiveBoundsForm.h"

extern void VkUnimplemented ( Widget, const char * );


//---- Start editable code block: headers and declarations

#include <time.h>
#include <Xm/Text.h>
#include <Xm/ToggleB.h>
#include <Xm/Form.h>

#include "pmapi.h"
#include "tv.h"

const char *const ArchiveBounds::boundsChanged = "boundsChanged";

String  ArchiveBounds::_defaultArchiveBoundsResources[] = {
        (char*)NULL
};



//---- End editable code block: headers and declarations


/*ARGSUSED*/
ArchiveBounds::ArchiveBounds(const char *name, Widget parent) : VkSimpleWindow(name)
{

    //---- Start editable code block:  constructor

    setDefaultResources(baseWidget(), _defaultArchiveBoundsResources);
    // install a callback to guard against unexpected widget destruction
    installDestroyHandler();

    // Add the component as the main view
    _archiveBoundsMainForm2 = new ArchiveBoundsForm("archiveBoundsMainForm2", mainWindowWidget());
    addView(_archiveBoundsMainForm2);

    setManual(False);
    _archiveBoundsMainForm2->addCallback(ArchiveBoundsForm::boundsChanged,
	this, (VkCallbackMethod) &ArchiveBounds::boundsChangedCB);

    _archiveBoundsMainForm2->addCallback(ArchiveBoundsForm::cancelActivate,
	this, (VkCallbackMethod) &ArchiveBounds::cancelActivateCB);
    
    _isCreated = True;
    //---- End editable code block:  constructor
}


ArchiveBounds::~ArchiveBounds()
{
    //---- Start editable code block: ArchiveBounds destructor


    //---- End editable code block: ArchiveBounds destructor

}

const char *ArchiveBounds::className()
{
    return ("ArchiveBounds");
}

/////////////////////////////////////////////////////////////// 
// The following functions are static member functions used to 
// interface with Motif.
/////////////////////////////////// 



//---- Start editable code block: End of generated code


void ArchiveBounds::showBounds(void)
{
    _archiveBoundsMainForm2->showBounds();
}

void ArchiveBounds::addBounds(struct timeval *st, struct timeval *ft, Boolean force)
{
    _archiveBoundsMainForm2->addBounds(st, ft, force);
}

void ArchiveBounds::setBounds(struct timeval *st, struct timeval *ft)
{
    _archiveBoundsMainForm2->setBounds(st, ft);
}

void ArchiveBounds::setManual(Boolean manual)
{
    _archiveBoundsMainForm2->setManual(manual);
}

Boolean ArchiveBounds::isManual(void)
{
    return _archiveBoundsMainForm2->isManual();
}

void
ArchiveBounds::getBounds(struct timeval *st, struct timeval *ft)
{
    _archiveBoundsMainForm2->getBounds(st, ft);
}

void
ArchiveBounds::getBounds(double *st, double *ft)
{
    _archiveBoundsMainForm2->getBounds(st, ft);
}

void
ArchiveBounds::showDetail(Boolean ms, Boolean year)
{
    _archiveBoundsMainForm2->showDetail(ms, year);
}

/*ARGSUSED*/
void
ArchiveBounds::boundsChangedCB(VkCallbackObject* obj, void *clientData, void *callData)
{
    // callback from the form. Pass it on to our callback
    callCallbacks(boundsChanged, (void *) callData);
}

/*ARGSUSED*/
void
ArchiveBounds::cancelActivateCB(VkCallbackObject* obj, void *clientData, void *callData)
{
    hide();
}

//---- End editable code block: End of generated code
