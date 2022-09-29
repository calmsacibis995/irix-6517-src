/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.44 88/02/08 
 */


/*
 * svc.c, Server-side remote procedure call interface.
 *
 * There are two sets of procedures here.  The xprt routines are
 * for handling transport handles.  The svc routines handle the
 * list of service routines.
 */

#ifdef __STDC__
	#pragma weak clear_pollfd = _clear_pollfd
	#pragma weak set_pollfd = _set_pollfd
	#pragma weak svcerr_auth = _svcerr_auth
	#pragma weak svcerr_decode = _svcerr_decode
	#pragma weak svcerr_noproc = _svcerr_noproc
	#pragma weak svcerr_noprog = _svcerr_noprog
	#pragma weak svcerr_progvers = _svcerr_progvers
	#pragma weak svcerr_systemerr = _svcerr_systemerr
	#pragma weak svcerr_weakauth = _svcerr_weakauth
	#pragma weak svc_copy = _svc_copy
	#pragma weak svc_getreq = _svc_getreq
	#pragma weak svc_getreqset = _svc_getreqset
	#pragma weak svc_register = _svc_register
	#pragma weak svc_sendreply = _svc_sendreply
	#pragma weak svc_unregister = _svc_unregister
	#pragma weak svc_versquiet = _svc_versquiet
	#pragma weak svc_xprt_alloc = _svc_xprt_alloc
	#pragma weak svc_xprt_destroy = _svc_xprt_destroy
	#pragma weak svc_xprt_free = _svc_xprt_free
	#pragma weak xports = _xports
	#pragma weak xprt_register = _xprt_register
	#pragma weak xprt_unregister = _xprt_unregister
#endif
#include "synonyms.h"
#include <sys/errno.h>
#include <rpc/rpc.h>
#include <rpc/pmap_clnt.h>
/* CIPSO and MAC: */
#include <rpc/errorhandler.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <bstring.h>		  /* prototype for bcopy() */
#include <string.h>
#include <pthread.h>
#include <mplib.h>

SVCXPRT **xports = NULL;

#define NULL_SVC ((struct svc_callout *)0)
#define	RQCRED_SIZE	400		/* this size is excessive */

#define	version_keepquiet(xprt)	(svc_flags(xprt) & SVC_VERSQUIET)

/*
 * The services list
 * Each entry represents a set of procedures (an rpc program).
 * The dispatch routine takes request structs and runs the
 * apropriate procedure.
 */
static struct svc_callout {
	struct svc_callout *sc_next;
	u_long			sc_prog;
	u_long			sc_vers;
	void			(*sc_dispatch)(struct svc_req *, SVCXPRT *);
} *svc_head _INITBSS;
extern pthread_rwlock_t svc_lock; 

static struct svc_callout *svc_find(u_long, u_long, struct svc_callout **);
void _svc_prog_dispatch(SVCXPRT *, struct rpc_msg *, struct svc_req *);

/* ***************  SVCXPRT related stuff **************** */

/*
 * Add fd to svc_pollfd
 */
static void
add_pollfd(int fd)
{
	if (fd < FD_SETSIZE) {
		FD_SET(fd, &svc_fdset);
		svc_nfds++;
		svc_nfds_set++;
		if (fd >= svc_max_fd)
			svc_max_fd = fd + 1;
	}
}

/*
 * the fd is still active but only the bit in fdset is cleared.
 * do not subtract svc_nfds or svc_npollfds
 */
void
clear_pollfd(int fd)
{
	if (fd < FD_SETSIZE && FD_ISSET(fd, &svc_fdset)) {
		FD_CLR(fd, &svc_fdset);
		svc_nfds_set--;
	}
}

/*
 * sets the bit in fdset for an active fd so that poll() is done for that
 */
void
set_pollfd(int fd)
{
	if (fd < FD_SETSIZE) {
		FD_SET(fd, &svc_fdset);
		svc_nfds_set++;
	}
}

/*
 * remove a svc_pollfd entry; it does not shrink the memory
 */
static void
remove_pollfd(int fd)
{
	clear_pollfd(fd);
	if (fd == (svc_max_fd - 1))
		svc_max_fd--;
	svc_nfds--;
}

/*
 * Activate a transport handle.
 */
void
xprt_register(SVCXPRT *xprt)
{
	register int sock = xprt->xp_sock;
	extern pthread_rwlock_t svc_fd_lock;
/* VARIABLES PROTECTED BY svc_fd_lock: xports, svc_fdset */

	MTLIB_INSERT((MTCTL_PTHREAD_RWLOCK_WRLOCK, &svc_fd_lock));
	if (xports == NULL) {
		xports = (SVCXPRT **)
			mem_alloc(FD_SETSIZE * sizeof(SVCXPRT *));
	}
	if (sock < FD_SETSIZE) {
		xports[sock] = xprt;
		add_pollfd(sock);
		if (svc_polling) {
			char dummy;

			/*
			 * This happens only in one of the MT modes.
			 * Wake up selector.
			 */
			write(svc_pipe[1], &dummy, sizeof (dummy));
		}
	}
	MTLIB_INSERT((MTCTL_PTHREAD_RWLOCK_UNLOCK, &svc_fd_lock));
}

/*
 * De-activate a transport handle. 
 */
void
xprt_unregister(SVCXPRT *xprt)
{ 
	register int sock = xprt->xp_sock;
	extern pthread_rwlock_t svc_fd_lock;
	MTLIB_INSERT((MTCTL_PTHREAD_RWLOCK_WRLOCK, &svc_fd_lock));
	if ((sock < FD_SETSIZE) && (xports[sock] == xprt)) {
		xports[sock] = (SVCXPRT *)0;
		remove_pollfd(sock);
	}
	MTLIB_INSERT((MTCTL_PTHREAD_RWLOCK_UNLOCK, &svc_fd_lock));
} 


/* ********************** CALLOUT list related stuff ************* */

/*
 * Add a service program to the callout list.
 * The dispatch routine will be called when a rpc request for this
 * program number comes in.
 */
bool_t
svc_register(SVCXPRT *xprt,
	u_long prog,
	u_long vers,
	void (*dispatch)(),
	int protocol)
{
	struct svc_callout *prev;
	register struct svc_callout *s;

/* VARIABLES PROTECTED BY svc_lock: s, prev, svc_head */

	MTLIB_INSERT( (MTCTL_PTHREAD_RWLOCK_WRLOCK, &svc_lock) );
	if ((s = svc_find(prog, vers, &prev)) != NULL_SVC) {
		if (s->sc_dispatch == dispatch)
			goto pmap_it;  /* he is registering another xptr */
		MTLIB_INSERT( (MTCTL_PTHREAD_RWLOCK_UNLOCK, &svc_lock) );
		return (FALSE);
	}
	s = (struct svc_callout *)mem_alloc(sizeof(struct svc_callout));
	if (s == (struct svc_callout *)0) {
		MTLIB_INSERT( (MTCTL_PTHREAD_RWLOCK_UNLOCK, &svc_lock) );
		return (FALSE);
	}
	s->sc_prog = prog;
	s->sc_vers = vers;
	s->sc_dispatch = dispatch;
	s->sc_next = svc_head;
	svc_head = s;
pmap_it:
	MTLIB_INSERT( (MTCTL_PTHREAD_RWLOCK_UNLOCK, &svc_lock) );
	/* now register the information with the local binder service */
	if (protocol) {
		return (pmap_set(prog, vers, (u_int)protocol, xprt->xp_port));
	}
	return (TRUE);
}

/*
 * Remove a service program from the callout list.
 */
void
svc_unregister(u_long prog, u_long vers)
{
	struct svc_callout *prev;
	register struct svc_callout *s;

	MTLIB_INSERT( (MTCTL_PTHREAD_RWLOCK_WRLOCK, &svc_lock) );
	if ((s = svc_find(prog, vers, &prev)) == NULL_SVC) {
		MTLIB_INSERT( (MTCTL_PTHREAD_RWLOCK_UNLOCK, &svc_lock) );
		return;
	}
	if (prev == NULL_SVC) {
		svc_head = s->sc_next;
	} else {
		prev->sc_next = s->sc_next;
	}
	s->sc_next = NULL_SVC;
	mem_free((char *) s, (u_int) sizeof(struct svc_callout));
	MTLIB_INSERT( (MTCTL_PTHREAD_RWLOCK_UNLOCK, &svc_lock) );

	/* now unregister the information with the local binder service */
	(void)pmap_unset(prog, vers);
}

/*
 * Search the callout list for a program number, return the callout
 * struct.
 */
static struct svc_callout *
svc_find(u_long prog, u_long vers, struct svc_callout **prev)
{
	register struct svc_callout *s, *p;

/* WRITE LOCK HELD ON ENTRY: svc_lock */

	p = NULL_SVC;
	for (s = svc_head; s != NULL_SVC; s = s->sc_next) {
		if ((s->sc_prog == prog) && (s->sc_vers == vers))
			goto done;
		p = s;
	}
done:
	*prev = p;
	return (s);
}

/* ******************* REPLY GENERATION ROUTINES  ************ */

/*
 * Send a reply to an rpc request
 */
bool_t
svc_sendreply(register SVCXPRT *xprt, xdrproc_t xdr_results, void *xdr_location)
{
	struct rpc_msg rply; 

	rply.rm_direction = REPLY;  
	rply.rm_reply.rp_stat = MSG_ACCEPTED; 
	rply.acpted_rply.ar_verf = xprt->xp_verf; 
	rply.acpted_rply.ar_stat = SUCCESS;
	rply.acpted_rply.ar_results.where = xdr_location;
	rply.acpted_rply.ar_results.proc = xdr_results;
	return (SVC_REPLY(xprt, &rply)); 
}

/*
 * No procedure error reply
 */
void
svcerr_noproc(register SVCXPRT *xprt)
{
	struct rpc_msg rply;

	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = PROC_UNAVAIL;
	SVC_REPLY(xprt, &rply);
}

/*
 * Can't decode args error reply
 */
void
svcerr_decode(register SVCXPRT *xprt)
{
	struct rpc_msg rply; 

	rply.rm_direction = REPLY; 
	rply.rm_reply.rp_stat = MSG_ACCEPTED; 
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = GARBAGE_ARGS;
	SVC_REPLY(xprt, &rply); 
}

/*
 * Some system error
 */
void
svcerr_systemerr(register SVCXPRT *xprt)
{
	struct rpc_msg rply; 

	rply.rm_direction = REPLY; 
	rply.rm_reply.rp_stat = MSG_ACCEPTED; 
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = SYSTEM_ERR;
	SVC_REPLY(xprt, &rply); 
}

/*
 * Tell RPC package to not complain about version errors to the client.  This
 * is useful when revving broadcast protocols that sit on a fixed address.
 * There is really one (or should be only one) example of this kind of
 * protocol: the portmapper (or rpc binder).
 */
void
svc_versquiet(register SVCXPRT *xprt)
{
	xprt->xp_p3 = (void *)(SVC_VERSQUIET|(u_long)xprt->xp_p3);
}

/*
 * Authentication error reply
 */
void
svcerr_auth(SVCXPRT *xprt, enum auth_stat why)
{
	struct rpc_msg rply;

	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_DENIED;
	rply.rjcted_rply.rj_stat = AUTH_ERROR;
	rply.rjcted_rply.rj_why = why;
	SVC_REPLY(xprt, &rply);
}

/*
 * Auth too weak error reply
 */
void
svcerr_weakauth(SVCXPRT *xprt)
{

	svcerr_auth(xprt, AUTH_TOOWEAK);
}

/*
 * Program unavailable error reply
 */
void 
svcerr_noprog(register SVCXPRT *xprt)
{
	struct rpc_msg rply;  

	rply.rm_direction = REPLY;   
	rply.rm_reply.rp_stat = MSG_ACCEPTED;  
	rply.acpted_rply.ar_verf = xprt->xp_verf;  
	rply.acpted_rply.ar_stat = PROG_UNAVAIL;
	SVC_REPLY(xprt, &rply);
}

/*
 * Program version mismatch error reply
 */
void  
svcerr_progvers(register SVCXPRT *xprt, u_long low_vers, u_long high_vers)
{
	struct rpc_msg rply;

	rply.rm_direction = REPLY;
	rply.rm_reply.rp_stat = MSG_ACCEPTED;
	rply.acpted_rply.ar_verf = xprt->xp_verf;
	rply.acpted_rply.ar_stat = PROG_MISMATCH;
	rply.acpted_rply.ar_vers.low = low_vers;
	rply.acpted_rply.ar_vers.high = high_vers;
	SVC_REPLY(xprt, &rply);
}

/* ******************* SERVER INPUT STUFF ******************* */

/*
 * Get server side input from some transport.
 *
 * Statement of authentication parameters management:
 * This function owns and manages all authentication parameters, specifically
 * the "raw" parameters (msg.rm_call.cb_cred and msg.rm_call.cb_verf) and
 * the "cooked" credentials (rqst->rq_clntcred).
 * However, this function does not know the structure of the cooked 
 * credentials, so it make the following assumptions: 
 *   a) the structure is contiguous (no pointers), and
 *   b) the cred structure size does not exceed RQCRED_SIZE bytes. 
 * In all events, all three parameters are freed upon exit from this routine.
 */

void
svc_getreq(int rdfds)
{
	fd_set readfds;

	FD_ZERO(&readfds);
	readfds.fds_bits[0] = (fd_mask)rdfds;
	svc_getreqset(&readfds);
}

void
svc_getreqset(fd_set *readfds)
{
	enum xprt_stat stat;
	struct rpc_msg msg;
	struct svc_req r;
	register SVCXPRT *xprt;
	register int sock;
	fd_set rdfds;
	union {
		char o_buf[2*MAX_AUTH_BYTES + RQCRED_SIZE];
		struct rpc_msg o_cred_align;
	} o_align;

#define cred_area o_align.o_buf

	for (sock = 0; sock < howmany(FD_SETSIZE, NFDBITS); sock++) {
		rdfds.fds_bits[sock] =
			readfds->fds_bits[sock] & svc_fdset.fds_bits[sock];
	}
	msg.rm_call.cb_cred.oa_base = cred_area;
	msg.rm_call.cb_verf.oa_base = &(cred_area[MAX_AUTH_BYTES]);
	r.rq_clntcred = &(cred_area[2*MAX_AUTH_BYTES]);

	for (sock = 0; sock < FD_SETSIZE; sock++) {
		if (!FD_ISSET(sock, &rdfds))
			continue;
		/* sock has input waiting */
		xprt = xports[sock];
		/* now receive msgs from xprtprt (support batch calls) */
		do {
			if (SVC_RECV(xprt, &msg))
				_svc_prog_dispatch(xprt, &msg, &r);

			if ((stat = SVC_STAT(xprt)) == XPRT_DIED){
				SVC_DESTROY(xprt);
				break;
			}
		} while (stat == XPRT_MOREREQS);
	}
}

void
_svc_prog_dispatch(xprt, msg, r)
    SVCXPRT *xprt;
    struct rpc_msg *msg;
    struct svc_req *r;
{
	register struct svc_callout *sc;
	enum auth_stat why;
	int prog_found;
	u_long low_vers;
	u_long high_vers;
	void (*disp_fn)();
#ifdef MTDEBUG
	pthread_t self;

	MTLIB_STATUS( (MTCTL_PTHREAD_SELF), self);
#endif
	r->rq_xprt = xprt;
	r->rq_prog = msg->rm_call.cb_prog;
	r->rq_vers = msg->rm_call.cb_vers;
	r->rq_proc = msg->rm_call.cb_proc;
	r->rq_cred = msg->rm_call.cb_cred;
	/* first authenticate the message */
	if ((why= _authenticate(r, msg)) != AUTH_OK) {
		svcerr_auth(xprt, why);
		return;
	}
	/* now match message with a registered service*/
	prog_found = FALSE;
	low_vers = (unsigned long) - 1;
	high_vers = 0;
	MTLIB_INSERT( (MTCTL_PTHREAD_RWLOCK_WRLOCK, &svc_lock) );
	for (sc = svc_head; sc != NULL_SVC; sc = sc->sc_next) {
		if (sc->sc_prog == r->rq_prog) {
			if (sc->sc_vers == r->rq_vers) {
				disp_fn = (*sc->sc_dispatch);
				MTLIB_INSERT( (MTCTL_PTHREAD_RWLOCK_UNLOCK, &svc_lock) );
#ifdef MTDEBUG
				mt_debug(1, "_svc_prog_dispatch: %d dispatch proc %d", self, r->rq_proc);
#endif
				disp_fn(r, xprt);
				return;
			}  /* found correct version */
			prog_found = TRUE;
			if (sc->sc_vers < low_vers)
				low_vers = sc->sc_vers;
			if (sc->sc_vers > high_vers)
				high_vers = sc->sc_vers;
		}   /* found correct program */
	}
	MTLIB_INSERT( (MTCTL_PTHREAD_RWLOCK_UNLOCK, &svc_lock) );

	/*
	 * if we got here, the program or version
	 * is not served ...
	 */
	if (prog_found) {
		if (!version_keepquiet(xprt)) {
			svcerr_progvers(xprt,
				low_vers, high_vers);
		}
	} else {
		svcerr_noprog(xprt);
	}
}

/* ******************* SVCXPRT allocation and deallocation ***************** */

/*
 * svc_xprt_alloc() - allocate a service transport handle
 */
SVCXPRT *
svc_xprt_alloc()
{
	SVCXPRT		*xprt = NULL;
	SVCXPRT_EXT	*xt = NULL;
	SVCXPRT_LIST	*xlist = NULL;
	struct rpc_msg	*msg = NULL;
	struct svc_req	*req = NULL;
#undef cred_area
	char		*cred_area = NULL;

	if ((xprt = (SVCXPRT *)calloc(1, sizeof(SVCXPRT))) == NULL)
		goto err_exit;

	if ((xt = (SVCXPRT_EXT *)calloc(1, sizeof(SVCXPRT_EXT))) == NULL)
		goto err_exit;
	xprt->xp_p3 = (caddr_t)xt; /* SVCEXT(xprt) = xt */

	if ((xlist = (SVCXPRT_LIST *)calloc(1, sizeof(SVCXPRT_LIST))) == NULL)
		goto err_exit;
	xt->my_xlist = xlist;
	xlist->xprt = xprt;

	if ((msg = (struct rpc_msg *)mem_alloc(sizeof(struct rpc_msg))) == NULL)
		goto err_exit;
	xt->msg = msg;

	if ((req = (struct svc_req *)mem_alloc(sizeof(struct svc_req))) == NULL)
		goto err_exit;
	xt->req = req;

	if ((cred_area = (char *)mem_alloc(2*MAX_AUTH_BYTES +
							RQCRED_SIZE)) == NULL)
		goto err_exit;
	xt->cred_area = cred_area;

	return (xprt);

err_exit:
	svc_xprt_free(xprt);
	return (NULL);
}

/*
 * svc_xprt_free() - free a service handle
 */
void
svc_xprt_free(SVCXPRT *xprt)
{

	SVCXPRT_EXT	*xt = xprt ? SVCEXT(xprt) : NULL;
	SVCXPRT_LIST	*my_xlist = xt ? xt->my_xlist: NULL;
	struct rpc_msg	*msg = xt ? xt->msg : NULL;
	struct svc_req	*req = xt ? xt->req : NULL;
#undef cred_area
	char		*cred_area = xt ? xt->cred_area : NULL;

	if (xprt)
		mem_free((char *) xprt, sizeof(*xprt));
	if (xt)
		mem_free((char *) xt, sizeof(*xt));
	if (my_xlist)
		mem_free((char *) my_xlist, sizeof(*my_xlist));
	if (msg)
		mem_free((char *) msg, sizeof(*msg));
	if (req)
		mem_free((char *) req, sizeof(*req));
	if (cred_area)
		mem_free((char *) cred_area, sizeof(*cred_area));
}

/*
 * svc_xprt_destroy() - free parent and child xprt list
 */
void
svc_xprt_destroy(xprt)
	SVCXPRT	 *xprt;
{
	SVCXPRT_LIST	*xlist, *xnext = NULL;

	if (SVCEXT(xprt)->parent)
		xprt = SVCEXT(xprt)->parent;
	for (xlist = SVCEXT(xprt)->my_xlist; xlist != NULL; xlist = xnext) {
		xnext = xlist->next;
		xprt = xlist->xprt;
		svc_xprt_free(xprt);
	}
}

/*
 * svc_copy() - make a copy of parent
 */
SVCXPRT *
svc_copy(SVCXPRT *xprt)
{
	return (svcudp_xprtcopy(xprt));
}


/*
 * _svc_destroy_private() - private SVC_DESTROY interface
 */
void
_svc_destroy_private(xprt)
	SVCXPRT *xprt;
{
	_svcudp_destroy_private(xprt);
}

