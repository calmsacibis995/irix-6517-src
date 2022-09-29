#ifndef Client_included
#define Client_included

#include "Activity.h"
#include "Boolean.h"
#include "Request.h"

class ChangeFlags;
class Cred;
class Event;
class Interest;
class Scanner;

//  Client is an abstract base type for clients of fam.  A Client has
//  a name which is used for debugging, and it receives events from
//  lower-level code.
//
//  These are the classes derived from Client.
//
//	InternalClient	some part of fam that needs to watch a file
//	MxClient	multiplexed client, an abstract type
//	TCP_Client	client that connects via TCP/IP protocol
//	RemoteClient	TCP_Client on a different machine

class Client {

public:    

    Client(const char *name = 0);
    virtual ~Client();

    virtual Boolean ready_for_events() = 0;
    virtual void post_event(const Event&, Request, const char *name) = 0;
    virtual void enqueue_for_scan(Interest *) = 0;
    virtual void dequeue_from_scan(Interest *) = 0;
    virtual void enqueue_scanner(Scanner *) = 0;
    const char *name();

protected:

    void name(const char *);

private:

    char *myname;

    Client(const Client&);		// Do not copy
    operator = (const Client&);		//   or assign

};

#endif /* !Client_included */
