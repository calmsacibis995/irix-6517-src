/* --------------------------------------------------------------------------- */
/* -                             SSCERRORS.C                                 - */
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
#include <stdarg.h>
#include <pthread.h>
#include <sys/types.h>
#include <malloc.h>

#include "sscErrors.h"

/* --------------------------------------------------------------------------- */
typedef struct sss_s_ERROR_OBJECT {
  struct sss_s_ERROR_OBJECT *link;
  int    errorCode;
  streamHandle  *errLog;
} errObject;
/* --------------------------------------------------------------------------- */
static pthread_mutex_t errorLock = PTHREAD_MUTEX_INITIALIZER;
static errObject *busyHandles = 0; 
static errObject *freeHandles = 0;
static unsigned long volatile errblkAllocCnt = 0;
static unsigned long volatile errblkFreeCnt  = 0;
static unsigned long volatile errblkBusyCnt  = 0;
/* ------------------------ findErrorBlock ----------------------------------- */
static errObject **findErrorBlock(errObject *hError)
{  errObject **p = &busyHandles;
   while(*p)
   { if(*p == hError) return p;
     p = &((*p)->link);
   }
   return 0;
}
/* ------------------------- sscErrorGetInfo --------------------------------- */
unsigned long SSCAPI sscErrorGetInfo(int idx)
{ switch(idx) {
  case SSCERR_GETERRBLK_ALLOC : return errblkAllocCnt;
  case SSCERR_GETERRBLK_BUSY  : return errblkBusyCnt;
  case SSCERR_GETERRBLK_FREE  : return errblkFreeCnt;
  };
  return 0;
}
/* ----------------------- sscCreateErrorObject ------------------------------ */
sscErrorHandle SSCAPI sscCreateErrorObject(void)
{ errObject *err;
  pthread_mutex_lock(&errorLock);
  if((err = freeHandles) != NULL) { freeHandles = err->link; errblkFreeCnt--; }
  pthread_mutex_unlock(&errorLock);
  if(!err) if((err = (errObject*)malloc(sizeof(errObject))) != 0) errblkAllocCnt++;
  if(err)
  { memset(err,0,sizeof(errObject));
    err->errLog = newMemoryStream();
    pthread_mutex_lock(&errorLock);
    if(err->errLog)
    { errblkBusyCnt++;
      err->link = busyHandles; busyHandles = err;
    }
    else
    { err->link = freeHandles; freeHandles = err;
      errblkFreeCnt++; err = 0;
    }
    pthread_mutex_unlock(&errorLock);
  }
  return err;
}
/* ---------------------- sscDeleteErrorObject ------------------------------ */
int SSCAPI sscDeleteErrorObject(sscErrorHandle hError)
{ errObject *err, **p;
  streamHandle stream;
  if(hError)
  { pthread_mutex_lock(&errorLock);
    if((p = findErrorBlock(hError)) == NULL)
    { pthread_mutex_unlock(&errorLock);
      return -1;
    }
    err = *p; *p = err->link;
    err->link = freeHandles;  freeHandles = err;
    errblkFreeCnt++; errblkBusyCnt--;
    stream = err->errLog;
    pthread_mutex_unlock(&errorLock);
    destroyStream(stream);
  }
  return 0;
}
/* ----------------------- sscIsErrorObject -------------------------------- */
int SSCAPI sscIsErrorObject(sscErrorHandle hError)
{  errObject  **p = 0;
   if(hError)
   { pthread_mutex_lock(&errorLock);
     p = findErrorBlock(hError);
     pthread_mutex_unlock(&errorLock);
   }
   return (p != 0); 
}
/* --------------------- sscGetErrorStream --------------------------------- */
streamHandle SSCAPI sscGetErrorStream(sscErrorHandle hError)
{  return (sscIsErrorObject(hError) != 0) ? ((errObject*)hError)->errLog : 0;
}
/* ---------------------- sscMarkErrorZone --------------------------------- */
int SSCAPI sscMarkErrorZone(sscErrorHandle hError)
{  if(sscIsErrorObject(hError)) ((errObject*)hError)->errorCode = 0;
   return 0;
}
/* -------------------- sscWereErrorsInZone -------------------------------- */
int SSCAPI sscWereErrorsInZone(sscErrorHandle hError)
{  return (sscIsErrorObject(hError) != 0) ? ((errObject*)hError)->errorCode : 0;
}
/* ----------------------- sscWereErrors ----------------------------------- */
int SSCAPI sscWereErrors(sscErrorHandle hError)
{  return (sscIsErrorObject(hError) != 0) ?
          sizeofStream(((errObject*)hError)->errLog) : 0;
}
/* ------------------------ sscError --------------------------------------- */
int SSCAPI sscError(sscErrorHandle hError,char *msg, ...)
{  char tmp[2048];
   va_list args;
   if(sscIsErrorObject(hError) && msg)
   {  va_start(args,msg); vsnprintf(tmp,sizeof(tmp),msg,args); va_end(args);
      ((errObject*)hError)->errorCode = -1;
      putString("\n<p>",((errObject*)hError)->errLog); 
      putString(tmp,((errObject*)hError)->errLog);
   }
   return 0;
}
/* ---------------------- sscErrorStream ----------------------------------- */
int SSCAPI sscErrorStream(sscErrorHandle hError,char *msg, ...)
{  char tmp[2048];
   va_list args;
   if(sscIsErrorObject(hError) && msg)
   {  va_start(args,msg); vsnprintf(tmp,sizeof(tmp),msg,args); va_end(args);
      ((errObject*)hError)->errorCode = -1;
      putString(tmp,((errObject*)hError)->errLog);
   }
   return 0;
}
