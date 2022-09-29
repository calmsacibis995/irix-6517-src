/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Netlook
 *
 *	$Revision: 1.26 $
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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <osfcn.h>
#include <limits.h>
#include <errno.h>
/* #include <getopt.h> */

#include <helpapi/HelpBroker.h>

#include <tuXExec.h>
#include <tuVisual.h>
#include <tuGC.h>
#include <tuTopLevel.h>
#include <tuScreen.h>
#include <tuGadget.h>
#include <tuGLGadget.h>
#include <tuCallBack.h>
#include <tuTimer.h>

#include "netlook.h"
#include "nlwindow.h"
#include "network.h"
#include "node.h"
#include "comm.h"
#include "conntable.h"
#include "connection.h"
#include "viewgadget.h"
#include "netlookgadget.h"
#include "snoopgadget.h"
#include "netnodegadget.h"
#include "trafficgadget.h"
#include "hidegadget.h"
#include "glmapgadget.h"
#include "dialog.h"
#include "infogadget.h"
#include "selectiongadget.h"
#include "tooloptions.h"
#include "helpoptions.h"
#include "nis.h"
#include "event.h"
#include "license.h"

extern "C" {
#include "exception.h"
int sethostresorder(const char *);
}

tuDeclareCallBackClass(NetLookCallBack, NetLook);
tuImplementCallBackClass(NetLookCallBack, NetLook);

// Size of the connection table
static const unsigned int connTableSize = 256;

// Open gadgets
static const unsigned int openMapWindow = 1;
static const unsigned int openSnoopWindow = 2;
static const unsigned int openNetNodeWindow = 4;
static const unsigned int openTrafficWindow = 8;
static const unsigned int openHideWindow = 16;

// XXX can this go away?
NetLook *netlook;
extern NetLookGadget *netlookgadget;
extern SnoopGadget *snoopgadget;
extern GLMapGadget *glmapgadget;
extern ViewGadget *viewgadget;
extern NetNodeGadget *netnodegadget;
extern TrafficGadget *trafficgadget;
extern HideGadget *hidegadget;
extern SelectionGadget *selectiongadget;
extern ToolOptions *tooloptions;
extern HelpOptions *helpoptions;

// NIS disable flag
extern int _yp_disabled;

// Define command line arguments
static XrmOptionDescRec args[] = {
    {"-f",	"*networksFile",	XrmoptionSepArg, NULL},
    {"-u",	"*controlsFile", 	XrmoptionSepArg, NULL},
    {"-y",	"*useyp",	 	XrmoptionNoArg, "True"},
};

static const int nargs = sizeof(args) / sizeof(XrmOptionDescRec);

static tuResourceItem opts[] = {
    { "buffer", 0, tuResLong, "60", offsetof(AppResources, buffer), },
    { "interval", 0, tuResLong, "50", offsetof(AppResources, interval), },
    { "networksFile", 0, tuResString, "~/network.data",
			offsetof(AppResources, networksFile) },
    { "controlsFile", 0, tuResString, "~/.netlookrc",
			offsetof(AppResources, controlsFile) },
    { "hostresorder", 0, tuResString, "local",
			offsetof(AppResources, hostresorder), },
    { "useyp", 0, tuResBool, "False", offsetof(AppResources, useyp), },
    { "sendDetectEvents", 0, tuResBool, "False",
			offsetof(AppResources, sendDetectEvents), },
    { "pingCommand", 0, tuResString, "/usr/etc/ping -R",
			offsetof(AppResources, pingCommand), },
    { "traceRouteCommand", 0, tuResString, "/usr/etc/traceroute",
			offsetof(AppResources, traceRouteCommand), },
    { "mapBackgroundColor", 0, tuResLong, "0",
			offsetof(AppResources, mapBackgroundColor), },
    { "viewboxColor", 0, tuResLong, "3",
			offsetof(AppResources, viewboxColor), },
    { "selectColor", 0, tuResLong, "3",
			offsetof(AppResources, selectColor), },
    { "adjustColor", 0, tuResLong, "3",
			offsetof(AppResources, adjustColor), },
    { "activeNetworkColor", 0, tuResLong, "6",
			offsetof(AppResources, activeNetworkColor), },
    { "inactiveNetworkColor", 0, tuResLong, "4",
			offsetof(AppResources, inactiveNetworkColor), },
    { "standardNodeColor", 0, tuResLong, "2",
			offsetof(AppResources, standardNodeColor), },
    { "activeDataStationColor", 0, tuResLong, "5",
			offsetof(AppResources, activeDataStationColor), },
    { "NISMasterColor", 0, tuResLong, "9",
			offsetof(AppResources, NISMasterColor), },
    { "NISSlaveColor", 0, tuResLong, "7",
			offsetof(AppResources, NISSlaveColor), },
    { "gatewayColor", 0, tuResLong, "6",
			offsetof(AppResources, gatewayColor), },
    0
};


NetLook::NetLook(int argc, char *argv[])
{
    // Initialize variables
    exc_autofail = 0;
    exc_progname = "netlook";
    lastDataFile = 0;
    lastUIFile = 0;
    openGadgets = 0;
    archiverPid = 0;
    datastations = 0;
    dataStationList = 0;
    netlook = this;
    iconic = False;

    // Parse -i arguments so X won't eat it as -iconic
    for (unsigned int i = 1; i < argc; i++) {
	if (argv[i][0] != '-' || argv[i][1] != 'i' || argv[i][2] != '\0')
	    continue;

	// Zero out the -i
	argv[i][0] = '\0';

	// Move to datastation name
	if (++i == argc)
	    break;

	if (dataStationList == 0)
	    dataStationList = new char*[1];
	else {
	    unsigned int n = datastations + 1;
	    char **dsl = new char*[n];
	    bcopy(dataStationList, dsl, datastations * sizeof(char *));
	    delete dataStationList;
	    dataStationList = dsl;
	}
	unsigned int len = strlen(argv[i]) + 1;
	dataStationList[datastations] = new char[len + 1];
	dataStationList[datastations][0] = '+';
	bcopy(argv[i], &dataStationList[datastations][1], len);
	datastations++;
	argv[i][0] = '\0';
    }

    // Get resources
    rdb = new tuResourceDB;
    ar = new AppResources; 
    className = "NetLook";
    instanceName = 0;
    screen = rdb->getAppResources(ar, args, nargs, opts,
				  &instanceName, className,
				  &argc, argv);
    cmap = screen->getDefaultColorMap();
    display = rdb->getDpy();

    // Initialize SGIHelp
    SGIHelpInit (display, className, ".");

    // Create and map the view window
    viewWindow = new nlWindow(this, "netlook", cmap, rdb,
			      instanceName, className);
    viewWindow->bind();
    tuLayoutHints hints;
    viewWindow->getLayoutHints(&hints);
    viewWindow->resize(hints.prefWidth, hints.prefHeight);
    viewWindow->setInitialOrigin(338, 307);
    viewWindow->catchDeleteWindow(new NetLookCallBack(this, NetLook::quit));
    viewWindow->setIconicCallBack(new NetLookCallBack(this, NetLook::iconify));
    tooloptions->setTopLevel(viewWindow);

    // Create the snoop window (needed for -i processing)
    snoopWindow = new nlWindow(this, "snoop", viewWindow);
    snoopWindow->bind();

    // Create executive for events
    exec = new tuXExec(display);

    // Create communications module
    if (ar->buffer <= 0) {
	dialog = new DialogBox(className, cmap, rdb, instanceName, className);
	dialog->error("Buffer size must be greater than zero");
	dialog->setCallBack(new NetLookCallBack(this, NetLook::immediateQuit));
	openDialogBox();
	return;
    }
    if (ar->interval <= 0) {
	dialog = new DialogBox(className, cmap, rdb, instanceName, className);
	dialog->error("Interval must be greater than zero");
	dialog->setCallBack(new NetLookCallBack(this, NetLook::immediateQuit));
	openDialogBox();
	return;
    }
    comm = new Comm(ar->buffer, ar->interval);

    // Set hostresorder and _yp_disabled
    if (ar->useyp)
	_yp_disabled = 0;
    else
	_yp_disabled = 1;
    sethostresorder(ar->hostresorder);

    // Check for bad arguments
    opterr = 0;
    optind = 1;
    for (int c; (c = getopt(argc, argv, "i:")) != -1; ) {
	dialog = new DialogBox(className, cmap, rdb, instanceName, className);
	dialog->error("usage: netlook [-f networksfile] [-i datastation] [-u controlsfile]\n\t\t\t[-y] [filter]");
	dialog->setCallBack(new NetLookCallBack(this, NetLook::immediateQuit));
	openDialogBox();
	return;
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
	comm->setFilter(buf);
	snoopgadget->setFilterText(buf);
	tooloptions->setFilter(buf);
    }

    // Create Dialog Box
    dialog = new DialogBox(className, viewWindow, True);

    // Check license
    char *message;
    if (getLicense(className, &message) == 0) {
	dialog->error(message);
	dialog->setCallBack(new NetLookCallBack(this, NetLook::immediateQuit));
	openDialogBox();
	return;
    }

    // OK to run, if a message is returned from licensing display that,
    // otherwise, use a progress notifier.
    if (message != 0) {
	dialog->information(message);
	delete message;
    } else
	dialog->progress("Starting NetLook..");
    setDialogBoxCallBack();
    openDialogBox();

    // Create timer
    timer = new tuTimer;
    timer->setCallBack(new NetLookCallBack(this, NetLook::initialize));
    timer->start(1.0);
}

void
NetLook::run(void)
{
    exec->loop();
}

void
NetLook::initialize(tuGadget *)
{
    // Create windows
    mapWindow = new nlWindow(this, "map", viewWindow);
    netnodeWindow = new nlWindow(this, "netnode", viewWindow);
    trafficWindow = new nlWindow(this, "traffic", viewWindow);
    hideWindow = new nlWindow(this, "hide", viewWindow);
    selectionWindow = new nlWindow(this, "select", viewWindow);

    // Bind resources
    mapWindow->bind();
    netnodeWindow->bind();
    trafficWindow->bind();
    hideWindow->bind();
    selectionWindow->bind();

    // Get hints for layout and resize
    tuLayoutHints hints;
    mapWindow->getLayoutHints(&hints);
    mapWindow->resize(hints.prefWidth, hints.prefHeight);
    mapWindow->setAspectRatios(hints.prefWidth, hints.prefHeight);
    mapWindow->setInitialOrigin(131, 29);
    snoopWindow->getLayoutHints(&hints);
    snoopWindow->resize(hints.prefWidth, hints.prefHeight);
    snoopWindow->setInitialOrigin(336, 60);
    snoopWindow->setInitialSize(333, 180);
    netnodeWindow->getLayoutHints(&hints);
    netnodeWindow->resize(hints.prefWidth, hints.prefHeight);
    netnodeWindow->setInitialOrigin(798, 296);
    trafficWindow->getLayoutHints(&hints);
    trafficWindow->resize(hints.prefWidth, hints.prefHeight);
    trafficWindow->setInitialOrigin(8, 187);
    hideWindow->getLayoutHints(&hints);
    hideWindow->resize(hints.prefWidth, hints.prefHeight);
    hideWindow->setInitialOrigin(688, 30);
    selectionWindow->getLayoutHints(&hints);
    selectionWindow->resize(hints.prefWidth, hints.prefHeight);

    // Set up call backs
    mapWindow->catchDeleteWindow(
			new NetLookCallBack(this, NetLook::closeMapGadget));
    snoopWindow->catchDeleteWindow(
			new NetLookCallBack(this, NetLook::closeSnoopGadget));
    netnodeWindow->catchDeleteWindow(
		new NetLookCallBack(this, NetLook::closeNetNodeGadget));
    trafficWindow->catchDeleteWindow(
		new NetLookCallBack(this, NetLook::closeTrafficGadget));
    hideWindow->catchDeleteWindow(
		new NetLookCallBack(this, NetLook::closeHideGadget));
    selectionWindow->catchDeleteWindow(
		new NetLookCallBack(this, NetLook::closeSelectionGadget));

    // Reset timer
    timer->setAutoReload(True);
    timer->setCallBack(new NetLookCallBack(this, NetLook::tick));

    // Create the connection table
    conntable = new ConnTable(connTableSize);

    // Clear picked object
    pickedObjectType = pkNone;

    // Get host name
    if (gethostname(thisName, sizeof thisName) < 0) {
	perror("Could not get this host's name");
	exit(-1);
    }

    // Use host name to get host entry
    struct hostent *he = gethostbyname(thisName);
    if (he == 0) {
	perror("Could not get this host entry");
	exit(-1);
    }

    // Save the host name and address
    strncpy(thisName, he->h_name, sizeof thisName);
    struct in_addr a;
    bcopy(he->h_addr, &a, sizeof a);
    thisAddr = a;

    // Open a socket for ioctl
    int s = socket(PF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
	perror("Could not open socket");
	exit(-1);
    }

    // Get the netmask this node is using
    struct ifconf ifc;
    struct ifreq *ifr;
    char buf[1024];
#define ifr_sin(ifr)	((struct sockaddr_in *) &(ifr)->ifr_addr)

    ifc.ifc_len = sizeof buf;
    ifc.ifc_buf = buf;
    if (ioctl(s, SIOCGIFCONF, &ifc) < 0) {
	perror("Could not get interface configuration");
	close(s);
	exit(-1);
    }

    ifr = ifc.ifc_req;
    int n = ifc.ifc_len / sizeof *ifr;
    for ( ; --n >= 0; ifr++) {
	if (ifr->ifr_addr.sa_family == AF_INET
	    && ifr_sin(ifr)->sin_addr.s_addr == thisAddr.getAddr()->s_addr) {
		if (ioctl(s, SIOCGIFNETMASK, ifr) >= 0) {
			thisMask = ifr_sin(ifr)->sin_addr;
			thisNet.setAddr(thisAddr, thisMask);
			break;
		}
	}
    }
#undef ifr_sin

    // Close the socket
    close(s);

    // Map main window for GL bitplanes calculation
    viewWindow->map();

    // Open and close selection gadget
    selectionWindow->setInitialOrigin(viewWindow->getXOrigin(),
				      viewWindow->getYOrigin());
    selectionWindow->setInitialSize(1, 1);
    selectionWindow->setOverrideRedirect(True);
    openSelectionGadget();
    viewWindow->pop();
    closeSelectionGadget(selectionWindow);

    // Long operation
    viewWindow->beginLongOperation();
    time_t start = time(0);

    // Initialize NIS information
    YPInit();

    // Create event handler and send start event
    int fd;
    eventHandler = new EV_handler(&fd, className);
    EV_event startEvent(NV_STARTUP);
    eventHandler->send(&startEvent);

    // Read rc filtes
    int uirc = openUIFile();
    int rc = openDataFile();

#if 0
    // XXX Work around window system bugs
    start = time(0) - start;
    if (start < 2)
	sleep(1);
#endif

    // Set default viewport
    setViewPort();

    // Open up gadgets
    openUpGadgets();

    // Start DataStations
    if (dataStationList != 0) {
	unsigned int rc;
	unsigned int errors = 0;
	for (n = 0; n < datastations; n++) {
	    if (dataStationList[n][0] == '+') {
		if (errors == 0)
		    rc = snoopgadget->add(dataStationList[n] + 1, True);
		else
		    rc = snoopgadget->add(dataStationList[n] + 1, False);
	    } else if (dataStationList[n][0] == '-')
		rc = snoopgadget->add(dataStationList[n] + 1, False);
	    else
		rc = snoopgadget->add(dataStationList[n], False);
	    if (rc == 0)
		errors++;
	    delete dataStationList[n];
	}
	delete dataStationList;
	dataStationList = 0;
	setToolsInterface();
    }

    // Start timer
    setTimer();

    viewWindow->endLongOperation();
    if (dialog->getDialogType() == tuProgress || rc == 0 || uirc == 0)
	closeDialogBox(dialog);
    XFlush(getDpy());
    if (rc == 0)
	openDialogBox();
}

void
NetLook::quit(tuGadget *)
{
    dialog->question("Save data to %s and UI to %s before quitting",
		     lastDataFile, lastUIFile);
    dialog->setCallBack(new NetLookCallBack(this, NetLook::doQuit));
    openDialogBox();
}

void
NetLook::doQuit(tuGadget *)
{
    switch (dialog->getHitCode()) {
	case tuYes:
	    if (saveDataFile() == 0 || saveUIFile() == 0) {
		openDialogBox();
		return;
	    }
	    // Save was successful, fall through
	case tuNo:
	    helpoptions->close();
	    if (archiverPid != 0)
		kill(archiverPid, SIGTERM);
	    // Send stop event
	    {
		EV_event stopEvent(NV_SHUTDOWN);
		eventHandler->send(&stopEvent);
	    }
	    exit(0);
    }
}

void
NetLook::immediateQuit(tuGadget *)
{
    exit(1);
}

void
NetLook::setTimer(void)
{
    if (trafficRescale == 0 && trafficTimeout == 0) {
	timer->stop();
	return;
    }

    float d = (trafficRescale == 0) ? trafficTimeoutValue
				    : trafficRescaleValue;
    timer->start(d);
}

void
NetLook::tick(tuGadget *)
{
    conntable->rescale();
    netlookgadget->render();
    glmapgadget->render();
}

void
NetLook::read(tuGadget *)
{
    if (comm->read() == 0) {
	Node *n = comm->getBadNode();
	snoopgadget->deactivate(n);
	exec->removeCallBack(n->getSnoopSocket());

	viewWindow->beginLongOperation();
	(void) comm->close(n);

	// Send stop snooping event
	EV_objID ifObject(n->interface->getName(),
			  n->interface->ipaddr.getString(),
			  n->interface->physaddr.getString());
	EV_event stopSnoop(NV_STOP_SNOOP, &ifObject);
	eventHandler->send(&stopSnoop);
	viewWindow->endLongOperation();

	dialog->warning("Connection lost with Data Station %s.\n\nCheck the network connection of the Data Station.  See Appendix A of the NetVisualyzer User's Guide for detailed help", n->interface->getName());
	setDialogBoxCallBack();
	openDialogBox();

	n->info &= ~SNOOPHOST;
	netlookgadget->render(n->getNetwork(), n);
	setToolsInterface();
    }
}

Node *
NetLook::startSnoop(const char *c)
{
    Node *n = getNode(c);
    if (n == 0) {
	n = addNode(c);
	if (n == 0) {
	    dialog->warning("Data Station name not recognized.\n\nCheck that %s is a valid name and is entered in the appropriate hosts database.  See Appendix A of the NetVisualyzer User's Guide for detailed help", c);
	    setDialogBoxCallBack();
	    return 0;
	}
    }

    // Check if already snooping
    if ((n->info & SNOOPHOST) != 0) {
	dialog->warning("Already snooping on the Data Station %s", c);
        setDialogBoxCallBack();
        return 0;
    }

    // Open communication
    viewWindow->beginLongOperation();
    if (comm->open(n) == 0) {
	viewWindow->endLongOperation();
	dialog->warning("Could not connect to %s.\n\nCheck that snoopd is installed and configured correctly on the Data Station.  See Appendix A of the NetVisualyzer User's Guide for detailed help", exc_message);
	setDialogBoxCallBack();
	return 0;
    }

    // Send start snooping event
    EV_objID ifObject(n->interface->getName(),
		      n->interface->ipaddr.getString(),
		      n->interface->physaddr.getString());
    EV_event startSnoop(NV_START_SNOOP, &ifObject, comm->getFilter());
    eventHandler->send(&startSnoop);

    viewWindow->endLongOperation();
    exec->addCallBack(n->getSnoopSocket(), False,
		      new NetLookCallBack(this, NetLook::read));
    n->info |= SNOOPHOST;
    netlookgadget->render(n->getNetwork(), n);
    setToolsInterface();
    return n;
}

unsigned int
NetLook::stopSnoop(Node *n)
{
    unsigned int rc = 1;

    snoopgadget->deactivate(n);
    exec->removeCallBack(n->getSnoopSocket());

    viewWindow->beginLongOperation();
    if (comm->close(n) == 0) {
	viewWindow->endLongOperation();
	dialog->warning("Could not gracefully disconnect from %s", n->display);
	setDialogBoxCallBack();
	rc = 0;
    }

    // Send stop snooping event
    EV_objID ifObject(n->interface->getName(),
		      n->interface->ipaddr.getString(),
		      n->interface->physaddr.getString());
    EV_event stopSnoop(NV_STOP_SNOOP, &ifObject);
    eventHandler->send(&stopSnoop);
    viewWindow->endLongOperation();

    n->info &= ~SNOOPHOST;
    netlookgadget->render(n->getNetwork(), n);
    setToolsInterface();
    return rc;
}

unsigned int
NetLook::setFilter(const char *f)
{
    static const char *headmsg =
		"Could not change filter on the following Data Stations:";
    char *errmsg = 0;
    unsigned int error = 0;
    Node *n, *next;

    // Set filters in tools
    tooloptions->setFilter(f);
    comm->setFilter(f);

    viewWindow->beginLongOperation();
    for (n = comm->getSnoopList(); n != 0; n = next) {
	if (comm->changeFilter(n) != 0) {
	    {
		// Send stop snooping event
		EV_objID ifObject(n->interface->getName(),
				  n->interface->ipaddr.getString(),
				  n->interface->physaddr.getString());

		EV_event stopSnoop(NV_STOP_SNOOP, &ifObject);
		eventHandler->send(&stopSnoop);

		// Send start snooping event
		EV_event startSnoop(NV_START_SNOOP, &ifObject,
				    comm->getFilter());
		eventHandler->send(&startSnoop);
	    }

	    next = n->snoop;
	    continue;
	}

	// Error
	char *c;
	if (error == 0) {
	    unsigned int len = strlen(headmsg) + strlen(n->display)
			       + strlen(exc_message) + 5;
	    c = new char[len];
	    strcpy(c, headmsg);
	    error = 1;
	} else {
	    unsigned int len = strlen(errmsg) + strlen(n->display)
			       + strlen(exc_message) + 5;
	    c = new char[len];
	    strcpy(c, errmsg);
	    delete errmsg;
	}
	errmsg = c;
	c += strlen(errmsg);
	*c++ = '\n';
	strcpy(c, n->display);
	c += strlen(n->display);
	*c++ = ':';
	*c++ = ' ';
	*c++ = ' ';
	strcpy(c, exc_message);

	// Clean up
	next = n->snoop;
	stopSnoop(n);
    }
    viewWindow->endLongOperation();

    conntable->clear();
    netlookgadget->render();
    glmapgadget->render();

    if (error != 0) {
	dialog->warning(errmsg);
	setDialogBoxCallBack();
	delete errmsg;
	return 0;
    }
    return 1;
}

void
NetLook::setToolsInterface(void)
{
    Node *n = comm->getSnoopList();
    if (n == 0) {
	tooloptions->setInterface(0);
	return;
    }
    char *name = n->interface->getName();
    if (name != 0) {
	char buf[128];
	unsigned int len = strlen(name);
	strncpy(buf, name, len);
	buf[len++] = ':';
	strncpy(buf + len, name, len);
	tooloptions->setInterface(buf);
    }
}

Node *
NetLook::getNode(const char *c)
{
    NetworkNode *n;
    SegmentNode *s;
    InterfaceNode *i;

    // First try IP address
    IPAddr ia = c;
    if (ia.isValid()) {
	i = ifdb.match(ia);
	if (i != 0)
	    return (Node *) i->data;
    }

    // Now try physical address
    PhysAddr pa = c;
    if (pa.isValid()) {
	for (n = (NetworkNode *) config.child;
		n != 0; n = (NetworkNode *) n->next) {
	    i = ifdb.match(pa, n);
	    if (i != 0)
		return (Node *) i->data;
	}
    }

    // Try name
    for (n = (NetworkNode *) config.child;
		n != 0; n = (NetworkNode *) n->next) {
	for (s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    for (i = (InterfaceNode *) s->child;
			i != 0; i = (InterfaceNode *) i->next) {
		char *m = i->getName();
		if (m != 0) {
		    if (strcmp(m, c) == 0)
			return (Node *) i->data;
		    char *p = strchr(m, '.');
		    if (p != 0 && strncmp(m, c, p - m) == 0)
			return (Node *) i->data;
		}
	    }
	}
    }

    return 0;
}

Network *
NetLook::getNetwork(const char *c)
{
    NetworkNode *n;
    SegmentNode *s;

    // First try IP network number
    IPAddr ia = c;
    IPNetNum na = c;
    if (!na.isValid())
	na = ia;
    if (na.isValid()) {
	n = netdb.match(na);
	if (n == 0)
	    goto tryname;
	if (n->ipmask.isValid()) {
	    na.setAddr(ia, n->ipmask);
	    n = netdb.match(na);
	    if (n == 0)
		goto tryname;
	}
	SegmentNode *s = segdb.match(na);
	if (s != 0)
	    return (Network *) s->data;
    }

tryname:
    // Try name
    for (n = (NetworkNode *) config.child;
		n != 0; n = (NetworkNode *) n->next) {
	for (s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    char *m = s->getName();
	    if (m != 0 && strcmp(m, c) == 0)
		return (Network *) s->data;
	}
    }

    return 0;
}

unsigned int
NetLook::find(const char *c, unsigned int findHidden)
{
    // Try to find a node
    Node *node = getNode(c);
    if (node != 0) {
	if ((node->info & HIDDEN) != 0) {
	    if (findHidden == False) {
		dialog->warning("%s is hidden", c);
		setDialogBoxCallBack();
		return 0;
	    }
	    setPickedNode(node);
	    return 1;
	}
	Rectf v = getViewPort();
	v.set(node->getNetwork()->position.position[X]
	      + node->position.position[X] - v.getWidth() / 2.0,
	      node->getNetwork()->position.position[Y]
	      + node->position.position[Y] - v.getHeight() / 2.0,
	      v.getWidth(), v.getHeight());
	setViewPort(v);
	setPickedNode(node);
	netlookgadget->render();
	glmapgadget->render();
	return 1;
    }
    Network *network = getNetwork(c);
    if (network != 0) {
	if ((network->info & HIDDEN) != 0) {
	    if (findHidden == False) {
		dialog->warning("%s is hidden", c);
		setDialogBoxCallBack();
		return 0;
	    }
	    setPickedNetwork(network);
	    return 1;
	}
	Rectf v = getViewPort();
	v.set(network->position.position[X] - v.getWidth() / 2.0,
	      network->position.position[Y] - v.getHeight() / 2.0,
	      v.getWidth(), v.getHeight());
	setViewPort(v);
	setPickedNetwork(network);
	netlookgadget->render();
	glmapgadget->render();
	return 1;
    }
    dialog->warning("Could not find %s", c);
    setDialogBoxCallBack();
    return 0;
}

unsigned int
NetLook::ping(const char *c)
{
    char pingbuf[512];
    Node *n = getNode(c);
    sprintf(pingbuf, "/usr/sbin/winterm -geometry 80x24 -hold -title \"Ping %s\" -icontitle Ping -c %s %s",
	    c, ar->pingCommand,
	    (n != 0 && n->interface->ipaddr.isValid()) ?
		n->interface->ipaddr.getString() : c);
    if (system(pingbuf) != 0) {
	dialog->warning(errno, "Could not start %s", ar->pingCommand);
	setDialogBoxCallBack();
	return 0;
    }
    return 1;
}

unsigned int
NetLook::trace(const char *c)
{
    char tracebuf[512];
    Node *n = getNode(c);
    sprintf(tracebuf, "/usr/sbin/winterm -geometry 80x24 -hold -title \"Trace Route %s\" -icontitle Trace -c %s %s",
	    c, ar->traceRouteCommand,
	    (n != 0 && n->interface->ipaddr.isValid()) ?
		n->interface->ipaddr.getString() : c);
    if (system(tracebuf) != 0) {
	dialog->warning(errno, "Could not start %s", ar->traceRouteCommand);
	setDialogBoxCallBack();
	return 0;
    }
    return 1;
}

void *
NetLook::hide(const char *c, unsigned int noposition)
{
    Node *node = getNode(c);
    if (node != 0)
	return hide(node, noposition);
    Network *network = getNetwork(c);
    if (network == 0) {
	dialog->warning("Could not find %s", c);
	setDialogBoxCallBack();
	return 0;
    }
    return hide(network, noposition);
}

Network *
NetLook::hide(Network *network, unsigned int noposition)
{
    if ((network->info & HIDDEN) != 0) {
	dialog->warning("%s is already hidden", network->display);
	setDialogBoxCallBack();
	return 0;
    }
    if ((network->info & PICKED) != 0)
	unpickObject();
    network->info |= HIDDEN;
    if (noposition == False) {
	position();
	conntable->clear();
	netlookgadget->render();
	glmapgadget->render();
    }
    return network;
}

Node *
NetLook::hide(Node *node, unsigned int noposition)
{
    if ((node->info & HIDDEN) != 0) {
	dialog->warning("%s is already hidden", node->display);
	setDialogBoxCallBack();
	return 0;
    }
    if ((node->info & PICKED) != 0)
	unpickObject();
    node->info |= HIDDEN;
    if (noposition == False) {
	node->getNetwork()->place();
	conntable->clear();
	netlookgadget->render();
	glmapgadget->render();
    }
    return node;
}

unsigned int
NetLook::unhide(const char *c)
{
    Node *node = getNode(c);
    if (node != 0) {
	if ((node->info & HIDDEN) == 0) {
	    dialog->warning("%s is not hidden", c);
	    setDialogBoxCallBack();
	    return 0;
	}
	node->info &= ~HIDDEN;
	node->getNetwork()->place();
    } else {
	Network *network = getNetwork(c);
	if (network == 0) {
	    dialog->warning("Could not find %s", c);
	    setDialogBoxCallBack();
	    return 0;
	}
	if ((network->info & HIDDEN) == 0) {
	    dialog->warning("%s is not hidden", c);
	    setDialogBoxCallBack();
	    return 0;
	}
	network->info &= ~HIDDEN;
    }
    position();
    netlookgadget->render();
    glmapgadget->render();
    return 1;
}


unsigned int
NetLook::Delete(const char *c)
{
    if (find(c, True) == 0)
	return 0;

    dialog->question("Are you sure that you want to delete %s", c);
    dialog->setCallBack(new NetLookCallBack(this, NetLook::doDelete));
    openDialogBox();
    return 1;
}

void
NetLook::doDelete(tuGadget *)
{
    if (dialog->getHitCode() != tuYes)
	return;

    switch (pickedObjectType) {
	case pkNone:
	case pkConnection:
	    break;
	case pkNode:
	    {
		Node *n = (Node *) pickedObject;
		InterfaceNode *i = n->interface;
		ifdb.remove(i);
		if (i->nextif != 0 || i->previf != 0) {
		    HostNode *h = i->getHost();
		    delete h;
		}
		SegmentNode *s = (SegmentNode *) i->parent;
		if (i == (InterfaceNode *) s->child)
		    s->child = i->next;
		delete i;
		unpickObject();
		delete n;
		netlookgadget->render();
		glmapgadget->render();
	    }
	    break;
	case pkNetwork:
	    {
		Network *n = (Network *) pickedObject;
		SegmentNode *s = n->segment;
		unpickObject();
		delete n;
		InterfaceNode *i, *in;
		for (i = (InterfaceNode *) s->child; i != 0; i = in) {
		    ifdb.remove(i);
		    if (i->nextif != 0 || i->previf != 0) {
			HostNode *h = i->getHost();
			delete h;
		    }
		    in = (InterfaceNode *) i->next;
		    delete (Node *) i->data;
		    delete i;
		}
		s->child = 0;
		segdb.remove(s);
		NetworkNode *nn = (NetworkNode *) s->parent;
		if (s == (SegmentNode *) nn->child)
		    nn->child = s->next;
		delete s;
		netlookgadget->render();
		glmapgadget->render();
	    }
	    break;
    }
}

void
NetLook::openUpGadgets(void)
{
    if ((openGadgets & openMapWindow) != 0)
	openMapGadget();
    if ((openGadgets & openSnoopWindow) != 0)
	openSnoopGadget();
    if ((openGadgets & openNetNodeWindow) != 0)
	openNetNodeGadget();
    if ((openGadgets & openTrafficWindow) != 0)
	openTrafficGadget();
    if ((openGadgets & openHideWindow) != 0)
	openHideGadget();
}

void
NetLook::openMapGadget(void)
{
    if (!mapWindow->isMapped())
	mapWindow->map();
    else {
	if (mapWindow->getIconic() == True)
	    mapWindow->setIconic(False);
	else
	    mapWindow->pop();
    }
    XFlush(getDpy());
    openGadgets |= openMapWindow;
}

void
NetLook::closeMapGadget(tuGadget *)
{
    mapWindow->unmap();
    mapWindow->setInitialOrigin(mapWindow->getXOrigin(),
				mapWindow->getYOrigin());
    mapWindow->setInitialSize(mapWindow->getWidth(),
			      mapWindow->getHeight());
    openGadgets &= ~openMapWindow;
}

void
NetLook::openSnoopGadget(void)
{
    if (!snoopWindow->isMapped())
	snoopWindow->map();
    else {
	if (snoopWindow->getIconic() == True)
	    snoopWindow->setIconic(False);
	else
	    snoopWindow->pop();
    }
    XFlush(getDpy());
    openGadgets |= openSnoopWindow;
}

void
NetLook::closeSnoopGadget(tuGadget *)
{
    snoopWindow->unmap();
    snoopWindow->setInitialOrigin(snoopWindow->getXOrigin(),
				  snoopWindow->getYOrigin());
    snoopWindow->setInitialSize(snoopWindow->getWidth(),
				snoopWindow->getHeight());
    openGadgets &= ~openSnoopWindow;
}

void
NetLook::openNetNodeGadget(void)
{
    if (!netnodeWindow->isMapped())
	netnodeWindow->map();
    else {
	if (netnodeWindow->getIconic() == True)
	    netnodeWindow->setIconic(False);
	else
	    netnodeWindow->pop();
    }
    XFlush(getDpy());
    openGadgets |= openNetNodeWindow;
}

void
NetLook::closeNetNodeGadget(tuGadget *)
{
    netnodeWindow->unmap();
    netnodeWindow->setInitialOrigin(netnodeWindow->getXOrigin(),
				    netnodeWindow->getYOrigin());
    netnodeWindow->setInitialSize(netnodeWindow->getWidth(),
				  netnodeWindow->getHeight());
    openGadgets &= ~openNetNodeWindow;
}

void
NetLook::openTrafficGadget(void)
{
    if (!trafficWindow->isMapped())
	trafficWindow->map();
    else {
	if (trafficWindow->getIconic() == True)
	    trafficWindow->setIconic(False);
	else
	    trafficWindow->pop();
    }
    XFlush(getDpy());
    openGadgets |= openTrafficWindow;
}

void
NetLook::closeTrafficGadget(tuGadget *)
{
    trafficWindow->unmap();
    trafficWindow->setInitialOrigin(trafficWindow->getXOrigin(),
				    trafficWindow->getYOrigin());
    trafficWindow->setInitialSize(trafficWindow->getWidth(),
				  trafficWindow->getHeight());
    openGadgets &= ~openTrafficWindow;
}

void
NetLook::openHideGadget(void)
{
    if (!hideWindow->isMapped())
	hideWindow->map();
    else {
	if (hideWindow->getIconic() == True)
	    hideWindow->setIconic(False);
	else
	    hideWindow->pop();
    }
    XFlush(getDpy());
    openGadgets |= openHideWindow;
}

void
NetLook::closeHideGadget(tuGadget *)
{
    hideWindow->unmap();
    hideWindow->setInitialOrigin(hideWindow->getXOrigin(),
				 hideWindow->getYOrigin());
    hideWindow->setInitialSize(hideWindow->getWidth(),
			       hideWindow->getHeight());
    openGadgets &= ~openHideWindow;
}

void
NetLook::openSelectionGadget(void)
{
    if (selectionWindow->isMapped())
	selectionWindow->pop();
    else
	selectionWindow->map();
}

void
NetLook::closeSelectionGadget(tuGadget *)
{
    selectionWindow->unmap();
}

void
NetLook::openDialogBox(void)
{
    if (dialog->isMapped()) {
	dialog->unmap();
	XFlush(getDpy());
    }
    dialog->map();
    XFlush(getDpy());
}

void
NetLook::closeDialogBox(tuGadget *)
{
    dialog->unmap();
}

void
NetLook::setDialogBoxCallBack(void)
{
    dialog->setCallBack(new NetLookCallBack(this, NetLook::closeDialogBox));
}

void
NetLook::openInfoBox(void)
{
    InfoGadget *infoBox = new InfoGadget("info", viewWindow, True);
    infoBox->bind();
    infoBox->setCallBack(new NetLookCallBack(this, NetLook::closeInfoBox));
    infoBox->catchDeleteWindow(
		new NetLookCallBack(this, NetLook::closeInfoBox));
    infoBox->setName("NetLook Information");
    infoBox->setIconName("NetLook Info");
    switch (pickedObjectType) {
	case pkNone:
	    {
		unsigned int networks = 0;
		unsigned int nodes = 0;
		for (NetworkNode *n = (NetworkNode *) config.child;
				n != 0; n = (NetworkNode *) n->next) {
		    for (SegmentNode *s = (SegmentNode *) n->child;
				s != 0; s = (SegmentNode *) s->next) {
			networks++;
			for (InterfaceNode *i = (InterfaceNode *) s->child;
				i != 0; i = (InterfaceNode *) i->next)
			    nodes++;
		    }
		}
		char buf[8];
		sprintf(buf, "%d", networks);
		infoBox->addText("Number of networks:", buf);
		sprintf(buf, "%d", nodes);
		infoBox->addText("Number of nodes:", buf);
	    }
	    break;
	case pkNetwork:
	    {
		Network *n = getPickedNetwork();
		unsigned int nodes = 0;
		for (InterfaceNode *i = (InterfaceNode *) n->segment->child;
			i != 0; i = (InterfaceNode *) i->next)
		    nodes++;
		if (n->segment->getName() != 0)
		    infoBox->addText("Name:", n->segment->getName());
		if (n->segment->ipnum.isValid())
		    infoBox->addText("IP Network Number:",
				     n->segment->ipnum.getString());
		char buf[8];
		sprintf(buf, "%d", nodes);
		infoBox->addText("Number of Nodes:", buf);
	    }
	    break;
	case pkNode:
	    {
		Node *n = getPickedNode();
		if (n->interface->getName() != 0)
		    infoBox->addText("Name:", n->interface->getName());
		if (n->interface->ipaddr.isValid())
		    infoBox->addText("IP Address:",
				     n->interface->ipaddr.getString());
		if (n->interface->dnaddr.isValid())
		    infoBox->addText("DECnet Address:",
				     n->interface->dnaddr.getString());
		if (n->interface->physaddr.isValid())
		    infoBox->addText("Physical Address:",
				     n->interface->physaddr.getString());
	    }
	    break;
	case pkConnection:
	    {
		Connection *c = getPickedConnection();
		Node *n = c->src.node;
		infoBox->addText("Traffic between:", "");
		if (n->interface->getName() != 0)
		    infoBox->addText("Name:", n->interface->getName());
		if (n->interface->ipaddr.isValid())
		    infoBox->addText("IP Address:",
				     n->interface->ipaddr.getString());
		if (n->interface->dnaddr.isValid())
		    infoBox->addText("DECnet Address:",
				     n->interface->dnaddr.getString());
		if (n->interface->physaddr.isValid())
		    infoBox->addText("Physical Address:",
				     n->interface->physaddr.getString());
		infoBox->addText("and", "");
		n = c->dst.node;
		if (n->interface->getName() != 0)
		    infoBox->addText("Name:", n->interface->getName());
		if (n->interface->ipaddr.isValid())
		    infoBox->addText("IP Address:",
				     n->interface->ipaddr.getString());
		if (n->interface->dnaddr.isValid())
		    infoBox->addText("DECnet Address:",
				     n->interface->dnaddr.getString());
		if (n->interface->physaddr.isValid())
		    infoBox->addText("Physical Address:",
				     n->interface->physaddr.getString());
	    }
	    break;
    }
    infoBox->resizeToFit();
    infoBox->mapWithCloseUnderMouse();
}

void
NetLook::closeInfoBox(tuGadget *g)
{
    InfoGadget *infoBox = (InfoGadget *) g;
    infoBox->unmap();
    infoBox->markDelete();
}

unsigned int
NetLook::openArchiver(void)
{
    if (archiverPid != 0)
	return 1;

    int fd[2];
    if (pipe(fd) != 0) {
	dialog->warning(errno, "Could not start NetFilters");
	return 0;
    }

    archiverPid = fork();
    if (archiverPid < 0) {
	dialog->warning(errno, "Could not start NetFilters");
	return 0;
    }
    if (archiverPid == 0) {
	if (dup2(fd[1], 1) < 0)
	    exit(-1);
	execlp("netfilters", "netfilters", "-stdout", 0);
	perror("netlook: Could not start NetFilters");
	exit(-1);
    }
    close(fd[1]);
    archiver = fdopen(fd[0], "r");
    exec->addCallBack(fd[0], False,
		      new NetLookCallBack(this, NetLook::readArchiver));
    return 1;
}

void
NetLook::readArchiver(tuGadget *)
{
    char buf[256];
    if (fgets(buf, 256, archiver) == 0) {
	closeArchiver();
	return;
    }
    snoopgadget->setFilterText(buf);
    if (setFilter(buf) == 0)
	openDialogBox();
}

void
NetLook::closeArchiver(void)
{
    exec->removeCallBack(fileno(archiver));
    fclose(archiver);
    kill(archiverPid, SIGTERM);
    archiverPid = 0;
}

void
NetLook::iconify(tuGadget *)
{
    if (viewWindow->getIconic() == False) {
	iconic = False;
	openUpGadgets();
	return;
    }
 
    iconic = True;
    mapWindow->unmap();
    snoopWindow->unmap();
    netnodeWindow->unmap();
    trafficWindow->unmap();
    hideWindow->unmap();
}

// Protected
void
NetLook::blank(void)
{
    // Clear connection table first
    conntable->clear();

    // Clear Networks and Nodes
    for (NetworkNode *n = (NetworkNode *) config.child;
		n != 0; n = (NetworkNode *) n->next) {
	for (SegmentNode *s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    for (InterfaceNode *i = (InterfaceNode *) s->child;
			i != 0; i = (InterfaceNode *) i->next) {
		Node *node = (Node *) i->data;
		if (node != 0)
		    delete node;
	    }
	    Network *net = (Network *) s->data;
	    if (net != 0)
		delete net;
	}
    }

    // Clear configuration
    NetworkNode *next;
    for (n = (NetworkNode *) config.child; n != 0; n = next) {
	next = (NetworkNode *) n->next;
	delete n;
    }
    config.child = 0;
    ifdb.create(&config);
    segdb.create(&config);
    netdb.create(&config);

    // Reset universe to default size
    netlookgadget->setUniverse();
}

void
NetLook::clear(void)
{
    blank();

    // Add this node
    NetworkNode *n;
    IPNetNum nn, mn;
    nn = thisAddr;
    mn.setAddr(thisAddr, thisMask);
    if (!(nn == mn)) {
	n = netdb.match(nn);
	if (n == 0) {
	    // Create a new network
	    n = new NetworkNode(&config);
	    n->ipnum = nn;
	    n->complete();
	    n->ipmask = thisMask;
	    netdb.enter(n);
    	}
    }
    thisNode = addNode(*thisAddr.getAddr(), thisName);

    // Set viewport to universe
    setViewPort();

    // Render
    netlookgadget->render();
    glmapgadget->render();
}

unsigned int
NetLook::openDataFile(const char *filename)
{
    char line[LINESIZE], program[64];
    float version, v;
    FILE *fp;
    NetworkNode *n;
    SegmentNode *s;
    InterfaceNode *i;
    IPNetNum nn, mn;
    int lineno, rc = 1;

    if (lastDataFile != 0)
	delete lastDataFile;

    if (filename == 0 || filename[0] == '\0') {
	lastDataFile = expandTilde(ar->networksFile);
	if (lastDataFile == 0) {
	    lastDataFile = new char[strlen(ar->networksFile) + 1];
	    strcpy(lastDataFile, ar->networksFile);
	}
    } else {
	lastDataFile = new char[strlen(filename) + 1];
	strcpy(lastDataFile, filename);
    }

    fp = fopen(lastDataFile, "r");
    if (fp == NULL) {
	dialog->warning(errno, "Could not open %s", lastDataFile);
	setDialogBoxCallBack();
	rc = 0;
	goto fileread;
    }

    /* Read check the program and version number */
    for ( ; ; ) {
	if (fgets(line, LINESIZE, fp) == NULL) {
	    dialog->warning(errno, "Could not read %s", lastDataFile);
	    setDialogBoxCallBack();
	    rc = 0;
	    goto fileread;
	}
	if (sscanf(line, "%63s %f\n", program, &version) > 0)
	    break;
    }
    v = VERSION - version;
    if (strcmp(program, PROGRAM) != 0 || v > 0.005) {
	dialog->warning("%s is not a NetLook data file", lastDataFile);
	setDialogBoxCallBack();
	rc = 0;
    } else {
	// Blank the data
	blank();

	// Parse the configuration file
	lineno = 2;
	if (!cf_parse(fp, &config, &lineno, line)) {
	    dialog->warning("%s: %s", lastDataFile, line);
	    setDialogBoxCallBack();
	    rc = 0;
	}
    }

    fclose(fp);

fileread:
    // Create the databases
    ifdb.create(&config);
    segdb.create(&config);
    netdb.create(&config);

    // Create the Node and Network instances
    for (n = (NetworkNode *) config.child;
		n != 0; n = (NetworkNode *) n->next) {
	for (s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    s->data = new Network(s);
	    for (i = (InterfaceNode *) s->child;
			i != 0; i = (InterfaceNode *) i->next) {
		Node *node = new Node(i);
		i->data = node;
		if (i->nextif != 0 || i->previf != 0)
		    node->info |= GATEWAY;
		HostNode *h = i->getHost();
		if (h->isNISserver())
		    node->info |= YPSERVER;
		if (h->isNISmaster())
		    node->info |= YPMASTER;
	    }
	}
    }

    // Add this node
    nn = thisAddr;
    mn.setAddr(thisAddr, thisMask);
    if (!(nn == mn)) {
	n = netdb.match(nn);
	if (n == 0) {
	    // Create a new network
	    n = new NetworkNode(&config);
	    n->ipnum = nn;
	    n->complete();
	    n->ipmask = thisMask;
	    netdb.enter(n);
    	}
    }
    thisNode = addNode(*thisAddr.getAddr(), thisName);

    // Hide objects
    hidegadget->hide();

    // Position the networks and nodes on the screen
    position();
    for (n = (NetworkNode *) config.child;
		n != 0; n = (NetworkNode *) n->next) {
	for (s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next)
	    ((Network *) s->data)->place();
    }

    return rc;
}

unsigned int
NetLook::saveDataFile(const char *filename)
{
    if (filename != 0) {
	delete lastDataFile;
	lastDataFile = new char[strlen(filename) + 1];
	strcpy(lastDataFile, filename);
    }

    FILE *fp = fopen(lastDataFile, "w");
    if (fp == 0) {
	dialog->warning(errno, "Could not write %s", lastDataFile);
	setDialogBoxCallBack();
	return 0;
    }

    fprintf(fp, "%s %1.2f\n", PROGRAM, VERSION);
    cf_print(fp, &config);
    if (fclose(fp) != 0) {
	dialog->warning(errno, "Could not write %s", lastDataFile);
	setDialogBoxCallBack();
	return 0;
    }
    return 1;
}

unsigned int
NetLook::openUIFile(const char *filename)
{
    char line[LINESIZE];
    char button[32];
    char state[32];
    unsigned int activeDS;
    int err = 0;

    if (datastations == 0)
	activeDS = True;
    else
	activeDS = False;

    if (lastUIFile != 0)
	delete lastUIFile;

    // If the file name is 0, use resource
    if (filename == 0 || filename[0] == '\0') {
	lastUIFile = expandTilde(ar->controlsFile);
	if (lastUIFile == 0) {
	    lastUIFile = new char[strlen(ar->controlsFile) + 1];
	    strcpy(lastUIFile, ar->controlsFile);
	}
    } else {
	lastUIFile = new char[strlen(filename) + 1];
	strcpy(lastUIFile, filename);
    }

    // Set default values
    netShow = ACTIVE;
    netIgnore = ADD;
    netDisplay = DMFULLNAME;
    nodeShow = ACTIVE;
    nodeIgnore = ADD;
    nodeDisplay = DMNODENAME;
    trafficBasis = PACKETS;
    trafficMode = ENDPOINT;
    trafficRescale = True;
    trafficRescaleValue = 5;
    trafficTimeout = True;
    trafficTimeoutValue = 60;
    trafficPacketColorStep = 1;
    trafficByteColorStep = 1024;
    if (netlookgadget->getAvailableBitplanes() <= 4) {
	trafficMinColorValue = 8;
	trafficMaxColorValue = 15;
    } else {
	trafficMinColorValue = 144;
	trafficMaxColorValue = 151;
    }

    FILE *fp = fopen(lastUIFile, "r");
    if (fp == NULL) {
	err = errno;
	dialog->warning(errno, "Could not open %s", lastUIFile);
	setDialogBoxCallBack();
    } else {
	while (fgets(line, LINESIZE-1, fp) != NULL) {
	    if (sscanf(line, "%s %[^\n]", button, state) != 2
		|| button[0] == '#')
		continue;
	    if (strcasecmp(button, "NetworkDisplay") == 0) {
		if (strcasecmp(state, "Name") == 0)
		    netDisplay = DMFULLNAME;
		else if (strcasecmp(state, "Number") == 0)
		    netDisplay = DMINETADDR;
	    } else if (strcasecmp(button, "NetworkShown") == 0) {
		if (strcasecmp(state, "All") == 0)
		    netShow = ALL;
		else if (strcasecmp(state, "Active") == 0)
		    netShow = ACTIVE;
	    } else if (strcasecmp(button, "NetworkNew") == 0) {
		if (strcasecmp(state, "Add") == 0)
		    netIgnore = ADD;
		else if (strcasecmp(state, "Ignore") == 0)
		    netIgnore = IGNORE;
	    } else if (strcasecmp(button, "NodeDisplay") == 0
		       || strcasecmp(button, "HostDisplay") == 0) {
		if (strcasecmp(state, "Full") == 0)
		    nodeDisplay = DMFULLNAME;
		else if (strcasecmp(state, "Local") == 0)
		    nodeDisplay = DMNODENAME;
		else if (strcasecmp(state, "Internet") == 0)
		    nodeDisplay = DMINETADDR;
		else if (strcasecmp(state, "Vendor") == 0)
		    nodeDisplay = DMVENDOR;
		else if (strcasecmp(state, "Physical") == 0
			 || strcasecmp(state, "Ethernet") == 0)
		    nodeDisplay = DMPHYSADDR;
	    } else if (strcasecmp(button, "NodeShown") == 0
		       || strcasecmp(button, "HostShown") == 0) {
		if (strcasecmp(state, "All") == 0)
		    nodeShow = ALL;
		else if (strcasecmp(state, "Active") == 0)
		    nodeShow = ACTIVE;
	    } else if (strcasecmp(button, "NodeNew") == 0
		       || strcasecmp(button, "HostNew") == 0) {
		if (strcasecmp(state, "Add") == 0)
		    nodeIgnore = ADD;
		else if (strcasecmp(state, "Ignore") == 0)
		    nodeIgnore = IGNORE;
	    } else if (strcasecmp(button, "Rescale") == 0) {
		int i = atoi(state);
		if (i < 0) {
		    trafficRescale = False;
		    i = -i;
		}
		trafficRescaleValue = i;
	    } else if (strcasecmp(button, "Timeout") == 0) {
		int i = atoi(state);
		if (i < 0) {
		    trafficTimeout = False;
		    i = -i;
		}
		trafficTimeoutValue = i;
	    } else if (strcasecmp(button, "TrafficBasis") == 0
		       || strcasecmp(button, "ScaleMode") == 0) {
		if (strcasecmp(state, "Packets") == 0)
		    trafficBasis = PACKETS;
		else if (strcasecmp(state, "Bytes") == 0)
		    trafficBasis = BYTES;
	    } else if (strcasecmp(button, "TrafficMode") == 0
		       || strcasecmp(button, "Gateway") == 0) {
		if (strcasecmp(state, "Hop") == 0
		    || strcasecmp(state, "Gateways") == 0)
		    trafficMode = HOP;
		else if (strcasecmp(state, "Endpoint") == 0
			 || strcasecmp(state, "Endpoints") == 0)
		    trafficMode = ENDPOINT;
	    } else if (strcasecmp(button, "Filter") == 0) {
		if (*(comm->getFilter()) != '\0')
		    continue;
		comm->setFilter(state);
		tooloptions->setFilter(state);
		snoopgadget->setFilterText(state);
	    } else if (strcasecmp(button, "DataStation") == 0) {
		if (dataStationList == 0)
		    dataStationList = new char*[1];
		else {
		    unsigned int c = datastations + 1;
		    char **dsl = new char*[c];
		    bcopy(dataStationList, dsl, datastations * sizeof(char *));
		    delete dataStationList;
		    dataStationList = dsl;
		}
		if (activeDS == False && state[0] == '+')
		    state[0] = '-';
		unsigned int len = strlen(state) + 1;
		dataStationList[datastations] = new char[len];
		bcopy(state, dataStationList[datastations], len);
		datastations++;
	    } else if (strcasecmp(button, "Hide") == 0)
		hidegadget->add(state);
	    else if (strcasecmp(button, "PacketScale") == 0
		     || strcasecmp(button, "PacketColorStep") == 0)
		trafficPacketColorStep = atoi(state);
	    else if (strcasecmp(button, "ByteScale") == 0
		     || strcasecmp(button, "ByteColorStep") == 0)
		trafficByteColorStep = atoi(state);
	    else if (strcasecmp(button, "MinColorValue") == 0)
		trafficMinColorValue = atoi(state);
	    else if (strcasecmp(button, "MaxColorValue") == 0)
		trafficMaxColorValue = atoi(state);
	    else if (strcasecmp(button, "NetLookGeometry") == 0)
		viewWindow->parseGeometry(state, "");
	    else if (strcasecmp(button, "NetLookMapGeometry") == 0) {
		mapWindow->parseGeometry(state, "");
		openGadgets |= openMapWindow;
	    } else if (strcasecmp(button, "NetLookSnoopGeometry") == 0) {
		snoopWindow->parseGeometry(state, "");
		openGadgets |= openSnoopWindow;
	    } else if (strcasecmp(button, "NetLookNetNodeGeometry") == 0) {
		netnodeWindow->parseGeometry(state, "");
		openGadgets |= openNetNodeWindow;
	    } else if (strcasecmp(button, "NetLookTrafficGeometry") == 0) {
		trafficWindow->parseGeometry(state, "");
		openGadgets |= openTrafficWindow;
	    } else if (strcasecmp(button, "NetLookHideGeometry") == 0) {
		hideWindow->parseGeometry(state, "");
		openGadgets |= openHideWindow;
	    }
	}

	fclose(fp);
    }

    // Update gadgets to reflect new values
    netnodegadget->update();
    if (trafficMinColorValue < trafficMaxColorValue)
	trafficColorValues = trafficMaxColorValue - trafficMinColorValue + 1;
    else
	trafficColorValues = trafficMinColorValue - trafficMaxColorValue + 1;
    trafficgadget->update();

    // If no datastations, add this hostname
    if (datastations == 0) {
	char buf[64];
	if (gethostname(buf, 64) == 0)
	    snoopgadget->add(buf, False);
    }

    if (err != 0) {
	errno = err;
	return 0;
    }
    return 1;
}

unsigned int
NetLook::saveUIFile(const char *filename)
{
    if (filename != 0) {
	delete lastUIFile;
	lastUIFile = new char[strlen(filename) + 1];
	strcpy(lastUIFile, filename);
    }

    FILE *fp = fopen(lastUIFile, "w");
    if (fp == 0) {
	dialog->warning(errno, "Could not write %s", lastDataFile);
	setDialogBoxCallBack();
	return 0;
    }

    fputs("NetworkDisplay\t", fp);
    switch (netDisplay) {
	case DMFULLNAME:
	    fputs("Name\n", fp);
	    break;
	case DMINETADDR:
	    fputs("Number\n", fp);
	    break;
    }
    fputs("NetworkShown\t", fp);
    switch (netShow) {
	case ALL:
	    fputs("All\n", fp);
	    break;
	case ACTIVE:
	    fputs("Active\n", fp);
	    break;
    }
    fputs("NetworkNew\t", fp);
    switch (netIgnore) {
	case ADD:
	    fputs("Add\n", fp);
	    break;
	case IGNORE:
	    fputs("Ignore\n", fp);
	    break;
    }
    fputs("NodeDisplay\t", fp);
    switch (nodeDisplay) {
	case DMFULLNAME:
	    fputs("Full\n", fp);
	    break;
	case DMNODENAME:
	    fputs("Local\n", fp);
	    break;
	case DMINETADDR:
	    fputs("Internet\n", fp);
	    break;
	case DMVENDOR:
	    fputs("Vendor\n", fp);
	    break;
	case DMPHYSADDR:
	    fputs("Physical\n", fp);
	    break;
    }
    fputs("NodeShown\t", fp);
    switch (nodeShow) {
	case ALL:
	    fputs("All\n", fp);
	    break;
	case ACTIVE:
	    fputs("Active\n", fp);
	    break;
    }
    fputs("NodeNew\t\t", fp);
    switch (nodeIgnore) {
	case ADD:
	    fputs("Add\n", fp);
	    break;
	case IGNORE:
	    fputs("Ignore\n", fp);
	    break;
    }
    fprintf(fp, "Rescale\t\t%d\n",
	    trafficRescale ?  trafficRescaleValue : -trafficRescaleValue);
    fprintf(fp, "Timeout\t\t%d\n",
	    trafficTimeout ?  trafficTimeoutValue : -trafficTimeoutValue);
    fputs("TrafficMode\t", fp);
    switch (trafficMode) {
	case HOP:
	    fputs("Hop\n", fp);
	    break;
	case ENDPOINT:
	    fputs("Endpoint\n", fp);
	    break;
    }
    fputs("TrafficBasis\t", fp);
    switch (trafficBasis) {
	case PACKETS:
	    fputs("Packets\n", fp);
	    break;
	case BYTES:
	    fputs("Bytes\n", fp);
	    break;
    }
    fprintf(fp, "PacketColorStep\t%d\n", trafficPacketColorStep);
    fprintf(fp, "ByteColorStep\t%d\n", trafficByteColorStep);
    fprintf(fp, "MinColorValue\t%d\n", trafficMinColorValue);
    fprintf(fp, "MaxColorValue\t%d\n", trafficMaxColorValue);

    // Save filter expression
    char *f = comm->getFilter();
    if (f != 0 && *f != '\0')
	fprintf(fp, "Filter\t\t%s\n", f);

    // Save DataStation List
    char **ds;
    unsigned int n = snoopgadget->get(&ds);
    if (n != 0) {
	for (unsigned int i = 0; i < n; i++) {
	    fprintf(fp, "DataStation\t%s\n", ds[i]);
	    delete ds[i];
	}
	delete ds;
    }

    // Save hidden objects
    ds = hidegadget->get();
    if (ds != 0) {
	for (unsigned int i = 0; ds[i] != 0; i++) {
	    fprintf(fp, "Hide\t\t%s\n", ds[i]);
	    delete ds[i];
	}
	delete ds;
    }

    // Save window geometry
    fprintf(fp, "NetLookGeometry\t%dx%d%+d%+d\n",
		viewWindow->getWidth(), viewWindow->getHeight(),
		viewWindow->getXOrigin(), viewWindow->getYOrigin());
    if ((openGadgets & openMapWindow) != 0) {
	fprintf(fp, "NetLookMapGeometry\t%dx%d%+d%+d\n",
		    mapWindow->getWidth(), mapWindow->getHeight(),
		    mapWindow->getXOrigin(), mapWindow->getYOrigin());
    }
    if ((openGadgets & openSnoopWindow) != 0) {
	fprintf(fp, "NetLookSnoopGeometry\t%dx%d%+d%+d\n",
		    snoopWindow->getWidth(), snoopWindow->getHeight(),
		    snoopWindow->getXOrigin(), snoopWindow->getYOrigin());
    }
    if ((openGadgets & openNetNodeWindow) != 0) {
	fprintf(fp, "NetLookNetNodeGeometry\t%dx%d%+d%+d\n",
		    netnodeWindow->getWidth(), netnodeWindow->getHeight(),
		    netnodeWindow->getXOrigin(), netnodeWindow->getYOrigin());
    }
    if ((openGadgets & openTrafficWindow) != 0) {
	fprintf(fp, "NetLookTrafficGeometry\t%dx%d%+d%+d\n",
		    trafficWindow->getWidth(), trafficWindow->getHeight(),
		    trafficWindow->getXOrigin(), trafficWindow->getYOrigin());
    }
    if ((openGadgets & openHideWindow) != 0) {
	fprintf(fp, "NetLookHideGeometry\t%dx%d%+d%+d\n",
		    hideWindow->getWidth(), hideWindow->getHeight(),
		    hideWindow->getXOrigin(), hideWindow->getYOrigin());
    }

    if (fclose(fp) != 0) {
	dialog->warning(errno, "Could not write %s", lastDataFile);
	setDialogBoxCallBack();
	return 0;
    }
    return 1;
}

Node *
NetLook::addNode(const char *c)
{
    InterfaceNode i;

    if (inet_addr(c) != INADDR_NONE)
	i.ipaddr = c;
    else
	i.setName(c);
    i.complete();
    if (!i.ipaddr.isValid())
	return 0;
    return addNode(*i.ipaddr.getAddr(), i.getName());
}

Node *
NetLook::addNode(struct in_addr a, const char *name)
{
    IPNetNum na = a;
    NetworkNode *n = netdb.match(na);
    if (n == 0) {
	// Create a new network
	n = new NetworkNode(&config);
	n->ipnum = na;
	n->complete();
	netdb.enter(n);
    }
    if (n->ipmask.isValid()) {
	na.setAddr(a, n->ipmask);
	n = netdb.match(na);
	if (n == 0) {
	    // Create a new network
	    n = new NetworkNode(&config);
	    n->ipnum = na;
	    n->complete();
	    netdb.enter(n);
	}
    }
    SegmentNode *s = segdb.match(na);
    if (s == 0) {
	// Create a new segment
	s = new SegmentNode(n);
	s->ipnum = na;
	s->complete();
	segdb.enter(s);
	s->data = new Network(s);
	position();
    }
    IPAddr ia = a;
    InterfaceNode *i = ifdb.match(ia);
    if (i == 0) {
	// XXX - search for the host
	// Make a new host
	i = new InterfaceNode(s);
	HostNode *h = new HostNode;
	i->setHost(h);
	i->ipaddr = ia;
	i->setName(name);
	i->complete();
	ifdb.enter(i);
	i->data = new Node(i);
	((Network *) s->data)->place();
    }
    return (Node *) i->data;
}

void
NetLook::position(void)
{
    netlookgadget->position();
}

void
NetLook::setViewPort(void)
{
    viewgadget->setViewPort();
}

void
NetLook::setViewPort(Rectf &v)
{
    viewgadget->setViewPort(v);
}

Rectf &
NetLook::getViewPort(void)
{
    return viewgadget->getViewPort();
}

void
NetLook::home(void)
{
    Rectf u = netlookgadget->getUniverse();
    float w = netlookgadget->getWidth();
    float h = netlookgadget->getHeight();
    float x = u.getWidth() / w;
    float y = u.getHeight() / h;
    if (x > y)
	viewgadget->setViewPort(-w * x / 2.0, -h * x / 2.0, w * x, h * x);
    else
	viewgadget->setViewPort(-w * y / 2.0, -h * y / 2.0, w * y, h * y);
    netlookgadget->render();
    glmapgadget->render();
}

void
NetLook::setNetShow(unsigned int i)
{
    netShow = i;
    netlookgadget->render();
    glmapgadget->render();
}

void
NetLook::setNetDisplay(unsigned int i)
{
    netDisplay = i;
    for (NetworkNode *n = (NetworkNode *) config.child;
		n != 0; n = (NetworkNode *) n->next) {
	for (SegmentNode *s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next)
	    ((Network *) s->data)->updateDisplayString();
    }
    netlookgadget->render();
    glmapgadget->render();
}

void
NetLook::setNodeDisplay(unsigned int i)
{
    nodeDisplay = i;
    for (NetworkNode *n = (NetworkNode *) config.child;
		n != 0; n = (NetworkNode *) n->next) {
	for (SegmentNode *s = (SegmentNode *) n->child;
		s != 0; s = (SegmentNode *) s->next) {
	    for (InterfaceNode *i = (InterfaceNode *) s->child;
			i != 0; i = (InterfaceNode *) i->next)
		((Node *) i->data)->updateDisplayString();
	}
    }
    netlookgadget->render();
    glmapgadget->render();
}

void
NetLook::setNodeShow(unsigned int i)
{
    nodeShow = i;
    netlookgadget->render();
    glmapgadget->render();
}

void
NetLook::setTrafficBasis(unsigned int i)
{
    trafficBasis = i;
}

void
NetLook::setTrafficMode(unsigned int i)
{
    trafficMode = i;
    conntable->clear();
}

void
NetLook::setTrafficRescale(unsigned int i)
{
    trafficRescale = i;
    setTimer();
}

void
NetLook::setTrafficRescaleValue(unsigned int i)
{
    trafficRescaleValue = i;
    setTimer();
}

void
NetLook::setTrafficTimeout(unsigned int i)
{
    trafficTimeout = i;
    setTimer();
}

void
NetLook::setTrafficTimeoutValue(unsigned int i)
{
    trafficTimeoutValue = i;
    setTimer();
}

void
NetLook::setTrafficMinColorValue(unsigned int i)
{
    trafficMinColorValue = i;
    if (trafficMinColorValue < trafficMaxColorValue)
	trafficColorValues = trafficMaxColorValue - trafficMinColorValue + 1;
    else
	trafficColorValues = trafficMinColorValue - trafficMaxColorValue + 1;
    conntable->rescale();
    netlookgadget->render();
    glmapgadget->render();
}

void
NetLook::setTrafficMaxColorValue(unsigned int i)
{
    trafficMaxColorValue = i;
    if (trafficMinColorValue < trafficMaxColorValue)
	trafficColorValues = trafficMaxColorValue - trafficMinColorValue + 1;
    else
	trafficColorValues = trafficMinColorValue - trafficMaxColorValue + 1;
    conntable->rescale();
    netlookgadget->render();
    glmapgadget->render();
}

void
NetLook::setTrafficPacketColorStep(unsigned int i)
{
    trafficPacketColorStep = i;
    conntable->rescale();
    netlookgadget->render();
    glmapgadget->render();
}

void
NetLook::setTrafficByteColorStep(unsigned int i)
{
    trafficByteColorStep = i;
    conntable->rescale();
    netlookgadget->render();
    glmapgadget->render();
}

enum PickObjects
NetLook::getPickedObjectType(void)
{
    return pickedObjectType;
}

Network *
NetLook::getPickedNetwork(void)
{
    return pickedObjectType == pkNetwork ? (Network *) pickedObject : 0;
}

Node *
NetLook::getPickedNode(void)
{
    return pickedObjectType == pkNode ? (Node *) pickedObject : 0;
}

Connection *
NetLook::getPickedConnection(void)
{
    return pickedObjectType == pkConnection ? (Connection *) pickedObject : 0;
}

void
NetLook::unpickObject(void)
{
    switch (pickedObjectType) {
	case pkNone:
	    return;
	case pkNetwork:
	    ((Network *) pickedObject)->info &= ~NETWORKPICKED;
	    break;
	case pkNode:
	    ((Node *) pickedObject)->info &= ~NODEPICKED;
	    ((Node *) pickedObject)->getNetwork()->info &= ~NODEPICKED;
	    break;
	case pkConnection:
	    {
		Connection *c = (Connection *) pickedObject;
		c->info &= ~CONNPICKED;
		c->src.node->info &= ~CONNPICKED;
		c->src.node->getNetwork()->info &= ~CONNPICKED;
		c->dst.node->info &= ~CONNPICKED;
		c->dst.node->getNetwork()->info &= ~CONNPICKED;
	    }
	    break;
    }

    pickedObject = 0;
    pickedObjectType = pkNone;
    selectiongadget->setSelection(0);
}

void
NetLook::setPickedNetwork(Network *n)
{
    if (pickedObjectType != pkNone)
	unpickObject();

    if (n != 0) {
	pickedObjectType = pkNetwork;
	pickedObject = n;
	n->info |= NETWORKPICKED;
	selectiongadget->setSelection(n->display);
    }
}

void
NetLook::setPickedNode(Node *n)
{
    if (pickedObjectType != pkNone)
	unpickObject();

    if (n != 0) {
	pickedObjectType = pkNode;
	pickedObject = n;
	n->info |= NODEPICKED;
	n->getNetwork()->info |= NODEPICKED;
	selectiongadget->setSelection(n->display);
    }
}

void
NetLook::setPickedConnection(Connection *c)
{
    if (pickedObjectType != pkNone)
	unpickObject();

    if (c != 0) {
	pickedObjectType = pkConnection;
	pickedObject = c;
	c->info |= CONNPICKED;
	c->src.node->info |= CONNPICKED;
	c->src.node->getNetwork()->info |= CONNPICKED;
	c->dst.node->info |= CONNPICKED;
	c->dst.node->getNetwork()->info |= CONNPICKED;
    }
}

void
NetLook::setNormalCursor(tuGadget *g)
{
    ((nlWindow *) g->getTopLevel())->setNormalCursor();
}

void
NetLook::setDragCursor(tuGadget *g)
{
    ((nlWindow *) g->getTopLevel())->setDragCursor();
}

void
NetLook::setSweepCursor(tuGadget *g)
{
    ((nlWindow *) g->getTopLevel())->setSweepCursor();
}

char *
NetLook::expandTilde(const char *c)
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
}
