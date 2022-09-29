/* Copyright 1991 Silicon Graphics, Inc. All rights reserved. */

#if 0
static char sccsid[] = "@(#)prot_pklm.c	1.5 91/06/25 NFSSRC4.1 Copyr 1990 Sun Micro";
/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */
#endif


/*
 * klm.c --
 *	procedures to handle kernel lock requests
 */

#include <stdio.h>
#include <syslog.h>
#include <netdb.h>
#include <sys/fcntl.h>
#include "prot_lock.h"

extern int debug;
extern SVCXPRT *klm_transp;
extern remote_result res_working;


#define same_proc(x, y) (obj_cmp(&x->lck.oh, &y->lck.oh))

static void klm_msg_routine (reclock *a ,int proc,
		void (*remote)(struct reclock *));


static void
remote_lock(a)
	struct reclock *a;
{
	if (debug)
		printf("enter remote_lock\n");

	if (nlm_call(NLM_LOCK_MSG, a, 0) == -1)
		a->rel = 1;
}

static void
remote_unlock(a)
	struct reclock *a;
{
	if (debug)
		printf("enter remote_unlock\n");

	if (nlm_call(NLM_UNLOCK_MSG, a, 0) == -1)
		a->rel = 1;	/* rpc error, discard */
	(void) remove_req_in_me(a);
}

static void
remote_test(a)
	struct reclock *a;
{
	if (debug)
		printf("enter remote_test\n");

	if (nlm_call(NLM_TEST_MSG, a, 0) == -1)
		a->rel = 1;
}


static void
remote_cancel(a)
	struct reclock *a;
{
	msg_entry *msgp;

	if (debug)
		printf("enter remote_cancel(%x)\n", a);

	if (nlm_call(NLM_CANCEL_MSG, a, 0) == -1)
		a->rel = 1;

	if ((msgp = retransmitted(a, KLM_LOCK)) != NULL) {
		/* msg is being processed */
		if (debug)
			printf("remove msg (%x) due to remote cancel\n",
				msgp->req);

		/* don't free the reclock here as 
		 * remove_req_in_me() will do that.
		 */
		msgp->req->rel = 0;
		dequeue(msgp);
	}
	remove_req_in_me(a);
}


proc_klm_test(a)
	reclock *a;
{
	if (debug)
		printf("klm_test:\n");
	klm_msg_routine(a, KLM_TEST, remote_test);
}

proc_klm_lock(a)
	reclock *a;
{
	if (debug)
		printf("klm_lock:\n");
	klm_msg_routine(a, KLM_LOCK, remote_lock);
}

proc_klm_cancel(a)
	reclock *a;
{
	if (debug)
		printf("klm_cancel:\n");
	klm_msg_routine(a, KLM_CANCEL, remote_cancel);
}

proc_klm_unlock(a)
	reclock *a;
{
	if (debug)
		printf("klm_unlock:\n");
	klm_msg_routine(a, KLM_UNLOCK, remote_unlock);
}


/*
 * common routine to handle msg passing form of communication;
 * klm_msg_routine is shared among all klm procedures:
 * proc_klm_test, proc_klm_lock, proc_klm_cancel, proc_klm_unlock;
 * proc specifies the name of the routine to branch to for reply purpose;
 * local and remote specify the name of routine that handles the call
 *
 * when a msg arrives, it is first checked to see
 *   if retransmitted;
 *	 if a reply is ready,
 *	   a reply is sent back and msg is erased from the queue
 *	 or msg is ignored!
 *   else if this is a new msg;
 *	 if data are remote
 *		a rpc request is sent, msg added to msg_queue (see nlm_call)
 *	 else if a request lock or similar lock
 *		reply is sent back immediately.
 */

static void
klm_msg_routine(a, proc, remote)
	reclock *a;
	int proc;
	void (*remote)(struct reclock *);
{
	struct msg_entry *msgp;
	remote_result resp;

	if (debug > 1)
		pr_lock(a);

	if ((msgp = retransmitted(a, proc)) != NULL) {
		if (debug)
			printf("retransmitted msg! %x->%x\n", msgp, msgp->req);
		a->rel = 1;
		if (msgp->reply == NULL) {
			klm_reply(proc, &res_working);
		} else {
			/*
			 * reply to kernel and dequeue only if we have
			 * our blocking request result
			 */
			klm_reply(proc, msgp->reply);

			if (msgp->reply->lstat != blocking) {
				if ((msgp->proc != NLM_LOCK) &&
				    (msgp->proc != NLM_LOCK_MSG) &&
				    (msgp->proc != NLM_LOCK_RECLAIM)) {
					/* set free reclock */
					if (msgp->req != NULL)	
						msgp->req->rel = 1;
				}
				dequeue(msgp);
			}
		}
	} else {
		remote(a);

		/* if we receive KLM_CANCEL from the kernel, reply we got it */
		if (proc == KLM_CANCEL) {
			resp.lstat = nlm_granted;
			klm_reply(proc, &resp);
		}
	}
}

/*
 * klm_reply send back reply from klm to requestor(kernel):
 * proc specify the name of the procedure return the call;
 * corresponding xdr routines are then used;
 */
klm_reply(proc, reply)
	int proc;
	remote_result *reply;
{
	bool_t (*xdr_reply)();
	klm_testrply args;
	register klm_testrply *argsp= &args;
	klm_stat stat_args;
	register klm_stat *stat_argsp= &stat_args;

	switch (proc) {
	case KLM_TEST:
	case NLM_TEST_MSG:	/* record in msgp->proc */
		xdr_reply = xdr_klm_testrply;
		argsp->stat = klm_working;
		if (reply->lstat == nlm_granted) {
			argsp->stat = klm_granted;
		} else {
			if (reply->lstat == nlm_denied)
				argsp->stat = klm_denied;
			else if (reply->lstat == nlm_denied_nolocks)
				argsp->stat = klm_denied_nolocks;
			argsp->klm_testrply_u.holder.svid =
				reply->stat.nlm_testrply_u.holder.svid;
			argsp->klm_testrply_u.holder.l_offset =
				reply->stat.nlm_testrply_u.holder.l_offset;
			argsp->klm_testrply_u.holder.l_len =
				reply->stat.nlm_testrply_u.holder.l_len;
			if (reply->stat.nlm_testrply_u.holder.exclusive)
				argsp->klm_testrply_u.holder.exclusive = TRUE;
			else
				argsp->klm_testrply_u.holder.exclusive = FALSE;
		}
		if (debug) {
			printf("KLM_REPLY(test): stat=%d (%s)",
					argsp->stat,
					klm_stat2name(argsp->stat));
			if (argsp->stat == klm_denied ||
			    argsp->stat == klm_denied_nolocks)
				printf(" svid=%d excl=%d offset=%d len=%d",
					argsp->klm_testrply_u.holder.svid,
					argsp->klm_testrply_u.holder.exclusive,
					argsp->klm_testrply_u.holder.l_offset,
					argsp->klm_testrply_u.holder.l_len);
			putchar('\n');
		}
		if (!svc_sendreply(klm_transp, xdr_reply, argsp))
			svcerr_systemerr(klm_transp);
		break;
	case KLM_LOCK:
	case NLM_LOCK_MSG:
	case NLM_LOCK_RECLAIM:
	case KLM_CANCEL:
	case NLM_CANCEL_MSG:
	case KLM_UNLOCK:
	case NLM_UNLOCK_MSG:
	case NLM_GRANTED:
	case NLM_GRANTED_MSG:
		xdr_reply = xdr_klm_stat;
		if (reply->lstat == nlm_denied)
			stat_argsp->stat = klm_denied;
		else if (reply->lstat == nlm_granted)
			stat_argsp->stat = klm_granted;
		else if (reply->lstat == nlm_denied_nolocks)
			stat_argsp->stat = klm_denied_nolocks;
		else if (reply->lstat == nlm_deadlck)
			stat_argsp->stat = klm_deadlck;
		else if (reply->lstat == nlm_blocked)
			stat_argsp->stat = klm_working;
		if (debug) {
			printf("KLM_REPLY: stat=%d (%s)\n",
			stat_argsp->stat, klm_stat2name(stat_argsp->stat));
		}
		if (!svc_sendreply(klm_transp, xdr_reply, stat_argsp))
			svcerr_systemerr(klm_transp);
		break;
	default:
		xdr_reply = xdr_void;
		syslog(LOG_ERR, "unknown klm_reply proc(%d)", proc);
		if (debug)
			printf("!! unknown klm_reply proc (%d)\n", proc);
		if (!svc_sendreply(klm_transp, xdr_reply, &reply->stat))
			svcerr_systemerr(klm_transp);
	}
}

/*
* Canonicalize the server name that the kernel gave us.
*/
static int
cp_server(char *sname, struct reclock *to)
{
	struct hostent *hp;
	static char sn[256], hn[256];

	if (strcasecmp(sname, sn) == 0) {
		sname = hn;	/* use cached answer */
	} else if ((hp = gethostbyname(sname)) != NULL) {
		strncpy(sn, sname, sizeof(sn)-1);
		strncpy(hn, hp->h_name, sizeof(hn)-1);
		sname = hp->h_name;
	}
	return (to->lck.server_name = copy_str(sname)) != NULL;
}

int
klm_lockargstoreclock(from, to)
	struct klm_lockargs *from;
	struct reclock *to;
{
	if (!cp_server(from->alock.server_name, to))
		return(-1);
	if (from->alock.fh.n_len) {
		if (obj_copy(&to->lck.fh, &from->alock.fh) == -1)
			return(-1);
	} else
		to->lck.fh.n_len = 0;
	to->lck.lox.base = from->alock.l_offset;
	to->lck.lox.length = from->alock.l_len;
	if (from->exclusive) {
		to->lck.lox.type = F_WRLCK;
		to->exclusive = TRUE;
	} else {
		to->lck.lox.type = F_RDLCK;
		to->exclusive = FALSE;
	}
	if (from->block) {
		to->block = TRUE;
	} else {
		to->block = FALSE;
	}
	to->lck.lox.LockID = 0;
	to->lck.lox.pid = from->alock.pid;

	return(0);
}

int
klm_unlockargstoreclock(from, to)
	struct klm_unlockargs *from;
	struct reclock *to;
{
	if (!cp_server(from->alock.server_name, to))
		return(-1);
	if (from->alock.fh.n_len) {
		if (obj_copy(&to->lck.fh, &from->alock.fh) == -1)
			return(-1);
	} else
		to->lck.fh.n_len = 0;
	to->lck.lox.base = from->alock.l_offset;
	to->lck.lox.length = from->alock.l_len;
	to->lck.lox.LockID = 0;
	to->lck.lox.pid = from->alock.pid;

	return(0);
}

int
klm_testargstoreclock(from, to)
	struct klm_testargs *from;
	struct reclock *to;
{
	if (!cp_server(from->alock.server_name, to))
		return(-1);
	if (from->alock.fh.n_len) {
		if (obj_copy(&to->lck.fh, &from->alock.fh) == -1)
			return(-1);
	} else
		to->lck.fh.n_len = 0;
	to->lck.lox.base = from->alock.l_offset;
	to->lck.lox.length = from->alock.l_len;
	if (from->exclusive) {
		to->lck.lox.type = F_WRLCK;
		to->exclusive = TRUE;
	} else {
		to->lck.lox.type = F_RDLCK;
		to->exclusive = FALSE;
	}
	to->block = TRUE;
	to->lck.lox.LockID = 0;
	to->lck.lox.pid = from->alock.pid;

	return(0);
}
