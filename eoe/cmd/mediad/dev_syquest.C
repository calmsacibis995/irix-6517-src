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

// A note on ejects in SQ5110/SQ5110C
// These two older syquest drives don't support software ejects 
// in the real sense. Although the Allow Media Removal (0x1B) 
// command can be issued, media isn't physically ejected. In order
// to prevent re-mounting of the same media immediately after a 
// software eject, we've defined two states: NORMAL and EXPECTING_EJECT
// We start off in the NORMAL state. On software ejects, we move into EXPECTING
// EJECT state. In NORMAL state, we merely use testunitready to see if media 
// is present. In EXPECTING_EJECT state, we see if the eject button has been 
// depressed. If eject button has been depressed, we move into the NORMAL state
// once again and query if media is present. If eject button has not been
// depressed, we pretend that there isn't media present, and this goes on
// until the media is physically ejected and shoved back in.

#define	SQ5110	"SyQuest SQ5110"		// Eject unsupported
#define	SQ5110C "SyQuest SQ5110C"		// Eject unsupported
#define	SQ3105S	"SyQuest SQ3105S"		// Eject supported
#define	SQ3270S "SyQuest SQ3270S"		// Eject supported
#define	SQ5200C "SyQuest SQ5200C"		// Eject supported

class	SyQuestDevice:public Device {

public:
	SyQuestDevice(const DeviceInfo &);
	const	char	*short_name()	const	{ return "syquest"; }
	const	char	*full_name()	const	{ return "syquest"; }
	const	char	*ftr_name()	const	{ return "syquest"; }
	const	char	*dev_name(FormatIndex, int partno) const;

	// Device capabilities

	virtual	int	features();

	// Media presence support

	virtual	int	is_media_present();
	virtual	int	eject();
	virtual	int	lock_media();
	virtual	int	unlock_media();

	// Media access

	int	capacity();
	int	sector_size(FormatIndex);
	bool	is_write_protected();
	int	read_data(char *, __uint64_t, unsigned, unsigned);

private:
	int	is_eject_on();

	enum	States	{ NORMAL, EXPECTING_EJECT };
	enum	Brand	{ GENERIC, SYQUEST };

	Brand	_brand;
	bool	_supports_sw_lock;
	bool	_supports_sw_eject;
	int	_blksize;
	DSReq	_dsr;
	char	_flavour[128];
	char	_dksc_name[128];

	States	state;
	unsigned char	_ctlr;
	unsigned char	_id;

};

const	char	SyQuest[] = "SyQuest";

///////////////////////////////////////////////////////////////////////////////

SyQuestDevice::SyQuestDevice(const DeviceInfo &info):
	Device(info),
	_brand(GENERIC),
	_supports_sw_eject(true),
	_supports_sw_lock(true),
	_dsr(*info.address().as_SCSIAddress())
{
	const	SCSIAddress	*sa = info.address().as_SCSIAddress();

	_ctlr	= sa->ctlr();
	_id	= sa->id();

	sprintf(_dksc_name, "/dev/rdsk/dks%dd%dvol", _ctlr, _id);

	const	char	*inquiry = info.inquiry();
	const	char	*vendor  = inquiry+8;

	if (!strncasecmp(vendor, SyQuest, strlen(SyQuest)))
		_brand = SYQUEST;
	strcpy(_flavour, vendor);
	state = NORMAL;
}

const	char *
SyQuestDevice::dev_name(FormatIndex fi, int partno) const
{
        if (fi == FMT_EFS || fi == FMT_XFS)
        {
                static char path[128];
                const SCSIAddress *sa = address().as_SCSIAddress();

                assert(partno != PartitionAddress::WholeDisk);
		if (partno == 8)
		    (void) sprintf(path, "/dev/dsk/dks%ud%uvh",
				   sa->ctlr(), sa->id());
		else
		    (void) sprintf(path, "/dev/dsk/dks%ud%us%u",
				   sa->ctlr(), sa->id(), partno);
                return path;
        }
        else
                return (_dksc_name);
}

int
SyQuestDevice::features()
{
	return (feature_set(0, 0));
}

int
SyQuestDevice::is_media_present()
{
        dsreq   *dsp = _dsr;

        if (!dsp)
                return -1;

	if (state == NORMAL){
		// Normal operation
		// Merely perform a Mode sense
       		testunitready00(dsp);
		return (STATUS(dsp) == STA_GOOD);
	}
	else {
		// Expecting an eject
		// Once we detect an eject, we
		// then go into the normal state
		// Until that, we pretend there's
		// no media
		if (is_eject_on()){
			// Eject has been depressed
			// We go into normal state
			state = NORMAL;
			return (0);
		}
		else {
			// Eject hasn't been depressed
			// We keep pretending there's no media
			return (0);
		}
	}
}

int
SyQuestDevice::is_eject_on()
{
#define PARAMS_SIZE 11
	uchar_t	*params = (uchar_t *)alloca(PARAMS_SIZE);
	dsreq	*dsp = _dsr;

	if (!dsp)
		return (0);

	bzero(params, PARAMS_SIZE);
	fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp), 0x03,
                  0, 0, 0, PARAMS_SIZE, 0);
        filldsreq(dsp, params, PARAMS_SIZE, DSRQ_READ|DSRQ_SENSE);
        doscsireq(getfd(dsp), dsp);

	if (STATUS(dsp) != STA_GOOD)
		return 0;

	if (params[8] & 0x20)
                return (1);		// Eject Button Depressed
        else 	return (0);		// Eject Button Not Depressed
}

int
SyQuestDevice::eject()
{
	dsreq	*dsp = _dsr;

	Log::debug("SyQuestDevice::eject()");

	if (!strncasecmp(_flavour, SQ5110,  strlen(SQ5110)) ||
	    !strncasecmp(_flavour, SQ5110C, strlen(SQ5110C))){

		// These flavours don't support LoEject bit
		// We go into a state wherein we expect eject 
		// button to be depressed

        	fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp), 0x1E,
                          0, 0, 0, 0, 0);
        	filldsreq(dsp, (uchar_t *) NULL, 0, DSRQ_READ);
        	doscsireq(getfd(dsp), dsp);

		if (STATUS(dsp) != STA_GOOD)
			return RMED_ECANTEJECT;

		state = EXPECTING_EJECT;
		return RMED_CONTEJECT;
	}
	else {
		// These flavours do support LoEject bit

        	fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp), 0x1B,
                          0, 0, 0, 0x02, 0);
        	filldsreq(dsp, (uchar_t *) NULL, 0, DSRQ_READ);
        	doscsireq(getfd(dsp), dsp);
                if (STATUS(dsp) == STA_GOOD) return RMED_NOERROR;
		else return RMED_ECANTEJECT;
	}
}

int
SyQuestDevice::lock_media()
{
	dsreq	*dsp = _dsr;

	if (!dsp)
		return -1;

	fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp), 0x1E,
                  0, 0, 0, 0x01, 0);
        filldsreq(dsp, (uchar_t *) NULL, 0, DSRQ_READ);
        doscsireq(getfd(dsp), dsp);

	return (STATUS(dsp));
}

int
SyQuestDevice::unlock_media()
{
	dsreq	*dsp = _dsr;
	
	if (!dsp)
		return -1;

        fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp), 0x1E,
                 0, 0, 0, 0, 0);
        filldsreq(dsp, (uchar_t *) NULL, 0, DSRQ_READ);
        doscsireq(getfd(dsp), dsp);

	return (STATUS(dsp));
}

int
SyQuestDevice::capacity()
{
#define INFO_SIZE 14
        dsreq   *dsp = _dsr;
        char    *block_sz;
        char    *num_blocks;
        char    *info = (char *)alloca(INFO_SIZE);
	
        if (!dsp)
                return -1;

        bzero(info, INFO_SIZE);
        fillg1cmd(dsp, (uchar_t *) CMDBUF(dsp), G1_RCAP, 0,
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
SyQuestDevice::is_write_protected()
{
#undef  PARAMS_SIZE
#define PARAMS_SIZE (12+8)
        dsreq   *dsp = _dsr;
        uchar_t *params = (uchar_t *)alloca(PARAMS_SIZE);

        if (!dsp)
                return -1;
        bzero(params, PARAMS_SIZE);
        fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp), G0_MSEN,
                  0, 1, 0, PARAMS_SIZE, 0);
        filldsreq(dsp, params, PARAMS_SIZE, DSRQ_READ | DSRQ_SENSE);
        doscsireq(getfd(dsp), dsp);
	if (STATUS(dsp) != STA_GOOD)
		return -1;

        if (params[2] & 0x80)
                return (true);
        else    return (false);
}

int
SyQuestDevice::sector_size(FormatIndex /* index */ )
{
        dsreq   *dsp = _dsr;
        char    *block_sz;
        char    *info = (char *)alloca(INFO_SIZE);

        if (!dsp)
                return -1;

        bzero(info, INFO_SIZE);
        fillg1cmd(dsp, (uchar_t *) CMDBUF(dsp), G1_RCAP, 0,
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
SyQuestDevice::read_data(char *buffer,
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
        dsp->ds_time = 60 * 1000;           
        (void) doscsireq(getfd(dsp), dsp);

        return STATUS(dsp);
}

///////////////////////////////////////////////////////////////////////////////

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
	if (strncmp(vendor, SyQuest, strlen(SyQuest)))
		return (NULL);
	return (new SyQuestDevice(info));
}
