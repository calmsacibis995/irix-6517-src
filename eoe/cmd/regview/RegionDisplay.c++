/*
 * RegionDisplay.h
 *
 *	definition of RegionDisplay class for regview
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

#ident "$Revision: 1.5 $"

#define XK_MISCELLANY
#include <X11/keysym.h>

#include <Xm/ScrolledW.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/Label.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "util.h"
#include "RegionDisplay.h"

/*
 * Access for Legend class
 */
static RegionDisplay *theRegionDisplay;

class DispInfo {
public:
    void *vaddr;
    GC gc;
};

/*
 * Name of callback list that we call when the user selects a page
 * with the mouse or arrow keys.
 */
const char RegionDisplay::pageSelectCB[] = "pageSelectCB";

XtResource RegionDisplay::resources[] = {
    { "invalidColor", "InvalidColor", XmRPixel, sizeof (unsigned long),
      XtOffset(RegionDisplay *, invalidColor), XmRString, "#608189" },
    { "residentColor", "ResidentColor", XmRPixel, sizeof (unsigned long),
      XtOffset(RegionDisplay *, residentColor), XmRString, "#8e388e" },
    { "pagedOutColor", "PagedOutColor", XmRPixel, sizeof (unsigned long),
      XtOffset(RegionDisplay *, pagedOutColor), XmRString, "#7171c6" },
    { "zappedColor", "ZappedColor", XmRPixel, sizeof (unsigned long),
      XtOffset(RegionDisplay *, zappedColor), XmRString, "#c67171" },
    { "lineColor", "LineColor", XmRPixel, sizeof (unsigned long),
      XtOffset(RegionDisplay *, lineColor), XmRString, "black" },
    { "lineWidth", "LineWidth", XmRInt, sizeof (int),
      XtOffset(RegionDisplay *, lineWidth), XmRString, "1" },
    { XmNfontList, XmCFontList, XmRFontList, sizeof (XmFontList),
      XtOffset(RegionDisplay *, fontList), XmRString,
      "-*-courier-medium-r-*-*-12-*" },
    { "textMargin", "TextMargin", XmRDimension, sizeof (Dimension),
      XtOffset(RegionDisplay *, textMargin), XmRString, "5" },
};

/*
 *  RegionDisplay::RegionDisplay(const char *name,
 *  			         Widget parent) : VkComponent(name)
 *
 *  Description:
 *      RegionDisplay constructor
 *
 *  Parameters:
 *      name    ViewKit name of object to create
 *      parent  parent Widget
 */

RegionDisplay::RegionDisplay(const char *name,
			     Widget parent) : VkComponent(name)
{
    theRegionDisplay = this;
    reset();
    hasFocus = False;

    int n;
    Arg wargs[10];

    n = 0;
    XtSetArg(wargs[n], XmNscrollingPolicy, XmAPPLICATION_DEFINED); n++;
    _baseWidget = XmCreateScrolledWindow(parent, "regionDisplay", wargs, n);
    installDestroyHandler();
    getResources(resources, XtNumber(resources));

    n = 0;
    XtSetArg(wargs[n], XmNorientation, XmVERTICAL); n++;
    scrollBar = XmCreateScrollBar(_baseWidget, "scrollBar", wargs, n);
    XtManageChild(scrollBar);
    XtAddCallback(scrollBar, XmNvalueChangedCallback, scrollCB, this);
    XtAddCallback(scrollBar, XmNdragCallback, scrollCB, this);

    drawArea = XmCreateDrawingArea(_baseWidget, "drawArea", NULL, 0);
    XtManageChild(drawArea);
    XtAddEventHandler(drawArea, FocusChangeMask, False,
		      focusHandler, this);

    XmScrolledWindowSetAreas(_baseWidget, NULL, scrollBar, drawArea);
}

/*
 * RegionDisplay destructor
 */
RegionDisplay::~RegionDisplay()
{
}

/*
 *  void RegionDisplay::zoomIn()
 *
 *  Description:
 *      Increase magnification by one step
 */

void RegionDisplay::zoomIn()
{
    zoomFactor++;
    setScrollBar();
    draw();
}

/*
 *  void RegionDisplay::zoomOut()
 *
 *  Description:
 *      Decrease magnification by one step
 */

void RegionDisplay::zoomOut()
{
    if (zoomFactor > 1) {
	zoomFactor--;
	setScrollBar();
	draw();
    }
}

/*
 *  void RegionDisplay::afterRealizeHook()
 *
 *  Description:
 *      Do initialization stuff that needs to take place after the
 *      window gets mapped.
 */

void RegionDisplay::afterRealizeHook()
{
    drawWin = XtWindow(drawArea);
    display = XtDisplay(drawArea);

    XtAddCallback(drawArea, XmNexposeCallback, exposeCB, this);
    XtAddCallback(drawArea, XmNinputCallback, inputCB, this);
    XtAddCallback(drawArea, XmNresizeCallback, resizeCB, this);

    XGCValues gcv;

    gcv.foreground = invalidColor;
    invalidGC = XCreateGC(display, drawWin, GCForeground, &gcv);

    gcv.foreground = residentColor;
    residentGC = XCreateGC(display, drawWin, GCForeground, &gcv);

    gcv.foreground = pagedOutColor;
    pagedOutGC = XCreateGC(display, drawWin, GCForeground, &gcv);

    gcv.foreground = zappedColor;
    zappedGC = XCreateGC(display, drawWin, GCForeground, &gcv);

    XmFontContext context;
    if (!XmFontListInitFontContext(&context, fontList)) {
	(void)fprintf(stderr, "Couldn't get a font!\n");
	exit(1);
    }

    XFontStruct *font;
    XmStringCharSet charset;
    XmFontListGetNextFont(context, &charset, &font);
    XmFontListFreeFontContext(context);
    gcv.font = font->fid;
    textHeight = font->ascent + font->descent;
    textWidth = XTextWidth(font, "00000000", 8) + textMargin * 2;

    int depth, n;
    n = 0;
    Arg wargs[10];
    XtSetArg(wargs[n], XmNdepth, &depth); n++;
    XtGetValues(drawArea, wargs, n);

    gcv.foreground = lineColor;
    gcv.line_width = lineWidth;
    lineGC = XtGetGC(drawArea, GCForeground | GCFont | GCLineWidth, &gcv);

    Pixel backgroundColor;
    n = 0;
    XtSetArg(wargs[n], XmNbackground, &backgroundColor); n++;
    XtGetValues(drawArea, wargs, n);

    gcv.foreground = backgroundColor;
    backgroundGC = XCreateGC(display, drawWin, GCForeground, &gcv);

    setScrollBar();
    resize();
}

/*
 *  DispInfo *RegionDisplay::makeDispInfo(prpgd_sgi_t *pageData,
 *  				          void *baseAddr, int pageSize)
 *
 *  Description:
 *      Turn a prpgd_sti_t array into an array of DispInfo objects.
 *      Each object has the stuff we need to draw the bar and label
 *      for the page.
 *
 *  Parameters:
 *      pageData  page descriptors
 *      baseAddr  base address of the region
 *      pageSize  how big each page is
 *
 *  Returns:
 *	An array of corresponding DispInfo objects.
 */

DispInfo *RegionDisplay::makeDispInfo(prpgd_sgi_t *pageData,
				      void *baseAddr, int pageSize)
{
    DispInfo *di = new DispInfo[pageData->pr_pglen];

    for (int i = 0; i < pageData->pr_pglen; i++) {
	switch (RegionInfo::getPageState(pageData->pr_data + i)) {
	  case RegionInfo::kResident:
	    di[i].gc = residentGC;
	    break;
	  case RegionInfo::kZapped:
	    di[i].gc = zappedGC;
	    break;
	  case RegionInfo::kPagedOut:
	    di[i].gc = pagedOutGC;
	  case RegionInfo::kInvalid:
	  default:
	    di[i].gc = invalidGC;
	    break;
	}

	di[i].vaddr = (void *)((char *)baseAddr + i * pageSize);
    }
	
    return di;
}

/*
 *  Boolean DispInfoEqual(DispInfo *d1, DispInfo *d2, int n)
 *
 *  Description:
 *      Compare to DispInfo arrays.
 *
 *  Parameters:
 *      d1  One DispInfo array
 *      d2  Another DispInfo array
 *      n   number of elements to compare
 *
 *  Returns:
 *     TRUE if they're the same, FALSE otherwise
 */

Boolean DispInfoEqual(DispInfo *d1, DispInfo *d2, int n)
{
    while (n--) {
	if (d1->vaddr != d2->vaddr || d1->gc != d2->gc) {
	    return False;
	}
	d1++;
	d2++;
    }
    return True;
}

/*
 *  void RegionDisplay::reset()
 *
 *  Description:
 *      Initialize some variables to flush our data structures.
 */

void RegionDisplay::reset()
{
    selectedPage = -1;
    scrollFactor = 0;
    zoomFactor = 1;
    if (disp) {
	delete [] disp;
	disp = NULL;
    }
    nPages = 0;
}

/*
 *  void RegionDisplay::newRegion()
 *
 *  Description:
 *      Reinitialize variables to setup for a new region.
 */

void RegionDisplay::newRegion()
{
    reset();
    setScrollBar();
    draw();
}

/*
 *  void RegionDisplay::update(RegionInfo *info)
 *
 *  Description:
 *      Update the display to reflect any changes in the region being
 *      monitored.
 *
 *  Parameters:
 *      info
 */

void RegionDisplay::update(RegionInfo *info)
{
    prpgd_sgi_t *pageData = info->getPageData();
    prmap_sgi_t *map = info->getMap();

    DispInfo *di = makeDispInfo(pageData, map->pr_vaddr,
				info->getPageSize());

    /*
     * Don't do anything if the new stuff is the same as the old.
     */
    if (disp && nPages == pageData->pr_pglen
	&& DispInfoEqual(di, disp, nPages)) {
	delete [] di;
	return;
    }

    if (disp) {
	delete [] disp;
    }

    disp = di;
    nPages = (int)pageData->pr_pglen;
    setScrollBar();
    draw();
}

/*
 *  void RegionDisplay::draw()
 *
 *  Description:
 *      Draw a bar for each page in the region, and label each bar if
 *      the labels wil fit vertically.
 */

void RegionDisplay::draw()
{
    if (!disp) {
	return;
    }
    
    XRectangle clip;

    clip.x = 0;
    clip.y = 0;
    clip.width = drawWidth;
    clip.height = drawHeight;
    XSetClipRectangles(display, invalidGC, 0, 0, &clip, 1, Unsorted);
    XSetClipRectangles(display, residentGC, 0, 0, &clip, 1, Unsorted);
    XSetClipRectangles(display, pagedOutGC, 0, 0, &clip, 1, Unsorted);
    XSetClipRectangles(display, backgroundGC, 0, 0, &clip, 1,
		       Unsorted);
    
    Dimension width = drawWidth - textWidth;
    Dimension x = textWidth;
    float height = (float)(drawHeight * zoomFactor) / (float)nPages;

    Boolean doOutlines = height > 2 * lineWidth;
    if (!doOutlines) {
	XDrawRectangle(display, drawWin, lineGC, x, 0, width, drawHeight);
    }

    /*
     * Draw a border if we have the focus, so keyboard traversal looks
     * nice.  If we've drawn the border, clip the rest of our output
     * so that we don't erase it (this avoids yucky flashing).
     */
    if (hasFocus) {
	XDrawRectangle(display, drawWin, lineGC, 0, 0, drawWidth - 1,
		       drawHeight - 1);
	clip.x = 1;
	clip.y = 1;
	clip.width = drawWidth - 2;
	clip.height = drawHeight - 2;
	XSetClipRectangles(display, invalidGC, 0, 0, &clip, 1, Unsorted);
	XSetClipRectangles(display, residentGC, 0, 0, &clip, 1, Unsorted);
	XSetClipRectangles(display, pagedOutGC, 0, 0, &clip, 1, Unsorted);
	XSetClipRectangles(display, backgroundGC, 0, 0, &clip, 1,
			   Unsorted);
    }

    Boolean doLabels = height >= textHeight;

    /*
     * Don't render the ones that the user won't see.
     */
    int i = (int)(scrollFactor / height);
    
    while (i < nPages) {
	int y = i * height - scrollFactor;

	/*
	 * Quit when we get to the bottom of the window
	 */
	if (y > drawHeight) {
	    break;
	}

	int ypixels = (i + 1) * height - y - scrollFactor;

	if (doLabels) {
	    char buf[20];
	    (void)sprintf(buf, "%08x", disp[i].vaddr);
	    XFillRectangle(display, drawWin, backgroundGC, 0, y,
			   textWidth, ypixels);
	    XDrawString(display, drawWin, lineGC, textMargin,
			y + textHeight, buf, strlen(buf));
	} else {
	    XFillRectangle(display, drawWin, backgroundGC, 0, (int)y,
			   textWidth, ypixels);
	}
	    
	XFillRectangle(display, drawWin, disp[i].gc, x + 1, y,
		       width - 2, ypixels);

	if (doOutlines) {
	    XDrawRectangle(display, drawWin, lineGC, x, y, width,
			   ypixels);
	}

	/*
	 * Draw an extra rectangle around the selected page.
	 */
	if (i == selectedPage) {
	    if (ypixels > lineWidth * 2) {
		XDrawRectangle(display, drawWin, lineGC, x + lineWidth,
			       y + lineWidth, width - 2 * lineWidth,
			       ypixels - 2 * lineWidth);
	    } else {
		XDrawLine(display, drawWin, lineGC, x, y, x + width, y);
	    }
	}
	y += ypixels;
	i++;
    }
}

/*
 *  void RegionDisplay::handleInput(XEvent *event)
 *
 *  Description:
 *      Deal with mouse button presses, mouse dragging, and the up and
 *      down arrow keys.
 *
 *  Parameters:
 *      event  an X event
 */

void RegionDisplay::handleInput(XEvent *event)
{
    if (!disp || !event) {
	return;
    }

    Boolean selected = False;
    float height = (float)(drawHeight * zoomFactor) / (float)nPages;
    int page;

    switch (event->type) {
    case ButtonPress:
	XtAddEventHandler(drawArea, PointerMotionMask, False,
			  motionHandler, this);
	break;
    case MotionNotify:
	page = (int)((scrollFactor + event->xmotion.y) / height);
	selected = True;
	break;
    case ButtonRelease:
	XtRemoveEventHandler(drawArea, PointerMotionMask, False,
			     motionHandler, this);
	page = (int)((scrollFactor + event->xbutton.y) / height);
	selected = True;
	break;
    case KeyPress:
	switch (XLookupKeysym(&event->xkey, 0)) {
	case XK_Up:
	    if (selectedPage > 0) {
		page = selectedPage - 1;
		selected = True;
	    }
	    break;
	case XK_Down:
	    if (selectedPage < nPages - 1) {
		page = selectedPage + 1;
		selected = True;
	    }
	    break;
	}
	break;
    }

    if (selected) {
	if (page < 0) {
	    page = 0;
	} else if (page >= nPages) {
	    page = nPages - 1;
	}
	if (page != selectedPage) {
	    selectedPage = page;
	    /*
	     * Check to see if we need to scroll in order to keep the
	     * selected page visible.
	     */
	    if (height * page < scrollFactor) {
		scrollFactor = (int)(height * page);
		setScrollBar();
	    } else if (page > 0 &&
		       height * (page + 1) > drawHeight + scrollFactor) {
		scrollFactor = (int)(height * (page + 1) - drawHeight);
		setScrollBar();
	    }
	    draw();
	    callCallbacks(pageSelectCB, disp[page].vaddr);
	}
    }
}

/*
 *  void RegionDisplay::resize()
 *
 *  Description:
 *      Determine our new size and render ourselves appropriately.
 */

void RegionDisplay::resize()
{
    int n = 0;
    Arg wargs[10];
    XtSetArg(wargs[n], XmNwidth, &drawWidth); n++;
    XtSetArg(wargs[n], XmNheight, &drawHeight); n++;
    XtGetValues(drawArea, wargs, n);

    draw();
}

/*
 *  void RegionDisplay::setScrollBar()
 *
 *  Description:
 *      Make the scroll bar attributes reflect our internal data
 *      structures appropriately.
 */

void RegionDisplay::setScrollBar()
{
    int n;
    Arg wargs[10];
    
    if (nPages == 0) {
	n = 0;
	XtSetArg(wargs[n], XmNminimum, 0); n++;
	XtSetArg(wargs[n], XmNmaximum, 1); n++;
	XtSetArg(wargs[n], XmNsliderSize, 1); n++;
	XtSetArg(wargs[n], XmNvalue, 0); n++;
	XtSetValues(scrollBar, wargs, n);
    } else {
	n = 0;
	XtSetArg(wargs[n], XmNsliderSize, drawHeight); n++;
	XtSetArg(wargs[n], XmNpageIncrement, drawHeight); n++;
	XtSetArg(wargs[n], XmNincrement, (drawHeight * zoomFactor) > nPages ?
		 (drawHeight * zoomFactor) / nPages : 1); n++;
	XtSetArg(wargs[n], XmNminimum, 0); n++;
	XtSetArg(wargs[n], XmNmaximum, drawHeight * zoomFactor); n++;
	if (scrollFactor > drawHeight * (zoomFactor - 1)) {
	    scrollFactor = drawHeight * (zoomFactor - 1);
	}
	XtSetArg(wargs[n], XmNvalue, scrollFactor); n++;
	XtSetValues(scrollBar, wargs, n);
    }
}

/*
 *  void RegionDisplay::scroll(XmScrollBarCallbackStruct *cb)
 *
 *  Description:
 *      The value of our scroll bar changed.  Update scrollFactor
 *      accordingly.
 *
 *  Parameters:
 *      cb
 */

void RegionDisplay::scroll(XmScrollBarCallbackStruct *cb)
{
    scrollFactor = cb->value;
    draw();
}

/*
 * Xt callback to call our draw function
 */
void RegionDisplay::exposeCB(Widget, XtPointer client, XtPointer)
{
    ((RegionDisplay *)client)->draw();
}

/*
 * Xt callback to call our input function
 */
void RegionDisplay::inputCB(Widget, XtPointer client, XtPointer cb)
{
    ((RegionDisplay
      *)client)->handleInput(((XmDrawingAreaCallbackStruct *)cb)->event); 
}

/*
 * Xt callback to call our resize function
 */
void RegionDisplay::resizeCB(Widget, XtPointer client, XtPointer)
{
    ((RegionDisplay *)client)->resize();
}

/*
 * Xt callback to call our scroll function
 */
void RegionDisplay::scrollCB(Widget, XtPointer client, XtPointer cb)
{
    ((RegionDisplay *)client)->scroll((XmScrollBarCallbackStruct *)cb);
}

/*
 * Event handler for pointer motion events.  This gets added only when
 * we're dragging, and removed when we stop dragging.
 */
void RegionDisplay::motionHandler(Widget, XtPointer client,
				  XEvent *event, Boolean *)
{
    ((RegionDisplay *)client)->handleInput(event);
}

/*
 * Event handler for FocusIn/FocusOut events.  We draw ourselves
 * slightly differently if we have the focus.
 */
void RegionDisplay::focusHandler(Widget, XtPointer client, XEvent *event,
				 Boolean *)
{
    RegionDisplay *regDisp = (RegionDisplay *)client;

    if (event->type == FocusIn) {
	regDisp->hasFocus = True;
	regDisp->draw();
    } else if (event->type == FocusOut) {
	regDisp->hasFocus = False;
	regDisp->draw();
    }
}

XtResource Legend::resources[] = {
    { "invalidString", "InvalidString", XmRString, sizeof (char *),
      XtOffset(Legend *, invalidString), XmRString,
      "Invalid (%d K)" },
    { "residentString", "ResidentString", XmRString, sizeof (char *),
      XtOffset(Legend *, residentString), XmRString,
      "Resident (%d K)" },
    { "pagedOutString", "PagedOutString", XmRString, sizeof (char *),
      XtOffset(Legend *, pagedOutString), XmRString,
      "Paged Out (%d K)" },
    { "zappedString", "PagedOutString", XmRString, sizeof (char *),
      XtOffset(Legend *, zappedString), XmRString,
      "Zapped (%d K)" },
};

/*
 *  Legend::Legend(const char *name, Widget parent) : VkComponent(name)
 *
 *  Description:
 *      Legend constructor.
 *
 *  Parameters:
 *      name    ViewKit name of object
 *      parent  parent Widget
 */

Legend::Legend(const char *name, Widget parent) : VkComponent(name)
{
    assert(theRegionDisplay);
    _baseWidget = XmCreateForm(parent, "legend", NULL, 0);
    installDestroyHandler();
    getResources(resources, XtNumber(resources));

    lastInvalidSize = (unsigned long)-1;
    lastResidentSize = (unsigned long)-1;
    lastPagedOutSize = (unsigned long)-1;
    lastZappedSize = (unsigned long)-1;

    invalid = createDraw(_baseWidget, "invalid",
			 theRegionDisplay->invalidColor, NULL);
    invalidLabel = createLabel(_baseWidget, "invalidLabel",
				      invalid, NULL);

    resident = createDraw(_baseWidget, "resident",
			  theRegionDisplay->residentColor, invalid);
    residentLabel = createLabel(_baseWidget, "residentLabel",
				       resident, invalid);

    Widget pagedOut = createDraw(_baseWidget, "pagedOut",
				 theRegionDisplay->pagedOutColor,
				 resident);
    pagedOutLabel = createLabel(_baseWidget, "pagedOutLabel",
				pagedOut, resident);

    Widget zapped = createDraw(_baseWidget, "zapped",
			       theRegionDisplay->zappedColor, pagedOut);
    zappedLabel = createLabel(_baseWidget, "zappedLabel", zapped,
			      pagedOut);
}

/*
 * Legend destructor.
 */
Legend::~Legend()
{
}

/*
 *  void Legend::setLabels(unsigned long invalidSize,
 *  		           unsigned long residentSize,
 *  		           unsigned long pagedOutSize,
 *  		           unsigned long zappedSize)
 *
 *  Description:
 *      Set the label strings in the legend to reflect sizes of
 *      current region.
 *
 *  Parameters:
 *      invalidSize
 *      residentSize
 *      pagedOutSize
 *      zappedSize
 */

void Legend::setLabels(unsigned long invalidSize,
		       unsigned long residentSize,
		       unsigned long pagedOutSize,
		       unsigned long zappedSize)
{
    char buf[200];

    if (invalidSize != lastInvalidSize) {
	(void)sprintf(buf, invalidString, invalidSize);
	SetLabel(invalidLabel, buf);
	lastInvalidSize = invalidSize;
    }

    if (residentSize != lastResidentSize) {
	(void)sprintf(buf, residentString, residentSize);
	SetLabel(residentLabel, buf);
	lastResidentSize = residentSize;
    }

    if (pagedOutSize != lastPagedOutSize) {
	(void)sprintf(buf, pagedOutString, pagedOutSize);
	SetLabel(pagedOutLabel, buf);
	lastPagedOutSize = pagedOutSize;
    }

    if (zappedSize != lastZappedSize) {
	(void)sprintf(buf, zappedString, zappedSize);
	SetLabel(zappedLabel, buf);
	lastZappedSize = zappedSize;
    }
}

/*
 *  Widget Legend::createDraw(Widget parent, char *name,
 *  			      unsigned long background, Widget topWidget)
 *
 *  Description:
 *      Create an XmDrawingArea widget to use as a part of a legend
 *      for the RegionDisplay component.
 *
 *  Parameters:
 *      parent      parent widget
 *      name        name of widget
 *      background  background color
 *      topWidget   top attachment
 *
 *  Returns:
 *	A drawing area widget
 */

Widget Legend::createDraw(Widget parent, char *name,
			  unsigned long background, Widget topWidget)
{
    int n;
    Arg wargs[5];

    n = 0;
    if (topWidget) {
	XtSetArg(wargs[n], XmNtopAttachment, XmATTACH_WIDGET); n++;
	XtSetArg(wargs[n], XmNtopWidget, topWidget); n++;
    }
    XtSetArg(wargs[n], XmNbackground, background); n++;
    Widget draw = XmCreateDrawingArea(parent, name, wargs, n);
    XtManageChild(draw);
    XtAddCallback(draw, XmNexposeCallback, exposeCB, this);
    return draw;
}

/*
 *  Widget Legend::createLabel(Widget parent, char *name, Widget leftWidget,
 *  			       Widget topWidget)
 *
 *  Description:
 *      Create a label widget to use as part of a legend for the
 *      RegionDisplay component.
 *
 *  Parameters:
 *      parent      parent widget
 *      name        name of widget
 *      leftWidget  left attachment
 *      topWidget   top attachment
 *
 *  Returns:
 *	the label widget
 */

Widget Legend::createLabel(Widget parent, char *name, Widget leftWidget,
			   Widget topWidget)
{
    int n;
    Arg wargs[5];

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
 *  void Legend::drawLegend(Widget draw)
 *
 *  Description:
 *      draw the border around the color in one of the drawing areas
 *      that comprise the legend.
 *
 *  Parameters:
 *      draw  drawing area widget
 */

void Legend::drawLegend(Widget draw)
{
    if (!theRegionDisplay) {
	return;
    }

    int n;
    Arg wargs[2];
    Dimension width, height;

    n = 0;
    XtSetArg(wargs[n], XmNwidth, &width); n++;
    XtSetArg(wargs[n], XmNheight, &height); n++;
    XtGetValues(draw, wargs, n);

    XDrawRectangle(XtDisplay(draw), XtWindow(draw),
		   theRegionDisplay->lineGC, 0, 0, width - 1, height - 1);
}

/*
 * Xt callback for expose event for drawing area that's part of the
 * legend.
 */
void Legend::exposeCB(Widget w, XtPointer client, XtPointer)
{
    ((Legend *)client)->drawLegend(w);
}
