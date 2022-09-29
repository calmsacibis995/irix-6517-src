/* --------------------------------------------------------------------------- */
/* -                             RGPSETUPSRV.C                               - */
/* --------------------------------------------------------------------------- */
/*                                                                             */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                  */
/* All Rights Reserved.                                                        */
/*                                                                             */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;      */
/* the contents of this file may not be disclosed to third parties, copied or  */
/* duplicated in any form, in whole or in part, without the prior written      */
/* permission of Silicon Graphics, Inc.                                        */
/*                                                                             */
/* RESTRICTED RIGHTS LEGEND:                                                   */
/* Use, duplication or disclosure by the Government is subject to restrictions */
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data      */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or    */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -     */
/* rights reserved under the Copyright Laws of the United States.              */
/*                                                                             */
/* --------------------------------------------------------------------------- */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/socket.h>
#include <malloc.h>

#include <rgPluginAPI.h>
#include <sscHTMLGen.h>
#include <sscPair.h>
#include <sscShared.h>
#include <eventmonapi.h>

#include "rgpDBCalls.h"
#include "rgpFormat.h"
#include "rgpRes.h"

/* --------------------------------------------------------------------------- */

#ifdef __TIME__
#ifdef __DATE__
#define INCLUDE_TIME_DATE_STRINGS 1
#endif
#endif

/* --------------------------------------------------------------------------- */

#define MYVERSION_MAJOR    1          /* Report Generator PlugIn Major Version number */
#define MYVERSION_MINOR    0          /* Report Generator PlugIn Minor Version number */
#define MYVERSION_BUILD    0          /* Report Generator PlugIn Build number         */
#define MAXNUMBERS_OF_KEYS 512        /* Max size of Pair array */

/* --------------------------------------------------------------------------- */

static const char myLogo      []               = "ESP Configuration Setup plugin";
static const char szVersion   []               = "Version";            /* Obligatory attribute name: current version of this plugin */
static const char szTitle     []               = "Title";              /* Obligatory attribute name: real name of this plugin */
static const char szThreadSafe[]               = "ThreadSafe";         /* Obligatory attribute name: Is this plugin threadsafe */
static const char szUnloadable[]               = "Unloadable";         /* Obligatory attribute name: Is this plugin unlodable */
static const char szUnloadTime[]               = "UnloadTime";         /* Obligatory attribute name: What unload time for this plugin */
static const char szAcceptRawCmdString[]       = "AcceptRawCmdString"; /* Obligatory attribute name: how plugin will process cmd parameters */

/* Error Message Format Strings ---------------------------------------------- */

static char szErrorFormat []                   = "ESP Configuration Setup Error: ";
static char szErrorFormat1[]                   = "ESP Configuration Setup Error: %s";
static char szErrorFormat2[]                   = "ESP Configuration Setup Error: %s : %s";
static char szErrorFormat3[]                   = "ESP Configuration Setup Error: %s : %s : %s";

/* Error Message Strings ----------------------------------------------------- */

static const char szErrMsgCantInitMutex  []    = "Can't initialize mutex \"seshFreeListmutex\"";
static const char szErrMsgCantInitResMan []    = "Unable to initialize resource manager";
static const char szErrMsgNoMemory       []    = "Not enough memory to create new session context";
static const char szErrMsgBadSession     []    = "Incorrect session";
static const char szErrMsgCantOpenHtmlGen[]    = "Unable to open the HTML generator";
static const char szErrMsgDestroyHTMLGen []    = "Unable to destroy the HTML generator";

static const char szErrMsgAttributeUnknown[]   = "Unknown attribute name";
static const char szErrMsgSetROAttr       []   = "Attempt to set read only attribute";
static const char szErrMsgUnknownReqType  []   = "Unknown request type";

static const char szErrMsgIncorrectArgc[]      = "Incorrect number of arguments.";
static const char szErrMsgNotEnoughArgs[]      = "There is not enough arguments to complete this request.";

static const char szErrMsgDefResFailed  []     = "Unable to load Resource file";
static const char szErrMsgTemplNotFound []     = "Unable to find HTML Template";
static const char szErrMsgSysIDNotFound []     = "Unable to retrieve system id";

static const char szErrMsgBadVarValue   []     = "The value \"%s\" is incorrect for variable %s";
static const char szErrMsgBadArgValue   []     = "The value \"%s\" is incorrect for argument %d";
static const char szErrMsgBadFormat     []     = "Incorrect \"Format:\" parameter";
static const char szErrMsgFormatNotFound[]     = "Undefined output format for this request";
static const char szErrMsgBadString     []     = "Unable to expract string from \"%s\"";   

static const char szErrMsgNoGlobalSetup[]      = "There is no parameters for global setup in the database.";
static const char szErrMsgGlobalSetupFailed[]  = "Unable to set global configuration data.";
static const char szErrMsgNoClasses        []  = "There is no event classes in the database.";
static const char szErrMsgNoEvents         []  = "There is no events in the database.";
static const char szErrMsgNoActions        []  = "There is no actions in the database.";
static const char szErrMsgEventIDParam     []  = "Event ID parameter is missing.";
static const char szErrMsgEventDscrParam   []  = "Event Description parameter is missing.";
static const char szErrMsgEventClassParam  []  = "Event Class parameter is missing";
static const char szErrMsgEventMonDead     []  = "Event monitoring daemon is not running\n";

/*** Specific  Errors ***/

static const char szErrMsgBadEventIDRange  []  = "Event ID must be integer value in 8400000-8500000 range. "
                                                 "Please, enter correct value and try again."; 
                                                 
static const char szErrMsgEventDscrMissing []  = "Event description is missing. "
                                                 "Please, enter the unique event description and try again.";
                                                 
static const char szErrMsgClassDscrMissing []  = "Class description is missing. "
                                                 "Please, enter the unique class description and try again.";
                                                 
static const char szErrMsgEventIDExist     []  = "Event with such ID already exist in database. "
                                                 "Please, enter different ID and try again.";
                                                 
static const char szErrMsgClassIDNotExist  []  = "Class with ClassID = <b>%s</b> does not exist in database.";

static const char szErrMsgEvenDescrExist   []  = "Event with such description already exist in database. "
                                                 "Please, enter unique event description and try again.";
                                                 
static const char szErrMsgClassDescrExist  []  = "Event class with such description already exist in database. " 
                                                 "Please, enter unique event class description and try again.";

static const char szErrMsgBadThrottlingValue[] = "\"The number of event occurances\" must be non negative integer value. "
                                                 "Please, enter the correct value and try again.";

static const char szErrMsgBadTimeoutValue   [] = "\"The minimum time interval\" must be non negative, multiple of 5 integer value. "   
                                                 "Please, enter the correct value and try again.";
                                                 
/* Other strings and declarations  ------------------------------------------------------- */
static char  szVersionStr[]                    = "1.0.0"; /*a.b.c -  Where a - major version, 
                                                                           b - minor version, 
                                                                           c - build       */
enum USER_AGENTS 
{
  UA_LINX = 1,
  UA_MOZILLA ,
  UA_OTHER
};
                                                                           
static const char szUserAgent[]                = "User-Agent";
static const char szAgentLynx[]                = "Lynx";
static const char szAgentMzll[]                = "Mozilla";
static const char szNscpTemplateName []        = "sscSetupTmplNetscape";
static const char szLynxTemplateName []        = "sscSetupLynx";
static const char szOthrTemplateName []        = "sscSetupLynx";

static const char szUnOrdered        []        = "";

static const char szSysIDArgTag      []        = "sys_id";
static const char szFormatArgTag     []        = "format:";
static const char szCustomArgTag     []        = "custom:";
static const char szOnEmptyArgTag    []        = "onempty:";

static const char szUsage[]                    = "Please use usage command: rgpsetup~usage";

static const char szUnloadTimeValue[]          = "300"; /* Unload time for this plug in (sec.) */
static const char szThreadSafeValue[]          = "1";   /* "Thread Safe" flag value - this plugin is thread safe */
static const char szUnloadableValue[]          = "1";   /* "Unloadable" flag value - RG core might unload this plugin from memory */
static const char szAcceptRawCmdStringValue[]  = "1";   /* RG server accept "raw" command string like key=value&key2=value&... */


/* --------------------------------------------------------------------------- */
/* Local "Session" structure                                                   */
/* --------------------------------------------------------------------------- */


typedef struct sss_s_MYSESSION 
{
  unsigned long           signature;  /* sizeof(mySession)       */
  struct sss_s_MYSESSION *next     ;  /* next session            */
  enum USER_AGENTS        UserAgent;  /* Current User Agent      */
  RGPResHandle            hDefRes  ;  /* Default Resource Handle */
  const char *            pGenErr  ;  /* Generic Error message   */
  const char *            pFrmtStr ;  /* Format  string          */
  const char *            pCustStr ;  /* Custom SQL addition for select requests  */
  const char *            pOnEmpty ;  /* The template in the case that the request is empty */  
  int                     Argc     ;  /* Argument  count         */
  char                  **Argv     ;  /* Argument  vector        */     
  int                     Varc     ;  /* Variables count         */  
  CMDPAIR                *Vars     ;  /* Variables Vector        */
} mySession;

/* --------------------------------------------------------------------------- */
/* Local Static Variables                                                      */
/* --------------------------------------------------------------------------- */

static pthread_mutex_t seshFreeListmutex;
static int volatile    mutex_initialized = 0;    /* seshFreeListmutex mutex initialized flag */
static mySession      *sesFreeList       = 0;    /* Session free list */
static int volatile    bRGPinitialized   = 0;    /* Plugin has been succesfully initialized  */

/* --------------------------------------------------------------------------- */
/* Local Function prototype                                                    */
/* --------------------------------------------------------------------------- */

void CmdUsage                 ( sscErrorHandle  hError, mySession *pSession );
void CmdPrintVars             ( sscErrorHandle  hError, mySession *pSession );

void CmdGetSysID              ( sscErrorHandle  hError, mySession *pSession );
void CmdSelectEvents          ( sscErrorHandle  hError, mySession *pSession );
void CmdSelectClasses         ( sscErrorHandle  hError, mySession *pSession );
void CmdSelectActions         ( sscErrorHandle  hError, mySession *pSession );
void CmdGetGlobalSetup        ( sscErrorHandle  hError, mySession *pSession );
void CmdSetGlobalFlags        ( sscErrorHandle  hError, mySession *pSession );

void CmdAddCustomEvent        ( sscErrorHandle  hError, mySession *pSession );
void CmdDeleteCustomEvents    ( sscErrorHandle  hError, mySession *pSession );
void CmdSetEventAttr          ( sscErrorHandle  hError, mySession *pSession );
void CmdSetEventActions       ( sscErrorHandle  hError, mySession *pSession );
void CmdUpdateActions         ( sscErrorHandle  hError, mySession *pSession );
void CmdDeleteActions         ( sscErrorHandle  hError, mySession *pSession );
void CmdDeleteClass           ( sscErrorHandle  hError, mySession *pSession );
void CmdSaveVariable          ( sscErrorHandle  hError, mySession *pSession );
void CmdSubstVariable         ( sscErrorHandle  hError, mySession *pSession );  

/* --------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------- */
/* --------------------------- Several Useful Helpers ------------------------ */
/* --------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------- */

char * get_var_value ( mySession * pSession, char *szName )
{
   int  idx;
    
    if ( pSession == NULL ) 
         return NULL;
  
    if ( pSession->Vars == NULL || pSession->Varc == 0 )
         return NULL;
        
    idx = sscFindPairByKey ( pSession->Vars, 0, szName );
    if ( idx < 0 )
         return NULL;
         
   return pSession->Vars[idx].value;
}

/* --------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------- */
/* ----------------------------- rgpgInit ------------------------------------ */
/* --------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------- */
int RGPGAPI rgpgInit(sscErrorHandle hError)
{ 
  /* Try initialize free list of mySession mutex */
  if ( bRGPinitialized == 0 ) 
  {
    if ( RGPResInit ( hError ) == 0 )
    {
       sscError ( hError, szErrorFormat1, szErrMsgCantInitResMan );
       return 0;
    }     
    
    if ( pthread_mutex_init(&seshFreeListmutex,0) != 0 )
    { 
       sscError ( hError, szErrorFormat1, szErrMsgCantInitMutex );
       return 0; /* error */
    }
    mutex_initialized++;
    bRGPinitialized = 1;
  }  
  
  return bRGPinitialized;
}

/* ----------------------------- rgpgDone ------------------------------------ */
int RGPGAPI rgpgDone(sscErrorHandle hError)
{ 
  mySession *s;

  (void) hError;
 
  /* Destroy  Resource Manager */
  RGPResDone ( hError );
 
  /* Destroy  Mutex and Session Lists Lists  */
  if ( mutex_initialized )
  { 
    pthread_mutex_destroy(&seshFreeListmutex);
    mutex_initialized = 0;
    while((s = sesFreeList) != 0)
    { 
      sesFreeList = s->next;
      free(s);
    }
  }
  /* Destroy other parts if any */
  
  bRGPinitialized = 0;
  return 1;
}

/* -------------------------- rgpgCreateSesion ------------------------------- */
rgpgSessionID RGPGAPI rgpgCreateSesion(sscErrorHandle hError)
{  
   mySession *s;

   /* get free session context block */ 
   pthread_mutex_lock(&seshFreeListmutex);
   if((s = sesFreeList) != 0) sesFreeList = s->next;
   pthread_mutex_unlock(&seshFreeListmutex);

   if(!s) /* we don't have any and must create the new one */
       s = (mySession*)malloc(sizeof(mySession));
   if(!s) 
   { /* Unable to create the new context , report error    */
     sscError( hError, szErrorFormat1, szErrMsgNoMemory );
     return 0;
   }
   memset(s,0,sizeof(mySession));
   s->signature = sizeof(mySession);
   return (rgpgSessionID)s;
}


/* ------------------------- rgpgDeleteSesion -------------------------------- */
void RGPGAPI rgpgDeleteSesion(sscErrorHandle hError,rgpgSessionID _s)
{ 
  mySession *s = (mySession *)_s;
  if(!s || s->signature != sizeof(mySession))
  { 
    sscError(hError, szErrorFormat1, szErrMsgBadSession );
  }
  else
  { pthread_mutex_lock(&seshFreeListmutex);
    s->next = sesFreeList; 
    sesFreeList  = s; 
    s->signature = 0;
    pthread_mutex_unlock(&seshFreeListmutex);
  }
}

/* ------------------------ rgpgGetAttribute --------------------------------- */
char *RGPGAPI rgpgGetAttribute( sscErrorHandle hError ,  
                                rgpgSessionID  session, 
                                const char    *attributeID,  
                                const char    *extraAttrSpec, 
                                int           *typeattr ) 
{  
   char *s;

   if( typeattr ) 
      *typeattr = RGATTRTYPE_STATIC;

        if(!strcasecmp(attributeID,szVersion))            s = (char*)szVersionStr;
   else if(!strcasecmp(attributeID,szTitle))              s = (char*)myLogo; 
   else if(!strcasecmp(attributeID,szUnloadTime))         s = (char*)szUnloadTimeValue;
   else if(!strcasecmp(attributeID,szThreadSafe))         s = (char*)szThreadSafeValue;
   else if(!strcasecmp(attributeID,szUnloadable))         s = (char*)szUnloadableValue;
   else if(!strcasecmp(attributeID,szAcceptRawCmdString)) s = (char*)szAcceptRawCmdStringValue;
   else 
   { 
     sscError(hError, szErrorFormat2, szErrMsgAttributeUnknown, attributeID );
     return 0;
   }
   
   return s;
}


/* --------------------- rgpgFreeAttributeString ------------------------------ */
void RGPGAPI rgpgFreeAttributeString ( sscErrorHandle hError, 
                                       rgpgSessionID  session,
                                       const char    *attributeID, 
                                       const char    *extraAttrSpec, 
                                             char    *attrString,   
                                              int     restype)
{  
  if (((restype == RGATTRTYPE_SPECIALALLOC) || (restype == RGATTRTYPE_MALLOCED)) && attrString) 
       free(attrString);
}



/* ------------------------ rgpgSetAttribute --------------------------------- */
void RGPGAPI rgpgSetAttribute( sscErrorHandle hError, 
                               rgpgSessionID  session, 
                               const char    *attributeID, 
                               const char    *extraAttrSpec, 
                               const char    *value )
{  
   mySession *pSession = (mySession *)session;

   if( pSession == NULL  || 
       pSession->signature != sizeof(mySession))
   { 
     sscError( hError, szErrorFormat, szErrMsgBadSession );
     return;
   }

   if( !strcasecmp(attributeID,szVersion)    || 
       !strcasecmp(attributeID,szTitle)      ||
       !strcasecmp(attributeID,szThreadSafe) || 
       !strcasecmp(attributeID,szUnloadable) ||
       !strcasecmp(attributeID,szUnloadTime))
   { 
     sscError ( hError, szErrorFormat, szErrMsgSetROAttr );
     return;
   }

   if( !strcasecmp ( attributeID, szUserAgent) && value ) 
   {
     if (  strcasecmp (value, szAgentLynx ) == 0 ) 
           pSession->UserAgent = UA_LINX;
     else  if (  strcasecmp (value, szAgentMzll ) == 0 ) 
           pSession->UserAgent = UA_MOZILLA;
     else        
           pSession->UserAgent = UA_OTHER;      
   }
}
/* ----------------------- rgpgGenerateReport -------------------------------- */
int RGPGAPI rgpgGenerateReport( sscErrorHandle  hError , 
                                 rgpgSessionID   session, 
                                 int             argc   , 
                                 char           *argv[] ,
                                 char           *rawCommandString,
                                 streamHandle    result ,
                                 char           *userAccessMask )
{  
   int         i,j,n;
   const char *szDefResName;
   int         cmdpNum;
   CMDPAIR     cmdpArr[MAXNUMBERS_OF_KEYS];
   
   mySession *pSession = (mySession *)session;

   /* Check is session valid */
   if( pSession == NULL || 
       pSession->signature != sizeof(mySession) )
   { 
     sscError(hError,szErrorFormat1, szErrMsgBadSession );
     return RGPERR_SUCCESS;
   }

   /* Let's check arguments count for this session */
   if(argc < 2)
   { /* We must have at least one command argument in order to proceed */
     sscError( hError, szErrorFormat1, szErrMsgIncorrectArgc );
     sscError( hError, szErrorFormat1, szUsage ); 
     return RGPERR_SUCCESS;
   }

   /* Initialize Command Pair array */
   cmdpNum = sscInitPair ( rawCommandString, cmdpArr, MAXNUMBERS_OF_KEYS );
   
   /* Fill Session Parameters */
   pSession->pFrmtStr = NULL;
   pSession->pCustStr = NULL;  
   pSession->pOnEmpty = NULL;
   pSession->pGenErr  = NULL; 
   pSession->hDefRes  = NULL;
   pSession->Varc     = cmdpNum;
   pSession->Vars     = cmdpArr;
   pSession->Argc     = argc;
   pSession->Argv     = argv;

   /* Check Common Attribute Here */
   /* Check WEB browser type (Lynx is only text based browser) */ 
   for ( i= 0; i < cmdpNum; ++i )
   {
     if ( strcasecmp ( cmdpArr[i].keyname, szUserAgent ) == 0 )
     { 
        if (  strcasecmp (cmdpArr[i].value, szAgentLynx ) == 0 ) 
              pSession->UserAgent = UA_LINX;
        else  if (  strcasecmp (cmdpArr[i].value, szAgentMzll ) == 0 ) 
              pSession->UserAgent = UA_MOZILLA;
        else        
              pSession->UserAgent = UA_OTHER;      
     }     
   }
    
   /* Load Default Resource file */
   switch ( pSession->UserAgent )
   {
     case UA_LINX   : szDefResName = szLynxTemplateName;  break;
/*   case UA_MOZILLA: szDefResName = szNscpTemplateName;  break; */
     case UA_MOZILLA: szDefResName = szLynxTemplateName;  break;
     default        : szDefResName = szOthrTemplateName;  break; 
   }     


  
   pSession->hDefRes = GetRGPResHandle ( hError, szDefResName, -1 );
   if ( pSession->hDefRes == NULL )
   {
     sscError( hError, szErrorFormat2, szErrMsgDefResFailed, szDefResName );
     return RGPERR_SUCCESS;
   }
   
   pSession->pGenErr = GetRGPResString ( hError, pSession->hDefRes, "GenErrMsg1", -1 );
   if ( pSession->pGenErr == NULL ) 
     return RGPERR_SUCCESS;

   /* Initialize The HTML Generator  */
   if( createMyHTMLGenerator ( result ) )
   { 
     sscError ( hError, szErrorFormat1, szErrMsgCantOpenHtmlGen );
     return RGPERR_SUCCESS;
   }

   /* Start HTML generation here  */
   if( 0 == strcasecmp(argv[1],      "USAGE"   ))
     CmdUsage             (hError, pSession );
     
   else if ( 0 == strcasecmp(argv[1],"PRINTVAR")) 
     CmdPrintVars         (hError, pSession );

   else if ( 0 == strcasecmp(argv[1], "SELECTEVENTS" )) 
     CmdSelectEvents      (hError, pSession );

   else if ( 0 == strcasecmp(argv[1], "SELECTCLASSES")) 
     CmdSelectClasses     (hError, pSession );
     
   else if ( 0 == strcasecmp(argv[1], "SELECTACTIONS"))  
     CmdSelectActions     (hError, pSession );

   else if ( 0 == strcasecmp(argv[1], "GLOBALSETUP")) 
     CmdGetGlobalSetup    (hError, pSession );
     
   else if ( 0 == strcasecmp(argv[1], "SETGLOBALFLAGS"))
     CmdSetGlobalFlags    (hError, pSession );

   else if ( 0 == strcasecmp(argv[1], "GETSYSID" )) 
     CmdGetSysID          (hError, pSession );
     
   else if ( 0 == strcasecmp(argv[1], "ADDCUSTOMEVENT"))  
     CmdAddCustomEvent    (hError, pSession );
     
   else if ( 0 == strcasecmp(argv[1], "DELETECUSTOMEVENTS"))  
     CmdDeleteCustomEvents(hError, pSession );
     
   else if ( 0 == strcasecmp(argv[1], "SETEVENTATTR"))  
     CmdSetEventAttr      (hError, pSession );
  
   else if ( 0 == strcasecmp(argv[1], "SETEVENTACTIONS"))  
     CmdSetEventActions   (hError, pSession );
     
   else if ( 0 == strcasecmp(argv[1], "UPDATEACTIONS"))  
     CmdUpdateActions     (hError, pSession );     
     
   else if ( 0 == strcasecmp(argv[1], "DELETEACTIONS"))  
     CmdDeleteActions     (hError, pSession );
     
   else if ( 0 == strcasecmp(argv[1], "SAVEVARIABLE"))  
     CmdSaveVariable      (hError, pSession );

   else if ( 0 == strcasecmp(argv[1], "DELETECLASS"))  
     CmdDeleteClass       (hError, pSession );
     
   else if ( 0 == strcasecmp(argv[1], "SUBSTVARIABLE"))       
     CmdSubstVariable     (hError, pSession );  

   
   else
   {
     sscError( hError, szErrorFormat2, (char*) szErrMsgUnknownReqType, argv[1] );
     sscError( hError, szErrorFormat1, szUsage );
   }
   /* End HTML generation here     */
   
   /*  Destroy The HTML Generator  */
   if ( deleteMyHTMLGenerator() ) 
        sscError (hError, szErrorFormat, szErrMsgDestroyHTMLGen );
        
   return RGPERR_SUCCESS;
}

/*******************************************************************************/
/*******************************************************************************/
/*******************************************************************************/
/*******************************************************************************/
/*******************************************************************************/
/* ------------------------------ ---------------------------------------------*/
void CmdUsage      ( sscErrorHandle  hError, mySession *pSession )
{
  Body ( RGPResExtractString ( hError, pSession->hDefRes, ":Usage" ));
}
/* ------------------------------ ---------------------------------------------*/
void CmdPrintVars  ( sscErrorHandle  hError, mySession *pSession )
{
   int i;
   
   Body ("<p>Print Variables:</p>");
   
   if ( pSession->Varc <= 0)
   { /* There is no variables*/
     Body ("<p>There is no variables to print</p>");
     return;  
   }
   
   Body ("<p>");
   for (i= 0; i < pSession->Varc; i++ ) 
   {
     FormatedBody ( "%s \t:%s<br>", pSession->Vars[i].keyname, pSession->Vars[i].value ); 
   }
   Body ("</p>");
}
/* ------------------------------ ---------------------------------------------*/
/* ------------------------------ ---------------------------------------------*/
/* ------------------------------ ---------------------------------------------*/
/* ------------------------------ ---------------------------------------------*/
/* ------------------------------ ---------------------------------------------*/
/* ------------------------------ ---------------------------------------------*/
/* ------------------------------ ---------------------------------------------*/
/* ------------------------------ ---------------------------------------------*/

typedef struct {
  const char *szFormat  ; /* Formating string  */
  const char *pRecordBeg; 
  const char *pRecordEnd; 
  CMDPAIR    *pGlobPairs;
  int         nGlobPairs; 
} GenRecFC;

int FormatRecordGenProc ( void * pContext, int idx, CMDPAIR *Vars, int Varc )
{
  GenRecFC *pFC = (GenRecFC *) pContext;
  
  if ( pFC == NULL )
       return ENUM_ERR;
       
  if ( idx == ENUMIDX_INIT )
  {
     pFC->pRecordBeg = FormatHead ( pFC->szFormat, pFC->pGlobPairs, pFC->nGlobPairs ); 
     return 1;
  }
  if ( idx == ENUMIDX_DONE )
  {
     FormatTail  ( pFC->pRecordEnd, pFC->pGlobPairs, pFC->nGlobPairs ); 
     return 1;
  }
  
  if ( idx >= 0 )
  {
     pFC->pRecordEnd = FormatRecord ( pFC->pRecordBeg, Vars, Varc );
     return 1;
  } 
  
  return 0;
}

int handle_system_id ( sscErrorHandle  hError, mySession *pSession, char *sysid, int len )
{
  char *pSysID;
  
  pSysID = get_var_value ( pSession, "system_id" );
  if ( pSysID == NULL ) 
  {
    if ( get_sys_id ( hError, sysid, len ) <= 0 )
    {
       sscError ( hError, szErrorFormat1, szErrMsgSysIDNotFound );
       return 0;
    }  
  }else {
   strncpy ( sysid, pSysID, len );
  }
  
  sysid [len-1] = 0; /* Just to terminate for sure */
  return 1;
}

int handle_extract_format ( sscErrorHandle  hError, mySession *pSession, int start_idx, const char *szDefault )
{
  int          i;
  const char * szFormat;
  
  for ( i = start_idx; i < pSession->Argc; i++ )
  {
    if ( strncasecmp ( pSession->Argv[i], szFormatArgTag, strlen(szFormatArgTag)) == 0 )
    { /* We have format tag */
      pSession->pFrmtStr = RGPResExtractString ( hError, 
                                                 pSession->hDefRes, 
                                                 pSession->Argv[i] + strlen(szFormatArgTag));  
      continue;
    }
  }
  if ( pSession->pFrmtStr == NULL )
  { 
    pSession->pFrmtStr = RGPResExtractString ( hError, 
                                               pSession->hDefRes, 
                                               (char*) szDefault );  
    if ( pSession->pFrmtStr == NULL )  
    {
      sscError ( hError, szErrorFormat1, szErrMsgFormatNotFound );
      return 0;
    } 
  }
  return 1;
}

int handle_extract_custom ( sscErrorHandle  hError, mySession *pSession, int start_idx )
{
  int          i;
  const char * szFormat;
  
  for ( i = start_idx; i < pSession->Argc; i++ )
  {
    if ( strncasecmp ( pSession->Argv[i], szCustomArgTag, strlen(szCustomArgTag)) == 0 )
    { /* We have format tag */
      pSession->pCustStr = RGPResExtractString ( hError, 
                                                 pSession->hDefRes, 
                                                 pSession->Argv[i] + strlen(szCustomArgTag));  
      continue;
    }
  }
  if ( pSession->pCustStr == NULL )
  {  
       pSession->pCustStr = "";
  }
  return 1;
}


int handle_extract_onempty ( sscErrorHandle  hError, mySession *pSession, int start_idx, const char * szDefault )
{
  int          i;
  const char * szFormat;
  
  for ( i = start_idx; i < pSession->Argc; i++ )
  {
    if ( strncasecmp ( pSession->Argv[i], szOnEmptyArgTag, strlen(szOnEmptyArgTag)) == 0 )
    { /* We have format tag */
      pSession->pOnEmpty = RGPResExtractString ( hError, 
                                                 pSession->hDefRes, 
                                                 pSession->Argv[i] + strlen(szOnEmptyArgTag));  
      continue;
    }
  }
  
  if ( pSession->pOnEmpty == NULL )
  {  
   if ( szDefault )
        pSession->pOnEmpty = RGPResExtractString ( hError, pSession->hDefRes, (char*) szDefault );  
        
   if ( pSession->pOnEmpty == NULL )
        pSession->pOnEmpty = "There are no records in database for your request";
  }
  return 1;
}



#define  ARGS_REQUIRED(hError, pSession, nArgs)               \
if ( pSession->Argc < nArgs + 2 )                             \
{                                                             \
  sscError ( hError, szErrorFormat1, szErrMsgNotEnoughArgs ); \
  return;                                                     \
}                                                             \

void register_changes ( sscErrorHandle hError )
{
  if(configure_event("UPDATE localhost -1 -1 -1 0 0"))
  {
      sscError(hError, (char*) szErrMsgEventMonDead );
      return;
  }
  
  if(configure_rule("UPDATE -1"))
  {
      sscError(hError, (char *) szErrMsgEventMonDead );
      return;
  }
  
  if(!emapiDeclareDaemonReloadConfig())
  {
      sscError(hError, "Failed response from event monitoring daemon\n" );
      return;
  }
}


/* ------------------------------ ---------------------------------------------*/
void CmdSelectEvents ( sscErrorHandle  hError, mySession *pSession )
{
  int   i, nRec  ;
  char  sysid[17];
  GenRecFC FC = {0}; 
  
  ARGS_REQUIRED( hError, pSession, 3 );  
  
  if ( !handle_system_id ( hError, pSession, sysid, sizeof(sysid)))
        return ;
  
  if ( !handle_extract_format ( hError, pSession, 5, NULL ))
        return;
        
  handle_extract_custom ( hError, pSession, 5 );
  handle_extract_onempty( hError, pSession, 5, NULL ); 
  
  FC.szFormat   = pSession->pFrmtStr;
  FC.pGlobPairs = pSession->Vars;
  FC.nGlobPairs = pSession->Varc;
  
  nRec = enum_events ( hError,  
                      (const char*)sysid, 
                       pSession->Argv[2], 
                       pSession->Argv[3], 
                       pSession->Argv[4], 
                       pSession->pCustStr, 
                       pSession->Vars, 
                       pSession->Varc,
                       FormatRecordGenProc, &FC );
  if ( nRec == 0 )
  { /* in this case we must execute on empty option */ 
    FormatHead ( pSession->pOnEmpty, pSession->Vars, pSession->Varc );
  }
}

/* ------------------------------ ---------------------------------------------*/
void CmdSelectClasses ( sscErrorHandle  hError, mySession *pSession )
{
  int   i, nRec  ;
  char  sysid[17];
  GenRecFC FC = {0}; 
  
  ARGS_REQUIRED( hError, pSession, 2 );  
  
  if ( !handle_system_id ( hError, pSession, sysid, sizeof(sysid) ))
        return ;
  
  if ( !handle_extract_format ( hError, pSession, 5, NULL ))
        return;

  handle_extract_custom ( hError, pSession, 5 );
  handle_extract_onempty( hError, pSession, 5, NULL );
  
  FC.szFormat   = pSession->pFrmtStr;
  FC.pGlobPairs = pSession->Vars;
  FC.nGlobPairs = pSession->Varc;

  nRec = enum_classes( hError,  
                      (const char*)sysid , 
                       pSession->Argv[2] , 
                       pSession->Argv[3] , 
                       pSession->Argv[4] , 
                       pSession->pCustStr, 
                       pSession->Vars    , 
                       pSession->Varc    ,
                       FormatRecordGenProc, &FC );
  if ( nRec == 0 )
  { 
    FormatHead ( pSession->pOnEmpty, pSession->Vars, pSession->Varc );
  }
}


/* ------------------------------ ---------------------------------------------*/
void CmdSelectActions ( sscErrorHandle  hError, mySession *pSession )
{
  int   i, nRec  ;
  char  sysid[17];
  GenRecFC FC = {0}; 
  
  ARGS_REQUIRED( hError, pSession, 3 );  
  
  if ( !handle_system_id ( hError, pSession, sysid, sizeof(sysid) ))
        return ;
  
  if ( !handle_extract_format ( hError, pSession, 5, NULL ))
    return;

  handle_extract_custom ( hError, pSession, 5 );
  handle_extract_onempty( hError, pSession, 5, NULL );

  FC.szFormat   = pSession->pFrmtStr;
  FC.pGlobPairs = pSession->Vars;
  FC.nGlobPairs = pSession->Varc;
  
  nRec = enum_actions( hError,  
                       (const char *)sysid, 
                       pSession->Argv[2] , 
                       pSession->Argv[3] , 
                       pSession->Argv[4] , 
                       pSession->pCustStr, 
                       pSession->Vars, 
                       pSession->Varc,
                       FormatRecordGenProc, &FC );
  if ( nRec == 0 )
  { 
    FormatHead ( pSession->pOnEmpty, pSession->Vars, pSession->Varc );
  }
}

/* ------------------------------ ---------------------------------------------*/
void CmdGetGlobalSetup ( sscErrorHandle  hError, mySession *pSession )
{
  int   i;
  int   Logging  ; 
  int   Filter   ; 
  int   Action   ;
  const char *Argv[3];
  
  ARGS_REQUIRED( hError, pSession, 0 );  
  
  if ( !handle_extract_format ( hError, pSession, 2, ":PageGlobalFlags" ))
    return;
    
  if ( !get_global_setup_flags ( hError,  &Logging, &Filter, &Action ) ||
        Logging == -1 || Filter == -1 || Action == -1 ) 
  {
    Argv[0] = szErrMsgNoGlobalSetup;
    VarPrintf ( pSession->pGenErr, NULL, Argv, 1 );
    return;
  }
  
  if ( Logging == 1 ) Argv[0] = "checked";  else  Argv[0] = "";
  if ( Filter  == 1 ) Argv[1] = "checked";  else  Argv[1] = "";
  if ( Action  == 1 ) Argv[2] = "checked";  else  Argv[2] = "";

  VarPrintf ( pSession->pFrmtStr, NULL, Argv, 3 );
  
  return;
}

/* ------------------------------ ---------------------------------------------*/
void CmdSetGlobalFlags ( sscErrorHandle  hError, mySession *pSession)
{
  int   i,  res;
  int   Logging = 0; 
  int   Filter  = 0; 
  int   Action  = 0;
  int   LoggingType   ;
  int   ThrottlingType;
  int   ActionType    ;
  const char *Vars[6] ;
  char  szConf[16];

  ARGS_REQUIRED( hError, pSession, 3 );
  
  if ( !handle_extract_format ( hError, pSession, 5, ":PageConfirmGlobalSetup" ))
    return;

  /* Find Flag To Set */
  LoggingType    = ResolveIntValue ( pSession->Vars, pSession->Varc, pSession->Argv[2], &Logging );
  ThrottlingType = ResolveIntValue ( pSession->Vars, pSession->Varc, pSession->Argv[3], &Filter  );
  ActionType     = ResolveIntValue ( pSession->Vars, pSession->Varc, pSession->Argv[4], &Action  );
  
  /* Set Flag in ssdb */     
  if ( 0 == set_global_setup_flags ( hError, Logging, Filter , Action ))
  {
    const char *pMsg =  szErrMsgGlobalSetupFailed;
    VarPrintf ( pSession->pGenErr, NULL, &pMsg, 1 );
    return;
  }
  
  /* Generate confirmation pages */
  if ( Logging ) 
  {
    Vars[0] = GetRGPResString ( hError, pSession->hDefRes, "StrLogOn"   , -1);
    Vars[1] = GetRGPResString ( hError, pSession->hDefRes, "StrLogOnMsg", -1);
  } else {
    Vars[0] = GetRGPResString ( hError, pSession->hDefRes, "StrLogOff"  , -1);
    Vars[1] = GetRGPResString ( hError, pSession->hDefRes, "StrLogOffMsg",-1);
  }  
     
  if ( Filter ) 
  {
    Vars[2] = GetRGPResString ( hError, pSession->hDefRes, "StrFilterOn"     ,-1);
    Vars[3] = GetRGPResString ( hError, pSession->hDefRes, "StrFilterOnMsg"  ,-1);
  } else  {
    Vars[2] = GetRGPResString ( hError, pSession->hDefRes, "StrFilterOff"    ,-1);
    Vars[3] = GetRGPResString ( hError, pSession->hDefRes, "StrFilterOffMsg" ,-1);
  }

  if ( Action ) 
  {
    Vars[4] = GetRGPResString ( hError, pSession->hDefRes, "StrActionsOn"    ,-1);
    Vars[5] = GetRGPResString ( hError, pSession->hDefRes, "StrActionsOnMsg" ,-1);
  } else  {
    Vars[4] = GetRGPResString ( hError, pSession->hDefRes, "StrActionsOff"   ,-1);
    Vars[5] = GetRGPResString ( hError, pSession->hDefRes, "StrActionsOffMsg",-1);
  }
  
  VarPrintf ( pSession->pFrmtStr, NULL, Vars, 6 ); 
  
  sprintf(szConf,"%d %d %d",Action,Filter,Logging);
  if(configure_global(szConf))
  {
     sscError ( hError, (char *) szErrMsgEventMonDead );
  }
}

/* ------------------------------ ---------------------------------------------*/
void CmdGetSysID ( sscErrorHandle  hError, mySession *pSession )
{
  char  sysid[17];
  
  const char *Name = szSysIDArgTag;
  const char *Vals = (const char *) sysid;
   
  if ( !handle_system_id ( hError, pSession, sysid, sizeof(sysid) ))
        return ;
  
  if ( !handle_extract_format ( hError, pSession, 0, ":FmtHiddenSysID" ))
        return;
 
  VarPrintf  ( pSession->pFrmtStr, &Name, &Vals, 1 );
}

void CmdSaveVariable  ( sscErrorHandle  hError, mySession *pSession )
{
  char fmt[128];
  int  idx;

  ARGS_REQUIRED( hError, pSession, 1 );  
  
  idx = sscFindPairByKey ( pSession->Vars, 0, pSession->Argv[2] );       
  if ( idx >= 0 )
  {
    FormatedBody ( "<input type=hidden name=%s value=%s>", pSession->Vars[idx].keyname, 
                                                           pSession->Vars[idx].value );
  }  
}

void CmdSubstVariable  ( sscErrorHandle  hError, mySession *pSession )
{
  ARGS_REQUIRED( hError, pSession, 0 );  

  if ( !handle_extract_format ( hError, pSession, 0, "::" ))
        return;
        
  FormatHead ( pSession->pFrmtStr, pSession->Vars, pSession->Varc );
}

/* ------------------------------ ---------------------------------------------*/
/* 
  the  order of parameters are 
  [2]  class_id, 
  [3]  class_desc, 
  [4]  event_type, 
  [5]  event_description, 
  [6]  thcount
  [7]  thtimeout
  [8]  enabled
  [9]  new_class
  [10] format
*/
void CmdAddCustomEvent ( sscErrorHandle  hError, mySession *pSession )
{
  const char *pMsg;
  char  sysid[17] ;
  char  szErr[512];
  int   bNew,  res;
  int   newclassid;
  int   neweventid;

  ARGS_REQUIRED( hError, pSession, 8 );  
  
  if ( !handle_system_id ( hError, pSession, sysid, sizeof(sysid) ))
        return;
        
  if ( !handle_extract_format ( hError, pSession, 10, ":AddCustomEventConf" ))
        return;
        
  handle_extract_custom ( hError, pSession, 8 );
  handle_extract_onempty( hError, pSession, 8, NULL ); 

  pMsg = (char *)szErr;        
  
  res  = ResolveIntValue ( pSession->Vars, pSession->Varc, pSession->Argv[9], &bNew );
  switch ( res )
  {
     case  VALTYPE_VARNOTFOUND   : bNew = 0; break;
     case  VALTYPE_SINGINTVARNAME:  
     case  VALTYPE_SINGINTVALUE  : break;
     default:
        sscError ( hError, "Incorrect parameter type for bnew class parameter" );
        return;
  }
  
  res = update_event( hError,     sysid,
                      pSession->Argv[2],                 /* class_id    */
                      bNew ? pSession->Argv[3] : NULL,   /* class_desc  */
                      pSession->Argv[4],                 /* typeid      */
                      pSession->Argv[5],                 /* typedescr   */
                      pSession->Argv[6],                 /* thcount     */       
                      pSession->Argv[7],                 /* thtimeout   */
                      pSession->Argv[8],                 /* enabled     */
                     &newclassid       ,     
                     &neweventid       ,     
                      pSession->Vars   , 
                      pSession->Varc   ,
                      OP_INSERT_EVENT );

  if ( res >= 0 ) 
  {
    int      nRec;
    GenRecFC FC = {0}; 
    char szNewClassID[16];
    char szNewEventID[16];
    
    sprintf ( szNewClassID, "%d", newclassid );
    sprintf ( szNewEventID, "%d", neweventid );
  
    FC.szFormat   = pSession->pFrmtStr;
    FC.pGlobPairs = pSession->Vars;
    FC.nGlobPairs = pSession->Varc;
 
    nRec = enum_events ( hError,  
                        (const char*)sysid, 
                         szNewClassID,  /* class_     */
                         szNewEventID,  /* event type */
                         "null", 
                         pSession->pCustStr, 
                         pSession->Vars, 
                         pSession->Varc,
                         FormatRecordGenProc, &FC );
                         
                         
    register_changes ( hError );
                         
    return;  
  }
  
  switch ( res ) 
  {
    case EVENTID_OUTOFRANGE    :
         pMsg = szErrMsgBadEventIDRange;
    break;

    case EVENTDSCR_EMPTY          : 
         pMsg = szErrMsgEventDscrMissing;
    break;
    
    case EVENT_ALREADY_EXIST      : 
         pMsg = szErrMsgEventIDExist; 
    break;
    
    case EVENTDSCR_ALREADY_EXIST  : 
         pMsg = szErrMsgEvenDescrExist;
    break;
    
    case CLASS_NOTFOUND           : 
         sprintf ( szErr, szErrMsgClassIDNotExist, newclassid );
    break;
   
    case CLASSDSCR_ALREADY_EXIST  : 
         pMsg = szErrMsgClassDescrExist;
    break;
    
    case CLASSDSCR_EMPTY          : 
         pMsg = szErrMsgClassDscrMissing;
    break;
    
    case THCOUNT_OUTOFRANGE:
         pMsg = szErrMsgBadThrottlingValue;
    break;
    
    case THTIMEOUT_OUTOFRANGE:
         pMsg = szErrMsgBadTimeoutValue;
    break;
    
    default:
         sprintf ( szErr, "Unexpected error occured" );
  }                      
 
Error :

  VarPrintf ( pSession->pGenErr, NULL, &pMsg, 1 );
  return ;  
}

/* ------------------------------ ---------------------------------------------*/
/* 
  the  order of parameters are 
  [2]  event_id
  [3]  delete_all - not only custom 
  [3]  format:
*/
void CmdDeleteCustomEvents ( sscErrorHandle  hError, mySession *pSession )
{
  const char *pMsg;
  char  sysid[17] ;
  char  szErr[512];
  int   bNew,  res;
  int   classid;

  ARGS_REQUIRED( hError, pSession, 1 );  
  
  if ( !handle_system_id ( hError, pSession, sysid, sizeof(sysid) ))
        return;
        
  if ( !handle_extract_format ( hError, pSession, 3, ":DelCEConf" ))
        return;

  handle_extract_custom ( hError, pSession, 3 );
  handle_extract_onempty( hError, pSession, 3, ":DelCEOnEmpty" ); 

  pMsg = (char *)szErr;        
  
  res = delete_events( hError,     
                       sysid,
                       pSession->Argv[2], /* event_id   */
                       pSession->Vars, 
                       pSession->Varc,
                       OP_DELETE_CUSTOM );
  if ( res > 0 ) 
  {
    /* Generate the new page here */    
    FormatHead ( pSession->pFrmtStr, pSession->Vars, pSession->Varc );

    register_changes ( hError );

    return;  
  }
  if ( res == 0 )
  {
    FormatHead ( pSession->pOnEmpty, pSession->Vars, pSession->Varc );
    
    register_changes ( hError );
  }
  
  /* Error processing goes here */
  switch ( res ) 
  {
    case EVENT_NOTFOUND: 
         sprintf ( szErr, szErrMsgEventIDExist ); 
    break;
         
    default:
         sprintf ( szErr, "Unexpected error occured" );
  }                      
 
Error :

  VarPrintf ( pSession->pGenErr, NULL, &pMsg, 1 );
  return ;  
}

/* ------------------------------ ---------------------------------------------*/

void CmdSetEventAttr ( sscErrorHandle  hError, mySession *pSession )
{
  const char *pMsg;
  char  sysid[17] ;
  char  szErr[512];
  int   bNew,  res;
  int   classid;
  int   eventid;

  ARGS_REQUIRED( hError, pSession, 4 );  
  
  if ( !handle_system_id ( hError, pSession, sysid, sizeof(sysid) ))
        return;
        
  if ( !handle_extract_format ( hError, pSession, 6, ":SetEventAttrConf" ))
        return;

  handle_extract_custom ( hError, pSession, 6 );
  handle_extract_onempty( hError, pSession, 6, NULL ); 
 
  pMsg = (char *)szErr;        

  /* 
  the  order of parameters are 
  [2]  event_id
  [3]  Throttling
  [4]  Freq
  [5]  Enabled
  [6]  format:
  */
  res = update_event( hError, 
                      sysid ,
                      NULL  ,
                      NULL  ,
                      pSession->Argv[2], 
                      NULL,
                      pSession->Argv[3],
                      pSession->Argv[4],
                      pSession->Argv[5],
                     &classid, 
                     &eventid,
                      pSession->Vars   , 
                      pSession->Varc   ,
                      OP_UPDATE_EVENT );
  if ( res >= 0 ) 
  {
    int      nRec;
    GenRecFC FC = {0}; 
  
    /* Generate the confirmation page here */    
    FC.szFormat   = pSession->pFrmtStr;
    FC.pGlobPairs = pSession->Vars;
    FC.nGlobPairs = pSession->Varc;
    
    nRec = enum_events ( hError,  
                        (const char*)sysid, 
                         "null",            /* */
                         pSession->Argv[2], /* event type */
                         "null", 
                         pSession->pCustStr, 
                         pSession->Vars, 
                         pSession->Varc,
                         FormatRecordGenProc, &FC );

    register_changes ( hError );

    return;  
  }
  
  /* Error processing goes here */
  switch ( res ) 
  {
    case EVENT_NOTFOUND :
         pMsg = "Event not found";
    break;
   
    case THCOUNT_OUTOFRANGE:
         pMsg = szErrMsgBadThrottlingValue;
    break;
    
    case THTIMEOUT_OUTOFRANGE:
         pMsg = szErrMsgBadTimeoutValue;
    break;
         
    default:
         sprintf ( szErr, "Unexpected error #%d occured", res );
  }                      
 
Error :

  VarPrintf ( pSession->pGenErr, NULL, &pMsg, 1 );
  return ;  
}

/* ----------------------------------------------------------------------- */
/* 
  the  order of parameters are 
  [2]  event_id
  [3]  action_id
  [4]  format:
*/
void CmdSetEventActions       ( sscErrorHandle  hError, mySession *pSession )
{
  const char *pMsg;
  char  sysid[17] ;
  char  szErr[512];
  
  int   res;

  ARGS_REQUIRED( hError, pSession, 2 );  
  
  if ( !handle_system_id ( hError, pSession, sysid, sizeof(sysid) ))
        return;
        
  if ( !handle_extract_format ( hError, pSession, 6, ":SetEventActionsConf" ))
        return;
 
  handle_extract_custom ( hError, pSession, 6 );
  handle_extract_onempty( hError, pSession, 6, ":SetEventActionsConfEmpty" );
  
  pMsg = (char *)szErr;        
  
  res = update_actionref ( hError, 
                           sysid ,
                           pSession->Argv[2],
                           pSession->Argv[3],
                           pSession->Vars   , 
                           pSession->Varc   ,
                           OP_UPDATE_ACTION);
  if ( res >= 0 ) 
  {
    GenRecFC FC = {0}; 
  
    /* Generate the new page here */    
    FC.szFormat   = pSession->pFrmtStr;
    
    FC.pGlobPairs = pSession->Vars;
    FC.nGlobPairs = pSession->Varc;
    
    res    =   enum_actions( hError,
                             sysid , 
                             "all", 
                             pSession->Argv[2],
                             "null",
                             pSession->pCustStr,
                             pSession->Vars, 
                             pSession->Varc,  
                             FormatRecordGenProc, &FC );
    if ( res == 0 )
    {
       FormatHead ( pSession->pOnEmpty, pSession->Vars, pSession->Varc );
    }
    
    register_changes ( hError );
    
    return;  
  }
  
  /* Error processing goes here */
  switch ( res ) 
  {
    case EVENT_NOTFOUND :
         pMsg = "Event not found";
    break;
    
    case ACTION_NOTFOUND :
         pMsg = "Action not found";
    break;
         
    default:
         sprintf ( szErr, "Unexpected error #%d occured", res );
  }                      
 
Error :

  VarPrintf ( pSession->pGenErr, NULL, &pMsg, 1 );
  return ;  
}

/* ----------------------------------------------------------------------- */
/* 
  the  order of parameters are 
  [ 2]  action_id
  [ 3]  action_desc
  [ 4]  action_cmd
  [ 5]  dsmthrottle
  [ 6]  dsmfreq
  [ 7]  retrycount 
  [ 8]  timeout
  [ 9]  userstring
  [10]  hostname 
  [11]  format:
*/
/* ----------------------------------------------------------------------- */

void CmdUpdateActions ( sscErrorHandle  hError, mySession *pSession )
{
  const char *pMsg;
  char  sysid[ 17];
  char  szErr[512];
  int   res,    op;
  int   newactionid;
  char  szIDStr[32];
  
  ARGS_REQUIRED( hError, pSession, 9 );  

  if ( strcasecmp ( pSession->Argv[2], "null" ) == 0 ) 
       op = OP_INSERT_ACTION;
  else    
       op = OP_UPDATE_ACTION;
  
  if ( !handle_system_id ( hError, pSession, sysid, sizeof(sysid) ))
        return;
       
  if ( !handle_extract_format ( hError, pSession, 6, (op == OP_INSERT_ACTION) ? ":AddActionConf" : ":UpdateActionConf"))
        return;

  handle_extract_custom ( hError, pSession, 6 );
  handle_extract_onempty( hError, pSession, 6, NULL );
 
  pMsg = (char *)szErr;        
 

  res = update_action(hError, 
                      sysid ,
                      pSession->Argv[ 2],
                      pSession->Argv[ 3], 
                      pSession->Argv[ 4], 
                      pSession->Argv[ 5],
                      pSession->Argv[ 6],                     
                      pSession->Argv[ 7],
                      pSession->Argv[ 8],
                      pSession->Argv[ 9],
                      pSession->Argv[10],
                     &newactionid       ,
                      pSession->Vars    , 
                      pSession->Varc    ,
                      op );
  if ( res >= 0 ) 
  {
    GenRecFC FC = {0};
  
    /* Generate the new confirmation page here  here */    
    /* we are just going to call select */
    
    FC.szFormat   = pSession->pFrmtStr;
    FC.pGlobPairs = pSession->Vars;
    FC.nGlobPairs = pSession->Varc;
    
    if ( op == OP_INSERT_ACTION ) 
         sprintf ( szIDStr, "%d", newactionid );
  
    res    =   enum_actions( hError,
                             sysid , 
                             "null", 
                             "null" ,
                             ( op == OP_INSERT_ACTION) ? szIDStr : pSession->Argv[ 2],
                             pSession->pCustStr,
                             pSession->Vars, 
                             pSession->Varc,  
                             FormatRecordGenProc, &FC );

    register_changes ( hError );

    return;  
  }
  
  /* Error processing goes here */
  switch ( res ) 
  {
    case ACTIONCMD_EMPTY:
         pMsg = "Action's command line is empty.";
    break;         
    
    case ACTIONDSCR_EMPTY:
         pMsg = "Action description is empty. "
                "Please, enter the unique action description and try again";
    break;
    
    case DSMTIMEOUT_OUTOFRANGE:
         pMsg = "Action timeout must be non negative, multiple of 5 integer value. "
                "Please, enter the correct value and try again";
    break;
    
    case DSMTHROT_OUTOFRANGE:
         pMsg = "The number of times an event must be registered before the action will be taken must be non negative integer value. "
                "Please, enter the correct value and try again";
    break;
    
    case DSMFREQ_OUTOFRANGE:
         pMsg = "The delay time between an event and the action must be non negative, multiple of 5 integer value. "
                "Please, enter the correct value and try again";
    break;
    
    case DSMRETRYCOUNT_OUTOFRANGE:
         pMsg = "The number of retry times must be integer value from 0 to 23. "
                "It is not recommended to set more then 3-4 retries. "
                "Please, enter the correct value and try again.";
    break;
    
    case ACTION_NOTFOUND:
         pMsg = "The action was not found in database.";
    break;
    
    case ACTION_ALREADY_EXIST:
         pMsg = "The action with such description already exist in database";
    break;

    default:
         sprintf ( szErr, "Unexpected error occured during update actions operation" );
  }                      
 
Error :

  VarPrintf ( pSession->pGenErr, NULL, &pMsg, 1 );
  return ;  
}



/* ----------------------------------------------------------------------- */
/* 
  the  order of parameters are 
  [2]  action_id
  [3]  format:
*/
/* ----------------------------------------------------------------------- */

void CmdDeleteActions ( sscErrorHandle  hError, mySession *pSession )
{
  const char *pMsg;
  char  sysid[ 17];
  char  szErr[512];
  int   res,    op;

  ARGS_REQUIRED( hError, pSession, 1 );  
  
  if ( !handle_system_id ( hError, pSession, sysid, sizeof(sysid) ))
        return;

  if ( !handle_extract_format ( hError, pSession, 6, ":DeleteActionConf1" ))
        return;
      
  pMsg = (char *)szErr;        

  res = delete_actions  (  hError , 
                           sysid  ,
                           pSession->Argv[2],
                           pSession->Vars   , 
                           pSession->Varc   ,  0);
  if ( res >= 0 ) 
  {
    FormatHead ( pSession->pFrmtStr, pSession->Vars, pSession->Varc );

    register_changes ( hError );

    return;  
  }
  
  /* Error processing goes here */
  switch ( res ) 
  {
    case ACTION_NOTFOUND:
         pMsg = "The action was not found in database.";
    break;
    
    default:
         sprintf ( szErr, "Unexpected error occured during delete actions operation" );
  }                      
 
Error :

  VarPrintf ( pSession->pGenErr, NULL, &pMsg, 1 );
  return ;  
}


/* ----------------------------------------------------------------------- */
/* 
  the  order of parameters are 
  [2]  class_id
  [3]  format:
*/
/* ----------------------------------------------------------------------- */
void CmdDeleteClass ( sscErrorHandle  hError, mySession *pSession )
{
  const char *pMsg;
  char  sysid[ 17];
  char  szErr[512];
  int   res,    op;

  ARGS_REQUIRED( hError, pSession, 1 );  
  
  if ( !handle_system_id ( hError, pSession, sysid, sizeof(sysid) ))
        return;

  if ( !handle_extract_format ( hError, pSession, 3, ":DelClassConf" ))
        return;

  handle_extract_custom ( hError, pSession, 3 );
      
  pMsg = (char *)szErr;        

  res  = delete_class  ( hError, 
                         sysid ,
                         pSession->Argv[2],
                         pSession->Vars   , 
                         pSession->Varc   , 0);
  if ( res >= 0 ) 
  {
    FormatHead ( pSession->pFrmtStr, pSession->Vars, pSession->Varc );

    register_changes ( hError );

    return;  
  }
  
  /* Error processing goes here */
  switch ( res ) 
  {
    case CLASS_NOTFOUND:
         pMsg = "The specified class was not found in database.";
    break;
    
    default:
         sprintf ( szErr, "Unexpected error occured during delete class operation" );
  }                      
 
Error :

  VarPrintf ( pSession->pGenErr, NULL, &pMsg, 1 );
  return ;  

}


