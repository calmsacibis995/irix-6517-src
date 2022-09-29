#ifndef Inventory_included
#define Inventory_included

//  An Inventory object presents the relevant part of the H/W
//  inventory as an indexable, searchable array.  The inventory is
//  read once, and a single copy is held, and all the objects refer to
//  it.
//
//  The only inventory records that are held are those that can
//  be translated to a valid DeviceAddress.

#include "bool.H"

class DeviceAddress;
struct inventory_s;

class Inventory {

public:

    // Use default constructors and destructor

    inline unsigned count() const;
    const inventory_s *operator [] (unsigned) const;
    const inventory_s *at(const DeviceAddress&) const;

private:

    static bool _initialized;
    static unsigned _ninv;
    static inventory_s *_invp;

    static void init();

};

inline unsigned
Inventory::count() const
{
    if (!_initialized)
	init();
    return _ninv;
}

#endif /* !Inventory_included */
