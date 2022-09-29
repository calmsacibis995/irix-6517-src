#include "DSO.H"

#include <dlfcn.h>
#include <string.h>
#include "Log.H"

DSO::DSO(const char *path)
: _handle(NULL)
{
    (void) strcpy(_path, path);
}

DSO::~DSO()
{
    unload();
}

void
DSO::load()
{
    if (!_handle)
    {   _handle = dlopen(_path, RTLD_NOW);
	if (!_handle)
	{   Log::error("%s", dlerror());
	    return;
	}
	void (*loadproc)() = (void (*)()) sym("load");
	if (loadproc)
	    (*loadproc)();
    }
}

void
DSO::unload()
{
    if (_handle)
    {
	void (*unloadproc)() = (void (*)()) sym("unload");
	if (unloadproc)
	    (*unloadproc)();
	dlclose(_handle);
	_handle = NULL;
    }
}

void *
DSO::sym(const char *name)
{
    return dlsym(_handle, name);
}
