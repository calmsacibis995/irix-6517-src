/* sessctrl.c++  - keep track of applications connected and terminate
 *		   when there are none.
 *
 * 
 *	NetVisualyzer Event Server
 *
 *	$Revision: 1.2 $
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
#include <unistd.h>
#include <errno.h>
#include <bstring.h>
#include <stdio.h>
#include <osfcn.h>
#include <syslog.h>
#include <signal.h>

#include "event.h"
#include "sessctrl.h"
#include "proctbl.h"

#define D_PRINTF(stuff) \
    if (EV_debugging) fprintf stuff;

extern int EV_debugging;
extern void done (int sig, ...);    //common exit for nveventd
procTable   *pidTbl;		    //keeps track of the nveventd clients
unsigned int	sessCtrlTime;	    //inactivity time

/*
 * checkClients handles SIGALRM. Every time it goes off, it checks to see
 * if there are any netvis clients of eventd still alive. If not it terminates
 * nveventd
 */
 
void checkClients(int sig, ...) {
    if (pidTbl->scan() == 0) {	    //Any clients
	done(SIGALRM);		    // No. We're done
    }
    else {				//yes. 
	(void) alarm(sessCtrlTime);	//reset the alarm 
	signal (SIGALRM, checkClients);	//and set up to catch it
    }    
}

/*
 * sessCtrl constructor.
 */
sessCtrl::sessCtrl(unsigned int timeout) {
    sessCtrlTime = timeout;	    // set inactivity/check timer 
    pidTbl = new (procTable);	    // create the process table of clients
    signal (SIGALRM, checkClients); // set up to catch the timer signal
}

/*
 * doCtrl - called every time a new client connects with nveventd
 */
void
sessCtrl::doCtrl(EV_event *ep) {
    if (ep->getType() != NV_REGISTER) {	// paranoid checking
	D_PRINTF ((stderr,
		"sessCtrl::doCtrl: expected NV_REGISTER event. got %d\n", 
		ep->getType()));
	return;	
    }
    (void) alarm ((unsigned int) 0);	// cancel the inactivity timer
    D_PRINTF ((stderr, 
	"sessCtrl::doCtrl: Got REGISTER event from %s(%d).\n", 
	 ep->getSrcApp(),  ep->getPID()));
    pidTbl->add(ep->getPID());		//add this client to the list
    (void) alarm (sessCtrlTime);	//reset to check on clients
}


