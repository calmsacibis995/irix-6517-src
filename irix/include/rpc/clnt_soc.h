/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef _NET_RPC_CLNT_SOC_H	/* wrapper symbol for kernel use */
#define _NET_RPC_CLNT_SOC_H	/* subject to change without notice */

#ident	"@(#)uts-comm:net/rpc/clnt_soc.h	1.3"

/*
 *  		PROPRIETARY NOTICE (Combined)
 *  
 *  This source code is unpublished proprietary information
 *  constituting, or derived under license from AT&T's Unix(r) System V.
 *  In addition, portions of such source code were derived from Berkeley
 *  4.3 BSD under license from the Regents of the University of
 *  California.
 *  
 *  
 *  
 *  		Copyright Notice 
 *  
 *  Notice of copyright on this source code product does not indicate 
 *  publication.
 *  
 *  	(c) 1986,1987,1988,1989  Sun Microsystems, Inc.
 *  	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 *  	          All rights reserved.
 */

/*
 * clnt.h - Client side remote procedure call interface.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */
/*
 * All the following declarations are only for backward compatibility
 * with SUNOS 4.0.
 */
#include <sys/socket.h>	/* COMPATIBILITY */
#include <netinet/in.h>	/* COMPATIBILITY */

#define UDPMSGSIZE	8800	/* rpc imposed limit on udp msg size */
struct sockaddr_in;

/*
 * enum clnt_stat
 * callrpc(host, prognum, versnum, procnum, inproc, in, outproc, out)
 *	char *host;
 *	u_long prognum, versnum, procnum;
 *	xdrproc_t inproc, outproc;
 *	char *in, *out;
 */
extern enum clnt_stat callrpc(const char *, u_long, u_long, u_long, 
	xdrproc_t, void *, xdrproc_t, void *);

/*
 * TCP based rpc
 * CLIENT *
 * clnttcp_create(raddr, prog, vers, fdp, sendsz, recvsz)
 *	struct sockaddr_in *raddr;
 *	u_long prog;
 *	u_long version;
 *	int *fdp;
 *	u_int sendsz;
 *	u_int recvsz;
 */
extern CLIENT *clnttcp_create(
	struct sockaddr_in *, u_long, u_long, int *, u_int, u_int);

/*
 * UDP based rpc.
 * CLIENT *
 * clntudp_create(raddr, program, version, wait, fdp)
 *	struct sockaddr_in *raddr;
 *	u_long program;
 *	u_long version;
 *	struct timeval wait;
 *	int *fdp;
 *
 * Same as above, but you specify max packet sizes.
 * CLIENT *
 * clntudp_bufcreate(raddr, program, version, wait, fdp, sendsz, recvsz)
 *	struct sockaddr_in *raddr;
 *	u_long program;
 *	u_long version;
 *	struct timeval wait;
 *	int *fdp;
 *	u_int sendsz;
 *	u_int recvsz;
 *
 */
extern CLIENT *clntudp_create(
	struct sockaddr_in *, u_long, u_long, struct timeval, int *);

extern CLIENT *clntudp_bufcreate(
	struct sockaddr_in *, u_long, u_long, struct timeval, int *,
	u_int, u_int);

/*
 * Memory based rpc (for speed check and testing)
 * CLIENT *
 * clntraw_create(prog, vers)
 *	u_long prog;
 *	u_long vers;
 */
extern CLIENT *clntraw_create(u_long, u_long);

#endif /* _NET_RPC_CLNT_SOC_H */

