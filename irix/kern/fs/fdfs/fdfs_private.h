/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef _FS_FDFS_FDFS_PRIVATE_H
#define _FS_FDFS_FDFS_PRIVATE_H

#ident	"$Revision: 1.4 $"

#include <ksys/behavior.h>
#include <sys/kmem.h>
#include <sys/vnode.h>
#include <sys/vfs.h>

/*
 * The fdfs_info structure represents a global set of data for an fdfs file 
 * system, established at mount time on the server cell for the file system
 * and propagated to all remote instances.
 */
typedef struct fdfs_info {
	dev_t		info_dev;       /* device to associate with mount */
        int             info_rdev;      /* Major number to be used for the rdev
                                           for each of the vnodes */
} fdfs_info_t;

/*
 * The fdfs_mount structure is used to represent the base file system behavior
 * and associated data on whichever cell hosts an fdfs file system.  It is also
 * used on other cells to support local processing of fdfs requests.  In this
 * case it is an interposer.
 */
typedef struct fdfs_mount {
	bhv_desc_t	mount_bhv;	/* behavior, either base fs or inter-
                                           poser, for fdfs functions */
        fdfs_info_t     mount_info;     /* global information common to all
                                           instances of the same fdfs file
                                           system. */
        struct vnode    *mount_root;    /* local root vnode */
        int             mount_refcnt;   /* extra reference count to reflect non-
                                           root vnodes */
} fdfs_mount_t;

/*
 * The fdfs_node structure is used for the base behavior of all fdfs vnodes,
 * whether on the cell hosting the fdfs file system or not.
 */
typedef struct fdfs_node {
	bhv_desc_t	node_bhv;	/* base vnode behavior */
        fdfs_mount_t    *node_mount;    /* associated fdfs mount structure */
} fdfs_node_t;

/*
 * Translate between behaviors and fdfs structures
 */
#define FDFS_BHVTOM(bdp)	((fdfs_mount_t *) BHV_PDATA(bdp))
#define FDFS_BHVTON(bdp)	((fdfs_node_t *) BHV_PDATA(bdp))
#define FDFS_MTOBHV(fmp)        (&(fmp)->mount_bhv)
#define FDFS_NTOBHV(fmp)        (&(fnp)->node_bhv)

/*
 * Global data
 */
extern int		fdfstype;
extern zone_t           *fdfszone;

/*
 * Prototypes of functions in fdops_cell.c, needed in fdops.c
 */
extern int fdfs_realvfsops(vfs_t *, struct mounta *, vfsops_t **);
extern int fdfs_import(vfs_t *);

/*
 * Prototypes of functions in fdops.c, needed in fdops_cell.c
 */
extern void fdfs_newroot(vfs_t *, fdfs_mount_t *);
extern int fdfs_root(bhv_desc_t *, struct vnode **);
extern int fdfs_statvfs(bhv_desc_t *, struct statvfs *, struct vnode *);

#endif  /* _FS_FDFS_FDFS_PRIVATE_H */
