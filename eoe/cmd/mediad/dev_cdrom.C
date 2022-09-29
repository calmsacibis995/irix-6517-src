#include <assert.h>
#include <bstring.h>
#include <dslib.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/scsi.h>
#include <sys/buf.h>
#include <sys/dvh.h>
#include <sys/iobuf.h>
#include <sys/dksc.h>
#include <sys/dkio.h>
#include <unistd.h>
#include <alloca.h>
#include <mediad.h>

#include "DeviceInfo.H"
#include "Device.H"
#include "DSReq.H"
#include "Log.H"
#include "PartitionAddress.H"
#include "SCSIAddress.H"

class CDROMDevice : public Device {

public:

    CDROMDevice(const DeviceInfo&);

    const char *short_name() const	{ return "CDROM"; }
    const char *full_name() const	{ return "CD-ROM"; }
    const char *ftr_name() const	{ return "cdrom"; }
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
    int sector_size(FormatIndex);
    bool is_write_protected()		{ return true; }
    bool has_audio_data();
    int read_data(char *, __uint64_t, unsigned, unsigned);

private:

    //  Device-specific constants

    enum Brand { GENERIC, TOSHIBA, TOSHIBA2, TOSHIBA5401, SONY, MATSHITA };
    enum { DATA_SECTOR_SIZE = 512 };
    enum { ISO_SECTOR_SIZE = 2048 };
    enum { AUDIO_SECTOR_SIZE = 2048 };
    enum { G6_READINFO = 0xc7 };	// Toshiba READ TRACK INFO cmd byte

    //  Device-specific method

    void act_like_a_cdrom();
    void get_blksize();
    int set_blksize(int);

    Brand _brand;
    int _blksize;
    DSReq _dsr;

};

const unsigned int STA_RETRY = ~ (unsigned int) 0;

//////////////////////////////////////////////////////////////////////////////

CDROMDevice::CDROMDevice(const DeviceInfo& info)
: Device(info),
  _brand(GENERIC),
  _blksize(0),
  _dsr(*info.address().as_SCSIAddress())
{
    const char Toshiba[] = "TOSHIBA CD";
    const char Toshiba5401[] = "TOSHIBA CD-ROM XM-5401";
    const char Sony[] = "SONY    CD";
    const char Matshita[] = "MATSHITACD";
    const char *inquiry = info.inquiry();
    const char *vendor = inquiry + 8;
    if (!strncasecmp(vendor, Toshiba, strlen(Toshiba)))
    {   if ((inquiry[2] & 0x07) != 2)
	    _brand = TOSHIBA;
	else if (!strncasecmp(vendor, Toshiba5401, strlen(Toshiba5401)))
	    _brand = TOSHIBA5401;
	else
	    _brand = TOSHIBA2;
    }
    else if (!strncasecmp(vendor, Sony, strlen(Sony)))
	_brand = SONY;
    else if (!strncasecmp(vendor, Matshita, strlen(Matshita)))
	_brand = MATSHITA;
    get_blksize();
}

//  CD-ROM's device name is complicated.  EFS and XFS use the
//  dksc block device driver (/dev/dsk/dksXdYsZ); HFS and ISO 9660
//  use the dksc raw driver (/dev/rdsk/dksXdYsZ); others (audio)
//  use the scsi driver (/dev/scsi/scXdYlZ).

const char *
CDROMDevice::dev_name(FormatIndex fi, int partno) const
{
    static char path[128];
    const SCSIAddress *sa = address().as_SCSIAddress();

    if (fi == FMT_EFS || fi == FMT_XFS)
    {
	assert(partno != PartitionAddress::WholeDisk);
	if (partno == 8)
	    (void) sprintf(path, "/dev/dsk/dks%ud%uvh", sa->ctlr(), sa->id());
	else
	    (void) sprintf(path, "/dev/dsk/dks%ud%us%u",
			   sa->ctlr(), sa->id(), partno);
	return path;
    }
    else if (fi == FMT_HFS || fi == FMT_ISO)
	(void) sprintf(path, "/dev/rdsk/dks%ud%uvol", sa->ctlr(), sa->id());
    else
	(void) sprintf(path, "/dev/scsi/sc%ud%ul%u",
		       sa->ctlr(), sa->id(), sa->lun());
    return path;
}

int
CDROMDevice::features()
{
    return feature_set(FILESYS_ISO9660 | FILESYS_CDDA |
		       FEATURE_RDONLY | FEATURE_EMPTY_EJECTABLE,
		       FILESYS_DOS);
}

int
CDROMDevice::is_media_present()
{
    dsreq *dsp = _dsr;
    if (!dsp)
	return -1;
    act_like_a_cdrom();
    testunitready00(dsp);
    return STATUS(dsp) == STA_GOOD;
}

int
CDROMDevice::eject()
{
// Always allow eject -- it's handy to be able to eject an empty drawer.
//    if (!is_media_present())
//	return -1;
    int error = 0;
    switch (_brand)
    {
    case TOSHIBA:
    case TOSHIBA2:
    case TOSHIBA5401:
	error = _dsr.g1cmd(0xc4, 1, 0, 0, 0, 0, 0, 0, 0, 0);
        break;
    default:
	error = _dsr.g0cmd(0x1b, 1, 0, 0, 2, 0);
        break;
    }

    if (error) {
        return RMED_ECANTEJECT;
    }
    else {
        return RMED_NOERROR;
    }
}

int
CDROMDevice::lock_media()
{
    return _dsr.g0cmd(0x1e, 0, 0, 0, 1, 0);
}

int
CDROMDevice::unlock_media()
{
    return _dsr.g0cmd(0x1e, 0, 0, 0, 0, 0);
}

int
CDROMDevice::capacity()
{
    dsreq *dsp = _dsr;
    unsigned char data[8];
    bzero(data, sizeof data);
    int rc = readcapacity25(dsp, (caddr_t) data, sizeof data, 0, 0, 0);
    if (rc != STA_GOOD)
	return rc;
    return V4(data);
}

int
CDROMDevice::sector_size(FormatIndex index)
{
    if (index == FMT_ISO)
	return ISO_SECTOR_SIZE;
    else if (index == FMT_CDDA)
	return AUDIO_SECTOR_SIZE;
    else
	return DATA_SECTOR_SIZE;
}

bool
CDROMDevice::has_audio_data()
{
    const uchar_t S2_TOC = 0x43;	// SCSI 2 TOC command

    dsreq *dsp = _dsr;
    switch (_brand)
    {
    case TOSHIBA:
    {
#define INFO_SIZE 4
	uchar_t *info = (uchar_t *)alloca(INFO_SIZE);
	fillg1cmd(dsp, (uchar_t *) CMDBUF(dsp),
		  G6_READINFO, 0, 0, 0, 0, 0, 0, B2(sizeof info), 0);
	filldsreq(dsp, info, INFO_SIZE, DSRQ_READ | DSRQ_SENSE);
	if (doscsireq(getfd(dsp), dsp) != STA_GOOD)
	    return false;
	fillg1cmd(dsp, (uchar_t *) CMDBUF(dsp),
		  G6_READINFO, 2, info[0], 0, 0, 0, 0, B2(INFO_SIZE), 0);
	filldsreq(dsp, info, INFO_SIZE, DSRQ_READ | DSRQ_SENSE);
	if (doscsireq(getfd(dsp), dsp) != STA_GOOD)
	    return false;
	return (info[3] & 0x4) == 0;
    }

    default:			// SONY, MATSHITA, GENERIC and newer TOSHIBA
	/*
	 * Stolen from cdplayer in libcdaudio to make SONY CDROMs
	 * detect music CDs correctly.
	 */
	struct toc_header {
	    unsigned short  length;
	    unsigned char   first;
	    unsigned char   last;
	};

	struct toc_entry {
	    unsigned char   reserved;
	    unsigned char   control;
	    unsigned char   track;
	    unsigned char   reserved2;
	    unsigned char   reserved3;
	    unsigned char   min;
	    unsigned char   sec;
	    unsigned char   frame;
	};

	struct toc {
	    struct toc_header   head;
	    struct toc_entry    tracks[1];
	};

	toc track_info;
	bzero(&track_info, sizeof track_info);
	fillg1cmd(dsp, (uchar_t *) CMDBUF(dsp),
		  S2_TOC, 2, 0, 0, 0, 0, 0, B2(sizeof track_info), 0);
	filldsreq(dsp, (uchar_t *) &track_info, sizeof track_info,
		  DSRQ_SENSE | DSRQ_READ);
	if (doscsireq(getfd(dsp), dsp) != STA_GOOD)
	    return false;
	return (track_info.tracks[0].control & 0x4) == 0;
    }
}

int
CDROMDevice::read_data(char *buffer,
		       __uint64_t start_sector,
		       unsigned nsectors,
		       unsigned sectorsize)
{
    assert(start_sector < 1LL << 32);
    assert(nsectors < 1 << 16);

    act_like_a_cdrom();
    set_blksize(sectorsize);
    dsreq *dsp = _dsr;
    if (!dsp)
	return -1;
    bzero(buffer, nsectors * sectorsize);
    fillg1cmd(dsp, (uchar_t *) CMDBUF(dsp),
	      G1_READ, 0, B4(start_sector), 0, B2(nsectors), 0);
    filldsreq(dsp, (uchar_t *) buffer,
	      nsectors * sectorsize, DSRQ_READ | DSRQ_SENSE);
    dsp->ds_time = 60 * 1000;		// 60 seconds
    int status = doscsireq(getfd(dsp), dsp);

    //  Set the sector size back to 512 -- other programs expect to find
    //  that size.  (specifically, the dksc driver)

    set_blksize(DATA_SECTOR_SIZE);
    return status;
}

//////////////////////////////////////////////////////////////////////////////

void
CDROMDevice::act_like_a_cdrom()
{
    if (_brand == TOSHIBA || _brand == TOSHIBA2 || _brand == TOSHIBA5401)
	(void) _dsr.g1cmd(0xc9, 0, 0, 0, 0, 0, 0, 0, 0, 0);
}

void
CDROMDevice::get_blksize()
{
#define PARAMS_SIZE (12 + 8)
    dsreq *dsp = _dsr;
    uchar_t *params = (uchar_t *)alloca(PARAMS_SIZE);

    for (int retry = 0; retry < 10; retry++)
    {
	switch (_brand)
	{
	case TOSHIBA:
	    modesense1a(dsp, (caddr_t) params, 12, 0, 0, 0);
	    break;

	default:

	    //  Amazingly enough, modesense fails if you don't request
	    //  any page code information

	    fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp),
		      G0_MSEN, 0, 1, 0, PARAMS_SIZE, 0);
	    filldsreq(dsp, params, PARAMS_SIZE, DSRQ_READ | DSRQ_SENSE);
	    (void) doscsireq(getfd(dsp), dsp);
	    break;
	}
	if (STATUS(dsp) == STA_GOOD)
	{   _blksize = V4(params + 8);
	    return;
	}
	else if (STATUS(dsp) == STA_CHECK)
	    continue;
	else if (STATUS(dsp) == STA_RETRY)
	{   sleep(1);
	    continue;
	}
	else
	{   _blksize = -1;
	    return;
	}
    }
}

int
CDROMDevice::set_blksize(int new_size)
{
    if (_blksize != new_size)
    {
	const SCSIAddress *sa = address().as_SCSIAddress();
	assert(sa != NULL);
	char dksc_path[PATH_MAX];
	sprintf(dksc_path, "/dev/rdsk/dks%dd%dvol", sa->ctlr(), sa->id());
	int dkfd = open(dksc_path, O_RDWR);
	if (dkfd < 0)
	{   Log::perror("can't open CD-ROM \"%s\"", dksc_path);
	    return -1;
	}
	if (ioctl(dkfd, DIOCSELFLAGS, 0x10) != 0)
	{   Log::perror("DIOCSELFLAGS failed on CD-ROM %s", dksc_path);
	    close(dkfd);
	    return -1;
	}
	uchar_t params[12];
	bzero(params, sizeof params);
	params[3] = 0x08;
	MSB4(params + 8, new_size);
	dk_ioctl_data arg = { (caddr_t) &params, sizeof params, 0 };
	if (ioctl(dkfd, DIOCSELECT, &arg) != 0)
	{   Log::perror("DIOCSELECT failed on CD-ROM %s", dksc_path);
	    close(dkfd);
	    return -1;
	}
	_blksize = new_size;
	close(dkfd);
    }
    return 1;
}

//////////////////////////////////////////////////////////////////////////////

extern "C"
Device *
instantiate(const DeviceInfo& info)
{
    const inventory_s& inv = info.inventory();
    if (inv.inv_class != INV_SCSI || inv.inv_type != INV_CDROM)
	return NULL;
    if (!info.address().as_SCSIAddress())
	return NULL;
    if (!info.inquiry())
	return NULL;
    return new CDROMDevice(info);
}
