#include "UnknownDevice.H"

#include <assert.h>

#include "DeviceInfo.H"

UnknownDevice::UnknownDevice(const DeviceInfo& info)
: Device(info)
{
    set_ignored(true);
    const inventory_s& inv = info.inventory();
    if (inv.inv_class == INV_SCSI && inv.inv_type == INV_CDROM)
	_type = CDROM;
    else if (inv.inv_class == INV_TAPE && inv.inv_type == INV_SCSIQIC)
	_type = TAPE;
    else
	_type = GENERIC;
}

UnknownDevice::~UnknownDevice()
{ }

const char *
UnknownDevice::short_name() const
{
    return "device";
}

const char *
UnknownDevice::full_name() const
{
    switch (_type)
    {
    case CDROM:
	return "Unknown CD-ROM Device";

    case TAPE:
	return "Unknown Tape Device";

    default:
	return "Unknown Device";
    }
}

const char *
UnknownDevice::ftr_name() const
{
    return "device";
}

const char *
UnknownDevice::dev_name(FormatIndex, int /* partno */ ) const
{
    return "";
}

int
UnknownDevice::features()
{
    // No features at all.
    return feature_set(0, ~0);
}

int
UnknownDevice::is_media_present()
{
    assert(0);
    return 0;
}

int
UnknownDevice::eject()
{
    assert(0);
    return -1;
}

int
UnknownDevice::lock_media()
{
    assert(0);
    return -1;
}

int
UnknownDevice::unlock_media()
{
    assert(0);
    return -1;
}

int
UnknownDevice::capacity()
{
    assert(0);
    return -1;
}

int
UnknownDevice::sector_size(FormatIndex)
{
    assert(0);
    return -1;
}

bool
UnknownDevice::is_write_protected()
{
    assert(0);
    return false;
}

bool
UnknownDevice::has_audio_data()
{
    assert(0);
    return false;
}

int
UnknownDevice::read_data(char *, __uint64_t, unsigned, unsigned)
{
    assert(0);
    return -1;
}

