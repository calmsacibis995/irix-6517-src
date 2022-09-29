/* server.c++  - implentation of the server classe for the nveventd
 * 
 *	NetVisualyzer Event Server
 *
 *	$Revision: 1.7 $
 *
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

#include <sys/types.h>
#include <sys/syslog.h>
#include <errno.h>
#include <bstring.h>
#include <stdio.h>
#include <osfcn.h>
#include <syslog.h>

#include "event.h"
#include "daemonName.h"
#include "server.h"
#include "logger.h"
#include "sessctrl.h"
#include "cfg.h"

extern "C" {
    void syslog (int p, const char *m, ...);   
};

// pointer to class that tells how to handle each received event
extern EV_cfg *cfg;	

/*
 * Initialize the server. Instatiated in main before cmdline arg processing
 */

EV_server::EV_server(void) { 
    int i; 
    openlog(eventDaemon, LOG_PID | LOG_CONS, LOG_USER);
    cfg = new EV_cfg;
    dumpInMsg = dumpOutMsg = debugLevel = foreground = 0;
    for (i = 0; i <= NV_MAX_EVENT; i++) {
	noSendList[i] = cfg->getNoSend(i);
    }
    stats.registerReqs = 0;
    stats.registerResps = 0;
    stats.eventsIn = 0;
    stats.eventsOut = 0;
    stats.eventsLogged = 0;
    stats.eventsUnknown = 0;
    logsize = (cfg->getLogSize()) ? cfg->getLogSize() : EV_LOG_SIZE;
    numlogs = (cfg->getNumLogs()) ? cfg->getNumLogs() : EV_NUM_LOGS;
    logfile = strdup(cfg->getLogName());
    if (logfile[0] == NULL) {
	delete logfile;
	logfile = strdup(EV_LOGFILE_NAME);
    }
	 
    dumpfd = stderr;
}

EV_server::~EV_server(void) {
    
}

/*
 * Run the server. This is the main dispatch loop for the event daemon
 */
void
EV_server::run(void) {    
    EV_stat rslt;
    int ready;
    int logit = 0;
    
    // start logging. Always do this. Not a cfg option.
    EV_logger logger(logsize, numlogs, logfile);
    if (logger.openLog() == EV_OK) {
	logit = 1;
    }
    
    // get and fd for client/server event communication
    eh = new EV_handler(&evfd, eventDaemon); 
    // tell the handler which events to tell the client NOT to send
    eh->setNoSends(noSendList);
    // tell the world I've started.
    syslog (LOG_DEBUG, " starting.");
    
    sessCtrl	*ctrl = new (sessCtrl);
    
    // XXXX what do I do if I can't get an evfd?
    
    // run forever processing incoming messages. Each is handled completely
    // before getting the next.
    for ( ; ; ) {
	FD_ZERO (&fds);
	FD_SET(evfd, &fds);
	ready = select (FD_SETSIZE,  &fds, 0, 0, 0);
	FD_CLR(evfd,  &fds);
	if (ready < 0 ) {
	    if (errno != EINTR) {
		//log a real error
		syslog (LOG_ERR, "EV_server::run select: %m");
	    }
	    continue;
	}
	else if (ready == 0){ // nothing there
	    continue;
	}
	
	ep = new EV_event;  // get space to receive the data.
	rslt = eh->receive(ep);
	
	if (rslt !=  EV_OK) {
	    syslog (LOG_ERR, "EV_server::run EV_handler::receive error %d",
		     rslt);
	    delete ep;
	    continue;
	}
	
	//now have the event.
	if (dumpInMsg) {
	    fprintf (stderr, "nveventd: in message:\n");
	    ep->dump(dumpfd);
	}
	stats.eventsIn++;
	
	
	// here I  dispatch events to the various subservers based on 
	// What they do is based on the EV_cfg class
	if (ep->getType() != NV_REGISTER) {
	    logger.logit(ep);
	    // action.doit(ep);
	}
	else { // reflect back the event with the noSendList to the client
	    eh->send(ep);
	    if (dumpOutMsg) {
		fprintf (stderr, "nveventd: out message:\n");
		ep->dump(dumpfd);
	    }
	    ctrl->doCtrl(ep);
	}
	
	delete ep;
    }

}

