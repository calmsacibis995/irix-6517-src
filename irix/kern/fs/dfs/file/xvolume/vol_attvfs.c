/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: vol_attvfs.c,v 65.3 1998/03/23 16:46:51 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: vol_attvfs.c,v $
 * Revision 65.3  1998/03/23 16:46:51  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 20:01:46  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:21:46  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.633.1  1996/10/02  19:04:34  damon
 * 	New DFS from Transarc
 * 	[1996/10/01  18:59:55  damon]
 *
 * Revision 1.1.628.3  1994/07/13  22:24:56  devsrc
 * 	merged with bl-10
 * 	[1994/06/28  21:02:00  devsrc]
 * 
 * 	Changed #include with double quotes to #include with angle brackets.
 * 	[1994/04/28  16:04:57  mbs]
 * 
 * Revision 1.1.628.2  1994/06/09  14:26:39  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:38:29  annie]
 * 
 * Revision 1.1.628.1  1994/02/04  20:37:33  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:21:15  devsrc]
 * 
 * Revision 1.1.626.1  1993/12/07  17:39:22  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  18:23:45  jaffe]
 * 
 * Revision 1.1.5.6  1993/01/21  16:36:15  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  18:30:09  cjd]
 * 
 * Revision 1.1.5.5  1993/01/13  20:10:52  shl
 * 	Transarc delta: mcinerny-ot6067-rename-spares-in-vol_stat_st 1.2
 * 	  Selected comments:
 * 	        Gave names to the three no-longer-un-used spares in vol_stat_st.
 * 	    Namely:
 * 	    spare1 --> tokenTimeout
 * 	    spare2 --> refCount
 * 	    spare3 --> volMoveTimeout
 * 	    also:
 * 	    VOL_STAT_SPARE3 --> VOL_STAT_VOLMOVETIMEOUT
 * 	    and struct ftserver_status_static:
 * 	    static_spare1 --> tokenTimeout
 * 	    static_spare2 --> refCount
 * 	    static_spare3 --> volMoveTimeout
 * 	    change member refs to new names
 * 	    oops--reopened the wrong delta.
 * 	[1993/01/12  22:39:48  shl]
 * 
 * Revision 1.1.5.4  1992/11/24  20:46:17  bolinger
 * 	Change include file install directory from .../afs to .../dcedfs.
 * 	[1992/11/22  19:42:25  bolinger]
 * 
 * Revision 1.1.5.3  1992/09/15  13:22:22  jaffe
 * 	Transarc delta: jaffe-ot4609-cleanup-lock-initialization 1.5
 * 	  Selected comments:
 * 	    Removed knowledge of the internal structure of the osi lock package from the
 * 	    rest of DFS.  In many cases this meant initializing locks on the fly instead
 * 	    of statically.  Because some code needs to have a static lock initialization,
 * 	    I've added a LOCK_DATA_INITIALIZER constant to lock.h.  This should be used
 * 	    whenever a lock needs to be statically initialized.
 * 	    add a function vol_InitAttVfsMod to initialize vol_attlock.
 * 	    declare vol_AddVfs to return a void.
 * 	    declare vol_RemoveVfs to return a void.
 * 	    fixed compilation errors caused by correct ANSI prototypes.
 * 	    use osi_vfs instead of vfs/mount.
 * 	    fixed ANSI/K&R mismatch re: "char" type in a function prototype.
 * 	    make vol_FindVfs take an int as the third argument instead of a char./
 * 	[1992/09/14  20:58:54  jaffe]
 * 
 * Revision 1.1.5.2  1992/08/31  22:10:03  jaffe
 * 	Replace missing RCS ids
 * 	[1992/08/31  16:25:55  jaffe]
 * 
 * 	Transarc delta: vijay-ot2674-vnode-concurrency 1.7
 * 	  Selected comments:
 * 	    Pass the vnode ops type (noop, readonly, readwrite) through to vol_FindVfs
 * 	    and volreg_Lookup. If the fileset is busy, compatibility between the type and
 * 	    the value of the concurrency field is determined. If compatible, the vnode op
 * 	    proceeds, else it sleeps. If the fileset is in a persistent error state, the
 * 	    error is returned back from the vnode op. The call signature for
 * 	    xvfs_GetVolume is changed to return this error value.
 * 	    This delta is checkpoint the changes made before testing.
 * 	    vol_FindVfs takes vnode ops type as a parameter.
 * 	    Change the xvfs_VfsGetVolume interface as well along with its variants.
 * 	    Fix lots of old bugs in vol_open's vnode lock-out.
 * 	    Not ready for prime time; closing to upgrade.
 * 	    One big ftserver change is to get the AG_AGGRINFO information before doing
 * 	    a VOL_AGOPEN, so that AG_AGGRINFO's call to VFS_STATFS doesn't deadlock
 * 	    waiting for the VOL_CLOSE (via xglue_root).
 * 	    See above
 * 	    Another checkpoint of on-going work to fix lots of little bugs
 * 	    Use new concurrency values.
 * 	    Had to close to upgrade; these are more fixes that emerged from testing etc.
 * 	    Don't return EIO if a filesystem (vfsp) isn't exported; just return a
 * 	    zero (OK) error code along with a null volp.
 * 	    One more round, distinguishing the VOLOP_SETSTATUS calls that can change
 * 	    a fileset's ID, requiring more clearing-the-way in Episode.
 * 	    A previous revision of this delta broke standalone Episode: many of
 * 	    the glue routines try to lookup the volume in the registry, and if the
 * 	    lookup fails (which it will in the standalone case), the glue routine
 * 	    returns failure.  The symptom was failures with ENODEV whenever
 * 	    accessing a standalone Episode fileset, despite a successful mount.
 * 	[1992/08/30  14:06:05  jaffe]
 * 
 * Revision 1.1.3.2  1992/04/21  16:27:41  mason
 * 	Transarc delta: cfe-ot2535-FindVfs-bad-loop 1.1
 * 	  Files modified:
 * 	    xvolume: vol_attvfs.c, vol_init.c
 * 	  Selected comments:
 * 	    Bad target for a ``continue'' in vol_FindVfs().
 * 	    Go for a goto rather than a disfunctional continue.
 * 	[1992/04/20  23:25:57  mason]
 * 
 * Revision 1.1  1992/01/19  02:58:36  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
/*
*/
/* Copyright (C) 1989, 1990 Transarc Corporation - All rights reserved */

#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/basicincludes.h>
#include <dcedfs/osi.h>			/* OS independence */
#include <dcedfs/osi.h>			/* OS independence */
#include <dcedfs/lock.h>
#include <dcedfs/volume.h>
#include <dcedfs/xvfs_vnode.h>
#include <vol_errs.h>
#include <vol_init.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/xvolume/RCS/vol_attvfs.c,v 65.3 1998/03/23 16:46:51 gwehrman Exp $")

/*
 * This module is specific to pre-existing file systems whose vnodes and vfs
 *   structures we cannot modify for exporting.
 *   It maintains a table associating VFS's with DFS volumes.  To find the
 *   volume that a given vnode belongs to, we look up its VFS in this table.
 */


struct tvol {
    struct tvol *next;
    struct volume *vol;
    struct osi_vfs *vfsp;
};

struct tvol *vol_attvfs = 0;		/* Global table of attached vols */
struct lock_data vol_attlock;		/* Lock for vol_attvfs table */

/* 
 * vol_InitAttVfsMod -- initialize the vol_attvfs.c module.
 */
void vol_InitAttVfsMod()
{
    lock_Init(&vol_attlock);
}

/*
 * Find the attached volume belonging to partition, vfsp
 */
int vol_FindVfs(vfsp, volpp)
  struct osi_vfs *vfsp;
  struct volume **volpp;
{
    register struct tvol *tp;
    register struct volume *volp;
    register long code;

    /* init the result */
    *volpp = (struct volume *) 0;
    lock_ObtainRead(&vol_attlock);
    for (tp = vol_attvfs; tp; tp = tp->next) {
	if (tp->vfsp == vfsp) {
	    volp = tp->vol;
	    VOL_HOLD(volp);
	    lock_ReleaseRead(&vol_attlock);
	    *volpp = volp;
	    return 0;
	}
    }
    lock_ReleaseRead(&vol_attlock);
    *volpp = (struct volume *)0;
    return(0);	/* not exported */
}


/*
 * Add a new entry vol entry (in partition vfsp) entry
 */
void vol_AddVfs(vfsp, volp)
    struct osi_vfs *vfsp;
    struct volume *volp;
{
    register struct tvol *tvp;

    lock_ObtainWrite(&vol_attlock);
    for (tvp = vol_attvfs; tvp; tvp = tvp->next) {
	if (tvp->vol == volp) {
	    lock_ReleaseWrite(&vol_attlock);
	    return;
	}
    }
    tvp = (struct tvol *) osi_Alloc(sizeof(struct tvol));
    tvp->next = vol_attvfs;
    tvp->vol = volp;
    VOL_HOLD(volp);	/* for the reference in this vfs table */
    tvp->vfsp = vfsp;
    vol_attvfs = tvp;
    lock_ReleaseWrite(&vol_attlock);
    return;
}


/*
 * Remove volp entry from the global table
 */
void vol_RemoveVfs(volp)
    register struct volume *volp;
{
    register struct tvol *tp, **lpp;

    lock_ObtainWrite(&vol_attlock);
    lpp = &vol_attvfs;
    for (tp = *lpp; tp; lpp = &tp->next, tp = *lpp) {
	if (tp->vol == volp) {
	    *lpp = tp->next;	/* remove us */
	    VOL_RELE(volp);
	    osi_Free((opaque) tp, sizeof(struct tvol));
	    break;
	}
    }
    lock_ReleaseWrite(&vol_attlock);
    return;
}
