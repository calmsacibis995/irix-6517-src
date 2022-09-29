#include "FileSystem.h"

#include <mntent.h>
#include <string.h>

#include "Event.h"

FileSystem::FileSystem(const mntent& mnt)
    : mydir   (strcpy(new char[strlen(mnt.mnt_dir   ) + 1], mnt.mnt_dir   )),
      myfsname(strcpy(new char[strlen(mnt.mnt_fsname) + 1], mnt.mnt_fsname))
{ }

FileSystem::~FileSystem()
{
    assert(!myinterests.size());
    delete [] mydir;
    delete [] myfsname;
}

Boolean
FileSystem::matches(const mntent& mnt) const
{
    return !strcmp(mydir, mnt.mnt_dir) && !strcmp(myfsname, mnt.mnt_fsname);
}

void
FileSystem::relocate_interests()
{
    for (ClientInterest *cip = myinterests.first();
	 cip;
	 cip = myinterests.next(cip))
    {
	cip->findfilesystem();
    }
}

Request
FileSystem::monitor(ClientInterest *cip, ClientInterest::Type type)
{
    myinterests.insert(cip);
    return hl_monitor(cip, type);
}

void
FileSystem::cancel(ClientInterest *cip, Request request)
{
    hl_cancel(request);
    myinterests.remove(cip);
}
