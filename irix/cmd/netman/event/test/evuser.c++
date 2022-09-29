#include <sys/types.h>
#include <time.h>
#include <stdio.h>
#include "event.h"

/*
 * evuser.c++ -- test program for users of the NetVisualyzer event classes
 * 
 * $Revision: 1.5 $
 * This program demonstrates rudimentry usage of various types of events. It
 * is tutorial in that it goes through various combinations and means of 
 * sending,  receiving, and extracting event data.
 */

main ()
{
/*
 * The instantiate and event handler to send/receive events. initialize the
 * application name.
 */
    EV_handle	handle;
    time_t then,  now;
    then = time((time_t *) 0);
    EV_handler eh(&handle, "evuser");
    now = time((time_t *) 0);
    printf("time to create event handler: %d secs\n", now-then);
    
/*
 * These classes must be declared and initialized to call the various
 * EV_event constructors with the optional arguments.
 * 
 */
    
    EV_objID  inter("wookie"),  localInt("bubba"),  nodeSeen("whizzer.wpd",  
							     "192.26.75.5");						      

 // declaration  and initialization and sending of a startup event type

    
    EV_event start(NV_STARTUP, &localInt , "total");
    eh.send(&start);
    printf("sent NV_STARTUP event(%d)\n", NV_STARTUP); 
    
    EV_event shut(NV_SHUTDOWN, "bubba", "filter to show char type interface");
    eh.send(&shut);
    printf("send NV_SHUTDOWN event(%d)\n", NV_SHUTDOWN);

 // declaration  and initialization and sending of a rate event
 // topNList XXXX temporary definition
 
    static char *olist[11] =  { "pike", "redbout", "sgi", "whizzer", "squaw", 
			"bubba", "wookie", "jenny", "ghatge", "mountain", 0};
 
    EV_event ev(NV_RATE_THRESH_HI_MET,628000., 512000.0, BYTE_BASED, 0.0,  
		 &inter, "total", (objectList *) &olist);
    eh.send(&ev);
    printf("sent NV_RATE_THRESH_HI_MET event(%d)\n", NV_RATE_THRESH_HI_MET); 
    
    EV_event evrate (NV_RATE_THRESH_LO_MET, 52., 50., PERCENT_BYTES,
		     724000., "rateInterface", "nfs");
    eh.send(&evrate);
    printf("sent NV_RATE_THRESH_LO_MET event(%d)\n", NV_RATE_THRESH_LO_MET);
		      
    EV_event evrateN (NV_RATE_THRESH_HI_MET, 74., 71., PERCENT_N_BYTES,
		     724000., "rateInterfaceN", "decnet");
    eh.send(&evrateN);
    printf("sent NV_RATE_THRESH_HI_MET event(%d)\n", NV_RATE_THRESH_HI_MET);

 // declaration  and initialization of a node detection event
 
    EV_event detect(NV_NEW_NODE, OBJ_NODE, &localInt, 
		   "ip.between squaw jenny", &nodeSeen);  
    eh.send(&detect);
    printf("sent NV_NEW_NODE event(%d)\n", NV_NEW_NODE); 


 // Use of pointer to initalize each member.
 
    EV_event *ep;  

    ep = new EV_event(NV_CONVERSE_STOP);          
    ep->setFilter("ip.host bubba and tcp");
    ep->setInterfaceName("bubba");
    ep->setInterfaceAddr("192.26.75.178");
    ep->setObjType(OBJ_CONVERSE);
    ep->setEndPt1Name("joshua");
    ep->setEndPt1Addr("192.26.75.189");
    ep->setEndPt2Name("denali");
    ep->setEndPt2Addr("192.26.75.191");
    ep->setBytes(1200500);
    ep->setPkts(80300);
    ep->setOtherData("Other data from evuser");
        
    eh.send(ep);
    printf("sent NV_CONVERSE_STOP event(%d)\n", NV_CONVERSE_STOP); 

    delete ep;
}
