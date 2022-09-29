/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.6 $"


#include <sys/types.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/fsid.h>
#include <sys/fstyp.h>
#include <sys/proc.h>
#include <sys/cred.h>

#include "dmi_private.h"


typedef	struct	{
	int	support_type;
	char	name[FSTYPSZ];
	dm_fsys_vector_t	*vptr;
} dm_vector_map_t;

/* Values for the support_type field. */

#define	DM_SUPPORT_UNKNOWN	0
#define	DM_SUPPORT_AVAIL	1


dm_vector_map_t	*dm_fsys_map = NULL;


int
dm_code_level(void)
{
	return(DM_CLVL_XOPEN);	/* initial X/Open compliant release */
}


/* Dummy routine which is stored in each function vector slot for which the
   filesystem provides no function of its own.  If an application calls the
   function, he will just get ENOSYS.
*/

static int
dm_enosys(void)
{
	return(ENOSYS);		/* function not supported by filesystem */
}


/* dm_query_fsys_for_vector() asks a filesystem for its list of supported
   DMAPI functions, and builds a dm_vector_map_t structure based upon the
   reply.  We ignore functions supported by the filesystem which we do not
   know about, and we substitute the subroutine 'dm_enosys' for each function
   we know about but the filesystem does not support.
*/

static void
dm_query_fsys_for_vector(
	bhv_desc_t	*bdp)
{
	dm_vector_map_t	*map;
	fsys_function_vector_t	*vecp;
	dm_fsys_vector_t  *vptr;
	dm_fcntl_t	dmfcntl;
	vnode_t		*vp = BHV_TO_VNODE(bdp);
	struct vfs      *vfsp = vp->v_vfsp;
	int		fstype;
	int		error;
	int		i;

	fstype = vfsp->vfs_fstype;
	map = &dm_fsys_map[fstype];

	/* Clear out any information left from a previous filesystem that was
	   in this slot and initialize it for the new filesystem.
	*/

	if (map->vptr) {
		kmem_free(map->vptr, sizeof(*map->vptr));
		map->vptr = NULL;
	}
	strcpy(map->name, vfssw[fstype].vsw_name);
	map->support_type = DM_SUPPORT_AVAIL;

	/* Next allocate a function vector and initialize all fields with a
	   dummy function that returns ENOSYS.
	*/

	vptr = map->vptr = kmem_alloc(sizeof(*map->vptr), KM_SLEEP);

	strncpy(vptr->fsys_name, map->name, sizeof(vptr->fsys_name));
	vptr->code_level = 0;
	vptr->clear_inherit = (dm_fsys_clear_inherit_t)dm_enosys;
	vptr->create_by_handle = (dm_fsys_create_by_handle_t)dm_enosys;
	vptr->downgrade_right = (dm_fsys_downgrade_right_t)dm_enosys;
	vptr->get_allocinfo_rvp = (dm_fsys_get_allocinfo_rvp_t)dm_enosys;
	vptr->get_bulkall_rvp = (dm_fsys_get_bulkall_rvp_t)dm_enosys;
	vptr->get_bulkattr_rvp = (dm_fsys_get_bulkattr_rvp_t)dm_enosys;
	vptr->get_config = (dm_fsys_get_config_t)dm_enosys;
	vptr->get_config_events = (dm_fsys_get_config_events_t)dm_enosys;
	vptr->get_destroy_dmattr = (dm_fsys_get_destroy_dmattr_t)dm_enosys;
	vptr->get_dioinfo = (dm_fsys_get_dioinfo_t)dm_enosys;
	vptr->get_dirattrs_rvp = (dm_fsys_get_dirattrs_rvp_t)dm_enosys;
	vptr->get_dmattr = (dm_fsys_get_dmattr_t)dm_enosys;
	vptr->get_eventlist = (dm_fsys_get_eventlist_t)dm_enosys;
	vptr->get_fileattr = (dm_fsys_get_fileattr_t)dm_enosys;
	vptr->get_region = (dm_fsys_get_region_t)dm_enosys;
	vptr->getall_dmattr = (dm_fsys_getall_dmattr_t)dm_enosys;
	vptr->getall_inherit = (dm_fsys_getall_inherit_t)dm_enosys;
	vptr->init_attrloc = (dm_fsys_init_attrloc_t)dm_enosys;
	vptr->mkdir_by_handle = (dm_fsys_mkdir_by_handle_t)dm_enosys;
	vptr->probe_hole = (dm_fsys_probe_hole_t)dm_enosys;
	vptr->punch_hole = (dm_fsys_punch_hole_t)dm_enosys;
	vptr->read_invis_rvp = (dm_fsys_read_invis_rvp_t)dm_enosys;
	vptr->release_right = (dm_fsys_release_right_t)dm_enosys;
	vptr->request_right = (dm_fsys_request_right_t)dm_enosys;
	vptr->remove_dmattr = (dm_fsys_remove_dmattr_t)dm_enosys;
	vptr->set_dmattr = (dm_fsys_set_dmattr_t)dm_enosys;
	vptr->set_eventlist = (dm_fsys_set_eventlist_t)dm_enosys;
	vptr->set_fileattr = (dm_fsys_set_fileattr_t)dm_enosys;
	vptr->set_inherit = (dm_fsys_set_inherit_t)dm_enosys;
	vptr->set_region = (dm_fsys_set_region_t)dm_enosys;
	vptr->symlink_by_handle = (dm_fsys_symlink_by_handle_t)dm_enosys;
	vptr->sync_by_handle = (dm_fsys_sync_by_handle_t)dm_enosys;
	vptr->upgrade_right = (dm_fsys_upgrade_right_t)dm_enosys;
	vptr->write_invis_rvp = (dm_fsys_write_invis_rvp_t)dm_enosys;

	/* Issue a F_DMAPI fcntl() to the filesystem in order to obtain its
	   vector of filesystem-specific DMAPI routines.
	*/

	dmfcntl.dmfc_subfunc = DM_FCNTL_FSYSVECTOR;
	dmfcntl.u_fcntl.vecrq.count = 0;
	dmfcntl.u_fcntl.vecrq.vecp = NULL;

	VOP_FCNTL(vp, F_DMAPI, &dmfcntl, 0, 0, DM_GET_CRED, NULL, error);


	/* If we still have an error at this point, then the filesystem simply
	   does not support DMAPI, so we give up with all functions set to
	   ENOSYS.
	*/

	if (error || dmfcntl.u_fcntl.vecrq.count == 0)
		return;

	/* The request succeeded and we were given a vector which we need to
	   map to our current level.  Overlay the dummy function with every
	   filesystem function we understand.
	*/

	vptr->code_level = dmfcntl.u_fcntl.vecrq.code_level;
	vecp = dmfcntl.u_fcntl.vecrq.vecp;
	for (i = 0; i < dmfcntl.u_fcntl.vecrq.count; i++) {
		switch (vecp[i].func_no) {
		case DM_FSYS_CLEAR_INHERIT:
			vptr->clear_inherit = vecp[i].u_fc.clear_inherit;
			break;
		case DM_FSYS_CREATE_BY_HANDLE:
			vptr->create_by_handle = vecp[i].u_fc.create_by_handle;
			break;
		case DM_FSYS_DOWNGRADE_RIGHT:
			vptr->downgrade_right = vecp[i].u_fc.downgrade_right;
			break;
		case DM_FSYS_GET_ALLOCINFO_RVP:
			vptr->get_allocinfo_rvp = vecp[i].u_fc.get_allocinfo_rvp;
			break;
		case DM_FSYS_GET_BULKALL_RVP:
			vptr->get_bulkall_rvp = vecp[i].u_fc.get_bulkall_rvp;
			break;
		case DM_FSYS_GET_BULKATTR_RVP:
			vptr->get_bulkattr_rvp = vecp[i].u_fc.get_bulkattr_rvp;
			break;
		case DM_FSYS_GET_CONFIG:
			vptr->get_config = vecp[i].u_fc.get_config;
			break;
		case DM_FSYS_GET_CONFIG_EVENTS:
			vptr->get_config_events = vecp[i].u_fc.get_config_events;
			break;
		case DM_FSYS_GET_DESTROY_DMATTR:
			vptr->get_destroy_dmattr = vecp[i].u_fc.get_destroy_dmattr;
			break;
		case DM_FSYS_GET_DIOINFO:
			vptr->get_dioinfo = vecp[i].u_fc.get_dioinfo;
			break;
		case DM_FSYS_GET_DIRATTRS_RVP:
			vptr->get_dirattrs_rvp = vecp[i].u_fc.get_dirattrs_rvp;
			break;
		case DM_FSYS_GET_DMATTR:
			vptr->get_dmattr = vecp[i].u_fc.get_dmattr;
			break;
		case DM_FSYS_GET_EVENTLIST:
			vptr->get_eventlist = vecp[i].u_fc.get_eventlist;
			break;
		case DM_FSYS_GET_FILEATTR:
			vptr->get_fileattr = vecp[i].u_fc.get_fileattr;
			break;
		case DM_FSYS_GET_REGION:
			vptr->get_region = vecp[i].u_fc.get_region;
			break;
		case DM_FSYS_GETALL_DMATTR:
			vptr->getall_dmattr = vecp[i].u_fc.getall_dmattr;
			break;
		case DM_FSYS_GETALL_INHERIT:
			vptr->getall_inherit = vecp[i].u_fc.getall_inherit;
			break;
		case DM_FSYS_INIT_ATTRLOC:
			vptr->init_attrloc = vecp[i].u_fc.init_attrloc;
			break;
		case DM_FSYS_MKDIR_BY_HANDLE:
			vptr->mkdir_by_handle = vecp[i].u_fc.mkdir_by_handle;
			break;
		case DM_FSYS_PROBE_HOLE:
			vptr->probe_hole = vecp[i].u_fc.probe_hole;
			break;
		case DM_FSYS_PUNCH_HOLE:
			vptr->punch_hole = vecp[i].u_fc.punch_hole;
			break;
		case DM_FSYS_READ_INVIS_RVP:
			vptr->read_invis_rvp = vecp[i].u_fc.read_invis_rvp;
			break;
		case DM_FSYS_RELEASE_RIGHT:
			vptr->release_right = vecp[i].u_fc.release_right;
			break;
		case DM_FSYS_REMOVE_DMATTR:
			vptr->remove_dmattr = vecp[i].u_fc.remove_dmattr;
			break;
		case DM_FSYS_REQUEST_RIGHT:
			vptr->request_right = vecp[i].u_fc.request_right;
			break;
		case DM_FSYS_SET_DMATTR:
			vptr->set_dmattr = vecp[i].u_fc.set_dmattr;
			break;
		case DM_FSYS_SET_EVENTLIST:
			vptr->set_eventlist = vecp[i].u_fc.set_eventlist;
			break;
		case DM_FSYS_SET_FILEATTR:
			vptr->set_fileattr = vecp[i].u_fc.set_fileattr;
			break;
		case DM_FSYS_SET_INHERIT:
			vptr->set_inherit = vecp[i].u_fc.set_inherit;
			break;
		case DM_FSYS_SET_REGION:
			vptr->set_region = vecp[i].u_fc.set_region;
			break;
		case DM_FSYS_SYMLINK_BY_HANDLE:
			vptr->symlink_by_handle = vecp[i].u_fc.symlink_by_handle;
			break;
		case DM_FSYS_SYNC_BY_HANDLE:
			vptr->sync_by_handle = vecp[i].u_fc.sync_by_handle;
			break;
		case DM_FSYS_UPGRADE_RIGHT:
			vptr->upgrade_right = vecp[i].u_fc.upgrade_right;
			break;
		case DM_FSYS_WRITE_INVIS_RVP:
			vptr->write_invis_rvp = vecp[i].u_fc.write_invis_rvp;
			break;
		default:		/* ignore ones we don't understand */
			break;
		}
	}
}


/* Given a behavior pointer, dm_fsys_vector() returns a pointer to the DMAPI 
   function vector to be used for the corresponding vnode.  There is one possible
   function vector for each filesystem type, although currently XFS is the
   only filesystem that actually supports DMAPI.
*/

dm_fsys_vector_t *
dm_fsys_vector(
	bhv_desc_t	*bdp)
{
	dm_vector_map_t	*map;
	struct vfs      *vfsp = BHV_TO_VNODE(bdp)->v_vfsp;
	int		fstype = vfsp->vfs_fstype;;

	/* If this is the first call, initialize the filesystem function
	   vector map.
	*/

	if (dm_fsys_map == NULL) {
		int	size = vfsmax * sizeof(*dm_fsys_map);
		int	i;

		dm_fsys_map = (dm_vector_map_t *)kmem_zalloc(size, KM_SLEEP);
		for (i = 0; i < vfsmax; i++) {
			dm_fsys_map[i].support_type = DM_SUPPORT_UNKNOWN;
		}
	}
	map = &dm_fsys_map[fstype];

	/* If a new filesystem has been dynamically loaded into a slot
	   previously held by another filesystem, then treat it as a
	   DM_SUPPORT_UNKNOWN.
	*/

	if (strcmp(map->name, vfssw[fstype].vsw_name))
		map->support_type = DM_SUPPORT_UNKNOWN;

	/* If we don't yet know what the filesystem supports, ask it. */

	if (map->support_type == DM_SUPPORT_UNKNOWN)
		dm_query_fsys_for_vector(bdp);

	/* Now return the function vector. */

	return(map->vptr);
}
