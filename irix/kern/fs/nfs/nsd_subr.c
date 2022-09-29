
#include "types.h"
#include <sys/cred.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <rpc/xdr.h>
#include <sys/proc.h>

#include "auth.h"
#include "clnt.h"
#include "xdr.h"
#include "nfs.h"
#include "nfs_clnt.h"
#include "nfs_stat.h"
#include "rnode.h"
#include "svc.h"
#include "nsd.h"

extern CLIENT *nclget(struct mntinfo *, struct cred *);
extern void    nclfree(CLIENT *cl);
static char *nsdnames_v1[] = { "null", "close" };

/*
** This is just a highly stripped version of rfscall for nsd.
*/
int
nsd_rfscall(mi, which, xdrargs, argsp, xdrres, resp, cred)
	struct mntinfo *mi;
	int	 which;
	xdrproc_t xdrargs;
	caddr_t	argsp;
	xdrproc_t xdrres;
	caddr_t	resp;
	struct cred *cred;
{
	CLIENT *client;
	enum clnt_stat status;
	struct rpc_err rpcerr;
	struct timeval wait;
	int timeo;
	struct mntinfo mzb, *mz = &mzb;
	
	/*
	** Make local copy of mntinfo structure.
	*/
	*mz = *mi;
	mz->mi_prog = NSDPROGRAM;
	mz->mi_vers = NSDVERSION;
	mz->mi_rfsnames = nsdnames_v1;

	if (mi->mi_flags & MIF_STALE) {
		return (ESTALE);
	}
	if (mi->mi_vers == NFS3_VERSION) {
		CLSTAT3.ncalls++;
	} else {
		CLSTAT.ncalls++;
	}

	rpcerr.re_errno = 0;
	timeo = mz->mi_timeo;
retry:
	client = nclget(mz, cred);
	/*
	 * If clget returns a NULL client handle, it is because clntkudp_create
	 * failed.  Rather than panic, we just return the appropriate error
	 * code which was set by clntkudp_create.
	 */
	if (!client) {
		return(rpc_createerr.cf_error.re_errno);
	}

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
		ASSERT(mz->mi_flags & MIF_INT);
		break;

	default:
		if (nohang()) {
			rpcerr.re_errno = ETIMEDOUT;
			goto out;
		}
		if (mz->mi_flags & MIF_STALE) {
			rpcerr.re_errno = ESTALE;
			goto out;
		}
	}

	if (status != RPC_SUCCESS) {
		CLNT_GETERR(client, &rpcerr);
		atomicSetUint(&mi->mi_flags, MIF_DOWN);
	}

out:
	nclfree(client);
	return (rpcerr.re_errno);
}

/*
** Nsd treats the handle as an nfs3 handle, so here we convert from a
** version 2 style handle to a version 3 one.
*/
bool_t
xdr_nfs_nsdfh(register XDR *xdrs, fhandle_t *objp)
{

	unsigned x = NFS_FHSIZE;

	if (!xdr_u_int(xdrs, &x)) {
		return (FALSE);
	}

	if (xdrs->x_op == XDR_DECODE || xdrs->x_op == XDR_ENCODE) {
		return (xdr_opaque(xdrs, (caddr_t)objp, NFS_FHSIZE));
	}

	if (xdrs->x_op == XDR_FREE)
		return (TRUE);

	return (FALSE);
}
