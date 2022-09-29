/*
 * Copyright 1991, 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetTop
 *
 *	$Revision: 1.37 $
 *	$Date: 1996/02/26 01:27:57 $
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
#include <net/if.h>
#include <osfcn.h>
#include <pwd.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>     // for errno

extern "C" {
char *ether_ntoa(struct etheraddr *);
struct etheraddr *ether_aton(char *);
int kill (pid_t pid, int sig);
int sethostresorder(const char *);
}

// tinyui includes
#include <tuCallBack.h>
#include <tuFilePrompter.h>
#include <tuGC.h>
#include <tuScreen.h>
#include <tuTimer.h>
#include <tuTopLevel.h>
#include <tuVisual.h>
#include <tuXExec.h>

// nettop includes
#include "dataGizmo.h"
#include "messages.h"
#include "nodeGizmo.h"
#include "netTop.h"
#include "top.h"
#include "font.h"
#include "event.h"

#include <dialog.h>
#include <tooloptions.h>

#include "help.h"
#include "helpapi/HelpBroker.h"

extern "C" {

#include <bstring.h>
/* #include <getopt.h> */

// netvis/snoop includes
#include "exception.h"
#include "expr.h"
#include "protocol.h"
#include "snooper.h"
#include "snoopstream.h"
#include "license.h"

int hist_init(SnoopStream*,  h_info*, int, int, int);
int hist_read(SnoopStream*, struct histogram*);

} // (extern C)

#define SETUP_FILE	    "~/.nettoprc"
const float ETHER_CAPACITY = (( 10.0 * 1000000.0) / 8.0);
const float ETHER_HEAD = 24.0; // xxx is this right???
const float FDDI_CAPACITY  = ((100.0 * 1000000.0) / 8.0);
const float FDDI_HEAD = 0.0; // xxx what should this be???

const char VIEW_GEOMETRY[] = "592x603+313+33";
const char TRAFFIC_GEOMETRY[] = "390x632+7+183";
const char NODE_GEOMETRY[] = "460x720+767+263";

// CVG
EV_handler *eh;

void
NetTop::doHelp(tuGadget *, void* helpCardName) {

    HELPdisplay((char*)helpCardName);

}

tuDeclareCallBackClass(NetTopCallBack, NetTop);
tuImplementCallBackClass(NetTopCallBack, NetTop);

extern NetTop *nettop;
extern int _yp_disabled;


// copied option/arg stuff from libtu/demos/popup.c++

// Define the command line arguments that we understand.  For example,
// saying "-fn foo" is the same as putting "NetTop*font: foo" in a
// .Xdefaults file. 
// NOTE: if I don't have a "-s", x will eat it, matching it to
// libtu's "-sync". So, I have to catch it this way (for number_sources)
// '-d' is fine, so get it with normal getopts
    

static XrmOptionDescRec args[] = {
    {"-i",	"*interface",		XrmoptionSepArg, NULL},
    {"-u",	"*controlsFile",	XrmoptionSepArg, NULL},
    {"-y",      "*useyp",               XrmoptionNoArg, "True"},
    {"-h",	"*help", 		XrmoptionNoArg, "Yes"},

    {"-s",      "*number_sources",      XrmoptionSepArg, NULL},
    
    {"-colormap",	"*colormap", 	XrmoptionNoArg, "Yes"},
};

static const int nargs = sizeof(args) / sizeof(XrmOptionDescRec);


static tuResourceItem opts[] = {
    { "interface", 0, tuResString, "", offsetof(AppResources, interface), },
    { "controlsFile", 0, tuResString, SETUP_FILE, offsetof(AppResources, controlsFile), },
    { "useyp", 0, tuResBool, "False", offsetof(AppResources, useyp), },
    { "hostresorder", 0, tuResString, "local", offsetof(AppResources, hostresorder), },
    { "help", 0, tuResBool, "False", offsetof(AppResources, help), },
    
    { "nohist", 0, tuResBool, "False", offsetof(AppResources, nohist), },
    { "noaddrlist", 0, tuResBool, "False", offsetof(AppResources, noaddrlist), },
    { "colormap", 0, tuResBool, "False", offsetof(AppResources, colormap), },

    { "number_sources", 0, tuResShort, "0", offsetof(AppResources, number_sources), },

    { "nodeControlBackgroundColor", 0, tuResColor, NULL, offsetof(AppResources, nodeControlBackgroundColor), },
    { "trafficControlBackgroundColor", 0, tuResColor, NULL, offsetof(AppResources, trafficControlBackgroundColor), },

    { "mainBackgroundRGBColor", 0, tuResString, "0x000000", offsetof(AppResources, mainBackgroundRGBColor), },
    { "selectRGBColor", 0, tuResString, "0x00FFFF", offsetof(AppResources, selectRGBColor), },
    { "labelRGBColor", 0, tuResString, "0x000000", offsetof(AppResources, labelRGBColor), },
    { "nameRGBColor", 0, tuResString, "0x00FF00", offsetof(AppResources, nameRGBColor), },
    { "nameErrorRGBColor", 0, tuResString, "0x0000FF", offsetof(AppResources, nameErrorRGBColor), },
    { "nameLockedRGBColor", 0, tuResString, "0xFF8000", offsetof(AppResources, nameLockedRGBColor), },
    { "baseRGBColor", 0, tuResString, "0x909090", offsetof(AppResources, baseRGBColor), },
    { "scaleRGBColor", 0, tuResString, "0x00FF00", offsetof(AppResources, scaleRGBColor), },
    { "titleRGBColor", 0, tuResString, "0xFFFFFF", offsetof(AppResources, titleRGBColor), },
    { "edgeRGBColor", 0, tuResString, "0x000000", offsetof(AppResources, edgeRGBColor), },

    { "mainBackgroundIndexColor", 0, tuResShort, "0", offsetof(AppResources, mainBackgroundIndexColor), },
    { "selectIndexColor", 0, tuResShort, "3", offsetof(AppResources, selectIndexColor), },
    { "labelIndexColor", 0, tuResShort, "0", offsetof(AppResources, labelIndexColor), },
    { "nameIndexColor", 0, tuResShort, "2", offsetof(AppResources, nameIndexColor), },
    { "nameErrorIndexColor", 0, tuResShort, "1", offsetof(AppResources, nameErrorIndexColor), },
    { "nameLockedIndexColor", 0, tuResShort, "4", offsetof(AppResources, nameLockedIndexColor), },
    { "baseIndexColor", 0, tuResShort, "15", offsetof(AppResources, baseIndexColor), },
    { "scaleIndexColor", 0, tuResShort, "2", offsetof(AppResources, scaleIndexColor), },
    { "titleIndexColor", 0, tuResShort, "7", offsetof(AppResources, titleIndexColor), },
    { "edgeIndexColor", 0, tuResShort, "0", offsetof(AppResources, edgeIndexColor), },
    { "totalTotalIndexColor", 0, tuResShort, "5", offsetof(AppResources, totalTotalIndexColor), },
    { "otherOtherIndexColor", 0, tuResShort, "9", offsetof(AppResources, otherOtherIndexColor), },
    { "totalAnyIndexColor", 0, tuResShort, "12", offsetof(AppResources, totalAnyIndexColor), },
    { "otherAnyIndexColor", 0, tuResShort, "13", offsetof(AppResources, otherAnyIndexColor), },
    { "anyAnyIndexColor", 0, tuResShort, "4", offsetof(AppResources, anyAnyIndexColor), },

    0,
};



NetTop::NetTop(int argc, char *argv[])
{   
    //  init some vars
    strcpy (windowName, "NetTop");
    fatalError = False;
    postedFatal = False;
    quitNow = False;
    snooping = False;
    subscribed = False; // CVG
    doTopOutput = False;
    useLogSmoothing = False;
    gotInterfaceFromResource = False;
    histSnpStm.ss_sock = -1;
    highestBin = -1;
    totalSrc = 0; totalDest = 0; // xxx always?
    otherSrc = 1; otherDest = 1; // xxx
    currentMax = 0.0;
    lastMax = 0.0;
    netfiltersPid = 0;
    fixedScale = 5;

    stepsTaken = 0;

    // lock 'total' and 'other'
    srcInfo[totalSrc].locked = True;
    srcInfo[otherSrc].locked = True;
    destInfo[totalDest].locked = True;
    destInfo[otherDest].locked = True;
    numSrcsLocked = 2;
    numDestsLocked = 2;
    assignNames();
    setLabelType(DMFULLNAME); // to make sure displayString is set

    viewgadgetGeom = NULL;
    datagizmoGeom = NULL;
    nodegizmoGeom = NULL;
    
    datagizmoWasMapped = False;
    nodegizmoWasMapped = False;
    iconic = False;

    // localHostName, etc. used to be init'ed here, but moved
    // below, after dialog is created, so can handle any error

    filter = strdup(""); // get from cmd line, rc file, too.

    doSrcAndDest = True;
    whichAxes = AXES_SRCS_DESTS;
    whichSrcs = NODES_SPECIFIC;
    whichDests = NODES_SPECIFIC;
    whichNodes = NODES_SPECIFIC;
    
    // xxx
    graphType = TYPE_PACKETS;
    nPackets = 1000;
    nBytes = 100000;
    numSrcs = 5;
    numDests = 5;
    dataUpdateTime = 10;  // 1 second
    nodeUpdateTime = 100; // 10 seconds
    scaleUpdateTime = 50; // 5 seconds
    
    
    // Create a place to store the resource database, and load it
    // up.  This also parses the command lines, opens the X display, and
    // returns to us the default screen on that display.

    rdb = new tuResourceDB;
    ar = new AppResources;
 
    char* instanceName = 0;
    char* className = "NetTop";
    tuScreen* screen = rdb->getAppResources(ar, args, nargs, opts,
					   &instanceName, className,
					   &argc, argv);

    cmap = screen->getDefaultColorMap();
    display = rdb->getDpy();

    // Initialize SGIHelp
    SGIHelpInit (display, className, ".");

    // Set up callbacks
    (new NetTopCallBack(this, NetTop::quit))->
                                registerName("__NetTop_quit");
    (new NetTopCallBack(this, NetTop::save))->
                                registerName("__NetTop_save");
    (new NetTopCallBack(this, NetTop::saveAs))->
                                registerName("__NetTop_saveAs");
    (new NetTopCallBack(this, NetTop::saveImageAs))->
                                registerName("__NetTop_saveImageAs");

    (new NetTopCallBack(this, NetTop::openDataGizmo))->
                                registerName("__NetTop_openDataGizmo");
    (new NetTopCallBack(this, NetTop::openNodeGizmo))->
                                registerName("__NetTop_openNodeGizmo");
    (new tuFunctionCallBack(NetTop::doHelp, GENERAL_HELP_CARD))->
                                registerName("__NetTop_help");


    tooloptions = new ToolOptions;
    if (viewgadgetGeom == NULL)
	viewgadgetGeom = strdup(VIEW_GEOMETRY);
    viewgadget = new ViewGadget(this, windowName, cmap, rdb,
                      instanceName, className, NULL, viewgadgetGeom);

    tooloptions->setTopLevel(viewgadget);

    dialog = new DialogBox("NetTop", viewgadget, False);
    setDialogBoxCallBack();

    if (ar->help) {
	usage();
	return;
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
	licDialog = new DialogBox("NetTop", viewgadget, False);
	licDialog->setCallBack(new NetTopCallBack(this, NetTop::closeLicDialogBox));
	licDialog->information(message);
	licDialog->map();
	XFlush(getDpy());
	delete message;
    }

    // this used to be up above.
    if (gethostname(localHostName, 64) < 0) {
	fatalError = True;
	quitNow = True;
	post(GETHOSTNAMEMSG);
	return;
    }
    strcpy(fullSnooperString, localHostName);
    strcpy(newSnooper.node, localHostName);
    strcat(fullSnooperString, ":"); // so analyzer will like it
    newSnooper.interface[0] = NULL;
    
    currentSnooper.node[0] = NULL;
    currentSnooper.interface[0] = NULL;
    
    // process controlsfile
    strcpy(lastFile,  ar->controlsFile);
    
    // xxxxx try local, then in $HOME
    switch (readFile(lastFile)) {
	case -1:
	    // major problem reading the file
	    return;
	    // quit(NULL);
	    break;
	case 1:
	    // xxxx
	    break;
    }

    // handle the interface; used to be fine in handleOptions,
    // but now -i gets interpreted as -iconic unless we do it
    // in our own resource.
    if (ar->interface[0] != NULL) {
	gotInterfaceFromResource = True;
	fatalError = True; // since might be posted by handleIntfc()
	quitNow = True;
	if (handleIntfc(ar->interface) == -1)
	    return;
	fatalError = False; // return this to normal value
	quitNow = False;
	snooperChanged();
    }

    // process options on command line
    if (handleOptions(argc, argv) == -1) {
	// quit((tuGadget*)0);
	return;
    }

    // if we caught number_sources (via "-s" on cmd line), handle it
    if (ar->number_sources > 0) {
	numSrcs = ar->number_sources;
	ar->number_sources = 0;
    }
	
    // make sure these params are within their legal bounds
    numSrcs = TU_MIN(TU_MAX(1, numSrcs), MAX_NODES);
    numDests = TU_MIN(TU_MAX(1, numDests), MAX_NODES);

    // have min values, but not max values for time params.
    dataUpdateTime  = TU_MAX(MIN_DATA_UPDATE_TIME, dataUpdateTime);
    nodeUpdateTime  = TU_MAX(MIN_NODE_UPDATE_TIME, nodeUpdateTime);
    scaleUpdateTime = TU_MAX(MIN_SCALE_UPDATE_TIME, scaleUpdateTime);
    dataSmoothPerc  = TU_MIN(TU_MAX(MIN_INTERP_PERC, dataSmoothPerc), MAX_INTERP_PERC);

    // scale time must not be < data time
    scaleUpdateTime = TU_MAX(scaleUpdateTime, dataUpdateTime);
    // node time must not be < data time
    nodeUpdateTime  = TU_MAX(nodeUpdateTime, dataUpdateTime);
    

    dataSmoothPeriod = (int) (.01 * dataUpdateTime * dataSmoothPerc + 0.5);
    dataSmoothInterval = 0.1;
    numSteps = dataSmoothPeriod; // (since interval is 0.1)



    
    if (ar->colormap)
	viewgadget->forceColorMap();
    viewgadget->setIconicCallBack(new NetTopCallBack(this, NetTop::iconicCB));

    tooloptions->setTopLevel(viewgadget);

    if (datagizmoGeom == NULL)
	datagizmoGeom = strdup(TRAFFIC_GEOMETRY);
    if (nodegizmoGeom == NULL)
	nodegizmoGeom = strdup(NODE_GEOMETRY);

    datagizmo = new DataGizmo(this, "Traffic Control Panel",
		    viewgadget, datagizmoGeom);
    nodegizmo = new NodeGizmo(this, "Nodes Control Panel",
		    viewgadget, nodegizmoGeom);

    datagizmoX = datagizmo->getXOrigin();
    datagizmoY = datagizmo->getYOrigin();
    nodegizmoX = nodegizmo->getXOrigin();
    nodegizmoY = nodegizmo->getYOrigin();

    top = new Top(this);
    top->setOutput(doTopOutput);

    smoothTimer = new tuTimer(True);
    smoothTimer->setCallBack(new NetTopCallBack(this, &NetTop::smoothData));

    if (ar->nodeControlBackgroundColor)
	nodegizmo->setBackground( (tuColor*) ar->nodeControlBackgroundColor);
    if (ar->trafficControlBackgroundColor)
	datagizmo->setBackground( (tuColor*) ar->trafficControlBackgroundColor);

    EV_handle evfd;
    static char* appName = "NetTop";

    eh = new EV_handler(&evfd, appName);
    event = new EV_event(NV_STARTUP);
    alarmEvent = new EV_event();
    eh->send(event);

    // Set hostresorder and _yp_disabled
    if (ar->useyp)
	_yp_disabled = 0;
    else
	_yp_disabled = 1;
    sethostresorder(ar->hostresorder);

    if (snoopInit() == -1) {
	//openDialogBox(); (not necessary; we did post(), which does this
	return;
    }
    
    if (ar->nohist) {
	(void) snoopStop(&histSnpStm, True);
	snooping = False;
    }
    
    if (ar->noaddrlist)
	stopTopSnooping();
	
    // Get hints for layout and resize
    tuLayoutHints hints;
    viewgadget->getLayoutHints(&hints);
    viewgadget->resize(hints.prefWidth, hints.prefHeight);
    datagizmo->getLayoutHints(&hints);
    datagizmo->resize(hints.prefWidth, hints.prefHeight);
    nodegizmo->getLayoutHints(&hints);
    nodegizmo->resize(hints.prefWidth, hints.prefHeight);
    
    // Map the view window only
    viewgadget->getVals();
    viewgadget->map();

    // set up netlook-style fonts after GL is initialized
    fontInit();

} // NetTop

NetTop::NetTop(const char* instanceName, tuColorMap* cmap,
	      tuResourceDB* db, char* progName, char* progClass)
{
    // I don't think I'll need this one...
}

void
NetTop::assignNames() {
    srcInfo[0].fullname = strdup("total");
    srcInfo[1].fullname = strdup("other");
    destInfo[0].fullname = strdup("total");
    destInfo[1].fullname = strdup("other");

    for (int i = 2; i < MAX_NODES; i++) {
	srcInfo[i].fullname = strdup("");
	destInfo[i].fullname = strdup("");
	srcInfo[i].ipaddr = NULL;
	destInfo[i].ipaddr = NULL;
	srcInfo[i].physaddr = NULL;
	destInfo[i].physaddr = NULL;
    }
 
} // assignNames


void
NetTop::openDataGizmo(tuGadget *) {
    if (datagizmo->isMapped()) {
	if (datagizmo->getIconic())
	    datagizmo->setIconic(False);
	datagizmo->pop();
    } else
	datagizmo->map();
}

void
NetTop::closeDataGizmo(void) {
    datagizmo->unmap();
}

void
NetTop::openNodeGizmo(tuGadget *) {
    if (nodegizmo->isMapped()) {
	if (nodegizmo->getIconic())
	    nodegizmo->setIconic(False);
	nodegizmo->pop();
    } else
	nodegizmo->map();
}

void
NetTop::closeNodeGizmo(void) {
    nodegizmo->unmap();
}

// xxx doesn't quite work: TINYUI BUGS (submitted 7/22)
//     if iconify gizmo first, icon doesn't disappear when iconify main
//     when map gizmo, it comes up non-iconic; if want iconic, must
//	 setIconic(True) *after* unmap, but then it flashes (there then gone)

// this gets called AFTER we actually change state between iconic & not
void
NetTop::iconicCB(tuGadget *) {
    if (viewgadget->getIconic()) {
	iconic = True;

	if (datagizmo->isMapped()) {
	    datagizmoWasMapped = True;
	    datagizmoX = datagizmo->getXOrigin();
	    datagizmoY = datagizmo->getYOrigin();
	    datagizmo->unmap();
	} else
	    datagizmoWasMapped = False;
	    
	if (nodegizmo->isMapped()) {
	    nodegizmoWasMapped = True;
	    nodegizmoX = nodegizmo->getXOrigin();
	    nodegizmoY = nodegizmo->getYOrigin();
	    nodegizmo->unmap();
	} else
	    nodegizmoWasMapped = False;

    } else {
	iconic = False;

	if (datagizmoWasMapped) {
	    datagizmo->setInitialOrigin(datagizmoX, datagizmoY);
	    datagizmo->map();
	}
	if (nodegizmoWasMapped) {
	    nodegizmo->setInitialOrigin(nodegizmoX, nodegizmoY);
	    nodegizmo->map();
	}
    }
    
}


void
NetTop::quit(tuGadget *g) {
    // printf("NetTop::quit\n");
    // we'll make a new dialog box, because otherwise sometimes
    // have trouble with it coming up iconified
    if (dialog->isMapped())
	dialog->unmap();
    
    if (!questionDialog)
	questionDialog = new DialogBox("NetTop", viewgadget, False);
    else if (questionDialog->isMapped())
	questionDialog->unmap();

    if (g)
	questionDialog->question("Save UI to %s before quitting", lastFile);
    else
	questionDialog->question("Fatal error. Save UI to %s before quitting", lastFile);
    questionDialog->setCallBack(new NetTopCallBack(this, NetTop::doQuit));
    questionDialog->map();
    XFlush(getDpy());
}

void
NetTop::doQuit(tuGadget *g) {
    enum tuDialogHitCode hitCode;

    if (g)
	hitCode = questionDialog->getHitCode();
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
	    if (histSnpStm.ss_sock != -1) {
		(void) snoopStop(&histSnpStm, True);

		for (int bin = highestBin; bin >= 0; bin--)
		    (void) ss_delete(&histSnpStm, bin);
	
		if (subscribed)
		    (void) ss_unsubscribe(&histSnpStm);
		ss_close(&histSnpStm);
	    }
    

	    if (netfiltersPid != 0)
		kill(netfiltersPid, SIGTERM);

	    HELPclose();
    
	    // make a new event, so it doesn't have the interface in it.
	    EV_event shutdown(NV_SHUTDOWN);
	    eh->send(&shutdown);

	    if (fatalError)
		exit(-1);
	    else
		exit(0);
	} // case
    } // switch
} // doQuit


void
NetTop::immediateQuit(tuGadget*) {
    exit(-1);
}


char *
NetTop::expandTilde(const char *c)
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
 * Read the configuration file.  If no file is specified, use SETUP_FILE
 * from the current or home directory.
 * After we get something open, set 'lastFile' to the name of the file
 * we opened, so we can write back to the same place later.
 */
 // copied from netgraph
 // xxx return -1 == big failure; quit
 //     return  0 == ok
 //     return  1 == no file read
int
NetTop::readFile(char* fileName) {

    // printf("NetTop::readFile(%s)\n", fileName);
    FILE *fp = NULL;
    char buf[256];

    char* newname = expandTilde(fileName);
    if (newname != 0) {
	strcpy(lastFile, newname);
	fileName = lastFile;
	delete [] newname;
    }
    
    // open it
    fp = fopen(fileName, "r");
    if (fp == NULL) {
	dialog->warning(errno,  "Could not open %s", fileName);
	openDialogBox();
	return 1;
    }

    char line[LINESIZE];
    char button[32];
    char state[32];
    char name[LINESIZE];
    char addr[LINESIZE];
    char* charP;
    int itemsScanned;
    int  i;
    int err = 0;
    static char white[] = " \t\n";


    // read parameters, names, etc
    while (fgets(line, LINESIZE-1, fp) != NULL) {
	itemsScanned = sscanf(line, "%s %s %s %s",
	    button, state, name, addr);
	// printf("line:<%s> (%d)\n", line, itemsScanned);
	if (itemsScanned < 2 || button[0] == '#')
		continue;
		
	if (strcasecmp(button, "SnoopInterface") == 0) {
	    if (strcmp(state, "")) {
		// printf("interface<%s>\n", state); // xxx
		strcpy(fullSnooperString, state);
	
		fatalError = True; // so any err will show up as 'error' not 'warning'
		if (handleIntfc(state) == -1) {
		    // handleIntfc puts up message
		    return -1;
		}
		fatalError = False;
		    
	    } // state not empty

	} else if (strcasecmp(button, "SnoopFilter") == 0) {
	    // tricky: filters can have spaces.. 
	    // find the second thing in the line; rest of line is filter
	    // line: %s\n", line);
	    charP = strtok(line, white);
	    // printf("charP: %s\n", charP);
	    charP = strtok(NULL, white);
	    // printf("charP: %s\n", charP);
	    setFilter(charP);
	    // if (filter)
	    // 	delete [] filter;
	    // filter = strdup(charP);

	} else if (strcasecmp(button, "MeasureTraffic") == 0) {
	    for (i = 0; i < NUM_GRAPH_TYPES; i++)
		if (strcasecmp(state, graphTypes[i]) == 0) {
		    graphType = i;
		    break;
		}
	} else if (strcasecmp(button, "NPackets") == 0) {
	    nPackets = atoi(state);
	} else if (strcasecmp(button, "NBytes") == 0) {
	    nBytes = atoi(state);

	} else if (strcasecmp(button, "DataUpdateTime") == 0) {
	    dataUpdateTime = (int)(10 * atof(state) + 0.5);
	} else if (strcasecmp(button, "InterpolatePercent") == 0) {
	    dataSmoothPerc = atoi(state);
	} else if (strcasecmp(button, "VerticalScale") == 0) {
	    for (i = 0; i < NUM_RESCALE_TYPES; i++)
		if (strcasecmp(state, rescaleTypes[i]) == 0) {
		    rescaleType = i;
		    break;
		}
	} else if (strcasecmp(button, "LockAt") == 0) {
	    fixedScale = atoi(state);
	} else if (strcasecmp(button, "ReduceAfter") == 0) {
	    scaleUpdateTime = (int)(10 * atof(state) + 0.5);
	} else if (strcasecmp(button, "LabelBy") == 0) {
	    if (strcasecmp(state, "Name") == 0)
		setLabelType(DMFULLNAME);
	    else if (strcasecmp(state, "Address") == 0)
		setLabelType(DMINETADDR);
	} else if (strcasecmp(button, "Display") == 0) {
	    for (i = 0; i < NUM_AXES_TYPES; i++)
		if (strcasecmp(state, axesTypes[i]) == 0) {
		    whichAxes = i;
		    if (i == AXES_NODES_FILTERS)
			doSrcAndDest = False;
		    else
			doSrcAndDest = True;
		    break;
		}

	} else if (strcasecmp(button, "Sources") == 0 ||
		   strcasecmp(button, "Nodes") == 0) {
	    numSrcs = atoi(state);
	} else if (strcasecmp(button, "Pairs") == 0) {
	    numSrcs = atoi(state);
	    numDests = numSrcs;
	} else if (strcasecmp(button, "Destinations") == 0 ||
		   strcasecmp(button, "Filters") == 0) {
	    numDests = atoi(state);
	} else if (strcasecmp(button, "ShowSourceNodes") == 0) {
	    for (i = 0; i < NUM_NODE_TYPES; i++)
		if (strcasecmp(state, genericTypes[i]) == 0) {
		    whichSrcs = i;
		    break;
		}
	} else if (strcasecmp(button, "ShowDestinationNodes") == 0) {
	    for (i = 0; i < NUM_NODE_TYPES; i++)
		if (strcasecmp(state, genericTypes[i]) == 0) {
		    whichDests = i;
		    break;
		}
	} else if (strcasecmp(button, "ShowNodes") == 0) {
	    for (i = 0; i < NUM_NODE_TYPES; i++)
		if (strcasecmp(state, genericTypes[i]) == 0) {
		    whichNodes = i;
		    break;
		}
	} else if (strcasecmp(button, "BusiestMeans") == 0) {
	    if (strcasecmp(state, "MostBytes") == 0)
		busyBytes = True;
	    else if (strcasecmp(state, "MostPackets") == 0)
		busyBytes = False;
	} else if (strcasecmp(button, "NodeUpdateTime") == 0) {
	    nodeUpdateTime = (int)(10 * atof(state) + 0.5);
	} else if (strcasecmp(button, "NetTopGeometry") == 0) {
		viewgadgetGeom = strdup(state);
	} else if (strcasecmp(button, "NodeControlGeometry") == 0) {
		nodegizmoGeom = strdup(state);
		nodegizmoWasMapped = True;
	} else if (strcasecmp(button, "TrafficControlGeometry") == 0) {
		datagizmoGeom = strdup(state);
		datagizmoWasMapped = True;
		
	} else if (strcasecmp(button, "Source") == 0 ||
		   strcasecmp(button, "Node") == 0) {
	    i = atoi(state);
	    if (i < 2 || i >= MAX_NODES || itemsScanned < 3)
		continue;

	    // printf("source %d: %s %s\n", i, name, addr);
	    // we have either name, or name & address
	    srcInfo[i].fullname = strdup(name);
	    if (itemsScanned == 4 && strlen(addr) > 0) {
		if (inet_addr(addr) == INADDR_NONE)
		    srcInfo[i].physaddr = strdup(addr);
		else
		    srcInfo[i].ipaddr = strdup(addr);
	    }

	} else if (strcasecmp(button, "Destination") == 0) {
	    i = atoi(state);
	    if (i < 2 || i >= MAX_NODES || itemsScanned < 3)
		continue;

	    // printf("dest %d: %s %s\n", i, name, addr);
	    // we have either name, or name & address
	    destInfo[i].fullname = strdup(name);
	    if (itemsScanned == 4 && strlen(addr) > 0) {
		if (inet_addr(addr) == INADDR_NONE)
		    destInfo[i].physaddr = strdup(addr);
		else
		    destInfo[i].ipaddr = strdup(addr);
	    }

	} else if (strcasecmp(button, "Filter") == 0) {
	    // tricky: filters can have spaces.. 
	    i = atoi(state);
	    if (i < 2 || i >= MAX_NODES || itemsScanned < 3)
		continue;
	    // find the third thing in the line; rest of line is filter
	    // line: %s\n", line);
	    charP = strtok(line, white);
	    // printf("charP: %s\n", charP);
	    charP = strtok(NULL, white);
	    // printf("charP: %s\n", charP);
	    charP = strtok(NULL, "\n");
	    // printf("charP: %s\n", charP);
	    destInfo[i].fullname = strdup(charP);
	    
	}  
    } // while

   

    fclose(fp);
        
    return 0;
} // readFile


void
NetTop::save(tuGadget *) {
    if (doSave() == -1)
	openDialogBox();
}


int
NetTop::doSave() {
    // printf("NetTop::doSave\n");

    FILE *fp = NULL;
    fp = fopen(lastFile, "w");

    if (fp == NULL) {
	dialog->warning(errno, "Could not write %s", lastFile);
	return - 1;
    }


    // traffic gizmo parameters
    fprintf(fp, "SnoopInterface\t%s\n", fullSnooperString);

    fprintf(fp, "SnoopFilter\t\t%s\n", filter);

    fprintf(fp, "MeasureTraffic\t%s\n", graphTypes[graphType]);

    if (graphType == TYPE_PNPACKETS)
	fprintf(fp, "NPackets\t%d\n", nPackets);
    else if (graphType == TYPE_PNBYTES)
	fprintf(fp, "NBytes\t%d\n", nBytes);

    fprintf(fp, "DataUpdateTime\t%f\n", (float)dataUpdateTime / 10.0);

    fprintf(fp, "InterpolatePercent\t%d\n", dataSmoothPerc);

    if (viewgadget)
	fprintf(fp, "VerticalScale\t%s\n", rescaleTypes[viewgadget->getRescaleType()]);

    if (datagizmo)
        fprintf(fp, "LockAt\t\t%d\n", datagizmo->getFixedScale());

    fprintf(fp, "ReduceAfter\t%f\n", (float)scaleUpdateTime / 10.0);

    // node gizmo parameters
    fprintf(fp, "LabelBy\t\t");
    switch(labelType) {
	case DMFULLNAME:
	    fprintf(fp, "Name\n");
	    break;
	case DMINETADDR:
	    fprintf(fp, "Address\n");
    }

    fprintf(fp, "Display\t\t%s\n", axesTypes[whichAxes]);

    switch (whichAxes) {
	case AXES_SRCS_DESTS:
	    fprintf(fp, "Sources\t\t%d\n", numSrcs);
	    fprintf(fp, "Destinations\t%d\n", numDests);
	    fprintf(fp, "ShowSourceNodes\t%s\n", genericTypes[whichSrcs]);
	    fprintf(fp, "ShowDestinationNodes\t%s\n", genericTypes[whichDests]);
	    break;
	case AXES_BUSY_PAIRS:
	    fprintf(fp, "Pairs\t\t%d\n", numSrcs);
	    break;
	case AXES_NODES_FILTERS:
	    fprintf(fp, "Nodes\t\t%d\n", numSrcs);
	    fprintf(fp, "Filters\t\t%d\n", numDests);
	    fprintf(fp, "ShowNodes\t%s\n", genericTypes[whichNodes]);
	    break;	
    }
	    
    fprintf(fp, "BusiestMeans\t%s\n", 
		busyBytes ?  "MostBytes" : "MostPackets");
    fprintf(fp, "NodeUpdateTime\t%f\n", (float)nodeUpdateTime / 10.0);
	    
    // node names
    for (int i = 2; i < MAX_NODES; i++) {
	switch (whichAxes) {
	    case AXES_SRCS_DESTS:
	    case AXES_BUSY_PAIRS:
		fprintf(fp, "Source %d\t", i);
		break;
	    case AXES_NODES_FILTERS:
		fprintf(fp, "Node %d\t\t", i);
		break;	
	}
	fprintf(fp, "%s", srcInfo[i].fullname);
	if (srcInfo[i].physaddr)
	    fprintf(fp, " %s\n", srcInfo[i].physaddr);
	else if (srcInfo[i].ipaddr)
	    fprintf(fp, " %s\n", srcInfo[i].ipaddr);
	else
	    fprintf(fp, " \n");
    } 

    for (i = 2; i < MAX_NODES; i++) {
	switch (whichAxes) {
	    case AXES_SRCS_DESTS:
	    case AXES_BUSY_PAIRS:
		fprintf(fp, "Destination %d\t", i);
		break;
	    case AXES_NODES_FILTERS:
		fprintf(fp, "Filter %d\t", i);
		break;	
	}
	fprintf(fp, "%s", destInfo[i].fullname);
	if (destInfo[i].physaddr)
	    fprintf(fp, " %s\n", destInfo[i].physaddr);
	else if (destInfo[i].ipaddr)
	    fprintf(fp, " %s\n", destInfo[i].ipaddr);
	else
	    fprintf(fp, " \n");
    } 
	    
    // window geometry
    if (viewgadget)
	fprintf(fp, "NetTopGeometry\t%dx%d%+d%+d\n",
		viewgadget->getWidth(),   viewgadget->getHeight(),
		viewgadget->getXOrigin(), viewgadget->getYOrigin());
    if (datagizmo && datagizmo->isMapped()) {
	fprintf(fp, "TrafficControlGeometry\t%dx%d%+d%+d\n",
		    datagizmo->getWidth(),   datagizmo->getHeight(),
		    datagizmo->getXOrigin(), datagizmo->getYOrigin());
    }
    if (nodegizmo && nodegizmo->isMapped()) {
	fprintf(fp, "NodeControlGeometry\t%dx%d%+d%+d\n",
		    nodegizmo->getWidth(),   nodegizmo->getHeight(),
		    nodegizmo->getXOrigin(), nodegizmo->getYOrigin());
    }


    fclose(fp);

    return 0;

} // doSave


void
NetTop::saveAs(tuGadget *) {
    if (prompter == 0) {
	prompter = new tuFilePrompter("prompter", viewgadget, True);
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
		new NetTopCallBack(this, NetTop::savePrompt));
    prompter->mapWithCancelUnderMouse();
}

void
NetTop::savePrompt(tuGadget *)
{
    char *file = prompter->getSelectedPath();
    prompter->unmap();
    if (file == 0)
	return;

    strcpy(lastFile, file);
    if (doSave() == -1)
	openDialogBox();
}



void
NetTop::saveImageAs(tuGadget *) {
//    printf("NetTop::save image as\n");
}



// post error or warning message
void
NetTop::post(int msg, char* errString, int lineNumber) {
    // once a fatal error is posted, don't post any more
    // (this is important, because if snoop read problem, might
    // continue to get more of them, and don't want lots of dialogs)
    if (postedFatal)
	return;
    if (fatalError)
	postedFatal = True;
	
    char msgBuf[250];

    switch (msg)
    {
	case ILLEGALINTFCMSG:
	    sprintf(msgBuf, "Data Station name not recognized.\n\nCheck that %s is a valid name and is entered in the appropriate hosts data base.  See Appendix A of the NetVisualyzer User's Guide for detailed help", errString);
            break;

	case NOSNOOPDMSG:
	case SSSUBSCRIBEERRMSG:
	    sprintf(msgBuf, "Could not connect to %s\n\nCheck that snoopd is installed and configured correctly on the Data Station. See Appendix A of the NetVisualyzer User's Guide for detailed help", exc_message);
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

    }
    
    if (fatalError) {
	// we'll make a new dialog box, because otherwise sometimes
	// have trouble with it coming up iconified
	dialog->markDelete();
	dialog = new DialogBox("NetTop", viewgadget, False);
	dialog->error(msgBuf);
    } else
	dialog->warning(msgBuf);
    
    openDialogBox();
} // post

// this isn't used anywhere currently (but it s/b someday, so leave it)
void
NetTop::postMany(int count, char* msgs[]) {
    // xxxx
    for (int i = 0; i < count; i++)
	printf("%s\n", msgs[i]);
}

void
NetTop::openDialogBox(void) {
    if (fatalError && quitNow)
	dialog->setCallBack(new NetTopCallBack(this, NetTop::immediateQuit));
    else
	dialog->setCallBack(new NetTopCallBack(this, NetTop::closeDialogBox));

    if (fatalError) {
	dialog->unmap();
	dialog->map();
	XFlush(getDpy());
    } else if (dialog->isMapped()) {
	dialog->pop();
    } else {
	dialog->map();
	XFlush(getDpy());
    }
    
}

void
NetTop::closeDialogBox(tuGadget *) {
    dialog->unmap();
    XFlush(getDpy());
    
    if (fatalError)
	quit(NULL);
}

void
NetTop::closeLicDialogBox(tuGadget *) {
    licDialog->unmap();
    XFlush(getDpy());
}

void
NetTop::setDialogBoxCallBack(void) {
    dialog->setCallBack(new NetTopCallBack(this, NetTop::closeDialogBox));
}


int
NetTop::openNetFilters() {
    if (netfiltersPid != 0)
	return 1;

    int fd[2];
    if (pipe(fd) != 0) {
	// fprintf(stderr, "Could not start NetFilters\n");
	dialog->warning(errno, "Could not start NetFilters");
	return 0;
    }

    netfiltersPid = fork();
    if (netfiltersPid < 0) {
	// fprintf(stderr, "Could not start NetFilters\n");
	dialog->warning(errno, "Could not start NetFilters");
	return 0;
    }
    if (netfiltersPid == 0) {
	if (dup2(fd[1], 1) < 0)
	    exit(-1);
	execlp("netfilters", "netfilters", "-stdout", 0);
        perror("nettop: Could not start NetFilters");
	exit(-1);
    }
    close(fd[1]);
    netfilters = fdopen(fd[0], "r");
    
    exec->addCallBack(fd[0], False,
	    new NetTopCallBack(this, NetTop::readNf));
    
    return 1;
} // openNetFilters

void
NetTop::readNf(tuGadget *) {
    if (lastGizmoFocused == nodegizmo)
	readNfForNodeG(NULL);
    else
	readNfForDataG(NULL);
}

// read the result from NetFilters, tell the nodeGizmo
void
NetTop::readNfForNodeG(tuGadget *) {
    char buf[256];
    if (fgets(buf, 256, netfilters) == 0) {
	closeNetFilters();
	return;
    }
    char* nl = strchr(buf, '\n');
    *nl = NULL;
    // printf("readNfForNodeG: %s\n", buf);
    tuGadget* field = nodegizmo->getFocusedGadget();

    if (field)
	field->setText(buf);
    // we don't know if this is a source or dest field, so try both.
    // (although it should be a destination field, if filter is going there)
    nodegizmo->getSrcName(field);
    nodegizmo->getDestName(field);    

}

// read the result from NetFilters, tell the dataGizmo
void
NetTop::readNfForDataG(tuGadget *) {
    char buf[256];
    if (fgets(buf, 256, netfilters) == 0) {
	closeNetFilters();
	return;
    }
    char* nl = strchr(buf, '\n');
    *nl = NULL;
    // printf("readNfForDataG: %s\n", buf);

    datagizmo->setFilter(buf);
    setFilter(buf);

}

void
NetTop::closeNetFilters(void)
{
    exec->removeCallBack(fileno(netfilters), False);
    fclose(netfilters);
    kill(netfiltersPid, SIGTERM);
    netfiltersPid = 0;
}


int
NetTop::setInterface(char* newInterfaceName) {
    // printf("NetTop::setInterface(%s)\n", newInterfaceName);
    // printf("   old: <%s>:<%s>,  new: <%s>:<%s>\n",
    //	currentSnooper.node, currentSnooper.interface, 
    //	newSnooper.node, newSnooper.interface);

    if (newInterfaceName && (!strcmp(newInterfaceName, ""))) {
	newInterfaceName = (char*) malloc (strlen(localHostName) + 1);
	sprintf(newInterfaceName, "%s:", localHostName);
    }
    
    char oldFullSnooperString[100];
    strcpy(oldFullSnooperString, fullSnooperString);
    if (handleIntfc(newInterfaceName) == -1) {
	// printf("NetTop::setInterface - handleIntfc FAILED\n");
	strcpy(fullSnooperString, oldFullSnooperString);
	datagizmo->setInterface(fullSnooperString);
	return -1;
    }
	// printf("NetTop::setInterface - handleIntfc PASSED\n");

      // note: handleIntfc took newInterfaceName,
      // and turned it into currentSnooper
      if (strcmp(newSnooper.node, currentSnooper.node) ||
	  strcmp(newSnooper.interface, currentSnooper.interface)) {

	if ((restartSnooping(True, True) == -1) ||
	        (top->restartSnooping() == -1)) {
	    strcpy(fullSnooperString, oldFullSnooperString);
    	    datagizmo->setInterface(fullSnooperString);
	    return -1;
	}
      } // if snooper changed
      
    // printf("   old: <%s>:<%s>,  new: <%s>:<%s>\n",
    //	currentSnooper.node, currentSnooper.interface, 
    //	newSnooper.node, newSnooper.interface);

    return 0;
} // setInterface


void
NetTop::setFilter(char* f) {
    delete [] filter;
    // first, skip over any leading blankspaces
    char *tmp = f;
    while (*tmp == ' ')
	tmp++;
	
    filter = strdup(tmp);
    if (filter[0])
	tooloptions->setFilter(filter);
    else
	tooloptions->setFilter(NULL);

    if (snooping)
	restartSnooping(False, True);
    if (top)
	top->newFilter();
} // setFilter

int
NetTop::addFilter(SnoopStream* snpStmP, char* filtExpr) {
    if (!filtExpr)
	return -99;
	
    ExprSource src;
    ExprError err;
    
    src.src_path = "NetTop";
    src.src_line = 0;
    src.src_buf = filtExpr;
    
    int bin = ss_compile(snpStmP, &src, &err);
    if (bin < 0) {
	char msg[160];
	strcpy(msg, "Invalid filter -- ");
	if (err.err_source.src_buf) {
	    strcat(msg, err.err_source.src_buf);
	    strcat(msg, " : ");
	}
	if (err.err_message)
	    strcat(msg, err.err_message);
	if (err.err_token && (err.err_token[0] != '\0')) {
	    strcat(msg, " : ");
	    strcat(msg, err.err_token);
	} 

	dialog->warning(msg);
	// printf("   whole filter is <%s>\n", filtExpr); // xxx
    }
    
    return bin;

} // addFilter

void
NetTop::addMatrixFilters(SnoopStream* snpStmP) {
    char filtExpr[400]; // xxx length?
    int bin;
    int s, d;
    tuBool generalFilter = strcmp(filter, "");
    char* filt;
    tuBool anyErrors = False;

    dialog->setMultiMessage(True);
    
    //	    SOURCES
    for (s = numSrcs-1; s >= 0; s--) {
	if (srcInfo[s].filter) delete [] srcInfo[s].filter;
	if (s == totalSrc) {

	    if (generalFilter)
		srcInfo[s].filter = strdup(filter);
	    else
		srcInfo[s].filter = strdup("snoop");
	    filt = srcInfo[s].filter;

	} else if (s == otherSrc) {
	    filtExpr[0] = NULL;
	    for (int ss = 0; ss < numSrcs; ss++) {
		// if this src has a bad filter, don't want to include it,
		if (ss == otherSrc || ss == totalSrc ||
		    srcInfo[ss].err ||
		    !srcInfo[ss].filter || !srcInfo[ss].filter[0])
		    continue;
		strcat(filtExpr, " !(");
		strcat(filtExpr, srcInfo[ss].filter);
		strcat(filtExpr, ") && ");
	    } // for ss

	    // build the filter for "other"
	    if (generalFilter)	    
		strcat(filtExpr, filter);
	    else
		strcat(filtExpr, " snoop");
		// (if no generalFilter, don't actually need "snoop",
		// but we already put a "&&" at the end of the others
	    
	    srcInfo[s].filter = strdup(filtExpr);
	    // printf("other source filter:\n\t%s\n", filtExpr);
	    filt = srcInfo[s].filter;

	} else {
	    
	    // regular sources	    
	    if (srcInfo[s].ipaddr) {
		if (doSrcAndDest)
		    sprintf(filtExpr, "ip.src == %s", srcInfo[s].ipaddr);
		else
		    sprintf(filtExpr, "ip.host(%s)", srcInfo[s].ipaddr);
		srcInfo[s].filter = strdup(filtExpr);
		
	    } else if (srcInfo[s].physaddr) {
		if (doSrcAndDest)
		    sprintf(filtExpr, "src == %s", srcInfo[s].physaddr);
		else
		    sprintf(filtExpr, "host(%s)", srcInfo[s].physaddr);
		srcInfo[s].filter = strdup(filtExpr);
		
	    } else if (srcInfo[s].fullname[0]) {
		if (ether_aton(srcInfo[s].fullname) != NULL) {
		    srcInfo[s].physaddr = strdup(srcInfo[s].fullname);
		    // if has a "/vendor" at the end, get rid of it.
		    char* slash = strchr(srcInfo[s].physaddr, '/');
		    if (slash)
			*slash = NULL;
		    if (doSrcAndDest)
			sprintf(filtExpr, "src == %s", srcInfo[s].physaddr);
		    else
			sprintf(filtExpr, "host(%s)", srcInfo[s].physaddr);
		} else
		    if (doSrcAndDest)
			sprintf(filtExpr, "ip.src == %s", srcInfo[s].fullname);
		    else
			sprintf(filtExpr, "ip.host(%s)", srcInfo[s].fullname);
		srcInfo[s].filter = strdup(filtExpr);
		
	    } else {
		srcInfo[s].filter = NULL;
	    }
	    
	    if (generalFilter && srcInfo[s].filter) {
		sprintf(filtExpr, "%s && %s", srcInfo[s].filter, filter);
		filt = filtExpr;
	    } else
		filt = srcInfo[s].filter;
	}
	
	/****
	printf("source %d; name=%s, ipaddr=%s, physaddr=%s, filter=%s\n", 
		s, srcInfo[s].fullname, srcInfo[s].ipaddr,
		srcInfo[s].physaddr, filt);
	****/
	
	bin = addFilter(snpStmP, filt);
	if (bin < 0) {
	    srcInfo[s].err = True;
	    addFilter(snpStmP, "0");
	    if (bin != -99) anyErrors = True;
	} else
	    srcInfo[s].err = False;	 
	       
	// printf("%s, bin = %d\n", filt, bin);
	// if (srcInfo[s].err) printf(" ==== Err,  so filter = \"0\"\n");
    } // for s


    //	    DESTINATIONS
    for (d = numDests-1; d >= 0; d--) {
	if (destInfo[d].filter) delete [] destInfo[d].filter;
	if (d == totalDest) {

	    if (generalFilter)
		destInfo[d].filter = strdup(filter);
	    else
		destInfo[d].filter = strdup("snoop");
	    filt = destInfo[d].filter;

	} else if (d == otherDest) {
	    filtExpr[0] = NULL;
	    for (int dd = 0; dd < numDests; dd++) {
		// if this dest has a bad filter, don't include it
		if (dd == otherDest || dd == totalDest ||
		    destInfo[dd].err ||
		    !destInfo[dd].filter || !destInfo[dd].filter[0])
		    continue;
		strcat(filtExpr, " !(");
		strcat(filtExpr, destInfo[dd].filter);
		strcat(filtExpr, ") && ");
	    } // for dd
	    
	    // build the filter for "other"
	    if (generalFilter)
		strcat(filtExpr, filter);
	    else
		strcat(filtExpr, " snoop");
	    
	    destInfo[d].filter = strdup(filtExpr);
	    // printf("other dest filter:\n\t%s\n", filtExpr);
	    filt = destInfo[d].filter;

	} else {
	    
	    // regular destinations
	    if (doSrcAndDest) {
		// make a 'destination' filter
		if (destInfo[d].ipaddr) {
		    sprintf(filtExpr, "ip.dst == %s", destInfo[d].ipaddr);
		    destInfo[d].filter = strdup(filtExpr);
		} else if (destInfo[d].physaddr) {
		    sprintf(filtExpr, "dst == %s", destInfo[d].physaddr);
		    destInfo[d].filter = strdup(filtExpr);
		} else if (destInfo[d].fullname[0]) {
		    if (ether_aton(destInfo[d].fullname) != NULL) {
			destInfo[d].physaddr = strdup(destInfo[d].fullname);
			// if has a "/vendor" at the end, get rid of it.
			char* slash = strchr(destInfo[d].physaddr, '/');
			if (slash)
			    *slash = NULL; 
			sprintf(filtExpr, "dst == %s", destInfo[d].physaddr);
		    } else
			sprintf(filtExpr, "ip.dst == %s", destInfo[d].fullname);
		    destInfo[d].filter = strdup(filtExpr);
		} else {
		    destInfo[d].filter = NULL;
		}
	    } else {
		// use what the user gave us as the filter
		destInfo[d].filter = strdup(destInfo[d].fullname);
	    }
	    
	    if (generalFilter && destInfo[d].filter) {
		sprintf(filtExpr, "%s && %s", destInfo[d].filter, filter);
		filt = filtExpr;
	    } else
		filt = destInfo[d].filter;
	    
	}
	/****
	printf("dest %d; name=%s, ipaddr=%s, physaddr=%s, filter=%s\n", 
		d, destInfo[d].fullname, destInfo[d].ipaddr,
		destInfo[d].physaddr, filt);
	****/
	bin = addFilter(snpStmP, filt);
	if (bin < 0) {
	    destInfo[d].err = True;
	    addFilter(snpStmP, "0");
	    if (bin != -99) anyErrors = True;
	} else
	    destInfo[d].err = False;	 
	       
	// printf("%s, bin = %d\n", filt, bin);
	// if (destInfo[d].err) printf(" ==== Err,  so filter = \"0\"\n");
    } // for d

    
    highestBin = bin;
    
    // calculate bin numbers (same as snoopd does) just once
    for (s = numSrcs-1, bin = 0; s >= 0; s--) {
	for (d = numDests-1; d >= 0; d--) {
	    bins[s][d] = bin++;
	}
    }
    
    if (anyErrors)
	openDialogBox();

    dialog->setMultiMessage(False);

} // addMatrixFilters


// set up snooping services
int
NetTop::snoopInit() {
    // printf("snoopInit: newSnooper: (%s).(%s)\n", newSnooper.node, newSnooper.interface);

    /*
     * Initialize protocols
     * XXX We must disable NIS temporarily, as protocol init routines
     * XXX tend to call getservbyname, which is horrendously slow thanks
     * XXX to Sun's quadratic-growth lookup botch.
     */
    int old_yp_disabled = _yp_disabled;
    _yp_disabled = 1;
    initprotocols();
    _yp_disabled = old_yp_disabled;

    bzero(&hist, sizeof hist);
    bzero(&oldHist, sizeof oldHist);
    zeroData();
    
    // get host, set up snoop stream
    int srcs = numSrcs;
    int dests = numDests;

    int retVal = hist_init(&histSnpStm, &newSnooper, srcs, dests,
			   dataUpdateTime);
    // printf("snoop pr_name: %s, pr_title: %s\n",
    // histSnpStm.ss_rawproto->pr_name, histSnpStm.ss_rawproto->pr_title);

    if (retVal < 0) {
	histSnpStm.ss_sock = -1;
	fatalError = True;
	quitNow = True;
	char ebuf[64];
	sprintf (ebuf, "%s:%s", newSnooper.node, newSnooper.interface);
	post(-1 * retVal, ebuf);
	return -1;
    } else
	snooperChanged();

// subscribe to snoop OK.

    subscribed = True;
// printf("Initially subscribed OK......\n");
    addMatrixFilters(&histSnpStm);
       
    if (!snoopStart(&histSnpStm, True)) {
	post(NOSTARTSNOOPMSG);
	return -1;
    }
   
    return 0;
    
} // snoopInit

tuBool
NetTop::isEther() {
    return (histSnpStm.ss_rawproto &&
	  !strcmp(histSnpStm.ss_rawproto->pr_name, "ether"));
}

tuBool
NetTop::isFddi() {
    return (histSnpStm.ss_rawproto &&
	  !strcmp(histSnpStm.ss_rawproto->pr_name, "fddi"));
}

void
NetTop::setExec(tuXExec *e) {
    exec = e;
    if (histSnpStm.ss_sock != -1)
	exec->addCallBack(histSnpStm.ss_sock, False,
	    new NetTopCallBack(this, &NetTop::readData));
    
    if (top)
	top->setExec(e);
    // xxx really need to see what kind of scaling, etc.
    // only do this if timed scaling
    viewgadget->setScaleUpdateTime(scaleUpdateTime);
} // setExec


int
NetTop::snoopStart(SnoopStream* streamp, tuBool doEvent) {
    if (snooping)
	return 0;

    if (streamp->ss_sock == -1)
	return 0;
	
    if (!ss_start(streamp))
	return 0;
    
    snooping = True;
    
    if (doEvent) {
	event->setType(NV_START_SNOOP);
	event->setInterfaceName(newSnooper.node);
	event->setFilter(filter);
	eh->send(event);
    }
    
    return 1;
}

int
NetTop::snoopStop(SnoopStream* streamp, tuBool doEvent) {
    if (!snooping)
	return 0;
	
    if (streamp->ss_sock == -1)
	return 0;
	
    if (!ss_stop(streamp))
	return 0;
    
    snooping = False;
    
    if (doEvent) {
	event->setType(NV_STOP_SNOOP);
	event->setInterfaceName(currentSnooper.node);
	eh->send(event);
    }
	
    return 1;
}



// stop snooping, delete filters, unsubscribe,
// resubscribe, add filters, start snooping again.
// This gets called if number of nodes changes, if filter changes,
// and also if interface changes.
// If interface changes, reopen and resubscribe.
// For anything else, don't reopen, but do resubscribe.
int
NetTop::restartSnooping(tuBool mustReopen, tuBool doEvent) {
    /***
    printf("restartSnooping(%s): currentSnooper: (%s):(%s), newSnooper: (%s):(%s)\n",
	mustReopen?"T":"F",
	currentSnooper.node, currentSnooper.interface,
	newSnooper.node, newSnooper.interface);
    ***/

    // do like netgraph does now, if mustReopen:
    // stop (but don't close) existing snooper, try to start new one.
    // if not successful, then restart existing one.
    // if successful, then delete/close old one, copy new to old, etc..

    SnoopStream newSnpStm;
    newSnpStm.ss_sock = -1;
    int oldHighestBin = highestBin;
    int srcs = numSrcs;
    int dests = numDests;
    
    bzero(&hist, sizeof hist);
    bzero(&oldHist, sizeof oldHist);
    // printf("NetTop::restartSnooping(),  zeroData()\n");
    zeroData();
	    

    // first stop data-smoothing timer; it will be restarted when we first get
    // get new data.
    smoothTimer->stop();

    if (snooping) {
    	if (!snoopStop(&histSnpStm, doEvent)) {
	    post(NOSTOPSNOOPMSG);
	    return -1;
    	}
    }


    if (mustReopen) {
	// try to start new snooper.
	// if ok, delete old one, else restart old one	
	
 	int retVal = hist_init(&newSnpStm, &newSnooper,
				   srcs, dests, dataUpdateTime);
	if (retVal < 0) {
	    // printf("hist_init returned %d\n", retVal);
	    // char ebuf[300];
	    // sprintf (ebuf, "%s:%s", newSnooper.node, newSnooper.interface);
	    post(-1 * retVal);
	    // init of new one failed, so restart existing one
	    strcpy(newSnooper.node,  currentSnooper.node);
	    strcpy(newSnooper.interface,  currentSnooper.interface);
	    (void) snoopStart(&histSnpStm, doEvent);
	    return -1;
	} else
	    snooperChanged();

	subscribed = True; // (hist_init called ss_subscribe)
	
	exec->addCallBack(newSnpStm.ss_sock, False,
		new NetTopCallBack(this, &NetTop::readData));


	addMatrixFilters(&newSnpStm);

	if (!snoopStart(&newSnpStm, doEvent)) {
	    post(NOSTARTSNOOPMSG);
	    // init of new one failed, so restart existing one
	    highestBin = oldHighestBin;
	    strcpy(newSnooper.node,  currentSnooper.node);
	    strcpy(newSnooper.interface,  currentSnooper.interface);
	    (void) snoopStart(&histSnpStm, doEvent);
	    return -1;
	}
	
	// new one worked, so close existing one
	
	if (histSnpStm.ss_sock != -1) {
	    for (int bin = oldHighestBin; bin >= 0; bin--)
		(void) ss_delete(&histSnpStm, bin);
   
	    (void) ss_unsubscribe(&histSnpStm);
	    ss_close(&histSnpStm);
	    exec->removeCallBack(histSnpStm.ss_sock);
	}

	histSnpStm = newSnpStm;
	exec->addCallBack(histSnpStm.ss_sock, False,
	    new NetTopCallBack(this, &NetTop::readData));
	
	return 0;
	
    } else {
	// !mustReopen
	// clean up snooping, resubscribe
	if (histSnpStm.ss_sock != -1) {
	    for (int bin = highestBin; bin >= 0; bin--) {
		if (!ss_delete(&histSnpStm, bin)) {
		    post(NODELFILTERMSG);
		    return -1;
		}
	    }
	}
	highestBin = -1;

	if (subscribed) {
	    if (histSnpStm.ss_sock != -1 && !ss_unsubscribe(&histSnpStm)) {
		post(SSUNSUBSCRIBEERRMSG);
		return -1;
	    }
	    subscribed = False; // unsubscribe suceeded..!
	}
	
	// just resubscribe, etc.
	char* interface;
	if (currentSnooper.interface[0])
	    interface = currentSnooper.interface;
	else
	    interface = NULL;
	if (!ss_subscribe(&histSnpStm, SS_HISTOGRAM, interface, 
			      srcs, dests, dataUpdateTime)) {
	    post(SSSUBSCRIBEERRMSG);
	    return -1;
	}

	subscribed = True;

	if (histSnpStm.ss_sock != -1 &&
	       !ss_setsnooplen(&histSnpStm, 128)) {
	    post(SETSNOOPLENMSG);
	    return -1;
	}

	addMatrixFilters(&histSnpStm);

	if (!snoopStart(&histSnpStm, doEvent)) {
	    post(NOSTARTSNOOPMSG);
	    return -1;
	}

	return 0;
	
    } // !mustReopen

} // restartSnooping


// parse "command line" options, from the command line 
int
NetTop::handleOptions(int argc, char** argv)
{
    
    // printf("handleOptions:\n");
    // for (int c=0;c<argc;c++) printf("\t%d: %s\n", c, argv[c]);

    int opt;
    char* options = "i:t:T:d:s:I:S:O";

    int optionErrs = 0;
    opterr = 0; // disable error messages from getopt
    optind = 1;
    while ((opt = getopt(argc, argv, options)) != -1) {	    

	switch (opt) {
	    case 'i':
		{
                   if (gotInterfaceFromResource)
                        continue;
 		    strcpy(fullSnooperString, optarg);
		    char* tempStr = strdup(optarg);		
		    fatalError = True; // since might be posted by handleIntfc()
		    quitNow = True;
		    if (handleIntfc(tempStr) == -1)
			return -1;
		    fatalError = False; // return this to normal value
		    quitNow = False;
		    snooperChanged();
		}
		break;
	    case 't':
		// turn into integer # of tenths and round off
		dataUpdateTime = (int) (atof(optarg) * 10.0 + 0.5 );
		break;
	    case 'T':
		// turn into integer # of tenths and round off
		nodeUpdateTime  = (int) (atof(optarg) * 10.0 + 0.5 );
		break;
	    case 'd':
		numDests = atoi(optarg);
		break;
	    case 's':
		numSrcs = atoi(optarg);
		break;
	    case 'I':
	    	dataSmoothPerc = atoi(optarg);
		break;
	    case 'S':
		// turn into integer # of tenths and round off
		scaleUpdateTime = (int) (atof(optarg) * 10.0 + 0.5);
		break;
	    case 'O':
		doTopOutput = True;
		break;

	    case '?':
		// If it's a valid option for us, must be a bad argument
		if (strchr(options, optopt) != NULL) {
		    char tmpStr[80];
		    sprintf(tmpStr,
			"-%c option requires an argument", optopt);
		    fatalError = True;
                    dialog->error(tmpStr);
                    openDialogBox();
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
	setFilter(buf);
    }

    return 0;
} // handleOptions (argc,argv)


void
NetTop::usage()
{
/*****
    char* msgs[5];
    msgs[0] =
"usage: nettop [-h] [-u controls_file] [-i interface]\n\n";
    msgs[1] =
"\t[-t data_update_time] [-T node_update_time]\n\n";
    msgs[2] =
"\t[-s number_sources] [-d number_destinations]\n\n";
    msgs[3] =
"\t[-I interpolation_percentage] [-S scale_recalc_time]\n\n";
    msgs[4] =
"\t[-y] [-O] [filter]";

    // this causes "." after each line
    dialog->setMultiMessage(True); 
    for (int i = 0; i < 5; i++)
	dialog->information(msgs[i]);
******/

    dialog->information(
"usage: nettop [-h] [-u controls_file] [-i interface]\n\t[-t data_update_time] [-T node_update_time]\n\t[-s number_sources] [-d number_destinations]\n\t[-I interpolation_percentage] [-S scale_recalc_time]\n\t[-y] [-O] [filter]");

    fatalError = True;
    quitNow = True;
    openDialogBox();

} // usage


// if the snooper node or interface changed, this is called.
void
NetTop::snooperChanged() {
    /****
    printf("NetTop::snooperChanged\n");
    printf("old: <%s>:<%s>,  new: <%s>:<%s>\n",
	currentSnooper.node, currentSnooper.interface, 
	newSnooper.node, newSnooper.interface);
    ****/
    
    strcpy(currentSnooper.node, newSnooper.node);
    strcpy(currentSnooper.interface, newSnooper.interface);

    if (fullSnooperString[0]) {
	sprintf(windowName, "NetTop -i %s", fullSnooperString);
	tooloptions->setInterface(fullSnooperString);
    } else {
	char tempStr[80];
	sprintf(tempStr, "%s:", localHostName); // so analyzer is happy
	sprintf(windowName, "NetTop -i %s", tempStr);
	tooloptions->setInterface(tempStr);
    }

    viewgadget->setName(windowName);

}


// take an interface name, figure out what kind of snooping is required,
// fill in newSnooper.
int
NetTop::handleIntfc(char* name) {
    // printf("NetTop::handleIntfc(%s)\n", name);

    if (name)
	strcpy(fullSnooperString, name);
    else
	fullSnooperString[0] = NULL;

 
    char* newInterface;

    enum snoopertype snptype = getsnoopertype(name, &newInterface);

    strcpy(newSnooper.node, name);

    switch (snptype) {
	case ST_LOCAL:
	    strcpy(newSnooper.node, localHostName);
	    break;
	case ST_TRACE:
	    break;
	case ST_REMOTE:
	    if (!newInterface)
		newInterface = strdup(name);

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

    return 0;
} // handleIntfc


// clear out all data arrays
void
NetTop::zeroData() {
    for (int s = 0; s < MAX_NODES; s++)
	for (int d = 0; d < MAX_NODES; d++) {
	    data[s][d] = 0.0;
	    newData[s][d] = 0.0;
	    deltaData[s][d] = 0.0;
	}
}

// xxx this will do the snoop read, when that's available.
void
NetTop::readData(tuGadget*) {
    if (histSnpStm.ss_sock == -1)
	return;
    if (! hist_read(&histSnpStm, &hist)) {
	setFatal();
	post(SSREADERRMSG);
	return;
    }
    
    int s, d;
    if (!useLogSmoothing) {
	// this shouldn't be necsry,
	// since data should have caught up to newData by the time more
	// newData comes in. But maybe not? Seems that timer is slower
	// than it should be, or snooping is faster (or maybe it's the
	// time I'm wasting?) But I don't get enough steps in at 100%

	for (s = 0; s < numSrcs; s++)
	    for (d = 0; d < numDests; d++)
		data[s][d] = newData[s][d];
    } // linear smoothing
    
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
	advanced = (int) (deltaTime * 10.0 / dataUpdateTime + .5);
    }

    oldHist.h_timestamp.tv_sec = hist.h_timestamp.tv_sec;
    oldHist.h_timestamp.tv_usec = hist.h_timestamp.tv_usec;
    int trueIntvl = dataUpdateTime * advanced;

    // count_read, via ss_read, returns cumulative numbers, so we have
    // to subtract the previous values.

    float temp;
    // xxx
    if (hist.h_bins != (numSrcs * numDests)) {
	// printf("readData: hist.h_bins = %d\n", hist.h_bins); 
	setFatal();
	post(SSREADBINSSG);
	return;
    }
    
    for (unsigned int bin = 0; bin < hist.h_bins; bin++) {
	temp = hist.h_count[bin].c_bytes;
	hist.h_count[bin].c_bytes -= oldHist.h_count[bin].c_bytes;
	oldHist.h_count[bin].c_bytes = temp;

	temp = hist.h_count[bin].c_packets;
	hist.h_count[bin].c_packets -= oldHist.h_count[bin].c_packets;
	oldHist.h_count[bin].c_packets = temp;
    }
    
    lastMax = currentMax;
    currentMax = 0.0;

    float totalPackets = hist.h_count[bins[totalSrc][totalDest]].c_packets;
    float totalBytes   = hist.h_count[bins[totalSrc][totalDest]].c_bytes;
    
    // Update and rescale the matrix of data    
    for (s = 0; s < numSrcs; s++) {
      for (d = 0; d < numDests; d++) {

	switch (graphType) {
	    case TYPE_PACKETS:
		newData[s][d] = 10.0 * hist.h_count[bins[s][d]].c_packets / trueIntvl;
		break;

	    case TYPE_BYTES:
		newData[s][d] = 10.0 * hist.h_count[bins[s][d]].c_bytes / trueIntvl;
		break;

	    case TYPE_PPACKETS:
		if (totalPackets) {
		    newData[s][d] = 100.0 * hist.h_count[bins[s][d]].c_packets /
				     totalPackets;
		} else {
		    newData[s][d] = 0;
		}
		break;

	    case TYPE_PBYTES:
		if (totalBytes) {
		    newData[s][d] = 100.0 * hist.h_count[bins[s][d]].c_bytes /
				    totalBytes;
		} else {
		    newData[s][d] = 0;
		}
		break;

	    case TYPE_PNPACKETS:
		newData[s][d] = 100.0 * hist.h_count[bins[s][d]].c_packets /
					(nPackets * trueIntvl / 10.0);
		break;
		
	    case TYPE_PNBYTES:
		newData[s][d] = 100.0 * (hist.h_count[bins[s][d]].c_bytes)/
				  (nBytes * trueIntvl / 10.0);
		break;
	    case TYPE_PETHER:
                newData[s][d] = 100.0 * (hist.h_count[bins[s][d]].c_bytes +
                                  ETHER_HEAD * hist.h_count[bins[s][d]].c_packets)/
                                  (ETHER_CAPACITY * trueIntvl / 10.0);
 		break;
	    case TYPE_PFDDI:
                newData[s][d] = 100.0 * (hist.h_count[bins[s][d]].c_bytes +
                                  FDDI_HEAD * hist.h_count[bins[s][d]].c_packets)/
                                  (FDDI_CAPACITY * trueIntvl / 10.0);
		break;

	} // switch

	if ((s != totalSrc || d != totalDest) && newData[s][d] > currentMax)
	    currentMax = newData[s][d];

      } // each dest
    } // each src
    
    tuBool badData = False; // xxx

    // set up things for data smoothing
    if (numSteps > 0) {
	for (s = 0; s < numSrcs; s++)
	    for (d = 0; d < numDests; d++)
		deltaData[s][d] = (newData[s][d] - data[s][d]) / numSteps;
	
	stepsTaken = 0;
	smoothTimer->start(); // this used to give dataSmoothInterval, but I don't thing it's necessary, because it hasn't changed.
    } else {
	for (s = 0; s < numSrcs; s++)
	    for (d = 0; d < numDests; d++)
		data[s][d] = newData[s][d];
    }
    renderGraph();
} // readData


// step each element of data array toward value in newData array,
// by appropriate increment in deltaData array.
void
NetTop::smoothData(tuGadget*) {
    if (useLogSmoothing) {
	for (int s = 0; s < numSrcs; s++)
	    for (int d = 0; d < numDests; d++)
		data[s][d] = (data[s][d] + newData[s][d]) / 2.0;
	
    } else {
	// printf("smooth: stepsTaken = %d\n", stepsTaken);
    
	// if data has gone all the way to newData, stop smoothing and stop timer
	if (stepsTaken++ >= numSteps) {
	    // printf("    stopping smooth timer\n");
	    smoothTimer->stop();
	    return;
	}
	
	for (int s = 0; s < numSrcs; s++)
	    for (int d = 0; d < numDests; d++) {
		data[s][d] += deltaData[s][d];
		if (data[s][d] < 0.0)
		    data[s][d] = 0.0;
	    }
    } // linear smoothing
    
    renderGraph();
} // smoothData

// True  => show source nodes vs destination nodes
// False => show nodes vs filters
void
NetTop::setSrcAndDest(tuBool b) {
    doSrcAndDest = b;
    viewgadget->setSrcAndDest(b);
    restartSnooping(False, False);
}

void
NetTop::setWhichAxes(int w) {
    // printf("NetTop::setWhichAxes(%s)\n", axesTypes[w]);
    switch(w) {
	case AXES_SRCS_DESTS:
	case AXES_BUSY_PAIRS:
	    doSrcAndDest = True;
	    break;
	case AXES_NODES_FILTERS:
	    doSrcAndDest = False;
	    break;
    } // switch
    
    viewgadget->setSrcAndDest(doSrcAndDest);
    whichAxes = w;
    restartSnooping(False, False);
    
    top->setWhichAxes(w);

}


void
NetTop::setNumNodes(int n) {
    // printf("NetTop::setNumNodes(%d)\n", n);
    setNumSrcs(n);
}
int
NetTop::getNumNodes() {
    // printf("NetTop::getNumNodes\n");
    return numSrcs;
}

void
NetTop::setNumFilters(int n) {
    // printf("NetTop::setNumFilters(%d)\n", n);
    setNumDests(n);
}
int
NetTop::getNumFilters() {
    // printf("NetTop::getNumFilters\n");
    return numDests; 
}


void
NetTop::setWhichSrcs(int w) {
    // printf("NetTop::setWhichSrcs(%s)\n", srcTypes[w]);
    whichSrcs = w;
    top->setWhichSrcs(w);
}

void
NetTop::setWhichDests(int w) {
    // printf("NetTop::setWhichDests(%s)\n", destTypes[w]);
    whichDests = w;
    top->setWhichDests(w);
}

void
NetTop::setWhichNodes(int w) {
    // printf("NetTop::setWhichNodes(%s)\n", nodeTypes[w]);
    whichNodes = w;
    top->setWhichNodes(w);
}


// nodeGizmo calls these to tell us the number of desired srcs/dests changed.
// We, in turn, pass the info on to viewgadget, who tells the graphgadget.
// Also, if need to stop snooping, remove the filters, re-add the right
// number of filters, restart snooping.

void
NetTop::setNumPairs(int n) {
    numSrcs = n;
    numDests = n;

    viewgadget->setNumSrcs(n);
    viewgadget->setNumDests(n);
    top->setNumSrcs(n);
    if (histSnpStm.ss_sock != -1)
	restartSnooping(False, False); // check return - if bad, quit? xxx
}

void
NetTop::setNumSrcs(int n) {
    numSrcs = n;
    viewgadget->setNumSrcs(n);
    if (histSnpStm.ss_sock != -1)
	restartSnooping(False, False); // check return - if bad, quit? xxx
    top->setNumSrcs(n);
}

void
NetTop::setNumDests(int n) {
    numDests = n;
    viewgadget->setNumDests(n);
    if (histSnpStm.ss_sock != -1)
	restartSnooping(False, False); // check return - if bad, quit? xxx
    top->setNumDests(n);
}

void
NetTop::setSrcLocked(int s, tuBool locked, tuBool tellNodeGizmo) {
    // don't do it if locking s/b disabled
    if (tellNodeGizmo &&
	  (whichAxes == AXES_BUSY_PAIRS ||
	  (whichAxes == AXES_SRCS_DESTS && whichSrcs == NODES_SPECIFIC) ||
	  (whichAxes == AXES_NODES_FILTERS && whichNodes == NODES_SPECIFIC)))
	return;

    // to be safe, don't allow locking/unlocking of total & other
    if (s == totalSrc || s == otherSrc)
	return;

    if (locked && !srcInfo[s].locked)
	numSrcsLocked++;
    else if (!locked && srcInfo[s].locked)
	numSrcsLocked--;
    srcInfo[s].locked = locked;
    // render, so name comes out in right color
    renderGraph();
    if (tellNodeGizmo)
	nodegizmo->setSrcLocked(s, locked);
}

void
NetTop::setDestLocked(int d, tuBool locked, tuBool tellNodeGizmo) {
    // don't do it if locking s/b disabled
    if (tellNodeGizmo &&
	  (whichAxes == AXES_BUSY_PAIRS ||
	  (whichAxes == AXES_SRCS_DESTS && whichDests == NODES_SPECIFIC) ||
	  (whichAxes == AXES_NODES_FILTERS)))
	return;

    // to be safe, don't allow locking/unlocking of total & other
    if (d == totalDest || d == otherDest)
	return;

    if (locked && !destInfo[d].locked)
	numDestsLocked++;
    else if (!locked && destInfo[d].locked)
	numDestsLocked--;
    destInfo[d].locked = locked;
    // render, so name comes out in right color
    renderGraph();
    if (tellNodeGizmo)
	nodegizmo->setDestLocked(d, locked);
}

int
NetTop::setDataUpdateTime(int t) {
// printf("NetTop::setDataUpdateTime(%d)\n", t);
    dataUpdateTime = t;
    // first stop data-smoothing timer; it will be restarted when we first get
    // get new data.
    smoothTimer->stop();
    

    if (snooping && !snoopStop(&histSnpStm, False)) {
	post(NOSTOPSNOOPMSG);
	// xxx maybe ss_close(&histSnpStm) & exit?
	return -1;
    }

    if (histSnpStm.ss_sock != -1 &&
           !ss_setinterval(&histSnpStm, dataUpdateTime)) {
	post(NOSETINTVLMSG);
	// xxx maybe ss_close(&histSnpStm) & exit?
	return -1;
    }

    if (!snoopStart(&histSnpStm, False)) {
	post(NOSTARTSNOOPMSG);
	// xxx maybe ss_close(&histSnpStm) & exit?
	return -1;
    }

    setDataSmoothPerc(dataSmoothPerc);
    
    return 0;
} // setDataUpdateTime

void
NetTop::startSmoothing() {
// printf("NetTop::startSmoothing\n");
    smoothTimer->start();
    numSteps = dataSmoothPeriod;
}

void
NetTop::stopSmoothing() {
// printf("NetTop::stopSmoothing\n");
    smoothTimer->stop();
    numSteps = 0;
}

void
NetTop::setLogSmooth(tuBool log) {
    useLogSmoothing = log;
}

int
NetTop::setDataSmoothPerc(int p) {
// printf("NetTop::setDataSmoothPerc(%d)\n", p);
    dataSmoothPerc = p;
    dataSmoothPeriod = (int) (.01 * dataUpdateTime * dataSmoothPerc + 0.5);
    smoothTimer->stop();
    // if percentage is 0, just stop smoothing 
    if (p == 0) {
	numSteps = 0;
	return 0;
    }
	
    // if we have less than two intervals over which to smooth, don't bother.
    // note: period is in (int) tenths, interval is in (float) seconds
    if (dataSmoothPeriod < 20 * dataSmoothInterval) {
	numSteps = -1;
	// numSteps == 0 means users says don't smooth, -1 means too short to smooth
	return 0;
    }
	
    smoothTimer->setDuration(dataSmoothInterval);
    // timer will be restarted when get next data (in readData)
    if (numSteps != 0)
	numSteps = dataSmoothPeriod;

    return 0; // -1 if didn't work xxx
} // setDataSmoothPerc

int
NetTop::setNodeUpdateTime(int t) {
    nodeUpdateTime = t;
    top->setNodeUpdateTime(t);
   return 0; // -1 if didn't work xxx
}

int
NetTop::setScaleUpdateTime(int t) {
    scaleUpdateTime = t;
    viewgadget->setScaleUpdateTime(scaleUpdateTime);
    return 0; // -1 if didn't work xxx
}

void
NetTop::setInclinationScrollBar(int i) {
     viewgadget->setInclinationScrollBar(i);
}

void
NetTop::setAzimuthScrollBar(int a) {
    viewgadget->setAzimuthScrollBar(a);    
}

void
NetTop::setDistanceScrollBar(int d) {
    viewgadget->setDistanceScrollBar(d);   
}


// NOTE: for this release, only do name, address, filter.
void
NetTop::setLabelType(int t) {
    labelType = t;
    for (int i = 0; i < MAX_NODES; i++) {
	switch(labelType) {
	    case DMFULLNAME:
		doFullName(&srcInfo[i]);
		doFullName(&destInfo[i]);
		break;
	    case DMHOSTNAME:
		doHostName(&srcInfo[i]);
		doHostName(&destInfo[i]);
		break;
	    case DMINETADDR:
		doInet(&srcInfo[i]);
		doInet(&destInfo[i]);
		break;
	    case DMDECNETADDR:
		doDecnet(&srcInfo[i]);
		doDecnet(&destInfo[i]);
		break;
	    case DMENETVENDOR:
		doEnetVen(&srcInfo[i]);
		doEnetVen(&destInfo[i]);
		break;
	    case DMENETADDR:
		doEnet(&srcInfo[i]);
		doEnet(&destInfo[i]);
		break;
	    case DMFILTER:
		srcInfo[i].displayStringPtr  = &srcInfo[i].filter;
		destInfo[i].displayStringPtr = &destInfo[i].filter;
		break;
	} // switch
    }
    
    // now render so we'll see new labels
    if (viewgadget)
	renderGraph();
} // setLabelType

void
NetTop::doFullName(nodeInfo* info) {
    // If there is a fullname, set displayStringPtr to that.
    // Also do it if displayStringPtr is null, which means it has
    // never been set and therefore the other possibilities might
    // be blank, too.
    if (info->fullname[0] || info->displayStringPtr == NULL)
	info->displayStringPtr = &info->fullname;
    else
	doInet(info);
} // doFullName

void
NetTop::doHostName(nodeInfo* info) {
    if (info->name[0] || info->displayStringPtr == NULL)
	info->displayStringPtr = &info->name;
    else if (info->fullname[0]) {
	int c = strcspn(info->fullname, ".");
	char* s = new char[c+1];
	strncpy(s, info->fullname, c);
	s[c] = '\0';
	info->name = s;
	info->displayStringPtr = &info->name;
    }
    // else ? xxx
} // doHostName

void
NetTop::doInet(nodeInfo* info) {
    if (info->ipaddr || info->displayStringPtr == NULL)
	info->displayStringPtr = &info->ipaddr;
    else 
	doEnet(info);
} // doInet

void
NetTop::doDecnet(nodeInfo* info) {
    if (info->dnaddr || info->displayStringPtr == NULL)
	info->displayStringPtr = &info->dnaddr;
    // else ? xxx
} // doDecnet

void
NetTop::doEnetVen(nodeInfo* info) {
    if (info->vendaddr || info->displayStringPtr == NULL)
	info->displayStringPtr = &info->vendaddr;
    else if (info->physaddr) {
	/*********
	I NEED etheraddr to do this; I just have string.. whoops
	char *v = ether_vendor((struct etheraddr *)
			interface->physaddr.getAddr());
	if (v == 0)
	    strcpy(s, n);
	else {
	    char t[12];
	    sscanf(n, "%*x:%*x:%*x:%s", t);
	    sprintf(s, "%s:%s", v, t);
	}
	********/
    }
    // else ? xxx
} // doEnetVen

void
NetTop::doEnet(nodeInfo* info) {
     if (info->physaddr || info->displayStringPtr == NULL)
	info->displayStringPtr = &info->physaddr;
} // doEnet


// xxx  I don't think there's any good reason why these are here
// and there are still many implemented in netTop.h ...
int
NetTop::restartTopSnooping()	{ return top->restartSnooping(); }

int
NetTop::stopTopSnooping()	{ return top->stopSnooping(); }

int
NetTop::getWhichAxes()		{ return whichAxes; }


int
NetTop::getWhichSrcs()		{ return whichSrcs; }

int
NetTop::getWhichDests()		{ return whichDests; }

int
NetTop::getWhichNodes()		{ return whichNodes; }

void
NetTop::setUseEaddr(tuBool e)	{ top->setUseEaddr(e); }

tuBool
NetTop::getUseEaddr()		{ return top->getUseEaddr(); }

float
NetTop::getCurrentMax()		{ return currentMax; }

float
NetTop::getLastMax()		{ return lastMax; }


Display*
NetTop::getDpy(void) {
    return display;
}

char*
NetTop::getFullSnooperString() {
    if (fullSnooperString[0])
	return strdup(fullSnooperString);
    else
	return localHostName;
}

void
NetTop::getRGBColors(long* mainBackgroundRGBColorp,
	    long* selectRGBColorp, 
	    long* labelRGBColorp, long* nameRGBColorp, 
	    long* nameErrorRGBColorp, long* nameLockedRGBColorp, 
	    long* baseRGBColorp, long* scaleRGBColorp, 
	    long* titleRGBColorp, long* edgeRGBColorp) {

    *mainBackgroundRGBColorp = strtol(ar->mainBackgroundRGBColor, NULL, 0);
    *selectRGBColorp = strtol(ar->selectRGBColor, NULL, 0);
    *labelRGBColorp = strtol(ar->labelRGBColor, NULL, 0);
    *nameRGBColorp = strtol(ar->nameRGBColor, NULL, 0);
    *nameErrorRGBColorp = strtol(ar->nameErrorRGBColor, NULL, 0);
    *nameLockedRGBColorp = strtol(ar->nameLockedRGBColor, NULL, 0);
    *baseRGBColorp = strtol(ar->baseRGBColor, NULL, 0);
    *scaleRGBColorp = strtol(ar->scaleRGBColor, NULL, 0);
    *titleRGBColorp = strtol(ar->titleRGBColor, NULL, 0);
    *edgeRGBColorp = strtol(ar->edgeRGBColor, NULL, 0);
    
} // getRGBColors


void
NetTop::getIndexColors(
	    short* mainBackgroundIndexColorp, 
	    short* selectIndexColorp, 
	    short* labelIndexColorp, 
	    short* nameIndexColorp, 
	    short* nameErrorIndexColorp, 
	    short* nameLockedIndexColorp, 
	    short* baseIndexColorp, 
	    short* scaleIndexColorp, 
	    short* titleIndexColorp, 
	    short* edgeIndexColorp, 
	    short* totalTotalIndexColorp, 
	    short* otherOtherIndexColorp, 
	    short* totalAnyIndexColorp, 
	    short* otherAnyIndexColorp, 
	    short* anyAnyIndexColorp) {

    *mainBackgroundIndexColorp = ar->mainBackgroundIndexColor;
    *selectIndexColorp = ar->selectIndexColor;
    *labelIndexColorp = ar->labelIndexColor;
    *nameIndexColorp = ar->nameIndexColor;
    *nameErrorIndexColorp = ar->nameErrorIndexColor;
    *nameLockedIndexColorp = ar->nameLockedIndexColor;
    *baseIndexColorp = ar->baseIndexColor;
    *scaleIndexColorp = ar->scaleIndexColor;
    *titleIndexColorp = ar->titleIndexColor;
    *edgeIndexColorp = ar->edgeIndexColor;
    *totalTotalIndexColorp = ar->totalTotalIndexColor;
    *otherOtherIndexColorp = ar->otherOtherIndexColor;
    *totalAnyIndexColorp = ar->totalAnyIndexColor;
    *otherAnyIndexColorp = ar->otherAnyIndexColor;
    *anyAnyIndexColorp = ar->anyAnyIndexColor;
    
} // getIndexColors

void
NetTop::setFatal(void) {
    if (histSnpStm.ss_sock != -1)
	exec->removeCallBack(histSnpStm.ss_sock);
    if (top) {
	top->removeSnpStmCB();
	stopTopSnooping();
    }
    // do those things before setting fatal true; in case one of
    // them has an error, we want the error for which this was
    // called to be the one that gets posted as fatal
    fatalError = True;
}

