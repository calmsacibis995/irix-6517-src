/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_config.c,v 65.7 1998/05/29 15:51:04 bdr Exp $";
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
 * $Log: cm_config.c,v $
 * Revision 65.7  1998/05/29 15:51:04  bdr
 * Add the proper locking around looking at the cred struct in the proc entry.
 *
 * Revision 65.6  1998/04/20  16:00:46  lmc
 * Don't use crhold() to set the reference count when we have just
 * bzero'd the cred structure.  crhold() expects a reference count
 * of at least 1.
 *
 * Revision 65.5  1998/03/23 16:43:26  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.4  1998/03/06 00:09:14  bdr
 * Deleted extra function prototypes that we did not need and were ifdef'ed out.
 *
 * Revision 65.3  1998/03/02  22:32:35  bdr
 * Changed calls to crget to crdup.  Modified various cm syscalls to be
 * calling the proper function inside of cm.
 *
 * Revision 65.2  1997/11/06  20:01:04  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/21  20:05:20  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:50  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:48:35  dce
 * *** empty log message ***
 *
 * Revision 1.2  1996/12/24  19:11:31  vrk
 * PV Incidence - 447140. use afs_xsetgroups() to preserve DFS PAG values across
 * setgroups() call.
 *
 * Revision 1.1  1996/04/16  18:32:32  vrk
 * Initial revision
 *
 * Revision 1.1  1994/09/12  16:59:27  dcebuild
 * Original File from OSF
 *
 * Revision 1.1.95.4  1994/09/12  16:59:26  maunsell_c
 * 	fix extern for *dfs_translate_creds() in dfs_client.ext build
 * 	[1994/09/12  16:58:57  maunsell_c]
 *
 * Revision 1.1.95.3  1994/08/08  18:54:47  mckeen
 * 	Init dfs_translate_creds hook
 * 	[1994/08/08  17:52:53  mckeen]
 * 
 * Revision 1.1.95.2  1994/06/09  14:12:34  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:25:55  annie]
 * 
 * Revision 1.1.95.1  1994/02/04  20:22:01  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:14:37  devsrc]
 * 
 * Revision 1.1.93.1  1993/12/07  17:27:35  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  15:43:58  jaffe]
 * 
 * Revision 1.1.6.3  1993/07/28  17:38:41  mbs
 * 	Added afscall_cm_newtgt into the afs_sysent table.
 * 	[1993/07/27  15:22:49  mbs]
 * 
 * Revision 1.1.6.2  1993/07/19  19:31:56  zeliff
 * 	HP port of DFS
 * 	[1993/07/19  18:24:18  zeliff]
 * 
 * Revision 1.1.4.5  1993/07/16  19:50:47  kissel
 * 	Purge more nfs exporter stuff
 * 	[1993/07/02  19:53:29  tmm]
 * 
 * 	Merge gamera fixes:
 * 	  - Fill in xdfs_vn_rele_op, xglue_cm_dcache_delete_op upon config.
 * 	[1993/06/29  21:23:03  tmm]
 * 
 * 	GAMERA Merge.
 * 	[1993/04/09  20:23:36  toml]
 * 
 * 	*** empty log message ***
 * 	[1993/06/21  14:30:43  kissel]
 * 
 * 	KLOAD changes.
 * 	Add in icl_printf.  Doesn't really belong here though.
 * 	[1993/03/15  16:09:40  toml]
 * 
 * 	afs -> dcedfs.
 * 	[1993/01/14  19:42:17  toml]
 * 
 * 	Init ptr to cm's global cred structure
 * 	[1992/11/23  19:53:30  kinney]
 * 
 * 	Initial revision for HPUX.  Remove references to afs_xioctl() for DFS1.0.2.  Support UX9.0.
 * 	[1992/10/14  16:42:17  toml]
 * 
 * 	remove unnecessary conditionals
 * 	add missing getpag entry
 * 	[1992/05/22  03:10:52  garyf]
 * 
 * 	separated out OSF/1 code
 * 	[1992/04/07  15:44:43  garyf]
 * 
 * Revision 1.1.2.2  1993/06/04  14:52:27  kissel
 * 	Initial HPUX RP version.  Removed AFS_OSF_ENV dependencies since this is a HPUX specific file.
 * 	[1993/06/03  21:39:46  kissel]
 * 
 * $EndLog$
*/
#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>			/* Standard vendor system headers */
#include <dcedfs/osi_sysconfig.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/systm.h>
#include <sys/syscall.h>
#include <ksys/cred.h>
#include <dcedfs/syscall.h>

#include <dcedfs/cm_trace.h>
#include <dcedfs/icl.h>

/* 
 * Declare all the AFS system calls we'll need to hook up. 
 */

extern int afscall_cm(), afscall_cm_pioctl(), afscall_setpag(),
	   afs_nosys(), afscall_getpag(), afscall_cm_newtgt();

  /*
   * Declare all the system calls we'll need to hook up.
   */
extern int
      afs_xflock(),     flock(),
      afs_xsetgroups(), setgroups(), 
      (*setgroupsp)(int, gid_t *);

/*
 * Structure of the system-entry table
 */
extern int nsysent;

extern struct vfsops afs_vfsops;

/* VRK - for us the declaration will get included from osi_cred.h 
 *
struct ucred *osi_credp;
*/

extern int (*xdfs_vn_rele_op)();
extern int (*xglue_cm_dcache_delete_op)();
extern int xglue_vn_rele();

/*
 * Function pointer provided for alien file systems to 
 * translate credentials based on user/network ID.
 *
 * storage for this function pointer is provided in hp-ux, nfs_server.c
 *
 * Why is this here?  It doesn't appear to be used.
 */
/* should be int not void, dfs_translate_creds is in nfs_server.c for HPUX */
/* and is init here with at_translate_creds in cm_post_config */
#ifndef AFS_IRIX_ENV
extern int (*dfs_translate_creds)();
#endif /* ! AFS_IRIX_ENV */

/*
 * cm_dcache_delete - Remove a vnode from the cm dnamecache.
 *
 * The DFS cm dnamecache is a cache of tuples <dir, file>
 * that speeds up lookups.  We clean this cache by removing
 * tuples whose directory vnode corresponds to the vnode we
 * are flushing.
 */
int cm_dcache_delete(vp)
register struct vnode *vp;
{
      extern int nh_delete_dvp(struct vnode *);

      if (vp->v_type != VDIR) {
              return(EINVAL);
      } else {
              return(nh_delete_dvp(vp));
      }
}

/*
 * cm_post_config -- Initialization postponed until CM is ready to roll.
 */
int
cm_post_config()
{
#if 0 /* VRK - SGIMIPS commented off for time being.... */
      xdfs_vn_rele_op = xglue_vn_rele;
      xglue_cm_dcache_delete_op = cm_dcache_delete;

         /* link cred translator mechanism */
      dfs_translate_creds = at_translate_creds;
      at_configure();
#endif /* 0 - VRK - SGIMIPS */

      return 0;
}

int
cm_configure(op)
int op;
{
	int	ret;
	extern  int set_afssyscalls(int);
	osi_cred_t *credp;

	switch (op) {
		case KLOAD_INIT_CONFIG:
/* VRK - revisit latter.....
			vfssw_assign(ret, afs_vfsops); */
			break;

		case KLOAD_INIT_UNCONFIG:
			return EBUSY;
		default:
			return EINVAL;
	}
	set_afssyscalls(op);

        /*
         * Init the pointer to the CM's global cred structure.
         */

	credp = pcred_access(osi_curproc());
        osi_credp = crdup(credp);
	pcred_unaccess(osi_curproc());
        bzero(osi_credp, sizeof(osi_cred_t));
	osi_credp->cr_ref = 1;

	return 0;
}


set_afssyscalls(op)
	int op;
{

#define SystemCall(index,proc) (((index) < nsysent) ? (sysent[(index)].sy_call = (proc)) : 0)
#define afson (op == KLOAD_INIT_CONFIG)
	
	AfsCall (AFSCALL_CM,            afson ? afscall_cm        : afs_nosys);
	AfsCall (AFSCALL_PIOCTL,        afson ? afscall_cm_pioctl : afs_nosys);
	AfsCall (AFSCALL_SETPAG,        afson ? afscall_setpag : afs_nosys);
	AfsCall (AFSCALL_GETPAG,        afson ? afscall_getpag : afs_nosys);
        AfsCall (AFSCALL_NEWTGT,        afson ? afscall_cm_newtgt : afs_nosys);

       /*
	* Stuff in the DFS setgroups wrapper to preserve PAG's across
        * a setgroups system call.
        */
	if (afson)
		setgroupsp = afs_xsetgroups;
	
	return 0;

}

dfs_client_config(op)
int op;
{
	extern int cm_configure(int);

	return(cm_configure(op));
}

