/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: px_config.c,v 65.3 1998/03/23 16:43:25 gwehrman Exp $";
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
 * $Log: px_config.c,v $
 * Revision 65.3  1998/03/23 16:43:25  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 20:01:03  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/24  14:29:46  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:50  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:48:35  dce
 * *** empty log message ***
 *
 * Revision 1.2  1996/04/19  23:05:15  brat
 * Corrected the header files included. agfs_mount.h is not needed, but it
 * dragged in a bunch of xaggr,osi files which are now directly included.
 *
 * Revision 1.1  1996/04/16  18:32:42  vrk
 * Initial revision
 *
 * Revision 1.1  1994/08/23  18:58:47  dcebuild
 * Original File from OSF
 *
 * Revision 1.1.14.3  1994/08/23  18:58:47  rsarbo
 * 	ifdef out code dependent on oldpac.
 * 	[1994/08/23  16:44:14  rsarbo]
 *
 * Revision 1.1.14.2  1994/06/09  14:12:38  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:25:59  annie]
 * 
 * Revision 1.1.14.1  1994/02/04  20:22:07  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:14:39  devsrc]
 * 
 * Revision 1.1.12.1  1993/12/07  17:27:38  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  15:44:41  jaffe]
 * 
 * Revision 1.1.8.2  1993/07/19  19:32:10  zeliff
 * 	HP port of DFS
 * 	[1993/07/19  18:24:33  zeliff]
 * 
 * Revision 1.1.4.5  1993/07/16  20:07:20  kissel
 * 	Merge gamera fixes:
 * 	- Fill in xdfs_vn_rele_op, xdfs_vcache_delete_op upon config.
 * 	[1993/06/29  21:23:25  tmm]
 * 
 * 	ODE is terrible - do resubmit...
 * 	[1993/06/28  18:35:35  tmm]
 * 
 * 	*** empty log message ***
 * 	[1993/06/21  14:31:27  kissel]
 * 
 * 	Kernel extensions support.
 * 	[1993/03/15  16:09:13  toml]
 * 
 * 	Add code to add agfs to vfs table.
 * 	afs -> dcedfs.
 * 	[1993/01/14  19:49:25  toml]
 * 
 * 	Initial coding for HPUX.
 * 	[1992/10/14  16:43:12  toml]
 * 
 * 	Initial GAMERA branch
 * 	[1993/04/02  11:50:02  mgm]
 * 
 * Revision 1.1.5.2  1993/06/28  18:21:42  tmm
 * 	Fill in AFSCALL_GETPAG slot if nosys - gamera compatability.
 * 
 * Revision 1.1.2.2  1993/06/04  14:53:30  kissel
 * 	Initial HPUX RP version.
 * 	[1993/06/03  21:40:44  kissel]
 * 
 * $EndLog$
*/

#include <sys/types.h>
#include <dcedfs/param.h>		/* Should be always first */
#include <dcedfs/osi_sysconfig.h>	/* Before syscall.h for kload.h */
#include <dcedfs/syscall.h>
#include <dcedfs/osi.h>
#include <dcedfs/astab.h>
#include <dcedfs/aggr.h>
#include <dcedfs/lock.h>


extern int afscall_exporter(), afscall_volser(), afscall_aggr(), afs_nosys();
extern struct vfsops agfs_vfsops;

extern struct aggrops ag_ufsops;

extern int (*xdfs_vn_rele_op)();
extern int (*xglue_vcache_delete_op)();
extern int xglue_vcache_delete();
extern int xglue_vn_rele();

int dfs_agfs_mcode = -1; 

#define afson (op == SYSCONFIG_CONFIGURE)

/*
 * The following indirect vectors allow a dynamically loaded DFS or
 * VxFS to call into each other.  Each registers its own vector
 * (holding pointers to ITS exported functions) with the kload
 * manager .. for the other to obtain and call through.
 *
 * For lack of any place better, hold these definitions here and in
 * VxFS itself.  (A header file would be nice ... which one?)
 */

/*
 * First come the lockObtain/Release stubs we provide for VxFS.  Within
 * DFS, these are implemented as macros ... but we want a real function
 * for external (by other extensions, e.g.) use.
 */
static void f_lock_ObtainRead(lock)
struct lock_data	*lock;
{
    lock_ObtainRead(lock);
}

static void f_lock_ObtainWrite(lock)
struct lock_data	*lock;
{
    lock_ObtainWrite(lock);
}

static void f_lock_ObtainShared(lock)
struct lock_data	*lock;
{
    lock_ObtainShared(lock);
}

static void f_lock_ReleaseRead(lock)
struct lock_data	*lock;
{
    lock_ReleaseRead(lock);
}

static void f_lock_ReleaseWrite(lock)
struct lock_data	*lock;
{
    lock_ReleaseWrite(lock);
}

static void f_lock_ReleaseShared(lock)
struct lock_data	*lock;
{
    lock_ReleaseShared(lock);
}


/*
 * Operations exported by DFS.
 * First come the function declarations, then the indirect vector itself
 */

extern void ag_setops();
extern void xvfs_InitFromXOps();
extern void xvfs_InitFromVFSOps();
extern int  vol_Attach();
extern int  volreg_Lookup();
extern int  vol_VolInactive();
extern int  xvfs_IsAdminGroup();
extern void dacl_GetLocalCellID();
extern void dacl_GetSysAdminGroupID();

struct dfs_ext_ops {
    int version_no;
    void (*ag_setops)();
    void (*xvfs_InitFromXOps)();
    void (*xvfs_InitFromVFSOps)();
    void (*lock_ObtainRead)();
    void (*lock_ObtainWrite)();
    void (*lock_ObtainShared)();
    void (*lock_ReleaseRead)();
    void (*lock_ReleaseWrite)();
    void (*lock_ReleaseShared)();
    opaque (*osi_Alloc)();
    void (*osi_Free)();
    int  (*vol_Attach)();
    int  (*volreg_Lookup)();
    int  (*vol_VolInactive)();
    int  (*xvfs_IsAdminGroup)();
    void (*dacl_GetLocalCellID)();
    void (*dacl_GetSysAdminGroupID)();
#ifdef old_pac
    sec_id_pac_t *(*hpdfs_extract_pac)();
#else
    int (*spare0)();
#endif
    int (*spare1)();
    int (*spare2)();
    int (*spare3)();
    int (*spare4)();
    int (*spare5)();
};

struct dfs_ext_ops dfs_ops = {
    1,
    ag_setops,
    xvfs_InitFromXOps,
    xvfs_InitFromVFSOps,
    f_lock_ObtainRead,
    f_lock_ObtainWrite,
    f_lock_ObtainShared,
    f_lock_ReleaseRead,
    f_lock_ReleaseWrite,
    f_lock_ReleaseShared,
    osi_Alloc,
    osi_Free,
    vol_Attach,
    volreg_Lookup,
    vol_VolInactive,
    xvfs_IsAdminGroup,
    dacl_GetLocalCellID,
    dacl_GetSysAdminGroupID,
#ifdef old_pac
    hpdfs_extract_pac,
#else
    NULL,
#endif
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

/*
 * Operations exported by VxFS 
 */
struct vxfs_ext_ops {
    int version_no;
    int (*vxfs_announce_dfs)();
    int (*spare1)();
    int (*spare2)();
    int (*spare3)();
    int (*spare4)();
};

struct vxfs_ext_ops	*vxfs_ops = NULL;

int afscall_fake_getpag(long a, long b, long c, long d, long e, int *f)
{
	return(0);
}

int
px_post_config()
{
#ifndef AFS_IRIX_ENV
	int code;

	xdfs_vn_rele_op = xglue_vn_rele;
	xglue_vcache_delete_op = xglue_vcache_delete;

	/*
	 * Register our exported functions for VxFS to find.  We don't
	 * expect any errors.
	 */
	if ((code = kload_set_cookie("dfs_ext_ops", &dfs_ops)) != 0)
	    return(code);

	/*
	 * If VxFS is present, tell it that we're here as well.
	 */
	vxfs_ops = (struct vxfs_ext_ops *) kload_get_cookie("vxfs_ext_ops");
	if (vxfs_ops != NULL)
	    (*vxfs_ops->vxfs_announce_dfs)();
#endif /* AFS_IRIX_ENV */

	return 0;
}

px_configure(op)
int op;
{
	int code;

	if (op == KLOAD_INIT_UNCONFIG)
		return(EBUSY);
	AfsCall(AFSCALL_PX,   afson ? afscall_exporter : afs_nosys);
	AfsCall(AFSCALL_VOL,  afson ? afscall_volser   : afs_nosys);
	AfsCall(AFSCALL_AGGR, afson ? afscall_aggr     : afs_nosys);

#ifndef AFS_IRIX_ENV 
/* VRK - Since, we don't support agfs, this part of the code is not required.. */
	/*
	 * Plug in UFS aggregate & fileset ops
	 */
	ag_setops (AG_TYPE_UFS, &ag_ufsops);

	if (afson) {
		if ((code = register_vfs_type(MOUNT_AGFS_NAME)) == -1) {
			return(code);
		}
		vfssw_assign(code, agfs_vfsops);
		dfs_agfs_mcode = code;
		agfs_Init(0);
		if ((unsigned)AfsProbe(AFSCALL_GETPAG) == (unsigned)afs_nosys) {
			AfsCall(AFSCALL_GETPAG, afscall_fake_getpag);
		}	
	} else {
                xdfs_vn_rele_op = 0;
                xglue_vcache_delete_op = 0;
		if ((unsigned)AfsProbe(AFSCALL_GETPAG) == (unsigned)afscall_fake_getpag) {
			AfsCall(AFSCALL_GETPAG, afs_nosys);
		}	
	}
#endif /* ! AFS_IRIX_ENV */

	return 0;
}


dfs_server_config(op)
int op;
{
	extern int px_configure(int);

	return(px_configure(op));
}
