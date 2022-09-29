#ifndef ClientInterest_included
#define ClientInterest_included

#include "Boolean.h"
#include "Cred.h"
#include "Interest.h"
#include "Request.h"

class Client;

//  ClientInterest -- an abstract base class for a filesystem entity
//  in which a client has expressed an interest.  The two kinds of
//  ClientInterest are File and Directory.
//
//  The ClientInterest is intimately tied to the Interest, Directory
//  and DirEntry.  And the whole hierarchy is very messy.

class ClientInterest : public Interest {

public:

    enum Type { FILE = 'W', DIRECTORY = 'M' };

    ~ClientInterest();

    virtual void suspend();
    virtual void resume();
    virtual Boolean active() const;

    void findfilesystem();
    void scan(Interest * = NULL);
    void unscan(Interest * = NULL);
    virtual Interest *find_name(const char *);
    virtual void do_scan();

    void become_user() const		{ mycred.become_user(); }
    const Cred& cred() const		{ return mycred; }
    FileSystem *filesystem() const	{ return myfilesystem; }
    Client *client() const		{ return myclient; }
    virtual Type type() const = 0;

    virtual void notify_created(Interest *);
    virtual void notify_deleted(Interest *);

protected:

    ClientInterest(const char *name, Client *, Request, const Cred&, Type);
    void post_event(const Event&, const char * = NULL);

private:

    enum { ACTIVE_STATE = 1 << 0 };

    //  Instance Variables

    Client *myclient;
    Request request;
    const Cred mycred;
    FileSystem *myfilesystem;
    Request fs_request;

    void filesystem(FileSystem *);

};

#endif /* !ClientInterest_included */
