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

/*
 * Client-server control protocol support.
 */
#include "rtmond.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>

static	int inet_fd = -1;
static	int unix_fd = -1;
static	fd_set rselect;
static	fd_set eselect;
static	struct sockaddr_un sun;

/*
 * Initialize the client-server communication paths.  We
 * accept clients through inet- and unix-domain sockets.
 * The latter is used for local service and is preferred
 * by the client-side code (rtmond_open).
 */
void
init_network(int port, const char* sockname)
{
    struct sockaddr_in sin;

    FD_ZERO(&rselect);
    FD_ZERO(&eselect);

    inet_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (inet_fd < 0)
	Fatal(NULL, "Cannot create INET socket: %s", strerror(errno));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    if (port == 0) {
	struct servent* sp = getservbyname("rtmon", "tcp");
	sin.sin_port = htons(sp ? sp->s_port : RTMOND_DEFPORT);
    } else
	sin.sin_port = htons(port);
    if (bind(inet_fd, &sin, sizeof (sin)) < 0)
	Fatal(NULL, "bind(INET): %s", strerror(errno));
    if (listen(inet_fd, 10) < 0)
	Fatal(NULL, "listen(INET): %s", strerror(errno));
    FD_SET(inet_fd, &rselect);

    IFTRACE(DEBUG)(NULL, "Listen for INET connections at %s:%u",
	inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

    unix_fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (unix_fd < 0)
	Fatal(NULL, "Cannot create UNIX socket: %s", strerror(errno));
    sun.sun_family = AF_UNIX;
    strncpy(sun.sun_path,
	sockname ? sockname : RTMOND_UNIXSOCKET, sizeof (sun.sun_path));
    (void) unlink(sun.sun_path);
    if (bind(unix_fd, &sun, sizeof (sun)) < 0)
	Fatal(NULL, "bind(UNIX:%s): %s", sun.sun_path, strerror(errno));
    if (listen(unix_fd, 10) < 0)
	Fatal(NULL, "listen(UNIX): %s", strerror(errno));
    FD_SET(unix_fd, &rselect);

    IFTRACE(DEBUG)(NULL, "Listen for UNIX connections at %.*s",
	sizeof (sun.sun_path), sun.sun_path);
}

/*
 * Cleanup network-related state.
 */
void
network_cleanup(void)
{
    (void) close(inet_fd), inet_fd = -1;
    (void) unlink(sun.sun_path);
    (void) close(unix_fd), unix_fd = -1;
}

/*
 * Purge a file descriptor from the select state.
 */
static void
select_purge(int fd)
{
    assert(0 <= fd && fd < FD_SETSIZE);
    FD_CLR(fd, &rselect);
    FD_CLR(fd, &eselect);
}

/*
 * Deal with a client whose file descriptor
 * has apparently closed.
 */
static void
network_exception(int fd, client_common_t* com)
{
    IFTRACE(RPC)(NULL, "Client %s:%u, drop network connection, fd %d",
       com->host, com->port, fd);
    select_purge(fd);
    purge_client(com);
}

static const char*
cmdName(uint64_t cmd)
{
#define	N(a)	(sizeof (a) / sizeof (a[0]))
    static const char* cmdNames[] = {
	"#0",
	"\"open\"",
	"\"params\"",
	"\"resume\"",
	"\"suspend\"",
	"\"close\""
    };
    if (cmd >= N(cmdNames)) {
	static char buf[80];
	sprintf(buf, "#%lld", cmd);
	return (buf);
    } else
	return (cmdNames[cmd]);
#undef N
}

/*
 * Handle input from the specified client.
 *
 * XXX should use a state machine and non-blocking i/o
 *     or maybe (yech) FIONREAD.
 */
static void
network_input(int fd, client_common_t* com)
{
    uint64_t cmd;
    uint64_t err = 0;

    if (read(fd, &cmd, sizeof (cmd)) == sizeof (cmd)) {
	IFTRACE(RPC)(NULL, "Client %s:%u, RPC cmd %s",
	    com->host, com->port, cmdName(cmd));
	switch (cmd) {
	case RTMON_CMD_OPEN:
	    { uint64_t cookie;
	      if (read(fd, &cookie, sizeof (cookie)) == sizeof (cookie)) {
		  err = client_open(com, cookie);
		  if (err == 0) {
		      struct iovec iov[2];
		      rtmon_cmd_prologue_t pr;
		      iov[0].iov_base = &cmd;
		      iov[0].iov_len = sizeof (cmd);
		      pr.ncpus = htons(getncpu());
		      pr.maxindbytes = htons(com->maxindbytes);
		      pr.schedpri = htons(getschedpri());
		      iov[1].iov_base = &pr;
		      iov[1].iov_len = sizeof (pr);
		      IFTRACE(RPC)(NULL,
			  "Client %s:%u, RPC response: %s <%u,%u,%u>"
			  , com->host, com->port
			  , cmdName(cmd)
			  , ntohs(pr.ncpus)
			  , ntohs(pr.maxindbytes)
			  , ntohs(pr.schedpri)
			);
		      (void) writev(fd, iov, 2);
		      return;
		  }
	      } else
		  err = EFAULT;
	    }
	    break;
	case RTMON_CMD_CLOSE:
	    network_exception(fd, com);
	    return;				/* NB: no response */
	case RTMON_CMD_SUSPEND:
	    (void) client_suspend(com);
	    return;				/* XXX no response */
	case RTMON_CMD_RESUME:
	    err = client_resume(com);
	    if (err == 0 || err == (uint64_t) -1) {
		(void) write(fd, &cmd, sizeof (cmd));
		if (err == (uint64_t) -1)
		    client_start(com);
		return;
	    }
	    break;
	case RTMON_CMD_PARAMS:
	    { rtmon_cmd_params_t params;
	      if (read(fd, &params, sizeof (params)) == sizeof (params))
	          err = client_setparams(com, &params);
	      else
		  err = EFAULT;
	    }
	    break;
	default:
	    err = EINVAL;
	    break;
	}
	cmd |= (err<<32);
	IFTRACE(RPC)(NULL, "Client %s:%u, RPC response: cmd %s, errno %d",
	    com->host, com->port, cmdName(cmd), err);
	(void) write(fd, &cmd, sizeof (cmd));
    } else
	network_exception(fd, com);
}

/*
 * Accept a connection request on the specified socket.
 */
static void
network_accept(int s)
{
    struct sockaddr name;
    int namelen = sizeof (name);
    int fd = accept(s, &name, &namelen);

    if (fd >= 0) {
	if (name.sa_family == AF_INET) {
	    struct sockaddr_in* sin = (struct sockaddr_in*) &name;
	    IFTRACE(RPC)(NULL, "New INET connection, fd %d, %s:%u",
		fd, inet_ntoa(sin->sin_addr), sin->sin_port);
	} else {
	    struct sockaddr_un* sun = (struct sockaddr_un*) &name;
	    IFTRACE(RPC)(NULL, "New UNIX connection, fd %d, %s",
		fd, sun->sun_path);
	}
	assert(fd < FD_SETSIZE);
	if (client_create(fd, &name)) {
	    FD_SET(fd, &rselect);
	    FD_SET(fd, &eselect);
	} else
	    (void) close(fd);
    } else
	Log(LOG_ERR, NULL, "accept: %s", strerror(errno));
}

/*
 * Scan the file descriptor set and service active clients.
 */
static int
scan_clients(fd_set* set, void (*func)(int fd, client_common_t*))
{
    int i, fd;
    int nfound = 0;
    for (i = 0; i < __howmany(FD_SETSIZE, __NFDBITS); i++) {
	fd_mask_t bits = set->fds_bits[i];
	if (bits != 0) {
	    fd_mask_t m = 1;
	    fd = 0;
	    do {
		if (bits & m) {
		    client_common_t* com = find_client(i*__NFDBITS+fd);
		    if (com == NULL)		/* client gone already */
			select_purge(fd);
		    else			/* dispatch client */
			(*func)(i*__NFDBITS+fd, com);
		    bits &= ~m;
		    nfound++;
		}
		m <<= 1, fd++;
	    } while (bits != 0);
	}
    }
    return (nfound);
}

/*
 * Main input handling loop.
 */
void
network_run(void)
{

    for (;;) {
	fd_set rfd, efd;
	int nfd;

	rfd = rselect;
	efd = eselect;
	nfd = select(FD_SETSIZE, &rfd, NULL, &efd, NULL);
	if (nfd < 0)
	    Fatal(NULL, "select(network_run): %s", strerror(errno));
	if (FD_ISSET(inet_fd, &rfd))
	    network_accept(inet_fd), nfd--;
	if (FD_ISSET(unix_fd, &rfd))
	    network_accept(unix_fd), nfd--;
	if (nfd > 0)
	    nfd -= scan_clients(&rfd, network_input);
	if (nfd > 0)
	    nfd -= scan_clients(&efd, network_exception);
    }
}
