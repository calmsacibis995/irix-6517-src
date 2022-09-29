#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <evmonapi.h>
#include <ssdbapi.h>
#include "sgmtask.h"
#include "sgmtaskerr.h"
#include "sgmauth.h"

__uint32_t deliverevnt_srvr(char *SGMHost,void *data,int dlen)
{
    deliverEvent_t    *deliverEvent = NULL;
    char              *evData = NULL;
    int               err = 0;
    int               i = 0;
    char              authStr[AUTHSIZE];
    int               authlen = 0, authType;
    int               alignment;

    if ( !SGMHost || !data || dlen <= 0 ) 
	return(ERR(SERVERERR, PARAMERR, INVALIDARGS));

    if ( (err=SGMSSDBInit(1)) != 0 ) {
	return(SSSERR(SERVERERR, SSDBERR, err));
    }

    if ( (authlen = SGMAUTHIGetAuth(SGMHost, authStr, &authType, 0)) == 0)
	return(ERR(SERVERERR, AUTHERR, CANTCREATEAUTHINFO));

    if ( !unpack_data(authStr, authlen, &deliverEvent, sizeof(deliverEvent_t),
		      &evData, data, dlen, 0) ) {
	return(ERR(SERVERERR, AUTHERR, CANTCREATEAUTHINFO));
    }

    alignment = 8-(authlen%8);

    /* Lets check if there is any clobbering of data over network */
    /* This, we do by checking the sizes of data received         */

    if ( dlen != (authlen+sizeof(deliverEvent_t) +deliverEvent->evDataLength+alignment)) {
	return(ERR(SERVERERR, RPCERR, CLOBBEREDDATA));
    }

    err = emapiIsDaemonInstalled();

    if ( !err ) {
	return (SSSERR(SERVERERR, EVENTMON, EVMONNOTRUNNING));
    }

#ifdef DEBUG
    errorexit("SGMTASK", 0, 7, 
	      "Event Data received from %s : %ld, %ld, %ld, %ld, %ld, %s",
	      SGMHost, deliverEvent->eventClass, deliverEvent->eventType,
	      deliverEvent->eventTimeStamp, deliverEvent->noOfEvents,
	      deliverEvent->evDataLength, evData);
#endif

    err = emapiSendEvent(SGMHost, deliverEvent->eventTimeStamp,
			 deliverEvent->eventType, deliverEvent->priority, 
			 evData);

    if ( !err ) {
	return(SSSERR(SERVERERR, EVENTMON, EVMONAPIERR));
    } 

    SGMSSDBDone(1);
    return(0);

}
