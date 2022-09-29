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

#ident           "$Revision: 1.3 $"

#ifndef __SSPINGDEFS_H_
#define __SSPINGDEFS_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <libgen.h>
#include <string.h>
#include <syslog.h>
#include <pthread.h>
#include <stdarg.h>
#include <sys/param.h>
#include <sys/stat.h>

#define MYVERSION_MAJOR                  1
#define MYVERSION_MINOR                  0
#define MAXKEYS                          512

/*---------------------------------------------------------------------------*/
/*   Default Configuration file definitions                                  */
/*---------------------------------------------------------------------------*/

#define SSPINGDIR                        "/var/pcp/pmdas/espping"
#define SERVICESFILE                     "/var/pcp/pmdas/espping/services.conf"
#define MAPFILE                          "/var/pcp/pmdas/espping/sgm.map"

/*---------------------------------------------------------------------------*/
/*   Types of actions to be performed                                        */
/*---------------------------------------------------------------------------*/

#define ADD                              1
#define DELETE                           2
#define UPDATE                           3

/*---------------------------------------------------------------------------*/
/*   Some constants                                                          */
/*---------------------------------------------------------------------------*/

#define MAXSERNAMELEN                    36
#define MAXSERCMDLEN                     256
#define MAXHOSTLEN                       MAXHOSTNAMELEN
#define MAXSERVICESPERHOST               30
#define SSDBUSER                         NULL
#define SSDBPASSWORD                     NULL
#define SSDATABASE                       "ssdb"
#define SSDBHOST                         "localhost"

/*---------------------------------------------------------------------------*/
/*   Some Error Definitions                                                  */
/*---------------------------------------------------------------------------*/

#define INVALIDARGS                      1
#define INVALIDOPERATION                 2
#define INVALIDCOMMAND                   3
#define INVALIDHOST                      4
#define SERVICEALREADYEXISTS             5
#define SERVICENOTFOUND                  6
#define UNABLETOADDSERVICE               7
#define HOSTNOTFOUND                     8
#define FILENOTREADABLE                  9
#define TOOMANYSERVICES                  10
#define UNABLETOADDHOST                  30

/*---------------------------------------------------------------------------*/
/*   RG Definitions                                                          */
/*---------------------------------------------------------------------------*/

static const char myLogo[]               = "ESP System Monitor Setup Server";
static const char szVersion[]            = "Version";
static const char szTitle[]              = "Title";
static const char szThreadSafe[]         = "ThreadSafe";
static const char szUnloadable[]         = "Unloadable";
static const char szUnloadTime[]         = "UnloadTime";
static const char szAcceptRawCmdString[] = "AcceptRawCmdString";
static const char szUserAgent[]          = "User-Agent";
static char szServerNameErrorPrefix[]    = "ESP System Monitor Setup Server Error: %s";
static const char szUnloadTimeValue[]    = "300";
static const char szThreadSafeValue[]    = "0";
static const char szUnloadableValue[]    = "1";
static const char szAcceptRawCmdStringValue[] = "1";

static int       writetofile             = 0;
static int       readfile                = 0;

/*---------------------------------------------------------------------------*/
/*  Some Structures                                                          */
/*---------------------------------------------------------------------------*/

typedef struct service_s {
    char    service_name[MAXSERNAMELEN];
    char    service_cmd[MAXSERCMDLEN];
    int     flag;
} service_t;

typedef struct hostservice_s {
    char     host_name[MAXHOSTLEN];
    int      services[MAXSERVICESPERHOST];
    int      nextservice;
} hostservice_t;

typedef struct host_s {
    char     host_name[MAXHOSTLEN];
    int      localflag;
} host_t;

#endif /* __SSPINGDEFS_H_ */
