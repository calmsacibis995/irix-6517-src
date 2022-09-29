/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * +++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 * 		PROPRIETARY NOTICE (Combined)
 * 
 * This source code is unpublished proprietary information
 * constituting, or derived under license from AT&T's UNIX(r) System V.
 * In addition, portions of such source code were derived from Berkeley
 * 4.3 BSD under license from the Regents of the University of
 * California.
 * 
 * 
 * 
 * 		Copyright Notice 
 * 
 * Notice of copyright on this source code product does not indicate 
 * publication.
 * 
 * 	(c) 1986,1987,1988,1989  Sun Microsystems, Inc
 * 	(c) 1983,1984,1985,1986,1987,1988,1989  AT&T.
 * 	          All rights reserved.
 *  
 */

#ident	"$Revision: 1.46 $"

#include <sys/types.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/pathname.h>
#include <sys/proc.h>
#include <sys/sysinfo.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/vfs.h>
#include <sys/vnode_private.h>
#include <sys/sat.h>
#include <sys/mbuf.h>
#include <string.h>
#include <sys/ckpt.h>

/*
 * Lookup the user file name,
 * Handle allocation and freeing of pathname buffer, return error.
 */
int
lookupname(char *fnamep,		/* user pathname */
	enum uio_seg seg,		/* addr space that name is in */
	enum symfollow followlink,	/* follow sym links */
	vnode_t **dirvpp,		/* ret for ptr to parent dir vnode */
	vnode_t **compvpp,		/* ret for ptr to component vnode */
	ckpt_handle_t *ckpt)		/* ret for ckpt lookup info */
{
	struct pathname lookpn;
	register int error;

	if (error = pn_get(fnamep, seg, &lookpn))
		return error;
	_SAT_PN_SAVE(&lookpn, curuthread);
	error = lookuppn(&lookpn, followlink, dirvpp, compvpp, ckpt);
	pn_free(&lookpn);
	return error;
}

/*
 * Starting at current directory, translate pathname pnp to end.
 * Leave pathname of final component in pnp, return the vnode
 * for the final component in *compvpp, and return the vnode
 * for the parent of the final component in dirvpp.
 *
 * This is the central routine in pathname translation and handles
 * multiple components in pathnames, separating them at /'s.  It also
 * implements mounted file systems and processes symbolic links.
 */
int
lookuppn(register struct pathname *pnp,	/* pathname to lookup */
	enum symfollow followlink,	/* (don't) follow sym links */
	vnode_t **dirvpp,		/* ptr for parent vnode */
	vnode_t **compvpp,		/* ptr for entry vnode */
	ckpt_handle_t *ckpt)		/* ptr for ckpt */
{
	vnode_t *vp;			/* current directory vp */
	vnode_t *cvp = NULLVP;		/* current component vp */
	vnode_t *tvp;			/* addressable temp ptr */
	char component[MAXNAMELEN];	/* buffer for component (incl null) */
	int error;
	int nlink = 0;
	int lookup_flags = dirvpp ? LOOKUP_DIR : 0;
	vnode_t *rdir;
	cred_t *cr = get_current_cred();

	/* use this root dir */
	rdir = curuthread->ut_rdir ? curuthread->ut_rdir : rootdir;

	SYSINFO.namei++;

	/*
	 * Start at current directory.
	 */
	vp = curuthread->ut_cdir;
	VN_HOLD(vp);

	_SAT_PN_START(curuthread);
begin:
	/*
	 * Disallow the empty path name.
	 */
	if (pnp->pn_pathlen == 0) {
		error = ENOENT;
		goto bad;
	}

	/*
	 * Each time we begin a new name interpretation (e.g.
	 * when first called and after each symbolic link is
	 * substituted), we allow the search to start at the
	 * root directory if the name starts with a '/', otherwise
	 * continuing from the current directory.
	 */
	if (*pnp->pn_path == '/') {
		VN_RELE(vp);
		pn_skipslash(pnp);
		vp = rdir;
		_SAT_PN_APPEND( "/" , curuthread);
		VN_HOLD(vp);
	}

	/*
	 * Eliminate any trailing slashes in the pathname.
	 */
	pn_fixslash(pnp);

next:
	/*
	 * Make sure we have a directory.
	 */
	if (vp->v_type != VDIR) {
		error = ENOTDIR;
		goto bad;
	}

	/*
	 * Process the next component of the pathname.
	 */
	if (error = pn_stripcomponent(pnp, component))
		goto bad;

	/*
	 * Check for degenerate name (e.g. / or "")
	 * which is a way of talking about a directory,
	 * e.g. "/." or ".".
	 */
	if (component[0] == 0) {
		/*
		 * If the caller was interested in the parent then
		 * return an error since we don't have the real parent.
		 */
		_SAT_PN_APPEND( "." , curuthread);
		_SAT_PN_FINALIZE(vp, curuthread);
		if (dirvpp != NULL) {
			VN_RELE(vp);
			if (curuthread->ut_syscallno !=
						SAT_SYSCALL_RENAME &&
				curuthread->ut_syscallno !=
						SAT_SYSCALL_LINK)
				_SAT_ACCESS(SAT_ACCESS_FAILED, EINVAL);
			return EINVAL;
		}
		(void) pn_set(pnp, ".");
		if (compvpp != NULL) 
			*compvpp = vp;
		else
			VN_RELE(vp);
		return 0;
	}

	/*
	 * Handle "..": two special cases.
	 * 1. If we're at the root directory (e.g. after chroot)
	 *    then ignore ".." so we can't get out of this subtree.
	 * 2. If this vnode is the root of a mounted file system,
	 *    then replace it with the vnode that was mounted on
	      so that we take the ".." in the other file system.
	 */
	if (component[0] == '.' && component[1] == '.' && component[2] == 0) {
checkforroot:
		if (VN_CMP(vp, curuthread->ut_rdir) || VN_CMP(vp, rootdir)) {
			cvp = vp;
			VN_HOLD(cvp);
			goto skip;
		}
		if (vp->v_flag & VROOT) {
			cvp = vp;
			vp = vp->v_vfsp->vfs_vnodecovered;
			VN_HOLD(vp);
			VN_RELE(cvp);
			goto checkforroot;
		}
	}

	VOP_LOOKUP(vp, component, &tvp, pnp, lookup_flags, rdir, cr, error);
	_SAT_PN_APPEND( component , curuthread);

	if (error == 0)
		cvp = tvp;
	else {
		cvp = NULL;
		/*
		 * On error, return hard error if
		 * (a) we're not at the end of the pathname yet, or
		 * (b) the caller didn't want the parent directory, or
		 * (c) we failed for some reason other than a missing entry.
		 */
		if (pn_pathleft(pnp) || dirvpp == NULL || error != ENOENT)
			goto bad;
		pn_setlast(pnp);

#if CKPT
		if (ckpt)
			*ckpt = ckpt_lookup(vp);
#endif

		*dirvpp = vp;
		if (compvpp != NULL)
			*compvpp = NULL;
		_SAT_PN_FINALIZE(NULL, curuthread);

		return 0;
	}

	/* pdc -- ugly, isn't it? */
	if (sat_enabled) {
		if (cvp->v_type == VLNK)
			_SAT_PN_APPEND( "/@" , curuthread);
		else if (cvp->v_vfsmountedhere != NULL)
			_SAT_PN_APPEND( "/!" , curuthread);
#ifdef NOTYET
		else if (ip->i_label && mac_is_moldy(ip->i_label))
			_SAT_PN_APPEND( "/>" , curuthread);
#endif
		else if (cvp->v_type == VDIR)
			_SAT_PN_APPEND( "//" , curuthread);
	}

	/*
	 * Traverse mount points.
	 *
	 * We do not call VN_LOCK in order to test cvp->v_vfsmountedhere
	 * because either it's about to be set after this test by a racing
	 * mount, but that mount will fail because cvp has more than one
	 * reference; or it's about to be cleared, in which case traverse
	 * will do nothing (possibly synchronizing via vfs_mlock).
	 */
	if (cvp->v_vfsmountedhere != NULL) {
		tvp = cvp;
		if ((error = traverse(&tvp)) != 0)
			goto bad;
		cvp = tvp;
	}

	/*
	 * Identify and process a moldy directory in the MAC configuration.
	 * Works much like the symlink processing below, prepending
	 * an additional component to the path if appropriate.
	 * The "-1" error return is a bit hackish.
	 */
	if ((error = _MAC_MOLDY_PATH(cvp, component, pnp, cr)) == -1) {
		VN_RELE(vp);
		vp = cvp;
		cvp = NULL;
		goto begin;
	}
	else if (error)
		goto bad;

	/*
	 * If we hit a symbolic link and there is more path to be
	 * translated or this operation does not wish to apply
	 * to a link, then place the contents of the link at the
	 * front of the remaining pathname.
	 */
	if (cvp->v_type == VLNK
	    && (followlink == FOLLOW || pn_pathleft(pnp))) {
		struct pathname linkpath;
		extern int maxsymlinks;

		if (++nlink > maxsymlinks) {
			error = ELOOP;
			goto bad;
		}
		pn_alloc(&linkpath);
		if (error = pn_getsymlink(cvp, &linkpath, cr)) {
			pn_free(&linkpath);
			goto bad;
		}
		if (pn_pathleft(&linkpath) == 0)
			(void) pn_set(&linkpath, ".");
		error = pn_insert(pnp, &linkpath);	/* linkpath before pn */
		pn_free(&linkpath);
		if (error)
			goto bad;
		VN_RELE(cvp);
		cvp = NULL;
		goto begin;
	}

skip:
	/*
	 * If no more components, return last directory (if wanted) and
	 * last component (if wanted).
	 */
	if (pn_pathleft(pnp) == 0) {
		pn_setlast(pnp);
		_SAT_PN_FINALIZE(cvp, curuthread);

#if CKPT
		if (ckpt)
			*ckpt = ckpt_lookup(vp);
#endif

		if (dirvpp != NULL) {
			/*
			 * Check that we have the real parent and not
			 * an alias of the last component.
			 */
			if (VN_CMP(vp, cvp)) {
				VN_RELE(vp);
				VN_RELE(cvp);

				if (curuthread->ut_syscallno !=
						SAT_SYSCALL_RENAME &&
					curuthread->ut_syscallno !=
						SAT_SYSCALL_LINK)
					_SAT_ACCESS(SAT_ACCESS_FAILED, EINVAL);

#if CKPT
				if (ckpt && *ckpt)
					ckpt_lookup_free(*ckpt);
#endif

				return EINVAL;
			}
			*dirvpp = vp;
		} else
			VN_RELE(vp);
		if (compvpp != NULL)
			*compvpp = cvp;
		else
			VN_RELE(cvp);
		return 0;
	}

	/*
	 * Skip over slashes from end of last component.
	 */ 
	pn_skipslash(pnp);

	/*
	 * Searched through another level of directory:
	 * release previous directory handle and save new (result
	 * of lookup) as current directory.
	 */
	VN_RELE(vp);
	vp = cvp;
	cvp = NULL;
	goto next;

bad:
	/*
	 * Error.  Release vnodes and return.
	 */
	if (cvp)
		VN_RELE(cvp);
	VN_RELE(vp);
	_SAT_PN_FINALIZE(NULL, curuthread);
	/*
	 * If there was an error not involving multiple pathnames,
	 * automatically generate an access_failed or access_denied
	 * record (sat_access decides which).  This also clears
	 * u_satrec.
	 *
	 * Multiple-pathname system calls need to handle errors
	 * themselves, so we don't do this automatically for them.
	 */
	if (curuthread->ut_syscallno != SAT_SYSCALL_RENAME
	    && curuthread->ut_syscallno != SAT_SYSCALL_LINK)
		_SAT_ACCESS(SAT_ACCESS_FAILED, error);

	return error;
}

/*
 * Traverse a mount point.  Routine accepts a vnode pointer as a reference
 * parameter and performs the indirection, releasing the original vnode.
 */
int
traverse(vnode_t **cvpp)
{
	register struct vfs *vfsp;
	register int error;
	register vnode_t *cvp;
	int s;
	vnode_t *tvp;

	cvp = *cvpp;

	/*
	 * If this vnode is mounted on, then we transparently indirect
	 * to the vnode that is the root of the mounted file system.
	 * Before we do this we must check that an unmount is not in
	 * progress on this vnode.
	 */
	s = VN_LOCK(cvp);
	while ((vfsp = cvp->v_vfsmountedhere) != NULL) {
		nested_spinlock(&vfslock);
		NESTED_VN_UNLOCK(cvp);
		if (vfsp->vfs_flag & (VFS_MLOCK|VFS_MWANT)) {
			ASSERT(vfsp->vfs_flag & VFS_MWANT ||
			       vfsp->vfs_busycnt == 0);
			vfsp->vfs_flag |= VFS_MWAIT;
			sv_wait(&vfsp->vfs_wait, PZERO, &vfslock, s);
			s = VN_LOCK(cvp);
			continue;
		}

		/*
		 * Emulate vfs_busycnt here so the filesystem is not unmounted
		 * while VFS_ROOT executes (uniprocessor vnodes assumes that
		 * VFS_ROOT does not sleep).
		 */
		ASSERT(vfsp->vfs_busycnt >= 0);
		vfsp->vfs_busycnt++;
		mutex_spinunlock(&vfslock, s);
		VFS_ROOT(vfsp, &tvp, error);
		vfs_unbusy(vfsp);

		if (error)
			return error;
		VN_RELE(cvp);
		cvp = tvp;
		s = VN_LOCK(cvp);
	}
	VN_UNLOCK(cvp, s);

	*cvpp = cvp;
	return 0;
}
