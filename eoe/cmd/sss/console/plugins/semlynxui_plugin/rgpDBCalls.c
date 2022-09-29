#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <pwd.h>
#include <stddef.h>
#include <sys/stat.h>
#include <malloc.h>

#include "rgpDBCalls.h"
#include "rgpFormat.h"

#include <ssdbapi.h>
#include <ssdberr.h>

typedef struct 
{
  sscErrorHandle          hError         ;
  ssdb_Client_Handle      hSSDBClient    ;
  ssdb_Connection_Handle  hSSDBConnection;
  ssdb_Error_Handle       hSSDBError     ;
  ssdb_Request_Handle     hSSDBRequest   ;
} SSDBContext;


#define SQL_MAX_CONTEXT_STACK 8

typedef struct 
{
  int         bFirst;
} SQLContext ;

typedef struct
{
  CMDPAIR  * Vars;
  int        Varc;
  char     * pBuf;
  int        nBuf;
  int        nBufUsed;
  int        nErrorCode;
  int        ErrVarType;
  const char*ErrVarName; 
  int        ContextIdx;
  SQLContext Context[SQL_MAX_CONTEXT_STACK];
} SQLRequest;

/* ------------------------------ ---------------------------------------------*/
/* -- Static Variables                                                         */
/* ------------------------------ ---------------------------------------------*/

static const char szSSDBDefaultUserName       [] = "";
static const char szSSDBDefaultPassword       [] = "";
static const char szSSDBDefaultHostName       [] = "localhost";
static const char szSSDBDefaultDatabaseName   [] = "ssdb";

/* ------------------------------ ---------------------------------------------*/
/* -- Error Messages                                                           */
/* ------------------------------ ---------------------------------------------*/

static const char szErrMsgCantAllocErrHandle  [] = "Can't alloc SSDB error handle";
static const char szErrMsgCantAllocCliHandle  [] = "Can't alloc SSDB client handle";
static const char szErrMsgCantOpenConnection  [] = "Can't open SSDB connection handle";
static const char szErrMsgSSDBRequestFailed   [] = "SSDB request \"%s\" failed";
static const char szErrMsgSSDBSeekRowFailed   [] = "SSDB SeekRow failed for row # %d";
static const char szErrMsgSSDBGetRowFailed    [] = "SSDB GetRow failed";
static const char szErrMsgGetNumRecFailed     [] = "SSDB GetNumRecords failed ";
static const char szErrMsgGetNextFieldFailed  [] = "SSDB GetNextField  failed ";

/* ------------------------------ ---------------------------------------------*/
/* -- Table and Fields Names                                                   */
/* ------------------------------ ---------------------------------------------*/

static const char szToolTbl                   [] = "tool";
static const char szSystemTbl                 [] = "system"; 
static const char szClassTbl                  [] = "event_class";
static const char szEventTypeTbl              [] = "event_type";
static const char szEventActionTbl            [] = "event_action";
static const char szActionRefTbl              [] = "event_actionref";

static const char  szSysIDFldName             [] = "sys_id";
static const char  szClassIDFldName           [] = "class_id";
static const char  szClassDescFldName         [] = "class_desc";
static const char  szEventIDFldName           [] = "type_id";
static const char  szEventDescrFldName        [] = "type_desc";
static const char  szThrottledFldName         [] = "throttled";
static const char  szSehThrotFldName          [] = "sehthrottle";
static const char  szSehFreqFldName           [] = "sehfrequency";
static const char  szPriorityFldName          [] = "priority";
static const char  szEnabledFldName           [] = "enabled";

static const char  szActionIDFldName          [] = "action_id";
static const char  szFrwdHostNameFldName      [] = "forward_hostname";
static const char  szDsmThrottleFldName       [] = "dsmthrottle";
static const char  szDsmFreqFldName           [] = "dsmfrequency";
static const char  szActionFldName            [] = "action";
static const char  szRetryFldName             [] = "retrycount";
static const char  szTimeoutValueFldName      [] = "timeoutval";
static const char  szUserStringFldName        [] = "userstring";
static const char  szActionDescFldName        [] = "action_desc";
static const char  szPrivateFldName           [] = "private";

static const char szLoggingFlagName           [] = "GLOBAL_LOGGING_FLAG";
static const char szFilterFlagName            [] = "GLOBAL_THROTTLING_FLAG";
static const char szActionFlagName            [] = "GLOBAL_ACTION_FLAG";


/* ------------------------------ ---------------------------------------------*/
/* -- Simple SQLRequests                                                       */
/* ------------------------------ ---------------------------------------------*/

static const char szSQLGetGlobalFlags [] = "select tool_option, option_default "
                                           "from tool "
                                           "where tool_name = 'SEM' and "
                                           "(tool_option = '%s' or "
                                           " tool_option = '%s' or "
                                           " tool_option = '%s')";

static const char szSQLSetGlobalFlag  [] = "update tool set option_default = \"%d\""
                                           "where tool_option = '%s' and "
                                           "tool_name = 'SEM'";

static const char szSQLGetSysIDList   [] = "select sys_id from system where active=1";

                          

/* ------------------------------ ---------------------------------------------*/
/* New ( Universal) SQLs                                                       */
/* ------------------------------ ---------------------------------------------*/

static const char szIndexFldName [] = "index";
static const char szPageFldName  [] = "page";

#define  EVENT_REC_SYM_IDX    2
#define  EVENT_REC_SYM_NUM   11
static CMDPAIR EventRecSymTable [] = {{ (char*)szIndexFldName     , NULL },
                                      { (char*)szPageFldName      , NULL },
                                      { (char*)szClassIDFldName   , NULL },
                                      { (char*)szClassDescFldName , NULL },
                                      { (char*)szEventIDFldName   , NULL },
                                      { (char*)szEventDescrFldName, NULL },
                                      { (char*)szThrottledFldName , NULL },
                                      { (char*)szSehThrotFldName  , NULL },
                                      { (char*)szSehFreqFldName   , NULL },
                                      { (char*)szPriorityFldName  , NULL },
                                      { (char*)szEnabledFldName   , NULL }};

#define  CLASS_REC_SYM_IDX    2
#define  CLASS_REC_SYM_NUM    4
static CMDPAIR ClassRecSymTable [] = {{ (char*)szIndexFldName     , NULL },
                                      { (char*)szPageFldName      , NULL },
                                      { (char*)szClassIDFldName   , NULL },
                                      { (char*)szClassDescFldName , NULL }};
                                            
#define  ACTION_REC_SYM_IDX   2
#define  ACTION_REC_SYM_NUM  12
static CMDPAIR ActionRecSymTable[] = { {(char*)szIndexFldName       , NULL },
                                       {(char*)szPageFldName        , NULL },
                                       {(char*)szActionIDFldName    , NULL },
                                       {(char*)szActionDescFldName  , NULL },
                                       {(char*)szActionFldName      , NULL },
                                       {(char*)szUserStringFldName  , NULL },
                                       {(char*)szDsmThrottleFldName , NULL },
                                       {(char*)szDsmFreqFldName     , NULL },
                                       {(char*)szTimeoutValueFldName, NULL },
                                       {(char*)szRetryFldName       , NULL },
                                       {(char*)szFrwdHostNameFldName, NULL },
                                       {(char*)szPrivateFldName     , NULL }};

/* ------------------------------ ---------------------------------------------*/
/* -- FreeSSDBRequest                                                          */
/* ------------------------------ ---------------------------------------------*/

void FreeSSDBRequest ( SSDBContext *pSSDB )
{
   if ( pSSDB )
   {
     if ( pSSDB->hSSDBError )
     {
       if ( pSSDB->hSSDBRequest != NULL ) 
       {
          if ( ssdbIsRequestValid ( pSSDB->hSSDBError, pSSDB->hSSDBRequest ))
               ssdbFreeRequest    ( pSSDB->hSSDBError, pSSDB->hSSDBRequest);     
          pSSDB->hSSDBRequest = NULL;
       }   
     }  
   }    
}

/* ------------------------------ ---------------------------------------------*/
/* -- CleanSSDB                                                                */
/* ------------------------------ ---------------------------------------------*/
void CleanSSDBContext ( SSDBContext *pSSDB )
{
   if ( pSSDB )
   {
     if ( pSSDB->hSSDBError )
     {
       /* Free SSDB Request if any */
       FreeSSDBRequest ( pSSDB );
 
       /* Close Connection if any */      
       if ( pSSDB->hSSDBConnection != NULL ) 
       {
         if ( ssdbIsConnectionValid ( pSSDB->hSSDBError, pSSDB->hSSDBConnection ) )
              ssdbCloseConnection   ( pSSDB->hSSDBError, pSSDB->hSSDBConnection );
              
         pSSDB->hSSDBConnection = NULL;
       }       

       /* Delete Client if Any */
       if ( pSSDB->hSSDBClient ) 
       {  
         if (ssdbIsClientValid ( pSSDB->hSSDBError, pSSDB->hSSDBClient ))
             ssdbDeleteClient  ( pSSDB->hSSDBError, pSSDB->hSSDBClient);
         pSSDB->hSSDBClient = NULL;
       }
       
       /* Delete Error Handle */
       ssdbDeleteErrorHandle(pSSDB->hSSDBError);
       pSSDB->hSSDBError = NULL;
     }            
     
     /* Detach sscErrorHandle */
     pSSDB->hError = NULL;
   }
}

/* ------------------------------ ---------------------------------------------*/
/* -- OpenSSDB --                                                              */
/* ------------------------------ ---------------------------------------------*/
int OpenSSDBContext ( sscErrorHandle hError, SSDBContext *pSSDB )
{
   /* Init Handles */
   pSSDB->hSSDBError      = NULL;
   pSSDB->hSSDBClient     = NULL;
   pSSDB->hSSDBConnection = NULL;
   pSSDB->hSSDBRequest    = NULL;
   pSSDB->hError          = hError; /* Attach Error Handle */

   /* Connect to the data base */   
   pSSDB->hSSDBError = ssdbCreateErrorHandle();
   if (pSSDB->hSSDBError == NULL )
   {
      sscError( hError, (char*) szErrMsgCantAllocErrHandle );
      goto Error;
   }
   
   /* Create ssdb Client */
   pSSDB->hSSDBClient = ssdbNewClient( pSSDB->hSSDBError, 
                                       szSSDBDefaultUserName, 
                                       szSSDBDefaultPassword, 0 );
   if ( pSSDB->hSSDBClient == NULL)
   {
      sscError( hError, (char*)szErrMsgCantAllocCliHandle );
      goto Error;
   }
   
   /* Establish a connection to a database */
   pSSDB->hSSDBConnection = ssdbOpenConnection ( pSSDB->hSSDBError, 
                                                 pSSDB->hSSDBClient,
                                                 szSSDBDefaultHostName,
                                                 szSSDBDefaultDatabaseName, 0 );
   if ( pSSDB->hSSDBConnection == NULL )
   {
      sscError( hError, (char*)szErrMsgCantOpenConnection );
      goto Error;
   }

   return 1;
 
 Error:  
 
   CleanSSDBContext  ( pSSDB );
   
   return 0;
}

/* ------------------------------ ---------------------------------------------*/
/* -- SendSSDBSelectRequest                                                    */
/* ------------------------------ ---------------------------------------------*/
int SendSSDBSelectRequest ( SSDBContext *pSSDB, const char *szTable, char *szRequest)
{
   int nRows;
  
   /* Close Prev Request if Any */
   if ( pSSDB->hSSDBRequest != NULL ) 
   {
     if ( ssdbIsRequestValid ( pSSDB->hSSDBError, pSSDB->hSSDBRequest ))
          ssdbFreeRequest    ( pSSDB->hSSDBError, pSSDB->hSSDBRequest);     
     pSSDB->hSSDBRequest = NULL;
   }   
  
   if ( szTable ) 
   {
     pSSDB->hSSDBRequest = ssdbSendRequestTrans( pSSDB->hSSDBError  , pSSDB->hSSDBConnection, 
                                                 SSDB_REQTYPE_SELECT, szTable, szRequest );
   } else {
     pSSDB->hSSDBRequest = ssdbSendRequest     ( pSSDB->hSSDBError  , pSSDB->hSSDBConnection, 
                                                 SSDB_REQTYPE_SELECT, szRequest );
   }

   if ( pSSDB->hSSDBRequest == NULL )                                              
        return -1;
   
   nRows = ssdbGetNumRecords ( pSSDB->hSSDBError, pSSDB->hSSDBRequest );
   if ( ssdbGetLastErrorCode ( pSSDB->hSSDBError ) != SSDBERR_SUCCESS )
   {
      sscError( pSSDB->hError, (char*) szErrMsgGetNumRecFailed );
      return -1;
   }
   return nRows;
}

/* ------------------------------ ---------------------------------------------*/
/* -- SendSSDBUpdateRequest                                                    */
/* ------------------------------ ---------------------------------------------*/
int SendSSDBUpdateRequest ( SSDBContext *pSSDB, const char *szTable, char *szRequest)
{
   /* Close Prev Request if Any */
   if ( pSSDB->hSSDBRequest != NULL ) 
   {
     if ( ssdbIsRequestValid ( pSSDB->hSSDBError, pSSDB->hSSDBRequest ))
          ssdbFreeRequest    ( pSSDB->hSSDBError, pSSDB->hSSDBRequest);     
     pSSDB->hSSDBRequest = NULL;
   }   
  
   /* Send Transaction */
   if ( szTable ) 
   {
     pSSDB->hSSDBRequest = ssdbSendRequestTrans( pSSDB->hSSDBError  , pSSDB->hSSDBConnection, 
                                                 SSDB_REQTYPE_UPDATE, szTable, szRequest );
   } else {
     pSSDB->hSSDBRequest = ssdbSendRequest     ( pSSDB->hSSDBError  , pSSDB->hSSDBConnection, 
                                                 SSDB_REQTYPE_UPDATE, szRequest );
   }
   if ( pSSDB->hSSDBRequest == NULL )                                              
        return -1;
   return  0;
}
/* ------------------------------ ---------------------------------------------*/
/* -- SendSSDBInsertRequest                                                    */
/* ------------------------------ ---------------------------------------------*/
int SendSSDBInsertRequest ( SSDBContext *pSSDB, const char *szTable, char *szRequest)
{
   /* Close Prev Request if Any */
   if ( pSSDB->hSSDBRequest != NULL ) 
   {
     if ( ssdbIsRequestValid ( pSSDB->hSSDBError, pSSDB->hSSDBRequest ))
          ssdbFreeRequest    ( pSSDB->hSSDBError, pSSDB->hSSDBRequest);     
     pSSDB->hSSDBRequest = NULL;
   }   
  
   /* Send Transaction */
   if ( szTable ) 
   {
     pSSDB->hSSDBRequest = ssdbSendRequestTrans( pSSDB->hSSDBError  , pSSDB->hSSDBConnection, 
                                                 SSDB_REQTYPE_INSERT, szTable, szRequest );
   } else {
     pSSDB->hSSDBRequest = ssdbSendRequest     ( pSSDB->hSSDBError  , pSSDB->hSSDBConnection, 
                                                 SSDB_REQTYPE_INSERT, szRequest );
   }
   if ( pSSDB->hSSDBRequest == NULL )                                              
        return -1;
   return  0;
}
/* ------------------------------ ---------------------------------------------*/
/* -- SendSSDBReplaceRequest                                                   */
/* ------------------------------ ---------------------------------------------*/
int SendSSDBReplaceRequest ( SSDBContext *pSSDB, const char *szTable, char *szRequest)
{
   /* Close Prev Request if Any */
   if ( pSSDB->hSSDBRequest != NULL ) 
   {
     if ( ssdbIsRequestValid ( pSSDB->hSSDBError, pSSDB->hSSDBRequest ))
          ssdbFreeRequest    ( pSSDB->hSSDBError, pSSDB->hSSDBRequest);     
     pSSDB->hSSDBRequest = NULL;
   }   
  
   /* Send Transaction */
   if ( szTable ) 
   {
     pSSDB->hSSDBRequest = ssdbSendRequestTrans( pSSDB->hSSDBError   , pSSDB->hSSDBConnection, 
                                                 SSDB_REQTYPE_REPLACE, szTable, szRequest );
   } else {
     pSSDB->hSSDBRequest = ssdbSendRequest     ( pSSDB->hSSDBError   , pSSDB->hSSDBConnection, 
                                                 SSDB_REQTYPE_REPLACE, szRequest );
   }
   if ( pSSDB->hSSDBRequest == NULL )                                              
        return -1;
   return  0;
}
/* ------------------------------ ---------------------------------------------*/
/* -- SendSSDBDelete                                                           */
/* ------------------------------ ---------------------------------------------*/
int SendSSDBDeleteRequest ( SSDBContext *pSSDB, const char *szTable, char *szRequest)
{
   /* Close Prev Request if Any */
   if ( pSSDB->hSSDBRequest != NULL ) 
   {
     if ( ssdbIsRequestValid ( pSSDB->hSSDBError, pSSDB->hSSDBRequest ))
          ssdbFreeRequest    ( pSSDB->hSSDBError, pSSDB->hSSDBRequest);     
     pSSDB->hSSDBRequest = NULL;
   }   
  
   /* Send Transaction */
   if ( szTable ) 
   {
     pSSDB->hSSDBRequest = ssdbSendRequestTrans( pSSDB->hSSDBError  , pSSDB->hSSDBConnection, 
                                                 SSDB_REQTYPE_DELETE, szTable, szRequest );
   } else {
     pSSDB->hSSDBRequest = ssdbSendRequest     ( pSSDB->hSSDBError  , pSSDB->hSSDBConnection, 
                                                 SSDB_REQTYPE_DELETE, szRequest );
   }
   
   if ( ssdbGetLastErrorCode ( pSSDB->hSSDBError ) != SSDBERR_SUCCESS )
   {
      sscError( pSSDB->hError, "The SSDB error occured: %s", ssdbGetLastErrorString ( pSSDB->hSSDBError ));     
      return -1;
   }
   
   return 0;
}

/* ------------------------------ ---------------------------------------------*/
/* -- SendSSDBLockTables                                                       */
/* ------------------------------ ---------------------------------------------*/
int SendSSDBLockTables ( SSDBContext *pSSDB, const char *szLockReq )
{
   if ( pSSDB->hSSDBRequest != NULL ) 
   {
     if ( ssdbIsRequestValid ( pSSDB->hSSDBError, pSSDB->hSSDBRequest ))
          ssdbFreeRequest    ( pSSDB->hSSDBError, pSSDB->hSSDBRequest);     
     pSSDB->hSSDBRequest = NULL;
   }   
   if ( szLockReq ) 
   {
     if ( !ssdbLockTable( pSSDB->hSSDBError, pSSDB->hSSDBConnection, (char *)szLockReq )) 
     {
        sscError ( pSSDB->hError, "Lock request %s failed", szLockReq );
        return 0;
     }
   }
   return 1;
}
/* ------------------------------ ---------------------------------------------*/
/* -- GetSSDBNextField                                                         */
/* ------------------------------ ---------------------------------------------*/
int GetSSDBNextField ( SSDBContext *pSSDB, void *field, int sizeof_field )
{
   if ( pSSDB->hSSDBRequest == NULL ) 
        return 0;

   if ( ssdbGetNextField ( pSSDB->hSSDBError, 
                           pSSDB->hSSDBRequest, field, sizeof_field ) == 0 )
   {
     sscError( pSSDB->hError, (char*)szErrMsgGetNextFieldFailed  );
     return 0; 
   }     
   return 1;
}
/* ------------------------------ ---------------------------------------------*/
/* -- GetSSDBSeekRow                                                           */
/* ------------------------------ ---------------------------------------------*/
int SSDBSeekRow ( SSDBContext *pSSDB, int Row )
{
   if ( !ssdbRequestSeek ( pSSDB->hError, pSSDB->hSSDBRequest, Row, 0 ))
   {
      sscError ( pSSDB->hError, (char*)szErrMsgSSDBSeekRowFailed, Row );
      return 0;
   }
   return 1;
}
/* ------------------------------ ---------------------------------------------*/
/* -- GetSSDBGetRow                                                            */
/* ------------------------------ ---------------------------------------------*/
const char **SSDBGetRow ( SSDBContext *pSSDB  )
{
   const char ** v = ssdbGetRow ( pSSDB->hError, pSSDB->hSSDBRequest );
   if ( v == NULL ) 
   {
      sscError ( pSSDB->hError, (char*)szErrMsgSSDBGetRowFailed );
   }   
   return v;
}

/* ------------------------------ ---------------------------------------------*/
/* -- GetSSDBGetRow                                                            */
/* ------------------------------ ---------------------------------------------*/
void  PrintLastSSDBError ( SSDBContext *pSSDB, const char * szReq )
{
  if ( pSSDB->hSSDBError != NULL && 
       pSSDB->hError     != NULL  )
  {
    if ( ssdbGetLastErrorCode ( pSSDB->hSSDBError ) != SSDBERR_SUCCESS )
    {
      sscError( pSSDB->hError, "The SSDB error: %s", ssdbGetLastErrorString ( pSSDB->hSSDBError ));     
      if ( szReq != NULL )
           sscError ( pSSDB->hError, "Last submitted request: %s", szReq );
    }
  }  
}

/* ------------------------------ ---------------------------------------------*/
/* -- ResolveIntValue                                                          */
/* ------------------------------ ---------------------------------------------*/

int IsStrBlank ( const char * str )
{
  if ( str == NULL || str[0] == 0 || (strspn ( str," ") == strlen (str))) 
       return 1;
  else 
       return 0;     
}

int ResolveIntValue ( CMDPAIR *Vars, int Varc, const char * pID, int *pVal )
{
    int  id, idx, cnt, lastfound;

    if ( pID == NULL )
         return VALTYPE_UNKNOWN;

    if ( IsStrBlank ( pID ))
         return VALTYPE_VARBLANK;
        
    if ( strcasecmp ( pID, "all" ) == 0 )
         return VALTYPE_ALL    ;
         
    if ( strcasecmp ( pID, "null") == 0 )
         return VALTYPE_NULL   ;
     
    if ( sscanf ( pID, "%d", &id ) == 1 )
    {
      if  ( pVal != NULL ) *pVal = id;
      return VALTYPE_SINGINTVALUE; 
    }    
    
    idx = cnt = 0;
    while ( idx < Varc )
    {
      idx = sscFindPairByKey ( Vars, idx, pID );
      if (  idx == -1 ) 
            break; 
            
      cnt++;
      lastfound = idx;
      idx++;
    } 

    if ( cnt == 1 )
    {    
      /* Let's try to convert to the int */
      if ( !IsStrBlank ( Vars[lastfound].value ))  
      {
        if ( sscanf ( Vars[lastfound].value, "%d", &id ) == 1 )
        {
          if  ( pVal != NULL ) 
               *pVal = id;
          return VALTYPE_SINGINTVARNAME;     
        } else {
          return VALTYPE_BADINTFORMAT;
        }
      } 
      else 
      {
          return VALTYPE_VALBLANK;
      }
    }     
    if ( cnt == 0 )
    {   
          return VALTYPE_VARNOTFOUND;
    }     
    if ( cnt >  1 )
    {
         if ( pVal ) *pVal = cnt;
         return VALTYPE_MULTINTVARNAME;
    }     
    
    return VALTYPE_UNKNOWN;    
}


/* ------------------------------ ---------------------------------------------*/
/* -- ResolveStrValue                                                          */
/* ------------------------------ ---------------------------------------------*/

int ResolveStrValue ( CMDPAIR *Vars, int Varc, const char * pID, const char **ppValue )
{
    int   id, idx, cnt ;
    const char * pName ;
    const char * pValue;  
    
    if ( pID == NULL )
         return VALTYPE_UNKNOWN;

    /* ID must be the name of the variable or double quated "string" */        
    pName = pID + strspn (pID, " "); /* skip white spaces */
    if ( pName[0] == '"' )
    {
      if (  ppValue )
           *ppValue = pName;
      return VALTYPE_SINGSTRVALUE;
    } else {
      if ( IsStrBlank ( pID ))
           return VALTYPE_VARBLANK;
    }
    
    if ( strcasecmp ( pName, "all" ) == 0 )
         return VALTYPE_ALL ;
         
    if ( strcasecmp ( pName, "null") == 0 )
         return VALTYPE_NULL;

    /* let's consider this stuff as variable name */    
    idx = cnt = 0;
    while ( idx < Varc )
    {
      idx = sscFindPairByKey ( Vars, idx, pName );
      if (  idx == -1 ) 
            break; /* not found  */   
         
      pValue = Vars[idx].value;
           
      cnt++;
      idx++;
    } 

    if ( cnt == 1 )
    {
         if ( ppValue ) 
             *ppValue = pValue;
         return VALTYPE_SINGSTRVARNAME; 
    }     
    if ( cnt == 0 )
    {   
         return VALTYPE_VARNOTFOUND;
    }     
    if ( cnt >  1 )
    {
         return VALTYPE_MULTSTRVARNAME;
    }     
    
    return VALTYPE_UNKNOWN;    
}


#define  SQLR_NOERROR           0
#define  SQLR_STACK_UNDERFLOW   1
#define  SQLR_STACK_OVERFLOW    2 
#define  SQLR_BUFFER_OVERFLOW   3
#define  SQLR_EQ_BADVARTYPE     4
#define  SQLR_EQ_VARNOTFOUND    5


void SQLInitRequest ( SQLRequest *pRQ, char *pBuf, int nBuf, CMDPAIR *Vars, int Varc )  
{
    pRQ->Vars       = Vars;
    pRQ->Varc       = Varc;
    pRQ->pBuf       = pBuf;
    pRQ->nBuf       = nBuf;
    pRQ->nBufUsed   =    0;
    pRQ->nErrorCode = SQLR_NOERROR;
    pRQ->ErrVarType =    0;
    pRQ->ErrVarName = NULL; 
    pRQ->ContextIdx =   -1;
}


void  SQLClear ( SQLRequest *pRQ  )
{
   if ( pRQ->nErrorCode != SQLR_NOERROR ) 
        return;
        
   if ( pRQ->ContextIdx < 0)
   {
     pRQ->nErrorCode = SQLR_STACK_UNDERFLOW;
     return;
   }
   pRQ->Context[pRQ->ContextIdx].bFirst = 1;  
}

void  SQLResetFirst ( SQLRequest *pRQ  )
{
   if ( pRQ->nErrorCode != SQLR_NOERROR ) 
        return;
        
   if ( pRQ->ContextIdx < 0)
   {
     pRQ->nErrorCode = SQLR_STACK_UNDERFLOW;
     return;
   }
   pRQ->Context[pRQ->ContextIdx].bFirst = 0;  
}

void  SQLPop   ( SQLRequest *pRQ  )
{
   if ( pRQ->nErrorCode != SQLR_NOERROR ) 
        return;
        
   if ( pRQ->ContextIdx < 0 )
   {
     pRQ->nErrorCode = SQLR_STACK_UNDERFLOW;
     return;
   }
   pRQ->ContextIdx--;
}

void  SQLPush ( SQLRequest *pRQ  )
{
   if ( pRQ->nErrorCode != SQLR_NOERROR ) 
        return;
        
   pRQ->ContextIdx++;
   if ( pRQ->ContextIdx >= SQL_MAX_CONTEXT_STACK )
   {
     pRQ->nErrorCode = SQLR_STACK_OVERFLOW;
     return;
   }
   SQLClear ( pRQ );
}  

void SQLString ( SQLRequest *pRQ, const char * str )
{
   int res;
   
   if ( pRQ->nErrorCode != SQLR_NOERROR ) 
        return;
   res = snprintf  ( pRQ->pBuf + pRQ->nBufUsed, 
                     pRQ->nBuf - pRQ->nBufUsed, "%s", str );
   pRQ->nBufUsed += res;
   if ( pRQ->nBufUsed >= pRQ->nBuf-1 )  
        pRQ->nErrorCode = SQLR_BUFFER_OVERFLOW;
}

void SQLSelect ( SQLRequest *pRQ, const char *szPar )
{
  SQLPush   ( pRQ );
  SQLString ( pRQ, " select " );
  SQLString ( pRQ, szPar );
}

/* The name must be "" if absent */
void SQLField  ( SQLRequest *pRQ, const char *szName )
{
  if (  pRQ->nErrorCode != SQLR_NOERROR ) 
        return;
  
  if ( !pRQ->Context[pRQ->ContextIdx].bFirst )
  {
    SQLString ( pRQ, ", " );  
  }
  if ( szName[0] != 0) 
  {
    SQLString ( pRQ, szName);  
    pRQ->Context[pRQ->ContextIdx].bFirst = 0;
  }   
}

void SQLTableField  ( SQLRequest *pRQ, const char *szTable, const char *szName )
{
  if (  pRQ->nErrorCode != SQLR_NOERROR ) 
        return;
        
  if ( !pRQ->Context[pRQ->ContextIdx].bFirst )
  {
    SQLString ( pRQ, ", ");  
  }
  if ( szName[0]  != 0) 
  {
    if ( szTable[0] != 0)
    {
      SQLString ( pRQ, szTable );  
      SQLString ( pRQ, "." );  
    }
    SQLString ( pRQ, szName );  
    pRQ->Context[pRQ->ContextIdx].bFirst = 0;
  }   
}

void SQLFrom  ( SQLRequest *pRQ, const char *szTable )
{
  SQLClear      ( pRQ );
  SQLString     ( pRQ, " from " );
  SQLString     ( pRQ, szTable  );
  SQLResetFirst ( pRQ );
}

void SQLIn    ( SQLRequest *pRQ, const char * szTable, const char * szName  )
{
  if (  pRQ->nErrorCode != SQLR_NOERROR ) 
        return;

  SQLPush       ( pRQ );
  SQLTableField ( pRQ, szTable, szName );
  SQLClear      ( pRQ );
  SQLString     ( pRQ, " in (" );
}

void SQLEndIn ( SQLRequest *pRQ )
{
  if (  pRQ->nErrorCode != SQLR_NOERROR ) 
        return;
  SQLString   ( pRQ, " )" );
  SQLPop      ( pRQ );
}

void SQLTable ( SQLRequest *pRQ, const char *szTable )
{
  SQLField    ( pRQ, szTable );
}

void SQLWhere ( SQLRequest *pRQ )
{
  SQLClear    ( pRQ );
  SQLString   ( pRQ, " where " );
}

void SQL_And  ( SQLRequest *pRQ )  
{
   if ( pRQ->nErrorCode != SQLR_NOERROR ) 
        return;
        
   if ( pRQ->Context[pRQ->ContextIdx].bFirst == 0 )
   {
      SQLString ( pRQ, " and " );  
   }    
}

void SQL_Or ( SQLRequest *pRQ )  
{
   if ( pRQ->nErrorCode != SQLR_NOERROR ) 
        return;
   if ( !pRQ->Context[pRQ->ContextIdx].bFirst )
   {
        SQLString ( pRQ, " or " );  
   }    
}

void SQL_Eq   ( SQLRequest *pRQ, const char * szTable, const char *szName, const char *szValue, int ValType )  
{
   int  validx, cnt;

   if ( pRQ->nErrorCode != SQLR_NOERROR ) 
        return;

   if ( ValType == VALTYPE_NULL || 
        ValType == VALTYPE_ALL   )
        return;

   SQL_And ( pRQ );

   if ( ValType == VALTYPE_SINGINTVALUE   || 
        ValType == VALTYPE_SINGSTRVALUE   ||       
        ValType == VALTYPE_SINGINTVARNAME ||
        ValType == VALTYPE_SINGSTRVARNAME  )  
   { 
     if ( szTable[0] != 0 )
     {
       SQLString  ( pRQ, szTable );
       SQLString  ( pRQ, "." );
     }
     SQLString    ( pRQ, szName  );
     SQLString    ( pRQ, " = "   );
     
     switch ( ValType )
     {
        case VALTYPE_SINGINTVALUE :
          SQLString    ( pRQ, szValue );
        break;
        
        case VALTYPE_SINGSTRVALUE :
          SQLString    ( pRQ, "'"     );
          SQLString    ( pRQ, szValue );
          SQLString    ( pRQ, "'"     );
        break;
        
        case VALTYPE_SINGINTVARNAME :
          validx = sscFindPairByKey ( pRQ->Vars, 0, szValue ); 
          if ( validx < 0 )
           {
              pRQ->nErrorCode = SQLR_EQ_VARNOTFOUND; 
              pRQ->ErrVarName = szValue; 
              return;
           }
          SQLString  ( pRQ, pRQ->Vars[validx].value );
        break;
        
        case VALTYPE_SINGSTRVARNAME:
          validx = sscFindPairByKey ( pRQ->Vars, 0, szValue ); 
          if ( validx < 0 )
           {
              pRQ->ErrVarName = szValue; 
              pRQ->nErrorCode = SQLR_EQ_VARNOTFOUND;
              return ;
           }
          SQLString    ( pRQ, "'"     );
          SQLString    ( pRQ, pRQ->Vars[validx].value );
          SQLString    ( pRQ, "'"     );
        break;
        
    } /* End of case */   
    
     SQLResetFirst( pRQ );
     return;
   }  

   if ( ValType ==  VALTYPE_MULTINTVARNAME  ||
        ValType ==  VALTYPE_MULTSTRVARNAME  )
   { /* multivalue case */
     if ( szTable[0] != 0 )
     {
       SQLString  ( pRQ, szTable );
       SQLString  ( pRQ, "." );
     }
     SQLString  ( pRQ, szName   );
     SQLString  ( pRQ, " in ( " );

     cnt = validx = 0;
     while ( validx < pRQ->Varc )
     {
       validx = sscFindPairByKey ( pRQ->Vars, validx, szName);
       if (  validx == -1 ) 
             break; 
      
        if ( cnt != 0 )   
             SQLString (pRQ, ",");
      
        if ( ValType == VALTYPE_MULTINTVARNAME )
        {
          SQLString  ( pRQ, pRQ->Vars[validx].value );
        } else {
          SQLString    ( pRQ, "'"     );
          SQLString    ( pRQ, pRQ->Vars[validx].value );
          SQLString    ( pRQ, "'"     );
        }
        validx++;
        cnt++;
     } 
   
     SQLString ( pRQ, " ) "); /* Close in statement */
     SQLResetFirst ( pRQ );     
     return;
  }  
  
  pRQ->nErrorCode = SQLR_EQ_BADVARTYPE; /* Unknown type */
  pRQ->ErrVarType = ValType; 

}

void SQL_EqKey ( SQLRequest *pRQ, const char * szTable1, const char * szTable2, const char *szKey ) 
{
   int res;

   if ( pRQ->nErrorCode != SQLR_NOERROR ) 
        return;

   SQL_And ( pRQ );
      
   res = snprintf  ( pRQ->pBuf + pRQ->nBufUsed, 
                     pRQ->nBuf - pRQ->nBufUsed, 
                     "%s.%s = %s.%s", szTable1, szKey, szTable2, szKey );
   pRQ->nBufUsed += res;
   if ( pRQ->nBufUsed >= pRQ->nBuf-1 )  
        pRQ->nErrorCode = SQLR_BUFFER_OVERFLOW;
   
   SQLResetFirst ( pRQ );
}

void SQL_EqIntConst ( SQLRequest *pRQ, const char * szTable, const char * szField, const char *szValue ) 
{
   SQL_And ( pRQ );

   if ( szTable[0] != 0 ) 
   {
      SQLString ( pRQ, szTable );
      SQLString ( pRQ, "." );
   }  
   SQLString ( pRQ, szField);
   SQLString ( pRQ, " = "  );
   SQLString ( pRQ, szValue);
   SQLResetFirst ( pRQ );
}  

void SQL_EqStrConst ( SQLRequest *pRQ, const char * szTable, const char * szField, const char *szValue ) 
{
   SQL_And ( pRQ );

   if ( szTable[0] != 0 ) 
   {
      SQLString ( pRQ, szTable );
      SQLString ( pRQ, "." );
   }  
   SQLString ( pRQ, szField);
   SQLString ( pRQ, " = '"  );
   SQLString ( pRQ, szValue);
   SQLString ( pRQ, "'"  );
   SQLResetFirst ( pRQ );
}  


int ReportSQLReqError ( sscErrorHandle hError, SQLRequest *pRQ  )
{
   if ( pRQ == NULL ) 
        return -1;

   if ( pRQ->nErrorCode == SQLR_NOERROR )
        return pRQ->nBufUsed;
        
   switch ( pRQ->nErrorCode  )     
   {
      case SQLR_STACK_UNDERFLOW:  
          sscError ( hError, "SQL Request: context stack underflow" );
      break;
      
      case SQLR_STACK_OVERFLOW :
          sscError ( hError, "SQL Request: context stack overflow" );
      break;
      
      case SQLR_BUFFER_OVERFLOW:
          sscError ( hError, "SQL Request: buffer overflow" );
      break;
      
      case SQLR_EQ_BADVARTYPE  :
          sscError ( hError, "SQL Request: bad variable type %d", pRQ->ErrVarType );
      break;
      
      case SQLR_EQ_VARNOTFOUND :
          sscError ( hError, "SQL Request: unable to find variable", pRQ->ErrVarName );
      break;

      default:
          sscError ( hError, "SQL Request: Unrecognized error #%d occured", pRQ->nErrorCode );
   }
   return -1;
}


/* ------------------------------ ---------------------------------------------*/
/* -- Database Access Helpers                                                  */
/* ------------------------------ ---------------------------------------------*/

int IsValidIntType (int V )
{
   if ( V == VALTYPE_SINGINTVALUE   || 
        V == VALTYPE_SINGINTVARNAME ||
        V == VALTYPE_MULTINTVARNAME ||
        V == VALTYPE_NULL ||
        V == VALTYPE_ALL  )
   {
      return 1;
   }
   return 0;
}

int FormatEventEnumSQLReq ( sscErrorHandle hError, 
                            char *pBuf, int nBuf ,
                            const char *sys_id   , 
                            const char *class_id , 
                            const char *event_id ,
                            const char *action_id,
                            const char *custom   ,
                            CMDPAIR    *Vars     , 
                            int         Varc     )
{
  int   res;
  int   SysIDType;
  int   ClassIDType;
  int   EventIDType;
  int   ActionIDType;
  SQLRequest   SQLRq = {0};
  
  memset ( pBuf, 0, nBuf );

  SysIDType = VALTYPE_SINGSTRVALUE;
  
  ClassIDType  = ResolveIntValue ( Vars, Varc, class_id, NULL );
  if ( !IsValidIntType (ClassIDType)) 
        return  0; 
  
  EventIDType  = ResolveIntValue ( Vars, Varc, event_id, NULL );
  if ( !IsValidIntType (EventIDType)) 
        return  0; 
       
  ActionIDType = ResolveIntValue ( Vars, Varc, action_id, NULL );
  if ( !IsValidIntType (ActionIDType)) 
        return  0; 
     
  SQLInitRequest ( &SQLRq, pBuf, nBuf, Vars, Varc  );
  SQLSelect      ( &SQLRq, "" );

  if ( EventIDType == VALTYPE_NULL )
       SQLString ( &SQLRq, " distinct " );
       
  SQLTableField  ( &SQLRq, szClassTbl    , szClassIDFldName   ); 
  SQLTableField  ( &SQLRq, szClassTbl    , szClassDescFldName ); 
  SQLTableField  ( &SQLRq, szEventTypeTbl, szEventIDFldName   ); 
  SQLTableField  ( &SQLRq, szEventTypeTbl, szEventDescrFldName); 
  SQLTableField  ( &SQLRq, szEventTypeTbl, szThrottledFldName );
  SQLTableField  ( &SQLRq, szEventTypeTbl, szSehThrotFldName  ); 
  SQLTableField  ( &SQLRq, szEventTypeTbl, szSehFreqFldName   ); 
  SQLTableField  ( &SQLRq, szEventTypeTbl, szPriorityFldName  ); 
  SQLTableField  ( &SQLRq, szEventTypeTbl, szEnabledFldName   ); 
  
  SQLFrom        ( &SQLRq, szClassTbl );
  SQLTable       ( &SQLRq, szEventTypeTbl );
  if (  ActionIDType != VALTYPE_NULL )
        SQLTable ( &SQLRq, szActionRefTbl );
      
  SQLWhere       ( &SQLRq );

  SQL_Eq         ( &SQLRq, szClassTbl    , szClassIDFldName , class_id , ClassIDType  );  
  SQL_Eq         ( &SQLRq, szClassTbl    , szSysIDFldName   , sys_id   , SysIDType    );  

  SQL_Eq         ( &SQLRq, szEventTypeTbl, szEventIDFldName , event_id , EventIDType  );  
  SQL_Eq         ( &SQLRq, szEventTypeTbl, szSysIDFldName   , sys_id   , SysIDType    );  
  
  SQL_EqKey      ( &SQLRq, szClassTbl    , szEventTypeTbl   , szClassIDFldName ); 
  SQL_EqKey      ( &SQLRq, szClassTbl    , szEventTypeTbl   , szSysIDFldName   ); 
  
  if ( ActionIDType != VALTYPE_NULL )
  {
   SQL_Eq        ( &SQLRq, szActionRefTbl, szActionIDFldName, action_id, ActionIDType );  
   SQL_Eq        ( &SQLRq, szActionRefTbl, szSysIDFldName   , sys_id   , SysIDType    );  
   SQL_EqKey     ( &SQLRq, szActionRefTbl, szClassTbl       , szClassIDFldName ); 
   SQL_EqKey     ( &SQLRq, szActionRefTbl, szEventTypeTbl   , szEventIDFldName ); 
  }

  /* custom add */  
  SQLString  ( &SQLRq, custom );

  return ReportSQLReqError ( hError, &SQLRq );
}

/* ------------------------------ ---------------------------------------------*/


int FormatClassEnumSQLReq ( sscErrorHandle hError, 
                            char *pBuf, int nBuf ,
                            const char *sys_id   , 
                            const char *class_id , 
                            const char *event_id ,
                            const char *action_id,
                            const char *custom   ,
                            CMDPAIR    *Vars     , 
                            int         Varc     )
{
  int   res;
  int   SysIDType;
  int   ClassIDType;
  int   EventIDType;
  int   ActionIDType;
  SQLRequest   SQLRq = {0};
  
  memset ( pBuf, 0, nBuf );

  SysIDType = VALTYPE_SINGSTRVALUE;
  
  ClassIDType  = ResolveIntValue ( Vars, Varc, class_id, NULL );
  if ( !IsValidIntType (ClassIDType)) 
        return  0; 

  EventIDType  = ResolveIntValue ( Vars, Varc, event_id, NULL );
  if ( !IsValidIntType (EventIDType)) 
        return  0; 
       
  ActionIDType = ResolveIntValue ( Vars, Varc, action_id, NULL );
  if ( !IsValidIntType (ActionIDType)) 
        return  0; 
            
  SQLInitRequest ( &SQLRq, pBuf, nBuf, Vars, Varc  );
  SQLSelect      ( &SQLRq, " distinct " );

  SQLTableField  ( &SQLRq, szClassTbl, szClassIDFldName   ); 
  SQLTableField  ( &SQLRq, szClassTbl, szClassDescFldName ); 
  
  SQLFrom        ( &SQLRq, szClassTbl );
  
  if (  EventIDType != VALTYPE_NULL )
        SQLTable ( &SQLRq, szEventTypeTbl );
        
  if (  ActionIDType != VALTYPE_NULL )
        SQLTable ( &SQLRq, szActionRefTbl );
      
  SQLWhere       ( &SQLRq );

  SQL_Eq         ( &SQLRq, szClassTbl    , szClassIDFldName , class_id , ClassIDType  );  
  SQL_Eq         ( &SQLRq, szClassTbl    , szSysIDFldName   , sys_id   , SysIDType    );
  
  if (  EventIDType != VALTYPE_NULL )
  {
   SQL_Eq        ( &SQLRq, szEventTypeTbl, szEventIDFldName , event_id , EventIDType  );  
   SQL_EqKey     ( &SQLRq, szClassTbl    , szEventTypeTbl   , szClassIDFldName        ); 
   SQL_EqKey     ( &SQLRq, szClassTbl    , szEventTypeTbl   , szSysIDFldName          ); 
  } 
  
  if ( ActionIDType != VALTYPE_NULL )
  {
   SQL_Eq        ( &SQLRq, szActionRefTbl, szActionIDFldName, action_id, ActionIDType );  
   SQL_Eq        ( &SQLRq, szActionRefTbl, szSysIDFldName   , sys_id   , SysIDType    );  
   SQL_EqKey     ( &SQLRq, szActionRefTbl, szClassTbl       , szClassIDFldName        ); 
   SQL_EqKey     ( &SQLRq, szActionRefTbl, szEventTypeTbl   , szEventIDFldName        ); 
  }
  
  /* custom add by */  
  SQLString  ( &SQLRq, custom  );

  return   ReportSQLReqError ( hError, &SQLRq );
}


/* ------------------------------ ---------------------------------------------*/

int FormatActionEnumSQLReq (sscErrorHandle hError, 
                            char *pBuf, int nBuf ,
                            const char *sys_id   , 
                            const char *class_id , 
                            const char *event_id ,
                            const char *action_id,
                            const char *custom   ,
                            CMDPAIR    *Vars     , 
                            int         Varc     )
{
  int   res;
  int   SysIDType;
  int   ClassIDType;
  int   EventIDType;
  int   ActionIDType;
  SQLRequest   SQLRq = {0};
  
  memset ( pBuf, 0, nBuf );

  SysIDType = VALTYPE_SINGSTRVALUE;
  
  ClassIDType  = ResolveIntValue ( Vars, Varc, class_id, NULL );
  if ( !IsValidIntType (ClassIDType)) 
        return  0; 
  
  EventIDType  = ResolveIntValue ( Vars, Varc, event_id, NULL );
  if ( !IsValidIntType (EventIDType)) 
        return  0; 
       
  ActionIDType = ResolveIntValue ( Vars, Varc, action_id, NULL );
  if ( !IsValidIntType (ActionIDType)) 
        return  0; 
    
  SQLInitRequest ( &SQLRq, pBuf, nBuf, Vars, Varc  );
  
  SQLSelect      ( &SQLRq, "" );
  if ( EventIDType == VALTYPE_NULL ) 
       SQLString ( &SQLRq, " distinct ");
       
  SQLTableField  ( &SQLRq, szEventActionTbl, szActionIDFldName     );
  SQLTableField  ( &SQLRq, szEventActionTbl, szActionDescFldName   );
  SQLTableField  ( &SQLRq, szEventActionTbl, szActionFldName       ); 
  SQLTableField  ( &SQLRq, szEventActionTbl, szUserStringFldName   );
  SQLTableField  ( &SQLRq, szEventActionTbl, szDsmThrottleFldName  );
  SQLTableField  ( &SQLRq, szEventActionTbl, szDsmFreqFldName      );
  SQLTableField  ( &SQLRq, szEventActionTbl, szTimeoutValueFldName );
  SQLTableField  ( &SQLRq, szEventActionTbl, szRetryFldName        );
  SQLTableField  ( &SQLRq, szEventActionTbl, szFrwdHostNameFldName );
  SQLTableField  ( &SQLRq, szEventActionTbl, szPrivateFldName      );
    
  SQLFrom        ( &SQLRq, szEventActionTbl );
  
  if ( EventIDType != VALTYPE_NULL || 
       ClassIDType != VALTYPE_NULL  ) 
  {     
       SQLTable ( &SQLRq, szActionRefTbl );
  }     
      
  SQLWhere       ( &SQLRq );
  
  SQL_Eq         ( &SQLRq, szEventActionTbl, szActionIDFldName , action_id, ActionIDType );  
  SQL_Eq         ( &SQLRq, szEventActionTbl, szSysIDFldName    , sys_id   , SysIDType    );  
  SQL_EqIntConst ( &SQLRq, szEventActionTbl, szPrivateFldName  , "0" );
  
  if ( EventIDType != VALTYPE_NULL || 
       ClassIDType != VALTYPE_NULL  )
  {
   SQL_Eq        ( &SQLRq, szActionRefTbl, szClassIDFldName , class_id , ClassIDType  );  
   SQL_Eq        ( &SQLRq, szActionRefTbl, szEventIDFldName , event_id , EventIDType  );  
   SQL_Eq        ( &SQLRq, szActionRefTbl, szSysIDFldName   , sys_id   , SysIDType    );  
   SQL_EqKey     ( &SQLRq, szActionRefTbl, szEventActionTbl , szActionIDFldName ); 
  }

  /* custom add by */  
  SQLString  ( &SQLRq, custom );

  return ReportSQLReqError ( hError, &SQLRq );
}

/* ------------------------------ ---------------------------------------------*/
/* -- Database Access Helpers                                                  */
/* ------------------------------ ---------------------------------------------*/

int  get_global_setup_flags ( sscErrorHandle hError, 
                              int           *pLog, 
                              int           *pFilter, 
                              int           *pAction )
{
   SSDBContext  SSDB = {0};
   int          i,   nRows;
   char         szOpt[64 ];
   char         szVal[256];
   char         szReq[512];
   int          res = SSDB_ERR;
   
   szReq[0] = 0;
   
   if ( !OpenSSDBContext ( hError, &SSDB ))
         return res;
         
   *pLog    = -1; /* Not Found */
   *pFilter = -1; /* Not Found */
   *pAction = -1; /* Not Found */
         
   snprintf ( szReq, sizeof(szReq), szSQLGetGlobalFlags , szLoggingFlagName, szFilterFlagName, szActionFlagName );      
   nRows = SendSSDBSelectRequest ( &SSDB, szToolTbl, szReq);
   if (  nRows == 3 )
   {
       for ( i = 0; i < nRows; i++ )
       {
        if ( GetSSDBNextField ( &SSDB, szOpt, sizeof(szOpt)) &&
             GetSSDBNextField ( &SSDB, szVal, sizeof(szVal))  )
         {     
           if ( strcasecmp ( szOpt, szLoggingFlagName ) == 0 )   
                sscanf ( szVal, "%d", pLog    );
           else if ( strcasecmp ( szOpt, szFilterFlagName ) == 0 )
                sscanf ( szVal, "%d", pFilter );
           else if ( strcasecmp ( szOpt, szActionFlagName ) == 0 )
                sscanf ( szVal, "%d", pAction );
         } else {
           goto Exit;
         }  
       }
       res = nRows;
   } 

Exit:

   if ( res == SSDB_ERR )
        PrintLastSSDBError ( &SSDB, szReq );
        
   CleanSSDBContext   ( &SSDB );    
 
   return res;
}

/* ------------------------------ --------------------------------------------- */
int set_global_setup_flags ( sscErrorHandle hError, 
                             int Logging, 
                             int Filter , 
                             int Action )
{
   SSDBContext  SSDB = {0};
   char         szReq[512];
   int          res  = SSDB_ERR;

   szReq[0] = 0;

   if ( !OpenSSDBContext ( hError, &SSDB )) return res;
 
   snprintf ( szReq, sizeof(szReq), szSQLSetGlobalFlag,  Logging, szLoggingFlagName );
   if ( -1 == SendSSDBUpdateRequest ( &SSDB, szToolTbl,  szReq )) goto Exit;

   snprintf ( szReq, sizeof(szReq), szSQLSetGlobalFlag,  Filter,  szFilterFlagName );
   if ( -1 == SendSSDBUpdateRequest ( &SSDB, szToolTbl,  szReq )) goto Exit;

   snprintf ( szReq, sizeof(szReq), szSQLSetGlobalFlag,  Action,  szActionFlagName );
   if ( -1 == SendSSDBUpdateRequest ( &SSDB, szToolTbl,  szReq )) goto Exit;
     
   res = 1;

 Exit:   

   if ( res == SSDB_ERR )
        PrintLastSSDBError ( &SSDB, szReq );

   CleanSSDBContext   ( &SSDB );    
 
   return res;
}
/* -------------------------------------------------------------------------- */
int  get_sys_id ( sscErrorHandle hError, char *sysid, int sysidlen )
{
   SSDBContext  SSDB = {0};
   int          cnt  =   0;
   int          res  = SSDB_ERR;
   char         szReq[512];

   szReq[0] = 0;

   if ( !OpenSSDBContext ( hError, &SSDB )) return res;
   
   snprintf ( szReq, sizeof(szReq), szSQLGetSysIDList );
   cnt = SendSSDBSelectRequest ( &SSDB, szSystemTbl, szReq );
   if ( cnt >= 0 ) 
   {
     if ( GetSSDBNextField ( &SSDB, sysid, sysidlen )) 
          res = cnt;         
   }

   if ( res == SSDB_ERR )
        PrintLastSSDBError ( &SSDB, szReq );
   
   CleanSSDBContext   ( &SSDB );    
 
   return res;
}

/* -------------------------------------------------------------------------- */

int ProcessSelectRequest (sscErrorHandle hError,
                          const char  *szReq, 
                          CMDPAIR     *Vars ,                 
                          int          Varc , 
                          EnumProc     pProc, 
                          void        *pProcData )
{
   int          i,  j, cnt;
   SSDBContext  SSDB = {0};
   int          res  = SSDB_ERR;
   char         szIdx [16];
   char         szPage[16];
   CMDPAIR      RecVars[64];
   
   if ( !OpenSSDBContext ( hError, &SSDB )) return res;
   
   cnt = SendSSDBSelectRequest ( &SSDB, NULL, (char *) szReq );
   if ( cnt >= 0 )
    {
       if ( pProc && cnt > 0 )
       {
          const char **Vector;
          int   VLen = Varc-2;
          
          memcpy ( RecVars, Vars, Varc * sizeof(CMDPAIR));
          
          RecVars[0].value = szIdx ;
          RecVars[1].value = szPage;
          
         /* Init Enumeration */       
         if ((*pProc)( pProcData, ENUMIDX_INIT, NULL, 0 ) == 0 ) 
         { /* No more Enumeration */
           res = ENUM_ERR; 
           goto  Exit;  
         } 
         
         if ( !SSDBSeekRow ( &SSDB, 0 ))
               goto Exit;  
               
         for ( i = 0; i < cnt; i++ ) 
         {
           sprintf ( szIdx, "%d", i );
         
           Vector = SSDBGetRow ( &SSDB );
           if ( Vector == NULL )
                goto Exit;

           for ( j = 0; j < VLen; j++ ) 
           {
             RecVars[j+2].value = (char *) Vector[j];
           }

           /* Call Enum Proc */              
           if ( (*pProc)( pProcData, i, RecVars, VLen+2 ) == 0 )    
           { /* No more Enumeration */
             res = ENUM_ERR;
             goto  Exit; 
           }      
         } /* End of for */
         
         /* Finish Enumeration*/       
         if ( (*pProc)( pProcData, ENUMIDX_DONE, NULL, 0 ) == 0 ) 
         { /* No more Enumeration */
           res = ENUM_ERR;
           goto  Exit;  
         }      
       } /* End of pProc */
       res = cnt;
   } /* End of pProc */
 
Exit:   

   if ( res == SSDB_ERR )
        PrintLastSSDBError ( &SSDB, szReq );

   CleanSSDBContext   ( &SSDB );    
   return res;
}

/* ------------------------------ --------------------------------------------- */

int enum_events ( sscErrorHandle hError    ,
                  const char *   szSysID   , 
                  const char *   szClassID , 
                  const char *   szEventID ,
                  const char *   szActionID,
                  const char *   szCustom  ,
                  CMDPAIR    *   Vars      , 
                  int            Varc      ,  
                  EnumProc       pProc     , 
                  void         * pProcData )
{
   int   res;
   char  szReq[4096] = "";

   res = FormatEventEnumSQLReq ( hError,  szReq  , sizeof (szReq), 
                                 szSysID, szClassID, szEventID, szActionID, szCustom, 
                                 Vars, Varc );
   if ( res == SQLR_ERR ) 
        return SQLR_ERR; /* This error already being reported */
   
   return ProcessSelectRequest ( hError, (const char*) szReq, 
                                 EventRecSymTable, EVENT_REC_SYM_NUM, 
                                 pProc, pProcData );
}

/* ------------------------------ --------------------------------------------- */

int enum_classes( sscErrorHandle hError    ,
                  const char *   szSysID   , 
                  const char *   szClassID , 
                  const char *   szEventID , 
                  const char *   szActionID,
                  const char *   szCustom  ,
                  CMDPAIR    *   Vars      , 
                  int            Varc      ,  
                  EnumProc       pProc     , 
                  void         * pProcData )
{
   int   res;
   char  szReq[4096] = "";

   res = FormatClassEnumSQLReq ( hError, szReq, sizeof (szReq), 
                                 szSysID, szClassID, szEventID, szActionID, szCustom, 
                                 Vars, Varc );
   if ( res == SQLR_ERR ) 
        return SQLR_ERR;
        
   return ProcessSelectRequest ( hError, (const char*) szReq, 
                                 ClassRecSymTable, CLASS_REC_SYM_NUM, 
                                 pProc, pProcData );
}


/* ------------------------------ --------------------------------------------- */

int enum_actions( sscErrorHandle hError    ,
                  const char *   szSysID   , 
                  const char *   szClassID , 
                  const char *   szEventID ,
                  const char *   szActionID,
                  const char *   szCustom  ,
                  CMDPAIR    *   Vars      , 
                  int            Varc      ,  
                  EnumProc       pProc     , 
                  void         * pProcData )
{
   int   res;
   char  szReq[4096] = "";

   res = FormatActionEnumSQLReq ( hError,  szReq, sizeof (szReq), 
                                  szSysID, szClassID, szEventID, szActionID, szCustom, 
                                  Vars, Varc );
   if ( res == SQLR_ERR ) 
        return res;
        
   return ProcessSelectRequest ( hError, (const char*) szReq, 
                                 ActionRecSymTable, ACTION_REC_SYM_NUM, 
                                 pProc, pProcData );
}

int verify_action_uid(const char *actuname)
{ struct passwd *ptr;
  uid_t act_uid = 0;
  setpwent();
  while ((ptr = getpwent()) != NULL)
  { if (strcasecmp(actuname, ptr->pw_name) == 0)
    { if (act_uid == ptr->pw_uid)
      { endpwent(); return 0;}
      endpwent();
      return 1;
    }
  }
  endpwent();
  return 2;
}


/* ------------------------------ --------------------------------------------- */
int update_event  ( sscErrorHandle  hError    , 
                    const char    * sys_id    ,
                    const char    * class_id  ,
                    const char    * class_dscr,
                    const char    * event_type, 
                    const char    * event_dscr,
                    const char    * thcount   ,
                    const char    * thtimeout ,
                    const char    * enabled   ,
                    int           * newclassid, 
                    int           * neweventid,
                    CMDPAIR       * Vars      , 
                    int             Varc      ,
                    int             op        )
{
   int          cnt;
   int          classvar;
   int          eventvar;
   int          thcountvar;
   int          thcountval;
   int          timeoutvar;
   int          timeoutval;
   int          enabledvar;
   int          enabledval;
   int          classdescvar;     
   int          eventdescvar;
   const char  *classdescval = NULL;
   const char  *eventdescval = NULL;
   SSDBContext  SSDB = {0};
   char         szReq[512];
   int          res  = SSDB_ERR;
   int          vartype  ;

   szReq[0] = 0;

   /*Input parameter validation */
   /* sys_id handled by caller  */
   /* let's assume that sysid is correct */
   
   eventvar     = ResolveIntValue ( Vars, Varc, event_type,  neweventid   );
   classvar     = ResolveIntValue ( Vars, Varc, class_id,    newclassid   );
   
   classdescvar = ResolveStrValue ( Vars, Varc, class_dscr, &classdescval );
   eventdescvar = ResolveStrValue ( Vars, Varc, event_dscr, &eventdescval );
   thcountvar   = ResolveIntValue ( Vars, Varc, thcount   , &thcountval   );
   timeoutvar   = ResolveIntValue ( Vars, Varc, thtimeout , &timeoutval   );
   enabledvar   = ResolveIntValue ( Vars, Varc, enabled   , &enabledval   );

   if ( op == OP_INSERT_EVENT || op == OP_VALIDATE_INSERT )
   {
     if ( class_dscr == NULL ) 
     {
       /* we are inserting into old class so class_id must be singular int or varname */
       if (!(classvar == VALTYPE_SINGINTVALUE || classvar == VALTYPE_SINGINTVARNAME))
       {
         sscError ( hError, "Incorrect class_id parameter type in update_event call" );
         sscError ( hError, "class_id type = %d", classvar );
         return TYPE_ERR;
       }     
     } else {
      /* we want to create the new class so class description must be non blank singular string */
      
      if (!(classdescvar == VALTYPE_SINGSTRVALUE || classdescvar == VALTYPE_SINGSTRVARNAME))
      {
           sscError ( hError, "Incorrect class_desc parameter in update_event call" );      
           sscError ( hError, "class_desc type = %d", classdescvar );
           return TYPE_ERR;
      }
      if ( IsStrBlank ( classdescval  ))
      {
           return CLASSDSCR_EMPTY;
      }     
     }

     /* validate event description */     
     /* we must have singular not blank string */
     if (!(eventdescvar == VALTYPE_SINGSTRVALUE || eventdescvar == VALTYPE_SINGSTRVARNAME ))
     {
        sscError ( hError, "Incorrect event_desc parameter type in update_event call" );
        sscError ( hError, "event_desc type = %d", eventdescvar );
        return TYPE_ERR;
     }
     if ( IsStrBlank ( eventdescval ))
     {
        return EVENTDSCR_EMPTY;
     }     

     
     /* We must check eventid type only if event is present */
     if (  eventvar != VALTYPE_NULL )
     {
       if (  eventvar == VALTYPE_VALBLANK )
             return  EVENTID_OUTOFRANGE;
           
       if (  eventvar == VALTYPE_BADINTFORMAT )
             return  EVENTID_OUTOFRANGE;     
     
       if (!(eventvar == VALTYPE_SINGINTVALUE || eventvar == VALTYPE_SINGINTVARNAME))
       {
          sscError ( hError, "Incorrect event_type parameter type in update_event call" );
          return TYPE_ERR;
       }
     
       /* Check the range of this variable */
       if ( *neweventid < 8400000 || *neweventid >= 8500000 )
            return EVENTID_OUTOFRANGE;
     }      
   } /* if insert operation */

   
   if ( op == OP_UPDATE_EVENT || op == OP_VALIDATE_UPDATE )
   { 
   
     /* we must have valid event id */
     if ( eventvar == VALTYPE_MULTINTVARNAME )
     {
        sscError ( hError, "Multiple updates is not supported for now" );
        return TYPE_ERR;
     }
     if (!(eventvar == VALTYPE_SINGINTVALUE || eventvar == VALTYPE_SINGINTVARNAME ))
     {
        sscError ( hError, "Incorrect type_id parameter type in update_event call" );
        return TYPE_ERR;
     }     
   }
   
   /* attributes validation */
   if ( thcountvar == VALTYPE_VALBLANK || thcountvar == VALTYPE_BADINTFORMAT) 
   {
       return THCOUNT_OUTOFRANGE;
   }
   if (!(thcountvar == VALTYPE_SINGINTVALUE || thcountvar == VALTYPE_SINGINTVARNAME ))
   {
       sscError ( hError, "Incorrect throttling parameter type in update_event call" );
       sscError ( hError, "thcount type = %d", thcountvar );
       return TYPE_ERR;
   }     
   if ( thcountval < 0 )
   {
       return THCOUNT_OUTOFRANGE;
   }

   if ( timeoutvar == VALTYPE_VALBLANK || timeoutvar == VALTYPE_BADINTFORMAT )
   {
       return THTIMEOUT_OUTOFRANGE;
   }
   if (!(timeoutvar == VALTYPE_SINGINTVALUE || timeoutvar == VALTYPE_SINGINTVARNAME ))
   {
       sscError ( hError, "Incorrect timeout parameter type in update_event call" );
       sscError ( hError, "thtimeout type = %d", timeoutvar );
       return TYPE_ERR;
   }     
   if ( timeoutval < 0 || timeoutval % 5 != 0)
   {
       return THTIMEOUT_OUTOFRANGE;
   }    
   
   if ( enabledvar == VALTYPE_VARNOTFOUND )
   {
     enabledval = 0;   
   } else {     
     if (!(enabledvar == VALTYPE_SINGINTVALUE  || enabledvar == VALTYPE_SINGINTVARNAME ))
      { 
         sscError ( hError, "Incorrect enabled parameter type in update_event call" );
         return TYPE_ERR;
      }    
   }    

   /* Let's goto DB level */

   if ( !OpenSSDBContext ( hError, &SSDB )) 
         goto Exit;
   
   if ( !SendSSDBLockTables ( &SSDB, "event_class, event_type" )) 
         goto Exit;

   if ( op == OP_INSERT_EVENT || op == OP_VALIDATE_INSERT )
   { /* we must check for it's existence */
     /* remember, that event is singular */

     if ( eventvar == VALTYPE_NULL )
     { /* we must generate the new one */
       /**/
         snprintf ( szReq,  sizeof(szReq), 
                   "select type_id from event_type where type_id>=8400000 and type_id<8500000 and sys_id = '%s'",
                    sys_id );
         cnt = SendSSDBSelectRequest ( &SSDB, NULL, szReq  );
         if ( cnt <  0 ) goto Exit;
         if ( cnt == 0 )
         {
           *neweventid = 8400000;
         } else {
           snprintf ( szReq,  sizeof(szReq), 
                     "select max(type_id) from event_type where type_id>=8400000 and type_id<8500000 and sys_id = '%s'",
                      sys_id );
           cnt = SendSSDBSelectRequest ( &SSDB, NULL, szReq  );
           if ( cnt < 0 || cnt > 1) goto Exit; 
           if ( cnt >= 0 )
            {
              const char ** vector;
              vector = SSDBGetRow ( &SSDB  );        
              if ( vector == NULL ) 
                   goto Exit;

              if ( vector[0] && sscanf ( vector[0], "%d", neweventid ) == 1 )
                   *neweventid += 1;
              else 
                   *neweventid  = 8400000;
            }         
         }         
     } else {   
     
     
       /* validate event_type */
       snprintf ( szReq,  sizeof(szReq), 
                 "select type_id from event_type where type_id=%d and sys_id = '%s'",
                  *neweventid, sys_id );
       cnt = SendSSDBSelectRequest ( &SSDB, NULL, szReq  );
       if ( cnt < 0 ) goto Exit; 
       if ( cnt > 0 ) 
       { 
          res = EVENT_ALREADY_EXIST; 
          goto Exit; 
       } /* Event Type Already Exist */
     }  

     snprintf (  szReq, sizeof(szReq), 
                "select type_id from event_type where type_desc='%s' and sys_id ='%s'",   
                 eventdescval, sys_id );
     cnt = SendSSDBSelectRequest ( &SSDB, NULL, szReq  );
     if ( cnt < 0 ) goto Exit; 
     if ( cnt > 0 ) { res  = EVENTDSCR_ALREADY_EXIST; goto Exit; }


     /* validate class_info */
     if ( class_dscr != NULL ) 
     { 
       /* We must validate class description */ 
       snprintf ( szReq, sizeof(szReq), 
                 "select class_id from event_class where class_desc = '%s' and sys_id = '%s'",
                  classdescval, sys_id ); 
       cnt = SendSSDBSelectRequest ( &SSDB, NULL, szReq  );
       if ( cnt < 0 ) goto Exit; 
       if ( cnt > 0 ) 
       { 
         const char ** vector;
         
         vector = SSDBGetRow ( &SSDB  );        
         if ( vector == NULL ) 
               goto Exit;

         res  = CLASSDSCR_ALREADY_EXIST;
         goto Exit; 
       } /* Class Description Already Exist */
       
       
       /* And Create New ClassID */
       snprintf ( szReq, sizeof(szReq), 
                 "select class_id from event_class where class_id > 7999 and sys_id = '%s'", sys_id );
       cnt = SendSSDBSelectRequest ( &SSDB, NULL, szReq  );
       if ( cnt < 0 ) goto Exit;
       if ( cnt == 0 )
       {
        *newclassid = 8000;
       } else {    
         snprintf ( szReq, sizeof(szReq), 
                   "select max(class_id) from event_class where class_id > 7999 and sys_id = '%s'", sys_id );
     
         cnt = SendSSDBSelectRequest ( &SSDB, NULL, szReq  );
         if ( cnt <= 0 || cnt > 1 ) goto Exit; /* DataBase Error */
         if ( cnt == 1 )
         {
           const char ** vector;
           vector = SSDBGetRow ( &SSDB  );        
           if ( vector == NULL ) 
                goto Exit;
              
           if ( vector[0] && sscanf ( vector[0], "%d", newclassid ) == 1 ) 
               *newclassid += 1;
           else 
              *newclassid  = 8000;
         }       
       }   
       
     } else {
       
       /* We must validate existence of ClassID */
       snprintf ( szReq, sizeof(szReq), 
                 "select class_id from event_class where  class_id = %d and sys_id = '%s'",
                 *newclassid, sys_id );
                
       cnt = SendSSDBSelectRequest ( &SSDB, NULL, szReq  );
       if ( cnt <  0 ) goto Exit; /* DataBase Error */
       if ( cnt == 0 ) { res  = CLASS_NOTFOUND; goto Exit; } /* Class ID Does not Exist */
     }
     
     /* ok we have finished with insert validation */
     
   } else {
   
     /* Validate Update or update */
     if ( eventvar == VALTYPE_SINGINTVALUE ||
          eventvar == VALTYPE_SINGINTVARNAME )
     {
       snprintf (  szReq, sizeof(szReq), 
                  "select type_id from event_type where type_id=%d and sys_id ='%s'",   
                   *neweventid, sys_id );
                   
       cnt = SendSSDBSelectRequest ( &SSDB, NULL, szReq  );
       if ( cnt <  0 ) goto Exit; 
       if ( cnt == 0 ) 
       { 
          res  = EVENT_NOTFOUND; 
          goto Exit; 
       }
     }
   } 
   
   if ( op == OP_VALIDATE_INSERT || op == OP_VALIDATE_UPDATE )
   {
      res   = 0;
      goto Exit;
   }

   /* We are ready to do insert now */
   
   if ( op == OP_INSERT_EVENT ) 
   {
     /* Insert event_class record */
     if ( class_dscr )
     { 
        snprintf ( szReq, sizeof(szReq), 
                   "insert into event_class values ('%s',%d,'%s')",
                    sys_id, *newclassid, classdescval );  
        cnt = SendSSDBInsertRequest ( &SSDB, NULL, szReq  );
        if ( cnt < 0 ) goto Exit;
        
     } 
     /* insert event record */
     snprintf ( szReq,  sizeof(szReq), 
 	       "insert into event_type values('%s',%d,%d,\"%s\",1,%d,%d,-1,%d)",
	        sys_id, *neweventid, *newclassid, eventdescval, thcountval, timeoutval, enabledval?1:0);
	       
     cnt = SendSSDBInsertRequest ( &SSDB, NULL, szReq  );
     if ( cnt < 0 ) goto Exit;
    
     res = 1;  /* Normal termination */
     goto Exit;
   }
   
   if ( op == OP_UPDATE_EVENT ) 
   {
     snprintf ( szReq,  sizeof(szReq), 
	       "update event_type set sehthrottle=%d, sehfrequency=%d, enabled=%d "
	       "where  type_id=%d and sys_id='%s'", 
	        thcountval, timeoutval, enabledval ? 1 : 0, *neweventid, sys_id );
	       
     cnt = SendSSDBUpdateRequest ( &SSDB, NULL, szReq  );
     if ( cnt < 0 ) goto Exit;
     res = 1;
     goto Exit;
   }

Exit:   

   if ( res == SSDB_ERR )
        PrintLastSSDBError ( &SSDB, szReq );
        
   CleanSSDBContext   ( &SSDB );    
 
   return res;
}
/* ---------------------------------------------------------------------------- */

int update_action ( sscErrorHandle  hError     , 
                    const char    * sys_id     ,
                    const char    * action_id  ,
                    const char    * action_desc, 
                    const char    * action_cmd , 
                    const char    * dsmthrottle,
                    const char    * dsmfreq    ,                     
                    const char    * retrycount ,
                    const char    * timeout    ,
                    const char    * userstring ,
                    const char    * hostname   ,
                    int           * newactionid,
                    CMDPAIR       * Vars       , 
                    int             Varc       ,
                    int             op         )
{
   int          cnt;
   SSDBContext  SSDB = {0};
   char         szReq[512] = "";
   int          res  = SSDB_ERR;
   int          actionidvar;
   int          dsmthrotvar;
   int          dsmthrotval;
   int          dsmfreqvar ;
   int          dsmfreqval ;  
   int          actiondescvar;
   int          action_cmdvar;
   int          retrycountvar;    
   int          retrycountval; 
   int          timeoutvar   ;
   int          timeoutval   ;
   int          userstringvar;
   int          hostnamevar  ;
   const char  *actiondescval = NULL;
   const char  *userstringval = NULL;
   const char  *action_cmdval = NULL;
   const char  *hostnameval   = NULL;
   const char **vector;
   
   actionidvar    = ResolveIntValue ( Vars, Varc, action_id  , newactionid    );
   
   actiondescvar  = ResolveStrValue ( Vars, Varc, action_desc, &actiondescval );
   action_cmdvar  = ResolveStrValue ( Vars, Varc, action_cmd , &action_cmdval );
   
   dsmthrotvar    = ResolveIntValue ( Vars, Varc, dsmthrottle, &dsmthrotval   );
   dsmfreqvar     = ResolveIntValue ( Vars, Varc, dsmfreq    , &dsmfreqval    );

   retrycountvar  = ResolveIntValue ( Vars, Varc, retrycount , &retrycountval );
   timeoutvar     = ResolveIntValue ( Vars, Varc, timeout    , &timeoutval    );
   
   userstringvar  = ResolveStrValue ( Vars, Varc, userstring , &userstringval );
   hostnamevar    = ResolveStrValue ( Vars, Varc, hostname   , &hostnameval   );

   if ( op == OP_UPDATE_ACTION || op == OP_VALIDATE_UPDATE )
   {
       /* we need action to do so */
       if (!(actionidvar == VALTYPE_SINGINTVALUE || actionidvar == VALTYPE_SINGINTVARNAME ))
       {
         sscError ( hError, "Incorrect action_id variable type in update_action call" );
         sscError ( hError, "action_id type = %d", actionidvar );
         return TYPE_ERR;
       }
   }

   if ( op == OP_INSERT_ACTION || op == OP_VALIDATE_INSERT )
   {
     if (!(actiondescvar == VALTYPE_SINGSTRVALUE || actiondescvar == VALTYPE_SINGSTRVARNAME))
     {
        sscError ( hError, "Incorrect actiondescvar parameter in update_event call" );      
        sscError ( hError, "class_desc type = %d", actiondescvar );
        return TYPE_ERR;
     }
     if ( IsStrBlank ( actiondescval  ))
     {
        return ACTIONDSCR_EMPTY;
     }     
   }   
   
   if (!(action_cmdvar == VALTYPE_SINGSTRVALUE || action_cmdvar == VALTYPE_SINGSTRVARNAME))
    {
       sscError ( hError, "Incorrect action_cmdvar parameter in update_event call" );      
       sscError ( hError, "action_cmd type = %d", action_cmdvar );
       return TYPE_ERR;
    }
   
   if (!(userstringvar == VALTYPE_SINGSTRVALUE || userstringvar == VALTYPE_SINGSTRVARNAME))
   {
        sscError ( hError, "Incorrect actiondescvar parameter in update_event call" );      
        sscError ( hError, "class_desc type = %d", userstringvar );
        return TYPE_ERR;
   }
   if(verify_action_uid(userstringval) == 0)
   { sscError ( hError, "Username is invalid. Action cannot be executed by root." );
     return TYPE_ERR;
   }
   if(verify_action_uid(userstringval) == 2)
   { sscError ( hError, "Invalid username." );
     return TYPE_ERR;
   }
   if ( IsStrBlank ( userstringval ))
   {
        userstringval = "nobody";
   }

   if ( timeoutvar == VALTYPE_VALBLANK || timeoutvar == VALTYPE_BADINTFORMAT) 
   {
       return DSMTIMEOUT_OUTOFRANGE;
   }
   if (!(timeoutvar == VALTYPE_SINGINTVALUE || timeoutvar == VALTYPE_SINGINTVARNAME ))
   {
       sscError ( hError, "Incorrect timeoutvar parameter type in update_event call" );
       sscError ( hError, "timeoutvar type = %d", timeoutvar );
       return TYPE_ERR;
   }     
   if ( timeoutval < 0 || timeoutval % 5 != 0)
   {
       return DSMTIMEOUT_OUTOFRANGE;
   }    


   if ( dsmthrotvar == VALTYPE_VALBLANK || dsmthrotvar == VALTYPE_BADINTFORMAT) 
   {
       return DSMTHROT_OUTOFRANGE;
   }
   if (!(dsmthrotvar == VALTYPE_SINGINTVALUE || dsmthrotvar == VALTYPE_SINGINTVARNAME ))
   {
       sscError ( hError, "Incorrect dsmthrotvar parameter type in update_event call" );
       sscError ( hError, "dsmthrotvar type = %d", dsmthrotvar );
       return TYPE_ERR;
   }     
   if ( dsmthrotval < 0 )
   {
       return DSMTHROT_OUTOFRANGE;
   }    

   if ( dsmfreqvar == VALTYPE_VALBLANK || dsmfreqvar == VALTYPE_BADINTFORMAT) 
   {
       return DSMFREQ_OUTOFRANGE;
   }
   if (!(dsmfreqvar == VALTYPE_SINGINTVALUE || dsmfreqvar == VALTYPE_SINGINTVARNAME ))
   {
       sscError ( hError, "Incorrect dsmfreqvar parameter type in update_event call" );
       sscError ( hError, "dsmfreqvar type = %d", dsmfreqvar );
       return TYPE_ERR;
   }     
   if ( dsmfreqval < 0 || dsmfreqval % 5 != 0)
   {
       return DSMFREQ_OUTOFRANGE;
   }    


   if ( retrycountvar == VALTYPE_VALBLANK || retrycountvar == VALTYPE_BADINTFORMAT) 
   {
       return DSMRETRYCOUNT_OUTOFRANGE;
   }
   if (!(retrycountvar == VALTYPE_SINGINTVALUE || retrycountvar == VALTYPE_SINGINTVARNAME ))
   {
       sscError ( hError, "Incorrect retrycountvar parameter type in update_event call" );
       sscError ( hError, "retrycountvar type = %d", retrycountvar );
       return TYPE_ERR;
   }     
   if ( retrycountval < 0 || retrycountval >= 24 )
   {
       return DSMRETRYCOUNT_OUTOFRANGE;
   }    
   
   if ( !OpenSSDBContext ( hError, &SSDB )) 
         goto Exit;
   
   if ( !SendSSDBLockTables ( &SSDB, "event_action" )) 
         goto Exit;
   
   if ( op == OP_UPDATE_ACTION || op == OP_VALIDATE_UPDATE )
   {
     /* validate action id */
     snprintf ( szReq,  sizeof(szReq), 
               "select action_id from event_action where action_id=%d and sys_id = '%s'",
                *newactionid, sys_id );
     cnt = SendSSDBSelectRequest ( &SSDB, NULL, szReq  );
     if ( cnt < 0  )   goto Exit; 
     if ( cnt == 0 ) { res  = ACTION_NOTFOUND; goto Exit; }
     
     if ( op == OP_UPDATE_ACTION )
     {             
        snprintf ( szReq,  sizeof(szReq), 
		  "update event_action set dsmthrottle=%d,dsmfrequency=%d,retrycount=%d,timeoutval=%d,userstring=\"%s\",action=\"%s\" "
		  "where action_id = %d",
  		   dsmthrotval, dsmfreqval, retrycountval, timeoutval,userstringval, action_cmdval, *newactionid );
	       
        cnt = SendSSDBUpdateRequest ( &SSDB, NULL, szReq  );
        if ( cnt < 0 ) goto Exit;
        res = 1;
     } else {
        res = 0;
     }
  }   
    
  if ( op == OP_INSERT_ACTION || op == OP_VALIDATE_INSERT )
  {
     /* Check action existence */
     snprintf ( szReq,  sizeof(szReq), 
               "select action_id from event_action where action_desc='%s' and sys_id = '%s'",
                actiondescval, sys_id );
     cnt = SendSSDBSelectRequest ( &SSDB, NULL, szReq  );
     if ( cnt < 0 ) goto Exit; 
     if ( cnt > 0 ) { res  = ACTION_ALREADY_EXIST; goto Exit; }
     
     if ( op == OP_INSERT_ACTION ) 
     {
      /* insert event record */
      snprintf (   szReq,  sizeof(szReq), 
     		  "insert into event_action values ('%s',NULL,'',%d,%d,\"%s\",%d,%d,\"%s\",\"%s\",0,0)", 
	   	   sys_id, 
	   	   dsmthrotval,
	   	   dsmfreqval,
	   	   action_cmdval,
	   	   retrycountval,
	   	   timeoutval,
	   	   userstringval,
	   	   actiondescval);
	       
      cnt = SendSSDBInsertRequest ( &SSDB, NULL, szReq  );
      if ( cnt < 0 ) goto Exit;

      /* retrieve the last action id */
      snprintf (   szReq, sizeof(szReq), 
     		  "select last_insert_id()" ); 
      cnt = SendSSDBSelectRequest ( &SSDB, NULL, szReq  );
      if ( cnt <= 0 ) goto Exit;
      if ( cnt >= 0 )
      {
         vector = SSDBGetRow ( &SSDB );
         if ( vector == NULL || 
              vector[0] == NULL || 
              sscanf ( vector[0], "%d", newactionid ) != 1)
              goto Exit;
              
         res = 1;       
      }
    } else {
      res = 0;
    }
  }

Exit:   

   if ( res == SSDB_ERR )
        PrintLastSSDBError ( &SSDB, szReq );
   
   CleanSSDBContext   ( &SSDB );    
   return res;
}

/* ---------------------------------------------------------------------------- */


int do_validate_sing_event ( SSDBContext *pDB, 
                             char        *pReq,
                             int          nReq,
                             const char  *sysid,
                             const char  *szEvent )
{
    int eventid, cnt;

    if ( 1 == sscanf ( szEvent, "%d", &eventid ))
    { 
       /* event id */
       snprintf (  pReq, nReq, 
                  "select type_id from event_type where "
                  "type_id = %d and sys_id = '%s'",
                   eventid, sysid);
                   
       cnt = SendSSDBSelectRequest ( pDB, NULL, pReq );
       if ( cnt < 0  ) 
            return SSDB_ERR;
            
       if ( cnt == 0 )
            return EVENT_NOTFOUND;
    } else {
      return EVENT_NOTFOUND;
    }
    return 1;
}                           


int do_validate_mult_events ( SSDBContext *pDB, 
                              char        *pReq,
                              int          nReq,
                              const char  *sysid,
                              const char  *szEvent, 
                              CMDPAIR     *Vars, 
                              int          Varc)
{
  int eventidx, eventcnt, cnt, eventid;
  
  eventidx = eventcnt = 0;
  while ( eventidx < Varc )
  {
    eventidx = sscFindPairByKey ( Vars, eventidx, szEvent );
    if ( eventidx == -1 ) 
         break; /* the end */

    cnt = do_validate_sing_event ( pDB, pReq, nReq, sysid, Vars[eventidx].value );
    if ( cnt < 0)
         return cnt;

    eventcnt++;
    eventidx++;
  } 
  return eventcnt;
}

int do_validate_sing_action ( SSDBContext *pDB  , 
                              char        *pReq ,
                              int          nReq ,
                              const char  *sysid,
                              const char  *szAction ) 
{
    int actionid, cnt;
   
    if ( 1 == sscanf ( szAction, "%d", &actionid ))
    { 
       /* action id */
       snprintf (  pReq, nReq, 
                  "select action_id from event_action where "
                  "action_id = %d and sys_id = '%s'",
                   actionid, sysid);
                   
       cnt = SendSSDBSelectRequest ( pDB, NULL, pReq );
       if ( cnt < 0  ) 
            return SSDB_ERR;
            
       if ( cnt == 0 )
            return ACTION_NOTFOUND;
    } else {
      return ACTION_NOTFOUND;
    }
    return 1;
}


int do_validate_mult_actions( SSDBContext *pDB, 
                              char        *pReq,
                              int          nReq,
                              const char  *sysid,
                              const char  *szAction, 
                              CMDPAIR     *Vars, 
                              int          Varc)
{
  int  actioncnt, actionidx, cnt;
  
  actioncnt = actionidx = 0;
  while ( actionidx < Varc )
  {
    actionidx = sscFindPairByKey ( Vars, actionidx, szAction );
    if ( actionidx == -1 ) 
         break; /* the end */

    cnt = do_validate_sing_action ( pDB, pReq, nReq, sysid, Vars[actionidx].value );
    if ( cnt < 0 ) 
         return cnt;
           
    actioncnt++; 
    actionidx++;
  } 
  
  return actioncnt;
}


#define MAX_ACTIONS 512

int do_update_actionref_sing(  SSDBContext *pDB, 
                               char        *pReq,
                               int          nReq,
                               const char  *sysid,
                               const char  *szEvent, 
                               const char  *szAction,
                               CMDPAIR     *Vars, 
                               int          Varc)
{
  int   cnt, nPriv;
  int   eventidx  ;
  int   eventcnt  ;
  int   actionidx ;
  int   actioncnt ;
  int   eventid   ;
  int   actionid  ;
  int   classid   ;
  int   OldIDS[MAX_ACTIONS];

    if ( 1 == sscanf ( szEvent, "%d", &eventid ))
    { 
       /* event id */
       /* extract class id */
       snprintf (  pReq, nReq, 
                  "select class_id from event_type where "
                  "type_id = %d and sys_id = '%s'",
                   eventid, sysid);
       cnt = SendSSDBSelectRequest ( pDB, NULL, pReq );
       if ( cnt  < 0 ) 
            return SSDB_ERR;
            
       if ( cnt == 0 )
            return EVENT_NOTFOUND;
            
       if ( !GetSSDBNextField ( pDB, &classid, sizeof(int)))
             return SSDB_ERR;
       
      /* 
        Remove old non private actions associated with this event  
        Let's assume that it is not possible to have more then 512 
        actions associated with single event.
        It is fair assumption because it is not possible to have 
        more then 512 key-value pair anyway.
        So basicaly we are going to store all SGM actions into stack array,
        delete all old actions, insert new action set and restore the SGM actions.
      */
   
      /* Save old private actions into local array */
       snprintf (  pReq, nReq, 
                  "select event_action.action_id from event_actionref, event_action "
                  "where event_actionref.type_id = %d and event_actionref.sys_id = '%s' and "
                  "event_actionref.action_id = event_action.action_id and event_action.private = 1",
                   eventid, sysid);
       nPriv = SendSSDBSelectRequest ( pDB, NULL, pReq );
       if ( nPriv < 0 ) 
            return SSDB_ERR;
            
       /* For all actions selected */     
       for ( actionidx = 0; actionidx < nPriv && actionidx < MAX_ACTIONS; ++actionidx )
       {
         if (!GetSSDBNextField ( pDB, OldIDS + actionidx, sizeof(int)))
              return SSDB_ERR;
       }
       
       snprintf ( pReq, nReq, 
                  "delete from event_actionref where "
                  "type_id = %d and sys_id = '%s'",
                   eventid, sysid);
       
       cnt = SendSSDBDeleteRequest ( pDB, NULL, pReq );
       if ( cnt < 0 ) 
            return SSDB_ERR;
            
       /* set new actions */     
       actionidx = 0; 
       while ( actionidx < Varc )
       {
         actionidx = sscFindPairByKey ( Vars, actionidx, szAction );
         if ( actionidx == -1 ) 
              break; /* the end */

         if ( 1 == sscanf ( Vars[actionidx].value, "%d", &actionid ))
         { 
            snprintf ( pReq, nReq, 
                      "replace into event_actionref values ('%s', NULL,%d,%d,%d) ",
                       sysid, classid, eventid, actionid );
                       
            cnt = SendSSDBInsertRequest ( pDB, NULL, pReq );
            if ( cnt < 0 ) 
                 return SSDB_ERR;
         }
         actionidx++;
       }
       
       /* Restore private actions if any */
       for ( actionidx = 0; actionidx < nPriv && actionidx < MAX_ACTIONS; ++actionidx )
       {
            snprintf ( pReq, nReq, 
                      "replace into event_actionref values ('%s', NULL,%d,%d,%d) ",
                       sysid, classid, eventid, OldIDS[actionidx] );
                       
            cnt = SendSSDBInsertRequest ( pDB, NULL, pReq );
            if ( cnt < 0 ) 
                 return SSDB_ERR;
       }
    }
    return 1; 
}                               

int do_update_actionref_mult ( SSDBContext *pDB, 
                               char        *pReq,
                               int          nReq,
                               const char  *sysid,
                               const char  *szEvent, 
                               const char  *szAction,
                               CMDPAIR     *Vars, 
                               int          Varc)
{
  int   cnt;
  int   eventidx  ;
  int   eventcnt  ;

  eventcnt = eventidx = 0;
  while ( eventidx < Varc )
  {
    eventidx = sscFindPairByKey ( Vars, eventidx, szEvent );
    if ( eventidx == -1 ) 
         break; /* the end */

    cnt = do_update_actionref_sing( pDB, pReq, nReq, sysid, Vars[eventidx].value, szAction, Vars, Varc );
    if ( cnt < 0 )
         return cnt;
    
    eventcnt++;
    eventidx++;
  } 
  return eventcnt;
}

/* ---------------------------------------------------------------------------- */
/* Set action vector for specified event                                        */
/* ---------------------------------------------------------------------------- */

int update_actionref ( sscErrorHandle  hError   , 
                       const char    * sys_id   ,
                       const char    * event_id ,
                       const char    * action_id,
                       CMDPAIR       * Vars     , 
                       int             Varc     ,
                       int             op       )
{
   int          cnt;
   SSDBContext  SSDB = {0};
   char         szReq[512];
   int          res  = SSDB_ERR;
   int          actionidvar;
   int          actionidval;
   int          eventidvar ;
   int          eventidval ;

   szReq[0] = 0;

   actionidvar = ResolveIntValue ( Vars, Varc, action_id, &actionidval );
   eventidvar  = ResolveIntValue ( Vars, Varc, event_id , &eventidval  );

   if (!(actionidvar == VALTYPE_VARNOTFOUND    || /* it means that we must remove all action from this event */
         actionidvar == VALTYPE_SINGINTVARNAME ||
         actionidvar == VALTYPE_MULTINTVARNAME ))
   {
      sscError ( hError, "Incorrect action_id variable type in update_actionref call" );
      sscError ( hError, "action_id  type = %d", actionidvar );
      return TYPE_ERR;
   }
   
   if (!(eventidvar  == VALTYPE_SINGINTVALUE   ||
         eventidvar  == VALTYPE_SINGINTVARNAME ||
         eventidvar  == VALTYPE_MULTINTVARNAME ))
   {
      sscError ( hError, "Incorrect event_id variable type in update_actionref call" );
      sscError ( hError, "event_id  type = %d", eventidvar );
      return TYPE_ERR;
   }

   if ( !OpenSSDBContext ( hError, &SSDB )) 
         goto Exit;
   
   if ( !SendSSDBLockTables ( &SSDB, "event_actionref, event_type, event_action, event_class")) 
         goto Exit;

   if ( eventidvar == VALTYPE_SINGINTVARNAME ||eventidvar == VALTYPE_MULTINTVARNAME )
   {
     cnt =  do_validate_mult_events( &SSDB, szReq, sizeof(szReq), sys_id, event_id, Vars, Varc);
   } else {
     cnt =  do_validate_sing_event ( &SSDB, szReq, sizeof(szReq), sys_id, event_id );
   }
   if ( cnt < 0 ) { res = cnt; goto Exit; }     

   if ( actionidvar != VALTYPE_VARNOTFOUND )
   {
     cnt =  do_validate_mult_actions ( &SSDB, szReq, sizeof(szReq), sys_id, action_id, Vars, Varc);
     if ( cnt < 0 ) { res = cnt; goto Exit; }     
   }  

   if ( eventidvar == VALTYPE_SINGINTVARNAME ||eventidvar == VALTYPE_MULTINTVARNAME )
      cnt = do_update_actionref_mult ( &SSDB, szReq, sizeof(szReq), sys_id, event_id, action_id, Vars, Varc);
   else
      cnt = do_update_actionref_sing ( &SSDB, szReq, sizeof(szReq), sys_id, event_id, action_id, Vars, Varc);
                               
   res = cnt;
   
Exit:   

   if ( res == SSDB_ERR )
        PrintLastSSDBError ( &SSDB, szReq );
   
   CleanSSDBContext   ( &SSDB );    
   return res;
}

/* ---------------------------------------------------------------------------- */
/* Delete Event Stuff                                                           */
/* ---------------------------------------------------------------------------- */

int do_delete_single_event ( SSDBContext *pDB, 
                             char        *pReq,
                             int          nReq,
                             const char  *sysid,
                             const char  *eventid, 
                             int          checkclass )
{
   int cnt, classid;

   /* check if this event does exist */
   
   snprintf (  pReq, nReq, 
              "select class_id from event_type where "
              "type_id = %s and sys_id = '%s'",
               eventid, sysid);
                   
   cnt = SendSSDBSelectRequest ( pDB, NULL, pReq );
   if ( cnt < 0  ) 
        return SSDB_ERR;

    /* But if it does not, who cares anyway */
              
   if ( cnt == 0 )
        return 0; /* No events actually were deleted */
            
   if (!GetSSDBNextField ( pDB, &classid, sizeof(int)))
        return SSDB_ERR;
   
   snprintf (  pReq, nReq, 
              "delete from event_actionref where type_id = %s and sys_id = '%s'",
              eventid, sysid);
   cnt = SendSSDBDeleteRequest ( pDB, NULL, pReq );
   if ( cnt < 0  ) 
        return SSDB_ERR;
        
   /* remove registration records for this event */        
   snprintf (  pReq, nReq, 
              "delete from event where type_id = %s and sys_id = '%s'",
               eventid, sysid);
   cnt = SendSSDBDeleteRequest ( pDB, NULL, pReq );
   if ( cnt < 0  ) 
        return SSDB_ERR;
        
   /* remove actions registration record for this event */        
   snprintf (  pReq, nReq, 
              "delete from actions_taken where event_id = %s and sys_id = '%s'",
               eventid, sysid);
   cnt = SendSSDBDeleteRequest ( pDB, NULL, pReq );
   if ( cnt < 0  ) 
        return SSDB_ERR;
                     
   /* delete event itself */     
   snprintf (  pReq, nReq, 
              "delete from event_type where type_id = %s and sys_id = '%s'",
               eventid, sysid);
   cnt = SendSSDBDeleteRequest ( pDB, NULL, pReq );
   if ( cnt < 0  ) 
        return SSDB_ERR;

   /* check if we need to delete this class as well */     
   if ( checkclass && classid >= 8000 ) /* custom class */     
   {
     snprintf (  pReq, nReq, 
                "select type_id from event_type where class_id = %d and sys_id = '%s'",
                 classid, sysid);
                      
     cnt = SendSSDBSelectRequest ( pDB, NULL, pReq );
     if ( cnt < 0  ) 
          return SSDB_ERR;
              
     if ( cnt == 0 )
     { /* remove class as well */
       snprintf (  pReq, nReq, 
                  "delete from event_class where class_id = %d and sys_id = '%s'",
                   classid, sysid);
       cnt = SendSSDBDeleteRequest ( pDB, NULL, pReq );
       if ( cnt < 0  ) 
            return SSDB_ERR;
     }     
   }
   return 1;
}                              

int do_delete_mult_events  (  SSDBContext *pDB, 
                              char        *pReq,
                              int          nReq,
                              const char  *sysid,
                              const char  *szEvent, 
                              CMDPAIR     *Vars, 
                              int          Varc,
                              int          checkclass)
{
  int eventidx, eventcnt, cnt, eventid, classid;
  
  eventidx = eventcnt = 0;
  while ( eventidx < Varc )
  {
    eventidx = sscFindPairByKey ( Vars, eventidx, szEvent );
    if ( eventidx == -1 ) 
         break; /* the end */

    if ( 1 == sscanf ( Vars[eventidx].value, "%d", &eventid ))
    { 
      cnt = do_delete_single_event ( pDB, pReq, nReq, sysid, Vars[eventidx].value, checkclass ); 
      if ( cnt < 0 )
           return cnt; 
       
      eventcnt++;
    } else {
      return EVENT_NOTFOUND; /* the variable is empty */
    }
    eventidx++;
  } 
  return eventcnt;
}

int delete_events ( sscErrorHandle  hError   , 
                    const char    * sys_id   ,
                    const char    * event_id ,
                    CMDPAIR       * Vars     , 
                    int             Varc     ,
                    int             op       )
{
   int          cnt;
   SSDBContext  SSDB = {0};
   char         szReq[512];
   int          res  = SSDB_ERR;
   int          eventidvar ;
   int          eventidval ;

   szReq[0] = 0;

   eventidvar  = ResolveIntValue ( Vars, Varc, event_id , &eventidval  );

   if (!(eventidvar == VALTYPE_SINGINTVARNAME ||
         eventidvar == VALTYPE_MULTINTVARNAME ))
   {
      sscError ( hError, "Incorrect eventid parameter type in delete_events call" );
      sscError ( hError, "eventid  type = %d", eventidvar );
      return TYPE_ERR;
   }

   if ( !OpenSSDBContext ( hError, &SSDB )) 
         goto Exit;
   
   if ( !SendSSDBLockTables ( &SSDB, "event_class, event, actions_taken, event_type, event_actionref")) 
         goto Exit;

   if ( eventidvar == VALTYPE_SINGINTVARNAME || eventidvar == VALTYPE_MULTINTVARNAME ) 
   {
       cnt = do_delete_mult_events   ( &SSDB, szReq, sizeof(szReq), sys_id, event_id,  Vars, Varc, 1);
   }    
   if ( eventidvar == VALTYPE_SINGINTVALUE )
   {
       cnt = do_delete_single_event ( &SSDB, szReq, sizeof(szReq),  sys_id, event_id, 1 );
   }
   res = cnt;
 
Exit:   

   if ( res == SSDB_ERR )
        PrintLastSSDBError ( &SSDB, szReq );
   
   CleanSSDBContext   ( &SSDB );    
   return res;
}                

/* ---------------------------------------------------------------------------- */
/* Delete Class Stuff                                                           */
/* ---------------------------------------------------------------------------- */

int do_delete_sing_class ( SSDBContext *pDB  , 
                           char        *pReq ,
                           int          nReq ,
                           const char  *sysid,
                           const char  *szClass ) 
{
    int  i, cnt, res;
    const char ** vector = NULL;

    /* Check if events exeist */
    snprintf ( pReq, nReq, 
              "select type_id from event_type where class_id = %s and sys_id = '%s'",
               szClass, sysid );
    cnt = SendSSDBSelectRequest ( pDB, NULL, pReq );
    if ( cnt < 0  ) 
         return SSDB_ERR;    

    /* for each event in the class do */
    for ( i = 0; i < cnt; i++)
    {
       vector = SSDBGetRow ( pDB ); 
       if ( vector == NULL || vector[0] == NULL )
            return SSDB_ERR;
       
       res = do_delete_single_event ( pDB, pReq, nReq, sysid, vector[0], 0 ); 
       if ( res == SSDB_ERR )
            return res;
    }

    /* remove class itself */     
    snprintf ( pReq, nReq, 
              "delete from event_class where class_id=%s and sys_id = '%s'",
               szClass, sysid );
    cnt = SendSSDBDeleteRequest ( pDB, NULL, pReq );
    if ( cnt < 0  ) 
         return SSDB_ERR;

    return cnt;
}


int do_delete_mult_class  (   SSDBContext *pDB, 
                              char        *pReq,
                              int          nReq,
                              const char  *sysid,
                              const char  *szClass, 
                              CMDPAIR     *Vars, 
                              int          Varc)
{
  int  classcnt, classidx, cnt;
  
  classcnt = classidx = 0;
  while ( classidx < Varc )
  {
    classidx = sscFindPairByKey ( Vars, classidx, szClass );
    if ( classidx == -1 ) 
         break; /* the end */

    if ( 1 == sscanf ( Vars[classidx].value, "%d", &cnt ))
    { 
      cnt = do_delete_sing_class ( pDB, pReq, nReq, sysid, Vars[classidx].value );
      if ( cnt < 0 ) 
           return cnt;
    } else {
      return CLASS_NOTFOUND;  
    }     
    classcnt++; 
    classidx++;
  } 
  return classcnt;
}

int delete_class  ( sscErrorHandle  hError   , 
                    const char    * sys_id   ,
                    const char    * class_id ,
                    CMDPAIR       * Vars     , 
                    int             Varc     ,
                    int             op       )
{
   int          cnt;
   int          classidvar;
   int          classidval;
   SSDBContext  SSDB = {0};
   char         szReq[512] = "";
   int          res  = SSDB_ERR;

   classidvar  = ResolveIntValue ( Vars, Varc, class_id , &classidval  );

   if ( classidvar == VALTYPE_VARNOTFOUND )
        return NOTHING_SELECTED;

   if (!(classidvar == VALTYPE_SINGINTVARNAME ||
         classidvar == VALTYPE_MULTINTVARNAME ||
         classidvar == VALTYPE_SINGINTVALUE   ))
   {
      sscError ( hError, "Incorrect classidvar parameter type in delete_class call" );
      sscError ( hError, "classidvar  type = %d", classidvar );
      return TYPE_ERR;
   }

   if ( !OpenSSDBContext ( hError, &SSDB )) 
         goto Exit;
   
   if ( !SendSSDBLockTables ( &SSDB, "event_class, event, actions_taken, event_type, event_actionref")) 
         goto Exit;

   if ( classidvar == VALTYPE_SINGINTVARNAME || 
        classidvar == VALTYPE_MULTINTVARNAME )
        cnt = do_delete_mult_class  ( &SSDB, szReq, sizeof(szReq), sys_id, class_id,  Vars, Varc );
   else       
        cnt = do_delete_sing_class  ( &SSDB, szReq, sizeof(szReq), sys_id, class_id );

   res = cnt;
 
Exit:   

   if ( res == SSDB_ERR )
        PrintLastSSDBError ( &SSDB, szReq );
   
   CleanSSDBContext   ( &SSDB );    
   return res;
}

/* ---------------------------------------------------------------------------- */
/*  Delete Actions stuff                                                        */
/* ---------------------------------------------------------------------------- */

int do_delete_sing_action (   SSDBContext *pDB  , 
                              char        *pReq ,
                              int          nReq ,
                              const char  *sysid,
                              const char  *szAction ) 
{
    int actionid, cnt;
   
    if ( 1 == sscanf ( szAction, "%d", &actionid ))
    { 
       /* delete action from the actionref */
       snprintf (  pReq, nReq, 
                  "delete from event_actionref where "
                  "action_id = %d and sys_id = '%s'",
                   actionid, sysid);
       cnt = SendSSDBDeleteRequest ( pDB, NULL, pReq );
       if ( cnt < 0  ) 
            return SSDB_ERR;
            
       /* delete from action taken table  */     
       snprintf (  pReq, nReq, 
                  "delete from actions_taken where "
                  "action_id = %d and sys_id = '%s'",
                   actionid, sysid);
       cnt = SendSSDBDeleteRequest ( pDB, NULL, pReq );
       if ( cnt < 0  ) 
            return SSDB_ERR;

       snprintf (  pReq, nReq, 
                  "select action_id from event_action where "
                  "action_id = %d and sys_id = '%s'",
                   actionid, sysid);
                   
       cnt = SendSSDBSelectRequest ( pDB, NULL, pReq );
       if ( cnt < 0  ) 
            return SSDB_ERR;
       if ( cnt == 0 )     
            return ACTION_NOTFOUND;
   
       /* delete action from event_action table */   
       snprintf (  pReq, nReq, 
                  "delete from event_action where "
                  "action_id = %d and sys_id = '%s'",
                   actionid, sysid);
                   
       cnt = SendSSDBDeleteRequest ( pDB, NULL, pReq );
       if ( cnt < 0  ) 
            return SSDB_ERR;
    } else {
      return ACTION_NOTFOUND;
    }
    return cnt;
}


int do_delete_mult_actions(   SSDBContext *pDB, 
                              char        *pReq,
                              int          nReq,
                              const char  *sysid,
                              const char  *szAction, 
                              CMDPAIR     *Vars, 
                              int          Varc)
{
  int  actioncnt, actionidx, cnt;
  
  actioncnt = actionidx = 0;
  while ( actionidx < Varc )
  {
    actionidx = sscFindPairByKey ( Vars, actionidx, szAction );
    if ( actionidx == -1 ) 
         break; /* the end */

    cnt = do_delete_sing_action ( pDB, pReq, nReq, sysid, Vars[actionidx].value );
    if ( cnt < 0 ) 
         return cnt;
           
    actioncnt++; 
    actionidx++;
  } 
  return actioncnt;
}


int delete_actions  (  sscErrorHandle  hError   , 
                       const char    * sys_id   ,
                       const char    * action_id,
                       CMDPAIR       * Vars     , 
                       int             Varc     ,
                       int             op       )
{
   int          cnt;
   SSDBContext  SSDB = {0};
   char         szReq[512] = "";
   int          res  = SSDB_ERR;
   int          actionidvar ;
   int          actionidval ;

   actionidvar  = ResolveIntValue ( Vars, Varc, action_id , &actionidval  );
   
   if ( actionidvar == VALTYPE_VARNOTFOUND )
         return NOTHING_SELECTED;

   if (!(actionidvar == VALTYPE_SINGINTVARNAME ||
         actionidvar == VALTYPE_MULTINTVARNAME ||
         actionidvar == VALTYPE_SINGINTVALUE   ))
   {
      sscError ( hError, "Incorrect actionid parameter type in delete_events call" );
      sscError ( hError, "actionid  type = %d", actionidvar );
      return TYPE_ERR;
   }

   if ( !OpenSSDBContext ( hError, &SSDB )) 
         goto Exit;
   
   if ( !SendSSDBLockTables ( &SSDB, "event_action, actions_taken, event_actionref")) 
         goto Exit;

   if ( actionidvar == VALTYPE_SINGINTVARNAME || actionidvar == VALTYPE_MULTINTVARNAME )
        cnt = do_delete_mult_actions  ( &SSDB, szReq, sizeof(szReq), sys_id, action_id,  Vars, Varc );
   else       
        cnt = do_delete_sing_action   ( &SSDB, szReq, sizeof(szReq), sys_id, action_id );

   res = cnt;
 
Exit:   

   if ( res == SSDB_ERR )
        PrintLastSSDBError ( &SSDB, szReq );
   
   CleanSSDBContext   ( &SSDB );    
   return res;
}               
