#ifndef Config_included
#define Config_included

#include "bool.H"
#include "Enumerable.H"

class DeviceAddress;
class FAMEvent;
class FAMonitor;
class FileMonitor;
enum  FormatIndex;
enum  Token;
class VolumeAddress;
struct config;

class Config : private Enumerable {

public:

    typedef void (*ConfigProc)(Config&, void *closure);

    Config(ConfigProc = 0, void *closure = 0);
    ~Config();

    bool device_is_ignored(const DeviceAddress&);
    bool volume_is_ignored(const VolumeAddress&);
    bool device_is_mentioned(const DeviceAddress&);
    bool volume_is_mentioned(const VolumeAddress&);
    bool device_is_shared(const DeviceAddress&);
    unsigned int device_inschk(const DeviceAddress&);
    unsigned int device_rmvchk(const DeviceAddress&);
    const char *mount_dir(const VolumeAddress&);
    const char *mount_options(const VolumeAddress&);
    bool mount_is_shared(const VolumeAddress&);
    bool mount_is_unshared(const VolumeAddress&);

private:
    
    // Instance Variables

    ConfigProc _proc;
    void *_closure;

    // Class Variables

    static const char *config_path;	// name of config file
    static bool initialized;
    static FileMonitor *config_monitor;
    static Enumerable::Set all_configs;

    static void init();
    static FormatIndex match_fstype(const char *);
    static bool parse_device(DeviceAddress *);
    static bool parse_volume(VolumeAddress *);
    static bool parse_line(config *);
    static config *parse_config();
    static int yygetchar();
    static void yyungetchar(int);
    static Token yylex();
    static void yyunlex();		// put the last token back.
    static void error(const char *msg);
    static void change_proc(FAMonitor&, const FAMEvent&, void *);

    ENUMERATION_METHODS(Config, all_configs);

    Config(const Config&);		// Do not copy
    void operator = (const Config&);	//  or assign.

};

#endif /* !Config_included */
