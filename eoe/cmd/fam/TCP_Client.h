#ifndef TCP_Client_included
#define TCP_Client_included

#include "ClientConnection.h"
#include "MxClient.h"
#include "Set.h"

//  A TCP_Client is a client that connects to fam using the TCP/IP
//  protocol.  This class implements the fam protocol, and it also
//  handles I/O buffering.
//
//  The order of TCP_Client's fields has been optimized.  The
//  connection is next to last because it contains big buffers, and
//  the ends of the buffers will rarely be accessed.  The Activity
//  is last because it is never accessed.

class TCP_Client : public MxClient {

public:

    TCP_Client(int fd);

    Boolean ready_for_events();
    void post_event(const Event&, Request, const char *name);
    void enqueue_for_scan(Interest *);
    void dequeue_from_scan(Interest *);
    virtual void enqueue_scanner(Scanner *);

private:

    Set<Interest *> to_be_scanned;
    Scanner *my_scanner;
    ClientConnection conn;
    Activity a;				// simply declaring it activates timer.

    void input_msg(const char *msg, int size);

    static void input_handler(const char *msg, unsigned nbytes, void *closure);
    static void unblock_handler(void *closure);

};

#endif /* !TCP_Client_included */
