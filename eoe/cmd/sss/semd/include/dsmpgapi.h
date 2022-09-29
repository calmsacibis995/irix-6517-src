/* --------------------------------------------------------------------------- */
/* -                            DSMPGAPI.H                                   - */
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
#ifndef H_DSMPGAPI_H
#define H_DSMPGAPI_H

#ifdef _MSC_VER 
#define DSMPGAPI _cdecl
#endif

#ifndef DSMPGAPI
#define DSMPGAPI
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------- */
/* Init DSM PlugIn module
   parameter(s):
    NONE
   return results:
    != 0 - success, == 0 - some errors
*/
int DSMPGAPI dsmpgInit();
typedef int DSMPGAPI FPdsmpgInit();

/* --------------------------------------------------------------------------- */
/* Send event to DSM PlugIn module
   parameter(s):
    int eventClass - event class, must be valid event class value
    int eventType - event type, must be valid event type value
    int eventPri - event priority/facility, must be valid piority/facility code
    int eventReptCnt - repeate counter, must be more than 0
    const char *eventBuf - pointer to event message buffer, must be non zero
    int eventBufSize - event message buffer size, must be non zero
    const char *lpszHostName - remote host name, must be valid host name
    const char *lpszOptionString - optional parameter(s) string, can be zero
   return results:
    != 0 - success, == 0 - some errors
*/
int DSMPGAPI dsmpgExecEvent(int eventClass,int eventType,int eventPri,int eventReptCnt,const char *eventBuf,int eventBufSize,const char *lpszHostName,const char *lpszOptionString);
typedef int DSMPGAPI FPdsmpgExecEvent(int eventClass,int eventType,int eventPri,int eventReptCnt,const char *eventBuf,int eventBufSize,const char *lpszHostName,const char *lpszOptionString);

/* --------------------------------------------------------------------------- */
/* Done DSM PlugIn module
   parameter(s):
    NONE
   return results:
    != 0 - success, == 0 - some errors
*/
int DSMPGAPI dsmpgDone();
typedef int DSMPGAPI FPdsmpgDone();

#ifdef __cplusplus
}
#endif

#endif /* #ifndef H_DSMPGAPI_H */
