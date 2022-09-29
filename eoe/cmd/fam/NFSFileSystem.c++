#include "NFSFileSystem.h"

#include <assert.h>
#include <mntent.h>
#include <stdlib.h>
#include <string.h>

#include "Log.h"
#include "ServerHost.h"

NFSFileSystem::NFSFileSystem(const mntent& mnt)
    : FileSystem(mnt)
{
    //  Extract the host name from the fs name.

    const char *fsname = mnt.mnt_fsname;
    const char *colon = strchr(fsname, ':');
    assert(colon);
    char hostname[MAXNAMELEN];
    int hostnamelen = colon - fsname;
    assert(hostnamelen < sizeof hostname);
    strncpy(hostname, fsname, hostnamelen);
    hostname[hostnamelen] = '\0';

    //  Look up the host by name.

    host = ServerHostRef(hostname);

    //  Store the remote directory name.  As a special case, "/"
    //  is translated to "" so it will prepend neatly to an absolute
    //  path.

    const char *dir_part = colon + 1;
    if (!strcmp(dir_part, "/"))
	dir_part++;
    remote_dir_len = strlen(dir_part);
    remote_dir = strcpy(new char[remote_dir_len + 1], dir_part);
    local_dir_len = strlen(dir());
}

NFSFileSystem::~NFSFileSystem()
{
    ClientInterest *cip;
    while (cip = interests().first())
	cip->findfilesystem();
    delete [] remote_dir;
}

Boolean
NFSFileSystem::dir_entries_scanned() const
{
    return !host->is_connected();
}


int
NFSFileSystem::attr_cache_timeout() const
{
    //  Need a real implementation for this...
    return 3;
}

//////////////////////////////////////////////////////////////////////////////
//  High level interface

Request
NFSFileSystem::hl_monitor(ClientInterest *ci, ClientInterest::Type type)
{
    const char *path = ci->name();
    assert(path[0] == '/');
    char remote_path[MAXPATHLEN];
    hl_map_path(remote_path, ci->name(), ci->cred());
    return host->send_monitor(ci, type, remote_path);
}

void
NFSFileSystem::hl_cancel(Request request)
{
    host->send_cancel(request);
}

void
NFSFileSystem::hl_suspend(Request request)
{
    host->send_suspend(request);
}

void
NFSFileSystem::hl_resume(Request request)
{
    host->send_resume(request);
}

void
NFSFileSystem::hl_map_path(char *remote_path, const char *path, const Cred& cr)
{
    char local_path[MAXPATHLEN];
    cr.become_user();
    if (realpath(path, local_path))
    {   assert(!strncmp(local_path, dir(), local_dir_len));
	(void) strcpy(remote_path, remote_dir);
	(void) strcpy(remote_path + remote_dir_len,
		       local_path + local_dir_len);
    }
    else
    {
	//  realpath failed -- remove components one at a time
	//  until it starts succeeding, then append the
	//  missing components to the path.  Use remote_path
	//  for scratch space.

	(void) strcpy(remote_path, path);
	char *p;
	do {
	    p = strrchr(remote_path, '/');
	    assert(p);
	    *p = '\0';
	} while (!realpath(remote_path, local_path));
	(void) strcpy(remote_path, remote_dir);
	(void) strcpy(remote_path + remote_dir_len,
		      local_path + local_dir_len);
	(void) strcat(remote_path, path + (p - remote_path));
    }

    // If we're famming a remote machine's root directory, then
    // remote_path will be empty at this point.  Make it "/" instead.
    if (!*remote_path) {
        strcpy(remote_path, "/");
    }
        
    //	Log::debug("NFSFileSystem::hl_map_path() mapped \"%s\" -> \"%s\"",
    //		   path, remote_path);
}

//////////////////////////////////////////////////////////////////////////////
//  Low level interface: no implementation.

void
NFSFileSystem::ll_monitor(Interest *, Boolean)
{ }

void
NFSFileSystem::ll_notify_created(Interest *)
{ }

void
NFSFileSystem::ll_notify_deleted(Interest *)
{ }
