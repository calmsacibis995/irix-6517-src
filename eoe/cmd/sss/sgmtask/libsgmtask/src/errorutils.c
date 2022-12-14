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

#ident           "$Revision: 1.6 $"

/*---------------------------------------------------------------------------*/
/* errorutils.c                                                              */
/*  Has a couple of error utilities that are used by various functions       */
/*---------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <rpc/rpc.h>
#include "sgmtaskerr.h"

#ifdef DEBUG
int   debug_level = LOG_DEBUG;
#else
int   debug_level = LOG_ERR;
#endif

int SGMStrErr(__uint32_t , char *, int );

/*---------------------------------------------------------------------------*/
/* errorexit                                                                 */
/* This is a generic errorexit function that gets called from any other      */
/* programs.  The input parameters that this function takes are              */
/*                                                                           */
/* exitcode    -   If the program wants to exit because of a catastrophic    */
/*                 error, the value of exitcode should not be 0.             */
/* sysloglevel -   If stderr is not a tty which happens in case any program  */
/*                 gets exec'd from another program (like DSM), and the      */
/*                 calling program closes stderr, then the message gets      */
/*                    logged to SYSLOG.  The sysloglevel determines the      */
/*                    priority level of the message that gets logged in      */
/*                    SYSLOG.  This sysloglevel has 2 conditions :           */
/*                    - If the stderr of the program is a tty, then          */
/*                      sysloglevel is ignored.                              */
/*                    - If sysloglevel > debug_level, then sysloglevel       */
/*                      is ignored.                                          */
/*                    In all other cases, message will be logged to SYSLOG.  */
/* format      -   specifies the format for the message.                     */
/* ...         -   Variable arguments.                                       */
/*                                                                           */
/*---------------------------------------------------------------------------*/

void errorexit(char *progname, int exitcode, int sysloglevel, char *format, ...)
{

    va_list  args;
    int      nobytes = 0;
    char     errString[1024];

    va_start(args, format);
    nobytes += vsnprintf(errString, 1024, format, args);
    va_end(args);

    if ( isatty(fileno(stderr)) ) {
	fprintf(stderr, "%s\n", errString);
    } else {
	if ( sysloglevel >= 0 ) {
	    openlog(progname, LOG_PID, LOG_LOCAL0);
	    setlogmask(LOG_UPTO(debug_level));
	    syslog(sysloglevel, errString);
	    closelog();
	}
    }

    if ( exitcode != 0 ) {
	exit(exitcode);
    }

    return;
}

/*---------------------------------------------------------------------------*/
/* pSSSErr                                                                   */
/* This is a decoder for errors generated by RPC Client/Server Task          */
/*---------------------------------------------------------------------------*/

void pSSSErr(__uint32_t  err)
{
    char         errString[1024];

    if ( err <= 0 ) return;

    if ( SGMStrErr(err, errString, sizeof(errString)) > 0 ) 
        errorexit("espd", 0, LOG_ERR, "%s", errString);
    return;
}

/*---------------------------------------------------------------------------*/
/* SGMStrErr                                                                 */
/* This is a decoder for errors generated by RPC Client/Server Task          */
/*---------------------------------------------------------------------------*/

int SGMStrErr(__uint32_t err, char *buffer, int buflen)
{
    int            nobytes = 0; 
    __uint32_t     tmperr  = 0;
    int            acterr  = 0;
    int            ssserr  = 0;

    if ( err <= 0 ) return(0);

    if ( buffer == NULL || buflen <= 0 ) return(0);

    tmperr = err;
    acterr = (tmperr&0xff);
    nobytes += snprintf(buffer+nobytes, buflen-nobytes, "Error in ");
    switch((tmperr>>24)&0xff) {
        case 3:
            nobytes += snprintf(buffer+nobytes, buflen-nobytes,
                                "ESP Module ");
            ssserr = 1;
            break;
        case 4:
            nobytes += snprintf(buffer+nobytes, buflen-nobytes,
                                "ESP RPC Task ");
            break;
        default:
            nobytes += snprintf(buffer+nobytes, buflen-nobytes,
                                "UnknownModule ");
    }

    switch((tmperr>>16)&0xff) {
        case CLIENTERR:
            nobytes += snprintf(buffer+nobytes, 1024-nobytes,
                                "Client Side, ");
            break;
        case SERVERERR:
            nobytes += snprintf(buffer+nobytes, 1024-nobytes,
                                "Server Side, ");
            break;
        default:
            nobytes += snprintf(buffer+nobytes, 1024-nobytes,
                                "Unknown Side, ");
            break;
    }

    if (ssserr) {
        switch((tmperr>>8)&0xff) {
            case SSDBERR:
                switch(acterr) {
                    case NORECORDSFOUND:
                        nobytes+=snprintf(buffer+nobytes, buflen-nobytes,
                                   "SSDB Error, Unable to find any records for the selected query");
                        break;
                    case UNKNOWNSSDBERR:
                        nobytes += snprintf(buffer+nobytes, buflen-nobytes,
                                   "Unknown SSDB Error");
                        break;
                    case UNABLETOGETACTIONID:
                        nobytes += snprintf(buffer+nobytes, buflen-nobytes,
                                   "Unable to get retrieve information from Database for selected operation. The data may have been deleted");
                        break;
                    case TOOMANYSUBSCRIPTIONS:
			nobytes += snprintf(buffer+nobytes, buflen-nobytes,
				   "The limit of maximum subscriptions as allowed by the Group Manager license is reached.  No further subscriptions are allowed");
			break;
                    default:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "SSDB API Error no. -%d.", acterr);
                        break;
                }
                break;
            case EVENTMON:
                nobytes += snprintf(buffer+nobytes, buflen-nobytes,
                    "EventMon %s", (acterr==1 ? "Not Running" : "API Error"));
                break;
            default:
                nobytes += snprintf(buffer+nobytes, buflen-nobytes,
                    "Unknown error %d", tmperr&0xffff);
                break;
        }
    } else {
        switch((tmperr>>8)&0xff) {
            case OSERR:
                switch(acterr) {
                    case MALLOCERR:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Unable to allocate memory. malloc/calloc returned NULL");
                        break;
                    case GETTEMPFILENAME:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Can't get a temporary filename from Operating System");
                        break;
                    default:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "OS Error no. %d.  Please refer to /usr/include/sys/errno.h to find the actual meaning of the error", acterr);                        break;
                }
                break;
            case RPCERR:
                switch(acterr) {
                    case RPCTCPPROTOINITERR:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "RPC TCP Protocol Init Error.  Please check whether all RPC services are running on both Client and Server");
                        break;
                    case CLOBBEREDDATA:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "RPC clobbered data.  Data sent didn't match data received");
                        break;
                    default:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "RPC Protocol Error %s (%d)", 
                                           clnt_sperrno((enum clnt_stat) acterr), acterr);
                        break;
                }
                break;
            case AUTHERR:
                switch(acterr) {
                    case CANTGETAUTHINFO:
                        nobytes += snprintf(buffer+nobytes, buflen-nobytes,
                                            "Unable to extract authentication information"
                                            " for host from SSDB.");
                        break;
                    case CANTCREATEAUTHINFO:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Unable to create/extract authentication info. for host");
                        break;
                    case CANTUNPACKDATA:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Unable to decrypt data.  Possible key mismatch");
                        break;
                    default:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Unknown authentication error! %d", acterr);
                        break;
                }
                break;
            case PARAMERR:
                switch(acterr) {
                    case INVALIDARGS:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Invalid arguments received by procedure/function");
                        break;
                    case INVALIDHOSTNAME:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Invalid Hostname. Some possibilities include trying to do a subscribe/unsubscribe to localhost or no Hostname is passed.");
                        break;
                    case INVALIDTASKPTR:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Invalid internal task pointer");
                        break;
                    case INVALIDDATAPTR:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Invalid internal data pointer");
                        break;
                    case INVALIDPROC:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Invalid RPC Procedure.");
                        break;
                    case INVALIDOFFSET:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Invalid offset specified.");
                        break;
                    case INVALIDFILENAME:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Not a valid filename");
                        break;
                    case NOTAREGULARFILE:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Filename specified is not a regular file");
                        break;
                    case INCOMPLETEDATA:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Incomplete data received");
                        break;
                    case INVALIDMULTIPLEHOST:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "More than one host in SSDB has same information!");
                        break;
                    default:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Unknown Parameter error! %d", acterr);
                        break;
                }
                break;
            case NETERR:
                switch(acterr) {
                    case INVALIDHOST:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Invalid host specified.  This could mean that network routines didnot return host information for selected host after several retries");
                        break;
                    case CANTGETHOSTINFO:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Can't get host information from network routines");
                        break;
                    default:
                        nobytes+= snprintf(buffer+nobytes, buflen-nobytes,
                                           "Unknown Network routine error! %d", acterr);
                        break;
                }
                break;
            default:
                nobytes += snprintf(buffer+nobytes, buflen-nobytes,
                    "Unknown error %d", tmperr&0xffff);
                break;
        }
    }

    return(nobytes);
}
