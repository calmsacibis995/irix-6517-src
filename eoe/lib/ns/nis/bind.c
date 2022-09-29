#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/endian.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/route.h>
#include <net/soioctl.h>
#include <ctype.h>
#include <alloca.h>
#include <netdb.h>
#include <assert.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_nis.h"

extern int _nis_sock;
extern struct sockaddr_in _nis_sin;
time_t _nis_last_attempt;
extern u_int32_t _nis_euid, _nis_egid;
extern char _nis_result[4096];
extern nisstring_t _nis_host;
extern int _nis_xid;
extern nsd_file_t *__nsd_mounts;

/*
** This function reads in a bind acknowlegement on the given file
** descriptor, checks it, then sets the binding to the replying
** system.
*/
static int
nis_bind_callback(nsd_file_t *rq)
{
	nisrequest_t *nisrq = (nisrequest_t *)rq->f_cmd_data;
	u_int32_t *p;
	u_int32_t port, status;

	nsd_logprintf(NSD_LOG_MIN, "entering nis_bind_callback:\n");
	assert(nisrq);

	/*
	** The following is the basic structure of what we expect to see
	** flying over the wire.
	struct {
	    struct {
		u_int32_t	rm_xid;
		u_int32_t	rm_direction;
		struct {
			u_int32_t	reply_stat;
			struct {
				struct {
					u_int32_t	oa_flavor;
					u_int32_t	oa_len;
					char		bytes[oa_len];
				} ar_verf;
				u_int32_t	ar_stat;
				struct {
					u_int32_t	port;
				} ru;
			} accept;
		} rm_resp;
	    } rpc;
	} msg;
	*/

	/*
	** Decode the packet parameters that we care about.
	** The fields that we really care about are the xid and
	** the port number.  Because of the request we sent we
	** should only receive back positive results.
	*/
	p = (u_int32_t *)(_nis_result);
	if ((ntohl(p[0]) != nisrq->xid) || (ntohl(p[1]) != REPLY) ||
	    (ntohl(p[2]) != MSG_ACCEPTED)) {
		nis_xid_new(nisrq->xid, rq, nis_bind_callback);
		return NSD_CONTINUE;
	}

	/*
	** For now we ignore the authentication/verification information.
	*/
	p += 5 + p[4]/sizeof(u_int32_t);
	status = p[0];
	if (status != SUCCESS) {
		nis_xid_new(nisrq->xid, rq, nis_bind_callback);
		return NSD_CONTINUE;
	}

	port = ntohl(p[1]);
	if (port == 0) {
		nis_xid_new(nisrq->xid, rq, nis_bind_callback);
		return NSD_CONTINUE;
	}

	/*
	** Looks good.  Set our global binding to this address and port.
	*/
	memcpy(&nisrq->server->sin, &_nis_sin, sizeof(struct sockaddr_in));
	nisrq->server->sin.sin_port = port;
	nisrq->server->again = 0;
	nisrq->server->tcp_port = 0;
	nisrq->server->bound_xid = ++_nis_xid;
	
	if (nisrq->server->count > 1) {
		nsd_logprintf(NSD_LOG_OPER,
		    "NIS server for domain %s OK.\n", nisrq->domain.string);
	} else {
		nsd_logprintf(NSD_LOG_OPER,
		    "NIS server for domain %s bound to %s.\n", 
			      nisrq->domain.string,
			      inet_ntoa(nisrq->server->sin.sin_addr));
	}

	nisrq->server->count = 0;
			      
	/*
	** Remove the timeout.
	*/
	nsd_timeout_remove(rq);

	/*
	** Retry the operation.
	*/
	if (nisrq->proc) {
		return (*nisrq->proc)(rq);
	} else {
		free(nisrq); rq->f_cmd_data = 0;
		return NSD_ERROR;
	}
}

/*
** This routine determines the broadcast addresses for each of our
** interfaces.
**
** We clobber whatever is in _nis_result;
*/
static int
nis_bcast_addrs(struct in_addr *addrs, int maxaddrs)
{
	int mib[6]={CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST, 0}, i=0;
	size_t needed;
	char *next, *lim, *p;
	struct ifa_msghdr *ifam;
	struct sockaddr *sa;
	struct sockaddr_in *sin;

	addrs[i++].s_addr = INADDR_LOOPBACK;

	if (sysctl(mib, 6, 0, &needed, NULL, 0) < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "nis_bcast_addrs: sysctl failed: %s\n",
		    strerror(errno));
		return 0;
	}

	if (needed < (sizeof(_nis_result) * sizeof(*_nis_result))) {
		p = _nis_result;
	} else {
		p = alloca(needed);
	}

	if (sysctl(mib, 6, p, &needed, NULL, 0) < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "nis_bcast_addrs: sysctl failed: %s\n",
		    strerror(errno));
		return 0;
	}
	
	lim = p + needed;
	for (next = p; next < lim; next += ifam->ifam_msglen) {
		ifam = (struct ifa_msghdr *)next;
		if ((ifam->ifam_type == RTM_IFINFO) ||
		    !(ifam->ifam_addrs & RTA_BRD) ||
		    !(ifam->ifam_flags & IFF_UP)) {
			continue;
		}
		p = (char *)(ifam + 1);
#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
	: sizeof(__uint64_t))
#ifdef _HAVE_SA_LEN
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))
#else
#define ADVANCE(x, n) (x += ROUNDUP(_FAKE_SA_LEN_DST(n)))
#endif
		if (ifam->ifam_addrs & RTA_NETMASK) {
			sa = (struct sockaddr *)p;
			ADVANCE(p, sa);
		}
		if (ifam->ifam_addrs & RTA_IFA) {
			sa = (struct sockaddr *)p;
			ADVANCE(p, sa);
		}
		sin = (struct sockaddr_in *)p;
		if (sin->sin_family != AF_INET) {
			continue;
		}
		if (sin->sin_addr.s_addr != INADDR_LOOPBACK) {
			memcpy(&addrs[i++], &sin->sin_addr,
			    sizeof(struct sockaddr_in));
		}
		if (i > maxaddrs) {
			break;
		}
	}

	return i;
}

/*
** This routine will parse the servers file for a list of server
** addresses, and stuff them into the passed in addrs array.
** we expect these addresses one per line, and skip empty lines
** and lines beginning with '#'.
**
** We clobber whatever is in _nis_result;
*/
static int
nis_server_addrs(nsd_file_t *rq, struct in_addr *addrs, int maxaddrs)
{
	FILE *fp;
	char *p, *q;
	int count = 0;
	struct hostent *h, he;
	nisrequest_t *nisrq = (nisrequest_t *)rq->f_cmd_data;
	char *hbuf;
	int herr;

	hbuf = _nis_result + 1024;
	p = nsd_attr_fetch_string(rq->f_attrs, "nis_servers", 0);
	if (p) {
		strncpy(_nis_result, p, 1023);
		_nis_result[1023] = (char)0;
		p = _nis_result;
		while ((q = strtok(p, " \t\n")) && (count < maxaddrs)) {
			p = (char)0;
			if (h = gethostbyname_r(q, &he, hbuf, 1024, &herr)) {
				memcpy(&addrs[count], h->h_addr_list[0],
				    sizeof(addrs[count]));
				count++;
			}
		}
	} else {
		nsd_cat(_nis_result, 1023, 5, NIS_BINDING_DIR, "/",
		     nisrq->domain.string, "/", NIS_SERVERS_FILE);
		fp = fopen(_nis_result, "r");
		if (! fp) {
			return 0;
		}
		while ((count < maxaddrs) &&
		    (fgets(_nis_result, 1023, fp) != NULL)) {
			for (p = _nis_result; *p && isspace(*p); p++);
			if ((! *p) || (*p == '#')) {
				continue;
			}
			if (q = strpbrk(p, " \t\n#")) {
				*q = (char)0;
			}
			if (h = gethostbyname_r(p, &he, hbuf, 1024, &herr)) {
				memcpy(&addrs[count], h->h_addr_list[0],
				    sizeof(addrs[count]));
				count++;
			}
		}
		fclose(fp);
	}

	return count;
}

/*
** This routine builds up a bind request if one does not already
** exist then broadcasts it out on each of our interfaces.  This
** routine very carefully determines the broadcast address for
** each interface on the machine and makes separate requests since
** that is what the rpc code this replaces did.
*/
int
nis_bind_broadcast(nsd_file_t *rq)
{
	nisrequest_t *nisrq = (nisrequest_t *)rq->f_cmd_data;
	u_int32_t *p, *save;
	int len, nets=0, errors = 0, i, maxttl, explicitbind=0;
	struct in_addr addrs[MAX_INTERFACES];
	struct sockaddr_in saddr;
	struct timeval t;
	long timeout;

	nsd_logprintf(NSD_LOG_MIN, "entering nis_bind_broadcast:\n");
	
	/*
	** The following is the basic structure that goes on the wire.  We
	** have to hand build this beast since it has variable length entities
	** in the middle of it.
	struct {
	    struct {
		u_int32_t	rm_xid;
		u_int32_t	rm_direction;
		struct {
		    u_int32_t	cb_rpcvers;
		    u_int32_t	cb_prog;
		    u_int32_t	cb_vers;
		    u_int32_t	cb_proc;
		    struct {
			u_int32_t	oa_flavor;
			u_int32_t	oa_len;
			struct {
			    u_int32_t	aup_time;
			    struct {
				u_int32_t	len;
				char		string[RNDUP(len)];
			    }
			    u_int32_t	aup_uid;
			    u_int32_t	aup_gid;
			    u_int32_t	aup_len;
			    u_int32_t	aup_gids[aup_len];
			} unix;
		    } cb_cred;
		    struct {
				u_int32_t	oa_flavor;
				u_int32_t	oa_len;
				char		bytes[oa_len];
		    } cb_verf;
		} rm_call;
	    } rpc;
	    struct {
		u_int32_t	prog;
		u_int32_t	vers;
		u_int32_t	proc;
		struct {
				u_int32_t	len;
				char		string[RNDUP(len)];
		} domain;
	    } yp;
	} msg;
	*/

	/*
	** We determine that addresses to send the bind request to.  If 
	** The request (still) has an address in it, then we try that.  Then
	** attempt to read this list from the given file, and failing this
	** we send a broadcast to each of the supported broadcast addresses.
	*/
	if (nisrq->server && 
	    nisrq->server->sin.sin_addr.s_addr && 
	    ! nisrq->server->sin.sin_port) {
		nsd_logprintf(NSD_LOG_OPER,"Explicitly binding to %s\n",
			      inet_ntoa(nisrq->server->sin.sin_addr));
		memcpy(&addrs[0],&nisrq->server->sin.sin_addr, 
		       sizeof(addrs[0]));
		nets=1;

		/* 
		** If a explicit bind failed, dont keep retrying it 
		** Also, disable broadcast and multicast logic
		*/

		if ( nisrq->server->sin.sin_addr.s_addr) {
			nisrq->server->sin.sin_addr.s_addr = 0;
			explicitbind=1;
		}
		nisrq->server->count = 0;
		nisrq->server->sin.sin_addr.s_addr = 0;
	} else if (nisrq->server->count == 1) {
		nsd_logprintf(NSD_LOG_CRIT,
		    "NIS server for domain %s not responding.\n",
		    nisrq->domain.string);
	} else if (nisrq->server->count > 1) {
		nsd_logprintf(NSD_LOG_OPER,
		    "NIS server for domain %s not responding.\n",
		    nisrq->domain.string);
	}
	nisrq->server->count++;

	if (! nets) {
		nets = nis_server_addrs(rq, addrs,
		    sizeof(addrs)/sizeof(addrs[0]));
		if (nets) {
			if (nisrq->server->multicast_ttl ==
			    DEFAULT_MULTICAST_TTL) {
				nisrq->server->multicast_ttl=0;
			}
		}
	}
	if (! nets) {
		nets = nis_bcast_addrs(addrs, sizeof(addrs)/sizeof(addrs[0]));
	}
	
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(PMAPPORT);
	memset(&saddr.sin_zero, 0, sizeof(saddr.sin_zero));

	/*
	** This is the equivelant to the code used in the old libc
	** routine to build a big message to send over the wire.
	** It was done with a few dozen calls to rpc and xdr routines,
	** but since that is all copyrighted by Sun, this has been
	** reverse engineered.
	*/
	p = (uint32_t *)_nis_result;
	nisrq->xid = ++_nis_xid;
	*p++ = htonl(nisrq->xid);
	*p++ = htonl(CALL);
	*p++ = htonl(RPC_MSG_VERSION);
	*p++ = htonl(PMAPPROG);
	*p++ = htonl(PMAPVERS);
	*p++ = htonl(PMAPPROC_CALLIT);

	/* auth */
	*p++ = htonl(AUTH_UNIX);
	save = p++;
	gettimeofday(&t);
	*p++ = htonl(t.tv_sec);

	*p++ = htonl(_nis_host.len);
	p[_nis_host.len/sizeof(*p)] = 0;
	memcpy(p, _nis_host.string, _nis_host.len);
	p += _nis_host.words;

	*p++ = htonl(_nis_euid);
	*p++ = htonl(_nis_egid);
	*p++ = 0;
	*save = htonl((p - save - 1) * sizeof(u_int32_t));

	/* verf */
	*p++ = 0;
	*p++ = 0;

	/* pmap req */
	*p++ = htonl(YPPROG);
	*p++ = htonl(YPVERS);
	*p++ = htonl(YPPROC_DOMAIN_NONACK);
	save = p++;

	*p++ = htonl(nisrq->domain.len);
	p[nisrq->domain.len/sizeof(*p)] = 0;
	memcpy(p, nisrq->domain.string, nisrq->domain.len);
	p += nisrq->domain.words;

	*save = htonl((p - save - 1) * sizeof(u_int32_t));

	len = (char *)p - _nis_result;

	/*
	** Loop through each of the addresses sending out the broadcast.
	*/
	for (i = 0; i < nets; i++) {
		saddr.sin_addr = addrs[i];
		nsd_logprintf(NSD_LOG_HIGH, "sending bind request to: %s\n",
		    inet_ntoa(addrs[i]));
		if (sendto(_nis_sock, _nis_result, len, 0, &saddr,
		    sizeof(struct sockaddr_in)) != len) {
			nsd_logprintf(NSD_LOG_HIGH,
				  "nis_bind_broadcast: sendto %s failed: %s\n",
				      inet_ntoa(saddr.sin_addr), 
				      strerror(errno));
			errors++;
		}
	}
	if (errors == nets) {
		/*
		** Report the last error.
		*/
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "nis_bind_broadcast: %d sendto(s) failed (last %s): %s\n",
			      nets, inet_ntoa(addrs[nets - 1]), 
			      strerror(errno));
		nsd_logprintf(NSD_LOG_RESOURCE,
			      "Can not contact any NIS servers.\n");
		nsd_logprintf(NSD_LOG_RESOURCE,
			      "Check for configuration or routing problem\n");
		free(nisrq); rq->f_cmd_data = 0;
		return NSD_ERROR;
	}

	if (!explicitbind) {

		/*
		** Send multicast packet if we have a ttl.  
		** We default a multicast ttl of 32.  Set to 0 to disable.
		*/
		maxttl = nsd_attr_fetch_long(rq->f_attrs, "nis_multicast", 10,
		    32);
		if ((nisrq->server->multicast_ttl > 0) && (maxttl > 0)) {
			u_char c;

			if (nisrq->server->multicast_ttl > maxttl) {
				nisrq->server->multicast_ttl = maxttl;
			}

			c = (u_char)maxttl;

			if (setsockopt(_nis_sock, IPPROTO_IP, IP_MULTICAST_TTL,
				       &c, sizeof(c)) < 0) {
				nsd_logprintf(NSD_LOG_RESOURCE,
				      "failed to set multicast ttl to %d: %s",
					      nisrq->server->multicast_ttl,
					      strerror(errno));
			} 
			saddr.sin_addr.s_addr = htonl(PMAP_MULTICAST_INADDR);
			nsd_logprintf(NSD_LOG_HIGH, 
				 "multicasting bind request [ttl=%d] to: %s\n",
				      nisrq->server->multicast_ttl, 
				      inet_ntoa(saddr.sin_addr));
			if (sendto(_nis_sock, _nis_result, len, 0, &saddr,
				   sizeof(struct sockaddr_in)) != len) {
				nsd_logprintf(NSD_LOG_RESOURCE,
				     "nis_bind_broadcast: sendto failed: %s\n",
					      strerror(errno));
				free(nisrq); rq->f_cmd_data = 0;
				return NSD_ERROR;
			}
		}

		/*
		** nexttime ensure we do broadcast and multcast
		*/
		nisrq->server->multicast_ttl += 8;
	}

	/*
	** Setup the callback for the reply.
	*/
	nis_xid_new(nisrq->xid, rq, nis_bind_callback);
	timeout = 1000 * ( nsd_attr_fetch_long(rq->f_attrs, "nis_timeout", 
					       10, 1) + 1);
	nsd_timeout_new(rq, timeout, nis_match_timeout, (void *)0);

	return NSD_CONTINUE;
}
