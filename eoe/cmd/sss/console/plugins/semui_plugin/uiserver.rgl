#include "ui.h"

/* ----------------------------- rgpgInit ------------------------------------ */
int RGPGAPI rgpgInit(sscErrorHandle hError)
{ /* Try initialize "free list of mySession" mutex */
  if(pthread_mutex_init(&seshFreeListmutex,0))
  { sscError(hError,szServerNameErrorPrefix,"Can't initialize mutex");
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
/* -------------------------- rgpgCreateSesion ------------------------------- */
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
/* ------------------------- rgpgDeleteSesion -------------------------------- */
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

/* ------------------------ rgpgGetAttribute --------------------------------- */
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

/* --------------------- rgpgFreeAttributeString ------------------------------ */
void RGPGAPI rgpgFreeAttributeString(sscErrorHandle hError,rgpgSessionID session,const char *attributeID,const char *extraAttrSpec,
	char *attrString,int restype)
{  
	if(((restype == RGATTRTYPE_SPECIALALLOC) || (restype == RGATTRTYPE_MALLOCED)) && attrString) free(attrString);
}


/*----------------------------------------rgpgSetAttribute---------------------------------------------*/

void  RGPGAPI rgpgSetAttribute(sscErrorHandle hError, rgpgSessionID session, const char *attributeID, const char *extraAttrSpec, const char *value)
{  char buff[128]; mySession *sess = session;

   if(!strcasecmp(attributeID,szVersion) || !strcasecmp(attributeID,szTitle) ||
      !strcasecmp(attributeID,szThreadSafe) || !strcasecmp(attributeID,szUnloadable) ||
      !strcasecmp(attributeID,szUnloadTime))
   { sscError(hError,szServerNameErrorPrefix,"Attempt to set read only attribute in rgpgSetAttribute()");
     return;
   }

   if(!strcasecmp(attributeID,"User-Agent") && value)
   { sess->textonly = (strcasecmp(value,"Lynx") == 0) ? 1 : 0;
     return;
   }
   if(!sess || sess->signature != sizeof(mySession))
   { sscError(hError,szServerNameErrorPrefix,"Incorrect session id in rgpgGetAttribute()");
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
                return 0;
        }


	/*  Initialize The HTML Generator  */

	i = createMyHTMLGenerator(result);
	if (i != 0)
	{
		sscError(hError,  "Sample server: Can't open the HTML generator");
		ssdbDeleteErrorHandle(error_handle);
		return 0;
	}

	if (argc < 2)
	{
		sscError(hError, "Invalid Request\n");
		ssdbDeleteErrorHandle(error_handle);
		return 0;
	}

	cmdpsize = sscInitPair(rawCommandString,cmdp,MAXNUMBERS_OF_KEYS);

	   /* Check WEB browser type (Lynx is only tex based) */
   for(i = 0;(i = sscFindPairByKey(cmdp,i,szUserAgent)) >= 0;i++)
    if(!strcasecmp(cmdp[i].value,"Lynx")) session->textonly = 1;

		/* Utility functions */
	        if (!strcasecmp(argv[1], "actionlist"))
                        session->report_type = ACTIONLIST;
                else if (!strcasecmp(argv[1],"typelist"))
                        session->report_type = TYPELIST;
                else if (!strcasecmp(argv[1],"eventlist"))
                        session->report_type = EVENTLIST;
                else if (!strcasecmp(argv[1],"GETSYSID"))
                        session->report_type = GETSYSID;
                else if (!strcasecmp(argv[1],"GETSYSID_UNHID"))
                        session->report_type = GETSYSID_UNHID;

		/* Action setup */
                else if (!strcasecmp(argv[1],"updateaction"))
                        session->report_type = UPDATEACTION;
                else if (!strcasecmp(argv[1],"updtactconfirm"))
                        session->report_type = UPDTACTCONFIRM;
                else if (!strcasecmp(argv[1],"setupdate"))
                        session->report_type = SETUPDATE;
                else if (!strcasecmp(argv[1],"deleteaction"))
                        session->report_type = DELETEACTION;
                else if (!strcasecmp(argv[1],"UPDATE_EVENT"))
                        session->report_type = UPDATE_EVENT;
                else if (!strcasecmp(argv[1],"RETURNHOSTNAME"))
                        session->report_type = RETURNHOSTNAME;
                else if (!strcasecmp(argv[1],"DELETELISTSET"))
                        session->report_type = DELETELISTSET;


		/* Event Reports */
                else if (!strcasecmp(argv[1],"EVENT_REPORT_BY_HOST"))
                        session->report_type = EVENT_REPORT_BY_HOST;
                else if (!strcasecmp(argv[1],"EVENT_REPORT_BY_TYPE_HOST"))
                        session->report_type = EVENT_REPORT_BY_TYPE_HOST;
                else if (!strcasecmp(argv[1],"EVENT_REPORT_PAGE"))
                        session->report_type = EVENT_REPORT_PAGE;
                else if (!strcasecmp(argv[1],"EVENT_REPORT_BY_CLASS_HOST"))
                        session->report_type = EVENT_REPORT_BY_CLASS_HOST;
                else if (!strcasecmp(argv[1],"EVENT_REPORT_TYPE_PAGE"))
                        session->report_type = EVENT_REPORT_TYPE_PAGE;
                else if (!strcasecmp(argv[1],"EVREPORT_CONT"))
                        session->report_type = EVREPORT_CONT;
                else if (!strcasecmp(argv[1],"CLASSREPORT_CONT"))
                        session->report_type = CLASSREPORT_CONT;
                else if (!strcasecmp(argv[1],"TYPEREPORT_CONT"))
                        session->report_type = TYPEREPORT_CONT;
                else if (!strcasecmp(argv[1],"EVENT_VIEW_PAGE_CONT"))
                        session->report_type = EVENT_VIEW_PAGE_CONT;
                else if (!strcasecmp(argv[1],"EVENT_VIEW_CLASS_PAGE_CONT"))
                        session->report_type = EVENT_VIEW_CLASS_PAGE_CONT;

		/* Actions Reports */
                else if (!strcasecmp(argv[1],"ACTION_VIEW_CONT"))
                        session->report_type = ACTION_VIEW_CONT;
                else if (!strcasecmp(argv[1],"ACTION_TAKEN_REPORT"))
                        session->report_type = ACTION_TAKEN_REPORT;
                else if (!strcasecmp(argv[1],"ACTION_REPORT_BY_ACTION"))
                        session->report_type = ACTION_REPORT_BY_ACTION;
                else if (!strcasecmp(argv[1],"ACTIONS_PAGE"))
                        session->report_type = ACTIONS_PAGE;
                else if (!strcasecmp(argv[1],"EVENT_ACTION_SPECIFIC_REPORT"))
                        session->report_type = EVENT_ACTION_SPECIFIC_REPORT;
                else if (!strcasecmp(argv[1],"EVENT_ACTIONS_PAGE"))
                        session->report_type = EVENT_ACTIONS_PAGE;

		/* Event Setup */
		else if (!strcasecmp(argv[1],"EVENT_VIEW_PAGE"))
                        session->report_type = EVENT_VIEW_PAGE;
		else if (!strcasecmp(argv[1],"EVENT_VIEW_TYPE_PAGE"))
                        session->report_type = EVENT_VIEW_TYPE_PAGE;
                else if (!strcasecmp(argv[1],"EVENT_VIEW_REPORT_PAGE"))
                        session->report_type = EVENT_VIEW_REPORT_PAGE;
                else if (!strcasecmp(argv[1],"EVENT_ADD_INFO"))
                        session->report_type = EVENT_ADD_INFO;
                else if (!strcasecmp(argv[1],"EVENT_ADD_CONFIRM"))
                        session->report_type = EVENT_ADD_CONFIRM;
                else if (!strcasecmp(argv[1],"CREATE_CLASS_LIST"))
                        session->report_type = CREATE_CLASS_LIST;
                else if (!strcasecmp(argv[1],"EVENT_UPDT_INFO"))
                        session->report_type = EVENT_UPDT_INFO;
                else if (!strcasecmp(argv[1],"EVENT_UPDT_CONFIRM"))
                        session->report_type = EVENT_UPDT_CONFIRM;
                else if (!strcasecmp(argv[1],"EVENT_UPDATE_ACTION_LIST"))
                        session->report_type = EVENT_UPDATE_ACTION_LIST;
                else if (!strcasecmp(argv[1],"EVENT_ACTION_AUD"))
                        session->report_type = EVENT_ACTION_AUD;
                else if (!strcasecmp(argv[1],"EVENT_ACTION_ADD_CONFIRM"))
                        session->report_type = EVENT_ACTION_ADD_CONFIRM;
                else if (!strcasecmp(argv[1],"EVENT_ACTION_UPD_CONFIRM"))
                        session->report_type = EVENT_ACTION_UPD_CONFIRM;
                else if (!strcasecmp(argv[1],"EVENT_DELETE"))
                        session->report_type = EVENT_DELETE;
                else if (!strcasecmp(argv[1],"EVENT_DELETE_LIST"))
                        session->report_type = EVENT_DELETE_LIST;
                else if (!strcasecmp(argv[1],"ARCHIVE_LIST"))
                        session->report_type = ARCHIVE_LIST;
                else if (!strcasecmp(argv[1],"DELETE_ARCHIVE"))
                        session->report_type = DELETE_ARCHIVE;
                else if (!strcasecmp(argv[1],"ARCHIVE_TABLE"))
                        session->report_type = ARCHIVE_TABLE;

                else
                {
                        sscError(hError, "Invalid Report Request %s",argv[1]);
                }

	switch (session->report_type)
	{
		case ACTIONLIST : 
			create_action_list(hError,session,connection,error_handle,cmdp);
			break;
		case EVENTLIST :
			create_event_list(hError,session,connection,error_handle,cmdp);		
			break;
		case TYPELIST :
			create_type_list(hError,session,connection,error_handle,cmdp);		
			break;
		case UPDATEACTION :
			action_parameters(hError,session,connection,error_handle,cmdp);
			break;
		case UPDTACTCONFIRM :
			update_action_params(hError,session,connection,error_handle,cmdp);
			break;
		case DELETEACTION :
			delete_action(hError,session,connection,error_handle,cmdp);		
			break; 
		case RETURNHOSTNAME :
			generate_hostname(hError,session,connection,error_handle);
			break;
		case GETSYSID:
			generate_sysid(hError,session,connection,error_handle);
			break;
		case GETSYSID_UNHID:
			generate_sysid_unhidden(hError,session,connection,error_handle);
			break;
		case DELETELISTSET:
			deletelistset(hError,session,connection,error_handle,cmdp);
			break;
		case EVENT_REPORT_BY_HOST:
			event_report_by_host(hError,session,connection,error_handle,cmdp);
			break;
		case EVREPORT_CONT:
			event_report_by_host(hError,session,connection,error_handle,cmdp);
			break;
		case CLASSREPORT_CONT:
			event_report_by_class_host(hError,session,connection,error_handle,cmdp);
			break;
		case TYPEREPORT_CONT:
			event_report_by_type_host(hError,session,connection,error_handle,cmdp);
			break;
		case EVENT_REPORT_BY_CLASS_HOST:
			event_report_by_class_host(hError,session,connection,error_handle,cmdp);
			break;
		case EVENT_REPORT_BY_TYPE_HOST:
			event_report_by_type_host(hError,session,connection,error_handle,cmdp);
			break;
		case EVENT_REPORT_PAGE:
			event_report_page(hError,session,connection,error_handle,cmdp);
			break;
		case EVENT_REPORT_TYPE_PAGE:
			event_report_type_page(hError,session,connection,error_handle,cmdp);
			break;
		case ACTION_TAKEN_REPORT:
			event_action_report(hError,session,connection,error_handle,cmdp);
			break;
		case EVENT_ACTION_SPECIFIC_REPORT:
			event_action_specific_report(hError,session,connection,error_handle,cmdp);
			break;
		case EVENT_ACTIONS_PAGE:
			event_actions_page(hError, session,connection,error_handle,cmdp);
			break;
		case ACTION_VIEW_CONT:
			view_action_page(hError,session,connection,error_handle,cmdp);
			break;
		case ACTIONS_PAGE:
			view_action_page(hError, session,connection,error_handle,cmdp);
			break;
		case ACTION_REPORT_BY_ACTION:
			action_report_by_action(hError, session,connection,error_handle,cmdp);
			break;

		case EVENT_VIEW_PAGE:
			event_view_page(hError, session,connection,error_handle,cmdp);
			break;
		case EVENT_VIEW_PAGE_CONT:
			event_description_view(hError, session,connection,error_handle,cmdp);
			break;
		case EVENT_VIEW_CLASS_PAGE_CONT:
			event_description_class(hError, session,connection,error_handle,cmdp);
			break;
		case EVENT_VIEW_TYPE_PAGE:
			event_view_type_configuration(hError, session,connection,error_handle,cmdp);
			break;
		case EVENT_VIEW_REPORT_PAGE:
			event_view_report_page(hError, session,connection,error_handle,cmdp);
			break;
		case EVENT_ADD_INFO:
			event_add_info(hError, session,connection,error_handle,cmdp);
			break;
		case EVENT_ADD_CONFIRM:
			event_add_confirm(hError, session,connection,error_handle,cmdp);
			break;
		case CREATE_CLASS_LIST:
			generate_class_list(hError,session,connection,error_handle);
			break;
		case EVENT_UPDT_INFO:
			event_updt_info(hError,session,connection,error_handle,cmdp);
			break;
		case EVENT_UPDT_CONFIRM:
			event_updt_confirm(hError,session,connection,error_handle,cmdp);
			break;
		case EVENT_UPDATE_ACTION_LIST:
			event_update_action_list(hError,session,connection,error_handle,cmdp);
			break;
		case EVENT_ACTION_AUD:
			event_action_aud(hError,session,connection,error_handle,cmdp);
			break;
		case EVENT_ACTION_ADD_CONFIRM:
			event_action_add_confirm(hError,session,connection,error_handle,cmdp);
			break;
		case EVENT_ACTION_UPD_CONFIRM:
			event_action_update_confirm(hError,session,connection,error_handle,cmdp);
			break;
		case EVENT_DELETE:
			event_delete(hError,session,connection,error_handle,cmdp);
			break;
		case EVENT_DELETE_LIST:
			event_delete_list(hError,session,connection,error_handle,cmdp);
			break;
		case ARCHIVE_LIST:
			archive_list(hError,session,connection,error_handle,cmdp);
			break;
		case DELETE_ARCHIVE:
			delete_archive(hError,session,connection,error_handle,cmdp);
			break;
		case ARCHIVE_TABLE:
			archive_table(hError,session,connection,error_handle,cmdp);
			break;

	}

	/*  Destroy The HTML Generator  */
	i = deleteMyHTMLGenerator();
	if (i != 0)
	{
		sscError(hError,  "Sample server: Can't destroy the HTML generator");
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
