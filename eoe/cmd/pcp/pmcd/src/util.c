/*
 * Copyright 1995, Silicon Graphics, Inc.
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

#ident "$Id: util.c,v 2.6 1998/11/15 08:35:24 kenmcd Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include "pmcd.h"

/* Based on Stevens (Unix Network Programming, p.83) */

void
StartDaemon()
{
    int childpid;

#if defined(HAVE_TERMIO_SIGNALS)
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
#endif

    if ((childpid = fork()) < 0)
	__pmNotifyErr(LOG_ERR, "StartDaemon: fork");
	/* but keep going */
    else if (childpid > 0) {
	/* parent, let her exit, but avoid ugly "Log finished" messages */
	fclose(stderr);
	exit(0);
    }

    /* not a process group leader, lose controlling tty */
    if (setsid() == -1)
	__pmNotifyErr(LOG_WARNING, "StartDaemon: setsid");
	/* but keep going */

    close(0);
    /* don't close other fd's -- we know that only good ones are open! */

    /* don't chdir("/") -- we still need to open pmcd.log */
}
