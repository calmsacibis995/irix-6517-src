/* -*- C++ -*- */

#ifndef _INV_VIEW_H_
#define _INV_VIEW_H_

/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: View.h,v 1.6 1997/12/12 04:39:30 markgw Exp $"

#include <stdio.h>
#include <unistd.h>

#include <Inventor/Xt/SoXt.h>
#include <Inventor/nodes/SoDrawStyle.h>

#include "pmapi.h"
#include "Args.h"
#include "Bool.h"
#include "Record.h"
#include "Window.h"

class INV_ModList;
class SoEventCallback;
class SoXtViewer;

class INV_View : public INV_Window
{
 public:

    enum RenderOptions { 
	nothing = 0, fetch = 1, metrics = 2, inventor = 4, metricLabel = 8, 
	timeLabel = 16, all = 31
    };

    typedef int (*SetupCB)();

 private:

    int				_sts;
    int				_argc;
    char			**_argv;
    OMC_String			_argFlags;
    OMC_Bool			_sourceFlag;
    OMC_Bool			_hostFlag;

    OMC_String			_launchVersion;
    OMC_Bool			_checkConfigFlag;
    OMC_String			_pmnsFile;

    SoSeparator			*_root;
    SoDrawStyle			*_drawStyle;

    OMC_String			_text;
    OMC_String			_prevText;

    int				_timeMode;
    int				_timeFD;
    OMC_String			_timePort;
    pmTime			_timeState;
    OMC_String			_timeZone;
    struct timeval		_interval;

    INV_Record			_record;

public:

    ~INV_View();
	      
    INV_View(int argc, char **argv,
	     // getopt flags based on theDefaultFlags
	     const OMC_String &flags,
	     // generate tool specific config for pmafm launch
	     INV_Record::ToolConfigCB toolCB = NULL,
	     // generate additional pmlogger configs
	     INV_Record::LogConfigCB logCB = NULL,
	     // monitoring from more than one source
	     OMC_Bool sourceFlag = OMC_true,
	     // monitor from more than one live host
	     OMC_Bool hostFlag = OMC_false,
	     // generate additional pmlogger config from modualted objects
	     OMC_Bool modConfig = OMC_true);

    int status() const
	{ return _sts; }

    const OMC_String &pmnsFile()
	{ return _pmnsFile; }
    OMC_Bool checkConfigOnly() const
	{ return _checkConfigFlag; }

    SoSeparator* root()
	{ return _root; }
    SoXtViewer *viewer();

//
// Manage and parse command line args
//

    int parseArgs();
    int parseConfig(SetupCB appCB);

//
// Launching other tools
//

    int launch(const OMC_Args& args);

//
// Show and update the scene
//

    OMC_Bool view(OMC_Bool showAxis = OMC_false,
		  float xAxis = 0.0, float yAxis = 0.0, float zAxis = 0.0, 
		  float angle = 0.0, float scale = 0.0);

    void render(RenderOptions options, time_t);

//
// view changes when dragging
//

    void hideView()
	{ _drawStyle->style.setValue(SoDrawStyle::LINES); }
    void showView()
	{ _drawStyle->style.setValue(SoDrawStyle::FILLED); }

//
// pmtime state information
//

    pmTime& timeState()
	{ return _timeState; }
    int timeFD() const
	{ return _timeFD; }
    int timeConnect(OMC_Bool reconnect = OMC_false);
    
//
// notifier when selection has changed
//

    static void selectionCB(INV_ModList *, OMC_Bool);

//
// Recording
//

    void record();
    void setRecordConfig(const char *config)
	{ _record.setRecordConfig(config); }
    void setRecAddConfig(const char *config)
	{ _record.setRecAddConfig(config); }
    
private:

    void createTextArea(Widget parent);
    void attachWidgets();

    static void updateCB(void *, SoSensor *);
    static void timeCommandCB(VkCallbackObject *obj, void *clientData, 
			      void *callData);
    void changeDir();

    static void recordStateCB(void *);
};

#endif
