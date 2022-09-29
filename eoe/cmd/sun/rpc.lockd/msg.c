/* Copyright 1991 Silicon Graphics, Inc. All rights reserved. */

#if 0
static char sccsid[] = "@(#)prot_msg.c	1.4 91/06/25 NFSSRC4.1 Copyr 1990 Sun Micro";
#endif

	/*
	 * Copyright (c) 1988 by Sun Microsystems, Inc.
	 */

	/*
	 * prot_msg.c
	 * consists all routines handle msg passing
	 */

#include <stdio.h>
#include <bstring.h>
#include <malloc.h>
#include <signal.h>
#include "prot_lock.h"
#include "prot_time.h"

extern int debug;
extern int grace_period;
extern remote_result res_working;

msg_entry *msg_q;		/* head of msg queue */

static msg_entry *klm_msg;	/* ptr to last msg to klm in msg queue */
static int used_msg;

/*
 * XXX The difference between rpc and msg-passing procedure numbers.
 */
#define PROC_DIFF	(NLM_LOCK_MSG - NLM_LOCK)



/*
 * retransmitted search through msg_queue to determine if "a" is
 * retransmission of a previously received msg;
 * it returns the addr of the msg entry if "a" is found
 * otherwise, it returns NULL
 */
msg_entry *
retransmitted(a, proc)
	struct reclock *a;
	int proc;
{
	msg_entry *msgp;

	msgp = msg_q;
	while (msgp != NULL) {
		if (same_fh(msgp->req, a) && 
		    same_lock(msgp->req, &a->lck.lox)) {
			if ((msgp->proc == NLM_LOCK_RECLAIM &&
			    (proc == KLM_LOCK || proc == NLM_LOCK_MSG)) ||
				msgp->proc == proc + PROC_DIFF || 
				msgp->proc == proc) {
				klm_msg = NULL;
				return (msgp);
			}
		}
		msgp = msgp->nxt;
	}
	return (NULL);
}

/*
 * match response's cookie with msg req
 * either return msgp or NULL if not found
 */
msg_entry *
search_msg(resp)
	remote_result *resp;
{
	msg_entry *msgp;

	msgp = msg_q;
	while (msgp != NULL) {
		if (obj_cmp(&msgp->req->cookie, &resp->cookie))
			return (msgp);
		msgp = msgp->nxt;
	}
	return (NULL);
}


/*
 * add a reclock to the msg queue. called from nlm_call when rpc call is 
 * successful and a reply is needed.
 *
 * proc is needed for sending back reply later.
 * if case of error, NULL is returned;
 * otherwise, the msg entry is returned
 */
msg_entry *
queue(a, proc)
	struct reclock *a;
	int proc;
{
	msg_entry *msgp;

	if ((msgp = (msg_entry *) xmalloc(sizeof(msg_entry))) == NULL)
		return (NULL);
	used_msg++;
	msgp->req = a;
	msgp->proc = proc;
	msgp->t.exp = 1;

	/* insert msg into msg queue */
	if (msg_q == NULL) {
		msgp->nxt = msgp->prev = NULL;
		msg_q = msgp;
		/* turn on alarm only when there are msgs in msg queue */
		if (grace_period == 0)
			start_timer(TIMER_MSGQ);
	}
	else {
		msgp->nxt = msg_q;
		msgp->prev = NULL;
		msg_q->prev = msgp;
		msg_q = msgp;
	}

	if ( proc != NLM_LOCK_RECLAIM && proc != NLM_GRANTED_MSG)
		klm_msg = msgp;			/* record last msg to klm */
	return (msgp);
}

/*
 * dequeue remove msg from msg_queue;
 * and deallocate space obtained  from malloc
 * lockreq is release only if a->rel == 1;
 */
dequeue(msgp)
	msg_entry *msgp;
{
	if (debug)
		printf("enter dequeue (msgp %x) ...\n", msgp);

	/*
	 * First, delete msg from msg queue since dequeue(),
	 * FREELOCK() and dequeue_lock() are recursive.
	 */
	if (klm_msg == msgp)
		klm_msg = NULL;
	if (msgp->prev != NULL)
		msgp->prev->nxt = msgp->nxt;
	else
		msg_q = msgp->nxt;
	if (msgp->nxt != NULL)
		msgp->nxt->prev = msgp->prev;

	if (msgp->req != NULL) {
		if (msgp->req->rel) {
			if (msgp->req->rnext != NULL)
				rm_req(msgp->req);
			release_reclock(msgp->req);
		}
	}
	if (msgp->reply != NULL)
		release_res(msgp->reply);

	bzero(msgp, sizeof (*msgp));
	free(msgp);
	used_msg--;

	if (debug)
		printf("msg_q %x\n", msg_q);
}

/*
 * Find a reclock and dequeue it.  But do not actually free reclock here.
 */
void
dequeue_lock(a)
	struct reclock *a;
{
	msg_entry *msgp;
	msg_entry *next_msgp;

	msgp = msg_q;
	while (msgp != NULL) {
		next_msgp= msgp->nxt;
		if (a == msgp->req) {
			msgp->req = NULL;  /* don't free here; caller does it */
			dequeue(msgp);
		}
		msgp = next_msgp;
	}
}

/*
 * if resp is not NULL, add reply to msg_entyr and reply if msg is last req;
 * otherwise, reply working
 */
add_reply(msgp, resp)
	msg_entry *msgp;
	remote_result *resp;
{
	if (debug)
		printf("enter add_reply ...\n");

	msgp->t.curr = 0; /* reset timer counter to record old msg */
	msgp->reply = resp;

	if (debug) {
		if (klm_msg && klm_msg->req)
			printf("  klm_msg->req=%x\n", klm_msg->req);
		if (msgp->req)
			printf("  msgp->req=%x reply_stat=%d\n", 
				msgp->req, resp->stat.stat);
	}
	if (klm_msg == msgp) { 
		/* reply immed */
		klm_reply(msgp->proc, resp);

		/*
		 * prevent timer routine reply "working" to already
		 * replied req
		 */
		klm_msg = NULL;
		if (resp->lstat != nlm_blocked) {
			/* don't free req if it's a lock request,
			 * 'cause the lock is queued for monitor.
			 */
			if ((msgp->req != NULL) &&
			    (msgp->proc != NLM_LOCK && 
			     msgp->proc != NLM_LOCK_MSG) &&
			    (msgp->proc != NLM_LOCK_RECLAIM))
						/* set free reclock */
				msgp->req->rel = 1;
			dequeue(msgp);
		}
	}
}

/*
 * Called from alarm signal handler:
 * check retransmiting status and reply to last req
 */
void
msgqtimer(void)
{
	msg_entry *msgp, *next;

	if (debug)
		printf("\nenter msgqtimer: %x\n", msg_q);

	if (grace_period > 0) {
		/* reduce the remaining grace period */
		grace_period--;
		if (grace_period == 0) {
			if (debug) {
				printf("**********end of grace period\n");
				fflush(stdout);
			}
			/* remove proc == klm_xxx in msg queue */
			next = msg_q;
			while ((msgp = next) != NULL) {
				next = msgp->nxt;
				if (msgp->proc == KLM_LOCK ||
				    msgp->proc == KLM_UNLOCK ||
				    msgp->proc == KLM_TEST ||
				    msgp->proc == KLM_CANCEL) {
					if (debug)
						printf("remove grace period msg (%x) from msg queue\n", msgp);
					if (msgp->req != NULL)	/* set free reclock */
						msgp->req->rel = 1;
					dequeue(msgp);
				}
			}
		}
	}

	next = msg_q;
	while ((msgp = next) != NULL) {
		next = msgp->nxt;
		if (msgp->reply == NULL) { /* check for retransimssion */
			if (msgp->proc != KLM_LOCK) {
				/* KLM_LOCK is for local blocked locks */
				if (msgp->t.exp == msgp->t.curr) {
					/* retransmit */
					if (debug)
						printf("xtimer retransmit: ");
					if (!USES_TCP(msgp->req))
					    (void)nlm_call(msgp->proc,
						msgp->req, 1);
					else if (debug)
					    printf("no answer for %x\n",
							msgp->req);
					msgp->t.curr = 0;
					msgp->t.exp = 2 * msgp->t.exp;
					/* double timeout period */
					if (msgp->t.exp > MAX_LM_TIMEOUT_COUNT){
					    msgp->t.exp = MAX_LM_TIMEOUT_COUNT;
					}
				}
				else 	/* increment current count */
					msgp->t.curr++;
			}
		} else {
			/* check if reply is sitting there too long */
			if (msgp->reply->lstat != nlm_blocked) {
				if (msgp->t.curr > OLDMSG)
					dequeue(msgp);
				else
					msgp->t.curr++;
			}
		}
	}

	if (grace_period != 0 || msg_q != NULL)
		start_timer(TIMER_MSGQ);
}
