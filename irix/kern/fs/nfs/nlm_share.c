#if 0
static char sccsid[] = "@(#)prot_share.c	1.3 90/11/09 NFSSRC4.1 Copyr 1990 Sun Micro";
#endif

	/*
	 * Copyright (c) 1986 by Sun Microsystems, Inc.
	 */

/*
 * Copyright (c) 1987 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

	/*
	 * prot_share.c consists of subroutines that implement the
	 * DOS-compatible file sharing services for PC-NFS
	 */

#define NFSSERVER
#include "types.h"
#include <ksys/vfile.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/mbuf.h>
#include <sys/pathname.h>
#include <sys/pda.h>
#include <sys/siginfo.h>    /* for CLD_EXITED exit code */
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <sys/sema.h>
#include <sys/pcb.h>        /* for setjmp and longjmp */
#include <sys/atomic_ops.h>
#include <sys/cmn_err.h>
#include "xdr.h"
#include "auth.h"
#include "auth_unix.h"
#include "svc.h"
#include "nfs.h"
#include "lockmgr.h"
#include "export.h"
#include "lockd_impl.h"
#include "nlm_svc.h"
#include "nlm_share.h"
#include "nlm_debug.h"


int share_debug = 0;
static mutex_t share_mutex;
extern int lock_share_requests;

#define SH_HASH	29
/*
 * File sharing records are organized as follows:
 *
 *                     fh (file handle)
 *                      |
 *                      v
 *                    hash()
 *                      |
 *                      +-------------+
 *                                    |
 *                                    v
 *               +-+-+-+-+-+-+-+-+-+-+-+-+-+
 *     fh_to_sx[]| | | | | | | | | | |*| | |
 *               +-+-+-+-+-+-+-+-+-+-+|+-+-+
 *                                    |
 *      +-----------------------------+
 *      |
 *      +-->  +---+   +---+   +---+   +---+
 *   sx       |FWD--->|FWD--->|FWD--->| 0 |   (head of chains for files
 *            +---+   +---+   +---+   +---+    whose handle hash to the
 *            | 0 |<---BCK|<---BCK|<---BCK|    same value)
 *            +---+   +---+   +---+   +---+
 *            |sxp|   |sxp|   |sxp|   |sxp|
 *            +---+   +-|-+   +---+   +---+
 *                      |
 *      +---------------+
 *      |
 *      +-->  +---+   +---+   +---+   +---+
 *   sxx      |FWD--->|FWD--->|FWD--->| 0 |   (multiple shared access to
 *            +---+   +---+   +---+   +---+    the same file)
 *            | 0 |<---BCK|<---BCK|<---BCK|
 *            +---+   +---+   +---+   +---+
 *            | s |   | s |   | s |   | s |	 (share: the structure
 *            | h |   | h |   | h |   | h |	  passed	in the RPC)
 *            | a |   | a |   | a |   | a |
 *            | r |   | r |   | r |   | r |
 *            | e |   | e |   | e |   | e |
 *            +---+   +---+   +---+   +---+
 */

struct	sxx {
	struct sxx	*fwd;
	struct sxx	*bck;
	int vers;
	nlm4_share	sh;
};

struct sx {
	struct sx	*fwd;
	struct sx	*bck;
	struct sxx	*sxxp;
	flock_t		*flock;
};

static struct sx *fh_to_sx[SH_HASH];

static void free_sxx ( struct sxx * );
static struct sxx *new_sxx ( int vers, nlm4_share *s );
static bool_t compatible ( struct sxx *ss , nlm4_share *s );
static struct flock *set_lock(int, long, nlm4_shareargs *, nlm4_shareres *,
	struct exportinfo *exi);
static void free_lock(int, char *, flock_t *, struct exportinfo *exi);

/*
 *	There are two entry points into the prot_share
 *	subsystem:
 *
 *	proc_nlm_share
 *		called out of lockd_dispatch() to process SHARE or UNSHARE request
 *
 *	destroy_client_shares
 *		called whenever a client has been declared dead by the
 *		status monitor. We throw away all sharing records
 *		for the client
 */

void
share_init(void)
{
	mutex_init(&share_mutex, MUTEX_DEFAULT, "share mutex");
	bzero(&fh_to_sx, sizeof(fh_to_sx));
}

void
nlm_share_options(int opt)
{
	lock_share_requests = opt;
}

static int
obj_cmp(netobj *a, netobj *b)
{
        if (a->n_len != b->n_len)
                return(FALSE);
        return bcmp(&a->n_bytes[0], &b->n_bytes[0], a->n_len) == 0;
}

/*
 * Upon entry, the cookie in resp has been filled in and grace period
 * filtering has been done.
 *
 * The following is the rough flow for proc_nlm_share:
 *
 *	hv = hash(fh)
 *	sxh = fh_to_sx[hv];
 *	if (sxh!=NULL) {
 *		scan the chain looking for sxh->sxxp.sh.fh==fh
 *	}
 *	if (sxh==NULL) {
 *		SHARE: allocate new sx and sxx and chain to fh_to_sx[hv]
 *		UNSHARE: give up
 *	}
 *	sxxh = sxh->sxxp
 *	scan the chain looking for (sxxh->sh.caller_mame==caller_name &&
 *		sxxh->sh.oh == oh) [i.e. the matching share structure]
 *	if (sxxh!=NULL) {
 *		SHARE: return {SUCCESS} [assume repeat request]
 *		UNSHARE: release *sxxh, which may free sxh, which may lead
 *		        to the fh_to_sx entry being reset
 *	}
 *	[there is no existing entry]
 *	UNSHARE: give up
 *	SHARE: scan chain starting at sxxxh->sxxp to see if the sharing
 *		is legal. If not, fail. Otherwise allocate new
 *		sxx, initialize it and add it to the list.
 *		Return {SUCCESS}
 */
void
proc_nlm_share(struct svc_req *Rqstp, nlm4_shareargs *req,
	nlm4_shareres *resp, struct exportinfo *exi)
{
	struct	sx	*sxh;
	struct	sxx	*sxxh;
	int	hv;
	bool_t	SHARE_REQ;
	register unsigned char *c;
	register int i;

/*
 * Remember what kind of request this was
 */

	SHARE_REQ = (Rqstp->rq_proc == NLM_SHARE);
#define UNSHARE_REQ (!SHARE_REQ)

#ifdef DEBUG
	if (share_debug) {
		printf("proc_nlm_share: %s from %s\n",
			SHARE_REQ?"share":"unshare",
			req->share.caller_name);
		if (SHARE_REQ)
			printf("share: mode=%d, access=%d\n",
				req->share.mode, req->share.access);
	}
#endif /* DEBUG */

	mutex_enter(&share_mutex);
/*
 * Compute the hash index and get the start of the sxx chain
 */
	hv = 0;
	i = req->share.fh.n_len;
	if (i > 20) i = 20;
	for (c = (unsigned char *) req->share.fh.n_bytes; --i;)
		hv += *c++;
	hv %= SH_HASH;

	sxh = fh_to_sx[hv];

/*
 *	If sxh != NULL, scan down looking for the fh we are dealing with
 */
	while (sxh != NULL) {
		if (obj_cmp(&req->share.fh, &sxh->sxxp->sh.fh))
			break;
		sxh = sxh->fwd;
	}

/*
 * If sxh is null, we're not currently holding a sharing state for
 * this file. If it's a SHARE request, allocate new sx & sxx structures
 */
	if (sxh == NULL) {
		if (SHARE_REQ) {
			flock_t *rl = NULL;

			if (lock_share_requests) {
				/* FSM_DN is essentially a request to NOT lock the
				 * file, so we just grant it...
				 */
				if ((req->share.mode == FSM_DN) &&
					(req->share.access == FSA_NONE))
						rl = NULL;
				else if (!(rl = set_lock(Rqstp->rq_vers,
					Rqstp->rq_xprt->xp_raddr.sin_addr.s_addr, req, resp,
					exi))) {
#ifdef DEBUG
						if (share_debug)
							printf("...set_lock failed\n");
#endif /* DEBUG */
						resp->stat = NLM4_DENIED;
						goto common_exit;
				}
#ifdef DEBUG
				else if (share_debug)
					printf("...set_lock succeeded!\n");
#endif /* DEBUG */
			}
#ifdef DEBUG
			if (share_debug)
				printf("...null sxh - new file\n");
#endif /* DEBUG */
			sxh = (struct sx *)NLM_KMEM_ALLOC(sizeof(struct sx), KM_SLEEP);
			sxh->bck = NULL;
			sxh->fwd = fh_to_sx[hv];
			fh_to_sx[hv] = sxh;
			sxh->sxxp = new_sxx(Rqstp->rq_vers, &req->share);
			sxh->flock = rl;
		}
		resp->stat = NLM4_GRANTED; /* either way.... */
		goto common_exit;
	}

/*
 * If sxh is non-null, we have sharing state for this file.
 * Let's try and find an sx which corresponds exactly to this
 * request.
 */
#ifdef DEBUG
	if (share_debug)
		printf("...sxh @0x%x\n", sxh);
#endif /* DEBUG */
	sxxh = sxh->sxxp;
	while (sxxh != NULL) {
		if (!strcasecmp(sxxh->sh.caller_name, req->share.caller_name) &&
			obj_cmp(&sxxh->sh.oh, &req->share.oh))
			break;
		sxxh = sxxh->fwd;
	}

/*
 * If sxxh is non-null, we have found an entry corresponding to
 * the request. For SHARE, we just reply "nlm_granted" -- presumably
 * this is an artefact of UDP-based RPC's. For UNSHARE, we must
 * throw away the sxx, and maybe the sx.
 */
	if (sxxh != NULL) {
#ifdef DEBUG
		if (share_debug)printf("...matching sxx @0x%x\n", sxxh);
#endif /* DEBUG */
		if (UNSHARE_REQ) {
/*
 * Routine list manipulation follows....
 */
			if (sxxh->bck == NULL)
				sxh->sxxp = sxxh->fwd;
			else
				sxxh->bck->fwd = sxxh->fwd;
			if (sxxh->fwd != NULL)
				sxxh->fwd->bck = sxxh->bck;
/*
 * Check to see if we need to eliminate the sxx as well
 */
			if (sxh->sxxp == NULL) {
				if (sxh->bck == NULL)
					fh_to_sx[hv] = sxh->fwd;
				else
					sxh->bck->fwd = sxh->fwd;
				if (sxh->fwd != NULL)
					sxh->fwd->bck = sxh->bck;
/*
 * Free the lock and close the fd now
 */
				if (lock_share_requests)
				    free_lock(Rqstp->rq_vers, sxxh->sh.fh.n_bytes, sxh->flock,
						exi);
				NLM_KMEM_FREE(sxh, sizeof(struct sx));
			}			
			free_sxx(sxxh);
		}
		resp->stat = NLM4_GRANTED; /* for SHARE or UNSHARE */
		goto common_exit;
	}

/*
 * We've now established that there's no existing sx corresponding
 * to this request. For UNSHARE, we just give "ok".... For
 * SHARE we've got to validate this sharing request. If it's o.k.
 * we build a new sx and add it to the list. [Current thinking
 * is that there's no special ordering here, so we can just stick it
 * on the front.]
 */
	if (SHARE_REQ) {
		if (!compatible(sxh->sxxp, &req->share)) {
#ifdef DEBUG
			if (share_debug)
				printf("...not compatible\n");
#endif /* DEBUG */
			resp->stat = NLM4_DENIED;
			goto common_exit;
		}
		sxxh = new_sxx(Rqstp->rq_vers, &req->share);
#ifdef DEBUG
		if (share_debug)printf("...compatible. new sxx @0x%x\n", sxxh);
#endif /* DEBUG */
		sxh->sxxp->bck = sxxh;
		sxxh->fwd = sxh->sxxp;
		sxh->sxxp = sxxh;
	}
	resp->stat = NLM4_GRANTED;	/* SHARE/UNSHARE. Now we fall through to... */
/*
 *	Common exit - issue reply with appropriate result code,
 * free request structure and quit
 */
common_exit:
	mutex_exit(&share_mutex);
	return;
}

/*
 * The following is the rough flow for destroy_client_shares:
 *
 *	for (hv = 0; hv < SH_HASH; hv++){
 *	sxh = fh_to_sx[hv];
 *	while (sxh!=NULL) {
 *		sxxh = sxh->sxxp
 *		while (sxxh != NULL) {
 *			if (!strcasecmp(sxxh->sh.caller_name,client))
 *				blow away the entry (may longjump out of loops)
 *			sxxh=sxxh->fwd
 *		}
 *		sxh=sxh->fwd;
 *	}
 */
	
/* ARGSUSED */
void
destroy_client_shares(struct svc_req *Rqstp, char *client)
{
	fhandle_t	*fhp;
	struct	sx	*sxh;
	struct	sx	*next_sx;
	struct	sxx	*sxxh;
	struct	sxx	*next_sxx;
	int	hv;
	struct exportinfo *exi;

#ifdef DEBUG
	if (share_debug)
		printf("destroy_client_shares for %s\n", client);
#endif /* DEBUG */

	mutex_enter(&share_mutex);
	for (hv = 0; hv < SH_HASH; hv++) {
		sxh = fh_to_sx[hv];

/*
 *	If sxxh != NULL, scan down looking for the fh we are dealing with
 */
		while (sxh != NULL) {
			sxxh = sxh->sxxp;
			next_sx = sxh->fwd;
			while (sxxh != NULL) {
				next_sxx = sxxh->fwd;
				if (!strcasecmp(sxxh->sh.caller_name, client)) {
/*
 * Blow this one away....
 */
#ifdef DEBUG
					if (share_debug)
						printf("... zap sxx @0x%x\n", sxxh);
#endif /* DEBUG */
					if (sxxh->bck == NULL)
						sxh->sxxp = sxxh->fwd;
					else
						sxxh->bck->fwd = sxxh->fwd;
					if (sxxh->fwd != NULL)
						sxxh->fwd->bck = sxxh->bck;
/*
 * Check to see if we need to eliminate the sxx as well
 */
					if (sxh->sxxp == NULL) {
#ifdef DEBUG
						if (share_debug)
							printf("...  &  sx @0x%x\n", sxh);
#endif /* DEBUG */
						if (sxh->bck == NULL)
							fh_to_sx[hv] = sxh->fwd;
						else
							sxh->bck->fwd = sxh->fwd;
						if (sxh->fwd != NULL)
							sxh->fwd->bck = sxh->bck;
/*
 * Release the associated lock and close the fd
 */
						if (lock_share_requests) {
							switch (sxxh->vers) {
								case NLM_VERSX:
									fhp = (fhandle_t *)sxxh->sh.fh.n_bytes;
									break;
								case NLM4_VERS:
									fhp = &((nfs_fh3 *)fhp)->fh3_u.nfs_fh3_i.fh3_i;
									break;
								default:
									if (sxxh->sh.fh.n_len == NFS_FHSIZE) {
										fhp = (fhandle_t *)sxxh->sh.fh.n_bytes;
									} else {
										fhp = &((nfs_fh3 *)fhp)->fh3_u.nfs_fh3_i.fh3_i;
									}
							}
							/*
							 * get the export entry
							 * if there is one, free the lock, otherwise,
							 * continue (the lock will have been released
							 * in unexport)
							 */
							if (exi = findexport(&fhp->fh_fsid,
								(struct fid *) &fhp->fh_xlen)) {
							   	 free_lock(sxxh->vers, sxxh->sh.fh.n_bytes,
									sxh->flock, exi);
									EXPORTED_MRUNLOCK();
							}
						}
						NLM_KMEM_FREE(sxh, sizeof(struct sx));
						free_sxx(sxxh);
						goto	skip_to_next_sx;
					}			
					free_sxx(sxxh);
				}	/* ... of "if (strcasecmp..." */
				sxxh = next_sxx;
			} /* of "while (sxxh..." */
skip_to_next_sx:
			sxh = next_sx;
		} /* ... of "while (sxh..." */
	} /* ... of	"for (hv... " */
	mutex_exit(&share_mutex);
}

/*
 *	------ Support routines -----
 */


static void
free_sxx(struct sxx *s)
{
	int size;

	size = strlen(s->sh.caller_name) + 1;
	NLM_KMEM_FREE(s->sh.caller_name, size);
	if (s->sh.fh.n_bytes) {
  		NLM_KMEM_FREE(s->sh.fh.n_bytes, s->sh.fh.n_len);
	}
	if (s->sh.oh.n_bytes) {
		NLM_KMEM_FREE(s->sh.oh.n_bytes, s->sh.oh.n_len);
	}
	NLM_KMEM_FREE(s, sizeof(struct sxx));
}





/*
 *	Allocate a new sxx structure based on that in the RPC. We make use
 * of the fact that the caller-name, fh and oh are dynamically
 * allocated, and that we won't need them in the original copy after
 * this point in time, so we just copy the pointers and lengths and
 * set the pointers to NULL
 */
static struct sxx *
new_sxx(int vers, nlm4_share *s)
{
	struct sxx *new;
	int size;

	new = (struct sxx *)NLM_KMEM_ALLOC(sizeof(struct sxx), KM_SLEEP);

	new->fwd = NULL;
	new->bck = NULL;
	new->vers = vers;

	size = strlen(s->caller_name) + 1;
	new->sh.caller_name = NLM_KMEM_ALLOC(size, KM_SLEEP);
	bcopy(s->caller_name, new->sh.caller_name, size);

	new->sh.fh.n_len = s->fh.n_len;
	new->sh.fh.n_bytes = s->fh.n_bytes;
	s->fh.n_bytes = NULL;

	new->sh.oh.n_len = s->oh.n_len;
	new->sh.oh.n_bytes = s->oh.n_bytes;
	s->oh.n_bytes = NULL;

	new->sh.mode = s->mode;
	new->sh.access = s->access;

	return(new);
}
/*
 * compatible - the guts of the whole thing. Takes a pointer to
 * the first sx structure in the list and one to the share structure
 * in the RPC. Returns TRUE if the SHARE is compatible with the
 * existing state.
 *
 * The checking relies on the bit encoding for fsh_mode and
 * fsh_access (see nlm_prot.h). Specifically,
 * if any existing access ANDed with the requested mode is non-zero, OR
 * if any existing mode ANDed with the requested access is non-zero,
 * we deny the SHARE.
 */
static bool_t
compatible(struct sxx *ss, nlm4_share *s)
{
	while (ss != NULL) {
		if (((int)ss->sh.mode & (int)s->access) ||
				((int)ss->sh.access & (int)s->mode)) {
			if (share_debug)
				printf("\nFile sharing request from %s (mode %d, access %d) conflicts\nwith sharing for %s (mode %d, access %d)\n",
				s->caller_name, s->mode, s->access,
				ss->sh.caller_name, ss->sh.mode, ss->sh.access);
			return(FALSE);
		}
		ss = ss->fwd;
	}
	return(TRUE);
}

static flock_t *
nlm_shareargstoflock(nlm4_shareargs *from, long ipaddr)
{
	fsh_mode mode;
	flock_t *flp = NULL;

	flp = (flock_t *)NLM_KMEM_ALLOC(sizeof(flock_t), KM_SLEEP);

	mode = (fsh_mode)from->share.mode;

	/* Novell's NFS Client doesn't set the mode if access is FSA_R;
	 * To be sure, we beef up the mode according to access.
	 */
	switch (from->share.access) {
		case FSA_R:
			mode = (fsh_mode)MAX((int)mode, (int)FSM_DW);
			break;
		case FSA_W:
		case FSA_RW:
			mode = (fsh_mode)MAX((int)mode, (int)FSM_DRW);
			break;
	}

	switch (mode) {
		case FSM_DR:
			/*
			 * Technically, writing should still be allowed.  There is no
			 * analogous lock in Unix, however.  Is this important in
			 * practice?
			 */
		case FSM_DRW:
			flp->l_type = F_WRLCK;
			break;
		case FSM_DW:
			flp->l_type = F_RDLCK;
			break;
		case FSM_DN:
			/* Just to be sure; these will be reset according to access */
			flp->l_type = 0;
			break;
	}

	flp->l_start = (off_t)0;
	flp->l_len = (off_t)0;
	flp->l_pid = -2;
	flp->l_sysid = ipaddr;

    return(flp);
}


/* Given a share request attempt to set the lock.
 * If successful, return the fd; otherwise return 0
 */
static struct flock *
set_lock(int vers, long ipaddr, nlm4_shareargs *req, nlm4_shareres *resp,
	struct exportinfo *exi)
{
    flock_t *flp = NULL;
	int error;
	fhandle_t *fhp;

	flp = nlm_shareargstoflock(req, ipaddr);
	switch (vers) {
		case NLM_VERSX:
			fhp = (fhandle_t *)req->share.fh.n_bytes;
			break;
		case NLM4_VERS:
			fhp = &((nfs_fh3 *)req->share.fh.n_bytes)->fh3_u.nfs_fh3_i.fh3_i;
			break;
		default:
			return(NULL);
	}
	switch (error = local_lock(exi, fhp, flp, 0)) {
		case 0:
			break;
		default:
			resp->stat = errno_to_nlm4(error);
			NLM_KMEM_FREE(flp, sizeof(flock_t));
			flp = NULL;
	}

    return(flp);
}

/* If no more share state exists for this file, free the lock.
 * Otherwise do nothing, as someone else still has this file locked.
 */
static void
free_lock(int vers, char *fh, flock_t *rl, struct exportinfo *exi)
{
	fhandle_t *fhp;

	switch (vers) {
		case NLM_VERSX:
			fhp = (fhandle_t *)fh;
			break;
		case NLM4_VERS:
			fhp = &((nfs_fh3 *)fh)->fh3_u.nfs_fh3_i.fh3_i;
			break;
		default:
			return;
	}
	if (rl) {
		(void)local_unlock(exi, fhp, rl, 0);
		NLM_KMEM_FREE(rl, sizeof(flock_t));
	}
}
