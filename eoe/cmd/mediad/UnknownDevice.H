#ifndef UnknownDevice_included
#define UnknownDevice_included

#include "Device.H"

class UnknownDevice : public Device {

public:

    UnknownDevice(const DeviceInfo&);
    virtual ~UnknownDevice();

    virtual const char *short_name() const;
    virtual const char *full_name() const;
    virtual const char *ftr_name() const;
    virtual const char *dev_name(FormatIndex, int partno) const;

    virtual int features();

    virtual int is_media_present();
    virtual int eject();
    virtual int lock_media();
    virtual int unlock_media();
    virtual int capacity();
    virtual int sector_size(FormatIndex);
    virtual bool is_write_protected();
    virtual bool has_audio_data();
    virtual int read_data(char *, __uint64_t, unsigned, unsigned);

private:

    enum Type { GENERIC, CDROM, TAPE };
    Type _type;

};

#endif /* !UnknownDevice_included */
