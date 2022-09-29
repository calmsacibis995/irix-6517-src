#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
#include <errno.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "portmap.h"

extern nsd_file_t *__nsd_mounts;

/*
** This file contains code to communicate with a portmapper on a given host.
*/
typedef struct pmap {
	struct pmap		*next;
	int			(*proc)(nsd_file_t **, void *);
	uint32_t		*result;
	void			*arg;
	struct in_addr		addr;
	uint32_t		xid;
	uint32_t		state;
	uint32_t		program;
	uint32_t		version;
	uint32_t		protocol;
	uint32_t		port;
} pmap_t;
#define PREREGISTER	0
#define REGISTER	1
#define UNREGISTER	2
#define FETCH		3

typedef struct pmap_string {
	uint32_t	len;
	uint32_t	words;
	char		string[256];
} pmap_string_t;
static pmap_string_t pmap_host;

static int pmap_sock = -1;
static struct sockaddr_in pmap_sin;

static int portmap_send(pmap_t *);
extern int bindresvport(int, struct sockaddr_in *);

static pmap_t *pmap_list = 0;
static uint32_t pmap_xid;

static int pmap_euid, pmap_egid;

/*
** This just adds a structure to our list of outstanding requests.
*/
static void
xid_push(pmap_t *pd)
{
	pd->xid = pmap_xid;
	pd->next = pmap_list;
	pmap_list = pd;
}

/*
** This just removes and returns a structure from our list of outstanding
** requests, keyed by xid.
*/
static pmap_t *
xid_pop(int xid)
{
	pmap_t *pd, **last;

	for (last = &pmap_list, pd = pmap_list; pd && (pd->xid != xid);
	    last = &pd->next, pd = pd->next);
	if (pd) {
		*last = pd->next;
	}

	return pd;
}

/*
** Assuming that there are no outstanding requests, this just closes
** the portmap socket.
*/
void
nsd_portmap_shake(void)
{
	if (! pmap_list && (pmap_sock != -1)) {
		nsd_callback_remove(pmap_sock);
		close(pmap_sock);
		pmap_sock = -1;
	}
}

/*
** This is the callback function for the portmap socket.  We parse the
** response packet, then depending on what request we made we take the
** next step.  The register is a two part affair, first an unregister
** then a register.  The getport function calls a callback function
** after setting the result.
*/
/*ARGSUSED*/
static int
portmap_callback(nsd_file_t **rqp, int fd)
{
	pmap_t *pd;
	uint32_t *p;
	char pmap_buf[1024];
	int (*proc)(nsd_file_t **, void *);
	void *arg;

	nsd_logprintf(NSD_LOG_MIN, "Enterring portmap_callback:\n");

	if (recv(fd, pmap_buf, sizeof(pmap_buf), 0) < 0) {
		nsd_logprintf(NSD_LOG_OPER,
		    "portmap_callback: recv failed: %s\n",
		    strerror(errno));
		return NSD_CONTINUE;
	}

	p = (uint32_t *)pmap_buf;
	if (ntohl(p[1]) != REPLY) {
		nsd_logprintf(NSD_LOG_HIGH, "\tnot REPLY\n");
		return NSD_CONTINUE;
	}

	pd = xid_pop(ntohl(p[0]));
	if (! pd) {
		nsd_logprintf(NSD_LOG_HIGH,
		    "\tno pmap_t structure for xid: 0x%x\n",
		    ntohl(p[0]));
		return NSD_CONTINUE;
	}

	nsd_timeout_remove((nsd_file_t *)pd);

	if (ntohl(p[2]) != MSG_ACCEPTED) {
		free(pd);
		nsd_logprintf(NSD_LOG_HIGH, "\tnot MSG_ACCEPTED\n");
		return NSD_CONTINUE;
	}

	p += 5 + ntohl(p[4])/sizeof(uint32_t);

	if (ntohl(p[0]) != SUCCESS) {
		if (pd->state == REGISTER) {
			/*
			** If we failed register then we backup and do
			** unregister then try again.
			*/
			pd->state = PREREGISTER;
			nsd_logprintf(NSD_LOG_HIGH,
			    "\tregister failed, starting over\n");
			return portmap_send(pd);
		}
		free(pd);
		return NSD_CONTINUE;
	}

	if (pd->state == PREREGISTER) {
		pd->state = REGISTER;
		nsd_logprintf(NSD_LOG_HIGH,
		    "\tsuccessful unregister, doing register\n");
		return portmap_send(pd);
	}

	if (pd->state == FETCH) {
		if (pd->result) {
			*pd->result = ntohl(p[1]);
			nsd_logprintf(NSD_LOG_HIGH,
			    "\tfound port: %d\n", *pd->result);
		}
		if (pd->proc) {
			proc = pd->proc;
			arg = pd->arg;
			free(pd);
			return (*proc)(rqp, arg);
		}
	}

	free(pd);
	nsd_logprintf(NSD_LOG_HIGH, "\tsuccessful portmap request\n");
	return NSD_CONTINUE;
}

/*
** This is the timeout function for the portmapper.  Typically we just
** resend the request.
*/
/*ARGSUSED*/
static int
portmap_timeout(nsd_file_t **rqp, nsd_times_t *to)
{
	pmap_t *pd;

	nsd_logprintf(NSD_LOG_MIN, "Enterring portmap_timeout:\n");

	if (! to->t_file) {
		return NSD_ERROR;
	}
	pd = (pmap_t *)to->t_file;

	xid_pop(pd->xid);
	nsd_timeout_remove(to->t_file);

	return portmap_send(pd);
}

/*
** This is an initialization function for the portmap module.  It just
** sets up the global state, opens a new socket, and registers a callback
** for the new socket.
*/
static int
portmap_init(void)
{
	int len = sizeof(struct sockaddr_in);
	struct timeval t;

	nsd_logprintf(NSD_LOG_MIN, "Enterring nsd_portmap_init:\n");

	gettimeofday(&t);
	pmap_xid = getpid() ^ t.tv_sec ^ t.tv_usec;

	if (gethostname(pmap_host.string, sizeof(pmap_host.string)) < 0) {
		strcpy(pmap_host.string, "IRIS");
		pmap_host.len = 4;
		pmap_host.words = 1;
	} else {
		pmap_host.len = strlen(pmap_host.string);
		pmap_host.words = WORDS(pmap_host.len);
	}

	pmap_euid = geteuid();
	pmap_egid = getegid();

	if (pmap_sock < 0) {
		pmap_sin.sin_family = AF_INET;
		pmap_sin.sin_port = 0;
		pmap_sin.sin_addr.s_addr = INADDR_ANY;
		memset(pmap_sin.sin_zero, 0, sizeof(pmap_sin.sin_zero));

		pmap_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (pmap_sock < 0) {
			nsd_logprintf(NSD_LOG_OPER,
			    "portmap_init: failed socket: %s\n",
			    strerror(errno));
			return NSD_ERROR;
		}
		if (bindresvport(pmap_sock, &pmap_sin)) {
			pmap_sin.sin_port = 0;
			bind(pmap_sock, &pmap_sin, len);
		}
		if (getsockname(pmap_sock, &pmap_sin, &len) != 0) {
			close(pmap_sock);
			nsd_logprintf(NSD_LOG_OPER,
			    "portmap_init: failed getsockname: %s\n",
			    strerror(errno));
			return NSD_ERROR;
		}
	}

	nsd_callback_new(pmap_sock, portmap_callback, NSD_READ);

	return NSD_OK;
}

/*
** This is the send function for the portmap module.  It puts together
** a portmap request based on the data in the pmap_t structure and puts
** it on the wire.  It saves a pointer to the pmap_t structure so that
** when the response comes back we know what to do with it.
*/
static int
portmap_send(pmap_t *pd)
{
	char pmap_buf[1024];
	uint32_t *p, *save;
	struct timeval t;
	struct sockaddr_in sin;

	nsd_logprintf(NSD_LOG_MIN, "Enterring portmap_send:\n");

	if (! pd) {
		return NSD_ERROR;
	}

	if (pmap_sock < 0) {
		portmap_init();
		if (pmap_sock < 0) {
			return NSD_ERROR;
		}
	}

	p = (uint32_t *)pmap_buf;
	*p++ = htonl(++pmap_xid);
	*p++ = htonl(CALL);
	*p++ = htonl(RPC_MSG_VERSION);
	*p++ = htonl(PMAPPROG);
	*p++ = htonl(PMAPVERS);
	switch (pd->state) {
	    case REGISTER:
		*p++ = htonl(PMAPPROC_SET);
		break;
	    case PREREGISTER:
	    case UNREGISTER:
		*p++ = htonl(PMAPPROC_UNSET);
		break;
	    case FETCH:
		*p++ = htonl(PMAPPROC_GETPORT);
	}

	/* auth */
	*p++ = htonl(AUTH_UNIX);
	save = p++;
	gettimeofday(&t);
	*p++ = htonl(t.tv_sec);
	*p++ = htonl(pmap_host.len);
	p[pmap_host.len/sizeof(*p)] = 0;
	memcpy(p, pmap_host.string, pmap_host.len);
	p += pmap_host.words;
	*p++ = htonl(pmap_euid);
	*p++ = htonl(pmap_egid);
	*p++ = 0;
	*save = htonl((p - save - 1) * sizeof(uint32_t));
	
	/* verf */
	*p++ = 0;
	*p++ = 0;

	/* pmap req */
	*p++ = htonl(pd->program);
	*p++ = htonl(pd->version);
	*p++ = htonl(pd->protocol);
	*p++ = htonl(pd->port);
	
	sin.sin_addr = pd->addr;
	sin.sin_family = AF_INET;
	sin.sin_port = PMAPPORT;
	memset(&sin.sin_zero, 0, sizeof(sin.sin_zero));

	if (sendto(pmap_sock, pmap_buf, (char *)p - pmap_buf, 0, &sin,
	    sizeof(sin)) < 0) {
		nsd_logprintf(NSD_LOG_OPER, "portmap_send: failed sendto\n");
	}

	xid_push(pd);
	nsd_timeout_new((nsd_file_t *)pd, 500, portmap_timeout, 0);

	return NSD_CONTINUE;
}

/*
** This is the register function.  It just creates a new pmap_t structure
** with information about this new request and calls the send function
** to do the work.
*/
void
nsd_portmap_register(int program, int version, int protocol, int port)
{
	pmap_t *pd;

	nsd_logprintf(NSD_LOG_MIN, "Enterring nsd_portmap_register:\n");

	/*
	** The first trip through we want these to be syncronous so we
	** just call the old libc routines.  We can tell it is the first
	** trip through because the tree has not been setup yet.
	*/
	if (! __nsd_mounts) {
		pmap_set(program, version, protocol, port);
		return;
	}

	pd = (pmap_t *)nsd_calloc(1, sizeof(pmap_t));
	if (! pd) {
		nsd_logprintf(NSD_LOG_OPER,
		    "nsd_portmap_register: malloc failed\n");
		return;
	}
	pd->program = program;
	pd->version = version;
	pd->protocol = protocol;
	pd->port = port;
	pd->state = PREREGISTER;
	pd->addr.s_addr = INADDR_LOOPBACK;

	portmap_send(pd);
}

/*
** This is the unregister function.  It just creates a new pmap_t structure
** with information about this new request and calls the send function
** to do the work.
*/
void
nsd_portmap_unregister(int program, int version)
{
	pmap_t *pd;

	nsd_logprintf(NSD_LOG_MIN, "Enterring nsd_portmap_unregister:\n");

	/*
	** The first trip through we want these to be syncronous so we
	** just call the old libc routines.  We can tell it is the first
	** trip through because the tree has not been setup yet.
	*/
	if (! __nsd_mounts) {
		pmap_unset(program, version);
		return;
	}

	pd = (pmap_t *)nsd_calloc(1, sizeof(pmap_t));
	if (! pd) {
		nsd_logprintf(NSD_LOG_OPER,
		    "nsd_portmap_register: malloc failed\n");
		return;
	}
	pd->program = program;
	pd->version = version;
	pd->protocol = 0;
	pd->port = 0;
	pd->state = UNREGISTER;
	pd->addr.s_addr = INADDR_LOOPBACK;

	portmap_send(pd);
}

/*
** This is the getport function.  It just creates a new pmap_t structure
** with information about this new request and calls the send function
** to do the work.  When the request has completed we save the results to
** the result pointer, and call the passed in proc.
*/
int
nsd_portmap_getport(int program, int version, int protocol,
    struct in_addr *addr, int (*proc)(nsd_file_t **, void *), uint32_t *result,
    void *arg)
{
	pmap_t *pd;

	nsd_logprintf(NSD_LOG_MIN, "Enterring nsd_portmap_getport:\n");

	if (! proc || ! result) {
		return NSD_ERROR;
	}

	pd = (pmap_t *)nsd_calloc(1, sizeof(pmap_t));
	if (! pd) {
		nsd_logprintf(NSD_LOG_OPER,
		    "nsd_portmap_register: malloc failed\n");
		return NSD_ERROR;
	}
	pd->program = program;
	pd->version = version;
	pd->protocol = protocol;
	pd->state = FETCH;
	if (addr) {
		pd->addr = *addr;
	} else {
		pd->addr.s_addr = INADDR_LOOPBACK;
	}
	pd->proc = proc;
	pd->result = result;
	pd->arg = arg;

	return portmap_send(pd);
}
