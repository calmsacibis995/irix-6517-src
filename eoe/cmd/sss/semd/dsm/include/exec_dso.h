/* --------------------------------------------------------------------------- */
/* -                             EXEC_DSO.H                                  - */
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

#ifndef H_EXEC_DSO_H
#define H_EXEC_DSO_H

#ifdef _MSC_VER 
#define EXECDSOAPI _cdecl
#endif

#ifndef EXECDSOAPI
#define EXECDSOAPI
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------- */
/* Pass event to particular DSO module
   parameter(s):
    const char *lpszDllName - dso module name, must be non zero string
    int eventClass - event class, must be valid event class value
    int eventType - event type, must be valid event type value
    int eventPri - event priority/facility value
    int eventReptCnt - event repeate counter
    const char *eventBuf - pointer to event message buffer
    int eventBufSize - event message buffer size
    const char *lpszHostName - remote host name (maybe optional)
    const char *lpszOptionString - additional optional parameter(s)
   return result(s):
    == 0 - success, != 0 - error
*/
int EXECDSOAPI exec_dso(const char *lpszDllName,int eventClass,int eventType,int eventPri,int eventReptCnt,const char *eventBuf,int eventBufSize,const char *lpszHostName,const char *lpszOptionString);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef H_EXEC_DSO_H */
