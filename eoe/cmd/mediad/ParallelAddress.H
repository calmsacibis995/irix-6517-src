#ifndef ParallelAddress_defined
#define ParallelAddress_defined

#include "DeviceAddress.H"

class ParallelAddress : public DeviceAddress {

public:

    ParallelAddress()			: DeviceAddress(RepDummy()) { }
    ParallelAddress(const ParallelAddress&);

    virtual bool operator == (const DeviceAddress&) const;
    virtual const ParallelAddress *as_ParallelAddress() const;
    virtual       ParallelAddress *as_ParallelAddress();
    virtual void  name(char *, unsigned int) const;

protected:

    virtual DeviceAddress *clone() const;

};

#endif /* ParallelAddress_defined */
