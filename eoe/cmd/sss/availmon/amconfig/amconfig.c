#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <libgen.h>
#include <fcntl.h>
#include <signal.h>

#include <ssdbapi.h>
#include <ssdberr.h>
#include <amssdb.h>
#include <oamdefs.h>

/* ************************************************** */
#define  MAX_EMAIL_NUM      256

#define  VISUAL                "/usr/bin/vi"

#define  VALUETYPE_UNKNOWN  0
#define  VALUETYPE_BOOL     1
#define  VALUETYPE_INT      2
#define  VALUETYPE_EMAIL    3

#define  DEF_BOOL_VALUE       0
#define  DEF_STATUS_VAL      60
#define  DEF_TICKER_VAL     300
#define  DEF_TICKFILE       "/var/adm/avail/.save/lasttick"
#define  DEF_AVAIL_EMAIL    "availmon@csd.sgi.com"

const char  szAmcAutoEmail  [] = "autoemail";
const char  szAmcShutdown   [] = "shutdownreason";
const char  szAmcTickerd    [] = "tickerd";
const char  szAmcHinv       [] = "hinvupdate";
const char  szAmcStatusInt  [] = "statusinterval";
const char  szAmcTickerInt  [] = "tickduration" ;
const char  szAmcEmailList  [] = "autoemail.list";
const char  szAmcTickFile   [] = "tickfile";

const char *szAmcEmailNames [] = { "AVAILABILITY_CENCR", 
                                   "AVAILABILITY_COMP",
                                   "AVAILABILITY", 
                                   "DIAGNOSTIC_CENCR",
                                   "DIAGNOSTIC_COMP",
                                   "DIAGNOSTIC",
                                   "PAGER" }; 

/* ************************************************** */

char    cmd[MAX_STR_LEN];

char    *tfn = NULL;               /* Temp file name  */

/* SSDB Relarted Variables */
ssdb_Client_Handle     hClient     = NULL;      
ssdb_Connection_Handle hConnection = NULL;      
ssdb_Request_Handle    hRequest    = NULL;
ssdb_Error_Handle      hError      = NULL;


typedef char *emails_t[SEND_TYPE_NO][MAX_EMAIL_NUM];

emails_t emails_old;
emails_t emails_new;

/* ************************************************** */
/*            Local Functions Prototypes              */
/* ************************************************** */

int     InitDB();
int     DoneDB();
void    PrintDBError(const char *location, ssdb_Error_Handle error);

int     GetConfigCount    ( void );
int     GetConfigType     ( const char *ValName );      
int     GetConfigIntVal   ( const char *ValName, int *pVal );   
int     SetConfigIntVal   ( const char *ValName, int value );
int     UpdateIntValue    ( const char * szName, int Value );
int     InsertStrValue    ( const char * szName, const char *szValue );

int     PrintConfigBool   ( const char *ValName );

int     GetEmailsFromFile ( emails_t emails , char *cfile );
int     GetEmailsFromSSDB ( emails_t emails );

int     LoadEmailList     ( char ** pPtr, int nPtr, const char * szListType  );
void    ShowAddresses     ( emails_t emails );
int     EditAddresses     ( emails_t emails );

int     UpdateRegistration( emails_t old, emails_t new );
void    ClearEmailList    ( emails_t emails );

int     RestoreConfigDefaults (void);

/* ************************************************** */

main(int argc, char *argv[])
{
    int         i, c, state, Res=0,type;
    char        line[MAX_LINE_LEN];

    /* init email list  */
    memset ( emails_old, 0, sizeof(emails_old) );
    memset ( emails_new, 0, sizeof(emails_new) );

    strcpy(cmd, basename(argv[0]) );

    if (getuid() != 0) 
    { fprintf( stderr, 
              "Error: %s: one must be root to execute this utility\n", 
               cmd);
      exit (-1);
    }
    
    /* Init SSDB connection */ 
    if ( !InitDB() )
    { /* Nothing can be done without SSDB today */
      exit (-1);
    }

    RestoreConfigDefaults ();

    /* process online options */
    switch (argc) 
    {
    case 1: /* "amconfig" format */
    
        printf("\tAvailMon Flag       State\n"
               "\t=============       =====\n\n");

        Res = PrintConfigBool ( szAmcAutoEmail );
        if ( Res == -1 ) 
             goto Exit;
        
        Res = PrintConfigBool ( szAmcShutdown  );   
        if ( Res == -1 ) 
             goto Exit;
           
        Res = PrintConfigBool ( szAmcTickerd   );
        if ( Res == -1 ) 
             goto Exit;
           
        Res = PrintConfigBool ( szAmcHinv      );    
        if ( Res == -1 ) 
             goto Exit;
        
        Res = GetConfigIntVal ( szAmcStatusInt, &c);
        if ( Res == -1 )  
             goto Exit;
           
        printf("\t%-20s%d (days)\n", szAmcStatusInt, c );
        
        Res = GetConfigIntVal ( szAmcTickerInt, &c );
        if ( Res == -1 )  
             goto Exit;
           
        printf("\t%-20s%d (seconds)\n", szAmcTickerInt, c );

        GetEmailsFromSSDB ( emails_old );
        ShowAddresses     ( emails_old );
        
    break;   /* End of case 1: ( "amconfig" format ) */
    
    
    case 2: /* "amconfig flagname" format */
    
        type = GetConfigType ( argv[1] );
        switch ( type ) 
        {
          case VALUETYPE_BOOL:
             Res = GetConfigIntVal ( argv[1], &c );
             if ( Res != -1 ) 
                  Res = !c;
          break;
       
          case VALUETYPE_INT:  
             Res = GetConfigIntVal ( argv[1], &c );
             if ( Res != -1 ) 
                  Res =  c;
          break;
        
          case VALUETYPE_EMAIL: /* Configuring email list*/
           
             if (getuid() != 0) 
              {
                fprintf( stderr, 
                        "Error: %s: must be superuser to change configuration file\n", 
                         cmd);
                Res = 1;
                goto Exit;
              }
 
             GetEmailsFromSSDB ( emails_new );
             ShowAddresses     ( emails_new );
             
             printf("\nDo you want to modify the addresses? [y/n] (n) ");
             gets(line);
             while (line[0] == 'y' || line[0] == 'Y') 
             {
               if ( EditAddresses ( emails_new ) == -1 )
               {
                  Res =  -1;
                  goto Exit;
               }   
                    
               ShowAddresses      ( emails_new );
               
               printf("\nDo you want to modify the addresses? [y/n] (n) ");
               gets(line);
             }

             if ( GetConfigIntVal ( szAmcAutoEmail, &c ) == -1 ) 
             {
                Res =  -1;
                goto Exit;
             }     
                  
             if ( c ) 
             { /* Emails registration needs to be updated */
             
               /* Load the Old list */             
               GetEmailsFromSSDB  ( emails_old );
               
               /* Update registration */               
               UpdateRegistration ( emails_old, emails_new );                  
             }
             
             /* SSDB Must be Updated */
             UpdateEmails ( emails_new );
             
          break;  /* End of emaillist editing */ 
        
          default :
            goto error;
        }
    break; /* End of case 2 ( "amconfig flagname" format )*/
        
    case 3: /* Set values format */
    
        if (getuid() != 0) 
        {
            fprintf( stderr, 
                    "Error: %s: must be superuser to set configuration flags.\n", 
                     cmd);
            Res = 1;
            goto Exit;
        }
    
        type = GetConfigType ( argv[1] );
        switch ( type ) 
        {
          case VALUETYPE_BOOL:
          
            if ( strcmp(argv[2], "on") == 0 )
                state = 1;
            else if ( strcmp(argv[2], "off") == 0 )
                state = 0;
            else
                goto error;
                
            Res = SetConfigIntVal ( argv[1], state );
            
          break;
          
          case VALUETYPE_INT:  
          
            c = atol( argv[2] );
            if ( c < 0 )
                 goto error;
            
            if ( c == 0 )
               if ( errno != 0 ) 
                    goto error;
                    
            Res = SetConfigIntVal ( argv[1], c );
            
          break;
        
           default:
             goto error;
        }
    break;  /* End of ( Set Values format ) */ 
        
    default:
      goto error;
    } /* end of main options switch */
    
    
  Exit:    

    ClearEmailList ( emails_new );
    ClearEmailList ( emails_old );
  
    DoneDB (); 
    
    return Res;
    
  error:
 
    fprintf(stderr, "Usage: %s [{autoemail|shutdownreason|tickerd|"
                    "hinvupdate} [{on|off}]]\n", cmd);
    fprintf(stderr, "       %s autoemail.list\n", cmd);
    fprintf(stderr, "       %s statusinterval [<days>]\n", cmd);
    fprintf(stderr, "       %s tickduration   [<seconds>]\n", cmd);

    ClearEmailList ( emails_new );
    ClearEmailList ( emails_old );
    
    DoneDB (); 
    
    return 1;
}
/* End of the main program */


/*
 * InitDB
 *   initializes Database Connection & sets up connection
 *
 */

int InitDB(void)
{
    int ErrCode = 0;
    
    /* Initialize SSDB library */
    if ( !ssdbInit()) 
    {
	fprintf ( stderr, 
	         "Error: %s: unable to initialize SSBD library\n", 
	          cmd);
	return (-1);
    }

    /* Create Error stream */
    hError = ssdbCreateErrorHandle();
    if ( hError == NULL ) 
    {
	PrintDBError("CreateErrorHandle", hError);
	ErrCode++;
	goto newend;
    }

    /* Create SSDB Client */
    hClient = ssdbNewClient( hError, SSDBUSER, SSDBPASSWD, 0);
    if ( hClient == NULL ) 
    {
	PrintDBError("NewClient", hError);
	ErrCode++;
	goto newend;
    }

    /* Open  SSDB Connection */
    hConnection = ssdbOpenConnection( hError, hClient, 
                                      SSDBHOST, SSDB, SSDB_CONFLG_RECONNECT | SSDB_CONFLG_REPTCONNECT );
    if ( !hConnection ) 
    {
	PrintDBError("OpenConnection", hError);
	ErrCode++;
	goto newend;
    }

newend:

    if (ErrCode != 0 ) 
    {
        DoneDB ();
        return -1;
    }     
         
    return 1;
}

/*
 * DoneDB  
 *     Closes all database related connections and handles
 */

int DoneDB ()
{
    /* Delete SQL Request if any */
    if ( hRequest != NULL ) 
    {
      if ( ssdbIsRequestValid ( hError, hRequest ))
           ssdbFreeRequest    ( hError, hRequest);     
       hRequest = NULL;
    }

    /* Close Connection if any */      
    if ( hConnection != NULL )
    {
      if ( ssdbIsConnectionValid ( hError, hConnection ))
           ssdbCloseConnection   ( hError, hConnection );
      hConnection = NULL;
    }       

    /* Delete Client if Any */
    if ( hClient ) 
    {  
      if (ssdbIsClientValid ( hError, hClient ))
          ssdbDeleteClient  ( hError, hClient);
      hClient = NULL;
    }
      
    /* Delete Error Handle */
    if ( hError != NULL )
    {
      ssdbDeleteErrorHandle( hError);
      hError = NULL;
    }            
    
    return 1;
}

/*
 * PrintDBError
 *  Prints last DB Error encountered
 */

void PrintDBError(const char *location, ssdb_Error_Handle error )
{
    fprintf ( stderr, 
              "Error in ssdb%s: %s\n", 
              location, 
              ssdbGetLastErrorString(error));
}

/*
 *   Return Number of AVAILMON Records in Database 
 */
 
int GetConfigCount ( void )
{
    int    nRows;
    const char **v;
    char   szSQL[512];
    
    /* Free Prev hRequest if Any */
    if ( hRequest != NULL ) 
    {
      if ( ssdbIsRequestValid ( hError, hRequest ))
           ssdbFreeRequest    ( hError, hRequest );     
       hRequest = NULL;
    }

    snprintf ( szSQL, sizeof(szSQL), 
               "select count(*) from tool where "
               "tool_name = 'AVAILMON'" );
   
    /* Send Request */
    hRequest = ssdbSendRequest( hError, hConnection, 
                                SSDB_REQTYPE_SELECT, szSQL);
    if ( hRequest == NULL ) 
    {
      PrintDBError("SendRequest", hError );
      return -1;
    }
    
    nRows = ssdbGetNumRecords ( hError, hRequest );
    if ( ssdbGetLastErrorCode ( hError ) != SSDBERR_SUCCESS )
    {
      PrintDBError("GetNumRecords", hError);
      return -1;
    }
    
    if ( nRows > 0 )
    {
      v = ssdbGetRow ( hError, hRequest );
      if ( v != NULL && v[0] != NULL ) 
      {
        sscanf ( v[0], "%d", &nRows );
      }   
    } 
    
    ssdbFreeRequest( hError, hRequest);
    hRequest = NULL;

    return nRows;
}


/*
 *  Update Int Value 
 */
 
int UpdateIntValue ( const char * szName, int Value )
{
    int    Res;
    int    oldValue;
    char   szSQL[512];

    /* Update SSDB */
    /* We need to Lock Tool Table First */
    if ( !ssdbLockTable( hError, hConnection, "tool" )) 
    {
       PrintDBError("LockTable", hError );
       return -1;
    } 
  
    Res = GetConfigIntVal ( szName, &oldValue );
    if ( Res == -1 )
    {
         ssdbUnLockTable ( hError, hConnection );
         return -1;
    }     

    /* Free Prev hRequest if Any */
    if ( hRequest != NULL ) 
    {
      if ( ssdbIsRequestValid ( hError, hRequest ))
           ssdbFreeRequest    ( hError, hRequest );     
       hRequest = NULL;
    }
    
    if ( Res == 0 )
    { /* There is no records */    
      snprintf ( szSQL, sizeof(szSQL), 
                 "insert into tool values('%s', '%s', '%d')",
	          TOOLCLASS, szName, Value );

      hRequest = ssdbSendRequest(hError, hConnection, 
                                 SSDB_REQTYPE_INSERT, szSQL );
    } 
    else 
    {  
      snprintf ( szSQL, sizeof(szSQL), 
                 "update tool set option_default = '%d' where "
                 "tool_name = 'AVAILMON' and tool_option = '%s'",
	          Value , szName );

      hRequest = ssdbSendRequest(hError, hConnection, 
                                 SSDB_REQTYPE_UPDATE, szSQL );
    }                          

    if ( hRequest == NULL )
    {
      PrintDBError("SendRequest", hError);
      ssdbUnLockTable ( hError, hConnection );
      return -1;
    }
    
    /* We dont need this request anymore */
    ssdbFreeRequest(hError, hRequest);
    hRequest = NULL;
    
    ssdbUnLockTable ( hError, hConnection );
    return 1;
}    

/*
 *  Insert Str Value 
 */
 
int InsertStrValue ( const char * szName, const char * szValue )
{
    int    Res;
    int    oldValue;
    char   szSQL[512];

    /* Free Prev hRequest if Any */
    if ( hRequest != NULL ) 
    {
      if ( ssdbIsRequestValid ( hError, hRequest ))
           ssdbFreeRequest    ( hError, hRequest );     
       hRequest = NULL;
    }
    
    snprintf ( szSQL, sizeof(szSQL), 
               "insert into tool values('%s', '%s', '%s')",
               TOOLCLASS, szName, szValue );

    hRequest = ssdbSendRequest(hError, hConnection, 
                               SSDB_REQTYPE_INSERT, szSQL );

    if ( hRequest == NULL )
    {
      PrintDBError("SendRequest", hError);
      ssdbUnLockTable ( hError, hConnection );
      return -1;
    }
    
    /* We dont need this request anymore */
    ssdbFreeRequest(hError, hRequest);
    hRequest = NULL;
    
    return 1;
}    


/*
 *  Restore Availmon config to default state 
 */

int RestoreConfigDefaults (void)
{
  if ( GetConfigCount() == 0 )
  {
    UpdateIntValue ( szAmcAutoEmail    ,  DEF_BOOL_VALUE );
    UpdateIntValue ( szAmcStatusInt    ,  DEF_STATUS_VAL ); 
    UpdateIntValue ( szAmcTickerInt    ,  DEF_TICKER_VAL ); 
    UpdateIntValue ( szAmcHinv         ,  DEF_BOOL_VALUE );
    
    InsertStrValue ( szAmcTickFile     ,  DEF_TICKFILE   );
    InsertStrValue ( szAmcEmailNames[3],  DEF_AVAIL_EMAIL); 

    /* Must be executed */    
    SetConfigIntVal ( szAmcTickerd     ,  DEF_BOOL_VALUE );
    SetConfigIntVal ( szAmcShutdown    ,  DEF_BOOL_VALUE );
  }
  
  return 1;  
}

int  GetConfigType ( const char *szVal )
{
    if ( 0 == strcmp ( szVal, szAmcAutoEmail ) || 
         0 == strcmp ( szVal, szAmcShutdown  ) || 
         0 == strcmp ( szVal, szAmcTickerd   ) || 
         0 == strcmp ( szVal, szAmcHinv      ) )
      return VALUETYPE_BOOL;
      
      
    if ( 0 == strcmp ( szVal, szAmcStatusInt ) ||  
         0 == strcmp ( szVal, szAmcTickerInt ) )
      return VALUETYPE_INT ;
      
    
    if ( 0 == strcmp ( szVal, szAmcEmailList ) )
      return VALUETYPE_EMAIL;
      
      
    return VALUETYPE_UNKNOWN;
}


/*
 * check whether config flag is turned on
 */
int GetConfigIntVal ( const char *szConfigVal, int * pVal)
{
    char   szSQL[512];
    int    nRows;
    const  char  ** v;
    
    if ( pVal == NULL ) 
         return -1;

    /* Free Prev hRequest if Any */
    if ( hRequest != NULL ) 
    {
      if ( ssdbIsRequestValid ( hError, hRequest ))
           ssdbFreeRequest    ( hError, hRequest );     
       hRequest = NULL;
    }
    
    /* Set default value for flag */     
    *pVal = 0;

    snprintf ( szSQL, sizeof(szSQL), 
               "select option_default from tool where "
               "tool_name = 'AVAILMON' and tool_option = '%s'",
	        szConfigVal );
   
    /* Send New Request */
    hRequest = ssdbSendRequest( hError, hConnection, 
                                SSDB_REQTYPE_SELECT, szSQL);
    if ( hRequest == NULL ) 
    {
      PrintDBError("SendRequest", hError );
      return -1;
    }
    
    nRows = ssdbGetNumRecords ( hError, hRequest );
    if ( ssdbGetLastErrorCode ( hError ) != SSDBERR_SUCCESS )
    {
      PrintDBError("GetNumRecords", hError);
      return -1;
    }
    
    if ( nRows > 0 )
    {
      v = ssdbGetRow ( hError, hRequest );
      if ( v != NULL && v[0] != NULL ) 
      {
        sscanf ( v[0], "%d", pVal );
      }   
    } 
    
    ssdbFreeRequest( hError, hRequest);
    hRequest = NULL;

    return nRows;
}

int  PrintConfigBool  ( const char *szValName )
{
    int Res; 
    int Val;

    printf("\t%-20s", szValName );

    Res = GetConfigIntVal ( szValName, &Val );
    if ( Res < 0 )
         return -1;
            
    if ( Val )
         printf("on\n");
    else
         printf("off\n");
         
    return 0;
}


int  SetConfigIntVal (const char *szName, int Value )
{
    int    Res;
    int    oldValue;
    char   szSQL[512];
    char   systemcmd[1024];

    /* Get Old value */
    Res = GetConfigIntVal ( szName, &oldValue );
    if ( Res == -1 )
         return -1;

    /* we are going to update deamons first then update SSDB */ 
    
    if ( strcmp(szName, szAmcAutoEmail ) == 0 ) 
    { /* auto email flag */
      /* Detected a change in autoemail from saved one !! Send report */
      
      if ( Value ) 
         snprintf ( systemcmd, sizeof(systemcmd), 
                    "/usr/etc/amformat -A -a -r 2>&1 >/dev/null &");
      else 
         snprintf ( systemcmd, sizeof(systemcmd), 
                    "/usr/etc/amformat -A -a -d 2>&1 >/dev/null &");
         
      system(systemcmd);
    } 
    else if ( strcmp(szName, szAmcShutdown ) == 0 ) 
    {
      snprintf(systemcmd, sizeof(systemcmd), 
               "echo %d > /var/adm/avail/.save/shutdownreason 2>/dev/null" , 
                Value );
                
      system(systemcmd);
    } 
    else if ( strcmp(szName, szAmcTickerd ) == 0 ) 
    {
      snprintf ( systemcmd, sizeof(systemcmd), 
                 "/usr/etc/eventmond -a %s 2>/dev/null",
 	         Value ? "on" : "off" );
      system(systemcmd);
    } 
    else if ( strcmp( szName, szAmcStatusInt ) == 0 ) 
    { /* Status Interval have been changed */
      if ( Value > 0 ) 
      {
         snprintf( systemcmd, sizeof(systemcmd), 
                   "/usr/etc/eventmond -e %d  2>/dev/null",
                    Value * 24 );
         system(systemcmd);
      }
    } 
    else if ( strcmp( szName, szAmcTickerInt ) == 0 ) 
    { /* TickerInterval have been changed */
      if ( Value > 0 ) 
      {
         snprintf( systemcmd, sizeof(systemcmd), 
                   "/usr/etc/eventmond -j %d  2>/dev/null",
                    (Value > 0 ? Value : DEF_BOOL_VALUE ));
         system(systemcmd);
      }
    }
    
    UpdateIntValue ( szName, Value );
    
    return  0;
}


int GetEmailsFromFile ( emails_t emails, char *cfile )
{
    FILE *fp;
    char *ap;
    int   i,type, c, newaddr;
    char  line[MAX_LINE_LEN]; 
    int   idx  [SEND_TYPE_NO];    
    int   len  [SEND_TYPE_NO];    

    if ((fp = fopen(cfile, "r")) == NULL) 
    {
        fprintf(stderr, "Error: %s: cannot open configuration file %s\n",
                cmd, cfile);
        return -1;
    }

    /* free prev */
    for (type = 0; type < SEND_TYPE_NO; type++) 
    {
        len[type] = strlen( sendtypestr[type] );
        idx[type] = 0;
        
        for ( i = 0; i < MAX_EMAIL_NUM; i++ ) 
        {
          if ( emails[type][i] != NULL ) 
          {
             free ( emails[type][i] );
             emails[type][i] = NULL;
          }     
        }
    }

    while (fgets(line, MAX_LINE_LEN, fp)) 
    {
        if (line[0] == '#')
            continue;

        for (type = 0; type < SEND_TYPE_NO; type++)
            if (strncmp(line, sendtypestr[type], len[type]) == 0)
                break;

        if (type == SEND_TYPE_NO)
            continue;

        ap = strtok ( line + len[type], " ,\r\n" );
        while ( ap ) 
        {
          if ( ap[0] != '\r' && ap[0] != '\n' && ap[0] != 0 )
          {
            if ( idx[type] < MAX_EMAIL_NUM )
            {
              emails[type][idx[type]] = strdup ( ap );
              if ( emails[type][idx[type]] == NULL ) 
              {
                fprintf ( stderr, "Error: %s: not enough memory\n", cmd );
                fclose  ( fp );
                return -1;                 
              }
              idx[type] ++;
            }  
          } 
          ap = strtok ( NULL, " ,\r\n" );
        }
    }
    
    fclose(fp);
    
    return   1;
}


int LoadEmailList ( char ** pPtr, int nPtr, const char * szListType  )
{
  int  i, nRows  ;
  int  len, idx  ;
  char szSQL[512];
  char szBuf[512];
  char *pTmp;
  const char ** v;

  if ( pPtr == NULL || nPtr == 0 || szListType == 0 )
       return -1;

  /* Free prev list */
  for ( i = 0; i < nPtr; i++ )
  {
     if ( pPtr[i] != NULL ) 
     {
          free ( pPtr[i] );
          pPtr[i] = NULL;
     }     
  }

  /* Free Prev hRequest if Any */
  if ( hRequest != NULL ) 
  {
    if ( ssdbIsRequestValid ( hError, hRequest ))
         ssdbFreeRequest    ( hError, hRequest );     
     hRequest = NULL;
  }
  
  snprintf ( szSQL, sizeof(szSQL),
             "select option_default from tool "
             "where tool_option = '%s' and tool_name='AVAILMON'",
             szListType );

  hRequest = ssdbSendRequest ( hError, hConnection, 
                               SSDB_REQTYPE_SELECT, szSQL );
  if ( hRequest == NULL )
  {
      PrintDBError("SendRequest", hError);
      return -1;
  }

  nRows = ssdbGetNumRecords ( hError, hRequest );
  if ( ssdbGetLastErrorCode ( hError ) != SSDBERR_SUCCESS )
  {
      PrintDBError("GetNumRecords", hError);
      return -1;
  }

  for ( idx = i = 0; i < nRows ; i++ ) 
  {
     v = ssdbGetRow ( hError, hRequest );
     if ( v != NULL && v[0] != NULL ) 
     { 
         len = strlen(v[0]);
         /* len < then 255 for current DB schema but if ... */
         if ( len > sizeof(szBuf))
              continue; 
         
         strcpy ( szBuf, v[0] );
          
         pTmp = strtok ( szBuf, " ,");
         while ( pTmp )
         {
           if ( pTmp[0] != 0 ) 
           {
            if ( idx < nPtr )
             {
               pPtr[idx] = strdup ( pTmp );
               if ( pPtr[idx] == NULL ) 
               {
                 fprintf ( stderr, "Error: %s: not enough memory\n", cmd );
                 return -1;                 
               }
               idx ++;
               pTmp = strtok ( NULL, " ," );
             }  
           }  
         }
     }   
  } 
    
  ssdbFreeRequest( hError, hRequest);
  hRequest = NULL;
  
  return idx;
}


int GetEmailsFromSSDB ( emails_t emails )
{
    int  type, count;

    for (type = 0; type < SEND_TYPE_NO; type++) 
    {
        count = LoadEmailList ( emails[type], MAX_EMAIL_NUM, szAmcEmailNames[type] );
        if ( count == -1 )
             return -1;
    }
    
    return 1;
}

void PrintfAddrList   ( char * szHeader, char ** pPtr, int nPtr )
{
    int i, comma = 0;

    printf ( szHeader );
    for ( i = 0; i < nPtr; i++ )
    {
      if ( pPtr[i] && pPtr[i][0] != 0 )
      {  
         if ( comma )
              printf ( "," );
            
         printf ( "%s", pPtr[i] );
         comma = 1;
      }
    }
    printf ( "\n" );
}

void    ShowAddresses ( emails_t emails )
{
    int type, i;

    printf("\nThe availability report will be sent to:\n");
    PrintfAddrList   ( "\tin compressed and encrypted form: ", emails[0], MAX_EMAIL_NUM );
    PrintfAddrList   ( "\tin compressed form: "              , emails[1], MAX_EMAIL_NUM );
    PrintfAddrList   ( "\tin plain text form: "              , emails[2], MAX_EMAIL_NUM );

    printf("\nThe diagnosis report will be sent to:\n");
    PrintfAddrList   ( "\tin compressed and encrypted form: ", emails[3], MAX_EMAIL_NUM );
    PrintfAddrList   ( "\tin compressed form: "              , emails[4], MAX_EMAIL_NUM );
    PrintfAddrList   ( "\tin plain text form: "              , emails[5], MAX_EMAIL_NUM );
    
    printf("\nThe pager report will be sent to:\n");
    PrintfAddrList   ( "\tin concise text form: "            , emails[6], MAX_EMAIL_NUM );
}

int  EditAddresses ( emails_t emails )
{
    int         pid, s, t;
    FILE        *fp, *fp1;
    char        *edit = VISUAL;
    void        (*sig)(), (*scont)(), signull();
    int          i, type, res = 1;

    /* Create file to edit */
    if (tfn == NULL) 
    {
        tfn = tempnam(NULL, "avail");
        
        if ( tfn == NULL) 
        {
            fprintf(stderr, "Error: cannot create temp file name\n");
            return -1;
        }
    }   
        
    fp = fopen(tfn, "w");
    if ( fp == NULL) 
    {
         fprintf(stderr, "Error: cannot create temp file\n");
         return -1;
    }
        
    fprintf ( fp, "#\n" );
    fprintf ( fp, "# automatic notification of availability/analysis/registration information\n" );
    fprintf ( fp, "#\n" );
        
    for (type = 0; type < SEND_TYPE_NO; type++) 
    {
      int comma = 0;
    
      fprintf ( fp, "#\n" );
      fprintf ( fp, "%s ", sendtypestr[type] ); 
      
      for ( i = 0; i < MAX_EMAIL_NUM; i++ )
      {
        if ( emails[type][i] && emails[type][i][0] != 0 )
        {  
          if ( comma )
               fprintf ( fp, "," );
             
          fprintf ( fp, emails[type][i] );
          comma = 1;
        }
      }
      fprintf ( fp, "\n" );
    }
        
    fclose(fp);

    /* Edit it */
    sig = sigset(SIGINT, SIG_IGN);

    /* fork an editor on the message */
    pid = fork();
    if (pid == 0) {
        /* CHILD - exec the editor. */
        if (sig != SIG_IGN)
            signal(SIGINT, SIG_DFL);
        execl (edit, edit, tfn, 0);
        perror(edit);
        exit(1);
    }
    
    if (pid == -1) 
    {
        /* ERROR - issue warning and get out. */
        perror("fork");
        res = -1;
        goto out;
    }
    
    /* PARENT - wait for the child (editor) to complete. */
    while ( wait(&s) != pid) ;

    /* Check the results. */
    if ((s & 0377) != 0) 
    {
      fprintf( stderr, "Fatal error in \"%s\"\n", edit );
    } 
    else 
    {
      /* Everything OK now */
      /* Reload Email adresses into array */
      res = GetEmailsFromFile ( emails, tfn );
    }

  out:
  
    /* delete temp file */
    unlink( tfn );
  
    sigset(SIGINT, sig);
    
    return res;
}

int addresscompare(const void *address1, const void *address2)
{
    return(strcmp(*(char **)address1, *(char **)address2));
}

int UpdateRegistration ( emails_t eold, emails_t enew )
{
    int     i, type;
    int     CmdLen,Len;
    
    int     nNew, nOld;
    int     nAdd, nDel;
    int     iNew, iOld;
    
    char   *New[MAX_EMAIL_NUM];
    char   *Old[MAX_EMAIL_NUM];
    char   *Add[MAX_EMAIL_NUM];
    char   *Del[MAX_EMAIL_NUM];
    char    Cmd[MAX_LINE_LEN ];
    
    /* For All except Chatty pager */
    for (type = 0; type < SEND_TYPE_NO - 1; type++) 
    {
   
        /* Sort old and new lists */
        /* We are using the fact that email_t lyst does not 
           have NULL gaps in it see Loading procedures for detals */
        for  ( nNew = 0; nNew < MAX_EMAIL_NUM; nNew++ ) 
        {
           if ( enew[type][nNew] == NULL ) 
                break;
           
           New[nNew] = enew[type][nNew];
        }
        if ( nNew ) 
             qsort ( New, nNew, sizeof(char *), addresscompare );
             
        for  ( nOld = 0; nOld < MAX_EMAIL_NUM; nOld++ ) 
        {
           if ( eold[type][nOld] == NULL ) 
                break;
           
           Old[nOld] = eold[type][nOld];
        }
        if ( nNew ) 
             qsort ( Old, nOld, sizeof(char *), addresscompare );
             
        iNew  = iOld = 0;
        nAdd  = nDel = 0;

        /* 
           We could use the same Arrays to store 
           pointers to the emails to be registered/unregisterd 
           instead if extra ones, but we dont want to
        */      
          
        while ( iNew < nNew  && iOld < nOld ) 
        {
            i = strcmp( New[iNew], Old[iOld] );
            
            if (i == 0) 
            {   /* Just ignore both */
                iNew ++;
                iOld ++;
            } 
            else if (i < 0) 
            {   /* Add new */
                Add[nAdd++] = New[iNew++];
            } 
            else 
            {   /* Remove old */
                Del[nDel++] = Old[iOld++];
            }
        }
        
        /* Append the rest of the */
        while ( iNew < nNew ) 
                Add[nAdd++] = New[iNew++];
                
        while ( iOld < nOld ) 
                Del[nDel++] = Old[iOld++];
                
        /* Do Registration/Unregistration */        
        /* Unregister First */
        if ( nDel )
        {
           int nArgs ;
           
           nArgs = 0;
           
           for ( i = 0 ; i < nDel; )
           {
             Len = strlen ( Del[i] );
             
             if ( nArgs == 0 )
             {
               CmdLen = sprintf ( Cmd, "/usr/etc/amformat -d -E %s:%s", 
                                  szAmcEmailNames[type], Del[i] );
                                  
               if ( CmdLen + Len < MAX_LINE_LEN - 24 )
               { 
                 nArgs = 1;
               } else {
                 /* Othewise The email address is to-o-o-o-o-o 
                  ( ~1000 characters ) long.
                  It is nothing we can do about it Let's just */
                  Cmd[0] = 0;
               }
               i++;
               continue ;
             } 
           
             if ( CmdLen + Len + 1 < MAX_LINE_LEN - 24 )
             {
               sprintf ( Cmd + CmdLen, ",%s", Del[i] );
               CmdLen += Len+1;
               i++;
               nArgs++;
             } 
             else 
             {
               sprintf ( Cmd + CmdLen, " 2>&1 >/dev/null &" );               
               system ( Cmd );
               nArgs = 0;
             }
           } /* End for All Emails */    
           
           if ( nArgs  )
           {
               sprintf ( Cmd + CmdLen, " 2>&1 >/dev/null &" );               
               system ( Cmd );
           }
        } 
        /* End of Deregistration */  
        
        /* Now Registration */
        if ( nAdd )
        {
           int nArgs ;
           
           nArgs = 0;
           
           for ( i = 0 ; i < nAdd; )
           {
             Len = strlen ( Add[i] );
             
             if ( nArgs == 0 )
             {
               CmdLen = sprintf ( Cmd, "/usr/etc/amformat -r -E %s:%s", 
                                  szAmcEmailNames[type], Add[i] );
               if ( CmdLen + Len < MAX_LINE_LEN - 24 )
               { 
                 nArgs = 1;
               } else {
                 /* Othewise The email address is to-o-o-o-o-o 
                   ( ~1000 characters ) long.
                   It is nothing we can do about it Let's just */
                 Cmd[0] = 0;
               }
               i++;
               continue ;
             } 
           
             if ( CmdLen + Len + 1 < MAX_LINE_LEN - 24 )
             {
               sprintf ( Cmd + CmdLen, ",%s", Add[i] );
               CmdLen += Len+1;
               i++;
               nArgs++;
             } 
             else 
             {
               sprintf ( Cmd + CmdLen, " 2>&1 >/dev/null &" );               
               system  ( Cmd );
               nArgs = 0;
             }
           } /* End for All Emails */    
           
           if ( nArgs )
           {
               sprintf ( Cmd + CmdLen, " 2>&1 >/dev/null &" );               
               system  ( Cmd );
           }
        } 
        
    } /* End for all types */
    
    return 1;
}

int  UpdateEmails ( emails_t emails )
{
    int    i, type;
    char   szSQL[512];

    /* Update SSDB */
    /* We need to Lock Tool Table First */
    if ( !ssdbLockTable( hError, hConnection, "tool" )) 
    {
       PrintDBError("LockTable", hError );
       return -1;
    } 

    /* Free Prev hRequest if Any */
    if ( hRequest != NULL ) 
    {
      if ( ssdbIsRequestValid ( hError, hRequest ))
           ssdbFreeRequest    ( hError, hRequest );     
       hRequest = NULL;
    }

    for (type = 0; type < SEND_TYPE_NO; type++) 
    {
      snprintf ( szSQL, sizeof(szSQL), 
                 "delete from tool where "
                 "tool_name = 'AVAILMON' and tool_option = '%s'",
	          szAmcEmailNames[type] );
	          
      hRequest = ssdbSendRequest(hError, hConnection, 
                                 SSDB_REQTYPE_DELETE, szSQL );
      if ( hRequest == NULL ) 
      {
        PrintDBError("SendRequest", hError);
        ssdbUnLockTable ( hError, hConnection );
        return -1;
      }                           

      ssdbFreeRequest(hError, hRequest);
      hRequest = NULL;
    
      for ( i = 0; i < MAX_EMAIL_NUM; i++ ) 
      {
        if ( emails[type][i] == NULL ) 
             break;
      
        snprintf ( szSQL, sizeof(szSQL), 
                   "insert into tool values('AVAILMON', '%s', '%s')",
	            szAmcEmailNames[type], emails[type][i] );
	            
        hRequest = ssdbSendRequest(hError, hConnection, 
                                   SSDB_REQTYPE_INSERT, szSQL );
        if ( hRequest )
        {                            
          ssdbFreeRequest(hError, hRequest);
          hRequest = NULL;
        }  
      }    
   }   
      
   ssdbUnLockTable ( hError, hConnection );
    
   return  0;
}

void ClearEmailList ( emails_t emails )
{
    int i, type;
    
    /* free prev */
    for (type = 0; type < SEND_TYPE_NO; type++) 
    {
        for ( i = 0; i < MAX_EMAIL_NUM; i++ ) 
        {
          if ( emails[type][i] != NULL ) 
          {
             free ( emails[type][i] );
             emails[type][i] = NULL;
          }     
        }
    }
}
