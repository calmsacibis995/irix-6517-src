#ifndef DeviceDSO_included
#define DeviceDSO_included

#include "DSO.H"

class Device;
class DeviceInfo;

class DeviceDSO : public DSO {

public:

    DeviceDSO(const char *path)		: DSO(path), _instances(0) { }
    ~DeviceDSO();

    Device *instantiate(const DeviceInfo&);
    void deinstantiate(Device *);

    void flush()			{ if (!_instances) unload(); }

private:

    int _instances;

};

#endif /* !DeviceDSO_included */
