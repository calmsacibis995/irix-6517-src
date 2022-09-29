#ident "$Revision: 1.4 $"

#include "types.h"
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/mkdev.h>
#include <sys/param.h>
#include <sys/pda.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/cmn_err.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include "auth.h"		/* XXX required before clnt.h */
#include "clnt.h"
#include "xdr.h"
#include "nfs.h"
#include "nfs_clnt.h"
#include "rnode.h"
#include "netinet/in.h"
#include "nfs_stat.h"
#include "export.h"
#include <rpcsvc/nlm_prot.h>
#include "nlm_debug.h"
#include "auth.h"
#include "svc.h"
#include "xattr.h"

#define MAXTIMO	(60 * HZ)
#define backoff(tim)	((((tim) << 1) > MAXTIMO) ? MAXTIMO : ((tim) << 1))

extern CLIENT *xclget(struct mntinfo *, struct cred *);
extern void xclfree(CLIENT *);

static int
xattrsub (struct mntinfo *mi, int which, xdrproc_ansi_t xdrargs, caddr_t argsp,
	  xdrproc_ansi_t xdrres, caddr_t resp, struct cred *cred,
	  u_long newprog, u_long newversion, char **newnames)
{
	CLIENT *client;
	enum clnt_stat status;
	struct rpc_err rpcerr;
	struct timeval wait;
	struct cred *newcred;
	int timeo;
	bool_t tryagain;
	struct mntinfo mzb, *mz = &mzb;

	/* copy mz */
	*mz = *mi;

	/* set new parameters */
	mz->mi_prog = newprog;
	mz->mi_vers = newversion;
	mz->mi_rfsnames = newnames;
	
	if (mz->mi_flags & MIF_TCP)
		return(rfscall_tcp(mz, which, xdrargs, argsp, 
				   xdrres, resp, cred, 0));
		
	rpcerr.re_errno = 0;
	newcred = NULL;
	timeo = mi->mi_timeo;

retry:
	/* retrieve client */
	client = xclget(mz, cred);

	/*
	 * If xclget returns a NULL client handle, it is because
	 * clntkudp_create failed.  Rather than panic, we just return the
	 * appropriate error code which was set by clntkudp_create.
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

		default:
			if ((mi->mi_flags & MIF_HARD|MIF_INT) == MIF_HARD) {
				tryagain = TRUE;
				timeo = backoff (timeo);
				if (!(mi->mi_flags & MIF_PRINTED) &&
				    !(atomicSetUint(&mi->mi_flags, MIF_PRINTED) & MIF_PRINTED))
				{
					printf("XATTR server %s not responding still trying\n", mi->mi_hostname ? mi->mi_hostname : "unknown");
				}
			}
			break;
		}
	} while (tryagain);

	if (status != RPC_SUCCESS) {
		CLNT_GETERR(client, &rpcerr);
		if (status != RPC_INTR && !(mi->mi_flags & MIF_INT)) {
			atomicSetUint(&mi->mi_flags, MIF_DOWN);
			printf("XATTR%d %s failed for server %s: %s\n",
			       mz->mi_vers,
			       mz->mi_rfsnames ? mz->mi_rfsnames[which] : "unknown",
			       mi->mi_hostname ? mi->mi_hostname : "unknown",
			       clnt_sperrno(status));
		}
	} else if (resp && *(int *)resp == EACCES && newcred == NULL &&
		   cred->cr_uid == 0 && cred->cr_ruid != 0) {
		/*
		 * Boy is this a kludge!  If the reply status is EACCES
		 * it may be because we are root (no root net access).
		 * Check the real uid, if it isn't root make that
		 * the uid instead and retry the call.
		 */
		newcred = crdup(cred);
		cred = newcred;
		cred->cr_uid = cred->cr_ruid;
		xclfree(client);
		goto retry;
	} else if (mi->mi_flags & MIF_HARD) {
		if ((mi->mi_flags & MIF_PRINTED) &&
		    (atomicClearUint(&mi->mi_flags, MIF_PRINTED) & MIF_PRINTED))
		{
			printf("XATTR server %s ok\n", mi->mi_hostname);
		}
	} else if (mi->mi_flags & MIF_DOWN) {
		atomicClearUint(&mi->mi_flags, MIF_DOWN);
	}

	xclfree(client);
	if (newcred) {
		crfree(newcred);
	}

	return (rpcerr.re_errno);
}

static char *xattrnames_v1[] = {
	"null", "getxattr", "setxattr", "rmxattr", "listxattr",
};

static long xattr_jukebox_delay = 10L * HZ;

int
xattrcall (struct mntinfo *mi, int which,
	   xdrproc_ansi_t xdrargs, caddr_t argsp,
	   xdrproc_ansi_t xdrres, caddr_t resp,
	   struct cred *cred, enum nfsstat3 *statusp)
{
	int rpcerror;
	int user_informed = 0;
	
	do {
		rpcerror = xattrsub (mi, which, xdrargs, argsp, xdrres, resp,
				     cred, XATTR_PROGRAM, XATTR1_VERSION,
				     xattrnames_v1);
		if (!rpcerror) {
			if (*statusp == NFS3ERR_JUKEBOX) {
				if (!user_informed) {
					user_informed = 1;
					printf("file temporarily unavailable on the server, retrying...\n");
				}
				delay(xattr_jukebox_delay);
			}
		}
	} while (!rpcerror && *statusp == NFS3ERR_JUKEBOX);

	return (rpcerror);
}
