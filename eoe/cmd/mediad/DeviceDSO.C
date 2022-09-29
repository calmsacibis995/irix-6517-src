#include "DeviceDSO.H"

#include <assert.h>
#include <dlfcn.h>
#include <stddef.h>

#include "Device.H"
#include "Log.H"

DeviceDSO::~DeviceDSO()
{
    assert(_instances == 0);
}

Device *
DeviceDSO::instantiate(const DeviceInfo& info)
{
    //  Ensure DSO is loaded.

    load();

    //  Find instantiation proc.  Call it.

    typedef Device *(InstProc(const DeviceInfo&));
    InstProc *instantiate = (InstProc *) sym("instantiate");
    Device *device = NULL;
    if (instantiate)
	device = (*instantiate)(info);
    else
	Log::error("device DSO \"%s\" has no instantiation procedure", name());

    //  If device was instantiated, count it.

    if (device)
	_instances++;
    return device;
}

void
DeviceDSO::deinstantiate(Device *device)
{
    delete device;
    _instances--;
}
