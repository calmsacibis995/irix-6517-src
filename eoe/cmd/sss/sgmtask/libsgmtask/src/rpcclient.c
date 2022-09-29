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
#include <errno.h>
#include <strings.h>
#include <syslog.h>
#include <sys/types.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "sgmtask.h"
#include "sgmtaskerr.h"
#include "sgmrpc.h"
#include "sgmstructs.h"

/*---------------------------------------------------------------------------*/
/* External function prototypes                                              */
/*---------------------------------------------------------------------------*/

extern struct hostent *SGMNETIGetOffHostEnt(char *);
extern int SGMSUBSICheckHostname(char *, char *, int);
extern void SGMIfree(char *, void *);
extern int xdr_sgmclnt(XDR *, sgmclnt_t *);
extern int xdr_client_xfr(XDR *, sgmclntxfr_t *);
extern int xdr_sgmsrvr(XDR *, sgmsrvr_t *);
extern void *SGMIcalloc(size_t, size_t);

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

static int writetoFile(FILE *, sgmsrvr_t *);
__uint32_t SGMIrpcclient(char *, int, sgmtask_t *, int);

static int writetoFile(FILE *fp, sgmsrvr_t *server)
{
    int   num_retries = 0;
    int   tmp = 0;
    int   byteswritten = 0;
    char  *data = server->data;
    int   bytestowrite = server->bytes1;

    while ( num_retries < 10 ) {
        tmp = fwrite(data, 1, server->bytes1, fp);

        if ( tmp > 0 ) {
            byteswritten += tmp;
        }

        if ( byteswritten < bytestowrite ) {
            data += byteswritten;
            bytestowrite -= byteswritten;
        } else break;
        num_retries ++;
    }
    return (byteswritten);
}


/*---------------------------------------------------------------------------*/
/* Name    : SGMIrpcclient                                                   */
/* Purpose : Main entry point for RPC Call                                   */
/* Inputs  : Hostname, Procedure to call and task data structure             */
/* Outputs : 0 or an Error                                                   */
/*---------------------------------------------------------------------------*/

__uint32_t SGMIrpcclient(char *hostname, int rpcproc, sgmtask_t *task, int flag)
{
    struct sockaddr_in                server_addr;
    struct hostent                    *hp;
    struct timeval                    timeout;
    enum   clnt_stat                  clnt_stat;
    FILE                              *fp = NULL;
    int                               socket = RPC_ANYSOCK;
    register CLIENT                   *client;
    sgmclnt_t                         sgmclnt = {0, 0, NULL};
    sgmsrvr_t                         sgmsrvr = {0, 0, 0, 0, 0, NULL, NULL};
    __uint32_t                        err = 0;
    char                              localfilename[25];
    sgmclntxfr_t                      clientxfr;
    int                               serverfileoffset = 0;
    sgmtaskrd_t                       *retdata = NULL;
    struct  stat                      statb;

    if ( hostname == NULL ) 
	return(ERR(CLIENTERR, PARAMERR, INVALIDHOSTNAME));
    
    if ( task == NULL ) 
	return(ERR(CLIENTERR, PARAMERR, INVALIDTASKPTR));
    else if (task->dataptr == NULL && task->datalen != 0 ) {
	return(ERR(CLIENTERR, PARAMERR, INVALIDDATAPTR));
    } else if ( task->dataptr != NULL && task->datalen == 0 ) {
	return(ERR(CLIENTERR, PARAMERR, INVALIDDATAPTR));
    }  

    if ( rpcproc != PROCESSPROC && rpcproc != XMITPROC && rpcproc != GETDATAPROC &&
	 rpcproc != CONFIGUPDTPROC && rpcproc != SUBSCRIBEPROC && 
	 rpcproc != UNSUBSCRIBEPROC && rpcproc != DELIVERREMOTEPROC  &&
         rpcproc != SETAUTHPROC && rpcproc != SGMHNAMECHANGEPROC &&
         rpcproc != SEMHNAMECHANGEPROC )
        return(ERR(CLIENTERR, PARAMERR, INVALIDPROC));

    /*-----------------------------------------------------------------------*/
    /*  Now that all data passed is OK, lets go ahead and start our          */
    /*  process of sending data to server.  We first create a TCP            */
    /*  RPC Client and set the timeout parameters.                           */
    /*-----------------------------------------------------------------------*/

    if ( (hp = (struct hostent *) SGMNETIGetOffHostEnt(hostname)) == NULL ) {
	return(ERR(CLIENTERR, NETERR, CANTGETHOSTINFO));
    }

    bcopy(hp->h_addr, (caddr_t) &server_addr.sin_addr, hp->h_length);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = 0;

    if ( (client = clnttcp_create(&server_addr, RPCPROGRAM, RPCVERSIONS,
#if 0
				  &socket, MAXBUFSIZE, MAXBUFSIZE)) == NULL ) {
#endif
				  &socket, 0, 0)) == NULL ) {
	return(ERR(CLIENTERR, RPCERR, RPCTCPPROTOINITERR));
    }

    client->cl_auth = authnone_create();

    if ( rpcproc == DELIVERREMOTEPROC ) 
        timeout.tv_sec  = 10;
    else 
        timeout.tv_sec  = DEFAULTTIMEOUT;

    timeout.tv_usec = 0;

    clnt_control(client, CLSET_TIMEOUT, &timeout);

    /*-----------------------------------------------------------------------*/
    /*  Now, populate the sgmclnt structure that we need to send.            */
    /*  This is the first structure that is always sent.                     */
    /*-----------------------------------------------------------------------*/

    sgmclnt.flag = flag;
    sgmclnt.datalen = task->datalen;
    sgmclnt.dataptr = task->dataptr;

    /*-----------------------------------------------------------------------*/
    /*  Now, Call the server to service our RPC request                      */
    /*-----------------------------------------------------------------------*/

    clnt_stat = clnt_call(client, rpcproc, xdr_sgmclnt,
			  &sgmclnt, xdr_sgmsrvr, &sgmsrvr, timeout);

    if ( clnt_stat != RPC_SUCCESS ) {
	err = ERR(CLIENTERR, RPCERR, clnt_stat);
	goto end;
    } else if (sgmsrvr.errCode != 0 ) {
        err = sgmsrvr.errCode;
	goto end;
    }

    if ( sgmsrvr.bytes1 != 0 ) {
        /*-------------------------------------------------------------------*/
        /*  sgmsrvr.data has bytes1 bytes of data. Read it and pass it       */
        /*  to the calling program to interpret it.                          */
        /*-------------------------------------------------------------------*/

	if ( (retdata = SGMIcalloc(1, sizeof(struct sgmtaskrd_s))) 
		      == NULL ) {
	    err = ERR(CLIENTERR, OSERR, MALLOCERR);
	    goto end;
        }

	retdata->flag = 0;
	retdata->datalen = sgmsrvr.bytes1;

	if ( (retdata->dataptr = SGMIcalloc(sgmsrvr.bytes1, 1)) == NULL ) {
	    err = ERR(CLIENTERR, OSERR, MALLOCERR);
	    SGMIfree("RPCClient:1", retdata);
	    goto end;
	} else {
	    memcpy(retdata->dataptr, sgmsrvr.data, sgmsrvr.bytes1);
	}

	task->rethandle = retdata;

    } else if ( sgmsrvr.flag & ISFILE ) {
        /*-------------------------------------------------------------------*/
        /*  sgmsrvr.data has a filename that the server created on its       */
        /*  system.  We need to transfer data and store it in out local      */
        /*  file.  Lets do it.                                               */
        /*-------------------------------------------------------------------*/


	/* Server has a lot of data to send.  Lets open a temp. file */
	/* and store the data in it.                                 */

        if ( stat("/tmp", &statb) == 0 ) {
	    snprintf(localfilename, 25, "/tmp/SgMcXXXXXX");
	} else if ( stat("/var/tmp", &statb) == 0 ) {
	    snprintf(localfilename, 25, "/var/tmp/SgMcXXXXXX");
	} else {
	    err = ERR(CLIENTERR, OSERR, errno);
	    goto end;
	}

	mktemp(localfilename);

	if ( (fp = fopen(localfilename, "w")) == NULL ) {
	    err = ERR(CLIENTERR, OSERR, errno);
	    goto end;
	}

        clientxfr.filename = strdup(sgmsrvr.filename);

	while ( sgmsrvr.bytes2 != 0 ) {
	    clientxfr.bytestotransfer = ( ( sgmsrvr.bytes2 >= MAXMEMORYALLOC )
					  ? MAXMEMORYALLOC : sgmsrvr.bytes2);
	    clientxfr.fileoffset = serverfileoffset;
	    clnt_freeres(client, xdr_sgmsrvr, &sgmsrvr);

	    clnt_stat = clnt_call(client, XMITPROC, xdr_client_xfr, 
				  &clientxfr, xdr_sgmsrvr, &sgmsrvr, timeout);

	    if ( clnt_stat != RPC_SUCCESS )  {
		err = ERR(CLIENTERR, RPCERR, clnt_stat);
	    } else if ( sgmsrvr.errCode != 0 ) {
		err = sgmsrvr.errCode;
	    } else err = 0;

	    if ( err ) {
		if ( fp ) fclose(fp);
		unlink(localfilename);
		break;
	    } else {
		serverfileoffset += writetoFile(fp, &sgmsrvr);
	    }
	}

	SGMIfree("RPCClient:2", clientxfr.filename);
	if ( fp ) fclose(fp);
	if ( err ) goto end;

        /*-------------------------------------------------------------------*/
        /*  Populate structure that is needed by calling function.The dataptr*/
        /*  going to be the filename we created. We also set the flag so that*/
        /*  the calling proc. knows that it needs to open a file.            */
        /*-------------------------------------------------------------------*/

	if ( (retdata = SGMIcalloc(1, sizeof(struct sgmtaskrd_s))) 
		      == NULL ) {
	    err = ERR(CLIENTERR, OSERR, MALLOCERR);
	    goto end;
        }

	retdata->flag |= ISFILE;
        if ( (retdata->dataptr = SGMIcalloc(strlen(localfilename)+1, 1)) == NULL ) {
	    err = ERR(CLIENTERR, OSERR, MALLOCERR);
	    SGMIfree("RPCClient:3", retdata);
	    goto end;
	} 

	retdata->datalen = strlen(localfilename)+1;
	strcpy(retdata->dataptr, localfilename);

    } else {
	/* successful operation.  Return 0 */
	goto end;
    }

    end:
        auth_destroy(client->cl_auth);
	clnt_freeres(client, xdr_sgmsrvr, &sgmsrvr);
	clnt_destroy(client);
	return(err);
}

