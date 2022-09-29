/*
** This file contains the NIS list routine, and some supporting
** functions.  When we get a request we attempt to bind to a
** server, then we open a TCP connection, send a YP_ALL request,
** then wait for the reply to all come in.
**
** The basic state diagram is:
**
**		failure------------failure<-----------failure
**		  |  |			|		    |
**		  |  V			|		    |
**	list() -> BINDING -success-> GETPORT -success-> CONNECTING
**							    |
**							    V
**							 READING <-|
**							    |      |
**							    V      |
**							PROCESSING |
**					    		    |      |
**					      return <-done-|-more-|
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <time.h>

#include <ns_api.h>
#include <ns_daemon.h>

#include "ns_nis.h"

extern nisstring_t _nis_domain;
extern nisstring_t _nis_host;
extern char _nis_result[4096];
extern uint32_t _nis_xid;
extern size_t __pagesize;

#define BINDING		0
#define GETPORT		1
#define CONNECTING	2
#define SENDING		3
#define READING		4
#define PROCESSING	5

#define ROUNDUP(a, b) ((a > 0) ? (((a) + (b) - 1) / (b)) * (b) : (b))

static nisrequest_t *nis_requests = 0;

static int nis_list_bind(nsd_file_t *);

static void
nis_list_clear(void **npp)
{
	nisrequest_t *np;

	if (! npp || ! *npp) {
		return;
	}

	np = *npp;

	if (np->result) {
		free(np->result);
		np->result = 0;
	}

	if (np->fd) {
		close(np->fd);
		nsd_callback_remove(np->fd);
	}

	free(np);
	*npp = 0;
}

/*
** Store the nis_request structure in a list indexed by file descriptor.
*/
static void
nis_list_save(nisrequest_t *np)
{
	np->next = nis_requests;
	nis_requests = np;
}

/*
** Look up the nis_request structure in our save list and remove it from
** this list if found.
*/
static nisrequest_t *
nis_list_lookup(int fd)
{
	nisrequest_t *np, **last;

	for (np = nis_requests, last = &nis_requests; np && (np->fd != fd);
	    last = &np->next, np = np->next);
	if (np) {
		*last = np->next;
	}

	return np;
}

static int
list_timeout(nsd_file_t **rqp, nsd_times_t *to)
{
	nisrequest_t *np = to->t_file->f_cmd_data;

	nsd_timeout_remove(to->t_file);
	if (np) {
		nis_list_lookup(np->fd);
	}

	*rqp = to->t_file;

	nis_list_clear(&to->t_file->f_cmd_data);
	nsd_logprintf(NSD_LOG_MIN,
	    "nis_list_timeout: no response from server\n");
	to->t_file->f_status = NS_FATAL;
	return NSD_ERROR;
}


/*
** This is essentially a realloc using the mmap'd file malloc from nsd.
*/
int
nis_grow(nsd_file_t *rq, int size)
{
	void *new;

	size = ROUNDUP(size, __pagesize);
	new = nsd_brealloc(rq->f_data, size);
	if (! new) {
		rq->f_used = rq->f_size = 0;
		rq->f_data = (char *)0;
		return 0;
	}
	if (rq->f_data && (rq->f_free != nsd_bfree)) {
		if (rq->f_used) {
			memcpy(new, rq->f_data, rq->f_used);
		}
		(*rq->f_free)(rq->f_data);
	}
	rq->f_free = nsd_bfree;
	rq->f_size = size;
	rq->f_data = new;

	return 1;
}

static int
nis_list_callback(nsd_file_t **rqp, int fd)
{
	int n;
	nsd_file_t *rq;
	uint32_t len, keylen, totallen, *p, *start=0;
	char *cp, *buf;
	nisrequest_t *np;

	nsd_logprintf(NSD_LOG_MIN, "entering nis_list_callback:\n");

	np = nis_list_lookup(fd);
	if (! np) {
		close(fd);
		nsd_callback_remove(fd);
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_ERROR -- np\n");
		return NSD_ERROR;
	}
	*rqp = rq = np->request;
	nsd_timeout_remove(rq);

	for (;;) {
		/*
		** If this is a new segment then we read the length.
		*/
		if (! np->last) {
			n = read(fd, _nis_result, sizeof(*p));
			switch (n) {
			    case -1:
				if (errno == EWOULDBLOCK) {
					/* Wait until more data is available. */
					nis_list_save(np);
					nsd_timeout_new(rq, 5000 *
					    nsd_attr_fetch_long(rq->f_attrs,
					    "nis_timeout", 10, 1), list_timeout,
					    np);
					return NSD_CONTINUE;
				}
			    case 0:
				nis_list_clear(&rq->f_cmd_data);
				nsd_logprintf(NSD_LOG_MIN,
				    "nis_list_callback: zero length read\n");
				rq->f_status = NS_FATAL;
				return NSD_ERROR;
			}
			p = (uint32_t *)_nis_result;
			np->last = ntohl(*p);
		}
		len = np->last & 0x7fffffff;
		np->last &= 0x80000000;
		if (len > 16384) {
			/*
			** Does not look like a segment header.
			*/
			nis_list_clear(&rq->f_cmd_data);
			nsd_logprintf(NSD_LOG_MIN,
			    "nis_list_callback: segment too large\n");
			rq->f_status = NS_FATAL;
			return NSD_ERROR;
		}

		/*
		** Build enough of a buffer to hold this segment.
		*/
		if (np->result) {
			len += np->len;
			if (np->result_len < len) {
				np->result =
				    (char *)nsd_realloc(np->result, len);
				np->result_len = len;
			}
		} else {
			np->result = (char *)nsd_malloc(len);
			np->result_len = len;
		}
		if (! np->result) {
			nis_list_clear(&rq->f_cmd_data);
			nsd_logprintf(NSD_LOG_RESOURCE,
			    "nis_list_callback: realloc failed\n");
			rq->f_status = NS_TRYAGAIN;
			return NSD_NEXT;
		}

		/*
		** Read in full segment.
		*/
		while (np->len < len) {
			n = read(fd, np->result + np->len, len - np->len);
			switch (n) {
			    case -1:
				if (errno == EWOULDBLOCK) {
					/* Wait until more data is available. */
					np->last += (len - np->len);
					nis_list_save(np);
					nsd_timeout_new(rq, 5000 *
					    nsd_attr_fetch_long(rq->f_attrs,
					    "nis_timeout", 10, 1), list_timeout,
					    np);
					return NSD_CONTINUE;
				}
				nis_list_clear(&rq->f_cmd_data);
				rq->f_status = NS_FATAL;
				nsd_logprintf(NSD_LOG_HIGH,
				    "\tnis_list_callback: read failed: %s\n",
				    strerror(errno));
				return NSD_ERROR;
			    case 0:
				/* socket closed */
				nis_list_clear(&rq->f_cmd_data);
				rq->f_status = NS_NOTFOUND;
				nsd_logprintf(NSD_LOG_MIN,
				    "nis_list_callback: zero length read\n");
				return NSD_NEXT;
			}
			np->len += n;
		}
		p = (uint32_t *)np->result;

		/*
		** If this is the first time through we have to process
		** the header.
		*/
		if (np->state == READING) {
			if ((ntohl(p[1]) != REPLY) ||
			    (ntohl(p[2]) != MSG_ACCEPTED)) {
				nis_list_clear(&rq->f_cmd_data);
				rq->f_status = NS_NOTFOUND;
				return NSD_NEXT;
			}

			p += 5 + ntohl(p[4])/sizeof(u_int32_t);

			if (ntohl(p[0]) != SUCCESS) {
				nis_list_clear(&rq->f_cmd_data);
				rq->f_status = NS_NOTFOUND;
				return NSD_NEXT;
			}

			np->state = PROCESSING;
			p++;
			np->len -= (char *)p - np->result;
		}

		/*
		** Walk through the data appending it to our result.
		*/
		if (rq->f_data) {
			cp = rq->f_data + rq->f_used;
		} else {
			cp = rq->f_data = (char *)nsd_malloc(1024);
			if (! rq->f_data) {
				nis_list_clear(&rq->f_cmd_data);
				nsd_logprintf(NSD_LOG_RESOURCE,
				    "nis_list_callback: malloc failed\n");
				rq->f_status = NS_TRYAGAIN;
				return NSD_NEXT;
			}
			rq->f_free = free;
			rq->f_used = 0;
			rq->f_size = 1024;
		}
		while (np->len > 0) {
			start = p;
			buf = cp;

			/* More? */
			if (np->len < sizeof(*p)) {
				break;
			}
			if (ntohl(*p++) != TRUE) {
				*cp++ = (char)0;
				rq->f_status = NS_SUCCESS;
				nis_list_clear(&rq->f_cmd_data);
				return NSD_NEXT;
			}
			np->len -= sizeof(*p);

			/* ptr */
			if (np->len < sizeof(*p)) {
				break;
			}
			if (ntohl(*p++) != TRUE) {
				*cp++ = (char)0;
				nis_list_clear(&rq->f_cmd_data);
				rq->f_status = NS_SUCCESS;
				return NSD_NEXT;
			}
			np->len -= sizeof(*p);

			/* Data */
			if (np->len < sizeof(*p)) {
				break;
			}
			len = ntohl(*p++);
			np->len -= sizeof(*p);
			if (len > 16384) {
				/* Record is unexpectedly large */
				nis_list_clear(&rq->f_cmd_data);
				nsd_logprintf(NSD_LOG_MIN,
				    "nis_list_callback: segment too large\n");
				rq->f_status = NS_FATAL;
				return NSD_ERROR;
			}

			if (np->len < len) {
				break;
			}
			
			/* 
			** Add the data length, the key length and one
			** char and make sure that we have both the data 
			** and key completely in the packet we've read in.
			** Also make sure there's enough space allocated
			** in rq->f_data for us.
			** 
			** If NIS_NULL_EXTEND_KEY is set, increase
			** keylen by one.  It's a null and we'll clobber 
			** it with a space later.
			*/
			totallen = len;
			if (np->flags & NIS_ENUMERATE_KEY) {
				if ((np->len < len) ||
				    (np->len < len + sizeof(*p)) ||
				    (np->len < len + *(p + WORDS(len)))) {
					break;
				}
				keylen = *(p + WORDS(len));
				totallen += keylen + 1;
			} 
				
			while (&cp[totallen] >= &rq->f_data[rq->f_size - 1]) {
				/* double buffer size */
				/* XXX - bad form not using nsd_*_result */
				if (! nis_grow(rq, rq->f_size * 2)) {
					nis_list_clear(&rq->f_cmd_data);
					nsd_logprintf(NSD_LOG_RESOURCE,
				    "nis_list_callback: failed malloc\n");
					rq->f_status = NS_TRYAGAIN;
					return NSD_NEXT;
				}
				buf = cp = rq->f_data + rq->f_used;
			}
			
			/* 
			** We now know that we have what we need to provide 
			** to the requester in memory.  If we're prepending 
			** the key, skip ahead to where we need to store the
			** data.  
			*/
			if (np->flags & NIS_ENUMERATE_KEY) {
				memcpy(&cp[keylen+1], p, len);
			} else {
				memcpy(cp, p, len);
			}
			p += WORDS(len);
			np->len -= (WORDS(len)) * sizeof(*p);

			/* Key */
			if (np->len < sizeof(*p)) {
				break;
			}
			len = ntohl(*p++);
			np->len -= sizeof(*p);
			if (len > 16384) {
				/* Record is unexpectedly large */
				nis_list_clear(&rq->f_cmd_data);
				nsd_logprintf(NSD_LOG_MIN,
				    "nis_list_callback: segment too large\n");
				rq->f_status = NS_FATAL;
				return NSD_ERROR;
			}

			/* 
			** Stuff the key plus a space into keyp.  
			** We know we have all of it in memory.
			** XXX: Should be able to specify the separator
			** via an attribute, no?
			*/
			if (np->flags & NIS_ENUMERATE_KEY) {
				if (np->flags & NIS_NULL_EXTEND_KEY) {
					len--;
				}
				memcpy(cp, p, len);
				cp[len] = ' ';
			} else if (np->len < len) {
				break;
			}
			p += WORDS(len);
			np->len -= (WORDS(len)) * sizeof(*p);

			/* Make sure the data ends in \n */
			if (cp[totallen - 1] == '\0') {
				cp[totallen -1] = '\n';
				cp += totallen;
				rq->f_used += totallen;
			} else if (cp[totallen - 1] == '\n') {
				cp += totallen;
				rq->f_used += totallen;
			} else {
				cp[totallen] = '\n';
				cp += (totallen + 1);
				rq->f_used += (totallen + 1);
			}
		}

		if (np->last) {
			/* Huh?  We were told this was the last packet. */
			nis_list_clear(&rq->f_cmd_data);
			nsd_logprintf(NSD_LOG_MIN,
			    "nis_list_callback: unexpected status\n");
			rq->f_status = NS_FATAL;
			return NSD_ERROR;
		}

		/*
		** Undo the last copy if we ran out of data after it.
		*/
		if (cp > buf) {
			rq->f_used -= cp - buf;
		}

		/*
		** Save the data from the last item.
		*/
		if (start && ((char *)start != np->result)) {
			np->len += (p - start) * sizeof(*p);
			if (np->len) {
				memmove(np->result, start, np->len);
			}
		}
	}
}

static int
nis_list_send(nsd_file_t **rqp, int fd)
{
	nisrequest_t *np;
	uint32_t *p, *save;
	int len, n;
	char *cp;
	nsd_file_t *rq;

	nsd_logprintf(NSD_LOG_MIN, "entering nis_list_send:\n");

	np = nis_list_lookup(fd);
	if (! np) {
		close(fd);
		nsd_callback_remove(fd);
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_ERROR\n");
		return NSD_ERROR;
	}
	*rqp = rq = np->request;

	/*
	** Encode YP_ALL request, and write it to socket.
	*/
	p = (uint32_t *)_nis_result;
	save = p++;
	*p++ = htonl(++_nis_xid);
	*p++ = htonl(CALL);
	*p++ = htonl(RPC_MSG_VERSION);
	*p++ = htonl(YPPROG);
	*p++ = htonl(YPVERS);
	*p++ = htonl(YPPROC_ALL);

	/* auth */
	*p++ = 0;
	*p++ = 0;

	/* verf */
	*p++ = 0;
	*p++ = 0;

	/* nis req */
	*p++ = htonl(np->domain.len);
	p[np->domain.len/sizeof(*p)] = 0;
	memcpy(p, np->domain.string, np->domain.len);
	p += np->domain.words;

	*p++ = htonl(np->map.len);
	p[np->map.len/sizeof(*p)] = 0;
	memcpy(p, np->map.string, np->map.len);
	p += np->map.words;

	len = (char *)p - _nis_result;

	*save = htonl(len + 0x80000000);

	cp = _nis_result;
	while (len > 0) {
		n = write(fd, cp, len);
		switch (n) {
		    case -1:
			if (errno == EWOULDBLOCK) {
				continue;
			}

			/*
			** Connect must have failed.  We unbind and start over.
			*/
			nsd_logprintf(NSD_LOG_RESOURCE, "nis_list_send: write failed: %s\n",
			    strerror(errno));
			close(fd);
			nsd_callback_remove(fd);
			np->fd = 0;
			np->server->sin.sin_port = 0;
			np->server->tcp_port = 0;
			np->server->sin.sin_addr.s_addr = 0;
			np->state = BINDING;
			return nis_list_bind(rq);
		    default:
			len -= n;
			cp += n;
		}
	}

	nsd_callback_remove(fd);
	nsd_callback_new(fd, nis_list_callback, NSD_READ);
	nis_list_save(np);
	np->state = READING;
	nsd_timeout_new(rq, 5000 * nsd_attr_fetch_long(rq->f_attrs,
	    "nis_timeout", 10, 1), list_timeout, np);
	nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_CONTINUE\n");
	return NSD_CONTINUE;
}

/*
** The connect routine opens the socket and begins the connect system
** call.  If we succeeded in setting FNDELAY then this should return
** immediately.
*/
static int
nis_list_connect(nsd_file_t *rq)
{
	int fd;
	nisrequest_t *np;
	struct sockaddr_in sin;

	nsd_logprintf(NSD_LOG_MIN, "entering nis_list_connect:\n");

	np = (nisrequest_t *)rq->f_cmd_data;

	/*
	** Create a new socket.
	*/
	fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd < 0) {
		nis_list_clear(&rq->f_cmd_data);
		rq->f_status = NS_TRYAGAIN;
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_NEXT\n");
		return NSD_NEXT;
	}
	np->fd = fd;
	nsd_callback_new(fd, nis_list_send, NSD_WRITE);

	/*
	** Mark socket for non-blocking.
	*/
	if (fcntl(fd, F_SETFL, FNDELAY) < 0) {
		nsd_logprintf(NSD_LOG_RESOURCE,
	"nis_list_connect: failed to set tcp socket for non-blocking I/O\n");
	}

	/*
	** Attempt to connect.
	*/
	sin = np->server->sin;
	sin.sin_port = np->server->tcp_port;
	nsd_logprintf(NSD_LOG_HIGH, "\tconnecting to: %s.%d\n", inet_ntoa(sin.sin_addr),
	    sin.sin_port);
	if (connect(fd, &sin, sizeof(sin)) < 0) {
		if (errno == EINPROGRESS) {
			np->state = CONNECTING;
			nis_list_save(np);
			nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_CONTINUE\n");
			return NSD_CONTINUE;
		}
		nis_list_clear(&rq->f_cmd_data);
		rq->f_status = NS_TRYAGAIN;
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_NEXT\n");
		return NSD_NEXT;
	}

	nis_list_save(np);
	return nis_list_send(&rq, fd);
}

/*
** This routine just calls the nsd_portmap_getport routine to fill in
** the tcp_port.  If we fail then we try rebinding to another host.
*/
static int
nis_list_getport(nsd_file_t **rqp, void *rqv)
{
	nsd_file_t *rq;
	nisrequest_t *np;

	nsd_logprintf(NSD_LOG_MIN, "entering nis_list_getport:\n");

	rq = (nsd_file_t *)rqv;
	if (rqp) {
		*rqp = rq;
	}
	np = rq->f_cmd_data;

	/*
	** If we have the tcp port already we just use it.
	*/
	if (np->server->tcp_port) {
		return nis_list_connect(rq);
	} else {
		if (np->state == GETPORT) {
			/* we failed to talk to portmap, so we rebind */
			np->server->sin.sin_port = 0;
			np->server->tcp_port = 0;
			np->state = BINDING;
			return nis_list_bind(rq);
		} else {
			np->state = GETPORT;
			return nsd_portmap_getport(YPPROG, YPVERS, IPPROTO_TCP,
			    &np->server->sin.sin_addr, nis_list_getport,
			    &np->server->tcp_port, rq);
		}
	}
}

static int
bind_timeout(nsd_file_t **rqp, nsd_times_t *to)
{
	nsd_timeout_remove(to->t_file);

	*rqp = to->t_file;

	return nis_list_bind(to->t_file);
}

/*
** This routine just loops trying to bind to a server.
*/
static int
nis_list_bind(nsd_file_t *rq)
{
	nisrequest_t *np;
	time_t now;

	nsd_logprintf(NSD_LOG_MIN, "entering nis_list_bind:\n");

	np = (nisrequest_t *)rq->f_cmd_data;

	/*
	** Bump the counter, and return an error if we have tried too
	** many times.
	*/
	if (--(np->count) <= 0) {
		nis_list_clear(&rq->f_cmd_data);
		rq->f_status = NS_UNAVAIL;
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_NEXT\n");
		return NSD_NEXT;
	}

	/*
	** If we are not bound we attempt to bind.
	*/
	if (np->server->sin.sin_port) {
		return nis_list_getport(0, (void *)rq);
	} else {
		np->state = BINDING;
		time(&now);
		if (now > np->server->again) {
			/*
			** Send a bind broadcast.
			*/
			np->proc = nis_list_bind;
			np->server->again = now + BIND_TIME;
			return nis_bind_broadcast(rq);
		} else {
			/*
			** New request, still waiting for bind
			** to complete.
			*/
			nsd_timeout_new(rq, 1000, bind_timeout, np);
		}
	}

	nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_CONTINUE\n");
	return NSD_CONTINUE;
}

/*
** This is the toplevel list routine.  We just allocate a structure
** to save state, fill it in, and start walking down the state diagram.
*/
int
list(nsd_file_t *rq)
{
	nisrequest_t *np;
	char *domain, *map;

	nsd_logprintf(NSD_LOG_MIN, "entering list (nis):\n");

	/*
	** First we allocate our state structure and fill it in.
	*/
	np = (nisrequest_t *)nsd_calloc(1, sizeof(nisrequest_t));
	if (! np) {
		rq->f_status = NS_TRYAGAIN;
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_NEXT\n");
		return NSD_NEXT;
	}
	np->count = nsd_attr_fetch_long(rq->f_attrs, "nis_retries", 10, 5);

	domain = nsd_attr_fetch_string(rq->f_attrs, "domain", 0);
	if (domain) {
		np->domain.len = strlen(domain);
		np->domain.words = WORDS(np->domain.len);
		strncpy(np->domain.string, domain, YPMAXSTRING);
	} else {
		np->domain = _nis_domain;
	}

	map = nsd_attr_fetch_string(rq->f_attrs, "table", 0);
	if (map) {
		np->map.len = strlen(map);
		np->map.words = WORDS(np->map.len);
		strncpy(np->map.string, map, YPMAXSTRING);
	} else {
		rq->f_status = NS_BADREQ;
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_ERROR\n");
		return NSD_ERROR;
	}

	/*  
	** Set attribute bits here to prevent redundant nsd_attr_fetch's
	** later.
	*/
	if (nsd_attr_fetch_bool(rq->f_attrs, "nis_enumerate_key", FALSE) ||
	    nsd_attr_fetch_bool(rq->f_attrs, "enumerate_key", FALSE)) {
		np->flags |= NIS_ENUMERATE_KEY;
	}
	if (nsd_attr_fetch_bool(rq->f_attrs, "null_extend_key", FALSE)) {
		np->flags |= NIS_NULL_EXTEND_KEY;
	}

	np->server = nis_domain_server(np->domain.string);
	if (! np->server) {
		rq->f_status = NS_TRYAGAIN;
		free(np);
		nsd_logprintf(NSD_LOG_HIGH, "\treturning NSD_ERROR\n");
		return NSD_ERROR;
	}

	rq->f_cmd_data = np;
	np->request = rq;

	if (np->server->sin.sin_port) {
		return nis_list_getport(0, (void *)rq);
	} else {
		return nis_list_bind(rq);
	}
}
