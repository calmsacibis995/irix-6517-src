/*
 * Copyright 1997, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

/* $Id: pmapi_dev.h,v 1.18 1997/11/13 02:03:01 markgw Exp $ */

#ifndef _PMAPI_DEV_H
#define _PMAPI_DEV_H

#include "pmapi.h"
#include "impl.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*
 * This is the header for the static noship library "libpcp_dev.a".
 * It is only distributed in the pcp_root ism (for IRIX6.4
 * or earlier) and in the IRIX root ism (for IRIX6.5 or later).
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * access control routines
 */
extern int __pmAccAddOp(unsigned int);
extern int __pmAccAddHost(const char *, unsigned int, unsigned int, int);
extern int __pmAccAddClient(const struct in_addr *, unsigned int *);
extern void __pmAccDelClient(const struct in_addr *);

extern void __pmAccDumpHosts(FILE *);
extern int __pmAccSaveHosts(void);
extern int __pmAccRestoreHosts(void);
extern void __pmAccFreeSavedHosts(void);

extern unsigned int __pmMakeAuthCookie(unsigned int, pid_t);

/*
 * Must be called by authorised clients in order that communication
 * with either a licensed or an unlicensed pmcd can take place.
 */
extern void __pmSetAuthClient(void);

/*
 * nodelock license check
 */
extern int __pmGetLicense(int, const char *, int);
#define GET_LICENSE_SILENT	0x00
#define GET_LICENSE_SHOW_EXP	0x01
#define GET_LICENSE_SHOW_NOLIC	0x02
#define GET_LICENSE_SHOW_ALL	0x7f
#define GET_LICENSE_DIE		0x80

/*
 * AF - general purpose asynchronous event management routines
 */
extern int __pmAFregister(const struct timeval *, void *, void (*)(int, void *));
extern int __pmAFunregister(int);
extern void __pmAFblock(void);
extern void __pmAFunblock(void);
extern int __pmAFisempty(void);

/*
 * private PDU protocol between pmlc and pmlogger
 */
#define LOG_PDU_VERSION1	1	/* datax pdus & PCP 1.x error codes */
#define LOG_PDU_VERSION2	2	/* private pdus & PCP 2.0 error codes */
#define LOG_PDU_VERSION		LOG_PDU_VERSION2

#define LOG_REQUEST_NEWVOLUME	1
#define LOG_REQUEST_STATUS	2
#define LOG_REQUEST_SYNC	3

typedef struct {
    __pmTimeval  ls_start;	/* start time for log */
    __pmTimeval  ls_last;	/* last time log written */
    __pmTimeval  ls_timenow;	/* current time */
    int		ls_state;	/* state of log (from __pmLogCtl) */
    int		ls_vol;		/* current volume number of log */
    __int64_t	ls_size;	/* size of current volume */
    char	ls_hostname[PM_LOG_MAXHOSTLEN];
				/* name of pmcd host */
    char	ls_fqdn[PM_LOG_MAXHOSTLEN];
				/* fully qualified domain name of pmcd host */
    char	ls_tz[40];      /* $TZ at collection host */
    char	ls_tzlogger[40]; /* $TZ at pmlogger */
} __pmLoggerStatus;

#define PDU_LOG_CONTROL		0x8000
#define PDU_LOG_STATUS		0x8001
#define PDU_LOG_REQUEST		0x8002

extern int __pmConnectLogger(const char *, int *, int *);
extern int __pmSendLogControl(int, const pmResult *, int, int, int);
extern int __pmDecodeLogControl(const __pmPDU *, pmResult **, int *, int *, int *);
extern int __pmSendLogRequest(int, int);
extern int __pmDecodeLogRequest(const __pmPDU *, int *);
extern int __pmSendLogStatus(int, __pmLoggerStatus *);
extern int __pmDecodeLogStatus(__pmPDU *, __pmLoggerStatus **);

/*
 * other interfaces shared by pmlc and pmlogger
 */

extern int __pmControlLog(int, const pmResult *, int, int, int, pmResult **);

#define PM_LOG_OFF		0	/* state */
#define PM_LOG_MAYBE		1
#define PM_LOG_ON		2

#define PM_LOG_MANDATORY	11	/* control */
#define PM_LOG_ADVISORY		12
#define PM_LOG_ENQUIRE		13

/* macros for logging control values from __pmControlLog() */
#define PMLC_SET_ON(val, flag) \
        val = (val & ~0x1) | (flag & 0x1)
#define PMLC_GET_ON(val) \
        (val & 0x1)
#define PMLC_SET_MAND(val, flag) \
        val = (val & ~0x2) | ((flag & 0x1) << 1)
#define PMLC_GET_MAND(val) \
        ((val & 0x2) >> 1)
#define PMLC_SET_AVAIL(val, flag) \
        val = (val & ~0x4) | ((flag & 0x1) << 2)
#define PMLC_GET_AVAIL(val) \
        ((val & 0x4) >> 2)
#define PMLC_SET_INLOG(val, flag) \
        val = (val & ~0x8) | ((flag & 0x1) << 3)
#define PMLC_GET_INLOG(val) \
        ((val & 0x8) >> 3)

#define PMLC_SET_STATE(val, state) \
        val = (val & ~0xf) | (state & 0xf)
#define PMLC_GET_STATE(val) \
        (val & 0xf)

/* 28 bits of delta, 32 bits of state */
#define PMLC_MAX_DELTA  0x0fffffff

#define PMLC_SET_DELTA(val, delta) \
        val = (val & 0xf) | (delta << 4)
#define PMLC_GET_DELTA(val) \
        (((val & ~0xf) >> 4) & PMLC_MAX_DELTA)

/*
 * deprecated pmlc <-> pmlogger protocol interfaces
 */
#define PMLC_PDU_STATUS_REQ	1
#define PMLC_PDU_STATUS		2
#define PMLC_PDU_NEWVOLUME	3
#define PMLC_PDU_SYNC		4

extern int __pmSendDataX(int, int, int, int, const void *);
extern int __pmDecodeDataX(__pmPDU *, int, int *, int *, void **);
extern int __pmSendControlReq(int, int, const pmResult *, int, int, int);
extern int __pmDecodeControlReq(const __pmPDU *, int, pmResult **, int *, int *, int *);

#ifdef __cplusplus
}
#endif

#endif /* _PMAPI_DEV_H */
