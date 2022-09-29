/* Copyright 1991 Silicon Graphics, Inc. All rights reserved. */

#if 0
static char sccsid[] = "@(#)prot_pnlm.c	1.7 91/06/25 NFSSRC4.1 Copyr 1990 Sun Micro";
/* Copyright (c) 1988 by Sun Microsystems, Inc.  */
#endif

/*
 * snlm.c
 *
 * 	consists of all server procedures called by nlm_prog
 */

#include <sys/types.h>
#include <stdio.h>
#include <syslog.h>
#include <bstring.h>
#include <errno.h>
#include <assert.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/prctl.h>
#include "prot_lock.h"

extern SVCXPRT		*nlm_transp;
extern int		debug;
extern int		pid;

nlm_stats local_lock(struct reclock *a);
nlm_stats local_unlock(struct reclock *a);
static nlm_stats local_test(struct reclock *a);
static nlm_stats local_grant(struct reclock *a);

static void show_lock_owner(int fd);


/* ---------------------------------------------------------------------- */

/*
 * Blocking lock list containing all lock requests that are blocked.
 */
static struct reclock	*blocking_req;


/*
 * find_block_req() - given a reclock, chk if this lock request exists in
 *		      the blocking lock lists, if it exists, return the ptr
 *		      of the found reclock in the list
 */
static struct reclock *
find_block_req(a)
	struct reclock	*a;
{
	struct reclock *cur;

	for (cur = blocking_req; cur != NULL; cur = cur->rnext) {
		/*
		 * When local_unlock is called, the fcntl returns the
		 * lock region for the current requestor if lock is granted.
		 * Check if the lock request is WITHIN the region will ensure
		 * all the blocking lock for the requestor is granted.
		 */
		if (same_type(&(cur->lck.lox), &(a->lck.lox)) &&
		    WITHIN(&(cur->lck.lox), &(a->lck.lox)) &&
		    SAMEOWNER(&(cur->lck.lox), &(a->lck.lox)) &&
		    (cur->lck.lox.ipaddr == a->lck.lox.ipaddr))
			break;
	}
	return (cur);
}

/*
 * Find the blocked request by the pid of its sproc thread.
 * Called when a blocked thread exits.
 */

static struct reclock *
find_blkpid(pid_t pid)
{
	struct reclock *cur;

	for (cur = blocking_req; cur != NULL; cur = cur->rnext) {
		if (cur->blk.pid == pid)
			break;
	}
	return (cur);
}

/* 
 * For debugging: used to make sure that a supposedly blocked process
 * isn't making additional requests.
 */
static struct reclock *
find_lckpid(struct reclock *a)
{
	struct reclock *cur;

	for (cur = blocking_req; cur != NULL; cur = cur->rnext) {
		if ((cur->lck.lox.pid == a->lck.lox.pid) &&
		    (cur->lck.lox.ipaddr == a->lck.lox.ipaddr))
			break;
	}
	return (cur);
}
/*
 * dequeue_block_req() -- remove the reclock lock request from the blocking
 *			  lock list
 */
static void
dequeue_block_req(a)
	struct reclock	*a;
{
	struct reclock *cur = blocking_req;
	struct reclock *prev = NULL;

	while (cur != NULL) {
		if (a == cur) {
			if (prev == NULL)
				blocking_req = cur->rnext;
			else
				prev->rnext = cur->rnext;
			cur->rnext = NULL;
			/*
			 * Don't release "cur" so nlm_call can put it on the
			 * msg queue to wait for a reply.
			 */
			return;
		}
		prev = cur;
		cur = cur->rnext;
	}
}

/*
 * queue_block_req() -- queue the lock request in the blocking lock list
 */
static int
queue_block_req(a)
	struct reclock	*a;
{
	char	*tmp_ptr;

	/* if its a retransmitted blocking request, do not queue it */
	if (find_block_req(a) != NULL)
		return (FALSE);

	/* since we r re-using the reclock, we need to swap the */
	/* clnt & svr name & caller_name				*/
	tmp_ptr = a->lck.server_name;
	a->lck.server_name = a->lck.clnt_name;
	a->lck.clnt_name = tmp_ptr;
	a->lck.caller_name = tmp_ptr;

	/* add to front of blocking_req list */
	a->rnext = blocking_req;
	blocking_req = a;
	return (TRUE);
}

/*
 * Called by the crash routine to clear out blocking locks held
 * by a specific host.
 */
void
clear_blk_host(u_long ipaddr)
{
	struct reclock *cur, *next;

	for (cur = blocking_req; cur != NULL; cur = next) {
		next = cur->rnext;
		if (cur->alock.lox.ipaddr == ipaddr) {
			if (debug)
				printf("clearing: %d, fd %d\n",
					cur->blk.pid, cur->blk.fd);
			assert (cur->blk.pid > 0);
			kill(cur->blk.pid, SIGTERM);
		}
	}
}

/* ---------------------------------------------------------------------- */

/*
 * NFS server LM procedures
 */

static void
nlmtest(struct reclock *a, int proc)
{
	if (debug > 1)
		printf("nlmtest(%x) \n", a);
	nlm_reply(proc, local_test(a), a);
	a->rel= 1;
}

proc_nlm_test(a)
	struct reclock *a;
{
	nlmtest(a, NLM_TEST);
}

proc_nlm_test_msg(a)
	struct reclock *a;
{
	nlmtest(a, NLM_TEST_MSG);
}

/* ---------------------------------------------------------------------- */

static int
req_is_queued(struct reclock *a)
{
	struct reclock *cur;
	for (cur = blocking_req; cur != NULL; cur = cur->rnext) {
		if (same_type(&cur->lck.lox, &a->lck.lox) &&
		    same_bound(&cur->lck.lox, &a->lck.lox) &&
		    SAMEOWNER(&cur->lck.lox, &a->lck.lox) &&
		    (cur->lck.lox.ipaddr == a->lck.lox.ipaddr))
			break;
	}
	return (cur != NULL);
}

static void
nlmlock(struct reclock *a, int proc)
{
	nlm_stats stat;

	if (debug > 1)
		printf("enter nlmlock(%x) \n", a);

	if (a->block && req_is_queued(a)) {
		/*
		 * Since this is identical to a previously queued
		 * blocking request we should actually send a reply
		 * saying that we are currently blocked (nlm_blocked).
		 * Punt for now ...
		 */
		if (debug)
			printf("Dropping duplicate request\n");
		a->rel = 1;
		return;
	}
	stat = local_lock(a);
	nlm_reply(proc, stat, a);

	/* if the lock is blocked, add it into the blocking lock list */
	/* and DO NOT free the reclock 'cause its being chained	      */

	if (stat != nlm_blocked || !queue_block_req(a))
		a->rel = 1;
}

proc_nlm_lock(a)
	struct reclock *a;
{
	nlmlock(a, NLM_LOCK);
}

proc_nlm_lock_msg(a)
	struct reclock *a;
{
	nlmlock(a, NLM_LOCK_MSG);
}

/* ---------------------------------------------------------------------- */

static void
nlmcancel(struct reclock *a, int proc)
{
	struct reclock *req;

	if (debug > 1)
		printf("enter nlmcancel(%x) \n", a);

	nlm_reply(proc, local_unlock(a), a);

	/* rm the prev blocking request if its queued */
	if ((req = find_block_req(a)) != NULL) {
		assert (req->blk.pid > 0);
		kill(req->blk.pid, SIGTERM);
	}
	a->rel= 1;
}

proc_nlm_cancel(a)
	struct reclock *a;
{
	nlmcancel(a, NLM_CANCEL);
}

proc_nlm_cancel_msg(a)
	struct reclock *a;
{
	nlmcancel(a, NLM_CANCEL_MSG);
}

/* ---------------------------------------------------------------------- */

static void
nlmunlock(struct reclock *a, int proc)
{
	if (debug > 1)
		printf("enter nlmunlock(%x) \n", a);
	nlm_reply(proc, local_unlock(a), a);
	a->rel= 1;
}

proc_nlm_unlock(a)
	struct reclock *a;
{
	nlmunlock(a, NLM_UNLOCK);
}

proc_nlm_unlock_msg(a)
	struct reclock *a;
{
	nlmunlock(a, NLM_UNLOCK_MSG);
}

/* ---------------------------------------------------------------------- */

/*
 * This operation occurs on a client.
 */

static void
nlmgranted(struct reclock *a, int proc)
{
	if (debug > 1)
                printf("enter nlmgranted(%x) \n", a);

        nlm_reply(proc, local_grant(a), a);
	a->rel= 1;
}

proc_nlm_granted(a)
        struct reclock *a;
{
	nlmgranted(a, NLM_GRANTED);
}

proc_nlm_granted_msg(a)
	struct reclock *a;
{
	nlmgranted(a, NLM_GRANTED_MSG);
}

/* ---------------------------------------------------------------------- */

/*
 * return rpc calls;
 * if rpc calls, directly reply to the request;
 * if msg passing calls, initiates one way rpc call to reply!
 */
nlm_reply(proc, stat, a)
	int proc;
	nlm_stats stat;
	struct reclock *a;
{
	remote_result reply;
	bool_t (*xdr_reply)();
	enum { NREPLY, NEWCALL } act;
	int rpc_err;
	char *name;
	int usecache;

	reply.lstat = stat;

	switch (proc) {
	case NLM_TEST:
		xdr_reply = xdr_nlm_testres;
		act = NREPLY;
		break;
	case NLM_GRANTED:
	case NLM_LOCK:
	case NLM_CANCEL:
	case NLM_UNLOCK:
		xdr_reply = xdr_nlm_res;
		act = NREPLY;
		break;
	case NLM_TEST_MSG:
		xdr_reply = xdr_nlm_testres;
		act = NEWCALL;
		proc = NLM_TEST_RES;
		name = a->lck.clnt_name;
		if (a->lck.lox.type == F_UNLCK) {
			reply.lstat = nlm_granted;
		} else {
			reply.lstat = nlm_denied;
			reply.stat.nlm_testrply_u.holder.svid = a->lck.lox.pid;
			reply.stat.nlm_testrply_u.holder.l_offset =
				a->lck.lox.base;
			reply.stat.nlm_testrply_u.holder.l_len =
				a->lck.lox.length;
			if (a->lck.lox.type == F_WRLCK)
			    reply.stat.nlm_testrply_u.holder.exclusive = TRUE;
			else
			    reply.stat.nlm_testrply_u.holder.exclusive = FALSE;
			reply.stat.nlm_testrply_u.holder.oh_len = 0;
			reply.stat.nlm_testrply_u.holder.oh_bytes = NULL;
			if (debug) {
	    printf("NLM_REPLY testmsg: stat=%d svid=%d l_offset=%u l_len=%u\n",
				    reply.lstat,
				    reply.stat.nlm_testrply_u.holder.svid,
				    reply.stat.nlm_testrply_u.holder.l_offset,
				    reply.stat.nlm_testrply_u.holder.l_len);
			}
		}
		break;
	case NLM_LOCK_MSG:
		xdr_reply = xdr_nlm_res;
		act = NEWCALL;
		proc = NLM_LOCK_RES;
		name = a->lck.clnt_name;
		break;
	case NLM_CANCEL_MSG:
		xdr_reply = xdr_nlm_res;
		act = NEWCALL;
		proc = NLM_CANCEL_RES;
		name = a->lck.clnt_name;
		break;
	case NLM_UNLOCK_MSG:
		xdr_reply = xdr_nlm_res;
		act = NEWCALL;
		proc = NLM_UNLOCK_RES;
		name = a->lck.clnt_name;
		break;
	case NLM_GRANTED_MSG:
		xdr_reply = xdr_nlm_res;
		act = NEWCALL;
		proc = NLM_GRANTED_RES;
		name = a->lck.clnt_name;
		break;
	default:
		syslog(LOG_ERR, "unknown nlm_reply proc value: %d", proc);
		return;
	}

	/* malloc a copy of cookie for global variable reply */
	if (a->cookie.n_len) {
		if (obj_copy(&reply.cookie, &a->cookie) == -1) {
			reply.cookie.n_len= 0;
			reply.cookie.n_bytes= NULL;
		}
	} else {
		reply.cookie.n_len= 0;
		reply.cookie.n_bytes= NULL;
	}

	if (act == NREPLY) { /* reply to nlm_transp */
		if (debug)
			printf("rpc nlm_reply %d: %d (%s)\n",
				proc, reply.lstat, nlm_stat2name(reply.lstat));
		if (!svc_sendreply(nlm_transp, xdr_reply, &reply))
			svcerr_systemerr(nlm_transp);
	}
	else { /* issue a one way rpc call to reply */
		if (debug)
			printf("nlm_reply msg: (%s, %d), result = %d (%s)\n",
			    name, proc, reply.lstat, nlm_stat2name(reply.lstat));
		/*
		 * First try sending the reply using the host cache.
		 * If that fails, try sending it without using the cache.
		 */
		usecache = 1;
    callit:
		rpc_err = a->rpctransp(name, NLM_PROG, NLM_VERS, proc,
			    xdr_reply, &reply, xdr_void, NULL, usecache, 0);

		if (rpc_err == RPC_CANTSEND) {
			if (usecache) {
				usecache = 0;
				goto callit;
			}
		} else if (rpc_err != RPC_TIMEDOUT) {
			syslog(LOG_ERR, "nlm_reply: transp(%s) error: %s",
				name, clnt_sperrno(rpc_err));
		}

		/* free up the cookie that was malloc'ed earlier */
		if (reply.cookie.n_len) {
			xfree(&reply.cookie.n_bytes);
			reply.cookie.n_len = 0;
			reply.cookie.n_bytes = NULL;
		}
	}
}

/* ---------------------------------------------------------------------- */

/*
 * Send a grant message when the queued request is granted.
 * Called when the rsetlkw thread returns (either a normal exit or is
 * killed by cancel or cleanup calls). 
 *
 * XXX how to communicate an fcntl() error back to the requester?
 */

int
grant_remote(void)
{
	pid_t pid;
	union wait status;
	struct reclock *req;

	if (debug)
		printf("\n+++ %s +++  grant_remote\n", prtime());

	while ((pid = wait3(&status.w_status, WNOHANG, 0)) > 0) {
	    if (debug)
		    printf(" reaping %d: %x\n", pid, status.w_status);
	    if ((req = find_blkpid(pid)) != NULL) {
		    if (WIFEXITED(status)) {
			    if (req->blk.stat == 0) {
				    if (debug)
					show_lock_owner(req->blk.fd);
				    (void) nlm_call(NLM_GRANTED_MSG, req, 0);
#if 0
			    } else {
				    /* 
				     * XXX What to do if lock failed??? 
				     * If send NLM_LOCK_RES (denied) back,
				     * the client's nlm_res_routine will
				     * drop this reply.
				     */

				    nlm_reply(NLM_LOCK_MSG, nlm_denied_nolocks,
						req);
#endif
			    }
		    }
		    fd_unblock(req);
		    dequeue_block_req(req);
	    } else if (debug) {
		    printf(" *** can't find req\n");
	    }
	}
	if (debug > 1)
		printf("--- grant_remote\n");
}

/*
 * rsetlkw, executed as a separate thread, will wait for the lock.
 * The SIGCHLD handler, grant_remote, notices when it finishes and
 * will notify the remote client with the result.
 */

static void
rsetlkw(void *arg, size_t len)
{
	struct flock ld;
	pid_t ppid;
	struct reclock *a = (struct reclock *)arg;

	(void) prctl(PR_TERMCHILD);
	if ((ppid = getppid()) == 1)
		/* our parent died before we could set TERMCHILD */
		_exit(0);

	ld.l_type = a->exclusive ? F_WRLCK : F_RDLCK;
	ld.l_whence = 0;
	ld.l_start = a->lck.lox.base;
	ld.l_len = a->lck.lox.length;
	/*
	 * Must use the master LM pid so when this process exits, the
	 * lock won't be freed by kern/os/flock.c's cleanlocks().
	 * (Using the master LM pid also lets locks get cleaned
	 * up when it exits.)
	 */
	ld.l_pid = ppid;
	ld.l_sysid = a->lck.lox.sysid;

	if (fcntl(a->blk.fd, F_RSETLKW, &ld) < 0) {
		a->blk.stat = oserror();
	}
	_exit(0);	/* don't close stdio buffers */
}

nlm_stats
local_lock(a)
	struct reclock *a;
{
	int fd;
	struct flock ld;
	nlm_stats stat;
	int isnewfd, isnewid;

	/*
	 * convert fh to fd
	 */
	if ((fd = get_fd(a, &isnewfd)) < 0) {
		return ((fd == -1) ? nlm_denied_nolocks : nlm_denied);
	}

	/*
	 * set the lock
	 */
	if (debug) {
		struct reclock *req;

		printf("enter local_lock...FD=%d %s\n",
			fd, isnewfd ? "NEW" :"");
		if (debug > 1) {
			pr_lock(a);
		} 
		if ((req = find_lckpid(a)) != NULL) {
			printf(">>> but it's blocked?! <<<\n");
			pr_lock(req);
		}
	}
	ld.l_type = a->exclusive ? F_WRLCK : F_RDLCK;
	ld.l_whence = 0;
	ld.l_start = a->lck.lox.base;
	ld.l_len = a->lck.lox.length;
	ld.l_pid = pid;
	ld.l_sysid = a->lck.lox.sysid =
			mksysid(a->lck.lox.pid, a->lck.lox.ipaddr, &isnewid);
	if (ld.l_sysid == 0) {
		/* Can't allocate a new ID -- reject the request */ 
		if (isnewfd) {
			remove_fd(fd);
		}
		return (nlm_denied_nolocks);
	}

	if (debug) {
		printf(
		    "type=%s%s start=%ld len=%ld  pid=%d ipaddr=%x sysid=%d %s\n",
			lck2name(ld.l_type), a->block ? " blk" : "",
			ld.l_start, ld.l_len, a->lck.lox.pid,
			a->lck.lox.ipaddr, ld.l_sysid, isnewid ? "NEW" : "");
	}

	/* First try to get the lock without waiting */
	if (fcntl(fd, F_RSETLK, &ld) != -1) {
		stat = nlm_granted;
	} else {
		switch (errno) {
		case EDEADLK:
			if (debug)
				printf("local_lock: fcntl: deadlock\n");
			stat = nlm_deadlck;
			break;

		case ENOLCK:
			if (debug)
				printf("local_lock: out of locks\n");
			stat = nlm_denied_nolocks;
			break;

		case EACCES:
		case EAGAIN:
			/*
			 * No such luck, now create a thread to do the waiting,
			 * if it's a blocking lock.
			 * The thread shares file descriptors so it can get 
			 * the lock for the main thread.
			 */
			if (debug)
				show_lock_owner(fd);
			if (a->block) {
				a->blk.fd = fd;
				a->blk.stat = 0;
				a->blk.pid = sprocsp(rsetlkw,
						PR_SADDR|PR_SFDS|PR_NOLIBC,
						(void *)a,
						(caddr_t)NULL,
						(size_t)64*1024);
				if (a->blk.pid < 0) {
					if (debug)
						printf("!sproc failed\n");
					syslog(LOG_ERR,
						    "sproc failed: %m");
					stat = nlm_denied_nolocks;
					goto cleanup;
				} else {
					stat = nlm_blocked;

					/* 
					 * Keep track of # of procs blocked on
					 * this fd.
					 */
					fd_block(fd, 1);
					if (debug)
					    printf("*block thread=%d\n",
						a->blk.pid);
				}
			} else {
				stat = nlm_denied;
			}
			break;

		default:
			if (debug)
				printf("unable to set a lock: %s\n",
					strerror(errno));
			stat = nlm_denied;
			break;
		}
	}
	if (stat == nlm_granted || stat == nlm_blocked) {
		/*
		 * Associate the fd with the sysid for recovery handling
		 * (need to find all fd's used by a specific host).
		 * mksysid acquires a reference to the sysid
		 * since this is a lock request, do not release the sysid
		 */
		if (save_fd(ld.l_sysid, fd)) {
			if ( record_lock( fd, a, &ld ) == 0 )
				return (stat);
			free_fd( fd );
		}
		 /* If can't make the association, fail the request. */
		if (stat == nlm_blocked) {
			kill(a->blk.pid, SIGTERM);
		}
		stat = nlm_denied_nolocks;
	} 
cleanup:
	if (isnewfd) {
		remove_fd(fd);
	}
	/*
	 * always release the sysid on failure
	 * we always come here on failure
	 */
	relsysid(ld.l_sysid);
	return (stat);
}



nlm_stats
local_unlock(a)
	struct reclock *a;
{
	static struct flock ld;
	static int fd;
	sysid_t id;
	int isnewfd;
	nlm_stats stat;


	/*
	 * convert fh to fd
	 */
	if ((fd = get_fd(a, &isnewfd)) < 0) {
		return ((fd == -1) ? nlm_denied_nolocks : nlm_denied);
	}
	if (debug) {
		struct reclock *req;

		if (debug > 1) {
			pr_lock(a);
		}
		if ((req = find_lckpid(a)) != NULL) {
			printf(">>> but it's blocked?! <<<\n");
			pr_lock(req);
		}
	}
	if (isnewfd) {
		/* Client is trying to unlock a file it has not locked.  */
		if (debug)
			printf(
	" >>> fd %d not locked: client %x %d unlocking! st %d len %d) <<<\n",
				fd, a->lck.lox.ipaddr, a->lck.lox.pid,
				a->lck.lox.base, a->lck.lox.length);
		remove_fd(fd);
		return (nlm_granted);
	}

	/*
	 * Clear the lock
	 * find the sysid
	 * if no sysid is found, just return nlm_granted
	 * findsysid() does not acquire a reference to the sysid
	 */
	ld.l_sysid = id = findsysid(a->lck.lox.pid, a->lck.lox.ipaddr);
	if (ld.l_sysid == 0) {
		/* Haven't seen this client before so how can it have a lock? */
		if (debug)
			printf(" >>> unknown client %x %d <<<\n",
				a->lck.lox.ipaddr, a->lck.lox.pid);
		return (nlm_granted);
	}
	ld.l_type = F_UNLCK;
	ld.l_whence = 0;
	ld.l_start = a->lck.lox.base;
	ld.l_len = a->lck.lox.length;
	ld.l_pid = pid;

	if (debug) {
		printf(
		    "type %s start=%ld len=%ld  pid %d ipaddr %x sysid=%d\n",
			lck2name(ld.l_type), ld.l_start, ld.l_len, a->lck.lox.pid,
			a->lck.lox.ipaddr, ld.l_sysid);
	}

	if (fcntl(fd, F_RSETLK, &ld) == -1) {
		if (errno == ENOLCK) {
			syslog(LOG_ERR, "out of locks.");
			stat = nlm_denied_nolocks;
		} else {
			syslog(LOG_ERR, "unlock fcntl: %m");
			stat = nlm_denied;
		}
	} else {
		stat = nlm_granted;
		/*
		 * this is an unlock, so release the sysid reference acquired in
		 * local_lock
		 * do the lock record accounting before freeing the file descriptor
		 * make sure the lock data has the correct sysid
		 */
		ld.l_sysid = id;
		if ( record_lock( fd, a, &ld ) ) {
			syslog(LOG_ERR, "unlock record_lock: %m");
		}
		/* Free the state if we don't hold any locks on this fd */
		free_fd(fd);
	}
	relsysid( id );
	return (stat);
}

static nlm_stats
local_test(a)
	struct reclock *a;
{
	int fd;
	struct flock ld;
	sysid_t id;
	int isnewfd, isnewid;
	nlm_stats stat;

	/*
	 * convert fh to fd
	 */
	if ((fd = get_fd(a, &isnewfd)) < 0) {
		return ((fd == -1) ? nlm_denied_nolocks : nlm_denied);
	}
	if (debug) {
		struct reclock *req;

		if ((req = find_lckpid(a)) != NULL) {
			printf(">>> but it's blocked?! <<<\n");
			pr_lock(req);
		}
	}

	/*
	 * test the lock
	 */
	ld.l_type = a->exclusive ? F_WRLCK : F_RDLCK;
	ld.l_whence = 0;
	ld.l_start = (a->lck.lox.base >= 0) ? a->lck.lox.base : 0;
	ld.l_len = a->lck.lox.length;
	ld.l_pid = pid;
	/* Save the sysid in case the fcntl() returns a different one */
	ld.l_sysid = id = mksysid(a->lck.lox.pid, a->lck.lox.ipaddr, &isnewid);
	if (ld.l_sysid == 0) {
		if (isnewfd) {
			remove_fd(fd);
		}
		return (nlm_denied_nolocks);
	}

	if (debug) {
		if (isnewfd)
			printf(">>> new fd %d <<<\n", fd);
		printf("before: start=%ld len=%ld type=%s sysid=%d %s\n",
		 ld.l_start, ld.l_len, lck2name(ld.l_type), ld.l_sysid,isnewid ?"NEW":"");
	}

	if (fcntl(fd, F_RGETLK, &ld) == -1) {
		syslog(LOG_ERR, "unable to test a lock: %m");
		stat = nlm_denied;
	} else {
		if (ld.l_type == F_UNLCK) {
			stat = nlm_granted;
			a->lck.lox.type = ld.l_type;
		} else {
			stat = nlm_denied;
			a->lck.lox.type = ld.l_type;
			a->lck.lox.base = ld.l_start;
			a->lck.lox.length = ld.l_len;
			if (ld.l_sysid == 0) { /* lock held by local process */
				a->lck.lox.pid = ld.l_pid;
			} else {
				/*
				 * Assume the LM is the only one
				 * setting sysid on locks.
				 */
				a->lck.lox.pid = sysid2pid(ld.l_sysid);
			}
		}
	}

	/*
	 * Update fd table
	 */
	if (isnewfd) {
		remove_fd(fd);
	}
	/*
	 * no lock is actually acquired, so release the sysid
	 */
	relsysid(id);

	if (debug) {
		printf("after:  start=%ld len=%ld type=%s sysid=%d\n",
			ld.l_start, ld.l_len, lck2name(ld.l_type), ld.l_sysid);
	}
	return (stat);
}

/*
 * This routine is executed by the client LM only. It's called as a
 * result of blocking thread gets the lock and the server sends us a
 * granted msg.
 */

static nlm_stats
local_grant(a)
	struct reclock *a;
{
	extern msg_entry *msg_q;
	msg_entry *msgp;
	remote_result *resp;

        if (debug) {
                printf("enter local_grant\n");
		pr_lock(a);
	}
	msgp = msg_q;
        while (msgp != NULL) {

		if (debug && msgp->req->alock.lox.pid == a->alock.lox.pid) {
			pr_lock(msgp->req);
		}
		if (obj_cmp(&msgp->req->cookie, &a->cookie) &&
		    same_bound(&(msgp->req->alock.lox), &(a->alock.lox)) &&
			same_type(&(msgp->req->alock.lox), &(a->alock.lox)) &&
			(msgp->req->alock.lox.pid == a->alock.lox.pid)) {


			/* upgrade request from pending to granted in
			 * monitoring list for recovery.  */
			upgrade_req_in_me(msgp->req);

			/*
			 * if the reply is for older request, set the
			 * reply result so that the nxt poll by the kernel
			 * will get this result
			 */
			if (msgp->reply != NULL) {
				msgp->reply->lstat = nlm_granted;
			} else if ((resp = get_res()) != NULL) {
				/* no reply is set, lets save this reply */
				resp->lstat = nlm_granted;
				msgp->reply = resp;
			}
			break;
                }
                msgp = msgp->nxt;
        }
        return (nlm_granted);
}


static void
show_lock_owner(int fd)
{
	struct flock ld;

	ld.l_type = F_WRLCK;
	ld.l_whence = 0;
	ld.l_start = 0;
	ld.l_len = 0;
	ld.l_pid = pid;
	ld.l_sysid = 0;
	if (fcntl(fd, F_RGETLK, &ld) < 0)
		printf("owner: getlk failed: %d", errno);
	else {
		printf("%d owner: type %s start=%ld len=%ld  pid=%d sysid=%d\n",
		    fd, lck2name(ld.l_type), ld.l_start, ld.l_len,
		    ld.l_pid, ld.l_sysid);
	}
}
