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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ssdbapi.h>
#include <ssdberr.h>
#include <eventmonapi.h>

#include "rgPluginAPI.h"
#include "sscHTMLGen.h"
#include "sscPair.h"
#include "sscErrors.h"

#define START 0
#define END 1
#define MAXNUMBERS_OF_KEYS 512
#define CHARTYPE 0
#define INTTYPE 1

#ifdef __TIME__
#ifdef __DATE__
#define INCLUDE_TIME_DATE_STRINGS 1
#endif
#endif

#define DRVERSION_MAJOR    1
#define DRVERSION_MINOR    0

static char szDefaultTempTable[] = "diags_temp";
static char szDefaultDiagAction[] = "/$sss/rg/rgpdiag~DIAG_REPORT";
static char szBasicSystemReport[] = "/$sss/rg/cmreport~1~1";

static char szDefaultUserName[] = "";             /* Default user name */
static char szDefaultPassword[] = "";             /* Default password */
static char szDefaultHostName[] = "localhost";    /* Default host name */
static char szDefaultDatabaseName[] = "ssdb";     /* Default db name */


typedef struct sss_s_MYSESSION {
  unsigned long signature;            /* sizeof(mySession) */
  struct sss_s_MYSESSION *next;       /* next pointer */
  int textonly;                       /* text mode only flag */
} mySession;

static char   myTitle[] = "Diagnostics Results Reports";

static const char myLogo[]            = "Diagnostics Results Plugin";
static const char szTitle[]           = "Title";
static const char szThreadSafe[]      = "ThreadSafe";
static const char szVersion[]         = "Version";
static const char szUnloadable[]      = "Unloadable";
static const char szUnloadTime[]      = "UnloadTime";
static const char szAcceptRawCmdString[] = "AcceptRawCmdString"; /* Obligatory attribute name: how plugin will process cmd parameters */
static char szServerNameErrorPrefix[] = "ESP Diagnostic Server Error: %s";
static char szVersionStr[64];

static pthread_mutex_t seshFreeListmutex;
static const char szUnloadTimeValue[] = "120"; /* Unload time for this plug in (sec.) */
static const char szThreadSafeValue[] = "0";   /* "Thread Safe" flag value - this plugin is thread safe */
static const char szUnloadableValue[] = "1";   /* "Unloadable" flag value - RG core might unload this plugin from memory */static const char szAcceptRawCmdStringValue[] = "1";   /* RG server accept "raw" command string like key=value&key2=value&... */
static const char szUserAgent[] = "User-Agent";

static int volatile mutex_inited = 0;  /* seshFreeListmutex mutex inited flag */
static mySession *sesFreeList = 0;  /* Session free list */

/* Some Lynx related static  variables  */
static const char szDiagRepTitle1[] = "SYSTEM INFORMATION &gt; Diagnostics Results";
static const char szErrDateFormat  [] = "\"%s\" parameter must be in mm/dd/yyyy format. Please enter date correctly and try again.";
static const char szErrMonthFormat [] = "Invalid month specified in \"%s\" parameter. Please enter correct month and try again.";
static const char szErrDayFormat   [] = "Invalid day specified in \"%s\" parameter. Please enter correct day and try again.";
static const char szErrDatesOrder  [] = "\"Start Date\" is greater then \"End Date\"";
	
int DRCreateTempTable(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection, ssdb_Error_Handle error_handle, time_t start_time, time_t end_time,char *sys_id);
void DRDropTempTable(sscErrorHandle hError,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle);
void DRMakeDate (time_t timeval, char *dateout, char *event_time);
long DRMakeTime(const char *dateval1,int startend);
void DRGetData (sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection, ssdb_Error_Handle error_handle,CMDPAIR *cmdp);
int get_variable(sscErrorHandle hError, mySession *session, CMDPAIR *cmdp,int type, char *varname,char *string,int *intval,int errornum);
int getnumrecords (sscErrorHandle hError,ssdb_Error_Handle error_handle,ssdb_Request_Handle req_handle);

/* ========================================================================== */
/* Init */
/* ========================================================================== */
int RGPGAPI rgpgInit(sscErrorHandle hError)
{ /* Try initialize "free list of mySession" mutex */
  if(pthread_mutex_init(&seshFreeListmutex,0))
  { sscError(hError,szServerNameErrorPrefix,"Can't initialize mutex");
    return 0; /* error */
  }
  mutex_inited++;
#ifdef INCLUDE_TIME_DATE_STRINGS
  sprintf(szVersionStr,"%d.%d %s %s",DRVERSION_MAJOR,DRVERSION_MINOR,__DATE__,__TIME__);
#else
  sprintf(szVersionStr,"%d.%d",DRVERSION_MAJOR,DRVERSION_MINOR);
#endif
  return 1; /* success */
}

/* ========================================================================== */
/* Done */
/* ========================================================================== */
int RGPGAPI rgpgDone(sscErrorHandle hError)
{ mySession *s;
  if(mutex_inited)
  { pthread_mutex_destroy(&seshFreeListmutex);
    mutex_inited = 0;
    while((s = sesFreeList) != 0)
    { sesFreeList = s->next;
      free(s);
    }
    return 1; /* success */
  }
  return 0; /* error - not inited */
}

/* ========================================================================== */
/* CreateSesion */
/* ========================================================================== */
rgpgSessionID RGPGAPI rgpgCreateSesion(sscErrorHandle hError)
{  mySession *s;
   pthread_mutex_lock(&seshFreeListmutex);
   if((s = sesFreeList) != 0) sesFreeList = s->next;
   pthread_mutex_unlock(&seshFreeListmutex);
   if(!s) s = (mySession*)malloc(sizeof(mySession));
   if(!s)
   { sscError(hError,szServerNameErrorPrefix,"No memory to create session in rgpgCreateSesion()");
     return 0;
   }
   memset(s,0,sizeof(mySession));
   s->signature = sizeof(mySession);
   return (rgpgSessionID)s;
}

/* ========================================================================== */
/* DeleteSesion */
/* ========================================================================== */
void RGPGAPI rgpgDeleteSesion(sscErrorHandle hError,rgpgSessionID _s)
{ mySession *s = (mySession *)_s;
  if(!s || s->signature != sizeof(mySession))
  { sscError(hError,szServerNameErrorPrefix,"Incorrect session id in rgpgDeleteSesion()");
  }
  else
  {
    pthread_mutex_lock(&seshFreeListmutex);
    s->next = sesFreeList; sesFreeList = s; s->signature = 0;
    pthread_mutex_unlock(&seshFreeListmutex);
  }
}

/* ========================================================================== */
/* GetAttribute */
/* ========================================================================== */
char *RGPGAPI rgpgGetAttribute(sscErrorHandle hError,rgpgSessionID session, const char *attributeID, const char *extraAttrSpec,int *typeattr)
{  char *s;

   if(typeattr) *typeattr = RGATTRTYPE_STATIC;
     if(!strcasecmp(attributeID,szVersion))    s = (char*)szVersionStr;
   else if(!strcasecmp(attributeID,szTitle))      s = (char*)myLogo;
   else if(!strcasecmp(attributeID,szUnloadTime)) s = (char*)szUnloadTimeValue;
   else if(!strcasecmp(attributeID,szThreadSafe)) s = (char*)szThreadSafeValue;
   else if(!strcasecmp(attributeID,szUnloadable)) s = (char*)szUnloadableValue;
   else if(!strcasecmp(attributeID,szAcceptRawCmdString)) s = (char*)szAcceptRawCmdStringValue;
   else
   { sscError(hError,"%s No such attribute '%s' in rgpgGetAttribute()",szServerNameErrorPrefix,attributeID);
     return 0;
   }
   if(!s) sscError(hError,szServerNameErrorPrefix,"No memory in rgpgGetAttribute()");
   return s;
}

/* ========================================================================== */
/* FreeAttributeString */
/* ========================================================================== */
void RGPGAPI rgpgFreeAttributeString(sscErrorHandle hError,rgpgSessionID session,const char *attributeID,const char *extraAttrSpec, char *attrString,int restype)
{
    if(((restype == RGATTRTYPE_SPECIALALLOC) || (restype == RGATTRTYPE_MALLOCED)) && attrString) free(attrString);
}

/* ========================================================================== */
/* SetAttribute */
/* ========================================================================== */
void  RGPGAPI rgpgSetAttribute(sscErrorHandle hError, rgpgSessionID session, const char *attributeID, const char *extraAttrSpec, const char *value)
{  char buff[128]; mySession *sess = session;

   if(!strcasecmp(attributeID,szVersion) || !strcasecmp(attributeID,szTitle) ||
      !strcasecmp(attributeID,szThreadSafe) || !strcasecmp(attributeID,szUnloadable) ||
      !strcasecmp(attributeID,szUnloadTime))
   {   sscError(hError,szServerNameErrorPrefix,"Attempt to set read only attribute in rgpgSetAttribute()");
       return;
   }
   if(!strcasecmp(attributeID,"User-Agent") && value)
   {   sess->textonly = (strcasecmp(value,"Lynx") == 0) ? 1 : 0;
       return;
   }
   if(!sess || sess->signature != sizeof(mySession))
   {   sscError(hError,szServerNameErrorPrefix,"Incorrect session id in rgpgGetAttribute()");
       return;
   }
}

/* ========================================================================== */
/* GenerateReport */
/* ========================================================================== */
int RGPGAPI rgpgGenerateReport(sscErrorHandle hError, rgpgSessionID _session, int argc, char* argv[],char *rawCommandString, streamHandle result, char *accessmask)
{
    ssdb_Client_Handle client;
    ssdb_Connection_Handle connection;
    ssdb_Request_Handle req_handle;
    ssdb_Error_Handle error_handle;
    int i,SQL_TYPE;
    CMDPAIR cmdp[MAXNUMBERS_OF_KEYS];
    mySession *session = _session;

    if ((error_handle = ssdbCreateErrorHandle()) == NULL)
    {   sscError(hError,"Database API Error: \"%s\" - can't create error handle\n",ssdbGetLastErrorString(0));
        return 0;
    }
    if ((client = ssdbNewClient(error_handle,szDefaultUserName,szDefaultPassword,0)) == NULL)
    {   sscError(hError,"Database API Error: \"%s\" - can't create new client handle\n",ssdbGetLastErrorString(error_handle));
        ssdbDeleteErrorHandle(error_handle);
        return 0;
    }

    /* Establish a connection to a database */
    if ((connection = ssdbOpenConnection(error_handle,client,szDefaultHostName,szDefaultDatabaseName,0)) == NULL)
    {   sscError(hError,"Database API Error: \"%s\" - can't open connection\n",ssdbGetLastErrorString(error_handle));
        ssdbDeleteErrorHandle(error_handle);
        return 0;
    }

    /*  Initialize The HTML Generator  */
    i = createMyHTMLGenerator(result);
    if (i != 0)
    {   sscError(hError,  "Sample server: Can't open the HTML generator");
        ssdbDeleteErrorHandle(error_handle);
        return 0;
    }
    if (argc < 2)
    {   sscError(hError, "Invalid Request\n");
        ssdbDeleteErrorHandle(error_handle);
        return 0;
    }
    sscInitPair(rawCommandString,cmdp,MAXNUMBERS_OF_KEYS);

    /* Check WEB browser type (Lynx is only tex based) */
    for(i = 0;(i = sscFindPairByKey(cmdp,i,szUserAgent)) >= 0;i++)
    if(!strcasecmp(cmdp[i].value,"Lynx")) session->textonly = 1;
    if (!strcasecmp(argv[1], "DIAG_REPORT"))
        DRGetData(hError,session,connection,error_handle,cmdp);
    else
    {   if (!strcasecmp(argv[1], "SYS_INFO"))
           DRGetSystemInfo(hError,session,connection,error_handle,NULL,NULL,NULL);
        else
           sscError(hError, "Invalid Report Request %s",argv[1]);
    }

    /*  Destroy The HTML Generator  */
    if (deleteMyHTMLGenerator())
    {   sscError(hError, "Server: Can't destroy the HTML generator");
        ssdbDeleteErrorHandle(error_handle);
        return 0;
    }
    if (ssdbIsConnectionValid(error_handle,connection))
        ssdbCloseConnection (error_handle,connection);
    if (ssdbIsClientValid(error_handle,client))
        ssdbDeleteClient(error_handle,client);
    if (error_handle)
        ssdbDeleteErrorHandle(error_handle);
    return 0;
}

/* ========================================================================== */
/* Functions to generate html */
/* ========================================================================== */
static void CellString(const char *cellparam,const char *format,...)
{   char buff[2048];
    va_list args;
    va_start(args, format);
    vsnprintf(buff, sizeof(buff), format, args);
    va_end(args);
    FormatedBody("<td %s><FONT FACE=Arial,Helvetica>%s</td>\n",cellparam ? cellparam : "",buff);
}

void DRDisplayErrorHtml(mySession *session, const char *szTitle,  const char *szError)
{   Body("<html><head><title>Embedded Support Partner</title></head>\n");
    if(session->textonly == 0)
    {   Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
	TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
	RowBegin(0);
	    CellString("bgcolor=\"#cccc99\" width=15","&nbsp;&nbsp;&nbsp;&nbsp;");
	    CellString("bgcolor=\"#cccc99\"","%s",szTitle);
	RowEnd();
	RowBegin(0);
	    CellString("colspan=2","&nbsp;<p>&nbsp;");
	RowEnd();
	RowBegin(0);
	    CellString(0,"&nbsp;");
	    CellString(0,"%s",szError);
	RowEnd();
	TableEnd();
    }
    else
    {   Body("<body>");
	FormatedBody("<pre>   %s</pre>",szTitle);
	Body("<hr width=100%>");
	FormatedBody("<p>%s",szError);
    }
    Body("</body></html>");
    return;
}

void DRDisplayHeader(mySession *session,char *formname)
{   Body("<html><head><title>Embedded Support Partner</title></head>\n");
    Body("<body bgcolor=\"#ffffcc\" vlink=\"#333300\" link=\"#333300\">\n");
    if(session->textonly == 0)
    {   Body("<form method=POST");
        if (formname)
            FormatedBody(" action=\"%s\" name=\"%s\">",szDefaultDiagAction,formname);
        else
	    Body(">");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
        RowBegin(0);
	    CellString("bgcolor=\"#cccc99\" width=15","&nbsp;&nbsp;&nbsp;&nbsp;");
	    CellString("bgcolor=\"#cccc99\"","SYSTEM INFORMATION &gt; Diagnostics Results");
        RowEnd();
        RowBegin(0);
	    CellString("colspan=2","&nbsp;<p>&nbsp;");
        RowEnd();
    }
    else
    {   FormatedBody("<pre>   %s</pre>",szDiagRepTitle1);
	Body("<hr width=100%>\n");
	FormatedBody("<form method=POST action=\"%s\">",szDefaultDiagAction);
    }
    return;
}

void DRDisplayFooter(mySession *session)
{   if(session->textonly == 0)
        TableEnd();
    else
	Body("<hr width=100%><p><a href=\"/index_sem.txt.html\">Return on Main Page</a>");
    Body("</form></body></html>");
    return;
}

void DRServerError(mySession *session, int errornum)
{   DRDisplayHeader(session,NULL);
    if(session->textonly == 0)
    {   RowBegin(0);
            CellString(0,"&nbsp;");
            CellString(0,"<b>Server Error:</b> Diagnostic server error. Error #: %d",errornum);
        RowEnd();
    }
    else
        Body("Server Error:<br>");
        FormatedBody("<pre>       Diagnostic server error. Error #: %d</pre>",errornum);
    DRDisplayFooter(session);
}

void DRDisplaySysInfo(mySession *session,const char *sys_name,char *sys_id,const char *sys_serial,const char *ip_type,const char *ip_address,char *startdate,char *enddate)
{   if(session->textonly == 0)
    {   RowBegin(0);
            CellString(0,"&nbsp;");
	    CellBegin(0);
	        TableBegin("border=0 cellspacing=0 cellpadding=0");
	        RowBegin(0);
		    CellString(0,"System name");
		    CellString(0,"&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;");
		    CellString(0,"%s",sys_name);
	        RowEnd();
	        RowBegin(0);
		    CellString(0,"System ID");
		    CellString(0,"&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;");
		    CellString(0,"%s",sys_id);
	        RowEnd();
	        RowBegin(0);
		    CellString(0,"System serial number");
		    CellString(0,"&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;");
		    CellString(0,"%s",sys_serial);
	        RowEnd();
	        RowBegin(0);
		    CellString(0,"System IP type");
		    CellString(0,"&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;");
		    CellString(0,"IP%s",ip_type);
	        RowEnd();
	        RowBegin(0);
		    CellString(0,"System IP address");
		    CellString(0,"&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;");
		    CellString(0,"%s",ip_address);
	        RowEnd();
	        if(startdate && enddate)
	        {  RowBegin(0);
		        CellString("colspan=3","&nbsp;");
	            RowEnd();
	            RowBegin(0);
		        CellString(0,"<b>Time period</b>");
		        CellString(0,"&nbsp;&nbsp;&nbsp;&nbsp;<b>:</b>&nbsp;");
		        CellString(0,"<b>%s - %s</b>",startdate,enddate);
	            RowEnd();
	        }
	        TableEnd();
	    CellEnd();
        RowEnd();
        RowBegin(0);
            CellString(0,"&nbsp;");
            CellString(0,"<hr>");
        RowEnd();
    }
    else
    {   FormatedBody("<pre>   System name          : %s</pre>",sys_name);
	FormatedBody("<pre>   System ID            : %s</pre>",sys_id);
	FormatedBody("<pre>   System serial number : %s</pre>",sys_serial);
	FormatedBody("<pre>   System IP type       : IP%s</pre>",ip_type);
	FormatedBody("<pre>   System IP address    : %s</pre>",ip_address);
	if(startdate && enddate)
	{   Body("<p>");
            FormatedBody("<pre>   Time period          : %s - %s</pre>",startdate,enddate);
	    Body("<p>");
	}
    }
}

void DRNoSystem(mySession *session,char *sys_id)
{   if(session->textonly == 0)
    {   RowBegin(0);
            CellString(0,"&nbsp;");
	    CellString(0,"<b>There is no system information for system with \"%s\" system ID.</b>",sys_id);
        RowEnd();
    }
    else
        FormatedBody("<p>There is no system information for system with \"%s\" system ID.",sys_id);
}

void DRDisplayNoRecords(mySession *session)
{   if(session->textonly == 0)
    {   RowBegin(0);
	    CellString("colspan=2","&nbsp;");
        RowEnd();
        RowBegin(0);
            CellString(0,"&nbsp;");
	    CellString(0,"<b>There are no diagnostics results on this system for the specified period of time.</b>");
        RowEnd();
    }
    else
        FormatedBody("<p>There are no diagnostics results on this system for the specified period of time.");
}


void DRDisplayReportTableStart(mySession *session, int pageno, int total_pages)
{   if(session->textonly == 0)
    {   RowBegin(0);
            CellString(0,"&nbsp;");
            CellBegin(0);
	        TableBegin("border=0 cellspacing=0 cellpadding=0 width=100%");
	        RowBegin(0);
		    CellString("align=right","Page %d of %d",pageno,total_pages);
	        RowEnd();
	        RowBegin(0);
		    CellBegin(0);
		        TableBegin("border=4 cellspacing=1 cellpadding=6 width=100%");
		        RowBegin("align=center");
			    CellString(0,"<b>No.</b>");
			    CellString(0,"<b>Diagnostic Name</b>");
			    CellString(0,"<b>Diagnostic Result</b>");
			    CellString(0,"<b>Diagnostic Result Time</b>");
		        RowEnd();
    }
    else
    {   FormatedBody("<p><pre>   Page %d of %d\n",pageno,total_pages);
	FormatedBody("  |----------------------------------------------------------------------|\n");
	FormatedBody("  | No. | Diagnostic Name               | Result   | Diagnostic Time     |\n");
    }
}

void DRDisplayReportTableEnd(mySession *session, int number_of_records, int pageno, int total_pages, char *sys_id, char *startdate, char *enddate)
{   int rec_sequence,firstpage,lastpage,bottom;

    firstpage = ((pageno-1)/10)*10+1;
    lastpage  = firstpage+9;
    if (lastpage >= total_pages)
        lastpage = total_pages;

    if(session->textonly == 0)
    {                   TableEnd();
		    CellEnd();
	        RowEnd();
	        RowBegin(0);
		    CellString(0,"&nbsp;");
                RowEnd();
	        RowBegin(0);
		    CellBegin("align=center");
    }
    else
    {   FormatedBody( "  ------------------------------------------------------------------------</pre>\n");
	Body("<p>\n");
    }
    if (firstpage > 1)
    {   if(session->textonly == 0)
        {   FormatedBody("<a href=\"%s?row_num=%d&sys_id=%s&config_start_time=%s&config_end_time=%s\"><img src=\"/images/double_arrow_left.gif\" border=0 alt=\"fisrt page\"></a>&nbsp;&nbsp; ",szDefaultDiagAction,0,sys_id,startdate,enddate);
            FormatedBody("<a href=\"%s?row_num=%d&sys_id=%s&config_start_time=%s&config_end_time=%s\"><img src=\"/images/arrow_left.gif\" border=0 alt=\"previous 10 pages\"></a>&nbsp;&nbsp; ",szDefaultDiagAction,(firstpage-11)*10,sys_id,startdate,enddate);
        }
	else
	{   FormatedBody("<a href=\"%s?row_num=%d&sys_id=%s&config_start_time=%s&config_end_time=%s\">&lt;&lt;</a>&nbsp;&nbsp; ",szDefaultDiagAction,0,sys_id,startdate,enddate);
	    FormatedBody("<a href=\"%s?row_num=%d&sys_id=%s&config_start_time=%s&config_end_time=%s\">&lt;</a>&nbsp;&nbsp; ",szDefaultDiagAction,(firstpage-11)*10,sys_id,startdate,enddate);
	}
    }
    for (rec_sequence = firstpage; rec_sequence <= lastpage; rec_sequence++)
    {   if (rec_sequence == pageno)
        {   if (total_pages != 1)
                FormatedBody("<b><font color=\"#ff3300\">%d</b></font>&nbsp;&nbsp;",rec_sequence);
        }
        else
            FormatedBody("<a href=\"%s?row_num=%d&sys_id=%s&config_start_time=%s&config_end_time=%s\"><b>%d</b></a>&nbsp;&nbsp; ",szDefaultDiagAction,10*(rec_sequence-1),sys_id,startdate,enddate,rec_sequence);
    }
    if (lastpage < total_pages)
    {   if(number_of_records%10 == 0)
            bottom = number_of_records -10;
        else
            bottom = number_of_records - (number_of_records%10);
	if(session->textonly == 0)
	{   FormatedBody("<a href=\"%s?row_num=%d&sys_id=%s&config_start_time=%s&config_end_time=%s\"><img src=\"/images/arrow_right.gif\" border=0 alt=\"next 10 pages\"></a>&nbsp;&nbsp; ",szDefaultDiagAction,(firstpage+9)*10,sys_id,startdate,enddate);
	    FormatedBody("<a href=\"%s?row_num=%d&sys_id=%s&config_start_time=%s&config_end_time=%s\"><img src=\"/images/double_arrow_right.gif\" border=0 alt=\"last page\"></a>&nbsp;&nbsp; ",szDefaultDiagAction,bottom,sys_id,startdate,enddate);
	}
	else
	{   FormatedBody("<a href=\"%s?row_num=%d&sys_id=%s&config_start_time=%s&config_end_time=%s\">&gt;    </a>&nbsp;&nbsp; ",szDefaultDiagAction,(firstpage+9)*10,sys_id,startdate,enddate);
	    FormatedBody("<a href=\"%s?row_num=%d&sys_id=%s&config_start_time=%s&config_end_time=%s\">&gt;&gt;</a>&nbsp;&nbsp; ",szDefaultDiagAction,bottom,sys_id,startdate,enddate);
	}
    }
    if(session->textonly == 0)
    {               CellEnd();
                RowEnd();
                TableEnd();
	    CellEnd();
        RowEnd();
    }
}

void DRDisplayReportRecord(mySession *session, int rec_sequence, const char *diag_name, const char *diag_flag, char *ev_date, char * ev_time)
{   const char *diag_status;

    if (!strcmp(diag_flag,"F"))
	diag_status = "FAIL";
    if (!strcmp(diag_flag,"C"))
	diag_status = "COMPLETE";
    if (!strcmp(diag_flag,"P"))
	diag_status = "PASS";
    if(session->textonly == 0)
    {   RowBegin("valign=top");
            CellString(0,"%d",rec_sequence+1);
	    CellString(0,"%s",diag_name);
	    CellString(0,"%s",diag_status);
	    CellString(0,"%s  %s",ev_date,ev_time);
        RowEnd();
    }
    else
    {   FormatedBody("  |----------------------------------------------------------------------|\n");
	FormatedBody("  |%-5d| %-29.29s | %-8s | %-10.10s %-8.8s |\n",rec_sequence+1,diag_name,diag_status,ev_date,ev_time);
    }
}
    

/* ========================================================================== */
/* Functions to operate with database                                         */
/* ========================================================================== */

void DRDropTempTable(sscErrorHandle hError,ssdb_Connection_Handle connection,ssdb_Error_Handle error_handle)
{   int number_of_records,rec_sequence;
    const char **vector5,**vector6;
    ssdb_Request_Handle req_handle5,req_handle6;

    if (!ssdbUnLockTable(error_handle,connection))
        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
    if (!(req_handle5 = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SHOW, "show tables")))
    {   sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
        return;
    }
    if ((number_of_records = getnumrecords(hError,error_handle,req_handle5)) > 0)
    {   for (rec_sequence=0;rec_sequence < number_of_records; rec_sequence++)
        {   vector5 = ssdbGetRow(error_handle,req_handle5);
            if (vector5)
            {   if(!strcmp(vector5[0],szDefaultTempTable))
                {   if (!(req_handle6 = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DROPTABLE,"drop table %s",szDefaultTempTable)))
                    {   sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                    }
                    ssdbFreeRequest(error_handle,req_handle6);
                }
            }
        }
    }
    ssdbFreeRequest(error_handle,req_handle5);
}

int DRCreateTempTable(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
ssdb_Error_Handle error_handle, long start_time, long end_time, char *sys_id)
{   ssdb_Request_Handle req_handle,req_handle1;
    int number_of_records,rec_sequence;
    const char **vector,**vector1;

    DRDropTempTable(hError,connection,error_handle);
    if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_CREATE,
    "create table %s(diag_name varchar(128),diag_flag char(1),diag_end bigint)",szDefaultTempTable)))
    {   sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
        return 0;
    }
    ssdbFreeRequest(error_handle,req_handle);
    if (!(req_handle1 = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
    "select dbname from archive_list")))
    {   sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
        DRDropTempTable(hError,connection,error_handle);
        return 0;
    }
    if ((number_of_records = getnumrecords(hError,error_handle,req_handle1)) > 0)
    {   for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
	{   vector1 = ssdbGetRow(error_handle,req_handle1);
	    if (vector1)
	    {   if(!(ssdbLockTable(error_handle,connection,
                "%s,%s.test_data,%s.event",szDefaultTempTable,vector1[0],vector1[0])))
                {   sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                    DRDropTempTable(hError,connection,error_handle);
                    return 0;
                }
                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
                "insert into %s select %s.test_data.test_name,%s.test_data.test_flag,%s.event.event_end "
                "from %s.test_data,%s.event "
                "where %s.test_data.event_id = %s.event.event_id "
		"and %s.event.event_start >= %d "
                "and %s.event.event_end <= %d "
                "and %s.test_data.sys_id = '%s' "
		"and %s.event.sys_id = '%s'",szDefaultTempTable,vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],vector1[0],
		vector1[0],vector1[0],start_time,vector1[0],end_time,vector1[0],vector1[0],sys_id,sys_id)))
                {   sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                    DRDropTempTable(hError,connection,error_handle);
                    return 0;
                }
		ssdbFreeRequest(error_handle,req_handle);
                if (!ssdbUnLockTable(error_handle,connection))
                {   sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                    DRDropTempTable(hError,connection,error_handle);
		    return 0;
                }
	    }
	}
    }
    ssdbFreeRequest(error_handle,req_handle1);
    if(!(ssdbLockTable(error_handle,connection,"%s,test_data,event"),szDefaultTempTable))
    {   sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
	DRDropTempTable(hError,connection,error_handle);
	return 0;
    }
    if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
    "insert into %s select test_data.test_name,test_data.test_flag,event.event_end "
    "from test_data,event "
    "where test_data.event_id = event.event_id "
    "and event.event_start >= %d "
    "and event.event_end <= %d "
    "and test_data.sys_id = '%s' "
    "and event.sys_id = '%s'",szDefaultTempTable,start_time,end_time,sys_id,sys_id)))
    {   sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
	DRDropTempTable(hError,connection,error_handle);
	return 0;
    }
    ssdbFreeRequest(error_handle,req_handle);
    return 1; /* Success */
}

void DRMakeDate (time_t timeval, char *dateout, char *event_time)
{   struct tm *timeptr;
    time_t timein = timeval;
    timeptr = localtime(&timein);
    strftime(dateout,20,"%m/%d/%Y",timeptr);
    strftime(event_time,20,"%X",timeptr);
}

long DRMakeTime(const char *dateval1,int startend)
{   time_t timevalue;
    struct tm timestruct;
    struct tm *timeptr;
    char dateval[40];
    int i;
    char *p;

    strcpy (dateval,dateval1);
    p = strtok(dateval,"/");
    i = atoi(p);
    i = i - 1;
    timestruct.tm_mon = i;
    p = strtok(NULL,"/");
    timestruct.tm_mday = atoi(p);
    p = strtok(NULL,"/");
    i = atoi(p);
    i = i - 1900;
    timestruct.tm_year = i;
    timestruct.tm_isdst = -1;
    if (!startend)
    {   timestruct.tm_sec = 00;
        timestruct.tm_min = 00;
        timestruct.tm_hour = 00;
    }
    else
    {   timestruct.tm_sec = 60;
        timestruct.tm_min = 59;
        timestruct.tm_hour = 23;
    }
    timevalue =  mktime(&timestruct);
    return (timevalue);
}

int IsStrBlank(char *szStr)
{   int  len;

    if(szStr == NULL)
	return 1;
    len = strlen(szStr);
    if(len == 0)
	return 1;
    if(strspn(szStr, " \t\r\n") == len)
	return 1;
    return 0;
}

int IsValidDateFormat(char * szDate, mySession *session, const char *szTitle, const char *szParam)
{   int m,d,y;
    char szErrorText[256];

    if(3 != sscanf(szDate, "%d/%d/%d", &m, &d, &y))
    {   snprintf(szErrorText, sizeof(szErrorText), szErrDateFormat, szParam);
	DRDisplayErrorHtml(session, szTitle, szErrorText);
	return 0;
    }
    if(m <=0 || m > 12)
    {   snprintf(szErrorText, sizeof(szErrorText), szErrMonthFormat, szParam);
	DRDisplayErrorHtml(session, szTitle, szErrorText);
	return 0;
    }
    if(d <=0 || d > 31)
	goto DayError;
    if(m == 4 || m == 6 || m == 9 || m == 11)
    {   if(d > 30)
	    goto DayError;
    }
    if(m == 2)
    {   if(d > 29)
	    goto DayError;
	if(d ==29)
	    if(y%4 != 0)
		goto DayError;
    }
    return 1;

    DayError:
	snprintf(szErrorText, sizeof(szErrorText), szErrDayFormat, szParam);
	DRDisplayErrorHtml(session, szTitle, szErrorText);
	return 0;
}

int IsDateVarValid(const char *szVarName, CMDPAIR *cmdp , mySession *session, const char * szTitle, const char *szParamName)
{   int idx;

    idx = sscFindPairByKey(cmdp, 0, szVarName);
    if(idx >= 0)
    {   if(IsStrBlank(cmdp[idx].value))
	{   char szErr[128];
	    sprintf(szErr, "Parameter \"%s\" is empty or missing", szParamName);
	    DRDisplayErrorHtml(session, szTitle, (const char *) szErr);
	    return 0;
	}
	return IsValidDateFormat(cmdp[idx].value, session, szTitle, szParamName);
    }
    return 1; /* this error will be handled by farther processing */
}

int DRGetSystemInfo(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection, ssdb_Error_Handle error_handle, char *sys_id, char *startdate, char *enddate)
{   int common_int;
    const char **vector;
    ssdb_Request_Handle req_handle;
    char sysid[32];

    if (!sys_id)
    {   if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
	"select hostname,sys_serial,sys_type,ip_addr,sys_id from system where active = 1 and local = 1")))
	{   sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle)); 
	    return 0;
	}
    }
    else
    {   if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select hostname,sys_serial,sys_type,ip_addr from system where sys_id = '%s' and active = 1",sys_id)))
        {   sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
	    return 0;
        }
    }
    if (getnumrecords(hError,error_handle,req_handle) == 0)
    {   DRNoSystem(session,sys_id);
	ssdbFreeRequest(error_handle,req_handle);
	return 0;
    }
    vector = ssdbGetRow(error_handle,req_handle);
    if (vector)
    {   if (!sys_id) 
	    strcpy(sysid,vector[4]);
	else
	    strcpy(sysid,sys_id);
        if (startdate && enddate)
            DRDisplaySysInfo(session,vector[0],sysid,vector[1],vector[2],vector[3],startdate,enddate);
	else
	    DRDisplaySysInfo(session,vector[0],sysid,vector[1],vector[2],vector[3],NULL,NULL);
    }
    ssdbFreeRequest(error_handle,req_handle);
    return 1; /*Success */
}

void DRGetData (sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection, ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{   ssdb_Request_Handle req_handle;
    int number_of_records,rec_sequence,common_int,row_num,balance_rows,total_pages,pageno,firstpage,lastpage,bottom;
    long start_time,end_time;
    const char **vector;
    char sys_id[32],startdate[40],enddate[40],dateout[16],event_time[16],common_string[2];

    if ( session->textonly != 0 )
    {   if(!IsDateVarValid ("config_start_time", cmdp , session, szDiagRepTitle1, "Start date"))
	    return;
	if(!IsDateVarValid ("config_end_time"  , cmdp , session, szDiagRepTitle1, "End  date"))
	    return;
    }
    if(!get_variable(hError,session,cmdp,CHARTYPE,"sys_id",sys_id,&common_int,501))
	return;
    if(!get_variable(hError,session,cmdp,CHARTYPE,"config_start_time",startdate,&common_int,502))
        return;
    if(!get_variable(hError,session,cmdp,CHARTYPE,"config_end_time",enddate,&common_int,503))
        return;
    if(!get_variable(hError,session,cmdp,INTTYPE,"row_num",common_string,&row_num,504))
        return;
    start_time = DRMakeTime(startdate,START);
    end_time   = DRMakeTime(enddate,END);
    if(end_time < start_time)
    {   DRDisplayErrorHtml(session, szDiagRepTitle1, szErrDatesOrder);
	return;
    }
    DRDisplayHeader(session,"DiagResultsForm");
    if (!DRCreateTempTable(hError,session,connection,error_handle,start_time,end_time,sys_id))
       	return;
    if (!DRGetSystemInfo(hError,session,connection,error_handle,sys_id,startdate,enddate))
	return;
    if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,"select * from %s",szDefaultTempTable)))
    {   sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
        DRDropTempTable(hError,connection,error_handle);
	ssdbFreeRequest(error_handle,req_handle);
	DRDisplayFooter(session);
	return;
    }
    if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
    {   DRDisplayNoRecords(session);
	DRDisplayFooter(session);
        DRDropTempTable(hError,connection,error_handle);
	ssdbFreeRequest(error_handle,req_handle);
        return;
    }
    total_pages = number_of_records/10;
    if (number_of_records%10)
       total_pages++;
    pageno = row_num/10;
    pageno++;
    DRDisplayReportTableStart(session,pageno,total_pages);
    balance_rows = row_num + 10;
    if (balance_rows > number_of_records)
        balance_rows = number_of_records;
    ssdbRequestSeek(error_handle,req_handle,row_num,0);
    vector = ssdbGetRow(error_handle,req_handle);
    for (rec_sequence = row_num; rec_sequence < balance_rows; rec_sequence++)
    {   if (vector)
        {   DRMakeDate(atoi(vector[2]),dateout,event_time);
	    DRDisplayReportRecord(session,rec_sequence,vector[0],vector[1],dateout,event_time);
	}
	vector = ssdbGetRow(error_handle,req_handle);
    }
    ssdbFreeRequest(error_handle,req_handle);
    DRDisplayReportTableEnd(session,number_of_records,pageno,total_pages,sys_id,startdate,enddate);
    DRDisplayFooter(session);
}

int get_variable(sscErrorHandle hError,mySession *session,CMDPAIR *cmdp,int type, char *varname,char *string,int *intval,int errornum)
{   int key;

    if ((key = sscFindPairByKey(cmdp,0,varname)) < 0)
    {   DRServerError(session,errornum);
        return 0;
    }
    if(type)
    {   if (sscanf(cmdp[key].value,"%d",intval) != 1)
        {   DRServerError(session,errornum);
            return 0;
        }
    }
    else
    {   if (cmdp[key].value && cmdp[key].value[0])
            strcpy(string,cmdp[key].value);
        else
        {   DRServerError(session,errornum);
            return 0;
        }
    }
    return 1;
}

int getnumrecords (sscErrorHandle hError,ssdb_Error_Handle error_handle,ssdb_Request_Handle req_handle)
{   int number_of_records;

    number_of_records = ssdbGetNumRecords(error_handle,req_handle);
    if(ssdbGetLastErrorCode(error_handle) != SSDBERR_SUCCESS)
    {   sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
        return 0;
    }
    return number_of_records;
}
