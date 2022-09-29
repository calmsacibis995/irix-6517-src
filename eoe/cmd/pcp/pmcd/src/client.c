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

#ident "$Id: client.c,v 2.20 1999/08/17 04:13:41 kenmcd Exp $"

#include <stdio.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "client.h"
#include "pmcd.h"

#define MIN_CLIENTS_ALLOC 8

ClientInfo	*client = NULL;
int		nClients = 0;		/* Number in array, (not all in use) */
int		maxClientFd = -1;	/* largest fd for a client */
fd_set		clientFds;		/* for client select() */

static int	clientSize = 0;

extern int	errno;
extern void	Shutdown(void);

/* Establish a new socket connection to a client */
ClientInfo *
AcceptNewClient(int reqfd)
{
    int		i, fd, addrlen;
    __pmIPC	ipc = { UNKNOWN_VERSION, NULL };

    i = NewClient();
    addrlen = sizeof(client[i].addr);
    fd = accept(reqfd, (struct sockaddr *)&client[i].addr, &addrlen);
    if (fd == -1) {
	__pmNotifyErr(LOG_ERR, "AcceptNewClient(%d) accept: %s\n",
	    reqfd, strerror(errno));
	Shutdown();
	exit(1);
    }
    if (fd > maxClientFd)
	maxClientFd = fd;

    PMCD_OPENFDS_SETHI(fd);

    FD_SET(fd, &clientFds);
    __pmAddIPC(fd, ipc);	/* unknown version before negotiation */
    client[i].fd = fd;
    client[i].status.connected = 1;
    client[i].status.changes = 0;
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "AcceptNewClient(%d): client[%d] (fd %d)\n", reqfd, i, fd);
#endif
    pmcd_trace(TR_ADD_CLIENT, client[i].addr.sin_addr.s_addr, fd, 0);

    return &client[i];
}

int
NewClient(void)
{
    int i;

    for (i = 0; i < nClients; i++)
	if (!client[i].status.connected)
	    break;

    if (i == clientSize) {
	int	j;
	clientSize = clientSize ? clientSize * 2 : MIN_CLIENTS_ALLOC;
	client = (ClientInfo*)
	    realloc(client, sizeof(ClientInfo) * clientSize);
	if (client == NULL) {
	    __pmNoMem("NewClient", sizeof(ClientInfo) * clientSize, PM_RECOV_ERR);
	    Shutdown();
	    exit(1);
	}
	for (j = i; j < clientSize; j++) {
	    client[j].profile = NULL;
	    client[j].szProfile = 0;
	}
    }
    if (i >= nClients)
	nClients = i + 1;
    return i;
}

void
DeleteClient(ClientInfo *cp)
{
    int		i;

    for (i = 0; i < nClients; i++)
	if (cp == &client[i])
	    break;

    if (i == nClients) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    __pmNotifyErr(LOG_ERR, "DeleteClient: tried to delete non-existent client\n");
	    Shutdown();
	    exit(1);
	}
#endif
	return;
    }
    if (cp->fd != -1) {
	__pmNoMoreInput(cp->fd);
	__pmResetIPC(cp->fd);
	FD_CLR(cp->fd, &clientFds);
	close(cp->fd);
    }
    if (i == nClients-1) {
	i--;
	while (i >= 0 && !client[i].status.connected)
	    i--;
	nClients = (i >= 0) ? i + 1 : 0;
    }
    if (cp->fd == maxClientFd) {
	maxClientFd = -1;
	for (i = 0; i < nClients; i++) {
	    if (client[i].fd > maxClientFd)
		maxClientFd = client[i].fd;
	}
    }
    for (i = 0; i < cp->szProfile; i++) {
	if (cp->profile[i] != NULL) {
	    __pmFreeProfile(cp->profile[i]);
	    cp->profile[i] = NULL;
	}
    }
    cp->status.connected = 0;
    cp->fd = -1;
}

#ifdef PCP_DEBUG
/* Convert a client file descriptor to a string describing what it is for. */
char *
nameclient(int fd)
{
#define FDNAMELEN 30
    static char fdStr[FDNAMELEN];
    static char *stdFds[4] = {"*UNKNOWN FD*", "stdin", "stdout", "stderr"};
    int		i;

    for (i = 0; i < nClients; i++)
	if (client[i].status.connected && fd == client[i].fd) {
	    sprintf(fdStr, "client[%d] input socket", i);
	    return fdStr;
	}
    if (fd >= -1 && fd < 3)
	return stdFds[fd + 1];
    return stdFds[0];
}
#endif

void
ShowClients(FILE *f)
{
    int			i;
    struct hostent	*hp;
    __pmIPC		*ipcptr;

    fprintf(f, "     fd  client connection from                    ipc ver  operations denied\n");
    fprintf(f, "     ==  ========================================  =======  =================\n");
    for (i = 0; i < nClients; i++) {
	if (client[i].status.connected == 0)
	    continue;

	fprintf(f, "    %3d  ", client[i].fd);

	hp = gethostbyaddr((void *)&client[i].addr.sin_addr.s_addr, sizeof(client[i].addr.sin_addr.s_addr), AF_INET);
	if (hp == NULL) {
	    char	*p = (char *)&client[i].addr.sin_addr.s_addr;
	    int	k;

	    for (k = 0; k < 4; k++) {
		if (k > 0)
		    fputc('.', f);
		fprintf(f, "%d", p[k] & 0xff);
	    }
	}
	else
	    fprintf(f, "%-40.40s", hp->h_name);

	__pmFdLookupIPC(client[i].fd, &ipcptr);
	fprintf(f, "  %7d", ipcptr ? ipcptr->version : -1);

	if (client[i].denyOps != 0) {
	    fprintf(f, "  ");
	    if (client[i].denyOps & OP_FETCH)
		fprintf(f, "fetch ");
	    if (client[i].denyOps & OP_STORE)
		fprintf(f, "store ");
	}

	fputc('\n', f);
    }
    fputc('\n', f);
}

void
MarkStateChanges(int changes)
{
    int			i;

    for (i = 0; i < nClients; i++) {
	if (client[i].status.connected == 0)
	    continue;
	client[i].status.changes |= changes;
    }
}
