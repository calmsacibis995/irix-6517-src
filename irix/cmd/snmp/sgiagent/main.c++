/*
 * Copyright 1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	SNMP Agent main
 *
 *	$Revision: 1.3 $
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

// Use to get syslog code names
#define SYSLOG_NAMES

#include <sys/types.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "oid.h"
#include "asn1.h"
#include "snmp.h"
#include "pdu.h"
#include "packet.h"
#include "subagent.h"
#include "sat.h"
#include "agent.h"
#include "snmpsa.h"
#include "systemsa.h"

#ifdef HPUX_MIB
// The MALLOC_AUDIT_KEY entry causes PCP memory audit to be turned on.
extern "C" {
#include "pmapi.h"

#ifdef PCP_DEBUG
MALLOC_AUDIT_KEY
#endif

}
#include "hpsystemsa.h"
#include "hpfssa.h"
#include "hptrapdestsa.h"
#include "hpprocsa.h"
#include "hpsnmpdconfsa.h"
#include "hpicmpsa.h"
#include "hpauthfailsa.h"
#endif

#ifdef MIB2
#include "ifsa.h"
#include "atsa.h"
#include "ipsa.h"
#include "icmpsa.h"
#include "tcpsa.h"
#include "udpsa.h"
#include "knlist.h"
#endif

#include "sgisa.h"
#if _MIPS_SZLONG == 64
#else

#ifdef RMON
#include "rmonsa.h"
#endif

#endif
#include "traphandler.h"

extern snmpTrapHandler *traph;
unsigned int udpTrapPort=162;

extern "C" {
#include "exception.h"
}; 


snmpAgent agent;

int rsocket;

static void done(int);

#ifdef MIB2
static char usage[] =
"usage: snmpd [ -d debug ] [ -l loglevel ] [ -u sysname ] [-p alternatePort] \
        [-t trapPort]\n\
       where:   debug    = dump, input, output, foreground\n\
                loglevel = DEBUG, INFO, NOTICE, WARNING,\n\
                           ERR, CRIT, ALERT, EMERG\n\
		sysname  = name of unix image to read /dev/kmem structures\n\
	        alternatePort = alternate UDP port number to listen for SNMP\
                                 requests \n\
		trapPort = alternate UDP port to send SNMP traps\n";
#else

#ifdef HPUX_MIB
static char usage[] =
"usage: hpsnmpd [ -d debug ] [ -l loglevel ] [ -p port | service ]\n\
       where:   port     = port agent will attach to\n\
                service  = service agent will attach to\n\
                debug    = dump, input, output, foreground\n\
                loglevel = DEBUG, INFO, NOTICE, WARNING,\n\
                           ERR, CRIT, ALERT, EMERG\n";
#else
static char usage[] =
"usage: snmpd [ -d debug ] [ -l loglevel ] \n\
       where:   debug    = dump, input, output, foreground\n\
                loglevel = DEBUG, INFO, NOTICE, WARNING,\n\
                           ERR, CRIT, ALERT, EMERG\n";
#endif
#endif

static int foreground = 0;

void
main(int argc, char **argv)
{
    // Parse options
    extern char *optarg;
    extern int exc_autofail;
    int c;
    int p;	// SNMP port
    char *sysname = "/unix";

    while ((c = getopt(argc, argv, "d:l:p:u:t:")) != -1) {
	switch (c) {
	    case 'd':
		if (strcmp(optarg, "dump") == 0)
		    agent.setDumpPacket(1);
		else if (strcmp(optarg, "input") == 0)
		    agent.setDumpRequest(1);
		else if (strcmp(optarg, "output") == 0)
		    agent.setDumpResponse(1);
		else if (strcmp(optarg, "foreground") != 0) {
		    fputs(usage, stderr);
		    exit(0);
		}
		foreground = 1;
		agent.setForeground(foreground);
		break;
	    case 'l':
		for (c = 0; prioritynames[c].c_name != 0; c++) {
		    if (strcasecmp(prioritynames[c].c_name, optarg) == 0) {
			exc_level = prioritynames[c].c_val;
			break;
		    }
		}
		if (prioritynames[c].c_name == 0) {
		    fputs(usage, stderr);
		    exit(0);
		}
		break;
	    case 'p':
		if ((p = atoi(optarg)) != 0)
		    {
		    agent.setPort(p);
		    }
		else
		    {
		    agent.setService(optarg);
		    }
		break;
	    case 'u':
		sysname = optarg;
		break;
	    case 't' :
		if ((udpTrapPort= atoi(optarg)) <= 0) {
		    udpTrapPort= 162;
		}
		break;
	    case '?':
		fputs(usage, stderr);
		exit(0);
	}
    }

    // Background unless debugging
    exc_autofail = 0;
    if (!foreground) {
        if (_daemonize(foreground ? _DF_NOFORK|_DF_NOCHDIR|_DF_NOCLOSE : 0,
		       -1, -1, -1) < 0) {
	    perror("unable to become a daemon");
	}
#ifdef MIB2
	exc_openlog("snmpd", LOG_PID, LOG_DAEMON);
#endif
#ifdef HPUX_MIB
	exc_openlog("hpsnmpd", LOG_PID, LOG_DAEMON);
#endif
    } else
	exc_progname = argv[0];
    exc_errlog(LOG_NOTICE, 0, "starting");

    // Initialize kernel name list

#ifdef MIB2
    init_nlist(sysname);
#endif
    
    // create the trap handler. This should be done before the agent since
    // the agent constructor path may generate a trap but the log isn't open yet.
    
    snmpTrapHandler traphandler;
    traphandler.setTrapPort(udpTrapPort);
    
    // Create sub-agents
    // The MIB2 system and snmp groups and the sgiHW MIB groups are 
    // supported in every agent.
    snmpSubAgent snmpsa;
    sgiHWSubAgent sgihwsa; //Must come before systemsa for trap handling!
    systemSubAgent systemsa;

#ifdef MIB2
    ifSubAgent ifsa;
    atSubAgent atsa;
    ipSubAgent ipsa;
    icmpSubAgent icmpsa;
    tcpSubAgent tcpsa;
    udpSubAgent udpsa;
#endif

    sgiNVSubAgent sginvsa;
#if _MIPS_SZLONG == 64
#else

#ifdef RMON
    rmonSubAgent rmonsa;
#endif

#ifdef HPUX_MIB
    int sts;
    if ((sts = pmLoadNameSpace(PM_NS_DEFAULT)) < 0) {
        exc_errlog(LOG_ERR, 0,
            "main: pmLoadNameSpace: %s\n", pmErrStr(sts));
        return;
    }
    if ((sts = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
        exc_errlog(LOG_ERR, 0,
            "main: pmNewContext: %s\n", pmErrStr(sts));
        return;
    }

    hpSystemSubAgent 	hpsystemsa;
    hpFsSubAgent 	hpfssa;
    hpTrapDestSubAgent 	hptrapdestsa;
    hpProcSubAgent 	hpprocsa;
    hpSnmpdConfSubAgent hpsnmpdconfsa;
    hpIcmpSubAgent 	hpicmpsa;
    hpAuthFailSubAgent 	hpauthfailsa;
#endif

#ifdef sgi
    rsocket = socket(AF_ROUTE, SOCK_RAW, 0);
    if (rsocket < 0) {
	syslog(LOG_ERR, "routing socket: %m");
	fprintf(stderr, "routed: routing socket: %s\n", strerror(errno));
	exit(1);
    }
#endif

#endif

    // Install a signal handler for common exits
    signal(SIGINT, (SIG_PF)done);
    signal(SIGTERM, (SIG_PF)done);

    // Run the agent
    agent.run();
    exit(0);
}

static void
done(int)
{
#ifdef MASTER
    // XXX - Shut down trap
    traph->send(SNMP_TRAP_ENTERPRISE);	// Specific trap id = 0
#endif
    exc_errlog(LOG_WARNING, 0, "exiting");
    if (!foreground)
	closelog();
    exit(0);
}
