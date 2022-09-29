#include "DeviceAddress.H"

#include <assert.h>
#include <stddef.h>
#include <sys/invent.h>

#include "ParallelAddress.H"
#include "SCSIAddress.H"

DeviceAddress::DeviceAddress(const DeviceAddress& that)
: _refcount(0)
{
    if (that._refcount == 0)
    {
	//  That is a DeviceAddress, not a derived class.
	//  Share its rep.

	_rep = that._rep;
	if (_rep)
	    _rep->_refcount++;
    }
    else
    {
	//  That is a derived class.  Clone it.
	
	_rep = that.clone();
    }	
}

DeviceAddress&
DeviceAddress::operator = (const DeviceAddress& that)
{
    if (this != &that)
    {   if (_rep && !--_rep->_refcount)
	    delete _rep;
	_rep = that._rep;
	if (_rep)
	{   assert(_rep->_refcount > 0);
	    _rep->_refcount++;
	}
    }
    return *this;
}

DeviceAddress::~DeviceAddress()
{
    if (_rep && !--_rep->_refcount)
	delete _rep;
}

DeviceAddress *
DeviceAddress::clone() const
{
    return new DeviceAddress(*this);
}

const SCSIAddress *
DeviceAddress::as_SCSIAddress() const
{
    return _rep ? _rep->as_SCSIAddress() : NULL;
}

SCSIAddress *
DeviceAddress::as_SCSIAddress()
{
    return _rep ? _rep->as_SCSIAddress() : NULL;
}

const ParallelAddress *
DeviceAddress::as_ParallelAddress() const
{
    return _rep ? _rep->as_ParallelAddress() : NULL;
}

ParallelAddress *
DeviceAddress::as_ParallelAddress()
{
    return _rep ? _rep->as_ParallelAddress() : NULL;
}

void
DeviceAddress::name(char *p, unsigned int n) const
{
    if (_rep)
	_rep->name(p, n);
    else if (n)
	*p = '\0';
}	

bool
DeviceAddress::operator == (const DeviceAddress& that) const
{
    if (_rep == that._rep)
	return true;
    if (!_rep || !that._rep)
	return false;
    return *_rep == *that._rep;
}

DeviceAddress::DeviceAddress(const inventory_s& inv)
: _refcount(0), _rep(NULL)
{
    switch (inv.inv_class)
    {

    case INV_DISK:
	if (inv.inv_type == INV_SCSIFLOPPY)
	    _rep = new SCSIAddress((int) inv.inv_controller,
				   (int) inv.inv_unit,
				   0);
	else if (inv.inv_type == INV_PCCARD)
	    _rep = new SCSIAddress((int) inv.inv_controller,
				   (int) inv.inv_unit,
				   inv.inv_state >> 8 & 0xFF);
	break;

    case INV_TAPE:
	_rep = new SCSIAddress((int) inv.inv_controller,
			       (int) inv.inv_unit,
			       0);
	break;

    case INV_SCSI:
	if (inv.inv_type == INV_CDROM || inv.inv_type == INV_OPTICAL)
	    _rep = new SCSIAddress((int) inv.inv_controller,
				   (int) inv.inv_unit,
				   inv.inv_state >> 8 & 0xFF);
	break;

    case INV_PARALLEL:
	if (inv.inv_type == INV_EPP_ECP_PLP)
	    _rep = new ParallelAddress;
	break;
    }
}

DeviceAddress::DeviceAddress(int ctlr, int id, int lun)
: _refcount(0),
  _rep(new SCSIAddress(ctlr, id, lun))
{ }

DeviceAddress::DeviceAddress(const Parallel&)
: _refcount(0),
  _rep(new ParallelAddress)
{ }
