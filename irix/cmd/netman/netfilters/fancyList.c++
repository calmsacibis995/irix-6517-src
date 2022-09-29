// $Revision: 1.3 $
// $Date: 1992/08/12 17:22:46 $

#include "dblLabel.h"
#include "fancyList.h"

#include <stdio.h>
#include <strings.h>

#include <tuCallBack.h>
#include <tuEventGrabber.h>
#include <tuGadget.h>
#include <tuGC.h>
#include <tuList.h>
#include <tuPalette.h>
#include <tuScrollBar.h>
#include <tuShader.h>
#include <tuStyle.h>
#include <tuTimer.h>
#include <tuWindow.h>
#include <tuUnPickle.h>

const int CHILD_EDGE = 0; // how much space on each side of children.

tuResourceItem fancyList::resourceItems[] = {
    0,
};

tuResourceChain fancyList::resourceChain = {
    "FancyList",
    fancyList::resourceItems,
    &tuList::resourceChain
};



tuDeclareCallBackClass(fancyListCallBack, fancyList);
tuImplementCallBackClass(fancyListCallBack, fancyList);

fancyList::fancyList(tuGadget* parent, char* inst)
: tuList(parent, inst) {
    resources = &resourceChain;
    leftScrollbar = new tuScrollBar(this, "leftScroll", True);
    	    	    	    	// Side effect: gets added to child list.
    rightScrollbar = new tuScrollBar(this, "rightScroll", True);

    children.removeAll();
    useOwnWindow();		// Make sure we clip our children.
    leftScrollbar->useOwnWindow();	// The scrollbar also needs to clip our
				    	// children.
    scrollbar->setDragCallBack(new fancyListCallBack(this, &fancyList::relayout));
    scrollbar->setIncCallBack(new fancyListCallBack(this, &fancyList::inc));
    scrollbar->setPageIncCallBack(new fancyListCallBack(this, &fancyList::pageInc));
    scrollbar->setDecCallBack(new fancyListCallBack(this, &fancyList::dec));
    scrollbar->setPageDecCallBack(new fancyListCallBack(this, &fancyList::pageDec));

    leftScrollbar->setContinuousUpdate(True);
    leftScrollbar->setDragCallBack(new fancyListCallBack(this, &fancyList::leftDrag));
    leftScrollbar->setIncCallBack(new fancyListCallBack(this, &fancyList::leftInc));
    leftScrollbar->setPageIncCallBack(new fancyListCallBack(this, &fancyList::leftPageInc));
    leftScrollbar->setDecCallBack(new fancyListCallBack(this, &fancyList::leftDec));
    leftScrollbar->setPageDecCallBack(new fancyListCallBack(this, &fancyList::leftPageDec));

    rightScrollbar->useOwnWindow();
    rightScrollbar->setContinuousUpdate(True);
    rightScrollbar->setDragCallBack(new fancyListCallBack(this, &fancyList::rightDrag));
    rightScrollbar->setIncCallBack(new fancyListCallBack(this, &fancyList::rightInc));
    rightScrollbar->setPageIncCallBack(new fancyListCallBack(this, &fancyList::rightPageInc));
    rightScrollbar->setDecCallBack(new fancyListCallBack(this, &fancyList::rightDec));
    rightScrollbar->setPageDecCallBack(new fancyListCallBack(this, &fancyList::rightPageDec));

    grabber->setCallBack(new fancyListCallBack(this, &fancyList::childevent));

}


fancyList::~fancyList() {
}



int fancyList::getNumInternalChildren() {
    return 2 + tuList::getNumInternalChildren();
}


tuGadget* fancyList::getInternalChild(int w) {
    int numListChildren = tuList::getNumInternalChildren();
    
    if (w < numListChildren)
    	return tuList::getInternalChild(w);
    else if (w == numListChildren) return leftScrollbar;
    else return rightScrollbar;
}



void fancyList::getLayoutHints(tuLayoutHints* hints) {
    if (!bound) bindResources();
    long prefWidth = 0; // tuList now puts a hard-coded margin here xxx
    long prefHeight = 0;
    tuLayoutHints h, hl, hr;
    tuBool fixedWidth = True;

    for (int i=0 ; i<children.getLength() ; i++) {
	children[i]->getLayoutHints(&h);
	prefWidth = TU_MAX(prefWidth, h.prefWidth);
	prefHeight += h.prefHeight;
	if ((h.flags & tuLayoutHints_fixedWidth) == 0) fixedWidth = False;
    }
    if (scrollbar) {
	scrollbar->getLayoutHints(&h);
	prefWidth += h.prefWidth;
	if (prefHeight < h.prefHeight) prefHeight = h.prefHeight;
    }
    if (leftScrollbar && rightScrollbar) {
	leftScrollbar->getLayoutHints(&hl);
	rightScrollbar->getLayoutHints(&hr);
    	prefHeight += hl.prefHeight; // xxx I don't know about this
	prefWidth = TU_MAX(prefWidth,
			(hl.prefWidth + hr.prefWidth + h.prefWidth));
    }
    
    hints->prefWidth  = short(TU_MAX(tuMinDimension, prefWidth));
    hints->prefHeight = short(TU_MAX(tuMinDimension, TU_MIN(prefHeight, 10000)));
    // xxx copied from tuList.c++; to prevent overflowing the short.
 
    hints->flags = fixedWidth ? tuLayoutHints_fixedWidth : 0;
}
	



void fancyList::map(tuBool propagate) {
    if (propagate) {
	leftScrollbar->map(True);
	rightScrollbar->map(True);
    }
    tuList::map(propagate);
}



void fancyList::resize(int w, int h) {
    // printf("fancyList::resize\n");
    
    if (w != getWidth() || h != getHeight())
	lastHeightCounted = 0;
	
    tuGadget::resize(w, h);
    tuWindow::Freeze();
    grabber->resize(w, h);
    tuLayoutHints hints, hintsH; // hintsH is for horizontal scrollbars
    scrollbar->getLayoutHints(&hints);
    leftScrollbar->getLayoutHints(&hintsH);
    
    short scrollbarWidth = hints.prefWidth;
    short hbarHeight = hintsH.prefHeight;
    
    int cwidth = w - scrollbarWidth - 2*CHILD_EDGE;
    if (cwidth < 2) cwidth = 2;
    short hbarWidth = cwidth / 2;
    
    int scrollbarHeight = h - hbarHeight;
    if (scrollbarHeight < 1) scrollbarHeight = 1;
    
    scrollbar->resize(scrollbarWidth, scrollbarHeight);
    scrollbar->move(w - scrollbarWidth, 0);
    
    if (lastHeightCounted == 0)
	totalHeight = 0;

    leftScrollbar->resize(hbarWidth, hbarHeight);
    rightScrollbar->resize(hbarWidth, hbarHeight);
    leftScrollbar->move(CHILD_EDGE, scrollbarHeight);
    rightScrollbar->move(hbarWidth+CHILD_EDGE,  scrollbarHeight);

    for (int i=lastHeightCounted ; i<children.getLength() ; i++) {
	tuGadget* child = children[i];
	child->getLayoutHints(&hints);
	child->resize(cwidth, hints.prefHeight);
	totalHeight += hints.prefHeight;
    }
    
    lastHeightCounted = i;
    relayout();
    tuWindow::Unfreeze();
}

void fancyList::relayout(tuGadget*) {
    if (scrollbar == NULL || leftScrollbar == NULL || rightScrollbar == NULL)
    	return;

    if (lastHeightCounted < children.getLength()) {
	if (getWidth() < 1 || getHeight() < 1) return;
	resize(getWidth(), getHeight());
	return;
    }
    int numkids = children.getLength();
    int height = getHeight();
    int heightForChildren = height - leftScrollbar->getHeight();

    if (opened) {		// Should do this less often XXX
	Display* dpy = window->getDpy();
	
	Window swindow = scrollbar->getWindow()->getWindow();
	XRaiseWindow(scrollbar->getWindow()->getDpy(), swindow);
	
	Window lwindow = leftScrollbar->getWindow()->getWindow();
	XRaiseWindow(leftScrollbar->getWindow()->getDpy(), lwindow);
	
	Window rwindow = rightScrollbar->getWindow()->getWindow();	
	XRaiseWindow(rightScrollbar->getWindow()->getDpy(), rwindow);
	
	if (doLayoutOnly) {
	    grabber->unmap();
	} else {
	    XWindowChanges values;
	    values.stack_mode = BottomIf;
	    values.sibling = swindow;
	    XConfigureWindow(dpy, grabber->getWindow()->getWindow(),
			     CWSibling | CWStackMode, &values);
	    values.sibling = lwindow;
	    XConfigureWindow(dpy, grabber->getWindow()->getWindow(),
			     CWSibling | CWStackMode, &values);
	    values.sibling = rwindow;
	    XConfigureWindow(dpy, grabber->getWindow()->getWindow(),
			     CWSibling | CWStackMode, &values);
	}
    }
    tuWindow::Freeze(); 
    int position;
    if (totalHeight > heightForChildren) {
	scrollbar->setRange(0, totalHeight - heightForChildren);
	scrollbar->setPercentShown(float(heightForChildren) / float(totalHeight));
	scrollbar->setEnabled(True);
	position = int(scrollbar->getPosition());
    } else {
	scrollbar->setRange(0, 0);
	scrollbar->setEnabled(False);
	position = 0;
    }
    int oldfirst = first;
    int oldlast = last + 1;
    if (first < 0 || first >= numkids || firstpos > position 
	|| firstpos + children[first]->getHeight() <= position) {
	//  Refigure first...
	if (first < 0 || first >= numkids) {
	    first = 0;
	    firstpos = 0;
	}
	while (first < numkids - 1 && firstpos < position) {
	    int h = children[first]->getHeight();
	    if (firstpos + h > position) break;
	    firstpos += h;
	    first++;
	}
	while (first > 0 && firstpos > position) {
	    first--;
	    firstpos -= children[first]->getHeight();
	}
	assert(first > 0 || firstpos == 0);
    }
    
    int p = firstpos - position;
    //  next 4 lines added 3/13/92 because tuList did it
    if (numkids > first && children[first]->getYOrigin() != p && window &&
	window->getWindow()) {
	    // next line changed 7/9/92 because tuList did it
//	    window->addRect(this, tuRect(0, 0, getWidth(), getHeight()), True);
	    window->addRect(this, bbox, True);
	}
	
    tuGadget* child;
    for (last = first ; last < numkids && p < heightForChildren; last++) {
	child = children[last];
	child->move(CHILD_EDGE, p);
	child->map();
	p += child->getHeight();
    }
    
    if (oldlast > numkids) oldlast = numkids;
    if (oldfirst < 0) {
	oldfirst = 0;
	oldlast = numkids;
    }
    for (int i=oldfirst ; i<oldlast ; i++) {
	if (i < first || i >= last) {
	    children[i]->unmap();
	}
    }
    
    // take care of horizontal scrollbars; width of left & right halves
    int maxLeftWidth = 0;
    int maxRightWidth = 0;
    int width;
    dblLabel* kid = (dblLabel*)0;
    for (i = 0; i < numkids; i++) {
    	kid = (dblLabel*) children[i];
    	width = kid->getLeftTextWidth();
	if (width > maxLeftWidth)
	    maxLeftWidth = width;
	width = kid->getRightTextWidth();
	if (width > maxRightWidth)
	    maxRightWidth = width;
    }
    
    if (kid)
    	width = kid->getLeftWidth();
    else
    	width = 10; // xxx this should be ok, I think.
    int leftPosition;
    if (maxLeftWidth > width) {
	leftScrollbar->setRange(0, maxLeftWidth - width);
	leftScrollbar->setPercentShown(float(width) / float(maxLeftWidth));
	leftScrollbar->setEnabled(True);
	leftPosition = int(leftScrollbar->getPosition() + 0.5);
    } else {
	leftScrollbar->setRange(0, 0);
	leftScrollbar->setEnabled(False);
	leftPosition = 0;
    }

    if (kid)
    	width = kid->getRightWidth();
    int rightPosition;
    if (maxRightWidth > width) {
	rightScrollbar->setRange(0, maxRightWidth - width);
	rightScrollbar->setPercentShown(float(width) / float(maxRightWidth));
	rightScrollbar->setEnabled(True);
	rightPosition = int(rightScrollbar->getPosition() + 0.5);
/***
("rightScrollbar upper bound: %d\n",  maxRightWidth - width);
printf("    percentShown: %f\n", float(width) / float(maxRightWidth));
printf("    position: %f, int position: %d\n", rightScrollbar->getPosition(), rightPosition);
printf("  width: %d,  maxRightWidth: %d\n", width, maxRightWidth);
***/
    } else {
	rightScrollbar->setRange(0, 0);
	rightScrollbar->setEnabled(False);
	rightPosition = 0;
    }
    
    for (i = 0; i < numkids; i++) {
	kid = (dblLabel*) children[i];
	kid->setLeftPosition(leftPosition);
	kid->setRightPosition(rightPosition);
	if (i >= first && i < last)
	    kid->render();
    }

    tuWindow::Unfreeze();
} // relayout

// xxx These are all here because
//  1) they aren't virtual in tuList (THEY ARE NOW, as of 1/2/92)
//  2) they aren't as good as they could be in tuList.
// When they're fixed there, remove them from this class.
void fancyList::inc(tuGadget*) {
    if (first < 0) {
	relayout();
    }
//xxx    if (scrollbar->getPosition() != scrollbar->getUpperBound()) {
	scrollbar->setPosition(firstpos + children[first]->getHeight());
	relayout();
//xxx    }
}


void fancyList::dec(tuGadget*) {
    if (first < 0) {
	relayout();
    }
//xxx    if (scrollbar->getPosition() != scrollbar->getLowerBound()) {
	scrollbar->setPosition(firstpos - children[first]->getHeight());
	relayout();
//xxx    }
}


void fancyList::pageInc(tuGadget*) {
    if (first < 0) {
	relayout();
    }
    if (scrollbar->getPosition() != scrollbar->getUpperBound()) {
    	scrollbar->setPosition(firstpos +
	    	    	       getHeight() - leftScrollbar->getHeight());
    	relayout();
    }
}


void fancyList::pageDec(tuGadget*) {
    if (first < 0) {
	relayout();
    }
    if (scrollbar->getPosition() != scrollbar->getLowerBound()) {
    	scrollbar->setPosition(firstpos -
	    	    	       getHeight() + leftScrollbar->getHeight());
    	relayout();
    }
}



// adjust all kids' left (if left=True) or right position
// I have 'for' loop in each 'if' case so that we don't have to do
// an 'if' every time throught the 'for'.
void fancyList::adjustKids(tuBool left) {
    dblLabel* kid;
    int position;
    if (left) {
    	position = int(leftScrollbar->getPosition() + 0.5);
        for (int i = 0; i < children.getLength(); i++) {
	    kid = (dblLabel*) children[i];
    	    kid->setLeftPosition(position);
	    kid->render();
    	}
    }
    else {
    	position = int(rightScrollbar->getPosition() + 0.5);
        for (int i = 0; i < children.getLength(); i++) {
	    kid = (dblLabel*) children[i];
    	    kid->setRightPosition(position);
	    kid->render();
    	}
    }
} // adjustKids


void fancyList::leftDrag(tuGadget*) {
    adjustKids(True);
} 


void fancyList::rightDrag(tuGadget*) {
    adjustKids(False);
}


void fancyList::leftInc(tuGadget*) {
    if (leftScrollbar->getUpperBound() != leftScrollbar->getPosition()) {
    	leftScrollbar->setPosition(leftScrollbar->getPosition()+1);
    	adjustKids(True);
    }
}


void fancyList::rightInc(tuGadget*) {
    if (rightScrollbar->getUpperBound() != rightScrollbar->getPosition()) {
    	rightScrollbar->setPosition(rightScrollbar->getPosition()+1);
    	adjustKids(False);
    }
}


void fancyList::leftDec(tuGadget*) {
    if (leftScrollbar->getLowerBound() != leftScrollbar->getPosition()) {
    	leftScrollbar->setPosition(leftScrollbar->getPosition()-1);
    	adjustKids(True);
    }
}

void fancyList::rightDec(tuGadget*) {
    if (rightScrollbar->getLowerBound() != rightScrollbar->getPosition()) {
    	rightScrollbar->setPosition(rightScrollbar->getPosition()-1);
    	adjustKids(False);
    }
}


void fancyList::leftPageInc(tuGadget*) {
    if (leftScrollbar->getUpperBound() != leftScrollbar->getPosition()) {
    	leftScrollbar->setPosition(leftScrollbar->getPosition() +
    	    	    	    	   leftScrollbar->getWidth());
    	adjustKids(True);
    }
}

void fancyList::rightPageInc(tuGadget*) {
    if (rightScrollbar->getUpperBound() != rightScrollbar->getPosition()) {
    	rightScrollbar->setPosition(rightScrollbar->getPosition() +
    	    	    	    	    rightScrollbar->getWidth());
    	adjustKids(False);
    }
}

void fancyList::leftPageDec(tuGadget*) {
    if (leftScrollbar->getLowerBound() != leftScrollbar->getPosition()) {
    	leftScrollbar->setPosition(leftScrollbar->getPosition() -
    	    	    	    	   leftScrollbar->getWidth());
    	adjustKids(True);
    }
}

void fancyList::rightPageDec(tuGadget*) {
    if (rightScrollbar->getLowerBound() != rightScrollbar->getPosition()) {
    	rightScrollbar->setPosition(rightScrollbar->getPosition() -
    	    	    	    	    rightScrollbar->getWidth());
    	adjustKids(False);
    }
}


void fancyList::childevent(tuGadget*) {
    if (doLayoutOnly || grabber == NULL) return; // grabber can be NULL if
                                                 // we've been markDelete'd
                                                 // but a stray event has
                                                 // come through.

    XEvent* event = grabber->getLastEvent();
    
    switch(event->type) {
	case ButtonPress: {
	    if (event->xbutton.button != tuStyle::getSelectButton()) break;
	    if (first < 0) break;
	    tuBool doubleclick = window->checkForMultiClick() > 1;
	    tuGadget* child = findChildAtPoint(event->xbutton.x,
				event->xbutton.y);
	    if (child == NULL) break;
	    selstart = selend = children.find(child);
	    if (!child->getEnabled()) break;
	    if (child->getSelected()) {
		if (!doubleclick) {
		    child->setSelected(False);
		    child->render();
		    current = NULL;
		}
	    } else {
		if (current && !allowMultipleSelections) {
		    current->setSelected(False);
		}
		child->setSelected(True);
		current = child;
	    }
	    if (callback) callback->doit(this);
	    if (othercallback && doubleclick) {
		othercallback->doit(this);
	    }
	    break;
	} // ButtonPress

	case MotionNotify:
	case ButtonRelease: {
	    if (event->xbutton.button != tuStyle::getSelectButton()) break;
	    if (selstart < 0 || selend < 0 || !allowMultipleSelections) break;
	    
	    int y = event->xmotion.y;
	    if (y < 0 || y >= getHeight()) {
		if (scrollTimer == NULL) {
		    scrollTimer = new tuTimer(True);
		    scrollTimer->setDuration(tuStyle::getAutoRepeatRate());
		    tuCallBack* callback =
			new fancyListCallBack(this, &tuList::scrolltick);
		    scrollTimer->setCallBack(callback);
		}
		if (!scrollTimer->isTicking()) scrollTimer->start();
		scrollup = (y < 0);
		if (scrollup) y = 0;
		else y = getHeight() - 1;
	    } else {
		if (scrollTimer && scrollTimer->isTicking()) {
		    scrollTimer->stop();
		}
	    }
	    tuGadget* child = findChildAtPoint(event->xmotion.x, y);
	    int newp = selend;
	    if (child) newp = children.find(child);
	    if (newp != selend && newp >= 0) {
		tuWindow::Freeze();
		// This code could be a lot faster.  But at least it generates
		// the minimum X traffic... XXX
		tuGadgetList list;
		int first = TU_MIN(selstart, selend);
		int last = TU_MAX(selstart, selend);
		for (int i=first ; i<=last ; i++) {
		    child = children[i];
		    list.add(child);
		}
		selend = newp;
		first = TU_MIN(selstart, selend);
		last = TU_MAX(selstart, selend);
		for (i=first ; i<=last ; i++) {
		    child = children[i];
		    int w = list.find(child);
		    if (w >= 0) {
			list.remove(w);
		    } else {
			list.add(child);
		    }
		}
		for (i=0 ; i<list.getLength() ; i++) {
		    child = list[i];
		    if (child->getEnabled()) {
			child->setSelected(!child->getSelected());
		    }
		}
		tuWindow::Unfreeze();
	    }
	    if (event->type == ButtonRelease) {
		selstart = selend = -1;
		if (scrollTimer && scrollTimer->isTicking()) {
		    scrollTimer->stop();
		}
	    }
	    break;
	} // MotionNotify, ButtonRelease	
	
    } // switch
} // childevent


void fancyList::deselect() {
    if (current) {
    	current->setSelected(False);
    	current = NULL;
    }
} // deselect


static tuGadget* Unpickle(tuGadget* parent, char* instanceName,
			  char** /* properties */) {
    return new fancyList(parent, instanceName);
}

void fancyList::registerForPickling() {
    tuUnPickle::registerForPickling(resourceChain.className, &Unpickle, "");
}


void fancyList::clearAll() {
    current = NULL;
    children.removeAll();
    lastHeightCounted = 0;
    leftScrollbar->setPosition(0.0);
    rightScrollbar->setPosition(0.0);
    if (mapped) {
	/**** xxx don't need this if list has its own render ??? **/
	XFillRectangle(window->getDpy(), window->getWindow(),
    	    	getBackgroundGC()->getGC(window->getColorMap()),
    	    	xAbs, yAbs, getWidth(), getHeight());
	/*****/
	render();
    }
} // clearAll


void fancyList::goToEnd() {
    scrollbar->setPosition(scrollbar->getUpperBound());
    relayout();
} // goToEnd


int fancyList::writeToFile(FILE* fp) {
    dblLabel* kid;

    for (int i=0 ; i<children.getLength() ; i++) {
    	kid = (dblLabel*) children[i];
    	if (!kid->writeToFile(fp)) {
	    return -1;
	}
    }
    
    return 0;
} // writeToFile


// largely copied from tuTextField.c++
void fancyList::render() {
    //printf("fancyList::render\n");

    tuGC* dark = parent->getBackgroundShade(tuShader_dark);
    tuGC* darkest = parent->getBackgroundShade(tuShader_darkest);
    tuGC* veryLight = parent->getBackgroundShade(tuShader_veryLight);
    tuGC* light = parent->getBackgroundShade(tuShader_light);

    int x1;
    x1 = xAbs + getWidth()/2 - 10;

    XFillRectangle(window->getDpy(), window->getWindow(),
    	    	dark->getGC(window->getColorMap()),
    	    	x1, yAbs, 2, getHeight());

} // render

