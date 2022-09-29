#ifndef MxClient_included
#define MxClient_included

#include "Client.h"
#include "RequestMap.h"

class ClientInterest;

//  MxClient is a multiplexed client (one with more than one request
//  outstanding).  It an an abstract base type whose most famous
//  derived class is TCP_Client.
//  
//  An MxClient maps Requests to ClientInterests.  It also implements
//  (part of) the suspend/resume protocol.
//
//  In a glorious spasm of overengineering, I implemented the table of
//  requests as an in-core B-Tree.  Why?  Because a B-Tree scales
//  really well and because it has good locality of reference.

class MxClient : public Client {

protected:

    ~MxClient();

    void post_event(const Event&, Request, const char *name);
    void dequeue_from_scan(Interest *);

    void monitor_file(Request, const char *path, const Cred&);
    void monitor_dir(Request, const char *path, const Cred&);
    void cancel(Request);
    void suspend(Request);
    void resume(Request);

private:

    RequestMap requests;

    ClientInterest *interest(Request);
    Boolean check_new(Request, const char *path);

};

#endif /* !MxClient_included */
