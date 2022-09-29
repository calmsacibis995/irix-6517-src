#ident	"$Revision: 1.1 $"

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/vnode.h>
#include "types.h"
#include "nfs.h"
#include "nfs_clnt.h"
#include "nfs_stat.h"
#include "nfs3_clnt.h"
#include "nfs3_rnode.h"
#include "mount.h"
#include "xdr.h"
#include "rnode.h"
#include "bootparam.h"
#include "auth.h"
#include "clnt.h"
#include "svc.h"
#include "svc_auth.h"
#include "xattr.h"
#include "bds.h"
#include "export.h"
#include "lockmgr.h"
#include "lockd_impl.h"


/* Structure contains fields that are pointers to key kernel data 
 * structures. This forces the type information to be sucked into 
 * the kernel symbol table.
 */
typedef struct nfs_icrash_s {

	SVCXPRT				*nfs_icrash0;

	fhandle_t			*nfs_icrash1;
	lockhandle_t			*nfs_icrash2;
	lockreq_t			*nfs_icrash3;
	nlm_wait_t			*nfs_icrash4;

	CLIENT				*nfs_icrash5;
		
	bdsattr_t			*nfs_icrash6;
	bds_flock64_t			*nfs_icrash7;
	bds_msg				*nfs_icrash8;

	access_cache			*nfs_icrash9;
	rddir_cache			*nfs_icrash10;
	
	struct exportinfo		*nfs_icrash11;
	mntinfo_t			*nfs_icrash12;
	rnode_t                         *nfs_icrash13;

	nfs_fhandle			*nfs_icrash14;
	symlink_cache			*nfs_icrash15;
	commit_t			*nfs_icrash16;
	xattr_cache_t			*nfs_icrash17;

	XDR				*nfs_icrash18;
	netobj				*nfs_icrash19;

} nfs_icrash_t;

nfs_icrash_t *nfs_icrash_struct;

/* Dummy function called by icrash_init() to ensure that symbol
 * information from this module gets included in the kernel
 * symbol table.
 */
void
nfs_icrash(void)
{
}

