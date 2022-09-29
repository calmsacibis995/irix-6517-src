#ifndef _EV_CFG_H_
#define _EV_CFG_H_
/*
 * cfg.h - Class definition for EV_cfg
 * 
 * $Revision: 1.2 $
 * 
 * This class is used by the server to determine how to dispose of incoming
 * events. Each event is configured to have an alarm level, whether or
 * not the application should send it at all, what action(s) the server should
 * take upon receipt, and a destination ID used to look up which other apps are
 * interested in the event. The configuration info is in EV_CFG_FILE.
 */
 
/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
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


#include "event.h"

#define EV_CFG_FILE "/usr/lib/netvis/eventcfgrc"
#define CFG_FILE_MAX_LINE 80
#define OPT_EVENT "event"
#define OPT_LOGGING "logging"
#define OPT_LEN 32
#define OPEN_READ "r"

class EV_cfgRecord {
friend class EV_cfg;
private:
    EV_level lvl;
    int noSnd;
    int actID;
    int dstID;
}; 

class EV_cfg {
public:
    EV_cfg (void);
    ~EV_cfg (void);
    inline EV_level getAlarmLevel(eventID id);  
    inline int getNoSend(eventID type);  
    inline int getActID(eventID type);  
    inline int getDstID(eventID type);
    inline int getLogSize(void);
    inline int getNumLogs(void);
    inline char *getLogName(void);
private:
    EV_cfgRecord cfgTbl[NV_MAX_EVENT + 1];
    void initCfgTbl(void);
    void badCfgFile(eventID expect, eventID found);
    int logSize;
    int numLogs;
    char *logName;
};

EV_level
EV_cfg::getAlarmLevel (eventID id) {
    return (cfgTbl[id].lvl);
}

int
EV_cfg::getNoSend (eventID id) {
    return (cfgTbl[id].noSnd);
}

int
EV_cfg::getActID (eventID id) {
    return (cfgTbl[id].actID);
}

int
EV_cfg::getDstID (eventID id) {
    return (cfgTbl[id].dstID);
}

int
EV_cfg::getLogSize(void) {
    return (logSize);
}

int
EV_cfg::getNumLogs(void) {
    return (numLogs);
}

char *
EV_cfg::getLogName(void) {
    return (logName);
}

#endif
