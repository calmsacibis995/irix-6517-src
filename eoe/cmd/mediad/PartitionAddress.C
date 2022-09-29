#include "PartitionAddress.H"

#include <stddef.h>

#include "operators.H"

PartitionAddress::PartitionAddress(const DeviceAddress& d,
				   FormatIndex fi, int p)
: VolumeAddress(RepDummy()), _dev(d), _format(fi), _part(p)
{ }

PartitionAddress::PartitionAddress(const PartitionAddress& that)
: VolumeAddress(RepDummy()),
  _dev(that.device()),
  _format(that.format()),
  _part(that.partition())
{ }

PartitionAddress::~PartitionAddress()
{ }

VolumeAddress *
PartitionAddress::clone() const
{
    return new PartitionAddress(*this);
}

const PartitionAddress *
PartitionAddress::partition(unsigned index) const
{
    if (index == 0)
	return this;
    else
	return NULL;
}

bool
PartitionAddress::operator == (const VolumeAddress& that) const
{
    const PartitionAddress *p = that.as_PartitionAddress();
    if (!p)
	return false;

    return (device() == p->device() &&
	    format() == p->format() &&
	    partition() == p->partition());
}

bool
PartitionAddress::overlaps(const VolumeAddress& that) const
{
    const PartitionAddress *p;

    //  Check all partitions in that volume.

    for (int i = 0; p = that.partition(i); i++)
    {
	//  This partition overlaps that partition if they're on the
	//  same device and have the same format and the same
	//  partition number.
	//
	//  FMT_UNKNOWN matches any format, and WholeDisk matches
	//  any partition number.

	if (device() != p->device())
	    continue;
	if (partition() == p->partition())
	{   if (format() == p->format())
		return true;
	    if (format() == FMT_UNKNOWN || p->format() == FMT_UNKNOWN)
		return true;
	}
	if (partition() == WholeDisk || p->partition() == WholeDisk)
	    return true;
    }
    return false;
}

const PartitionAddress *
PartitionAddress::as_PartitionAddress() const
{
    return this;
}

PartitionAddress *
PartitionAddress::as_PartitionAddress()
{
    return this;
}
