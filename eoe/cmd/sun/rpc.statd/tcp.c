#ifndef lint
static char sccsid[] = "@(#)tcp.c	1.1 88/08/05 NFSSRC4.0 1.8 88/02/07 Copyr 1984 Sun Micro";
#endif

	/*
	 * Copyright (c) 1984 by Sun Microsystems, Inc.
	 */

	/*
	 * make tcp calls
	 */
#include <stdio.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <sys/time.h>

extern int debug;
/*
 *  routine taken from new_calltcp.c;
 *  no caching is done!
 *  continueously calling if timeout;
 *  in case of error, print put error msg; this msg usually is to be
 *  thrown away
 */
int
call_tcp(host, prognum, versnum, procnum, inproc, in, outproc, out, tot )
        char *host;
        xdrproc_t inproc, outproc;
        char *in, *out;
	int tot;
{
        struct sockaddr_in server_addr;
	struct in_addr *get_addr_cache();
        enum clnt_stat clnt_stat;
        struct hostent *hp;
        struct timeval  tottimeout;
        register CLIENT *client;
        int socket = RPC_ANYSOCK;
 
        if ((hp = gethostbyname(host)) == NULL) {
		if (debug)
			herror(host);
		return((int) RPC_UNKNOWNHOST);
	}
        bcopy(hp->h_addr, (caddr_t)&server_addr.sin_addr, hp->h_length);      
        server_addr.sin_family = AF_INET;
        server_addr.sin_port =  0;

        tottimeout.tv_usec = 0;
        tottimeout.tv_sec = tot; 
        if ((client = clnttcp_create(&server_addr, prognum,
            versnum,  &socket, 0, 0)) == NULL) { 
		if (debug)
			clnt_pcreateerror("clnttcp_create");
		return ((int) rpc_createerr.cf_stat);  
	}
again:
        clnt_stat = clnt_call(client, procnum, inproc, in,
            outproc, out, tottimeout);
	if (clnt_stat != RPC_SUCCESS)  {
		if( clnt_stat == RPC_TIMEDOUT) {
			if(tot != 0) {
				if(debug)
					printf("call_tcp timeout, retry\n");
				goto again;
			}
			/* if tot == 0, no reply is expected */
		}
		else {
			if(debug) {
				printf("clnt_call failed: %s\n",
					clnt_sperrno(clnt_stat));
			}
		}
	}
        /* should do cacheing, rather than always destroy */
        (void) close(socket);
        clnt_destroy(client);
        return (int) clnt_stat;
}

