/*---------------------------------------------------------------------------*/
/*                                                                           */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                */
/* All Rights Reserved.                                                      */
/*                                                                           */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics Inc.;     */
/* contents of this file may not be disclosed to third parties, copied or    */
/* duplicated in any form, in whole or in part, without the prior written    */
/* permission of Silicon Graphics, Inc.                                      */
/*                                                                           */
/* RESTRICTED RIGHTS LEGEND:                                                 */
/* Use,duplication or disclosure by the Government is subject to restrictions*/
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data    */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or  */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -   */
/* rights reserved under the Copyright Laws of the United States.            */
/*                                                                           */
/*---------------------------------------------------------------------------*/

#ident           "$Revision: 1.7 $"


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ssdbapi.h>
#include <syslog.h>
#include <sys/time.h>
#include "sgmtask.h"
#include "sgmauth.h"
#include "sgmtaskerr.h"

/*---------------------------------------------------------------------------*/
/*  External Declarations                                                    */
/*---------------------------------------------------------------------------*/

extern int SGMAUTHISetAuth(char *, char *, int);
extern int SGMAUTHIGetAuth(char *, char *, int *, int);
extern __uint32_t SGMIrpcclient(char *, int, sgmtask_t *, int);
extern int pack_data(char **, char *, int, void *, int, void *, int, int);
extern int unpack_data(char *, int, void **, int, void **, char *, int, int);
extern void SGMIfree(char *, void *);
extern __uint32_t SGMSSDBInit(int);
extern void SGMSSDBDone(int);

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

int dsmpgInit(void);
int dsmpgDone(void);
int dsmpgExecEvent(int, int, int, int, void *, int, char *, const char **);

/*---------------------------------------------------------------------------*/
/* Name    : dsmpgInit                                                       */
/* Purpose : Init for DSO                                                    */
/* Inputs  : None                                                            */
/* Outputs : Always 0                                                        */
/*---------------------------------------------------------------------------*/

int dsmpgInit(void)
{
    return(0);
}

/*---------------------------------------------------------------------------*/
/* Name    : dsmpgDone                                                       */
/* Purpose : Stub for DSO exit                                               */
/* Inputs  : None                                                            */
/* Outputs : Always 0                                                        */
/*---------------------------------------------------------------------------*/

int dsmpgDone(void)
{
    return(0);
}

/*---------------------------------------------------------------------------*/
/* Name    : dsmpgExecEvent                                                  */
/* Purpose : Main entry point for Remote Delivery of events                  */
/* Inputs  : Class, type, priority, #of events, Data, Datalen, args          */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int dsmpgExecEvent(
				    int    evClass,
				    int    evType, 
				    int    priority,
				    int    numEvents,
				    void   *evData,
				    int    evDataLength,
				    char   *myHost,
                                    const char **optStr)
{
    deliverEvent_t     deliverEvent;
    sgmtask_t          task;
    char               *ptr;
    __uint32_t         err = 0;
    char               *remoteHost = NULL;
    char               authStr[AUTHSIZE];
    int                authlen = 0, authType;
    time_t             evTimeStamp = 0;
    int                myinit = 0;
    char               *sysid = NULL;
#ifdef DEBUG
    struct timeval     t;
    double             entered_time, before_rpc, after_rpc;
    char               mybuf[256];

    gettimeofday(&t);
    entered_time = (double) t.tv_sec + (double) t.tv_usec/1000000.0;
#endif

    /*-----------------------------------------------------------------------*/
    /*   Pick up Remote Host and Timestamp passed to us as arguments         */
    /*-----------------------------------------------------------------------*/

    remoteHost = (char *) optStr[0];
    sysid      = (char *) optStr[2];

    if ( optStr[1] ) evTimeStamp = (int) atoi(optStr[1]);


    if ( remoteHost == NULL ) return (1);
    if ( evType <= 0 ) return (1);

    if ( (err = SGMSSDBInit(0)) != 0 ) {
	pSSSErr(SSSERR(SSSERROR, SSDBERR, err));
	return(1);
    }

    /*-----------------------------------------------------------------------*/
    /*   Init SSDB.We need this as SGMAUTHIGetAuth will try to access SSDB to*/
    /*   get the authorization details for the remote host. Once the auth.   */
    /*   details are obtained, populate deliverEvent structure, encrypt      */
    /*   the data and send it over to RPC Server                             */
    /*-----------------------------------------------------------------------*/

    authlen = SGMAUTHIGetAuth(remoteHost, authStr, &authType, 0);
    SGMSSDBDone(0);

    if ( authlen == 0 )  {
	return(1);
    }

    deliverEvent.eventClass    = evClass;
    deliverEvent.eventType     = evType;
    deliverEvent.priority      = priority;
    deliverEvent.eventTimeStamp= evTimeStamp;
    deliverEvent.noOfEvents    = numEvents;
    deliverEvent.evDataLength  = evDataLength;

    task.datalen = pack_data(&ptr, authStr, authlen, (void *) &deliverEvent, 
                             sizeof(deliverEvent_t), (void *) evData, 
                             evDataLength, 0);
    if ( !task.datalen ) {
        return(1);
    }
    task.rethandle = NULL;
    task.dataptr = ptr;

    /*-----------------------------------------------------------------------*/
    /*   Call RPC Client to deliver event to remote machine.  Once this      */
    /*   is done, free the data we malloced and authStr.  Close connection   */
    /*   with DB and return.                                                 */
    /*-----------------------------------------------------------------------*/
#ifdef DEBUG
    gettimeofday(&t);
    before_rpc = t.tv_sec + (double) t.tv_usec/1000000.0;
#endif

    err = SGMIrpcclient(remoteHost, DELIVERREMOTEPROC, &task, 0);

#ifdef DEBUG
    gettimeofday(&t);
    after_rpc = t.tv_sec + (double) t.tv_usec/1000000.0;
#endif
    (void) SGMIfree("deliverremote", ptr);

#ifdef DEBUG
    snprintf(mybuf, sizeof(mybuf), "Entered: %f, Before RPC: %f, After RPC: %f", entered_time, before_rpc, after_rpc);
    errorexit("dsmpgExecEvent", 0, 7, 
              "Remote Delivery of event for %s,%d (%s)", 
                remoteHost, evTimeStamp, mybuf);
#endif

    if (err) {
	pSSSErr(err);
        return(1);
    }

    return(0);
}

