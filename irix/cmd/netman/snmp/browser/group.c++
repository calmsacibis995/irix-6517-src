/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Mib Browser Group Level
 *
 *	$Revision: 1.22 $
 *	$Date: 1996/02/26 01:28:06 $
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

#include <string.h>
#include <time.h>
#include <sys/time.h>

// tinyui includes
#include <tuCallBack.h>
#include <tuColor.h>
#include <tuFont.h>
#include <tuGadget.h>
#include <tuLabelButton.h>
#include <tuMultiChoice.h>
#include <tuPalette.h>
#include <tuRadioButton.h>
#include <tuStyle.h>
#include "tuTextField.h"
#include "tuTextView.h"
#include <tuTopLevel.h>
#include <tuUnPickle.h>
#include <tuWindow.h>

// snmp includes
#include <asn1.h>
#include <snmp.h>
#include <pdu.h>
#include <oid.h>
#include <packet.h>
#include <message.h>

#include <dialog.h>

#include "helpWin.h"

// mib browser includes
#include "mibquery.h"
#include "parser.h"
#include "y.tab.h"

#include "browser.h"
#include "group.h"
#include "groupLayout.h"

#define INIT_STRING "\v"

extern Browser *browser;
extern HelpWin *helpTop;

tuDeclareCallBackClass(GroupCallBack, Group);
tuImplementCallBackClass(GroupCallBack, Group);

Group::Group(struct mibNode* n, const char* instanceName, bTopLevel* otherTopLevel)
: bTopLevel(n, instanceName, otherTopLevel)
{
    if (helpTop == NULL)
	helpCard = "/usr/lib/HelpCards/Browser.group.help";
    else
	helpCard = "/usr/lib/HelpCards/Browser.group.helpfile";

    node = n;

    char winName[80];
    strcpy(winName, instanceName);
    strcat(winName, " Subtree");

    setName(winName);
    setIconName(instanceName);

    TUREGISTERGADGETS
    
    // Unpickle the UI, and set up callbacks
    stuff = tuUnPickle::UnPickle(this, layoutstr);

    ((tuLabelMenuItem*)findGadget("save"))->
	setCallBack(new GroupCallBack(this, bTopLevel::save));
    ((tuLabelMenuItem*)findGadget("saveAs"))->
	setCallBack(new GroupCallBack(this, bTopLevel::saveAs));
    ((tuLabelMenuItem*)findGadget("describe"))->
	setCallBack(new GroupCallBack(this, bTopLevel::doDescribe));

    tuLabelMenuItem* getItem = ((tuLabelMenuItem*)findGadget("get"));
    getItem->setCallBack(new GroupCallBack(this, Group::get));
    
    tuLabelMenuItem* setItem = ((tuLabelMenuItem*)findGadget("set"));
    setItem->setCallBack(new GroupCallBack(this, Group::set));    

    if (node->parentname[0] == NULL) {
	tuGadget *g = findGadget("openParent");
	g->setEnabled(False);
    }
    
    if (parentBTL == NULL) {
	howOpen = 0;
    } else
	howOpen = parentBTL->getHowOpen();
    closeCheckBox = (tuCheckBox*)findGadget("closeCheckBox");
    closeCheckBox->setSelected(howOpen == 1);
 
    nodeField = (tuTextField*)findGadget("nodeField");
    nodeField->setText(browser->getNodeName());

    getTimeLbl = (tuLabel*)findGadget("getTimeLbl");
    setTimeLbl = (tuLabel*)findGadget("setTimeLbl");


    // figure out fullname and fullid (paths), and put those labels up
    doPathLabels();
        
    tuRowColumn* rc = (tuRowColumn*)findGadget("centralRC");
       
    // go through list of children of this node, and for each,
    // add appropriate stuff (label + label + label,
    //   label + label + field, or label + label + button)
    tuLabelButton* button;
    tuFrame* frame;
    tuTextField* field = NULL;
    tuLabel* label;
    char* childName;
    struct mibNode* child = node->child;
    int row = 0;
    valStruct *val, *lastVal;
    val = NULL;
    lastVal = NULL;
    tuBool noSubWindows = True;
    // note: use child name as instance name of buttons & name labels,
    // child subid as instance name of value labels/fields
    while (child != NULL) {
	childName = strdup(child->name);
	char childId[4];
	sprintf(childId, "%d", child->subid);
	char childBracedId[6];
	sprintf(childBracedId, "{%d}", child->subid);
	label = new tuLabel(rc, "", childBracedId);
	label = new tuLabel(rc, "", childName);
	
	if (child->table) {
	    noSubWindows = False;
	    button = new tuLabelButton(rc, childName, "Open table...");
	    button->setCallBack(new GroupCallBack(this, Group::groupSelected));
	    val = 0;
	} else if (child->syntax == 0) {
	    // it's a group
	    noSubWindows = False;
	    button = new tuLabelButton(rc, childName, "Open group...");
	    button->setCallBack(new GroupCallBack(this, Group::groupSelected));
	    if (child->child == NULL)
		button->setEnabled(False);
	    val = 0;
	} else {
	    frame = new tuFrame(rc, "");
	    frame->setFrameType(tuFrame_Innie);
	    field = new tuTextField(frame, childId);
	    // save a pointer to it so we can change text, save, etc.
	    val = new valStruct;
	    val->field = field;
	    switch (child->access) {
		case READ_ONLY:
		    //field->setEnabled(False);	
		    field->setResource("readOnly", "True");
		    field->setBackground(tuPalette::getReadOnlyBackground(getScreen()));
		    break;
		case READ_WRITE:
		case WRITE_ONLY:
		    // maybe set it to something we'll recognize,
		    // so don't 'set' if it didn't change?
		    field->setText(INIT_STRING);
		    field->setTerminatorCallBack
			(new GroupCallBack(this, Group::nextField));
		    break;
		case NOT_ACCESSIBLE:
		    field->setText("(not accessible)");
		    field->setEnabled(False);	
		    break;
	    } // switch
	}
	
	if (val != 0) {
	    val->name = childName;
	    val->access = child->access;
	    val->enumlist = child->enum_list;
	    val->oidType = child->syntax == OBJECTID;
	    val->next = NULL;
	    if (lastVal != NULL)
		lastVal->next = val;
	    else if (vals == NULL)
		vals = val;		
	    lastVal = val;
	}
	row++;
	child = child->sibling;
    } // while child != NULL
    
    // set focus to first writeable field, if any
    // do this by going to the 'next' after the last one
    if (field)
	nextField(field);
    else {
	// if no fields, then no vars to get or set
	setItem->setEnabled(False);
	getItem->setEnabled(False);
    }
    
    if (noSubWindows)
	closeCheckBox->setEnabled(False);
	
    tuMenu *navMenu = (tuMenu*) findGadget("navMenu");
    if (navMenu) {
	if (node->child) {
	    if (!buildMenu(navMenu, node, "")) {
		tuGadget* label = findGadget("navigateLabel");
		if (label)
		    label->setEnabled(False);
	    }
	} else {
	    tuGadget* label = findGadget("navigateLabel");
	    if (label)
		label->setEnabled(False);
	}
    }
} // Group


void
Group::getLayoutHints(tuLayoutHints* hints) {

    hints->flags = 0;
    tuLayoutHints stdHints;
    tuTopLevel::getLayoutHints(&stdHints);
 
    //make sure height is between min & max preferred heights
    int height = stdHints.prefHeight;
    height = TU_MAX(height, MIN_PREF_HEIGHT);
    height = TU_MIN(height, MAX_PREF_HEIGHT);

    hints->prefWidth = MAX_PREF_WIDTH; // xxx
    hints->prefHeight = height;
} // getLayoutHints



void
Group::groupSelected(tuGadget *g) {
    char* newName = g->getInstanceName();

    if (closeCheckBox->getSelected()) {
	howOpen = 1;
	this->node->group = NULL;
	this->markDelete();
    } else
	howOpen = 0;

    struct mibNode* newNode = FindByName(node, newName);
    
    openGroup(newNode);

} // groupSelected


void
Group::saveFile(FILE *fp) {
    // printf("Group::saveFile: %s\n", node->name); 
    writeSaveFile(fp);

    // now save any open child windows (tables)
    saveRecurse(node->child, fp);
} // saveFile

void
Group::writeSaveFile(FILE *fp) {
    // printf("Group::writeSaveFile: %s\n", node->name); 
    fprintf(fp, "\n");
    
    if (getTimeStr[0] != NULL)
	fprintf(fp, "As of: %s\n", getTimeStr);
    fprintf(fp, "Name:  %s\n", nameField->getText());
	    
    fprintf(fp, "OID:   %s\n", idField->getText());

    

    valStruct *val;
    int	maxLen = 0;
    for (val = vals; val != NULL; val = val->next) {
	maxLen = TU_MAX(strlen(val->name), maxLen);
    }

    for (val = vals; val != NULL; val = val->next) {
	char* oidString = val->field->getInstanceName();
	switch (strlen(oidString)) {
	    case 1:
		fprintf(fp, "     {%s} %s",  oidString, val->name);
		break;
	    case 2:
		fprintf(fp, "    {%s} %s",  oidString, val->name);
		break;
	    case 3:
		fprintf(fp, "   {%s} %s",  oidString, val->name);
		break;
	    default:
		fprintf(fp, "   {%s} %s",  oidString, val->name);
	} // switch

	// now pad with blanks.
	int deltaLen = maxLen - strlen(val->name);
	for (int i = 0; i < deltaLen; i++)
	    fprintf(fp, " ");
	fprintf(fp, " : %s\n",  val->field->getText());
    }
    
} // writeSaveFile


// look through the "vals" linked list for the field with
// this instance name
tuTextField*
Group::findValField(char* fieldInstName) {
    valStruct *val;
    for (val = vals; val != NULL; val = val->next) {
	if (!strcmp(val->field->getInstanceName(), fieldInstName))
	    return val->field;
    }
    
    return NULL;

} // findValField

// look through the "vals" linked list for the valStruct with a field
// with this instance name
valStruct*
Group::findVal(char* fieldInstName) {
    valStruct *val;
    for (val = vals; val != NULL; val = val->next) {
	if (!strcmp(val->field->getInstanceName(), fieldInstName))
	    return val;
    }
    
    return NULL;
} // findVal


// look through the "vals" linked list for the valStruct with this gadget
valStruct*
Group::findVal(tuTextField* field) {
    valStruct *val;
    for (val = vals; val != NULL; val = val->next) {
	if (val->field == field)
	    return val;
    }
    
    return NULL;
} // findVal

// if user types CR in a field, move to the next writable field
void
Group::nextField(tuGadget *gadget) {
    valStruct* initVal = findVal((tuTextField*)gadget);
    valStruct* val = initVal;
    
    if (!initVal)
	return;

    do {
	// wrap around the list if we hit the end;
	// we'll at least stop back on the same one
	if (val->next)
	    val = val->next;
	else
	    val = vals;
    } while (val->access != READ_WRITE && val->access != WRITE_ONLY &&
	     val != initVal);

    // now, only actually set the focus if the field is writeable
    // (it may not be if all are read-only and we called this from
    // the constructor)
    if (val->access == READ_WRITE || val->access == WRITE_ONLY) {
	requestFocus(val->field, tuFocus_explicit);
	val->field->movePointToEnd(False);
    }
} // nextField

void
Group::get(tuGadget*) {
    // printf("Group::get (%s, %s)\n", node->name, fullId);
    
    // set node field, in case node changed since when we opened this window
    nodeField->setText(browser->getNodeName());

    // xxx at a meeting several months ago, people said to display the
    // time the 'get' was requested, rather than the time the answer was
    // returned. Is this really right?
    struct timeval tval;
    gettimeofday(&tval, 0);
    long clockSec = tval.tv_sec;
    struct tm* timeStruct;
    timeStruct = localtime(&clockSec);
    ascftime(getTimeStr, "%c", timeStruct);

    getTimeLbl->setText("In Progress");

    varBindList vl;
    int fullLength = strlen(fullId);
    char* oidString = new char[fullLength + 6];
    strcpy(oidString, fullId);
    char subIdString[6];

    for (valStruct* val = vals; val != NULL; val = val->next) {
	if (val->access == WRITE_ONLY || val->access == NOT_ACCESSIBLE)
	    continue;
	    
	// figure out the full oid for this child
	oidString[fullLength] = NULL;
	sprintf(subIdString, ".%s.0", val->field->getInstanceName());
	strcat(oidString, subIdString);
	
	// printf("    will ask to get oid = %s\n", oidString);
	varBind* v = new varBind(oidString);
	vl.appendVar(v);
    }
    
    // xxx delete [] oidString; ???
    int result;
    result = browser->get(this, &vl);

} // get


void
Group::set(tuGadget*) {
    // printf("Group::set (%s)\n", node->name);

    // set node field, in case node changed since when we opened this window
    nodeField->setText(browser->getNodeName());

    // xxx see question above about when to figure time
    struct timeval tval;
    gettimeofday(&tval, 0);
    long clockSec = tval.tv_sec;
    struct tm* timeStruct;
    timeStruct = localtime(&clockSec);
    ascftime(setTimeStr, "%c", timeStruct);

    setTimeLbl->setText("In Progress");

    varBindList vl;
    int fullLength = strlen(fullId);
    char* oidString = new char[fullLength + 6];
    strcpy(oidString, fullId);
    char subIdString[6];
    tuBool anythingToSet = False;

    for (valStruct* val = vals; val != NULL; val = val->next) {
	if (val->access == READ_ONLY || val->access == NOT_ACCESSIBLE ||
	    !strcmp(val->field->getText(), INIT_STRING))
	    continue;
	
	// figure out the full oid for this child
	oidString[fullLength] = NULL;
	sprintf(subIdString, ".%s.0", val->field->getInstanceName());
	strcat(oidString, subIdString);
	// printf("    will ask to set oid = %s\n", oidString);
	
	char* valString = strdup(val->field->getText());
	
	// if this oid is of enumerated type, possibly replace string w/ int
	varBind* v;
	
	if (val->enumlist) {
	    int value = getEnumValue(val->enumlist, valString);
	    if (value == -1)
		v = makeVarBind(oidString, strdup(valString));
	    else {  
		delete [] valString; // xxx we didn't really need a copy
		v = makeVarBind(oidString, value);
	    }
	} else if (val->oidType && (valString[0] != '.')) {
	    // have to prepend '.' to valString,  if not already there
	    char* newValString = new char[strlen(valString)+2];
	    strcpy(newValString, ".");
	    strcat(newValString, valString);
	    v = makeVarBind(oidString, strdup(newValString));
	    // delete [] newValString;

	} else 
	    v = makeVarBind(oidString, strdup(valString));

	vl.appendVar(v);
	anythingToSet = True;
	// delete [] valString;
    }

    if (anythingToSet)
	browser->set(this, &vl);
    else {
	setTimeLbl->setText("");
	browser->getDialogBox()->warning("Nothing to set yet in this group.");
        browser->openDialogBox();
    }
} // set


void
Group::handleGetResponse(struct result* vbls) {
    
    varBindList* vl = vbls->goodResult.getNext();
    
    if (!vl) {
	getTimeLbl->setText("Error");
	handleErrs(vbls);
	return;
    }
    
    varBind* v;
    asnObjectIdentifier* aoi;
    asnObject* o;
    char* s;

    // xxx handle bad results,  too! xxx
    for (; vl != NULL; vl = vl->getNext()) {
	v = vl->getVar();
	if (!v) {
	    printf("var pointer is NIL!\n");
	    continue;
	}
	aoi = v->getObject();
	s = aoi->getString();
	// I should probably copy this so it's cleaner. problem with deleting, etc. xxx
	// printf("object: %s\n", s);
	// get rid of the ".0" at the end of the (long) oid
	s[strlen(s)-2] = NULL;
	// now get just the subid from it
	s = strrchr(s, '.') + 1;
	// printf("      : %s\n", s);
	
	// find the label or field that matches
	valStruct* val = findVal(s);
	tuTextField* field = NULL;
	// delete [] s;
	if (val)
	    field = val->field;
	if (field) {
	    o = v->getObjectValue();
	    s = o->getString();
	    // printf("value: %s\n", s);
	
	    // find out if this var is of an enum type, etc;
	    if (val->enumlist) {
		char* enumString = getEnumString(val->enumlist, atoi(s));
		if (enumString)
		    s = enumString;
	    }

	    field->setText(s);
	    // make sure it's shifted all the way to the left, so see beginning
	    field->movePointToHome(False);
	    // actually, always a textfield, so s/change valStruct?xxx
	}

	// delete [] s;
    }
    
    // doErrors(vbls);

    getTimeLbl->setText(getTimeStr);
    
    // xxx clear vbls??
} // handleGetResponse


void
Group::handleSetResponse(struct result* vbls) {
    varBindList* vl = vbls->goodResult.getNext();

    if (!vl) {
	setTimeLbl->setText("Error");
	handleErrs(vbls);
	return;
    }
    
    setTimeLbl->setText(setTimeStr);
    
    // xxx check if any errors; that's about all we can do here
    // doErrors(vbls);
 
    // xxx clear vbls??
} // handleSetResponse


void
Group::doErrors(struct result* vbls) {
    if (vbls->notfoundResult.getNext()) {
	doOneError(&vbls->notfoundResult, notfoundMsg);
    }   
    if (vbls->badvalueResult.getNext()) {
	doOneError(&vbls->badvalueResult, badvalueMsg);
    }   
    if (vbls->readonlyResult.getNext()) {
	doOneError(&vbls->readonlyResult, readonlyMsg);
    }   
    if (vbls->generrorResult.getNext()) {
	doOneError(&vbls->generrorResult, generrorMsg);
    }   
    if (vbls->timeexpiredResult.getNext()) {
	doOneError(&vbls->timeexpiredResult, timeexpiredMsg);
    }
} // doErrors

void
Group::doOneError(varBindList* vl, char* errMsg) {
    varBind* v;
    asnObjectIdentifier* aoi;
    char* s;
    for (vl = vl->getNext(); vl != NULL; vl = vl->getNext()) {
	v = vl->getVar();
	if (!v) continue;
	aoi = v->getObject();
	s = aoi->getString();
	// xxx see comments in handleGetResponse()
	s[strlen(s)-2] = NULL;
	s = strrchr(s, '.') + 1;
	tuTextField* field = findValField(s);
	if (field) {
	    field->setText(errMsg);
	    // xxx and red text, and italics??
	}
	// delete [] s; 
    }

} // doOneError
