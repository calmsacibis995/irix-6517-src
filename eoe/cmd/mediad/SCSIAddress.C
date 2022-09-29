#include "SCSIAddress.H"

#include <assert.h>
#include <stdio.h>
#include <string.h>

SCSIAddress::SCSIAddress(int ctlr, int id, int lun)
: DeviceAddress(RepDummy()), _ctlr(ctlr), _id(id), _lun(lun)
{
    assert(0 <= id && id < 16);
    assert(0 <= lun && lun < 8);
}

SCSIAddress::SCSIAddress(const SCSIAddress& that)
: DeviceAddress(RepDummy()),
  _ctlr(that.ctlr()),
  _id(that.id()),
  _lun(that.lun())
{
}

DeviceAddress *
SCSIAddress::clone() const
{
    return new SCSIAddress(*this);
}

bool
SCSIAddress::operator == (const DeviceAddress& that) const
{
    const SCSIAddress *p = that.as_SCSIAddress();
    if (!p)
	return false;
    return _ctlr == p->ctlr() && _id == p->id() && _lun == p->lun();
}

const SCSIAddress *
SCSIAddress::as_SCSIAddress() const
{
    return this;
}

SCSIAddress *
SCSIAddress::as_SCSIAddress()
{
    return this;
}

void
SCSIAddress::name(char *buffer, unsigned int n) const
{
    char temp[100];
    if (lun())
	sprintf(temp, "SCSI ctlr %d, id %d, lun %d", ctlr(), id(), lun());
    else
	sprintf(temp, "SCSI ctlr %d, id %d", ctlr(), id());
    (void) strncpy(buffer, temp, n);
}
