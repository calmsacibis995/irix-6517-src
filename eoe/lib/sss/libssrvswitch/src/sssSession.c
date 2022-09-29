/* --------------------------------------------------------------------------- */
/* -                             SSSSESSION.C                                - */
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
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <malloc.h>

#include "rgAPI.h"
#include "sssSession.h"
#include "sssStatus.h"

extern SSSGLBINFO sssGlbInfo;     /* Switcher Global Info structure */

static sssSession *ssesFreeList = 0;
static rgPluginHead *rgPluginHeadFreeList = 0;
static pthread_mutex_t ssesFreeListMutex = PTHREAD_MUTEX_INITIALIZER;
/* -------------------------- newPluginHead ---------------------------------- */
static rgPluginHead *newPluginHead(const char *plugin)
{ rgPluginHead *p;
  pthread_mutex_lock(&ssesFreeListMutex);
  if((p = rgPluginHeadFreeList) != 0)
  { rgPluginHeadFreeList = p->link;
    sssGlbInfo.statTotalrgPluginHeadInFreeList--;
  }
  pthread_mutex_unlock(&ssesFreeListMutex);
  if(!p)
  { if((p = (rgPluginHead*)malloc(sizeof(rgPluginHead))) != 0)
     sssGlbInfo.statTotalrgPluginHeadAlloced++;
  }
  if(p)
  { memset(p,0,sizeof(rgPluginHead));
    if(plugin)
    { strncpy(p->pname,plugin,sizeof(p->pname)-1);
      p->pname[sizeof(p->pname)-1] = 0;
    }
  }
  return p;
}
/* -------------------------- deletePluginHead ------------------------------- */
static void deletePluginHead(rgPluginHead *p)
{ pthread_mutex_lock(&ssesFreeListMutex);
  p->link = rgPluginHeadFreeList; rgPluginHeadFreeList = p;
  sssGlbInfo.statTotalrgPluginHeadInFreeList++;
  pthread_mutex_unlock(&ssesFreeListMutex);
}
/* ---------------------------- newSSSSession -------------------------------- */
sssSession *newSSSSession(sscErrorHandle hError)
{  sssSession *s;
   pthread_mutex_lock(&ssesFreeListMutex);
   if((s = ssesFreeList) != 0)
   { ssesFreeList = s->next;
     sssGlbInfo.statTotalsssSessionInFreeList--;
   }
   pthread_mutex_unlock(&ssesFreeListMutex);
   if(!s)
   { if((s = (sssSession*)malloc(sizeof(sssSession))) != 0)
      sssGlbInfo.statTotalsssSessionAlloced++;
   }
   if(s)
   { memset(s,0,sizeof(sssSession));
     s->err = hError;
   }
   return s;
}
/* --------------------------- closeSSSSession ------------------------------- */
void closeSSSSession(sssSession *s)
{  rgPluginHead *p, *q;
   
   for(p = s->openRGSessions;p;)
   {  q = p; p = p->link;
      srgDeleteReportSession(s->err,q->session);
      deletePluginHead(q);
   }
   pthread_mutex_lock(&ssesFreeListMutex);
   s->next = ssesFreeList; ssesFreeList = s;
   sssGlbInfo.statTotalsssSessionInFreeList++;
   pthread_mutex_unlock(&ssesFreeListMutex);
}
/* ---------------------------- addRGPlugin ---------------------------------- */
void addRGPlugin(const char *pluginName, srgSessionID session, sssSession *SSS)
{  rgPluginHead *p;
   if((p = newPluginHead(pluginName)) != 0)
   { p->link = SSS->openRGSessions; SSS->openRGSessions = p;
     p->session = session;
   }
   else sscError(SSS->err, "No memory to create plugin header in addRGPlugin()");
}
