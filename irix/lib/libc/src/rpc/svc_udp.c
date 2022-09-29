/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.24 88/02/08 
 */


/*
 * svc_udp.c,
 * Server side for UDP/IP based RPC.  (Does some caching in the hopes of
 * achieving execute-at-most-once semantics.)
 */

#ifdef __STDC__
	#pragma weak svcudp_bufcreate = _svcudp_bufcreate
	#pragma weak svcudp_create = _svcudp_create
	#pragma weak svcudp_enablecache = _svcudp_enablecache
#endif
#include "synonyms.h"

#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/errorhandler.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>	/* prototype for strerror() */
#include <bstring.h>	/* prototype for bzero() */
#include <unistd.h>	/* prototype for close() */
#include <stdlib.h>
#include "priv_extern.h"

#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/mac.h>
#include <sys/sesmgr.h>
#include <t6net.h>
#include <pthread.h>
#include <mplib.h>

extern int __svc_label_agile;

extern pthread_mutex_t svc_mutex;

#define rpc_buffer(xprt) ((xprt)->xp_p1)
#ifndef MAX
#define MAX(a, b)	 ((a > b) ? a : b)
#endif

static bool_t svcudp_recv(register SVCXPRT *xprt, struct rpc_msg *msg);
static bool_t svcudp_reply(register SVCXPRT *xprt, struct rpc_msg *msg);
static enum xprt_stat svcudp_stat(SVCXPRT *xprt);
static bool_t svcudp_getargs(SVCXPRT *, xdrproc_t, void *);
static bool_t svcudp_freeargs(SVCXPRT *, xdrproc_t, void *);
static void svcudp_destroy(register SVCXPRT *xprt);
#ifdef sgi
static		int cache_get(SVCXPRT *, struct rpc_msg *, char **, u_long *);
static		void cache_set(SVCXPRT *, u_long);
#endif

static struct xp_ops svcudp_op = {
	svcudp_recv,
	svcudp_stat,
	svcudp_getargs,
	svcudp_reply,
	svcudp_freeargs,
	svcudp_destroy
};

/*
 * kept in xprt->xp_p2
 */
struct svcudp_data {
	u_int   su_iosz;	/* byte size of send.recv buffer */
	u_long	su_xid;		/* transaction id */
	XDR	su_xdrs;	/* XDR handle */
	char	su_verfbody[MAX_AUTH_BYTES];	/* verifier body */
	char * 	su_cache;	/* cached data, NULL if no cache */
};
#define	su_data(xprt)	((struct svcudp_data *)(xprt->xp_p2))

/*
 * Usage:
 *	xprt = svcudp_bufcreate(sock);
 *
 * If sock<0 then a socket is created, else sock is used.
 * If the socket, sock is not bound to a port then svcudp_create
 * binds it to an arbitrary port.  In any (successful) case,
 * xprt->xp_sock is the registered socket number and xprt->xp_port is the
 * associated port number.
 * Once *xprt is initialized, it is registered as a transporter;
 * see (svc.h, xprt_register).
 * The routines returns NULL if a problem occurred.
 */
SVCXPRT *
svcudp_bufcreate(register int sock, u_int sendsz, u_int recvsz)
{
	bool_t madesock = FALSE;
	register SVCXPRT *xprt = NULL;
	register struct svcudp_data *su = NULL;
	struct sockaddr_in addr;
	int len = sizeof(struct sockaddr_in);

	if (sock == RPC_ANYSOCK) {
		if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
			_rpc_errorhandler(LOG_ERR, "svcudp_bufcreate: socket creation problem: %s", strerror(errno));
			return ((SVCXPRT *)NULL);
		}
		madesock = TRUE;
	}
	if (__svc_label_agile && tsix_on (sock) == -1) {
		_rpc_errorhandler(LOG_ERR, "svcudp_bufcreate: tsix_on failed: %s", strerror(errno));
		goto err_out;
	}
	bzero((char *)&addr, sizeof (addr));
	addr.sin_family = AF_INET;
	if (bindresvport(sock, &addr)) {
		addr.sin_port = 0;
		(void)bind(sock, (struct sockaddr *)&addr, len);
	}
	if (getsockname(sock, (struct sockaddr *)&addr, &len) != 0) {
		_rpc_errorhandler(LOG_ERR, "svcudp_bufcreate - cannot getsockname: %s", strerror(errno));
		goto err_out;
	}
	if ((xprt = svc_xprt_alloc()) == NULL) {
		_rpc_errorhandler(LOG_ERR, "svcudp_bufcreate: out of memory");
		goto err_out;
	}
	su = (struct svcudp_data *)mem_alloc(sizeof(*su));
	if (su == NULL) {
		_rpc_errorhandler(LOG_ERR, "svcudp_bufcreate: out of memory");
		goto err_out;
	}
	su->su_iosz = ((MAX(sendsz, recvsz) + 3) / 4) * 4;
	if ((rpc_buffer(xprt) = mem_alloc(su->su_iosz)) == NULL) {
		_rpc_errorhandler(LOG_ERR, "svcudp_bufcreate: out of memory");
		goto err_out;
	}
	xdrmem_create(
		&(su->su_xdrs), rpc_buffer(xprt), su->su_iosz, XDR_DECODE);
	su->su_cache = NULL;
	xprt->xp_p2 = (caddr_t)su;
	xprt->xp_verf.oa_base = su->su_verfbody;
	xprt->xp_ops = &svcudp_op;
	xprt->xp_port = ntohs(addr.sin_port);
	xprt->xp_sock = sock;
	xprt_register(xprt);
	return (xprt);
err_out:
	if (su)
		mem_free((char *) su, sizeof(*su));
	if (xprt)
		svc_xprt_free(xprt);
	if (madesock)
		(void)close(sock);
	return ((SVCXPRT *)NULL);

}

SVCXPRT *
svcudp_create(int sock)
{

	return(svcudp_bufcreate(sock, UDPMSGSIZE, UDPMSGSIZE));
}

SVCXPRT *
svcudp_xprtcopy(SVCXPRT *parent)
{
	SVCXPRT *xprt;
	struct svcudp_data *su;

	if ((xprt = svc_xprt_alloc()) == NULL) {
		return (NULL);
	}
	SVCEXT(xprt)->parent = parent;
	SVCEXT(xprt)->flags = SVCEXT(parent)->flags;

	xprt->xp_sock = parent->xp_sock;
	xprt->xp_port = parent->xp_port;
	xprt->xp_ops = &svcudp_op;

	su = (struct svcudp_data *)mem_alloc(sizeof(*su));
	if (su == NULL) {
		svc_xprt_free(xprt);
		return (NULL);
	}
	su->su_iosz = su_data(parent)->su_iosz;
	if ((rpc_buffer(xprt) = mem_alloc(su->su_iosz)) == NULL) {
		svc_xprt_free(xprt);
		mem_free((char *) su, sizeof(*su));
		return (NULL);
	}
	xdrmem_create(
		&(su->su_xdrs), rpc_buffer(xprt), su->su_iosz, XDR_DECODE);
	su->su_cache = NULL;
	xprt->xp_p2 = (caddr_t)su;
	xprt->xp_verf.oa_base = su->su_verfbody;
	return (xprt);
}

/* ARGSUSED */ 
static enum xprt_stat
svcudp_stat(SVCXPRT *xprt)
{
	return (XPRT_IDLE); 
}

static bool_t
svcudp_recv(register SVCXPRT *xprt, struct rpc_msg *msg)
{
	register struct svcudp_data *su = su_data(xprt);
	register XDR *xdrs = &(su->su_xdrs);
	register int rlen;
	char *reply;
	u_long replylen;
	mac_t packetlabel = NULL;

again:
	xprt->xp_addrlen = sizeof(struct sockaddr_in);
	if (__svc_label_agile)
		rlen = tsix_recvfrom_mac(xprt->xp_sock, rpc_buffer(xprt),
					 (int) su->su_iosz, 0,
					 (struct sockaddr *)&(xprt->xp_raddr),
					 &(xprt->xp_addrlen), &packetlabel);
	else {
		rlen = recvfrom(xprt->xp_sock, rpc_buffer(xprt),
					 (int) su->su_iosz, 0,
					 (struct sockaddr *)&(xprt->xp_raddr),
					 &(xprt->xp_addrlen));
	}
	if (rlen == -1 && errno == EINTR) {
		mac_free (packetlabel);
		packetlabel = NULL;
		goto again;
	}
	if (rlen < 4*sizeof(u_long)) {
		mac_free (packetlabel);
		return (FALSE);
	}
	if (__svc_label_agile &&
		tsix_set_mac (xprt->xp_sock, packetlabel) == -1) {
		_rpc_errorhandler(LOG_ERR, "svcudp_recv tsix_set_mac: %s",
				  strerror(errno));
		mac_free (packetlabel);
		return (FALSE);
	}
	mac_free (packetlabel);
	xdrs->x_op = XDR_DECODE;
	XDR_SETPOS(xdrs, 0);
	if (! xdr_callmsg(xdrs, msg)) {
		return (FALSE);
	}
	su->su_xid = msg->rm_xid;
	if (su->su_cache != NULL) {
		if (cache_get(xprt, msg, &reply, &replylen)) {
			(void) sendto(xprt->xp_sock, reply, (int) replylen, 0,
					  (struct sockaddr *) &xprt->xp_raddr,
					  xprt->xp_addrlen);
			return (FALSE);
		}
	}
	return (TRUE);
}

static bool_t
svcudp_reply(register SVCXPRT *xprt, struct rpc_msg *msg)
{
	register struct svcudp_data *su = su_data(xprt);
	register XDR *xdrs = &(su->su_xdrs);
	register int slen;
	register bool_t stat = FALSE;

	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, 0);
	msg->rm_xid = su->su_xid;
	if (xdr_replymsg(xdrs, msg)) {
		slen = (int)XDR_GETPOS(xdrs);
		if (sendto(xprt->xp_sock, rpc_buffer(xprt), slen, 0,
			(struct sockaddr *)&(xprt->xp_raddr), xprt->xp_addrlen)
			== slen) {
			stat = TRUE;
			if (su->su_cache && slen >= 0) {
				cache_set(xprt, (u_long) slen);
			}
		}
	}
	return (stat);
}

static bool_t
svcudp_getargs(SVCXPRT *xprt, xdrproc_t xdr_args, void * args_ptr)
{

	if (svc_mt_mode != RPC_SVC_MT_NONE)
		svc_args_done(xprt);
	return ((*(p_xdrproc_t)xdr_args)(&(su_data(xprt)->su_xdrs), args_ptr));
}

static bool_t
svcudp_freeargs(SVCXPRT *xprt, xdrproc_t xdr_args, void * args_ptr)
{
	register XDR *xdrs = &(su_data(xprt)->su_xdrs);

	xdrs->x_op = XDR_FREE;
	return ((*(p_xdrproc_t)xdr_args)(xdrs, args_ptr));
}

static void
svcudp_destroy(register SVCXPRT *xprt)
{
	MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_LOCK, &svc_mutex));
	_svcudp_destroy_private(xprt);
	MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_UNLOCK,&svc_mutex));
}

void
_svcudp_destroy_private(SVCXPRT *xprt)
{
	register struct svcudp_data *su = su_data(xprt);

	if (svc_mt_mode != RPC_SVC_MT_NONE) {
		if (SVCEXT(xprt)->parent)
			xprt = SVCEXT(xprt)->parent;
		svc_flags(xprt) |= SVC_DEFUNCT;
		if (SVCEXT(xprt)->refcnt > 0)
			return;
	}

	xprt_unregister(xprt);
	(void)close(xprt->xp_sock);
	XDR_DESTROY(&(su->su_xdrs));
	mem_free(rpc_buffer(xprt), su->su_iosz);
	mem_free((caddr_t)su, sizeof(struct svcudp_data));
	if (svc_mt_mode != RPC_SVC_MT_NONE)
		svc_xprt_destroy(xprt);
	else
		svc_xprt_free(xprt);
}


/***********this could be a separate file*********************/

/*
 * Fifo cache for udp server
 * Copies pointers to reply buffers into fifo cache
 * Buffers are sent again if retransmissions are detected.
 */

#define SPARSENESS 4	/* 75% sparse */

#define ALLOC(type, size)	\
	(type *) mem_alloc((unsigned) (sizeof(type) * (size)))

#define BZERO(addr, type, size)	 \
	bzero((char *) (addr), (int) sizeof(type) * (int) (size))

#define FREE(addr, type, size)  \
	mem_free((char *) (addr), (sizeof(type) * (size)))

/*
 * An entry in the cache
 */
typedef struct cache_node *cache_ptr;
struct cache_node {
	/*
	 * Index into cache is xid, proc, vers, prog and address
	 */
	u_long cache_xid;
	u_long cache_proc;
	u_long cache_vers;
	u_long cache_prog;
	struct sockaddr_in cache_addr;
	/*
	 * The cached reply and length
	 */
	char * cache_reply;
	u_long cache_replylen;
	/*
 	 * Next node on the list, if there is a collision
	 */
	cache_ptr cache_next;	
};



/*
 * The entire cache
 */
struct udp_cache {
	u_long uc_size;		/* size of cache */
	cache_ptr *uc_entries;	/* hash table of entries in cache */
	cache_ptr *uc_fifo;	/* fifo list of entries in cache */
	u_long uc_nextvictim;	/* points to next victim in fifo list */
	u_long uc_prog;		/* saved program number */
	u_long uc_vers;		/* saved version number */
	u_long uc_proc;		/* saved procedure number */
	struct sockaddr_in uc_addr; /* saved caller's address */
};


/*
 * the hashing function
 */
#define CACHE_LOC(transp, xid)	\
 (xid % (SPARSENESS*((struct udp_cache *) su_data(transp)->su_cache)->uc_size))	

extern pthread_mutex_t  dupreq_lock;

/*
 * Enable use of the cache. 
 * Note: there is no disable.
 */
int
svcudp_enablecache(SVCXPRT *transp, u_long size)
{
	struct svcudp_data *su = su_data(transp);
	struct udp_cache *uc;

	MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_LOCK, &dupreq_lock) );
	if (su->su_cache != NULL) {
		_rpc_errorhandler(LOG_ERR, "enablecache: cache already enabled");
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &dupreq_lock) );
		return(0);	
	}
	uc = ALLOC(struct udp_cache, 1);
	if (uc == NULL) {
		_rpc_errorhandler(LOG_ERR, "enablecache: could not allocate cache");
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &dupreq_lock) );
		return(0);
	}
	uc->uc_size = size;
	uc->uc_nextvictim = 0;
	uc->uc_entries = ALLOC(cache_ptr, size * SPARSENESS);
	if (uc->uc_entries == NULL) {
		_rpc_errorhandler(LOG_ERR, "enablecache: could not allocate cache data");
		FREE(uc, struct udp_cache, 1);
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &dupreq_lock) );
		return(0);
	}
	BZERO(uc->uc_entries, cache_ptr, size * SPARSENESS);
	uc->uc_fifo = ALLOC(cache_ptr, size);
	if (uc->uc_fifo == NULL) {
		_rpc_errorhandler(LOG_ERR, "enablecache: could not allocate cache fifo");
		FREE(uc->uc_entries, cache_ptr, size * SPARSENESS);
		FREE(uc, struct udp_cache, 1);
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &dupreq_lock) );
		return(0);
	}
	BZERO(uc->uc_fifo, cache_ptr, size);
	su->su_cache = (char *) uc;
	MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &dupreq_lock) );
	return(1);
}


/*
 * Set an entry in the cache
 */
static void
cache_set(SVCXPRT *xprt, u_long replylen)
{
	register cache_ptr victim;	
	register cache_ptr *vicp;
	register struct svcudp_data *su = su_data(xprt);
	struct udp_cache *uc = (struct udp_cache *) su->su_cache;
	u_long loc;
	char *newbuf;

	/*
 	 * Find space for the new entry, either by
	 * reusing an old entry, or by mallocing a new one
	 */
	MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_LOCK, &dupreq_lock) );
	victim = uc->uc_fifo[uc->uc_nextvictim];
	if (victim != NULL) {
		loc = CACHE_LOC(xprt, victim->cache_xid);
		for (vicp = &uc->uc_entries[loc]; 
		  *vicp != NULL && *vicp != victim; 
		  vicp = &(*vicp)->cache_next) 
				;
		if (*vicp == NULL) {
			_rpc_errorhandler(LOG_ERR, "cache_set: victim not found");
			MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &dupreq_lock) );
			return;
		}
		*vicp = victim->cache_next;	/* remote from cache */
		newbuf = victim->cache_reply;
	} else {
		victim = ALLOC(struct cache_node, 1);
		if (victim == NULL) {
			_rpc_errorhandler(LOG_ERR, "cache_set: victim alloc failed");
			MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &dupreq_lock) );
			return;
		}
		newbuf = mem_alloc(su->su_iosz);
		if (newbuf == NULL) {
			_rpc_errorhandler(LOG_ERR, "cache_set: could not allocate new rpc_buffer");
			FREE(victim, struct cache_node, 1);
			MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &dupreq_lock) );
			return;
		}
	}

	/*
	 * Store it away
	 */
	victim->cache_replylen = replylen;
	victim->cache_reply = rpc_buffer(xprt);
	rpc_buffer(xprt) = newbuf;
	xdrmem_create(&(su->su_xdrs), rpc_buffer(xprt), su->su_iosz, XDR_ENCODE);
	victim->cache_xid = su->su_xid;
	victim->cache_proc = uc->uc_proc;
	victim->cache_vers = uc->uc_vers;
	victim->cache_prog = uc->uc_prog;
	victim->cache_addr = uc->uc_addr;
	loc = CACHE_LOC(xprt, victim->cache_xid);
	victim->cache_next = uc->uc_entries[loc];	
	uc->uc_entries[loc] = victim;
	uc->uc_fifo[uc->uc_nextvictim++] = victim;
	uc->uc_nextvictim %= uc->uc_size;
	MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &dupreq_lock) );
}

/*
 * Try to get an entry from the cache
 * return 1 if found, 0 if not found
 */
static int
cache_get(SVCXPRT *xprt, 
	struct rpc_msg *msg, 
	char **replyp, 
	u_long *replylenp)
{
	u_long loc;
	register cache_ptr ent;
	register struct svcudp_data *su = su_data(xprt);
	register struct udp_cache *uc = (struct udp_cache *) su->su_cache;

#	define EQADDR(a1, a2)	(bcmp((char*)&a1, (char*)&a2, sizeof(a1)) == 0)

	MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_LOCK, &dupreq_lock) );
	loc = CACHE_LOC(xprt, su->su_xid);
	for (ent = uc->uc_entries[loc]; ent != NULL; ent = ent->cache_next) {
		if (ent->cache_xid == su->su_xid &&
		  ent->cache_proc == uc->uc_proc &&
		  ent->cache_vers == uc->uc_vers &&
		  ent->cache_prog == uc->uc_prog &&
		  EQADDR(ent->cache_addr, uc->uc_addr)) {
			*replyp = ent->cache_reply;
			*replylenp = ent->cache_replylen;
			MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &dupreq_lock) );
			return(1);
		}
	}
	/*
	 * Failed to find entry
	 * Remember a few things so we can do a set later
	 */
	uc->uc_proc = msg->rm_call.cb_proc;
	uc->uc_vers = msg->rm_call.cb_vers;
	uc->uc_prog = msg->rm_call.cb_prog;
	uc->uc_addr = xprt->xp_raddr;
	MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, &dupreq_lock) );
	return(0);
}
