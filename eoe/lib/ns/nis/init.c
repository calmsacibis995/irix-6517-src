#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <net/soioctl.h>
#include <arpa/inet.h>
#include <malloc.h>
#include <fcntl.h>
#include <assert.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include <t6net.h>
#include "ns_nis.h"

nisstring_t _nis_domain;
nisstring_t _nis_host;
int _nis_sock = -1;
int _nis_sock_priv_port = -1;
u_int32_t _nis_euid, _nis_egid;
char _nis_result[4096];
struct sockaddr_in _nis_sin;
int _nis_xid;
extern int nis_binder_init(void);

extern int bindresvport(int, struct sockaddr_in *);

typedef struct nis_xid {
	int			xid;
	nsd_file_t		*rq;
	nsd_callout_proc	*proc;
	struct nis_xid		*next;
} nis_xid_t;
static nis_xid_t *nis_xids = 0;

static nis_server_t *nis_servers = 0;

/*
** This function will add an xid structure to a global list for later
** mapping from xid to request.
*/
int
nis_xid_new(int xid, nsd_file_t *rq, nsd_callout_proc *proc)
{
	nis_xid_t *new;

	new = (nis_xid_t *)nsd_calloc(1, sizeof(nis_xid_t));
	if (! new) {
		return NSD_ERROR;
	}

	new->xid = xid;
	new->rq = rq;
	new->proc = proc;
	new->next = nis_xids;
	nis_xids = new;

	return NSD_OK;
}

/*
** This routine will search through a linked list for a given xid
** and return the request associated with it.  We remove the record
** if we find it.
*/
int
nis_xid_lookup(int xid, nsd_file_t **rq, nsd_callout_proc **cp)
{
	nis_xid_t **last, *nx;

	for (last = &nis_xids, nx = nis_xids; nx && (nx->xid != xid);
	    last = &nx->next, nx = nx->next);
	if (nx) {
		*last = nx->next;
		*rq = nx->rq;
		*cp = nx->proc;
		free(nx);
		return NSD_OK;
	}

	return NSD_ERROR;
}

/*
** This routine will search through the server list for a server
** on the requested _nis_domain.  If one is not found a new one is
** added.
*/
nis_server_t *
nis_domain_server(char *domain)
{
	nis_server_t *ns;

	for (ns = nis_servers; ns; ns = ns->next) {
		if (strcmp(domain, ns->domain) == 0) {
			return ns;
		}
	}

	ns = (nis_server_t *)nsd_calloc(1, sizeof(nis_server_t));
	if (! ns) {
		return (nis_server_t *)0;
	}

	ns->multicast_ttl = DEFAULT_MULTICAST_TTL;
	strcpy(ns->domain, domain);
	ns->next = nis_servers;
	nis_servers = ns;

	return ns;
}

/*
** This routine will read in a new packet from the given file
** descriptor into a global buffer, lookup the transaction ID
** in our list, then call the appropriate callback function to
** finish the work.
*/
static int
nis_callback(nsd_file_t **rqp, int fd)
{
	int len, secure;
	u_int32_t xid, *ui;
	nsd_callout_proc *cp;
	char *addrstr;

	/*
	** Read in the response packet.
	*/
	len = sizeof(_nis_sin);
	if (recvfrom(fd, _nis_result, sizeof(_nis_result), 0, &_nis_sin,
	    (int *)&len) < 0) {
		return NSD_CONTINUE;
	}

	/*
	** The first 32 bits of any rpc contain a transaction ID which
	** we can use to map to a request and callback function.
	*/
	ui = (u_int32_t *)_nis_result;
	xid = ntohl(ui[0]);
	if (nis_xid_lookup(xid, rqp, &cp) != NSD_OK) {
		*rqp = 0;
		return NSD_CONTINUE;
	}

	/*
	** If we are running in "secure" mode then this must be coming
	** from a low numberred port else we ignore it.
	*/
	secure = nsd_attr_fetch_bool((*rqp)->f_attrs, "nis_secure", 0);
	if (secure && (_nis_sin.sin_family != AF_INET ||
	    _nis_sin.sin_port >= IPPORT_RESERVED)) {
		addrstr = inet_ntoa(_nis_sin.sin_addr);
		nsd_logprintf(NSD_LOG_OPER,
		    "nis_callback: received insecure bind reply from: %s\n",
		    addrstr);
		return NSD_CONTINUE;
	}
	nsd_logprintf(NSD_LOG_LOW, "nis_callback: calling handler for xid = %lu\n", xid);
	if (cp) {
		return (*cp)(*rqp);
	} else {
		if (*rqp && (*rqp)->f_cmd_data) {
			free((*rqp)->f_cmd_data);
			(*rqp)->f_cmd_data = 0;
		}
		return NSD_ERROR;
	}
}

/*
** Check if the nis client should run.
*/
static int
chkconfig_yp(void) {
	char buf[16];
	int fd;

	fd = nsd_open("/etc/config/yp", O_RDONLY, 0);
	if (fd < 0) {
		return FALSE;
	}
	if (read(fd, buf, sizeof(buf)) < 2) {
		close(fd);
		return FALSE;
	}
	close(fd);
	if (strncasecmp(buf, "on", 2) == 0) {
		return TRUE;
	} else {
		return FALSE;
	}
}

/*
** This routine is called when lamed is initializing when it first
** opens this library, or when the lamed init proceedure is explicitely
** called on our method.
*/
int
init(char *map)
{
	char buf[1024];
	int len, on = 1;
	struct timeval t;

	/*
	** If the old chkconfig file exists then we believe it.
	*/
	if (! chkconfig_yp()) {
		return NSD_ERROR;
	}

	/*
	** Init for the YP method just means that we need to reset the
	** hostname and domainname.
	**
	** We round up the length of the string to the next word since
	** that is the way we use it in the protocol code.
	*/
	if (gethostname(buf, sizeof(buf)) < 0) {
		nsd_logprintf(NSD_LOG_CRIT, "init (nis): no hostname set\n");
		return NSD_ERROR;
	} else {
		memset(&_nis_host, 0, sizeof(nisstring_t));
		len = strlen(buf);
		_nis_host.len = (_nis_host.len > YPMAXPEER) ? YPMAXPEER : len;
		memcpy(_nis_host.string, buf, _nis_host.len);
		_nis_host.words = WORDS(_nis_host.len);
	}

	if ((getdomainname(buf, sizeof(buf)) < 0) || (! *buf)) {
		nsd_logprintf(NSD_LOG_CRIT, "init (nis): no NIS domain set\n");
		return NSD_ERROR;
	} else {
		memset(&_nis_domain, 0, sizeof(nisstring_t));
		len = strlen(buf);
		_nis_domain.len = (_nis_domain.len > YPMAXDOMAIN) ?
		    YPMAXDOMAIN : len;
		memcpy(_nis_domain.string, buf, _nis_domain.len);
		_nis_domain.words = WORDS(_nis_domain.len);
	}

	_nis_euid = geteuid();
	_nis_egid = getegid();
	gettimeofday(&t);
	_nis_xid = getpid() ^ t.tv_sec ^ t.tv_usec;

	/*
	** Dont create a new socket when we are reinitilized
	*/

	if (! map && (_nis_sock < 0)) {

	        struct sockaddr_in sin;
		int len = sizeof(struct sockaddr_in);

		if ((_nis_sock =
		    socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "init (nis): failed to create socket: %s\n",
			    strerror(errno));
			return NSD_ERROR;
		}

		if (setsockopt(_nis_sock, SOL_SOCKET, SO_BROADCAST, &on,
		    sizeof(on)) < 0) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			       "init(nis): failed to set socket options: %s\n",
				      strerror(errno));
			return NSD_ERROR;
		}

		nsd_callback_new(_nis_sock, nis_callback, NSD_READ);

		/*
		** Create a socket to talk to ypserv from a privilege port.
		** The socket will only be used to talk to ypserv,
		** there's no need to allow broadcasting on it.
		*/
		if ((_nis_sock_priv_port =
		    socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "init (nis): failed to create socket for priv port: %s\n",
			    strerror(errno));
			return NSD_ERROR;
		}

	        /*
		** Try to bind to a reserve port. If it fails,
		** nsd will not be able to receive YP_SECURE maps.
		*/
		memset(&sin, 0, len);
		sin.sin_family = AF_INET;
		if (bindresvport(_nis_sock_priv_port, &sin)) {
		    close ( _nis_sock_priv_port );
		    _nis_sock_priv_port = -1;
		    nsd_logprintf(NSD_LOG_RESOURCE,
				  "init (nis): failed to bind socket to priv port: %s\n",
				  strerror(errno));
		    return NSD_ERROR;
		}

		nsd_callback_new(_nis_sock_priv_port, nis_callback, NSD_READ);

	}

	nis_binder_init();

	return NSD_OK;
}

int
dump(FILE *fp)
{
	nis_server_t *ns;
	char *security;

	/*
	** We just walk our binding list printing our servers for each
	** domain.
	*/
	fprintf(fp, "NIS servers:\n");
	for (ns = nis_servers; ns; ns = ns->next) {
		fprintf(fp, "\tdomain: %s addr: %s\n", ns->domain,
		    inet_ntoa(ns->sin.sin_addr));
		if (ns->sin.sin_port) {
			fprintf(fp, "\t\tudp port: %d\n", ns->sin.sin_port);
		} else {
			fprintf(fp, "\t\tudp unbound\n");
		}
		if (ns->tcp_port) {
			fprintf(fp, "\t\ttcp port: %d\n", ns->tcp_port);
		} else {
			fprintf(fp, "\t\ttcp unbound\n");
		}
		fprintf(fp, "\t\tmulticast ttl: %d\n", ns->multicast_ttl);
	}
	security = nsd_attr_fetch_string(0, "nis_security", 0);
	fprintf(fp, "set binding allowed by: %s\n", (security) ? security :
	    "nobody");
	
	return NSD_OK;
}

/*ARGSUSED*/
int
shake(int priority, time_t now)
{
	nis_server_t *ns;

	if (priority > 0) {
		for (ns = nis_servers; ns; ns = ns->next) {
			nis_netgroup_clear(&ns->netgroup);
		}
	}

	return NSD_OK;
}
