#ifndef DeviceAddress_included
#define DeviceAddress_included

#include "bool.H"

class inventory_s;
class ParallelAddress;
class SCSIAddress;

//  Abstract base class and envelope class for device addresses.
//  Every device has a unique address.  Most devices have a SCSI
//  address, but there are some non-SCSI devices (e.g.  Moosehead
//  floppy).

class DeviceAddress {

public:

    class Parallel { };

    DeviceAddress(const inventory_s&);
    DeviceAddress(int ctlr, int id, int lun = 0);
    DeviceAddress(const Parallel&);

    DeviceAddress()			: _refcount(0), _rep(0) { }
    DeviceAddress(const DeviceAddress&);
    DeviceAddress& operator = (const DeviceAddress&);
    virtual ~DeviceAddress();

    virtual bool operator == (const DeviceAddress&) const;
    virtual const SCSIAddress *as_SCSIAddress() const;
    virtual       SCSIAddress *as_SCSIAddress();
    virtual const ParallelAddress *as_ParallelAddress() const;
    virtual       ParallelAddress *as_ParallelAddress();
    virtual void  name(char *, unsigned int) const;

    bool valid() const			{ return _rep != 0; }

protected:

    struct RepDummy			{ RepDummy() { } };

    DeviceAddress(const RepDummy&)	: _refcount(1), _rep(0) { }
    virtual DeviceAddress *clone() const;

private:

    int _refcount;
    DeviceAddress *_rep;
};

#endif /* !DeviceAddress */
