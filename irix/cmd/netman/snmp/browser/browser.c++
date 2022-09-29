/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Mib Browser main window
 *
 *	$Revision: 1.23 $
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

#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <osfcn.h>    


// tinyui includes
#include <tuCallBack.h>
#include <tuGadget.h>
#include <tuGC.h>
#include <tuResourceDB.h>
#include <tuScreen.h>
#include <tuTimer.h>
#include <tuUnPickle.h>
#include <tuVisual.h>
#include <tuWindow.h>
#include <tuXExec.h>

// snmp includes
#include <asn1.h>
#include <snmp.h>
#include <pdu.h>
#include <oid.h>
#include <packet.h>
#include <message.h>

#include <dialog.h>
#include <license.h>

// mib browser includes
#include "group.h"
#include "indiv.h"
#include "messages.h"
#include "mibquery.h"
#include "parser.h"


#include "browser.h"
#include "browserLayout.h"
#include "desc.h"
#include "table.h"

#include "event.h" 
#include "helpWin.h"

#include "help.h"



extern Browser *browser;
mibQueryHandler *queryHandler;

// Events
EV_handler *eh_browser;

// Text help window
HelpWin* helpTop;

tuDeclareCallBackClass(BrowserCallBack, Browser);
tuImplementCallBackClass(BrowserCallBack, Browser);


Browser::Browser(const char* instanceName, tuColorMap* cmap,
	   tuResourceDB* rdb, AppResources* ar, 
           char* progName, char* progClass)
: bTopLevel(0, instanceName, cmap, rdb, progName, progClass)
{

    char* className = "Browser";
    setName("Browser");
    setIconName("Browser");
    howOpen = 0; // leave this open when open a child window
    indiv = NULL;
    lastFile = NULL;
   
    
    // Set up callbacks
    catchDeleteWindow(new BrowserCallBack(this, Browser::quit));
    catchQuitApp(new BrowserCallBack(this, Browser::quit));

    // these next four are used by all or many bTopLevel subclasses
    (new tuFunctionCallBack(bTopLevel::closeIt, NULL))->registerName("__bTop_close");
    (new tuFunctionCallBack(bTopLevel::closeKids, NULL))->registerName("__bTop_closeKids");
    (new tuFunctionCallBack(bTopLevel::openParent, NULL))->registerName("__bTop_openParent");
    (new tuFunctionCallBack(bTopLevel::openMain, NULL))->registerName("__bTop_openMain");
    (new tuFunctionCallBack(Browser::doHelp, NULL))->registerName("__bTop_help");

    (new BrowserCallBack(this, bTopLevel::save))->
			registerName("__Browser_save");
     (new BrowserCallBack(this, bTopLevel::saveAs))->
			registerName("__Browser_saveAs");
    (new BrowserCallBack(this, Browser::quit))->
			registerName("__Browser_quit");
    (new BrowserCallBack(this, Browser::closeKids))->
			registerName("__Browser_closeKids");
    (new BrowserCallBack(this, Browser::mibSelected))->
			registerName("__Browser_mibSelected");
    (new BrowserCallBack(this, Browser::nextField))->
			registerName("__Browser_nextField");

     
    TUREGISTERGADGETS
    stuff = tuUnPickle::UnPickle(this, layoutstr);

    nodeField	    = (tuTextField*) findGadget("nodeField");
    communityField  = (tuTextField*) findGadget("communityField");
    timeoutField    = (tuTextField*) findGadget("timeoutField");
    retriesField    = (tuTextField*) findGadget("retriesField");

    // set text of those fields to (default or resource) values
    if (strlen(ar->node) == 0) {
	char *localName = new char[64];
	gethostname(localName, 64);
	nodeField->setText(localName);
    } else
	nodeField->setText(ar->node);
    communityField->setText(ar->community);
    char tmpStr[20];
    sprintf(tmpStr, "%d", ar->timeout);
    timeoutField->setText(tmpStr);
    sprintf(tmpStr, "%d", ar->retries);
    retriesField->setText(tmpStr);
    
    
    // Create Dialog Box
    dialog = new DialogBox("Browser", this, False);
    progressDialog = new DialogBox("Browser", this, False);
    progressDialog->setCallBack(new BrowserCallBack(this, Browser::closeProgressDialogBox));
 
    // Create executive for events
    Display* display = rdb->getDpy();
    exec = new tuXExec(display);

    if (!ar->help) {
#ifndef UNLICENSED
	// Check license
	char *message;
	if (getLicense(className, &message) == 0) {
	    dialog->error(message);
	    dialog->setCallBack(new BrowserCallBack(this, Browser::quit));
	    openDialogBox();
	    return;
	}
	if (message != 0) {
	    licDialog = new DialogBox("Browser", this, False);
	    licDialog->setCallBack(new BrowserCallBack(this, Browser::closeLicDialogBox));
	    licDialog->information(message);
	    licDialog->map();
	    XFlush(getDpy());
	    delete message;
	}
#endif
	progressDialog->progress(Message[STARTING_MSG]);
	progressDialog->map();
	XFlush(getDpy());

	EV_handle evbrowser;
	static char *appName = "Browser";

	eh_browser = new EV_handler(&evbrowser, appName);
	EV_event start(NV_STARTUP);
	eh_browser->send(&start);

	// Create timer
	timer = new tuTimer;
	timer->setCallBack(new BrowserCallBack(this, Browser::init));
	timer->start(1); // xxx
    } // not help

    tuLayoutHints hints;
    Display* disp = getDpy();
    if (strcmp(ServerVendor(disp), "Silicon Graphics") != 0)
    {
	// setup no showcase help window
	helpTop = new HelpWin("browserHelp", (tuTopLevel*)this);
	helpTop->setName("Browser Help");
	helpTop->setIconName("Browser Help");
	helpTop->bind();
	helpTop->getLayoutHints(&hints);
	helpTop->resize(hints.prefWidth, hints.prefHeight);
	helpCard = "/usr/lib/HelpCards/Browser.main.helpfile";
    }
    else
    {
	helpTop = NULL;
	helpCard = "/usr/lib/HelpCards/Browser.main.help";
    }

} // Browser

void
Browser::init(tuGadget *) {

    beginLongOperation();

    mib = NULL;
    char *ErrMsg = ReadAndParse();

    if ( ErrMsg && ErrMsg[0] ) {	
	// put the error dialog box 
	dialog->error(ErrMsg);
	// Talk to Vic about this
	//CVG    dialog->setCallBack(new BrowserCallBack(this, Browser::quit));
	openDialogBox(); 
   }

    node = mib;	
       
    navMenu = (tuMenu*) findGadget("navMenu");
    buttonsRC = (tuRowColumn*) findGadget("buttonsRC");
    
    checkForChild("mib-2");
    checkForChild("enterprises");
    checkForChild("experimental");
    
    // For each mib menu item, create appropriate cascading menus below it
    // First find each item, before there's a whole tree to wade through.
    char* name[3];
    name[0] = "mib-2";
    name[1] = "enterprises";
    name[2] = "experimental";
    tuLabelMenuItem* item[3];
    for (int i = 0; i < 3; i++)
	item[i] = (tuLabelMenuItem*) (navMenu->findGadget(name[i]));

    tuMenu* newMenu;
    mibNode* child;
    char childPath[150]; // xxx length?

    for (i = 0; i < 3; i++) {
	// printf("processing %s\n", name[i]);
	child = FindByName(mib, name[i]);
	if (FindFullNameId(child, fullName, fullId) != 0)
	    break;
	// now modify the idPath - skip first part ("1.") so it's relative to here,
	// and add a '.' at end (so we can add more on for children)
	int len = strlen(fullId);
	fullId[len] = '.';
	fullId[len+1] = '\0';
	char* idPath = strchr(fullId, '.') + 1;
	// printf("fullName: %s, fullId: %s\n", fullName, fullId);
 	// check if groups under it; if so, create menu, else bail
	// xxx ??? check how/what?? xxx
	newMenu = new tuMenu(this, "", NULL);
	// build the menu
	if (buildMenu(newMenu, child, idPath))
	    item[i]->setSubMenu(newMenu);
	else
	    item[i]->setEnabled(False);
    }

    // set initial input focus
    requestFocus(communityField, tuFocus_explicit);
    communityField->movePointToEnd(False);


    // setup up the stuff for snmp transactions/ requests
    /*
     * XXX We must disable NIS temporarily, as mibQueryHandler makes an
     * XXX snmpMessageHandler,  which calls getservbyname.
     * XXX getservbyname is horrendously slow thanks
     * XXX to Sun's quadratic-growth lookup botch.
     */
    extern int _yp_disabled;
    _yp_disabled = 1;
    queryHandler = new mibQueryHandler();
    _yp_disabled = 0;
    
    highTransId = 0;
    trInfo* ti = new trInfo;
    ti->transId = highTransId;
    ti->btl = NULL;
    ti->next = NULL;
    transInfoList = ti;
   
    closeProgressDialogBox(NULL);

    endLongOperation();

} // init

void
Browser::run() {
    exec->loop();
}

void
Browser::nextField(tuGadget *field) {
    tuTextField* next = NULL;
    if (field == nodeField)
	next = communityField;
    else if (field == communityField)
	next = timeoutField;
    else if (field == timeoutField)
	next = retriesField;
    else if (field == retriesField)
	next = nodeField;
    else
	requestFocus(field, tuFocus_none);
	
    if (next) {
	requestFocus(next, tuFocus_explicit);
	next->movePointToEnd(False);
    }
} // nextField



// check if the given mib has any children.
// If it doesn't, disable the corresponding button AND menu item
void
Browser::checkForChild(char* name) {
    struct mibNode* newNode = FindByName(mib, name);
    if (newNode == NULL || newNode->child == NULL) {
	tuGadget *g = buttonsRC->findGadget(name);
	g->setEnabled(False);
	g = navMenu->findGadget(name);
	g->setEnabled(False);
    }
}



void
Browser::quit(tuGadget *) {

    HELPclose();

    EV_event shutdown(NV_SHUTDOWN);
    eh_browser->send(&shutdown);

    exit(0);
} // quit


void
Browser::closeKids(tuGadget *g) {
    if (indiv) {
	indiv->unmap();
    }
    // now close the other ("normal") child windows
    bTopLevel::closeKids(g, NULL);
}

// a group or a table calls this when it is about to be deleted.
// we need this in case there's an outstanding snmp request for that
// bTopLevel window.
void
Browser::notifyBtlClosed(bTopLevel *btl) {    
    // look up which transId's (0 or more) match this bTopLevel
    trInfo* tiPrevious;
    for (trInfo* ti = transInfoList; ti != NULL; ti = ti->next) {
	if (ti->btl == btl) {
	    // delete this one from transInfoList
	    if (ti == transInfoList)
		transInfoList = ti->next;
	    else
		tiPrevious->next = ti->next;
	    delete ti;
	} else
	    tiPrevious = ti;
    }

} // notifyBtlClosed

void
Browser::mibSelected(tuGadget *g) {
    if (!mib) {
	getDialogBox()->error(Message[NO_MIB_MSG]);
	openDialogBox();

	return;
    }

    char* name = g->getInstanceName();
    
    tuLayoutHints hints;

    if (!strcmp(name, "individual")) {
	if (indiv) {
	    if (indiv->isMapped()) {
		if (indiv->getIconic())
		    indiv->setIconic(False);
		// XMapWindow(indiv->getDpy(), indiv->getWindow()->getWindow());
		indiv->pop();
	    } else
		indiv->map();
	} else {
	    indiv = new Indiv(name, this);

	    indiv->getLayoutHints(&hints);
	    indiv->resize(hints.prefWidth, hints.prefHeight);
	    indiv->mapWithGadgetUnderMouse(indiv);
	}
	return;
    }
    
    struct mibNode* newNode = FindByName(mib, name);
    
    if (newNode == NULL) {
	// fprintf(stderr, "WHOOPS - nothing here for %s\n", name);
	// fprintf(stderr, "  button should have been disabled!\n");
	getDialogBox()->error(Message[NODE_NOT_FOUND_MSG]);
	openDialogBox();

	return;
    }
    
    openGroup(newNode);
    
} // mibSelected



void
Browser::doHelp(tuGadget* g, void*) {


    bTopLevel *bt = (bTopLevel*) (g->getTopLevel());
    char* helpCardName = bt->getHelpCardName();

    if (helpTop == NULL)
	HELPdisplay(helpCardName);
    else
    {
	int err = helpTop->setContent(helpCardName);
	if (err == 0)
	    helpTop->map();
    }


} // doHelp



void
Browser::usage() {
    dialog->information("usage: browser [-n node] [-c community]\n[-r retries] [-t timeout]");
    dialog->setCallBack(new BrowserCallBack(this, Browser::quit));
    dialog->resizeToFit();
    dialog->map();

}

void
Browser::readFields(char** hostp, char** commp,
		    int* timeoutp, int* retriesp) {

    char hostName[100]; // xxx length?
    char *hostPtr = hostName;
    char commName[100]; // xxx length?
    char *commPtr = commName;
    
    strcpy(hostName, nodeField->getText());
    strcpy(commName, communityField->getText());
    *timeoutp = atoi(timeoutField->getText());
    *retriesp = atoi(retriesField->getText());

    // get rid of leading and trailing blanks
    while (*hostPtr == ' ')
	(hostPtr)++;
    int len = strlen(hostPtr) - 1;
    while (hostPtr[len] == ' ')
	hostPtr[len--] = NULL;

    while (*commPtr == ' ')
	(commPtr)++;
    len = strlen(commPtr) - 1;
    while (commPtr[len] == ' ')
	commPtr[len--] = NULL;

    if (strlen(hostPtr) == 0) {
	char *name = new char[64];
	gethostname(name, 64);
	*hostp = name;
    } else
	*hostp = strdup(hostPtr);

    *commp = strdup(commPtr);
        
    // printf("readFields: node: %s, comm: %s, timeout: %d, retries: %d\n",
    //	*hostp, *commp, *timeoutp, *retriesp);
} // readFields

char*
Browser::getNodeName() {
    return strdup(nodeField->getText());
}

void
Browser::outputResult(int result) {
    // printf("  returned: %d: ", result);

    switch(result) {
	case MIBQUERY_ERR_encodeError:
	    getDialogBox()->warning(Message[ENCODE_MSG]);
	    openDialogBox();
	    // printf("encode error\n");
	    break;

	case MIBQUERY_ERR_decodeError:
	    getDialogBox()->warning(Message[DECODE_MSG]);
	    openDialogBox();
	    // printf("decode error\n");
	    break;

	case MIBQUERY_ERR_sendError:
	    getDialogBox()->warning(Message[NO_REMOTE_MSG]);
	    openDialogBox();
	    // printf("send error\n");
	    break;

	case MIBQUERY_ERR_recvError:
	    getDialogBox()->error(Message[RECEIVE_MSG]);
	    openDialogBox();
	    // printf("receive error\n");
	    break;
	    
	case MIBQUERY_ERR_timeout:
	    getDialogBox()->error(Message[TIMEOUT_MSG]);
	    openDialogBox();
	    // printf("timeout error\n");
	    break;
	    
	case MIBQUERY_ERR_mallocError:
	    getDialogBox()->error(Message[MALLOC_MSG]);
	    openDialogBox();
	    // printf("malloc error\n");
	    break;
	    
	case MIBQUERY_ERR_noError:
	    // printf("no error\n");
	    break;
	    
	default:
	    break;
    } // switch
    
} // outputResult

// This is called from an object (group, table or individual) to tell
// the mibQueryHandler to do an snmp get
int
Browser::get(bTopLevel* caller, varBindList* vbl) {

    // find end of transInfoList
    for (trInfo* ti = transInfoList;
	    ti != NULL && ti->btl != NULL;
	    ti = ti->next)
	;
    // put this transaction in the list
    ti->transId = ++highTransId;
    ti->btl = caller;
    trInfo* ti2 = new trInfo;
    ti->next = ti2;
    ti2->transId = 0;
    ti2->btl = NULL;
    ti2->next = NULL;
    

    // printf("transaction id: %d\n", highTransId);
    
    char* host;
    char* comm;
    int timeout;
    int retries;
    
    readFields(&host, &comm, &timeout, &retries);
    
    int result = queryHandler->get(host, comm,
				   timeout, retries, 
				   vbl, highTransId);
    
    outputResult(result);
    
    return result;
    
} // get


int
Browser::getNext(bTopLevel* caller, varBindList* vbl) {
    // find end of transInfoList
    for (trInfo* ti = transInfoList;
	    ti != NULL && ti->btl != NULL;
	    ti = ti->next)
	;
    // put this transaction in the list
    ti->transId = ++highTransId;
    ti->btl = caller;
    trInfo* ti2 = new trInfo;
    ti->next = ti2;
    ti2->transId = 0;
    ti2->btl = NULL;
    ti2->next = NULL;
    

    // printf("transaction id: %d\n", highTransId);
    
    char* host;
    char* comm;
    int timeout;
    int retries;
    
    readFields(&host, &comm, &timeout, &retries);
    
    int result = queryHandler->getNext(host, comm,
    				   timeout, retries, 
				   vbl, highTransId);
    
    outputResult(result);
    
    return result;
    
} // getNext

int
Browser::set(bTopLevel* caller, varBindList* vbl) {
    // printf("Browser::set\n");
    // find end of transInfoList
    for (trInfo* ti = transInfoList;
	    ti != NULL && ti->btl != NULL;
	    ti = ti->next)
	;
    // put this transaction in the list
    ti->transId = ++highTransId;
    ti->btl = caller;
    trInfo* ti2 = new trInfo;
    ti->next = ti2;
    ti2->transId = 0;
    ti2->btl = NULL;
    ti2->next = NULL;
    

    // printf("transaction id: %d\n", highTransId);

    char* host;
    char* comm;
    int timeout;
    int retries;
    
    readFields(&host, &comm, &timeout, &retries);
    
    int result = queryHandler->set(host, comm,
    				   timeout, retries, 
				   vbl, highTransId);
    
    outputResult(result);
    
    return result;
} // set


void
printResultList(varBindList* vl) {
    varBind* v;
    asnObjectIdentifier* aoi;
    asnObject* o;
    char* s;
    for (vl = vl->getNext(); vl != NULL; vl = vl->getNext()) {
	v = vl->getVar();
	if (!v) {
	    // printf("var pointer is NIL!\n");
	    continue;
	}
	aoi = v->getObject();
	s = aoi->getString();
	printf("object: %s\t", s);
	delete [] s;
	o = v->getObjectValue();
	s = o->getString();
	printf("value: %s\n", s);
	delete [] s;
	
//	vl->removeVar(v); // xxx XXX THIS IS A TEST
    }
    
} // printResultList


// This is called by mibQueryHandler when it's through with a get request.
// I get the transaction ID and a structure with several varBindLists -
// one with 'good' data, others with the vars that got various errors.
void
Browser::getResponse(int transId, struct result* vbls) {
    snmpResponse(transId, vbls, True);   
    // closeDialogBox(NULL);    
} // getResponse

void
Browser::setResponse(int transId, struct result* vbls) {
    // printf("Browser::setResponse\n");
    
    snmpResponse(transId, vbls, False);
	
} // setResponse


void
Browser::snmpResponse(int transId, struct result* vbls, tuBool get) {
    // printf("snmpResponse(transID = %d) -\n", transId);

//    printf("=== Good Results:\n");
//    printResultList(&vbls->goodResult);

    
    /******
    // xxx shouldn't have printfs here...
    if (vbls->notfoundResult.getNext()) {
	printf("=== not found Results:\n");
	printResultList(&vbls->notfoundResult);
    }   
    if (vbls->badvalueResult.getNext()) {
	printf("=== bad value Results:\n");
	printResultList(&vbls->badvalueResult);
    }   
    if (vbls->readonlyResult.getNext()) {
	printf("=== read only Results:\n");
	printResultList(&vbls->readonlyResult);
    }   
    if (vbls->generrorResult.getNext()) {
	printf("=== gen error Results:\n");
	printResultList(&vbls->generrorResult);
    }   
    if (vbls->timeexpiredResult.getNext()) {
	printf("=== time expired Results:\n");
	printResultList(&vbls->timeexpiredResult);
    }
    *****/
    
    
    // look up which bTopLevel matches this transId
    trInfo* tiPrevious;
    for (trInfo* ti = transInfoList;
	    ti != NULL && ti->btl != NULL;
	    ti = ti->next) {
	if (ti->transId == transId)
	    break;
	tiPrevious = ti;
    }
    
    if (ti && ti->btl) {
	if (get)
	    ti->btl->handleGetResponse(vbls);
	else 
	    ti->btl->handleSetResponse(vbls);
    } else {
	return;
    }
    
    // delete this one from transInfoList
    if (ti == transInfoList)
	transInfoList = ti->next;
    else
	tiPrevious->next = ti->next;
	
    delete ti;

} // snmpResponse

DialogBox*
Browser::getDialogBox(void) {
    if (dialog->isMapped())
	dialog->unmap();
    dialog->markDelete();
    // printf("dialog was markDelete()'ed; creating new one\n");
    dialog = new DialogBox("Browser", this, False);
    setDialogBoxCallBack();
    
    return dialog;
}

void
Browser::openDialogBox(void) {
    if (dialog->isMapped()) {
	dialog->unmap();
	// dialog->setIconic(False);
	XFlush(getDpy());
    }
    // dialog->setIconic(False);
    dialog->map();
    XFlush(getDpy());
}

void
Browser::closeDialogBox(tuGadget *) {
    dialog->unmap();
    XFlush(getDpy());
}

void
Browser::closeLicDialogBox(tuGadget *) {
    licDialog->unmap();
    XFlush(getDpy());
}

void
Browser::closeProgressDialogBox(tuGadget *) {
    progressDialog->unmap();
    XFlush(getDpy());
}

void
Browser::setDialogBoxCallBack(void) {
    dialog->setCallBack(new BrowserCallBack(this, Browser::closeDialogBox));
}

void
Browser::setLastFile(char* newName) {
    if (lastFile) 
	delete lastFile;
    lastFile = strdup(newName);
}
