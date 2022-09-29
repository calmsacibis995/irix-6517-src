#include "DeviceInfo.H"

#include <bstring.h>
#include <stddef.h>

DeviceInfo::DeviceInfo(const DeviceAddress& a, const inventory_s& i)
: _inq_valid(false), _address(a), _inventory(i), _dso(NULL)
{ }

void
DeviceInfo::inquiry(const char *inq)
{
    bcopy(inq, _inquiry, sizeof _inquiry);
    _inq_valid = true;
}
