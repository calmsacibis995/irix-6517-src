/*
 * msg.c++ - the class that implements event messaging between clients and the
 *           event daemon
 * 
 * This is ONLY place that understands the communication protocols and data
 * formats being used. Everyone else deals with the EV_event class. The only
 * user of the msg classes is the EV_handler class.
 * 
 * Note any changes to message definition of the communications between client
 * and server require changes in FIVE places:
 * 
 * the file nveventd_types (for ToolTalk type compiler)
 * the include files tteventdOps.h and tteventdOpStrs.h
 * and here in the two functions EV_msg::send and EV_msg::receiveEnd
 * 
 * Note also that the send and receive sides are split into client and server
 * versions of the classes.
 * 
 * $Revision: 1.11 $
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

extern "C" {
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libc.h>
#include <sys/time.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <tt_c.h>
}
#include <osfcn.h>
#include "event.h"
#include "ttEventdOpStrs.h"
#include "msg.h"

#define D_PRINTF(stuff) if (EV_debugging) fprintf stuff;

extern int EV_debugging;    // space allocated in event.c++
/*
 * All the initalization is for tooltalk communications
 */
EV_msg::EV_msg(void) {
    tterr = TT_OK;
    ttop = NULL; // the tooltalk operation
    ttreplymark = 0; // pointer to tt data for the NV_Register request/reply
    mark = 0;        // pointer to the tt data for incoming notice
    opnum = 0;	     // operation number - goes with the op which is a string
    m, msgin = 0;   // pointers to the tt message.
   
}

/*
 * close the tooltalk communications
 */
EV_msg::~EV_msg(void) {
    tt_close();
}

/*
 * open TT communications. Common code called by client and server opens.
 * Currently open forces all clients to use the same/default ttsession that
 * is running on the local host and defined by the localhost and X display 
 * ":0". In the future clients may be allowed to specify a session to join.
 * 
 * To avoid the annoying "sh: ttsession not found" message on systems that
 * have not installed tooltalk,  open checks for the package. This is only 
 * a problem in 4.0.5 and less releases as tooltalk will be bundled with the
 * OS with the next major release of Irix (4.1/5.0 whatever it is called).
 */
 
int
EV_msg::open (void /* XXXX session */) {

#define X_DISPLAY ":0"
#define TT_HOME_DIR "/usr/ToolTalk"
#define TT_SESSION_EXEC "/usr/sbin/ttsession"
    
    char *my_session;
    char *xhost = new  char[MAXHOSTNAMELEN + (sizeof X_DISPLAY) + 1];
    struct stat statbuf;
    
    /* check to see if ToolTalk is installed. This is to avoid those 
     * nasty sh: ttsession not found messages from the tt_open call.
     * Note the check requires both the ToolTalk directory structure
     * and ttsession to be in a known place.
     */
   
     
    if ((stat(TT_HOME_DIR, &statbuf) == -1) ||
	(stat(TT_SESSION_EXEC, &statbuf) == -1)) {
	return (-1);
    }
        
    if (gethostname( xhost,  MAXHOSTNAMELEN) != 0) {
	perror("EV_msg::open: gethostname");
    }
    else {
	strcat(xhost, X_DISPLAY);
    }
    my_session = tt_X_session(xhost);
    if ((ttstatus = tt_ptr_error(my_session)) != TT_OK) {
	fprintf(stderr, "EV_msg::open: tt_X_session: %s\n", 
		tt_status_message(ttstatus));
    }
    else {
	D_PRINTF((stderr, "EV_msg::open: using ttsession %s\n", my_session));
    }
    ttstatus = tt_default_session_set(my_session);
    D_PRINTF((stderr, "EV_msg::open: tt_default_session_set: %s\n", 
	    tt_status_message(ttstatus)));
       
    ttpid = tt_open();
    ttfd = tt_fd();
    
    return (EV_OK);  // can always do this. Subsequent functions handle errors
}

/* EV_msg::send
 * Send an event via tooltalk.
 * 
 * Note that the argument types, count, and order must be identical to those
 * used in EV_msg::receiveEnd AND with the nveventd_types file which defines
 * messages to tooltalk. Also the ttEventdOps.h and ttEventdOpStrs.h files must
 * be kept in sync with these functions.
 * Changes must be kept in sync in all five places.
 * 
 * This is common code used by both the client and server side.
 */
int
EV_msg::send(EV_event *ep, char *noSends) {

    int retval = EV_OK;

    D_PRINTF((stderr, "EV_msg::send ENTRY with event:\n"));
    
    if (EV_debugging) ep->dump(stderr);
	 
	 /*
	  * XXXXX The highlight events will be added
	  */
	    
    
    int argc = 0;
    
    //first set the common data shared by all events
      
    tt_message_iarg_add(m, TT_IN, "int", (int) ep->evType); argc++; 
    tt_message_iarg_add(m, TT_IN, "int", (int) ep->timeSent); argc++;
    tt_message_iarg_add(m, TT_IN, "int", (int) ep->alarmLevel); argc++;
    tt_message_arg_add(m, TT_IN,  "string", ep->getSrcApp()); argc++;
    tt_message_iarg_add(m, TT_IN, "int", (int) ep->pid); argc++;
    tt_message_arg_add(m, TT_IN, "string", ep->getUserName()); argc++;
    tt_message_iarg_add (m, TT_IN, "int", (int) ep->uid);
    tt_message_arg_add(m, TT_IN,  "string", ep->srcHost.name); argc++;
    tt_message_arg_add(m, TT_IN,  "string", ep->srcHost.addr); argc++;
    tt_message_arg_add(m, TT_IN,  "string", ep->filter); argc++;
    tt_message_arg_add(m, TT_IN,  "string", ep->interface.name); argc++;
    tt_message_arg_add(m, TT_IN,  "string", ep->interface.addr); argc++;
    tt_message_arg_add (m, TT_IN, "string", ep->otherData); argc++;

    // now set event specific data
          
    switch (ep->evType) {
	case NV_REGISTER:
	    // On the request side, this is NULL place holder. On a response
	    // there will be data.
	    tt_message_arg_add (m, TT_INOUT, "string", NULL); argc++;
	break;
	case NV_NEW_NODE:
	    tt_message_arg_add (m, TT_IN, "string", ep->node.name); argc++;
	    tt_message_arg_add (m, TT_IN, "string", ep->node.addr); argc++;
	    tt_message_arg_add (m, TT_IN, "string", ep->node.MACAddr); argc++;
    	break;
	case NV_NEW_NET:
	    tt_message_arg_add (m, TT_IN, "string", ep->net.name); argc++;
	    tt_message_arg_add (m, TT_IN, "string", ep->net.addr); argc++;
	break;
	case NV_NEW_PROTO:
	    tt_message_arg_add (m, TT_IN, "string", ep->protocol); argc++;
 	break;
	case NV_CONVERSE_START:
	case NV_CONVERSE_STOP:
	    tt_message_arg_add (m, TT_IN, "string", ep->pair1.name); argc++;
	    tt_message_arg_add (m, TT_IN, "string", ep->pair1.addr); argc++;
	    tt_message_arg_add (m, TT_IN, "string", ep->pair1.MACAddr); argc++;
	    tt_message_arg_add (m, TT_IN, "string", ep->pair2.name); argc++;
	    tt_message_arg_add (m, TT_IN, "string", ep->pair2.addr); argc++;
	    tt_message_arg_add (m, TT_IN, "string", ep->pair2.MACAddr); argc++;
	    tt_message_iarg_add (m, TT_IN, "int", (int) ep->converseBytes); argc++;
	    tt_message_iarg_add (m, TT_IN, "int", (int) ep->conversePkts); argc++;
	break;
	case NV_RATE_THRESH_HI_MET:
	case NV_RATE_THRESH_HI_UN_MET:
	case NV_RATE_THRESH_LO_MET:
	case NV_RATE_THRESH_LO_UN_MET:
	    tt_message_iarg_add (m, TT_IN, "int", (int) ep->base); argc++;
	    tt_message_iarg_add (m, TT_IN, "int", (int) ep->rate); argc++;
	    tt_message_iarg_add (m, TT_IN, "int", (int) ep->thresholdRate); argc++;
	    tt_message_iarg_add (m, TT_IN, "int", (int) ep->baseRate); argc++;
	break;
	case NV_NEW_TOPN:
            {
	        int n = 0;
	        while (ep->topNList[n]) {
	           tt_message_arg_add (m, TT_IN, "string", ep->topNList[n]);
		   argc++;
	        }
            }
	break;
	  
    }
    D_PRINTF( (stderr, "EV_msg::send sending event %d\n", ep->evType));
    // ep->dump();
    ttstatus = tt_message_send(m);
    if (tt_is_err(ttstatus)) {
	D_PRINTF( (stderr, "EV_msg::send tt_message_send: %s\n", 
		 tt_status_message(tterr)));
	retval =  EV_MSG_ERR_SEND;
    }
    
    return (retval);
}
 
/*
 * receiveStart is the common code to begin a receive by 
 * called by both client and server classes.
 */
 
int
EV_msg::receiveStart (void) {
    
    
    D_PRINTF((stderr, "EV_msg::receiveStart ENTERED\n"));
    
    
    mark = tt_mark(); // to later tell tooltalk the beginning of memory to free
    
    msgin = tt_message_receive();   // get the message from tooltalk
    
    if (!msgin) {
	D_PRINTF((stderr, "EV_msg::receive: no message on active descriptor\n"));
	return (EV_MSG_ERR_RECEIVE);
    }
 /*   
    if (ttstatus = tt_ptr_error(msgin) !=TT_OK) {
	D_PRINTF((stderr, "EV_msg::receive: tt_message_receive: %s\n", 
		tt_status_message(ttstatus)));
	return (ttstatus);
    }
     
    opnum = tt_message_opnum (msgin);
    if (ttstatus = tt_int_error(opnum) != TT_OK) {
	D_PRINTF( (stderr, "EV_msg:receive tt_message_opnum: %s\n", 
		 tt_status_message(ttstatus)));
	return (ttstatus);
    }
*/    return (EV_OK);
}
/*
 * receiveEnd is called to actually get the data by both the client and
 * server classes.
 */
int
EV_msg::receiveEnd (EV_event *ep) {

    int argc;		// count of arguments in the message
    int nargs = 0;	// which argument being processed
    argc = tt_message_args_count(msgin);
    D_PRINTF( (stderr, "EV_msg::receive ttmsg %d, %s\n", 
	     opnum, tt_message_op(msgin)));
    
    // first get the data common to all events

    tt_message_arg_ival(msgin, nargs, (int *) &ep->evType); nargs++; 
    tt_message_arg_ival(msgin, nargs, (int *) &ep->timeSent); nargs++;
    tt_message_arg_ival(msgin, nargs, (int *) &ep->alarmLevel); nargs++;
    ep->srcApp = strdup (tt_message_arg_val(msgin, nargs)); nargs++;
    tt_message_arg_ival (msgin, nargs, (int *) &ep->pid); nargs++;
    ep->userName = strdup (tt_message_arg_val(msgin, nargs)); nargs++;
    tt_message_arg_ival (msgin, nargs, (int *) &ep->uid); nargs++;
    ep->srcHost.setName (tt_message_arg_val(msgin, nargs)); nargs++;
    ep->srcHost.setAddr (tt_message_arg_val(msgin, nargs)); nargs++;
    ep->setFilter (tt_message_arg_val(msgin, nargs )); nargs++;
    ep->setInterfaceName (tt_message_arg_val(msgin, nargs)); nargs++;
    ep->setInterfaceAddr (tt_message_arg_val(msgin, nargs)); nargs++;
    ep->setOtherData (tt_message_arg_val (msgin, nargs)); nargs++;
  
    // now get the event specific data
    switch (opnum) {
    case NV_EV_DETECT_NODE:
	ep->objType = OBJ_NODE;
	ep->setNodeName (tt_message_arg_val (msgin, nargs)); nargs++;
	ep->setNodeAddr (tt_message_arg_val (msgin, nargs)); nargs++;
	ep->setNodeMACAddr (tt_message_arg_val (msgin, nargs)); nargs++;
    break;
    case NV_EV_DETECT_NET:
	ep->objType = OBJ_NET;
	ep->setNetName (tt_message_arg_val (msgin, nargs)); nargs++;
	ep->setNetAddr (tt_message_arg_val (msgin, nargs)); nargs++;
    break;
    case NV_EV_DETECT_PROTO:
	ep->objType = OBJ_PROTO;
	ep->protocol = tt_message_arg_val (msgin, nargs); nargs++;
    break;
    case NV_EV_DETECT_CONV:
	ep->objType = OBJ_CONVERSE;
	ep->setEndPt1Name (tt_message_arg_val (msgin, nargs)); nargs++;
	ep->setEndPt1Addr (tt_message_arg_val (msgin, nargs)); nargs++;
	ep->setEndPt1MACAddr (tt_message_arg_val (msgin, nargs)); nargs++;
	ep->setEndPt2Name (tt_message_arg_val (msgin, nargs)); nargs++;
	ep->setEndPt2Addr (tt_message_arg_val (msgin, nargs)); nargs++;
	ep->setEndPt2MACAddr (tt_message_arg_val (msgin, nargs)); nargs++;
	tt_message_arg_ival (msgin, nargs, (int *) &ep->converseBytes); nargs++;
	tt_message_arg_ival (msgin, nargs, (int *) &ep->conversePkts); nargs++;
    break;
    case NV_EV_RATE_RPT:
        {
	    int irate = 0, ithresh = 0; int ibofrate = 0;
	    tt_message_arg_ival (msgin, nargs, (int *) &ep->base); nargs++;
	    tt_message_arg_ival (msgin, nargs, &irate); nargs++;
	    ep->rate = irate;
	    tt_message_arg_ival (msgin, nargs, &ithresh); nargs++;
	    ep->thresholdRate = ithresh;
	    tt_message_arg_ival (msgin, nargs, &ibofrate); nargs++;
	    ep->baseRate = ibofrate;
        }
    break;
    case NV_EV_TOPN_RPT:
        {
	    int n = 0;
	    D_PRINTF ((stderr, "EV_msg::receive got topN list.\n"));
	    while (0/*XXXX how do I look at a value?*/) {
	        ep->topNList[n] = strdup(tt_message_arg_val (msgin, nargs)); 
                nargs++;
	    }
        }
    break;
    }
    return (EV_OK);
}


/*
 * Client open is straighforward. Just join the tootalk session and return
 * a file descriptor to the user.
 */
int
EV_clnt_msg::open(void) {
    EV_msg::open();
    ttstatus = tt_session_join(tt_default_session());
    if (tt_is_err(ttstatus)) {
	D_PRINTF((stderr, "EV_msg::open tt_session_join: %s\n", tt_status_message(ttstatus)));
	ttfd = -1;
    }
    return (EV_msg::ttfd);
}

/*
 * The server open. It is the one that declares itself to
 * ToolTalk as a handler of netvis event messages.
 */
int
EV_srvr_msg::open(void) {
    ttreplymark = 0;
    EV_msg::open();
	//declare ptype etc.
    ttstatus = tt_ptype_declare("SGI_NV_event");
    if (tt_is_err(ttstatus)) {
	D_PRINTF((stderr, "EV_msg::open tt_ptype_declare: %s\n", 
		tt_status_message (ttstatus)));
	ttfd = -1;
    }
    ttstatus = tt_session_join(tt_default_session());
    if (tt_is_err(ttstatus)) {
	D_PRINTF((stderr, "EV_msg::open tt_session_join: %s\n", tt_status_message(ttstatus)));
	ttfd = -1;
    }
    return (EV_msg::ttfd);  
}

/*
 * client send -- the client send must determine whether it is sending a
 *                notification or a request. This is based on the event being
 *                sent. EV_handler will send the NV_Register event on behalf
 *                of the user and this is treated as a request because the
 *                reply with the no sends list should come back. 
 *		  Additionally,  if it is a request,  do not destroy the msg
 *                until the reply comes back. (I'm not sure why this has to
 *                be handled this way,  but the TT guru says so.
 */
int
EV_clnt_msg::send(EV_event *ep, char *noSends) {
    int retval;
    
    /*
     * Must do this before calling tooltalk message create routines because they
     * require the operation to be passed to them.
     */
    switch (ep->evType) {
	case NV_REGISTER:
	    ttop=evdOpStrs[NV_EV_REGISTER];
	break;
	case NV_STARTUP:
	case NV_SHUTDOWN:
	case NV_START_SNOOP:
	case NV_STOP_SNOOP:
	case NV_OTHER:
	    ttop = evdOpStrs[NV_EV_START_STOP_SNOOP];
	break;
	case NV_NEW_NODE:
	    ttop = evdOpStrs[NV_EV_DETECT_NODE];
	break;
	case NV_NEW_NET:
	    ttop =evdOpStrs[NV_EV_DETECT_NET];
	break;
	case NV_NEW_PROTO:
	    ttop = evdOpStrs[NV_EV_DETECT_PROTO];
	break;
	case NV_CONVERSE_START:
	case NV_CONVERSE_STOP:
	    ttop = evdOpStrs[NV_EV_DETECT_CONV];
	break;
	case NV_RATE_THRESH_HI_MET:
	case NV_RATE_THRESH_HI_UN_MET:
	case NV_RATE_THRESH_LO_MET:
	case NV_RATE_THRESH_LO_UN_MET:
	    ttop =evdOpStrs[NV_EV_RATE_RPT];
	break;
	case NV_NEW_TOPN:
	    ttop = evdOpStrs[NV_EV_TOPN_RPT];
	break;
    }
    if (ep->evType == NV_REGISTER) {
	m = tt_prequest_create (TT_SESSION, ttop);
    }
    else {
	// this is a general event notification	
	m = tt_pnotice_create(TT_SESSION, ttop );
    }
    if ((ttstatus = (enum tt_status) tt_ptr_error(m)) != TT_OK) {
	D_PRINTF((stderr, "EV_msg::send tt message create: %s\n", 
		tt_status_message(ttstatus)));
	retval = -1;
    }
    
    retval = EV_msg::send(ep, NULL);
    
    // can destroy it, no reply is coming
    
    if (ep->evType != NV_REGISTER)
	tt_message_destroy(m); 
	
    return (retval);
	
}

/*
 * This routine currently is called by the server only for the response to
 * the NV_Register message. However,  it will be used for forwarding events
 * in a future release. Since it is replying,  it use tt_message_reply rather
 * than tt_message_send. Once the reply is done,  the original incoming request
 * data may be destroyed.
 */
int
EV_srvr_msg::send(EV_event *ep, char *noSends) {
    int retval;
    
    if (ep->evType == NV_REGISTER) {
	if (!noSends) {
	    D_PRINTF( (stderr,
		    "EV_msg::send should have no noSendList passed\n"));
	}
	
	// set the argument for noSends, the others are returned as is
	
	tt_message_arg_val_set (msgin, TT_NOSEND_ARG_NUM,  noSends);
	D_PRINTF((stderr, "EV_msg::send Sending reply\n"));
	if ((tterr = tt_message_reply(msgin)) != TT_OK) {
	    D_PRINTF( (stderr, "EV_msg::send reply error %s\n", 
			tt_status_message (tterr)));
	    retval = -1;
	}
	else {
	    retval = EV_OK;
	}
	tt_release(ttreplymark); 
	ttreplymark = 0; 
	tt_message_destroy (msgin);
    }
    else {
	switch (ep->evType) {
	    case NV_REGISTER:
		ttop=evdOpStrs[NV_EV_REGISTER];
	    break;
	    case NV_STARTUP:
	    case NV_SHUTDOWN:
	    case NV_START_SNOOP:
	    case NV_STOP_SNOOP:
	    case NV_OTHER:
		ttop = evdOpStrs[NV_EV_START_STOP_SNOOP];
	    break;
	    case NV_NEW_NODE:
		ttop = evdOpStrs[NV_EV_DETECT_NODE];
	    break;
	    case NV_NEW_NET:
		ttop =evdOpStrs[NV_EV_DETECT_NET];
	    break;
	    case NV_NEW_PROTO:
		ttop = evdOpStrs[NV_EV_DETECT_PROTO];
	    break;
	    case NV_CONVERSE_START:
	    case NV_CONVERSE_STOP:
		ttop = evdOpStrs[NV_EV_DETECT_CONV];
	    break;
	    case NV_RATE_THRESH_HI_MET:
	    case NV_RATE_THRESH_HI_UN_MET:
	    case NV_RATE_THRESH_LO_MET:
	    case NV_RATE_THRESH_LO_UN_MET:
		ttop =evdOpStrs[NV_EV_RATE_RPT];
	    break;
	    case NV_NEW_TOPN:
		ttop = evdOpStrs[NV_EV_TOPN_RPT];
	    break;
	}
	m = tt_pnotice_create(TT_SESSION, ttop );
	retval = EV_msg::send(ep, NULL);
	tt_message_destroy (m);
    } 
    
    return(retval); 
}

/*
 * client receive -- receive a message for a client
 * 
 * If this message is the reply to register request,  it has (potentially)
 * a list of events not to send. So capture them. Called by EV_handler::receive
 */
int 
EV_clnt_msg::recv(EV_event *ep, char *noSends) {
    char * op;
    EV_stat retval = -1;
    Tt_state ttstate;
    if ((retval = EV_msg::receiveStart()) != EV_OK) return (retval);
    ttstate = tt_message_state(msgin);
    D_PRINTF((stderr, "EV_clnt_msg::recv TT req is in state %d\n", ttstate));
    switch (ttstate) {
	case TT_STARTED:
	case TT_CREATED:
	case TT_QUEUED:
	case TT_SENT:
	    D_PRINTF((stderr,"EV_clnt_msg::recv tt req still being processed\n"));
	break;
	case TT_FAILED:
	    D_PRINTF((stderr, "EV_cln_msg::recv TT req failed\n"));
	break;
	case TT_REJECTED:
	    D_PRINTF((stderr, "EV_cln_msg::recv TT req rejected\n"));
	break;
	case TT_STATE_LAST:
	    D_PRINTF((stderr, "EV_clnt_msg::recv unknown ttstatus\n"));
	break;
	case TT_HANDLED:
	    D_PRINTF((stderr,"EV_clnt_msg::recv tt req handled\n"));
	    op = tt_message_op (msgin);
	     if ((ttstatus = tt_ptr_error(op)) != TT_OK) {
		D_PRINTF( (stderr, "EV_clnt_msg:recv tt_message_opnum: %s\n", 
		 tt_status_message(ttstatus)));
	    return (ttstatus);
	    }
	    if (strcmp(op, evdOpStrs[NV_EV_REGISTER]) == 0) {
		if (noSends) {
		    char *ttPtr = NULL, *sndP = NULL;
		    int noSendCnt = 0;
		    ttPtr = sndP = tt_message_arg_val(msgin, TT_NOSEND_ARG_NUM); 
		    // only events not to send are in this message. Use them as
		    // an index to set the noSend value to true for that event.
		    while (ttPtr && *sndP) {
			noSends[*sndP++] = 1;
			noSendCnt++;
		    }	    
		    D_PRINTF((stderr, "EV_clnt_msg::recv noSendCnt %d\n",
			    noSendCnt));
		    if (EV_debugging) {
			for (int c = 0; c < NV_MAX_EVENT; c++) {
			    if (noSends[c] != 0) {
				D_PRINTF ((stderr,
					    "\tEvent %d will not be sent\n", 
					   c));
			    }
			}
		    }
		}
		else {
		    D_PRINTF ((stderr, "EV_clnt_msg::recv: requires noSendList\n"));
		}
	    }
	    else {
		EV_msg::receiveEnd(ep);

	    }
	    D_PRINTF((stderr, "EV_clnt_msg::recv destroying TT msg %s\n", 
	    tt_message_op(msgin)));
	    tterr = tt_message_destroy(msgin);
	    tt_release (mark);
	    retval = EV_OK;
	break;
    }
    return(retval);
}

/*
 * server receive - to handle message coming to the server
 * 
 * Note that the Register message is not destroyed and the data is not released.
 * It will be after the reply.
 */
int
EV_srvr_msg::recv(EV_event *ep, char *noSends) {
    EV_msg::receiveStart();
    opnum = tt_message_opnum (msgin);
    if ((ttstatus = tt_int_error(opnum)) != TT_OK) {
	D_PRINTF( (stderr, "EV_msg:receive tt_message_opnum: %s\n", 
		 tt_status_message(ttstatus)));
	return (ttstatus);
    }
    if (opnum == NV_EV_REGISTER) {
	ttreplymark = mark;
    }
    EV_msg::receiveEnd(ep);
    
    if (opnum != NV_EV_REGISTER) {
	D_PRINTF((stderr, "EV_srvr_msg::recv destroying TT msg %s\n", 
		tt_message_op(msgin)));
	tterr = tt_message_destroy(msgin);
	tt_release (mark);
	return(/*XXXX*/ TT_OK);
    }
}
