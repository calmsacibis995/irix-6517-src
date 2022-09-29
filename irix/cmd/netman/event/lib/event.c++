/*
 * event.c++ - The user visible portion of the netvis event library. 
 *
 * The primary classes are EV_event and EV_handler. This library is used
 * both by the event server and and client.
 *               
 * $Revision: 1.9 $
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


#include <stdio.h> 
#include <unistd.h>
#include <sys/types.h>	    // for uint, time_t
#include <string.h>
#include <pwd.h>
#include <sys/param.h>	    //for MAXHOSTNAMELEN
#include <osfcn.h>	    //for gethostname
#include <libc.h>	    //for bzero
#include <time.h>	    //for cftime
#include <errno.h>	    //for errno

#include "event.h"
#include "daemonName.h"
#include "msg.h"

#ifdef EV_DEBUG
int EV_debugging = 1;
#else
int EV_debugging = 0;
#endif

static int amEventServer = 0;

#define D_PRINTF(stuff) \
    if (EV_debugging) fprintf stuff;
    
/*
 * The following classed taken together along with the EV_event class totally
 * define the information in a netvis event.
 */
    
/*
 * The application class is used by the library to determine whether it is
 * operating on behalf of the client or server.
 */
EV_application::EV_application (void) {
    struct passwd *pwdent;
    appName = NULL;
    userName = NULL;
    pid = getpid();
    uid = getuid();
    pwdent = getpwuid(uid);
    if (pwdent && pwdent->pw_name)
	userName = strdup(pwdent->pw_name);
}

EV_application::~EV_application(void) {
    delete [] appName;
    delete [] userName;
}

EV_application::EV_application(char *n) {
    setAppName(n);
}

void
EV_application::setAppName (char *n) {
    if (appName) {		// already pointing to a name	
	delete [] appName;	// deallocate that space
    }
    if (n) {			// make sure a name is passed
	appName = strdup (n);	// allocate and copy
    }
    else {
	appName = NULL;
    }
}

void
EV_application::setUserName (char *n) {
    if (userName) {		// already pointing to a name	
	delete [] userName;	// deallocate that space
    }
    if (n) {			// make sure a name is passed
	userName = strdup (n);	// allocate and copy
    }
    else {
	userName = NULL;
    }
}

char *
EV_application::getAppName(void) {
    return (appName);
}

char *
EV_application::getUserName(void) {
    return (userName);
}

/*
 * The EV_data class contains the snoop filter and a place for arbitrary opaque
 * user data. All events contain this info.
 */
EV_data::EV_data (void) {
    filter = NULL;
    otherData = NULL;    
}

EV_data::~EV_data(void) {
    delete filter;
    delete otherData;
}


void
EV_data::setFilter (char *f) {
    if (filter) {		// already pointing to a filter
	delete [] filter;	// deallocate that space
    }		
    if (f) {			// make sure use supplied a filter
	filter = strdup (f);	// allocate and copy
    }
    else {
	filter = NULL;
    }
}

void
EV_data::setOtherData (char *o) {	
    if (otherData) {	    // already pointing to data
	delete [] otherData;    // deallocate old space
    }
    if (o) {			    // make sure user supplied data	
	otherData = strdup (o);	    // allocate and copy
    }
    else {
	otherData = NULL;
    }
}



char *
EV_data::getFilter() {
    return (filter);
}

char *
EV_data::getOtherData() {
    return (otherData);
}

/*
 * This class defines the detection of network object or a protocol or a
 * converstaion between two network objects.
 */
EV_obj_detect::EV_obj_detect() {
    protocol = NULL;
    objType = OBJ_OTHER;
    converseBytes = 0;
    conversePkts = 0;
}

EV_obj_detect::EV_obj_detect (objectType obj) {
    objType = obj;
    protocol = NULL;
    converseBytes = 0;
    conversePkts = 0;
}

void
EV_obj_detect::setProto(char *p) {
    if (protocol) {		    // already pointing to protocol
	delete [] protocol;    // deallocate old space
    }
    if (p) {			    // make sure user supplied protocol	
	protocol = strdup (p);	    // allocate and copy
    }
    else {
	protocol = NULL;
    }
}

char *
EV_obj_detect::getProto () {
    return (protocol);
}

/*
 * EV_rate defines the fundamental information if a rate event.
 */
EV_rate::EV_rate() {
    rate = 0.0;
    thresholdRate = 0.0;
    base = BYTE_BASED;
    baseRate = 0.0;
    topNList = NULL;
}

EV_rate::EV_rate(float r,  float t,  rateBase b, float bofrate,  objectList *olist) {
    rate = r;
    thresholdRate = t;
    base = b;
    baseRate = bofrate;
    // copy object list
    //XXXX topNList = olist; temporarily user the line below
    topNList = olist;
}

EV_rate::~EV_rate(void) {
   //delete topNList;
}
void
EV_rate::setTopNList ( objectList *olist) {
    // copy the list
    // topNList = olist;
    // XXXX temporarily use the line below
    topNList = olist;
}

/*
 * The EV_event class defines the direct user API for constructing an event
 * and retrieving data from it.
 * 
 * There is a different constructor for each general category of event.
 */
 
EV_event::EV_event(void) {
   evType =  NV_OTHER;
   srcApp = NULL;
   pid = 0;
   userName = NULL;
   uid = 32767;
   alarmLevel = EV_LEVEL_INFO;
   timeSent = 0;
}

/*
 * constructor for the basic event with no additional info
 */
EV_event::EV_event(const eventID id, EV_objID *intrfce, 
		    char *flt) {
    evType = id;
    srcApp = NULL;
    pid = 0;
    userName = NULL;
    uid = 32767;
    alarmLevel = EV_LEVEL_INFO;
    timeSent = 0;
    if (intrfce) {
	interface.setName(intrfce->getName());
	interface.setAddr(intrfce->getAddr());
    }
    setFilter(flt);
}

/*
 * constructor for the basic event with no additional info.
 * allow user to specify interface as a string rather than EV_objId
 */

EV_event::EV_event(const eventID id, char *intrfce, 
		    char *flt) {
    evType = id;
    srcApp = NULL;
    pid = 0;
    userName = NULL;
    uid = 32767;
    alarmLevel = EV_LEVEL_INFO;
    timeSent = 0;
    interface.setName(intrfce);
    setFilter(flt);
}

/*
 * Constructor for object detection events
 */
EV_event::EV_event(const eventID id, objectType obj,
		    EV_objID *intrfce, char *flt, EV_objID *nd,
		    EV_objID *network, EV_objID *n1, EV_objID *n2,
		    uint bytes, uint pkts, char *proto) {
    evType = id;
    srcApp = NULL;
    pid = 0;
    userName = NULL;
    uid =  32767;
    alarmLevel = EV_LEVEL_INFO;
    timeSent = 0;
    objType = obj;
    if (intrfce) {
	interface.setName(intrfce->getName());
	interface.setAddr(intrfce->getAddr());
    }
    setFilter(flt);
    if (nd) {
	node.setName(nd->getName());
	node.setAddr(nd->getAddr());
	node.setMACAddr(nd->getMACAddr());
    }
    if (network) {
	net.setName(network->getName());
	net.setAddr(network->getAddr());
    }
    /*
     * XXXX should make sure that the object type is a OBJ_CONVERSE 
     * and reconcile this with n1 and n2 parameters. They don't make sense 
     * otherwise
     * 
     */
    if (n1) {
	pair1.setName(n1->getName());
	pair1.setAddr(n1->getAddr());
	pair1.setMACAddr(n1->getMACAddr());
    }
    if (n2) {
	pair2.setName(n2->getName());
	pair2.setAddr(n2->getAddr());
	pair2.setMACAddr(n2->getMACAddr());
    }
    converseBytes = bytes;
    conversePkts = pkts;
    setProto(proto);
}

/*
 * Constructor for object detection events.
 * Allows the user to specify strings rather than EV_objIDs
 */

EV_event::EV_event(const eventID id, objectType obj,
		    char *intrfce, char *flt, char *nd,
		    char *network, char *n1, char *n2,
		    uint bytes, uint pkts, char *proto) {
    evType = id;
    srcApp = NULL;
    pid = 0;
    userName = NULL;
    uid = 32767;
    alarmLevel = EV_LEVEL_INFO;
    timeSent = 0;
    objType = obj;
    interface.setName(intrfce);
    setFilter(flt);
    node.setName(nd);
    net.setName(network);
    /*
     * XXXX should make sure that the object type is a OBJ_CONVERSE 
     * and reconcile this with n1 and n2 parameters. They don't make sense 
     * otherwise
     * 
     */
    pair1.setName(n1);
    pair2.setName(n2);
    converseBytes = bytes;
    conversePkts = pkts;
    setProto(proto);
}

/*
 * Rate event constructor
 */
EV_event::EV_event(const eventID id, float rt, float thresh, 
		    rateBase rbase, rateType bofrate, 
		    EV_objID *intrfce, char *flt,
		    objectList *olist) 
{
    evType =id;
    srcApp = NULL;
    userName = NULL;
    uid = 32767;
    alarmLevel = EV_LEVEL_INFO;
    timeSent = 0;
    base = rbase;
    if (intrfce) {
	interface.setName(intrfce->getName());
	interface.setAddr(intrfce->getAddr());
    }
    setFilter(flt);
    setRate(rt);
    setThreshRate(thresh);
    setRateOfBase(bofrate);
    setTopNList(olist);
}

/*
 * Rate event constructor.
 * Allows user to specify strings rather than EV_objIDs
 */

EV_event::EV_event(const eventID id, float rt, float thresh, 
		    rateBase rbase, rateType bofrate, 
		    char *intrfce, char *flt,
		    objectList *olist) 
{
    evType =id;
    srcApp = NULL;
    userName = NULL;
    uid = 32767;
    alarmLevel = EV_LEVEL_INFO;
    timeSent = 0;
    base = rbase;
    interface.setName(intrfce);
    setFilter(flt);
    setRate(rt);
    setThreshRate(thresh);
    setRateOfBase(bofrate);
    setTopNList(olist);
}

EV_event::~EV_event() {
    delete [] srcApp;
    delete [] userName;
    
}

void
EV_event::setSrcApp (char *n) {
    if (srcApp) {		// already pointing to a name	
	delete [] srcApp;	// deallocate that space
    }
    if (n) {			// make sure a name is passed
	srcApp = strdup (n);	// allocate and copy
    }
    else {
	srcApp = NULL;
    }
}

void
EV_event::setUserName (char *n) {
    if (userName) {		// already pointing to a name	
	delete [] userName;	// deallocate that space
    }
    if (n) {			// make sure a name is passed
	userName = strdup (n);	// allocate and copy
    }
    else {
	userName = NULL;
    }
}

void
EV_event::setNodeName (char *n) {
    node.setName(n);
}

void
EV_event::setNodeAddr (char *a) {
    node.setAddr(a);
}

void
EV_event::setNodeMACAddr (char *m) {
    node.setMACAddr(m);
}

void
EV_event::setNetName (char *n) {
    net.setName(n);
}

void
EV_event::setNetAddr (char *a) {
    net.setAddr(a);
}

void
EV_event::setInterfaceName (char *n) {
    interface.setName(n);
}

void
EV_event::setInterfaceAddr (char *a) {
    interface.setAddr(a);
}

void
EV_event::setEndPt1Name (char *n) {
    pair1.setName(n);
}

void
EV_event::setEndPt1Addr (char *a) {
    pair1.setAddr(a);
}

void
EV_event::setEndPt1MACAddr (char *m) {
    pair1.setMACAddr(m);
}

void
EV_event::setEndPt2Name (char *n) {
    pair2.setName(n);
}

void
EV_event::setEndPt2Addr (char *a) {
    pair2.setAddr(a);
}

void
EV_event::setEndPt2MACAddr (char *m) {
    pair2.setMACAddr(m);
}


char *
EV_event::getSrcApp(void) {
    return (srcApp);
}

char *
EV_event::getUserName(void) {
    return (userName);
}

char *
EV_event::getSrcHostName (void) {
    return (srcHost.getName());
}

char *
EV_event::getSrcHostAddr (void) {
    return (srcHost.getAddr());
}

char *
EV_event::getNodeName () {
    return(node.getName());
}

char *
EV_event::getNodeAddr () {
    return(node.getAddr());
}

char *
EV_event::getNodeMACAddr () {
    return(node.getMACAddr());
}

char *
EV_event::getNetName () {
    return(net.getName());
}

char *
EV_event::getNetAddr () {
    return(net.getAddr());
}

char *
EV_event::getInterfaceName () {
    return(interface.getName());
}

char *
EV_event::getInterfaceAddr () {
    return(interface.getAddr());
}

char *
EV_event::getEndPt1Name () {
    return(pair1.getName());
}

char *
EV_event::getEndPt1Addr () {
    return(pair1.getAddr());
}

char *
EV_event::getEndPt1MACAddr () {
    return(pair1.getMACAddr());
}

char *
EV_event::getEndPt2Name () {
    return(pair2.getName());
}

char *
EV_event::getEndPt2Addr () {
    return(pair2.getAddr());
}

char *
EV_event::getEndPt2MACAddr () {
    return(pair2.getMACAddr());
}

/*
 * 
  is used during debugging. The user can tell the event to print itself.
 */
void EV_event::dump( FILE *fd) {
    char tbuf[28];
    EV_timeStamp ts;
    ts = timeSent;
    cftime (tbuf, "%x %X", &ts);
    
    fprintf(fd, "\nEVENT DUMP:\n");
    fprintf(fd, "\tCOMMON type %d, timeStamp '%s',  srcApp '%s', \
app pid %d, user %s, uid %d, srcHost.name '%s', \
srcHost.addr '%s', filter '%s', interface.name '%s', interface.addr '%s', \
otherData '%s'\n", 
	    evType, tbuf,  
            srcApp, pid, userName, uid, srcHost.getName(), srcHost.getAddr(), filter, 
            interface.getName(), interface.getAddr(),
            otherData);
    fprintf(fd, "\tDETECT objType %d\n", objType);
fprintf(fd, "node.name, %s, node.addr %s, node.MACAddr %s \
net.name %s, net.addr %s, pair1.name '%s', pair1.addr '%s', \
pair1.MACAddr '%s', pair2.name '%s', pair2.addr '%s', \
pair2.MACAddr '%s', bytes %d, pkts %d\n", 
            node.getName(), node.getAddr(), node.getMACAddr(), net.getName(),
	    net.getAddr(), 
	    pair1.getName(), pair1.getAddr(), pair1.getMACAddr(), 
	    pair2.getName(), pair2.getAddr(), pair2.getMACAddr(), 
            converseBytes, conversePkts);
    fprintf(fd, "\tRATE rate %f, threshold %f, base %d, baseRate %f, \
topNList '%x'\n", rate, thresholdRate, base, baseRate, topNList);
    if (topNList) {
	    int i = 0;
	    while (topNList[i]) {
	     fprintf(fd, "topN%d %s ", i, topNList[i]);
	     i++;
	    }
	    fprintf(fd, "\n");
	}

     
}

/* EV_handler class is instantiated by the application to start up the event
 * handling engine.
 * 
 * Inintialize the static class variable to make sure there is never more that
 * one handler per application.
 */
EV_handle EV_handler::handle = NULL; // initialize to show no connection

/*
 * The EV_handler constructor is repsonsible for establishing/initializing the
 * communications channel. The most important thing that it does on behalf of
 * the user is to exchange configuration information between the client and
 * server. In particular,  if being used by the client,  it asks the server for
 * the list of events not to send because they are not currently configured 
 * as being of interest or because there are just too many of them.
 * 
 * XXXX currently the handler sends a request and then waits for a reply.
 *      In the future it will do this asynchronously so that if server
 *      communication fails,  it will not wait for the timeout.
 * 
 */
EV_handler::EV_handler (EV_handle *h, char *app, struct timeval *t) {
    fd_set fds;
    int ready;
    struct timeval timeout;
    EV_stat rslt = -1;
    
#define   EV_REG_MSG_TIMEOUT	2  

    if (handle) return; // protect user from duplicating connection to server
    bzero(regList, sizeof regList);
    bzero (noSendList, sizeof noSendList);
    setAppName (app);  // This must be passed by user
    if (strcmp (app, eventDaemon) == 0) {
	amEventServer = 1;
	msg =  new EV_srvr_msg();
    }
    else {
	msg = new EV_clnt_msg();
    }
    *h = handle = msg->open(/*XXXX session?*/);

    if (handle <= 0) {
	*h = handle = NULL;
	D_PRINTF((stderr,
	"EV_handler::EV_handler: cannot open communication channel.\n"));
	return;	    
    }
    if ( !amEventServer) {
	// send the initial registration message to the event daemon
	// This is the pseudo event
        EV_event *e = new EV_event(NV_REGISTER);
            
	    
	// EV_handler::send normally would do this for all my users	
	// here I do it directly for myself.
		
	e->setSrcApp (getAppName());
	e->pid = getPID();
	e->uid = (int) getUID();
	e->setUserName (getUserName());
	e->srcHost.setName(localHost.getName());
	e->timeSent = time ((time_t *)NULL);
	    
	msg->send(e);
	int iter = 0;
	// set up and wait for a reply
	while (rslt && iter < 3) {
	    D_PRINTF((stderr, "EV_handler::EV_handler iter %d\n", iter));
	    iter++;
	    FD_ZERO (&fds);
	    FD_SET (handle, &fds);
	    timeout.tv_sec = (t) ? t->tv_sec : EV_REG_MSG_TIMEOUT;
	    timeout.tv_usec = (t) ? t->tv_usec : 0;
		   
	    ready = select (FD_SETSIZE, &fds, 0, 0, &timeout);
	    if (ready == -1) {
		D_PRINTF((stderr, "EV_handler: select: %d\n", errno));
		handle = NULL;
		break;
	    }
	    else if (! ready) {
		D_PRINTF((stderr, "EV_handler: timed out connecting to server\n"));
		handle = NULL;
		continue;
	    }
	    else {
		// the reply should tell me what events not to send.
		// This is the only time that receive has this extra 
		// parameter. This is the EV_handler noSendList. See
		 // EV_handler_send for its use.
			
		rslt = msg->recv(e, noSendList);
	    } 
	}
	if (rslt != EV_OK) 
	    handle = NULL;
	delete e;
    }
}
	

EV_handler::~EV_handler(void) {

    delete msg;
}

/*
 * These methods are used by a client to (un)register for events of interest.
 * It is not implemented in the first release.
 */
void
EV_handler::reg (const eventID event) {
    regList[event] = 1;
}

void
EV_handler::unregister (const eventID event) {
    regList[event] = 0;
}


EV_stat 
EV_handler::send (EV_event *event) {
    /*
     * I have the event and I have the localHost and application from the
     * handler. What I do is construct the event msg and send it on. 
     * 
     */
    
    char noSnds[NV_MAX_EVENT + 1];
    int cnt = 0;
    
    // Check to see if comm channel is open.
    if (!handle) {
	return (EV_OK);
    }
    
    // clients only send events that are not proscibed
    if (noSendList[event->evType] && !amEventServer) { 
        // configured not to send this one
	return (EV_OK);
    }
    if (amEventServer && event->evType == NV_REGISTER) { 
	// fill in list of events not to send
	// pack the list into minimum space for the message
	// note that this is a reply and therfore the client's message
	// is reflected back unchanged
	for (int i = 0; i < NV_MAX_EVENT; i++) {
	    if (noSendList[i]) {
		noSnds[cnt++] = i;
	    }
	} 
	// 0 terminate the list
	noSnds[cnt] = 0;   
	D_PRINTF ((stderr,  "EV_handler::send replying with cnt %d noSends\n",
		   cnt));
    }
    else {
	// standard event send where the handler fills in this default info
	event->setSrcApp (getAppName());
	event->pid = getPID();
	event->uid = (int) getUID();
	event->setUserName (getUserName());
	event->srcHost.setName(localHost.getName());
	event->timeSent = time ((time_t *)NULL);
    }
    return(msg->send(event, (cnt) ? noSnds : NULL));
}

EV_stat
EV_handler::receive (EV_event *event) {
    /*
     * XXXX Should this routine be generalized to use the noSendList as the
     * optional second parameter to msg::receive so that EV_handler::EV_handler
     * does not special case the initial NV_REGISTER event?
     */
    if (! handle) {
	return (EV_OK);
    }
    
    return (msg->recv(event));
}

/*
 * This method is used by the server to set the noSendList of the handler to a
 * given configuration.
 */
void EV_handler::setNoSends(char *noSends) {
    if (noSends) {
	bcopy (noSends, noSendList,  NV_MAX_EVENT);
    }
}
