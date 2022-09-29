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

#ident           "$Revision: 1.6 $"

#include <ssdbapi.h>
#include "sspingdefs.h"
#include "rgPluginAPI.h"
#include "sscHTMLGen.h"
#include "sscPair.h"
#include "sscShared.h"

service_t         *espservices;
int               numservices;
hostservice_t     *esphservices;
int               numhservices;

int               restartdaemon;

typedef struct sss_s_MYSESSION {
  unsigned long signature;
  struct sss_s_MYSESSION *next;
  int textonly;
} mySession;

static char szVersionStr[64];
static pthread_mutex_t seshFreeListmutex;
static int volatile mutex_inited         = 0;
static mySession *sesFreeList            = 0;

void display_retpage(int, int err, char *format, ...);

#define    SERVICE                       1
#define    HOSTS                         2
#define    VIEWSETUP			 3

/*---------------------------------------------------------------------------*/
/* Some Global Defines                                                       */
/*---------------------------------------------------------------------------*/

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

void display_configuration(void)
{
    int err = 0;
    int i = 0;
    int found = 0;
    int j = 0;
    int found1 = 0;
    int serindex = -1;

/*    if ( (err = readHostServices(MAPFILE)) != 0 ) {
        display_retpage(VIEWSETUP, err, "Error in Hosts");
        return;
    }*/
    readHostServices(MAPFILE);

        TableBegin("BORDER=\"0\" CELLPADDING=\"0\" CELLSPACING=\"0\" WIDTH=\"100%\"");
        RowBegin("");
          CellBegin("WIDTH=\"15\" BGCOLOR=\"#cccc99\"");
          Body("&nbsp;&nbsp;&nbsp;\n");
          CellEnd();
          CellBegin("BGCOLOR=\"#cccc99\"");
            Body("<FONT FACE=\"Arial,Helvetica\">\n");
              Body("SETUP &gt; System Monitor &gt; View Current Setup</FONT>\n");
          CellEnd();
        RowEnd();
        RowBegin("");
          CellBegin("COLSPAN=\"2\"");
          Body("&nbsp;<p>&nbsp;\n");
          CellEnd();
        RowEnd();
        RowBegin("");
          CellBegin("");
          CellEnd();
          CellBegin("");
            TableBegin("BORDER=\"0\" CELLPADDING=\"0\" CELLSPACING=\"0\"");
              RowBegin("");
                CellBegin("COLSPAN=\"3\"");
                  Body("<B>\n");
                    Body("<FONT FACE=\"Arial,Helvetica\">\n");
                    Body("Services available for Monitoring\n");
                    Body("</FONT>\n");
                  Body("</B>\n");
                CellEnd();
              RowEnd();
              RowBegin("");
                CellBegin("COLSPAN=\"3\"");
                  Body("<FONT FACE=\"Arial,Helvetica\">\n");
                  Body("&nbsp; </FONT>\n");
                CellEnd();
              RowEnd();

	      if ( espservices == NULL ) {
		RowBegin("");
		    CellBegin("COLSPAN=\"3\"");
		    Body("No services configured.\n");
		    CellEnd();
                RowEnd();
                RowBegin("");
                  CellBegin("COLSPAN=\"3\"");
                  Body("&nbsp;&nbsp;\n");
                  CellEnd();
                RowEnd();
	      } else {
		  for (i = 0; i < numservices; i++ ) {
		      if ( espservices[i].flag ) {
			  found = 1;
			  RowBegin("");
			    CellBegin("");
			      FormatedBody("%s",espservices[i].service_name);
                            CellEnd();
			    CellBegin("");
			      Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                            CellEnd();
			    CellBegin("");
			      FormatedBody("%s",espservices[i].service_cmd);
                            CellEnd();
			  RowEnd();
                          RowBegin("");
			  CellBegin("COLSPAN=\"3\"");
			  Body("&nbsp;&nbsp;\n");
			  RowEnd();
		      }
		  }

		  if ( found == 0 ) {
		    RowBegin("");
		    CellBegin("COLSPAN=\"3\"");
		    Body("No services configured.\n");
		    CellEnd();
                    RowEnd();
                    RowBegin("");
                    CellBegin("COLSPAN=\"3\"");
                    Body("&nbsp;&nbsp;\n");
                    CellEnd();
                    RowEnd();
		  }
              }

              RowBegin("");
                CellBegin("COLSPAN=\"3\"");
                  Body("<B>\n");
                    Body("<FONT FACE=\"Arial,Helvetica\">\n");
                    Body("Services that are monitored for Hosts\n");
                    Body("</FONT>\n");
                  Body("</B>\n");
                CellEnd();
              RowEnd();
              RowBegin("");
                CellBegin("COLSPAN=\"3\"");
                  Body("<FONT FACE=\"Arial,Helvetica\">\n");
                  Body("&nbsp; </FONT>\n");
                CellEnd();
              RowEnd();


	      if ( esphservices != NULL ) {
	         j = i = found = found1 = 0;
		 for ( i = 0; i < numhservices; i++ ) {
		     found = 1;
			RowBegin("");
			  CellBegin("");
			    FormatedBody("%s",esphservices[i].host_name);
                          CellEnd();
			  CellBegin("");
			      Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
                          CellEnd();
			  CellBegin("");
                     for( j = 0; j <= esphservices[i].nextservice; j++ ) {
			 if ( esphservices[i].services[j] >= 0 ) {
			     found1 = 1;
                             serindex = esphservices[i].services[j];
			     FormatedBody("%s &nbsp;&nbsp;",espservices[serindex].service_name);
			 }
		     }

		     if ( found1 == 0 ) {
			 Body("No services configured\n");
		     }
			CellEnd();

			RowEnd();
			RowBegin("");
			CellBegin("COLSPAN=\"3\"");
			Body("&nbsp;&nbsp;\n");
			CellEnd();
			RowEnd();
		 }
	      } else {
                RowBegin("");
                    CellBegin("COLSPAN=\"3\"");
                    Body("No hosts configured.\n");
                    CellEnd();
                RowEnd();
                RowBegin("");
                  CellBegin("COLSPAN=\"3\"");
                  Body("&nbsp;&nbsp;\n");
                  CellEnd();
                RowEnd();
	      }
           TableEnd();
          CellEnd();
        RowEnd();
        RowBegin("");
          CellBegin("");
          CellEnd();
          CellBegin("");
          CellEnd();
        RowEnd();
        TableEnd();
    return;
}

void display_retpage(int func, int err, char *format, ...)
{
    va_list    args;
    char       errStr[1024];
    int        nobytes = 0;

    if ( format != NULL ) {
	va_start(args, format);
	nobytes = vsnprintf(errStr, sizeof(errStr), format, args);
    }

    switch (err)
    {
	case -1:
	    nobytes += snprintf(&errStr[nobytes], sizeof(errStr)-nobytes,
				"<br>\n");
            break;
	case 0:
	    nobytes += snprintf(&errStr[nobytes], sizeof(errStr)-nobytes,
				" successful. <br>\n");
	    break;
        case INVALIDARGS:
	    nobytes += snprintf(&errStr[nobytes], sizeof(errStr)-nobytes,
				" : Invalid arguments received<br>\n");
	    break;
	case INVALIDOPERATION:
	    nobytes += snprintf(&errStr[nobytes], sizeof(errStr)-nobytes,
				" : Attempt to perform an invalid operation<br>\n");
	    break;
	case INVALIDCOMMAND:
	    nobytes += snprintf(&errStr[nobytes], sizeof(errStr)-nobytes,
				" : Command entered doesn't contain HOST keyword<br>\n");
	    break;
	case SERVICEALREADYEXISTS:
	    nobytes += snprintf(&errStr[nobytes], sizeof(errStr)-nobytes,
				" : A service name same as the one entered already exists<br>\n");
	    break;
	case SERVICENOTFOUND:
	    nobytes += snprintf(&errStr[nobytes], sizeof(errStr)-nobytes,
				" : Attempt to update a service that doesn't exist.  It may have been deleted by someone.<br>\n");
	    break;
	case UNABLETOADDSERVICE:
	    nobytes += snprintf(&errStr[nobytes], sizeof(errStr)-nobytes,
				" : Unable to add service because of memory allocation error<br>\n");
	    break;
	case HOSTNOTFOUND:
	    nobytes += snprintf(&errStr[nobytes], sizeof(errStr)-nobytes,
				" : Selected host is not valid or it doesn't exist any longer<br>\n");
	    break;
	case FILENOTREADABLE:
	    nobytes += snprintf(&errStr[nobytes], sizeof(errStr)-nobytes,
				" : Unable to open Configuration file for reading<br>\n");
	    break;
	case TOOMANYSERVICES:
	    nobytes += snprintf(&errStr[nobytes], sizeof(errStr)-nobytes,
				" : Attempt to add too many services for a single host.<br>\n");
	    break;
	case UNABLETOADDHOST:
	    nobytes += snprintf(&errStr[nobytes], sizeof(errStr)-nobytes,
				" : Unable to add host information retrieved from Database due to memory error<br>\n");
	    break;
	default:
	    nobytes += snprintf(&errStr[nobytes], sizeof(errStr)-nobytes,
				" : Unknown error %d<br>\n", err);
	    break;
    }

    Body("<html>\n");
    Body("<head>\n");
    Body("<title>\n");
    Body("SGI Embedded Support Partner - Ver 1.0\n");
    Body("</title>\n");
    Body("</head>\n");
    Body("<body bgcolor=\"#ffffcc\">\n");
    TableBegin("border=0 cellpadding=0 cellspacing=0 width=100%");
    RowBegin("");
      CellBegin("bgcolor=\"#cccc99\" width=15");
      Body("&nbsp;&nbsp;&nbsp;&nbsp;\n");
      CellEnd();
      CellBegin("bgcolor=\"#cccc99\"");
      Body("SETUP &gt; System Monitor &gt; \n");
      switch(func)
      {
	  case SERVICE:
	      Body("Service \n");
	      break;
	  case HOSTS:
	      Body("Hosts\n");
	      break;
	  case VIEWSETUP:
	      Body("View Current Setup\n");
	      break;
      }
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
        FormatedBody("%s",errStr);
    CellEnd();
    RowEnd();
    TableEnd();
    Body("</body>\n");
    Body("</html>\n");

    return;
}

static void displayService(void)
{
    int  i = 0;

    if ( !espservices ) return;
    if ( numservices < 0 ) return;

    for (i = 0; i < numservices; i++ ) {
	FormatedBody("<option value=\"%s\">%s",espservices[i].service_name,espservices[i].service_name);
    }

    return;
}

static void displayHosts(void)
{
    int   i = 0;

    if ( !esphservices || numhservices < 0 ) return;

    for ( i = 0; i < numhservices; i++ ) {
	FormatedBody("<option value=\"%s\">%s",esphservices[i].host_name,esphservices[i].host_name);
    }

    return;

}

int setHostServiceOpts(CMDPAIR *cmdp, int cmdpsize)
{
    char *hostname = NULL;
    char buffer[1024];
    int  i = 0;
    int  submittype = -1;
    int  err = 0;
    int  nobytes = 0;

    for ( i = 0; i < cmdpsize; i++ ) {
	if ( !strcasecmp(cmdp[i].keyname, "sys_host") ) {
	    hostname = cmdp[i].value;
	} else if ( !strcasecmp(cmdp[i].keyname, "sys_serv" )) {
	    if ( cmdp[i].value ) {
	        nobytes += snprintf(&buffer[nobytes], sizeof(buffer)-nobytes,
				"%s, ", cmdp[i].value);
	    }
	} else if ( !strcasecmp(cmdp[i].keyname, "submit_type") ) {
	    submittype = atoi(cmdp[i].value);
	}
    }

    if ( !hostname ) {
	display_retpage(HOSTS,-1, "Need a hostname to proceed.");
	return(INVALIDARGS);
    } else if ( !nobytes ) {
	display_retpage(HOSTS,-1, "Need to select atleast a single service");
	return(INVALIDARGS);
    } else if ( submittype < 0 ) {
	display_retpage(HOSTS,-1, "No submit type specified");
	return(INVALIDARGS);
    }


    if ( (err = readHostServices(MAPFILE)) != 0 ) {
	display_retpage(HOSTS,err, "Error in Hosts");
        return(err);
    }

    switch(submittype)
    {
	case 0:
	    err = processservice(hostname, buffer, DELETE);
            display_retpage(HOSTS,err, "Deletion of services for Host %s", hostname);
            break;
	case 1:
	    err = processservice(hostname, buffer, UPDATE);
            display_retpage(HOSTS,err, "Update of services for Host %s", hostname);
            break;
	case 2:
	    err = processservice(hostname, buffer, ADD);
            display_retpage(HOSTS,err, "Addition of services for Host %s", hostname);
            break;
	default:
	    display_retpage(HOSTS,-1, "Invalid submit type specified");
	    return(INVALIDARGS);
    }

    return(err);
}

int setServiceOpts(CMDPAIR *cmdp, int cmdpsize, int flag)
{
    char *servicename = NULL;
    char *servicecmd  = NULL;
    int  i = 0;
    int  submittype = -1;
    int  err = 0;

    for ( i = 0; i < cmdpsize; i++ ) {
	if ( !strcasecmp(cmdp[i].keyname, "serv_name") ) {
	    servicename = cmdp[i].value;
	} else if ( !strcasecmp(cmdp[i].keyname, "serv_cmd") ) {
	    servicecmd = cmdp[i].value;
	} else if ( !strcasecmp(cmdp[i].keyname, "sys_serv") ) {
	    servicename = cmdp[i].value;
	} else if ( !strcasecmp(cmdp[i].keyname, "sys_cmd") ) {
	    servicecmd = cmdp[i].value;
	} else if ( !strcasecmp(cmdp[i].keyname, "submit_type")) {
	    submittype = atoi(cmdp[i].value);
	}
    }

    if ( !servicename ) {
	display_retpage(SERVICE, -1, "Need to specify a service name");
	return(INVALIDARGS);
    }

    switch(flag)
    {
	case ADD:
	    if ( !servicecmd ) {
		display_retpage(SERVICE, -1, 
		      "Need to specify a command to add to the service %s", 
		      servicename);
	        return(INVALIDARGS);
	    }
	    else {
		err = modService(servicename, servicecmd, ADD);
		display_retpage(SERVICE, err,"Addition of Service %s",servicename);
		return(err);
	    }
	    break;
	default:
	    if ( submittype == 0 ) {
		err = deleteService(servicename);
		display_retpage(SERVICE, err, "Deletion of Service %s",servicename);
		return(err);
	    } else if (submittype == 1) {
		if ( !servicecmd ) {
		    display_retpage(SERVICE, -1, 
		      "Need to specify a command to add to the service %s", 
		      servicename);
	            return(INVALIDARGS);
		} else {
		    err = modService(servicename, servicecmd, UPDATE);
		    display_retpage(SERVICE, err, "Update of Service %s", servicename);
		    return(err);
		}
	    } else {
		display_retpage(SERVICE, -1, "Invalid submit type received");
		return(INVALIDARGS);
	    }
    }

}

int CheckSSPingDirectory(void)
{
    struct   stat    statbuf;

    if ( stat(SSPINGDIR, &statbuf) == 0 ) return(1);

    return(0);
}

int ConfigureSSPing(void)
{
    struct stat statbuf;
    char   systemcmd[256];
    char   conffile[256];

    snprintf(conffile, sizeof(conffile), "%s/Configure", SSPINGDIR);

    if ( stat(SERVICESFILE, &statbuf) == 0 && 
         stat(MAPFILE, &statbuf) == 0 && 
         stat(conffile, &statbuf) == 0 ) {
        snprintf(systemcmd, sizeof(systemcmd),
                 "cd %s; ./Configure -t %s -m %s 1>&2 >/dev/null &",
                 SSPINGDIR, SERVICESFILE, MAPFILE);
        system(systemcmd);
        return(1);
    }
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
   FILE  *fp = NULL;
   int   confirmation = 0;
   int   submit_type = -1;
   int   proc = 0;
   char  *servicename = NULL;
   char  *servicecmd  = NULL;
   int   checkservicefile;

   mySession *sess = (mySession *)session;

   espservices = NULL;
   numservices = -1;
   esphservices = NULL;
   numhservices = -1;
   restartdaemon = 0;

   if(!sess || sess->signature != sizeof(mySession))
   { 
     sscError(hError,szServerNameErrorPrefix,"Incorrect session id in rgpgGenerateReport()");
     return(RGPERR_SUCCESS);
   }

   if ( argc < 2 ) {
       sscError(hError,szServerNameErrorPrefix,"Wrong number of arguments<br>\n");
       return(RGPERR_SUCCESS);
   }

   if ( !CheckSSPingDirectory() ) {
       sscError(hError,szServerNameErrorPrefix,
		"Unable to find /var/pcp/pmdas/espping directory. Please check if pcp_eoe subsystem is installed <br>\n");
       return(RGPERR_SUCCESS);
   }


   if ( createMyHTMLGenerator(result))
   {
       sscError(hError, szServerNameErrorPrefix, 
		"Can't open the HTML generator<br>\n");
       return(RGPERR_SUCCESS);
   }

   cmdpsize = sscInitPair(rawCommandString,cmdp,MAXKEYS);

   /* Set Browser type */

   for(i = 0;(i = sscFindPairByKey(cmdp,i,szUserAgent)) >= 0;i++)
     if ( !strcasecmp(cmdp[i].value,"Lynx")) sess->textonly = 1;

   checkservicefile = readServices(SERVICESFILE);

   if (  (err = GetHostData() ) != 0 ) {
       display_retpage(0, err, "Error retrieving host information");
       goto end;
   }

   if ( !strcasecmp(argv[1], "displayService") ) {
       if ( checkservicefile > 0 ) displayService();
   } else if ( !strcasecmp(argv[1], "setService") ) {
       if ( checkservicefile < 0 ) {
	   display_retpage(0, FILENOTREADABLE, "Read services");
	   goto end;
       }
       err = setServiceOpts(cmdp, cmdpsize, 0);
   } else if ( !strcasecmp(argv[1], "addService") ) {
       if ( checkservicefile < 0 ) {
	   display_retpage(0, FILENOTREADABLE, "Read services");
	   goto end;
       }
       err = setServiceOpts(cmdp, cmdpsize, ADD);
   } else if ( !strcasecmp(argv[1], "displayHosts") ) {
       displayHosts();
   } else if ( !strcasecmp(argv[1], "setHostService") ) {
       if ( checkservicefile < 0 ) {
	   display_retpage(0, FILENOTREADABLE, "Read services");
	   goto end;
       } else if ( checkservicefile == 0 ) {
	   display_retpage(0, -1, "No services to add to hosts.  Please add a service");
       }
       err = setHostServiceOpts(cmdp, cmdpsize);
   } else if ( !strcasecmp(argv[1], "displayconf") ) {
       display_configuration(); 
   } else {
       display_retpage(0, -1, "Unknown arguments passed. (%s)", argv[1]);
   }

   end:
   deleteMyHTMLGenerator();

   if ( restartdaemon ) {
       ConfigureSSPing();
       restartdaemon = 0;
   }

   if ( espservices ) free(espservices);
   if ( esphservices) free(esphservices);
   espservices = NULL;
   esphservices = NULL;
   return(RGPERR_SUCCESS);
}
