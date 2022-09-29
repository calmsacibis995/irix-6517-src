#ifndef DeviceInfo_included
#define DeviceInfo_included

#include <sys/invent.h>

#include "bool.H"
#include "DeviceAddress.H"

class DeviceDSO;

//  DeviceInfo is a collection of info to be passed to device
//  instantiation routines in the hope that a device can recognize
//  itself.  It contains a device address, a H/W inventory record,
//  and, in the case of a SCSI device, the result of a SCSI INQUIRY
//  command.

class DeviceInfo {

public:

    DeviceInfo(const DeviceAddress&, const inventory_s&);

    const inventory_s& inventory() const { return _inventory; }
    const DeviceAddress& address() const { return _address; }
    const char *inquiry() const		 { return _inq_valid ? _inquiry : 0; }

    DeviceDSO *dso() const		 { return _dso; }

    void inquiry(const char *);
    void dso(DeviceDSO *p)		 { _dso = p; }

private:

    DeviceAddress _address;
    inventory_s _inventory;
    DeviceDSO *_dso;
    bool _inq_valid;
    char _inquiry[98];

};

#endif /* !DeviceInfo_included */
