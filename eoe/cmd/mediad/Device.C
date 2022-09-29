#include "Device.H"

#include <assert.h>
#include <stddef.h>

#include "Config.H"
#include "CallBack.H"
#include "DeviceInfo.H"
#include "Event.H"
#include "strplus.H"

Enumerable::Set Device::devices;

Device::Device(const DeviceInfo& info)
: Enumerable(devices),
  _dso(info.dso()),
  _address(info.address()),
  _inv_record(info.inventory()),
  _name(NULL),
  _is_ignored(false),
  _is_shared(false),
  _config(new Config(config_proc, this))
{
    _is_shared = _config->device_is_shared(_address);
}

Device::~Device()
{
    delete _config;
    delete [] _name;
}

Device *
Device::at(const DeviceAddress& addr)
{
    for (Device *p = first(); p; p = p->next())
	if (p->_address == addr)
	    break;
    return p;
}

int
Device::suspend_monitoring()
{
    return 0;				// Always successful.
}

int
Device::resume_monitoring()
{
    return 0;				// Always successful.
}

bool
Device::has_audio_data()
{
    return false;
}

void
Device::set_name(const char *new_name)
{
    assert(new_name != NULL);
    delete [] _name;
    _name = strdupplus(new_name);
}

void
Device::set_shared(bool value)
{
    bool was_shared = _is_shared;
    _is_shared = value;
    if (_is_shared != was_shared)
	CallBack::activate(Event(Event::Config, NULL, NULL));
}

void
Device::set_ignored(bool value)
{
    _is_ignored = value;
}

void
Device::config_proc(Config& config, void *closure)
{
    Device *device = (Device *) closure;
    device->set_shared(config.device_is_shared(device->address()));
}
