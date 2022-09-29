/*
 * Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
 *
 *	event server (nveventd) main
 *
 *$Revision: 1.7 $
 */
 
/*
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
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
/* #include <getopt.h> */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "server.h"
#include "cfg.h"

EV_cfg *cfg;
EV_server server;

void done(int, ...);
static char usage[] = \
"usage: nveventd [ -d [input|output|foreground] ] \
[-F <logfilename>] [-n <numlogfiles>] [-s <logfilesize>] \n";
static int foreground = 0;

main(int argc, char **argv)
{
    // Parse options

    extern char *optarg;
    int c;
    
    while ((c = getopt(argc, argv, "d:F:n:s:")) != -1) {
	switch (c) {
	case 'd':
	    if (strncmp(optarg, "input", 2) == 0)
		server.setDumpIn(1);
	    else if (strncmp(optarg, "output", 3) == 0)
		server.setDumpOut(1);
	    else if (strncmp(optarg, "foreground", 3) != 0) {
		fputs(usage, stderr);
		exit(0);
	    }
	     foreground = 1;
	     server.setForeground(foreground);
	break;
	case 'F':
	    server.setLogFile(optarg);
	break;
	case 'n':
	    server.setNumLogs(atoi(optarg));
	break;
	case 's':
	    server.setLogSize(atoi(optarg));
	break;
	case '?':
	    fputs(usage, stderr);
	    exit(0);
	}
    }
    // Background unless debugging
    // COMMENTED OUT. WILL BE STARTED IN BACKGROUND BY TOOLTALK 

/*
    if (!foreground) {
    	if (_daemonize(foreground ? _DF_NOFORK|_DF_NOCHDIR|_DF_NOCLOSE : 0,
	    -1, -1, -1) < 0) {
	    perror("unable to become a daemon");
	    exit (errno);
     	}
    }
   
*/


    // Install a signal handler for common exits
    signal(SIGINT, done);
    signal(SIGTERM, done);

    // Run the server
    server.run();
    exit(0);
}

void
done(int sig, ...)
{
    // XXX - Shut down trap
    if (sig == SIGALRM) {
	syslog(LOG_DEBUG, "Exiting: no-client timer expired.");
    }
    else {
	syslog (LOG_DEBUG, "main caught signal %d: exiting", sig);
    }
    closelog();
    //XXXX should add a close event to the eventlog
    exit(0);
    
}
