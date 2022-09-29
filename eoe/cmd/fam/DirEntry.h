#ifndef DirEntry_included
#define DirEntry_included

#include "Interest.h"

class Directory;

//  A DirEntry is a subclass of Interest that represents an entry
//  in a Directory that a Client has expressed an interest in.
//
//  A DirEntry overrides a lot of Interest's methods and forwards
//  them to its parent directory.
//
//  DirEntries are stored on a singly-linked list.  The head of the
//  list is in the parent.  Each DirEntry also has a parent pointer.
//  The entries in the list are in the order they're returned by
//  readdir().

class DirEntry : public Interest {

public:

    virtual Boolean active() const;
    virtual void scan(Interest * = 0);
    virtual void unscan(Interest * = 0);
    virtual void do_scan();

protected:

    void post_event(const Event&, const char * = 0);

private:

    //  Instance Variables

    Directory *const parent;
    DirEntry *next;

    //  Private Instance Methods

    DirEntry(const char *name, Directory *parent, DirEntry *next);
				// Only a Directory may create a DirEntry.
    virtual ~DirEntry();

    virtual void notify_created(Interest *);
    virtual void notify_deleted(Interest *);

friend class Directory;
friend class DirectoryScanner;

};

#endif /* !DirEntry_included */
