/*
 * cfg.c++ -- implementation of classes that determine what the server does
 *	       with the events it receives
 *
 * $Revision: 1.3 $
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
 
#include <syslog.h>
#include <stdio.h>
#include <strings.h>

#include "event.h"
#include "cfg.h"

extern "C" {
    void syslog (int p, const char *m, ...); 
}

EV_cfg::EV_cfg (void) {
    char optbuf[OPT_LEN + 1];
    char buf[CFG_FILE_MAX_LINE];
    FILE *cf;
    char *str = NULL;
    eventID evType;
    
    logSize = 0;
    numLogs = 0;
    logName = new char[CFG_FILE_MAX_LINE];
    logName[0] = NULL;
    
    initCfgTbl();
     
    cf = fopen(EV_CFG_FILE, OPEN_READ);
    if (cf == (FILE *) NULL) {
	syslog(LOG_ERR, "EV_cfg::EV_cfg: fopen: %m");
	return;
    }
    eventID id = 0;
    /*
     * At this point read in the option lines. XXXX
     * Currently the loop below expects a very rigid format.
     * All the events must be in the file in order. However, there can be
     * other option lines interspersed. But for now there are no others.
     * The string OPT_EVENT must be left justified on each line.
     */
    while ((str = fgets(buf, CFG_FILE_MAX_LINE, cf)) != NULL) {
	if (buf[0] == '#') {
	    continue;
	}
	if (strncmp(buf, OPT_EVENT, strlen(OPT_EVENT)) == 0)  {
	    sscanf(buf, "%s %d %d %d %d %d", 
		    optbuf, 
		    &evType, 
		    &cfgTbl[id].lvl, 
		    &cfgTbl[id].noSnd, 
		    &cfgTbl[id].actID, 
		    &cfgTbl[id].dstID );
	    if (id != evType) {		// requires all events in order
		badCfgFile(id, evType);
		break;
	    }
	    id++; 
	}
	else if (strncmp(buf, OPT_LOGGING, strlen(OPT_LOGGING)) == 0) {
	    sscanf(buf, "%s %d %d %s", 
		    optbuf, &logSize, &numLogs, logName);
	    
	}
    }

}

void
EV_cfg::badCfgFile(eventID expect, eventID found) {
    syslog (LOG_ERR, 
	    "EV_cfg::EV_cfg: bad %s file format at event %d. Found %d.",
	    EV_CFG_FILE, (int) expect, (int) found);
    initCfgTbl();

}

void
EV_cfg::initCfgTbl(void) {    
    int i;
    for ( i = 0; i < NV_MAX_EVENT; i++) {
	 cfgTbl[i].lvl = EV_LEVEL_INFO;
	 cfgTbl[i].noSnd = 0;
	 cfgTbl[i].actID = 0;
	 cfgTbl[i].dstID = 0;
    }
    
}
