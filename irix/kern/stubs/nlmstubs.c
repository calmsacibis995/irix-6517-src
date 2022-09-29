#include <sys/types.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include "fs/nfs/types.h"
#include "fs/nfs/auth.h"
#include "fs/nfs/auth_unix.h"
#include "fs/nfs/svc.h"
#include "fs/nfs/xdr.h"
#include "fs/nfs/nfs.h"
#include "fs/nfs/export.h"
#include "fs/nfs/nfs_impl.h"

int
release_all_locks()
{
	return 0;
}

void
lockd_cleanlocks()
{
}

void
share_init()
{
}

void
nlmsvc_init()
{
}

int
nlm_test_auth()
{
	return(1);
}

int
proc_nlm_test()
{
	return(NFS_RPC_PROC);
}

int
nlm_lock_auth()
{
	return(1);
}

int
proc_nlm_lock()
{
	return(NFS_RPC_PROC);
}

int
nlm_unlock_auth()
{
	return(1);
}

int
proc_nlm_unlock()
{
	return(NFS_RPC_PROC);
}

int
nlm_cancel_auth()
{
	return(1);
}

int
proc_nlm_cancel()
{
	return(NFS_RPC_PROC);
}

int
nlm_free_all()
{
	return(NFS_RPC_PROC);
}

int
nlm4_test_auth()
{
	return(1);
}

int
proc_nlm4_test()
{
	return(NFS_RPC_PROC);
}

int
nlm4_lock_auth()
{
	return(1);
}

int
proc_nlm4_lock()
{
	return(NFS_RPC_PROC);
}

int
nlm4_unlock_auth()
{
	return(1);
}

int
proc_nlm4_unlock()
{
	return(NFS_RPC_PROC);
}

int
nlm4_cancel_auth()
{
	return(1);
}

int
proc_nlm4_cancel()
{
	return(NFS_RPC_PROC);
}

int
proc_nlm4_share()
{
	return(NFS_RPC_PROC);
}

int
nlm4_share_auth()
{
	return(1);
}

int
nlm4_free_all()
{
	return(NFS_RPC_PROC);
}

int
nlm_reply()
{
	return 0;
}

void
export_unlock()
{
}
