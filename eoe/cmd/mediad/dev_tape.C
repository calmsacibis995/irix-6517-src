#include <assert.h>
#include <bstring.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mtio.h>
#include <sys/tpsc.h>
#include <mediad.h>

#include "DeviceInfo.H"
#include "Device.H"
#include "DSReq.H"
#include "Log.H"
#include "PartitionAddress.H"
#include "SCSIAddress.H"
#include "VolumeAddress.H"

//  Non-DAT tapes

class TapeDevice : public Device {

public:

    enum NameType { SHORT, FULL, FTR };

    TapeDevice(const DeviceInfo&, int type);
    ~TapeDevice();

    const char *short_name() const	{ return some_name(SHORT, _type); }
    const char *full_name()  const	{ return some_name(FULL, _type);  }
    const char *ftr_name()   const	{ return some_name(FTR, _type); }
    const char *dev_name(FormatIndex, int partno) const;

    //  Device capabilities

    virtual int features();

    // 	Media presence support

    virtual int suspend_monitoring();
    virtual int resume_monitoring();
    virtual int is_media_present();
    virtual int eject();
    virtual int lock_media();
    virtual int unlock_media();

    //  Media access

    int capacity();
    int sector_size(FormatIndex)	{ return SECTOR_SIZE; }
    bool is_write_protected();
    int read_data(char *, __uint64_t, unsigned, unsigned);

    static const char *some_name(NameType, int);

private:

    bool useVariableDevice();

    //  Device-specific constants

    enum { SECTOR_SIZE = 512 };

    //  Device-specific variables

    int _fd_stat;			
    int _type;
    char _rw_path[64];			// Path to rewinding device
    char _nr_path[64];			// Path to nonrewinding device
    char _stat_device[100];		// Path to stat device

};

//////////////////////////////////////////////////////////////////////////////

TapeDevice::TapeDevice(const DeviceInfo& info, int type)
: Device(info),
  _type(type)
{
    const DeviceAddress& da = info.address();
    const SCSIAddress *sa = info.address().as_SCSIAddress();
    assert(sa != 0);

    //  Check whether this tape is /dev/tape.  If not,
    //	generate a /dev/rmt/tps pathname.


    VolumeAddress devtape("/dev/tape");
    const PartitionAddress *tape_pa = devtape.partition(0);
    if (tape_pa != NULL && tape_pa->device() == da)
	strcpy(_rw_path, "/dev/tape");
    else {
	// Logic to figure out whether or not to use variable block
	// device.  If useVariableDevice says we should, try it.  If
	// the device doesn't exist, fall back to tpsXdY device.
	bool needDevice = true;
	if (useVariableDevice()) {
	    sprintf(_rw_path, "/dev/rmt/tps%ud%uv", sa->ctlr(), sa->id());
	    if (access(_rw_path, F_OK) == 0) {
		needDevice = false;
	    }
	}
	if (needDevice) {
	    sprintf(_rw_path, "/dev/rmt/tps%ud%u", sa->ctlr(), sa->id());
	}
    }
	
    //  Check whether this tape is /dev/tape.  If not,
    //	generate a /dev/rmt/tps pathname.

    VolumeAddress devtapenr("/dev/nrtape");
    const PartitionAddress *tapenr_pa = devtapenr.partition(0);
    if (tapenr_pa != NULL && tapenr_pa->device() == da)
	strcpy(_nr_path, "/dev/tapenr");
    else {
	bool needDevice = true;
	if (useVariableDevice()) {
	    sprintf(_nr_path, "/dev/rmt/tps%ud%unrv", sa->ctlr(), sa->id());
	    if (access(_nr_path, F_OK) == 0) {
		needDevice = false;
	    }
	}
	if (needDevice) {
	    sprintf(_nr_path, "/dev/rmt/tps%ud%unr", sa->ctlr(), sa->id());
	}
    }

    sprintf(_stat_device, "/dev/rmt/tps%ud%ustat", sa->ctlr(), sa->id());
    _fd_stat = open(_stat_device, O_RDONLY);
}

TapeDevice::~TapeDevice()
{
	(void) close(_fd_stat);
}

const char *
TapeDevice::dev_name(FormatIndex fi, int partno) const
{
    if (fi == FMT_RAW && partno == PartitionAddress::WholeDisk)
	return _rw_path;
    else
	return _nr_path;
}

int
TapeDevice::features()
{
    int add = 0;
    int remove = (0xFF & ~FILESYS_RAW) | FEATURE_MOUNTABLE;
    return feature_set(add, remove);
}

int
TapeDevice::suspend_monitoring()
{
    (void) close(_fd_stat);
    _fd_stat = -1;
    return 0;
}

int
TapeDevice::resume_monitoring()
{
    _fd_stat = open(_stat_device, O_RDONLY);
    if (_fd_stat < 0)
    {
	Log::perror("can't open tape device %s", _stat_device);
	return -1;
    }
    return 0;
}

int
TapeDevice::is_media_present()
{
    // Use an MTIOCGET ioctl to learn whether the tape is present.

    struct mtget mt_status;
    bzero(&mt_status, sizeof mt_status);
    mt_status.mt_type = MTNOP;
    if (ioctl(_fd_stat, MTIOCGET, &mt_status) != 0)
    {   Log::perror("tape device \"%s\" MTIOCGET", _stat_device);
	return false;
    }

    // The status is a 32bit word
    // Lower 16 are returned in: mt_dsreg
    // Upper 16 are returned in: mt_erreg
    // We look for any of:
    //   (1) CT_LOADED = 0x20000 - in erreg
    //   (2) CT_MOTION = 0x20	 - in dsreg
    //   (3) CT_ONL    = 0x40	 - in dsreg

    if ((mt_status.mt_erreg & (CT_LOADED>>16)) ||
	(mt_status.mt_dsreg & CT_MOTION) || 
	(mt_status.mt_dsreg & CT_ONL)){
	  return (true);
    }
    else {  
	if (_type == TPDLT){
		// With DLTs, the tape driver has some problem. It
		// does not indicate media presence, while tape is
		// in use by setting the above condition. We use this
		// kludge merely to get around DLTs non-compliance with
		// the above scheme.
		if ( mt_status.mt_erreg == 0x0000 &&
	     	     mt_status.mt_dsreg == 0x0200){
			return (-1);
		}
        }
	return (false);
    }
}

int
TapeDevice::eject()
{
    Log::debug("TapeDevice::eject()");
    DSReq::close_all();
    int fd = open(_nr_path, O_RDONLY);
    if (fd < 0)
	return errno == EIO ? RMED_NOERROR : RMED_ENODISC;

    struct mtop mt_com;
    bzero(&mt_com, sizeof mt_com);
    mt_com.mt_op = MTUNLOAD;

    if (ioctl(fd, MTIOCTOP, &mt_com) != 0)
    {	if (errno != EAGAIN)
	    Log::perror("TAPE device \"%s\"", _nr_path);
	(void) close(fd);
	return RMED_ECANTEJECT;
    }
    (void) close(fd);
    return RMED_NOERROR;
}
    
int
TapeDevice::lock_media()
{
    return 0;				// Don't bother locking tapes.

}

int
TapeDevice::unlock_media()
{
    return 0;				// No real lock/unlock, just be happy.
}

int
TapeDevice::capacity()
{
    assert(0);
    return -1;
}

bool
TapeDevice::is_write_protected()
{
    // Use an MTIOCGET ioctl to learn whether the tape is present.

    struct mtget mt_status;
    bzero(&mt_status, sizeof mt_status);
    mt_status.mt_type = MTNOP;
    if (ioctl(_fd_stat, MTIOCGET, &mt_status) != 0)
    {   Log::perror("TAPE device \"%s\" MTIOCGET", _stat_device);
	return false;
    }

    // Return true iff the tape is write-protected.

    if (mt_status.mt_dposn & MT_WPROT)
	return true;
    else
	return false;
}

int
TapeDevice::read_data(char * /* buffer */,
		     __uint64_t /* start_sector */,
		     unsigned /* nsectors */,
		     unsigned /* sectorsize */)
{
    assert(0);
    return -1;
}

//////////////////////////////////////////////////////////////////////////////

const char *
TapeDevice::some_name(NameType name_type, int tape_type)
{
    const char *fn, *sn, *rn;

    switch (tape_type)
    {
    case TPQIC24:
	fn = "QIC 24 tape";
	sn = "QIC";
	rn = "qic";
	break;

    case TPQIC150:
	fn = "QIC 150 tape";
	sn = "QIC";
	rn = "qic";
	break;

    case TP9TRACK:
	fn = "9-track tape";
	sn = "9track";
	rn = "9track";
	break;

    case TP8MM_8200:
	fn = "8mm 8200 tape";
	sn = "8mm";
	rn = "8mm";
	break;

    case TP8MM_8500:
	fn = "8mm 8500 tape";
	sn = "8mm";
	rn = "8mm";
	break;

    case TPQIC1000:
	fn = "QIC 1000 tape";
	sn = "QIC";
	rn = "qic";
	break;

    case TPQIC1350:
	fn = "QIC 1350 tape";
	sn = "QIC";
	rn = "qic";
	break;

    case TP3480:
	fn = "3480 tape";
	sn = "3480";
	rn = "3480";
	break;

    case TPDLT:
	fn = "DLT tape";
	sn = "DLT";
	rn = "dlt";
	break;

    case TPD2:
	fn = "D2 tape";
	sn = "D2";
	rn = "d2";
	break;

    case TPNTP:
	fn = "NTP tape";
	sn = "NTP";
	rn = "ntp";
	break;

    case TPDAT:
	fn = "DAT tape";
	sn = "DAT";
	rn = "dat";
	break;

    case TPUNKNOWN:
    case TPDLTSTACKER:
    case TPNTPSTACKER:
    default:
	//  unknown tape or stacker
	fn = "generic tape";
	sn = "cartridge";
	rn = "cartridge";
	break;
    }

    if (name_type == FULL)
	return fn;
    else if (name_type == SHORT)
	return sn;
    else
    {   assert(name_type == FTR);
	return rn;
    }
}

// QIC and 8mm tapes don't have variable block devices, everthing else
// does.
bool TapeDevice::useVariableDevice()
{
    switch (_type) {
    case TPQIC24:
    case TPQIC150:
    case TPQIC1000:
    case TPQIC1350:
    case TP8MM_8200:
    case TP8MM_8500:
	return false;

    case TP9TRACK:
    case TP3480:
    case TPDLT:
    case TPD2:
    case TPNTP:
    case TPDLTSTACKER:
    case TPNTPSTACKER:
    case TPDAT:
    case TPUNKNOWN:
    default:	
	return true;
    }
}

//////////////////////////////////////////////////////////////////////////////

extern "C"
Device *
instantiate(const DeviceInfo& info)
{
    // H/W inventory must say it's a tape.

    const inventory_s& inv = info.inventory();
    if (inv.inv_class != INV_TAPE || inv.inv_type != INV_SCSIQIC)
	return NULL;

    // Must be a SCSI device.

    const SCSIAddress *sa = info.address().as_SCSIAddress();
    if (!sa)
	return NULL;

    // SCSI LUN must be zero;

    if (sa->lun() != 0)
	return NULL;

    if (TPDAT == inv.inv_state)		// DATs are done in dev_dat.C
	return NULL;

    // All tests passed -- create TapeDevice.

    return new TapeDevice(info, inv.inv_state);
}
