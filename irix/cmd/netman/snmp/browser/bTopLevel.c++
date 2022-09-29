/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Mib Browser - common class for all 'tuTopLevel' windows
 *
 *	$Revision: 1.14 $
 *	$Date: 1996/02/26 01:28:05 $
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

#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>


// tinyui includes
#include <tuCallBack.h>
#include <tuCheckBox.h>
#include <tuFilePrompter.h>
#include <tuFont.h>
#include <tuFrame.h>
#include <tuLabel.h>
#include <tuLabelMenuItem.h>
#include <tuMenu.h>
#include <tuPalette.h>
#include <tuSeparator.h>
#include <tuStyle.h>
#include "tuTextField.h"
#include <tuWindow.h>

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
#include "bTopLevel.h"
#include "desc.h"
#include "group.h"
#include "messages.h"
#include "mibquery.h"
#include "parser.h"
#include "table.h"

extern "C" {
char *ether_aton(char *);
}


extern Browser *browser;
extern tuFilePrompter *prompter;

// this is used as magic 'path' to parent, in navigate menu
#define PARENT "parentNode"

tuDeclareCallBackClass(bTopCallBack, bTopLevel);
tuImplementCallBackClass(bTopCallBack, bTopLevel);


bTopLevel::bTopLevel(struct mibNode* n,
	   const char* instanceName, tuColorMap* cmap, tuResourceDB* db,
           char* progName, char* progClass, Window transientFor)
: tuTopLevel(instanceName, cmap, db, progName, progClass, transientFor)
{
    node = n;
    
    catchDeleteWindow(new tuFunctionCallBack(bTopLevel::closeIt, NULL));

}

bTopLevel::bTopLevel(struct mibNode* n,
	   const char* instanceName, bTopLevel* otherTopLevel,
           tuBool transientForOtherTopLevel)
: tuTopLevel(instanceName, (tuTopLevel*)otherTopLevel, transientForOtherTopLevel)
{
//    printf("bTopLevel(node: %s, instanceName: %s, otherTop: %s)\n",
//	n->name, instanceName, otherTopLevel->getInstanceName());

    node = n;
    
    catchDeleteWindow(new tuFunctionCallBack(bTopLevel::closeIt, NULL));

    parentBTL = otherTopLevel;
    closeCheckBox = NULL;

}



bTopLevel::~bTopLevel() {
    
}

void
bTopLevel::save(tuGadget *) {
    // printf("%s: bTopLevel save... \n", instanceName);
    char* lastFile = browser->getLastFile();
    if (lastFile) {
	struct stat statbuf;
	tuBool newFile = stat(lastFile, &statbuf);

	FILE *fp = fopen(lastFile, "a");
	if (!fp) {
	    browser->getDialogBox()->warning(errno, "Could not open %s", lastFile);
	    browser->openDialogBox();
	    return;
	}

	if (newFile)
	    fprintf(fp, "# data saved by Browser\n");
	else
	    fprintf(fp, "\n");
	    
	saveFile(fp);
	
	fclose(fp);
    } else {
	// no file name yet.
	browser->getDialogBox()->warning(Message[NOFILE_MSG]);
	browser->openDialogBox();
    }

} // save


void
bTopLevel::saveAs(tuGadget *) {
    // printf("%s: bTopLevel save as... \n", instanceName);
    assert(prompter != NULL);
    prompter->setCallBack(new bTopCallBack(this, bTopLevel::savePrompterFile));
    prompter->mapWithCancelUnderMouse();
} // saveAs

void
bTopLevel::savePrompterFile(tuGadget*) {
    assert(prompter != NULL);
    char* fileName = prompter->getSelectedPath();
    prompter->unmap();
    if (fileName) {
	struct stat statbuf;
	tuBool newFile = stat(fileName, &statbuf);

	FILE *fp = fopen(fileName, "a");
	if (!fp) {
	    browser->getDialogBox()->warning(errno, "Could not open %s", fileName);
	    browser->openDialogBox();
	    return;
	}
	
	browser->setLastFile(fileName);
	
	if (newFile)
	    fprintf(fp, "# data saved by Browser\n");
	else
	    fprintf(fp, "\n");
    
	saveFile(fp);
	
	fclose(fp);
    }
} // savePrompterFile

void
bTopLevel::saveRecurse(struct mibNode* n, FILE* fp) {
    bTopLevel* top;
    top = (bTopLevel*) n->group;
    if (top) {
	// printf("recurse - has a 'top': %s\n", top->getInstanceName());
	top->writeSaveFile(fp);
    }	

    if (n->child) {
	// printf("recurse child: %s\n", n->child->name);
	saveRecurse(n->child, fp);
    } 
    if (n->sibling) {
	// printf("recurse sibling: %s\n", n->sibling->name);
	saveRecurse(n->sibling, fp);
    }
} // saveRecurse

void
bTopLevel::saveFile(FILE* fp) {
    // printf("bTopLevel::save %s\n", instanceName);
    saveRecurse(node, fp);
} // save

void
bTopLevel::writeSaveFile(FILE* fp) {
    // printf("bTopLevel::writeSaveFile\n"); // xxx
} // writeSaveFile

// (recursively) build roll-over submenu for this group and its children
// (to allow user to walk the tree with menus)
tuBool
bTopLevel::buildMenu(tuMenu *parentMenu, mibNode *n, char* parentPath) {
    // for each child that's a group, make a labelMenuItem
    // with parentMenu as its parent, and if that group isn't empty,
    // make a submenu for it and call this recursively.
    struct mibNode* child = n->child;
    tuLabelMenuItem* newItem;
    tuMenu* newMenu = NULL;
    char* childName;
    char childID[4];
    char childPath[80]; // xxx length?
    tuBool somethingInMenu = False;
    
    while (child != NULL) {
	if (child->syntax == 0 || child->table) {
	    somethingInMenu = True;
	    childName = strdup(child->name);
	    sprintf(childID, "%d.", child->subid);
	    childPath[0] = NULL;
	    strcat(childPath, parentPath);
	    strcat(childPath, childID);
	    newItem = new tuLabelMenuItem(parentMenu, childPath, childName);
	    newItem->setCallBack(new bTopCallBack(this, bTopLevel::menuSelected));
	    if (child->child == NULL) {
		newItem->setEnabled(False);
	    } else {
		// make a newMenu if we don't have an unused one already 
		if (!newMenu)
		    newMenu = new tuMenu(this, "", NULL);
    		if (buildMenu(newMenu, child, childPath)) {
		    newItem->setSubMenu(newMenu);
		    newMenu = NULL;
		}
	    }
	}
	child = child->sibling;
    }

    if (parentPath[0] == NULL) {
	// group & table call this function with the last arg an empty string.
	// There, we want to add the *parent* to the end of the menu.
	if (somethingInMenu)
	    new tuSeparator(parentMenu, "", True);
	somethingInMenu = True;

	newItem = new tuLabelMenuItem(parentMenu, PARENT, node->parentname);
	newItem->setCallBack(new bTopCallBack(this, bTopLevel::menuSelected));
    }
    return somethingInMenu;
} // buildMenu

// an item was selected from the roll-over menu.
// Open the appropriate window.
void
bTopLevel::menuSelected(tuGadget *g) {
    // printf("menuSelected - name: %s\n", g->getInstanceName());
    
    char* idPath = strdup(g->getInstanceName());
    struct mibNode* newNode;
    if (strcmp(idPath, PARENT))
	newNode = FindById(node, idPath);
    else
	newNode = node->parent;
	
    if (newNode == NULL) {

	browser->getDialogBox()->warning(Message[NODE_NOT_FOUND_MSG]);
	browser->openDialogBox();

	// printf("WHOOPS Didn't find %s below %s\n", idPath, node->name);
	return;
    }

    // the following is the same code used a couple other places. JUST HAVE IT ONCE!
    if (closeCheckBox && closeCheckBox->getSelected()) {
	howOpen = 1;
	this->node->group = NULL;
	this->markDelete();
    } else
	howOpen = 0;

    openGroup(newNode);

} // menuSelected


// given a node, map, pop, or create the corresponding group or table window
void
bTopLevel::openGroup(struct mibNode* newNode) {

    bTopLevel* bTpLvl = (bTopLevel*)newNode->group;
    if (bTpLvl) {
	if (bTpLvl->isMapped()) {
	    if (bTpLvl->getIconic())
		bTpLvl->setIconic(False);
	    // XMapWindow(bTpLvl->getDpy(), bTpLvl->getWindow()->getWindow());
	    bTpLvl->pop();
	} else
	    bTpLvl->map();
    } else {
	// make a new group or table for this
	beginLongOperation();
	char* name = newNode->name;
	
	if (newNode->table)
	    bTpLvl = (bTopLevel*) (new Table(newNode, name, this));
	else
	    bTpLvl = (bTopLevel*) (new Group(newNode, name, this));
	
	newNode->group = bTpLvl;

	bTpLvl->resizeToFit();
	bTpLvl->mapWithGadgetUnderMouse(bTpLvl);
	endLongOperation();
    }
    
} // openGroup


void
bTopLevel::doDescribe(tuGadget*) {
    Desc* desc = (Desc*) (node->describe);
    
    if (desc) {
	if (desc->isMapped()) {
	    if (desc->getIconic())
		desc->setIconic(False);
	    // XMapWindow(desc->getDpy(), desc->getWindow()->getWindow());
	    desc->pop();
	} else
	    desc->map();
	return;
    }

    beginLongOperation();
    desc = new Desc(node, instanceName, this);

    node->describe = desc;

    
    tuLayoutHints hints;
    desc->getLayoutHints(&hints);
    int height = TU_MIN(hints.prefHeight,  MAX_PREF_HEIGHT);
    desc->resize(hints.prefWidth, height);
    desc->mapWithGadgetUnderMouse(desc);
    endLongOperation();
    
} // doDescribe


void
bTopLevel::doPathLabels() {

    nameField = (tuTextField*)findGadget("nameField");
    idField = (tuTextField*)findGadget("idField");

    if (FindFullNameId(node, fullName, fullId)== 0) {
	nameField->setText(fullName);
	idField->setText(fullId);
	
	// we want the tail end of the name showing, so if fullName
	// is wider than the field, set the insertion point to
	// the right end minus the width of the field
	// BUT, WE WON"T BOTHER WITH THIS FOR NOW.....
	// nameField->setInsertionPoint(strlen(fullName) - xxx);
    }
} // doPathLabels



// close a node's describe window, and any subgroup windows.
void
bTopLevel::closeNodesKids(struct  mibNode* n) {
    if (n == NULL)
	return;
    // printf("closeNodesKids: %s\n", n->name);

    struct mibNode* child = n->child;
    bTopLevel* top;
    top = (bTopLevel*) n->describe;
    // close the node's describe window, if it exists
    if (top) {
	top->markDelete();
	n->describe = NULL;
    }
    while (child != NULL) {
	// close this child's group window, if it exists
	top = (bTopLevel*) child->group;
	if (top) {
	    top->markDelete();
	    child->group = NULL;
	}
	// continue to the node's children
	if (child->syntax == 0) {
	    // it's a group
	    closeNodesKids(child);
	} else if (child->table) {
	    // it's a table, which might have a describe window
	    top = (bTopLevel*) child->describe;
	    if (top) {
		top->markDelete();
		child->describe = NULL;
	    }
	}
	    
	child = child->sibling;
    }   

} // closeNodesKids

void
bTopLevel::closeKids(tuGadget *g, void*) {
    bTopLevel *bt = (bTopLevel*) (g->getTopLevel());
    closeNodesKids(bt->getNode());
} // closeKids


void
bTopLevel::closeIt(tuGadget* g, void*) {
    bTopLevel *bt = (bTopLevel*) (g->getTopLevel());
    browser->notifyBtlClosed(bt);
    struct mibNode* node = bt->getNode();
    if (node)
	node->group = NULL;
    bt->markDelete();
} // close


void
bTopLevel::openMain(tuGadget *, void*) {
    if (browser->getIconic())
	browser->setIconic(False);
    browser->pop();
} // openMain


 
void
bTopLevel::openParent(tuGadget *g, void*) {
 
    bTopLevel *bTpLvl = (bTopLevel*) (g->getTopLevel());
    struct mibNode* thisNode = bTpLvl->getNode();
    struct mibNode* parentNode = NULL;
    if (thisNode)
	parentNode = thisNode->parent;

    if (parentNode == NULL) {
	    // printf("WHOOPS - bTopLevel::openParent - couldn't find parent\n");
	    browser->getDialogBox()->warning(Message[PARENT_NOT_FOUND_MSG]);
            browser->openDialogBox();

	    // printf(" --- the menuItem should have been disabled!\n");
	    return;
    }

    
    bTpLvl->openGroup(parentNode);

} // openParent



varBind*
bTopLevel::makeVarBind(char*oidString, char* valString) {
    asnObject* value = 0;
    if (strlen(valString) == 0) {
	// value = new asnNull;
	// if it's blank, we'll assume it's a string, because
	// nothing else makes sense
	value = new asnOctetString(valString);
    } else {
	for (char* s = valString; *s != 0; s++) {
	    if (!isdigit(*s)) {
		if (*s != '.') {
		    if (*s != ':')
                        value = new asnOctetString(valString);
                    else
                        value = new asnOctetString(ether_aton(valString), 6);
                } else if (s == valString)
                    value = new asnObjectIdentifier(valString + 1);
                else
                    value = new snmpIPaddress(valString);
                break;
            } // if not digit
	} // for each char
    
	if (value == 0)
	    value = new asnInteger(atoi(valString));
    } // else

    varBind* v = new varBind(oidString, value);
    
    return v;
    
} // makeVarBind


varBind*
bTopLevel::makeVarBind(char*oidString, int val) {
    asnObject* value = new asnInteger(val);

    varBind* v = new varBind(oidString, value);
    
    return v;
    
} // makeVarBind

void
bTopLevel::handleGetResponse(struct result* ) {
    // printf("bTopLevel::handleGetResponse\n");
}

void
bTopLevel::handleSetResponse(struct result* ) {
    // printf("bTopLevel::handleSetResponse\n");
}

void
bTopLevel::handleErrs(struct result* vbls) {

    char* errString = NULL;

    if (vbls->notfoundResult.getNext())
	errString = Message[NO_SUCH_NAME_MSG];
    else if (vbls->badvalueResult.getNext())
	errString = Message[BAD_VALUE_MSG];
    else if (vbls->readonlyResult.getNext())
	errString = Message[READ_ONLY_OBJECT_MSG];
    else if (vbls->generrorResult.getNext())
	errString = Message[INTERNAL_GENERIC_MSG];
    else if (vbls->timeexpiredResult.getNext())
	errString = Message[NO_RESPONSE_MSG];
    else
	errString = Message[NO_VARS_MSG];

    browser->getDialogBox()->information(errString);
    browser->openDialogBox();

}
