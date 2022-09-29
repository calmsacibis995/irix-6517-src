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

class DATDevice : public Device {

public:

    DATDevice(const DeviceInfo&);
    ~DATDevice();

    const char *short_name() const	{ return "DAT"; }
    const char *full_name() const	{ return "DAT"; }
    const char *ftr_name() const	{ return "dat"; }
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
    bool has_audio_data();
    int read_data(char *, __uint64_t, unsigned, unsigned);

private:

    //  Device-specific constants

    enum { SECTOR_SIZE = 512 };

    //  Device-specific variables

    int	 _fd_stat;			
    char _rw_path[64];			// Path to rewinding device
    char _nr_path[64];			// Path to nonrewinding device
    char _stat_device[100];		// Path to stat device

};

//////////////////////////////////////////////////////////////////////////////

DATDevice::DATDevice(const DeviceInfo& info)
: Device(info)
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
    else
	sprintf(_rw_path, "/dev/rmt/tps%ud%uv", sa->ctlr(), sa->id());
	
    //  Check whether this tape is /dev/tape.  If not,
    //	generate a /dev/rmt/tps pathname.

    VolumeAddress devtapenr("/dev/nrtape");
    const PartitionAddress *tapenr_pa = devtapenr.partition(0);
    if (tapenr_pa != NULL && tapenr_pa->device() == da)
	strcpy(_nr_path, "/dev/nrtape");
    else
	sprintf(_nr_path, "/dev/rmt/tps%ud%unrv", sa->ctlr(), sa->id());

    sprintf(_stat_device, "/dev/rmt/tps%ud%ustat", sa->ctlr(), sa->id());
    (void) resume_monitoring();
}

DATDevice::~DATDevice()
{
	(void) close(_fd_stat);
}

const char *
DATDevice::dev_name(FormatIndex fi, int partno) const
{
    if (fi == FMT_RAW && partno == PartitionAddress::WholeDisk)
	return _rw_path;
    else
	return _nr_path;
}

int
DATDevice::features()
{
    int add = FILESYS_AUDIO;
    int remove = (0xFF & ~FILESYS_RAW) | FEATURE_MOUNTABLE;
    return feature_set(add, remove);
}

int
DATDevice::suspend_monitoring()
{
    (void) close(_fd_stat);
    _fd_stat = -1;
    return 0;
}

int
DATDevice::resume_monitoring()
{
    _fd_stat = open(_stat_device, O_RDONLY);
    if (_fd_stat < 0)
    {
	Log::perror("can't open DAT device %s", _stat_device);
	return -1;
    }

    if (fcntl(_fd_stat, F_SETFD, FD_CLOEXEC) < 0)
    {
	Log::perror("DAT F_SETFD fcntl failed");
	return -1;
    }

    return 0;
}

int
DATDevice::is_media_present()
{
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
    //   (2) CT_MOTION = 0x20    - in dsreg
    //   (3) CT_ONL    = 0x40    - in dsreg

    if ((mt_status.mt_erreg & (CT_LOADED>>16)) ||
        (mt_status.mt_dsreg & CT_MOTION) ||
        (mt_status.mt_dsreg & CT_ONL)){
          return (true);
    }
    else  return (false);
}

int
DATDevice::eject()
{
    Log::debug("DATDevice::eject()");
    DSReq::close_all();
    int fd = open(_nr_path, O_RDONLY);
    if (fd < 0)
	return errno == EIO ? RMED_NOERROR : RMED_ENODISC;

    struct mtop mt_com;
    bzero(&mt_com, sizeof mt_com);
    mt_com.mt_op = MTUNLOAD;

    if (ioctl(fd, MTIOCTOP, &mt_com) != 0)
    {	if (errno != EAGAIN)
	    Log::perror("DAT device \"%s\"", _nr_path);
	(void) close(fd);
	return RMED_ECANTEJECT;
    }
    (void) close(fd);
    return RMED_NOERROR;
}
    
int
DATDevice::lock_media()
{
    return 0;				// Don't bother locking tapes.

}

int
DATDevice::unlock_media()
{
    return 0;				// No real lock/unlock, just be happy.
}

int
DATDevice::capacity()
{
    assert(0);
    return -1;
}

bool
DATDevice::is_write_protected()
{
    // Use an MTIOCGET ioctl to learn whether the tape is present.

    struct mtget mt_status;
    bzero(&mt_status, sizeof mt_status);
    mt_status.mt_type = MTNOP;
    if (ioctl(_fd_stat, MTIOCGET, &mt_status) != 0)
    {   Log::perror("DAT device \"%s\" MTIOCGET", _stat_device);
	return false;
    }

    // Return true iff the tape is write-protected.

    if (mt_status.mt_dposn & MT_WPROT)
	return true;
    else
	return false;
}

bool
DATDevice::has_audio_data()
{
    // Use an MTIOCGET ioctl to learn whether the tape is audio.

    struct mtget mt_status;
    bzero(&mt_status, sizeof mt_status);
    mt_status.mt_type = MTNOP;
    if (ioctl(_fd_stat, MTIOCGET, &mt_status) != 0)
    {   Log::perror("DAT device \"%s\" MTIOCGET", _stat_device);
	return false;
    }

    // Return true iff the tape is an audio medium.
    // The following code is weird -- I stole it from old mediad.
    // If there's a better definition for the DAT-specific mtget
    // structure, I don't know where it is.

    return ((ulong) mt_status.mt_erreg << 16 & CT_AUD_MED) != 0;
}

int
DATDevice::read_data(char * /* buffer */,
		     __uint64_t /* start_sector */,
		     unsigned /* nsectors */,
		     unsigned /* sectorsize */ )
{
    assert(0);
    return -1;
}

//////////////////////////////////////////////////////////////////////////////

extern "C"
Device *
instantiate(const DeviceInfo& info)
{
    // H/W inventory must say it's a tape.

    const inventory_s& inv = info.inventory();
    if (inv.inv_class != INV_TAPE)
	return NULL;

    // Must be a SCSI device.

    const SCSIAddress *sa = info.address().as_SCSIAddress();
    if (!sa)
	return NULL;

    // SCSI LUN must be zero;

    if (sa->lun() != 0)
	return NULL;

    if (TPDAT != inv.inv_state)
	return NULL;

    // All tests passed -- create DATDevice.

    return new DATDevice(info);
}
