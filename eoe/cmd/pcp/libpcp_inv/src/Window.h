/* -*- C++ -*- */

#ifndef _INV_WINDOW_H
#define _INV_WINDOW_H

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

#ident "$Id: Window.h,v 1.1 1997/08/20 05:13:05 markgw Exp $"

#include <Vk/VkWindow.h>
#include "Bool.h"
#include "LaunchMenu.h"

class VkMenuItem;
class VkMenuToggle;
class VkMenuConfirmFirstAction;
class VkSubMenu;
class VkRadioSubMenu;

class INV_Window;
class INV_Form;

class INV_Window: public VkWindow
{     
private:

    static String	_defaultWindowResources[];

protected:

    INV_Form		*_form;

    VkSubMenu		*_fileMenu;
    VkMenuItem		*_recordButton;
    VkMenuItem		*_saveButton;
    VkMenuItem		*_printButton;
    VkMenuItem		*_exitButton;
    VkSubMenu		*_optionsMenu;
    VkMenuItem		*_showVCRButton;
    VkMenuItem		*_newVCRButton;
    VkMenuItem		*_separator;
    VkSubMenu		*_launchMenu;    
    INV_LaunchMenu	*_launchItems;

public:
    
    virtual ~INV_Window();

    INV_Window(const char * name, 
	       ArgList args = NULL,
	       Cardinal argCount = 0 );

    const char *className()
	{ return "INV_Window"; }
    virtual Boolean okToQuit();
    
    VkSubMenu *optionsMenu()
	{ return _optionsMenu; }

protected:
    
    virtual void exitButtonCB ( Widget, XtPointer );
    virtual void printButtonCB ( Widget, XtPointer );
    virtual void saveButtonCB ( Widget, XtPointer );
    virtual void recordButtonCB ( Widget, XtPointer );
    virtual void showVCRButtonCB ( Widget, XtPointer );
    virtual void newVCRButtonCB ( Widget, XtPointer );
    
    void switchLaunchMenu(uint_t i);

    void hideRecordButton();
    void recordState(OMC_Bool state);

private:
    
    static void exitButtonCBCallback ( Widget, XtPointer, XtPointer );
    static void printButtonCBCallback ( Widget, XtPointer, XtPointer );
    static void saveButtonCBCallback ( Widget, XtPointer, XtPointer );
    static void recordButtonCBCallback ( Widget, XtPointer, XtPointer );
    static void showVCRButtonCBCallback ( Widget, XtPointer, XtPointer );
    static void newVCRButtonCBCallback ( Widget, XtPointer, XtPointer );
};

#endif /* _INV_WINDOW_H_ */
