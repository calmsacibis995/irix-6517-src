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

#ident "$Id: agent.c,v 2.14 1998/11/15 08:35:24 kenmcd Exp $"

#include <stdio.h>
#include <syslog.h>
#include "pmcd.h"
#if defined(HAVE_DLFCN)
#include <dlfcn.h>
#elif defined(HAVE_SHL)
#include <dl.h>
#endif
#include <sys/wait.h>
#include <sys/resource.h>

/* Return a pointer to the agent that is reposible for a given domain.
 * Note that the agent may not be in a connected state!
 */
AgentInfo *
FindDomainAgent(int domain)
{
    int i;
    for (i = 0; i < nAgents; i++)
	if (agent[i].pmDomainId == domain)
	    return &agent[i];
    return NULL;
}

void
CleanupAgent(AgentInfo* aPtr, int why, int status)
{
    extern int	AgentDied;
    int		exit_status;
    int		reason = 0;

    if (aPtr->ipcType == AGENT_DSO) {
	if (aPtr->ipc.dso.dlHandle != NULL) {
#if defined(HAVE_DLFCN)
	    dlclose(aPtr->ipc.dso.dlHandle);
#elif defined(HAVE_SHL)
	    shl_unload((shl_t) aPtr->ipc.dso.dlHandle);
#else
!bozo!
#endif
	}
	pmcd_trace(TR_DEL_AGENT, aPtr->pmDomainId, -1, -1);
    }
    else {
	pmcd_trace(TR_DEL_AGENT, aPtr->pmDomainId, aPtr->inFd, aPtr->outFd);
	if (aPtr->inFd != -1) {
	    __pmNoMoreInput(aPtr->inFd);
	    __pmResetIPC(aPtr->inFd);
	    close(aPtr->inFd);
	    aPtr->inFd = -1;
	}
	if (aPtr->outFd != -1) {
	    __pmNoMoreInput(aPtr->outFd);
	    __pmResetIPC(aPtr->outFd);
	    close(aPtr->outFd);
	    aPtr->outFd = -1;
	}
	if (aPtr->ipcType == AGENT_SOCKET &&
	    aPtr->ipc.socket.addrDomain == AF_UNIX) {
	    /* remove the Unix domain socket */
	    unlink(aPtr->ipc.socket.name);
	}
    }

    __pmNotifyErr(LOG_INFO, "CleanupAgent ...\n");
    fprintf(stderr, "Cleanup \"%s\" agent (dom %d):", aPtr->pmDomainLabel, aPtr->pmDomainId);

    if (why == AT_CONFIG) {
	fprintf(stderr, " unconfigured");
    }
    else {
	if (why == AT_EXIT) {
	    fprintf(stderr, " terminated");
	    exit_status = status;
	    reason = (status << 8) | REASON_EXIT;
	}
	else {
	    reason = REASON_PROTOCOL;
	    fprintf(stderr, " protocol failure for fd=%d", status);
	    exit_status = -1;
	    if (aPtr->status.isChild == 1) {
		pid_t	pid = -1;
		pid_t	done;
		int	wait_status;
		if (aPtr->ipcType == AGENT_PIPE)
		    pid = aPtr->ipc.pipe.agentPid;
		else if (aPtr->ipcType == AGENT_SOCKET)
		    pid = aPtr->ipc.socket.agentPid;
		for ( ; ; ) {

#if defined(HAVE_WAIT3)
		    done = wait3(&wait_status, WNOHANG, NULL);
#elif defined(HAVE_WAITPID)
		    done = waitpid((pid_t)-1, &wait_status, WNOHANG);
#else
		    break;
#endif
		    if (done <= 0)
			break;
		    else if (done == pid)
			exit_status = wait_status;
		}
	    }
	}

	if (exit_status != -1) {
	    if (WIFEXITED(exit_status)) {
		fprintf(stderr, ", exit(%d)", WEXITSTATUS(exit_status));
		reason = (WEXITSTATUS(exit_status) << 8) | reason;
	    }
	    else if (WIFSIGNALED(exit_status)) {
		fprintf(stderr, ", signal(%d)", WTERMSIG(exit_status));
		if (WCOREDUMP(exit_status))
		    fprintf(stderr, ", dumped core");
		reason = (WTERMSIG(exit_status) << 16) | reason;
	    }
	}
    }
    fputc('\n', stderr);
    aPtr->reason = reason;
    aPtr->status.connected = 0;
    aPtr->status.busy = 0;
    aPtr->status.notReady = 0;
    AgentDied = 1;

    if (_pmcd_trace_mask)
	pmcd_dump_trace(stderr);

    MarkStateChanges(PMCD_DROP_AGENT);
}
