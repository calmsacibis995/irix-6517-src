#ifndef DSO_included
#define DSO_included

#include <limits.h>

//  DSO is an abstract base class for dynamic shared objects.  The
//  two kinds of DSO are DeviceDSO and FileSystemDSO.

class DSO {

public:

    DSO(const char *path);
    virtual ~DSO();

    const char *name() const		{ return _path; }

    virtual void load();
    virtual void unload();
    
protected:

    void *sym(const char *name);

private:

    void *_handle;
    char _path[PATH_MAX];

};

#endif /* !DSO_included */
