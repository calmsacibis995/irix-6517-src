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

#include <stdlib.h>
#include <Xm/PushB.h>
#include "RunForm.h"
#include "RunDialog.h"


RunDialog::RunDialog(const char *name,
	const char *host,
	const char *archive,
	double delta,
	const char *start,
	const char *finish,
	int numspecs,
	const char **specs)
	: VkGenericDialog(name)
{
    _host    = (host == NULL ? NULL : strdup(host));
    _archive = (archive == NULL ? NULL : strdup(archive));
    _delta   = delta;
    _start   = (start == NULL ? NULL : strdup(start));
    _finish  = (finish == NULL ? NULL : strdup(finish));
    _numspecs = numspecs;
    _specs = (char **)specs;
}

RunDialog::~RunDialog()
{
    if (_host != NULL)    free(_host);
    if (_archive != NULL) free(_archive);
    if (_start != NULL)   free(_start);
    if (_finish != NULL)  free(_finish);
}


Widget
RunDialog::createDialog(Widget parent)
{
    Widget	runDialogBaseWidget = VkGenericDialog::createDialog(parent);
    Arg		args[2];
    Cardinal	count;

    setTitle("Performance Co-Pilot Launch");
    _showCancel = _showApply = _showOK = True;

    _runForm = new RunForm("runForm", runDialogBaseWidget, _host, _archive,
	_delta, (const char *)_start, (const char *)_finish, _numspecs,
	(const char **)_specs);
    _runForm->setParent(this);

    count = 0;
    XtSetArg(args[count], XmNwidth, 395); count++;
    XtSetValues(_runForm->baseWidget(), args, count);
    Widget help = XtCreateManagedWidget("help",
			xmPushButtonWidgetClass, runDialogBaseWidget, args, 0);
    XtAddCallback(help, XmNactivateCallback, &RunDialog::helpCallback, (XtPointer)this);
    _runForm->show();
    return(runDialogBaseWidget);
}

const char *
RunDialog::className()
{
    return ("RunDialog");
}

void
RunDialog::apply(Widget w, XtPointer callData)
{
    _runForm->apply(w, callData);
}

void
RunDialog::cancel(Widget w, XtPointer callData) 
{
    _runForm->cancel(w, callData);
}

void
RunDialog::ok(Widget w, XtPointer callData)
{
    _runForm->ok(w, callData);
}

void
RunDialog::help(Widget w, XtPointer callData)
{
    _runForm->help(w, callData);
}

void
RunDialog::helpCallback(Widget w, XtPointer clientData, XtPointer callData)
{
    RunDialog* obj = (RunDialog *)clientData;
    obj->help(w, callData);
}

