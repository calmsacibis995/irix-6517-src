/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: xvfs_osglue.c,v 65.9 1998/12/07 18:58:11 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*************************************************************************
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
 * HISTORY
 * $Log: xvfs_osglue.c,v $
 * Revision 65.9  1998/12/07 18:58:11  gwehrman
 * Removed include <os/as/region.h>.
 *
 * Revision 65.8  1998/10/28 15:27:16  lmc
 * Added checks in xglue_read() and xglue_write() for a directory
 * or a link.  An error is returned for both cases which is consistent
 * with xfs.
 *
 * Revision 65.7  1998/04/22  16:02:56  bdr
 * Set the VOP paging functions to be called directly instead of going through
 * the glue layer.
 *
 * Revision 65.6  1998/03/31  23:23:38  bdr
 * Added in new vnodeops for 6.5
 *
 * Revision 65.5  1998/03/23  17:13:14  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.4  1998/02/03 21:45:29  lmc
 * Changes made for new byteRange and recordLock types.  They now have
 * 64bit fields.
 *
 * Revision 65.2  1997/11/06  20:00:49  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/24  14:29:53  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:48  jdoak
 * Initial revision
 *
 * Revision 64.6  1997/08/21  01:43:09  bdr
 * Make xglue_close pay attention to the lastclose flag passed in.
 * Without this we were removing files that still had refcounts in the
 * file table.
 *
 * Revision 64.5  1997/07/26  00:58:55  gw
 * Removed comment line.
 *
 * Revision 64.4  1997/07/10  20:03:23  gw
 * Changed the VOPN and or VOPX calls to work with behaviors.
 *
 * Revision 64.3  1997/04/24  16:00:38  lmc
 * Change xglue_pathconf to fs_pathconf because there is no way to get
 * this information from a DFS server to a DFS client.  With this fix,
 * the client gets the information from the local filesystem.  Since gcc
 * ignores the return value from pathconf(), giving it information (even
 * if it is inaccurate) allows gcc to work in dfs namespace.  On the server
 * side, going directly to fs_pathconf() is okay too.
 *
 * Revision 64.2  1997/02/25  17:02:39  gw
 * hanged parameter to VOPN_LOOKUP to use the correct vnode.
 *
 * Revision 64.1  1997/02/14  19:48:16  dce
 * *** empty log message ***
 *
 * Revision 1.8  1996/12/14  00:26:59  vrk
 * Added Log keyword in the header.
 *
 *
 */

#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/volume.h>
#include <dcedfs/xvfs_vnode.h>
#include <dcedfs/tkc.h>
#include <dcedfs/osi_user.h>
#include <dcedfs/osi_uio.h>
/* XXX-RAG adding */
#include <sys/pathname.h>
#include <sys/fcntl.h>
#include <sys/flock.h>
#include <dcedfs/xvfs_vnode.h>
/*
XXX-RAG removing
#include <ufs/inode.h>
*/
#ifdef SGIMIPS
#include <sys/fs_subr.h>	/* For fs_pathconf prototype */
#include <sys/pfdat.h>		/* For pfd_t structure  */
#endif


/*
 * Generic token glue layer that guarantees data consistency among multiple
 * physical file systems.
 */



static int
xglue_open(
    bhv_desc_t *bdp,
    vnode_t **vpp,
    mode_t flags,
    osi_cred_t *credp)
{
    struct tkc_vcache *vcp;
    register int code;
    struct volume *volp;
    int hoFlags;
    struct vnode *vp;
    int preempting;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_open (flags=0x%x) = ", flags));
    /*
    **  The call to xvfs_GetVolume returns a pointer to the volume structure
    **  associated with the vnode vp.
    **
    **
    **  Server side : xvfs_GetVolume ends up calling VOPX_GETVOLUME,
    **          which results in a call to xvfs2sgi_getvolume routine
    **          (defined in xvfs_vfs2os.c). This routine will pick up
    **          the vfsp from the vnode, and call vol_FindVfs, defined
    **          in file/xvolume/vol_attvfs.c). The vol_FindVfs routine
    **          searches through a chain that associates vfs pointers
    **          to volume structure pointers. If the search results in
    **          a match, the matching volume structure pointer is
    **          returned.  Note that the vfsp to volume structure
    **          pointer association would have been created when the
    **          volume was exported by dfsexport. This can be seen by
    **          the call in vol_ufsAttach to vol_AddVfs.
    **
    **  Client side : Note that the xglue_open routine is traversed on
    **          the client side also.  When xvfs_GetVolume invokes
    **          VOPX_GETVOLUME, it will be invoking the cm routine
    **          cm_getvolume. This routine just returns a NULL
    **          pointer. This causes the routine xvfs_NeedTokens to
    **          say that we don't need tokens. Thus we will bypass the
    **          token obtaining routines here and deal with tokens in
    **          the cm layer.
    */
    vp = BHV_TO_VNODE(bdp);
    code = (int)xvfs_GetVolume(vp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_open: %d\n", code));
      return(code);
    }
    /*
    **  If the volume pointed at by volp is not exported or it is a
    **  read only volume, we don't have to bother with tokens.
    */
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_OPEN, 0, NULL))) {
	    preempting = osi_PreemptionOff();
            VOPN_OPEN(vp, vpp, flags, credp, code);
	    osi_RestorePreemption(preempting);
            vol_EndVnodeOp(volp, NULL);
        }
        goto openerr;
    }
    if (flags & FWRITE) hoFlags = 1;
    else hoFlags = 0;
    preempting = osi_PreemptionOff();
    if (!(code = tkc_HandleOpen(vp, hoFlags, &vcp))) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_OPEN, 0, NULL))) {
            VOPN_OPEN(vp, vpp, flags, credp, code);
            vol_EndVnodeOp(volp, NULL);
        }
        tkc_Release(vcp);
        if (code) tkc_HandleClose(vp, hoFlags);
    }
    osi_RestorePreemption(preempting);

openerr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_open %d\n", code));
    return (code);
}

static int
xglue_close(
  bhv_desc_t    *bdp,
  int flags,
  lastclose_t lastclose,        /* lastclose is either L_FALSE */
                                /*     L_TRUE or FROM_VN_KILL */
  osi_cred_t *credp)
{
    register struct tkc_vcache *vcp;
    register int code;
    struct volume *volp;
    int hoFlags;
    vnode_t	*vp;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_close (flags=0x%x) = ", flags));
    /*
     * If this isn't the last holder of this file, we had better
     * keep our tokens and data!  Nothing to do then.
     */
    if (lastclose == L_FALSE)
        return (0);

    vp = BHV_TO_VNODE(bdp);
    code = xvfs_GetVolume(vp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_close: %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_CLOSE, 0, NULL))) {
            VOPN_CLOSE(bdp, flags, lastclose, credp, code);
            vol_EndVnodeOp(volp, NULL);
        }
        goto closerr;
    }
    /*
     * XXX Do we really need to get the OPEN tokens again; they've gotten
     * during the open system call.  Answer (kazar): No.
     */
    if (!(code = vol_StartVnodeOp(volp, VNOP_CLOSE, 0, NULL))) {
        VOPN_CLOSE(bdp, flags, lastclose, credp, code);
        vol_EndVnodeOp(volp, NULL);
    }

    if (flags & FWRITE) hoFlags = 1;
    else hoFlags = 0;
    if (vcp = tkc_HandleClose(vp, hoFlags)) {
        /* HandleClose actually does all of our hard work */
        tkc_Release(vcp);
    }

closerr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_close %d\n", code));
    return (code);
}

static int
xglue_read(
  bhv_desc_t	*bdp,
  struct uio *uiop,
  int flags,
  osi_cred_t *credp,
  flid_t	*fl)
{
    register struct tkc_vcache *vcp;
    int code, dataToken;
    struct volume *volp;
    int preempting;
    vnode_t	*vp;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_read (flags=0x%x) = ", flags));
    vp = BHV_TO_VNODE(bdp);
#ifdef SGIMIPS
    if (vp->v_type == VDIR) {
	return(EISDIR);
    }
    if (vp->v_type == VLNK) {
	return(EINVAL);
    }
#endif /* SGIMIPS */
    code = xvfs_GetVolume(vp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_read: code = %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_RDWR, 0, NULL))) {
            preempting = osi_PreemptionOff();
            VOPN_READ(bdp, uiop, flags, credp, fl, code);
            osi_RestorePreemption(preempting);
            vol_EndVnodeOp(volp, NULL);
        }
        goto readerr;
    }
    dataToken = TKN_DATA_READ;
    VOPN_RWUNLOCK(bdp, VRWLOCK_READ);
    preempting = osi_PreemptionOff();
    if (vcp = tkc_Get(vp, TKN_STATUS_WRITE|dataToken, 0)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_RDWR, 0, NULL))) {
	    VOPN_RWLOCK(bdp, VRWLOCK_READ);
            VOPN_READ(bdp, uiop, flags, credp, fl, code);
            VOPN_RWUNLOCK(bdp, VRWLOCK_READ);
            vol_EndVnodeOp(volp, NULL);
        }
        tkc_Put(vcp);
    } else
        code = EINVAL;
    osi_RestorePreemption(preempting);
    VOPN_RWLOCK(bdp, VRWLOCK_READ);
readerr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_read %d\n", code));
    return (code);
}

static int
xglue_write(
  bhv_desc_t	*bdp,
  struct uio *uiop,
  int flags,
  osi_cred_t *credp,
  flid_t	*fl)
{
    register struct tkc_vcache *vcp;
    int code, dataToken;
    struct volume *volp;
    int preempting;
    vnode_t *vp;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_write (flags=0x%x) = ", flags));
    vp = BHV_TO_VNODE(bdp);
#ifdef SGIMIPS
    if (vp->v_type == VDIR) {
        return(EISDIR);
    }
    if (vp->v_type == VLNK) {
        return(EINVAL);
    }
#endif /* SGIMIPS */
    code = xvfs_GetVolume(vp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_write: code = %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_RDWR, 0, NULL))) {
            preempting = osi_PreemptionOff();
            VOPN_WRITE(bdp, uiop, flags, credp, fl, code);
            osi_RestorePreemption(preempting);
            vol_EndVnodeOp(volp, NULL);
        }
        goto writeerr;
    }
    dataToken = TKN_DATA_WRITE;
    VOPN_RWUNLOCK(bdp, VRWLOCK_WRITE);
    preempting = osi_PreemptionOff();
    if (vcp = tkc_Get(vp, TKN_STATUS_WRITE|dataToken, 0)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_RDWR, 0, NULL))) {
            VOPN_RWLOCK(bdp, VRWLOCK_WRITE);
            VOPN_WRITE(bdp, uiop, flags, credp, fl, code);
            VOPN_RWUNLOCK(bdp, VRWLOCK_WRITE);
            vol_EndVnodeOp(volp, NULL);
        }
        tkc_Put(vcp);
    } else
        code = EINVAL;
    osi_RestorePreemption(preempting);
    VOPN_RWLOCK(bdp, VRWLOCK_WRITE);
writeerr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_write %d\n", code));
    return (code);
}

static int
xglue_ioctl(
  bhv_desc_t	*bdp,
  int cmd,
  void *arg,
  int flags,
  osi_cred_t *credp,
  int *rv,
  struct vopbd *vopbdp)
{
    register int code;

    /*
     * XXX ADD SOME TOKEN SUPPORT FOR THIS!! XXXX
     */
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_ioctl (cmd=0x%x, flags=%x, arg=%x) = ",
           cmd, flags, arg));
    VOPN_IOCTL(bdp, cmd, arg, flags, credp, rv, vopbdp, code);  
							/* SGI XFS returns */
                                                        /* ENOSYS for this */
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_ioctl %d\n", code));
    return (code);
}

/*
**      vop_setfl is a vnodeop that exists only in SGI. All the
**      filesystems seem to be returning zero for this call. Only nfs
**      seems to all fs_setfl, which makes sure that nflags does not
**      contain FDIRECT (if it does, EINVAL is returned). Otherwise,
**      fs_setfl also returns 0.  Thus, there is no modification that
**      is done under vop_setfl, and hence no need for obtaining any
**      tokens.
*/
static int
xglue_setfl(
   bhv_desc_t *bdp,
   int oflags,
   int nflags,
   osi_cred_t *cr)
{
   int retcode;
   AFSLOG(XGLUE_DEBUG, 1, ("In xglue_setfl (oflags=0x%x, nflags=0x%x) = ",
                                                         oflags, nflags));
   VOPN_SETFL(bdp, oflags, nflags, cr, retcode);   /* SGI XFS just returns 0.*/
                                                /* No need for tokens */
   AFSLOG(XGLUE_DEBUG, 1, ("END xglue_setfl %d\n", code));
   return (retcode);
}

static int
xglue_getattr(
    bhv_desc_t	*bdp,
    struct vattr *vattrp,
    int flags,
    osi_cred_t *credp)
{
    register struct tkc_vcache *vcp;
    register int code;
    struct volume *volp;
    int preempting; 
    vnode_t	*vp;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_getattr (vattrp=0x%x) = ", vattrp));
    vp = BHV_TO_VNODE(bdp);
    code = xvfs_GetVolume(vp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_getattr: code = %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_GETATTR, 0, NULL))) {
            preempting = osi_PreemptionOff();
            VOPN_GETATTR(bdp, vattrp, flags, credp, code);
            osi_RestorePreemption(preempting);
            vol_EndVnodeOp(volp, NULL);
        }
        goto getattrerr;
    }
    preempting = osi_PreemptionOff();
    if (vcp = tkc_Get(vp, TKN_STATUS_READ, 0)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_GETATTR, 0, NULL))) {
            VOPN_GETATTR(bdp, vattrp, flags, credp, code);
            vol_EndVnodeOp(volp, NULL);
        }
        tkc_Put(vcp);
    } else
        code = EINVAL;
    osi_RestorePreemption(preempting);

getattrerr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_getattr %d\n", code));
    return (code);
}


static int
xglue_setattr(
  bhv_desc_t	*bdp,
  struct vattr *vattrp,
  int	flags,
  osi_cred_t *credp)
{
    register struct tkc_vcache *vcp;
    register int code;
    struct volume *volp;
    int preempting; 
    vnode_t	*vp;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_setattr "));
    vp = BHV_TO_VNODE(bdp);
    code = xvfs_GetVolume(vp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_setattr: code = %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_SETATTR, 0, NULL))) {
            preempting = osi_PreemptionOff();
            VOPN_SETATTR(bdp, vattrp, flags, credp, code);
            osi_RestorePreemption(preempting);
            vol_EndVnodeOp(volp, NULL);
        }
        goto setattrerr;
    }
    preempting = osi_PreemptionOff();
    if (vcp = tkc_Get(vp, TKN_STATUS_WRITE, 0)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_SETATTR, 0, NULL))) {
            VOPN_SETATTR(bdp, vattrp, flags, credp, code);
            vol_EndVnodeOp(volp, NULL);
        }
        tkc_Put(vcp);
    } else
        code = EINVAL;
    osi_RestorePreemption(preempting);

setattrerr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_setattr %d\n", code));
    return (code);
}

int
xglue_access(
  bhv_desc_t	*bdp,
  int mode,
  osi_cred_t *credp)
  {
    register struct tkc_vcache *vcp;
    register int code;
    struct volume *volp;
    int preempting;
    vnode_t	*vp;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_access (mode=0x%x) = ", mode));
    vp = BHV_TO_VNODE(bdp);
    code = xvfs_GetVolume(vp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_access code = %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_ACCESS, 0, NULL))) {
            preempting = osi_PreemptionOff();
            VOPN_ACCESS(bdp, mode, credp, code);
	    osi_RestorePreemption(preempting);
            vol_EndVnodeOp(volp, NULL);
        }
        goto accesserr;
    }
    preempting = osi_PreemptionOff();
    if (vcp = tkc_Get(vp, TKN_STATUS_READ, 0)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_ACCESS, 0, NULL))) {
            VOPN_ACCESS(bdp, mode, credp, code);
            vol_EndVnodeOp(volp, NULL);
        }
        tkc_Put(vcp);
    } else
        code = EINVAL;
    osi_RestorePreemption(preempting);

accesserr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_access %d\n", code));
    return (code);
}

static int
xglue_lookup(
  bhv_desc_t	*dir_bdp,
  char *namep,
  struct vnode **vpp,
  pathname_t *pnp,
  int flags,
  struct vnode *mvp,
  osi_cred_t *credp)
{
    register struct tkc_vcache *vcp;
    register int code;
    struct volume *volp;
    int preempting;
    vnode_t	*dp;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_lookup (namep=%s) = ", namep));
    dp = BHV_TO_VNODE(dir_bdp);
    code = xvfs_GetVolume(dp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_lookup: %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_LOOKUP, 0, NULL))) {
	    preempting = osi_PreemptionOff();
            VOPN_LOOKUP(dir_bdp, namep, vpp, pnp, flags, mvp, credp, code);
	    osi_RestorePreemption(preempting);
            if (volp && !code) /* registered volume but not exported? */
                XVFS_CONVERT(*vpp);
            vol_EndVnodeOp(volp, NULL);
        }
        goto lookerr;
    }
    
    preempting = osi_PreemptionOff();
    if (vcp = tkc_Get(dp, TKN_DATA_READ, 0)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_LOOKUP, 0, NULL))) {
            VOPN_LOOKUP(dir_bdp, namep, vpp, pnp, flags, mvp, credp, code);
            if (!code)                  /***SDM*** HPUX has some check */
                                        /* about NFS type here ***SDM***/
                XVFS_CONVERT(*vpp);
            vol_EndVnodeOp(volp, NULL);
        }
        tkc_Put(vcp);
    } else
        code = EINVAL;
    osi_RestorePreemption(preempting);

lookerr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_lookup %d\n", code));
    return (code);
}

static int
xglue_create(
    bhv_desc_t		*dir_bdp,
    char *namep,
    struct vattr *vattrp,
    int excl,
    int mode,
    struct vnode **vpp,
    osi_cred_t *credp)
{
    register struct tkc_vcache *vcp;
    register int code;
    struct volume *volp;
    int preempting;
    vnode_t	*dp;

    AFSLOG(XGLUE_DEBUG,1,("In xglue_create (namep=%s, mode=0x%x)=",
           namep, mode));
    dp = BHV_TO_VNODE(dir_bdp);
    code = xvfs_GetVolume(dp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_create: %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_CREATE, 0, NULL))) {
            preempting = osi_PreemptionOff();
            VOPN_CREATE(dir_bdp, namep, vattrp, excl, mode, vpp, credp, code);
	    osi_RestorePreemption(preempting);
            if (volp && !code) /* registered volume but not exported? */
                XVFS_CONVERT(*vpp);
            vol_EndVnodeOp(volp, NULL);
        }
        goto createrr;
    }
    preempting = osi_PreemptionOff();
    if (vcp = tkc_Get(dp, TKN_DATA_WRITE|TKN_STATUS_WRITE, 0)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_CREATE, 0, NULL))) {
            VOPN_CREATE(dir_bdp, namep, vattrp, excl, mode, vpp, credp, code);
            if (!code)                  /***SDM*** HPUX has some check */
                                        /* about NFS type here ***SDM***/
                XVFS_CONVERT(*vpp);
            vol_EndVnodeOp(volp, NULL);
        }
        tkc_Put(vcp);
    } else
        code = EINVAL;
    osi_RestorePreemption(preempting);

createrr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_create %d\n", code));
    return (code);
}

static int
xglue_remove(
  bhv_desc_t	*dir_bdp,
  char *namep,
  osi_cred_t *credp)
{
    struct vnode *vp = 0;
    struct tkc_sets sets[2];
    register int code;
    struct volume *volp1 = 0, *volp2 = 0;
    int preempting; 
    vnode_t 	*dp;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_remove "));

    dp = BHV_TO_VNODE(dir_bdp);
    preempting = osi_PreemptionOff();
    VOPN_LOOKUP(dir_bdp, namep, &vp, (struct pathname *)NULL, 0, dp, credp, code)
    if (code) {
	osi_RestorePreemption(preempting);
        return (code);
    }
    osi_RestorePreemption(preempting);

    code = xvfs_GetVolume(dp, &volp1);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_remove: %d\n", code));
      volp1 = (struct volume *)0;
      goto remerr;
    }
    code = xvfs_GetVolume(vp, &volp2);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_remove: %d\n", code));
      volp2 = (struct volume *)0;
      goto remerr;
    }
    if (volp1 != volp2) {
        code = EXDEV;
        goto remerr;
    }

    if (!xvfs_NeedTokens(volp1)) {
        if (!(code = vol_StartVnodeOp(volp1, VNOP_REMOVE, 0, NULL))) {
            preempting = osi_PreemptionOff();
            VOPN_REMOVE(dir_bdp, namep, credp, code);
	    osi_RestorePreemption(preempting);
            vol_EndVnodeOp(volp1, NULL);
        }
        goto remerr;
    }
    if (dp == vp) {
        code = EINVAL;
        goto remerr;
    }

    preempting = osi_PreemptionOff();
    tkc_Set(0, dp, TKN_DATA_WRITE|TKN_STATUS_WRITE, 0);
    tkc_Set(1, vp, TKN_STATUS_WRITE, 0);
    tkc_GetTokens(sets, 2);
    VN_HOLD(vp);
    if (!(code = vol_StartVnodeOp(volp1, VNOP_REMOVE, 0, NULL))) {
        VOPN_REMOVE(dir_bdp, namep, credp, code);
        vol_EndVnodeOp(volp1, NULL);
    }
    tkc_PutTokens(sets, 2);
    tkc_HandleDelete(vp, volp1);
    osi_RestorePreemption(preempting);
    VN_RELE(vp);

remerr:
    if (vp)    VN_RELE(vp);
    if (volp1) xvfs_PutVolume(volp1);
    if (volp2) xvfs_PutVolume(volp2);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_remove %d\n", code));
    return (code);
}

static int
xglue_link(
  bhv_desc_t	*bdp,
  struct vnode  *dp,
  char *namep,
  osi_cred_t *credp)
{
    register int code;
    struct volume *volp1, *volp2;
    struct tkc_sets sets[2];
    vnode_t	*vp;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_link (namep=%s) = ", namep));
    vp = BHV_TO_VNODE(bdp);
    code = xvfs_GetVolume(vp, &volp1);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_link: %d\n", code));
      return(code);
    }
    code = xvfs_GetVolume(dp, &volp2);
    if (code) {
      xvfs_PutVolume(volp1);
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_link: %d\n", code));
      return(code);
    }
    if (volp1 != volp2) {
        code = EXDEV;
        goto linkerr;
    }
    if (!xvfs_NeedTokens(volp1)) {
        if (!(code = vol_StartVnodeOp(volp1, VNOP_LINK, 0, NULL))) {
            VOPN_LINK(bdp, dp, namep, credp, code);
            vol_EndVnodeOp(volp1, NULL);
        }
        goto linkerr;
    }
    tkc_Set(0, vp, TKN_DATA_WRITE|TKN_STATUS_WRITE, 0);
    tkc_Set(1, dp, TKN_DATA_WRITE|TKN_STATUS_WRITE, 0);
    tkc_GetTokens(sets, 2);
    if (!(code = vol_StartVnodeOp(volp1, VNOP_LINK, 0, NULL))) {
        VOPN_LINK(bdp, dp, namep, credp, code);
        vol_EndVnodeOp(volp1, NULL);
    }
    tkc_PutTokens(sets, 2);

linkerr:
    xvfs_PutVolume(volp1); xvfs_PutVolume(volp2);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_link%d\n", code));
    return (code);
}

static int
xglue_rename(
  bhv_desc_t	*bdp,	/* 'from' directory */
  char *namep,
  struct vnode *tdp, /* 'to'   directory  */
  char *tnamep,
  struct pathname *tpnp,
  osi_cred_t *credp)
{
    struct tkc_sets sets[4];
    struct vnode *vp = 0, /* 'from' vnode */
	    	 *tp = 0; /* 'to'   vnode */
    register int code, size;
    struct volume *volp1, *volp2;
    vnode_t	*dp;
    bhv_desc_t	*tbdp;

    tbdp = VNODE_TO_FIRST_BHV(tdp);

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_rename (namep=%s, tnamep=%s) = ",
                            namep, tnamep));

    dp = BHV_TO_VNODE(bdp);
    code = xvfs_GetVolume(dp, &volp1);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_rename fail1: %d\n", code));
      return(code);
    }
    code = xvfs_GetVolume(tdp, &volp2);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_rename fail2: %d\n", code));
      xvfs_PutVolume(volp1);
      return(code);
    }
    /* need to check both since otherwise could dephantomize 2nd dir
     * if it is offline at the time.
     */
    if (volp1 != volp2) {
        code = EXDEV;
        goto renerr;
    }
    if (!xvfs_NeedTokens(volp1)) {
        if (!(code = vol_StartVnodeOp(volp1, VNOP_RENAME, 0, NULL))) {
            VOPN_RENAME(bdp, namep, tdp, tnamep, tpnp, credp, code);
            vol_EndVnodeOp(volp1, NULL);
        }
        goto renerr;
    }

    /* note that tkc_Set is a macro! */
    tkc_Set(0, dp, TKN_DATA_WRITE|TKN_STATUS_WRITE, 0);
    size = 1;
    if (tdp != dp) {
        tkc_Set(size, tdp, TKN_DATA_WRITE|TKN_STATUS_WRITE, 0);
        size++;
    }

    /* look up 'from' vnode to do token sync. */
    VOPN_LOOKUP(bdp, namep, &vp, (struct pathname *)NULL, 0, dp, credp, code)
    if (code) {
                           /***SDM*** flags is 0 ***SDM***/
                           /***SDM***Ok to set mvp to be dp?***SDM***/
                           /***SDM***callee ignores this param ***SDM***/
        vp = (struct vnode *)0;
        goto renerr;
    }
    tkc_Set(size, vp, TKN_DATA_WRITE|TKN_STATUS_WRITE, 0);
    size++;

    /* look up 'to' vnode to do token sync. */
    VOPN_LOOKUP(tbdp, tnamep, &tp, tpnp, 0, tdp, credp, code);
    if (code) {
        tp = (struct vnode *)0;
    } else {
        tkc_Set(size, tp, TKN_STATUS_WRITE, 0);
        size++;
    }

    /*
     * watch for invalid parms: duplicate vnode going into tkc_GetTokens
     * will cause an infinite loop.  dp == tdp is only legal dup vnode.
     * tp is only potentially null pointer.
     */
    if (vp == dp || vp == tp || vp == tdp ||
        dp == tp || tp == tdp) {
        code = EINVAL;
        goto renerr;
    }

    tkc_GetTokens(sets, size);
    if (tp) VN_HOLD(tp);
    if (!(code = vol_StartVnodeOp(volp1, VNOP_RENAME, 0, NULL))) {
        VOPN_RENAME(bdp, namep, tdp, tnamep, tpnp, credp, code);
        vol_EndVnodeOp(volp1, NULL);
    }
    tkc_PutTokens(sets, size);
    /* if the rename over-wrote an existing object, delete that object */
    if (tp) {
        tkc_HandleDelete(tp, volp1);
        VN_RELE(tp);
    }

renerr:
    if (tp) VN_RELE(tp);
    if (vp) VN_RELE(vp);
    xvfs_PutVolume(volp1);
    xvfs_PutVolume(volp2);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_rename %d\n", code));
    return (code);
}

/*
** Name : xglue_mkdir
**
** Input : dp - pointer to vnode of directory in which new directory is 
**              being created.
**         namep - pointer to character string of name of directory to be 
**                 created.
**         vattrp - pointer to attribute structure of directory entry to be 
**                  created.
**         credp - pointer to user credentials structure.
**
** Output : vpp - pointer to vnode of newly created directory will be 
**                placed at *vpp.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Invoked as a result of mkdir system call. vn_create calls 
**         VOP_MKDIR, which ends up here if the vnode of the parent 
**         directory has been converted to DFS vnodeops format.
**
** Observations: SGI mkdir vnodeop call matches the HPUX vnodeop call in 
**               signature. Hence the only change to this proc from the HPUX 
**               proc has been to change credp from being a struct ucred *
**               to struct cred * and to mark the function as static int 
**               instead of int.
*/
static int
xglue_mkdir(
  bhv_desc_t	*bdp,
  char *namep,
  struct vattr *vattrp,
  struct vnode **vpp,
  osi_cred_t *credp)
{
    register struct tkc_vcache *vcp;
    register int code;
    struct volume *volp;
    int preempting;
    vnode_t	*dp;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_mkdir (namep=%s) = ", namep));
    dp = BHV_TO_VNODE(bdp);
    code = xvfs_GetVolume(dp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_mkdir: %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_MKDIR, 0, NULL))) {
            preempting = osi_PreemptionOff();
            VOPN_MKDIR(bdp, namep, vattrp, vpp, credp, code);
            osi_RestorePreemption(preempting);
            vol_EndVnodeOp(volp, NULL);
        }
        goto mkdirerr;
    }
    preempting = osi_PreemptionOff();
    if (vcp = tkc_Get(dp, TKN_DATA_WRITE|TKN_STATUS_WRITE, 0)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_MKDIR, 0, NULL))) {
            VOPN_MKDIR(bdp, namep, vattrp, vpp, credp, code);
            vol_EndVnodeOp(volp, NULL);
        }
        if (!code)
            XVFS_CONVERT (*vpp);
        tkc_Put(vcp);
    } else
        code = EINVAL;
    osi_RestorePreemption(preempting);
mkdirerr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_mkdir %d\n", code));
    return (code);
}

/*
** Name : xglue_rmdir
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
** Observations: SGI vnodeop rmdir has one additional parameter (as compared 
**               to HPUX) - the pointer to the vnode of the current directory.
**               The file system layer (efs/xfs) seems to use this pointer 
**               to make sure that the directory being removed is not
**               the current directory.
**
**               ***SDM*** The code below doesn't make a lot of sense for SGI.
**               It seems to first do a xvfs_GetVolume on the vnode of the 
**               parent directory. If the vnode of the parent directory is 
**               marked as not converted, the code does not below do any
**               token management. This can cause problems because the 
**               protocol exporter could have doled out tokens for the 
**               directory being removed. But the parent directory
**               vnode itself could have been reclaimed after it was used 
**               for lookup.
*/
static int
xglue_rmdir(
  bhv_desc_t	*bdp,
  char *namep,
  struct vnode *current_dir_vp, /* pointer to vnode of current directory */
                                /* for SGI */
  osi_cred_t *credp)
{
    struct vnode *vp = 0;
    struct tkc_sets sets[2];
    register int code;
    struct volume *volp;
    vnode_t	*dp;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_rmdir (namep=%s) = ", namep));
    dp = BHV_TO_VNODE(bdp);
    code = xvfs_GetVolume(dp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_rmdir: %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_RMDIR, 0, NULL))) {
            VOPN_RMDIR(bdp, namep, current_dir_vp, credp, code); 
                                         /* SGI added current_dir_vp */
            vol_EndVnodeOp(volp, NULL);
        }
        goto rmdirerr;
    }

    if (code = vol_StartVnodeOp(volp, VNOP_RMDIR, 0, NULL)) {
        vp = (struct vnode *)0;
        goto rmdirerr;
    }

    VOPN_LOOKUP(bdp, namep, &vp, (struct pathname *)NULL, 0, dp, credp, code);
    /*
     * We can't call tkc_Set() below until we do this...
     */
    vol_EndVnodeOp(volp, NULL);
    if (code) {
        vp = (struct vnode *)0;
        goto rmdirerr;
    }
    XVFS_CONVERT(dp); /***SDM*** do they mean to convert vp? ***SDM***/
    if (dp == vp) {
        code = EINVAL;
        goto rmdirerr;
    }

    tkc_Set(0, dp, TKN_DATA_WRITE|TKN_STATUS_WRITE, 0);
    tkc_Set(1, vp, TKN_STATUS_WRITE, 0);
    tkc_GetTokens(sets, 2);
    VN_HOLD(vp);
    if (!(code = vol_StartVnodeOp(volp, VNOP_RMDIR, 0, NULL))) {
        VOPN_RMDIR(bdp, namep, current_dir_vp, credp, code);
        vol_EndVnodeOp(volp, NULL);
    }
    tkc_PutTokens(sets, 2);
    tkc_HandleDelete(vp, volp);
    VN_RELE(vp);

rmdirerr:
    if (vp) VN_RELE(vp); /* release the reference acquired by VOPN_LOOKUP */
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_rmdir %d\n", code));
    return (code);

}

/*
** Name : xglue_readdir
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
** Observations: SGI readdir has the additional parameter int *eofp 
**               (as compared to HPUX readdir). The file system layer code
**               (xfs/efs) seems to return a boolean indication of eof here.
*/
static int
xglue_readdir(
  bhv_desc_t	*bdp,
  struct uio *uiop,
  osi_cred_t *credp,
  int *eofp)
{
    register struct tkc_vcache *vcp;
    register int code;
    struct volume *volp;
    int preempting;
    vnode_t	*vp;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_readdir = "));
    vp = BHV_TO_VNODE(bdp);
    code = xvfs_GetVolume(vp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_readdir: code = %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_READDIR, 0, NULL))) {
            preempting = osi_PreemptionOff();
            VOPN_READDIR(bdp, uiop, credp, eofp, code);  /* Added eofp */
                                                         /* param for SGI */
            osi_RestorePreemption(preempting);
            vol_EndVnodeOp(volp, NULL);
        }
        goto readdirerr;
    }
    preempting = osi_PreemptionOff();
    VOPN_RWUNLOCK(bdp, VRWLOCK_READ);
    if (vcp = tkc_Get(vp, TKN_DATA_READ, 0)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_READDIR, 0, NULL))) {
            VOPN_RWLOCK(bdp, VRWLOCK_READ);
            VOPN_READDIR(bdp, uiop, credp, eofp, code);  /* Added eofp */
                                                         /* param for SGI */
            VOPN_RWUNLOCK(bdp, VRWLOCK_READ);
            vol_EndVnodeOp(volp, NULL);
        }
        tkc_Put(vcp);
    } else
        code = EINVAL;
    osi_RestorePreemption(preempting);
    VOPN_RWLOCK(bdp, VRWLOCK_READ);

readdirerr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_readdir %d\n", code));
    return (code);
}

/*
** Name : xglue_symlink
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
** Observations: No code changes were needed for xglue_symlink from the 
**               HPUX version except to change ucred * to cred *.
*/
static int
xglue_symlink(
  bhv_desc_t	*bdp,
  char *namep,
  struct vattr *vattrp,
  char *targetp,
  osi_cred_t *credp)
{
    register struct tkc_vcache *vcp;
    register int code;
    struct volume *volp;
    int preempting;
    vnode_t	*vp;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_symlink (namep=%s, target=%s) = ",
           namep, targetp));
    vp = BHV_TO_VNODE(bdp);
    code = xvfs_GetVolume(vp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_symlink: code = %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_SYMLINK, 0, NULL))) {
            preempting = osi_PreemptionOff();
            VOPN_SYMLINK(bdp, namep, vattrp, targetp, credp, code);
            osi_RestorePreemption(preempting);
            vol_EndVnodeOp(volp, NULL);
        }
        goto symlinkerr;
    }
    preempting = osi_PreemptionOff();
    if (vcp = tkc_Get(vp, TKN_DATA_WRITE|TKN_STATUS_WRITE, 0)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_SYMLINK, 0, NULL))) {
            VOPN_SYMLINK(bdp, namep, vattrp, targetp, credp, code);
            vol_EndVnodeOp(volp, NULL);
        }
        tkc_Put(vcp);
    } else
        code = EINVAL;
    osi_RestorePreemption(preempting);

symlinkerr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_symlink %d\n", code));
    return (code);
}

/*
** Name : xglue_readlink
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
**         vnodes that have been converted to DFS vnodeop format.
**
** Observations: No changes were readed for xglue_readlink from the HPUX 
**               version.
*/
static int
xglue_readlink(
  bhv_desc_t	*bdp,
  struct uio *uiop,
  osi_cred_t *credp)
{
    register struct tkc_vcache *vcp;
    register int code;
    struct volume *volp;
    int preempting;
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_readlink = "));
    code = xvfs_GetVolume(vp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_readlink: code = %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_READLINK, 0, NULL))) {
            preempting = osi_PreemptionOff();
            VOPN_READLINK(bdp, uiop, credp, code);
            osi_RestorePreemption(preempting);
            vol_EndVnodeOp(volp, NULL);
        }
        goto readlinkerr;
    }
  
    preempting = osi_PreemptionOff();
    if (vcp = tkc_Get(vp, TKN_DATA_READ, 0)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_READLINK, 0, NULL))) {
            VOPN_READLINK(bdp, uiop, credp, code);
            vol_EndVnodeOp(volp, NULL);
        }
        tkc_Put(vcp);
    } else
        code = EINVAL;
    osi_RestorePreemption(preempting);

readlinkerr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_readlink%d\n", code));
    return (code);
}

/*
** Name : xglue_fsync
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
**
** Observations: SGI xglue_fsync signature differs from that of HPUX. 
**               The parameter flag exists in SGI while the parameter 
**               nonce does not.
*/
int xglue_fsync(
  bhv_desc_t	*bdp,
  int flag,             /* SGI flag that specifies FSYNC_DATA, FSYNC_WAIT */
                                                /* FSYNC_INVAL */
  osi_cred_t *credp,
  off_t	start,
  off_t	stop)
{
    register struct tkc_vcache *vcp;
    register int code;
    struct volume *volp;
    int preempting;
    vnode_t	*vp;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_fsync\n"));
    vp = BHV_TO_VNODE(bdp);
    code = xvfs_GetVolume(vp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_fsync: code = %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_FSYNC, 0, NULL))) {
            preempting = osi_PreemptionOff();
            VOPN_FSYNC(bdp, flag, credp, start, stop, code);
            osi_RestorePreemption(preempting);
            vol_EndVnodeOp(volp, NULL);
        }
        goto fsyncerr;
    }
    preempting = osi_PreemptionOff();
    if (vcp = tkc_Get(vp, TKN_DATA_WRITE|TKN_STATUS_WRITE, 0)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_FSYNC, 0, NULL))) {
            VOPN_FSYNC(bdp, flag, credp, start, stop, code);
            vol_EndVnodeOp(volp, NULL);
        }
        tkc_Put(vcp);
    } else
        code = EINVAL;
    osi_RestorePreemption(preempting);
fsyncerr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_fsync %d\n", code));
    return (code);
}

/*
** Name : xglue_inactive
**
** Input : vp - pointer to vnode which is being inactivated.
**
** Output : None.
**
** Returns : 0.
**
** Notes : VOP_INACTIVE is invoked when a vnode reference count goes down 
**         to zero. vn_rele from os/vnode.c invokes VOP_INACTIVE.
**
** Observations: SGI vop_inactive signature matches that of HPUX, except 
**               that the return type is void in the SGI version, while 
**               it is int in the HPUX version. No token management
**               is needed for this call.  In 6.4, it is int.
*/
static int
xglue_inactive(
  bhv_desc_t	*bdp,
  osi_cred_t *credp)
{
    /* REFERENCED */
    vnode_t	*vp;
    int		code=0;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_inactive=%x\n", vp));
    VOPN_INACTIVE(bdp,credp,code);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_inactive: %d\n", code));
    return(code);
}

/*
** Name : xglue_fid
**
** Input : vp - pointer to vnode of file whose fid is being sought.
**
** Output : fidpp - A fid structure is allocated as a result of the call 
**                  to xglue_fid, and the pointer to that structure is 
**                  deposited at *fidpp.
**
** Returns : 0 on success and error code on failure.
**
** Notes : VOP_FID is used by the nfs layer to manufacture file handles 
**         that the server gives to clients. Examples are makefh in nfs.
**
** Observations: The SGI xglue_fid matches that of HPUX. This procedure 
**               needs no token management. There are no changes to this 
**               procedure from the HPUX version.
*/
static int
xglue_fid(
    bhv_desc_t	*bdp,
    struct fid **fidpp)
{
    int code;
    /* REFERENCED */
    vnode_t	*vp;

    /*
     * XXX Any TOKENS here?? XXX
     */
    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_fid, vp = %x", vp));
    VOPN_FID(bdp, fidpp, code);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_fid (code %d)\n", code));
    return (code);
}

/*
** Name : xglue_fid2
**
** Input : vp - pointer to vnode of file whose fid is being sought.
**
** Output : fidp - A fid structure is allocated as a result of the call 
**                 to xglue_fid, and the pointer to that structure is 
**                 deposited at *fidp
**
** Returns : 0 on success and error code on failure.
**
** Notes : VOP_FID2 is used by the xfs layer to manufacture file handles 
**         with 64-bit inode numbers. 
**
** Observations: This routine should behave similar to the fid routine above.
*/
static int
xglue_fid2(
    bhv_desc_t	*bdp,
    struct fid *fidp)
{
    int code;
    /* REFERENCED */
    vnode_t *vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_fid, vp = %x", vp));
    VOPN_FID2(bdp, fidp, code);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_fid (code %d)\n", code));
    return (code);
}

/*
** Name : xglue_rwlock
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
** Observations: This procedure is completely new for SGI and does not 
**               exist in the HPUX version. No token management is done.
*/
static void
xglue_rwlock(
    bhv_desc_t	*bdp,
    vrwlock_t write_lock)
{
    /* REFERENCED */
    vnode_t *vp;
    /*
     * XXX Any TOKENS here?? XXX
     */
    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_rwlock, vp = %x", vp));
    VOPN_RWLOCK(bdp, write_lock);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_rwlock (code %d)\n", code));
    /* XX-RAG void -> no return value */
}

/*
** Name : xglue_rwunlock
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
** Observations: This procedure is completely new for SGI and does not 
**               exist in the HPUX version. No token management is done.
*/
static void
xglue_rwunlock(
    bhv_desc_t	*bdp,
    vrwlock_t write_lock)
{
    /* REFERENCED */
    vnode_t	*vp;
    /*
     * XXX Any TOKENS here?? XXX
     */
    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_rwunlock, vp = %x", BHV_TO_VNODE(bdp)));
    VOPN_RWUNLOCK(bdp, write_lock);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_rwunlock (code %d)\n", code));
    /* XXX-RAG void -> no return value
    return (code);
    */
}

/*
** Name : xglue_seek
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
** Notes : xglue_seek is invoked when an lseek(2) system call is made.
**
** Observations: This routine does not exist in the HPUX version of glue 
**               code but does in the OSF version. The signature of SGI 
**               xglue_seek is different from that of the OSF
**               version. There is no need to do token management, as the 
**               file system layer does nothing on seek calls except verify 
**               that the new offset being set is in the zero to 
**               MAX_FILE_OFFSET range.
*/
static int
xglue_seek(
    bhv_desc_t	*bdp,
    off_t oldoff,
    off_t *newoffp)
{
    int code;
    /* REFERENCED */
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_seek: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    VOPN_SEEK(bdp, oldoff, newoffp, code);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_seek: %d\n", code));
    return(code);
}

/*
** Name : xglue_cmp
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
** Observations: This routine does not exist in any of the reference glue 
**               logic sources. There is no need to do token management, as 
**               the file system layer is just returning true or false based 
**               on whether vp1 == vp2.
*/
static int
xglue_cmp(
    bhv_desc_t		*bdp,
    struct vnode 	*vp2)
{
    int code;
    /* REFERENCED */
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_cmp: (vp1=0x%x) (vp2=0x%x)\n", 
						BHV_TO_VNODE(bdp),vp2));
    VOPN_CMP(bdp, vp2, code);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_cmp: %d\n", code));
    return(code);
}

/*
** Name : xglue_frlock
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
** Observations: SGI vnodeop frlock seems to be analogous to HPUX vnodeop 
**               lockcntl. The code below is from the HPUX lockcntl glue 
**               code, adapted for SGI.
**
***SDM***        There's work remaining to be done with 64 bit versions below.
**
**               The comments below are from the original HPUX version.
**
**               The lockctl vnode operations themselves shouldn't be done 
**               under a vol_StartVnodeOp.  This is because if the 
**               VOP*_LOCKCTL call blocks, we don't want to be left blocking 
**               out new volume operations while waiting for the lock.  Note 
**               that this means all file systems have to implement
**               VOP*_LOCKCTL such that it can run correctly independent 
**               of conflicting volume operations (although the token work 
**               have been done properly).
**
**               Note also that this doesn't apply to the VOP_GETATTR done 
**               here, since it doesn't block waiting for other vnode 
**               operations to complete.
*/
static int
xglue_frlock(
  bhv_desc_t	*bdp,
  int cmd,
  struct flock *lfp,
  int flag,
  off_t offset,
  vrwlock_t vrwlock,
  osi_cred_t *cred)
{
    vnode_t	*vp;
    int code = 0, tokenType;
    uint64_t startPos, endPos;
    register struct tkc_vcache *vcp;
    tkc_byteRange_t byteRange;
    struct volume *volp;
    afs_recordLock_t lockBlocker;
    int oldwhence = lfp->l_whence;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_lockctl "));

    vp = BHV_TO_VNODE(bdp);
    code = xvfs_GetVolume(vp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_lockctl: code = %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        VOPN_FRLOCK(bdp, cmd, lfp, flag, offset, vrwlock, cred, code);
        goto lckerr;
    }

    switch (lfp->l_whence) {
        case 0: startPos = lfp->l_start; break;			/* SEEK_SET */
        case 1: startPos = lfp->l_start + offset; break;	/* SEEK_CUR */
        case 2:							/* SEEK_END */
        {
            register struct tkc_vcache *tvcp;
            struct vattr va;
            int preempting;
 
	    preempting = osi_PreemptionOff();
            if (tvcp = tkc_Get(vp, TKN_STATUS_READ, 0)) {
                if (!(code = vol_StartVnodeOp(volp, VNOP_GETATTR, 0, NULL))) {
                    VOPN_GETATTR(bdp, &va, 0, cred, code);
                    vol_EndVnodeOp(volp, NULL);
                }
                tkc_Put(tvcp);
            } else {
                code = EINVAL;
                osi_RestorePreemption(preempting);
                goto lckerr;
            }
            osi_RestorePreemption(preempting);

            startPos = lfp->l_start + va.va_size;
            break;
        }
        default:
            code = EINVAL;
            goto lckerr;
     }
    endPos = (lfp->l_len == 0 ?  0x7fffffff : startPos + lfp->l_len - 1);

    if (lfp->l_type == F_UNLCK) {
        VOPN_FRLOCK(bdp, cmd, lfp, flag, offset, vrwlock, cred, code);
        byteRange.beginRange = startPos; byteRange.endRange = endPos;
        if (code = (int)tkc_PutLocks(vp, &byteRange)) {
            code = EINVAL;
            goto lckerr;
        }
    } else {
        tokenType  = (lfp->l_type == F_RDLCK ? TKN_LOCK_READ:TKN_LOCK_WRITE);
        byteRange.beginRange = startPos;
        byteRange.endRange = endPos;
        code = (int)tkc_GetLocks(vp, tokenType, &byteRange, (((cmd==F_SETLKW) ||
                                (cmd==F_RSETLKW))? 1 : 0), &lockBlocker);
        if (code) {
            /*
             * If only F_GETLK, return the blocker's info
             */
            if ((cmd == F_GETLK) || (cmd == F_RGETLK)) {
                code = 0;
                lfp->l_whence = oldwhence;
                lfp->l_type = lockBlocker.l_type;
                lfp->l_start = (long)lockBlocker.l_start_pos;
                lfp->l_pid = lockBlocker.l_pid;
		lfp->l_sysid = lockBlocker.l_sysid;

                if (lockBlocker.l_end_pos == 0x7fffffff)
                   lfp->l_len = 0;
                else
                   lfp->l_len = (long)(lockBlocker.l_end_pos - 
                                    lockBlocker.l_start_pos + 1);
            }
            goto lckerr;
        }
        VOPN_FRLOCK(bdp, cmd, lfp, flag, offset, vrwlock, cred, code);
    }

lckerr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_lockctl (code %d)\n", code));
    return (code);
}

/*
** Name : xglue_realvp
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
** Observations: This routine does not exist in any of the reference glue 
**               logic sources. No token management is done since the file 
**               systems don't really seem to do anything for this call.
*/
static int
xglue_realvp(
    bhv_desc_t		*bdp,
    struct vnode 	**vp2)
{
    int code;
    /* REFERENCED */
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_realvp: (vp1=0x%x) \n", BHV_TO_VNODE(bdp)));
    VOPN_REALVP(bdp, vp2, code);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_realvp: (vp2=0x%d) (code=%d) \n", 
                                                              vp2, code));
    return(code);
}

/*
** Name : xglue_bmap
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
** Observations: Shouldn't we be getting status write tokens because we
**               might enlarge the file? The answer is no, as this operation
**               does not change any of the file contents or status.
*/
static int 
xglue_bmap(
  bhv_desc_t	*bdp,
  off_t offset,
  ssize_t count,
  int flags,
  osi_cred_t *credp,
  struct bmapval *bmapp,
  int *nbmaps)
{
    int code;
    /* REFERENCED */
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_bmap: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    VOPN_BMAP(bdp, offset, count, flags, credp, bmapp, nbmaps, code);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_bmap: %d\n", code));
    return (code);
}

/*
** Name : xglue_strategy
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
** Observations: The signature of SGI vop_strategy has one additional param,
**               the vnode pointer, as compared to that of the HPUX version.
*/
static void 
xglue_strategy(
  bhv_desc_t	*bdp,
  struct buf *bp)
{
    /* REFERENCED */
  vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_strategy: (vp=0x%x)\n", vp));
    VOPN_STRATEGY(bdp, bp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_strategy: %d\n", code));
    /* XXX-RAG no return because void */
}

/*
** Name : xglue_map
**
** Input : vp - pointer to vnode of file that is being mmapped or execed.
**         offset - offset into file at which mapping should start
**         len - size of mapping
**         prot - protection associated with mapping
**         flags - specify private or shared mapping.
**         credp - pointer to user credentials structure
**	   nvp - 
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : exec and mmap call VOP_MAP.
**
** Observations: Code modelled after xglue_map in RIOS/xvfs_aixglue.c.
**               Signature between the RIOS version and the SGIMIPS version
**               is quite different. 
***SDM***        Can't we decide whether to get read tokens or write tokens
**               based on the protection type?
*/
static int 
xglue_map(
    bhv_desc_t	*bdp,
    off_t offset,
    size_t len,
    mprot_t prot,
    uint flags,
    osi_cred_t *credp,
    vnode_t **nvp)
{
    register struct tkc_vcache *vcp;
    int code;
    struct volume *volp;
    int preempting;
    vnode_t	*vp;

    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_map\n"));
    vp = BHV_TO_VNODE(bdp);
    code = xvfs_GetVolume(vp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_map: %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_MAP, 0, NULL))) {
            VOPN_MAP(bdp, offset, len, prot, flags, credp, nvp, code);
            vol_EndVnodeOp(volp, NULL);
        }
        goto out;
    }
    preempting = osi_PreemptionOff();
    if (vcp = tkc_Get(vp, TKN_UPDATE, 0)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_MAP, 0, NULL))) {
            VOPN_MAP(bdp, offset, len, prot, flags, credp, nvp, code);
            vol_EndVnodeOp(volp, NULL);
        }
        tkc_Put(vcp);
    } else
        code = EINVAL;
    osi_RestorePreemption(preempting);
out:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("%d\n", code));
    return (code);
}

/*
** Name : xglue_addmap
**
** Input : vp - pointer to vnode of file that is being mmapped or execed.
**         map_type - operation for addmap
**         vt - pointer to structure containing regions
**         pgno_type - virtual page number type
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
**         system layer doesn't do anything in kudzu.
**
** Observations: xglue_addmap does not exist in any of the reference
**               platforms. Since the file system layer does not do anything
**               significant for this routine, we do no token management here.
*/
static int
xglue_addmap(
    bhv_desc_t	*bdp,
    vaddmap_t map_type,
    struct __vhandl_s *vt,
    pgno_t *pgno_type,
    off_t offset,
    size_t len,
    mprot_t prot,
    struct cred *credp)
{
    int code;
    /* REFERENCED */
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_addmap: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    VOPN_ADDMAP(bdp, map_type, vt, pgno_type, offset, len, prot, credp, code);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_addmap: %d\n", code));
    return (code);
}

/*
** Name : xglue_delmap
**
** Input : vp - pointer to vnode of file that is being mmapped or execed.
**         offset - offset into file at which mapping should start
**         pregp - pointer to pregion structure in which to set mapping
**         addrp - virtual address at which mapping should start
**         len - size of mapping
**         prot - protection associated with mapping
**         max_prot - max protections allowed.
**         flags - specify private or shared mapping.
**         credp - pointer to user credentials structure
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : called from replacereg, dupreg and mapreg in region.c. The file
**
** Observations: xglue_addmap does not exist in any of the reference
*/
static int
xglue_delmap(
    bhv_desc_t	*bdp,
    struct __vhandl_s *vt,
    size_t len,
    osi_cred_t *credp)
{
    int code;
    /* REFERENCED */
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_delmap: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    VOPN_DELMAP(bdp, vt, len, credp, code);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_delmap: %d\n", code));
    return (code);
}

/*
** Name : xglue_poll
**
** Input : vp - pointer to vnode of file for which poll is being done.
**         events - events being sought
**         anyyet - ???
**
** Output : reventsp - actual events that occured are returned here
**          phpp - poll head pointer???
**
** Returns : returns 1 on success. error code otherwise.
**
** Notes : Efs and xfs seem to call fs_poll in fs_subr.c, which doesn't
**         do much. There are no calls to vop_poll in os.
**
** Observations: xglue_poll does not exist in any reference code. Since the
**               file systems don't do much for this call, there is no need
**               to do any token management.
*/
static int 
xglue_poll(
  bhv_desc_t	*bdp,
  short events,
  int anyyet,
  short *reventsp,
  struct pollhead **phpp,
  unsigned int *genp)
{
    int code;
    /* REFERENCED */
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_poll: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    VOPN_POLL(bdp, events, anyyet, reventsp, phpp, genp, code);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_poll: %d\n", code));
    return (code);
}

/*
** Name : xglue_dump
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
** Notes : Both xfs and efs point to fs_nosys for this vnodeop.
**
** Observations: Does not exist in any reference platform implementation.
**               No need for token management. 
*/
static int 
xglue_dump(
  bhv_desc_t	*bdp,
  caddr_t addr,
  daddr_t bn,
  u_int count)
{
    int code;
    /* REFERENCED */
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_dump: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    VOPN_DUMP(bdp, addr, bn, count, code);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_dump: %d\n", code));
    return (code);
}

/*
** Name : xglue_pathconf
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
** Observations: No need to obtain tokens.
*/
static int 
xglue_pathconf(
  bhv_desc_t	*bdp,
  int cmd,
  long *valp,
  osi_cred_t *cr)
{
    int code;
    /* REFERENCED */
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_pathconf: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    VOPN_PATHCONF(bdp, cmd, valp, cr, code);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_pathconf: %d\n", code));
    return (code);
}

/*
** Name : xglue_allocstore
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
** Observations: No need to obtain tokens.
*/
static int 
xglue_allocstore(
  bhv_desc_t	*bdp,
  off_t offset,
  size_t count,
  osi_cred_t *credp)
{
    int code;
    /* REFERENCED */
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_allocstore: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
#if 0
    /*  The parameter types are mismatched here, but pathconf only
	returns EINVAL, so I'm going to just return EINVAL at
	this level.  */
    VOPN_PATHCONF(bdp, offset, &count, credp, code);
#endif
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_allocstore: %d\n", code));
    return (EINVAL);
}

/*
** Name : xglue_fcntl
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
** Observations: Since no data or status changes are made to the file, no
**               tokens are obtained below before calling VOPN_FCNTL.
*/
static int 
xglue_fcntl(
  bhv_desc_t	*bdp,
  int cmd,
  void *arg,
  int flags,
  off_t offset,
  osi_cred_t *cr,
  rval_t *rvp)
{
    register struct tkc_vcache *vcp;
    register int code;
    struct volume *volp;
    int preempting;
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_fcntl: (vp=0x%x)\n", vp));
    code = xvfs_GetVolume(vp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_fcntl: code = %d\n", code));
      return(code);
    }

    if (!xvfs_NeedTokens(volp)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_SETATTR, 0, NULL))) {
            preempting = osi_PreemptionOff();
            VOPN_FCNTL(bdp, cmd, arg, flags, offset, cr, rvp, code);
            osi_RestorePreemption(preempting);
            vol_EndVnodeOp(volp, NULL);
        }
        goto fcntlerr;
    }
    preempting = osi_PreemptionOff();
    if (vcp = tkc_Get(vp, TKN_DATA_WRITE|TKN_STATUS_WRITE, 0)) {
        if (!(code = vol_StartVnodeOp(volp, VNOP_SETATTR, 0, NULL))) {
            VOPN_FCNTL(bdp, cmd, arg, flags, offset, cr, rvp, code);
            vol_EndVnodeOp(volp, NULL);
        }
        tkc_Put(vcp);
    } else
        code = EINVAL;
    osi_RestorePreemption(preempting);
fcntlerr:
    xvfs_PutVolume(volp);
    AFSLOG(XGLUE_DEBUG, 1, ("END xglue_fcntl: %d\n", code));
    return (code);
}

/*
** Name : xglue_reclaim
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
** Observations: Code modelled after OSF1 version of xglue_reclaim.
**               No need to obtain tokens as file status and data should not
**               change as a result of VOP_RECLAIM.
*/
static int 
xglue_reclaim(
    bhv_desc_t	*bdp,
    int flag)
{
    int code;
    /* REFERENCED */
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_reclaim: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    VOPN_RECLAIM(bdp, flag, code);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_reclaim: %d\n", code));
    return(code);
}

/*
** Name : xglue_attr_get
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
** Observations: These attr_* vnodeops are new for SGI and do not exist on
**		 any other reference platform. No token management is done
**		 for these calls.
*/
static int 
xglue_attr_get(
    bhv_desc_t	*bdp,
    char *name,
    char *value,
    int *valuelenp,
    int flags,
    osi_cred_t *credp)
{
    int code;
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_attr_get: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    VOPN_ATTR_GET(bdp, name, value, valuelenp, flags, credp, code);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_attr_get: %d\n", code));
    return(code);
}

/*
** Name : xglue_attr_set
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
** Observations: 
*/
static int 
xglue_attr_set(
    bhv_desc_t	*bdp,
    char *name,
    char *value,
    int valuelen,
    int flags,
    osi_cred_t *credp)
{
    int code;
    /* REFERENCED */
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_attr_set: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    VOPN_ATTR_SET(bdp, name, value, valuelen, flags, credp, code);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_attr_set: %d\n", code));
    return(code);
}

/*
** Name : xglue_attr_remove
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
** Observations: Code does not exist in any reference platform.
*/
static int 
xglue_attr_remove(
    bhv_desc_t	*bdp,
    char *name,
    int flags,
    osi_cred_t *credp)
{
    int code;
    /* REFERENCED */
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_attr_remove: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    VOPN_ATTR_REMOVE(bdp, name, flags, credp, code);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_attr_remove: %d\n", code));
    return(code);
}

/*
** Name : xglue_attr_list
**
** Input :  vp - pointer to vnode whose attributes are being listed
**
** Output : None.
**
** Returns : 0 on success and error code on failure.
**
** Notes : Only xfs uses this entry point.
**
** Observations: Code does not exist in any other reference platform.
*/
static int 
xglue_attr_list(
    bhv_desc_t	*bdp,
    char *buffer,
    int buflen,
    int flags,
    struct attrlist_cursor_kern *cursor,
    osi_cred_t *credp)
{
    int code;
    /* REFERENCED */
    vnode_t	*vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_attr_list: (vp=0x%x)\n", BHV_TO_VNODE(bdp)));
    VOPN_ATTR_LIST(bdp, buffer, buflen, flags, cursor, credp, code);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_attr_list: %d\n", code));
    return(code);
}

/*
 * Added cr parameter
 * XXX RAG do not know how in HPUX params to xglue_unmount and
 * VFSX_UNMOUNT are different.
 */
/* VFS Operation */
int xglue_unmount (
    bhv_desc_t	*bdp,
    int flag,
    osi_cred_t *cr)
{
    int code;				/* error return code */
    struct volume *volp;		/* volume registry entry */
    struct vfs *tvfsp;			/* loop control */
    struct vfs *vfsp;			/* virtual file system */

    vfsp = (struct vfs *)BHV_VOBJ(bdp);
    code = xvfs_VfsGetVolume(vfsp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("xglue_unmount: code = %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens (volp)) {
	if (!(code = vol_StartVnodeOp(volp, VFSOP_UNMOUNT, 0, NULL))) {
	    code = VFSX_UNMOUNT (vfsp, flag, cr);
	    vol_EndVnodeOp(volp, NULL);
	}
	goto done;
    }

    if (tkc_flushvfsp (vfsp)) {
	 code = EBUSY;
	 goto done;
    }

    if (!(code = vol_StartVnodeOp(volp, VFSOP_UNMOUNT, 0, NULL))) {
	code = VFSX_UNMOUNT (vfsp, flag, cr);
	vol_EndVnodeOp(volp, NULL);
    }

done:
    xvfs_PutVolume (volp);
    return (code);
}


/* VFS Operation */
/*
 * removed extra arg nm. Not needed in SGI
 */
int xglue_root (
    bhv_desc_t	*bdp,
    struct vnode **vpp)			/* where to put vnode pointer */
{
    int code;				/* error return code */
    struct volume *volp;		/* volume registry entry */
    struct vfs *vfsp;			/* virtual file system */

    vfsp = (struct vfs *)BHV_VOBJ(bdp);
    *vpp = 0; 			/* clear any garbage of caller's */
    code = xvfs_VfsGetVolume(vfsp, &volp);
    if (code) {
      AFSLOG(XGLUE_DEBUG, 1, ("In xglue_root: code = %d\n", code));
      return(code);
    }
    if (!xvfs_NeedTokens (volp)) {
	if (!(code = vol_StartVnodeOp(volp, VFSOP_ROOT, 0, NULL))) {
	    code = VFSX_ROOT (vfsp, vpp);
	    if (volp && !code) /* registered volume but not exported? */
		XVFS_CONVERT (*vpp);
	    vol_EndVnodeOp(volp, NULL);
	}
	goto done;
    }

    if (!(code = vol_StartVnodeOp(volp, VFSOP_ROOT, 0, NULL))) {
	code = VFSX_ROOT (vfsp, vpp);
	vol_EndVnodeOp(volp, NULL);
    }
    if (!code)
	XVFS_CONVERT (*vpp);

done:
    xvfs_PutVolume (volp);
    return (code);
}


/* VFS Operation */
int xglue_vget (
    bhv_desc_t	*bdp,
    struct vnode **vpp,			/* where to put vnode pointer */
    struct fid *fidp)			/* file ID */
{
    int code;				/* error return code */
    struct volume *volp;		/* volume registry entry */
    struct vfs *vfsp;			/* virtual file system */

    vfsp = (struct vfs *)BHV_VOBJ(bdp);
    *vpp = NULL;	/* eliminate any stack garbage in caller */
    code = xvfs_VfsGetVolume (vfsp, &volp);
    if (!xvfs_NeedTokens(volp)) {
	if (!(code = vol_StartVnodeOp(volp, VFSOP_VGET, 0, NULL))) {
	    code = VFSX_VGET (vfsp, vpp, fidp);
	    if (volp && !code && *vpp) /* registered volume but not exported? */
		XVFS_CONVERT (*vpp);
	    vol_EndVnodeOp(volp, NULL);
	}
	goto done;
    }

    if (!(code = vol_StartVnodeOp(volp, VFSOP_VGET, 0, NULL))) {
	code = VFSX_VGET (vfsp, vpp, fidp);
	if (!code && *vpp)
	    XVFS_CONVERT (*vpp);
	vol_EndVnodeOp(volp, NULL);
    }

done:
    xvfs_PutVolume (volp);
    return (code);
}
/*
 *  xglue_link_removed
 */
static void
xglue_link_removed(
    bhv_desc_t  *bdp,
    vnode_t 	*dvp,
    int		linkzero)
{
    /* REFERENCED */
    vnode_t     *vp;

    vp = BHV_TO_VNODE(bdp);
    AFSLOG(XGLUE_DEBUG, 1, ("In xglue_link_removed: (vp=0x%x)\n", 
	BHV_TO_VNODE(bdp)));
    VOPN_LINK_REMOVED(bdp, dvp, linkzero);
    AFSLOG(XGLUE_DEBUG, 1, ("xglue_link_removed done\n"));
}

/* vector of N-ops */

struct osi_vnodeops xglue_ops = {
    BHV_IDENTITY_INIT_POSITION(VNODE_POSITION_BASE),
    xglue_open,
    xglue_close,
    xglue_read,
    xglue_write,
    xglue_ioctl,
    xglue_setfl,
    xglue_getattr,
    xglue_setattr,
    xglue_access,
    xglue_lookup,
    xglue_create,
    xglue_remove,
    xglue_link,
    xglue_rename,
    xglue_mkdir,
    xglue_rmdir,
    xglue_readdir,
    xglue_symlink,
    xglue_readlink,
    xglue_fsync,
    xglue_inactive,
    xglue_fid,
    xglue_fid2,
    xglue_rwlock,
    xglue_rwunlock,
    xglue_seek,
    xglue_cmp,
    xglue_frlock,
    xglue_realvp,
    xglue_bmap,
    xglue_strategy,
    xglue_map,
    xglue_addmap,
    xglue_delmap,
    xglue_poll,
    xglue_dump,
    fs_pathconf,		/* PV 455500, gcc doesn't handle bad return
				code from pathconf().  We can't get the
				info from the server because there is no
				protocol, so we just give the value that
				the client file system is using.  These
				are inaccurate, but the same as the SUN
				implementation.  Currently, the Cray
				implementation returns -1.  If this is
				a server, the value from fs_pathconf
				is correct. */
    xglue_allocstore,
    xglue_fcntl,
    xglue_reclaim,
    xglue_attr_get,
    xglue_attr_set,
    xglue_attr_remove,
    xglue_attr_list,
    (vop_cover_t)fs_nosys, 	
    fs_noval,
    fs_vnode_change,
    fs_tosspages,
    fs_flushinval_pages,
    fs_flush_pages,
    fs_invalfree_pages,
    fs_pages_sethole,
    (vop_commit_t)fs_nosys,
    (vop_readbuf_t)fs_nosys,
    fs_strgetmsg,
    fs_strputmsg,
};
