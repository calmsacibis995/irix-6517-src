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


class   ZipDevice:public Device {

public:
        ZipDevice(const DeviceInfo &);
        const   char    *short_name()   const   { return "zip"; }
        const   char    *full_name()    const   { return "zip"; }
        const   char    *ftr_name()     const   { return "zip"; }
        const   char    *dev_name(FormatIndex, int partno) const;

        // Device capabilities

        virtual int     features();

        // Media presence support

        virtual int     is_media_present();
        virtual int     eject();
        virtual int     lock_media();
        virtual int     unlock_media();

        // Media access

        int     capacity();
        int     sector_size(FormatIndex);
        bool    is_write_protected();
        int     read_data(char *, __uint64_t, unsigned, unsigned);

private:
        enum    Brand   { GENERIC, ZIP };

        Brand   _brand;
        bool    _supports_sw_lock;
        bool    _supports_sw_eject;
        int     _blksize;
        DSReq   _dsr;
        char    _dksc_name[128];

        unsigned char   _ctlr;
        unsigned char   _id;

};

const   char	Zip[] = "IOMEGA  ZIP 100";

///////////////////////////////////////////////////////////////////////////////

ZipDevice::ZipDevice(const DeviceInfo &info):
        Device(info),
        _brand(GENERIC),
        _supports_sw_eject(true),
        _supports_sw_lock(true),
        _dsr(*info.address().as_SCSIAddress())
{
        const   SCSIAddress     *sa = info.address().as_SCSIAddress();

        _ctlr   = sa->ctlr();
        _id     = sa->id();

        sprintf(_dksc_name, "/dev/rdsk/dks%dd%dvol", _ctlr, _id);

        const   char    *inquiry = info.inquiry();
        const   char    *vendor  = inquiry+8;

        if (!strncasecmp(vendor, Zip, strlen(Zip)))
                _brand = ZIP;
}

const   char *
ZipDevice::dev_name(FormatIndex fi, int partno) const
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
ZipDevice::features()
{
        return (feature_set(0, 0));
}

int
ZipDevice::is_media_present()
{
        dsreq   *dsp = _dsr;

        if (!dsp)
                return -1;

        testunitready00(dsp);
        return (STATUS(dsp) == STA_GOOD);
}

int
ZipDevice::eject()
{
	dsreq	*dsp = _dsr;

        fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp), 0x1B,
                  1, 0, 0, 0, 0);
        filldsreq(dsp, (uchar_t *) NULL, 0, DSRQ_READ);
        doscsireq(getfd(dsp), dsp);
        if (STATUS(dsp) == STA_GOOD) return RMED_NOERROR;
        else return RMED_ECANTEJECT; 
}

int
ZipDevice::lock_media()
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
ZipDevice::unlock_media()
{
        dsreq   *dsp = _dsr;
        
        if (!dsp)
                return -1;
        
        fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp), 0x1E,
                 0, 0, 0, 0, 0);
        filldsreq(dsp, (uchar_t *) NULL, 0, DSRQ_READ);
        doscsireq(getfd(dsp), dsp);

        return (STATUS(dsp));
}

int
ZipDevice::capacity()
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
ZipDevice::is_write_protected()
{
#define PARAMS_SIZE (12+8)
        dsreq   *dsp = _dsr;
	uchar_t	*params = (uchar_t *)alloca(PARAMS_SIZE);

	if (!dsp)
		return -1;
	
        fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp), G0_MSEN, 
		  0, 1, 0, PARAMS_SIZE, 0);
        filldsreq(dsp, params, PARAMS_SIZE, DSRQ_READ | DSRQ_SENSE);
        doscsireq(getfd(dsp), dsp);
	
	if (params[2] & 0x80)
		return (true);
	else	return (false);
}

int
ZipDevice::sector_size(FormatIndex /* index */ )
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
ZipDevice::read_data(char *buffer,
                     __uint64_t start_sector,
                     unsigned nsectors,
                     unsigned sectorsize)
{
	dsreq *dsp = _dsr;

        assert(start_sector < 65536);
        assert(nsectors < 256);

     	testunitready00(dsp);
     	if (STATUS(dsp) != STA_GOOD) 
		return -1;

	// Perform Read 
	
     	fillg0cmd(dsp, (uchar_t *) CMDBUF(dsp), 
		  G0_READ, 
		  0, 
		  B2(start_sector), 
		  B1(nsectors),     
		  0);
     	filldsreq(dsp, (uchar_t *) buffer, nsectors * sectorsize, 
		  DSRQ_READ | DSRQ_SENSE );
     	(void) doscsireq(getfd(dsp), dsp );

	return (STATUS(dsp));
}

///////////////////////////////////////////////////////////////////////////////

extern "C"
Device *
instantiate(const DeviceInfo &info)
{
        const   inventory_s     &inv = info.inventory();
        const   SCSIAddress     *sa  = info.address().as_SCSIAddress();

	if (inv.inv_class != INV_DISK || inv.inv_type != INV_SCSIFLOPPY)
        	return (NULL);

        if (!sa)
                return (NULL);

        const   char    *inquiry = info.inquiry();
        if (!inquiry)
                return (NULL);

        const   char    *vendor = inquiry+8;
        if (strncasecmp(vendor, Zip, strlen(Zip)))
                return (NULL);
        return (new ZipDevice(info));
}








