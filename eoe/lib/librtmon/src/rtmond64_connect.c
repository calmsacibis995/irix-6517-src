/**************************************************************************
 *                                                                        *
 *       Copyright (C) 1996, Silicon Graphics, Inc.                       *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * This set of API's is used to connect to the IRIX 6.4 rtmond.
 */

#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <rtmon.h>

#include <rpc/rpc.h>
#define _RPCGEN_CLNT
#include "rtmond64.h"

int
rtmond64_setup_control(rtmond64_t *rtmond64, const char *hostname,
		       int protocol)
    /*
     * Set up control connection to rtmond on specified host and return
     * control state in *rtmond64.  Data connections that are created with the
     * control connection will use the indicated protocol.  Return 0 on
     * success, -1 on failure.
     */
{
    struct sockaddr_in inaddr;
    int len, bsize;

    /*
     * Create a socket to accept() incoming rtmond connections on and grab
     * the port that gets assigned to it to tell the rtmond so it knows
     * where to connect to ...
     */
    inaddr.sin_family = AF_INET;
    inaddr.sin_addr.s_addr = INADDR_ANY;
    inaddr.sin_port = INADDR_ANY;
    len = sizeof inaddr;
    if ((rtmond64->socket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0
	|| bind(rtmond64->socket, &inaddr, sizeof inaddr) < 0
	|| listen(rtmond64->socket, 1) < 0
	|| getsockname(rtmond64->socket, &inaddr, &len) < 0) {
	if (rtmond64->socket > 0)
	    close(rtmond64->socket);
	return -1;
    }
    rtmond64->port = inaddr.sin_port;

    /*
     * Set large receive buffer size (but don't complain if we don't get it).
     * This large size will be inherited by all the connections that we
     * accept() off of this socket and decrease the likelyhood of losing
     * data.
     */
    bsize = 60*1024;
    setsockopt(rtmond64->socket, SOL_SOCKET, SO_RCVBUF, &bsize, sizeof bsize);

    /*
     * Create the RPC client state to send RPCs to the rtmond.  For the old
     * WindView protocol format for CPU event data, RTMON_PROTOCOL1, we use
     * the old WindView RPCs.  For all other protocol formats we use the new
     * SGI RPC.  We also allow a NULL or empty hostname to represent
     * "localhost" to match that aspect of the new IRIX 6.5 connection API.
     */
    if (hostname == NULL || *hostname == '\0')
	hostname = "localhost";
    rtmond64->client = protocol == RTMON_PROTOCOL1
	? clnt_create(hostname, RTMOND_SVC, RTMOND_WINDVIEW, "udp")
	: clnt_create(hostname, RTMOND_SVC, RTMOND_SGI1, "udp");
    if (rtmond64->client == NULL) {
	close(rtmond64->socket);
	clnt_pcreateerror("rtmond64_setup_control RPC");
	errno = EPROTO;
	return -1;
    }

    rtmond64->protocol = protocol;
    return 0;
}

void
rtmond64_teardown_control(rtmond64_t *rtmond64)
    /*
     * Tear down control connection to rtmond.
     */
{
    close(rtmond64->socket);
    clnt_destroy(rtmond64->client);
}

int
rtmond64_get_ncpu(rtmond64_t *rtmond64)
    /*
     * Return the number of CPUs on the rtmond host.  Return -1 on failure.
     */
{
    u_int *res;
    int ncpu;

    if (rtmond64->protocol == RTMON_PROTOCOL1) {
	fputs("rtmond64_get_ncpu RPC: not supported for RTMON_PROTOCOL1\n", stderr);
	errno = EPROTO;
	return -1;
    }
    res = rtmond64_get_ncpu_2(NULL, rtmond64->client);
    if (res == NULL) {
	clnt_perror(rtmond64->client, "rtmond64_get_ncpu RPC");
	errno = EPROTO;
	return -1;
    }
    ncpu = *res;
    clnt_freeres(rtmond64->client, xdr_u_int, res);
    return ncpu;
}

int
rtmond64_get_connection(rtmond64_t *rtmond64, int cpu, uint64_t eventmask)
    /*
     * Get a connection for the specified event data on the indicated CPU.
     * Return file descriptor of connection on success, -1 on failure.
     */
{
    u_int *res;
    int err;

    if (rtmond64->protocol == RTMON_PROTOCOL1) {
	evtLogRec logControl;
	logControl.state = 1;
	logControl.mode = cpu;
	logControl.portNo = rtmond64->port;
	res = wvproc_evt_log_control_1(&logControl, rtmond64->client);
    } else {
	rtmond64_get_connection_args connreq;
	connreq.protocol = rtmond64->protocol;
	connreq.cpu = cpu;
	connreq.mask = eventmask;
	connreq.port = rtmond64->port;
	res = rtmond64_get_connection_2(&connreq, rtmond64->client);
    }
    if (res == NULL) {
	clnt_perror(rtmond64->client, "rtmond64_get_connection RPC");
	errno = EPROTO;
	return -1;
    }
    err = *res;
    clnt_freeres(rtmond64->client, xdr_u_int, res);
    if (err != 0) {
	errno = err;
	return -1;
    }

    return accept(rtmond64->socket, NULL, 0);
}

int
rtmond64_get_CPU_connection(const char *hostname, int protocol,
			    int cpu, uint64_t eventmask)
    /*
     * Convenience routine to return a CPU event data connection to rtmond
     * on "hostname" for CPU "cpu" and set the event mask to "eventmask".
     * Returns file descriptor of connection on success, -1 on failure.
     */
{
    rtmond64_t rtmond64;
    int fd;

    if (rtmond64_setup_control(&rtmond64, hostname, protocol) < 0)
	return -1;
    fd = rtmond64_get_connection(&rtmond64, cpu, eventmask);
    rtmond64_teardown_control(&rtmond64);
    return fd;
}

int
rtmond64_suspend_connection(rtmond64_t *rtmond64, int cpu)
{
    u_int *res;
    int err;

    if (rtmond64->protocol == RTMON_PROTOCOL1) {
	evtLogRec logControl;
	logControl.state = 0;
	logControl.mode = cpu;
	logControl.portNo = rtmond64->port;
	res = wvproc_evt_log_control_1(&logControl, rtmond64->client);
    } else {
	rtmond64_suspend_resume_args suspend_args;
	suspend_args.cpu = cpu;
	suspend_args.port = rtmond64->port;
	res = rtmond64_suspend_connection_2(&suspend_args, rtmond64->client);
    }
    if (res == NULL) {
	clnt_perror(rtmond64->client, "rtmond64_suspend_connection RPC");
	errno = EPROTO;
	return -1;
    }
    err = *res;
    clnt_freeres(rtmond64->client, xdr_u_int, res);
    if (err != 0) {
	errno = err;
	return -1;
    }
    return 0;
}

int
rtmond64_resume_connection(rtmond64_t *rtmond64, int cpu)
{
    u_int *res;
    int err;

    if (rtmond64->protocol == RTMON_PROTOCOL1) {
	evtLogRec logControl;
	logControl.state = 1;
	logControl.mode = cpu;
	logControl.portNo = rtmond64->port;
	res = wvproc_evt_log_control_1(&logControl, rtmond64->client);
    } else {
	rtmond64_suspend_resume_args resume_args;
	resume_args.cpu = cpu;
	resume_args.port = rtmond64->port;
	res = rtmond64_resume_connection_2(&resume_args, rtmond64->client);
    }
    if (res == NULL) {
	clnt_perror(rtmond64->client, "rtmond64_resume_connection RPC");
	errno = EPROTO;
	return -1;
    }
    err = *res;
    clnt_freeres(rtmond64->client, xdr_u_int, res);
    if (err != 0) {
	errno = err;
	return -1;
    }
    return 0;
}
