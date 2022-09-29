/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.36 88/02/08 
 */


/* 
 * clnt_simple.c
 * Simplified front end to rpc.
 */

#ifdef __STDC__
	#pragma weak callrpc = _callrpc
#endif
#include "synonyms.h"

#include <stdio.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netdb.h>
#include <strings.h>
#include <bstring.h>
#include <unistd.h>
#include <pthread.h>
#include <mplib.h>

extern pthread_mutex_t callrpc_lock;

static CLIENT		*client;
static int		sock, valid;	/* avoid clash with socket(2) prototype */
static unsigned long	oldprognum, oldversnum;
static char		*oldhost;

enum clnt_stat
callrpc(
	const char *host,
	u_long prognum,
	u_long versnum,
	u_long procnum,
	xdrproc_t inproc,
	void *in,
	xdrproc_t outproc,
	void *out)
{
	struct sockaddr_in server_addr;
	enum clnt_stat clnt_stat;
	struct hostent *h, hp;
	struct timeval timeout, tottimeout;
	int  herr;
	char buf[4096];

	MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_LOCK, &callrpc_lock) );  \
	if (oldhost == NULL) {
		oldhost = malloc(256);
		oldhost[0] = 0;
		sock = RPC_ANYSOCK;
	}
	if (valid && oldprognum == prognum && oldversnum == versnum
		&& strcmp(oldhost, host) == 0) {
		/* reuse old client */		
	} else {
		valid = 0;
		(void)close(sock);
		sock = RPC_ANYSOCK;
		if (client) {
			clnt_destroy(client);
			client = NULL;
		}
		if ((h = gethostbyname_r(host, &hp, buf, sizeof(buf), &herr)) 
		    == NULL) {
			MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &callrpc_lock) );  \
			return (RPC_UNKNOWNHOST);
		}
		timeout.tv_usec = 0;
		timeout.tv_sec = 5;
		bcopy(h->h_addr, (char *)&server_addr.sin_addr, h->h_length);
		server_addr.sin_family = AF_INET;
		server_addr.sin_port =  0;
		if ((client = clntudp_create(&server_addr, (u_long)prognum,
		    (u_long)versnum, timeout, &sock)) == NULL) {
			MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &callrpc_lock) );  \
			return (rpc_createerr.cf_stat);
		}
		valid = 1;
		oldprognum = prognum;
		oldversnum = versnum;
		(void) strcpy(oldhost, host);
	}
	tottimeout.tv_sec = 25;
	tottimeout.tv_usec = 0;
	clnt_stat = clnt_call(client, procnum, inproc, in,
	    outproc, out, tottimeout);
	/* 
	 * if call failed, empty cache
	 */
	if (clnt_stat != RPC_SUCCESS)
		valid = 0;
	MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &callrpc_lock) );  \
	return (clnt_stat);
}
