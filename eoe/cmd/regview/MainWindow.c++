/*
 * MainWindow.c++
 *
 *	definition of MainWindow class for regview
 *
 * Copyright 1994, Silicon Graphics, Inc.
 * All Rights Reserved.
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

#ident "$Revision: 1.6 $"

#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/RowColumn.h>
#include <Xm/PushB.h>

#include <Vk/VkApp.h>
#include <Vk/VkInfoDialog.h>

#include <stdio.h>
#include <stdlib.h>

#include "util.h"
#include "MainWindow.h"

/*
 *  MainWindow::MainWindow(const char *name) : VkSimpleWindow(name)
 *
 *  Description:
 *      MainWindow constructor.  Create all our widgets and futz
 *      around with their attachments.
 *
 *  Parameters:
 *      name  ViewKit name of this component
 */

MainWindow::MainWindow(const char *name) : VkSimpleWindow(name)
{
    lastSize = (unsigned long)-1;
    theApplication->setMainWindow(this);

    form = XmCreateForm(mainWindowWidget(), "form", NULL, 0);

    createLabels();
    createRegionDisplay();
    createLegend();
    createSymbolDisplay();
    createButtons();

    int n;
    Arg wargs[10];

    n = 0;
    XtSetArg(wargs[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(wargs[n], XmNtopWidget, labelForm); n++;
    XtSetArg(wargs[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(wargs[n], XmNbottomWidget, buttonRC); n++;
    XtSetValues(regDisp->baseWidget(), wargs, n);

    n = 0;
    XtSetArg(wargs[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(wargs[n], XmNtopWidget, labelForm); n++;
    XtSetArg(wargs[n], XmNbottomAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(wargs[n], XmNbottomWidget, buttonRC); n++;
    XtSetArg(wargs[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(wargs[n], XmNleftWidget, regDisp->baseWidget()); n++;
    XtSetValues(symTab->baseWidget(), wargs, n);

    n = 0;
    XtSetArg(wargs[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
    XtSetArg(wargs[n], XmNleftWidget, legend->baseWidget()); n++;
    XtSetValues(labelForm, wargs, n);

    addView(form);
}

/*
 * MainWindow destructor
 */
MainWindow::~MainWindow()
{
}

/*
 *  void MainWindow::afterRealizeHook()
 *
 *  Description:
 *      Postponed initialization.  Create the RegionInfo object, which
 *      is the guy that handles input from bloatview and polling the
 *      region that we're monitoring.
 */

void MainWindow::afterRealizeHook()
{
    regInfo = new RegionInfo;
    regInfo->addCallback(RegionInfo::newRegionCB, this,
			 (VkCallbackMethod)&MainWindow::newRegion);
    regInfo->addCallback(RegionInfo::updateCB, this,
			 (VkCallbackMethod)&MainWindow::update);
}

/*
 * ViewKit linkage
 */
const char *MainWindow::className()
{
    return "MainWindow";
}

/*
 *  Widget MainWindow::createLabel(Widget parent, char *name,
 *  			           Widget leftWidget, Widget topWidget)
 *
 *  Description:
 *      Create a label for use in the info area of Region Viewer.
 *      This convenience function takes care of the top and left
 *      Widget attachments if non-NULL.
 *
 *  Parameters:
 *      parent      parent of label
 *      name        name of label
 *      leftWidget  left attachment
 *      topWidget   top attachment
 *
 *  Returns:
 *	The newly created label widget
 */

Widget MainWindow::createLabel(Widget parent, char *name,
			       Widget leftWidget, Widget topWidget)
{
    int n;
    Arg wargs[10];

    n = 0;
    if (leftWidget) {
	XtSetArg(wargs[n], XmNleftAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(wargs[n], XmNleftWidget, leftWidget); n++;
    }
    if (topWidget) {
	XtSetArg(wargs[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(wargs[n], XmNtopWidget, topWidget); n++;
    }
    
    Widget label = XmCreateLabel(parent, name, wargs, n);
    XtManageChild(label);
    return label;
}

/*
 *  void MainWindow::createLabels()
 *
 *  Description:
 *      Create all the labels that display information to the user
 *      about the region being monitored.
 */

void MainWindow::createLabels()
{
    labelForm = XmCreateForm(form, "labelForm", NULL, 0);
    XtManageChild(labelForm);

    Widget procTitle = createLabel(labelForm, "procTitle", NULL, NULL);
    procLabel = createLabel(labelForm, "procLabel", procTitle, NULL);

    Widget objTitle = createLabel(labelForm, "objTitle", NULL, procTitle);
    objLabel = createLabel(labelForm, "objLabel", objTitle, procLabel);
    
    Widget typeTitle = createLabel(labelForm, "typeTitle", NULL, objTitle);
    typeLabel = createLabel(labelForm, "typeLabel", typeTitle, objLabel);

    Widget permTitle = createLabel(labelForm, "permTitle", NULL, typeTitle);
    permLabel = createLabel(labelForm, "permLabel", permTitle, typeLabel);
    
    Widget addrTitle = createLabel(labelForm, "addrTitle", NULL, permTitle);
    addrLabel = createLabel(labelForm, "addrLabel", addrTitle, permLabel);
    
    Widget sizeTitle = createLabel(labelForm, "sizeTitle", NULL, addrTitle);
    sizeLabel = createLabel(labelForm, "sizeLabel", sizeTitle, addrLabel);
}

/*
 *  void MainWindow::createRegionDisplay()
 *
 *  Description:
 *      Create the component that shows a bar for each page in the
 *      region.
 */

void MainWindow::createRegionDisplay()
{
    regDisp = new RegionDisplay("regionDisplay", form);
    regDisp->addCallback(RegionDisplay::pageSelectCB, this,
			 (VkCallbackMethod)&MainWindow::pageSelect);
    regDisp->show();
}

/*
 *  void MainWindow::createSymbolDisplay()
 *
 *  Description:
 *      Create the scrolled text widget that displays symbols when the
 *      user clicks on a page.
 */

void MainWindow::createSymbolDisplay()
{
    symTab = new SymTab("symTab", form);
    symTab->show();
}

/*
 *  void MainWindow::createButtons()
 *
 *  Description:
 *      Create the row of buttons along the bottom of region viewer.
 */

void MainWindow::createButtons()
{
    buttonRC = XmCreateRowColumn(form, "buttonRC", NULL, 0);
    XtManageChild(buttonRC);

    Widget zoomIn = XmCreatePushButton(buttonRC, "zoomIn", NULL, 0);
    XtManageChild(zoomIn);
    XtAddCallback(zoomIn, XmNactivateCallback, zoomInCB, this);

    Widget zoomOut = XmCreatePushButton(buttonRC, "zoomOut", NULL, 0);
    XtManageChild(zoomOut);
    XtAddCallback(zoomOut, XmNactivateCallback, zoomOutCB, this);

    Widget zap = XmCreatePushButton(buttonRC, "zap", NULL, 0);
    XtManageChild(zap);
    XtAddCallback(zap, XmNactivateCallback, zapCB, this);

    Widget detach = XmCreatePushButton(buttonRC, "detach", NULL, 0);
    XtManageChild(detach);
    XtAddCallback(detach, XmNactivateCallback, detachCB, this);

    Widget quit = XmCreatePushButton(buttonRC, "quit", NULL, 0);
    XtManageChild(quit);
    XtAddCallback(quit, XmNactivateCallback, quitCB, this);

    Widget help = XmCreatePushButton(buttonRC, "help", NULL, 0);
    XtManageChild(help);
    XtAddCallback(help, XmNactivateCallback, helpCB, this);
}

/*
 *  void MainWindow::createLegend()
 *
 *  Description:
 *      Create the legend component that displays the key for what the
 *      colors mean in the RegionDisplay component.
 */

void MainWindow::createLegend()
{
    legend = new Legend("legend", form);
    legend->show();
}

/*
 *  static char * MapFlags(prmap_sgi_t *map)
 *
 *  Description:
 *      Translate the flags for a region into a meaningful word that
 *      describes what type of region it is
 *
 *  Parameters:
 *      map  region pointer
 *
 *  Returns:
 *      string describing the type of region
 */

static char * MapFlags(prmap_sgi_t *map)
{
    if (map->pr_mflags & MA_PHYS) {
	return "Physical Device";
    } else if (map->pr_mflags & MA_STACK) {
	return "Stack";
    } else if (map->pr_mflags & MA_BREAK) {
	return "Break";
    } else if (map->pr_mflags & MA_EXEC) {
	return "Text";
    } else if (map->pr_mflags & MA_COW) {
	return "Data";
    } else if (map->pr_mflags & MA_SHMEM) {
	return "Shmem";
    } else if ((map->pr_mflags & (MA_READ | MA_WRITE)) ==
	       (MA_READ | MA_WRITE)) {
	return "RW";
    } else if (map->pr_mflags & MA_READ) {
	return "RO";
    }
    return "Other";
}

/*
 *  void MainWindow::newRegion(RegionInfo *, void *, void *)
 *
 *  Description:
 *      Callback function for the newRegionCB callback of the
 *      RegionInfo object.  The RegionInfo object calls this callback
 *      list when bloatview tells it about a new region to monitor.
 *      
 *      Our duty is to update our info labels and inform the
 *      RegionDisplay object and the SymTab object of the new region.
 *      The RegionDisplay object doesn't actually get new info until
 *      the next update (see below); its newRegion function really
 *      just clears out the old junk.
 */

void MainWindow::newRegion(RegionInfo *, void *, void *)
{
    regDisp->newRegion();
    theApplication->busy("readingInfo");
    prpsinfo_t *psinfo = regInfo->getPSInfo();
    prmap_sgi_t *map = regInfo->getMap();

    SetLabel(procLabel, psinfo->pr_fname);
    char *objName = regInfo->getObjName();
    SetLabel(objLabel, objName[0] ? objName : " ");
    SetLabel(typeLabel, MapFlags(map));
    char permstr[4];
    strcpy(permstr, "---");
    if (map->pr_mflags & MA_READ) {
	permstr[0] = 'r';
    }
    if (map->pr_mflags & MA_WRITE) {
	permstr[1] = 'w';
    }
    if (map->pr_mflags & MA_EXEC) {
	permstr[2] = 'x';
    }
    SetLabel(permLabel, permstr);

    char buf[20];
    (void)sprintf(buf, "%08x", map->pr_vaddr);
    SetLabel(addrLabel, buf);

    symTab->setObject(regInfo->getObjName(), map, regInfo->getOffset());

    theApplication->notBusy();
}


/*
 *  void MainWindow::update(RegionInfo *, void *, void *)
 *
 *  Description:
 *      Callback function for the updateCB callback list of the
 *      RegionInfo object.  This gets called whenever the RegionInfo
 *      object polls the region being monitored.  All we do is notify
 *      the RegionDisplay object.
 */

void MainWindow::update(RegionInfo *, void *, void *)
{
    prmap_sgi_t *map = regInfo->getMap();
    char buf[20];
    if (map->pr_size != lastSize) {
	lastSize = map->pr_size;
	(void)sprintf(buf, "%d K", map->pr_size / 1024);
	SetLabel(sizeLabel, buf);
    }

    int pageSize = regInfo->getPageSize() / 1024;
    unsigned long invalidSize = 0, residentSize = 0, pagedOutSize = 0;
    unsigned long zappedSize = 0;
    prpgd_sgi_t *pageData = regInfo->getPageData();

    for (int i = 0; i < pageData->pr_pglen; i++) {
	switch (regInfo->getPageState(pageData->pr_data + i)) {
	  case RegionInfo::kResident:
	    residentSize += pageSize;
	    break;
	  case RegionInfo::kZapped:
	    zappedSize += pageSize;
	    break;
	  case RegionInfo::kPagedOut:
	    pagedOutSize += pageSize;
	    break;
	  case RegionInfo::kInvalid:
	  default:
	    invalidSize += pageSize;
	    break;
	}
    }

    legend->setLabels(invalidSize, residentSize, pagedOutSize, zappedSize);

    regDisp->update(regInfo);
}

/*
 *  void MainWindow::pageSelect(SymTab *, void *, void *vaddr)
 *
 *  Description:
 *      Callback function for the pageSelectCB list of the
 *      RegionDisplay object.  The RegionDisplay object calls this
 *      list when the user clicks to select a page.  We tell the
 *      SymTab object to display the symbols for this page.
 *
 *  Parameters:
 *      vaddr  virtual address of the page that was selected.
 */

void MainWindow::pageSelect(SymTab *, void *, void *vaddr)
{
    prmap_sgi_t *map = regInfo->getMap();
    symTab->setPage(vaddr);
}

/*
 * Xt callback function for the "Zoom In" button
 */
void MainWindow::zoomInCB(Widget, XtPointer client, XtPointer)
{
    ((MainWindow *)client)->regDisp->zoomIn();
}

/*
 * Xt callback function for the "Zoom Out" button
 */
void MainWindow::zoomOutCB(Widget, XtPointer client, XtPointer)
{
    ((MainWindow *)client)->regDisp->zoomOut();
}

/*
 * Xt callback function for the "Detach" button
 */
void MainWindow::detachCB(Widget, XtPointer client, XtPointer)
{
    ((MainWindow *)client)->regInfo->detach();
}

/*
 * Xt callback function for the "Zap" button
 */
void MainWindow::zapCB(Widget, XtPointer client, XtPointer)
{
    ((MainWindow *)client)->regInfo->zap();
}

/*
 * Xt callback function for the "Quit" button
 */
void MainWindow::quitCB(Widget, XtPointer, XtPointer)
{
    exit(0);
}

/*
 * Xt callback function for the "Help" button
 */
void MainWindow::helpCB(Widget, XtPointer, XtPointer)
{
    theInfoDialog->post("helpMsg");
}
