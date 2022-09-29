/**************************************************************************
 *                                                                        *
 *       Copyright (C) 1997, Silicon Graphics, Inc.                       *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#define	__RTMON_PROTO__
/*
 * rtmond client-server protocol, client side.
 */
#include "rtmon.h"

#include <netdb.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <sys/time.h>

static	int rtmond_cmd(rtmond_t* rt, uint64_t cmd, size_t len, void* param);

/*
 * Try to connect to the server using a UNIX domain
 * socket.  If hostname is NULL or an empty string
 * then the default socket name is used.
 */
static int
tryunixdomain(rtmond_t* rt, const char* hostname)
{
    struct sockaddr_un sun;

    rt->socket = socket(PF_UNIX, SOCK_STREAM, 0);
    if (rt->socket < 0)
	return (0);
    sun.sun_family = AF_UNIX;
    strcpy(sun.sun_path,
	hostname && hostname[0] != '\0' ? hostname : RTMOND_UNIXSOCKET);
    if (connect(rt->socket, &sun, sizeof (sun)) < 0) {
	(void) close(rt->socket), rt->socket = -1;
	return (0);
    } else
	return (1);
}

/*
 * Construct magic cookie that uniquely identifies our
 * connection.  This token is passed to the server who
 * uses it to dispatch event data that should go to a
 * specific connection and by the system to mark the
 * "owner" of sensitive events such as those containing
 * system call parameters.  We really need 128 bits for
 * this value but only have 64 right now so use a combination
 * of the PID and time to construct a value that is
 * unlikely to be guessed or duplicated (the latter can
 * easily happen to apps that make several successive
 * calls to establish per-CPU streams--such as kernprof).
 */
static uint64_t
make_cookie(void)
{
    struct timeval tv;
    (void) gettimeofday(&tv, NULL);
    return (
	  (((uint64_t) getpid())<<32)
	| (((uint64_t) tv.tv_sec)<<8)
	| (((uint64_t) tv.tv_usec)>>(20-8)) /* top ~8 bits of [0, 1e6) */
    );
}

/*
 * Open a connection to the rtmond process on the specified
 * host at the specified port (meaningful only when the host
 * is specified as an Internet hostname or address).  We
 * try first to use a local connection w/ a UNIX domain socket
 * and fallback to a TCP socket only on failure.  Note also
 * that if a TCP socket is used and the hostname is NULL or
 * an empty string then we use the loopback interface directly
 * (i.e. w/o using the name service to lookup the address).
 *
 * If the returned handle is not null then it contains server
 * state information that a client can interrogate (e.g. the
 * number of CPUs).
 */
rtmond_t*
rtmond_open(const char* hostname, int port)
{
    rtmond_t* rt;
    rtmon_cmd_prologue_t pr;

    rt = (rtmond_t*) malloc(sizeof (*rt));
    if (rt == NULL)
	return (rt);
    rt->socket = -1;
    rt->protocol = RTMON_PROTOCOL2;
    rt->ncpus = 0;
    rt->maxindbytes = 0;
    rt->schedpri = 0;
    rt->cookie = (uint64_t) -1;
    rt->cpus[0] = rt->cpus[1] = 0;

    if (!tryunixdomain(rt, hostname)) {
	struct sockaddr_in sin;

	rt->socket = socket(PF_INET, SOCK_STREAM, 0);
	if (rt->socket < 0)
	    goto bad;
	sin.sin_family = AF_INET;
	if (hostname && hostname[0] != '\0') {
	    struct hostent* hp = gethostbyname(hostname);
	    if (hp)
		memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
	    else
		sin.sin_addr.s_addr = inet_addr(hostname);
	} else
	    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	if (port == 0) {
	    struct servent* sp = getservbyname("rtmon", "tcp");
	    if (sp)
		sin.sin_port = htons(sp->s_port);
	    else
		sin.sin_port = htons(RTMOND_DEFPORT);
	} else
	    sin.sin_port = htons(port);
	if (connect(rt->socket, &sin, sizeof (sin)) < 0) {
	    (void) close(rt->socket), rt->socket = -1;
	    goto bad;
	}
    }
    /*
     * Set large receive buffer size if possible.
     */
    { int bs = 128*1024;
      (void) setsockopt(rt->socket, SOL_SOCKET, SO_RCVBUF, &bs, sizeof (bs)); }
    rt->cookie = make_cookie();
    if (rtmond_cmd(rt, RTMON_CMD_OPEN, sizeof (rt->cookie), &rt->cookie) == 0 &&
	read(rt->socket, &pr, sizeof (pr)) == sizeof (pr)) {
	/*
	 * Read server configuration information that follows.
	 */
	rt->ncpus = ntohs(pr.ncpus);
	rt->maxindbytes = ntohs(pr.maxindbytes);
	rt->schedpri = ntohs(pr.schedpri);
	return (rt);
    }
bad:
    rtmond_close(rt);
    return (NULL);
}

/*
 * Shutdown a server connection and reclaim resources.
 */
void
rtmond_close(rtmond_t* rt)
{
    if (rt->socket >= 0) {
	(void) rtmond_cmd(rt, RTMON_CMD_CLOSE, 0, NULL);
	(void) close(rt->socket);
    }
    free(rt);
}

/*
 * Send the current state/parameters to the server.
 * This is normally done as part of starting up event
 * collection but may also be done separately by a
 * client after suspending a connection (I think?).
 */
int
rtmond_setparams(rtmond_t* rt)
{
    rtmon_cmd_params_t params;

    params.protocol = rt->protocol;
    params.maxindbytes = rt->maxindbytes;
    params.ncpus = RTMOND_MAXCPU;
    params.events = rt->mask;
    memcpy(params.cpus, rt->cpus, sizeof (params.cpus));
    return (rtmond_cmd(rt, RTMON_CMD_PARAMS, sizeof (params), &params));
}

/*
 * Start event collection on one or more CPUs and return
 * the (uncollated) event data on the server connection.
 * Returns zero if successful, otherwse -1.
 */
int
rtmond_start_ncpu(rtmond_t* rt, uint ncpu, const uint64_t cpumask[], uint64_t evmask)
{
    int i;

    if (ncpu > RTMOND_MAXCPU) {
	errno = EINVAL;
	return (-1);
    }
    rt->mask = evmask;
    for (i = 0; i < RTMOND_MAXCPU; i += 64)
	rt->cpus[i>>6] = (ncpu > i ? cpumask[i>>6] : 0);
    return (rtmond_setparams(rt) < 0 ? -1 : rtmond_resume_data(rt));
}

/*
 * As above, but start collection on one CPU only
 * (likewise, CPUs are numbered starting with zero).
 */
int
rtmond_start_cpu(rtmond_t* rt, uint cpu, uint64_t evmask)
{
    if (cpu > RTMOND_MAXCPU) {
	errno = EINVAL;
	return (-1);
    }
    rt->mask = evmask;
    memset(rt->cpus, 0, sizeof (rt->cpus));
    rt->cpus[cpu/64] |= 1LL<<(cpu%64);
    return (rtmond_setparams(rt) < 0 ? -1 : rtmond_resume_data(rt));
}

/*
 * Suspend event data transfer.  Note that this does
 * not return an acknowledgement from the server; the
 * client must decide when event data has stopped flowing.
 */
int
rtmond_suspend_data(rtmond_t* rt)
{
    return (rtmond_cmd(rt, RTMON_CMD_SUSPEND, 0, NULL));
}

/*
 * Resume event data transfer.
 */
int
rtmond_resume_data(rtmond_t* rt)
{
    return (rtmond_cmd(rt, RTMON_CMD_RESUME, 0, NULL));
}

static int
rtmond_cmd(rtmond_t* rt, uint64_t cmd, size_t len, void* param)
{
    uint64_t resp;
    size_t cc;

    if (len > 0) {
	struct iovec iov[2];
	iov[0].iov_base = &cmd;
	iov[0].iov_len = sizeof (cmd);
	iov[1].iov_base = param;
	iov[1].iov_len = len;
	cc = writev(rt->socket, iov, 2);
    } else
	cc = write(rt->socket, &cmd, sizeof (cmd));
    if (cc != sizeof (cmd)+len)			/* cmd send error */
	return (-1);
    switch (cmd) {
    case RTMON_CMD_OPEN:			/* wait for response */
    case RTMON_CMD_PARAMS:
    case RTMON_CMD_RESUME:
	if (read(rt->socket, &resp, sizeof (resp)) != sizeof (resp))
	    return (-1);
	if (resp != cmd) {
	    errno = (int)(resp >> 32);		/* decode error number */
	    return (-1);
	}
	break;
    }
    return (0);
}
