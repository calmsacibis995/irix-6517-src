#include "MonitorClient.H"

#include <assert.h>
#include <bstring.h>
#include <mediaclient.h>
#include <mntent.h>
#include <unistd.h>
#include <xdr_mc.h>

#include "CallBack.H"
#include "Device.H"
#include "DeviceMonitor.H"
#include "Event.H"
#include "Log.H"
#include "Partition.H"
#include "Scheduler.H"
#include "Volume.H"

MonitorClient::MonitorClient(int fd, bool is_remote)
: _handler(read_handler, this),
  _wants_forced_unmount(false),
  _is_remote(is_remote)
{
    //  Wrap an xdrrec stream around the fd.

    xdrrec_create(&_xdrs, 0, 0, this, read_proc, write_proc);

    //  Activate read handler.

    _handler.fd(fd);
    _handler.activate();

    CallBack::add(event_proc, this);

    //  Dump the world's state now.

    mc_event event = { MCE_NO_EVENT, -1, -1 };
    transmit_world(&event);
}

MonitorClient::~MonitorClient()
{
    CallBack::remove(event_proc, this);

    xdr_destroy(&_xdrs);
    (void) close(_handler.fd());
}

void
MonitorClient::read_handler(int fd, void *closure)
{
    MonitorClient *client = (MonitorClient *) closure;

    // If the data read consists of a single 'S'; then that's the
    // indication that the client wants to receive forced unmount events.

    char byte = ' ';
    ssize_t count = read(fd, &byte, 1);

    if (count == 1 && byte == 'S' && !client->is_remote())
	client->_wants_forced_unmount = true;

    // close down on EOF.
    if (count == 0)
	delete client;
}

void
MonitorClient::event_proc(const Event& event, void *closure)
{
    MonitorClient *client = (MonitorClient *) closure;

    mc_event mce;
    switch (event.type())
    {
    case Event::Config:
	mce.mce_type = MCE_CONFIG;
	break;

    case Event::Insertion:
	mce.mce_type = MCE_INSERTION;
	break;

    case Event::Ejection:
	mce.mce_type = MCE_EJECTION;
	break;

    case Event::Suspend:
	mce.mce_type = MCE_SUSPEND;
	break;

    case Event::Resume:
	mce.mce_type = MCE_RESUME;
	break;

    case Event::Mount:
	mce.mce_type = MCE_MOUNT;
	break;

    case Event::Dismount:
	mce.mce_type = MCE_DISMOUNT;
	break;

    case Event::ForceUnmount:
	mce.mce_type = MCE_FORCEUNMOUNT;
	break;

    case Event::Export:
	mce.mce_type = MCE_EXPORT;
	break;

    case Event::Unexport:
	mce.mce_type = MCE_UNEXPORT;
	break;

    default:
	Log::error("MonitorClient::event_proc unknown event %d",
		   (int) event.type());
	return;
    }
    Device *dev = event.device();
    if (dev)
    {   if (client->is_remote() && !dev->is_shared())
	    return;			// Drop events for unshared devices.
	mce.mce_device = client->count_devices(dev);
    }
    else
	mce.mce_device = -1;
    Volume *vol = event.volume();
    if (vol)
    {   if (client->is_remote() && !vol->devices_are_shared())
	    return;			// Drop events for unshared volumes.
	mce.mce_volume = client->count_volumes(vol);
    }
    else
	mce.mce_volume = -1;
    if (mce.mce_type == MCE_FORCEUNMOUNT) 
    {
	if (client->wants_forced_unmount())
	{
	    assert(!client->is_remote());
	    client->transmit_world(&mce);
	    char response;
	    (void) read_proc(client, &response, 1);
	    return;
	}
	// forced unmount is a nop for all clients who haven't requested it
	return;
    }
    client->transmit_world(&mce);
}

int
MonitorClient::read_proc(void *closure, void *vbuffer, u_int nbytes)
{
    MonitorClient *client = (MonitorClient *) closure;

    int fd = client->_handler.fd();
    char *buffer = (char *) vbuffer;

    return read(fd, buffer, nbytes);
}

int
MonitorClient::write_proc(void *closure, void *vbuffer, u_int nbytes)
{
    MonitorClient *client = (MonitorClient *) closure;

    int fd = client->_handler.fd();
    char *buffer = (char *) vbuffer;

    return write(fd, buffer, nbytes);
}

///////////////////////////////////////////////////////////////////////////////
//  Enumeration.  We do not use the nifty enumeration methods defined
//  in Enumerable.  Instead, we have our own, and we get a different
//  set of volumes/partitions/devices depending on whether the client
//  is local or remote.  Remote clients can't see any devices that
//  are not shared, nor partitions on those devices, nor volumes that
//  contain unshared partitions.  Local clients can see everything.

//  count_partitions(NULL) returns the number of (shared?) partitions
//  in the system.  count_partitions(p) returns the (shared?) index of
//  partition p.

unsigned int
MonitorClient::count_partitions(const Partition *part)
{
    if (is_remote())
    {
	assert(part == NULL || part->device()->is_shared());
	unsigned int count = 0;
	for (const Partition *p = Partition::first(); p != part; p = p->next())
	{   assert(p != NULL);
	    if (p->device()->is_shared())
		count++;
	}
	return count;
    }
    else
    {
	if (part)
	    return part->index();
	else
	    return Partition::count();
    }
}

unsigned int
MonitorClient::count_devices(const Device *dev)
{
    if (is_remote())
    {
	assert(dev == NULL || dev->is_shared());
	unsigned int count = 0;
	for (const Device *dp = Device::first(); dp != dev; dp = dp->next())
	{   assert(dp != NULL);
	    if (dp->is_shared())
		count++;
	}
	return count;
    }
    else
    {
	if (dev)
	    return dev->index();
	else
	    return Device::count();
    }
}

unsigned int
MonitorClient::count_volumes(const Volume *vol)
{
    if (is_remote())
    {
	assert(vol == NULL || vol->devices_are_shared());
	unsigned int count = 0;
	for (const Volume *vp = Volume::first(); vp != vol; vp = vp->next())
	{   assert(vp != NULL);
	    if (vp->devices_are_shared())
		count++;
	}
	return count;
    }
    else
    {
	if (vol)
	    return vol->index();
	else
	    return Volume::count();
    }
}

Partition *
MonitorClient::nth_partition(unsigned n)
{
    if (is_remote())
    {
	unsigned int count = 0;
	for (Partition *pp = Partition::first(); ; pp = pp->next())
	{   assert(pp != NULL);
	    if (pp->device()->is_shared())
		if (count++ == n)
		    return pp;
	}
    }	
    else
    {
	return Partition::nth(n);
    }
}

Device *
MonitorClient::nth_device(unsigned n)
{
    if (is_remote())
    {
	unsigned int count = 0;
	for (Device *dp = Device::first(); ; dp = dp->next())
	{   assert(dp != NULL);
	    if (dp->is_shared())
		if (count++ == n)
		    return dp;
	}
    }	
    else
    {
	return Device::nth(n);
    }
}

Volume *
MonitorClient::nth_volume(unsigned n)
{
    if (is_remote())
    {
	unsigned int count = 0;
	for (Volume *vp = Volume::first(); ; vp = vp->next())
	{   assert(vp != NULL);
	    if (vp->devices_are_shared())
		if (count++ == n)
		    return vp;
	}
    }	
    else
    {
	return Volume::nth(n);
    }
}

///////////////////////////////////////////////////////////////////////////////
//  XDR.  This stuff is very ugly.

struct my_xdr_mc_closure {
    xdr_mc_closure mc_stuff;
    MonitorClient *client;
    Volume *vol;
    Device *dev;
    Partition *part;
};

void
MonitorClient::transmit_world(const mc_event *event)
{
    my_xdr_mc_closure mxmc = {
	{
	    xdr_volpartptrs,
	    xdr_devpartptrs,
	    xdr_devptr,
	},
	this,
	NULL, NULL, NULL,
    };

    _xdrs.x_op = XDR_ENCODE;
    _xdrs.x_public = (caddr_t) &mxmc;
    (void) xdr_world(&_xdrs, (mc_event *) event);
    xdrrec_endofrecord(&_xdrs, TRUE);	// Flush world out.
}

#if 0
void
MonitorClient::transmit_zap(const mc_event *event)
{
    unsigned int n_things = 0;

    _xdrs.x_op = XDR_ENCODE;
    (xdr_u_int(&_xdrs, &n_things) &&
     xdr_u_int(&_xdrs, &n_things) &&
     xdr_u_int(&_xdrs, &n_things) &&
     xdr_mc_event(&_xdrs, (mc_event *) event) &&
     xdr_enumerate(&_xdrs, &n_things, NULL) &&
     xdr_enumerate(&_xdrs, &n_things, NULL) &&
     xdr_enumerate(&_xdrs, &n_things, NULL));
    xdrrec_endofrecord(&_xdrs, TRUE);	// Flush zap out.
}
#endif

bool_t
MonitorClient::xdr_world(XDR *xdrs, mc_event *event)
{
    unsigned int n_partitions = count_partitions();
    unsigned int n_devices = count_devices();
    unsigned int n_volumes = count_volumes();

    return (xdr_u_int(xdrs, &n_volumes) &&
	    xdr_u_int(xdrs, &n_devices) &&
	    xdr_u_int(xdrs, &n_partitions) &&
	    xdr_mc_event(xdrs, event) &&
	    xdr_enumerate(xdrs, &n_volumes, MonitorClient::xdr_volume) &&
	    xdr_enumerate(xdrs, &n_devices, MonitorClient::xdr_device) &&
	    xdr_enumerate(xdrs, &n_partitions, MonitorClient::xdr_partition));
}

bool_t
MonitorClient::xdr_enumerate(XDR *xdrs,
			     unsigned int *np,
			     bool_t (MonitorClient::*enumerated)(XDR *, unsigned int))
{
    unsigned int i;

    if (xdr_u_int(xdrs, np))
	for (i = 0; i < *np; i++)
	    if (!(this->*enumerated)(xdrs, i))
		return FALSE;
    return TRUE;
}

bool_t
MonitorClient::xdr_volume(XDR *xdrs, unsigned int index)
{
    assert(xdrs->x_op == XDR_ENCODE);	// Don't know how to do anything else

    //  Find the right volume.

    Volume *volp = nth_volume(index);
    assert(volp);

    //  Build an mc_volume structure.  Note that we're violating
    //  constness, but since x_op is XDR_ENCODE, it's okay.

    mc_volume vol;
    bzero(&vol, sizeof vol);
    vol.mcv_label     = (char *) (volp->label() ? volp->label() : "");
    vol.mcv_fsname    = (char *) volp->fsname();
    vol.mcv_dir       = (char *) volp->dir();
    vol.mcv_type      = (char *) volp->type();
    vol.mcv_subformat = (char *) (volp->subformat() ? volp->subformat() : "");
    vol.mcv_mntopts   = (char *) volp->options();
    vol.mcv_mounted   = volp->is_mounted();
    if (volp->forced_unmount())		// this is a forced unmount event...
	vol.mcv_mounted |= MC_VOL_FORCEUNMOUNT;
    vol.mcv_exported  = volp->is_shared();

    //  Transmit it.

    my_xdr_mc_closure *mxmc = (my_xdr_mc_closure *) xdrs->x_public;
    mxmc->vol = volp;
    return xdr_mc_volume(xdrs, &vol);
}

bool_t
MonitorClient::xdr_volpartptrs(XDR *xdrs, u_int *, mc_partition ***)
{
    assert(xdrs->x_op == XDR_ENCODE);

    const Partition *pp;

    my_xdr_mc_closure *mxmc = (my_xdr_mc_closure *) xdrs->x_public;
    Volume *vol = mxmc->vol;
    unsigned int nparts = vol->npartitions();

    // Translate it.

    if (!xdr_u_int(xdrs, &nparts))
	return FALSE;
    unsigned int ncheck = 0;
    for (unsigned int i = 0; pp = vol->partition(i); i++)
    {   unsigned int index = mxmc->client->count_partitions(pp);
	if (!xdr_u_int(xdrs, &index))
	    return FALSE;
	ncheck++;
    }
    assert(ncheck == nparts);

    return TRUE;
}

bool_t
MonitorClient::xdr_device(XDR *xdrs, unsigned int index)
{
    assert(xdrs->x_op == XDR_ENCODE);	// Don't know how to do anything else

    //  Find the right device.

    Device *dp = nth_device(index);
    assert(dp);
    DeviceMonitor *dmp = DeviceMonitor::at(dp);

    //  Build an mc_device structure.  Note that we're violating
    //  constness, but since x_op is XDR_ENCODE, it's okay.

    mc_device dev;
    bzero(&dev, sizeof dev);
    dev.mcd_name = (char *) dp->name();
    dev.mcd_fullname = (char *) dp->full_name();
    dev.mcd_ftrname = (char *) dp->ftr_name();
    dev.mcd_devname = (char *) dp->dev_name(FMT_RAW,
					    PartitionAddress::WholeDisk);

    dev.mcd_invent = dp->inventory();
    dev.mcd_monitored = ((dmp->is_suspended() ? MC_DEV_SUSPENDED :  0) |
			 (dp->is_ignored()    ? 0 : MC_DEV_NOTIGNORED));
    dev.mcd_media_present = dmp ? dmp->is_media_present() : false;
    dev.mcd_write_protected = dmp ? dmp->is_write_protected() : false;
    dev.mcd_shared = dp->is_shared();

    //  Transmit it.

    my_xdr_mc_closure *mxmc = (my_xdr_mc_closure *) xdrs->x_public;
    mxmc->dev = dp;
    return xdr_mc_device(xdrs, &dev);
}

bool_t
MonitorClient::xdr_devpartptrs(XDR *xdrs, u_int *, mc_partition ***)
{
    assert(xdrs->x_op == XDR_ENCODE);
    Partition *pp;
    unsigned int nparts;

    my_xdr_mc_closure *mxmc = (my_xdr_mc_closure *) xdrs->x_public;
    Device *dev = mxmc->dev;

    //  Look at all partitions.  Count the ones for this device.

    nparts = 0;
    for (pp = Partition::first(); pp; pp = pp->next())
	if (pp->device() == dev)
	    nparts++;

    //  Translate the partition count, then translate all partitions.

    if (!xdr_u_int(xdrs, &nparts))
	return FALSE;
    unsigned int ncheck = 0;
    for (pp = Partition::first(); pp; pp = pp->next())
	if (pp->device() == dev)
	{   unsigned int index = mxmc->client->count_partitions(pp);
	    if (!xdr_u_int(xdrs, &index))
		return FALSE;
	    ncheck++;
	}
    assert(ncheck == nparts);

    return TRUE;
}

bool_t
MonitorClient::xdr_partition(XDR *xdrs, unsigned int index)
{
    assert(xdrs->x_op == XDR_ENCODE);	// Don't know how to do anything else

    //  Find the right partition.

    const Partition *p = nth_partition(index);
    assert(p);

    //  Build an mc_partition structure.

    mc_partition part;
    part.mcp_device = NULL;
    part.mcp_format = (char *) p->format_name();
    part.mcp_index = p->address().partition();
    part.mcp_sectorsize = p->sector_size();
    part.mcp_sector0 = p->start_sector();
    part.mcp_nsectors = p->n_sectors();

    //  Transmit it.

    my_xdr_mc_closure *mxmc = (my_xdr_mc_closure *) xdrs->x_public;
    mxmc->dev = p->device();
    return xdr_mc_partition(xdrs, &part);
}

bool_t
MonitorClient::xdr_devptr(XDR *xdrs, mc_device **)
{
    assert(xdrs->x_op == XDR_ENCODE);
    my_xdr_mc_closure *mxmc = (my_xdr_mc_closure *) xdrs->x_public;
    Device *dev = mxmc->dev;
    unsigned int index = mxmc->client->count_devices(dev);
    return xdr_u_int(xdrs, &index);
}
bool_t
xdr_string(register XDR *xdrs, char **cpp, u_int maxsize)
{
	register char *sp = *cpp;  /* sp is the actual string pointer */
	u_int size;
	u_int nodesize;

	/*
	 * first deal with the length since xdr strings are counted-strings
	 */
	switch (xdrs->x_op) {
	case XDR_FREE:
		if (sp == NULL) {
			return(TRUE);	/* already free */
		}
		/* fall through... */
	case XDR_ENCODE:
		size = (int)strlen(sp);
		break;
	}
	if (! xdr_u_int(xdrs, &size)) {
		return (FALSE);
	}
	if (size > maxsize) {
		return (FALSE);
	}
	nodesize = size + 1;

	/*
	 * now deal with the actual bytes
	 */
	switch (xdrs->x_op) {

	case XDR_DECODE:
		if (nodesize == 0) {
			return (TRUE);
		}
		if (sp == NULL) {
			*cpp = sp = (char *)mem_alloc(nodesize);
		}
		if (sp == NULL) {
		        int enough_memory = 0; assert(enough_memory);
			return (FALSE);
		}
		sp[size] = 0;
		/* fall into ... */

	case XDR_ENCODE:
		return (xdr_opaque(xdrs, sp, size));

	case XDR_FREE:
		mem_free(sp, nodesize);
		*cpp = NULL;
		return (TRUE);
	}
	return (FALSE);
}
