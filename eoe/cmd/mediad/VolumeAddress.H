#ifndef VolumeAddress_included
#define VolumeAddress_included

#include "bool.H"
#include "FormatIndex.H"

class PartitionAddress;

//  VolumeAddress is an abstract base class and an envelope class.

class VolumeAddress {

    enum { WholeDisk = -1, UnknownPartition = -2 };

public:

    VolumeAddress()			: _refcount(0), _rep(0) { }
    VolumeAddress(const char *path,
		  FormatIndex fmt = FMT_UNKNOWN,
		  int partno = UnknownPartition);
    VolumeAddress(const VolumeAddress&);
    VolumeAddress& operator = (const VolumeAddress&);
    virtual ~VolumeAddress();

    bool valid() const			{ return _rep != 0; }

    virtual const PartitionAddress *as_PartitionAddress() const;
    virtual PartitionAddress *as_PartitionAddress();
    virtual bool operator == (const VolumeAddress&) const;
    virtual bool overlaps(const VolumeAddress&) const;
    virtual const PartitionAddress *partition(unsigned index) const;

protected:

    struct RepDummy			{ RepDummy() { } };

    VolumeAddress(const RepDummy&)	: _refcount(1), _rep(0) { }
    virtual VolumeAddress *clone() const;

private:

    int _refcount;
    VolumeAddress *_rep;

};

#endif /* !VolumeAddress_included */
