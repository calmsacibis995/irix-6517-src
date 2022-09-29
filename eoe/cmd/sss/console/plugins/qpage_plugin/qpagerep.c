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

#ident           "$Revision: 1.11 $"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <pthread.h>
#include <sys/types.h>
#include "rgPluginAPI.h"
#include "sscHTMLGen.h"
#include "sscPair.h"
#include "sscShared.h"


typedef struct sss_s_MYSESSION {
  unsigned long signature; 
  struct sss_s_MYSESSION *next;
  int textonly;  
} mySession;

/*---------------------------------------------------------------------------*/
/* Some Global Defines                                                       */
/*---------------------------------------------------------------------------*/

#ifdef __TIME__
#ifdef __DATE__
#define INCLUDE_TIME_DATE_STRINGS 1
#endif
#endif

#define DELETE                           1
#define ADD                              2
#define PAGER                            1
#define ADMIN                            2
#define SERVICE                          3
#define MODEM                            4
#define CONFIGFILE                       "/etc/qpage.cf"
#define MYVERSION_MAJOR                  1
#define MYVERSION_MINOR                  0
#define ALLOCSTRUCT                      1
#define NOALLOCSTRUCT                    0
#define MAXKEYS                          512


static const char myLogo[]               = "ESP QPage Setup Server";
static const char szVersion[]            = "Version";
static const char szTitle[]              = "Title";
static const char szThreadSafe[]         = "ThreadSafe";
static const char szUnloadable[]         = "Unloadable";
static const char szUnloadTime[]         = "UnloadTime";
static const char szAcceptRawCmdString[] = "AcceptRawCmdString";
static const char szUserAgent[]          = "User-Agent";
static char szServerNameErrorPrefix[]    = "ESP QPage Setup Server Error: %s";
static const char szUnloadTimeValue[]    = "300";
static const char szThreadSafeValue[]    = "0";
static const char szUnloadableValue[]    = "1";
static const char szAcceptRawCmdStringValue[] = "1";

static char szVersionStr[64];
static pthread_mutex_t seshFreeListmutex;
static int volatile mutex_inited         = 0;
static mySession *sesFreeList            = 0;
static int       writetofile             = 0;
static int       readfile                = 0;

/*---------------------------------------------------------------------------*/
/* Structures                                                                */
/*---------------------------------------------------------------------------*/

typedef struct pager_s {
    struct    pager_s *next;
    char      *name;
    char      *pagertext;
    char      *pagerid;
    char      *sername;
} pager_t;

typedef struct service_s {
    struct    service_s  *next;
    char      *name;
    char      *sertext;
    char      *modname;
    char      *phone;
    char      *passwd;
    char      *parity;
    char      *allowpid;
    int       baudrate;
    int       maxmsgsize;
    int       maxtries;
} service_t;

typedef struct modem_s {
    struct    modem_s  *next;
    char      *name;
    char      *modtext;
    char      *device;
    char      *initcmd;
    char      *dialcmd;
} modem_t;

typedef struct qpageconfig_s {
    char       *admin;
    char       *forcehostname;
    char       *queuedir;
    char       *lockdir;
    int        identtimeout;
    int        snpptimeout;
    char       *synchronous;
} qpageconfig_t;

/*---------------------------------------------------------------------------*/
/* Static Global Structures                                                  */
/*---------------------------------------------------------------------------*/

static qpageconfig_t  currconfig;
static modem_t        *modem = NULL;
static service_t      *service = NULL;
static pager_t        *pager   = NULL;

/*---------------------------------------------------------------------------*/
/* Function Prototypes                                                       */
/*---------------------------------------------------------------------------*/

static void print_header(FILE *);
static void dump_pagerconfig(FILE *);
static void dump_serviceconfig(FILE *);
static void dump_modemconfig(FILE *);
static void dump_conf_file(FILE *);
static void freeservice(service_t *);
static int deleteservice(char *);
static void freepager(pager_t *);
static int deletepager(char *);
static void freemodem(modem_t *);
static int deletemodem(char *);
static void free_config(void);
static pager_t *checkpager(char *, int);
static void addpager(char *, char *);
static service_t *checkservice(char *, int);
static void addservice(char *, char *);
static modem_t *checkmodem(char *, int);
static void addmodem(char *, char *);
static void process_qpageopts(char *, int);
static int process_buf(char *);
static int read_conf_file(char *);

/*---------------------------------------------------------------------------*/
/* Report Generator Interfaces                                               */
/*---------------------------------------------------------------------------*/

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
	if (readfile) {
	   if ( writetofile == 1 ) {
             if ( (fp = fopen(CONFIGFILE, "w")) != NULL) {
               dump_conf_file(fp);
               fclose(fp);
	       system("/etc/killall -HUP qpage.d");
             } else {
	       free_config();
               sscError(hError, szServerNameErrorPrefix, "Unable to open %s for writing data\n", CONFIGFILE);
	       return(0);
             }
	   }
	   free_config();
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

static void print_header(FILE *fp)
{
    time_t   mytime = time(0);

    if ( !fp ) return;

    fprintf(fp, "#\n");
    fprintf(fp, "# File : /etc/qpage.cf (QuickPage Configuration File)\n");
    fprintf(fp, "#\n");
    fprintf(fp, "# This file is automatically updated by ESP Server. Any\n");
    fprintf(fp, "# changes made to this file will be lost.  Please use\n");
    fprintf(fp, "# ESP Web Interface to update this file.\n#\n");
    fprintf(fp, "# Last Updated by %s on %s", myLogo, ctime(&mytime));
    fprintf(fp, "#\n#\n\n");
}


/*---------------------------------------------------------------------------*/
/* dump_pagerconfig                                                          */
/*   Dumps Pager Configuration into file                                     */
/*---------------------------------------------------------------------------*/
static void dump_pagerconfig(FILE *fp)
{
    pager_t  *tmppager = pager;
    service_t *tmpservice = NULL;


    if (!fp) return; fprintf(fp, "\n# Given below are the Pagers available for QuickPage\n\n");
    while (tmppager != NULL ) {
	if ( (tmpservice = checkservice(tmppager->sername, NOALLOCSTRUCT))) {
            if (tmppager->name) fprintf(fp,"pager=%s\n",tmppager->name);
	    if (tmppager->pagertext) fprintf(fp,"\ttext=%s\n", tmppager->pagertext);
	    if (tmppager->sername) fprintf(fp,"\tservice=%s\n",tmppager->sername);
	    if (tmppager->pagerid) fprintf(fp,"\tpagerid=%s\n",tmppager->pagerid);
	} 
        tmppager=tmppager->next;
    }
}

/*---------------------------------------------------------------------------*/
/* dump_serviceconfig                                                        */
/*   Dumps Service Configuration into file                                   */
/*---------------------------------------------------------------------------*/

static void dump_serviceconfig(FILE *fp)
{
    service_t  *tmpservice = service;
    modem_t    *tmpmodem   = NULL;

    if (!fp) return;
    fprintf(fp, "\n# Given below are the Services available for QuickPage\n\n");
    while(tmpservice != NULL ) {
	if ( (tmpmodem = checkmodem(tmpservice->modname, NOALLOCSTRUCT))) {
	    if ( tmpservice->name ) fprintf(fp, "service=%s\n", tmpservice->name);
	    if (tmpservice->sertext) fprintf(fp,"\ttext=%s\n",tmpservice->sertext);
	    if (tmpservice->modname) fprintf(fp,"\tdevice=%s\n",tmpservice->modname);
	    if (tmpservice->phone) fprintf(fp,"\tphone=%s\n",tmpservice->phone);
	    if (tmpservice->passwd) fprintf(fp,"\tpassword=%s\n",tmpservice->passwd);
	    if (tmpservice->parity) fprintf(fp,"\tparity=%s\n",tmpservice->parity);
	    else fprintf(fp, "\tparity=even\n");
	    if (tmpservice->allowpid) fprintf(fp,"\tallowpid=%s\n",tmpservice->allowpid);
	    else fprintf(fp, "\tallowpid=yes\n");
	    if (tmpservice->baudrate) fprintf(fp,"\tbaudrate=%d\n",tmpservice->baudrate);
	    if (tmpservice->maxmsgsize) fprintf(fp,"\tmaxmsgsize=%d\n",tmpservice->maxmsgsize);
	    if (tmpservice->maxtries) fprintf(fp,"\tmaxtries=%d\n",tmpservice->maxtries);
	} 
	tmpservice = tmpservice->next;
    }
}

/*---------------------------------------------------------------------------*/
/* dump_modemconfig                                                          */
/*   Dumps Modem configuration into file                                     */
/*---------------------------------------------------------------------------*/

static void dump_modemconfig(FILE *fp)
{
    modem_t   *tmpmodem = modem;

    if ( !fp ) return;

    fprintf(fp, "\n# Modems for dialing out\n\n");
    while ( tmpmodem != NULL ) {
	if (tmpmodem->name) fprintf(fp, "modem=%s\n", tmpmodem->name);
	if (tmpmodem->modtext) fprintf(fp, "\ttext=%s\n", tmpmodem->modtext);
	if (tmpmodem->device) fprintf(fp, "\tdevice=%s\n", tmpmodem->device);
	if (tmpmodem->initcmd) fprintf(fp,"\tinitcmd=%s\n",tmpmodem->initcmd);
	if (tmpmodem->dialcmd) fprintf(fp,"\tdialcmd=%s\n",tmpmodem->dialcmd);
	tmpmodem = tmpmodem->next;
    }
    return;
}

/*---------------------------------------------------------------------------*/
/* dump_conf_file()                                                          */
/*   Dumps information into configuration file                               */
/*---------------------------------------------------------------------------*/

static void dump_conf_file(FILE *fp)
{
    if ( !fp ) return;

    print_header(fp);
    fprintf(fp, "# QuickPage Administration Variables\n\n");

    if ( currconfig.admin ) {
	fprintf(fp, "administrator=%s\n", currconfig.admin);
    }
    if ( currconfig.forcehostname) {
	fprintf(fp, "forcehostname=%s\n", currconfig.forcehostname);
    }
    if ( currconfig.queuedir ) {
	fprintf(fp, "queuedir=%s\n", currconfig.queuedir);
    }
    if ( currconfig.lockdir  ) {
	fprintf(fp, "lockdir=%s\n", currconfig.lockdir);
    }
    if ( currconfig.identtimeout ) {
	fprintf(fp, "identtimeout=%d\n", currconfig.identtimeout);
    }
    if ( currconfig.snpptimeout ) {
	fprintf(fp, "snpptimeout=%d\n", currconfig.snpptimeout);
    }
    if ( currconfig.synchronous ) {
	fprintf(fp, "synchronous=%s\n", currconfig.synchronous);
    }
    dump_modemconfig(fp);
    dump_serviceconfig(fp);
    dump_pagerconfig(fp);
}



/*---------------------------------------------------------------------------*/
/* freeservice                                                               */
/*   Frees up a Service List                                                 */
/*---------------------------------------------------------------------------*/

static void freeservice(service_t *s)
{
    if (s) {
	if ( s->name) free(s->name);
	if ( s->sertext) free(s->sertext);
	if ( s->modname) free(s->modname);
	if ( s->phone) free(s->phone);
	if ( s->passwd) free(s->passwd);
	if ( s->parity) free(s->parity);
	if ( s->allowpid) free(s->allowpid);
    }
}

/*---------------------------------------------------------------------------*/
/* deleteservice                                                             */
/*   Deletes a service node from service list                                */
/*---------------------------------------------------------------------------*/

static int deleteservice(char *sername)
{
    service_t   *tmpservice1 = service;
    service_t   *tmpservice2 = NULL;
    int         j = 0;

    if ( !sername ) return(-1);

    while ( tmpservice1 != NULL ) {
        if ( !strcasecmp(tmpservice1->name, sername) ) {
            break;
        }
        j++;
        tmpservice2 = tmpservice1;
        tmpservice1 = tmpservice1->next;
    }

    if ( tmpservice1 == NULL ) return(0);

    if ( j == 0 ) {
        /* Match found in the first node */
        service = tmpservice1->next;
    } else {
        tmpservice2->next = tmpservice1->next;
    }

    freeservice(tmpservice1);
    return(1);
}


/*---------------------------------------------------------------------------*/
/* freepager                                                                 */
/*   Frees up a Pager List                                                   */
/*---------------------------------------------------------------------------*/

static void freepager(pager_t *p)
{
    if ( p ) {
	if ( p->pagertext) free(p->pagertext);
	if ( p->name) free(p->name);
	if ( p->sername) free(p->sername);
	if ( p->pagerid) free(p->pagerid);
    }
}

/*---------------------------------------------------------------------------*/
/* deletepager                                                               */
/*   Deletes a pager from Pager List                                         */
/*---------------------------------------------------------------------------*/

static int deletepager(char *pagername)
{
    pager_t    *tmppager1 = pager;
    pager_t    *tmppager2 = NULL;
    int        j = 0;

    if ( !pagername ) return(-1);

    while ( tmppager1 != NULL ) {
        if ( !strcasecmp(tmppager1->name, pagername) ) {
            break;
        }
        j++;
        tmppager2 = tmppager1;
        tmppager1 = tmppager1->next;
    }

    if ( tmppager1 == NULL ) return(0);

    if ( j == 0 ) {
        pager = tmppager1->next;
    } else {
        tmppager2->next = tmppager1->next;
    }

    freepager(tmppager1);
    return(1);
}


/*---------------------------------------------------------------------------*/
/* freemodem                                                                 */
/*   Frees up a Modem List                                                   */
/*---------------------------------------------------------------------------*/

static void freemodem(modem_t *m)
{
    if (m) {
        if ( m->name ) free(m->name);
	if ( m->modtext) free(m->modtext);
	if ( m->device)  free(m->device);
	if ( m->initcmd) free(m->initcmd);
	if ( m->dialcmd) free(m->dialcmd);
    }
}

/*---------------------------------------------------------------------------*/
/* deletemodem                                                               */
/*   deletes a modem from modem List                                         */
/*---------------------------------------------------------------------------*/

static int deletemodem(char *modname)
{
    modem_t   *tmpmodem1 = modem;
    modem_t   *tmpmodem2 = NULL;
    int       j = 0;

    if ( !modname ) return(-1);

    while ( tmpmodem1 != NULL ) {
        if ( !strcasecmp(tmpmodem1->name, modname) ) {
            break;
        }
        j++;
        tmpmodem2 = tmpmodem1;
        tmpmodem1 = tmpmodem1->next;
    }

    if ( tmpmodem1 == NULL ) return(0);

    if ( j == 0 ) modem = tmpmodem1->next;
    else tmpmodem2->next = tmpmodem1->next;

    freemodem(tmpmodem1);
    return(1);
}

/*---------------------------------------------------------------------------*/
/* free_config                                                               */
/*   Frees up all the qpage configuration that was read-in                   */
/*---------------------------------------------------------------------------*/

static void free_config(void)
{
    modem_t   *tmpmodem, *tmpmodem1;
    service_t *tmpservice, *tmpservice1;
    pager_t   *tmppager, *tmppager1;

    /* Free Pagers */
    tmppager = pager;
    while (tmppager) {
	tmppager1 = tmppager->next;
	freepager(tmppager);
	tmppager = tmppager1;
    }
    pager = NULL;

    tmpservice = service;
    while (tmpservice) {
	tmpservice1 = tmpservice->next;
	freeservice(tmpservice);
	tmpservice = tmpservice1;
    }
    service = NULL;

    tmpmodem = modem;
    while (tmpmodem) {
	tmpmodem = tmpmodem->next;
	freemodem(tmpmodem);
	tmpmodem = tmpmodem;
    }
    modem = NULL;

    if ( currconfig.admin ) free(currconfig.admin);
    if ( currconfig.forcehostname) free(currconfig.forcehostname);
    if ( currconfig.queuedir ) free(currconfig.queuedir);
    if ( currconfig.lockdir  ) free(currconfig.lockdir);
    memset(&currconfig, 0, sizeof(currconfig));
    readfile = 0;
}

/*---------------------------------------------------------------------------*/
/* checkpager                                                                */
/*   Checks whether an entry for given name exists in the linked list        */
/*   and if not, allocates it and puts the name in the allocated pager       */
/*   struct.                                                                 */
/*---------------------------------------------------------------------------*/

static pager_t *checkpager(char *name, int flag)
{
    pager_t *tmppager = NULL;
    pager_t *tmppager1 = NULL;
    int       found = 0;

    if ( !name ) return(NULL);
    tmppager = pager;

    if ( tmppager != NULL ) {
	while(tmppager->next != NULL) {
	    if ( strcmp(tmppager->name, name) == 0 ) {
		found = 1;
		break;
	    }
	    tmppager = tmppager->next;
        }

        if ( !found && !strcmp(tmppager->name, name)) {
		found = 1;
        }
    }

    if ( !found ) {
        if ( !flag ) return(NULL);
	if ( (tmppager1 = (pager_t *) calloc(1, sizeof(pager_t))) == NULL){
	    return(NULL);
	}

        tmppager1->name = strdup(name);
	if ( pager == NULL ) {
	    tmppager = pager = tmppager1;
	} else {
	    tmppager->next = tmppager1;
	    tmppager = tmppager->next;
	}
    }

    return(tmppager);
}

/*---------------------------------------------------------------------------*/
/* addpager                                                                  */
/*   Takes 2 arguments, str which points to the name string and str1 which   */
/*   points to the option string and updates the name with given options.    */
/*---------------------------------------------------------------------------*/

static void addpager(char *str, char *str1)
{
    char *tmpBuffer = str1;
    char *tmpBuffer1 = NULL;
    char *bufval = NULL;
    pager_t *tmppager = NULL;


    strtok(str, "=");
    bufval = strtok(NULL, "=");

    tmppager = checkpager(bufval, ALLOCSTRUCT);

    if ( tmppager == NULL ) return;

    /* Now, lets look at other values */

    if ( str1 ) {
        while( (tmpBuffer1 = strtok_r(tmpBuffer, " \t\n", &bufval)) != NULL ) {

            if ( !strncmp(tmpBuffer1, "text=", 5) ) {
                if ( tmppager->pagertext) free(tmppager->pagertext);
                tmppager->pagertext = strdup(tmpBuffer1+5);
            } else if ( !strncmp(tmpBuffer1, "pagerid=", 8) ) {
                if ( tmppager->pagerid) free(tmppager->pagerid);
                tmppager->pagerid = strdup(tmpBuffer1+8);
            } else if ( !strncmp(tmpBuffer1, "service=", 8) ) {
                if ( tmppager->sername) free(tmppager->sername);
                tmppager->sername = strdup(tmpBuffer1+8);
            } 

            tmpBuffer = NULL;
        }
    }

    return;
}

/*---------------------------------------------------------------------------*/
/* checkservice                                                              */
/*   Checks whether an entry for given name exists in the linked list        */
/*   and if not, allocates it and puts the name in the allocated service     */
/*   struct.                                                                 */
/*---------------------------------------------------------------------------*/

static service_t *checkservice(char *name, int flag)
{
    service_t *tmpservice = NULL;
    service_t *tmpservice1 = NULL;
    int       found = 0;

    if ( !name ) return(NULL);
    tmpservice = service;

    if ( tmpservice != NULL ) {
	while(tmpservice->next != NULL) {
	    if ( strcmp(tmpservice->name, name) == 0 ) {
		found = 1;
		break;
	    }
	    tmpservice = tmpservice->next;
        }

	if ( !found && !strcmp(tmpservice->name, name)) {
		found = 1;
        }
    }

    if ( !found ) {
	if ( !flag ) return(NULL);
	if ( (tmpservice1 = (service_t *) calloc(1, sizeof(service_t))) == NULL){
	    return(NULL);
	}

	tmpservice1->name = strdup(name);
	if ( service == NULL ) {
	    tmpservice = service = tmpservice1;
	} else {
	    tmpservice->next = tmpservice1;
	    tmpservice = tmpservice->next;
	}
    } 

    return(tmpservice);
}

/*---------------------------------------------------------------------------*/
/* addservice                                                                */
/*   Takes 2 arguments, str which points to the name string and str1 which   */
/*   points to the option string and updates the name with given options.    */
/*---------------------------------------------------------------------------*/

static void addservice(char *str, char *str1)
{
    char *tmpBuffer = str1;
    char *tmpBuffer1 = NULL;
    char *bufval = NULL;
    service_t *tmpservice = NULL;


    strtok(str, "=");
    bufval = strtok(NULL, "=");

    tmpservice = checkservice(bufval, ALLOCSTRUCT);

    if ( tmpservice == NULL ) return;

    /* Now, lets look at other values */

    if ( str1 ) {
        while( (tmpBuffer1 = strtok_r(tmpBuffer, " \t\n", &bufval)) != NULL ) {

            if ( !strncmp(tmpBuffer1, "text=", 5) ) {
		if (tmpservice->sertext != NULL) free(tmpservice->sertext);
                tmpservice->sertext = strdup(tmpBuffer1+5);
            } else if ( !strncmp(tmpBuffer1, "device=", 7)) {
		if (tmpservice->modname != NULL) free(tmpservice->modname);
                tmpservice->modname = strdup(tmpBuffer1+7);
            } else if ( !strncmp(tmpBuffer1, "phone=", 6)) {
		if (tmpservice->phone != NULL) free(tmpservice->phone);
                tmpservice->phone = strdup(tmpBuffer1+6);
            } else if ( !strncmp(tmpBuffer1, "password=", 9)) {
		if (tmpservice->passwd != NULL) free(tmpservice->passwd);
                tmpservice->passwd = strdup(tmpBuffer1+9);
            } else if ( !strncmp(tmpBuffer1, "baudrate=", 9)) {
                tmpservice->baudrate = atoi(tmpBuffer1+9);
	    } else if ( !strncmp(tmpBuffer1, "parity=", 7)) {
		if (tmpservice->parity != NULL) free(tmpservice->parity);
                tmpservice->parity = strdup(tmpBuffer1+7);
	    } else if ( !strncmp(tmpBuffer1, "allowpid=", 9)) {
		if (tmpservice->allowpid != NULL) free(tmpservice->allowpid);
                tmpservice->allowpid = strdup(tmpBuffer1+9);
	    } else if ( !strncmp(tmpBuffer1, "maxmsgsize=", 11)) {
                tmpservice->maxmsgsize = atoi(tmpBuffer1+11);
	    } else if ( !strncmp(tmpBuffer1, "maxtries=", 9)) {
                tmpservice->maxtries = atoi(tmpBuffer1+9);
	    }

            tmpBuffer = NULL;
        }
    }

    return;
}

/*---------------------------------------------------------------------------*/
/* checkmodem                                                                */
/*   Checks whether an entry for given name exists in the linked list        */
/*   and if not, allocates it and puts the name in the allocated modem       */
/*   struct.                                                                 */
/*---------------------------------------------------------------------------*/

static modem_t  *checkmodem(char *name, int flag)
{
    modem_t  *tmpmodem = NULL;
    modem_t  *tmpmodem1 = NULL;
    int      found = 0;

    if ( !name ) return(NULL);
    tmpmodem = modem;

    if ( tmpmodem != NULL ) {
	while ( tmpmodem->next != NULL ) {
	    if ( strcmp(tmpmodem->name, name) == 0 ) {
		found = 1;
		break;
	    }
	    tmpmodem = tmpmodem->next;
	}

	if ( !found && !strcmp(tmpmodem->name, name)) {
		found = 1;
	}
    }

    if ( !found )  {
	if ( !flag ) return(NULL);
	if ( (tmpmodem1 = (modem_t *) calloc(1, sizeof(modem_t))) == NULL) {
	    return(NULL);
	}

	tmpmodem1->name = strdup(name);
	if ( modem == NULL ) {
	    tmpmodem = modem = tmpmodem1;
	} else {
	    tmpmodem->next = tmpmodem1;
	    tmpmodem = tmpmodem->next;
	}
    } 

    return(tmpmodem);
}

/*---------------------------------------------------------------------------*/
/* addmodem                                                                  */
/*   Takes 2 arguments, str which points to the name string and str1 which   */
/*   points to the option string and updates the name with given options.    */
/*---------------------------------------------------------------------------*/

static void addmodem(char *str, char *str1)
{

    char *tmpBuffer = str1;
    char *tmpBuffer1 = NULL;
    char *bufval = NULL;
    modem_t *tmpmodem = NULL;


    strtok(str, "=");
    bufval = strtok(NULL, "=");

    tmpmodem = checkmodem(bufval, ALLOCSTRUCT);

    if ( tmpmodem == NULL ) return;

    /* Now, lets look at other values */

    if ( str1 ) {
	while( (tmpBuffer1 = strtok_r(tmpBuffer, " \t\n", &bufval)) != NULL ) {

	    if ( !strncmp(tmpBuffer1, "text=", 5) ) {
		if ( tmpmodem->modtext != NULL) free(tmpmodem->modtext);
		tmpmodem->modtext = strdup(tmpBuffer1+5);
	    } else if ( !strncmp(tmpBuffer1, "device=", 7) ) {
		if ( tmpmodem->device != NULL) free(tmpmodem->device);
		tmpmodem->device = strdup(tmpBuffer1+7); 
	    } else if ( !strncmp(tmpBuffer1, "initcmd=", 8)) {
		if ( tmpmodem->initcmd != NULL) free(tmpmodem->initcmd);
		tmpmodem->initcmd = strdup(tmpBuffer1+8);
	    } else if ( !strncmp(tmpBuffer1, "dialcmd=", 8)) {
		if ( tmpmodem->dialcmd != NULL) free(tmpmodem->dialcmd);
		tmpmodem->dialcmd = strdup(tmpBuffer1+8);
	    }

	    tmpBuffer = NULL;
	}
    }

    return;
}

/*---------------------------------------------------------------------------*/
/* process_qpageopts                                                         */
/*   Sets the main qpage options                                             */
/*---------------------------------------------------------------------------*/

static void process_qpageopts(char *str, int i)
{
    char *tmpBuffer = str;
    char *bufval    = NULL;

    if ( str ) {
	strtok(str, "=");
	bufval = strtok(NULL, "=");
	if ( bufval ) {
	    switch(i) {
		case 1:
                    if ( currconfig.admin != NULL) free(currconfig.admin);
		    currconfig.admin = strdup(bufval);
		    break;
		case 2:
                    if ( currconfig.forcehostname != NULL) free(currconfig.forcehostname);
		    currconfig.forcehostname =strdup(bufval);
		    break;
		case 3:
		    currconfig.identtimeout = atoi(bufval);
		    break;
		case 6:
		case 4:
		    break;
		case 5:
		    if (currconfig.lockdir != NULL) free(currconfig.lockdir);
		    currconfig.lockdir = strdup(bufval);
		    break;
		case 7:
		    if (currconfig.queuedir != NULL) free(currconfig.queuedir);
		    currconfig.queuedir = strdup(bufval);
		    break;
		case 8:
		    currconfig.snpptimeout = atoi(bufval);
		    break;
		case 9:
		    currconfig.synchronous = strdup(bufval);
		    break;
	    }
	}
    }

    return;
}

/*---------------------------------------------------------------------------*/
/* process_buf                                                               */
/*   Processes the buffer for each of the configuration item read            */
/*---------------------------------------------------------------------------*/

static int process_buf(char *buffer)
{

    char *orgBuffer = buffer;
    char *tmpBuffer = NULL;
    char *tmpBuffer1= NULL;

    if ( !buffer ) return(0);

    tmpBuffer = strtok_r(orgBuffer, " \t\n", &tmpBuffer1);

    if ( tmpBuffer != NULL ) {
	switch(*tmpBuffer) {
	    case 'a':
		if ( !strncmp(tmpBuffer, "administrator=", 14)) {
		    process_qpageopts(tmpBuffer,1);
		} 
		break;
	    case 'f':
		if ( !strncmp(tmpBuffer, "forcehostname=", 14)) {
		    process_qpageopts(tmpBuffer,2);
		}
		break;
	    case 'g':
		if ( !strncmp(tmpBuffer, "group=", 6)) {
		}
		break;
	    case 'i':
		if ( !strncmp(tmpBuffer, "identtimeout=", 13)) {
		    process_qpageopts(tmpBuffer,3);
		} else if ( !strncmp(tmpBuffer, "include=", 8)) {
		    process_qpageopts(tmpBuffer,4);
		}
		break;
	    case 'l':
		if ( !strncmp(tmpBuffer, "lockdir=", 8)) {
		    process_qpageopts(tmpBuffer,5);
		}
		break;
	    case 'm':
		if ( !strncmp(tmpBuffer, "modem=", 6)) {
		    addmodem(tmpBuffer, tmpBuffer1);
		}
		break;
	    case 'p':
		if ( !strncmp(tmpBuffer, "pidfile=", 8)) {
		    process_qpageopts(tmpBuffer,6);
                } else if ( !strncmp(tmpBuffer, "pager=", 6)) {
		    addpager(tmpBuffer, tmpBuffer1);
		}
		break;
	    case 'q':
		if ( !strncmp(tmpBuffer, "queuedir=", 9)) {
		    process_qpageopts(tmpBuffer,7);
		}
		break;
	    case 's':
		if ( !strncmp(tmpBuffer, "snpptimeout=", 12)) {
		    process_qpageopts(tmpBuffer,8);
		} else if ( !strncmp(tmpBuffer, "synchronous=", 12)) {
		    process_qpageopts(tmpBuffer,9);
		} else if ( !strncmp(tmpBuffer, "service=", 8)) {
		    addservice(tmpBuffer, tmpBuffer1);
		}
		break;
	    default:
		break;
	}
    } else {
	return(0);
    }

    return(1);

}

/*---------------------------------------------------------------------------*/
/* read_conf_file                                                            */
/*   Reads the configuration file for each configuration element and its     */
/*   sub elements.                                                           */
/*---------------------------------------------------------------------------*/

static int read_conf_file(char *filename)
{
    char buf[2048];
    int  tmpBytes = 0, tmpBytes1 = 0;
    char readbuf[1024];
    FILE *fp = NULL;
    
    if (filename == NULL) return(0);

    if ( (fp = fopen(filename, "r")) == NULL) {
	return(0);
    }

    tmpBytes = 0;
    while(fgets(readbuf, sizeof(readbuf), fp) != NULL) {
	switch(readbuf[0]) {
	    case '#':
            case '\n':
		break;
	    case ' ':
	    case '\t':
		tmpBytes += snprintf(&buf[tmpBytes], sizeof(buf)-tmpBytes, "%s", readbuf);
		break;
	    default:
		buf[tmpBytes] = '\0';
		if ( tmpBytes ) {
		    if ( !process_buf(buf) ) {
			fclose(fp);
		        return(0);
                    }
		}
		tmpBytes = 0;
		tmpBytes += snprintf(&buf[tmpBytes], sizeof(buf)-tmpBytes, "%s", readbuf);
		break;
	}
    }

    if ( tmpBytes ) {
      if ( !process_buf(buf) ) {
	fclose(fp);
	return(0);
      }
    }

    fclose(fp);
    readfile = 1;
    return(1);
}

/*---------------------------------------------------------------------------*/
/* display_error                                                             */
/*   Displays error in case of a delete PAGER/MODEM/SERVICE                  */
/*---------------------------------------------------------------------------*/

static void display_error(mySession *sess, char *ptr, int func, int mod, int err)
{
   char  buffer[1024];
   int   nobytes = 0;

   if ( sess->textonly  == 0 )
   {
      Body("<HTML>\n");
      Body("<HEAD>\n");
      Body("<TITLE>\n");
      Body("System Support Software - ver.1.0</TITLE>\n");
      Body("</HEAD>\n");
      Body("<BODY BGCOLOR=\"#ffffcc\">\n");
        TableBegin("BORDER=\"0\" CELLPADDING=\"0\" CELLSPACING=\"0\" WIDTH=\"100%\"");
    	RowBegin("");
    	  CellBegin("WIDTH=\"15\" BGCOLOR=\"#cccc99\"");
    	  Body("&nbsp;&nbsp;&nbsp;\n");
	  CellEnd();
    	  CellBegin("BGCOLOR=\"#cccc99\"");
    	    Body("<FONT FACE=\"Arial,Helvetica\">\n");
    	      Body("SETUP &gt; Paging &gt;\n");

	      nobytes += snprintf(&buffer[nobytes], sizeof(buffer), "Error deleting ");
	      switch ( mod ) {
		  case PAGER:
		      Body("Pager\n");
                      nobytes += snprintf(&buffer[nobytes], sizeof(buffer), "Pager.  Pager ");
		      break;
		  case SERVICE:
		      Body("Service\n");
                      nobytes += snprintf(&buffer[nobytes], sizeof(buffer), "Service.  Service ");
		      break;
		  case MODEM:
                      nobytes += snprintf(&buffer[nobytes], sizeof(buffer), "Modem.  Modem ");
                  case ADMIN:
		      Body("Modem/Admin\n");
		      break;
	      }

	      Body("</FONT>\n");
    	  CellEnd();
    	RowEnd();
    	RowBegin("");
    	  CellBegin("COLSPAN=\"2\"");
    	  Body("&nbsp;\n");
          CellEnd();
    	RowEnd();
    	RowBegin("");
    	  CellBegin("ALIGN=\"RIGHT\" COLSPAN=\"2\"");
    	    Body("<INPUT TYPE=\"BUTTON\"\n");
    	  Body("onClick=\"showMap()\" VALUE=\"   Help   \">\n");
	  CellEnd();
    	RowEnd();
    	RowBegin("");
    	  CellBegin("COLSPAN=\"2\"");
    	  Body("&nbsp;</TD>\n");
    	RowEnd();

	if ( !ptr ) {
	    Body("Error displaying error page.\n");
	    goto end;
        } else if ( err == 0 ) {
	    nobytes += snprintf(&buffer[nobytes], sizeof(buffer), "%s not found. <br>", ptr);
	} else if ( err < 0 ) {
	    nobytes += snprintf(&buffer[nobytes], sizeof(buffer), "name must be specified <br>");
	}
  

    	RowBegin("");
    	  CellBegin("");
    	  CellEnd();
    	  CellBegin("");
    	   TableBegin("BORDER=\"0\" CELLPADDING=\"0\" CELLSPACING=\"0\"");
	      RowBegin("");
		  CellBegin("COLSPAN=\"3\"");
		  FormatedBody("%s",buffer);
		  CellEnd();
	      RowEnd();
    	   TableEnd();
    	  CellEnd();
    	RowEnd();

    end:
    	RowBegin("");
    	  CellBegin("");
    	  CellEnd();
    	  CellBegin("");
    	  CellEnd();
    	RowEnd();
          TableEnd();
          Body("<P>\n");
          Body("</P>\n");
      Body("</BODY>\n");
    Body("</HTML>\n");
   } 
   else 
   {
      /*  Lynx */ 
      Body("<HTML>\n");
      Body("<HEAD>\n");
      Body("<TITLE>\n");
      Body("Embedded Support Partner - ver.1.0</TITLE>\n");
      Body("</HEAD>\n");
      Body("<BODY BGCOLOR=\"#ffffcc\">\n");
     
      switch ( mod ) 
      {
	case PAGER:  
	 FormatedBody("<pre>   SETUP &gt; Paging &gt; %s</pre>\n","Pager");
	break;
		      
	case SERVICE:
	  FormatedBody("<pre>   SETUP &gt; Paging &gt; %s</pre>\n","Service");
        break;
		      
        case MODEM:
        case ADMIN:
	  FormatedBody("<pre>   SETUP &gt; Paging &gt; %s</pre>\n","Modem/Admin");
        break;
      }
      Body("<hr width=100%%>\n");

      if ( !ptr ) 
      {
              Body("Error displaying error page.\n");
         
      }	else {    
      
              switch ( mod ) 
              {
	        case PAGER:
                   FormatedBody("Error deleting Pager. Pager ");
	        break;

	        case SERVICE:
                   FormatedBody("Error deleting Service. Service ");
	        break;

	        case MODEM:
                   FormatedBody("Error deleting Modem.  Modem ");
                break;
                
                case ADMIN:
                   Body("Modem/Admin\n");
	        break;
              }

              if ( err == 0 ) 
              {
                  FormatedBody("%s not found. <br>",ptr);
              } 
              else if ( err < 0 ) 
              {
	          FormatedBody("name must be specified <br>");
              }

      }
      Body("</BODY>\n");
      Body("</HTML>\n");
   }
}

/*---------------------------------------------------------------------------*/
/* display_confirmation                                                      */
/*   Displays confirmation page                                              */
/*---------------------------------------------------------------------------*/

static void display_confirmation(mySession *sess, void *ptr, int func, int mod) 
{
    modem_t       *tmpmodem   = 0;
    service_t     *tmpservice = 0;
    pager_t       *tmppager   = 0;
    qpageconfig_t *tmpadmin   = 0;
    char          *name       = 0;

    if ( sess->textonly == 0 )
    {
    Body("<HTML>\n");
      Body("<HEAD>\n");
        Body("<TITLE>\n");
        Body("Embedded Support Partner - ver.1.0</TITLE>\n");
      Body("</HEAD>\n");
      Body("<BODY BGCOLOR=\"#ffffcc\">\n");
        TableBegin("BORDER=\"0\" CELLPADDING=\"0\" CELLSPACING=\"0\" WIDTH=\"100%\"");
    	RowBegin("");
    	  CellBegin("WIDTH=\"15\" BGCOLOR=\"#cccc99\"");
    	  Body("&nbsp;&nbsp;&nbsp;\n");
	  CellEnd();
    	  CellBegin("BGCOLOR=\"#cccc99\"");
    	    Body("<FONT FACE=\"Arial,Helvetica\">\n");
    	      Body("SETUP &gt; Paging &gt;\n");

	      switch ( mod ) {
		  case PAGER:
		      Body("Pager\n");
		      break;
		  case SERVICE:
		      Body("Service\n");
		      break;
		  case MODEM:
                  case ADMIN:
		      Body("Modem/Admin\n");
		      break;
	      }

	      Body("</FONT>\n");
    	  CellEnd();
    	RowEnd();
    	RowBegin("");
    	  CellBegin("COLSPAN=\"2\"");
    	  Body("&nbsp;\n");
          CellEnd();
    	RowEnd();
    	RowBegin("");
    	  CellBegin("ALIGN=\"RIGHT\" COLSPAN=\"2\"");
    	    Body("<INPUT TYPE=\"BUTTON\"\n");
    	  Body("onClick=\"showMap()\" VALUE=\"   Help   \">\n");
	  CellEnd();
    	RowEnd();
    	RowBegin("");
    	  CellBegin("COLSPAN=\"2\"");
    	  Body("&nbsp;</TD>\n");
    	RowEnd();

	if ( !ptr ) {
	    Body("Error displaying confirmation page.\n");
	    goto end;
        }

    	RowBegin("");
    	  CellBegin("");
    	  CellEnd();
    	  CellBegin("");
    	    TableBegin("BORDER=\"0\" CELLPADDING=\"0\" CELLSPACING=\"0\"");

	switch(func) 
	{
	    case DELETE:
		name = (char *) ptr;
		break;
            case ADD:
		switch(mod) 
		{
		    case PAGER:
			tmppager = (pager_t *) ptr;
			break;
		    case SERVICE:
			tmpservice = (service_t *) ptr;
			break;
		    case MODEM:
			tmpmodem = (modem_t *) ptr;
			break;
		    case ADMIN:
			tmpadmin = (qpageconfig_t *) ptr;
			break;
		}
		break;
	}

	switch(mod)
	{
	    case ADMIN:
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<B>\n");
    		    Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		    Body("QuickPage Administration Variables\n");
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
    	      RowBegin("");
    		CellBegin("");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("Administrator's E-mail address\n");
		  Body("</FONT>\n");
    		CellEnd();
    		CellBegin("");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		    Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
    		  Body("</FONT>\n");
    		CellEnd();
    		CellBegin("");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
		  if ( tmpadmin->admin ) FormatedBody("%s",tmpadmin->admin);
		  else FormatedBody("No value set");
		  Body("</FONT>\n");
    		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("&nbsp; </FONT>\n");
    		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("Number of seconds to wait for a reply before giving up on queries\n");
		  Body("</FONT>\n");
    		CellEnd();
    		CellBegin("");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		    Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
    		  Body("</FONT>\n");
    		CellEnd();
    		CellBegin("");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
		  if ( tmpadmin->identtimeout ) FormatedBody("%d secs",tmpadmin->identtimeout);
		  else FormatedBody("No value set");
		  Body("</FONT>\n");
    		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("&nbsp; </FONT>\n");
    		CellEnd();
    	      RowEnd();
	      break;
	    case MODEM:
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<B>\n");
    		    Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		    if ( func == DELETE ) Body("The following Modem is deleted :\n");
                    else Body("The following Modem is Added/Updated :\n");
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
      	        RowBegin("");
      		  CellBegin("");
      		    Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		    Body("&nbsp;&nbsp;&nbsp;&nbsp;Name</FONT>\n");
      		  CellEnd();
      		  CellBegin("");
      		    Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		    Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
      		    Body("</FONT>\n");
      		  CellEnd();
      		  CellBegin("");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
		  if ( func == DELETE ) FormatedBody("%s",name);
		  else  FormatedBody("%s",tmpmodem->name);
      		  CellEnd();
      	        RowEnd();
		if ( func != DELETE ) {
      	        RowBegin("");
      		  CellBegin("COLSPAN=\"3\"");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		  Body("&nbsp; &nbsp;</FONT>\n");
      		  CellEnd();
      	        RowEnd();
      	        RowBegin("");
      		  CellBegin("");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		  Body("&nbsp;&nbsp;&nbsp;&nbsp;Device</FONT>\n");
      		  CellEnd();
      		  CellBegin("");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		    Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
      		  Body("</FONT>\n");
      		  CellEnd();
      		  CellBegin("");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
		  FormatedBody("%s",tmpmodem->device);
      		  CellEnd();
      	        RowEnd();
      	        RowBegin("");
      		  CellBegin("COLSPAN=\"3\"");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		  Body("&nbsp; </FONT>\n");
      		  CellEnd();
      	        RowEnd();
      	        RowBegin("");
      		  CellBegin("");
      		  Body("&nbsp;&nbsp;&nbsp;&nbsp;Initialization command</TD>\n");
      		  CellBegin("");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		    Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
      		  Body("</FONT>\n");
      		  CellEnd();
      		  CellBegin("");
		  FormatedBody("%s",tmpmodem->initcmd);
		  CellEnd();
      	        RowEnd();
      	        RowBegin("");
      		  CellBegin("COLSPAN=\"3\"");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		  Body("&nbsp; </FONT>\n");
      		  CellEnd();
      	        RowEnd();
		}

	      break;
	    case SERVICE:

    	      RowBegin("");
    	      CellBegin("COLSPAN=\"3\"");
    		Body("<B>\n");
    		Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		if (func == DELETE ) Body("The following Service is deleted :\n");
                else Body("The following Service is Added/Updated :\n");
		Body("</FONT>\n");
    		Body("</B>\n");
    	      CellEnd();
    	      RowEnd();
    	      RowBegin("");
              CellBegin("COLSPAN=\"3\"");
    	        Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		Body("&nbsp; \n");
		Body("</FONT>\n");
    	      CellEnd();
    	      RowEnd();

    	      RowBegin("VALIGN=\"TOP\"");
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;Name\n");
		CellEnd();
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
		CellEnd();
    		CellBegin("");
		if ( func == DELETE ) FormatedBody("%s",name);
		else if ( tmpservice->name ) FormatedBody("%s",tmpservice->name);
		else FormatedBody("No value set");
		CellEnd();
    	      RowEnd();

	      if ( func != DELETE ) {
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("&nbsp; </FONT>\n");
    		CellEnd();
    	      RowEnd();
    	      RowBegin("VALIGN=\"TOP\"");
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;Modem Name\n");
		CellEnd();
    		CellBegin("");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		    Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
    		  Body("</FONT>\n");
    		CellEnd();
    		CellBegin("");
    		if ( tmpservice->modname) FormatedBody("%s",tmpservice->modname);
		else FormatedBody("No value set");
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("&nbsp; </FONT>\n");
    		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("");
    		  Body("&nbsp;&nbsp;&nbsp;&nbsp;Maximum number of retries\n");
		CellEnd();
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
		CellEnd();
    		CellBegin("");
    		FormatedBody("%d",tmpservice->maxtries);
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		Body("&nbsp;&nbsp;\n");
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("");
    		  Body("&nbsp;&nbsp;&nbsp;&nbsp;Maximum length of message\n");
		CellEnd();
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp\n");
		CellEnd();
    		CellBegin("");
    		FormatedBody("%d",tmpservice->maxmsgsize);
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		Body("&nbsp;&nbsp;</TD>\n");
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;Phone Number of Paging Service\n");
		CellEnd();
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
		CellEnd();
    		CellBegin("");
    		if ( tmpservice->phone ) FormatedBody("%s",tmpservice->phone);
		else FormatedBody("No value set");
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		Body("&nbsp;&nbsp;\n");
		CellEnd();
    	      RowEnd();
            }

	      break;
            case PAGER:

  	      RowBegin("");
    	      CellBegin("COLSPAN=\"3\"");
    		  Body("<B>\n");
    		  if ( func == DELETE ) Body("The following Pager is deleted :\n");
                  else Body("The following Pager is Added/Updated :\n");
                  Body("</B>\n");
    	      CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("&nbsp; </FONT>\n");
    		CellEnd();
    	      RowEnd();

    	      RowBegin("VALIGN=\"TOP\"");
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;Name\n");
		CellEnd();
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
		CellEnd();
    		CellBegin("");
		if ( func == DELETE ) FormatedBody("%s",name);
		else if ( tmppager->name ) FormatedBody("%s",tmppager->name);
		else FormatedBody("No value set");
		CellEnd();
    	      RowEnd();

	      if ( func != DELETE ) {
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		Body("&nbsp;&nbsp;\n");
		CellEnd();
    	      RowEnd();
    	      RowBegin("VALIGN=\"TOP\"");
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;Service\n");
		CellEnd();
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
		CellEnd();
    		CellBegin("");
    		if ( tmppager->sername ) FormatedBody("%s",tmppager->sername);
		else FormatedBody("No value set");
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		Body("&nbsp;&nbsp;\n");
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;Pager ID\n");
		CellEnd();
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;:\n");
		CellEnd();
    		CellBegin("");
    		if ( tmppager->pagerid ) FormatedBody("%s",tmppager->pagerid);
		else FormatedBody("No value set");
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("&nbsp; </FONT>\n");
    		CellEnd();
    	      RowEnd();
           }
	      break;
	}

    	   TableEnd();
    	  CellEnd();
    	RowEnd();

    end:
    	RowBegin("");
    	  CellBegin("");
    	  CellEnd();
    	  CellBegin("");
    	  CellEnd();
    	RowEnd();
          TableEnd();
          Body("<P>\n");
          Body("</P>\n");
      Body("</BODY>\n");
    Body("</HTML>\n");
   } else {
     /* Lynx part */

      Body("<HTML>\n");
      Body("<HEAD>\n");
      Body("<TITLE>\n");
      Body("Embedded Support Partner - ver.1.0</TITLE>\n");
      Body("</HEAD>\n");
      Body("<BODY BGCOLOR=\"#ffffcc\">\n");

      switch ( mod ) 
      {
	 case PAGER:
 	    Body("<pre>   SETUP &gt; Paging &gt; Pager</pre>\n");
         break;
         
         case SERVICE:
 	    Body("<pre>   SETUP &gt; Paging &gt; Service</pre>\n");
         break;
         
         case MODEM:
         case ADMIN:
 	    Body("<pre>   SETUP &gt; Paging &gt; Modem/Admin</pre>\n");
         break;
      }
      Body("<hr width=100%>\n");

        if ( !ptr ) 
        {
	    Body("Error displaying confirmation page.\n");
	    goto end;
        }

	switch(func) 
	{
	    case DELETE:
		name = (char *) ptr;
		break;
            case ADD:
		switch(mod) 
		{
		    case PAGER:
			tmppager = (pager_t *) ptr;
			break;
		    case SERVICE:
			tmpservice = (service_t *) ptr;
			break;
		    case MODEM:
			tmpmodem = (modem_t *) ptr;
			break;
		    case ADMIN:
			tmpadmin = (qpageconfig_t *) ptr;
			break;
		}
		break;
	}

	switch(mod)
	{
	    case ADMIN:
               Body           ( "<b>QuickPage Administration Variables</b><br><br>" );
               FormatedBody   ( "<pre>   Administrator's E-mail address        : %s</pre>\n", tmpadmin->admin ? tmpadmin->admin : "No value set" );
		  
               if ( tmpadmin->identtimeout )
                  FormatedBody( "<p><pre>   Number of seconds to wait for a reply : %d secs</pre>\n", tmpadmin->identtimeout );
               else    
                  FormatedBody( "<p><pre>   Number of seconds to wait for a reply : %s</pre>\n", "No value set" );
               Body ( "<pre>   before giving up on queries</pre>\n" );           
	    break;

	      
	    case MODEM:
    		if ( func == DELETE ) 
    		    Body("<p>The following Modem is deleted<br>\n");
                else 
                    Body("<p>The following Modem is Added/Updated<br>\n");

		if ( func == DELETE ) 
 	            FormatedBody("<pre>      Name  :%s</pre>\n",name);
		else  
		{
		    FormatedBody("<pre>      Name                   :%s</pre>\n",tmpmodem->name);
		    FormatedBody("<pre>      Device                 :%s</pre>\n",tmpmodem->device);
		    FormatedBody("<pre>      Initialization command :%s</pre>\n",tmpmodem->initcmd);
		}
	    break;
	    
	    case SERVICE:
    		if (func == DELETE ) 
    		   Body("<p>The following Service is deleted :<br>\n");
                else 
                   Body("<p>The following Service is Added/Updated :<br>\n");

		if ( func == DELETE ) 
		{
		  FormatedBody("<pre>      Name  : %s</pre>",name);
		} 
		else 
		{
		  if ( tmpservice->name ) 
		      FormatedBody("<pre>      Name                     : %s</pre>",tmpservice->name);
		  else 
		      FormatedBody("<pre>      Name                     : No value set</pre>");

    	          if ( tmpservice->modname) 
    		      FormatedBody("<pre>      Modem Name               : %s</pre>",tmpservice->modname);
		  else 
		      FormatedBody("<pre>      Modem Name               : No value set</pre>");

      	  	      FormatedBody("<pre>      Maximum number of retries: %d</pre>",tmpservice->maxtries);
    		      FormatedBody("<pre>      Maximum length of message: %d</pre>",tmpservice->maxmsgsize);
    		
    		  if ( tmpservice->phone ) 
   		      FormatedBody("<pre>      Phone Number of Paging Service: %s</pre>",tmpservice->phone);
		  else 
		      FormatedBody("<pre>      Phone Number of Paging Service: No value set</pre>");
		}
	    break;

            case PAGER:
 	      if ( func == DELETE )
 	         Body("The following Pager is deleted :\n");
              else 
                 Body("The following Pager is Added/Updated :\n");
                  
              if ( func == DELETE ) 
              {
                    FormatedBody("<pre>      Name    : %s</pre>",name);
	      } 
	      else 
	      {
    	        if ( tmppager->name ) 
    	            FormatedBody("<pre>      Name    : %s</pre>",tmppager->name);
	        else 
	            FormatedBody("<pre>      Name    : No value set</pre>");
	            
      	        if ( tmppager->sername ) 
    	            FormatedBody("<pre>      Service : %s",tmppager->sername);
	        else 
	            FormatedBody("<pre>      Service : No value set");
		
    	        if ( tmppager->pagerid ) 
    	            FormatedBody("<pre>      Pager ID: %s",tmppager->pagerid);
   	        else 
   	            FormatedBody("<pre>      Pager ID: No value set");
              }
              
	     break;
	}
      Body("</BODY>\n");
    Body("</HTML>\n");
   } /* End of Lynx */

}

/*---------------------------------------------------------------------------*/
/* display_configuration                                                     */
/*   Displays configuration information                                      */
/*---------------------------------------------------------------------------*/

static void display_configuration(mySession *sess)
{
  modem_t   *tmpmodem = modem;
  service_t *tmpservice = service;
  pager_t   *tmppager   = pager;
  int       i = 0;

  if ( sess->textonly == 0 )
  {
        TableBegin("BORDER=\"0\" CELLPADDING=\"0\" CELLSPACING=\"0\" WIDTH=\"100%\"");
    	RowBegin("");
    	  CellBegin("WIDTH=\"15\" BGCOLOR=\"#cccc99\"");
    	  Body("&nbsp;&nbsp;&nbsp;\n");
	  CellEnd();
    	  CellBegin("BGCOLOR=\"#cccc99\"");
    	    Body("<FONT FACE=\"Arial,Helvetica\">\n");
    	      Body("SETUP &gt; Paging &gt; View Current Setup</FONT>\n");
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
    		    Body("QuickPage Administration Variables\n");
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
    	      RowBegin("");
    		CellBegin("");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("Administrator's E-mail address\n");
		  Body("</FONT>\n");
    		CellEnd();
    		CellBegin("");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		    Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
    		  Body("</FONT>\n");
    		CellEnd();
    		CellBegin("");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
		  if ( currconfig.admin ) FormatedBody("%s",currconfig.admin);
		  else FormatedBody("No value set");
		  Body("</FONT>\n");
    		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("&nbsp; </FONT>\n");
    		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("Number of seconds to wait for a reply before giving up on queries\n");
		  Body("</FONT>\n");
    		CellEnd();
    		CellBegin("");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		    Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
    		  Body("</FONT>\n");
    		CellEnd();
    		CellBegin("");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
		  if ( currconfig.identtimeout ) FormatedBody("%d secs",currconfig.identtimeout);
		  else FormatedBody("No value set");
		  Body("</FONT>\n");
    		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("&nbsp; </FONT>\n");
    		CellEnd();
    	      RowEnd();

	      /* Print Modems */

    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<B>\n");
    		    Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		    Body("Modem Setup \n");
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


	    i = 1;
            if ( tmpmodem == NULL ) {
		RowBegin("");
		  CellBegin("COLSPAN=\"3\"");
		  Body("No modems configured.\n");
		  CellEnd();
                RowEnd();
    	        RowBegin("");
    		  CellBegin("COLSPAN=\"3\"");
    		  Body("&nbsp;&nbsp;\n");
		  CellEnd();
    	        RowEnd();
	    } else {
	      while ( tmpmodem != NULL ) {
		RowBegin("");
		  CellBegin("COLSPAN=\"3\"");
		  Body("<i>\n");
		  FormatedBody("Modem %d",i);
		  Body("</i>\n");
		  CellEnd();
                RowEnd();
    	        RowBegin("");
    		  CellBegin("COLSPAN=\"3\"");
    		  Body("&nbsp;&nbsp;</TD>\n");
    	        RowEnd();
      	        RowBegin("");
      		  CellBegin("");
      		    Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		    Body("&nbsp;&nbsp;&nbsp;&nbsp;Name</FONT>\n");
      		  CellEnd();
      		  CellBegin("");
      		    Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		    Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
      		    Body("</FONT>\n");
      		  CellEnd();
      		  CellBegin("");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
		  FormatedBody("%s",tmpmodem->name);
      		  CellEnd();
      	        RowEnd();
      	        RowBegin("");
      		  CellBegin("COLSPAN=\"3\"");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		  Body("&nbsp; &nbsp;</FONT>\n");
      		  CellEnd();
      	        RowEnd();
      	        RowBegin("");
      		  CellBegin("");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		  Body("&nbsp;&nbsp;&nbsp;&nbsp;Device</FONT>\n");
      		  CellEnd();
      		  CellBegin("");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		    Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
      		  Body("</FONT>\n");
      		  CellEnd();
      		  CellBegin("");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
		  FormatedBody("%s",tmpmodem->device);
      		  CellEnd();
      	        RowEnd();
      	        RowBegin("");
      		  CellBegin("COLSPAN=\"3\"");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		  Body("&nbsp; </FONT>\n");
      		  CellEnd();
      	        RowEnd();
      	        RowBegin("");
      		  CellBegin("");
      		  Body("&nbsp;&nbsp;&nbsp;&nbsp;Initialization command</TD>\n");
      		  CellBegin("");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		    Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
      		  Body("</FONT>\n");
      		  CellEnd();
      		  CellBegin("");
		  FormatedBody("%s",tmpmodem->initcmd);
		  CellEnd();
      	        RowEnd();
      	        RowBegin("");
      		  CellBegin("COLSPAN=\"3\"");
      		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
      		  Body("&nbsp; </FONT>\n");
      		  CellEnd();
      	        RowEnd();
		i++;
		tmpmodem = tmpmodem->next;
            }
	  }

            /* Print Services */

    	    RowBegin("");
    	      CellBegin("COLSPAN=\"3\"");
    		Body("<B>\n");
    		Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		Body("Services  Setup \n");
		Body("</FONT>\n");
    		Body("</B>\n");
    	      CellEnd();
    	    RowEnd();
    	    RowBegin("");
              CellBegin("COLSPAN=\"3\"");
    	        Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		Body("&nbsp; \n");
		Body("</FONT>\n");
    	      CellEnd();
    	    RowEnd();

          i = 1;
          if ( tmpservice == NULL ) {
		RowBegin("");
		  CellBegin("COLSPAN=\"3\"");
		  Body("No Services configured.\n");
		  CellEnd();
                RowEnd();
    	        RowBegin("");
    		  CellBegin("COLSPAN=\"3\"");
    		  Body("&nbsp;&nbsp;\n");
		  CellEnd();
    	        RowEnd();
	  } else {
	    while (tmpservice != NULL ) {
    	      RowBegin("VALIGN=\"TOP\"");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<I>\n");
    		  FormatedBody("Service %d",i);
		  Body("</I>\n");
    		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("&nbsp; \n");
		  Body("</FONT>\n");
    		CellEnd();
    	      RowEnd();
    	      RowBegin("VALIGN=\"TOP\"");
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;Name\n");
		CellEnd();
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
		CellEnd();
    		CellBegin("");
    		if ( tmpservice->name ) FormatedBody("%s",tmpservice->name);
		else FormatedBody("No value set");
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("&nbsp; </FONT>\n");
    		CellEnd();
    	      RowEnd();
    	      RowBegin("VALIGN=\"TOP\"");
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;Modem Name\n");
		CellEnd();
    		CellBegin("");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		    Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;&nbsp;\n");
    		  Body("</FONT>\n");
    		CellEnd();
    		CellBegin("");
    		if ( tmpservice->modname) FormatedBody("%s",tmpservice->modname);
		else FormatedBody("No value set");
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("&nbsp; </FONT>\n");
    		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("");
    		  Body("&nbsp;&nbsp;&nbsp;&nbsp;Maximum number of retries\n");
		CellEnd();
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
		CellEnd();
    		CellBegin("");
    		FormatedBody("%d",tmpservice->maxtries);
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		Body("&nbsp;&nbsp;\n");
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("");
    		  Body("&nbsp;&nbsp;&nbsp;&nbsp;Maximum length of message\n");
		CellEnd();
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp\n");
		CellEnd();
    		CellBegin("");
    		FormatedBody("%d",tmpservice->maxmsgsize);
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		Body("&nbsp;&nbsp;</TD>\n");
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;Phone Number of Paging Service\n");
		CellEnd();
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
		CellEnd();
    		CellBegin("");
    		if ( tmpservice->phone ) FormatedBody("%s",tmpservice->phone);
		else FormatedBody("No value set");
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		Body("&nbsp;&nbsp;\n");
		CellEnd();
    	      RowEnd();
	      i++;
	      tmpservice = tmpservice->next;
            }
          }

  	    RowBegin("");
    	      CellBegin("COLSPAN=\"3\"");
    		  Body("<B>\n");
    		  Body("Pager Setup</B>\n");
    	      CellEnd();
    	    RowEnd();
    	    RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("&nbsp; </FONT>\n");
    		CellEnd();
    	    RowEnd();

          /* Pagers */

	  i = 1;
          if ( tmppager == NULL ) {
		RowBegin("");
		  CellBegin("COLSPAN=\"3\"");
		  Body("No Pagers configured.\n");
		  CellEnd();
                RowEnd();
    	        RowBegin("");
    		  CellBegin("COLSPAN=\"3\"");
    		  Body("&nbsp;&nbsp;\n");
		  CellEnd();
    	        RowEnd();
	  } else {
	    while ( tmppager != NULL ) {
    	      RowBegin("VALIGN=\"TOP\"");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<I>\n");
    		  FormatedBody("Pager %d",i);
		  Body("</I>\n");
    		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		Body("&nbsp;&nbsp;\n");
		CellEnd();
    	      RowEnd();
    	      RowBegin("VALIGN=\"TOP\"");
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;Name\n");
		CellEnd();
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
		CellEnd();
    		CellBegin("");
    		if ( tmppager->name ) FormatedBody("%s",tmppager->name);
		else FormatedBody("No value set");
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		Body("&nbsp;&nbsp;\n");
		CellEnd();
    	      RowEnd();
    	      RowBegin("VALIGN=\"TOP\"");
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;Service\n");
		CellEnd();
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;:&nbsp;\n");
		CellEnd();
    		CellBegin("");
    		if ( tmppager->sername ) FormatedBody("%s",tmppager->sername);
		else FormatedBody("No value set");
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		Body("&nbsp;&nbsp;\n");
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;Pager ID\n");
		CellEnd();
    		CellBegin("");
    		Body("&nbsp;&nbsp;&nbsp;&nbsp;:\n");
		CellEnd();
    		CellBegin("");
    		if ( tmppager->pagerid ) FormatedBody("%s",tmppager->pagerid);
		else FormatedBody("No value set");
		CellEnd();
    	      RowEnd();
    	      RowBegin("");
    		CellBegin("COLSPAN=\"3\"");
    		  Body("<FONT FACE=\"Arial,Helvetica\">\n");
    		  Body("&nbsp; </FONT>\n");
    		CellEnd();
    	      RowEnd();
	      i++;
	      tmppager = tmppager->next;
           }
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
          Body("<P>\n");
          Body("</P>\n");
    } /* end of Mozilla part */   
    else
    { /* lynx part */
  	  
          Body           ( "<b>QuickPage Administration Variables</b><br><br>" );
          FormatedBody   ( "<pre>   Administrator's E-mail address        : %s</pre>\n", currconfig.admin ? currconfig.admin : "No value set" );
		  
          if ( currconfig.identtimeout )
             FormatedBody( "<p><pre>   Number of seconds to wait for a reply : %d secs</pre>\n", currconfig.identtimeout );
          else    
             FormatedBody( "<p><pre>   Number of seconds to wait for a reply : %s</pre>\n", "No value set" );
          Body ( "<pre>   before giving up on queries</pre>\n" );           
	       
          /* Print Modems */
  	  Body("<b><p>Modem Setup</b><br>\n");
          i = 1;
          if ( tmpmodem == NULL ) 
          {
              Body("<br> No modems configured.\n");
	  } else {
	      while ( tmpmodem != NULL ) 
	      {
		FormatedBody("<p>&nbsp;Modem %d<br>",i);
		FormatedBody("<pre>      Name                   :%s</pre>\n",tmpmodem->name);
		FormatedBody("<pre>      Device                 :%s</pre>\n",tmpmodem->device);
		FormatedBody("<pre>      Initialization command :%s</pre>\n",tmpmodem->initcmd);
		i++; tmpmodem = tmpmodem->next;
              }
	  }

            /* Print Services */
            
          Body("<br><p><b>Services  Setup</b>\n");

          i = 1;
          if ( tmpservice == NULL ) 
          {
  	    Body("<br>No Services configured.\n");
	  } else {
	    while (tmpservice != NULL ) 
	    {
    		FormatedBody("<br>&nbsp;Service %d",i);
		  
    		if ( tmpservice->name ) 
    		  FormatedBody("<pre>      Name                          : %s</pre>",tmpservice->name);
		else 
		  FormatedBody("<pre>      Name                          : No value set</pre>");
   	      
    		if ( tmpservice->modname) 
    		  FormatedBody("<pre>      Modem Name                    : %s</pre>",tmpservice->modname);
		else 
		  FormatedBody("<pre>      Modem Name                    : No value set</pre>");

      	  	  FormatedBody("<pre>      Maximum number of retries     : %d</pre>",tmpservice->maxtries);
    		  FormatedBody("<pre>      Maximum length of message     : %d</pre>",tmpservice->maxmsgsize);
    		
    		if ( tmpservice->phone ) 
   		  FormatedBody("<pre>      Phone Number of Paging Service: %s</pre>",tmpservice->phone);
		else 
		  FormatedBody("<pre>      Phone Number of Paging Service: No value set</pre>");
	      i++;
	      tmpservice = tmpservice->next;
            }
          }

          /* Pagers */
          Body("<p><b>Pager Setup</b>\n");
	  i = 1;
          if ( tmppager == NULL ) 
          {
  	    Body("<br>No Pagers configured.\n");
	  } 
	  else 
	  {
	    while ( tmppager != NULL ) 
	    {
    	      FormatedBody("<b>Pager %d</b>",i);
		  
    	      if ( tmppager->name ) 
    	          FormatedBody("<pre>      Name    : %s</pre>",tmppager->name);
	      else 
	          FormatedBody("<pre>      Name    : No value set</pre>");
		
    	      if ( tmppager->sername ) 
    	          FormatedBody("<pre>      Service : %s",tmppager->sername);
	      else 
	          FormatedBody("<pre>      Service : No value set");
		
    	      if ( tmppager->pagerid ) 
    	          FormatedBody("<pre>      Pager ID: %s",tmppager->pagerid);
   	      else 
   	          FormatedBody("<pre>      Pager ID: No value set");
		
	      i++;
	      tmppager = tmppager->next;
           }
         }
    }   
}

/*---------------------------------------------------------------------------*/
/* display_service_page                                                      */
/*   Displays the page that sets Qpage Administration and Modem Commands     */
/*---------------------------------------------------------------------------*/

static void display_service_page(sscErrorHandle hError)
{
  
  modem_t   *tmpmodem = NULL;

  tmpmodem = modem;

  /* Read Modems to display */

  while ( tmpmodem != NULL ) {
    FormatedBody("<option value=\"%s\">%s\n",tmpmodem->name,tmpmodem->name);
    tmpmodem = tmpmodem->next;
  }

  return;

}

/*---------------------------------------------------------------------------*/
/* display_pager_page                                                        */
/*   Displays the web page that allows one to set the Pager Configuration    */
/*---------------------------------------------------------------------------*/

static void display_pager_page(sscErrorHandle hError)
{
  
  service_t  *tmpservice = NULL;

  tmpservice = service;

  /* Read Pagers to display */

  while(tmpservice != NULL ) 
  {
    FormatedBody("<option value=\"%s\">%s",tmpservice->name,tmpservice->name);
    tmpservice = tmpservice->next;
  }

  return;
}

/*---------------------------------------------------------------------------*/
/*- Display Parameter validation error                                      -*/
/*---------------------------------------------------------------------------*/

void  display_param_missing_errors ( mySession *sess, char *szPage, char *szParam )
{

      Body("<HTML>\n");
      Body("<HEAD>\n");
      Body("<TITLE>\n");
      Body("Embedded Support Partner - ver.1.0</TITLE>\n");
      Body("</HEAD>\n");
      Body("<BODY BGCOLOR=\"#ffffcc\">\n");
      FormatedBody("<pre>   SETUP &gt; Paging &gt; %s</pre>",szPage);
      Body("<hr width=100%>\n");
      FormatedBody("<p>Parameter %s is missing or empty. Please, enter %s and try again.",szParam,szParam);
      Body("</BODY>\n");
      Body("</HTML>\n");
}

void  display_param_str_errors ( mySession *sess, char *szPage, char *szParam )
{
      Body("<HTML>\n");
      Body("<HEAD>\n");
      Body("<TITLE>\n");
      Body("Embedded Support Partner - ver.1.0</TITLE>\n");
      Body("</HEAD>\n");
      Body("<BODY BGCOLOR=\"#ffffcc\">\n");
      FormatedBody("<pre>   SETUP &gt; Paging &gt; %s</pre>",szPage);
      Body("<hr width=100%>\n");
      FormatedBody("<p>%s should not contain '\"' nor blank spaces",szParam);
      Body("</BODY>\n");
      Body("</HTML>\n");
}

void  display_select_errors ( mySession *sess, char *szPage, char *szParam )
{
      Body("<HTML>\n");
      Body("<HEAD>\n");
      Body("<TITLE>\n");
      Body("Embedded Support Partner - ver.1.0</TITLE>\n");
      Body("</HEAD>\n");
      Body("<BODY BGCOLOR=\"#ffffcc\">\n");
      FormatedBody("<pre>   SETUP &gt; Paging &gt; %s</pre>",szPage);
      Body("<hr width=100%>\n");
      FormatedBody("%s parameter is missing. Please, select %s and try again.",szParam,szParam);
      Body("</BODY>\n");
      Body("</HTML>\n");
}

void  display_param_int_errors ( mySession *sess, char *szPage, char *szParam, char *szLimit )
{
      Body("<HTML>\n");
      Body("<HEAD>\n");
      Body("<TITLE>\n");
      Body("Embedded Support Partner - ver.1.0</TITLE>\n");
      Body("</HEAD>\n");
      Body("<BODY BGCOLOR=\"#ffffcc\">\n");
      FormatedBody("<pre>   SETUP &gt; Paging &gt; %s</pre>",szPage);
      Body("<hr width=100%>\n");
      FormatedBody("<p>Parameter %s must be %s",szParam,szLimit);
      Body("</BODY>\n");
      Body("</HTML>\n");
}

int IsStrBlank ( const char * str )
{
  if ( str == NULL || str[0] == 0 || (strspn ( str," ") == strlen (str))) 
       return 1;
  else 
       return 0;     
}

int IsAnyBadChars ( const char * str )
{
   if ( strpbrk( str, " " ) != NULL )
        return 1;
        
   if ( strchr ( str, '"' ) != NULL )
        return 1;
        
   return 0;     
}

int IsValidInt ( const char * str, int min, int max )
{
  int  val;
  
  if ( strspn ( str, "0123456789" ) != strlen(str))
       return 0;
       
  if ( sscanf ( str, " %d", &val ) != 1 )
       return 0;
       
  if ( val < min || val > max ) 
       return 0;         
  
  return 1;
}


/*---------------------------------------------------------------------------*/
/* setmodemopts                                                              */
/*   Sets Modem Options                                                      */
/*---------------------------------------------------------------------------*/

static int setmodemopts(mySession *sess, CMDPAIR *cmdp)
{
    char tmp1[80], tmp[1024];
    int  i = 0, j = 0, nobytes = 0;
    char tmp2[80];
    modem_t *tmpmodem = 0;

    memset(tmp1, 0, sizeof(tmp1));
    memset(tmp, 0, sizeof(tmp));

    for ( i = 0; (i = sscFindPairByKey(cmdp, i, "qp_modem_name")) >= 0; i++) 
    {
       j = 1;
       if ( sess->textonly )
       {
         if ( IsStrBlank(cmdp[i].value))
         {
           display_param_missing_errors ( sess, "Modem/Admin", "\"Modem name\"" );       
           return 0;
         } 
         if ( IsAnyBadChars(cmdp[i].value))
         {
           display_param_str_errors ( sess, "Modem/Admin", "\"Modem name\"" );       
           return 0;
         }
       }
       snprintf(tmp1, sizeof(tmp1), "modem=%s", cmdp[i].value);
       strncpy (tmp2, cmdp[i].value, sizeof(tmp2));
    }

    if ( j == 0 ) 
    {
       return (1);
    }

    for ( i = 0; (i = sscFindPairByKey(cmdp, i, "qp_modem_device")) >= 0; i++) 
    {
       nobytes += snprintf(&tmp[nobytes], sizeof(tmp)-nobytes, "device=%s  ", cmdp[i].value);
    }

    for ( i = 0; (i = sscFindPairByKey(cmdp, i, "qp_modem_init_cmd")) >= 0; i++) 
    {
       nobytes += snprintf(&tmp[nobytes], sizeof(tmp)-nobytes, "initcmd=%s  ", cmdp[i].value);
    }

    if ( nobytes ) 
    {
	addmodem ( tmp1, tmp );
	tmpmodem = checkmodem ( tmp2, NOALLOCSTRUCT );
	if ( tmpmodem ) 
	     display_confirmation ( sess, (void *) tmpmodem, ADD, MODEM );
        writetofile = 1;
    }
    
    return(0);
}

/*---------------------------------------------------------------------------*/
/* setpageropts                                                              */
/*   Set Pager Options                                                       */
/*---------------------------------------------------------------------------*/

static int setpageropts(mySession *sess, CMDPAIR *cmdp)
{
    char  tmp1[80], tmp[1024];
    int i = 0, j = 0, nobytes = 0;
    char tmp2 [80];
    pager_t *tmppager = 0;

    memset(tmp1, 0, sizeof(tmp1));
    memset(tmp, 0, sizeof(tmp));

    for ( i = 0; (i = sscFindPairByKey(cmdp, i, "pager_name")) >= 0; i++) 
    {
       j = 1;
       if ( sess->textonly )
       {
         if ( IsStrBlank(cmdp[i].value))
         {
           display_param_missing_errors ( sess, "Pager", "\"Pager name\"" );       
           return 0;
         } 
         if ( IsAnyBadChars(cmdp[i].value))
         {
           display_param_str_errors ( sess, "Pager", "\"Pager name\"" );       
           return 0;
         }
       }  
       
       snprintf(tmp1, sizeof(tmp1), "pager=%s", cmdp[i].value);
       strncpy(tmp2, cmdp[i].value, sizeof(tmp2));
    }

    if ( j == 0 ) {
       return (1);
    }

    for ( i = 0; (i = sscFindPairByKey(cmdp, i, "pager_id")) >= 0; i++) 
    {
       if ( sess->textonly )
       {
         if ( IsStrBlank(cmdp[i].value))
         {
           display_param_missing_errors ( sess, "Pager", "Pager ID" );       
           return 0;
         } 
         if ( IsAnyBadChars(cmdp[i].value))
         {
           display_param_str_errors ( sess, "Pager", "Pager ID" );       
           return 0;
         }
       }  
       nobytes += snprintf(&tmp[nobytes], sizeof(tmp)-nobytes, "pagerid=%s  ", cmdp[i].value);
    }

    for ( i = 0; (i = sscFindPairByKey(cmdp, i, "pager_service_name")) >= 0; i++) 
    {
       if ( sess->textonly )
       {
         if ( strcmp ( cmdp[i].value, "0" ) == 0 )
         {
           display_select_errors ( sess, "Pager", "Pager service" );
           return 0;
         }  
       }  
       nobytes += snprintf(&tmp[nobytes], sizeof(tmp)-nobytes, "service=%s  ", cmdp[i].value);
    }
    tmp[nobytes] = '\0';

    if ( nobytes ) 
    {
	addpager(tmp1, tmp);
	tmppager = checkpager(tmp2, NOALLOCSTRUCT);
	if ( tmppager) 
	     display_confirmation(sess, (void *) tmppager, ADD, PAGER);
        writetofile = 1;
    }

    return(0);
}

/*---------------------------------------------------------------------------*/
/* setserviceopts                                                            */
/*   Set Service Options                                                     */
/*---------------------------------------------------------------------------*/

static int setserviceopts(mySession *sess, CMDPAIR *cmdp)
{
    char tmp1[80], tmp[1024];
    int  i = 0, j = 0, nobytes = 0;
    char tmp2[80];
    service_t  *tmpservice = 0;

    memset(tmp1, 0, sizeof(tmp1));
    memset(tmp, 0, sizeof(tmp));

    for ( i = 0; (i = sscFindPairByKey(cmdp, i, "service_name")) >= 0; i++) 
    {
       j = 1;
       if ( sess->textonly )
       {
        if ( IsStrBlank(cmdp[i].value))
        {
         display_param_missing_errors ( sess, "Service", "\"Service name\"" );       
         return 0;
        } 
        if ( IsAnyBadChars(cmdp[i].value))
        {
         display_param_str_errors ( sess, "Service", "\"Service name\"" );       
         return 0;
        }
       }
       snprintf(tmp1, sizeof(tmp1), "service=%s", cmdp[i].value);
       strncpy(tmp2, cmdp[i].value, sizeof(tmp2));
    }

    if ( j == 0 ) 
    {
        return(1);
    } 
    
    for ( i = 0; (i = sscFindPairByKey(cmdp, i, "modem_device")) >= 0; i++) 
    {
       /* check the modem value */
       if ( sess->textonly )
       {
         if ( strcmp(cmdp[i].value, "0" ) == 0)
          {
            display_select_errors ( sess, "Services", "Device" );
            return 0;
          }
       }   
       nobytes += snprintf(&tmp[nobytes], sizeof(tmp)-nobytes, "device=%s  ", cmdp[i].value);
    }
    
    for ( i = 0; (i = sscFindPairByKey(cmdp, i, "modem_maxtries")) >= 0; i++) 
    {
       if ( sess->textonly )
       {
         int  val;
         if ( IsStrBlank(cmdp[i].value))
         {
           display_param_missing_errors ( sess, "Services", "\"Maximum number of retries\"" );       
           return 0;
         }
         if ( !IsValidInt ( cmdp[i].value, 6, 0x7FFFFFFF ))
         {
           display_param_int_errors ( sess, "Service", "\"Maximum number of retries\"", "positive integer value greater then 5" );
           return 0;
         }  
       }  
       nobytes += snprintf(&tmp[nobytes], sizeof(tmp)-nobytes, "maxtries=%s  ", cmdp[i].value);
    }
    for ( i = 0; (i = sscFindPairByKey(cmdp, i, "modem_maxmsgsize")) >= 0; i++) 
    {
       if ( sess->textonly )
       {
         int  val;
         if ( IsStrBlank(cmdp[i].value))
         {
           display_param_missing_errors ( sess, "Services", "\"Maximum length of message\"" );       
           return 0;
         }
         if ( !IsValidInt ( cmdp[i].value, 0, 0x7FFFFFFF ))
         {
           display_param_int_errors ( sess, "Service", "\"Maximum length of message\"", "non negative integer value." );
           return 0;
         }  
       }  
       nobytes += snprintf(&tmp[nobytes], sizeof(tmp)-nobytes, "maxmsgsize=%s  ", cmdp[i].value);
    }
    
    for ( i = 0; (i = sscFindPairByKey(cmdp, i, "modem_no")) >= 0; i++) 
    {
       if ( sess->textonly )
       {
         int  val;
         if ( IsStrBlank(cmdp[i].value))
         {
           display_param_missing_errors ( sess, "Services", "\"Phone number\"" );       
           return 0;
         }
         
         if ( strlen ( cmdp[i].value ) < 7  || !IsValidInt ( cmdp[i].value, 0x80000000, 0x7FFFFFFF ))
         {
           display_param_int_errors ( sess, "Service", "\"Phone Number\"", "at least 7-digit number without spaces and/or '-'" );
           return 0;
         }  
       }  
       nobytes += snprintf(&tmp[nobytes], sizeof(tmp)-nobytes, "phone=%s  ", cmdp[i].value);
    }
    tmp[nobytes] = '\0';
    if ( nobytes ) 
    {
	addservice(tmp1, tmp);
        tmpservice = checkservice(tmp2, NOALLOCSTRUCT);
        if ( tmpservice ) 
             display_confirmation(sess, (void *)tmpservice,ADD, SERVICE);
	writetofile = 1;
    }

    return(0);
}

/*---------------------------------------------------------------------------*/
/* setadminopts                                                              */
/*   Set QuickPage Administration Options                                    */
/*---------------------------------------------------------------------------*/

static int setadminopts(mySession *sess, CMDPAIR *cmdp)
{
    char  tmp1[80], tmp[1024];
    int   i = 0, j = 0, nobytes = 0;

    memset(tmp1, 0, sizeof(tmp1));
    memset(tmp, 0, sizeof(tmp));

    for (i=0; (i=sscFindPairByKey(cmdp,i,"qp_admin")) >= 0; i++) {
	if (currconfig.admin) free(currconfig.admin);
	currconfig.admin = strdup(cmdp[i].value);
	writetofile = 1;
    }

    for (i=0; (i=sscFindPairByKey(cmdp,i,"qp_ident_timeout")) >= 0; i++) {
	j = atoi(cmdp[i].value);
	currconfig.identtimeout = j;
	writetofile = 1;
    }

    display_confirmation(sess, &currconfig, ADD, ADMIN);

    return(0);
}

/*---------------------------------------------------------------------------*/
/* deleteconfig                                                              */
/*   Deletes configuration element                                           */
/*---------------------------------------------------------------------------*/

static int deleteconfig(mySession *sess, CMDPAIR *cmdp, int cmdpsize)
{
    int err = 1;
    int i = 0;
    int mod = 0;

    for (i = 0; i < cmdpsize; i++) 
    {
	if ( !strcasecmp(cmdp[i].keyname, "pager_name") ) 
	{
           if ( sess->textonly )
           {
             if ( IsStrBlank (cmdp[i].value))
             {
               display_param_missing_errors ( sess, "Pager", "\"Pager name\"" );       
               return 0;
             }     
             if ( IsAnyBadChars(cmdp[i].value))
             {
              display_param_str_errors ( sess, "Pager", "\"Pager name\"" );       
              return 0;
             }
           }
           
           if(cmdp[i].value) err = deletepager(cmdp[i].value);
	    mod = PAGER;
	    break;
	} 
	else if ( !strcasecmp(cmdp[i].keyname, "service_name") ) 
	{
           if ( sess->textonly )
           {
             if ( IsStrBlank (cmdp[i].value))
             {
               display_param_missing_errors ( sess, "Service", "\"Service name\"" );       
               return 0;
             }     
             if ( IsAnyBadChars(cmdp[i].value))
             {
               display_param_str_errors ( sess, "Service", "\"Service name\"" );       
               return 0;
             }
           }
	   if(cmdp[i].value) err = deleteservice(cmdp[i].value);
	   mod = SERVICE;
	   break;
	} 
	else if ( !strcasecmp(cmdp[i].keyname, "qp_modem_name") ) 
	{
           if ( sess->textonly )
           {
             if ( IsStrBlank (cmdp[i].value))
             {
               display_param_missing_errors ( sess, "Modem/Admin", "\"Modem name\"" );       
               return 0;
             }
             if ( IsAnyBadChars(cmdp[i].value))
             {
               display_param_str_errors ( sess, "Modem/Admin", "\"Modem name\"" );       
               return 0;
             }
           }
	   if(cmdp[i].value) err = deletemodem(cmdp[i].value);
	   mod = MODEM;
	   break;
	}
    }

    if ( err == 1 ) 
    { 
	writetofile = 1;
	display_confirmation(sess, cmdp[i].value, DELETE, mod);
        return(0);
    } else {
	display_error(sess, cmdp[i].value, DELETE, mod, err);
    }

    return(err);
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
   int   submittype = 0;
   int   proc = 0;

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

   if ( readfile == 0 ) {
       
       if ( !read_conf_file(CONFIGFILE) ) {
          sscError(hError,szServerNameErrorPrefix,
	           "Can't read configuration file %s<br>\n", CONFIGFILE);
          return(RGPERR_SUCCESS);
       } 
   }

   /*  Initialize The HTML Generator  */
   if(createMyHTMLGenerator(result))
   { sscError(hError,szServerNameErrorPrefix,"Can't open the HTML generator<br>\n");
     return(RGPERR_SUCCESS);
   }

   /* Initialize Pair array (cmdpsize eq. number of valid pairs) */

   cmdpsize = sscInitPair(rawCommandString,cmdp,MAXKEYS);

   /* Set Browser type */

   for(i = 0;(i = sscFindPairByKey(cmdp,i,szUserAgent)) >= 0;i++)
       if ( !strcasecmp(cmdp[i].value,"Lynx")) sess->textonly = 1;

   /* Find out what the user wants to do (add or delete) */
   if ( sess->textonly == 0 )
   {
     for(i = 0;(i = sscFindPairByKey(cmdp,i,"submit_type")) >=0; i++) {
         if ( !strcasecmp(cmdp[i].value, "add")) submittype = 1;
         else if ( !strcasecmp(cmdp[i].value, "del")) submittype = 2;
     }
   } else {
     if ( sscFindPairByKey ( cmdp, 0, "submit_add" ) >= 0)
          submittype = 1;
     else
     if ( sscFindPairByKey ( cmdp, 0, "submit_del" ) >= 0)
          submittype = 2;
   }

   /* Parse Arguments */
   
   if ( !strcasecmp(argv[1], "getpager") ) {
       display_pager_page(hError);
       goto end;
   } else if ( !strcasecmp(argv[1], "getservice") ) {
       display_service_page(hError);
       goto end;
   } else if ( !strcasecmp(argv[1], "setpager")) {
       proc = 1;
   } else if ( !strcasecmp(argv[1], "setservice")) {
       proc = 2;
   } else if ( !strcasecmp(argv[1], "setadmin")) {
       submittype = 1;
       proc = 3;
   } else if ( !strcasecmp(argv[1], "setmodem")) {
       proc = 4;
   } else if ( !strcasecmp(argv[1], "showsetup")) {
       display_configuration ( sess );
       goto end;
   } else {
       sscError(hError,szServerNameErrorPrefix,"Invalid Arguments<br>\n");
       goto end;
   }

   if ( proc == 0 ) {
       sscError(hError,szServerNameErrorPrefix,"Invalid Arguments<br>\n");
       goto end;
   }

   if ( submittype == 1 ) {
       switch(proc) {
	   case 1 :
	       err = setpageropts(sess, cmdp);
	       break;
	   case 2 :
	       err = setserviceopts(sess, cmdp);
	       break;
	   case 3 :
	       err = setadminopts(sess, cmdp);
	       break;
	   case 4 :
	       err = setmodemopts(sess, cmdp);
	       break;
	   default:
	       sscError(hError,szServerNameErrorPrefix,
			      "Unknown internal corruption<br>\n");
	       err++;
	       break;
       }
   } else if ( submittype == 2 ) {
       err = deleteconfig(sess, cmdp, cmdpsize);
   } else {
       sscError(hError, szServerNameErrorPrefix, "Invalid Submit Type<br>\n");
       goto end;
   }

   if ( err ) {
       sscError(hError,szServerNameErrorPrefix,
		 "Request %s didnot complete succesfully<br>\n", argv[1]);
       goto end;
   }

   if ( writetofile == 1 ) {
       if ( (fp = fopen(CONFIGFILE, "w")) != NULL) {
	   dump_conf_file(fp);
	   fclose(fp);
	   system("/etc/killall -HUP qpage.d");
       } else {
	   sscError(hError, szServerNameErrorPrefix, 
                         "Unable to open %s for writing data<br>\n", CONFIGFILE);
       }
       writetofile = 0;
   }

   end:
   deleteMyHTMLGenerator();
   return(RGPERR_SUCCESS);
}
