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

#ident           "$Revision: 1.3 $"

/*---------------------------------------------------------------------------*/
/* dbutils.c                                                                 */
/* Utilities required by amformat                                            */
/*---------------------------------------------------------------------------*/

#include <amstructs.h>
#include <amformat.h>
#include <amdefs.h>
#include <ssdbapi.h>
#include <amssdb.h>
#include <syslog.h>

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

int  getConfig(Event_t *, amConfig_t *);
static void PrintDBError(char *, ssdb_Error_Handle);
extern char *getCanonicalHostName(char *);

extern int aflag;
extern char *cmd;

/*---------------------------------------------------------------------------*/
/* Name    : getConfig                                                       */
/* Purpose : Get availmon's configuration from SSDB.                         */
/* Inputs  : Event strucutre and amConfig_t structure                        */
/* Outputs : Error Code                                                      */
/*---------------------------------------------------------------------------*/

int  getConfig(Event_t *event, amConfig_t *Config)
{
    ssdb_Client_Handle        Client = 0;
    ssdb_Connection_Handle    Connection = 0;
    ssdb_Request_Handle       Request = 0;
    ssdb_Error_Handle         Error = 0;
    int                       NumCols, NumRows, i, j, k;
    char                      sqlbuffer[1024];
    int			      ErrCode = 0;
    int                       Format = 0;
    const char                **vector = NULL;

    if ( event == NULL ) return(-1);
    if ( Config == NULL ) return(-1);

    /*-----------------------------------------------------------------------*/
    /* Initialize SSDB API.  If we can't initialize, we can't do anything    */
    /*-----------------------------------------------------------------------*/

    if ( !ssdbInit()) {
	PrintDBError("Init", Error);
	return(-1);
    }

    /*-----------------------------------------------------------------------*/
    /* Get an Error Handle from SSDB API                                     */
    /*-----------------------------------------------------------------------*/

    if ( (Error = ssdbCreateErrorHandle()) == NULL ) {
	PrintDBError("CreateErrorHandle", Error);
	ErrCode++;
	goto end;
    }

    /*-----------------------------------------------------------------------*/
    /* Get a Client Handle.  What should be the SSDBUSER and SSDBPASSWD ?    */
    /*-----------------------------------------------------------------------*/

    if ( (Client = ssdbNewClient(Error, SSDBUSER, SSDBPASSWD, 0))
		 == NULL ) {
	PrintDBError("NewClient", Error);
	ErrCode++;
	goto end;
    }

    /*-----------------------------------------------------------------------*/
    /* Get a Connection Handle from SSDB API                                 */
    /*-----------------------------------------------------------------------*/

    if ( !(Connection = ssdbOpenConnection(Error, Client,
            	        SSDBHOST, SSDB, (SSDB_CONFLG_RECONNECT |
			SSDB_CONFLG_REPTCONNECT))) ) {
	PrintDBError("OpenConnection", Error);
	ErrCode++;
	goto end;
    }

    /*-----------------------------------------------------------------------*/
    /* Generate SQL String.  If hostname field is NULL, which is normally the*/
    /* case when availmon tools log the event, then pickup the record where  */
    /* local = 1.  Otherwise, select that record for hostname                */
    /*-----------------------------------------------------------------------*/

    if ( event->hostname != NULL ) {
	sprintf(sqlbuffer, "SELECT %s, %s, %s from %s where %s=\"%s\" and active = 1",
			   SYSTEM_SYSID, SYSTEM_SERIAL, SYSTEM_HOST, SYSTEMTABLE, 
			   SYSTEM_HOST, event->hostname);
    } else {
	sprintf(sqlbuffer, "SELECT sys_id, sys_serial, hostname from system where local=1 and active=1");
    }

    /*-----------------------------------------------------------------------*/
    /* Send Request to SSDB API                                              */
    /*-----------------------------------------------------------------------*/

    if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,
				     sqlbuffer))) {
	PrintDBError("SendRequest", Error);
	ErrCode++;
	goto end;
    }

    /*-----------------------------------------------------------------------*/
    /* If we didn't get any rows, we can't do anything.                      */
    /*-----------------------------------------------------------------------*/

    if ( (NumRows = ssdbGetNumRecords(Error, Request)) == 0 ) {
	PrintDBError("GetNumRecords didn't return any records", Error);
	ErrCode++;
	goto end;
    }

    /*-----------------------------------------------------------------------*/
    /* Pick up the first row.  There can be multiple rows, but we will ignore*/
    /* the remaining ones                                                    */
    /*-----------------------------------------------------------------------*/

    ssdbRequestSeek(Error, Request, 0, 0);
    vector = ssdbGetRow(Error, Request);
    if ( vector ) {
	event->sys_id = atoi(vector[0]);
	event->serialnum = strdup(vector[1]);
	/* event->hostname  = strdup(vector[2]); */
	event->hostname = getCanonicalHostName((char *) vector[2]);
    } else 
	ErrCode++;

    /*-----------------------------------------------------------------------*/
    /* Now that we got the basis system information, lets look at what the   */
    /* SSDB says is availmon's configuration.  Form another request.         */
    /*-----------------------------------------------------------------------*/

    ssdbFreeRequest(Error, Request);

    sprintf(sqlbuffer, "SELECT %s,%s from %s where %s = '%s'",
		       TOOLOPTION, OPTIONDEFAULT, TOOLTABLE, TOOLNAME,TOOLCLASS);

    if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,
				     sqlbuffer))) {
	PrintDBError("SendRequest", Error);
	ErrCode++;
	goto end;
    }

    /*-----------------------------------------------------------------------*/
    /* If SSDB didn't return any records, that means there is no config.     */
    /*-----------------------------------------------------------------------*/

    if ( (NumRows = ssdbGetNumRecords(Error, Request)) == 0 ) {
	PrintDBError("GetNumRecords didn't return any records", Error);
	ErrCode++;
	goto end;
    }

    /*-----------------------------------------------------------------------*/
    /* Pick up the values of 'autoemail', 'hinvupdate' and 'statusinterval'  */
    /* values.  If amformat got invoked with a -a option, then we need to    */
    /* read the values of e-mail addresses from SSDB to mail reports         */
    /*-----------------------------------------------------------------------*/

    ssdbRequestSeek(Error, Request, 0, 0);
    for ( i = 0; i < NumRows; i++ ) {
	vector = ssdbGetRow(Error, Request);
	if ( vector ) {
	    if ( strcmp(vector[0], "autoemail") == 0 ) {
		if (atoi(vector[1])) Config->configFlag |= AUTOEMAIL;
	    } else if ( strcmp(vector[0], "hinvupdate") == 0 ) {
		if (atoi(vector[1])) Config->configFlag |= HINVUPDATE;
	    } else if ( strcmp(vector[0], "statusinterval") == 0 ) {
		if (atoi(vector[1])) event->statusinterval = atoi(vector[1]);
	    } else {
		if ( (Format = convertToFormatCode(vector[0])) != 0 ) {
                    if (vector[1] && vector[1][0] == '\0' ) continue;
		    if ( aflag ) {
			if (Config->notifyParams[Format-1] == NULL ) {
			    Config->notifyParams[Format-1] = 
			      (repparams_t *)newrepparams(Format);
			}
			insertaddr(Config->notifyParams[Format-1],
				   (emailaddrlist_t *) newaddrnode(vector[1]));
		    }
		}
	    }
	}
    }


    end:
	if (Request) ssdbFreeRequest(Error, Request);
	if (Connection) ssdbCloseConnection(Error, Connection);
	if (Error) ssdbDeleteErrorHandle(Error);
	if ( !ssdbDone() ) {
	    PrintDBError("Done", 0);
	    return(-1);
	}

	if (ErrCode) {
	    return(-1);
	}
	return(0);

}

/*---------------------------------------------------------------------------*/
/* Name    : PrintDBError                                                    */
/* Purpose : Prints the Database error returned by SSDB                      */
/* Inputs  : The call where error occured and Error Handle                   */
/* Outputs : None.                                                           */
/*---------------------------------------------------------------------------*/

static void PrintDBError(char *location, ssdb_Error_Handle Error)
{
    errorexit(cmd, 0, LOG_ERR, "Error in ssdb%s: %s", location, 
	      ssdbGetLastErrorString(Error));
}

/*---------------------------------------------------------------------------*/
/* Name    : read_data_from_ssdb                                             */
/* Purpose : Reads ObjectID data from SSDB and populates Event structure     */
/* Inputs  : Event Structure and Object ID                                   */
/* Outputs : 1 or 0                                                          */
/*---------------------------------------------------------------------------*/

int read_data_from_ssdb(Event_t *event, __uint32_t objectID)
{
    ssdb_Client_Handle        Client = 0;
    ssdb_Connection_Handle    Connection = 0;
    ssdb_Request_Handle       Request = 0;
    ssdb_Error_Handle         Error = 0;
    int                       NumCols = 0, NumRows = 0; 
    int                       i = 0;
    const char                **vector = NULL;
    int                       ErrCode = 0;
    char                      sqlbuffer[1024];

    if ( event == NULL ) return(-1);
    /*-----------------------------------------------------------------------*/
    /* Initialize SSDB API.  If we can't initialize, we can't do anything    */
    /*-----------------------------------------------------------------------*/

    if ( !ssdbInit()) {
	PrintDBError("Init", Error);
	return(-1);
    }

    /*-----------------------------------------------------------------------*/
    /* Get an Error Handle from SSDB API                                     */
    /*-----------------------------------------------------------------------*/

    if ( (Error = ssdbCreateErrorHandle()) == NULL ) {
	PrintDBError("CreateErrorHandle", Error);
	ErrCode++;
	goto nend;
    }

    /*-----------------------------------------------------------------------*/
    /* Get a Client Handle.  What should be the SSDBUSER and SSDBPASSWD ?    */
    /*-----------------------------------------------------------------------*/

    if ( (Client = ssdbNewClient(Error, SSDBUSER, SSDBPASSWD, 0))
	 == NULL ) {
	PrintDBError("NewClient", Error);
	ErrCode++;
	goto nend;
    }

    /*-----------------------------------------------------------------------*/
    /* Get a Connection Handle from SSDB API                                 */
    /*-----------------------------------------------------------------------*/

    if ( !(Connection = ssdbOpenConnection(Error, Client,
					   SSDBHOST, SSDB, (SSDB_CONFLG_RECONNECT |
							    SSDB_CONFLG_REPTCONNECT))) ) {
	PrintDBError("OpenConnection", Error);
	ErrCode++;
	goto nend;
    }
    
    /*-----------------------------------------------------------------------*/
    /* Generate SQL String to read data from SSDB                            */
    /*-----------------------------------------------------------------------*/

    sprintf(sqlbuffer, "SELECT %s,%s.%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s FROM %s,%s WHERE %s.%s = %s.%s AND %s.%s = %d",
	    EVENT_TYPEID, AVAILMONTABLE,AVAILMON_EVENTTIME, AVAILMON_LASTTICK,
	    AVAILMON_PREVSTART, AVAILMON_START, AVAILMON_STATUSINT,
	    AVAILMON_BOUNDS, AVAILMON_DIAGTYPE, AVAILMON_DIAGFILE,
	    AVAILMON_SYSLOG, AVAILMON_HINVCHANGE, AVAILMON_VERCHANGE,
	    AVAILMON_METRICS, AVAILMON_FLAG, AVAILMON_SUMMARY, 
	    AVAILMONTABLE, EVENTTABLE,
	    EVENTTABLE, EVENT_EVENTID, AVAILMONTABLE, AVAILMON_EVENTID,
	    EVENTTABLE, EVENT_EVENTID, objectID);
#ifdef DEBUG
    errorexit(cmd, 0, LOG_DEBUG, "Sql String is %s", sqlbuffer);
#endif

    /*-----------------------------------------------------------------------*/
    /* Send Request to SSDB API                                              */
    /*-----------------------------------------------------------------------*/

    if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,
				     sqlbuffer))) {
	PrintDBError("SendRequest", Error);
	ErrCode++;
	goto nend;
    }

    /*-----------------------------------------------------------------------*/
    /* If we didn't get any rows, we can't do anything.                      */
    /*-----------------------------------------------------------------------*/

    if ( (NumRows = ssdbGetNumRecords(Error, Request)) == 0 ) {
	PrintDBError("GetNumRecords didn't return any records", Error);
	ErrCode++;
	goto nend;
    }

    /*-----------------------------------------------------------------------*/
    /* Otherwise, pickup the first row                                       */
    /*-----------------------------------------------------------------------*/

    ssdbRequestSeek(Error, Request, 0, 0);
    vector = ssdbGetRow(Error, Request);
    if ( vector ) {
	event->eventcode      = (__uint32_t) atoi(vector[0]);
	event->eventtime      = (time_t) atoi(vector[1]);
	event->lasttick       = (time_t) atoi(vector[2]);
	event->prevstart      = (time_t) atoi(vector[3]);
	event->starttime      = (time_t) atoi(vector[4]);
	event->statusinterval = (__uint32_t) atoi(vector[5]);
	event->bounds         = (__uint32_t) atoi(vector[6]);

	if ( vector[7] == NULL || strcmp(vector[7], "NULL") == 0 ) {
	    event->diagtype = NULL;
	} else {
	    event->diagtype = strdup(vector[7]);
	}

	if ( vector[8] == NULL || strcmp(vector[8], "NULL") == 0 ) {
	    event->diagfile = NULL;
	} else {
	    event->diagfile = strdup(vector[8]);
	}

	if ( vector[9] == NULL || strcmp(vector[9], "NULL") == 0 ) {
	    event->syslog = NULL;
	} else {
	    event->syslog = strdup(vector[9]);
	}

	event->hinvchange     = (__uint32_t) atoi(vector[10]);
	event->verchange      = (__uint32_t) atoi(vector[11]);
	event->metrics        = (__uint32_t) atoi(vector[12]);
	event->flag           = (__uint32_t) atoi(vector[13]);
	event->shutdowncomment= NULL;
	
	if ( vector[14] == NULL || strcmp(vector[14], "NULL") == 0 ) {
	    event->summary = NULL;
	} else {
	    event->summary = strdup(vector[14]);
	}
    } else
	ErrCode++;
    
nend:
    if (Request) ssdbFreeRequest(Error, Request);
    if (Connection) ssdbCloseConnection(Error, Connection);
    if (Error) ssdbDeleteErrorHandle(Error);
    if ( !ssdbDone() ) {
	PrintDBError("Done", 0);
	return(-1);
    }

    if (ErrCode) {
	return(-1);
    }
    return(0);
				    
}
