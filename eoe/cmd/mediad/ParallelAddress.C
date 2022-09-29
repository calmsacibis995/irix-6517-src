#include "ParallelAddress.H"

#include <assert.h>
#include <stdio.h>
#include <string.h>

ParallelAddress::ParallelAddress(const ParallelAddress& /* that */ )
: DeviceAddress(RepDummy())
{
}

DeviceAddress *
ParallelAddress::clone() const
{
    return new ParallelAddress(*this);
}

bool
ParallelAddress::operator == (const DeviceAddress& that) const
{
    const ParallelAddress *p = that.as_ParallelAddress();
    if (!p)
	return false;
    return true;
}

const ParallelAddress *
ParallelAddress::as_ParallelAddress() const
{
    return this;
}

ParallelAddress *
ParallelAddress::as_ParallelAddress()
{
    return this;
}

void
ParallelAddress::name(char *buffer, unsigned int n) const
{
    strncpy(buffer, "Parallel Port", n);
}
