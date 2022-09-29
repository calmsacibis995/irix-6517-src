#ifndef MonitorClient_included
#define MonitorClient_included

#include <rpc/rpc.h>

#include "IOHandler.H"
#include "bool.H"

class Device;
class Event;
class Partition;
class Volume;
class mc_event;
struct mc_device;
struct mc_partition;

class MonitorClient {

public:

    MonitorClient(int fd, bool is_remote);
    ~MonitorClient();

    bool is_remote()			{ return _is_remote; }
    bool wants_forced_unmount()		{ return _wants_forced_unmount; }

    void transmit_world(const mc_event *);

private:

    typedef bool_t (MonitorClient::*EnumFunc)(XDR *, u_int);

    //  Instance Variables

    ReadHandler _handler;
    XDR _xdrs;
    bool _is_remote;
    bool _wants_forced_unmount;

    unsigned int count_partitions(const Partition * = 0);
    unsigned int count_devices(const Device * = 0);
    unsigned int count_volumes(const Volume * = 0);
    Partition *nth_partition(unsigned);
    Device *nth_device(unsigned);
    Volume *nth_volume(unsigned);

    //  XDR stuff

    bool_t xdr_world(XDR *, mc_event *);
    bool_t xdr_enumerate(XDR *, u_int *, EnumFunc);
    bool_t xdr_partition(XDR *, u_int);
    bool_t xdr_device(XDR *, u_int);
    bool_t xdr_volume(XDR *, u_int);
    static bool_t xdr_volpartptrs(XDR *, u_int *, mc_partition ***);
    static bool_t xdr_devpartptrs(XDR *, u_int *, mc_partition ***);
    static bool_t xdr_devptr(XDR *, mc_device **);
				  
    //  Class Methods

    static void read_handler(int, void *);
    static int read_proc(void *, void *, u_int);
    static int write_proc(void *, void *, u_int);
    static void event_proc(const Event&, void *);

    MonitorClient(const MonitorClient&); // Do not copy
    void operator = (const MonitorClient&);//  or assign.

};

#endif /* !MonitorClient_included */
