#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <time.h>
#include <sys/endian.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/route.h>
#include <malloc.h>
#include <alloca.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include <t6net.h>
#include <assert.h>
#include <sys/mac.h>
#include "ns_nis.h"

static int _nis_binder_sock = -1;
static int _nis_binder_port;
extern int _nis_sock;
extern uint32_t _nis_euid, _nis_egid;
extern nisstring_t _nis_domain;
extern nisstring_t _nis_host;
extern int _nis_xid;
extern char _nis_result[];
extern struct sockaddr_in _nis_sin;
extern time_t _nis_last_attempt;
extern nsd_file_t *__nsd_mounts;

extern int bindresvport(int, struct sockaddr_in *);

typedef struct {
	int			fd;
	struct sockaddr_in	sin;
	mac_t			lbl;
	uint32_t		xid;
	nis_server_t		*server;
} binddata_t;

/*
** This routine uses the attribute nis_set to determine who can set the
** domain binding.  If nis_set is set to TRUE then anyone can set it,
** if FALSE then noone, and if set to "local" then we test the address
** that the request came on to see if it is from a reserved port over
** loopback.
*/
static int
nis_getlocal_addrs(struct in_addr *addrs, int maxaddrs)
{
	size_t needed;
	char *buf, *next, *lim, *cp;
	struct if_msghdr *ifm;
	int mib[6], i=0;
	struct ifa_msghdr *ifam;
	struct sockaddr_in *mask, *addr;
	struct sockaddr *sa;
	
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;

	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,"nis_getlocal_addrs failed first sysctl: %s\n",
			      strerror(errno));
		return 0;
	}
	buf = alloca(needed);
	memset(buf,0, needed);
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,"nis_getlocal_addrs failed second sysctl: %s\n", 
			      strerror(errno));
		return 0;
	}

	lim = buf + needed;
	for (next = buf; next < lim; next += ifm->ifm_msglen) {
		addr = mask = (struct sockaddr_in *)0;
		ifm = (struct if_msghdr *)next;

		ifam = (struct ifa_msghdr *)ifm;
		
		if (ifam->ifam_addrs & RTA_NETMASK) {
			mask = (struct sockaddr_in *)(ifam + 1);
		}
		if (ifam->ifam_addrs & RTA_IFA) {
#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
	    : sizeof(__uint64_t))
#ifdef _HAVE_SA_LEN
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))
#else
#define ADVANCE(x, n) (x += ROUNDUP(_FAKE_SA_LEN_DST(n)))
#endif
			cp = (char *)mask;
			sa = (struct sockaddr *)mask;
			ADVANCE(cp, sa);
			addr = (struct sockaddr_in *)cp;
			if (! addr || addr->sin_family != AF_INET) {
				continue;
			}
			memcpy(&addrs[i++], &addr->sin_addr, sizeof(struct sockaddr_in));
			
			if (i > maxaddrs) {
				break;
				
			}
		}
	}
	return i;
}	

static int
nis_bind_permissions(struct in_addr addr)
{
	int i, nets;
	struct in_addr addrs[MAX_INTERFACES];
	char *security;

	security = nsd_attr_fetch_string(NULL, "nis_security", NULL);

	if (! security || !strcasecmp(security,"none")) {
		nsd_logprintf(NSD_LOG_OPER, "nis_binder rejected (from host %s)\n",
			      inet_ntoa(addr));
		return 0;
	}

	if (!strcasecmp(security,"any")) {
		return 1;
	}

	if (!strcasecmp(security,"local")) {
		if (addr.s_addr == INADDR_LOOPBACK)
			return 1;

		nets = nis_getlocal_addrs(addrs, sizeof(addrs)/sizeof(addrs[0]));
		for (i = 0; i < nets; i++) {
			if (addr.s_addr == addrs[i].s_addr)
			return 1;
		}
		nsd_logprintf(NSD_LOG_OPER,"nis_binder rejected from host %s\n",
			      inet_ntoa(addr));
		return 0;
	}		

	nsd_logprintf(NSD_LOG_CRIT, "nis_security set to invalid value %s, defaulting to none\n", security);
	nsd_logprintf(NSD_LOG_OPER, "nis_binder rejected from host %s\n", inet_ntoa(addr));
	nsd_attr_store(NULL,"nis_security","none");
	return 0;
}

int
nis_binder_send(nsd_file_t *rq)
{
	binddata_t *sd;
	uint32_t *p;

	nsd_logprintf(NSD_LOG_MIN, "entering nis_binder_send\n");

	sd = (binddata_t *)rq->f_sender;
	rq->f_sender = (void *)0;

	if (sd) {
		p = (uint32_t *)_nis_result;

		*p++ = sd->xid;
		*p++ = htonl(REPLY);
		*p++ = MSG_ACCEPTED;
		*p++ = 0;
		*p++ = 0;
		*p++ = SUCCESS;
		if (sd->server->sin.sin_port) {
			*p++ = YPBIND_SUCC_VAL;
			*p++ = htonl(sd->server->sin.sin_addr.s_addr);
			*p++ = htons(sd->server->sin.sin_port);
		} else {
			nsd_logprintf(NSD_LOG_OPER,
			    "bind failed for domain: %s\n", sd->server->domain);
			*p++ = YPBIND_FAIL_VAL;
			*p++ = YPBIND_ERR_NOSERV;
		}

		tsix_sendto_mac(sd->fd, _nis_result, ((char *)p) - _nis_result,
		    0, &sd->sin, sizeof(sd->sin), sd->lbl);
		
		mac_free(sd->lbl);
		free(sd);
	}

	if (rq->f_cmd_data) {
		free(rq->f_cmd_data), rq->f_cmd_data = 0;
	}
	nsd_file_clear(&rq);

	return NSD_OK;
}

static int
nis_binder(nsd_file_t *rq)
{
	nisrequest_t *nisrq = (nisrequest_t *)rq->f_cmd_data;
	time_t now;
	long timeout;

	nsd_logprintf(NSD_LOG_MIN, "entering nis_binder:\n");

	if (nisrq->server->sin.sin_port) {
		return NSD_OK;
	} else {
		if (--(nisrq->count) <= 0) {
			free(nisrq), rq->f_cmd_data = (void *)0;
			rq->f_status = NS_UNAVAIL;
			nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_ERROR:\n");
			return NSD_NEXT;
		}
		time(&now);
		if (now > nisrq->server->again) {
			nisrq->server->again = now + BIND_TIME;
			return nis_bind_broadcast(rq);
		} else {
			timeout = 1000 * nsd_attr_fetch_long(rq->f_attrs, 
						      "nis_timeout", 10, 1);
			nsd_timeout_new(rq, timeout, nis_match_timeout,
			    (void *)nisrq);
		}
	}
	return NSD_CONTINUE;
}

int
nis_binder_callback(nsd_file_t **rqp, int fd)
{
	int alen, rlen, uid;
	uint32_t *p, *q, reply[1024];
	char *domain;
	nsd_file_t *rq;
	nisrequest_t *nisrq;
	binddata_t *sd;
	nis_server_t *server;
	mac_t lbl;

	nsd_logprintf(NSD_LOG_MIN, "entering nis_binder_callback\n");
	*rqp = 0;

	/*
	** The following is the structure that we expect to get
	** (Not sure of this -- RGM XXX ---))
	struct {
	    struct {
		uint32_t	rm_xid;
		uint32_t	rm_direction;
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
		    struct {
			struct {
			    u_int32_t	len;
			    char	string[RNDUP(len)];
			} domainname;
			struct {
			    struct inaddr addr;
			    uint32_t port;
			} ypbinding
			uint32_t	version;
		    } accept
		} rm_resp;
	    } rpc;
	} msg;
	*/

	/*
	** Read in the request packet.
	*/
	alen = sizeof(_nis_sin);
	if (tsix_recvfrom_mac(fd, _nis_result, 4096, 0, &_nis_sin,
			      (int *) &alen, &lbl) < 0) {
		return NSD_CONTINUE;
	}

	/*
	** Parse request.
	*/
	p = (uint32_t *)_nis_result;

	q = reply;
	*q++ = p[0];		/* copy xid */
	*q++ = htonl(REPLY);	/* change to reply state */
	*q++ = MSG_ACCEPTED;

	if (ntohl(p[1]) != CALL) {
		mac_free(lbl);
		return NSD_CONTINUE;
	}
	if (ntohl(p[2]) != RPC_MSG_VERSION) {
		*q++ = RPC_MISMATCH;
		rlen = (q - reply) * sizeof(uint32_t);
		goto binder_result;
	}

	/*
	** Fill in reply with accepted header.
	*/
	*q++ = 0;		/* no verification on reply */
	*q++ = 0;

	if (ntohl(p[3]) != YPBINDPROG) {
		*q++ = PROG_MISMATCH;
		rlen = (q - reply) * sizeof(uint32_t);
		goto binder_result;
	}
	if (ntohl(p[4]) != YPBINDVERS) {
		*q++ = PROG_MISMATCH;
		rlen = (q - reply) * sizeof(uint32_t);
		goto binder_result;
	}

	switch (ntohl(p[5])) {
	    case YPBINDPROC_NULL:
		*q++ = SUCCESS;
		break;

	    case YPBINDPROC_DOMAIN:
		/* skip auth/verf */
		p += 7;
		rlen = ntohl(*p);
		p += (rlen) ? WORDS(rlen) : 1;
		rlen = ntohl(*p);
		p += (rlen) ? WORDS(rlen) + 1 : 2;

		domain = (char *)&p[1];
		domain[ntohl(p[0])] = (char)0;

		nsd_logprintf(NSD_LOG_MIN, "nis_binder domain request for %s\n", domain);

		*q++ = SUCCESS;
		server = nis_domain_server(domain);
		if (server && server->sin.sin_port) {
			/*
			** successful ypbind result is a status word followed
			** by the address and port of the server in network
			** byte order.
			*/
		        *q++ = YPBIND_SUCC_VAL;
			*q++ = htonl(server->sin.sin_addr.s_addr);
			*q++ = htons(server->sin.sin_port);
		} else {
			if (nsd_file_init(&rq, domain, strlen(domain),
			    (nsd_file_t *)0, NFREG, 0) != NSD_OK) {
				break;
			}

			nisrq = (nisrequest_t *)nsd_calloc(1,
			    sizeof(nisrequest_t));
			if (! nisrq) {
				nsd_file_clear(&rq);
				break;
			}
			strcpy(nisrq->domain.string, domain);
			nisrq->domain.len = strlen(domain);
			nisrq->domain.words = WORDS(nisrq->domain.len);
			nisrq->server = server;
			nisrq->proc = nis_binder;
			nisrq->count = 12;

			sd = (binddata_t *)nsd_calloc(1,
			    sizeof(binddata_t));
			if (! sd) {
				free(nisrq);
				nsd_file_clear(&rq);
				break;
			}
			sd->xid = reply[0];
			sd->fd = fd;
			sd->lbl = lbl;
			sd->server = server;
			memcpy(&sd->sin, &_nis_sin, sizeof(_nis_sin));

			rq->f_cmd_data = nisrq;
			rq->f_send = nis_binder_send;
			rq->f_sender = (void *)sd;
			*rqp = rq;
			return nis_bind_broadcast(rq);
		}
		break;

	    case YPBINDPROC_SETDOM:
		/*
		** We test whether this operation is allowed based on the
		** value of the nis_set attribute.  If the attribute is
		** set to none then we return a permissions error; if set
		** to local then we test to see that the owner of the
		** other end of this socket is root on the local host;
		** if the attribute is set to any then we just do the set
		** and respond.
		*/
		    
	        /* pull uid out of auth */
		p += 6;
		uid = (int)(*p);
		if (uid != AUTH_UNIX) {
			nsd_logprintf(NSD_LOG_OPER, "nis_binder command received with unsupported authentication type 0x%x\n", uid);
			uid = 1;
		} else {
			uid = 0;
		}
		p += 3;
		rlen = ntohl(*p);
		p += (rlen) ? WORDS(rlen) + 1 : 2;	
		if (! uid) {
			uid = (int)(*p);
			if (uid) {
       nsd_logprintf(NSD_LOG_OPER, "rejected nis_binder command received from uid %d at %s\n", 
		     uid, inet_ntoa(_nis_sin.sin_addr));
			}
		}
		p +=2;
		rlen = ntohl(*p);
		p += rlen + 1;

		if ((! uid) && nis_bind_permissions(_nis_sin.sin_addr)) {
			/* skip cb_verf*/
			rlen = ntohl(*p);
			p += (rlen) ? WORDS(rlen) + 1 : 2;

			/*
			** Get domain from request.
			*/
			rlen = ntohl(p[0]);
			if (rlen > MAXNAMELEN) {
				break;
			}
			domain = (char *)alloca(rlen + 1);
			strncpy(domain, (char *)&p[1], rlen);
			domain[rlen] = (char)0;
			/* should the else be 2 (or 1) */
			p += (rlen) ? WORDS(rlen) + 1 : 2;

			nsd_logprintf(NSD_LOG_MIN, "nis_binder domain set request for %s from %s\n",
 			   domain, inet_ntoa(_nis_sin.sin_addr));
			
			/*
			** Get address from request.
			*/
			server = nis_domain_server(domain);
			server->sin.sin_addr.s_addr = ntohl(p[0]);
			server->sin.sin_port = ntohs(p[1]);
			server->sin.sin_family = AF_INET;

			nsd_logprintf(NSD_LOG_OPER,
			    "nis_binder: NIS server for domain %s set to %s\n",
			    domain, inet_ntoa(server->sin.sin_addr));
			*q++ = SUCCESS;
		} else {
			*q++ = PROG_UNAVAIL;
		}			
		break;

	    default:
		*q++ = PROC_UNAVAIL;
	}
	rlen = (q - reply) * sizeof(uint32_t);

	/*
	** Send the result back.
	*/
binder_result:
	if (tsix_sendto_mac(fd, reply, rlen, 0, &_nis_sin,
			    sizeof(struct sockaddr_in), lbl) < rlen) {
		mac_free(lbl);
		return NSD_ERROR;
	}
	mac_free(lbl);

	return NSD_OK;
}

int
nis_binder_init(void)
{
	struct sockaddr_in sin;
	int len = sizeof(struct sockaddr_in);
	nsd_file_t *fp;
	nisrequest_t *nisrq;

	/*
	** Dont bother creating a new socket when we are reinitilized
	** but do re-register this with portmap.   This allows nsd to re-
	** register ypbind with a new portmap when sent a HUP.
	*/

	if (_nis_binder_sock < 0) {
		/*
		** Open up a socket for use as the ypbind request port.  
		** We explicitely bind it to a reserved port since this is
		** how NIS does security.  
		*/

		_nis_binder_sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
		if (_nis_binder_sock < 0) {
			nsd_logprintf(NSD_LOG_RESOURCE, "nis_register: unable to open socket: %s\n",
				      strerror(errno));
			return NSD_ERROR;
		}
		memset(&sin, 0, len);
		sin.sin_family = AF_INET;
		if (bindresvport(_nis_binder_sock, &sin)) {
			sin.sin_port = 0;
			(void)bind(_nis_binder_sock, 
				   (struct sockaddr *)&sin, len);
		}
		if (getsockname(_nis_binder_sock, (struct sockaddr *)&sin, 
				&len) != 0) {
			nsd_logprintf(NSD_LOG_RESOURCE, "nis_register: unable to bind socket: %s\n",
				      strerror(errno));
			return NSD_ERROR;
		}
		_nis_binder_port = sin.sin_port;

		/*
		** Allow request from processes at various labels
		*/
		if ( tsix_on ( _nis_binder_sock ) )
		{
			nsd_logprintf(NSD_LOG_RESOURCE, "nis_register: unable to turn privileged socket on: %s\n",
				      strerror(errno));
			return NSD_ERROR;
		}
	}

	nsd_callback_new(_nis_binder_sock, nis_binder_callback, NSD_READ);

	if (! __nsd_mounts) {
		nsd_portmap_unregister(YPBINDPROG, YPBINDVERS);
	}
	nsd_portmap_register(YPBINDPROG, YPBINDVERS, IPPROTO_UDP,
	    _nis_binder_port);

	/*
	** Start a binding for the default domain.
	*/
	if (nsd_file_init(&fp, "domain", sizeof("domain") - 1,
	    (nsd_file_t *)0, NFREG, 0) != NSD_OK) {
		return NSD_ERROR;
	}

	nisrq = (nisrequest_t *)nsd_calloc(1, sizeof(nisrequest_t));
	if (! nisrq) {
		nsd_file_clear(&fp);
		return NSD_ERROR;
	}
	strcpy(nisrq->domain.string, _nis_domain.string);
	nisrq->domain.len = _nis_domain.len;
	nisrq->domain.words = _nis_domain.words;
	nisrq->server = nis_domain_server(_nis_domain.string);
	nisrq->proc = nis_binder;
	nisrq->count = 12;

	fp->f_cmd_data = nisrq;
	fp->f_send = nis_binder_send;
	fp->f_sender = 0;
	return nis_bind_broadcast(fp);
}
