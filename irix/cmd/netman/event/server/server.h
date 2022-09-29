/*
 * server.h -- class definition of the main event engine for the nveventd server
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

#include <strings.h>
#include <sys/types.h> 
#include "event.h"	// for EV_handler, EV_handle


struct eventStats {
    uint	registerReqs;
    uint	registerResps;
    uint	eventsIn;
    uint	eventsOut;
    uint	eventsLogged;
    uint	eventsUnknown;
};
 
class  EV_server {
public:
    EV_server(void);
    ~EV_server(void);
    inline  void setDumpIn (int d);
    inline  void setDumpOut (int d);
    inline  void setForeground (int d);
    inline  void setDebugLevel (int l );
    inline  void setLogFile (char *f);
    inline  void setNumLogs (int n);
    inline  void setLogSize (int s);
    void run(void);
private:
    EV_handle evfd;
    EV_handler *eh;
    EV_event *ep;
    struct eventStats stats;
    fd_set fds;
    FILE *dumpfd;
    int  dumpInMsg;
    int  dumpOutMsg;
    int  foreground;
    int  debugLevel;
    int  numlogs;
    int  logsize;
    char *logfile;
    char noSendList[NV_MAX_EVENT + 1];
    void cleanup(void);
};

// inline functions

void
EV_server::setDumpOut (int d) {
    dumpInMsg = d;
}
    
void 
EV_server::setDumpIn (int d) {
    dumpOutMsg = d;
}
    
void 
EV_server::setForeground (int d) {
    foreground = d;
}
    
void 
EV_server::setDebugLevel (int l) {
    debugLevel = l;
}
	
void
EV_server::setNumLogs (int n) {
    numlogs = n;
}

void EV_server::setLogSize (int s) {
    logsize = s;
}

void EV_server::setLogFile (char *f) {
	if (f) {
	    delete [strlen (logfile)] logfile;
	    logfile = strdup (f);
	}
}
