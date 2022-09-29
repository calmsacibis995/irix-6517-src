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
#ifndef _FS_PROCFS_PROCFS_PRIVATE_H
#define _FS_PROCFS_PROCFS_PRIVATE_H

#ident	"$Revision: 1.5 $"

#include <ksys/behavior.h>
#include <sys/kmem.h>
#include <sys/vnode.h>
#include <sys/vfs.h>

/*
 * The procfs_info structure represents a global set of data for an fdfs file 
 * system, established on the server cell for the file system and propagated to 
 * all remote instances.
 */
typedef struct {
	dev_t		info_dev;       /* device to associate with mount */
} procfs_info_t;

/*
 * Define a macro to see if we are on the special procfs cell
 */
#if     CELL_IRIX
#include <ksys/cell.h>
#if     CFS_TEST
#define PROCFS_CELL             (1)
#else   /* CFS_TEST */
#define PROCFS_CELL             (golden_cell)
#endif  /* CFS_TEST */
#define PROCFS_CELL_HERE()	(cellid() == PROCFS_CELL)
#else   /* CELL */
#define PROCFS_CELL_HERE()	(1)
#endif  /* CELL */

extern int		procfs_type;
extern zone_t           *procfs_trzone;
extern procfs_info_t    procfs_info;
extern vfs_t            *procfs_vfs;
extern lock_t           procfs_lock;
extern struct prnode    *procfs_freelist;
extern int              procfs_nfree;
extern struct prnode    procfs_root;
extern struct prnode    procfs_pinfo;
extern struct bhv_desc  procfs_bhv;
extern int              procfs_refcnt;
			
/*
 * Global structures
 */
extern vnodeops_t      prvnodeops;

/*
 * Prototypes of functions in prvfsops.c, needed in prvfsops_cell.c
 */
extern int procfs_newvfs(vfs_t *);
extern void procfs_initvfs(vfs_t *);
extern void procfs_initnodes(vfs_t *);
extern int procfs_umcheck(vfs_t *);
extern void procfs_umfinal(vfs_t *, bhv_desc_t *);
extern int prroot(bhv_desc_t *, struct vnode **);
extern int prstatvfs(bhv_desc_t *, struct statvfs *, struct vnode *);
#if     CELL_IRIX
extern prnode_t * prnode_alloc(void);
extern void prnode_free(prnode_t *);
#endif	/* CELL_IRIX */

/*
 * Prototypes of functions in prvfsops_cell.c, needed in prvfsops.c
 */
extern int procfs_realvfsops(vfs_t *, struct mounta *, vfsops_t **);
extern int procfs_import(vfs_t *);
extern void procfs_cellmount(vfs_t *, bhv_desc_t *);


#endif  /* _FS_PROCFS_PROCFS_PRIVATE_H */

