/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Mib Browser Describer
 *
 *	$Revision: 1.11 $
 *	$Date: 1992/10/03 01:07:00 $
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


// tinyui includes
#include <tuCallBack.h>
#include <tuGadget.h>
#include <tuLabel.h>
#include <tuLabelButton.h>
#include <tuLabelMenuItem.h>
#include <tuRowColumn.h>
#include <tuTextField.h>
#include <tuTopLevel.h>
#include <tuUnPickle.h>


// mib browser includes
#include "parser.h"

#include "browser.h"
#include "desc.h"
#include "descLayout.h"

#include "helpWin.h"

extern Browser *browser;
extern HelpWin *helpTop;


tuDeclareCallBackClass(DescCallBack, Desc);
tuImplementCallBackClass(DescCallBack, Desc);


Desc::Desc(struct mibNode* n, const char* instanceName, bTopLevel* otherTopLevel)
: bTopLevel(n, instanceName, otherTopLevel)
{
    if (helpTop == NULL)
	helpCard = "/usr/lib/HelpCards/Browser.desc.help";
    else
	helpCard = "/usr/lib/HelpCards/Browser.desc.helpfile";

    if (n == NULL)
	return;
    
    node = n;
    char winName[80];
    strcpy(winName, "Description of ");


    struct mibNode* descNode = node; // start with this node
    // if this node is a table, the child is the 'entry', and the
    // child's children are the 'columns' of the table. We want to
    // show the description for those grandchildren as well as the child.
    tuBool doKids = False;
    tuBool doGrandKids = False;
    tuBool doOnlyThis = False;
    strcat(winName, node->name);
    if (node->syntax == 0) { // a group
	doKids = True;
	strcat(winName, " Subtree");
    } else if (node->table) {
	doKids = True;
	doGrandKids = True;
	strcat(winName, " Table");
    } else {
	doOnlyThis = True;
    }
    

    
    setName(winName);
    setIconName(winName);

    TUREGISTERGADGETS
   
    // unpickle the UI, and set up callbacks
    stuff = tuUnPickle::UnPickle(this, layoutstr);

    ((tuLabelMenuItem*)findGadget("save"))->setCallBack(new DescCallBack(this, bTopLevel::save));
    ((tuLabelMenuItem*)findGadget("saveAs"))->setCallBack(new DescCallBack(this, bTopLevel::saveAs));

    DescCallBack* closeCB = new DescCallBack(this, Desc::closeIt);
    ((tuLabelMenuItem*)findGadget("close"))->setCallBack(closeCB);
    catchDeleteWindow(closeCB);


    doPathLabels();

    tuRowColumn* rc = (tuRowColumn*)findGadget("centralRC");

    // for this node & then go through list of children of this node
    // and display the name and description of each.
    char* childName;
    char *whole, *line;
    tuLabel* label;
    int row = 0;
	
    while (descNode != NULL) {
	childName = strdup(descNode->name);

	// break the description into pieces (at newlines)
	// and make a label for each. Put them all in the rowColumn
	label = new tuLabel(rc, "", childName);
	rc->place(label, row++, 0);
	
	whole = strdup(descNode->descr);
	line = strtok(whole, "\n");
	char firstLine[80];
	// xxx this first line needs 22 blanks in front to line up w/others!
	if (line == NULL) {
	    line = "                      no description available in MIB";
	} else {
	    strcpy(firstLine, "                      ");
	    strcat(firstLine, line);
	    line = firstLine;
	}
	while (line != NULL) {
	    label = new tuLabel(rc, "", line);
	    rc->place(label, row++, 0);
	    line = strtok(NULL, "\n");
	}

//	printf("%s:\t%s\n\n", childName, descNode->descr);

	if (descNode->enum_list) {
	    sprintf(firstLine,
		    "                      Possible values and meanings:");
	    label = new tuLabel(rc, "", firstLine);
	    rc->place(label, row++, 0);
	    for (struct enumList* enumItem = descNode->enum_list;
		 enumItem != NULL; enumItem = enumItem->next) {
		sprintf(firstLine,
		        "                      %d : %s",
			 enumItem->value, enumItem->name);
		label = new tuLabel(rc, "", firstLine);
		rc->place(label, row++, 0);	    
	    }
	    // printf("\n%s enum list: \n", descNode->name);
	    // print_enumlist(descNode->enum_list);
	}
	
	delete [] whole;
	delete [] childName;	

	if (doKids) {
	    descNode = descNode->child;
	    doKids = False;
	} else if (doGrandKids) {
	    descNode = descNode->child;
	    doGrandKids = False;
	} else if (doOnlyThis) {
	    break;
	} else
	    descNode = descNode->sibling;
    }  
} // Desc


void
Desc::saveFile(FILE * fp) {
    // printf("Desc::saveFile\n");
    writeSaveFile(fp);
} // save

void
Desc::writeSaveFile(FILE *fp) {
    fprintf(fp, "\nDescription of:\n");

    fprintf(fp, "Name:  %s\n", nameField->getText());
	    
    fprintf(fp, "OID:   %s\n", idField->getText());

    // go through all the labels in the rowColumn, and get their text
    tuRowColumn* rc = (tuRowColumn*)findGadget("centralRC");
    int rows = rc->getNumRows();
    tuGadget* gadget;
    for (int r = 0; r < rows; r++) {
	gadget = rc->getChildAt(r, 0);
	if (gadget)
	    fprintf(fp, "%s\n", gadget->getText());
	    
    }
} // writeSaveFile


void
Desc::closeIt(tuGadget*) {
    if (node)
	node->describe = NULL;
    markDelete();
}
