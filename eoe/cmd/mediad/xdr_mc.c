#include "xdr_mc.h"

#include <assert.h>
#include <string.h>

#include "mediaclient.h"

#define MAX ((u_int) 0 - 1)		/* A lot. */

/*
 *  This file is included in both the server and the client sides.  It
 *  knows how to XDR-translate media client/server data structures.
 */

/*****************************************************************************/
/* private */

static bool_t
xdr_invent(XDR *xdrs, inventory_t *inv)
{
    if (xdrs->x_op == XDR_DECODE)
	inv->inv_next = NULL;
    assert(sizeof inv->inv_controller == sizeof (u_int));
    assert(sizeof inv->inv_unit == sizeof (u_int));
    return (xdr_int(xdrs, &inv->inv_class) &&
	    xdr_int(xdrs, &inv->inv_type) &&
	    xdr_u_int(xdrs, (u_int *) &inv->inv_controller) &&
	    xdr_u_int(xdrs, (u_int *) &inv->inv_unit) &&
	    xdr_int(xdrs, &inv->inv_state));
}

/*****************************************************************************/
/* public */

bool_t
xdr_mc_system(XDR *xdrs, mc_system_t *sys)
{
    return (xdr_array(xdrs,
		      (caddr_t *) &sys->mcs_volumes,
		      &sys->mcs_n_volumes,
		      MAX,
		      sizeof (mc_volume_t),
		      xdr_mc_volume) &&
	    xdr_array(xdrs,
		      (caddr_t *) &sys->mcs_devices,
		      &sys->mcs_n_devices,
		      MAX,
		      sizeof (mc_device_t),
		      xdr_mc_device) &&
	    xdr_array(xdrs,
		      (caddr_t *) &sys->mcs_partitions,
		      &sys->mcs_n_parts,
		      MAX,
		      sizeof (mc_partition_t),
		      xdr_mc_partition));
}

bool_t
xdr_mc_volume(XDR *xdrs, mc_volume_t *vol)
{
    xdr_mc_closure_t *data = (xdr_mc_closure_t *) xdrs->x_public;

    return (xdr_string(xdrs, &vol->mcv_label,     MAX) &&
	    xdr_string(xdrs, &vol->mcv_fsname,    MAX) &&
	    xdr_string(xdrs, &vol->mcv_dir,       MAX) &&
	    xdr_string(xdrs, &vol->mcv_type,      MAX) &&
	    xdr_string(xdrs, &vol->mcv_subformat, MAX) &&
	    xdr_string(xdrs, &vol->mcv_mntopts,   MAX) &&
	    xdr_int(xdrs, &vol->mcv_mounted) &&
	    xdr_bool(xdrs, &vol->mcv_exported) &&
	    (*data->volpartproc)(xdrs, &vol->mcv_nparts, &vol->mcv_parts));
}

bool_t
xdr_mc_device(XDR *xdrs, mc_device_t *dev)
{
    xdr_mc_closure_t *data = (xdr_mc_closure_t *) xdrs->x_public;

    return (xdr_string(xdrs, &dev->mcd_name,      MAX) &&
	    xdr_string(xdrs, &dev->mcd_fullname,  MAX) &&
	    xdr_string(xdrs, &dev->mcd_ftrname,   MAX) &&
	    xdr_string(xdrs, &dev->mcd_devname,   MAX) &&
	    xdr_invent(xdrs, &dev->mcd_invent) &&
	    xdr_int(xdrs, &dev->mcd_monitored) &&
	    xdr_bool(xdrs, &dev->mcd_media_present) &&
	    xdr_bool(xdrs, &dev->mcd_write_protected) &&
	    xdr_bool(xdrs, &dev->mcd_shared) &&
	    (*data->devpartproc)(xdrs, &dev->mcd_nparts, &dev->mcd_parts));
}

bool_t
xdr_mc_partition(XDR *xdrs, mc_partition_t *part)
{
    xdr_mc_closure_t *data = (xdr_mc_closure_t *) xdrs->x_public;

    return ((*data->partdevproc)(xdrs, &part->mcp_device) &&
	    xdr_string(xdrs, &part->mcp_format,   MAX) &&
	    xdr_int(xdrs, &part->mcp_index) &&
	    xdr_u_int(xdrs, &part->mcp_sectorsize) &&
	    xdr_uint64(xdrs, &part->mcp_sector0) &&
	    xdr_uint64(xdrs, &part->mcp_nsectors));
}

bool_t
xdr_mc_event(XDR *xdrs, mc_event_t *event)
{
    return (xdr_enum(xdrs, (enum_t *) &event->mce_type) &&
	    xdr_int(xdrs, &event->mce_volume) &&
	    xdr_int(xdrs, &event->mce_device));
}

/*****************************************************************************/

unsigned int
mc_count_pp(const mc_system_t *sys)
{
    unsigned int i, npp, nv, nd;
    const mc_volume_t *vp;
    const mc_device_t *dp;

    npp = 0;
    nv = sys->mcs_n_volumes;
    for (i = 0, vp = sys->mcs_volumes; i < nv; i++, vp++)
	npp += vp->mcv_nparts;
    nd = sys->mcs_n_devices;
    for (i = 0, dp = sys->mcs_devices; i < nd; i++, dp++)
	npp += dp->mcd_nparts;
    return npp;
}

unsigned int
mc_count_c(const mc_system_t *sys)
{
    unsigned int nc, i, nv, nd, np;

    nc = 0;
    nv = sys->mcs_n_volumes;
    for (i = 0; i < nv; i++)
    {   const mc_volume_t *vol = &sys->mcs_volumes[i];
	nc += strlen(vol->mcv_label) + 1;
	nc += strlen(vol->mcv_fsname) + 1;
	nc += strlen(vol->mcv_dir) + 1;
	nc += strlen(vol->mcv_type) + 1;
	nc += strlen(vol->mcv_subformat) + 1;
	nc += strlen(vol->mcv_mntopts) + 1;
    }

    nd = sys->mcs_n_devices;
    for (i = 0; i < nd; i++)
    {   const mc_device_t *dev = &sys->mcs_devices[i];
	nc += strlen(dev->mcd_name) + 1;
	nc += strlen(dev->mcd_fullname) + 1;
	nc += strlen(dev->mcd_ftrname) + 1;
	nc += strlen(dev->mcd_devname) + 1;
    }

    np = sys->mcs_n_parts;
    for (i = 0; i < np; i++)
    {	const mc_partition_t *part = &sys->mcs_partitions[i];
	nc += strlen(part->mcp_format) + 1;
    }
    return nc;
}
