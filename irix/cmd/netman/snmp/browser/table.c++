/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Mib Browser Table
 *
 *	$Revision: 1.22 $
 *	$Date: 1996/02/26 01:28:07 $
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

#include <bstring.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

// tinyui includes
#include <tuCallBack.h>
#include <tuGadget.h>
#include <tuPalette.h>
#include "tuTextField.h"
#include "tuTextView.h"
#include <tuTopLevel.h>
#include <tuUnPickle.h>


// snmp includes
#include <asn1.h>
#include <snmp.h>
#include <pdu.h>
#include <oid.h>
#include <packet.h>
#include <message.h>

#include <dialog.h>

// mib browser includes
#include "mibquery.h"
#include "parser.h"
#include "y.tab.h"

#include "browser.h"
#include "messages.h"
#include "table.h"
#include "tableLayout.h"
// xxx #include "unpickle.h"

#include "helpWin.h"

extern Browser *browser;
extern HelpWin *helpTop;

// how long the string of all concatenated indices for a table row can be
#define MAX_INDICES_LEN 200

tuDeclareCallBackClass(TableCallBack, Table);
tuImplementCallBackClass(TableCallBack, Table);


Table::Table(struct mibNode* n, const char* instanceName, bTopLevel* otherTopLevel)
: bTopLevel(n, instanceName, otherTopLevel)
{

    if (helpTop == NULL)
	helpCard = "/usr/lib/HelpCards/Browser.table.help";
    else
	helpCard = "/usr/lib/HelpCards/Browser.table.helpfile";

    node = n;

    char winName[80];
    strcpy(winName, instanceName);
    strcat(winName, " Table");

    setName(winName);
    setIconName(instanceName);

    bzero(frames, sizeof(frames));
    bzero(fields, sizeof(fields));
    endLabel = NULL;
    
    struct mibNode* child = node->child;
    highCol = 0;
    highRow = 1; // 0 is names, 1 is indices
    maxIndexLength = 0;
    for (int r = 0; r < MAX_ROWS; r++)
	for (int c = 0; c < MAX_COLS; c++)
	    edited[r][c] = False;

    TUREGISTERGADGETS;
   
    // Unpickle the UI, and set up callbacks
    stuff = tuUnPickle::UnPickle(this, layoutstr);

    ((tuLabelMenuItem*)findGadget("describe"))->
	setCallBack(new TableCallBack(this, bTopLevel::doDescribe));
    ((tuLabelMenuItem*)findGadget("save"))->
	setCallBack(new TableCallBack(this, bTopLevel::save));
    ((tuLabelMenuItem*)findGadget("saveAs"))->
	setCallBack(new TableCallBack(this, bTopLevel::saveAs));
	
    ((tuLabelMenuItem*)findGadget("clear"))->
	setCallBack(new TableCallBack(this, Table::clearTable));
    ((tuLabelMenuItem*)findGadget("getNext"))->
	setCallBack(new TableCallBack(this, Table::getNext));
    ((tuLabelMenuItem*)findGadget("set"))->
	setCallBack(new TableCallBack(this, Table::set));
    ((tuLabelMenuItem*)findGadget("set"))->setEnabled(False);

    nodeField = (tuTextField*)findGadget("nodeField");
    nodeField->setText(browser->getNodeName());

    getTimeLbl = (tuLabel*)findGadget("getTimeLbl");
    setTimeLbl = (tuLabel*)findGadget("setTimeLbl");

    doPathLabels();
    
    centralRC = (tuRowColumn*)findGadget("centralRC");
    char* childName;
   
    child = child->child;
    char tmpStr[5];
    tuBool anIndex = False;
    tuLabel* label = NULL;
    
    while (child != NULL) {
	childName = strdup(child->name); // xxx shouldn't need strdup
	label = new tuLabel(centralRC, " ", childName);
	centralRC->place(label, 0, highCol);
	colLabels[highCol] = label;
	if (child->index != 0) {
	    anIndex = True;
	    sprintf(tmpStr, "(index %d)", child->index);
	    label = new tuLabel(centralRC, " ", tmpStr);
	    centralRC->place(label, 1, highCol);
	}
	
	colAccess[highCol] = child->access;
	colIndex[highCol] = child->index;
	colEnumList[highCol] = child->enum_list;
	colOidType[highCol] = child->syntax == OBJECTID;
	highCol++;
	child = child->sibling;
    }
    
    if (!anIndex) {
	// if no column is an index (weird, but it seems to happen sometimes)
	// put an empty label in row 1 just to fix the height of the row.
	label = new tuLabel(centralRC, "", "");
	centralRC->place(label, 1, 0);
	
    }

    highCol--;


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

    
} // Table

Table::~Table() {
    // xxx clean up memory!!!
    // especially *oids[][] (if I still have them), *indexString[][],etc
} // ~Table

void
Table::getLayoutHints(tuLayoutHints* hints) {
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


// if user types CR in a field, move to the next writable field
void
Table::nextField(tuGadget *gadget) {
    // look at other columns, then other rows, for 'next' writeable field
    // (writeable column, && row > 1 && not 'endOfTable')
    
    // find where this field is
    int row, col;
    tuBool foundIt = False;
    for (row = 2; row <= highRow; row++) {
	for (col = 0; col <= highCol; col++) {
	    if (fields[row][col] == gadget) {
		foundIt = True;
		break;
	    }
	}
	if (foundIt) break;
    }
    if (!foundIt) return;
    
    // now step through until find another writeable field
    do {
	if (col == highCol) {
	    col = 0;
	    if (row == highRow || (row == (highRow-1) && endLabel))
		row = 2; // 0 is names, 1 is indices
	    else
		row++;
	} else
	    col++;
    } while (colAccess[col] != READ_WRITE && colAccess[col] != WRITE_ONLY);

    requestFocus(fields[row][col], tuFocus_explicit);
    fields[row][col]->movePointToEnd(False);

} // nextField

void
Table::clearTable(tuGadget*) {
    // clear what we're already showing of the table
    if (endLabel) {
	endLabel->markDelete();
	endLabel = NULL;
    }
    
    // unmap the rowcolumn when we're deleting, so we don't have
    // to watch it relayout as each frame disappears, etc.
    centralRC->unmap();
    for (int r = 2; r <= highRow; r++)
	for (int c = 0; c <= highCol; c++) {
	    edited[r][c] = False;
	    if (frames[r][c]) {
		frames[r][c]->markDelete();
		frames[r][c] = NULL;
		fields[r][c] = NULL;
	    }
    }
    centralRC->map();
    
    highRow = 1;
    getTimeLbl->setText("");
    setTimeLbl->setText("");
    ((tuLabelMenuItem*)findGadget("set"))->setEnabled(False);

    // clean out lastVBL
    if (lastVBL.getNext())
	lastVBL.clear(0); // xxx destroy or not? default is destroy

} // clearTable

void
Table::getFirst(tuGadget*) {
    // printf("Table::getFirst\n");

    struct timeval tval;
    gettimeofday(&tval, 0);
    long clockSec = tval.tv_sec;
    struct tm* timeStruct;
    timeStruct = localtime(&clockSec);
    ascftime(getTimeStr, "%c", timeStruct);

    getTimeLbl->setText("In Progress");

    int result;

    // set node field, in case node changed since when we opened this window
    nodeField->setText(browser->getNodeName());
    
    clearTable(NULL);
    
    // 1st time, build varBindList of entry row (oid.1)
    varBindList vl;
    
    int fullLength = strlen(fullId);
    char oidString[100]; //xxx could be VERY long???
    strcpy(oidString, fullId);
    char subIdString[6];

    strcat(oidString, ".1"); // for 'entry'     xxx right?
    fullLength += 2;
     
    struct mibNode* child = node->child->child;
    while (child != NULL) {
	// figure out the full oid for this child
	oidString[fullLength] = NULL;
	char subIdString[6];
	sprintf(subIdString, ".%d", child->subid);
	strcat(oidString, subIdString);
	// printf("    will ask to get oid = %s\n", oidString);
	varBind* v = new varBind(oidString);
	vl.appendVar(v);

	child = child->sibling;
    }

    result = browser->getNext(this, &vl);


} // getFirst

void
Table::getNext(tuGadget*) {
    // printf("Table::getNext\n");

    // if we already hit the end of the table, nothing to do.
    if (endLabel)
	return;
	
    struct timeval tval;
    gettimeofday(&tval, 0);
    long clockSec = tval.tv_sec;
    struct tm* timeStruct;
    timeStruct = localtime(&clockSec);
    ascftime(getTimeStr, "%c", timeStruct);

    getTimeLbl->setText("In Progress");

    int result;

    // set node field, in case node changed since when we opened this window
    nodeField->setText(browser->getNodeName());

    if (lastVBL.getNext()) {
	// if already did a get before, resend the vbl we got last time	
	result = browser->getNext(this, &lastVBL);
    } else {
	getFirst(NULL);
    }
} // getNext


void
Table::set(tuGadget*) {
    // printf("Table::set\n");

    if (highRow < 2) {
	browser->getDialogBox()->warning(Message[GET_FIRST_MSG]);
        browser->openDialogBox();

	return;
    }

    struct timeval tval;
    gettimeofday(&tval, 0);
    long clockSec = tval.tv_sec;
    struct tm* timeStruct;
    timeStruct = localtime(&clockSec);
    ascftime(setTimeStr, "%c", timeStruct);

    setTimeLbl->setText("In Progress");


    // set node label, in case node changed since when we opened this window
    nodeField->setText(browser->getNodeName());

    varBindList vl;
    int fullLength = strlen(fullId);
    int subIdLength = maxIndexLength + 8;
    // oid string has: table id + '1' + col num + row index + any more?
    char* oidString = new char[fullLength + subIdLength];
    strcpy(oidString, fullId);
    char* subIdString = new char[subIdLength];
    tuBool anythingToSet = False;
    
    for (int r = 2; r <= highRow; r++) {
      // if we're on the last row, and there's an endLabel, we're done
      if (r == highRow && endLabel)
	  break;

      for (int c = 0; c <= highCol; c++) {
	if (colAccess[c] == READ_ONLY || colAccess[c] == NOT_ACCESSIBLE)
	    continue;

	// the oid should look like this: <table_fullId>.1.<col+1>.???.<index_1>.<index_2>.etc
	// figure out the full oid for this child
	oidString[fullLength] = NULL;
	sprintf(subIdString, ".1.%d%s", c+1, indexString[r]);
	strcat(oidString, subIdString);
	// printf("    will ask to set oid = %s\n", oidString);
	
	char* valString;
	if (fields[r][c])
	    valString = strdup(fields[r][c]->getText());
	else
	    continue; // in case the field is missing (var missing)

	// if this oid is of enumerated type, possibly replace string w/ int
	varBind* v;
	
	if (colEnumList[c]) {
	    int value = getEnumValue(colEnumList[c], valString);
	    if (value == -1)
		v = makeVarBind(oidString, strdup(valString));
	    else {  
		delete [] valString; // xxx we didn't really need a copy
		v = makeVarBind(oidString, value);
	    }
	} else if (colOidType[c] && (valString[0] != '.')) {
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

      } // each column
    } // each row
    
    if (anythingToSet)
	browser->set(this, &vl);
    else {
	setTimeLbl->setText("");
	browser->getDialogBox()->warning("Nothing to set in this table.");
        browser->openDialogBox();
    }
    
} // set

void
Table::saveFile(FILE *fp) {
    // printf("Table::saveFile: %s\n", node->name); // xxx need this fcn?
    writeSaveFile(fp);
} // saveFile

void
Table::writeSaveFile(FILE *fp) { 
    // printf("Table::writeSaveFile: %s\n", node->name);
    fprintf(fp, "\n");
    
    if (getTimeStr[0] != NULL)
	fprintf(fp, "As of: %s\n", getTimeStr);
    fprintf(fp, "Name:  %s\n", nameField->getText());
	    
    fprintf(fp, "OID:   %s\n", idField->getText());

    // find max width of each column
    int maxWidth[MAX_COLS];
    int row;
    for (int col = 0; col <= highCol; col++) {
	maxWidth[col] = strlen(colLabels[col]->getText());
	for (row = 2; row <= highRow; row++)
	    if (fields[row][col])
		maxWidth[col] = TU_MAX(maxWidth[col],
				   strlen(fields[row][col]->getText()));
	maxWidth[col] += 2; // (add two for space between columns)
    }

    int deltaLen;
    
    // print column headings
    fprintf(fp, "     "); // want space at beginning of line
    for (col = 0; col <= highCol; col++) {
	fprintf(fp, "%s",  colLabels[col]->getText());
	// now pad with blanks.
	deltaLen = maxWidth[col] - strlen(colLabels[col]->getText());
	for (int i = 0; i < deltaLen; i++)
	    fprintf(fp, " ");
    }
    fprintf(fp, "\n     "); // for beginning of next line
    
    // print each row
    // xxxxx (careful with wrapping -- probably wider than 80 chars)
    for (row = 2; row <= highRow; row++) {
	for (col = 0; col <= highCol; col++) {
	    if (fields[row][col])
		fprintf(fp, "%s", fields[row][col]->getText());
	    // now pad with blanks.
	    if (fields[row][col])
		deltaLen = maxWidth[col] - strlen(fields[row][col]->getText());
	    else
		deltaLen = maxWidth[col];
	    for (int i = 0; i < deltaLen; i++)
		fprintf(fp, " ");
	}
	fprintf(fp, "\n     "); // for beginning of next line
    }
    fprintf(fp, "\n");

} // writeSaveFile


void
Table::handleGetResponse(struct result* vbls) {
    // printf("Table::handleGetResponse\n");
    
    // clean out lastVBL
    if (lastVBL.getNext())
	lastVBL.clear(0); // xxx destroy or not? default is destroy

    varBindList* vl = vbls->goodResult.getNext();
    
    if (!vl) {
	getTimeLbl->setText("Error");
	handleErrs(vbls);
	return;
    }

    varBind* v;
    asnObjectIdentifier* aoi;
    asnObject* o;
    char *oid, *value;
    
    int row;
    int col;
    tuFrame* frame;
    tuTextField* field;
    tuBool postedEOT = False; // whether we already said 'end of table'
    
    // unmap while we're adding on to the table, so it comes up cleanly
    centralRC->unmap();


    while (vl != NULL) {
	v = vl->getVar();
	vl = vl->getNext();
	vbls->goodResult.removeVar(v, 0);

	if (!v) {
	    // printf("var pointer is NIL!\n");
	    continue;
	}
	
	aoi = v->getObject();
	oid = aoi->getString();
	// printf("table got back: oid: %s\n", oid);
	
	// I should probably copy this so it's cleaner. problem with deleting, etc. xxx

	// check to make sure it's really in the table
	int fullLength = strlen(fullId);
	if (strncmp(fullId, oid, fullLength)) {
	    if (postedEOT)
		continue;
	    postedEOT = True;
	    row = ++highRow;
	    col = 0;
	    endLabel = new tuLabel(centralRC, "", "End Of Table");
	    endLabel->setResource("backgroundShade", "tomato");
	    endLabel->setResource("font", "-*-helvetica-medium-o-normal--14-*-*-*-p-*-iso8859-1");
	    centralRC->place(endLabel, row, col);
	    endLabel->map();
	    
	    // clear out lastVBL and return? xxx
	    // no, maybe got vars out of order (possible?) so might still
	    // be some good ones. Just go to the next var.
	    continue;
	}

	// copy this var to lastVBL
	lastVBL.appendVar(v);
	
	// figure out row and column from the oid
	// the oid should look like this: <table_fullId>.1.<col+1>.???.<index_1>.<index_2>.etc
	char* colPtr = oid + fullLength + 3;
	col = atoi(colPtr) - 1;
	// printf("oid: %s, colPtr: %s, col: %d\n", oid, colPtr, col);

	o = v->getObjectValue();
	value = o->getString();
	// printf("value: %s\n", value);
	
	// find out if this var is of an enum type, etc;
	if (colEnumList[col]) {
	    char* enumString = getEnumString(colEnumList[col], atoi(value));
	    if (enumString)
		value = enumString;
	}
	// clear the value so we won't bother sending it when we do a getNext
	v->setObjectValue(NULL);
	
	// compare each existing row's index string to this oid
	char* thisIndexPtr = strchr(colPtr, '.');
	// printf("  index = %s\n", thisIndexPtr);
	for (int r = 2; r <= highRow; r++) {
	    if (!indexString[r])
		continue;
	    // printf("      row %d indexString = %s\n", r, indexString[r]);
	    int len = strlen(indexString[r]);
	    
	    if (strlen(thisIndexPtr) == len && !strcmp(thisIndexPtr, indexString[r])) {
		// printf("  MATCHES row %d\n", r);
		break;
	    }
	} // for r
	row = r;
	if (row > highRow) {
	    highRow = row;
	    if (indexString[row])
		delete [] indexString[row];
	    indexString[row] = strdup(thisIndexPtr);
	    maxIndexLength = TU_MAX(maxIndexLength, strlen(indexString[row]));
	}

	if (fields[row][col]) {
	    field = fields[row][col];
	    // printf("field[%d][%d] already exists; old val(%s), new val (%s)\n", 
	    // row, col, field->getText(), value); // xxx
	    field->setText(value);
	    field->map();
	    field->movePointToHome(False);
	} else {
	    frame = new tuFrame(centralRC, "");
	    frame->setFrameType(tuFrame_Innie);
	    frames[row][col] = frame;
	    centralRC->place(frame, row, col);
	    field = new tuTextField(frame, "");
	    fields[row][col] = field;
	    field->setText(value);

	    if (colAccess[col] == READ_ONLY) {
		//field->setEnabled(False);
		field->setResource("readOnly", "True");
		field->setBackground(tuPalette::getReadOnlyBackground(getScreen()));
	    } else if (colAccess[col] == NOT_ACCESSIBLE)
		field->setEnabled(False);
	    else
		field->setTerminatorCallBack
		    (new TableCallBack(this, Table::nextField));

    	    frame->map();
	    field->movePointToHome(False);
	} // new field

	edited[row][col] = False;
	
	if (colIndex[col] != 0) {
	    // this var is in an index column
	    // printf("oid: %s is an INDEX #%d,  in col #%d\n", oid, colIndex[col], col);	    
	}


	// delete [] oid; xxx
	// delete [] value;
    } // for vl
    
    centralRC->map();
    getTimeLbl->setText(getTimeStr);
    ((tuLabelMenuItem*)findGadget("set"))->setEnabled(True);

    /*****
    for (int r = 2; r <= highRow; r++) {
	printf("row %d:\n", r);
	for (int c = 0; c < MAX_COLS; c++)
	    if (colIndex[c] > 0)
		if (fields[r][c])
		    printf("   index %d = %s\n", colIndex[c], fields[r][c]->getText());
		else
		    printf("   index %d doesn't have a value\n", colIndex[c]);
	printf("IndexString: %s\n", indexString[r]);
    }
    *****/
    
} // handleGetResponse

void
Table::handleSetResponse(struct result* vbls) {
    // printf("Table::handleSetResponse\n");
    varBindList* vl = vbls->goodResult.getNext();

    if (!vl) {
	setTimeLbl->setText("Error");
	handleErrs(vbls);
	return;
    }

    setTimeLbl->setText(setTimeStr);
    
} // handleSetResponse

