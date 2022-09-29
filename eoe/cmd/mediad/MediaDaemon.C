#include "MediaDaemon.H"

#include <assert.h>
#include <bstring.h>
#include <dslib.h>
#include <invent.h>
#include <mntent.h>
#include <string.h>

#include "CallBack.H"
#include "Config.H"
#include "Device.H"
#include "DeviceDSO.H"
#include "DeviceInfo.H"
#include "DeviceMonitor.H"
#include "DSReq.H"
#include "Event.H"
#include "FormatDSO.H"
#include "FormatLibrary.H"
#include "Inventory.H"
#include "Log.H"
#include "PartitionAddress.H"
#include "UnknownDevice.H"
#include "VolumeAddress.H"

MediaDaemon::MediaDaemon()
: _config(new Config(config_proc, this)),
  _device_lib("/usr/lib/devicelib"),
  _format_lib("/usr/lib/devicelib")
{
    //  For each H/W device, see whether monitoring it is prohibited,
    //  and if not, create a Monitor for it.

    Inventory hinv;
    const inventory_s *invp;
    for (unsigned i = 0; invp = hinv[i]; i++)
    {	
	DeviceAddress da(*invp);
	bool dev_is_ignored = _config->device_is_ignored(da);
	Device *dev = create_device(dev_is_ignored, *invp);

	if (dev != NULL)
	    (void) new DeviceMonitor(dev, _format_lib);

	//  DeviceMonitors are self-enumerating; no need to save the ptr.
    }

    //  Flush any unused DSOs.

    _device_lib.flush();
}

MediaDaemon::~MediaDaemon()
{
    //  Ignore all monitored devices.  This is so that filesystems
    //  will be dismounted and events will be sent to clients showing
    //  all devices as UnknownDevice.

    bool config_changed = false;
    Inventory hinv;
    const inventory_s *invp;
    for (unsigned i = 0; invp = hinv[i]; i++)
    {	DeviceAddress da(*invp);
	Device *dev = Device::at(da);
	if (dev != NULL)
	{   DeviceMonitor *mon = DeviceMonitor::at(dev);
	    if (!dev->is_ignored())
	    {   mon->set_device(NULL);
		destroy_device(dev);
		mon->set_device(create_device(true, *invp));
		config_changed = true;
	    }
	}
    }

    //  Broadcast the new configuration.

    if (config_changed)
	CallBack::activate(Event(Event::Config, NULL, NULL));

    //  Destroy all device monitors.

    for (DeviceMonitor *mon; mon = DeviceMonitor::first(); )
	delete mon;

    //  Destroy all instantiated devices.

    for (Device *dev; dev = Device::first(); )
	destroy_device(dev);
    assert(DeviceMonitor::count() == 0);

    delete _config;
}

Device *
MediaDaemon::create_device(bool is_ignored, const inventory_s& inv)
{
    DeviceAddress addr(inv);
    DeviceInfo info(addr, inv);
    Device *return_dev = NULL;
    char aname[30];
    addr.name(aname, sizeof aname);
    if (is_ignored)
    {
	return_dev = new UnknownDevice(info);
	Log::info("ignoring device at %s", aname);
    }
    else
    {
	//  Wart.  If it's a SCSI device, do an INQUIRY command and
	//  put the result in the DeviceInfo.

	const SCSIAddress *sa = addr.as_SCSIAddress();
	if (sa)
	{   char inquiry[98];
	    bzero(inquiry, sizeof inquiry);
	    DSReq dsp(*sa);

	    if (dsp && inquiry12(dsp, inquiry, sizeof inquiry, 0) == STA_GOOD)
		info.inquiry(inquiry);
	}

	//  Iterate through all device DSOs.  Try to instantiate all
	//  of them.  If none succeed, or if more than one succeeds,
	//  log an error.

	DeviceDSO *dso;
	int ndev = 0;
	for (int i = 0; dso = _device_lib[i]; i++)
	{   info.dso(dso);
	    Device *device = dso->instantiate(info);
	    if (device)
	    {   ndev++;
		if (ndev == 1)
		{
		    //  Only use the first dev DSO that recognizes the device.

		    return_dev = device;
		}
		else
		    dso->deinstantiate(device);
	    }
	}

	// Hack: Do not print message for parallel floppy, as it often is
	// not installed.

	if (ndev == 0)
	{
	    if (!addr.as_ParallelAddress())
		Log::error("couldn't find DSO for device at %s", aname);
	}
	else
	{   if (ndev > 1)
		Log::error("found multiple DSOs matching device at %s", aname);
	    Log::info("monitoring device at %s", aname);
	}
    }
    if (return_dev)
	name_device(return_dev);
    return return_dev;
}

void
MediaDaemon::destroy_device(Device *dev)
{
    assert(DeviceMonitor::at(dev) == NULL);

    DeviceDSO *dso = dev->dso();
    if (dso == NULL)
	delete dev;			// Unknown device.
    else
	dso->deinstantiate(dev);
}

void
MediaDaemon::name_device(Device *device)
{
    const char *short_name = device->short_name();
    char name[100];
    strcpy(name, short_name);
    unsigned int ndevs = 1;

    for (Device *other_dev = Device::first(); other_dev; )
    {
	const char *other_name = other_dev->name();
	if (other_name && !strcmp(name, other_name))
	{
	    // That device name is already in use.
	    // Bump the counter and concatenate a new name,
	    // then start searching at the beginning.

	    ndevs++;
	    sprintf(name, "%s%u", short_name, ndevs);
	    other_dev = Device::first();
	}
	else
	    other_dev = other_dev->next();
    }

    // Most recent name is not in use, so we'll use it.

    device->set_name(name);
}

void
MediaDaemon::config_proc(Config& config, void *closure)
{
    MediaDaemon *cosmo_mediad = (MediaDaemon *) closure;
    Inventory hinv;
    const inventory_s *invp;

    bool config_changed = false;
    for (unsigned i = 0; invp = hinv[i]; i++)
    {	DeviceAddress da(*invp);
	Device *dev = Device::at(da);
	if (dev == NULL)
	    continue;
	DeviceMonitor *dm = DeviceMonitor::at(dev);
	bool ignored = config.device_is_ignored(da);
	assert(dm != NULL);
	if (dev->is_ignored() != ignored)
	{
	    dm->set_device(NULL);
	    cosmo_mediad->destroy_device(dev);
	    Device *new_device = cosmo_mediad->create_device(ignored, *invp);
	    dm->set_device(new_device);
	    config_changed = true;
	}
    }

    if (config_changed)
    {
	//  Flush any unused DSOs.

	cosmo_mediad->_device_lib.flush();

	CallBack::activate(Event(Event::Config, NULL, NULL));
    }
}
