#ifndef lint
static char sccsid[] =  "@(#)bootparam_subr.c	1.1 88/03/07 4.0NFSSRC; from 1.3 87/11/17 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

/*
 * Subroutines that implement the bootparam services.
 */

#include <rpcsvc/bootparam.h>
#include <netdb.h>
#include <nlist.h>
#include <stdio.h>
#include <strings.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/syssgi.h>
#include <net/route.h>
#include <sys/sbd.h>
#include <net/if.h>			/* for structs ifnet and ifaddr */
#include <netinet/in.h>
/* include <netinet/in_var.h>*/		/* for struct in_ifaddr */
#include <syslog.h>


/*
 * If true, reject whoami's from hosts in other NIS domains by requiring
 * that the hostname's IP domain matches our NIS domain's name.
 */
int interdomain;

#define LINESIZE	1024	

int debug = 0;

static struct in_addr get_route(struct in_addr);
void getf_printres(bp_getfile_res *, bp_getfile_arg *);

/*
 * Whoami turns a client address into a client name
 * and suggested route machine.
*/
bp_whoami_res *
bootparamproc_whoami_1(argp)
	bp_whoami_arg *argp;
{
	static bp_whoami_res res;
	struct in_addr clnt_addr;
	struct in_addr route_addr;
	struct hostent *hp;
	static char clnt_entry[LINESIZE];
	static char domain[32];

	if (argp->client_address.address_type != IP_ADDR_TYPE) {
		if (debug)
			syslog(LOG_ERR, 
				"Whoami failed: unknown address type %d",
				argp->client_address.address_type);
		return (NULL);
	}
	bcopy ((char *) &argp->client_address.bp_address.ip_addr,
	       (char *) &clnt_addr,  sizeof (clnt_addr));
	hp = gethostbyaddr((char *)&clnt_addr, sizeof clnt_addr, AF_INET);
	if (hp == NULL) {
		if (debug)
			syslog(LOG_ERR, 
				"Whoami failed: gethostbyaddr for %s.",
				inet_ntoa (clnt_addr));
		return (NULL);
	}
	
	if (bp_getclntent(hp->h_name, clnt_entry) != 0) {
		/*
		 * When diskless wants to install client tree, the entry	
		 * may not exist in bootparam data base.
		 */
		if (debug)
			syslog(LOG_ERR, "bp_getclntent for %s failed.",
				hp->h_name);
		return (NULL);
	}
	res.client_name = hp->h_name;
	getdomainname(domain, sizeof domain);
	if (domain[0] == '\0') {
		if (debug)
			syslog(LOG_ERR, "null domainname.");
		return (NULL);
	}
	if (interdomain) {
		char *dp;

		dp = strchr(hp->h_name, '.');
		if (dp && strcasecmp(dp + 1, domain)) {
			if (debug)
				syslog(LOG_ERR, 
					"interdomain whoami request from %s.",
					hp->h_name);
			return (NULL);
		}
	}
	res.domain_name = domain;
	res.router_address.address_type = IP_ADDR_TYPE;
	route_addr = get_route(clnt_addr);
	bcopy ((char *) &route_addr, 
	       (char *) &res.router_address.bp_address.ip_addr, 
	       sizeof (res.router_address.bp_address.ip_addr));

	if (debug) {
	    char buf[16];

	    strcpy(buf, inet_ntoa(clnt_addr));
	    syslog(LOG_ERR,
		"Whoami_1: name of addr \"%s\" is \"%s\", router addr=\"%s\"",
		buf, res.client_name, 
		inet_ntoa(res.router_address.bp_address.ip_addr));
        }
	return (&res);
}


#define ROUNDUP(a) ((a) > 0 ? (1 + (((a) - 1) | (sizeof(__uint64_t) - 1))) \
		    : sizeof(__uint64_t))
#ifdef _HAVE_SA_LEN
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))
#else
#define ADVANCE(x, n) (x += ROUNDUP(_FAKE_SA_LEN_DST(n)))
#endif

/*
 * get_route paws through the kernel's routing table looking for a route
 * via a gateway that is on the same network and subnet as the client.
 * Any gateway will do because the selected gateway will return ICMP redirect
 * messages to the client if it can not route the traffic itself.
 */
static struct in_addr
get_route(struct in_addr client_addr)
{
	int l, i;
	char *cp;
	struct sockaddr_in *gate;
	struct {
		struct	rt_msghdr rtm;
		struct sockaddr_in dst;
	} req;
	struct {
		struct	rt_msghdr rtm;
		char	m_space[512];
	} ans;
	static struct in_addr no_addr;
	static int s;
	u_short ident;

	ident = getpid();
	s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0) {
		syslog(LOG_ERR,
		       "get_route() socket(PF_ROUTE,SOCK_RAW): %m");
		return no_addr;
	}

	bzero(&req, sizeof(req));
	req.rtm.rtm_type = RTM_GET;
	req.rtm.rtm_flags = RTF_HOST;
	req.rtm.rtm_version = RTM_VERSION;
	req.rtm.rtm_addrs = RTA_DST;
	req.dst.sin_family = AF_INET;
	req.dst.sin_addr = client_addr;
	req.rtm.rtm_msglen = sizeof(req);
	if (write(s, &req, req.rtm.rtm_msglen) < 0) {
		syslog(LOG_ERR, "get_route() write(routing socket): %m");
		close(s);
		return no_addr;
	}

	do {
		l = read(s, &ans, sizeof(ans));
		if (l < 0) {
			syslog(LOG_ERR,"get_route: read(routing socket): %m");
			close(s);
			return no_addr;
		}
	} while (l > 0 && ans.rtm.rtm_pid != ident);
	close(s);

	if (ans.rtm.rtm_version != RTM_VERSION) {
		syslog(LOG_ERR, "get_route(): routing message version %d"
		       " not understood\n",
		       ans.rtm.rtm_version);
		return no_addr;
	}
	if (ans.rtm.rtm_msglen > sizeof(ans)) {
		syslog(LOG_ERR, "get_route(): message length  mismatch,"
		       " in packet %d, returned %d\n",
		       ans.rtm.rtm_msglen, sizeof(ans));
		return no_addr;
	}
	if (ans.rtm.rtm_errno)  {
		syslog(LOG_ERR, "get_route(): RTM_GET: %s (errno %d)",
		       strerror(ans.rtm.rtm_errno),
		       ans.rtm.rtm_errno);
		return no_addr;
	}
	for (cp = ans.m_space, i = 1; i; i <<= 1) {
		gate = (struct sockaddr_in *)cp;
		if (i & ans.rtm.rtm_addrs) {
			if (i == RTA_GATEWAY)
				return gate->sin_addr;
			ADVANCE(cp, (struct sockaddr*)gate);
		}
	}

	if (debug)
		syslog(LOG_ERR, "No route found for client %s.", 
		       inet_ntoa(client_addr));

	return no_addr;
}

/*
 * Getfile gets the client name and the key and returns its server
 * and the pathname for that key.
 */
bp_getfile_res *
bootparamproc_getfile_1(argp)
	bp_getfile_arg *argp;
{
	static bp_getfile_res res;
	static char clnt_entry[LINESIZE];
	struct hostent *hp;
	char *cp;

	if (bp_getclntkey(argp->client_name, argp->file_id, clnt_entry) != 0) {
		if (debug)
			syslog(LOG_ERR, "bp_getclntkey(%s,%s) failed.",
				argp->client_name, argp->file_id);
		return (NULL);
	}
	if ((cp = (char *)index(clnt_entry, ':')) == 0) {
		return (NULL);
	}
	*cp++ = '\0';
	res.server_name = clnt_entry;
	res.server_path = cp;
	if (*res.server_name == 0) {
		res.server_address.address_type = IP_ADDR_TYPE;
		bzero(&res.server_address.bp_address.ip_addr,
		      sizeof(res.server_address.bp_address.ip_addr));
	} else {
		if ((hp = gethostbyname(res.server_name)) == NULL) {
			if (debug)
				syslog(LOG_ERR,
					"getfile_1: gethostbyname(%s) failed",
					res.server_name);
			return (NULL);
		}
		res.server_address.address_type = IP_ADDR_TYPE;
		bcopy ((char *) hp->h_addr,
		       (char *) &res.server_address.bp_address.ip_addr,
		       sizeof (res.server_address.bp_address.ip_addr));
	}
	getf_printres(&res, argp);
	return (&res);
}

void
getf_printres(res, argp)
	bp_getfile_res *res;
	bp_getfile_arg *argp;
{
	syslog(LOG_ERR, "getfile_1: \"%s of %s\" is \"%s(%s):%s\"",
		argp->file_id, argp->client_name,
		res->server_name,
		inet_ntoa(res->server_address.bp_address.ip_addr),
		res->server_path);
}
