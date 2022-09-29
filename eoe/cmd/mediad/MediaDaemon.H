#ifndef MediaDaemon_included
#define MediaDaemon_included

#include "DeviceLibrary.H"
#include "FormatLibrary.H"

class Config;
class Device;
class DeviceAddress;
struct inventory_s;

class MediaDaemon {

public:

    MediaDaemon();
    ~MediaDaemon();

private:

    //  Instance Variables

    Config *_config;
    DeviceLibrary _device_lib;
    FormatLibrary _format_lib;

    //  Private Instance Methods

    void name_device(Device *);
    Device *create_device(bool is_ignored, const inventory_s&);
    void destroy_device(Device *);

    MediaDaemon(const MediaDaemon&);	// Do not copy
    void operator = (const MediaDaemon&);//  or assign.

    //  Private Class Method

    static void config_proc(Config&, void *); // config file changed callback

};

#endif /* !MediaDaemon_included */
