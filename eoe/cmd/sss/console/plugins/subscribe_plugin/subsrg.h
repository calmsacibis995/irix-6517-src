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

#ident           "$Revision: 1.2 $"

#ifndef __SUBSRG_H_
#define __SUBSRG_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <pthread.h>
#include <sys/types.h>
#include <sgmtask.h>
#include <sgmtaskerr.h>
#include <subscribe.h>
#include <ssdbapi.h>
#include "rgPluginAPI.h"
#include "sscHTMLGen.h"
#include "sscPair.h"
#include "sscShared.h"

typedef struct sss_s_MYSESSION {
  unsigned long signature;
  struct sss_s_MYSESSION *next;
  int textonly;
} mySession;

typedef struct SubscribeClass_s {
    __uint32_t    class_id;
    int           flag;
    int           (*doexitop)(char *, int, int);
} SubscribeClass_t;

/*---------------------------------------------------------------------------*/
/* Some of my defines                                                        */
/*---------------------------------------------------------------------------*/

#define  FORMHOSTNAME                     "host_s"
#define  FORMCLASSNAME                    "sub_cl"
#define  FORMTYPENAME                     "sub_ev"

#define  GETCLASSDATA                     1
#define  GETTYPEDATA                      2
#define  PROCESSDATA                      3

#define  CLASSCANTBESUBSCRIBED            1
#define  CLASSSUBSCRIBEDINFULL            2
#define  MAXINTERNALCLASSES               6

/*---------------------------------------------------------------------------*/
/* Some Global Defines                                                       */
/*---------------------------------------------------------------------------*/

#ifdef __TIME__
#ifdef __DATE__
#define INCLUDE_TIME_DATE_STRINGS 1
#endif
#endif

#define MYVERSION_MAJOR                  1
#define MYVERSION_MINOR                  0
#define MAXKEYS                          512

static const char myLogo[]               = "ESP Subscribe Setup Server";
static const char szVersion[]            = "Version";
static const char szTitle[]              = "Title";
static const char szThreadSafe[]         = "ThreadSafe";
static const char szUnloadable[]         = "Unloadable";
static const char szUnloadTime[]         = "UnloadTime";
static const char szAcceptRawCmdString[] = "AcceptRawCmdString";
static const char szUserAgent[]          = "User-Agent";
static char szServerNameErrorPrefix[]    = "ESP Subscribe Setup Server Error: %s";
static const char szUnloadTimeValue[]    = "150";
static const char szThreadSafeValue[]    = "0";
static const char szUnloadableValue[]    = "1";
static const char szAcceptRawCmdStringValue[] = "1";
static char szVersionStr[64];
static pthread_mutex_t seshFreeListmutex;
static int volatile mutex_inited         = 0;
static mySession *sesFreeList            = 0;

#endif /* __SUBSRG_H_ */
