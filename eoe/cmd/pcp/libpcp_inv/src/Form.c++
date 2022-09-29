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
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: Form.c++,v 1.5 1999/04/30 01:44:04 kenmcd Exp $"

#include <sys/stat.h>
#include <unistd.h>
#include <iostream.h>
#include <Xm/Form.h> 
#include <Xm/Label.h>
 
#include <Vk/VkApp.h>
#include <Vk/VkResource.h>
#include <Vk/VkWindow.h>
#include <Vk/VkMenuBar.h>
#include <Vk/VkSubMenu.h>
#include <Vk/VkErrorDialog.h>
#include <Vk/VkWarningDialog.h>
#include <Vk/VkFileSelectionDialog.h>

#include <Inventor/Xt/viewers/SoXtExaminerViewer.h>
#include <Inventor/SoOutput.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/Xt/SoXtPrintDialog.h>
#include <Inventor/nodes/SoSeparator.h>

#include "Inv.h"
#include "App.h"
#include "Form.h"
#include "View.h"

INV_Form::INV_Form(const char *name, Widget parent)
: INV_FormUI(name, parent),
  _parent(0),
  _printDialog(0)
{ 
}

INV_Form::INV_Form(const char *name) 
: INV_FormUI(name),
  _parent(0),
  _printDialog(0)
{ 
}

INV_Form::~INV_Form()
{
}

const char * 
INV_Form::className()
{
    return ("INV_Form");
}

void 
INV_Form::setParent( VkWindow  * parent )
{
    _parent = parent;
}

void 
INV_Form::exitButtonCB( Widget, XtPointer)
{
    theApp->terminate(0);
}

void 
INV_Form::saveButtonCB( Widget, XtPointer)
{
    theFileSelectionDialog->setTitle("Save Scene as Inventor");
    theFileSelectionDialog->setFilterPattern("*.iv");

    if (theFileSelectionDialog->postAndWait() == VkDialogManager::OK) {
	struct stat buf;
	if (stat(theFileSelectionDialog->fileName(), &buf) == 0) {
	    sprintf(theBuffer, "%s exists, overwrite?", 
		    theFileSelectionDialog->fileName());
	    int sts = theWarningDialog->postAndWait(theBuffer, TRUE, TRUE);
	    if (sts == VkDialogManager::CANCEL)
		return;
	}
	SoOutput outFile;
	SbBool success = outFile.openFile(theFileSelectionDialog->fileName());
	if (success == FALSE) {
	    sprintf(theBuffer, "Save as %s: %s", 
		    theFileSelectionDialog->fileName(),
		    strerror(errno));
	    theErrorDialog->postAndWait(theBuffer);
	}
	else {
	    SoWriteAction write(&outFile);
	    write.apply(theView->root());
	    outFile.closeFile();
	}
    }
}

void 
INV_Form::printButtonCB( Widget, XtPointer)
{
     if (_printDialog == NULL) {
	_printDialog = new SoXtPrintDialog(_form, NULL, FALSE);

	if (_printDialog == NULL) {
	    cerr << pmProgname << ": Warning: Out of memory" << endl;
	    theErrorDialog->postAndWait("Unable to print: Out of Memory");
	    return;
	}

	_printDialog->setSceneGraph(theView->root());
	_printDialog->setGLRenderAction(_viewer->getGLRenderAction());
	_printDialog->setTitle("Print Scene");
    }
    
    if (!_printDialog->isVisible())
	_printDialog->show();   
}

void 
INV_Form::showVCRButtonCB( Widget, XtPointer)
{
    int sts;

    if ((sts = pmTimeShowDialog(1)) < 0) {
	sprintf(theBuffer, "Could not show VCR: %s", pmErrStr(sts));
	theErrorDialog->postAndWait(theBuffer);
    }
}

void 
INV_Form::newVCRButtonCB( Widget, XtPointer)
{
    int		sts = 0;

    sts = theView->timeConnect(OMC_true);

    if (sts < 0) {
	sprintf(theBuffer, "Unable to connect new time controls: %s",
		pmErrStr(sts));
	theErrorDialog->postAndWait(theBuffer);
	theApp->terminate(1);
    }
}

void 
INV_Form::vcrActivateCB ( Widget, XtPointer)
{
    int sts;

    if ((sts = pmTimeShowDialog(1)) < 0) {
	sprintf(theBuffer, "Could not show VCR: %s", pmErrStr(sts));
	theErrorDialog->postAndWait(theBuffer);
    }    
}

extern "C" VkComponent * CreateForm( const char *name, Widget parent ) 
{  
    VkComponent *obj =  new INV_Form ( name, parent );
    obj->show();

    return ( obj );
}

void 
INV_Form::recordButtonCB( Widget, XtPointer)
{
    theView->record();
}
