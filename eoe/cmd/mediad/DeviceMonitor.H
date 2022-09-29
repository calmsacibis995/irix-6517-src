#ifndef DeviceMonitor_included
#define DeviceMonitor_included

#include "Enumerable.H"
#include "FormatIndex.H"
#include "Task.H"

class CallBack;
class Config;
class Device;
class DeviceAddress;
class Format;
class FormatLibrary;

class DeviceMonitor : private Enumerable {

public:

    DeviceMonitor(Device *, FormatLibrary&);
    ~DeviceMonitor();

    //  State Reporting

    bool is_media_present() const	{ return _state != NO_MEDIA; }
    bool is_locked() const		{ return _media_locked; }
    bool is_write_protected() const	{ return _write_protected; }
    bool is_ignored() const;
    bool is_suspended() const		{ return _is_suspended; }
    Device *device() const		{ return _device; }

    //  External Transitions

    void set_device(Device *);
    void resume();
    int suspend();
    int eject();

    //  Enumeration

    ENUMERATION_METHODS(DeviceMonitor, monitors);
    static DeviceMonitor *at(const Device *);
    static DeviceMonitor *at(const DeviceAddress&);

private:

    enum State { UNKNOWN, NO_MEDIA, MEDIA };

    //  Instance Variables

    Config *_config;
    Device *_device;
    FormatLibrary& _format_lib;
    State _state;
    bool _is_suspended;
    bool _media_locked;
    bool _write_protected;
    Task _poll_task;
    unsigned int _inschk;
    unsigned int _rmvchk;
    unsigned int _postejectticks;
    unsigned int _currentchk;

    //  Class Variables

    static Enumerable::Set monitors;
    static CallBack *callbacks;

    //  Private Instance Methods

    void reschedule();
    void check_state();
    void handle_insertion();
    void handle_ejection();
    void lock();
    void unlock();
    bool dismount_volumes(bool force);
    void delete_volumes(bool force_dismount);
    void delete_partitions();

    //  Private Class Methods

    static void poll_proc(Task&, void *); // Poll task callback
    static void config_proc(Config&, void *); // Config file changed callback

    DeviceMonitor(const DeviceMonitor&); // Do not copy
    void operator = (const DeviceMonitor&);//  or assign.

};

#endif /* !DeviceMonitor_included */
