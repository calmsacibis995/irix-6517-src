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

#ident	"$Revision: 1.33 $"

#include <sys/types.h>
#include <sys/capability.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/fs_subr.h>
#include <sys/hwgraph.h>
#include <sys/immu.h>
#include <sys/iograph.h>
#include <sys/kmem.h>
#include <sys/systm.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/vfs.h>
#include <sys/vnode.h>
#include <fs/specfs/spec_lsnode.h>
#include "hwgnode.h"
#include <ksys/hwg.h>

/*
** hardware graph pseudo-filesystem support routines.
*/

mrlock_t hwgfs_node_mrlock;
static vertex_hdl_t devhdl;
static int hwgfs_devhdl(vertex_hdl_t src, char **remainder, vertex_hdl_t *vhdlp);
static struct zone *hwgfs_perm_zone = (struct zone *)NULL;

#define HWGRAPH_DEVHDL ".devhdl"

/*
 * Permissions on a hwgfs file.  The default permissions, can be
 * overridden by storing INFO_LBL_PERMISSIONS label at the vertex.
 *
 * In order to get persistent permissions, it's generally preferable 
 * to create a permanent special file in a regular file system with 
 * the appropriate locator string and appropriate permissions.
 */
typedef struct hwgfs_perm_s {
	mode_t	hp_mode;
	uid_t	hp_uid;
	gid_t	hp_gid;
} *hwgfs_perm_t;

#define HWG_MODE_DEFAULT(type)	((type == VDIR) ? 0755 : 0600)
#define HWG_UID_DEFAULT		0
#define HWG_GID_DEFAULT		0


/* 
 * Locking just for changing permissions on hwgfs files.  Need a way to lock
 * against someone removing the vertex from the driver (hwgraph) side while
 * someone else simultaneously tries to change permissions.  One multi-reader
 * lock is probably adequate.  If not, we can always break this up later.
 */
static mrlock_t hwgfs_perm_mrlock;
#define HWGFS_PERM_INIT()		mrlock_init(&hwgfs_perm_mrlock, MRLOCK_BARRIER, "hwgfs_perm", 0)
#define HWGFS_PERM_ACCESS()		{mraccess(&hwgfs_perm_mrlock); s=s;}
#define HWGFS_PERM_UPDATE()		{mrupdate(&hwgfs_perm_mrlock); s=s;}
#define HWGFS_PERM_ACCESS_DONE()	{mraccunlock(&hwgfs_perm_mrlock); s=s;}
#define HWGFS_PERM_UPDATE_DONE()	{mrunlock(&hwgfs_perm_mrlock); s=s;}
#define HWGFS_PERM_FREE()		mrfree(&hwgfs_perm_mrlock)

/*
** hwginit Initializes the hardware graph file system.
*/
/* ARGSUSED */
void
hwginit(struct vfssw *vswp, int fstype)
{
	dev_t dev;
	graph_error_t rc;

	hwgfstype = fstype;
	ASSERT(hwgfstype != 0);

	vswp->vsw_vfsops = &hwgvfsops;
	vswp->vsw_vnodeops = &hwgvnodeops;

	dev = getudev();
	if (dev == -1) {
		cmn_err(CE_WARN, "hwginit: can't get unique device number");
		dev = 0;
	}

	hwgdev = makedevice(dev, 0);

	mrinit(&hwgfs_node_mrlock, "hwgfs");

	/* Create the .devhdl directory */
	rc = hwgraph_path_add(hwgraph_root, HWGRAPH_DEVHDL, &devhdl);
	ASSERT(rc == GRAPH_SUCCESS); rc = rc;

	rc = hwgraph_traverse_set(devhdl, hwgfs_devhdl);
	ASSERT(rc == GRAPH_SUCCESS); rc = rc;
}


/*
** hwgfs_earlyinit performs early initializations that are useful before
** file systems (esp hwgfs) are initialized.  For instance, some device
** drivers want to use hwgfs_chmod to set permissions on devices.  They
** want to do this before the file systems have been initialized; so we
** perform the small amount of setup required here.  hwgfs_earlyinit is
** called exactly once from hwgraph_init before we get into device driver
** code.  This is an odd interface that results from hwgfs living on the
** border between file systems and device drivers.
*/
/* ARGSUSED */
void
hwgfs_earlyinit(void)
{
	HWGFS_PERM_INIT();

	hwgfs_perm_zone = kmem_zone_init(sizeof(struct hwgfs_perm_s),
						 "hwgfs perm");
}


/*
 * Support for /hw/.devhdl.  This directory is indented to make it easy to
 * translate from a dev_t to a file system name.  It is NOT intended to
 * provide alternate paths to devices for other purposes.  The only intended
 * user of this facility is a system-supplied library routine.
 *
 * This routine is registered as a traverse routine for the /hw/.devhdl
 * vertex.  Returns 1 on success, 0 on failure.
 */
/* ARGSUSED */
static int
hwgfs_devhdl(vertex_hdl_t src, char **remainder, vertex_hdl_t *vhdlp)
{
	vertex_hdl_t vhdl;

	ASSERT(src == devhdl);

	vhdl = (vertex_hdl_t)atoi(*remainder); /* Sigh.  Decimal only. :-( */

	if ((vhdl == 0) || ((hwgraph_vertex_ref(vhdl) != GRAPH_SUCCESS)))
		return(0);

	*remainder = NULL;
	*vhdlp = vhdl;

	return(1);
}


/* Total number of hwgnodes in use (for debug only)*/
static int hwgnode_count = 0;	

/*
** hwgfs_allocnode allocates a hardware graph file system node
** and links it onto a linked list of such nodes associated
** with a vertex in the hardware graph.
*/
hwgnode_t *
hwgfs_allocnode(vertex_hdl_t vertex, vtype_t vtype, hwgmount_t *hwgmount)
{
	struct vnode *vp;
	hwgnode_t *hwgnode;
	hwgnode_t *hwglist;
	graph_error_t rv;

	ASSERT(vertex != GRAPH_VERTEX_NONE);
	hwgnode = kmem_alloc(sizeof(hwgnode_t), KM_SLEEP);

	vp = vn_alloc(VFSBHV_TO_VFS(&hwgmount->hwgmnt_vfsbhv), vtype, vhdl_to_dev(vertex));
	bhv_desc_init(HWG_TO_VNODEBHV(hwgnode), hwgnode, vp, &hwgvnodeops);
	vn_bhv_insert_initial(VN_BHV_HEAD(vp), HWG_TO_VNODEBHV(hwgnode));

	hwgnode->hwg_mount = hwgmount;
	hwgnode->hwg_vertex = vertex;
	hwgnode->hwg_valid = 1;

	mrupdate(&hwgfs_node_mrlock);
	hwgnode_count++;
	hwgmount->hwgmnt_count++;
	hwgnode->hwg_next_on_fs = hwgmount->hwgmnt_head;
	hwgnode->hwg_prev_on_fs = NULL;
	if (hwgmount->hwgmnt_head)
		hwgmount->hwgmnt_head->hwg_prev_on_fs = hwgnode;
	hwgmount->hwgmnt_head = hwgnode;

again:
	rv = hwgraph_info_get_LBL(vertex, INFO_LBL_HWGFSLIST, (arbitrary_info_t *)&hwglist);
	if (rv == GRAPH_SUCCESS) {
		/*
		 * Insert new node.  It's easiest to just stick this new node
		 * near the start of the list.  Order is unimportant.
		 */
		ASSERT(hwglist != NULL);
		hwgnode->hwg_next_on_vertex = hwglist->hwg_next_on_vertex;
		hwgnode->hwg_prev_on_vertex = hwglist;
		hwglist->hwg_next_on_vertex = hwgnode;
		if (hwgnode->hwg_next_on_vertex)
			hwgnode->hwg_next_on_vertex->hwg_prev_on_vertex = hwgnode;
		mrunlock(&hwgfs_node_mrlock);
	} else {
		mrunlock(&hwgfs_node_mrlock);
		/* Start a new node list on this vertex. */
		hwgnode->hwg_next_on_vertex = NULL;
		hwgnode->hwg_prev_on_vertex = NULL;
		rv = hwgraph_info_add_LBL(vertex, INFO_LBL_HWGFSLIST, (arbitrary_info_t)hwgnode);
		if (rv == GRAPH_DUP) {
			mrupdate(&hwgfs_node_mrlock);
			goto again;
		}
	}

	(void)hwgraph_vertex_ref(vertex);

	return(hwgnode);
}


/*
** hwgfs_freenode deallocates a hardware graph file system node after
** delinking it from two lists: the associated vertex' linked list and
** the per-mount file system's linked list.
*/
void
hwgfs_freenode(bhv_desc_t *bdp)
{
	hwgnode_t *hwgp = VNODE_TO_HWG(bdp);
	hwgmount_t *hwgmount = hwgp->hwg_mount;
	vertex_hdl_t vertex = hwgp->hwg_vertex;
	int was_first, was_last;
	graph_error_t rv;

	mrupdate(&hwgfs_node_mrlock);
	ASSERT_ALWAYS(hwgp->hwg_valid == 1);
	hwgp->hwg_valid = 0;

	/*
	 * Pull this hwgnode off the vertex list.
	 */
	if (hwgp->hwg_next_on_vertex) {
		was_last = 0;
		hwgp->hwg_next_on_vertex->hwg_prev_on_vertex = hwgp->hwg_prev_on_vertex;
	} else
		was_last = 1;

	if (hwgp->hwg_prev_on_vertex) {
		was_first = 0;
		hwgp->hwg_prev_on_vertex->hwg_next_on_vertex = hwgp->hwg_next_on_vertex;
	} else
		was_first = 1;

	/*
	 * If we were first on the list, we need to adjust the labelled
	 * information hanging off the vertex.  Either remove it entirely
	 * (if we were first and last) OR replace it with the next node
	 * on the list (if we were first, but not last).
	 */
	if (was_first) {
		if (was_last)
			rv = hwgraph_info_remove_LBL(vertex, INFO_LBL_HWGFSLIST, NULL);
		else
			rv = hwgraph_info_replace_LBL(vertex, INFO_LBL_HWGFSLIST, 
					(arbitrary_info_t)hwgp->hwg_next_on_vertex, NULL);
		ASSERT(rv == GRAPH_SUCCESS); rv = rv;
	}

	hwgp->hwg_next_on_vertex = NULL; /* sanity */
	hwgp->hwg_prev_on_vertex = NULL; /* sanity */


	/*
	 * Pull this hwgnode off the file system list.
	 */
	if (hwgp->hwg_next_on_fs)
		hwgp->hwg_next_on_fs->hwg_prev_on_fs = hwgp->hwg_prev_on_fs;

	if (hwgp->hwg_prev_on_fs)
		hwgp->hwg_prev_on_fs->hwg_next_on_fs = hwgp->hwg_next_on_fs;
	else
		hwgmount->hwgmnt_head = hwgp->hwg_next_on_fs;

	/* Make sure an hwgsync in progress doesn't get lost */
	if (hwgmount->hwgmnt_cursor == hwgp)
		hwgmount->hwgmnt_cursor = hwgp->hwg_next_on_fs;
	
	hwgp->hwg_next_on_fs = NULL; /* sanity */
	hwgp->hwg_prev_on_fs = NULL; /* sanity */


	/* Adjust global count of outstanding hwgnodes */
	ASSERT(hwgnode_count > 0);
	hwgnode_count--;

	/* Adjust per-mounted file system count of outstanding hwgnodes */
	ASSERT(hwgmount->hwgmnt_count > 0);
	hwgmount->hwgmnt_count--;

	vn_bhv_remove(VN_BHV_HEAD(BHV_TO_VNODE(bdp)), bdp);

	mrunlock(&hwgfs_node_mrlock);

	kmem_free(hwgp, sizeof(hwgnode_t));
	(void)hwgraph_vertex_unref(vertex);
}

/*
** hwgfs_getnode searches for a hardware graph file system node that represents
** a given vertex handle and type (VCHR, VBLK, VDIR).  If not found, allocate
** the appropriate type node.
*/
struct vnode *
hwgfs_getnode(vertex_hdl_t vertex, vtype_t type, hwgmount_t *hwgmount, struct cred *cr)
{
	hwgnode_t *hwgp;
	struct vnode *vp = NULL;
	graph_error_t rv;

	ASSERT(vertex != GRAPH_VERTEX_NONE);
again:
	hwgp = NULL;
	mraccess(&hwgfs_node_mrlock);
	rv = hwgraph_info_get_LBL(vertex, INFO_LBL_HWGFSLIST, (arbitrary_info_t *)&hwgp);
	if (rv == GRAPH_SUCCESS) {
		for ( ; hwgp != NULL; hwgp=hwgp->hwg_next_on_vertex) {
			ASSERT(hwgp->hwg_valid);
			ASSERT(hwgp->hwg_vertex == vertex);

			vp = HWG_TO_VNODE(hwgp);
			if ((hwgp->hwg_mount == hwgmount) && (vp->v_type == type))
				break;
		}
	}

	/*
	 * Did we find a match?  If so, return the corresponding vnode.
	 * If not, create one and link it in to the vertex' list.  It's
	 * possible that multiple processes executing this code will
	 * allocate multiple vnodes with multiple hwgnodes for the same
	 * (vertex,type), but it doesn't really matter.
	 */
	if (hwgp == NULL) {
		mraccunlock(&hwgfs_node_mrlock);
		hwgp = hwgfs_allocnode(vertex, type, hwgmount);
		vp = HWG_TO_VNODE(hwgp);
	} else {
		vmap_t vmap;

		VMAP(vp, vmap);
		mraccunlock(&hwgfs_node_mrlock);
		if (!(vp = vn_get(vp, &vmap, 0)))
			goto again;
	}

	ASSERT(hwgp->hwg_valid == 1);

	if ((type == VCHR) || (type == VBLK)) {
		struct vnode *newvp;

		newvp = spec_vp(vp, vhdl_to_dev(vertex), type, cr);

		VN_RELE(vp);
		return(newvp);
	}

	return(vp);
}


/*
 * Manage permissions on a hwgfs file.  Permissions (mode, uid, gid) are stored
 * at a vertex under labelled information.
 * The INFO_LBL_PERMISSIONS label at a vertex indicates:
 *	no label--> default permissions
 *	ptr	--> pointer to structure with permissions
 *
 * This is useful only in limited circumstances.  For persistent permission
 * changes, it's preferable to create an actual special file via mknod that
 * references the desired device; then put persistent permissions on that
 * special file.
 */

void
hwgfs_get_permissions(	vertex_hdl_t vhdl, 
			enum vtype type, 
			mode_t *modep, 
			uid_t *uidp, 
			gid_t *gidp)
{
	hwgfs_perm_t permissions;
	graph_error_t rc;
	int s;
	

	HWGFS_PERM_ACCESS();
	rc = hwgraph_info_get_LBL(	vhdl, 
					INFO_LBL_PERMISSIONS, 
					(arbitrary_info_t *)&permissions);

	if (rc == GRAPH_SUCCESS) {
		ASSERT(permissions != NULL);
		*modep = permissions->hp_mode;
		*uidp = permissions->hp_uid;
		*gidp = permissions->hp_gid;
	} else {
		*modep = HWG_MODE_DEFAULT(type);
		*uidp = HWG_UID_DEFAULT;
		*gidp = HWG_GID_DEFAULT;
	}
	HWGFS_PERM_ACCESS_DONE();
}

void
hwgfs_set_permissions(	vertex_hdl_t vhdl, 
			enum vtype type, 
			mode_t mode, 
			uid_t uid, 
			gid_t gid)
{
	hwgfs_perm_t old_perm, new_perm;
	graph_error_t rc;
	int s;

	old_perm = NULL;

	/*
	 * If we're setting permissions back to the default, free
	 * the permissions structure.  If we're setting permissions
	 * to something other than the default, add permission info
	 * to the graph (or replace & free the old permissions).
	 */
	if (	(type != VNON) && (mode == HWG_MODE_DEFAULT(type)) &&
		(uid == HWG_UID_DEFAULT) &&
		(gid == HWG_GID_DEFAULT)) {

		HWGFS_PERM_UPDATE();
		(void)hwgraph_info_get_LBL(vhdl, 
					INFO_LBL_PERMISSIONS, 
					(arbitrary_info_t *)&old_perm);
		if (old_perm)
			(void)hwgraph_info_remove_LBL(vhdl,
					INFO_LBL_PERMISSIONS,
					(arbitrary_info_t *)&old_perm);
	} else {
 		new_perm = (hwgfs_perm_t)kmem_zone_alloc(hwgfs_perm_zone, KM_SLEEP);
		ASSERT(new_perm);

		new_perm->hp_mode = mode;
		new_perm->hp_uid = uid;
		new_perm->hp_gid = gid;

		HWGFS_PERM_UPDATE();

		rc = hwgraph_info_get_LBL(vhdl, 
					INFO_LBL_PERMISSIONS, 
					(arbitrary_info_t *)&old_perm);

		if (rc == GRAPH_SUCCESS) {
			rc = hwgraph_info_replace_LBL(vhdl,
				INFO_LBL_PERMISSIONS,
				(arbitrary_info_t)new_perm,
				(arbitrary_info_t *)&old_perm);
		} else {
			ASSERT(rc == GRAPH_NOT_FOUND);
			rc = hwgraph_info_add_LBL(vhdl,
					INFO_LBL_PERMISSIONS,
					(arbitrary_info_t)new_perm);
			if (rc != GRAPH_SUCCESS)
				kmem_zone_free(hwgfs_perm_zone, new_perm);
		}
	}

	HWGFS_PERM_UPDATE_DONE();

	if (old_perm)
		kmem_zone_free(hwgfs_perm_zone, old_perm);
}


void
hwgfs_chmod(vertex_hdl_t vhdl, mode_t new_mode)
{
	mode_t old_mode;
	uid_t uid;
	gid_t gid;

	hwgfs_get_permissions(vhdl, VNON, &old_mode, &uid, &gid);
	hwgfs_set_permissions(vhdl, VNON, new_mode, uid, gid);
}


/*
 * Called by hwgraph when a vertex is freed.  This gives us an opportunity
 * to clean up any state that we may have added to this vertex.  In theory,
 * by the time hwgfs_vertex_destroy is called there should be no way for
 * anyone to open the hwgfs file corresponding to vhdl -- edges leading to
 * vhdl will have already been removed.
 */
void 
hwgfs_vertex_destroy(vertex_hdl_t vhdl)
{
	hwgfs_perm_t old_perm = NULL;
	graph_error_t rc;
	int s;

	HWGFS_PERM_UPDATE();
	rc = hwgraph_info_get_LBL(vhdl, 
				INFO_LBL_PERMISSIONS, 
				(arbitrary_info_t *)&old_perm);

	if (rc == GRAPH_SUCCESS) {
		ASSERT(old_perm != NULL);
		rc = hwgraph_info_replace_LBL(vhdl,
				INFO_LBL_PERMISSIONS,
				(arbitrary_info_t)NULL,
				(arbitrary_info_t *)NULL);
	}
	HWGFS_PERM_UPDATE_DONE();

	if (old_perm)
		kmem_zone_free(hwgfs_perm_zone, old_perm);
}


/*
 * Called by hwgraph when a vertex is cloned.  This gives us an opportunity
 * to clean up any hwgfs state (e.g. permissions) that shouldn't be cloned.  
 */
void
hwgfs_vertex_clone(vertex_hdl_t vhdl)
{
        hwgraph_info_remove_LBL(vhdl, INFO_LBL_PERMISSIONS, NULL);
}

