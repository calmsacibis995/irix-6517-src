/* ------------------------------------------------------------------- */
/* -                         SSDBIUTIL.C                             - */
/* ------------------------------------------------------------------- */
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
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "ssdbiutil.h"
#include "ssdbthread.h"


extern ssdb_GSTATUS ssdb_gst;   /* SSDB Global Statistics Structure */

/* Client & Connection & Request & Error env struct lists */
static ssdb_CLIENT*     clientWorkList     = 0; /* Client Work list */
static ssdb_CLIENT*     clientFreeList     = 0; /* Client Free list */
static ssdb_CONNECTION* connectionFreeList = 0; /* Connection Free list */
static ssdb_REQUEST*    requestFreeList    = 0; /* Request Free list */
static ssdb_REQUEST*    requestWorkList    = 0; /* Request Work list */
static ssdb_LASTERR*    errorWorkList      = 0; /* Error Work list */
static ssdb_LASTERR*    errorFreeList      = 0; /* Error Free list */

static CRITICAL_SECTION listCriticalSection;    /* Critical section for non destructive access to different internal lists */

/* Static functions for access to Critical Sections */
/* =================================================================== */
/*                         Critical Sections                           */
/* =================================================================== */

/* -------------- ssdbiu_initCritialSections --------------------- */
int ssdbiu_initCritialSections(void)
{ return ssdbthread_init_critical_section(&listCriticalSection);
}
/* -------------- ssdbiu_doneCritialSections --------------------- */
int ssdbiu_doneCritialSections(void)
{ return ssdbthread_done_critical_section(&listCriticalSection);
}
/* =================================================================== */
/*                             Client                                  */
/* =================================================================== */
/* --------------- ssdbiu_checkClientHandler ---------------------- */
int ssdbiu_checkClientHandler(ssdb_CLIENT *cli,int strictFlg)
{ ssdb_CLIENT *c;
  int retcode = 0;
  
  if(cli)
  { if(cli->lnk_sizeof == sizeof(ssdb_CLIENT) && cli->lnk_tag == SSDB_ISTRC_TAG_CLIENT)
    { if(strictFlg)
      {	ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock free client list */
        for(c = clientWorkList;c;c = (ssdb_CLIENT *)c->lnk_next)
        { if(c == cli) { retcode++; break; }
	}
	ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock free client list */ 
      }
      else retcode++;
    }
  }
  return retcode;
}

/* --------------- ssdbiu_newClientHandler ------------------------ */
ssdb_CLIENT* ssdbiu_newClientHandler(void)
{ ssdb_CLIENT *cli;
  ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock free client list */
  if((cli = clientFreeList) != 0) { clientFreeList = (ssdb_CLIENT *)cli->lnk_next; ssdb_gst.statTotalFreeClients--; }
  ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock free client list */ 
  if(!cli) { if((cli = (ssdb_CLIENT *)malloc(sizeof(ssdb_CLIENT))) != 0) ssdb_gst.statTotalAllocClients++; }
  if(cli)
  { memset((void *)cli,0,sizeof(ssdb_CLIENT));
    /* Initialize linker structure */
    cli->lnk_sizeof  = sizeof(ssdb_CLIENT);
    cli->lnk_tag     = SSDB_ISTRC_TAG_CLIENT;
    if(!ssdbthread_init_critical_section(&cli->clientCriticalSection)) cli = ssdbiu_delClientHandler(cli);
    else cli->initCritSecFlg++;
  }
  return cli;
}

/* ---------------- ssdbiu_delClientHandler ----------------------- */
ssdb_CLIENT *ssdbiu_delClientHandler(ssdb_CLIENT *cli)
{ if(cli)
  { if(cli->lnk_sizeof == sizeof(ssdb_CLIENT) && cli->lnk_tag == SSDB_ISTRC_TAG_CLIENT)
    { if(cli->initCritSecFlg)
      {	ssdbthread_enter_critical_section(&cli->clientCriticalSection); 
	cli->initCritSecFlg = 0; cli->lnk_tag = 0; cli->in_delete_flg++;
	ssdbthread_leave_critical_section(&cli->clientCriticalSection);
	ssdbthread_done_critical_section(&cli->clientCriticalSection);
      }
      ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock free client list */
      cli->lnk_next = (void*)clientFreeList; (clientFreeList = cli)->lnk_tag = 0; ssdb_gst.statTotalFreeClients++;
      ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock free client list */ 
    }
  } 
  return 0;
}

/* ---------------- ssdbiu_lockClientHandler ---------------------- */
int ssdbiu_lockClientHandler(ssdb_CLIENT *cli)
{ int retcode = 0;
  if(cli)
  { if((cli->lnk_sizeof == sizeof(ssdb_CLIENT)) && (cli->lnk_tag == SSDB_ISTRC_TAG_CLIENT) &&
       cli->initCritSecFlg && !cli->in_delete_flg)
    { ssdbthread_enter_critical_section(&cli->clientCriticalSection);
      retcode++;
    }
  }
  return retcode;
}

/* ---------------- ssdbiu_unlockClientHandler -------------------- */
int ssdbiu_unlockClientHandler(ssdb_CLIENT *cli)
{ int retcode = 0;
  if(cli)
  { if((cli->lnk_sizeof == sizeof(ssdb_CLIENT)) && (cli->lnk_tag == SSDB_ISTRC_TAG_CLIENT) && cli->initCritSecFlg)
    { ssdbthread_leave_critical_section(&cli->clientCriticalSection);
      retcode++;
    }
  }
  return retcode;
}

/* ----------------- ssdbiu_dropFreeClientList -------------------- */
void ssdbiu_dropFreeClientList(void)
{ ssdb_CLIENT *cli;
  ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock free client list */
  while((cli = clientFreeList) != 0)
  { clientFreeList = (ssdb_CLIENT *)cli->lnk_next;
    free((void*)cli);
  }
  ssdb_gst.statTotalFreeClients = 0;
  ssdb_gst.statTotalAllocClients = 0;
  ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock free client list */ 
}

/* ------------- ssdbiu_removeFirstClientFromWorkList ------------- */
ssdb_CLIENT* ssdbiu_removeFirstClientFromWorkList(void)
{ ssdb_CLIENT *cli;
  ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock free client list */
  if((cli = clientWorkList) != 0) clientWorkList = (ssdb_CLIENT *)cli->lnk_next;
  ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock free client list */ 
  return cli;
}

/* -------------- ssdbiu_getFirstClientFromWorkList --------------- */
ssdb_CLIENT* ssdbiu_getFirstClientFromWorkList(void)
{ ssdb_CLIENT *cli;
  ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock free client list */
  cli = clientWorkList;
  ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock free client list */ 
  return cli;
}

/* --------------- ssdbiu_addClientHandlerToWorkList -------------- */
ssdb_CLIENT* ssdbiu_addClientHandlerToWorkList(ssdb_CLIENT *cli)
{ if(cli)
  { ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock Work client list */
    cli->lnk_next = (void*)clientWorkList; clientWorkList = cli;
    ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock Work client list */ 
  }
  return cli;
}

/* --------------- ssdbiu_subClientHandlerFromWorkList ------------ */
ssdb_CLIENT* ssdbiu_subClientHandlerFromWorkList(ssdb_CLIENT *cli)
{ ssdb_CLIENT **clipp;
  if(cli)
  { ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock Work client list */
    for(clipp = &clientWorkList;*clipp;clipp = (ssdb_CLIENT **)&((*clipp)->lnk_next))
    { if(cli == *clipp) { *clipp = (ssdb_CLIENT *)cli->lnk_next; break; }
    }         
    ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock Work client list */ 
  }
  return cli;
}

/* =================================================================== */
/*                         Connection                                  */
/* =================================================================== */
/* --------------  ssdbiu_newConnectionHandler --------------------- */
ssdb_CONNECTION* ssdbiu_newConnectionHandler(void)
{ ssdb_CONNECTION *con = 0;
  ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock Free Connection structure list */
  if((con = connectionFreeList) != 0) { connectionFreeList = (ssdb_CONNECTION*)con->lnk_next; ssdb_gst.statTotalFreeConn--; }
  ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock Free Connection structure list */ 
  if(!con) { if((con = (ssdb_CONNECTION *)malloc(sizeof(ssdb_CONNECTION))) != 0) ssdb_gst.statTotalAllocConn++; }
  if(con)
  { memset((void*)con,0,sizeof(ssdb_CONNECTION));
    con->lnk_sizeof = sizeof(ssdb_CONNECTION);
    con->lnk_tag    = SSDB_ISTRC_TAG_CONNECTION;
    if(!ssdbthread_init_critical_section(&con->connectionCriticalSection)) con = ssdbiu_delConnectionHandler(con);
    else con->initCritSecFlg++;
  }
  return con;
}

/* --------------- ssdbiu_delConnectionHandler --------------------- */
ssdb_CONNECTION *ssdbiu_delConnectionHandler(ssdb_CONNECTION *con)
{ if(con)
  { if((con->lnk_sizeof == sizeof(ssdb_CONNECTION)) && (con->lnk_tag == SSDB_ISTRC_TAG_CONNECTION))
    { if(con->initCritSecFlg)
      {	ssdbthread_enter_critical_section(&con->connectionCriticalSection); 
        con->initCritSecFlg = 0; con->lnk_parent = 0; con->lnk_tag = 0; con->in_delete_flg++;
        ssdbthread_leave_critical_section(&con->connectionCriticalSection);
        ssdbthread_done_critical_section(&con->connectionCriticalSection);
      }
      ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock Free Connection structure list */
      con->lnk_next = (void*)connectionFreeList; (connectionFreeList = con)->lnk_tag = 0;
      con->lnk_parent = 0; ssdb_gst.statTotalFreeConn++;
      ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock Free Connection structure list */ 
    }
  }
  return 0;
}

/* --------------- ssdbiu_checkConnectionHandler ------------------- */
int ssdbiu_checkConnectionHandler(ssdb_CONNECTION *con)
{ int retcode = 0;
  if(con)
  { if((con->lnk_sizeof == sizeof(ssdb_CONNECTION)) && (con->lnk_tag == SSDB_ISTRC_TAG_CONNECTION) && con->initCritSecFlg) retcode++;
  }
  return retcode;
}

/* ---------------- ssdbiu_lockConnectionHandler -------------------- */
int ssdbiu_lockConnectionHandler(ssdb_CONNECTION *con)
{ int retcode = 0;
  if(con)
  { if((con->lnk_sizeof == sizeof(ssdb_CONNECTION)) && (con->lnk_tag == SSDB_ISTRC_TAG_CONNECTION) &&
       con->initCritSecFlg  && !con->in_delete_flg)
    { ssdbthread_enter_critical_section(&con->connectionCriticalSection);
      retcode++;
    }
  }
  return retcode;
}

/* --------------- ssdbiu_unlockConnectionHandler ------------------- */
int ssdbiu_unlockConnectionHandler(ssdb_CONNECTION *con)
{ int retcode = 0;
  if(con)
  { if((con->lnk_sizeof == sizeof(ssdb_CONNECTION)) && (con->lnk_tag == SSDB_ISTRC_TAG_CONNECTION) && con->initCritSecFlg) 
    { ssdbthread_leave_critical_section(&con->connectionCriticalSection);
      retcode++;
    }
  }
  return retcode;
}

/* ---------------- ssdbiu_dropFreeConnectionList ------------------ */
void ssdbiu_dropFreeConnectionList(void)
{ ssdb_CONNECTION *con;
  ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock free connection list */
  while((con = connectionFreeList) != 0)
  { connectionFreeList = (ssdb_CONNECTION *)con->lnk_next;
    free((void*)con);
  }
  ssdb_gst.statTotalAllocConn = 0;
  ssdb_gst.statTotalFreeConn = 0;
  ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock free connection list */ 
}

/* =================================================================== */
/*                           Request                                   */
/* =================================================================== */
/* ----------------- ssdbiu_newRequestHandler --------------------- */
ssdb_REQUEST *ssdbiu_newRequestHandler(void)
{ ssdb_REQUEST *req = 0;
  ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock Free Request structure list */
  if((req = requestFreeList) != 0) { requestFreeList = (ssdb_REQUEST*)req->lnk_next; ssdb_gst.statTotalFreeRequest--; }
  if(!req) { if((req = (ssdb_REQUEST *)malloc(sizeof(ssdb_REQUEST))) != 0) ssdb_gst.statTotalAllocRequest++; }
  if(req)
  { memset((void*)req,0,sizeof(ssdb_REQUEST));
    req->lnk_sizeof = sizeof(ssdb_REQUEST);
    req->lnk_tag    = SSDB_ISTRC_TAG_REQUEST;
    req->worklist   = requestWorkList;
    requestWorkList = req;
  }
  ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock Free Request structure list */
  return req;
}

/* ---------------- ssdbiu_delRequestHandler ---------------------- */
void ssdbiu_delRequestHandler(ssdb_REQUEST *req)
{ ssdb_REQUEST **r;
  if(req)
  { if((req->lnk_sizeof == sizeof(ssdb_REQUEST)) && (req->lnk_tag == SSDB_ISTRC_TAG_REQUEST))
    { ssdbiu_freeRequestMySqlResult(ssdbiu_freeRequestFieldInfo(ssdbiu_freeRequestResultData(req)));
      ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock Free Request structure list */
      req->lnk_next = (void*)requestFreeList; (requestFreeList = req)->lnk_tag = 0; req->lnk_parent = 0;
      ssdb_gst.statTotalFreeRequest++;
      for(r = &requestWorkList;*r;r = &((*r)->worklist))
      {	if(*r == req)
        { *r = req->worklist;
          break;
        }
      }	
      ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock Free Request structure list */ 
    }
  }
}

/* ---------------- ssdbiu_checkRequestHandler -------------------- */
int ssdbiu_checkRequestHandler(ssdb_REQUEST *req)
{ ssdb_REQUEST *r;
  int retcode = 0;
  if(req)
  { ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock Free Request structure list */
    for(r = requestWorkList;r;r = r->worklist)
    { if(r == req)
      {	if((req->lnk_sizeof == sizeof(ssdb_REQUEST)) && (req->lnk_tag == SSDB_ISTRC_TAG_REQUEST)) { retcode++; break; }
      }
    }
    ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock Free Request structure list */
  }
  return retcode;
}

/* ---------------- ssdbiu_dropFreeRequestList -------------------- */
void ssdbiu_dropFreeRequestList(void)
{ ssdb_REQUEST *req;
  ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock free request list */
  while((req = requestFreeList) != 0)
  { requestFreeList = (ssdb_REQUEST *)req->lnk_next;
    free((void*)req);
    ssdb_gst.statTotalFreeRequest--;
    ssdb_gst.statTotalAllocRequest--;
  }
  ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock free request list */ 
}

/* ---------------- ssdbiu_freeRequestFieldInfo ------------------- */
ssdb_REQUEST *ssdbiu_freeRequestFieldInfo(ssdb_REQUEST *req)
{ if(req)
  { if(req->field_info_ptr && req->field_info_ptr != req->field_info)
    { free((void*)req->field_info_ptr);
      ssdb_gst.statTotalAllocReqFielInfo--;
    }
    req->field_info_ptr = 0;
    req->field_info_size = 0;
  }
  return req;
}

/* -------------- ssdbiu_allocRequestFieldInfo ------------------- */
int ssdbiu_allocRequestFieldInfo(ssdb_REQUEST *req,int numfields)
{ int retcode = 0;
  if(req && numfields >= 0)
  { ssdbiu_freeRequestFieldInfo(req);
    if(numfields > SSDB_MAX_FIELDS_INREQ)
    { if((req->field_info_ptr = (ssdb_FIELDINFO *)malloc(sizeof(ssdb_FIELDINFO)*numfields)) != 0)
      {	req->field_info_size = numfields;
        ssdb_gst.statTotalAllocReqFielInfo++;
        retcode++;
      }
    }
    else
    { req->field_info_ptr = req->field_info;
      req->field_info_size = numfields;
      retcode++; 		
    }
    if(req->field_info_ptr) memset(req->field_info_ptr,0,numfields*sizeof(ssdb_FIELDINFO));
  }
  return retcode;
}
/* ------------- ssdbiu_freeRequestMySqlResult ------------------- */
ssdb_REQUEST *ssdbiu_freeRequestMySqlResult(ssdb_REQUEST *req)
{ if(req)
  { if(req->res) { mysql_free_result(req->res); req->res = 0; } /* Free MySQL results */
  }
  return req;
}

/* -------------- ssdbiu_allocRequestResultData ------------------ */
int ssdbiu_allocRequestResultData(ssdb_REQUEST *req,int size)
{ int retcode = 0;
  if(req)
  { ssdbiu_freeRequestResultData(req);
    if((req->data_size = size) != 0)
    { if((req->data_ptr = malloc(size)) != 0)
      {	memset(req->data_ptr,0,size);
        ssdb_gst.statTotalAllocReqResult++;
	retcode++;
      }
    }    
  }
  return retcode;
}
/* -------------- ssdbiu_freeRequestResultData ------------------- */
ssdb_REQUEST *ssdbiu_freeRequestResultData(ssdb_REQUEST *req)
{ if(req)
  { if(req->data_ptr)
    { free((void*)req->data_ptr); req->data_ptr = 0;
      ssdb_gst.statTotalAllocReqResult--;
    }
    req->data_size = 0;
  }
  return req;
}

/* =================================================================== */
/*                           Error                                     */
/* =================================================================== */
/* -------------- ssdbiu_initializeErrorHandler ----------------- */
ssdb_LASTERR *ssdbiu_initializeErrorHandler(ssdb_LASTERR *err)
{ if(err)
  { memset((void*)err,0,sizeof(ssdb_LASTERR));
    err->lnk_sizeof = sizeof(ssdb_LASTERR);
    err->lnk_tag    = SSDB_ISTRC_TAG_LASTERROR;
  }
  return err;
}

/* ----------------- ssdbiu_newErrorHandler --------------------- */
ssdb_LASTERR *ssdbiu_newErrorHandler(void)
{ ssdb_LASTERR *err = 0;
  ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock Free LastError structure list */
  if((err = errorFreeList) != 0) { errorFreeList = (ssdb_LASTERR*)err->lnk_next; ssdb_gst.statTotalFreeErrorHandle--; }
  ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock Free LastError structure list */ 
  if(!err) { if((err = (ssdb_LASTERR *)malloc(sizeof(ssdb_LASTERR))) != 0) ssdb_gst.statTotalAllocErrorHandle++; }
  ssdbiu_initializeErrorHandler(err);
  return err;
}

/* ---------------- ssdbiu_delErrorHandler ---------------------- */
void ssdbiu_delErrorHandler(ssdb_LASTERR *err)
{ if(err)
  { if((err->lnk_sizeof == sizeof(ssdb_LASTERR)) && (err->lnk_tag == SSDB_ISTRC_TAG_LASTERROR))
    { ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock Free LastError structure list */
      err->lnk_next = (void*)errorFreeList; (errorFreeList = err)->lnk_tag = 0;
      err->lnk_parent = 0; ssdb_gst.statTotalFreeErrorHandle++;
      ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock Free LastError structure list */ 
    }
  }
}

/* ---------------- ssdbiu_checkErrorHandler -------------------- */
int ssdbiu_checkErrorHandler(ssdb_LASTERR *err)
{ int retcode = 0;
  if(err)
  { if((err->lnk_sizeof == sizeof(ssdb_LASTERR)) && (err->lnk_tag == SSDB_ISTRC_TAG_LASTERROR)) retcode++;
  }
  return retcode;
}

/* ---------------- ssdbiu_dropFreeErrorList -------------------- */
void ssdbiu_dropFreeErrorList(void)
{ ssdb_LASTERR *err;
  ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock free LastError structure list */
  while((err = errorFreeList) != 0)
  { errorFreeList = (ssdb_LASTERR *)err->lnk_next;
    free((void*)err);
    ssdb_gst.statTotalAllocErrorHandle--;
    ssdb_gst.statTotalFreeErrorHandle--;
  }
  ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock free LastError structure list */ 
}

/* ---------------- ssdbiu_dropWorkErrorList -------------------- */
void ssdbiu_dropWorkErrorList(void)
{ ssdb_LASTERR *err;
  ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock free/work LastError structure list */
  while((err = errorWorkList) != 0)
  { errorWorkList = (ssdb_LASTERR *)err->lnk_next;
    err->lnk_next = (void*)errorFreeList; (errorFreeList = err)->lnk_tag = 0; err->lnk_parent = 0;
  }
  ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock free/work LastError structure list */ 
}

/* -------------- ssdbiu_addErrorHandlerToWorkList -------------- */
ssdb_LASTERR *ssdbiu_addErrorHandlerToWorkList(ssdb_LASTERR *err)
{ if(err)
  { if((err->lnk_sizeof == sizeof(ssdb_LASTERR)) && (err->lnk_tag == SSDB_ISTRC_TAG_LASTERROR))
    { ssdbthread_enter_critical_section(&listCriticalSection);   /* Lock Work LastError structures list */
      err->lnk_next   = (void*)errorWorkList; errorWorkList = err;
      ssdbthread_leave_critical_section(&listCriticalSection);   /* Unlock Work LastError structures list */ 
    }
  }
  return err;
}

/* ------------- ssdbiu_subErrorHandlerFromWorkList ------------- */
ssdb_LASTERR *ssdbiu_subErrorHandlerFromWorkList(ssdb_LASTERR *err)
{ ssdb_LASTERR **epp;
  if(err)
  { if((err->lnk_sizeof == sizeof(ssdb_LASTERR)) && (err->lnk_tag == SSDB_ISTRC_TAG_LASTERROR))
    { for(epp = &errorWorkList;*epp;epp = (ssdb_LASTERR**)&((*epp)->lnk_next))
      { if(*epp == err) { *epp = err->lnk_next; break; }
      }
    }
  }
  return err;
}

/* =================================================================== */
/*                       Misc: Client & Connect                        */
/* =================================================================== */

/* --------------- ssdbiu_getClientHandleByAny --------------------- */
ssdb_CLIENT *ssdbiu_getClientHandleByAny(void *something)
{ ssdb_CLIENT *cli     = (ssdb_CLIENT *)something;
  ssdb_CONNECTION *con = (ssdb_CONNECTION *)something;
  ssdb_REQUEST *req    = (ssdb_REQUEST *)something;

  if(something)
  { if(ssdbiu_checkRequestHandler(req))	con = (ssdb_CONNECTION *)req->lnk_parent;
    if(ssdbiu_checkConnectionHandler(con)) cli = (ssdb_CLIENT *)con->lnk_parent;
    if(!ssdbiu_checkClientHandler(cli,0)) cli = 0;
  }
  return cli;
}

/* ----------- ssdbiu_removeFirstConnectionFromClient -------------- */
ssdb_CONNECTION *ssdbiu_removeFirstConnectionFromClient(ssdb_CLIENT *cli)
{ ssdb_CONNECTION *con = 0;
  if(cli)
  { if((con = cli->conn_list) != 0)
    { con->lnk_parent = 0;
      cli->conn_list = (ssdb_CONNECTION *)con->lnk_next;
    }
  }
  return con;
}

/* --------------- ssdbiu_addConnectionToClient -------------------- */
ssdb_CLIENT *ssdbiu_addConnectionToClient(ssdb_CLIENT *cli,ssdb_CONNECTION *con)
{ if(cli && con)
  { con->lnk_next = (void *)cli->conn_list; (cli->conn_list = con)->lnk_parent = cli;
  }
  return cli;
}

/* ---------------- ssdbiu_subConnectionFromClient ----------------- */
ssdb_CLIENT *ssdbiu_subConnectionFromClient(ssdb_CLIENT *cli,ssdb_CONNECTION *con)
{ ssdb_CONNECTION **cpp;
  if(cli && con)
  { con->lnk_parent = 0;
    for(cpp = &cli->conn_list;*cpp;cpp = (ssdb_CONNECTION **)&((*cpp)->lnk_next))
    { if(*cpp == con) { *cpp = (ssdb_CONNECTION *)con->lnk_next; break; }
    }
  }
  return cli;
}

/* ----------------- ssdbiu_checkConnectionInClient ----------------- */
int ssdbiu_checkConnectionInClient(ssdb_CLIENT *cli,ssdb_CONNECTION *con)
{ ssdb_CONNECTION *c;
  int retcode = 0;
  if(cli && con)
  { for(c = cli->conn_list;c;c = (ssdb_CONNECTION *)c->lnk_next)
    { if(c == con)
      {	retcode++;
	break;
      }
    }
  }
  return retcode;
}

/* =================================================================== */
/*                       Misc: Connect & Request                       */
/* =================================================================== */

/* -------------- ssdbiu_addRequestHandlerToConnect ---------------- */
ssdb_REQUEST *ssdbiu_addRequestHandlerToConnect(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ if(con && req && req->lnk_sizeof == sizeof(ssdb_REQUEST) && req->lnk_tag == SSDB_ISTRC_TAG_REQUEST)
  { req->lnk_next = con->req_list; (con->req_list = req)->lnk_parent = con;
  }
  return req;
}

/* -------------- ssdbiu_subRequestHandlerFromConnect -------------- */
ssdb_REQUEST *ssdbiu_subRequestHandlerFromConnect(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ ssdb_REQUEST **rqq;
  if(con && req && (req->lnk_sizeof == sizeof(ssdb_REQUEST)) && (req->lnk_tag == SSDB_ISTRC_TAG_REQUEST))
  { req->lnk_parent = 0;
    for(rqq = &con->req_list;*rqq;rqq = (ssdb_REQUEST **)&((*rqq)->lnk_next))
    { if(*rqq == req)
      {	*rqq = (ssdb_REQUEST *)req->lnk_next;
        break;
      }
    }
  }
  return req;
}

/* ------------ ssdbiu_removeFirstRequestHandlerFromConnect -------- */
ssdb_REQUEST *ssdbiu_removeFirstRequestHandlerFromConnect(ssdb_CONNECTION *con)
{ ssdb_REQUEST *req = 0;
  if(con)
  { if((req = con->req_list) != 0)
    { con->req_list = (ssdb_REQUEST *)req->lnk_next;
      req->lnk_parent = 0; 
    }
  }
  return req;
}

/* --------------- ssdbiu_checkRequestHandlerInConnect ------------- */
int ssdbiu_checkRequestHandlerInConnect(ssdb_CONNECTION *con,ssdb_REQUEST *req)
{ ssdb_REQUEST *r;
  int retcode = 0;
  if(con && req && (req->lnk_sizeof == sizeof(ssdb_REQUEST)) && (req->lnk_tag == SSDB_ISTRC_TAG_REQUEST))
  { for(r = con->req_list;r;r = (ssdb_REQUEST *)r->lnk_next)
    { if(r == req)
      {	retcode++;
        break;
      }
    }
  }
  return retcode;
}

/* =================================================================== */
/*                       Misc: Convertors                              */
/* =================================================================== */

/* ------------------- ssdbiu_convert_blob_2_blob ----------------- */
int ssdbiu_convert_blob_2_blob(const char *srcmem,char *destmem,int size,int field_size)
{ if(destmem && srcmem && size && field_size)
  { memset(destmem,0,field_size);
    strncpy(destmem,srcmem,field_size);
    destmem[field_size-1] = 0;
  }
  /*  memcpy(destmem,srcmem,size); */
  return 1;
}

/* ------------------- ssdbiu_convert_char_2_char ----------------- */
int ssdbiu_convert_char_2_char(const char *srcmem, char *destmem,int size,int field_size)
{ if(destmem && srcmem) *destmem = *srcmem;
  return 1;
}

/* ------------------ ssdbiu_convert_string_2_short --------------- */
int ssdbiu_convert_string_2_short(const char *srcmem, char *destmem, int size,int field_size)
{ short i;
  if(srcmem && destmem)
  { i = (short)atoi(srcmem);
    memcpy(destmem,&i,sizeof(short));
  }
  return 1;
}

/* ------------------- ssdbiu_convert_string_2_int ---------------- */
int ssdbiu_convert_string_2_int(const char *srcmem,char *destmem,int size,int field_size)
{ if(destmem) *((int*)destmem) = srcmem ? atoi(srcmem) : 0;
  return 1;
}

/* ------------------- ssdbiu_convert_string_2_long --------------- */
int ssdbiu_convert_string_2_long(const char *srcmem,char *destmem,int size,int field_size)
{ if(destmem) *((long *)destmem) = srcmem ? atol(srcmem) : 0; 
  return 1;
}

#ifndef _MSC_VER
/* ------------------- ssdbiu_convert_string_2_longlong --------------- */
int ssdbiu_convert_string_2_longlong(const char *srcmem,char *destmem,int size,int field_size)
{ if(destmem) *((long long *)destmem) = srcmem ? atoll(srcmem) : 0;
  return 1;
}
#endif

/* ------------------ ssdbiu_convert_string_2_float --------------- */
int ssdbiu_convert_string_2_float(const char *srcmem,char *destmem,int size,int field_size)
{ if(destmem) *((float*)destmem) = srcmem ? atof(srcmem) : (float)0.0;
  return 1;
}

/* ------------------ ssdbiu_convert_string_2_double -------------- */
int ssdbiu_convert_string_2_double(const char *srcmem, char *destmem,int size,int field_size)
{ if(destmem) *((double*)destmem) = srcmem ? (double)atof(srcmem) : (double)0.0;
  return 1;
}

/* ------------------- ssdbiu_getFieldByteSize -------------------- */
int ssdbiu_getFieldByteSize(int mysql_type, ssdbiu_typeconvert_request **fptr)
{ int retcode;
  switch(mysql_type) {
  case FIELD_TYPE_DECIMAL     :
  case FIELD_TYPE_TINY_BLOB   :
  case FIELD_TYPE_MEDIUM_BLOB :
  case FIELD_TYPE_LONG_BLOB   :
  case FIELD_TYPE_BLOB        :
  case FIELD_TYPE_VAR_STRING  :
  case FIELD_TYPE_STRING      : if(fptr) *fptr = ssdbiu_convert_blob_2_blob;
                                retcode = 0;
				break;
  case FIELD_TYPE_TINY        : if(fptr) *fptr = ssdbiu_convert_char_2_char;
				retcode = sizeof(char);
				break;
  case FIELD_TYPE_SHORT       : if(fptr) *fptr = ssdbiu_convert_string_2_short;
				retcode = sizeof(short);
				break;
  case FIELD_TYPE_LONG        : if(fptr) *fptr = ssdbiu_convert_string_2_long;
				retcode = sizeof(int);
				break;
  case FIELD_TYPE_FLOAT       : if(fptr) *fptr = ssdbiu_convert_string_2_float;
				retcode = sizeof(float);
				break;
  case FIELD_TYPE_DOUBLE      : if(fptr) *fptr = ssdbiu_convert_string_2_double;
				retcode = sizeof(double);
				break;
#ifdef _MSC_VER
  case FIELD_TYPE_LONGLONG    : if(fptr) *fptr = ssdbiu_convert_string_2_long;
				retcode = sizeof(unsigned long); 
				break;
#else
  case FIELD_TYPE_LONGLONG    : if(fptr) *fptr = ssdbiu_convert_string_2_longlong;
                                retcode = sizeof(unsigned long long);
                                break;
#endif
  case FIELD_TYPE_INT24       : if(fptr) *fptr = ssdbiu_convert_string_2_int;
				retcode = sizeof(int);
				break;
  case FIELD_TYPE_NULL        :
  case FIELD_TYPE_TIMESTAMP   :
  case FIELD_TYPE_DATE        :
  case FIELD_TYPE_TIME        :
  case FIELD_TYPE_DATETIME    :
  case FIELD_TYPE_YEAR        :
  case FIELD_TYPE_NEWDATE     : retcode = (-1);
                                break;
  default                     : retcode = (-1);
  };
  return retcode;
}

/* =================================================================== */
/*                            String                                   */
/* =================================================================== */

/* ---------------- ssdbiu_makeStringFromString -------------------- */
char *ssdbiu_makeStringFromString(const char *srcstr,int *dstlen)
{ char *str = 0;
  int strsize = 0;
  if(srcstr)
  { strsize = strlen(srcstr);
    if((str = malloc(strsize+1)) != 0)
    { if(strsize) strcpy(str,srcstr);
      else        str[0] = 0;
    }
    else strsize = 0;
  }
  if(dstlen) *dstlen = strsize;
  return str;
}
/* -------------------- ssdbiu_freeString -------------------------- */
void ssdbiu_freeString(char *str)
{ if(str) free(str);
}

/* -------------------- ssdbiu_CopyString -------------------------- */
int ssdbiu_CopyString(const char *src,char *dst,int dst_max_size,int *dst_actual_size)
{ int src_size;
  if(!src || !dst || !dst_max_size) return 0;
  if((src_size = strlen(src)) >= dst_max_size) return 0;
  if(src_size) strcpy(dst,src);  
  else         dst[0] = 0;
  if(dst_actual_size) *dst_actual_size = src_size;
  return 1;
}
/* -------------------- ssdbiu_CopyString2 ------------------------- */
int ssdbiu_CopyString2(const char *src,char *dst,int src_actual_size,int dst_max_size,int *dst_actual_size)
{ if(!src || !dst) return 0;
  memset(dst,0,dst_max_size+1);
  if(src_actual_size > dst_max_size) src_actual_size = dst_max_size;
  if(src_actual_size) memcpy(dst,src,src_actual_size);
  if(dst_actual_size) *dst_actual_size = src_actual_size;
  return 1;
}

/* ------------------- ssdbiu_StrCheckSymbol ----------------------- */
int ssdbiu_StrCheckSymbol(const char *srcstr,const char *patterns)
{ int i,j,src_len,ptr_len;
  int retcode = 0;
  if(srcstr && srcstr[0] && patterns && patterns[0])
  { src_len = strlen(srcstr);
    ptr_len = strlen(patterns);
    for(i = 0;i < src_len && !retcode;i++)
    { for(j = 0;j < ptr_len;j++)
      {	if(srcstr[i] == patterns[j])
        { retcode++;
          break;
        }
      }
    }
  }
  return retcode;
}
