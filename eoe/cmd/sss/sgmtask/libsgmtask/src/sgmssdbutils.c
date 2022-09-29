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

#ident           "$Revision: 1.12 $"

/*---------------------------------------------------------------------------*/
/* sgmtask_ssdb.c                                                            */
/*   This file contains all SSDB related calls that are required by          */
/*   subscribe and unsubscribe.                                              */
/*---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Include files                                                             */
/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <syslog.h>
#include <ssdbapi.h>
#include <ssdberr.h>
#include "subscribe.h"
#include "sgmtaskerr.h"
#include "sgmtask.h"

extern void *SGMIcalloc(size_t,size_t);
extern char *SGMNETIGetHostIP(char *);

static int                      ssdb_init_done = 0;
static ssdb_Client_Handle       Client = 0;
static ssdb_Error_Handle        Error  = 0;
static ssdb_Connection_Handle   Connection = 0;

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

__uint32_t SGMSSDBInit(int flag)
{
    int          ssdberr = 0;

    if ( ssdb_init_done ) return(0);

    if ( flag ) {
	if ( !ssdbInit() ) {
	    ssdberr = ssdbGetLastErrorCode(NULL);
	    if ( ssdberr != SSDBERR_ALREADYINIT1  || ssdberr != SSDBERR_ALREADYINIT2 ) 
		return((ssdberr > 0 ? ssdberr : -1*ssdberr));
	}
    }

    if ( (Error = ssdbCreateErrorHandle()) == NULL ) {
	ssdberr = ssdbGetLastErrorCode(NULL);
	return((ssdberr > 0 ? ssdberr : -1*ssdberr));
    }

    if ( (Client = ssdbNewClient(Error, SSDBUSER, SSDBPASSWORD, 0))
                 == NULL ) {
	ssdberr = ssdbGetLastErrorCode(NULL);
	if (Error) ssdbDeleteErrorHandle(Error);
	Error = 0;
	return((ssdberr > 0 ? ssdberr : -1*ssdberr));
    }

    if ( !(Connection = ssdbOpenConnection(Error, Client,
                        NULL, SSDATABASE, (SSDB_CONFLG_RECONNECT |
                        SSDB_CONFLG_REPTCONNECT))) ) {
	ssdberr = ssdbGetLastErrorCode(NULL);
	if (Client) ssdbDeleteClient(Error, Client);
	if (Error) ssdbDeleteErrorHandle(Error);
	Client = 0;
	Error = 0;
	return((ssdberr > 0 ? ssdberr : -1*ssdberr));
    }

    ssdb_init_done = 1;
    return(0);
}

void SGMSSDBDone(int flag)
{

    if ( !ssdb_init_done ) return;
    if ( Connection ) ssdbCloseConnection(Error, Connection);
    if (Client) ssdbDeleteClient(Error, Client);
    if (Error) ssdbDeleteErrorHandle(Error);
    Error = Connection = Client = 0;
    ssdb_init_done = 0;
    if ( flag ) ssdbDone();
    return;
}

/*---------------------------------------------------------------------------*/
/* Name    : SGMSSDBSetSystemDetails                                         */
/* Purpose : Sets SYSINFO data from SSDB                                     */
/* Inputs  : sysinfo structure, hostname and procedure (1 or 0)              */
/* Outputs : Error returned by SSDBAPI                                       */
/*---------------------------------------------------------------------------*/

int SGMSSDBSetSystemDetails(char *hostname, sysinfo_t *sys, int procedure)
{
    ssdb_Request_Handle        Request = 0;
    int                        ErrCode = 0;
    int                        reqtype = 0;
    char                       sqlstr[1024];

    if ( !sys ) return(UNKNOWNSSDBERR);

    if ( procedure == 1 ) {
	snprintf(sqlstr, sizeof(sqlstr), "insert into system values (0, '%s', %d,'%s','%s','%s',0,0,%d)",
		 sys->sys_id, sys->sys_type, sys->sys_serial, hostname, 
		 SGMNETIGetHostIP(hostname), time(0));
	reqtype = SSDB_REQTYPE_INSERT;
    } else {
	snprintf(sqlstr, sizeof(sqlstr), 
	"update system set hostname = '%s',ip_addr = '%s', sys_type = %d, sys_serial = '%s' where sys_id = '%s'",
	 hostname, SGMNETIGetHostIP(hostname), sys->sys_type, sys->sys_serial, sys->sys_id);
	reqtype = SSDB_REQTYPE_UPDATE;
    }

    if ( reqtype ) {
	if ( !ssdbLockTable(Error, Connection, "system") ) {
            ErrCode = ssdbGetLastErrorCode(Error);
            goto end;
        }
	if ( !(Request = ssdbSendRequest(Error, Connection,reqtype,sqlstr))) {
	    ErrCode = ssdbGetLastErrorCode(Error);
	} else {
	    ssdbFreeRequest(Error, Request);
	}
	ssdbUnLockTable(Error, Connection);
    }

    end:
    return((ErrCode < 0) ? -1*ErrCode : ErrCode);
}

/*---------------------------------------------------------------------------*/
/* Name    : SGMSSDBGetSystemDetails                                         */
/* Purpose : Gets SYSINFO data from SSDB                                     */
/* Inputs  : sysinfo structure, active and local that are required           */
/* Outputs : Error returned by SSDBAPI                                       */
/*---------------------------------------------------------------------------*/

int SGMSSDBGetSystemDetails(sysinfo_t *sys, int active, int local)
{
    ssdb_Request_Handle		Request = 0;
    int				NumRows;
    int				ErrCode = 0;
    const char                  **vector;

    if (!sys) return UNKNOWNSSDBERR;

    /*-----------------------------------------------------------------------*/
    /*   Lets send our select statement to SSDB.  If SSDB didn't return any  */
    /*   records, then we have an error. Otherwise, read data from SSDB.     */
    /*-----------------------------------------------------------------------*/

    if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,
            "select sys_id,sys_type,sys_serial,hostname,ip_addr from system where active=%d and local=%d",
                    active, local))) {
        goto end;
    }


    if ( (NumRows = ssdbGetNumRecords(Error, Request)) == 0 ) {
	if ( Request ) ssdbFreeRequest(Error, Request);
	return(NORECORDSFOUND);
    }

    ssdbRequestSeek(Error, Request, 0, 0);
    vector = ssdbGetRow(Error, Request);
    if ( vector ) {
	strcpy(sys->sys_id, vector[0]);
	sys->sys_type = atoi(vector[1]);
	strcpy(sys->sys_serial, vector[2]);
	strcpy(sys->hostname, vector[3]);
	strcpy(sys->ip_addr, vector[4]);
    }

    end:
        ErrCode = ssdbGetLastErrorCode(Error);
        if (Request) ssdbFreeRequest(Error, Request);
        return((ErrCode < 0) ? -1*ErrCode : ErrCode);
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*   Gets data in table.                                                     */
/*---------------------------------------------------------------------------*/

int SGMSSDBCheckTableData(char *sqlstr, int *nrows)
{
    ssdb_Request_Handle     Request = 0;
    int                     NumRows = 0;
    int                     ErrCode = 0;    

    if ( !sqlstr || !nrows ) return(UNKNOWNSSDBERR);
    if ( (Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,
                                     sqlstr))) {
	NumRows = ssdbGetNumRecords(Error, Request);
	ssdbFreeRequest(Error, Request);
	*nrows = NumRows;
    } else {
	ErrCode = ssdbGetLastErrorCode(Error);
    }

    return((ErrCode > 0 ? ErrCode : -1*ErrCode));
}

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*   Sets data in table.                                                     */
/*---------------------------------------------------------------------------*/

int SGMSSDBSetTableData(char *sqlstr, char *table, int reqType)
{

    ssdb_Request_Handle      Request = 0;
    int                      ErrCode = 0;

    if ( !sqlstr || !table ) return(1);

    /*-----------------------------------------------------------------------*/
    /*   Lets send our sqlstr statement to SSDB. If SSDB didn't return any   */
    /*   any error, we are all ok.                                           */
    /*-----------------------------------------------------------------------*/

    if ( !ssdbLockTable(Error, Connection, table) ) goto end;
    if ( !(Request = ssdbSendRequest(Error, Connection, reqType,
                                     sqlstr))) {
        goto end;
    }

    end:
        ErrCode = ssdbGetLastErrorCode(Error);
        ssdbUnLockTable(Error, Connection);
        if (Request) ssdbFreeRequest(Error, Request);
        return((ErrCode > 0 ? ErrCode : -1*ErrCode));
}

/*---------------------------------------------------------------------------*/
/* Name    : SGMSSDBGetTableData                                             */
/* Purpose : A generic function that returns data pertaining to sql stmt.    */
/* Inputs  : Sql String, Pointer to return data, pointers to rows & cols     */
/* Outputs : Error code returned by SSDBAPI                                  */
/*---------------------------------------------------------------------------*/

int SGMSSDBGetTableData(char *sqlstr, char **retPtr, int *nrows, int *ncols, 
                        char *rowdel, char *coldel)
{
    ssdb_Request_Handle      Request = 0;
    int                      NumCols = 0, NumRows = 0, i, j, offset = 0;
    int                      ErrCode = 0;
    char                     *tmpPtr = NULL;
    const char               **vector;
    int                      totBytes = 0;
    char                     rowDelimiter[5];
    char                     colDelimiter[5];
    int                      rowdelsize = 0, coldelsize = 0;


    if ( !sqlstr || !retPtr || !nrows || !ncols ) return 1;

    memset((void *) rowDelimiter, 0, 5);
    memset((void *) colDelimiter, 0, 5);

    if ( rowdel != NULL ) {
	rowdelsize = strlen(rowdel);
	rowdelsize = (rowdelsize > 4 ? 4 : rowdelsize);
	for ( totBytes = 0; totBytes < rowdelsize; totBytes++) 
	    rowDelimiter[totBytes] = *(rowdel+totBytes);
    } else {
	rowdelsize = 1;
	rowDelimiter[0]='@';
    }

    if ( coldel != NULL ) {
	coldelsize = strlen(coldel);
	coldelsize = (coldelsize > 4 ? 4 : coldelsize);
	for ( totBytes = 0; totBytes < coldelsize; totBytes++) 
	    colDelimiter[totBytes] = *(coldel+totBytes);
    } else {
	coldelsize = 1;
	colDelimiter[0]='|';
    }

    totBytes = 0;

    /*-----------------------------------------------------------------------*/
    /*   Lets send our select statement to SSDB.  If SSDB didn't return any  */
    /*   records, then we have an error. Otherwise, read data from SSDB.     */
    /*-----------------------------------------------------------------------*/

    if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT, 
				     sqlstr))) {
	goto end;
    }

    if ( (NumRows = ssdbGetNumRecords(Error, Request)) == 0 ) {
	goto end;
    }

    if ( (NumCols = ssdbGetNumColumns(Error, Request)) == 0 ) {
	goto end;
    }

    for ( i = 0; i < NumCols; i++ ) {
	int tmpBytes;

	if ( (tmpBytes = ssdbGetFieldMaxSize(Error, Request, i)) == 0 ) {
	    goto end;
	} else {
	    totBytes += tmpBytes + coldelsize;
	}
    }

    totBytes = (totBytes+rowdelsize)*NumRows + 100;
    offset   = 0;

    if ( (tmpPtr = (char *) SGMIcalloc(totBytes, 1)) == NULL ) {
	goto end;
    } else {
	ssdbRequestSeek(Error, Request, 0, 0);
	for ( i = 0; i < NumRows; i++ ) {
	    vector = ssdbGetRow(Error, Request);
	    if (vector) {
		for ( j = 0; j < NumCols; j++ ) {
		    offset += snprintf(tmpPtr+offset, totBytes-offset,"%s%s",
				       (j == 0 ? rowDelimiter:colDelimiter),vector[j]);
		}
	    }
	}

	if ( offset+1 < totBytes ) *(tmpPtr+offset+1) = '\0';
	(*retPtr) = tmpPtr;
	*nrows = NumRows;
	*ncols = NumCols;
        ErrCode = 0;
	goto end1;
    }

    end:
	ErrCode = ssdbGetLastErrorCode(Error);
	*nrows = *ncols = 0;
	*retPtr = NULL;
    end1:
	if (Request) ssdbFreeRequest(Error, Request);
	return((ErrCode > 0 ? ErrCode : -1*ErrCode));
}

/*---------------------------------------------------------------------------*/
/* SGMCLNTSSDBif                                                             */
/*  Operations that are performed on SGM side.                               */
/*---------------------------------------------------------------------------*/

__uint32_t  SGMCLNTSSDBif(char *offsemhname, subscribe_t *semsub, class_data_t *data, int func, int opts)
{
    ssdb_Request_Handle     Request = 0;
    int                     ErrCode = 0;
    char                    sqlstr[1024];
    class_data_t            *tmpData = data;
    int                     reqtype = 0;

    if ( !semsub || !offsemhname ) 
        return ERR(SERVERERR, PARAMERR, INVALIDARGS);

    /*-----------------------------------------------------------------------*/
    /*  If we get a clean-up request and the options are NEWHOST, then, some */
    /*  error happened in Phase 2 of Subscribe.  So, we need to remove the   */
    /*  entry we made in System Table.  In all other cases, we               */
    /*  insert into system table if opts contain NEWHOST.                    */
    /*                                                                       */
    /*  A point to be noted here is we disregard the hostname that SEM had   */
    /*  sent us.  We always insert the name that we recognize as SEM hostname*/
    /*  from our machine.  This is because all authentication is based on    */
    /*  the hostname that we recognize as the SEM hostname not what SEM      */
    /*  recognizes as its own hostname.                                      */
    /*-----------------------------------------------------------------------*/

    if ( opts & NEWHOST ) {
	if ( func == CLEANUP ) {
	    reqtype = SSDB_REQTYPE_DELETE;
	    sprintf(sqlstr, "delete from system where sys_id = '%s'",
		    semsub->sysinfo.sys_id);
	} else {
	    if ( (ErrCode = SGMSSDBSetSystemDetails(offsemhname, &semsub->sysinfo, 1)) != 0 ) {
		goto end1;
	    }
            reqtype = SSDB_REQTYPE_UPDATE;
	    sprintf(sqlstr, "update system set active = 1,local = 0 where sys_id = '%s'",
                 semsub->sysinfo.sys_id);
	}
    } else if ( opts & ENBLHOST ) {
        if ( func != CLEANUP ) {
	    reqtype = SSDB_REQTYPE_UPDATE;
	    sprintf(sqlstr, "update system set active = 1,local = 0 where sys_id = '%s'",
		    semsub->sysinfo.sys_id);
	}
    } else if ( opts & DSBLHOST ) {
	reqtype = SSDB_REQTYPE_UPDATE;
	sprintf(sqlstr, "update system set active = 0 where sys_id = '%s'",
		 semsub->sysinfo.sys_id);
    }

    /*-----------------------------------------------------------------------*/
    /*   Now, send request to SSDB.  Before sending it, Lock System Table.   */
    /*-----------------------------------------------------------------------*/

    if ( reqtype ) {
        if ( !ssdbLockTable(Error, Connection, "system") ) goto end;
        if ( !(Request = ssdbSendRequest(Error, Connection,reqtype,sqlstr))) {
	    ErrCode = ssdbGetLastErrorCode(Error);
	    ssdbUnLockTable(Error, Connection);
	    goto end1;
        } else {
	    ssdbFreeRequest(Error, Request);
        }
        if ( !ssdbUnLockTable(Error, Connection) ) goto end;
    }

    if ( !func ) goto end;
    while (tmpData != NULL) {
	type_data_t   *tmpTypeData = tmpData->type;

        /*-------------------------------------------------------------------*/
        /*   We get all the classes & types to subscribe/unsubscribe in a    */
        /*   linked list.  Classes are in a linked list and each class has   */
        /*   the types associated with it in the linked list.                */
        /*   We first need to check and insert the Class description in SSDB.*/
        /*   We insert it only in case of a SUBSCRIBE. Before inserting it,  */
        /*   lets first delete it to make sure that there is no duplication. */
        /*-------------------------------------------------------------------*/

        if ( func == SUBSCRIBE ) {
	    if ( !(Request = ssdbSendRequestTrans(Error, Connection,
		             SSDB_REQTYPE_DELETE, "event_class",
			     "delete from event_class where sys_id = '%s' "
			     "and class_id = %d",
			     semsub->sysinfo.sys_id, tmpData->class_id))) {
		ErrCode = ssdbGetLastErrorCode(Error);
		goto end1;
            } else {
		ssdbFreeRequest(Error, Request);
	    }

	    if ( !(Request = ssdbSendRequestTrans(Error, Connection,
			     SSDB_REQTYPE_INSERT, "event_class",
			     "insert into event_class values('%s',%d,\"%s\")",
			     semsub->sysinfo.sys_id, tmpData->class_id,
			     tmpData->class_desc))) {
		ErrCode = ssdbGetLastErrorCode(Error);
		goto end1;
	    } else {
		ssdbFreeRequest(Error, Request);
	    }
	}

        /*-------------------------------------------------------------------*/
        /*   Now, insert the types from type linked list of class            */
        /*-------------------------------------------------------------------*/

        if ( !ssdbLockTable(Error, Connection, "event_type") ) goto end;
	while ( tmpTypeData != NULL ) {
	    if ( func == CLEANUP || func == SUBSCRIBE ) {
		sprintf(sqlstr, "delete from event_type where sys_id ='%s' and type_id = %d and class_id = %d",
			semsub->sysinfo.sys_id, tmpTypeData->type_id, tmpData->class_id);
		if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_DELETE,
						 sqlstr))) {
                    ErrCode = ssdbGetLastErrorCode(Error);
		    ssdbUnLockTable(Error, Connection);
		    goto end1;
                } else {
		    ssdbFreeRequest(Error, Request);
		}
	    }

	    if ( func == SUBSCRIBE ) {
		if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_INSERT,
		       "insert into event_type values('%s',%d,%d,\"%s\",1,0,0,-1,1)",
		       semsub->sysinfo.sys_id, tmpTypeData->type_id,
		       tmpData->class_id, tmpTypeData->type_desc))) {
		    ErrCode = ssdbGetLastErrorCode(Error);
		    ssdbUnLockTable(Error, Connection);
		    goto end1;
                } else {
		    ssdbFreeRequest(Error, Request);
		}
	    }

	    if ( func == UNSUBSCRIBE ) {
		if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_UPDATE,
		       "update event_type set enabled = 0 where sys_id ='%s' and type_id = %d and class_id = %d",
		       semsub->sysinfo.sys_id, tmpTypeData->type_id, tmpData->class_id))) {
		    ErrCode = ssdbGetLastErrorCode(Error);
		    ssdbUnLockTable(Error, Connection);
		    goto end1;
                } else {
		    ssdbFreeRequest(Error, Request);
		}
	    }
	    tmpTypeData = tmpTypeData->next;
	}
        if ( !ssdbUnLockTable(Error, Connection) ) goto end;
	tmpData = tmpData->next;
    }

    end:
        ErrCode = ssdbGetLastErrorCode(Error);
    end1:
        if (Request) ssdbFreeRequest(Error, Request);
        return((ErrCode < 0) ? -1*ErrCode : ErrCode);
}

/*---------------------------------------------------------------------------*/
/* SGMSSDBInsertSysInfoChangeAction                                          */
/*   During subscription,we need to capture information about hostname changes */
/*   otherwise, authentication willnot work.  For this purpose, we will insert*/
/*   an action in our local database which calls confupdt and pulls relevent */
/*   info. from remote host                                                  */
/*---------------------------------------------------------------------------*/

int SGMSSDBInsertSysInfoChangeAction(char *sys_id, char *compare, char *insertstring, char *optstring)
{
    ssdb_Request_Handle req = 0;
    const char **vector;
    int   NumRows = 0;
    int   NumCols = 0;
    int   action_id = 0;
    char  sqlstr[1024];

    if (!sys_id || !compare ) return(0);

    snprintf(sqlstr, sizeof(sqlstr), 
	 "select action_id from event_action where action like '%s'", compare);

    if ( !(req = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,sqlstr))) {
	    return(0);
    }

    if ( (NumRows = ssdbGetNumRecords(Error, req)) == 0 ) {
	ssdbFreeRequest(Error, req);
	if ( !insertstring || !optstring ) return(0);
        if ( !ssdbLockTable(Error, Connection, "event_action")) goto end;
	if ( !(req = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_INSERT,
	       "insert into event_action values('%s',NULL, '', 1, 0, '%s',2,300,'root', '%s', 1,0)",
	       sys_id, insertstring, optstring))) {
	    ssdbUnLockTable(Error, Connection);
	    ssdbFreeRequest(Error, req);
	    return(0);
	}
	ssdbUnLockTable(Error, Connection);
	ssdbFreeRequest(Error, req);
	snprintf(sqlstr, sizeof(sqlstr), 
	    "select action_id from event_action where action like '%s'", compare);
	if ( !(req = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,sqlstr))) {
	    return(0);
	}
    }

    ssdbRequestSeek(Error, req, 0, 0);
    vector = ssdbGetRow(Error, req);
    if ( vector ) {
	action_id = atoi(vector[0]);
    }

    end:
	ssdbFreeRequest(Error, req);
	return(action_id);
}

/*---------------------------------------------------------------------------*/
/* SGMSSDBInsertSysInfoChange                                                */
/*  Once subscription is complete, we will insert an action in               */
/*  event_action table and event_actionref table that links any              */
/*  SYSINFO CHANGE events that are logged by ConfigMon. This                 */
/*  will enable SGM to send its own hostname changes to subscribed           */
/*  SEMs.                                                                    */
/*---------------------------------------------------------------------------*/

int SGMSSDBInsertSysInfoChange(char *mysysid)
{
    ssdb_Request_Handle    req = 0;
    int                    a_id = 0;

    if ( !mysysid ) return(0);

    a_id = SGMSSDBInsertSysInfoChangeAction(mysysid, "/usr/etc/confupdt -c%%", 
			"/usr/etc/confupdt -c -I %I %H", "Notify SysInfo Change");
    if ( a_id == 0 ) return(0);

    if ( !ssdbLockTable(Error, Connection, "event_actionref")) return(0);
    if ( !(req = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_DELETE,
		 "delete from event_actionref where sys_id = '%s' and class_id = %d and type_id = %d and action_id = %d",
		 mysysid, CONFIGMONCLASS, SYSINFOCHANGE, a_id))) {
        ssdbUnLockTable(Error, Connection);
	return(0);
    }
    ssdbFreeRequest(Error, req);
    if ( !(req = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_INSERT,
		 "insert into event_actionref values('%s', NULL, %d, %d, %d)", 
		 mysysid, CONFIGMONCLASS, SYSINFOCHANGE, a_id))) {
	ssdbUnLockTable(Error, Connection);
	return(0);
    }
    ssdbUnLockTable(Error, Connection);
    ssdbFreeRequest(Error, req);

    return(1);
}

/*---------------------------------------------------------------------------*/
/* SGMSSDBGetActionID                                                        */
/*  A function that is called from various places to get the action          */
/*  ID for host                                                              */
/*---------------------------------------------------------------------------*/

int SGMSSDBGetActionID(char *field_name, char *sys_id)
{
    ssdb_Request_Handle req = 0;
    const char **vector;
    int   NumRows = 0;
    int   a_id = 0;

    if ( (req = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,
                     "select action_id from event_action where %s = '%s'",
                     field_name, sys_id) )) {
        if ( (NumRows = ssdbGetNumRecords(Error, req)) == 0 ) {
            ssdbFreeRequest(Error, req);
            return(0);
        }

        ssdbRequestSeek(Error, req, 0, 0);
        vector = ssdbGetRow(Error, req);
        if ( vector ) {
            a_id = atoi(vector[0]);
        }
        ssdbFreeRequest(Error, req);
    }
    return(a_id);
}

/*---------------------------------------------------------------------------*/
/* SGMSRVRSSDBif                                                             */
/*  Operations that are performed on SEM side.                               */
/*---------------------------------------------------------------------------*/

int SGMSRVRSSDBif(char *offsgmhname, subscribe_t *sem, subscribe_t *sgm, int proc, int opts, char *data)
{
    ssdb_Request_Handle        Request = 0;
    int                        action_id = 0;
    char                       *tmpData = data;
    char                       *tmpStr1, *tmpStr2;
    int                        reqType = 0;
    char                       sqlstr[1024];
    int                        ErrCode = 0;


    if ( !sem || !sgm || !offsgmhname )
        return ERR(SERVERERR, PARAMERR, INVALIDARGS);

    /*-----------------------------------------------------------------------*/
    /*  First, get the action_id from SSDB.  We need the action ID to insert */
    /*  into event_actionref table.  If we can't get the action ID, then,    */
    /*  lets insert an action for forwarding events to SGM only when proc    */
    /*  is SUBSCRIBE and opts is PROCDATA.  In all other cases, bail out.    */
    /*-----------------------------------------------------------------------*/

    action_id = SGMSSDBGetActionID("forward_hostname", offsgmhname);

    if ( proc == SUBSCRIBE && opts == PROCDATA && !action_id ) {
        if ( !ssdbLockTable(Error, Connection, "event_action")) goto end;
	if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_INSERT,
			 "insert into event_action values('%s',NULL,'%s',1,0,'%s',2,300,'root','Forward Event to Host',1,1)",
			 sem->sysinfo.sys_id, offsgmhname,
			 "/usr/lib32/internal/libsgmtask.so %F %S %I"))) {
	    ssdbUnLockTable(Error, Connection);
	    goto end;
        } else {
	    ssdbFreeRequest(Error, Request);
	}
	if ( !ssdbUnLockTable(Error, Connection)) goto end;
    } else if ( !action_id ) {
	ErrCode = UNABLETOGETACTIONID;
	goto end1;
    }

    if ( proc == SUBSCRIBE ) {
	if ( opts == CLEANUP ) {
	    reqType = SSDB_REQTYPE_DELETE;
	    sprintf(sqlstr, "delete from event_actionref where action_id = %d",
		    sgm->timekey);
	} else if ( opts == ENBLHOST ) {
	    reqType = SSDB_REQTYPE_UPDATE;
	    sprintf(sqlstr, "update event_actionref set action_id = %d where action_id = %d",
		    action_id, sgm->timekey);
	} 

	if ( reqType ) {
	    if ( !ssdbLockTable(Error, Connection, "event_actionref")) goto end;
	    if ( !(Request = ssdbSendRequest(Error, Connection, reqType, sqlstr))) {
		ssdbUnLockTable(Error, Connection);
		goto end;
	    } else {
		ssdbFreeRequest(Error, Request);
	    }
	    if ( !ssdbUnLockTable(Error, Connection)) goto end;
	}
    }

    if ( opts == PROCDATA ) {
	/* This is where you insert/delete entries from SSDB */
	if ( tmpData ) {
	    while ( (tmpStr1 = strtok_r(tmpData, "&", &tmpStr2)) != NULL ) {
		char *class, *type;

		class = strtok(tmpStr1, "|");
		while ( (type = strtok(NULL, ",")) != NULL ) {
		    switch(proc) { 
                      case SUBSCRIBE:
                      case UNSUBSCRIBE:
			reqType = SSDB_REQTYPE_DELETE;
			sprintf(sqlstr, "delete from event_actionref where class_id = %s and type_id = %s and action_id = %d",
				class, type, action_id);
                        break;
		    }

		    /* Now, send the request */
		    Request = ssdbSendRequest(Error, Connection, reqType, sqlstr);
		    ssdbFreeRequest(Error, Request);
		    if ( proc == SUBSCRIBE ) {
		        if ( !(Request = ssdbSendRequestTrans(Error, Connection,
				     SSDB_REQTYPE_INSERT, "event_actionref",
				     "insert into event_actionref values('%s',NULL,%s,%s,%d)",
                                     sem->sysinfo.sys_id, class, type, 
				     sgm->timekey))) {
			    goto end;
			} else {
			    ssdbFreeRequest(Error, Request);
			}
                    }
		}
		tmpData = NULL;
	    }
	} else if ( proc == UNSUBSCRIBE ) {
	    /* Delete all entries */
	    Request = ssdbSendRequestTrans(Error, Request, SSDB_REQTYPE_DELETE, 
				      "event_actionref", 
				      "delete from event_actionref where action_id = %d",
				      action_id);
	    ssdbFreeRequest(Error, Request);
	}
    }

    if ( opts == CLEANUP || proc == UNSUBSCRIBE ) {
	/* Check whether there are any floating entries for action_id 
	   and remove them */
        snprintf(sqlstr, 1024, 
		 "select type_id, class_id from event_actionref where action_id = %d",
		 action_id);

	if ( (Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT, sqlstr))) {
	    reqType = ssdbGetNumRecords(Error, Request);
	    if ( !reqType && !ssdbGetLastErrorCode(Error)) {
		ssdbFreeRequest(Error, Request);
		Request = ssdbSendRequestTrans(Error, Connection, 
			  SSDB_REQTYPE_DELETE, "event_action", 
			  "delete from event_action where action_id = %d", action_id);
	    }
	    ssdbFreeRequest(Error, Request);
	}
    }
    end:
        ErrCode = ssdbGetLastErrorCode(Error);
    end1:
        if (Request) ssdbFreeRequest(Error, Request);
        return((ErrCode > 0 ? ErrCode : -1*ErrCode));
}
