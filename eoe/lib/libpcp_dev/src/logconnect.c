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

#ident "$Id: logconnect.c,v 1.9 1997/10/24 06:32:09 markgw Exp $"

#include "pmapi_dev.h"
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <syslog.h>
#include <netdb.h>
#include <errno.h>
#include <limits.h>
#if defined(sgi)
#include <dlfcn.h>
#endif

/*
 * expect one of pid or port to be 0 ... if port is 0, use
 * hostname+pid to find port, assuming pmcd is running there
 */
int
__pmConnectLogger(const char *hostname, int *pid, int *port)
{
    int			n, sts;
    __pmLogPort		*lpp;
    struct sockaddr_in	myAddr;
    struct hostent*	servInfo;
    int			fd;	/* Fd for socket connection to pmcd */
    int			nodelay=1;
    struct linger	nolinger = {1, 0};
    __pmPDU		*pb;
    __pmIPC		ipc = { UNKNOWN_VERSION, NULL };
    __pmPDUHdr		*php;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "__pmConnectLogger(host=%s, pid=%d, port=%d)\n",
		hostname, *pid, *port);
#endif

    /*
     * catch pid == PM_LOG_ALL_PIDS ... this tells __pmLogFindPort
     * to get all ports
     */
    if (*pid == PM_LOG_ALL_PIDS) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "__pmConnectLogger: pid == PM_LOG_ALL_PIDS makes no sense here\n");
#endif
	return -ECONNREFUSED;
    }

    if (*pid == PM_LOG_NO_PID && *port == PM_LOG_PRIMARY_PORT) {
	/*
	 * __pmLogFindPort can only lookup based on pid, so xlate
	 * the request
	 */
	*pid = PM_LOG_PRIMARY_PID;
	*port = PM_LOG_NO_PORT;
    }

    if (*port == PM_LOG_NO_PORT) {
	if ((n = __pmLogFindPort(hostname, *pid, &lpp)) < 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "__pmConnectLogger: __pmLogFindPort: %s\n", pmErrStr(n));
#endif
	    return n;
	}
	else if (n != 1) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "__pmConnectLogger: __pmLogFindPort -> 1, cannot contact pmcd\n");
#endif
	    return -ECONNREFUSED;
	}
	*port = lpp->port;
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "__pmConnectLogger: __pmLogFindPort -> pid = %d\n", lpp->port);
#endif
    }

    if ((servInfo = gethostbyname(hostname)) == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "__pmConnectLogger: gethostbyname: %s\n",
		    hstrerror(h_errno));
#endif
	return -ECONNREFUSED;
    }

    /* Create socket and attempt to connect to the pmlogger control port */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
	return -errno;

    /* avoid 200 ms delay */
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,
		   (char *) &nodelay, sizeof(nodelay)) < 0) {
	sts = -errno;
	close(fd);
	return sts;
    }

    /* don't linger on close */
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER,
		   (char *) &nolinger, sizeof(nolinger)) < 0) {
	sts = -errno;
	close(fd);
	return sts;
    }

    memset(&myAddr, 0, sizeof(myAddr));	/* Arrgh! &myAddr, not myAddr */
    myAddr.sin_family = AF_INET;
    memcpy(&myAddr.sin_addr, servInfo->h_addr, servInfo->h_length);
    myAddr.sin_port = htons(*port);

    sts = connect(fd, (struct sockaddr*) &myAddr, sizeof(myAddr));
    if (sts < 0) {
	sts = -errno;
	close(fd);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "__pmConnectLogger: connect: %s\n", pmErrStr(sts));
#endif
	return sts;
    }

    /* Expect an error PDU back: ACK/NACK for connection */
    sts = __pmGetPDU(fd, PDU_BINARY, TIMEOUT_NEVER, &pb);
    if (sts == PDU_ERROR) {
	__pmOverrideLastFd(PDU_OVERRIDE2);	/* don't dink with the value */
	__pmDecodeError(pb, PDU_BINARY, &sts);
	if (sts == 0)
	    sts = LOG_PDU_VERSION1;
	else if (sts == PM_ERR_V1(PM_ERR_CONNLIMIT) ||
	         sts == PM_ERR_V1(PM_ERR_PERMISSION)) {
	    /*
	     * we do expect PM_ERR_CONNLIMIT and PM_ERR_PERMISSION as
	     * real responses, even from a PCP 1.x pmcd
	     */
	    sts = XLATE_ERR_1TO2(sts);
	}
	php = (__pmPDUHdr *)pb;
	if (*pid != PM_LOG_NO_PID && *pid != PM_LOG_PRIMARY_PID && php->from != *pid) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT)
		fprintf(stderr, "__pmConnectLogger: ACK response from pid %d, expected pid %d\n",
			    php->from, *pid);
#endif
	    sts = -ECONNREFUSED;
	}
	*pid = php->from;
    }
    else {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "__pmConnectLogger: ACK PDU type=%d?\n", sts);
#endif
	sts = PM_ERR_IPC;
    }

    if (sts >= LOG_PDU_VERSION1) {
	ipc.version = sts;
	sts = __pmAddIPC(fd, ipc);
	if (sts >= 0 && ipc.version >= LOG_PDU_VERSION2) {
	    __pmCred handshake[1];
	    handshake[0].c_type = CVERSION;
	    handshake[0].c_vala = LOG_PDU_VERSION;
	    handshake[0].c_valb = 0;
	    handshake[0].c_valc = 0;
	    sts = __pmSendCreds(fd, PDU_BINARY, 1, handshake);
	}
	if (sts >= 0) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "__pmConnectLogger: PDU version=%d fd=%d\n",
					ipc.version, fd);
#endif
	    return fd;
	}
    }
    /* error if we get here */
    __pmNoMoreInput(fd);
    close(fd);
    return sts;
}
