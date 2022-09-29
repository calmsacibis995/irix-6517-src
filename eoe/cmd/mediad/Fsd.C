#include "Fsd.H"

#include <assert.h>
#include <errno.h>
#include <fam.h>
#include <mntent.h>
#include <string.h>
#include <unistd.h>

Fsd *Fsd::_auto;
//Fsd *Fsd::_tab;

Fsd::Fsd(const char *path)
: _path(strcpy(new char[strlen(path) + 1], path)),
  _nentries(0),
  _entries(NULL),
  _mon(path, change_proc, this),
  _end_exist_seen(false)
{
    read();
}

Fsd::~Fsd()
{
    destroy();
}

const mntent *
Fsd::operator [] (unsigned int index) const
{
    if (index < _nentries)
	return _entries + index;
    else
	return NULL;
}

mntent *
Fsd::operator [] (unsigned int index)
{
    if (index < _nentries)
	return _entries + index;
    else
	return NULL;
}

mntent *
Fsd::at_fsname_type(const char *fsname, const char *type)
{
    for (mntent *mnt = _entries, *end = mnt + _nentries; mnt < end; mnt++)
	if (!strcmp(fsname, mnt->mnt_fsname) && !strcmp(type, mnt->mnt_type))
	    return mnt;
    return NULL;
}

mntent *
Fsd::at_dir(const char *dir)
{
    for (mntent *mnt = _entries, *end = mnt + _nentries; mnt < end; mnt++)
	if (!strcmp(dir, mnt->mnt_dir))
	    return mnt;
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////

void
Fsd::replace(unsigned int index, const mntent *new_entry)
{
    const char tmplate[] = "/etc/fsdXXXXXX";
    char tmpname[sizeof tmplate];
    (void) strcpy(tmpname, tmplate);
    mktemp(tmpname);
    FILE *f = setmntent(_path, "r+");
    if (!f)
	return;
    FILE *tf = setmntent(tmpname, "w");
    if (!tf)
    {   (void) endmntent(f);
	return;
    }
    destroy();

    const mntent *mnt;
    int error = 0;			// assume not found
    for (int i = 0; mnt = getmntent(f); i++)
    {   if (i == index)
	    mnt = new_entry;
	if (addmntent(tf, mnt))
	{   error = errno;
	    break;
	}
    }
    if (i <= index)
	if (addmntent(tf, mnt))
    	    error = errno;

    // Rename the files before releasing the lock on either one.
    
    if (error == 0)
    {	if (::rename(tmpname, _path))
	    error = errno;
    }
    else
	(void) unlink(tmpname);
    endmntent(f);
    endmntent(tf);

    read();
}

#if 0

void
Fsd::remove(unsigned int index)
{
    const char tmplate[] = "/etc/fsdXXXXXX";
    char tmpname[sizeof tmplate];
    (void) strcpy(tmpname, tmplate);
    mktemp(tmpname);
    FILE *f = setmntent(_path, "r+");
    if (!f)
	return;
    FILE *tf = setmntent(tmpname, "w");
    if (!tf)
    {   (void) endmntent(f);
	return;
    }
    destroy();

    const mntent *mnt;
    int error = 0;			// assume not found
    for (int i = 0; mnt = getmntent(f); i++)
	if (i != index && addmntent(tf, mnt))
	{   error = errno;
	    break;
	}
    if (i <= index)
	if (addmntent(tf, mnt))
    	    error = errno;

    // Rename the files before releasing the lock on either one.
    
    if (error == 0)
    {	if (::rename(tmpname, _path))
	    error = errno;
    }
    else
	(void) unlink(tmpname);
    endmntent(f);
    endmntent(tf);

    read();
}

#endif

void
Fsd::append(const mntent *new_entry)
{
    FILE *f = setmntent(_path, "w+");
    if (f)
    {   (void) addmntent(f, new_entry);
	(void) endmntent(f);
	destroy();
	read();
    }
}

///////////////////////////////////////////////////////////////////////////////

void
Fsd::read()
{
    unsigned int i;

    FILE *f = setmntent(_path, "r+");
    if (!f)
	return;

    for (_nentries = 0; getmntent(f); _nentries++)
	continue;
    rewind(f);
    _entries = new mntent[_nentries];
    for (i = 0; i < _nentries; i++)
    {   
	mntent *mnt = getmntent(f);
	assert(mnt);

	//  Allocate one string per mntent, copy all _mnt* strings
	//  into that one string.

	int nchar = strlen(mnt->mnt_fsname) + strlen(mnt->mnt_dir) +
		    strlen(mnt->mnt_type) + strlen(mnt->mnt_opts) + 4;
	char *buf = new char[nchar];
	char *p = buf;
	_entries[i].mnt_fsname = strcpy(p, mnt->mnt_fsname);
	p += strlen(p) + 1;

	_entries[i].mnt_dir = strcpy(p, mnt->mnt_dir);
	p += strlen(p) + 1;

	_entries[i].mnt_type = strcpy(p, mnt->mnt_type);
	p += strlen(p) + 1;

	_entries[i].mnt_opts = strcpy(p, mnt->mnt_opts);
	p += strlen(p) + 1;

	_entries[i].mnt_freq = mnt->mnt_freq;
	_entries[i].mnt_passno = mnt->mnt_passno;
    }
    assert(!getmntent(f));
    endmntent(f);
}

void
Fsd::destroy()
{
    if (_entries)
    {   for (unsigned int i = 0; i < _nentries; i++)
	    delete [] _entries[i].mnt_fsname;
	delete [] _entries;
	_nentries = 0;
	_entries = NULL;
    }
}

void
Fsd::change_proc(FAMonitor&, const FAMEvent& event, void *closure)
{
    Fsd *fsd = (Fsd *) closure;
    FAMCodes code = event.code;
    if (code == FAMEndExist)
	fsd->_end_exist_seen = true;
    else if (fsd->_end_exist_seen)
    {   fsd->destroy();
	fsd->read();
    }
}

//////////////////////////////////////////////////////////////////////////////

Fsd&
Fsd::fsd_auto()
{
    if (!_auto)
	_auto = new Fsd("/etc/fsd.auto");
    assert(_auto);
    return *_auto;
}

#if 0					// not needed

Fsd&
Fsd::fsd_tab()
{
    if (!_tab)
	_tab = new Fsd("/etc/fsd.tab");
    assert(_tab);
    return *_tab;
}

#endif /* 0 */
