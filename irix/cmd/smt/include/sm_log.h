#ifndef SM_LOG_H
#define SM_LOG_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.10 $
 */

#include <sys/syslog.h>

#define SM_LOGON_STDERR		0
#define SM_LOGON_SYSLOG		1

#define SM_ISSYSERR		1

extern void sm_openlog(int, int, const char *, int, int);
extern void sm_log(int, int, const char *, ...);
extern void sm_hexdump(int, const char *, int);
extern void sm_loglevel(int);

#define LOG_DBGCF	(LOG_DEBUG+1)
#define LOG_DBGSCHED	(LOG_DEBUG+2)
#define LOG_DBGSNMP	(LOG_DEBUG+3)
#define LOG_DBGTLV	(LOG_DEBUG+4)
#define LOG_DBGFS	(LOG_DEBUG+5)
#define LOG_DBGEVNT	(LOG_DEBUG+6)

#ifdef DEBUG
#define CFDEBUG(x)	sm_log x
#define SCHEDDEBUG(x)	sm_log x
#define SNMPDEBUG(x)	sm_log x
#define ERROR(string)	sm_log(LOG_DBGSNMP, 0, string)
#define TLVDEBUG(x)	sm_log x
#define FSDEBUG(x)	sm_log x
#define EVNTDEBUG(x)	sm_log x
#else
#define ERROR(string)
#define CFDEBUG(x)
#define SCHEDDEBUG(x)
#define SNMPDEBUG(x)
#define TLVDEBUG(x)
#define FSDEBUG(x)
#define EVNTDEBUG(x)
#endif

#endif
