/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *      helpWin; Display a text help file.
 *
 *      $Revision: 1.3 $
 *      $Date: 1996/02/26 01:28:12 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#include "helpWinLayout.h"
#include "helpWin.h"

#include <dialog.h>

#include <errno.h>
#include <signal.h>
#include <device.h>
#include <osfcn.h>

#include <bstring.h>
#include <stdio.h>
#include <string.h>

#include <sys/stat.h>

#include <tu/tuTopLevel.h>
#include <tu/tuXExec.h>
#include <tu/tuGadget.h>
#include <tu/tuCallBack.h>
#include <tu/tuLabelButton.h>
#include <tu/tuLabel.h>
#include <tu/tuCheckBox.h>
#include <tu/tuFrame.h>
#include <tu/tuRowColumn.h>
#include <tu/tuTextField.h>
#include <tu/tuUnPickle.h>


tuDeclareCallBackClass(HelpWinCallBack, HelpWin);
tuImplementCallBackClass(HelpWinCallBack, HelpWin);

HelpWin::HelpWin(const char* instanceName, tuTopLevel* othertoplevel,
			       tuBool transientForOtherTopLevel)
: tuTopLevel(instanceName, othertoplevel, transientForOtherTopLevel)
{
    setName(instanceName);
    TUREGISTERGADGETS;

    tuGadget* stuff = tuUnPickle::UnPickle(this, layoutstr);

    helpField = (tuList*) findGadget("helpField");
    ((tuLabelButton*)findGadget("pageup"))->setCallBack(new HelpWinCallBack(this, HelpWin::doPageUp));
    ((tuLabelButton*)findGadget("pagedown"))->setCallBack(new HelpWinCallBack(this, HelpWin::doPageDown));
    ((tuLabelButton*)findGadget("quit"))->setCallBack(new HelpWinCallBack(this, HelpWin::doQuit));

    // setup dialog box
    dialog = new DialogBox("HelpWin", (tuTopLevel*)this, False);

    HelpWinCallBack* closeCB = new HelpWinCallBack(this, HelpWin::doQuit);
    ((tuLabelButton*)findGadget("quit"))->setCallBack(closeCB);
    catchDeleteWindow(closeCB);
}


void HelpWin::getLayoutHints(tuLayoutHints* hints) {
    hints->flags = 0;
    hints->prefWidth = 600;
    hints->prefHeight = 490;
    setInitialOrigin(10, 250);
}


int HelpWin::setContent(const char* helpName)
{
    FILE *fp;
    char line[80];

    struct stat statbuf;
    if (stat(helpName, &statbuf) == 0)
    {
	// file exists
	helpField->deleteAllExternalChildren();
	fp = fopen(helpName, "r");
	while (fgets(line, 80, fp) != NULL)
	    new tuLabel(helpField, "helpList", line);
	fclose(fp);
	return 0;
    }
    else
    {
	// help file not exists
	dialog->warning(errno, "Could not open help file %s", helpName);
	dialog->map();
	return 1;
    }
} 


void HelpWin::doPageUp(tuGadget*)
{
    int totalLabel = helpField->getNumExternalChildren();
    if (totalLabel == 0)
	return;
    int listH = helpField->getHeight();
    tuGadget* child = helpField->getExternalChild(0);
    if (child)
    {
	int labelH = child->getHeight();
	int nLabel = listH/labelH;
	if (totalLabel > nLabel)
	{
	    for (int i = 0; i < totalLabel; i++)
	    {
		child = helpField->getExternalChild(i);
		if (helpField->isChildVisible(child))
		    break;
	    }
	    i -= nLabel; 
	    if (i < 0)
		i = 0;
	    tuGadget* firstChild = helpField->getExternalChild(i);
	    helpField->scrollChildToTop(firstChild);
	}
    }
}
	

void HelpWin::doPageDown(tuGadget*)
{
    int totalLabel = helpField->getNumExternalChildren();
    if (totalLabel == 0)
	return;
    int listH = helpField->getHeight();
    tuGadget* child = helpField->getExternalChild(0);
    if (child)
    {
	int labelH = child->getHeight();
	int nLabel = listH/labelH;
	if (totalLabel > nLabel)
	{
	    for (int i = 0; i < totalLabel; i++)
	    {
		child = helpField->getExternalChild(i);
		if (helpField->isChildVisible(child))
		    break;
	    }
	    i += nLabel;
	    if (i >= totalLabel)
		i = totalLabel - 1;
	    tuGadget* firstChild = helpField->getExternalChild(i);
	    helpField->scrollChildToTop(firstChild);
	}
    }
}


void HelpWin::doQuit(tuGadget*)
{
    unmap();
}
