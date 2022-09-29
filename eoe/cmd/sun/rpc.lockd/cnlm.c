/*
 * Copyright 1991 by Silicon Graphics, Inc.
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

/*
 * Routines used by NFS client LM requesting locks from the NFS server LM.
 */

#include <stdio.h>
#include <syslog.h>
#include "prot_lock.h"

extern int 	debug;

static void nlm_res_routine ( remote_result *reply ,
	remote_result *(*cont )(struct reclock *, remote_result *));

/* ---------------------------------------------------------------------- */

/*
 * Routines to process a result message from the server LM.
 */

static remote_result *
cont_lock(a, resp)
	struct reclock *a;
	remote_result *resp;
{
	if (debug) {
		printf("cont_lock ID=%d\n", a->lck.lox.LockID);
	}

	switch (resp->lstat) {
	case nlm_granted:
		a->rel = 0;
		if (add_mon(a, 1) == -1)
		    if (debug)
			printf("add_mon failed in cont_lock.\n");
		return (resp);
	case denied:
	case nolocks:
		a->rel = 1;
		a->block = FALSE;
		return (resp);
	case deadlck:
		a->rel = 1;
		a->block = TRUE;
		return (resp);
	case blocking:
		a->rel = 0;
		a->block = TRUE;
		return (resp);
	case grace:
		a->rel = 0;
		release_res(resp);
		return (NULL);
	default:
		a->rel = 1;
		release_res(resp);
		if (debug)
			printf("unknown lock return: %d\n", resp->lstat);
		return (NULL);
	}
}


static remote_result *
cont_unlock(a, resp)
	struct reclock *a;
	remote_result *resp;
{
	if (debug)
		printf("enter cont_unlock\n");

	a->rel = 1;
	switch (resp->lstat) {
		case nlm_granted:
			return (resp);
		case denied:		/* impossible */
		case nolocks:
			return (resp);
		case blocking:		/* impossible */
			return (resp);
		case grace:
			a->rel = 0;
			release_res(resp);
			return (NULL);
		default:
			a->rel = 0;
			release_res(resp);
			syslog(LOG_ERR,
				"unknown rpc_unlock return: %d\n",
				resp->lstat);
			return (NULL);
		}
}

static remote_result *
cont_test(a, resp)
	struct reclock *a;
	remote_result *resp;
{
	if (debug)
		printf("enter cont_test\n");

	a->rel = 1;
	switch (resp->lstat) {
	case grace:
		a->rel = 0;
		release_res(resp);
		return (NULL);
	case denied:
		if (debug)
			printf("lock blocked by %d, (%u, %u)\n",
				resp->lholder.svid, resp->lholder.l_offset,
				resp->lholder.l_len);
		return (resp);
	case nlm_granted:
	case nolocks:
	case blocking:
		return (resp);
	default:
		syslog(LOG_ERR, "cont_test: unknown return: %d\n",
			resp->lstat);
		release_res(resp);
		return (NULL);
	}
}

static remote_result *
cont_cancel(a, resp)
	struct reclock *a;
	remote_result *resp;
{
	if (debug)
		printf("enter cont_cancel\n");

	return(cont_unlock(a, resp));
}

static remote_result *
cont_reclaim(a, resp)
	struct reclock *a;
	remote_result *resp;
{
	if (debug)
		printf("enter cont_reclaim\n");

	switch (resp->lstat) {
	case nlm_granted:
		break;
	case denied:
	case nolocks:
	case blocking:
		if (a->reclaim) {
			kill_process(a);
		}
		release_res(resp);
		resp = NULL;
		break;
	case grace:
		if (a->reclaim)
			syslog(LOG_ERR,
	"reclaim lock req(%x) is returned due to grace period, impossible", a);
		release_res(resp);
		resp = NULL;
		break;
	default:
		syslog(LOG_ERR, "unknown cont_reclaim return: %d", resp->lstat);
		release_res(resp);
		resp = NULL;
		break;
	}

	return (resp);
}

/*ARGSUSED*/
static remote_result *
cont_grant(a, resp)	/* executes on server LM after sending a grant msg */
        struct reclock *a;
        remote_result *resp;
{
        if (debug)
                printf("enter cont_grant...\n");
 
	a->rel = 1;
        return (resp);
}

proc_nlm_test_res(reply)
	remote_result *reply;
{
	nlm_res_routine(reply, cont_test);
}

proc_nlm_lock_res(reply)
	remote_result *reply;
{
	nlm_res_routine(reply, cont_lock);
}

proc_nlm_cancel_res(reply)
	remote_result *reply;
{
	nlm_res_routine(reply, cont_cancel);
}

proc_nlm_unlock_res(reply)
	remote_result *reply;
{
	nlm_res_routine(reply, cont_unlock);
}

proc_nlm_granted_res(reply)
        remote_result *reply;
{
	nlm_res_routine(reply, cont_grant);
}

/*
 * common routine shared by all nlm routines that expects replies from svr nlm:
 * nlm_lock_res, nlm_test_res, nlm_unlock_res, nlm_cancel_res
 * private routine "cont" is called to continue local operation;
 * reply is match with msg in msg_queue according to cookie
 * and then attached to msg_queue;
 */

static void
nlm_res_routine(reply, cont)
	remote_result *reply;
	remote_result *(*cont)(struct reclock *, remote_result *);
{
	msg_entry *msgp;
	remote_result *resp;

	if (debug) {
		printf("enter nlm_res_routine...\n");
		pr_all();
	}
	if ((msgp = search_msg(reply)) == NULL) {
		release_res(reply);	/* not found, discard this reply */
	} else {
		if (msgp->reply != NULL) { /* reply already exists */
			if (msgp->reply->lstat != reply->lstat) {
				if (debug)
					printf("inconsistent reply (%d, %d) exists for lock(%x)\n", msgp->reply->lstat, reply->lstat, msgp->req);
			}
			release_res(reply);
			return;
		}
		if (debug) {
			printf("=req %x, lstat %d\n", msgp->req, reply->lstat);
			if (debug > 1)
				pr_lock(msgp->req);
		}

		/* continue process req according to remote reply */
		if (msgp->proc == NLM_LOCK_RECLAIM)
			resp = cont_reclaim(msgp->req, reply);
		else
			resp = cont(msgp->req, reply);

		if (resp == NULL)
			return;

		/* if we got the result for a locking request, we need to */
		/* add it to our status monitor list			  */	
		if ((msgp->proc == NLM_LOCK) || (msgp->proc == NLM_LOCK_MSG))
			(void) add_req_to_me(msgp->req, resp->lstat);

		add_reply(msgp, resp);

		/* if the result is for CANCEL msgs, remove the queued 	*/
		/* request, we do not need to reply to the kernel 	*/
		/* 'cause we have replied to it when requested for the 	*/
		/* 1st time that its granted. the msg was queued to 	*/
		/* ensure that the msg sent to the client is ack'ed	*/
		if ((msgp->proc == NLM_CANCEL) || 
		    (msgp->proc == NLM_CANCEL_MSG) ||
		    (msgp->proc == NLM_GRANTED_MSG) ||
		    (msgp->proc == NLM_LOCK_RECLAIM))	
			dequeue(msgp);
	}
}

/* ---------------------------------------------------------------------- */

/*
 * msg passing calls to nlm msg procedure:
 * - called by client to handle a kernel request (see klm.c)
 * - called by server to send a grant msg to a client (see snlm.c)
 *
 * proc specifies the type of nlm procedure,
 * retransmit indicate whether this is retransmission.
 *
 * returns -1 if rpc call is not successful, returns 0 otherwise.
 */

nlm_call(proc, a, retransmit)
	int proc;
	struct reclock *a;
	int retransmit;
{
	int 		rpc_err;
	bool_t 		(*xdr_arg)();
	char 		*name, *args;
	int 		func;
	int 		valid;
	int		status;
	nlm_testargs 	*nlm_testargs_a;
	nlm_lockargs 	*nlm_lockargs_a;
	nlm_cancargs 	*nlm_cancargs_a;
	nlm_unlockargs 	*nlm_unlockargs_a;

	if (retransmit == 0)
		valid = 1;	/* use cache value for first time calls */
	else
		valid = 0;	/* invalidate cache */

	func = proc;		/* this is necc for NLM_LOCK_RECLAIM */
	switch (proc) {
	case NLM_TEST_MSG:
		xdr_arg = xdr_nlm_testargs;
		name = a->lck.server_name;
		if ((nlm_testargs_a = get_nlm_testargs()) == NULL) {
			syslog(LOG_ERR, "unable to allocate nlm_testargs.");
			return (-1);
		}
		if (reclocktonlm_testargs(a, nlm_testargs_a) == -1) {
			release_nlm_testargs(nlm_testargs_a);
			return(-1);
		}
		args = (char *)nlm_testargs_a;
		break;

	case NLM_LOCK_RECLAIM:
		func = NLM_LOCK_MSG;
		valid = 0;			/* turn off udp cache */
		/* Fall through ... */

	case NLM_LOCK_MSG:
		xdr_arg = xdr_nlm_lockargs;
		name = a->lck.server_name;
		if ((nlm_lockargs_a = get_nlm_lockargs()) == NULL) {
			syslog(LOG_ERR, "unable to allocate nlm_lockargs.");
			return (-1);
		}
		if (reclocktonlm_lockargs(a, nlm_lockargs_a) == -1) {
			release_nlm_lockargs(nlm_lockargs_a);
			return(-1);
		}
		if (proc == NLM_LOCK_RECLAIM)
			nlm_lockargs_a->reclaim = TRUE;
		args = (char *)nlm_lockargs_a;
		break;

	case NLM_CANCEL_MSG:
		xdr_arg = xdr_nlm_cancargs;
		name = a->lck.server_name;
		if ((nlm_cancargs_a = get_nlm_cancargs()) == NULL) {
			syslog(LOG_ERR, "unable to allocate nlm_cancargs.");
			return (-1);
		}
		if (reclocktonlm_cancargs(a, nlm_cancargs_a) == -1) {
			release_nlm_cancargs(nlm_cancargs_a);
			return(-1);
		}
		args = (char *)nlm_cancargs_a;
		break;

	case NLM_UNLOCK_MSG:
		xdr_arg = xdr_nlm_unlockargs;
		name = a->lck.server_name;
		if ((nlm_unlockargs_a = get_nlm_unlockargs()) == NULL) {
			syslog(LOG_ERR, "unable to allocate nlm_unlockargs.");
			return (-1);
		}
		if (reclocktonlm_unlockargs(a, nlm_unlockargs_a) == -1) {
			release_nlm_unlockargs(nlm_unlockargs_a);
			return(-1);
		}
		args = (char *)nlm_unlockargs_a;
		break;
	
	case NLM_GRANTED_MSG:
                xdr_arg = xdr_nlm_testargs;
		name = a->lck.server_name;

                if ((nlm_testargs_a = get_nlm_testargs()) == NULL) {
                        syslog(LOG_ERR, "unable to allocate nlm_testargs.");
			return (-1);
                }
                if (reclocktonlm_testargs(a, nlm_testargs_a) == -1) {
			release_nlm_testargs(nlm_testargs_a);
			return(-1);
		}
                args = (char *)nlm_testargs_a;
                break;

	default:
		syslog(LOG_ERR, "%d not supported in nlm_call", proc);
		return (-1);
	}

	if (debug)
		printf(
	"nlm_call: proc %d (%s, %d=%d) op %d (%ld,%ld) retran %d val %d\n",
			proc, name, a->lck.lox.sysid, a->lck.lox.pid,
			a->lck.lox.type, a->lck.lox.base,
			a->lck.lox.length, retransmit, valid);

	/*
	 * call is a one-way rpc call to simulate msg passing.
	 * no timeout nor reply is specified.
	 */

	status = 0;
	if ((rpc_err = a->rpctransp(name, NLM_PROG, NLM_VERS, func, xdr_arg,
		args, xdr_void, NULL, valid, 0)) == (int)RPC_TIMEDOUT ) {
		/*
		 * if first-time rpc call is successful, add msg to msg_queue
		 */
		if (retransmit == 0 && queue(a, proc) == NULL) {
			status = -1;
		}
	} else {
		if (debug) {
			syslog(LOG_ERR, "nlm_call to %s: %s",
				name, clnt_sperrno(rpc_err));
		}	
		status = -1;
	}
	release_nlm_req(proc, args);
	return (status);
}
