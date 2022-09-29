#ifndef DirectoryScanner_included
#define DirectoryScanner_included

#include "Scanner.h"

#include <stddef.h>
#include <sys/types.h>
#include <sys/dir.h>

#include "Event.h"

class Client;
class Directory;
class DirEntry;

//  DirectoryScanner scans a directory for created or deleted files.
//  It is an object because a scan can be interrupted when the output
//  stream becomes blocked.
//
//  The interface is quite weird.  Each time the done() method is called,
//  it does as much work as it can, until output is blocked.  Then
//  it returns true or false depending on whether it got done.
//
//  This whole flow control thing needs a good redesign.
//
//  Since a large number of DirectoryScanners is created, we have our
//  own new and delete operators.  They cache the most recently freed
//  DirectoryScanner for re-use.

class DirectoryScanner : public Scanner {

public:

    typedef void (*DoneHandler)(DirectoryScanner *, void *closure);

    DirectoryScanner(Directory&, const Event&, Boolean, DoneHandler, void *);
    virtual ~DirectoryScanner();

    virtual Boolean done();

    void *operator new (size_t);
    void operator delete (void *);

private:

    //  Instance Variables

    Directory& directory;
    const DoneHandler done_handler;
    void *const closure;
    const Event new_event;
    const Boolean scan_entries;
    DIR *dir;
    int openErrno;
    DirEntry **epp;
    DirEntry *discard;

    //  Class Variable

    static DirectoryScanner *cache;

    //  Private Instance Methods

    DirEntry **match_name(DirEntry **epp, const char *name);

};

#endif /* !DirectoryScanner_included */
