/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *  1.2 88/02/08 
 */


/*
 * This is the rpc server side idle loop
 * Wait for input, call server program.
 */

#ifdef __STDC__
	#pragma weak rpc_control = _rpc_control
	#pragma weak svc_args_done = _svc_args_done
	#pragma weak svc_max_fd = _svc_max_fd
	#pragma weak svc_mt_mode = _svc_mt_mode
	#pragma weak svc_nfds = _svc_nfds
	#pragma weak svc_nfds_set = _svc_nfds_set
	#pragma weak svc_pipe = _svc_pipe
	#pragma weak svc_polling = _svc_polling
	#pragma weak svc_run = _svc_run
	#pragma weak mt_debug = _mt_debug
#endif
#include "synonyms.h"

#include <rpc/rpc.h>
#include <rpc/errorhandler.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>		/* prototype for select() */
#include <fcntl.h>
#include <string.h>		/* prototype for strerror() */
#include <pthread.h>
#include <mplib.h>
#include <stdarg.h>
#include "priv_extern.h"

extern void clear_pollfd(int);
extern void set_pollfd(int);

extern pthread_mutex_t svc_thr_mutex;
extern pthread_cond_t svc_thr_fdwait;
extern pthread_mutex_t svc_mutex;


static void start_threads(int);
static void create_pipe(void);
static void clear_pipe(void);
static int select_next_pollfd(fd_set *);
static SVCXPRT *make_xprt_copy(SVCXPRT *);
static void _svc_run_mt(void);
static void __svc_run(void);

void _svc_prog_dispatch(SVCXPRT *, struct rpc_msg *, struct svc_req *);
static void _svc_done_private(SVCXPRT *);
static int fds_nbits(int, fd_set *);

extern pthread_rwlock_t svc_fd_lock;

/*
 * Variables used for MT
 */
int svc_mt_mode = RPC_SVC_MT_NONE;	/* multi-threading mode */
int svc_pipe[2] = {0,0}; /* pipe for breaking out of poll: read(0), write(1) */

/* BEGIN PROTECTED BY svc_mutex */
static int svc_thr_max = 1;	/* default maximum number of threads allowed */
static int svc_thr_total;	/* current number of threads */
static int svc_thr_active = 0;	/* current number of threads active */
#define	CIRCULAR_BUFSIZE	1024
static int svc_pending_fds[CIRCULAR_BUFSIZE+1];	/* fds with pending data */
static int svc_next_pending;		/* next one to be processed */
static int svc_last_pending;	/* last one in list */
static int svc_total_pending = 0;		/* total in list */
static int svc_thr_total_creates;	/* total created - stats */
static int svc_thr_total_create_errors;	/* total create errors - stats */
static int svc_waiters = 0;	 /* number of waiting threads */
/* END PROTECTED BY svc_mutex */

/* BEGIN PROTECTED BY svc_fd_lock: */
int svc_nfds = 0;				/* total number of active file descriptors */
int svc_nfds_set = 0;	/* total number of fd bits set in svc_fdset */
int svc_max_fd = 0;	/* largest active file descriptor */
/* END PROTECTED BY svc_fd_lock: */

/* BEGIN PROTECTED BY svc_thr_mutex */
#define	POLLSET_EXTEND	256
static int svc_pollfds = 0;	/* no of active fds in last select() - output */
static int svc_next_pollfd; /* next fd  to processin svc_pollset */
bool_t svc_polling = { FALSE };	/* true if a thread is polling */
/* END PROTECTED BY svc_thr_mutex */

void
svc_run()
{
	/* NO OTHER THREADS ARE RUNNING */

	switch (svc_mt_mode) {
	case RPC_SVC_MT_NONE:
		__svc_run();
		break;
	default:
		_svc_run_mt();
		break;
	}
}

static void
__svc_run()
{
	fd_set readfds;

	for (;;) {
		readfds = svc_fdset;
		switch (select(FD_SETSIZE, &readfds, (fd_set *)0, (fd_set *)0,
				   (struct timeval *)0)) {
		case -1:
			if (errno == EINTR) {
				continue;
			}
			_rpc_errorhandler(LOG_ERR, "svc_run: select failed: %s", strerror(errno));
			return;
		case 0:
			continue;
		default:
			svc_getreqset(&readfds);
		}
	}
}

static void
_svc_run_mt()
{
	int nfds;
	int n_polled, dispatch;

	static bool_t first_time = TRUE;
	int main_thread = 0;
	int n_new;
	int myfd;
	SVCXPRT *parent_xprt, *xprt;
	fd_set readfds;
	pthread_t self=0;

	MTLIB_STATUS((MTCTL_PTHREAD_SELF), self);
	/*
	 * Server is multi-threaded.  Do "first time" initializations.
	 * Since only one thread exists in the beginning, there's no
	 * need for mutex protection for first time initializations.
	 */
	if (first_time) {
		main_thread = self;
		first_time = FALSE;
		svc_thr_total = 1;  /* this thread */
		svc_next_pending = svc_last_pending = 0;

		/*
		 * Create a pipe for waking up the poll, if new
		 * descriptors have been added to svc_fdset.
		 */
		create_pipe();
	}

	/* OTHER THREADS ARE RUNNING */

	for (;;) {
		/*
		 * svc_thr_mutex prevents more than one thread from
		 * trying to select a descriptor to process further.
		 * svc_thr_mutex is unlocked after a thread selects
		 * a descriptor on which to receive data.  If there are
		 * no such descriptors, the thread will poll with
		 * svc_thr_mutex locked, after unlocking all other
		 * locks.  This prevents more than one thread from
		 * trying to poll at the same time.
		 */
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_LOCK, &svc_thr_mutex) );
		MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_LOCK, &svc_mutex) );
continue_with_locks:
		myfd = -1;

		/*
		 * Check if there are any descriptors with data pending.
		 */
		if (svc_total_pending > 0) {
			myfd = svc_pending_fds[svc_next_pending++];
			if (svc_next_pending > FD_SETSIZE)
				svc_next_pending = 0;
			svc_total_pending--;
		}

		/*
		 * Get the next active file descriptor to process.
		 */
		if (myfd == -1 && svc_pollfds == 0) {
			/*
			 * svc_pollset is empty; do polling
			 */
			svc_polling = TRUE;

			/*
			 * if there are no file descriptors, return
			 */
			MTLIB_INSERT( (MTCTL_PTHREAD_RWLOCK_RDLOCK, &svc_fd_lock) );
			if (svc_nfds == 0) {
				MTLIB_INSERT( (MTCTL_PTHREAD_RWLOCK_UNLOCK, 
					       &svc_fd_lock) );
				svc_polling = FALSE;
				MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, 
					       &svc_mutex) );
				MTLIB_INSERT( (MTCTL_PTHREAD_MUTEX_UNLOCK, 
					       &svc_thr_mutex) );

				if (!main_thread) {
					svc_thr_total--;
#ifdef MTDEBUG
					mt_debug(1, "_svc_run_mt: %d exit, svc_thr_total is %d", self, svc_thr_total);
#endif
					MTLIB_INSERT( (MTCTL_PTHREAD_EXIT, 0) );
				}
				break;
			}

			nfds = fds_nbits(svc_max_fd, &svc_fdset);
			MTLIB_INSERT((MTCTL_PTHREAD_RWLOCK_UNLOCK, 
				&svc_fd_lock));

			if (nfds == 0) {
				/*
		 		* There are file descriptors, but none of them
		 		* are available for polling.  If this is the
		 		* main thread, or if no thread is waiting,
		 		* wait on condition variable, otherwise exit.
		 		*/
				svc_polling = FALSE;
				MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_UNLOCK, 
					&svc_thr_mutex));
				if ((!main_thread) && svc_waiters > 0) {
					svc_thr_total--;
					MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_UNLOCK,
						&svc_mutex));
#ifdef MTDEBUG
					mt_debug(1, 
						"_svc_run_mt: exit %d "
						"svc_thr_total %d",
						self, svc_thr_total);
#endif
					MTLIB_INSERT((MTCTL_PTHREAD_EXIT, 0));
				}
				while (svc_nfds_set == 0 && svc_pollfds == 0 &&
				       svc_total_pending == 0) {
					svc_waiters++;
					MTLIB_INSERT((MTCTL_PTHREAD_COND_WAIT,
						&svc_thr_fdwait, &svc_mutex));
					svc_waiters--;
				}

				MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_UNLOCK, 
					&svc_mutex));
				continue;
			}

			/*
			 * We're ready to poll.  Always set svc_pipe[0]
			 * as the last one, since the poll will occasionally
			 * need to be interrupted.  Release svc_mutex for
			 * the duration of the poll, but hold on to
			 * svc_thr_mutex, as we don't want any other thread
			 * to do the same.
			 */
			readfds = svc_fdset;
			FD_SET(svc_pipe[0], &readfds);

			do {
				MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_UNLOCK, 
					&svc_mutex));
#ifdef MTDEBUG
				mt_debug(1, "_svc_run_mt: %d select", self);
#endif
				n_polled = select(FD_SETSIZE, &readfds, 
						  (fd_set *)0, (fd_set *)0, 
						  (struct timeval *)0);
				MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_LOCK, 
					&svc_mutex));
			} while (n_polled <= 0);
			svc_polling = FALSE;

			if (FD_ISSET(svc_pipe[0], &readfds)) {
				clear_pipe();
				n_polled--;
				FD_CLR(svc_pipe[0], &readfds);
			}
			svc_pollfds = n_polled;
			svc_next_pollfd = 0;

			/*
			 * If no descriptor is active, continue.
			 */
			if (svc_pollfds == 0) {
				goto continue_with_locks;
			}
		}

		/*
		 * If a file descriptor has already not been selected,
		 * choose a file descriptor.
		 * svc_pollfds and svc_next_pollfd are updated.
		 */
		if (myfd == -1) {
			if ((myfd = select_next_pollfd(&readfds)) == -1) {
				goto continue_with_locks;
			}
		}

		/*
		 * Check to see if new threads need to be started.
		 * Count of threads that could be gainfully employed is
		 * obtained as follows:
		 *  - count 1 for poller
		 *  - count 1 for this request
		 *  - count active file descriptors (svc_pollfds)
		 *  - count pending file descriptors
		 *
		 * (svc_thr_total - svc_thr_active) are already available.
		 * This thread is one of the available threads.
		 *
		 * Number of new threads should not exceed
		 *  (svc_thr_max - svc_thr_total).
		 */
		if (svc_thr_total < svc_thr_max &&
			svc_mt_mode == RPC_SVC_MT_AUTO) {
#ifdef MTDEBUG
			mt_debug(1, "_svc_run_mt: %d svc_thr_total is %d "
				    "svc_thr_max is %d svc_thr_active is %d", 
				    self, svc_thr_total, svc_thr_max, 
				    svc_thr_active);
			mt_debug(1, "_svc_run_mt: %d svc_pollfds is %d "
				    "svc_total_pending is %d", 
				    self, svc_pollfds, svc_total_pending);
#endif
			n_new = 1 + 1 + svc_pollfds + svc_total_pending -
					(svc_thr_total - svc_thr_active);
			if (n_new > (svc_thr_max - svc_thr_total))
				n_new = svc_thr_max - svc_thr_total;
			if (n_new > 0)
				start_threads(n_new);
		}

		/*
		 * Get parent xprt.  It is possible for the parent service
		 * handle to be destroyed by now, due to a race condition.
		 * Check for this, and if so, log a warning and go on.
		 */
		parent_xprt = xports[myfd];
		if (parent_xprt == NULL || parent_xprt->xp_sock != myfd ||
			svc_defunct(parent_xprt) || svc_failed(parent_xprt)) {
			goto continue_with_locks;
		}

		/*
		 * Make a copy of parent xprt, update svc_fdset.
		 */
		if ((xprt = make_xprt_copy(parent_xprt)) == NULL) {
			goto continue_with_locks;
		}

		/*
		 * Keep track of active threads in automatic mode.
		 */
		if (svc_mt_mode == RPC_SVC_MT_AUTO)
			svc_thr_active++;

		/*
		 * Release mutexes so other threads can get going.
		 */
		MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_UNLOCK, &svc_mutex));
		MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_UNLOCK, &svc_thr_mutex));

		/*
		 * Process request.
		 */
		{
			struct rpc_msg *msg;
			struct svc_req *r;
			char *cred_area;
#ifdef MTDEBUG
			pthread_t self=0;
		
			MTLIB_STATUS((MTCTL_PTHREAD_SELF), self);
			mt_debug(1, "_svc_run_mt: %d process request", self);
#endif
			msg = SVCEXT(xprt)->msg;
			r = SVCEXT(xprt)->req;
			cred_area = SVCEXT(xprt)->cred_area;

			msg->rm_call.cb_cred.oa_base = cred_area;
			msg->rm_call.cb_verf.oa_base = &(cred_area[MAX_AUTH_BYTES]);
			r->rq_clntcred = &(cred_area[2*MAX_AUTH_BYTES]);
		
			/* now receive msgs from xprtprt (support batch calls) */
			if ((dispatch = SVC_RECV(xprt, msg))) {

				if (svc_mt_mode != RPC_SVC_MT_NONE)
					svc_flags(xprt) |= SVC_ARGS_CHECK;
				_svc_prog_dispatch(xprt, msg, r);
			} else
				svc_args_done(xprt);
			/*
			 * Finish up, if automatic mode, or not dispatched.
			 */
			if (svc_mt_mode == RPC_SVC_MT_AUTO || !dispatch) {
				if (svc_flags(xprt) & SVC_ARGS_CHECK)
					svc_args_done(xprt);
				MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_LOCK, 
					&svc_mutex));
				_svc_done_private(xprt);
				if (svc_mt_mode == RPC_SVC_MT_AUTO) {
					/*
					 * not active any more
					 */
					svc_thr_active--;

					/*
					 * If not main thread, exit unless
					 * there's some immediate work.
					 */
					if (!main_thread && svc_pollfds <= 0 &&
					    svc_total_pending <= 0 &&
					    (svc_polling || svc_waiters > 0)) {
						svc_thr_total--;
						MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_UNLOCK, 
							&svc_mutex));
#ifdef MTDEBUG
						mt_debug(1, "_svc_run_mt: exit "
							" %d svc_thr_total %d",
							self, svc_thr_total);
#endif
						MTLIB_INSERT((MTCTL_PTHREAD_EXIT, 0));
						/* NOTREACHED */
					}
				}
				MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_UNLOCK, 
					&svc_mutex));
			}
		}
	}
}

/*
 * start_threads() - Start specified number of threads.
 */
static void
start_threads(num_threads)
	int		num_threads;
{
	int i, error;
#if !defined(_LIBC_ABI) && !defined(_LIBC_NOMP)
	pthread_t   pt;
	pthread_attr_t  pta;
#endif

	MTLIB_INSERT((MTCTL_PTHREAD_ATTR_INIT, &pta));
	MTLIB_INSERT((MTCTL_PTHREAD_ATTR_SETDETACHSTATE, &pta, 
		PTHREAD_CREATE_DETACHED));

	for (i = 0; i < num_threads; i++) {
		MTLIB_STATUS((MTCTL_PTHREAD_CREATE, &pt, &pta, 
			(void *(*)(void*))_svc_run_mt, NULL), error);
#ifdef MTDEBUG
		mt_debug(1, "start_threads: pthread_create: pt=%d error=%d", 
			 pt, error);
#endif
		if (error) {
			svc_thr_total_create_errors++;
		} else {
			svc_thr_total++;
			svc_thr_total_creates++;
		}
	}
}

/*
 * create_pipe() - create pipe for breaking out of poll.
 */
static void
create_pipe()
{
	if (pipe(svc_pipe) == -1) {
		_rpc_errorhandler(LOG_ERR, "create_pipe: pipe: %s: exiting", 
			strerror(errno));
		exit(1);
	}
	if (fcntl(svc_pipe[0], F_SETFL, O_NONBLOCK) == -1) {
		_rpc_errorhandler(LOG_ERR, "create_pipe: fcntl: %s: exiting", 
			strerror(errno));
		exit(1);
	}
	if (fcntl(svc_pipe[1], F_SETFL, O_NONBLOCK) == -1) {
		_rpc_errorhandler(LOG_ERR, "create_pipe: fcntl: %s: exiting", 
			strerror(errno));
		exit(1);
	}
}

/*
 * clear_pipe() - Empty data in pipe.
 */
static void
clear_pipe()
{
	char	buf[16];
	int i;

	do {
		i = (int)read(svc_pipe[0], buf, sizeof (buf));
	} while (i == sizeof (buf));
}

/*
 * select_next_pollfd() - Select the next active fd in readfds.
 */
static int
select_next_pollfd(fd_set *readfds)
{
	int i;
	fd_set rdfds;

	for (i = 0; i < howmany(FD_SETSIZE, NFDBITS); i++) {
		rdfds.fds_bits[i] =
			readfds->fds_bits[i] & svc_fdset.fds_bits[i];
	}

	for (i = svc_next_pollfd; svc_pollfds > 0 && i < FD_SETSIZE; i++) {
		if (FD_ISSET(i, &rdfds)) {
			svc_pollfds--;
			svc_next_pollfd = i + 1;
			return (i);
		}
	}
	svc_next_pollfd = svc_pollfds = 0;
	return (-1);
}


/*
 *  Given an fd_set pointer and the number of bits to check in it,
 *  return the number of bits set.
 */
static int
fds_nbits(fdmax, fdset)
	int fdmax;		/* number of bits we must test */
	fd_set *fdset;	/* source fd_set array */
{
	/* register declarations ordered by expected frequency of use */
	register int i, j, n=0;

	j = ((fdmax >= FD_SETSIZE) ? FD_SETSIZE : fdmax);
	for (i = 0; i < j; i++) {
		if (FD_ISSET(i, fdset)) {
			n++;
		}
	}
	return (n);
}

/*
 * make_xprt_copy() - make a copy of the parent xprt.
 * Clear fd bit in svc_fdset.
 */
static SVCXPRT *
make_xprt_copy(parent)
	SVCXPRT *parent;
{
	SVCXPRT_LIST	*xlist = SVCEXT(parent)->my_xlist;
	SVCXPRT_LIST	*xret;
	SVCXPRT	 	*xprt;
	int	 	fd = parent->xp_sock;
	xret = xlist->next;
	if (xret) {
		xlist->next = xret->next;
		xret->next = NULL;
		xprt = xret->xprt;
		svc_flags(xprt) = svc_flags(parent);
	} else
		xprt = svc_copy(parent);

	if (xprt) {
		SVCEXT(parent)->refcnt++;
		MTLIB_INSERT((MTCTL_PTHREAD_RWLOCK_WRLOCK, &svc_fd_lock));
		clear_pollfd(fd);
		MTLIB_INSERT((MTCTL_PTHREAD_RWLOCK_UNLOCK, &svc_fd_lock));
	}
	return (xprt);
}

/*
 * _svc_done_private() - return copies to library.
 */
static void
_svc_done_private(xprt)
	SVCXPRT	 *xprt;
{
	SVCXPRT	 *parent;
	SVCXPRT_LIST	*xhead, *xlist;
	if ((parent = SVCEXT(xprt)->parent) == NULL)
		return;

	xhead = SVCEXT(parent)->my_xlist;
	xlist = SVCEXT(xprt)->my_xlist;
	xlist->next = xhead->next;
	xhead->next = xlist;
	SVCEXT(parent)->refcnt--;
	svc_flags(xprt) |= svc_flags(parent);
	if (SVCEXT(parent)->refcnt == 0 && (svc_failed(xprt) ||
						svc_defunct(xprt))) {
		_svc_destroy_private(xprt);
	}
}

/*
 * Mark argument completion.  Release file descriptor.
 */
void
svc_args_done(xprt)
	SVCXPRT	 *xprt;
{
	char	dummy;
	SVCXPRT *parent = SVCEXT(xprt)->parent;
	bool_t  wake_up_poller;
	enum	xprt_stat stat;
	svc_flags(xprt) |= svc_flags(parent);
	svc_flags(xprt) &= ~SVC_ARGS_CHECK;
	if (svc_failed(xprt) || svc_defunct(parent))
		return;
	if (svc_type(xprt) == SVC_CONNECTION &&
				(stat = SVC_STAT(xprt)) != XPRT_IDLE) {
		if (stat == XPRT_MOREREQS) {
			MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_LOCK, &svc_mutex));
			svc_pending_fds[svc_last_pending++] = xprt->xp_sock;
			if (svc_last_pending > FD_SETSIZE)
				svc_last_pending = 0;
			svc_total_pending++;
			MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_UNLOCK, &svc_mutex));
			wake_up_poller = FALSE;
		} else {
			/*
			 * connection failed
			 */
			return;
		}
	} else {
		MTLIB_INSERT((MTCTL_PTHREAD_RWLOCK_WRLOCK, &svc_fd_lock));
		set_pollfd(xprt->xp_sock);
		MTLIB_INSERT((MTCTL_PTHREAD_RWLOCK_UNLOCK, &svc_fd_lock));
		wake_up_poller = TRUE;
	}

	if (!wake_up_poller || !svc_polling) {
		/*
		 * Wake up any waiting threads.
		 */
		MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_LOCK, &svc_mutex));
		if (svc_waiters > 0) {
			MTLIB_INSERT((MTCTL_PTHREAD_COND_BROADCAST, 
				&svc_thr_fdwait));
			MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_UNLOCK, &svc_mutex));
			return;
		}
		MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_UNLOCK, &svc_mutex));
	}

	/*
	 * Wake up any polling thread.
	 */
	if (svc_polling) {
		(void) write(svc_pipe[1], &dummy, sizeof (dummy));
	}
}

bool_t
rpc_control(op, info)
	int	 op;
	void		*info;
{
	int		tmp;
	switch (op) {
	case RPC_SVC_MTMODE_SET:
		tmp = *((int *)info);
		if (tmp != RPC_SVC_MT_NONE && tmp != RPC_SVC_MT_AUTO &&
						tmp != RPC_SVC_MT_USER)
			return (FALSE);
		if (svc_mt_mode != RPC_SVC_MT_NONE && svc_mt_mode != tmp)
			return (FALSE);
		if (!_libc_rpc_mt_init())
			return (FALSE);
		svc_mt_mode = tmp;
		return (TRUE);
	case RPC_SVC_MTMODE_GET:
		*((int *)info) = svc_mt_mode;
		return (TRUE);
	case RPC_SVC_THRMAX_SET:
		if ((tmp = *((int *)info)) < 1)
			return (FALSE);
		if (!_libc_rpc_mt_init())
			return (FALSE);
		MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_LOCK, &svc_mutex));
		svc_thr_max = tmp;
		MTLIB_INSERT((MTCTL_PTHREAD_MUTEX_UNLOCK, &svc_mutex));
		MTLIB_INSERT((MTCTL_PTHREAD_SETCONCURRENCY, tmp));
		return (TRUE);
	case RPC_SVC_THRMAX_GET:
		*((int *)info) = svc_thr_max;
		return (TRUE);
	case RPC_SVC_THRTOTAL_GET:
		*((int *)info) = svc_thr_total;
		return (TRUE);
	case RPC_SVC_THRCREATES_GET:
		*((int *)info) = svc_thr_total_creates;
		return (TRUE);
	case RPC_SVC_THRERRORS_GET:
		*((int *)info) = svc_thr_total_create_errors;
		return (TRUE);
	case __RPC_CLNT_MINFD_SET:
		tmp = *((int *)info);
		if (tmp < 0)
			return (FALSE);
		return (TRUE);
	case __RPC_CLNT_MINFD_GET:
		return (TRUE);
	default:
		return (FALSE);
	}
}

static int mtdebug = 0;

/*VARARGS2*/
void
mt_debug(int level, char *str, ...)
{
    va_list ap;

    va_start(ap, str);
    if (mtdebug == level || (mtdebug > 10 && (mtdebug - 10) >= level)) {
    	if (_using_syslog) {
    		vsyslog(LOG_ERR, str, ap);
    	} else {
    		vfprintf(stderr, str, ap);
    		putc('\n', stderr);
    	}
	}
    va_end(ap);
    return;
}
