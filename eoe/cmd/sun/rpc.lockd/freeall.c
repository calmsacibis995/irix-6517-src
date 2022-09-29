/* Copyright 1991 Silicon Graphics, Inc. All rights reserved. */

#if 0
static char sccsid[] = "@(#)prot_freeall.c	1.3 91/06/25 NFSSRC4.1 Copyr 1984 Sun Micro";
#endif

	/*
	 * Copyright (c) 1988 by Sun Microsystems, Inc.
	 */

	/*
	 * prot_freeall.c consists of subroutines that implement the
	 * DOS-compatible file sharing services for PC-NFS
	 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/file.h>
#include "prot_lock.h"

extern int debug;

void *
proc_nlm_freeall(Rqstp, Transp)
	struct svc_req *Rqstp;
	SVCXPRT *Transp;
{
	nlm_notify	req;

	req.name = NULL;
	if (!svc_getargs(Transp, xdr_nlm_notify, &req)) {
		svcerr_decode(Transp);
		return;
	}

	if (debug) {
		printf("proc_nlm_freeall from %s\n",
			req.name);
	}
	destroy_client_shares(req.name);
#if 0
	zap_all_locks_for(req.name);
#endif

	free(req.name);
	svc_sendreply(Transp, xdr_void, NULL);
}

#if 0
static void
zap_all_locks_for(client)
	char *client;
{
	if (debug)
		printf("zap_all_locks_for %s\n", client);

	/* there is nothing to zap on the client side 'cause only */
	/* monitored lock requests r saved by the client	  */
	/* so in the case of client crashes & recovers, all locks */
	/* in the server r cleared anyway.			  */
	if (debug)
		printf("DONE zap_all_locks_for %s\n", client);
}
#endif
