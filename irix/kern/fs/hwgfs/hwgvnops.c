/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ident	"$Revision: 1.50 $"

#include <sys/types.h>
#include <sys/attributes.h>
#include <sys/capability.h>
#include <sys/cmn_err.h>
#include <sys/kabi.h>
#include <sys/conf.h>
#include <sys/cred.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/dirent.h>
#include <sys/driver.h>
#include <sys/errno.h>
#include <ksys/vfile.h>
#include <sys/fs_subr.h>
#include <ksys/hwg.h>
#include <sys/immu.h>
#include <sys/invent.h>
#include <sys/iograph.h>
#include <sys/kmem.h>
#include <sys/nodepda.h>
#include <sys/pathname.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/statvfs.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <sys/kabi.h>
#include <sys/acl.h>
#include <sys/mac_label.h>
#include <sys/stat.h>
#include "hwgnode.h"


static int cap_hwg_get(vertex_hdl_t, char *, char *, int *, int);
static int cap_hwg_match(const char *, int);

/*
** hardware graph pseudo-filesystem vnode operations.
*/



/*
 * Internal access checker.
 */
/* ARGSUSED */
static int
hwg_iaccess(	vertex_hdl_t vertex,
		enum vtype type,
		mode_t desired_mode,
		struct cred *cr)
{
	mode_t mode, orgmode = desired_mode;
	uid_t uid;
	gid_t gid;
	int error;

	/*
	 * Verify that MAC policy allows the requested access.
	 */
	if (error = _MAC_HWG_IACCESS(vertex, orgmode, cr))
		return error;

	/* CAP_DEVICE_MGT permits callers to ignore DAC */
	if (cap_able_cred(cr, CAP_DEVICE_MGT))
		return 0;

	/* Get current permissions and check them against credentials. */
	hwgfs_get_permissions(vertex, type, &mode, &uid, &gid);

	/*
	 * If there's an Access Control List, use it instead of mode bits
	 */
	if ((error = _ACL_HWG_IACCESS(vertex, uid, gid, orgmode, mode, cr)) != -1)
		return error;

	/*
	 * check mode bits
	 */
	if (cr->cr_uid != uid) {
		desired_mode >>= 3;
		if (!groupmember(gid, cr))
			desired_mode >>= 3;
	}

	/*
	 * give permission if the mode bits exactly match the desired access
	 */
	if ((mode & desired_mode) == desired_mode)
		return 0;

	/* check for appropriate privilege */
	if (((orgmode & VWRITE) && !cap_able_cred(cr, CAP_DAC_WRITE)) ||
	    ((orgmode & VREAD) && !cap_able_cred(cr, CAP_DAC_READ_SEARCH)) ||
	    ((orgmode & VEXEC) && !cap_able_cred(cr, CAP_DAC_EXECUTE))) {
		return EACCES;
	}

	/* the caller has appropriate privilege */
	return 0;
}



/* ARGSUSED */
static int
hwgaccess(	bhv_desc_t *bdp,
		int mode,
		struct cred *cr)
{
	vertex_hdl_t vertex = VNODE_TO_HWG(bdp)->hwg_vertex;
	vnode_t *vp = BHV_TO_VNODE(bdp);

	return(hwg_iaccess(vertex, vp->v_type, mode, cr));
}

/*
** A file in this file system represents a hardware graph vertex.  Such a
** vertex can act as a character special file, a block special file, or as
** a directory.
**
** Returns 1 on success; 0 if name is not found in directory.
*/

static int
hwg_dirlookup(vertex_hdl_t dvertex, char *name, vertex_hdl_t *vertexp, vtype_t *type)
{
	graph_error_t rv;
	vtype_t loc_type = VNON;

	ASSERT(dvertex != GRAPH_VERTEX_NONE);
	rv = hwgraph_traverse(dvertex, name, vertexp);
	if (rv == GRAPH_SUCCESS) {
		vertex_hdl_t vertex = *vertexp;
		struct cdevsw *csw = hwgraph_cdevsw_get(vertex);
		struct bdevsw *bsw = hwgraph_bdevsw_get(vertex);

		ASSERT((csw == NULL) || (bsw == NULL));

		if ((bsw == NULL) && (csw == NULL))
			loc_type = VDIR;
		else if (bsw == NULL)
			loc_type = VCHR;
		else if (csw == NULL)
			loc_type = VBLK;

		/* 
		 * This artificially imposes a tree hierarchy on the hwgraph
		 * so that commands like "find" that recursively descend can
		 * deal with hwgfs.
		 *
		 * hwgfs forces vertexes (represented as directories) to appear
		 * as symbolic links unless we're looking at the first edge
		 * from the vertex' parent to the vertex.
		 *
		 * Note that if the original edge pointing to a dir vertex is
		 * removed, the hwgfs will automatically promote some other
		 * edge to represent a real file, and all "symlinks" are
		 * adjusted (no work involved) accordingly to point to the 
		 * new name.
		 */
		if ((loc_type == VDIR) &&
		    strcmp(name, HWGRAPH_EDGELBL_DOT) &&
		    strcmp(name, HWGRAPH_EDGELBL_DOTDOT)) {

			vertex_hdl_t connectpt = hwgraph_connectpt_get(vertex);
			hwgraph_vertex_unref(connectpt);

			if (connectpt != dvertex)
				loc_type = VLNK;
			else {
				graph_edge_place_t place = EDGE_PLACE_WANT_REAL_EDGES;

				/* 
				 * Walk through parent's edges to see if we're
				 * the first edge pointing to this vertex. We
				 * quit looking at edges when we run out of
				 * outgoing edges or when we find the name
				 * we're looking up, or when we find some
				 * other name pointing to the same vertex.
				 */
				for (;;) {
					vertex_hdl_t foundv;
					char foundname[LABEL_LENGTH_MAX];

					rv = hwgraph_edge_get_next(dvertex, foundname, &foundv, &place);
					if (rv == GRAPH_SUCCESS) {
						hwgraph_vertex_unref(foundv);
					} else {
						loc_type = VLNK; /* sanity */
						break;
					}

					if (!strcmp(foundname, name)) {
						if (foundv != vertex)
							loc_type = VLNK; /* sanity */
						break;
					}

					if (foundv == vertex) {
						/* We're *not* the first edge pointing to the vertex */
						loc_type = VLNK;
						break;
					}
				}
			}
		}

		*type = loc_type;
		return(1);
	} else
		return(0);
}


/* ARGSUSED */
static int
hwglookup(	bhv_desc_t *bdp,
		char *component,
		struct vnode **vpp,
		struct pathname *pnp,
		int flags,
		struct vnode *rdir,
		struct cred *cr)
{
	vnode_t *dvp = BHV_TO_VNODE(bdp);
	struct vnode *newvp;
	hwgnode_t *hwgp = VNODE_TO_HWG(bdp);
	vertex_hdl_t vertex;
	vtype_t type;
	int error;

	if (error = hwg_iaccess(hwgp->hwg_vertex, dvp->v_type, VEXEC, cr))
		return(error);

	/* Short-cut for handling "."  -- current directory */
	if (!strcmp(component, ".")) {
			VN_HOLD(dvp);
			*vpp = dvp;
			return(0);
	} 

	if (!hwg_dirlookup(hwgp->hwg_vertex, component, &vertex, &type))
		return(ENOENT);

	ASSERT((type == VDIR) || (type == VCHR)
		|| (type == VBLK) || (type == VLNK));

	newvp = hwgfs_getnode(vertex, type, hwgp->hwg_mount, cr);
	ASSERT(newvp != NULL);

	(void)hwgraph_vertex_unref(vertex);

	*vpp = newvp;

	return 0;
}


/* ARGSUSED */
static int
hwgopen(	bhv_desc_t *bdp,
		struct vnode **vpp,
		mode_t flag,
		struct cred *cr)
{
	ASSERT(VNODE_TO_HWG(bdp)->hwg_valid == 1);
	return(0);
}

/* ARGSUSED */
static int
hwginactive(bhv_desc_t *bdp, struct cred *cr)
{
	return VN_INACTIVE_CACHE;
}

/* ARGSUSED */
static int
hwgreclaim(bhv_desc_t *bdp, int flag)
{
	hwgfs_freenode(bdp);
	return 0;
}

/* ARGSUSED */
static int
hwgcreate(	bhv_desc_t *bdp,
		char *name,
		struct vattr *vap,
		int flags,
		int mode,
		struct vnode **vpp,
		struct cred *cr)
{
	vnode_t *dvp = BHV_TO_VNODE(bdp);
	hwgnode_t *hwgp = VNODE_TO_HWG(bdp);
	vertex_hdl_t dvertex = hwgp->hwg_vertex;
	vertex_hdl_t vertex;
	struct vnode *newvp;
	vtype_t type;
	int error;

	if (*vpp) {
		VN_RELE(*vpp);
		*vpp = NULL;
	}

	if (dvp->v_type != VDIR)
		return(ENOTDIR);

	if (error = hwg_iaccess(dvertex, dvp->v_type, VEXEC, cr))
		return(error);

	if (!hwg_dirlookup(dvertex, name, &vertex, &type))
		return(ENODEV);

	if (flags & VEXCL) {
		(void)hwgraph_vertex_unref(vertex);
		return(EEXIST);
	} 

	if (((type == VDIR) || type == VLNK) && (mode & VWRITE)) {
		(void)hwgraph_vertex_unref(vertex);
		return(EISDIR);
	} 

	if (mode) {
		int error = hwg_iaccess(vertex, type, mode, cr);
		if (error) {
			(void)hwgraph_vertex_unref(vertex);
			return(error);
		}
	}

	if ((type == VDIR) || (type == VLNK)) {
		(void)hwgraph_vertex_unref(vertex);
		return(ENOSYS);
	}

	ASSERT((type == VCHR) || (type == VBLK));

	newvp = hwgfs_getnode(vertex, type, hwgp->hwg_mount, cr);
	(void)hwgraph_vertex_unref(vertex);

	*vpp = newvp;

	return(0);
}


/* ARGSUSED */
static int
hwggetattr(	bhv_desc_t *bdp,
		struct vattr *vap,
		int flags,
		struct cred *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	vertex_hdl_t vhdl = VNODE_TO_HWG(bdp)->hwg_vertex;
	timespec_t currtime;
	int size;
	mode_t mode; 
	uid_t uid; 
	gid_t gid;

	if (vp->v_type == VLNK) {
		/*
		 * Is there any requirement that a stat on a VLNK file should
		 * return the number of characters in the symbolic link?  If
		 * no one really counts on this behavior, we can skip this
		 * overhead.
		 */
		char devname[MAXDEVNAME];

		vertex_to_name(vhdl, devname, MAXDEVNAME);
		size = strlen(devname)+1;
	} else
		size = 0;

	hwgfs_get_permissions(vhdl, vp->v_type, &mode, &uid, &gid);

	vap->va_type = vp->v_type;
	vap->va_mode = mode;
	vap->va_uid = uid;
	vap->va_gid = gid;
	vap->va_fsid = HWGRAPH_STRING_DEV;
	vap->va_nodeid = vhdl;
	vap->va_nlink = (vp->v_type == VDIR) ? 2 : 1;
	vap->va_size = size;
	vap->va_rdev = vhdl_to_dev(vap->va_nodeid);
	vap->va_blksize = MAXBSIZE;
	vap->va_nblocks = 0;

	nanotime_syscall(&currtime);
	vap->va_atime = currtime;
	vap->va_mtime = currtime;
	vap->va_ctime = currtime;
	vap->va_vcode = 0;
	vap->va_xflags = 0;
	vap->va_extsize = 0;
	vap->va_nextents = 0;
	vap->va_anextents = 0;
	vap->va_projid = 0;
	vap->va_gencount = 0;

	return(0);
}


/*
 * Bit mask of which file attributes are settable on a hwgfs file.
 * The "time" fields aren't really settable, but we permit attempts
 * to set these fields.  The time returned from a stat call is 
 * always the current time.  Since specfs tracks the times accurately, 
 * times appear to work properly at user-level for open special files.
 */
#define HWGFS_SETTABLE (AT_MODE|AT_UID|AT_GID|AT_ATIME|AT_MTIME|AT_CTIME)

static int
hwgsetattr(	bhv_desc_t *bdp,
		struct vattr *vap,
		int flags,
		struct cred *cr)
{
	vnode_t *vp = BHV_TO_VNODE(bdp);
	vertex_hdl_t vhdl = VNODE_TO_HWG(bdp)->hwg_vertex;
	mode_t mode;
	uid_t old_uid, new_uid;
	gid_t old_gid, new_gid;
	int mask, file_owner, error;

	/* Can only set a few attributes.  */
	mask = vap->va_mask;
	if (mask & ~HWGFS_SETTABLE)
		return EINVAL;

	/*
	 * Verify that MAC policy allows the requested access.
	 */
	if (error = _MAC_HWG_IACCESS(vhdl, VWRITE, cr))
		return error;

	/* Get old permissions */
	hwgfs_get_permissions(vhdl, vp->v_type, &mode, &old_uid, &old_gid);

	file_owner = (cr->cr_uid == old_uid);
	new_uid = (mask & AT_UID) ? vap->va_uid : old_uid;
	new_gid = (mask & AT_GID) ? vap->va_gid : old_gid;
	
	if (mask & (AT_MODE | AT_UID | AT_GID)) {
		/*
		 * CAP_FOWNER overrides the following restrictions:
		 *
		 * The user ID of the calling process must be equal
		 * to the file owner ID, except in cases where the
		 * CAP_FSETID capability is applicable.
		 */
		if (!file_owner && !cap_able_cred(cr, CAP_FOWNER))
			return(EPERM);

		/* setuid, setgid, and sticky bits are never valid in hwgfs */
		if (mask & AT_MODE)
			mode = vap->va_mode & MODEMASK &
				~(S_ISUID|S_ISGID|S_ISVTX);

		/*
		 * Change file ownership.  Must be the owner or privileged.
		 * If the system was configured with the "restricted_chown"
		 * option, the owner is not permitted to give away the file,
		 * and can change the group id only to a group of which he
		 * or she is a member.
		 */
		if (mask & (AT_UID|AT_GID)) {
			/*
			 * CAP_CHOWN overrides the following restrictions:
			 *
			 * If _POSIX_CHOWN_RESTRICTED is defined, this
			 * capability shall override the restriction that
			 * a process cannot change the user ID of a file
			 * it owns and the restriction that the group ID
			 * supplied to the chown() function shall be equal
			 * to either the group ID or one of the
			 * supplementary group IDs of the calling process.
			 */
			if (restricted_chown && (old_uid != new_uid ||
			     (old_gid != new_gid &&
			      !groupmember(new_gid, cr))) &&
			    !cap_able_cred(cr, CAP_CHOWN))
				return (EPERM);
		}
	}


	/*
	 * Change file times
	 */
	if (mask & (AT_ATIME | AT_MTIME)) {
		if (!file_owner) {
			if ((flags & ATTR_UTIME) &&
			    !cap_able_cred(cr, CAP_FOWNER)) {
				return EPERM;
			}
			if ((error = hwg_iaccess(vhdl, vp->v_type, VWRITE, cr)) && !cap_able_cred(cr, CAP_FOWNER)) {
				return error;
			}
		}
	}

	/* Set new permissions */
	hwgfs_set_permissions(vhdl, vp->v_type, mode, new_uid, new_gid);

	return(0);
}



#define	round(r)	(((r)+sizeof(off_t)-1)&(~(sizeof(off_t)-1)))
#define	irix5_round(r)	(((r)+sizeof(irix5_off_t)-1)&(~(sizeof(irix5_off_t)-1)))

static int
hwgfs_fmtdirent(	void *bp, 
			int maxlen, 
			ino_t d_ino, 
			off_t offset, 
			char *d_name)
{
	register int i;
	register struct dirent *dirent = (struct dirent *)bp;
	register int reclen, rreclen;
	int dsize;

	dsize = (char *)dirent->d_name - (char *)dirent;
	reclen = dsize + strlen(d_name) + 1;
	rreclen = round(reclen);

	/* If doesn't fit, return 0 without doing anything. */
	if (rreclen > maxlen)
		return(0);

	dirent->d_off = offset;
	dirent->d_ino = d_ino;
	dirent->d_reclen = rreclen;
	strcpy(dirent->d_name, d_name);

	/*
	 * Pad to nearest word boundary (if necessary).
	 */
	for (i = reclen; i < rreclen; i++)
		dirent->d_name[i-dsize] = '\0';
	return rreclen;
}


static int
irix5_hwgfs_fmtdirent(	void *bp, 
			int maxlen, 
			ino_t d_ino, 
			off_t offset, 
			char *d_name)
{
	register int i;
	register struct irix5_dirent *dirent = (struct irix5_dirent *)bp;
	register int reclen, rreclen;
	int dsize;

	dsize = (char *)dirent->d_name - (char *)dirent;
	reclen = dsize + strlen(d_name) + 1;
	rreclen = irix5_round(reclen);

	/* If doesn't fit, return 0 without doing anything. */
	if (rreclen > maxlen)
		return(0);

	dirent->d_off = offset;
	dirent->d_ino = d_ino;
	dirent->d_reclen = rreclen;
	strcpy(dirent->d_name, d_name);

	/*
	 * Pad to nearest word boundary (if necessary).
	 */
	for (i = reclen; i < rreclen; i++)
		dirent->d_name[i-dsize] = '\0';
	return rreclen;
}

/* ARGSUSED */
static int
hwgreaddir(	bhv_desc_t *bdp,
		register struct uio *uiop,
		struct cred *fcr,
		int *eofp)
{
	int reclen;
	int error = 0;

	int bufsize;			/* size of user's buffer */
	void *bep;			/* ptr to dirent in output buffer */
	int bufleft;			/* number bytes remaining in buffer */
	caddr_t bufbase;		/* buffer for translation */
	vertex_hdl_t d_ino;
	char d_name[LABEL_LENGTH_MAX+1];
	int (*dirent_func)(void *, int, ino_t, off_t, char *);
	graph_error_t rv;
	graph_edge_place_t place, oplace;

	vertex_hdl_t vertex = VNODE_TO_HWG(bdp)->hwg_vertex;

	dirent_func = ABI_IS_IRIX5(GETDENTS_ABI(get_current_abi(), uiop)) ?
				irix5_hwgfs_fmtdirent : hwgfs_fmtdirent;

	if (uiop->uio_resid <= 0)
		return ENOENT;

	/*
	 * Get some memory to fill with fs-independent versions of the
	 * requested entries.
	 */
	bufsize = uiop->uio_resid;
	if (uiop->uio_segflg == UIO_SYSSPACE) {
		ASSERT(uiop->uio_iovcnt == 1);
		bufbase = NULL;
		bep = uiop->uio_iov->iov_base;
		ASSERT(((__psint_t)bep & (NBPW-1)) == 0);
	} else {
		/*
		 * Don't try to allocate some arbitrarily large bucket.
		 */
		bufsize = MIN(bufsize, 0x4000);

		bufbase = kmem_alloc(bufsize, KM_SLEEP);
		bep = bufbase;
		ASSERT(((__psint_t)bep & (NBPW-1)) == 0);
	}
	bufleft = bufsize;

	place = (graph_edge_place_t)uiop->uio_offset;

	/*
	 * Loop until user's request is satisfied or until all edges
	 * have been examined.
	 */
	while (1) {
		oplace = place;
		rv = hwgraph_edge_get_next(vertex, d_name, &d_ino, &place);

		if (rv != GRAPH_SUCCESS)
			break;

		(void)hwgraph_vertex_unref(d_ino);

		reclen = (*dirent_func)(bep, bufleft, (ino_t)d_ino,
						      (off_t)place, d_name);

		if (reclen > 0) {
			/*
			 * Update buffer state variables.
			 */
			bep = (void *)((char *)bep + reclen);
			bufleft -= reclen;
		} else {
			place = oplace; /* We'll need to re-read */
			break;
		}

	}

	/* Error if no entries have been returned yet. */
	if (bufleft == bufsize) {
		error = EINVAL;
	} else {
		if (uiop->uio_segflg == UIO_USERSPACE) {
			if (uiomove((caddr_t) bufbase, bufsize-bufleft, UIO_READ, uiop))
				error=EFAULT;
		} else
			uiop->uio_resid = bufleft;
	}

	if (eofp)
		*eofp = (rv == GRAPH_HIT_LIMIT);

	uiop->uio_offset = (off_t)place;

	if (bufbase != NULL)
		kmem_free(bufbase, bufsize);

	return(error);
}


/*
 * We permit readlink to work on any file in hwgfs, and we return 
 * the file's canonical hwgraph name.  The vnode layer happens to
 * enforce the policy that readlinks can be done only on files of
 * type VLNK.
 */
/* ARGSUSED */
static int
hwgreadlink(
	bhv_desc_t *bdp,
	struct uio *uiop,
	struct cred *cr)
{
	hwgnode_t *hwgp = VNODE_TO_HWG(bdp);
	vertex_hdl_t vhdl = hwgp->hwg_vertex;
	char devname[MAXDEVNAME];
	int error;

	vertex_to_name(vhdl, devname, MAXDEVNAME);
	error = uiomove(devname, strlen(devname), UIO_READ, uiop);

	return(error);
}


/*
 * Some labelled information is exported to user-level via attr_get.
 *
 * After a kernel module adds labelled information to a vertex, it
 * may choose to export that label via hwgraph_info_export_LBL, or
 * it may choose to leave the information invisible to attr_get.
 */
static int
hwgattr_get(bhv_desc_t *bdp, char *name, char *value, int *valuelenp, 
	    int flags, struct cred *cred)
{
	hwgnode_t *hwgp;
	vertex_hdl_t vhdl;
	char buff[MAXDEVNAME];
	arbitrary_info_t info;
	char *infoptr;
	int error;
	int length = 0;
	struct uio uio;
	struct iovec iov;
	graph_error_t rc;
	int exportinfo;

	hwgp = VNODE_TO_HWG(bdp);
	vhdl = hwgp->hwg_vertex;

	/*
	 * Verify that MAC policy allows the requested access.
	 */
	if (error = _MAC_HWG_IACCESS(vhdl, VREAD, cred))
		return error;

	/* A few attributes are implicitly set for hwgraph files */
	if (INFO_LBL_RESERVED(name)) {
		if (!strcmp(name, _DEVNAME_ATTR)) {
			buff[0] = 0;
			vertex_to_name(vhdl, buff, MAXDEVNAME);
			length = strlen(buff);
			infoptr = buff;
			goto found;
		} else if (!strcmp(name, _DRIVERNAME_ATTR)) {
			device_driver_t driver;
	
			buff[0] = 0;
			driver = device_driver_getbydev(vhdl);
			device_driver_name_get(driver, buff, MAXDEVNAME);
			length = strlen(buff);
			infoptr = buff;
			goto found;
		} else if (!strcmp(name, _MASTERNODE_ATTR)) {
			cnodeid_t cnodeid = master_node_get(vhdl);
	
			if (cnodeid == CNODEID_NONE)
				return(ENOATTR);
	
			vhdl = cnodeid_to_vertex(cnodeid);
			ASSERT(vhdl != GRAPH_VERTEX_NONE);
	
			vertex_to_name(vhdl, buff, MAXDEVNAME);
			infoptr = buff;
			length = strlen(buff);
			goto found;
		} else if (!strcmp(name, _INVENT_ATTR)) {
			inventory_t *pinv;
			invplace_t invplace = INVPLACE_NONE;

			/* 
			 * Jam a bunch of inventory structures associated with
			 * the specified device into a user buffer.  Stop when
			 * we run out of inventory structures, or when we run
			 * out of room in the user buffer.
			 */
			uio.uio_iov = &iov;
			uio.uio_iovcnt = 1;
			uio.uio_offset = 0;
			uio.uio_segflg = UIO_SYSSPACE;
			uio.uio_resid = iov.iov_len = *valuelenp;
			uio.uio_pmp = NULL;
			uio.uio_pio = 0;
			uio.uio_readiolog = 0;
			uio.uio_writeiolog = 0;
			uio.uio_pbuf = 0;
			iov.iov_base = value;

			while ((pinv = device_inventory_get_next(vhdl, &invplace)) != NULL) {
				
#if _MIPS_SIM == _ABI64
				/* The first argument in an invent is a
				 * pointer. As the user really does not
				 * need it we just cut it off
				 */
				if (!ABI_IS_64BIT(get_current_abi())) 
					error = uiomove((char *)pinv + 4,
							sizeof(*pinv) - 8,
							UIO_READ, &uio);
				else 
#endif
					error = uiomove(pinv, sizeof(*pinv),
							UIO_READ, &uio);
				if (error)
					return(error);
			}
			*valuelenp = uio.uio_offset; /* Total number of bytes copied */
			return(0);
		}
	}

	error = _MAC_HWG_GET(vhdl, name, value, valuelenp, flags);
	if (error != -1)
		return(error);
	error = _ACL_HWG_GET(vhdl, name, value, valuelenp, flags);
	if (error != -1)
		return(error);
	error = cap_hwg_get(vhdl, name, value, valuelenp, flags);
	if (error != -1)
		return(error);

	rc = hwgraph_info_get_exported_LBL(vhdl, name, &exportinfo, &info);

	if (rc != GRAPH_SUCCESS)
		return(ENOATTR);

	if (exportinfo == INFO_DESC_PRIVATE)
		return(ENOATTR);

	if (exportinfo == INFO_DESC_EXPORT) {
		/* Export the info field itself */
		infoptr = (char *)&info;
		length = sizeof(arbitrary_info_t);
	} else {
		/* Info field is a pointer to exportinfo bytes */
		ASSERT(exportinfo > 0);
		infoptr = (char *)info;
		length = exportinfo;
	}

found:
	/*
	 * If the user's buffer is too small tell them how big the
	 * attribute really is but don't copy any data
	 */
	if (*valuelenp < length) {
		*valuelenp = length;
		return(E2BIG);
	}

	/* Invariant: infoptr points to attribute value, length is #bytes */

	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_resid = iov.iov_len = *valuelenp;
	uio.uio_pmp = NULL;
	uio.uio_pio = 0;
	uio.uio_readiolog = 0;
	uio.uio_writeiolog = 0;
	uio.uio_pbuf = 0;
	iov.iov_base = value;

	error = uiomove(infoptr, length, UIO_READ, &uio);
	
	/*
	 * Set this to the number of bytes we actually copied out.
	 * uio_offset works since we initialize it to 0 above.
	 */
	*valuelenp = uio.uio_offset;

	return(error);
}

static int
hwgattr_replace(vertex_hdl_t vhdl, char *name, int len, char *info)
{
	arbitrary_info_t oinfo;
	graph_error_t rv;

	rv = hwgraph_info_replace_LBL(vhdl, name, (arbitrary_info_t) info,
				      &oinfo);
	if (rv == GRAPH_SUCCESS) {
		kern_free((void *) oinfo);
		hwgraph_info_export_LBL(vhdl, name, len);
		return(0);
	} else {
		return(ENOATTR);
	}
}

static int
hwgattr_create(vertex_hdl_t vhdl, char *name, int len, char *info)
{
	graph_error_t rv;

	rv = hwgraph_info_add_LBL(vhdl, name, (arbitrary_info_t) info);
	if (rv == GRAPH_SUCCESS) {
		hwgraph_info_export_LBL(vhdl, name, len);
		return(0);
	} else {
		return(EEXIST); /* (Probably a DUP) */
	}
}

static int
hwgattr_set(bhv_desc_t *bdp, char *name, char *value, int valuelen, int flags, 
		struct cred *cred)
{
	hwgnode_t *hwgp;
	vertex_hdl_t vhdl;
	char *info;
	int error;

	hwgp = VNODE_TO_HWG(bdp);
	vhdl = hwgp->hwg_vertex;

	/*
	 * Verify that MAC policy allows the requested access.
	 */
	if (error = _MAC_HWG_IACCESS(vhdl, VWRITE, cred))
		return error;

	/* 
	 * Make sure we've got permission to make make special files,
	 * since by changing this attribute we're essentially creating
	 * a new special file.
	 */
	if (!cap_able_cred(cred, CAP_MKNOD))
		return(EPERM);

	if (!strcmp(name, INFO_LBL_CONTROLLER_NAME)) {
		if (valuelen < sizeof(int))
			return(EINVAL);
		device_controller_num_set(vhdl, *(int *)value);	
		return(0);
	}

	/* Don't allow anyone to change pre-defined hwgfs attributes. */
	if (INFO_LBL_RESERVED(name))
		return(EEXIST);

	if (error = _MAC_HWG_SET(name, flags))
		return(error);
	if (error = _ACL_HWG_SET(name, flags))
		return(error);
	if (error = cap_hwg_match(name, flags))
		return(error);

	info = (char *) kern_malloc(valuelen);
	bcopy(value, info, valuelen);

	if (flags & ATTR_REPLACE) {
		error = hwgattr_replace(vhdl, name, valuelen, info);
		if (error)
			kern_free(info);
	} else if (flags & ATTR_CREATE) {
		error = hwgattr_create(vhdl, name, valuelen, info);
		if (error)
			kern_free(info);
	} else {
		error = hwgattr_create(vhdl, name, valuelen, info);
		if (error)
			error = hwgattr_replace(vhdl, name, valuelen, info);
		if (error)
			kern_free(info);
	}

	return(error);
}

/*
 * Remove a hwgfs file attribute that was set by an earlier hwgattr_set call.
 * Only special users (i.e. root) can add/remove attributes.
 */
static int
hwgattr_remove(bhv_desc_t *bdp, char *name, int flags, struct cred *cred)
{
	hwgnode_t *hwgp;
	vertex_hdl_t vhdl;
	graph_error_t rv;
	char *oldvalue;
	int error;

	hwgp = VNODE_TO_HWG(bdp);
	vhdl = hwgp->hwg_vertex;

	/*
	 * Verify that MAC policy allows the requested access.
	 */
	if (error = _MAC_HWG_IACCESS(vhdl, VWRITE, cred))
		return error;

	if (!cap_able_cred(cred, CAP_MKNOD))
		return(EPERM);

	/* Don't allow anyone to change pre-defined hwgfs attributes. */
	if (INFO_LBL_RESERVED(name))
		return(EEXIST);

	if (error = _MAC_HWG_REMOVE(name, flags))
		return(error);
	if (error = _ACL_HWG_REMOVE(name, flags))
		return(error);
	if (error = cap_hwg_match(name, flags))
		return(error);

	rv = hwgraph_info_remove_LBL(vhdl, name, (arbitrary_info_t *)&oldvalue);

	if (rv == GRAPH_SUCCESS) {
		kern_free(oldvalue);
		return(0);
	} else
		return(ENOATTR);
}

__inline static int
cap_match_name(const char *name)
{
	size_t len = strlen(name) + 1;

	return (len == sizeof(SGI_CAP_FILE) && !bcmp(name, SGI_CAP_FILE, len));
}

/*ARGSUSED*/
static int
cap_hwg_get(vertex_hdl_t vhdl, char *name, char *value, int *valuelenp,
	    int flags)
{
	if (cap_match_name(name)) {
		if ((flags & ATTR_ROOT) == 0)
			return(ENOATTR);

		if (*valuelenp < sizeof(struct cap_set)) {
			*valuelenp = sizeof(struct cap_set);
			return(E2BIG);
		}
	}
	return(-1);
}

static int
cap_hwg_match(const char *name, int flags)
{
	if (cap_match_name(name)) {
		if ((flags & ATTR_ROOT) == 0)
			return(ENOATTR);
	}
	return(0);
}

vnodeops_t hwgvnodeops = {
	BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
	hwgopen,	/* open */
	(vop_close_t)fs_noerr,
	(vop_read_t)fs_nosys,
	(vop_write_t)fs_nosys,
	(vop_ioctl_t)fs_nosys,
	fs_setfl,	/* setfl */
	hwggetattr,
	hwgsetattr,	/* setattr */
	hwgaccess,
	hwglookup,
	hwgcreate,
	(vop_remove_t)fs_nosys,
	(vop_link_t)fs_nosys,
	(vop_rename_t)fs_nosys,
	(vop_mkdir_t)fs_nosys,
	(vop_rmdir_t)fs_nosys,
	hwgreaddir,
	(vop_symlink_t)fs_nosys,
	hwgreadlink,	/* readlink */
	(vop_fsync_t)fs_nosys,
	hwginactive,	/* inactive */
	(vop_fid_t)fs_nosys,
	(vop_fid2_t)fs_nosys,
	fs_rwlock,	/* rwlock */
	fs_rwunlock,	/* rwunlock */
	(vop_seek_t)fs_noerr,
	fs_cmp,		/* cmp */
	(vop_frlock_t)fs_nosys,
	(vop_realvp_t)fs_nosys,
	(vop_bmap_t)fs_nosys,
	(vop_strategy_t)fs_noval,
	(vop_map_t)fs_nodev,
	(vop_addmap_t)fs_nosys,
	(vop_delmap_t)fs_nosys,
	(vop_poll_t)fs_nosys,
	(vop_dump_t)fs_nosys,
	fs_pathconf,
	(vop_allocstore_t)fs_nosys,
	(vop_fcntl_t)fs_nosys,
	hwgreclaim,	/* reclaim */
	hwgattr_get,	/* attr_get */
	hwgattr_set,	/* attr_set */
	hwgattr_remove,	/* attr_remove */
	(vop_attr_list_t)fs_nosys,
	fs_cover,	
	(vop_link_removed_t)fs_noval,
	fs_vnode_change,
	(vop_ptossvp_t)fs_noval,
	(vop_pflushinvalvp_t)fs_noval,
	(vop_pflushvp_t)fs_nosys,
	(vop_pinvalfree_t)fs_noval,
	(vop_sethole_t)fs_noval,
	(vop_commit_t)fs_nosys,
	(vop_readbuf_t)fs_nosys,
	fs_strgetmsg,
	fs_strputmsg,
};
