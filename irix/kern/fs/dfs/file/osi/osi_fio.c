/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: osi_fio.c,v 65.9 1999/08/25 22:13:13 overby Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/* Copyright 1992 Transarc Corporation - All rights reserved */

/*
 * HISTORY
 * $Log: osi_fio.c,v $
 * Revision 65.9  1999/08/25 22:13:13  overby
 * Before calling VOP_GETATTR, set "vattr_t *va_mask" to the fields that
 * need to be returned.
 *
 * Revision 65.8  1998/10/16 19:55:42  lmc
 * Pull the O_RSYNC and O_DSYNC flags from the read/writes of the
 * CacheItems file.  This is to speed up the initial scan of the cache
 * files on startup.  This is the fix for PV 632996.
 *
 * Revision 65.7  1998/04/21  15:56:37  bdr
 * Pull the VOP_RWLOCK and VOP_RWUNLOCK calls from around VOP_READ
 * and VOP_WRITE in osi_RDWR().  This is to keep dfs calls to VOP_READ
 * and VOP_WRITE the same as the rest of the os.  The inode lock was
 * pushed down into the vnode layer in 65.
 *
 * Revision 65.6  1998/04/20  16:00:46  lmc
 * Don't use crhold() to set the reference count when we have just
 * bzero'd the cred structure.  crhold() expects a reference count
 * of at least 1.
 *
 * Revision 65.5  1998/03/23 16:26:27  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.4  1998/03/06 00:09:58  bdr
 * Added added osi_RDWR() code.
 *
 * Revision 65.3  1998/01/07  15:18:16  lmc
 * No longer use KT_ISUPROC because VOP_SETATTR uses the parameter and not
 * the creds from the proc structure.  Added some prototypes and added
 * some explicit casting for "long"s being put into "int"s.
 *
 * Revision 65.2  1997/11/06  19:58:19  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/24  14:29:50  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:39  jdoak
 * Initial revision
 *
 * Revision 64.5  1997/05/22  15:55:03  gw
 * Thi
 *  code adds a panic to osi_Open if the call to VFS_VGET returns
 * a null vnodep.  xfs_vget checks the generation number and if
 * it does not match the handle, it returns a null vnodep but no error.
 * The code was written in response to pv 491062. It appears the customer
 * had deleted the cache was going to reconfigure.
 *
 * Revision 64.4  1997/03/27  21:34:52  lord
 * Fix osi_vptofid so that it does not reference unavailable memory.
 * This is PV 473711.
 *
 * Revision 64.1  1997/02/14  19:46:50  dce
 * *** empty log message ***
 *
 * Revision 1.6  1996/06/15  18:45:22  brat
 * Typo in prev checkin. It should be osi_curproc, not osi_curprocp.
 *
 * Revision 1.5  1996/06/15  02:46:13  brat
 * Removed references to procp via u.u_procp and made them access
 * osi_curprocp. And some things are done only if we have user area.
 *
 * Revision 1.4  1996/05/31  18:18:05  brat
 * Added a call to mapin pages for a buffer in osi_MapStrategy. Unlike other
 * architectures however, we do this for buffers in kernel space. This is required
 * to implement map/unmap functionality.
 *
 * Revision 1.3  1996/05/10  02:06:29  bhim
 * Added #include <dcedfs/osi_user.h> to define osi_getufilelimit.
 *
 * Revision 1.2  1996/04/06  00:17:35  bhim
 * No Message Supplied
 *
 * Revision 1.1  1994/06/09  14:15:06  dcebuild
 * Original File from OSF
 *
 * Revision 1.1.703.2  1994/06/09  14:15:05  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:27:38  annie]
 *
 * Revision 1.1.703.1  1994/02/04  20:23:58  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:15:26  devsrc]
 * 
 * Revision 1.1.701.2  1994/01/20  18:43:28  annie
 * 	added copyright header
 * 	[1994/01/20  18:39:43  annie]
 * 
 * Revision 1.1.701.1  1993/12/07  17:29:30  jaffe
 * 	1.0.3a update from Transarc
 * 
 * 	$TALog: osi_fio.c,v $
 * 	Revision 1.3  1993/10/19  17:27:01  bwl
 * 	Bug fix.
 * 
 * 	In afs_strat_map (called from cm_strategy, calls cm_ustrategy), call
 * 	iodone.  Also, save/restore the b_bcount field, since the kernel might
 * 	want to see it.
 * 	[from r1.2 by delta bwl-o-ot9096-hp-mmap-dfs, r1.1]
 * 
 * Revision 1.2  1993/08/09  14:26:12  berman
 * 	Once more we close to reopen an orthogonal
 * 	delta for export.
 * 
 * 	Casts for HP compiler.
 * 	[from r1.1 by delta bwl-o-db3961-port-103-to-HP, r1.8]
 * 
 * Revision 1.1  1993/07/30  18:17:22  bwl
 * 	Port DFS 1.0.3 to HP/UX, adapting HP's changes (which were made to the
 * 	1.0.2a code base) to our own code base.
 * 
 * 	Moved to HPUX from HP800 subdirectory, while adapting changes from HP.
 * 	[added by delta bwl-o-db3961-port-103-to-HP, r1.1]
 * 
 * Revision 1.6  1993/03/03  23:21:28  blake
 * 	The beginnings of SunOS loadable module support.  I have written
 * 	a new efsconfig.c that contains the initialization code for loading
 * 	Episode.  The big deal here is that all the symbols in the DFS core
 * 	component must be referenced through a jump table.  I have added
 * 	the machinery to do this to the OSI layer (so far), defining big
 * 	hairy macros for each exported kernel symbol.  Still to do: convert
 * 	all the other parts of the DFS core to use this mechanism; write the
 * 	initialization code common to both Episode and DFS that sets up the
 * 	linkage to the core; write the initialization code for DFS; and
 * 	write the initialization code for the core (including afs_syscall).
 * 
 * 	Made osi_fioinit static.
 * 	[from r1.5 by delta blake-db3108-SunOS5.x-syscall-and-modload-code, r1.1]
 * 
 * Revision 1.5  1993/02/02  00:11:46  blake
 * 	Another checkpoint.  Notable differences from the preceding one:
 * 
 * 	1) I did an upgrade to bring my sources up to date; following this,
 * 	   several files had old interfaces that needed updating.
 * 
 * 	2) Another external interface change: a new version of osi_vptofh
 * 	   replaces the old combination of osi_vptofh, VFS_VPTOFH, and #ifdefs.
 * 
 * 	3) I built all the affected code on the RIOS and (sort of) built the
 * 	   osi code on the Sun, thereby uncovering a variety of silly bugs.
 * 
 * 	Fix bug in osi_rdwr usage uncovered by adding prototype.
 * 	[from r1.4 by delta blake-db3108-port-osi-layer-to-SunOS5.x, r1.2]
 * 
 * Revision 1.4  1993/01/28  00:24:56  blake
 * 	This is the first part of the port of the OSI layer to SunOS 5.x,
 * 	consisting of everything that except locking and synchronization
 * 	primitives.  Aside from adding various SunOS-specific definitions,
 * 	I have reorganized the OSI code somewhat.  I have removed many of
 * 	the allegedly (but not really) generic definitions from the top-level
 * 	directory and pruned back the thicket of #ifdefs in the "generic" code.
 * 
 * 	A few external interface changes were necessary.  These result in
 * 	small changes to a large number of files.
 * 
 * 	1) "struct ucred" has been replaced by "osi_cred_t" since the SVR4
 * 	credential structure has a different name.
 * 
 * 	2) To do a setattr in most systems, one fills in as much of the
 * 	vattr structure as appropriate and leaves the remaining members
 * 	set to -1; in SVR4, you indicate which members you want to set
 * 	by means of a bit mask contained in the structure.  "osi_vattr_null(vap)"
 * 	has therefore been replaced by "osi_vattr_init(vap, mask)"; there
 * 	are also osi_vattr_add and osi_vattr_sub functions for manipulating
 * 	the mask.
 * 
 * 	3) The SVR4 timeout/untimeout mechanism is also quite different,
 * 	but I was able to confine the interface changes to osi_net.c.
 * 
 * 	This is just a checkpoint before I begin major surgery on the
 * 	various OSI sleep/wakeup, locking, preemption, and spl routines.
 * 	I have linted the OSI code on a Sun and built it on a RIOS, but
 * 	that is all.  No warranties express or implied.
 * 
 * 	Reorganization of OSI code for SunOS 5.x port.
 * 	[from r1.3 by delta blake-db3108-port-osi-layer-to-SunOS5.x, r1.1]
 * 
 * Revision 1.3  1992/11/25  21:07:38  jaffe
 * 	Fix occasions of afs/ that were not yet at the OSF
 * 	[from revision 1.1 by delta jaffe-sync-with-dcedfs-changes, r1.1]
 * 
 * Revision 1.1  1992/06/06  23:38:44  mason
 * 	Really a bit misnamed - this delta adds code necessary for HP-UX; it isn't
 * 	sufficient for a complete port.
 * 
 * 	HP-UX file I/O routines (probably not finished yet)
 * 	[added by delta mason-add-hp800-osi-routines, revision 1.1]
 * 
 * $EndLog$
 */
#include <dcedfs/param.h>	/* Should be always first */

#include <dcedfs/sysincludes.h>
#include <dcedfs/xvfs_vnode.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_uio.h>
#include <dcedfs/osi_cred.h>
#include <dcedfs/osi_user.h>
#include <dcedfs/lock.h>

#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/kmem.h>

osi_cred_t osi_cred;		/* Credential struct used when calling UFS layer */
struct vattr tvattr;		/* Holds file's attributes */
struct lock_data osi_xosi;	/* lock is for tvattr */
static long osi_fioinit = 0;

#ifndef AFS_VFSCACHE
/* 
 * Get an existing inode.  Common code for iopen, iread/write, iinc/dec. 
 *
 * Passed a device, inode #.
 * Returns an inode pointer and a vnode pointer.
 */
struct inode *
igetinode(dev, inode, vpp)
	struct vnode **vpp; 
	dev_t dev;
	ino_t inode;
{
        struct inode *ip;
	struct mount *mp;
	DEFINE_OSI_UERROR;

	/* Isn't it fun calling into UFS like this? */
	/* This call appears to not be MP safe at all .... */
	if ((mp = getmp(dev)) == (struct mount *)NULL) {
	    osi_setuerror(ENOENT);
	    return NULL;
	}
        ip = (struct inode *)iget(dev, mp, inode);
	if (ip == NULL) {
	    osi_setuerror(ENOENT);	
	    return ip;
	}

	if (ip->i_nlink == 0) {
	    iput(ip);
	    osi_setuerror(ENOENT);
	    return NULL;
	}

	/* the inode must be returned unlocked */
	iunlock(ip);
	*vpp = ITOV(ip);
	return ip;
}

/* 
 * Generic local file system Open interface 
 *
 * N.B. This osi_Open signature goes *exclusively* with the !AFS_VFSCACHE
 *      configuration.
 */
struct osi_file *osi_Open(adev, ainode)
    register struct osi_dev *adev;
    long ainode; 
{
    register struct inode *ip;
    register struct osi_file *afile;
    struct vnode *vp = (struct vnode *)0;

    if (!osi_fioinit) {
	bzero((char *)&osi_cred, sizeof(osi_cred_t));
	lock_Init(&osi_xosi);
        osi_cred.cr_ref=1;      /* Set the reference without
                                        using crhold() because
                                        it doesn't like 0 as ref */
	osi_fioinit = 1;
    }
    afile = (struct osi_file *) osi_Alloc(sizeof(struct osi_file));
    osi_setuerror(0);	
    ip = (struct inode *) igetinode((dev_t) adev->dev, (ino_t)ainode, &vp);
    if (!ip) {
	     return (struct osi_file *) 0;
    }
    /* 
     * Save the vnode pointer for the inode ip;
     * also ip is already prele'd in igetinode 
     */
    afile->vnode = vp;
    afile->offset = 0;
    afile->proc = (int (*)()) 0;
    return afile;
}
#endif	/* AFS_VFSCACHE */


/* 
 * Generic local file system Stat interface 
 */
#ifdef SGIMIPS
int
osi_Stat(
    register struct osi_file *afile,
    register struct osi_stat *astat)
#else
int
osi_Stat(afile, astat)
    register struct osi_file *afile;
    register struct osi_stat *astat; 
#endif
{
#ifdef SGIMIPS
    register int code;
#else
    register long code;
#endif
    struct vattr tvattr;

#ifdef SGIMIPS
    tvattr.va_mask = AT_SIZE|AT_BLKSIZE|AT_UPDATIME|AT_UPDMTIME;
#endif
    VOP_GETATTR(afile->vnode, &tvattr, ATTR_LAZY, &osi_cred, code);
    if (code == 0) {
	astat->size = tvattr.va_size;
	astat->blocksize = tvattr.va_blksize;
	astat->mtime = tvattr.va_mtime.tv_sec;
	astat->atime = tvattr.va_atime.tv_sec;
    }
    return code;
}


/* 
 * Generic local file system Close interface 
 */
#ifdef SGIMIPS
int
osi_Close( register struct osi_file *afile) 
#else
int
osi_Close(afile)
    register struct osi_file *afile; 
#endif
{
    VN_RELE(afile->vnode);
    osi_Free((char *)afile, sizeof(struct osi_file));
    return 0;
}


/* 
 * Generic local file system truncate interface 
 */
#ifdef SGIMIPS
int
osi_Truncate(
    register struct osi_file *afile,
    long asize) 
#else
int
osi_Truncate(afile, asize)
    register struct osi_file *afile;
    long asize; 
#endif
{
    osi_cred_t *oldCred;
#ifdef SGIMIPS
    register int code;
#else
    register long code;
#endif
    int null_time = 0;
    struct vattr tvattr;

    osi_vattr_null(&tvattr);
#ifdef SGIMIPS
    osi_set_size_flag(&tvattr);
#endif /* SGIMIPS */
    tvattr.va_size = asize;
    VOP_SETATTR(afile->vnode, &tvattr, null_time ,&osi_cred, code);
    return code;
}

/* 
 * Generic local file system read interface 
 */
#ifdef SGIMIPS
int
osi_Read(
    register struct osi_file *afile,
    char *aptr,
    long asize) 
#else
int
osi_Read(afile, aptr, asize)
    register struct osi_file *afile;
    char *aptr;
    long asize; 
#endif
{
#ifdef SGIMIPS
    ssize_t resid;
    register int code;
#else
    int resid;
    register long code;
#endif
    DEFINE_OSI_UERROR;

    code = osi_rdwr(UIO_READ, afile->vnode, (caddr_t)aptr, asize,
		    afile->offset, OSI_UIOSYS, 0, &resid);
    if (code == 0) {
	code = (int)asize - (int)resid;
	afile->offset += code;
    } else {
	 osi_setuerror (code);
	 code = -1;
    }
    return code;
}


/* 
 * Generic local file system write interface 
 */
#ifdef SGIMIPS
int
osi_Write(
    register struct osi_file *afile,
    char *aptr,
    long asize)
#else
int
osi_Write(afile, aptr, asize)
    register struct osi_file *afile;
    char *aptr;
    long asize; 
#endif
{
#ifdef SGIMIPS
    ssize_t resid;
    register int code;
#else
    int resid;
    register long code;
#endif
    DEFINE_OSI_UERROR;

    code = osi_rdwr(UIO_WRITE, afile->vnode, (caddr_t)aptr, asize,
		    afile->offset, OSI_UIOSYS, 0, &resid);
    if (code == 0) {
	code = (int)asize - (int)resid;
	afile->offset += code;
    } else {
	 osi_setuerror (code);
	 code = -1;
    }
    if (afile->proc) {
	(*afile->proc)(afile, code);
    }
    return code;
}

/* 
 * This routine written from the RT NFS port strategy routine.
 * It has been generalized a bit, but should still be pretty clear. 
 */
#ifdef SGIMIPS
long osi_MapStrategy(
	int (*aproc)(struct buf *),
	register struct buf *bp)
#else
long osi_MapStrategy(aproc, bp)
	int (*aproc)(struct buf *);
	register struct buf *bp;
#endif
{
	long code;

	if (!(bp->b_flags & B_MAPPED)) {
		bp_mapin (bp);
	}
	code = (*aproc) (bp);
	return code;
}

int
osi_vptofid(struct vnode *vp, osi_fid_t *fidp)
{
    osi_fid_t *tfidp;
    int code;

    VOP_FID(vp, &tfidp, code);
    if (code != 0)
	return code;

    fidp->fid_len = tfidp->fid_len;
    bcopy(tfidp->fid_data, fidp->fid_data, fidp->fid_len);
    freefid(tfidp);

    return 0;
}

/* Adding osi_Open for the VFSCACHE case */
#ifdef AFS_VFSCACHE
#ifdef SGIMIPS
struct osi_file *
osi_Open (
    struct osi_vfs *vfsp,
    osi_fid_t *fhandle)
#else
struct osi_file *
osi_Open (vfsp, fhandle)
    struct osi_vfs *vfsp;
    osi_fid_t *fhandle;
#endif
{
    register struct osi_file *afile;
    struct vnode *vp, *vnodep;
    int code;

    if (!osi_fioinit) {
	bzero((char *)&osi_cred, sizeof(osi_cred_t));
	lock_Init(&osi_xosi);
	osi_cred.cr_ref=1;	/* Set the reference without
					using crhold() because
					it doesn't like 0 as ref */
	osi_fioinit = 1;
    }

    afile = (struct osi_file *) osi_Alloc(sizeof(struct osi_file));

    VFS_VGET(vfsp, &vnodep, fhandle, code); 

    /*
     * This panic was added to detect if vnodep was null upon return 
     * from the call to VFS_VGET which indicates that the generation
     * number for the file handle does not match the inode, or
     * in our case, some one deleted the dcache files. 
     */

    if (vnodep == NULL) {
	panic("vnode returned from VFS_VGET is null, were dcache files deleted?");
    }
    if (!code) {
	vp = vnodep;
	VOP_OPEN(vp, &vnodep, FREAD|FWRITE, &osi_cred, code);
	if(code)
	   panic("failed to open vnode");
    }
    else 
        panic("failed to vget vnode from vfs");

    /* Assign the return value */
    afile->vnode = vnodep;
    afile->offset = 0;
    afile->proc = (int (*)()) 0;

    return afile;
}
#endif

#ifdef SGIMIPS
int
osi_QuickTrunc(vfsp, fhandle, asize)
    struct osi_vfs *vfsp;
    osi_fid_t *fhandle;
    long asize;
{
    int	code;
    struct vattr tvattr;
    struct vnode *vnodep;
    int null_time = 0;
    osi_cred_t *oldCred;

    VFS_VGET(vfsp, &vnodep, fhandle, code);
    if (code) {
	panic("failed to vget vnode from vfs");
    }
    osi_vattr_null(&tvattr);
    osi_set_size_flag(&tvattr);
    tvattr.va_size = asize;
    VOP_SETATTR(vnodep, &tvattr, null_time, &osi_cred, code);
    VN_RELE(vnodep);
    return(code);
}

#endif /* SGIMIPS */

/*
 * Generic local file system RDWR interface
 */
osi_RDWR(afile, arw, auio)
    struct osi_file *afile;
    enum uio_rw arw;
    struct uio *auio;
{
    int code;

    osi_RestorePreemption(0);
    if (arw== UIO_WRITE) {
	VOP_WRITE(((struct osi_file *)afile)->vnode, auio, 0, osi_credp, 
			NULL, code);
    }
    else /* UIO_READ */ {
	VOP_READ(((struct osi_file *)afile)->vnode, auio, 0, osi_credp, 
			NULL, code);
    }
    osi_PreemptionOff();
    return code;
}

