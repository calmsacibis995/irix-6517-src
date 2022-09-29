#ifndef Directory_included
#define Directory_included

#include "ClientInterest.h"

class DirEntry;
class DirectoryScanner;

//  A Directory represents a directory that we are monitoring.
//  It's derived from ClientInterest.  See the comments in
//  ClientInterest and Interest.
//
//  Each Directory has a linked list of DirEntries.  The DirEntries
//  are sorted in the order they're returned by readdir(2).

class Directory : public ClientInterest {

public:

    Directory(const char *name, Client *, Request, const Cred&);
    ~Directory();

    virtual void resume();

    Type type() const;
    Interest *find_name(const char *);
    virtual void do_scan();

    Boolean chdir();

    void unhang();

private:

    enum { SCANNING = 1 << 0, RESCAN_SCHEDULED = 1 << 1 };

    //  Instance Variable

    DirEntry *entries;

    pid_t unhangPid;

    //  Class Variable

    static Directory *current_dir;

    //  Class Methods

    static void chdir_task(void *);
    static void new_done_handler(DirectoryScanner *, void *);
    static void scan_done_handler(DirectoryScanner *, void *);
    static void scan_task(void *);

friend class DirEntry;
friend class DirectoryScanner;

};

#endif /* !Directory_included */
