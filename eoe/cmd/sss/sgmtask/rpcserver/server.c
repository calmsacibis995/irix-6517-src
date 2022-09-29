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

#ident           "$Revision: 1.9 $"

/*---------------------------------------------------------------------------*/
/* Include files                                                             */
/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <syslog.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <utmp.h>
#include <rpcsvc/rusers.h>
#include <errno.h>
#include "sgmtask.h"
#include "sgmtaskerr.h"
#include "sgmrpc.h"
#include "sgmstructs.h"

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

void processData();
extern int xdr_sgmclnt();
extern int xdr_sgmsrvr();
extern int xdr_client_xfr();
extern int deliver_to_eventmon();

/*---------------------------------------------------------------------------*/
/* Global Declarations                                                       */
/*---------------------------------------------------------------------------*/

char *cmd;

/*---------------------------------------------------------------------------*/
/* Name    : main                                                            */
/* Purpose : Main server code for RPC Connections                            */
/* Inputs  : None                                                            */
/* Outputs : None                                                            */
/*---------------------------------------------------------------------------*/

int main(int argc, char **argv)
{
    register  SVCXPRT         *transport;
    int   len = sizeof(struct sockaddr_in);
    struct sockaddr_in  addr;

    cmd = argv[0];
    signal_setup();

#ifdef USEINETD
    if ( getsockname(0, &addr, &len) != 0 ) {
	errorexit(cmd, EXIT, LOG_ERR, "getsockname error");
    }
#endif

#ifdef USEINETD
    if ( (transport = svctcp_create(0, 0, 0)) == NULL ) {
	errorexit(cmd,EXIT,LOG_ERR, "Error initializing RPC Service for SGM");
    }
#else
    if ( (transport = svctcp_create(RPC_ANYSOCK, MAXBUFSIZE, MAXBUFSIZE))
		      == NULL ) {
	errorexit(cmd, EXIT, LOG_ERR, 
			  "Error initializing RPC Service for SGM Server");
    }
    pmap_unset(RPCPROGRAM, RPCVERSIONS);
#endif

#ifdef USEINETD
    if ( !svc_register(transport, RPCPROGRAM, RPCVERSIONS, processData, 0)) {
	errorexit(cmd, EXIT, LOG_ERR, "Error in svc_register");
    }
#else
    if ( !svc_register(transport, RPCPROGRAM, RPCVERSIONS, processData,
		       IPPROTO_TCP)) {
	errorexit(cmd, EXIT, LOG_ERR, 
			  "Error in svc_register");
    }
#endif

    svc_run();
    errorexit(cmd, EXIT, LOG_CRIT, 
			 "Fatal Error: svc_run returned from processData");
}

/*---------------------------------------------------------------------------*/
/* Name    : processData                                                     */
/* Purpose : Main RPC Handling routine                                       */
/* Inputs  : Given by RPC                                                    */
/* Outputs : None                                                            */
/*---------------------------------------------------------------------------*/

void processData(rqstp, transp)
		 struct svc_req  *rqstp;
  	         SVCXPRT         *transp;
{
    uid_t                     userID;
    struct authunix_parms     *unix_cred;
    sgmclnt_t                 clientdata = {0, 0, NULL};
    sgmsrvr_t                 serverdata = {0, 0, 0, 0, 0, NULL, NULL};
    sgmclntxfr_t              clientxfrdata= {NULL, 0, 0};
    __uint32_t                err = 0;
    char                      remHostName[512];

    if ( rqstp->rq_proc == NULLPROC ) {
#ifdef DEBUG
        errorexit(cmd, NOEXIT, LOG_DEBUG, "NULLPROC received from client");
#endif
	if ( svc_sendreply(transp, xdr_void, 0) == 0 ) {
	    errorexit(cmd, EXIT, LOG_ERR, "Can't reply to client's NULLPROC");
	}

#ifdef USEINETD
        exit(0);
#else
	return;
#endif
    }


    /* Get Official Client Name */

    if ( SGMNETIGetOffHostNmFrAddr(&transp->xp_raddr, remHostName) ) {
	serverdata.errCode = ERR(SERVERERR, NETERR, CANTGETHOSTINFO);
	goto sendreply;
    }


    if ( rqstp->rq_proc == XMITPROC ) {
	if ( !svc_getargs(transp, xdr_client_xfr, &clientxfrdata)) {
	    svcerr_systemerr(transp);
#ifdef USEINETD
            errorexit(cmd, NOEXIT, LOG_ERR, "Error in svc_getargs");
	    exit(1);
#else
	    return;
#endif
	}

#ifdef DEBUG
        errorexit(cmd, NOEXIT, LOG_DEBUG, 
		  "clientxfrdata: file %s (%ld, %ld)",
		  clientxfrdata.filename, clientxfrdata.bytestotransfer,
		  clientxfrdata.fileoffset);
#endif
        if ( clientxfrdata.filename == NULL ) {
	    serverdata.errCode = ERR(SERVERERR, SSSERROR, INVALIDFILENAME);
	    goto sendreply;
	}

	xfrdata(&clientxfrdata, &serverdata);
	svc_freeargs(transp, xdr_client_xfr, &clientxfrdata);

    } else {
	if ( !svc_getargs(transp, xdr_sgmclnt, &clientdata)) {
	    svcerr_systemerr(transp);
#ifdef USEINETD
            errorexit(cmd, NOEXIT, LOG_ERR, "Error in svc_getargs");
	    exit(1);
#else
	    return;
#endif
	}

	switch (rqstp->rq_proc) {
          case SETAUTHPROC:
#ifdef DEBUG
              errorexit(cmd, NOEXIT, LOG_DEBUG, "Setting Authorization for %s", remHostName);
#endif
              serverdata.errCode = setauth_srvr(remHostName, clientdata.dataptr,
                                                clientdata.datalen);
              break;
          case SGMHNAMECHANGEPROC:
#ifdef DEBUG
              errorexit(cmd, NOEXIT, LOG_DEBUG, "Entering SGM Host Name Change for %s", remHostName);
#endif
	      serverdata.errCode = sgmhname_change(remHostName, clientdata.dataptr,
						clientdata.datalen);
              break;
          case SEMHNAMECHANGEPROC:
#ifdef DEBUG
              errorexit(cmd, NOEXIT, LOG_DEBUG, "Entering SEH Host Name change for %s", remHostName);
#endif
	      serverdata.errCode = semhname_change(remHostName, clientdata.dataptr,
						clientdata.datalen);
	      break;
	  case DELIVERREMOTEPROC:
#ifdef DEBUG
              errorexit(cmd, NOEXIT, LOG_DEBUG, "Remote Event Delivery from %s", remHostName);
#endif
	      serverdata.errCode = deliverevnt_srvr(remHostName,
						    clientdata.dataptr,
						    clientdata.datalen);
	      break;
	  case GETDATAPROC:
#ifdef DEBUG
              errorexit(cmd, NOEXIT, LOG_DEBUG, "Get Data Request from %s", remHostName);
#endif
	      serverdata.errCode = getData_srvr(remHostName,
						&serverdata.data, 
						&serverdata.bytes1, 
						clientdata.dataptr,
						clientdata.datalen);
	      break;
	  case SUBSCRIBEPROC:
#ifdef DEBUG
              errorexit(cmd, NOEXIT, LOG_DEBUG, "Subscription request from %s",remHostName);
#endif
	      serverdata.errCode = subscribe_srvr(remHostName,
                                                  &serverdata.data,
						  &serverdata.bytes1,
						  clientdata.dataptr,
						  clientdata.datalen);
	      break;
          case UNSUBSCRIBEPROC:
#ifdef DEBUG
              errorexit(cmd, NOEXIT, LOG_DEBUG, "UnSubscribe Request from %s", remHostName);
#endif
	      serverdata.errCode = unsubscribe_srvr(remHostName,
                                                  &serverdata.data,
						  &serverdata.bytes1,
						  clientdata.dataptr,
						  clientdata.datalen);
	      break;
	  case CONFIGUPDTPROC:
#ifdef DEBUG
              errorexit(cmd, NOEXIT, LOG_DEBUG, "Entering Config Update");
#endif
	      serverdata.errCode = confupdt_srvr (remHostName,
                                                  &serverdata.data,
						  &serverdata.bytes1,
						  clientdata.dataptr,
						  clientdata.datalen);
	      break;
	  default:
	      svcerr_systemerr(transp);
#ifdef USEINETD
              errorexit(cmd, NOEXIT, LOG_ERR, "Invalid proc received !!");
	      exit(1);
#else
	      return;
#endif
	}
    }

    sendreply:

    if ( serverdata.errCode > 0 ) pSSSErr(serverdata.errCode);

    if ( !svc_sendreply(transp, xdr_sgmsrvr, &serverdata) ) {
	errorexit(cmd, EXIT, LOG_ERR, "Can't reply back to client (%d)",
		  rqstp->rq_proc);
	svcerr_systemerr(transp);
    }
#ifdef USEINETD
    exit(0);
#else
    return;
#endif
}
