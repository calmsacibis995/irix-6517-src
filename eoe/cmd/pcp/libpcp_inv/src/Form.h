/* -*- C++ -*- */

#ifndef _INV_FORM_H
#define _INV_FORM_H

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

#ident "$Id: Form.h,v 1.1 1997/08/20 05:06:34 markgw Exp $"

#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Vk/VkWindow.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkSubMenu.h>
#include "FormUI.h"

class SoXtPrintDialog;

class INV_Form : public INV_FormUI
{
protected:
    
    VkWindow		*_parent;
    SoXtPrintDialog	*_printDialog;
    
public:
    
    INV_Form(const char *, Widget);
    INV_Form(const char *);
    ~INV_Form();

    const char *  className();

    virtual void setParent(VkWindow  *);
    virtual void vcrActivateCB(Widget, XtPointer );
    virtual void exitButtonCB(Widget, XtPointer);
    virtual void printButtonCB(Widget, XtPointer);
    virtual void saveButtonCB(Widget, XtPointer);
    virtual void showVCRButtonCB(Widget, XtPointer);
    virtual void newVCRButtonCB(Widget, XtPointer);
    virtual void recordButtonCB(Widget, XtPointer);
};

#endif /* _INV_FORM_H_ */
