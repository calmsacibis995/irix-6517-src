/* --------------------------------------------------------------------------- */
/* -                              RGAPI.C                                    - */
/* --------------------------------------------------------------------------- */
/*                                                                             */
/* Copyright 1992-1998 Silicon Graphics, Inc.                                  */
/* All Rights Reserved.                                                        */
/*                                                                             */
/* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;      */
/* the contents of this file may not be disclosed to third parties, copied or  */
/* duplicated in any form, in whole or in part, without the prior written      */
/* permission of Silicon Graphics, Inc.                                        */
/*                                                                             */
/* RESTRICTED RIGHTS LEGEND:                                                   */
/* Use, duplication or disclosure by the Government is subject to restrictions */
/* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data      */
/* and Computer Software clause at DFARS 252.227-7013, and/or in similar or    */
/* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -     */
/* rights reserved under the Copyright Laws of the United States.              */
/*                                                                             */
/* --------------------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <malloc.h>

#include "rgAPI.h"
#include "rgAPIinternal.h"
#include "sssStatus.h"
#include "sscPair.h"

extern SSSGLBINFO sssGlbInfo;     /* Switcher Global Info structure */

/* --------------------------------------------------------------------------- */
#ifdef __TIME__
#ifdef __DATE__
#define INCLUDE_TIME_DATE_STRINGS 1
#endif
#endif
/* --------------------------------------------------------------------------- */
/*   Structure 'sss_s_PLUGIN_FACE' is describing the data block responsible    */
/*   for RG server specific data. Only one block is allocated for each         */
/*   dynamicaly loaded RG server.                                              */
typedef struct sss_s_PLUGIN_FACE {
   unsigned long signature;                       /* sizeof(struct sss_s_PLUGIN_FACE) */
   struct sss_s_PLUGIN_FACE *link;                /* pointer to next */
   char       pname[128];                         /* plugin name */
   char       ptitle[128];                        /* plugin title */
   char       pversion[64];                       /* plugin version */
   void       *DSO;                               /* DSO handle */

   pthread_mutex_t lockmutex;                     /* Mutex for lock access to plugin */

   int         usageCount;                        /* Usage counter */
   int         timeout;                           /* unload timeout */
 
   int         unloadable;                        /* Unloadable Flag */
   int         threadSafe;                        /* Thread safe Flag */
   int         unloadtime;                        /* Unload time value (5 sec units) */

   /* Function pointers for dynamic linking with RG plugins */
   FPrgpgInit                *init;
   FPrgpgDone                *done;
   FPrgpgCreateSesion        *createSesion;
   FPrgpgDeleteSesion        *deleteSesion;
   FPrgpgGetAttribute        *getAttribute;
   FPrgpgFreeAttributeString *freeAttributeString;
   FPrgpgSetAttribute        *setAttribute;
   FPrgpgGenerateReport      *generateReport;
} PluginFace_t;
/* --------------------------------------------------------------------------- */
/*   Structure "REPORT_SESSION" keeps session specific data for RG core.
 */
typedef struct sss_s_REPORT_SESSION {
   struct sss_s_REPORT_SESSION *link;
   PluginFace_t                *plugin;
   void                        *sessionID;
} ReportSession_t;
/* --------------------------------------------------------------------------- */
static pthread_t unloadManagerID;
static pthread_mutex_t faceLock;
static pthread_mutex_t rgSessionLock;
static pthread_cond_t  faceCond;
static int exit_phase                              = 0; /* RG Core exit phase */
static int srgInitedFlg                            = 0; /* RG API inited flag */
static const char szUnloadable[]                   = "Unloadable";
static const char szThreadSafe[]                   = "ThreadSafe";
static const char szUnloadTime[]                   = "UnloadTime";
static const char szVersion[]                      = "Version";
static const char szTitle[]                        = "Title";
static const char szIncorrectAttrNameNull[]        = "Incorrect attribute name - NULL";

static const char szInvSesIdIn[]                   = "Invalid session ID in %s";
static const char szCantFindFpointer[]             = "Can't find '%s' in RG server '%s'";
static const char szPgFn_rgpgInit[]                = "rgpgInit";
static const char szPgFn_rgpgDone[]                = "rgpgDone";
static const char szPgFn_rgpgCreateSesion[]        = "rgpgCreateSesion";
static const char szPgFn_rgpgDeleteSesion[]        = "rgpgDeleteSesion";
static const char szPgFn_rgpgGetAttribute[]        = "rgpgGetAttribute";
static const char szPgFn_rgpgFreeAttributeString[] = "rgpgFreeAttributeString";
static const char szPgFn_rgpgSetAttribute[]        = "rgpgSetAttribute";
static const char szPgFn_rgpgGenerateReport[]      = "rgpgGenerateReport";
/* --------------------------------------------------------------------------- */
static char szRGCoreVersion[16];                   /* static buffer for RG core version string */
static char szRGCoreTitle[128];                    /* static buffer for RG title and version string */
static PluginFace_t *busyFaceHandles               = 0; /* list of all in use PLUGIN_FACE blocks */
static PluginFace_t *freeFaceHandles               = 0; /* list of all unused PLUGIN_FACE blocks */
static ReportSession_t *busySessionHandles         = 0; /* list of all in use REPORT_SESSION blocks */
static ReportSession_t *freeSessionHandles         = 0; /* list of all unused REPORT_SESSION blocks */
/* ------------------------------- newPluginFace ----------------------------- */
static PluginFace_t *newPluginFace(const char *serverName)
{  PluginFace_t *face;
   if((face = freeFaceHandles) != 0)
   { freeFaceHandles = face->link; 
     sssGlbInfo.statTotalPluginFaceInFreeList--;
   }
   if(!face)
   { if((face = (PluginFace_t*)malloc(sizeof(PluginFace_t))) != 0)
      sssGlbInfo.statTotalPluginFaceAlloced++;
   }
   if(face)
   { memset(face,0,sizeof(PluginFace_t));
     if(!pthread_mutex_init(&face->lockmutex,NULL))
     { face->signature = sizeof(PluginFace_t);
       face->usageCount = 1;
       face->unloadtime = PF_TIME_TO_KEEP_PLUGIN;
       if(serverName) strncpy(face->pname,serverName,sizeof(face->pname)-1);
       face->pname[sizeof(face->pname)-1] = 0;
     }
     else
     { face->link = freeFaceHandles; freeFaceHandles = face;
       face = 0; sssGlbInfo.statTotalPluginFaceInFreeList++;
     }
   }
   return face;
}
/* ----------------------------- deletePluginFace ---------------------------- */
static PluginFace_t *deletePluginFace(PluginFace_t *face)
{ face->signature = 0;
  if(face->DSO) dlclose(face->DSO);
  pthread_mutex_destroy(&face->lockmutex);
  face->link = freeFaceHandles; freeFaceHandles = face;
  sssGlbInfo.statTotalPluginFaceInFreeList++;
  return 0;
}
/* ------------------------------- linkPluginFace ---------------------------- */
static void linkPluginFace(PluginFace_t *face)
{ if(face && face->signature == sizeof(PluginFace_t))
  { face->link = busyFaceHandles; busyFaceHandles = face;
  }
}
/* ------------------------------- findPluginByName -------------------------- */
static PluginFace_t *findPluginByName(const char *name)
{ PluginFace_t *face = 0;
  if(name)
  { for(face = busyFaceHandles;face;face = face->link)
    { if(!strcmp(face->pname,name)) 
      { face->usageCount++;
        break;
      }
    }
  }
  return face;
}
/* -------------------------------- loadNewPlugIn --------------------------- */
static PluginFace_t *loadNewPlugIn(sscErrorHandle hError,const char *serverName)
{  int i;
   PluginFace_t *p;
   char *s = 0;

   sssGlbInfo.statTotalLoadNewPlugIn++;

   if((p = newPluginFace(serverName)) == 0)
   { sscError(hError, "No memory for plugin face in srgCreateReportSession()");
     return 0;
   }
   
   if((p->DSO = dlopen(p->pname,RTLD_NOW)) == NULL)
   { sscError(hError, "%s - Can't load RG server '%s'",dlerror(),p->pname);
     return deletePluginFace(p);
   }
        if((p->init = (FPrgpgInit*)dlsym(p->DSO,szPgFn_rgpgInit)) == NULL) s = (char*)szPgFn_rgpgInit;
   else if((p->done = (FPrgpgDone*)dlsym(p->DSO,szPgFn_rgpgDone)) == NULL) s = (char*)szPgFn_rgpgDone;
   else if((p->createSesion = (FPrgpgCreateSesion*)dlsym(p->DSO,szPgFn_rgpgCreateSesion)) == NULL) s = (char*)szPgFn_rgpgCreateSesion;
   else if((p->deleteSesion = (FPrgpgDeleteSesion*)dlsym(p->DSO,szPgFn_rgpgDeleteSesion)) == NULL) s = (char*)szPgFn_rgpgDeleteSesion;
   else if((p->getAttribute = (FPrgpgGetAttribute*)dlsym(p->DSO,szPgFn_rgpgGetAttribute)) == NULL) s = (char*)szPgFn_rgpgGetAttribute;
   else if((p->freeAttributeString = (FPrgpgFreeAttributeString*)dlsym(p->DSO,szPgFn_rgpgFreeAttributeString)) == NULL) s = (char*)szPgFn_rgpgFreeAttributeString;
   else if((p->setAttribute = (FPrgpgSetAttribute*)dlsym(p->DSO,szPgFn_rgpgSetAttribute)) == NULL) s = (char*)szPgFn_rgpgSetAttribute;
   else if((p->generateReport = (FPrgpgGenerateReport*)dlsym(p->DSO,szPgFn_rgpgGenerateReport)) == NULL) s = (char*)szPgFn_rgpgGenerateReport;

   if(s)
   { sscError(hError,(char*)szCantFindFpointer,s,p->pname); 
     return deletePluginFace(p);
   }

   if(!p->init(hError))
   { sscError(hError,"Can not init RG server '%s'",p->pname);
     return deletePluginFace(p);
   }

   if((s = p->getAttribute(0,0,szUnloadable,0,&i)) != 0)
   { p->unloadable = atoi(s);
     p->freeAttributeString(0,0,szUnloadable,0,s,i);
   }

   if((s = p->getAttribute(0,0,szThreadSafe,0,&i)) != 0)
   { p->threadSafe = atoi(s);
     p->freeAttributeString(0,0,szThreadSafe,0,s,i);
   }

   if((s = p->getAttribute(0,0,szUnloadTime,0,&i)) != 0)
   { if((p->unloadtime = (atoi(s)/RGPLUGIN_UNLOAD_TIMESTEP)) < 1) p->unloadtime = 1;
     p->freeAttributeString(0,0,szUnloadTime,0,s,i);
   }

   if((s = p->getAttribute(0,0,szTitle,0,&i)) != 0)
   { strncpy(p->ptitle,s,sizeof(p->ptitle)); p->ptitle[sizeof(p->ptitle)-1] = 0;
     p->freeAttributeString(0,0,szTitle,0,s,i);
   }

   if((s = p->getAttribute(0,0,szVersion,0,&i)) != 0)
   { strncpy(p->pversion,s,sizeof(p->pversion)); p->pversion[sizeof(p->pversion)-1] = 0;
     p->freeAttributeString(0,0,szVersion,0,s,i);
   }

   return p;
}
/* -------------------------------- unloadManager ---------------------------- */
void *unloadManager(void* arg)
{ int someWaiting;
  PluginFace_t *p, **pp;
  for(;!exit_phase;)
  { someWaiting = 0;
    pthread_mutex_lock(&faceLock);
     
    for(pp = &busyFaceHandles;(p = *pp) != 0 && !exit_phase;)
    { if(p->usageCount <= 0 && p->unloadable)
      { p->timeout--; someWaiting++;

        if(p->timeout <= 0) /* unload  */
        { *pp = p->link;
          p->done(0);
          deletePluginFace(p);
	  sssGlbInfo.statTotalUnloadPlugin++;
        }
        else pp = &((*pp)->link);
      }
      else pp = &((*pp)->link);
    }
    if(!exit_phase)
    { if(!someWaiting) pthread_cond_wait(&faceCond,&faceLock);
      pthread_mutex_unlock(&faceLock);
      sleep(RGPLUGIN_UNLOAD_TIMESTEP);
    }
  }
  return 0;
}
/* ----------------------------- markToUnloadPlugin -------------------------- */
static void markToUnloadPlugin(PluginFace_t* plugin)
{ if(plugin && plugin->signature == sizeof(PluginFace_t))
  { pthread_mutex_lock(&faceLock);
    plugin->usageCount--;
    if(plugin->usageCount <= 0) 
    { plugin->timeout = plugin->unloadtime;
      pthread_cond_signal(&faceCond);
    }
    pthread_mutex_unlock(&faceLock);
  }
}
/* --------------------------------- lockPlugin ------------------------------ */
static void lockPlugin(PluginFace_t* plugin)
{ if(plugin && plugin->signature == sizeof(PluginFace_t) &&
     plugin->threadSafe == 0) pthread_mutex_lock(&plugin->lockmutex);
}
/* -------------------------------- unlockPlugin ----------------------------- */
static void unlockPlugin(PluginFace_t* plugin)
{ if(plugin && plugin->signature == sizeof(PluginFace_t) &&
     plugin->threadSafe == 0) pthread_mutex_unlock(&plugin->lockmutex);
}
/* ----------------------------- findSessionBlock ---------------------------- */
static ReportSession_t ** findSessionBlock(ReportSession_t *session)
{  ReportSession_t **p = &busySessionHandles;
   for(;*p;p = &((*p)->link))
    if(*p == session) return p;
   return 0;
}
/* ----------------------------- newReportSession ---------------------------- */
static ReportSession_t *newReportSession(sscErrorHandle hError,PluginFace_t *p)
{  ReportSession_t *session;
   pthread_mutex_lock(&rgSessionLock);
   if((session = freeSessionHandles) != 0)
   { freeSessionHandles = session->link; 
     sssGlbInfo.statTotalReportSessionInFreeList--;
   }
   pthread_mutex_unlock(&rgSessionLock);
   if(!session)
   { if((session = (ReportSession_t*)malloc(sizeof(ReportSession_t))) != 0)
      sssGlbInfo.statTotalReportSessionAlloced++;
   }
   if(session)
   { memset(session,0,sizeof(ReportSession_t));
     session->plugin = p;
     pthread_mutex_lock(&rgSessionLock);
     session->link = busySessionHandles;  busySessionHandles = session;
     pthread_mutex_unlock(&rgSessionLock);
   }
   else sscError(hError,"Can't alloc 'ReportSession_t' structure"); 
   return session;
}
/* ----------------------------- destroyReportSession ------------------------ */
static void destroyReportSession(ReportSession_t *session)
{  ReportSession_t **p;
   pthread_mutex_lock(&rgSessionLock);
   if((p = findSessionBlock(session)) != 0)
   { *p = session->link; session->link = freeSessionHandles;
     freeSessionHandles = session;
     sssGlbInfo.statTotalReportSessionInFreeList++;
   }
   pthread_mutex_unlock(&rgSessionLock);
}
/* -------------------------------- checkSessionID --------------------------- */
static int checkSessionID(ReportSession_t *session)
{  ReportSession_t **p;
   pthread_mutex_lock(&rgSessionLock);
   p = findSessionBlock(session);
   pthread_mutex_unlock(&rgSessionLock);
   return (p != 0);
}
/* ------------------------------ srgInit ------------------------------------ */
int srgInit(sscErrorHandle hError)
{ if(!srgInitedFlg)
  { exit_phase = 0;
    snprintf(szRGCoreVersion,sizeof(szRGCoreVersion),"%d.%d",RGCORE_VERSION_MAJOR,RGCORE_VERSION_MINOR);
#ifdef INCLUDE_TIME_DATE_STRINGS
    snprintf(szRGCoreTitle,sizeof(szRGCoreTitle),"SSS Report Generator Core version %d.%d (%s %s)",
             RGCORE_VERSION_MAJOR,RGCORE_VERSION_MINOR,__TIME__,__DATE__);
#else
    snprintf(szRGCoreTitle,sizeof(szRGCoreTitle),"SSS Report Generator Core version %d.%d",
             RGCORE_VERSION_MAJOR,RGCORE_VERSION_MINOR);
#endif
    memset(&faceCond,0,sizeof(faceCond));
    memset(&rgSessionLock,0,sizeof(rgSessionLock));
    memset(&faceLock,0,sizeof(faceLock));

    if(pthread_mutex_init(&faceLock,NULL))
    { sscError(hError,"Can not create mutex 'faceLock' in srgInit()");
      return 0;     
    }

    if(pthread_mutex_init(&rgSessionLock,NULL))
    { sscError(hError,"Can not create mutex 'rgSessionLock' in srgInit()");
      return 0;     
    }

    if(pthread_cond_init(&faceCond,NULL))
    { pthread_mutex_destroy(&faceLock);
      pthread_mutex_destroy(&rgSessionLock);
      sscError(hError,"Can not create condition 'faceCond' in srgInit()");
      return 0;
    }

    if(pthread_create(&unloadManagerID,NULL,unloadManager,NULL))
    { pthread_mutex_destroy(&faceLock);
      pthread_mutex_destroy(&rgSessionLock);
      pthread_cond_destroy(&faceCond);
      sscError(hError,"Can not start the 'unloadManager' in srgInit()");
      return 0;
    }
    srgInitedFlg++;
  }
  return srgInitedFlg; 
} 
/* ------------------------------ srgDone ------------------------------------ */
int srgDone(sscErrorHandle hError)
{ if(srgInitedFlg)
  { exit_phase++;
    pthread_mutex_lock(&faceLock); pthread_cond_signal(&faceCond);
    pthread_mutex_unlock(&faceLock);
    pthread_kill(unloadManagerID,SIGKILL);
    pthread_mutex_destroy(&faceLock);
    pthread_mutex_destroy(&rgSessionLock);
    pthread_cond_destroy(&faceCond);
    srgInitedFlg = 0;
  }
  return 1;
}
/* -------------------------- srgGetAttribute -------------------------------- */
char *srgGetAttribute(sscErrorHandle hError, const char *attributeID, const char *extraAttrSpec, int *attr_type)
{ char *s = 0;

  if(attributeID)
  { if(attr_type) *attr_type = RGATTRTYPE_STATIC;
         if(!strcasecmp(attributeID,szVersion)) s = szRGCoreVersion;
    else if(!strcasecmp(attributeID,szTitle))   s = szRGCoreTitle; 
    else sscError(hError,"No such attribute '%s' in srgGetAttribute()",attributeID);
  }
  else sscError(hError,(char*)szIncorrectAttrNameNull);
  return s;
}
/* ------------------------- srgFreeAttribute -------------------------------- */
void srgFreeAttribute(sscErrorHandle hError, const char *attributeID, const char *extraAttrSpec,char *str,int restype)
{ if(restype == RGATTRTYPE_SPECIALALLOC || restype == RGATTRTYPE_MALLOCED) free(str);
}
/* ------------------------- srgSetAttribute --------------------------------- */
void srgSetAttribute(sscErrorHandle hError, const char *attributeID, const char *extraAttrSpec, const char *value)
{ if(attributeID)
  { if(!strcasecmp(attributeID,szVersion) || !strcasecmp(attributeID,szTitle)) 
    { sscError(hError,"Attempt to set read only attribute \"%s\" in srgSetAttribute()",attributeID);
    }
    else sscError(hError,"No such attribute '%s' in srgSetAttribute()",attributeID);
  }
  else sscError(hError,(char*)szIncorrectAttrNameNull);
}
/* -------------------------- srgCreateReportSession ------------------------- */
srgSessionID srgCreateReportSession(sscErrorHandle hError, const char *serverName)
{  PluginFace_t *p;
   ReportSession_t *r = 0;  

   if(serverName && serverName[0])
   { pthread_mutex_lock(&faceLock);
     if((p = findPluginByName(serverName)) == 0) linkPluginFace((p = loadNewPlugIn(hError,serverName)));
     pthread_mutex_unlock(&faceLock);

     if(p)
     { if((r = newReportSession(hError,p)) != 0)
       { if((r->sessionID = p->createSesion(hError)) == 0)
         { srgDeleteReportSession(hError,r);
           sscError(hError,"Can't create session in RG server '%s'", serverName); 
           return 0;
         }
       }
     }
   }
   return r;
} 
/* ----------------------------- srgDeleteSession ---------------------------- */
void srgDeleteReportSession(sscErrorHandle hError, srgSessionID hSession)
{  ReportSession_t *r = (ReportSession_t *)hSession;
   if(checkSessionID(r)) 
   { lockPlugin(r->plugin);
     r->plugin->deleteSesion(hError, r->sessionID);
     unlockPlugin(r->plugin);
     markToUnloadPlugin(r->plugin);
     destroyReportSession(r);
   }
   else sscError(hError,(char*)szInvSesIdIn,"srgDeleteReportSession()");
}
/* -------------------------- srgGetReportAttribute -------------------------- */
char *srgGetReportAttribute(sscErrorHandle hError, srgSessionID hSession, const char *attributeID, const char *extraAttrSpec, int *restype)
{  char *res = 0;
   ReportSession_t *r = (ReportSession_t *)hSession;
   if(checkSessionID(r)) 
   { lockPlugin(r->plugin);
     res = r->plugin->getAttribute(hError, r->sessionID, attributeID, extraAttrSpec, restype);
     unlockPlugin(r->plugin);
   }
   else sscError(hError,(char*)szInvSesIdIn,"srgGetReportAttribute()");
   return res;
}
/* ----------------------- srgFreeReportAttributeString ---------------------- */
void srgFreeReportAttributeString(sscErrorHandle hError,srgSessionID hSession,const char *attributeID,const char *extraAttrSpec,char *attrString,int restype)
{ ReportSession_t *r = (ReportSession_t *)hSession;
  if(checkSessionID(r)) 
  { lockPlugin(r->plugin);
    r->plugin->freeAttributeString(hError,r->sessionID,attributeID,extraAttrSpec,attrString,restype);
    unlockPlugin(r->plugin);
  }
  else sscError(hError,(char*)szInvSesIdIn,"srgFreeReportAttributeString()");
}
/* -------------------------- srgSetReportAttribute -------------------------- */
void srgSetReportAttribute(sscErrorHandle hError, srgSessionID hSession, const char *attributeID, const char *extraAttrSpec, const char *value)
{ ReportSession_t *r = hSession;
  if(checkSessionID(r))
  { if(attributeID)   sscValidateURLString((char*)attributeID);
    if(extraAttrSpec) sscValidateURLString((char*)extraAttrSpec);
    if(value)         sscValidateURLString((char*)value);
    lockPlugin(r->plugin);
    r->plugin->setAttribute(hError, r->sessionID, attributeID, extraAttrSpec, value);
    unlockPlugin(r->plugin);
  }
  else sscError(hError,(char*)szInvSesIdIn,"srgSetReportAttribute()");
}
/* -------------------------- srgGenerateReport ------------------------------ */
int srgGenerateReport(sscErrorHandle hError, srgSessionID hSession, int argc, char* argv[],char *rawCommandString, streamHandle result)
{  int retcode = RGPERR_GENERICERROR;
   ReportSession_t *r = hSession;
   if(checkSessionID(r))
   { lockPlugin(r->plugin);
     retcode = r->plugin->generateReport(hError, r->sessionID, argc, argv, rawCommandString, result,(char *)0);
     unlockPlugin(r->plugin);
   }
   else sscError(hError,(char*)szInvSesIdIn,"srgGenerateReport()");
   return retcode;
}
/* -------------------------- rgiOpenPlugInInfo ------------------------------ */
void rgiOpenPlugInInfo(void)
{ pthread_mutex_lock(&faceLock);
}
/* -------------------------- rgiClosePlugInInfo ----------------------------- */
void rgiClosePlugInInfo(void)
{ pthread_mutex_unlock(&faceLock);
}
/* ------------------------- rgiGetFirstPlugInInfo --------------------------- */
hPlugInInfo rgiGetFirstPlugInInfo(void)
{ return (hPlugInInfo)busyFaceHandles;
}
/* -------------------------- rgiGetNexPlugInInfo ---------------------------- */
hPlugInInfo rgiGetNexPlugInInfo(hPlugInInfo _p)
{ PluginFace_t *p = (PluginFace_t *)_p;
  if(p) p = p->link;
  return (hPlugInInfo)p;
}
/* --------------------- rgiGetPlugInPathByPlugInInfo ------------------------ */
const char *rgiGetPlugInPathByPlugInInfo(hPlugInInfo _p)
{ PluginFace_t *p = (PluginFace_t *)_p;
  return p ? (const char*)p->pname : 0;
}
/* -------------------- rgiGetPlugInTitleByPlugInInfo ------------------------ */
const char *rgiGetPlugInTitleByPlugInInfo(hPlugInInfo _p)
{ PluginFace_t *p = (PluginFace_t *)_p;
  return p ? (const char*)p->ptitle : 0;
}
/* ------------------- rgiGetPlugInVersionByPlugInInfo ----------------------- */
const char *rgiGetPlugInVersionByPlugInInfo(hPlugInInfo _p)
{ PluginFace_t *p = (PluginFace_t *)_p;
  return p ? (const char*)p->pversion : 0;
}
/* ------------------ rgiGetPlugInUsageCountByPlugInInfo --------------------- */
int rgiGetPlugInUsageCountByPlugInInfo(hPlugInInfo _p)
{ PluginFace_t *p = (PluginFace_t *)_p;
  return p ? p->usageCount : 0;
}
/* ---------------- rgiGetPlugInUnloadableFlagByPlugInInfo ------------------- */
int rgiGetPlugInUnloadableFlagByPlugInInfo(hPlugInInfo _p)
{ PluginFace_t *p = (PluginFace_t *)_p;
  return p ? p->unloadable : 0;
}
/* ---------------- rgiGetPlugInThreadSafeFlagByPlugInInfo ------------------- */
int rgiGetPlugInThreadSafeFlagByPlugInInfo(hPlugInInfo _p)
{ PluginFace_t *p = (PluginFace_t *)_p;
  return p ? p->threadSafe : 0;
}
/* ---------------- rgiGetPlugInUnloadTimeoutByPlugInInfo -------------------- */
int rgiGetPlugInUnloadTimeoutByPlugInInfo(hPlugInInfo _p)
{ PluginFace_t *p = (PluginFace_t *)_p;
  return p ? p->unloadtime : 0;
}
/* ------------- rgiGetPlugInCurrentUnloadTimeoutByPlugInInfo ---------------- */
int rgiGetPlugInCurrentUnloadTimeoutByPlugInInfo(hPlugInInfo _p)
{ PluginFace_t *p = (PluginFace_t *)_p;
  return p ? p->timeout : 0;
}

