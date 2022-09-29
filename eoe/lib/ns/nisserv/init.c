#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <alloca.h>
#include <sys/param.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/uio.h>
#include <mdbm.h>
#include <ns_api.h>
#include <ns_daemon.h>
#include <t6net.h>
#include "ns_nisserv.h"

static nisserv_t *_nsdata_list;
static uint32_t _nsdata_xid;
nisstring_t _nisserv_host;
uint32_t _nisserv_euid, _nisserv_egid;
char _nisserv_buf[8192];
nsd_file_t *__nsd_mounts;
int _nisserv_udp=-1, _nisserv_tcp=-1;

extern int _ltoa(long, char *);
extern int bindresvport(int, struct sockaddr_in *);
extern int sendfromto(int, void *, int, int, struct in_addr *, void *, int,
    mac_t);
extern int recvfromto(int, void *, int, int, struct sockaddr *, int,
    struct in_addr *, mac_t *);

static int dispatch_tcp(nsd_file_t **, int);

securenets_t *_nisserv_securenets = 0;

/*
** Looks up the NIS specific structure by transaction ID.
*/
nisserv_t *
nisserv_data_byxid(uint32_t xid)
{
	nisserv_t *nd;

	for (nd = _nsdata_list; nd && (nd->xid != xid); nd = nd->next);

	return nd;
}

/*
** Creates a data structure to hold NIS specific protocol information for
** a request and adds it to a list.
*/
nisserv_t *
nisserv_data_new(void)
{
	nisserv_t *nd;

	nd = (nisserv_t *)nsd_calloc(1, sizeof(nisserv_t));
	if (! nd) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "nisserv_data_new: malloc failed\n");
		return (nisserv_t *)0;
	}

	nd->xid = ++_nsdata_xid;
	nd->next = _nsdata_list;
	_nsdata_list = nd;

	return nd;
}

/*
** Looks up the NIS specific structure by file descriptor.
*/
nisserv_t *
nisserv_data_byfd(int fd)
{
	nisserv_t *nd;

	for (nd = _nsdata_list; nd && (nd->fd != fd); nd = nd->next);

	return nd;
}

/*
** Removes the NIS specific data structure by transaction ID.
*/
void
nisserv_data_clear(nisserv_t **data)
{
	nisserv_t **last, *nd;

	for (last = &_nsdata_list, nd = _nsdata_list; 
	     nd && (nd != *data);
	     last = &nd->next, nd = nd->next);
	if (nd) {
		*last = nd->next;
	}
	if (data && *data) {
		if ((*data)->data) {
			free((*data)->data);
		}
		if ((*data)->lbl) {
			mac_free((*data)->lbl);
		}
		if ((*data)->cred) {
			nsd_cred_clear(&(*data)->cred);
		}
		free(*data);
		*data = 0;
	}
}

/*
** This routine is called when we are waiting for more data, or the queue
** to drain in dispatch_tcp.
*/
static int
nisserv_timeout(nsd_file_t **rqp, nsd_times_t *to) {
	nisserv_t *nd;

	nsd_logprintf(NSD_LOG_LOW, "entering nisserv_timeout:\n");

	nd = (nisserv_t *)to->t_clientdata;
	nsd_timeout_remove(to->t_file);
	if (nd) {
		nsd_callback_remove(nd->fd);
		close(nd->fd);
		nisserv_data_clear(&nd);
	}
	*rqp = 0;
	return NSD_CONTINUE;
}

/*
** This is a commn routine to return the results to the client.  For UDP
** we use recvfromto to send the packet to the client from the same address
** it was originally sent to.  For TCP we prepend a length and use writev.
** This routine assumes that this is the last packet sent so it closes any
** remaining file descriptors and destorys the data.
*/
static void
reply(nisserv_t *nd, char *buf, int len)
{
	if (nd->proto == IPPROTO_UDP) {
		if (sendfromto(nd->fd, buf, len, 0, &nd->local, &nd->sin,
		    sizeof(nd->sin), nd->lbl) < 0) {
			nsd_logprintf(NSD_LOG_LOW,
			    "nisserv reply: sendfromto failed: %s\n",
			    strerror(errno));
		}
	} else {
		struct iovec iov[2];
		uint32_t p;

		p = htonl(0x80000000 + len);
		iov[0].iov_base = &p;
		iov[0].iov_len = sizeof(p);
		iov[1].iov_base = buf;
		iov[1].iov_len = len;
		if (writev(nd->fd, iov, 2) < 0) {
			nsd_logprintf(NSD_LOG_LOW,
			    "nisserv reply: writev failed: %s\n",
			    strerror(errno));
		}
		nsd_callback_remove(nd->fd);
		close(nd->fd);
	}

	nisserv_data_clear(&nd);
}

/*
** This is a handy routine to return an error to a requestor.
*/
static int
reject_err(nisserv_t *nd, uint32_t error)
{
	uint32_t *p;

	if (! nd) {
		return NSD_CONTINUE;
	}

	p = (uint32_t *)_nisserv_buf;

	*p++ = htonl(nd->xid);
	*p++ = htonl(REPLY);
	*p++ = htonl(MSG_DENIED);
	*p++ = htonl(error);

	reply(nd, _nisserv_buf, (char *)p - _nisserv_buf);

	return NSD_CONTINUE;
}

/*
** This is a handy routine to return an error to a requestor.
*/
static int
accept_err(nisserv_t *nd, uint32_t error)
{
	uint32_t *p;

	if (! nd) {
		return NSD_CONTINUE;
	}

	p = (uint32_t *)_nisserv_buf;

	*p++ = htonl(nd->xid);
	*p++ = htonl(REPLY);
	*p++ = htonl(MSG_ACCEPTED);

	*p++ = 0, *p++ = 0;

	*p++ = htonl(error);

	reply(nd, _nisserv_buf, (char *)p - _nisserv_buf);

	return NSD_CONTINUE;
}

/*
** This is a convenience routine to return an error from the NIS level.
*/
static int
nisserv_err(nisserv_t *nd, uint32_t error)
{
	uint32_t *p;

	if (! nd) {
		return NSD_CONTINUE;
	}

	p = (uint32_t *)_nisserv_buf;

	*p++ = htonl(nd->xid);
	*p++ = htonl(REPLY);
	*p++ = htonl(MSG_ACCEPTED);

	*p++ = 0, *p++ = 0;

	*p++ = htonl(SUCCESS);

	if (nd->cmd == YPPROC_ALL) {
		*p++ = 1;
	}

	*p++ = htonl(error);

	switch (nd->cmd) {
	case YPPROC_ALL:
		*p++=0;

		/* ypresp_key_val */
	case YPPROC_FIRST:
	case YPPROC_NEXT:
		/* ypresp_xfr  -- should never go here */
	case YPPROC_XFR:
		*p++=0;

		/* ypresp_val */
	case YPPROC_MATCH:		
		/* ypresp_master */
	case YPPROC_MASTER:
		/* ypresp_order */
	case YPPROC_ORDER:
		/* ypresp_ypmaplist */
	case YPPROC_MAPLIST:
		*p++=0;

		/* void */
	case YPPROC_NULL:
	case YPPROC_CLEAR:
		/* bool (done by the status)*/
	case YPPROC_DOMAIN:		
	case YPPROC_DOMAIN_NONACK:
		/* ypresp_all (FALSE)*/
	default:
		;
	}

	reply(nd, _nisserv_buf, (char *)p - _nisserv_buf);

	return NSD_CONTINUE;
}

/*
** This is a convenience routine to return an error from the NIS level.
*/
static int
nispush_err(nisserv_t *nd, uint32_t transid, uint32_t error)
{
	uint32_t *p;

	if (! nd) {
		return NSD_CONTINUE;
	}

	p = (uint32_t *)_nisserv_buf;

	*p++ = htonl(nd->xid);
	*p++ = htonl(REPLY);
	*p++ = htonl(MSG_ACCEPTED);

	*p++ = 0, *p++ = 0;

	*p++ = htonl(TRUE);
	*p++ = htonl(transid);
	*p++ = htonl(error);

	reply(nd, _nisserv_buf, (char *)p - _nisserv_buf);

	return NSD_CONTINUE;
}

static int
nis_check(nsd_cred_t *cp, nsd_file_t *fp)
{
	int i;

	nsd_logprintf(NSD_LOG_LOW, "entering nis_check:\n");
	nsd_logprintf(NSD_LOG_LOW, "\tcred uid: %d, gid: %d\n", cp->c_uid,
	    cp->c_gid[0]);
	nsd_logprintf(NSD_LOG_LOW, "\tfile uid: %d, gid: %d\n", fp->f_uid,
	    fp->f_gid);
	if (((cp->c_uid == 0) || (cp->c_uid == fp->f_uid)) &&
	    (fp->f_mode & S_IRUSR)) {
		nsd_logprintf(NSD_LOG_LOW, "\tsucceed case 1\n");
		return 1;
	} else if (fp->f_mode & S_IROTH) {
		nsd_logprintf(NSD_LOG_LOW, "\tsucceed case 2\n");
		return 1;
	} else {
		if (fp->f_mode & S_IRGRP) {
			for (i = 0; i < cp->c_gids; i++) {
				if (cp->c_gid[i] == fp->f_gid) {
					nsd_logprintf(NSD_LOG_LOW,
					    "\tsucceed case 3\n");
					return 1;
				}
			}
		}
	}

	nsd_logprintf(NSD_LOG_LOW, "\tfail: case 4\n");
	return 0;
}

/*
** This is a convenience routine used when building a response packet.
*/
static int
rpc_hdr(nisserv_t *nd, uint32_t *buf)
{
	uint32_t *p;

	if (! nd || ! buf) {
		return 0;
	}

	p = buf;

	*p++ = htonl(nd->xid);
	*p++ = htonl(REPLY);
	*p++ = htonl(MSG_ACCEPTED);

	*p++ = 0; *p++ = 0;
	*p++ = SUCCESS;

	return (p - buf);
}

/*
** This routine post-parses hosts entries.  NSD passes host data with multiple
** addresses, but pre-6.5 Irix and other operating systems cannot parse this
** so here we strip them off.
*/
static int
host_rewrite(char *src, int slen, char *dst, int dlen)
{
	int len;
	char *d;
	struct in_addr addr;

	nsd_logprintf(NSD_LOG_HIGH, "entering host_rewrite: %*.*s\n", slen,
	    slen, src);
	for (; *src && slen && isspace(*src); src++, slen--);

	/* copy first address */
	for (len = 0, d = dst; *src && slen && (len < dlen) && !isspace(*src);
	    src++, slen--, d++, len++) {
		*d = *src;
	}

	/* skip subsequent addresses */
	for (;;) {
		/* skip whitespace */
		for (; *src && slen && isspace(*src); src++, slen--);
		
		/* skip if address */
		if (! inet_aton(src, &addr) || ! (*src && slen)) {
			break;
		}
		for (; *src && slen && !isspace(*src); src++, slen--);
	}

	/* copy rest of line */
	if (len < dlen) {
		*d++ = '\t', len++;
		for (; *src && (*src != '\n') && slen && (len < dlen);
		    src++, slen--, d++, len++) {
			*d = *src;
		}
	}

	/* null extend to next word boundary */
	for (dlen = len; dlen % 4; d++, dlen++) {
		*d = (char)0;
	}

	nsd_logprintf(NSD_LOG_HIGH, "exiting host_rewrite: %*.*s\n", len,
	    len, dst);
	return len;
}

/*
** Sends results from a forwarded request to the requestor.  This is only
** called from YPPROC_MATCH and the portmap register.
*/
static int
nisserv_send(nsd_file_t *rq)
{
	nisserv_t *nd;
	uint32_t *p;
	int len;

	nsd_logprintf(NSD_LOG_MIN, "entering nisserv_send:\n");

	nd = rq->f_sender;
	if (! nd) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_CONTINUE\n");
		return NSD_CONTINUE;
	}
	rq->f_sender = 0;

	if (! nd->sin.sin_port) {
		nisserv_data_clear(&nd);
		nsd_file_clear(&rq);
		nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_CONTINUE\n");
		return NSD_CONTINUE;
	}

	/*
	** If we determine that this came from a "secure" map then we require
	** that the request be sent from port<1024.
	*/
	if (nsd_attr_fetch_bool(rq->f_attrs, "nis_secure", 0) &&
	    ntohs(nd->sin.sin_port) >= 1024) {
		nsd_file_clear(&rq);
		nsd_logprintf(NSD_LOG_LOW, "\tReturning YP_NOMAP\n");
		return nisserv_err(nd, YP_NOMAP);
	}
	if (! nis_check(nd->cred, rq)) {
		nsd_logprintf(NSD_LOG_LOW, "\tPermission denied\n");
		nsd_file_clear(&rq);
		return nisserv_err(nd, YP_NOMAP);
	}

	/*
	** Build up match reply.
	*/
	p = (uint32_t *)_nisserv_buf;

	p += rpc_hdr(nd, p);

	if (rq->f_status == NS_SUCCESS) {
		*p++ = htonl(YP_TRUE);
		if (rq->f_used > 0) {
			if (strncasecmp(nd->table.string, "hosts.", 6) == 0) {
				len = host_rewrite(rq->f_data, rq->f_used,
				    (char *)(p + 1), (sizeof(_nisserv_buf) -
				     ((char *)p - _nisserv_buf)) - sizeof(*p));
				*p++ = htonl(len);
				p += WORDS(len);
			} else {
				*p++ = htonl(rq->f_used);
				p[rq->f_used/sizeof(*p)] = 0;
				memcpy(p, rq->f_data, rq->f_used);
				p += WORDS(rq->f_used);
				nsd_logprintf(NSD_LOG_HIGH,
				    "\tlen = %d, result = %*s\n",
				    rq->f_used, rq->f_used, rq->f_data);
			}
		} else {
			*p++ = htonl(0);
		}
	} else if (rq->f_status == NS_NOTFOUND) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOKEY\n");
		*p++ = YP_NOKEY;
		*p++ = 0;
	} else {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOMAP [%d]\n",
		    rq->f_status);
		*p++ = YP_NOMAP;
		*p++ = 0;
	}

	reply(nd, _nisserv_buf, (char *)p - _nisserv_buf);
	nsd_file_clear(&rq);

	return NSD_CONTINUE;
}

/*
** Just sends back an empty data field.
*/
/*ARGSUSED*/
static int
nisserv_null(nsd_file_t **rqp, nisserv_t *nd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_MIN, "entering nisserv_null:\n");

	p = (uint32_t *)_nisserv_buf;
	p += rpc_hdr(nd, p);

	reply(nd, _nisserv_buf, (char *)p - _nisserv_buf);

	return NSD_CONTINUE;
}

/*
** This checks to see if we are handling a given domain and return true
** if we do, and false if we don't and the requestor wanted a nack.
*/
/* ARGSUSED */
static int
nisserv_domain(nsd_file_t **rqp, nisserv_t *nd, uint32_t *p, int nack)
{
	nsd_file_t *dp;
	int len;

	nsd_logprintf(NSD_LOG_MIN, "entering nisserv_domain:\n");

	dp = __nsd_mounts;

	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		dp = (nsd_file_t *)0;
	}
	nsd_logprintf(NSD_LOG_LOW, "\tlen = %d, domain = %s\n", len, p);

	/*
	** Search through top level directory for domain directory.
	*/
	if (dp &&
	    nsd_file_byname(__nsd_mounts->f_data, (char *)p, len)) {
		p = (uint32_t *)_nisserv_buf;
		p += rpc_hdr(nd, p);
		*p++ = htonl(TRUE);
		nsd_logprintf(NSD_LOG_LOW, "\treturning TRUE to %s.%d\n",
		    inet_ntoa(nd->sin.sin_addr), nd->sin.sin_port);
		reply(nd, _nisserv_buf, (char *)p - _nisserv_buf);
	} else if (nack) {
		p = (uint32_t *)_nisserv_buf;
		p += rpc_hdr(nd, p);
		*p++ = htonl(FALSE);
		nsd_logprintf(NSD_LOG_LOW, "\treturning FALSE\n");
		reply(nd, _nisserv_buf, (char *)p - _nisserv_buf);
	} else {
		/* While we have been told NOT to send a NACK, We know that
		** this routine is only ever intended to get called by 
		** portmap as a callit.  In that case we dont want it to
		** wait around for an answer that we will never give it,
		** so lets send it a GARBAGE_ARGS which will tell portmap
		** that we will not be sending a responce.   Portmap will 
		** not pass this on.
		*/
		p = (uint32_t *)_nisserv_buf;
		p += rpc_hdr(nd,p);
		*(p-1) = GARBAGE_ARGS;
		*p++ = htonl(FALSE);
		nsd_logprintf(NSD_LOG_MAX, "\treturning RPC GARBAGE_ARGS\n");
		reply(nd, _nisserv_buf, (char *)p - _nisserv_buf);
	}		

	return NSD_CONTINUE;
}

/*
** Looks up a specific key in a specific map.
*/
static int
nisserv_match(nsd_file_t **rqp, nisserv_t *nd, uint32_t *p)
{
	nsd_file_t *dp, *cp, *rq;
	int len;

	nsd_logprintf(NSD_LOG_MIN, "entering nisserv_match:\n");

	/*
	** Domain.
	*/
	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		nsd_logprintf(NSD_LOG_LOW, "\tReturning YP_BADARGS\n");
		return nisserv_err(nd, YP_BADARGS);
	}
	memcpy(nd->domain.string, p, len);
	nd->domain.string[len] = (char)0;
	nd->domain.len = len;
	nd->domain.words = WORDS(len);
	p += nd->domain.words;
	nsd_logprintf(NSD_LOG_LOW, "\tdomain = %s\n", nd->domain.string);

	/*
	** We do not serve data from the .local domain.
	*/
	if (strcasecmp(nd->domain.string, NS_DOMAIN_LOCAL) == 0) {
		nsd_logprintf(NSD_LOG_LOW, "\tReturning YP_NODOM\n");
		return nisserv_err(nd, YP_NODOM);
	}

	/*
	** Table.
	*/
	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		nsd_logprintf(NSD_LOG_LOW, "\tReturning YP_BADARGS 2\n");
		return nisserv_err(nd, YP_BADARGS);
	}

	memcpy(nd->table.string, p, len);
	nd->table.string[len] = (char)0;
	nd->table.len = len;
	nd->table.words = WORDS(nd->table.len);
	nsd_logprintf(NSD_LOG_LOW, "\ttable = %s\n", nd->table.string);
	p += nd->table.words;

	/*
	** Key.
	*/
	len = ntohl(*p++);
	if (len <= 0 || len > MAXNAMELEN ) {
		nsd_logprintf(NSD_LOG_LOW, "\tReturning YP_BADARGS 3\n");
		return nisserv_err(nd, YP_BADARGS);
	}
	memcpy(nd->key.string, p, len);
	nd->key.string[len] = (char)0;
	/* 
	** the mail.aliases map requires a null to be 
	** accounted for in len.   This is not in the protocol, 
	** but in the client programs.
	if (!strcmp(nd->table.string,"mail.aliases")) {
		len=strlen(nd->key.string);
	}
	*/
	nd->key.len = len;
	nd->key.words = WORDS(len);
	p += nd->key.words;
	nsd_logprintf(NSD_LOG_LOW, "\tkey = %s (%d)\n", nd->key.string, len);

	dp = __nsd_mounts;
	if (! dp || !(dp = nsd_file_byname(dp->f_data, nd->domain.string,
	    nd->domain.len))) {
		nsd_logprintf(NSD_LOG_LOW, "\tReturning YP_NODOM\n");
		return nisserv_err(nd, YP_NODOM);
	}

	if (! (dp = nsd_file_byname(dp->f_data, nd->table.string,
	    nd->table.len))) {
		nsd_logprintf(NSD_LOG_LOW, "\tReturning YP_NOMAP 2\n");
		return nisserv_err(nd, YP_NOMAP);
	}

	if ((rq = nsd_file_byname(dp->f_data, nd->key.string, nd->key.len)) &&
	    rq->f_status == NS_SUCCESS && rq->f_type == NFREG) {
		/*
		** In the daemon cache so we just return the data.
		** If the file is marked with the nis_secure attribute then
		** we require that the client request from port<1024.
		*/
		if (nsd_attr_fetch_bool(rq->f_attrs, "nis_secure", 0) &&
		    ntohs(nd->sin.sin_port) >= 1024) {
			nsd_logprintf(NSD_LOG_LOW, "\tReturning YP_NOMAP 3\n");
			return nisserv_err(nd, YP_NOMAP);
		}
		if (! nis_check(nd->cred, rq)) {
			nsd_logprintf(NSD_LOG_LOW, "\tPermission denied\n");
			return nisserv_err(nd, YP_NOMAP);
		}
		p = (uint32_t *)_nisserv_buf;
		p += rpc_hdr(nd, p);
		*p++ = htonl(YP_TRUE);
		if (rq->f_used > 0) {
			if (strncasecmp(nd->table.string, "hosts.", 6) == 0) {
				len = host_rewrite(rq->f_data, rq->f_used,
				    (char *)(p + 1),
				    (sizeof(_nisserv_buf) -
				     ((char *)p - _nisserv_buf)) - sizeof(*p));
				*p++ = htonl(len);
				p += WORDS(len);
			} else {
				*p++ = ntohl(rq->f_used);
				p[rq->f_used/sizeof(*p)] = 0;
				memcpy(p, rq->f_data, rq->f_used);
				p += WORDS(rq->f_used);
			}
		} else {
			*p++ = htonl(0);
		}

		reply(nd, _nisserv_buf, (char *)p - _nisserv_buf);

		return NSD_CONTINUE;
	}

	/*
	** If nis_secure attribute is set on table then we require the
	** request to be coming from port<1024.  If this isn't the case
	** then we do not bother to setup a request.
	*/
	if (nsd_attr_fetch_bool(dp->f_attrs, "nis_secure", 0) &&
	    ntohs(nd->sin.sin_port) >= 1024) {
		nsd_logprintf(NSD_LOG_LOW, "\tReturning YP_NOMAP 4\n");
		return nisserv_err(nd, YP_NOMAP);
	}

	/*
	** Setup a request.
	*/
	if (nsd_file_init(&rq, nd->key.string, nd->key.len, dp, NFREG,
	    nd->cred) != NSD_OK) {
		return nisserv_err(nd, YP_YPERR);
	}

	rq->f_index = LOOKUP;
	nsd_file_copycallouts(dp, rq, 0);
	nsd_attr_store(&rq->f_attrs, "key", nd->key.string);

	for (cp = nsd_file_getcallout(rq); cp; cp = nsd_file_nextcallout(rq)) {
		if (cp->f_lib && cp->f_lib->l_funcs[rq->f_index]) {
			nsd_attr_continue(&rq->f_attrs, cp->f_attrs);
			*rqp = rq;
			rq->f_send = nisserv_send;
			rq->f_sender = nd;
			return (*cp->f_lib->l_funcs[rq->f_index])(rq);
		}
	}

	/*
	** No callouts match.
	*/
	nsd_logprintf(NSD_LOG_LOW, "\tReturning YP_NOMAP\n");
	nisserv_data_clear((nisserv_t **)&rq->f_sender);
	nsd_file_clear(&rq);
	return nisserv_err(nd, YP_NOMAP);
}

/*
** This returns the first key in the data.  This only works out of
** the hash file.  We do not bother to implement this with all the
** other protocols as it would be very inefficient.
*/
/*ARGSUSED*/
static int
nisserv_first(nsd_file_t **rqp, nisserv_t *nd, uint32_t *p)
{
	int len,pagesize;
	hash_file_t *hf;
	kvpair kv;
	char *key, *val;
	nsd_file_t *dp,*dp2;
	struct stat sbuf;

	nsd_logprintf(NSD_LOG_MIN, "entering nisserv_first:\n");

	/*
	** Arguments are a domain and a map.  We walk from the top of
	** the tree looking for these.
	*/
	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning YP_BADARGS\n");
		return nisserv_err(nd, YP_BADARGS);
	}

	dp = __nsd_mounts;
	if (! dp || !(dp = nsd_file_byname(dp->f_data, (char *)p, len))) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning YP_NODOM\n");
		return nisserv_err(nd, YP_NODOM);
	}
	p += WORDS(len);

	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning YP_BADARGS 2\n");
		return nisserv_err(nd, YP_BADARGS);
	}

	if (! (dp = nsd_file_byname(dp->f_data, (char *)p, len))) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning YP_NOMAP\n");
		return nisserv_err(nd, YP_NOMAP);
	}

	/*
	** The library name is ".nisserv" or ".ypserv".  XXX - We should really
	** walk through all the callouts testing the library as the name
	** could change.
	*/
	if (! (dp2 = nsd_file_byname(dp->f_callouts, ".nisserv", 8))) {
		if (! (dp2 = nsd_file_byname(dp->f_callouts, ".ypserv", 7))) {
			nsd_logprintf(NSD_LOG_HIGH, "\treturning YP_NODOM 2\n");
			return nisserv_err(nd, YP_NODOM);
		}
	}
	dp = dp2;

	/*
	** Now look for the database file.
	*/
	hf = hash_get_file(dp);
	if (! hf) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning YP_NOMAP\n");
		return nisserv_err(nd, YP_NOMAP);
	}
	if (stat(hf->file, &sbuf)) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning YP_NOMAP 2\n");
		return nisserv_err(nd, YP_NOMAP);
	}
	if ((! hf->map) || (hf->version < sbuf.st_mtime)
	    || _Mdbm_invalid(hf->map)) {
		if (! hash_reopen(hf)) {
			nsd_logprintf(NSD_LOG_HIGH, "\treturning YP_BADDB 3\n");
			return nisserv_err(nd, YP_BADDB);
		}
		hf->version = sbuf.st_mtime;
	}

	pagesize = MDBM_PAGE_SIZE(hf->map);
	key = alloca(pagesize);
	val = alloca(pagesize);
	
	for (kv.key.dptr = key , kv.key.dsize = pagesize,
		     kv.val.dptr = val , kv.val.dsize = pagesize, 
		kv = mdbm_first(hf->map,kv); 
	     kv.key.dptr; 
	     kv.key.dptr = key , kv.key.dsize = pagesize,
		     kv.val.dptr = val , kv.val.dsize = pagesize, 
		kv = mdbm_next(hf->map,kv)) {
		/* Skip private records. */
		if ((kv.key.dsize < 3) ||
		    (strncmp(kv.key.dptr, "YP_", 3) != 0)) {
			break;
		}
	}

	if (! kv.key.dptr) {
		nsd_logprintf(NSD_LOG_HIGH, "\treturning YP_NOMORE\n");
		return nisserv_err(nd, YP_NOMORE);
	}

	/*
	** Build the response.
	*/
	p = (uint32_t *)_nisserv_buf;
	p += rpc_hdr(nd, p);
	*p++ = htonl(TRUE);

	*p++ = htonl(kv.val.dsize);
	memcpy(p, kv.val.dptr, kv.val.dsize);
	p += WORDS(kv.val.dsize);

	*p++ = htonl(kv.key.dsize);
	memcpy(p, kv.key.dptr, kv.key.dsize);
	p += WORDS(kv.key.dsize);

	reply(nd, _nisserv_buf, (char *)p - _nisserv_buf);

	return NSD_CONTINUE;
}

/*
** This looks up a key in the hash file, then returns the next one.
** This is only implemented for the hash files.
*/
/*ARGSUSED*/
static int
nisserv_next(nsd_file_t **rqp, nisserv_t *nd, uint32_t *p)
{
	int len;
	hash_file_t *hf;
	kvpair kv;
	int pagesize;
	char *val, *key;
	nsd_file_t *dp,*dp2;
	struct stat sbuf;

	nsd_logprintf(NSD_LOG_MIN, "entering nisserv_next:\n");

	/*
	** Arguments are a domain and a map.  We walk from the top of
	** the tree looking for these.
	*/
	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_BADARGS\n");
		return nisserv_err(nd, YP_BADARGS);
	}
	nsd_logprintf(NSD_LOG_HIGH, "\tdomain: %*.*s\n", len, len, p);

	dp = __nsd_mounts;
	if (! dp || !(dp = nsd_file_byname(dp->f_data, (char *)p, len))) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NODOM\n");
		return nisserv_err(nd, YP_NODOM);
	}
	p += WORDS(len);

	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_BADARGS 2\n");
		return nisserv_err(nd, YP_BADARGS);
	}
	nsd_logprintf(NSD_LOG_HIGH, "\ttable: %*.*s\n", len, len, p);

	if (! (dp = nsd_file_byname(dp->f_data, (char *)p, len))) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOMAP\n");
		return nisserv_err(nd, YP_NOMAP);
	}
	p += WORDS(len);

	/*
	** The library name is ".nisserv" or ".ypserv".  XXX - We should really
	** walk through all the callouts testing the library as the name
	** could change.
	*/
	if (! (dp2 = nsd_file_byname(dp->f_callouts, ".nisserv", 8))) {
		if(! (dp2 = nsd_file_byname(dp->f_callouts, ".ypserv", 7))) {
			nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NODOM\n");
			return nisserv_err(nd, YP_NODOM);
		}
	}
	dp = dp2;

	/*
	** Now look for the database file.
	*/
	hf = hash_get_file(dp);
	if (! hf) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOMAP\n");
		return nisserv_err(nd, YP_NOMAP);
	}
	if (stat(hf->file, &sbuf)) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOMAP\n");
		return nisserv_err(nd, YP_NOMAP);
	}
	if ((! hf->map) || (hf->version < sbuf.st_mtime) 
	    || _Mdbm_invalid(hf->map)) {
		if (! hash_reopen(hf)) {
			nsd_logprintf(NSD_LOG_LOW, "\treturning YP_BADDB\n");
			return nisserv_err(nd, YP_BADDB);
		}
		hf->version = sbuf.st_mtime;
	}

	/*
	** Look for the key.
	*/
	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_BADARGS 3\n");
		return nisserv_err(nd, YP_BADARGS);
	}
	nsd_logprintf(NSD_LOG_HIGH, "\tkey: %*.*s\n", len, len, p);

	pagesize=MDBM_PAGE_SIZE(hf->map);
	key = alloca(pagesize);
	val = alloca(pagesize);

	kv.key.dptr = (char *)p;
	kv.key.dsize = len;
	kv.val.dptr = key;
	kv.val.dsize = pagesize;

	kv.val = mdbm_fetch(hf->map, kv);
	if (! kv.val.dptr) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOMORE\n");
		return nisserv_err(nd, YP_NOMORE);
	}

	for (kv.key.dptr = key , kv.key.dsize = pagesize,
		     kv.val.dptr = val , kv.val.dsize = pagesize, 
		     kv = mdbm_next(hf->map,kv); 
	     kv.key.dptr;
	     kv.key.dptr = key , kv.key.dsize = pagesize,
		     kv.val.dptr = val , kv.val.dsize = pagesize, 
		     kv = mdbm_next(hf->map,kv)) {
		/* Skip private records. */
		if ((kv.key.dsize < 3) ||
		    (strncmp(kv.key.dptr, "YP_", 3) != 0)) {
			break;
		}
	}

	if (! kv.key.dptr) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOMORE\n");
		return nisserv_err(nd, YP_NOMORE);
	}

	/*
	** Build the response.
	*/
	p = (uint32_t *)_nisserv_buf;
	p += rpc_hdr(nd, p);
	*p++ = htonl(TRUE);

	*p++ = htonl(kv.val.dsize);
	memcpy(p, kv.val.dptr, kv.val.dsize);
	p += WORDS(kv.val.dsize);

	*p++ = htonl(kv.key.dsize);
	memcpy(p, kv.key.dptr, kv.key.dsize);
	p += WORDS(kv.key.dsize);

	nsd_logprintf(NSD_LOG_HIGH, "\tkey: %*.*s, val: %*.*s\n", kv.key.dsize,
	    kv.key.dsize, kv.key.dptr, kv.val.dsize, kv.val.dsize, kv.val.dptr);

	reply(nd, _nisserv_buf, (char *)p - _nisserv_buf);

	return NSD_CONTINUE;
}

/*
** This exec's the ypxfr function.  Ypxfr will copy over a new hash
** file from the master server then ping us to reopen the hash files.
*/
/*ARGSUSED*/
static int
nisserv_xfr(nsd_file_t **rqp, nisserv_t *nd, uint32_t *p)
{
	pid_t pid;
	uint32_t transid, proto, port;
	char tidstr[16], protostr[16], portstr[16], *ipaddr;
	int len, i;

	nsd_logprintf(NSD_LOG_MIN, "entering nisserv_xfr:\n");

	/*
	** Ypmap_parms.  domain, map, ordernum, owner (owner saved in nd->key).
	*/
	len = ntohl(*p++);
	nsd_logprintf(NSD_LOG_LOW, "\tlen: %d, domain: %s\n", len, p);
	if (len > MAXNAMELEN) {
		return nispush_err(nd, 0, YPPUSH_BADARGS);
	}
	memcpy(nd->domain.string, p, len);
	nd->domain.string[len] = (char)0;
	nd->domain.len = len;
	nd->domain.words = WORDS(len);
	p += nd->domain.words;

	len = ntohl(*p++);
	nsd_logprintf(NSD_LOG_LOW, "\tlen: %d, table: %s\n", len, p);
	if (len > MAXNAMELEN) {
		return nispush_err(nd, 0, YPPUSH_BADARGS);
	}

	memcpy(nd->table.string, p, len);
	nd->table.string[len] = (char)0;
	nd->table.len = len;
	nd->table.words = WORDS(nd->table.len);
	p += nd->table.words;

	/* we ignore ordernum */
	p++;

	len = ntohl(*p++);
	nsd_logprintf(NSD_LOG_LOW, "\tlen: %d, key: %s\n", len, p);
	if (len > MAXNAMELEN) {
		return nispush_err(nd, 0, YPPUSH_BADARGS);
	}
	memcpy(nd->key.string, p, len);
	nd->key.string[len] = (char)0;
	nd->key.len = len;
	nd->key.words = WORDS(len);
	p += nd->key.words;

	transid = ntohl(*p++);
	_ltoa((long)transid, tidstr);

	proto = ntohl(*p++);
	_ltoa((long)proto, protostr);

	port = ntohl(*p++);
	_ltoa((long)port, portstr);

	ipaddr = inet_ntoa(nd->sin.sin_addr);

	pid = fork();
	switch (pid) {
	    case -1:
		nsd_logprintf(NSD_LOG_RESOURCE, "nisserv_xfr: failed fork\n");
		return nispush_err(nd, transid, YPPUSH_XFRERR);
	    case 0:
		/*
		** Child.
		*/
		for (i = 3; i < getdtablehi(); i++) {
			close(i);
		}
		if (execl("/usr/sbin/ypxfr", "ypxfr", "-d", nd->domain.string,
		    "-C", tidstr, protostr, ipaddr, portstr, nd->table.string,
		    (char *)0) < 0) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "nisserv_xfr: failed exec\n");
		}
		nispush_err(nd, transid, YPPUSH_XFRERR);
		exit(1);
	}

	/*
	** Parent.
	*/
	return nisserv_null(rqp, nd, p);
}

/*
** This exec's the ypxfr function.  Ypxfr will copy over a new hash
** file from the master server then ping us to reopen the hash files.
*/
/*ARGSUSED*/
static int
nisserv_newxfr(nsd_file_t **rqp, nisserv_t *nd, uint32_t *p)
{
	pid_t pid;
	uint32_t transid, proto;
	char tidstr[16], protostr[16], name[64];
	int len, i;

	nsd_logprintf(NSD_LOG_MIN, "entering nisserv_xfr:\n");

	/*
	** Ypmap_parms.  domain, map, ordernum, owner (owner saved in nd->key).
	*/
	len = ntohl(*p++);
	nsd_logprintf(NSD_LOG_LOW, "\tlen: %d, domain: %s\n", len, p);
	if (len > MAXNAMELEN) {
		return nispush_err(nd, 0, YPPUSH_BADARGS);
	}
	memcpy(nd->domain.string, p, len);
	nd->domain.string[len] = (char)0;
	nd->domain.len = len;
	nd->domain.words = WORDS(len);
	p += nd->domain.words;

	len = ntohl(*p++);
	nsd_logprintf(NSD_LOG_LOW, "\tlen: %d, table: %s\n", len, p);
	if (len > MAXNAMELEN) {
		return nispush_err(nd, 0, YPPUSH_BADARGS);
	}

	memcpy(nd->table.string, p, len);
	nd->table.string[len] = (char)0;
	nd->table.len = len;
	nd->table.words = WORDS(nd->table.len);
	p += nd->table.words;

	/* we ignore ordernum */
	p++;

	len = ntohl(*p++);
	nsd_logprintf(NSD_LOG_LOW, "\tlen: %d, key: %s\n", len, p);
	if (len > MAXNAMELEN) {
		return nispush_err(nd, 0, YPPUSH_BADARGS);
	}
	memcpy(nd->key.string, p, len);
	nd->key.string[len] = (char)0;
	nd->key.len = len;
	nd->key.words = WORDS(len);
	p += nd->key.words;

	transid = ntohl(*p++);
	_ltoa((long)transid, tidstr);

	proto = ntohl(*p++);
	_ltoa((long)proto, protostr);

	len = ntohl(*p++);
	nsd_logprintf(NSD_LOG_LOW, "\tlen:%d, name: %s\n", len, p);
	if (len >= sizeof(name)) {
		return nispush_err(nd, 0, YPPUSH_BADARGS);
	}
	memcpy(name, p, len);
	name[len] = 0;

	pid = fork();
	switch (pid) {
	    case -1:
		nsd_logprintf(NSD_LOG_RESOURCE, "nisserv_xfr: failed fork\n");
		return nispush_err(nd, transid, YPPUSH_XFRERR);
	    case 0:
		/*
		** Child.
		*/
		for (i = 3; i < getdtablehi(); i++) {
			close(i);
		}
		if (execl("/usr/sbin/ypxfr", "ypxfr", "-d", nd->domain.string,
		    "-C", tidstr, protostr, name, nd->table.string,
		    (char *)0) < 0) {
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "nisserv_xfr: failed exec\n");
		}
		nispush_err(nd, transid, YPPUSH_XFRERR);
		exit(1);
	}

	/*
	** Parent.
	*/
	return nisserv_null(rqp, nd, p);
}

/*
** This is just a notice that we should reopen all the hash files to
** keep from getting bad data.  It would be nice if the protocol told
** us which specific file, but . . .
*/
static int
nisserv_clear(nsd_file_t **rqp, nisserv_t *nd, uint32_t *p)
{
	nsd_logprintf(NSD_LOG_MIN, "entering nisserv_clear:\n");

	hash_clear();

	return nisserv_null(rqp, nd, p);
}

/*
** This routine returns the master server for the requested table and
** domain.  This is stored as a key in the hash file.  This only makes
** sense for the hash files so that is what we return.
*/
/*ARGSUSED*/
static int
nisserv_master(nsd_file_t **rqp, nisserv_t *nd, uint32_t *p)
{
	int len;
	hash_file_t *hf;
	kvpair kv;
	nsd_file_t *dp, *dp2;
	struct stat sbuf;

	nsd_logprintf(NSD_LOG_MIN, "entering nisserv_master:\n");

	/*
	** Arguments are a domain and a map.  We walk from the top of
	** the tree looking for these.
	*/
	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_BADARGS\n");
		return nisserv_err(nd, YP_BADARGS);
	}
	nsd_logprintf(NSD_LOG_LOW, "\tlen = %d, domain = %s\n", len, p);

	dp = __nsd_mounts;
	if (! dp || !(dp = nsd_file_byname(dp->f_data, (char *)p, len))) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NODOM\n");
		return nisserv_err(nd, YP_NODOM);
	}
	p += WORDS(len);

	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_BADARGS\n");
		return nisserv_err(nd, YP_BADARGS);
	}
	nsd_logprintf(NSD_LOG_LOW, "\tlen = %d, map = %s\n", len, p);

	if (! (dp = nsd_file_byname(dp->f_data, (char *)p, len))) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOMAP\n");
		return nisserv_err(nd, YP_NOMAP);
	}

	/*
	** The library name is ".nisserv" or ".ypserv".  XXX - We should really
	** walk through all the callouts testing the library as the name
	** could change.
	*/
	if (! (dp2 = nsd_file_byname(dp->f_callouts, ".nisserv", 8))) {
		if (! (dp2 = nsd_file_byname(dp->f_callouts, ".ypserv", 7))) {
			nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NODOM\n");
			return nisserv_err(nd, YP_NODOM);
		}
	}
	dp = dp2;

	/*
	** Now look for the database file.
	*/
	hf = hash_get_file(dp);
	if (! hf) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOMAP\n");
		return nisserv_err(nd, YP_NOMAP);
	}
	if (stat(hf->file, &sbuf)) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOMAP\n");
		return nisserv_err(nd, YP_NOMAP);
	}
	if ((! hf->map) || (hf->version < sbuf.st_mtime)
	    || _Mdbm_invalid(hf->map)) {
		if (! hash_reopen(hf)) {
			nsd_logprintf(NSD_LOG_LOW, "\treturning YP_BADDB\n");
			return nisserv_err(nd, YP_BADDB);
		}
		hf->version = sbuf.st_mtime;
	}

	/*
	** Pull out the magic key.
	*/
	kv.key.dptr = "YP_MASTER_NAME";
	kv.key.dsize = sizeof("YP_MASTER_NAME") - 1;

	kv.val.dsize = MDBM_PAGE_SIZE(hf->map);
	kv.val.dptr = alloca(kv.val.dsize);

	kv.val = mdbm_fetch(hf->map, kv);

	if (! kv.val.dptr) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOKEY\n");
		return nisserv_err(nd, YP_NOKEY);
	}

	/*
	** Build the response.
	*/
	p = (uint32_t *)_nisserv_buf;
	p += rpc_hdr(nd, p);
	*p++ = htonl(TRUE);
	*p++ = htonl(kv.val.dsize);
	memcpy(p, kv.val.dptr, kv.val.dsize);
	p += WORDS(kv.val.dsize);

	reply(nd, _nisserv_buf, (char *)p - _nisserv_buf);

	return NSD_CONTINUE;
}

/*ARGSUSED*/
static int
nisserv_order(nsd_file_t **rqp, nisserv_t *nd, uint32_t *p)
{
	int len;
	hash_file_t *hf;
	kvpair kv;
	nsd_file_t *dp, *dp2;
	char buf[MAXNAMELEN];
	struct stat sbuf;

	nsd_logprintf(NSD_LOG_MIN, "entering nisserv_order:\n");

	/*
	** Arguments are a domain and a map.  We walk from the top of
	** the tree looking for these.
	*/
	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		return nisserv_err(nd, YP_BADARGS);
	}
	nsd_logprintf(NSD_LOG_LOW, "\tlen: %d, domain: %s\n", len, p);

	dp = __nsd_mounts;
	if (! dp || !(dp = nsd_file_byname(dp->f_data, (char *)p, len))) {
		return nisserv_err(nd, YP_NODOM);
	}
	p += WORDS(len);

	len = ntohl(*p++);
	if (len > MAXNAMELEN) {
		return nisserv_err(nd, YP_BADARGS);
	}

	nsd_logprintf(NSD_LOG_LOW, "\tlen: %d, table: %s\n", len, p);

	if (! (dp = nsd_file_byname(dp->f_data, (char *)p, len))) {
		return nisserv_err(nd, YP_NOMAP);
	}

	/*
	** The library name is ".nisserv" or ".ypserv".  XXX - We should really
	** walk through all the callouts testing the library as the name
	** could change.
	*/
	if (! (dp2 = nsd_file_byname(dp->f_callouts, ".nisserv", 8))) {
		if (! (dp2 = nsd_file_byname(dp->f_callouts, ".ypserv", 7))) {
			return nisserv_err(nd, YP_NODOM);
		}
	}
	dp = dp2;

	/*
	** Now look for the database file.
	*/
	hf = hash_get_file(dp);
	if (! hf) {
		return nisserv_err(nd, YP_NOMAP);
	}
	if (stat(hf->file, &sbuf)) {
		return nisserv_err(nd, YP_NOMAP);
	}
	if ((! hf->map) || (hf->version < sbuf.st_mtime)
	    || _Mdbm_invalid(hf->map)) {
		if (! hash_reopen(hf)) {
			return nisserv_err(nd, YP_BADDB);
		}
		hf->version = sbuf.st_mtime;
	}

	/*
	** Pull out the magic key.
	*/
	kv.key.dptr = "YP_LAST_MODIFIED";
	kv.key.dsize = sizeof("YP_LAST_MODIFIED") - 1;

	kv.val.dsize =  MDBM_PAGE_SIZE(hf->map);
	kv.val.dptr = alloca(kv.val.dsize);

	kv.val = mdbm_fetch(hf->map, kv);

	if (! kv.val.dptr) {
		return nisserv_err(nd, YP_NOKEY);
	}
	memcpy(buf, kv.val.dptr, kv.val.dsize);
	buf[kv.val.dsize] = (char)0;

	/*
	** Build the response.
	*/
	p = (uint32_t *)_nisserv_buf;
	p += rpc_hdr(nd, p);
	*p++ = htonl(TRUE);
	*p++ = htonl(atol(buf));

	reply(nd, _nisserv_buf, (char *)p - _nisserv_buf);

	return NSD_CONTINUE;
}

/*ARGSUSED*/
static int
nisserv_maplist(nsd_file_t **rqp, nisserv_t *nd, uint32_t *p)
{
	int len;
	nsd_file_t *dp, *fp;
	u_char *c;
	uint32_t *u, last;
	hash_file_t *hf;
	char *table;

	nsd_logprintf(NSD_LOG_MIN, "entering nisserv_maplist:\n");

	/*
	** The only argument is a domain.  We walk the directory tree looking
	** for the domain directory.
	*/
	len = (int)ntohl(*p++);
	if (len > MAXNAMELEN) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_BADARGS\n");
		return nisserv_err(nd, YP_BADARGS);
	}
	nsd_logprintf(NSD_LOG_LOW, "len = %d, domain = %s\n", len, p);

	dp = __nsd_mounts;
	if (! dp || !(dp = nsd_file_byname(dp->f_data, (char *)p, len))) {
		nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NODOM\n");
		return nisserv_err(nd, YP_NODOM);
	}

	/*
	** Walk the directory appending each map name to the end of the
	** response. XXX - We should not encode information about the
	** directory format here.
	*/
	p = (uint32_t *)_nisserv_buf;
	p += rpc_hdr(nd, p);
	*p++ = htonl(TRUE);
	for (c = (u_char *)dp->f_data; c && *c; c = (u_char *)(u + 1)) {
		u = (uint32_t *)c;
		u += WORDS(*c + 1);
		if (*(c+1) == '.') {
			continue;
		}
		if (*u == last) {
			continue;
		}
		last = *u;
		fp = nsd_file_byid(last);
		if (! fp) {
			continue;
		}
		hf = hash_get_file(fp);
		if (! hf) {
			continue;
		}
		if (access(hf->file, R_OK) < 0) {
			continue;
		}
		table = nsd_attr_fetch_string(fp->f_attrs, "table", fp->f_name);

		len = strlen(table);
		if ((char *)&p[len + 3] >=
		    &_nisserv_buf[sizeof(_nisserv_buf)]) {
			break;
		}
		*p++ = htonl(TRUE);
		*p++ = htonl(len);
		p[len/sizeof(*p)] = 0;
		memcpy(p, table, len);
		p += WORDS(len);
	}
	*p++ = FALSE;

	reply(nd, _nisserv_buf, (char *)p - _nisserv_buf);

	return NSD_CONTINUE;
}

/*
** This function simply clears the list of securenets.
*/
static void
securenets_clear(void)
{
	securenets_t *sp;

	for (sp = _nisserv_securenets; sp; sp = _nisserv_securenets) {
		_nisserv_securenets = _nisserv_securenets->next;
		free(sp);
	}
}

/*
** This routine walks the list of securenets checking that this address
** is on one of them.
*/
int
nisserv_insecurenets(struct in_addr *addr)
{
	securenets_t *sp;

	if (! _nisserv_securenets || (addr->s_addr == INADDR_LOOPBACK)) {
		return 1;
	}
	if (! addr) {
		return 0;
	}

	for (sp = _nisserv_securenets; sp; sp = sp->next) {
		if ((addr->s_addr & sp->netmask.s_addr) ==
		    (sp->addr.s_addr & sp->netmask.s_addr)) {
			return 1;
		}
	}

	return 0;
}

/*
** This routine just parses the securenets file and adds entries to
** the list.
*/
static int
securenets_parse(void)
{
	FILE *fp;
	securenets_t *sp;
	char *p, *q;;

	securenets_clear();

	fp = fopen("/etc/securenets", "r");
	if (! fp) {
		nsd_logprintf(NSD_LOG_MAX,
		    "securenets_parse: failed to open securenets file\n");
		return 0;
	}
	while (fgets(_nisserv_buf, sizeof(_nisserv_buf), fp) != NULL) {
		for (p = _nisserv_buf; *p && isspace(*p); p++);
		for (q = p; *q && !isspace(*q); q++);
		if (! *q) {
			nsd_logprintf(NSD_LOG_MAX,
			    "securenets_parse: no first space\n");
			continue;
		}
		*q++ = 0;

		sp = (securenets_t *)nsd_malloc(sizeof(*sp));
		if (! sp) {
			nsd_logprintf(NSD_LOG_RESOURCE,
    "securenets_parse: failed malloc, securenets list will be incomplete\n");
			fclose(fp);
			return 0;
		}

		if (! inet_aton(p, &sp->netmask)) {
			nsd_logprintf(NSD_LOG_MAX,
			    "securenets_parse: failed inet_aton for netmask\n");
			free(sp);
			continue;
		}

		for (p = q; *p && isspace(*p); p++);
		for (q = p; *q && !isspace(*q); q++);
		if (*q) {
			*q = 0;
		}

		if (! inet_aton(p, &sp->addr)) {
			nsd_logprintf(NSD_LOG_MAX,
			    "securenets_parse: failed inet_aton for address\n");
			free(sp);
			continue;
		}

		sp->next = _nisserv_securenets;
		_nisserv_securenets = sp;
	}

	fclose(fp);

	return 1;
}

/*
** This routine is called when we receive a packet of the udp port.
*/
static int
dispatch_udp(nsd_file_t **rqp, int fd)
{
	uint32_t *p, xid, cmd;
	nisserv_t *nd;
	int len, nlen, i;
	struct sockaddr_in sin;
	struct in_addr dst;
	mac_t lbl;

	nsd_logprintf(NSD_LOG_MIN, "entering dispatch_udp:\n");
	*rqp = 0;

	/*
	** Read in the packet.
	*/
	len = sizeof(sin);
	memset(_nisserv_buf, 0, sizeof(_nisserv_buf));
	nlen = recvfromto(fd, _nisserv_buf, sizeof(_nisserv_buf), 0,
	    (struct sockaddr *)&sin, len, &dst, &lbl);
	if (nlen < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "dispatch_udp: failed recvfrom: %s\n", strerror(errno));
		return NSD_CONTINUE;
	}

	/*
	** If this is not coming from one of our supported networks then we
	** ignore the request.
	*/
	if (! nisserv_insecurenets(&sin.sin_addr)) {
		nsd_logprintf(NSD_LOG_MIN,
		    "dispatch_udp: request from insecure network %s\n",
		    inet_ntoa(sin.sin_addr));
		return NSD_CONTINUE;
	}

	p = (uint32_t *)_nisserv_buf;
	xid = ntohl(p[0]);
	if (ntohl(p[1]) != CALL) {
		mac_free(lbl);
		return NSD_CONTINUE;
	}

	nd = nisserv_data_new();
	if (! nd) {
		mac_free(lbl);
		return NSD_CONTINUE;
	}
	nd->proto = IPPROTO_UDP;
	nd->xid = xid;
	nd->fd = fd;
	nd->lbl = lbl;
	nd->sin = sin;
	nd->local = dst;

	if (ntohl(p[2]) != RPC_MSG_VERSION) {
		return reject_err(nd, RPC_MISMATCH);
	}
	if ((ntohl(p[3]) != YPPROG) || (ntohl(p[4]) != YPVERS)) {
		return accept_err(nd, PROG_MISMATCH);
	}
	cmd = ntohl(p[5]);
	nd->cmd = cmd;

	/*
	** Remember credentials.
	*/
	switch (ntohl(p[6])) {
	    case AUTH_NONE:
		nd->cred = nsd_cred_new(0, 1, 0);
		if (! nd->cred) {
			return accept_err(nd, SYSTEM_ERR);
		}
		p += WORDS(ntohl(p[7]));
		p += 8;
		break;
	    case AUTH_UNIX:
		p += WORDS(ntohl(p[9]));
		p += 10;
		nd->cred = nsd_cred_new((uid_t)ntohl(p[0]), 0);
		if (! nd->cred) {
			return accept_err(nd, SYSTEM_ERR);
		}
		nd->cred->c_gid[0] = (gid_t)ntohl(p[1]);
		nd->cred->c_gids = ntohl(p[2]);
		for (i = 0; i < nd->cred->c_gids; i++) {
			nd->cred->c_gid[i+1] = (gid_t)ntohl(p[i+3]);
		}
		p += (i + 3);
		break;
	    case AUTH_DES:
		nsd_logprintf(NSD_LOG_OPER, 
		    "dispatch_udp: received AUTH_DES authorization\n");
		return reject_err(nd, AUTH_FAILED);
	    default:
		nsd_logprintf(NSD_LOG_OPER, 
		    "dispatch_udp: received unknown authentication type 0x%x\n",
		    ntohl(p[6]));
		return reject_err(nd, AUTH_FAILED);
	}

	/* skip verf */
	p += WORDS(ntohl(p[1]));
	p += 2;
	switch (cmd) {
	    case YPPROC_NULL:
		return nisserv_null(rqp, nd, p);
	    case YPPROC_DOMAIN:
		return nisserv_domain(rqp, nd, p, 1);
	    case YPPROC_DOMAIN_NONACK:
		return nisserv_domain(rqp, nd, p, 0);
	    case YPPROC_MATCH:
		return nisserv_match(rqp, nd, p);
	    case YPPROC_FIRST:
		return nisserv_first(rqp, nd, p);
	    case YPPROC_NEXT:
		return nisserv_next(rqp, nd, p);
	    case YPPROC_XFR:
		return nisserv_xfr(rqp, nd, p);
	    case YPPROC_CLEAR:
		return nisserv_clear(rqp, nd, p);
	    case YPPROC_ALL:
		/* We do not support yp_all over udp. */
		return nisserv_err(nd, YP_BADOP);
	    case YPPROC_MASTER:
		return nisserv_master(rqp, nd, p);
	    case YPPROC_ORDER:
		return nisserv_order(rqp, nd, p);
	    case YPPROC_MAPLIST:
		return nisserv_maplist(rqp, nd, p);
	    case YPPROC_NEWXFR:
		return nisserv_newxfr(rqp, nd, p);
	}

	return accept_err(nd, PROC_UNAVAIL);
}

/*
** This routine is called when we receive a request packet on the tcp port.
** This routine if horribly complex because it is reintrant.  If we do
** not have enough data to complete the next part of the process then we
** save state and fallback to the select loop until it arrives.  When we
** get back here we start back where we left off.
*/
/* ARGSUSED */
static int
nisserv_all(nsd_file_t **rqp, nisserv_t *nd, uint32_t *p)
{
	int len, qlen;
	nsd_file_t *dp, *dp2;
	uint32_t *end = (uint32_t *)(_nisserv_buf + 1600);
	hash_file_t *hf;
	struct stat sbuf;
	kvpair kv;
	int pagesize=0;
	char *key, *val, *q;

	switch(nd->status) {
	    case 0:
	    case 1:
	    case 2:
		/* Huh? */
	    case 3:
		len = ntohl(*p++);
		if (len > MAXNAMELEN) {
			nsd_logprintf(NSD_LOG_LOW,
			    "nisserv_all: domain too long\n");
			return nisserv_err(nd, YP_BADARGS);
		}
		memcpy(nd->domain.string, p, len);
		nd->domain.string[len] = 0;
		nd->domain.len = len;
		nd->domain.words = WORDS(len);
		nsd_logprintf(NSD_LOG_LOW, "\tdomain = %s\n",
		    nd->domain.string);
		
		/*
		** We do not serve from the .local domain.
		*/
		if (strcasecmp(nd->domain.string, NS_DOMAIN_LOCAL) == 0) {
			nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NODOM\n");
			return nisserv_err(nd, YP_NODOM);
		}

		p += nd->domain.words;

		len = ntohl(*p++);
		if (len > MAXNAMELEN) {
			nsd_logprintf(NSD_LOG_LOW,
			    "\treturning YP_BADARGS 2\n");
			return nisserv_err(nd, YP_BADARGS);
		}

		memcpy(nd->table.string, p, len);
		nd->table.string[len] = 0;
		nd->table.len = len;
		nd->table.words = WORDS(len);
		nsd_logprintf(NSD_LOG_LOW, "\ttable = %s\n", nd->table.string);

		p += nd->table.words;

		/*
		** Done parsing request.
		*/
		free(nd->data);
		nd->data = 0;
		nd->size = nd->len = 0;

		/*
		** Walk the tree looking for the callout directory.
		*/
		if (! __nsd_mounts ||
		    ! (dp = nsd_file_byname(__nsd_mounts->f_data,
		    nd->domain.string, nd->domain.len))) {
			nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NODOM\n");
			return nisserv_err(nd, YP_NODOM);
		}

		dp = nsd_file_byname(dp->f_data, nd->table.string,
		    nd->table.len);
		if (! dp) {
			nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOMAP\n");
			return nisserv_err(nd, YP_NOMAP);
		}

		dp2 = nsd_file_byname(dp->f_callouts, ".nisserv", 8);
		if (! dp2) {
			dp2 = nsd_file_byname(dp->f_callouts, ".ypserv", 7);
			if (! dp2) {
				nsd_logprintf(NSD_LOG_LOW,
				    "\treturning YP_NODOM\n");
				return nisserv_err(nd, YP_NODOM);
			}
		}
		dp = dp2;

		if (! nis_check(nd->cred, dp)) {
			nsd_logprintf(NSD_LOG_LOW, "\tpermission denied\n");
			return nisserv_err(nd, YP_NODOM);
		}

		/*
		** Check the nis_secure argument to see if we can accept
		** this request.
		*/
		if (nsd_attr_fetch_bool(dp->f_attrs, "nis_secure", 0) &&
		    ntohs(nd->sin.sin_port) >= 1024) {
			nsd_logprintf(NSD_LOG_LOW, "\tReturning YP_NOMAP\n");
			return nisserv_err(nd, YP_NOMAP);
		}

		/*
		** Now look for the database file, and reopen it if needed.
		*/
		hf = hash_get_file(dp);
		if (! hf) {
			nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOMAP\n");
			return nisserv_err(nd, YP_NOMAP);
		}
		if (stat(hf->file, &sbuf)) {
			nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOMAP 2\n");
			return nisserv_err(nd, YP_NOMAP);
		}
		if ((! hf->map) || (hf->version < sbuf.st_mtime)
		    || _Mdbm_invalid(hf->map)) {
			if (! hash_reopen(hf)) {
				nsd_logprintf(NSD_LOG_LOW,
				    "\treturning YP_BADDB 3\n");
				return nisserv_err(nd, YP_BADDB);
			}
			hf->version = sbuf.st_mtime;
		}

		if (! pagesize) {
			pagesize = MDBM_PAGE_SIZE(hf->map);
			key = alloca(pagesize);
			val = alloca(pagesize);
		}

		/*
		** We may sleep when the socket buffer fills so we need
		** a callback to deal with when the buffer empties.
		*/
		nsd_callback_new(nd->fd, dispatch_tcp, NSD_WRITE);

		/*
		** Build out-going header.
		*/
		p = (uint32_t *)_nisserv_buf;
		p++;

		p += rpc_hdr(nd, p);
		break;
	    case 4:
		/*
		** We get here if we were in the middle of writing data
		** and found we would block.  We reopen the hash file if
		** needed, then jump back to where we were.
		*/

		/*
		** Walk the tree looking for the callout directory.
		*/
		if (! __nsd_mounts ||
		    ! (dp = nsd_file_byname(__nsd_mounts->f_data,
		    nd->domain.string, nd->domain.len))) {
			nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NODOM\n");
			goto tcperror;
		}

		dp = nsd_file_byname(dp->f_data, nd->table.string,
		    nd->table.len);
		if (! dp) {
			nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOMAP\n");
			goto tcperror;
		}

		dp2 = nsd_file_byname(dp->f_callouts, ".nisserv", 8);
		if (! dp2) {
			dp2 = nsd_file_byname(dp->f_callouts, ".ypserv", 7);
			if (! dp2) {
				nsd_logprintf(NSD_LOG_LOW,
				    "\treturning YP_NODOM\n");
				goto tcperror;
			}
		}
		dp = dp2;

		if (! nis_check(nd->cred, dp)) {
			nsd_logprintf(NSD_LOG_LOW, "\tpermission denied\n");
			goto tcperror;
		}

		/*
		** Now look for the database file, and reopen it if needed.
		*/
		hf = hash_get_file(dp);
		if (! hf) {
			nsd_logprintf(NSD_LOG_MIN,
			    "dispatch_tcp: got NULL hash pointer\n");
			goto tcperror;
		}
		if (stat(hf->file, &sbuf)) {
			nsd_logprintf(NSD_LOG_LOW, "\treturning YP_NOMAP 2\n");
			return nisserv_err(nd, YP_NOMAP);
		}
		if ((! hf->map) || (hf->version < sbuf.st_mtime)
		    || _Mdbm_invalid(hf->map)) {
			if (! hash_reopen(hf)) {
				nsd_logprintf(NSD_LOG_RESOURCE,
				    "\tfailed to reopen hash file\n");
				goto tcperror;
			}
			hf->version = sbuf.st_mtime;
		}

		if (! pagesize) {
			pagesize = MDBM_PAGE_SIZE(hf->map);
			key = alloca(pagesize);
			val = alloca(pagesize);
		}

		/*
		** Lookup saved key in database to reset hints.
		*/
		kv.key.dptr = nd->key.string;
		kv.key.dsize = nd->key.len;
		kv.val.dsize = 0;
		kv.val.dptr = 0;
		mdbm_fetch(hf->map, kv);

		if (nd->data && (nd->size > 0)) {
			memcpy(_nisserv_buf, nd->data, nd->size);
			free(nd->data);
			nd->data = (char *)0;
		}
		len = nd->size;
		nd->size = 0;

		nsd_logprintf(NSD_LOG_LOW, "len = %d, nd->len = %d\n",
		    len, nd->len);
		p = (uint32_t *)(_nisserv_buf + len);
		q = _nisserv_buf + nd->len;
		goto writeagain;
	    case 5:
		goto writeend;
	}

	/*
	** Walk through the hash file printing each entry.  We write
	** out 4KB at a time.
	*/
	for (kv.key.dptr = key , kv.key.dsize = pagesize,
	    kv.val.dptr = val , kv.val.dsize = pagesize,
	    kv = mdbm_first(hf->map,kv);
	    kv.key.dptr;
	    kv.key.dptr = key , kv.key.dsize = pagesize,
	    kv.val.dptr = val , kv.val.dsize = pagesize,
	    kv = mdbm_next(hf->map,kv)) {
		/* Skip private records. */
		if ((kv.key.dsize == 0) || ((kv.key.dsize > 3) &&
		    (strncmp(kv.key.dptr, "YP_", 3) == 0))) {
			continue;
		}

		*p++ = htonl(TRUE);
		*p++ = htonl(TRUE);
		*p++ = htonl(kv.val.dsize);
		if (kv.val.dsize > 0) {
			p[kv.val.dsize/sizeof(*p)] = 0;
			memcpy(p, kv.val.dptr, kv.val.dsize);
			p += WORDS(kv.val.dsize);
		}

		*p++ = htonl(kv.key.dsize);
		if (kv.key.dsize > 0) {
			p[kv.key.dsize/sizeof(*p)] = 0;
			memcpy(p, kv.key.dptr, kv.key.dsize);
			p += WORDS(kv.key.dsize);
		}

		if (p >= end) {
			q = _nisserv_buf;
			*(uint32_t *)_nisserv_buf =
			    htonl((char *)end - q - sizeof(*p));
writeagain:
			len = (char *)end - q;
			qlen = write(nd->fd, q, len);
			if (qlen < len) {
				if (qlen < 0) {
					if (errno == EWOULDBLOCK) {
						nd->len = q - _nisserv_buf;
						len = (char *)p - _nisserv_buf;
						p = (uint32_t *)_nisserv_buf;
						nd->status = 4;
						memcpy(nd->key.string,
						    kv.key.dptr, kv.key.dsize);
						nd->key.len = kv.key.dsize;
						goto needmore;
					} else {
						nsd_logprintf(NSD_LOG_LOW,
						    "\terror in write: %s\n",
						    strerror(errno));
						goto tcperror;
					}
				}

				q += qlen;
				goto writeagain;
			}

			len = (p - end) * sizeof(*p);
			if (len > 0) {
				memcpy(_nisserv_buf + sizeof(*p), end, len);
				p = (uint32_t *)(_nisserv_buf + len);
			} else {
				p = (uint32_t *)_nisserv_buf;
			}
			p++;
		}
	}

	*p++ = htonl(FALSE);
	len = (char *)p - _nisserv_buf;
	p = (uint32_t *)_nisserv_buf;
	*p = htonl(0x80000000 + len - sizeof(*p));
writeend:
	qlen = write(nd->fd, p, len);
	if (qlen < len) {
		if (qlen < 0) {
			if (errno == EWOULDBLOCK) {
				nd->status = 5;
				goto needmore;
			} else {
				nsd_logprintf(NSD_LOG_LOW,
				    "\terror on final write: %s\n",
				    strerror(errno));
				goto tcperror;
			}
		}

		len -= qlen;
		p += qlen/sizeof(*p);
		goto writeend;
	}

	nsd_callback_remove(nd->fd);
	close(nd->fd);
	nisserv_data_clear(&nd);
	return NSD_CONTINUE;

tcperror:
	nsd_callback_remove(nd->fd);
	close(nd->fd);
	if (nd) {
		nisserv_data_clear(&nd);
	}
	nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_CONTINUE\n");
	return NSD_CONTINUE;

needmore:
	nsd_timeout_new((nsd_file_t *)nd, 30000, nisserv_timeout, (void *)nd);
	if (len > 0) {
		nd->data = (char *)nsd_malloc(len);
		if (! nd->data) {
			nsd_callback_remove(nd->fd);
			close(nd->fd);
			nsd_timeout_remove((nsd_file_t *)nd);
			nisserv_data_clear(&nd);
			return NSD_CONTINUE;
		}
		memcpy(nd->data, p, len);
		nd->size = len;
		nsd_logprintf(NSD_LOG_LOW,
		    "\treturning NSD_CONTINUE: needmore, status = %d\n",
		    nd->status);
		return NSD_CONTINUE;
	}
	nsd_logprintf(NSD_LOG_LOW,
	    "\treturning NSD_CONTINUE: needmore 2, status = %d\n", nd->status);
	return NSD_CONTINUE;
}

/*
** This routine reads in an entire request then hands it off to be
** processed by another.
*/
static int
dispatch_tcp(nsd_file_t **rqp, int fd)
{
	nisserv_t *nd;
	int n, i;
	u_int32_t *p=0;
	struct sockaddr_in sin;

	nsd_logprintf(NSD_LOG_MIN, "entering dispatch_tcp:\n");
	*rqp = 0;

	/*
	** Locate NIS data for connection.
	*/
	nd = nisserv_data_byfd(fd);
	if (! nd) {
		nsd_logprintf(NSD_LOG_LOW,
		    "dispatch_tcp: Cannot find request structure for "
		    "file descriptor: %d\n", fd);
		goto disperr;
	}

	nsd_timeout_remove((nsd_file_t *)nd);

	switch (nd->status) {
	    case 0:
		/*
		** First time through.  We need to determine the size of
		** the request and read in as much as we can.
		*/
		n = read(fd, &nd->size, sizeof(nd->size));
		if (n < sizeof(nd->size)) {
			nsd_logprintf(NSD_LOG_LOW,
			    "dispatch_tcp: read failed: %s\n",
			    strerror(errno));
			goto disperr;
		}

		if (nd->size & 0x80000000) {
			nd->size &= ~0x80000000;
		} else {
			nsd_logprintf(NSD_LOG_LOW,
			    "dispatch_tcp: continuation packet on request\n");
			goto disperr;
		}
		if (nd->size > 4096) {
			nsd_logprintf(NSD_LOG_LOW,
			    "dispatch_tcp: request too large\n");
			return reject_err(nd, GARBAGE_ARGS);
		}

		nd->data = nsd_calloc(1, nd->size);
		if (! nd->data) {
			nsd_logprintf(NSD_LOG_LOW,
			    "dispatch_tcp: failed malloc\n");
			return accept_err(nd, SYSTEM_ERR);
		}
		nd->size -= sizeof(nd->size);

	    case 1:
		/*
		** We have started to read in the packet.  Here we read
		** as much as is available.
		*/
		n = read(fd, nd->data + nd->len,
		    nd->size + sizeof(nd->size) - nd->len);
		switch (n) {
		    case -1:
			if (errno != EWOULDBLOCK) {
				nsd_logprintf(NSD_LOG_LOW,
				    "dispatch_tcp: read failed: %s\n",
				    strerror(errno));
				goto disperr;
			}
			n = 0;
			break;
		    case 0:
			goto disperr;
		}
		nd->len += n;
		if (nd->len < nd->size) {
			nd->status = 1;
			nsd_timeout_new((nsd_file_t *)nd, 5000,
			    nisserv_timeout, (void *)nd);
			nsd_logprintf(NSD_LOG_HIGH,
			    "dispatch_tcp: incomplete read, waiting "
			    "for more: %d of %d\n", nd->len, nd->size);
			return NSD_CONTINUE;
		}

	    case 2:
		n = sizeof(sin);
		if (getpeername(fd, &sin, &n) < 0) {
			nsd_logprintf(NSD_LOG_LOW,
			    "dispatch_tcp: getpeername failed: %s\n",
			    strerror(errno));
			return reject_err(nd, AUTH_FAILED);
		}

		/*
		** If this is not coming from one of our supported networks
		** then we ignore the request.
		*/
		if (! nisserv_insecurenets(&sin.sin_addr)) {
			nsd_logprintf(NSD_LOG_MIN,
			    "dispatch_udp: request from insecure network %s\n",
			    inet_ntoa(sin.sin_addr));
			return reject_err(nd, AUTH_FAILED);
		}

		p = (uint32_t *)nd->data;
		nd->xid = ntohl(p[0]);
		if (ntohl(p[1]) != CALL) {
			return NSD_CONTINUE;
		}

		nd->fd = fd;
		nd->sin = sin;

		if (ntohl(p[2]) != RPC_MSG_VERSION) {
			return reject_err(nd, RPC_MISMATCH);
		}
		if ((ntohl(p[3]) != YPPROG) || (ntohl(p[4]) != YPVERS)) {
			return accept_err(nd, PROG_MISMATCH);
		}
		nd->cmd = ntohl(p[5]);

		/*
		** Remember credentials.
		*/
		switch (ntohl(p[6])) {
		    case AUTH_NONE:
			nd->cred = nsd_cred_new(0, 1, 0);
			if (! nd->cred) {
				return accept_err(nd, SYSTEM_ERR);
			}
			p += WORDS(ntohl(p[7]));
			p += 8;
			break;
		    case AUTH_UNIX:
			p += WORDS(ntohl(p[9]));
			p += 10;
			nd->cred = nsd_cred_new((uid_t)ntohl(p[0]), 0);
			if (! nd->cred) {
				return accept_err(nd, SYSTEM_ERR);
			}
			nd->cred->c_gid[0] = (gid_t)ntohl(p[1]);
			nd->cred->c_gids = ntohl(p[2]);
			for (i = 0; i < nd->cred->c_gids; i++) {
				nd->cred->c_gid[i+1] = (gid_t)ntohl(p[i+3]);
			}
			p += (i + 3);
			break;
		    case AUTH_DES:
			nsd_logprintf(NSD_LOG_OPER, 
			    "dispatch_udp: received AUTH_DES authorization\n");
			return reject_err(nd, AUTH_FAILED);
		    default:
			nsd_logprintf(NSD_LOG_OPER, 
			    "dispatch_udp: received unknown authentication "
			    "type 0x%x\n", ntohl(p[6]));
			return reject_err(nd, AUTH_FAILED);
		}

		/* skip verf */
		p += WORDS(ntohl(p[1]));
		p += 2;
		nd->status = 3;

		nsd_callback_remove(fd);
	}

	switch (nd->cmd) {
	    case YPPROC_NULL:
		return nisserv_null(rqp, nd, p);
	    case YPPROC_DOMAIN:
		return nisserv_domain(rqp, nd, p, 1);
	    case YPPROC_DOMAIN_NONACK:
		return nisserv_domain(rqp, nd, p, 0);
	    case YPPROC_MATCH:
		return nisserv_match(rqp, nd, p);
	    case YPPROC_FIRST:
		return nisserv_first(rqp, nd, p);
	    case YPPROC_NEXT:
		return nisserv_next(rqp, nd, p);
	    case YPPROC_XFR:
		return nisserv_xfr(rqp, nd, p);
	    case YPPROC_CLEAR:
		return nisserv_clear(rqp, nd, p);
	    case YPPROC_ALL:
		return nisserv_all(rqp, nd, p);
	    case YPPROC_MASTER:
		return nisserv_master(rqp, nd, p);
	    case YPPROC_ORDER:
		return nisserv_order(rqp, nd, p);
	    case YPPROC_MAPLIST:
		return nisserv_maplist(rqp, nd, p);
	    case YPPROC_NEWXFR:
		return nisserv_newxfr(rqp, nd, p);
	}

	return accept_err(nd, PROC_UNAVAIL);

disperr:
	nsd_callback_remove(fd);
	close(fd);
	if (nd) {
		nisserv_data_clear(&nd);
	}
	nsd_logprintf(NSD_LOG_LOW, "\treturning NSD_CONTINUE\n");
	return NSD_CONTINUE;
}

/*
** This routine just handles setting up a TCP connection.
*/
/*ARGSUSED*/
static int
accept_tcp(nsd_file_t **rqp, int fd)
{
	int rsock, n;
	struct sockaddr_in sin;
	nisserv_t *nd;

	nsd_logprintf(NSD_LOG_MIN, "entering accept_tcp:\n");

	/*
	** We have fallen out of select so we must have a connection
	** ready to be accepted on.
	*/
	n = sizeof(sin);
	rsock = accept(fd, &sin, &n);
	if (rsock < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "accept_tcp: error in accept: %s\n", strerror(errno));
		return NSD_CONTINUE;
	}

	if (! nisserv_insecurenets(&sin.sin_addr)) {
		nsd_logprintf(NSD_LOG_MIN,
		    "accept_tcp: request from insecure network %s\n",
		    inet_ntoa(sin.sin_addr));
		close(rsock);
		return NSD_CONTINUE;
	}

	nd = nisserv_data_new();
	if (! nd) {
		nsd_logprintf(NSD_LOG_RESOURCE, "accept_tcp: malloc failed\n");
		close(rsock);
		return NSD_CONTINUE;
	}

	nd->fd = rsock;
	nd->proto = IPPROTO_TCP;
	memcpy(&nd->sin, &sin, sizeof(nd->sin));
	if (tsix_get_mac(nd->fd, &nd->lbl) == -1) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "accept_tcp: tsix_get_mac failed\n");
		nisserv_data_clear(&nd);
		close(rsock);
		return NSD_CONTINUE;
	}
	if (tsix_set_mac(nd->fd, nd->lbl) == -1) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "accept_tcp: tsix_set_mac failed\n");
		nisserv_data_clear(&nd);
		close(rsock);
		return NSD_CONTINUE;
	}

	nsd_timeout_new((nsd_file_t *)nd, 5000, nisserv_timeout, (void *)nd);
	nsd_callback_new(rsock, dispatch_tcp, NSD_READ);

	return NSD_CONTINUE;
}

/*
** Check if the nis server should run.
*/
static int
chkconfig_ypserv(void) {
	char buf[16];
	int fd;

	fd = nsd_open("/etc/config/ypserv", O_RDONLY, 0);
	if (fd < 0) {
		nsd_logprintf(NSD_LOG_CRIT,
		    "ypserv is not enabled by chkconfig\n");
		return FALSE;
	}
	if (read(fd, buf, sizeof(buf)) < 0) {
		close(fd);
		nsd_logprintf(NSD_LOG_RESOURCE,"failed to chkconfig ypserv\n");
		return FALSE;
	}
	close(fd);
	if (strncasecmp(buf, "on", 2) == 0) {
		return TRUE;
	} else {
		nsd_logprintf(NSD_LOG_CRIT,
		    "ypserv is not enabled by chkconfig\n");
		return FALSE;
	}
}

/*
** The init function is called when the library is first opened or when
** the daemon receives a signal.  We just set the state back to startup
** by freeing all the cached files, open up sockets for the NIS service
** and register the service with the portmapper.
*/
/* ARGSUSED */
int
init(char *map)
{
	struct sockaddr_in sin;
	int len = sizeof(struct sockaddr_in);
	struct timeval t;
	int n;

	nsd_logprintf(NSD_LOG_MIN, "entering init (nisserv):\n");

	/*
	** If the old ypserv chkconfig esists then we believe it.
	*/
	if (! chkconfig_ypserv()) {
		return NSD_ERROR;
	}

	hash_clear();
	gettimeofday(&t);
	_nsdata_xid = getpid() ^ t.tv_sec ^ t.tv_usec;

	if (gethostname(_nisserv_host.string,
	    sizeof(_nisserv_host.string)) < 0) {
		strcpy(_nisserv_host.string, "IRIS");
		_nisserv_host.len = 4;
		_nisserv_host.words = 1;
	} else {
		_nisserv_host.len = strlen(_nisserv_host.string);
		_nisserv_host.words = WORDS(_nisserv_host.len);
	}

	_nisserv_euid = geteuid();
	_nisserv_egid = getegid();

	/*
	** Setup service sockets, bind them to reserved port, and register
	** the services with the portmapper.
	*/
	if (_nisserv_udp < 0) {
		_nisserv_udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (_nisserv_udp < 0) {
			nsd_logprintf(NSD_LOG_RESOURCE, 
			    "init (nisserv): failed to create UDP socket: %s\n",
			    strerror(errno));
			return NSD_ERROR;
		}
		if (tsix_on(_nisserv_udp) == -1) {
			close(_nisserv_udp);
			_nisserv_udp = -1;
			return NSD_ERROR;
		}
	}

	memset(&sin, 0, len);
	sin.sin_family = AF_INET;
	if (bindresvport(_nisserv_udp, &sin) < 0) {
		sin.sin_port = 0;
		bind(_nisserv_udp, (struct sockaddr *)&sin, len);
	}

	if (getsockname(_nisserv_udp, (struct sockaddr *)&sin, &len) < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "init (nisserv): failed to bind UDP socket: %s\n",
		    strerror(errno));
		close(_nisserv_udp);
		_nisserv_udp = -1;
		return NSD_ERROR;
	}

	n = 1;
	setsockopt(_nisserv_udp, IPPROTO_IP, IP_RECVDSTADDR, &n, sizeof(n));

	if (! __nsd_mounts) {
		nsd_portmap_unregister(YPPROG, YPVERS);
	}
	nsd_portmap_register(YPPROG, YPVERS, IPPROTO_UDP, sin.sin_port);
	nsd_callback_new(_nisserv_udp, dispatch_udp, NSD_READ);
	nsd_logprintf(NSD_LOG_LOW, "Added nisserv UDP on port %d\n",
	    sin.sin_port);


	if (_nisserv_tcp < 0) {
		_nisserv_tcp = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (_nisserv_tcp < 0) {
			nsd_logprintf(NSD_LOG_RESOURCE, 
			    "init (nisserv): failed to create TCP socket: %s\n",
			    strerror(errno));
			return NSD_ERROR;
		}
		if (tsix_on(_nisserv_tcp) == -1) {
			close(_nisserv_tcp);
			_nisserv_tcp = -1;
			return NSD_ERROR;
		}
	}

	len = sizeof(struct sockaddr_in);
	memset(&sin, 0, len);
	sin.sin_family = AF_INET;
	if (bindresvport(_nisserv_tcp, &sin) < 0) {
		sin.sin_port = 0;
		bind(_nisserv_tcp, (struct sockaddr *)&sin, len);
	}

	if (getsockname(_nisserv_tcp, (struct sockaddr *)&sin, &len) < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "init (nisserv): failed to bind TCP socket: %s\n",
		    strerror(errno));
		close(_nisserv_tcp);
		_nisserv_tcp = -1;
		return NSD_ERROR;
	}

	/*
	** Set non-blocking I/O for the stream socket.  We continue if this
	** fails, but we may block later.
	*/
	if (fcntl(_nisserv_tcp, F_SETFL, FNDELAY) < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,
	    "init (nisserv): failed to set tcp socket for non-blocking I/O\n");
	}

	n = 4096;
	if (setsockopt(_nisserv_tcp, SOL_SOCKET, SO_SNDBUF, &n,
	    sizeof(n)) < 0) {
		nsd_logprintf(NSD_LOG_LOW,
		    "accept_tcp: setsockopt failed: %s\n", strerror(errno));
	}

	nsd_portmap_register(YPPROG, YPVERS, IPPROTO_TCP, sin.sin_port);
	nsd_callback_new(_nisserv_tcp, accept_tcp, NSD_READ);

	if (listen(_nisserv_tcp, 100) < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,
		    "init (nisserv): listen failed: %s\n", strerror(errno));
		return NSD_ERROR;
	}
	nsd_logprintf(NSD_LOG_LOW, "Added nisserv TCP on port %d\n",
	    sin.sin_port);

	securenets_parse();

	return NSD_OK;
}
