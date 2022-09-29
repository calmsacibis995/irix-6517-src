#ifndef LocalFileSystem_included
#define LocalFileSystem_included

#include "FileSystem.h"

//  LocalFileSystem represents a local (non-NFS) file system.
//
//  LocalFileSystem has a null implementation of the high level
//  interface.  Its low level interface puts nonexistent files 
//  on the list to be polled.
//
//  Note that a LocalFileSystem may represent a weird file system
//  like a DOS floppy or and ISO-9660 CD-ROM.  Don't assume EFS/xFS...

class LocalFileSystem : public FileSystem {

public:

    LocalFileSystem(const mntent&);

    virtual Boolean dir_entries_scanned() const;
    virtual int attr_cache_timeout() const;

    // High level monitoring interface

    virtual Request hl_monitor(ClientInterest *, ClientInterest::Type);
    virtual void    hl_cancel(Request);
    virtual void    hl_suspend(Request);
    virtual void    hl_resume(Request);
    virtual void    hl_map_path(char *remote_path, const char *local_path,
				const Cred&);

    // Low level monitoring interface

    virtual void ll_monitor(Interest *, Boolean imonitored);
    virtual void ll_notify_created(Interest *);
    virtual void ll_notify_deleted(Interest *);

};

#endif /* !LocalFileSystem_included */
