#include "Inventory.H"

#include <assert.h>
#include <invent.h>

#include "DeviceAddress.H"

bool         Inventory::_initialized = false;
unsigned     Inventory::_ninv;
inventory_s *Inventory::_invp;

const inventory_s *
Inventory::operator [] (unsigned i) const
{
    if (!_initialized)
	init();
    return i < _ninv ? &_invp[i] : 0;
}

const inventory_s *
Inventory::at(const DeviceAddress& da) const
{
    if (!_initialized)
	init();
    for (unsigned i = 0; i < _ninv; i++)
	if (DeviceAddress(_invp[i]) == da)
	    return &_invp[i];
    return NULL;
}

void
Inventory::init()
{
    assert(!_initialized);
    inventory_s *inv;
    int ninv;

    //  Iterate through the hardware and count the relevant devices.

    setinvent();
    ninv = 0;
    while (inv = getinvent())
        if (DeviceAddress(*inv).valid())
	    ninv++;

    //  Allocate memory.

    _invp = new inventory_s[ninv];
    _ninv = ninv;

    //  Iterate through the hardware again, save relevant devices'
    //  inventory records and addresses.  (The loop below used to
    //  reverse the order because the hinv list was in a seemingly
    //  backwards order.)

    setinvent();
    int i = 0;
    while (inv = getinvent())
	if (DeviceAddress(*inv).valid())
	    _invp[i++] = *inv;
    assert(i == _ninv);
    endinvent();
    _initialized = true;
}
