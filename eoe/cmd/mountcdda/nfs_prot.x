/*
 * Wrapper for the standard nfs protocol spec.
 */
#include <rpcsvc/nfs_prot.x>

/*
 *	sgi_nfs protocol: the moungfs port implements the NFS protocol,
 *	and in addition the function SGI_NFSPROC_ROOT, to get the root
 *	file handle of a file system.
 */

union rootres switch (nfsstat status) {
case NFS_OK:
	nfs_fh	file;
default:
	void;
};
	
program SGI_NFS_PROGRAM {
	version SGI_NFS_VERSION {
		void
		SGI_NFSPROC_NULL(void) = 0;

		rootres
		SGI_NFSPROC_ROOT(nfspath) = 3;
	} = 1;
} = 391007;
