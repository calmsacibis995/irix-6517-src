/* --------------------------------------------------------------------------- */
/* -                              SEMAPI.H                                   - */
/* --------------------------------------------------------------------------- */
/*                                                                             */
/* Copyright 1992-1999 Silicon Graphics, Inc.                                  */
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
/* This is internal SSS API needed for EventMon/SEM communication.             */
/* Some SSS application like SGM "remote delivery" server can use it.          */
/* Addtional information available from: legalov@sgi.com                       */
/* --------------------------------------------------------------------------- */
#ifndef __SEMAPI_H_
#define __SEMAPI_H_

#ifdef _MSC_VER
#define SEMAPI _cdecl
#endif

#ifndef SEMAPI
#define SEMAPI
#endif

/* --------------------------------------------------------------------------- */
/* SEM API version definition */
#define SEMAPI_VERMAJOR              2          /* Major version number of SEM API */
#define SEMAPI_VERMINOR              0          /* Minor version number of SEM API */

/* SEM API max event size */
#define SEM_EVENTDATA_MAX_SIZE       (1024*16)  /* SEM API event data max size */

/* SEM API error code for all SEM API functions */
#define SEMERR_SUCCESS               0   /* Success */
#define SEMERR_FATALERROR            1   /* Unrecoverable */
#define SEMERR_ERROR                 2   /* Recoverable (retry) */

/* SEM API integer values name id */
#define SEMAPI_IVAL_MAXEVENTSIZE     1   /* Name id for "max event size" integer value */
#define SEMAPI_IVAL_VERMAJOR         2   /* Name id for "SEM API major version number" integer value */
#define SEMAPI_IVAL_VERMINOR         3   /* Name id for "SEM API minor version number" integer value */
#define SEMAPI_IVAL_OPENSOCKERRCNT   4   /* Name id for "Open socket error counter" integer value */
#define SEMAPI_IVAL_BINDSOCKERRCNT   5   /* Name id for "Bind socket error counter" integer value */
#define SEMAPI_IVAL_SETSOCKOPTSNDCNT 6   /* Name id for "Set Sock option snd buffer size" integer value */
#define SEMAPI_IVAL_SETSOCKOPTRCVCNT 7   /* Name id for "Set Sock option receive buffer size" integer value */
#define SEMAPI_IVAL_SNDERRORCNT      8   /* Name id for "Send error counter" integer value */
#define SEMAPI_IVAL_TRYSNDCNT        9   /* Name id for "Try to send counter" integer value */
#define SEMAPI_IVAL_SELECTERROR      10  /* Name id for "select error counter" integer value */
#define SEMAPI_IVAL_SELECTTIMEOUT    11  /* Name id for "select timeout" integer value */
#define SEMAPI_IVAL_SELECTINTR       12  /* Name id for "select interrupted" integer value */
#define SEMAPI_IVAL_RCVERROR         13  /* Name id for "receive error" integer value */
#define SEMAPI_IVAL_CLOCKSIZE        14   /* Name id for "Internal clock size" integer value */

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------- semapiInit -------------------------------- */
/* SEM API initialize function.                                                */
/* parameter(s):                                                               */
/*  int verMajor - major number of SEM API version.                            */
/*  int verMinor - minor number of SEM API version.                            */
/* return code:                                                                */
/*  SEMERR_SUCCESS - succes, SEMERR_FATALERROR - error                         */
/* note:                                                                       */
/*  verMajor must be = SEMAPI_VERMAJOR                                         */
/*  verMinor must be = SEMAPI_VERMINOR                                         */
/* --------------------------------------------------------------------------- */
int SEMAPI semapiInit(int verMajor,int verMinor);
typedef int SEMAPI FPsemapiInit(int verMajor,int verMinor);

/* ------------------------------- semapiDone -------------------------------- */
/* SEM API deinitialize function.                                              */
/* parameter(s):                                                               */
/*  none                                                                       */
/* return code:                                                                */
/*  SEMERR_SUCCESS - succes, SEMERR_FATALERROR - error                         */
/* --------------------------------------------------------------------------- */
int SEMAPI semapiDone();
typedef int SEMAPI FPsemapiDone();


/* ------------------------ semapiGetIntegerValue  --------------------------- */
/* SEM API get "integer info" function.                                        */
/* parameter(s):                                                               */
/*  int integerName - "name id" of particular integer value from SEM API       */
/*                    SEMAPI_IVAL_...                                          */
/* return code:                                                                */
/*  integer value for specified "name id"                                      */
/* --------------------------------------------------------------------------- */
int SEMAPI semapiGetIntegerValue(int integerName);
typedef int SEMAPI FPsemapiGetIntegerValue(int integerName);


/* --------------------------- semapiDeliverEvent ---------------------------- */
/*  This function is used to deliver Events to the SEM.                        */
/*  semapiDeliverEvent establishes a connection to the SEH, then sends a       */
/*  package of data to it. This function returns error codes discovered by the */
/*  API or SEH.  This will equal SEMERR_SUCCESS, if there were no errors.      */
/*  Error codes are defined in current file.                                   */
/*  parameter(s):                                                              */
/*   const char *hostNameFrom - hostname "from", can be NULL for localhost     */
/*   long eventTimeStamp - event timestamp. This allows the user to provide    */
/*                         the time values that will be entered into the SSDB  */
/*                         tables (can be 0).                                  */
/*                         The value is normally generated by the time(2)      */
/*                         function, which generates a time_t (the value of    */
/*                         time in seconds since 00:00:00 UTC, January 1,1970. */
/*   int eventClass - event class. The integer value that corresponds to the   */
/*                    class of Event.                                          */
/*   int eventType - event type. The integer value that corresponds to the     */
/*                   type of Event.                                            */
/*   int pri - event priority. Used to define the priority of the Event.       */
/*             This is needed for the SSNotify action. Should be set to 0 if   */
/*             no priority is provided.                                        */
/*   int eventCounter - multiple event count. The integer value that           */
/*                      corresponds how many of these Events have occurred.    */
/*                      This is needed since some Event generators, like       */
/*                      syslogd, does its own filtering. Several Events of the */
/*                      same type are collected, then all are logged at once   */
/*                      to the SEM. For example, if you are logging one Event, */
/*                      set this argument to 1. Notice: 0 mean 1.:)            */
/*   int eventDataLength - length of event data. The integer value that        */
/*                         corresponds how many bytes of data are to be copied */
/*                         from eventData and sent to the SEH (can't be 0).    */
/*   void *eventData - pointer to Event data. Pointer to the data to be sent   */
/*                     with the Event. Can not be NULL.                        */
/* return code:                                                                */
/*  SEMERR_SUCCESS - succes                                                    */
/*  SEMERR_FATALERROR - some fatal error. (Unrecoverable error)                */
/*  SEMERR_ERROR - some recoverable error. (For the same set of data)          */
/* --------------------------------------------------------------------------- */
int SEMAPI semapiDeliverEvent(const char *hostNameFrom, /* hostname "from" can be NULL for localhost */
                              long eventTimeStamp,      /* event timestamp, can be 0 */
                              int eventClass,           /* input Event Class, any values */
                              int eventType,            /* input Event Type, any values */
                              int pri,                  /* input EventPriority, any values */
                              int eventCounter,         /* multiple Event count, can be 0 if there is only one event */
                              int eventDataLength,      /* length of Event data, can't be 0 */
                              void *eventData);         /* pointer to Event data, can't be NULL */
typedef int SEMAPI FPsemapiDeliverEvent(const char *hostNameFrom,long eventTimeStamp,
                                        int eventClass,int eventType,int pri,
                                        int eventCounter,int eventDataLength,void *eventData);


#ifdef __cplusplus
}
#endif

#endif /* __SEMAPI_H_ */
