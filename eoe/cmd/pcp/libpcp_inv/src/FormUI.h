/* -*- C++ -*- */

#ifndef _INV_FORMUI_H_
#define _INV_FORMUI_H_

/*
 * Copyright 1995, Silicon Graphics, Inc.
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
 * ININV_FormATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY INV_Form, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: FormUI.h,v 1.1 1997/08/20 05:07:01 markgw Exp $"

#include <Vk/VkComponent.h>

class SoXtExaminerViewer;

class INV_FormUI : public VkComponent
{ 

public:

    INV_FormUI(const char *, Widget);
    INV_FormUI(const char *);
    ~INV_FormUI();
    void create ( Widget );
    const char *  className();

    class SoXtExaminerViewer *_viewer;

    Widget  _form;
    Widget  _label;
    Widget  _scaleLabel;
    Widget  _scaleText;
    Widget  _scaleWheel;
    Widget  _timeLabel;
    Widget  _vcr;

    static float theMultiplier;

protected:

    virtual void vcrActivateCB ( Widget, XtPointer ) = 0;
    virtual void scaleTextActivateCB ( Widget, XtPointer );
    virtual void wheelChangedCB ( Widget, XtPointer );
    virtual void wheelDragCB ( Widget, XtPointer );

private: 

    static String      _defaultFormResources[];

    static void vcrActivateCBCallback ( Widget, XtPointer, XtPointer );
    static void scaleTextActivateCBCallback ( Widget, XtPointer, XtPointer );
    static void wheelChangedCBCallback ( Widget, XtPointer, XtPointer );
    static void wheelDragCBCallback ( Widget, XtPointer, XtPointer );

    static float calcScale(int value);
    
    void setScale(float value);
    float setScale(char *value);
    void showScale();
};

#endif /* _INV_FORMUI_H_ */
