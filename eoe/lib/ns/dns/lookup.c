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
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/endian.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <errno.h>
#include <malloc.h>
#include <ctype.h>
#include <sys/stat.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_dns.h"

extern server_t *dns_servers(void);
extern domain_t *dns_domains(void);
extern void dns_free_request(dnsrequest_t **);
extern void dns_pushdown_server(server_t *);
extern nsd_file_t *dns_xid_lookup(uint16_t);
extern int *dns_xid_new(uint16_t, nsd_file_t *);
extern int _dns_sock;
extern int _dns_ndots;
extern time_t _dns_restime;
extern sort_t *sortlist;

static int dns_query(nsd_file_t *);

static uint16_t id_count = 0;

extern int _ltoa(long, char *);

/*
** All DNS messages look alike.  The following is what flies over the wire:
**	----------------------------------
**	|            HEADER              |	Struct HEADER
**	|--------------------------------|
**	|            Question            |	QTYPE/QCLASS/QNAME
**	|--------------------------------|
**	|            Answer              |	Resource Record List
**	|--------------------------------|
**	|            Authority           |	Resource Record List
**	|--------------------------------|
**	|            Additional          |	Resource Record List
**	----------------------------------
**
** A resource record looks like:
**	Domain Name / Type / Class / TTL / Data Length / Data
** the data is determined by the type.
**
** The header defines what type of a request it is, what is supported, etc.
** It also contains information explaining what all the other fields look
** like.  Any field but the header can be empty.
*/

/*
** This is the timeout handler for an incoming dns response.
** If we get here then we just delete the callbacks and timeout
** and try again with the next server address.
*/
static int
dns_query_timeout(nsd_file_t **rqp, nsd_times_t *to)
{
	dnsrequest_t *dnsrq;
	server_t *sp;
	nsd_file_t *rq;

	rq = to->t_file;
	*rqp = rq;
	if (! rq) {
		return NSD_ERROR;
	}

	/*
	** Remove the timeout from previous request.
	*/
	nsd_timeout_remove(rq);
	dnsrq = (dnsrequest_t *)rq->f_cmd_data;
	if (! dnsrq) {
		rq->f_status = NS_FATAL;
		return NSD_ERROR;
	}
	dns_xid_lookup(dnsrq->xid);

	if (++dnsrq->retries > dnsrq->max_tries) {
		/*
		** Switch to next server.
		*/
		dnsrq->retries = 0;
		if (((sp = dnsrq->servers) == (server_t *)0) ||
		    (dnsrq->flags & DNS_PARALLEL)) {
			/*
			** Tried all servers, so here we croak.
			*/
			rq->f_status = NS_NOTFOUND;
			dns_free_request((dnsrequest_t **)&rq->f_cmd_data);
			return NSD_NEXT;
		}
		dnsrq->servers = sp->next;
		dns_pushdown_server(sp);
		free(sp);
	}

	return dns_query(rq);
}

static void
clearaddrs(struct addrlist **extra)
{
	sort_t *sp;
	addrlist_t *ap;

	for (sp = sortlist; sp; sp = sp->next) {
		for (ap = sp->sublist; ap; ap = sp->sublist) {
			sp->sublist = ap->next;
			free(ap);
		}
	}

	if (extra) {
		for (ap = *extra; ap; ap = *extra) {
			*extra = ap->next;
			free(ap);
		}
	}
}

static int
addrsort(addrlist_t *addr, addrlist_t **extra)
{
	sort_t *sp;

	for (sp = sortlist; sp; sp = sp->next) {
		if ((addr->addr.s_addr & sp->netmask.s_addr) ==
		    (sp->addr.s_addr & sp->netmask.s_addr)) {
			nsd_logprintf(NSD_LOG_HIGH, "\tsortlist match: %s\n",
			    inet_ntoa(addr->addr));
			addr->next = sp->sublist;
			sp->sublist = addr;
			return 1;
		}
	}

	addr->next = *extra;
	*extra = addr;

	return 0;
}

/*
** This is the callback for an incoming dns response.  We read the
** packet off of the given file descriptor, parse it, and either
** setup a response, or try again.
*/
int
dns_query_callback(nsd_file_t **rqp, int fd)
{
	int len, n, ancount, qdcount;
	u_int16_t type;
	struct sockaddr_in saddr;
	nsd_file_t *rq;
	dnsrequest_t *dnsrq;
	u_char ansbuf[1024], *cp, *eom;
	char namebuf[256];
	char aliasbuf[1024], *ap;
	char result[1024];
	char tmpbuf[512];
	char *key;
	HEADER *hp;
	domain_t *domain;
	server_t *sp, **end;
	sort_t *so;
	addrlist_t *extra=0, *addr;
	uint32_t ttl, minttl=-1, maxttl;

	/*
	** Read in response packet.
	*/
	len = sizeof(saddr);
	if ((n = recvfrom(fd, ansbuf, sizeof(ansbuf), 0, &saddr, &len)) < 0) {
		nsd_logprintf(NSD_LOG_OPER, 
			      "dns_query_callback: failed recvfrom: %s\n",
			      strerror(errno));
		return NSD_CONTINUE;
	}

	/*
	** Parse response.
	*/
	hp = (HEADER *)ansbuf;

	nsd_logprintf(NSD_LOG_MIN,
	    "entering dns_query_callback for request %lu\n", hp->id);
	nsd_logprintf(NSD_LOG_LOW,
	    "\treceived response from: %s\n", inet_ntoa(saddr.sin_addr));

	rq = dns_xid_lookup(hp->id);
	*rqp = rq;
	if (! rq) {
		nsd_logprintf(NSD_LOG_MIN,
		    "\tno matching request for xid %d\n",hp->id);
		return NSD_CONTINUE;
	}
	nsd_timeout_remove(rq);

	dnsrq = (dnsrequest_t *)rq->f_cmd_data;
	if (! dnsrq) {
		return NSD_ERROR;
	}

	/*
	** Check for an error.
	*/
	if (hp->rcode != NOERROR || ntohs(hp->ancount) == 0) {
		switch (hp->rcode) {
			case NXDOMAIN:
			case NOERROR:
				/*
				** Switch to next domain.
				*/
				if (dnsrq->domains) {
					domain = dnsrq->domains->next;
					free(dnsrq->domains);
					dnsrq->domains = domain;
					dnsrq->xid = ++id_count;
					if (domain) {
						nsd_logprintf(NSD_LOG_LOW, 
						  "\ttrying next domain %s\n",
							      domain->name);
						return dns_query(rq);
					}
				}
				nsd_logprintf(NSD_LOG_LOW,
				    "\tno more domains\n");
				dns_free_request(
				    (dnsrequest_t **)&rq->f_cmd_data);
				rq->f_status = NS_NOTFOUND;
				return NSD_NEXT;
			case SERVFAIL:
			case NOTIMP:
			case REFUSED:
				nsd_logprintf(NSD_LOG_MIN,
				    "\treceived error: %d\n", hp->rcode);

				/*
				** Remove this server and try again.
				*/
				for (end = &dnsrq->servers, sp = dnsrq->servers;
				     sp; end = &sp->next, sp = sp->next) {
					if (sp->addr == saddr.sin_addr.s_addr) {
						*end = sp->next;
						dns_pushdown_server(sp);
						free(sp);
						break;
					}
				}
				if (! sp) {
					/*
					** Went through list and did not
					** find server.  May have run into
					** server bug so we bump retries.
					*/
					if (++dnsrq->retries >
					    dnsrq->max_tries) {
						dnsrq->retries = 0;
						sp = dnsrq->servers;
						dnsrq->servers = sp->next;
						dns_pushdown_server(sp);
						free(sp);
					}
				}
				if (dnsrq->servers == (server_t *)0) {
					/*
					** Tried all servers, so here we croak.
					*/
					nsd_logprintf(NSD_LOG_LOW,
					    "\tno more servers\n");
					rq->f_status = NS_NOTFOUND;
			    dns_free_request((dnsrequest_t **)&rq->f_cmd_data);
					return NSD_NEXT;
				}
				nsd_logprintf(NSD_LOG_LOW,
				    "\ttrying next server\n");
				return dns_query(rq);
			case FORMERR:
			default:
				nsd_logprintf(NSD_LOG_MIN, "\tunknown error\n");
				rq->f_status = NS_FATAL;
			    dns_free_request((dnsrequest_t **)&rq->f_cmd_data);
				return NSD_ERROR;
		}
	}

	/*
	** Success.  Setup response.  We don't support inverse queries.
	*/
	ancount = ntohs(hp->ancount);
	nsd_logprintf(NSD_LOG_MAX, "\t\t%d answers in response\n", ancount);
	qdcount = ntohs(hp->qdcount);
	cp = ansbuf + sizeof(HEADER);
	eom = ansbuf + n;
	if (qdcount) {
		cp += __dn_skipname(cp, eom) +
		    QFIXEDSZ;
	} else {
		nsd_logprintf(NSD_LOG_MIN, "\tno address in packet\n");
		if (hp->aa) {
			dns_free_request((dnsrequest_t **)&rq->f_cmd_data);
			rq->f_status = NS_NOTFOUND;
			return NSD_NEXT;
		} else {
			dns_free_request((dnsrequest_t **)&rq->f_cmd_data);
			rq->f_status = NS_TRYAGAIN;
			return NSD_NEXT;
		}
	}

	maxttl = nsd_attr_fetch_long(rq->f_attrs, "dns_max_ttl", 10, 0);

	ap = aliasbuf;
	while (--ancount >= 0 && cp < eom) {
		if ((n = dn_expand(ansbuf, eom, cp, tmpbuf, sizeof(tmpbuf)))
		    < 0) {
			nsd_logprintf(NSD_LOG_MIN, "\tfailed dn_expand 1\n");
			clearaddrs(&extra);
			dns_free_request((dnsrequest_t **)&rq->f_cmd_data);
			rq->f_status = NS_TRYAGAIN;
			return NSD_NEXT;
		}
		
		cp += n;
		GETSHORT(type, cp);
		cp += 2;
		GETLONG(ttl, cp);
		GETSHORT(len, cp);
		if ((len <= 0) || (len > 500)) {
			nsd_logprintf(NSD_LOG_MIN,
			    "\tillegal length found in packet\n");
			clearaddrs(&extra);
			dns_free_request((dnsrequest_t **)&rq->f_cmd_data);
			rq->f_status = NS_TRYAGAIN;
			return NSD_NEXT;
		}

		switch (type) {
		    case T_CNAME:
			/*
			** The cname is sitting in tmpbuf.
			*/
			n = strlen(tmpbuf);
			memcpy(ap, tmpbuf, n);
			nsd_logprintf(NSD_LOG_MAX,
			    "\t\tfound cname record: %*.*s\n", n, n, ap);
			ap[n++] = ' ';
			ap += n;
			cp += len;
			continue;
		    case T_PTR:
			/*
			** We only care about the first answer.
			**
			** This answer is a possibly compressed version
			** of the real name of the system.
			*/
			if ((n = dn_expand(ansbuf, eom, cp, namebuf,
			    sizeof(namebuf))) < 0) {
				nsd_logprintf(NSD_LOG_MIN,
				    "\tfailed dn_expand 3\n");
			    dns_free_request((dnsrequest_t **)&rq->f_cmd_data);
				clearaddrs(&extra);
				rq->f_status = NS_TRYAGAIN;
				return NSD_NEXT;
			}
			nsd_logprintf(NSD_LOG_MAX, "\t\tfound ptr record: %s\n",
			    namebuf);
			key = nsd_attr_fetch_string(rq->f_attrs, "key", 0);
			len = nsd_cat(result, sizeof(result), 4, key, "\t",
			    namebuf, "\n");
			dns_free_request((dnsrequest_t **)&rq->f_cmd_data);
			nsd_set_result(rq, NS_SUCCESS, result,
			    len, VOLATILE);
			if (maxttl) {
				if (ttl > maxttl) {
					ttl = maxttl;
				}
				n = _ltoa((long)ttl, tmpbuf);
				tmpbuf[n] = 0;
				nsd_attr_store(&rq->f_attrs, "timeout", tmpbuf);
			}
			clearaddrs(&extra);
			return NSD_OK;
		    case T_A:
			/*
			** This is the "next" A record for the host.  The
			** real name of the system is currently in tmpbuf,
			** we move this to namebuf for piecing back together
			** later.
			*/
			memcpy(namebuf, tmpbuf, sizeof(namebuf));

			/*
			** The thing in the RDATA should be an address
			** so we append it to the address list.  If this address
			** matches an address in the sortlist then we insert
			** it where dns_insortlist() to put it instead of 
			** appending to the end.
			*/
			addr = (struct addrlist *)nsd_malloc(sizeof(*addr));
			if (! addr) {
				nsd_logprintf(NSD_LOG_RESOURCE,
				    "dns_query_callback: malloc failed\n");
			    dns_free_request((dnsrequest_t **)&rq->f_cmd_data);
				rq->f_status = NS_TRYAGAIN;
				clearaddrs(&extra);
				return NSD_NEXT;
			}
			GETLONG(addr->addr.s_addr, cp);
			addrsort(addr, &extra);

			if (minttl > ttl) {
				minttl = ttl;
			}
			continue;
		    case T_TXT:
			/*
			** Simple text element so we just copy this result
			** and return.
			*/
			nsd_logprintf(NSD_LOG_MAX,
			    "\t\tfound text record: %*.*s\n", len, len, cp);
			memcpy(result, cp, len);
			result[len] = '\n';
			dns_free_request((dnsrequest_t **)&rq->f_cmd_data);
			nsd_set_result(rq, NS_SUCCESS, result, len + 1,
			    VOLATILE);
			if (maxttl) {
				if (ttl > maxttl) {
					ttl = maxttl;
				}
				n = _ltoa((long)ttl, tmpbuf);
				tmpbuf[n] = 0;
				nsd_attr_store(&rq->f_attrs, "timeout", tmpbuf);
			}
			clearaddrs(&extra);
			return NSD_OK;
		    default:
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "dns_query_callback: unknown response type (%d)\n",
			    type);
			cp += len;
		}
	}

	/*
	** Now we piece together the result from what we found in the
	** response.  The format we need to return is:
	**	address[ address]*\tname\talias[ alias]*
	*/
	cp = (uchar *)result;
	for (so = sortlist; so; so = so->next) {
		for (addr = so->sublist; addr; addr = so->sublist) {
			if (cp != (uchar *)result) {
				*cp++ = ' ';
			}
			so->sublist = addr->next;
			strcpy((char *)cp, inet_ntoa(addr->addr));
			cp += strlen((char *)cp);
			free(addr);
		}
	}
	for (addr = extra; addr; addr = extra) {
		if (cp != (uchar *)result) {
			*cp++ = ' ';
		}
		extra = addr->next;
		strcpy((char *)cp, inet_ntoa(addr->addr));
		cp += strlen((char *)cp);
		free(addr);
	}

	*cp++ = '\t';
	len = strlen(namebuf);
	memcpy(cp, namebuf, len);
	cp += len;

	len = ap - aliasbuf;
	if (len > 0) {
		*cp++ = '\t';
		memcpy(cp, aliasbuf, len);
		cp += len - 1;
	}
	*cp++ = '\n';

	dns_free_request((dnsrequest_t **)&rq->f_cmd_data);
	nsd_set_result(rq, NS_SUCCESS, result, (char *)cp - result,
	    VOLATILE);

	/*
	** Since the ttl is on each resource record, and we are piecing
	** together multiple records for this response, we use the minimum
	** ttl for the timeout.
	*/
	if (maxttl) {
		ttl = (minttl > maxttl) ? maxttl : minttl;
		n = _ltoa((long)ttl, tmpbuf);
		tmpbuf[n] = 0;
		nsd_attr_store(&rq->f_attrs, "timeout", tmpbuf);
	}

	nsd_logprintf(NSD_LOG_HIGH, "\tsuccess\n");
	return NSD_OK;
}

/*
** This routine will just send a request packet to the next address
** in the address list and sets up the callbacks for the response.
** We use this routine instead of search when the search path is not
** used.  Typically on address lookups, or lookups where there is a
** trailing dot on the key.
*/
static int
dns_query(nsd_file_t *rq)
{
	dnsrequest_t *dnsrq;
	u_char reqbuf[1024], *cp;
	char name[256];
	int buflen, len;
	struct sockaddr_in saddr;
	HEADER *hp;
	u_char *dnptrs[20], **dpp, **lastdnptr;
	server_t *sp;

	dnsrq = (dnsrequest_t *)rq->f_cmd_data;

	/*
	** Format a query.
	*/
	memset(reqbuf, 0, sizeof(reqbuf));
	hp = (HEADER *)reqbuf;
	hp->id = dnsrq->xid;
	hp->opcode = QUERY;
	hp->rd = 1;
	hp->rcode = NOERROR;
	cp = reqbuf + sizeof(HEADER);
	buflen = sizeof(reqbuf) - sizeof(HEADER);
	dpp = dnptrs;
	*dpp++ = reqbuf;
	*dpp++ = NULL;
	lastdnptr = dnptrs + sizeof(dnptrs)/sizeof(dnptrs[0]);

	strcpy(name, dnsrq->key);
	if (dnsrq->domains) {
		strcat(name, dnsrq->domains->name);
	}

	nsd_logprintf(NSD_LOG_MIN, "dns_query: trying for name %s\n", name);

	if ((len = dn_comp(name, cp, buflen, dnptrs, lastdnptr)) < 0) {
		nsd_logprintf(NSD_LOG_OPER, 
			      "dns_query: unable to compress domain name %s\n",
			      name);
		rq->f_status = NS_NOTFOUND;
		dns_free_request((dnsrequest_t **)&rq->f_cmd_data);
		return NSD_NEXT;
	}

	cp += len, buflen -= len;
	PUTSHORT(dnsrq->type, cp);
	PUTSHORT(dnsrq->class, cp);
	hp->qdcount = htons(1);

	len = cp - reqbuf;

	/*
	** Send the query to the current server address.
	*/
	if (! dnsrq->servers) {
		/*
		** Tried all servers, so here we croak.
		*/
		rq->f_status = NS_NOTFOUND;
		dns_free_request((dnsrequest_t **)&rq->f_cmd_data);
		return NSD_NEXT;
	}

	saddr.sin_family = AF_INET;
	saddr.sin_port = NAMESERVER_PORT;
	for (sp = dnsrq->servers; sp; sp = sp->next) {
		saddr.sin_addr.s_addr = sp->addr;

		nsd_logprintf(NSD_LOG_MIN,
		    "dns_query: sending request %lu to %s\n", hp->id,
		    inet_ntoa(saddr.sin_addr));

		if (sendto(_dns_sock, reqbuf, len, 0, &saddr,
		    sizeof(struct sockaddr_in)) != len) {
			if (sp->next) {
				nsd_logprintf(NSD_LOG_LOW,
				    "dns_query: failed sendto %s: %s\n",
				    inet_ntoa(saddr.sin_addr), strerror(errno));
				dns_pushdown_server(sp);
				continue;
			} else {
				dns_free_request((dnsrequest_t **)
						 &rq->f_cmd_data);
				nsd_logprintf(NSD_LOG_RESOURCE, 
				     "dns_query: failed final sendto %s: %s\n",
					      inet_ntoa(saddr.sin_addr), 
					      strerror(errno));
				rq->f_status = NS_TRYAGAIN;
				return NSD_NEXT;
			}
		}
		if (! (dnsrq->flags & DNS_PARALLEL)) {
			break;
		}
	}

	/*
	** Setup a timeout and save our xid.
	*/
	rq->f_cmd_data = (nsd_file_t *)dnsrq;
	dns_xid_new(dnsrq->xid, rq);
	nsd_timeout_new(rq, dnsrq->timeout, dns_query_timeout, (void *)0);

	return NSD_CONTINUE;
}

/*
** This routine will format a query for DNS, send it off to named
** as determined by /etc/resolv.conf, and setup the callbacks for
** the results.
*/
int
lookup(nsd_file_t *rq)
{
	dnsrequest_t *dnsrq;
	char *p, *q, *table, *domain, *key, *servers, *search;
	u_int32_t addr;
	domain_t *dp=0;
	int ndots = 0;
	struct stat sbuf;

	/*
	** Firewall
	*/
	if (! rq) {
		return NSD_ERROR;
	}

	/*
	** Reread resolv.conf if needed.
	*/
	if (stat(_PATH_RESCONF, &sbuf) < 0) {
		if (_dns_restime && dns_config() != NSD_OK) {
			rq->f_status = NS_NOTFOUND;
			return NSD_NEXT;
		}
	} else if (sbuf.st_mtime > _dns_restime) {
		if (dns_config() != NSD_OK) {
			rq->f_status = NS_NOTFOUND;
			return NSD_NEXT;
		}
	}

	domain = nsd_attr_fetch_string(rq->f_attrs, "domain", 0);

	table = nsd_attr_fetch_string(rq->f_attrs, "table", 0);
	key = nsd_attr_fetch_string(rq->f_attrs, "key", 0);
	if (! table || ! key) {
		if (dp) free(dp);
		rq->f_status = NS_BADREQ;
		return NSD_ERROR;
	}

	servers = nsd_attr_fetch_string(rq->f_attrs, "dns_servers", 0);
	search = nsd_attr_fetch_string(rq->f_attrs, "dns_search", 0);

	dnsrq = (dnsrequest_t *)nsd_calloc(1, sizeof(dnsrequest_t));
	if (! dnsrq) {
		nsd_logprintf(NSD_LOG_RESOURCE, "lookup (dns): failed malloc\n");
		if (dp) free(dp);
		rq->f_status = NS_TRYAGAIN;
		return NSD_NEXT;
	}
	rq->f_cmd_data = (nsd_file_t *)dnsrq;

	dnsrq->max_tries = (int)nsd_attr_fetch_long(rq->f_attrs, "dns_retries",
	    10, 3);
	dnsrq->timeout = (int)nsd_attr_fetch_long(rq->f_attrs, "dns_timeout",
	    10, 2000);
	if (dnsrq->timeout < 0) {
		dnsrq->timeout=0;
	}
	if (nsd_attr_fetch_bool(rq->f_attrs, "dns_parallel", 0)) {
		dnsrq->flags |= DNS_PARALLEL;
	}
	dnsrq->xid = ++id_count;

	if (servers) {
		dnsrq->servers = dns_servers_parse(servers);
	}
	if (! dnsrq->servers) {
		dnsrq->servers = dns_servers();
	}
	
	/*
	** Determine DNS query type.
	*/

	nsd_logprintf(NSD_LOG_LOW,
	    "\tdomain = %s, table = %s, key = %s\n", domain, table, key);

	if (strcasecmp(table, "hosts.byname") == 0) {
		dnsrq->type = T_A;
		dnsrq->class = C_IN;
		strncpy(dnsrq->key, key, MAXDNAME-1);
		dnsrq->key[MAXDNAME - 1] = 0;
	}
	else if (strcasecmp(table, "hosts.byaddr") == 0) {
		dnsrq->type = T_PTR;
		dnsrq->class = C_IN;

		/*
		** We have to convert the name into a in-addr.arpa
		** form for the lookup.
		*/
		addr = inet_addr(key);
		snprintf(dnsrq->key, sizeof(dnsrq->key),
		    "%u.%u.%u.%u.in-addr.arpa", (addr & 0xff),
		    (addr & 0xff00) >> 8, (addr & 0xff0000) >> 16,
		    (addr & 0xff000000) >> 24);
	}
	else if (strcasecmp(table, "mx") == 0) {
		dnsrq->type = T_MX;
		dnsrq->class = C_IN;
		strncpy(dnsrq->key, key, MAXDNAME-1);
	}
	else {
		dnsrq->type = T_TXT;
		dnsrq->class = C_HS;
		strcpy(dnsrq->key, key);
		for (p = dnsrq->key; *p; *p++);
		*p++ = '.';
		for (q = key; *q; *p++, *q++) {
			*p = (*q == '.') ? '_' : *q;
			if (isupper(*p)) {
				*p = tolower(*p);
			}
		}
		*p++ = '.';
		*p = (char)0;
	}

	/*
	** Check for trailing dot.  If we have dots then we also
	** try just the null domain.  The number of dots can be controlled
	** from resolv.conf.  If we are passed in a search directive then
	** we use that instead of what is in resolv.conf.
	*/
	for (p = dnsrq->key; *p; p++) {
		if (*p == '.') {
			ndots++;
		}
	}
	if ((*(--p) != '.') && (dnsrq->type != T_PTR)) {
		domain_t **tdp;

		if (domain) {
			dnsrq->domains = dns_domain_parse(domain);
		} else if (search) {
			dnsrq->domains = dns_search_parse(search);
		}
		if (! dnsrq->domains) {
			dnsrq->domains = dns_domains();
		}
		
		dp = (domain_t *)nsd_calloc(1, sizeof(domain_t));
		if (dp) {
			dp->name[0] = '.';
			dp->name[1] = (char)0;
		}
		if (ndots >= _dns_ndots) {
			/* prepend to domain list */
			dp->next = dnsrq->domains;
			dnsrq->domains = dp;
		} else {
			/* append to domain list */
			for (tdp = &dnsrq->domains; *tdp; tdp = &(*tdp)->next);
			*tdp = dp;
		}
	}
	return dns_query(rq);
}
