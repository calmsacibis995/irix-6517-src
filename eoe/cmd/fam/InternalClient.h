#ifndef InternalClient_included
#define InternalClient_included

#include "Client.h"

class ClientInterest;

//  An InternalClient is a hook for fam to use itself to monitor files.
//  It's used to monitor /etc/exports and /etc/mtab, and possibly others.
//
//  An InternalClient monitors a single file and calls a callback function
//  with any events received.  An InternalClient cannot monitor a directory.

class InternalClient : public Client {

public:

    typedef void (*EventHandler)(const Event&, void *closure);

    InternalClient(const char *filename, EventHandler, void *closure);
    ~InternalClient();

    Boolean ready_for_events();
    void post_event(const Event&, Request, const char *name);
    void enqueue_for_scan(Interest *);
    void dequeue_from_scan(Interest *);
    void enqueue_scanner(Scanner *);

private:

    EventHandler handler;
    void *closure;
    ClientInterest *interest;

};

#endif /* !InternalClient_included */
