// $Revision: 1.1
// $Date: 1996/02/26 01:28:24 $

#include "tuBitmapTile.h"
#include "tuGC.h"
#include "tuPalette.h"
#include "tuScreen.h"
#include "tuShader.h"
#include "tuUnPickle.h"
#include "tuVisual.h"
#include "tuWindow.h"
#include "tuError.h"

tuResourceItem tuBitmapTile::resourceItems[] = {
    { "bitmap", "Bitmap", tuResString, 0, offsetof(tuBitmapTile, fileName) },
    { "foreground", "Foreground", tuResForeGC, &tuPalette::black,
	offsetof(tuBitmapTile, fgc), },
    0,
};

tuResourceChain tuBitmapTile::resourceChain = {
    "BitmapTile",
    tuBitmapTile::resourceItems,
    &tuGadget::resourceChain
};

tuBitmapTile::tuBitmapTile(tuGadget* parent, const char* inst)
: tuGadget(parent, inst) {
    resources = &resourceChain;
    stencil = 0;
    stencilgc = 0;
    pix_width = pix_height = 32;
}

tuBitmapTile::~tuBitmapTile() {
    if (stencil) {
	XFreePixmap(window->getDpy(), stencil);
	stencil = 0;
    }
    if (stencilgc) {
	XFreeGC(window->getDpy(), stencilgc);
	stencilgc = 0;
    }
}

void tuBitmapTile::bindResources(tuResourceDB* db,tuResourcePath* path)
{
    tuGadget::bindResources(db, path);
    if (fileName) {
	if (stencil != 0)
	    XFreePixmap(window->getDpy(), stencil);
	unsigned int w, h;
	int result = XReadBitmapFile(window->getDpy(),
				     window->getScreen()->getRootWindow(),
				     fileName, &w, &h, &stencil, 0, 0);
	if (result == BitmapSuccess) {
	    pix_width = w;
	    pix_height = h;
	} else
	    stencil = 0;
    }
}

void tuBitmapTile::getLayoutHints(tuLayoutHints* hints)
{
    if (!bound) bindResources();
    hints->prefWidth = pix_width + 3;
    hints->prefHeight = pix_height + 3;
    hints->flags = tuLayoutHints_fixedWidth | tuLayoutHints_fixedHeight;
}

void tuBitmapTile::setBitmap(const char* data, unsigned int w, unsigned int h) {
    if (stencil != 0)
	XFreePixmap(window->getDpy(), stencil);
    stencil = XCreateBitmapFromData(window->getDpy(),
				    window->getScreen()->getRootWindow(),
				    data, w, h);
    pix_width = w;
    pix_height = h;
    if (stencilgc != 0) {
	XFreeGC(window->getDpy(), stencilgc);
	stencilgc = 0;
    }
    updateLayout();
}

void tuBitmapTile::render() {
    tuShader* mom;
    tuShader* kid;
    int kidShade;
    int momShade;
    int w, h;

    kid = mom = getBackgroundShader();
    kidShade = momShade = getNaturalShade();
    if (parent)  {
	mom = parent->getBackgroundShader();
	momShade = parent->getNaturalShade();
    }
    outlineEdges(0, 0, w = getWidth(), h = getHeight(),
		mom->getStepGC(momShade, -1), mom->getStepGC(momShade, +1),
		kid->getGC(tuShader_darkest), kid->getGC(tuShader_darkest));
    renderEdges(2, 2, w-4, h-4,
		 kid->getStepGC(kidShade, +1), kid->getStepGC(kidShade, -1));
    // The bitmap is drawn kind of funny in order to allow both the background
    // and foreground colors to be dithered.  The dithered background is drawn
    // by the expose processing in tuGadget.  The dithered foreground is drawn
    // by drawing a dithered rectangle stenciled through the bitmap
    // onto the background.
    if (stencil) {
	Display* dpy = window->getDpy();
	Window theWindow = window->getWindow();
	if (stencilgc == None) {
	    XGCValues gcv;
	    stencilgc = XCreateGC(dpy, theWindow, 0, &gcv);
	    XCopyGC(dpy, fgc->getGC(window->getColorMap()),
		    GCForeground|GCBackground|GCStipple|GCFillStyle,
		    stencilgc);
	    XSetClipMask(dpy, stencilgc, stencil);
	}
	w = TU_MIN(pix_width, w-6);
	h = TU_MIN(pix_height, h-6);
	XSetClipOrigin(dpy, stencilgc, xAbs+3, yAbs+3); // XXX not here !!
	XFillRectangle(dpy, theWindow, stencilgc, xAbs+3, yAbs+3, w, h);
    }
}

static tuGadget* Unpickle(tuGadget* parent, char* instanceName,
			  char** /*properties*/) {
    return new tuBitmapTile(parent, instanceName);
}

void tuBitmapTile::registerForPickling() {
    tuUnPickle::registerForPickling(resourceChain.className, &Unpickle, "");
}
