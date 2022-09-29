#ifndef Device_included
#define Device_included

#include <sys/invent.h>
#include <sys/types.h>

#include "bool.H"

#include "DeviceAddress.H"
#include "Enumerable.H"
#include "FormatIndex.H"

class Config;
class DeviceDSO;
class DeviceInfo;
class Media;

//  Abstract base class for device instances.

class Device : private Enumerable {

public:

    //  Device features.  Bits in least significant byte
    //  are filesystem types that most devices will support.
    //  Bits in 2nd byte are filesystems types that most devices
    //  won't support.  Bits in 3rd byte are features that
    //  most devices support.  Bits in 4th byte are features
    //  that most devices won't support.  So the "typical"
    //	device would return 0x00FF00FF, in hopes of being
    //  compatible with mediad flags defined in the future.
    //
    //  Use the feature_set() function to compose a device's
    //  feature set.  Pass in the uncommon features the
    //  device does support and the common features it
    //	doesn't support.

    enum FeatureSet {

	//  Common filesystems

	FILESYS_RAW = 1 << FMT_RAW,
	FILESYS_EFS = 1 << FMT_EFS,
	FILESYS_HFS = 1 << FMT_HFS,
	FILESYS_DOS = 1 << FMT_DOS,
	FILESYS_XFS = 1 << FMT_XFS,

	//  Uncommon filesystems

	FILESYS_ISO9660 = 1 << FMT_ISO,
	FILESYS_CDDA    = 1 << FMT_CDDA,
	FILESYS_AUDIO   = 1 << FMT_AUDIO, // non-CD audio

	//  Common features

	FEATURE_SW_LOCK = 1 << 16,	// can media be locked in?
	FEATURE_SW_EJECT = 1 << 17,	// can it eject under sw control?
	FEATURE_MOUNTABLE = 1 << 18,	// can we mount filesystems?
	FEATURE_FAST_POLL = 1 << 19,	// should we poll faster after eject?

	//  Uncommon features

	FEATURE_RDONLY = 1 << 24,
	FEATURE_EMPTY_EJECTABLE = 1 << 25 // can we eject without media?

    };

    int feature_set(int add, int remove) { return 0x00FF00FF & ~remove | add; }

    Device(const DeviceInfo&);
    virtual ~Device();

    const DeviceAddress& address() const { return _address; }
    const inventory_t& inventory() const { return _inv_record; }
    DeviceDSO *dso()			{ return _dso; }
    bool is_ignored() const		{ return _is_ignored; }
    bool is_shared() const		{ return _is_shared; }

    //  Each device has four names.

    virtual const char *short_name() const = 0;// e.g., "CDROM"
    virtual const char *full_name() const = 0;// e.g., "CD-ROM"
    virtual const char *ftr_name() const = 0;// e.g., "cdrom"
    virtual const char *dev_name(FormatIndex, int partno) const = 0;
					// e.g., "/dev/scsi/sc0d4l0"
    inline const char *name() const;	// e.g., "CDROM2"

    void set_name(const char *);
    void set_ignored(bool);
    void set_shared(bool);

    //  Device capabilities

    virtual int features() = 0;

    bool filesys_efs()		{ return (features() & FILESYS_EFS) != 0; }
    bool filesys_hfs()		{ return (features() & FILESYS_HFS) != 0; }
    bool filesys_dos()		{ return (features() & FILESYS_DOS) != 0; }
    bool filesys_xfs()		{ return (features() & FILESYS_XFS) != 0; }

    bool filesys_iso9660()	{ return (features() & FILESYS_ISO9660) != 0; }
    bool filesys_cdda()		{ return (features() & FILESYS_CDDA) != 0; }

    bool feature_sw_lock()	{ return (features() & FEATURE_SW_LOCK) != 0; }
    bool feature_sw_eject()    { return (features() & FEATURE_SW_EJECT) != 0; }
    bool feature_mountable()  { return (features() & FEATURE_MOUNTABLE) != 0; }
    bool feature_fast_poll()  { return (features() & FEATURE_FAST_POLL) != 0; }

    bool feature_rdonly()	{ return (features() & FEATURE_RDONLY) != 0; }
    bool feature_empty_ejectable()
			{ return (features() & FEATURE_EMPTY_EJECTABLE) != 0; }

    // 	Media presence support

    virtual int suspend_monitoring();
    virtual int resume_monitoring();
    virtual int is_media_present() = 0;
    virtual int eject() = 0;
    virtual int lock_media() = 0;	// returns 0 on success
    virtual int unlock_media() = 0;

    //  Media access

    virtual int capacity() = 0;
    virtual int sector_size(FormatIndex) = 0;
    virtual bool is_write_protected() = 0;
    virtual bool has_audio_data();
    virtual int read_data(char *buf,
			  __uint64_t start_sect,
			  unsigned nsect,
			  unsigned secsize) = 0;

    //  Device enumeration

    ENUMERATION_METHODS(Device, devices);
    static Device *at(const DeviceAddress&);

private:

    //  Instance Variables

    DeviceDSO *_dso;
    const DeviceAddress _address;
    inventory_t _inv_record;
    char *_name;
    bool _is_ignored;
    bool _is_shared;
    Config *_config;

    //  Class Variables

    static Enumerable::Set devices;

    //  Class Methods

    static void config_proc(Config&, void *closure);

    Device(const Device&);		// Do not copy
    void operator = (const Device&);	//  or assign.

};

inline const char *
Device::name() const
{
    return _name;
}

#endif /* !Device_included */
