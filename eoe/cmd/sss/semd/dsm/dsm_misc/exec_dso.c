/* --------------------------------------------------------------------------- */
/* -                             EXEC_DSO.C                                  - */
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
/* Note:                                                                       */
/* Add to build make file  '-woff 1048' option                                 */
/* --------------------------------------------------------------------------- */
#include <stdio.h>
#include <dlfcn.h>
#include "dsmpgapi.h"
#include "exec_dso.h"

/* --------------------- exec_dso ------------------------ */
int exec_dso(const char *lpszDllName,int eventClass,int eventType,int eventPri,int eventReptCnt,const char *eventBuf,int eventBufSize,const char *lpszHostName,const char *lpszOptionString)
{ FPdsmpgExecEvent *fpdsmpgExecEvent;
  FPdsmpgInit *fpdsmpgInit;
  FPdsmpgDone *fpdsmpgDone;
  void *plugInHandle;
  int retcode = 1;

  if(lpszDllName && lpszDllName[0] && eventBuf && eventBufSize && (plugInHandle = dlopen(lpszDllName,RTLD_LAZY)) != NULL)
  { if(((fpdsmpgInit = (FPdsmpgInit*)dlsym(plugInHandle,"dsmpgInit")) != 0) &&
       ((fpdsmpgDone = (FPdsmpgDone*)dlsym(plugInHandle,"dsmpgDone")) != 0) &&
       ((fpdsmpgExecEvent = (FPdsmpgExecEvent*)dlsym(plugInHandle,"dsmpgExecEvent")) != 0))
    { if(!fpdsmpgInit())
      { retcode = fpdsmpgExecEvent(eventClass,eventType,eventPri,eventReptCnt,eventBuf,eventBufSize,lpszHostName,lpszOptionString);
        fpdsmpgDone();
      }
    }
    dlclose(plugInHandle);
  }
  return retcode;
}
