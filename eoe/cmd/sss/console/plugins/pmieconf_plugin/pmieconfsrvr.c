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

#ident           "$Revision: 1.4 $"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <libgen.h>
#include <string.h>
#include <syslog.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/param.h>
#include <sys/stat.h>
#include "rgPluginAPI.h"
#include "sscHTMLGen.h"
#include "sscPair.h"
#include "sscShared.h"

#define MYVERSION_MAJOR                  1
#define MYVERSION_MINOR                  0
#define MAXKEYS                          512


typedef struct sss_s_MYSESSION {
  unsigned long signature;
  struct sss_s_MYSESSION *next;
  int textonly;
} mySession;

static const char myLogo[]               = "ESP System Monitor Setup Server";
static const char szVersion[]            = "Version";
static const char szTitle[]              = "Title";
static const char szThreadSafe[]         = "ThreadSafe";
static const char szUnloadable[]         = "Unloadable";
static const char szUnloadTime[]         = "UnloadTime";
static const char szAcceptRawCmdString[] = "AcceptRawCmdString";
static const char szUserAgent[]          = "User-Agent";
static char szServerNameErrorPrefix[]    = "ESP System Monitor Setup Server Error: %s";
static const char szUnloadTimeValue[]    = "50";
static const char szThreadSafeValue[]    = "0";
static const char szUnloadableValue[]    = "1";
static const char szAcceptRawCmdStringValue[] = "1";
static char szVersionStr[64];
static pthread_mutex_t seshFreeListmutex;
static int volatile mutex_inited         = 0;
static mySession *sesFreeList            = 0;

#define PMIECONF			 "/usr/sbin/pmieconf"
#define NAWK				 "/usr/bin/nawk"
#define CHKCONFIG                        "/sbin/chkconfig"
#define INITPMIE                         "/etc/init.d/pmie"
#define CHKCONFIG_PMIE                   "pmie"

#define DISPLAYONLY                      1
#define DISPLAYCONFIRMATION              2

#define INVALIDARGUMENT			 300
#define FILENOTFOUND			 301
#define NOTANEXECUTABLE			 302
#define PMIE_GLOBAL_NOTFOUND             303

#ifdef __TIME__
#ifdef __DATE__
#define INCLUDE_TIME_DATE_STRINGS 1
#endif
#endif

/*---------------------------------------------------------------------------*/
/* rgpgInit                                                                  */
/*   Report Generator Initialization Routine                                 */
/*---------------------------------------------------------------------------*/

int RGPGAPI rgpgInit(sscErrorHandle hError)
{
    if(pthread_mutex_init(&seshFreeListmutex,0)) {
        sscError(hError,szServerNameErrorPrefix,"Can't initialize mutex");
        return 0;
    }
#ifdef INCLUDE_TIME_DATE_STRINGS
    snprintf(szVersionStr,sizeof(szVersionStr),"%d.%d %s %s",MYVERSION_MAJOR,MYVERSION_MINOR,__DATE__,__TIME__);
#else
    snprintf(szVersionStr,sizeof(szVersionStr),"%d.%d",MYVERSION_MAJOR,MYVERSION_MINOR);
#endif
    return 1;
}

/*---------------------------------------------------------------------------*/
/* rgpgDone                                                                  */
/*   Report Generator Exit Routine                                           */
/*---------------------------------------------------------------------------*/

int RGPGAPI rgpgDone(sscErrorHandle hError)
{
    mySession *s;
    FILE      *fp = NULL;
    if(mutex_inited)
    {
        pthread_mutex_destroy(&seshFreeListmutex);
        mutex_inited = 0;
        while((s = sesFreeList) != 0)
        {
            sesFreeList = s->next;
            free(s);
        }
        return 1; /* success */
    }
  return 0; /* error - not inited */
}

/*---------------------------------------------------------------------------*/
/* rgpgCreateSesion                                                          */
/*   Report Generator Session Creation Routine                               */
/*---------------------------------------------------------------------------*/

rgpgSessionID RGPGAPI rgpgCreateSesion(sscErrorHandle hError)
{  mySession *s;
   pthread_mutex_lock(&seshFreeListmutex);
   if((s = sesFreeList) != 0) sesFreeList = s->next;
   pthread_mutex_unlock(&seshFreeListmutex);
   if(!s) s = (mySession*)malloc(sizeof(mySession));
   if(!s)
   { sscError(hError,szServerNameErrorPrefix,"No memory to create session in rgpgCreateSesion()");     return 0;
   }
   memset(s,0,sizeof(mySession));
   s->signature = sizeof(mySession);
   return (rgpgSessionID)s;
}

/*---------------------------------------------------------------------------*/
/* rgpgDeleteSesion                                                          */
/*   Report Generator Session Destroyer routine                              */
/*---------------------------------------------------------------------------*/

void RGPGAPI rgpgDeleteSesion(sscErrorHandle hError,rgpgSessionID _s)
{ mySession *s = (mySession *)_s;
  if(!s || s->signature != sizeof(mySession))
  { sscError(hError,szServerNameErrorPrefix,"Incorrect session id in rgpgDeleteSesion()");
  }
  else
  { pthread_mutex_lock(&seshFreeListmutex);
    s->next = sesFreeList; sesFreeList = s; s->signature = 0;
    pthread_mutex_unlock(&seshFreeListmutex);
  }
}

/*---------------------------------------------------------------------------*/
/* rgpgGetAttribute                                                          */
/*   Report Generator Attribute Retriever                                    */
/*---------------------------------------------------------------------------*/

char *RGPGAPI rgpgGetAttribute(sscErrorHandle hError,rgpgSessionID session, const char *attributeID, const char *extraAttrSpec,int *typeattr)
{  char *s;

   if(typeattr) *typeattr = RGATTRTYPE_STATIC;
        if(!strcasecmp(attributeID,szVersion))            s = (char*)szVersionStr;
   else if(!strcasecmp(attributeID,szTitle))              s = (char*)myLogo;
   else if(!strcasecmp(attributeID,szUnloadTime))         s = (char*)szUnloadTimeValue;
   else if(!strcasecmp(attributeID,szThreadSafe))         s = (char*)szThreadSafeValue;
   else if(!strcasecmp(attributeID,szUnloadable))         s = (char*)szUnloadableValue;
   else if(!strcasecmp(attributeID,szAcceptRawCmdString)) s = (char*)szAcceptRawCmdStringValue;
   else
   { sscError(hError,"%s No such attribute '%s' in rgpgGetAttribute()",szServerNameErrorPrefix,attributeID);
     return 0;
   }
   if(!s) sscError(hError,szServerNameErrorPrefix,"No memory in rgpgGetAttribute()");
   return s;
}

/*---------------------------------------------------------------------------*/
/* rgpgFreeAttributeString                                                   */
/*---------------------------------------------------------------------------*/

void RGPGAPI rgpgFreeAttributeString(sscErrorHandle hError,rgpgSessionID session,const char *attributeID,const char *extraAttrSpec,char *attrString,int restype)
{  if(((restype == RGATTRTYPE_SPECIALALLOC) || (restype == RGATTRTYPE_MALLOCED)) && attrString) free(attrString);
}

/*---------------------------------------------------------------------------*/
/* rgpgSetAttribute                                                          */
/*---------------------------------------------------------------------------*/

void RGPGAPI rgpgSetAttribute(sscErrorHandle hError, rgpgSessionID session, const char *attributeID, const char *extraAttrSpec, const char *value)
{  mySession *sess = (mySession *)session;

   if(!strcasecmp(attributeID,szVersion) || !strcasecmp(attributeID,szTitle) ||
      !strcasecmp(attributeID,szThreadSafe) || !strcasecmp(attributeID,szUnloadable) ||
      !strcasecmp(attributeID,szUnloadTime))
   { sscError(hError,szServerNameErrorPrefix,"Attempt to set read only attribute in rgpgSetAttribute()");
     return;
   }

   if(!strcasecmp(attributeID,szUserAgent) && value)
   { sess->textonly = (strcasecmp(value,"Lynx") == 0) ? 1 : 0;
   }

   if(!sess || sess->signature != sizeof(mySession))
   { sscError(hError,szServerNameErrorPrefix,"Incorrect session id in rgpgGetAttribute()");
     return;
   }
}


static void generate_error_page (mySession *session, int err)
{
    Body("<html>\n");
    Body("<head>\n");
    Body("<title>\n");
    Body("SGI Embedded Support Partner - Ver 1.0\n");
    Body("</title>\n");
    Body("</head>\n");
    Body("<body bgcolor=\"#ffffcc\">\n");

    if ( session->textonly == 0 )
    {
	    TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
	    RowBegin("");
	       CellBegin("bgcolor=\"#cccc99\" width=15");
	       Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
	       CellEnd();
	       CellBegin("bgcolor=\"#cccc99\"");
	       Body("SETUP &gt; Performance Monitoring &gt; Error\n");
	       CellEnd();
	    RowEnd();
	    
	    RowBegin("");
	       CellBegin("colspan=2");
	       Body("&nbsp;<p>&nbsp;\n");
	       CellEnd();
	    RowEnd();
	    
	    RowBegin("");
            CellBegin("");
            CellEnd();
            CellBegin("");
    }
    else
    {
            Body("<pre>   SETUP &gt; Performance Monitoring &gt; Error</pre>\n");
    }

    switch(err)
    {
       case INVALIDARGUMENT:
	   FormatedBody ( "Invalid arguments specified." );
	   break;
	   
	   case FILENOTFOUND:
	   FormatedBody ( "Required executable files are missing. " 
	                  "Check whether %s and %s are present and have "
	                  "execute permissions set.", PMIECONF, NAWK );
	   break;
	   
	   case NOTANEXECUTABLE:
	   FormatedBody ( "Invalid file(s). Check whether %s and %s are "
	                  "present and have execute permissions set.", PMIECONF, NAWK );
	   break;
	   
       default:
	   FormatedBody ( "Operating System Error: %s", strerror(err));
	   break;
    }
    
    if ( session->textonly == 0 )
    {
            CellEnd();
	    RowEnd();
	    TableEnd();
    }  
    Body("</body></html>\n");
}

static int report_error( sscErrorHandle hError, int err)
{
    switch(err)
    {
       case INVALIDARGUMENT:
	   sscError ( hError, "Invalid arguments specified." );
	   break;
	   
       case PMIE_GLOBAL_NOTFOUND:
	   sscError ( hError, "Automated performance monitoring status was not found. " 
	                      "Run %s to check whether pmie flag is present.", CHKCONFIG );
	   break;
	   
       case FILENOTFOUND:
	   sscError ( hError, "Required executable files are missing. " 
	                      "Check whether %s and %s are present and have "
	                      "execute permissions set.", PMIECONF, NAWK );
	   break;
	   
       case NOTANEXECUTABLE:
	   sscError ( hError, "Invalid file(s). Check whether %s and %s are "
	                      "present and have execute permissions set.", PMIECONF, NAWK );
	   break;
	   
       default:
	   sscError ( hError, "Operating System Error: %s", strerror(err));
	   break;
    }
    return(RGPERR_SUCCESS);
}

static int getPMIEGlobal ( sscErrorHandle hError )
{
    FILE   *fp = NULL;
    char    cmd [128];
    char    buf [256];
    char    key [256];
    char    val [256];
    int     res  = -1;

    /* Retrieve pmie global status */
    snprintf(cmd, sizeof(cmd), "%s  | grep -i \"%s\"", CHKCONFIG, CHKCONFIG_PMIE );
    if ( (fp = popen(cmd, "r")) != NULL ) 
    {
      if ( fgets(buf, sizeof(buf), fp) != NULL ) 
      { 
        if ( 2 == sscanf ( buf, "%s %s", key, val ))
        {
          if ( strcasecmp ( key, CHKCONFIG_PMIE ) == 0)
          {
             if ( strcasecmp ( val, "on" ) == 0)
               res = 1;
             else 
               res = 0;
          }
        }
      } 
      pclose ( fp );
    }
   
    return res; 
} 

static int getPMIEConf  (sscErrorHandle hError, mySession *session, int display)
{
    FILE    *fp;
    char    cmd[1024];
    char    buffer[1024];
    char    *param;
    int     enabled = 0;
    int     i = 0;
    int     pmie_global = 0;
    
    pmie_global = getPMIEGlobal ( hError );
    if ( pmie_global < 0 )
         return (PMIE_GLOBAL_NOTFOUND);

    snprintf(cmd, sizeof(cmd), 
	     "%s list all enabled 2>/dev/null | %s '\n"
	     "/  rule: /    { $1 = \"\"; rule=$0; next }\n"
	     "/enabled = /  { printf \"%%s %%s\\n\", rule, $3; next }'\n",
	     PMIECONF, NAWK);

    if ( (fp = popen(cmd, "r")) != NULL ) 
    {
        if ( display == DISPLAYCONFIRMATION ) 
        {
	   Body("<html>\n");
	   Body("<head>\n");
	   Body("<title>\n");
	   Body("SGI Embedded Support Partner - Ver 1.0\n");
	   Body("</title>\n");
	   Body("</head>\n");
	   Body("<body bgcolor=\"#ffffcc\">\n");
           if ( session->textonly == 0 )
           {
	    TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
	    RowBegin("");
	       CellBegin("bgcolor=\"#cccc99\" width=15");
	       Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
	       CellEnd();
	       CellBegin("bgcolor=\"#cccc99\"");
	       Body("SETUP &gt; Performance Monitoring &gt; Configuration\n");
	       CellEnd();
	    RowEnd();

	    RowBegin("");
	       CellBegin("colspan=2");
	       Body("&nbsp;<p>&nbsp;\n");
	       CellEnd();
	    RowEnd();

	    RowBegin("");
            CellBegin("");
            CellEnd();
            CellBegin("");
           } else {
            Body("<pre>   SETUP &gt; Performance Monitoring &gt; Configuration</pre>\n");
            Body("<hr width = 100%>\n");
           }
        }	   
	
        while ( fgets(buffer, sizeof(buffer), fp) != NULL ) 
	{
	    param = strtok(buffer, " ");
	    if ( param != NULL ) 
	    {
	      
              if ( i == 0 )  
              { /* We have Some Rules Here, So Let's Generate The second Table Header if neded */
        	 if ( session->textonly == 0 )
        	 {
                   Body("<font face=\"Arial,Helvetica\">Automated performance monitoring:\n");
	           if ( display == DISPLAYONLY || display == DISPLAYCONFIRMATION ) 
	           {/* Only Status Must be displayed */
                     FormatedBody ( "&nbsp;<b>%s</b>", pmie_global ?  "<font color=\"#333300\">Enabled </font>" : 
                                                                      "<font color=\"#cc6633\">Disabled</font>" );
                   }
                   else
                   {/* Radio Buttons Must be displayed */
                    Body(" &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;\n");
                    FormatedBody ( "&nbsp;&nbsp;&nbsp;<input type=\"radio\" name=\"pmie_global\" value=\"1\" %s> Enabled ", pmie_global ? "checked" : "" );
                    FormatedBody ( "&nbsp;&nbsp;&nbsp;<input type=\"radio\" name=\"pmie_global\" value=\"0\" %s> Disabled", pmie_global ? "" : "checked" );
                   }
                                                                      
                   Body("<p>Automated performance monitoring must be enabled for the enabled performance rules to take effect. \n");
                   Body("</font>    \n");
	           CellEnd();
	           RowEnd();
                   /* End of This Row */

                   /* Skip one row    */
	           RowBegin("");
	           CellBegin("");
	           CellEnd();
	           
	           CellBegin("");
	           Body("<p>&nbsp;<hr><p>&nbsp;\n");
	           CellEnd();
	           RowEnd();
	           
	           /* */
	           RowBegin("");
                   CellBegin("");
                   CellEnd();
                   CellBegin("");

                   if ( display == DISPLAYONLY || display == DISPLAYCONFIRMATION )
                   {
                    Body("Current status of automated PMIE monitoring rules:<p>\n");
                   }else{
                    Body("Enable or disable automated PMIE monitoring for individual performance rules:<p>\n");
                   }
        	 
		   TableBegin("border=4 cellpadding=6 cellspacing=1 width=90%");
		   RowBegin("");
		   Body("<th> No. </th>\n");
		   Body("<th> PMIE Rule Description </th>\n");
        	   if ( display == DISPLAYONLY || display == DISPLAYCONFIRMATION ) 
        	   {
	        	 Body("<th> PMIE Rule </th>\n");
	        	 Body("<th> Status </th>\n");
		   } else {
		 	 Body("<th> Enabled </th>\n");
	         	 Body("<th> Disabled </th>\n");
		   }
		   RowEnd();
		 }
		 else
		 {
                   Body("Automated performance monitoring:\n");
                   
	           if ( display == DISPLAYONLY || display == DISPLAYCONFIRMATION ) 
	           {/* Only Status Must be displayed */
                     FormatedBody ( "&nbsp;<b>%s</b>", pmie_global ?  "Enabled " : "Disabled" );
                   }
                   else
                   {/* Radio Buttons Must be displayed */
                    Body(" &nbsp;&nbsp;&nbsp;\n");
                    FormatedBody ( "&nbsp;<input type=\"radio\" name=\"pmie_global\" value=\"1\" %s> Enabled ", pmie_global ? "checked" : "" );
                    FormatedBody ( "&nbsp;<input type=\"radio\" name=\"pmie_global\" value=\"0\" %s> Disabled", pmie_global ? "" : "checked" );
                   }
                                                                      
                   Body("<p>Automated performance monitoring must be enabled for the enabled performance rules to take effect. \n");
                   Body("<hr width=100%>\n");
                   /* End of This Row */

                   /* Skip one row    */
	           /* */
                   if ( display == DISPLAYONLY || display == DISPLAYCONFIRMATION )
                   {
                    Body("<p>Current status of automated PMIE monitoring rules:<p>\n");
                   }else{
                    Body("<p>Enable or disable automated PMIE monitoring for individual performance rules:<p>\n");
                   }
		 
		   FormatedBody ("<pre>-------------------------------------------------------------------------------</pre>");
		   FormatedBody ("<pre>%-43.43s|", " PMIE Rule Description" );
		   if ( display == DISPLAYONLY || display == DISPLAYCONFIRMATION )
		   {
		     FormatedBody ("%-25.25s|", " PMIE Rule" );
		     FormatedBody ("%-8.8s",    " Status"    );
		   }   
		   FormatedBody ("</pre>");
		   FormatedBody ("<pre>-------------------------------------------------------------------------------</pre>");
		 }
              } /* End of if ( i == 0 ) */
	    
	    
	      if ( session->textonly == 0)
	      {
	      RowBegin("");
		CellBegin("");
		FormatedBody("%d",++i);
		CellEnd();
		CellBegin("");
		FormatedBody("%s",strtok(NULL,"[]"));
		CellEnd();

		if ( !strcasecmp(strtok(NULL, " \n"), "yes") ) 
		{
		    enabled = 1;
		} else {
		    enabled = 0;
		}

		if ( display == DISPLAYONLY || display == DISPLAYCONFIRMATION ) 
		{
		    CellBegin("");
		    FormatedBody("%s",param);
		    CellEnd();
		    CellBegin("");
		    FormatedBody("<font color=%s><b>%s</b></font>",(enabled==0?"#cc6633":"#333300"),(enabled==0?"Disabled":"Enabled"));
		    CellEnd();
		} else {
		    CellBegin("align=center");
		    FormatedBody("<input type=\"radio\" name=\"%s\" value=\"1 %d\" %s",param,(enabled==1?1:0),(enabled==1?"checked>":">"));
		    CellEnd();
		    CellBegin("align=center");
		    FormatedBody("<input type=\"radio\" name=\"%s\" value=\"0 %d\" %s",param,(enabled==1?1:0),(enabled==0?"checked>":">"));
		    CellEnd();

		}
	      RowEnd();
	      }
	      else
	      { /* Lynx Stuff */
		++i;
		FormatedBody("<pre>%-43.43s|",strtok(NULL,"[]"));

		if ( !strcasecmp(strtok(NULL, " \n"), "yes") ) 
		{
		  enabled = 1;
		} else {
		  enabled = 0;
		}

		if ( display == DISPLAYONLY || display == DISPLAYCONFIRMATION ) 
		{
		    FormatedBody ( "%-25.25s|", param );
		    FormatedBody ( " %-8.8s<pre>", (enabled==0 ? "Disabled" : "Enabled" ));
		} else {
		    FormatedBody("<input type=\"radio\" name=\"%s\" value=\"1 %d\" %s Enabled",param,(enabled==1?1:0),(enabled==1?"checked>":">"));
		    FormatedBody("<input type=\"radio\" name=\"%s\" value=\"0 %d\" %s Disabled",param,(enabled==1?1:0),(enabled==0?"checked>":">"));
		    FormatedBody("</pre>");
		}
	      } /* End for Lynx */
	   } /* End Param != NULL */
	}
        
        if ( i == 0 )  
        {
          if ( session->textonly == 0 )
          { /* This is the continuation of the first Table */
	    RowBegin("");
	    CellBegin("colspan=4 align=center");
	    Body("<font color=red><i>No PMIE Rules found !!! Check if PCP is properly installed</i></font>\n");
	    CellEnd();
	    RowEnd();
	  } else {
	    RowBegin("");
	    Body("<p>No PMIE Rules found !!! Check if PCP is properly installed\n");
	  }  
	} 
	else 
	{
          /* This is the closing of the second table*/
          if ( session->textonly == 0 )
          {
	  TableEnd();
	  }
	  else
	  {
	   FormatedBody ("<pre>-------------------------------------------------------------------------------</pre>");
	  } 
	}  

        if ( display == 0 )
        {
  	  /* Add Accept Button if needed */
	  if ( i != 0 )
          {
            Body("<p><INPUT TYPE=\"SUBMIT\" VALUE=\"   Accept   \">&nbsp;&nbsp;&nbsp;<input type=\"reset\" value=\"   Clear   \">\n");
          }	
        } else { 
          /* Just close Global(First) Table */
	  if ( display == DISPLAYCONFIRMATION ) 
  	  {
            if ( session->textonly == 0 )
            {
	      CellEnd();
	      RowEnd();
	      TableEnd();
	    }  
            Body("</body></html>\n");
	  }
        }  
        
        pclose ( fp );
    } 
    else 
    {
        return(errno);
    }
    return(0);
}

static int CheckExecutables(char *file)
{
    struct stat    statbuf;

    if ( !file ) 
          return(INVALIDARGUMENT);

    if ( stat(file, &statbuf) < 0 ) {
	 return(FILENOTFOUND);
    } else if ( !(statbuf.st_mode & (S_IFREG|S_IEXEC))) {
	 return(NOTANEXECUTABLE);
    }

    return(0);
}

int setPMIEConf(CMDPAIR *cmdp, int cmdpsize)
{
    int err = 0;
    int i = 0;
    int oldvalue = 0;
    int newvalue = 0;
    int pmieon   = 0;
    char cmd[256];

    for(i = 0; i < cmdpsize; i++) 
    {
      if ( strcasecmp ( cmdp[i].keyname, "pmie_global"  ) == 0 ) 
      {
        pmieon = 0;
        sscanf( cmdp[i].value, "%d", &pmieon );
        
        if ( pmieon )
           snprintf(cmd, sizeof(cmd), "%s pmie on; %s start >/dev/null 2>&1", CHKCONFIG, INITPMIE);
        else
           snprintf(cmd, sizeof(cmd), "%s pmie off; %s stop >/dev/null 2>&1", CHKCONFIG, INITPMIE);
        system(cmd);
        
        continue;
      } 
      
      oldvalue = newvalue = -1;
      sscanf(cmdp[i].value, "%d %d", &newvalue, &oldvalue);
      if ( oldvalue < 0 || newvalue < 0 ) 
      {
        continue;
      }
      else if ( oldvalue == newvalue ) 
      {
        continue;
        
      } 
      else 
      {
        snprintf(cmd, sizeof(cmd), "%s %s %s >/dev/null 2>&1",
                 PMIECONF, (newvalue == 0 ? "disable" : "enable"),
                 cmdp[i].keyname);
        system(cmd);
      }
    } /* End of For */
    
    return(0);
}

/*---------------------------------------------------------------------------*/
/* rgpgGenerateReport                                                        */
/*   Actual Report Generator                                                 */
/*---------------------------------------------------------------------------*/

int RGPGAPI rgpgGenerateReport(sscErrorHandle hError,rgpgSessionID session, int argc, char* argv[],char *rawCommandString,streamHandle result, char *userAccessMask)
{  CMDPAIR cmdp[MAXKEYS];
   int cmdpsize,i;
   int   err = 0;

   mySession *sess = (mySession *)session;

   if(!sess || sess->signature != sizeof(mySession))
   { 
     sscError(hError,szServerNameErrorPrefix,"Incorrect session id in rgpgGenerateReport()");
     return(RGPERR_SUCCESS);
   }

   if ( argc < 2 ) {
       sscError(hError,szServerNameErrorPrefix,"Wrong number of arguments<br>\n");
       return(RGPERR_SUCCESS);
   }


   if ( createMyHTMLGenerator(result))
   {
       sscError(hError, szServerNameErrorPrefix, 
		"Can't open the HTML generator<br>\n");
       return(RGPERR_SUCCESS);
   }
   
   if ( (err = CheckExecutables(PMIECONF)) != 0 ||
        (err = CheckExecutables(NAWK    )) != 0  ) 
   {
        generate_error_page (sess, err);
   } 
   else 
   {
        cmdpsize = sscInitPair(rawCommandString,cmdp,MAXKEYS);

	/* Set Browser type */

	for(i = 0;(i = sscFindPairByKey(cmdp,i,szUserAgent)) >= 0;i++)
    	if ( !strcasecmp(cmdp[i].value,"Lynx")) sess->textonly = 1;

	if ( !strcasecmp(argv[1], "GetConf") ) 
	{
    	   err = getPMIEConf(hError, sess, 0);
	} 
	else if ( !strcasecmp(argv[1], "DisplayConf") ) 
	{
    	   err = getPMIEConf(hError, sess, DISPLAYONLY);
	} 
	else if ( !strcasecmp(argv[1], "SetConf") ) 
	{
    	   err = setPMIEConf(cmdp, cmdpsize);
    	   if ( !err ) 
    	   err = getPMIEConf(hError, sess, DISPLAYCONFIRMATION);       
	} 
	else 
	{
    	   err = INVALIDARGUMENT;
	}

	if ( err ) 
             report_error(hError, err);
   }

   deleteMyHTMLGenerator();

   return(RGPERR_SUCCESS);
}
