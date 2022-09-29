#if 0
static char sccsid[] = "@(#)prot_priv.c	1.5 91/06/25 NFSSRC4.1 Copyr 1990 Sun Micro";
#endif

	/*
	 * Copyright (c) 1988 by Sun Microsystems, Inc.
	 */

	/*
	 * consists of all private protocols for comm with
	 * status monitor to handle crash and recovery
	 */

#include <stdio.h>
#include <syslog.h>
#include <bstring.h>
#include <string.h>
#include <netdb.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/fcntl.h>
#include "prot_lock.h"
#include "priv_prot.h"
#include <rpcsvc/sm_inter.h>

extern int debug;
extern int pid;
extern char hostname[];
extern msg_entry *msg_q;	/* head of msg queue */

static void
unlockproc(int fd, sysid_t id)
{
	struct flock ld;

	ld.l_type = F_UNLCK;
	ld.l_whence = 0;
       	ld.l_start = 0;
       	ld.l_len = 0;
       	ld.l_pid = pid;
	ld.l_sysid = id;
       	if (debug) {
               	printf("unlockproc: sysid=%d\n", ld.l_sysid);
       	}
       	if (fcntl(fd, F_RSETLK, &ld) < 0) {
               	syslog(LOG_ERR, "unable to clear a lock: %m");
       	}
}

static void
proc_priv_crash(statp)
	struct status *statp;
{
	struct hostent *hp;

	if (debug)
		printf("enter proc_priv_CRASH: %s\n", statp->mon_name);

	if ((hp = gethostbyname(statp->mon_name)) == NULL) {
                if (debug)
                        printf(" >>> unknown host\n");
		return;
        }
	foreach_fd(((struct in_addr *)hp->h_addr)->s_addr, unlockproc);
	clear_blk_host(((struct in_addr *)hp->h_addr)->s_addr);
	delete_hash(statp->mon_name);
}

/*
 * reclaim_locks() -- will send out reclaim lock requests to the server.
 *		      listp is the list of established/granted lock requests.
 */
static void
reclaim_locks(listp)
	struct reclocklist *listp;
{
	struct reclock *ff;

	FOREACH_RECLIST(listp, ff) {
		/* set reclaim flag & send out the request */
		ff->reclaim = 1;
		if (nlm_call(NLM_LOCK_RECLAIM, ff, 0) == -1) {
			if (queue(ff, NLM_LOCK_RECLAIM) == NULL)
				syslog(LOG_ERR,
  "reclaim request (%x) cannot be sent and cannot be queued for resend later!",
					ff);
		}
	}
}

/*
 * reclaim_pending() -- will setup the existing queued msgs of the pending
 *		      	lock requests to allow retransmission.
 *			note that reclaim requests for these pending locks
 *			are not sent out.
 */
static void
reclaim_pending(listp)
	struct reclocklist *listp;
{
	msg_entry *msgp;
	struct reclock *ff;

	/* for each pending lock request for the recovered server */
	FOREACH_RECLIST(listp, ff) {
		/* find the msg in the queue that holds this lock request */
		for (msgp = msg_q; msgp != NULL; msgp = msgp->nxt) {
			
			/* if msg is found, free & nullified the exisiting */
			/* response of this lock request.  this will allow */
			/* retransmission of the requests.		   */
			if (msgp->req == ff) {
				if (msgp->reply != NULL) {
					release_res(msgp->reply);
					msgp->reply = NULL;
				}
				break;
			}
		}
	}
}


#define up(x)	(((x) % 2 == 1) || ((x) %2 == -1))

static void
proc_priv_recovery(statp)
	struct status *statp;
{
	struct lm_vnode *mp;
	struct priv_struct *privp;

	if (debug)
		printf("enter proc_priv_RECOVERY.....\n");

	privp = (struct priv_struct *) statp->priv;
	if (privp->pid != pid) {
		if (debug)
			printf("this is not for me(%d): %d\n", privp->pid, pid);
		return;
	}

	if (debug)
		printf("enter proc_lm_recovery due to %s state(%d)\n",
			statp->mon_name, statp->state);

	destroy_client_shares(statp->mon_name);

	delete_hash(statp->mon_name);
	if (!up(statp->state)) {
		if (debug)
			printf("%s is not up.\n", statp->mon_name);
		return;
	}
	if (strcasecmp(statp->mon_name, hostname) == 0) {
		if (debug)
			printf("I have been declared as failed!!!\n");
	}

	if (debug)
		print_monlocks();
	mp = find_me(statp->mon_name);
	if (mp) {
		reclaim_locks(&mp->exclusive);
		reclaim_locks(&mp->shared);
		reclaim_pending(&mp->pending);
	}
}

void
priv_prog(rqstp, transp)
	struct svc_req *rqstp;
	SVCXPRT *transp;
{
	struct status stat;

	bzero(&stat, sizeof (struct status));
	if (!svc_getargs(transp, xdr_status, &stat)) {
		svcerr_decode(transp);
		return;
	}
	if (debug)
		printf("\n+++ PRIV_PROG: %d\n", rqstp->rq_proc);

	switch (rqstp->rq_proc) {
	case PRIV_CRASH:
		proc_priv_crash(&stat);
		break;
	case PRIV_RECOVERY:
		proc_priv_recovery(&stat);
		break;
	default:
		svcerr_noproc(transp);
		return;
	}
	if (!svc_sendreply(transp, xdr_void, NULL)) {
		svcerr_systemerr(transp);
	}
	if (!svc_freeargs(transp, xdr_status, &stat)) {
		syslog(LOG_ERR, "unable to free arguments");
		exit(1);
	}
	if (debug)
		printf("--- PRIV_PROG\n");
}
