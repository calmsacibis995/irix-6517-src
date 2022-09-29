#ifndef PartitionAddress_included
#define PartitionAddress_included

#include "DeviceAddress.H"
#include "FormatIndex.H"
#include "VolumeAddress.H"

class PartitionAddress : public VolumeAddress {

public:

    enum { WholeDisk = -1, UnknownPartition = -2 };

    PartitionAddress(const DeviceAddress&, FormatIndex, int partno);
    PartitionAddress(const PartitionAddress&);
    virtual ~PartitionAddress();

    virtual const PartitionAddress *as_PartitionAddress() const;
    virtual PartitionAddress *as_PartitionAddress();
    virtual bool operator == (const VolumeAddress&) const;
    virtual bool overlaps(const VolumeAddress&) const;
    virtual const PartitionAddress *partition(unsigned index) const;

    const DeviceAddress& device() const	{ return _dev; }
    FormatIndex format() const		{ return _format; }
    int partition() const		{ return _part; }

protected:

    virtual VolumeAddress *clone() const;

private:

    DeviceAddress _dev;
    FormatIndex _format;
    int _part;

};

#endif /* !PartitionAddress_included */
