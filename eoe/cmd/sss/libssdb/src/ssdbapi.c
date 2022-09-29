/* --------------------------------------------------------------------------- */
/* -                            SSDBAPI.C                                    - */
/* --------------------------------------------------------------------------- */
/*
 * Copyright 1992-1998 Silicon Graphics, Inc.
 * All Rights Reserved.
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
/* --------------------------------------------------------------------------- */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sss/ssdbapi.h>
#include <sss/ssdberr.h>
#include "ssdbidata.h"
#include "ssdbiutil.h"
#include "ssdbosdep.h"
#include "ssdbthread.h"

extern void my_init(void);
extern void my_end(int infoflag);
extern char my_thread_init(void);
extern void my_thread_end(void);

ssdb_GSTATUS ssdb_gst;   /* SSDB Global Statistics Structure */

/* Various strings */
static const char szSSDBVersionDescriptor[] = "(C) 1998-99 Silicon Graphics Inc. System Support Database.";
static const char szSSDBContactAddress[]    = "satish@csd.sgi.com, legalov@csd.sgi.com";

/* Default strings */
static const char szDefaultUserName[] = "localhost";  /* Default user name */
static const char szDefaultPassword[] = "";           /* Default password */
static const char szDefaultHostName[] = "localhost";  /* Default host name */

/* SSDB Error string(s) */
static const char szSSDBERRMSGAlreadyInInitPhase[]      = "Already in ssdbInit() phase";
static const char szSSDBERRMSGAlreadyInitialized[]      = "Already initialized";
static const char szSSDBERRMSGInitiError1[]             = "Init Error: CriticalSection/Mutex";
static const char szSSDBERRMSGNotInited[]               = "SSDB API Not Inited";
static const char szSSDBERRMSGInvalidParameter[]        = "Invalid Parameter";
static const char szSSDBERRMSGAlreadyInDone[]           = "Already in Done";
static const char szSSDBERRMSGNotEnoughResources[]      = "Not Enough Resources";
static const char szSSDBERRMSGResourceInUse[]           = "Resource already in use";
static const char szSSDBERRMSGDatabaseConnectionError[] = "Database connection error";
static const char szSSDBERRMSGDConnectionNotOpened[]    = "Connection not opened";
static const char szSSDBERRMSGSQLRequestError[]         = "SQL Request error";
static const char szSSDBERRMSGProcessResultsError[]     = "Process result error";
static const char szSSDBERRMSGDBError[]			= "Error in DB Server handling request";
static const char szSSDBERRMSGENDOFSETError[]           = "Requested for more rows after reaching end of selected set";

/* Static variables */
static int  ssdbApiInitCounter = 0;                   /* SSDB API Init flag */
static ssdb_LASTERR ssdbLastErrorStatic;              /* Static ssdb_LASTERR structure for main ssdb error */
static CRITICAL_SECTION lockCriticalSection;          /* Critical section for Lock/UnLock function */

/* static function predefinitions */
extern int ssdbiu_process_INSERT_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req);     /* Processor for INSERT request */
extern int ssdbiu_process_SELECT_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req);     /* Processor for SELECT request */
extern int ssdbiu_process_UPDATE_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req);     /* Processor for UPDATE request */
extern int ssdbiu_process_DELETE_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req);     /* Processor for DELETE request */
extern int ssdbiu_process_CREATE_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req);     /* Processor for CREATE request */
extern int ssdbiu_process_DROPTABLE_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req);  /* Processor for CREATE request */
extern int ssdbiu_process_ALTER_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req);      /* Processor for ALTER request */
extern int ssdbiu_process_REPLACE_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req);    /* Processor for REPLACE request */
extern int ssdbiu_process_SHOW_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req);       /* Processor for SHOW request */
extern int ssdbiu_process_DESCRIBE_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req);   /* Processor for DESCRIBE request */
extern int ssdbiu_process_LOCK_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req);       /* Processor for LOCK request */
extern int ssdbiu_process_UNLOCK_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req);     /* Processor for UNLOCK request */
extern int ssdbiu_process_DROPDATABASE_Results(ssdb_CONNECTION *con,ssdb_REQUEST *req);     /* Processor for DROPDATABASE request */
static int ssdbiu_SelectDBInternal(ssdb_CONNECTION *con);	/* Prototype for forward declared function */

/* Request processing table */
ssdb_REQPROCENTRY reqProcTbl[] = {
 { SSDB_REQTYPE_INSERT,       ssdbiu_process_INSERT_Results },
 { SSDB_REQTYPE_SELECT,       ssdbiu_process_SELECT_Results },
 { SSDB_REQTYPE_UPDATE,       ssdbiu_process_UPDATE_Results },
 { SSDB_REQTYPE_DELETE,       ssdbiu_process_DELETE_Results },
 { SSDB_REQTYPE_CREATE,       ssdbiu_process_CREATE_Results },
 { SSDB_REQTYPE_DROPTABLE,    ssdbiu_process_DROPTABLE_Results },
 { SSDB_REQTYPE_DROPDATABASE, ssdbiu_process_DROPDATABASE_Results },
 { SSDB_REQTYPE_ALTER,        ssdbiu_process_ALTER_Results },
 { SSDB_REQTYPE_REPLACE,      ssdbiu_process_REPLACE_Results },
 { SSDB_REQTYPE_DESCRIBE,     ssdbiu_process_DESCRIBE_Results },
 { SSDB_REQTYPE_SHOW,         ssdbiu_process_SHOW_Results },
 { SSDB_REQTYPE_LOCK,         ssdbiu_process_LOCK_Results},
 { SSDB_REQTYPE_UNLOCK,       ssdbiu_process_UNLOCK_Results}
};
#define reqProcTblSize (sizeof(reqProcTbl)/sizeof(ssdb_REQPROCENTRY))
/* ======================================================================================= */
/* ------------------------- ssdbiu_setLastError ----------------------------- */
static int ssdbiu_setLastError(ssdb_LASTERR *serr,int errorcode,const char *errmsg)
{ int retcode = 0;
  if(ssdbiu_checkErrorHandler(serr))
  { serr->lasterror_code    = errorcode;
    serr->last_error_strptr = (char *)errmsg;
    retcode++;
  }
  return retcode;
}
/* ------------------- ssdbiu_copyErrorStrToErrorHandle ---------------------- */
static int ssdbiu_copyErrorStrToErrorHandle(ssdb_LASTERR *serr,int errorcode,const char *errmsg)
{ int retcode = 0;
  int str_size = 0;
  if(ssdbiu_checkErrorHandler(serr))
  { serr->lasterror_code    = errorcode;
    serr->last_error_strptr = 0;
    if(errmsg)
    { if((str_size = strlen(errmsg)) > SSDB_STATIC_ERRSTR_SIZE) str_size = SSDB_STATIC_ERRSTR_SIZE;
    }
    if(str_size)
    { memcpy(serr->error_str_buffer,errmsg,str_size); serr->error_str_buffer[str_size] = 0;
      serr->last_error_strptr = &serr->error_str_buffer[0];
    }
    retcode++;
  }
  return retcode;
}
/* ----------------------- ssdbiu_SetSSDBError ------------------------------- */
static int ssdbiu_SetSSDBError(void *serr,int errorcode,const char *errmsg)
{ if(!ssdbiu_setLastError((ssdb_LASTERR *)serr,errorcode,errmsg)) ssdbiu_setLastError(&ssdbLastErrorStatic,errorcode,errmsg);
  return 0;
}
/* ---------------------- ssdbiu_CopySSDBError ------------------------------- */
static void ssdbiu_CopySSDBError(void *serr,int errorcode,const char *errmsg)
{ if(!ssdbiu_copyErrorStrToErrorHandle((ssdb_LASTERR *)serr,errorcode,errmsg))
   ssdbiu_copyErrorStrToErrorHandle(&ssdbLastErrorStatic,errorcode,errmsg);
}
/* ------------------ ssdbiu_DeleteConnectionInternal ------------------------ */
static int ssdbiu_DeleteConnectionInternal(ssdb_CONNECTION *con,int alreadylocked)
{ ssdb_REQUEST *req;
  int retcode = 0;

  if(alreadylocked || ssdbiu_lockConnectionHandler(con))
  { con->in_delete_flg++;
    while((req = ssdbiu_removeFirstRequestHandlerFromConnect(con)) != 0)
     ssdbiu_delRequestHandler(req); /* Clear Requests list */

    /* Close "real" connection to db */
    if(con->connected)
    { con->connected = 0; mysql_close(&con->mysql);
    }
    con->username[0]  = (con->password[0] = 0); /* Clear username & password fields */
    con->hostname[0]  = (con->dbname[0] = 0);   /* Clear hostname & databasename fields */
    con->uname_len    = (con->passwd_len = 0);  /* Clear username & password lens */
    con->hostname_len = (con->dbname_len = 0);  /* Clear hostname & databasename lens */
    con->flags        = 0;                      /* Reset all flags */
    ssdbiu_unlockConnectionHandler(con);
    ssdbiu_delConnectionHandler(con);
    retcode++;
  }
  return retcode;
}
/* -------------------- ssdbiu_DeleteClientInternal -------------------------- */
static int ssdbiu_DeleteClientInternal(ssdb_CLIENT *cli,int alreadylocked)
{ ssdb_CONNECTION *con;
  int retcode = 0;
  if(alreadylocked || ssdbiu_lockClientHandler(cli))
  { cli->in_delete_flg++;
    /* Clear All Connections */
    while((con = cli->conn_list) != 0)
    { cli->conn_list = (ssdb_CONNECTION *)con->lnk_next;
      ssdbiu_DeleteConnectionInternal(con,0);
    }
    cli->uname_len   = (cli->passwd_len = 0);
    cli->username[0] = (cli->password[0] = 0);
    cli->flags       = 0;
    ssdbiu_unlockClientHandler(cli);
    ssdbiu_delClientHandler(cli);
    retcode++;
  }
  return retcode;
}
/* ---------------------- ssdbiu_ConnectInternal ----------------------------- */
static int ssdbiu_ConnectInternal(ssdb_CONNECTION *con)
{ int i,reptcnt;
  int retcode = 0;
  
  if(con)
  { if(!con->in_connect_flg++)
    { /* Can't be zero */
      reptcnt = ((con->flags & SSDB_CONFLG_RECONNECT) != 0) ? SSDB_CONNECT_REPTCOUNTER : 1; 
      for(i = 0;i < reptcnt && !retcode;i++)
      {	memset(&con->mysql,0,sizeof(MYSQL));
        if((con->connected = mysql_connect(&con->mysql,
                                           con->hostname_len ? con->hostname : 0,
					   con->uname_len    ? con->username : 0,
					   con->passwd_len   ? con->password : 0)) != 0) retcode++; /* Success */
      }
    }
    con->in_connect_flg--;
  }
  return retcode;
}

/* ----------------------- ssdbiu_mysqlRealQuery ----------------------------- */
static int ssdbiu_mysqlRealQuery(ssdb_CONNECTION *con,const char *sqlRequest,int requst_length)
{ int retcode = 0;
  int loop_on = 2;  

  if(con)
  { if(!con->in_rquery_flg++)
    { while(!retcode && loop_on)
      { retcode++; loop_on--; /* set return code to success and decrement loop counter */
	if(mysql_real_query(&con->mysql,sqlRequest,requst_length))
	{ /* Check "reconnnect" bit in connection flags */
          if((con->flags & SSDB_CONFLG_RECONNECT) != 0)
          { switch(mysql_errno(&con->mysql)) {
            case CR_SERVER_GONE_ERROR :
            case CR_SERVER_LOST       : if(ssdbiu_ConnectInternal(con) && ssdbiu_SelectDBInternal(con))	break;
                                        loop_on = 0;
					break;
            default  : loop_on = 0;
            }; /* end of switch(mysql_errno(&con->mysql)) */
	  }
	  else loop_on = 0; /* break loop */
          retcode = 0; /* any error appear */
	}
      } /* end of while(!retcode && loop_on) */
    }
    con->in_rquery_flg--;
  }
  return retcode;
}
/* ---------------------- ssdbiu_SelectDBInternal ---------------------------- */
static int ssdbiu_SelectDBInternal(ssdb_CONNECTION *con)
{ int retcode = 0;
  if(con && con->connected)
  { if(!con->in_selectdb_flg++)
    { if(!mysql_select_db(&con->mysql,con->dbname)) retcode++;
    }
    con->in_selectdb_flg--;
  }
  return retcode;
}
/* -------------------- ssdbiu_checkSqlRequestType --------------------------- */
static int ssdbiu_checkSqlRequestType(int sqlRequestType)
{ int i;
  int retcode = 0;
  int reqTypeReal = sqlRequestType & SSDB_REQTYPE_MASK; /* Extract real request type */
  for(i = 0;i < reqProcTblSize;i++)
  { if(reqProcTbl[i].req_type == reqTypeReal) { retcode++; break; }
  }
  return retcode;   
}
/* ------------------- ssdbiu_processRequestResults -------------------------- */
static int ssdbiu_processRequestResults(ssdb_CONNECTION *con,int sqlRequestType,ssdb_REQUEST *req)
{ int i,reqTypeReal;
  int retcode = 0;
  if(con && req)
  { ssdbiu_freeRequestMySqlResult(ssdbiu_freeRequestFieldInfo(req));
    reqTypeReal = sqlRequestType & SSDB_REQTYPE_MASK;
    req->sql_req_type = sqlRequestType;
    for(i = 0;i < reqProcTblSize;i++)
    { if(reqProcTbl[i].req_type == reqTypeReal)
      {	if(reqProcTbl[i].fpProcessor) retcode = reqProcTbl[i].fpProcessor(con,req);
	break;
      }
    }
  }
  return retcode;
}
/* ------------------------- ssdbLockTable ----------------------------------- */
int SSDBAPI ssdbLockTable(ssdb_Error_Handle errHandle, ssdb_Connection_Handle connection, char *table_names,...)
{ ssdb_Request_Handle req_handle;
  char temp_rqbuf[1024*4],result[1024*8];
  int sqlreqstr_size,argc,j;
  char *argv[SSDB_MAX_LOCKTABLES_CNT],*c;
  va_list args;

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited); /* not inited */
  }

  if(!table_names) /* check for table names */
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter); /* invalid paremeter(s) */
  }

  va_start(args,table_names);   /* create request string */
  sqlreqstr_size = ssdb_vsnprintf(temp_rqbuf,sizeof(temp_rqbuf),table_names,args);
  va_end(args);

  if(sqlreqstr_size <= 0)
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter); /* invalid paremeter(s) */
  }

  argc = 0;
  for(c = temp_rqbuf;*c && argc < SSDB_MAX_LOCKTABLES_CNT;)
  { while(*c == ' ' || *c == '\t') c++;
    if(!(*c)) break;
    argv[argc++] = c;
    while(*c && *c != ' ' && *c != '\t' && *c != ',') c++;
    if(!(*c)) break;
    *c++ = 0;    
  }

  if(!argc)
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter); /* invalid paremeter(s) */
  }

  j = sprintf(result,"LOCK TABLES ");
  sqlreqstr_size = 0;
  for(sqlreqstr_size = 0;sqlreqstr_size < argc;sqlreqstr_size++)
  { if(sqlreqstr_size) j += snprintf(&result[j],sizeof(result)-j,", ");
    j += snprintf(&result[j],sizeof(result)-j,"%s WRITE",argv[sqlreqstr_size]);
  }

  ssdbthread_enter_critical_section(&lockCriticalSection);
  req_handle = ssdbSendRequest(errHandle,connection,SSDB_REQTYPE_LOCK,result);
  ssdbthread_leave_critical_section(&lockCriticalSection);

  if(!req_handle) return 0;
  ssdbFreeRequest(errHandle,req_handle);
  return 1;
}

/* ------------------------- ssdbUnLockTable --------------------------------- */
int SSDBAPI ssdbUnLockTable(ssdb_Error_Handle errHandle, ssdb_Connection_Handle connection)
{ ssdb_Request_Handle req_handle;
  if(!(req_handle = ssdbSendRequest(errHandle,connection,SSDB_REQTYPE_UNLOCK,"UNLOCK TABLES")))	return 0;
  ssdbFreeRequest(errHandle,req_handle);
  return 1;
}

/* ---------------------- ssdbSendRequestTrans ------------------------------- */
ssdb_Request_Handle SSDBAPI ssdbSendRequestTrans(ssdb_Error_Handle errHandle, ssdb_Connection_Handle connection, int sqlRequestType, const char *table_names, char *sqlString,...)
{ char temp_rqbuf[SSDB_REQUEST_STACK_BUFF_SIZE];
  ssdb_LASTERR err_handle;
  ssdb_Request_Handle req_handle;
  int sqlreqstr_size;
  va_list args;

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited); /* not inited */
    return 0;
  }

  va_start(args,sqlString);   /* create request string */
  sqlreqstr_size = ssdb_vsnprintf(temp_rqbuf,sizeof(temp_rqbuf),sqlString,args);
  va_end(args);

  /* check request string size */
  if(sqlreqstr_size <= 0)
  { ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter); /* invalid paremeter(s) */
    return 0;
  }

  if((sqlRequestType == SSDB_REQTYPE_INSERT) || (sqlRequestType == SSDB_REQTYPE_UPDATE) || (sqlRequestType == SSDB_REQTYPE_DELETE))
  { if(ssdbLockTable(errHandle,connection,(char *)table_names) == 0) return 0;
  }

  if(!(req_handle = ssdbSendRequest(errHandle,connection,sqlRequestType,temp_rqbuf)))
  { ssdbiu_initializeErrorHandler(&err_handle); 
    if((sqlRequestType == SSDB_REQTYPE_INSERT) || (sqlRequestType == SSDB_REQTYPE_UPDATE) || (sqlRequestType == SSDB_REQTYPE_DELETE))
     ssdbUnLockTable((ssdb_Error_Handle)&err_handle,connection);
    return 0;
  }

  if((sqlRequestType == SSDB_REQTYPE_INSERT) || (sqlRequestType == SSDB_REQTYPE_UPDATE) || (sqlRequestType == SSDB_REQTYPE_DELETE))
  { if(!ssdbUnLockTable(errHandle,connection))
    { ssdbFreeRequest(errHandle,req_handle);
      return 0;
    }
  }
  return req_handle;
}

/* ------------------------ ssdbSendRequest ---------------------------------- */
ssdb_Request_Handle SSDBAPI ssdbSendRequest(ssdb_Error_Handle errHandle,ssdb_Connection_Handle connection,int sqlRequestType, char *sqlString, ...)
{ char temp_rqbuf[SSDB_REQUEST_STACK_BUFF_SIZE];
  int sqlreqstr_size;
  va_list args;
  ssdb_REQUEST *req;
  ssdb_CONNECTION *con = (ssdb_CONNECTION *)connection;

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited); /* not inited */
    return 0;
  }

  /* check for invalid parameter(s) */
  if(!ssdbiu_checkSqlRequestType(sqlRequestType) || !ssdbiu_checkConnectionHandler(con) ||
     !ssdbiu_checkClientHandler((ssdb_CLIENT *)con->lnk_parent,1))
  { ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
    return 0;
  }

  /* create request string */
  va_start(args,sqlString);
  sqlreqstr_size = ssdb_vsnprintf(temp_rqbuf,sizeof(temp_rqbuf),sqlString,args);
  va_end(args);

  /* check request string size */
  if(sqlreqstr_size <= 0)
  { ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter); /* invalid paremeter(s) */
    return 0;
  }

  if((req = ssdbiu_newRequestHandler()) == 0)
  { ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTENOGHRES,szSSDBERRMSGNotEnoughResources);
    return 0;
  }

  /* printf("Start Query(size:%d): \"%s\"\n",sqlreqstr_size,temp_rqbuf); FIXME */

  /* begin critical section for connection */
  if(!ssdbiu_lockConnectionHandler(con)) 
  { ssdbiu_delRequestHandler(req);
    ssdbiu_SetSSDBError(errHandle,SSDBERR_RESOURCEINUSE,szSSDBERRMSGResourceInUse); /* client already locked or deleted */
    return 0;
  }


  /* Step one: exec real query */
  if(!ssdbiu_mysqlRealQuery(con,temp_rqbuf,sqlreqstr_size))
  { ssdbiu_unlockConnectionHandler(con); ssdbiu_delRequestHandler(req);
    ssdbiu_SetSSDBError(errHandle,SSDBERR_SQLQUERYERROR,szSSDBERRMSGSQLRequestError); /* can't make real query */
    return 0;
  }

  /* Step two: call real "processor" */
  if(!ssdbiu_processRequestResults(con,sqlRequestType,req))
  { ssdbiu_unlockConnectionHandler(con); ssdbiu_delRequestHandler(req);
    ssdbiu_SetSSDBError(errHandle,SSDBERR_PROCESSRESERR,szSSDBERRMSGProcessResultsError); /* can't process results */
    return 0;
  }

  ssdbiu_addRequestHandlerToConnect(con,req);
  ssdbiu_unlockConnectionHandler(con);

  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return (ssdb_Request_Handle)req;
}

/* ---------------------- ssdbIsRequestValid --------------------------------- */
int SSDBAPI ssdbIsRequestValid(ssdb_Error_Handle errHandle,ssdb_Request_Handle request)
{ ssdb_REQUEST *req = (ssdb_REQUEST *)request;	

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited); /* not inited */
  }

  if(!ssdbiu_checkRequestHandler(req) || !ssdbiu_checkConnectionHandler((ssdb_CONNECTION *)req->lnk_parent))
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter); /* Invalid parameter */
  }	
  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return 1;
}
/* ------------------------ ssdbFreeRequest ---------------------------------- */
int SSDBAPI ssdbFreeRequest(ssdb_Error_Handle errHandle,ssdb_Request_Handle request)
{ ssdb_CONNECTION *con;
  ssdb_REQUEST *req = (ssdb_REQUEST *)request;	

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited); /* not inited */
  }

  if(!ssdbiu_checkRequestHandler(req) || !ssdbiu_lockConnectionHandler((con = (ssdb_CONNECTION *)req->lnk_parent)))
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter); /* Invalid parameter */
  }	

  if(!ssdbiu_checkRequestHandlerInConnect(con,req)) /* Check request handler in connect struct */
  { ssdbiu_unlockConnectionHandler(con);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter); /* Incorrect request handler parent pointer */
  }

  ssdbiu_subRequestHandlerFromConnect(con,req); /* Remove handler from connection */
  ssdbiu_delRequestHandler(req);
  ssdbiu_unlockConnectionHandler(con);

  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return 1; /* Success */
}
/* ------------------------ ssdbRequestSeek ---------------------------------- */
int SSDBAPI ssdbRequestSeek(ssdb_Error_Handle errHandle,ssdb_Request_Handle request,int row,int col)
{ ssdb_CONNECTION *con;
  ssdb_REQUEST *req = (ssdb_REQUEST *)request;	

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
  }

  if(row < 0 || col < 0 || !ssdbiu_checkRequestHandler(req) ||
     !ssdbiu_lockConnectionHandler((con = (ssdb_CONNECTION *)req->lnk_parent))) 
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }	

  if(!req->res)
  { ssdbiu_unlockConnectionHandler(con);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_DBERROR,szSSDBERRMSGDBError);
  }

  if(row >= req->num_rows || col >= req->columns)
  { ssdbiu_unlockConnectionHandler(con);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }

  mysql_data_seek(req->res,row);
  mysql_field_seek(req->res,0);


  if((req->current_row_ptr = mysql_fetch_row(req->res)) == 0)
  { ssdbiu_unlockConnectionHandler(con);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_DBERROR,szSSDBERRMSGDBError);
  }

  req->cur_field_number = col;
  req->cur_row_number   = row;

  ssdbiu_unlockConnectionHandler(con);

  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return 1; /* Success */
}
/* ----------------------- ssdbGetNextField ---------------------------------- */
int SSDBAPI ssdbGetNextField(ssdb_Error_Handle errHandle,ssdb_Request_Handle request,void *field,int sizeof_field)
{ ssdb_CONNECTION *con;
  ssdb_REQUEST *req = (ssdb_REQUEST *)request;	

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
  }

  if(!field || sizeof_field <= 0 || !ssdbiu_checkRequestHandler(req) ||
     !ssdbiu_lockConnectionHandler((con = (ssdb_CONNECTION *)req->lnk_parent))) 
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }	

  if(!req->res || !req->columns || !req->num_rows) /* Check Results */
  { ssdbiu_unlockConnectionHandler(con);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_DBERROR,szSSDBERRMSGDBError);
  }

  /* Check column and row number */
  if(req->cur_field_number >= req->columns) 
  { req->cur_field_number = 0;
    if(++req->cur_row_number >= req->num_rows)
    { ssdbiu_unlockConnectionHandler(con);
      return ssdbiu_SetSSDBError(errHandle,SSDBERR_DBERROR,szSSDBERRMSGDBError);
    }
    if(!(req->current_row_ptr = mysql_fetch_row(req->res)))
    { ssdbiu_unlockConnectionHandler(con);
      return ssdbiu_SetSSDBError(errHandle,SSDBERR_DBERROR,szSSDBERRMSGDBError);
    }
  }
  else if(req->cur_row_number >= req->num_rows)
  { ssdbiu_unlockConnectionHandler(con);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_ENDOFSET,szSSDBERRMSGENDOFSETError);
  }

  if(!req->current_row_ptr || req->cur_field_number >= req->field_info_size) /* Additional check */
  { ssdbiu_unlockConnectionHandler(con);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_DBERROR,szSSDBERRMSGDBError);
  }

  if(req->field_info[req->cur_field_number].byte_len > sizeof_field) /* Check destination object size */
  { ssdbiu_unlockConnectionHandler(con);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }

  if(!req->field_info[req->cur_field_number].Fpconvert) /* Check function pointer */
  { ssdbiu_unlockConnectionHandler(con);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_DBERROR,szSSDBERRMSGDBError);
  }

  /* Real data translation */
  req->field_info[req->cur_field_number].Fpconvert(req->current_row_ptr[req->cur_field_number],
						   field, req->field_info[req->cur_field_number].maxlen,sizeof_field);
  req->cur_field_number++;	
  ssdbiu_unlockConnectionHandler(con);
  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return 1; /* Success */
} 
/* ---------------------- ssdbGetNumRecords ---------------------------------- */
int SSDBAPI ssdbGetNumRecords(ssdb_Error_Handle errHandle,ssdb_Request_Handle request)
{ int num_rows;
  ssdb_CONNECTION *con;
  ssdb_REQUEST *req = (ssdb_REQUEST *)request;	

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
  }

  if(!ssdbiu_checkRequestHandler(req)|| !ssdbiu_lockConnectionHandler((con = (ssdb_CONNECTION *)req->lnk_parent)))
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }	

  num_rows = req->num_rows;
  ssdbiu_unlockConnectionHandler(con);

  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return num_rows;	
}
/* ----------------------- ssdbGetNumColumns --------------------------------- */
int SSDBAPI ssdbGetNumColumns(ssdb_Error_Handle errHandle, ssdb_Request_Handle request)
{ int columns;
  ssdb_CONNECTION *con;
  ssdb_REQUEST *req = (ssdb_REQUEST *)request;	

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
  }

  if(!ssdbiu_checkRequestHandler(req) || !ssdbiu_lockConnectionHandler((con = (ssdb_CONNECTION *)req->lnk_parent))) /* Check input parameter(s) */
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }	

  columns = req->columns;
  ssdbiu_unlockConnectionHandler(con);

  if(columns) ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  else        ssdbiu_SetSSDBError(errHandle,SSDBERR_DBERROR,szSSDBERRMSGDBError);
  return columns;
}
/* ------------------------ ssdbGetFieldSize --------------------------------- */
int SSDBAPI ssdbGetFieldSize(ssdb_Error_Handle errHandle,ssdb_Request_Handle request,int field_number)
{ int fieldsize;
  ssdb_CONNECTION *con;
  ssdb_REQUEST *req = (ssdb_REQUEST *)request;	

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
  }

  if(field_number < 0 || !ssdbiu_checkRequestHandler(req) ||
     !ssdbiu_lockConnectionHandler((con = (ssdb_CONNECTION *)req->lnk_parent)))
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }	

  if(req->columns <= field_number || req->field_info_size < field_number)
  { ssdbiu_unlockConnectionHandler(con);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }

  if(!req->field_info_ptr)
  { ssdbiu_unlockConnectionHandler(con);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_DBERROR,szSSDBERRMSGDBError);
  }

  fieldsize = req->field_info_ptr[field_number].byte_len; 
  ssdbiu_unlockConnectionHandler(con);
  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return fieldsize;
}	
/* ---------------------- ssdbGetFieldFieldName ------------------------------ */
int SSDBAPI ssdbGetFieldName(ssdb_Error_Handle errHandle,ssdb_Request_Handle request,int field_number,char *field_name,int field_name_size)
{ ssdb_CONNECTION *con;
  ssdb_REQUEST *req = (ssdb_REQUEST *)request;

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
  }

  if(field_number < 0 || !ssdbiu_checkRequestHandler(req) ||
     !ssdbiu_lockConnectionHandler((con = (ssdb_CONNECTION *)req->lnk_parent)))
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }

  if(req->columns <= field_number || req->field_info_size < field_number || !field_name || !field_name_size )
  { ssdbiu_unlockConnectionHandler(con);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }

  if(!req->field_info_ptr)
  { ssdbiu_unlockConnectionHandler(con);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_DBERROR,szSSDBERRMSGDBError);
  }

  field_name[0] = 0;
  strncpy(field_name,req->field_info_ptr[field_number].name,field_name_size);
  field_name[field_name_size-1] = 0;
  ssdbiu_unlockConnectionHandler(con);
  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return 1;
}
/* ----------------------- ssdbGetFieldMaxSize ------------------------------- */
int SSDBAPI ssdbGetFieldMaxSize(ssdb_Error_Handle errHandle,ssdb_Request_Handle request,int field_number)
{ int fieldsize;
  ssdb_CONNECTION *con;
  ssdb_REQUEST *req = (ssdb_REQUEST *)request;

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
  }

  if(field_number < 0 || !ssdbiu_checkRequestHandler(req) ||
     !ssdbiu_lockConnectionHandler((con = (ssdb_CONNECTION *)req->lnk_parent)))
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }

  if(req->columns <= field_number || req->field_info_size < field_number)
  { ssdbiu_unlockConnectionHandler(con);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }

  if(!req->field_info_ptr)
  { ssdbiu_unlockConnectionHandler(con);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_DBERROR,szSSDBERRMSGDBError);
  }

  fieldsize = req->field_info_ptr[field_number].maxlen;
  ssdbiu_unlockConnectionHandler(con);

  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return fieldsize;
}
/* ------------------------ ssdbGetRowWithSeek ------------------------------- */
const char ** SSDBAPI ssdbGetRowWithSeek(ssdb_Error_Handle errHandle,ssdb_Request_Handle request,int row)
{ char **argv;
  ssdb_CONNECTION *con;
  ssdb_REQUEST *req = (ssdb_REQUEST *)request;

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
    return 0;
  }

  if(row < 0 || !ssdbiu_checkRequestHandler(req) ||!ssdbiu_lockConnectionHandler((con = (ssdb_CONNECTION *)req->lnk_parent)))
  { ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
    return 0;
  }

  if(row >= req->num_rows)
  { ssdbiu_unlockConnectionHandler(con);
    ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
    return 0;
  }

  req->cur_row_number   = row;
  req->cur_field_number = 0;
  mysql_data_seek(req->res,row);

  if((req->current_row_ptr = mysql_fetch_row(req->res)) == 0)
  { ssdbiu_unlockConnectionHandler(con);
    ssdbiu_SetSSDBError(errHandle,SSDBERR_DBERROR,szSSDBERRMSGDBError);
    return 0;
  }

  argv = req->current_row_ptr;
  ssdbiu_unlockConnectionHandler(con);
  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return (const char **)argv;
}
/* --------------------------- ssdbGetRow ------------------------------------ */
const char ** SSDBAPI ssdbGetRow(ssdb_Error_Handle errHandle,ssdb_Request_Handle request)
{ char **argv;
  ssdb_CONNECTION *con;
  ssdb_REQUEST *req = (ssdb_REQUEST *)request;	

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
    return 0;
  }

  if(!ssdbiu_checkRequestHandler(req) || !ssdbiu_lockConnectionHandler((con = (ssdb_CONNECTION *)req->lnk_parent)))
  { ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
    return 0;
  }	

  argv = req->current_row_ptr;
  req->current_row_ptr = mysql_fetch_row(req->res);
  ssdbiu_unlockConnectionHandler(con);
  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return (const char **)argv;
}
/* ========================================================================== */
/* ========================================================================== */
/* ========================================================================== */
/* ========================================================================== */

/* ----------------------------- ssdbInit ----------------------------------- */
int SSDBAPI ssdbInit()
{ static int in_ssdbInitFlg = 0; /* Internal flag for lock this entry point */
  int i;

  if(in_ssdbInitFlg++ || ssdbApiInitCounter)
  { ssdbiu_setLastError(&ssdbLastErrorStatic,SSDBERR_ALREADYINIT1,szSSDBERRMSGAlreadyInInitPhase);
    in_ssdbInitFlg--;
    return 0;
  }

  for(i = 0;i < 5;i++) /* Why not 6,7,8...? Middle-ceiling value. */
  { if(ssdbApiInitCounter >= 2) /* The first check */
    { ssdbiu_setLastError(&ssdbLastErrorStatic,SSDBERR_ALREADYINIT2,szSSDBERRMSGAlreadyInitialized);
      in_ssdbInitFlg--;
      return 0;
    }
    if(ssdbApiInitCounter)
    { ssdbiu_setLastError(&ssdbLastErrorStatic,SSDBERR_ALREADYINIT1,szSSDBERRMSGAlreadyInInitPhase);
      in_ssdbInitFlg--;
      return 0;
    }
  }

  /* Initialize global error structure & Reset global errors */
  ssdbiu_setLastError(ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic),SSDBERR_SUCCESS,0); 
  ssdbApiInitCounter = 1; /* The first initialization phase */
  memset(&ssdb_gst,0,sizeof(ssdb_GSTATUS));

  if(!ssdbiu_initCritialSections())
  { ssdbiu_setLastError(&ssdbLastErrorStatic,SSDBERR_INITERROR1,szSSDBERRMSGInitiError1);
    ssdbApiInitCounter = 0; in_ssdbInitFlg--;
    return 0; /* Fatal error - can't init critical sections */
  }

  if(!ssdbthread_init_critical_section(&lockCriticalSection))
  { ssdbiu_doneCritialSections();
    ssdbiu_setLastError(&ssdbLastErrorStatic,SSDBERR_INITERROR1,szSSDBERRMSGInitiError1);
    ssdbApiInitCounter = 0; in_ssdbInitFlg--;
    return 0; /* Fatal error - can't init critical sections */      
  }

  my_init();
  my_thread_init();

  ssdbApiInitCounter++;                                /* Initialization complete - 2 */
  in_ssdbInitFlg--;                                    /* Unlock this routine entry point */
  return 1;                                            /* Success */
}

/* ---------------------------- ssdbDone ------------------------------------ */
int SSDBAPI ssdbDone()
{ ssdb_CLIENT *cli;
  static int in_ssdbDoneFlg = 0; /* Recursive Protection flag */

  if(in_ssdbDoneFlg++)
  { ssdbiu_setLastError(&ssdbLastErrorStatic,SSDBERR_ALREADYDONE,szSSDBERRMSGAlreadyInDone);
    in_ssdbDoneFlg--;
    return 0;
  }

  if(ssdbApiInitCounter == 1)
  { ssdbiu_setLastError(&ssdbLastErrorStatic,SSDBERR_ALREADYINIT1,szSSDBERRMSGAlreadyInInitPhase);
    in_ssdbDoneFlg--;
    return 0;
  }

  if(ssdbApiInitCounter < 2)
  { ssdbiu_setLastError(ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic),SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
    in_ssdbDoneFlg--;
    return 0;
  }

  ssdbApiInitCounter--; /* The first deinit phase */

  ssdbiu_setLastError(&ssdbLastErrorStatic,SSDBERR_SUCCESS,0); /* Reset global error storage */

  /* clear all Clients in work queue */
  while((cli = ssdbiu_removeFirstClientFromWorkList()) != 0) ssdbiu_DeleteClientInternal(cli,0);

  /* clear free client list (real memory deallocation) */
  ssdbiu_dropFreeClientList();

  /* clear free connection list (real memory deallocation) */
  ssdbiu_dropFreeConnectionList();

  /* clear free request list (real memory deallocation) */
  ssdbiu_dropFreeRequestList();

  /* free lasterror work list (move all error hanle to free list) */
  ssdbiu_dropWorkErrorList();

  /* clear free lasterror list (real memory deallocation) */
  ssdbiu_dropFreeErrorList();


  /* Destroy Lock/UnLock Critical section */
  ssdbthread_done_critical_section(&lockCriticalSection);

  /* Last step in Deinit process: destroy critical sections */
  ssdbiu_doneCritialSections();

  my_thread_end();
  my_end(0);

  ssdbApiInitCounter = 0; /* Now ready for next init */
  in_ssdbDoneFlg--;       /* Unlock this entry point */
  return 1;               /* Success */
}
/* ---------------------- ssdbCreateErrorHandle ----------------------------- */
ssdb_Error_Handle SSDBAPI ssdbCreateErrorHandle()
{ ssdb_LASTERR *err = 0;
  if(ssdbApiInitCounter >= 2)
  { if((err = ssdbiu_newErrorHandler()) != 0) ssdbiu_setLastError(&ssdbLastErrorStatic,SSDBERR_SUCCESS,0);
    else                                      ssdbiu_setLastError(&ssdbLastErrorStatic,SSDBERR_NOTENOGHRES,szSSDBERRMSGNotEnoughResources);
  }
  else ssdbiu_setLastError(ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic),SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
  return (ssdb_Error_Handle)err;
}

/* ---------------------- ssdbDeleteErrorHandle ----------------------------- */
int SSDBAPI ssdbDeleteErrorHandle(ssdb_Error_Handle errHandle)
{ ssdb_LASTERR *err = (ssdb_LASTERR *)errHandle;
  int retcode = 0;
  if(ssdbApiInitCounter >= 2)
  { if(ssdbiu_checkErrorHandler(err))
    { ssdbiu_delErrorHandler(err);
      ssdbiu_setLastError(&ssdbLastErrorStatic,SSDBERR_SUCCESS,0);
      retcode++;
    }
    else ssdbiu_setLastError(&ssdbLastErrorStatic,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }
  else ssdbiu_setLastError(ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic),SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
  return retcode;
}
/* --------------------- ssdbIsErrorHandleValid ----------------------------- */
int SSDBAPI ssdbIsErrorHandleValid(ssdb_Error_Handle errHandle)
{ ssdb_LASTERR *err = (ssdb_LASTERR *)errHandle;
  int retcode = 0;
  if(ssdbApiInitCounter >= 2)
  { if((retcode = ssdbiu_checkErrorHandler(err)) != 0) ssdbiu_setLastError(&ssdbLastErrorStatic,SSDBERR_SUCCESS,0);
    else                                               ssdbiu_setLastError(&ssdbLastErrorStatic,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }
  else ssdbiu_setLastError(ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic),SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
  return retcode;
}

/* ------------------------- ssdbNewClient ---------------------------------- */
ssdb_Client_Handle SSDBAPI ssdbNewClient(ssdb_Error_Handle errHandle, const char *username, const char *password, unsigned long flags)
{ ssdb_CLIENT *cli;

  if(ssdbApiInitCounter < 2) /* check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
    return 0;
  }

  if((cli = ssdbiu_newClientHandler()) == 0)
  { ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTENOGHRES,szSSDBERRMSGNotEnoughResources);
    return 0;
  }

  cli->flags = flags;

  if(username || (flags & SSDB_CLIFLG_ENABLEDEFUNAME) != 0)
  { if(!ssdbiu_CopyString(username ? username : szDefaultUserName,cli->username,SSDB_MAX_USERNAME_SIZE,&cli->uname_len))
    { ssdbiu_delClientHandler(cli);
      ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
      return 0;
    }
  }

  if(password || (flags & SSDB_CLIFLG_ENABLEDEFPASWD) != 0)
  { if(!ssdbiu_CopyString(password ? password : szDefaultPassword,cli->password,SSDB_MAX_PASSWORD_SIZE,&cli->passwd_len))
    { ssdbiu_delClientHandler(cli);
      ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
      return 0;
    }
  }

  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return (ssdb_Client_Handle)ssdbiu_addClientHandlerToWorkList(cli);
}
/* ------------------------ ssdbDeleteClient -------------------------------- */
int SSDBAPI ssdbDeleteClient(ssdb_Error_Handle errHandle, ssdb_Client_Handle client)
{ ssdb_CLIENT *cli = (ssdb_CLIENT *)client;

  if(ssdbApiInitCounter < 2) /* Check Init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
  }

  if(!ssdbiu_checkClientHandler(cli,1)) /* check Client handler */
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }  

  if(!ssdbiu_lockClientHandler(cli))
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_RESOURCEINUSE,szSSDBERRMSGResourceInUse);
  }

  ssdbiu_DeleteClientInternal(ssdbiu_subClientHandlerFromWorkList(cli),1); /* perfect assembler code */
  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return 1; /* Success */
}

/* ----------------------- ssdbIsClientValid ------------------------------- */
int SSDBAPI ssdbIsClientValid(ssdb_Error_Handle errHandle, ssdb_Client_Handle client)
{ int retcode = 0;

  if(ssdbApiInitCounter >= 2) /* Check init status */
  { if((retcode = ssdbiu_checkClientHandler((ssdb_CLIENT *)client,1)) != 0) ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
    else                                                                    ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }
  else
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
  }
  return retcode;
}
/* ----------------------- ssdbOpenConnection ----------------------------- */
ssdb_Connection_Handle SSDBAPI ssdbOpenConnection(ssdb_Error_Handle errHandle, ssdb_Client_Handle client,const char *hostname,const char *dbname, unsigned long flags)
{ char *c;
  ssdb_CONNECTION *con;
  ssdb_CLIENT *cli = (ssdb_CLIENT *)client;

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
    return 0;
  }

  if(!ssdbiu_checkClientHandler(cli,1) || !dbname || !dbname[0] || ssdbiu_StrCheckSymbol(dbname,"\"\\/'`*:?")) /* Check parameters */
  { ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
    return 0;
  }

  if((con = ssdbiu_newConnectionHandler()) == 0) /* Create new Connection structure */
  { ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTENOGHRES,szSSDBERRMSGNotEnoughResources);
    return 0;
  }

  if(hostname || (flags & SSDB_CONFLG_DEFAULTHOSTNAME) != 0) /* initialize host name */
  { if(!ssdbiu_CopyString(hostname ? hostname : szDefaultHostName,con->hostname,SSDB_MAX_HOSTNAME_SIZE,&con->hostname_len))
    { ssdbiu_delConnectionHandler(con);
      ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter); /* incorrect host name */
      return 0;
    }
  }

  if(!ssdbiu_CopyString(dbname,con->dbname,SSDB_MAX_DATABASE_SIZE,&con->dbname_len))
  { ssdbiu_delConnectionHandler(con);
    ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter); /* incorrect data base name */
    return 0;
  }

  if(!ssdbiu_lockClientHandler(cli))
  { ssdbiu_SetSSDBError(errHandle,SSDBERR_RESOURCEINUSE,szSSDBERRMSGResourceInUse);
    return 0;
  }

  con->flags = (int)flags; /* Save connection flags */
  ssdbiu_CopyString2(cli->username,con->username,cli->uname_len,SSDB_MAX_USERNAME_SIZE,&con->uname_len);
  ssdbiu_CopyString2(cli->password,con->password,cli->passwd_len,SSDB_MAX_PASSWORD_SIZE,&con->passwd_len);

  ssdbiu_unlockClientHandler(cli);

  if(!ssdbiu_ConnectInternal(con)) /* make real connection */
  { c = mysql_error(&con->mysql);
    ssdbiu_CopySSDBError(errHandle,SSDBERR_DBCONERROR1,c ? (const char *)c : szSSDBERRMSGDatabaseConnectionError);
    ssdbiu_delConnectionHandler(con);
    return 0; /* can't connect to database */
  }

  if(!ssdbiu_SelectDBInternal(con)) /* select DB */
  { c = mysql_error(&con->mysql);
    ssdbiu_CopySSDBError(errHandle,SSDBERR_DBCONERROR2,c ? (const char *)c : szSSDBERRMSGDatabaseConnectionError);
    ssdbiu_DeleteConnectionInternal(con,0); /* close connection and delete handler */
    return 0;
  }

  if(!ssdbiu_lockClientHandler(cli))
  { ssdbiu_DeleteConnectionInternal(con,0);
    ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
    return 0;
  }

  ssdbiu_unlockClientHandler(ssdbiu_addConnectionToClient(cli,con));
  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return (ssdb_Connection_Handle)con;
}
/* --------------------- ssdbIsConnectionValid ---------------------------- */
int SSDBAPI ssdbIsConnectionValid(ssdb_Error_Handle errHandle,ssdb_Connection_Handle connection)
{ ssdb_CONNECTION *con = (ssdb_CONNECTION *)connection;

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
  }

  if(!ssdbiu_checkConnectionHandler(con) || !ssdbiu_checkClientHandler((ssdb_CLIENT *)con->lnk_parent,0))
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }

  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return 1; /* Success */
}

/* ----------------------- ssdbCloseConnection --------------------------- */
int SSDBAPI ssdbCloseConnection(ssdb_Error_Handle errHandle,ssdb_Connection_Handle connection)
{ ssdb_CLIENT *cli;
  ssdb_CONNECTION *con = (ssdb_CONNECTION *)connection;

  if(ssdbApiInitCounter < 2) /* Check init status */
  { ssdbiu_initializeErrorHandler(&ssdbLastErrorStatic);
    return ssdbiu_SetSSDBError(errHandle,SSDBERR_NOTINITED,szSSDBERRMSGNotInited);
  }

  if(!ssdbiu_lockConnectionHandler(con)) /* validate connection handle */
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_INVPARAM,szSSDBERRMSGInvalidParameter);
  }

  cli = (ssdb_CLIENT *)con->lnk_parent;
  ssdbiu_unlockConnectionHandler(con);

  if(!ssdbiu_lockClientHandler(cli)) /* lock client handle */
  { return ssdbiu_SetSSDBError(errHandle,SSDBERR_RESOURCEINUSE,szSSDBERRMSGResourceInUse);
  }

  ssdbiu_unlockClientHandler(ssdbiu_subConnectionFromClient(cli,con));
  ssdbiu_DeleteConnectionInternal(con,0);

  ssdbiu_SetSSDBError(errHandle,SSDBERR_SUCCESS,0);
  return 1; /* Success */
}

/* ----------------------- ssdbGetLastErrorCode --------------------------- */
int SSDBAPI ssdbGetLastErrorCode(ssdb_Error_Handle errHandle)
{ ssdb_LASTERR *err = (ssdb_LASTERR *)errHandle;
  if(!err) err = &ssdbLastErrorStatic;
  if(ssdbiu_checkErrorHandler(err)) return err->lasterror_code;
  return ssdbApiInitCounter < 2 ? SSDBERR_NOTINITED : SSDBERR_INVPARAM;
}

/* ---------------------- ssdbGetLastErrorString -------------------------- */
const char * SSDBAPI ssdbGetLastErrorString(ssdb_Error_Handle errHandle)
{ ssdb_LASTERR *err = (ssdb_LASTERR *)errHandle;
  if(!err) err = &ssdbLastErrorStatic;
  if(ssdbiu_checkErrorHandler(err)) return err->last_error_strptr;
  return ssdbApiInitCounter < 2 ? szSSDBERRMSGNotInited : szSSDBERRMSGInvalidParameter;
}
/* ----------------------- ssdbGetParamInt -------------------------------- */
int SSDBAPI ssdbGetParamInt(int paramName,int addParam)
{ switch(paramName) {
  case SSDBPARI_VERSIONMAJOR    : return SSDB_VMAJORNUMBER;                 /* "Get SSDB major version number" */
  case SSDBPARI_VERSIONMINOR    : return SSDB_VMINORNUMBER;                 /* "Get SSDB minor version number" */
  case SSDBPARI_MAXUSERNAMESIZE : return SSDB_MAX_USERNAME_SIZE;            /* "Get SSDB max User name string size" */
  case SSDBPAPI_MAXPASSWDSIZE   : return SSDB_MAX_PASSWORD_SIZE;            /* "Get SSDB max Password string size" */
  case SSDBPAPI_MAXHOSTNAMESIZE : return SSDB_MAX_HOSTNAME_SIZE;            /* "Get SSDB max Hostname string size" */
  case SSDBPAPI_MAXDBNAMESIZE   : return SSDB_MAX_DATABASE_SIZE;            /* "Get SSDB max DataBase name string size" */
  case SSDBPAPI_ISINITEDFLG     : return (ssdbApiInitCounter >= 2) ? 1 : 0; /* "Get SSDB isInited Flag" */
  case SSDBPAPI_CONNECTREPTCNT  : return SSDB_CONNECT_REPTCOUNTER;          /* "Get SSDB repeat connection counter" */
  case SSDBPAPI_GLOBALSTATUS    : switch(addParam) {
                                  case SSDBPAPI_GS_CLIENTALLOC  : return (int)ssdb_gst.statTotalAllocClients;
                                  case SSDBPAPI_GS_CLIENTFREE   : return (int)ssdb_gst.statTotalFreeClients;
                                  case SSDBPAPI_GS_CONNALLOC    : return (int)ssdb_gst.statTotalAllocConn;
                                  case SSDBPAPI_GS_CONNFREE     : return (int)ssdb_gst.statTotalFreeConn;
                                  case SSDBPAPI_GS_REQUESTALLOC : return (int)ssdb_gst.statTotalAllocRequest;
                                  case SSDBPAPI_GS_REQUESTFREE  : return (int)ssdb_gst.statTotalFreeRequest;
				  case SSDBPAPI_GS_REQINFOALLOC : return (int)ssdb_gst.statTotalAllocReqFielInfo;
				  case SSDBPAPI_GS_REQRESALLOC  : return (int)ssdb_gst.statTotalAllocReqResult;
                                  case SSDBPAPI_GS_ERRHALLOC    : return (int)ssdb_gst.statTotalAllocErrorHandle;
                                  case SSDBPAPI_GS_ERRHFREE     : return (int)ssdb_gst.statTotalFreeErrorHandle;				  
				  };
				  break;
  case SSDBPAPI_MAXLOCKTABLES   : return SSDB_MAX_LOCKTABLES_CNT;
  };
  return 0;
}

/* ---------------------- ssdbGetParamString ------------------------------ */
const char * SSDBAPI ssdbGetParamString(int paramName,int addParam)
{ switch(paramName) {
  case SSDBPARS_VERDESCRIPTION    : return szSSDBVersionDescriptor;
  case SSDBPARS_ERRORMSGSTRBYCODE : switch(addParam) {
				    case SSDBERR_SUCCESS       : return 0;
				    case SSDBERR_INVPARAM      : return szSSDBERRMSGInvalidParameter;
				    case SSDBERR_NOTINITED     : return szSSDBERRMSGNotInited;
				    case SSDBERR_ALREADYINIT1  : return szSSDBERRMSGAlreadyInInitPhase;
				    case SSDBERR_ALREADYINIT2  : return szSSDBERRMSGAlreadyInitialized;
				    case SSDBERR_INITERROR1    : return szSSDBERRMSGInitiError1;
				    case SSDBERR_ALREADYDONE   : return szSSDBERRMSGAlreadyInDone;
				    case SSDBERR_NOTENOGHRES   : return szSSDBERRMSGNotEnoughResources;
				    case SSDBERR_RESOURCEINUSE : return szSSDBERRMSGResourceInUse;
				    case SSDBERR_DBCONERROR1   : return szSSDBERRMSGDatabaseConnectionError;
				    case SSDBERR_DBCONERROR2   : return szSSDBERRMSGDatabaseConnectionError;
				    case SSDBERR_CONNOTOPENED  : return szSSDBERRMSGDConnectionNotOpened;
				    case SSDBERR_SQLQUERYERROR : return szSSDBERRMSGSQLRequestError;
				    case SSDBERR_PROCESSRESERR : return szSSDBERRMSGProcessResultsError;
				    case SSDBERR_DBERROR       : return szSSDBERRMSGDBError;
				    case SSDBERR_ENDOFSET      : return szSSDBERRMSGENDOFSETError;
				    };
				    break;
 case SSDBPARS_DEFAULTUSERNAME    : return szDefaultUserName;
 case SSDBPARS_DEFAULTPASSWORD    : return szDefaultPassword;
 case SSDBPARS_DEFAULTHOSTNAME    : return szDefaultHostName;
 case SSDBPARS_CONTACTADDRESS     : return szSSDBContactAddress;
 };
 return 0;
}
