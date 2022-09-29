// $Revision: 1.9 $
// $Date: 1996/02/26 01:27:46 $

#include "blankFiller.h"
#include "blankFillerLayout.h"

#include <bstring.h>
#include <stdio.h>
#include <string.h>

#include <tuCallBack.h>
#include <tuFont.h>
#include <tuFrame.h>
#include <tuLabel.h>
#include <tuLabelButton.h>
#include <tuList.h>
#include <tuRowColumn.h>
#include <tuStyle.h>
#include <tuTextField.h>
#include <tuTruncLabel.h>
#include <tuUnPickle.h>
#include <tuWindow.h>

#define HELP_CARD	    "/usr/lib/HelpCards/NetFilters.filler.help"
#define HELP_FILE	    "/usr/lib/HelpCards/NetFilters.filler.helpfile"

#define MIN_PREF_HEIGHT 250
#define MIN_PREF_WIDTH  400
#define MAX_PREF_HEIGHT 400
#define MAX_PREF_WIDTH  600


static char seps[] = " \t\n\"', ()[]";

// look for blank, paren, comma, etc.
// if open paren/brace/bracket, return pointer to char before it;
// if | & or = return pointer to char after it, unless that char is same,
//   in which case return pointer to char after _that_;
// otherwise, return pointer to character after the hit
char*
parseFcn(char* string) {
    char* hit = strpbrk(string, "(){}[]|&, \t");
    if (hit == NULL)
	return string + strlen(string);
	
    if (strchr("({[\"), ",  (int)(*hit))) {
	return hit + 1;
    } else  if (strchr("|&=", (int)(*hit))) {
	if (*hit == *(hit+1))
	    return hit + 2;
	else
	    return hit + 1;
    } else
	return hit + 1;
    
}

extern void archHelpFcn(tuGadget *g, void* card);

/***
tuGadget *
unPickleFromFile(tuGadget *parent, char *fn);
****/

tuDeclareCallBackClass(blankFillerCallBack, blankFiller);
tuImplementCallBackClass(blankFillerCallBack, blankFiller);

blankFiller::blankFiller(const char* instanceName,
			       tuTopLevel* othertoplevel,
			       tuBool transientForOtherTopLevel)
: tuTopLevel(instanceName, othertoplevel, transientForOtherTopLevel)
{
    initialize();
}

blankFiller::~blankFiller() {
}

void blankFiller::initialize() {
    static tuBool firsttime = True;
    if (firsttime) {
	firsttime = False;
	TUREGISTERGADGETS
	(new blankFillerCallBack(this, blankFiller::accept))
	    ->registerName("__blankFiller_accept");
	(new blankFillerCallBack(this, blankFiller::cancel))
	    ->registerName("__blankFiller_cancel");
	Display* disp = getDpy();
	if (strcmp(ServerVendor(disp), "Silicon Graphics") != 0)
	    // no Showcase help
	    (new tuFunctionCallBack(archHelpFcn, HELP_FILE))->registerName("__blankFiller_help");
	else
	    // Showcase help available
	    (new tuFunctionCallBack(archHelpFcn, HELP_CARD))->registerName("__blankFiller_help");
    }
    
    tuGadget* stuff = tuUnPickle::UnPickle(this, layoutstr);
//    tuGadget* stuff = unPickleFromFile(this,  "fillerPickle");

    varsRC = (tuRowColumn*) stuff->findGadget("varsRC");
    filterRC = (tuRowColumn*) stuff->findGadget("filterRC");
    commentRC = (tuRowColumn*) stuff->findGadget("commentRC");
    buttonsRC = (tuRowColumn*) stuff->findGadget("buttonsRC");
    catchDeleteWindow((new blankFillerCallBack(this, blankFiller::cancel)));

} // initialize


void
blankFiller::getLayoutHints(tuLayoutHints* hints) {

    hints->flags = 0;
    tuLayoutHints varsHints, filterHints, commentHints, buttonHints;
    varsRC->getLayoutHints(&varsHints);
    filterRC->getLayoutHints(&filterHints);
    commentRC->getLayoutHints(&commentHints);
    buttonsRC->getLayoutHints(&buttonHints);

// look at width of filter & comment labels, too! xxx

//    int width  = TU_MIN(varsHints.prefWidth + 60,  MAX_PREF_WIDTH);
//    width  = TU_MAX(width,  MIN_PREF_WIDTH);
//    hints->prefWidth = width;

    int height = TU_MIN(varsHints.prefHeight +
			filterHints.prefHeight +
			commentHints.prefHeight + 
			buttonHints.prefHeight + 30, MAX_PREF_HEIGHT);
    height = TU_MAX(height, MIN_PREF_HEIGHT);
    hints->prefWidth = MAX_PREF_WIDTH; // xxx
    hints->prefHeight = height;

}


void
blankFiller::map(tuBool propagate) {
    tuGadget::map(propagate);
    XFlush(window->getDpy());
}


void
blankFiller::setCallBack(tuCallBack* c) {
    if (callback) callback->markDelete();
    callback = c;
}


void
blankFiller::setFilter(char* varFilter) {
    // first, clean up old stuff
    if (filter) delete [] filter;
    for (int i = 0; i < numVars; i++) {
	if (varNames[i])  delete [] varNames[i];
	if (varLbls[i])   varLbls[i]->markDelete();
	if (varFrames[i]) varFrames[i]->markDelete();
    }
    for (i = 0; i < MAX_LINES; i++) {
	if (filterLbls[i])  filterLbls[i]->markDelete();
    }

    filter = strdup(varFilter);

    // check length, and see if it's too long to fit in the window.

    int availWidth = MAX_PREF_WIDTH - 100; // 100 isn't precisely correct. xxx
    tuFont* font = tuFont::getFont(getScreen(), tuStyle::labelFont);
    int numPieces;
    char** newStrings = breakStringToFit(filter, font, availWidth, &numPieces);
    for (i = 0; i < numPieces; i++) {
	filterLbls[i] = new tuLabel((tuGadget*)filterRC, "", newStrings[i]);
//xxx??	delete [] newStrings[i];
    }
    
    numVars = 0;
    char buf[80]; // xxx how long should it really be?

    int length;
    char* startPtr;
    startPtr = strchr(varFilter, '$');
    while (startPtr) {
    	length = strcspn(startPtr, seps);
    	strncpy(buf, startPtr, length);
	buf[length] = 0;
	// check to see if we've seen this one before
	for (i = 0; i < numVars; i++)
	    if (!strcmp(buf, varNames[i]))
		break;
	if (i == numVars) {
	    // it's new, so add it
    	    varNames[numVars] = strdup(buf);
	    varLbls[numVars] = new tuLabel((tuGadget*)varsRC, "", varNames[numVars]);
	    varFrames[numVars] = new tuFrame((tuGadget*)varsRC, "");
	    varFrames[numVars]->setFrameType(tuFrame_Innie);
	    varFields[numVars] = new tuTextField((tuGadget*)varFrames[numVars], "");
	    numVars++;
	}
	// find the next $, if any
	startPtr = strchr(startPtr + length, '$');
    }
varsRC->grow(numVars, 2); // xxx I shouldn't need this, right?

//    updateLayout();    
    filterRC->updateLayout();
    varsRC->updateLayout();
    resizeToFit();
    return;
} // setFilter


void
blankFiller::setComment(char* varComment) {
    if (comment) delete [] comment;
    for (int i = 0; i < MAX_LINES; i++) {
	if (commentLbls[i])  commentLbls[i]->markDelete();
    }
    
    comment = strdup(varComment);

    // check length, break into multiple labels if necsy
    int availWidth = MAX_PREF_WIDTH - 110; // 110 isn't precisely correct. xxx
    tuFont* font = tuFont::getFont(getScreen(), tuStyle::labelFont);
    int numPieces;
    char** newStrings = breakStringToFit(comment, font, availWidth, &numPieces);
    for (i = 0; i < numPieces; i++) {
	commentLbls[i] = new tuLabel((tuGadget*)commentRC, "", newStrings[i]);
//xxx??	delete [] newStrings[i];
    }
  
//    updateLayout();  
    commentRC->updateLayout();
    resizeToFit();
} // setComment


char**
blankFiller::breakStringToFit(char* string, tuFont* font, int availWidth, int* numP) {
    int numPieces = 0;
    char** newStrings = (char**)malloc(MAX_LINES * sizeof(char*));
    
    int labelWidth = font->getInkedWidth(string, strlen(string));
    while (labelWidth > availWidth) {
	char *occur1 = parseFcn(string);
	if (font->getInkedWidth(string, occur1 - string) > availWidth) {
	    // it's too long, even up to first blank, comma or right paren
printf("string is too long, even up to first blank, etc\n");
return NULL;
//xxx return now but indicate something's wrong?
//break at the limit,  no matter if it's in the middle of a word??
	}
	
	char *occur2;
	char *stringEnd = string + strlen(string);
	while (True) {
	    occur2 = parseFcn(occur1);
	    if (occur2 == stringEnd)
		break;
		
	    labelWidth = font->getInkedWidth(string, occur2 - string);
	    if (labelWidth > availWidth) {
		// occur2 is too long, so occur1 is our place to break
		break;
	    } else {
		// 2nd occurence of those chars is too short; look at next
		occur1 = occur2;
	    }
	} // while true
	
	// occur1 should be the place to break..
	int length = occur1 - string;
	char* piece = new char[length+1];
	strncpy(piece, string, length);
	piece[length+1] = NULL;
	newStrings[numPieces++] = piece;

	string = occur1;
	labelWidth = font->getInkedWidth(string, strlen(string));
    } // while too wide

    newStrings[numPieces++] = string;
    *numP = numPieces;
    return newStrings;
    
} // breakStringToFit


char*
blankFiller::replaceVars(char* oldString) {
    char stringBuf[300], varBuf[100]; // xxx how long should these really be?
    char *startPtr, *dollarPtr, *endPtr;
    int length, i;
    
    stringBuf[0] = 0;
    startPtr = oldString;
    endPtr = startPtr + strlen(oldString);
    while (startPtr && startPtr < endPtr) {
    	// find the next $, copy the string up to there
    	dollarPtr = strchr(startPtr, '$');
	if (!dollarPtr) {
	    // if we didn't find another $, copy the rest of the string & bail
	    strcat(stringBuf, startPtr);
	    break;
	}

	length = dollarPtr - startPtr;
	strncat(stringBuf, startPtr, length);
	
	// find the var name
    	length = strcspn(dollarPtr, seps);
    	strncpy(varBuf, dollarPtr, length);
	startPtr = dollarPtr + length;
	varBuf[length] = 0;
	for (i = 0; i < numVars; i++)
	    if (!strcmp(varBuf, varNames[i]))
		break;

	// if we didn't find a match, it's probably because the var
	// was in the comment and not in the filter.
	// If we didn't find a match, or if the user didn't fill in a value,
	// just copy the $VAR name; otherwise copy the value
	length = (i < numVars) ? varFields[i]->getLength() : 0;
	if (length == 0) {
	    strcat(stringBuf, varBuf);
	} else {
	    // copy the var's new value in place of the var's name
 	    strcat(stringBuf, varFields[i]->getText());
	}
    }
    length = strlen(stringBuf) + 1;
    char *newString = new char[length];
    strncpy(newString, stringBuf, length);
    return newString;
    
} // replaceVars
    
    
void
blankFiller::accept(tuGadget *) {
    canceled = False;

    filter = replaceVars(filter);
    comment = replaceVars(comment);
    
    if (callback) callback->doit(this);
}


void
blankFiller::cancel(tuGadget *) {
    unmap();
    canceled = True;
    if (callback) callback->doit(this);
}



void
blankFiller::processEvent(XEvent* ev) {
    Display* dpy;
    switch (ev->type) {
      case ClientMessage:
	dpy = window->getDpy();
	if ((ev->xclient.message_type ==
	     XInternAtom(dpy, "WM_PROTOCOLS", False)) &&
	    (ev->xclient.format == 32) &&
	    (ev->xclient.data.l[0] == 
	     XInternAtom(dpy, "WM_DELETE_WINDOW", False))) {
	    cancel((tuGadget*)NULL);
	    return;
	}
	break;
    }
    tuTopLevel::processEvent(ev);
}
