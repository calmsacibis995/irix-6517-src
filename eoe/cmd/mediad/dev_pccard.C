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
#include <alloca.h>
#include <mediad.h>

#include "DeviceInfo.H"
#include "Device.H"
#include "DSReq.H"
#include "Log.H"
#include "PartitionAddress.H"
#include "SCSIAddress.H"

#define	INFO_SIZE	(14)

class	PCCardDevice: public Device {

public:
	PCCardDevice(const DeviceInfo &);
	const	char	*short_name()	const	{ return "pccard"; };
	const	char	*full_name()	const	{ return "pccard"; };
	const	char	*ftr_name()	const	{ return "pccard"; };
	const	char	*dev_name(FormatIndex, int partno) const;

	// Device capabilities

	virtual	int	features();

	// Media presence support

	virtual	int	is_media_present();
	virtual int	eject();
	virtual int	lock_media();
	virtual	int	unlock_media();

	// Media access

	int	capacity();
	int	sector_size(FormatIndex );
	bool	is_write_protected();
	int	read_data(char *, __uint64_t, unsigned, unsigned);
private:
	enum	{ CARD_INFO = 0xC0, MEDIA_REMOVE = 0x1E };
	enum	Brand	{ GENERIC, ADTRON };
	enum States { NORMAL, EXPECTING_EJECT };

        States  state;
	Brand	_brand;
	bool	_supports_sw_lock;
	bool	_supports_sw_eject;
	int	_blksize;
	DSReq	_dsr;
	char	_dksc_name[128];

	unsigned char	_ctlr;
	unsigned char 	_id;
	unsigned char	_lun;
};

const	char	Adtron[] = "ADTRON  SDDS";

/////////////////////////////////////////////////////////////////////////////

PCCardDevice::PCCardDevice(const DeviceInfo &info):
	Device(info),
	_brand(GENERIC),
	_supports_sw_eject(false),
	_supports_sw_lock(false),
	_dsr(*info.address().as_SCSIAddress())
{
	const	SCSIAddress	*sa = info.address().as_SCSIAddress();

	_ctlr	= sa->ctlr();
	_id	= sa->id();
	_lun	= sa->lun();

	if (_lun)
	     sprintf(_dksc_name, "/dev/rdsk/dks%dd%dl%dvol", _ctlr, _id, _lun);
	else sprintf(_dksc_name, "/dev/rdsk/dks%dd%dvol", _ctlr, _id);

	const	char	*inquiry = info.inquiry();
	const	char	*vendor	 = inquiry+8;

	if (!strncasecmp(vendor, Adtron, strlen(Adtron)))
		_brand = ADTRON;
	state = NORMAL;
}

const	char *
PCCardDevice::dev_name(FormatIndex fi, int partno) const
{
	if( fi == FMT_EFS || fi == FMT_XFS)
        {
                static char path[128];
                const SCSIAddress *sa = address().as_SCSIAddress();

                assert(partno != PartitionAddress::WholeDisk);

		if (sa->lun() == 0){
		    if (partno == 8)
			(void) sprintf(path, "/dev/dsk/dks%ud%uvh",
				       sa->ctlr(), sa->id());
		    else
			(void) sprintf(path, "/dev/dsk/dks%ud%us%u",
				       sa->ctlr(), sa->id(), partno);
                	return (path);
		}
		else {
		    if (partno == 8)
			(void) sprintf(path, "/dev/dsk/dks%ud%ul%uvh",
				       sa->ctlr(), sa->id(), sa->lun());
		    else
			(void) sprintf(path, "/dev/dsk/dks%ud%ul%us%u",
				       sa->ctlr(), sa->id(), sa->lun(), partno);
			return (path);
		}
        }
	else	return (_dksc_name);
}

int
PCCardDevice::features()
{
	int	add 	= 0;
	int	remove  = (FEATURE_SW_LOCK | FEATURE_SW_EJECT);
	
	return (feature_set(add, remove));
}

int
PCCardDevice::is_media_present()
{
    dsreq       *dsp = _dsr;
    char	*info = (char *)alloca(INFO_SIZE);


    if (!dsp)
        return -1;

    if (state == NORMAL){
        // We are in the state: NORMAL
        // Here, we merely issue a 'testunitready'
        // and use its return status to determine
        // presence or absence of media in drive

        testunitready00(dsp);
        return (STATUS(dsp) == STA_GOOD);

    }
    else {

        // We are in the state: EXPECTING_EJECT
        // In order to find out if media has been
        // changed, we need to use the device specific
        // command: 0xC0, which lets us obtain card
        // information, using which we find out if the
        // media has been changed or not

        bzero(info, INFO_SIZE);
        fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp), CARD_INFO,
                  (_lun << 5), 0, B2(INFO_SIZE), 0);
        filldsreq(dsp, (uchar_t *) info, INFO_SIZE, DSRQ_READ);
        doscsireq(getfd(dsp), dsp);

        if (STATUS(dsp) == STA_GOOD){
                if (info[0] & 0x04){
                        // The user seems to have physically
                        // ejected the media. Make a transition
                        // to the NORMAL state now. Normally this
                        // event will occur as soon as user removes
                        // media from the drive.

                        state = NORMAL;
                }
        }
        return (false);
    }
}

int
PCCardDevice::eject()
{
        int     error;

        switch (_brand){
        case    ADTRON:
                // Adtron always returns a good status after this
                error = _dsr.g0cmd(MEDIA_REMOVE, (_lun << 5), 0, 0, 0, 0);
                break;
        default:
                // All other vendors, don't know yet
                error = _dsr.g0cmd(MEDIA_REMOVE, (_lun << 5), 0, 0, 0, 0);
                break;
        }
        state = EXPECTING_EJECT;

        if (error != 0) error = RMED_ENODISC;
        else error = RMED_CONTEJECT;
        // If no error occurred, send back code RMED_CONTEJECT, which
        // means to use hardware eject.  If an error did occur, send
        // back RMED_ENODISK, which is the best guess of the error.
        
        return (error);
}

int
PCCardDevice::lock_media()
{
	// No real lock
	return (0);
}

int
PCCardDevice::unlock_media()
{
	// No real unlock
	return (0);
}

int
PCCardDevice::capacity()
{
	dsreq	*dsp = _dsr;
	char	*block_sz;
	char	*num_blocks;
	char	*info = (char *)alloca(INFO_SIZE);
	
	if (!dsp)
		return -1;

	bzero(info, INFO_SIZE);
	fillg1cmd(dsp, (uchar_t *) CMDBUF(dsp), G1_RCAP, (_lun << 5),
		  0, 0, 0, 0, 0, 0, 0, 0);
	filldsreq(dsp, (uchar_t *) info, 8, DSRQ_READ | DSRQ_SENSE);
	doscsireq(getfd(dsp), dsp);
	if (STATUS(dsp) != STA_GOOD)
		return -1;

	num_blocks = &info[0];
	block_sz   = &info[4];
	_blksize   = V4(block_sz);

	return ((int) (V4(block_sz)*V4(num_blocks)));
}

bool
PCCardDevice::is_write_protected()
{
	dsreq	*dsp = _dsr;
	char	*info = (char *)alloca(INFO_SIZE);

	if (!dsp)
		return -1;
	
	bzero(info, INFO_SIZE);
	fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp), CARD_INFO, (_lun << 5),
	 	  0, 0, INFO_SIZE, 0);	
	filldsreq(dsp, (uchar_t *) info, INFO_SIZE, DSRQ_READ);
	doscsireq(getfd(dsp), dsp);
	if (STATUS(dsp) != STA_GOOD)
		return -1;
	
	if (info[0] & 0x80)
		return (true);
	else 	return (false);
}

int
PCCardDevice::sector_size(FormatIndex /* index */ )
{
	dsreq	*dsp = _dsr;
	char	*block_sz;
	char	*info = (char *)alloca(INFO_SIZE);

	if (!dsp)
		return -1;

	bzero(info, INFO_SIZE);
	fillg1cmd(dsp, (uchar_t *) CMDBUF(dsp), G1_RCAP, (_lun << 5),
		  0, 0, 0, 0, 0, 0, 0, 0);
	filldsreq(dsp, (uchar_t *) info, 8, DSRQ_READ | DSRQ_SENSE);
	doscsireq(getfd(dsp), dsp);
	if (STATUS(dsp) != STA_GOOD)
		return -1;

	block_sz = &info[4];
	_blksize = V4(block_sz);

	return ((int) _blksize);
}

int
PCCardDevice::read_data(char *buffer,
			__uint64_t start_sector,
			unsigned nsectors,
			unsigned sectorsize)
{
        assert(start_sector < 1LL << 32);
        assert(nsectors < 1 << 16);

    	dsreq *dsp = _dsr;
    	if (!dsp)
        	return -1;

	fillg1cmd(dsp, (uchar_t *) CMDBUF(dsp),
		  G1_READ, 0, B4(start_sector), 0, B2(nsectors), 0);
    	filldsreq(dsp, (uchar_t *) buffer,
                  nsectors * sectorsize, DSRQ_READ | DSRQ_SENSE);
    	dsp->ds_time = 60 * 1000;           // 60 seconds
    	(void) doscsireq(getfd(dsp), dsp);

    	return STATUS(dsp);
}

/////////////////////////////////////////////////////////////////////////////

extern "C" 
Device *
instantiate(const DeviceInfo &info)
{
	const	SCSIAddress	*sa  = info.address().as_SCSIAddress();

	if (!sa)
		return (NULL);
	
	const	char	*inquiry = info.inquiry();
	if (!inquiry)
		return (NULL);

	const	char	*vendor = inquiry+8;
	if (strncasecmp(vendor, Adtron, strlen(Adtron)))
		return (NULL);
	return (new PCCardDevice(info));	
}
