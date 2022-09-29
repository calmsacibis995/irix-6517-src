/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP Query Handler
 *
 *	$Revision: 1.8 $
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

/**** CVG Adding the support for TOO_BIG error handling as compared to  ****/
/**** mibquery.c++_0603							****/
/***									****/
/***  CVG using the same reqBlock for the resends. as Compared to the   ****/
/***  mibquery.c++_0604							****/

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <bstring.h>
#include <string.h>
#include <osfcn.h>

#include <tuCallBack.h> // For UI
#include <tuGadget.h>
#include <tuXExec.h> // For UI
#include <tuTimer.h>

#include <ctype.h>
#include "oid.h"
#include "asn1.h"
#include "packet.h"
#include "snmp.h"
#include "pdu.h"
#include "message.h"
#include "mibquery.h" 
#include "browser.h" // browser UI stuff

extern Browser *browser;
extern tuXExec *exec;
extern mibQueryHandler *queryHandler;

const unsigned int defaultBufferSize = 8192;
const unsigned int defaultTries = 3;
const unsigned int defaultInterval = 3;
const unsigned int communityStrLen = 80;

int lastblocknum = 1000; // remove this CVG

int reqid;

void timerFcn (tuGadget*, void* ReqIdP )
{
	queryHandler->timer(*(int*)ReqIdP);
}

void responseFcn (tuGadget*, void*)
{
	queryHandler->response();
}


mibQueryHandler::mibQueryHandler(void)
{

    tuCallBack* socketCallBack;
    socketCallBack  = new tuFunctionCallBack(responseFcn, NULL);

    sock = mh.open();
    exec->addCallBack(sock, False, socketCallBack);

    requestBuffer = 0;
    responseBuffer = 0;
    
    transList = 0;

}

mibQueryHandler::~mibQueryHandler(void)
{
    exec->removeCallBack(sock);
    (void) mh.close();
    if (requestBuffer != 0)
	delete requestBuffer;
    if (responseBuffer != 0)
	delete responseBuffer;
}

int
mibQueryHandler::get(char *hostname, char *community,
                     int timeout, int retries,
                     varBindList *arg, unsigned int transId)
{

    int reqType = SNMP_GetRequest;
    return query(hostname, community, timeout, retries,
		 reqType, arg, transId);
}

int
mibQueryHandler::getNext(char *hostname, char *community,
                     int timeout, int retries,
                     varBindList *arg, unsigned int transId)
{

    int reqType = SNMP_GetNextRequest;
    return query(hostname, community, timeout, retries,
		 reqType, arg, transId);
}

int
mibQueryHandler::set(char *hostname, char *community,
                     int timeout, int retries,
                     varBindList *arg, unsigned int transId)
{

    int reqType = SNMP_SetRequest;
    return query(hostname, community, timeout, retries,
		 reqType, arg, transId);
}


int
mibQueryHandler::query(char *hostname, char *community,
                     int timeout, int retries,
                         int reqType,  varBindList *arg, int transId)
{
    int                rc;
    snmpPacket         request; 
    struct transInfo   *transBlock;

    asnObjectIdentifier* aoi;
    asnObject* o;
    char *oid, *value;


    transBlock             = new struct transInfo;
    transBlock->tranId     = transId;
    transBlock->totalReqs  = 0;
    transBlock->totalReceived = 0;
    transBlock->totalList = 0;
    transBlock->tranResult = new struct result;
    transBlock->hostname   = strdup(hostname);
    transBlock->timeout    = timeout;
    transBlock->maxRetries = retries;
    
    transBlock->community = strdup(community);
    
    transBlock->next = NULL;

    // divide the arg in chunks of 5 vars

    
    // int totalList = 0;

    if ( (reqType == SNMP_GetRequest ) || ( reqType == SNMP_GetNextRequest) )
    {
     for ( varBindList *vbl = arg->getNext(); vbl != 0; transBlock->totalList++ )
     {
	transBlock->reqBlock[transBlock->totalList] = new reqInfo;

     	for ( int j = 0; ( (vbl != 0) && (j < MAXIDS) ); j++ )
     	{
		varBind *v = vbl->getVar();
		vbl = vbl->getNext();
                arg->removeVar(v, 0);

	        aoi = v->getObject();
        	oid = aoi->getString();
//        	varBind* newV = new varBind(oid, aoi);
        	varBind* newV = new varBind(aoi);
		
		transBlock->reqBlock[transBlock->totalList]->requestpdu.getVarList()->appendVar(newV);

     	}

        transBlock->reqBlock[transBlock->totalList]->requestpdu.setRequestId(reqid);
        transBlock->reqBlock[transBlock->totalList]->requestpdu.setErrorStatus(0);
        transBlock->reqBlock[transBlock->totalList]->requestpdu.setErrorIndex(0);
        transBlock->reqBlock[transBlock->totalList]->requestpdu.setType(reqType);
        transBlock->reqBlock[transBlock->totalList]->reqId  = reqid;

    	if (requestBuffer == 0)
        	requestBuffer = new char[defaultBufferSize];

    	request.setCommunity(transBlock->community);

    	request.setPDU(&(transBlock->reqBlock[transBlock->totalList]->requestpdu));

        // Encode the request

        int len = request.encode(requestBuffer, defaultBufferSize);
        if (len == 0)
                return MIBQUERY_ERR_encodeError;

        // Send the message

        rc = mh.sendPacket(hostname, &request, len);

        // Check for send error

        if (rc != len)
            return MIBQUERY_ERR_sendError;
	
	// start timer
	
	tuCallBack* timerCallBack;
	timerCallBack = new tuFunctionCallBack(timerFcn, &(transBlock->reqBlock[transBlock->totalList]->reqId));

	tuTimer *localTimer = new tuTimer();
	transBlock->reqBlock[transBlock->totalList]->reqTimer = localTimer;
	localTimer->setCallBack(timerCallBack);
	localTimer->start((float)timeout); 
	
	reqid++;
	transBlock->totalReqs++;


    } // end of for varBindList *vbl ..

   } // end of if reqType == GetRequest

   else  // reqType is SetRequest
   {
        transBlock->reqBlock[transBlock->totalList] = new reqInfo;

        for ( varBindList *vbl = arg->getNext(); vbl != 0; )

        {
          varBind *v = vbl->getVar();
          vbl = vbl->getNext();
          arg->removeVar(v, 0);
          transBlock->reqBlock[transBlock->totalList]->requestpdu.getVarList()->appendVar(v);
        }

        transBlock->reqBlock[transBlock->totalList]->requestpdu.setRequestId(reqid);
        transBlock->reqBlock[transBlock->totalList]->requestpdu.setErrorStatus(0);
        transBlock->reqBlock[transBlock->totalList]->requestpdu.setErrorIndex(0);
        transBlock->reqBlock[transBlock->totalList]->requestpdu.setType(reqType);
        transBlock->reqBlock[transBlock->totalList]->reqId  = reqid;

        if (requestBuffer == 0)
                requestBuffer = new char[defaultBufferSize];

        request.setCommunity(transBlock->community);

        request.setPDU(&(transBlock->reqBlock[transBlock->totalList]->requestpdu));

        // Encode the request

        int len = request.encode(requestBuffer, defaultBufferSize);
        if (len == 0)
                return MIBQUERY_ERR_encodeError;

        // Send the message

        rc = mh.sendPacket(hostname, &request, len);

        // Check for send error

        if (rc != len)
            return MIBQUERY_ERR_sendError;

        tuCallBack* timerCallBack;
        timerCallBack = new tuFunctionCallBack(timerFcn, &(transBlock->reqBlock[transBlock->totalList]->reqId));

        tuTimer *localTimer = new tuTimer();
        transBlock->reqBlock[transBlock->totalList]->reqTimer = localTimer;
        localTimer->setCallBack(timerCallBack);
        localTimer->start((float)timeout); 

        reqid++;
        transBlock->totalReqs++;
	// printf("query: totalReqs= %d\n", transBlock->totalReqs); // ppp
        transBlock->totalList++;
   }
                                 
    // Put the transBlock on the list of transBlocks	

    (void) addToList(transBlock);  // Will add the transBlock on the
 			  	 // transList of this instance.

    return MIBQUERY_ERR_noError;

} // end of query


void mibQueryHandler::addToList(struct transInfo *transAdd)
{
	if ( transList == NULL )

	    transList = transAdd;

	else

	{
	   for ( struct transInfo *transWalk = transList;
		 transWalk->next != NULL;
		 transWalk = transWalk->next )

		; // walk to the end of the list

	   transWalk->next = transAdd;
	}
}


void mibQueryHandler::removeFromList(struct transInfo *transRemove)
{
	struct transInfo *transWalk, *prev;

	if ( ( transList->next == NULL ) && ( transList->tranId == transRemove->tranId ) )

        {
	   delete transList;
	   transList = NULL;
	   return;
	}


	for ( transWalk = prev = transList; transWalk != NULL; transWalk = transWalk->next )
        {
	    if ( transWalk->tranId == transRemove->tranId )
	    {
	     if ( prev == transWalk ) // First list member to be deleted
	     {
		transList = transList->next;
		delete transWalk;
		return;
	     }

	     else

	     {
		prev->next = transWalk->next;
		delete transWalk;
		return;
	     }		       

	    }

	    prev = transWalk;
		
	}
}


void mibQueryHandler::response()
{
    // printf("mibQueryHandler::response\n"); // ppp
    snmpPacket 	 response;
    int 	 rc;
    unsigned int reqtype;
    int foundit  = FALSE;

    // Receive the response
    response.setAsn(responseBuffer, defaultBufferSize);
    rc = mh.recv(0, &response, defaultBufferSize);
    if (rc < 0)
	return; 

    // Decode the response
    int len = response.decode();
    if (len == 0)
	return; 

    // Pull out response PDU
    snmpPDU *pdu = (snmpPDU *) response.getPDU();
    
    struct transInfo *transptr;
    for ( transptr = transList; transptr != NULL; transptr = transptr->next )

    {
	for ( int i = 0; i < transptr->totalList; i++ )

	{
           if ( transptr->reqBlock[i]->reqId == pdu->getRequestId() )

           {
		// check for the errors; if none then fill in the response
		// values; else resend the packet 
		// CVG DO I have to stop timer here??

                transptr->reqBlock[i]->reqTimer->stop();

		foundit = TRUE;
                transptr->reqBlock[i]->received = TRUE;
                transptr->totalReceived++;
		// printf("response: totalReceived = %d\n", transptr->totalReceived); // ppp
		reqtype = transptr->reqBlock[i]->requestpdu.getType(); 

		int error = pdu->getErrorStatus();
		int index = pdu->getErrorIndex();
		
		// printf("error: %d\n", error);
		
// Really don't bother to send unless genErr or tooBig, but if one
// of those, need to put it in a different result varbindlist.

		if ( error != SNMP_ERR_noError)
		{
		    if ( error == SNMP_ERR_tooBig )
			bigResend(transptr, i, reqtype);

		    else {
			// printf("   response->resend\n"); // ppp
			resend(transptr, i, error, index, reqtype); 
			// note - we ought to check return value!
		    }
		}

		else  // no error!!

		{
    		   // Fill in response values

    		   for (varBindList *vbl = pdu->getVarList()->getNext();
                        vbl != 0; ) 
		   {
        		varBind *v = vbl->getVar();
			vbl = vbl->getNext();
        		pdu->getVarList()->removeVar(v, 0);
        		transptr->tranResult->goodResult.appendVar(v);
 	 	   }
		}

		break;
	   } // end of if foundit
	   if (foundit == TRUE)
	        break;
        }  // end of for i = ..
	
	if (foundit == TRUE)
	    break;

    }  // end of for transptr ...

    if ( foundit == TRUE ) // if not, there is no point in checking
    {
	// check whether all the reqids in transBlock have been
 	// received. If yes, then send it to UI.

	if ( transptr->totalReqs == transptr->totalReceived )
	 {
	   if ( ( reqtype == SNMP_GetRequest) || 
                ( reqtype == SNMP_GetNextRequest) )

	     browser->getResponse(transptr->tranId, transptr->tranResult);

	   else
	     browser->setResponse(transptr->tranId, transptr->tranResult); 
	 }
    }
}



int mibQueryHandler::resend(struct transInfo *transptr,int blocknum,
                            int error, int index, unsigned int reqtype)
{
  int rc;
  snmpPacket request;
  int needToResend = FALSE;
 
    asnObjectIdentifier* aoi;
    asnObject* o;
    char *oid, *value;

    int count = 1;

    for ( varBindList *vbl = transptr->reqBlock[blocknum]->requestpdu.getVarList()->getNext(); vbl != 0; count++ )

    {
      if ( count == index )
      {
       varBind *v = vbl->getVar();
       vbl = vbl->getNext();
       transptr->reqBlock[blocknum]->requestpdu.getVarList()->removeVar(v, 0);


       switch (error) {

              case SNMP_ERR_noSuchName:
	           	transptr->tranResult->notfoundResult.appendVar(v);
                   	break;

              case SNMP_ERR_badValue:
			transptr->tranResult->badvalueResult.appendVar(v);
                   	break;

              case SNMP_ERR_readOnly:
			transptr->tranResult->readonlyResult.appendVar(v);
                   	break;

              case SNMP_ERR_genErr:
			transptr->tranResult->generrorResult.appendVar(v);
                   	break;

              default :
                   	break;
       } // end of switch
       
    }  // end of if count == index

    else
    {
       needToResend = TRUE;
        
       vbl = vbl->getNext();


    }
   } // end of for ...


	// if we have a bad index (e.g. from the agent bug),
	// don't resend, because we didn't remove anything
	// NOTE: currently, the return val is ignored!
	if ( index >= count)
	    return MIBQUERY_ERR_snmpError;
	    
	if ( needToResend == FALSE )
	{
	   //printf("theres is no need for resend as there was only one var..\n");
	   // printf("returning from resend...\n");
	   return MIBQUERY_ERR_noError;
	}
	
        if (requestBuffer == 0)
                requestBuffer = new char[defaultBufferSize];

	// CVG put the community from the transptr 
        request.setCommunity(transptr->community );

	request.setPDU(&(transptr->reqBlock[blocknum]->requestpdu));

        // Encode the request

        int len = request.encode(requestBuffer, defaultBufferSize);
        if (len == 0)
                return MIBQUERY_ERR_encodeError;

        // Send the message

        rc = mh.sendPacket(transptr->hostname, &request, len);


        // Check for send error
        if (rc != len)
            return MIBQUERY_ERR_sendError;

	// start the timer

	tuCallBack* timerCallBack;
        timerCallBack = new tuFunctionCallBack(timerFcn, &reqid);

        tuTimer *localTimer = new tuTimer();
	transptr->reqBlock[blocknum]->reqTimer = localTimer;
        localTimer->setCallBack(timerCallBack);
        localTimer->start((float)transptr->timeout); // CVG

        transptr->totalReqs++; // increment request count only if there
			       // are no errors.
	// printf("resend: totalReqs = %d\n", transptr->totalReqs); // ppp
	return MIBQUERY_ERR_noError;

} // resend

int mibQueryHandler::bigResend(struct transInfo *transptr, int  blocknum,
                               unsigned int reqtype)
{
   int        rc;
   snmpPacket request;

if ( lastblocknum == blocknum )
{
	// printf("WHY????????\n");
	return(0); // returning something to make the compiler happy
}

   lastblocknum = blocknum;

   // Count the total vars in the original request

   int totalvars = 0;

   for (varBindList *vbl = transptr->reqBlock[blocknum]->requestpdu.getVarList()->getNext(); vbl != 0; vbl = vbl->getNext() )
        totalvars++;

   // Divide those vars in 2 separate varBindLists and request blocks

   int i = 0;
   for (vbl = transptr->reqBlock[blocknum]->requestpdu.getVarList()->getNext(); ((vbl != 0) && (i < 2));  i++ )
   {
       transptr->reqBlock[transptr->totalList] = new reqInfo; 
       for ( int j = 0; (( vbl != 0) && ( j <= (totalvars/2) )); j++ )
       {
	   varBind *v = vbl->getVar();
	   vbl = vbl->getNext();
	   transptr->reqBlock[transptr->totalList]->requestpdu.getVarList()->appendVar(v);
       }

        transptr->reqBlock[transptr->totalList]->requestpdu.setRequestId(reqid);
        transptr->reqBlock[transptr->totalList]->requestpdu.setErrorStatus(0);
        transptr->reqBlock[transptr->totalList]->requestpdu.setErrorIndex(0);
        transptr->reqBlock[transptr->totalList]->requestpdu.setType(reqtype);
        transptr->reqBlock[transptr->totalList]->reqId  = reqid;

        if (requestBuffer == 0)
                requestBuffer = new char[defaultBufferSize];

        // CVG put the community from the transptr
        request.setCommunity(transptr->community );
        request.setPDU(&(transptr->reqBlock[transptr->totalList]->requestpdu));

        // Encode the request

        int len = request.encode(requestBuffer, defaultBufferSize);
        if (len == 0)
                return MIBQUERY_ERR_encodeError;

        // Send the message

        rc = mh.sendPacket(transptr->hostname, &request, len);


        // Check for send error
        if (rc != len)
            return MIBQUERY_ERR_sendError;

        // start the timer

        tuCallBack* timerCallBack;
        timerCallBack = new tuFunctionCallBack(timerFcn, &reqid);

        tuTimer *localTimer = new tuTimer();
        transptr->reqBlock[transptr->totalList]->reqTimer = localTimer;
        localTimer->setCallBack(timerCallBack);
        localTimer->start((float)transptr->timeout);

        reqid++;
        transptr->totalReqs++; // increment request count only if there
                               // are no errors.
	// printf("bigResend: totalReqs = %d\n", transptr->totalReqs); // ppp
	transptr->totalList++;

   }

 
} // bigResend


void mibQueryHandler::timer(int requestId)           
{
    int foundit = FALSE;
    unsigned int reqtype;
    int rc;
    snmpPacket request;

    // printf("mibQueryHandler::timer ... requestId is %d\n", requestId);

    for ( struct transInfo *transptr = transList; transptr != NULL;
          transptr = transptr->next )
    {
        for ( int i = 0; i < transptr->totalList; i++ )
        {
           if ( transptr->reqBlock[i]->reqId == requestId )
	   {
		foundit = TRUE;

	   	if ( transptr->reqBlock[i]->tries >= transptr->maxRetries )
		{
		  transptr->totalReceived++;
		  // printf("timer: totalReceived = %d\n", transptr->totalReceived); // ppp
		  reqtype = transptr->reqBlock[i]->requestpdu.getType();
		  
		 // Fill in the timerexpired varBindList and call
		 // UI 

		  for ( varBindList *vbl = transptr->reqBlock[i]->requestpdu.getVarList()->getNext();
			vbl != 0;  )

		  {
		   varBind *v = vbl->getVar();
		   vbl = vbl->getNext();
		   transptr->reqBlock[i]->requestpdu.getVarList()->removeVar(v, 0);
		   transptr->tranResult->timeexpiredResult.appendVar(v);
		  }

		  if ( transptr->totalReqs == transptr->totalReceived )
		  { // call UI 
                    if ( ( reqtype == SNMP_GetRequest) || 
                         ( reqtype == SNMP_GetNextRequest) ) 

                      browser->getResponse(transptr->tranId, transptr->tranResult);
                    else
                      browser->setResponse(transptr->tranId, transptr->tranResult);
		  }
		}
		else

		{
		  // resend the packet and increment the tries.
		  // CVG Do I have to start timer here??

                 transptr->reqBlock[i]->tries++;
		 transptr->reqBlock[i]->reqTimer->start((float)transptr->timeout);
 
        	 if (requestBuffer == 0)
                	requestBuffer = new char[defaultBufferSize];

       		 request.setCommunity(transptr->community);
        	 request.setPDU(&(transptr->reqBlock[i]->requestpdu));

        	 // Encode the request

        	 int len = request.encode(requestBuffer, defaultBufferSize);
        	 if (len == 0)
                   return;

        	 // Send the message

        	 rc = mh.sendPacket(transptr->hostname, &request, len);



        	 // Check for send error
		/****** ??? CVG
        	 if (rc != len)
            		return;
		*******/

		}
		
		return;
		
	    } // if foundit
	    
	} // for i

    } // for transPtr
	
}


