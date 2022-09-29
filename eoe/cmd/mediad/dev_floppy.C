#include <assert.h>
#include <bstring.h>
#include <dslib.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/dkio.h>
#include <sys/smfd.h>
#include <unistd.h>
#include <mediad.h>

#include "DeviceInfo.H"
#include "Device.H"
#include "DSReq.H"
#include "Log.H"
#include "PartitionAddress.H"
#include "SCSIAddress.H"

class FloppyDevice : public Device {

public:

    FloppyDevice(const DeviceInfo&);

    const char *short_name() const	{ return "floppy"; }
    const char *full_name() const	{ return "floppy"; }
    const char *ftr_name() const	{ return "floppy"; }
    const char *dev_name(FormatIndex, int partno) const;

    //  Device capabilities

    virtual int features();

    // 	Media presence support

    virtual int is_media_present();
    virtual int eject();
    virtual int lock_media();
    virtual int unlock_media();

    //  Media access

    int capacity();
    int sector_size(FormatIndex)	{ return SECTOR_SIZE; }
    bool is_write_protected();
    int read_data(char *, __uint64_t, unsigned, unsigned);

private:

    //  Device-specific constants

    enum Brand { GENERIC, INSITE, TEAC, NCR };
    enum { SECTOR_SIZE = 512 };
    enum States { NORMAL, EXPECTING_EJECT };

    //  Device-specific method

    States state;
    Brand _brand;
    bool _supports_sw_eject;
    bool _supports_sw_lock;
    char _smfd_name_hi[24];
    char _smfd_name_lo[24];
    int _capacity;
    DSReq	_dsr;

};

static const char Insite[] = "INSITE";
static const char Teac[] = "TEAC";
static const char Ncr[] = "NCR";

//////////////////////////////////////////////////////////////////////////////

FloppyDevice::FloppyDevice(const DeviceInfo& info)
: Device(info),
  _brand(GENERIC),
  _dsr(*info.address().as_SCSIAddress()),
  _supports_sw_eject(true),
  _supports_sw_lock(true),
  _capacity(-1)
{
    const SCSIAddress *sa = info.address().as_SCSIAddress();
    (void) sprintf(_smfd_name_hi, "/dev/rdsk/fds%ud%u.3.5hi",
		   sa->ctlr(), sa->id());
    (void) sprintf(_smfd_name_lo, "/dev/rdsk/fds%ud%u.3.5",
		   sa->ctlr(), sa->id());
    const char *inquiry = info.inquiry();
    const char *vendor = inquiry + 8;
    if (!strncasecmp(vendor, Insite, strlen(Insite)))
	_brand = INSITE;
    else if (!strncasecmp(vendor, Teac, strlen(Teac))){
	_brand = TEAC;
        _supports_sw_eject = true;
        _supports_sw_lock = false;
    }
    else if (!strncasecmp(vendor, Ncr, strlen(Ncr))){
	_brand = NCR;
	_supports_sw_eject = false;
	_supports_sw_lock = false;
    }
    state = NORMAL;
}

const char *
FloppyDevice::dev_name(FormatIndex, int /* partno */ ) const
{
    // The TEAC SCSI floppy only supports two capacities:
    // 1440 and 2880.  The Insite floptical works fine
    // no matter which device you access it through.
    // So we skimp a little on generating the device name.

    return _capacity == 1440 ? _smfd_name_lo : _smfd_name_hi;
}

int
FloppyDevice::features()
{
    int remove = FILESYS_EFS | FILESYS_XFS;
    if (!_supports_sw_lock)
	remove |= FEATURE_SW_LOCK;
    if (!_supports_sw_eject)
	remove |= FEATURE_SW_EJECT;
    return feature_set(0, remove);
}

int
FloppyDevice::is_media_present()
{
    dsreq	*dsp = _dsr;
    char	asc, asq;

    if (state == NORMAL){
	if (!dsp)
		return -1;

    	testunitready00(dsp);
    	if (STATUS(dsp) != STA_GOOD){
        	if (dsp->ds_sensebuf != NULL && dsp->ds_senselen >= 14){
        	    asc =  dsp->ds_sensebuf[12];
            	    asq =  dsp->ds_sensebuf[13];
            	    if (asc == 0x04 && asq == 0x00)
		    {		_capacity = -1;
                		return (false);
		    }
        	}
        	return (true);
	}
    	return (true);

    }
    else {
	if (!dsp)
		return -1;

    	testunitready00(dsp);
	if (dsp->ds_sensebuf != NULL && dsp->ds_senselen >= 14){
    		asc = dsp->ds_sensebuf[12];
    		asq = dsp->ds_sensebuf[13];
		if (STATUS(dsp) != STA_GOOD){
			if (asc == 0x28 && asq == 0x00)
				state = NORMAL;
		}
	}
	_capacity = -1;
        return (false);
    }
}

int
FloppyDevice::eject()
{
#define FLEJECT_RETRY_COUNT	3
#define FLEJECT_RETRY_INTERVAL	2

     Log::debug("FloppyDevice::eject()");

    if (_supports_sw_eject){
    	int fd = open(_smfd_name_hi, O_RDONLY);
    	if (fd < 0)
		return RMED_ENODISC;

    	if (ioctl(fd, SMFDEJECT, 0) != 0)
    	{	Log::perror("floppy device \"%s\"", _smfd_name_hi);
		(void) close(fd);
		return RMED_ECANTEJECT;
    	}
    	(void) close(fd);
    }
      else {
	state = EXPECTING_EJECT;
    }
    _capacity = -1;
    return RMED_NOERROR;
}
    
int
FloppyDevice::lock_media()
{
    return 0;
}

int
FloppyDevice::unlock_media()
{
    return 0;				// No real lock/unlock, just be happy
}

int
FloppyDevice::capacity()
{
    switch (_brand)
    {
    default:
	{
	    // On Insite floptical drive, READ CAPACITY command works.
	    // Assume, brashly, that other drives work as well.

	    int fd = open(_smfd_name_hi, O_RDONLY);
	    if (fd < 0)
		return -1;
	    _capacity = -1;
	    (void) ioctl(fd, DIOCREADCAPACITY, &_capacity);
	    (void) close(fd);
	}
	break;
	
    case TEAC:
	{
	    // Try reading the high density device.

	    char sector[512];
	    int fd, rc;
	    _capacity = -1;
	    fd = open(_smfd_name_hi, O_RDONLY);
	    if (fd < 0)
		return -1;
	    rc = read(fd, sector, sizeof sector);
	    (void) close(fd);
	    if (rc == sizeof sector)
		_capacity = 2880;	// High density read works.

	    if (_capacity == -1)
	    {
		fd = open(_smfd_name_lo, O_RDONLY);
		if (fd < 0)
		    return -1;
		rc = read(fd, sector, sizeof sector);
		(void) close(fd);
		if (rc == sizeof sector)
		    _capacity = 1440;	// Low density read works.
	    }

	}
    }
    Log::debug("Floppy capacity is %d sectors", _capacity);
    return _capacity;
}

bool
FloppyDevice::is_write_protected()
{
    int fd = open(_smfd_name_hi, O_RDONLY);
    if (fd < 0)
	return false;
    int status;
    if (ioctl(fd, SMFDMEDIA, &status) == -1)
    {   (void) close(fd);
	return false;
    }
    (void) close(fd);
    return (status & SMFDMEDIA_WRITE_PROT) ? true : false;
}

int
FloppyDevice::read_data(char *buffer,
			__uint64_t start_sector,
			unsigned nsectors,
			unsigned sectorsize)
{
    assert(start_sector < 65536);    
    assert(nsectors < 256);

    if (sectorsize != SECTOR_SIZE)
	return EINVAL;			// Is this necessary?

    if (_capacity == -1)
	(void) capacity();		// initialize _capacity so we open
					// the right device.

    int fd = open(dev_name(FMT_RAW, PartitionAddress::WholeDisk), O_RDONLY);
    if (fd < 0)
	return errno;
    if (lseek(fd, (off_t) (sectorsize * start_sector), SEEK_SET) == -1)
    {	(void) close(fd);
	return errno;
    }
    int nbytes = nsectors * sectorsize;
    for (int i = 0, j = 0; i < nbytes; i += j)
    {   j = read(fd, buffer + i, nbytes - i);
	if (j <= 0)
	{   (void) close(fd);
	    return j == 0 ? ENOSPC : errno;
	}
    }
    (void) close(fd);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////

extern "C"
Device *
instantiate(const DeviceInfo& info)
{
    const inventory_s& inv = info.inventory();
    if (inv.inv_class != INV_DISK || inv.inv_type != INV_SCSIFLOPPY)
	return NULL;
    const SCSIAddress *sa = info.address().as_SCSIAddress();
    if (!sa)
	return NULL;
    if (sa->lun() != 0)
	return NULL;
    const char *inquiry = info.inquiry();
    if (!inquiry)
	return NULL;
    const char *vendor = inquiry + 8;
    if (strncasecmp(vendor, Insite, strlen(Insite)) &&
	strncasecmp(vendor, Teac, strlen(Teac)) &&
	strncasecmp(vendor, Ncr, strlen(Ncr)))
	return NULL;
    return new FloppyDevice(info);
}
