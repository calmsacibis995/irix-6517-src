#ident "$Revision: 1.3 $"
/*--------------------------------------------------------------------*/
/*                                                                    */
/* Copyright 1992-1998 Silicon Graphics, Inc.                         */
/* All Rights Reserved.                                               */
/*                                                                    */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics    */
/* Inc.; the contents of this file may not be disclosed to third      */
/* parties, copied or duplicated in any form, in whole or in part,    */
/* without the prior written permission of Silicon Graphics, Inc.     */
/*                                                                    */
/* RESTRICTED RIGHTS LEGEND:                                          */
/* Use, duplication or disclosure by the Government is subject to     */
/* restrictions as set forth in subdivision (c)(1)(ii) of the Rights  */
/* in Technical Data and Computer Software clause at                  */
/* DFARS 252.227-7013, and/or in similar or successor clauses in      */
/* the FAR, DOD or NASA FAR Supplement.  Unpublished - rights         */
/* reserved under the Copyright Laws of the United States.            */
/*                                                                    */
/*--------------------------------------------------------------------*/

/*--------------------------------------------------------------------*/
/* amconvert.c                                                        */
/*   amconvert is the program which migrates old availmon configurati */
/*   on to new sss configuration. It is normally run at install time  */
/*--------------------------------------------------------------------*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ssdbapi.h>
#include <amssdb.h>
#include <amdefs.h>
#include <oamdefs.h>
#include <amnotify.h>

#define  MAX_EMAIL_NUM          256

typedef char *emails_t[SEND_TYPE_NO][MAX_EMAIL_NUM];

emails_t emails_old;
emails_t emails_new;

ssdb_Client_Handle     hClient     = 0;
ssdb_Connection_Handle hConnection = 0;
ssdb_Request_Handle    hRequest    = 0;
ssdb_Error_Handle      hError      = 0;
int                    bTableLocked= 0;

/*
 * Function Prototypes
 */
 
static void initdb  (void);
static void donedb  (void);

static void MyAtExit();
static void PrintDBErrorAndExit(char *, ssdb_Error_Handle);

static void convertConfig(void);
static void updateConfigFlags(char *, char *);
static int  chkconfig(amconf_t, int);
static int  addresscompare(const void *, const void *);

static void ClearEmailList     ( emails_t emails );
static int  GetEmailsFromFile  ( emails_t emails , char *cfile );
static void UpdateEmails       ( emails_t emails );
static int  UpdateRegistration ( emails_t eold, emails_t enew );


/*
 * initdb
 *   initializes Database Library & sets up connection
 *
 */

void initdb (void)
{
    /*
     * Initialize DB API
     */

    if ( !ssdbInit()) 
    {
        /* It is OK to call PrintDBErrorAndExit with hError == NULL */
	PrintDBErrorAndExit("Init", hError);
    }

    hError = ssdbCreateErrorHandle();
    if ( hError == NULL ) 
    {
	PrintDBErrorAndExit ("CreateErrorHandle", hError );
    }

    if ((hClient = ssdbNewClient(hError, SSDBUSER, SSDBPASSWD, 0)) == NULL ) 
    {
	PrintDBErrorAndExit ("NewClient", hError );
    }

    hConnection = ssdbOpenConnection( hError, hClient, SSDBHOST, SSDB,
                                     SSDB_CONFLG_RECONNECT|SSDB_CONFLG_REPTCONNECT); 
    if ( hConnection == NULL ) 
    {
	PrintDBErrorAndExit ("OpenConnection", hError);
    }
    
    /* We need to Lock Tool Table */
    if ( !ssdbLockTable( hError, hConnection, "tool" )) 
    {
        PrintDBErrorAndExit("LockTable", hError );
        bTableLocked = 1;
    } 
}

/*
 *  Clean up DataBase Related connections
 */

void  donedb (void)
{
   if ( hError )
   { 
     if (hRequest)
         ssdbFreeRequest( hError, hRequest );

     if ( bTableLocked  )
     {
         ssdbUnLockTable ( hError, hConnection );
         bTableLocked = 0;
     }    

     if (ssdbIsConnectionValid ( hError, hConnection ))
     {
         ssdbCloseConnection   ( hError, hConnection );
         hConnection = NULL;
     }    

     if (ssdbIsClientValid ( hError, hClient ))
     {
         ssdbDeleteClient  ( hError, hClient );
         hClient = NULL;
     }    

     ssdbDeleteErrorHandle(hError);
     hError = NULL;
   }     

   ssdbDone();
}


/*
 *  PrintDBErrorAndExit
 *  Prints last DB Error encountered and exit
 */

void PrintDBErrorAndExit(char *location, ssdb_Error_Handle error)
{
    fprintf( stderr, "Error in ssdb%s: %s\n", location, 
		      ssdbGetLastErrorString(error));
    exit(-1);		      
}

/*
 * convertConfig
 *   Reads old availmon configuration which already exists on a system
 *   and inserts it into the database
 */

void convertConfig(void)
{ 
    amconf_t  i;
    int       k;
    int       newautoemail;
    int       oldautoemail; 
    int       oldConfig   ;    
    char      word[10];

    /*
     * lets first check autoemail flag value.  There is a possibility that
     * someone has changed it manually during the time machine was up
     * last time.  We check whether the value in config directory is
     * the same as the value in .save directory.  If they both are different,
     * then we need to either register or de-register accordingly.
     */
     
    /* retrieve autoemail flag from config directory */     
    newautoemail = chkconfig(amc_autoemail, 0); 
    
    /* retrieve autoemail flag from .save  directory */     
    oldautoemail = chkconfig(amc_autoemail, 1); 

    /* for all flags */
    for ( i = amc_autoemail ; i <= amc_statusinterval; i++ ) 
    {
	snprintf(word, 10, "%d", chkconfig(i, 0));
	updateConfigFlags(amcs[i].flagname, word);
    }

    /*
     * Update tickfile and tickduration values
     */

    updateConfigFlags ("tickfile", "/var/adm/avail/.save/lasttick");
    updateConfigFlags ("tickduration", "300");

    /*
     * Update email adresses 
     */

    /* Read email list from config directory if exist */
    if ( GetEmailsFromFile ( emails_new,  amcs[amc_autoemaillist].filename ) < 0 )
    {  /* Loading Failed */
       exit (-1);
    }
    
    /* Read email list from .save directory  if exist */
    /* If this directory does not exist this list wil be empry */
    oldConfig = GetEmailsFromFile ( emails_old, samcs[amc_autoemaillist].filename );   
    if ( oldConfig < 0 )
    {  /* Loading Failed */
       exit (-1);
    }
    
    /*
     * Insert addresses found in config/autoemail.list directory
     * into SSDB.  At this point, addr will contain config/autoemail.list
     * entries.  We will go ahead and insert these into ESP database
     */
    UpdateEmails ( emails_new );
    
    /* We are not going to use SSDB anymore so we can unlock table 
     * close  connection, etc 
     */ 
    donedb ();
    
    /* Do Registration/Deregistration if needed */

    if ( oldConfig ) 
    {   /* we have old configuration in .save directory */
        if ( oldautoemail != newautoemail ) 
        {  
           if ( newautoemail == 0 )
           { /* autoemail flag is "off"   
              * we need deregister old emails 
              */
             ClearEmailList     ( emails_new );
             UpdateRegistration ( emails_old, emails_new );
           } else {
             /* the autoemail flag now "on"
              * we need to register all new emails 
              */ 
             ClearEmailList     ( emails_old );
             UpdateRegistration ( emails_old, emails_new );
           }
        } else {
           /* flags are in the same state
            * we need to update registration if newautoemail "on"
            */
           if ( newautoemail )
                UpdateRegistration ( emails_old, emails_new );
        }
    } else {
        /* There is no previous configuration */
        if ( newautoemail ) 
        {  /* we need to register all new emails */
           ClearEmailList     ( emails_old );
           UpdateRegistration ( emails_old, emails_new );
        }
    }
}

/*
 * updateConfigFlags
 *    Updates Configuration Flag in the Database.  Inserts if not exists.
 */

void updateConfigFlags(char *configFlag, char *configValue)
{
    char   sqlstmt[1024];

    snprintf(sqlstmt, sizeof(sqlstmt), 
	     "DELETE from %s where %s = '%s' and %s = '%s'",
              TOOLTABLE, TOOLNAME, TOOLCLASS, TOOLOPTION, configFlag);

    hRequest = ssdbSendRequest(hError, hConnection, 
			       SSDB_REQTYPE_DELETE, sqlstmt);
    if ( hRequest == NULL ) 
    {
      PrintDBErrorAndExit("SendRequest", hError);
    }
    ssdbFreeRequest(hError, hRequest);
    hRequest = NULL;
    
    snprintf( sqlstmt, sizeof(sqlstmt), 
             "INSERT into %s values('%s', '%s', '%s')",
	      TOOLTABLE, TOOLCLASS, configFlag, configValue);

    hRequest = ssdbSendRequest( hError, hConnection, 
                                SSDB_REQTYPE_INSERT, sqlstmt);
    if ( hRequest == NULL )
    {
      PrintDBErrorAndExit("SendRequest", hError);
    }  
    ssdbFreeRequest(hError, hRequest);
    hRequest = NULL;
}

/*
 * chkconfig
 *    A simple utility to check if a flag is on or off
 */

int
chkconfig(amconf_t flag, int sflag)
{
    amc_t       *amc;
    char        line[MAX_LINE_LEN];
    FILE        *fp;
    int         confflag = 0;

    if (sflag)
	amc = samcs;
    else
	amc = amcs;

    if ((fp = fopen(amc[flag].filename, "r")) == NULL) 
    {
        /* if file is missing it means that flag is off or default */    
	if ( flag == amc_statusinterval ) 
	    return 60;
	return 0; 
    }

    /*
     * check "on" or "off".
     */

    if ( fgets(line, MAX_LINE_LEN, fp) == NULL ) {
	confflag = 0;
    } else if ( flag == amc_statusinterval ) {
	confflag = atoi(line);
    } else if ( strncasecmp(line, "on", 2) != 0 ) {
	confflag = 0;
    } else 
	confflag = 1;

    fclose(fp);
    return(confflag);
}

/*
 * addresscompare
 *    a function used by qsort to compare two e-mail addresses
 */

int
addresscompare(const void *address1, const void *address2)
{
    return(strcmp(*(char **)address1, *(char **)address2));
}

/*
 * main function
 *  takes the following arguments
 */

int main(int argc, char **argv)
{
    if (getuid() != 0) 
    {
        fprintf( stderr, 
                 "Error: must be superuser to run amconvert\n" );
        return 0;         
    }

    /* Initialize pointer arrays  */
    memset ( emails_new, 0, sizeof(emails_new));     
    memset ( emails_old, 0, sizeof(emails_old));     

    /*
     * Register atexit function
     */
    if ( atexit(MyAtExit) != 0 )
    {
       perror ("atexit call failed");      
       return -1;
    }

    /*
     * Initialize SSDB Connection.
     */
    initdb();
	
    /*
     * convert configuration
     */
    convertConfig();

    /* Normal termination */
    exit(0);
}


void MyAtExit()
{
   /* Clean up time */
   ClearEmailList ( emails_new );
   ClearEmailList ( emails_old );

   donedb ();
}

/*
 * Read Emails From file
 * Returns values:  0 if file does not exist
 *                  1 on succees
 *                 -1 if error
 *  
 */

int GetEmailsFromFile ( emails_t emails, char *cfile )
{
    FILE *fp;
    char *ap;
    int   i,type, c, newaddr;
    char  line[MAX_LINE_LEN]; 
    int   idx  [SEND_TYPE_NO];    
    int   len  [SEND_TYPE_NO];    

    /* free previous content if any */
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
    
    if ( access ( cfile, F_OK ) != 0 )
    { /* file does not exist */
      /* do nothing */ 
      return 0;
    }

    if ((fp = fopen(cfile, "r")) == NULL) 
    {
        fprintf(stderr, 
                "Error: amconvert: cannot open configuration file %s\n",
                cfile);
        return -1;
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
                fprintf ( stderr, "Error: amconvert: not enough memory\n" );
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
        /* We are using the fact that email_t list does not 
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
        if ( nOld ) 
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
               CmdLen = sprintf ( Cmd, "/usr/etc/amformat -A -d -E %s:%s", 
                                  repTypestr[type], Del[i] );
                                  
               if ( CmdLen + Len < MAX_LINE_LEN - 24 )
               { 
                 nArgs = 1;
               } else {
                 /* Otherwise The email address is to-o-o-o-o-o 
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
               system  ( Cmd );
               nArgs = 0;
             }
           } /* End for All Emails */    
           
           if ( nArgs  )
           {
               sprintf ( Cmd + CmdLen, " 2>&1 >/dev/null &" );               
               system  ( Cmd );
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
               CmdLen = sprintf ( Cmd, "/usr/etc/amformat -A -r -E %s:%s", 
                                  repTypestr[type], Add[i] );
               if ( CmdLen + Len < MAX_LINE_LEN - 24 )
               { 
                 nArgs = 1;
               } else {
                 /* Otherwise The email address is to-o-o-o-o-o 
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

void  UpdateEmails ( emails_t emails )
{
    int    i, type;
    char   szSQL[512];

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
	          repTypestr[type] );
	          
      hRequest = ssdbSendRequest(hError, hConnection, 
                                 SSDB_REQTYPE_DELETE, szSQL );
      if ( hRequest == NULL ) 
      {
        PrintDBErrorAndExit("SendRequest", hError);
      }                           
      ssdbFreeRequest(hError, hRequest);
      hRequest = NULL;
    
      for ( i = 0; i < MAX_EMAIL_NUM; i++ ) 
      {
        if ( emails[type][i] == NULL ) 
             break;
        
        snprintf ( szSQL, sizeof(szSQL), 
                   "insert into tool values('AVAILMON', '%s', '%s')",
	            repTypestr[type], emails[type][i] );
	            
        hRequest = ssdbSendRequest(hError, hConnection, 
                                   SSDB_REQTYPE_INSERT, szSQL );
        if ( hRequest == NULL ) 
        {
          PrintDBErrorAndExit("SendRequest", hError);
        }                           
        ssdbFreeRequest(hError, hRequest);
        hRequest = NULL;
      }    
   }
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
