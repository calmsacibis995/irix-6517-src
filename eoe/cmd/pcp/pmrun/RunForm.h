//
// Copyright 1997, Silicon Graphics, Inc.
// ALL RIGHTS RESERVED
//
// UNPUBLISHED -- Rights reserved under the copyright laws of the United
// States.   Use of a copyright notice is precautionary only and does not
// imply publication or disclosure.
//
// U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
// in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
// in similar or successor clauses in the FAR, or the DOD or NASA FAR
// Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
// 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
//
// THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
// INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
// DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
// PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
// GRAPHICS, INC.
//
//

#ifndef RUNFORM_H
#define RUNFORM_H

#include "RunFormUI.h"
#include <Vk/VkDialogManager.h>


class RunForm : public RunFormUI
{
  public:

    RunForm(const char	*name,
	    Widget	parent,
	    char	*host,
	    char	*archive,
	    double	delta,
	    const char	*start,
	    const char	*end,
	    int		numspecs,
	    const char	**specs);

    virtual ~RunForm();

    const char *className();

    virtual void setParent(VkDialogManager*);
    virtual void apply(Widget, XtPointer);
    virtual void cancel(Widget, XtPointer);
    virtual void ok(Widget, XtPointer);
    virtual void help(Widget, XtPointer);

  protected:

    class VkDialogManager *_parent;

    int launch();

    void intervalUnitsActivate(Widget, XtPointer);
    void intervalTextLosingFocus(Widget, XtPointer);
    void toggleValueChanged(Widget, XtPointer);

  private:

};
#endif

