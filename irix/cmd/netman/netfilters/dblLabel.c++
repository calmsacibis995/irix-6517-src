// $Revision: 1.6 $
// $Date: 1996/02/26 01:27:46 $

#include <stdio.h>

#include "dblLabel.h"

#include <tuLabel.h>
#include <tuFont.h>
#include <tuGC.h>
#include <tuShader.h>
#include <tuWindow.h>
#include <tuVisual.h>
#include <tuStyle.h>
#include <tuPalette.h>
#include <tuUnPickle.h>

const int INSET = 3;	// space between edges and text -- allow for neg lbearing
const int GAP = 4;  	// space between left & right text strings

tuResourceItem dblLabel::resourceItems[] = {
    { "leftLabelString", 0, tuResString, 0,
	offsetof(dblLabel, leftText), },
    { "rightLabelString", 0, tuResString, 0,
	offsetof(dblLabel, rightText), },
    { "foreground", 0, tuResForeGC, &tuPalette::textForeground,
	offsetof(dblLabel, gc), },
    { "disabledForeground", 0, tuResForeGC,
      &tuPalette::disabledTextForeground, offsetof(dblLabel, disabledgc), },
    { "selectedBackground", 0, tuResForeGC,
      &tuPalette::basicBackground, offsetof(dblLabel, selectedgc), },
    { "font", 0, tuResFont, tuStyle::labelFont,
	offsetof(dblLabel, font) },
    0,
};

tuResourceChain dblLabel::resourceChain = {
    "DblLabel",
    dblLabel::resourceItems,
    &tuGadget::resourceChain
};

dblLabel::dblLabel(tuGadget* parent, const char* inst,
    const char* lt,  const char* rt)
: tuGadget(parent, inst) {
    resources = &resourceChain;

    leftText = strdup(lt);
    rightText = strdup(rt);
    
    if (leftText[0] != '\0')
	markResourceChanged(&leftText);
    if (rightText[0] != '\0')
	markResourceChanged(&rightText);
}

dblLabel::~dblLabel() {
    // 3/23/94 note: ~tuLabel is empty; maybe this can be, too?
    if (gc) gc->markDelete();
    if (disabledgc) disabledgc->markDelete();
    if (selectedgc) selectedgc->markDelete();
    gc = NULL;
    disabledgc = NULL;
    selectedgc = NULL;
    // xxx 7/30/92: was dumping core, deleting twice, so commented these out
    // delete [] leftText;
    // delete [] rightText;
}


u_long
dblLabel::getEventMask() {
    return ButtonPressMask | tuGadget::getEventMask();
    // so tuGadget will see any attempted drag operation
}

void
dblLabel::bindResources(tuResourceDB* db, tuResourcePath* path) {
    tuGadget::bindResources(db, path);
    leftTextLen = strlen(leftText);
    rightTextLen = strlen(rightText);
    leftTextWidth = font->getInkedWidth(leftText, leftTextLen) + 2; // xxx
    rightTextWidth = font->getInkedWidth(rightText, rightTextLen) + 2; // xxx
}

void
dblLabel::getLayoutHints(tuLayoutHints* hints) {
    if (!bound) bindResources();
    int leftWidth = font->getInkedWidth(leftText, leftTextLen);
    int rightWidth = font->getInkedWidth(rightText, rightTextLen);
    
    hints->prefWidth = TU_MAX(2, (leftWidth + rightWidth));
    hints->prefHeight = TU_MAX(1, font->getMaxHeight());
    hints->verticalAscent = font->getMaxAscent();
    hints->horizontalAscent = 0;
    hints->flags = tuLayoutHints_fixedWidth | tuLayoutHints_fixedHeight |
	tuLayoutHints_ascent;
}

void
dblLabel::setSelected(tuBool value) {
    if (value != selected) {
	tuWindow::Freeze();

	if (!value && opened && mapped) {
	    // We just turned off the selected bit; clear the window to clear
	    // the selection highlighting out of the window.
	    window->addRect(this, bbox, True);
	}
	tuGadget::setSelected(value);
	tuWindow::Unfreeze();
    }
}


void
dblLabel::resize(int w,  int h) {
    tuGadget::resize(w, h);
    int availWidth = w - 2*INSET - GAP;
    leftWidth = availWidth / 2;
    rightWidth = availWidth - leftWidth;
    leftX = INSET;
    rightX = INSET + leftWidth + GAP;
} // resize


int
dblLabel::writeToFile(FILE* fp) {
    return (fprintf(fp, "%s\n", leftText) && fprintf(fp, "%s\n", rightText));
} // writeToFile


/* 3/30/94: tuGadget::paintText() has changed a lot, probably because of
 * i18n.  I changed this a bit,  but didn't follow all of their changes.
 */
// copied from tuGadget and modified
// startOffset is position (pixels from left) at which to start painting.
// maxWidth is what's available to paint in.
void
dblLabel::ourPaintText(int x, int y, tuFont* font, tuGC* gc,
			  const char* text, int length, 
			  int startOffset, int maxWidth) {

    if (length <= 0) return;
    if (window->getOwner() != this) {
	// We need to do clipping because we aren't in our own window.  The
	// clipping we do is pretty brutal: if the window isn't high enough,
	// don't paint at all.  If it isn't wide enough, drop characters off
	// the end until we can fit.
	if (y - font->getMaxAscent() < 0) return;
	if (y + font->getMaxDescent() > getHeight()) return;
	int leftEdge = x; // where to start text on the left
	if (leftEdge < 0) leftEdge = 0;
	while (x - startOffset < leftEdge) {
	    x += font->getWidth(text, 1);
    	    text++;
	    length--;
	    if (length == 0) return;
	}
	int maxroom = maxWidth - x + startOffset + leftEdge - 1;
	if (maxroom <= 0) return;
    	while (font->getInkedWidth(text, length) > maxroom) length--;
    }

    XmbDrawString(window->getDpy(), window->getWindow(),
		font->getFontSet(), gc->getGC(window->getColorMap()),
		xAbs + x - startOffset, yAbs + y, text, length);
} // ourPaintText


void
dblLabel::render() {
    if (!isRenderable()) return;
    
    Display* dpy = window->getDpy();
    Window w = window->getWindow();
    tuColorMap* cmap = window->getColorMap();

    tuGC* g = selected ? gc : selectedgc;

    int x1 = leftX + leftWidth + 1;
    int x2 = rightX - 1;
    tuGC* dark = parent->getBackgroundShade(tuShader_dark);

    XFillRectangle(dpy, w, g->getGC(cmap),
		       xAbs, yAbs, getWidth(), getHeight());
    XFillRectangle(dpy, w, dark->getGC(cmap),
		       x1, yAbs, x2-x1, getHeight());	

    if (!enabled) g = disabledgc;
    else g = selected ? selectedgc : gc;

    if (leftTextLen > 0) {
    	ourPaintText(leftX,
    		font->getMaxAscent(), font, g, leftText, leftTextLen, 
		leftPosition, leftWidth);
    }

    if (rightTextLen > 0) {
	ourPaintText(rightX,
		font->getMaxAscent(), font, g, rightText, rightTextLen, 
		rightPosition, rightWidth);
    }
}


static tuGadget*
Unpickle(tuGadget* parent, char* instanceName,
			  char** /*properties*/) {
    return new dblLabel(parent, instanceName, "", "");
}

void
dblLabel::registerForPickling() {
    tuUnPickle::registerForPickling(resourceChain.className, &Unpickle, "");
}


// copied from tuLabel, where it says:
// XXX This should make labels always have the same
// background color as their parent.  I'm not sure,
// though, if this implementation is a good idea or if it's a hack...
int
dblLabel::getNaturalShade() {
    return parent->getNaturalShade();
}

void
dblLabel::setLeftText(char* newText)  {
    if (strcmp(newText, leftText) == 0) return;
    delete [] leftText;
    leftText = strdup(newText);
    leftTextLen = strlen(newText);
    leftTextWidth = font->getInkedWidth(leftText, leftTextLen) + 2; // xxx
    markResourceChanged(&leftText);

    updateLayout();
    if (opened && mapped) {
	// erase old text and cause an expose
	window->addRect(this, bbox, True);
    }
    // tuLabel::setText erases the old text here before rendering,
    // but we won't bother, because render does that anyway.
//    render();

} // setLeftText


void
dblLabel::setRightText(char* newText) {
    if (strcmp(newText, rightText) == 0) return;
    delete [] rightText;
    rightText = strdup(newText);
    rightTextLen = strlen(newText);
    rightTextWidth = font->getInkedWidth(rightText, rightTextLen) + 2; // xxx
    markResourceChanged(&rightText);
    
    updateLayout();
    if (opened && mapped) {
	// erase old text and cause an expose
	window->addRect(this, bbox, True);
    }
//    updateLayout();
//    render();

} // setRightText

void
dblLabel::setFont(tuFont* f) {
    if (f != font) {
      font = f;
      markResourceChanged(&font);
      if (bound && mapped) {
	  // erase old text and cause an expose
	  window->addRect(this, bbox, True);
      }
    }
}

