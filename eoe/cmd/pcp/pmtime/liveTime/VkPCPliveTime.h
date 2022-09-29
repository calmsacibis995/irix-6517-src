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
// $Id: VkPCPliveTime.h,v 1.4 1997/08/21 04:34:46 markgw Exp $
//

//////////////////////////////////////////////////////////////
//
// Header file for VkPCPliveTime
//
//    This class is a ViewKit VkWindow subclass
//
// Normally, very little in this file should need to be changed.
// Create/add/modify menus using the builder.
//
// Try to restrict any changes to adding members below the
// "//---- End generated code section" markers
// Doing so will allow you to make chnages using the builder
// without losing any changes you may have made manually
//
//////////////////////////////////////////////////////////////
#ifndef VKPCPLIVETIME_H
#define VKPCPLIVETIME_H
#include <Vk/VkWindow.h>

class VkMenuItem;
class VkMenuToggle;
class VkMenuConfirmFirstAction;
class VkSubMenu;
class VkRadioSubMenu;

//---- End generated headers

#include "pmapi.h"

//---- VkPCPliveTime class declaration

class VkPCPliveTime: public VkWindow { 

  public:

    VkPCPliveTime( const char * name, 
                   ArgList args = NULL,
                   Cardinal argCount = 0 );
    ~VkPCPliveTime();
    const char *className();

    //--- End generated code section
    int initialize(int, int, pmTime *);
    int getDetailSetting(void);
    void showDialog(int);
    void addTimezone(char *, char *, Boolean setItem = False);
    inline VkSubMenu *getTimezoneRadioPane(void) {return _timezoneRadioPane;}
    void start(void);
    void stop(void);

    inline virtual void handleWmDeleteMessage() {hide();};
    inline virtual void handleWmQuitMessage() {hide();};

  protected:


    // Classes created by this class

    class LiveTimeForm *_liveTimeForm;

    // Menu items created by this class
    VkSubMenu  *_filePane2;
    VkMenuItem *_separator4;
    VkMenuItem *_hideButton;
    VkSubMenu  *_optionsPane2;
    VkSubMenu  *_timezoneRadioPane;
    VkSubMenu  *_menuPane;
    VkMenuToggle *_detailedPositions;
    VkMenuToggle *_showYear;

    // Menu Operations

    virtual void detailedPositionsValueChanged ( Widget, XtPointer );
    virtual void hideButtonActivate ( Widget, XtPointer );
    virtual void showYearValueChanged ( Widget, XtPointer );
    virtual void timezoneValueChanged ( Widget, XtPointer );

    //--- End generated code section

    void setUpWindowProperties();

  private:


    // Callbacks to interface with Motif

    static void detailedPositionsValueChangedCallback ( Widget, XtPointer, XtPointer );
    static void hideButtonActivateCallback ( Widget, XtPointer, XtPointer );
    static void showYearValueChangedCallback ( Widget, XtPointer, XtPointer );
    static void timezoneValueChangedCallback ( Widget, XtPointer, XtPointer );

    static String      _defaultVkPCPliveTimeResources[];

    //--- End generated code section

};
#endif
