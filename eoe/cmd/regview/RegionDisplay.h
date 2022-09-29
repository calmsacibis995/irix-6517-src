/*
 * RegionDisplay.h
 *
 *	declaration of RegionDisplay class for regview
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

#include <Xm/ScrollBar.h>

#include <Vk/VkComponent.h>

#include "RegionInfo.h"

/*
 * The Legend component is closely associated with the RegionDisplay
 * component (which is why it's in here).  The Legend component
 * displays color swatches and labels to let the user know about the
 * color coding in the RegionDisplay component.
 */
class Legend : public VkComponent {
public:
    Legend(const char *name, Widget parent);
    ~Legend();
    void setLabels(unsigned long invalidSize,
		   unsigned long residentSize,
		   unsigned long pagedOutSize,
		   unsigned long zappedSize);
private:
    static XtResource resources[];
    Widget invalid, resident, pagedOut, zapped;
    Widget invalidLabel, residentLabel, pagedOutLabel, zappedLabel;
    char *invalidString, *residentString, *pagedOutString;
    char *zappedString;
    unsigned long lastInvalidSize, lastResidentSize, lastPagedOutSize;
    unsigned long lastZappedSize;
    GC gc;

    Widget createDraw(Widget parent, char *name,
		      unsigned long background, Widget topWidget);
    Widget createLabel(Widget parent, char *name, Widget leftWidget,
		       Widget topWidget);

    void drawLegend(Widget draw);
    static void exposeCB(Widget w, XtPointer client, XtPointer cb);
};

class DispInfo;

/*
 * The RegionDisplay component displays a bar for each page in a
 * region, and colors each bar differently depending on whether the
 * corresponding page is invalid, resident, paged out, or zapped.
 * You can zoom in and zoom out as well as scroll.
 */
class RegionDisplay : public VkComponent {
    friend class Legend;
public:
    static const char pageSelectCB[];

    RegionDisplay(const char *name, Widget parent);
    ~RegionDisplay();
    void newRegion();
    void update(RegionInfo *info);
    void zoomIn();
    void zoomOut();
private:
    void afterRealizeHook();
    void reset();
    void draw();
    void handleInput(XEvent *event);
    void resize();
    void scroll(XmScrollBarCallbackStruct *cb);
    void setScrollBar();

    DispInfo *makeDispInfo(prpgd_sgi_t *pageData, void *baseAddr,
			   int pageSize);

    static void exposeCB(Widget, XtPointer client, XtPointer);
    static void inputCB(Widget, XtPointer client, XtPointer cb);
    static void resizeCB(Widget, XtPointer client, XtPointer);
    static void scrollCB(Widget, XtPointer client, XtPointer cb);
    static void motionHandler(Widget, XtPointer, XEvent *, Boolean *);
    static void focusHandler(Widget, XtPointer, XEvent *, Boolean *);

    static XtResource resources[];
    unsigned long invalidColor, residentColor, pagedOutColor, zappedColor;
    unsigned long lineColor;
    GC invalidGC, residentGC, pagedOutGC, lineGC, zappedGC, backgroundGC;
    int lineWidth;
    Widget scrollBar, drawArea;
    Window drawWin;
    Display *display;
    DispInfo *disp;
    int nPages;
    Dimension drawWidth, drawHeight;
    Dimension textWidth, textHeight, textMargin;
    XmFontList fontList;
    int zoomFactor, scrollFactor;
    int selectedPage;
    Boolean hasFocus;
};

