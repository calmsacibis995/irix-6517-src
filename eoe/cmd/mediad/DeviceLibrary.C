#include "DeviceLibrary.H"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <unistd.h>

#include "DeviceDSO.H"
#include "Log.H"

DeviceLibrary::DeviceLibrary(const char *dir)
: _ndsos(0)
{
    // Read the directory; find all files matching pattern.

    DIR *D = opendir(dir);
    if (!D)
	return;
    (void) re_comp("^dev_.*\\.so$");
    char path[PATH_MAX];
    strcpy(path, dir);
    char *enddir = path + strlen(path);
    *enddir++ = '/';
    for (direct *dp; dp = readdir(D); )
    {   if (!re_exec(dp->d_name))
	    continue;
	if (_ndsos == MAX_DEVICE_DSOS)
	{   Log::error("too many Device DSOs in %s: max is %d",
		       dir, MAX_DEVICE_DSOS);
	    break;
	}
	(void) strcpy(enddir, dp->d_name);
	_dsos[_ndsos] = new DeviceDSO(path);
	_ndsos++;
    }
    (void) closedir(D);

    // Sort the DSOs alphabetically.

    qsort(_dsos, _ndsos, sizeof *_dsos, compare);
}

DeviceLibrary::~DeviceLibrary()
{
    for (DeviceDSO **p = _dsos; p  < _dsos + _ndsos; p++)
	delete *p;
}

DeviceDSO *DeviceLibrary::operator [] (unsigned int index)
{
    return index < _ndsos ? _dsos[index] : NULL;
}

//  Flush any uninstantiated DSOs out of memory.

void
DeviceLibrary::flush()
{
    for (int i = 0; i < _ndsos; i++)
	_dsos[i]->flush();
}

int
DeviceLibrary::compare(const void *v0, const void *v1)
{
    const DeviceDSO *dso0 = * (const DeviceDSO **) v0;
    const DeviceDSO *dso1 = * (const DeviceDSO **) v1;
    return strcmp(dso0->name(), dso1->name());
}
