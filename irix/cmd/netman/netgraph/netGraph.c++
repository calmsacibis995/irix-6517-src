/*
 * Copyright 1989,1990,1991,1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph
 *
 *	$Revision: 1.42 $
 *	$Date: 1996/02/26 01:27:47 $
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h> // for rand()
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <signal.h>
#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>
#include <osfcn.h>
#include <pwd.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>   // for errno
#include <helpapi/HelpBroker.h>

// tinyui includes
#include <tuCallBack.h>
#include <tuFilePrompter.h>
#include <tuGadget.h>
#include <tuGC.h>
#include <tuPalette.h>
#include <tuRowColumn.h>
#include <tuScreen.h>
#include <tuTimer.h>
#include <tuTopLevel.h>
#include <tuVisual.h>
#include <tuWindow.h>
#include <tuXExec.h>

// netgraph includes
#include "arg.h"
#include "constants.h"
#include "editControl.h"
#include "messages.h"
#include "netGraph.h"
#include "paramControl.h"
#include "scrollGadget.h"
#include "timeGadget.h"
#include "viewGadget.h"

#include "dialog.h"
#include "event.h"
#include "tooloptions.h"

#include "help.h"

extern "C" {

#include <bstring.h>
#include <getopt.h>
#include <malloc.h>

// netvis/snoop includes
#include "exception.h"
#include "expr.h"
#include "license.h"
#include "protocol.h"
#include "snooper.h"
#include "snoopstream.h"

#include "netGraph_xdr.h"

int hist_init(SnoopStream*, h_info*, int interval);
int hist_read(SnoopStream*, struct histogram*);

int kill (pid_t pid, int sig);
int sethostresorder(const char *);

} // (extern C)


extern int _yp_disabled;


#define CONTROLS_FILE	    "~/.netgraphrc"
const float MEGABIT        = (1000000.0 / 8.0); // # bytes/sec for 1M bits/sec
const float ETHER_CAPACITY = (10.0 * MEGABIT);
const float ETHER_HEAD     = 24.0; // xxx is this right???
const float FDDI_CAPACITY  = (100.0 * MEGABIT);
const float FDDI_HEAD      = 0.0; // xxx what should this be???
const float TOKENRING_CAPACITY = (16.0 * MEGABIT);
const float TOKENRING_HEAD = 0.0; // xxx what should this be???

const char VIEW_GEOMETRY[] = "414x220+506+271";
const char EDIT_GEOMETRY[] = "445x618+8+142";
const char PARAM_GEOMETRY[] = "526x335+207+40";


// this is to convert from my "types" to event's "bases"
// static char* graphTypes[] = { "packets", "bytes", "%packets", "%bytes",
// 		"%ether", "%fddi", %tokenring,"%npackets", "%nbytes", 0 };
enum rateBase rateBases[] = { PACKET_BASED, BYTE_BASED, PERCENT_PKTS, PERCENT_BYTES, 
		PERCENT_UTIL, PERCENT_UTIL, PERCENT_UTIL, PERCENT_N_PKTS, PERCENT_N_BYTES};

void

NetGraph::doHelp(tuGadget *, void* helpCardName) {

    HELPdisplay((char*)helpCardName);

}

tuDeclareCallBackClass(NetGraphCallBack, NetGraph);
tuImplementCallBackClass(NetGraphCallBack, NetGraph);

extern NetGraph *netgraph;


// Define the args that we understand.  For example,
// saying "-fn foo" is the same as putting "NetGraph*font: foo" in a
// .Xdefaults file. 
// HAD TO ADD -n & -s, since they were being eaten by tuResourceDB
// as -name and -sync! Also -i (-iconic)
static XrmOptionDescRec args[] = {
    {"-i",	"*interface",		XrmoptionSepArg, NULL},
    {"-u",	"*controlsFile",	XrmoptionSepArg, NULL},
    {"-y",      "*useyp",               XrmoptionNoArg, "True"},
    {"-h",	"*help", 		XrmoptionNoArg, "Yes"},
    {"-n",	"*noTime",		XrmoptionNoArg, "True"},
    {"-s",	"*scrollTime",		XrmoptionNoArg, "True"},
};

static const int nargs = sizeof(args) / sizeof(XrmOptionDescRec);

// Define some application specific resources.  Right now, they all
// need to be placed into one structure.  Note that most of these
// names match up to the command line arguments above.
struct AppResources {
    char* interface;
    char* controlsFile;
    char* hostresorder;
    tuBool useyp;
    tuBool help;
    tuBool noTime;
    tuBool scrollTime;

    long editControlBackgroundColor;
    long paramControlBackgroundColor;
    long selectColor;
    long alarmColor;

    short   stripBackgroundIndexColor;
    short   stripScaleIndexColor;
    short   scrollingTimeIndexColor;
};


static tuResourceItem opts[] = {
    { "interface", 0, tuResString, "", offsetof(AppResources, interface), },
    { "controlsFile", 0, tuResString, CONTROLS_FILE, offsetof(AppResources, controlsFile), },
    { "useyp", 0, tuResBool, "False", offsetof(AppResources, useyp), },
    { "hostresorder", 0, tuResString, "local", offsetof(AppResources, hostresorder), },
    { "help", 0, tuResBool, "False", offsetof(AppResources, help), },
    { "noTime", 0, tuResBool, "False", offsetof(AppResources, noTime), },
    { "scrollTime", 0, tuResBool, "False", offsetof(AppResources, scrollTime), },

    { "editControlBackgroundColor", 0, tuResColor, NULL, offsetof(AppResources, editControlBackgroundColor), },
    { "paramControlBackgroundColor", 0, tuResColor, NULL, offsetof(AppResources, paramControlBackgroundColor), },
    { "selectColor", 0, tuResColor, NULL, offsetof(AppResources, selectColor), },
    { "alarmColor", 0, tuResColor, NULL, offsetof(AppResources, alarmColor), },

    { "stripBackgroundIndexColor", 0, tuResShort, "7", offsetof(AppResources, stripBackgroundIndexColor), },
    { "stripScaleIndexColor", 0, tuResShort, "41", offsetof(AppResources, stripScaleIndexColor), },
    { "scrollingTimeIndexColor", 0, tuResShort, "0", offsetof(AppResources, scrollingTimeIndexColor), },
    0,
};


NetGraph::NetGraph(int argc, char *argv[]) {   
    // XXXXX to see what malloc problem is.
    // printf("malloc checking is enabled\n");
    // mallopt(M_DEBUG, 1);

    // Default and initial values
    strcpy(lastFile, "");
    strcpy(recordFile, "");
    strcpy(logFile, "");
    strcpy (windowName, "NetGraph");
    
    viewGadgetGeom = NULL;
    editControlGeom = NULL;
    paramControlGeom = NULL;
    freeDataList = NULL;
    rfp = NULL;
    afp = stderr;

    // used to init localHostName, snooper stuff here, but moved
    // farther down until after dialog was created, so we can
    // post any error

    bzero(&hist, sizeof hist);
    bzero(&oldHist, sizeof oldHist);
    bzero(filters, sizeof filters);
    
    fatalError = False;
    postedFatal = False;
    quitNow = False;
    history = False;
    outputAscii = False;
    lockPercentages = False;
    maxValues = False;
    recording = False;
    sameScale = False;
    snooping = False;
    useLines = False;
    for (int i = 0; i < 128; i++)
	got[i] = False;
    gotTimeType = False;
    gotTimeTypeFromResource = False;
    gotInterfaceFromResource = False;
    editControlWasMapped = False;
    paramControlWasMapped = False;
    iconic = False;

    errs = 0;
    highestBin = -1;
    highestNumber = 0;
    sampleSize = 0;
    historyDirection = 1;
    historySamples = 0;
    netfiltersPid = 0;
    strips = 0;
    timerSeconds = 0;
    timeSinceUpdate = 0;
    snpStm.ss_sock = -1; // so we can tell if it's been opened yet
     
    interval = DEF_INTERVAL;
    samples = 61;
    period = 600;
    avgPeriod = -1; // if user doesn't specify, we'll make it = period
    updateTime = DEF_UPD_TIME;
    timeType = TIME_SCROLLING;  // how to display time labels

    editControl = NULL;
    paramControl = NULL;
    selectedStrip = NULL;
    specifiedFilter = NULL;
    for (i = 0; i < MAX_RC_LINES; i++)
	rcLines[i] = NULL;
    
    protoInit();
    
    // get resources
    
    // Create a place to store the resource database, and load it
    // up.  This also parses the command lines, opens the X display, and
    // returns to us the default screen on that display.

    rdb = new tuResourceDB;
    AppResources ar;
    
    ar.editControlBackgroundColor = 0;
    ar.paramControlBackgroundColor = 0;
    ar.alarmColor = 0;
    ar.selectColor = 0;

    
    char* instanceName = 0;
    char* className = "NetGraph";
 
    tuScreen* screen = rdb->getAppResources(&ar, args, nargs, opts,
					   &instanceName, className,
					   &argc, argv);

    cmap = screen->getDefaultColorMap();
    display = rdb->getDpy();

    // Initialize SGIHelp
    SGIHelpInit (display, className, ".");

    // Set up callbacks (must do this before create viewGadget)
    (new NetGraphCallBack(this, NetGraph::quit))->
				registerName("__NetGraph_quit");
    (new NetGraphCallBack(this, NetGraph::save))->
				registerName("__NetGraph_save");
    (new NetGraphCallBack(this, NetGraph::saveAs))->
				registerName("__NetGraph_saveAs");

    (new NetGraphCallBack(this, NetGraph::openEditControl))->
				registerName("__NetGraph_edit");
    (new NetGraphCallBack(this, NetGraph::add))->
				registerName("__NetGraph_add");
    (new NetGraphCallBack(this, NetGraph::openParamControl))->
				registerName("__NetGraph_params");
    (new NetGraphCallBack(this, NetGraph::changeAll))->
				registerName("__NetGraph_allBars");
    (new NetGraphCallBack(this, NetGraph::changeAll))->
				registerName("__NetGraph_allLines");
    (new NetGraphCallBack(this, NetGraph::deleteCB))->
				registerName("__NetGraph_delete");
    (new NetGraphCallBack(this, NetGraph::catchUp))->
				registerName("__NetGraph_catchUp");
				
    (new tuFunctionCallBack(NetGraph::doHelp, GENERAL_HELP_CARD))->
				registerName("__NetGraph_help");

 
    // create main window (must do this before create dialog box)
    toolOptions = new ToolOptions;
    viewGadget = new ViewGadget(this, windowName, cmap, rdb,
		    instanceName, className, NULL, VIEW_GEOMETRY);
    stripParent = viewGadget->getStripParent();

    addItem = viewGadget->getAddItem();
    editItem = viewGadget->getEditItem();
    deleteItem = viewGadget->getDeleteItem();

    viewGadget->setIconicCallBack(new NetGraphCallBack(this, NetGraph::iconicCB));
    setSelectedStrip(NULL);

    // create dialog box (must do this before have a chance of any errors)
    dialog = new DialogBox("NetGraph", viewGadget, False);
    setDialogBoxCallBack();

    if (ar.help) {
	usage();
	return;
    }

    // whichever is last *should* win, but I can't tell order
    if (ar.scrollTime) {
	gotTimeTypeFromResource = True;
	timeType = TIME_SCROLLING;
    } else if (ar.noTime) {
	gotTimeTypeFromResource = True;
	timeType = TIME_NONE;
    }
	
    // Check license
    char *message;
    if (getLicense(className, &message) == 0) {
	fatalError = True;
	quitNow = True;
        dialog->error(message);
        openDialogBox();
        return;
    }
    if (message != 0) {
    	licDialog = new DialogBox("NetGraph", viewGadget, False);
	licDialog->setCallBack(new NetGraphCallBack(this, NetGraph::closeLicDialogBox));
        licDialog->information(message);
	licDialog->map();
	XFlush(getDpy());
        delete message;
    }

    if (gethostname(localHostName, 64) < 0) {
	fatalError = True;
	quitNow = True;
	post(GETHOSTNAMEMSG);
	return;
    }
    strcpy(newSnooper.node, localHostName);
    strcpy(fullSnooperString, localHostName);
    strcat(fullSnooperString, ":"); // so analyzer will like it
    newSnooper.interface[0] = NULL;

    currentSnooper.node[0] = NULL;
    currentSnooper.interface[0] = NULL;

    stripBackgroundIndexColor = ar.stripBackgroundIndexColor;
    stripScaleIndexColor = ar.stripScaleIndexColor;
    scrollingTimeIndexColor = ar.scrollingTimeIndexColor;

    // process options on command line

    strcpy(lastFile,  ar.controlsFile);
    
    // handle the interface; used to be fine in handleOptions,
    // but now -i gets interpreted as -iconic unless we do it
    // in our own resource. 
    if (ar.interface[0] != NULL) {
	gotInterfaceFromResource = True;
	fatalError = True; // since might be posted by handleIntfc()
	quitNow = True;
	if (handleIntfc(ar.interface, True) == -1)
	    return;
	fatalError = False; // return this to normal value
	quitNow = False;
	snooperChanged();
    }

    int highestLine = readFile(lastFile);
    if (highestLine == -1)
	return;

    if (handleOptions(argc, argv) == -1)
	return;


    // Read configuration, & process options in history file
    //   (that aren't already set)
    if (history) {
	// don't allow certain options when running from history file
	undoIllegalOptions();

	if (handleHistFile(fullSnooperString) == -1) {
	    return;
	}

	if (errs)
	    postMany(errs, errStrings);

    } else {
	// since we're not running from a history file,
	// take care of the rest of the controlsFile.
	if (doRestOfFile(highestLine) == -1)
	    return;

    }

    if (strcmp(logFile, "") && !open_plain_outfile(logFile, &afp)) {
	post(LOGNOOPENMSG);
	afp = stderr;
    }


    // add time legend to window
    // first, make sure period is an integer number of Intervals
    period = (samples - 1) * interval;
    timeGadget = new TimeGadget(this, stripParent, "", 
			period, interval, timeType);
    viewGadget->addTime(timeGadget);
    if (timeType != TIME_NONE)
	timeGadget->map();


    if (history) {
	addItem->setEnabled(False);
	scrollGadget = new ScrollGadget(this, stripParent, "");
	float percShown = ((float)samples / historySamples);
	if (percShown > 1.0)
	    percShown = 1.0;
	scrollGadget->setScrollPercShown(percShown);
	scrollGadget->setScrollPerc(0.0);
	scrollGadget->map();
	viewGadget->addScrollBox(scrollGadget);
	scrollGadget->map();
    }


    // open history file for writing, and write general stuff
    if (recording) {
	addItem->setEnabled(False);

	char optStr[80];
	char* optP = optStr;

	optStr[0] = NULL;

	buildOptLine(optP, False);

	if (!open_xdr_outfile(
	      recordFile, &rfp, &xdr, &highestNumber, &optP)) {
	    post(BADXDRWROPENMSG);
	    recording = False;    
	    if (rfp)
		close_xdr_file(rfp, &xdr);
	} else {
	    if (outputAscii) {
		// first do stuff that open_xdr_outfile wrote to outfile
		printf("Number of strip charts: %d\n",  highestNumber);
		printf("%s\n", optP);
	    }
	    int type, baseRate, color, avgColor, style;
	    int alarmSet, alarmBell;
	    float alarmLo, alarmHi;
	    char* string;

	    for (StripGadget* sg = strips; sg != 0; sg = sg->getNext()) {
		string = filters[sg->getBin()].expr;
	        type = sg->getType();
		baseRate = sg->getBaseRate();
		color = sg->getColor();
		avgColor = sg->getAvgColor();
		style = sg->getStyle();
		alarmSet = (int) sg->getAlarmSet();
		alarmBell = (int) sg->getAlarmBell();
		alarmLo = sg->getAlarmLoVal();
		alarmHi = sg->getAlarmHiVal();

		// NOTE: version == 0 means use NETGRAPH_HIST_VERSION
		if (!do_xdr_stripInfo(&xdr, 0,
			&string, &type, &baseRate, 
			&color, &avgColor, &style,
			&alarmSet, &alarmBell, &alarmLo, &alarmHi)) {
		    post(BADXDRWRITEMSG);
		    recording = False;
		    if (rfp)
			close_xdr_file(rfp, &xdr);
		    break;
		}

		if (outputAscii) {
		    // now strip info
		    printf("filter: %s ", string);
		    if (type < TYPE_PNPACKETS)
			printf("%s\n", heading[type]);
		    else
			printf("Percent of %d %s\n",
			    sg->getBaseRate(), heading[type]);

		} // outputAscii
	    } // for each strip
	    if (outputAscii)
		printf("\n");

	} // opened ok
    } // recording



    EV_handle evfd;
    static char* appName = "NetGraph";

    eh = new EV_handler(&evfd, appName);
    event = new EV_event(NV_STARTUP);
    alarmEvent = new EV_event();
    eh->send(event);
   

    // Create other windows

    if (editControlGeom == NULL)
	editControlGeom = strdup(EDIT_GEOMETRY);
    if (paramControlGeom == NULL)
	paramControlGeom = strdup(PARAM_GEOMETRY);


    if (viewGadgetGeom == NULL)
	viewGadgetGeom = strdup(VIEW_GEOMETRY);
	
    viewGadget->parseGeometry(viewGadgetGeom, "");

    editControl = new EditControl(this, "Edit Control Panel",
		    viewGadget, editControlGeom);
    paramControl = new ParamControl(this, "Parameters Control Panel",
		    viewGadget, paramControlGeom);

    editControlX = editControl->getXOrigin();
    editControlY = editControl->getYOrigin();
    paramControlX = paramControl->getXOrigin();
    paramControlY = paramControl->getYOrigin();


    if (ar.editControlBackgroundColor)
	editControl->setBackground( (tuColor*) ar.editControlBackgroundColor);
    if (ar.paramControlBackgroundColor)
	paramControl->setBackground( (tuColor*) ar.paramControlBackgroundColor);

    if (ar.alarmColor)
	alarmColor = (tuColor*) ar.alarmColor;
    else
 	alarmColor = tuPalette::getRed(screen);
	
    if (ar.selectColor)
	selectColor =(tuColor*) ar.selectColor;
    else
 	selectColor = tuPalette::getSelectFillColor(screen);



    toolOptions->setTopLevel(viewGadget);

    // Get hints for layout and resize
    tuLayoutHints hints;
    viewGadget->getLayoutHints(&hints);
    viewGadget->resize(hints.prefWidth, hints.prefHeight);
    editControl->getLayoutHints(&hints);
    editControl->resize(hints.prefWidth, hints.prefHeight);
    paramControl->getLayoutHints(&hints);
    paramControl->resize(hints.prefWidth, hints.prefHeight);
    
    // Map the view window
    viewGadget->map();

    // Set hostresorder and _yp_disabled
    if (ar.useyp)
	_yp_disabled = 0;
    else
	_yp_disabled = 1;
    sethostresorder(ar.hostresorder);


} // NetGraph


NetGraph::NetGraph(const char* instanceName, tuColorMap* cmap,
	      tuResourceDB* db, char* progName, char* progClass) {
    // I don't think I'll need this one...
}

void
NetGraph::add(tuGadget *) {
    StripGadget* sg;
    if (makeNewStrip(&sg, selectedStrip) == -1)
	return;



    
    int retVal;
    
    if (selectedStrip)
	retVal = netgraph->setupStripInts((char*)selectedStrip->getFilter(),
	    selectedStrip->getType(),  selectedStrip->getBaseRate(),
	    selectedStrip->getColor(), selectedStrip->getAvgColor(), 
	    selectedStrip->getStyle(), selectedStrip->getAlarmSet(),
	    selectedStrip->getAlarmBell(),
	    selectedStrip->getAlarmLoVal(),
	    selectedStrip->getAlarmHiVal(), 0, sg, True);
    else if (editControl && editControl->isMapped())
	retVal = netgraph->setupStripInts(editControl->getFilter(),
	    editControl->getType(),  editControl->getBaseRate(),
	    editControl->getColor(), editControl->getAvgColor(), 
	    editControl->getStyle(), editControl->getAlarmSet(),
	    editControl->getAlarmBell(),
	    editControl->getAlarmLoVal(),
	    editControl->getAlarmHiVal(), 0, sg, True);
    else
	retVal = netgraph->setupStripInts("total", TYPE_PACKETS,
	    1000, 4, 1, STYLE_BAR, False, False,
	    0.0, 10.0, 0, sg, True);

		
    if (retVal == -1) {
	deleteGraph(sg);
	sg->setNumber(0);
    }
    
}



void
NetGraph::openEditControl(tuGadget *) {    
    // give the control panel a pointer to the selected strip
    editControl->setSg(selectedStrip, NULL);

    if (editControl->isMapped()) {
	if (editControl->getIconic())
	    editControl->setIconic(False);
	editControl->pop();
    } else
	editControl->map();

} // openEditControl

void
NetGraph::closeEditControl(void) {
    editControl->unmap();
}

void
NetGraph::openParamControl(tuGadget *) {
    if (paramControl->isMapped()) {
	if (paramControl->getIconic())
	    paramControl->setIconic(False);
	paramControl->pop();
    } else
	paramControl->map();
}

void
NetGraph::closeParamControl(void) {
    paramControl->unmap();
}


// this gets called AFTER we actually change state between iconic & not
void
NetGraph::iconicCB(tuGadget *) {
    if (viewGadget->getIconic()) {
	iconic = True;

	if (editControl->isMapped()) {
	    editControlWasMapped = True;
	    editControlX = editControl->getXOrigin();
	    editControlY = editControl->getYOrigin();
	    editControl->unmap();
	} else
	    editControlWasMapped = False;
	    
	if (paramControl->isMapped()) {
	    paramControlWasMapped = True;
	    paramControlX = paramControl->getXOrigin();
	    paramControlY = paramControl->getYOrigin();
	    paramControl->unmap();
	} else
	    paramControlWasMapped = False;

    } else {
	iconic = False;

	if (editControlWasMapped) {
	    editControl->setInitialOrigin(editControlX, editControlY);
	    editControl->map();
	}
	if (paramControlWasMapped) {
	    paramControl->setInitialOrigin(paramControlX, paramControlY);
	    paramControl->map();
	}
    }
    
}

tuBool
NetGraph::isIconic(void) {
    return iconic;
}

// change to all lines or all bars
void
NetGraph::changeAll(tuGadget* g) {
    int newStyle;
    if (strcmp(g->getInstanceName(), "allLinesItem"))
	newStyle = STYLE_BAR;
    else
	newStyle = STYLE_LINE;
	
    for (StripGadget* sg = strips; sg != 0; sg = sg->getNext()) {
		sg->setStyle(newStyle);
    }
    viewGadget->render();
    
    // printf("NetGraph::changeAll -  %s\n", graphStyles[newStyle]);

}


// delete the selected graph
void
NetGraph::deleteCB(tuGadget*) {
    if (!selectedStrip) {
	// printf("deleteCB - no strip selected\n");
	return;
    }
	
    // printf("NetGraph::delete (%d)\n", selectedStrip->getNumber());
    // xxx find selected graph and call deleteGraph() 
    
/***** NOW, disable delete if only one strip..
    // if it's the last strip,  quit
    if (viewGadget->getNumberOfGraphs() == 1) {
	quit(NULL);
    }
******/
    if (selectedStrip)
    	deleteGraph(selectedStrip);

    // if there's only one graph left now, disable delete
    if (viewGadget->getNumberOfGraphs() == 1)
	deleteItem->setEnabled(False);
    
}


void
NetGraph::catchUp(tuGadget*) {
    // printf("NetGraph::catchUp\n");
    if (snpStm.ss_sock != -1) {
	(void) snoopStop(&snpStm);
	(void) snoopStart(&snpStm);
    }
}


void
NetGraph::quit(tuGadget *g) {

    if (recording || history)
	doQuit(NULL);
    else {
	if (dialog->isMapped())
	    dialog->unmap();

	// we'll make a new dialog box, because otherwise sometimes
	// have trouble with it coming up iconified
	dialog->markDelete();
	dialog = new DialogBox("NetGraph", viewGadget, False);

	if (g)
	    dialog->question("Save UI to %s before quitting", lastFile);
	else
	    dialog->question("Fatal error. Save UI to %s before quitting", lastFile);
	
	dialog->setCallBack(new NetGraphCallBack(this, NetGraph::doQuit));
	dialog->map();
	XFlush(getDpy());
    }
}


void
NetGraph::doQuit(tuGadget *g) {
    enum tuDialogHitCode hitCode;

    if (g)
	hitCode = dialog->getHitCode();
    else
	hitCode = tuNo;


    // if it's a fatal error, can't cancel; take it to mean don't save
    if (fatalError && hitCode == tuCancel)
	hitCode = tuNo;
	
    switch (hitCode) {
	case tuYes:
	    if (doSave() == -1) {
		openDialogBox();
		return;
	    }
	    // Save was successful, fall through
	case tuNo: {
	    // xxx actually, I think I can just unsubscribe & close 
	    // (without bothering to stop & delete)
	    if (snpStm.ss_sock != -1) {
		(void) snoopStop(&snpStm);

		for (int bin = highestBin; bin >= 0; bin--)
		    (void) ss_delete(&snpStm, bin);

		(void) ss_unsubscribe(&snpStm);
		ss_close(&snpStm);
	    }
    

	    if (netfiltersPid != 0) {
		kill(netfiltersPid, SIGTERM);
	    }

	    // make a new event, so it doesn't have the interface in it.
	    EV_event shutdown(NV_SHUTDOWN);
	    eh->send(&shutdown);

	    HELPclose();
    
	    if (fatalError)
		exit(-1);
	    else
		exit(0);
	}
    } // switch
    
}

void
NetGraph::immediateQuit(tuGadget*) {
    exit(-1);
}


char *
NetGraph::expandTilde(const char *c)
{
    if (c[0] != '~')
        return 0;
    char *p = strchr(c, '/');
    if (p == c + 1 || c[1] == '\0') {
        char *home = getenv("HOME");
	unsigned int len = strlen(home);
	if (len == 1)
	    len = 0;
	char *nc = new char[strlen(c) + len];
	strncpy(nc, home, len);
	if (p != 0)
	    strcpy(nc + len, p);
	return nc;
    }
    if (p != 0)
        *p = '\0';
    struct passwd *pw = getpwnam(c + 1);
    if (pw == 0)
        return 0;
    unsigned int len = strlen(pw->pw_dir);
    char *nc = new char[strlen(c) + len];
    strncpy(nc, pw->pw_dir, len + 1);
    if (p != 0) {
        *(nc + len) = '/';
        strcpy(nc + len + 1, p + 1);
        *p = '/';
    }
    return nc;
} // expandTilde


/*
 * Read the configuration file.  If no file is specified, use CONTROLS_FILE
 * from the current or home directory.
 * After we get something open, set 'lastFile' to the name of the file
 * we opened, so we can write back to the same place later.
 */
 // copied from netgraph
 // xxx return -1 == big failure; quit
 //	return  n == highest line number we saved.
int
NetGraph::readFile(char* fileName) {
    // printf("NetGraph::readFile(%s)\n", fileName);

    static char white[] = " \t\n";
    static char dblQuote[] = "\"";
    static char sglQuote[] = "'";
    static char defaultFilter[] = "total";

    int err = 0;
    FILE *fp = NULL;
  
    char buf[256];
    char *filtExpr;
    char *tokens[9];
    int lineNumber = 1;

    errs = 0;


    if (strcmp(fileName, "-") == 0) {
	fp = stdin;
	strcpy(lastFile, CONTROLS_FILE);
    } else {
	char* newname = expandTilde(fileName);
	if (newname != 0) {
	    strcpy(lastFile, newname);
	    fileName = lastFile;
	    delete [] newname;
	}
	
	fp = fopen(fileName, "r");
    }

    // if there's no file, use the 'total' filter,
    // else read options & graph specifications
    if (fp == NULL) {
	err = errno;
	dialog->warning(errno, "Could not open %s", fileName);
	openDialogBox();
	strcpy(buf, defaultFilter);
    } else {
	fgets(buf, sizeof buf, fp);
    } // (fp not null)



    // go through file, get options & strip specifications
    char tempStr[256];
    do {

	// first see if line starts with "#"; if so, it's a comment
	// then see if there are any quotes in the config file line
	//   if yes, then first token is what's between quotes
	//   if not, then tokens are all separated by whitespace
	// also check if it's an option or  geometry line

	if (buf[0] == '#')
	    continue;



	strcpy(tempStr, buf); // copy so we can still get the whole thing
	if (strchr(buf, '"')) {
	    filtExpr = strtok(buf, dblQuote);
	} else if (strchr(buf, '\'')) {
	    filtExpr = strtok(buf, sglQuote);
	} else {
    	    filtExpr = strtok(buf, white);
	}
	
	if (filtExpr == NULL)
	    continue;

	if (!strcmp(filtExpr, "option")) {
	    if (handleStringOfOptions(tempStr) == -1)
		return -1;

	} else if (!strcmp(filtExpr, "NetGraphGeometry")) {
	    viewGadgetGeom = strdup(strtok(NULL, white));
	} else if (!strcmp(filtExpr, "EditControlGeometry")) {
	    editControlGeom = strdup(strtok(NULL, white));
	    editControlWasMapped = True;
	} else if (!strcmp(filtExpr, "ParamControlGeometry")) {
	    paramControlGeom = strdup(strtok(NULL, white));
	    paramControlWasMapped = True;

	} else {
	    // write the rest of the file (this line to the end) to
	    // memory. If it turns out we're running from a history file,
	    // we won't care about most of it.
	    if (lineNumber > MAX_RC_LINES) {
		fatalError = True;
		quitNow = True;
		post(RCTOOBIGMSG);
		openDialogBox();
		return -1;
	    } else
		rcLines[lineNumber] = strdup(tempStr);
	}

    } while (lineNumber++ && fp != NULL && fgets(buf, sizeof buf, fp) != NULL);

    rcLines[lineNumber] = NULL;

    if (fp != stdin)
	fclose(fp);

    // if we collected any errors from the config file, post them all together

    if (errs) {
	if (errs >= MAXERRS) {
	    errs = MAXERRS;
	    fatalError = True;
	    quitNow = True;
	    postMany(errs, errStrings);
	    return -1;
	}

	// if no legit filters, prepare to bail out.
	if ((highestBin == 0) && (filters[0].used == 1)) {
	    fatalError = True;
	    quitNow = True;
	    postMany(errs, errStrings);
	    return -1;
	}

	postMany(errs, errStrings);
    }

    return lineNumber;
    
} // readFile

// handle the part of the controls file we just copied to memory
// when we first read the file.
int
NetGraph::doRestOfFile(int lastLine) {

    // First,  check time args now that we've got the options.
    // Then start snooping.
    if (checkTimeArgs(&samples, &period, &interval, &updateTime,
	    &avgPeriod, &avgSamples) == -1) {
	fatalError = True;
	quitNow = True;
	return -1;
    }

    // get host, set up snoop stream
    int retVal = hist_init(&snpStm, &newSnooper, interval);
    if (retVal < 0) {
	snpStm.ss_sock = -1;
	fatalError = True;
	quitNow = True;
	// char ebuf[64];
	// sprintf (ebuf, "%s:%s", newSnooper.node, newSnooper.interface);
	post(-1 * retVal);
	return -1;
    } else {
	snooperChanged();
    }

    // this filter is really only needed if user wants totals or
    // percentages, but we'll always add it just to be safe and simple.
    ExprError err;
    int bins = ss_add(&snpStm, 0, &err);
    strcpy (filters[0].expr, "total");
    filters[0].used++;
    if (bins < 0) {
	fatalError = True;
	quitNow = True;
	post(NOADDPRMSCSMSG);
	return -1;
    }

    if (specifiedFilter) {
	// the user specified a filter on the command line,
	// so just use it; ignore rest of the file
	rcLines[1] = specifiedFilter;
	lastLine = 1;
    }
    static char white[] = " \t\n";
    static char dash[] = "-";
    static char dblQuote[] = "\"";
    static char sglQuote[] = "'";

  
    char buf[256];
    char *filtExpr;
    char *tokens[9];
    
    for (int lineNumber = 1; lineNumber <= lastLine; lineNumber++) {
	if (rcLines[lineNumber] == NULL)
	    continue;
	    
	if (strchr(rcLines[lineNumber], '"')) {
	    filtExpr = strtok(rcLines[lineNumber], dblQuote);
	} else if (strchr(rcLines[lineNumber], '\'')) {
	    filtExpr = strtok(rcLines[lineNumber], sglQuote);
	} else {
    	    filtExpr = strtok(rcLines[lineNumber], white);
	}
	if (filtExpr == NULL)
	    continue;

	// there can be as many as 9 more tokens -
	// type, N for %Nbytes/%Npackets, color, avgColor, style;
	// alarmSet, alarmBell, alarmLoVal, alarmHival
	for (int i = 0; i < 9; i++) {
	    tokens[i] = strtok(NULL, white);
	}
	
	StripGadget* sg = new StripGadget(this, stripParent, "");
	if (completeNewStrip(sg, 0) == -1)
	    return -1;

	if (setupStripTkns(filtExpr, tokens, lineNumber,
	      sg, False) == -1) {
	    // it failed, so this isn't a good strip, so delete it.
	    deleteGraph(sg);
	}

	if (errs > MAXERRS)
	    break;

    } // for each line

    if (errs) {
	if (errs >= MAXERRS) {
	    errs = MAXERRS;
	    fatalError = True;
	    quitNow = True;
	    postMany(errs, errStrings);
	    return -1;
	}

	// if no legit filters, prepare to bail out.
	if ((highestBin == 0) && (filters[0].used == 1)) {
	    fatalError = True;
	    quitNow = True;
	    postMany(errs, errStrings);
	    return -1;
	}

	postMany(errs, errStrings);
    }

    return 0;
    
} // doRestOfFile


void
NetGraph::save(tuGadget *) {
    if (doSave() == -1)
	openDialogBox();
}

int
NetGraph::doSave() {
    // printf("NetGraph::doSave, <%s>\n", lastFile);
    
    FILE *fp = NULL;
    fp = fopen(lastFile, "w");

    if (fp == NULL) {
	dialog->warning(errno, "Could not write %s", lastFile);
	return - 1;
    }

    /***

    // traffic gizmo parameters
    fprintf(fp, "SnoopInterface\t%s\n", fullSnooperString);
	    
    fprintf(fp, "SnoopFilter\t\t%s\n", filter);
	    
    fprintf(fp, "MeasureTraffic\t%s\n", graphTypes[type]);

    fprintf(fp, "DataUpdateTime\t%f\n",
			(float)dataUpdateTime / 10.0);

    if (viewGadget)
	fprintf(fp, "VerticalScale\t%s\n", rescaleTypes[viewGadget->getRescaleType()]);

    if (editControl)
        fprintf(fp, "LockAt\t\t%d\n", editControl->getFixedScale());
    ****/
    
	
	    
 	    
  

// vvvvvv from old netgraph.c++

    // write the file.

    // first, the options/args/params
    char optLine[80];
    optLine[0] = NULL;
    buildOptLine(optLine, True);
    fputs(optLine, fp);
    

    // now the graph specifications
    // note that the file has to be in the reverse order of the 'strips'
    // linked list, and we don't have back-pointers on that, so do this:
    //  - go through 'strips'   
    //    = get spec of each strip
    //    = write spec (one line) to array of spec-lines
    //  - go through that array backwards
    //    = write line to file
    char* filterExpr;
    char* graphType;
    int   baseRate;
    char* graphStyle;
    int   colorIdx;
    int   avgColorIdx;
    char* alarmSet;
    char* alarmBell;
    float alarmLo;
    float alarmHi;
    char  specLine[20][300]; // can't fit more than 20 graphs on a screen
    int   line = 0;

    for (StripGadget* sg = strips; sg != 0; sg = sg->getNext()) {
	filterExpr = filters[sg->getBin()].expr;
	if (strchr(filterExpr, ' ')) {
	    char tempStr[302];
	    strcpy(tempStr, "\"");
	    strcat(tempStr, filterExpr);
	    strcat(tempStr, "\"");
	    filterExpr = tempStr;
	}
	graphType = graphTypes[sg->getType()];
	baseRate = sg->getBaseRate(); 
	graphStyle = graphStyles[sg->getStyle()];
	colorIdx = sg->getColor();
	avgColorIdx = sg->getAvgColor();
	alarmSet = alarmSets[sg->getAlarmSet()];
	alarmBell = alarmBells[sg->getAlarmBell()];
	alarmLo = sg->getAlarmLoVal();
	alarmHi = sg->getAlarmHiVal();
	if (sg->getType() >= TYPE_PNPACKETS)
	    sprintf(specLine[line++], "%s %s %d %d %d %s %s %s %.2f %.2f\n",
		filterExpr, graphType, baseRate, colorIdx, avgColorIdx, graphStyle,
		alarmSet, alarmBell, alarmLo, alarmHi);
 	else
	    sprintf(specLine[line++], "%s %s %d %d %s %s %s %.2f %.2f\n",
		filterExpr, graphType, colorIdx, avgColorIdx, graphStyle,
		alarmSet, alarmBell, alarmLo, alarmHi);
     }

    line--;

    do {
	fputs(specLine[line--], fp);
    } while (line >= 0);

    // window geometry
    if (viewGadget)
	fprintf(fp, "NetGraphGeometry\t%dx%d%+d%+d\n",
		viewGadget->getWidth(),   viewGadget->getHeight(),
		viewGadget->getXOrigin(), viewGadget->getYOrigin());
    if (editControl && editControl->isMapped()) {
	fprintf(fp, "EditControlGeometry\t%dx%d%+d%+d\n",
		    editControl->getWidth(),   editControl->getHeight(),
		    editControl->getXOrigin(), editControl->getYOrigin());
    }
    if (paramControl && paramControl->isMapped()) {
	fprintf(fp, "ParamControlGeometry\t%dx%d%+d%+d\n",
		    paramControl->getWidth(),   paramControl->getHeight(),
		    paramControl->getXOrigin(), paramControl->getYOrigin());
    }


    fclose(fp);
    return 0;

// ^^^^^^


} // doSave

void
NetGraph::saveAs(tuGadget *) {
    if (prompter == 0) {
	prompter = new tuFilePrompter("prompter", viewGadget, True);
	prompter->bind();
    }
    if (prompter->isMapped())
	prompter->unmap();
    char *home = getenv("HOME");
    if (home == 0)
	home = "/usr/tmp";
    prompter->readDirectory(home);
    prompter->resizeToFit();
    prompter->setName("Save Controls");
    prompter->setCallBack(
		new NetGraphCallBack(this, NetGraph::savePrompt));
    prompter->mapWithCancelUnderMouse();

}

void
NetGraph::savePrompt(tuGadget *)
{
    char *file = prompter->getSelectedPath();
    prompter->unmap();
    if (file == 0)
	return;

    strcpy(lastFile, file);
    if (doSave() == -1)
	openDialogBox();

}


// post error or warning message
void
NetGraph::post(int msg, char* errString) {
// NetGraph::post(int msg, char* errString, int lineNumber) {
    // once a fatal error is posted, don't post any more
    // (this is important, because if snoop read problem, might
    // continue to get more of them, and don't want lots of dialogs)
    if (postedFatal)
	return;
    if (fatalError)
	postedFatal = True;
	
    char msgBuf[250];

    switch(msg)
    {
	case NOSNOOPDMSG:
	case SSSUBSCRIBEERRMSG:
	    sprintf(msgBuf, "Could not connect to %s\n\nCheck that snoopd is installed and configured correctly on the Data Station. See Appendix A of the NetVisualyzer User's Guide for detailed help", exc_message);
	    break;

	case ILLEGALINTFCMSG:
	    sprintf(msgBuf, "Data Station name not recognized.\n\nCheck that %s is a valid name and is entered in the appropriate hosts data base.  See Appendix A of the NetVisualyzer User's Guide for detailed help", errString);
	    break;

	default:
	    if (errString == NULL && exc_message == NULL) {
		strcpy(msgBuf, Message[msg]);
	    
	    } else {
		char buf[128];
		if (exc_message != NULL) {
		    if (errString == NULL)
			errString = exc_message;
		    else {
			sprintf(buf, "%s: %s", errString, exc_message);
			errString = buf;
		    }
		    exc_message = NULL;
		}
	    }
	
	    if (errString == NULL)
		sprintf(msgBuf, "%s\n", Message[msg]);
	    else
		sprintf(msgBuf, "%s: %s\n", Message[msg], errString);
	    break;

    }
    
    if (fatalError) {
	// we'll make a new dialog box, because otherwise sometimes
	// have trouble with it coming up iconified
	dialog->markDelete();
	dialog = new DialogBox("NetGraph", viewGadget, False);
	dialog->error(msgBuf);
    } else
	dialog->warning(msgBuf);
    
    openDialogBox();
} // post


void
NetGraph::postMany(int count, char* msgs[]) {
    // printf("NetGraph::postMany(%d)\n", count);
    if (postedFatal)
	return;
    if (fatalError)
	postedFatal = True;
    
    dialog->setMultiMessage(True);

    for (int i = 0; i < count; i++) {
	if (fatalError) {
	    // we'll make a new dialog box, because otherwise sometimes
	    // have trouble with it coming up iconified
	    dialog->markDelete();
	    dialog = new DialogBox("NetGraph", viewGadget, False);
	    dialog->error(msgs[i]);
	} else
	    dialog->warning(msgs[i]);
    }

    openDialogBox();
    
    dialog->setMultiMessage(False);
}

void
NetGraph::collectErrs(int msg, char *errString,  int lineNumber) {
    if (errs < MAXERRS) {
        errStrings[errs] = (char*) malloc(200);
        if (msg < 0)
            strcpy(errStrings[errs], "");
        else
            strncpy(errStrings[errs], Message[msg], 199);

        if (lineNumber) {
            char tempStr[200];
            sprintf(tempStr, "%s on line %d: ", errStrings[errs], lineNumber);
            strncpy(errStrings[errs], tempStr, 199);
        }

        if (errString)
            strncat(errStrings[errs], errString, 199-strlen(errStrings[errs]));
    }
    else if (errs == MAXERRS)
        strcpy(errStrings[(MAXERRS-1)], Message[TOOMANYERRSMSG]);
    errs++;
	    
} // collectErrs


void
NetGraph::openDialogBox(void) {
    if (fatalError && quitNow)
	dialog->setCallBack(new NetGraphCallBack(this, NetGraph::immediateQuit));
    else
	dialog->setCallBack(new NetGraphCallBack(this, NetGraph::closeDialogBox));

    if (dialog->isMapped())
	dialog->pop();
    else {
	dialog->map();
	XFlush(getDpy());
    }
}

void
NetGraph::closeDialogBox(tuGadget *) {
    dialog->unmap();
    XFlush(getDpy());
    if (fatalError)
	quit(NULL);
}

void
NetGraph::closeLicDialogBox(tuGadget *) {
    licDialog->unmap();
    XFlush(getDpy());
}

void
NetGraph::setDialogBoxCallBack(void) {
    dialog->setCallBack(new NetGraphCallBack(this, NetGraph::closeDialogBox));
}

int
NetGraph::openNetFilters() {
    if (netfiltersPid != 0)
	return 1;

    int fd[2];
    if (pipe(fd) != 0) {
	dialog->warning(errno, "Could not start NetFilters");
	return 0;
    }

    netfiltersPid = fork();
    if (netfiltersPid < 0) {
	dialog->warning(errno, "Could not start NetFilters");
	return 0;
    }
    if (netfiltersPid == 0) {
	if (dup2(fd[1], 1) < 0)
	    exit(-1);
	execlp("netfilters", "netfilters", "-stdout", 0);
        perror("netgraph: Could not start NetFilters");
 	exit(-1);
    }
    close(fd[1]);
    netfilters = fdopen(fd[0], "r");
    
    exec->addCallBack(fd[0], False,
	    new NetGraphCallBack(this, NetGraph::readNetFilters));

    
    return 1;
} // openNetFilters

// read the result from NetFilters, tell editControl
void
NetGraph::readNetFilters(tuGadget*) {
    char buf[256];
    if (fgets(buf, 256, netfilters) == 0) {
	closeNetFilters();
	return;
    }
    char* nl = strchr(buf, '\n');
    *nl = NULL;
    // printf("readNetFilters: %s\n", buf);

    editControl->setFilter(buf);
    // make it use new filter, as if user typed it.
    editControl->filterType(NULL);

} // readNetFilters

void
NetGraph::closeNetFilters(void) {
    exec->removeCallBack(fileno(netfilters), False);
    fclose(netfilters);
    kill(netfiltersPid, SIGTERM);
    netfiltersPid = 0;
}


void
NetGraph::protoInit() {
    /*
     * Initialize protocols
     * XXX We must disable NIS temporarily, as protocol init routines
     * XXX tend to call getservbyname, which is horrendously slow thanks
     * XXX to Sun's quadratic-growth lookup botch.
     */
    _yp_disabled = 1;
    initprotocols();
    _yp_disabled = 0;
}

// set up snooping services
int
NetGraph::snoopInit() {

    bzero(&hist, sizeof hist);
    bzero(&oldHist, sizeof oldHist);

    if (!snoopStart(&snpStm)) {
	fatalError = True;
	post(NOSTARTSNOOPMSG);
	return -1;
    }
    
    return 0;
    
} // snoopInit

tuBool
NetGraph::isEther() {
    return (!strcmp(snpStm.ss_rawproto->pr_name, "ether"));
}

tuBool
NetGraph::isFddi() {
    return (!strcmp(snpStm.ss_rawproto->pr_name, "fddi"));
}

tuBool
NetGraph::isTokenRing() {
    return (!strcmp(snpStm.ss_rawproto->pr_name, "tokenring"));
}

void
NetGraph::setExec(tuXExec *e) {
    // printf("NetGraph::setExec\n");
    exec = e;

    if (fatalError)
	return;
	
    timer = new tuTimer();
    timer->setCallBack(new NetGraphCallBack(this, NetGraph::reallyStart));
    timer->start(1.0);
    // printf("... started 1 second timer\n");
}

void
NetGraph::reallyStart(tuGadget*) {
    // printf("NetGraph::reallyStart\n");

    if (history) {
	//timer = new tuTimer(True);
	timer->setAutoReload(True);
	// printf("setting timer call back\n");
	timer->setCallBack(new NetGraphCallBack(this, NetGraph::historyMove));
	startTimer();
    } else if (snoopInit() != -1) {
	if (snpStm.ss_sock != -1) {
	    exec->addCallBack(snpStm.ss_sock, False,
		new NetGraphCallBack(this, &NetGraph::readData));
	    // printf("setExec - addCallBack(%d)\n", snpStm.ss_sock);
	}
    }
    
}

// handle a string of options, from hist file or rc file.
// If this is called to parse the command line, "fileName" will point
// to a string, and we'll return the name there.
// If it is called to parse commands in the file, "fileName" will be a
// null pointer.
int
NetGraph::handleStringOfOptions(char* str) {

    // first, make like argc/argv
    int argc;
    char *argv[256];
    static char white[] = " \t\n";

    argv[0] = strtok(str, white);
    if (!argv[0])
	return 0;

    if (strcmp(argv[0], "option")) {
	argv[1] = strdup(argv[0]);
	argc = 2;
    } else {
	argc = 1;
    }
    for (;; argc++) {
	argv[argc] = strtok(NULL, white);
	if (argv[argc] == NULL)
	    break;
    }

    // now parse the options
    return handleOptions(argc, argv);

} // handleStringOfOptions


// parse "command line" options, whether from actual command line or from
// the setup file.
int
NetGraph::handleOptions(int argc, char** argv) {
    // printf("NetGraph::handleOptions\n");
    // for (int c = 0; c < argc; c++) printf("  %d:%s\n", c, argv[c]);

    int opt;
    char* options = "i:l:o:t:T:U:A:MOPSarsnh";

    int optionErrs = 0;
    opterr = 0; // disable error messages from getopt
    optind = 1;
    
    tuBool notHistoryYet = !(history);
    while ((opt = getopt(argc, argv, options)) != -1) {	    
	if (notHistoryYet)
	    got[opt] = True;
	else if (got[opt])
	    continue;

	if ((opt == 'a') || (opt == 'r') || (opt == 's') || (opt == 'n')) {
	    if (gotTimeTypeFromResource)
		continue;
	    else if (notHistoryYet)
		gotTimeType = True;
	    else if (gotTimeType)
		continue;
	}

	switch (opt) {
	    case 'i':
		{
		    if (gotInterfaceFromResource)
			continue;
		    strcpy(fullSnooperString, optarg);
		    char* tempStr = strdup(optarg);		
		    fatalError = True; // since might be posted by handleIntfc()
		    quitNow = True;
		    if (handleIntfc(tempStr, True) == -1)
			return -1;
		    fatalError = False; // return this to normal value
		    quitNow = False;
		    snooperChanged();
		}
		break;
		
	    case 'o':
		strcpy(recordFile, optarg);
		recording = True;
		break;
	    case 'O':
	    	outputAscii = True;
		break;
	    case 'l':
		strcpy(logFile, optarg);
		break;
	    case 't':
		// turn into integer # of tenths and round off
		interval = (int) (atof(optarg) * 10.0 + 0.5 );
		break;
	    case 'T':
		// turn into integer # of tenths and round off
		period  = (int) (atof(optarg) * 10.0 + 0.5 );
		break;
	    case 'U':
		// turn into integer # of tenths and round off
		updateTime  = (int) (atof(optarg) * 10.0 + 0.5 );
		break;
	    case 'A':
		// turn into integer # of tenths and round off
		avgPeriod = (int) (atof(optarg) * 10.0 + 0.5 );
		break;
	    case 'h':
		usage();
		return -1;
	    case 'a':
		timeType = TIME_ABSOLUTE;
		break;
	    case 'n':
		timeType = TIME_NONE;
		break;
	    case 'r':
		timeType = TIME_RELATIVE;
		break;
	    case 's':
		timeType = TIME_SCROLLING;
		break;
	    case 'M':
		maxValues = True;
		break;
	    case 'P':
		lockPercentages = True;
		break;
	    case 'S':
		sameScale = True;
		break;

	    case '?':
		// If it's a valid option for us, must be a bad argument
		if (strchr(options, optopt) != NULL) {
		    char tmpStr[80];
		    char *msgs[1];
		    msgs[0] = tmpStr;
		    sprintf(tmpStr,
			"-%c option requires an argument", optopt);
		    fatalError = True;
		    quitNow = True;
		    postMany(1, msgs);
		    return -1;
		}
		optionErrs++;
		break;
	} // switch opt
    } // while more opts

    if (optionErrs) {
	usage();
	return -1;
    }

    // Parse filter expression
    if (optind != argc) {
	char buf[1024];
	char *f = buf;
	for (int i = optind; i < argc; i++) {
	    if (f != buf)
		*f++ = ' ';
	    strcpy(f, argv[i]);
	    f += strlen(f);
	}
	specifiedFilter = new char[strlen(buf) + 3];
	sprintf(specifiedFilter, "\"%s\"", buf);
    }

    return 0;
} // handleOptions (argc,argv)


// when running from history file, some options are illegal, some meaningless.
// Turn these off if they're set, and post warning (not a fatal error).
// Don't worry about -t and -U; just ignore them (automatically, since
// will read them from hist file after call this function).
// Here, just undo 'recording' if set, and post warning about that.
void
NetGraph::undoIllegalOptions() {
    // doesn't make sense to read in & write out history simultaneously
    if (recording) {
	collectErrs(HISTNORECORDMSG);
	recording = False;
	if (rfp)
	    close_xdr_file(rfp, &xdr);
    }


} // NetGraph::undoIllegalOptions


void
NetGraph::usage() {
    char msgbuf[300];
    
    strcpy(msgbuf, 
"usage: netgraph [-h] [-M] [-P] [-S] [-arsn] [-u controls_file] [-i interface] ");
    strcat(msgbuf, 
"[-t time_interval] [-T time_period] [-U update_time] [-A average_period] ");
    strcat(msgbuf, 
"[-o history_output_file] [-O] [-l alarm_log_file] [-y] [filter]");
    
    fatalError = True;
    quitNow = True;
    
       	usageDialog = new DialogBox("NetGraph", viewGadget, False);
	usageDialog->setCallBack(new NetGraphCallBack(this, NetGraph::immediateQuit));
        usageDialog->information(msgbuf);
	usageDialog->map();
	XFlush(getDpy());

    
    
    // dialog->information(msgbuf);
    // openDialogBox();
    
} // usage


// if the snooper node or interface changed, this is called.
void
NetGraph::snooperChanged() {
    // printf("snooperChanged; new: <%s>:<%s>; current: <%s>:<%s>\n", 
    // newSnooper.node, newSnooper.interface, currentSnooper.node, currentSnooper.interface);

    strcpy(currentSnooper.node, newSnooper.node);
    strcpy(currentSnooper.interface, newSnooper.interface);


    if (fullSnooperString[0]) {
	sprintf(windowName, "NetGraph -i %s", fullSnooperString);
	if (!history)
	    toolOptions->setInterface(fullSnooperString);
    } else {
	char tempStr[80];
	sprintf(tempStr, "%s:", localHostName); // so analyzer is happy
	sprintf(windowName, "NetGraph -i %s", tempStr);
	toolOptions->setInterface(tempStr);
    }

    viewGadget->setName(windowName);
    
}




// take an interface name, figure out what kind of snooping is required,
// fill in newSnooper struct
int
NetGraph::handleIntfc(char* name,  tuBool allowTrace) {
    // printf("handleIntfc: name: %s\n", name);
    if (name)
	strcpy(fullSnooperString, name);
    else
	fullSnooperString[0] = NULL;
    
    char* newInterface;
    
    enum snoopertype snptype = getsnoopertype(name, &newInterface);
    // printf("after getsnoopertype: name: %s, newInterface: %s\n", name, newInterface);

    // printf("handleIntfc: newSnooper.node (%s), newSnooper.interface (%s)\n", 
    //	newSnooper.node,  newSnooper.interface);

    strcpy(newSnooper.node, name);

    switch (snptype) {
	case ST_LOCAL:
	    // strcpy(name, localHostName);
	    strcpy(newSnooper.node, localHostName);

	    break;
	case ST_TRACE:
	    // printf("snoopertype = ST_TRACE\n");
	    if (allowTrace || history) {
		history = True;
		sprintf(windowName, "NetGraph -i %s", name);
		viewGadget->setName(windowName);
	    } else {
		post(NOHISTMSG);
		return -1;
	    }
	    break;
	case ST_REMOTE:
	    if (!newInterface)
		newInterface = strdup(name);
//	    printf("ST_REMOTE: name(%s), hp->name(%s)\n", 
//		name, (*hpp)->h_name);
	    break;
	case ST_NULL:
	    post(ILLEGALINTFCMSG, name);
	    return -1;
	default:
	    post(BADINTFCMSG, name);
	    break;
    }

    if (newInterface)
	strcpy(newSnooper.interface, newInterface);
    else
	newSnooper.interface[0] = NULL;

    // printf("handleIntfc returned ok\n");

    return 0;
} // handleIntfc





// vvvv from old netgraph.c++

/* Build the "option" line for the rc file and history file.
 * The 'forConfigFile' parameter is true for the rc file - write all options
 * into the line; and false for the history file, since we don't want to
 * write -l & -U there. 
 * The -f option is never written into this option line, since by the time
 * it would be read, it's always too late to matter.
 * The -o option is not written into this option line, since you can't save
 * to the rc file if you're running -o, and it doesn't make sense to say
 * -o in the history file, since the history is already in a file.
 */
void
NetGraph::buildOptLine(char* line, tuBool forConfigFile) {
    char tempStr[80];

    strcat(line, "option ");
    if (maxValues)
	strcat(line, "-M ");
    if (lockPercentages)
	strcat(line, "-P ");
    if (sameScale)
	strcat(line, "-S ");

    if (fullSnooperString[0]) {
	sprintf(tempStr, "-i %s ", fullSnooperString);
	strcat(line, tempStr);
    }

    sprintf(tempStr, "-t %.1f ", interval/10.0);
    strcat(line, tempStr);

    sprintf(tempStr, "-T %.1f ", period/10.0);
    strcat(line, tempStr);

    sprintf(tempStr, "-A %.1f ", avgPeriod/10.0);
    strcat(line, tempStr);

    switch (timeType) {
	case TIME_ABSOLUTE:
	    strcat(line, "-a ");
	    break;
	case TIME_RELATIVE:
	    strcat(line, "-r ");
	    break;
	case TIME_SCROLLING:
	    strcat(line, "-s ");
	    break;
	case TIME_NONE:
	    strcat(line, "-n ");
	    break;
    }

    if (forConfigFile && strcmp(logFile, "")) {
	sprintf(tempStr, "-l %s", logFile);
	strcat(line, tempStr);
	sprintf(tempStr, "-U %.1f ", updateTime/10.0);
	strcat(line, tempStr);
    }

    strcat(line, "\n");

} // NetGraph::buildOptLine


/*
 * Take six tokens (filter expression, type, N, style, color, avgColor),
 * convert the type,
 * style & color into integers, and call another function to setup the strip
 * based on those integers.
 * Last param says whether to post errors immediately(True) or collect them.
 * Return 0 if it worked, -1 if it failed.
 */
int
NetGraph::setupStripTkns(char* filtExpr, char* tokens[], int lineNumber,
    StripGadget* sg, tuBool postNow) {
    int typeInt = 0;
    int baseRate = -1;
    int styleInt;
    int intVal;
    int colorInt = -1;
    int avgColorInt = 1;
    tuBool alarmSet = False;
    tuBool alarmBell = False;
    float floatVal =  0.0;
    float alarmLo  = -1.0;
    float alarmHi  =  0.0;
    int retVal = 0;

    // if no style is specified in the rc file, then the -L param matters.
    if (useLines)
	styleInt = 1;
    else
	styleInt = 0;

    // Just in case they're in the wrong order in the file, we'll check
    // all 9 (max) tokens against all kinds of tokens.
    for (int i = 0; i < 9; i++) {
	if (tokens[i] && 
	    ((retVal = matchToken(tokens[i], &typeInt,
		&intVal, &styleInt,
		&alarmSet, &alarmBell, &floatVal)) < 0)) {

//printf("tokens[%d]: <%s>, retVal: %d\n", i, tokens[i], retVal);

	    if (postNow)
		post(ILLEGALTOKENMSG, tokens[i], /* lineNumber */);
	    else
		collectErrs(ILLEGALTOKENMSG, tokens[i], lineNumber);
	} else {
	    if (retVal == FLOAT_TKN) {
		// if a float, alarmLoVal, then alarmHiVal
		if (alarmLo == -1.0)
		    alarmLo = floatVal;
		else
		    alarmHi = floatVal;
		    
	    } else if (retVal == INT_TKN) {
		// if an int, possibly baseRate, then color, then avgColor
		if ((typeInt == TYPE_PNPACKETS || typeInt == TYPE_PNBYTES)
		      && baseRate == -1)
		    baseRate = intVal;
		else if (colorInt == -1)
		    colorInt = intVal;
		else
		    avgColorInt = intVal;
	    }
	}
    }

    // we set this to -1 to keep track of whether it had been assigned yet,
    // but now make it a reasonable value if it hasn't been assigned.
    if (alarmLo == -1.0)
	alarmLo = 0.0;

    // same with baseRate
    if (typeInt == TYPE_PNPACKETS && baseRate == -1)
	baseRate = 1000;
    else if (typeInt== TYPE_PNBYTES && baseRate == -1)	  
	baseRate = 10000;

    // same with colorInt vs avgColorInt
    if (colorInt == -1)
	colorInt = 4;

    retVal = setupStripInts(filtExpr, typeInt, baseRate,
		    colorInt, avgColorInt, styleInt,
		    alarmSet, alarmBell, alarmLo, alarmHi,
		    lineNumber, sg, postNow);
    return(retVal);
} // netgraph::setupStripTkns


// take a token, and try to match it to color (int), %n base rate (int), 
// graph Type, graphStyle,
// alarmSet (no or yes), alarmBell (silent or bell), or alarmVal (float).
// return value tells what it is (-1=none, 0=type, 1=style, 2=int,
// 3=alarmSet (bool), 4=alarmBell (bool), 5=float) and
// 'fooVal' is the corresponding integer value.
int
NetGraph::matchToken(char* token,
     int* typeIntp, int* intp, int* styleIntp,
     tuBool* alarmSetp, tuBool* alarmBellp, float* floatValp) {

    if (isdigit(*token)) {
	// if has a decimal point, it's a float, so alarm value (lo or hi).
	// if not, then it's an int, so color.
	if (strchr(token, '.')) {
	    *floatValp = atof(token);
	    return FLOAT_TKN;
	} else {
	    *intp = atoi(token);
	    return INT_TKN;
	}
    }

    for (int i = 0; i < strlen(token); i++) {
	if (isupper(token[i]))
	    token[i] = _tolower(token[i]);
    }

    for (i = 0; graphTypes[i] != 0; i++) {
	if (strcmp(token, graphTypes[i]) == 0) {
	    *typeIntp = i;
	    return TYPE_TKN;
	}
    }

    for (i = 0; graphStyles[i] != 0; i++) {
	if (strcmp(token, graphStyles[i]) == 0) {
	    *styleIntp = i;
	    return STYLE_TKN;
	}
    }

    for (i = 0; alarmSets[i] != 0; i++) {
	if (strcmp(token, alarmSets[i]) == 0) {
	    *alarmSetp = i;
	    return ALARMSET_TKN;
	}
    }

    for (i = 0; alarmBells[i] != 0; i++) {
	if (strcmp(token, alarmBells[i]) == 0) {
	    *alarmBellp = i;
	    return ALARMBELL_TKN;
	}
    }

    return BAD_TKN;


} // netgraph::matchToken



/*
 * Take filter string, integer graph type, integer graph style and integer
 * color, and set up a
 * new strip. This is called when adding a strip from data at startup
 * (in .netgraphrc, etc.), as well as when adding a strip on the fly.
 * Last param says whether to post errors immediately(True) or collect them.
 * Return 0 if it worked, -1 if it failed.
 */
int
NetGraph::setupStripInts(char* filtExpr, int typeInt, int baseRate, 
    int colorInt, int avgColorInt, int styleInt,
    tuBool alarmSet, tuBool alarmBell, float alarmLo, float alarmHi,
    int lineNumber, StripGadget* sg, tuBool postNow) {

    // if sg pointer is bad, can't do anything
    if (!sg) {
	if (postNow)
	    post(BADSTRIPMSG);
	else
	    collectErrs(BADSTRIPMSG);
	return -1;
    }


    // if this is an existing strip, and neither the type nor the filtExpr
    // changed, just change the color and/or style and get out of here.
    if ((sg->getBin() >= 0) &&
	(typeInt == sg->getType()) &&
	(sg->getBaseRate() == baseRate) &&
	!strcmp(filtExpr, filters[sg->getBin()].expr)) {
	    sg->setColor(colorInt);
	    sg->setAvgColor(avgColorInt);
	    sg->setStyle(styleInt);
	    return 0;
	}


    // if this filter expression matches one we already saw, don't
    // bother adding it.
    for (int bin = 0; bin <= highestBin; bin++) {
	// check against each existing
	if (!strcmp (filtExpr,filters[bin].expr)) {
	    // a match, so current bin is the bin the new strip will use
	    break;
	}
    } // (check existing filters)

    if (bin > highestBin) {
	ExprSource src;
	ExprError err;
        // it didn't match, so we have to add it.
	if (strcmp(filtExpr, "total")) {
	    src.src_path = "netgraph";
	    src.src_line = 0;
	    src.src_buf = filtExpr;
	    // printf("adding %s\n", filtExpr);
	    bin = ss_compile(&snpStm, &src, &err);
	    // printf("bin %d: %s\n", bin, filtExpr);
	} else {
	    bin = ss_add(&snpStm, 0, &err);
	    // of course, this should never happen, since we added a
	    // promiscuous filter up there in appInit, right after hist_init
	}

	if (bin < 0) {

	    if (postNow) {
		char* msgs[2];
		msgs[0] = (char*) malloc(200);
		strcpy(msgs[0], "Invalid filter --  ");
		if (err.err_message)
		    strcat(msgs[0], err.err_message);
		if (lineNumber) {
		    char lineStr[80];
		    sprintf(lineStr, " on line %d", lineNumber);
		    strncat(msgs[0], lineStr, 199-strlen(msgs[0]));
		}
		if (err.err_token && (err.err_token[0] != '\0')) {
		    strcat(msgs[0], ": ");
		    strncat(msgs[0], err.err_token, 199-strlen(msgs[0]));
		    // msgs[1] = err.err_token;
		    // postMany(2, msgs);
		    postMany(1, msgs);
		} else {
		    postMany(1, msgs);
		}
    
	    }
	    else {
		char tempStr[80];
		strcpy(tempStr, "Invalid filter -- ");
		if (err.err_message)
		    strcat(tempStr, err.err_message);
		if (lineNumber) {
		    char lineStr[80];
		    sprintf(lineStr, " on line %d", lineNumber);
		    strncat(tempStr, lineStr, 199 - strlen(tempStr));
		}
		if (err.err_token && (err.err_token[0] != '\0')) {
		    strcat(tempStr, ": ");
		    strcat(tempStr, err.err_token);
		}
	        collectErrs(-1, tempStr);
	    }

	    return (-1);
	} // if bin < 0
	highestBin = bin;
	strcpy(filters[bin].expr, filtExpr);
    } // (didn't match any already added filter)

    filters[bin].used++;

    // zero the y data
    dataStruct* ptr = sg->getCurrentData();
    sg->setAvgData(ptr);
    sg->setSamplesSoFar(0);
    for (int i = 0; i < samples; i++) {
	ptr->y = 0;
	ptr->avg = 0;
	ptr = ptr->next;
    }


    // Set up this strip chart
    sg->setBin(bin);

    finishSetupStripInts(filtExpr, typeInt, baseRate,
	colorInt, avgColorInt, styleInt,
	alarmSet, alarmBell, alarmLo, alarmHi, sg);

    return (0);
} // netgraph::setupStripInts


// Finish setting up the strip.
// This is broken off from setupStripInts because it is needed for
// when we're running from a history file, too.
void
NetGraph::finishSetupStripInts(char* filtExpr, int typeInt, int baseRate,
    int colorInt, int avgColorInt, int styleInt,
    tuBool alarmSet, tuBool alarmBell,
    float alarmLo, float alarmHi, StripGadget* sg) {

    // printf("finishSetupStripInts(%s, %d, %d, %d, %d, %d)\n", 
    //     filtExpr, typeInt, baseRate, colorInt, avgColorInt, styleInt);
	
    sg->setColor(colorInt);
    sg->setAvgColor(avgColorInt);
    sg->setType(typeInt);
    sg->setStyle(styleInt);
    sg->setAlarmSet(alarmSet);
    sg->setAlarmBell(alarmBell);
    sg->setAlarmLoVal(alarmLo);
    sg->setAlarmHiVal(alarmHi);
    if (typeInt == TYPE_PNPACKETS || typeInt == TYPE_PNBYTES)
	sg->setBaseRate(baseRate);


    sg->setFilter(filtExpr);
    sg->setType(typeInt);

    if (lockPercentages && typeInt > TYPE_BYTES) {
	// Lock in percentages
	sg->setScaleType(SCALE_CONSTANT);
    }

    // reset max, current and avg values
    sg->zeroMaxVal();
    sg->setCurVal(0.0);
    sg->setAvgVal(0.0);

} // netGraph:finishSetupStripInts

int
NetGraph::makeNewStrip(StripGadget** sgp, StripGadget* belowSg) {
    // printf("NetGraph::makeNewStrip; now has %d rows\n", stripParent->getNumRows());

    stripParent->grow(stripParent->getNumRows()+1, stripParent->getNumCols());
    StripGadget* newStrip = new StripGadget(this, stripParent, "");

    int row, col;

    if (belowSg) {
	stripParent->findChild(belowSg, &row, &col);
	// printf("'belowSg' is at row %d\n", row);	
	// move others down to make room for new one
	for (int r = stripParent->getNumRows()-2; r >= row; r--) {
	    tuGadget* g = stripParent->getChildAt(r, col);
	    stripParent->place(g, r+1, col);
	}
    } else {
	row = stripParent->getNumRows();
	col = 0;
	if (timeGadget)
	    row-=2;
	else
	    row-=1;
	stripParent->place(newStrip, row, col);
    }

    // printf("new strip placed at row %d of %d\n", row, stripParent->getNumRows());
    // if (belowSg) {
	// stripParent->findChild(belowSg, &row, &col);
	// printf("'belowSg' is NOW at row %d\n", row);
    // }
    *sgp = newStrip;
    
    return completeNewStrip(newStrip, belowSg);
} // NetGraph::makeNewStrip


// takes a fairly empty StripGadget, fills it in the rest of the way,
// adds it to the strip list and to the window
int
NetGraph::completeNewStrip(StripGadget* sg, StripGadget* belowSg) {
    // printf("NetGraph::completeNewStrip\n");
    sg->setNumber(++highestNumber);

    if (maxValues)
	sg->setScaleType(SCALE_MAXVALUE);

    if (history) {
	if (strips) {
	    for (StripGadget *x = strips; x->getNext() != NULL;
		    x = x->getNext())
		;
	    x->setNext(sg);
	} else
	    strips = sg;
	sg->setNext(NULL);
    } else {
	// set up a circularly linked list for the currentData
	dataStruct* ptr;
	if (freeDataList) {
	    ptr = freeDataList->data;
	    freeDataList = freeDataList->next;
	} else if (setupCircData(&ptr, samples) == -1) {
	    fatalError = True;
	    return -1;
	}

	sg->setHeadData(samples, ptr);
	sg->setCurrentData(ptr);
	sg->setAvgData(ptr);


	if (belowSg) {
	    sg->setNext(belowSg->getNext());
	    belowSg->setNext(sg);
	} else {
	    sg->setNext(strips);
	    strips = sg;
	}
    }


    sg->setBin(-1);

    // Add it to the window.
    viewGadget->addAGraph(sg);


    return 0;
} // NetGraph::completeNewStrip

void
NetGraph::setSameScale(tuBool newVal) {
    sameScale = newVal;
}

void
NetGraph::setLockPercentages(tuBool newVal) {
    lockPercentages = newVal;
    for (StripGadget* sg = strips; sg != 0; sg = sg->getNext()) {
    
	if (lockPercentages && (sg->getType() > TYPE_BYTES)) {
	    // Lock in percentages
	    sg->setScaleType(SCALE_CONSTANT);
	} else if (maxValues)
	    sg->setScaleType(SCALE_MAXVALUE);
	else
	    sg->setScaleType(SCALE_VARIABLE);

    }
}

void
NetGraph::setMaxVals(tuBool newVal) {
    maxValues = newVal;
    for (StripGadget* sg = strips; sg != 0; sg = sg->getNext()) {
	if (!(lockPercentages && (sg->getType() > TYPE_BYTES))) {
	    if (maxValues)
		sg->setScaleType(SCALE_MAXVALUE);
	    else
		sg->setScaleType(SCALE_VARIABLE);
	}
    }
}

void
NetGraph::setTimeType(int t) {
    viewGadget->newTimeArgs(timeGadget, period, interval, t);
    if (history) {
	scrollGadget->unmap();
	scrollGadget->map();
    }
}

// this is called from paramControl to send all the args at once
int
NetGraph::handleNewArgs(Arg* newArgs) {

    int newPeriod = newArgs->period;
    int newAvgPeriod = newArgs->avgPeriod;
    int newInterval = newArgs->interval;
    int newUpdateTime = newArgs->updateTime;
    char* newName;
    if (!newArgs->interface ||
	 newArgs->interface && (!strcmp(newArgs->interface, ""))) {
	newName = (char*) malloc (strlen(localHostName) + 1);
	sprintf(newName, "%s:", localHostName);
    } else
	newName = strdup(newArgs->interface);
	
    int newTimeType = newArgs->timeType;
    maxValues = (tuBool)newArgs->maxValues;
    lockPercentages = (tuBool)newArgs->lockPercentages;
    sameScale = (tuBool)newArgs->sameScale;
    updateTime = newArgs->updateTime;


    for (StripGadget* sg = strips; sg != 0; sg = sg->getNext()) {
	if (lockPercentages && (sg->getType() > TYPE_BYTES))
	    sg->setScaleType(SCALE_CONSTANT);
	else if (maxValues)
	    sg->setScaleType(SCALE_MAXVALUE);
	else
	    sg->setScaleType(SCALE_VARIABLE);

    }

    /**
    printf("handleNewArgs: newName: %s, fullSnooperString: %s\n", 
    newName, fullSnooperString);
    printf("handleNewArgs 1; new: <%s>:<%s>; current: <%s>:<%s>\n", 
    newSnooper.node, newSnooper.interface, currentSnooper.node, currentSnooper.interface);
    **/
    
    // if there was a *different* interface specified, handle it.
    tuBool interfaceChanged = False;
    char oldFullSnooperString[100];
    strcpy(oldFullSnooperString, fullSnooperString);
    if (!history && strcmp(newName, fullSnooperString)) {
	interfaceChanged = True;
	int retVal = handleIntfc(newName, False);
	delete newName;
	if (retVal == -1) {
	    strcpy(fullSnooperString, oldFullSnooperString);
	    paramControl->setInterfaceField(fullSnooperString);
	    return -1;
	}
    }

    
    /**
    printf("handleNewArgs 2; new: <%s>:<%s>; current: <%s>:<%s>\n", 
    newSnooper.node, newSnooper.interface, currentSnooper.node, currentSnooper.interface);
    **/
    // Must restart snooper if not running now,
    // or if either the interval or the interface changed.
    // interface changed if:
    //	  both pointers are non-nil, and strings are different, OR
    //    one pointer is nil, the other isn't (& must point to a non-blank)

    tuBool mustRestart = (!history) && (
	(snpStm.ss_sock == -1) ||
	(newInterval != interval) ||
	(interfaceChanged));

    if (useNewTimeArgs(newPeriod, newInterval, newUpdateTime, newAvgPeriod,
	    newTimeType, mustRestart) == -1)
	return -1;

    if (mustRestart) {
	viewGadget->render();
	
	if (restartSnooping(newInterval) == -1) {
	    strcpy(fullSnooperString, oldFullSnooperString);
	    paramControl->setInterfaceField(fullSnooperString);
	    return -1;
	}
    }

    return 0;

} // NetGraph::handleNewArgs


int
NetGraph::useNewTimeArgs(int newPeriod, int newInterval, int newUpdateTime,
    int newAvgPeriod, int newTimeType, tuBool willRestart) {
    
    int newSamples;
    int newAvgSamples;

    if (checkTimeArgs(&newSamples, &newPeriod, &newInterval, &newUpdateTime,
	    &newAvgPeriod, &newAvgSamples) == -1) {
	return -1;
    }

    int i;
    StripGadget *sg;
    dataStruct *ptr;

  if (history) {
    if (newSamples != samples) {
	// try to keep right edge fixed.
	// If this puts left edge past the end (because
	// we were showing more samples than in file), place left edge such
	// that we see the last part of the file.
	// If it puts left end before beginning, put left end *at* beginning.
	long* newTimePtr = timePtr + sampleSize * (samples - newSamples);
	if (newTimePtr >= historyEnd)
	    newTimePtr = (long*)historyEnd - sampleSize * newSamples;

	if (newTimePtr < historyBeg)
	    timePtr = (long*) historyBeg;
	else
	    timePtr = newTimePtr;

	float* dataPtr = (float*) (timePtr + 2);

	for (sg = strips; sg != 0; sg = sg->getNext(), dataPtr++) {
	    sg->setNumPoints(newSamples);
	    sg->setCurrentData((dataStruct*)dataPtr);
	    sg->rescale();
	}

	historyTicks = (timePtr - (long*)historyBeg) / sampleSize;

	float percShown = ((float)newSamples / historySamples);
	if (percShown > 1.0) {
	    percShown = 1.0;
	    scrollGadget->setScrollPerc(0.0);
	} else {
	    scrollGadget->setScrollPerc(
		historyTicks / (float) (historySamples - newSamples));
	}
	
	scrollGadget->setScrollPercShown(percShown);

    } // numSamples != samples
    
    if (newAvgSamples != avgSamples && newAvgSamples != 0) {
	// need to recalc all averages
	int samplesSoFar;
	float *fPtr, *tempPtr;

	for (sg = strips; sg != 0; sg = sg->getNext(), fPtr++) {
	    sg->setAvgVal(0.0);
	    sg->setAvgData(sg->getHeadData());
	    
	    // step through data
	    for (samplesSoFar = 1,  fPtr = (float*) (sg->getHeadData());
		    fPtr < historyEnd;
		    samplesSoFar++, fPtr += sampleSize) {
		if (samplesSoFar > newAvgSamples) {
		    *(fPtr + 1) = sg->getAvgVal() +
	    	    (*fPtr - *(float*)(sg->getAvgData())) / newAvgSamples;

		    tempPtr = (float*) sg->getAvgData();
		    tempPtr += sampleSize;
		    sg->setAvgData((dataStruct*)tempPtr);
		    sg->setAvgVal(*(fPtr + 1));
	    
		} else  {
		    *(fPtr + 1) = ((sg->getAvgVal()) * (samplesSoFar - 1) +
	    	    	(*fPtr)) /
			 samplesSoFar;
		    sg->setAvgVal(*(fPtr + 1));
		}
	    
	    } // for each sample 
	    
	} // for each strip
	
    } // avgSamples changed
    
  } else {
    // (not history)
    // If # of samples didn't change, and we're restarting, clear data.
    // If # of samples changed (because of -t or -T), reallocate memory
    // for each strip; if we're not restarting, copy data.
    if (newSamples == samples) {
    
	if (willRestart) {
	    // printf("CLEARING DATA -- samples didn't change\n");
	    for (sg = strips; sg != 0; sg = sg->getNext()) {
		for (ptr = sg->getCurrentData(), i = 0;
			i < samples; ptr = ptr->next, i++) {
		    ptr->y = 0;
		    ptr->avg = 0;
		}
		sg->zeroMaxVal();
		sg->setCurVal(0.0);
		sg->setAvgVal(0.0);
		sg->setSamplesSoFar(0);
		sg->setAvgData(sg->getCurrentData());
	    }
	} // willRestart
    } else {

	// first free up everything that might be in freeDataList
	for (dataStruct2* ptr2 = freeDataList; ptr2 != 0; ptr2 = ptr2->next) {
	    if (ptr2->data)
		free((void*) ptr2->data);
	}
	freeDataList = NULL;	

	// temporarily stop snooping while we're moving the data
	if (snpStm.ss_sock != -1)
	    (void) snoopStop(&snpStm);


	// now go through strips, and reallocate memory
	dataStruct *newPtr, *oldPtr, *ptr;
	// if (willRestart)
	// printf("CLEARING DATA -- number of samples changed\n");

	for (sg = strips; sg != 0; sg = sg->getNext()) {
	    if (setupCircData(&newPtr, newSamples) == -1)
		return -1;

	    // if we're going to restart the snooper,
	    // we want data area to be all zeros, and maxVal = 0;
	    // if we won't be restarting the snooper,
	    // copy data up to minimum size of old & new areas, but:
	    // if have fewer samples now, be sure to copy the most recent ones,
	    // if have more now, be sure to copy to the most recent slots.
	    if (willRestart) {
		sg->zeroMaxVal();
		sg->setCurVal(0.0);
		sg->setAvgVal(0.0);
		sg->setSamplesSoFar(0);
		sg->setAvgData(sg->getCurrentData());

	    } else {
		ptr = newPtr;
		oldPtr = sg->getCurrentData();
		if (newSamples < samples)
		    for (i = 0; i < (samples - newSamples); i++) 
		    	oldPtr = oldPtr->next;
		else if (newSamples > samples)
		    for (i = 0; i < (newSamples - samples); i++) 
			ptr = ptr->next;

		int toCopy = (samples < newSamples) ? samples : newSamples;
		for (i=0; i < toCopy; i++, ptr=ptr->next, oldPtr=oldPtr->next)
		    ptr->y = oldPtr->y;
		    ptr->avg = oldPtr->avg;
	    }
	    // set avgData pointer, too, to newAvgSamples behind currentData
	    // which is the same as (newSamples - newAvgSamples) ahead.
	    ptr = newPtr;
	    for (i=0; i < (newSamples - newAvgSamples); i++)
		ptr = ptr->next;
	    sg->setAvgData(ptr);
    
	    // free old data area
	    free(sg->getHeadData());

	    // set sg data area to new area
	    sg->setHeadData(newSamples, newPtr);
	    sg->setCurrentData(newPtr);
	    sg->rescale();

	} // for each strip

	if (snpStm.ss_sock != -1) {
	    (void) snoopStart(&snpStm);
	}

    } // if newSamples != samples

  } // if !history


    // tell timeGadget if period, interval or timeType changed
    if ((newPeriod != period) || (newInterval != interval) ||
	   (newTimeType != timeType)) {
	viewGadget->newTimeArgs(timeGadget, newPeriod, newInterval,
				newTimeType);

	if (history) {
	    samples = newSamples;
	    updateHistory();
	}
	else
	    viewGadget->render();
    }

    // if changed avgPeriod (& therefore avgSamples), have to start
    // calculating averages over from scratch.
    if (newAvgSamples != avgSamples && newAvgSamples != 0) {
	for (sg = strips; sg != 0; sg = sg->getNext()) {
	    sg->setSamplesSoFar(0);
	    sg->setAvgVal(0.0);
	    sg->setAvgData(sg->getCurrentData());
	}
    }

    timeType = newTimeType;
    samples = newSamples;
    avgSamples = newAvgSamples;
    period = newPeriod;
    avgPeriod = newAvgPeriod;
    interval = newInterval;
    updateTime = newUpdateTime;

    drawData();
    
    return 0;

} // NetGraph::useNewTimeArgs()


int
NetGraph::checkTimeArgs(int* newSamplesp, int* newPeriodp,
    int* newIntervalp, int* newUpdateTimep,
    int* newAvgPeriodp, int* newAvgSamplesp) {

    int newInterval = *newIntervalp;
    int newPeriod   = *newPeriodp;
    int newUpdateTime = *newUpdateTimep;
    int newAvgPeriod  = *newAvgPeriodp;
    
    if (*newIntervalp < 0) {
	// post(NEGINTVLMSG);
	// return -1;
	*newIntervalp = -1 * *newIntervalp;
    } else if (*newIntervalp == 0)
	*newIntervalp = DEF_INTERVAL;

    if (*newPeriodp < 0) {
	// post(NEGPERIODMSG);
	// return -1;
	*newPeriodp = -1 * *newPeriodp;
    } else if (*newPeriodp == 0)
	*newPeriodp = 600;

    *newSamplesp = *newPeriodp / *newIntervalp + 1;
    if (*newSamplesp <= 1) {
	// post(BADPERIODMSG);
	// return -1;
	*newPeriodp =  *newIntervalp;
	*newSamplesp = 2;
    }
    
    // if update time is smaller than interval, make it same as interval
    // (no longer give error message that it's too small)
    if (*newUpdateTimep < *newIntervalp) {
	*newUpdateTimep = *newIntervalp;
    }

    if (*newPeriodp < *newUpdateTimep) {
	// post(BADUPDTMPERMSG);
	// return -1;
	*newUpdateTimep = *newPeriodp;
    }


    // if 0 < newAvgPeriod < *newIntervalp, make it = 0
    if (*newAvgPeriodp > 0 && *newAvgPeriodp < *newIntervalp ) {
	*newAvgPeriodp = 0;
    }
    
    // if newAvgPeriod > newPeriod , make it = newPeriod
    if (*newAvgPeriodp > 0 && *newAvgPeriodp > *newPeriodp ) {
	*newAvgPeriodp = *newPeriodp;
    }
    
    // if user didn't specify avgPeriod, make it = newPeriod
    if (*newAvgPeriodp < 0) {
	*newAvgPeriodp = *newPeriodp;
    }

    // now use that to calculate how many samples to average over    
    *newAvgSamplesp = *newAvgPeriodp / *newIntervalp;

    // if we changed any here, correct them on paramControl (if open)
    if (paramControl) {
	char tempStr[20];
	if (*newIntervalp != newInterval) {
	    sprintf(tempStr, "%.1f", *newIntervalp/10.0);
	    paramControl->setIntervalField(tempStr);
	}
	if (*newUpdateTimep != newUpdateTime) {
	    sprintf(tempStr, "%.1f", *newUpdateTimep/10.0);
	    paramControl->setUpdateField(tempStr);
	}
	if (*newPeriodp != newPeriod) {
	    sprintf(tempStr, "%.1f", *newPeriodp/10.0);
	    paramControl->setPeriodField(tempStr);
	}
	if (*newAvgPeriodp != newAvgPeriod) {
	    sprintf(tempStr, "%.1f", *newAvgPeriodp/10.0);
	    paramControl->setAvgPeriodField(tempStr);
	}
	
    }
    
    return 0;
} //NetGraph::checkTimeArgs


int
NetGraph::setupCircData(dataStruct** ptrp, int numSamples) {

    *ptrp = (dataStruct*) malloc(numSamples * sizeof(dataStruct));
    if (*ptrp == 0) {
	post(MALLOCERRMSG);
	return -1;
    }
    bzero(*ptrp, numSamples * sizeof(dataStruct));
    dataStruct* ptr2 = *ptrp;
    for (int i = 1; i < numSamples; i++) {
	ptr2->next = ptr2 + 1;
	ptr2++;
    }
    ptr2->next = *ptrp;

    return 0;
} // NetGraph::setupCircData

int
NetGraph::snoopStart(SnoopStream* streamp) {
    if (snooping)
	return 0;
	
    if (!ss_start(streamp))
	return 0;
    
    snooping = True;
    
    event->setType(NV_START_SNOOP);
    event->setInterfaceName(newSnooper.node);
    eh->send(event);
    return 1;
}

int
NetGraph::snoopStop(SnoopStream* streamp) {
    if (!snooping)
	return 0;
	
    if (!ss_stop(streamp))
	return 0;
    
    snooping = False;
    
    event->setType(NV_STOP_SNOOP);
    event->setInterfaceName(currentSnooper.node);
    eh->send(event);
    return 1;
}

int
NetGraph::restartSnooping(int intvl) {
    int retVal, bin;
    SnoopStream newSnpStm;
    // printf("restartSnooping\n");
    /**** already cleared in useNewTimeArgs
    // clear out data area
    printf("CLEARING DATA -- restartSnooping\n");
    dataStruct *ptr;
    int i;
    for (StripGadget* sg = strips; sg != 0; sg = sg->getNext()) {
	for (ptr = sg->getCurrentData(), i = 0;
		i < samples; ptr = ptr->next, i++) {
	    ptr->y = 0;
	    ptr->avg = 0;
	}
	
	sg->zeroMaxVal();
	sg->setCurVal(0.0);
	sg->setAvgVal(0.0);
	sg->setSamplesSoFar(0);
	sg->setAvgData(sg->getCurrentData());
    }
    ********/
    
    bzero(&hist, sizeof hist);
    bzero(&oldHist, sizeof oldHist);
    
    // stop (but don't close) existing snooper, try to start new one.
    // if not successful, then restart existing one.
    // if successful, then delete/close old one, copy new to old, etc..

    // stop existing snooper
    if (snpStm.ss_sock != -1) {
	(void) snoopStop(&snpStm);
    }
    
    // try to start new one
    newSnpStm.ss_sock = -1;

    // printf("about to call hist_init; newSnooper.node: %s, newSnooper.interface:%s\n", 
    //	newSnooper.node, newSnooper.interface);
	
    retVal = hist_init(&newSnpStm, &newSnooper, intvl);
    if (retVal < 0) {
    	// init of new one failed, so restart existing one and post a message
	// snoopStart issues message based on newSnooper
	strcpy(newSnooper.node, currentSnooper.node);
	strcpy(newSnooper.interface, currentSnooper.interface);
	(void) snoopStart(&snpStm);
	post(-1 * retVal);
	return -1;
    } else {
	snooperChanged();
    }

   // set up new snoop requests
    ExprSource src;
    ExprError err;
    for (bin = 0; bin <= highestBin; bin++) {
	if (strcmp(filters[bin].expr, "total")) {
	    src.src_path = "netgraph";
	    src.src_line = 0;
	    src.src_buf = filters[bin].expr;
	    ss_compile(&newSnpStm, &src, &err);
	} else {
	    ss_add(&newSnpStm, 0, &err);
	}
    }

    // start snooping again
    if (!snoopStart(&newSnpStm)) {
    	// start of new one failed, so restart existing one and post a message
	// xxx shouldn't we delete new one, too???
	// snoopStart issues message based on newSnooper
	strcpy(newSnooper.node, currentSnooper.node);
	strcpy(newSnooper.interface, currentSnooper.interface);
	(void) snoopStart(&snpStm);
	post(NOSTARTSNOOPMSG);
	return -1;
    }
    
    // if successful, then delete/close old one
    if (snpStm.ss_sock != -1) {
	for (bin = highestBin; bin >= 0; bin--)
	    (void) ss_delete(&snpStm, bin);

	(void) ss_unsubscribe(&snpStm);
	ss_close(&snpStm);
	exec->removeCallBack(snpStm.ss_sock);
	// printf("restartSnooping - removeCallBack(%d)\n", snpStm.ss_sock);
    }

    snpStm = newSnpStm;
    
    exec->addCallBack(snpStm.ss_sock, False,
		new NetGraphCallBack(this, &NetGraph::readData));
    // printf("restartSnooping - addCallBack(%d)\n", snpStm.ss_sock);

    // take care of the possibility that we were on an ether
    // and now fddi, etc.
    if (editControl)
	editControl->enableIntfcButtons();

    return 0;
      
} // NetGraph::restartSnooping


// we're going to be running from a history file, so open it, etc.
int
NetGraph::handleHistFile(char* fileName) {
    // test_xdr_read(fileName, &rfp, &xdr);

    // printf("NetGraph::handleHistFile(%s)\n", fileName);
    int highNum, fileLength, memLength;
    char optionStr[80];
    char* optionStrP;

    optionStrP = optionStr;

    int historyVersion = open_xdr_infile(fileName, &rfp, &xdr,
			    &fileLength, &highNum, &optionStrP);
    if (historyVersion == 0) {
	fatalError = True;
	post(BADXDRRDOPENMSG);
	return -1;
    }

    handleStringOfOptions(optionStr); 

    if (checkTimeArgs(&samples, &period, &interval, &updateTime,
		    &avgPeriod, &avgSamples) == -1) {
	fatalError = True;
	return -1;
    }

    highestNumber = 0;

    // setup strips
    int type, baseRate, color, avgColor, style;
    int alarmSet, alarmBell;
    tuBool alarmSetB, alarmBellB;
    float alarmLo, alarmHi;
    char filterExpr[80];
    char* filterExprP;
    filterExprP = filterExpr;

    baseRate = 99; // xxx this isn't in old history files
    avgColor = 0; // xxx this isn't in old history files

    StripGadget* sg;
    StripGadget* lastSg = 0;

    // make parent big enough for all strips
    stripParent->grow(highNum + 2, 1);
    
    for (int stripNum = 1; stripNum <= highNum; stripNum++) {
	if (!do_xdr_stripInfo(&xdr, historyVersion, 
		&filterExprP, &type, &baseRate, 
		&color, &avgColor, &style,
		&alarmSet, &alarmBell, &alarmLo, &alarmHi)) {
	    fatalError = True;
	    post(BADXDRREADMSG);
	    return -1;
	}

	sg = new StripGadget(this, stripParent, "");
	
	// put this new strip in the right place
	// (order in hist file is from bottom to top)
	stripParent->place(sg, highNum - stripNum, 0);
	// printf("strip # %d (of %d) at row %d\n", stripNum, highNum, highNum-stripNum);
	if (completeNewStrip(sg, 0) == -1)
	    return -1;	// completeNewStrips posts its own error, & sets fatal

	// fake 'bin' number so can keep track of filter expressions
	sg->setBin(stripNum);
	strcpy(filters[stripNum].expr, filterExpr);

	    
	alarmSetB = (alarmSet != 0);
	alarmBellB = (alarmBell != 0);
	finishSetupStripInts(filterExpr, type, baseRate,
	    color, avgColor, style, 
	    alarmSetB, alarmBellB, alarmLo, alarmHi, sg);

    }

    // each time sample has: 2 longs for time, 2 floats for each strip
    sampleSize = 2 + 2 * highestNumber;
    
   
    // calculate how much memory we need to read in all the data.
    // We're already past the beginning of the file, which we don't save.

    fileLength -= get_xdr_pos(&xdr);
    
    // Now, based on (2 time + (1 data for each strip)) per sample,
    // calculate how many time samples. Round up, in case got chopped.
    // Also note that fileLength & memLength are in bytes; we want
    // longs and floats, which are both 4 bytes long.
    
    historySamples = (int) ((fileLength / 4.0) / (2 + highNum) + 0.5);

    // for our storage, we need 2 floats per strip per sample, not 1
    // (data & avg, which we'll calculate after reading the data in)

    memLength = historySamples * (2 + 2 * highNum) * 4;
    historyBeg = malloc(memLength);
    bzero(historyBeg, memLength);
    
    float* dataPtr;
    timePtr = (long*) historyBeg;
    dataPtr = (float*) (timePtr + 2); // time needs 2 longs
    // printf("first timePtr: %x; file has %d samples\n", timePtr, historySamples);

    // we don't really need the whole chunk -- there's the part of the file
    // that we already read (setup, etc.) that doesn't get copied in.

    historyEnd = (void*) ((char*)historyBeg + memLength);

    // set up the pointers for each strip (where data starts for each)
    for (sg = strips; sg != 0; sg = sg->getNext()) {
	sg->setHeadData(samples, (dataStruct*) dataPtr);
	sg->setCurrentData((dataStruct*) dataPtr);
	sg->setAvgData((dataStruct*) dataPtr);
	sg->setAvgVal(0.0);
	
	dataPtr += 2; // room for sample + avg
    }
    
        
    // now read in the rest of the historical data
    int ok;
    struct timeval timestamp;
    
    int samplesSoFar = 0;	// for calculating averages
    float* fPtr;
    
    ok = do_xdr_time(&xdr, &timestamp);
    while (ok) {
	*timePtr++ = timestamp.tv_sec;
	*timePtr++ = timestamp.tv_usec;
	dataPtr    = (float*) timePtr;	// first data goes right after time

	samplesSoFar++;

	// read in data; advance dataPtr by 2, so room to put avg
	for (sg = strips; sg != 0; sg = sg->getNext(), dataPtr+=2) {
	    if (!do_xdr_data(&xdr, dataPtr))
		break;
	    
	    // calc average
	    if (avgSamples != 0) {
		if (samplesSoFar > avgSamples) {
		    *(dataPtr + 1) = sg->getAvgVal() +
	    		(*dataPtr - *(float*)(sg->getAvgData()) ) / avgSamples;

		    fPtr = (float*) sg->getAvgData();
		    fPtr += sampleSize;
		    sg->setAvgData((dataStruct*)fPtr);
		    sg->setAvgVal(*(dataPtr + 1));
	    
		} else  {
		    *(dataPtr + 1) = ((sg->getAvgVal()) * (samplesSoFar - 1) +
	    	    	(*dataPtr)) /
			 samplesSoFar;
		    sg->setAvgVal(*(dataPtr + 1));
		}
	    } // need to calc average

	} // for each strip
		    
	timePtr = (long*) dataPtr; // next time goes right after last data

	ok = do_xdr_time(&xdr, &timestamp);
	
    } // while ok ( each time sample)

    timePtr = (long*) historyBeg;
    historyTicks = 0;

    for (sg = strips; sg != 0; sg = sg->getNext()) {
	// printf("%d: %s\n", sg->getNumber(), filters[sg->getBin()].expr);
	sg->rescale();
    }

    timerSeconds = 0.5;

    return 0;

} // NetGraph::handleHistFile


void
NetGraph::historyMove(tuGadget*) {
    // printf("NetGraph::historyMove\n");
    
    if (historyDirection == 0)
	return;


    
    long* timePtr1; //  where timePtr would be if stepped back
    long* timePtr2; //  where right end would be if stepped forward

    // need room for data & avgData
    timePtr1 = timePtr - sampleSize;
    timePtr2 = timePtr + sampleSize * samples;
    
    if (((historyDirection == -1) && (timePtr1 <  historyBeg))
     || ((historyDirection ==  1) && (timePtr2 >= historyEnd))) {
	historyDirection = 0;
//xxx	scrollGadget->zeroThrottle();
	viewGadget->render();
	return;
    }

    timePtr = timePtr + historyDirection * sampleSize;
    historyTicks += historyDirection;
    scrollGadget->setScrollPerc((float)historyTicks /
	(float)(historySamples - samples));

    updateHistory();
} // NetGraph::historyMove


void
NetGraph::updateHistory() {

    float* dataPtr = (float*) (timePtr + 2);

    // For same scale option
    for (int i = 0; i < NUM_GRAPH_TYPES; i++)
	max[i] = 0.0;


    int increment; // how far from dataPtr to "current data" (right end)
    // for each time point, have 2 time vals, + data & avg for each strip
    increment = sampleSize * (samples - 1);

    tuBool pastEnd = (timePtr + increment >= historyEnd);

    long* timePtr2; // "right" end of chart (latest time displayed)
    timePtr2 = timePtr + increment;
    if (pastEnd) {
	hist.h_timestamp.tv_sec = 0;
	hist.h_timestamp.tv_usec = 0;
    } else {
	hist.h_timestamp.tv_sec  = *timePtr2++;
	hist.h_timestamp.tv_usec = *timePtr2;
    }

    int newScale;
    for (StripGadget* sg = strips; sg != 0; sg = sg->getNext(), dataPtr+=2) {
	sg->setCurrentData((dataStruct*) dataPtr);
	// current value (for label) is at *right* end.
	if (pastEnd)
	    sg->setCurVal(0.0);
	else {
	    sg->setCurVal(*(dataPtr + increment));
	    sg->setAvgVal(*(dataPtr + increment + 1));
	    checkAlarms(sg, *(dataPtr + increment), hist.h_timestamp);
	}

	if (!lockPercentages || sg->getType() < TYPE_PPACKETS) {
	    if (sameScale) {
		newScale = sg->findVertScale(0.0); // really could use current max
		if (newScale > max[sg->getType()])
		    max[sg->getType()] = newScale;
	    } else 
		sg->rescale();
	}
    }

    if (sameScale) {
	for (StripGadget* sg = strips; sg != 0; sg = sg->getNext()) {
	    if (!lockPercentages || sg->getType() < TYPE_PPACKETS) {
		sg->setDataHeight(max[sg->getType()]);
		sg->rescale(False);
	    }
	}
    }

    drawData();
    
//    timeGadget->updateTime(hist.h_timestamp);
//     viewGadget->render();
    
} // NetGraph::updateHistory

void
NetGraph::startTimer() {
    // printf("NetGraph::startTimer; timerSeconds = %f\n", timerSeconds);
    if (timerSeconds > 0)
	timer->start(timerSeconds);
    else
	timer->stop();
}

void
NetGraph::throttle(double t) {
    // printf("NetGraph::throttle(%f)\n", t);
    
    // define some different speeds at which we can run history
    static float timerDelays[6] = {0.0, 1.0, 0.5, 0.2, 0.05, 0.02};

    if (t == 0) {
	historyDirection = 0;
    } else if (t < 0) {
	t = -t;
	historyDirection = -1;
    } else {
	historyDirection =  1;
    }

    timerSeconds = timerDelays[(int)t];

    startTimer();
} // throttle


// scroll history to the given percentage of the way through
void
NetGraph::scroll(double p) {
    historyDirection = 0;
    historyTicks = (int) (p * (historySamples - samples));

    if (historyTicks < 0)
	historyTicks = 0;

    timePtr = (long*)historyBeg + historyTicks * sampleSize;
    updateHistory();
    
}

// increment/decrement history
// -1 = dec; +1 = inc; -2/+2 = page dec/inc
void
NetGraph::scrollInc(int inc) {
    throttle(0);

    switch (inc) {
	case -1:
	    historyTicks--;
	    break;
	case 1:
	    historyTicks++;
	    break;
	case -2:
	    historyTicks -= (samples - 1);
	    break;
	case 2:
	    historyTicks += (samples - 1);
	    break;
    }

    if (historyTicks < 0)
	historyTicks = 0;
    if (historyTicks > historySamples)
	historyTicks = historySamples;
	
    scrollGadget->setScrollPerc(
		historyTicks / (float) (historySamples - samples));

    timePtr = (long*)historyBeg + historyTicks * sampleSize;
    updateHistory();
}

void
NetGraph::readData(tuGadget*) {
    // printf("NetGraph::readData\n");
#ifdef CHECK_TIME
    struct timeval *tp1, *tp2;
    tp1 = (struct timeval*)malloc(sizeof(struct timeval));
    tp2 = (struct timeval*)malloc(sizeof(struct timeval));
    gettimeofday(tp1, 0);
#endif

    if (! hist_read(&snpStm, &hist)) {
	exec->removeCallBack(snpStm.ss_sock);
	fatalError = True;
	post(SSREADERRMSG);
	return;
    }

    int i;
    int advanced;   // how many intervals did we advance since last time?
    double deltaTime = 0;
    if ((oldHist.h_timestamp.tv_sec == 0) &&
	(oldHist.h_timestamp.tv_usec == 0)) {
	// this is the first sample, so we'll say we didn't miss anything
	advanced = 1;
    } else {
	// did this sample come at the expected time, or was >= 1 dropped?
	deltaTime = hist.h_timestamp.tv_sec - oldHist.h_timestamp.tv_sec
	  + (hist.h_timestamp.tv_usec - oldHist.h_timestamp.tv_usec)/1000000.0;

	if (deltaTime == 0) {
	    // this is screwy, but apparently happens sometimes if
	    // communication is lost to data station.
	    // If time of this sample is same as previous one, ignore it.
	    return;
	}
	advanced = (int) (deltaTime * 10.0 / interval + .5);
    }

    // If we're recording, make a list of timestamps (normally only one
    // value, but if we skipped, calculate intermediate times).
    // Then, make a list of the values for all the strips - if we skipped,
    // we have to record time, data1, data2.., time, data1, data2.., etc.
    typedef struct timeStr {
	struct timeval  tv;
	struct timeStr* next;
    } timeStruct;
    timeStruct* timeList = NULL;
    if (recording) {
	if (advanced > 1) {
	    double secEach = deltaTime / advanced;
	    double timeSec = oldHist.h_timestamp.tv_sec +
			    oldHist.h_timestamp.tv_usec / 1000000.0;

	    timeStruct *t1, *t2;
	    t1 = NULL;
	    for (i = 0; i < advanced; i++) {
		t2 = new timeStruct;
		if (timeList)
		    t1->next = t2;
		else
		    timeList = t2;
		timeSec += secEach;
		t2->tv.tv_sec  = (long)timeSec;
		t2->tv.tv_usec = (long)((timeSec - t2->tv.tv_sec) * 1000000.0);

		t2->next = NULL;
		t1 = t2;
	    } // for i
	} else {
	    timeList = new timeStruct;
	    timeList->tv.tv_sec = hist.h_timestamp.tv_sec;
	    timeList->tv.tv_usec = hist.h_timestamp.tv_usec;
	    timeList->next = NULL;
	}
    }

    oldHist.h_timestamp.tv_sec = hist.h_timestamp.tv_sec;
    oldHist.h_timestamp.tv_usec = hist.h_timestamp.tv_usec;

    // hist_read, via ss_read, returns cumulative numbers, so we have
    // to subtract the previous values.

    float temp;

    for (unsigned int bin = 0; bin < hist.h_bins; bin++) {
	temp = hist.h_count[bin].c_bytes;
	hist.h_count[bin].c_bytes -= oldHist.h_count[bin].c_bytes;
	oldHist.h_count[bin].c_bytes = temp;

	temp = hist.h_count[bin].c_packets;
	hist.h_count[bin].c_packets -= oldHist.h_count[bin].c_packets;
	oldHist.h_count[bin].c_packets = temp;
    }

    // For same scale option
    for (i = 0; i < NUM_GRAPH_TYPES; i++)
	max[i] = 0.0;

    // Update and rescale all the strips
    dataStruct *ptr, *dataList, *d1, *d2;
    dataList = NULL;	// dataList, d1 & d2 are for recording
    d1 = NULL;
    int samplesSoFar;
    float oldestAvgDataVal;
    int trueIntvl;
    int newScale;
    
    for (StripGadget* sg = strips; sg != 0; sg = sg->getNext()) {
	// note - currentData points to 'oldest' data, which will be 
	//  filled with brand new data.
	ptr = sg->getCurrentData();
	// we'll change the currentData pointer first, 
	// so that if changing the val (& therefore val label)
	// causes a render, we'll be in the right place
	sg->setCurrentData(ptr->next);
	
	// save this in case avgPer = per, which means old data
	// might be overwritten by new data before recalc average.
	oldestAvgDataVal = sg->getAvgData()->y;
	trueIntvl = interval * advanced;
    
	switch (sg->getType()) {
	    case TYPE_PACKETS:
		ptr->y = 10.0 * hist.h_count[sg->getBin()].c_packets/trueIntvl;
		break;

	    case TYPE_BYTES:
		ptr->y = 10.0 * hist.h_count[sg->getBin()].c_bytes / trueIntvl;
		break;

	    case TYPE_PPACKETS:
		if (hist.h_count[0].c_packets) {
		    ptr->y = 100.0 * hist.h_count[sg->getBin()].c_packets /
				     hist.h_count[0].c_packets;
		} else {
		    ptr->y = 0;
		}
		break;

	    case TYPE_PBYTES:
		if (hist.h_count[0].c_bytes) {
		    ptr->y = 100.0 * hist.h_count[sg->getBin()].c_bytes /
				    hist.h_count[0].c_bytes;
		} else {
		    ptr->y = 0;
		}
		break;

	    case TYPE_PNPACKETS:
		if (sg->getNPackets() <= 0)
		    ptr->y = 0.0;
		else
		    ptr->y = 100.0 * hist.h_count[sg->getBin()].c_packets /
					(sg->getNPackets() * trueIntvl / 10.0);
		break;
		
	    case TYPE_PNBYTES:
		if (sg->getNBytes() <= 0)
		    ptr->y = 0.0;
		else
		    ptr->y = 100.0 * (hist.h_count[sg->getBin()].c_bytes)/
				  (sg->getNBytes() * trueIntvl / 10.0);
		break;
	    case TYPE_PETHER:
                ptr->y = 100.0 * (hist.h_count[sg->getBin()].c_bytes +
                                  ETHER_HEAD * hist.h_count[sg->getBin()].c_packets)/
                                  (ETHER_CAPACITY * trueIntvl / 10.0);
 		break;
	    case TYPE_PFDDI:
                ptr->y = 100.0 * (hist.h_count[sg->getBin()].c_bytes +
                                  FDDI_HEAD * hist.h_count[sg->getBin()].c_packets)/
                                  (FDDI_CAPACITY * trueIntvl / 10.0);
		break;
	    case TYPE_PTOKENRING:
                ptr->y = 100.0 * (hist.h_count[sg->getBin()].c_bytes +
                                  TOKENRING_HEAD * hist.h_count[sg->getBin()].c_packets)/
                                  (TOKENRING_CAPACITY * trueIntvl / 10.0);
		break;
	} // switch


	// calc average, if need to
	if (avgSamples != 0) {
    	    samplesSoFar = sg->getSamplesSoFar() + 1;    
	    if (samplesSoFar > avgSamples) {
		// we're well along, so recalc avg based on old average,
		// oldest sample we used to average in, and newest sample
		ptr->avg = sg->getAvgVal() +
	    	(   ptr->y - oldestAvgDataVal) / avgSamples;

		sg->setAvgData(sg->getAvgData()->next);
		sg->setAvgVal(ptr->avg);
	    
	    } else  {
		// if we aren't even up to 'avgPeriod' yet, calc avg so far
		ptr->avg = (sg->getAvgVal() * (samplesSoFar - advanced) +
	    	    	(ptr->y)) /
			 samplesSoFar;
		sg->setAvgVal(ptr->avg);
		sg->setSamplesSoFar(samplesSoFar -1 + advanced);
	    }

	} // calc average
	
	
	if (recording) {
	    d2 = new dataStruct;
	    if (dataList)
		d1->next = d2;
	    else
		dataList = d2;
	    d2->y = ptr->y;
	    d2->avg = ptr->avg; // XXX IS THIS NECESSARY??
	    d2->next = NULL;
	    d1 = d2;
	}

	// copy the avg & y to any we skipped over (missed samples)
	float avgY = ptr->y;
	float avg = ptr->avg;

	for (i = 1; i < advanced; i++) {
	    ptr = ptr->next;
	    ptr->y = avgY;
	    ptr->avg = avg;
	    sg->setAvgData(sg->getAvgData()->next);
	}


	sg->setCurVal(ptr->y);

	checkAlarms(sg, ptr->y, hist.h_timestamp);

	if (!lockPercentages || sg->getType() < TYPE_PPACKETS) {
	    if (sameScale) {
		newScale = sg->findVertScale(0.0); // really could use current max
		if (newScale > max[sg->getType()])
		    max[sg->getType()] = newScale;
	    } else 
		sg->rescale();
	}

    } // for each strip

    if (recording) {
	// record time, & data for each strip
	// (repeat if advanced > 1)
	for (timeStruct *t = timeList; t; t = t->next) {
	    if (!do_xdr_time(&xdr, &t->tv))
		post(BADXDRWRITEMSG);

   	    if (outputAscii) {
	    	char timeString[20];
		char usecString[10];
		long clockSec;
		clockSec = t->tv.tv_sec;
    	    	struct tm* tmStruct;
		tmStruct = localtime(&clockSec);
		ascftime(timeString, "%H:%M:%S", tmStruct);
		sprintf(usecString, "%f",  (float)  t->tv.tv_usec/1000000.0);
		printf("%s%s\n", timeString, strchr(usecString, '.'));
	    }
	    
	    for (dataStruct* d = dataList; d; d = d->next) {
		if (!do_xdr_data(&xdr, &(d->y)))
		    post(BADXDRWRITEMSG);
		if (outputAscii)
		    printf("\t%6f\t%6f\n",  d->y, d->avg);
	    }
 	} // for each time
    } // if recording

    if (sameScale) {
	for (StripGadget* sg = strips; sg != 0; sg = sg->getNext()) {
	    if (!lockPercentages || sg->getType() < TYPE_PPACKETS) {
		sg->setDataHeight(max[sg->getType()]);
		sg->rescale(False);
	    }
	}
    }

    timeSinceUpdate += interval;

    if (timeSinceUpdate >= updateTime) {
        drawData();
	timeSinceUpdate = 0;
    }

#ifdef CHECK_TIME
    gettimeofday(tp2, 0);
    int sec, usec;
    sec = tp2->tv_sec - tp1->tv_sec;
    if (sec) {
        sec--;    
        usec = 1000000 + tp2->tv_usec - tp1->tv_usec;
    } else
        usec = tp2->tv_usec - tp1->tv_usec;
    totalSec += sec;
    totalUsec += usec;
    intervals++;
#endif


} // NetGraph::readData

void
NetGraph::drawData(void) {
    // printf("NetGraph::drawData\n");
    timeGadget->updateTime(hist.h_timestamp);
    // viewGadget->render();
    for (StripGadget *sg = strips; sg != 0; sg = sg->getNext()) {
	sg->render();
    }
} // NetGraph::drawData(void)


void
NetGraph::checkAlarms(StripGadget* sg, float value, struct timeval tval) {
    tuBool alarmSet = sg->getAlarmSet();

    if (!alarmSet)
	return;

    float loVal = sg->getAlarmLoVal();
    float hiVal = sg->getAlarmHiVal();
    tuBool alarmBell = sg->getAlarmBell();
    tuBool alarmLoWasMet = sg->getAlarmLoMet();
    tuBool alarmHiWasMet = sg->getAlarmHiMet();
    int alarmType = ALM_NONE;

    if (alarmLoWasMet && (value > loVal))
	alarmType = NV_RATE_THRESH_LO_UN_MET;
    else if (!alarmLoWasMet && (value < loVal))
	alarmType = NV_RATE_THRESH_LO_MET;

    if (alarmHiWasMet && (value < hiVal))
	alarmType = NV_RATE_THRESH_HI_UN_MET;
    else if (!alarmHiWasMet && (value > hiVal))
	alarmType = NV_RATE_THRESH_HI_MET;


    if (alarmType != ALM_NONE) {
	long clockSec = tval.tv_sec;
	struct tm* timeStruct;
	char timeString[10];
	timeStruct = localtime(&clockSec);
	ascftime(timeString, "%H:%M:%S", timeStruct);
		
	sendAlarmEvent(alarmType, sg, value);
	    	    
	switch(alarmType) {
	  case NV_RATE_THRESH_LO_UN_MET:
	    sg->setAlarmLoMet(False);
	    fprintf(afp,"Alarm condition no longer met at %s:\n", timeString);
	    fprintf(afp, "     graph: %s %s\n", filters[sg->getBin()].expr, sg->getTypeText());
	    fprintf(afp, "     value: %.2f > %.2f\n",
		value, sg->getAlarmLoVal());
	    break;
	  case NV_RATE_THRESH_LO_MET:
	    sg->setAlarmLoMet(True);
	    if (alarmBell)
		XBell(getDpy(), 0);
	    fprintf(afp, "Alarm condition met at %s:\n", timeString);
	    fprintf(afp, "     graph: %s %s\n", filters[sg->getBin()].expr, sg->getTypeText());
	    fprintf(afp, "     value: %.2f < %.2f\n",
		value, sg->getAlarmLoVal());
	    break;
	  case NV_RATE_THRESH_HI_UN_MET:
	    sg->setAlarmHiMet(False);
	    fprintf(afp, "Alarm condition no longer met at %s:\n", timeString);
	    fprintf(afp, "     graph: %s %s\n", filters[sg->getBin()].expr, sg->getTypeText());
	    fprintf(afp, "     value: %.2f < %.2f\n",
		value, sg->getAlarmHiVal());
	    break;
	  case NV_RATE_THRESH_HI_MET:
	    sg->setAlarmHiMet(True);
	    if (alarmBell)
		XBell(getDpy(), 0);
	    fprintf(afp, "Alarm condition met at %s:\n", timeString);
	    fprintf(afp, "     graph: %s %s\n", filters[sg->getBin()].expr, sg->getTypeText());
	    fprintf(afp, "     value: %.2f > %.2f\n",
		value, sg->getAlarmHiVal());
	    break;
	} // switch
	fflush(afp);
    } // we have something to report

} // NetGraph::checkAlarms

void
NetGraph::sendAlarmEvent(int alarmType, StripGadget* sg, float value) {
    alarmEvent->setType(alarmType);
    alarmEvent->setFilter(filters[sg->getBin()].expr);
    alarmEvent->setRateBase(rateBases[sg->getType()]);
    alarmEvent->setRate(value);

    if (sg->getType() == TYPE_PNBYTES) {
	alarmEvent->setRateOfBase(sg->getNBytes());
    } else if (sg->getType() == TYPE_PNPACKETS) {
	alarmEvent->setRateOfBase(sg->getNPackets());
    }

    switch (alarmType) {
	case NV_RATE_THRESH_LO_MET:
	case NV_RATE_THRESH_LO_UN_MET:
	    alarmEvent->setThreshRate(sg->getAlarmLoVal());
	    break;
	case NV_RATE_THRESH_HI_MET:
	case NV_RATE_THRESH_HI_UN_MET:
	    alarmEvent->setThreshRate(sg->getAlarmHiVal());
	    break;
    }
    
    eh->send(alarmEvent);

} // sendAlarmEvent


void
NetGraph::deleteGraph(StripGadget *strip) {
    // remove the strip from the linked list
    if (strip == strips) {
	strips = strip->getNext();
    } else {
	StripGadget *prevsg;
	for (StripGadget *sg = strips; sg != 0; sg = sg->getNext()) {
	    if (sg == strip) {
		break;
	    }
	    prevsg = sg;
	}
	prevsg->setNext(strip->getNext());
    }

    int thisBin = strip->getBin();
    if (thisBin >= 0)
	stopUsingBin(thisBin);

    setSelectedStrip(NULL);

    dataStruct2* ds = new dataStruct2;
    ds->data = strip->getHeadData();
    ds->next = freeDataList;
    freeDataList = ds;
    strip->freeData();
    viewGadget->removeAGraph(strip);
    
    
} // NetGraph::deleteGraph


// When a graph is going away, call this with its bin #.
// This function decrements that filter's count, removes the filter if
// nobody else is using it, and goes through the graphs, decrementing
// the bin of each one using a higher bin. (Because snoopstream, when
// a bin is removed, shifts the higher ones down.)
// It also updates hist & oldHist, which also keep track of things per bin.
void
NetGraph::stopUsingBin(int bin) {
    if (--filters[bin].used == 0) {
	// filter is now unused
	(void) snoopStop(&snpStm);	
	(void) ss_delete(&snpStm, bin);
	(void) snoopStart(&snpStm);

	// remove the filter, shift the others down
	for (int i = bin; i < highestBin; i++) {
	    strcpy(filters[i].expr, filters[i+1].expr);
	    filters[i].used = filters[i+1].used;

	    hist.h_count[i].c_bytes = hist.h_count[i+1].c_bytes;
	    hist.h_count[i].c_packets = hist.h_count[i+1].c_packets;
	    oldHist.h_count[i].c_bytes = oldHist.h_count[i+1].c_bytes;
	    oldHist.h_count[i].c_packets = oldHist.h_count[i+1].c_packets;
	}

	// clear the unused bin in hist, oldHist
	hist.h_count[highestBin].c_bytes = 0;
	hist.h_count[highestBin].c_packets = 0;
	oldHist.h_count[highestBin].c_bytes = 0;
	oldHist.h_count[highestBin].c_packets = 0;


	// shift the bin numbers
	for (StripGadget* sg = strips; sg != 0; sg = sg->getNext()) {
	    sg->shiftBin(bin);
	}

	highestBin--;
    } // filter now unused

} // NetGraph::stopUsingBin

void
NetGraph::setSelectedStrip(StripGadget *strip) {

    if (editControl && editControl->isMapped())
	editControl->setSg(strip, NULL);

    // take care of menu items that depend on selection, etc
    if (strip) {
	char* filter = filters[strip->getBin()].expr;
	if (strcmp(filter, "total"))
	    toolOptions->setFilter(filter);
	else
	    toolOptions->setFilter(NULL);

	strip->changeSelected(True);
	if (!recording && !history &&
	      (viewGadget->getNumberOfGraphs() > 1))
	    deleteItem->setEnabled(True);
    } else {
	toolOptions->setFilter(NULL);
	deleteItem->setEnabled(False);	
    }

	
    if (selectedStrip)
	selectedStrip->changeSelected(False);

    selectedStrip = strip;

}



// simple stuff (probably should be marked "inline" in netGraph.h)

tuXExec*
NetGraph::getExec(void)		{ return exec; }
	
DialogBox*
NetGraph::getDialogBox(void)	{ return dialog; }

char*
NetGraph::getFullSnooperString() {
    if (fullSnooperString[0])
	return strdup(fullSnooperString);
    else
	return localHostName;
}

Display *
NetGraph::getDpy(void)		{ return display; }


