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

/* The main routine for the SGM UI server */

#include "ui.h"

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
	if(typeattr) *typeattr = RGATTRTYPE_STATIC;
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
	if(!s) sscError(hError,szServerNameErrorPrefix,"No memory in rgpgGetAttribute()");
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
{  char buff[128]; mySession *sess = session;

	if(!strcasecmp(attributeID,szVersion) || !strcasecmp(attributeID,szTitle) ||
	!strcasecmp(attributeID,szThreadSafe) || !strcasecmp(attributeID,szUnloadable) ||
	!strcasecmp(attributeID,szUnloadTime))
	{
		sscError(hError,szServerNameErrorPrefix,"Attempt to set read only attribute in rgpgSetAttribute()");
		return;
	}

	if(!strcasecmp(attributeID,"User-Agent") && value)
	{ sess->textonly = (strcasecmp(value,"Lynx") == 0) ? 1 : 0;
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
                sscError(hError,"Database API Error: \"%s\" - can't create error handle\n",ssdbGetLastErrorString(0));
                return 0;
        }
        if ((client = ssdbNewClient(error_handle,szDefaultUserName,szDefaultPassword,0)) == NULL)
        {
                sscError(hError,"Database API Error: \"%s\" - can't create new client handle\n",ssdbGetLastErrorString(error_handle));
		ssdbDeleteErrorHandle(error_handle);	
                return 0;
        }

        /* Establish a connection to a database */
        if ((connection = ssdbOpenConnection(error_handle,client,szDefaultHostName,szDefaultDatabaseName,0)) == NULL)
        {
                sscError(hError,"Database API Error: \"%s\" - can't open connection\n",ssdbGetLastErrorString(error_handle));
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

	        if (!strcasecmp(argv[1], "SGM_EVENT_VIEW"))
                        session->report_type = SGM_EVENT_VIEW;
	        else if (!strcasecmp(argv[1], "SGM_EVENT_DESCRIPTION_CONT"))
                        session->report_type = SGM_EVENT_DESCRIPTION_CONT;
	        else if (!strcasecmp(argv[1], "SGM_EVENT_CLASS_VIEW_CONT"))
                        session->report_type = SGM_EVENT_CLASS_VIEW_CONT;
	        else if (!strcasecmp(argv[1], "SGM_GENERATE_SYSTEMS"))
                        session->report_type = SGM_GENERATE_SYSTEMS;
	        else if (!strcasecmp(argv[1], "SGM_CLASS_LIST"))
                        session->report_type = SGM_CLASS_LIST;
	        else if (!strcasecmp(argv[1], "SGM_UPDATE_EVCLASS_LIST"))
                        session->report_type = SGM_UPDATE_EVCLASS_LIST;
	        else if (!strcasecmp(argv[1], "SGM_UPDATE_EVENT_LIST"))
                        session->report_type = SGM_UPDATE_EVENT_LIST;
	        else if (!strcasecmp(argv[1], "SGM_EVENT_UPDATE_INFO"))
                        session->report_type = SGM_EVENT_UPDATE_INFO;
	        else if (!strcasecmp(argv[1], "SGM_EVENT_UPDATE_CONFIRM"))
                        session->report_type = SGM_EVENT_UPDATE_CONFIRM;
                else
                {
                        sscError(hError, "Invalid Report Request %s",argv[1]);
                }

	switch (session->report_type)
	{
		case SGM_EVENT_VIEW : 
			sgm_event_view(hError,session,connection,error_handle,cmdp);
			break;
		case SGM_EVENT_DESCRIPTION_CONT : 
			event_description_sgm_view(hError,session,connection,error_handle,cmdp);
			break;
		case SGM_EVENT_CLASS_VIEW_CONT : 
			event_description_sgm_class(hError,session,connection,error_handle,cmdp);
			break;
		case SGM_GENERATE_SYSTEMS : 
			generate_list_of_systems(hError,session,connection,error_handle,cmdp);
			break;
		case SGM_CLASS_LIST : 
			sgm_class_list(hError,session,connection,error_handle,cmdp);
			break;
		case SGM_UPDATE_EVCLASS_LIST : 
			sgm_update_evclass_list(hError,session,connection,error_handle,cmdp);
			break;
		case SGM_UPDATE_EVENT_LIST : 
			sgm_update_event_list(hError,session,connection,error_handle,cmdp);
			break;
		case SGM_EVENT_UPDATE_INFO : 
			sgm_event_update_info(hError,session,connection,error_handle,cmdp);
			break;
		case SGM_EVENT_UPDATE_CONFIRM : 
			sgm_event_update_confirm(hError,session,connection,error_handle,cmdp);
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
