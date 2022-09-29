#ifndef SCSIAddress_defined
#define SCSIAddress_defined

#include "DeviceAddress.H"

class SCSIAddress : public DeviceAddress {

public:

    SCSIAddress()			: DeviceAddress(RepDummy()) { }
    SCSIAddress(const SCSIAddress&);
    SCSIAddress(int ctlr, int id, int lun = 0);
    virtual bool operator == (const DeviceAddress&) const;
    virtual const SCSIAddress *as_SCSIAddress() const;
    virtual       SCSIAddress *as_SCSIAddress();
    virtual void  name(char *, unsigned int) const;


    int ctlr () const			{ return  _ctlr; }
    int id   () const			{ return    _id; }
    int lun  () const			{ return   _lun; }

protected:

    virtual DeviceAddress *clone() const;

private:

    unsigned char  _ctlr;
    unsigned char    _id;
    unsigned char   _lun;

};

#endif /* SCSIAddress_defined */
