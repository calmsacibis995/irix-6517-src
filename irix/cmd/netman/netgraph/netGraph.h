#ifndef __netGraph_h_
#define __netGraph_h_
/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	NetGraph
 *
 *	$Revision: 1.18 $
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

#include <netinet/in.h>
#include "protocols/decnet.h"
#include "protocols/ether.h"
#include "histogram.h"


extern "C" {

#include <rpc/types.h>
#include "snoopstream.h"

}


#include "constants.h"
#include "stripGadget.h"

typedef struct dataStrStr {
    dataStruct*	data;
    struct dataStrStr* next;
} dataStruct2;


class Display;
class tuColor;
class tuColorMap;
class tuFilePrompter;
class tuResourceDB;
class tuRowColumn;
class tuScreen;
class tuTimer;
class tuTopLevel;
class tuXExec;

class EV_event;
class EV_handler;
class DialogBox;
class ToolOptions;

class Arg;
class EditControl;
class ParamControl;
class ScrollGadget;
class TimeGadget;
class ViewGadget;

class NetGraph {
public:
	NetGraph(int argc, char *argv[]);
	NetGraph(const char* instanceName, tuColorMap* cmap,
	      tuResourceDB* db, char* progName, char* progClass);

	Display *getDpy(void);
		

	SnoopStream	histSnpStm;

	static void doHelp(tuGadget *, void *);
	void	    quit(tuGadget *);
	void	    save(tuGadget *);
	void	    saveAs(tuGadget *);


	void	    post(int msg, char *errString = NULL);
//	void	    post(int msg, char *errString = NULL, int lineNumber = 0);
	int	    openNetFilters(void);
	void	    readNetFilters(tuGadget*);
	void	    closeNetFilters(void);
	
	void	    setExec(tuXExec *e);
	tuXExec*    getExec(void);
	
	DialogBox*  getDialogBox(void);
	void	    openDialogBox(void);
	void	    closeDialogBox(tuGadget *);
	void	    closeLicDialogBox(tuGadget *);
	void	    setDialogBoxCallBack(void);

	char*	    getFullSnooperString();

	tuBool	    isEther(void);
	tuBool	    isFddi(void);
	tuBool	    isTokenRing(void);
	tuBool	    isIconic(void);
	
	void	    deleteGraph(StripGadget*);
//	void	    addEdit(int, long);
	void	    doParams();
	void	    throttle(double);
	void	    scroll(double);
	void	    scrollInc(int);
	void	    ask(int msg,int event,char *text);
	void	    saveAndQuit();

	int	    getInterval(void)	    { return interval; }
	int	    getPeriod(void)	    { return period; }
	int	    getAvgPeriod(void)	    { return avgPeriod; }
	int	    getUpdateTime(void) { return updateTime; }
	int	    getScale(void);
	tuBool	    getSameScale()	    { return sameScale; }
	tuBool	    getLockPercentages()    { return lockPercentages; }
	tuBool	    getMaxValues()	    { return maxValues; }

	void	    setSameScale(tuBool newVal);
	void	    setLockPercentages(tuBool newVal);
	void	    setMaxVals(tuBool newVal);

	tuColor*    getAlarmColor()	    { return alarmColor; }
	tuColor*    getSelectColor()	    { return selectColor; }
	short	    getStripBackgroundIndexColor()
			    { return stripBackgroundIndexColor; }
	short	    getStripScaleIndexColor()
			    { return stripScaleIndexColor; }
	short	    getScrollingTimeIndexColor()
			    { return scrollingTimeIndexColor; }

	void	    renderGraph(void);
	void	    setTimeType(int t);
	int	    handleNewArgs(Arg*);
	
	int	    setupStripInts(char* filtExpr, int typeInt, int baseRate,
			int colorInt, int avgColorInt,
			int styleInt, tuBool alarmSet, tuBool alarmBell,
			float alarmLo, float alarmHi,
			int lineNumber, StripGadget* sg, tuBool postNow);

	int	    makeNewStrip(StripGadget**, StripGadget*);
	int	    completeNewStrip(StripGadget*, StripGadget*);


	void	    setSelectedStrip(StripGadget* s);
	StripGadget*    getSelectedStrip(void)	     { return selectedStrip; }
	
	tuBool	    getRecording()	{ return recording; }

	tuBool	    getHistory()	{ return history; }
	int	    getHighestNumber()  { return highestNumber; }
	void*	    getHistoryEnd()	{ return historyEnd; }
	int	    getTimeType()	{ return timeType; }
	struct timeval getH_timestamp()	{ return hist.h_timestamp; }
	
private:
	tuResourceDB	*rdb;
	tuColorMap	*cmap;
	Display		*display;
	tuXExec		*exec;
	EV_handler	*eh;
	EV_event	*event;
	EV_event	*alarmEvent;

	ViewGadget	*viewGadget;
	EditControl	*editControl;
	ParamControl	*paramControl;
	ToolOptions	*toolOptions;	
	DialogBox	*dialog, *licDialog, *usageDialog;
	
	tuRowColumn	*stripParent;
	tuGadget	*addItem, *editItem, *deleteItem;
	StripGadget	*strips;
	TimeGadget	*timeGadget;
	ScrollGadget	*scrollGadget;

	tuTimer*	timer;
	tuFilePrompter*	prompter;

	char*		viewGadgetGeom;
	char*		editControlGeom;
	char*		paramControlGeom;
	
	tuColor*	alarmColor;
	tuColor*	selectColor;
	short		stripBackgroundIndexColor;
	short		stripScaleIndexColor;
	short		scrollingTimeIndexColor;
	
	tuBool		editControlWasMapped;
	tuBool		paramControlWasMapped;
	tuBool		iconic;
	int		editControlX, editControlY;
	int		paramControlX, paramControlY;
		
	float		max[NUM_GRAPH_TYPES]; // highest val for each type

	FILE		*netfilters;
	int		netfiltersPid;	
	

	tuBool		fatalError;
	tuBool		quitNow;
	tuBool		snooping;
	
	h_info		currentSnooper;
	h_info		newSnooper;
	char		fullSnooperString[100];
	char		localHostName[65];
	
	char*		interface;
	char*		interfaceName;
	char*		newInterfaceName;
	char		windowName[80];

	int		highestBin;
	int		highestNumber;
	int		sampleSize;
	StripGadget*	selectedStrip; // which graph is selected
	
	struct histogram hist;
	struct histogram oldHist;
	
	char*		specifiedFilter;
	char*		rcLines[MAX_RC_LINES]; // xxx how many?
	void		doQuit(tuGadget *);
	void		immediateQuit(tuGadget *);
	int		doSave();
	void		savePrompt(tuGadget *);

	void		postMany(int count, char* msgs[]);

	void		protoInit(void);
	int		snoopInit(void);
	void		reallyStart(tuGadget*);
	
	void		snooperChanged();
	int		handleIntfc(char*, tuBool);
	int		readFile(char* fileName = NULL);
	int		doRestOfFile(int firstLine);
	char*		expandTilde(const char*);
	int		handleStringOfOptions(char*);
	int		handleOptions(int, char**);
	void		usage(void);
	
//	void		openAddControl(tuGadget *);
	void		add(tuGadget *);	
	void		openEditControl(tuGadget *);
	void		closeEditControl(void);
	void		openParamControl(tuGadget *);
	void		closeParamControl(void);
	void		iconicCB(tuGadget *);
	void		changeAll(tuGadget *);
	void		deleteCB(tuGadget *);
	void		catchUp(tuGadget *);

	char		lastFile[FILENAMESIZE];
	char		recordFile[FILENAMESIZE];
	char		logFile[FILENAMESIZE];
	FILE*		afp;	// log file pointer
	FILE*		rfp;	// record file pointer
	XDR		xdr;	// xdr stream for record file (in & out)
	void		*historyBeg, *historyEnd;
	long*		timePtr;
	float		timerSeconds;
	int		historyDirection;   // -1=back, 0=still, 1=forward
	int		historySamples;	    // total # samples in history file
	int		historyTicks;	    // how far we are into history
	
	int		interval;
	int		samples;
	int		period;
	int		avgSamples; // # samples over which to calc moving avg
	int		avgPeriod;  // time over which to calc moving avg
	int		updateTime;
	int		timeSinceUpdate;
	int		timeType;
	tuBool		sameScale;
	tuBool		lockPercentages;
	tuBool		maxValues;
	tuBool		useLines;

	tuBool		got[128];   // for tracking which options we saw
	tuBool		gotTimeType;
	tuBool		gotTimeTypeFromResource;
	tuBool		gotInterfaceFromResource;
	tuBool		recording;  // we're recording history into file
	tuBool		history;    // we're replaying history from file
	tuBool		outputAscii; // output ascii "history"
	SnoopStream	snpStm;

	dataStruct2*   freeDataList;

	struct {
	    char  expr[255]; // the filter expression
	    int	  used;	    // how many times this is used
	}		filters[HIST_MAXBINS];
	
		
	tuBool		postedFatal;
	int		errs;
	char*		errStrings[MAXERRS];

	void		collectErrs(int msg, char *errString = NULL,
			    int lineNumber = 0);

	void		undoIllegalOptions();
	int		writeFile(char*);
	void		buildOptLine(char*, tuBool);

	int		setupStripTkns(char*, char*[], int,
			    StripGadget*, tuBool);

	void		finishSetupStripInts(char* filtExpr, int typeInt,
			    int baseRate, int colorInt, int avgColorInt,
			    int styleInt, tuBool alarmSet, tuBool alarmBell,
			    float alarmLo, float alarmHi, StripGadget* sg);

	int		matchToken(char*, int*, int*, int*,
			    tuBool*, tuBool*, float*);

	int		useNewTimeArgs(int, int, int, int, int, tuBool);
	int		checkTimeArgs(int*, int*, int*, int*, int*, int*);
	int		setupCircData(dataStruct**, int);

	int		snoopStart(SnoopStream* streamp);
	int		snoopStop(SnoopStream*  streamp);
	
	int		restartSnooping(int);
	int		handleHistFile(char*);
	void		historyMove(tuGadget*);
	void		updateHistory();
	void		startTimer(void);
	void		readData(tuGadget*);
	void		drawData(void);
	void		checkAlarms(StripGadget*, float, struct timeval);
	void		sendAlarmEvent(int alarmType, StripGadget* sg, float value);
	void		stopUsingBin(int);

};

#endif /* __netGraph_h_ */
