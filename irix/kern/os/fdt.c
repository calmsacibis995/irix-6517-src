/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.60 $"

/*
 * Code for managing the per-process file descriptor table.
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/prctl.h>
#include <sys/proc.h>
#include <sys/kmem.h>
#include <ksys/vfile.h>
#include <sys/vnode.h>
#include <sys/atomic_ops.h>
#include <sys/poll.h>		/* needed by fdt_select_convert */
#include <ksys/fdt.h>
#include <ksys/fdt_private.h>
#include <limits.h>
#include <sys/vsocket.h>
#include "os/proc/pproc_private.h"

#define INIT_NFILES 16

#ifdef DEBUG
static int	num_fdts = 0;		/* number of extant fdts */
long fdt_expands;
long fdt_max_nofiles;

#define	FDT_DEBUG_INCR(x)	(x)++
#define	FDT_DEBUG_DECR(x)	(x)--

/*
 * In debug mode, do fd use accounting for all procs - this helps
 * test it since there are relatively few shd procs
 */
static int 	fdtest = 1;
int 		nshdwaiting;

#else /* DEBUG */
#define	FDT_DEBUG_INCR(x)
#define	FDT_DEBUG_DECR(x)
#endif

static struct zone *fdt_zone;

extern void fspe_free_cookie(void *);

/*
 * Internal routines.
 */
static fdt_t * 		fdt_copy(fdt_t *);
static void 		fdt_hold(fdt_t *, int);
static void 		fdt_scan_flag(fdt_t *, int);
static vfile_t *	fdt_reset(fdt_t *, int);
static int		ufadd(int, fdt_t *, vfile_t *, int *);

/*
 * Internal get routine.  Does no error checking on 'fd'.
 * Locking is the responsibility of the caller. We may or
 * may not need locking.
 *
 * Caller is guaranteed that the returned file struct's reference
 * count is > 0.
 */
static __inline vfile_t *
fdt_get(fdt_t *fdt, int fd)
{
	vfile_t 	*fp;

	ASSERT(fd >= 0);

	if ((fp = fdt->fd_flist[fd].uf_ofile) == NULL || 
					(fp->vf_flag & FINPROGRESS))
		return NULL;
	ASSERT_ALWAYS(fp->vf_count > 0);

	return fp;
}

/* 
 * Same as fdt_get(), but doesn't assert that the associated file
 * struct has a > 0 reference count.
 */
static __inline vfile_t *
fdt_get_noassert(fdt_t *fdt, int fd)
{
	vfile_t		*fp;

	ASSERT(fd >= 0);

	if ((fp = fdt->fd_flist[fd].uf_ofile) == NULL || 
					(fp->vf_flag & FINPROGRESS))
		return NULL;

	return fp;
}

/* fdt_fd_destroy(fdt, fd)
 * This routine is used to tear down the mutex lock and sync var
 * for each file descriptor. This happens when we 1) destroy the whole
 * fdt; 2) need to change the size of fdt.
 */
void 
fdt_fd_destroy(fdt_t *fdt, int fd)
{
        FDT_FD_LOCK(fdt, fd);
	if (fdt->fd_flist[fd].uf_waiting) {
	        FDT_WAKE(fdt, fd); 
		fdt->fd_flist[fd].uf_waiting = 0;
	}
	sv_destroy(&fdt->fd_syncs[fd].fd_fwait);
	FDT_FD_UNLOCK(fdt, (fd));
	mutex_destroy(&fdt->fd_syncs[fd].fd_mutex);
}

/*
 * in use list is a per uthread list of fds (e.g. ints), based at 1 such that 
 * a value of 0 means no fd present.
 *
 * fdt_inuse gets called for *shared fdt* sprocs to take a hold on an open
 * file.  One syscall can cause holds on more than one file.  The first
 * open file held is remembered in ut_fdinuse.  Subsequent (second and beyond)
 * open files holds are remembered in ut_fdmany - which is dynamically allocated
 * (or expanded) only when required - e.g. when multiple open files are held
 * by a single syscall.  ut_fdmanymax is the current size of ut_fdmany, and
 * ut_fdmanysz is the current number in use in ut_fdmany.
 * 
 * Or put another way:
 *         ut_fdmany is a dynamic array: ut_fdmany[ut_fdmanymax]
 *         ut_fdmany[0..ut_fdmanysz-1] are in use
 * 
 * fdt_release is called at the end of syscall processing (only if there are
 * open file holds outstanding) to release all the holds.  All of this is
 * done to synchronize close syscalls with all other open file syscalls
 * when done from an sproc - to keep from closing a file that has a read
 * in progress.
 *
 */
static void
fdt_inuse(
	int		fd,
	fdt_t		*fdt)
{
	uthread_t	*ut = curuthread;
	cnt_t 		*fdcnt = &fdt->fd_flist[fd].uf_inuse;
#if DEBUG
	proc_t		*p = UT_TO_PROC(curuthread);

	if (ISSHDFD(&p->p_proxy)) {
		ASSERT(mutex_mine(&fdt->fd_syncs[fd].fd_mutex));
		ASSERT(ismrlocked(&fdt->fd_lock, MR_UPDATE | MR_ACCESS));
	}
#endif

	(*fdcnt)++;

	if (ut->ut_fdinuse) {
		/*
		 * already have one, start a list -- klunko-rama
		 */
		if (ut->ut_fdmanysz >= ut->ut_fdmanymax) {
			/* allocate a chunk */
			ut->ut_fdmanymax += 16;
			ut->ut_fdmany = (int *) kmem_realloc(ut->ut_fdmany,
				       ut->ut_fdmanymax * sizeof(int), 
				       KM_SLEEP);
		}
		ut->ut_fdmany[ut->ut_fdmanysz++] = fd + 1;
	} else
		ut->ut_fdinuse = fd + 1;
}

/*
 * fdt_release_one is called to release a hold (from fdt_inuse) on a single
 * fd.  The list of held fd's may need to be compressed, as the release can
 * be applied to any fd in the list.  fdt_release_one is called when a syscall
 * that has taken a reference needs to back out - usually due to an error.
 */
static int
fdt_release_one(
	int		fd,
	fdt_t		*fdt)
{
	int 		i, todo;
	uthread_t	*ut = curuthread;
	proc_t		*p = UT_TO_PROC(ut);
	int		found = 0;
	cnt_t *		fdcnt = &fdt->fd_flist[fd].uf_inuse;
	int             isshd = ISSHDFD(&p->p_proxy);

#ifdef DEBUG
	if (isshd) {
		ASSERT(mutex_mine(&fdt->fd_syncs[fd].fd_mutex));
		ASSERT(ismrlocked(&fdt->fd_lock, MR_UPDATE | MR_ACCESS));
	}
#endif
	
	/* Adjust fd to be based at 1 */
	fd++;

	if (ut->ut_fdinuse == fd) {
		found = 1;
		(*fdcnt)--;
		if (todo = ut->ut_fdmanysz) {
			ut->ut_fdinuse = ut->ut_fdmany[todo - 1];
			ASSERT(ut->ut_fdinuse);
			ut->ut_fdmanysz--;
		} else {
			ut->ut_fdinuse = 0;
		}
	} else if (todo = ut->ut_fdmanysz) {
		for (i = 0; i < todo; i++) {
			if (fd == ut->ut_fdmany[i]) {
				found = 1;
				(*fdcnt)--;
				ut->ut_fdmany[i] = ut->ut_fdmany[todo - 1];
				ut->ut_fdmanysz--;
				break;
			}
		}
	}

	if (!found)
		return(0);

	/*
	 * Since we have the fdt lock for READ, noone can add themselves
	 * to the list.  Others could be in here with us, but the worst
	 * that can happen is that sv_broadcast gets called gratuitously.
	 */
	if (isshd) {
      	        /* put fd back to its real value */
	        fd--;
		if (fdt->fd_flist[fd].uf_waiting) {
#ifdef DEBUG
			nshdwaiting++;
#endif
			fdt->fd_flist[fd].uf_waiting = 0;
			FDT_WAKE(fdt, fd);
		}
	}
	return(1);
}

/*
 * Initialize file descriptor table module.
 *
 * Also, allocate a fdt for proc0.  It's needed by fuser, as
 * well as being inherited by all subsequent processes.
 */
void
fdt_init(void)
{
	fdt_zone = kmem_zone_init(sizeof(struct fdt), "fdt");
}

fdt_t *
fdt0_init(void)
{
	fdt_t		*new_fdt;
	int fd;

	ASSERT(fdt_zone);

	/*
         * Allocate/initialize proc0 fdt.
	 */
	new_fdt = kmem_zone_zalloc(fdt_zone, KM_SLEEP); /* zero'd */
	FDT_DEBUG_INCR(num_fdts);

	/*
	 * Allocate INIT_NFILES open files.
	 */
	new_fdt->fd_flist = kmem_zalloc(INIT_NFILES * sizeof(struct ufchunk),
		KM_SLEEP);
	new_fdt->fd_nofiles = INIT_NFILES;
	FDT_INIT_LOCK(new_fdt);
	new_fdt->fd_syncs = kmem_alloc(INIT_NFILES * sizeof(fdt_syncs_t), 
				       KM_SLEEP);
	for (fd = 0; fd < INIT_NFILES; fd++) {
	        FDT_FD_INIT(new_fdt, fd, "proc0 syncs");
	}
	
	return new_fdt;
}

/*
 * Scan the file descriptor table for entries with 'flag' set in
 * the uf_pofile field, and perform the required processing.
 * Used in support of close-on-exec (FCLOSEXEC) flag.
 *
 * An assumption is that no other routines may be modifying the
 * fdt while this routine is executing.
 */
static void
fdt_scan_flag(fdt_t *fdt, int flag)
{
	vfile_t		*fp;
	struct ufchunk	*ufp;
	int		fd;

	ufp = fdt->fd_flist;
	for (fd = 0; fd < fdt->fd_nofiles; fd++) {
		fp = ufp[fd].uf_ofile;
		if (fp != NULL && !(fp->vf_flag & FINPROGRESS)) {
			ASSERT(fp->vf_count > 0);
			if (ufp[fd].uf_pofile & flag) {
				/*
				 * Protect against fuser callers.
				 */
				FDT_WRITE_LOCK(fdt);
				ASSERT(ufp[fd].uf_inuse == 0);
				ASSERT(flag == FCLOSEXEC);
				ufp[fd].uf_ofile = NULL;
				vfile_close(fp);
				FDT_UNLOCK(fdt);
			}
		}
	}
}

/*
 * Allocate a file descriptor table, initializing its contents with
 * that from the calling process.
 *
 * Locking is the responsibility of the caller.  
 * Note that this routine does _not_ take refs on file structures.
 *
 * WARNING - this does bit for bit copy of fd_flist, meaning
 * it copies inuse bits, and descriptors whose file structs
 * aren't finished being set up (FINPROGRESS).
 */
static fdt_t *
fdt_copy(fdt_t *fdt)
{
	fdt_t		*new_fdt;
	int fd;

	/*
	 * Allocate fdt, copy contents from source.
	 */
	new_fdt = kmem_zone_alloc(fdt_zone, KM_SLEEP);	/* not zero'd */
	FDT_DEBUG_INCR(num_fdts);

	new_fdt->fd_nofiles = fdt->fd_nofiles;
	ASSERT(new_fdt->fd_nofiles > 0);
	new_fdt->fd_flist = kmem_alloc(new_fdt->fd_nofiles * 
					sizeof(struct ufchunk), KM_SLEEP);
	FDT_INIT_LOCK(new_fdt);
	bcopy(fdt->fd_flist, new_fdt->fd_flist, 
		new_fdt->fd_nofiles * sizeof(struct ufchunk));

	new_fdt->fd_syncs = kmem_alloc(new_fdt->fd_nofiles *
				       sizeof(fdt_syncs_t), KM_SLEEP);
	for (fd = 0; fd < new_fdt->fd_nofiles; fd++) {
	        FDT_FD_INIT(new_fdt, fd, "fdt_copy syncs");
	}

        return new_fdt;
}

/*
 * fdt_create - create a new, empty fdt structure
 */

fdt_t *
fdt_create(
	int	nofiles)
{
	fdt_t		*fdt;
	int fd;

	fdt = kmem_zone_zalloc(fdt_zone, KM_SLEEP);	/* zero'd */
	FDT_DEBUG_INCR(num_fdts);

	fdt->fd_nofiles = nofiles;
	fdt->fd_flist = kmem_zalloc(fdt->fd_nofiles * sizeof(struct ufchunk),
		KM_SLEEP);
	fdt->fd_syncs = kmem_alloc(fdt->fd_nofiles * sizeof(fdt_syncs_t),
				   KM_SLEEP);
	for (fd = 0; fd < fdt->fd_nofiles; fd++) {
		FDT_FD_INIT(fdt, fd, "fdt_create syncs");
	}

	return(fdt);
}

/*
 * fdt_hold - go through all fds and hold them.  
 *
 * - Resets any descriptors for FINPROGRESS file structures.
 *   These could be here when called by an sproc.
 * - Reset inuse descriptors.
 * - Handle FD_NODUP_FORK flag arg by not dup'ing such descriptors.
 *
 * Locking is the responsibility of the caller.  
 */
static void
fdt_hold(fdt_t *fdt, int flag)
{
	struct ufchunk	*ufp;
	vfile_t		*fp;
	int 		fd;
	
	ufp = fdt->fd_flist;
	for (fd = 0; fd < fdt->fd_nofiles; fd++) {
		if ((fp = ufp[fd].uf_ofile) == NULL) {
			ASSERT(ufp[fd].uf_inuse == 0);
			continue;
		}
		if (fp->vf_flag & FINPROGRESS) {
			ufp[fd].uf_ofile = NULL;
		} else {
			ASSERT(fp->vf_count > 0);
			if ((ufp[fd].uf_pofile & FD_NODUP_FORK) &&
			    (flag == FD_NODUP_FORK)) {
				ufp[fd].uf_ofile = NULL;
		        } else {
				VFILE_REF_HOLD(fp);
			}
		}
		ufp[fd].uf_inuse = 0;
		ufp[fd].uf_waiting = 0;
	}
}

/*
 * Fork a file descriptor table for a new process.  The fdt is for
 * either a regular process or an sproc that's not sharing fd's with
 * its parent.
 *
 * Note that this routine handles the case of proc0 forking proc1.
 */
fdt_t *
fdt_fork()
{
	proc_t		*p = curprocp;
	fdt_t		*fdt = p->p_fdt;
	fdt_t		*new_fdt;
	int		isshrd;

	ASSERT(fdt != NULL);

	/*
	 * First copy the fdt bit for bit, then hold extra file struct
	 * refs for the new copy.  Note that:
	 * - fdt_copy doesn't copy the inuse state associated with fd's
	 *   that are inuse due to a syscall-in-progress
	 * - fdt_hold clears out any fd's (in the copy) that were in the
	 *   process of being opened, and deals with the FD_NODUP_FORK flag.
	 *
	 * Do the copy+hold atomically (only applies to sprocs/sthreads).
	 */
	if (isshrd = ISSHDFD(&p->p_proxy))
		FDT_WRITE_LOCK(fdt);

	new_fdt = fdt_copy(fdt);
	fdt_hold(new_fdt, FD_NODUP_FORK);

	if (isshrd)
		FDT_UNLOCK(fdt);

	return new_fdt;
}

/*
 * Undo effects of fdt_fork().  Guaranteed that only the caller
 * has access to the fdt.
 *
 * Note that when decrementing file struct counts, VFILE_REF_RELEASE is used
 * instead of vfile_close to avoid wiping out parent's file locks.
 */
void
fdt_forkfree(fdt_t *fdt)
{
	vfile_t		*fp;
	int 		i;

	/*
	 * First undo effects of fdt_hold, then undo effects of fdt_copy.
	 *
	 * Note: no lock on the fdt is needed because no other process
	 * has access to it (because it hasn't been installed).
	 */
	for (i = 0; i < fdt->fd_nofiles; i++) {
		fp = fdt_get(fdt, i);
		if (fp != NULL) {
			/*
			 * Unlikely scenario: another sproc member closed
			 * the file after they were duplicated in a failed
			 * fork attempt.  If count is down to just one,
			 * close happened behind our backs and we need to
			 * do full last-close processing.
			 */
			if (fp->vf_count > 1) {
				VFILE_REF_RELEASE(fp);
			} else
				vfile_close(fp);
		}
	}

	fdt_free(fdt);
}

/*
 * Free a file descriptor table and its ufchunk.
 */
void
fdt_free(fdt_t *fdt)
{

        int fd;
	
	for (fd = 0; fd < fdt->fd_nofiles; fd++) {
	        fdt_fd_destroy(fdt, fd);
	}

        kmem_free(fdt->fd_syncs, fdt->fd_nofiles * sizeof(fdt_syncs_t));
	kmem_free(fdt->fd_flist, fdt->fd_nofiles * sizeof(struct ufchunk));

	FDT_FREE_LOCK(fdt);
	kmem_zone_free(fdt_zone, fdt);
	FDT_DEBUG_DECR(num_fdts);
}

/*
 * Close-on-exec processing, plus try to shrink the fd_flist.
 * This is a noop for fd-sharing sprocs.
 */
void
fdt_exec(void)
{
	fdt_t		*fdt = curprocp->p_fdt;
	int		hifd;

	ASSERT(fdt);

	/*
	 * Close-on-exec processing.
	 */
	fdt_scan_flag(fdt, FCLOSEXEC);

	/*
	 * Shrink fdt
	 */
	hifd = fdt_gethi();
	if (hifd < fdt->fd_nofiles) {
	        int fd;
		FDT_WRITE_LOCK(fdt); /* for fuser */
		fdt->fd_flist = (struct ufchunk *)kmem_realloc(fdt->fd_flist,
			hifd * sizeof(struct ufchunk), KM_SLEEP);
		for (fd = 0; fd < fdt->fd_nofiles; fd++) {
		        fdt_fd_destroy(fdt, fd);
		}
	        kmem_free(fdt->fd_syncs, fdt->fd_nofiles * sizeof(fdt_syncs_t));
		fdt->fd_syncs = kmem_alloc(hifd * sizeof(fdt_syncs_t), 
					   KM_SLEEP);
		for (fd = 0 ; fd < hifd; fd++) {
		        FDT_FD_INIT(fdt, fd, "fdt_exec syncs");
		}
		fdt->fd_nofiles = hifd;
		FDT_UNLOCK(fdt);
	}

}	

/*
 * Close all files and free file descriptor table.
 * 
 * Note that the caller guarantees that 3rd party table scanners
 * (fdt_fuser) can't be racing with fdt_exit.
 */
void
fdt_exit(void)
{
	vfile_t		*fp;
	int		i;
	uthread_t	*ut = curuthread;
	proc_t		*p = UT_TO_PROC(ut);
	fdt_t		*fdt = p->p_fdt;

	ASSERT(fdt);

	if (ut->ut_fdmany != NULL) {
		kmem_free(ut->ut_fdmany, ut->ut_fdmanymax * sizeof(int));
		ut->ut_fdmany = NULL;
		ut->ut_fdmanysz = 0;
	}

	if (IS_SPROC(ut->ut_pproxy) && ISSHDFD(ut->ut_pproxy)) {
		p->p_fdt = NULL;
		return;
	}

	/*
	 * Note that we're playing a little fast and loose here
	 * by not zero'ing out the uf_ofile pointer in the fdt
	 * for a fp that we call closef on.  This is fast, and
	 * in general is not a problem because noone else has
	 * access to the fdt at this point.  
	 *
	 * However, closef can cause the file struct's reference count 
	 * to go to zero, and then call fdt_vnode_isopen().  Hence, that 
	 * routine uses fdt_get_noassert(), which is tolerant of fd's 
	 * that refer to file structs with zero reference counts.
	 */
	if (fdt != NULL) {
		for (i = 0; i < fdt->fd_nofiles; i++) {
			fp = fdt_get(fdt, i);
			if (fp != NULL)
				vfile_close(fp);
		}

		/* Free file descriptor table */
		fdt_free(fdt);
		p->p_fdt = NULL;
	}
}

/*
 * Close ALL fds - no marking is done no nothing.
 * Often used for kernel deamons.
 */
void
fdt_closeall(void)
{
	vfile_t		*fp;
	int 		i, nofiles;

	nofiles = curprocp->p_fdt->fd_nofiles;

	for (i = 0; i < nofiles; i++)
		if (closefd(i, &fp) == 0)
			vfile_close(fp);
}

/*
 * Return maximum allocated fd for current process.
 * Note that this isn't the maximum valid nor the maximum permissible
 * fd - simply the 'not to exceed' fd.
 */
int
fdt_nofiles(void)
{
	ASSERT(curprocp);
	ASSERT(curprocp->p_fdt);

	return curprocp->p_fdt->fd_nofiles;
}

/*
 * Return highest valid fd.
 *
 * Actually we return 1 larger so that the return value semantic of
 * getdtablehi is the same as getdtablesize (i.e. one should decrement
 * before using). This makes it easy to translate from one call to the other.
 */
int
fdt_gethi(void)
{
	proc_t 		*p = curprocp;
	struct ufchunk 	*ufp;
	vfile_t		*fp;
	fdt_t		*fdt = p->p_fdt;
	int 		i, hifd;

	ASSERT(fdt);
	FDT_READ_LOCK(fdt);

	/* search backwards */
	hifd = 1;
	ufp = fdt->fd_flist;
	for (i = fdt->fd_nofiles-1; i >= 0; i--) {
	        FDT_FD_LOCK(fdt, i);
		if ((fp = ufp[i].uf_ofile) != NULL &&
		    !(fp->vf_flag & FINPROGRESS)) {
			hifd = i + 1;
			FDT_FD_UNLOCK(fdt, i);
			break;
		}
		FDT_FD_UNLOCK(fdt, i);
	}

	FDT_UNLOCK(fdt);
	return hifd;
}

/*
 * Allocating shared area - ``move'' fds from upage to share.
 * Called at fork time.  
 * 
 * Note: can't get shaddr pointer from proc structure because
 * it's not setup yet.
 */
void
fdt_share_alloc(shaddr_t *sa) 
{
	ASSERT(curuthread->ut_fdinuse == 0);
	sa->s_fdt = curprocp->p_fdt;
}

/*
 * Deallocating shared area at either exec or exit time.
 * Guaranteed that no other sproc is accessing the shared area.
 *
 * The flag arg indicates whether deallocating on behalf of
 * exec or exit processing:
 * SHDEXEC => exec
 * SHDEXIT => exit
 */
void
fdt_share_dealloc(proc_t *p, int flag) 
{
	shaddr_t 	*sa = p->p_shaddr;
	fdt_t		*fdt;
	vfile_t		*fp;
	int		i;

	ASSERT(ISSHDFD(&p->p_proxy));

	if (flag == SHDEXEC) {		
		/*
		 * exec processing - move fds from share to proc.
		 */
		ASSERT(p->p_fdt == sa->s_fdt);
		sa->s_fdt = NULL;
	} else {			
		/*
		 * exit processing - close fds and free file descriptor table.
		 */
		fdt = sa->s_fdt;
		FDT_READ_LOCK(fdt);
		for (i = 0; i < fdt->fd_nofiles; i++) {
		        FDT_FD_LOCK(fdt, i);
			fp = fdt_get(fdt, i);
			if (fp != NULL)
				vfile_close(fp);
			FDT_FD_UNLOCK(fdt, i);
		}
		FDT_UNLOCK(fdt);
		fdt_free(fdt);
		sa->s_fdt = NULL;
	}
}

/*
 * Detaching shared area at exec or exit time.
 *
 * The flag arg indicates whether detaching on behalf of
 * exec or exit processing:
 * SHDEXEC => exec
 * SHDEXIT => exit
 *
 * The 'last' flag indicates whether this is the last
 * sproc that is sharing file descriptors.  The caller must
 * guarantee that the notion of 'last' doesn't change while
 * this routine is being executed.
 */
void
fdt_share_detach(proc_t *p, int flag, int last) 
{
	fdt_t		*fdt, *new_fdt;
	shaddr_t	*sa;
	vfile_t		*fp;
	int		i;

	sa = p->p_shaddr;
	fdt = sa->s_fdt;

	if (flag == SHDEXEC) {		
		/*
		 * exec - copy fds from share to upage, taking
		 * additional file struct references.
		 *
		 * As in fdt_fork(), first copy the fdt bit for bit, 
		 * then hold extra file struct refs for the new copy.  
		 * Note that:
		 * - fdt_copy doesn't copy the inuse state associated with 
		 *   fd's that are inuse due to a syscall-in-progress
		 * - fdt_hold clears out any fd's (in the copy) that were 
		 *   in the process of being opened
		 *
		 * Do the copy+hold atomically.
		 */
		FDT_WRITE_LOCK(fdt);
		new_fdt = fdt_copy(fdt);
		fdt_hold(new_fdt, 0);
		FDT_UNLOCK(fdt);
		
		p->p_fdt = new_fdt;
	} else {
		/*
		 * exit - do close processing on open files, including 
		 * releasing file locks.  (Each sproc can have its own 
		 * file locks.)  If we're not the last member sharing fds, 
		 * HOLD each fp first so that the files remain open.
		 */
		FDT_READ_LOCK(fdt);
		for (i = 0; i < fdt->fd_nofiles; i++) {
		        FDT_FD_LOCK(fdt, i);
			fp = fdt_get(fdt, i);
			if (fp != NULL) {
				if (!last)
					VFILE_REF_HOLD(fp);
				vfile_close(fp);
			}
			FDT_FD_UNLOCK(fdt, i);
		}
		FDT_UNLOCK(fdt);
	
		/*
		 * Note that if we're last we've closed the fd's but 
		 * the sa_fdt still points to 'fd's' - thats ok - 
		 * If no other member is sharing fds then NO new members 
		 * can share fd's since the share mask is strictly limited.  
		 * So, free up the memory here.
		 */
		if (last) {
			fdt_free(fdt);
			sa->s_fdt = NULL;
		}
	}
}

/*
 * Essentially the same as fdt_get except it will return a vfile
 * that still has the FINPROGRESS flag set, meaning vfile_ready()
 * hasn't been called on it.  Exported to idbg.
 *
 * Don't do asserts here, since it's called by idbg.
 */
vfile_t *
fdt_get_idbg(fdt_t *fdt, int fd)
{

	return (fdt->fd_flist[fd].uf_ofile);

}

static vfile_t *
fdt_swapfast(fdt_t *fdt, int fd, vfile_t *swapfp)
{
	vfile_t		*fp;

	ASSERT(fd >= 0);

	if ((fp = fdt->fd_flist[fd].uf_ofile) == NULL || 
					(fp->vf_flag & FINPROGRESS))
		return NULL;
	ASSERT(fp->vf_count > 0);

	fdt->fd_flist[fd].uf_ofile = swapfp;

	return fp;
}
/* 
 * Internal routine to extract the file struct pointer of a given fd,
 * and to reset the fd to NULL.  Return file struct pointer to caller.
 */
static vfile_t *
fdt_reset(fdt_t *fdt, int fd)
{
	vfile_t		*fp;

	ASSERT(fd >= 0);
	FDT_READ_LOCK(fdt);
	FDT_FD_LOCK(fdt, fd);
	fdt_release_one(fd, fdt);
	ASSERT(fdt->fd_flist[fd].uf_inuse == 0);

	fp = fdt->fd_flist[fd].uf_ofile;
	ASSERT(fp != NULL);
	fdt->fd_flist[fd].uf_ofile = NULL;
	FDT_FD_UNLOCK(fdt, fd);
	FDT_UNLOCK(fdt);
	return fp;
}

/*
 * Allocate a file descriptor and set its file pointer.
 * Upon success, return the fd in fdp. 
 */
int
fdt_alloc(vfile_t *fp, int *fdp)
{
	fdt_t		*fdt = UT_TO_PROC(curuthread)->p_fdt;
	int 		error;

	ASSERT(fdt != NULL);

	/*
	 * Note that the fd is allocated with its flags zero'd.
	 * Write lock fdt because it's changing.
	 */

	error = ufadd(0, fdt, fp, fdp);

	return error;
}

/*
 * Atomically allocate a set of file descriptors and set their file pointers.
 * Upon success, return the array of fd's.
 */
int
fdt_alloc_many(int numfps, vfile_t **fparray, int *fdarray)
{
	fdt_t		*fdt = UT_TO_PROC(curuthread)->p_fdt;
	int 		i, j, error;

	ASSERT(numfps > 0);
	ASSERT(fdt != NULL);

	/*
	 * Note that each fd is allocated with its flags zero'd.
	 * Write lock fdt because it's changing.
	 */
	for (i = 0; i < numfps; i++) {
		error = ufadd(0, fdt, fparray[i], &fdarray[i]);
		if (error) {
			for (j = 0; j < i; j++)
				(void) fdt_reset(fdt, fdarray[j]);
			break;
		}
	}

	return error;
}

/*
 * Undo the effects of fdt_alloc.  Return fp from the table to the caller.
 */
vfile_t *
fdt_alloc_undo(int fd)
{
	fdt_t		*fdt = UT_TO_PROC(curuthread)->p_fdt;
	vfile_t		*fp;

	ASSERT(fdt != NULL);

	/*
	 * Write lock fdt because it's changing, and to synchronize
	 * with fdt_fork who is duplicating the table.
	 */
	fp = fdt_reset(fdt, fd);
	
	ASSERT(fp != NULL);
	return fp;
}

/*
 * ufadd(start, fdt, fdp)
 *
 * Allocate an entry in file-descriptor chain (reuse existing entry 
 * if possible) - such that -
 * 1. allocted descriptor number is no less than start 
 * 2. allocted descriptor number is lowest available
 * RETURN:
 *	EINVAL, EMFILE on error;
 *	0 if successful, and
 *	   *fdp contains file descriptor number.
 *	   *nofilesp is updated to count-in any newly malloc'ed descriptors
 *		(INCLUDING unused slots).
 */

static int
ufadd(int start, fdt_t *fdt, vfile_t *fp, int *fdp)
{
	struct ufchunk 		*ufp;
	int 			i, n, nofiles;
	rlim_t 			openmax;
	struct rlimit		rlim;
	int			isshd = 
				  ISSHDFD(&UT_TO_PROC(curuthread)->p_proxy);

	i = start;
	FDT_READ_LOCK(fdt);
	nofiles = fdt->fd_nofiles;

	VPROC_GETRLIMIT(curvprocp, RLIMIT_NOFILE, &rlim);
	if (rlim.rlim_cur >= RLIM32_INFINITY)
		openmax = RLIM32_INFINITY;
	else
		openmax = rlim.rlim_cur;

	/*
	** sanity check: start has to be in the range
	** 0 .. (p_rlimit[RLIMIT_NOFILE].rlim_cur-1)
	** (equivalent to 0..(OPEN_MAX-1) in user-land);
	**/
	if ((i < 0) || ((rlim_t)i >= openmax)) {
	        FDT_UNLOCK(fdt);
		return EINVAL;
	}

expand:
	if (i >= nofiles) {
		char *op;
		int tmpfd;
		/*
		 * We need to expand fd_flist to hold more open files.
		 * Use kmem_realloc, which may relocate fd_flist.
		 * uf_inuse list is relative (contains fd number) and so
		 * doesn't care about relocation.
		 */
		if ((rlim_t)nofiles >= openmax) {
		        FDT_UNLOCK(fdt);
			return EMFILE;
		}

		FDT_UNLOCK(fdt);
		FDT_WRITE_LOCK(fdt);

		/* Check to see if the list hasn't grown
		 */
		nofiles = fdt->fd_nofiles;
		if (i < nofiles) {
			goto afterexpand;
		}

		while (fdt->fd_nofiles <= i)
			fdt->fd_nofiles *= 2;
		fdt->fd_flist = (struct ufchunk *)kmem_realloc(fdt->fd_flist,
			fdt->fd_nofiles * sizeof(struct ufchunk), KM_SLEEP);
		FDT_DEBUG_INCR(fdt_expands);
#if DEBUG
		if (fdt->fd_nofiles > fdt_max_nofiles)
			fdt_max_nofiles = fdt->fd_nofiles;
#endif
		op = (char *)fdt->fd_flist + 
			(nofiles * sizeof(struct ufchunk));
		bzero(op, ((fdt->fd_nofiles - nofiles) * 
			sizeof(struct ufchunk)));

		/* The idea here is that because all these mutex locks
		 * are protected by fdt read lock, we are here only
		 * when no one is holding the mutex so we can freely
		 * destroy it.
		 */
		for (tmpfd = 0; tmpfd < nofiles; tmpfd++)
		        fdt_fd_destroy(fdt, tmpfd);
		        
		kmem_free(fdt->fd_syncs, nofiles * sizeof(fdt_syncs_t));
		fdt->fd_syncs = kmem_alloc(fdt->fd_nofiles * 
					   sizeof(fdt_syncs_t), KM_SLEEP);
		for (tmpfd = 0; tmpfd < fdt->fd_nofiles; tmpfd++) {
			FDT_FD_INIT(fdt, tmpfd, "ufadd syncs");
		}
		nofiles = fdt->fd_nofiles;
	}

	/*
	 * Now look for an unused entry
	 */
 afterexpand:
	if (openmax == RLIM32_INFINITY) {
		/*
		 * Even with no limit, we can't overflow
		 * our counters.
		 */
		n = INT_MAX;
	} else {
		ASSERT(openmax < (rlim_t)INT_MAX);
		n = (int)openmax;
	}
	ufp = fdt->fd_flist;
	for (; i < fdt->fd_nofiles; i++) {
	        if (i >= n) {
		        FDT_UNLOCK(fdt);
			return EMFILE;
		}
		if (ufp[i].uf_ofile == NULL) {
		        if (isshd) {
			        FDT_FD_LOCK(fdt, i);
				if (ufp[i].uf_ofile == NULL) {
				        ASSERT(ufp[i].uf_inuse == 0);
					ufp[i].uf_pofile = 0;
					ufp[i].uf_ofile = fp;
					*fdp = i;
					fdt_inuse (i, fdt);
					FDT_FD_UNLOCK(fdt, i);
					FDT_UNLOCK(fdt);
					return 0;
				}
				FDT_FD_UNLOCK(fdt, i);
			} else {
			  	ASSERT(ufp[i].uf_inuse == 0);
			  	ufp[i].uf_pofile = 0;
			  	ufp[i].uf_ofile = fp;
			  	*fdp = i;
			  	FDT_UNLOCK(fdt);
			  	return 0;
			}
		}
	}
	/*
	 * No empty fd found - expand.
	 */
	ASSERT(i == fdt->fd_nofiles);
	goto expand;
	
}

/*
 * Convert a user supplied file descriptor into a pointer to a file
 * structure.  Only task is to check range of the descriptor (soft
 * resource limit was enforced at open time and shouldn't be checked
 * here).
 */
int
getf(int fd, struct vfile **fpp)
{
	uthread_t	*ut = curuthread;
	proc_t		*p;
	fdt_t		*fdt;

	ASSERT(ut);
	p = UT_TO_PROC(ut);
	ASSERT(p);
	fdt = p->p_fdt;
	ASSERT(fdt);

#ifdef DEBUG
	if (!ISSHDFD(&p->p_proxy) && !fdtest)
#else
	if (!ISSHDFD(&p->p_proxy))
#endif
	{
		/* fast path for regular processes */
		if (fd < 0 || fd >= fdt->fd_nofiles ||
		    (*fpp = fdt_get(fdt, fd)) == NULL)
			return EBADF;	
		return 0;
	}

	FDT_READ_LOCK(fdt);
	if (fd < 0 || fd >= fdt->fd_nofiles) {
		FDT_UNLOCK(fdt);
		return EBADF;
	}

	FDT_FD_LOCK(fdt, fd);
	if ((*fpp = fdt_get(fdt, fd)) == NULL) {
	        FDT_FD_UNLOCK(fdt, fd);
		FDT_UNLOCK(fdt);
		return EBADF;
	}

	/*
	 * Mark the file descriptor as in-use to synchronize
	 * with close.
	 */
	fdt_inuse (fd, fdt);

	FDT_FD_UNLOCK(fdt, fd);
	FDT_UNLOCK(fdt);
	return 0;
}

/*
 * closefd - fd close protocol - waits if any threads that are
 * sharing file descriptors have the fd inuse.
 *
 * We hold the fdt read lock as well as the per fd mutex lock
 * across the entire get/inuse check to single
 * thread closes by different members of the same share group.
 * We hold the fdt lock for write to keep getf'ers at bay.
 *
 * Note that the only time this needs to be called rather than just
 * setf() is if the fd was ever completely setup (i.e. had an fp set).
 * This happens for example if one has called vfile_alloc & vfile_ready but then
 * needs to bail out..
 *
 * Note thats it's possible that closefd will close the 'wrong' fd - say
 * a thread has vfile_alloced & vfile_ready'd a new fd, but is still in the kernel
 * in the call that did the alloc. Another thread comes along and blindly
 * closes everything - closing the newly opened (but not finished) fd.
 * Then the original thread wakes up and re-validates the fd - could be
 * invalid or could have been re-alloced... I view this as a bug in the
 * app - until the alloc'ing thread returns, no other thread has any
 * business scanning fds.
 */
int
closefd(int fd, vfile_t **fpp)
{
	proc_t 		*p = curprocp;
	struct ufchunk 	*ufp;
	fdt_t		*fdt = p->p_fdt;
	int 		isshd;

	if (isshd = ISSHDFD(&p->p_proxy)) {
restart:
		FDT_READ_LOCK(fdt);
	}
restart2:

	if (fd < 0 || fd >= fdt->fd_nofiles) {
	        if (isshd)
		        FDT_UNLOCK(fdt);
		return EBADF;
	}

	ufp = fdt->fd_flist;

	if (isshd) {
	        FDT_FD_LOCK(fdt, fd);
		if ((*fpp = fdt_get(fdt, fd)) == NULL) {
		        FDT_FD_UNLOCK(fdt, fd);
			FDT_UNLOCK(fdt);
			return EBADF;
		}
		if (ufp[fd].uf_inuse > 0) {
			/*
			 * It is possible for the current process to be holding
			 * the uf_inuse reference - that happens if closefd is
			 * called during a syscall that created the file.
			 */
			if (curuthread->ut_fdinuse) {
				if (fdt_release_one (fd, fdt)) {
				        FDT_FD_UNLOCK(fdt, fd);
					goto restart2;
				}
			}

			ufp[fd].uf_waiting++;
			ASSERT(ufp[fd].uf_waiting > 0);
			
			/* unlock fdt read access because we don't want
			 * to hold it while we are waiting on a sync
			 * variable.
			 */
			FDT_UNLOCK(fdt);
			FDT_WAIT(fdt, fd);

			/*
			 * start all over - since we released the fdt lock
			 * another thread could have come in and closed the fd
			 */
			goto restart;
		}
		ufp[fd].uf_ofile = NULL;
		FDT_FD_UNLOCK(fdt, fd);
		FDT_UNLOCK(fdt);
	} else {
		/*
		 * Protect against fuser callers.
		 */
	        if ((*fpp = fdt_get(fdt, fd)) == NULL) 
		        return EBADF;
		FDT_WRITE_LOCK(fdt);
		ASSERT(ufp[fd].uf_inuse == 0);
		ufp[fd].uf_ofile = NULL;
		FDT_UNLOCK(fdt);
	}
	return 0;
}

/*
 * Release 'use of' one or more file descriptors.
 * This is typically called at the end of syscall processing.
 */
void
fdt_release(void)
{
	cnt_t *		fdcnt;
	int 		i, isshd, todo, fd;
	uthread_t	*ut = curuthread;
	proc_t		*p = UT_TO_PROC(ut);
	fdt_t		*fdt = p->p_fdt;

	if ((fd = ut->ut_fdinuse) == 0) {
		ASSERT_ALWAYS(ut->ut_fdmanysz == 0);
		return;
	}
	/* in use list based at 1 */
	fd--;

	if (isshd = ISSHDFD(&p->p_proxy)) {
		FDT_READ_LOCK(fdt);
	}
	FDT_FD_LOCK(fdt, fd);
	fdcnt = &fdt->fd_flist[fd].uf_inuse;
	
	ASSERT_ALWAYS(*fdcnt > 0);
	(*fdcnt)--;
	if (fdt->fd_flist[fd].uf_waiting) {
#ifdef DEBUG
	        nshdwaiting++;
#endif
		fdt->fd_flist[fd].uf_waiting = 0;
		FDT_FD_UNLOCK(fdt, fd);
		FDT_WAKE(fdt, fd);
	} else
	        FDT_FD_UNLOCK(fdt, fd);
	ut->ut_fdinuse = 0;

	if (todo = ut->ut_fdmanysz) {
		for (i = 0; i < todo; i++) {
			/* in use list based at 1 */
			fd = ut->ut_fdmany[i] - 1;
			FDT_FD_LOCK(fdt, fd);
			fdcnt = &fdt->fd_flist[fd].uf_inuse;
			ASSERT_ALWAYS(*fdcnt > 0);
			(*fdcnt)--;
			if (fdt->fd_flist[fd].uf_waiting) {
#ifdef DEBUG
			        nshdwaiting++;
#endif
				fdt->fd_flist[fd].uf_waiting = 0;
				FDT_FD_UNLOCK(fdt, fd);
				FDT_WAKE(fdt, fd);
			} else
			        FDT_FD_UNLOCK(fdt, fd);
		}
		ut->ut_fdmanysz = 0;
	}

	if (isshd) {
		FDT_UNLOCK(fdt);
	}
}

/*
 * Allocate a new file descriptor (with index <= 'start') and
 * point it at 'fp'.  A new file struct ref is taken.
 */
int
fdt_dup(int start, struct vfile *fp, int *fdp)
{
	fdt_t		*fdt = UT_TO_PROC(curuthread)->p_fdt;
	int 		error;

	ASSERT(fdt != NULL);

	/* Take a fp ref for the new fd. */
	ASSERT(!(fp->vf_flag & FINPROGRESS));
	VFILE_REF_HOLD(fp);
	   
	/*
	 * Note that the fd is allocated with its flags zero'd
	 * (including FCLOSEXEC).  Write lock fdt because it's changing.
	 */
	error = ufadd(start, fdt, fp, fdp);

	if (error)
		VFILE_REF_RELEASE(fp);

	return error;
}

/*
 * Get a file descriptor's flags.
 */
char
fdt_getflags(int fd)
{
	proc_t 		*p = curprocp;
	fdt_t		*fdt = p->p_fdt;
	int 		isshd;
	char 		flags = 0;

	if (isshd = ISSHDFD(&p->p_proxy))
		FDT_READ_LOCK(fdt);

	if (fd < fdt->fd_nofiles) {
		flags = fdt->fd_flist[fd].uf_pofile;
	}
	if (isshd)
		FDT_UNLOCK(fdt);
	return flags;
}

/*
 * Set a file descriptor's flags.
 */
void
fdt_setflags(int fd, char flags)
{
	proc_t 		*p = curprocp;
	fdt_t		*fdt = p->p_fdt;
	int 		isshd;

	if (isshd = ISSHDFD(&p->p_proxy))
		FDT_READ_LOCK(fdt);

	ASSERT(0 <= fd && fd < fdt->fd_nofiles);

	FDT_FD_LOCK(fdt, fd);
	fdt->fd_flist[fd].uf_pofile = flags;
	FDT_FD_UNLOCK(fdt, fd);

	if (isshd)
		FDT_UNLOCK(fdt);
}

/*
 * Determine whether the supplied vnode 'vp' is open by the current process.
 * Return 1 if true, 0 otherwise.
 *
 * Note that to be considered open, the corresponding file structure must
 * have a count > 0.
 */
int
fdt_vnode_isopen(vnode_t *vp)
{
	proc_t		*p = curprocp;
	fdt_t		*fdt;
	vfile_t		*fp;
	int		isshd, i, open = 0;

	ASSERT(p);
	fdt = p->p_fdt;

	if (isshd = ISSHDFD(&p->p_proxy))
		FDT_READ_LOCK(fdt);

	/*
	 * This code uses fdt_get_noassert so that it can tolerate file
	 * structs with a zero reference count.  See comment in fdt_exit.
	 */
	for (i = 0 ; i < fdt->fd_nofiles; i++ ) {
	        if (isshd)
	                FDT_FD_LOCK(fdt, i);
		if ((fp = fdt_get_noassert(fdt, i))) {
			if (VF_DATA_EQU(fp, vp) && fp->vf_count > 0) {
				open = 1;
				if (isshd)
					FDT_FD_UNLOCK(fdt, i);
				break;
			}
		}
		if (isshd)
		        FDT_FD_UNLOCK(fdt, i);
	}
	if (isshd)
		FDT_UNLOCK(fdt);	

	return open;
}

/* helper macro for fdt_fuser */
#define	IN_USE(vp)	((vp) != NULL && (VN_CMP((vp), fvp) \
			 || contained && (vp)->v_vfsp == fvp->v_vfsp))

/*
 * Determine whether any open files for a particular process 
 * reference the vnode 'fvp'.
 * 
 * If 'contained' is 1, then this routine instead checks if
 * any of the open files refer to vnodes that are in the same
 * file system (vfs) as fvp's file system.
 *
 * Return 1 if true, 0 otherwise.
 */
int
fdt_fuser(proc_t *p, vnode_t *fvp, int contained)
{
	fdt_t		*fdt = p->p_fdt;
	vfile_t		*fp;
	int 		i, rv = 0;

	/*
	 * Must be case of an embryo process that is not
	 * fully set up during fork processing, but has
	 * already been installed in the active process table.
	 */
	if (fdt == NULL)
		return rv;

	/*
	 * Take fdt write lock to prevent:
	 * - fd_flist from changing
	 * - an open file from being being closed while 
	 *   we're looking at it.
	 */
	FDT_WRITE_LOCK(fdt);
	for (i = 0; i < fdt->fd_nofiles; i++) {
		fp = fdt_get(fdt, i);
		if (fp && VF_IS_VNODE(fp) && IN_USE(VF_TO_VNODE(fp))) {
			rv = 1;
			break;
		}
	}
	FDT_UNLOCK(fdt);

	return rv;
}

/*
 * Like fdt_fuser, but for vsockets instead of vnodes.
 */
int
fdt_suser(proc_t *p, vsock_t *vsp)
{
	fdt_t		*fdt;
	vfile_t		*fp;
	int 		i, rv = 0;

	if (IS_SPROC(&p->p_proxy) && ISSHDFD(&p->p_proxy)) {
		fdt = p->p_shaddr->s_fdt;
	} else
		fdt = p->p_fdt;

	if (fdt == NULL)
		return rv;

	FDT_WRITE_LOCK(fdt);
	for (i = 0; i < fdt->fd_nofiles; i++) {
		fp = fdt_get(fdt, i);
		if (fp && (VF_IS_VSOCK(fp)) && (VF_TO_VSOCK(fp) == vsp)) {
			rv = 1;
			break;
		}
	}
	FDT_UNLOCK(fdt);

	return rv;
}

/*
 * Return the vnode for an open file in a given process.
 * Used by PIOCGETINODE /dev/proc ioctl.
 */
int
fdt_lookup_fdinproc(proc_t *p, int fd, vnode_t **vpp)
{
	fdt_t		*fdt;
	vfile_t		*fp;
	int 		rv;

	if (IS_SPROC(&p->p_proxy) && ISSHDFD(&p->p_proxy))
		fdt = p->p_shaddr->s_fdt;
	else
		fdt = p->p_fdt;
	/*
	 * Must be case of an embryo process that is not
	 * fully set up during fork processing, but has
	 * already been installed in the active process table.
	 */
	if (fdt == NULL)
		return EBADF;

	/*
	 * Take fdt read lock to prevent:
	 * - fd_flist from changing
	 * - an open file from being being closed while 
	 *   we're looking at it.
	 */
	FDT_READ_LOCK(fdt);
	if (fd < 0 || fd >= fdt->fd_nofiles) {
	        FDT_UNLOCK(fdt);
		return EBADF;
	}
	FDT_FD_LOCK(fdt, fd);
	if (!(fp = fdt_get(fdt, fd)))
		rv = EBADF;
	else if (!VF_IS_VNODE(fp))
		rv = EINVAL;
	else {
		*vpp = VF_TO_VNODE(fp);
		VN_HOLD(*vpp);
		rv = 0;
	}
	FDT_FD_UNLOCK(fdt, fd);
	FDT_UNLOCK(fdt);

	return rv;
}

/*
 * Return a referenced file struct pointer for the open file at index 'fd'
 * in the process's file descriptor table.  It's the caller's responsibility 
 * to release the reference.	
 *
 * This routine does fd error checking so that it may be called on
 * fd's passed in from syscalls.  If the return value is NULL, the caller
 * may assume EBADF.
 */
vfile_t *
fdt_getref(proc_t *p, int fd)
{
	fdt_t		*fdt = p->p_fdt;
	vfile_t		*fp;

	ASSERT(fdt);
	FDT_READ_LOCK(fdt);
	if (fd < 0 || fd >= fdt->fd_nofiles) {
	        FDT_UNLOCK(fdt);
		return NULL;
	}

	FDT_FD_LOCK(fdt, fd);
	fp = fdt_get(fdt, fd);
	if (fp != NULL)
		VFILE_REF_HOLD(fp);
	FDT_FD_UNLOCK(fdt, fd);
	FDT_UNLOCK(fdt);

	return fp;
}

/*
 * Return a referenced file struct pointer for the _next_ open file
 * in the process's file descriptor table.  Start searching at index 'fd'.
 * It's the caller's responsibility to release the reference.	
 *
 * It's assumed that the fd is >= 0.
 */
vfile_t *
fdt_getref_next(proc_t *p, int fd, int *returnfd)
{
	fdt_t		*fdt = p->p_fdt;
	vfile_t		*fp;
	int 		i;

	ASSERT(fd >= 0);
	ASSERT(fdt);

	fp = NULL;
	FDT_READ_LOCK(fdt);
	for (i = fd; i < fdt->fd_nofiles; i++) {
	        FDT_FD_LOCK(fdt, fd);
		fp = fdt_get(fdt, i);
		if (fp != NULL) {
			VFILE_REF_HOLD(fp);
			*returnfd = i;
			FDT_FD_UNLOCK(fdt, fd);
			break;
		}
		FDT_FD_UNLOCK(fdt, fd);
	}
	FDT_UNLOCK(fdt);

	return fp;
}
/*
 * Return a referenced file struct pointer for the open file at index 'fd'
 * in the process's file descriptor table, atomically replacing the file pointer
 * with 'swapfp'.  It's the caller's responsibility to release the reference.	
 *
 * This routine does fd error checking so that it may be called on
 * fd's passed in from syscalls.  If the return value is NULL, the caller
 * may assume EBADF.
 */
vfile_t *
fdt_swapref(proc_t *p, int fd, vfile_t *swapfp)
{
	fdt_t		*fdt = p->p_fdt;
	vfile_t		*fp;

	if (fd < 0 || fd >= fdt->fd_nofiles)
		return NULL;

	ASSERT(fdt);
	FDT_WRITE_LOCK(fdt);
	fp = fdt_swapfast(fdt, fd, swapfp);
	FDT_UNLOCK(fdt);

	return fp;
}
/*
 * Convert an array of pollfd structures to an array of vnode/vsock pointers.
 * The vnode/vsock pointer is put in the pv_vobj field of the corresponding
 * pollvobj structure.  A pv_vtype value of 1 indicates vnode, a value of 2
 * indicates vsocket.
 *
 * For invalid fd's, this routine also sets the 'revents' field, to 0
 * if the fd was < 0, else to POLLNVAL.  In the latter case, this
 * routine's return code will be EBADF.  Also, invalid fd's have their 
 * corresponding pvobj field set to NULL.
 */
int
fdt_select_convert(struct pollfd *pfd, int nfds, struct pollvobj *pvobj)
{
	proc_t		*p = curprocp;
	vfile_t		*fp;
	fdt_t		*fdt;
	int		i, fd, error = 0;

	if (!ISSHDFD(&p->p_proxy)) {
		fdt = p->p_fdt;
		for (i = 0 ; i < nfds; i++, pfd++, pvobj++) {
			fd = pfd->fd;
			if ((unsigned) fd < fdt->fd_nofiles) {
				fp = fdt_get(fdt, fd);
				if (fp) {
					if (VF_IS_VNODE(fp)) {
						pvobj->pv_vobj = 
							VF_TO_VNODE(fp);
						pvobj->pv_vtype = 1;
					} else {
						pvobj->pv_vobj = 
							VF_TO_VSOCK(fp);
						pvobj->pv_vtype = 2;
					}
					continue;
				}
			}

			/*
			 * error case - clear vobj and either zero revents
			 * or set to POLLNVAL (zero iff fd was < 0).
			 */
			pvobj->pv_vobj = NULL;
			if (pfd->fd < 0)
				pfd->revents = 0;
			else {
				pfd->revents = POLLNVAL;
				error = EBADF;
			}
		}
	} else {
		for (i = 0 ; i < nfds; i++, pfd++, pvobj++) {
			if (!getf(pfd->fd, &fp)) {
				if (VF_IS_VNODE(fp)) {
					pvobj->pv_vobj = VF_TO_VNODE(fp);
					pvobj->pv_vtype = 1;
				} else {
					pvobj->pv_vobj = VF_TO_VSOCK(fp);
					pvobj->pv_vtype = 2;
				}
				continue;
			}
			/*
			 * error case - clear vobj and either zero revents
			 * or set to POLLNVAL (zero iff fd was < 0).
			 */
			pvobj->pv_vobj = NULL;
			if (pfd->fd < 0)
				pfd->revents = 0;
			else {
				pfd->revents = POLLNVAL;
				error = EBADF;
			}
		}
	}

	return error;
}
