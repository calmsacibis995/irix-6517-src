#ifndef NFSFileSystem_included
#define NFSFileSystem_included

#include "FileSystem.h"
#include "ServerHostRef.h"

//  NFSFileSystem represents an NFS file system.
//
//  NFSFileSystem implements the high level FileSystem interface.  It
//  has a null implementation of the low level interface.  (See
//  FileSystem.H for the high and low level interfaces.)
//
//  Perhaps the most significant thing NFSFileSystem does is mapping
//  local paths to remote paths (hl_map_path()).

class NFSFileSystem : public FileSystem {

public:

    NFSFileSystem(const mntent&);
    ~NFSFileSystem();

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

    virtual void ll_monitor(Interest *, Boolean);
    virtual void ll_notify_created(Interest *);
    virtual void ll_notify_deleted(Interest *);

private:

    ServerHostRef host;
    char *remote_dir;
    unsigned remote_dir_len;
    unsigned local_dir_len;

};

#endif /* !NFSFileSystem_included */
