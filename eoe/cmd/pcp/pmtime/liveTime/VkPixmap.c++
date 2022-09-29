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

//
// $Id: VkPixmap.c++,v 1.2 1997/05/22 05:36:14 markgw Exp $
//
//////////////////////////////////////////////////////////////
// VkSetHighlightingPixmap utility
//
// Given an XPM pixmap that has the background field set
// symbolically to the string "background",this function
// will install the pixmap in a label or button in such a way
// that the pixmap will locate-highlight in the SGI-style.
// See bug warning below.
//
// Some variation of this code should migrate into ViewKit
// in a future release for easier use.
//
///////////////////////////////////////////////////////////

#include <Vk/VkApp.h>
#include <Vk/VkResource.h>
#include <Vk/VkFormat.h>
#include <Xm/ToggleB.h>
#include <Vk/xpm.h>

Pixmap VkCreatePixmapWithBG(Widget w, char **description,
			    Pixel bg, Pixel fg, Pixel bs, Pixel ts );

void VkSetHighlightingPixmap(Widget w, 
			     char **xPixmapDesc,
			     const char *resource)

{
    Pixmap pix, armPix, locPix;
    Pixel  fg, ts, bs, bg, locate, ac, noColor;

    XtVaGetValues(w,
		  XmNarmColor,    &ac,
		  XmNbackground,  &bg,
		  XmNforeground,  &fg,
		  XmNtopShadowColor,     &ts,
		  XmNbottomShadowColor,  &bs,		  		  
		  NULL);		  

    locate =  SgGetLocatePixel(w,  bg);
    
    // This is not quite right, but there's little that can be done
    // because locate-highlight is broken in SGI's Motif
    // pixmap code. The locateArmPixmap should be the arm color
    // of the button, but in fact, buttons don't change to the
    // arm color. More importantly, the locatePixmap is the correct
    // color here, but Motif does not change the button background
    // to locate highlight when a pixmap is installed. The only
    // work around until SGI's Motif is fixed is to make margins
    // zero, and make the pixmap bigger to accomodate.

    if(!resource || !strcmp(resource, XmNlabelPixmap))
    {
	pix    = VkCreatePixmapWithBG(w,  xPixmapDesc, bg, fg, ts, bs);
	armPix = VkCreatePixmapWithBG(w,  xPixmapDesc, ac, fg, ts, bs);
	locPix = VkCreatePixmapWithBG(w,  xPixmapDesc, locate, fg, ts, bs);

	XtVaSetValues(w,
		      XmNarmPixmap,              armPix,
		      SgNlocatePixmap,           locPix,
		      SgNlocateArmPixmap,        pix,
		      SgNpixmapLocateHighlight,  TRUE,
		      XmNlabelPixmap,            pix,
		      NULL);    
    }
    else if(resource && !strcmp(resource, XmNselectPixmap))
    {
	pix    = VkCreatePixmapWithBG(w,  xPixmapDesc, bg, fg, ts, bs);
	locPix = VkCreatePixmapWithBG(w,  xPixmapDesc, locate, fg, ts, bs);

	XtVaSetValues(w,
		      SgNlocateSelectPixmap,    locPix,
		      SgNpixmapLocateHighlight, TRUE,
		      XmNselectPixmap,          pix,
		      NULL);    
    }
}


Pixmap VkCreatePixmapWithBG(Widget w, char **description,
			    Pixel bg, Pixel fg, Pixel ts, Pixel bs)
{
    Pixmap          pix = NULL;
    XpmAttributes   attributes;
    XpmColorSymbol *symbols;    
    Widget          parent = XtParent(w);
    Display        *display = theApplication->display();
    unsigned int closeness = 0;

    symbols = new XpmColorSymbol[10];

    symbols[0].name  = XmNbackground;
    symbols[0].value = NULL;
    symbols[0].pixel = bg;
    symbols[1].name  = XmNforeground;
    symbols[1].value = NULL;
    symbols[1].pixel = ts;
    symbols[2].name  = XmNtopShadowColor;
    symbols[2].value = NULL;
    symbols[2].pixel = ts;
    symbols[3].name  = XmNbottomShadowColor;
    symbols[3].value = NULL;
    symbols[3].pixel = bs;        

    closeness = (unsigned int) VkGetResource(w,
					     "xpmColorCloseness", "XpmColorCloseness", 
					     XmRInt, 0);

    attributes.colorsymbols   = symbols;
    attributes.numsymbols     = 4;
    attributes.closeness      = closeness;    
    attributes.depth          = DefaultDepth(display, DefaultScreen(display));
    attributes.visual         = DefaultVisual(display,
					      DefaultScreen(display));
    attributes.colormap  = DefaultColormap(display,
					   DefaultScreen(display));
    
    attributes.valuemask = (XpmDepth | XpmVisual | XpmColormap |
			    XpmColorSymbols | XpmCloseness);

    int status =  XpmCreatePixmapFromData(theApplication->display(), 
					  RootWindowOfScreen(XtScreen(parent)),
					  description, &pix, 
					  NULL, 
					  &attributes);

    if(status == XpmNoMemory)
	XtWarning("Error from Xpm: Not enough memory to create pixmap");
    else if (status == XpmColorFailed)
	XtWarning("Error from Xpm: Failed to parse or alloc a color");
    
    XpmFreeAttributes(&attributes);
    delete symbols;

    if(pix)
	return (pix);
    else
	return XmUNSPECIFIED_PIXMAP;
}

