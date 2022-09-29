/* -*- C++ -*- */

#ifndef _INV_RECORD_H_
#define _INV_RECORD_H_

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

#ident "$Id: Record.h,v 1.6 1997/12/12 04:35:41 markgw Exp $"

#include <stdio.h>
#include <X11/Intrinsic.h>
#include "Bool.h"
#include "String.h"
#include "List.h"

class VkCallbackObject;

class INV_Record;

struct INV_RecordHost {
public:

    OMC_String		_host;
    OMC_StrList		_metrics;
    OMC_StrList		_once;
    pmRecordHost	*_rhp;
    INV_Record		*_record;
    XtInputId		_id;

    ~INV_RecordHost();
    INV_RecordHost(const char *host);

    friend ostream& operator<<(ostream &os, const INV_RecordHost &host);

private:

    INV_RecordHost(const INV_RecordHost &);
    const INV_RecordHost &operator=(const INV_RecordHost &);
};

typedef OMC_List<INV_RecordHost *> INV_RecordList;

class INV_Record {
public:

    friend struct INV_RecordHost;

    typedef OMC_Bool	(*ToolConfigCB)(FILE *);
    typedef void	(*LogConfigCB)(INV_Record &);
    typedef void	(*StateCB)(void *);
    
private:

    OMC_Bool		_active;
    OMC_Bool		_modConfig;
    ToolConfigCB	_toolConfigCB;	// Does the tool need a config file?
    LogConfigCB		_logConfigCB;	// App can add metrics to log config
    StateCB		_stateCB;	// CB if all loggers have terminated
    void		*_stateData;	// State data to be passed back
    INV_RecordList	_list;
    uint_t		_lastHost;
    uint_t		_numActive;
    OMC_String		_allConfig;	// Use this file as the config
    OMC_String		_addConfig;	// Add this file to the config

public:

    ~INV_Record();

    INV_Record(ToolConfigCB toolCB = NULL,
	       OMC_Bool modConfig = OMC_true,
	       LogConfigCB logCB = NULL,
	       StateCB stateCB = NULL,
	       void *stateData = NULL,
	       const char *allConfig = NULL,
	       const char *addConfig = NULL);

    OMC_Bool active() const
	{ return _active; }
    uint_t numHosts() const
	{ return _list.length(); }

    void add(const char *host, const char *metric);
    void addOnce(const char *host, const char *metric);

    // Note delta is in milliseconds
    void changeState(uint_t delta = 10);

    int dumpConfig(FILE *fp, const INV_RecordHost &host, uint_t delta);
    int dumpFile(FILE *fp);

    // Use this config only
    void setRecordConfig(const char *config)
	{ _allConfig = config; }

    // Add this config to the metrics in the scene
    void setRecAddConfig(const char *config)
	{ _addConfig = config; }

    friend ostream &operator<<(ostream &os, const INV_Record &rhs);

private:

    static void logFailCB(XtPointer clientData, int *, XtInputId *id);

    int checkspecial(char* p);
};

#endif
