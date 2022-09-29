#include "FormatLibrary.H"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/dir.h>
#include <unistd.h>

#include "FormatDSO.H"
#include "Log.H"

FormatLibrary::FormatLibrary(const char *dir)
: _ndsos(0),
  _flush_task(flush_proc, this)
{
    // Read the directory; find all files matching pattern.

    DIR *D = opendir(dir);
    if (!D)
	return;
    (void) re_comp("^fmt_.*\\.so$");
    char path[PATH_MAX];
    strcpy(path, dir);
    char *enddir = path + strlen(path);
    *enddir++ = '/';
    for (direct *dp; dp = readdir(D); )
    {   if (!re_exec(dp->d_name))
	    continue;
	if (_ndsos == MAX_FORMAT_DSOS)
	{   Log::error("too many format DSOs in %s: max is %d",
		       dir, MAX_FORMAT_DSOS);
	    break;
	}
	(void) strcpy(enddir, dp->d_name);
	_dsos[_ndsos] = new FormatDSO(path);
	_ndsos++;
    }
    (void) closedir(D);

    // Sort the DSOs alphabetically.

    qsort(_dsos, _ndsos, sizeof *_dsos, compare);
}

FormatLibrary::~FormatLibrary()
{
    for (FormatDSO **p = _dsos; p  < _dsos + _ndsos; p++)
	delete *p;
}

FormatDSO *FormatLibrary::operator [] (unsigned int index)
{
    _flush_task.schedule(Task::ASAP);
    return index < _ndsos ? _dsos[index] : NULL;
}

//  Flush any uninstantiated DSOs out of memory.

void
FormatLibrary::flush()
{
    for (int i = 0; i < _ndsos; i++)
	_dsos[i]->flush();
}

void
FormatLibrary::flush_proc(Task&, void *closure)
{
    FormatLibrary *lib = (FormatLibrary *) closure;
    lib->flush();
}

int
FormatLibrary::compare(const void *v0, const void *v1)
{
    const FormatDSO *dso0 = * (const FormatDSO **) v0;
    const FormatDSO *dso1 = * (const FormatDSO **) v1;
    return strcmp(dso0->name(), dso1->name());
}
