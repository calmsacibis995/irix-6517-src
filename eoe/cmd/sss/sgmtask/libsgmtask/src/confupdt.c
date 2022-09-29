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

#ident           "$Revision: 1.1 $"

/*---------------------------------------------------------------------------*/
/* getdata.c                                                                 */
/*   This file contains an internal function of Client, SGMIRPCGetData       */
/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h>
#include <syslog.h>
#include "sgmtask.h"
#include "sgmtaskerr.h"
#include "subscribe.h"
#include <ssdbapi.h>

/*---------------------------------------------------------------------------*/
/* External Declarations                                                     */
/*---------------------------------------------------------------------------*/

static __uint32_t SGMIConfUpdt(char *remoteHost)
{
    sgmtask_t          task;
    __uint32_t         err = 0;


    if ( !remoteHost ) return(0);
    task.datalen = 0;
    task.dataptr = NULL;
    task.rethandle = NULL;

    err = SGMIrpcclient(remoteHost, CONFUPDTPROC, &task);

    if ( !err ) 
	(*retPtr) = task.rethandle;

    return(err);
}

static void PrintDBError(char *location, ssdb_Error_Handle Error)
{
    errorexit(command, NOEXIT, LOG_ERR, "Error in ssdb%s: %s\n",
	      location, ssdbGetLastErrorString(Error));
}

static int execSQLStmt(error, connection, statement, statement_Type)
    ssdb_Error_Handle error;
    ssdb_Connection_Handle connection;
    char *statement;
    int  statement_Type;
{
    ssdb_Request_Handle    Request;
    int                    ErrCode = 0;

    if ( error == NULL || connection == NULL || statement == NULL )
	return(-1);

    if ( !(Request = ssdbSendRequest(error, connection, statement_Type,
				     statement))) {
	ErrCode = ssdbGetLastErrorCode(error);
    } else {
	ssdbFreeRequest(error, Request);
    }

    return(ErrCode);
}


int updateDB(char *file, char *hostname)
{
    FILE   *fp;
    char   table_name[64];
    char   *k;
    char   sqlstmt[1024];
    char   buffer[4096];
    char   sysid[20];
    int    ErrCode = 0;
    const  char  **vector;
    int    reqType;
    ssdb_Error_Handle      Error = 0;
    ssdb_Connection_Handle Connection = 0;
    ssdb_Request_Handle    Request = 0;
    ssdb_Client_Handle     Client = 0;

    if ( file == NULL ) return(0);

    if ( (fp = fopen(file, "r")) == NULL ) {
	errorexit(command, NOEXIT, LOG_ERR, "Can't open %s for reading",
		  file);
        return(0);
    }

    if ( !ssdbInit() ) {
	PrintDBError("Init", Error);
	return(0);
    }

    if ( (Error = ssdbCreateErrorHandle()) == NULL) {
	PrintDBError("ErrorHandle", Error);
	return(0);
    }

    if ( (Client = ssdbNewClient(Error, NULL, NULL, 0)) == NULL ) {
	ErrCode = ssdbGetLastErrorCode(Error);
	goto end;
    }

    if ( !(Connection = ssdbOpenConnection(Error, Client, NULL, SSDB,
			(SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT)))) {
	ErrCode = ssdbGetLastErrorCode(Error);
	goto end;
    }

    sprintf(sqlstmt, "select sys_id from system where hostname = '%s'",
	    hostname);

    if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,
				     sqlstmt))) {
	ErrCode = ssdbGetLastErrorCode(Error);
	goto end;
    }

    if ( !ssdbGetNumColumns(Error, Request) || 
	 !ssdbGetNumRecords(Error, Request) ) {
	PrintDBError("Error in retrieving data for sys_id", Error);
	goto end;
    }

    /* Now, let's get the sys_id */

    ssdbRequestSeek(Error, Request, 0, 0);
    vector = ssdbGetRow(Error, Request);
    if ( vector ) strcpy(sysid, vector[0]);
    ssdbFreeRequest(Error, Request);

    /* OK, we got the sys_id, now, lets read the file and 
       insert the data into DB */


    while ( fgets(buffer, 4096, fp) != NULL ) {
	/* Remove the newline character from buffer */
	k = strrspn(buffer, "\n");
	*k = '\0';
	switch(buffer[0]) {
	    case TABLEDELIMITER:
		k = strtok(&buffer[2], " ");
		strcpy(table_name, k);
		k = strtok(NULL, " ");
		if ( atoi(k) == 0 ) {
		    sprintf(sqlstmt, "delete from %s where sys_id = '%s'",
			    sysid);
		    reqType = SSDB_REQTYPE_DELETE;
		}
		break;
	    case HEADERDELIMITER:
		sprintf("create table %s %s", table_name, &buffer[2]);
		reqType = SSDB_REQTYPE_CREATE;
		break;
	    case RECORDDELIMITER:
		sprintf(sqlstmt, "insert into %s values (%s)", table_name, &buffer[2]);
		reqType = SSDB_REQTYPE_INSERT;
		break;
            default:
		break;
	}

	if ( (ErrCode = execSQLStmt(Error, Connection, sqlstmt, reqType)) < 0 ) {
	    goto end;
	}
    }

    end:
	if ( Connection ) ssdbCloseConnection(Error, Connection);
	if ( Client )     ssdbDeleteClient(Error, Client);
	if ( Error  )     ssdbDeleteErrorHandle(Error);
	ssdbDone();
	if ( fp ) fclose(fp);
	return(ErrCode);
}
