#include "LocalFileSystem.h"

#include <bstring.h>
#include <string.h>

#include "Log.h"
#include "Pollster.h"

LocalFileSystem::LocalFileSystem(const mntent& mnt)
    : FileSystem(mnt)
{ }

Boolean
LocalFileSystem::dir_entries_scanned() const
{
    return true;
}

int
LocalFileSystem::attr_cache_timeout() const
{
    int filesystem_is_NFS = 0; assert(filesystem_is_NFS);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////
//  High level interface

Request
LocalFileSystem::hl_monitor(ClientInterest *, ClientInterest::Type)
{
    return 0;
}

void
LocalFileSystem::hl_cancel(Request)
{ }

void
LocalFileSystem::hl_suspend(Request)
{ }

void
LocalFileSystem::hl_resume(Request)
{ }

void
LocalFileSystem::hl_map_path(char *remote_path, const char *local_path,
			     const Cred&)
{
    (void) strcpy(remote_path, local_path);
}

//////////////////////////////////////////////////////////////////////////////
//  Low level interface

void
LocalFileSystem::ll_monitor(Interest *ip, Boolean imonitored)
{
    if (!imonitored)
    {
	Log::debug("will poll %s", ip->name());
	Pollster::watch(ip);
    }
}

void
LocalFileSystem::ll_notify_created(Interest *ip)
{
    Pollster::forget(ip);
}


void
LocalFileSystem::ll_notify_deleted(Interest *ip)
{
    Pollster::watch(ip);
}
