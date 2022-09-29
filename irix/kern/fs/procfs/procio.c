#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kmem.h>
#include <sys/sema.h>
#include <sys/sysmacros.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/cmn_err.h>
#include <sys/uio.h>
#include <sys/cred.h>
#include <sys/sat.h>
#include <ksys/vproc.h>
#include "os/proc/pproc_private.h"
#include "prdata.h"

#include <sys/idbg.h>
#include <sys/idbgentry.h>

#define PROCQ_LOCK()                mutex_spinlock(&procq_lock)
#define PROCQ_UNLOCK(s)             mutex_spinunlock(&procq_lock, s)

/* 
 * This structure defines a linked list of procfs requests which
 * have been issued by clients but not yet been picked up for servicing.
 * The list is maintained as a singly linked, null terminated, fifo list.
 * Since data can be pushed/pulled from the remote cell only by the mesg
 * xthread, this module allocates a buffer into/from which the xthread
 * does io to the remote cell.
 */
typedef struct procfs_queue {
	struct procfs_queue	*forw;		/* list linkage */
	sema_t			sema;		/* sync with response */
	proc_t			*targproc;	/* prusrio arg from xthread */
	uthread_t		*targut;	/* prusrio arg from xthread */
	uio_t			*uiop;		/* prusrio arg from xthread */
	uio_rw_t		rw;		/* prusrio arg from xthread */
	cred_t			*xcred;		/* creds of rd/writer */
	caddr_t			balloc;		/* local buffer allocated */
	ssize_t			bsize;		/* local buffer size */
	int			retval;		/* retval from prusrio */
} qent_t;


/*
 * Global variables.
 */
sema_t	procfs_q_sema;		/* Counting sema for daemon to fork */
lock_t	procq_lock;	
qent_t	*procfs_q, *tail_q;	/* List of unserviced requests */


/*
 * Forward routine declarations.
 */
STATIC void	add_msg_to_q_and_wait(qent_t *);
STATIC void	wait_for_msg_response(qent_t *);
STATIC qent_t	*get_procfs_req(void);
STATIC void 	do_procfs_req(qent_t *);

/*
 * Could optimize in the ioctl case by not massaging the uio, 
 * which is already of type SYS.
 */
STATIC int
massage_uio(uio_t *old, enum uio_rw rw, qent_t *entry)
{
	int	error = 0;
	uio_t	*new = kmem_alloc(sizeof(uio_t), KM_SLEEP);
	iovec_t	*iovec = kmem_alloc(sizeof(iovec_t), KM_SLEEP);
	caddr_t	buf = (old->uio_resid == 0) ? NULL : 
				kmem_alloc(old->uio_resid, KM_SLEEP);

	entry->bsize = old->uio_resid;
	entry->balloc = buf;
	new->uio_iovcnt = 1;
	new->uio_iov = iovec;
	new->uio_segflg = UIO_SYSSPACE;
	new->uio_resid = old->uio_resid;
	new->uio_offset = old->uio_offset;
	new->uio_iov->iov_base = buf;
	new->uio_iov->iov_len = old->uio_resid;
	if (rw == UIO_WRITE) {
		error = uiomove(buf, old->uio_resid, UIO_WRITE, old);
	}
	entry->uiop = new;
	return(error);
}


STATIC int
free_uio(uio_t *new, uio_t *old, enum uio_rw rw, qent_t *entry)
{
	int	error= 0 ;

	caddr_t	buf = entry->balloc;
	ssize_t	count = old->uio_resid - new->uio_resid;

	if ((rw == UIO_READ) && (entry->retval == 0)) {
		error = uiomove(buf, count, UIO_READ, old);
	}
	old->uio_offset = new->uio_offset;
	old->uio_resid = new->uio_resid;
	if (entry->bsize) kmem_free(entry->balloc, entry->bsize);
	kmem_free(new->uio_iov, sizeof(iovec_t));
	kmem_free(new, sizeof(uio_t));
	return(error);
}


/*
 * This is the routine that the xthreads invoke to q a request to a 
 * daemon. All the parameters of the remote prusrio are recorded.
 */
int
issue_procfs_req(proc_t *p, uthread_t *ut, enum uio_rw rw, struct uio *uiop)
{
	int	ret;
	qent_t	*entry;

	entry = kmem_alloc(sizeof(qent_t), KM_SLEEP);
	/*
	 * Fill in the q entry with passed in parameters.
	 */
	entry->targproc = p;
	entry->targut = ut;
	entry->xcred = get_current_cred();
	entry->rw = rw;

	/*
	 * Create a new uio, copy in write data from remote cell.
	 */
	ret = massage_uio(uiop, rw, entry);
	if (ret) {
		ret = entry->retval = EFAULT;
	} else {
		/*
	 	 * Q request and wait for daemon to answer.
	 	 */
		add_msg_to_q_and_wait(entry);
	}

	ret = entry->retval;

	/*
	 * Copy the results to remote cell, update remote callers 
	 * uio.
	 */
	free_uio(entry->uiop, uiop, rw, entry);
	kmem_free(entry, sizeof(qent_t));

	return(ret);
}

/*
 * This routine takes the given procfs q entry, places it on the
 * end of the global msg list, and then waits for a response from
 * the daemon.
 */
STATIC void
add_msg_to_q_and_wait(qent_t *entry)
{
	int     s;

	/*
	 * Initialize the semaphore.
	 */
	initnsema(&entry->sema, 0, "proc_q");
	entry->forw = NULL;

	/*
	 * Place the message on the queue.
	 */
	s = PROCQ_LOCK();
	if (!procfs_q) {
		tail_q = procfs_q = entry;
	} else {
		tail_q->forw = entry;
		tail_q = entry;
	}
	PROCQ_UNLOCK(s);

	/*
	 * Poke the daemon.
	 */
	vsema(&procfs_q_sema);

	/*
	 * Wait for daemon to respond.
	 */
	wait_for_msg_response(entry);

	/*
	 * Message has already been removed from the queue.
	 * Just free the semaphore here.
	 */
	freesema(&entry->sema);
	return;
}

/*
 * This routine waits on the message semaphore until the
 * procfs daemon completes the request.
 */
STATIC void
wait_for_msg_response(qent_t *entry)
{
	psema(&entry->sema, PZERO);
	return;
}


/*
 * Routine invoked by daemon to signal to message xthread that 
 * the request servicing has completed.
 */
STATIC void
resp_procfs_req(qent_t *entry)
{
	vsema(&entry->sema);
}



/* 
 * This routine is called by the procfs daemon to wait till the next
 * request arrives to fork a servicing thread.
 */
STATIC void
waitfor_procfs_req(void)
{
	/*
	 * If there are no current requests, wait for one.
	 * Non breakable sleep.
	 */
	psema(&procfs_q_sema, PZERO);
	return;
}

/* 
 * This routine is called by the procfs daemon to find out the next
 * q entry it should service.
 */
STATIC qent_t *
get_procfs_req(void)
{
	int	s;
	qent_t	*ret;

	s = PROCQ_LOCK();

	/*
	 * Pick up a request in fifo order, take it off the q, and 
	 * return it to the daemon for servicing.
	 */
	ret = procfs_q;
	if (procfs_q == tail_q)
		procfs_q = tail_q = NULL;
	else
		procfs_q = procfs_q->forw;

	PROCQ_UNLOCK(s);
	return(ret);
}


/*
 * Entry point for the procfs remote prsuio servicing daemon thread.
 */
void
do_procio(void)
{
	int	newp = 0;
	qent_t	*request;
	sigvec_t *sigvp;

	/*
	 * Initialize the daemon.
	 */
	bcopy("procio", curprocp->p_psargs, 7);
	bcopy("procio", curprocp->p_comm, 6);
	_SAT_SET_COMM(curprocp->p_comm);
	curthreadp->k_cpuset = 1;

	/*
	 * Initialize globals used by this module.
	 */
	initnsema(&procfs_q_sema, 0, "proc_q_sema");
	spinlock_init(&procq_lock, "procq_lock");
	procfs_q = tail_q = NULL;

	/*
	 * Arrange for children to exit directly without signalling.
	 */
	VPROC_GET_SIGVEC(curvprocp, VSIGVEC_UPDATE, sigvp);
	sigvp->sv_flags |= SNOWAIT;
	VPROC_PUT_SIGVEC(curvprocp);

	/*
	 * Do for life of system.
	 */
	while (1) {
		/*
		 * Wait for a request to spawn a service thread.
		 */
		waitfor_procfs_req();
		newp = newproc();
		if (newp) {
			/*
			 * Initialize the new service thread.
			 */
			bcopy("prociocld", curprocp->p_psargs, 10);
			bcopy("prociocld", curprocp->p_comm, 9);
			_SAT_SET_COMM(curprocp->p_comm);
			curthreadp->k_cpuset = 1;

			/*
			 * Pick up the request to service, do it, respond
			 * and die.
			 */
			request = get_procfs_req();
			do_procfs_req(request);
			resp_procfs_req(request);
			exit(CLD_EXITED, 0);
		}
	}
}

STATIC void
do_procfs_req(qent_t *request)
{
	cred_t	*curcred = get_current_cred();		/* sys_cred */
	/*
	 * Assume identity of originating xthread.
	 */
	set_current_cred(request->xcred);
	request->retval = prusrio(request->targproc, request->targut, 
				request->rw, request->uiop, 1);
	/*
	 * Reacquire original identity.
	 */
	set_current_cred(curcred);
	return;
}
