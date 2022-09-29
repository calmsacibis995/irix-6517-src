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
#include <string.h>
#include <time.h>
#include <dlfcn.h>
#include "dsmpgapi.h"

/* --------------------- exec_dso ------------------------ */
int exec_dso(const char *lpszDllName,int eventClass,int eventType,int eventPri,int eventReptCnt,const char *eventBuf,int eventBufSize,const char *lpszHostName,const char **lpszOptionString)
{ FPdsmpgExecEvent *fpdsmpgExecEvent;
  FPdsmpgInit *fpdsmpgInit;
  FPdsmpgDone *fpdsmpgDone;
  void *plugInHandle;
  int retcode = 0;

  if(lpszDllName && lpszDllName[0] && eventBuf && eventBufSize && (plugInHandle = dlopen(lpszDllName,RTLD_LAZY)) != NULL)
  { if(((fpdsmpgInit = (FPdsmpgInit*)dlsym(plugInHandle,"dsmpgInit")) != 0) &&
       ((fpdsmpgDone = (FPdsmpgDone*)dlsym(plugInHandle,"dsmpgDone")) != 0) &&
       ((fpdsmpgExecEvent = (FPdsmpgExecEvent*)dlsym(plugInHandle,"dsmpgExecEvent")) != 0))
    { if(fpdsmpgInit())
      { retcode = fpdsmpgExecEvent(eventClass,eventType,eventPri,eventReptCnt,eventBuf,eventBufSize,lpszHostName,lpszOptionString);
        fpdsmpgDone();
      }
    }
    dlclose(plugInHandle);
  }
  return retcode;
}

void Usage(char *s) {

          fprintf(stderr,
                  "Usage: %s [-C event class] \n"
                  "          [-T event type]  \n"
                  "          [-H hostname  ]  \n"
                  "          [-N event count] \n"
                  "          [-D event data]  \n",s);
    exit(1);
}

int main(int argc, char *argv[])
{
  int c=0;
  unsigned length=0,count=0,send=0;
  int ev_class=0,ev_type=0;
  char *ev_data=NULL;
  char hostname[256];
  char ev_ts[20];
  char *args[2];
  int  err = 0;

  if ( argc < 8 ) {
    Usage(argv[0]);
  }
  sprintf(ev_ts, "%d", time(0));
  strcpy(hostname, "localhost");
  while ( -1 != (c = getopt(argc,argv,"H:C:T:D:N:")))
    {
      switch (c)
        {
        case 'H':
          strcpy(hostname, optarg);
          break;
        case 'C':
          ev_class = atoi(optarg);
          break;
        case 'T':
          ev_type = atoi(optarg);
          break;
        case 'N':
          count = atoi(optarg);
          break;
        case 'D':
          ev_data = optarg;
          break;
        default: /* unknown or missing argument */
          Usage(argv[0]);
        } /* switch */
    } /* while */

    length = strlen(ev_data);
    args[0] = hostname;
    args[1] = ev_ts;

    err = exec_dso("/usr/lib32/internal/libsgmtask.so", ev_class, ev_type, 0, count, ev_data, length, NULL, (const char **) args);

    printf("exec_dso returned %d\n", err);
}
