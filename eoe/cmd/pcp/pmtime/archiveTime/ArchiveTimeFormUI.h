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
// Header file for ArchiveTimeFormUI
//
// $Id: ArchiveTimeFormUI.h,v 1.3 1997/05/22 05:34:03 markgw Exp $
//
//////////////////////////////////////////////////////////////

#ifndef ARCHIVETIMEFORMUI_H
#define ARCHIVETIMEFORMUI_H

#include <Vk/VkComponent.h>

class VkOptionMenu;
class VkMenuAction;
class VkMenuToggle;
class VkMenuItem;

class ArchiveTimeFormUI : public VkComponent
{ 

  public:

    ArchiveTimeFormUI(const char *, Widget);
    ArchiveTimeFormUI(const char *);
    ~ArchiveTimeFormUI();
    void create(Widget);
    const char *className();


  protected:

    Widget  _archiveTimeForm;

    Widget  _interval;
    Widget  _intervalLabel;
    Widget  _speedLabel;
    Widget  _positionLabel;
    Widget  _vcrLabel;
    Widget  _position;
    Widget  _positionScale;
    Widget  _speed;
    Widget  _speedThumb;
    Widget  _vcrForward;
    Widget  _vcrReverse;
    Widget  _vcrStop;

    VkOptionMenu  *_unitsOption;
    VkOptionMenu  *_vcrOption;

    VkMenuItem *_intervalUnitsDays;
    VkMenuItem *_intervalUnitsHours;
    VkMenuItem *_intervalUnitsMilliseconds;
    VkMenuItem *_intervalUnitsMinutes;
    VkMenuItem *_intervalUnitsSeconds;
    VkMenuItem *_intervalUnitsWeeks;
    VkMenuItem *_vcrOptionFast;
    VkMenuItem *_vcrOptionPlay;
    VkMenuItem *_vcrOptionStep;

    // These virtual functions are called from the private callbacks (above)
    // Intended to be overriden in derived classes to define actions
    virtual void intervalActivate(Widget, XtPointer);
    virtual void intervalUnitsActivate(Widget, XtPointer);
    virtual void intervalValueChanged(Widget, XtPointer);
    virtual void positionActivate(Widget, XtPointer);
    virtual void positionScaleDrag(Widget, XtPointer);
    virtual void positionScaleValueChanged(Widget, XtPointer);
    virtual void positionValueChanged(Widget, XtPointer);
    virtual void speedActivate(Widget, XtPointer);
    virtual void speedThumbDrag(Widget, XtPointer);
    virtual void speedThumbValueChanged(Widget, XtPointer);
    virtual void speedValueChanged(Widget, XtPointer);
    virtual void vcrActivate(Widget, XtPointer);
    virtual void vcrOptionActivate(Widget, XtPointer);


  private: 
    // Array of default resources
    static String      _defaultArchiveTimeFormUIResources[];

    // Callbacks to interface with Motif
    static void intervalActivateCallback(Widget, XtPointer, XtPointer);
    static void intervalUnitsActivateCallback(Widget, XtPointer, XtPointer);
    static void intervalValueChangedCallback(Widget, XtPointer, XtPointer);
    static void positionActivateCallback(Widget, XtPointer, XtPointer);
    static void positionScaleDragCallback(Widget, XtPointer, XtPointer);
    static void positionScaleValueChangedCallback(Widget, XtPointer, XtPointer);
    static void positionValueChangedCallback(Widget, XtPointer, XtPointer);
    static void speedActivateCallback(Widget, XtPointer, XtPointer);
    static void speedThumbDragCallback(Widget, XtPointer, XtPointer);
    static void speedThumbValueChangedCallback(Widget, XtPointer, XtPointer);
    static void speedValueChangedCallback(Widget, XtPointer, XtPointer);
    static void vcrActivateCallback(Widget, XtPointer, XtPointer);
    static void vcrOptionActivateCallback(Widget, XtPointer, XtPointer);

};
#endif

