/*
*
* Copyright 1997-1999 Silicon Graphics, Inc.
* All Rights Reserved.
*
* This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
* the contents of this file may not be disclosed to third parties, copied or
* duplicated in any form, in whole or in part, without the prior written
* permission of Silicon Graphics, Inc.
*
* RESTRICTED RIGHTS LEGEND:
* Use, duplication or disclosure by the Government is subject to restrictions
* as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
* and Computer Software clause at DFARS 252.227-7013, and/or in similar or
* successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
* rights reserved under the Copyright Laws of the United States.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <errno.h>
#include <malloc.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_dns.h"

static server_t *servers = (server_t *)0;
static domain_t *domains = (domain_t *)0;
sort_t *sortlist = (sort_t *)0;
int _dns_sock = -1;
int _dns_ndots = 1;
time_t _dns_restime = 0;

extern int dns_query_callback(nsd_file_t **, int);
extern char **strtoargv(char *);

extern struct addrlist *alist, *nosort;

typedef struct dns_xid {
	uint16_t	xid;
	nsd_file_t	*rq;
	struct dns_xid	*next;
} dns_xid_t;

static dns_xid_t *dns_xids = (dns_xid_t *)0;

/*
** This routine will add a new xid record to our list.
*/
int
dns_xid_new(uint16_t xid, nsd_file_t *rq)
{
	dns_xid_t *new;
	
	new = (dns_xid_t *)nsd_calloc(1, sizeof(dns_xid_t));
	if (! new) {
		nsd_logprintf(NSD_LOG_RESOURCE, "dns_xid_new: failed malloc\n");
		return NSD_ERROR;
	}

	new->xid = xid;
	new->rq = rq;
	new->next = dns_xids;
	dns_xids = new;

	return NSD_OK;
}

/*
** This routine will look down the xid list and return the request
** associated with an xid.  We remove the record if it is found.
*/
nsd_file_t *
dns_xid_lookup(uint16_t xid)
{
	dns_xid_t **last, *dx;
	nsd_file_t *result=0;

	for (last = &dns_xids, dx = dns_xids; dx && (dx->xid != xid);
	    last = &dx->next, dx = dx->next);
	if (dx) {
		*last = dx->next;
		result = dx->rq;
		free(dx);
	}

	return result;
}

/*
** This routine will copy the current list of servers into the request.
** We copy the list so that if the resolv.conf file changes in the middle
** of a request that we will not get confused.
*/
server_t *
dns_servers(void)
{
	server_t *sp, *new, *result, **end;

	result = (server_t *)0;
	end = (server_t **)0;
	for (sp = servers; sp; sp = sp->next) {
		new = (server_t *)nsd_malloc(sizeof(server_t));
		if (! new) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "dns_servers: failed malloc\n");
			return (server_t *)0;
		}
		memcpy(new, sp, sizeof(server_t));
		new->next = (server_t *)0;
		if (end) {
			*end = new;
		} else {
			result = new;
		}
		end = &new->next;
	}

	return result;
}

/*
** Push a given dns server to the bottom of the default server list
** Dont complain if we dont find it, requests can be made to any
** dns server, not just those in the default list.
*/

void
dns_pushdown_server(server_t *tsp) 
{
	server_t *sp, *csp, **end;
	struct in_addr a;

	if (! tsp)
		return;

	a.s_addr = tsp->addr;
	csp = (server_t *)0;
	for (end = &servers, sp = servers; 
	     sp ; 
	     end = &sp->next, sp = sp->next) {
		if (csp == (server_t *)0 && 
		    sp->addr == tsp->addr && 
		    sp->next != (server_t *)0) {
			csp = sp;
			*end = sp->next;
		}
	}		
	if (csp) {
		*end = csp;
		csp->next = (server_t *)0;
		nsd_logprintf(NSD_LOG_OPER,
		    "dns_server %s pushed to bottom of server list\n",
		    inet_ntoa(a));
	}
}

/*
** This routine will return a copy of the domain list.  We copy the
** domain list for each request so that the resolv.conf file can change
** while we are in the middle of a request and we will not become
** confused.
*/
domain_t *
dns_domains(void)
{
	domain_t *dp, *new, *result, **end;

	result = (domain_t *)0;
	end = (domain_t **)0;
	for (dp = domains; dp; dp = dp->next) {
		new = (domain_t *)nsd_malloc(sizeof(domain_t) + dp->len);
		if (! new) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "dns_domains: failed malloc\n");
			return (domain_t *)0;
		}
		memcpy(new, dp, sizeof(domain_t) + dp->len);
		new->next = (domain_t *)0;
		if (end) {
			*end = new;
		} else {
			result = new;
		}
		end = &new->next;
	}

	return result;
}

/*
** This routine just walks the server list freeing each node.
*/
static void
free_servers(server_t **list)
{
	server_t *sp, **hsp;
	for (hsp = list; *list; *list = sp) {
		sp = (*list)->next;
		free(*list);
	}
	*hsp = (server_t *)0;
}

/*
** This routine walks over the domain list freeing each node.
*/
static void
free_domains(domain_t **list)
{
	domain_t *dp,**hdp;
	for (hdp=list; *list; *list = dp) {
		dp = (*list)->next;
		free(*list);
	}
	*hdp=(domain_t *)0;
}

/*
** This routine walks over the sort list freeing each node.
*/
static void
free_sortlist(sort_t **list)
{
	sort_t *sp, **hsp;
	for (hsp=list; *list; *list = sp) {
		sp = (*list)->next;
		free(*list);
	}
	*hsp=(sort_t *)0;
}

/*
** This routine will free the memory associated with a DNS request
** structure.
*/
void
dns_free_request(dnsrequest_t **rqp)
{
	dnsrequest_t *rq;
	if (rqp && *rqp) {
		rq = *rqp;
		if (rq->servers) {
			free_servers(&rq->servers);
		}
		if (rq->domains) {
			free_domains(&rq->domains);
		}
		free(rq);
		*rqp = (dnsrequest_t *)0;
	}
}

/*
** Given a line like:
**	domain foo.bar.com.
** this routine will create a list of domain structures like:
**	foo.bar.com bar.com
*/
domain_t *
dns_domain_parse(char *domain)
{
	domain_t *dp, *dpn, **end;
	char *q, *last;
	int len;

	/*
	** Skip keyword.
	*/
	if (strncasecmp("domain", domain, sizeof("domain") - 1) == 0) {
		domain += sizeof("domain");
	}
	for (; *domain && isspace(*domain); domain++);
	if (! *domain) {
		return (domain_t *)0;
	}
	while (*domain == '.') {
		domain++;
	}
	len = strcspn(domain, " \t");

	/*
	** Allocate memory for a new domain node.
	*/
	dp = (domain_t *)nsd_calloc(1, sizeof(domain_t) + len + 1);
	if (! dp) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "dns_domain_parse: failed malloc\n");
		return (domain_t *)0;
	}

	*dp->name = '.';
	memcpy(dp->name+1, domain, len);
	dp->len = len + 1;
	dp->name[dp->len] = 0;
	end = &dp->next;

	/*
	** Each sub-element in the name is also added as
	** a domainname.   Do not add any TLD to this list.
	*/
	for (last = domain + len, domain = strchr(domain, '.');
	    domain && (domain < last); domain = q) {
		q = strchr(domain + 1, '.');
		if (!q) {
			break;
		}

		len = last - domain;
		dpn = (domain_t *)nsd_calloc(1, sizeof(domain_t) + len + 1);
		if (! dpn) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "dns_domain_parse: failed malloc\n");
			free_domains(&dp);
			return (domain_t *)0;
		}
		memcpy(dpn->name, domain, len);
		dpn->len = len;
		dpn->name[len] = 0;

		*end = dpn;
		end = &dpn->next;
	}

	return dp;
}

/*
** This will simply split a search path creating a list of domain structures.
*/
domain_t *
dns_search_parse(char *search)
{
	char *p;
	domain_t *list, *dp, **end;
	int len;

	if (strncasecmp(search, "search", sizeof("search" - 1)) == 0) {
		search += sizeof("search");
	}

	for (list = 0, end = &list; *search; search = p) {
		for (; *search && isspace(*search); search++);
		for (p = search; *p && ! isspace(*p); p++);
		if (*search) {
			len = p - search;
			dp = (domain_t *)nsd_calloc(1,
			    sizeof(domain_t) + len + 1);
			if (! dp) {
				nsd_logprintf(NSD_LOG_RESOURCE,
				    "dns_search_parse: failed malloc\n");
				free_domains(&list);
				return (domain_t *)0;
			}

			*dp->name = '.';
			memcpy(dp->name+1, search, len);
			dp->len = len + 1;
			dp->name[dp->len] = 0;

			*end = dp;
			end = &dp->next;
		}
	}

	return list;
}

/*
** This will parse a line like:
**	nameserver 1.2.3.4 machine foo.sgi.com
** and create a server structure for each element.
*/
server_t *
dns_servers_parse(char *server)
{
	server_t *sp, *list, **end;
	struct hostent *he, h;
	char *p, *q, buf[1024];
	int n;

	if (strncasecmp(server, "nameserver", sizeof("nameserver") - 1) == 0) {
		server += sizeof("nameserver");
	}

	for (list = 0, end = &list; *server; server = p) {
		for (; *server && isspace(*server); server++);
		if (! *server) {
			break;
		}
		for (p = server; *p && ! isspace(*p); p++);

		sp = (server_t *)nsd_calloc(1, sizeof(server_t));
		if (! sp) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "dns_servers_parse: failed malloc\n");
			free_servers(&list);
			return (server_t *)0;
		}

		n = p - server;
		memcpy(buf, server, n);
		buf[n] = 0;
		n += (8 - (n % 8));
		q = buf + n;
		he = gethostbyname_r(buf, &h, q, sizeof(buf) - n, &n);
		if (! he) {
			nsd_logprintf(NSD_LOG_CRIT,
			  "dns_servers_parse: illegal nameserver "
			  "address: %s\n", buf);
			free_servers(&list);
			free(sp);
			return (server_t *)0;
		}
		memcpy(&sp->addr, he->h_addr_list[0], sizeof(sp->addr));
		if (sp->addr == 0) {
			sp->addr = INADDR_LOOPBACK;
		}

		*end = sp;
		end = &sp->next;
	}
	
	return list;
}

/*
** This routine will pasrse a sortlist line
*/
sort_t *
dns_sortlist_parse(char *p)
{
	sort_t *tse, **se, *ne;
	char *q, buf[64];
	int len;

	if (! strncasecmp(p, "sortlist", sizeof("sortlist") - 1)) {
		p += sizeof("sortlist");
	}

	/* loop through entries */
	for (tse = 0, se = &tse; *p; p = q) {
		for (; *p && isspace(*p); p++);
		if (! *p) {
			break;
		}
		for (q = p; *q && ! isspace(*q) && *q != '/' && *q != '&'; q++);
		len = q - p;
		memcpy(buf, p, len);
		buf[len] = 0;

		ne = (sort_t *)nsd_calloc(1, sizeof(sort_t));
		if (! ne) {
			nsd_logprintf(NSD_LOG_RESOURCE,
				      "failed malloc");
			return (sort_t*)0;
		}

		/* parse address */
		if (! inet_aton(buf, &ne->addr)) {
			nsd_logprintf(NSD_LOG_CRIT,
				      "illegal address in sortlist: %s\n", buf);
			for (; *q && ! isspace(*q); q++);
			free(ne);
			continue;
		}

		/* parse netmask, or use default */
		if (*q == '/' || *q == '&') {
			p = q + 1;
			for (; *q && ! isspace(*q); q++);
			len = q - p;
			memcpy(buf, p, len);
			buf[len] = 0;
			if (! inet_aton(buf, &ne->netmask)) {
				nsd_logprintf(NSD_LOG_CRIT,
				    "illegal netmask in sortlist: %s\n", buf);
				free(ne);
				continue;
			}
		} else {
			if (IN_CLASSA(ne->addr.s_addr)) {
				ne->netmask.s_addr =
					IN_CLASSA_NET;
			} else if (IN_CLASSB(ne->addr.s_addr)) {
				ne->netmask.s_addr =
					IN_CLASSB_NET;
			} else if (IN_CLASSC(ne->addr.s_addr)) {
				ne->netmask.s_addr =
					IN_CLASSC_NET;
			} else if (IN_CLASSD(ne->addr.s_addr)) {
				ne->netmask.s_addr =
					IN_CLASSD_NET;
			} else {
				nsd_logprintf(NSD_LOG_CRIT,
				    "bad address in sortlist: %s\n", buf);
				free(ne);
				continue;
			}
		}

		/* append to end of list */
		*se = ne;
		se = &ne->next;
	}

	return (tse);
}
	

/*
** Check if the dns server is going to be run.
*/
static int
chkconfig_named(void) {
	char buf[16];
	int fd;

	fd = nsd_open("/etc/config/named", O_RDONLY, 0);
	if (fd < 0) {
		return FALSE;
	}
	if (read(fd, buf, sizeof(buf)) < 0) {
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
** This routine just parses the resolv.conf file into some global variables.
*/
int
dns_config(void)
{
	FILE *fp;
	char buf[1024], *p, *q;
	server_t **ends;
	struct stat sbuf;
	struct hostent *hp, h;
	char hbuf[1024];
	int herr;

	/*
	** Remove any old servers, domains, sortlist, options.
	*/
	free_servers(&servers);
	free_domains(&domains);
	free_sortlist(&sortlist);
	_dns_ndots = 1;
	ends = &servers;

	fp = fopen(_PATH_RESCONF, "r");
	if (! fp) {
		/*
		** Defaults.
		*/
		if (chkconfig_named() == FALSE) {
			nsd_logprintf(NSD_LOG_CRIT,
    "No /etc/resolv.conf or nameserver found.  No default DNS server set.\n");
			servers = (server_t *)0;
		} else {
			p = nsd_attr_fetch_string(0, "hostname", 0);
			if (p && (hp = gethostbyname_r(p, &h, hbuf,
			    sizeof(hbuf), &herr)) &&
			    (q = strchr(hp->h_name, '.'))) {
				domains = dns_domain_parse(q + 1);
				nsd_logprintf(NSD_LOG_OPER,
	    "dns_config: using default domain: %s server: localhost\n", q + 1);

			} else if (((getdomainname(buf,sizeof(buf)) == 0) &&
				    *buf) &&
				   (q = strchr(hp->h_name, '.'))) {
				domains = dns_domain_parse(buf);
				nsd_logprintf(NSD_LOG_OPER,
"dns_config: using NIS domain (%s) as default dns domain, server: localhost\n",
				    buf);
			}
			servers = dns_servers_parse("0.0.0.0");
		}
		_dns_restime = 0;
		return NSD_OK;
	}

	fstat(fileno(fp), &sbuf);
	_dns_restime = sbuf.st_mtime;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		/* forward past leading whitespace */
		for (p = buf; *p && isspace(*p); p++);

		/* skip comments */
		if ((*p == ';') || (*p == '#')) {
			continue;
		}

		/* terminate line on comment */
		q = strpbrk(p, ";#\n");
		if (q) {
			*q = (char)0;
		}
		
		if (!strncasecmp(p, "domain", sizeof("domain") - 1)) {
			/* read default domain name */
			if (domains) {
				nsd_logprintf(NSD_LOG_CRIT,
"init (dns): multiple domains or search paths not allowed in resolv.conf\n");
				continue;
			}

			domains = dns_domain_parse(p);

		} else if (!strncasecmp(p, "search", sizeof("search") - 1)) {
			/* set search list */
			if (domains) {
				nsd_logprintf(NSD_LOG_CRIT,
"init (dns): multiple domains or search paths not allowed in resolv.conf\n");
				continue;
			}
			domains = dns_search_parse(p);

		} else if (!strncasecmp(p, "nameserver",
		    sizeof("nameserver") - 1)) {
			/* append nameserver to list */
			*ends = dns_servers_parse(p);
			if (*ends) {
				ends = &(*ends)->next;
			}

		} else if (!strncasecmp(p, "sortlist",
		    sizeof("sortlist") - 1)) {
			/* free old list if one exists */
			if (sortlist) {
				free_sortlist(&sortlist);
			}
			sortlist = dns_sortlist_parse(p);

		} else if (!strncasecmp(p, "options", sizeof("options") - 1)) {
			p += sizeof("options");
			for (;;) {
				for (; *p && isspace(*p); p++);
				if (! *p) {
					break;
				}
				if (! strncasecmp(p, "ndots:",
				    sizeof("ndots:") - 1)) {
					p += sizeof("ndots:") - 1;
					_dns_ndots = atoi(p);
					if (_dns_ndots < 1) {
						_dns_ndots = 1;
					}
				}
				for (; *p && ! isspace(*p); p++);
				if (! *p) {
					break;
				}
			}

		} else if (!strncasecmp(p, "hostresorder",
		    sizeof("hostresorder") - 1)) {
			nsd_logprintf(NSD_LOG_OPER,
"Ignoring hostresorder keyword found in resolv.conf.  Resolve order is now controlled by nsswitch.conf.\n");
		}
	}
	fclose(fp);

	if (! domains) {
		char hostname[MAXHOSTNAMELEN + 1];
		
		hostname[MAXHOSTNAMELEN]='\0';
		if ((gethostname(hostname, MAXHOSTNAMELEN) != -1) &&
		    ((p = strchr(hostname, '.')) != NULL)) {
			domains = dns_domain_parse(++p);
			nsd_logprintf(NSD_LOG_OPER, 
			   "using DNS domain %s determined from hostname\n", p);
		}
	}
		
	if (! domains) {
		nsd_logprintf(NSD_LOG_OPER,
			   "init (dns): No domainname could be determined.\n");
		nsd_logprintf(NSD_LOG_OPER,
			    "Only fully qualified lookups will be resolved\n");
	}

	if (! servers) {
		/*
		** Add a "default" server address.
		**
		** We want the address to be 127.0.0.1 and next to be NULL.
		*/
		servers = (server_t *)nsd_calloc(1, sizeof(server_t));
		if (! servers)  {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "init (dns): failed malloc\n");
			return NSD_ERROR;
		}
		servers->addr = INADDR_LOOPBACK;
	}

	return NSD_OK;
}

/*
** This routine will read the /etc/resolv.conf file and setup a list
** of "domains", and a list of "servers" for doing lookups.
*/
int
init(char *map)
{
	/*
	** Open up a socket for use in making dns lookups for all requests.
	*/
	if (! map && _dns_sock < 0) {
		if ((_dns_sock = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "init (dns): failed to create socket: %s\n",
			    strerror(errno));
			return NSD_ERROR;
		}

		nsd_callback_new(_dns_sock, dns_query_callback, NSD_READ);
	}

	/*
	** Parse resolv.conf.
	*/
	return dns_config();
}

/*
** The dump function just prints out our current state.  Essentially, this
** is just what we parsed out of resolv.conf.
*/
int
dump(FILE *fp)
{
	domain_t *dp;
	server_t *sp;
	sort_t *ap;
	struct in_addr addr;

	if (_dns_restime) {
		fprintf(fp, "resolv.conf timestamp: %s", ctime(&_dns_restime));
	} else {
		fprintf(fp, "missing or unparsable resolv.conf\n");
	}

	/*
	** Print out flags.
	*/
	fprintf(fp, "options:\n");
	fprintf(fp, "\tndots: %d\n", _dns_ndots);

	/*
	** Print out server list.
	*/
	fprintf(fp, "servers:\n");
	for (sp = servers; sp; sp = sp->next) {
		addr.s_addr = sp->addr;
		fprintf(fp, "\taddress: %s\n", inet_ntoa(addr));
	}

	/*
	** Print out search path.
	*/
	fprintf(fp, "domain search patch:\n");
	for (dp = domains; dp; dp = dp->next) {
		fprintf(fp, "\tdomain: %s\n", dp->name);
	}

	/*
	** Print out sort list.
	*/
	if (sortlist) {
		fprintf(fp, "sort list:\n");
		for (ap = sortlist; ap; ap = ap->next) {
			fprintf(fp, "\t%s/", inet_ntoa(ap->addr));
			fprintf(fp, "%s\n", inet_ntoa(ap->netmask));
		}
	}

	return NSD_OK;
}
