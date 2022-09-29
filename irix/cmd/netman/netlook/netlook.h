/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Netlook
 *
 *	$Revision: 1.9 $
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

#include "cf.h"
#include "db.h"
#include "define.h"

class tuResourceDB;
class tuScreen;
class tuColorMap;
struct _XDisplay;
class tuGadget;
class tuXExec;
class tuTimer;
class tuTopLevel;
struct AppResources;
class nlWindow;
class Network;
class Node;
class Comm;
class ConnTable;
class Connection;
class DialogBox;
class Rectf;
class EV_handler;
struct __file_s;

typedef unsigned char tuBool;

// Objects the user can pick
enum PickObjects { pkNone, pkNetwork, pkNode, pkConnection };

// Resources for NetLook
struct AppResources {
    int buffer;
    int interval;
    char *networksFile;
    char *controlsFile;
    char *hostresorder;
    unsigned char useyp;
    unsigned char sendDetectEvents;
    char *pingCommand;
    char *traceRouteCommand;
    unsigned int mapBackgroundColor;
    unsigned int viewboxColor;
    unsigned int selectColor;
    unsigned int adjustColor;
    unsigned int activeNetworkColor;
    unsigned int inactiveNetworkColor;
    unsigned int standardNodeColor;
    unsigned int activeDataStationColor;
    unsigned int NISMasterColor;
    unsigned int NISSlaveColor;
    unsigned int gatewayColor;
};

class NetLook {
public:
	NetLook(int argc, char *argv[]);

	// Run does not return
	void run(void);

	// Window stuff
	inline struct _XDisplay *getDpy(void);
	inline tuTopLevel *getTopLevel(void);
	inline tuBool isIconic(void);
	void setNormalCursor(tuGadget *);
	void setDragCursor(tuGadget *);
	void setSweepCursor(tuGadget *);

	// Map Gadget
	void openMapGadget(void);
	void closeMapGadget(tuGadget *);
	inline nlWindow *getMapWindow(void);

	// Snoop Gadget
	void openSnoopGadget(void);
	void closeSnoopGadget(tuGadget *);

	// Network and Node Gadget
	void openNetNodeGadget(void);
	void closeNetNodeGadget(tuGadget *);

	// Traffic Gadget
	void openTrafficGadget(void);
	void closeTrafficGadget(tuGadget *);

	// Hide Gadget
	void openHideGadget(void);
	void closeHideGadget(tuGadget *);

	// Selection Gadget
	void openSelectionGadget(void);
	void closeSelectionGadget(tuGadget *);

	// Dialog Box
	void openDialogBox(void);
	void closeDialogBox(tuGadget *);
	inline DialogBox *getDialogBox(void);
	void setDialogBoxCallBack(void);

	// Infomation Box
	void openInfoBox(void);
	void closeInfoBox(tuGadget *);

	// Archiver
	unsigned int openArchiver(void);
	void closeArchiver(void);

	// Iconify
	void iconify(tuGadget *);

	// Quit
	void quit(tuGadget * = 0);
	void doQuit(tuGadget *);
	void immediateQuit(tuGadget * = 0);

	// Start snooping on a node
	Node *startSnoop(const char *);
	unsigned int stopSnoop(Node *);

	// Set filter expression
	unsigned int setFilter(const char *);

	// Hiding
	void *hide(const char *, unsigned int noposition = 0);
	Network *hide(Network *, unsigned int noposition = 0);
	Node *hide(Node *, unsigned int noposition = 0);
	unsigned int unhide(const char *);

	// Clear data
	void clear(void);

	// Read and write UI files
	unsigned int openUIFile(const char *filename = 0);
	unsigned int saveUIFile(const char *filename = 0);

	// Read and write data files
	unsigned int openDataFile(const char *filename = 0);
	unsigned int saveDataFile(const char *filename = 0);

	// Get configuration database
	inline CfNode *getDatabase(void);
	inline InterfaceDB *getIfDB(void);
	inline SegmentDB *getSegDB(void);
	inline NetworkDB *getNetDB(void);

	// Operations
	unsigned int find(const char *, unsigned int findHidden = 0);
	unsigned int ping(const char *);
	unsigned int trace(const char *);
	unsigned int Delete(const char *);

	// Get the connection table
	inline ConnTable *getConnectionTable(void);

	// Position networks and nodes on screen
	void position(void);

	// User pick
	enum PickObjects getPickedObjectType(void);
	void setPickedNetwork(Network *);
	Network *getPickedNetwork(void);
	void setPickedNode(Node *);
	Node *getPickedNode(void);
	void setPickedConnection(Connection *);
	Connection *getPickedConnection(void);
	void unpickObject(void);

	// Get and set stuff
	inline unsigned int getNetDisplay(void);
	void setNetDisplay(unsigned int);

	inline unsigned int getNetShow(void);
	void setNetShow(unsigned int);

	inline unsigned int getNetIgnore(void);
	inline void setNetIgnore(unsigned int);

	inline unsigned int getNodeDisplay(void);
	void setNodeDisplay(unsigned int);

	inline unsigned int getNodeShow(void);
	void setNodeShow(unsigned int);

	inline unsigned int getNodeIgnore(void);
	inline void setNodeIgnore(unsigned int);

	inline unsigned int getTrafficBasis(void);
	void setTrafficBasis(unsigned int);

	inline unsigned int getTrafficMode(void);
	void setTrafficMode(unsigned int);

	inline unsigned int getTrafficRescale(void);
	void setTrafficRescale(unsigned int);

	inline unsigned int getTrafficRescaleValue(void);
	void setTrafficRescaleValue(unsigned int);

	inline unsigned int getTrafficTimeout(void);
	void setTrafficTimeout(unsigned int);

	inline unsigned int getTrafficTimeoutValue(void);
	void setTrafficTimeoutValue(unsigned int);

	inline unsigned int getTrafficMinColorValue(void);
	void setTrafficMinColorValue(unsigned int);

	inline unsigned int getTrafficMaxColorValue(void);
	void setTrafficMaxColorValue(unsigned int);

	inline unsigned int getTrafficColorValues(void);

	inline unsigned int getTrafficPacketColorStep(void);
	void setTrafficPacketColorStep(unsigned int);

	inline unsigned int getTrafficByteColorStep(void);
	void setTrafficByteColorStep(unsigned int);

	// Colors
	inline unsigned int getMapBackgroundColor(void);
	inline unsigned int getViewboxColor(void);
	inline unsigned int getSelectColor(void);
	inline unsigned int getAdjustColor(void);
	inline unsigned int getActiveNetworkColor(void);
	inline unsigned int getInactiveNetworkColor(void);
	inline unsigned int getStandardNodeColor(void);
	inline unsigned int getActiveDataStationColor(void);
	inline unsigned int getNISMasterColor(void);
	inline unsigned int getNISSlaveColor(void);
	inline unsigned int getGatewayColor(void);

	// Viewport functions
	void setViewPort(void);
	void setViewPort(Rectf &);
	Rectf &getViewPort(void);
	void home(void);

	// Get the event handler
	inline EV_handler *getEventHandler(void);

	// Check to send detect events
	inline unsigned int getSendDetectEvents(void);

protected:
	// Real initialization work
	void initialize(tuGadget *);

	// Callback when snoop socket is ready
	void read(tuGadget *);

	// Callback from archiver
	void readArchiver(tuGadget *);

	// Clear the database
	void blank(void);

	// Find a node or network given a string
	Node *getNode(const char *);
	Network *getNetwork(const char *);

	// Add a node
	Node *addNode(const char *);
	Node *addNode(struct in_addr, const char *);

	// Delete callback
	void doDelete(tuGadget *);

	// Timer functions
	void setTimer(void);
	void tick(tuGadget *);

	// Open up gadgets
	void openUpGadgets(void);

	// Expand ~ in resources
	char *expandTilde(const char *);

	// Set ToolOptions interface
	void setToolsInterface(void);

private:
	AppResources *ar;
	char *instanceName;
	char *className;
	tuResourceDB *rdb;
	tuScreen *screen;
	tuColorMap *cmap;
	struct _XDisplay *display;
	tuXExec *exec;
	tuTimer *timer;
	EV_handler *eventHandler;

	nlWindow *viewWindow;
	nlWindow *mapWindow;
	nlWindow *snoopWindow;
	nlWindow *netnodeWindow;
	nlWindow *trafficWindow;
	nlWindow *hideWindow;
	nlWindow *selectionWindow;
	DialogBox *dialog;

	CfNode config;
	InterfaceDB ifdb;
	SegmentDB segdb;
	NetworkDB netdb;

	Comm *comm;

	ConnTable *conntable;

	struct __file_s *archiver;
	int archiverPid;

	char *lastDataFile;
	char *lastUIFile;
	unsigned int openGadgets;

	enum PickObjects pickedObjectType;
	void *pickedObject;

	Node *thisNode;
	char thisName[HOSTNAMESIZE];
	IPAddr thisAddr;
	IPAddr thisMask;
	IPNetNum thisNet;

	unsigned int netDisplay;
	unsigned int netShow;
	unsigned int netIgnore;

	unsigned int nodeDisplay;
	unsigned int nodeShow;
	unsigned int nodeIgnore;

	unsigned int trafficBasis;
	unsigned int trafficMode;
	unsigned int trafficRescale;
	unsigned int trafficRescaleValue;
	unsigned int trafficTimeout;
	unsigned int trafficTimeoutValue;
	unsigned int trafficMinColorValue;
	unsigned int trafficMaxColorValue;
	unsigned int trafficColorValues;
	unsigned int trafficPacketColorStep;
	unsigned int trafficByteColorStep;

	tuBool iconic;

	// For start up
	unsigned int datastations;
	char **dataStationList;
};

// Inline functions
struct _XDisplay *
NetLook::getDpy(void)
{
    return display;
}

nlWindow *
NetLook::getMapWindow(void)
{
    return mapWindow;
}

tuTopLevel *
NetLook::getTopLevel(void)
{
    return (tuTopLevel *) viewWindow;
}

tuBool
NetLook::isIconic(void)
{
    return iconic;
}

DialogBox *
NetLook::getDialogBox(void)
{
    return dialog;
}

CfNode *
NetLook::getDatabase(void)
{
    return &config;
}

ConnTable *
NetLook::getConnectionTable(void)
{
    return conntable;
}

unsigned int
NetLook::getNetDisplay(void)
{
    return netDisplay;
}

unsigned int
NetLook::getNetShow(void)
{
    return netShow;
}

unsigned int
NetLook::getNetIgnore(void)
{
    return netIgnore;
}

void
NetLook::setNetIgnore(unsigned int i)
{
    netIgnore = i;
}

unsigned int
NetLook::getNodeDisplay(void)
{
    return nodeDisplay;
}

unsigned int
NetLook::getNodeShow(void)
{
    return nodeShow;
}

unsigned int
NetLook::getNodeIgnore(void)
{
    return nodeIgnore;
}

void
NetLook::setNodeIgnore(unsigned int i)
{
    nodeIgnore = i;
}

unsigned int
NetLook::getTrafficBasis(void)
{
    return trafficBasis;
}

unsigned int
NetLook::getTrafficMode(void)
{
    return trafficMode;
}

unsigned int
NetLook::getTrafficRescale(void)
{
    return trafficRescale;
}

unsigned int
NetLook::getTrafficRescaleValue(void)
{
    return trafficRescaleValue;
}

unsigned int
NetLook::getTrafficTimeout(void)
{
    return trafficTimeout;
}

unsigned int
NetLook::getTrafficTimeoutValue(void)
{
    return trafficTimeoutValue;
}

unsigned int
NetLook::getTrafficMinColorValue(void)
{
    return trafficMinColorValue;
}

unsigned int
NetLook::getTrafficMaxColorValue(void)
{
    return trafficMaxColorValue;
}

unsigned int
NetLook::getTrafficColorValues(void)
{
    return trafficColorValues;
}

unsigned int
NetLook::getTrafficPacketColorStep(void)
{
    return trafficPacketColorStep;
}

unsigned int
NetLook::getTrafficByteColorStep(void)
{
    return trafficByteColorStep;
}

InterfaceDB *
NetLook::getIfDB(void)
{
    return &ifdb;
}

SegmentDB *
NetLook::getSegDB(void)
{
    return &segdb;
}

NetworkDB *
NetLook::getNetDB(void)
{
    return &netdb;
}

// Colors
unsigned int
NetLook::getMapBackgroundColor(void)
{
    return ar->mapBackgroundColor;
}

unsigned int
NetLook::getViewboxColor(void)
{
    return ar->viewboxColor;
}

unsigned int
NetLook::getSelectColor(void)
{
    return ar->selectColor;
}

unsigned int
NetLook::getAdjustColor(void)
{
    return ar->adjustColor;
}

unsigned int
NetLook::getActiveNetworkColor(void)
{
    return ar->activeNetworkColor;
}

unsigned int
NetLook::getInactiveNetworkColor(void)
{
    return ar->inactiveNetworkColor;
}

unsigned int
NetLook::getStandardNodeColor(void)
{
    return ar->standardNodeColor;
}

unsigned int
NetLook::getActiveDataStationColor(void)
{
    return ar->activeDataStationColor;
}

unsigned int
NetLook::getNISMasterColor(void)
{
    return ar->NISMasterColor;
}

unsigned int
NetLook::getNISSlaveColor(void)
{
    return ar->NISSlaveColor;
}

unsigned int
NetLook::getGatewayColor(void)
{
    return ar->gatewayColor;
}

EV_handler *
NetLook::getEventHandler(void)
{
    return eventHandler;
}

unsigned int
NetLook::getSendDetectEvents(void)
{
   return ar->sendDetectEvents;
}
