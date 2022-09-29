#include "DeviceMonitor.H"

#include <assert.h>
#include <mntent.h>
#include <mediad.h>

#include "CallBack.H"
#include "Config.H"
#include "Device.H"
#include "Event.H"
#include "Format.H"
#include "FormatDSO.H"
#include "FormatLibrary.H"
#include "FormatRaw.H"
#include "Log.H"
#include "Volume.H"

//  A DeviceMonitor exists for the life of mediad.  It has two logical
//  state flags, is_ignored and is_suspended.  The DeviceMonitor only
//  talks to the hardware if both flags are false.
//
//  is_suspended is set when a client does mediad_get_exclusiveuse(),
//  and is cleared when the client does mediad_release_exclusiveuse()
//  or the client dies.  The flag's value is stored in the
//  DeviceMonitor's _is_suspended member.
//
//  is_ignored is set by the MediaDaemon when the config file says to
//  ignore the device.  The actual flag is stored in the device, not
//  the DeviceMonitor.  UnknownDevices are always ignored, and other
//  devices are never ignored.  The MediaDaemon changes is_ignored by
//  destroying the old device and creating a new one.
//
//  The sequence to change the device is like this (several places in
//  MediaDaemon.C).
//
//	DeviceMonitor *mon = DeviceMonitor::at(dev);
//	mon->set_device(NULL);
//	destroy_device(dev);			/* destroys old device */
//	Device *new_dev = create_device(...);	/* creates new device */
//	mon->set_device(new_dev);
//
//  The MediaDaemon owns the Devices, and the old device has to be
//  destroyed before the new one is created.

//////////////////////////////////////////////////////////////////////////////
//  Constructor, destructor, looker upper.  Standard C++ overhead stuff.
//

Enumerable::Set DeviceMonitor::monitors;

DeviceMonitor::DeviceMonitor(Device *dp, FormatLibrary& lib)
: Enumerable(monitors),
  _config(new Config(config_proc, this)),
  _device(dp),
  _format_lib(lib),
  _state(UNKNOWN),
  _is_suspended(false),
  _media_locked(false),
  _write_protected(false),
  _poll_task(poll_proc, this),
  _postejectticks(POSTEJECT_CHKS)
{
    DeviceAddress da = _device->address();
    _inschk = _config->device_inschk(da);
    _rmvchk = _config->device_rmvchk(da);
    _currentchk = _inschk;

    // If this device is not ignored, begin monitoring it.

    if (dp != NULL && !is_ignored())
    {
	if (_device->resume_monitoring() == 0)
	    check_state();
    }
}

DeviceMonitor::~DeviceMonitor()
{
    delete_volumes(false);
    delete_partitions();
    unlock();
    delete _config;
}

DeviceMonitor *
DeviceMonitor::at(const Device *dev)
{
    for (DeviceMonitor *p = first(); p; p = p->next())
	if (p->_device == dev)
	    break;
    return p;
}

DeviceMonitor *
DeviceMonitor::at(const DeviceAddress& da)
{
    for (DeviceMonitor *p = first(); p; p = p->next())
	if (p->_device->address() == da)
	    break;
    return p;
}

///////////////////////////////////////////////////////////////////////////////
//  polling -- poll_proc() is a TaskProc.  It runs every few seconds
//  unless the media is locked.  poll_proc() calls check_state() do
//  do the actual polling.  check_state() calls handle_insertion()
//  or handle_ejection() if appropriate, then reschedules itself.

void
DeviceMonitor::poll_proc(Task& t, void *closure)
{
    DeviceMonitor *dm = (DeviceMonitor *) closure;
    assert(&t == &dm->_poll_task);
    dm->check_state();
}

void
DeviceMonitor::reschedule()
{
    assert(!_is_suspended);

    if (_media_locked)
    {
	_currentchk = _rmvchk;
    }
    else if (_postejectticks < POSTEJECT_CHKS && _device->feature_fast_poll())
    {
	_currentchk = 1;
	_postejectticks++;
    }
    else
    {
	_currentchk++;
	if (_currentchk > _inschk)
	    _currentchk = _inschk;
    }
    timeval interval = { (long) _currentchk, 0 };
    _poll_task.schedule(interval);
}

//  check_state() is called every few seconds to check whether
//  an insertion or hardware ejection has happened.

void
DeviceMonitor::check_state()
{
    if (_is_suspended == true)
	return;

    //  Probe for media.  If it's changed, handle it.

    int media_present = _device->is_media_present();
    if (media_present == true && _state != MEDIA)
	handle_insertion();
    else if (media_present == false && _state != NO_MEDIA)
	handle_ejection();
    reschedule();
}

//////////////////////////////////////////////////////////////////////////////
//  Hardware transition handlers.  These are called when we detect that
//  the device's hardware state has changed -- i.e. the user has
//  inserted or ejected a medium.

void
DeviceMonitor::handle_insertion()
{
    if (_state == UNKNOWN)
	Log::info("Device \"%s\" medium is present.", _device->name());
    else
	Log::info("Device \"%s\" medium was inserted.", _device->name());
    _state = MEDIA;

    if (_device->feature_mountable())
	lock();

    //  Check write-protection.

    _write_protected = _device->is_write_protected();

    //  Identify any formats on the media.

    if (_device->feature_mountable())
    {   FormatDSO *format;
	FormatRaw::inspect(*_device);
	for (int i = 0; format = _format_lib[i]; i++)
	    format->inspect(*_device);
    }
    else
    {
	// It's a tape.

	if (_device->has_audio_data())
	{
	    // XXX hack for DAT
	    PartitionAddress address(_device->address(),
				     FMT_AUDIO,
				     PartitionAddress::WholeDisk);
	    Partition::create(address, _device, 0, 0, 0, "music");
	}
	else
	{
	    PartitionAddress address(_device->address(),
				     FMT_RAW,
				     PartitionAddress::WholeDisk);
	    Partition::create(address, _device, 0, 0, 0, "archive");
	}
    }

    //  Notify everybody who is anybody.

    CallBack::activate(Event(Event::Insertion, _device, NULL));
}

//  handle_ejection() is called when we discover the medium is gone.
//  We update our state to reflect reality. (-:

void
DeviceMonitor::handle_ejection()
{
    if (_state == UNKNOWN)
	Log::info("Device \"%s\" medium is absent.", _device->name());
    else
	Log::info("Device \"%s\" medium was ejected.", _device->name());

    delete_volumes(true);
    delete_partitions();
    
    _state = NO_MEDIA;
    _media_locked = false;
    _write_protected = false;
    _postejectticks = 0;
    CallBack::activate(Event(Event::Ejection, _device, NULL));
}

bool
DeviceMonitor::is_ignored() const
{
    return _device ? _device->is_ignored() : true;
}

//////////////////////////////////////////////////////////////////////////////
//  Actions.  These routines are called on behalf of some client
//  or something.  Each returns 0 on success, or an errno value
//  on failure.

void
DeviceMonitor::set_device(Device *dev)
{
    bool was_ignored = is_ignored();
    bool now_ignored = dev ? dev->is_ignored() : true;

    if (now_ignored && !was_ignored)
    {
	assert(_device != NULL);
	if (!is_suspended())
	{
	    if (_poll_task.scheduled())
		_poll_task.cancel();
	    delete_volumes(false);
	    delete_partitions();
	    unlock();
	}
    }

    _device = dev;

    if (was_ignored && !now_ignored)
    {
	assert(_device != NULL);
	if (!is_suspended())
	    if (_device->resume_monitoring() == 0)
	    {	_state = UNKNOWN;	// Force check_state to do something.
		check_state();
	    }
    }
}

void
DeviceMonitor::resume()
{
    CallBack::activate(Event(Event::Resume, _device, NULL));
    _is_suspended = false;
    _state = UNKNOWN;			// Force check_state to do something.
    if (_device->resume_monitoring() == 0)
	check_state();
}

int
DeviceMonitor::suspend()
{
    _is_suspended = true;
    if (!dismount_volumes(false))
	return RMED_ECANTUMOUNT;
    delete_partitions();
    if (_device->suspend_monitoring() < 0)
	return RMED_ESYSERR;
    if (_poll_task.scheduled())
	_poll_task.cancel();
    CallBack::activate(Event(Event::Suspend, _device, NULL));
    return RMED_NOERROR;
}

int
DeviceMonitor::eject()
{
    Log::info("Ejecting device \"%s\".", _device->name());

    //  Check that we're in a good state.

    if (_state == NO_MEDIA && !_device->feature_empty_ejectable())
	return RMED_ENODISC;
    if (_is_suspended)
	return RMED_ECANTUMOUNT;

    //  Dismount all volumes.

    if (!dismount_volumes(false))
	return RMED_ECANTUMOUNT;
    delete_partitions();
    unlock();
    reschedule();
    int rc = _device->eject();
    if (rc == RMED_NOERROR || rc == RMED_CONTEJECT)
      { 
	_state = NO_MEDIA;
	_write_protected = false;
	_postejectticks = 0;
	CallBack::activate(Event(Event::Ejection, _device, NULL));
      }
    return rc;
}

//////////////////////////////////////////////////////////////////////////////

// dismount_volumes returns true on success.

bool
DeviceMonitor::dismount_volumes(bool force)
{
    //  Dismount and delete all volumes using this device.  Note that
    //  if ANY partition of a volume is on this device, the whole
    //  volume is dismounted.

#if 0
    for (Volume *vol = Volume::first(), *next = NULL; vol; vol = next)
    {   next = vol->next();
	const Partition *part;
	for (unsigned int i = 0; part = vol->partition(i); i++)
	    if (part->device() == _device)
	    {   if (vol->is_mounted())
		    vol->dismount(false);
		if (force && vol->is_mounted())
		    vol->dismount(true);
		if (vol->is_mounted())
		    return false;

		delete vol;
		break;
	    }
    }
    return true;
#else
    bool success = true;
    for (Volume *vol = Volume::first(), *next = NULL; vol; vol = next)
    {   next = vol->next();
	const Partition *part;
	for (unsigned int i = 0; part = vol->partition(i); i++)
	    if (part->device() == _device)
	    {   if (vol->is_mounted())
		    vol->dismount(force);
		if (vol->is_mounted())
		    success = false;
		else
		    delete vol;
		break;
	    }
    }
    return success;
#endif
}

void
DeviceMonitor::delete_volumes(bool force_dismount)
{
    for (Volume *vol = Volume::first(), *next = NULL; vol; vol = next)
    {   next = vol->next();
	const Partition *part;
	for (unsigned int i = 0; part = vol->partition(i); i++)
	    if (part->device() == _device)
	    {   if (vol->is_mounted())
		    vol->dismount(force_dismount);
		delete vol;
		break;
	    }
    }
}

void
DeviceMonitor::delete_partitions()
{
    //  Delete all partitions on this device.

    for (Partition *part = Partition::first(), *nexp = NULL; part; part = nexp)
    {	nexp = part->next();
	if (part->device() == _device)
	    delete part;
    }
}

void
DeviceMonitor::lock()
{
    //  Lock the media into place, if possible.

    if (_device->feature_sw_lock())
    {   if (_device->lock_media() == 0)
	    _media_locked = true;
	else
	    Log::perror("couldn't lock %s", _device->name());
    }
}

void
DeviceMonitor::unlock()
{
    //  Enable ejection, if possible.

    if (_device->feature_sw_lock())
    {   if (_device->unlock_media() == 0)
	    _media_locked = false;
	else
	    Log::perror("couldn't unlock %s", _device->name());
    }
}

//  config_proc is called whenever the config file changes.  All it
//  does is copy the new values for inschk and rmvchk to the
//  DeviceMonitor's variables.  DeviceMonitor::reschedule() will start
//  using the new values shortly.

void
DeviceMonitor::config_proc(Config& config, void *closure)
{
    DeviceMonitor *mon = (DeviceMonitor *) closure;
    DeviceAddress da = mon->_device->address();
    mon->_inschk = config.device_inschk(da);
    mon->_rmvchk = config.device_rmvchk(da);
}
