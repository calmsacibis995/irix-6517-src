/*
 * MainWindow.h
 *
 *	declaration of MainWindow class for regview
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

#ident "$Revision: 1.4 $"

#include <Vk/VkSimpleWindow.h>
#include "RegionDisplay.h"
#include "SymTab.h"

/*
 * MainWindow class implements the main window of Region Viewer.
 */
class MainWindow : public VkSimpleWindow {
public:
    MainWindow(const char *name);
    ~MainWindow();
    virtual const char *className();
private:
    void afterRealizeHook();
    Widget createLabel(Widget parent, char *name,
		       Widget leftWidget, Widget topWidget);
    void createLabels();
    void createRegionDisplay();
    void createSymbolDisplay();
    void createButtons();
    void createLegend();

    void newRegion(RegionInfo *, void *, void *);
    void update(RegionInfo *, void *, void *);
    void pageSelect(SymTab *, void *, void *);

    static void zoomInCB(Widget, XtPointer client, XtPointer);
    static void zoomOutCB(Widget, XtPointer client, XtPointer);
    static void detachCB(Widget, XtPointer client, XtPointer);
    static void zapCB(Widget, XtPointer client, XtPointer);
    static void quitCB(Widget, XtPointer client, XtPointer);
    static void helpCB(Widget, XtPointer client, XtPointer);

    Widget form, labelForm, procLabel, objLabel, typeLabel, permLabel;
    Widget addrLabel, sizeLabel;
    Widget buttonRC;

    RegionDisplay *regDisp;
    Legend *legend;
    RegionInfo *regInfo;
    SymTab *symTab;
    unsigned long lastSize;
};

