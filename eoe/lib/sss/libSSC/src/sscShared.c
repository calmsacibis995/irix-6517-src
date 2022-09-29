/* --------------------------------------------------------------------------- */
/* -                             SSCSHARED.C                                 - */
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

#include "sscShared.h"

static pthread_mutex_t sharedLock = PTHREAD_MUTEX_INITIALIZER;
static unsigned long ulArray[SSC_SHARED_MAXLONGIDX];
static char strArray[SSC_SHARED_MAXSTRIDX][SSC_SHARED_MAXSTRLEN+1];

/* -------------------------- sscSetSharedLong ------------------------------- */
int SSCAPI sscSetSharedLong(int idx,unsigned long value)
{ int retcode = 0;
  if(idx < SSC_SHARED_MAXLONGIDX && idx >= 0)
  { pthread_mutex_lock(&sharedLock);
    ulArray[idx] = value;
    pthread_mutex_unlock(&sharedLock);
    retcode++;
  }
  return retcode;
}
/* -------------------------- sscGetSharedLong ------------------------------- */
unsigned long SSCAPI sscGetSharedLong(int idx)
{ unsigned long retcode = 0l;
  if(idx < SSC_SHARED_MAXLONGIDX && idx >= 0)
  { pthread_mutex_lock(&sharedLock);
    retcode = ulArray[idx];
    pthread_mutex_unlock(&sharedLock);
  }
  return retcode;
}
/* ------------------------ sscSetSharedString ------------------------------- */
int SSCAPI sscSetSharedString(int idx,const char *str)
{ int retcode = 0;
  if(str && idx < SSC_SHARED_MAXSTRIDX && idx >= 0)
  { if((retcode = strlen(str)) > SSC_SHARED_MAXSTRLEN) retcode = SSC_SHARED_MAXSTRLEN;
    pthread_mutex_lock(&sharedLock);
    strncpy(strArray[idx],str,retcode);
    strArray[idx][retcode] = 0;
    pthread_mutex_unlock(&sharedLock);
  }
  return retcode;
}
/* ------------------------ sscGetSharedString ------------------------------- */
int SSCAPI sscGetSharedString(int idx,char *strbuf,int strbufsize)
{ int retcode = 0;
  if(strbuf && (strbufsize > 0) && (idx < SSC_SHARED_MAXSTRIDX) && (idx >= 0))
  { pthread_mutex_lock(&sharedLock);
    strArray[idx][SSC_SHARED_MAXSTRLEN] = 0;
    if((retcode = strlen(strArray[idx])) >= strbufsize) retcode = (strbufsize - 1);
    strncpy(strbuf,strArray[idx],retcode);
    strbuf[retcode] = 0;
    pthread_mutex_unlock(&sharedLock);
  }
  return retcode;
}
