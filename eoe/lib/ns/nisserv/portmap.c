#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_nisserv.h"

typedef struct pmapdata {
	int			program;
	int			version;
	int			protocol;
	int			port;
	int			state;
	nisserv_t		*nisdata;
} pmapdata_t;

static int portmap_send(nsd_file_t *);

extern int _nisserv_udp;
extern uint32_t _nisserv_euid, _nisserv_egid;
extern nisstring_t _nisserv_host;
extern char _nisserv_buf[];

/*
** This routine handles the case when a portmap request fails.  It will
** simply resend the request and refresh the timeout.
*/
static int
portmap_timeout(nsd_file_t **rqp, nsd_times_t *to)
{
	nsd_logprintf(NSD_LOG_MIN, "entering portmap_timeout:\n");

	*rqp = to->t_file;
	if (! to->t_file) {
		return NSD_ERROR;
	}

	nsd_timeout_remove(to->t_file);

	return portmap_send(to->t_file);
}

/*
** This routine handles the results that come back from a call to the
** portmapper.  It will either resend the request, return an error, or
** progress to the next state and send the next request.
*/
static int
portmap_callback(nsd_file_t *rq)
{
	nisserv_t *nd;
	pmapdata_t *pd;
	uint32_t *p;

	nsd_logprintf(NSD_LOG_MIN, "entering portmap_callback:\n");

	nsd_timeout_remove(rq);

	nd = (nisserv_t *)rq->f_sender;
	if (! nd) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_ERROR 1\n");
		return NSD_ERROR;
	}

	pd = (pmapdata_t *)rq->f_cmd_data;
	if (! pd) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_ERROR 2\n");
		nisserv_data_clear((nisserv_t **)&rq->f_sender);
		return NSD_ERROR;
	}

	/*
	** The data is in _nisserv_buf.
	*/
	p = (uint32_t *)_nisserv_buf;
	if (ntohl(p[2]) != MSG_ACCEPTED) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_ERROR 3\n");
		free(rq->f_cmd_data), rq->f_cmd_data = (void *)0;
		nisserv_data_clear((nisserv_t **)&rq->f_sender);
		return NSD_ERROR;
	}

	p += 5 + ntohl(p[4])/sizeof(uint32_t);
	if (ntohl(p[0]) != SUCCESS) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_ERROR 4\n");
		free(rq->f_cmd_data), rq->f_cmd_data = (void *)0;
		nisserv_data_clear((nisserv_t **)&rq->f_sender);
		return NSD_ERROR;
	}

	if (! pd->state) {
		pd->state++;
		return portmap_send(rq);
	}

	nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_OK\n");
	free(rq->f_cmd_data), rq->f_cmd_data = (void *)0;
	nisserv_data_clear((nisserv_t **)&rq->f_sender);
	return NSD_OK;
}

/*
** This sends a portmap unregister/register request to the portmapper.
*/
static int
portmap_send(nsd_file_t *rq)
{
	nisserv_t *nd;
	pmapdata_t *pd;
	uint32_t *p, *save;
	struct timeval t;
	struct sockaddr_in sin;
	int len;

	nsd_logprintf(NSD_LOG_MIN, "entering portmap_send:\n");

	nd = (nisserv_t *)rq->f_sender;
	if (! nd) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_ERROR\n");
		return NSD_ERROR;
	}

	pd = (pmapdata_t *)rq->f_cmd_data;
	if (! pd) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_ERROR 2\n");
		return NSD_ERROR;
	}

	p = (uint32_t *)_nisserv_buf;
	*p++ = htonl(nd->xid);
	*p++ = htonl(CALL);
	*p++ = htonl(RPC_MSG_VERSION);
	*p++ = htonl(PMAPPROG);
	*p++ = htonl(PMAPVERS);
	if (pd->state) {
		*p++ = htonl(PMAPPROC_SET);
	} else {
		*p++ = htonl(PMAPPROC_UNSET);
	}

	/* auth */
	*p++ = htonl(AUTH_UNIX);
	save = p++;
	gettimeofday(&t);
	*p++ = htonl(t.tv_sec);
	*p++ = htonl(_nisserv_host.len);
	memcpy(p, _nisserv_host.string, _nisserv_host.len);
	p += _nisserv_host.words;
	*p++ = htonl(_nisserv_euid);
	*p++ = htonl(_nisserv_egid);
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

	len = (char *)p - _nisserv_buf;

	sin.sin_addr.s_addr = INADDR_ANY;
	sin.sin_family = AF_INET;
	sin.sin_port = PMAPPORT;

	if (sendto(_nisserv_udp, _nisserv_buf, len, 0, &sin, sizeof(sin)) !=
	    len) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_ERROR: %s\n",
		    strerror(errno));
		return NSD_ERROR;
	}

	/*
	** Setup the callback for the reply.
	*/
	nsd_timeout_new(rq, 1000, portmap_timeout, (void *)0);

	return NSD_CONTINUE;
}

/*
** This routine creates a request for a portmap registration.
*/
int
nisserv_portmap(int program, int version, int proto, int port)
{
	nsd_file_t *rq;
	pmapdata_t *pd;
	nisserv_t *nd;

	nsd_logprintf(NSD_LOG_MIN, "entering nisserv_portmap:\n");

	pd = (pmapdata_t *)nsd_calloc(1, sizeof(pmapdata_t));
	if (! pd) {
		nsd_logprintf(NSD_LOG_RESOURCE, "nisserv_portmap: failed malloc\n");
		return NSD_ERROR;
	}
	pd->program = program;
	pd->version = version;
	pd->protocol = proto;
	pd->port = port;

	nd = nisserv_data_new();
	if (! nd) {
		free(pd);
		return NSD_ERROR;
	}
	nd->proc = portmap_callback;
	nd->proto = proto;

	if (nsd_file_init(&rq, "register", sizeof("register"),
	    (nsd_file_t *)0, NFREG) != NSD_OK) {
		free(pd);
		free(nd);
		return NSD_ERROR;
	}
	rq->f_cmd_data = pd;
	rq->f_sender = nd;
	nd->request = rq;

	return portmap_send(rq);
}
