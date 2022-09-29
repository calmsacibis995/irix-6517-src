/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: xvfs_vfs2os.c,v 65.8 1999/02/08 20:52:41 mek Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1986-1995 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*      Copyright (c) 1990, 1991 UNIX System Laboratories, Inc. */
/*      Copyright (c) 1984, 1986, 1987, 1988, 1989, 1990 AT&T   */
/*        All Rights Reserved   */

/*      THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF          */
/*      UNIX System Laboratories, Inc.                          */
/*      The copyright notice above does not evidence any        */
/*      actual or intended publication of such source code.     */

/*
**      Module : xvfs_vfs2os.c
**
**      Purpose : This module provides routines that are used on the server
**                side of DFS. The file server (aka protocol exporter) calls
**                these routines when it wants to access the local file
**                system. The file server itself has been coded to a standard
**                set of vnodeops, called the XOPs, whcih are provided in
**                this module. The purpose of XOPs is to interface to the
**                vendor specific vnode ops. Thus, you will see the XOPs 
**                routines below invoking SGI vnodeops.
*/

#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/volume.h>
#include <dcedfs/xvfs_vnode.h>
#include <dcedfs/xvfs_xattr.h>
#include <dcedfs/dacl.h>
#include <dcedfs/osi_user.h>
#include <dcedfs/osi_uio.h>
#include <dcedfs/debug.h>
#include <dcedfs/direntry.h>
#include <dcedfs/afs4int.h>	/* Included to keep compiler from complaining
					about use of afsACL */

/* XXX-RAG delete ufs/inode.h and add some more files */
#include <sys/pathname.h>
#include <sys/fcntl.h>
#include <sys/vfs.h>
/* Define R_* here just like kern/os/vnode.c file */
#ifndef SGIMIPS			/*  These are now in unistd.h */
#define EFF_ONLY_OK     010     /* use effective ids instead of real */
#define EX_OK           020     /* check if a regular, executable file */
#define R_OK            004
#define W_OK            002
#define X_OK            001
#endif
#include <sys/proc.h>		/* To pick up curprocp */
#include <sys/kmem.h>

/*
#include <ufs/inode.h>
*/

extern unsigned int xfs_get_fid_ino (struct fid *);
extern unsigned int xfs_get_fid_gen (struct fid *);



/*
** Name : px2sgi_open
**
** Input : None
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : Normally called during open processing.
**
** Observations: The protocol exporter does not call open. Hence this is a
**               noop for now.
*/
static int
px2sgi_open(struct vnode **vpp, long i, osi_cred_t *credp)
{
        return (0);       /* The protocol exporter does not call open */
}

/*
** Name : px2sgi_close
**
** Input : None.
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : Normally invoked upon file close.
**
** Observations: The protocol exporter does not call close. Hence this is a
**               noop for now.
*/
static int
px2sgi_close(struct vnode *vp, long i, osi_cred_t *credp)
{
        return (0);       /* The protocol exporter does not call close */
}

/*
** Name : px2sgi_rdwr
**
** Input : vp - pointer to vnode of file which is being read/written
**         uiop - structure that describes the io to be done
**         rw - indicates whether a read or a write is being done
**         ioflag - indicates flags associated with io, such as direct, etc.
**         credp - pointer to user credentials structure
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : Called to do file io.
**
** Observations: Code modelled after OSF1 version.
*/
static int
px2sgi_rdwr (vp, uiop, rw, ioflag, credp)
    struct vnode *vp;
    struct uio *uiop;
    enum uio_rw rw;
    int ioflag;
    osi_cred_t *credp;
{
    int code;
    bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(vp);/* assume dfs bhv is 1st */

    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_rdwr: vp=0x%x, rw=%d\n", vp, rw));

    osi_RestorePreemption(0);
    if (rw == UIO_READ) {
	VOPN_RWLOCK (bdp, VRWLOCK_READ);
        VOPN_READ(bdp, uiop, ioflag, credp, &curuthread->ut_flid, code);
	VOPN_RWUNLOCK (bdp, VRWLOCK_READ);
    }
    else {
	VOPN_RWLOCK (bdp, VRWLOCK_WRITE);
        VOPN_WRITE(bdp, uiop, ioflag, credp, &curuthread->ut_flid, code);
	VOPN_RWUNLOCK (bdp, VRWLOCK_WRITE);
    }

    osi_PreemptionOff();
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_rdwr: code = %d\n", code));
    return(code);
}

/*
** Name : px2sgi_ioctl
**
** Input : None.
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : Normally called for IOCTL processing.
**
** Observations: The protocol exporter never invokes this operation.
*/
static int
px2sgi_ioctl(struct vnode *vp, int int1, void *arg)
{
        return (0);
}

/*
** Name : px2sgi_select
**
** Input : None.
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : Normally called to process select(2) calls.
**
** Observations: The protocol exporter never invokes this operation.
*/
static int
px2sgi_select(struct vnode *vp, int int1, osi_cred_t *credp)
{
        return (0);
}

/*
** Name : px2sgi_getattr
**
** Input : vp - pointer to vnode of file whose attributes are being sought
**         xvattrp - pointer to struct where attributes will be deposited
**         flag - indicates whether extended attributes are needed or not
**         credp - pointer to use credenetials structure
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : Called by the protocol exporter in lots of places.
**
** Observations: Modelled after the OSF1 and HPUX code.
*/
static int
px2sgi_getattr(vp, xvattrp, flag, credp)
   struct vnode *vp;
   struct xvfs_attr *xvattrp;
   int flag;
   osi_cred_t *credp;
{
    int code;
    struct afsFid Fid;
    long afsAccess;
    bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(vp);/* assume dfs bhv is 1st */

          
    if (flag)
        bzero(xvattrp, sizeof(struct xvfs_attr));
    else
        bzero(xvattrp, sizeof(struct vattr));

    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_getattr: vp=%x\n", vp));

    osi_RestorePreemption(0);
    VOPN_GETATTR(bdp, &xvattrp->vattr, 0 /*flags */, credp, code);

    if (!flag) {
        osi_PreemptionOff();
        return(code);    
    }

    /*
     * Fill the ep_attrs fields: In most cases they're not used by 
     * the UFS implementation
     */
    VOPX_AFSFID(vp, &Fid, 0, code);                    /* Ignore errors */
    if (!code) {                    /* Ignore errors */
	AFS_hset64(xvattrp->xvattr.fileID, Fid.Vnode, Fid.Unique);
    }
    /*
     * Setup xvattr.callerAccess
     */
    afsAccess = 0;
    VOPN_ACCESS(bdp, VREAD, credp, code);
    if (code == 0) 
        afsAccess |= dacl_perm_read;
    VOPN_ACCESS(bdp, VWRITE, credp, code);
    if (code == 0) {
       if (OSI_ISDIR(vp)) 
           afsAccess |= (dacl_perm_insert|dacl_perm_delete|dacl_perm_write);
       else 
           afsAccess |= dacl_perm_write;
    }
    VOPN_ACCESS(bdp, VEXEC, credp, code);
    if (code == 0) 
        afsAccess |= dacl_perm_execute;
    code = 0;

    if (xvattrp->vattr.va_uid == credp->cr_uid || xvfs_IsAdminGroup(credp) ||
        credp->cr_uid == 0 )
        afsAccess |= dacl_perm_control;

    /* XXX-RAG
     * Is va_mtime guaranteed to identify a unique update ??
     */
    AFS_hset64(xvattrp->xvattr.dataVersion, xvattrp->vattr.va_mtime.tv_sec,
    		xvattrp->vattr.va_mtime.tv_nsec/1000);
    xvattrp->xvattr.author = xvattrp->vattr.va_uid;
    xvattrp->xvattr.callerAccess = afsAccess;
    /*
     * Set up the minimum access rights for any caller, ie., anonAccess.
     * Note that anonAccess is an optimization used by CM for a "quick" check.
     */
    afsAccess = 0;
    if (((xvattrp->vattr.va_mode & 07) & R_OK)        &&  /* AND "other" */
        (((xvattrp->vattr.va_mode >> 3) & 07) & R_OK) &&  /* AND "group" */
        (((xvattrp->vattr.va_mode >> 6) & 07) & R_OK))    /* AND "owner" */
        afsAccess |= dacl_perm_read;

    if (((xvattrp->vattr.va_mode & 07) & W_OK)        &&  /* AND "other" */
        (((xvattrp->vattr.va_mode >> 3) & 07) & W_OK) &&  /* AND "group" */
        (((xvattrp->vattr.va_mode >> 6) & 07) & W_OK))    /* AND "owner" */
       if (OSI_ISDIR(vp))
           afsAccess |= (dacl_perm_insert | dacl_perm_delete);
       else
           afsAccess |= dacl_perm_write;

    if (((xvattrp->vattr.va_mode & 07) & X_OK)        &&  /* AND "other" */
        (((xvattrp->vattr.va_mode >> 3) & 07) & X_OK) &&  /* AND "group" */
        (((xvattrp->vattr.va_mode >> 6) & 07) & X_OK))    /* AND "owner" */
        afsAccess |= dacl_perm_execute;

    xvattrp->xvattr.anonAccess = afsAccess;
    xvattrp->xvattr.parentVnode = -1;
    xvattrp->xvattr.parentUnique = -1;
    xvattrp->xvattr.fstype = AG_TYPE_UFS;

out:
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_getattr: code = %d\n", code));
    osi_PreemptionOff();
    return code;
}

/*
** Name : px2sgi_setattr
**
** Input : vp - pointer to vnode of file whose attributes are being set
**         xvattrp - pointer to attributes structure
**         xflag - indicates whether extended attributes are being set or not
**         credp - pointer to user credentials structure
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : 
**
** Observations:  Code modelled after HPUX version.
*/
static int
px2sgi_setattr(vp, xvattrp, xflag, credp)
   struct vnode *vp;
   struct xvfs_attr *xvattrp;
   int xflag;
   struct cred *credp;
{
    int code;
    int null_time = 0;
    struct cred *tcredp;
    bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(vp);/* assume dfs bhv is 1st */
        
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_setattr: vp=%x\n", vp));

    /* 
     * If the caller is the member of the system admin group. Let the caller 
     * assume root's privileges for the duration of setattr.
     *
     * Don't bash cr_uid it in place, since *credp is a shared structure.
     */
    /* XXX-RAG crcopy is different in SGI, so changing */
    tcredp = crdup(credp);     /* crfree's credp as side effect */
    if (xvfs_IsAdminGroup(credp)) 
	 tcredp->cr_uid = 0;
    
    osi_RestorePreemption(0);
    VOPN_SETATTR(bdp, &xvattrp->vattr, 0, tcredp, code);

    crfree(tcredp);             /* free credential */
done:
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_setattr: code = %d\n", code));
    osi_PreemptionOff();
    return code;
}

/*
** Name : px2sgi_access
**
** Input : 
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : 
**
** Observations: Code modelled after the OSF1 version.
*/
static int
px2sgi_access(vp, mode, credp)
   struct vnode *vp;
   int mode;
   struct cred *credp;
{
    int code;
    bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(vp);/* assume dfs bhv is 1st */

    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_access: vp=0x%x, mode=%d\n", vp, mode));

    osi_RestorePreemption(0);
    VOPN_ACCESS(bdp, mode, credp, code);
    osi_PreemptionOff();

    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_access: code = %d\n", code));
    return(code);
}

/*
** Name : px2sgi_lookup
**
** Input : 
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : 
**
** Observations: Code modelled after the HPUX version.
*/
static int
px2sgi_lookup(dvp, namep, vpp, credp)
   struct vnode *dvp;
   char *namep;
   struct vnode **vpp;
   struct cred *credp;
{
   int code;
   struct vnode *mvp = dvp;
   bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(dvp);/* assume dfs bhv is 1st */

   AFSLOG(XVFS_DEBUG, 1, ("px2sgi_lookup: dvp=0x%x, name=%s\n", dvp, namep));

   osi_RestorePreemption(0);
   VOPN_LOOKUP(bdp, namep, vpp, (struct pathname *)NULL, 0, mvp, credp, code);
   osi_PreemptionOff();

   if (!code) {
        /*
        **      Convert the vnode ops to DFS format
        */
        xvfs_convert(*vpp);
   }

   AFSLOG(XVFS_DEBUG, 1, ("px2sgi_lookup: code = %d\n", code));
   return code;
}

/*
** Name : px2sgi_create
**
** Input : 
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : 
**
** Observations: Code modelled after HPUX version.
*/
static int
px2sgi_create(dvp, namep, vattrp, excl, mode, vpp, credp)
    struct vnode *dvp;
    char *namep;
    struct vattr *vattrp;
    int excl;
    int mode;
    struct vnode **vpp;
    osi_cred_t *credp;
{
    int code, flags = 0;
    bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(dvp);/* assume dfs bhv is 1st */

    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_create: dvp=%x, name=%s, mode=%d\n", 
                           dvp, namep, mode));

    /*
     * can't create directories. use mkdir.
     */
    if (vattrp->va_type == VDIR)
        return (EISDIR);

    osi_RestorePreemption(0);
    VOPN_CREATE(bdp, namep, vattrp, excl, mode, vpp, credp, code);
    osi_PreemptionOff();

    if (!code && *vpp) {
        /* 
         * We convert its v_op pointer from the standard ufs_vnodeops 
         * to the extended xvnodeops.
         */
        xvfs_convert(*vpp);
    }
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_create: code = %d\n", code));
    return code;
}

/*
** Name : px2sgi_remove
**
** Input : 
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : 
**
** Observations: Code modelled after the HPUX version.
*/
static int
px2sgi_remove(dvp, namep, credp)
   struct vnode *dvp;
   char *namep;
   struct cred *credp;
{
    int code;
    bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(dvp);/* assume dfs bhv is 1st */

    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_remove: dvp=%x, name=%s\n", dvp, namep));
    osi_RestorePreemption(0);
    VOPN_REMOVE(bdp, namep, credp, code);
    osi_PreemptionOff();
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_remove: code = %d\n", code));
    return code;
}

/*
** Name : px2sgi_link
**
** Input : 
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : 
**
** Observations: Code modelled after the HPUX version. Code juggled the
**               parameters fvp and tdvp, as the way the PX passes them is
**               different from the way the SGI routines accept these two
**               parameters.
*/
static int
px2sgi_link(fvp, tdvp, namep, credp)
   struct vnode *fvp, *tdvp;
   char *namep;
   struct cred *credp;
{
    int code;
    bhv_desc_t *tbdp = VNODE_TO_FIRST_BHV(tdvp);/* assume dfs bhv is 1st */

    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_link: tdvp=%x, fvp=%x, name=%s\n", tdvp, fvp, namep));
    osi_RestorePreemption(0);
    VOPN_LINK(tbdp, fvp, namep, credp, code);
    osi_PreemptionOff();
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_link: code = %d\n", code));
    return (code);
}

/*
** Name : px2sgi_rename
**
** Input : 
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : 
**
** Observations: 
*/
static int
px2sgi_rename(odvp, onamep, ndvp, nnamep, credp)
   struct vnode *odvp, *ndvp;
   char *onamep, *nnamep;
   struct cred *credp;
{
    int code;
    bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(odvp);/* assume dfs bhv is 1st */

    AFSLOG(XVFS_DEBUG,1,("px2sgi_rename: odvp=%x, oname=%s, ndvp=%x, nname=%s\n",
	   odvp, onamep, ndvp, nnamep));
    osi_RestorePreemption(0);
    VOPN_RENAME(bdp, onamep, ndvp, nnamep, (struct pathname *)NULL,
				      credp, code);
    osi_PreemptionOff();
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_rename: code = %d\n", code));
    return code;
}

/*
** Name : px2sgi_mkdir
**
** Input : 
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : 
**
** Observations: 
*/
static int
px2sgi_mkdir (dvp, namep, vattrp, vpp, credp)
    struct vnode **vpp, *dvp;
    char *namep;
    struct vattr *vattrp;
    osi_cred_t *credp;
{
    int code;
    bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(dvp);/* assume dfs bhv is 1st */

    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_mkdir: dvp=%x, name=%s\n", dvp, namep));

    osi_RestorePreemption(0);
    VOPN_MKDIR(bdp, namep, vattrp, vpp, credp, code);
    osi_PreemptionOff();
    if (!code && *vpp)
        xvfs_convert(*vpp);
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_mkdir: code = %d\n", code));
    return code;
}

/*
** Name : px2sgi_rmdir
**
** Input : 
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : 
**
** Observations: Code modelled after the HPUX version. Passing in u.u_cdir
**		 as the current directory pointer to SGI rmdir. It is 
**		 used to make sure that the current directory is not
**		 removed. I wonder if the current directory of the client
**		 needs to be passed in here! But there is no way to get
**		 that information into the server here.
*/
static int
px2sgi_rmdir(dvp, namep, cdp, credp)
    struct vnode *dvp;
    char *namep;
    struct vnode *cdp;
    osi_cred_t *credp;
{
    int code;
    bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(dvp);/* assume dfs bhv is 1st */

    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_rmdir: dvp=%x, name=%s\n", dvp, namep));
    osi_RestorePreemption(0);
    VOPN_RMDIR(bdp, namep, curuthread->ut_cdir, credp, code);
    osi_PreemptionOff();
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_rmdir: code = %d\n", code));
    return code;
}

/*
** Name : px2sgi_readdir
**
** Input : 
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : 
**
** Observations: Code modelled after the HPUX version. Code juggled the
**               parameters fvp and tdvp, as the way the PX passes them is
**               different from the way the SGI routines accept these two
**               parameters.
*/
static int 
px2sgi_readdir(vp, uiop, credp, eofp, isPX)
    struct vnode *vp;
    struct uio *uiop;
    osi_cred_t *credp;
    int *eofp;
    int isPX;
{
    int code;
    int sink;
    /* REFERENCED */
    DEFINE_OSI_UERROR;
    bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(vp);/* assume dfs bhv is 1st */

    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_readdir: vp=%x\n", vp));
    osi_setuerror (0);

    /*
     * XXX RAG Assumes dir_min??? is same as dirent struct.
     * We are doing like SUN or OSF because we have offset
     * field in the dirent struct.
     * we support 32 and 64 bit environments.
     * 32 and 64 bit changes are dealt by the caller.
     * Look at code in kutils/dirent.h, cm and px.
     */
    osi_RestorePreemption(0);
    VOPN_RWLOCK(bdp, VRWLOCK_READ);
    VOPN_READDIR(bdp, uiop, credp, &sink, code);
    VOPN_RWUNLOCK(bdp, VRWLOCK_READ);
    osi_PreemptionOff();
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_readdir: code = %d\n", code));
    return (code);
}


int px2sgi_symlink(dvp, namep, vattrp, linkcontentsp, credp)
    struct vnode *dvp;
    char *namep, *linkcontentsp;
    struct vattr *vattrp;
    osi_cred_t *credp;
{
    int code;
    struct vnode *vp;
    struct vnode *mvp = dvp; /* xxx */
    /* REFERENCED */
    DEFINE_OSI_UERROR;
    bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(dvp);/* assume dfs bhv is 1st */
    
    osi_setuerror (0);
    osi_RestorePreemption(0);
    VOPN_SYMLINK(bdp, namep, vattrp, linkcontentsp, credp, code);
    if (!code) {
        /* obtain a vnode ptr to the just created symlink */
        VOPN_LOOKUP(bdp, namep, &vp, (struct pathname *)NULL, 0, 
			   mvp, credp, code);
	if (!code) {
	    xvfs_convert(vp);
	    VN_RELE(vp);
	}
    }
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_symlink: code = %d\n", code));
    osi_PreemptionOff();
    return (code);
}


int px2sgi_readlink(vp, uiop, credp)
    struct vnode *vp;
    struct uio *uiop;
    osi_cred_t *credp;
{
    int code;
    /* REFERENCED */
    DEFINE_OSI_UERROR;
    bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(vp);/* assume dfs bhv is 1st */

    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_readlink: vp=%x\n", vp));
    osi_setuerror (0);
    osi_RestorePreemption(0);
    VOPN_READLINK(bdp, uiop, credp, code);
    osi_PreemptionOff();
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_readlink: code = %d\n", code));
    return (code);
}


/*
 * This routine is in the VOPX vector,
 * but is really only called by px2sgi_afsfid().
 * This procedure is dependent on xfs fid definition.
 * fid_ino and fid_gen are both 32 bit in xfs. xfs
 * does have 64 bit fid_ino, xfs_get_fid_ino() only
 * gives us the lower 32 bits.
 */
int px2sgi_fid( struct vnode *vp, struct fid **fidpp)
{
    int code;
    bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(vp);/* assume dfs bhv is 1st */
    /* REFERENCED */
    DEFINE_OSI_UERROR;

    osi_setuerror (0);
    VOPN_FID(bdp, fidpp, code);
    return (code);
}


int px2sgi_afsfid(vp, afsFidp, wantv) 
    struct vnode *vp;
    struct afsFid *afsFidp;
    int wantv;				/* want volume field filled in, too */
{
    struct volume *volp=NULL;
    int code;
    struct fid *fidp = NULL;
    /* REFERENCED */
    DEFINE_OSI_UERROR;

    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_afsfid: vp=%x(%x) = ", vp, vp->v_fops));
    osi_setuerror (0);
    code = px2sgi_fid(vp, &fidp);
    if (!code) {
	afsFidp->Vnode = xfs_get_fid_ino(fidp);
	afsFidp->Unique = xfs_get_fid_gen(fidp);

	if (wantv) {
	    afsFidp->Cell = tkm_localCellID;
	    code = xvfs_GetVolume(vp, &volp);
	    if (!code) {
		afsFidp->Volume = volp->v_volId;
		xvfs_PutVolume(volp);
	    } else
                AFS_hzero(afsFidp->Volume);
	}
	freefid(fidp);
    }
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_fid: code=%d [vnode=0x%x, unique=0x%x]\n", 
	   code, afsFidp->Vnode, afsFidp->Unique));
    return code;
}


/* 
 * The call to convert a vnode pointer into a held volume pointer.
 *
 * XXXXXX UFS SPECIFIC - FIX THIS!!!! XXXXX
 */
int px2sgi_getvolume(vp, volpp)
    struct vnode *vp;
    struct volume **volpp; 
{
    if (vol_FindVfs(vp->v_vfsp, volpp))
      return EIO;
    return 0;
}



int px2sgi_getacl(
    struct vnode *vp,
    struct dfs_acl *AccessListp,
    int int1,
    osi_cred_t *credp)
{
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_getacl: vp=0x%x\n", vp));

    /*
     * Currently UFS doesn't handle ACL. So we simply return the following
     */
    return ENOTTY;
}

int px2sgi_setacl(
    struct vnode *vp,
    struct dfs_acl *AccessListp,
    struct vnode *svp,
    int int1,
    int int2,
    osi_cred_t *credp)
{
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_setacl: vp=0x%x\n", vp));

    /*
     * Currently UFS doesn't handle ACL. So we simply return the following
     */
    return ENOTTY;
}



int px2sgi_quota(vp, quotaStr, quotaLen, credp)
     struct vnode *vp;
     char *quotaStr;
     long quotaLen;
     osi_cred_t *credp; 
{
    int code = EINVAL;
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_quota: vp=%x, quotaStr='%s'\n", vp,
			   quotaStr));
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_quota: code=%d\n", code));
    return code;
}


int px2sgi_getlength(
    struct vnode *vp,
    off_t *lenp,
    osi_cred_t *cred)
{
    struct vattr vattr;
    int code;
    osi_cred_t **savedcredp;
    bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(vp);/* assume dfs bhv is 1st */
    /* REFERENCED */
    DEFINE_OSI_UERROR;

    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_getlength: vp=0x%x ", vp));

    *lenp = 0;
    osi_setuerror (0);
    osi_RestorePreemption(0);
    vattr.va_mask = AT_SIZE;
    VOPN_GETATTR(bdp, &vattr, 0, cred, code);
    if (!code) {
	*lenp = (long) vattr.va_size;
    }
    AFSLOG(XVFS_DEBUG, 1, ("len=%d (code = %d)\n", *lenp, code));
    osi_PreemptionOff();
    return(code);
}



int px2sgi_fsync(
    struct vnode *vp,
    int flag,
    osi_cred_t *credp,
    off_t start,
    off_t stop)
{
    int code;
    bhv_desc_t *bdp = VNODE_TO_FIRST_BHV(vp);/* assume dfs bhv is 1st */
    /* REFERENCED */
    DEFINE_OSI_UERROR;

    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_fsync: vp=%x\n", vp));
    osi_setuerror (0);
    /* 
     * XXX RAG Do we wait for  fsync to finish ?
     * Flag of 0 means NOWAIT
     */
    osi_RestorePreemption(0);
    VOPN_FSYNC(bdp, 0, credp, start, stop, code);
    osi_PreemptionOff();
    AFSLOG(XVFS_DEBUG, 1, ("px2sgi_fsync: code=%d\n", code));
    return code;
}


int px2sgi_nosys()
{
    AFSLOG(XVFS_DEBUG, 0, 
	   ("px2sgi_nosys: UNSUPPORTED... EXITING\n"));
    return EINVAL;
}


int px2sgi_vfsgetvolume (vfsp, volpp)
    struct vfs *vfsp;			/* virtual file system */
    struct volume **volpp;		/* where to put volume ptr */
{
    if (vol_FindVfs (vfsp, volpp))
      return EIO;
    return 0;
}

/*
 * The following functions are not called from the Protocol Exporter, but
 * may eventually be called from the CM express ops, if those are ever
 * gotten to work again.  In that event, these functions will have to be
 * implemented. For now, they are just no-ops.
 */

int px2sgi_inactive(struct vnode *vp, osi_cred_t *credp)  {return 0;}
int px2sgi_bmap(struct vnode *vp, long l, struct vnode **vpp, long *longp) 
		{return 0;}
int px2sgi_strategy(struct buf *bp) {return 0;}
int px2sgi_ustrategy(struct buf *bp) {return 0;}
int px2sgi_bread(struct vnode *vp, daddr_t addr, struct buf **bpp) {return 0;}
int px2sgi_brelse(struct vnode *vp, struct buf *bp) {return 0;}
int px2sgi_lockctl(struct vnode *vp, struct flock *flockp, int int1,
		osi_cred_t *credp, int int2, long long1) {return 0;}
int px2sgi_cmp() {return 0;}
int px2sgi_hold(struct vnode *vp) {return 0;}
int px2sgi_rele(struct vnode *vp) {return 0;}
int px2sgi_lock() {return 0;}
int px2sgi_unlock() {return 0;}
int px2sgi_setlength() {return 0;}

/*  Even though we'll never return from panic() add return to keep compiler
	happy, just to be safe we make it a -1 */
int px2sgi_panic() {panic("Invalid X-op"); return -1;}
void px2sgi_void_panic() {panic("Invalid X-op");}
/* Vector of X-ops */
/* 
 * XXX RAG need to change xfs_convert which assumes name to be xufs_ops
 */

struct xvfs_xops xufs_ops = {
    px2sgi_open,
    px2sgi_close,
    px2sgi_rdwr,
    px2sgi_ioctl,
    px2sgi_select,
    px2sgi_getattr,
    px2sgi_setattr,
    px2sgi_access,
    px2sgi_lookup,
    px2sgi_create,
    px2sgi_remove,
    px2sgi_link,
    px2sgi_rename,
    px2sgi_mkdir,
    px2sgi_rmdir,
    px2sgi_readdir,
    px2sgi_symlink,
    px2sgi_readlink,
    px2sgi_fsync,
    px2sgi_inactive,
    px2sgi_bmap,               /* bmap */
    px2sgi_strategy,               /* strategy */
    px2sgi_ustrategy,               /* ustrategy */
    px2sgi_bread,               /* bread */
    px2sgi_brelse,               /* brelse */
    px2sgi_lockctl,
    px2sgi_fid,
    px2sgi_hold,               /* hold */
    px2sgi_rele,               /* rele */
    px2sgi_setacl,
    px2sgi_getacl,
    px2sgi_afsfid,
    px2sgi_getvolume,
    px2sgi_getlength,
    (int (*)(struct vnode *, u_int))px2sgi_panic,
				                       /* map (SunOS, AIX) */
    (int (*)(struct vnode *, int))px2sgi_panic,        /* unmap (AIX) */
    (int (*)(struct vnode *))px2sgi_panic,               /* reclaim (OSF/1) */
    (int (*)(struct vnode *, struct uio *, int, osi_cred_t *))px2sgi_panic,
				               /* various SunOS ops ... */
    (int (*)(struct vnode *, struct uio *, int, osi_cred_t *))px2sgi_panic,
    (int (*)(struct vnode *, struct vnode **))px2sgi_panic,
    (void (*)(struct vnode *, int))px2sgi_void_panic,
    (void (*)(struct vnode *, int))px2sgi_void_panic,
    px2sgi_panic,
    px2sgi_panic,
    px2sgi_panic,
    px2sgi_panic,
    px2sgi_panic,
    px2sgi_panic,
    px2sgi_panic,               /* ... last of SunOS ops */
    px2sgi_panic,               /* pagein (HP/UX) */
    px2sgi_panic                /* pageout (HP/UX) */
};
