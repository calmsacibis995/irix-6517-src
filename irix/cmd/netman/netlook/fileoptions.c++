/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	File Options
 *
 *	$Revision: 1.3 $
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

#include <stdio.h>
#include <tuCallBack.h>
#include <tuFilePrompter.h>
#include <tuGLGadget.h>
#include "fileoptions.h"
#include "netlook.h"
#include "netlookgadget.h"
#include "glmapgadget.h"

tuDeclareCallBackClass(FileOptionsCallBack, FileOptions);
tuImplementCallBackClass(FileOptionsCallBack, FileOptions);

extern NetLookGadget *netlookgadget;
extern GLMapGadget *glmapgadget;
extern char *exc_message;


FileOptions::FileOptions(NetLook *nl)
{
    // Set up callbacks
    (new FileOptionsCallBack(this, FileOptions::open))->
					registerName("File_open");
    (new FileOptionsCallBack(this, FileOptions::save))->
					registerName("File_save");
    (new FileOptionsCallBack(this, FileOptions::saveData))->
					registerName("Save_data");
    (new FileOptionsCallBack(this, FileOptions::saveUI))->
					registerName("Save_ui");
    (new FileOptionsCallBack(this, FileOptions::saveDataAs))->
					registerName("Save_dataas");
    (new FileOptionsCallBack(this, FileOptions::saveUIAs))->
					registerName("Save_uias");
    (new FileOptionsCallBack(this, FileOptions::quit))->
					registerName("File_quit");

    // Save netlook pointer
    netlook = nl;

    // File prompter
    prompter = 0;
}

void
FileOptions::open(tuGadget *)
{
    if (prompter == 0) {
	prompter = new tuFilePrompter("prompter", netlook->getTopLevel(), True);
	prompter->bind();
    } else if (prompter->isMapped())
	prompter->unmap();
    prompter->readDirectory(0);
    prompter->resizeToFit();
    prompter->setName("Open");
    prompter->setCallBack(
		new FileOptionsCallBack(this, FileOptions::openPrompt));
    prompter->mapWithCancelUnderMouse();
}

void
FileOptions::openPrompt(tuGadget *)
{
    char *file = prompter->getSelectedPath();
    prompter->unmap();
    if (file == 0)
	return;

    if (netlook->openDataFile(file) == 0)
	netlook->openDialogBox();
    delete [] file;

    // Set viewport to universe
    netlook->setViewPort();

    // Render
    netlookgadget->render();
    glmapgadget->render();
}

void
FileOptions::save(tuGadget *)
{
    if (netlook->saveDataFile() == 0) {
	netlook->openDialogBox();
	return;
    }
    if (netlook->saveUIFile() == 0) {
	netlook->openDialogBox();
	return;
    }
}

void
FileOptions::saveData(tuGadget *)
{
    if (netlook->saveDataFile() == 0) {
	netlook->openDialogBox();
	return;
    }
}

void
FileOptions::saveUI(tuGadget *)
{
    if (netlook->saveUIFile() == 0) {
	netlook->openDialogBox();
	return;
    }
}

void
FileOptions::saveDataAs(tuGadget *)
{
    if (prompter == 0) {
	prompter = new tuFilePrompter("prompter", netlook->getTopLevel(), True);
	prompter->bind();
    }
    if (prompter->isMapped())
	prompter->unmap();
    char *home = getenv("HOME");
    if (home == 0)
	home = "/usr/tmp";
    prompter->readDirectory(home);
    prompter->resizeToFit();
    prompter->setName("Save Networks");
    prompter->setCallBack(
		new FileOptionsCallBack(this, FileOptions::saveDataPrompt));
    prompter->mapWithCancelUnderMouse();
}

void
FileOptions::saveDataPrompt(tuGadget *)
{
    char *file = prompter->getSelectedPath();
    prompter->unmap();
    if (file == 0)
	return;

    netlook->saveDataFile(file);
}

void
FileOptions::saveUIAs(tuGadget *)
{
    if (prompter == 0) {
	prompter = new tuFilePrompter("prompter", netlook->getTopLevel(), True);
	prompter->bind();
    }
    if (prompter->isMapped())
	prompter->unmap();
    char *home = getenv("HOME");
    if (home == 0)
	home = "/usr/tmp";
    prompter->readDirectory(home);
    prompter->resizeToFit();
    prompter->setName("Save Controls");
    prompter->setCallBack(
		new FileOptionsCallBack(this, FileOptions::saveUIPrompt));
    prompter->mapWithCancelUnderMouse();
}

void
FileOptions::saveUIPrompt(tuGadget *)
{
    char *file = prompter->getSelectedPath();
    prompter->unmap();
    if (file == 0)
	return;

    netlook->saveUIFile(file);
}

void
FileOptions::quit(tuGadget *)
{
    netlook->quit();
}
