/*	 @(#)nfs_subr.c	1.7 88/08/09 NFSSRC4.0 from 2.84 88/02/08 SMI
 *
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 *
 * Client side utilities
 */
#ident      "$Revision: 1.83 $"

#include "types.h"
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/mkdev.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/uthread.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/mac_label.h>
#include <sys/cmn_err.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include "auth.h"		/* XXX required before clnt.h */
#include "clnt.h"
#include "xdr.h"
#include "nfs.h"
#include "nfs_clnt.h"
#include "rnode.h"
#include "bds.h"
#include "netinet/in.h"
#include "nfs_stat.h"
#include "export.h"
#include <rpcsvc/nlm_prot.h>
#include "nlm_debug.h"
#include "nsd.h"

/*
 * Maps for minor device allocation.
 */
struct minormap	nfs_minormap;
struct minormap	nfs3_minormap;
long make_local_fsid(u_long, struct mntinfo *, dev_t, struct minormap *);

extern CLIENT 	*clget(struct mntinfo *, struct cred *);
extern u_long	clnt_getaddr(CLIENT *);

extern int max_nfs_clients;
extern int max_nsd_clients;
extern int max_xnfs_clients;

/* 
 * Client handle table for caching client handle structures. Functions
 * clgetcommon and clfreecommon manage these structures.
 */
struct chtab {
	int	ch_timesused;
	bool_t	ch_inuse;
	u_long	ch_prog;
	u_long	ch_vers;
	bool_t	ch_hard;
	CLIENT	*ch_client;
};

struct chtab *chtable  = NULL;
struct chtab *xchtable = NULL;
struct chtab *nchtable = NULL;

mutex_t	chtablesem;		/* client handle table lock */
mutex_t xchtablesem;		/* xattr client handle table lock */
mutex_t	nchtablesem;		/* all other client handle table lock */

/*
 * Define exihashtab[] here to be part of the nfs client pkg.
 */
struct	exportinfo *exihashtab[EXIHASHTABSIZE];

void
chtables_init(void)
{
	mutex_init(&chtablesem,  MUTEX_DEFAULT, "chtbsem");
	mutex_init(&xchtablesem, MUTEX_DEFAULT, "xchtbsem");
	mutex_init(&nchtablesem, MUTEX_DEFAULT, "nchtbsem");
	
	if (max_nfs_clients > 0 )
		chtable  = kmem_zalloc(sizeof(struct chtab) * max_nfs_clients,  KM_SLEEP);
	if (max_xnfs_clients > 0)
		xchtable = kmem_zalloc(sizeof(struct chtab) * max_xnfs_clients, KM_SLEEP);
	if (max_nsd_clients > 0 )
		nchtable = kmem_zalloc(sizeof(struct chtab) * max_nsd_clients,  KM_SLEEP);

	if (chtable ==  NULL) 
		max_nfs_clients  = 0;
	if (xchtable == NULL)
		max_xnfs_clients = 0;
	if (nchtable == NULL)
		max_nsd_clients  = 0;
}

static CLIENT *
clgetcommon(struct mntinfo *mi, struct cred *cred, struct chtab *table,
	    int numclients, mutex_t *sem)
{
	int error;
	struct chtab *ch;
	int retrans;
	enum kudp_intr intr = (mi->mi_flags & MIF_INT) ? KUDP_INTR : KUDP_NOINTR;
	struct chtab *lastunused;
	
	/*
	 * If soft mount and server is down just try once
	 */
	if (!(mi->mi_flags & MIF_HARD) && (mi->mi_flags & MIF_DOWN)) {
		retrans = 1;
	} else {
		retrans = mi->mi_retrans;
		if (retrans <= 0)	/* i.e. don't REtransmit */
		    retrans = 1;
	}

	/*
	 * Find an unused handle or create one.
	 */
	lastunused = (struct chtab *)-1L;
	while (lastunused) {
		lastunused = NULL;
		mutex_lock(sem, PZERO);
		/* NB: clstat.nclgets update is serialized by sem */
		if (mi->mi_vers == NFS3_VERSION) 
			CLSTAT3.nclgets++;
		else
			CLSTAT.nclgets++;
		for (ch = table ; ch && ch < &table[numclients]; ch++) { 
			if (ch->ch_inuse == FALSE) {
				if (ch->ch_client == NULL) {
					ch->ch_inuse = TRUE;
					mutex_unlock(sem);
					ch->ch_client =
						clntkudp_create(&mi->mi_addr,
						    mi->mi_prog, mi->mi_vers,
						    retrans, intr,
						    KUDP_XID_CREATE, cred);
					if (ch->ch_client == NULL) {
						ch->ch_inuse = FALSE;
						return(NULL);
					}
					ch->ch_prog = mi->mi_prog;
					ch->ch_vers = mi->mi_vers;
				} else {
					if ((ch->ch_prog == mi->mi_prog) &&
					    (ch->ch_vers == mi->mi_vers)) {
						ch->ch_inuse = TRUE;
						mutex_unlock(sem);
						error = clntkudp_init(ch->ch_client,
						      &mi->mi_addr, retrans,
						      intr, KUDP_XID_CREATE, 
						      cred);
						if (error) {
							CLNT_DESTROY(ch->ch_client);
							ch->ch_client =
								clntkudp_create(&mi->mi_addr,
								    mi->mi_prog, mi->mi_vers,
								    retrans, intr,
								    KUDP_XID_CREATE, cred);
							if (ch->ch_client == NULL) {
								ch->ch_inuse = FALSE;
								return(NULL);
							}
						}
					} else {
						lastunused = ch;
						continue;
					}
				}
				ch->ch_timesused++;
				clntkudp_soattr(ch->ch_client, mi->mi_ipmac);
				return (ch->ch_client);
			}
		} /* for (ch = table ; ... */
		/*
		 * If we get here, there are no available handles.
		 * If there are none with the requested program/version,
		 * make an unused handle available.
		 */
		if (lastunused) {
			CLIENT *cl;
			
			lastunused->ch_inuse = TRUE;
			ASSERT(lastunused->ch_client != NULL);
			cl = lastunused->ch_client;
			lastunused->ch_client = NULL;
			lastunused->ch_inuse = FALSE;
			mutex_unlock(sem);
			CLNT_DESTROY(cl);
#ifdef DEBUG
			cmn_err(CE_NOTE,
				"Reusing client handle %lx. prog=%d, vers=%d. new prog=%d new vers=%d\n",
				lastunused,lastunused->ch_prog,
				lastunused->ch_vers, mi->mi_prog, mi->mi_vers);
#endif
		}
	} /* while (lastunused) */

	/* 
	 * If we get here, there are no unused available handles.
	 * Just allocate one and leave.
	 */
	{
		CLIENT *cl = NULL;
		
		mutex_unlock(sem);
		cl = clntkudp_create(&mi->mi_addr,
				     mi->mi_prog, mi->mi_vers,
				     retrans, intr,
				     KUDP_XID_CREATE, cred);
		if (cl != NULL) {
			
			clntkudp_soattr(cl, mi->mi_ipmac);
#ifdef DEBUG
			cmn_err(CE_NOTE,"New client handle %lx. prog=%d, vers=%d.\n", 
				cl, mi->mi_prog, mi->mi_vers);
#endif
			if (mi->mi_vers == NFS3_VERSION) 
				CLSTAT3.nclsleeps++;
			else
				CLSTAT.nclsleeps++;

			return (cl);
		} else {
			return (NULL);
		}
	} 
}

/*
 * Called after mi is marked stale to wake up all users of the mount point.
 */
void
cl_abort_all(struct mntinfo *mi, struct chtab *table, int numclients, mutex_t *sem)
{
	struct chtab *ch;
	void ckuwakeup(void *p);

	/*
	 * Search the table, looking for in use clients that match.
	 */
	mutex_lock(sem, PZERO);
	for (ch = table; ch && ch < &table[numclients]; ch++) { 
		if (ch->ch_inuse != TRUE || !ch->ch_client ||
		    clnt_getaddr(ch->ch_client) != mi->mi_addr.sin_addr.s_addr) {
			continue;
		}
		ckuwakeup(ch->ch_client->cl_private);
	}
	mutex_unlock(sem);
}

void
nfs_wakeup(struct mntinfo *mi)
{
	cl_abort_all(mi, chtable,  max_nfs_clients,  &chtablesem);
	cl_abort_all(mi, xchtable, max_xnfs_clients, &xchtablesem);
	cl_abort_all(mi, nchtable, max_nsd_clients,  &nchtablesem);
}

/*
 * The caller is an NFS server who is exiting.  If the NFS file system
 * is still around then clean it up as much as possible.
 *
 * XXX - it would be nice if this tried to do an unmount if possible.
 */
/* ARGSUSED */
void
stop_nfs(void *arg)
{
	extern	lock_t mntinfolist_lock;
	extern	mntinfo_t *mntinfolist;
	mntinfo_t	 *mi;
	int	ospl = mutex_spinlock(&mntinfolist_lock);
	pid_t	pid = current_pid();

	for (mi = mntinfolist; mi; mi = mi->mi_next) {
		if (mi->mi_pid == pid) {
			mutex_spinunlock(&mntinfolist_lock, ospl);
			mutex_enter(&mi->mi_lock);
			break;
		}
	}
	if (mi) {
		atomicSetUint(&mi->mi_flags, MIF_STALE);
		nfs_wakeup(mi);
		mutex_exit(&mi->mi_lock);
	} else
		mutex_spinunlock(&mntinfolist_lock, ospl);
}

CLIENT *
xclget(struct mntinfo *mi, struct cred *cred)
{
	return(clgetcommon(mi, cred, xchtable, max_xnfs_clients, &xchtablesem));
}

CLIENT *
clget(struct mntinfo *mi, struct cred *cred)
{
	return(clgetcommon(mi, cred, chtable, max_nfs_clients, &chtablesem));
}

CLIENT *
nclget(struct mntinfo *mi, struct cred *cred)
{
	return(clgetcommon(mi, cred, nchtable, max_nsd_clients, &nchtablesem));
}

static void
clfreecommon(CLIENT *cl, struct chtab *table, int numclients, mutex_t *sem)
{
	struct chtab *ch;
	int found = 0;

	mutex_lock(sem, PZERO);
	for (ch = table; ch && ch < &table[numclients]; ch++) {
		if (ch->ch_client == cl) {
			ch->ch_inuse = FALSE;
			found = 1;
			break;
		}
	}
	mutex_unlock(sem);

	if ( found == 0 ) {
		CLNT_DESTROY(cl);
#ifdef DEBUG
		cmn_err(CE_NOTE,"Free client handle %lx.\n", cl);
#endif
	}
}

void
xclfree(CLIENT *cl)
{
	clfreecommon(cl, xchtable, max_xnfs_clients, &xchtablesem);
}

void
clfree(CLIENT *cl)
{
	clfreecommon(cl, chtable, max_nfs_clients, &chtablesem);
}

void
nclfree(CLIENT *cl)
{
	clfreecommon(cl, nchtable, max_nsd_clients, &nchtablesem);
}

char *rfsnames[] = {
	"null", "getattr", "setattr", "unused", "lookup", "readlink", "read",
	"unused", "write", "create", "remove", "rename", "link", "symlink",
	"mkdir", "rmdir", "readdir", "fsstat" };


/*
 * Back off for retransmission timeout, MAXTIMO is in 10ths of a sec
 */
#define MAXTIMO	900
#define backoff(tim)	((((tim) << 1) > MAXTIMO) ? MAXTIMO : ((tim) << 1))

/* Hack to help recognize read ahead requests from biod */
#define is_biod()	(curuthread ? (curuthread->ut_asid.as_pasid == 0) \
				: 0)

int
rfscall(mi, which, xdrargs, argsp, xdrres, resp, cred, frombio)
	struct mntinfo *mi;
	int	 which;
	xdrproc_t xdrargs;
	caddr_t	argsp;
	xdrproc_t xdrres;
	caddr_t	resp;
	struct cred *cred;
	int frombio;
{
	CLIENT *client;
	enum clnt_stat status;
	struct rpc_err rpcerr;
	struct timeval wait;
	struct cred *newcred;
	int timeo;
	bool_t tryagain;

	if (mi->mi_flags & MIF_STALE) {
		return (ESTALE);
	}
	if ((mi->mi_flags & MIF_PRINTED) && nohang()) {
		return (ETIMEDOUT);
	}
	if (mi->mi_vers == NFS3_VERSION) {
		CLSTAT3.ncalls++;
		CLSTAT3.reqs[which]++;
	} else {
		CLSTAT.ncalls++;
		CLSTAT.reqs[which]++;
	}

        if (mi->mi_flags & MIF_TCP) {
		/* XXX - fast timeout (SNOHANG) and nsd not supported */
            	return(rfscall_tcp(mi, which,
		    xdrargs, argsp, xdrres, resp, cred, frombio));
	}
	rpcerr.re_errno = 0;
	newcred = NULL;
	timeo = mi->mi_timeo;
retry:
	client = clget(mi, cred);
	/*
	 * If clget returns a NULL client handle, it is because clntkudp_create
	 * failed.  Rather than panic, we just return the appropriate error
	 * code which was set by clntkudp_create.
	 */
	if (!client) {
		return(rpc_createerr.cf_error.re_errno);
	}

	/*
	 * If hard mounted fs, retry call forever unless hard error occurs
	 */
	do {
		tryagain = FALSE;

		wait.tv_sec = timeo / 10;
		wait.tv_usec = 100000 * (timeo % 10);
		status = CLNT_CALL(client, which, xdrargs, argsp,
		    xdrres, resp, wait);
		switch (status) {
		case RPC_SUCCESS:
			/* Got a message through -- there is a server */
			
			if (mi->mi_flags & MIF_NO_SERVER)
				atomicClearUint(&mi->mi_flags, MIF_NO_SERVER);
			break;

		/*
		 * Unrecoverable errors: give up immediately
		 */
		case RPC_AUTHERROR:
		case RPC_CANTENCODEARGS:
		case RPC_CANTDECODERES:
		case RPC_VERSMISMATCH:
		case RPC_PROGVERSMISMATCH:
		case RPC_CANTDECODEARGS:
		case RPC_PROGUNAVAIL:
			break;

		case RPC_INTR:	
			ASSERT(mi->mi_flags & MIF_INT);
			break;

#ifdef DEBUG
		case RPC_CANTSEND:
			CLNT_GETERR(client, &rpcerr); /* not a server error */
			printf("rfscall: can't send, error %d\n", rpcerr.re_errno);
#endif /* DEBUG */
		default:
			/* No response from server, no server? */
			if (!(mi->mi_flags & MIF_NO_SERVER))
				atomicSetUint(&mi->mi_flags, MIF_NO_SERVER);
			
			if (nohang()) {
				if (!(mi->mi_flags & MIF_PRINTED) &&
				    !(atomicSetUint(&mi->mi_flags, MIF_PRINTED) & MIF_PRINTED))
				{
					printf("NFS server %s not responding\n",
					       mi->mi_hostname);
				}
				rpcerr.re_errno = ETIMEDOUT;
				goto out;
			}
			if (mi->mi_flags & MIF_STALE) {
				rpcerr.re_errno = ESTALE;
				goto out;
			}

			/*
			 * If biod has a problem doing a read on a hard/intr
			 * mount, don't bother to retry it since we are holding
			 * the node locked and anyone waiting for it will get
			 * stuck forever.  If we are pushing out delayed
			 * writes however, there is no alternative to spinning
			 * indefinitely.  This means that a down server can
			 * still hang a hard/intr client if there are write
			 * requests pending.
			 */
			if ((mi->mi_flags & MIF_HARD) && !((mi->mi_flags & MIF_INT) &&
							   is_biod()   &&
							   which == RFS_READ)) {
				tryagain = TRUE;
				timeo = backoff(timeo);
				if (!(mi->mi_flags & MIF_PRINTED) &&
				    !(atomicSetUint(&mi->mi_flags, MIF_PRINTED) & MIF_PRINTED))
				{
					printf("NFS server %s not responding still trying\n",
					       mi->mi_hostname);
				}
			}
		}
	} while (tryagain);

	if (status != RPC_SUCCESS) {
		CLNT_GETERR(client, &rpcerr);
		/* Don't print a message if interrupted */
		if ((status != RPC_INTR) && !((mi->mi_flags & MIF_INT) && 
					      is_biod() && 
					      which == RFS_READ)) {
			
			if (mi->mi_vers == NFS3_VERSION) 
				CLSTAT3.nbadcalls++;
			else
				CLSTAT.nbadcalls++;
			atomicSetUint(&mi->mi_flags, MIF_DOWN);

			printf("NFS%d %s failed for server %s: %s\n", 
				mi->mi_vers,
				mi->mi_rfsnames ? mi->mi_rfsnames[which] : "unknown",
				mi->mi_hostname ? mi->mi_hostname : "unknown",
				clnt_sperrno(status));
		}
	} else if (resp && *(int *)resp == EACCES &&
	    newcred == NULL && cred->cr_uid == 0 && cred->cr_ruid != 0) {
		/*
		 * Boy is this a kludge!  If the reply status is EACCES
		 * it may be because we are root (no root net access).
		 * Check the real uid, if it isn't root make that
		 * the uid instead and retry the call.
		 */
		newcred = crdup(cred);
		cred = newcred;
		cred->cr_uid = cred->cr_ruid;
		clfree(client);
		goto retry;
	} else if (mi->mi_flags & MIF_HARD) {
		if ((mi->mi_flags & MIF_PRINTED) &&
		    (atomicClearUint(&mi->mi_flags, MIF_PRINTED) & MIF_PRINTED))
		{
			printf("NFS server %s ok\n", mi->mi_hostname);
		}
	} else {
		/*
		 * Need to clear printed so that nohang processes will
		 * retry.
		 */
		atomicClearUint(&mi->mi_flags, MIF_PRINTED|MIF_DOWN);
	}

out:
	clfree(client);
	if (newcred) {
		crfree(newcred);
	}
	return (rpcerr.re_errno);
}

/*
 * Copy some nfs file attributes into the file's vnode.
 */
void
nattr_to_iattr(
	struct nfsfattr *na,
	bhv_desc_t *bdp)
{
	vnode_t *vp;
	struct rnode *rp;
	enum vtype newtype;

	rp = bhvtor(bdp);
	vp = BHV_TO_VNODE(bdp);

	/*
	 * The automounter changes symlinks (direct-map roots) back into
	 * directories before attempting to unmount them.
	 */
	newtype = n2v_type(na);
	if (newtype != vp->v_type) {
		if (vp->v_type == VLNK && rp->r_symval) {
			kmem_free(rp->r_symval, rp->r_symlen);
			rp->r_symval = NULL;
			ASSERT(rp->r_unldrp == NULL);
		}
		vp->v_type = newtype;
	}

	/*
	 * If someone on the server or another client grew the file or
	 * truncated it, use the attributed size.  Otherwise, we changed
	 * r_size, so make na_size consistent.
	 */
	if (rp->r_size < na->na_size ||
	    (!(rp->r_flags & RDIRTY) && !(rp->r_bds_flags & RBDS_DIRTY))) {
		rp->r_size = na->na_size;
	} else {
		na->na_size = rp->r_size;
	}
	vp->v_rdev = n2v_rdev(na);
}

/* nfs_exp - expand old device format to new device format.
 *	However, allow 8 bits of major number.
 *
 *	Uncompress the received dev_t if its top 16 bits are
 *	zero, indicating an old style reply.  Note that the
 *	reply may come from an older NFS server or an NFS
 *	server implementing this hack.  If the major number
 *	is zero, the minor number must not be greater than 255.
 *  NOTE: If the upper 16 bits are 0xffff, we assume the server
 *	has sign extended.
 */
dev_t
nfs_expdev(dev_t dev)
{
	major_t major;
	minor_t minor;

	if (((dev & 0xffff0000) == 0) || ((dev & 0xffff0000) == 0xffff0000)) {
		major = ((dev >> ONBITSMINOR) & (0x80|OMAXMAJ));
		minor = (dev & OMAXMIN);
		return(((major << NBITSMINOR) | minor));
	} else {
		return(dev);
	}
}


/*
 * nfs_cmpdev - compress new device format to old device format
 *	but don't perform the scsi mapping in cmpdev()
 *
 * If the dev_t will fit into 16 bits, then compress it; otherwise,
 *	copy it as is.
 */
dev_t
nfs_cmpdev(dev_t dev)
{
	major_t major;
	minor_t minor;

	major = (dev >> NBITSMINOR);
	minor = (dev & MAXMIN);
	if (major > (0x80|OMAXMAJ) || minor > OMAXMIN)
		return(dev);
	else
		return(((major << ONBITSMINOR) | minor)); 
}


/*
 * Translate nfs file attributes to vnode attributes.  This routine must
 * be called after calling nattr_to_iattr, or the attributed size may be
 * inconsistent.
 */
void
nattr_to_vattr(rp, na, va)
	struct rnode *rp;
	struct nfsfattr *na;
	struct vattr *va;
{
	struct mntinfo *mi;

	va->va_type = ntype_to_vtype(na->na_type);
	va->va_mode = na->na_mode;
	va->va_uid = na->na_uid == NFS_UID_NOBODY ? UID_NOBODY : (uid_t)na->na_uid;
	va->va_gid = na->na_gid == NFS_GID_NOBODY ? GID_NOBODY : (gid_t)na->na_gid;
	mi = rtomi(rp);
	if (na->na_fsid == mi->mi_rootfsid) {
		va->va_fsid = rtov(rp)->v_vfsp->vfs_fsid.val[0];
	} else {
		va->va_fsid = make_local_fsid(na->na_fsid, mi, 
					      nfs_major, &nfs_minormap);
	}
	va->va_nodeid = na->na_nodeid;
	va->va_nlink = na->na_nlink;
	va->va_size = na->na_size;
	va->va_atime.tv_sec  = na->na_atime.tv_sec;
	va->va_atime.tv_nsec = na->na_atime.tv_usec * 1000;
	va->va_mtime.tv_sec  = na->na_mtime.tv_sec;
	va->va_mtime.tv_nsec = na->na_mtime.tv_usec * 1000;
	va->va_ctime.tv_sec  = na->na_ctime.tv_sec;
	va->va_ctime.tv_nsec = na->na_ctime.tv_usec * 1000;
        /*
         * Uncompress the received dev_t if its top 16 bits are
         * zero, indicating an old style reply.  Note that the
         * reply may come from an older NFS server or an NFS
         * server implementing this hack.  If the major number
         * is zero, the minor number must not be greater than
         * 255.
         */
	va->va_rdev = nfs_expdev(na->na_rdev);
	/*
	 * Report the block size for NFS, and report the number of blocks in
	 * terms of BBSIZE.  This conforms to the SVID.
	 */
	/*
	 * BDS code should've gone here to set va_blksize correctly.
	 * But how to determine whether to lock r_statelock? This
	 * routine can be called from either NFS 2 or 3 code.
	 */
	va->va_blksize = rtoblksz(rp);
	va->va_nblocks = BTOBB(na->na_size);
	/*
	 * This bit of ugliness is a *TEMPORARY* hack to preserve the
	 * over-the-wire protocols for named-pipe vnodes.  It remaps the
	 * special over-the-wire type to the VFIFO type. (see note in nfs.h)
	 *
	 * BUYER BEWARE:
	 *  If you are porting the NFS to a non-SUN server, you probably
	 *  don't want to include the following block of code.  The
	 *  over-the-wire special file types will be changing with the
	 *  NFS Protocol Revision.
	 */
	if (NA_ISFIFO(na)) {
		va->va_type = VFIFO;
		va->va_mode = (va->va_mode & MODEMASK) | S_IFIFO;
		va->va_rdev = NODEV;
	}
	/*
	 * Clear attributes we don't know anything about.
	 */
	va->va_vcode = 0;
	va->va_xflags = 0;
	va->va_extsize = 0;
	va->va_nextents = 0;
	va->va_anextents = 0;
	va->va_projid = 0;
	va->va_gencount = 0;
}

/*
 * Make an fsid suitable for return in a vattr struct that distinguishes
 * nodes having the given remote fsid from other nodes accessed under mi.
 * Used to give nohide-exported file trees a unique stat(2) st_dev.
 */
long
make_local_fsid(u_long remote, struct mntinfo *mi, 
		dev_t majorno, struct minormap *minormap)
{
	struct fsidmap *fsid;
	long local;
	int minor;

	milock(mi);
	for (fsid = mi->mi_fsidmaps; fsid; fsid = fsid->fsid_next) {
		if (fsid->fsid_remote == remote) {
			miunlock(mi);
			return fsid->fsid_local;
		}
	}
	fsid = (struct fsidmap *) kern_malloc(sizeof *fsid);
	fsid->fsid_remote = remote;
	if ((minor = vfs_getnum(minormap)) == -1) {
		printf ("Illegal minor 0x%x\n", minor);
	}
	local = makedev(majorno, minor);
	fsid->fsid_local = local;
	fsid->fsid_next = mi->mi_fsidmaps;
	mi->mi_fsidmaps = fsid;
	miunlock(mi);
	return local;
}

void
vattr_to_sattr(va, sa)
	struct vattr *va;
	struct nfssattr *sa;
{
	long mask = va->va_mask;

	sa->sa_mode = (mask & AT_MODE) ? va->va_mode : -1;
	sa->sa_uid = (mask & AT_UID) ? va->va_uid : -1;
	sa->sa_gid = (mask & AT_GID) ? va->va_gid : -1;
	sa->sa_size = (mask & AT_SIZE) ? va->va_size : -1;
	if (mask & AT_ATIME) {
		sa->sa_atime.tv_sec  = va->va_atime.tv_sec;
		sa->sa_atime.tv_usec = va->va_atime.tv_nsec / 1000;
	} else {
		sa->sa_atime.tv_sec = sa->sa_atime.tv_usec = -1;
	}
	if (mask & AT_MTIME) {
		sa->sa_mtime.tv_sec  = va->va_mtime.tv_sec;
		sa->sa_mtime.tv_usec = va->va_mtime.tv_nsec / 1000;
	} else {
		sa->sa_mtime.tv_sec = sa->sa_mtime.tv_usec = -1;
	}
}

void
setdiropargs(da, name, dbdp)
	struct nfsdiropargs *da;
	char *name;
	bhv_desc_t *dbdp;
{

	da->da_fhandle = *bhvtofh(dbdp);
	da->da_name = name;
}

gid_t
setdirgid(bhv_desc_t *dbdp)
{
	vnode_t *dvp;

	/*
	 * To determine the expected group-id of the created file:
	 *  1)	If the filesystem was not mounted with the Old-BSD-compatible
	 *	GRPID option, and the directory's set-gid bit is clear,
	 *	then use the process's gid.
	 *  2)	Otherwise, set the group-id to the gid of the parent directory.
	 */
	dvp = BHV_TO_VNODE(dbdp);
	if (!(dvp->v_vfsp->vfs_flag & VFS_GRPID) &&
	    !(bhvtor(dbdp)->r_nfsattr.na_mode & VSGID)) {
		return (get_current_cred()->cr_gid);
	}
	return (bhvtor(dbdp)->r_nfsattr.na_gid);
}

mode_t
setdirmode(bhv_desc_t *dbdp, mode_t om)
{

	/*
	 * Modify the expected mode (om) so that the set-gid bit matches
	 * that of the parent directory (dvp).
	 */
	om &= ~VSGID;
	if (bhvtor(dbdp)->r_nfsattr.na_mode & VSGID)
		om |= VSGID;
	return (om);
}

#define	PREFIXLEN	4
static char prefix[PREFIXLEN + 1] = ".nfs";

char *
newname()
{
	char *news;
	char *s1, *s2;
	unsigned int id;
	static unsigned int newnum;

	news = kmem_alloc(NFS_MAXNAMLEN, KM_SLEEP);
	for (s1 = news, s2 = prefix; s2 < &prefix[PREFIXLEN]; ) {
		*s1++ = *s2++;
	}
	if (newnum == 0) {
		newnum = lbolt;
	}
	id = newnum++;
	while (id) {
		*s1++ = "0123456789ABCDEF"[id & 0x0f];
		id = id >> 4;
	}
	*s1 = '\0';
	return (news);
}

/*
 * record a lock in the vnode
 */
int
nfs_reclock(
	vnode_t *vp,
	register struct flock *ld,
	int cmd,
	int flag,
	cred_t *cr)
{
	int error;

	NLM_DEBUG(NLMDEBUG_TRACE | NLMDEBUG_CALL,
		printf("nfs_reclock: vp %p pid %d off %lld len %lld\n",
			vp, ld->l_pid, ld->l_start, ld->l_len));
	/*
	 * It is essential that all requests block.  This is because the
	 * server may reply with a lock grant before replying to a cancel
	 * for the same lock.  We assume correctness on the part of the
	 * server (i.e., that it does not grant locks it should not have).
	 */
	switch (cmd) {
		case F_SETLK:
		case F_SETLKW:
			cmd = SETFLCK | SLPFLCK;
			break;
		case F_SETBSDLK:
		case F_SETBSDLKW:
			cmd = SETBSDFLCK | SLPFLCK;
			break;
		default:
			/*
			 * ignore the command if it is not for setting a lock
			 */
			return(0);
	}
	VOP_RWLOCK(vp, VRWLOCK_WRITE);
	error = reclock(vp, ld, cmd | INOFLCK, flag, (off_t)0, cr);
	VOP_RWUNLOCK(vp, VRWLOCK_WRITE);
	NLM_DEBUG(NLMDEBUG_ERROR,
		if (error && (error != EINTR))
			printf("nfs_reclock: vnode 0x%p, pid %d, return %d\n", vp,
				current_pid(), error));
	return(error);
}
