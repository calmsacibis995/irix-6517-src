/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Mib Browser Individual Var getter/setter
 *
 *	$Revision: 1.20 $
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

#include <string.h>
#include <time.h>
#include <sys/time.h>

// tinyui includes
#include <tuCallBack.h>
#include <tuGadget.h>
#include <tuPalette.h>
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
#include "browser.h"
#include "indiv.h"
#include "indivLayout.h"
#include "messages.h"
#include "mibquery.h"
#include "parser.h"
#include "y.tab.h"

#include "helpWin.h"

extern Browser *browser;
extern HelpWin *helpTop;

tuBool firstIndiv = True;


tuDeclareCallBackClass(IndivCallBack, Indiv);
tuImplementCallBackClass(IndivCallBack, Indiv);


/********************/
// THIS SHOULD BE IN CHARU'S CODE W/ FINDBYNAME, ETC xxxxx
/*  this is like FindById, but it doesn't go farther than can find
*  (object is to ignore instance number)
*  When hits a table, just go as far as the column & stop.
*  table item oid == <table_id>.1.<column num> 
*/
struct mibNode *
FindTableItemById(struct mibNode *start_ptr, char *Id)
{
	static	char *id;
	struct mibNode *found_child;
	int  	i, id_int, total_id;
	int id_array[20];
	char IdCopy[200]; /* xxx vic - length??? */
	/* need to copy it; otherwise strtok trashes it.
	 * Also allocate it rather than do strdup,  so that it
	 * will clean itself up better.
	 */
	strcpy(IdCopy, Id);
	total_id = 0;


	for ( i = 0; i < 20; i++)
		id_array[i] = 0;

  	for ( (id = strtok(IdCopy, ".")); id != NULL; (id = strtok(NULL, ".")) )	
	{
		id_array[total_id] = atoi(id);
		total_id++;
	}
	

	/* Now search the tree */

	if (start_ptr == NULL ) {
	    found_child = mib;
	    id_int = 1;
	} else {
	    found_child = start_ptr;
	    id_int = 0;
	}

	tuBool hitTable = False;
	

	for (; id_int < total_id; id_int++) {
	    found_child = find_child(found_child, id_array[id_int]);
		
	    if (hitTable) {
		/* if looking for entry, we're there */
		if (id_int == total_id - 1)
		    break;

		/* otherwise, go one deeper and that's the one */
		found_child = find_child(found_child, id_array[id_int+1]);
		break;

	    }
	    
	    if (found_child && found_child->table) {
		hitTable = True;
	    }
	}

	return(found_child);

}


/********************/

Indiv::Indiv(const char* instanceName, bTopLevel* otherTopLevel)
: bTopLevel(0, instanceName, otherTopLevel)
{
    
    if (helpTop == NULL)
	helpCard = "/usr/lib/HelpCards/Browser.indiv.help";
    else
	helpCard = "/usr/lib/HelpCards/Browser.indiv.helpfile";

    node = NULL;
    
    setName("Variable");
    setIconName("Variable");

    
    // Set up callbacks
    if (firstIndiv) {
	firstIndiv = False;
	(new tuFunctionCallBack(Indiv::close, NULL))->registerName("__Indiv_close");
	(new IndivCallBack(this, Indiv::nextField))->
			registerName("__Indiv_nextField");
    }

    catchDeleteWindow(new tuFunctionCallBack(Indiv::close, NULL));

    (new IndivCallBack(this, Indiv::get))->
			registerName("__Indiv_get");
    (new IndivCallBack(this, Indiv::getNext))->
			registerName("__Indiv_getNext");
    (new IndivCallBack(this, Indiv::set))->
			registerName("__Indiv_set");

    TUREGISTERGADGETS;
    stuff = tuUnPickle::UnPickle(this, layoutstr);
    
    ((tuLabelMenuItem*)findGadget("describe"))->setCallBack(new IndivCallBack(this, Indiv::describe));
    ((tuLabelMenuItem*)findGadget("save"))->setCallBack(new IndivCallBack(this, bTopLevel::save));
    ((tuLabelMenuItem*)findGadget("saveAs"))->setCallBack(new IndivCallBack(this, bTopLevel::saveAs));
    
    nodeField  = (tuTextField*) findGadget("nodeField");
    oidField   = (tuTextField*) findGadget("oidField");
    nameField  = (tuTextField*) findGadget("nameField");
    valueField = (tuTextField*) findGadget("valueField");

    nodeField->setText(browser->getNodeName());

    requestFocus(oidField, tuFocus_explicit);

} // Indiv


void
Indiv::close(tuGadget *g, void*) {
    tuTopLevel *t = g->getTopLevel();
    t->unmap();
//    t->markDelete();
}


void
Indiv::describe(tuGadget *) {
    // need to find the node before can describe it
    if (handleFields()) {
	doDescribe((bTopLevel*)this);
    } else {
	// fprintf(stderr, "can't describe,  because don't have good node\n");
	browser->getDialogBox()->warning(Message[NODE_NOT_FOUND_MSG]);
        browser->openDialogBox();
    }
    
} // describe

// xxx when user types one field, the other clears and disables??? xxx
// read fields, find the matching node, etc.
// If the oid is specified, use it and set the name.
// Else, if the name is specified, use it and set the oid.
// Note: FindById trashes the id I send it; it leaves just the subid.
int
Indiv::handleFields() {

    NeedsInstance = False;
    WrongName = False;
    nodeField->setText(browser->getNodeName());
 
    strcpy(fullName, nameField->getText());
    strcpy(fullId,   oidField->getText());
    
    if (fullId[0] != NULL) {
	// get rid of trailing .0 if there, for purposes of finding node
	int length = strlen(fullId);
	if (fullId[length-2] == '.' && fullId[length-1] == '0')
	    fullId[length-2] = NULL;

	node = FindTableItemById(NULL, fullId);
	if (node && FindFullNameId(node, fullName, fullId) == 0) {
	    nameField->setText(fullName);
	    if (strcmp(fullId, oidField->getText()) == 0 ) 
		NeedsInstance = True;
	    return True;
	} else {
	    nameField->setText("(unknown)");
	}
	       
    } else {
        int length = strlen(fullName);
        if (fullName[length-2] == '.' && fullName[length-1] == '0')
            fullName[length-2] = NULL;

	node = FindByFullName(mib, fullName);
	if (node && FindFullNameId(node, fullName, fullId) == 0) {
	    nameField->setText(fullName);
	    oidField->setText(fullId);
	    if (node->parent && node->parent->parent &&
		    node->parent->parent->table)
		NeedsInstance = True;
	    return True;
	}
	WrongName = True;
    }
    
    return False;
    
} // handleFields

void
Indiv::nextField(tuGadget *field) {
    tuTextField* next = NULL;
    if (field == oidField)
	next = nameField;
    else if (field == nameField)
	next = valueField;
    else if (field == valueField)
	next = oidField;
    else
	requestFocus(field, tuFocus_none);
	
    if (next) {
	requestFocus(next, tuFocus_explicit);
	next->movePointToEnd(False);
    }
} // nextField


void
Indiv::saveFile(FILE *fp) {
    writeSaveFile(fp);
} // saveFile

void
Indiv::writeSaveFile(FILE *fp) {
    struct timeval tval;
    gettimeofday(&tval, 0);
    long clockSec = tval.tv_sec;
    struct tm* timeStruct;
    timeStruct = localtime(&clockSec);
    ascftime(getTimeStr, "%c", timeStruct);

    if (getTimeStr[0] != NULL)
	fprintf(fp, "As of: %s\n", getTimeStr);

    fprintf(fp, "OID:   %s\n", oidField->getText());
    fprintf(fp, "Name:  %s\n", nameField->getText());
    fprintf(fp, "Value: %s\n", valueField->getText());

} // writeSaveFile

// calls handleFields, but also tacks on ".0" if appropriate, etc.
char*
Indiv::getOidString() {
    char* oidString;
    
    if (!handleFields())  {
	if (WrongName) {
	    browser->getDialogBox()->warning(Message[NAME_NOT_IN_MIB_VIEW]);
	    browser->openDialogBox();
	    return NULL;
    	}

	oidString = strdup(oidField->getText());

    } else { 		// handleFields() == True

	// valueField->setText("");
    
	if (node->table) {
	    browser->getDialogBox()->warning(Message[TAB_NOT_VAR_MSG]);
	    browser->openDialogBox();
	    return NULL;
	}

	if (node->syntax == 0 ) {
	    browser->getDialogBox()->warning(Message[GROUP_NOT_VAR_MSG]);
	    browser->openDialogBox();
	    return NULL;
	}

	if (node->parent) {
	    if ( node->parent->table ) {
		browser->getDialogBox()->warning(Message[ENTRY_NOT_VAR_MSG]);
		browser->openDialogBox();
		return NULL;
	    }

	    if (node->parent->parent) {
		if (node->parent->parent->table && NeedsInstance) {
		    browser->getDialogBox()->warning(Message[VAR_IN_TAB_MSG]);
		    browser->openDialogBox();
		    return NULL;
		}
	    } // node->parent->parent
	} // node->parent

	const char* oidText = oidField->getText();
	oidString = new char[strlen(oidText) + 3];
	strcpy(oidString, oidText);

	if ( (node->parent) &&  (node->parent->syntax == 0) ) { // in group

	    // add  trailing .0 if not there

	    int length = strlen(oidText);
	    if (!(oidString[length-2] == '.' && oidString[length-1] == '0'))
		strcat(oidString, ".0"); // append ".0" 

	}

    }  // end of handleFields() == True

    return oidString;
    
} // getOidString


void
Indiv::get(tuGadget*) {

    char* oidString = getOidString();
    if (oidString == NULL) {
	valueField->setText("");
	return;
    }
 
    varBind* v = new varBind(oidString);
    // build varBindList;
    varBindList vl;
    vl.appendVar(v);


    browser->get(this, &vl);


} // get

void
Indiv::getNext(tuGadget*) {
    if (!handleFields()) {
	browser->getDialogBox()->warning(Message[NO_SUCH_NAME_MSG]);
        browser->openDialogBox();
	return;
    }
    valueField->setText("");

    /*
    printf("Indiv::getNext\n");
    printf("  name: %s\n", nameField->getText());
    printf("  oid: %s\n", oidField->getText());
    */
    
    // build varBindList;
    varBindList vl;
    const char* oidText = oidField->getText();
    char* oidString = new char[strlen(oidText) + 2];
    strcpy(oidString, oidText);

    varBind* v = new varBind(oidString);

    vl.appendVar(v);

    // maybe put the rest in bTopLevel::getNext, and call that (so all can share) ?xxx
    int result;
    result = browser->getNext(this, &vl);
    // printf("get next returned: %d: ", result);

} // getNext

void
Indiv::set(tuGadget*) {

    char* oidString = getOidString();
    if (oidString == NULL)
	return;
        
    char* valString = strdup(valueField->getText());

    // xxxx node was already found by handleFields...
    node = FindTableItemById(NULL, oidString);

    
    varBindList vl;
    varBind* v;
     // find out if this var is of an enum type, etc;
    if (node && node->enum_list) {
	int value = getEnumValue(node->enum_list, valString);
	if (value == -1)
	    v = makeVarBind(oidString, strdup(valString));
	else {  
	    delete [] valString; // xxx we didn't really need a copy
	    v = makeVarBind(oidString, value);
	}
	
    } else if (node && (node->syntax == OBJECTID) && (valString[0] != '.')) {
	    // have to prepend '.' to valString,  if not already there
	    char* newValString = new char[strlen(valString)+2];
	    strcpy(newValString, ".");
	    strcat(newValString, valString);
	    v = makeVarBind(oidString, strdup(newValString));
	    // delete [] newValString;

    } else
	v = makeVarBind(oidString, strdup(valString));


    vl.appendVar(v);
    // delete [] valString;
    
    int result;
    result = browser->set(this, &vl);
    // printf("set returned: %d: ", result);

} // set

 




void
Indiv::handleGetResponse(struct result* vbls) {    
    varBindList* vl = vbls->goodResult.getNext();
    
    if (!vl) {
	handleErrs(vbls);
	return;
    }
    
    varBind* v = vl->getVar();
    char* oidString = v->getObject()->getString();
    oidField->setText(oidString);
    // get rid of trailing .0 if there, for purposes of finding node
    int length = strlen(oidString);
    if (oidString[length-2] == '.' && oidString[length-1] == '0')
	oidString[length-2] = NULL;

    // this will let us find name for node even if it's in a table
    node = FindTableItemById(NULL, oidString);
    
    if (node && FindFullNameId(node, fullName, fullId) == 0) {
	nameField->setText(fullName);
    } else
	nameField->setText("(unknown)");

    delete [] oidString;
    
    char* value = v->getObjectValue()->getString();
    
    // find out if this var is of an enum type, etc;
    if (node && node->enum_list) {
	char* enumString = getEnumString(node->enum_list, atoi(value));
	if (enumString)
	    value = enumString;
    }

    valueField->setText(value);
    valueField->movePointToHome(False);

    // delete [] value;
} // handleGetResponse

void
Indiv::handleSetResponse(struct result* vbls) {
    varBindList* vl = vbls->goodResult.getNext();
    
    if (!vl) {
	handleErrs(vbls);
	return;
    }
    
} // handleSetResponse
