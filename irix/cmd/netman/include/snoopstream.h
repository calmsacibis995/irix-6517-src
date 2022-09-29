#ifndef SNOOPSTREAM_H
#define SNOOPSTREAM_H
/*
 * Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 *	Snoopstream include file
 *
 *	$Revision: 1.8 $
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

#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <netinet/in.h>
#include "snoopd_rpc.h"

struct sockaddr_in;
struct sockaddr;
struct timeval;
struct expr;
struct exprsource;
struct exprerror;
struct snoopstats;

typedef struct snoopstream {
	CLIENT		*ss_client;
	XDR		ss_xdr;
	enum svctype	ss_service;
	int		ss_sock;
	struct timeval	ss_timeout;
	struct protocol	*ss_rawproto;
	char		*ss_server;
	struct in_addr	ss_srvaddr;
	struct rpc_err	ss_rpcerr;
} SnoopStream;

/*
 * Snoopstream operations raise exceptions upon error, then return a
 * failure code: negative for ss_compile and ss_add; false for the rest.
 * If ss_compile fails and the exprerror result parameter is non-zero,
 * it contains a syntax error report referring to the exprsource arg.
 *
 * Upon success, ss_compile and ss_add return a non-negative number.
 * The rest return true.
 */
int	ss_open(SnoopStream *, char *, struct sockaddr_in *, struct timeval *);
void	ss_close(SnoopStream *);
int	ss_subscribe(SnoopStream *, enum svctype, char *, u_int, u_int, u_int);
int	ss_unsubscribe(SnoopStream *);
int	ss_setsnooplen(SnoopStream *, u_int);
int	ss_seterrflags(SnoopStream *, u_int);
int	ss_compile(SnoopStream *, struct exprsource *, struct exprerror *);
int	ss_add(SnoopStream *, struct expr *, struct exprerror *);
int	ss_delete(SnoopStream *, int);
int	ss_start(SnoopStream *);
int	ss_stop(SnoopStream *);
int	ss_read(SnoopStream *, xdrproc_t, void *);
int	ss_getstats(SnoopStream *, struct snoopstats *);
int	ss_getaddr(SnoopStream *, int, struct sockaddr *);
int	ss_setinterval(SnoopStream *, u_int);

#endif /* !SNOOPSTREAM_H */
