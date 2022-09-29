/* --------------------------------------------------------------------------- */
/* -                             EVMONAPI.H                                  - */
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
/* EventMon API is a set of functions for connecting to and communicating with */
/* EventMon demon. (EventMon demon is a system demon responsible for           */
/* intercepting all system events messages from syslog demon, filtering and    */
/* buffering them.)                                                            */
/* EventMon API functions use two named pipe, provided by EventMon demon, and  */
/* internal commands for communication with EventMon demon. EventMon API uses  */
/* reduced set of error codes in functions.                                    */
/* --------------------------------------------------------------------------- */
/* $Revision: 1.2 $ */
#ifndef H_EVMONAPI_H
#define H_EVMONAPI_H

#ifdef _MSC_VER 
#define EVMONAPI _cdecl
#endif

#ifndef EVMONAPI
#define EVMONAPI
#endif

#define EVMONAPI_MAXEVENTSIZE (1024*16)  /* EventMon API max event message size
                                            (sorry for this limitation, but you can increase
                                            this value if you need more:), this is
                                            middle ceiling value */

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------- emapiIsDaemonInstalled ---------------------------- */
/* Check file location of eventmonitor file - "eventmond" in /usr/etc directory.
   (exist or not exist (to be or not to be:))
   parameter(s):
    none
   return result(s):
    != 0 - daemon binary file exist, == 0 - file not found
   note:
    useful function for very accurate people and installation rountines.
*/
int EVMONAPI emapiIsDaemonInstalled(void);
typedef int EVMONAPI FPemapiIsDaemonInstalled(void);

/* ----------------------- emapiIsDaemonStarted ------------------------------ */
/* Check eventmonitor daemon started status.
   parameter(s):
    none
   return result(s):
    != 0 - daemon started, == 0 - daemon not started
*/   
int EVMONAPI emapiIsDaemonStarted(void);
typedef int EVMONAPI FPemapiIsDaemonStarted(void);

/*---------------------- emapiDeclareDaemonUnload ---------------------------- */
/* Declare daemon unload prcedure
   parameter(s):
    none
   return result(s):
    != 0 - daemon unload procedure started,
    == 0 - daemon unload procedure not started
   note:
    application(caller) must have root permissions
*/   
int EVMONAPI emapiDeclareDaemonUnload(void);
typedef int EVMONAPI FPemapiDeclareDaemonUnload(void);

/* ------------------ emapiDeclareDaemonReloadConfig ------------------------- */
/* Declare 'start' daemon 'reload configuration info prcedure'
   parameter(s):
    none
   return result(s):
    != 0 - daemon will start 'reload configuration info',
    == 0 - can't start 'reload configuration info'
   note:
    a) application(caller) must have root permissions
    b) really 'reload config info' can be very slow process, eventmod
       used special thread for this process.
*/   
int EVMONAPI emapiDeclareDaemonReloadConfig(void);
typedef int EVMONAPI FPemapiDeclareDaemonReloadConfig(void);

/* ------------------------ emapiSendEvent------------------------------------ */
/* Send event to eventmon daemon.
   parameter(s):
    char *hostname - host name from event coming (can be 0 for "localhost")
    unsigned long time -  the time value since 00:00:00 GMT, Jan. 1, 1970,
                          measured in seconds. If time equ 0 - not defined.
    int etype - event type (message seq. number)
    int epri - event priority/facility
    char *eventbuffer - event message buffer (ASCIZ string)
   return result(s):
    != 0 - event message was sent successfuly, else some error
   note:
    this function used eventmonitor 'out of band' protocol agreements
*/
int EVMONAPI emapiSendEvent(char *hostname_from,unsigned long time,int etype,int epri,char *eventbuffer);
typedef int EVMONAPI FPemapiSendEvent(char *hostname_from,unsigned long time,int etype,int epri,char *eventbuffer);

/* ----------------------- emapiSendMessage ---------------------------------- */
/* Send message to eventmon daemon.
   parameter(s):
    const char *msgbuf - message buffer (must be non zero)
    int msgsize - message size (bytes) (must be non zero)
    char *responsebuffer - buffer for response message (can be zero)
    int *responsebuffersize - input: response buffer size (can be zero)
                              output: actual size of response message
   return result(s):
    != 0 - message was sent successfuly, else some error
   note:
    Now for internal usage only. (Configuration purposes.) 
 */
int EVMONAPI emapiSendMessage(const char *msgbuf,int msgsize,char *respbuffer, int *respbufsize);
typedef int EVMONAPI FPemapiSendMessage(const char *msgbuf,int msgsize,char *respbuffer, int *respbufsize);

#ifdef __cplusplus
}
#endif
#endif /* #ifndef H_EVMONAPI_H */

