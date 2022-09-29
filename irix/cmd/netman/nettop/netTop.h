#ifndef __netTop_h_
#define __netTop_h_

/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetTop
 *
 *	$Revision: 1.18 $
 *	$Date: 1996/02/26 01:27:58 $
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

#include <netinet/in.h>
#include "protocols/decnet.h"
#include "protocols/ether.h"
#include "histogram.h"

#include "top.h"

extern "C" {

#include <rpc/types.h>
#include "snoopstream.h"

}


#include "constants.h"
#include "viewGadget.h"

class Display;
class tuColorMap;
class tuFilePrompter;
class tuResourceDB;
class tuScreen;
class tuTimer;
class tuTopLevel;
class tuXExec;

class EV_event;
class DialogBox;
class ToolOptions;
class DataGizmo;
class NodeGizmo;
class Top;

typedef struct {
    char**	displayStringPtr; // pointer to one of the others
    char*	filter;
    char*	fullname;
    char*	name;
    char*	vendaddr;
    char*	physaddr;
    char*	ipaddr;
    char*	dnaddr;
    tuBool	err;
    tuBool	locked;
} nodeInfo;

// Define some application specific resources.  Right now, they all
// need to be placed into one structure.  Note that most of these
// names match up to the command line arguments above.
struct AppResources {
    char* interface;
    char* controlsFile;
    char* hostresorder;
    tuBool useyp;
    tuBool help;
    
    short  number_sources;  // this is a workaround
    
    tuBool nohist;	    // this one is secret
    tuBool noaddrlist;	    // this one is secret
    tuBool colormap;	    // this one is secret
    
    long nodeControlBackgroundColor;
    long trafficControlBackgroundColor;
    
    char* mainBackgroundRGBColor;
    char* selectRGBColor;
    char* labelRGBColor;
    char* nameRGBColor;
    char* nameErrorRGBColor;
    char* nameLockedRGBColor;
    char* baseRGBColor;
    char* scaleRGBColor;
    char* titleRGBColor;
    char* edgeRGBColor;

    short mainBackgroundIndexColor;
    short labelIndexColor;
    short selectIndexColor;
    short nameIndexColor;
    short nameErrorIndexColor;
    short nameLockedIndexColor;
    short baseIndexColor;
    short scaleIndexColor;
    short titleIndexColor;
    short edgeIndexColor;
    
    short totalTotalIndexColor;
    short otherOtherIndexColor;
    short totalAnyIndexColor;
    short otherAnyIndexColor;
    short anyAnyIndexColor;
};

class NetTop {
public:
	NetTop(int argc, char *argv[]);
	NetTop(const char* instanceName, tuColorMap* cmap,
	      tuResourceDB* db, char* progName, char* progClass);

	Display *getDpy(void);
	
	float	data[MAX_NODES][MAX_NODES];	// what is drawn
	
	nodeInfo    srcInfo[MAX_NODES];
	nodeInfo    destInfo[MAX_NODES];

	SnoopStream	histSnpStm;

	void	quit(tuGadget *);
	static void	doHelp(tuGadget *, void *);
	void	save(tuGadget *);
	void	saveAs(tuGadget *);
	void	saveImageAs(tuGadget *);

	void	post(int msg, char *errString = NULL, int lineNumber = 0);
	int	openNetFilters(void);
	void	closeNetFilters(void);
	
	int	snoopStart(SnoopStream* streamp, tuBool doEvent = False);
	int	snoopStop(SnoopStream*  streamp, tuBool doEvent = False);

	int	restartSnooping(tuBool reopen = False, tuBool doEvent = False);
	
	void	    setExec(tuXExec *e);
	tuXExec*    getExec(void)	{ return exec; }
	
	DialogBox*  getDialogBox(void)	{ return dialog; }
	void	    openDialogBox(void);
	void	    closeDialogBox(tuGadget *);
	void	    closeLicDialogBox(tuGadget *);
	void	    setDialogBoxCallBack(void);

	void	    setFatal(void);
	void	    setQuitNow(void)	{ quitNow = True; }
	
	long	getNodeControlBackgroundColor()
			{ return ar->nodeControlBackgroundColor; }
	long	getTrafficControlBackgroundColor()
			{ return ar->trafficControlBackgroundColor; }
			
	void	getRGBColors(long*, long*, long*, long*, long*,
			     long*, long*, long*, long*, long*);
	void	getIndexColors(short*, short*, short*, short*, short*,
			       short*, short*, short*, short*, short*,
			       short*, short*, short*, short*, short*);
	
	int	setInterface(char* i);
	char*	getFullSnooperString(void);
	h_info*	getCurrentSnooper()	{ return &currentSnooper; }
	h_info*	getNewSnooper()		{ return &newSnooper; }

	tuBool	isEther(void);
	tuBool	isFddi(void);
	tuBool	isIconic(void)		{ return iconic; }
	
	void	setFilter(char* f);
	char*	getFilter(void)		{ return filter; }

	// the next two are just used for initial values
	int	getFixedScale()		{ return fixedScale; }
	tuBool	getBusyBytes()		{ return busyBytes; }

	void	setSrcAndDest(tuBool);
	tuBool	getSrcAndDest(void)	{ return doSrcAndDest; }
	void	setNumPairs(int);
	void	setNumSrcs(int);
	void	setNumDests(int);
	void	setNumNodes(int);
	void	setNumFilters(int);
	int	getNumPairs(void)	{ return numSrcs; }
	int	getNumSrcs(void)	{ return numSrcs; }
	int	getNumDests(void)	{ return numDests; }
	int	getNumNodes(void);
	int	getNumFilters(void);
	
	int	setDataUpdateTime(int t);
	int	setDataSmoothPerc(int p);
	int	setNodeUpdateTime(int t);
	int	setScaleUpdateTime(int t);
	int	getDataUpdateTime(void)	{ return dataUpdateTime; }
	int	getDataSmoothPerc(void)	{ return dataSmoothPerc; }
	int	getNodeUpdateTime(void)	{ return nodeUpdateTime; }
	int	getScaleUpdateTime(void) { return scaleUpdateTime; }
	
	void	setInclinationScrollBar(int i);
	void	setAzimuthScrollBar(int a);
	void	setDistanceScrollBar(int d);

	void	renderGraph(void)	{ viewgadget->renderGraph(); }

	void	setHowDraw(int type)	{ viewgadget->setHowDraw(type); }
	void	setScale(int s)		{ viewgadget->setScale(s); }
	int	getHowDraw(void)	{ return viewgadget->getHowDraw(); }
	int	getScale(void)		{ return viewgadget->getScale(); }
	int	getRescaleType(void)	{ return rescaleType; }

	void	setGraphType(int type)	{ graphType = type; viewgadget->figureScale(); }
	void	setLabelType(int type);
	void	setNPackets(int n)	{ nPackets = n; }
	void	setNBytes(int n)	{ nBytes = n; }
	int	getGraphType(void)	{ return graphType; }
	int	getLabelType(void)	{ return labelType; }
	int	getNPackets(void)	{ return nPackets; }
	int	getNBytes(void)		{ return nBytes; }
	
	int	getTotalSrc(void)	{ return totalSrc; }
	int	getOtherSrc(void)	{ return otherSrc; }
	int	getTotalDest(void)	{ return totalDest; }
	int	getOtherDest(void)	{ return otherDest; }
	
	tuBool	getSrcLocked(int s)	{ return srcInfo[s].locked; }
	tuBool	getDestLocked(int d)	{ return destInfo[d].locked; }
	void	setSrcLocked(int s, tuBool locked, tuBool tellNodeGizmo = FALSE);
	void	setDestLocked(int d, tuBool locked, tuBool tellNodeGizmo = FALSE);
	int	getNumSrcsLocked(void)	{ return numSrcsLocked; }
	int	getNumDestsLocked(void)	{ return numDestsLocked; }
	
	void	setShowTotals(tuBool s)	{ viewgadget->setShowTotals(s); }
	void	setLighting(tuBool l)	{ viewgadget->setLighting(l); }
	void	startSmoothing(void);
	void	stopSmoothing(void);
	void	setLogSmooth(tuBool);
	tuBool	getLogSmooth(void)	{ return useLogSmoothing; }
	
	int	restartTopSnooping(void);
	int	startTopSnooping(void);
	int	stopTopSnooping(void);
	
	void	setWhichAxes(int);
	void	setWhichSrcs(int);
	void	setWhichDests(int);
	void	setWhichNodes(int);
	void	setBusyBytes(tuBool b)	{ busyBytes = b; top->setBusyBytes(b); }
	int	getWhichAxes(void);
	int	getWhichSrcs(void);
	int	getWhichDests(void);
	int	getWhichNodes(void);
	
	void	setUseEaddr(tuBool);
	tuBool	getUseEaddr(void);
	
	float	getCurrentMax(void);
	float	getLastMax(void);
	
	void	setLastGizmoFocused(tuTopLevel* last)
			{ lastGizmoFocused = last; }

private:
	tuResourceDB	*rdb;
	AppResources	*ar;
	tuColorMap	*cmap;
	Display		*display;
	tuXExec		*exec;

	EV_event	*event;
	EV_event	*alarmEvent;

	Top		*top;
	ViewGadget	*viewgadget;
	DataGizmo	*datagizmo;
	NodeGizmo	*nodegizmo;
	ToolOptions	*tooloptions;	
	DialogBox	*dialog, *licDialog;
	DialogBox	*questionDialog;
	tuTopLevel	*lastGizmoFocused;
			    // which gizmo last changed focused gadget
	tuFilePrompter*	prompter;
	
	char*		viewgadgetGeom;
	char*		datagizmoGeom;
	char*		nodegizmoGeom;
	
	tuBool		datagizmoWasMapped;
	tuBool		nodegizmoWasMapped;
	tuBool		iconic;
	int		datagizmoX, datagizmoY;
	int		nodegizmoX, nodegizmoY;
		
	char		lastFile[FILENAMESIZE];
	FILE		*netfilters;
	int		netfiltersPid;
	
	float		currentMax; // highest value seen in this interval
	float		lastMax;    // highest value seen in last interval
	float		newData[MAX_NODES][MAX_NODES];
			    // data received from snoopd
	float		deltaData[MAX_NODES][MAX_NODES];
			    // how much to move each step
	int		numSteps;   // how many steps over which to smooth data
	int		stepsTaken; // how many we've done already 
	int		numSrcs;
	int		numDests;
	int		totalSrc;   // src that's really total, not real source
	int		otherSrc;
	int		totalDest;
	int		otherDest;
	
	int		whichAxes;  // src&dest, pairs, nodes&filters
	int		whichSrcs; // busy, specific
	int		whichDests;
	int		whichNodes;
	
	int		graphType;  // bytes, packets, etc.
	int		labelType;  // local, full, ip, etc.
	int		nPackets;   // for "% of N packets"
	int		nBytes;
	int		dataUpdateTime;	// in tenths of a second
	int		dataSmoothPeriod;   // in tenths of a second
	float		dataSmoothInterval; // in seconds
	int		dataSmoothPerc;	// percentage of dataUpdateTime
	int		nodeUpdateTime;	// in tenths
	int		scaleUpdateTime;// in tenths
	tuTimer		*smoothTimer;
	int		rescaleType;
	// the next two are just used for initial values
	tuBool		busyBytes;
	int		fixedScale;

	tuBool		fatalError;
	tuBool		postedFatal;
	tuBool		quitNow;
	tuBool		snooping;
	tuBool		subscribed; // CVG
	tuBool		doTopOutput;
	tuBool		useLogSmoothing;
	tuBool		gotInterfaceFromResource;
	tuBool		doSrcAndDest;  // T = src & dest; F = node & filter
	tuBool		srcLocked[MAX_NODES];
	tuBool		destLocked[MAX_NODES];
	int		numSrcsLocked;
	int		numDestsLocked;
	
	h_info		currentSnooper;
	h_info		newSnooper;
	char		fullSnooperString[100];
	char		localHostName[64];
	
	char		windowName[80];
	char*		filter;
	int		highestBin;
	struct histogram hist;
	struct histogram oldHist;
	int		bins[MAX_NODES][MAX_NODES]; // filter bin for each pair

	void		doQuit(tuGadget *);
	void		immediateQuit(tuGadget *);
	int		doSave();
	void		savePrompt(tuGadget *);

	void		postMany(int count, char* msgs[]);

	void		readNf(tuGadget *);
	void		readNfForNodeG(tuGadget *);
	void		readNfForDataG(tuGadget *);
	
	void		assignNames(void);
	void		zeroData(void);	
	void		readData(tuGadget*);    // fills in data[][]
	void		smoothData(tuGadget*);  // steps data toward newData
	

	int		addFilter(SnoopStream*, char* filtExpr);
	void		addMatrixFilters(SnoopStream*);
	int		snoopInit(void);

	void		snooperChanged();
	int		handleIntfc(char*);

	char*		expandTilde(const char*);
	int		readFile(char*);
	int		handleOptions(int, char**);
	void		usage(void);

	void		openDataGizmo(tuGadget *);
	void		closeDataGizmo(void);
	void		openNodeGizmo(tuGadget *);
	void		closeNodeGizmo(void);
	void		iconicCB(tuGadget *);

	void		doFullName(nodeInfo*);
	void		doHostName(nodeInfo*);
	void		doInet(nodeInfo*);
	void		doDecnet(nodeInfo*);
	void		doEnetVen(nodeInfo*);
	void		doEnet(nodeInfo*);

};


#endif /* __netTop_h_ */
