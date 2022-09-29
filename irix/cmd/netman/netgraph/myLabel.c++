// $Revision: 1.2 $
// $Date: 1992/10/16 02:18:42 $
#include <string.h>
#include <stdio.h>		// For sprintf

#include <tuGadget.h>
#include <tuLabel.h>
#include <tuFont.h>
#include <tuGC.h>
#include <tuShader.h>
#include <tuWindow.h>
#include <tuVisual.h>
#include <tuStyle.h>
#include <tuPalette.h>
#include <tuUnPickle.h>

#include "myLabel.h"

tuResourceChain myLabel::resourceChain = {
    "myLabel",
    tuLabel::resourceItems,
    &tuGadget::resourceChain
};


myLabel::myLabel(tuGadget* parent, const char* inst, const char* t)
: tuLabel(parent, inst, t) {
    resources = &resourceChain;

//    if (text)
//	delete [] text;

//    text = new char[21];

    textArray[0] = NULL;
    textArray[20] = NULL;
    
//    strncpy(text, t, 20);
    
}

myLabel::~myLabel() {
    if (text)
	delete [] text;
    if (textArray)
	delete [] textArray;
}


void myLabel::setText(const char* t) {
    if (strcmp(t, textArray) == 0) return;
    strncpy(textArray, t, 20);
    text = textArray;
// printf("myLabel::setText(%s), text:<%s>\n", t, text);

    markResourceChanged(&text);
//xxxxXXXXX
//    updateLayout();
    if (opened && mapped) {
	// Erase old text and cause an expose.
	window->addRect(this, bbox, True);
    }
}


static tuGadget* Unpickle(tuGadget* parent, char* instanceName,
			  char**		/*properties*/) {
    return new myLabel(parent, instanceName, "XxXxXx");
}

void myLabel::registerForPickling() {
    tuUnPickle::registerForPickling(resourceChain.className, &Unpickle, "");
}
