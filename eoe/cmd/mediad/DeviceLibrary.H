#ifndef DeviceLibrary_included
#define DeviceLibrary_included

#include "bool.H"

class DeviceDSO;

class DeviceLibrary {

public:

    DeviceLibrary(const char *directory);
    ~DeviceLibrary();

    DeviceDSO *operator [] (unsigned int);

    void flush();

private:

    enum { MAX_DEVICE_DSOS = 100 };

    DeviceDSO *_dsos[MAX_DEVICE_DSOS];
    unsigned int _ndsos;

    static int compare(const void *, const void *); // qsort comparison routine

};

#endif /* !DeviceLibrary_included */
