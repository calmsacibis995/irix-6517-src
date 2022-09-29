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
// Header file for LiveTimeFormUI
//
// $Id: LiveTimeFormUI.h,v 1.3 1997/05/22 05:35:59 markgw Exp $
//
//////////////////////////////////////////////////////////////

#ifndef LIVETIMEFORMUI_H
#define LIVETIMEFORMUI_H

#include <Vk/VkComponent.h>

class VkOptionMenu;
class VkMenuAction;
class VkMenuToggle;
class VkMenuItem;

class LiveTimeFormUI : public VkComponent
{ 
  public:

    LiveTimeFormUI(const char *, Widget);
    LiveTimeFormUI(const char *);
    ~LiveTimeFormUI();
    void create(Widget);
    const char *className();


  protected:

    Widget  _liveTimeForm;

    Widget  _interval;
    Widget  _realtimeLabel;
    Widget  _timeLabel;
    Widget  _intervalLabel;
    Widget  _position;
    Widget  _vcrForward;
    Widget  _vcrStop;

    VkOptionMenu  *_unitsOption;

    VkMenuItem *_intervalUnitsDays;
    VkMenuItem *_intervalUnitsHours;
    VkMenuItem *_intervalUnitsMilliseconds;
    VkMenuItem *_intervalUnitsMinutes;
    VkMenuItem *_intervalUnitsSeconds;
    VkMenuItem *_intervalUnitsWeeks;


    // These virtual functions are called from the private callbacks (above)
    // Intended to be overriden in derived classes to define actions

    virtual void intervalActivate(Widget, XtPointer);
    virtual void intervalUnitsActivate(Widget, XtPointer);
    virtual void intervalValueChanged(Widget, XtPointer);
    virtual void positionActivate(Widget, XtPointer);
    virtual void positionValueChanged(Widget, XtPointer);
    virtual void vcrActivate(Widget, XtPointer);


  private: 
    // Array of default resources
    static String      _defaultLiveTimeFormUIResources[];

    // Callbacks to interface with Motif
    static void intervalActivateCallback(Widget, XtPointer, XtPointer);
    static void intervalUnitsActivateCallback(Widget, XtPointer, XtPointer);
    static void intervalValueChangedCallback(Widget, XtPointer, XtPointer);
    static void positionActivateCallback(Widget, XtPointer, XtPointer);
    static void positionValueChangedCallback(Widget, XtPointer, XtPointer);
    static void vcrActivateCallback(Widget, XtPointer, XtPointer);

};
#endif

