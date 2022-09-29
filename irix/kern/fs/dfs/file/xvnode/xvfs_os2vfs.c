/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: xvfs_os2vfs.c,v 65.11 1998/12/07 18:58:10 gwehrman Exp $";
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

#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/stds.h>
#include <dcedfs/xvfs_vnode.h>
/*  Include systm.h before debug.h so that the prototype for
	dprintf() is picked up.  This is so the compiler doesn't
	complain because debug.h redefines dprintf() to be nothing.  */
#include <sys/systm.h>			/* XXX-RAG for rval_t */
#include <dcedfs/debug.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_user.h>
#include <dcedfs/osi_uio.h>
#include <dcedfs/osi_buf.h>
#include <sys/pathname.h>
#include <sys/kfcntl.h>
#include <sys/fcntl.h>			/* XXX-RAG should this be included thru osi?? */
#include <sys/fs_subr.h>		/* XXX-BRAT for fs_* functions */
#ifdef DEBUG
#include <sys/cmn_err.h>
#endif /* DEBUG */
#include <sys/vnode.h>
#include <sys/flock.h>
#include <sys/kabi.h>
#include <sys/proc.h>

int irix5_n32_to_flock(enum xlate_mode, void *, int , xlate_info_t *);

/*
**  Module : xvfs_os2vfs
**
**  Purpose : This module contains various routines that conform to SGI
**            vnodeops syntax. These routines then call DFS's XOPs that conform
**            to an old Sun vnodeops syntax.
**
**            The main components of DFS (the cache manager CM and the file
**            server aka protocol exporter px) were written to conform to the
**            XOPs syntax. This is done so that, when porting DFS, the CM
**            and px can be left untouched, and instead, these routines that
**            convert XOPs syntax calls to native vnodeops calls have to be
**            provided.
**
**            Many of the routines here trivially invoke the corresponding
**            XOP routine. If there is a difference in the parameter signature
**            between the native vnodeop and the XOP, that would have to be
**            taken care of.
**
**            The set of routines given here below are used in invoking the
**            Cache Manager (client side DFS) from the kernel calls. The
**            kernel calls the OOPs, which will invoke the NOPs given in this
**            module. The NOPs then invoke the XOPs, which correspond to
**            cm_* routines specified in the DFS source file tree of
**            file/cm/cm_vnodeops.c.
*/

/* Forward declarations */


/*
** Name : sgi2cm_open
**
** Input : vpp - pointet to pointer to vnode of file to be opened
**         flags - mode flags of the open
**         credp - pointer to user credentials structure
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : Invoked by xglue_open when opening a remotely accessed dfs file.
**
** Observations: There are no differences in the parameter list between 
**               SGI vnodeops open call and DFS open call. Hence this is just
**               a direct pass through routine.
*/
static int
sgi2cm_open(
    bhv_desc_t	*bdp,
    struct vnode **vpp,
    mode_t flags,
    cred_t *credp)
{
    int error = 0;
    vnode_t	*vp;

    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_open (flags = 0x%x\n",flags));

    VOPX_OPEN(vpp, flags, credp, error);

    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_open %d\n",error));
    return (error);
}

/*
** Name : sgi2cm_close
**
** Input :  vp - pointer to vnode of file being closed
**          flags - the file mode flags
**          lastclose - is L_TRUE, L_FALSE or FROM_VN_KILL
**          offset - the file offset from the file table entry
**          credp - pointer to the user credentials structure
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Called when user calls close(2) for a remote DFS file. Called 
**         from xglue_close in this instance.
**
** Observations: Not passing the offset and lastclose flags to the CM close
**               routine. Most of the filesystems don't seem to use the
**               offset parameter anyway. NFS seems to use the lastclose 
**               indication to do file flushing, etc.
*/
static int
sgi2cm_close(
    bhv_desc_t	*bdp,
    int flags,
    lastclose_t lastclose,      /* lastclose is either L_FALSE */
                                /*      L_TRUE or FROM_VN_KILL */
    cred_t *credp)
{
    int error;
    int preempting;
    struct vnode *vp;

    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_close (flags =0x%x)\n", flags));
    vp = BHV_TO_VNODE(bdp);
    preempting = osi_PreemptionOff();
    VOPX_CLOSE(vp, flags, credp, error);
    osi_RestorePreemption(preempting);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_close %d\n",error));
    return (error);
}

/*
** Name : sgi2cm_read
**
** Input :  vp - pointer to vnode of file which is being read
**          uiop - pointer to uio structure that describes the user buffer, etc
**          flags - specifies the io flags - direct, fsync, invis, etc.
**          credp - pointer to user credentials structure.
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Called when read(2) system call is executed for a remote DFS file.
**         Called from xglue_read in this case.
**
** Observations: Since the cm has a common rdwr call that takes an additional
**               parameter that specifies whether to do a read or a write,
**               we are simply invoking VOPX_RDWR, passing in UIO_READ as
**               the operation parameter. Not doing anything with massaging
**               the flags, in terms of removing flag bits that the cm won't
**               understand.
*/
static int
sgi2cm_read(
    bhv_desc_t	*bdp,
    struct uio *uiop,
    int flags,
    osi_cred_t *credp,
    flid_t	*fl)
{
    int error;
    struct vnode *vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_read (flags = 0x%x)\n", flags));

    if (uiop->uio_resid == 0)
	return 0;

    if ((uiop->uio_offset < 0) ||
        ((off_t)(uiop->uio_offset + uiop->uio_resid) < 0))
	return EINVAL;

    VOPX_RDWR(vp, uiop, UIO_READ,flags, credp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_read %d\n",error));
    return (error);
}

/*
** Name : sgi2cm_write
**
** Input :  vp - pointer to vnode of file which is being written
**          uiop - pointer to uio structure that describes the user buffer, etc
**          flags - specifies the io flags - direct, fsync, invis, etc.
**          credp - pointer to user credentials structure.
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Called when write(2) system call is executed for a remote DFS file.
**         Called from xglue_write in this case.
**
** Observations: Since the cm has a common rdwr call that takes an additional
**               parameter that specifies whether to do a read or a write,
**               we are simply invoking VOPX_RDWR, passing in UIO_WRITE as
**               the operation parameter. Not doing anything with massaging
**               the flags, in terms of removing flag bits that the cm won't
**               understand.
*/
static int
sgi2cm_write(
    bhv_desc_t	*bdp,
    struct uio *uiop,
    int flags,
    osi_cred_t *credp,
    flid_t	*fl)
{
    int error;
    struct vnode *vp;

    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_write (flags = 0x%x)\n", flags));
    vp = BHV_TO_VNODE(bdp);

    if (uiop->uio_resid == 0)
        return 0;

    if ((uiop->uio_offset < 0) ||
        ((off_t)(uiop->uio_offset + uiop->uio_resid) < 0))
        return EINVAL;

    if ((vp->v_type == VREG) &&
        (uiop->uio_offset + uiop->uio_resid > uiop->uio_limit))
	return EFBIG;

    VOPX_RDWR(vp, uiop, UIO_WRITE,flags, credp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_write %d\n",error));
    return (error);
}

/*
** Name : sgi2cm_ioctl
**
** Input :  vp - vnode of file on which ioctl is being done
**          cmd - command that specifies the type of the ioctl
**          arg - data associated with above command
**          flags - ???
**          credp - pointer to user credentials structure
**
** Output : rv - integer return value here???
**
** Returns : 0 on success and error code on failure.
**
** Notes : Called when ioctl(2) is invoked on a remote dfs file. Called by
**         xglue_ioctl in this case.
**
** Observations: We pass in only the three parameters, vp, cmd and arg that
**               cm_ioctl is expecting. IOCTL is not supported for remote 
**               files and cm_ioctl returns ENOTTY for this call. Hence the
**               unused parameters don't hurt.
*/
static int
sgi2cm_ioctl(
    bhv_desc_t	*bdp,
    int cmd,
    void *arg,
    int flags,
    osi_cred_t *credp,
    int *rv,
    struct vopbd *vbds)
{
    int error;
    struct vnode *vp;

    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_ioctl (flags = 0x%x)\n", flags));
    vp = BHV_TO_VNODE(bdp);
    VOPX_IOCTL(vp, cmd, arg, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_ioctl %d\n",error));
    return (error);
}

/*
** Name : sgi2cm_setfl
**
** Input :  vp - pointer to vnode of file for which setfl is being done
**          oflags - value of old flags
**          nflags - value of new flags
**          credp - pointer to user credentials structure
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Callers of VOP_SETFL include fcntl (when cmd is F_SETFL), vn_open
**         when creating a file or opening a file. All file systems (efs and
**         xfs supply fs_noerr for this call, which just returns zero. nfs
**         and nfs3 supply fs_setfl (defined in fs_subr.c) for this call,
**         which just makes sure nflags does not specify FDIRECT.
**
** Observations: Do some logging and then just turn around and call fs_setfl.
*/
static int
sgi2cm_setfl(
    bhv_desc_t	*bdp,
    int oflags,
    int nflags,
    cred_t *credp)
{
    int error;

    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_setfl (flags = 0x%x)\n", flags));
    error = fs_setfl(bdp, oflags, nflags, credp);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_setfl %d\n",error));
    return (error);
}

/*
** Name : sgi2cm_getattr
**
** Input :  vp - pointer to vnode of file for which getattr is being called
**          flags - the cm doesn't seem to be using this parameter, but it
**                  specifies additional attributes needed for xfs or
**                  ATTR_LAZY for nfs. exec seems to call it with ATTR_EXEC.
**          credp - pointer to user credentials structure.
**
** Output:  vattrp - pointer to attributes structure where attribute values
**                   will be returned.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Called in lots of places to get attributes assocaited with a file.
**
** Observations: We have to check to see if the cm is filling all the 
**               attributes SGI is expecting or not. If it is not, we
**               might have to fill appropriate values. Such machine
**               dependent code seems to exist intermixed with the cm code
**               itself. Hence, we just call the cm routin here directly.
*/
static int
sgi2cm_getattr(
    bhv_desc_t	*bdp,
    struct vattr *vattrp,
    int flags,
    osi_cred_t *credp)
{
    int error;
    struct vnode *vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_getattr\n"));
    VOPX_GETATTR(vp, (struct xvfs_attr *)vattrp, flags, credp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_getattr %d\n", error));
    return (error);
      
}

/*
** Name : sgi2cm_setattr
**
** Input :  vp - pointer to vnode of file for which setattr is being called
*           vattrp - pointer to attributes structure that specifies the
**                   attributes to be set and their values
**          flags - specifies ATTR_LAZY, ATTR_EXEC and other xfs related
**                  additional attributes
**          credp - pointer to user credentials structure
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Called to set the attributes associated with a file.
**
** Observations: Just call the cm setattr routine. What the cm sends over the
**               wire has a fixed format, irrespective of the type of the
**               server file system. Hence, the cm calls a translate routine,
**               cm_TranslateStatus to translate machine specific attributes
**               structure into one that can be sent over the wire. The
**               cm_TranslateStatus routine calls various osi routines, such
**               as osi_setting_mode, osi_setting_mtime, etc. to figure out
**               which attributes that it knows and cares about are getting
**               modified. It then fills the over the wire structure and 
**               does an rpc to store status at the file server.
*/
static int
sgi2cm_setattr(
    bhv_desc_t	*bdp,
    struct vattr *vattrp,
    int flags,
    osi_cred_t *credp)
{
    int error;
    struct vnode *vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_setattr\n"));
    VOPX_SETATTR(vp, (struct xvfs_attr *)vattrp, flags, credp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_setattr %d\n", error));
    return (error);
}

/*
** Name : sgi2cm_access
**
** Input : vp - pointer to vnode of file for which VOP_ACCESS is being called
**         mode - type of access desired - read/write/exec
**         flags - seems to be an unused parameter.
**         credp - pointer to user credentials structure
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Called as a result of access(2) system call, as well as other
**         kernel internal routines call VOP_ACCESS (for example exec, core
**         dump creation, etc.).
**
** Observations: SGI access routine seems to have the additional parameter,
**               flags, which the cm does not have. Since all callers of
**               VOP_ACCESS seem to supply zero, and since all file systems
**               seem to not bother with this flags parameter, we are just
**               dropping it and not passing it on to the cm.
*/
static int
sgi2cm_access(
    bhv_desc_t	*bdp,
    int mode,
    osi_cred_t *credp)
{
    int error;
    struct vnode *vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_access\n"));
    VOPX_ACCESS(vp, mode, credp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_access %d\n", error));
    return (error);
}

/*
** Name : sgi2cm_lookup
**
** Input : dp - parent directory vnode pointer in which to lookup namep
**         namep - null terminated string that specifies the name of the file
**                 which is being looked up.
**         pnp - path name structure that contains name lookup cache hash
**               information for SGI dnlc.
**         flags - can be either LOOKUP_DIR or zero. file system layer
**                 doesn't seem to use it.
**         mvp - the filesystem layer doesn't seem to care about this param.
**         credp - pointer to the user credentials structure.
**
** Output : Returns the held vnode of the file corresponding to namep in vpp.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Called only in one place in the kernel, in lookuppn. Everyone
**         else calls lookupname, which calls lookuppn.
**
** Observations: SGI lookup routine gets passed the pnp pointer because
**               it uses the hash index in the pnp structure to look up
**               in the dnlc. Since the cm does not have this parameter, we
**               just drop it in our call to the VOPX routine.
**
**               We also drop the unused parameters, flags and mvp.
*/
static int
sgi2cm_lookup(
    bhv_desc_t	*dir_bdp,
    char *namep,
    struct vnode **vpp,
    pathname_t *pnp,
    int flags,
    struct vnode *mvp,
    osi_cred_t *credp)
{
    int error;
    struct vnode *dp;

    dp = BHV_TO_VNODE(dir_bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_lookup\n"));
    VOPX_LOOKUP(dp, namep, vpp, credp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_lookup %d\n", error));
    return (error);
      
}

/*
** Name : sgi2cm_create
**
** Input :  dp - pointer to vnode of directory in whihc file is being created
**          namep - name of file entry being created
**          vattrp - pointer to attributes structure that specifies attributes
**                   of file being created
**          excl - enum that specifies either nonexclusive or exclusive create.
**          mode - file permissions modes
**          credp - pointer to user credentials structure
**
** Output : Returns pointer to vnode of file created in *vpp.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Used by vn_create in the kernel to create a file.
**
** Observations: There are no parameter differences between SGI create and
**               the cm create. Hence, this is just a pass through routine.
*/
static int
sgi2cm_create(
    bhv_desc_t	*dir_bdp,
    char *namep,
    struct vattr *vattrp,
    int excl,
    int mode,
    struct vnode **vpp,
    osi_cred_t *credp)
{
    int error;
    vnode_t	*dp;

    dp = BHV_TO_VNODE(dir_bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_create\n"));
    VOPX_CREATE(dp, namep, vattrp, excl, mode, vpp, credp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_create %d\n", error));
    return (error);
}

/*
** Name : sgi2cm_remove
**
** Input :  dp - pointer to vnode of directory entry in which namep is being
**               removed.
**          namep - pointer to character string that contains name of file
**                  being removed.
**          credp - pointer to user credentials structure
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : vn_remove, which gets called for unlink(2) system calls, invokes
**         VOP_REMOVE.
**
** Observations: No parameter differences between SGI and cm. Hence this is
**               just a passthrough call.
*/
static int
sgi2cm_remove(
    bhv_desc_t	*dir_bdp,
    char *namep,
    osi_cred_t *credp)
{
    int error;
    vnode_t	*dp;

    dp = BHV_TO_VNODE(dir_bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_remove\n"));
    VOPX_REMOVE(dp, namep, credp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_remove %d\n", error));
    return (error);
}

/*
** Name : sgi2cm_link
**
** Input :  tdvp - specifies the vnode pointer of the directory which is
**                 to contain the new link
**          fvp - pointer to vnode of existing file to which new link is
**                being created
**          namep - string that contains the name of the new link
**          credp - pointer to user credentials structure
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : Called by vn_link, which gets called by link(2).
**
** Observations: The SGI link and the cm link juggled the tdvp and fvp
**               parameters. Hence this routine just juggles them and invokes
**               cm link routine.
*/
static int
sgi2cm_link(
    bhv_desc_t	*tbdp,
    struct vnode *fvp,
    char *namep,
    osi_cred_t *credp)
{
    int error;
    int preempting;
    struct vnode *tdvp;

    tdvp = BHV_TO_VNODE(tbdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_link\n"));
    preempting = osi_PreemptionOff();
    VOPX_LINK(fvp, tdvp, namep, credp, error);
    osi_RestorePreemption(preempting);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_link %d\n", error));
    return (error);     
}

/*
** Name : sgi2cm_rename
**
** Input :  fromdp - pointer to vnode of directory in which fromnamep exists
**          fromnamep - current name of file being renamed
**          todp - pointer to vnode of directory to which file is being moved
**          tonamep - new name of file
**          topnp - path name structure containing dnlc hash value, etc.   
**          credp - pointer to user credentials structure
**
** Output : None
**
** Returns : 0 on success and error code on failure.
**
** Notes : Called when rename(2) is called.
**
** Observations: The SGI rename has one additional component, tpnp, as 
**               compared to the cm rename. Since the cm does not need
**               this component, we just drop it and call the cm.
*/
static int
sgi2cm_rename(
    bhv_desc_t	*bdp,
    char *fromnamep,
    struct vnode *todp,
    char *tonamep,
    struct pathname *topnp,
    osi_cred_t *credp)
{
    int error;
    int preempting;
    struct vnode *fromdp;

    fromdp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_rename\n"));
    preempting = osi_PreemptionOff();
    VOPX_RENAME(fromdp, fromnamep, todp, tonamep, credp, error);
    osi_RestorePreemption(preempting);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_rename %d\n", error));
    return (error);      
}

/*
** Name : sgi2cm_mkdir
**
** Input :  dp - pointer to vnode of directory in which new directory is being
**               created
**          namep - pointer to string containing name of directory being created
**          vattrp - pointer to attributes structure specifying attributes of
**                   directory being created
**          credp - pointer to user credentials structure
**
** Output : pointer to vnode of created directory is returned in *vpp
**
** Returns : 0 on success and error code on failure.
**
** Notes : Called upon an mkdir(2) system call
**
** Observations: Since there are no differences in parameter list between cm
**               mkdir and SGI mkdir, this is just a pass through routine.
*/
static int
sgi2cm_mkdir(
    bhv_desc_t	*bdp,
    char *namep,
    struct vattr *vattrp,
    struct vnode **vpp,
    osi_cred_t *credp)
{
    int error;
    struct vnode *dp;

    dp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_mkdir\n"));
    VOPX_MKDIR(dp, namep, vattrp, vpp, credp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_mkdir %d\n", error));
    return (error);
      
}

/*
** Name : sgi2cm_rmdir
**
** Input : dp - pointer to vnode of parent directory
**         namep - name of directory being removed
**         current_dir_vp - pointer to vnode of current directory of calling 
**                          process
**         credp - pointer to user credentials strcture.
**
** Output : None.
**
** Returns : 0 on success, error otherwise.
**
** Notes : Invoked when rmdir(2) system call is made. vn_remove calls 
**         VOP_RMDIR for removing directories.
**
** Observations: The cm rmdir routine seems to take an unused third param
**               called junkcp (which is defined as a char * in cm_vnodeops.c).
**               We will just pass in 0 here (just as HPUX is doing).
*/
static int
sgi2cm_rmdir(
    bhv_desc_t	*bdp,
    char *namep,
    struct vnode *current_dir_vp,
    osi_cred_t *credp)
{
    int error;
    int preempting;
    struct vnode *dp;

    dp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_rmdir\n"));
    preempting = osi_PreemptionOff();
    VOPX_RMDIR(dp, namep, (struct vnode *) 0, credp, error);
    osi_RestorePreemption(preempting);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_rmdir %d\n", error));
    return (error);      
}

/*
** Name : sgi2cm_readdir
**
** Input : vp - pointer to vnode of directory entry whose contents are to 
**              be read.
**         uiop - pointer to uio structure that gives the parameters 
**                associated with the io, such as count, offset, etc.
**         credp - pointer to user credentials structure
**         eofp - pointer to an int location where a boolean indication 
**                of eof is returned.
**
** Output : None.
**
** Returns : 0 upon success, an error code otherwise.
**
** Notes : Called as a result of getdents(2) system call.
**
** Observations: The cm readdir has an additional parameter, called isPX,
**		 which is ignored and required to be zero. Hence, we will
**		 just append this zero parameter and call the vopx routine.
*/
static int
sgi2cm_readdir(
    bhv_desc_t	*bdp,
    struct uio *uiop,
    osi_cred_t *credp,
    int *eofp)
{
    int error;
    struct vnode *vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_readdir\n"));
    VOPX_READDIR(vp, uiop, credp, eofp, 0, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_readdir %d\n", error));
    return (error);      
}

/*
** Name : sgi2cm_symlink
**
** Input : vp - pointer to vnode of directory that should contain the 
**              symbolic link.
**         namep - pointer to character string of symbolic link name.
**         vattrp - pointer to attributes structure that gives the attributes 
**                  of the created symbolic link.
**         targetp - pointer to character string that contains the name of 
**                   the entry that the symbolic link should point to. The 
**                   name mentioned by targetp need not exist. The name 
**                   targetp just gets written into the symbolic link's 
**                   contents.
**         credp - pointer to user credentials structure.
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : This procedure is called when a symlink(2) system call is made. 
**         The symlink procedure in os/vncalls.c invokes VOP_SYMLINK.
**
** Observations: Since there are no differences in parameter lists between
**		 the SGI and cm versions, this is just a passthrough proc.
*/
static int
sgi2cm_symlink(
    bhv_desc_t	*bdp,
    char *namep,
    struct vattr *vattrp,
    char *targetp,
    osi_cred_t *credp)
{
    int error;
    struct vnode *vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_symlink\n"));
    VOPX_SYMLINK(vp, namep, vattrp, targetp, credp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_symlink %d\n", error));
    return (error);
}

/*
** Name : sgi2cm_readlink
**
** Input : vp - pointer to the vnode of the symbolic link whose contents 
**              are being read.
**         uiop - pointer to uio structure which give the parameters 
**                associated with the io, such as byte count, offset, etc.
**         credp - pointer to user's credentials structure.
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : This function is called when a readlink(2) system call is made. 
**         The readlink function in os/vncalls.c invokes VOP_READLINK, 
**         which results in this function being called for
**	   remote DFS files.
**
** Observations: This is simply a passthrough procedure that invokes the 
**		 corresponding vopx routine, as there are no differences in
**		 parameter lists between the SGI and cm versions.
*/
static int
sgi2cm_readlink(
    bhv_desc_t	*bdp,
    struct uio *uiop,
    osi_cred_t *credp)
{
    int error;
    struct vnode *vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_readlink\n"));
    VOPX_READLINK(vp, uiop, credp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_readlink %d\n", error));
    return (error);
}

/*
** Name : sgi2cm_fsync
**
** Input : vp - pointer to vnode of file which is being synced
**         flag - specifies wait/nowait, data to be flushed or not and 
**                cache to be invalidated or not.
**         credp - pointer to user's credentials structure.
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : xglue_fsync is invoked when an fsync(2) system call is made.
**	   Called from msync proc of os/mmap.c, swapadd for os/swapalloc.c
**	   and fsync and fdatasync procs of os/vncalls.c.
**
** Observations: The cm does not have the flags parameter. Hence, for now,
**		 we are just leaving that parameter and invoking the cm.
**		 Need to check the validity of this later!!!!!!!!!!!!!!
*/
static int
sgi2cm_fsync(
    bhv_desc_t	*bdp,
    int flag,
    osi_cred_t *credp,
    off_t start,
    off_t stop)
{
    int error;
    struct vnode *vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_fsync\n"));
    VOPX_FSYNC(vp, flag, credp, start, stop, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_fsync %d\n", error));
    return (error);      
}

/*
** Name : sgi2cm_inactive
**
** Input : vp - pointer to vnode which is being inactivated.
**	   credp - pointer to user credentials structure
**
** Output : None.
**
** Returns : None.
**
** Notes : VOP_INACTIVE is invoked when a vnode reference count goes down 
**         to zero. vn_rele from os/vnode.c invokes VOP_INACTIVE.
**
** Observations: This is a simple pass through routine.
*/
static int
sgi2cm_inactive(
    bhv_desc_t	*bdp,
    osi_cred_t *credp)
{
    struct vnode *vp;
    int error=0;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_inactive\n"));
    VOPX_INACTIVE(vp, credp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_inactive %d\n", error));
    return error;			
}

/*
** Name : sgi2cm_fid
**
** Input : vp - pointer to vnode of file whose fid is being sought.
**
** Output : fidpp - A fid structure is allocated as a result of the call 
**                  to vop_fid, and the pointer to that structure is 
**                  deposited at *fidpp.
**
** Returns : 0 on success and error code on failure.
**
** Notes : VOP_FID is used by the nfs layer to manufacture file handles 
**         that the server gives to clients. Examples are makefh in nfs.
**
** Observations: Calls osi_Alloc to allocate space for a fid structure,
**		 and then calls the vopx_fid routine to manufacture the fid.
**		 If an error is returned, calls osi_Free to free up allocated
**		 kernel heap (doesn't exist in the HPUX version, probably
**		 because the current cm_fid routine doesn't return errors).
*/

static int
sgi2cm_fid(
    bhv_desc_t	*bdp,
    struct fid **fidpp)
{
    int error;
    struct vnode *vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_fid "));

    VOPX_FID(vp, fidpp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_fid %d\n", error));
    return (error);
}

/*
** Name : sgi2cm_fid2
**
** Input :  vp - pointer to vnode whose attributes are being listed
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Only xfs uses this entry point.
**
** Observations: Does not exist in any reference platform. Just return
**		 ENOSYS like nfs is doing.
*/
static int 
sgi2cm_fid2 (
    bhv_desc_t	*bdp,
    struct fid *fidp)
{
    int error;
    struct vnode *vp;
    struct fid *tmp_fidp;

    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_fid2 "));
    vp = BHV_TO_VNODE(bdp);

    VOPX_FID(vp, &tmp_fidp, error);
    bcopy((caddr_t)tmp_fidp, (caddr_t)fidp,
	    sizeof(struct fid) - MAXFIDSZ + (tmp_fidp)->fid_len);
    freefid(tmp_fidp);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_fid2 %d\n", error));
    return (error);
}

/*
** Name : sgi2cm_rwlock
**
** Input : vp - pointer to vnode of file for which we are calling VOP_RWLOCK
**         write_lock: boolean that indicates whether a read lock or write 
**                     lock is being obtained. This determines whether a 
**                     shared lock or an exclusive lock is obtained.
**
** Output : None.
**
** Returns : None.
**
** Notes : VOP_READ/VOP_WRITE/VOP_READDIR calls are bracketted by calls 
**         to VOP_RWLOCK and VOP_RWUNLOCK. The token management will be 
**         done when those routines get called.
**
** Observations: Not yet sure what has to be done here. The SUNOS version of
**		 the cm provides a rwlock call, but even that entry point 
**		 seems to be pointing to the noop call. Why!!!!!!!!!!!
*/
static void
sgi2cm_rwlock(
    bhv_desc_t	*bdp,
    int write_lock)
{
    struct vnode *vp;
    int error;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_rwlock\n"));
    VOPX_RWLOCK(vp, write_lock, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_rwlock %d\n", error));
}

/*
** Name : sgi2cm_rwunlock
**
** Input : vp - pointer to vnode of file for which we are calling VOP_RWUNLOCK
**         write_lock - boolean indicating whether a write_lock or read_lock 
**                      is being released.
**
** Output : None.
**
** Returns : None.
**
** Notes : VOP_READ/VOP_WRITE/VOP_READDIR calls are bracketted by calls to 
**         VOP_RWLOCK and VOP_RWUNLOCK.
**
** Observations: Change this according to whatever happens to rwlock above.
**		 To Do!!!!!!!!!!!
*/
static void
sgi2cm_rwunlock(
    bhv_desc_t	*bdp,
    int write_lock)
{
    struct vnode *vp;
    int error;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_rwunlock\n"));
    VOPX_RWUNLOCK(vp, write_lock, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_rwunlock %d\n", error));
}

/*
** Name : sgi2cm_seek
**
** Input : vp - pointer to vnode of file on which an lseek is being done.
**         oldoff - current offset in file.
**         newoffp - pointer to location where new offset exists. *newoffp 
**                   specifies the new offset.
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Invoked when an lseek(2) system call is made.
**
** Observations: There is no cm_seek routine. All we do here is make sure
**		 that the specified newoffset is positive and return. Code
**		 modelled after the OSF1 version.
*/
static int
sgi2cm_seek(
    bhv_desc_t	*bdp,
    off_t oldoff,
    off_t *newoffp)
{
	int error;
    	struct vnode *vp;

	AFSLOG(XVFS_DEBUG, 1, ("sgi2cm_seek: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));

	if (*newoffp < 0)
		error = EINVAL;
	else
		error = 0;

	AFSLOG(XVFS_DEBUG, 1, ("sgi2cm_seek: error=%d\n", error));
	return (error);
}

/*
** Name : sgi2cm_cmp
**
** Input : vp1 - pointer to one of the vnodes
**         vp2 - pointer to the other vnode
**
** Output : None.
**
** Returns : returns 1 if the vnodes are the same. 0 if they are not.
**
** Notes : There are no invocations of VOP_CMP!
**
** Observations: There is no cm_cmp routine. We just compare the two vnodes
**		 here and return equality indication.
*/
static int
sgi2cm_cmp(
    bhv_desc_t	*bdp,
    struct vnode *vp2)
{
    int error;
    struct vnode *vp1;

    vp1 = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_cmp vp1 = 0x%x, vp2 = 0x%x\n",vp1, vp2));
    error = (vp1 == vp2);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_cmp %d\n", error));
    return (error);
}

/*
** Name : sgi2cm_frlock
**
** Input : vp - pointer to vnode of file on which lock activity is being 
**              performed.
**         cmd - can be any of the lock commands given to fcntl(2).
**         lfp - pointer to structure of type flock, containing information 
**               about the lock operation, such as the byte range being 
**               locked, type of lock, etc.
**         flag - the flag picked up from the file descriptor.
**         offset - the offset picked up from the file descriptor.
**         cred - pointer to the user's credentials structure.
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : fcntl(2) invokes VOP_FRLOCK on any of the file locking commands. 
**         The F_RSET* commands are invoked by the lockdaemon to acquire 
**         locks on behalf of remote clients. If you examine 
**         cmd/sun/rpc.lockd/priv.c and snlm.c, you will notice various 
**         fcntl calls.
**
** Observations: Code modelled after HPUX nux_lockctl.
**		 There might be work here to do for 64 bit stuff!!!!!!!!!!!
*/
static int
sgi2cm_frlock(
  bhv_desc_t	*bdp,
  int cmd,
  struct flock *lfp,
  int flag,
  off_t offset,
  vrwlock_t vrwlock,
  osi_cred_t *cred)
{
     int error, oldwhence;
     off_t len;
     struct vnode *vp;

     vp = BHV_TO_VNODE(bdp);
     AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_lockctl (cmd=0x%x) = ", cmd));

     /*
      * Handle relative offsets
      */
     oldwhence = lfp->l_whence;
     switch (oldwhence) {
	case 0 :	break;						/* SEEK_SET */
	case 1:  lfp->l_start += offset; break;				/* SEEK_CUR */
	case 2:  VOPX_GETLENGTH (vp, &len, get_current_cred(), error);			/* SEEK_END */
		        lfp->l_start += len; break;
	default:	return EINVAL;
     }
     lfp->l_whence = 0;

     if(cmd == F_SETBSDLK)
          cmd = F_SETLK;

     if(cmd == F_SETBSDLKW)
	  cmd = F_SETLKW;

     VOPX_LOCKCTL(vp, lfp, cmd, get_current_cred(), 0, offset, error);

     switch (oldwhence) {
	case 1:	lfp->l_start -= offset; break;				/* SEEK_CUR */
	case 2:  lfp->l_start -= len;					/* SEEK_END */
     }
     if (cmd != F_GETLK || lfp->l_type == F_UNLCK)
	 lfp->l_whence = oldwhence;

     AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_lockctl (code %d)\n", error));
     return (error);
}

/*
** Name : sgi2cm_realvp
**
** Input : vp1 - pointer to one of the vnodes
**
** Output : vp2 - pointer to pointer to the location where the real vnode 
**                pointer should be set
**
** Returns : returns 1 on success. error code otherwise.
**
** Notes : The file system layer code (xfs,efs and nfs) seems to invoke 
**         this routine. But all of them supply fs_nosys for their own 
**         version of realvp. This must be a useless call.
**
** Observations: This routine does not exist in any of the reference 
**		 platform code. We will just return ENOSYS, as this code
**		 does not exist in the cm code also.
*/
static int
sgi2cm_realvp(
    bhv_desc_t *bp,
    struct vnode **vp2)
{
    int error;

    AFSLOG(XVFS_DEBUG,1,("In sgi2cm_realvp bp=0x%x, vp2=0x%x\n",bp, vp2));
    error = ENOSYS;
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_realvp %d\n", error));
    return (error);
}

/*
** Name : sgi2cm_bmap
**
** Input : vp - pointer to vnode on which bmap operation is being performed.
**         offset - offset into file for blocks to be mapped
**         count - number of bytes to be mapped
**         credp - pointer to user credentials structure
**         bmapp - pointer to struct bmapval where results are returned
**         nbmaps - pointer to an integer location that specifies number of
**                  bmapval structures at bmapp.
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Seems to be called only from os/swapalloc.c. Also nfs server
**         seems to call efs/xfs_bmap. SGI efs/xfs_bmap routines allocate
**         extents and disk space. The nfs layer routines seem to
**         call bmap and follow it with chunkio calls, which invoke
**         VOP_STRATEGY.
**
** Observations: The SGI bmap routine signature is quite different from that
**		 of the cm bmap routine. The code below is modelled after the
**		 nfs_bmap routine in SGI (fs/nfs/nfs_vnodeops.c).
*/
static int
sgi2cm_bmap(
    bhv_desc_t	*bdp,
    off_t offset,
    ssize_t count,
    int flags,
    osi_cred_t *credp,
    struct bmapval *bmapp,
    int *nbmaps)
{
    register struct bmapval *rabmv, *bmv;
    register int bsize = DFS_BLKSIZE;
    register int n = *nbmaps;
    register long off = offset;
    struct vnode *vp;

    vp = BHV_TO_VNODE(bdp);
    ASSERT(n > 0);
#ifdef DEBUG_1
    cmn_err (CE_CONT,
	     "Offset = %d, count=%d in sgi2cm_bmap\n",
	     offset,count);
#endif /* DEBUG_1 */    
    bmv = bmapp;
    bmv->length = (int)BTOBBT(bsize);
    bmv->bsize = bsize;
    bmv->bn = OFFTOBBT(offset / bsize * bsize );
    bmv->offset = bmv->bn;
    bmv->pboff = (int)offset % bsize;
    bmv->pbsize = (int)MIN(bsize - bmv->pboff, count);
    bmv->pbdev = vp->v_rdev;
    /*
     * Set BMAP_DELAY to cause the buffer cache to treat DFS buffers
     * as DELALLOC buffers.  This helps insure that there will be
     * buffers available for other filesystems.
     */
    bmv->eof = BMAP_DELAY;
    
    while (--n > 0) {
	rabmv = bmv + 1;

	rabmv->length = bmv->length;
	rabmv->bn = bmv->bn + bmv->length;
	rabmv->offset = rabmv->bn;
	rabmv->pbsize = rabmv->bsize = bsize;
	rabmv->pboff = 0;
	rabmv->pbdev = vp->v_rdev;
	/*
	 * Set BMAP_DELAY to cause the buffer cache to treat DFS buffers
	 * as DELALLOC buffers.  This helps insure that there will be
	 * buffers available for other filesystems.
	 */
	rabmv->eof = BMAP_DELAY;
	bmv++;
    }
    return (0);
}

/*
** Name : sgi2cm_strategy
**
** Input : vp - pointer to vnode of file for which strategy is being called.
**         bp - poiner to buf structure that gives the parameters of the io.
**
** Output : None.
**
** Returns : returns 1 on success. error code otherwise.
**
** Notes : The file system layer code (xfs,efs and nfs) seems to invoke 
**         the strategy operation as a part of another VOP (read/write)
**         for things like direct io. Since tokens would have been 
**         obtained for the scheduled read/write, there is no need to
**         do any token management as a part of the strategy operation.
**
** Observations: This is a simple pass through routine that invokes the
**		 cm routine.
*/
static void
sgi2cm_strategy(
    bhv_desc_t	*bdp,
    struct buf *bp)
{
    struct vnode *vp;
    /* REFERENCED */
    int error;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("sgi2cm_strategy: (bp=0x%x)\n", bp));
    VOPX_STRATEGY(vp, bp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_strategy: error=%d\n", error));
}

/*
** Name : sgi2cm_map
**
** Input : vp - pointer to vnode of file that is being mmapped or execed.
**         offset - offset into file at which mapping should start
**         len - size of mapping
**         prot - protection associated with mapping
**         flags - specify private or shared mapping.
**         credp - pointer to user credentials structure
**	   nvp - ?
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : exec and mmap call VOP_MAP.
**
** Observations: The filesystem no longer does anything with map.
*/
static int
sgi2cm_map(
    bhv_desc_t	*bdp,
    off_t offset,
    size_t len,
    mprot_t prot,
    uint flags,
    osi_cred_t *credp,
    vnode_t **nvp)
{
    struct vnode *vp;
    int error;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("sgi2cm_map: vp = 0x%x\n",vp));
    VOPX_MAP(vp, flags, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_map, error = 0d%d\n",error));

    return (0);
}

/*
** Name : sgi2cm_addmap
**
** Input : vp - pointer to vnode of file that is being mmapped or execed.
**	   map_type - operation for addmap
**	   vt - pointer to structure containing regions
**	   pgno_type - virtual page number type
**         offset - offset into file at which mapping should start
**         len - size of mapping
**         prot - protection associated with mapping
**         credp - pointer to user credentials structure
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : called from replacereg, dupreg and mapreg in region.c. The file
**         system layer just increments a map count in the inode so that
**         it does not permit record locking while the file is mapped.
**
** Observations: Code does not exist in any reference platform. Code below
**		 is modelled after nfs_addmap. We will just call cm_addmap.
**		 Note that currently, cm_addmap exists only for SUNOS. We
**		 need to create one for SGI. In the SGI version of cm_addmap,
**		 we will do the maintenance of a mapcnt word which we will
**		 associate with the scache structure.
*/
static int
sgi2cm_addmap (
    bhv_desc_t  *bdp,
    vaddmap_t map_type,
    struct __vhandl_s *vt,
    pgno_t *pgno_type,
    off_t offset,
    size_t len,
    mprot_t prot,
    struct cred *credp)
{
    int error=0;
    struct vnode *vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("sgi2cm_addmap, vp = 0x%x\n",vp));
    VOPX_ADDMAP(vp,  map_type, vt, pgno_type, offset, len, prot, 
							    credp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_addmap, error = 0d%d\n",error));
    return (error);
}

/*
** Name : sgi2cm_delmap
**
** Input : bdp - pointer to behavior for file that is being mmapped or execed.
**         vt - 
**         len - size of mapping
**         credp - pointer to user credentials structure
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : called from replacereg, dupreg and mapreg in region.c. The file
**
** Observations: Code doesn't exist for any reference platform. We will
**		 just call cm_delmap, which we will have to write for SGI.
**		 This routine will have to do proper maintenance of the mapcnt
**		 word which we will add to the scache structure.
*/
static int
sgi2cm_delmap (
    bhv_desc_t	*bdp,
    struct __vhandl_s *vt,
    size_t len,
    struct cred *credp)
{
    int error;
    struct vnode *vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("sgi2cm_delmap, vp = 0x%x\n",vp));
    VOPX_DELMAP(vp, vt, len, credp, error);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_delmap, error = 0d%d\n",error));
    return (error);
}

/*
** Name : sgi2cm_poll
**
** Input : vp - pointer to vnode of file for which poll is being done.
**         events - events being sought
**         anyyet - ???
**
** Output : reventsp - actual events that occured are returned here
**          phpp - poll head pointer???
**	    genp - 
**
** Returns : returns 1 on success. error code otherwise.
**
** Notes : Efs and xfs seem to call fs_poll in fs_subr.c, which doesn't
**         do much. There are no calls to vop_poll in os.
**
** Observations: Code doesn't exist in any reference platform. There is
**		 also no cm_poll. Let us just call fs_poll below, just like
**		 nfs and nf3 are doing.
*/
static int
sgi2cm_poll (
  bhv_desc_t	*bdp,
  short events,
  int anyyet,
  short *reventsp,
  struct pollhead **phpp,
  unsigned int *genp)
{
    int error;
    struct vnode *vp;

    AFSLOG(XVFS_DEBUG, 1, ("sgi2cm_poll: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    error = fs_poll(bdp, events, anyyet, reventsp, phpp, genp);
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_poll: error=%d\n", error));
    return (error);
}

/*
** Name : sgi2cm_dump
**
** Input : vp - pointer to vnode of file for which dump is being called
**         addr - ?
**         bn - ?
**         count - ?
**
** Output : None
**
** Returns : returns 1 on success. error code otherwise.
**
** Notes : Both xfs and efs point to fs_nosys for this vnodeop. nfs and nfs3
**	   also point to fs_nosys for this routine, which just returns ENOSYS.
**
** Observations: Just return ENOSYS.
*/
static int
sgi2cm_dump (
  bhv_desc_t *vp,
  caddr_t addr,
  daddr_t bn,
  u_int count)
{
    int error;

    AFSLOG(XVFS_DEBUG, 1, ("sgi2cm_dump: (vp=0x%x)\n", vp));
    error = ENOSYS;
    AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_dump: error=%d\n", error));
    return (error);
}

/*
** Name : sgi2cm_pathconf
**
** Input :  vp - pointer to vnode of file on which POSIX pathconf is being done
**          cmd - subcommand of pathconf; specifies which attribute to get
**          credp - pointer to user credentials structure.
**
** Output : valp - pointer to location where attribute value will be put
**
** Returns : 0 on success and error code on failure.
**
** Notes : No file activity is done on behalf of this call. It just gets
**         various limits related info when POSIX pathconf(2) is done.
**
** Observations: HPUX is the only one that has code for pathconf, and it is
**		 just returning EINVAL for this code. In the SGI, nfs is
**		 calling fs_pathconf, while nfs3 seems to send something over
**		 the wire. For now, let us just mimic the HPUX code and
**		 return EINVAL.
*/
static int
sgi2cm_pathconf (
  bhv_desc_t	*bdp,
  int cmd,
  long *valp,
  struct cred *cr)
{
      int error;

      AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_pathconf "));
      error = EINVAL;
      AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_pathconf %d\n", error));
      return (error);
}

/*
** Name : sgi2cm_allocstore
**
** Input :  vp - pointer to vnode of file on which POSIX pathconf is being done
**          offset - ???
**          count - ???
**          credp - pointer to user credentials structure.
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Supposed to be dead code.
**
** Observations: Let us just return ENOSYS, just as nfs and nfs3 are doing.
*/
static int 
sgi2cm_allocstore(
  bhv_desc_t	*bdp,
  off_t offset,
  size_t count,
  osi_cred_t *credp)
{
      int error;

      AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_allocstore "));
      error = ENOSYS;
      AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_allocstore %d\n", error));
      return (error);
}

#ifdef _K64U64
int irix5_to_flock(enum xlate_mode, void *, int, xlate_info_t *);
int flock_to_irix5(void *, int, xlate_info_t *);
int irix5_n32_to_flock(enum xlate_mode, void *, int, xlate_info_t *);
int flock_to_irix5_n32(void *, int, xlate_info_t *);
#endif
/*
** Name : sgi2cm_fcntl
**
** Input :  vp - pointer to vnode of file for which fcntl is being called.
**          cmd - function code of the fcntl call
**          arg - void * pointer that points to data associated with cmd
**          offset - current file offset as obtained from the file table
**          flags - flags obtained from the file table entry
**          cr - pointer to user credentials structure.
**
** Output : return value is deposited at location pointed to be rvp
**
** Returns : 0 on success and error code on failure.
**
** Notes : fcntl from os/vncalls.c invokes VOP_FCNTL only if it does not 
**         process cmd itself. For efs, this is invoked to get direct io
**         info. In the case of xfs, it seems to get bit maps of the file,
**         etc.
**
** Observations: Code does not exist in any reference platform. Code below
**		 is like nfs and nfs3, and just returns ENOSYS.
*/
static int 
sgi2cm_fcntl(
  bhv_desc_t	*bdp,
  int cmd,
  void *arg,
  int flags,
  off_t offset,
  osi_cred_t *cr,
  rval_t *rvp)
{
      int error = 0;
      struct flock bf;
      struct vnode *vp;
      struct irix5_flock i5_bf;
      char abi = get_current_abi();

      vp = BHV_TO_VNODE(bdp);
      AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_fcntl vp = 0x%x, cmd = %d ",vp, cmd));

      switch(cmd) {
	case F_ALLOCSP:
	case F_FREESP:
	case F_ALLOCSP64:
	case F_FREESP64:
		if ((flags & FWRITE) == 0) {
			error = EBADF;
		} else if (vp->v_type != VREG) {
			error = EINVAL;
#ifdef _K64U64
		} else if (ABI_IS_IRIX5_64(abi)) {
			if (copyin((caddr_t)arg, &bf, sizeof(bf))) {
				error = EFAULT;
				break;
			}
#endif
		} else if (cmd == F_ALLOCSP64 || cmd == F_FREESP64 ||
			ABI_IS_IRIX5_N32(abi)) {
                        /*
                         * The n32 flock structure is the same size as the
                         * o32 flock64 structure. So the copyin_xlate
                         * with irix5_n32_to_flock works here.
                         */
                        if (COPYIN_XLATE((caddr_t)arg, &bf, sizeof bf,
                                irix5_n32_to_flock,
                                abi, 1)) {
                                error = EFAULT;
                                break;
                        }
                } else {
                        if (copyin((caddr_t)arg, &i5_bf, sizeof i5_bf)) {
                                error = EFAULT;
                                break;
                        }
                        /*
                         * Now expand to 64 bit sizes.
                         */
                         bf.l_type = i5_bf.l_type;
                        bf.l_whence = i5_bf.l_whence;
                        bf.l_start = i5_bf.l_start;
                        bf.l_len = i5_bf.l_len;
                }
                if ((error = convoff(vp, &bf, 0, offset, SEEKLIMIT32, 
			curprocp->p_cred)) == 0) {
                        struct vattr vattr;
                        vattr.va_size = bf.l_start;
                        vattr.va_mask = AT_SIZE;
                        error = sgi2cm_setattr(bdp, &vattr, 0, curprocp->p_cred);
                }
                break;
        default:
                error = EINVAL;
                        break;
        }

      AFSLOG(XVFS_DEBUG, 1, ("END sgi2cm_fcntl %d\n", error));
      return (error);
}

/*
** Name : sgi2cm_reclaim
**
** Input :  vp - pointer to vnode which is being reclaimed.
**          flag - seems to be either 0 or FSYNC_INVAL
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : vn_reclaim in os/vnode.c invokes VOP_RECLAIM when reclaiming vnodes.
**
** Observations: Code below is modelled after the OSF1 version. It ignores
**		 the flag parameter and invokes cm_reclaim.
*/
static int 
sgi2cm_reclaim(
    bhv_desc_t	*bdp,
    int flag)
{
    int error;
    struct vnode *vp;
    
    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XVFS_DEBUG, 1, ("sgi2cm_reclaim: (vp=0x%x, flag=%d)\n", vp, flag));
    VOPX_RECLAIM(vp, error);
    AFSLOG(XVFS_DEBUG, 1, ("sgi2cm_reclaim: error=%d\n", error));
    return (error);
}

/*
** Name : sgi2cm_attr_get
**
** Input :  vp - pointer to vnode whose attributes are being obtained.
**	    name - ???
**	    value - ???
**	    flags - ???
**
** Output : valuelenp - ???
**
** Returns : 0 on success and error code on failure.
**
** Notes : Only xfs uses these attr_* vnodeops. Efs and nfs point to fs_nosys
**	   for these calls. nfs3 does not even supply these entry points!
**
** Observations: Does not exist in any reference platform. Just return
**		 ENOSYS like nfs is doing.
*/
static int 
sgi2cm_attr_get(
    bhv_desc_t	*bdp,
    char *name,
    char *value,
    int *valuelenp,
    int flags,
    osi_cred_t *credp)
{
    int code;
    struct vnode *vp;

    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_attr_get: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    code = ENOSYS;
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_attr_get: %d\n", code));
    return(code);
}

/*
** Name : sgi2cm_attr_set
**
** Input :  vp - pointer to vnode whose attributes are being set
**	    name - ???
**	    value - ???
**	    valuelen - ???
**	    flags - ???
**	    credp - pointer to user credentials structure.
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : 
**
** Observations: Does not exist in any reference platform. Just return
**		 ENOSYS like nfs is doing.
*/
static int 
sgi2cm_attr_set(
    bhv_desc_t	*bdp,
    char *name,
    char *value,
    int valuelen,
    int flags,
    osi_cred_t *credp)
{
    int code;
    struct vnode *vp;

    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_attr_set: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    code = ENOSYS;
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_attr_set: %d\n", code));
    return(code);
}

/*
** Name : sgi2cm_attr_remove
**
** Input :  vp - pointer to vnode whose attributes are being removed
**	    name - ???
**	    flags - ???
**	    credp - pointer to user credentials structure.
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Only xfs uses these entry points.
**
** Observations: Does not exist in any reference platform. Just return
**		 ENOSYS like nfs is doing.
*/
static int 
sgi2cm_attr_remove(
    bhv_desc_t	*bdp,
    char *name,
    int flags,
    osi_cred_t *credp)
{
    int code;
    struct vnode *vp;

    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_attr_remove: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    code = ENOSYS;
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_attr_remove: %d\n", code));
    return(code);
}

/*
** Name : sgi2cm_attr_list
**
** Input :  vp - pointer to vnode whose attributes are being listed
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Only xfs uses this entry point.
**
** Observations: Does not exist in any reference platform. Just return
**		 ENOSYS like nfs is doing.
*/
static int 
sgi2cm_attr_list(
    bhv_desc_t	*bdp,
    char *buffer,
    int buflen,
    int flags,
    struct attrlist_cursor_kern *cursor,
    osi_cred_t *credp)
{
    int code;
    struct vnode *vp;

    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_attr_list: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    code = ENOSYS;
    AFSLOG(XVFS_DEBUG, 1, ("In sgi2cm_attr_list: %d\n", code));
    return(code);
}

/* vector of N-ops */

struct osi_vnodeops sgi2cm_ops = {
    BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE), 	/* behavior position */
    sgi2cm_open,
    sgi2cm_close,
    sgi2cm_read,
    sgi2cm_write,
    sgi2cm_ioctl,
    sgi2cm_setfl,
    sgi2cm_getattr,
    sgi2cm_setattr,
    sgi2cm_access,
    sgi2cm_lookup,
    sgi2cm_create,
    sgi2cm_remove,
    sgi2cm_link,
    sgi2cm_rename,
    sgi2cm_mkdir,
    sgi2cm_rmdir,
    sgi2cm_readdir,
    sgi2cm_symlink,
    sgi2cm_readlink,
    sgi2cm_fsync,
    sgi2cm_inactive,
    sgi2cm_fid,
    sgi2cm_fid2,
    sgi2cm_rwlock,
    sgi2cm_rwunlock,
    sgi2cm_seek,
    sgi2cm_cmp,
    sgi2cm_frlock,
    sgi2cm_realvp,
    sgi2cm_bmap,
    sgi2cm_strategy,
    sgi2cm_map,
    sgi2cm_addmap,
    sgi2cm_delmap,
    sgi2cm_poll,
    sgi2cm_dump,
    sgi2cm_pathconf,
    sgi2cm_allocstore,
    sgi2cm_fcntl,
    sgi2cm_reclaim,
    sgi2cm_attr_get,
    sgi2cm_attr_set,
    sgi2cm_attr_remove,
    sgi2cm_attr_list
};
