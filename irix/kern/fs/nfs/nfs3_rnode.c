/*
 * SGI rnode implementation.
 */
#ident  "$Revision: 1.32 $"

#include "types.h"
#include <sys/debug.h>          /* for ASSERT and METER */
#include <sys/cred.h>
#include <sys/systm.h>          /* for splhi */
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/sysmacros.h>
#include <sys/acl.h>
#include <sys/cmn_err.h>
#include <sys/pda.h>
#include <ksys/vproc.h>
#include "nfs.h"
#include "nfs_clnt.h"
#include "nfs3_clnt.h"
#include "rnode.h"
#include "nfs3_rnode.h"
#include "auth.h"
#include "svc.h"
#include "xattr.h"

#ifdef r_fh
#undef r_fh
#endif
#define r_fh	r_ufh.r_fh3
#define	RTOV	rtov

extern struct vnodeops nfs3_vnodeops;
extern struct vnodeops nfs_vnodeops;
extern vtype_t nf3_to_vt[];

extern void	nfs3_cache_check(bhv_desc_t *,
		    struct wcc_attr *, cred_t *);
extern void	nfs3_cache_check_fattr3(bhv_desc_t *,
		    struct fattr3 *, cred_t *);
extern int	nfs3_attrcache(bhv_desc_t *, struct fattr3 *, 
		    enum staleflush, int *);

void	rinactive(rnode_t *rp);

/*
 * Free the resources associated with an rnode.
 */
void
rinactive(rnode_t *rp)
{
	cred_t *cred;
	access_cache *acp, *nacp;
	rddir_cache *rdc, *nrdc;
	char *contents;
	xattr_cache_t *eap;
	int size;

	/*
	 * Free any held credentials and disconnect any access cache
	 * and/or directory entry cache entries from the rnode.
	 */
	mutex_enter(&rp->r_statelock);
	cred = rp->r_cred;
	rp->r_cred = NULL;
	acp = rp->r_acc;
	rp->r_acc = NULL;
	rdc = rp->r_dir;
	rp->r_dir = NULL;
	rp->r_direof = NULL;
	contents = rp->r_symlink.contents;
	eap = rp->r_xattr;
	rp->r_xattr = NULL;
	size = rp->r_symlink.size;
	rp->r_symlink.contents = NULL;
	mutex_exit(&rp->r_statelock);

	/*
	 * Free the held credential.
	 */
	if (cred != NULL)
		crfree(cred);
	
	/*
	 * Free the access cache entries.
	 */
	while (acp != NULL) {
		crfree(acp->cred);
		nacp = acp->next;
		kmem_free((caddr_t)acp, sizeof (*acp));
		acp = nacp;
	}

	/*
	 * Free the readdir cache entries.
	 */
	while (rdc != NULL) {
		nrdc = rdc->next;
		mutex_enter(&rp->r_statelock);
		while (rdc->flags & RDDIR) {
			rdc->flags |= RDDIRWAIT;
			sv_wait(&rdc->cv, PZERO-1, &rp->r_statelock, 0);
			mutex_lock(&rp->r_statelock, PZERO);
		}
		mutex_exit(&rp->r_statelock);
		if (rdc->entries != NULL)
			kmem_free(rdc->entries, rdc->buflen);
		sv_destroy(&rdc->cv);
		kmem_free((caddr_t)rdc, sizeof (*rdc));
		rdc = nrdc;
	}

	/*
	 * Free the symbolic link cache.
	 */
	if (contents != NULL)
		kmem_free(contents, size);

	/*
	 * Free the extended attribute cache.
	 */
	if (eap != NULL)
		xattr_cache_free(eap);
}

/*
 * Return a vnode for the given NFS Version 3 file handle.
 * If no rnode exists for this fhandle, create one and put it
 * into the hash queues.  If the rnode for this fhandle
 * already exists, return it.
 */
/* ARGSUSED */
int
makenfs3node(nfs_fh3 *fh3, struct fattr3 *attr, 
		struct vfs *vfsp, bhv_desc_t **bdpp, cred_t *cr)
{
	int newnode = 0;
	int status  = 0;
	vnode_t *vp;
	bhv_desc_t *bdp;
	bhv_desc_t *vfs_bdp;
	struct rnode *rp;
	nfs_fhandle *fh = (nfs_fhandle *)fh3;

	vfs_bdp = bhv_base_unlocked(VFS_BHVHEAD(vfsp));

	if (attr && ((attr->type < NF3NONE) || (attr->type > NF3FIFO))) {
		cmn_err(CE_WARN, "Bad attribute type %d from server %s\n",
			attr->type, vfs_bhvtomi(vfs_bdp)->mi_hostname);
		return(EINVAL);
	}

	mutex_lock(&rnodemonitor, PINOD);
	(void)make_rnode((fhandle_t *)fh, vfsp, &nfs3_vnodeops, &newnode,
			   vfs_bhvtomi(vfs_bdp), &bdp, NULL);

	if (bdp == NULL) {
#ifdef DEBUG
		printf("bad vp in makenfs3node\n");
#endif /* DEBUG */
		mutex_unlock(&rnodemonitor);
		return (EAGAIN);
	}

	vp = BHV_TO_VNODE(bdp);
	rp = bhvtor(bdp);

	/* if this is a new rnode, we need to initialize its attribute info */
	if (newnode) {
		rsetflag(rp, RV3);
		bzero((caddr_t)&rp->r_attr, sizeof(rp->r_attr));
		rp->r_xattr = NULL;
		rp->r_fh.fh_len = fh->fh_len;
		bzero(rp->r_cookieverf, sizeof(cookieverf3));
		bcopy(fh->fh_buf, rp->r_fh.fh_buf, fh->fh_len);
		vp = rtov(rp);
		ASSERT(rp->r_fh.fh_len == fh->fh_len);
		ASSERT(vp == rtov(rp));
		if (attr != NULL) {
			vp->v_type = nf3_to_vt[attr->type];
			vp->v_rdev = makedevice(attr->rdev.specdata1,
						attr->rdev.specdata2);
			status = nfs3_attrcache(bdp, attr, NOFLUSH, NULL);
			mutex_unlock(&rnodemonitor);
			if (status) {
	#ifdef DEBUG
				printf("Bad attrib caching in makenfs3node\n");
	#endif /* DEBUG */
				return(EAGAIN);
			}
		} else {
			mutex_unlock(&rnodemonitor);
		}
	} else {
		mutex_unlock(&rnodemonitor);
		ASSERT(rp->r_fh.fh_len == fh->fh_len);
		ASSERT(vp == rtov(rp));
	}

	*bdpp = bdp;


	return (0);
}
