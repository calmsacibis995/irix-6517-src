#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/endian.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <net/if.h>
#include <net/soioctl.h>
#include <ctype.h>
#include <assert.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include "ns_nis.h"
#include "htree.h"

extern nisstring_t _nis_domain;
extern nisstring_t _nis_host;
extern int _nis_sock;
extern int _nis_sock_priv_port;
extern time_t _nis_last_attempt;
extern u_int32_t _nis_euid, _nis_egid;
extern char _nis_result[4096];
extern int _nis_xid;

static int nis_match(nsd_file_t *);
static int nis_loop(nsd_file_t *);

extern int _ltoa(long, char *);

/*
** This is the timeout handler.  If we are called then a request timed out
** so we delete the timeout, and the xid, and retry.  If we timed out
** then we assume that our binding went away, and so try to rebind.
*/
int
nis_match_timeout(nsd_file_t **rqp, nsd_times_t *to)
{
	nsd_file_t *nrq, *rq;
	nsd_callout_proc *cp;
	nisrequest_t *nisrq;

	nsd_logprintf(NSD_LOG_MIN, "entering nis_match_timeout:\n");

	rq = to->t_file;
	assert(rq);
	*rqp = rq;
	nisrq = rq->f_cmd_data;
	assert(nisrq);

	/*
	** Remove the timeout, and the callback.
	*/
	nsd_timeout_remove(rq);
	nis_xid_lookup(nisrq->xid, &nrq, &cp);

	/*
	** The match failed so we assume that our server went away.
	*/
	if (nisrq->server &&
	    (nisrq->xid > nisrq->server->bound_xid)) {
                nisrq->server->sin.sin_port = 0;
		nisrq->server->sin.sin_addr.s_addr = 0;
	} else {
		nsd_logprintf(NSD_LOG_OPER, "nis_match_timeout request %d is older than binding id %d\n", nisrq->xid, nisrq->server->bound_xid);
	}

	/*
	** Try again.
	*/
	if (nisrq->proc) {
		return (*nisrq->proc)(rq);
	} else {
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_ERROR:\n");
		free(nisrq), rq->f_cmd_data = (void *)0;
		return NSD_ERROR;
	}
}

/*
** This is the callback handler for a request.  If we are called then
** it means we have a response waiting on our incoming file descriptor.
** First we need to remove the timeout from the list, then setup our
** result, and return.
*/
static int
nis_match_callback(nsd_file_t *rq)
{
	nisrequest_t *nisrq = (nisrequest_t *)rq->f_cmd_data;
	u_int32_t *p;
	u_int32_t len, status;
	char *c;

	nsd_logprintf(NSD_LOG_LOW, "entering nis_match_callback:\n");

	/*
	** The following is the structure that we expect to get back
	** from the server.  We decode this incrementally since there
	** are variable length elements in the middle of it, etc.
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
					u_int32_t	status;
					struct {
						u_int32_t	len;
						char		bytes[len];
					} ru;
				} ypresp;
			} accept;
		} rm_resp;
	    } rpc;
	} msg;
	*/

	/*
	** Decode the packet parameters that we care about.
	** The fields that we really care about are the xid and
	** the port number.
	*/
	p = (u_int32_t *)_nis_result;
	if (ntohl(p[0]) != nisrq->xid) {
		nis_xid_new(nisrq->xid, rq, nis_match_callback);
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_CONTINUE:\n");
		return NSD_CONTINUE;
	}
	if (ntohl(p[1]) != REPLY) {
		nis_xid_new(nisrq->xid, rq, nis_match_callback);
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_CONTINUE:\n");
		return NSD_CONTINUE;
	}
	status = ntohl(p[2]);
	if (status != MSG_ACCEPTED) {
		nisrq->server->sin.sin_port = 0;
		nisrq->server->sin.sin_addr.s_addr = 0;
		nsd_timeout_remove(rq);
		return (*nisrq->proc)(rq);
	}

	p += 5 + p[4]/sizeof(u_int32_t);

	status = ntohl(p[0]);
	if (status != SUCCESS) {
		nsd_logprintf(NSD_LOG_HIGH, "\tstatus != success\n");
		rq->f_status = NS_TRYAGAIN;
		goto match_error;
	}

	status = ntohl(p[1]);
	if (status != YP_TRUE) {
		if ((status == YP_NOMORE) || (status == YP_NOKEY)) {
			if (strcasecmp(nisrq->map.string,
			    "group.bymember") != 0) {
				if (strncasecmp(nisrq->map.string,
				    "netgroup.", sizeof("netgroup.")-1) == 0) {
					int check = 0;

					/*
					** Special hack for netgroup.by* case
					** insensitivity in nis domainname.
					*/
nsd_logprintf(NSD_LOG_MIN, "retrying lowercase\n");
					for (c = nisrq->key.string; *c; c++) {
						if (isupper(*c)) {
							check = 1;
						}
						*c = tolower(*c);
					}
					if (check) {
						/*
						** Retry request with lowercase
						** key.
						*/
						nsd_timeout_remove(rq);
						return (*nisrq->proc)(rq);
					}
				}

				nsd_logprintf(NSD_LOG_HIGH,
				    "\tstatus = nomore or nokey\n");
				rq->f_status = NS_NOTFOUND;
			}
		}
		else if (status == YP_NOMAP) {
			/*
			** Ugly hack.  If we have workarounds for missing
			** maps then we forward the request to the workaround 
			** and remember that this server doesn't know about
			** the map.
			*/
			if (nisrq->nomap_proc) {
				/*
				** Remember that this server doesn't know this
				** map. 
				*/
				if (!strcasecmp(nisrq->map.string, 
						"group.bymember")) {
					nisrq->server->flags |= NO_GRP_BYMBR;
				} 
				else if (!strcasecmp(nisrq->map.string, 
						     "rpc.byname")){
					nisrq->server->flags |= NO_RPC_BYNAME;
				}
				else if (!strcasecmp(nisrq->map.string, 
						     "services")) {
					nisrq->server->flags |= NO_SERVICES;
					if ((nisrq->server->flags &
					    NO_SERVICES2) || 
					    (nsd_attr_fetch_bool(rq->f_attrs,
					    "no_pseudo_maps", FALSE))){
						nisrq->nomap_proc = nis_loop;
					} else {
						nisrq->map.len =
					    sizeof("services.byservicename")-1;
						memcpy(nisrq->map.string,
						    "services.byservicename",
						    nisrq->map.len);
						nisrq->map.string[
						    nisrq->map.len] = 0;
						nisrq->map.words =
						    WORDS(nisrq->map.len);
					}
				}
				else if (!strcasecmp(nisrq->map.string,
				    "services.byservicename")) {
					nisrq->server->flags |= NO_SERVICES2;
					nisrq->nomap_proc = nis_loop;
				}
				else if (!strncasecmp(nisrq->map.string,
				    "netgroup", sizeof("netgroup") - 1)) {
					nisrq->server->flags |= NO_NETGROUP;
				}
				nsd_timeout_remove(rq);
				return (*nisrq->nomap_proc)(rq);
			}
			nsd_logprintf(NSD_LOG_HIGH, "\tstatus = nomap\n");
			rq->f_status = NS_UNAVAIL;
		}
		else {
			nsd_logprintf(NSD_LOG_HIGH, "\tstatus != yptrue\n");
			rq->f_status = NS_TRYAGAIN;
		}
		goto match_error;
	}

	len = ntohl(p[2]);
	if (len > YPMAXRECORD) {
		nsd_logprintf(NSD_LOG_HIGH, "\tlen > YPMAXRECORD\n");
		rq->f_status = NS_TRYAGAIN;
		goto match_error;
	}

	p += 3;

	/*
	** Remove the timeout associated with this request.
	*/
	nsd_timeout_remove(rq);

	memcpy(_nis_result, p, len);

	if (strcasecmp(nisrq->map.string, "group.bymember") == 0) {
		/* special hack for group.bymember we loop through all */
		if (rq->f_data && (rq->f_used > 0)) {
			_nis_result[len] = (char)0;
			c = strchr(_nis_result, ':');
			if (c) {
				nsd_append_result(rq, NS_SUCCESS, ++c,
				    len - (c - _nis_result));
			}
		} else {
			nsd_set_result(rq, NS_SUCCESS, _nis_result, len,
			    VOLATILE);
		}
		if (rq->f_data && (rq->f_used > 0) &&
		    (rq->f_data[rq->f_used - 1] != ',')) {
			nsd_append_result(rq, NS_SUCCESS, ",", 1);
		}
		free(nisrq), rq->f_cmd_data = (void *)0;
		return NSD_NEXT;
	}

	if (_nis_result[len - 1] != '\n') {
		_nis_result[len++] = '\n';
	}

	/* Add the key if we were told to. */
	if (nsd_attr_fetch_bool(rq->f_attrs, "nis_enumerate_key", FALSE)) {
	  char *sep = nsd_attr_fetch_string(rq->f_attrs, "separator", " ");

	  nsd_append_result(rq, NS_SUCCESS, nisrq->key.string, nisrq->key.len);
	  nsd_append_result(rq, NS_SUCCESS, sep, strlen(sep));
	} 

	nsd_append_result(rq, NS_SUCCESS, _nis_result, len);


	/*
	** Free up the nis specific request data.
	*/
	free(nisrq), rq->f_cmd_data = (void *)0;
	
	nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_OK:\n");
	return NSD_OK;

match_error:
	nsd_timeout_remove(rq);

	nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_NEXT:\n");
	free(nisrq), rq->f_cmd_data = (void *)0;
	return NSD_NEXT;
}

/*
** This routine will parse a services style record looking for a matching
** name/protocol line.
*/
static int
check_services(nsd_file_t *rq, char *buf)
{
	char *key, *proto, *p, *q;
	int klen, plen, len;
	nisrequest_t *nisrq = (nisrequest_t *)rq->f_cmd_data;

	/*
	** The key currently looks like:
	**	name/proto
	*/
	key = nisrq->key.string;
	proto = strchr(key, '/');
	if (proto) {
		*proto++ = 0;
		plen = strlen(proto);
	}
	klen = strlen(key);
	
	for (p = buf; p && *p; p = strchr(p + 1, '\n')) {
		for (; *p && isspace(*p); p++);
		if (! *p || *p == '#') continue;
		
		/* Check protocol. */
		for (q = p; *q && ! isspace(*q); q++);
		for (; *q && (*q == ' ' || *q == '\t'); q++);
		for (; *q && ! isspace(*q) && (*q != '/'); q++);
		if (*q++ != '/') continue;
		if (proto && (strncasecmp(proto, q, plen) != 0)) continue;

		/* Look for name. */
		for (q = p; q && *q && *q != '\n'; q = strpbrk(q, " \t\n#")) {
			for (; *q && (*q == ' ' || *q == '\t'); q++);
			if (! *q || *q == '#') break;
			len = strcspn(q, " \t\n#");
			if (len == klen) {
				if (strncasecmp(key, q, len) == 0) {
					len = strcspn(p, "#\n");
					nsd_set_result(rq, NS_SUCCESS, p, len,
					    VOLATILE);
					nsd_append_result(rq, NS_SUCCESS, "\n",
					    sizeof("\n"));
					return NSD_OK;
				}
			}
		}
	}
	
	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

/*
** This routine will walk through a group record appending an entry for
** each group the requested login appears in.
*/
static int
check_group(nsd_file_t *rq, char *buf)
{
	char *p, *q;
	int len;
	nisrequest_t *nisrq = (nisrequest_t *)rq->f_cmd_data;
	
	if (! rq->f_data) {
		nsd_set_result(rq, NS_SUCCESS, nisrq->key.string,
		    nisrq->key.len, STATIC);
		nsd_append_result(rq, NS_SUCCESS, ":", sizeof(":"));
	}

	for (p = strstr(buf, nisrq->key.string); p && *p;
	    p = strstr(p + 1, nisrq->key.string)) {
		q = p - 1;
		if (! (q < buf || isspace(*q) || *q == ',' || *q == ':') ||
		    ! (isspace(p[nisrq->key.len]) || p[nisrq->key.len] == ',' ||
		    p[nisrq->key.len] == 0)) continue;
		
		/*
		** Backup to beginning of line.
		*/
		for (q = p; (q > buf) && *q && (*q != '\n'); --q);
		
		/*
		** Skip if we are in a comment.
		*/
		for (; *q && isspace(*q); q++);
		if (*q == '#') {
			continue;
		}
		
		/*
		** Get gid.
		*/
		for (q++; *q && (*q != ':') && (*q != '\n'); q++);
		for (q++; *q && (*q != ':') && (*q != '\n'); q++);
		if (! *q || (*q == '\n') || (q > p)) continue;
		
		/*
		** Append to record.
		*/
		len = strcspn(++q, ": \t\n");
		nsd_append_result(rq, NS_SUCCESS, q, len);
		nsd_append_result(rq, NS_SUCCESS, ",", len);
	}
	
	return NSD_NEXT;
}

/*
** This routine will parse a rpc style record looing for an rpc name.
*/
static int
check_rpc(nsd_file_t *rq, char *buf)
{
	char *p, *q;
	int len;
	nisrequest_t *nisrq = (nisrequest_t *)rq->f_cmd_data;
	
	nsd_logprintf(NSD_LOG_MIN, "check_rpc:\n");

	for (p = buf; p && *p; p = strchr(p + 1, '\n')) {
		for (; *p && isspace(*p); p++);
		if (! *p || *p == '#') continue;
		
		for (q = p; q && *q && *q != '\n'; q = strpbrk(q, " \t\n#")) {
			for (; *q && (*q == ' ' || *q == '\t'); q++);
			if (! *q || *q == '#') break;
			len = strcspn(q, " \t\n#");
			if (len == nisrq->key.len) {
				if (memcmp(q, nisrq->key.string, len) == 0) {
					len = strcspn(p, "\n");
					nsd_set_result(rq, NS_SUCCESS, p, len,
					    VOLATILE);
					nsd_append_result(rq, NS_SUCCESS, "\n",
					    sizeof("\n"));
					return NSD_OK;
				}
			}
		}
	}
	
	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

/*
** This is ugly.  We parse the entire netgroup file into a couple of
** hash files and then lookup the original request in these.
*/
static int
check_netgroup(nsd_file_t *rq, char *buf)
{
	nisrequest_t *nisrq = (nisrequest_t *)rq->f_cmd_data;
	ht_node_t *n;

	nsd_logprintf(NSD_LOG_MIN, "check_netgroup:\n");

	if (nis_netgroup_parse(buf, &nisrq->server->netgroup)) {
		if (strcasecmp(nisrq->map.string, "netgroup.byhost") == 0) {
			n = ht_lookup(&nisrq->server->netgroup.byhost,
			    nisrq->key.string, nisrq->key.len, strncmp);
		} else {
			n = ht_lookup(&nisrq->server->netgroup.byuser,
			    nisrq->key.string, nisrq->key.len, strncmp);
		}
		if (n) {
			nsd_set_result(rq, NS_SUCCESS, DS_STRING(&n->val),
			    DS_LEN(&n->val), VOLATILE);
			return NSD_OK;
		}
	}
	
	rq->f_status = NS_NOTFOUND;
	return NSD_NEXT;
}

/*
** This is the callback handler for a request.  If we are called then
** it means we have a response waiting on our incoming file descriptor.
** First we need to remove the timeout from the list, then setup our
** result, and return.
*/
static int
nis_loop_callback(nsd_file_t *rq)
{
	int status;
	nisrequest_t *nisrq = (nisrequest_t *)rq->f_cmd_data;

	nsd_logprintf(NSD_LOG_MIN, "entering nis_loop_callback:\n");

	status = NSD_NEXT;
	if (rq->f_loop) {
		if ((rq->f_loop->f_status == NS_SUCCESS) &&
		    rq->f_loop->f_data) {
			if (strncasecmp(nisrq->map.string,
			    "services", sizeof("services") - 1) == 0) {
				status = check_services(rq, rq->f_loop->f_data);
			} else if (strncasecmp(nisrq->map.string,
			    "group", sizeof("group") - 1) == 0) {
				status = check_group(rq, rq->f_loop->f_data);
			} else if (strncasecmp(nisrq->map.string,
			    "rpc", sizeof("rpc") - 1) == 0) {
				status = check_rpc(rq, rq->f_loop->f_data);
			} else if (strncasecmp(nisrq->map.string,
			    "netgroup", sizeof("netgroup") - 1) == 0) {
				status = check_netgroup(rq, rq->f_loop->f_data);
			} else {
				nsd_logprintf(NSD_LOG_LOW,
				    "\treturning NSD_ERROR:\n");
				nsd_logprintf(NSD_LOG_HIGH,
				    "\t\tmap name: %s\n", nisrq->map.string);
				free(nisrq), rq->f_cmd_data = (void *)0;
				return NSD_ERROR;
			}
		}
		nsd_file_clear(&rq->f_loop);
	}
	free(nisrq), rq->f_cmd_data = (void *)0;

	return status;
}

/*
** This routine builds up a packet for the request if one does
** not already exist, then spits it out the socket to the server.
** We also setup the reply callback here, since we have all the
** socket information available.
*/
static int
nis_send_request(nsd_file_t *rq)
{
	nisrequest_t *nisrq = (nisrequest_t *)rq->f_cmd_data;
	u_int32_t *p;
	int len,i;
	long timeout;
	char *q;
	int sock;

	nsd_logprintf(NSD_LOG_MIN, "entering nis_send_request:\n");

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
	    struct ypreq_key {
		struct {
		    u_int32_t	len;
		    char	string[RNDUP(len)];
		} domain;
		struct {
		    u_int32_t	len;
		    char	string[RNDUP(len)];
		} map;
		struct {
		    u_int32_t	len;
		    char	bytes[RNDUP(len)];
		} datum;
	    } key;
	} msg;
	*/

	/*
	** This is the equivelant to the code used in the old libc
	** routine to build a big message to send over the wire.
	** It was done with a few dozen calls to rpc and xdr routines,
	** but since that is all copyrighted by Sun, this has been
	** reverse engineered.  Ugly, but . . .
	*/
	nisrq->xid = ++_nis_xid;
	p = (uint32_t *)_nis_result;
	*p++ = htonl(nisrq->xid);
	*p++ = htonl(CALL);
	*p++ = htonl(RPC_MSG_VERSION);
	*p++ = htonl(YPPROG);
	*p++ = htonl(YPVERS);
	*p++ = htonl(YPPROC_MATCH);

	/* auth */
#if 0
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
#else
	*p++ = 0;
	*p++ = 0;
#endif

	/* verf */
	*p++ = 0;
	*p++ = 0;

	/* nis req */
	*p++ = htonl(nisrq->domain.len);
	p[nisrq->domain.len/sizeof(*p)] = 0;
	memcpy(p, nisrq->domain.string, nisrq->domain.len);
	p += nisrq->domain.words;

	*p++ = htonl(nisrq->map.len);
	p[nisrq->map.len/sizeof(*p)] = 0;
	memcpy(p, nisrq->map.string, nisrq->map.len);
	p += nisrq->map.words;

	*p++ = htonl(nisrq->key.len);
	p[(nisrq->key.len)/sizeof(*p)] = 0;
	/*
	** Requests for some NIS maps are case insensitive.
	** We have to lower case them.  Netgroups are complex.  We always
	** send them with given case first then retry lowercase if that
	** fails.
	*/
	if (nsd_attr_fetch_bool(rq->f_attrs, "casefold", FALSE) &&
	    (strncasecmp(nisrq->map.string, "netgroup.",
	     sizeof("netgroup.") - 1) != 0)) {
nsd_logprintf(NSD_LOG_MIN, "casefold\n");
		q = (char *)p;
		for (i=0;i< nisrq->key.len;i++) {
			*q++ = tolower(nisrq->key.string[i]);
		}
	} else {
		memcpy(p, nisrq->key.string, nisrq->key.len);
	}
	nisrq->key.words = WORDS(nisrq->key.len);
	p += nisrq->key.words; 

	nsd_logprintf(NSD_LOG_MAX, "key: %s, len: %d, words: %d\n",
		      nisrq->key.string, nisrq->key.len, nisrq->key.words);
	nsd_logprintf(NSD_LOG_LOW, "\tsending request for: %s %s %s to %s\n",
	    nisrq->domain.string, nisrq->map.string, nisrq->key.string,
	    inet_ntoa(nisrq->server->sin.sin_addr));

	len = (char *)p - _nis_result;

#if 0
	if ( nsd_attr_fetch_bool(rq->f_attrs, "nis_secure", FALSE)
	     && ( _nis_sock_priv_port != -1 ) )
            sock = _nis_sock_priv_port;
	else
#endif
	    sock = _nis_sock;

	if (sendto(sock, _nis_result, len, 0, &nisrq->server->sin,
	    sizeof(struct sockaddr_in)) != len) {
		rq->f_status = NS_TRYAGAIN;
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_NEXT:\n");
		free(nisrq), rq->f_cmd_data = (void *)0;
		return NSD_NEXT;
	}

	/*
	** Setup the callback for the reply.
	*/
	nis_xid_new(nisrq->xid, rq, nis_match_callback);
	timeout = 1000 * nsd_attr_fetch_long(rq->f_attrs, "nis_timeout", 10,1);
	nsd_timeout_new(rq, timeout, nis_match_timeout, (void *)0);

	nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_CONTINUE:\n");
	return NSD_CONTINUE;
}

/*
** This routine will loop through the given map until the requested key
** is found or we get a null return.
*/
static int
nis_loop(nsd_file_t *rq)
{
	int status;
	nsd_loop_t l = {0};
	nisrequest_t *nisrq = (nisrequest_t *)rq->f_cmd_data;

	nsd_logprintf(NSD_LOG_MIN, "entering nis_loop:\n");

	l.lb_flags = NSD_LOOP_FIRST;
	l.lb_proc = nis_loop_callback;

	/*
	** These are maps that are not supported by earlier versions
	** of NIS so we fake them by looping through another map.
	*/
	nisrq->nomap_proc = 0;
	if (strncasecmp(nisrq->map.string, "rpc.byname",
	    nisrq->map.len) == 0) {
		l.lb_table = "rpc.bynumber";
		nisrq->proc = nis_loop;
	}
	else if (strncasecmp(nisrq->map.string, "group.bymember",
	    nisrq->map.len) == 0) {
		l.lb_table = "group.byname";
		nisrq->proc = nis_loop;
	}
	else if (strncasecmp(nisrq->map.string, "services",
	    sizeof("services") - 1) == 0) {
		l.lb_table = "services.byname";
		nisrq->proc = nis_loop;
	}
	else if (strncasecmp(nisrq->map.string, "netgroup",
	    sizeof("netgroup") - 1) == 0) {
		l.lb_table = "netgroup";
		l.lb_attrs = "nis_enumerate_key";
		nisrq->proc = nis_loop;
	}
	
	/*
	** Send the request.
	*/
	status = nsd_loopback(rq, &l);
	switch (status) {
	    case NSD_OK:
		return nis_loop_callback(rq);
	    case NSD_CONTINUE:
		return NSD_CONTINUE;
	    case NSD_ERROR:
	    default:
		return NSD_ERROR;
	}
}

/*
** This routine is called when a nis request comes in and the domain is
** not bound.  This routine will make a nis broadcast, setup a timeout,
** then continue.
*/
static int
nis_match(nsd_file_t *rq)
{
	nisrequest_t *nisrq = (nisrequest_t *)rq->f_cmd_data;
	time_t now;
	long timeout;

	nsd_logprintf(NSD_LOG_MIN, "entering nis_match:\n");

	/*
	** Bump the counter, and return an error if we have
	** tried too many times.
	*/
	if (--(nisrq->count) <= 0) {
		free(nisrq), rq->f_cmd_data = (void *)0;
		rq->f_status = NS_UNAVAIL;
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_ERROR:\n");
		return NSD_NEXT;
	}

	/*
	** If we are not bound we attempt to rebind.
	*/
	if (nisrq->server->sin.sin_port) {
		/*
		** Send the request.
		*/
		return nis_send_request(rq);
	} else {
		time(&now);
		if (now > nisrq->server->again) {
			/*
			** Send a bind broadcast.
			*/
			nisrq->server->again = now + BIND_TIME;
			return nis_bind_broadcast(rq);
		} else {
			/*
			** New request, still waiting for bind
			** to complete.
			*/
			timeout = 1000 * nsd_attr_fetch_long(rq->f_attrs, 
						      "nis_timeout", 10, 1);
			nsd_timeout_new(rq, timeout, nis_match_timeout,
			    (void *)nisrq);
		}
	}

	nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_CONTINUE:\n");
	return NSD_CONTINUE;
}


int
lookup(nsd_file_t *rq)
{
	nisrequest_t *nisrq;
	char *map, *key, *domain;
	int pseudo = 0;

	nsd_logprintf(NSD_LOG_MIN, "entering lookup (nis):\n");

	nisrq = (nisrequest_t *)nsd_calloc(1, sizeof(nisrequest_t));
	if (! nisrq) {
		rq->f_status = NS_TRYAGAIN;
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_NEXT:\n");
		return NSD_NEXT;
	}
	rq->f_cmd_data = (nsd_file_t *)nisrq;
	nisrq->count = nsd_attr_fetch_long(rq->f_attrs, "nis_retries", 10, 5);

	/*
	** The second two arguments in the request argv are "map" and "key".
	** These are the only two we care about.
	**
	** We round the string lengths up to the next word boundary here
	** since that is how they will be used later.
	*/
	map = nsd_attr_fetch_string(rq->f_attrs, "table", (char *)0);
	if (! map) {
		rq->f_status = NS_BADREQ;
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_ERROR - table:\n");
		free(nisrq), rq->f_cmd_data = (void *)0;
		return NSD_ERROR;
	}
	nisrq->map.len = strlen(map);
	memcpy(nisrq->map.string, map, nisrq->map.len);
	nisrq->map.string[nisrq->map.len] = 0;
	nisrq->map.words = WORDS(nisrq->map.len);

	key = nsd_attr_fetch_string(rq->f_attrs, "key", (char *)0);
	if (! key) {
		rq->f_status = NS_BADREQ;
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_ERROR - key:\n");
		free(nisrq), rq->f_cmd_data = (void *)0;
		return NSD_ERROR;
	}

	/* 
	** the mail.aliases map requires a null to be 
	** accounted for in len.   This is not in the protocol, 
	** but in the client programs.
	*/
	nisrq->key.len = strlen(key);
	if (nsd_attr_fetch_bool(rq->f_attrs, "null_extend_key", FALSE)) {
		nisrq->key.len++;
	}
	memcpy(nisrq->key.string, key, nisrq->key.len);
	nisrq->key.string[nisrq->key.len] = 0;
	nisrq->key.words = WORDS(nisrq->key.len);

	domain = nsd_attr_fetch_string(rq->f_attrs, "domain",
	    _nis_domain.string);
	if (! *domain) {
		rq->f_status = NS_UNAVAIL;
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_NEXT:\n");
		free(nisrq), rq->f_cmd_data = (void *)0;
		return NSD_NEXT;
	}
	nisrq->domain.len = strlen(domain);
	memcpy(nisrq->domain.string, domain, nisrq->domain.len);
	nisrq->domain.string[nisrq->domain.len] = 0;
	nisrq->domain.words = WORDS(nisrq->domain.len);

	nisrq->server = nis_domain_server(nisrq->domain.string);
	if (! nisrq->server) {
		rq->f_status = NS_TRYAGAIN;
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_NEXT:\n");
		free(nisrq), rq->f_cmd_data = (void *)0;
		return NSD_NEXT;
	}

	/*
	** Set the default loopup handler. 
	*/
	nisrq->proc = nis_match;

	/*
	** There are a number of maps that did not exist in NIS, but were
	** added later for performance.  For these we first try to look
	** them up with the new name, and if that fails we fall back into
	** a workaround routine.
	*/
	pseudo = nsd_attr_fetch_bool(rq->f_attrs, "no_pseudo_maps", FALSE);
	if (!(strncasecmp(nisrq->map.string, "rpc.byname", nisrq->map.len))) {
		/* 
		** If we've been asked not to lookup a pseduo map or we know
		** that the current server doesn't know about this map, 
		** skip right to the workaround.
		*/
		if ((nisrq->server->flags & NO_RPC_BYNAME) || pseudo) {
			nisrq->proc = nis_loop;
		}
		nisrq->nomap_proc = nis_loop;
	} 
	else if (!(strcasecmp(nisrq->map.string,"services"))) {
		if ((nisrq->server->flags & NO_SERVICES) || pseudo) {
			if ((nisrq->server->flags & NO_SERVICES2) || 
			    pseudo) {
				nisrq->proc = nis_loop;
			} else {
				/* NIS+ has this map under a different name */
				nisrq->map.len =
				    sizeof("services.byservicename") - 1;
				memcpy(nisrq->map.string,
				    "services.byservicename", nisrq->map.len);
				nisrq->map.words = WORDS(nisrq->map.len);
				nisrq->nomap_proc = nis_match;
			}
		} else {
			nisrq->nomap_proc = nis_match;
		}
	}
	else if (strncasecmp(nisrq->map.string, "services.byservicename",
	    nisrq->map.len) == 0) {
		if ((nisrq->server->flags & NO_SERVICES2) || pseudo) {
			nisrq->proc = nis_loop;
		}
		nisrq->nomap_proc = nis_loop;
	}
	else if (!(strncasecmp(nisrq->map.string, "group.bymember",
			       nisrq->map.len))) {
		if ((nisrq->server->flags & NO_GRP_BYMBR) || pseudo) {
			nisrq->proc = nis_loop;
		}
		nisrq->nomap_proc = nis_loop;
	}
	else if (!(strncasecmp(nisrq->map.string, "netgroup.",
	    sizeof("netgroup.") - 1))) {
		if ((nisrq->server->flags & NO_NETGROUP) || pseudo) {
			nisrq->proc = nis_loop;
		}
		nisrq->nomap_proc = nis_loop;
	}

	/*
	** Make our request to the nis server.  This sets up a callback
	** and timeout handler so we can just return back to the main
	** loop.
	*/
	return (*nisrq->proc)(rq);
}
