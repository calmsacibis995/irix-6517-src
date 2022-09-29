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

#include "availability.h"

/* ----------------------------- rgpgInit ------------------------------------ */
int RGPGAPI rgpgInit(sscErrorHandle hError)
{
	/* Try initialize "free list of mySession" mutex */
	if(pthread_mutex_init(&seshFreeListmutex,0))
	{
		sscError(hError,szServerNameErrorPrefix,"Can't initialize mutex");
		return 0; /* error */
	}
	mutex_inited++;
	#ifdef INCLUDE_TIME_DATE_STRINGS
		sprintf(szVersionStr,"%d.%d %s %s",MYVERSION_MAJOR,MYVERSION_MINOR,__DATE__,__TIME__);
	#else
		sprintf(szVersionStr,"%d.%d",MYVERSION_MAJOR,MYVERSION_MINOR);
	#endif
	return 1; /* success */
}

/* ----------------------------- rgpgDone ------------------------------------ */
int RGPGAPI rgpgDone(sscErrorHandle hError)
{
	mySession *s;
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

/* -------------------------- rgpgCreateSesion ------------------------------- */
rgpgSessionID RGPGAPI rgpgCreateSesion(sscErrorHandle hError)
{
	mySession *s;
	pthread_mutex_lock(&seshFreeListmutex);
	if((s = sesFreeList) != 0)
		sesFreeList = s->next;
	pthread_mutex_unlock(&seshFreeListmutex);
	if(!s)
		s = (mySession*)malloc(sizeof(mySession));
	if(!s)
	{
		sscError(hError,szServerNameErrorPrefix,"No memory to create session in rgpgCreateSesion()");
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
		sscError(hError,szServerNameErrorPrefix,"Incorrect session id in rgpgDeleteSesion()");
	}
	else
	{
		pthread_mutex_lock(&seshFreeListmutex);
		s->next = sesFreeList;
		sesFreeList = s;
		s->signature = 0;
		pthread_mutex_unlock(&seshFreeListmutex);
	}
}

/* ------------------------ rgpgGetAttribute --------------------------------- */
char *RGPGAPI rgpgGetAttribute(sscErrorHandle hError,rgpgSessionID session, const char *attributeID, const char *extraAttrSpec,int *typeattr)
{
	char *s;
	if(typeattr)
		*typeattr = RGATTRTYPE_STATIC;
	if(!strcasecmp(attributeID,szVersion))
		s = (char*)szVersionStr;
	else if(!strcasecmp(attributeID,szTitle))
		s = (char*)myLogo;
	else if(!strcasecmp(attributeID,szUnloadTime))
		s = (char*)szUnloadTimeValue;
	else if(!strcasecmp(attributeID,szThreadSafe))
		s = (char*)szThreadSafeValue;
	else if(!strcasecmp(attributeID,szUnloadable))
		s = (char*)szUnloadableValue;
	else if(!strcasecmp(attributeID,szAcceptRawCmdString))
		s = (char*)szAcceptRawCmdStringValue;
	else
	{
		sscError(hError,"%s No such attribute '%s' in rgpgGetAttribute()",szServerNameErrorPrefix,attributeID);
		return 0;
	}
	if(!s)
		sscError(hError,szServerNameErrorPrefix,"No memory in rgpgGetAttribute()");
	return s;
}

/* --------------------- rgpgFreeAttributeString ------------------------------ */
void RGPGAPI rgpgFreeAttributeString(sscErrorHandle hError,rgpgSessionID session,const char *attributeID,const char *extraAttrSpec,
	char *attrString,int restype)
{  
	if(((restype == RGATTRTYPE_SPECIALALLOC) || (restype == RGATTRTYPE_MALLOCED)) && attrString)
		free(attrString);
}


/*----------------------------------------rgpgSetAttribute---------------------------------------------*/

void  RGPGAPI rgpgSetAttribute(sscErrorHandle hError, rgpgSessionID session, const char *attributeID, const char *extraAttrSpec, const char *value)
{
	char buff[128]; mySession *sess = session;

	if(!strcasecmp(attributeID,szVersion) || !strcasecmp(attributeID,szTitle) ||
	!strcasecmp(attributeID,szThreadSafe) || !strcasecmp(attributeID,szUnloadable) ||
	!strcasecmp(attributeID,szUnloadTime))
	{
		sscError(hError,szServerNameErrorPrefix,"Attempt to set read only attribute in rgpgSetAttribute()");
		return;
	}

	if(!strcasecmp(attributeID,"User-Agent") && value)
	{
		sess->textonly = (strcasecmp(value,"Lynx") == 0) ? 1 : 0;
		return;
	}
	if(!sess || sess->signature != sizeof(mySession))
	{
		sscError(hError,szServerNameErrorPrefix,"Incorrect session id in rgpgGetAttribute()");
		return;
	}
}


/*----------------------------------------rgpgGenerateReport---------------------------------------------*/

int RGPGAPI rgpgGenerateReport(sscErrorHandle hError, rgpgSessionID _session, int argc, char* argv[],char *rawCommandString, streamHandle result,
	char *accessmask)
{  
	ssdb_Client_Handle client;
	ssdb_Connection_Handle connection;
	ssdb_Request_Handle req_handle;
	ssdb_Error_Handle error_handle;
	int i,cmdpsize,SQL_TYPE;
	CMDPAIR cmdp[MAXNUMBERS_OF_KEYS];
	mySession *session = _session;

	if ((error_handle = ssdbCreateErrorHandle()) == NULL)
        {
                sscError(hError,"SSDB API Error %d: \"%s\" - can't create error handle\n",
			ssdbGetLastErrorCode(0),ssdbGetLastErrorString(0));
                return 0;
        }
        if ((client = ssdbNewClient(error_handle,szDefaultUserName,szDefaultPassword,0)) == NULL)
        {
                sscError(hError,"SSDB API Error %d: \"%s\" - can't create new client handle\n",
			ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
		ssdbDeleteErrorHandle(error_handle);
                return 0;
        }

        /* Establish a connection to a database */
        if ((connection = ssdbOpenConnection(error_handle,client,szDefaultHostName,szDefaultDatabaseName,0)) == NULL)
        {
                sscError(hError,"SSDB API Error %d: \"%s\" - can't open connection\n",
			ssdbGetLastErrorCode(error_handle),ssdbGetLastErrorString(error_handle));
		ssdbDeleteErrorHandle(error_handle);
		ssdbDeleteClient(error_handle,client);
                return 0;
        }


	/*  Initialize The HTML Generator  */

	i = createMyHTMLGenerator(result);
	if (i != 0)
	{
		sscError(hError,  "Sample server: Can't open the HTML generator");
		ssdbDeleteErrorHandle(error_handle);
		ssdbDeleteClient(error_handle,client);
		ssdbCloseConnection (error_handle,connection);
		return 0;
	}

	if (argc < 2)
	{
		sscError(hError, "Invalid Request\n");
		ssdbDeleteErrorHandle(error_handle);
		ssdbDeleteClient(error_handle,client);
                ssdbCloseConnection (error_handle,connection);
		return 0;
	}

	cmdpsize = sscInitPair(rawCommandString,cmdp,MAXNUMBERS_OF_KEYS);

	   /* Check WEB browser type (Lynx is only tex based) */
   for(i = 0;(i = sscFindPairByKey(cmdp,i,szUserAgent)) >= 0;i++)
    if(!strcasecmp(cmdp[i].value,"Lynx")) session->textonly = 1;

		/* Availmon Setup */
                if (!strcasecmp(argv[1],"AVAILMON_CONFIG"))
                        session->report_type = AVAILMON_CONFIG;
                else if (!strcasecmp(argv[1],"AVAILMON_CONFIG_CONFIRM"))
                        session->report_type = AVAILMON_CONFIG_CONFIRM;
                else if (!strcasecmp(argv[1],"AVAILMON_MAIL"))
                        session->report_type = AVAILMON_MAIL;
                else if (!strcasecmp(argv[1],"AVAILMON_MAIL_CONFIRM"))
                        session->report_type = AVAILMON_MAIL_CONFIRM;
		else if (!strcasecmp(argv[1],"GLOBALPAGE"))
                        session->report_type = GLOBALPAGE;
                else if (!strcasecmp(argv[1],"GLOBALCONFIRM"))
                        session->report_type = GLOBALCONFIRM;
                else
                {
                        sscError(hError, "Invalid Report Request %s",argv[1]);
                }

	switch (session->report_type)
	{
		case AVAILMON_CONFIG:
			availmon_config(hError,session,connection,error_handle);
			break;
		case AVAILMON_CONFIG_CONFIRM:
			availmon_config_confirm(hError,session,connection,error_handle,cmdp);
			break;
		case AVAILMON_MAIL:
			availmon_mail(hError,session,connection,error_handle,cmdp);
			break;
		case AVAILMON_MAIL_CONFIRM:
			availmon_mail_confirm(hError,session,connection,error_handle,cmdp);
			break;
		case GLOBALPAGE:
                        global_setup(hError,session,connection,error_handle,cmdp);
                        break;
                case GLOBALCONFIRM:
                        globalconfirm(hError,session,connection,error_handle,cmdp);
                        break;
	}

	/*  Destroy The HTML Generator  */
	i = deleteMyHTMLGenerator();
	if (i != 0)
	{
		sscError(hError,  "Sample server: Can't destroy the HTML generator");
	}
        if (ssdbIsConnectionValid(error_handle,connection))
                ssdbCloseConnection (error_handle,connection);

        if (ssdbIsClientValid(error_handle,client))
                ssdbDeleteClient(error_handle,client);

        if (error_handle)
                ssdbDeleteErrorHandle(error_handle);
	return 0;
}





#if 0

<script language="JavaScript">

<!--
function showMap()
{ var map=window.open('help.html', 'help', 
  'width=650,height=350,status=yes,scrollbars=yes,resizable=yes');
  map.main=self;
  map.main.name="sss_main";
  map.focus();
}

function isPosInt(inputVal)
{ inputStr = inputVal.toString()
  for(i = 0;i < inputStr.length; i++)
  { oneChar = inputStr.charAt(i);
    if(oneChar < "0" || oneChar > "9") return false;
  }
  return true;
}

function checkString(inputVal, fieldname)
{ inputStr = inputVal.toString()
  for(i = 0;i < inputStr.length; i++)
  { oneChar = inputStr.charAt(i);
    if(oneChar == "\"")
    { alert("String " + fieldname + " should not contain \"'s");
      return false;
    }
  }
  return true;
}

function clearForm(av_config_form) {
  for(i = 0; i < av_config_form.av_autoemail.length; i++)
  { av_config_form.av_autoemail[i].checked = 0;
  }
  for(i = 0; i < av_config_form.av_shutdown.length; i++)
  { av_config_form.av_shutdown[i].checked = 0;
  }
  for(i = 0; i < av_config_form.av_hinv.length; i++)
  { av_config_form.av_hinv[i].checked = 0;
  }
  for(i = 0; i < av_config_form.av_sysmsg.length; i++)
  { av_config_form.av_sysmsg[i].checked = 0;
  }
  for(i = 0; i < av_config_form.av_uptimed.length; i++)
  { av_config_form.av_uptimed[i].checked = 0;
  }
  av_config_form.av_daysupdate.value = "";
  av_config_form.av_lasttick.value = "";
  av_config_form.av_tickduration.value = "";

}

function checkData(av_config_form)
{ var autoEmailChoice = -1;
  var sutdownChoice = -1;
  var hinvChoice = -1;
  var msgChoice = -1;
  var uptimedChoice = -1;
  var statusUpdate = av_config_form.av_daysupdate.value;
  av_config_form.av_daysupdate.focus();
  var uptimeFile = av_config_form.av_lasttick.value;
  av_config_form.av_lasttick.focus();
  var uptimeCheck = av_config_form.av_tickduration.value;
  av_config_form.av_tickduration.focus();
  for(i = 0; i < av_config_form.av_autoemail.length; i++)
  { if(av_config_form.av_autoemail[i].checked)
      autoEmailChoice = i;
  }
  for(i = 0; i < av_config_form.av_shutdown.length; i++)
  { if(av_config_form.av_shutdown[i].checked)
      sutdownChoice = i;
  }
  for(i = 0; i < av_config_form.av_hinv.length; i++)
  { if(av_config_form.av_hinv[i].checked)
      hinvChoice = i;
  }
  for(i = 0; i < av_config_form.av_sysmsg.length; i++)
  { if(av_config_form.av_sysmsg[i].checked)
      msgChoice = i;
  }
  for(i = 0; i < av_config_form.av_uptimed.length; i++)
  { if(av_config_form.av_uptimed[i].checked)
      uptimedChoice = i;
  }
  if(autoEmailChoice == -1 && sutdownChoice == -1 && hinvChoice == -1 && msgChoice == -1 &&  uptimedChoice ==-1 && statusUpdate == "" && uptimeFile == "" && uptimeCheck == "")
  { alert("Nothing to setup");
    return false;
  }
  if(autoEmailChoice == -1)
  { alert("You must enable or disable automatic email distribution");
    return false;
  }
  msg1 = "\n\nAutomatic e-mail distribution = " + av_config_form.av_autoemail[autoEmailChoice].value;
  if(sutdownChoice == -1)
  { alert("You must enable or disable display of shutdown reason");
    return false;
  }
  msg1 += "\nDisplay of shutdown reason = " + av_config_form.av_shutdown[sutdownChoice].value;
  if(hinvChoice == -1)
  { alert("You must choose to include HINV information into e-mail or not")
    return false;
  }
  msg1 += "\nInclude HINV information into e-mail = " + av_config_form.av_hinv[hinvChoice].value;
  if(msgChoice == -1)
  { alert("You must enable or disable capturing of important system messages");
    return false;
  }
  msg1 += "\nCapturing of important system messages = " + av_config_form.av_sysmsg[msgChoice].value;
  if(uptimedChoice == -1)
  { alert("You must to choose to start uptime deamon or not");
    return false;
  }
  msg1 += "\nStart uptime daemon = " + av_config_form.av_uptimed[uptimedChoice].value;
  if(statusUpdate == "")
  { alert("You must enter number of days between status updates");
    return false;
  }
  if(!isPosInt(statusUpdate) || statusUpdate > 300)
  { msg1 = "Incorrect entry for number of days between status updates\nMust be less or equal to 300 days";
    alert(msg1);
    return false;
  }
  msg1 += "\nNumber of days between status updates = " + statusUpdate;
  if(uptimeFile == "")
  { uptimeFile = "/var/adm/avail/.save/lasttick";
    document.av_config_form.av_lasttick.value = uptimeFile;
  }
  else {
     if (!checkString(document.av_config_form.av_lasttick.value, "filename for last update write")) return false;
  }
  msg1 += "\nFilename for last uptime write = " + uptimeFile;
  if(!isPosInt(uptimeCheck) || uptimeCheck > 600)
  { msg1 = "Incorrect entry for time interval between uptime check\nMust be less or equal to 600 seconds";
    alert(msg1);
    return false;
  }
  if(uptimeCheck == "")
  { uptimeCheck =  "300";
    document.av_config_form.av_tickduration.value = uptimeCheck;
  }
  msg1 += "\nInterval in seconds between uptime check = " + uptimeCheck;
  return window.confirm("You entered the following configuration for Availmon Monitor setup:" + msg1);
}
#endif
