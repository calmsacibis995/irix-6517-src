/*
 * Copyright 1992-1999 Silicon Graphics, Inc.
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

/* The main routine for the availabiity plugin */

#include "availability.h"

static const char szLynxAutoEmailRadioFormat[] = "<p><pre>   Automatic e-mail distribution               : <input type=\"radio\" name=av_autoemail value=1 %s> Enable <input type=\"radio\" name=av_autoemail value=0 %s> Disable</pre>";
static const char szLynxShutDownRadioFormat [] = "<p><pre>   Display reason for shutdown                 : <input type=\"radio\" name=av_shutdown  value=1 %s> Enable <input type=\"radio\" name=av_shutdown  value=0 %s> Disable</pre>";
static const char szLynxHinvRadioFormat     [] = "<p><pre>   Include HINV information in the e-mail      : <input type=\"radio\" name=av_hinv      value=1 %s> Yes    <input type=\"radio\" name=av_hinv      value=0 %s> No</pre>";
static const char szLynxSysMsgRadioFormat   [] = "<p><pre>   Capture important system messages           : <input type=\"radio\" name=av_sysmsg    value=1 %s> Enable <input type=\"radio\" name=av_sysmsg    value=0 %s> Disable</pre>";
static const char szLynxUpTimeRadioFormat   [] = "<p><pre>   Start uptime daemon                         : <input type=\"radio\" name=av_uptimed   value=1 %s> Yes    <input type=\"radio\" name=av_uptimed   value=0 %s> No</pre>";
static const char szLynxNumDaysInputFormat  [] = "<p><pre>   Number of days between status updates       : <input type=\"text\" name=av_daysupdate size=4 value=%s> days</pre>";
static const char szLynxUptimeCheckFormat   [] = "<p><pre>   Interval in seconds between uptime checks   : <input type=\"text\" name=av_tickduration size=4 value=%s> seconds</pre>";
static const char szLynxInTextEmailFormat   [] = "<pre>      in text form                       :<input type=\"text\" name=av_text            size=30 value=\"%s\"></pre>";
static const char szLynxInCompEmailFormat   [] = "<pre>      in compressed form                 :<input type=\"text\" name=av_compressed      size=30 value=\"%s\"></pre>";
static const char szLynxInEncrEmailFormat   [] = "<pre>      in compressed encrypted form       :<input type=\"text\" name=av_encrypted       size=30 value=\"%s\"></pre>";
static const char szLynxInTextDiagFormat    [] = "<pre>      in text form                       :<input type=\"text\" name=av_diag_text       size=30 value=\"%s\"></pre>";
static const char szLynxInCompDiagFormat    [] = "<pre>      in compressed form                 :<input type=\"text\" name=av_diag_compressed size=30 value=\"%s\"></pre>";
static const char szLynxInEncrDiadFormat    [] = "<pre>      in compressed encrypted form       :<input type=\"text\" name=av_diag_encrypted  size=30 value=\"%s\"></pre>";
static const char szLynxInPagerAmailFormat  [] = "<p><pre>   Enter email addresses for chatty pager:<input type=\"text\" name=av_pager        size=30 value=\"%s\"></pre>";


/* Some Lynx related Helpers */

static int IsStrBlank ( const char * str )
{
  if ( str == NULL || str[0] == 0 || (strspn ( str," ") == strlen (str))) 
       return 1;
  else 
       return 0;     
}

static int IsAnyBadChars ( const char * str )
{
   if ( strpbrk( str, " " ) != NULL )
        return 1;

   if ( strchr ( str, '"' ) != NULL )
        return 1;
   return 0;
}


static int IsValidInt ( const char * str, int min, int max )
{
  int  val;
  
  if ( strspn ( str, "0123456789" ) != strlen(str))
       return 0;
       
  if ( sscanf ( str, "%d", &val ) != 1 )
       return 0;
       
  if ( val < min || val > max ) 
       return 0;         
  
  return 1;
}

int addresscompare(const void *address1, const void *address2)
{
    return(strcmp(*(char **)address1, *(char **)address2));
}

static void updateemailconfig_availmon(ssdb_Error_Handle Error, ssdb_Request_Handle Request, 
				       int num_records, char *newconfig, char *tag)
{
    char                 *nc[128];
    char                 *oc[128];
    int                  numnc = 0;
    int                  numoc = 0;
    char                 *tmpaddr = NULL;
    char                 *p = NULL;
    int                  i = 0, j = 0, k = 0;
    char                 regist[1024];
    char                 deregist[1024];
    int                  regbytes = 0, deregbytes = 0;
    int                  adds = 0; 
    int                  deletes = 0;
    const char           **vector;
    char                 **ap; 
    char                 **ap1;

    if ( !Request || !Error || !tag ) return;

    regbytes += snprintf(&regist[regbytes], sizeof(regist), 
			 "/usr/etc/amformat -r -E %s:", tag);
    deregbytes += snprintf(&deregist[deregbytes], sizeof(deregist),
			 "/usr/etc/amformat -d -E %s:", tag);

    if ( newconfig && strlen(newconfig) ) {
	tmpaddr = strdup(newconfig);
	p = strtok(tmpaddr, " ,");
	while (p) { 
	    nc[numnc] = p;
	    numnc++;
	    p = strtok(NULL, " ,");
	}
    }

    if ( numnc ) {
	qsort(nc, numnc, sizeof(char *), addresscompare);
    }

    nc[numnc] = NULL;

    ssdbRequestSeek(Error, Request, 0, 0);
    for ( i = 0; i < num_records; i++ ) {
	vector = ssdbGetRow(Error, Request);
	if ( vector && vector[0] && vector[0][0] ) {
	    oc[numoc] = strdup(vector[0]);
	    numoc++;
	}
    }

    oc[numoc] = NULL;

    if ( numoc ) {
	qsort(oc, numoc, sizeof(char *), addresscompare);
    }

    if ( !numnc && !numoc ) goto end;

    /* Start the comparision */

    ap = nc;
    ap1= oc;

    while ( *ap  && *ap1) {
       i = strcmp(*ap, *ap1);
       if ( i == 0 ) {
           ap++;
	   ap1++;
       } else if ( i < 0 ) {
           regbytes += snprintf(&regist[regbytes], sizeof(regist), "%s,", *ap);
	   ap++;
	   adds++;
       } else {
           deregbytes += snprintf(&deregist[deregbytes], sizeof(deregist), "%s,", *ap1);
           ap1++;
           deletes++;
       }
    }

    while (*ap) {
	regbytes += snprintf(&regist[regbytes], sizeof(regist), "%s,", *ap);
	ap++;
	adds++;
    }

    while (*ap1) {
	deregbytes += snprintf(&deregist[deregbytes], sizeof(deregist), "%s,", *ap1);
	ap1++;
	deletes++;
    }

    snprintf(&regist[regbytes], sizeof(regist), " 2>&1 >/dev/null &");
    snprintf(&deregist[deregbytes], sizeof(deregist), " 2>&1 >/dev/null &");
    if ( adds ) system(regist);
    if ( deletes ) system(deregist);
    end:
    free(tmpaddr);
    if ( numoc ) 
	for ( i = 0; i < numoc; i++ ) free(oc[i]);
    return;
}

static void updatert_availmon(ssdb_Error_Handle Error, ssdb_Connection_Handle Connection,
                              int av_autoemail, int av_shutdown, int av_uptimed, int av_daysupdate,
                              int av_tickduration) 
{
    const  char **vector;
    ssdb_Request_Handle  Request = 0;
    int    numrows = 0;
    int    i = 0;
    int    runcommand = 0;
    char   systemcmd[1024];
    int    tickerddone = 0;

    if ( !Error || !Connection ) return;

    if ( !(Request = ssdbSendRequest(Error, Connection, SSDB_REQTYPE_SELECT,
		     "select tool_option, option_default from tool where tool_name = 'AVAILMON'"))) 
    {
	return;
    }

    if ( !(numrows = ssdbGetNumRecords(Error, Request))) return;
    ssdbRequestSeek(Error, Request, 0, 0);
    for ( i = 0; i < numrows; i++ ) {
	runcommand = 0;
	vector = ssdbGetRow(Error, Request);
	if ( vector ) {
	    if ( !strcmp(vector[0], "autoemail") ) {
		if ( vector[1] && vector[1][0] != '\0' && atoi(vector[1]) != av_autoemail)
		{
		    /* Detected a change in autoemail from saved one !! Send report */
		    if ( av_autoemail ) snprintf(systemcmd, sizeof(systemcmd), "/usr/etc/amformat -A -a -r 2>&1 >/dev/null &");
		    else snprintf(systemcmd, sizeof(systemcmd), "/usr/etc/amformat -A -a -d 2>&1 >/dev/null &");
		    runcommand = 1;
		}
	    } else if ( !strcmp(vector[0], "shutdownreason") ) {
		if ( vector[1] && vector[1][0] != '\0' && atoi(vector[1]) != av_shutdown ) {
		    snprintf(systemcmd, sizeof(systemcmd), "echo %d > /var/adm/avail/.save/shutdownreason 2>/dev/null" , 
							    av_shutdown);
		    runcommand = 1;
		}
	    } else if ( !strcmp(vector[0], "tickerd") && !tickerddone ) {
		if ( vector[1] && vector[1][0] != '\0' && atoi(vector[1]) != av_uptimed ) {
		    snprintf(systemcmd, sizeof(systemcmd), "/usr/etc/eventmond -a off 2>/dev/null");
		    if ( av_daysupdate > 0 ) {
			system(systemcmd);
			if ( av_uptimed ) {
			    snprintf(systemcmd, sizeof(systemcmd), "/usr/etc/eventmond -a on -j %d -e %d 2>/dev/null",
						   (av_tickduration > 0 ? av_tickduration:300), av_daysupdate*24 );
			    runcommand = 1;
			} else 
			    runcommand = 0;
		    } else {
			system(systemcmd);
		    }
                    tickerddone = 1;
		}
            } else if ( !strcmp(vector[0], "statusinterval") && !tickerddone ) {
		if ( vector[1] && vector[1][0] != '\0' && atoi(vector[1]) != av_daysupdate) {
		    snprintf(systemcmd, sizeof(systemcmd), "/usr/etc/eventmond -a off 2>/dev/null");
		    if ( av_daysupdate > 0 ) {
		        system(systemcmd);
		        if ( av_uptimed ) {
			    snprintf(systemcmd, sizeof(systemcmd), "/usr/etc/eventmond -a on -j %d -e %d  2>/dev/null",
					           (av_tickduration > 0 ? av_tickduration:300), av_daysupdate*24 );
                            runcommand = 1;
                        } else runcommand = 0;
		    } else {
		        system(systemcmd);
		    }
		    tickerddone = 1;
		}
	    }
	}

	if ( runcommand ) {
            system(systemcmd);
	}
    }

    ssdbFreeRequest(Error, Request);
    return;
}

void  display_param_missing_errors ( const char *szParam )
{
      Body("<HTML>\n");
      Body("<HEAD>\n");
      Body("<TITLE>\n");
      Body("Embedded Support Partner - ver.1.0</TITLE>\n");
      Body("</HEAD>\n");
      Body("<BODY BGCOLOR=\"#e3e6d8\">\n");
      FormatedBody("<pre>   SETUP &gt; Availmon Monitor &gt; Configuration</pre>");
      Body("<hr width=100%>\n");
      FormatedBody("<p>Parameter &quot;%s&quot; is missing or empty.",szParam);
      Body("</BODY>\n");
      Body("</HTML>\n");
}

void  display_param_str_errors ( const char *szParam )
{
      Body("<HTML>\n");
      Body("<HEAD>\n");
      Body("<TITLE>\n");
      Body("Embedded Support Partner - ver.1.0</TITLE>\n");
      Body("</HEAD>\n");
      Body("<BODY BGCOLOR=\"#e3e6d8\">\n");
      FormatedBody("<pre>   SETUP &gt; Availmon Monitor &gt; Configuration</pre>");
      Body("<hr width=100%>\n");
      FormatedBody("<p>&quot;%s&quot; should not contain '&quot;' character",szParam);
      Body("</BODY>\n");
      Body("</HTML>\n");
}

void  display_param_int_errors ( mySession *sess, const char *szParam, const char *szLimit )
{
      Body("<HTML>\n");
      Body("<HEAD>\n");
      Body("<TITLE>\n");
      Body("Embedded Support Partner - ver.1.0</TITLE>\n");
      Body("</HEAD>\n");
      Body("<BODY BGCOLOR=\"#e3e6d8\">\n");
      FormatedBody("<pre>   SETUP &gt; Availmon Monitor &gt; Configuration</pre>");
      Body("<hr width=100%>\n");
      FormatedBody("<p>Parameter &quot;%s&quot; must be %s",szParam,szLimit);
      Body("</BODY>\n");
      Body("</HTML>\n");
}

int CheckInputFieldForBlank (CMDPAIR *cmdp, const char * szFieldName, const char *szParamName )
{
  int idx;
  
  idx = sscFindPairByKey ( cmdp, 0, szFieldName );
  if ( idx >= 0 )
  {
     if ( IsStrBlank ( cmdp[idx].value ))
     {
        display_param_missing_errors ( szParamName );
        return 0;
     }
  } 
  return 1;
}

int CheckInputFieldForBadChars (CMDPAIR *cmdp, const char * szFieldName, const char *szParamName )
{
  int idx;
  
  idx = sscFindPairByKey ( cmdp, 0, szFieldName );
  if ( idx >= 0 )
  {
    if ( IsAnyBadChars ( cmdp[idx].value ))
    {
      display_param_str_errors ( szParamName );
      return 0 ;
    }
  }
  return 1;
}

/* End of  Some Lynx Related Helpers */

void availmon_config(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle)
{
	ssdb_Request_Handle req_handle;
        int number_of_records;
        const char **vector;
        
	help_create(hError,"setup_availmon_configuration");
	
	if ( session->textonly == 0 )
	{
	TableBegin("border=0 cellpadding=0 cellspacing=0");
                RowBegin("");
                        CellBegin("");
                                Body("Automatic e-mail distribution:\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
	}	
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select option_default from tool where tool_option = 'autoemail' and tool_name='AVAILMON'")))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
				{
					sscError(hError,"Configuration Missing for availmon\n");
					ssdbFreeRequest(error_handle,req_handle);
					return;
				}
				vector = ssdbGetRow(error_handle,req_handle);
				if(vector)
				{
				    if ( session->textonly == 0 )
				    {
					if (atoi(vector[0]))
					{
						Body("<input type=\"radio\" name=av_autoemail value=1 checked>\n");
						CellEnd();
						CellBegin("");
							Body("&nbsp;&nbsp;\n");
						CellEnd();
						CellBegin("");
							Body("Enable\n");
						CellEnd();
						CellBegin("");
							 Body("&nbsp;&nbsp;&nbsp;\n");
						CellEnd();
						CellBegin("");
							Body("<input type=\"radio\" name=av_autoemail value=0>\n");
						CellEnd();
                                                CellBegin("");
                                                        Body("&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("Disable\n");
						CellEnd();
					}
					else
					{
						Body("<input type=\"radio\" name=av_autoemail value=1>\n");
						CellEnd();
                                                CellBegin("");
                                                        Body("&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("Enable\n");
                                                CellEnd();
                                                CellBegin("");
                                                         Body("&nbsp;&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("<input type=\"radio\" name=av_autoemail value=0 checked>\n");
						CellEnd();
                                                CellBegin("");
                                                        Body("&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("Disable\n");
                                                CellEnd();
					}
				    }
				    else
				    {
					if (atoi(vector[0]))
					  FormatedBody(szLynxAutoEmailRadioFormat,"checked","");
					else
					  FormatedBody(szLynxAutoEmailRadioFormat,"","checked");
				    }	
				    
				} 
				ssdbFreeRequest(error_handle,req_handle);	
				
	if ( session->textonly == 0 )
	{
		RowEnd();
		RowBegin("");
			CellBegin("colspan=7");
				Body("&nbsp;\n");
			CellEnd();
		RowEnd();
                RowBegin("");
                        CellBegin("");
                                Body("Display reason for shutdown:\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
	} 
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select option_default from tool where tool_option = 'shutdownreason' and tool_name='AVAILMON'")))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
				{
					sscError(hError,"Configuration Missing for availmon\n");
					ssdbFreeRequest(error_handle,req_handle);
					return;
				}
				vector = ssdbGetRow(error_handle,req_handle);
				if(vector)
				{
				    if ( session->textonly == 0 )
				    {
					if (atoi(vector[0]))
					{
						Body("<input type=\"radio\" name=av_shutdown value=1 checked>\n");
						CellEnd();
						CellBegin("");
							Body("&nbsp;&nbsp;\n");
						CellEnd();
						CellBegin("");
							Body("Enable\n");
						CellEnd();
						CellBegin("");
							 Body("&nbsp;&nbsp;&nbsp;\n");
						CellEnd();
						CellBegin("");
							Body("<input type=\"radio\" name=av_shutdown value=0>\n");
						CellEnd();
                                                CellBegin("");
                                                        Body("&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("Disable\n");
						CellEnd();
					}
					else
					{
						Body("<input type=\"radio\" name=av_shutdown value=1>\n");
						CellEnd();
                                                CellBegin("");
                                                        Body("&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("Enable\n");
                                                CellEnd();
                                                CellBegin("");
                                                         Body("&nbsp;&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("<input type=\"radio\" name=av_shutdown value=0 checked>\n");
						CellEnd();
                                                CellBegin("");
                                                        Body("&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("Disable\n");
                                                CellEnd();
					}
				    } 
				    else
				    {
					if (atoi(vector[0]))
					  FormatedBody(szLynxShutDownRadioFormat,"checked","");
					else
					  FormatedBody(szLynxShutDownRadioFormat,"","checked");
				    }
				}
				ssdbFreeRequest(error_handle,req_handle);
	if ( session->textonly == 0 )
	{
		RowEnd();
		RowBegin("");
			CellBegin("colspan=7");
				Body("&nbsp;\n");
			CellEnd();
		RowEnd();
                RowBegin("");
                        CellBegin("");
                                Body("Include HINV information in the e-mail:\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
	} 
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select option_default from tool where tool_option = 'hinvupdate' and tool_name='AVAILMON'")))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
				{
					sscError(hError,"Configuration Missing for availmon\n");
					ssdbFreeRequest(error_handle,req_handle);
					return;
				}
				vector = ssdbGetRow(error_handle,req_handle);
				if(vector)
				{
				    if ( session->textonly == 0 )
				    {
					if (atoi(vector[0]))
					{
						Body("<input type=\"radio\" name=av_hinv value=1 checked>\n");
						CellEnd();
						CellBegin("");
							Body("&nbsp;&nbsp;\n");
						CellEnd();
						CellBegin("");
							Body("Yes\n");
						CellEnd();
						CellBegin("");
							 Body("&nbsp;&nbsp;&nbsp;\n");
						CellEnd();
						CellBegin("");
							Body("<input type=\"radio\" name=av_hinv value=0>\n");
						CellEnd();
                                                CellBegin("");
                                                        Body("&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("No\n");
						CellEnd();
					}
					else
					{
						Body("<input type=\"radio\" name=av_hinv value=1>\n");
						CellEnd();
                                                CellBegin("");
                                                        Body("&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("Yes\n");
                                                CellEnd();
                                                CellBegin("");
                                                         Body("&nbsp;&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("<input type=\"radio\" name=av_hinv value=0 checked>\n");
						CellEnd();
                                                CellBegin("");
                                                        Body("&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("No\n");
                                                CellEnd();
					}
				    } 
				    else
				    {
					if (atoi(vector[0]))
					  FormatedBody(szLynxHinvRadioFormat,"checked","");
					else
					  FormatedBody(szLynxHinvRadioFormat,"","checked");
				    }
				}
				ssdbFreeRequest(error_handle,req_handle);	
				
#if 0
	if ( session->textonly == 0 )
	{
		RowEnd();
		RowBegin("");
			CellBegin("colspan=7");
				Body("&nbsp;\n");
			CellEnd();
		RowEnd();
                RowBegin("");
                        CellBegin("");
                                Body("Capture important system messages:\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
	}
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select option_default from tool where tool_option = 'livenotification' and tool_name='AVAILMON'")))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
				{
					sscError(hError,"Configuration Missing for availmon\n");
					ssdbFreeRequest(error_handle,req_handle);
					return;
				}
				vector = ssdbGetRow(error_handle,req_handle);
				if(vector)
				{
				    if ( session->textonly == 0 )
				    {
					if (atoi(vector[0]))
					{
						Body("<input type=\"radio\" name=av_sysmsg value=1 checked>\n");
						CellEnd();
						CellBegin("");
							Body("&nbsp;&nbsp;\n");
						CellEnd();
						CellBegin("");
							Body("Enable\n");
						CellEnd();
						CellBegin("");
							 Body("&nbsp;&nbsp;&nbsp;\n");
						CellEnd();
						CellBegin("");
							Body("<input type=\"radio\" name=av_sysmsg value=0>\n");
						CellEnd();
                                                CellBegin("");
                                                        Body("&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("Disable\n");
						CellEnd();
					}
					else
					{
						Body("<input type=\"radio\" name=av_sysmsg value=1>\n");
						CellEnd();
                                                CellBegin("");
                                                        Body("&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("Enable\n");
                                                CellEnd();
                                                CellBegin("");
                                                         Body("&nbsp;&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("<input type=\"radio\" name=av_sysmsg value=0 checked>\n");
						CellEnd();
                                                CellBegin("");
                                                        Body("&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("Disable\n");
                                                CellEnd();
					}
				    } 
				    else
				    {
					if (atoi(vector[0]))
					  FormatedBody(szLynxSysMsgRadioFormat,"checked","");
					else
					  FormatedBody(szLynxSysMsgRadioFormat,"","checked");
				    }
				}
				ssdbFreeRequest(error_handle,req_handle);	
#endif
	if ( session->textonly == 0 )
	{
		RowEnd();
		RowBegin("");
			CellBegin("colspan=7");
				Body("&nbsp;\n");
			CellEnd();
		RowEnd();
                RowBegin("");
                        CellBegin("");
                                Body("Start uptime daemon:\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
	} 
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select option_default from tool where tool_option = 'tickerd' and tool_name='AVAILMON'")))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
				{
					sscError(hError,"Configuration Missing for availmon\n");
					ssdbFreeRequest(error_handle,req_handle);
					return;
				}
				vector = ssdbGetRow(error_handle,req_handle);
				if(vector)
				{
				    if ( session->textonly == 0 )
				    {
					if (atoi(vector[0]))
					{
						Body("<input type=\"radio\" name=av_uptimed value=1 checked>\n");
						CellEnd();
						CellBegin("");
							Body("&nbsp;&nbsp;\n");
						CellEnd();
						CellBegin("");
							Body("Yes\n");
						CellEnd();
						CellBegin("");
							 Body("&nbsp;&nbsp;&nbsp;\n");
						CellEnd();
						CellBegin("");
							Body("<input type=\"radio\" name=av_uptimed value=0>\n");
						CellEnd();
                                                CellBegin("");
                                                        Body("&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("No\n");
						CellEnd();
					}
					else
					{
						Body("<input type=\"radio\" name=av_uptimed value=1>\n");
						CellEnd();
                                                CellBegin("");
                                                        Body("&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("Yes\n");
                                                CellEnd();
                                                CellBegin("");
                                                         Body("&nbsp;&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("<input type=\"radio\" name=av_uptimed value=0 checked>\n");
						CellEnd();
                                                CellBegin("");
                                                        Body("&nbsp;&nbsp;\n");
                                                CellEnd();
                                                CellBegin("");
                                                        Body("No\n");
                                                CellEnd();
					}
				    } 
				    else
				    {
					if (atoi(vector[0]))
					  FormatedBody(szLynxUpTimeRadioFormat,"checked","");
					else
					  FormatedBody(szLynxUpTimeRadioFormat,"","checked");
				    }
				}
				ssdbFreeRequest(error_handle,req_handle);	
	if ( session->textonly == 0 )
	{
		RowEnd();
		RowBegin("");
			CellBegin("colspan=7");
				Body("&nbsp;\n");
			CellEnd();
		RowEnd();
                RowBegin("");
                        CellBegin("");
                                Body("Number of days between status updates (default = 60) (0 - 300):\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("colspan=5");
       } 
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select option_default from tool where tool_option = 'statusinterval' and tool_name='AVAILMON'")))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
				{
					sscError(hError,"Configuration Missing for availmon\n");
					ssdbFreeRequest(error_handle,req_handle);
					return;
				}
				vector = ssdbGetRow(error_handle,req_handle);
				if(vector)
				{
					if ( session->textonly == 0 )
					{
						FormatedBody("<input type=\"text\" name=av_daysupdate size=10 value=\"%s\"> days",vector[0]);
						CellEnd();
					} 
					else
					{
						FormatedBody(szLynxNumDaysInputFormat,vector[0]);
					}	
				}
				ssdbFreeRequest(error_handle,req_handle);	

				if ( session->textonly != 0 ) {
				    Body("<pre>   (0 - 300)</pre>\n");
				}
#if 0	
	if ( session->textonly == 0 )
	{
		RowEnd();
		RowBegin("");
			CellBegin("colspan=7");
				Body("&nbsp;\n");
			CellEnd();
		RowEnd();
		RowBegin("");
                        CellBegin("");
                                Body("Filename for last uptime write (default = /var/adm/avail/.save/lasttick):\n");
                        CellEnd();
                        CellBegin("");
                                Body("&nbsp;&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("colspan=5");
       } 
       
                                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                                "select option_default from tool where tool_option = 'tickfile' and tool_name='AVAILMON'")))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        return;
                                }
                                if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                                {
                                        sscError(hError,"Configuration Missing for availmon\n");
                                        ssdbFreeRequest(error_handle,req_handle);
                                        return;
                                }
                                vector = ssdbGetRow(error_handle,req_handle);
                                if(vector)
                                {
					if ( session->textonly == 0 )
					{
                                                FormatedBody("<input type=\"text\" name=av_lasttick size=25 value=\"%s\">",vector[0]);
                                                CellEnd();
                                        } 
                                        else
                                        {
                                                FormatedBody("<p><pre>   Filename for last uptime write        :<input type=\"text\" name=av_lasttick size=30 value=\"%s\"></pre>",vector[0]);
                                        }
                                }
                                ssdbFreeRequest(error_handle,req_handle);
#endif
	if ( session->textonly == 0 )
	{
                RowEnd();
                RowBegin("");
                        CellBegin("colspan=7");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
		RowBegin("");
                        CellBegin("");
                                Body("Interval in seconds between uptime checks (default = 300 seconds):\n");
                        CellEnd();
                        CellBegin("");
                                Body("&nbsp;&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("colspan=5");
        }                        
                                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                                "select option_default from tool where tool_option = 'tickduration' and tool_name='AVAILMON'")))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        return;
                                }
                                if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                                {
                                        sscError(hError,"Configuration Missing for availmon\n");
                                        ssdbFreeRequest(error_handle,req_handle);
                                        return;
                                }
                                vector = ssdbGetRow(error_handle,req_handle);
                                if(vector)
                                {
					if ( session->textonly == 0 )
					{
                                                FormatedBody("<input type=\"text\" name=av_tickduration size=10 value=\"%s\"> seconds",vector[0]);
                                                CellEnd();
                                        }
                                        else
                                        {
                                                FormatedBody(szLynxUptimeCheckFormat,vector[0]);
                                        }        
                                }
                                ssdbFreeRequest(error_handle,req_handle);
	if ( session->textonly == 0 )
	{
                RowEnd();
                RowBegin("");
                        CellBegin("colspan=7");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
		TableEnd();
	}
	else 
	{
                Body("<pre>   (default - 300 seconds)  </pre>\n");
	}	
}

void availmon_config_confirm(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{

	ssdb_Request_Handle req_handle;
        int number_of_records;
        const char **vector;
	char av_autoemail[10],av_shutdown[10],av_hinv[10],av_sysmsg[10],av_uptimed[10];
	char av_daysupdate[10],av_lasttick[255],av_tickduration[10],common_string[2];
	int  common_int,pageselect;
	int  EnblVal;
	char VectBuf[256];

	if (!get_variable(hError,cmdp,INTTYPE,"pageselect",common_string,&pageselect,600))
		return;

	if(!pageselect)
		 help_create(hError,"setup_availmon_viewcurr"); 
		 
	if(pageselect)
	{
                if ( session->textonly )
                { /* we must validate input parameters and generate page if error */
                  int idx = 0;                 
                
                  idx = sscFindPairByKey ( cmdp, 0, "av_daysupdate" );
                  if ( idx >= 0 )
                  { /* Found let's try */
                    if ( IsStrBlank ( cmdp[idx].value ))
                    {
                       display_param_missing_errors ( "Number of days between status updates" );
                       return ;
                    }
                    if ( !IsValidInt ( cmdp[idx].value, 0, 300 ))
                    {
                       display_param_int_errors ( session, "Number of days between status updates", "non-negative, less then 300 integer value." );
                       return;
                    }
                  }

                  idx = sscFindPairByKey ( cmdp, 0, "av_tickduration" );
                  if ( idx >= 0 )
                  { /* Found let's try */
                    if ( IsStrBlank ( cmdp[idx].value ))
                    {
                       display_param_missing_errors ( "Interval between update check" );
                       return ;
                    }
                    if ( !IsValidInt ( cmdp[idx].value, 0, 600 ))
                    {
                       display_param_int_errors ( session, "Interval between update check", "non-negative, less then 600 integer value." );
                       return;
                    }
                  }
#if 0
                  idx = sscFindPairByKey ( cmdp, 0, "av_lasttick" );
                  if ( idx >= 0 )
                  {
                    if ( IsStrBlank ( cmdp[idx].value ))
                    {
                       display_param_missing_errors ( "Filename for last uptime write" );
                       return ;
                    }
                    if ( !CheckInputFieldForBadChars(cmdp, "av_lasttick", "Filename for last uptime write" ))
                          return;
                  }
#endif
                }   
	
		if (!get_variable(hError,cmdp,CHARTYPE,"av_autoemail",av_autoemail,&common_int,601))
			return;
		if (!get_variable(hError,cmdp,CHARTYPE,"av_shutdown",av_shutdown,&common_int,602))
			return;
		if (!get_variable(hError,cmdp,CHARTYPE,"av_hinv",av_hinv,&common_int,603))
			return;
#if 0
		if (!get_variable(hError,cmdp,CHARTYPE,"av_sysmsg",av_sysmsg,&common_int,604))
			return;
#endif
		if (!get_variable(hError,cmdp,CHARTYPE,"av_uptimed",av_uptimed,&common_int,605))
			return;
		if (!get_variable(hError,cmdp,CHARTYPE,"av_daysupdate",av_daysupdate,&common_int,606))
			return;
#if 0
		if (!get_variable(hError,cmdp,CHARTYPE,"av_lasttick",av_lasttick,&common_int,607))
			return;
#endif
		if (!get_variable(hError,cmdp,CHARTYPE,"av_tickduration",av_tickduration,&common_int,608))
			return;

		/* Added by sri to update availmon daemon real-time before configuration is written to DB */

                updatert_availmon(error_handle, connection, atoi(av_autoemail), atoi(av_shutdown), atoi(av_uptimed),
                                  atoi(av_daysupdate), atoi(av_tickduration));

		if(!(ssdbLockTable(error_handle,connection,"tool")))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}

		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
		"update tool set option_default = '%s' where tool_name = 'AVAILMON' and tool_option='autoemail'",av_autoemail)))
		{	
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		ssdbFreeRequest(error_handle,req_handle);
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
		"update tool set option_default = '%s' where tool_name = 'AVAILMON' and tool_option='shutdownreason'",av_shutdown)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		ssdbFreeRequest(error_handle,req_handle);
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
		"update tool set option_default = '%s' where tool_name = 'AVAILMON' and tool_option='hinvupdate'",av_hinv)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		ssdbFreeRequest(error_handle,req_handle);
#if 0
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
		"update tool set option_default = '%s' where tool_name = 'AVAILMON' and tool_option='livenotification'",av_sysmsg)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		ssdbFreeRequest(error_handle,req_handle);
#endif
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
		"update tool set option_default = '%s' where tool_name = 'AVAILMON' and tool_option='tickerd'",av_uptimed)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		ssdbFreeRequest(error_handle,req_handle);
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
		"update tool set option_default = '%s' where tool_name = 'AVAILMON' and tool_option='statusinterval'",av_daysupdate)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		ssdbFreeRequest(error_handle,req_handle);
#if 0
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
		"update tool set option_default = '%s' where tool_name = 'AVAILMON' and tool_option='tickfile'",av_lasttick)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		ssdbFreeRequest(error_handle,req_handle);
#endif
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
		"update tool set option_default = '%s' where tool_name = 'AVAILMON' and tool_option='tickduration'",av_tickduration)))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		ssdbFreeRequest(error_handle,req_handle);

               if ( session->textonly == 0)
               {
		Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE>\n");
		Body("</HEAD>\n");
		Body("<body bgcolor=\"#ffffcc\" link=\"#333300\" vlink=\"#333300\">\n");
		Body("<form method=POST>\n");
		TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
			RowBegin("");
				CellBegin("bgcolor=\"#cccc99\" width=15");
					Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
				CellEnd();
				CellBegin("bgcolor=\"#cccc99\"");
					Body("SETUP &gt;  Availmon Monitor &gt; Configuration\n");
				CellEnd();
			RowEnd();
			RowBegin("");
				CellBegin("colspan=2");
					Body("&nbsp;<p>&nbsp;\n");
				CellEnd();
			RowEnd();
			RowBegin("");
				CellBegin("");
					Body("&nbsp;\n");
				CellEnd();
				CellBegin("");
	        } else {
		Body("<HTML> <HEAD> <TITLE>SGI Embedded Support Partner - ver.1.0</TITLE></HEAD>\n");
		Body("<body bgcolor=\"#ffffcc\" link=\"#333300\" vlink=\"#333300\">\n");
		Body("<pre>   SETUP &gt;  Availmon Monitor &gt; Configuration</pre>\n");
		Body("<hr width=100%>\n");
	        }			
	        
	} /* end of if(pageselect) */
	

        EnblVal = -1;
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
	"select option_default from tool where tool_option = 'autoemail' and tool_name='AVAILMON'")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
	{
		sscError(hError,"Configuration Missing for availmon\n");
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		ssdbFreeRequest(error_handle,req_handle);
		return;
	}
	vector = ssdbGetRow(error_handle,req_handle);
	if(vector)
	{
	   EnblVal = atoi(vector[0]);
	} 
	ssdbFreeRequest(error_handle,req_handle);

        if ( session->textonly == 0 )
        {
	TableBegin("border=0 cellpadding=0 cellspacing=0");
		RowBegin("");
			CellBegin("");
				Body("Automatic e-mail distribution\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");

		        if( EnblVal != -1 )
		        {
		          if ( EnblVal )  
		   	     Body("Enabled\n");
		          else
		             Body("Disabled\n");
		        }     
			CellEnd();
		RowEnd();
	} else {
	  /* Lynx */
          if( EnblVal != -1 )
	      FormatedBody  ( "<pre>   Automatic e-mail distribution           : %s</pre>", EnblVal ? "Enabled" : "Disabled" );
	  else    
	      FormatedBody  ( "<pre>   Automatic e-mail distribution           : </pre>" );
	}	

        EnblVal = -1;
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
	"select option_default from tool where tool_option = 'shutdownreason' and tool_name='AVAILMON'")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
	{
		sscError(hError,"Configuration Missing for availmon\n");
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		ssdbFreeRequest(error_handle,req_handle);
		return;
	}
	vector = ssdbGetRow(error_handle,req_handle);
	if(vector)
	{
	   EnblVal = atoi(vector[0]);
	}
	ssdbFreeRequest(error_handle,req_handle);

		
        if ( session->textonly == 0 )
        {
		RowBegin("");
			CellBegin("colspan=3");
				Body(" &nbsp;\n");
			CellEnd();
		RowEnd();
		RowBegin("");
			CellBegin("");
				Body("Display reason for shutdown\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");

		        if( EnblVal != -1 )
		        {
		          if ( EnblVal )  
		   	     Body("Enabled\n");
		          else
		             Body("Disabled\n");
		        }     
			CellEnd();
		RowEnd();
	} else {
	  /* Lynx */
          if( EnblVal != -1 )
	      FormatedBody  ( "<pre>   Display reason for shutdown             : %s</pre>", EnblVal ? "Enabled" : "Disabled" );
	  else    
	      FormatedBody  ( "<pre>   Display reason for shutdown             : </pre>" );
	}	
		


        EnblVal = -1;
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
	"select option_default from tool where tool_option = 'hinvupdate' and tool_name='AVAILMON'")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
	{
		sscError(hError,"Configuration Missing for availmon\n");
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		ssdbFreeRequest(error_handle,req_handle);
		return;
	}
	vector = ssdbGetRow(error_handle,req_handle);
	if(vector)
	{
           EnblVal = atoi(vector[0]);
	}
	ssdbFreeRequest(error_handle,req_handle);
		
        if ( session->textonly == 0 )
        {
		RowBegin("");
			CellBegin("colspan=3");
				Body(" &nbsp;\n");
			CellEnd();
		RowEnd();
		RowBegin("");
			CellBegin("");
				Body("Include HINV information in the e-mail\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
			
		        if( EnblVal != -1 )
		        {
		          if ( EnblVal )  
		   	     Body("Yes\n");
		          else
		             Body("No\n");
		        }     
		        
			CellEnd();
		RowEnd();
       } else {
	  /* Lynx */
          if( EnblVal != -1 )
	      FormatedBody  ( "<pre>   Include HINV information in the e-mail : %s</pre>", EnblVal ? "Yes" : "No" );
	  else    
	      FormatedBody  ( "<pre>   Include HINV information in the e-mail : </pre>" );
       }	
#if 0
		
        EnblVal = -1;		
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
	"select option_default from tool where tool_option = 'livenotification' and tool_name='AVAILMON'")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
	{
		sscError(hError,"Configuration Missing for availmon\n");
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		ssdbFreeRequest(error_handle,req_handle);
		return;
	}
	vector = ssdbGetRow(error_handle,req_handle);
	if(vector)
	{
		EnblVal = atoi(vector[0]);
	}       
	ssdbFreeRequest(error_handle,req_handle);
        if ( session->textonly == 0 )
        {
		RowBegin("");
			CellBegin("colspan=3");
				Body(" &nbsp;\n");
			CellEnd();
		RowEnd();
		RowBegin("");
			CellBegin("");
				Body("Capture important system messages\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
			
		        if( EnblVal != -1 )
		        {
		          if ( EnblVal )  
		   	     Body("Enabled\n");
		          else
		             Body("Disabled\n");
		        }     

			CellEnd();
		RowEnd();
       } else {
	  /* Lynx */
          if( EnblVal != -1 )
	      FormatedBody  ( "<pre>   Capture important system messages       : %s</pre>", EnblVal ? "Enabled" : "Disabled" );
	  else    
	      FormatedBody  ( "<pre>   Capture important system messages       : </pre>" );
       }	

#endif
        EnblVal  = -1;
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
	"select option_default from tool where tool_option = 'tickerd' and tool_name='AVAILMON'")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
	{
		sscError(hError,"Configuration Missing for availmon\n");
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		ssdbFreeRequest(error_handle,req_handle);
		return;
	}
	vector = ssdbGetRow(error_handle,req_handle);
	if(vector)
	{
           EnblVal = atoi(vector[0]);
	}       
	ssdbFreeRequest(error_handle,req_handle);

	
        if ( session->textonly == 0 )
        {
		RowBegin("");
			CellBegin("colspan=3");
				Body(" &nbsp;\n");
			CellEnd();
		RowEnd();
		RowBegin("");
			CellBegin("");
				Body("Start uptime daemon\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
			
		        if( EnblVal != -1 )
		        {
		          if ( EnblVal )  
		   	     Body("Yes\n");
		          else
		             Body("No\n");
		        }     

			CellEnd();
		RowEnd();
        } else {
	  /* Lynx */
          if( EnblVal != -1 )
	      FormatedBody  ( "<pre>   Start uptime daemon                     : %s</pre>", EnblVal ? "Yes" : "No" );
	  else    
	      FormatedBody  ( "<pre>   Start uptime daemon                     : </pre>" );
        }	
		

	VectBuf[0] = 0;
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
	"select option_default from tool where tool_option = 'statusinterval' and tool_name='AVAILMON'")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
	{
		sscError(hError,"Configuration Missing for availmon\n");
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		ssdbFreeRequest(error_handle,req_handle);
		return;
	}
	vector = ssdbGetRow(error_handle,req_handle);
	if(vector)
	{
	   strncpy ( VectBuf, vector[0], sizeof(VectBuf));
           VectBuf [sizeof(VectBuf)-1] = 0;
	}	
	ssdbFreeRequest(error_handle,req_handle);

		
        if ( session->textonly == 0 )
        {
		RowBegin("");
			CellBegin("colspan=3");
				Body(" &nbsp;\n");
			CellEnd();
		RowEnd();
		RowBegin("");
			CellBegin("");
				Body("Number of days between status updates\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
		                FormatedBody("%s",VectBuf);
			CellEnd();
		RowEnd();
		
        } else {
          FormatedBody  ( "<pre>   Number of days between status updates   : %s</pre>", VectBuf );
        }


	VectBuf[0] = 0;
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
	"select option_default from tool where tool_option = 'tickduration' and tool_name='AVAILMON'")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
	{
		sscError(hError,"Configuration Missing for availmon\n");
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		ssdbFreeRequest(error_handle,req_handle);
		return;
	}
	VectBuf[0] = 0;
	vector = ssdbGetRow(error_handle,req_handle);
	if(vector)
	{
	   strncpy ( VectBuf, vector[0], sizeof(VectBuf));
           VectBuf [sizeof(VectBuf)-1] = 0;
	}
	ssdbFreeRequest(error_handle,req_handle);
		
		
        if ( session->textonly == 0 )
        {
		RowBegin("");
			CellBegin("colspan=3");
				Body(" &nbsp;\n");
			CellEnd();
		RowEnd();
		RowBegin("");
			CellBegin("");
				Body("Interval in seconds between uptime checks\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
                        FormatedBody("%s",VectBuf);
			CellEnd();
		RowEnd();
        } else {
          FormatedBody  ( "<pre>   Interval in seconds between uptime checks: %s</pre>", VectBuf );
        }

#if 0
	VectBuf[0] = 0;
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
	"select option_default from tool where tool_option = 'tickfile' and tool_name='AVAILMON'")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
	{
		sscError(hError,"Configuration Missing for availmon\n");
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		ssdbFreeRequest(error_handle,req_handle);
		return;
	}
	vector = ssdbGetRow(error_handle,req_handle);
	if(vector)
	{
	   strncpy ( VectBuf, vector[0], sizeof(VectBuf));
           VectBuf [sizeof(VectBuf)-1] = 0;
	}
	ssdbFreeRequest(error_handle,req_handle);
		
		
        if ( session->textonly == 0 )
        {
		RowBegin("");
			CellBegin("colspan=3");
				Body(" &nbsp;\n");
			CellEnd();
		RowEnd();
		RowBegin("");
			CellBegin("");
				Body("Filename for last uptime write\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
                        FormatedBody("%s",VectBuf);

			CellEnd();
		RowEnd();
		
        } else {
          FormatedBody  ( "<pre>   Filename for last uptime write    : %s</pre>", VectBuf );
        }
		
#endif
	if (!ssdbUnLockTable(error_handle,connection))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
	if(pageselect)
	{
               if ( session->textonly == 0)
               {
		 TableEnd();
		 CellEnd();
		 RowEnd();
		 TableEnd();
		 Body("</form></body></html>\n");
	       } else {
         	 Body("</body></html>\n");
	       }
	}
	if(!pageselect)
	{
               if ( session->textonly == 0 )
               {
		RowBegin("");
			CellBegin("");
				Body("&nbsp;\n");
			CellEnd();
		RowEnd();
	       } else {
                Body("<br>	    \n");
	       }
	       
	    availmon_mail_confirm_html(hError,session,connection,error_handle,pageselect);
	}
}

void availmon_mail(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmd)
{
        ssdb_Request_Handle req_handle;
        int number_of_records,rec_sequence;
        const char **vector;
	char email_list[1024];

	help_create(hError,"setup_availmon_email");
	if ( session->textonly == 0 )
	{
	Body("<table border=0 cellpadding=0 cellspacing=0>\n");
	RowBegin("");
		CellBegin("colspan=3");
			Body("<b>E-mail list for availability report:</b>\n");
		CellEnd();
	RowEnd();
	RowBegin("");
		CellBegin("colspan=3");
			Body("&nbsp;\n");
		CellEnd();
	RowEnd();
	RowBegin("");
		CellBegin("");
			Body("Enter e-mail addresses that receive availability report in text form:	\n");
		CellEnd();
		CellBegin("");
			Body("&nbsp;&nbsp;&nbsp;\n");
		CellEnd();
		CellBegin("");
	} 
	else 		
	{
 		Body("<p><pre>   <b>E-mail list for availability report:</b></pre>\n");
		Body("<p><pre>   Enter e-mail addresses that receive availability report</pre>\n");
	}
			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
			"select option_default from tool where tool_option = 'AVAILABILITY' and tool_name='AVAILMON'")))
			{
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}
			if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
			{
				ssdbFreeRequest(error_handle,req_handle);
			        if ( session->textonly == 0 )	
			        {
				 Body("<input type=\"text\" name=av_text size=25>\n");
				} 
				else
				{
			         FormatedBody(szLynxInTextEmailFormat,"");
				}
			}
			else
			{
				for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
				{
					if(rec_sequence > 0)
						strcat(email_list,",");
					else
						memset(email_list,0,1024);

					vector = ssdbGetRow(error_handle,req_handle);
					if(vector)
					{
						strcat(email_list,vector[0]);
					}
				}
				ssdbFreeRequest(error_handle,req_handle);	
				
			        if ( session->textonly == 0 )	
			        {
				   FormatedBody("<input type=\"text\" name=av_text size=25 value=\"%s\">",email_list);
			        } 
			        else 
			        {
			           FormatedBody(szLynxInTextEmailFormat,email_list);
			        }
			     
			}
			
	if ( session->textonly == 0 )
	{
		CellEnd();
	RowEnd();
	RowBegin("");
                CellBegin("colspan=3");
                        Body("&nbsp;\n");
                CellEnd();
        RowEnd();
	RowBegin("");
                CellBegin("");
                        Body("Enter e-mail addresses that receive availability report in compressed form:\n");
		CellEnd();
                CellBegin("");
                        Body("&nbsp;&nbsp;&nbsp;    \n");
                CellEnd();
                CellBegin("");
        } 
			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                        "select option_default from tool where tool_option = 'AVAILABILITY_COMP' and tool_name='AVAILMON'")))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }
                        if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                        {
				ssdbFreeRequest(error_handle,req_handle);
			        if ( session->textonly == 0 )	
			        {
                                Body("<input type=\"text\" name=av_compressed size=25>\n");
                                }
                                else
                                {
                                FormatedBody(szLynxInCompEmailFormat,"");
                                }
                        }
                        else
                        {
                                for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
                                {
                                        if(rec_sequence > 0)
                                                strcat(email_list,",");
                                        else
                                                memset(email_list,0,1024);

                                        vector = ssdbGetRow(error_handle,req_handle);
                                        if(vector)
                                        {
                                                strcat(email_list,vector[0]);
                                        }
                                }
                                ssdbFreeRequest(error_handle,req_handle);
                                if ( session->textonly == 0 )
                                {
                                FormatedBody("<input type=\"text\" name=av_compressed size=25 value=\"%s\">",email_list);
                                } 
                                else
                                {
                                FormatedBody(szLynxInCompEmailFormat,email_list);
                                }
                        }
	if ( session->textonly == 0 )
	{
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=3");
                        Body("&nbsp;\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("");
                        Body("Enter e-mail addresses that receive availability report in compressed encrypted form:\n");
		CellEnd();
                CellBegin("");
                        Body("&nbsp;&nbsp;&nbsp;    \n");
                CellEnd();
                CellBegin("");
	}                
                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                        "select option_default from tool where tool_option = 'AVAILABILITY_CENCR' and tool_name='AVAILMON'")))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }
                        if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                        {
				ssdbFreeRequest(error_handle,req_handle);
			        if ( session->textonly == 0 )	
			        {
                                Body("<input type=\"text\" name=av_encrypted size=25>\n");
                                }
                                else
                                {
                                FormatedBody(szLynxInEncrEmailFormat,"");
                                }
                        }
                        else
                        {
                                for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
                                {
                                        if(rec_sequence > 0)
                                                strcat(email_list,",");
                                        else
                                                memset(email_list,0,1024);

                                        vector = ssdbGetRow(error_handle,req_handle);
                                        if(vector)
                                        {
                                                strcat(email_list,vector[0]);
                                        }
                                }
                                ssdbFreeRequest(error_handle,req_handle);
 		        	if ( session->textonly == 0 )	
			        {
                                FormatedBody("<input type=\"text\" name=av_encrypted size=25 value=\"%s\">",email_list);
			        } 
			        else
			        {
                                FormatedBody(szLynxInEncrEmailFormat,email_list);
			        }
                                                                
                        }
	if ( session->textonly == 0 )
	{
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=3");
                        Body("&nbsp;\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=3 ");
                        Body("<b>E-mail list for diagnostic report:</b>\n");
		 CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=3");
                        Body("&nbsp;\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("");
			Body("Enter e-mail addresses that receive diagnostic report in text form:\n");
		CellEnd();
                CellBegin("");
                        Body("&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("");
        } 
        else 
        {
        Body("<p><pre>   <b>E-mail list for diagnostic report:</b></pre>\n");
	Body("<p><pre>   Enter e-mail addresses that receive diagnostic report</pre>\n");
        }
                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                        "select option_default from tool where tool_option = 'DIAGNOSTIC' and tool_name='AVAILMON'")))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }
                        if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                        {
				ssdbFreeRequest(error_handle,req_handle);
			        if ( session->textonly == 0 )	
			        {
                                Body("<input type=\"text\" name=av_diag_text size=25>\n");
                                }
                                else
                                {
                                FormatedBody(szLynxInTextDiagFormat,"");
                                }
                        }
                        else
                        {
                                for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
                                {
                                        if(rec_sequence > 0)
                                                strcat(email_list,",");
                                        else
                                                memset(email_list,0,1024);

                                        vector = ssdbGetRow(error_handle,req_handle);
                                        if(vector)
                                        {
                                                strcat(email_list,vector[0]);
                                        }
                                }
                                ssdbFreeRequest(error_handle,req_handle);
                                if (session->textonly == 0 )
                                {
                                FormatedBody("<input type=\"text\" name=av_diag_text size=25 value=\"%s\">",email_list);
                                }
                                else
                                {
                                FormatedBody(szLynxInTextDiagFormat,email_list);
                                }
                        }
        if ( session->textonly == 0 )
        {
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=3");
                        Body("&nbsp;\n");
                CellEnd();
        RowEnd();
	RowBegin("");
                CellBegin("");
                        Body("Enter e-mail addresses that receive diagnostic report in compressed form:\n");
		CellEnd();
                CellBegin("");
                        Body("&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("");
	} 
                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                        "select option_default from tool where tool_option = 'DIAGNOSTIC_COMP' and tool_name='AVAILMON'")))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }
                        if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                        {
				ssdbFreeRequest(error_handle,req_handle);
				if ( session->textonly == 0 )
				{
                                Body("<input type=\"text\" name=av_diag_compressed size=25>\n");
                                }
                                else
                                {
                                FormatedBody(szLynxInCompDiagFormat,"");
                                }
                        }
                        else
                        {
                                for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
                                {
                                        if(rec_sequence > 0)
                                                strcat(email_list,",");
                                        else
                                                memset(email_list,0,1024);

                                        vector = ssdbGetRow(error_handle,req_handle);
                                        if(vector)
                                        {
                                                strcat(email_list,vector[0]);
                                        }
                                }
                                ssdbFreeRequest(error_handle,req_handle);
				if ( session->textonly == 0 )
				{
                                FormatedBody("<input type=\"text\" name=av_diag_compressed size=25 value=\"%s\">",email_list);
                                }
                                else
                                {
                                FormatedBody(szLynxInCompDiagFormat,email_list);
                                }
                        }
	if ( session->textonly == 0 )
	{
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=3");
                        Body("&nbsp;\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("");
                        Body("Enter e-mail addresses that receive diagnostic report in compressed encrypted form:\n");
		CellEnd();
                CellBegin("");
                        Body("&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("");
	}
                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                        "select option_default from tool where tool_option = 'DIAGNOSTIC_CENCR' and tool_name='AVAILMON'")))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }
                        if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                        {
				ssdbFreeRequest(error_handle,req_handle);
				if ( session->textonly == 0)
				{
                                Body("<input type=\"text\" name=av_diag_encrypted  size=25>\n");
                                }
                                else
                                {
                                FormatedBody(szLynxInEncrDiadFormat,"");
                                }
                        }
                        else
                        {
                                for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
                                {
                                        if(rec_sequence > 0)
                                                strcat(email_list,",");
                                        else
                                                memset(email_list,0,1024);

                                        vector = ssdbGetRow(error_handle,req_handle);
                                        if(vector)
                                        {
                                                strcat(email_list,vector[0]);
                                        }
                                }
                                ssdbFreeRequest(error_handle,req_handle);
                                if ( session->textonly == 0 )
                                {
                                FormatedBody("<input type=\"text\" name=av_diag_encrypted size=25 value=\"%s\">",email_list);
                                }
                                else
                                {
                                FormatedBody(szLynxInEncrDiadFormat,email_list);
                                }
                                
                        }
	if ( session->textonly == 0 )
	{
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=3");
                        Body("&nbsp;\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=3");
			Body("<b>E-mail list for chatty pager</b>\n");
		CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=3");
			Body("&nbsp;\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("");
			Body("Enter email addresses for chatty pager:\n");
		CellEnd();
                CellBegin("");
                        Body("&nbsp;&nbsp;&nbsp;\n");
                CellEnd();
                CellBegin("");
	}
	else
	{
	Body("<p><pre>   <b>E-mail list for chatty pager</b></pre>	\n");
	}
                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                        "select option_default from tool where tool_option = 'PAGER' and tool_name='AVAILMON'")))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }
                        if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                        {
				ssdbFreeRequest(error_handle,req_handle);
				if ( session->textonly == 0 )
				{
                                Body("<input type=\"text\" name=av_pager size=25>\n");
                                }
                                else
                                {
                                FormatedBody(szLynxInPagerAmailFormat,"");
                                }
                        }
                        else
                        {
                                for (rec_sequence=0; rec_sequence < number_of_records; rec_sequence++)
                                {
                                        if(rec_sequence > 0)
                                                strcat(email_list,",");
                                        else
                                                memset(email_list,0,1024);

                                        vector = ssdbGetRow(error_handle,req_handle);
                                        if(vector)
                                        {
                                                strcat(email_list,vector[0]);
                                        }
                                }
                                ssdbFreeRequest(error_handle,req_handle);
                                if ( session->textonly == 0 )
                                {
                                FormatedBody("<input type=\"text\" name=av_pager size=25 value=\"%s\">",email_list);
                                }
                                else
                                {
                                FormatedBody(szLynxInPagerAmailFormat,email_list);
                                }
                        }
                      
	if ( session->textonly == 0 )
	{
                CellEnd();
        RowEnd();
	TableEnd();
	} 
}	



void availmon_mail_confirm(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{
	ssdb_Request_Handle req_handle;
        int number_of_records,rec_sequence,common_int,pageselect;
        const char **vector;
	char *p;
        char email_list[1024],common_string[2];

	if (!get_variable(hError,cmdp,INTTYPE,"pageselect",common_string,&pageselect,609))
		return;

        if ( session->textonly )
        {	
           /* Avail Report */
          if ( !CheckInputFieldForBadChars ( cmdp, "av_text",           "E-mail addresses that receive availability report in text form" ))        	
                return ;
          if ( !CheckInputFieldForBadChars ( cmdp, "av_compressed",     "E-mail addresses that receive availability report in compressed form" ))        	
                return ;
          if ( !CheckInputFieldForBadChars ( cmdp, "av_encrypted",      "E-mail addresses that receive availability report in compressed encrypted form" ))        	
                return ;
                
          /* Diagnostic report */      
          if ( !CheckInputFieldForBadChars ( cmdp, "av_diag_text",      "E-mail addresses that receive diagnostic report in text form" ))        	
                return ;
          if ( !CheckInputFieldForBadChars ( cmdp, "av_diag_compressed","E-mail addresses that receive diagnostic report in compressed form" ))        	
                return ;
          if ( !CheckInputFieldForBadChars ( cmdp, "av_diag_encrypted", "E-mail addresses that receive diagnostic report in compressed encrypted form" ))        	
                return ;
                
          /* Pager report */      
          if ( !CheckInputFieldForBadChars ( cmdp, "av_pager",          "E-mail address for chatty pager" ))        	
                return ;
	}
		
	if (!get_variable_variable(hError,cmdp,CHARTYPE,"av_text",email_list,&common_int,610)) 
		return;

	if(!(ssdbLockTable(error_handle,connection,"tool")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
	"select option_default from tool where tool_option = 'AVAILABILITY' and tool_name='AVAILMON'")))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}

	number_of_records = getnumrecords(hError,error_handle,req_handle);
	updateemailconfig_availmon(error_handle, req_handle, number_of_records, email_list, "AVAILABILITY");
	ssdbFreeRequest(error_handle,req_handle);

	if(number_of_records)
	{
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
                "delete from tool where tool_option = 'AVAILABILITY' and tool_name='AVAILMON'")))
                {
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
		ssdbFreeRequest(error_handle,req_handle);
	}

	if (!strlen(email_list))
	{
		if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
		"insert into tool values ('AVAILMON','AVAILABILITY','')")))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		ssdbFreeRequest(error_handle,req_handle);
	}
	else
	{
		p = strtok(email_list," ,");	
		while (p)
		{
			if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
			"insert into tool values('AVAILMON','AVAILABILITY','%s')",p)))
			{                                                       
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				return;
			}	
			ssdbFreeRequest(error_handle,req_handle); 
			p = strtok(NULL," ,");
		}
	}	

		
	if (!get_variable_variable(hError,cmdp,CHARTYPE,"av_compressed",email_list,&common_int,611))
	{
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select option_default from tool where tool_option = 'AVAILABILITY_COMP' and tool_name='AVAILMON'")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
	number_of_records = getnumrecords(hError,error_handle,req_handle);
	updateemailconfig_availmon(error_handle, req_handle, number_of_records, email_list, "AVAILABILITY_COMP");
        ssdbFreeRequest(error_handle,req_handle);

        if(number_of_records)
        {
                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
                "delete from tool where tool_option='AVAILABILITY_COMP' and tool_name='AVAILMON'")))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		ssdbFreeRequest(error_handle,req_handle);
	}
	if (!strlen(email_list))
        {
                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
                "insert into tool values ('AVAILMON','AVAILABILITY_COMP','')")))
                {
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
		ssdbFreeRequest(error_handle,req_handle);
        }
        else
        {
                p = strtok(email_list," ,");
                while (p)
                {
                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
                        "insert into tool values('AVAILMON','AVAILABILITY_COMP','%s')",p)))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }
                        ssdbFreeRequest(error_handle,req_handle);
                        p = strtok(NULL," ,");
                }
        }
	if (!get_variable_variable(hError,cmdp,CHARTYPE,"av_encrypted",email_list,&common_int,612))
	{
                if (!ssdbUnLockTable(error_handle,connection))
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }

        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select option_default from tool where tool_option = 'AVAILABILITY_CENCR' and tool_name='AVAILMON'")))
	{
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
        number_of_records = getnumrecords(hError,error_handle,req_handle);
	updateemailconfig_availmon(error_handle, req_handle, number_of_records, email_list, "AVAILABILITY_CENCR");
        ssdbFreeRequest(error_handle,req_handle);

        if(number_of_records)
        {
                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
                "delete from tool where tool_option = 'AVAILABILITY_CENCR' and tool_name='AVAILMON'")))
		{
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
		ssdbFreeRequest(error_handle,req_handle);
	}	
	if (!strlen(email_list))
        {
                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
                "insert into tool values ('AVAILMON','AVAILABILITY_CENCR','')")))
                {
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
                ssdbFreeRequest(error_handle,req_handle);
        }
        else
        {
                p = strtok(email_list," ,");
                while (p)
                {
                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
                        "insert into tool values('AVAILMON','AVAILABILITY_CENCR','%s')",p)))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }
                        ssdbFreeRequest(error_handle,req_handle);
                        p = strtok(NULL," ,");
                }
        }
	if (!get_variable_variable(hError,cmdp,CHARTYPE,"av_diag_text",email_list,&common_int,613))
	{
                if (!ssdbUnLockTable(error_handle,connection))
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }

        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select option_default from tool where tool_option = 'DIAGNOSTIC' and tool_name='AVAILMON'")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
        number_of_records = getnumrecords(hError,error_handle,req_handle);
	updateemailconfig_availmon(error_handle, req_handle, number_of_records, email_list, "DIAGNOSTIC");
        ssdbFreeRequest(error_handle,req_handle);

        if(number_of_records)
        {
                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
                "delete from tool where tool_option = 'DIAGNOSTIC' and tool_name='AVAILMON'")))
		{
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			return;
		}
		ssdbFreeRequest(error_handle,req_handle);
        }
        if (!strlen(email_list))
        {
                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
                "insert into tool values ('AVAILMON','DIAGNOSTIC','')")))
                {
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
                ssdbFreeRequest(error_handle,req_handle);
        }
        else
        {
                p = strtok(email_list," ,");
                while (p)
                {
                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
                        "insert into tool values('AVAILMON','DIAGNOSTIC','%s')",p)))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }
                        ssdbFreeRequest(error_handle,req_handle);
                        p = strtok(NULL," ,");
                }
        }
	if (!get_variable_variable(hError,cmdp,CHARTYPE,"av_diag_compressed",email_list,&common_int,614))
	{
                if (!ssdbUnLockTable(error_handle,connection))
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select option_default from tool where tool_option = 'DIAGNOSTIC_COMP' and tool_name='AVAILMON'")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
        number_of_records = getnumrecords(hError,error_handle,req_handle);
	updateemailconfig_availmon(error_handle, req_handle, number_of_records, email_list, "DIAGNOSTIC_COMP");
        ssdbFreeRequest(error_handle,req_handle);

        if(number_of_records)
        {
                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
                "delete from tool where tool_option = 'DIAGNOSTIC_COMP' and tool_name='AVAILMON'")))
                {
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
                ssdbFreeRequest(error_handle,req_handle);
        }
        if (!strlen(email_list))
        {
                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
                "insert into tool values ('AVAILMON','DIAGNOSTIC_COMP','')")))
                {
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
                ssdbFreeRequest(error_handle,req_handle);
        }
        else
        {
                p = strtok(email_list," ,");
                while (p)
                {
                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
                        "insert into tool values('AVAILMON','DIAGNOSTIC_COMP','%s')",p)))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }
                        ssdbFreeRequest(error_handle,req_handle);
                        p = strtok(NULL," ,");
                }
        }
	if (!get_variable_variable(hError,cmdp,CHARTYPE,"av_diag_encrypted",email_list,&common_int,615))
	{
                if (!ssdbUnLockTable(error_handle,connection))
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select option_default from tool where tool_option = 'DIAGNOSTIC_CENCR' and tool_name='AVAILMON'")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
        number_of_records = getnumrecords(hError,error_handle,req_handle);
	updateemailconfig_availmon(error_handle, req_handle, number_of_records, email_list, "DIAGNOSTIC_CENCR");
        ssdbFreeRequest(error_handle,req_handle);

        if(number_of_records)
        {
                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
                "delete from tool where tool_option = 'DIAGNOSTIC_CENCR' and tool_name='AVAILMON'")))
                {
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
                ssdbFreeRequest(error_handle,req_handle);
        }
        if (!strlen(email_list))
        {
                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
                "insert into tool values ('AVAILMON','DIAGNOSTIC_CENCR','')")))
                {
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
                ssdbFreeRequest(error_handle,req_handle);
        }
        else
        {
                p = strtok(email_list," ,");
                while (p)
                {
                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
                        "insert into tool values('AVAILMON','DIAGNOSTIC_CENCR','%s')",p)))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }
                        ssdbFreeRequest(error_handle,req_handle);
                        p = strtok(NULL," ,");
                }
        }
	if (!get_variable_variable(hError,cmdp,CHARTYPE,"av_pager",email_list,&common_int,616))
	{
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
        "select option_default from tool where tool_option = 'PAGER'and tool_name='AVAILMON'")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		if (!ssdbUnLockTable(error_handle,connection))
			sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }
        number_of_records = getnumrecords(hError,error_handle,req_handle);
	updateemailconfig_availmon(error_handle, req_handle, number_of_records, email_list, "PAGER");
        ssdbFreeRequest(error_handle,req_handle);

        if(number_of_records)
        {
                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_DELETE,
                "delete from tool where tool_option = 'PAGER' and tool_name='AVAILMON'")))
                {
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
                ssdbFreeRequest(error_handle,req_handle);
        }
        if (!strlen(email_list))
        {
                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
                "insert into tool values ('AVAILMON','PAGER','')")))
                {
                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
			if (!ssdbUnLockTable(error_handle,connection))
				sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                        return;
                }
                ssdbFreeRequest(error_handle,req_handle);
        }
        else
        {
                p = strtok(email_list," ,");
                while (p)
                {
                        if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_INSERT,
                        "insert into tool values('AVAILMON','PAGER','%s')",p)))
                        {
                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
				if (!ssdbUnLockTable(error_handle,connection))
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                return;
                        }
                        ssdbFreeRequest(error_handle,req_handle);
                        p = strtok(NULL," ,");
                }
        }
	if (!ssdbUnLockTable(error_handle,connection))
	{
		sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
		return;
	}
	availmon_mail_confirm_html(hError,session,connection,error_handle,pageselect);
}

void availmon_mail_confirm_html(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle, int pageselect)
{
	ssdb_Request_Handle req_handle;
        int i,number_of_records;
        const char **vector;

	if(pageselect == 2)
	{
	   if ( session->textonly == 0 )
	   {
		Body("<html><head><title>SGI Embedded Support Partner - ver.1.0</title></head>\n");
		Body("<body bgcolor=\"#ffffcc\" link=\"#333300\" vlink=\"#333300\"><form method=POST>\n");
		TableBegin("border=0 cellpadding=0 cellspacing=0");
			RowBegin("");
				CellBegin("bgcolor=\"#cccc99\" width=\"15\"");
					Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
				CellEnd();
				CellBegin("bgcolor=\"#cccc99\"");
					Body("SETUP &gt; Availability MailList\n");
				CellEnd();
			RowEnd();
			RowBegin("");
				CellBegin("colspan=2");
					Body("&nbsp;<p>&nbsp;\n");
				CellEnd();
			RowEnd();
			RowBegin("");
				CellBegin("");
					Body("&nbsp;\n");
				CellEnd();
				CellBegin("");
					TableBegin("border=0 cellpadding=0 cellspacing=0");
           } 
           else 
           {
		Body("<html><head><title>SGI Embedded Support Partner - ver.1.0</title></head>\n");
		Body("<body bgcolor=\"#ffffcc\" link=\"#333300\" vlink=\"#333300\">\n");
 	        Body("<pre>   SETUP &gt; Availability MailList</pre>\n");
 	        Body("<hr width=100%>\n");
           }
	}
	
	if ( session->textonly == 0 )
	{
		RowBegin("");
			CellBegin("colspan=3");
				Body("<b>Availmon Monitor E-mail list for availability report:</b>\n");
			CellEnd();
		RowEnd();
		
		RowBegin("");
                        CellBegin("colspan=3");
				Body("&nbsp;\n");
			CellEnd();
                RowEnd();
                
                RowBegin("valign=top");
                        CellBegin("");
				Body("E-mail addresses that receive availability report in text form\n");
			CellEnd();
			CellBegin("");
				Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
			CellEnd();
			CellBegin("");
	} 
	else 
	{
		Body("<p><b>Availmon Monitor E-mail list for availability report:</b>\n");
		Body("<p>E-mail addresses that receive availability report<br>\n");
		Body("<pre>     in text form:</pre>\n");
	}
			
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select option_default from tool where tool_option = 'AVAILABILITY' and tool_name = 'AVAILMON'")))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                                {
               			      if ( session->textonly == 0 )
               			      {
					Body("None\n");
				      } else {
					FormatedBody("<pre>        None</pre>");
				      }
                                }
				else
				{
					for(i = 0; i < number_of_records; i++)
					{
		                                vector = ssdbGetRow(error_handle,req_handle);
               					if(vector)
               					{
               					    if ( session->textonly == 0 )
               					    {
							FormatedBody("%s ",vector[0]);
						    } else {
							FormatedBody("<pre>        %s</pre>",vector[0]);
						    }
						}
					}
				}
				ssdbFreeRequest(error_handle,req_handle);
			
	if ( session->textonly == 0 )
	{
			CellEnd();
		RowEnd();
		
		RowBegin("");
                        CellBegin("colspan=3");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
                RowBegin("valign=top");
                        CellBegin("");
                                Body("E-mail addresses that receive availability report in compressed form\n");
			CellEnd();
                        CellBegin("");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("");
                        
	} 
	else 
	{
		Body("<p><pre>     in compressed form:</pre>\n");
	}
                                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                                "select option_default from tool where tool_option = 'AVAILABILITY_COMP' and tool_name = 'AVAILMON'")))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        return;
                                }
                                if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                                {
               			      if ( session->textonly == 0 )
               			      {
					Body("None\n");
				      } else {
					FormatedBody("<pre>        None</pre>");
				      }
                                }
				else
                                {
                                        for(i = 0; i < number_of_records; i++)
                                        {
                                                vector = ssdbGetRow(error_handle,req_handle);
                                                if(vector)
                                                {
               					    if ( session->textonly == 0 )
               					    {
							FormatedBody("%s ",vector[0]);
						    } else {
							FormatedBody("<pre>        %s</pre>",vector[0]);
						    }
						}
                                        }
                                }
                                ssdbFreeRequest(error_handle,req_handle);
                                
	if ( session->textonly == 0 )
	{
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("colspan=3");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
                RowBegin("valign=top");
                        CellBegin("");
                                Body("E-mail addresses that receive availability report in compressed encrypted form\n");
			CellEnd();
                        CellBegin("");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("");
                        
  	} 
  	else 
  	{
		Body("<p><pre>     in compressed encrypted form:</pre>\n");
  	                        
  	}
                                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                                "select option_default from tool where tool_option = 'AVAILABILITY_CENCR' and tool_name = 'AVAILMON'")))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        return;
                                }
                                if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                                {
               			      if ( session->textonly == 0 )
               			      {
					Body("None\n");
				      } else {
					FormatedBody("<pre>        None</pre>");
				      }
                                }
				else
                                {
                                        for(i = 0; i < number_of_records; i++)
                                        {
                                                vector = ssdbGetRow(error_handle,req_handle);
                                                if(vector)
                                                {
               					    if ( session->textonly == 0 )
               					    {
							FormatedBody("%s ",vector[0]);
						    } else {
							FormatedBody("<pre>        %s</pre>",vector[0]);
						    }
						}
                                        }
                                }
                                ssdbFreeRequest(error_handle,req_handle);
                                
	if ( session->textonly == 0 )
	{
                                
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("colspan=3");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
        } 
        else
        {
        }        
                       
        /* Diagnostic reports */                  
	if ( session->textonly == 0 )
	{
                RowBegin("");
			CellBegin("colspan=3");
				Body("<b>Availmon Monitor E-mail list for  diagnostic report:</b>\n");
			CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("colspan=3");
				Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
		RowBegin("valign=top");
                        CellBegin("");
                                Body("E-mail addresses that receive diagnostic report in text form\n");
			CellEnd();
                        CellBegin("");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("");
	} 
	else
	{
		Body("<p><b>Availmon Monitor E-mail list for  diagnostic report:</b>\n");
                Body("<p><pre>   E-mail addresses that receive diagnostic report</pre>\n");
                Body("<pre>      in text form:</pre>\n");
	}                        
                                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                                "select option_default from tool where tool_option = 'DIAGNOSTIC' and tool_name = 'AVAILMON'")))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        return;
                                }
                                if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                                {
               			      if ( session->textonly == 0 )
               			      {
					Body("None\n");
				      } else {
					FormatedBody("<pre>        None</pre>");
				      }
                                }
				else
                                {
                                        for(i = 0; i < number_of_records; i++)
                                        {
                                                vector = ssdbGetRow(error_handle,req_handle);
                                                if(vector)
                                                {
               					    if ( session->textonly == 0 )
               					    {
							FormatedBody("%s ",vector[0]);
						    } else {
							FormatedBody("<pre>        %s</pre>",vector[0]);
						    }
						}
                                        }
                                }
                                ssdbFreeRequest(error_handle,req_handle);
                                         
	if ( session->textonly == 0 )
	{
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("colspan=3");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
		RowBegin("valign=top");
                        CellBegin("");
                                Body("E-mail addresses that receive diagnostic report in compressed form\n");
			CellEnd();
                        CellBegin("");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("");
        } 
        else
        {
                Body("<p><pre>      in compressed form :</pre>\n");
        }
                                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                                "select option_default from tool where tool_option = 'DIAGNOSTIC_COMP' and tool_name = 'AVAILMON'")))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        return;
                                }
                                if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                                {
               			      if ( session->textonly == 0 )
               			      {
					Body("None\n");
				      } else {
					FormatedBody("<pre>        None</pre>");
				      }
                                }
				else
                                {
                                        for(i = 0; i < number_of_records; i++)
                                        {
                                                vector = ssdbGetRow(error_handle,req_handle);
                                                if(vector)
                                                {
               					    if ( session->textonly == 0 )
               					    {
							FormatedBody("%s ",vector[0]);
						    } else {
							FormatedBody("<pre>        %s</pre>",vector[0]);
						    }
						}
                                        }
                                }
                                ssdbFreeRequest(error_handle,req_handle);
                                
	if ( session->textonly == 0 )
	{
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("colspan=3");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
                RowBegin("valign=top");
                        CellBegin("");
                                Body("E-mail addresses that receive diagnostic report in compressed encrypted form\n");
			CellEnd();
                        CellBegin("");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("");
        } 
        else
        {
               Body("<p><pre>      in compressed encrypted form:</pre>\n");
        }
                                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                                "select option_default from tool where tool_option = 'DIAGNOSTIC_CENCR' and tool_name = 'AVAILMON'")))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        return;
                                }
                                if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                                {
               			      if ( session->textonly == 0 )
               			      {
					Body("None\n");
				      } else {
					FormatedBody("<pre>        None</pre>");
				      }
                                }
				else
                                {
                                        for(i = 0; i < number_of_records; i++)
                                        {
                                                vector = ssdbGetRow(error_handle,req_handle);
                                                if(vector)
                                                {
               					    if ( session->textonly == 0 )
               					    {
							FormatedBody("%s ",vector[0]);
						    } else {
							FormatedBody("<pre>        %s</pre>",vector[0]);
						    }
						}
                                        }
                                }
                                ssdbFreeRequest(error_handle,req_handle);
                                
	if ( session->textonly == 0 )
	{
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("colspan=3");
                                Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
		RowBegin("");
                        CellBegin("colspan=3");
                                Body("<b>E-mail list for chatty pager</b>\n");
			CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("colspan=3");
				Body("&nbsp;\n");
                        CellEnd();
                RowEnd();
		RowBegin("valign=top");
                        CellBegin("");
                                Body("E-mail  addresses for chatty pager\n");
			CellEnd();
                        CellBegin("");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("");
         } 
         else 
         {
                Body("<p><b>E-mail list for chatty pager</b><br><br>\n");
                Body("<pre>   E-mail  addresses for chatty pager:</pre>\n");
         
         }
                                if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                                "select option_default from tool where tool_option = 'PAGER' and tool_name = 'AVAILMON'")))
                                {
                                        sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                                        return;
                                }
                                if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
                                {
               			      if ( session->textonly == 0 )
               			      {
					Body("None\n");
				      } else {
					FormatedBody("<pre>        None</pre>");
				      }
                                }
				else
                                {
                                        for(i = 0; i < number_of_records; i++)
                                        {
                                                vector = ssdbGetRow(error_handle,req_handle);
                                                if(vector)
                                                {
               					    if ( session->textonly == 0 )
               					    {
							FormatedBody("%s ",vector[0]);
						    } else {
							FormatedBody("<pre>        %s</pre>",vector[0]);
						    }
						}
                                        }
                                }
                                ssdbFreeRequest(error_handle,req_handle);

	if ( session->textonly == 0 )
	{
                        CellEnd();
                RowEnd();
	TableEnd();
	}
	else
	{
	}
	
	if (pageselect == 2)
	{
		if ( session->textonly == 0 )
		{
			CellEnd();
			RowEnd();
			TableEnd();
			Body("</form></body></html>\n");
		} else {
			Body("</body></html>\n");
		}
	}
}

void global_setup(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
	ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{

	ssdb_Request_Handle req_handle;
        int number_of_records,rec_sequence;
        const char **vector;
        int action_id,logging_event,filter_event,action_event;

	help_create(hError,"setup_global_event_cfg");

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                "select option_default from tool where tool_name = 'SEM' and tool_option = 'GLOBAL_LOGGING_FLAG'")))
	{
                sscError(hError,"Database API Error: \"%s\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }

	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
	{
		print_error();
		ssdbFreeRequest(error_handle,req_handle);
		return; 
        }       
	vector = ssdbGetRow(error_handle,req_handle);
	if (vector)
	{
		logging_event = atoi(vector[0]);
		ssdbFreeRequest(error_handle,req_handle);
	}
	else
	{
		print_error();
		ssdbFreeRequest(error_handle,req_handle);
		return;
	}	

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                "select option_default from tool where tool_name = 'SEM' and tool_option = 'GLOBAL_THROTTLING_FLAG'")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }

	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
        {       
		print_error();
		ssdbFreeRequest(error_handle,req_handle);
                return; 
        }       

        vector = ssdbGetRow(error_handle,req_handle);
        if (vector)
        {
                filter_event = atoi(vector[0]);
		ssdbFreeRequest(error_handle,req_handle);
        }
        else
        {       
		print_error();
		ssdbFreeRequest(error_handle,req_handle);
                return; 
        }      

	if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
                "select option_default from tool where tool_name = 'SEM' and tool_option = 'GLOBAL_ACTION_FLAG'")))
        {
                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
                return;
        }

	if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
        {
		print_error();
		ssdbFreeRequest(error_handle,req_handle);
                return;
        }

        vector = ssdbGetRow(error_handle,req_handle);
        if (vector)
        {
                action_event = atoi(vector[0]);
		ssdbFreeRequest(error_handle,req_handle);
        }
        else
        {      
                print_error();
		ssdbFreeRequest(error_handle,req_handle);
                return;
        }
	TableBegin("border=0 cellpadding=0 cellspacing=0");
	RowBegin("");
                CellBegin("");
			Body("<b>Log events</b>\n");
		CellEnd();
		CellBegin("align=right");
			if (logging_event)
			Body("<INPUT TYPE=\"RADIO\" NAME=\"GLOBAL_LOGGING_FLAG\" value=1 checked>&nbsp;Yes&nbsp;&nbsp;&nbsp;<INPUT TYPE=\"RADIO\" NAME=\"GLOBAL_LOGGING_FLAG\" value=0>&nbsp;No\n");
			else
				Body("<INPUT TYPE=\"RADIO\" NAME=\"GLOBAL_LOGGING_FLAG\" value=1>&nbsp;Yes&nbsp;&nbsp;&nbsp;<INPUT TYPE=\"RADIO\" NAME=\"GLOBAL_LOGGING_FLAG\" value=0 checked>&nbsp;No\n");
		CellEnd();
	RowEnd();
	RowBegin("");
		CellBegin("colspan=2");
			Body("&nbsp;\n");
                CellEnd();
	RowEnd();
	RowBegin("");
		CellBegin("colspan=2");
			Body("This parameter enables or disables global event logging. Select \"Yes\" to log events in the SGI Embedded Support Partner database. Select \"No\" if you do not want to log any events in SGI Embedded Support Partner database.\n");
		CellEnd();
	RowEnd();
	RowBegin("");
                CellBegin("colspan = 2");
                        Body("&nbsp;<hr>&nbsp;\n");
                CellEnd();
	RowEnd();
        RowBegin("");
                CellBegin("");
                        Body("<b>Throttle events</b>\n");
                CellEnd();
                CellBegin("align=right");
			if(filter_event)
			Body("<INPUT TYPE=\"RADIO\" NAME=\"GLOBAL_THROTTLING_FLAG\" value=1 checked>&nbsp;Yes&nbsp;&nbsp;&nbsp;<INPUT TYPE=\"RADIO\" NAME=\"GLOBAL_THROTTLING_FLAG\" value=0>&nbsp;No\n");
			else
				Body("<INPUT TYPE=\"RADIO\" NAME=\"GLOBAL_THROTTLING_FLAG\" value=1>&nbsp;Yes&nbsp;&nbsp;&nbsp;<INPUT TYPE=\"RADIO\" NAME=\"GLOBAL_THROTTLING_FLAG\" value=0 checked>&nbsp;No\n");
		CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=2");
                        Body("&nbsp;\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=2");
                        Body("This parameter enables or disables event throttling for all events. Select \"Yes\" to require that a specific  number of events must occur before the event is registered in the SGI Embedded Support Partner database. Select \"No\" to register every event in the SGI Embedded System Partner database.	\n");
		CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=2");
                        Body("&nbsp;<hr>&nbsp;\n");
                CellEnd();
	RowEnd();
        RowBegin("");
                CellBegin("");
                        Body("<b>Act on events</b>\n");
                CellEnd();
                CellBegin("align=right");
			if(action_event)
			Body("<INPUT TYPE=\"RADIO\" NAME=\"GLOBAL_ACTION_FLAG\" value=1 checked>&nbsp;Yes&nbsp;&nbsp;&nbsp;<INPUT TYPE=\"RADIO\" NAME=\"GLOBAL_ACTION_FLAG\" value=0>&nbsp;No\n");
                        else    
                                Body("<INPUT TYPE=\"RADIO\" NAME=\"GLOBAL_ACTION_FLAG\" value=1>&nbsp;Yes&nbsp;&nbsp;&nbsp;<INPUT TYPE=\"RADIO\" NAME=\"GLOBAL_ACTION_FLAG\" value=0 checked>&nbsp;No\n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=2 ");
                        Body("&nbsp;        \n");
                CellEnd();
        RowEnd();
        RowBegin("");
                CellBegin("colspan=2 ");
                        Body("This parameter enables or disables SGI Embedded Support Partner actions in response to events. Select \"Yes\" to specify that the SGI Embedded Support Partner should perform actions in response to all events that occur. Select \"No\" to specify that the SGI Embedded Support Partner should not respond to events that occur.\n");
		CellEnd();
        RowEnd();
	RowBegin("");
                CellBegin("colspan = 2");
                        Body("&nbsp;\n");
                CellEnd();
        RowEnd();
	RowBegin("");
                CellBegin("align=right colspan=2");
                        Body("<INPUT TYPE=\"SUBMIT\" VALUE=\"   Accept   \">\n");
		CellEnd();
        RowEnd();
	TableEnd();
}


void globalconfirm(sscErrorHandle hError, mySession *session,ssdb_Connection_Handle connection,
        ssdb_Error_Handle error_handle,CMDPAIR *cmdp)
{

	ssdb_Request_Handle req_handle;
        int number_of_records,rec_sequence;
        const char **vector;
        int action_id,key,common_int,global_flag,global_throttle,global_logging;
	char logging_event[10],filter_event[10],action_event[10],common_string[2],buffer[32];

	Body("<html><head><title>SGI Embedded Support Partner - ver.1.0</title></HEAD>\n");
        Body("<body bgcolor=\"#ffffcc\" link=\"#333300\" vlink=\"#333300\">\n");
	Body("<form method=post action=\"/$sss/rg/libavailability\">\n");
        TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
                RowBegin("");
                        CellBegin("bgcolor=\"#cccc99\" width=15");
                                Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
                        CellEnd();
                        CellBegin("bgcolor=\"#cccc99\"");
                                Body("SETUP &gt; Global &gt; Global Configuration \n");
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("colspan=2");
                                Body("&nbsp;<p>&nbsp;\n");
                        CellEnd();
                RowEnd();
                RowBegin("");
                        CellBegin("@ &nbsp;");
                        CellEnd();
	 		CellBegin("");
				if(!(ssdbLockTable(error_handle,connection,"tool")))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}

				if(!get_variable(hError,cmdp,CHARTYPE,"GLOBAL_LOGGING_FLAG",logging_event,&common_int,618))
				{
					if (!ssdbUnLockTable(error_handle,connection))
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}

				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
				"update tool set option_default = '%s' where tool_option = 'GLOBAL_LOGGING_FLAG'",logging_event)))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					if (!ssdbUnLockTable(error_handle,connection))
                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				ssdbFreeRequest(error_handle,req_handle);
				if (atoi(logging_event))
					Body("SGI Embedded Support Partner logging capability is <b>enabled</b>. Events will be logged based on event parameter settings.\n");
				else
					Body("SGI Embedded Support Partner logging capability is <b>disabled</b>. Events will not be logged in SGI Embedded Support Partner database.\n");
				Body("<p>\n");
				if(!get_variable(hError,cmdp,CHARTYPE,"GLOBAL_THROTTLING_FLAG",filter_event,&common_int,619))
				{
					if (!ssdbUnLockTable(error_handle,connection))
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}

				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
				"update tool set option_default = '%s' where tool_option = 'GLOBAL_THROTTLING_FLAG'",filter_event)))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					if (!ssdbUnLockTable(error_handle,connection))
                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				ssdbFreeRequest(error_handle,req_handle);
				if (atoi(filter_event))
					Body("SGI Embedded Support Partner throttling capability is <b>enabled</b>. Events will be registered with Embedded Support Partner database based on event parameter settings.\n");
				else
					Body("SGI Embedded Support Partner throttling capability is <b>disabled</b>. Each event will be registered.\n");
				Body("<p>\n");
				if(!get_variable(hError,cmdp,CHARTYPE,"GLOBAL_ACTION_FLAG",action_event,&common_int,620))
				{
					if (!ssdbUnLockTable(error_handle,connection))
						sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}

				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_UPDATE,
				"update tool set option_default = \"%s\" where tool_option = 'GLOBAL_ACTION_FLAG'",action_event)))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					if (!ssdbUnLockTable(error_handle,connection))
                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				ssdbFreeRequest(error_handle,req_handle);
				if (atoi(action_event))
					Body("SGI Embedded Support Partner action capability is <b>enabled</b>. Actions will be taken against events based on action parameter settings.\n");
				else
					Body("SGI Embedded Support Partner action capability is <b>disabled</b>. No actions will be taken against events.\n");

				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select option_default from tool where tool_option='GLOBAL_ACTION_FLAG' and tool_name='SEM'")))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					if (!ssdbUnLockTable(error_handle,connection))
                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}

				if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
				{
					sscError(hError,"Failed to read global configuration data\n");
					if (!ssdbUnLockTable(error_handle,connection))
                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					ssdbFreeRequest(error_handle,req_handle);
					return;
				}

				vector = ssdbGetRow(error_handle,req_handle);
				if (vector)
				{
					global_flag = atoi(vector[0]);
					ssdbFreeRequest(error_handle,req_handle);
				}
				else
					ssdbFreeRequest(error_handle,req_handle);	

				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select option_default from tool where tool_option='GLOBAL_THROTTLING_FLAG' and tool_name='SEM'")))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					if (!ssdbUnLockTable(error_handle,connection))
                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}

				if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
				{
					sscError(hError,"Failed to read global configuration data\n");
					if (!ssdbUnLockTable(error_handle,connection))
                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					ssdbFreeRequest(error_handle,req_handle);
					return;
				}

				vector = ssdbGetRow(error_handle,req_handle);
				if (vector)
				{
					global_throttle = atoi(vector[0]);
					ssdbFreeRequest(error_handle,req_handle);
				}
				else
					ssdbFreeRequest(error_handle,req_handle);	
				
				if (!(req_handle = ssdbSendRequest(error_handle,connection,SSDB_REQTYPE_SELECT,
				"select option_default from tool where tool_option='GLOBAL_LOGGING_FLAG' and tool_name='SEM'")))
				{
					sscError(hError,"1 Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					if (!ssdbUnLockTable(error_handle,connection))
                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}

				if ((number_of_records = getnumrecords(hError,error_handle,req_handle)) == 0)
				{
					sscError(hError,"Failed to read global configuration data\n");
					if (!ssdbUnLockTable(error_handle,connection))
                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					ssdbFreeRequest(error_handle,req_handle);
					return;
				}

				vector = ssdbGetRow(error_handle,req_handle);
				if (vector)
				{
					global_logging = atoi(vector[0]);
					ssdbFreeRequest(error_handle,req_handle);
				}
				else
					ssdbFreeRequest(error_handle,req_handle);

				sprintf(buffer,"%d %d %d",global_flag,global_throttle,global_logging);
				if(configure_global(buffer))
				{
					sscError(hError, "Event monitoring daemon eventmond is not running\n");
					if (!ssdbUnLockTable(error_handle,connection))
                                                sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}
				if (!ssdbUnLockTable(error_handle,connection))
				{
					sscError(hError,"Database API Error: \"%s\"\n\n",ssdbGetLastErrorString(error_handle));
					return;
				}

			CellEnd();
		RowEnd();
	TableEnd();
	Body("</form>\n");
	Body("</body></html>\n");
}

void print_error(void)
{
	CellBegin("colspan=2");
                Body("<b>No parameters for global setup in the database.</b>\n");
                RowEnd();
                TableEnd();
                Body("</body> </html>\n");
                return;
}
