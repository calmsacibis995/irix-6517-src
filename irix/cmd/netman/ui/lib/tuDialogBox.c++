// $Revision: 1.1 $
// $Date: 1996/02/26 01:28:24 $

#include <strings.h>

#include "tuDialogBox.h"
#include "tuDialogBoxLayout.h"
#include "tuRowColumn.h"
#include "tuLabel.h"
#include "tuUnPickle.h"
#include "tuWindow.h"


tuDialogBox::tuDialogBox(const char* instanceName, tuColorMap* cmap,
			 tuResourceDB* db, char* progName,
			 char* progClass, Window transientFor)
: tuTopLevel(instanceName, cmap, db, progName, progClass, transientFor)
{
    initialize();
}

tuDialogBox::tuDialogBox(const char* instanceName,
			 tuTopLevel* othertoplevel,
			 tuBool transientForOtherTopLevel)
: tuTopLevel(instanceName, othertoplevel, transientForOtherTopLevel)
{
    initialize();
}

tuDialogBox::~tuDialogBox() {
}

void tuDialogBox::initialize(void) {
    static tuBool firsttime = True;
    if (firsttime) {
	firsttime = False;
	TUREGISTERGADGETS;
    }
    tuGadget* stuff = tuUnPickle::UnPickle(this, layoutstr);
    textContainer = (tuRowColumn *) stuff->findGadget("_text_container");
    buttonContainer = (tuRowColumn *) stuff->findGadget("_button_container");
    bitmap = (tuBitmapTile *) stuff->findGadget("_bitmap");
    stuff->map();
}

void tuDialogBox::map(tuBool propagate) {
    resizeToFit();
    tuTopLevel::map(propagate);
    XFlush(window->getDpy());
}

void tuDialogBox::setText(const char* s) {
    const char* list[2];
    list[0] = s;
    list[1] = 0;
    setText(list);
}

void tuDialogBox::setText(const char* list[]) {
    for (int i = textContainer->getNumExternalChildren(); i != 0; i--) {
	tuGadget* g = textContainer->getExternalChild(0);
	g->markDelete();
    }
    textContainer->grow(1,1);

    for (i = 0; list[i] != 0; i++) {
	tuLabel* l = new tuLabel(textContainer, "text", list[i]);
	l->map();
    }
    textContainer->updateLayout();
}
