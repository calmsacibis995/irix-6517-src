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

/*---------------------------------------------------------------------------*/
/* getdata.c                                                                 */
/*   This file contains an internal function of Client, SGMIRPCGetData       */
/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include "sgmtask.h"
#include "sgmtaskerr.h"
#include "subscribe.h"
#include "sgmauth.h"

/*---------------------------------------------------------------------------*/
/* External Declarations                                                     */
/*---------------------------------------------------------------------------*/

extern void *SGMIcalloc(size_t, size_t);
extern int pack_data(char **, char *, int, void *, int, void *, int, int);
extern int unpack_data(char *, int, void **, int, void **, char *, int, int);
extern void SGMIfree(char *, void *);
extern int SGMAUTHISetAuth(char *, char *, int);
extern int SGMAUTHIGetAuth(char *, char *, int *, int);
extern __uint32_t SGMIrpcclient(char *, int, sgmtask_t *, int);
extern int SGMSSDBGetSystemDetails(sysinfo_t *, int, int);

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

__uint32_t SGMIRPCGetData(char *, char *, char **);
__uint32_t SGMAUTHIUpdateAuth(char *);

__uint32_t SGMAUTHIUpdateAuth(char *remoteHost)
{
    sgmtask_t          task;
    __uint32_t         err = 0;
    char               authStr[AUTHSIZE];
    int                authlen = 0;
    int                authType = 0;
    int                NewAuthType = STANDARD_AUTH|FULLENCR;
    sysinfo_t          sysinfo;
    char               junk[50];

    if ( !remoteHost ) return(ERR(CLIENTERR, PARAMERR, INVALIDARGS));

    if ( (err = SGMSSDBGetSystemDetails(&sysinfo, 1, 1)) != 0 ) {
        return(SSSERR(CLIENTERR, SSDBERR, err));
    } else {
	snprintf(junk, sizeof(junk), "SYSID:%s", sysinfo.sys_id);
    }

    if ( (authlen = SGMAUTHIGetAuth(remoteHost, authStr, &authType,
				    CREATEAUTHIFNOTFOUND)) == 0) {
	return(ERR(CLIENTERR, AUTHERR, CANTCREATEAUTHINFO));
    }

    task.datalen = pack_data((char **)&task.dataptr, authStr, authlen, junk,
			     strlen(junk), NULL, 0, NewAuthType);
    task.rethandle = NULL;
    err = SGMIrpcclient(remoteHost, SETAUTHPROC, &task, NewAuthType);
    SGMIfree("UpdateAuth:1", task.dataptr);

    if ( err && NewAuthType == authType ) {
	    SGMAUTHISetAuth(remoteHost, authStr, DELETEAUTH);
    }

    return(err);
}

/*---------------------------------------------------------------------------*/
/* Name    : SGMIRPCGetData                                                  */
/* Purpose : Is to return data from the remote host for a given sql string   */
/* Inputs  : Remote Hostname, Sql String and pointer to return data          */
/* Outputs : Error or 0 for success                                          */
/*---------------------------------------------------------------------------*/

__uint32_t SGMIRPCGetData(char *remoteHost, char *sqlstr, 
						   char **retPtr)
{
    sgmtask_t          task = { 0, 0, 0 };
    __uint32_t         err = 0;
    char               authStr[AUTHSIZE];
    int                authlen = 0;
    int                authType = 0;
    sgmtaskrd_t        *retdata;
    char               *mydata = NULL;

    if ( !sqlstr || !remoteHost ) return(ERR(CLIENTERR, PARAMERR,INVALIDARGS));

    /*-----------------------------------------------------------------------*/
    /*  Lets try to get Authorization info. for remote Host.  We pass a      */
    /*  flag, CREATEAUTHIFNOTFOUND, which lets SGMAUTHIGetAuth create a      */
    /*  new authinfo if it is not found in SSDB.  If a new auth has been     */
    /*  created, which we can determine by means of authType, then,          */
    /*  we first send auth.info to server before sending any other request   */
    /*-----------------------------------------------------------------------*/

    if ( (authlen = SGMAUTHIGetAuth(remoteHost, authStr, &authType,
                                   0) ) == 0) {
	return(ERR(CLIENTERR, AUTHERR, CANTGETAUTHINFO));
    }

    /*-----------------------------------------------------------------------*/
    /*  Now, send the SQLSTR and get results from Server                     */
    /*-----------------------------------------------------------------------*/

    task.datalen = pack_data((char **)&task.dataptr, authStr, authlen, sqlstr, 
                             strlen(sqlstr)+1, NULL, 0, 0);
    if ( !task.datalen ) return(ERR(CLIENTERR, AUTHERR, CANTPACKDATA));
    task.rethandle = NULL;

    err = SGMIrpcclient(remoteHost, GETDATAPROC, &task, 0);
    SGMIfree("GetDataRPC:1", task.dataptr);
    task.dataptr = NULL;

    retdata = (sgmtaskrd_t *) task.rethandle;

    if ( err ) return(err);
    else if ( !retdata ) {
	return(ERR(SERVERERR, SSSERROR, MALLOCERR));
    } else if ( !retdata->dataptr || !retdata->datalen ) {
	SGMIfree("GetDataRPC:2", retdata);
	return(ERR(SERVERERR, SSSERROR, MALLOCERR));
    }

    if ( !unpack_data(authStr, authlen, (void **) &mydata, 0, NULL,
		      retdata->dataptr, retdata->datalen, 0)) {
	SGMIfree("GetDataRPC:3", retdata->dataptr);
	SGMIfree("GetDataRPC:4", retdata);
	return(ERR(CLIENTERR, RPCERR, CLOBBEREDDATA));
    }

    /* At this point, mydata will have a character string that represents
       the data we got from remote host.  Lets allocate memory for 
       it and copy it over
    */

    if ( ((*retPtr) = SGMIcalloc(retdata->datalen-authlen, 1)) != NULL ) {
	memcpy( (*retPtr), mydata, retdata->datalen-authlen);
    }

    SGMIfree("GetDataRPC:5", retdata->dataptr);
    SGMIfree("GetDataRPC:6", retdata);
    return(0);
}
