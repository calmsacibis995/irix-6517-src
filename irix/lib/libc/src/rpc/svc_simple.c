/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 * 1.19 88/02/08
 */


/* 
 * svc_simple.c
 * Simplified front end to rpc.
 */

#ifdef __STDC__
	#pragma weak registerrpc = _registerrpc
#endif
#include "synonyms.h"

#include <stdio.h>
#include <rpc/rpc.h>
#include <rpc/errorhandler.h>
#include <rpc/pmap_clnt.h>	/* prototype for pmap_unset() */
#include <sys/socket.h>
#include <netdb.h>
#include <bstring.h>		/* prototype for bzero() */

static struct proglst {
	void *(*p_progname)(void *);
	u_long  p_prognum;
	u_long  p_procnum;
	xdrproc_t p_inproc, p_outproc;
	struct proglst *p_nxt;
} *proglst;
static void universal(struct svc_req *rqstp, SVCXPRT *transp);
static SVCXPRT *transp;
static struct proglst *pl;

int
registerrpc(u_long prognum, 
	u_long versnum, 
	u_long procnum,
	void *(*progname)(void *), 
	xdrproc_t inproc, 
	xdrproc_t outproc)
{
	
	if (procnum == NULLPROC) {
		_rpc_errorhandler(LOG_ERR,
		   "registerrpc: can't reassign procedure number %u", NULLPROC);
		return (-1);
	}
	if (transp == 0) {
		transp = svcudp_create(RPC_ANYSOCK);
		if (transp == NULL) {
			_rpc_errorhandler(LOG_ERR,
				"registerrpc: couldn't create an rpc server");
			return (-1);
		}
	}
	(void) pmap_unset(prognum, versnum);
	if (!svc_register(transp, prognum, versnum, 
	    universal, IPPROTO_UDP)) {
		_rpc_errorhandler(LOG_ERR,
		    "registerrpc: couldn't register prog %u vers %u",
		    prognum, versnum);
		return (-1);
	}
	pl = (struct proglst *)malloc(sizeof(struct proglst));
	if (pl == NULL) {
		_rpc_errorhandler(LOG_ERR, "registerrpc: out of memory");
		return (-1);
	}
	pl->p_progname = progname;
	pl->p_prognum = prognum;
	pl->p_procnum = procnum;
	pl->p_inproc = inproc;
	pl->p_outproc = outproc;
	pl->p_nxt = proglst;
	proglst = pl;
	return (0);
}

static void
universal(struct svc_req *rqstp, SVCXPRT *transp)
{
	u_long prog, proc;
	void *outdata;
	char xdrbuf[UDPMSGSIZE];
	struct proglst *pl;

	/* 
	 * enforce "procnum 0 is echo" convention
	 */
	if (rqstp->rq_proc == NULLPROC) {
		if (svc_sendreply(transp, (xdrproc_t) xdr_void, (char *)NULL) == FALSE) {
			_rpc_errorhandler(LOG_ERR,
				"registerrpc dispatch: svc_sendreply failed");
		}
		return;
	}
	prog = rqstp->rq_prog;
	proc = rqstp->rq_proc;
	for (pl = proglst; pl != NULL; pl = pl->p_nxt)
		if (pl->p_prognum == prog && pl->p_procnum == proc) {
			/* decode arguments into a CLEAN buffer */
			bzero(xdrbuf, sizeof(xdrbuf)); /* required ! */
			if (!svc_getargs(transp, pl->p_inproc, xdrbuf)) {
				svcerr_decode(transp);
				return;
			}
			outdata = (*(pl->p_progname))(xdrbuf);
			if (outdata == NULL && pl->p_outproc != (xdrproc_t) xdr_void)
				/* there was an error */
				return;
			if (!svc_sendreply(transp, pl->p_outproc, outdata)) {
				_rpc_errorhandler(LOG_ERR,
			    "registerrpc dispatch: trouble replying to prog %u",
				    pl->p_prognum);
				return;
			}
			/* free the decoded arguments */
			(void)svc_freeargs(transp, pl->p_inproc, xdrbuf);
			return;
		}
	_rpc_errorhandler(LOG_ERR,
		"registerrpc dispatch: never registered prog %u", prog);
	exit(1);
}
